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
	htmlpart.c -- see htmlpart.h for the API, the sanitization
		     allowlist, and the no-C-recursion rationale.
		     ANSI C (C89 prototypes) throughout; no scanf/
		     sscanf/fscanf anywhere.

	Tree-building uses an explicit heap-allocated, realloc-grown
	stack of open elements (struct hpstack) instead of recursive
	descent -- see htmlpart_Parse(). Each stack entry is its own
	malloc'd struct hpframe (the stack itself only holds pointers),
	specifically so that growing the stack (realloc'ing the pointer
	array) never invalidates a "child append target" pointer that
	an inner frame is holding into an outer frame -- an earlier
	draft of this file stored struct hpframe directly in the
	growable array and took its address for exactly that purpose,
	which is a dangling-pointer bug waiting for the array to grow
	past an already-taken address; pointer-array-of-owned-structs
	sidesteps it entirely. htmlpart_Free() similarly frees with an
	explicit heap-allocated work list rather than recursing into
	->children.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>	/* strncasecmp */
#include <ctype.h>

#include <htmlpart.h>

/* Tag-only forward declarations for the internal (private to this
   file) struct types used below, so that every later reference --
   including the ones inside the function forward-declarations right
   after this -- resolves to the same file-scope (if still incomplete)
   type. Without this, a struct tag's *first* appearance being inside
   a function prototype's parameter list would give it prototype
   scope instead (a well-known C gotcha), silently creating a
   second, disconnected type of the same name and making the
   forward-declared prototype conflict with the real definition
   later in the file. */
struct hpbuf_s;
struct childsink;
struct hpframe;
struct hpstack;

/* ---- forward declarations (static, alphabetical) ---- */

static int attr_allowed(const char *tag, const char *attr);
static void decode_entities_into(struct hpbuf_s *out, const unsigned char *data, long start, long end);
static char *filter_style(const char *raw);
static void finalize_pop(struct hpframe *f, struct hpstack *st, struct childsink *rootsink);
static long find_matching_index(const struct hpstack *st, const char *tag);
static int freelist_push(struct htmlnode ***items, long *count, long *cap, struct htmlnode *n);
static long handle_close_tag(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink);
static long handle_open_tag(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink);
static long handle_text_run(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink);
static char *hp_strdup(const char *s);
static char *hpbuf_finish(struct hpbuf_s *b, long *outlen);
static void hpbuf_init(struct hpbuf_s *b);
static void hpbuf_putc(struct hpbuf_s *b, int c);
static void hpbuf_putn(struct hpbuf_s *b, const char *s, long n);
static void hpbuf_puts(struct hpbuf_s *b, const char *s);
static void hpstack_init(struct hpstack *st);
static struct hpframe *hpstack_pop(struct hpstack *st);
static void hpstack_push(struct hpstack *st, struct hpframe *f);
static struct hpframe *hpstack_top(const struct hpstack *st);
static int href_scheme_ok(const char *href);
static void htmlnode_free_shallow(struct htmlnode *n);
static int is_markup_start(const unsigned char *data, long pos, long len);
static int is_void_tag(const char *name);
static struct htmlnode *new_element_node(const char *tag);
static struct htmlnode *new_text_node(char *text, long textlen);
static long read_name(const unsigned char *data, long pos, long len, char *buf, int bufsz);
static void sink_append(struct childsink *sink, struct htmlnode *node);
static long skip_bang_or_pi(const unsigned char *data, long pos, long len);
static long skip_comment(const unsigned char *data, long pos, long len);
static long skip_rawtext_element(const unsigned char *data, long pos, long len, const char *tagname);
static long skip_to_tag_end(const unsigned char *data, long pos, long len);
static struct childsink *currentsink(const struct hpstack *st, struct childsink *rootsink);
static int topdrop(const struct hpstack *st);

/* ---- small growable byte buffer, used for entity-decoded text/attr
   values and filtered style strings; doubling growth, no size cap
   (see htmlpart.h's "No fixed size or depth caps" note) ---- */

struct hpbuf_s { char *data; long len; long cap; };

static void hpbuf_init(struct hpbuf_s *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void hpbuf_putc(struct hpbuf_s *b, int c)
{
    if (b->len + 1 >= b->cap) {
        long ncap = b->cap ? b->cap * 2 : 64;
        char *nb = (char *) realloc(b->data, ncap);
        if (!nb) return; /* best-effort: drop the byte rather than abort */
        b->data = nb;
        b->cap = ncap;
    }
    b->data[b->len++] = (char) c;
}

static void hpbuf_putn(struct hpbuf_s *b, const char *s, long n)
{
    long i;
    for (i = 0; i < n; ++i) hpbuf_putc(b, (unsigned char) s[i]);
}

static void hpbuf_puts(struct hpbuf_s *b, const char *s)
{
    while (*s) hpbuf_putc(b, (unsigned char) *s++);
}

/* NUL-terminates and returns the buffer (caller owns/frees); *outlen
   (if non-NULL) gets the length not counting the NUL. Never returns
   NULL except on total allocation failure. */
static char *hpbuf_finish(struct hpbuf_s *b, long *outlen)
{
    hpbuf_putc(b, '\0');
    if (!b->data) {
        char *empty = (char *) malloc(1);
        if (empty) empty[0] = '\0';
        if (outlen) *outlen = 0;
        return empty;
    }
    if (outlen) *outlen = b->len - 1;
    return b->data;
}

static char *hp_strdup(const char *s)
{
    char *r;
    if (!s) return NULL;
    r = (char *) malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

/* ---- HTML named-entity table: the complete standard ISO-8859-1
   (Latin-1) HTML4 named entity set, plus the five markup-special
   entities. Deliberately larger than mimepart.c's 6-entry table --
   this parser's job is exactly to turn HTML text into readable Latin-
   1, so accented names/currency signs in real mail (see the fixture
   corpus) are worth decoding properly, not just passing through
   unknown-entity-verbatim. Anything outside this set (there is no
   Latin-1 codepoint for it, e.g. &mdash; &hellip; &trade; &euro;)
   falls through to the same "unknown: emit &name; verbatim" handling
   mimepart.c's emit_entity uses, rather than guessing at an ASCII
   transliteration. ---- */

struct hp_entity { const char *name; unsigned char latin1; };

static const struct hp_entity hp_entities[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
    { "nbsp", 0xA0 }, { "iexcl", 0xA1 }, { "cent", 0xA2 }, { "pound", 0xA3 },
    { "curren", 0xA4 }, { "yen", 0xA5 }, { "brvbar", 0xA6 }, { "sect", 0xA7 },
    { "uml", 0xA8 }, { "copy", 0xA9 }, { "ordf", 0xAA }, { "laquo", 0xAB },
    { "not", 0xAC }, { "shy", 0xAD }, { "reg", 0xAE }, { "macr", 0xAF },
    { "deg", 0xB0 }, { "plusmn", 0xB1 }, { "sup2", 0xB2 }, { "sup3", 0xB3 },
    { "acute", 0xB4 }, { "micro", 0xB5 }, { "para", 0xB6 }, { "middot", 0xB7 },
    { "cedil", 0xB8 }, { "sup1", 0xB9 }, { "ordm", 0xBA }, { "raquo", 0xBB },
    { "frac14", 0xBC }, { "frac12", 0xBD }, { "frac34", 0xBE }, { "iquest", 0xBF },
    { "Agrave", 0xC0 }, { "Aacute", 0xC1 }, { "Acirc", 0xC2 }, { "Atilde", 0xC3 },
    { "Auml", 0xC4 }, { "Aring", 0xC5 }, { "AElig", 0xC6 }, { "Ccedil", 0xC7 },
    { "Egrave", 0xC8 }, { "Eacute", 0xC9 }, { "Ecirc", 0xCA }, { "Euml", 0xCB },
    { "Igrave", 0xCC }, { "Iacute", 0xCD }, { "Icirc", 0xCE }, { "Iuml", 0xCF },
    { "ETH", 0xD0 }, { "Ntilde", 0xD1 }, { "Ograve", 0xD2 }, { "Oacute", 0xD3 },
    { "Ocirc", 0xD4 }, { "Otilde", 0xD5 }, { "Ouml", 0xD6 }, { "times", 0xD7 },
    { "Oslash", 0xD8 }, { "Ugrave", 0xD9 }, { "Uacute", 0xDA }, { "Ucirc", 0xDB },
    { "Uuml", 0xDC }, { "Yacute", 0xDD }, { "THORN", 0xDE }, { "szlig", 0xDF },
    { "agrave", 0xE0 }, { "aacute", 0xE1 }, { "acirc", 0xE2 }, { "atilde", 0xE3 },
    { "auml", 0xE4 }, { "aring", 0xE5 }, { "aelig", 0xE6 }, { "ccedil", 0xE7 },
    { "egrave", 0xE8 }, { "eacute", 0xE9 }, { "ecirc", 0xEA }, { "euml", 0xEB },
    { "igrave", 0xEC }, { "iacute", 0xED }, { "icirc", 0xEE }, { "iuml", 0xEF },
    { "eth", 0xF0 }, { "ntilde", 0xF1 }, { "ograve", 0xF2 }, { "oacute", 0xF3 },
    { "ocirc", 0xF4 }, { "otilde", 0xF5 }, { "ouml", 0xF6 }, { "divide", 0xF7 },
    { "oslash", 0xF8 }, { "ugrave", 0xF9 }, { "uacute", 0xFA }, { "ucirc", 0xFB },
    { "uuml", 0xFC }, { "yacute", 0xFD }, { "thorn", 0xFE }, { "yuml", 0xFF },
    { NULL, 0 }
};

/* Named/numeric references to zero-width Unicode formatting
   characters -- unlike hp_entities above, none of these have a
   Latin-1 codepoint (they postdate Latin-1 entirely), so they can't
   go in that table. Unlike the "some real character, we don't know
   which" case numeric entities >0xFF fall back to ('?'), these are
   dropped silently: a real browser renders zero-width characters as
   literally nothing, so a visible '?' would be actively wrong, not
   just approximate. Confirmed live in real mail (National Grid,
   found 2026-08-16): &zwnj; used as "hidden preheader" anti-clipping
   padding (a wall of them in a color:#ffffff div, controlling the
   inbox preview snippet -- see revival/tests/html-fixtures/README.md
   for the same trick found independently in the fixture corpus) was
   rendering as literal "&zwnj;" text instead of vanishing. zwj/lrm/
   rlm are the same character class (invisible joining/direction
   marks) and get the same treatment on the same reasoning, even
   though only zwnj has been directly observed in real mail so far. */
static const char * const hp_zerowidth_names[] = {
    "zwnj", "zwj", "lrm", "rlm", NULL
};

static int is_zerowidth_codepoint(long code)
{
    return code == 0x200B || code == 0x200C || code == 0x200D
        || code == 0x200E || code == 0x200F || code == 0xFEFF;
}

/* Same ASCII-fallback set as mimepart_Utf8ToLatin1's own
   emit_smart_punct (mimepart.c) -- deliberately not shared code
   (different translation unit, same "own independent copy" precedent
   already used elsewhere in this pair of files, e.g. htmltext.c's own
   NodeIsStyleHidden). This handles the entity-reference form of the
   same characters (`&rsquo;`/`&#8217;`/etc.), which reaches this
   function unchanged by the UTF-8-to-Latin1 pre-pass in text822.c --
   entities are plain ASCII regardless of the document's encoding, so
   a source using `&mdash;` instead of a raw UTF-8 em dash needs its
   own fix here, not just the raw-bytes one. Before this, an unmatched
   named entity fell through to hp_entities' final else-branch and
   rendered as literal "&mdash;" text (see decode_entities_into's
   fallthrough below); a numeric one above 0xFF became '?' like any
   other out-of-range codepoint. */
static int try_smart_punct(struct hpbuf_s *out, long code)
{
    switch (code) {
    case 0x2018: case 0x2019: hpbuf_putc(out, '\''); return 1;
    case 0x201C: case 0x201D: hpbuf_putc(out, '"'); return 1;
    case 0x2013: hpbuf_putc(out, '-'); return 1;
    case 0x2014: hpbuf_puts(out, "--"); return 1;
    case 0x2026: hpbuf_puts(out, "..."); return 1;
    default: return 0;
    }
}

static long smart_punct_codepoint_for_name(const char *name)
{
    if (strcmp(name, "lsquo") == 0) return 0x2018;
    if (strcmp(name, "rsquo") == 0) return 0x2019;
    if (strcmp(name, "ldquo") == 0) return 0x201C;
    if (strcmp(name, "rdquo") == 0) return 0x201D;
    if (strcmp(name, "ndash") == 0) return 0x2013;
    if (strcmp(name, "mdash") == 0) return 0x2014;
    if (strcmp(name, "hellip") == 0) return 0x2026;
    return 0;
}

/* name/namelen point into the caller's (not necessarily NUL-
   terminated) input buffer -- copied into a small stack buffer before
   strtol, same reasoning as mimepart.c's emit_entity. Numeric entities
   above 0xFF (nearly all of Unicode) become '?', matching
   mimepart_Utf8ToLatin1's policy for the same reason: there is no
   Latin-1 byte for them -- except the known zero-width codepoints
   above (dropped silently, see their own comment) and the small
   smart-punctuation set above (try_smart_punct: curly quotes, en/em
   dash, ellipsis -- ASCII-substituted instead of '?'). Named entities
   go through the same three exceptions, checked in the same order,
   before falling back to literal passthrough (e.g. unrecognized
   "&foo;" text). */
static void decode_entities_into(struct hpbuf_s *out, const unsigned char *data, long start, long end)
{
    long i = start;

    while (i < end) {
        if (data[i] == '&') {
            long j = i + 1, estart = j;
            while (j < end && j - estart < 32 && data[j] != ';' && data[j] != '&'
                   && !isspace(data[j])) ++j;
            if (j < end && data[j] == ';' && j > estart) {
                char nbuf[36];
                long namelen = j - estart;
                int k, matched = 0;

                if (namelen >= (long) sizeof(nbuf)) namelen = sizeof(nbuf) - 1;
                memcpy(nbuf, data + estart, namelen);
                nbuf[namelen] = '\0';

                if (namelen >= 2 && nbuf[0] == '#') {
                    long code;
                    char *ep, *st2;
                    if (namelen >= 3 && (nbuf[1] == 'x' || nbuf[1] == 'X')) {
                        st2 = nbuf + 2;
                        code = strtol(st2, &ep, 16);
                    } else {
                        st2 = nbuf + 1;
                        code = strtol(st2, &ep, 10);
                    }
                    {
                        int valid = (ep != st2 && code > 0);
                        if (valid && code <= 0xFF) {
                            hpbuf_putc(out, (int) code);
                        } else if (valid && is_zerowidth_codepoint(code)) {
                            /* drop silently */
                        } else if (!(valid && try_smart_punct(out, code))) {
                            hpbuf_putc(out, '?');
                        }
                    }
                    matched = 1;
                } else {
                    for (k = 0; hp_entities[k].name; ++k) {
                        if (strcmp(hp_entities[k].name, nbuf) == 0) {
                            hpbuf_putc(out, hp_entities[k].latin1);
                            matched = 1;
                            break;
                        }
                    }
                    if (!matched) {
                        for (k = 0; hp_zerowidth_names[k]; ++k) {
                            if (strcmp(hp_zerowidth_names[k], nbuf) == 0) {
                                matched = 1; /* drop silently, emit nothing */
                                break;
                            }
                        }
                    }
                    if (!matched) {
                        long spcode = smart_punct_codepoint_for_name(nbuf);
                        if (spcode && try_smart_punct(out, spcode)) matched = 1;
                    }
                }
                if (matched) { i = j + 1; continue; }
                hpbuf_putc(out, '&');
                hpbuf_putn(out, (const char *) data + estart, namelen);
                hpbuf_putc(out, ';');
                i = j + 1;
                continue;
            }
            hpbuf_putc(out, '&');
            ++i;
            continue;
        }
        if (data[i] == '\r') { ++i; continue; } /* normalize CRLF/lone-CR to LF-only */
        hpbuf_putc(out, data[i]);
        ++i;
    }
}

/* ---- tag/attribute allowlist tables (see htmlpart.h) ---- */

enum hp_tagrule { TAGRULE_KEEP, TAGRULE_DROP, TAGRULE_UNKNOWN };

struct tagrule_entry { const char *name; enum hp_tagrule cls; };

static const struct tagrule_entry hp_tagrules[] = {
    { "html", TAGRULE_KEEP }, { "head", TAGRULE_KEEP }, { "body", TAGRULE_KEEP },
    { "title", TAGRULE_KEEP },
    { "p", TAGRULE_KEEP }, { "div", TAGRULE_KEEP }, { "span", TAGRULE_KEEP },
    { "br", TAGRULE_KEEP }, { "hr", TAGRULE_KEEP },
    { "a", TAGRULE_KEEP }, { "b", TAGRULE_KEEP }, { "strong", TAGRULE_KEEP },
    { "i", TAGRULE_KEEP }, { "em", TAGRULE_KEEP }, { "u", TAGRULE_KEEP },
    { "tt", TAGRULE_KEEP }, { "code", TAGRULE_KEEP }, { "pre", TAGRULE_KEEP },
    { "blockquote", TAGRULE_KEEP },
    { "h1", TAGRULE_KEEP }, { "h2", TAGRULE_KEEP }, { "h3", TAGRULE_KEEP },
    { "h4", TAGRULE_KEEP }, { "h5", TAGRULE_KEEP }, { "h6", TAGRULE_KEEP },
    { "ul", TAGRULE_KEEP }, { "ol", TAGRULE_KEEP }, { "li", TAGRULE_KEEP },
    { "dl", TAGRULE_KEEP }, { "dt", TAGRULE_KEEP }, { "dd", TAGRULE_KEEP },
    { "table", TAGRULE_KEEP }, { "thead", TAGRULE_KEEP }, { "tbody", TAGRULE_KEEP },
    { "tr", TAGRULE_KEEP }, { "td", TAGRULE_KEEP }, { "th", TAGRULE_KEEP },
    { "img", TAGRULE_KEEP }, { "font", TAGRULE_KEEP },
    /* dropped, contents included -- script/style/textarea are also
       raw-text spans, handled before this table is even consulted
       (see handle_open_tag); listed here too for documentation and
       so classify_tag() gives the right answer if ever consulted for
       them directly (e.g. a stray, never-opened </script>). */
    { "script", TAGRULE_DROP }, { "style", TAGRULE_DROP },
    { "meta", TAGRULE_DROP }, { "link", TAGRULE_DROP },
    { "iframe", TAGRULE_DROP }, { "object", TAGRULE_DROP }, { "embed", TAGRULE_DROP },
    { "form", TAGRULE_DROP }, { "input", TAGRULE_DROP }, { "select", TAGRULE_DROP },
    { "button", TAGRULE_DROP }, { "textarea", TAGRULE_DROP },
    { NULL, TAGRULE_UNKNOWN }
};

static enum hp_tagrule classify_tag(const char *name)
{
    int i;
    for (i = 0; hp_tagrules[i].name; ++i) {
        if (strcmp(hp_tagrules[i].name, name) == 0) return hp_tagrules[i].cls;
    }
    return TAGRULE_UNKNOWN;
}

/* void (no-content, never has a real closing tag in practice) elements
   -- self-close implicitly even without "/>" so an omitted-in-practice
   closer (almost every real "<img ...>"/"<br>"/"<input ...>") doesn't
   leave a phantom open frame on the stack. */
static int is_void_tag(const char *name)
{
    static const char * const voidtags[] = {
        "br", "hr", "img", "input", "link", "meta", "embed",
        "area", "base", "col", "param", "source", "track", "wbr", NULL
    };
    int i;
    for (i = 0; voidtags[i]; ++i) {
        if (strcmp(voidtags[i], name) == 0) return 1;
    }
    return 0;
}

/* Per-element attribute allowlist (see htmlpart.h). "style" is
   allowed on any element that reaches here (i.e. any kept element --
   attr_allowed is only ever consulted for a HP_KEEP-classified tag). */
static int attr_allowed(const char *tag, const char *attr)
{
    if (strcmp(attr, "style") == 0) return 1;
    if (strcmp(tag, "a") == 0) {
        return strcmp(attr, "href") == 0;
    }
    if (strcmp(tag, "img") == 0) {
        return strcmp(attr, "src") == 0 || strcmp(attr, "alt") == 0
            || strcmp(attr, "width") == 0 || strcmp(attr, "height") == 0;
    }
    if (strcmp(tag, "table") == 0 || strcmp(tag, "td") == 0 || strcmp(tag, "th") == 0) {
        return strcmp(attr, "colspan") == 0 || strcmp(attr, "rowspan") == 0
            || strcmp(attr, "border") == 0;
    }
    if (strcmp(tag, "font") == 0) {
        return strcmp(attr, "color") == 0 || strcmp(attr, "size") == 0;
    }
    return 0;
}

/* href scheme gate: http:/https:/mailto: only. A leading run of
   whitespace is tolerated before the scheme (classic evasion is
   " javascript:..." with a leading space/tab/newline); anything with
   no recognized scheme at all -- including a relative link or a bare
   "#fragment" -- fails this check per the design doc's literal
   allowlist ("must be http:/https:/mailto: scheme"). */
static int href_scheme_ok(const char *href)
{
    const char *p = href;
    char scheme[16];
    int i = 0;

    while (*p && isspace((unsigned char) *p)) ++p;
    while (*p && *p != ':' && i < (int) sizeof(scheme) - 1
           && (isalnum((unsigned char) *p) || *p == '+' || *p == '-' || *p == '.')) {
        scheme[i++] = (char) tolower((unsigned char) *p);
        ++p;
    }
    scheme[i] = '\0';
    if (*p != ':') return 0;
    return strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0
        || strcmp(scheme, "mailto") == 0;
}

/* Filters a raw (already entity-decoded) style="..." value down to the
   fixed property allowlist, canonicalized into that fixed order (not
   source order), last-value-wins per property if declared more than
   once. Returns NULL (nothing kept) rather than an empty string if no
   allowed property was present.

   `display`/`visibility` were added 2026-08-16, alongside the
   original 5 cosmetic properties -- not for cosmetic effect (neither
   renderer does CSS layout) but so `htmlatk_Render`/`htmltext_ToText`
   can detect and skip `display:none`/`visibility:hidden` content
   entirely. This matters more than it might look: the "hidden
   preheader" trick -- `<div style="display:none">short summary text
   for the inbox preview line</div>` -- is an industry-standard pattern
   in essentially every commercial marketing email, and was previously
   rendering as ordinary visible body text (confirmed live, National
   Grid fixture, 2026-08-16: "We have helpful resources to help manage
   energy costs" at the very top of the rendered body is that exact
   hidden div's text, with no visible counterpart anywhere else in the
   source). Without these two properties surviving the allowlist, a
   renderer has no way to know a node was ever marked hidden in the
   first place. */
static const char * const hp_style_props[7] = {
    "color", "background-color", "font-weight", "font-style", "text-decoration",
    "display", "visibility"
};

static char *filter_style(const char *raw)
{
    char *vals[7];
    int i;
    const char *p = raw;
    struct hpbuf_s out;

    for (i = 0; i < 7; ++i) vals[i] = NULL;

    while (*p) {
        const char *propstart, *propend, *valstart, *valend;
        long proplen;

        while (*p && (isspace((unsigned char) *p) || *p == ';')) ++p;
        if (!*p) break;
        propstart = p;
        while (*p && *p != ':' && *p != ';') ++p;
        propend = p;
        while (propend > propstart && isspace((unsigned char) propend[-1])) --propend;
        proplen = propend - propstart;

        if (*p == ':') {
            ++p;
            while (*p && isspace((unsigned char) *p)) ++p;
            valstart = p;
            while (*p && *p != ';') ++p;
            valend = p;
            while (valend > valstart && isspace((unsigned char) valend[-1])) --valend;

            for (i = 0; i < 7; ++i) {
                if ((long) strlen(hp_style_props[i]) == proplen
                    && strncasecmp(hp_style_props[i], propstart, (size_t) proplen) == 0) {
                    free(vals[i]);
                    vals[i] = (char *) malloc(valend - valstart + 1);
                    if (vals[i]) {
                        memcpy(vals[i], valstart, valend - valstart);
                        vals[i][valend - valstart] = '\0';
                    }
                    break;
                }
            }
        }
        if (*p == ';') ++p;
    }

    hpbuf_init(&out);
    for (i = 0; i < 7; ++i) {
        if (vals[i]) {
            if (out.len > 0) hpbuf_putc(&out, ';');
            hpbuf_puts(&out, hp_style_props[i]);
            hpbuf_putc(&out, ':');
            hpbuf_puts(&out, vals[i]);
            free(vals[i]);
        }
    }
    if (out.len == 0) {
        free(out.data);
        return NULL;
    }
    return hpbuf_finish(&out, NULL);
}

/* ---- node constructors ---- */

static struct htmlnode *new_element_node(const char *tag)
{
    struct htmlnode *n = (struct htmlnode *) malloc(sizeof(struct htmlnode));
    if (!n) return NULL;
    n->type = HTMLPART_ELEMENT;
    n->tag = hp_strdup(tag);
    n->attrs = NULL;
    n->text = NULL;
    n->textlen = 0;
    n->children = NULL;
    n->next = NULL;
    return n;
}

static struct htmlnode *new_text_node(char *text, long textlen)
{
    struct htmlnode *n = (struct htmlnode *) malloc(sizeof(struct htmlnode));
    if (!n) { free(text); return NULL; }
    n->type = HTMLPART_TEXT;
    n->tag = NULL;
    n->attrs = NULL;
    n->text = text;
    n->textlen = textlen;
    n->children = NULL;
    n->next = NULL;
    return n;
}

static void htmlnode_free_shallow(struct htmlnode *n)
{
    struct htmlattr *a = n->attrs;
    while (a) {
        struct htmlattr *an = a->next;
        free(a->name);
        free(a->value);
        free(a);
        a = an;
    }
    free(n->tag);
    free(n->text);
    free(n);
}

/* ---- child-append targets ("sinks") and the explicit open-element
   stack -- see the file header comment for why frames are individually
   malloc'd rather than stored inline in the growable array. ---- */

struct childsink { struct htmlnode *head, *tail; };

enum hpframekind { HPF_KEEP, HPF_UNKNOWN, HPF_DROP };

struct hpframe {
    char *tag;			/* lowercased, for close-tag matching */
    enum hpframekind kind;
    struct htmlnode *node;	/* HPF_KEEP only */
    struct childsink ownsink;	/* HPF_KEEP only: this element's own
				   children accumulate here */
    struct childsink *sink;	/* where a tag opened *inside* this
				   frame should attach: &ownsink for
				   HPF_KEEP, the parent's own sink
				   (forwarded/aliased) for HPF_UNKNOWN
				   -- so an unknown wrapper's children
				   land exactly where the wrapper's own
				   children would have -- and NULL
				   (discard) for HPF_DROP. */
};

struct hpstack {
    struct hpframe **items;	/* array of pointers: growing this array
				   via realloc must never invalidate a
				   &frame->ownsink an inner HPF_UNKNOWN
				   frame is holding, which it would if
				   frames were stored inline */
    long count, cap;
};

static void hpstack_init(struct hpstack *st)
{
    st->items = NULL;
    st->count = 0;
    st->cap = 0;
}

static void hpstack_push(struct hpstack *st, struct hpframe *f)
{
    if (st->count >= st->cap) {
        long ncap = st->cap ? st->cap * 2 : 32;
        struct hpframe **ni = (struct hpframe **) realloc(st->items, ncap * sizeof(struct hpframe *));
        if (!ni) return; /* OOM: frame is orphaned (leaked); parsing
                             continues best-effort rather than aborting */
        st->items = ni;
        st->cap = ncap;
    }
    st->items[st->count++] = f;
}

static struct hpframe *hpstack_pop(struct hpstack *st)
{
    if (st->count <= 0) return NULL;
    return st->items[--st->count];
}

static struct hpframe *hpstack_top(const struct hpstack *st)
{
    if (st->count <= 0) return NULL;
    return st->items[st->count - 1];
}

static struct childsink *currentsink(const struct hpstack *st, struct childsink *rootsink)
{
    struct hpframe *top = hpstack_top(st);
    return top ? top->sink : rootsink;
}

static int topdrop(const struct hpstack *st)
{
    struct hpframe *top = hpstack_top(st);
    return top != NULL && top->kind == HPF_DROP;
}

static void sink_append(struct childsink *sink, struct htmlnode *node)
{
    node->next = NULL;
    if (!sink) { htmlnode_free_shallow(node); return; }
    if (sink->tail) {
        sink->tail->next = node;
        sink->tail = node;
    } else {
        sink->head = sink->tail = node;
    }
}

/* Pops (already-removed-from-stack) frame f: for a kept element,
   attaches its accumulated children and appends the finished node to
   whatever is now the current sink (its parent's, since f is already
   off the stack). For UNKNOWN/DROP frames there is nothing left to do
   -- UNKNOWN's children were appended directly into the forwarded
   parent sink in real time; DROP's were discarded in real time. */
static void finalize_pop(struct hpframe *f, struct hpstack *st, struct childsink *rootsink)
{
    if (f->kind == HPF_KEEP) {
        f->node->children = f->ownsink.head;
        sink_append(currentsink(st, rootsink), f->node);
    }
    free(f->tag);
    free(f);
}

static long find_matching_index(const struct hpstack *st, const char *tag)
{
    long i;
    for (i = st->count - 1; i >= 0; --i) {
        if (strcmp(st->items[i]->tag, tag) == 0) return i;
    }
    return -1;
}

/* ---- tokenizer primitives ---- */

static long read_name(const unsigned char *data, long pos, long len, char *buf, int bufsz)
{
    int n = 0;
    while (pos < len) {
        unsigned char c = data[pos];
        if (isalnum(c) || c == '-' || c == ':' || c == '_' || c == '.') {
            if (n < bufsz - 1) buf[n++] = (char) tolower(c);
            ++pos;
        } else {
            break;
        }
    }
    buf[n] = '\0';
    return pos;
}

static int is_markup_start(const unsigned char *data, long pos, long len)
{
    unsigned char c;
    if (data[pos] != '<' || pos + 1 >= len) return 0;
    c = data[pos + 1];
    return isalpha(c) || c == '/' || c == '!' || c == '?';
}

static long skip_comment(const unsigned char *data, long pos, long len)
{
    long p = pos + 4; /* past "<!--" */
    while (p + 2 < len) {
        if (data[p] == '-' && data[p + 1] == '-' && data[p + 2] == '>') return p + 3;
        ++p;
    }
    return len; /* unterminated: consume to EOF rather than corrupt nesting */
}

/* <!DOCTYPE ...> and <?xml ...?> alike: neither needs anything more
   than "find the next '>'" for the mail-HTML subset this project
   targets (see htmlpart.h). Recognized and skipped without ever being
   pushed as an entity -- the direct fix for the htmlview bug class. */
static long skip_bang_or_pi(const unsigned char *data, long pos, long len)
{
    long p = pos + 2;
    while (p < len && data[p] != '>') ++p;
    return (p < len) ? p + 1 : len;
}

/* Scans past a start tag's attributes (respecting quoted values that
   may contain '>') to the '>' that ends it. Used only for raw-text
   elements, where the attributes themselves are always discarded. */
static long skip_to_tag_end(const unsigned char *data, long pos, long len)
{
    while (pos < len && data[pos] != '>') {
        if (data[pos] == '"' || data[pos] == '\'') {
            unsigned char q = data[pos];
            ++pos;
            while (pos < len && data[pos] != q) ++pos;
        }
        ++pos;
    }
    return (pos < len) ? pos + 1 : len;
}

/* script/style/textarea: scans for the literal, case-insensitive
   closing tag rather than tokenizing content as markup, same
   technique as mimepart_HtmlToText's <script>/<style> skip -- this is
   what keeps a stray '<' inside embedded JS/CSS from desyncing the
   tokenizer and swallowing a real subsequent tag (including the
   element's own closing tag). Always fully discarded regardless of
   surrounding drop state -- the caller never pushes a stack frame for
   these at all. */
static long skip_rawtext_element(const unsigned char *data, long pos, long len, const char *tagname)
{
    long contentstart = skip_to_tag_end(data, pos, len);
    long p = contentstart;

    while (p < len) {
        if (data[p] == '<' && p + 1 < len && data[p + 1] == '/') {
            char namebuf[64];
            long q = read_name(data, p + 2, len, namebuf, (int) sizeof(namebuf));
            if (strcmp(namebuf, tagname) == 0) {
                while (q < len && data[q] != '>') ++q;
                return (q < len) ? q + 1 : len;
            }
        }
        ++p;
    }
    return len; /* unterminated: consume to EOF */
}

static long handle_text_run(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink)
{
    long start = pos;
    while (pos < len && !(data[pos] == '<' && is_markup_start(data, pos, len))) ++pos;
    if (!topdrop(st) && pos > start) {
        struct hpbuf_s buf;
        long tlen;
        char *text;

        hpbuf_init(&buf);
        decode_entities_into(&buf, data, start, pos);
        text = hpbuf_finish(&buf, &tlen);
        if (text) {
            struct htmlnode *tn = new_text_node(text, tlen);
            if (tn) sink_append(currentsink(st, rootsink), tn);
        }
    }
    return pos;
}

static long handle_close_tag(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink)
{
    char namebuf[64];
    long p, q, idx;

    p = read_name(data, pos + 2, len, namebuf, (int) sizeof(namebuf));
    q = p;
    while (q < len && data[q] != '>') ++q;
    if (namebuf[0] == '\0') return (q < len) ? q + 1 : q;

    /* Search up the stack for a matching open tag. Not found: a stray
       close tag (the extra "</table>" in fixture 08's imbalance) is
       ignored outright -- the stack is untouched, matching the design
       doc's graceful-degradation requirement rather than treating it
       as an error. Found: pop everything from the top down to and
       including the match (an "implied close" cascade -- also covers
       fixture 07's imbalance the other direction, since any tags left
       open past the last real close simply get closed here or at
       end-of-input). */
    idx = find_matching_index(st, namebuf);
    if (idx >= 0) {
        while (st->count - 1 >= idx) {
            struct hpframe *f = hpstack_pop(st);
            if (f) finalize_pop(f, st, rootsink);
        }
    }
    return (q < len) ? q + 1 : q;
}

static long handle_open_tag(const unsigned char *data, long pos, long len, struct hpstack *st, struct childsink *rootsink)
{
    char namebuf[64];
    long p;
    int parentdrop;
    enum hpframekind kindwanted;
    struct htmlnode *node;
    struct htmlattr **attrtail;
    int selfclose;

    p = read_name(data, pos + 1, len, namebuf, (int) sizeof(namebuf));

    /* raw-text elements are handled atomically, regardless of the
       current drop state -- see skip_rawtext_element's comment for
       why this must not go through the general attribute/child loop
       below even when already inside a dropped ancestor. */
    if (namebuf[0] != '\0'
        && (strcmp(namebuf, "script") == 0 || strcmp(namebuf, "style") == 0
            || strcmp(namebuf, "textarea") == 0)) {
        return skip_rawtext_element(data, p, len, namebuf);
    }

    parentdrop = topdrop(st);
    if (parentdrop) {
        kindwanted = HPF_DROP;
    } else {
        switch (classify_tag(namebuf)) {
        case TAGRULE_KEEP: kindwanted = HPF_KEEP; break;
        case TAGRULE_DROP: kindwanted = HPF_DROP; break;
        default:           kindwanted = HPF_UNKNOWN; break;
        }
    }

    node = NULL;
    attrtail = NULL;
    if (kindwanted == HPF_KEEP) {
        node = new_element_node(namebuf);
        if (node) attrtail = &node->attrs;
    }

    selfclose = 0;
    for (;;) {
        while (p < len && isspace(data[p])) ++p;
        if (p >= len) break;
        if (data[p] == '>') { ++p; break; }
        if (data[p] == '/' && p + 1 < len && data[p + 1] == '>') { selfclose = 1; p += 2; break; }
        if (data[p] == '/') { ++p; continue; }

        {
            char attrnamebuf[64];
            long namestart = p, valstart = 0, valend = 0;
            int hasvalue = 0;

            p = read_name(data, p, len, attrnamebuf, (int) sizeof(attrnamebuf));
            if (p == namestart) { ++p; continue; } /* unrecognized char: force progress */

            while (p < len && isspace(data[p])) ++p;
            if (p < len && data[p] == '=') {
                ++p;
                while (p < len && isspace(data[p])) ++p;
                if (p < len && (data[p] == '"' || data[p] == '\'')) {
                    unsigned char qc = data[p];
                    ++p;
                    valstart = p;
                    while (p < len && data[p] != qc) ++p;
                    valend = p;
                    if (p < len) ++p;
                } else {
                    valstart = p;
                    while (p < len && !isspace(data[p]) && data[p] != '>') ++p;
                    valend = p;
                }
                hasvalue = 1;
            }

            if (node && attrtail && attrnamebuf[0] != '\0' && attr_allowed(namebuf, attrnamebuf)) {
                struct hpbuf_s buf;
                char *decoded;

                hpbuf_init(&buf);
                if (hasvalue) decode_entities_into(&buf, data, valstart, valend);
                decoded = hpbuf_finish(&buf, NULL);

                if (decoded && strcmp(attrnamebuf, "style") == 0) {
                    char *filtered = filter_style(decoded);
                    free(decoded);
                    decoded = filtered;
                } else if (decoded && strcmp(namebuf, "a") == 0 && strcmp(attrnamebuf, "href") == 0
                           && !href_scheme_ok(decoded)) {
                    free(decoded);
                    decoded = NULL;
                }

                if (decoded) {
                    struct htmlattr *at = (struct htmlattr *) malloc(sizeof(struct htmlattr));
                    if (at) {
                        at->name = hp_strdup(attrnamebuf);
                        at->value = decoded;
                        at->next = NULL;
                        *attrtail = at;
                        attrtail = &at->next;
                    } else {
                        free(decoded);
                    }
                }
            }
        }
    }

    if (selfclose || is_void_tag(namebuf)) {
        if (node) sink_append(currentsink(st, rootsink), node);
        return p;
    }

    if (kindwanted == HPF_KEEP && !node) {
        /* node allocation failed: degrade to forwarding its children
           to the parent (HPF_UNKNOWN behavior) rather than losing
           real content or crashing */
        kindwanted = HPF_UNKNOWN;
    }

    {
        struct hpframe *f = (struct hpframe *) malloc(sizeof(struct hpframe));
        if (!f) return p; /* OOM: best-effort -- this element's content
                              attaches wherever the (unchanged) current
                              top of stack points */
        f->tag = hp_strdup(namebuf);
        f->kind = kindwanted;
        f->node = node;
        f->ownsink.head = f->ownsink.tail = NULL;
        if (kindwanted == HPF_KEEP) {
            f->sink = &f->ownsink;
        } else if (kindwanted == HPF_UNKNOWN) {
            f->sink = currentsink(st, rootsink);
        } else {
            f->sink = NULL;
        }
        hpstack_push(st, f);
    }
    return p;
}

/* ---- public entry points ---- */

struct htmlnode *htmlpart_Parse(const unsigned char *data, long len)
{
    struct childsink rootsink;
    struct hpstack st;
    long pos = 0;

    if (len < 0) len = 0;
    rootsink.head = rootsink.tail = NULL;
    hpstack_init(&st);

    while (pos < len) {
        if (data[pos] == '<' && pos + 3 < len
            && data[pos + 1] == '!' && data[pos + 2] == '-' && data[pos + 3] == '-') {
            pos = skip_comment(data, pos, len);
        } else if (data[pos] == '<' && pos + 1 < len && data[pos + 1] == '!') {
            pos = skip_bang_or_pi(data, pos, len);
        } else if (data[pos] == '<' && pos + 1 < len && data[pos + 1] == '?') {
            pos = skip_bang_or_pi(data, pos, len);
        } else if (data[pos] == '<' && pos + 1 < len && data[pos + 1] == '/') {
            pos = handle_close_tag(data, pos, len, &st, &rootsink);
        } else if (data[pos] == '<' && pos + 1 < len && isalpha(data[pos + 1])) {
            pos = handle_open_tag(data, pos, len, &st, &rootsink);
        } else {
            /* not a recognizable construct (includes a lone/stray '<'
               that isn't followed by a letter, '!', '?' or '/', and
               ordinary non-'<' text) -- handle_text_run's own
               is_markup_start check absorbs it as literal text */
            pos = handle_text_run(data, pos, len, &st, &rootsink);
        }
    }

    /* End of input with elements still open (fixture 07's class of
       imbalance: more opens than closes) -- implicitly close them all,
       innermost first, attaching whatever content they accumulated.
       Never recurses -- this is the same explicit-stack machinery as
       every other close, just driven to completion at EOF instead of
       by a matched </tag>. */
    while (st.count > 0) {
        struct hpframe *f = hpstack_pop(&st);
        if (f) finalize_pop(f, &st, &rootsink);
    }
    free(st.items);

    return rootsink.head;
}

static int freelist_push(struct htmlnode ***items, long *count, long *cap, struct htmlnode *n)
{
    if (*count >= *cap) {
        long ncap = *cap ? *cap * 2 : 64;
        struct htmlnode **ni = (struct htmlnode **) realloc(*items, ncap * sizeof(struct htmlnode *));
        if (!ni) return 0;
        *items = ni;
        *cap = ncap;
    }
    (*items)[(*count)++] = n;
    return 1;
}

void htmlpart_Free(struct htmlnode *n)
{
    struct htmlnode **items = NULL;
    long count = 0, cap = 0;
    struct htmlnode *cur;

    if (!n) return;

    for (cur = n; cur; cur = cur->next) {
        if (!freelist_push(&items, &count, &cap, cur)) { free(items); return; }
    }

    while (count > 0) {
        struct htmlnode *x = items[--count];
        if (x->type == HTMLPART_ELEMENT) {
            struct htmlnode *c;
            struct htmlattr *a;
            for (c = x->children; c; c = c->next) {
                if (!freelist_push(&items, &count, &cap, c)) { free(items); return; }
            }
            a = x->attrs;
            while (a) {
                struct htmlattr *an = a->next;
                free(a->name);
                free(a->value);
                free(a);
                a = an;
            }
            free(x->tag);
        } else {
            free(x->text);
        }
        free(x);
    }
    free(items);
}

const char *htmlpart_GetAttr(const struct htmlnode *n, const char *propname)
{
    struct htmlattr *a;
    if (!n || n->type != HTMLPART_ELEMENT) return NULL;
    for (a = n->attrs; a; a = a->next) {
        if (strcmp(a->name, propname) == 0) return a->value;
    }
    return NULL;
}

char *htmlpart_GetStyleProp(const struct htmlnode *n, const char *propname)
{
    const char *style = htmlpart_GetAttr(n, "style");
    const char *p;
    long proplen;

    if (!style) return NULL;
    proplen = (long) strlen(propname);
    p = style;
    while (*p) {
        const char *namestart = p, *nameend, *valstart, *valend;

        while (*p && *p != ':') ++p;
        nameend = p;
        if (*p == ':') ++p;
        valstart = p;
        while (*p && *p != ';') ++p;
        valend = p;

        if (nameend - namestart == proplen && strncmp(namestart, propname, (size_t) proplen) == 0) {
            char *out = (char *) malloc(valend - valstart + 1);
            if (!out) return NULL;
            memcpy(out, valstart, valend - valstart);
            out[valend - valstart] = '\0';
            return out;
        }
        if (*p == ';') ++p;
    }
    return NULL;
}
