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

	lset-based table reflow (BuildLsetGrid/RenderTableAsLset in
	htmlatk.c), added as a follow-up to the original table/spread-only
	design above -- see revival/doc/lset-table-reflow-gate1-report.md
	for the full research trail this implementation is based on.
	table/spread is a fixed-pixel-column model (ATK_DEFAULT_COLUMN_
	THICKNESS, table.ch) that never reflows on window resize; lset/
	lpair (src/atk/adew/lset.c, src/atk/supportviews/lpair.c) is a tree
	of binary, PERCENTAGE-mode splits that genuinely does (lpair.c:385,
	objcvt recomputed from a live percentage on every DesiredSize
	pass). This is now the default path for an HTML <table>; table/
	spread is kept, unmodified, as the fallback for the one shape lset
	structurally cannot represent.

	Routing rule (TableNeedsGridFallback, htmlatk.c): a table routes to
	the OLD table/spread path if and only if it has any colspan>1 or
	rowspan>1 cell anywhere in its own rows (not a descendant/ancestor
	table's), or is structurally degenerate (no row with any real
	<td>/<th>). Otherwise it routes to the new lset path. This is a
	per-<table>-node decision, not a per-document one: a span-free
	outer wrapper table can (and in the real fixture corpus, routinely
	does) route to lset while a colspan-bearing table nested many
	levels inside one of its cells independently routes to table/
	spread, and vice versa -- confirmed against real fixtures 09/12/16
	(the corpus's only colspan-bearing fixtures): the specific nested
	tables carrying colspan="3"/"8"/"12"/"2" cells show up as real
	table_ImbeddedObject NESTED-TABLE structures in htmlatktest.test's
	dump output at whatever depth they actually occur, while every
	span-free table around them -- including the top-level document
	wrapper in all three fixtures -- uses lset. table/spread's own
	JOINED/SetInterior spanning mechanism has no lset equivalent
	(lpair is a strict two-child guillotine split, confirmed in Gate 1;
	there is no "this region covers two rows" concept anywhere in lset/
	lpair), so a table that needs it structurally cannot use the new
	path at all -- this is not a heuristic tuned against the corpus,
	it is the only choice lset's own data model allows.

	N-way splits as a chain of binary splits, and the percentage
	assigned at each link (LsetChainPct, htmlatk.c): lset/lpair only
	ever splits one region into two (Gate 1, section 1.1), so an N-cell
	row (or an N-row table) is built as a right-leaning chain of N-1
	lset split nodes: node 0's left is leaf 0, node 0's right is node 1
	(covering leaves 1..N-1), node 1's left is leaf 1, node 1's right
	is node 2, and so on, the last node's right being leaf N-1 directly
	(no further node needed for N==1). Traced against real lpair.c
	source (not assumed from the symmetric N=2 case Gate 1's own probe
	tested): lsetv.c's initkids() passes ls->pct straight through to
	lpair's HSplit/VSplit as the *second* positional argument's share
	-- lpair.c:498-510's PERCENTAGE branch sets objsize[1] (the second,
	right child) to exactly pct, and ComputeSizesFromTotal (lpair.c:
	373-398) gives the first (left) child whatever total space is left
	over. So ls->pct is unambiguously "how much of this split's own
	space goes to ls->right", not a symmetric or left-biased value.
	Given that, and wanting every leaf to end up with an equal 1/N
	share of the whole chain's space (this module's chosen v1 default,
	see the width-hints paragraph below for why), each link's pct must
	give its right subtree (which still contains `remaining-1` not-yet-
	peeled leaves) exactly (remaining-1)/remaining of that link's own
	space, so that -- inductively, one level of recursion at a time --
	each remaining leaf keeps getting an equal share of whatever space
	it was handed. That formula, LsetChainPct(remaining) = round(100 *
	(remaining-1) / remaining), was checked with a standalone offline
	harness (mirrors lpair.c's own integer division exactly, including
	its floor-not-round objcvt arithmetic) before being wired into
	htmlatk.c, computing the actual final leaf-pixel breakdown for
	N=2..5 against several total widths (900/1000/800/601, chosen to
	include both evenly- and unevenly-divisible cases): every case sums
	back to exactly the input total (no lost or double-counted pixels
	across the whole chain) and every leaf lands within 1-4px of a
	perfectly equal 1/N share, e.g. N=5/total=1000px gives leaf widths
	[200,200,198,201,201] against an ideal of 200.00 each. Rounding to
	the nearest integer percentage point (rather than truncating, the
	naive choice) roughly halves the worst-case per-leaf deviation
	versus the ideal share in that same harness -- confirmed, not
	assumed. BuildLsetChain itself builds this chain with a plain
	backwards for-loop, deliberately NOT the bounded-C-recursion this
	file already uses for nested <table> depth (see that section's own
	note above): a single row's cell *count* is bounded only by how
	much markup a sender puts in one <tr>, which -- unlike table-
	nesting depth -- scales with raw document content, not structure,
	so it gets the same explicit-iteration treatment htmlpart.c/
	htmltext.c already use for genuinely document-size-bound walks.

	Per-column/per-cell HTML width hints are deliberately NOT consulted
	for the lset path's percentages (equal-split is this module's v1
	default, unconditionally) -- unlike the table/spread path just
	above, which does read the table-level width= attribute (Parse
	WidthPixels/HTML_TABLE_DEFAULT_WIDTH). Checked against the real
	fixture corpus before deciding, not assumed: no fixture anywhere in
	revival/tests/html-fixtures/ uses a per-<td> width= or a <col>/
	<colgroup> width hint (grepped for both); the only width attribute
	seen anywhere is a whole-table width="100%"/width="NNN", which says
	nothing about relative *column* proportions in the first place --
	there is no real signal in this corpus this module could act on
	even if it tried. If a future fixture or live message shows a real
	per-column width pattern, this is the place to add it (thread a
	per-leaf weight through BuildLsetCell/BuildLsetChain instead of the
	uniform "1 of N" LsetChainPct(remaining) call), flagged here rather
	than silently left unconsidered.

	A visible thin resize-divider bar between every lset-split region
	is an inherent, unavoidable property of this construction, not a
	defect introduced by this module: lsetv.c's initkids() (the code
	that turns a saved lset tree back into live views on open, Gate 1
	section 1.1) hardcodes moveable=TRUE on every HSplit/VSplit call --
	`lsetview_HSplit(self,v1,v2,ls->pct,TRUE)` -- with no data-level
	knob this module's construction code can set to suppress it (the
	`struct lset` data section, lset.ch, has no such field at all).
	table/spread's border="0" -> GHOST handling (BuildTableGrid, this
	file) has no lset equivalent for the same reason colspan/rowspan
	doesn't: there is nothing in lset's own data model to carry that
	information even if this module wanted to set it. This is a real,
	visible difference from a genuine CSS-style borderless layout
	table and is called out here deliberately rather than silently
	discovered later; it was not treated as a blocking design fork
	because it follows directly from the lset/lpair mechanism Gate 1
	already evaluated and recommended, and no per-instance API exists
	in this class to work around it from htmlatk.c without modifying
	lset/lpair/lsetview themselves, which is out of this module's
	scope.

	htmlatktest.test's dump/writeds/roundtrip subcommands needed no
	*new* subcommands to exercise the lset path -- the same fixture-
	file-in, real-datastream-or-greppable-dump-out shape already
	proves it -- but DumpStyledText/DumpCellTextContent's embedded-view
	scan and DumpTableCell's table_ImbeddedObject case both needed a
	third branch alongside their existing "table"/"text" class checks
	(a new DumpLsetTree, mutually recursive with both, since a lset
	leaf's own content is "text" that can itself embed a nested table
	or a nested lset, exactly symmetric with a table cell's "text"
	content being able to embed a nested lset -- both nesting
	directions are real, confirmed against fixtures 09/12/16 above, not
	just the shallow case a single new branch would have covered).

	CORRECTION, found via live testing in ez (not caught by any
	offline dump/writeds/roundtrip check -- those only prove
	construction/serialization round-trips correctly, not that the
	object actually draws its content at a sensible size): the first
	version of BuildLsetGrid wrapped each row's cell-split lset in one
	more outer lset (lsetview_MakeVert) to stack the rows into a
	single merged tree, mirroring wdc's original proposed shape
	literally. Live-tested in ez against a real message
	(revival/render_test.ez, the National Grid mail used throughout
	Stage 3), it rendered as a wall of empty horizontal divider lines
	with no visible text, even though the underlying datastream
	genuinely contained the real cell text (confirmed offline via
	roundtrip -- CELL-TEXT-CONTENT entries up to 615 chars long were
	present and intact; this was a display bug, not a missing-data
	bug). Root cause, traced directly in
	src/atk/supportviews/lpair.c's lpair__DesiredSize (lines 300-370):
	its two branches are NOT symmetric despite looking parallel. The
	lpair_VERTICAL branch (side-by-side children -- what
	lsetview_MakeHorz/our per-row cell split compiles down to,
	lpair.c:331-353) genuinely asks each child for its desired height
	at a given width and returns max(d0,d1): content-aware, correct.
	The lpair_HORIZONTAL branch (stacked children -- what
	lsetview_MakeVert/the old row-stacking layer compiled down to,
	lpair.c:354-364) does NOT sum its children's desired heights at
	all -- it takes whatever `height` it was *given*, splits that by
	percentage (lpair_ComputeSizesFromTotal), and reports back
	`(height > 2048) ? STARTHEIGHT : height`, i.e. it echoes the input
	or caps at a hardcoded 256px constant, never actually querying its
	row children's real content height. This is correct behavior for
	lset's original design target (splitting an already-fixed-size
	interactive window pane, where "total size" is externally given
	and stays fixed) and wrong for a content-driven inset embedded
	inline in flowing text, where the height has to be derived from
	content, bottom-up. No amount of correct pct-formula or routing
	logic in this module could have fixed this -- it's one level
	below, in lpair itself, and out of this module's scope to change.

	Fix, now implemented: stop using lset/lpair for the row-stacking
	dimension entirely. It never needed percentage-based reflow in the
	first place -- rows should just stack at their natural height,
	unconditionally, regardless of window width, which is exactly what
	the surrounding `text` object's own per-line flow already does
	correctly for every other block this renderer inserts (paragraphs,
	RenderImageInline's images, RenderTable's whole table object).
	BuildLsetGrid now takes an out-param and fills it with one lset
	root per row (cells only, lsetview_MakeHorz -- the dimension that
	IS content-aware) instead of building and returning one merged
	tree; RenderTableAsLset inserts each row as its own
	text_AlwaysAddView call at successive text positions, separated by
	EnsureLineBreak, the same idiom every other block-level insertion
	in this file already uses. Re-verified after the fix: same offline
	suite (16 fixtures, 3 regression suites, writeds/roundtrip) all
	still green, and revival/render_test.ez regenerated for wdc to
	re-check live in ez. General lesson for this project, not just
	this bug: offline writeds/roundtrip proves construction and
	serialization are correct, but does not prove a view actually
	sizes/draws itself sensibly -- that still needs a real live check
	in ez/messages, the same lesson Stage 3's original
	`\begindata{table,...}`-visible-as-text corruption bug already
	taught, now confirmed a second time for a completely different
	failure mode (wrong size instead of wrong bytes).

	SECOND CORRECTION, same live-testing cycle, found immediately after
	the one above: wdc re-checked the per-row-insertion fix in ez and
	reported the divider lines were gone (expected -- see the "visible
	resize-divider bar" note above, a per-lset-split property, not a
	per-row-stacking one, so removing the row-stacking lpair layer
	removes exactly one divider per table, the one that used to run
	between rows) but ALSO no table content at all rendered past the
	first line of plain body text -- worse than the first bug, not
	better. Root-caused by going straight to the same offline dump
	tool rather than guessing from the live symptom (per this file's
	own stated methodology): `htmlatktest.test dump` on the raw fixture
	showed `RUN[65,66) style=VIEW ...` immediately followed by
	`RUN[66,67) style=VIEW ...` -- the two row-views were sitting at
	back-to-back text positions with NOTHING between them, not even a
	newline run. RenderTableAsLset's loop called EnsureLineBreak(st)
	before each row after the first, but EnsureLineBreak's own guard
	(`if (st->trailingNL == 0)`) was reading stale state: the loop set
	`st->trailingNL = 0` only once, after the whole loop finished, not
	after each individual row insertion. So on the first
	inter-row EnsureLineBreak call, trailingNL was still whatever it
	had been left at BEFORE the table started (2, from the paragraph
	break the intro text ended with) -- EnsureLineBreak saw a nonzero
	value and, believing a break already existed, inserted nothing.
	Every row after the first landed on the exact same text position as
	the row before it, with no line break to force it onto its own
	line -- explaining both symptoms at once (no dividers, since
	dividers were correctly removed by the first fix; no visible
	content, since stacking N views with zero separation onto one
	logical line is not a state a text view's line-layout code is
	built to display sensibly). Fixed by moving `st->trailingNL = 0;
	st->anyContent = 1;` inside the loop, right after each row's own
	text_AlwaysAddView call, mirroring exactly how InsertLiteral/
	FlushPendingSpace update this same state immediately alongside
	their own insertion rather than deferring it. Verified two ways
	before calling this fixed: (1) re-ran the same dump command and
	confirmed a real `RUN[...) style=(none) text="\n"` run now sits
	between every pair of adjacent row-views; (2) re-ran the full
	offline suite again (16 fixtures, 3 regression suites) -- all still
	green. revival/render_test.ez regenerated a second time; NOT yet
	re-confirmed live in ez as of this writing. Sharpens the general
	lesson above: it's not just that offline testing can't prove a view
	draws at a sensible size -- it also could have caught THIS bug
	(missing separator between two adjacent embedded views is a pure
	text-position fact, no X11 needed) if the dump output had been
	actually read after the first fix instead of only re-running the
	pass/fail suite and trusting a byte-count match. Read the dump
	output, not just its exit code.

	THIRD CORRECTION, same live-testing cycle, this one outside this
	module entirely: wdc re-checked the trailingNL fix above in ez and
	reported real progress (the first row's real content -- an image
	alt-text block -- finally showed) but everything after it was
	still missing, plus a large blank gap before it. Investigation
	(with wdc pointing at real prior art in the tree --
	src/contrib/mit/neos/eosaux.c's eos__InitializeObject and
	src/atk/supportviews/bpair.c -- and then at src/atk/adew/cel.c/
	celv.c) found the actual root cause one level below this module,
	in lsetview itself (src/atk/adew/lsetv.ch/.c): it has NO
	DesiredSize override at all, so it inherits lpair's unmodified --
	and lpair__DesiredSize only ever examines self->obj[0]/obj[1] (an
	internal split node's two children); a bare LEAF lsetview (any
	single cell -- self->mode != lsetview_IsSplit, obj[0]/obj[1] both
	NULL, real content living in self->child instead) falls through to
	lpair's generic fallback, which echoes back whatever height it was
	asked for rather than ever consulting self->child's real content
	size. This is NOT specific to how htmlatk.c builds its lset trees
	-- it's a genuine gap in lsetview itself, confirmed three
	independent ways (a full manual trace through initkids/makeview/
	dolink, checking lsetv.ch's override list directly, and reading
	eosaux.c's real fixed-pixel-chrome usage pattern, which sizes
	panes by construction-time constants specifically because nothing
	in lpair/lsetview can derive a size from real content). The fix
	(implemented on the html branch, same session, in lsetv.ch/.c, NOT
	in this file) was modeled directly on celview__DesiredSize
	(celv.c), a sibling adew class that already solves exactly this
	problem correctly for its own single-child case: a new
	lsetview__DesiredSize override that forwards straight to
	self->child's real DesiredSize whenever this is a leaf hosting
	real content, falling through to super_DesiredSize (lpair's
	existing, unchanged split-partitioning behavior) for genuine
	internal split nodes or a not-yet-linked/empty leaf. Purely
	additive -- no existing lset/Arbcon behavior for split nodes
	changes. Requires classpp regeneration (lsetv.eh/.ih) and a
	rebuild+install+md5-verify of lset.do/lsetv.do specifically (not
	text822.do this time -- a different, shared, dynamically-loaded ATK
	class), same install-then-verify discipline as every other fix in
	this project. This module's own construction code
	(BuildLsetGrid/RenderTableAsLset/BuildLsetChain/BuildLsetCell)
	needed NO changes for this fix -- it was already producing the
	right tree shape; the missing piece was one level below, in the
	toolkit itself. As of this writing, re-verified offline (16
	fixtures + 3 regression suites still green -- expected, since this
	fix only affects live view geometry, not dataobject construction/
	serialization, so offline dump/writeds/roundtrip cannot exercise it
	at all) but NOT yet re-confirmed live in ez.

	FOURTH CORRECTION, same live-testing cycle, also outside this
	module, in lsetview again: wdc confirmed the DesiredSize fix above
	worked (text blocks show at their real size and resize with the
	window) but reported a large, still-unfixed amount of blank space
	between blocks -- enough that scrolling between them takes a full
	screen at a time -- and the scrollbar itself doesn't reflect the
	true content size either. Root cause: lsetview_WantNewSize
	(lsetv.c) only ever handled the "redraw myself" half of a
	size-change notification (self->mode = lsetview_UpdateView plus a
	same-level lsetview_WantUpdate call); it never escalated to the
	real WantNewSize chain that tells an ENCLOSING container "I've
	changed size, please re-layout/re-reserve space around me."
	view__WantNewSize (view.c, the true base default) climbs exactly
	one level to self->parent when self==requestor; celview__WantNewSize
	(celv.c) calls this via super_WantNewSize(self,self) in addition to
	its own bookkeeping -- lsetview never made that call at all. Net
	effect: once a leaf's real content linked/loaded and its
	DesiredSize started reporting a real (usually much smaller) height,
	nothing ever told the embedding text object -- which had already
	reserved space for that leaf based on an earlier, pre-content
	guess -- to re-ask and shrink its reservation, and the scrollbar
	(sized from that same stale reservation) never updated either. Same
	failure shape as the DesiredSize gap, same fix pattern: added
	super_WantNewSize(self,self) to lsetview__WantNewSize, modeled on
	celview's already-working equivalent, alongside (not replacing) the
	existing local mode/WantUpdate handling. Rebuilt+installed+
	md5-verified lsetv.do again (lset.do unaffected, only lsetv.c
	changed this time). Re-verified offline (16 fixtures + 3 regression
	suites still green, same reasoning as above -- this is a live
	view-geometry fix, offline tooling can't exercise it) but NOT yet
	re-confirmed live in ez.

	FIFTH CORRECTION / methodology note, same cycle: after the fourth
	correction wdc reported "still looks the same." Live lldb tracing
	(register reads on lsetview__DesiredSize's entry args, since this
	build has no DWARF line info -- see sonnet-playbook.md's lldb
	notes) initially appeared to show a genuine infinite loop:
	lsetview__DesiredSize firing hundreds of times a second, cycling
	through a small fixed set of self pointers with identical
	width/height each time. Root-caused this to the same missing-
	debounce gap celview__WantNewSize (celv.c) already guards against
	via its own sizepending field/check -- lsetview_WantNewSize
	escalated unconditionally on every call, with nothing to stop a
	downstream re-layout reaction from re-triggering another
	escalation before the first was serviced. Added an equivalent
	sizepending field to lsetview (lsetv.ch data: section) and the
	same guard/reset pair celview uses (WantNewSize only escalates
	`if (!self->sizepending)`, then sets it TRUE; DesiredSize resets it
	FALSE at the top, since a real query means whatever was pending has
	been serviced). This is a real, additive hardening worth keeping
	regardless of what's below.

	HOWEVER: re-testing after this fix showed the SAME apparent loop
	pattern still occurring under lldb (now cycling a larger ~57-object
	set instead of ~6, same rough call rate) -- but a follow-up test
	running ez PLAINLY in the background, no debugger attached, for 15+
	seconds showed ZERO calls to lsetview__DesiredSize after the
	initial render and the process sitting fully idle at 0% CPU. This
	strongly suggests the apparent "infinite loop" was an LLDB
	ARTIFACT (the batch script's rapid breakpoint-hit -> register-read
	-> continue cycle interacting with X11 in some way that generates
	synthetic re-dispatch, not a real runaway loop in the app itself),
	not a genuine bug -- real, undebugged execution shows no ongoing
	work at all once the initial layout completes, which is the
	CORRECT steady-state behavior. This was not fully re-confirmed with
	a clean return-value trace (attempting to capture the actual
	dWidth/dHeight results via `finish` inside a scripted lldb
	breakpoint-command sequence hit real tooling friction -- `finish`
	resumes the target asynchronously, which aborts the remaining
	queued commands in that same script invocation, corrupting
	subsequent convenience-variable state -- a genuine limitation of
	this scripted-breakpoint-command technique worth remembering for
	future sessions, not yet solved). Also newly noted for future
	sessions: invoking plain `ez` (no absolute path) under lldb from
	this checkout resolved to a DIFFERENT checkout's binary
	(andrew-6.4/build/bin/ez was found first on PATH) -- this did NOT
	actually invalidate the test, since ANDREWDIR (which was set
	correctly to this checkout's build/) governs dynamic .do loading
	independent of which copy of the runapp/ez executable itself is
	running (confirmed: the breakpoint resolved to freshly-compiled
	code matching this session's own edits, disassembling to the exact
	`self->sizepending = FALSE` store this fix added) -- but it's a
	sharp edge worth flagging so a future session doesn't waste time
	chasing a phantom "wrong build" theory the way this one almost did.

	Net state as of this writing: the sizepending debounce fix is
	real, correct, and kept, but whether it (or anything else) actually
	resolves wdc's visual complaint (large gaps between blocks,
	scrollbar not reflecting true content size) has NOT been
	re-confirmed live -- the lldb investigation above answered "is
	there a runaway loop" (apparently no, in real use) but not "why are
	the gaps still there," which remains the open question for the
	next live check.

	SIXTH CORRECTION, found live 2026-08-16 -- this is the one that
	actually explains the gaps, found from wdc's own diagnostic
	instinct rather than another blind source-reasoning pass: wdc
	reported that clicking into one of the large blank gaps highlighted
	a big solid black zone, "as if we missed eliminating an empty text
	zone" -- and sent a screenshot confirming exactly that (a large
	solid black rectangle sitting right where an empty spacer row
	should be, directly below a real content block). This is
	lsetview__Update's own literal rendering for a focused, genuinely
	empty leaf (lsetv.c): `if(self->HasFocus)
	lsetview_SetTransferMode(self,graphic_BLACK); ...
	lsetview_EraseVisualRect(self);` -- Arbcon's built-in "this is an
	empty, ready-to-paste pane, click here" indicator, never meant to
	appear in read-only rendered mail. Root cause, found by re-reading
	BuildLsetCell (this file) against the DesiredSize fix from the
	third correction above: that fix's forwarding condition is
	`self->mode != lsetview_IsSplit && self->child`, and BuildLsetCell
	deliberately left a genuinely-empty cell (CellIsEmpty) as a bare
	leaf with dobj==NULL/viewname=="" (modeled on table_EmptyCell's
	zero-overhead philosophy) -- meaning self->child is NEVER created
	for an empty cell (lsetv.c's makeview() declines for an empty
	viewname), so an empty leaf ALWAYS falls through to lpair's
	original, still-broken DesiredSize fallback regardless of the
	third correction's fix -- that fix only ever covered non-empty
	leaves. Real marketing/newsletter email is full of near-invisible
	spacer/padding rows (confirmed throughout revival/tests/
	html-fixtures/), so this gap hit constantly and is the actual,
	dominant source of the reported large blank areas -- not a
	remaining DesiredSize/WantNewSize propagation issue as the fourth/
	fifth corrections above assumed (those fixes are still real and
	correct, just not what was causing THIS symptom). Fix: BuildLsetCell
	now always creates a real "text" dataobject and attaches it as
	leaf->dobj/viewname, even for an empty cell (leaving the text
	object's own content empty in that case rather than skipping
	object creation entirely) -- self->child is therefore never NULL
	for any real cell, so the third correction's DesiredSize fix always
	applies, and an empty text view's own natural height is small/
	correct (the same as any other blank line elsewhere in this
	renderer, never a reported bug) -- the minimal-footprint outcome
	table_EmptyCell's zero-overhead design was trying to approximate a
	different, lset-incompatible way. Verified: re-ran writeds+roundtrip
	on the National Grid fixture -- LEAF (empty) count went from 8 to 0
	and CELL-TEXT-CONTENT len=0 appeared 8 times in its place (same 8
	cells, now real empty text objects instead of bare dobj==NULL
	leaves); all 16 fixtures + all 3 regression suites still green;
	revival/render_test.ez regenerated. NOT yet re-confirmed live in
	ez, but this is the first fix in this whole lset-table-reflow
	investigation that was actually confirmed against a real,
	wdc-provided screenshot rather than source-reasoning alone --
	notably higher confidence than the fourth/fifth corrections above.

	SEVENTH addition, requested live 2026-08-16, same session: wdc
	confirmed the sixth correction fixed the black-box rendering, but
	pointed out (with a screenshot, plus a follow-up note that the
	ATK caret needed many ^N presses to step down through the
	remaining blank area between real content blocks) that consecutive
	blank spacer rows should coalesce into a single blank line, the
	way HTML/browsers collapse redundant whitespace -- otherwise N
	stacked blank spacer <tr>s (a common real-mail padding pattern,
	confirmed throughout revival/tests/html-fixtures/) become N
	stacked blank lines. Added RowIsEntirelyBlank() (a row has >=1
	real <td>/<th> and every one is CellIsEmpty) and coalescing logic
	in BuildLsetGrid's row loop: a run of 2+ consecutive entirely-blank
	rows collapses to just the first; a single isolated blank row is
	left alone (real, intentional spacing, not redundant). Verified
	structurally via writeds/roundtrip on the National Grid fixture --
	confirmed the coalescing logic itself fires correctly where
	applicable (a nested spacer-table under the document's first
	top-level row now resolves to a single len=0 leaf after
	coalescing) -- but this fixture's overall leaf/blank-leaf counts
	were UNCHANGED (52 total, 8 blank) before vs. after, meaning this
	specific document doesn't actually have any 2+-consecutive-blank-row
	runs for the fix to act on beyond that one nested case. This means
	coalescing, while a real and correct improvement kept in the code,
	is NOT the full explanation for the many-^N-presses symptom on
	THIS fixture -- something is still causing at least one nested
	lset-in-cell-in-lset chain (the document's very first top-level row,
	a single-cell row wrapping a nested table that itself coalesces to
	one blank row) to draw far taller on screen than its now-minimal
	underlying character content (0 real characters at the bottom of
	that chain) should require. All 16 fixtures + 3 regression suites
	still green; render_test.ez regenerated. This nested-chain height
	mystery is the genuinely open question carried into the next live
	check -- not yet root-caused.

	NINTH item, requested live 2026-08-16, same session (wdc: "much
	better" after the eighth fix above) -- removed the table/spread
	hybrid entirely, per wdc's explicit direction: "I don't like the
	hybrid model of going back to table on colspan... we should be
	able to unsplit an lset crossing multiple columns." Design
	discussion (with wdc, before writing code): the key realization is
	that each lset row already builds its own fully independent
	binary-split tree (unlike table/spread's single shared grid), so a
	colspan cell doesn't need "unsplitting"/merging at all -- it just
	needs to be a proportionally WIDER leaf in its own row's split
	chain. wdc's own counter-example sharpened this from "no shared
	state needed" (my first, incomplete answer) to "needs one shared
	table-wide column count so cells in DIFFERENT rows still align":
	`Date | Description(colspan=3) | Status` over `Jan1 | ItemA |
	ItemB | ItemC | Shipped` only aligns if Description's weight (3)
	and ItemA+ItemB+ItemC's combined weight (1+1+1) are both expressed
	as fractions of the SAME total (5), not each row's own local cell
	count. Implemented:
	- TableColumnCount(tablenode): one lightweight pre-scan per table,
	  the max sum of colspan values (default 1) across all real rows --
	  deliberately simpler than the old BuildTableGrid's own Pass A, no
	  rowspan carry-over bookkeeping needed (rowspan still gets no
	  cross-row vertical handling at all, unchanged from before -- see
	  below).
	- LsetChainPctWeighted(remainingWeight, thisWeight): generalizes
	  the old LsetChainPct(remaining) -- pct = 100*(remainingWeight-
	  thisWeight)/remainingWeight, which reduces to the exact original
	  formula when every weight is 1 (checked, byte-identical for the
	  equal-weight case). BuildLsetChain now takes struct wleaf{leaf,
	  weight} pairs instead of bare struct lset* items.
	- BuildLsetCell now reports its own weight (colspan value) via an
	  out-parameter on every path through the function.
	- BuildLsetGrid computes ncols once per table, sums each row's real
	  cell weights, and pads with a MakeFillerLeaf (a real, empty
	  "text"-backed leaf, same reasoning as the sixth correction above)
	  when a row's own colspan sum falls short of ncols -- keeps a
	  genuinely ragged row's real cells aligned with its neighbors
	  instead of letting them silently stretch to fill 100%.
	Removed entirely: TableNeedsGridFallback (the routing decision),
	RenderTable/BuildTableGrid (the whole table/spread construction
	path -- ~230 lines), ParseWidthPixels/HTML_TABLE_DEFAULT_WIDTH (only
	used by the removed path), and the `#include <table.ih>` dependency.
	Every <table> now builds via lset unconditionally; the main walk's
	dispatch no longer branches on colspan/rowspan at all.

	Verified two ways beyond the usual fixture/regression-suite pass
	(all 16 fixtures + 3 suites still green, and zero TABLE-AT/NESTED-
	TABLE markers anywhere in the corpus now, confirming the hybrid is
	genuinely gone, not just unreachable): (1) a synthetic fixture built
	from wdc's own Date/Description/Status example -- writeds'd datastream
	shows the row-1 splits at pct=25 and pct=80, hand-verified to
	resolve to exactly Date=20%/Description=60%/Status=20%, and the
	colspan-free sibling row's splits are the untouched 50/67/75/80
	equal-weight chain; (2) a synthetic ragged-row fixture (a 4-cell
	row over a 1-cell row) -- the 1-cell row's single split is pct=75,
	meaning the real cell gets the complementary 25%, exactly matching
	column A's 25% share in the 4-cell row above it, confirming the
	filler-padding mechanism holds alignment.

	Real regression caught and fixed during this work, not by wdc but
	by re-running the fixture suite myself before declaring it done:
	BuildLsetGrid's own "no real rows" branch used to set
	st->hardfail=1 -- harmless before, since TableNeedsGridFallback's
	own "!anyRealRow" check routed that case to the OLD table/spread
	path instead (which degrades silently, no hardfail). With the
	hybrid gone, EVERY empty/decorative <table> in real mail, and every
	discarded "peek" attempt from BuildLsetCell's own sole-nested-table
	shortcut (see the SECOND CORRECTION above -- a multi-row nested
	table's peek is expected to be discarded, not a real failure),
	started hitting this same branch, and st->hardfail cascades into
	RenderHtmlPart falling back the WHOLE message to plain text
	(text822.c). Caught via the fixture suite itself: 3 of 16 fixtures
	(01, 08, 12) started reporting RENDER-OK: 0 despite producing
	complete, correct-looking dumps -- a real false-positive failure,
	not a construction bug. Fixed by no longer setting st->hardfail for
	this specific "structurally empty, nothing to render" case in
	either BuildLsetGrid or its caller RenderTableAsLset -- genuine
	allocation failures elsewhere in the file are unaffected, still
	propagate normally. All 16 fixtures back to RENDER-OK: 1 after the
	fix.

	ELEVENTH item, found live 2026-08-16, same session -- while
	investigating the still-open TENTH item (the scrolling/paging bug,
	see this project's memory entry; not fixed here, this is a separate
	finding), wdc pulled up render_test.ez's
	raw datastream by hand and asked why the first two lsets were an
	"apparent empty text, and view", noting "there's always a pair of
	nested lsets, and this seems incorrect"). Root cause: BuildLsetCell's
	sole-nested-table fast path (see the SECOND CORRECTION item) builds
	the nested table's own row (`inner`, already a complete lset leaf/
	chain) but then WRAPPED it inside the outer leaf this function
	speculatively allocates at entry (`leaf->dobj = inner`) instead of
	just using `inner` directly -- producing a pointless lset-wrapping-
	an-lset pair for every single-cell nested table. This is an
	extremely common real-world idiom (marketing HTML padding hack: a
	`<td>` whose only child is its own single-row single-cell `<table>`,
	e.g. National Grid's `<td align="center">&nbsp;</td>` spacer wrapped
	in its own table, nationalgrid-body.html:113-115) -- confirmed as
	the exact case wdc was looking at: object IDs 4318748208 (outer,
	empty viewname/dataname since it's mid-construction in the pasted
	excerpt) wrapping 4318748720 wrapping empty text 4318748976. Fixed:
	discard the unused outer shell via dataobject_Destroy and return
	`inner` directly as this cell's own leaf; *outWeight (the outer
	td's own colspan) is set earlier in the function regardless of
	which object gets returned, so this doesn't affect weight/alignment
	at all. Verified: all 16 fixtures + 3 regression suites still green;
	render_test.ez regenerated -- the National Grid fixture's first
	table now shows ONE lset leaf wrapping the empty text directly, not
	a pair; the colspan_test.html synthetic fixture (no nested tables,
	so this path never triggers) writeds'd byte-identical modulo object
	IDs, confirming the fix is a no-op outside the nested-spacer-table
	case. Whether this also explains any part of the still-open
	scrolling/paging bug (see project_html_mail_rendering.md's memory
	entry) is NOT confirmed -- fewer redundant lsetview layers can only
	help that investigation, but it wasn't the target of this fix and
	shouldn't be assumed to resolve it.

	TWELFTH through FIFTEENTH items, 2026-08-16, same session -- the
	TENTH item's scrolling/paging bug turned out to be a real core ATK
	bug (BackSpace's end-of-file line-padding wrongly snapping a
	just-reached end-of-document position back to the start of the
	previous, very tall line -- fixed in src/atk/text/textv.c, not this
	file) plus a genuine architectural gap this renderer's own output
	was the first thing to expose badly enough to notice: wrapping the
	entire email body in one monolithic lset (the "bulletproof layout"
	wrapper pattern virtually all commercial marketing HTML uses)
	breaks ATK's own character-count-based scrollbar model. Fixed with
	general peeling (TableIsTrivialWrapper + BuildLsetGrid's row-level
	splice, both keeping each nested table's own column-alignment scope
	fully independent after an interim version wrongly smeared them
	together -- see below), plus two more real bugs caught chasing the
	resulting "still too much whitespace" report: NodeIsVisuallyEmpty
	never looked inside a nested <table> to judge real blankness (so a
	<tr> wrapping nothing but a spacer table could never be recognized
	as blank for consecutive-blank-row coalescing -- fixed, and
	BuildLsetGrid's blank-check now runs BEFORE its splice-eligibility
	check, not after, so a genuinely blank spliced row's blankness
	isn't lost across the splice boundary); and htmlpart.c's style
	property allowlist never kept `display`/`visibility`, so the
	industry-standard "hidden preheader" trick (a `<div
	style="display:none">` holding text meant only for the email
	client's inbox-preview line) was rendering as ordinary visible body
	text -- confirmed live, National Grid: "We have helpful resources
	to help manage energy costs" at the very top of the rendered body,
	with no visible counterpart anywhere else in the source. Fixed in
	both renderers (htmlatk.c's NodeIsStyleHidden and htmltext.c's own
	independent copy), not just this one.

	Full detail -- the reasoning for each fix, what was tried and
	rejected, and an honest accounting of what's still imperfect about
	this whole approach -- is in revival/doc/html-mail-rendering-
	design.md's "Table strategy: lset/lpair, not table/spread" section
	now, not duplicated here; this file's own log stays focused on
	htmlatk.c-local judgment calls. All 16 fixtures + 3 regression
	suites green after every step above; render_test.ez regenerated
	each time. Whether the scrolling bug is FULLY resolved now (vs.
	just no longer looping via the specific BackSpace mechanism) is
	still wdc's call to make via live re-testing -- not yet confirmed
	as of this entry.

	SIXTEENTH item, 2026-08-16, same session -- live testing of the
	TWELFTH-through-FIFTEENTH fixes above surfaced two more findings,
	both confirmed against a real National Grid message (not the
	synthetic fixture corpus; the actual message this whole
	investigation has been chasing, saved at revival/tests/
	national-grid.html -- kept out of html-fixtures/ since that
	directory's own regression test hardcodes an expectation of
	exactly 16 files). First: BuildLsetGrid's blank-row coalescing
	(previous item) only dropped a blank <tr> when the row immediately
	before it was ALSO blank -- a deliberate judgment call at the time
	("an isolated blank row is ordinary, intentional spacing, worth
	preserving"), but this message's real body alternates one blank
	spacer row with every single real content row, so every isolated
	blank row fell through untouched: wdc found this independently via
	careful mouseover boundary-hunting in the live ez render ("a blank
	line between every lset"), confirmed structurally via
	`htmlatktest.test roundtrip` on render_test.ez showing
	LSET-AT[9]/[13]/[17]/... as genuinely empty (CELL-TEXT-CONTENT
	len=0), alternating with real content at [11]/[15]/[19]/....
	Fixed: every blank row is dropped now, isolated or not (see
	RowIsEntirelyBlank's and BuildLsetGrid's own comments) -- this
	renderer has no way to represent a source spacer's actual intended
	height anyway (an 8px CSS spacer and a full blank text line render
	identically here), so there was never a real fidelity trade-off
	being preserved by keeping isolated blanks, just an oversight.
	Verified: all 16 fixtures + 3 regression suites still green;
	render_test.ez regenerated against the real National Grid source
	via writeds, roundtrip-dumped, confirmed zero empty cells remain
	(previously 8); wdc confirmed live ("YES! That's done it!").
	Second, NOT fixed today (see revival/doc/revival.md's "Open
	issues" for the full writeup): a related-looking "scroll to end of
	document is non-deterministic" report turned out to be a distinct,
	pre-existing, non-HTML-specific bug in core ATK's scrollbar endzone
	click handling (textview__endzone/setframe, src/atk/text/textv.c)
	-- every discrete click fires a second, chained "page by one more
	line" scroll operation whose target depends on transient redraw
	state, producing a deterministic 3-click cycle rather than settling
	on the true end. Confirmed via targeted logging in setframe()/
	endzone(), deliberately deferred (wdc: "let's document that
	endzone scrollbar behavior and not fix it today") -- logged in
	revival.md rather than here since it's a general ATK scrollbar bug,
	not an htmlatk.c-local one.

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
