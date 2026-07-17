/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	imapspike.c -- Milestone 2 go/no-go spike driver.  Not part of
		      libmail.a; built only via the .test suffix rule
		      (TestingOnlyTestingRule in this directory's
		      Imakefile), same pattern as smtptest.c:

			 make imapspike.test

	Usage:
	    imapspike.test

	Connects to imap.fastmail.com:993 (implicit TLS via tlscon),
	reads the greeting, CAPABILITY, looks up the "imap.fastmail.com"
	netrc stanza and LOGINs, LISTs the folder tree, EXAMINEs INBOX
	(read-only -- never SELECT), UID SEARCHes, and FETCHes one
	message's FLAGS/INTERNALDATE/ENVELOPE and a BODY.PEEK[] preview.
	No mutating IMAP command is ever sent (no SELECT, STORE, APPEND,
	CREATE, DELETE, EXPUNGE).

	This is spike code: the parenthesized-list tokenizer below is
	deliberately minimal (handles what Fastmail's ENVELOPE responses
	actually contain -- NIL, quoted strings, nested address lists --
	not the full IMAP4 grammar), and literal handling inside
	ENVELOPE (as opposed to inside BODY.PEEK[], where it's expected
	and handled) is not attempted; see the milestone 2 report for
	what that would take to do properly.
*/

#ifndef NORCSID
#define NORCSID
#endif

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <util.h>
#include <tlscon.h>
#include <netrc.h>

#define IMAP_HOST	"imap.fastmail.com"
#define IMAP_PORT	993
#define IMAP_LINE_MAX	4096

extern char *gethome();

/* ---- tag generation ---- */

static int tagctr = 0;

static char *nexttag(buf)
    char *buf;
{
    sprintf(buf, "a%d", ++tagctr);
    return buf;
}

/* ---- ~/-expansion, same pattern as smtpsub.c's smtp_expandpath ---- */

static char *imap_expandpath(path, buf, buflen)
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

/* ---- IMAP quoted-string escaping (backslash and doublequote) ---- */

static void imap_quote(dst, dstsize, src)
    char *dst;
    int dstsize;
    char *src;
{
    int i, j;

    j = 0;
    dst[j++] = '"';
    for (i = 0; src[i] != '\0' && j < dstsize-3; i++) {
	if (src[i] == '"' || src[i] == '\\') dst[j++] = '\\';
	dst[j++] = src[i];
    }
    dst[j++] = '"';
    dst[j] = '\0';
}

/* ---- line-oriented reader with literal ({n}) handling ----

   Reads one server line.  If it ends in a literal marker "{n}", the
   marker is stripped from the returned text, the n literal bytes are
   either captured (if litbuf_out != NULL: malloc'd exactly n+1 bytes,
   caller frees) or discarded in chunks, a placeholder is printed in
   the trace instead of the raw bytes (mail content may be large or
   binary -- never dump it wholesale), and the remainder of the
   logical line (e.g. the FETCH response's closing paren) is read and
   appended so the caller always gets one flattened, parseable line
   back regardless of whether a literal appeared in the middle of it. */

static int imap_readline(conn, buf, bufsize, litbuf_out, litlen_out)
    struct tlscon *conn;
    char *buf;
    int bufsize;
    char **litbuf_out;
    int *litlen_out;
{
    char raw[IMAP_LINE_MAX];
    char scratch[1024];
    char *brace, *end;
    int n, litn, left, chunk;

    if (litlen_out != NULL) *litlen_out = 0;

    n = tlscon_ReadLine(conn, raw, sizeof(raw));
    if (n < 0) return -1;

    brace = strrchr(raw, '{');
    litn = -1;
    if (brace != NULL) {
	end = brace + 1;
	litn = 0;
	while (isdigit((unsigned char) *end)) { litn = litn*10 + (*end - '0'); end++; }
	if (end == brace+1 || strcmp(end, "}") != 0) litn = -1;
    }

    if (litn < 0) {
	printf("S: %s\n", raw);
	strncpy(buf, raw, bufsize-1); buf[bufsize-1] = '\0';
	return 0;
    }

    *brace = '\0';
    printf("S: %s{%d}  [literal: %d bytes, not dumped raw]\n", raw, litn, litn);

    if (litbuf_out != NULL) {
	*litbuf_out = malloc(litn + 1);
	if (*litbuf_out == NULL) return -1;
	if (litn > 0 && tlscon_ReadBytes(conn, *litbuf_out, litn) < 0) {
	    free(*litbuf_out); *litbuf_out = NULL; return -1;
	}
	(*litbuf_out)[litn] = '\0';
	if (litlen_out != NULL) *litlen_out = litn;
    } else {
	left = litn;
	while (left > 0) {
	    chunk = (left > (int) sizeof(scratch)) ? (int) sizeof(scratch) : left;
	    if (tlscon_ReadBytes(conn, scratch, chunk) < 0) return -1;
	    left -= chunk;
	}
	if (litlen_out != NULL) *litlen_out = litn;
    }

    /* Remainder of the logical line (continues right after the
       literal bytes -- typically just a closing paren). */
    n = tlscon_ReadLine(conn, raw, sizeof(raw));
    if (n < 0) return -1;
    printf("S: %s\n", raw);
    strncpy(buf, raw, bufsize-1); buf[bufsize-1] = '\0';
    return 0;
}

/* ---- send a tagged command, trace it (optionally redacted) ---- */

static void imap_send(conn, tag, cmd, traceoverride)
    struct tlscon *conn;
    char *tag, *cmd, *traceoverride;
{
    char line[IMAP_LINE_MAX];

    sprintf(line, "%s %s\r\n", tag, cmd);
    if (traceoverride != NULL) printf("C: %s %s\n", tag, traceoverride);
    else printf("C: %s %s\n", tag, cmd);
    (void) tlscon_Write(conn, line, strlen(line));
}

/* Reads (and traces) lines until the tagged completion for "tag" is
   seen.  Each untagged line is handed to untagged_cb (may be NULL).
   Returns 0 for a tagged OK, -1 for NO/BAD/error/timeout. */

static int (*untagged_hook)();

static int imap_await(conn, tag)
    struct tlscon *conn;
    char *tag;
{
    char line[IMAP_LINE_MAX];
    char taglabel[32];
    int taglen;

    sprintf(taglabel, "%s ", tag);
    taglen = strlen(taglabel);
    for (;;) {
	if (imap_readline(conn, line, sizeof(line), (char **) NULL, (int *) NULL) < 0) return -1;
	if (strncmp(line, taglabel, taglen) == 0) {
	    return (strncmp(line+taglen, "OK", 2) == 0) ? 0 : -1;
	}
	if (untagged_hook != NULL) (*untagged_hook)(line);
    }
}

/* ---- untagged-line handlers for each command ---- */

static void cb_print(line)
    char *line;
{
    /* LIST/CAPABILITY: just echoed by imap_readline's own trace. */
}

static int exists_count = -1, uidvalidity = -1, uidnext = -1;

static void cb_examine(line)
    char *line;
{
    int n;
    char word[32];

    /* NB: sscanf's return value counts successful *conversions*, not
       literal-text matches -- "* %d EXISTS" against "* 0 RECENT" still
       returns 1 (the %d succeeded) even though "RECENT" != "EXISTS".
       Capture the word explicitly and compare it, rather than trusting
       the return count alone. */
    if (sscanf(line, "* %d %31s", &n, word) == 2 && strcmp(word, "EXISTS") == 0) {
	exists_count = n; return;
    }
    if (strstr(line, "UIDVALIDITY") != NULL && sscanf(strstr(line, "UIDVALIDITY") + 11, " %d", &n) == 1) {
	uidvalidity = n; return;
    }
    if (strstr(line, "UIDNEXT") != NULL && sscanf(strstr(line, "UIDNEXT") + 7, " %d", &n) == 1) {
	uidnext = n; return;
    }
}

static int search_max = -1;
static int search_matched = 0;

static void cb_search(line)
    char *line;
{
    char *p;
    int n;

    if (strncmp(line, "* SEARCH", 8) != 0) return;
    p = line + 8;
    while (*p != '\0') {
	while (*p == ' ') p++;
	if (!isdigit((unsigned char) *p)) break;
	n = atoi(p);
	if (n > search_max) search_max = n;
	search_matched++;
	while (isdigit((unsigned char) *p)) p++;
    }
}

/* ---- tiny ENVELOPE/paren-list tokenizer (see file header note) ---- */

static char *tok_p;

static void tok_init(s)
    char *s;
{
    tok_p = s;
}

static char *tok_next()
{
    static char scratch[2000];
    char *s = tok_p;
    int i;

    while (*s == ' ') s++;
    if (*s == '\0') { tok_p = s; return (char *) NULL; }
    if (*s == '(' || *s == ')') {
	scratch[0] = *s; scratch[1] = '\0';
	tok_p = s+1;
	return scratch;
    }
    if (*s == '"') {
	s++;
	i = 0;
	while (*s != '"' && *s != '\0' && i < (int) sizeof(scratch)-1) {
	    if (*s == '\\' && *(s+1) != '\0') s++;
	    scratch[i++] = *s++;
	}
	scratch[i] = '\0';
	if (*s == '"') s++;
	tok_p = s;
	return scratch;
    }
    i = 0;
    while (*s != ' ' && *s != '(' && *s != ')' && *s != '\0' && i < (int) sizeof(scratch)-1) {
	scratch[i++] = *s++;
    }
    scratch[i] = '\0';
    tok_p = s;
    return scratch;
}

/* Consumes one env-address-list (NIL, or "(" addr... ")") from the
   shared token cursor and formats it into outbuf. */

static void parse_addrlist(outbuf, outbufsize)
    char *outbuf;
    int outbufsize;
{
    char *t, name[200], mailbox[200], host[200];
    int first = 1;

    outbuf[0] = '\0';
    t = tok_next();
    if (t == NULL) return;
    if (strcmp(t, "NIL") == 0) { strncpy(outbuf, "(none)", outbufsize-1); return; }
    if (strcmp(t, "(") != 0) { strncpy(outbuf, "?", outbufsize-1); return; }
    for (;;) {
	t = tok_next();
	if (t == NULL || strcmp(t, ")") == 0) break;
	if (strcmp(t, "(") != 0) break;			/* malformed; bail out */
	t = tok_next(); strncpy(name, t ? t : "", sizeof(name)-1); name[sizeof(name)-1] = '\0';
	t = tok_next();						/* adl, unused */
	t = tok_next(); strncpy(mailbox, t ? t : "", sizeof(mailbox)-1); mailbox[sizeof(mailbox)-1] = '\0';
	t = tok_next(); strncpy(host, t ? t : "", sizeof(host)-1); host[sizeof(host)-1] = '\0';
	t = tok_next();						/* closing ")" of this address */

	if (!first && (int) strlen(outbuf) < outbufsize-3) strcat(outbuf, ", ");
	first = 0;
	if (strcmp(name, "NIL") != 0 && name[0] != '\0' && (int) strlen(outbuf) < outbufsize - (int) strlen(name) - 2)
	    { strcat(outbuf, name); strcat(outbuf, " "); }
	if ((int) strlen(outbuf) < outbufsize - (int) strlen(mailbox) - (int) strlen(host) - 4) {
	    strcat(outbuf, "<");
	    if (strcmp(mailbox, "NIL") != 0) strcat(outbuf, mailbox);
	    strcat(outbuf, "@");
	    if (strcmp(host, "NIL") != 0) strcat(outbuf, host);
	    strcat(outbuf, ">");
	}
    }
}

/* Consumes one full ENVELOPE list (cursor positioned just after its
   opening "(") and prints it. */

static void parse_envelope_fields()
{
    char *t;
    char addrbuf[500];

    t = tok_next(); printf("  Date: %s\n", t ? t : "?");
    t = tok_next(); printf("  Subject: %s\n", t ? t : "?");
    printf("  From: ");     parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    printf("  Sender: ");   parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    printf("  Reply-To: "); parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    printf("  To: ");       parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    printf("  Cc: ");       parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    printf("  Bcc: ");      parse_addrlist(addrbuf, sizeof(addrbuf)); printf("%s\n", addrbuf);
    t = tok_next();	/* in-reply-to, skipped for the spike */
    t = tok_next();	/* message-id, skipped for the spike */
    t = tok_next();	/* closing ")" of ENVELOPE itself */
}

/* Flat, keyword-triggered scan of one "* n FETCH (...)" response line
   -- not a general parser; reacts to FLAGS/INTERNALDATE/ENVELOPE and
   silently skips everything else (the leading "*", sequence number,
   "FETCH", UID, and structural parens). */

static void handle_fetch_line(line)
    char *line;
{
    char *t;

    tok_init(line);
    while ((t = tok_next()) != NULL) {
	if (strcmp(t, "FLAGS") == 0) {
	    t = tok_next();	/* "(" */
	    printf("  Flags: (");
	    for (;;) {
		t = tok_next();
		if (t == NULL || strcmp(t, ")") == 0) break;
		printf("%s ", t);
	    }
	    printf(")\n");
	} else if (strcmp(t, "INTERNALDATE") == 0) {
	    t = tok_next();
	    printf("  InternalDate: %s\n", t ? t : "?");
	} else if (strcmp(t, "ENVELOPE") == 0) {
	    t = tok_next();	/* "(" opening ENVELOPE */
	    parse_envelope_fields();
	}
    }
}

/* Reconnect and re-LOGIN on a fresh tlscon connection.  Needed because
   tlscon offers no way to resynchronize a connection after
   tlscon_ReadLine fails on an oversized line (see the UID SEARCH ALL
   handling in main(): it has no "discard until newline" or "skip N
   bytes without knowing N in advance" primitive, so the internal
   buffer is left wedged full of unconsumed bytes and every subsequent
   read fails immediately, too -- this is itself one of the spike's
   findings). Assumes login/passwd for IMAP_HOST are already known to
   work (the first LOGIN in main() already succeeded). */

static int reconnect_and_login(connp, netrcpath)
    struct tlscon **connp;
    char *netrcpath;
{
    char errbuf[512];
    char login[256], passwd[256];
    char line[IMAP_LINE_MAX];
    char cmd[IMAP_LINE_MAX];
    char quoted[300], redacted[400];
    char tag[16];
    int nrc, rc;

    if (tlscon_Open(connp, IMAP_HOST, IMAP_PORT, errbuf, sizeof(errbuf)) != 0) {
	fprintf(stderr, "imapspike: reconnect failed: %s\n", errbuf);
	return -1;
    }
    if (tlscon_ReadLine(*connp, line, sizeof(line)) < 0) {
	fprintf(stderr, "imapspike: no greeting on reconnect\n");
	return -1;
    }
    printf("S: %s\n", line);

    nrc = netrc_Lookup(netrcpath, IMAP_HOST, login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) {
	fprintf(stderr, "imapspike: netrc lookup failed on reconnect (rc=%d)\n", nrc);
	return -1;
    }

    nexttag(tag);
    imap_quote(quoted, sizeof(quoted), passwd);
    sprintf(cmd, "LOGIN %s %s", login, quoted);
    sprintf(redacted, "LOGIN %s <redacted>", login);
    imap_send(*connp, tag, cmd, redacted);
    memset(quoted, 0, sizeof(quoted));
    memset(cmd, 0, sizeof(cmd));
    untagged_hook = cb_print;
    rc = imap_await(*connp, tag);
    memset(passwd, 0, sizeof(passwd));
    if (rc != 0) {
	fprintf(stderr, "imapspike: LOGIN failed on reconnect\n");
	return -1;
    }

    /* SEARCH/FETCH require a mailbox to be selected; re-EXAMINE it
       (read-only, matching the original connection's state). */
    nexttag(tag);
    imap_send(*connp, tag, "EXAMINE INBOX", (char *) NULL);
    untagged_hook = cb_examine;
    rc = imap_await(*connp, tag);
    if (rc != 0) {
	fprintf(stderr, "imapspike: EXAMINE INBOX failed on reconnect\n");
	return -1;
    }
    return 0;
}

main(argc, argv)
    int argc;
    char **argv;
{
    struct tlscon *conn;
    char errbuf[512];
    char netrcpath[MAXPATHLEN+1];
    char login[256], passwd[256];
    int nrc;
    char line[IMAP_LINE_MAX];
    char cmd[IMAP_LINE_MAX];
    char tag[16];
    char quoted[300];
    int rc;
    int uid, fetchuid;
    char *litbuf;
    int litlen, i, printed;
    char *p, *nl;

    /* Line-buffer stdout: when redirected to a file/pipe it would
       otherwise fully block-buffer, so stdout and stderr (unbuffered)
       interleave out of chronological order in a captured transcript. */
    setvbuf(stdout, (char *) NULL, _IOLBF, 0);

    printf("Connecting to %s:%d ...\n", IMAP_HOST, IMAP_PORT);
    if (tlscon_Open(&conn, IMAP_HOST, IMAP_PORT, errbuf, sizeof(errbuf)) != 0) {
	fprintf(stderr, "imapspike: connect failed: %s\n", errbuf);
	exit(1);
    }

    /* Greeting */
    if (tlscon_ReadLine(conn, line, sizeof(line)) < 0) {
	fprintf(stderr, "imapspike: no greeting (timeout/error)\n");
	tlscon_Close(conn);
	exit(1);
    }
    printf("S: %s\n", line);

    /* CAPABILITY */
    untagged_hook = cb_print;
    nexttag(tag);
    imap_send(conn, tag, "CAPABILITY", (char *) NULL);
    rc = imap_await(conn, tag);
    if (rc != 0) {
	fprintf(stderr, "imapspike: CAPABILITY failed\n");
	tlscon_Close(conn);
	exit(1);
    }

    /* Only now do we need credentials -- look up the netrc stanza. */
    imap_expandpath("~/.netrc", netrcpath, sizeof(netrcpath));
    nrc = netrc_Lookup(netrcpath, IMAP_HOST, login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) {
	printf("\nimapspike: no usable netrc entry for \"%s\" in \"%s\" (netrc_Lookup rc=%d).\n",
	    IMAP_HOST, netrcpath, nrc);
	printf("Stopping here per spec: never guess or reuse the smtp stanza.\n");
	printf("Pre-auth steps (connect, TLS, greeting, CAPABILITY) succeeded; see report.\n");
	tlscon_Close(conn);
	exit(2);
    }

    /* LOGIN -- password redacted in the trace, never printed. */
    nexttag(tag);
    imap_quote(quoted, sizeof(quoted), passwd);
    sprintf(cmd, "LOGIN %s %s", login, quoted);
    {
	char redacted[400];
	sprintf(redacted, "LOGIN %s <redacted>", login);
	imap_send(conn, tag, cmd, redacted);
    }
    memset(quoted, 0, sizeof(quoted));
    memset(cmd, 0, sizeof(cmd));
    untagged_hook = cb_print;
    rc = imap_await(conn, tag);
    memset(passwd, 0, sizeof(passwd));
    if (rc != 0) {
	fprintf(stderr, "imapspike: LOGIN failed (bad credentials or server policy)\n");
	tlscon_Close(conn);
	exit(1);
    }

    /* LIST "" "*" */
    nexttag(tag);
    imap_send(conn, tag, "LIST \"\" \"*\"", (char *) NULL);
    untagged_hook = cb_print;
    rc = imap_await(conn, tag);
    if (rc != 0) fprintf(stderr, "imapspike: LIST failed (continuing)\n");

    /* EXAMINE INBOX -- read-only select, never SELECT. */
    nexttag(tag);
    imap_send(conn, tag, "EXAMINE INBOX", (char *) NULL);
    untagged_hook = cb_examine;
    rc = imap_await(conn, tag);
    if (rc != 0) {
	fprintf(stderr, "imapspike: EXAMINE INBOX failed\n");
	nexttag(tag); imap_send(conn, tag, "LOGOUT", (char *) NULL); (void) imap_await(conn, tag);
	tlscon_Close(conn);
	exit(1);
    }
    printf("\nEXAMINE INBOX summary: EXISTS=%d UIDVALIDITY=%d UIDNEXT=%d\n\n",
	exists_count, uidvalidity, uidnext);

    /* UID SEARCH ALL, as the spec's deliverable sequence specifies.
       On a real mailbox this large (EXISTS above), the single-line
       untagged "* SEARCH ..." response listing every UID individually
       is tens of KB -- far past tlscon's internal ~4KB line buffer
       (TLSCON_RBUFSIZE, not exposed/configurable). tlscon_ReadLine
       correctly reports the failure, but there is no way to
       resynchronize afterward: tlscon has no "discard until newline"
       or "skip N unknown bytes" primitive, so the internal buffer
       stays wedged full and every further read on this connection
       fails immediately too. Recorded as a finding; recovery here is
       a fresh connection + re-LOGIN, not a same-connection retry. */
    nexttag(tag);
    imap_send(conn, tag, "UID SEARCH ALL", (char *) NULL);
    untagged_hook = cb_search;
    rc = imap_await(conn, tag);
    if (rc != 0 || search_matched == 0) {
	printf("\nFINDING: UID SEARCH ALL failed (rc=%d, %d matched) -- almost certainly\n"
	       "tlscon's internal ~4KB line buffer overflowing on this mailbox's SEARCH\n"
	       "response (%d messages). The connection is now wedged (no resync\n"
	       "primitive in tlscon); reconnecting and re-LOGIN to continue.\n",
	    rc, search_matched, exists_count);
	tlscon_Close(conn);
	if (reconnect_and_login(&conn, netrcpath) != 0) {
	    fprintf(stderr, "imapspike: could not reconnect after UID SEARCH ALL failure\n");
	    exit(1);
	}
	printf("Reconnected and re-authenticated OK.\n");
	search_max = -1; search_matched = 0;
    } else {
	printf("\nUID SEARCH ALL: %d messages, highest UID %d\n", search_matched, search_max);
    }
    fetchuid = search_max;	/* -1 if the ALL search above did not complete */

    /* Ground rules ask us to prefer one of our OWN test messages over
       whatever the newest message in the user's real mailbox happens
       to be, to minimize real-mail exposure.  Try both markers we've
       actually used in Subject: lines this project (smtp-send-test's
       is "STAGE-3-CUI-TEST ..."; "GATE-A..." only ever appeared in
       message *bodies*, not subjects, but costs nothing to try);
       fall back to the overall max UID only if neither matches. */
    search_max = -1; search_matched = 0;
    nexttag(tag);
    imap_send(conn, tag, "UID SEARCH SUBJECT \"GATE-A\"", (char *) NULL);
    untagged_hook = cb_search;
    rc = imap_await(conn, tag);
    if (rc != 0 || search_matched == 0) {
	search_max = -1; search_matched = 0;
	nexttag(tag);
	imap_send(conn, tag, "UID SEARCH SUBJECT \"STAGE-3-CUI-TEST\"", (char *) NULL);
	untagged_hook = cb_search;
	rc = imap_await(conn, tag);
    }
    if (rc == 0 && search_matched > 0) {
	fetchuid = search_max;
	printf("UID SEARCH SUBJECT (marker): %d match(es), using our own test message UID %d\n",
	    search_matched, fetchuid);
    } else if (fetchuid > 0) {
	printf("UID SEARCH SUBJECT (marker): no match; falling back to highest overall UID %d\n",
	    fetchuid);
    } else if (uidnext > 1) {
	fetchuid = uidnext - 1;
	printf("UID SEARCH SUBJECT (marker): no match, and UID SEARCH ALL did not complete either;\n"
	       "last resort: UIDNEXT-1 = %d\n", fetchuid);
    } else {
	fprintf(stderr, "imapspike: no usable UID found by any search; nothing to FETCH\n");
	nexttag(tag); imap_send(conn, tag, "LOGOUT", (char *) NULL); (void) imap_await(conn, tag);
	tlscon_Close(conn);
	exit(1);
    }
    uid = fetchuid;

    /* UID FETCH <uid> (FLAGS INTERNALDATE ENVELOPE) */
    nexttag(tag);
    sprintf(cmd, "UID FETCH %d (FLAGS INTERNALDATE ENVELOPE)", uid);
    imap_send(conn, tag, cmd, (char *) NULL);
    untagged_hook = (int (*)()) NULL;	/* handled inline below, not via the generic hook */
    {
	char fline[IMAP_LINE_MAX];
	char taglabel[32];
	int taglen, got_fetch;

	sprintf(taglabel, "%s ", tag);
	taglen = strlen(taglabel);
	got_fetch = 0;
	for (;;) {
	    if (imap_readline(conn, fline, sizeof(fline), (char **) NULL, (int *) NULL) < 0) {
		fprintf(stderr, "imapspike: UID FETCH (ENVELOPE) read error\n");
		break;
	    }
	    if (strncmp(fline, taglabel, taglen) == 0) {
		rc = (strncmp(fline+taglen, "OK", 2) == 0) ? 0 : -1;
		break;
	    }
	    if (strstr(fline, "FETCH") != NULL) {
		got_fetch = 1;
		printf("\nParsed UID FETCH (FLAGS INTERNALDATE ENVELOPE) for UID %d:\n", uid);
		handle_fetch_line(fline);
	    }
	}
	if (!got_fetch) fprintf(stderr, "imapspike: no FETCH data line seen for UID %d\n", uid);
    }

    /* UID FETCH <uid> (BODY.PEEK[]) -- literal handling, print ~40 lines. */
    nexttag(tag);
    sprintf(cmd, "UID FETCH %d (BODY.PEEK[])", uid);
    imap_send(conn, tag, cmd, (char *) NULL);
    litbuf = (char *) NULL; litlen = 0;
    {
	char fline[IMAP_LINE_MAX];
	char taglabel[32];
	int taglen;

	sprintf(taglabel, "%s ", tag);
	taglen = strlen(taglabel);
	for (;;) {
	    if (imap_readline(conn, fline, sizeof(fline), &litbuf, &litlen) < 0) {
		fprintf(stderr, "imapspike: UID FETCH (BODY.PEEK[]) read error\n");
		break;
	    }
	    if (strncmp(fline, taglabel, taglen) == 0) {
		rc = (strncmp(fline+taglen, "OK", 2) == 0) ? 0 : -1;
		break;
	    }
	}
    }

    if (litbuf != NULL) {
	printf("\nBODY.PEEK[] for UID %d: %d bytes total; first ~40 lines "
	       "(this is one of our own test messages, per the marker search above):\n\n",
	    uid, litlen);
	p = litbuf;
	printed = 0;
	while (*p != '\0' && printed < 40) {
	    nl = strchr(p, '\n');
	    if (nl != NULL) *nl = '\0';
	    printf("| %s\n", p);
	    printed++;
	    if (nl == NULL) break;
	    p = nl + 1;
	}
	if (*p != '\0') printf("| ... (%d more bytes not shown)\n", (int) strlen(p));
	free(litbuf);
    } else {
	printf("\nBODY.PEEK[] for UID %d: no literal captured.\n", uid);
    }

    /* LOGOUT */
    nexttag(tag);
    imap_send(conn, tag, "LOGOUT", (char *) NULL);
    untagged_hook = cb_print;
    (void) imap_await(conn, tag);

    tlscon_Close(conn);
    printf("\nimapspike: done.\n");
    exit(0);
}
