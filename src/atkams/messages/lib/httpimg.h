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
	httpimg.h -- a blocking, curl-backed htmlatk_ImageResolver
	(htmlatk.h) for http:/https: <img src>. NOT wired into any real
	rendering call site yet: text822.c's RenderHtmlPart() currently
	passes a NULL resolver to htmlatk_Render() on purpose ("real cid:
	image resolution is out of scope here, not silently skipped" --
	see that comment). Wiring httpimg_ResolveImage() in as the third
	argument there is a deliberately separate follow-up, for two
	reasons this module cannot decide for its caller:

	1. Policy: unconditionally fetching remote images every time a
	   message is opened turns <img src> into a read receipt/tracking
	   pixel -- the same reason every mainstream mail client defaults
	   remote image loading OFF and asks per-message or per-sender.
	   That preference (and its storage/UI) belongs in messages.c/
	   options.c, not here.

	2. Budget interaction with RenderHtmlPart()'s HTML_RENDER_TIME_
	   BUDGET (text822.c): that budget is checked *after*
	   htmlatk_Render() returns, not preemptively -- deliberately, per
	   its own comment, because both htmlpart_Parse() and
	   htmlatk_Render() are documented to always terminate quickly for
	   any input, so nothing needs interrupting mid-call. A network
	   fetch breaks that premise: it does not terminate quickly for
	   adversarial or merely slow/dead hosts. httpimg_ResolveImage()
	   is given its own explicit struct httpimg_budget (fetch count +
	   wall-clock deadline, see below) so a caller CAN bound total
	   worst-case time across every <img> in one message, but nothing
	   here enforces that a caller actually does -- the caller (whoever
	   wires this into RenderHtmlPart) needs to size that budget
	   against HTML_RENDER_TIME_BUDGET deliberately, not inherit it by
	   accident.

	Scheme gate: htmlpart.c's attr_allowed() lets ANY string through as
	<img src> -- unlike <a href>, which href_scheme_ok() restricts to
	http:/https:/mailto:, src gets no such gate at parse time (see
	htmlpart.c around attr_allowed/href_scheme_ok). So this module does
	its own scheme check before touching the network: only http:// and
	https:// (case-insensitive, leading whitespace tolerated, same
	evasion the href gate already guards against) proceed; anything
	else -- file:, data:, javascript:, cid:, a bare relative path, junk
	-- returns FALSE immediately, no subprocess spawned. curl is also
	told --proto/--proto-redir =http,https as defense in depth against
	a same-origin redirect pivoting to a different scheme.

	No shell is ever invoked: the curl command line is built as an
	argv[] and run via fork()+execvp(), not popen()/system(), so
	nothing in an attacker-controlled src string is ever shell-
	interpreted (contrast htmlatk_LaunchURL()'s popen() path, which
	must instead single-quote-escape its argument by hand because
	`open` has no argv-exec equivalent this codebase already uses
	here). This also sidesteps curl misreading a src that happens to
	start with '-' as a flag: the URL is always passed after a literal
	"--" argv separator.

	Non-goal: this does not attempt to block requests to private/
	local-network/link-local addresses (SSRF hardening). A sender-
	controlled URL can make this process fetch http://localhost/... or
	an internal address just as trivially as loading the same URL in
	any ordinary web browser's <img> tag would -- this module's threat
	model is "don't let the image renderer become a shell/file-read
	primitive or hang the app," not "don't let email make outbound
	requests to addresses on the reader's own network," which no
	mainstream mail client's remote-image loading defends against
	either. Flagged here rather than silently assumed.
*/

#ifndef HTTPIMG_H
#define HTTPIMG_H

#include <andrewos.h>
#include <class.h>
#include <time.h>

/* Per-render-pass budget, threaded through as the resolver's rock (see
   htmlatk_ImageResolver's rock parameter, htmlatk.h). One message can
   contain many <img> tags; without a shared cap each one would pay its
   own full network timeout independently, so a message with N slow/
   dead remote images could take on the order of N * (per-fetch
   timeout) wall-clock seconds before RenderHtmlPart() ever gets to
   check HTML_RENDER_TIME_BUDGET. httpimg_ResolveImage() decrements
   fetchesLeft once per real network attempt (success or failure --
   the cost already happened by the time curl exits, so it counts
   either way) so total elapsed wall-clock time across the whole
   render pass stays bounded by roughly fetchesLeft * (per-fetch
   timeout), capped further by deadline. A bad-scheme src (file:,
   data:, javascript:, cid:, ...) is rejected before any of that --
   it never touches the network or the fetch count, since it costs
   nothing to reject. Callers must allocate one of these per render pass (call
   httpimg_InitBudget() once, then pass its address as resolverRock to
   htmlatk_Render()) -- rock == NULL is treated as "no budget was
   configured" and every fetch is refused, not as "unlimited," since
   that is the safe direction to fail in. */
/* Cache is fixed-size and append-only for the lifetime of one render
   pass (never evicted, just stops growing once full) -- real mail
   that motivated this (a Shelf Awareness book-list newsletter)
   re-references the same handful of small chrome icons (buy/share
   buttons, social icons) once per row, and each repeat was paying a
   full network fetch and consuming the fetch-count budget for bytes
   already in hand from the first occurrence -- confirmed via a live
   trace: the exact same src (e.g. "sar3/buy-l.png") entering
   httpimg_ResolveImage() five separate times in one render pass. A
   cache hit costs neither a fetch nor budget. HTTPIMG_CACHE_SIZE is
   sized for "a handful of repeated icons," not "cache every unique
   image" -- the book covers themselves are each fetched once anyway,
   so they don't need caching; this only needs to be bigger than the
   distinct set of *repeated* srcs in real newsletter chrome. */
#define HTTPIMG_CACHE_SIZE 16

struct httpimg_cache_entry {
    char *src;
    boolean ok;
    unsigned char *bytes;   /* NULL when ok is FALSE */
    long len;
    char *mimetype;         /* NULL when ok is FALSE */
};

struct httpimg_budget {
    int fetchesLeft;
    time_t deadline;
    int cacheCount;
    struct httpimg_cache_entry cache[HTTPIMG_CACHE_SIZE];
};

/* Initializes budget for one render pass: up to maxFetches network
   attempts total, none of them starting after time(NULL)+maxSeconds.
   Each individual fetch's own --max-time is further capped to
   whatever time remains until that deadline when it starts, so a
   fetch that begins near the end of the window cannot by itself run
   past it. Caller must call httpimg_FreeBudget() on the same struct
   once the render pass is done (the cache above owns malloc'd
   copies of every distinct src's bytes/mimetype it saw, freed
   there). */
void httpimg_InitBudget(struct httpimg_budget *budget, int maxFetches, int maxSeconds);

/* Frees every cache entry's malloc'd src/bytes/mimetype. Safe to call
   on a budget that was InitBudget()'d but never used (cacheCount 0).
   Does not free budget itself -- callers own that allocation (a
   stack-local struct in every current caller), same as InitBudget()
   doesn't allocate the struct either. */
void httpimg_FreeBudget(struct httpimg_budget *budget);

/* htmlatk_ImageResolver-shaped (see htmlatk.h): rock must be a
   struct httpimg_budget* (see above). Resolves src via a single
   blocking `curl` subprocess (no shell, see file header) when src's
   scheme is http: or https: and the budget above still allows it;
   returns FALSE untouched-out-params for every other case (bad
   scheme, exhausted budget, curl exited nonzero, response bigger than
   this module's fixed size cap, or a Content-Type that doesn't start
   with "image/"). On TRUE, *bytesOut and *mimetypeOut are malloc'd copies
   the caller owns (matches htmlatk_ImageResolver's documented
   ownership contract) and no fetched bytes are left on disk -- the
   temp files curl wrote to are unlinked before this function
   returns either way. */
boolean httpimg_ResolveImage(void *rock, const char *src,
    unsigned char **bytesOut, long *lenOut, char **mimetypeOut);

#endif /* HTTPIMG_H */
