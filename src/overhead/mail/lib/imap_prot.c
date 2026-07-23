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
	imap_prot.c -- IMAP protocol layer implementation.  See
		      imap_prot.h for the public API and its rationale.
		      ANSI C (C89 prototypes) throughout; no
		      scanf/sscanf/fscanf anywhere -- numbers are parsed
		      with strtoul/strtoull (+ endptr checks), keywords
		      with strncmp/strcasecmp.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>	/* strcasecmp */
#include <ctype.h>
#include <stdarg.h>

#include <tlscon.h>
#include <imap_prot.h>

#define IMAP_TAG_SIZE 16
#define IMAP_CAPBUF_SIZE 4096
#define IMAP_BODY_CHUNK 65536

struct imapconn {
    struct tlscon *tls;
    int alive;
    int tagctr;
    char *host;
    int port;
    char *capabilities;		/* normalized " TOKEN1 TOKEN2 ... ", uppercased */
    char errmsg[512];
    char *examined_mailbox;	/* NULL if none currently examined */
    int examined_readwrite;	/* 0 if examined_mailbox was EXAMINEd (read-only),
				   1 if SELECTed (read-write) -- see imap_Select,
				   added for the writeback milestone; imap_Reopen
				   re-issues whichever mode was last in effect */
    struct imap_mboxinfo mboxinfo;

    /* Offline unit-test source (see imap_TestParseEnvelope): when
       non-NULL, imap_linebuf_read/imap_next_token read from this
       in-memory buffer instead of conn->tls. NULL (the memset(0)
       default) for every real, live connection -- this is inert
       outside the canned-response test path. */
    const char *test_canned;
    size_t test_cannedlen;
    size_t test_cannedpos;
};

/* ---- tracing (IMAP_TRACE env var; mirrors smtpsub.c's AMS_SMTP_TRACE) ---- */

static int imap_tracing = -1;

static void imap_trace(const char *dir, const char *text)
{
    if (imap_tracing < 0) imap_tracing = (getenv("IMAP_TRACE") != NULL);
    if (!imap_tracing) return;
    if (strlen(text) > 200) {
        fprintf(stderr, "%s: %.180s... [%lu bytes total]\n", dir, text, (unsigned long) strlen(text));
    } else {
        fprintf(stderr, "%s: %s\n", dir, text);
    }
}

/* ---- small helpers ---- */

static char *imap_strdup(const char *s)
{
    char *p;
    size_t n;

    if (s == NULL) return NULL;
    n = strlen(s) + 1;
    p = (char *) malloc(n);
    if (p != NULL) memcpy(p, s, n);
    return p;
}

static void imap_seterr(struct imapconn *conn, const char *fmt, ...)
{
    va_list ap;

    if (conn == NULL) return;
    va_start(ap, fmt);
    vsnprintf(conn->errmsg, sizeof(conn->errmsg), fmt, ap);
    va_end(ap);
}

/* IMAP quoted-string escaping (backslash and doublequote). */
static void imap_quote(char *dst, size_t dstsize, const char *src)
{
    size_t i, j;

    j = 0;
    dst[j++] = '"';
    for (i = 0; src[i] != '\0' && j + 2 < dstsize; i++) {
        if (src[i] == '"' || src[i] == '\\') dst[j++] = '\\';
        dst[j++] = src[i];
    }
    dst[j++] = '"';
    dst[j] = '\0';
}

static void imap_nexttag(struct imapconn *conn, char *tagbuf, size_t tagbufsize)
{
    conn->tagctr++;
    snprintf(tagbuf, tagbufsize, "a%d", conn->tagctr);
}

/* Builds conn->capabilities as " TOKEN1 TOKEN2 ... " (uppercased, space
   padded at both ends so imap_Capable can do an exact-token substring
   search). Replaces any previous value (LOGIN's response may hand us
   a fresher, post-auth capability list -- see imap_Login). */
static void imap_setcap(struct imapconn *conn, const char *rawcaplist)
{
    size_t n, i;
    char *norm;

    if (conn->capabilities != NULL) free(conn->capabilities);
    n = strlen(rawcaplist);
    norm = (char *) malloc(n + 3);
    if (norm == NULL) { conn->capabilities = imap_strdup(" "); return; }
    norm[0] = ' ';
    for (i = 0; i < n; i++) norm[i + 1] = (char) toupper((unsigned char) rawcaplist[i]);
    norm[n + 1] = ' ';
    norm[n + 2] = '\0';
    conn->capabilities = norm;
}

int imap_Capable(struct imapconn *conn, const char *cap)
{
    char upcap[130];
    size_t i, n;

    if (conn == NULL || conn->capabilities == NULL || cap == NULL) return 0;
    n = strlen(cap);
    if (n > 126) n = 126;
    upcap[0] = ' ';
    for (i = 0; i < n; i++) upcap[i + 1] = (char) toupper((unsigned char) cap[i]);
    upcap[n + 1] = ' ';
    upcap[n + 2] = '\0';
    return strstr(conn->capabilities, upcap) != NULL;
}

const char *imap_ErrMsg(struct imapconn *conn)
{
    static const char empty[] = "";
    if (conn == NULL) return empty;
    return conn->errmsg;
}

/* ---- command sending ---- */

/* Sends "<tag> <cmdtext>\r\n". If tracetext is non-NULL, it is traced
   instead of cmdtext (LOGIN uses this to redact the password).
   Returns 0 on success; on write failure marks the connection dead
   and returns -1 (caller maps that to IMAP_DEAD). */
static int imap_sendcmd(struct imapconn *conn, const char *tag,
                         const char *cmdtext, const char *tracetext)
{
    char *line;
    size_t linelen;
    int rc;
    char tracebuf[700];

    if (!conn->alive || conn->tls == NULL) {
        imap_seterr(conn, "connection is dead");
        return -1;
    }

    linelen = strlen(tag) + 1 + strlen(cmdtext) + 3;
    line = (char *) malloc(linelen);
    if (line == NULL) {
        imap_seterr(conn, "out of memory building command");
        return -1;
    }
    sprintf(line, "%s %s\r\n", tag, cmdtext);

    snprintf(tracebuf, sizeof(tracebuf), "%s %s", tag, tracetext != NULL ? tracetext : cmdtext);
    imap_trace("C", tracebuf);

    rc = tlscon_Write(conn->tls, line, (int) strlen(line));
    free(line);
    if (rc < 0) {
        conn->alive = 0;
        imap_seterr(conn, "write failed; connection is now dead");
        return -1;
    }
    return 0;
}

/* Builds and sends "<before><var><after>" as one command, sized to
   whatever length var actually is.  For commands that embed a
   caller-supplied uidset or search criteria: those can exceed any fixed
   staging buffer (a 100-uid uidset of 5-digit uids is ~700 bytes), and
   snprintf truncation there produces a syntactically mangled command
   the server rejects.  Same return convention as imap_sendcmd. */
static int imap_sendcmd_var(struct imapconn *conn, const char *tag,
                             const char *before, const char *var,
                             const char *after)
{
    char *cmdtext;
    int rc;

    cmdtext = (char *) malloc(strlen(before) + strlen(var) + strlen(after) + 1);
    if (cmdtext == NULL) {
        imap_seterr(conn, "out of memory building command");
        return -1;
    }
    strcpy(cmdtext, before);
    strcat(cmdtext, var);
    strcat(cmdtext, after);
    rc = imap_sendcmd(conn, tag, cmdtext, NULL);
    free(cmdtext);
    return rc;
}

/* ---- physical-line reader with literal detection ---- */

struct imap_linebuf {
    char *data;
    size_t len;
    size_t pos;
    unsigned long pending_literal;
};

static void imap_linebuf_free(struct imap_linebuf *lb)
{
    if (lb->data != NULL) free(lb->data);
    lb->data = NULL;
    lb->len = 0;
    lb->pos = 0;
    lb->pending_literal = 0;
}

/* ---- canned in-memory source, for offline unit testing of the
   literal-aware tokenizer (see imap_TestParseEnvelope). Mirrors
   tlscon_ReadLineAlloc/tlscon_ReadBytes's contracts exactly, reading
   from conn->test_canned instead of the network. Never invoked by any
   live code path (conn->test_canned is NULL for every real
   connection, checked at each of the two call sites below). */

static int imap_canned_readline(struct imapconn *conn, char **linep)
{
    const char *base = conn->test_canned + conn->test_cannedpos;
    const char *nl;
    size_t remaining, linelen;
    char *out;

    remaining = conn->test_cannedlen - conn->test_cannedpos;
    if (remaining == 0) return -1;
    nl = (const char *) memchr(base, '\n', remaining);
    if (nl == NULL) return -1;
    linelen = (size_t) (nl - base);
    out = (char *) malloc(linelen + 1);
    if (out == NULL) return -1;
    memcpy(out, base, linelen);
    if (linelen > 0 && out[linelen - 1] == '\r') linelen--;
    out[linelen] = '\0';
    conn->test_cannedpos += (size_t) (nl - base) + 1;
    *linep = out;
    return (int) linelen;
}

static int imap_canned_readbytes(struct imapconn *conn, char *buf, int n)
{
    size_t remaining = conn->test_cannedlen - conn->test_cannedpos;
    if (n < 0 || (size_t) n > remaining) return -1;
    memcpy(buf, conn->test_canned + conn->test_cannedpos, (size_t) n);
    conn->test_cannedpos += (size_t) n;
    return n;
}

/* Reads one physical line via tlscon_ReadLineAlloc (or the canned
   in-memory source in test mode), detects and strips a trailing "{n}"
   literal marker (leaving lb->pending_literal set for the tokenizer
   -- see imap_next_token -- to pick up). Returns 0 on success, -1 on
   failure (connection marked dead). */
static int imap_linebuf_read(struct imapconn *conn, struct imap_linebuf *lb)
{
    char *line;
    int n;
    size_t len;
    char *brace;

    imap_linebuf_free(lb);

    n = (conn->test_canned != NULL) ? imap_canned_readline(conn, &line)
                                     : tlscon_ReadLineAlloc(conn->tls, &line);
    if (n < 0) {
        conn->alive = 0;
        imap_seterr(conn, "read failed; connection is now dead");
        return -1;
    }
    len = (size_t) n;

    brace = strrchr(line, '{');
    if (brace != NULL) {
        char *end = brace + 1;
        if (isdigit((unsigned char) *end)) {
            char *endptr;
            unsigned long v = strtoul(end, &endptr, 10);
            if (endptr != end && *endptr == '}' && *(endptr + 1) == '\0') {
                len = (size_t) (brace - line);
                line[len] = '\0';
                lb->data = line;
                lb->len = len;
                lb->pos = 0;
                lb->pending_literal = v;
                imap_trace("S", line);
                if (v > 0) {
                    char tbuf[64];
                    snprintf(tbuf, sizeof(tbuf), "  [literal marker: %lu bytes to follow]", v);
                    imap_trace("S", tbuf);
                }
                return 0;
            }
        }
    }

    lb->data = line;
    lb->len = len;
    lb->pos = 0;
    lb->pending_literal = 0;
    imap_trace("S", line);
    return 0;
}

/* ---- unsolicited-update tracking (EXISTS/EXPUNGE/FETCH mid-command) ---- */

static void imap_check_unsolicited(struct imapconn *conn, const char *line)
{
    const char *p;
    char *endptr;
    unsigned long n;

    if (line[0] != '*' || line[1] != ' ') return;
    p = line + 2;
    if (!isdigit((unsigned char) *p)) return;
    n = strtoul(p, &endptr, 10);
    if (endptr == p) return;
    while (*endptr == ' ') endptr++;
    if (strncmp(endptr, "EXISTS", 6) == 0 && (endptr[6] == '\0' || endptr[6] == ' ')) {
        conn->mboxinfo.exists = (long) n;
    }
    /* EXPUNGE and unsolicited FETCH: the line has already been fully
       read by the caller, so there is nothing further needed here to
       avoid desyncing the stream; we simply don't act on their
       content in this milestone (per the spec's Parsing requirements). */
}

/* ---- literal-aware tokenizer (weaves tlscon_ReadBytes into token parsing) ---- */

enum imap_toktype { IMAP_TOK_EOF, IMAP_TOK_LPAREN, IMAP_TOK_RPAREN, IMAP_TOK_ATOM, IMAP_TOK_STRING, IMAP_TOK_NIL };

struct imap_token {
    enum imap_toktype type;
    char *value;	/* malloc'd for ATOM/STRING (including literal-sourced
			   STRING tokens); NULL otherwise */
};

static void imap_token_free(struct imap_token *t)
{
    if (t->value != NULL) free(t->value);
    t->value = NULL;
}

/* Returns the next token from lb. If lb's buffered text is exhausted
   and lb->pending_literal is nonzero (this physical line ended in a
   "{n}" marker), the literal's n bytes are read directly off the wire
   via tlscon_ReadBytes -- not malloc'd-whole-message, just this one
   field's worth, which is always small (a header field, not a body)
   -- and returned as a single STRING token; the next physical line
   (the logical response's continuation, e.g. ENVELOPE's closing
   parens) is then read automatically so tokenizing can carry on
   transparently. This is what closes the milestone 2 spike's known
   stub: a literal appearing inside ENVELOPE now parses like any other
   string, indistinguishable to the caller from a quoted string.
   Returns 0 on success (including a clean IMAP_TOK_EOF), -1 on a read
   failure (connection now dead). */
static int imap_next_token(struct imapconn *conn, struct imap_linebuf *lb, struct imap_token *tok)
{
    tok->type = IMAP_TOK_EOF;
    tok->value = NULL;

    while (lb->pos < lb->len && lb->data[lb->pos] == ' ') lb->pos++;

    if (lb->pos >= lb->len) {
        if (lb->pending_literal > 0) {
            unsigned long n = lb->pending_literal;
            char *data = (char *) malloc(n + 1);
            unsigned long got;

            if (data == NULL) {
                imap_seterr(conn, "out of memory reading literal token");
                return -1;
            }
            got = 0;
            while (got < n) {
                unsigned long want = (n - got) > IMAP_BODY_CHUNK ? IMAP_BODY_CHUNK : (n - got);
                int rr = (conn->test_canned != NULL)
                             ? imap_canned_readbytes(conn, data + got, (int) want)
                             : tlscon_ReadBytes(conn->tls, data + got, (int) want);
                if (rr < 0) {
                    free(data);
                    conn->alive = 0;
                    imap_seterr(conn, "read failed reading literal token; connection is now dead");
                    return -1;
                }
                got += want;
            }
            data[n] = '\0';
            lb->pending_literal = 0;
            {
                char tbuf[64];
                snprintf(tbuf, sizeof(tbuf), "  [%lu-byte literal token consumed]", n);
                imap_trace("S", tbuf);
            }
            if (imap_linebuf_read(conn, lb) < 0) { free(data); return -1; }
            tok->type = IMAP_TOK_STRING;
            tok->value = data;
            return 0;
        }
        return 0;	/* IMAP_TOK_EOF */
    }

    if (lb->data[lb->pos] == '(') { lb->pos++; tok->type = IMAP_TOK_LPAREN; return 0; }
    if (lb->data[lb->pos] == ')') { lb->pos++; tok->type = IMAP_TOK_RPAREN; return 0; }

    if (lb->data[lb->pos] == '"') {
        size_t start, i, j;
        char *val;

        lb->pos++;
        start = lb->pos;
        i = start; j = 0;
        while (i < lb->len && lb->data[i] != '"') {
            if (lb->data[i] == '\\' && i + 1 < lb->len) i++;
            i++; j++;
        }
        val = (char *) malloc(j + 1);
        if (val == NULL) { imap_seterr(conn, "out of memory reading quoted string"); return -1; }
        i = start; j = 0;
        while (i < lb->len && lb->data[i] != '"') {
            if (lb->data[i] == '\\' && i + 1 < lb->len) i++;
            val[j++] = lb->data[i++];
        }
        val[j] = '\0';
        if (i < lb->len && lb->data[i] == '"') i++;
        lb->pos = i;
        tok->type = IMAP_TOK_STRING;
        tok->value = val;
        return 0;
    }

    {
        size_t start = lb->pos;
        size_t n;
        char *val;

        while (lb->pos < lb->len && lb->data[lb->pos] != ' ' &&
               lb->data[lb->pos] != '(' && lb->data[lb->pos] != ')') {
            lb->pos++;
        }
        n = lb->pos - start;
        val = (char *) malloc(n + 1);
        if (val == NULL) { imap_seterr(conn, "out of memory reading atom"); return -1; }
        memcpy(val, lb->data + start, n);
        val[n] = '\0';
        if (strcmp(val, "NIL") == 0) {
            free(val);
            tok->type = IMAP_TOK_NIL;
            tok->value = NULL;
        } else {
            tok->type = IMAP_TOK_ATOM;
            tok->value = val;
        }
        return 0;
    }
}

/* If a token holds an owned string (ATOM or STRING), transfers that
   ownership to the caller (as a plain malloc'd char*, NUL-terminated)
   and clears the token; NIL/EOF/paren tokens yield NULL. Used to
   populate struct imap_envelope fields, where NULL means "absent"
   (matching NIL). */
static char *imap_tok_to_str(struct imap_token *tok)
{
    if (tok->type == IMAP_TOK_STRING || tok->type == IMAP_TOK_ATOM) {
        char *v = tok->value;
        tok->value = NULL;
        return v;
    }
    return NULL;
}

/* ---- generic "read lines until tagged completion" driver ----

   Used by every command whose untagged responses are line-oriented
   text with no literal continuation expected (CAPABILITY, LOGIN,
   LOGOUT, LIST, SEARCH/ESEARCH, EXAMINE). FETCH (imap_UidFetchMeta,
   imap_UidFetchBody) is literal-aware in its own untagged-response
   handling and does not use this driver -- see those functions. */

static int imap_await_tagged(struct imapconn *conn, const char *tag,
    void (*untagged_cb)(struct imapconn *, const char *, void *), void *rock,
    char *tagline_out, size_t tagline_outsize)
{
    struct imap_linebuf lb;
    char taglabel[IMAP_TAG_SIZE + 2];
    size_t taglen;
    int rc;

    memset(&lb, 0, sizeof(lb));
    snprintf(taglabel, sizeof(taglabel), "%s ", tag);
    taglen = strlen(taglabel);

    for (;;) {
        if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;

        if (lb.len >= taglen && strncmp(lb.data, taglabel, taglen) == 0) {
            const char *status = lb.data + taglen;
            if (strncmp(status, "OK", 2) == 0) rc = IMAP_OK;
            else if (strncmp(status, "NO", 2) == 0) rc = IMAP_NO;
            else rc = IMAP_BAD;
            if (rc != IMAP_OK) imap_seterr(conn, "%s", lb.data);
            if (tagline_out != NULL && tagline_outsize > 0) {
                strncpy(tagline_out, lb.data, tagline_outsize - 1);
                tagline_out[tagline_outsize - 1] = '\0';
            }
            if (lb.pending_literal > 0) {
                struct imap_token junk;
                (void) imap_next_token(conn, &lb, &junk);
                imap_token_free(&junk);
            }
            imap_linebuf_free(&lb);
            return rc;
        }

        imap_check_unsolicited(conn, lb.data);
        if (untagged_cb != NULL) untagged_cb(conn, lb.data, rock);

        /* Defensive: none of this driver's commands are expected to
           produce a literal, but drain one if it somehow occurs, so
           the stream can't desync even in that case. */
        if (lb.pending_literal > 0) {
            struct imap_token junk;
            (void) imap_next_token(conn, &lb, &junk);
            imap_token_free(&junk);
        }
    }
}

/* ---- CAPABILITY ---- */

static void imap_cb_capability(struct imapconn *conn, const char *line, void *rockp)
{
    char *capbuf = (char *) rockp;
    const char *p;

    (void) conn;
    if (strncmp(line, "* CAPABILITY", 12) != 0) return;
    p = line + 12;
    while (*p == ' ') p++;
    if (strlen(capbuf) + 1 + strlen(p) < IMAP_CAPBUF_SIZE) {
        strcat(capbuf, " ");
        strcat(capbuf, p);
    }
}

/* ---- connection lifecycle ---- */

int imap_Open(struct imapconn **connp, const char *host, int port, char *errbuf, int errlen)
{
    struct tlscon *tls;
    struct imapconn *conn;
    char *line;
    int n;
    char tag[IMAP_TAG_SIZE];
    char capbuf[IMAP_CAPBUF_SIZE];
    int rc;

    if (errbuf != NULL && errlen > 0) errbuf[0] = '\0';
    if (connp == NULL || host == NULL) return IMAP_BAD;

    if (imap_tracing < 0) imap_tracing = (getenv("IMAP_TRACE") != NULL);

    if (tlscon_Open(&tls, host, port, errbuf, errlen) != 0) return IMAP_DEAD;

    n = tlscon_ReadLineAlloc(tls, &line);
    if (n < 0) {
        if (errbuf != NULL) snprintf(errbuf, (size_t) errlen, "no greeting from server");
        tlscon_Close(tls);
        return IMAP_DEAD;
    }
    imap_trace("S", line);
    if (strncmp(line, "* OK", 4) != 0 && strncmp(line, "* PREAUTH", 9) != 0) {
        if (errbuf != NULL) snprintf(errbuf, (size_t) errlen, "unexpected greeting: %s", line);
        free(line);
        tlscon_Close(tls);
        return IMAP_BAD;
    }
    free(line);

    conn = (struct imapconn *) malloc(sizeof(struct imapconn));
    if (conn == NULL) {
        if (errbuf != NULL) snprintf(errbuf, (size_t) errlen, "out of memory");
        tlscon_Close(tls);
        return IMAP_DEAD;
    }
    memset(conn, 0, sizeof(struct imapconn));
    conn->tls = tls;
    conn->alive = 1;
    conn->tagctr = 0;
    conn->host = imap_strdup(host);
    conn->port = port;
    conn->capabilities = imap_strdup(" ");
    conn->errmsg[0] = '\0';
    conn->examined_mailbox = NULL;

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, "CAPABILITY", NULL) < 0) {
        if (errbuf != NULL) snprintf(errbuf, (size_t) errlen, "%s", conn->errmsg);
        imap_Close(conn);
        return IMAP_DEAD;
    }

    capbuf[0] = '\0';
    rc = imap_await_tagged(conn, tag, imap_cb_capability, capbuf, NULL, 0);
    if (rc != IMAP_OK) {
        if (errbuf != NULL) snprintf(errbuf, (size_t) errlen, "CAPABILITY failed: %s", conn->errmsg);
        imap_Close(conn);
        return rc;
    }
    imap_setcap(conn, capbuf);

    *connp = conn;
    return IMAP_OK;
}

int imap_Login(struct imapconn *conn, const char *login, const char *passwd)
{
    char tag[IMAP_TAG_SIZE];
    char quoted[300];
    char cmdtext[600];
    char tracetext[400];
    char tagline[1024];
    int rc, sendrc;

    if (conn == NULL || login == NULL || passwd == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    imap_quote(quoted, sizeof(quoted), passwd);
    snprintf(cmdtext, sizeof(cmdtext), "LOGIN %s %s", login, quoted);
    snprintf(tracetext, sizeof(tracetext), "LOGIN %s <redacted>", login);

    imap_nexttag(conn, tag, sizeof(tag));
    sendrc = imap_sendcmd(conn, tag, cmdtext, tracetext);
    memset(quoted, 0, sizeof(quoted));
    memset(cmdtext, 0, sizeof(cmdtext));
    if (sendrc < 0) return IMAP_DEAD;

    tagline[0] = '\0';
    rc = imap_await_tagged(conn, tag, NULL, NULL, tagline, sizeof(tagline));
    if (rc == IMAP_OK) {
        /* Fastmail (and many servers) hand back a fresher, post-auth
           CAPABILITY list as a response code on the LOGIN OK; prefer
           it over the pre-auth list captured in imap_Open. */
        char *p = strstr(tagline, "[CAPABILITY ");
        if (p != NULL) {
            char *start = p + 12;
            char *end = strchr(start, ']');
            if (end != NULL) {
                size_t len = (size_t) (end - start);
                char *tmp = (char *) malloc(len + 1);
                if (tmp != NULL) {
                    memcpy(tmp, start, len);
                    tmp[len] = '\0';
                    imap_setcap(conn, tmp);
                    free(tmp);
                }
            }
        }
    }
    return rc;
}

void imap_Close(struct imapconn *conn)
{
    if (conn == NULL) return;
    if (conn->alive && conn->tls != NULL) {
        char tag[IMAP_TAG_SIZE];
        imap_nexttag(conn, tag, sizeof(tag));
        if (imap_sendcmd(conn, tag, "LOGOUT", NULL) == 0) {
            (void) imap_await_tagged(conn, tag, NULL, NULL, NULL, 0);
        }
    }
    if (conn->tls != NULL) tlscon_Close(conn->tls);
    if (conn->host != NULL) free(conn->host);
    if (conn->capabilities != NULL) free(conn->capabilities);
    if (conn->examined_mailbox != NULL) free(conn->examined_mailbox);
    free(conn);
}

int imap_TestForceClose(struct imapconn *conn)
{
    if (conn == NULL) return IMAP_BAD;
    if (conn->tls != NULL) { tlscon_Close(conn->tls); conn->tls = NULL; }
    conn->alive = 0;
    imap_seterr(conn, "connection forcibly closed (test hook)");
    return IMAP_OK;
}

int imap_Reopen(struct imapconn *conn, const char *login, const char *passwd)
{
    char errbuf[256];
    struct tlscon *tls;
    char *line;
    int n;
    char tag[IMAP_TAG_SIZE];
    char capbuf[IMAP_CAPBUF_SIZE];
    int rc;
    unsigned long old_uidvalidity;

    if (conn == NULL || login == NULL || passwd == NULL) return IMAP_BAD;

    if (conn->tls != NULL) { tlscon_Close(conn->tls); conn->tls = NULL; }
    conn->alive = 0;

    if (tlscon_Open(&tls, conn->host, conn->port, errbuf, sizeof(errbuf)) != 0) {
        imap_seterr(conn, "reconnect failed: %s", errbuf);
        return IMAP_DEAD;
    }
    conn->tls = tls;
    conn->alive = 1;
    conn->tagctr = 0;

    n = tlscon_ReadLineAlloc(conn->tls, &line);
    if (n < 0) {
        imap_seterr(conn, "no greeting on reconnect");
        conn->alive = 0;
        return IMAP_DEAD;
    }
    imap_trace("S", line);
    free(line);

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, "CAPABILITY", NULL) < 0) return IMAP_DEAD;
    capbuf[0] = '\0';
    rc = imap_await_tagged(conn, tag, imap_cb_capability, capbuf, NULL, 0);
    if (rc != IMAP_OK) return rc;
    imap_setcap(conn, capbuf);

    rc = imap_Login(conn, login, passwd);
    if (rc != IMAP_OK) return rc;

    if (conn->examined_mailbox != NULL) {
        struct imap_mboxinfo info;
        char *mailbox;
        int wasreadwrite;

        old_uidvalidity = conn->mboxinfo.uidvalidity;
        mailbox = imap_strdup(conn->examined_mailbox);
        wasreadwrite = conn->examined_readwrite;
        /* Re-select in whatever mode was in effect before the drop --
           a replay in progress needs read-write back, not a downgrade
           to EXAMINE that would make its next STORE/EXPUNGE fail. */
        rc = wasreadwrite ? imap_Select(conn, mailbox, &info) : imap_Examine(conn, mailbox, &info);
        if (mailbox != NULL) free(mailbox);
        if (rc != IMAP_OK) return rc;
        if (old_uidvalidity != 0 && info.uidvalidity != old_uidvalidity) {
            return IMAP_UIDCHANGED;
        }
    }

    return IMAP_OK;
}

/* ---- LIST ---- */

struct imap_list_rock {
    int (*cb)(const char *name, const char *delim, const char *flags, void *rock);
    void *userrock;
};

/* Tokenizes an already-fully-read "* LIST (...) delim name" line.
   Borrows `line` into a local, non-owning linebuf (pending_literal is
   left 0, so imap_next_token's literal branch -- the only one that
   would try to free/replace the buffer -- can never trigger here; a
   defensive drain in imap_await_tagged already handles the
   (unobserved in practice) case of a literal actually appearing in a
   LIST response). */
static void imap_cb_list(struct imapconn *conn, const char *line, void *rockp)
{
    struct imap_list_rock *r = (struct imap_list_rock *) rockp;
    struct imap_linebuf lb;
    struct imap_token tok;
    char flags[512];
    char delim[64];
    char name[1024];
    char *p;

    if (strncmp(line, "* LIST", 6) != 0) return;

    lb.data = (char *) line;
    lb.len = strlen(line);
    lb.pos = 6;
    lb.pending_literal = 0;

    flags[0] = '\0';
    if (imap_next_token(conn, &lb, &tok) < 0) return;
    if (tok.type == IMAP_TOK_LPAREN) {
        int first = 1;
        imap_token_free(&tok);
        for (;;) {
            if (imap_next_token(conn, &lb, &tok) < 0) return;
            if (tok.type == IMAP_TOK_RPAREN || tok.type == IMAP_TOK_EOF) { imap_token_free(&tok); break; }
            if (!first) strncat(flags, " ", sizeof(flags) - strlen(flags) - 1);
            first = 0;
            if (tok.value != NULL) strncat(flags, tok.value, sizeof(flags) - strlen(flags) - 1);
            imap_token_free(&tok);
        }
    } else {
        imap_token_free(&tok);
    }

    delim[0] = '\0';
    if (imap_next_token(conn, &lb, &tok) < 0) return;
    if (tok.value != NULL) { strncpy(delim, tok.value, sizeof(delim) - 1); delim[sizeof(delim) - 1] = '\0'; }
    imap_token_free(&tok);

    name[0] = '\0';
    if (imap_next_token(conn, &lb, &tok) < 0) return;
    if (tok.value != NULL) { strncpy(name, tok.value, sizeof(name) - 1); name[sizeof(name) - 1] = '\0'; }
    imap_token_free(&tok);

    p = name;
    if (r->cb != NULL) (void) r->cb(p, delim, flags, r->userrock);
}

int imap_List(struct imapconn *conn, const char *ref, const char *pattern,
              int (*cb)(const char *, const char *, const char *, void *), void *rock)
{
    char tag[IMAP_TAG_SIZE];
    char cmdtext[600];
    char qref[300], qpat[300];
    struct imap_list_rock lrock;

    if (conn == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    imap_quote(qref, sizeof(qref), ref != NULL ? ref : "");
    imap_quote(qpat, sizeof(qpat), pattern != NULL ? pattern : "*");
    snprintf(cmdtext, sizeof(cmdtext), "LIST %s %s", qref, qpat);

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, cmdtext, NULL) < 0) return IMAP_DEAD;

    lrock.cb = cb;
    lrock.userrock = rock;
    return imap_await_tagged(conn, tag, imap_cb_list, &lrock, NULL, 0);
}

/* ---- EXAMINE / SELECT ---- */

static void imap_cb_examine(struct imapconn *conn, const char *line, void *rockp)
{
    struct imap_mboxinfo *info = (struct imap_mboxinfo *) rockp;
    const char *p;
    char *endptr;
    unsigned long n;
    char word[32];
    size_t i;

    if (line[0] != '*' || line[1] != ' ') return;
    p = line + 2;

    if (isdigit((unsigned char) *p)) {
        n = strtoul(p, &endptr, 10);
        if (endptr != p) {
            const char *q = endptr;
            while (*q == ' ') q++;
            i = 0;
            while (q[i] != '\0' && q[i] != ' ' && i < sizeof(word) - 1) { word[i] = q[i]; i++; }
            word[i] = '\0';
            if (strcmp(word, "EXISTS") == 0) { info->exists = (long) n; conn->mboxinfo.exists = (long) n; }
            return;
        }
    }

    if (strncmp(p, "OK [", 4) == 0) {
        const char *code = p + 4;
        if (strncmp(code, "UIDVALIDITY ", 12) == 0) {
            n = strtoul(code + 12, &endptr, 10);
            if (endptr != code + 12) info->uidvalidity = n;
        } else if (strncmp(code, "UIDNEXT ", 8) == 0) {
            n = strtoul(code + 8, &endptr, 10);
            if (endptr != code + 8) info->uidnext = n;
        } else if (strncmp(code, "HIGHESTMODSEQ ", 14) == 0) {
            unsigned long long v = strtoull(code + 14, &endptr, 10);
            if (endptr != code + 14) info->highestmodseq = v;
        }
    }
}

/* Shared by imap_Examine and imap_Select (writeback milestone):
   identical wire dialogue and untagged-response parsing either way
   (FLAGS/EXISTS/UIDVALIDITY/UIDNEXT/HIGHESTMODSEQ are returned the
   same shape by both commands per RFC 3501 -- SELECT is EXAMINE plus
   read-write permission), differing only in the command keyword and
   in what gets remembered for imap_Reopen to replay on reconnect. */
static int imap_do_select(struct imapconn *conn, const char *mailbox,
                           struct imap_mboxinfo *out, int readwrite)
{
    char tag[IMAP_TAG_SIZE];
    char cmdtext[300];
    struct imap_mboxinfo info;
    int rc;

    if (conn == NULL || mailbox == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    memset(&info, 0, sizeof(info));
    snprintf(cmdtext, sizeof(cmdtext), "%s %s", readwrite ? "SELECT" : "EXAMINE", mailbox);
    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, cmdtext, NULL) < 0) return IMAP_DEAD;

    rc = imap_await_tagged(conn, tag, imap_cb_examine, &info, NULL, 0);
    if (rc == IMAP_OK) {
        conn->mboxinfo = info;
        conn->examined_readwrite = readwrite;
        if (conn->examined_mailbox != NULL) free(conn->examined_mailbox);
        conn->examined_mailbox = imap_strdup(mailbox);
        if (out != NULL) *out = info;
    }
    return rc;
}

int imap_Examine(struct imapconn *conn, const char *mailbox, struct imap_mboxinfo *out)
{
    return imap_do_select(conn, mailbox, out, 0);
}

/* Real implementation as of the writeback milestone (M4): previously a
   permanent stub (see the DIVERGENCE comment this replaces in
   imap_prot.h), always returning IMAP_BAD. Nothing in the tree ever
   called it while it was a stub (grep confirms), so there is no prior
   behavior to preserve here -- this is the first real behavior this
   entry point has ever had. Needed because imap_UidStoreFlags/
   imap_UidExpunge require a read-write-selected mailbox; EXAMINE's
   mailbox access is read-only by protocol definition and a server may
   (Fastmail does) reject STORE/EXPUNGE issued against an EXAMINEd
   mailbox with a tagged NO. */
int imap_Select(struct imapconn *conn, const char *mailbox, struct imap_mboxinfo *out)
{
    return imap_do_select(conn, mailbox, out, 1);
}

/* ---- UID SEARCH (ESEARCH-preferring, plain-SEARCH fallback) ---- */

static int imap_growuidarr(unsigned long **arr, long *cap, long need)
{
    long newcap;
    unsigned long *tmp;

    if (need <= *cap) return 0;
    newcap = *cap > 0 ? *cap * 2 : 1024;
    while (newcap < need) newcap *= 2;
    tmp = (unsigned long *) realloc(*arr, (size_t) newcap * sizeof(unsigned long));
    if (tmp == NULL) return -1;
    *arr = tmp;
    *cap = newcap;
    return 0;
}

static int imap_uidarr_append(unsigned long **arr, long *count, long *cap, unsigned long v)
{
    if (imap_growuidarr(arr, cap, *count + 1) < 0) return -1;
    (*arr)[*count] = v;
    (*count)++;
    return 0;
}

/* Expands a compressed IMAP sequence-set ("1:5,7,9:3939") into
   individual ascending values, appended to *arr. */
static int imap_expand_seqset(const char *s, unsigned long **arr, long *count, long *cap)
{
    const char *p = s;

    while (*p != '\0') {
        char *end;
        unsigned long a, b, v;

        a = strtoul(p, &end, 10);
        if (end == p) return -1;
        p = end;
        if (*p == ':') {
            p++;
            b = strtoul(p, &end, 10);
            if (end == p) return -1;
            p = end;
        } else {
            b = a;
        }
        /* endpoint-tested loops, not v <= bound: a range ending at
           ULONG_MAX would otherwise wrap v and never terminate */
        if (a > b) { v = a; a = b; b = v; }
        for (v = a; ; v++) {
            if (imap_uidarr_append(arr, count, cap, v) < 0) return -1;
            if (v == b) break;
        }
        if (*p == ',') p++;
        else break;
    }
    return 0;
}

static int imap_ulong_cmp(const void *a, const void *b)
{
    unsigned long ua = *(const unsigned long *) a;
    unsigned long ub = *(const unsigned long *) b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

struct imap_search_rock {
    unsigned long **arr;
    long *count;
    long cap;
    int parse_error;
};

static void imap_cb_search(struct imapconn *conn, const char *line, void *rockp)
{
    struct imap_search_rock *r = (struct imap_search_rock *) rockp;
    const char *p;

    (void) conn;

    if (strncmp(line, "* SEARCH", 8) == 0) {
        p = line + 8;
        for (;;) {
            char *end;
            unsigned long v;
            while (*p == ' ') p++;
            if (*p == '\0') break;
            v = strtoul(p, &end, 10);
            if (end == p) { r->parse_error = 1; break; }
            if (imap_uidarr_append(r->arr, r->count, &r->cap, v) < 0) { r->parse_error = 1; break; }
            p = end;
        }
        return;
    }

    if (strncmp(line, "* ESEARCH", 9) == 0) {
        /* "* ESEARCH (TAG "aN") UID ALL <seqset>" -- with RETURN
           (ALL) there is exactly one return-data item, ALL, so we
           just locate " ALL " and take the rest of the line as the
           sequence-set. */
        const char *all = strstr(line, " ALL ");
        if (all != NULL) {
            if (imap_expand_seqset(all + 5, r->arr, r->count, &r->cap) < 0) r->parse_error = 1;
        }
        return;
    }
}

int imap_UidSearch(struct imapconn *conn, const char *criteria, unsigned long **uidsp, long *countp)
{
    char tag[IMAP_TAG_SIZE];
    struct imap_search_rock rock;
    int rc;

    if (conn == NULL || criteria == NULL || uidsp == NULL || countp == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    *uidsp = NULL;
    *countp = 0;
    rock.arr = uidsp;
    rock.count = countp;
    rock.cap = 0;
    rock.parse_error = 0;

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd_var(conn, tag,
                         imap_Capable(conn, "ESEARCH") ? "UID SEARCH RETURN (ALL) " : "UID SEARCH ",
                         criteria, "") < 0) return IMAP_DEAD;

    rc = imap_await_tagged(conn, tag, imap_cb_search, &rock, NULL, 0);
    if (rc != IMAP_OK) {
        if (*uidsp != NULL) { free(*uidsp); *uidsp = NULL; }
        *countp = 0;
        return rc;
    }
    if (rock.parse_error) {
        if (*uidsp != NULL) { free(*uidsp); *uidsp = NULL; }
        *countp = 0;
        imap_seterr(conn, "could not parse SEARCH/ESEARCH response");
        return IMAP_BAD;
    }

    if (*countp > 1) qsort(*uidsp, (size_t) *countp, sizeof(unsigned long), imap_ulong_cmp);
    return IMAP_OK;
}

/* ---- ENVELOPE parsing ---- */

/* Consumes one env-address-list ("(" addr... ")" or NIL) from the
   token stream and formats it into a single malloc'd "Name
   <mailbox@host>, ..." string (NULL if NIL/empty). Each address is
   the standard RFC 3501 4-tuple (name, adl, mailbox, host); adl is
   discarded (unused by any modern mail system). */
static char *imap_parse_addrlist(struct imapconn *conn, struct imap_linebuf *lb)
{
    struct imap_token tok;
    char *out;
    size_t outcap, outlen;

    if (imap_next_token(conn, lb, &tok) < 0) return NULL;
    if (tok.type == IMAP_TOK_NIL || tok.type == IMAP_TOK_EOF) { imap_token_free(&tok); return NULL; }
    if (tok.type != IMAP_TOK_LPAREN) { imap_token_free(&tok); return NULL; }
    imap_token_free(&tok);

    outcap = 256;
    out = (char *) malloc(outcap);
    if (out == NULL) return NULL;
    out[0] = '\0';
    outlen = 0;

    for (;;) {
        char *name, *mailbox, *host;
        size_t need;

        if (imap_next_token(conn, lb, &tok) < 0) break;
        if (tok.type == IMAP_TOK_RPAREN || tok.type == IMAP_TOK_EOF) { imap_token_free(&tok); break; }
        if (tok.type != IMAP_TOK_LPAREN) { imap_token_free(&tok); break; }
        imap_token_free(&tok);

        name = NULL; mailbox = NULL; host = NULL;
        if (imap_next_token(conn, lb, &tok) == 0) { name = imap_tok_to_str(&tok); imap_token_free(&tok); }
        if (imap_next_token(conn, lb, &tok) == 0) { imap_token_free(&tok); }		/* adl, discarded */
        if (imap_next_token(conn, lb, &tok) == 0) { mailbox = imap_tok_to_str(&tok); imap_token_free(&tok); }
        if (imap_next_token(conn, lb, &tok) == 0) { host = imap_tok_to_str(&tok); imap_token_free(&tok); }
        if (imap_next_token(conn, lb, &tok) == 0) { imap_token_free(&tok); }		/* closing ")" */

        need = outlen + (name != NULL ? strlen(name) + 1 : 0) +
               (mailbox != NULL ? strlen(mailbox) : 0) + (host != NULL ? strlen(host) : 0) + 8;
        if (need > outcap) {
            char *grown;
            while (need > outcap) outcap *= 2;
            grown = (char *) realloc(out, outcap);
            if (grown == NULL) {
                free(out);
                if (name != NULL) free(name);
                if (mailbox != NULL) free(mailbox);
                if (host != NULL) free(host);
                return NULL;
            }
            out = grown;
        }
        if (outlen > 0) { strcpy(out + outlen, ", "); outlen += 2; }
        if (name != NULL) { strcpy(out + outlen, name); outlen += strlen(name); out[outlen++] = ' '; }
        out[outlen++] = '<';
        if (mailbox != NULL) { strcpy(out + outlen, mailbox); outlen += strlen(mailbox); }
        out[outlen++] = '@';
        if (host != NULL) { strcpy(out + outlen, host); outlen += strlen(host); }
        out[outlen++] = '>';
        out[outlen] = '\0';

        if (name != NULL) free(name);
        if (mailbox != NULL) free(mailbox);
        if (host != NULL) free(host);
    }

    if (outlen == 0) { free(out); return NULL; }
    return out;
}

/* Consumes one full ENVELOPE list; lb->pos must be positioned right
   after ENVELOPE's own opening "(" (the caller consumes that token
   itself, matching how it recognizes the ENVELOPE keyword). */
static int imap_parse_envelope(struct imapconn *conn, struct imap_linebuf *lb, struct imap_envelope *env)
{
    struct imap_token tok;

    memset(env, 0, sizeof(*env));

    if (imap_next_token(conn, lb, &tok) < 0) return -1;
    env->date = imap_tok_to_str(&tok); imap_token_free(&tok);

    if (imap_next_token(conn, lb, &tok) < 0) return -1;
    env->subject = imap_tok_to_str(&tok); imap_token_free(&tok);

    env->from    = imap_parse_addrlist(conn, lb);
    env->sender  = imap_parse_addrlist(conn, lb);
    env->replyto = imap_parse_addrlist(conn, lb);
    env->to      = imap_parse_addrlist(conn, lb);
    env->cc      = imap_parse_addrlist(conn, lb);
    env->bcc     = imap_parse_addrlist(conn, lb);

    if (imap_next_token(conn, lb, &tok) < 0) return -1;
    env->inreplyto = imap_tok_to_str(&tok); imap_token_free(&tok);

    if (imap_next_token(conn, lb, &tok) < 0) return -1;
    env->messageid = imap_tok_to_str(&tok); imap_token_free(&tok);

    if (imap_next_token(conn, lb, &tok) < 0) return -1;	/* closing ")" of ENVELOPE */
    imap_token_free(&tok);

    return 0;
}

void imap_FreeEnvelope(struct imap_envelope *e)
{
    if (e == NULL) return;
    if (e->date != NULL) free(e->date);
    if (e->subject != NULL) free(e->subject);
    if (e->from != NULL) free(e->from);
    if (e->sender != NULL) free(e->sender);
    if (e->replyto != NULL) free(e->replyto);
    if (e->to != NULL) free(e->to);
    if (e->cc != NULL) free(e->cc);
    if (e->bcc != NULL) free(e->bcc);
    if (e->inreplyto != NULL) free(e->inreplyto);
    if (e->messageid != NULL) free(e->messageid);
    memset(e, 0, sizeof(*e));
}

/* ---- UID FETCH (FLAGS INTERNALDATE ENVELOPE) ---- */

int imap_UidFetchMeta(struct imapconn *conn, const char *uidset,
    int (*cb)(unsigned long, const char *, const char *, const struct imap_envelope *, void *),
    void *rock)
{
    char tag[IMAP_TAG_SIZE];
    char taglabel[IMAP_TAG_SIZE + 2];
    size_t taglen;
    struct imap_linebuf lb;
    int rc;

    if (conn == NULL || uidset == NULL || cb == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd_var(conn, tag, "UID FETCH ", uidset,
                         " (FLAGS INTERNALDATE ENVELOPE)") < 0) return IMAP_DEAD;

    snprintf(taglabel, sizeof(taglabel), "%s ", tag);
    taglen = strlen(taglabel);
    memset(&lb, 0, sizeof(lb));

    for (;;) {
        char *fetchpos;

        if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;

        if (lb.len >= taglen && strncmp(lb.data, taglabel, taglen) == 0) {
            if (strncmp(lb.data + taglen, "OK", 2) == 0) rc = IMAP_OK;
            else if (strncmp(lb.data + taglen, "NO", 2) == 0) rc = IMAP_NO;
            else rc = IMAP_BAD;
            if (rc != IMAP_OK) imap_seterr(conn, "%s", lb.data);
            imap_linebuf_free(&lb);
            return rc;
        }

        imap_check_unsolicited(conn, lb.data);

        fetchpos = strstr(lb.data, "FETCH");
        if (fetchpos != NULL) {
            struct imap_token tok;
            unsigned long uid = 0;
            int haveuid = 0;
            char flags[512];
            char internaldate[128];
            struct imap_envelope env;

            flags[0] = '\0';
            internaldate[0] = '\0';
            memset(&env, 0, sizeof(env));
            lb.pos = (size_t) (fetchpos - lb.data) + 5;

            for (;;) {
                if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; }
                if (tok.type == IMAP_TOK_EOF) { imap_token_free(&tok); break; }
                if (tok.type == IMAP_TOK_LPAREN || tok.type == IMAP_TOK_RPAREN) { imap_token_free(&tok); continue; }
                if (tok.type == IMAP_TOK_ATOM && tok.value != NULL && strcasecmp(tok.value, "UID") == 0) {
                    imap_token_free(&tok);
                    if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; }
                    if (tok.value != NULL) uid = strtoul(tok.value, NULL, 10);
                    haveuid = 1;
                    imap_token_free(&tok);
                    continue;
                }
                if (tok.type == IMAP_TOK_ATOM && tok.value != NULL && strcasecmp(tok.value, "FLAGS") == 0) {
                    imap_token_free(&tok);
                    if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; } /* "(" */
                    imap_token_free(&tok);
                    for (;;) {
                        if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; }
                        if (tok.type == IMAP_TOK_RPAREN || tok.type == IMAP_TOK_EOF) { imap_token_free(&tok); break; }
                        if (tok.value != NULL) {
                            if (flags[0] != '\0') strncat(flags, " ", sizeof(flags) - strlen(flags) - 1);
                            strncat(flags, tok.value, sizeof(flags) - strlen(flags) - 1);
                        }
                        imap_token_free(&tok);
                    }
                    continue;
                }
                if (tok.type == IMAP_TOK_ATOM && tok.value != NULL && strcasecmp(tok.value, "INTERNALDATE") == 0) {
                    imap_token_free(&tok);
                    if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; }
                    if (tok.value != NULL) {
                        strncpy(internaldate, tok.value, sizeof(internaldate) - 1);
                        internaldate[sizeof(internaldate) - 1] = '\0';
                    }
                    imap_token_free(&tok);
                    continue;
                }
                if (tok.type == IMAP_TOK_ATOM && tok.value != NULL && strcasecmp(tok.value, "ENVELOPE") == 0) {
                    imap_token_free(&tok);
                    if (imap_next_token(conn, &lb, &tok) < 0) { imap_FreeEnvelope(&env); return IMAP_DEAD; } /* "(" */
                    imap_token_free(&tok);
                    (void) imap_parse_envelope(conn, &lb, &env);
                    continue;
                }
                imap_token_free(&tok);
            }

            if (haveuid) (void) cb(uid, flags, internaldate, &env, rock);
            imap_FreeEnvelope(&env);
        }
    }
}

/* ---- UID FETCH BODY.PEEK[] (streamed) ---- */

int imap_UidFetchBody(struct imapconn *conn, unsigned long uid, FILE *out, long *sizep)
{
    char tag[IMAP_TAG_SIZE];
    char cmdtext[128];
    char taglabel[IMAP_TAG_SIZE + 2];
    size_t taglen;
    struct imap_linebuf lb;
    int rc;
    long written;

    if (conn == NULL || out == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }
    if (sizep != NULL) *sizep = 0;

    snprintf(cmdtext, sizeof(cmdtext), "UID FETCH %lu (BODY.PEEK[])", uid);
    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, cmdtext, NULL) < 0) return IMAP_DEAD;

    snprintf(taglabel, sizeof(taglabel), "%s ", tag);
    taglen = strlen(taglabel);
    memset(&lb, 0, sizeof(lb));
    written = 0;

    if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;

    for (;;) {
        if (lb.len >= taglen && strncmp(lb.data, taglabel, taglen) == 0) {
            if (strncmp(lb.data + taglen, "OK", 2) == 0) rc = IMAP_OK;
            else if (strncmp(lb.data + taglen, "NO", 2) == 0) rc = IMAP_NO;
            else rc = IMAP_BAD;
            if (rc != IMAP_OK) imap_seterr(conn, "%s", lb.data);
            imap_linebuf_free(&lb);
            if (rc == IMAP_OK && sizep != NULL) *sizep = written;
            return rc;
        }

        imap_check_unsolicited(conn, lb.data);

        if (lb.pending_literal > 0 && strstr(lb.data, "FETCH") != NULL) {
            unsigned long remaining = lb.pending_literal;
            char chunk[IMAP_BODY_CHUNK];

            lb.pending_literal = 0;	/* consumed here directly, not via imap_next_token */
            while (remaining > 0) {
                unsigned long want = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
                if (tlscon_ReadBytes(conn->tls, chunk, (int) want) < 0) {
                    conn->alive = 0;
                    imap_seterr(conn, "read failed streaming body literal; connection is now dead");
                    return IMAP_DEAD;
                }
                if (fwrite(chunk, 1, (size_t) want, out) != (size_t) want) {
                    imap_seterr(conn, "fwrite failed streaming body literal");
                    remaining -= want;
                    while (remaining > 0) {
                        unsigned long w2 = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
                        if (tlscon_ReadBytes(conn->tls, chunk, (int) w2) < 0) { conn->alive = 0; break; }
                        remaining -= w2;
                    }
                    return IMAP_BAD;
                }
                written += (long) want;
                remaining -= want;
            }
            if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;
            continue;	/* re-check this freshly-read line against the tag, etc. */
        }

        if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;
    }
}

/* ---- write entry points (writeback milestone; additive only) ---- */

int imap_Create(struct imapconn *conn, const char *mailbox)
{
    char tag[IMAP_TAG_SIZE];
    char qmailbox[300];
    char cmdtext[350];

    if (conn == NULL || mailbox == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    imap_quote(qmailbox, sizeof(qmailbox), mailbox);
    snprintf(cmdtext, sizeof(cmdtext), "CREATE %s", qmailbox);

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd(conn, tag, cmdtext, NULL) < 0) return IMAP_DEAD;

    return imap_await_tagged(conn, tag, NULL, NULL, NULL, 0);
}

int imap_UidStoreFlags(struct imapconn *conn, const char *uidset, int add, const char *flagslist)
{
    char tag[IMAP_TAG_SIZE];
    char after[160];

    if (conn == NULL || uidset == NULL || flagslist == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    snprintf(after, sizeof(after), " %sFLAGS.SILENT (%s)", add ? "+" : "-", flagslist);

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd_var(conn, tag, "UID STORE ", uidset, after) < 0) return IMAP_DEAD;

    return imap_await_tagged(conn, tag, NULL, NULL, NULL, 0);
}

int imap_UidExpunge(struct imapconn *conn, const char *uidset)
{
    char tag[IMAP_TAG_SIZE];

    if (conn == NULL || uidset == NULL) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }
    if (!imap_Capable(conn, "UIDPLUS")) {
        imap_seterr(conn, "UID EXPUNGE requires UIDPLUS, which this server has not advertised");
        return IMAP_BAD;
    }

    imap_nexttag(conn, tag, sizeof(tag));
    if (imap_sendcmd_var(conn, tag, "UID EXPUNGE ", uidset, "") < 0) return IMAP_DEAD;

    return imap_await_tagged(conn, tag, NULL, NULL, NULL, 0);
}

int imap_Append(struct imapconn *conn, const char *mailbox,
                 const char *flagslist, const char *internaldate,
                 FILE *bodyf, long bodysize,
                 unsigned long *out_uidvalidity, unsigned long *out_uid)
{
    char tag[IMAP_TAG_SIZE];
    char qmailbox[300];
    char qdate[128];
    char *cmdtext;
    size_t cmdcap;
    struct imap_linebuf lb;
    char tagline[1024];
    int rc;
    long remaining;
    char chunk[IMAP_BODY_CHUNK];

    if (out_uidvalidity != NULL) *out_uidvalidity = 0;
    if (out_uid != NULL) *out_uid = 0;

    if (conn == NULL || mailbox == NULL || bodyf == NULL || bodysize < 0) return IMAP_BAD;
    if (!conn->alive || conn->tls == NULL) { imap_seterr(conn, "connection is dead"); return IMAP_DEAD; }

    imap_quote(qmailbox, sizeof(qmailbox), mailbox);
    if (internaldate != NULL && internaldate[0] != '\0') {
        imap_quote(qdate, sizeof(qdate), internaldate);
    } else {
        qdate[0] = '\0';
    }

    cmdcap = strlen(qmailbox) + strlen(qdate)
             + (flagslist != NULL ? strlen(flagslist) : 0) + 64;
    cmdtext = (char *) malloc(cmdcap);
    if (cmdtext == NULL) {
        imap_seterr(conn, "out of memory building APPEND command");
        return IMAP_BAD;
    }
    /* Build the optional "(flags) " and "date " tokens only when
       present -- an empty INTERNALDATE or flag list must not leave a
       stray double space or an empty quoted-string-shaped gap in the
       command text (a real server, tried live, answers BAD "Missing
       required argument" to "APPEND mailbox  {n}" with the empty
       date token still there as two bare spaces). */
    {
        char middle[416];
        size_t mlen = 0;

        middle[0] = '\0';
        if (flagslist != NULL && flagslist[0] != '\0') {
            snprintf(middle, sizeof(middle), "(%s) ", flagslist);
            mlen = strlen(middle);
        }
        if (qdate[0] != '\0') {
            snprintf(middle + mlen, sizeof(middle) - mlen, "%s ", qdate);
        }
        snprintf(cmdtext, cmdcap, "APPEND %s %s{%ld}", qmailbox, middle, bodysize);
    }

    imap_nexttag(conn, tag, sizeof(tag));
    rc = imap_sendcmd(conn, tag, cmdtext, NULL);
    free(cmdtext);
    if (rc < 0) return IMAP_DEAD;

    /* Synchronizing literal: uniquely among this module's commands, the
       server must send a "+ " continuation line mid-command, before the
       literal's bytes may be written -- not a tagged response yet. A
       tagged BAD/NO here instead (a malformed mailbox name, quota,
       etc.) is handled like any other early failure. */
    memset(&lb, 0, sizeof(lb));
    if (imap_linebuf_read(conn, &lb) < 0) return IMAP_DEAD;
    if (lb.len == 0 || lb.data[0] != '+') {
        char taglabel[IMAP_TAG_SIZE + 2];
        size_t taglen;

        snprintf(taglabel, sizeof(taglabel), "%s ", tag);
        taglen = strlen(taglabel);
        if (lb.len >= taglen && strncmp(lb.data, taglabel, taglen) == 0) {
            if (strncmp(lb.data + taglen, "NO", 2) == 0) rc = IMAP_NO;
            else rc = IMAP_BAD;
            imap_seterr(conn, "%s", lb.data);
        } else {
            imap_seterr(conn, "expected \"+\" continuation for APPEND literal, got: %s", lb.data);
            rc = IMAP_BAD;
        }
        imap_linebuf_free(&lb);
        return rc;
    }
    imap_linebuf_free(&lb);

    remaining = bodysize;
    while (remaining > 0) {
        long want = remaining > (long) sizeof(chunk) ? (long) sizeof(chunk) : remaining;
        size_t got = fread(chunk, 1, (size_t) want, bodyf);

        if (got == 0) {
            imap_seterr(conn, "short read from local body file while streaming APPEND literal");
            return IMAP_BAD;
        }
        if (tlscon_Write(conn->tls, chunk, (int) got) < 0) {
            conn->alive = 0;
            imap_seterr(conn, "write failed streaming APPEND literal; connection is now dead");
            return IMAP_DEAD;
        }
        remaining -= (long) got;
    }
    if (tlscon_Write(conn->tls, "\r\n", 2) < 0) {
        conn->alive = 0;
        imap_seterr(conn, "write failed terminating APPEND; connection is now dead");
        return IMAP_DEAD;
    }

    tagline[0] = '\0';
    rc = imap_await_tagged(conn, tag, NULL, NULL, tagline, sizeof(tagline));
    if (rc == IMAP_OK) {
        char *p = strstr(tagline, "[APPENDUID ");
        if (p != NULL) {
            char *start = p + 11;
            char *end;
            unsigned long uv = strtoul(start, &end, 10);

            if (end != start && *end == ' ') {
                unsigned long ud = strtoul(end + 1, &end, 10);
                if (out_uidvalidity != NULL) *out_uidvalidity = uv;
                if (out_uid != NULL) *out_uid = ud;
            }
        }
    }
    return rc;
}

/* ---- offline unit test entry point (canned response, no network) ---- */

/* See imap_prot.h for the full contract. Drives the exact same
   imap_linebuf_read/imap_next_token/imap_parse_envelope code the live
   imap_UidFetchMeta path uses, via a throwaway conn whose
   test_canned/test_cannedlen/test_cannedpos redirect all reads to
   `canned` instead of a tlscon. `canned` must begin with ENVELOPE's
   own opening "(" -- this function consumes that token itself, then
   hands off to imap_parse_envelope, matching exactly how
   imap_UidFetchMeta consumes it before calling imap_parse_envelope. */
int imap_TestParseEnvelope(const char *canned, size_t cannedlen, struct imap_envelope *out)
{
    struct imapconn fakeconn;
    struct imap_linebuf lb;
    struct imap_token tok;
    int rc;

    if (canned == NULL || out == NULL) return -1;

    memset(&fakeconn, 0, sizeof(fakeconn));
    fakeconn.alive = 1;
    fakeconn.test_canned = canned;
    fakeconn.test_cannedlen = cannedlen;
    fakeconn.test_cannedpos = 0;

    memset(&lb, 0, sizeof(lb));
    if (imap_linebuf_read(&fakeconn, &lb) < 0) return -1;

    if (imap_next_token(&fakeconn, &lb, &tok) < 0) { imap_linebuf_free(&lb); return -1; }
    if (tok.type != IMAP_TOK_LPAREN) { imap_token_free(&tok); imap_linebuf_free(&lb); return -1; }
    imap_token_free(&tok);

    rc = imap_parse_envelope(&fakeconn, &lb, out);
    imap_linebuf_free(&lb);
    return rc;
}
