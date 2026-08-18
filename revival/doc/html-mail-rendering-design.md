# HTML mail rendering — design

Design doc for the project tracked in `roadmap.md` under "HTML mail
rendering". Lives on the `html` branch.

Not calling this "html5" — nothing here targets HTML5 conformance.
It's a deliberately small, explicit allowlist subset aimed at what
real mail actually contains, closer in spirit to how `mimepart.c` is
scoped than to a browser engine.

## Goals

- Render real-world HTML mail (which leans on table layout, inline
  styles, and inline images) as ATK rich text, not stripped plain
  text.
- Make links clickable (launch via `open`, macOS LaunchServices).
- Do this with one shared, ATK-independent parser used by both a
  plain-text renderer (`cui`, and `text822.c`'s fallback path) and an
  ATK-styled renderer (`text822.c`'s primary path), per the staged
  plan already in `roadmap.md`.
- Fail closed and gracefully: unsupported input degrades to something
  readable, never to a blank pane or a corrupted document (the
  `htmlview` DOCTYPE bug this project exists to not repeat).

## Non-goals

- No CSS layout engine (box model, flexbox, floats). No JS. No
  external stylesheets. No full HTML5 tag set.
- No general-purpose web rendering. This is a mail-body renderer;
  scope is what MIME `text/html` parts actually contain, not what a
  browser must handle.
- `htmlview`'s authoring/composition side is untouched — this only
  affects the read path.

## Architecture (unchanged from roadmap.md)

1. **Parser library** — tokenizer → simplified tree, ATK-independent,
   fixture-tested (`mimepart.c`/`imap_prot.c` shape).
2. **Plain-text renderer** — walks the tree, emits text. Used
   unconditionally by `cui`; used by `text822.c` as the fallback path
   (see Fallback strategy below).
3. **ATK-styled renderer** — walks the tree, builds ATK text/`lset`/
   image insets (`lset`, not `table` — see Table strategy below for
   why). `text822.c`'s primary path.
4. *Optional, later* — retarget `htmlview`'s standalone viewer onto
   the same parser+ATK-renderer.

This doc fills in the parts roadmap.md deliberately left vague:
sanitization allowlist, table strategy, image strategy, and the
fallback contract between renderers 2 and 3.

## Sanitization: explicit allowlist

Default-deny. Anything not on this list is dropped (element) or
ignored (attribute), not passed through unrecognized. This is a
parser-level policy (stage 1), enforced before either renderer sees
the tree, so both renderers get the same already-sanitized input.

**Elements let through:**
`html`, `head`, `body`, `title` (read, not rendered), `p`, `div`,
`span`, `br`, `hr`, `a`, `b`, `strong`, `i`, `em`, `u`, `tt`/`code`,
`pre`, `blockquote`, `h1`–`h6`, `ul`/`ol`/`li`, `dl`/`dt`/`dd`,
`table`/`thead`/`tbody`/`tr`/`td`/`th`, `img`, `font` (mail HTML still
uses this constantly despite being deprecated everywhere else).

**Elements explicitly dropped, contents included (need to skip over,
not render their text):**
`script`, `style`, `head`'s non-`title` children (`meta`, `link`),
`iframe`, `object`, `embed`, `form` and all form controls (`input`,
`select`, `button`, `textarea`) — mail HTML sometimes carries these
from newsletter templates; render nothing rather than an inert
control.

**Elements unknown/unrecognized:** skipped structurally (don't push
an unpoppable entity — this is the exact `<!DOCTYPE>` bug class this
project exists to fix), no output, children still walked (so an
unknown wrapper `<div>`-alike doesn't eat its contents).

**Attributes let through, per element:**
- `a`: `href` only (must be `http:`/`https:`/`mailto:` scheme —
  anything else, e.g. `javascript:`, `file:`, is stripped and the tag
  degrades to plain text, not a dead/dangerous link).
- `img`: `src`, `alt`, `width`, `height` (see Images below for how
  `src` is resolved and gated).
- `table`/`td`/`th`: `colspan`, `rowspan`, `border` (cosmetic only —
  maps to a boundary color, not a real width).
- `font`: `color`, `size` (mapped through `html.c`'s existing
  tag→stylesheet machinery, same as today).
- Inline `style="..."` on any element: parsed for a **small** fixed
  property set only — `color`, `background-color`, `font-weight`,
  `font-style`, `text-decoration`. No selectors, no `!important`, no
  cascade — it's a flat per-element property bag applied at parse
  time, matching how `html.c`'s stylesheet lookup already works
  per-tag. Anything else in the `style` attribute is ignored.

Everything else — `class`, `id`, `on*` handlers, `style` properties
outside that list, any attribute on a dropped element — is discarded
silently. There's no JS engine to hand an `onclick` to and no CSS
cascade to resolve an `id` selector against, so keeping them around
would just be dead weight.

**Remote content:** `img src` pointing at `http(s)://` is not
auto-fetched. Default is to show a placeholder (alt text if present,
else `[image not loaded]`) with a per-message or per-preference
opt-in to fetch — this is the standard mail-client anti-tracking-pixel
posture, and worth stating outright rather than defaulting to "just
`popen` a fetch," which every image in every marketing email would
trigger silently otherwise. `cid:` (inline MIME-attached images,
already decoded by `mimepart.c`) are not "remote" and always render.
**Status:** this policy is decided but not implemented — today,
`htmlatk_Render()`'s resolver argument is always `NULL` (`text822.c`),
so no image, remote or `cid:`, is ever actually resolved to real
bytes; every `<img>` renders its text placeholder unconditionally.
Two separate prerequisites, not one: (a) `cid:` resolution needs a
Content-ID lookup `mimepart.c` doesn't parse or expose at all yet (it
only tracks Content-Type/Content-Disposition) — that's the more
foundational gap, since even opt-in remote fetching is only half the
picture without it; (b) the remote-fetch opt-in setting itself doesn't
exist anywhere yet. See Planned next work below.

**No fixed size caps.** Earlier drafts of this doc proposed hard
size/depth thresholds (reject-and-fall-back beyond ~2MB of markup, a
table-nesting depth of 3, etc.), reasoning from what the fixture
corpus showed as "typical." That was importing a browser's
defend-against-the-hostile-open-web posture where it doesn't belong.
`dataobj.c`'s `dataobject_Read` already parses arbitrarily deep
`\begindata{...}` nesting with plain C recursion and no depth cap, and
that's been correct for the entire life of AUIS — the fixture corpus
itself proves real mail routinely exceeds any "reasonable-sounding"
size guess (28 levels of table nesting, 18MB of embedded images in a
single message, both ordinary commercial mail, not edge cases). Large
or deeply nested is not the same as pathological, and rejecting real
content because it's bigger than a guessed threshold is a worse
failure mode than just rendering it.

The one real concern hiding inside "size caps" is implementation
robustness against adversarial input (a message deliberately crafted
with, say, a million levels of nesting, aimed at a crash or a hang) —
but that's an implementation property, not a policy number. Build the
tree-walker with an explicit heap-allocated stack rather than
per-level C recursion (sidesteps stack overflow regardless of depth),
and let a parse *taking too long* — not a size or depth threshold —
be what triggers the renderer-level fallback below. Images decode
through the ordinary `image` object path, same as any other image
insertion in ATK; this project doesn't need its own size gate for
that.

## Table strategy: `lset`/`lpair`, not `table`/`spread`

### Superseded: the original `table`/`spread` plan

The original plan here (kept below for the record, not because it's
current) targeted `src/atk/table/table.ch` — a real grid dataobject
with `struct cell` positions holding text, a value, or an embedded
dataobject (`table_ImbeddedObject`), plus per-edge boundary colors
including `JOINED` for merged cells, viewed via `spread`
(`src/atk/table/spread.ch`). It mapped HTML fairly directly: `<table>`
→ `table_New`+`table_ChangeSize`, each real cell → a fresh `text`
dataobject via `table_ImbeddedObject` (not a scalar text/object
dispatch — real `table` datastreams like `PAPERS/atk/Sherman.Alloc`
lines 663–1066 confirm a cell's content is always a full nested `text`
document, mixed prose and embedded objects together, not a choice
between two cell kinds), `colspan`/`rowspan` → `JOINED` boundary
colors, `border="0"` → `GHOST`/`SUPPRESSED` boundary color instead of
`BLACK`.

This was built (Stage 3, Gates 1–5) and worked, but was **rejected and
replaced with `lset`/`lpair`** once real live testing (`revival/doc/
lset-table-reflow-*` reports) exposed the actual problem: `spread`'s
`DesiredSize` (`spread.c:290–311`) is a fixed-pixel grid that never
recomputes on resize — no `Reshape`/`SetSize`/`ObservedResized` path
touches it at all. Real HTML tables need to reflow (wrap text, resize
columns) as the window resizes; `table`/`spread` simply can't, by
design — it's built for the same "already-sized interactive spreadsheet
pane" use case `lpair`'s own fixed-pixel sizeforms serve (see
`src/contrib/mit/neos/eosaux.c` for a real app using `lpair` exactly
that way). `lset`/`lpair` (`src/atk/adew/lset.ch`, `src/atk/
supportviews/lpair.ch`) is percentage-based instead —
`objcvt[i] = objsize[i]*totalsize/100`, recomputed on every real
`DesiredSize` call (`lpair.c:385`) — genuinely resize-aware, which
`table`/`spread` structurally is not. `border`/`GHOST`/`SUPPRESSED`
boundary-color handling has no `lset` equivalent (see "Known
imperfections" below) — that capability was lost in the switch, not
carried forward.

### Current approach: one `lset` tree per row, inserted into the
### surrounding text's own line flow

Each HTML table row becomes its own `lset` — a binary split tree
(`lset` is strictly binary: `left`/`right` only, so an N-cell row is
N−1 nested horizontal splits) whose leaves are the cells' own `text`
dataobjects, built via the same `htmlatk_Render()` recursion used
everywhere else in this renderer (so nested tables, images, and mixed
content inside a cell all just work, no special-casing). `colspan`
gets a proportionally larger split weight in its own row's chain
(`LsetChainPctWeighted`) rather than any merged-cell representation
(`lpair` can't represent a genuine merged cell — it's a guillotine
split tree); cross-row column alignment for a colspan header over a
plain data row is preserved by expressing every row's weights as
fractions of one shared `TableColumnCount`, not each row's own local
cell count.

**Rows are deliberately NOT stacked via a second `lset`/`lpair` split
layer.** An earlier version of this renderer did that, and it's wrong
for the same reason `table`/`spread` was rejected: `lpair`'s
`HORIZONTAL` (stacking) branch never sums its children's real heights,
only echoes back whatever height it was given — fine for `lset`'s
original fixed-window-pane design target, wrong for content that needs
to report its own real height. Instead, `RenderTableAsLset` inserts
each row as its own view directly into the enclosing `text`'s own line
flow — the exact mechanism this renderer already uses for every other
block (paragraphs, images) — and relies on `lsetview`'s own
`DesiredSize` (added to core ATK by this project, see below) to report
each row's real height correctly within that flow.

**Two core ATK toolkit gaps, not specific to this renderer, found and
fixed along the way** (both in `src/atk/adew/lsetv.c`/`.ch`, additive,
no existing `lset` behavior changed): `lsetview` had no `DesiredSize`
override at all — a leaf inherited `lpair`'s generic fallback, which
never consults `self->child`'s real content size, so it always echoed
back whatever it was asked for. Fixed by forwarding to `self->child`
for leaves (modeled on the sibling `celview` class's already-working
equivalent), falling through to `lpair`'s existing split-node behavior
unchanged. Similarly, `lsetview_WantNewSize` only handled local redraw
and never escalated to the parent the way `view__WantNewSize`/
`celview__WantNewSize` do — fixed with the same `super_WantNewSize`
escalation pattern (debounced against a real re-triggering loop
observed live). Neither gap was `lset`'s fault exactly — nothing in
this codebase had ever needed a resizable, content-driven `lset` leaf
embedded inline in flowing text before; `eosaux.c`'s real usage
confirms every other caller uses `lpair` for fixed-size UI chrome,
which never exercised this path.

### Peeling: eliminating superfluous wrapper tables/cells

Real marketing HTML almost universally wraps its *entire* body in an
outer single-row, single-cell `<table>` shell (the "bulletproof
layout" pattern, for cross-email-client compatibility), with the real
content nested one or more tables deep inside that one cell — often
several layers of this, each one alone doing nothing but reproducing
the same shell. Built literally, this collapses the whole visible
message into a tiny handful of `lset` objects sitting behind just one
or two character positions in the top-level document (confirmed live,
National Grid fixture, 2026-08-16: the entire ~2000+ pixel-tall body
ended up as ONE embedded view at one single character position out of
68 in the whole document).

That disproportion isn't just wasteful nesting — it actively breaks
things, because ATK's own scrollbar model (`textview`'s `getinfo()`,
`textv.c`) sizes the elevator/thumb from **character count**
(`total->end = text_GetLength(...) << FINESCROLL`), with no pixel-height
term anywhere in it. That assumption — character position is roughly
proportional to vertical space — holds for ordinary typed text (every
character contributes a small, bounded sliver of height) and breaks
down completely the moment one character position *is* an embedded
view worth thousands of pixels: the elevator reports "nearly the whole
document visible" when almost none of it, pixel-wise, actually is.
Narrower windows make it worse (more text-wrapping inside the one
oversized view, making it even taller), which is why the practical
symptom was "can't reach a lot of real content via `^V` no matter how
many presses, unless the window is made much wider."

Fixed with two complementary checks in `htmlatk.c`, both applied via
the same iterative walk used for everything else (so chains of nested
wrappers collapse all the way down, not just one level):
- `TableIsTrivialWrapper`: a `<table>` with exactly one real row and
  one real cell is pure structural boilerplate — inline its cell's
  children directly, no `lset` built for it at all.
- `BuildLsetGrid`'s own row loop: a `<tr>` with exactly one real cell
  whose sole content is itself a nested `<table>` (per
  `NodeIsSoleNestedTable`) has that inner table's rows spliced in
  directly — each inner table rebuilt **recursively and
  independently**, with its own separately-computed
  `TableColumnCount`/column alignment, not flattened into one raw row
  list and re-padded against the *outer* table's column count. That
  distinction mattered in practice: an earlier version of this fix did
  the naive flatten-then-pad, and a single genuinely 2-column row
  elsewhere in the same wrapper (a footer logo-plus-social-icons row)
  pushed the whole table's column count to 2, so every other,
  genuinely single-column spliced-in row got padded with a spurious
  empty 50% filler — visually squishing real content into the left
  half of the window, inconsistently across window widths. Keeping
  each nested table's column scope fully self-contained (via real
  recursion, not list-splicing before the fact) fixed that.

A third, related bug surfaced and got fixed in core ATK itself while
chasing this: `BackSpace` (`textv.c`), used by `^V`/`^B`'s "get line
aligned" step, pads the last line's matched span by one phantom
character so that `pos == textLength` always matches *some* line when
the document has no trailing newline. That padding also made it think
a position exactly at the true end of the document was still *inside*
the previous (very tall) line, snapping the scroll position back to
that line's start on literally every keystroke — an infinite loop
through the same content, confirmed via live instrumented tracing.
Narrow ordinary text essentially never has a single line taller than
one screen, so this had presumably been a dormant bug in ATK for
decades; a document with one or more disproportionately tall embedded
views (exactly what an unpeeled wrapper table produces) is the first
thing to reliably trigger it. Fixed narrowly (only the specific
boundary case, not the general "snap to line start" behavior every
other caller relies on) rather than removing the padding outright,
since other callers depend on `pos == textLength` always matching some
line to avoid a different failure.

A fourth bug, also `htmlatk.c`-local, surfaced from live testing after
the peeling fixes above landed: `BuildLsetGrid`'s blank-row coalescing
only dropped a blank `<tr>` when the row immediately before it was
*also* blank, on the theory that an isolated blank row was ordinary,
intentional spacing worth preserving. Real templates that alternate
one blank spacer row with every single content row (confirmed live,
National Grid) defeated that theory completely — every isolated blank
row fell straight through as its own real, empty `lset`, found by
careful mouseover boundary-hunting in the rendered view ("a blank line
between every lset") and confirmed structurally via
`htmlatktest.test roundtrip`. Fixed by dropping every blank row
unconditionally, isolated or not — there was never a real fidelity
trade-off here (this renderer can't represent a spacer's actual
intended height anyway; an 8px CSS spacer and a full blank text line
render identically), just an oversight in the original coalescing
logic.

### Known imperfections, and why this is still the right call

This is **not** a general HTML reflow engine, and peeling is a
heuristic, not a principled fix — it recognizes two very specific,
extremely common real-world wrapper shapes (whole-table 1×1, and
row-level "sole cell wraps a table") and collapses them. Real HTML
wrapper patterns this doesn't catch (a 1-row wrapper with incidental
whitespace-only sibling cells that don't quite match the exact
node-shape checked, for instance) will still under-peel and reproduce
some version of the same disproportion, just less severely. `lset` has
no notion of a transparent/no-op grouping container the way a real box
model does — every non-empty node in the tree costs a real `lset`+
`lpair` object, so "peeling" is fundamentally working around a
structural mismatch rather than resolving it. It's kept anyway because
the alternative (leaving wrapper tables in place) is worse in a way
that's directly visible to the user (the scrollbar/reachability
breakage above) — this is the best fit available within `lset`/
`lpair`'s actual capabilities, not a claim that it's correct HTML
rendering.

Two capabilities `table`/`spread` had that `lset` genuinely doesn't,
not recovered by this switch: **no border/background rendering at
all** (`table`/`spread`'s `JOINED`/`GHOST`/`SUPPRESSED` boundary-color
model has no `lset` equivalent), and **every `lset` split renders a
visible thin resize-divider/grab bar** between its two sides —
`lsetv.c`'s `initkids()` hardcodes `moveable=TRUE` on every split with
no data-level way to suppress it, and `lset.ch`'s data section has no
such field to add one cheaply. A genuine invisible CSS layout table
(the overwhelmingly common real-world case — `border="0"`, used purely
for alignment) will show these divider bars where a browser would show
nothing. Both are accepted, known gaps, not fixed here.

The scrollbar-elevator inaccuracy itself (`getinfo()`'s character-count
model) is *mitigated*, not fixed, by peeling — spreading real content
across many more character positions makes the character-count proxy
for "position in the document" roughly accurate again, but the
underlying assumption is still there and could still mislead for any
single remaining embedded view disproportionately taller than its
neighbors. A properly pixel-aware elevator would mean changing
`getinfo()`/`position()` in core `textv.c` to weight by real per-line
height — shared scrollbar code used by every ATK application, a much
bigger and riskier change than anything in this renderer, and out of
scope here.

Separately, a related-looking but distinct scrollbar bug, now fixed: a
held click on the bottom endzone (long enough to trigger the button's
auto-repeat) could blank the whole screen. Root cause was in core ATK's
`BackSpace` (`textv.c`), not this renderer, and not specific to HTML
mail — see `revival/doc/revival.md`'s "Old bugs never found till now"
for the full writeup.

**Degradation, not failure:** email templates nest tables deeply for
Outlook-compatibility hacks — the fixture corpus's real max is 28
levels (see `revival/tests/html-fixtures/README.md`), and that's
rendered correctly, not flattened. Degradation to sequential
paragraph text is still the right response to a genuine parse
failure partway through a table (a malformed/unclosed structure the
parser can't make sense of), just not to depth or nesting alone.

## Images: the `image` inset, capability-checked

`image__ReadOtherFormat` (`src/atk/basics/common/image.c:2314`)
already does exactly what was asked for: it checks the MIME type
against a known-decodable list (`image/gif`, `image/x-gif`,
`image/pbm`, `image/ppm`, `image/pgm`, `image/jpeg`) and returns
`FALSE` for anything else, rather than silently producing garbage.
**Note: no PNG.** PNG is extremely common in modern mail (most
signature logos, many inline images — 5.4% of HTML-bearing messages in
the fixture corpus carry one, see its README) and isn't in that list —
a real gap, not a hypothetical one. Decided: add it, not just fall
back. `image` already has format-specific subclasses following the
identical pattern PNG would need — `jpeg` (`src/atk/basics/common/
jpeg.ch`) and `tif` (`src/atk/image/tif.ch`) each just override
`Read`/`Write`/`Load` and provide a class `Ident()` for format
detection, wrapping an external codec (libjpeg for `jpeg`). A `png`
subclass wrapping a PNG decoder (zlib inflate + PNG's scanline
filters) fits the same slot. It's also close in spirit to AUIS's own
native `raster` object (`src/atk/raster/cmd/raster.ch`) — a
bitmap-plus-run-length-compression format AUIS already decodes — PNG
is architecturally the same idea (bitmap + compressed encoding) with
color instead of `raster`'s monochrome RLE and a different (DEFLATE)
compression scheme. Still a separate, scoped task from this renderer
work (touches `image.c`/a new `png.ch`, not the HTML parser/renderer),
but no longer an open question — just not yet started.

Rendering plan: `<img>` → attempt `image_New` + `ReadOtherFormat`
against the resolved bytes (either `cid:`-referenced MIME part, or
fetched remote content per the policy above). On `FALSE` (or content
not fetched because remote-blocked), render a text placeholder using
`alt` if present, else `[image: <mime-type> not supported]` — same
idea as `PrintAttachmentLine` already does in `cui` for non-renderable
MIME parts, just at the inline-image granularity instead of the
whole-message granularity.

## Fallback strategy (now explicit, per renderer)

Two levels, both explicit:

1. **Parser-level, per-construct:** already covered above (unknown
   tags skip-not-corrupt, disallowed attributes/elements dropped). The
   tree the renderers see is always well-formed, even if the source
   wasn't.

2. **Renderer-level, whole-message:** the ATK-styled renderer (stage
   3) is attempted first for any `text/html` part. If it hits a
   condition it can't recover from structurally (not "unknown tag" —
   that's already handled at the parser level — but something like a
   render-time failure constructing an inset, or parsing simply taking
   too long — the adversarial-input case, not a size/depth number, see
   the Sanitization section), the **whole message** falls back to the
   stage-2 plain-text renderer, not a half-rendered ATK document. No
   partial/
   corrupted rendering state is ever shown to the user — this is the
   direct lesson from the `htmlview` DOCTYPE bug (partial render,
   silently wrong) and is being stated as a hard requirement here
   rather than left implicit.

   `mimepart_SelectAlternative`'s existing plain-text-vs-html part
   selection is a separate, upstream decision (which MIME part to
   use); this fallback is downstream of that — it's what happens when
   the chosen `text/html` part itself defeats the renderer.

## Links: click-to-launch

`<a href>` → a text style applied to the URL run at insertion time
(new named style, e.g. `htmllink`, reusing `html.c`'s existing
tag→stylesheet plumbing) plus a `Hit()` view-method override on the
containing view that detects a click inside that style run and
`popen("open <url>", "r")`s it (matching `htmlview__Hit`'s existing
anchor-detection code, which today only echoes the URL to the message
line — this wires the same detection to an actual action). Scheme is
already restricted to `http`/`https`/`mailto` by the sanitization
pass, so nothing unexpected reaches `popen`.

**Status: half-built.** The style-application half of this landed in
Stage 3 (every `<a href>` run does get the underlined link style), and
`htmlatk_LinkAt()`/`htmlatk_LaunchURL()` (`htmlatk.c`) exist and work —
confirmed via `htmlatktest.test linkat`. But the `Hit()`-override half
was never actually written: grepping the live `messages`/`text822.c`
path (2026-08-17) turned up zero callers of either function outside
the offline test tool. Clicking a link in a real rendered message
today does nothing. See Planned next work below.

## Implementation

Status snapshot of what's actually built and fixed — an inventory,
distinct from the architecture/rationale sections above. Full
bug-by-bug technical detail (root causes, code, live-verification
steps) lives in `revival/doc/porting-assessment.md` (§24 for the
`lset`/`lpair`/`textview` fixes below) and `revival/doc/revival.md`'s
"Old bugs never found till now" section — this section is a pointer
and summary, not a duplicate.

**Stage 1 — Parser library:** done, committed (`756e137a`).
`htmlpart.c`/`.h`. 20/20 fixture tests (`html-parse-tests`).

**Stage 2 — Plain-text renderer:** done, committed (`56f48784`).
`htmltext.c`/`.h`. 13/13 fixture tests (`html-totext-tests`).

**Stage 3 — ATK-styled renderer:** done, committed. `htmlatk.c`/`.h`,
`text822.c`'s primary path. Walks the parsed tree into `text`/`lset`/
`image` insets — see "Table strategy" above for the `lset`-vs-`table`
decision and the peeling heuristic.

**Core ATK toolkit changes, `lset`/`lpair`** (`src/atk/adew/lsetv.c`/
`.ch`, `src/atk/supportviews/lpair.c`) — none of this is HTML-specific;
`lset`/`lpair` had simply never been asked to do this before this
project:
- `lsetview` gained a real `DesiredSize` override (previously inherited
  `lpair`'s generic no-op fallback, which never consulted a leaf's real
  content size) and `WantNewSize` escalation to its parent.
- `lset` gained real fixed-pixel splits (`lsetview_MakeHorzFixed`/
  `MakeVertFixed`), for small, non-reflowing decoration/spacer cells.
- `lpair__DesiredSize`'s "pathological content" height clamp — a
  hardcoded 2048px cutoff from before any caller routinely exceeded it
  — raised to a practically-unreachable 1,000,000; real HTML mail
  bodies routinely exceed the old value.
- `lpair__DesiredSize`'s HORIZONTAL (stacked-row) branch never summed
  its children's real heights, only echoed back whatever height it was
  offered — fixed to query each child's real height and sum them
  (`b38e9fda90`).
- `htmlatk.c`'s `BuildLsetCell` splice-through (a shortcut meant only
  for a truly trivial single-cell wrapper) could also fire for a
  genuine multi-cell nested row, flattening whole subsequent newsletter
  sections into one side-by-side chain — fixed to only fire for a
  genuine single-cell leaf (`a9a82d66f1`).

**Core ATK toolkit changes, `textview`** (`src/atk/text/textv.c`) —
also general-purpose fixes, not HTML-specific, all first exposed by
this project because it's the first content to embed a view taller
than one screen:
- `BackSpace`'s end-of-file phantom-character padding could mistake the
  true end of a document for still being inside the previous
  (oversized) line, snapping the scroll position back to that line's
  start on every keystroke — an infinite loop.
- `BackSpace`'s `MoveByPixels` fast-path used a stale calculation that
  swallowed an entire backward-move request whenever the current line's
  height exceeded it — silently no-op'd `Escape-v`/prior-screen while
  inside any oversized embedded view, including plain `raster` images,
  not just HTML mail.
- `textview_Visible` conflated text-position containment with
  pixel-level visibility, treating a position at the end of an
  oversized, only-partly-scrolled-into line as "already visible" — made
  `Escape->` ("go to end") silently no-op.
- `BackSpace`'s pixel-budget search never credited a target position's
  own line height when the target sat at that line's *end* (only lines
  strictly before it) — made "go to end" snap all the way back to the
  top instead of framing near the bottom, once the `Visible()` fix
  above correctly detected it needed to move at all. (`1971a51eb3`,
  together with the `Visible()` fix)
- `textview__MoveForward`'s pixel branch unconditionally advanced the
  scroll-top to `text_GetLength()` whenever a forward page (`^v`)
  consumed the entire remaining height of the document's last line —
  a position with no line of its own, which the layout code renders
  as a blank screen. Not specific to oversized embedded views; latent
  for any document where forward-paging exactly exhausts the last
  line. Suspected root cause of the original motivating complaint
  ("space-space-space skips the last chunk of a message and moves to
  the next one"). Fixed to stop at the last line instead, capped at
  its own bottom edge, so further paging becomes a no-op once there
  (`de73971e25`).
- `BackSpace`'s `MoveByPixels` "stay within the current line" fast-path
  was applied to any `pos` passed in, though the shortcut's own logic
  only makes sense when `pos` is the current top position. The
  scrollbar's bottom-endzone "jump to end" (`setframe()`/`endzone()`)
  computes its target from a freshly-found line near the end of the
  document, not the current top — a held click (past the endzone
  button's ~100ms auto-repeat) re-issued the identical jump request
  while `pixelsReadyToBeOffTop` still held the first click's landing
  offset, tripping the fast-path against that unrelated value and
  returning the target completely unmoved: `text_GetLength()`, the same
  no-line-of-its-own position the `MoveForward` fix above already
  identified as rendering blank. Fixed by gating the shortcut on
  `pos == textview_GetTopPosition(self)`, matching what its own comment
  already claimed (`492e9b6e24`).

**Known open bug:** forward/backward paging (`^v`/`Escape-v`) and the
scrollbar's elevator position are still not pixel-accurate — landing
positions are approximately but not exactly where a precise scroll
would put them. Not a scrollbar-click bug like the ones above (those are
fixed); this is the underlying character-count-based positioning model
described earlier in this document, still only mitigated by peeling, not
fixed at the root.

## Planned next work

Three gaps found live against the real National Grid message
(2026-08-17), agreed sequencing below — unlike Open questions further
down, these aren't undecided, just not yet done.

1. **Smart punctuation renders as `?`.** `mimepart_Utf8ToLatin1`
   (`mimepart.c`, shared with plain-text mail bodies, not
   HTML-specific) exactly converts any codepoint ≤ 0xFF and turns
   anything above into a literal `?`, same policy `htmlpart.c`'s own
   numeric-entity decoding follows (see its comment). That's correct
   for genuine Latin-1 gaps (there is no Latin-1 byte for, say, CJK
   text), but real marketing HTML is saturated with a small, common
   set of Unicode punctuation just *above* Latin-1 that has an obvious
   ASCII fallback — curly single/double quotes (U+2018/2019/201C/201D),
   en/em dash (U+2013/2014), ellipsis (U+2026) — and today all of
   those become `?` too, which is most of what actually produces the
   "ton of question marks" in a typical message. ATK's `compchar.c`
   compose feature was considered and doesn't apply here: it's a
   *keyboard-input* helper (accent-key + letter → composed glyph while
   typing), not a rendering fallback, and can't display a codepoint
   with no Latin-1 slot regardless. Fix: map that specific small
   codepoint set to sane ASCII in `mimepart_Utf8ToLatin1` itself
   (benefits plain-text mail too), instead of the blanket `?`.
   **Scope boundary:** this fixes Western marketing-copy punctuation
   only. It does not, and cannot, help with the much bigger gap below.
2. **Link clicks don't do anything live.** See the Links section
   above — the library half (`htmlatk_LinkAt`/`htmlatk_LaunchURL`) is
   built and tested; the `Hit()`-override wiring into `messages`'s
   actual message view was never written. Needs figuring out where
   `text822.c`'s displayed content handles mouse clicks today (or
   whether a wrapper view needs adding) before the override can go in.
3. **Remote image fetching isn't implemented at all.** See the Images
   section above — the anti-tracking-pixel opt-in *policy* was decided
   at this doc's outset, but nothing resolves images yet, `cid:` or
   remote (`text822.c` passes a `NULL` resolver unconditionally). Two
   prerequisites stack here: `cid:` resolution needs Content-ID
   parsing `mimepart.c` doesn't have, and only after that does the
   remote-fetch opt-in setting itself (still undesigned — global vs.
   per-message vs. per-sender, where it lives) become buildable.
   Deliberately last: real user-facing privacy/security tradeoffs,
   wants explicit sign-off on the setting's shape before writing code.

Agreed order: (1) first — small, self-contained, no open design
questions. (2) second — bigger, but nothing left to decide. (3) last —
blocked on design decisions above, not on effort.

## Open questions

- **Non-Latin scripts render as walls of `?`, and there's no fix short
  of a genuine Unicode text model.** Inventoried directly against the
  real National Grid message's multi-language "please translate this"
  footer (2026-08-17; the same block item 1 above partially addresses)
  — of ~200 non-ASCII characters in that block, only the ~14 Western
  European accented Latin letters (à, é, ô, etc.) and the one curly
  apostrophe item 1 fixes actually have a Latin-1-or-ASCII
  representation. Everything else is a different alphabet with no
  Latin-1 equivalent at all: Cyrillic (61 chars, Russian), Bengali
  (53), Hebrew (37, +Yiddish), Arabic (22), CJK ideographs (14,
  Chinese), plus 4 Vietnamese characters that specifically need
  combining dot-below diacritics (Unicode's Latin Extended Additional
  block, outside Latin-1's 256 codepoints) even though *other*
  Vietnamese diacritics (grave/acute/circumflex) are already Latin-1
  and already render fine — so a single Vietnamese sentence can be
  partially readable and partially not. No mapping-table trick fixes
  this the way item 1 fixes smart quotes; it would need ATK's text
  model to actually store/route multi-byte characters and a
  Unicode-aware font, which is a fundamentally bigger undertaking than
  anything else in this document. Not planned, not scoped — logged
  here as a known, structural limitation of building on ATK's 8-bit
  Latin-1 text model, not a bug to chase.
- PNG support in `image.c` (a new `png.ch` subclass, see Images above)
  — separate task from this renderer work, not blocking it, but worth
  deciding whether it's a prerequisite or a parallel track. Same
  question applies to SVG/WEBP, which the fixture corpus also turned
  up (`revival/tests/html-fixtures/README.md`) but which weren't on
  this doc's radar originally — SVG in particular is a much bigger
  lift than PNG (a vector format, not a bitmap codec) and probably
  isn't worth it; WEBP is closer to PNG/JPEG in shape.
- Whether `font` element support is worth the complexity given `style`
  attribute parsing already covers `color`/weight — possibly `font`
  can be implemented as sugar over the same style-property path
  instead of a separate code path.
- What a parse-taking-too-long threshold should actually be, now that
  the renderer-level fallback is keyed on time rather than size/depth
  — needs a number once there's a working parser to benchmark against
  real (and deliberately pathological) input, not before.
