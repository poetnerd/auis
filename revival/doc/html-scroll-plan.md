# Fixed-width HTML tables with horizontal scroll (Thunderbird-style)

> **Status (2026-09-25):** Steps 1 and 2 are both implemented and
> live-verified — canonical status lives in `roadmap.md`'s Open items
> (item 1), not duplicated here. The stray-whitespace bug from Step 2 is
> fixed. That work uncovered a further, distinct bug — elevator-drag
> scrolling was inaccurate specifically for a document containing an
> oversized embedded view (national-grid.html's tableau table) — now
> **fully fixed** in **Step 3** below, including the drag-into-endzone
> mis-landing that was the last open piece: three real bugs (LP64 int/
> long truncation in `scroll.c`'s `set_frame`; an out-of-bounds
> `self->lines[-1]` read in `textv.c`'s `DoUpdate`; an `off`-reset-to-0
> bug in `setframe()` when a drag decodes exactly to the document's end).
> wdc-confirmed working end to end, including a full re-test of legacy
> `.ez` scrolling (`revival/testing.ez`) to rule out regressions. Click/
> page navigation in the same scrollbar was fixed earlier (see `textv.c`'s
> `FINESCROLL` widen) and remains confirmed working.

## Context

`messages` renders HTML-email `<table>`s as `lset`/`lpair` insets embedded in
the message body text. Today every table is laid out top-down: the enclosing
`textview` hands the table's root view whatever width is left on the current
line, and `lpair__DesiredSize` just divides that offered width by percentage
among its cells — it never reports a genuine natural/larger width. Text cells
reflow to fit (`textview` honors any offered width); image cells don't
(`imagev__DesiredSize` always reports its native size and gets clipped at
paint time). The result, confirmed live: simple mostly-percentage tables
(`national-grid.html`, `LinkedIn`, each with exactly one pixel-width "floor"
table) degrade tolerably, but genuinely multi-column pixel-width layouts
(`bookrack.html`, nested `640→440/160→200/110`) get crushed into a narrow
strip — "a terrible mess," per live screenshot.

Real evidence from three saved fixtures plus live Thunderbird comparison
converged on a rule: **a `<table>`'s own `width=` attribute already declares
its intent.** `width="100%"`/no width → author meant "flow," keep today's
percentage-reflow behavior. A literal pixel value → author declared a real
design width; don't crush it narrower, even if that means the table needs
horizontal scrolling to see the rest — matching Thunderbird exactly.

An earlier version of this plan tried to implement that by teaching the
enclosing `textview`/`drawtxtv.c` to let a table's embedded view genuinely
exceed the offered line width, plus adding a real horizontal-scroll axis to
core `textview` itself (mirroring `imagev`'s already-working scroll template).
**Rejected** by wdc: that's a needlessly large, shared-code change for what
should be a local problem. The revised design below instead gives a
"tableau" table its own **self-contained scrollable inset** — two rendering
classes, exactly as proposed: (1) ordinary reflowing text/percentage tables,
unchanged, and (2) a new view class for pixel-width tables that owns its
natural width and its own scrollbar internally. **This touches only
`lset.ch`/`lsetv.c`/`htmlatk.c` plus one new small class — `lpair.c`,
`textv.c`, and `drawtxtv.c` are untouched.**

## Key findings that shape the design

- **`lset`/`lsetview` has zero blast radius outside HTML-table rendering.**
  Confirmed by grep: the only callers of `class_NewObject("lset")` are
  `htmlatk.c` (3 sites) plus `lsetv.c`'s own internal default-construction
  fallback. `lpair` itself (the generic split-pane primitive `lset` reuses)
  has huge blast radius — `messwind`, `frame`, the help browser, several
  `src/contrib` apps — so **`lpair.c` is not touched**; every layout change
  is scoped to `lsetv.c`/`lset.ch`, which already has precedent for exactly
  this kind of override (`lsetview__DesiredSize`'s existing leaf-node
  forward, `lsetv.c:458-467`, and three prior purpose-built `lset` fields —
  `nobar`/`vcenter`/`autoheight`, each with its own `\V` version bump).
- **`scroll` (`src/atk/supportviews/scroll.ch:93`, `class scroll : view`) is
  a generic, standalone wrapper, not a window-outer-layer-only mechanism.**
  It's constructed the ordinary object-system way (`Create`/`InitializeObject`
  are classprocedures); its `child`/`scrollee` can be any `struct view *`,
  independently settable (`SetView`/`SetChild`/`SetScrollee`,
  `scroll.ch:125-129`); it has no `DesiredSize` override, so it passively
  accepts whatever size it's offered — the same well-behaved contract every
  other embedded inset (`imagev`, `htmllinkview`) already honors. Existing
  uses in the tree (`treev.c`, `spread.c`, `sliderv.c`, the `ex*` examples)
  are all `GetApplicationLayer`-style whole-window wrapping, so there's no
  direct precedent for embedding `scroll` as an ordinary inline inset via a
  dataobject's `viewname` — architecturally there's nothing that should
  prevent it (its constructor hooks are the same per-dataobject
  `InitializeObject` every embeddable view class uses), but it's the one
  piece worth a cheap early prototype rather than assuming.
- **`imagev` (`src/atk/image/imagev.c:1478-1587`) is the working template**
  for the horizontal-scroll interface itself (`x_getinfo`/`x_setframe`/
  `x_whatisat`, registered via `GetInterface("scroll,horizontal")`) — but
  scoped to `lsetview` instead of `imagev`. `scroll` obtains these by
  querying its *scrollee* (`scroll.c:580-581`,
  `self->fns[type] = view_GetInterface(self->scrollee, InterfaceName[type])`),
  so the scrollee (the inner `lsetview`) is what needs to answer them, not
  the wrapper.
- **`ParseFixedPixelWidth`** (`htmlatk.c:151-161`) already has exactly the
  right accept/reject semantics for a `<table width="...">` check (bare int
  or `"NNNpx"`; rejects `%`/`auto`) and is directly reusable as-is — it's
  currently only ever called for `<td>`/`<img>` level checks, never on the
  `<table>` tag itself.

## Implementation phases

### Step 1 — Natural-width computation (`lset.ch` + `lsetv.c` + `htmlatk.c`; no new view class yet)

Unchanged from the earlier version of this plan — this part was never the
problem, and stays useful regardless of how the width is ultimately used:

1. **`lset.ch`**: add `int minwidth;` to the `data:` section (a fourth
   purpose-built field, following `nobar`/`vcenter`/`autoheight`'s exact
   pattern), bump the persisted version (`\V5`) in `lset__Read`/`lset__Write`
   with the same "0 for older files / non-table callers" default.
2. **`htmlatk.c`**: reuse `ParseFixedPixelWidth` on the `<table>` tag's own
   `width=` attribute inside `BuildLsetGrid`, and set the resulting pixel
   value as `minwidth` on the row-chain root(s) `BuildLsetChain` returns for
   that table. Nested tables (e.g. bookrack's `200`/`110` book-cover columns)
   get their own `minwidth` independently when `BuildLsetGrid` recurses —
   falls out of the existing recursive structure, no special-casing needed.
3. **`lsetv.c`**: extend `lsetview__DesiredSize`'s split-node case (currently
   a bare `super_DesiredSize(...)` call, `lsetv.c:464-466`) to query both
   children's own natural width (`view_DesiredSize(..., view_NoSet, ...)`),
   sum per split orientation, and report
   `max(offered_width, own_minwidth, sum_of_children_natural)` as `*dWidth`
   — while still dividing the *offered* width by percentage for the actual
   child `WidthSet` allocation, exactly as today. Entirely confined to
   `lsetv.c`.
4. Flag each table (at the same `BuildLsetGrid` site) as a "tableau" — has a
   genuine pixel-width floor somewhere in itself or a descendant — vs. plain
   percentage/reflow. This flag decides the `viewname` used in Step 2.
5. **Verify via instrumentation** (temporary trace prints, removed before
   commit): confirm `lsetview__DesiredSize` reports `750`/`640`/`512` etc.
   for the fixtures' floor tables and a correctly-summed value for
   bookrack's nested columns.

### Step 2 — Self-contained scrollable table inset (new class + `lsetv.c`; still no `textview`/`drawtxtv.c`/`lpair.c` changes)

6. **Prototype first, cheaply**: before building the real thing, confirm
   `scroll` genuinely works as an ordinary bounded inline inset (not just a
   window's outer layer) — e.g. temporarily wrap a trivial existing embedded
   view in a hand-built `scroll` instance and confirm it draws/clips
   correctly inside `messages` at a fixed small size. This resolves the one
   real unknown in this plan before committing to the class design below.
7. **New view class** (name TBD, e.g. `lsetscrollview`) — either a direct
   `scroll` subclass or a thin composing wrapper — used as the row-root
   `lset`'s `viewname` **only** for tables flagged "tableau" in Step 1
   (plain percentage tables keep using plain `lsetview`, unchanged). Its
   `InitializeObject` constructs an ordinary `lsetview` for its own
   dataobject as the `scroll`'s child/scrollee (`scroll_SetView`), and sets
   a horizontal-only location (`scroll_BOTTOM`). It reports a *bounded* size
   to its own parent (textview) — inherited, unmodified `scroll` behavior —
   while its inner `lsetview` is explicitly sized to its own
   Step-1-reported natural width (queried via `view_NoSet`, then given
   that exact width via `WidthSet`), so the table renders at full natural
   size internally regardless of how little width the surrounding text
   offered the wrapper.
8. **`lsetv.c`**: add `x_getinfo`/`x_setframe`/`x_whatisat` to `lsetview`
   (mirroring `imagev.c:1520-1587`'s structure) and answer
   `GetInterface("scroll,horizontal")` with them — this is what the
   Step 2 prototype (item 6) should also help pin down: whether panning is
   achieved via re-positioning `lsetview`'s own composed child-view/window
   tree (most likely, given `lpair`'s children are real child views) or
   needs something more involved. Confined entirely to `lsetv.c`.
9. **Visual verification** against all three fixtures: `national-grid.html`/
   `LinkedIn` unchanged (their single floor table renders via the new class
   but is usually ≤ window width already, so the scrollbar rarely engages);
   `bookrack.html` should now render at full nested natural width with a
   working horizontal scrollbar, matching the Thunderbird screenshot's
   shape — while remaining interactive (link-hover cursor and click-to-open,
   from the already-shipped `htmllinkview` feature, must still work inside
   the wrapped table, since the inner content is still ordinary `lsetview`/
   `htmllinkview`, just wrapped).
10. Re-check the still-open, distinct roadmap item ("widening the window
    doesn't let a stale table layout re-expand") — this change may
    incidentally affect it; note whichever way it goes in roadmap.md rather
    than assuming.

## Verification (end-to-end, live)

Rebuild (`cd src/atkams/messages && make dependInstall`, relink if
`messages` turns out to be statically linked into `runapp` — check
`nm -g runapp` first per this project's established convention), then
live-test in `messages` against `national-grid.html`, `LinkedIn`, and
`bookrack.html` per the checks in steps 9-10 above.

## Docs/housekeeping (once live-verified, per this project's established workflow)

- `revival/doc/roadmap.md`: **done 2026-09-23** — item 1 rewritten with
  the correct diagnosis and current status.
- `fossil add` the `LinkedIn` fixture: **done 2026-09-23**.
- `revival/doc/porting-assessment.md`: new lettered item recording the
  root-cause chain, the rejected textview-wide approach and why, and the
  three-fixture evidence trail from this session — **deferred until the
  national-grid whitespace bug (roadmap.md item 1) is also resolved**,
  so this doesn't need a follow-up rewrite.
- `revival/doc/porting-changelog.md`: dated entry — same deferral.
- New memory file (per this session's own auto-memory convention): a
  `project_table_reflow_scroll.md` pointer memory — same deferral.
- Fossil commits: one per step (1, 2), each compile-verified per touched
  file before committing, per this project's established check-in workflow
  — **done 2026-09-23** for everything built so far (crash fix, chrome-
  flicker fix included); the whitespace-bug fix (`htmlatk.c`'s
  `CellMinwidth`) is also committed, dated 2026-09-24.

## Step 3 — Elevator-drag scrolling accuracy (implemented 2026-09-24, fully fixed 2026-09-25)

> **Status (2026-09-25): DONE, wdc-confirmed working end to end.**
> Dragging now tracks proportionally through documents containing an
> oversized embedded view (national-grid.html's tableau table),
> including reaching genuine top and bottom content, and correctly
> landing there when the drag ends in either endzone. `struct
> scrollweight`/`scrollweightlist` (textv.ch/textv.c),
> `RecordScrollWeight`, `CalculateCharsPerLine`,
> `AccumulatedWeightThrough`, and `DecodeWeight` are all in place.
>
> Five real bugs were found and fixed live over the course of this step,
> the last three specifically chasing "drag into the bottom endzone
> lands at the wrong, much-earlier position" (a fresh click to the same
> spot always worked, isolating this to something drag-specific):
> 1. Dictionary-key bug (string-literal pointer identity, see
>    `ScrollWeightsKey`'s own comment).
> 2. Flat-extraWeight dead-zone bug (see `DecodeWeight`'s own comment).
> 3. `scroll.c`'s `set_frame()` declared its position parameter `int`;
>    every caller passes a `long`, silently truncated — with weighted
>    positions now routinely in the hundreds of millions, this wrapped
>    once a drag target crossed `INT_MAX`. Widened to `long`.
> 4. `textv.c`'s `DoUpdate` scroll-shortcut computed `stopline =
>    self->nLines - 2`, underflowing to `-1` whenever the viewport was
>    filled by a single oversized line (exactly the state once scrolled
>    near a document's end) — the next line then read
>    `self->lines[-1]`, one struct before the array, out of bounds.
>    Garbage `y`/`height` corrupted the blit's rectangle math. That
>    same degenerate case (`nLines==1`, the single line itself
>    oversized) is also now routed to the existing, already-reliable
>    full-redraw path instead of the scroll-shortcut, since the
>    shortcut's `redrawline == scrollLine` scenario was never designed
>    for and a deeper defect in it (traced down through `drawtxtv.c`'s
>    `view_InsertView` call and `graphic.c`'s rectangle-intersect math,
>    both of which computed provably correct values) was never fully
>    isolated — not worth chasing further into decades-old core
>    graphics code when a known-good path was one condition away.
> 5. **The actual root cause of the endzone mis-landing:** `setframe()`
>    decodes a drag target into a raw position and a pixel `off`-within-
>    that-line. A drag landing exactly at the document's end decodes to
>    `position == tl` (one past the last line) with `off == 0`.
>    `MoveBack` then snaps that down to the real last line (the
>    oversized view) — but `off`'s own re-validation against the
>    *new* line's height was gated behind `if (off != 0)`, so with
>    `off` already `0` from the out-of-range decode, the correction
>    never ran and `off` stayed `0` — landing at the very *top* of the
>    giant view instead of its bottom. Two-part fix: track the snap
>    from *both* `MoveBack` calls (the first, by-lines, didn't change
>    anything here; it was the second, by-pixels, that actually
>    performed the tl→tl-1 snap and was never being watched), and let
>    that `forceup` flag alone open the `off`-revalidation block even
>    when `off` is already `0`.
>
> All temporary `dbglog`/`/tmp/elevator-debug.log` tracing added while
> chasing these (in `scroll.c`, `textv.c`, and a since-reverted probe in
> `drawtxtv.c`) has been removed. Re-verified live against
> `national-grid.html` (full range, both endzones) and against legacy
> `.ez` scrolling (`revival/testing.ez`) to confirm no regression.

### Context

Fixing the whitespace bug above immediately surfaced a new, distinct bug:
dragging the message-body scrollbar's elevator through/past
national-grid.html's embedded tableau table lands in the wrong place —
reported variously across several live-test rounds as landing short,
warping to the top mid-drag, freezing, and (worst) "stuck at bottom,
dragging goes back to top." Three iterations were tried directly in
`scroll.c`'s `HandleThumbing` (symmetric end-clamping, swapping in
`what_is_at()`, dropping `seenLength` centering) — full blow-by-blow is in
that function's own comment, not repeated here. Each iteration improved
some symptoms and left others, because none of them addressed the actual
root cause. **Click/page navigation in the same scrollbar (left/right-click
in the track) is already correct**, including through the table — fixed
earlier by widening `textv.c`'s `FINESCROLL`/`FINEMASK` from 7 to 14 bits.
This step is drag-only.

### Root cause (confirmed via live trace, 2026-09-24)

`scroll.c`'s generic elevator math (`from_bar_to_range`/`from_range_to_bar`,
`scroll.c:980-1013`) does plain linear interpolation between a track pixel
coordinate and `total` — a range reported by the scrollee's `GetInfo`
callback. That's the correct, necessary behavior for a reusable scrollbar;
it's only as accurate as the `total`/`seen` values it's handed.

`textv.c`'s `getinfo()` (`textv.c:2461-2494`) reports `total->end = tl
<<FINESCROLL`, where `tl` is `text_GetLength()` — the document's raw
**character count**. Every character is implicitly treated as occupying
equal vertical space. That assumption is harmless for ordinary text (line
density is fairly uniform) but breaks badly for an embedded view: a table
occupies exactly one character in the text stream (confirmed:
`linedesc.nChars`/`.containsView` in `textv.c:74-79`), yet can occupy
thousands of real pixels. `position()` (`textv.c:2438-2459`) already
carries a *fine* sub-position offset for such a line (the very thing
`FINESCROLL` was widened to hold more of), but only *within* that one
line — it does nothing to correct `total`'s coarse, per-character
granularity *around* it.

Net effect, confirmed by re-tagging every trace line with its owning
`scroll`/`textview` instance (`self=%p`, added to the existing
`dbglog`/`/tmp/elevator-debug.log` tracing in both files) and re-running
the drag live: dragging through the table's neighborhood, `getinfo()`'s
real `seen` width balloons to ~188,000,000 units against a `total` of
188,186,624 — essentially "one screenful is almost the whole document" —
while the elevator's own captured `seenLength` (16,194–38,895,616 across
different drag-start points in the *same* document, a 2400x spread)
reflects whatever ordinary-text density happened to be under the cursor
when the drag began. Dragging across that band therefore either takes a
huge, invisible mouse motion to register any content change ("stuck"), or
crosses the table's entire pixel range for a track motion of one or two
pixels ("warps").

### Why click/page already work and drag doesn't

Click/page resolve via `whatisat()` → `textview_Locate()`
(`textv.c:2496-2509`), which walks the *currently-rendered* lines directly
using their real per-line pixel `y`/`height` — genuinely pixel-accurate,
independent of `total`. It works iteratively: `DoRepeatScroll`'s timer
re-evaluates it against freshly-rendered content on each tick, converging
over a few ticks for a far-off click. Drag has no such convergence loop —
`HandleThumbing` calls `from_bar_to_range()` fresh on every mouse-move
event and must get a usable answer in one shot from `total`/`seen` alone.
(An earlier attempt to reuse `what_is_at()`/`textview_Locate()` directly
for drag was tried and made things worse — confirmed live as "bounces
around... doesn't correlate with the mouse" — because drag's redraw
(`im_ForceUpdate()`) is asynchronous: rapid mouse-move events can outrun
the pending redraw, so `textview_Locate()` resolves against stale
`self->lines[]`. Click's timer-based repeat avoids this by construction;
continuous drag doesn't. This is why the fix has to correct `total`
itself, not swap in click's resolver.)

### Why the "obviously correct" fix (real cumulative pixel height) doesn't fit this architecture

The clean fix would be: make `total` genuinely proportional to real
document pixel height, end to end. That's not available cheaply.
`self->lines[]` (what `getinfo()` reads `y`/`height` from) is **not the
whole document** — it's a virtualized window of only the currently
rendered/visible lines, confirmed by reading `nLines`'s management
throughout `textv.c` (grows/shrinks as the view scrolls, `DoUpdate` is
where lines get (re)computed). Computing a true whole-document cumulative
pixel height would mean either laying out the entire document up front
(defeats the lazy-rendering this class is built around, and doesn't scale
to long documents) or maintaining a persistent per-line height cache for
content that may never be visited. Neither is a small change.

### Scoped design actually being proposed

Don't touch the character-count basis for ordinary text at all — it isn't
broken, and per-line real heights for most of a document simply aren't
known without full layout. Instead, correct **only** for lines that
`containsView` (an embedded inset, e.g. the tableau table) and are large
enough to matter:

1. **No new field on any dataobject — the value already exists.** wdc
   asked (2026-09-24) why this couldn't just be filled in as the document
   is instantiated rather than tracked in a side cache. The pixel height
   itself doesn't need a new place to live: `textview_LineRedraw` already
   computes and holds it, generically, for *any* embedded view the moment
   it lays that line out — `tl->height` at `textv.c:1127` (the real paint
   path, `DoUpdate`) and the same field via the measurement-only
   `textview_GetHeight` mode used by `MoveByPixels`/`MoveByLines`/
   `MoveForward`/`MoveBack` (~8 more call sites) — no table-specific or
   even HTML-specific code involved, so this covers the already-confirmed
   analogous case (a large raster in a plain `.ez` file) for free, not
   just tables. Adding a field to `lset.ch` (mirroring `minwidth`) was
   considered and rejected: it would only cover HTML tables, and the
   value it would hold is already sitting in `tl->height` at the exact
   moment it's needed — a new field would just be a second place for the
   same number to go stale in.
2. **What's actually missing is somewhere for that number to survive
   scrolling away.** `self->lines[]` is a moving window (`textv.c`'s
   `nLines` grows/shrinks as the view scrolls) — once a line scrolls back
   out, its `linedesc` (and the `tl->height` it held) is gone. `total`
   needs to reflect lines the view isn't currently looking at, so
   `struct textview` gets a small, sorted `{position, extraWeight}` list
   — populated (insert-or-update by position) from a single small helper
   called both from `DoUpdate`'s paint path and from `LineRedraw`'s
   measurement-only call sites, every time either one discovers a line
   whose `tl->height` is meaningfully larger than ordinary text (skip
   the rest, so an inline image or small inset doesn't grow the list).
   `extraWeight` is that height, `FINEGRID`-scaled, minus the 1-character
   baseline it would otherwise have contributed. This is genuinely just
   an index of *where* an already-computed number was seen, not a
   separate computation of the number itself — the actual geometry stays
   sourced from the view's own real, already-happening layout, exactly
   where wdc's question was pointing. (Whether the underlying view's
   height is itself stable across re-layout — e.g. not width-dependent —
   still matters for the index entry to stay valid without needing to be
   refreshed; for the table case this is already true, since
   `lsetscrollview`'s `DesiredSize` was fixed earlier this session
   (`lsetscrlv.c`, 2026-09-23) to report a constant, width-independent
   height.)
3. **Encode side** (`position()`, `textv.c:2438-2459`, plus `getinfo()`'s
   `total->end`): after the existing coarse `pos<<FINESCROLL` + fine-offset
   computation, add the accumulated `extraWeight` of every cached entry at
   or before `pos`. `getinfo()`'s `total->end` becomes `tl`'s own encoded
   value via this same path (today it bypasses `position()` entirely and
   just does `tl<<FINESCROLL` — that bypass goes away).
4. **Decode side** (`setframe()`, `textv.c:2513` on, specifically the
   `position >>= FINESCROLL` prologue): subtract the same accumulated
   `extraWeight` for entries at or before the encoded value, before
   treating the result as a real character position and handing it to the
   existing `MoveBack(..., textview_MoveByLines, ...)` /
   `MoveBack(..., textview_MoveByPixels, ...)` walk — unchanged from here
   on, since that walk is already genuinely pixel-accurate once given a
   close starting guess.
5. Click's own path (`whatisat`/`textview_Locate`) does not change — it
   never depended on `total`, and doesn't call the new encode/decode
   correction. It will keep working exactly as it does today; the new
   `position()` weighting is additive and doesn't change which real
   character/pixel position any existing caller resolves to, only how
   coarse positions are spaced relative to each other in scrollbar space.

**Accepted, documented limitation** (refined 2026-09-24, after wdc asked
whether the correction would end up trying to scale by content that
hasn't been rendered yet — it can't, and this is the honest shape of
that): `total` under-reports by the `extraWeight` of any oversized line
not yet *discovered* — full paint or measurement-only walk, either
counts. Because `setframe`'s own decode walk (used by every drag step,
every page, every click) already measures the lines it crosses to land
correctly, discovery mostly keeps pace with navigation itself rather than
surfacing as one late, isolated correction — the drag/page operation that
first reaches an oversized line's neighborhood is usually the same
operation that discovers it, so the resulting `total` bump is folded into
that same step's own redraw rather than arriving as a surprise on a later,
unrelated one. It is *not* zero-risk: a single large jump (a fast drag
motion, or an endzone jump) could still discover several oversized lines'
worth of weight in one step, which the elevator will visibly reflect as a
one-time size/position correction at that moment, and this hasn't been
verified live yet — only reasoned from `textview_LineRedraw`'s call
sites, not traced. Worth watching for specifically during verification:
drag a large, never-before-scrolled document with an oversized view
somewhere in the untouched middle, single continuous gesture, no prior
click/page pass over it first.

**Blast radius**: entirely within `textv.c` (`position()`, `getinfo()`,
`setframe()`, plus the new position-index field/helpers) — smaller than
the version of this design discussed earlier in the day, which would also
have touched `lset.ch`. `scroll.c` is not expected to need further
changes — its existing linear interpolation becomes correct once handed a
properly-weighted `total`, which is the whole point of pushing the fix
here rather than continuing to patch `HandleThumbing`. `lpair.c`/
`drawtxtv.c`/`lset.ch` remain untouched, consistent with this plan's
original scoping.

### Verification plan

- Rebuild `libtext.a`/`libsupviews.a`, reinstall, relink `runapp` — same
  procedure already used this session (`rm -f lib*.a && make lib*.a`,
  `install -m 644` + `ranlib` into `build/lib/atk/`, then `cd
  src/atk/apps && rm -f runapp && make dependInstall`).
- Clear `/tmp/elevator-debug.log`, reproduce the national-grid.html drag
  (bottom end-zone → drag elevator up → drag back down), then filter the
  trace to the one dragged `scroll`/`textview` pair using the `self=`/
  `scrollee=` tags already added this session (`grep -E
  "self=0x...|self=0x..."`) — same method used to root-cause this bug, so
  it's already proven to isolate the right signal.
- Confirm the drag tracks proportionally through the table's band (no
  stuck/warp), confirm `revival/testing.ez` (the big-raster `.ez` file)
  and ordinary documents are unaffected, and re-confirm click/page are
  still exact (should be untouched, but worth a live re-check since
  `position()` is shared code).
- **Done 2026-09-25**: all temporary `dbglog` tracing stripped from
  `scroll.c`/`textv.c` (and a since-reverted probe in `drawtxtv.c`), the
  elevator-drag item in `roadmap.md` folded from "Now open" to closed.

### Implementation notes (what actually shipped, differs from the plan above in a few places)

- No `lset.ch` field, as planned — the weight blob is reached via
  `dictionary_Insert`/`LookUp` (`dict.ch`), keyed by a static sentinel's
  address (`ScrollWeightsKey`), NOT a string literal — dict.c compares
  keys by pointer identity, and with `-fwritable-strings` each textual
  occurrence of a string literal gets its own address, so a string key
  silently never matches across call sites. Found live (`RecordScrollWeight`
  showed ACCEPT, `getinfo`'s `total` never moved) before being traced to
  `dict.c`'s `dd->id == id` comparison.
- `extraWeight` is NOT simply added as a flat bonus on top of the normal
  encoding — an early version did this and created a huge "dead zone" in
  the track (any drag landing in it collapsed to nearly the same
  position — the drag-side analog of the exact click-side bug this
  feature exists to fix). `DecodeWeight` instead proportionally rescales
  wherever an encoded value falls within a weighted line's full span
  (`extraWeight + realOffMax + 1`) back down to that line's narrow real
  `off` range (`0..height/FINEGRID`) — see its own inline comment for the
  derivation. `struct scrollweight` carries `height` (not just
  `position`/`extraWeight`) so decode can compute `realOffMax`.
- `SCROLLWEIGHT_CHARS_PER_LINE` is not a fixed constant — an early version
  hardcoded 60, which undershot for a wide window (more real characters
  actually fit per line, so a fixed low estimate understates how much
  weight an oversized view deserves relative to real text).
  `CalculateCharsPerLine` derives it dynamically from the view's current
  width and font metrics (`fontdesc_FontSummary`'s `maxSpacing`),
  mirroring `CalculateLineHeight`'s existing pattern.
- **Endzone mis-landing, fixed 2026-09-25** (see the Status note at the
  top of this section for full detail): the initial suspicion —
  `scroll.c`'s cached `cur`/`des` bar state going stale mid-gesture —
  turned out not to be it. The real chain was three separate bugs: an
  LP64 `int`/`long` truncation in `scroll.c`'s `set_frame()`, an
  out-of-bounds `self->lines[-1]` read in `textv.c`'s `DoUpdate` scroll-
  shortcut, and (the actual root cause) `setframe()`'s `off`-
  revalidation being unreachable when a drag decoded exactly to the
  document's end (`off` already `0`, so the `off != 0` guard skipped
  the very clamp that would have corrected it after `MoveBack` snapped
  to the real last line).

### Open questions (never depended on, safe to leave as follow-ups)

- Exact numeric threshold for "large enough to cache" — shipped as
  `height <= typicalHeight * 3` (reject). Works for the fixtures tested;
  not stress-tested against many small-but-not-tiny embedded views.
- Whether the weight list needs any eviction/size bound for pathological
  documents with many oversized insets — likely not, given how rare these
  are, but not stress-tested.