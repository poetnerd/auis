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

**Size caps:** total decoded HTML size and per-image decoded size get
a hard cap (exact numbers TBD, but something like 2MB/document,
5MB/image) — reject-and-fall-back beyond that rather than trying to
parse or paint something pathological.

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
- `<td>`/`<th>` cell content → `table_TextCell` for text-only cells,
  `table_ImbeddedObject` for a cell containing an `<img>` (or, later,
  a nested `<table>` — nesting is allowed structurally since a cell
  can embed any dataobject, but see the depth cap below).
- `colspan`/`rowspan` → `SetInterior`/boundary colors set to `JOINED`
  across the spanned chunk, per the existing `IsJoinedAbove`/
  `IsJoinedToLeft` macros.
- `border="0"` (extremely common in email templates, used purely for
  layout not visible grid lines) → boundary color `GHOST` or
  `SUPPRESSED` rather than `BLACK`, so layout-only tables don't render
  a visible grid nobody wants to see.

**Degradation, not failure:** deeply nested tables (email templates
sometimes nest 3-4 deep for outlook-compatibility hacks) get a depth
cap (e.g. 3) — beyond that, inner tables flatten to sequential
paragraph text within their cell rather than recursing further. This
is a bounded, deliberate degrade, not an error path.

## Images: the `image` inset, capability-checked

`image__ReadOtherFormat` (`src/atk/basics/common/image.c:2314`)
already does exactly what was asked for: it checks the MIME type
against a known-decodable list (`image/gif`, `image/x-gif`,
`image/pbm`, `image/ppm`, `image/pgm`, `image/jpeg`) and returns
`FALSE` for anything else, rather than silently producing garbage.
**Note: no PNG.** PNG is extremely common in modern mail (most
signature logos, many inline images) and isn't in that list — this is
a real gap, not a hypothetical one. Whether to add PNG decoding to
`image.c` itself, or treat "unsupported format" as the expected case
and always show the fallback for PNG specifically, is an open
question — leaning toward actually adding PNG support given how
common it is, but that's a separate, scoped task from this renderer
work (touches `image.c`, not the HTML parser/renderer).

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
   tags skip-not-corrupt, oversized documents/images rejected,
   disallowed attributes/elements dropped). The tree the renderers see
   is always well-formed, even if the source wasn't.

2. **Renderer-level, whole-message:** the ATK-styled renderer (stage
   3) is attempted first for any `text/html` part. If it hits a
   condition it can't recover from structurally (not "unknown tag" —
   that's already handled at the parser level — but something like a
   render-time failure constructing an inset, or exceeding the size
   caps mid-render), the **whole message** falls back to the stage-2
   plain-text renderer, not a half-rendered ATK document. No partial/
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

- Exact size caps (document/image) — placeholder numbers above, want
  real-world data from a sample of actual HTML mail before fixing
  numbers.
- PNG support in `image.c` — separate task, flagged above, not
  blocking this project but worth deciding whether it's a prerequisite
  or a parallel track.
- Table nesting depth cap — 3 is a guess; revisit once there's a
  fixture corpus of real template-heavy marketing email to test
  against.
- Whether `font` element support is worth the complexity given `style`
  attribute parsing already covers `color`/weight — possibly `font`
  can be implemented as sugar over the same style-property path
  instead of a separate code path.
