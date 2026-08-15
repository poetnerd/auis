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
	htmltext.h -- Stage 2 of the HTML-mail-rendering project (see
		     revival/doc/html-mail-rendering-design.md): walks the
		     already-sanitized tree htmlpart_Parse() (Stage 1,
		     htmlpart.h) produces and flattens it into plain text.
		     ANSI C (C89 prototypes) throughout, no scanf/sscanf/
		     fscanf anywhere, no C recursion anywhere -- same
		     policy as htmlpart.c, for the same reason (a cell in
		     the deepest fixture's 28-level table nesting is still
		     an ordinary node in this tree; this module has to walk
		     it without blowing the C stack exactly as htmlpart.c
		     had to build and free it without blowing the C stack).

	Scope: struct htmlnode* (Stage 1's output tree) in, malloc'd
	NUL-terminated char* (caller frees, same ownership convention as
	mimepart_HtmlToText and htmlpart_GetStyleProp) out. Does not know
	about MIME, character sets, ATK, or click handling -- those are,
	respectively, upstream of htmlpart_Parse(), upstream of it again,
	Stage 3's job, and Stage 3's job. Per the design doc's Architecture
	section, this is "used unconditionally by cui; used by text822.c
	as the fallback path" -- both future wiring, not part of this
	patch (see revival/doc/html-mail-rendering-design.md's "Fallback
	strategy" section for when/why text822.c will fall back to this
	renderer instead of the Stage-3 ATK-styled one).

	This is the intended eventual replacement for mimepart_HtmlToText
	(src/ams/libs/hdrs/mimepart.h) -- that function's own doc comment
	already calls itself "the deliberately dumb strip shim" scheduled
	for replacement. The call shape differs because the work is now
	split in two: where mimepart_HtmlToText takes raw HTML bytes
	directly, a caller here first runs htmlpart_Parse() (Stage 1) and
	hands the resulting tree to htmltext_ToText() (this module) --
	splitting "tokenize+sanitize" from "flatten to text" is the whole
	point of the two-stage design, since Stage 3 (the ATK-styled
	renderer) needs to consume the exact same Stage-1 tree without
	going through this text path at all. A future text822.c/cui.c
	call site looks like:

	    struct htmlnode *root = htmlpart_Parse(bytes, len);
	    char *text = htmltext_ToText(root);
	    ... use text ...
	    free(text);
	    htmlpart_Free(root);

	instead of today's one-call mimepart_HtmlToText(bytes, len).

	== Rendering decisions (judgment calls, logged here per the
	   project's habit of keeping this log in the code, not a
	   separate report -- see htmlpart.h's own such section) ==

	Block vs. inline: p, div, blockquote, h1-h6, dl, ul, ol, table,
	pre force a paragraph break (a blank line) before and after;
	li, dt, dd, tr, td, th force a single line break (no blank line)
	before and after -- so table rows/cells and list items stack
	tightly, one per line, rather than each opening its own paragraph;
	html, body, thead, tbody are transparent (no break of their own,
	children just flow); span, b, strong, i, em, u, tt, code, font, a
	are inline (no break at all). Breaks collapse: nested block
	elements finishing back-to-back (e.g. a <div> whose last child is
	a <p>) never produce more than one blank line, however many block
	ends coincide at that point in the tree.

	head and title are suppressed entirely (node and children never
	walked) -- the design doc calls "read, not rendered" a renderer-
	level decision for title; this renderer makes it by not emitting
	title text at all, since real mail's <title> is boilerplate
	("Untitled Document", a marketing subject restated) that adds
	nothing read as plain text. (In practice head's only surviving
	child is title anyway -- meta/link/script/style are dropped by
	Stage 1 before this module ever sees the tree -- so suppressing
	head is equivalent to suppressing title, but title is also
	special-cased directly in case a malformed document puts it
	somewhere else.)

	<br> emits a literal newline unconditionally (not collapsed the
	way paragraph breaks are) -- <br><br> is a real mail author's
	deliberate way of asking for a blank line, so it is honored
	literally rather than folded down to a single line break the way
	two adjacent <p>s finishing at the same point would be.

	<hr> emits a fixed 40-character dashed line, paragraph-broken on
	both sides -- this is a plain-text terminal target, not a GUI, so
	a literal rule character is what "visual separator" can mean here
	(see the design doc's Non-goals: no box model, no layout engine).

	<img> becomes "[image: <alt text>]" if alt is non-empty, else
	"[image]" -- alt text itself has its internal whitespace collapsed
	the same as ordinary text content, since alt attributes in real
	mail templates are sometimes multi-line. No image bytes are ever
	touched here (see htmlpart.h's own note that this module doesn't
	decode images -- neither does this one; Stage 3 is where
	image__ReadOtherFormat comes in, per the design doc's Images
	section).

	<a href="..."> renders its rendered inner content, then " (url)"
	immediately after (e.g. "click here (https://example.com/x)") --
	an improvement on mimepart_HtmlToText, which drops link targets
	entirely (see that function's own doc comment). If href was
	stripped by Stage 1's scheme allowlist (javascript:, a bare
	fragment, etc. -- see htmlpart.h), htmlpart_GetAttr() returns NULL
	here and no "(url)" suffix is added; the anchor's inner text still
	renders normally. An anchor with no inner text but a surviving
	href (e.g. an image-only link) renders as "(url)" alone, or
	"[image] (url)" if the image placeholder above supplied the
	visible token -- not specially suppressed, since a bare URL is
	still useful plain-text output.

	<li> gets a "- " marker under <ul>, a sequential "N. " marker
	(count resets to 1 at each <ol>, does not attempt to track
	a/i/A/I list-style-type since Stage 1 doesn't even keep that
	attribute) under <ol>; nesting under an outer list adds 2 spaces
	of indent per extra level. List items are not blank-line
	separated from each other (see "line break" above) to keep long
	lists compact -- only the list as a whole gets a surrounding
	blank line. The indent/marker is only applied at the start of the
	item; if an item's own content is itself multi-line (e.g. a
	nested <p> inside an <li>), continuation lines are not re-indented
	-- deliberately not implemented, to avoid a general line-wrapping/
	reflow engine this module has no other reason to have. <dt> gets
	no indent; <dd> gets a flat 4-space indent, same reasoning.

	<table>/<tr>/<td>/<th> degrade to one cell's text per line (not
	tab/space-joined into one row line) -- chosen over column-joining
	because real mail tables routinely have cells containing multiple
	sentences (see the fixture corpus), which would make a joined-row
	line unreadably long without a real column layout to break it
	back up, and this module deliberately does not attempt visual
	column alignment (see the design doc's Table strategy section --
	that alignment is Stage 3's ATK table object's job, not this
	one's). Rows are separated from each other by a single line break
	(not a blank line, to avoid a large table turning into mostly
	vertical whitespace); the table as a whole gets a surrounding
	blank line, which also separates back-to-back tables from each
	other. colspan/rowspan/border are not consulted here at all (no
	merged-cell reconstruction, no border rendering) -- cosmetic-only
	attributes per htmlpart.h, meaningless outside a real grid.

	Whitespace: outside <pre>, any run of source whitespace (space,
	tab, CR -- already normalized to LF by Stage 1 -- newline) inside
	a text node collapses to at most one output space, and that space
	is suppressed entirely if the output already ends in whitespace
	(including a line break just emitted for a block element) -- so
	the routine "\n  \n" text nodes real mail HTML leaves between
	tags never produce visible extra spacing. Inside <pre>, text is
	copied through byte-for-byte, whitespace and all -- <pre> is
	still paragraph-broken from its surroundings, just not
	whitespace-collapsed internally. The final output has any
	trailing whitespace/blank lines trimmed and ends in exactly one
	newline (or is the empty string, for an empty/all-suppressed
	input) -- never returns NULL (mirrors mimepart_HtmlToText's
	"never NULL" contract).
*/

#ifndef HTMLTEXT_H
#define HTMLTEXT_H

#include <htmlpart.h>

/* Renders root (a top-level sibling list as returned by
   htmlpart_Parse(), or NULL) into malloc'd, NUL-terminated plain text.
   Never returns NULL (an empty/all-suppressed tree yields a malloc'd
   ""); caller frees the result. Does not free or otherwise modify the
   tree -- root is read-only to this function, same convention as
   htmlpart_GetAttr()/htmlpart_GetStyleProp(). Iterative throughout,
   via an explicit heap-allocated stack -- see this header's own
   comment above and htmlpart.h's "No fixed size or depth caps"
   section for why. */
char *htmltext_ToText(const struct htmlnode *root);

#endif /* HTMLTEXT_H */
