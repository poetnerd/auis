/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer:
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice, this permission notice, and the following
 * disclaimer appear in supporting documentation, and that the names of
 * IBM, Carnegie Mellon University, and other copyright holders, not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS
 * DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *  $
*/

/*
	smtpsub.c -- Direct SMTP submission, called from the sendmail
		    else-clause of dropoff_auth() (dropoff.c) when the
		    "smtphost" preference is set.  See smtp_dropoff()
		    below; the D_* return codes are in dropoff.h.

	Security note: the password is never written to Dropoff_ErrMsg,
	stdout/stderr, or any other output.  The optional AMS_SMTP_TRACE
	protocol trace (stderr, off by default) redacts the AUTH PLAIN
	line instead of printing it.
*/

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#ifndef MAXPATHLEN
#include <sys/param.h>
#endif /* MAXPATHLEN */

#include <util.h>
#include <dropoff.h>
#include <tlscon.h>
#include <netrc.h>

#define SMTP_DEFAULT_PORT 465
#define SMTP_LINE_MAX	1024
#define SMTP_BODY_BUFSIZE 65536
#define SMTP_CAP_MAX	2048

extern int errno;

/* ---- tiny protocol trace, off unless AMS_SMTP_TRACE is set ---- */

static int smtp_tracing = -1;

static void smtp_trace(dir, text)
    char *dir, *text;
{
    if (smtp_tracing < 0) smtp_tracing = (getenv("AMS_SMTP_TRACE") != NULL);
    if (smtp_tracing) fprintf(stderr, "%s: %s\n", dir, text);
}

/* ---- standard (RFC 4648) base64, used only for AUTH PLAIN ---- */

static void smtp_b64encode(in, inlen, out)
    unsigned char *in;
    int inlen;
    char *out;
{
    static char tbl[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j;
    unsigned int v;

    j = 0;
    for (i = 0; i < inlen; i += 3) {
	v = ((unsigned int) in[i]) << 16;
	if (i+1 < inlen) v |= ((unsigned int) in[i+1]) << 8;
	if (i+2 < inlen) v |= (unsigned int) in[i+2];
	out[j++] = tbl[(v >> 18) & 0x3f];
	out[j++] = tbl[(v >> 12) & 0x3f];
	out[j++] = (i+1 < inlen) ? tbl[(v >> 6) & 0x3f] : '=';
	out[j++] = (i+2 < inlen) ? tbl[v & 0x3f] : '=';
    }
    out[j] = '\0';
}

/* ---- reply-line parsing ---- */

/* Read exactly one line and split it into code/separator/text.  Returns
   0 on a well-formed line, -1 on a read error, timeout, or a line that
   doesn't start with a 3-digit code. */
static int smtp_readline1(conn, pcode, psep, text, textlen)
    struct tlscon *conn;
    int *pcode;
    char *psep;
    char *text;
    int textlen;
{
    char line[SMTP_LINE_MAX];
    int n;

    n = tlscon_ReadLine(conn, line, sizeof(line));
    if (n < 0) {
	smtp_trace("S", "<read error/timeout>");
	return -1;
    }
    smtp_trace("S", line);
    if (n < 3 || !isdigit((unsigned char) line[0]) ||
		 !isdigit((unsigned char) line[1]) ||
		 !isdigit((unsigned char) line[2]))
	return -1;
    *pcode = (line[0]-'0')*100 + (line[1]-'0')*10 + (line[2]-'0');
    *psep = (n >= 4) ? line[3] : ' ';
    if (text != NULL && textlen > 0) {
	strncpy(text, (n >= 5) ? line+4 : "", textlen-1);
	text[textlen-1] = '\0';
    }
    return 0;
}

/* Read a (possibly multiline) reply.  textbuf receives the text of the
   last line.  Returns the reply code, or -1 on error/garbled reply. */
static int smtp_getreply(conn, textbuf, textlen)
    struct tlscon *conn;
    char *textbuf;
    int textlen;
{
    int code, firstcode;
    char sep;
    char text[SMTP_LINE_MAX];

    firstcode = -1;
    for (;;) {
	if (smtp_readline1(conn, &code, &sep, text, sizeof(text)) < 0) return -1;
	if (firstcode == -1) firstcode = code;
	else if (code != firstcode) return -1;	/* inconsistent multiline reply */
	if (textbuf != NULL && textlen > 0) {
	    strncpy(textbuf, text, textlen-1); textbuf[textlen-1] = '\0';
	}
	if (sep == ' ') break;
	if (sep != '-') return -1;
    }
    return firstcode;
}

/* Like smtp_getreply(), but also accumulates every line's text (one per
   line, newline-separated) into capbuf, for scanning EHLO capabilities. */
static int smtp_getreply_cap(conn, capbuf, capbuflen, textbuf, textlen)
    struct tlscon *conn;
    char *capbuf;
    int capbuflen;
    char *textbuf;
    int textlen;
{
    int code, firstcode;
    char sep;
    char text[SMTP_LINE_MAX];

    firstcode = -1;
    capbuf[0] = '\0';
    for (;;) {
	if (smtp_readline1(conn, &code, &sep, text, sizeof(text)) < 0) return -1;
	if (firstcode == -1) firstcode = code;
	else if (code != firstcode) return -1;
	if ((int) (strlen(capbuf) + strlen(text) + 2) < capbuflen) {
	    strcat(capbuf, text);
	    strcat(capbuf, "\n");
	}
	if (textbuf != NULL && textlen > 0) {
	    strncpy(textbuf, text, textlen-1); textbuf[textlen-1] = '\0';
	}
	if (sep == ' ') break;
	if (sep != '-') return -1;
    }
    return firstcode;
}

static void smtp_writeline(conn, line)
    struct tlscon *conn;
    char *line;
{
    smtp_trace("C", line);
    (void) tlscon_Write(conn, line, strlen(line));
}

/* Best-effort RSET+QUIT (used when we're bailing out after RCPT but
   before DATA) or plain QUIT (used on earlier bail-outs).  Errors are
   ignored -- we're already returning an error to our own caller. */
static void smtp_abort(conn, doReset)
    struct tlscon *conn;
    int doReset;
{
    char text[SMTP_LINE_MAX];

    if (doReset) {
	smtp_writeline(conn, "RSET\r\n");
	(void) smtp_getreply(conn, text, sizeof(text));
    }
    smtp_writeline(conn, "QUIT\r\n");
    (void) smtp_getreply(conn, text, sizeof(text));
    tlscon_Close(conn);
}

/* Expand a leading "~/" the same way the profile machinery does. */
static char *smtp_expandpath(path, buf, buflen)
    char *path;
    char *buf;
    int buflen;
{
    char *home;

    if (path[0] == '~' && path[1] == '/') {
	home = gethome((char *) NULL);
	if (home != NULL) {
	    snprintf(buf, buflen, "%s/%s", home, path+2);
	    return buf;
	}
    }
    strncpy(buf, path, buflen-1);
    buf[buflen-1] = '\0';
    return buf;
}

/* Stream fd f to conn as the DATA body: LF -> CRLF, dot-stuffing, and a
   guaranteed trailing CRLF before the terminating "." line.  Closes f
   (via fclose on the FILE* it opens on f) on every path -- per the
   dropoff.c hook contract, smtp_dropoff() is responsible for f being
   closed on all paths, and this is the one place that does it once
   DATA streaming has begun.  Returns 0 on success, -1 on error (with a
   reason in errbuf). */
static int smtp_send_body(conn, f, errbuf, errbuflen)
    struct tlscon *conn;
    int f;
    char *errbuf;
    int errbuflen;
{
    FILE *fp;
    char *buf, *outline;
    int len, hadNL, lastHadNL, ok;

    fp = fdopen(f, "r");
    if (fp == NULL) {
	snprintf(errbuf, errbuflen, "fdopen failed: %s", strerror(errno));
	close(f);
	return -1;
    }

    buf = (char *) malloc(SMTP_BODY_BUFSIZE);
    outline = (char *) malloc(SMTP_BODY_BUFSIZE + 4);
    if (buf == NULL || outline == NULL) {
	snprintf(errbuf, errbuflen, "out of memory streaming message body");
	if (buf != NULL) free(buf);
	if (outline != NULL) free(outline);
	fclose(fp);
	return -1;
    }

    ok = 1;
    lastHadNL = 1;
    while (ok && fgets(buf, SMTP_BODY_BUFSIZE, fp) != NULL) {
	char *outp = outline;

	len = strlen(buf);
	hadNL = (len > 0 && buf[len-1] == '\n');
	if (hadNL) buf[--len] = '\0';
	if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';

	if (buf[0] == '.') *outp++ = '.';
	memcpy(outp, buf, len); outp += len;
	if (hadNL) { *outp++ = '\r'; *outp++ = '\n'; lastHadNL = 1; }
	else lastHadNL = 0;

	if (tlscon_Write(conn, outline, outp - outline) != 0) {
	    snprintf(errbuf, errbuflen, "connection write failed while sending body");
	    ok = 0;
	}
    }
    if (ok && ferror(fp)) {
	snprintf(errbuf, errbuflen, "read failed on message file: %s", strerror(errno));
	ok = 0;
    }
    if (ok && !lastHadNL) {
	if (tlscon_Write(conn, "\r\n", 2) != 0) {
	    snprintf(errbuf, errbuflen, "connection write failed while sending body");
	    ok = 0;
	}
    }

    free(buf);
    free(outline);
    fclose(fp);		/* also closes f */
    return ok ? 0 : -1;
}

int smtp_dropoff(f, tolist, returnpath)
    int f;
    char **tolist;
    char *returnpath;
{
    char *host, *netrcpref;
    char netrcpath[MAXPATHLEN+1];
    int port, nrc, code, i;
    char login[256], passwd[256];
    struct tlscon *conn;
    char errbuf[512];
    char replytext[SMTP_LINE_MAX];
    char capbuf[SMTP_CAP_MAX];
    char localhost[256];
    char line[SMTP_LINE_MAX + 64];
    unsigned char authraw[600];
    char authb64[900];
    int authlen;
    int anyAccepted, anyRejected, anyTempFail;
    char rejected[1024];

    host = getprofile("smtphost");
    if (host == NULL || host[0] == '\0') {
	close(f);
	strcpy(Dropoff_ErrMsg, "smtp_dropoff called with no smtphost preference set");
	return D_TEMP_FAIL;
    }
    port = getprofileint("smtpport", SMTP_DEFAULT_PORT);

    netrcpref = getprofile("smtpnetrc");
    if (netrcpref == NULL || netrcpref[0] == '\0') netrcpref = "~/.netrc";
    smtp_expandpath(netrcpref, netrcpath, sizeof(netrcpath));

    nrc = netrc_Lookup(netrcpath, host, login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) {
	close(f);
	switch (nrc) {
	case NETRC_NOENT:
	    sprintf(Dropoff_ErrMsg, "Can't open netrc file \"%s\"", netrcpath);
	    break;
	case NETRC_BADPERM:
	    sprintf(Dropoff_ErrMsg,
		"netrc file \"%s\" is group- or world-readable; refusing to use it",
		netrcpath);
	    break;
	case NETRC_NOMATCH:
	    sprintf(Dropoff_ErrMsg, "No netrc entry (or default) for machine \"%s\" in \"%s\"",
		host, netrcpath);
	    break;
	default:
	    sprintf(Dropoff_ErrMsg, "netrc lookup failed for \"%s\"", netrcpath);
	}
	return D_OSERR;
    }

    if (tlscon_Open(&conn, host, port, errbuf, sizeof(errbuf)) != 0) {
	close(f);
	sprintf(Dropoff_ErrMsg, "Can't connect to %.200s:%d: %.200s", host, port, errbuf);
	return D_TEMP_FAIL;
    }

    /* Greeting */
    code = smtp_getreply(conn, replytext, sizeof(replytext));
    if (code != 220) {
	sprintf(Dropoff_ErrMsg, "Bad greeting from %.200s: %d %.200s", host, code, replytext);
	tlscon_Close(conn);
	close(f);
	return D_TEMP_FAIL;
    }

    if (gethostname(localhost, sizeof(localhost)) != 0) strcpy(localhost, "localhost");

    sprintf(line, "EHLO %.200s\r\n", localhost);
    smtp_writeline(conn, line);
    code = smtp_getreply_cap(conn, capbuf, sizeof(capbuf), replytext, sizeof(replytext));
    if (code / 100 != 2) {
	sprintf(Dropoff_ErrMsg, "EHLO rejected by %.200s: %d %.200s", host, code, replytext);
	tlscon_Close(conn);
	close(f);
	return D_TEMP_FAIL;
    }
    if (strcasestr(capbuf, "AUTH") == NULL || strcasestr(capbuf, "PLAIN") == NULL) {
	sprintf(Dropoff_ErrMsg, "%.200s does not offer AUTH PLAIN", host);
	smtp_abort(conn, 0);
	close(f);
	return D_TEMP_FAIL;
    }

    /* AUTH PLAIN \0login\0password */
    authlen = 1 + strlen(login) + 1 + strlen(passwd);
    if (authlen > (int) sizeof(authraw) - 1) {
	strcpy(Dropoff_ErrMsg, "netrc login/password too long for AUTH PLAIN");
	smtp_abort(conn, 0);
	close(f);
	return D_OSERR;
    }
    authraw[0] = '\0';
    strcpy((char *) authraw + 1, login);
    authraw[1 + strlen(login)] = '\0';
    strcpy((char *) authraw + 1 + strlen(login) + 1, passwd);
    smtp_b64encode(authraw, authlen, authb64);

    sprintf(line, "AUTH PLAIN %s\r\n", authb64);
    smtp_trace("C", "AUTH PLAIN <redacted>");
    (void) tlscon_Write(conn, line, strlen(line));
    code = smtp_getreply(conn, replytext, sizeof(replytext));
    memset(line, 0, sizeof(line));			/* scrub the base64 auth line */
    memset(authraw, 0, sizeof(authraw));
    memset(authb64, 0, sizeof(authb64));
    if (code / 100 != 2) {
	sprintf(Dropoff_ErrMsg,
	    "AUTH failed (check app password / netrc): %d %.200s", code, replytext);
	smtp_abort(conn, 0);
	close(f);
	return D_TEMP_FAIL;
    }

    /* MAIL FROM */
    sprintf(line, "MAIL FROM:<%.200s>\r\n", login);
    smtp_writeline(conn, line);
    code = smtp_getreply(conn, replytext, sizeof(replytext));
    if (code / 100 != 2) {
	sprintf(Dropoff_ErrMsg, "MAIL FROM rejected: %d %.200s", code, replytext);
	smtp_abort(conn, 0);
	close(f);
	return D_TEMP_FAIL;
    }

    /* RCPT TO, one per recipient */
    anyAccepted = anyRejected = anyTempFail = 0;
    rejected[0] = '\0';
    for (i = 0; tolist[i] != NULL; i++) {
	sprintf(line, "RCPT TO:<%.200s>\r\n", tolist[i]);
	smtp_writeline(conn, line);
	code = smtp_getreply(conn, replytext, sizeof(replytext));
	if (code / 100 == 2) {
	    anyAccepted++;
	} else if (code / 100 == 5) {
	    anyRejected = 1;
	    if (strlen(rejected) + strlen(tolist[i]) + strlen(replytext) + 8 < sizeof(rejected)) {
		strcat(rejected, tolist[i]);
		strcat(rejected, " (");
		strcat(rejected, replytext);
		strcat(rejected, "); ");
	    }
	} else {
	    anyTempFail = 1;
	}
    }
    if (anyTempFail) {
	sprintf(Dropoff_ErrMsg, "Temporary RCPT failure talking to %.200s", host);
	smtp_abort(conn, 1);
	close(f);
	return D_TEMP_FAIL;
    }
    if (anyRejected) {
	sprintf(Dropoff_ErrMsg, "Recipient(s) rejected, message not sent: %.400s", rejected);
	smtp_abort(conn, 1);
	close(f);
	return D_BAD_PARMS;
    }

    /* DATA */
    smtp_writeline(conn, "DATA\r\n");
    code = smtp_getreply(conn, replytext, sizeof(replytext));
    if (code != 354) {
	sprintf(Dropoff_ErrMsg, "DATA rejected: %d %.200s", code, replytext);
	smtp_abort(conn, 1);
	close(f);
	return D_TEMP_FAIL;
    }

    /* Stream the body (this closes f). */
    if (smtp_send_body(conn, f, errbuf, sizeof(errbuf)) != 0) {
	sprintf(Dropoff_ErrMsg, "Error sending message body: %.400s", errbuf);
	tlscon_Close(conn);
	return D_TEMP_FAIL;
    }

    smtp_writeline(conn, ".\r\n");
    code = smtp_getreply(conn, replytext, sizeof(replytext));
    if (code / 100 == 2) {
	sprintf(Dropoff_ErrMsg, "%d %.200s", code, replytext);
	smtp_writeline(conn, "QUIT\r\n");
	(void) smtp_getreply(conn, replytext, sizeof(replytext));
	tlscon_Close(conn);
	return D_OK;
    } else if (code / 100 == 5) {
	sprintf(Dropoff_ErrMsg, "Message rejected after DATA: %d %.200s", code, replytext);
	smtp_writeline(conn, "QUIT\r\n");
	(void) smtp_getreply(conn, replytext, sizeof(replytext));
	tlscon_Close(conn);
	return D_BAD_PARMS;
    } else {
	sprintf(Dropoff_ErrMsg, "Temporary failure after DATA: %d %.200s", code, replytext);
	smtp_writeline(conn, "QUIT\r\n");
	(void) smtp_getreply(conn, replytext, sizeof(replytext));
	tlscon_Close(conn);
	return D_TEMP_FAIL;
    }
}
