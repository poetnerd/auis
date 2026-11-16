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
	httpimg.c -- see httpimg.h for the API, the resolver contract this
	implements (htmlatk_ImageResolver, htmlatk.h), and why this is not
	wired into any real rendering call site yet.

	Subprocess, not a linked HTTP/TLS library: this shells out to the
	system `curl` (argv-exec, no shell -- see below) rather than
	linking libcurl/OpenSSL directly. Two concrete reasons, not just
	convenience: (1) src/ams/msclients/imapsync's Imakefile already
	references ${SSLLIB} for exactly this kind of need and that make
	variable is not actually defined anywhere in
	src/config/site.def on this checkout, so that path is unproven on
	this build -- picking it here would mean debugging a crypto-
	library link before writing a single fetch; (2) the system curl on
	this Mac is already TLS-capable via SecureTransport with no extra
	dependency at all. A subprocess per image is measurably slower
	than a kept-open connection, but this module fetches at most a
	handful of images per message render, not a hot loop.

	Temp files via mkstemp(), not ams_CUI_Gen(Local)TmpFileName()
	(writeone.c's usual helper for this in this same directory):
	those take a live struct ams* (ams_GetAMS()), which would tie this
	module to an active AMS session and the ATK class system just to
	name two scratch files. mkstemp() keeps this headless-testable the
	same way htmlatk.c deliberately stayed X11-free (see
	htmlatktest.c's header comment).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <andrewos.h>
#include <class.h>

#include "httpimg.h"

#define TRUE 1
#define FALSE 0

/* Hard cap on a fetched image's body size, enforced twice: curl is
   told --max-filesize so it can abort mid-transfer once the server's
   own Content-Length (or the byte count actually received) exceeds
   this, and ReadFileCapped() below independently stat()s the file
   curl wrote before reading it, so a server that omits Content-Length
   and simply stops sending at the boundary does not get a pass
   either. 4MB is generous for real inline mail images (the design
   doc's own fixtures top out far smaller) while still bounding worst-
   case memory use per fetch. */
#define HTTPIMG_MAX_BYTES (4L * 1024 * 1024)

/* Fallback used when a caller passes perFetchMaxSeconds <= 0 to
   httpimg_InitBudget() (callers are expected to pass a real value --
   currently text822.c's "ams.imagefetchtimeout" preference -- this is
   just the safe floor if one doesn't). Whatever value is actually in
   effect (the field, not this constant) is still further clamped down
   to whatever remains of the caller's overall struct httpimg_budget
   deadline (see httpimg_ResolveImage below). --connect-timeout is
   kept well under it so a host that never completes a TCP handshake
   fails fast and leaves time for the caller's remaining budget rather
   than eating the whole per-fetch allowance on a connect that was
   never going to succeed. */
#define HTTPIMG_DEFAULT_MAX_TIME 5
#define HTTPIMG_CONNECT_TIMEOUT 3

#define HTTPIMG_HEADER_READ_CAP 16384

void httpimg_InitBudget(struct httpimg_budget *budget, int maxSeconds, int perFetchMaxSeconds)
{
    if (!budget) return;
    budget->deadline = time(NULL) + ((maxSeconds > 0) ? maxSeconds : 0);
    budget->perFetchMaxSeconds = (perFetchMaxSeconds > 0) ? perFetchMaxSeconds : HTTPIMG_DEFAULT_MAX_TIME;
    budget->cacheCount = 0;
}

void httpimg_FreeBudget(struct httpimg_budget *budget)
{
    int i;
    if (!budget) return;
    for (i = 0; i < budget->cacheCount; ++i) {
        free(budget->cache[i].src);
        if (budget->cache[i].bytes) free(budget->cache[i].bytes);
        if (budget->cache[i].mimetype) free(budget->cache[i].mimetype);
    }
    budget->cacheCount = 0;
}

/* Returns a malloc'd copy of a cache entry's bytes/mimetype into
   *bytesOut/*mimetypeOut (matching httpimg_ResolveImage()'s normal
   ownership contract -- the caller frees these independently of
   the cache's own copies) and TRUE, or FALSE with the out-params
   untouched if the entry recorded a failed fetch (still a cache hit
   -- a src that failed once fails the same way every time within one
   render pass, no reason to re-fetch it just to fail again). */
static boolean CacheHit(struct httpimg_cache_entry *entry,
    unsigned char **bytesOut, long *lenOut, char **mimetypeOut)
{
    if (!entry->ok) return FALSE;
    *bytesOut = (unsigned char *) malloc((size_t) entry->len);
    if (!*bytesOut) return FALSE;
    memcpy(*bytesOut, entry->bytes, (size_t) entry->len);
    *mimetypeOut = strdup(entry->mimetype);
    if (!*mimetypeOut) { free(*bytesOut); *bytesOut = NULL; return FALSE; }
    *lenOut = entry->len;
    return TRUE;
}

/* Records src's outcome into budget's cache (a copy, independent of
   what was just handed back to the caller) if there's room left --
   silently a no-op past HTTPIMG_CACHE_SIZE entries, see that
   constant's own comment on why that's fine (real repeats are a
   small, fixed set of chrome icons, not every unique image). A
   malloc failure while copying is likewise just a silent skip (this
   is a hit-rate optimization, not correctness-critical -- a src that
   fails to cache is simply fetched again next time it's seen, same
   as if caching didn't exist for it at all). */
static void CacheStore(struct httpimg_budget *budget, const char *src,
    boolean ok, const unsigned char *bytes, long len, const char *mimetype)
{
    struct httpimg_cache_entry *entry;
    if (budget->cacheCount >= HTTPIMG_CACHE_SIZE) return;
    entry = &budget->cache[budget->cacheCount];
    entry->src = strdup(src);
    if (!entry->src) return;
    entry->ok = ok;
    entry->bytes = NULL;
    entry->mimetype = NULL;
    entry->len = 0;
    if (ok) {
        entry->bytes = (unsigned char *) malloc((size_t) len);
        entry->mimetype = strdup(mimetype);
        if (!entry->bytes || !entry->mimetype) {
            free(entry->src);
            if (entry->bytes) free(entry->bytes);
            if (entry->mimetype) free(entry->mimetype);
            return;
        }
        memcpy(entry->bytes, bytes, (size_t) len);
        entry->len = len;
    }
    ++budget->cacheCount;
}

/* Mirrors htmlpart.c's href_scheme_ok() -- same leading-whitespace
   tolerance (the classic " javascript:..." evasion), same "no
   recognized scheme at all fails" default -- but with the allowlist
   narrowed to http/https only (no mailto: here; that scheme has
   nothing to fetch). attr_allowed() in htmlpart.c does not gate <img
   src> at all (only <a href> goes through href_scheme_ok there), so
   this check is this module's own responsibility, not a redundant
   double-check of something already guaranteed upstream. */
static int SchemeOk(const char *src)
{
    const char *p = src;
    char scheme[8];
    int i = 0;

    if (!p) return FALSE;
    while (*p && isspace((unsigned char) *p)) ++p;
    while (*p && *p != ':' && i < (int) sizeof(scheme) - 1
           && isalpha((unsigned char) *p)) {
        scheme[i++] = (char) tolower((unsigned char) *p);
        ++p;
    }
    scheme[i] = '\0';
    if (*p != ':') return FALSE;
    return strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0;
}

/* Runs `curl` (argv[], no shell -- see file header) with stdin/stderr
   redirected to /dev/null, response headers written to headerPath
   (-D) and the response body written to bodyPath (-o). Returns TRUE
   only if curl both execs and exits 0; on any failure the caller is
   responsible for whatever partial bodyPath/headerPath content may
   exist (both are always unlinked by httpimg_ResolveImage() before it
   returns, success or not). */
static boolean RunCurl(const char *src, int maxTimeSecs, const char *bodyPath, const char *headerPath, const char *resolveArg)
{
    char maxTimeBuf[16];
    char connectTimeoutBuf[16];
    char maxFilesizeBuf[24];
    /* 25 argv entries (the fixed sequence built below) plus the NULL
       terminator.  Sized with headroom and checked below: an exact fit
       has overflowed twice, once when the array was first sized and
       again when -g was added (2026-11-16), both times corrupting an
       adjacent stack buffer and breaking every image fetch. */
    char *argv[32];
    int argc = 0;
    pid_t pid;
    int status;

    sprintf(maxTimeBuf, "%d", maxTimeSecs);
    sprintf(connectTimeoutBuf, "%d", HTTPIMG_CONNECT_TIMEOUT);
    sprintf(maxFilesizeBuf, "%ld", (long) HTTPIMG_MAX_BYTES);

    argv[argc++] = "curl";
    argv[argc++] = "-s";
    argv[argc++] = "-S";
    /* No -L: redirects are followed by httpimg_ResolveImage, which
       checks each new host's addresses first (VetUrl). */
    argv[argc++] = "-g";	/* --globoff: [] and {} in a URL are literal, not curl ranges */
    argv[argc++] = "--resolve";      argv[argc++] = (char *) resolveArg;
    argv[argc++] = "--proto";        argv[argc++] = "=http,https";
    argv[argc++] = "--proto-redir";  argv[argc++] = "=http,https";
    argv[argc++] = "--connect-timeout"; argv[argc++] = connectTimeoutBuf;
    argv[argc++] = "--max-time";     argv[argc++] = maxTimeBuf;
    argv[argc++] = "--max-filesize"; argv[argc++] = maxFilesizeBuf;
    argv[argc++] = "-A";             argv[argc++] = "AUIS-messages-httpimg/1.0";
    argv[argc++] = "-D";             argv[argc++] = (char *) headerPath;
    argv[argc++] = "-o";             argv[argc++] = (char *) bodyPath;
    argv[argc++] = "--";             /* everything after this is positional, never a flag */
    argv[argc++] = (char *) src;
    if (argc >= (int) (sizeof(argv) / sizeof(argv[0]))) return FALSE;	/* never, but never silently */
    argv[argc] = NULL;

    pid = fork();
    if (pid < 0) return FALSE;

    if (pid == 0) {
        int nullIn = open("/dev/null", O_RDONLY);
        if (nullIn >= 0) { dup2(nullIn, 0); if (nullIn != 0) close(nullIn); }
        int nullErr = open("/dev/null", O_WRONLY);
        if (nullErr >= 0) { dup2(nullErr, 2); if (nullErr != 2) close(nullErr); }
        execvp("curl", argv);
        _exit(127); /* only reached if execvp itself failed (curl not on PATH, etc.) */
    }

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return FALSE;
    }
    return (boolean) (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/* Reads the last "Content-Type:" header line out of headerPath (curl
   -D, with -L following redirects, appends each response in the
   chain to this same file -- the last occurrence is the one that
   actually describes bodyPath's content) and returns a malloc'd,
   lowercased copy of just the type/subtype token (parameters like
   "; charset=..." and the trailing CRLF are stripped). Returns NULL
   if there is no such header, matching this module's "no Content-
   Type means refuse, don't guess from the URL" policy. Lowercased
   because ImageClassForMimetype() in htmlatk.c matches "image/" with
   a case-sensitive strncmp(), and real servers are not all
   consistent about header value casing. */
static char *ExtractContentType(const char *headerPath)
{
    FILE *fp;
    char line[512];
    char *last = NULL;
    long totalRead = 0;

    fp = fopen(headerPath, "r");
    if (!fp) return NULL;

    while (totalRead < HTTPIMG_HEADER_READ_CAP && fgets(line, sizeof(line), fp)) {
        size_t linelen = strlen(line);
        char *p, *end;
        totalRead += (long) linelen;

        if (linelen < 14 /* strlen("Content-Type:X") */) continue;
        if (strncasecmp(line, "Content-Type:", 13) != 0) continue;

        p = line + 13;
        while (*p && isspace((unsigned char) *p)) ++p;
        end = p;
        while (*end && *end != ';' && *end != '\r' && *end != '\n') ++end;
        while (end > p && isspace((unsigned char) end[-1])) --end;
        if (end == p) continue;

        if (last) free(last);
        last = (char *) malloc((size_t) (end - p) + 1);
        if (!last) break;
        memcpy(last, p, (size_t) (end - p));
        last[end - p] = '\0';
        for (p = last; *p; ++p) *p = (char) tolower((unsigned char) *p);
    }
    fclose(fp);
    return last;
}

/* stat()s bodyPath first and refuses (-1) anything empty or over
   HTTPIMG_MAX_BYTES before ever calling malloc()/fread() -- the
   independent enforcement of the cap RunCurl()'s --max-filesize
   already asked curl for, see HTTPIMG_MAX_BYTES's own comment. */
static long ReadFileCapped(const char *bodyPath, unsigned char **outBuf)
{
    struct stat st;
    FILE *fp;
    unsigned char *buf;
    size_t got;

    if (stat(bodyPath, &st) != 0) return -1;
    if (st.st_size <= 0 || st.st_size > HTTPIMG_MAX_BYTES) return -1;

    fp = fopen(bodyPath, "rb");
    if (!fp) return -1;

    buf = (unsigned char *) malloc((size_t) st.st_size);
    if (!buf) { fclose(fp); return -1; }

    got = fread(buf, 1, (size_t) st.st_size, fp);
    fclose(fp);
    if ((long) got != (long) st.st_size) { free(buf); return -1; }

    *outBuf = buf;
    return (long) st.st_size;
}

/* Returns a malloc'd copy of src with the characters a URL can't hold
   percent-encoded, as a browser does before fetching: space, control
   and non-ASCII bytes, and "<>\\^`{|}.  Existing %XX escapes are left
   alone.  The Economist's sponsor image had literal spaces in its query
   string, and curl refused it as malformed (exit 3) without sending
   anything (2026-11-16). */
static char *UrlForCurl(const char *src)
{
    static const char hex[] = "0123456789ABCDEF";
    char *out = malloc(strlen(src) * 3 + 1), *o;
    const unsigned char *p;

    if (!out) return NULL;
    for (p = (const unsigned char *) src, o = out; *p; ++p) {
        if (*p <= ' ' || *p >= 0x7F || strchr("\"<>\\^`{|}", *p)) {
            *o++ = '%'; *o++ = hex[*p >> 4]; *o++ = hex[*p & 0xF];
        } else {
            *o++ = (char) *p;
        }
    }
    *o = '\0';
    return out;
}

/* ---- Private-network protection ----

   A message can aim an image at localhost, a 192.168.x.x router or a
   cloud metadata address and make the reader's machine send a GET
   inside its own network.  Each URL's host is resolved here and
   refused if any of its addresses is loopback, private, link-local,
   shared (CGNAT), multicast or reserved; curl is then pinned to the
   checked address with --resolve, so it can't look the name up again
   and get a private answer (DNS rebinding).  Redirects are followed
   here, not by curl, so every hop is checked the same way. */

#define HTTPIMG_MAX_HOPS 5

static int V4IsPrivate(unsigned long a)	/* host byte order */
{
    unsigned b0 = (unsigned) (a >> 24), b1 = (unsigned) ((a >> 16) & 0xFF);

    return b0 == 0 || b0 == 10 || b0 == 127 || b0 >= 224	/* this net, private, loopback, multicast/reserved */
        || (b0 == 100 && (b1 & 0xC0) == 64)			/* 100.64/10 shared (CGNAT) */
        || (b0 == 169 && b1 == 254)				/* link-local, cloud metadata */
        || (b0 == 172 && (b1 & 0xF0) == 16)			/* 172.16/12 */
        || (b0 == 192 && b1 == 168)				/* 192.168/16 */
        || (b0 == 192 && b1 == 0 && ((a >> 8) & 0xFF) == 0)	/* 192.0.0/24 */
        || (b0 == 198 && (b1 & 0xFE) == 18);			/* 198.18/15 benchmarking */
}

static int AddrIsPrivate(const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return V4IsPrivate(ntohl(((const struct sockaddr_in *) sa)->sin_addr.s_addr));
    if (sa->sa_family == AF_INET6) {
        const unsigned char *b = ((const struct sockaddr_in6 *) sa)->sin6_addr.s6_addr;
        static const unsigned char zero[16];
        int i, mapped = 1;

        for (i = 0; i < 10; ++i) if (b[i]) mapped = 0;
        if (mapped && b[10] == 0xFF && b[11] == 0xFF)		/* ::ffff:a.b.c.d */
            return V4IsPrivate(((unsigned long) b[12] << 24) | ((unsigned long) b[13] << 16)
                               | ((unsigned long) b[14] << 8) | b[15]);
        if (memcmp(b, zero, 15) == 0 && (b[15] == 0 || b[15] == 1)) return 1;	/* ::, ::1 */
        return (b[0] & 0xFE) == 0xFC				/* fc00::/7 unique local */
            || (b[0] == 0xFE && (b[1] & 0xC0) == 0x80)		/* fe80::/10 link-local */
            || b[0] == 0xFF;					/* multicast */
    }
    return 1;	/* unknown family: refuse */
}

/* Checks url: http(s) only, no user:password@, and every address its
   host resolves to public.  Fills resolveArg with curl's
   "host:port:addr" pin for the first address.  Returns 0 if usable. */
static int VetUrl(const char *url, char *resolveArg, size_t argsize)
{
    const char *p = strstr(url, "://"), *h, *e, *a;
    char host[256], portbuf[8], addr[INET6_ADDRSTRLEN];
    int port, refuse = 0;
    struct addrinfo hints, *res = NULL, *ai;

    if (!p || !SchemeOk(url)) return -1;
    port = (strncasecmp(url, "https", 5) == 0) ? 443 : 80;
    h = p + 3;
    for (e = h; *e && *e != '/' && *e != '?' && *e != '#'; ++e)
        if (*e == '@') return -1;	/* credentials in the URL */
    if (*h == '[') {			/* [IPv6 literal] */
        a = strchr(h, ']');
        if (!a || a > e) return -1;
        if ((size_t) (a - h - 1) >= sizeof(host)) return -1;
        memcpy(host, h + 1, (size_t) (a - h - 1)); host[a - h - 1] = '\0';
        a++;
    } else {
        for (a = h; a < e && *a != ':'; ++a) ;
        if ((size_t) (a - h) >= sizeof(host) || a == h) return -1;
        memcpy(host, h, (size_t) (a - h)); host[a - h] = '\0';
    }
    if (a < e && *a == ':') {
        port = atoi(a + 1);
        if (port <= 0 || port > 65535) return -1;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(portbuf, "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &res) != 0 || !res) return -1;
    for (ai = res; ai; ai = ai->ai_next)
        if (AddrIsPrivate(ai->ai_addr)) refuse = 1;
    if (!refuse) {
        /* Pin an IPv4 address when there is one: it works on networks
           without IPv6, where an IPv6 pin would just fail. */
        struct addrinfo *pick = res;
        for (ai = res; ai; ai = ai->ai_next)
            if (ai->ai_family == AF_INET) { pick = ai; break; }
        if (pick->ai_family == AF_INET)
            inet_ntop(AF_INET, &((struct sockaddr_in *) pick->ai_addr)->sin_addr, addr, sizeof(addr));
        else
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *) pick->ai_addr)->sin6_addr, addr, sizeof(addr));
        if (snprintf(resolveArg, argsize, (pick->ai_family == AF_INET6) ? "%s:%d:[%s]" : "%s:%d:%s",
                     host, port, addr) >= (int) argsize)
            refuse = 1;
    }
    freeaddrinfo(res);
    return refuse ? -1 : 0;
}

/* Reads the response's status code and, for a redirect, its Location
   (into loc).  Returns the status, or 0. */
static int ReadRedirect(const char *headerPath, char *loc, size_t locsize)
{
    FILE *fp = fopen(headerPath, "r");
    char line[2048];
    int status = 0;

    loc[0] = '\0';
    if (!fp) return 0;
    if (fgets(line, sizeof(line), fp) && strncmp(line, "HTTP/", 5) == 0) {
        char *sp = strchr(line, ' ');
        if (sp) status = atoi(sp + 1);
    }
    while (fgets(line, sizeof(line), fp)) {
        if (strncasecmp(line, "Location:", 9) == 0) {
            char *v = line + 9, *end;
            while (*v == ' ' || *v == '\t') ++v;
            end = v + strlen(v);
            while (end > v && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ')) --end;
            *end = '\0';
            strncpy(loc, v, locsize - 1);
            loc[locsize - 1] = '\0';
        }
    }
    fclose(fp);
    return status;
}

/* Resolves a redirect's Location against the URL it came from. */
static char *JoinUrl(const char *base, const char *loc)
{
    const char *sch = strstr(base, "://"), *pathstart, *slash;
    char *out;
    size_t n;

    if (strstr(loc, "://")) return strdup(loc);
    if (!sch) return NULL;
    if (loc[0] == '/' && loc[1] == '/') {		/* //host/path */
        n = (size_t) (sch - base) + 1;
        out = malloc(n + strlen(loc) + 1);
        if (out) { memcpy(out, base, n); strcpy(out + n, loc); }
        return out;
    }
    pathstart = sch + 3;
    while (*pathstart && *pathstart != '/' && *pathstart != '?' && *pathstart != '#') ++pathstart;
    if (loc[0] == '/') {				/* /path */
        n = (size_t) (pathstart - base);
    } else {						/* relative to the base's directory */
        for (slash = pathstart; *slash && *slash != '?' && *slash != '#'; ++slash) ;
        while (slash > pathstart && slash[-1] != '/') --slash;
        n = (size_t) (slash - base);
        if (slash == pathstart) {			/* no path: add the root */
            out = malloc(n + strlen(loc) + 2);
            if (out) { memcpy(out, base, n); out[n] = '/'; strcpy(out + n + 1, loc); }
            return out;
        }
    }
    out = malloc(n + strlen(loc) + 1);
    if (out) { memcpy(out, base, n); strcpy(out + n, loc); }
    return out;
}

boolean httpimg_ResolveImage(void *rock, const char *src,
    unsigned char **bytesOut, long *lenOut, char **mimetypeOut)
{
    struct httpimg_budget *budget = (struct httpimg_budget *) rock;
    char bodyPath[]   = "/tmp/auis-httpimg-body.XXXXXX";
    char headerPath[] = "/tmp/auis-httpimg-hdr.XXXXXX";
    int bodyFd = -1, headerFd = -1;
    int perFetchTime;
    time_t now, remain;
    boolean curlOk;
    char *contentType = NULL;
    unsigned char *bytes = NULL;
    long len = -1;
    boolean result = FALSE;

    if (!budget || !src) return FALSE;
    if (!SchemeOk(src)) return FALSE; /* free rejection: never touches the network or the budget */

    /* Cache lookup before the budget check below (not after): a
       repeat src that's already cached costs neither a fetch nor
       budget, see httpimg.h's HTTPIMG_CACHE_SIZE comment for the real
       newsletter-chrome-icon repetition this is for. */
    {
        int ci;
        for (ci = 0; ci < budget->cacheCount; ++ci) {
            if (strcmp(budget->cache[ci].src, src) == 0) {
                boolean hit = CacheHit(&budget->cache[ci], bytesOut, lenOut, mimetypeOut);
                return hit;
            }
        }
    }

    now = time(NULL);
    if (now >= budget->deadline) return FALSE;
    remain = budget->deadline - now;
    perFetchTime = (remain < budget->perFetchMaxSeconds) ? (int) remain : budget->perFetchMaxSeconds;
    if (perFetchTime < 1) perFetchTime = 1;

    bodyFd = mkstemp(bodyPath);
    if (bodyFd < 0) return FALSE;
    close(bodyFd);
    headerFd = mkstemp(headerPath);
    if (headerFd < 0) { unlink(bodyPath); return FALSE; }
    close(headerFd);

    curlOk = FALSE;
    {
        char *cur = strdup(src), resolveArg[600], loc[2048];
        int hop;

        for (hop = 0; cur && hop <= HTTPIMG_MAX_HOPS; ++hop) {
            char *url, *next;
            int status;

            if (hop > 0 && time(NULL) >= budget->deadline) break;
            if (VetUrl(cur, resolveArg, sizeof(resolveArg)) != 0) break;
            url = UrlForCurl(cur);
            if (!url || !RunCurl(url, perFetchTime, bodyPath, headerPath, resolveArg)) { free(url); break; }
            free(url);
            status = ReadRedirect(headerPath, loc, sizeof(loc));
            if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && loc[0]) {
                next = JoinUrl(cur, loc);
                free(cur);
                cur = next;
                continue;
            }
            curlOk = TRUE;
            break;
        }
        free(cur);
    }
    if (curlOk) {
        contentType = ExtractContentType(headerPath);
        if (contentType && strncmp(contentType, "image/", 6) == 0) {
            len = ReadFileCapped(bodyPath, &bytes);
        }
    }

    unlink(bodyPath);
    unlink(headerPath);

    if (bytes && len > 0 && contentType) {
        CacheStore(budget, src, TRUE, bytes, len, contentType);
        *bytesOut = bytes;
        *lenOut = len;
        *mimetypeOut = contentType;
        return TRUE;
    }

    CacheStore(budget, src, FALSE, NULL, 0, NULL);
    if (bytes) free(bytes);
    if (contentType) free(contentType);
    return result;
}
