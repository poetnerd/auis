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
	(RenderTable / BuildTableGrid below) is a separate, self-contained
	piece of code the main walk detours into at a <table> PRE action --
	see htmlatk.h's own note on why *that* part uses ordinary bounded
	C recursion instead of an explicit stack.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <andrewos.h>
#include <class.h>
#include <text.ih>
#include <envrment.ih>
#include <style.ih>
#include <fontdesc.ih>
#include <dataobj.ih>
#include <table.ih>

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

/* Builds "[image: alt]" / "[image]" into a caller buffer (bufsz must
   be comfortably larger than any real alt text -- truncates rather
   than overflowing for a pathological alt attribute). Shared by the
   top-level inline placeholder and the table-cell placeholder. */
static void BuildImgPlaceholder(char *buf, size_t bufsz, const struct htmlnode *n)
{
    const char *alt = htmlpart_GetAttr(n, "alt");
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
        sprintf(buf, "[image: %.*s]", (int) (bufsz > 32 ? bufsz - 32 : 1), tmp);
    } else {
        strncpy(buf, "[image]", bufsz - 1);
        buf[bufsz - 1] = '\0';
    }
}

static void emit_img_placeholder_inline(struct hax_state *st, const struct htmlnode *n)
{
    char buf[600];
    BuildImgPlaceholder(buf, sizeof(buf), n);
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
    return "raster";
}

/* Attempts to read already-fetched, not-yet-CTE-decoded-by-us raw
   bytes (the resolver hands back fully decoded image bytes, so
   encoding is always "" here -- see image__ReadOtherFormat's contract
   in src/atk/basics/common/image.c, which only re-decodes base64/qp
   itself when told to) into targetObj via fmemopen, matching Gate 1's
   validated approach (see the delegated-session smoke test). Returns
   the ReadOtherFormat result; does not destroy targetObj on failure --
   that is the caller's job, since the caller also owns how targetObj
   was created (class_NewObject vs. table_Imbed). */
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

/* Top-level (non-table-cell) <img> handling: try a real embed via the
   resolver; on any failure (no resolver, resolver declines, format
   unsupported) fall back to the inline text placeholder. */
static void RenderImageInline(struct hax_state *st, const struct htmlnode *n)
{
    const char *src = htmlpart_GetAttr(n, "src");
    unsigned char *bytes = NULL;
    long len = 0;
    char *mimetype = NULL;
    boolean resolved = FALSE;

    if (src && st->resolver) {
        resolved = (*st->resolver)(st->resolverRock, src, &bytes, &len, &mimetype);
    }

    if (resolved && bytes) {
        struct dataobject *dob = (struct dataobject *) class_NewObject((char *) ImageClassForMimetype(mimetype));
        if (dob && TryReadImageInto(dob, mimetype, bytes, len)) {
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
    free(bytes);
    free(mimetype);
    emit_img_placeholder_inline(st, n);
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

/* A cell with no children at all, or whose only children are
   whitespace-only text nodes, is genuinely empty -- the only case
   worth a fast path (see this file's BuildTableGrid, below, and
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
   that). <img>/<table> are always "real content" regardless of what's
   inside them -- an empty-looking table can still have visible
   borders, and an unresolvable image still gets a placeholder -- so
   recursion stops there rather than trying to determine if a whole
   nested table is itself empty. Plain C recursion (not an explicit
   heap stack): this only ever descends through incidental wrapper
   markup around genuinely trivial content, the same "bounded by
   markup, not document size" reasoning already used for table-nesting
   recursion elsewhere in this file (see htmlatk.h's own note on why
   that's safe). */
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
    if (strcmp(n->tag, "img") == 0 || strcmp(n->tag, "table") == 0) return 0;
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

/* HTML width="NNN" (a bare pixel integer) -> real ATK column widths,
   since ATK's own default (TABLE_DEFAULT_COLUMN_THICKNESS, table.ch:
   99) is far too narrow for real content and produces severe one-
   word-per-line wrapping -- confirmed live, National Grid mail,
   2026-08-16, via the writeds/ez round-trip technique (see revival/
   doc/sonnet-playbook.md's "Verification tools" section). Deliberately
   strict: only a bare integer counts ("750"), not "100%" -- resolving
   a percentage needs a container width that doesn't exist yet at
   construction time (no live view), and misreading "100%" as 100
   *pixels* would make the bug worse, not better. Percentage/missing/
   malformed widths all fall through to HTML_TABLE_DEFAULT_WIDTH, a
   far more reasonable stand-in for "an email body's width" than
   ATK's own tiny built-in default -- most real HTML mail either
   omits width on layout tables entirely or uses width="100%" (see
   revival/tests/html-fixtures), so this default covers most
   real-world tables, not just an edge case. */
#define HTML_TABLE_DEFAULT_WIDTH 600

static long ParseWidthPixels(const char *s)
{
    long v;
    char *end;
    if (!s || !*s) return 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0) return 0;
    return v;
}

static int RenderTable(struct hax_state *st, const struct htmlnode *tablenode);

/* Fills an already-created (but not yet sized) table object T from
   tablenode's row/cell structure. RenderTable (below) is a thin
   wrapper that creates a fresh top-level struct table and inserts it
   as a view; a nested <table> inside a cell instead reaches here via
   this cell-content loop's own call into htmlatk_Render() (which,
   when it walks into the nested <table> tag, calls RenderTable again)
   -- not, as an earlier version of this function did, via BuildTableGrid
   calling itself directly on the same struct table's cell. That old
   direct self-call was a special case only nested <table>s used; the
   general per-cell recursion into htmlatk_Render() below now covers
   nested tables, inline images, and any mix of the two with ordinary
   rich text, all through the same one path (see htmlatk.h's
   judgment-call log). The no-longer-needed explicit depth counter the
   direct self-call used to thread through (never actually checked
   against a limit -- see htmlatk.h's own note that this recursion is
   bounded by markup structure, not a policed counter) is gone with
   it; C-stack depth for nested tables is now bounded by the
   BuildTableGrid -> htmlatk_Render -> RenderTable -> BuildTableGrid
   call chain instead, same bound, one indirection deeper. */
static int BuildTableGrid(struct table *T, const struct htmlnode *tablenode, struct hax_state *st)
{
    struct nodevec rows;
    long r, ncols = 0;
    long *pending; /* remaining rowspan carry-over, indexed by column */
    long pendcap = 0;
    const char *borderAttr;
    int border0;
    struct chunk whole;

    CollectRows(tablenode, &rows);

    /* Pass A: dimensions only. */
    pending = NULL;
    for (r = 0; r < rows.count; ++r) {
        const struct htmlnode *tr = rows.items[r];
        const struct htmlnode *td;
        long col = 0;
        long c;

        if ((long) pendcap < ncols) {
            long *np = (long *) realloc(pending, ncols * sizeof(long));
            if (np) { for (c = pendcap; c < ncols; ++c) np[c] = 0; pending = np; pendcap = ncols; }
        }
        for (td = tr->children; td; td = td->next) {
            long colspan, rowspan;
            if (!htmlpart_IsElement(td)) continue;
            if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
            while (col < pendcap && pending[col] > 0) ++col;
            colspan = ParsePositiveInt(htmlpart_GetAttr(td, "colspan"), 1, 1000);
            rowspan = ParsePositiveInt(htmlpart_GetAttr(td, "rowspan"), 1, 1000);
            if (col + colspan > pendcap) {
                long ncap = col + colspan;
                long *np = (long *) realloc(pending, ncap * sizeof(long));
                if (np) { for (c = pendcap; c < ncap; ++c) np[c] = 0; pending = np; pendcap = ncap; }
            }
            if (rowspan > 1 && col < pendcap) {
                for (c = col; c < col + colspan && c < pendcap; ++c) pending[c] = rowspan - 1;
            }
            col += colspan;
        }
        if (col > ncols) ncols = col;
        for (c = 0; c < pendcap; ++c) if (pending[c] > 0) --pending[c];
    }
    free(pending);

    if (rows.count == 0 || ncols == 0) {
        table_ChangeSize(T, rows.count > 0 ? (int) rows.count : 1, ncols > 0 ? (int) ncols : 1);
        nodevec_free(&rows);
        return TRUE; /* empty/malformed table: degrade to an empty grid, not a crash */
    }
    table_ChangeSize(T, (int) rows.count, (int) ncols);

    /* Real column widths -- see ParseWidthPixels's own comment for
       why. Evenly divides the table's total width (explicit or
       defaulted) across every column; a floor of 40px keeps a
       pathologically wide column count from producing degenerate
       slivers (table.c's own ChangeThickness floor is 10, but that's
       too tight for real text to be usable in). */
    {
        long totalWidth = ParseWidthPixels(htmlpart_GetAttr(tablenode, "width"));
        long perCol, wc;
        if (totalWidth <= 0) totalWidth = HTML_TABLE_DEFAULT_WIDTH;
        perCol = totalWidth / ncols;
        if (perCol < 40) perCol = 40;
        for (wc = 0; wc < ncols; ++wc) table_ChangeThickness(T, COLS, (int) wc, (int) perCol);
    }

    /* border="0" (or literally border="0", per the design doc) ->
       suppress all lines; anything else (including the attribute's
       plain absence -- the design doc does not address that case
       explicitly, resolved here by treating "absent" the same as
       "present and non-zero", i.e. visible, since that is the more
       common real intent for a table that bothered to include cell
       structure at all) -> visible black lines, since table.c's own
       class default for freshly grown edges is GHOST (dotted/
       invisible), not BLACK -- confirmed by reading table__ChangeSize
       in src/atk/table/table.c. */
    borderAttr = htmlpart_GetAttr(tablenode, "border");
    border0 = (borderAttr && strcmp(borderAttr, "0") == 0);
    whole.TopRow = 0; whole.BotRow = (int) rows.count - 1;
    whole.LeftCol = 0; whole.RightCol = (int) ncols - 1;
    table_SetInterior(T, &whole, border0 ? GHOST : BLACK);
    table_SetBoundary(T, &whole, border0 ? GHOST : BLACK);

    /* Pass B: place content, now that dimensions are final. */
    pendcap = 0; pending = NULL;
    for (r = 0; r < rows.count; ++r) {
        const struct htmlnode *tr = rows.items[r];
        const struct htmlnode *td;
        long col = 0;
        long c;

        if (pendcap < ncols) {
            long *np = (long *) realloc(pending, ncols * sizeof(long));
            if (np) { for (c = pendcap; c < ncols; ++c) np[c] = 0; pending = np; pendcap = ncols; }
        }
        for (td = tr->children; td; td = td->next) {
            long colspan, rowspan;
            struct chunk cellchunk, spanchunk;
            struct cell *cellp;

            if (!htmlpart_IsElement(td)) continue;
            if (strcmp(td->tag, "td") != 0 && strcmp(td->tag, "th") != 0) continue;
            while (col < pendcap && pending[col] > 0) ++col;
            if (col >= ncols) break; /* ragged/malformed row: drop the overflow cell, degrade not crash */

            colspan = ParsePositiveInt(htmlpart_GetAttr(td, "colspan"), 1, 1000);
            rowspan = ParsePositiveInt(htmlpart_GetAttr(td, "rowspan"), 1, 1000);
            if (col + colspan > ncols) colspan = ncols - col;
            if (col + colspan > pendcap) {
                long ncap = col + colspan;
                long *np = (long *) realloc(pending, ncap * sizeof(long));
                if (np) { for (c = pendcap; c < ncap; ++c) np[c] = 0; pending = np; pendcap = ncap; }
            }
            if (rowspan > 1) {
                for (c = col; c < col + colspan && c < pendcap; ++c) pending[c] = rowspan - 1;
            }

            if (colspan > 1 || rowspan > 1) {
                spanchunk.TopRow = (int) r; spanchunk.BotRow = (int) (r + rowspan - 1);
                spanchunk.LeftCol = (int) col; spanchunk.RightCol = (int) (col + colspan - 1);
                if (spanchunk.BotRow >= (int) rows.count) spanchunk.BotRow = (int) rows.count - 1;
                table_SetInterior(T, &spanchunk, JOINED);
            }

            cellchunk.TopRow = (int) r; cellchunk.BotRow = (int) r;
            cellchunk.LeftCol = (int) col; cellchunk.RightCol = (int) col;

            /* General case, per the real ATK precedent (see htmlatk.h's
               judgment-call note and PAPERS/atk/Sherman.Alloc lines
               663-1066: a real `table` datastream dump whose second
               cell is prose, then an inline embedded `calc` spreadsheet
               view, then more prose -- one cell, mixed rich content,
               not a scalar choice between "plain text" or "exactly one
               embedded object"). An empty cell (CellIsEmpty above) is
               left as the table_EmptyCell every freshly-grown cell
               already defaults to (table__ChangeSize's CreateCell(T,
               cell,NULL) path, src/atk/table/table.c) -- allocating a
               real "text" dataobject with nothing in it would be pure
               overhead for the overwhelmingly common empty-cell case.
               Every other cell, however simple ("plain text") or
               complex (styled prose around an inline image, or a
               nested <table>), gets a fresh "text" dataobject via
               table_Imbed() and this same module's own htmlatk_Render()
               recurses into it at position 0 -- nested <table>s and
               <img>s inside the cell fall out for free as ordinary
               embedded views within that recursive call, exactly as
               they would at the top level of the document, with no
               per-cell content-type dispatch needed here at all. */
            if (CellIsEmpty(td)) {
                /* table_EmptyCell already in place; nothing to do. */
            } else {
                table_Imbed(T, "text", &cellchunk);
                cellp = table_GetCell(T, (int) r, (int) col);
                if (cellp->celltype == table_ImbeddedObject) {
                    long celllen = 0;
                    if (!htmlatk_Render((struct text *) cellp->interior.ImbeddedObject.data,
                                         0, td->children, st->resolver, st->resolverRock, &celllen))
                        st->hardfail = 1;
                } else {
                    /* table_Imbed's class_NewObject("text") failed --
                       a real ATK object-allocation failure, same
                       category RenderTable's own table_New() failure
                       is (see htmlatk.h's Returns-FALSE contract). */
                    st->hardfail = 1;
                }
            }
            col += colspan;
        }
        for (c = 0; c < pendcap; ++c) if (pending[c] > 0) --pending[c];
    }
    free(pending);
    nodevec_free(&rows);
    return TRUE;
}

/* Top-level entry: builds a fresh struct table for tablenode and
   inserts it as an inline view at the walker's current position. */
static int RenderTable(struct hax_state *st, const struct htmlnode *tablenode)
{
    struct table *T = table_New();
    if (!T) { st->hardfail = 1; return FALSE; }
    if (!BuildTableGrid(T, tablenode, st)) st->hardfail = 1;
    FlushPendingSpace(st); /* AddView doesn't collapse into text runs like InsertLiteral does */
    text_AlwaysAddView(st->dest, st->pos, dataobject_ViewName((struct dataobject *) T), (struct dataobject *) T);
    ++st->pos;
    st->anyContent = 1;
    st->trailingNL = 0;
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

            if (strcmp(n->tag, "table") == 0) {
                if (!RenderTable(&st, n)) { /* hardfail already recorded */ }
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
