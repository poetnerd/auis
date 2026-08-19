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
	htmlatk.c -- see htmlatk.h for the API and the rendering-decision
		     log (style composition via nested environments, style
		     reuse vs. per-link copies, table-cell scoping, bounded
		     table-nesting recursion, resolver ownership, popen
		     safety). ANSI C (C89 prototypes) throughout, no scanf/
		     sscanf/fscanf anywhere.

	The general (non-table) walk mirrors htmltext.c's iterative pre/
	post-order structure exactly (explicit heap-allocated work stack,
	PRE items pushed before a POST item and before children, so no C
	recursion) -- see htmltext.c's own file header comment. It differs
	from htmltext.c in what each visit *does*: instead of appending
	bytes to a scratch buffer, PRE/POST actions insert real characters
	into the caller's struct text via text_AlwaysInsertCharacters, and
	open/close real environment_InsertStyle() spans. Table-building
	(RenderTableAsLset / BuildLsetGrid below) is a separate, self-
	contained piece of code the main walk detours into at a <table>
	PRE action -- see htmlatk.h's own note on why *that* part uses
	ordinary bounded C recursion instead of an explicit stack.

	NOTE 2026-08-16: tables used to route to one of two constructions
	(table/spread, or lset) depending on whether they used colspan/
	rowspan -- that hybrid is gone. Every table now builds via lset,
	including colspan tables (a colspan cell is just a wider-weighted
	leaf in its row's own independent split chain -- see
	BuildLsetGrid's own comment for the full design and why it doesn't
	need table/spread's shared-grid model at all). See htmlatk.h's
	judgment-call log for the design discussion this replaced.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include <andrewos.h>
#include <class.h>
#include <text.ih>
#include <envrment.ih>
#include <style.ih>
#include <fontdesc.ih>
#include <dataobj.ih>
#include <lset.ih>
#include <lsetv.ih>

#include <htmlpart.h>
#include "htmlatk.h"

/* ==================================================================== *
 * Small reusable primitives
 * ==================================================================== */

static char *dupstr(const char *s)
{
    char *r;
    if (!s) return NULL;
    r = (char *) malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

/* Case-insensitive substring test -- used to interpret style="..."
   property values (font-weight/font-style/text-decoration), which
   htmlpart.c preserves verbatim (only the property *name* is matched
   case-insensitively at parse time, see htmlpart.c's filter_style()),
   so "Bold", "BOLD", "bold" must all be recognized here. */
static int value_contains_ci(const char *hay, const char *needle)
{
    size_t hlen, nlen, i;
    if (!hay) return 0;
    hlen = strlen(hay);
    nlen = strlen(needle);
    if (nlen == 0 || hlen < nlen) return 0;
    for (i = 0; i + nlen <= hlen; ++i) {
        if (strncasecmp(hay + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

/* Parses a decimal opaque attribute value (colspan/rowspan/font size --
   htmlpart.c hands these back unparsed, see htmlpart.h) per this
   tree's scanf-ban policy (strtol, not sscanf/atoi). Returns deflt for
   a missing/unparseable/non-positive value. maxval clamps against a
   degenerate/hostile value (e.g. colspan="999999999") -- a narrower,
   different judgment call than the size/depth cap the design doc
   rejects for genuine deep nesting: nothing about real HTML mail
   needs a four-thousand-column table, unlike 28 levels of nested
   layout tables, which is a real, legitimate pattern (see
   htmlatk.h/the design doc). */
static long ParsePositiveInt(const char *s, long deflt, long maxval)
{
    long v;
    char *end;
    if (!s || !*s) return deflt;
    v = strtol(s, &end, 10);
    if (end == s || v <= 0) return deflt;
    if (v > maxval) v = maxval;
    return v;
}

/* Like ParsePositiveInt, but for a width="..." attribute specifically
   -- unlike colspan/rowspan/font-size (ParsePositiveInt's other
   callers), a trailing '%' is extremely common on width and
   ParsePositiveInt's strtol-based parse doesn't notice trailing
   garbage at all (strtol("100%", &end, 10) happily returns 100 with
   end pointing at '%', which ParsePositiveInt never checks). Resolving
   a genuine percentage needs a container width not available at
   construction time -- same reasoning the old, since-removed
   ParseWidthPixels used (see this file's Table-strategy design-doc
   history) -- so this REJECTS (returns 0) anything with trailing
   content other than nothing or "px", rather than silently
   misreading "100%" as 100 pixels. */
static long ParseFixedPixelWidth(const char *s)
{
    long v;
    char *end;
    if (!s || !*s) return 0;
    v = strtol(s, &end, 10);
    if (end == s || v <= 0) return 0;
    if (*end != '\0' && strcmp(end, "px") != 0) return 0;
    if (v > 100000) v = 100000;
    return v;
}

/* Generic growable pointer vector, reused for row-collection and
   subtree scans below -- same "best-effort, drop rather than abort on
   OOM" policy as htmlpart.c/htmltext.c's own growable buffers. */
struct nodevec { const struct htmlnode **items; long count, cap; };

static void nodevec_init(struct nodevec *v) { v->items = NULL; v->count = 0; v->cap = 0; }

static void nodevec_push(struct nodevec *v, const struct htmlnode *n)
{
    if (v->count >= v->cap) {
        long ncap = v->cap ? v->cap * 2 : 32;
        const struct htmlnode **ni = (const struct htmlnode **) realloc((void *) v->items, ncap * sizeof(*ni));
        if (!ni) return;
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->count++] = n;
}

static void nodevec_free(struct nodevec *v) { free((void *) v->items); v->items = NULL; v->count = v->cap = 0; }

/* ==================================================================== *
 * Static/reusable style objects -- see htmlatk.h's "reuse, not
 * per-run allocation" note.
 * ==================================================================== */

static struct style *BoldSt, *ItalicSt, *UnderlineSt, *FixedSt, *LinkTemplateSt;
static int StylesInited = 0;

static void InitStyles(void)
{
    if (StylesInited) return;

    BoldSt = style_New();
    if (BoldSt) {
        style_SetName(BoldSt, "htmlatk-bold");
        style_AddNewFontFace(BoldSt, (long) fontdesc_Bold);
    }
    ItalicSt = style_New();
    if (ItalicSt) {
        style_SetName(ItalicSt, "htmlatk-italic");
        style_AddNewFontFace(ItalicSt, (long) fontdesc_Italic);
    }
    UnderlineSt = style_New();
    if (UnderlineSt) {
        style_SetName(UnderlineSt, "htmlatk-underline");
        style_AddUnderline(UnderlineSt);
    }
    FixedSt = style_New();
    if (FixedSt) {
        style_SetName(FixedSt, "htmlatk-fixed");
        style_AddNewFontFace(FixedSt, (long) fontdesc_Fixed);
        style_SetFontFamily(FixedSt, "andytype");
    }
    LinkTemplateSt = style_New();
    if (LinkTemplateSt) {
        style_SetName(LinkTemplateSt, "htmlatk-link");
        style_AddUnderline(LinkTemplateSt);
        style_AddAttribute(LinkTemplateSt, "color", "blue");
    }
    StylesInited = 1;
}

/* Small process-lifetime cache for arbitrary CSS color-ish values
   (style="color:..."/"background-color:...", <font color>) -- see
   htmlatk.h's reuse note. Keyed by "attrname\x01value" so the same
   literal color string used as foreground vs. background never
   collides. Capped; beyond the cap this degrades to one-off
   allocation (never reused, never freed here -- a bounded process-
   lifetime style leak for a pathological input with hundreds of
   distinct colors, same "best effort, not a crash" posture as the
   growable-buffer OOM paths above). */
struct colorcache_entry { char *key; struct style *st; };
#define COLORCACHE_MAX 64
static struct colorcache_entry ColorCache[COLORCACHE_MAX];
static int ColorCacheCount = 0;

/* Normalizes a CSS color value (from style="color:.../background-
   color:..." or <font color>) into something XParseColor -- the real
   name/hex resolver every ATK color ultimately goes through, see
   src/atk/basics/x/xcmap.c:180's xcolormap__SetColor -- can actually
   parse. XParseColor understands standard X11 color names and X11's
   old-style #RGB/#RRGGBB/#RRRGGGBBB/#RRRRGGGGBBBB hex (identical to
   CSS's own hex syntax), but knows nothing of CSS's rgb()/rgba()
   functional syntax or CSS-only keywords like inherit/currentColor/
   transparent -- those were previously passed through verbatim into
   style_AddAttribute and would silently fail deep in XParseColor (a
   stderr print, no color applied), which is what a design review
   caught (real fixture 15 has both `color:rgba(27,27,27,0.65)` and
   `color:inherit`).

   Returns a pointer into outbuf (caller-owned, at least 8 bytes) for
   an rgb()/rgba() value converted to "#RRGGBB" (alpha is dropped --
   ATK text color has no alpha/transparency channel, same "opaque"
   degradation already implicit everywhere else in this renderer).
   Returns NULL for inherit/initial/unset/currentColor/transparent (or
   an empty value) -- there is no cascade here to inherit *from*, so
   "apply no color override" is the correct semantic, not a guess, and
   NULL propagates out through AttrColorStyleFor/marks_push (see
   marks_push's own note on why callers must not just assume every
   push succeeds) to mean exactly that. Anything else (presumably a
   bare color name) passes through unchanged -- CSS's common named
   colors and X11's rgb.txt overlap heavily, and a name that doesn't
   overlap fails exactly the way it already did before this function
   existed, not a new failure mode, just no longer reached for the
   rgb()/rgba()/keyword cases this function does handle. */
static const char *CssColorToX11(const char *value, char *outbuf, size_t outbufsz)
{
    char tmp[64];
    size_t i, n;
    long r, g, b;
    char *p, *endp;

    while (*value == ' ' || *value == '\t') ++value;
    n = strlen(value);
    while (n > 0 && (value[n - 1] == ' ' || value[n - 1] == '\t')) --n;
    if (n == 0) return NULL;
    if (n >= sizeof(tmp)) return value; /* implausibly long -- not ours to fix, pass through */
    for (i = 0; i < n; ++i) tmp[i] = (char) tolower((unsigned char) value[i]);
    tmp[n] = '\0';

    if (strcmp(tmp, "inherit") == 0 || strcmp(tmp, "initial") == 0
        || strcmp(tmp, "unset") == 0 || strcmp(tmp, "currentcolor") == 0
        || strcmp(tmp, "transparent") == 0)
        return NULL;

    if (strncmp(tmp, "rgb(", 4) == 0 || strncmp(tmp, "rgba(", 5) == 0) {
        if (outbufsz < 8) return NULL;
        p = strchr(tmp, '(');
        if (!p) return value; /* malformed -- let XParseColor fail on it as-is, same as before */
        ++p;
        r = strtol(p, &endp, 10);
        if (endp == p || *endp == '%') return NULL; /* percentages/malformed -- don't guess a color */
        p = endp; while (*p == ' ' || *p == ',') ++p;
        g = strtol(p, &endp, 10);
        if (endp == p || *endp == '%') return NULL;
        p = endp; while (*p == ' ' || *p == ',') ++p;
        b = strtol(p, &endp, 10);
        if (endp == p || *endp == '%') return NULL;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        sprintf(outbuf, "#%02lx%02lx%02lx", r, g, b);
        return outbuf;
    }

    return value;
}

static struct style *AttrColorStyleFor(const char *attrname, const char *value)
{
    char key[300];
    char colorbuf[16];
    int i;
    struct style *st;

    if (!value || !*value) return NULL;
    value = CssColorToX11(value, colorbuf, sizeof(colorbuf));
    if (!value) return NULL;
    sprintf(key, "%.32s\1%.256s", attrname, value);
    for (i = 0; i < ColorCacheCount; ++i) {
        if (strcmp(ColorCache[i].key, key) == 0) return ColorCache[i].st;
    }
    st = style_New();
    if (!st) return NULL;
    style_AddAttribute(st, (char *) attrname, (char *) value);
    if (ColorCacheCount < COLORCACHE_MAX) {
        ColorCache[ColorCacheCount].key = dupstr(key);
        ColorCache[ColorCacheCount].st = st;
        if (ColorCache[ColorCacheCount].key) ++ColorCacheCount;
    }
    return st;
}

/* <font size="N">, N clamped to HTML's traditional 1-7 range -- a
   small fixed cache, same reuse reasoning as the bold/italic/
   underline statics above (there are only 7 possible buckets). */
static struct style *FontSizeCache[8]; /* index 1..7 used */

static struct style *FontSizeStyleFor(long n)
{
    static const long points[8] = { 0, 8, 10, 12, 14, 18, 24, 32 };
    if (n < 1) n = 1;
    if (n > 7) n = 7;
    if (!FontSizeCache[n]) {
        struct style *st = style_New();
        if (!st) return NULL;
        style_SetFontSize(st, style_ConstantFontSize, points[n]);
        FontSizeCache[n] = st;
    }
    return FontSizeCache[n];
}

/* ==================================================================== *
 * Render state, break/whitespace bookkeeping (deferred-space model --
 * see htmlatk.c's design notes in the delegated-session report; no
 * live re-reading of dest's already-inserted characters is needed,
 * every decision is driven off small counters updated as this module
 * inserts).
 * ==================================================================== */

/* Used two ways: on the open-mark stack, only style/start are live
   (len is filled in once the tag closes); on the pending-application
   list (see ApplyPendingStyles), all three fields are live. */
struct hax_mark { struct style *style; long start; long len; };

struct hax_listframe { int ordered; int counter; };

struct hax_state {
    struct text *dest;
    long pos;
    int anyContent;     /* has anything at all been inserted yet */
    int spacePending;   /* a collapsed source space is waiting to be flushed */
    int trailingNL;     /* 0, 1, or 2 (capped) consecutive real newlines at the tail */
    int predepth;
    struct hax_listframe *list; long listcount, listcap;
    struct hax_mark *marks; long markcount, markcap;
    /* Completed (start,len,style) spans, applied in one final pass
       after the whole tree has been walked and every character/view
       has been inserted -- see this file's header comment ("Deferred
       style application") for why this can't happen inline during
       the walk. */
    struct hax_mark *pending; long pendingcount, pendingcap;
    htmlatk_ImageResolver resolver;
    void *resolverRock;
    int hardfail;
};

/* Returns 1 if a mark was actually pushed, 0 if not (NULL style, or a
   realloc failure) -- callers must use this to decide whether they
   opened a mark, not just assume every call here succeeds. Getting
   this wrong is a real bug, not just a style nit: marks_finalize pops
   exactly as many marks as it's told a tag opened, and a caller that
   overcounts (e.g. always incrementing a local "opened" counter
   regardless of whether marks_push actually pushed anything) causes
   marks_finalize to pop marks belonging to an enclosing/earlier tag
   instead, corrupting that unrelated span. See AttrColorStyleFor's
   own note on why NULL returns became common once CSS color
   normalization was added (`inherit`/`transparent`/etc. are frequent
   in real mail and now correctly produce no style at all, not a
   phantom pushed-then-never-really-pushed mark). */
static int marks_push(struct hax_state *st, struct style *s)
{
    if (!s) return 0;
    if (st->markcount >= st->markcap) {
        long ncap = st->markcap ? st->markcap * 2 : 16;
        struct hax_mark *ni = (struct hax_mark *) realloc(st->marks, ncap * sizeof(struct hax_mark));
        if (!ni) return 0;
        st->marks = ni;
        st->markcap = ncap;
    }
    st->marks[st->markcount].style = s;
    st->marks[st->markcount].start = st->pos;
    ++st->markcount;
    return 1;
}

static void pending_push(struct hax_state *st, struct style *style, long start, long len)
{
    if (st->pendingcount >= st->pendingcap) {
        long ncap = st->pendingcap ? st->pendingcap * 2 : 32;
        struct hax_mark *ni = (struct hax_mark *) realloc(st->pending, ncap * sizeof(struct hax_mark));
        if (!ni) return;
        st->pending = ni;
        st->pendingcap = ncap;
    }
    st->pending[st->pendingcount].style = style;
    st->pending[st->pendingcount].start = start;
    st->pending[st->pendingcount].len = len;
    ++st->pendingcount;
}

/* Pops the top n *open* marks (LIFO) and records each one's now-final
   (start,len,style) into st->pending for real application later --
   does NOT call environment_InsertStyle/WrapStyle here. See this
   file's header comment: text_AlwaysInsertCharacters() auto-extends
   any style span whose range abuts the insertion point (confirmed
   empirically during this task -- inserting characters immediately
   after an already-created span's end silently grows that span to
   swallow the new, unrelated content, and repeats for every further
   insertion after that point). Since this walk keeps inserting more
   document content after any given tag's marks would otherwise be
   finalized, marks cannot be safely materialized until the entire
   walk -- and therefore every insertion -- is complete. */
static void marks_finalize(struct hax_state *st, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        struct hax_mark m;
        long len;
        if (st->markcount == 0) break;
        m = st->marks[--st->markcount];
        len = st->pos - m.start;
        if (len <= 0) continue;
        pending_push(st, m.style, m.start, len);
    }
}

/* Real, final application of every recorded span -- called exactly
   once, after the main walk loop has finished inserting every
   character and view. Order among independent (or nested) spans does
   not matter here (see htmlatk.h's style-composition note) because
   nothing inserted after this point disturbs anything created here. */
static void ApplyPendingStyles(struct hax_state *st)
{
    long i;
    for (i = 0; i < st->pendingcount; ++i) {
        struct environment *et = environment_WrapStyle(
            ((struct text *) st->dest)->rootEnvironment,
            st->pending[i].start, st->pending[i].len, st->pending[i].style);
        if (!et) st->hardfail = 1;
    }
}

static int EndsInWhitespace(struct hax_state *st)
{
    return !st->anyContent || st->trailingNL > 0 || st->spacePending;
}

static void FlushPendingSpace(struct hax_state *st)
{
    if (st->spacePending) {
        text_AlwaysInsertCharacters(st->dest, st->pos, " ", 1);
        ++st->pos;
        st->spacePending = 0;
        st->trailingNL = 0;
        st->anyContent = 1;
    }
}

static void MaybeSpace(struct hax_state *st)
{
    if (!EndsInWhitespace(st)) st->spacePending = 1;
}

/* Inserts a literal (already-formed, no further collapsing) run --
   markers, the href fallback text, hr's dashed rule, etc. Never
   contains an embedded newline in practice; callers that need a
   newline use EnsureLineBreak/EnsureParaBreak instead so the
   trailingNL bookkeeping stays correct. */
static void InsertLiteral(struct hax_state *st, const char *s)
{
    long n = (long) strlen(s);
    if (n == 0) return;
    FlushPendingSpace(st);
    text_AlwaysInsertCharacters(st->dest, st->pos, (char *) s, n);
    st->pos += n;
    st->anyContent = 1;
    st->trailingNL = 0;
    st->spacePending = 0;
}

static void EnsureLineBreak(struct hax_state *st)
{
    st->spacePending = 0;
    if (!st->anyContent) return;
    if (st->trailingNL == 0) {
        text_AlwaysInsertCharacters(st->dest, st->pos, "\n", 1);
        ++st->pos;
        st->trailingNL = 1;
    }
}

static void EnsureParaBreak(struct hax_state *st)
{
    st->spacePending = 0;
    if (!st->anyContent) return;
    while (st->trailingNL < 2) {
        text_AlwaysInsertCharacters(st->dest, st->pos, "\n", 1);
        ++st->pos;
        ++st->trailingNL;
    }
}

/* Same collapsing rules as htmltext.c's emit_text_run/is_collapsible_
   space (0xA0 included -- see that file's own comment on why), just
   driven by the counters above instead of buffer-peeking. */
static int is_collapsible_space(unsigned char c) { return isspace(c) || c == 0xA0; }

static void EmitTextRun(struct hax_state *st, const char *text, long len, int inPre)
{
    long i;

    if (inPre) {
        if (len <= 0) return;
        FlushPendingSpace(st);
        text_AlwaysInsertCharacters(st->dest, st->pos, (char *) text, len);
        st->pos += len;
        st->anyContent = 1;
        st->spacePending = 0;
        st->trailingNL = (text[len - 1] == '\n') ? 1 : 0;
        return;
    }

    i = 0;
    while (i < len) {
        unsigned char c = (unsigned char) text[i];
        if (is_collapsible_space(c)) {
            while (i < len && is_collapsible_space((unsigned char) text[i])) ++i;
            if (!EndsInWhitespace(st)) st->spacePending = 1;
        } else {
            /* batch a run of non-space bytes into one insert call */
            long start = i;
            while (i < len && !is_collapsible_space((unsigned char) text[i])) ++i;
            FlushPendingSpace(st);
            text_AlwaysInsertCharacters(st->dest, st->pos, (char *) text + start, i - start);
            st->pos += (i - start);
            st->anyContent = 1;
            st->trailingNL = 0;
        }
    }
}

/* ==================================================================== *
 * List markers, image placeholder text -- ported 1:1 from htmltext.c's
 * own logic (see htmlatk.h's reuse note); text content, just emitted
 * via InsertLiteral/EmitTextRun instead of appended to a byte buffer.
 * ==================================================================== */

static void listctx_push(struct hax_state *st, int ordered)
{
    if (st->listcount >= st->listcap) {
        long ncap = st->listcap ? st->listcap * 2 : 16;
        struct hax_listframe *ni = (struct hax_listframe *) realloc(st->list, ncap * sizeof(struct hax_listframe));
        if (!ni) return;
        st->list = ni;
        st->listcap = ncap;
    }
    st->list[st->listcount].ordered = ordered;
    st->list[st->listcount].counter = 0;
    ++st->listcount;
}

static void listctx_pop(struct hax_state *st) { if (st->listcount > 0) --st->listcount; }
static struct hax_listframe *listctx_top(struct hax_state *st)
{
    return (st->listcount > 0) ? &st->list[st->listcount - 1] : NULL;
}

static void emit_li_marker(struct hax_state *st)
{
    struct hax_listframe *top = listctx_top(st);
    long depth = st->listcount;
    long indent = (depth > 1) ? (depth - 1) * 2 : 0;
    long i;
    char numbuf[32];

    EnsureLineBreak(st);
    for (i = 0; i < indent; ++i) InsertLiteral(st, " ");
    if (top && top->ordered) {
        ++top->counter;
        sprintf(numbuf, "%d. ", top->counter);
        InsertLiteral(st, numbuf);
    } else {
        InsertLiteral(st, "- ");
    }
}

/* Fills label (a small caller buffer, at least 9 bytes: "img.jpeg" +
   NUL) with "img.<ext>" when a format can be identified, or plain
   "image" otherwise -- the word BuildImgPlaceholder below brackets.
   Prefers mimetypeHint (the real fetched Content-Type, when the
   resolver actually got bytes but something downstream -- e.g. no
   decoder for this format at all, see raster__ReadOtherFormat's
   image/xwd-only gate -- failed to turn them into a displayable
   image: knowing it really was a PNG the toolkit simply can't decode
   is more useful than a placeholder that just says "image") over
   guessing from src's URL path extension (the only signal available
   when the resolver was never called at all -- no resolver installed,
   remote loading off, a non-http(s) scheme). Both are restricted to a
   fixed set of recognized image extensions/subtypes rather than
   echoing whatever trailing token happens to follow the last '.' in
   a URL verbatim -- a tracking-pixel URL's "open.aspx" is not useful
   as "[img.aspx]", and an untrusted sender's src is not a string this
   function should insert into the rendered document unfiltered. */
static void ImgFormatLabel(char label[16], const char *mimetypeHint, const char *src)
{
    static const char *mimeToExt[][2] = {
        {"jpeg", "jpg"}, {"png", "png"}, {"gif", "gif"}, {"webp", "webp"},
        {"bmp", "bmp"}, {"x-icon", "ico"}, {"svg+xml", "svg"},
        {"pbm", "pbm"}, {"pnm", "pnm"}, {"ppm", "ppm"}, {"pgm", "pgm"},
    };
    static const char *knownExt[] = {
        "jpg", "jpeg", "png", "gif", "webp", "bmp", "ico", "svg",
        "pbm", "pnm", "ppm", "pgm",
    };
    size_t i;

    if (mimetypeHint && strncmp(mimetypeHint, "image/", 6) == 0) {
        const char *sub = mimetypeHint + 6;
        for (i = 0; i < sizeof(mimeToExt) / sizeof(mimeToExt[0]); ++i) {
            if (strcmp(sub, mimeToExt[i][0]) == 0) {
                sprintf(label, "img.%s", mimeToExt[i][1]);
                return;
            }
        }
    }

    if (src) {
        const char *p, *dot = NULL, *slash = NULL;
        char ext[8];
        int ei = 0;
        for (p = src; *p && *p != '?' && *p != '#'; ++p) {
            if (*p == '.') dot = p;
            if (*p == '/') slash = p;
        }
        if (dot && (!slash || dot > slash)) {
            ++dot;
            while (dot[ei] && ei < (int) sizeof(ext) - 1 && isalnum((unsigned char) dot[ei])) {
                ext[ei] = (char) tolower((unsigned char) dot[ei]);
                ++ei;
            }
            ext[ei] = '\0';
            for (i = 0; i < sizeof(knownExt) / sizeof(knownExt[0]); ++i) {
                if (strcmp(ext, knownExt[i]) == 0) {
                    sprintf(label, "img.%s", ext);
                    return;
                }
            }
        }
    }

    strcpy(label, "image");
}

/* Builds "[img.png: alt]" / "[img.png]" (or "[image: alt]" / "[image]"
   when no format could be identified, see ImgFormatLabel above) into
   a caller buffer (bufsz must be comfortably larger than any real alt
   text -- truncates rather than overflowing for a pathological alt
   attribute). mimetypeHint is the real fetched Content-Type when
   available (NULL if the resolver was never called or declined -- see
   ImgFormatLabel's own note on why that's the preferred source when
   present). Shared by the top-level inline placeholder and the
   table-cell placeholder. */
static void BuildImgPlaceholder(char *buf, size_t bufsz, const struct htmlnode *n, const char *mimetypeHint)
{
    char label[16];
    const char *alt = htmlpart_GetAttr(n, "alt");

    ImgFormatLabel(label, mimetypeHint, htmlpart_GetAttr(n, "src"));
    if (alt && alt[0]) {
        /* alt text itself gets the same whitespace-collapse treatment
           as ordinary text content per htmltext.h -- reuse htmltext_
           ToText's collapsing indirectly is overkill for a single
           attribute string, so this does a small local collapse. */
        char tmp[512];
        size_t ti = 0, ai = 0, alen = strlen(alt);
        int lastspace = 1;
        while (alt[ai] && ti + 1 < sizeof(tmp)) {
            unsigned char c = (unsigned char) alt[ai++];
            if (isspace(c) || c == 0xA0) {
                if (!lastspace) { tmp[ti++] = ' '; lastspace = 1; }
            } else {
                tmp[ti++] = (char) c;
                lastspace = 0;
            }
        }
        while (ti > 0 && tmp[ti - 1] == ' ') --ti;
        tmp[ti] = '\0';
        (void) alen;
        sprintf(buf, "[%.*s: %.*s]", (int) sizeof(label) - 1, label, (int) (bufsz > 48 ? bufsz - 48 : 1), tmp);
    } else {
        sprintf(buf, "[%.*s]", (int) (bufsz > 4 ? bufsz - 4 : 1), label);
    }
}

static void emit_img_placeholder_inline(struct hax_state *st, const struct htmlnode *n, const char *mimetypeHint)
{
    char buf[600];
    BuildImgPlaceholder(buf, sizeof(buf), n, mimetypeHint);
    MaybeSpace(st);
    InsertLiteral(st, buf);
}

/* ==================================================================== *
 * Image resolution + embedding -- shared by the top-level <img>
 * handler and the table-cell embedder. See htmlatk.h's resolver note.
 * ==================================================================== */

/* Mirrors text822.c's InsertProperObject dispatch table exactly (see
   the Gate-1 research: src/atkams/messages/lib/text822.c around its
   "image/" handling) -- deliberate reuse of established, working
   precedent in this exact codebase, not a fresh design. */
static const char *ImageClassForMimetype(const char *mt)
{
    if (!mt || strncmp(mt, "image/", 6) != 0) return "raster";
    if (strncmp(mt + 6, "gif", 3) == 0 || strncmp(mt + 6, "x-gif", 5) == 0) return "gif";
    if (strncmp(mt + 6, "pbm", 3) == 0 || strncmp(mt + 6, "pnm", 3) == 0
        || strncmp(mt + 6, "ppm", 3) == 0 || strncmp(mt + 6, "pgm", 3) == 0) return "pbm";
    if (strncmp(mt + 6, "jpeg", 4) == 0) return "jpeg";
    if (strncmp(mt + 6, "png", 3) == 0) return "png";
    return "raster";
}

/* Identifies an image's real format from its own leading bytes,
   independent of whatever Content-Type the server claimed. Real-
   world motivating case (media.shelf-awareness.com's book-cover
   images, seen via a live national-grid.html/book-rack repro):
   actual JPEG bytes served under a .gif URL with a genuine, server-
   sent "Content-Type: image/gif" header. That is not this module's
   bug, and not a gif__Load bug either -- confirmed directly by
   fetching the exact URLs, running `file` on the bytes (reports
   "JPEG image data"), and feeding those same bytes to the real ATK
   gif class headlessly, which correctly rejects them at the GIF
   signature check gifin_open_file() does first. The decoder was
   never broken; the label was.

   Only ever consulted from RenderImageInline() AFTER the server-
   labeled decoder attempt already failed -- trusting the declared
   Content-Type first is still correct and free for the overwhelming
   common case where it's accurate; this is a fallback for the real
   world's mislabeled minority, not a replacement for believing
   servers at all. Returns NULL, not a guess, for anything this
   module doesn't recognize -- an unidentified magic number should
   fall through to the ordinary placeholder, not a second wrong
   attempt. Recognizes png here for the same reason it recognizes
   jpeg/gif: ImageClassForMimetype() now routes "image/png" to the
   real png class (src/atk/basics/common/png.c), so a mislabeled-as-
   something-else real PNG works through this same retry path with no
   further change here, which is exactly the point of sniffing the
   bytes instead of the filename. */
static const char *SniffImageMimetype(const unsigned char *bytes, long len)
{
    if (!bytes || len < 4) return NULL;
    if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return "image/jpeg";
    if (len >= 6 && (memcmp(bytes, "GIF87a", 6) == 0 || memcmp(bytes, "GIF89a", 6) == 0)) return "image/gif";
    if (len >= 8 && memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0) return "image/png";
    return NULL;
}

/* Attempts to read already-fetched, not-yet-CTE-decoded-by-us raw
   bytes (the resolver hands back fully decoded image bytes, so
   encoding is always "" here -- see image__ReadOtherFormat's contract
   in src/atk/basics/common/image.c, which only re-decodes base64/qp
   itself when told to) into targetObj via fmemopen, matching Gate 1's
   validated approach (see the delegated-session smoke test). Returns
   the ReadOtherFormat result; does not destroy targetObj on failure --
   that is the caller's job, since the caller also owns how targetObj
   was created. */
static boolean TryReadImageInto(struct dataobject *targetObj, const char *mimetype,
                                  unsigned char *bytes, long len)
{
    FILE *fp;
    boolean ok;

    if (!targetObj || !bytes || len <= 0) return FALSE;
    fp = fmemopen(bytes, (size_t) len, "r");
    if (!fp) return FALSE;
    ok = dataobject_ReadOtherFormat(targetObj, fp, (char *) mimetype, "", NULL);
    fclose(fp);
    return ok;
}

/* TEMPORARY diagnostic tracing -- same shape/rationale as httpimg.c's
   Trace() (raw write(), not stdio, see that comment), and deliberately
   writing to the same /tmp/httpimg-trace.log so a repro's network-
   fetch trace and this decode-layer trace interleave into one
   chronological narrative instead of two files to cross-reference by
   hand. Added because httpimg.c's own trace only covers whether the
   *fetch* succeeded -- it has no visibility into what RenderImageInline
   does with the bytes afterward, and a real repro (a shelf-awareness
   book cover still showing as a placeholder after the sniff-retry
   fix) needs exactly that visibility to diagnose. Remove once the
   actual cause is found. */
static void TraceDecode(const char *fmt, ...)
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

/* Redirects fd 2 (stderr) to /dev/null for the duration of a single
   speculative decode attempt this function expects to fail routinely
   now that a first attempt failing is a normal, handled step (trigger
   the sniff-based retry) rather than a real problem -- gif__Load's own
   "Couldn't read GIF image." (gif.c) has no way to know it's being
   tried speculatively and will print on every mislabeled-as-gif image,
   which is now the common case for some real-world senders (shelf-
   awareness.com), not the rare one. Deliberately local to just this
   one call via dup2 save/restore, not a change to gif.c itself: gif__Load
   is called from other places in the app (gif__Read, gif__Ident) that
   have nothing to do with html image rendering and should keep
   whatever diagnostic behavior they already have. Restores stderr
   unconditionally even if open()/dup() failed partway (savedFd < 0 is
   checked before the restoring dup2, not assumed to have succeeded). */
static int SuppressStderrBegin(void)
{
    int savedFd;
    int devnull;
    fflush(stderr);
    savedFd = dup(2);
    devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
    return savedFd;
}

static void SuppressStderrEnd(int savedFd)
{
    fflush(stderr);
    if (savedFd >= 0) { dup2(savedFd, 2); close(savedFd); }
}

/* Top-level (non-table-cell) <img> handling: try a real embed via the
   resolver; on any failure (no resolver, resolver declines, format
   unsupported) fall back to the inline text placeholder. If the
   server-declared Content-Type's decoder fails, retries once against
   whatever SniffImageMimetype() identifies from the bytes themselves
   -- see that function's own note on why (a mislabeled-but-otherwise-
   fine image, e.g. real JPEG bytes served as "image/gif") and why
   this is a fallback attempted only after the declared type already
   failed, not a first-choice replacement for it. */
static void RenderImageInline(struct hax_state *st, const struct htmlnode *n)
{
    const char *src = htmlpart_GetAttr(n, "src");
    unsigned char *bytes = NULL;
    long len = 0;
    char *mimetype = NULL;
    boolean resolved = FALSE;
    struct dataobject *dob = NULL;
    boolean ok = FALSE;
    const char *placeholderHint = NULL;

    if (src && st->resolver) {
        resolved = (*st->resolver)(st->resolverRock, src, &bytes, &len, &mimetype);
    }

    TraceDecode("DECODE ENTER src=%s resolved=%d bytes=%p len=%ld mimetype=%s\n",
        src ? src : "(null)", resolved, (void *) bytes, len, mimetype ? mimetype : "(null)");

    if (resolved && bytes) {
        int savedErr;
        placeholderHint = mimetype;
        dob = (struct dataobject *) class_NewObject((char *) ImageClassForMimetype(mimetype));
        savedErr = SuppressStderrBegin(); /* speculative: a labeled-decode failure here is expected/routine now, see SuppressStderrBegin's own comment */
        ok = dob && TryReadImageInto(dob, mimetype, bytes, len);
        SuppressStderrEnd(savedErr);
        TraceDecode("  attempt1 class=%s ok=%d\n", ImageClassForMimetype(mimetype), ok);

        if (!ok) {
            const char *sniffed = SniffImageMimetype(bytes, len);
            TraceDecode("  sniffed=%s\n", sniffed ? sniffed : "(null)");
            if (sniffed && (!mimetype || strcmp(sniffed, mimetype) != 0)) {
                if (dob) dataobject_Destroy(dob);
                dob = (struct dataobject *) class_NewObject((char *) ImageClassForMimetype(sniffed));
                ok = dob && TryReadImageInto(dob, sniffed, bytes, len);
                placeholderHint = sniffed; /* the truth, whether or not this retry itself succeeded */
                TraceDecode("  attempt2 class=%s ok=%d\n", ImageClassForMimetype(sniffed), ok);
            }
        }

        TraceDecode("  -> final ok=%d\n", ok);
        if (ok) {
            FlushPendingSpace(st); /* AddView doesn't collapse into text runs like InsertLiteral does */
            text_AlwaysAddView(st->dest, st->pos, dataobject_ViewName(dob), dob);
            ++st->pos;
            st->anyContent = 1;
            st->trailingNL = 0;
            st->spacePending = 0;
            free(bytes);
            free(mimetype);
            return;
        }
        if (dob) dataobject_Destroy(dob);
    }
    /* placeholderHint (when non-NULL here) is the best-known real
       format for a resolved-but-undecodable image -- the sniffed
       type if a sniff was attempted (truthful regardless of whether
       the retry itself succeeded), else the server's declared
       Content-Type -- so BuildImgPlaceholder needs it before
       mimetype is freed below. */
    emit_img_placeholder_inline(st, n, placeholderHint);
    free(bytes);
    free(mimetype);
}

/* ==================================================================== *
 * Table building. Bounded real C recursion across nested <table>
 * levels -- see htmlatk.h's judgment-call note on why this differs
 * from the rest of this project's no-C-recursion discipline.
 * ==================================================================== */

/* Collects, in document order, every <tr> that belongs directly to
   tablenode -- descending through transparent containers (thead/
   tbody, or any other still-present wrapper) but never into a nested
   <table>, whose own rows are that table's business, not this one's.
   Iterative (explicit stack), even though it isn't in the bounded-
   recursion-is-fine category the table-nesting recursion itself is --
   this one costs nothing extra to make non-recursive and searches
   sibling/child structure, not nesting depth, so there's no reason to
   special-case it. */
static void CollectRows(const struct htmlnode *tablenode, struct nodevec *rows)
{
    struct nodevec stack;
    const struct htmlnode *c;
    long i;

    nodevec_init(&stack);
    nodevec_init(rows);
    /* push in reverse so we pop in document order */
    {
        struct nodevec tmp;
        nodevec_init(&tmp);
        for (c = tablenode->children; c; c = c->next) nodevec_push(&tmp, c);
        for (i = tmp.count - 1; i >= 0; --i) nodevec_push(&stack, tmp.items[i]);
        nodevec_free(&tmp);
    }
    while (stack.count > 0) {
        const struct htmlnode *n = stack.items[--stack.count];
        if (!htmlpart_IsElement(n)) continue;
        if (strcmp(n->tag, "tr") == 0) { nodevec_push(rows, n); continue; }
        if (strcmp(n->tag, "table") == 0) continue; /* nested table: not our row */
        if (n->children) {
            struct nodevec tmp;
            nodevec_init(&tmp);
            for (c = n->children; c; c = c->next) nodevec_push(&tmp, c);
            for (i = tmp.count - 1; i >= 0; --i) nodevec_push(&stack, tmp.items[i]);
            nodevec_free(&tmp);
        }
    }
    nodevec_free(&stack);
}

/* A <table> with exactly one real row containing exactly one real
   <td>/<th> is pure structural boilerplate -- the "bulletproof
   layout" wrapper real marketing HTML uses to box its entire body for
   cross-client compatibility. Confirmed live, National Grid,
   2026-08-16 (wdc's own hand-edited render2_test.ez experiment): the
   WHOLE visible email was wrapped this way, collapsing everything
   into ONE lset leaf at the very top of the document -- a single
   ~2000+ pixel-tall embedded view sitting behind just one of the top
   document's ~68 character positions. That single-character/huge-
   pixel-height disproportion breaks several things downstream that
   assume character count is roughly proportional to vertical space
   (confirmed by reading the source, not just observed): the scrollbar
   elevator's own size (textv.c's getinfo(), total->end = text length
   << FINESCROLL -- no pixel term at all) and reachability via
   MoveForward/^V at narrower window widths (narrower width means more
   text-wrapping inside that one view, making the single already-huge
   line even taller). This renderer has no border/background support
   for tables anyway (see Gate 2's judgment-call log), so a 1x1
   wrapper carries no visual intent worth preserving. Fix: treat it as
   fully transparent -- push its cell's children onto the SAME
   iterative walk stack used for everything else, so whatever's really
   inside (routinely itself a real multi-row table) inserts its own
   rows directly into the CURRENT text's own line flow instead of
   collapsing into one more monolithic wrapper. Applied uniformly
   wherever a <table> tag is encountered by the main walk below (top
   level or nested inside a cell, both go through the same dispatch),
   so chains of nested 1x1 wrappers collapse all the way down, not
   just one level. This is a different, complementary check from
   BuildLsetCell's own NodeIsSoleNestedTable below (which asks "does
   THIS CELL's sole content, once built, resolve to exactly one row" from
   the cell's perspective, coalescing-aware) -- this one asks "is the
   TABLE ELEMENT itself a 1x1 no-op" from the table's own perspective,
   regardless of what's inside or how many rows survive coalescing. */
static int TableIsTrivialWrapper(const struct htmlnode *tablenode, const struct htmlnode **outCellChildren)
{
    struct nodevec rows;
    const struct htmlnode *tr;
    const struct htmlnode *td;
    const struct htmlnode *foundCell = NULL;
    int result = 0;

    CollectRows(tablenode, &rows);
    if (rows.count == 1) {
        tr = rows.items[0];
        for (td = tr->children; td; td = td->next) {
            if (!htmlpart_IsElement(td)) continue;
            if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
            if (foundCell) { foundCell = NULL; break; } /* more than one real cell */
            foundCell = td;
        }
        if (foundCell) {
            *outCellChildren = foundCell->children;
            result = 1;
        }
    }
    nodevec_free(&rows);
    return result;
}

/* A cell with no children at all, or whose only children are
   whitespace-only text nodes, is genuinely empty -- the only case
   worth a fast path (see this file's BuildLsetCell, below, and
   htmlatk.h's judgment-call note on why every other cell, however
   simple, goes through the general recursive path instead of a
   scalar/dispatch shortcut). Uses the same is_collapsible_space test
   EmitTextRun uses for ordinary body text, so "a cell containing only
   a run of spaces/tabs/newlines (or a literal &nbsp;)" is treated the
   same way here as it would be anywhere else in this renderer. */
/* Recurses through wrapper elements (<div>, <span>, and the like) to
   decide whether a node contributes any real visible content -- NOT
   just a shallow direct-children check, which would wrongly call a
   cell "non-empty" just because it contains an empty <div> (real mail
   uses exactly this shape for "hidden preheader" anti-clipping
   padding, e.g. <div style="color:#ffffff">&zwnj; &zwnj; ...</div> --
   confirmed live, National Grid, 2026-08-16: after the &zwnj; fix
   above, that div's own text is nothing but collapsible whitespace,
   but a direct-children-only check never looks inside the div to see
   that). <img> is always "real content" regardless of what's inside
   it -- an unresolvable image still gets a placeholder, so there's
   nothing to look inside. <table> used to get the same unconditional
   treatment ("an empty-looking table can still have visible
   borders"), but that reasoning doesn't hold for THIS renderer: it
   has no border/background rendering at all (see html-mail-rendering-
   design.md's "Known imperfections" note), so a table whose every
   real cell is itself visually empty genuinely has nothing to show --
   treating it as unconditionally non-empty only meant a `<tr>` whose
   sole content is a spacer `<table>` (an extremely common real-world
   shape: `<td><table><tr><td>&nbsp;</td></tr></table></td>`) could
   never be recognized as blank for RowIsEntirelyBlank's own
   consecutive-blank-row coalescing below, unlike a plain
   `<td>&nbsp;</td>` with no nested table, which always could. This
   went unnoticed for a while because it was rarely consequential when
   whole wrapper tables collapsed into one opaque embedded view anyway
   (see the Peeling section in html-mail-rendering-design.md) --
   peeling those wrappers away turns each spacer-table-wrapped row into
   its own independent top-level line, making this gap directly
   visible as extra whitespace. Mutually recursive with CellIsEmpty
   below (forward-declared here) -- plain C recursion, not an explicit
   heap stack: this only ever descends through incidental wrapper
   markup and genuinely trivial nested tables, the same "bounded by
   markup, not document size" reasoning already used for table-nesting
   recursion elsewhere in this file (see htmlatk.h's own note on why
   that's safe). */
static int CellIsEmpty(const struct htmlnode *cellnode);
static int NodeIsVisuallyEmpty(const struct htmlnode *n)
{
    const struct htmlnode *c;
    if (htmlpart_IsText(n)) {
        long i;
        for (i = 0; i < n->textlen; ++i) {
            if (!is_collapsible_space((unsigned char) n->text[i])) return 0;
        }
        return 1;
    }
    if (strcmp(n->tag, "img") == 0) return 0;
    if (strcmp(n->tag, "table") == 0) {
        struct nodevec rows;
        long r;
        int empty = 1;
        CollectRows(n, &rows);
        for (r = 0; r < rows.count && empty; ++r) {
            const struct htmlnode *tr = rows.items[r];
            const struct htmlnode *td;
            for (td = tr->children; td; td = td->next) {
                if (!htmlpart_IsElement(td)) continue;
                if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
                if (!CellIsEmpty(td)) { empty = 0; break; }
            }
        }
        nodevec_free(&rows);
        return empty;
    }
    for (c = n->children; c; c = c->next) {
        if (!NodeIsVisuallyEmpty(c)) return 0;
    }
    return 1;
}

static int CellIsEmpty(const struct htmlnode *cellnode)
{
    const struct htmlnode *c;
    for (c = cellnode->children; c; c = c->next) {
        if (!NodeIsVisuallyEmpty(c)) return 0;
    }
    return 1;
}

/* ==================================================================== *
 * lset-based table construction (BuildLsetGrid/RenderTableAsLset). This
 * is now the ONLY table representation this module produces -- the
 * earlier table/spread path (and the hybrid routing rule that sent
 * colspan/rowspan tables there) is gone as of 2026-08-16. See
 * htmlatk.h's judgment-call log for the full design discussion: each
 * lset row builds its own independent binary-split tree (unlike
 * table/spread's single shared grid), so a colspan cell doesn't need
 * "merging" or a shared grid at all -- it just needs to be a wider-
 * WEIGHTED leaf in its own row's split chain (LsetChainPctWeighted,
 * BuildLsetChain below), with every row's weights expressed as
 * fractions of the same table-wide column count (TableColumnCount) so
 * cells in different rows still align visually, the way a real
 * `Date | Description(colspan=3) | Status` header would need to.
 * Reuses CollectRows/CellIsEmpty/htmlatk_Render, the same shared
 * pieces the old table/spread path used, rather than a second
 * parallel tree-walk. Rowspan is NOT given any special vertical
 * handling -- lset rows are independent lines in the surrounding
 * text's own flow with no shared vertical coordinate system, so a
 * cell "reaching down" into a later row has no natural
 * representation here; a rowspan cell's content renders fully within
 * its own starting row instead (accepted degradation -- the real
 * fixture corpus has zero rowspan occurrences, see Gate 1's report). *
 * ==================================================================== */

/* Small growable vector of struct lset* -- same shape as this file's
   struct nodevec above (htmlnode*), duplicated rather than templated
   because this codebase has no generic container mechanism and every
   other growable buffer in this file (nodevec, hax_mark stacks) is its
   own small dedicated type; adding a void*-casting generic here would
   be less readable, not more, for a two-call-site helper. */
struct lsetvec { struct lset **items; long count, cap; };

static void lsetvec_init(struct lsetvec *v) { v->items = NULL; v->count = 0; v->cap = 0; }

static void lsetvec_push(struct lsetvec *v, struct lset *n)
{
    if (v->count >= v->cap) {
        long ncap = v->cap ? v->cap * 2 : 8;
        struct lset **ni = (struct lset **) realloc(v->items, ncap * sizeof(*ni));
        if (!ni) return;
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->count++] = n;
}

static void lsetvec_free(struct lsetvec *v) { free(v->items); v->items = NULL; v->count = v->cap = 0; }

static int BuildLsetGrid(const struct htmlnode *tablenode, struct hax_state *st, struct lsetvec *outRows);
static int NodeIsSoleNestedTable(const struct htmlnode *td, const struct htmlnode **outTable);

/* Small paired vector of (leaf, weight) for one row's cells --
   analogous to lsetvec above but carrying each cell's WEIGHT alongside
   it (a plain cell's weight is 1; a colspan="N" cell's weight is N),
   since BuildLsetChain below needs both to compute proportional
   splits. Kept separate from lsetvec (used for rows, which never need
   a weight) rather than adding an unused field there for the row
   case. fixedpx (0 if unset -- see CellFixedPixelWidth) is a THIRD,
   independent per-cell attribute: when set, BuildLsetChain gives this
   leaf an absolute pixel width via lset's new Fixed split types
   instead of a proportional share, and weight is simply ignored for
   that leaf (still set to its normal colspan-derived value regardless,
   for uniformity -- callers other than BuildLsetChain, if any were
   ever added, shouldn't have to know about this special case). */
struct wleaf { struct lset *leaf; long weight; long fixedpx; };
struct wlvec { struct wleaf *items; long count, cap; };

static void wlvec_init(struct wlvec *v) { v->items = NULL; v->count = 0; v->cap = 0; }

static void wlvec_push(struct wlvec *v, struct lset *leaf, long weight, long fixedpx)
{
    if (v->count >= v->cap) {
        long ncap = v->cap ? v->cap * 2 : 8;
        struct wleaf *ni = (struct wleaf *) realloc(v->items, ncap * sizeof(*ni));
        if (!ni) return;
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->count].leaf = leaf;
    v->items[v->count].weight = weight;
    v->items[v->count].fixedpx = fixedpx;
    ++v->count;
}

static void wlvec_free(struct wlvec *v) { free(v->items); v->items = NULL; v->count = v->cap = 0; }

/* The table-wide column count a colspan cell's weight is expressed
   against, so cells in DIFFERENT rows still align visually even
   though each row builds its own fully independent lset split tree
   (see this section's header comment) -- e.g. a `Date | Description
   (colspan=3) | Status` header row and a `Jan1 | ItemA | ItemB |
   ItemC | Shipped` data row both need their cells sized as fractions
   of the same total (5, here), not of their own row's local cell
   count (3 vs. 5), or Description's 3/5 share wouldn't line up with
   ItemA+ItemB+ItemC's combined 3/5. Deliberately simpler than the old
   BuildTableGrid's own Pass A: no rowspan carry-over bookkeeping,
   since rowspan gets no cross-row vertical handling at all here (see
   this section's header comment) and so cannot affect which column a
   later row's cells logically start at the way it would in a real
   shared-grid model. Just the max, over all rows, of that row's own
   colspan values summed -- a plain <td> with no colspan attribute
   counts as 1 (ParsePositiveInt's own default). */
static long TableColumnCount(const struct htmlnode *tablenode)
{
    struct nodevec rows;
    long r, ncols = 0;

    CollectRows(tablenode, &rows);
    for (r = 0; r < rows.count; ++r) {
        const struct htmlnode *tr = rows.items[r];
        const struct htmlnode *td;
        long rowsum = 0;
        for (td = tr->children; td; td = td->next) {
            if (!htmlpart_IsElement(td)) continue;
            if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
            rowsum += ParsePositiveInt(htmlpart_GetAttr(td, "colspan"), 1, 1000);
        }
        if (rowsum > ncols) ncols = rowsum;
    }
    nodevec_free(&rows);
    return ncols;
}

/* The percentage assigned to one binary lset split's RIGHT child,
   given that this split's own total weight (summed over every leaf
   still inside it, left and right together) is `remainingWeight`, and
   the leaf being peeled off as this split's LEFT child has weight
   `thisWeight` (a plain cell's weight is 1; a colspan="N" cell's
   weight is N -- see TableColumnCount above for why weights, not
   counts, are what alignment across rows actually needs).

   Traced (not assumed) from real ATK source: lsetv.c's initkids()
   calls lsetview_HSplit/VSplit(self, v1, v2, ls->pct, TRUE) with
   v1==view-of-ls->left, v2==view-of-ls->right (lsetv.c ~line 143-150).
   HSplit/VSplit (lpair.c:543-570) both funnel into
   lpair__SetUp(..., pct, lpair_PERCENTAGE, ...), whose PERCENTAGE
   branch (lpair.c:498-510) sets objsize[1] = pct (the SECOND arg's
   slot, i.e. l2 == ls->right) and leaves objsize[0] (l1 == ls->left)
   to be whatever ComputeSizesFromTotal derives as "the rest"
   (lpair.c:373-398: objcvt[1] = objsize[1]*totalsize/100; objcvt[1-i]
   = totalsize - objcvt[i], i.e. the complement). So: ls->pct is the
   percentage of *this split's own* available space given to
   ls->right, and ls->left gets the complement (100-pct)% -- not a
   50/50-agnostic value the way the Gate 1 probe's symmetric 2-leaf
   case left ambiguous.

   Given that, pct = 100 * (remainingWeight - thisWeight) / remainingWeight
   -- the fraction of this split's own weight NOT accounted for by the
   leaf being peeled off, i.e. what's left for the right subtree. This
   is a direct generalization of the original equal-weight formula
   (100*(remaining-1)/remaining): when every leaf has weight 1,
   remainingWeight equals the plain leaf-count and thisWeight is
   always 1, reducing to exactly that formula (checked against the old
   N=3/4/5 worked arithmetic in htmlatk.h's judgment-call log -- this
   produces identical results for the equal-weight case, byte for
   byte). Verified by hand for the Date/Description/Status example
   above: weights [1,3,1] over remainingWeight=5 resolve to Date=20%,
   Description=60% (3/5), Status=20% of the row -- and a sibling row
   with weights [1,1,1,1,1] (also remainingWeight=5) gives each of its
   5 cells 20%, so the 3 cells "under" Description sum to exactly 60%,
   matching it. Rounded to the nearest integer percentage point (not
   truncated), same rounding technique as the original formula. */
static int LsetChainPctWeighted(long remainingWeight, long thisWeight)
{
    long rightWeight;
    if (remainingWeight <= 0) return 0;
    rightWeight = remainingWeight - thisWeight;
    return (int) (((2L * rightWeight * 100L) + remainingWeight) / (2L * remainingWeight));
}

/* Builds a right-leaning chain of count-1 binary lset split nodes over
   cells[0..count-1] (already-built lset leaves/subtrees plus their
   weights, one per row or one per cell), splits typed splittype
   (lsetview_MakeHorz for a row's cells, lsetview_MakeVert for a
   table's rows) EXCEPT where an individual cell carries a nonzero
   fixedpx (see CellFixedPixelWidth/struct wleaf), which instead gets
   fixedsplittype (lsetview_MakeHorzFixed/MakeVertFixed -- the sibling
   type to splittype, pct reinterpreted as an absolute pixel bsize) for
   that one split. A fixed cell is always folded in as the LEFT/TOP
   child (see below), matching lpair_TOPFIXED's own convention
   (lpair.c:504-510: objsize[0]=bsize for TOPFIXED) -- and matching
   real markup order in every corpus example seen so far, where
   decoration/spacer cells precede real content.

   A fixed cell's own weight plays NO part in the proportional pool at
   all -- it's excluded from remainingWeight entirely, both in the
   initial seed and the fold loop below, so cells[count-1] being fixed
   doesn't corrupt the running total either. This is correct because
   lpair_ComputeSizesFromTotal (lpair.c:373-398) carves the fixed
   cell's literal pixels off FIRST, at runtime, and only ever hands
   "the rest" on to whatever's nested inside the other child -- there
   is no shared percentage basis for a fixed cell to participate in.
   Concretely, for a real row shaped [D1(fixed 1px), D2(fixed 10px),
   D3(fixed 4px), Content(proportional)]: the fold produces
   [D1|[D2|[D3|Content]]], each fixed split peeling off its own literal
   pixels in turn, with Content finally getting whatever's left after
   all three -- exactly real-browser table-auto-layout behavior for a
   handful of thin border/spacer columns beside open content, not an
   equal-ish 25% share for each the way pure colspan-weighting would
   have given them.

   Iterative (built tail-to-head with a plain for loop), NOT C
   recursion, unlike this file's nested-<table> recursion above (see
   htmlatk.h's own note on why that one is safe): a single pathological
   row could carry an arbitrarily large number of <td>s, so unlike
   table-nesting depth (bounded by markup structure) this chain's
   length is bounded only by document content, the same category of
   concern htmlpart.c/htmltext.c already use an explicit stack for --
   so this does too, just via a simple backwards loop rather than a
   heap-allocated work list, since the shape (fold right-to-left, no
   branching/backtracking) doesn't need one. On an allocation failure
   partway through, returns NULL and records st->hardfail (htmlatk.h's
   Returns-FALSE note); does not free the leaves/subtrees already
   folded in, same best-effort-on-OOM posture as this file's other
   growable buffers. */
static struct lset *BuildLsetChain(struct wleaf *cells, long count, int splittype, int fixedsplittype, struct hax_state *st)
{
    struct lset *rest;
    long i;
    long remainingWeight;

    if (count <= 0) return NULL;
    rest = cells[count - 1].leaf;
    remainingWeight = (cells[count - 1].fixedpx > 0) ? 0 : cells[count - 1].weight;
    for (i = count - 2; i >= 0; --i) {
        struct lset *node = (struct lset *) class_NewObject("lset");
        if (!node) { st->hardfail = 1; return NULL; }
        if (cells[i].fixedpx > 0) {
            node->type = fixedsplittype;
            node->pct = (int) cells[i].fixedpx; /* a pixel bsize here, not a percentage -- see lsetview_MakeHorzFixed's comment in lsetv.ch */
        } else {
            long thisWeight = cells[i].weight;
            remainingWeight += thisWeight;
            node->type = splittype;
            node->pct = LsetChainPctWeighted(remainingWeight, thisWeight);
        }
        node->left = (struct dataobject *) cells[i].leaf;
        node->right = (struct dataobject *) rest;
        rest = node;
    }
    return rest;
}

/* A leaf with real content but no text of its own to hold -- used to
   pad a row out to the table's shared column count (TableColumnCount)
   when that row's own cells' colspans don't sum to it, so alignment
   across rows holds even for a genuinely ragged/shorter row rather
   than letting its real cells silently stretch to fill 100% and drift
   out of alignment with its neighbors. Always attaches a real (empty)
   "text" object rather than leaving dobj==NULL, same reasoning as
   BuildLsetCell's own empty-cell fix below (lsetview__DesiredSize's
   forwarding fix only applies when self->child exists). */
static struct lset *MakeFillerLeaf(struct hax_state *st)
{
    struct lset *leaf = (struct lset *) class_NewObject("lset");
    struct text *ct;
    if (!leaf) { st->hardfail = 1; return NULL; }
    ct = (struct text *) class_NewObject("text");
    if (ct) {
        leaf->dobj = (struct dataobject *) ct;
        strcpy(leaf->viewname, dataobject_ViewName((struct dataobject *) ct));
        strcpy(leaf->dataname, "text");
    } else {
        st->hardfail = 1;
    }
    return leaf;
}

/* Builds one <td>/<th>'s lset leaf: an "lset" object with no left/right
   (a leaf, per Gate 1's finding that dobj/left/right are mutually
   exclusive -- lsetv.c's dolink() branches on "ls->left && ls->right"
   vs. makeview(), never both) and, for any cell (empty or not), a
   fresh "text" dataobject attached via plain field assignment (Gate 1
   section 1.4: leaf->dobj = obj; strcpy(leaf->viewname,
   dataobject_ViewName(obj)); -- no lset_InsertObject call, it only
   constructs a *fresh* empty object by class name, not attach an
   existing one) that this module's own htmlatk_Render() recurses into
   -- "a cell is just another dataobject, rich content included," the
   same real-ATK-precedent design this whole module is grounded in
   (see htmlatk.h's judgment-call log) -- nested <table>s/<img>s/nested
   <table>s-that-themselves-route-to-lset all fall out for free as
   ordinary content of that recursive call.

   CORRECTION, found live 2026-08-16 (wdc: clicking into the large
   blank gaps between blocks highlighted a big black selection zone --
   "as if we missed eliminating an empty text zone"): this originally
   left an empty cell (CellIsEmpty) as a bare leaf with
   dobj==NULL/viewname=="", modeled on table_EmptyCell's zero-overhead
   philosophy -- lsetv.c's makeview() does decline to create a child
   view for an empty viewname (confirmed by reading it: `if (lv &&
   *lv!='\0' ...)`), so self->child stays NULL. That's exactly the
   case lsetview__DesiredSize's own fix (added earlier this session,
   see htmlatk.h's judgment-call log) does NOT cover -- its forwarding
   condition is `self->mode != lsetview_IsSplit && self->child`, and a
   bare empty leaf never has a self->child at all, so it unconditionally
   falls through to lpair's original, still-broken fallback (echo the
   asked-for height, capped at 256px) regardless of that fix. Real
   marketing email is full of near-invisible spacer/padding rows
   (confirmed throughout revival/tests/html-fixtures/), so this gap hit
   constantly and compounded into exactly the reported symptom: large
   blank areas that are still real, clickable, selectable view
   rectangles (an oversized region under a leaf with no real content to
   fill it) -- not literally eliminated the way this comment's original
   claim assumed. table/spread's own empty-cell path is unaffected by
   any of this (spread's row-height computation has never gone through
   lsetview at all); this is lset-path-specific. Fix: always attach a
   real (possibly zero-length) "text" object, even for an empty cell,
   so self->child is never NULL and lsetview__DesiredSize's fix always
   applies -- an empty text view's own natural height is small/correct
   (the same as any other blank line in this renderer, never a
   reported bug), which is exactly the minimal-footprint behavior
   table_EmptyCell's zero-overhead design was trying to approximate a
   different way; this trades a few bytes of always-allocated "text"
   dataobject per empty cell for actually working correctly, a trade
   worth making given lset's structural limitation.

   SECOND CORRECTION, found live 2026-08-16 -- wdc read this exact
   generated datastream by hand (revival/render_test.ez) and asked the
   right question directly: "Is something deciding that to have an
   lset embedded in a text object that it needs to be wrapped in a
   text?" Yes -- this always wraps a cell's content in a fresh "text"
   object and recurses, even when that cell's ENTIRE content is
   nothing but one nested <table> that itself routes to the lset path.
   For the extremely common real-mail case of a <td> that exists
   purely to hold a nested layout table, that produces a real,
   traceable, unnecessary chain: text -> (one embedded view) -> lset
   -> text -> (one embedded view) -> lset -> ... one extra
   text/lsetview level per nesting, for content that has no text of
   its own to hold. Confirmed by hand-tracing render_test.ez's actual
   bytes: the document's first row (a blank spacer) was
   text[outer]->lset[row]->text[cell]->lset[nested-row]->text[empty],
   five objects deep for what is semantically one blank cell. Fixed:
   when a cell's only real child (skipping whitespace-only text) is a
   single <table> (originally gated on "doesn't need the table/spread
   fallback" via TableNeedsGridFallback -- that whole function and the
   fallback it gated are gone as of later the same day, see this
   section's header comment, so this check is now unconditional),
   build that nested table's own rows first; if it resolves to exactly
   one row (the common case after row
   coalescing above), attach that row directly as this leaf's own
   dobj/viewname -- no "text" wrapper at all. A nested table that
   still has multiple rows after coalescing has no single object to
   attach this way (a cell can only hold one dataobject), so it falls
   through to the general text-wrapping path below unchanged, same as
   before this fix. Whether this also explains the persisting reports
   of oversized rendering for deeply-nested chains is NOT yet
   confirmed -- but reducing real, unnecessary nesting depth is a
   correct simplification on its own regardless of that open question. */
static int NodeIsSoleNestedTable(const struct htmlnode *td, const struct htmlnode **outTable)
{
    const struct htmlnode *c;
    const struct htmlnode *found = NULL;
    for (c = td->children; c; c = c->next) {
        if (htmlpart_IsText(c)) {
            long i;
            for (i = 0; i < c->textlen; ++i)
                if (!is_collapsible_space((unsigned char) c->text[i])) return 0;
            continue;
        }
        if (!htmlpart_IsElement(c) || strcmp(c->tag, "table") != 0) return 0;
        if (found) return 0; /* more than one real element child */
        found = c;
    }
    if (!found) return 0;
    *outTable = found;
    return 1;
}

/* Returns a positive pixel width if td should get a FIXED (not
   proportional) width in its row's lset split, else 0.

   ONLY an empty spacer/border <td> (NodeIsVisuallyEmpty) whose own
   width="N" IS the real intended size qualifies -- there's no content
   to expand it past that, so trusting it literally is safe, AND it's
   always small (a border/spacer strip, realistically never more than
   a few tens of pixels), so it can never plausibly exceed a real
   window's available width on its own. Confirmed against a real
   fixture (The Book Rack, 2026-08-17): a <td width="1" bgcolor="..."
   style="font-size:0px"/> border-color strip sitting next to real
   content was previously getting an equal ~25% share alongside three
   other cells in the same row, instead of the ~1px it actually is.

   REJECTED, live-tested and reverted same day: also giving a fixed
   width to a <td> whose only content is a nested <table> with its own
   explicit width="N" (the common "outer <td width=1> wraps a real
   width=640 table" idiom). That's a fundamentally different case --
   the value can be large (a full content column, easily 600px+) --
   and lpair's TOPFIXED sizeform (lsetview_MakeHorzFixed) has no
   graceful-degrade behavior: lpair_ComputeSizesFromTotal's
   objcvt[0]=min(totalsize,bsize) correctly caps the fixed side when
   the window is narrower than requested, but objcvt[1] (whatever
   comes after it in that row) gets totalsize-objcvt[0], which can be
   exactly 0 -- and a zero-width rectangle is treated as empty and
   skipped from view_FullUpdate entirely, along with everything nested
   inside it. Confirmed live via lpair.c/lsetv.c instrumentation
   (revival/render_test.ez in a ~500px-wide ez window against this
   fixture's real 640px content column): DoFullUpdate showed
   objsize[0]=640 objcvt[0]=407 objcvt[1]=0, rightBottomObject
   rect(w=-1,h=-1) empty=1 -- exactly the "three bars then nothing"
   symptom wdc reported live, in both messages and ez. A real window
   narrower than a newsletter's declared content width is completely
   ordinary, not an edge case. Leaving this case on its EXISTING
   proportional/colspan-weighted default (unchanged from before this
   whole feature) instead achieves the same practical outcome when
   there's room -- a lone weight-1 cell with nothing else of substance
   in its row still gets ~100% of whatever's available -- but degrades
   gracefully (shrinks to fit) instead of an all-or-nothing collapse
   when there isn't.

   Percentage widths are not handled here (see ParseFixedPixelWidth) --
   resolving one needs a container width not available at construction
   time. A text-bearing cell with no explicit sizing signal at all
   keeps the existing equal/colspan-weighted default; guessing an
   intrinsic content width is out of scope, same as it always was. */
static long CellFixedPixelWidth(const struct htmlnode *td)
{
    long w;

    if (NodeIsVisuallyEmpty(td)) {
        w = ParseFixedPixelWidth(htmlpart_GetAttr(td, "width"));
        if (w > 0) return w;
    }
    return 0;
}

/* Builds one <td>/<th>'s lset leaf and reports its WEIGHT (its own
   colspan value, default 1 -- see TableColumnCount's comment above for
   why weight, not a boolean/count, is what cross-row alignment needs)
   via *outWeight, and its FIXED PIXEL width (see CellFixedPixelWidth
   above), 0 if none, via *outFixedPx -- both set on every path through
   this function regardless of which branch actually builds the leaf.
   The two are independent/orthogonal: *outWeight still reflects this
   cell's real colspan even when *outFixedPx is also set, since
   TableColumnCount/cross-row alignment (colspan-based) and
   BuildLsetChain's fixed-vs-proportional split choice are unrelated
   concerns -- see BuildLsetChain's own comment. */
static struct lset *BuildLsetCell(const struct htmlnode *td, struct hax_state *st, long *outWeight, long *outFixedPx)
{
    struct lset *leaf = (struct lset *) class_NewObject("lset");
    const struct htmlnode *soleTable;
    struct text *ct;
    *outWeight = ParsePositiveInt(htmlpart_GetAttr(td, "colspan"), 1, 1000);
    *outFixedPx = CellFixedPixelWidth(td);
    if (!leaf) { st->hardfail = 1; return NULL; }

    if (NodeIsSoleNestedTable(td, &soleTable)) {
        struct lsetvec innerRows;
        int savedHardfail = st->hardfail;
        if (BuildLsetGrid(soleTable, st, &innerRows) && innerRows.count == 1
            && innerRows.items[0]->left == NULL) {
            /* inner is already a complete lset LEAF (not a split) for
               the nested table's one row -- i.e. that row itself has
               at most one real cell, so there's no side-by-side
               content of its own being hidden by inlining it. Use it
               AS this cell's leaf directly instead of wrapping it in
               the shell we speculatively allocated above (leaf->dobj
               = inner would produce a pointless lset-wrapping-an-lset
               pair for every single-cell nested table, and this
               spacer-table idiom is extremely common in real
               marketing HTML -- wdc caught this live in
               render_test.ez, 2026-08-16: the first two objects in
               the National Grid fixture were exactly this redundant
               pair around one empty spacer cell). *outWeight and
               *outFixedPx were already set above (from td's own
               colspan, and CellFixedPixelWidth -- which itself checks
               this same soleTable's own width= for exactly this case,
               see its comment) and are unaffected by which lset object
               we return.

               The innerRows.items[0]->left==NULL guard was added
               2026-08-18, after this original count==1-only version
               was found live-collapsing Book Rack's ENTIRE message
               into ONE embedded view (VIEW-AT count: 1 in
               htmlatktest.test dump) with 87 chained MakeHorz splits
               and zero MakeVert -- i.e. every section of the
               newsletter, each reached via exactly this idiom (a
               single-row nested table whose one row is itself a real
               decoration+content split, not a plain leaf), kept
               getting folded in as "just this cell's own content"
               instead of becoming its own separate row-view in
               RenderTableAsLset's per-line text flow (BuildLsetGrid's
               own header comment above explains why that per-line
               flow, not a second lset/lpair stacking layer, is how
               this renderer represents multiple rows). Since
               lpair__DesiredSize's side-by-side (VERTICAL) branch
               reports max(d0,d1) for its free dimension, chaining 87
               unrelated sections together this way made the whole
               document's reported height collapse to its single
               tallest link -- <space> paging then thought the first
               screenful was the entire message. A genuinely trivial
               single-cell wrapper (this guard's true case) has
               nothing of its own to lose by inlining; a single ROW
               that's itself a real multi-cell split does, and now
               falls through below to the ordinary path instead,
               which re-walks it via htmlatk_Render/RenderTableAsLset
               and gets its own row-view(s) in the flow like any other
               table. */
            struct lset *inner = innerRows.items[0];
            lsetvec_free(&innerRows);
            dataobject_Destroy((struct dataobject *) leaf);
            return inner;
        }
        /* More than one row, or a single row that's itself a real
           split (see the left==NULL guard above), or the attempt
           failed outright -- no single leaf to attach directly; undo
           any failure flag from this abandoned attempt (BuildLsetGrid
           only sets it on a genuine "no rows at all" case) and fall
           through to the general path below, which re-walks and
           rebuilds this same <table> the ordinary way via
           htmlatk_Render's own tag dispatch. */
        lsetvec_free(&innerRows);
        st->hardfail = savedHardfail;
    }

    ct = (struct text *) class_NewObject("text");
    if (ct) {
        if (!CellIsEmpty(td)) {
            long celllen = 0;
            if (!htmlatk_Render(ct, 0, td->children, st->resolver, st->resolverRock, &celllen))
                st->hardfail = 1;
        }
        leaf->dobj = (struct dataobject *) ct;
        strcpy(leaf->viewname, dataobject_ViewName((struct dataobject *) ct));
        strcpy(leaf->dataname, "text");
    } else {
        /* class_NewObject("text") failed -- a real ATK object-
           allocation failure (htmlatk.h's Returns-FALSE contract);
           leave this one leaf blank (degrade, not crash) rather than
           aborting the whole table over one cell. */
        st->hardfail = 1;
    }
    return leaf;
}

/* Fills a fresh root lset tree from tablenode's row/cell structure:
   one lset per real row, each row an lsetview_MakeHorz chain of that
   row's own cells (colspan cells get a proportionally larger weight
   in that chain -- see TableColumnCount/LsetChainPctWeighted above).
   Reuses CollectRows for row collection; cells within a row are the
   row node's own direct <td>/<th> children only -- HTML rows never
   nest their cells inside a transparent wrapper the way
   <tbody>/<thead> wrap rows.

   Rows are NOT stacked via a second lset/lpair layer (an earlier
   version of this function did that and it was wrong -- see
   htmlatk.h's judgment-call log for the full root-cause writeup:
   lpair__DesiredSize's row-stacking branch, src/atk/supportviews/
   lpair.c:354-364, never sums its children's real heights, only
   echoes/caps whatever height it was given, which is fine for lset's
   original fixed-window-pane use case but wrong for a content-driven
   inset in flowing text). Instead RenderTableAsLset below inserts
   each row as its own view directly into the surrounding text's own
   line flow -- the same per-line auto-height stacking this renderer
   already uses for every other block. A row with zero real cells
   (e.g. a stray bare <tr></tr>) is dropped from the result entirely
   rather than inserted as a placeholder. Returns FALSE (leaving
   *outRows empty) only if the table has no real rows anywhere. */
/* A row counts as "blank" for coalescing purposes if it has at least
   one real <td>/<th> cell and every one of them is CellIsEmpty -- a
   row with NO real cells at all isn't "blank content" in the same
   sense, it's just dropped by the main build loop below regardless
   (cells.count==0 never gets pushed to outRows), so it doesn't need
   coalescing logic of its own. */
static int RowIsEntirelyBlank(const struct htmlnode *tr)
{
    const struct htmlnode *td;
    int any = 0;
    for (td = tr->children; td; td = td->next) {
        if (!htmlpart_IsElement(td)) continue;
        if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
        any = 1;
        if (!CellIsEmpty(td)) return 0;
    }
    return any;
}

/* Real marketing/newsletter HTML routinely uses all-<td>-empty spacer
   <tr>s purely for vertical padding -- sometimes stacked (several
   consecutive blank rows), sometimes one isolated blank row between
   every real content row (both confirmed live in the National Grid
   fixture: a run of blanks between two footer blocks, AND -- found
   later the same day via wdc's own mouseover boundary-hunting in the
   rendered ez view, then confirmed structurally via `htmlatktest.test
   roundtrip` showing LSET-AT[9]/[13]/[17]/... as genuinely empty
   CELL-TEXT-CONTENT len=0 -- a single blank <tr> alternating with
   every one of the ~9 real content rows in the fixture's main body).
   Every blank row is dropped unconditionally, isolated or not: each
   one becomes its own full text-line-height view in the surrounding
   flow per RenderTableAsLset below (this renderer has no way to
   represent a source spacer's actual intended height -- an 8px CSS
   spacer and a full blank text line both come out the same size here),
   so keeping even one isolated blank row between every real row means
   doubling the row count and, empirically, most of the "too much
   whitespace" complaint. An earlier version of this function only
   coalesced runs of 2+ consecutive blanks down to one, on the theory
   that an isolated blank row was "ordinary, real, intentional
   spacing" worth preserving -- that theory doesn't survive contact
   with a template that alternates content/spacer on every row. */
static int BuildLsetGrid(const struct htmlnode *tablenode, struct hax_state *st, struct lsetvec *outRows)
{
    struct nodevec rows;
    long r, ncols;

    lsetvec_init(outRows);
    CollectRows(tablenode, &rows);
    ncols = TableColumnCount(tablenode);

    for (r = 0; r < rows.count; ++r) {
        const struct htmlnode *tr = rows.items[r];
        const struct htmlnode *td;
        struct wlvec cells;
        struct lset *rowRoot;
        long rowWeight = 0;

        /* Blank-row dropping has to run BEFORE the sole-nested-table
           splice check below, not after -- NodeIsVisuallyEmpty now
           looks inside a nested <table> to judge real blankness (see
           its own comment), so RowIsEntirelyBlank correctly recognizes
           a <tr> whose sole cell wraps nothing but a blank spacer
           table. Checking splice-eligibility first (an earlier version
           of this code did) meant a genuinely blank spacer-table row
           always got recursed into and spliced in as its own row
           regardless. Every blank row is dropped, not just runs of 2+
           (see RowIsEntirelyBlank's own comment on why isolated blanks
           turned out not to be safe to keep). */
        if (RowIsEntirelyBlank(tr)) {
            continue;
        }

        {
            /* A row with exactly one real cell whose sole content is
               itself a <table> gets that inner table's OWN rows
               spliced in directly, each already independently
               built+padded to THAT inner table's own column count --
               not this (outer) table's ncols. Splicing raw <tr> nodes
               and padding them all against one shared ncols (an
               earlier version of this fix, 2026-08-16) was wrong: it
               smeared together column counts for what are semantically
               separate, unrelated table layouts. Confirmed live,
               National Grid, 2026-08-16 (wdc's own screenshot): a
               genuine 2-column footer row (logo + social icons)
               elsewhere in the SAME outer wrapper pushed the whole
               table's ncols to 2, so every other, genuinely
               single-column spliced-in row got padded with a spurious
               empty 50% filler -- visually squishing real
               single-column content into the left half of the window.
               Recursing here (BuildLsetGrid calling itself on
               soleTable) keeps each nested table's column alignment
               fully self-contained. */
            const struct htmlnode *soleCell = NULL;
            const struct htmlnode *soleTable;
            int soleCellSeen = 0;
            for (td = tr->children; td; td = td->next) {
                if (!htmlpart_IsElement(td)) continue;
                if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
                if (soleCellSeen) { soleCell = NULL; break; } /* more than one real cell */
                soleCell = td;
                soleCellSeen = 1;
            }
            if (soleCell && NodeIsSoleNestedTable(soleCell, &soleTable)) {
                struct lsetvec innerRows;
                long j;
                if (BuildLsetGrid(soleTable, st, &innerRows)) {
                    for (j = 0; j < innerRows.count; ++j) lsetvec_push(outRows, innerRows.items[j]);
                }
                lsetvec_free(&innerRows);
                continue;
            }
        }

        wlvec_init(&cells);
        for (td = tr->children; td; td = td->next) {
            struct lset *leaf;
            long weight, fixedpx;
            if (!htmlpart_IsElement(td)) continue;
            if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
            leaf = BuildLsetCell(td, st, &weight, &fixedpx);
            if (leaf) { wlvec_push(&cells, leaf, weight, fixedpx); rowWeight += weight; }
        }
        if (cells.count > 0) {
            /* Pad out to the table's shared column count so this
               row's cells stay aligned with sibling rows that have a
               different local cell count (see TableColumnCount's own
               comment) -- e.g. a colspan-free data row under a
               colspan header, or a genuinely ragged row. Filler is
               always proportional (fixedpx=0) -- it exists purely for
               colspan cross-row alignment, unrelated to any real
               cell's fixed-pixel sizing. */
            if (rowWeight < ncols) {
                struct lset *filler = MakeFillerLeaf(st);
                if (filler) wlvec_push(&cells, filler, ncols - rowWeight, 0);
            }
            rowRoot = BuildLsetChain(cells.items, cells.count, lsetview_MakeHorz, lsetview_MakeHorzFixed, st);
            if (rowRoot) lsetvec_push(outRows, rowRoot);
        }
        wlvec_free(&cells);
    }
    nodevec_free(&rows);

    if (outRows->count == 0) {
        /* A structurally empty/malformed table (no real rows anywhere)
           is not a FAILURE -- there's simply nothing to render, the
           same graceful degrade the old table/spread path had (grow
           to a minimal empty grid, still return TRUE). Does NOT set
           st->hardfail: this branch fires often now that there's no
           more fallback path to catch it first -- both genuinely
           empty <table>s in real mail (decorative/spacer markup) and
           discarded "peek" attempts from BuildLsetCell's sole-nested-
           table shortcut (a real, common, harmless case for a nested
           table that isn't a single row) hit this every time. Marking
           the whole render failed for either would be wildly
           disproportionate -- RenderHtmlPart falls back the ENTIRE
           message to plain text on a hardfail (see text822.c), and
           this needs to stay reserved for genuine construction
           failures (allocation failures, which still propagate via
           their own explicit st->hardfail=1 sites elsewhere in this
           file), not "this one table/peek had nothing in it." */
        return FALSE;
    }
    return TRUE;
}

/* Top-level entry: builds one lset per row (cells only, see
   BuildLsetGrid's note above for why rows are no longer stacked via a
   second lset/lpair layer) and inserts each as its own inline view at
   successive positions in the walker's text flow, one per line -- the
   same text_AlwaysAddView call this module uses for any single
   embedded view (lset does not override ViewName, falls through to
   the generic "lset"+"view"="lsetview" algorithm, Gate 1 section 1.6),
   just called once per row instead of once for the whole table.
   EnsureLineBreak between rows forces each row onto its own line the
   same way it does for any other line-level content in this file --
   without it, two back-to-back embedded views with no break between
   them could end up flowed onto the same line if they're narrow
   enough, which a row of an HTML table should never do. HTML
   width=/border= attributes are deliberately NOT consulted anywhere
   in this section -- lset has no border/gridline concept for a
   border= attribute to control, and per-column width hints have no
   real signal in the fixture corpus to act on (see htmlatk.h's
   judgment-call log); column proportions come entirely from
   colspan-derived weights (TableColumnCount/LsetChainPctWeighted
   above) instead. */
static int RenderTableAsLset(struct hax_state *st, const struct htmlnode *tablenode)
{
    struct lsetvec rows;
    long i;
    int ok = BuildLsetGrid(tablenode, st, &rows);
    /* !ok means BuildLsetGrid found no real rows -- a benign, common
       case (a genuinely empty/decorative <table>), not a failure; see
       BuildLsetGrid's own comment on why this must NOT set
       st->hardfail. Just render nothing for this table and move on. */
    if (!ok) { lsetvec_free(&rows); return FALSE; }
    FlushPendingSpace(st); /* AddView doesn't collapse into text runs like InsertLiteral does */
    for (i = 0; i < rows.count; ++i) {
        struct lset *rowRoot = rows.items[i];
        if (i > 0) EnsureLineBreak(st);
        text_AlwaysAddView(st->dest, st->pos, dataobject_ViewName((struct dataobject *) rowRoot), (struct dataobject *) rowRoot);
        ++st->pos;
        /* EnsureLineBreak's own guard (trailingNL==0) has to see this
           reset on every iteration, not just once after the loop --
           the view character just inserted isn't a text newline, so
           whatever trailingNL was before this row (e.g. 2, carried
           over from a paragraph break before the table) is stale and
           must not survive into the next iteration's EnsureLineBreak
           call. Missing this the first time (only setting it once,
           after the whole loop) caused a real, confirmed bug: two
           row-views landed on back-to-back text positions with
           *nothing* between them (RUN[65,66)/RUN[66,67) directly
           adjacent, no newline run at all -- found by reading the raw
           dump output after wdc reported the fixed version showed no
           table content at all past the first line of body text). */
        st->trailingNL = 0;
        st->anyContent = 1;
    }
    lsetvec_free(&rows);
    st->spacePending = 0;
    return !st->hardfail;
}

/* ==================================================================== *
 * Main iterative walk (mirrors htmltext.c's structure -- see this
 * file's header comment).
 * ==================================================================== */

struct hax_walkitem { const struct htmlnode *node; int post; int marksOpened; };
struct hax_walkstack { struct hax_walkitem *items; long count, cap; };

static void ws_push(struct hax_walkstack *ws, const struct htmlnode *n, int post, int marksOpened)
{
    if (ws->count >= ws->cap) {
        long ncap = ws->cap ? ws->cap * 2 : 256;
        struct hax_walkitem *ni = (struct hax_walkitem *) realloc(ws->items, ncap * sizeof(struct hax_walkitem));
        if (!ni) return;
        ws->items = ni;
        ws->cap = ncap;
    }
    ws->items[ws->count].node = n;
    ws->items[ws->count].post = post;
    ws->items[ws->count].marksOpened = marksOpened;
    ++ws->count;
}

static void ws_push_siblings(struct hax_walkstack *ws, const struct htmlnode *first)
{
    struct nodevec tmp;
    long i;
    const struct htmlnode *s;

    nodevec_init(&tmp);
    for (s = first; s; s = s->next) nodevec_push(&tmp, s);
    for (i = tmp.count - 1; i >= 0; --i) ws_push(ws, tmp.items[i], 0, 0);
    nodevec_free(&tmp);
}

static int tag_is_para(const char *t)
{
    return strcmp(t, "p") == 0 || strcmp(t, "div") == 0 || strcmp(t, "blockquote") == 0
        || strcmp(t, "h1") == 0 || strcmp(t, "h2") == 0 || strcmp(t, "h3") == 0
        || strcmp(t, "h4") == 0 || strcmp(t, "h5") == 0 || strcmp(t, "h6") == 0
        || strcmp(t, "dl") == 0;
}

static int tag_is_suppressed(const char *t)
{
    return strcmp(t, "head") == 0 || strcmp(t, "title") == 0;
}

/* `display:none`/`visibility:hidden` (added to htmlpart.c's style
   property allowlist alongside this check, 2026-08-16): an
   industry-standard trick in commercial marketing email is a `<div
   style="display:none">short summary text</div>` right at the top of
   the body -- the "preheader" -- read only by the email client's
   inbox-list preview line, never meant to be visible in the opened
   message. Without this check that div rendered as ordinary visible
   body text (confirmed live, National Grid fixture, 2026-08-16: "We
   have helpful resources to help manage energy costs" at the very top
   of the rendered body, with no visible counterpart anywhere else in
   the source -- found while chasing a separate "too much whitespace"
   report, worth more on its own than the whitespace investigation that
   led to it). Checked once, right alongside tag_is_suppressed, so a
   hidden node's children are never even pushed onto the walk stack --
   same "skip structurally, don't half-render" treatment as any other
   suppressed content. */
static int NodeIsStyleHidden(const struct htmlnode *n)
{
    char *sv;
    int hidden = 0;

    sv = htmlpart_GetStyleProp(n, "display");
    if (sv) { if (value_contains_ci(sv, "none")) hidden = 1; free(sv); }
    if (!hidden) {
        sv = htmlpart_GetStyleProp(n, "visibility");
        if (sv) { if (value_contains_ci(sv, "hidden")) hidden = 1; free(sv); }
    }
    return hidden;
}

/* Pushes style marks for a node's tag-implied and style=-implied
   formatting; returns how many marks were pushed (for the matching
   POST to pop/finalize). Does not handle <a> (needs the href value
   captured separately, see the main loop) or <table>/<img> (handled
   entirely at PRE with no children walked, see the main loop). */
static int PushFormattingMarks(struct hax_state *st, const struct htmlnode *n)
{
    int count = 0;
    const char *t = n->tag;
    char *sv;

    /* A still-deferred collapsed space (see EmitTextRun/MaybeSpace)
       belongs to whatever content precedes this tag, not to the mark
       about to be opened -- flush it now so marks_push's recorded
       start position doesn't include it (it would otherwise land
       inside this tag's own children's first inserted text and get
       misattributed as this run's own leading character). */
    FlushPendingSpace(st);

    if (strcmp(t, "b") == 0 || strcmp(t, "strong") == 0) { count += marks_push(st, BoldSt); }
    if (strcmp(t, "i") == 0 || strcmp(t, "em") == 0) { count += marks_push(st, ItalicSt); }
    if (strcmp(t, "u") == 0) { count += marks_push(st, UnderlineSt); }
    if (strcmp(t, "tt") == 0 || strcmp(t, "code") == 0) { count += marks_push(st, FixedSt); }

    if (strcmp(t, "font") == 0) {
        const char *color = htmlpart_GetAttr(n, "color");
        const char *size = htmlpart_GetAttr(n, "size");
        if (color && *color) { count += marks_push(st, AttrColorStyleFor("color", color)); }
        if (size && *size) { count += marks_push(st, FontSizeStyleFor(ParsePositiveInt(size, 3, 7))); }
    }

    sv = htmlpart_GetStyleProp(n, "color");
    if (sv) { count += marks_push(st, AttrColorStyleFor("color", sv)); free(sv); }
    sv = htmlpart_GetStyleProp(n, "background-color");
    if (sv) { count += marks_push(st, AttrColorStyleFor("background-color", sv)); free(sv); }
    sv = htmlpart_GetStyleProp(n, "font-weight");
    if (sv) { if (value_contains_ci(sv, "bold")) { count += marks_push(st, BoldSt); } free(sv); }
    sv = htmlpart_GetStyleProp(n, "font-style");
    if (sv) { if (value_contains_ci(sv, "italic") || value_contains_ci(sv, "oblique")) { count += marks_push(st, ItalicSt); } free(sv); }
    sv = htmlpart_GetStyleProp(n, "text-decoration");
    if (sv) { if (value_contains_ci(sv, "underline")) { count += marks_push(st, UnderlineSt); } free(sv); }

    return count;
}

boolean htmlatk_Render(struct text *dest, long pos, const struct htmlnode *root,
                        htmlatk_ImageResolver resolver, void *resolverRock, long *lengthOut)
{
    struct hax_state st;
    struct hax_walkstack ws;
    long startpos = pos;

    InitStyles();

    st.dest = dest;
    st.pos = pos;
    st.anyContent = (pos > 0) ? 1 : 0;
    st.spacePending = 0;
    st.trailingNL = 2; /* don't force a leading break at the very start */
    st.predepth = 0;
    st.list = NULL; st.listcount = 0; st.listcap = 0;
    st.marks = NULL; st.markcount = 0; st.markcap = 0;
    st.pending = NULL; st.pendingcount = 0; st.pendingcap = 0;
    st.resolver = resolver;
    st.resolverRock = resolverRock;
    st.hardfail = 0;

    ws.items = NULL; ws.count = 0; ws.cap = 0;
    ws_push_siblings(&ws, root);

    while (ws.count > 0) {
        struct hax_walkitem item = ws.items[--ws.count];
        const struct htmlnode *n = item.node;

        if (!item.post) {
            if (n->type == HTMLPART_TEXT) {
                EmitTextRun(&st, n->text, n->textlen, st.predepth > 0);
                continue;
            }
            /* ELEMENT */
            if (tag_is_suppressed(n->tag)) continue;
            if (NodeIsStyleHidden(n)) continue;

            if (strcmp(n->tag, "table") == 0) {
                const struct htmlnode *cellChildren;
                if (TableIsTrivialWrapper(n, &cellChildren)) {
                    /* 1x1 no-op wrapper table -- see TableIsTrivialWrapper's
                       own comment. Inline its cell's children directly
                       instead of building an lset for it at all. */
                    ws_push_siblings(&ws, cellChildren);
                    continue;
                }
                /* Every table builds via lset now -- no more routing
                   decision (the earlier hybrid, and the colspan/rowspan
                   check that drove it, are gone as of 2026-08-16; see
                   this file's header comment and htmlatk.h's judgment-
                   call log). BuildLsetGrid/RenderTableAsLset degrade
                   gracefully (hardfail recorded, nothing inserted) for
                   a structurally degenerate table with no real rows. */
                if (!RenderTableAsLset(&st, n)) { /* hardfail already recorded */ }
                continue;
            }
            if (strcmp(n->tag, "img") == 0) {
                RenderImageInline(&st, n);
                continue;
            }
            if (strcmp(n->tag, "br") == 0) {
                st.spacePending = 0;
                text_AlwaysInsertCharacters(st.dest, st.pos, "\n", 1);
                ++st.pos; st.anyContent = 1; st.trailingNL = (st.trailingNL < 2) ? st.trailingNL + 1 : 2;
                continue;
            }
            if (strcmp(n->tag, "hr") == 0) {
                EnsureParaBreak(&st);
                InsertLiteral(&st, "----------------------------------------");
                EnsureParaBreak(&st);
                continue;
            }
            if (strcmp(n->tag, "ul") == 0) { EnsureParaBreak(&st); listctx_push(&st, 0); }
            else if (strcmp(n->tag, "ol") == 0) { EnsureParaBreak(&st); listctx_push(&st, 1); }
            else if (strcmp(n->tag, "li") == 0) { emit_li_marker(&st); }
            else if (strcmp(n->tag, "dt") == 0) { EnsureLineBreak(&st); }
            else if (strcmp(n->tag, "dd") == 0) { EnsureLineBreak(&st); InsertLiteral(&st, "    "); }
            else if (strcmp(n->tag, "pre") == 0) { EnsureParaBreak(&st); ++st.predepth; }
            else if (tag_is_para(n->tag)) { EnsureParaBreak(&st); }

            {
                int opened = 0;
                if (strcmp(n->tag, "a") == 0) {
                    const char *href = htmlpart_GetAttr(n, "href");
                    if (href && *href) {
                        struct style *ls = NULL;
                        FlushPendingSpace(&st); /* see PushFormattingMarks's own note */
                        if (LinkTemplateSt) {
                            ls = style_New();
                            if (ls) { style_Copy(LinkTemplateSt, ls); style_AddAttribute(ls, "href", (char *) href); }
                        }
                        opened += marks_push(&st, ls);
                    }
                } else {
                    opened = PushFormattingMarks(&st, n);
                }
                ws_push(&ws, n, 1, opened);
            }
            if (n->children) ws_push_siblings(&ws, n->children);
        } else {
            /* POST */
            if (strcmp(n->tag, "a") == 0) {
                if (item.marksOpened > 0 && st.markcount >= (long) item.marksOpened) {
                    struct hax_mark *m = &st.marks[st.markcount - item.marksOpened];
                    if (st.pos == m->start) {
                        const char *href = htmlpart_GetAttr(n, "href");
                        if (href && *href) InsertLiteral(&st, (char *) href);
                    }
                }
            } else if (strcmp(n->tag, "ul") == 0 || strcmp(n->tag, "ol") == 0) {
                EnsureParaBreak(&st); listctx_pop(&st);
            } else if (strcmp(n->tag, "li") == 0 || strcmp(n->tag, "dt") == 0 || strcmp(n->tag, "dd") == 0) {
                EnsureLineBreak(&st);
            } else if (strcmp(n->tag, "pre") == 0) {
                --st.predepth; EnsureParaBreak(&st);
            } else if (tag_is_para(n->tag)) {
                EnsureParaBreak(&st);
            }
            marks_finalize(&st, item.marksOpened);
        }
    }

    free(ws.items);
    free(st.list);
    free(st.marks);

    /* Only now, with every character/view already inserted and
       nothing left to insert, is it safe to actually materialize the
       recorded style spans -- see marks_finalize's comment. */
    ApplyPendingStyles(&st);
    free(st.pending);

    if (lengthOut) *lengthOut = st.pos - startpos;
    return !st.hardfail;
}

/* ==================================================================== *
 * Gate 4: click handling
 * ==================================================================== */

char *htmlatk_LinkAt(struct text *t, long pos)
{
    struct environment *env;
    char *href;

    if (!t || pos < 0) return NULL;
    env = environment_GetInnerMost(t->rootEnvironment, pos);
    while (env) {
        if (env->type == environment_Style && env->data.style) {
            href = style_GetAttribute(env->data.style, "href");
            if (href) return dupstr(href);
        }
        env = (struct environment *) environment_GetParent(env);
    }
    return NULL;
}

void htmlatk_LaunchURL(const char *url)
{
    char *cmd;
    size_t need;
    const char *p;
    char *q;
    FILE *fp;

    if (!url || !*url) return;

    /* "open '<shell-quoted url>'" -- see htmlatk.h's popen-safety
       note. Every literal single quote in url is replaced with the
       standard '\'' escape (close quote, escaped quote, reopen
       quote), so the shell never sees an unescaped byte from url. */
    need = strlen("open ''") + 1;
    for (p = url; *p; ++p) need += (*p == '\'') ? 4 : 1;
    cmd = (char *) malloc(need);
    if (!cmd) return;
    strcpy(cmd, "open '");
    q = cmd + strlen(cmd);
    for (p = url; *p; ++p) {
        if (*p == '\'') { strcpy(q, "'\\''"); q += 4; }
        else *q++ = *p;
    }
    strcpy(q, "'");

    fp = popen(cmd, "r");
    if (fp) pclose(fp);
    free(cmd);
}
