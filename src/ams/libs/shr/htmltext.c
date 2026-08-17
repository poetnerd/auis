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
	htmltext.c -- see htmltext.h for the API and the rendering-
		     decision log. ANSI C (C89 prototypes) throughout; no
		     scanf/sscanf/fscanf anywhere.

	Walks the tree with an explicit heap-allocated, realloc-grown
	stack of "visit this node" work items, each tagged PRE or POST
	(pushed in that order around a node's children, the standard
	iterative pre-and-post-order trick) -- never C recursion, same
	discipline and same reasoning as htmlpart.c's own tree-builder
	and htmlpart_Free(). List numbering/indent state (which <ol>/<ul>
	a given <li> is nested under) is carried on a second small
	explicit stack, pushed/popped in lockstep with the main walk
	visiting a <ul>/<ol> node's PRE/POST items -- correct regardless
	of what's interleaved between them, since both stacks are driven
	by the same strictly-nested LIFO traversal.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>	/* strncasecmp */
#include <ctype.h>

#include <htmlpart.h>
#include <htmltext.h>

/* ---- growable output byte buffer -- same shape as htmlpart.c's
   private hpbuf_s, reimplemented here since that one is static to
   htmlpart.c and not part of its public API ---- */

struct htbuf { char *data; long len; long cap; };

static void htbuf_init(struct htbuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void htbuf_putc(struct htbuf *b, int c)
{
    if (b->len + 1 >= b->cap) {
        long ncap = b->cap ? b->cap * 2 : 256;
        char *nb = (char *) realloc(b->data, ncap);
        if (!nb) return; /* best-effort: drop the byte rather than abort,
                             matching htmlpart.c's hpbuf_putc OOM policy */
        b->data = nb;
        b->cap = ncap;
    }
    b->data[b->len++] = (char) c;
}

static void htbuf_putn(struct htbuf *b, const char *s, long n)
{
    long i;
    for (i = 0; i < n; ++i) htbuf_putc(b, (unsigned char) s[i]);
}

static void htbuf_puts(struct htbuf *b, const char *s)
{
    while (*s) htbuf_putc(b, (unsigned char) *s++);
}

static char *htbuf_finish(struct htbuf *b)
{
    htbuf_putc(b, '\0');
    if (!b->data) {
        char *empty = (char *) malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    return b->data;
}

/* ---- spacing/break primitives -- see htmltext.h's "Whitespace"
   rendering-decision note for what these implement ---- */

static void rstrip_trailing_spaces(struct htbuf *b)
{
    while (b->len > 0 && (b->data[b->len - 1] == ' ' || b->data[b->len - 1] == '\t')) {
        --b->len;
    }
}

/* Ensures the buffer ends with a single line break (one '\n'), unless
   the buffer is still empty (no leading break on empty output) or
   already ends with one. Used for line-level breaks (list items,
   table rows/cells, definition terms) -- never adds a second '\n', so
   consecutive LINE-class elements stack one-per-line with no blank
   line between them. */
static void ensure_line_break(struct htbuf *b)
{
    rstrip_trailing_spaces(b);
    if (b->len == 0) return;
    if (b->data[b->len - 1] != '\n') htbuf_putc(b, '\n');
}

/* Ensures the buffer ends with exactly two line breaks (one blank
   line), unless still empty. Already-present trailing newlines beyond
   two are left alone (never subtracted), so this only ever pads up to
   two, which is what makes nested PARA-class elements finishing at
   the same tree position collapse to a single blank line rather than
   stacking one per ancestor. */
static void ensure_para_break(struct htbuf *b)
{
    long n, cnt = 0;

    rstrip_trailing_spaces(b);
    if (b->len == 0) return;
    n = b->len;
    while (n > 0 && b->data[n - 1] == '\n') { ++cnt; --n; }
    while (cnt < 2) { htbuf_putc(b, '\n'); ++cnt; }
}

/* A space is needed before the next token only if the buffer is
   non-empty and doesn't already end in whitespace -- avoids doubled
   spaces where adjacent inline runs (two text nodes either side of an
   <a>/<b>/<span> boundary, or a synthesized token like an image
   placeholder or a link's "(url)" suffix) meet. */
static void maybe_space(struct htbuf *b)
{
    if (b->len > 0 && !isspace((unsigned char) b->data[b->len - 1])) {
        htbuf_putc(b, ' ');
    }
}

/* 0xA0 (Latin-1 non-breaking space, what &nbsp; decodes to) is a
   valid, displayable character in ATK's 8-bit Latin-1 text model --
   see mimepart.h's mimepart_Utf8ToLatin1() comment -- so this isn't
   about charset support. It's collapsed like ordinary whitespace
   anyway because the one thing that distinguishes it from a plain
   space -- resisting line-wrap -- is meaningless here: this renderer
   does no line-wrapping to protect against. Email templates use
   &nbsp; constantly as spacer padding; without this, isspace() alone
   leaves those spacer runs as stray visible characters. */
static int is_collapsible_space(unsigned char c)
{
    return isspace(c) || c == 0xA0;
}

/* Emits len bytes of already entity-decoded text content. Outside
   <pre> (in_pre == 0), any run of source whitespace collapses to at
   most one output space, itself suppressed if the buffer already ends
   in whitespace -- see htmltext.h's "Whitespace" note. Inside <pre>
   (in_pre != 0), bytes are copied through verbatim. */
static void emit_text_run(struct htbuf *b, const char *text, long len, int in_pre)
{
    long i;

    if (in_pre) {
        htbuf_putn(b, text, len);
        return;
    }
    i = 0;
    while (i < len) {
        unsigned char c = (unsigned char) text[i];
        if (is_collapsible_space(c)) {
            while (i < len && is_collapsible_space((unsigned char) text[i])) ++i;
            if (b->len > 0 && !is_collapsible_space((unsigned char) b->data[b->len - 1])) {
                htbuf_putc(b, ' ');
            }
        } else {
            htbuf_putc(b, (int) c);
            ++i;
        }
    }
}

/* ---- ordered/unordered list nesting state, pushed/popped in
   lockstep with <ul>/<ol> PRE/POST -- see file header comment ---- */

struct htx_listframe { int ordered; int counter; };

struct htx_state {
    struct htbuf buf;
    struct htx_listframe *list;
    long listcount, listcap;
    int predepth;	/* >0 while inside one or more nested <pre> */
};

static void listctx_push(struct htx_state *st, int ordered)
{
    if (st->listcount >= st->listcap) {
        long ncap = st->listcap ? st->listcap * 2 : 16;
        struct htx_listframe *ni = (struct htx_listframe *)
            realloc(st->list, ncap * sizeof(struct htx_listframe));
        if (!ni) return; /* best-effort: nesting/numbering degrades for
                             this branch rather than aborting the walk */
        st->list = ni;
        st->listcap = ncap;
    }
    st->list[st->listcount].ordered = ordered;
    st->list[st->listcount].counter = 0;
    ++st->listcount;
}

static void listctx_pop(struct htx_state *st)
{
    if (st->listcount > 0) --st->listcount;
}

static struct htx_listframe *listctx_top(struct htx_state *st)
{
    return (st->listcount > 0) ? &st->list[st->listcount - 1] : NULL;
}

/* ---- per-tag special-case emitters ---- */

static void emit_img_placeholder(struct htbuf *b, const struct htmlnode *n)
{
    const char *alt = htmlpart_GetAttr(n, "alt");

    maybe_space(b);
    htbuf_puts(b, "[image");
    if (alt && alt[0]) {
        htbuf_puts(b, ": ");
        emit_text_run(b, alt, (long) strlen(alt), 0);
    }
    htbuf_puts(b, "]");
}

static void emit_hr(struct htbuf *b)
{
    ensure_para_break(b);
    htbuf_puts(b, "----------------------------------------");
    ensure_para_break(b);
}

static void emit_href_suffix(struct htbuf *b, const struct htmlnode *n)
{
    const char *href = htmlpart_GetAttr(n, "href");

    if (!href || !href[0]) return;
    maybe_space(b);
    htbuf_putc(b, '(');
    htbuf_puts(b, href);
    htbuf_putc(b, ')');
}

/* <li> marker/indent: "- " under <ul>, sequential "N. " (per-<ol>
   counter, reset at each <ol>) under <ol>; 2 extra spaces of indent
   per level of list nesting beyond the outermost. See htmltext.h's
   "<li>" rendering-decision note. */
static void emit_li_marker(struct htx_state *st)
{
    struct htx_listframe *top = listctx_top(st);
    long depth = st->listcount;
    long indent = (depth > 1) ? (depth - 1) * 2 : 0;
    long i;
    char numbuf[32];

    ensure_line_break(&st->buf);
    for (i = 0; i < indent; ++i) htbuf_putc(&st->buf, ' ');
    if (top && top->ordered) {
        ++top->counter;
        sprintf(numbuf, "%d. ", top->counter);
        htbuf_puts(&st->buf, numbuf);
    } else {
        htbuf_puts(&st->buf, "- ");
    }
}

/* PARA-class tags handled generically (no extra state beyond the
   blank-line break itself) -- ul/ol/li/dt/dd/table/tr/td/th/pre/a are
   all special-cased individually in do_pre_action/do_post_action
   instead, since each needs more than just a break. */
static int tag_is_para(const char *t)
{
    return strcmp(t, "p") == 0 || strcmp(t, "div") == 0 || strcmp(t, "blockquote") == 0
        || strcmp(t, "h1") == 0 || strcmp(t, "h2") == 0 || strcmp(t, "h3") == 0
        || strcmp(t, "h4") == 0 || strcmp(t, "h5") == 0 || strcmp(t, "h6") == 0
        || strcmp(t, "dl") == 0;
}

/* Suppressed entirely -- node and children never walked. See
   htmltext.h's "head and title are suppressed" rendering-decision
   note. */
static int tag_is_suppressed(const char *t)
{
    return strcmp(t, "head") == 0 || strcmp(t, "title") == 0;
}

/* Case-insensitive substring test, local to this file -- same
   reasoning as htmlatk.c's own copy (not shared across translation
   units, each renderer is independent by design). */
static int htx_value_contains_ci(const char *hay, const char *needle)
{
    size_t hlen = strlen(hay), nlen = strlen(needle);
    size_t i;
    if (nlen == 0 || nlen > hlen) return 0;
    for (i = 0; i + nlen <= hlen; ++i) {
        if (strncasecmp(hay + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

/* `display:none`/`visibility:hidden` -- see htmlatk.c's own
   NodeIsStyleHidden for the full story (the "hidden preheader" trick
   in commercial marketing email: a <div style="display:none"> whose
   text is meant only for the email client's inbox-preview line, never
   the opened message). Same fix, same reasoning, mirrored here since
   this plain-text renderer is a fully independent module from
   htmlatk.c, not because the bug is different. */
static int NodeIsStyleHidden(const struct htmlnode *n)
{
    char *sv;
    int hidden = 0;

    sv = htmlpart_GetStyleProp(n, "display");
    if (sv) { if (htx_value_contains_ci(sv, "none")) hidden = 1; free(sv); }
    if (!hidden) {
        sv = htmlpart_GetStyleProp(n, "visibility");
        if (sv) { if (htx_value_contains_ci(sv, "hidden")) hidden = 1; free(sv); }
    }
    return hidden;
}

static void do_pre_action(struct htmlnode *n, struct htx_state *st)
{
    const char *t = n->tag;

    if (strcmp(t, "br") == 0) {
        rstrip_trailing_spaces(&st->buf);
        htbuf_putc(&st->buf, '\n');
        return;
    }
    if (strcmp(t, "hr") == 0) { emit_hr(&st->buf); return; }
    if (strcmp(t, "img") == 0) { emit_img_placeholder(&st->buf, n); return; }

    if (strcmp(t, "ul") == 0) { ensure_para_break(&st->buf); listctx_push(st, 0); return; }
    if (strcmp(t, "ol") == 0) { ensure_para_break(&st->buf); listctx_push(st, 1); return; }
    if (strcmp(t, "li") == 0) { emit_li_marker(st); return; }
    if (strcmp(t, "dt") == 0) { ensure_line_break(&st->buf); return; }
    if (strcmp(t, "dd") == 0) { ensure_line_break(&st->buf); htbuf_puts(&st->buf, "    "); return; }

    if (strcmp(t, "table") == 0) { ensure_para_break(&st->buf); return; }
    if (strcmp(t, "tr") == 0 || strcmp(t, "td") == 0 || strcmp(t, "th") == 0) {
        ensure_line_break(&st->buf);
        return;
    }

    if (strcmp(t, "pre") == 0) { ensure_para_break(&st->buf); ++st->predepth; return; }

    if (tag_is_para(t)) { ensure_para_break(&st->buf); return; }

    /* a, span, b, strong, i, em, u, tt, code, font, html, body, thead,
       tbody: inline or transparent -- no break of their own, children
       just flow into the surrounding context */
}

static void do_post_action(struct htmlnode *n, struct htx_state *st)
{
    const char *t = n->tag;

    if (strcmp(t, "a") == 0) { emit_href_suffix(&st->buf, n); return; }

    if (strcmp(t, "ul") == 0 || strcmp(t, "ol") == 0) {
        ensure_para_break(&st->buf);
        listctx_pop(st);
        return;
    }
    if (strcmp(t, "li") == 0 || strcmp(t, "dt") == 0 || strcmp(t, "dd") == 0) {
        ensure_line_break(&st->buf);
        return;
    }
    if (strcmp(t, "table") == 0) { ensure_para_break(&st->buf); return; }
    if (strcmp(t, "tr") == 0 || strcmp(t, "td") == 0 || strcmp(t, "th") == 0) {
        ensure_line_break(&st->buf);
        return;
    }
    if (strcmp(t, "pre") == 0) { --st->predepth; ensure_para_break(&st->buf); return; }
    if (tag_is_para(t)) { ensure_para_break(&st->buf); return; }

    /* inline/transparent: nothing to undo */
}

/* ---- explicit heap-allocated walk stack (no C recursion) ---- */

struct htx_walkitem { struct htmlnode *node; int post; };

struct htx_walkstack { struct htx_walkitem *items; long count, cap; };

static void walkstack_push1(struct htx_walkstack *ws, struct htmlnode *n, int post)
{
    if (ws->count >= ws->cap) {
        long ncap = ws->cap ? ws->cap * 2 : 256;
        struct htx_walkitem *ni = (struct htx_walkitem *)
            realloc(ws->items, ncap * sizeof(struct htx_walkitem));
        if (!ni) return; /* best-effort: this branch stops walking rather
                             than aborting the whole render */
        ws->items = ni;
        ws->cap = ncap;
    }
    ws->items[ws->count].node = n;
    ws->items[ws->count].post = post;
    ++ws->count;
}

/* Pushes a sibling chain onto ws in reverse order, so popping visits
   them in original (document) order -- same iterative-preorder trick
   htmlparttest.c's push_siblings_reversed uses (and for the same
   reason: no C recursion, and a temp array first so the reversal
   doesn't require walking the list twice with pointer juggling). */
static void walkstack_push_siblings(struct htx_walkstack *ws, struct htmlnode *first)
{
    struct htmlnode **tmp = NULL;
    long tc = 0, tcap = 0, i;
    struct htmlnode *s;

    for (s = first; s; s = s->next) {
        if (tc >= tcap) {
            long ncap = tcap ? tcap * 2 : 64;
            struct htmlnode **ni = (struct htmlnode **) realloc(tmp, ncap * sizeof(struct htmlnode *));
            if (!ni) { free(tmp); return; }
            tmp = ni;
            tcap = ncap;
        }
        tmp[tc++] = s;
    }
    for (i = tc - 1; i >= 0; --i) {
        walkstack_push1(ws, tmp[i], 0);
    }
    free(tmp);
}

/* Trims trailing whitespace/blank lines from the finished buffer and
   leaves it ending in exactly one newline, or empty if there was
   nothing but whitespace to begin with. */
static void trim_trailing(struct htbuf *b)
{
    while (b->len > 0) {
        char c = b->data[b->len - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') { --b->len; continue; }
        break;
    }
    if (b->len > 0) htbuf_putc(b, '\n');
}

char *htmltext_ToText(const struct htmlnode *root)
{
    struct htx_state st;
    struct htx_walkstack ws;
    char *result;

    htbuf_init(&st.buf);
    st.list = NULL;
    st.listcount = 0;
    st.listcap = 0;
    st.predepth = 0;

    ws.items = NULL;
    ws.count = 0;
    ws.cap = 0;

    /* root is caller-owned/read-only (see htmltext.h); the walk only
       ever reads through it, but the stack items need non-const
       pointers to match struct htmlnode's own (non-const) ->next/
       ->children fields, same cast htmlparttest.c's build_walk makes
       for the same reason. */
    walkstack_push_siblings(&ws, (struct htmlnode *) root);

    while (ws.count > 0) {
        struct htx_walkitem item = ws.items[--ws.count];
        struct htmlnode *n = item.node;

        if (!item.post) {
            if (n->type == HTMLPART_TEXT) {
                emit_text_run(&st.buf, n->text, n->textlen, st.predepth > 0);
                continue;
            }
            /* ELEMENT */
            if (tag_is_suppressed(n->tag)) continue; /* no children walked, no POST */
            if (NodeIsStyleHidden(n)) continue; /* no children walked, no POST */
            do_pre_action(n, &st);
            walkstack_push1(&ws, n, 1);
            if (n->children) walkstack_push_siblings(&ws, n->children);
        } else {
            do_post_action(n, &st);
        }
    }

    free(ws.items);
    free(st.list);

    trim_trailing(&st.buf);
    result = htbuf_finish(&st.buf);
    return result;
}
