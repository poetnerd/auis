# Fixed-width HTML tables with horizontal scroll (Thunderbird-style)

> **Status (2026-09-23):** Steps 1 and 2 below are both implemented and
> live-verified — canonical status lives in `roadmap.md`'s Open items
> (item 1), not duplicated here. Short version: the crash and the
> chrome-flicker/double-frame bug are fixed; national-grid.html's
> stray-whitespace rendering bug is still open and not yet root-caused.

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
  flicker fix included); the eventual whitespace-bug fix gets its own
  commit(s) on top.