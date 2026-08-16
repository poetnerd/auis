# Task: replace `table`/`spread` with `lset`/`lpair` for HTML table rows (lset-table-reflow)

## Read first

Read `revival/doc/sonnet-playbook.md`, then `revival/doc/html-mail-rendering-design.md`
and `revival/doc/html-atk-renderer-prompt.md` (Stage 3's own prompt — all 5
gates are done, this is a follow-up, not a restart). This task lives on the
`html` branch (`~/src/AUIS/html/`), same as the rest of the HTML-mail-
rendering project.

## The problem, confirmed not hypothetical

`src/atkams/messages/lib/htmlatk.c` currently maps every HTML `<table>` onto
ATK's native `table`/`spread` object (`src/atk/table/`). This was the right
call for the recursive-embedding shape of a table cell (see
`PAPERS/atk/Sherman.Alloc` lines 663-1066, the real precedent that grounded
that decision), but `table`/`spread` turns out to be architecturally wrong
for one specific, extremely common real-world HTML pattern: `<table
width="100%">` used purely as a layout/stacking mechanism (not real tabular
data) — the overwhelming majority of real marketing/newsletter HTML mail
(see `revival/tests/html-fixtures/`) uses exactly this shape.

**Root cause, confirmed by reading the actual ATK source, not assumed:**
`table`/`spread` is a fixed-pixel-grid model. `ATK_DEFAULT_COLUMN_THICKNESS`
(`src/atk/table/table.ch:44`) is a bare 99 pixels, and `spread__DesiredSize`
(`src/atk/table/spread.c:290-311`) computes its own width from the sum of
each column's *fixed* `table_ColumnWidth` — even when a container imposes a
narrower `view_WidthSet` width, spread just agrees to report that width
without ever recomputing/shrinking its columns to fit. There is no resize-
driven recompute anywhere in `spread.c` (grepped for
`Reshape`/`WantsToBeBigger`/`SetSize`/`ObservedResized` — nothing).
`htmlatk.c`'s own `BuildTableGrid()` currently works around the 99px default
by reading an explicit HTML `width=` (pixel-only) attribute and calling
`table_ChangeThickness()` once at construction time (`ParseWidthPixels()`/
`HTML_TABLE_DEFAULT_WIDTH`, added 2026-08-16) — this fixed the original
"one word per line" symptom, but the underlying columns are still frozen at
whatever width they were given at construction. Resize the containing
window and the table does not reflow; it just gets a horizontal scrollbar.
Confirmed live via `ez` (see the `writeds`/`ez` verification technique in
`sonnet-playbook.md`'s "Verification tools" section) on a real message
(National Grid, `+IMAP1H2NM9J0002CI9` in wdc's mirrored INBOX).

## The proposed fix, confirmed via real ATK source and docs

`lset` (`src/atk/adew/lset.c`/`.ch`) + its view `lsetview`
(`src/atk/adew/lsetv.c`/`.ch`), built on `lpair`
(`src/atk/supportviews/lpair.c`/`.ch`), is architecturally the right tool
instead — confirmed two ways:

1. `build/doc/adew/Tutorial.doc`, section "1.2 Working with lset" — lset
   splits a window into rectangular regions via recursive horizontal/
   vertical splits (interactively: the "Split Horizontal"/"Split Vertical"
   menu commands).
2. **The actual resize math, read directly**: `lpair.c:385`:
   `l->objcvt[i] = (l->objsize[i] * totalsize) / 100;` — `objsize[i]` is a
   **percentage** (0-100, clamped at `lpair.c:269-272`), and the real pixel
   size (`objcvt[i]`) is recomputed from that percentage against
   `totalsize` (the *current* available space) every time `DesiredSize`
   runs (`lpair__DesiredSize`, `lpair.c:325`). This is genuine proportional,
   resize-aware layout — confirmed by `lsetview__ObservedChanged`
   (`lsetv.c:545`) reacting to `Data(self)->pct` changes by updating
   `self->header.lpair.objsize[1]` and requesting a redraw. The opposite of
   `table`'s fixed-pixel columns.

**Important constraint, not yet designed around**: `lpair` is strictly
binary (`obj[0]`/`obj[1]`, two slots only — confirmed in `lpair.c`). `lset`'s
N-way splitting works by recursively subdividing one region into two,
repeatedly. Representing an HTML `<tr>` with N `<td>`s means building a tree
of N-1 nested binary splits, each carrying its own percentage — not a flat
N-column construct the way `table_ChangeSize(rows, cols)` gives for free.
This is the main new complexity Gate 1 below needs to resolve.

## Scope questions this task needs to answer, not guess at

- **Full replacement or hybrid?** Real tabular data (Amazon's actual
  shipment/price grid in fixture 01, say) may be *better* served by a real
  fixed grid than by a tree of percentage splits — a genuine data table
  isn't a resize-fluid layout the way a `width=100%` wrapper table is. Does
  this task replace `table`/`spread` everywhere, or only for the common
  "layout wrapper" case (heuristically: `width="100%"` and/or `border="0"`
  and/or single-column), keeping `table`/`spread` for tables that look like
  real data? Don't assume either answer — read the fixture corpus's own
  real-world table shapes (`revival/tests/html-fixtures/README.md` already
  has relevant stats) before deciding.
- **`colspan`/`rowspan`**: `table`'s `JOINED`/`SetInterior` mechanism has no
  obvious `lset` equivalent (a percentage-tree split doesn't have a native
  "this region spans two rows" concept). Needs real design thought, not a
  hand-wave — may be a good signal for the hybrid-vs-full-replacement
  question above (a table using colspan/rowspan is more likely to be real
  tabular data than a layout wrapper anyway).
- **Nesting**: the current recursive `table_Imbed(T, "text", &cellchunk)` +
  `htmlatk_Render()`-into-a-fresh-"text"-object pattern (see `htmlatk.h`'s
  own judgment-call log) would need an `lset`-shaped equivalent — how does
  an `lset` region host arbitrary rich content (text + image + nested
  table/lset)? Gate 1 needs to find the real construction API for this
  (`lset__InsertObject(name, viewname)` is the entry point spotted so far,
  not yet fully traced through to how you attach a *specific already-built*
  dataobject/view pair to a region rather than asking lset to construct a
  fresh empty one).

## Gate 1 (research — no code)

Map the exact `lset`/`lpair` construction API with file:line evidence,
same rigor as Stage 3's own Gate 1 did for `table`/`spread`/`text`:

1. How does code (not a human using Arbcon interactively) programmatically
   create an `lset`, split it N ways, set each region's percentage, and
   attach a specific dataobject+view to each region? Trace
   `lset__InsertObject`/`lsetview` construction all the way through — the
   Tutorial's interactive workflow (click, paste, drag dividers) is a
   *precedent* for what's possible, not the API surface itself.
2. Confirm (empirically, via a small headless test using
   `htmlatktest.test`'s own `InitATK()`/`class_NewObject()` pattern — no
   X11 needed for construction, same as `table`/`text` needed none) that an
   `lset` tree can actually be built, `Write()`-ed to a real datastream,
   and read back correctly — same `writeds`/`roundtrip` verification loop
   already used for `table`/`text` (see `sonnet-playbook.md`).
3. Answer the scope questions above with evidence, not guesses, and report
   back a concrete recommendation (hybrid vs. full replacement, and how
   colspan/rowspan/nesting would actually work) before writing any real
   `BuildTableGrid()`-equivalent construction code. This is explicitly a
   design gate, not just a mechanics gate — the html-atk-renderer Gate 1
   was mechanics-only because the target (map tags to `table`/`text`
   objects) was already decided; this task's target isn't decided yet.

## Ground rules

Same as `html-atk-renderer-prompt.md`'s: no `fossil add`/commit, leave
everything uncommitted for direct review. Compile-verify against a clean
baseline (0 errors, only the expected `-Wdeprecated-non-prototype` class).
No concurrent builds. Never touch `~/.IMAP`/`~/preferences`/`~/.netrc`. Use
`revival/tests/html-fixtures/` (read-only) as real-world test input. Don't
modify `htmlpart.c`/`htmltext.c`/`htmlpart.h`/`htmltext.h` (Stage 1/2,
unrelated to this task) or the design doc itself without flagging it first.
**Do not modify the existing `table`/`spread`-based path in `htmlatk.c`
until Gate 1's hybrid-vs-full-replacement question is actually answered** —
if the answer is "hybrid," the existing code for genuine data tables stays
as-is and this task only adds a new path alongside it, not a rip-and-replace.

Remember `text822.do` is a *dynamically loaded* module — `make text822.do`
alone does not update what `messages` actually loads at runtime. After any
change touching `text822.c`/`htmlatk.c`, `make install` in
`src/atkams/messages/lib/` and verify with `md5` that
`build/dlib/atk/text822.do` matches the freshly built local copy before
considering a fix "live." This bit wdc directly on 2026-08-16 — several
real fixes looked broken in `messages` for hours because only the local
build, never the installed `.do`, had actually been updated.

## Final report

Same format as other prompts in this directory — what Gate 1 found (with
file:line evidence), the hybrid-vs-full-replacement recommendation and why,
and (if time allows within this session) how far into actual construction
code you got. This is very plausibly a multi-session task; a solid Gate 1
alone, cleanly reported, is a legitimate stopping point.
