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
3. **ATK-styled renderer** — walks the tree, builds ATK text/table/
   image insets. `text822.c`'s primary path.
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

## Table strategy: Andrew's `table` object, not text-flow unpacking

Confirmed `src/atk/table/table.ch` is a real grid dataobject — rows/
columns, a `struct cell` per position that can hold text, a value, or
**an embedded dataobject** (`table_ImbeddedObject`, via the `Imbed()`
method), plus per-edge boundary colors including `JOINED` for merged
cells. Its view is `spread` (`src/atk/table/spread.ch`), a standard
dataobject/view pair — embeddable as an inset like any other ATK
object, not a special case.

This is a much better target than unpacking `<table>` into flowed
text: HTML `<table>`/`<tr>`/`<td>` maps onto it directly —
- `<table>` → `table_New` + `table_ChangeSize(rows, cols)`.
- `<td>`/`<th>` cell content → `table_ImbeddedObject` holding a fresh
  `text` dataobject, for any cell with real content, with the same
  HTML-to-ATK renderer (`htmlatk_Render()`) called recursively on the
  cell's children into that object — **not** a dispatch between
  `table_TextCell` for "text-only" cells and `table_ImbeddedObject` for
  a cell containing an `<img>`/nested `<table>`, which is what an
  earlier implementation did and a design review (grounded in real ATK
  precedent, see below) found to be a real mis-architecture: a table
  cell's content is an arbitrary nested document — rich text that can
  itself contain embedded objects — not a scalar choice between two
  cell kinds. `PAPERS/atk/Sherman.Alloc` lines 663–1066 is a genuine
  `table` object datastream dump whose cells are each a full
  `\begindata{text,...}\enddata{text,...}` datastream; the cell at
  line 675 contains prose, then an embedded `calc` spreadsheet inline
  via `\view{calcv,...}`, then more prose after it, all inside that
  one cell's own text object. A cell whose content happens to be
  simple (line 668's cell is just a centered/bold heading) is still a
  full `text` object in the real format, not a different
  representation — there is no separate "plain" cell-content type to
  special-case for. Nesting (a `<table>` inside a `<td>`, an `<img>`
  inside prose inside a `<td>`, any mix) falls out for free once cell
  content is genuinely just another `htmlatk_Render()` call: nesting
  is allowed structurally since a cell can embed any dataobject, and
  there's no depth limit imposed here either, same reasoning as above.
  A cell with no content at all (no children, or only whitespace text)
  is the one deliberate exception: it is left as `table_EmptyCell`
  (every freshly-grown cell's own default, see `table__ChangeSize` in
  `src/atk/table/table.c`) rather than paying for an empty `text`
  dataobject.
- `colspan`/`rowspan` → `SetInterior`/boundary colors set to `JOINED`
  across the spanned chunk, per the existing `IsJoinedAbove`/
  `IsJoinedToLeft` macros.
- `border="0"` (extremely common in email templates, used purely for
  layout not visible grid lines) → boundary color `GHOST` or
  `SUPPRESSED` rather than `BLACK`, so layout-only tables don't render
  a visible grid nobody wants to see.

**Degradation, not failure:** email templates nest tables deeply for
Outlook-compatibility hacks — the fixture corpus's real max is 28
levels (see `revival/tests/html-fixtures/README.md`), and that's
rendered correctly, not flattened. Degradation to sequential
paragraph text is still the right response to a genuine parse
failure partway through a table (a malformed/unclosed structure the
parser can't make sense of), just not to depth alone.

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

## Open questions

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
