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
	htmlpart.h -- HTML mail body tokenizer/sanitizer.  ANSI C (C89
		     prototypes) throughout, no scanf/sscanf/fscanf
		     anywhere (numbers -- there are none of interest
		     here, but where digit runs need scanning, plain
		     char-at-a-time loops are used, same policy as
		     mimepart.c/overhead/mail/lib/imap_prot.c, the
		     tree's other born-ANSI modules).

	Scope: turns raw text/html MIME-part bytes (already MIME/QP/
	base64-decoded by mimepart.c -- this module knows nothing about
	MIME) into a simplified, already-sanitized tree: element nodes
	(tag name + an allowlisted, already-filtered attribute list)
	and text nodes (entities decoded).  Sanitization -- which
	elements/attributes survive at all -- happens inline during
	tree-building, per the explicit allowlist in
	revival/doc/html-mail-rendering-design.md's "Sanitization"
	section, so every consumer of this tree (a future plain-text
	renderer, a future ATK-styled renderer) sees the same
	already-safe input; neither renderer re-derives the allowlist.

	Does not know about ATK, class.h, or the "text"/"environment"
	display machinery -- lives in libmsshr.a next to mimepart.c
	specifically so it can link into both atkams/messages/lib and
	ams/msclients/cui without pulling in ATK.  Does not lay out or
	render anything -- no box model, no table geometry, no image
	decoding, no click handling.  Callers decide what to do with
	the tree (walk it into flowed text, walk it into ATK insets,
	etc.) -- that is Stage 2/3 of the HTML-mail-rendering project,
	not this module's job.

	== Sanitization allowlist (enforced here, not by callers) ==

	Elements kept as ELEMENT nodes: html, head, body, title (kept
	in the tree, not auto-suppressed -- "read, not rendered" is a
	renderer-level decision, see the design doc), p, div, span, br,
	hr, a, b, strong, i, em, u, tt, code, pre, blockquote, h1-h6,
	ul, ol, li, dl, dt, dd, table, thead, tbody, tr, td, th, img,
	font.

	Elements dropped with their contents (no node, contents not
	walked into the tree at all): script, style, meta, link,
	iframe, object, embed, form, input, select, button, textarea.
	script/style/textarea are additionally handled as opaque
	"raw text" spans (scanned for their literal closing tag, same
	technique as mimepart_HtmlToText's <script>/<style> skip) so
	that a literal '<' inside embedded JS/CSS can never desync the
	tokenizer or swallow a real subsequent tag.

	Elements not on either list above: skipped structurally --
	no node is created for the tag itself, but its children are
	still walked and attached to *its* parent, so an unrecognized
	wrapper element does not eat its contents.  This is the direct
	fix for the htmlview <!DOCTYPE> bug class this project exists
	to not repeat: an unrecognized construct must never become an
	entity that is pushed and never popped.  <!DOCTYPE ...> and
	<!-- comments --> are recognized explicitly (not by falling
	into "unknown element") and are skipped without being pushed
	onto anything at all.

	Attributes kept, per element (everything else on any element,
	including class/id/on*, is discarded silently):
	  a:      href (only if the value's scheme is http:, https:,
	          or mailto: -- anything else, including no scheme at
	          all, e.g. a relative link or bare "#anchor", is
	          stripped; the <a> node survives with no href)
	  img:    src, alt, width, height
	  table, td, th: colspan, rowspan, border
	  font:   color, size
	  any kept element: style (see below)

	style="..." is parsed for a small fixed property allowlist --
	color, background-color, font-weight, font-style,
	text-decoration, display, visibility, font-size, font-family --
	everything else in the value is dropped.
	No selectors, no cascade, no !important: it's a flat per-
	element property bag, canonicalized into that fixed property
	order (not source order) with last-value-wins if a property is
	declared more than once in the same attribute.  If nothing in
	the value matches the allowlist, the style attribute is omitted
	entirely rather than kept empty.  Use htmlpart_GetStyleProp()
	rather than re-splitting the string by hand.

	Numeric-looking attribute values (colspan, width, border, ...)
	are passed through as opaque strings, unparsed -- this module
	does not call atoi/strtol on them.  Turning them into numbers
	is a renderer-level concern (and per this tree's scanf policy,
	should use strtol, not sscanf, when it happens).

	== No fixed size or depth caps ==

	Per the design doc's "No fixed size caps" section: real mail
	routinely nests tables far deeper than any "reasonable-sounding"
	guess (28 levels, confirmed in the fixture corpus), and
	dataobj.c's own recursive datastream reader has never needed a
	depth cap.  This module accordingly does NOT recursion-descend
	per nesting level: htmlpart_Parse() tracks open elements with
	an explicit heap-allocated, realloc-grown stack, and
	htmlpart_Free() frees the resulting tree with an explicit
	heap-allocated work list rather than recursing into ->children
	-- freeing a several-thousand-level-deep tree would blow the C
	stack exactly as building it recursively would, so both
	directions avoid per-level C recursion, not just the builder.
	See revival/tests/html-parse-tests's deep-nesting case, the
	regression test for this decision.

	A parse that is simply *slow* (not deep, not large -- slow) is
	a renderer-level fallback trigger per the design doc, not this
	module's concern; htmlpart_Parse() always runs to completion.
*/

#ifndef HTMLPART_H
#define HTMLPART_H

/* Node kind. ELEMENT nodes carry tag/attrs/children; TEXT nodes carry
   text/textlen. A node is never both. */
enum htmlnode_type { HTMLPART_ELEMENT = 1, HTMLPART_TEXT = 2 };

struct htmlattr {
    char *name;			/* lowercased attribute name */
    char *value;		/* value, entities decoded, quotes
				   stripped; never NULL (a valueless
				   boolean attribute, e.g. bare
				   "border", stores "") */
    struct htmlattr *next;
};

struct htmlnode {
    enum htmlnode_type type;
    char *tag;			/* ELEMENT only: lowercased tag name;
				   NULL for TEXT */
    struct htmlattr *attrs;	/* ELEMENT only: already-filtered
				   allowlisted attributes, in the order
				   they appeared in the source; NULL if
				   none survived (or none were given) */
    char *text;			/* TEXT only: malloc'd, NUL-terminated,
				   entities already decoded; NULL for
				   ELEMENT */
    long textlen;		/* TEXT only: byte length of text, not
				   counting the NUL */
    struct htmlnode *children;	/* ELEMENT only: first child, or NULL */
    struct htmlnode *next;	/* next sibling at this nesting level
				   (either node type); NULL after the
				   last one */
};

#define htmlpart_IsElement(n) ((n)->type == HTMLPART_ELEMENT)
#define htmlpart_IsText(n)    ((n)->type == HTMLPART_TEXT)

/* Tokenizes and sanitizes data[0..len) (raw HTML bytes -- need not be
   NUL-terminated, len is authoritative, embedded NULs are passed
   through as literal bytes same as mimepart's byte-buffer handling).
   Returns the first node of the top-level sibling list (a fragment
   may have several top-level siblings, e.g. text before any tag, or a
   <body> soup with no enclosing <html>) or NULL for an empty/entirely-
   dropped document. Never returns NULL due to malformed markup alone
   -- mismatched/unclosed tags degrade gracefully (see the design
   doc's "Sanitization" section and fixtures 07/08 in
   revival/tests/html-fixtures): an unmatched close tag is ignored, a
   never-closed open tag is implicitly closed at end of input, with
   whatever content it accumulated by then. Returns NULL only on
   allocation failure or a genuinely empty/all-dropped input. */
struct htmlnode *htmlpart_Parse(const unsigned char *data, long len);

/* Frees a node list (siblings and, for ELEMENT nodes, children,
   recursively in effect but implemented iteratively -- see the
   header comment above on the no-C-recursion requirement). NULL-safe. */
void htmlpart_Free(struct htmlnode *n);

/* Case-sensitive lookup by attribute name (names are already
   lowercased at parse time, so pass a lowercase name). Returns NULL
   if the node has no such attribute (either it was never present, or
   it was present but not on the allowlist and so never stored).
   ELEMENT nodes only. */
const char *htmlpart_GetAttr(const struct htmlnode *n, const char *propname);

/* Convenience accessor for a single property out of an already-
   filtered style="..." attribute (see the allowlist above). propname
   must be one of "color", "background-color", "font-weight",
   "font-style", "text-decoration", "display", "visibility",
   "font-size", "font-family" (case-sensitive, matching
   htmlpart_Parse's canonical lowercase output); anything else always
   returns NULL. Returns a malloc'd, NUL-terminated copy of the
   property's value (caller frees -- unlike htmlpart_GetAttr, this one
   extracts a substring rather than handing back an owned field, so it
   follows mimepart.c's other "standalone decode primitive" ownership
   convention, e.g. mimepart_DecodeQP, not mimepart_GetParam's
   borrowed-pointer one), or NULL if the node has no style attribute,
   or the given property wasn't present in it. Equivalent to, but
   safer than, hand-splitting htmlpart_GetAttr(n, "style") on ';'/':'. */
char *htmlpart_GetStyleProp(const struct htmlnode *n, const char *propname);

#endif /* HTMLPART_H */
