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
	mimepart.c -- see mimepart.h for the API and its rationale.
	ANSI C (C89 prototypes) throughout; no scanf/sscanf/fscanf
	anywhere -- numbers are parsed with strtol, keywords with
	strncasecmp.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>	/* strncasecmp */
#include <ctype.h>

#include <mimepart.h>

/* ---- small local helpers (no libc extensions assumed) ---- */

static char *mp_strdup(const char *s)
{
    char *r;
    if (!s) return NULL;
    r = (char *) malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

static int hdrnamecmp(const unsigned char *s, long slen, const char *name)
{
    long nlen = (long) strlen(name);
    return (slen == nlen) && (strncasecmp((const char *) s, name, (size_t) nlen) == 0);
}

/* trims leading/trailing whitespace from data[0..len), returns a
   malloc'd NUL-terminated copy (lowercased if lower!=0) */
static char *trimdup(const unsigned char *data, long len, int lower)
{
    char *out;
    long i;
    while (len > 0 && isspace(data[0])) { ++data; --len; }
    while (len > 0 && isspace(data[len-1])) --len;
    out = (char *) malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, data, len);
    out[len] = '\0';
    if (lower) for (i = 0; i < len; ++i) out[i] = tolower((unsigned char) out[i]);
    return out;
}

static void mimeparam_free(struct mimeparam *p)
{
    while (p) {
        struct mimeparam *n = p->next;
        free(p->name);
        free(p->value);
        free(p);
        p = n;
    }
}

const char *mimepart_GetParam(const struct mimepart *p, const char *name)
{
    struct mimeparam *q;
    for (q = p->params; q; q = q->next) {
        if (strcasecmp(q->name, name) == 0) return q->value;
    }
    return NULL;
}

const char *mimepart_GetDispParam(const struct mimepart *p, const char *name)
{
    struct mimeparam *q;
    for (q = p->dispparams; q; q = q->next) {
        if (strcasecmp(q->name, name) == 0) return q->value;
    }
    return NULL;
}

/* ---- "type/subtype; name=value; ..." / "attachment; name=value"
   header-value parser, shared by Content-Type and Content-Disposition ---- */

/* finds the next top-level ';' in s (i.e. not inside a "quoted string"),
   honoring backslash escapes inside quotes; returns NULL if none */
static char *paramend(char *s)
{
    int inquotes = 0;
    while (*s) {
        if (inquotes) {
            if (*s == '"') inquotes = 0;
            else if (*s == '\\' && s[1]) ++s;
        } else if (*s == ';') {
            return s;
        } else if (*s == '"') {
            inquotes = 1;
        }
        ++s;
    }
    return NULL;
}

/* unquotes/unescapes a "..." value in place-ish (returns a fresh
   malloc'd copy); if s doesn't start with '"', returns a trimmed
   copy of s unchanged */
static char *unquote(const char *s)
{
    char *out, *t;
    long len = (long) strlen(s);

    while (len > 0 && isspace((unsigned char) s[len-1])) --len;
    while (*s && isspace((unsigned char) *s)) { ++s; --len; }
    if (*s != '"') {
        out = (char *) malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, s, len);
        out[len] = '\0';
        return out;
    }
    out = (char *) malloc(len + 1);
    if (!out) return NULL;
    t = out;
    ++s;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1]) ++s;
        *t++ = *s++;
    }
    *t = '\0';
    return out;
}

/* parses hdrval ("type/subtype; p=v; ...") into *out_primary
   (malloc'd, trimmed, lowercased) and *out_params (malloc'd list,
   names lowercased, values unquoted/unescaped but case-preserved).
   Safe to call with out_primary==NULL (Content-Disposition params
   without needing the disposition token again). */
static void parse_typed_header(const char *hdrval, char **out_primary,
                                struct mimeparam **out_params)
{
    char *copy, *s, *semi, *eq, *t;
    struct mimeparam *head = NULL, *tail = NULL;

    if (out_primary) *out_primary = NULL;
    *out_params = NULL;
    if (!hdrval || !hdrval[0]) return;

    copy = mp_strdup(hdrval);
    if (!copy) return;

    semi = paramend(copy);
    if (semi) *semi = '\0';
    if (out_primary) {
        *out_primary = trimdup((unsigned char *) copy, (long) strlen(copy), 1);
    }
    s = semi ? semi + 1 : NULL;

    while (s) {
        char *next = paramend(s);
        if (next) *next = '\0';
        eq = strchr(s, '=');
        if (eq) {
            struct mimeparam *pm;
            *eq = '\0';
            pm = (struct mimeparam *) malloc(sizeof(struct mimeparam));
            if (!pm) break;
            pm->name = trimdup((unsigned char *) s, (long) strlen(s), 1);
            t = eq + 1;
            while (*t && isspace((unsigned char) *t)) ++t;
            pm->value = unquote(t);
            pm->next = NULL;
            if (!pm->name || !pm->value) {
                free(pm->name); free(pm->value); free(pm);
            } else if (tail) {
                tail->next = pm; tail = pm;
            } else {
                head = tail = pm;
            }
        }
        s = next ? next + 1 : NULL;
    }
    free(copy);
    *out_params = head;
}

/* ---- Content-Transfer-Encoding ---- */

static int parse_encoding(const char *cte)
{
    while (cte && *cte && isspace((unsigned char) *cte)) ++cte;
    if (!cte) return MIMEPART_ENC_NONE;
    if (strncasecmp(cte, "quoted-printable", 16) == 0) return MIMEPART_ENC_QP;
    if (strncasecmp(cte, "base64", 6) == 0) return MIMEPART_ENC_BASE64;
    return MIMEPART_ENC_NONE;
}

/* ---- QP / base64 decode ---- */

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

long mimepart_DecodeQP(const unsigned char *in, long inlen, unsigned char **outp)
{
    unsigned char *out;
    long i = 0, o = 0;

    out = (unsigned char *) malloc(inlen > 0 ? inlen : 1);
    if (!out) { *outp = NULL; return -1; }
    while (i < inlen) {
        if (in[i] == '=') {
            if (i + 1 < inlen && in[i+1] == '\n') {
                i += 2; /* soft break, LF-only */
            } else if (i + 2 < inlen && in[i+1] == '\r' && in[i+2] == '\n') {
                i += 3; /* soft break, CRLF */
            } else if (i + 2 < inlen) {
                int h1 = hexval(in[i+1]), h2 = hexval(in[i+2]);
                if (h1 >= 0 && h2 >= 0) {
                    out[o++] = (unsigned char) ((h1 << 4) | h2);
                } else {
                    out[o++] = 'X'; /* malformed escape: lenient passthrough */
                }
                i += 3;
            } else {
                /* trailing bare '=' at EOF */
                ++i;
            }
        } else {
            out[o++] = in[i++];
        }
    }
    *outp = out;
    return o;
}

static int b64val(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

long mimepart_DecodeBase64(const unsigned char *in, long inlen, unsigned char **outp)
{
    unsigned char *out;
    long i, o = 0;
    int group[4], gct = 0;

    out = (unsigned char *) malloc(inlen > 0 ? inlen : 1);
    if (!out) { *outp = NULL; return -1; }
    for (i = 0; i < inlen; ++i) {
        int c = in[i];
        int v;
        if (c == '=') continue; /* padding handled by group flush below */
        v = b64val(c);
        if (v < 0) continue; /* whitespace or any other stray byte: skip */
        group[gct++] = v;
        if (gct == 4) {
            out[o++] = (unsigned char) ((group[0] << 2) | (group[1] >> 4));
            out[o++] = (unsigned char) (((group[1] & 0xF) << 4) | (group[2] >> 2));
            out[o++] = (unsigned char) (((group[2] & 0x3) << 6) | group[3]);
            gct = 0;
        }
    }
    if (gct == 2) {
        out[o++] = (unsigned char) ((group[0] << 2) | (group[1] >> 4));
    } else if (gct == 3) {
        out[o++] = (unsigned char) ((group[0] << 2) | (group[1] >> 4));
        out[o++] = (unsigned char) (((group[1] & 0xF) << 4) | (group[2] >> 2));
    }
    *outp = out;
    return o;
}

/* ---- line scanning over an in-memory buffer ---- */

/* Locates the line starting at pos: *linestart==pos, *lineend is the
   offset of the first CR/LF (i.e. content length is *lineend-pos),
   *nextpos is where the following line starts (past the terminator,
   or == len at EOF with no terminator). Returns 0 if pos >= len. */
static int next_line(const unsigned char *data, long len, long pos,
                      long *lineend, long *nextpos)
{
    long e;
    if (pos >= len) return 0;
    e = pos;
    while (e < len && data[e] != '\n') ++e;
    if (e < len) {
        long contentend = e;
        if (contentend > pos && data[contentend-1] == '\r') --contentend;
        *lineend = contentend;
        *nextpos = e + 1;
    } else {
        *lineend = e;
        *nextpos = e;
    }
    return 1;
}

#define HDRBUFSZ 2000

/* Scans an RFC822-style header block starting at data[0], stopping at
   the first blank line or EOF. Only Content-Type, Content-Transfer-
   Encoding and Content-Disposition are captured (into caller-supplied
   HDRBUFSZ buffers, empty string if absent); *bodystart is set to the
   offset of the first byte after the header block. */
static void split_headers_body(const unsigned char *data, long len, long *bodystart,
                                char *ctypebuf, char *ctebuf, char *cdispbuf)
{
    long pos = 0, lineend, nextpos;
    int which = 0; /* 0=none, 1=ctype, 2=cte, 3=cdisp */

    ctypebuf[0] = ctebuf[0] = cdispbuf[0] = '\0';
    while (next_line(data, len, pos, &lineend, &nextpos)) {
        long linestart = pos;
        if (lineend == linestart) { pos = nextpos; break; } /* blank line */
        if (data[linestart] == ' ' || data[linestart] == '\t') {
            char *dst = (which == 1) ? ctypebuf : (which == 2) ? ctebuf :
                        (which == 3) ? cdispbuf : NULL;
            if (dst) {
                long used = (long) strlen(dst);
                long avail = HDRBUFSZ - 1 - used;
                long ws = linestart;
                while (ws < lineend && (data[ws] == ' ' || data[ws] == '\t')) ++ws;
                if (avail > 1) { dst[used] = ' '; ++used; --avail; }
                if (lineend - ws < avail) avail = lineend - ws;
                if (avail > 0) { memcpy(dst + used, data + ws, avail); dst[used + avail] = '\0'; }
            }
        } else {
            const unsigned char *colon = (const unsigned char *)
                memchr(data + linestart, ':', lineend - linestart);
            which = 0;
            if (colon) {
                long namelen = colon - (data + linestart);
                long valstart = (colon - data) + 1;
                long vallen, take;
                char *dst = NULL;
                while (valstart < lineend && (data[valstart] == ' ' || data[valstart] == '\t')) ++valstart;
                vallen = lineend - valstart;
                if (hdrnamecmp(data + linestart, namelen, "content-type")) { which = 1; dst = ctypebuf; }
                else if (hdrnamecmp(data + linestart, namelen, "content-transfer-encoding")) { which = 2; dst = ctebuf; }
                else if (hdrnamecmp(data + linestart, namelen, "content-disposition")) { which = 3; dst = cdispbuf; }
                if (dst) {
                    take = vallen;
                    if (take > HDRBUFSZ - 1) take = HDRBUFSZ - 1;
                    if (take > 0) memcpy(dst, data + valstart, take);
                    dst[take] = '\0';
                }
            }
        }
        pos = nextpos;
    }
    *bodystart = pos;
}

/* ---- multipart splitting ---- */

static void append_child(struct mimepart **head, struct mimepart **tail, struct mimepart *c)
{
    c->next = NULL;
    if (*tail) { (*tail)->next = c; *tail = c; }
    else { *head = *tail = c; }
}

static struct mimepart *parse_one_part(const unsigned char *data, long len);

/* Splits data[0..len) on "--boundary" lines per RFC 2046 and parses
   each part found. Returns NULL (no children) if no valid boundary
   line is ever found at all -- the caller degrades to treating the
   whole buffer as an opaque leaf in that case. */
static struct mimepart *split_multipart(const unsigned char *data, long len, const char *boundary)
{
    long pos = 0, lineend, nextpos;
    long blen = (long) strlen(boundary);
    long partstart = -1;
    int foundfirst = 0, done = 0;
    struct mimepart *head = NULL, *tail = NULL;

    if (blen == 0) return NULL;

    while (!done && next_line(data, len, pos, &lineend, &nextpos)) {
        long linestart = pos;
        long linelen = lineend - linestart;
        int is_delim = 0, is_final = 0;

        if (linelen >= 2 + blen
            && data[linestart] == '-' && data[linestart+1] == '-'
            && memcmp(data + linestart + 2, boundary, blen) == 0) {
            long p = linestart + 2 + blen;
            is_delim = 1;
            if (p + 1 < lineend && data[p] == '-' && data[p+1] == '-') { is_final = 1; p += 2; }
            while (p < lineend) {
                if (data[p] != ' ' && data[p] != '\t') { is_delim = 0; break; }
                ++p;
            }
        }

        if (is_delim) {
            if (foundfirst && partstart >= 0) {
                long cend = linestart;
                if (cend > partstart && data[cend-1] == '\n') --cend;
                if (cend > partstart && data[cend-1] == '\r') --cend;
                if (cend < partstart) cend = partstart;
                {
                    struct mimepart *c = parse_one_part(data + partstart, cend - partstart);
                    if (c) append_child(&head, &tail, c);
                }
            }
            foundfirst = 1;
            if (is_final) {
                done = 1;
            } else {
                partstart = nextpos;
            }
        }
        pos = nextpos;
    }
    if (foundfirst && !done && partstart >= 0 && partstart <= len) {
        struct mimepart *c = parse_one_part(data + partstart, len - partstart);
        if (c) append_child(&head, &tail, c);
    }
    return foundfirst ? (head ? head : NULL) : NULL;
}

/* parses one multipart child: its own header block, then recurses */
static struct mimepart *parse_one_part(const unsigned char *data, long len)
{
    long bodystart;
    char ctype[HDRBUFSZ], cte[HDRBUFSZ], cdisp[HDRBUFSZ];
    struct mimepart *part;

    if (len < 0) len = 0;
    split_headers_body(data, len, &bodystart, ctype, cte, cdisp);
    part = mimepart_Parse(data + bodystart, len - bodystart,
                           ctype[0] ? ctype : NULL, cte[0] ? cte : NULL);
    if (part && cdisp[0]) {
        parse_typed_header(cdisp, &part->disposition, &part->dispparams);
    }
    return part;
}

/* ---- public entry points ---- */

struct mimepart *mimepart_Parse(const unsigned char *data, long len,
                                 const char *content_type,
                                 const char *content_transfer_encoding)
{
    struct mimepart *part;

    part = (struct mimepart *) malloc(sizeof(struct mimepart));
    if (!part) return NULL;
    part->type = NULL;
    part->params = NULL;
    part->disposition = NULL;
    part->dispparams = NULL;
    part->encoding = MIMEPART_ENC_NONE;
    part->body = NULL;
    part->bodylen = 0;
    part->children = NULL;
    part->next = NULL;

    if (len < 0) len = 0;

    parse_typed_header(content_type, &part->type, &part->params);
    if (!part->type) part->type = mp_strdup("text/plain");

    if (strncasecmp(part->type, "multipart/", 10) == 0) {
        const char *boundary = mimepart_GetParam(part, "boundary");
        if (boundary && boundary[0]) {
            part->children = split_multipart(data, len, boundary);
        }
        if (!part->children) {
            /* No usable boundary, or no parts found inside it: degrade
               to an opaque leaf so the caller still gets *something*
               displayable/attachable rather than nothing. */
            part->encoding = MIMEPART_ENC_NONE;
            part->body = (unsigned char *) malloc(len > 0 ? len : 1);
            if (part->body) { memcpy(part->body, data, len); part->bodylen = len; }
        }
    } else {
        part->encoding = parse_encoding(content_transfer_encoding);
        if (part->encoding == MIMEPART_ENC_QP) {
            part->bodylen = mimepart_DecodeQP(data, len, &part->body);
        } else if (part->encoding == MIMEPART_ENC_BASE64) {
            part->bodylen = mimepart_DecodeBase64(data, len, &part->body);
        } else {
            part->body = (unsigned char *) malloc(len > 0 ? len : 1);
            if (part->body) { memcpy(part->body, data, len); part->bodylen = len; }
        }
    }
    return part;
}

static unsigned char *read_all(FILE *fp, long *lenp)
{
    unsigned char *buf = NULL;
    long alloced = 0, used = 0;
    size_t got;

    for (;;) {
        if (used + 65536 > alloced) {
            unsigned char *nb;
            alloced = alloced ? alloced * 2 : 65536;
            if (alloced < used + 65536) alloced = used + 65536;
            nb = (unsigned char *) realloc(buf, alloced);
            if (!nb) { free(buf); *lenp = 0; return NULL; }
            buf = nb;
        }
        got = fread(buf + used, 1, (size_t) (alloced - used), fp);
        if (got == 0) break;
        used += (long) got;
    }
    *lenp = used;
    return buf;
}

struct mimepart *mimepart_ParseFile(FILE *fp, const char *content_type,
                                     const char *content_transfer_encoding)
{
    long len;
    unsigned char *data = read_all(fp, &len);
    struct mimepart *part = mimepart_Parse(data, len, content_type, content_transfer_encoding);
    free(data);
    return part;
}

struct mimepart *mimepart_ParseMessageFile(FILE *fp)
{
    long len, bodystart;
    unsigned char *data = read_all(fp, &len);
    char ctype[HDRBUFSZ], cte[HDRBUFSZ], cdisp[HDRBUFSZ];
    struct mimepart *part;

    if (!data) return NULL;
    split_headers_body(data, len, &bodystart, ctype, cte, cdisp);
    part = mimepart_Parse(data + bodystart, len - bodystart,
                           ctype[0] ? ctype : NULL, cte[0] ? cte : NULL);
    if (part && cdisp[0]) {
        parse_typed_header(cdisp, &part->disposition, &part->dispparams);
    }
    free(data);
    return part;
}

void mimepart_Free(struct mimepart *p)
{
    while (p) {
        struct mimepart *n = p->next;
        free(p->type);
        mimeparam_free(p->params);
        free(p->disposition);
        mimeparam_free(p->dispparams);
        free(p->body);
        mimepart_Free(p->children);
        free(p);
        p = n;
    }
}

const struct mimepart *mimepart_SelectAlternative(const struct mimepart *alt)
{
    const struct mimepart *c, *html = NULL;

    for (c = alt->children; c; c = c->next) {
        if (strcasecmp(c->type, "text/plain") == 0) return c;
        if (!html && strcasecmp(c->type, "text/html") == 0) html = c;
    }
    if (html) return html;
    return alt->children;
}

/* ---- text/html -> plain-text shim ---- */

struct html_entity { const char *name; unsigned char latin1; };
static const struct html_entity html_entities[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
    { "apos", '\'' }, { "nbsp", 0xA0 },
    { NULL, 0 }
};

/* appends a single output byte, growing the buffer as needed */
static void emit(char **bufp, long *lenp, long *capp, int c)
{
    if (*lenp + 1 >= *capp) {
        long ncap = *capp ? *capp * 2 : 256;
        char *nb = (char *) realloc(*bufp, ncap);
        if (!nb) return;
        *bufp = nb; *capp = ncap;
    }
    (*bufp)[(*lenp)++] = (char) c;
}

/* name/namelen point into the caller's (not necessarily
   NUL-terminated) input buffer, so the numeric-entity path copies
   into a small NUL-terminated stack buffer before handing anything to
   strtol -- calling strtol directly on an unterminated slice would
   read past the entity for as long as digit characters happened to
   continue in memory. namelen is already capped (<32) by the caller. */
static void emit_entity(char **bufp, long *lenp, long *capp, const char *name, long namelen)
{
    int i;
    char nbuf[36];

    if (namelen >= (long) sizeof(nbuf)) namelen = sizeof(nbuf) - 1;
    memcpy(nbuf, name, namelen);
    nbuf[namelen] = '\0';

    if (namelen >= 2 && nbuf[0] == '#') {
        long code;
        char *start, *endp;
        if (namelen >= 3 && (nbuf[1] == 'x' || nbuf[1] == 'X')) {
            start = nbuf + 2;
            code = strtol(start, &endp, 16);
        } else {
            start = nbuf + 1;
            code = strtol(start, &endp, 10);
        }
        emit(bufp, lenp, capp, (endp != start && code > 0 && code <= 0xFF) ? (int) code : '?');
        return;
    }
    for (i = 0; html_entities[i].name; ++i) {
        if (strcasecmp(html_entities[i].name, nbuf) == 0) {
            emit(bufp, lenp, capp, html_entities[i].latin1);
            return;
        }
    }
    /* unknown entity: emit verbatim, "&name;" */
    emit(bufp, lenp, capp, '&');
    for (i = 0; i < namelen; ++i) emit(bufp, lenp, capp, nbuf[i]);
    emit(bufp, lenp, capp, ';');
}

static int tag_is(const char *html, long start, long len, const char *name)
{
    long i = start, nl = (long) strlen(name);
    if (i < len && html[i] == '/') ++i;
    if (len - i < nl) return 0;
    if (strncasecmp(html + i, name, (size_t) nl) != 0) return 0;
    i += nl;
    return (i >= len) || isspace((unsigned char) html[i]) || html[i] == '>';
}

char *mimepart_HtmlToText(const char *html, long len)
{
    char *out = NULL;
    long outlen = 0, outcap = 0;
    long i = 0;
    int nlrun = 0;

    while (i < len) {
        if (html[i] == '<') {
            long tagstart = i + 1;
            long j = tagstart;
            const char *which;
            while (j < len && html[j] != '>') ++j;
            which = tag_is(html, tagstart, len, "script") ? "script"
                  : tag_is(html, tagstart, len, "style") ? "style"
                  : tag_is(html, tagstart, len, "head") ? "head" : NULL;
            if (which) {
                /* skip through the matching close tag */
                long k = (j < len) ? j + 1 : len;
                for (;;) {
                    long lt = k;
                    while (lt < len && html[lt] != '<') ++lt;
                    if (lt >= len) { k = len; break; }
                    if (tag_is(html, lt + 1, len, which) && lt + 1 < len && html[lt+1] == '/') {
                        long ke = lt + 1;
                        while (ke < len && html[ke] != '>') ++ke;
                        k = (ke < len) ? ke + 1 : len;
                        break;
                    }
                    k = lt + 1;
                }
                i = k;
                continue;
            }
            i = (j < len) ? j + 1 : len;
            continue;
        }
        if (html[i] == '&') {
            long j = i + 1, start = j;
            while (j < len && j - start < 32 && html[j] != ';' && !isspace((unsigned char) html[j]) && html[j] != '&') ++j;
            if (j < len && html[j] == ';' && j > start) {
                emit_entity(&out, &outlen, &outcap, html + start, j - start);
                i = j + 1;
                continue;
            }
            emit(&out, &outlen, &outcap, '&');
            ++i;
            continue;
        }
        if (html[i] == '\r') {
            /* normalize CRLF/lone-CR to LF-only (matches the rest of
               this tree's text handling); dropped silently, does not
               disturb the newline-run count below */
            ++i;
            continue;
        }
        if (html[i] == '\n') {
            ++nlrun;
            if (nlrun <= 2) emit(&out, &outlen, &outcap, '\n');
            ++i;
            continue;
        }
        nlrun = 0;
        emit(&out, &outlen, &outcap, html[i]);
        ++i;
    }
    emit(&out, &outlen, &outcap, '\0');
    if (!out) out = mp_strdup("");
    return out;
}

/* ---- UTF-8 -> Latin-1 ---- */

unsigned char *mimepart_Utf8ToLatin1(const unsigned char *in, long inlen, long *outlenp)
{
    unsigned char *out = (unsigned char *) malloc(inlen + 1 > 0 ? inlen + 1 : 1);
    long i = 0, o = 0;

    if (!out) { if (outlenp) *outlenp = 0; return NULL; }
    while (i < inlen) {
        unsigned char c = in[i];
        if (c < 0x80) {
            out[o++] = c; ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < inlen && (in[i+1] & 0xC0) == 0x80) {
            long cp = ((c & 0x1F) << 6) | (in[i+1] & 0x3F);
            out[o++] = (cp <= 0xFF) ? (unsigned char) cp : '?';
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < inlen
                   && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80) {
            out[o++] = '?'; /* always > 0xFF */
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < inlen
                   && (in[i+1] & 0xC0) == 0x80 && (in[i+2] & 0xC0) == 0x80 && (in[i+3] & 0xC0) == 0x80) {
            out[o++] = '?'; /* always > 0xFF */
            i += 4;
        } else {
            out[o++] = '?';
            ++i;
        }
    }
    out[o] = '\0';
    if (outlenp) *outlenp = o;
    return out;
}
