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

#include <andrewos.h>
#include <class.h>

#include "httpimg.h"

/* TEMPORARY diagnostic tracing -- see the 2026-08-18 national-grid.html
   "gives up partway through, at the same image every time" bug. Two
   fixes (budget sizing, then the im.c SIGCHLD wildcard-reap race) were
   deployed and neither changed the failure point at all, which is the
   opposite of what either theory predicts (a race should move around
   run to run; a too-small budget should move if the budget changes) --
   so before guessing a fourth time, this traces every call so the next
   repro can be read instead of inferred from a screenshot. Uses raw
   write(), not fprintf/stdio, deliberately: see feedback_lldb_
   debugging_workflow's note that fprintf can stay silent even when a
   function is proven to have run, most likely stdio buffering not
   getting flushed before something (a signal, _exit() in the forked
   child, the app itself) cuts output short. write() has no such
   buffer. Remove once the actual cause is found. */
static void Trace(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    int len;
    int fd = open("/tmp/httpimg-trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len > 0) write(fd, buf, (size_t) ((len < (int) sizeof(buf)) ? len : (int) sizeof(buf) - 1));
    close(fd);
}

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
static boolean RunCurl(const char *src, int maxTimeSecs, const char *bodyPath, const char *headerPath)
{
    char maxTimeBuf[16];
    char connectTimeoutBuf[16];
    char maxFilesizeBuf[24];
    /* 24 argv entries (see the fixed sequence built below) plus one
       for the required NULL terminator -- argv[24] = NULL, not
       argv[23], so this must be sized 25, not 24 (an off-by-one here
       previously wrote one pointer past the array's end and silently
       corrupted an adjacent stack buffer). */
    char *argv[25];
    int argc = 0;
    pid_t pid;
    int status;

    sprintf(maxTimeBuf, "%d", maxTimeSecs);
    sprintf(connectTimeoutBuf, "%d", HTTPIMG_CONNECT_TIMEOUT);
    sprintf(maxFilesizeBuf, "%ld", (long) HTTPIMG_MAX_BYTES);

    argv[argc++] = "curl";
    argv[argc++] = "-s";
    argv[argc++] = "-S";
    argv[argc++] = "-L";
    argv[argc++] = "--max-redirs";   argv[argc++] = "5";
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

    Trace("ENTER src=%s rock=%p\n", src ? src : "(null)", rock);

    if (!budget || !src) { Trace("  -> FALSE: no budget/src\n"); return FALSE; }
    if (!SchemeOk(src)) { Trace("  -> FALSE: bad scheme\n"); return FALSE; } /* free rejection: never touches the network or the budget */

    /* Cache lookup before the budget check below (not after): a
       repeat src that's already cached costs neither a fetch nor
       budget, see httpimg.h's HTTPIMG_CACHE_SIZE comment for the real
       newsletter-chrome-icon repetition this is for. */
    {
        int ci;
        for (ci = 0; ci < budget->cacheCount; ++ci) {
            if (strcmp(budget->cache[ci].src, src) == 0) {
                boolean hit = CacheHit(&budget->cache[ci], bytesOut, lenOut, mimetypeOut);
                Trace("  cache hit for %s ok=%d\n", src, hit);
                return hit;
            }
        }
    }

    now = time(NULL);
    if (now >= budget->deadline) { Trace("  -> FALSE: deadline passed (now=%ld deadline=%ld)\n", (long) now, (long) budget->deadline); return FALSE; }
    remain = budget->deadline - now;
    perFetchTime = (remain < budget->perFetchMaxSeconds) ? (int) remain : budget->perFetchMaxSeconds;
    if (perFetchTime < 1) perFetchTime = 1;

    Trace("  budget before: remain=%lds perFetchTime=%d\n", (long) remain, perFetchTime);

    bodyFd = mkstemp(bodyPath);
    if (bodyFd < 0) { Trace("  -> FALSE: mkstemp(body) failed errno=%d (%s)\n", errno, strerror(errno)); return FALSE; }
    close(bodyFd);
    headerFd = mkstemp(headerPath);
    if (headerFd < 0) { Trace("  -> FALSE: mkstemp(header) failed errno=%d (%s)\n", errno, strerror(errno)); unlink(bodyPath); return FALSE; }
    close(headerFd);

    curlOk = RunCurl(src, perFetchTime, bodyPath, headerPath);
    Trace("  RunCurl returned %d\n", curlOk);
    if (curlOk) {
        contentType = ExtractContentType(headerPath);
        Trace("  contentType=%s\n", contentType ? contentType : "(none)");
        if (contentType && strncmp(contentType, "image/", 6) == 0) {
            len = ReadFileCapped(bodyPath, &bytes);
            Trace("  ReadFileCapped returned len=%ld\n", len);
        }
    }

    unlink(bodyPath);
    unlink(headerPath);

    if (bytes && len > 0 && contentType) {
        CacheStore(budget, src, TRUE, bytes, len, contentType);
        *bytesOut = bytes;
        *lenOut = len;
        *mimetypeOut = contentType;
        Trace("  -> TRUE len=%ld\n", len);
        return TRUE;
    }

    CacheStore(budget, src, FALSE, NULL, 0, NULL);
    if (bytes) free(bytes);
    if (contentType) free(contentType);
    Trace("  -> FALSE (fell through: curlOk=%d)\n", curlOk);
    return result;
}
