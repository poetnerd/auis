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
	htmlatk.h -- Stage 3 of the HTML-mail-rendering project (see
		     revival/doc/html-mail-rendering-design.md): walks the
		     already-sanitized tree htmlpart_Parse() (Stage 1,
		     htmlpart.h) produces and inserts it as real ATK styled
		     text/table/image objects into a caller-supplied
		     struct text (rootEnvironment-bearing) object, instead
		     of Stage 2's (htmltext.h) flattening to a plain
		     malloc'd string. ANSI C (C89 prototypes) throughout, no
		     scanf/sscanf/fscanf anywhere -- same policy as
		     htmlpart.c/htmltext.c.

	Placement: unlike Stage 1/2 (src/ams/libs/shr, libmsshr.a -- kept
	ATK-independent so cui can link the same tree-builder/flattener
	without pulling in class.h), this module links against ATK's
	class system directly (text/environment/style/table/image), so it
	lives in src/atkams/messages/lib next to text822.c -- the exact
	place it will eventually be called from (see the Gate 5 section
	below), and a directory that already has the right ATK headers/
	libraries wired into its Imakefile. It depends on libmsshr.a
	(htmlpart.h's tree type, and htmltext_ToText() for the two cases
	documented below where this renderer deliberately reuses Stage 2's
	flattening instead of re-deriving equivalent logic) but nothing in
	libmsshr.a depends on it, so cui's ATK-independence is unaffected.

	Scope: struct htmlnode* (Stage 1's output tree) in; real content
	inserted into a caller-owned struct text object out. Does not know
	about MIME or character sets (upstream of htmlpart_Parse(), same
	as htmltext.c) and does not itself decide *which* MIME part to
	render (that is mimepart_SelectAlternative's job, upstream again,
	per the design doc's Architecture section) -- but unlike Stage 2,
	this module DOES need raw bytes for <img> content, which live in
	MIME parts this module has no access to on its own. That need is
	met by a caller-supplied resolver callback (htmlatk_ImageResolver
	below) rather than by this module reaching into MIME structures
	itself -- keeps this module's only two dependencies libmsshr.a (the
	tree type + the text fallback) and ATK's class system, not
	mimepart.c too.

	== Judgment calls (logged here per this project's habit of keeping
	   this log in the code rather than a separate report -- see
	   htmlpart.h's and htmltext.h's own such sections) ==

	Style composition via nested environments, not merged style
	objects: ATK's "environment"/"nestedmark" machinery is designed to
	represent overlapping/nested formatting directly -- text__
	ApplyEnvironment (src/atk/text/text.c) walks an environment's
	*parent* chain and layers every ancestor's style in root-to-leaf
	order before applying the environment's own style, so a style
	painted for an *inner* range automatically composes on top of
	whatever an *outer* range already painted, with no work on this
	module's part. Given that, this renderer does the simplest thing
	that uses that machinery as intended: every independent formatting
	axis (bold, italic, underline, one color, one link) gets its own
	environment_InsertStyle() span over exactly the source range that
	axis's tag/property covers, always anchored at the document's
	rootEnvironment with absolute character positions (matching most
	of text822.c's own InsertStyle call sites, e.g. its TinyStyle/
	BoldStyle header-highlighting code -- text822.c has one call site
	that nests explicitly by anchoring a second InsertStyle off the
	first call's returned environment instead of off rootEnvironment,
	which also works, but anchoring at rootEnvironment throughout is
	simpler and was the majority pattern, so that's what this module
	does uniformly). <b><i>x</i></b> therefore produces two sibling-
	in-the-call, nested-by-range environment spans (an outer Bold span
	covering the whole run, an inner Italic span covering just "x"),
	not one merged "bold-italic" style object -- and nestedmark's own
	containment logic is what makes the inner one actually nest inside
	the outer one for rendering purposes, not anything this module
	tracks itself.

	Deferred style application -- a real bug found and fixed during
	this module's own Gate-2 testing, not just a stylistic choice:
	text_AlwaysInsertCharacters() silently auto-extends any already-
	created environment_InsertStyle()/WrapStyle() span whose range
	newly-inserted characters land at or inside -- confirmed
	empirically (see the delegated-session report for the minimal
	repro): insert "hello", wrap a style over "linkA" immediately
	following it, then insert " middle" right after that span's end,
	and the span's own recorded length grows to silently swallow
	" middle" too, and keeps growing with every further insertion
	after that point, even ones nowhere near the original tag. Since
	this renderer's walk keeps inserting more document content after
	any given tag's own text is done (its siblings, its ancestors'
	remaining children, the rest of the document), naively finalizing
	each tag's style span as soon as its POST is reached -- the
	obvious, first-tried implementation -- corrupts essentially every
	multi-run document: a run's style silently bleeds forward to cover
	unrelated later text, sometimes for the rest of the document.
	Confirmed this is specifically about *interleaving* insertion with
	span creation, not about creating multiple spans under the same
	parent per se -- inserting ALL of a document's text first, with no
	span created until every character (and embedded view) is already
	in place, reproduces correctly every time, including for nested/
	overlapping spans (see the same repro file). Accordingly, this
	module never calls environment_InsertStyle/WrapStyle during the
	walk itself: PushFormattingMarks/the <a> handling only push onto
	an in-memory open-mark stack (start position + style), and
	marks_finalize (called at each tag's POST) only *records* the
	resulting (start, length, style) triple into a pending list --
	ApplyPendingStyles() is called exactly once, after the entire walk
	has finished inserting every character and every table/image view,
	and is the only place environment_InsertStyle/WrapStyle actually
	get called. This is the reason htmlatk_Render() cannot be
	incrementally observed mid-walk the way a naive implementation
	might allow -- there is no partially-styled intermediate state to
	observe, styling is genuinely all-or-nothing at the very end.

	Reuse, not per-run allocation, for the *fixed* style axes: per this
	project's own Gate-1 research finding that contrib/srctext/html/
	html.c's per-entity style_New()+style_Copy() is real, working
	precedent but heavier than needed, this module follows text822.c's
	simpler convention instead -- one static struct style* each for
	bold/italic/underline (and a couple of <font size> buckets),
	created once and reused via environment_InsertStyle() for every
	run that needs them, never copied. Arbitrary CSS color values
	(from style="color:..." or <font color>) can't be precomputed --
	there's no fixed list -- so those get a small process-lifetime
	cache (linear-scanned, capped, see ColorStyleFor() in htmlatk.c)
	keyed by the literal color string, so repeated use of the same
	color within or across messages (extremely common -- mail
	templates reuse a brand color constantly) still shares one style
	object rather than allocating fresh ones. Link styles are the one
	deliberate exception to "never copy": htmlview__Hit's precedent
	(and this module's own Gate-4 click handling, see htmlatk_LinkAt()
	below) needs to recover *which* URL a given clicked run pointed at,
	which means the URL has to live somewhere per-occurrence, not on a
	shared style object -- so link styles get a style_Copy() of a
	shared template style plus a style_AddAttribute(..., "href", ...)
	every time, mirroring html.c's own precedent for exactly this
	need. Links are also far less frequent per message than inline
	bold/italic runs, so the per-occurrence allocation cost this
	avoids elsewhere is not a concern here.

	Table-cell content IS full general recursion into this same
	renderer -- an earlier version of this module got this wrong, via
	a mutually-exclusive CELL_PLAIN/CELL_HAS_TABLE/CELL_HAS_IMG
	dispatch (a ClassifyCell() per <td>) that treated a cell's content
	as a scalar choice between "plain text" or "exactly one embedded
	object", which mangles the extremely common real pattern of a
	caption/paragraph with an inline image or icon in it (everything
	in the cell but whichever branch ClassifyCell() picked first was
	silently dropped). A design review grounded this in real ATK
	precedent: PAPERS/atk/Sherman.Alloc lines
	663-1066 is a genuine `table` object datastream dump whose second
	data cell (line 675) is `\begindata{text,...}` containing prose,
	then an inline embedded `calc` spreadsheet view mid-paragraph
	(`\view{calcv,...}`), then more prose after it, all inside one
	cell's own text object -- i.e. a table cell's content is an
	arbitrary nested document, exactly like the top-level message body
	is, not a special restricted case of it. (Contrast the *first* data
	cell, line 668: also `\begindata{text,...}`, but happens to contain
	only a single centered/bold heading run -- still a full text
	object, just one whose content happens to be simple. There is no
	separate scalar cell-content representation for the "happens to be
	simple" case in the real format at all.)

	So BuildTableGrid() (htmlatk.c) now does the obviously-general
	thing instead of the three-way dispatch: for any non-empty <td>/
	<th>, table_Imbed(T, "text", &cellchunk) creates a fresh "text"
	dataobject for that cell, and this module's own htmlatk_Render()
	is called recursively on the cell's children into that object at
	position 0, threading the same resolver/rock through. Nested
	<table>s and <img>s inside a cell fall out for free as ordinary
	embedded views within that recursive call -- exactly how they are
	already handled at the top level of the document -- with no
	per-cell content-type dispatch left in BuildTableGrid() at all;
	the old CELL_HAS_TABLE case's hand-rolled "find the nested <table>
	node and recurse BuildTableGrid() directly into its cell" logic is
	gone too, subsumed by the same general path (a nested <table> is
	just another tag htmlatk_Render() walks into, which reaches
	RenderTable() the same way the top-level walk does).

	table_TextCell (the table object's own scalar spreadsheet-string
	cell type, '-prefixed for "literal string, not formula" -- see
	src/atk/table/table.ch's struct cell union) is no longer used by
	this renderer at all, for any cell, including simple single-run
	text cells like Sherman.Alloc's own first cell above -- the general
	recursive "text" dataobject path handles those correctly too (a
	one-run text object with no embedded views), and unlike the old
	table_TextCell path it does not need the '-prefix escaping
	convention, which was a real latent bug this fix incidentally
	closes: a genuine HTML cell whose flattened text happened to start
	with '=' or otherwise contain the table object's own formula syntax
	was never actually safe going through htmltext_ToText() + a raw
	'-prefix concatenation, since nothing in that path escaped a stray
	embedded quote or verified the flattened text couldn't itself be
	misparsed by the spreadsheet formula reader on a subsequent
	ReadASCII/edit round-trip. htmltext_ToText() itself (Stage 2) is
	consequently no longer called anywhere in this module -- grepped
	to confirm no other call site existed -- so the htmltext.h include
	was removed from htmlatk.c along with it.

	Empty cells (no children at all, or only whitespace-only text
	nodes -- CellIsEmpty() in htmlatk.c) are the one deliberate
	exception left to "always recurse": they are left as the
	table_EmptyCell every freshly-grown cell already defaults to
	(confirmed by reading table__ChangeSize's CreateCell(T,cell,NULL)
	path in src/atk/table/table.c, which sets celltype = table_EmptyCell
	for every newly-grown cell before any content is placed), rather
	than paying for a table_Imbed() + a real "text" dataobject with
	nothing in it. This is a fast path for a genuinely trivial case,
	not a reintroduction of the old scalar/dispatch mistake -- every
	cell with *any* real content, however simple, goes through the one
	general recursive path uniformly.

	Table-nesting recursion uses real (bounded) C call frames, not an
	explicit heap stack, UNLIKE htmlpart.c's tree builder and
	htmltext.c's tree walker. Deliberate, and different from the rest
	of this project's no-C-recursion discipline: a nested <table> can
	only appear one level deeper than its enclosing <table><tr><td>,
	so the recursion depth is bounded by how much markup accompanies
	each level (that three-tag minimum, plus whatever content the
	author put in the cell) -- not by document *size* the way
	htmlpart.c's original concern was (a flat run of thousands of
	unclosed <span>s costs only bytes, not tags-around-a-table). The
	real fixture corpus's worst case is 28 levels (see
	revival/tests/html-fixtures/README.md); even three orders of
	magnitude beyond that is a trivial amount of C stack. If this ever
	proves to be a real adversarial-input concern in practice, it
	should convert to an explicit stack the same way Stage 1/2 did --
	flagged here rather than silently assumed safe forever. Since the
	table-cell-content fix above, this recursion runs through one more
	indirection per level than it originally did: a nested <table>
	inside a cell no longer reaches BuildTableGrid() via BuildTableGrid()
	calling itself directly (that was specific to the old CELL_HAS_TABLE
	case) -- it reaches it via this module's own htmlatk_Render() being
	called on the cell's content, walking into the <table> tag, and
	calling RenderTable() the same way the top-level document walk
	does. Same call-frame bound (still one C stack frame per nesting
	level, still governed by markup structure not document size), just
	one more frame per level (BuildTableGrid -> htmlatk_Render ->
	RenderTable -> BuildTableGrid) than the direct self-call took.

	Remote/cid image resolution is the caller's job, not this module's:
	htmlatk_ImageResolver is a callback the caller supplies (NULL is
	valid and means "never resolve -- always placeholder", which is
	what every Gate 2-4 standalone test in this module uses, since the
	real fixture corpus's HTML bodies were harvested without their
	sibling MIME parts -- see revival/tests/html-fixtures/README.md --
	so there is nothing to resolve cid: references against outside a
	live message anyway). Per the design doc's remote-image policy,
	a resolver a real caller (Gate 5's text822.c) supplies is expected
	to always decline http(s):// sources unless the user has opted in,
	and to always resolve cid: sources against the already-decoded
	MIME part they reference -- this module enforces neither policy
	itself, it just calls whatever resolver it was given.

	popen() argument safety: the design doc's Links section describes
	the click action as `popen("open <url>", "r")`, mirroring
	htmlview__Hit's URL-echoing precedent. Taken completely literally
	that is a shell-injection hazard: Stage 1's sanitization restricts
	the URL's *scheme* to http/https/mailto (see htmlpart.h) but does
	not -- and has no reason to -- restrict shell metacharacters
	elsewhere in the URL, since nothing about scheme-checking a URL is
	supposed to also be a shell-safety pass. htmlatk_LaunchURL() still
	uses popen() (matching the design doc's chosen mechanism) but
	single-quotes the URL and escapes any embedded single quote
	(the standard '\'' trick) before handing it to `open`, so the
	shell never sees an unescaped user-controlled byte. This is a
	hardening of the design doc's literal pseudocode, not a deviation
	from its intent, and is called out here rather than silently
	patched in without comment.

	CSS color normalization: style="color:.../background-color:..."
	and <font color> values were originally passed to style_AddAttribute
	verbatim. That is wrong for any CSS syntax XParseColor -- the real
	name/hex resolver every ATK color eventually goes through, see
	src/atk/basics/x/xcmap.c:180's xcolormap__SetColor -- cannot parse:
	`rgb()`/`rgba()` functional syntax (real fixture 15 has
	`rgba(27,27,27,0.65)`), and CSS-only keywords like `inherit`/
	`currentColor`/`transparent` (fixture 15 also has `color:inherit`).
	Both previously reached XParseColor verbatim and silently failed
	(a stderr print, no color applied). CssColorToX11() in htmlatk.c
	now normalizes: `inherit`/`initial`/`unset`/`currentColor`/
	`transparent`/empty return NULL (no cascade exists here to inherit
	*from*, so "apply no override" is correct, not a guess);
	`rgb()`/`rgba()` with plain integer components convert to
	"#RRGGBB" with alpha dropped (no alpha channel on ATK text color,
	same "opaque" degradation used elsewhere in this module);
	percentage components (`rgb(50%,20%,10%)`, not seen in the real
	corpus but valid CSS) are deliberately NOT guessed at -- also NULL,
	rather than silently rendering black; anything else (a bare name,
	or already-X11-native hex) passes through unchanged, so it fails
	(or succeeds) exactly the way it already did before this function
	existed. Verified directly against the real corpus's exact
	`rgb()`/`rgba()`/`inherit` values plus a synthetic sweep covering
	transparent/currentColor/#RGB/#RRGGBB/a named color/a malformed
	percentage form -- every case produced the intended result with no
	styling bleed into neighboring runs.

	A real, adjacent bug this surfaced (fixed alongside it, not left
	for later): marks_push() returning "did I actually push?" used to
	be silently discarded -- PushFormattingMarks() always incremented
	its local `count` regardless of whether marks_push() pushed
	anything, which was harmless while AttrColorStyleFor() only ever
	returned NULL for a genuinely empty value (rare), but would have
	been a real corruption bug once `inherit`/`transparent`/etc.
	(common in real mail) started returning NULL routinely: an
	inflated count fed into marks_finalize() at the tag's POST pops
	that many marks regardless of how many were truly pushed for this
	tag, silently consuming an *enclosing* or *earlier* tag's still-open
	mark instead and corrupting that unrelated span. marks_push() now
	returns 1/0 and every call site accumulates the real count via
	`count += marks_push(...)` instead of an unconditional `++count`.
	Confirmed via the same synthetic sweep above (adjacent spans/plain
	runs around every no-color case came back with exactly the expected
	boundaries, no bleed).

	== Gate 5 (not implemented by this module) ==

	Wiring this into src/atkams/messages/lib/text822.c (replacing the
	current mimepart_HtmlToText-based shim) and the renderer-level
	whole-message fallback contract (this renderer fails => the whole
	message falls back to htmltext_ToText(), never a half-rendered
	document) are Gate 5 work, out of scope for this module itself --
	see revival/doc/html-atk-renderer-prompt.md. htmlatk_Render()'s
	`ok` result flag (see below) is the hook Gate 5 needs for that
	fallback decision; this module does not implement the fallback
	policy itself, only reports success/failure honestly.
*/

#ifndef HTMLATK_H
#define HTMLATK_H

#include <htmlpart.h>
#include <text.ih>

/* Resolves an <img>'s already-sanitized src attribute value (as-is
   from htmlpart_GetAttr(imgnode, "src") -- may be a "cid:..." URI, an
   http(s):// URL, or anything else a sender's markup put there; this
   module does not interpret the string itself, the resolver owns that
   entirely) to raw, not-yet-decoded image bytes plus the MIME content
   type they should be interpreted as (e.g. "image/jpeg") -- the same
   two inputs image__ReadOtherFormat()/dataobject_ReadOtherFormat()
   need (see src/atk/basics/common/image.c's ReadOtherFormat contract).
   Returns TRUE and fills bytesOut, lenOut, and mimetypeOut (all three
   caller-owned mallocs the renderer frees after use) on success;
   returns FALSE (leaving the out-parameters untouched) if the image
   can't or shouldn't be resolved -- remote content the caller's
   fetch-policy declined, a cid: reference with no matching MIME part,
   any I/O failure, etc. A NULL resolver is valid and equivalent to a
   resolver that always returns FALSE (always placeholder) -- see this
   header's own note above on why every standalone test in this module
   uses NULL. rock is an opaque caller value threaded through
   unexamined (typically the enclosing message's MIME part list). */
typedef boolean (*htmlatk_ImageResolver)(void *rock, const char *src,
    unsigned char **bytesOut, long *lenOut, char **mimetypeOut);

/* Renders root (a top-level sibling list as returned by
   htmlpart_Parse(), or NULL) by inserting styled text/table/image
   content into dest starting at character position pos. dest is a
   plain struct text* -- deliberately not struct text822* -- so this
   module has no dependency on text822.ch and can be built/tested
   fully standalone; text822 (or any other simpletext-family class)
   works unmodified since AlwaysInsertCharacters/AlwaysAddView/
   rootEnvironment are all inherited, unoverridden, methods/fields (see
   this header's own placement note above). Does not free or otherwise
   read root beyond walking it -- same read-only convention as
   htmltext_ToText().

   Returns TRUE if the whole tree was rendered (possibly with
   individual per-element degradations already logged inline the way
   Stage 1/2 degrade -- an unsupported image format placeholdering is
   success, not failure, exactly as it is for htmltext_ToText()).
   Returns FALSE only for a real construction failure this module
   cannot recover from partway through (e.g. table_New()/style_New()
   returning NULL -- ATK object allocation failing) -- per the design
   doc's renderer-level fallback contract, a FALSE return means the
   caller (Gate 5's text822.c) should discard/ignore whatever was
   partially inserted and fall back to htmltext_ToText() for the
   *whole* message, never show a half-rendered document. This module
   does not implement that discard/fallback itself -- dest is a live
   ATK object mid-edit by the time a failure could be noticed, and
   unwinding a partial edit safely belongs with whoever owns dest's
   undo/edit-transaction semantics (Gate 5), not this module.

   *lengthOut, if non-NULL, receives the number of characters/view-
   slots inserted (dest's length increased by exactly this much) --
   useful for a caller doing its own position bookkeeping afterward,
   same shape as text822.c's own ShowPos convention. */
boolean htmlatk_Render(struct text *dest, long pos,
    const struct htmlnode *root,
    htmlatk_ImageResolver resolver, void *resolverRock,
    long *lengthOut);

/* Gate-4 click handling. htmlatk_Render() applies a distinct
   (underlined, per the design doc) style to every rendered <a href>
   run, via a style object carrying a private "href" attribute (see
   this header's own note above on why link styles are copied, not
   shared). htmlatk_LinkAt() walks the environment chain at document
   position pos (innermost outward, via environment_GetInnerMost() then
   environment_GetParent() -- exactly htmlview__Hit's/html__GetAnchorDest's
   precedent, see src/contrib/srctext/html/html.c) looking for such a
   style, and returns a malloc'd copy of its href (caller frees) if
   found, or NULL if pos isn't inside a link run at all (including the
   case where the original <a> had no href attribute -- Stage 1 leaves
   the <a> node itself in the tree with no href in that case, see
   htmlpart.h, and this renderer does not apply a link style to an
   anchor with no href to link to). t must be the same dest object (or
   share its rootEnvironment) that htmlatk_Render() inserted into. */
char *htmlatk_LinkAt(struct text *t, long pos);

/* Shell-safely launches url (already scheme-restricted to http/https/
   mailto by Stage 1, see this header's own popen-safety note above)
   via `open` (macOS LaunchServices), matching the design doc's Links
   section. No return value -- matches htmlview__Hit's own
   fire-and-forget precedent; failures are not surfaced (macOS `open`
   itself pops a Finder dialog on a URL it can't handle, which is
   already adequate user feedback for that case, same as it would be
   for any other agent invoking `open`). NULL-safe (does nothing). */
void htmlatk_LaunchURL(const char *url);

#endif /* HTMLATK_H */
