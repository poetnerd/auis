# M3 Wave 4, Batch I2: `atk/eq`, `atk/figure`, `atk/chart`, `atk/table`, `atk/rofftext`, `atk/raster/cmd` — ansify + `-pe`/`.eh` rollout, closes Wave 4 — REPORT

## 1. Status

All 6 directories gate clean, twice each. The wave-end tree-wide gate
(`make -C src dependInstall`) is clean, twice, except the two
documented pre-existing `contrib/zip/utility/ltapp.c` errors (Wave-1/2
baseline, queued for Wave 7 C2 — confirmed identical both runs, not
new fallout). Stopped here per the prompt's Gate section. No fossil
commit made. `fossil status`/`fossil extras` output in §6.

`m3-i2-insets-batch2-session.diff` (tree root, 18,449 lines) has the
full diff. This report covers what's in it and why.

## 2. What I did, in order

Read `sonnet-playbook.md`, `rollout-procedure.md`,
`m3-i2-insets-batch2-prompt.md`, `m3-rollout-runbook.md`'s "Current
standing per-batch checklist" and I1 findings entry, per the prompt's
instructions — this is a flagged-risky batch with orchestrator
pre-diagnosis already done, so I went straight to the real
(non-dry-run) conversion work per directory, in the prescribed order:
`atk/eq` → `atk/figure` → `atk/chart` → `atk/table` → `atk/rofftext`
→ `atk/raster/cmd`, each fully gated (twice) before starting the
next, then the wave-end tree-wide gate.

For each directory: real `ansify --dir` (or explicit file list for
`eq`/`rofftext`, excluding the generated `eqparse.c`/`num.c`) →
triage any DRIFT/skip → add `CLASSFLAGS = $(CLASSINCLUDES) -pe` →
force-regenerate `.eh` → re-run `ansify` on anything that failed
pre-`-pe` → fix real `-pe` fallout → checklist items 4/6 → anchored
`malloc`/`free`/`realloc`/`calloc` grep → item-8 stranded-forward-decl
grep → subtree-local gate (`make clean && make depend && make -k
install`, twice).

## 3. Per-directory `ansify --dir` results vs. pre-diagnosis

### `atk/eq` (5 real `.c` files; `eqparse.c` excluded, confirmed
`fossil finfo` returns "no history for file")

Real `ansify` run matched pre-diagnosis: `.ch` count 2 (eq, eqv), no
surprises in scope. Conversion counts (methods/classprocs/helpers):

| File | methods | classprocs | helpers |
|---|---|---|---|
| draweqv.c | 4 | 0 | 9 |
| eq.c | 22 | 3 | 2 |
| eqv.c | 13 | 3 | 0 |
| eqvcmds.c | 1 | 0 | 41 |
| symbols.c | 0 | 1 | 0 |

`eq.c`/`eqv.c` initially failed the pre-`-pe` compile gate with only
truncated warnings visible — matches the **Pilot A `eq__WriteFILE`
finding** documented in `porting-assessment.md` (`char sep` promotion
conflict against the not-yet-typed `.eh`); confirmed by manual
`--no-compile` + direct `make` that this is exactly that shape, and
it resolved cleanly once `-pe` was added and `.eh` regenerated.

**Real bug found**: `eqv.ch`'s `InitializeObject` declared `self` as
`struct eq *` instead of its own `struct eqview *` — same species as
I1's `cmapv.ch` finding (restating a sibling/unrelated class's type
instead of the class's own). Confirmed via field-access errors
(`self->off_x` etc. are `eqview` fields) and via `eqv.eh`'s own
`-pe`-generated `InitializeObject` prototype (which classpp always
types to the class's own self regardless of `.ch`, per the M1
mechanics note) already using `struct eqview *`. Fixed `eqv.ch`,
rebuilt the signature DB, reconverted `eqv.c` clean.

## `atk/figure` (17 files) — extra scrutiny per its M1 Pilot B history

Confirmed at the start, per the prompt's instruction: `figobj.ch`'s
`Build` still reads `(struct figview *v, enum view_MouseAction
action, ...)`, `EnumerateSelection` is still `void *rock`,
`ToolName`/`ToolModify`/`Instantiate` still correctly keep `long
rock`. None of Pilot B's three original fixes regressed.

9 of 17 files converted clean on the first pass; 8 failed with the
expected pre-`-pe` transient pattern and converted clean after `-pe`
+ regen. Conversion counts:

| File | methods | classprocs | helpers |
|---|---|---|---|
| figattr.c | 4 | 2 | 1 |
| figio.c | 0 | 2 | 4 |
| figobj.c | 33 | 3 | 1 |
| figoell.c | 5 | 2 | 0 |
| figogrp.c | 21 | 3 | 3 |
| figoins.c | 7 | 2 | 0 |
| figoplin.c | 22 | 4 | 5 |
| figorect.c | 16 | 2 | 1 |
| figorrec.c | 4 | 2 | 0 |
| figospli.c | 5 | 3 | 3 |
| figotext.c | 16 | 4 | 11 |
| figtoolv.c | 7 | 3 | 65 |
| figure.c | 22 | 3 | 7 |
| figv.c | 34 | 3 | 44 |
| fontsamp.c | 6 | 3 | 1 |
| fontsel.c | 3 | 3 | 1 |
| fontselv.c | 4 | 3 | 6 |

**New finding not in the pre-diagnosis, surprising, worth flagging
prominently: an include-order bug, not previously documented in any
M3 batch.** Several files (`figobj.c`, `figogrp.c`, `figure.c`,
`figoplin.c`, `figotext.c`, and — caught only via the staleness lesson
below — `figorect.c`, `figoins.c`) `#include` their own `.eh` file
*before* the header that fully defines an `enum` type used in one of
their typed method prototypes (`enum view_MouseAction` from
`view.ih`, `enum figobj_HitVal` from `figobj.ih`). Under K&R this was
invisible; under `-pe`'s typed prototypes, C's rule that an `enum`
tag first mentioned inside a function-prototype parameter list gets
*prototype scope* means each early, enum-before-definition mention
creates a distinct incomplete type, so the same-named enum used later
in the real definition doesn't match — `conflicting types` errors on
every method using that enum. Confirmed the fix by finding the
already-`-pe`'d, working precedent: `atk/text/textv.c` includes
`view.ih` (line 54) *before* `textv.eh` (line 60). Fixed by moving
each affected file's own `.eh` include to the end of its include
block, matching that convention. This is a source-file defect
(pre-existing, 35-year-old include order that only ANSI prototyping
exposes), not an `ansify` or `classpp` bug — flagging it because nothing
in the M3 runbook's taxonomy names this shape yet.

**Real bugs (`.ch`-vs-implementation), fixed at the `.ch` level:**
- `figobj.ch` and `figogrp.ch`'s `SetParent` both declared the third
  parameter as `struct *fig ancestor` — a malformed declaration
  missing the actual struct tag (should be `struct figure *`).
  Confirmed against both real K&R implementations (`figobj.c:531`,
  `figogrp.c:494`, both typing the parameter `struct figure *
  ancfig`) and call sites in `figure.c`. Fixed both `.ch` files to
  `SetParent(long parentref, struct figure *ancestor)`.
- `figorrec.ch`'s `InitializeObject` declared `self` as `struct
  figoell *` instead of `struct figorrec *` — same species as `eqv.ch`
  above (mistyped to a sibling class, likely copy-paste). Fixed.

**Real bugs, fixed at the `.c` level (generic-param-plus-cast
pattern, same species as I1's `srctextv.ch`/`asmtextv.ch` finding):**
- `figv.c`'s `SetDataObject(struct figview *self, struct dataobject
  *fig)` body accessed `fig->root`/`->originx`/`->originy` (`figure`
  fields) directly on the generic `dataobject *` parameter. Added a
  local `struct figure *figfig = (struct figure *) fig;` cast and
  retargeted the three field accesses — matches `celv.c`'s
  established convention.
- `figv.c`'s file-local static helpers `EnumSelSplot` (called from
  `figview__EnumerateSelection`, whose `rock` is `void *` per Pilot
  B's original fix) and, in `figure.c`, the file-local `EOT_AllArea`/
  `EOT_OverlapArea`/`EOT_IncludeArea` (called from
  `figure__EnumerateObjectTree`, whose `rock` is likewise `void *`)
  all still declared their own `rock` parameter as `long` — the
  file-local-helper counterpart of Pilot B's original "rock idiom"
  finding, this time in helpers rather than `.ch`-declared methods.
  Confirmed each body treats `rock` purely opaquely (passed through,
  no arithmetic) before retyping to `void *`.
- `fontselv.c`'s `ObservedChanged` — `fontselv.ch` declares it with
  the covariant `struct fontsel *fontsel`, but the base class
  (`view.ch`) declares the generic `struct observable *changed`, and
  `-pe` exports the base type regardless of the override's claim
  (same rule as I1's finding). Added a local `struct fontsel *fsdobj`
  cast, retargeted 6 field-macro call sites.

**Confirmed dead/unwired code, not M3 fallout:** `figospli.c` defines
`figospli__InitializeClass`, but `figospli.ch` never declares
`InitializeClass` — and, unlike `InitializeObject`/`FinalizeObject`
(which classpp always auto-wires per the M1 mechanics note),
`InitializeClass` is only wired if declared. Confirmed via
`figospli.eh` having zero mentions of `InitializeClass` anywhere.
Left as K&R (harmless, unreachable) — flagging the possibility that
this was meant to run once-per-class setup that's silently never
executed, but that's a behavior-changing call outside this batch's
scope, not a conversion fix.

**Process finding, not a source bug: a real methodological gap in my
own verification.** `figoell.c`, `figoins.c`, and `figorect.c`
succeeded on the very first (pre-`-pe`) `ansify` pass and I never
edited them again — but `make figorect.o`/`figoins.o` "succeeded" when
I spot-checked them mid-session because the `.o` was stale (matches
Pilot A finding #3: `.o` files don't depend on `.eh`/`.ih` in the
generated Makefiles). Only the real `make clean && make depend &&
make -k install` gate — which deletes every `.o` and forces a true
fresh compile of all 17 files — caught that `figorect.c` and
`figoins.c` had the same include-order bug as the other 5. Both fixed
the same way; re-ran the full clean gate afterward, clean twice for
real. Lesson applied to every subsequent directory in this batch:
individually spot-checking an untouched file with `make X.o` is not
trustworthy; only a full `clean` gate is.

### `atk/chart` (13 files)

`.ch` count 12 as pre-diagnosis expected. **One deviation from
convention worth flagging**: unlike every other `-pe`'d directory,
`atk/chart`'s Imakefile has **no M2 `COMPILERFLAGS` override at all**
— its `COMPILERFLAGS` comes from the tree-wide imake default
(`-Wno-implicit-function-declaration`, not `-Werror=...`). No
`m2-*-REPORT.md` mentions `atk/chart` either. This is out of scope to
fix here (the task is `-pe` only), but it means this directory has no
compiler net at all for implicit declarations — I treated the
malloc/stdlib blind-spot check as load-bearing here, not just
routine.

Conversion counts:

| File | methods | classprocs | helpers |
|---|---|---|---|
| chart.c | 16 | 3 | 16 |
| chartapp.c | 2 | 1 | 0 |
| chartcsn.c | 4 | 1 | 0 |
| chartdot.c | 4 | 1 | 0 |
| charthst.c | 5 | 1 | 0 |
| chartlin.c | 4 | 1 | 0 |
| chartmap.c | 4 | 2 | 0 |
| chartobj.c | 14 | 2 | 18 |
| chartp.c | 0 | 0 | 9 |
| chartpie.c | 4 | 2 | 2 |
| chartstk.c | 4 | 1 | 0 |
| chartv.c | 14 | 4 | 15 |
| chartx1a.c | 1 | 0 | 1 (+1 hand-converted) |

**The 6 `FinalizeObject` DRIFT findings** (`chartapp`, `chartcsn`,
`chartdot`, `charthst`, `chartlin`, `chartstk`, all reporting `.c has
2 params, .ch has 0+1`) match the pre-diagnosis exactly — confirmed
each is the documented bare-`FinalizeObject();` shape, not the broken
B3 shape, and confirmed **no fix is needed**: after `-pe` was on,
`chartapp.c`'s still-K&R `FinalizeObject` definition compiled clean
against the typed `.eh` (both params are pointers, no
promotion-narrowing conflict), proving the pre-diagnosis ruling
correct empirically, not just by inspection.

**Real bugs found, all new for this batch:**
- `chart.ch`'s and `chartv.ch`'s `Create` classprocs both declared
  their specification parameter as a bare, untyped token
  (`chart_Specification` / `chartv_Specification`, no `*`, no
  parameter name) — the exact "typeless `.ch` declaration" shape from
  Pilot B finding #1. Confirmed each name is a real typedef
  (`typedef struct chart_specification chart_Specification;`) declared
  later in the same `.ch`, and that the real K&R implementations type
  the parameter as a pointer, iterated with `specification++` and
  `specification->attribute` (array-of-structs sentinel-terminated
  idiom). Fixed both to `struct chart_specification *specification` /
  `struct chartv_specification *specification`.
- `chart.ch`'s `Apply` used unusual cast-style K&R parameter syntax
  (`(long(*)())proc`), which both `classpp` and `ansify` render
  awkwardly with redundant grouping parens around the parameter in the
  `.c` definition (`( long (*proc) () )`), producing a real parse
  error (`proc` becomes an unnamed abstract declarator, not a name).
  This is a genuine `ansify` parser limitation for this syntax shape,
  not something I fixed in the tool (out of scope, tool construction
  stays top-level) — worked around by rewriting `chart.ch`'s
  declaration to conventional syntax (`long (*proc)()`), after which
  both `classpp`'s `.eh` export and `ansify`'s `.c` conversion render
  cleanly with no parens ambiguity.
- `chartapp.ch`'s `InitializeObject(struct chartapp *)` (type given,
  no parameter name) round-trips through `classpp -D`'s signature dump
  as `args: struct  *` — **the class tag itself goes missing** when a
  classproc parameter's declared type textually equals the class's own
  self type. Confirmed directly in `build/desc/chartapp.desc`. This is
  a `classpp -D` dump bug, not an `ansify` parsing bug (the malformed
  text is already wrong in the DB `ansify` reads from) — bounded to
  this one instance, hand-fixed `chartapp.c`'s definition directly
  rather than chasing a new classpp fix.
- `chartx1a.ch` has no `classprocedures` section at all, yet
  `chartx1a.c` defines `chartx1app__InitializeObject`. Unlike
  `figospli`'s dead-code case above, this one **is live** — confirmed
  via `chartx1a.eh` showing it wired into the standard
  classpp-generated `New()` constructor path regardless of `.ch`
  declaration (matches the M1 mechanics note that `InitializeObject`/
  `FinalizeObject` are always auto-generated). This is a genuine
  `ansify` signature-DB coverage gap (the DB only captures classprocs
  explicitly declared in a `.ch`), not dead code. Hand-converted the
  one function to ANSI using the established `(struct classheader
  *classID, struct chartx1app *self)` convention.

### `atk/table` (10 files)

`.ch` count 2 (`table`, `spread`) as expected. Conversion counts:

| File | methods | classprocs | helpers |
|---|---|---|---|
| eval.c | 0 | 0 | 12 |
| funs.c | 0 | 0 | 19 |
| hit.c | 0 | 0 | 18 |
| keyboard.c | 0 | 0 | 25 |
| menu.c | 0 | 0 | 24 |
| print.c | 0 | 0 | 9 |
| spread.c | 14 | 4 | 10 |
| tabio.c | 0 | 0 | 12 |
| table.c | 26 | 3 | 10 |
| update.c | 0 | 0 | 17 |

No `.ch`-vs-implementation bugs here — purely mechanical conversion
plus checklist-item-8 narrow-param forward-decl fixes (`IsNotSeparator`
in `hit.c`; 14 `k_*` handlers sharing `(struct spread *V, char ch)` in
`keyboard.c`; `printVal` in `print.c`; `updateString`/`updateValue` in
`update.c`). `print.c`/`update.c` both showed a red herring first: an
`extern char * fcvt();` K&R declaration conflicting with macOS
`_stdlib.h`'s real `fcvt` prototype — confirmed via a pristine-file
baseline compile that this is **only a warning, not an error**, in
both files' unconverted state (exit 0), so it was never the actual
blocker; the real errors were the narrow-param conflicts listed
above, which happened to sit right after the `fcvt` warning in
`ansify`'s truncated 12-line report.

### `atk/rofftext` (9 nominal, 8 real; `num.c` excluded)

Confirmed `fossil finfo src/atk/rofftext/num.c` returns "no history
for file". `.ch` count 4 as expected. All 8 real files converted
clean on the first pass (both pre- and post-`-pe`, zero compile
failures either time):

| File | methods | classprocs | helpers |
|---|---|---|---|
| mantext.c | 4 | 1 | 0 |
| mmtext.c | 4 | 1 | 0 |
| roffchrs.c | 0 | 0 | 1 |
| roffcmds.c | 0 | 0 | 51 |
| roffstyl.c | 0 | 0 | 6 |
| rofftext.c | 5 | 4 | 35 |
| rofftxta.c | 3 | 1 | 1 |
| roffutil.c | 0 | 0 | 10 |

**Two parser bailouts** (`gettblopts`, `gettblfmt` in `roffcmds.c`,
"unparseable K&R declarations"): both take a 2-D array parameter
(`char argv[80][80]`) — a legitimate `ansify` parser limitation for
multi-dimensional array parameters, not a bug. Hand-converted both to
ANSI directly from the unambiguous K&R declaration block.

`make depend` confirmed load-bearing and run before `install` both
gate cycles — `num.c`/`num.h` verified present after `depend`, absent
after `clean`, both times.

### `atk/raster/cmd` (8 files)

`.ch` count 4 (`raster`, `rasterv`, `rastimg`, `rastoolv`) as
expected. 4 of 8 succeeded pre-`-pe`; 4 failed with the standard
transient pattern (`raster.c`, `rastervt.c`, `rastoolv.c`,
`rastvaux.c`).

| File | methods | classprocs | helpers |
|---|---|---|---|
| dispbox.c | 0 | 0 | 11 |
| raster.c | 11 | 3 | 4 |
| rasterv.c | 6 | 3 | 32 |
| rastervt.c | — (left K&R, see below) | | |
| rastimg.c | 5 | 3 | 1 |
| rastoolv.c | 6 | 3 | 46 |
| rastvaux.c | 18 | 0 | 23 |
| rastvauy.c | 10 | 0 | 8 |

**Confirmed pre-existing, not M3 fallout:** `rastervt.c` (the
`rasterviewtest` test program, wired only into the Imakefile's `test::`
target via `ClassTestProgramTarget`, never `install::`/`all::` —
confirmed by reading `andrew.rls`'s macro definition) has always failed
to compile, even in its pristine committed K&R form (`fossil revert` +
direct `make rastervt.o` → exit 2, 2 real errors, before I touched
anything). Root cause: `#define class_StaticEntriesOnly` before
`#include <environ.ih>` suppresses `environ_AndrewDir`'s macro
definition, but the file's own code calls it directly, contradicting
its own comment ("not utilized by this .c file itself"). This is
`ansify`'s own compile gate reaching a file the real build doesn't
build at all — exactly the documented O1 precedent. Left K&R,
untouched, per that policy; the subtree-local gate never builds it
either way (confirmed: `rasterviewtest` absent from both `make -k
install` runs' output).

**Real bugs, generic-param-plus-cast pattern (same species as
`figv.c`/`fontselv.c` above):**
- `raster.c`'s `ObservedChanged(struct raster *self, struct
  observable *pix, long status)` body called
  `rasterimage_GetResized(pix)`/`GetWidth`/`GetHeight`/`GetChanged`
  directly on the generic parameter, even though `raster.ch` declares
  it with the covariant `struct rasterimage *pix` (again, `-pe`
  exports the base type regardless). Added a local `struct
  rasterimage *ripix` cast, retargeted 4 call sites.
- `rastvaux.c`'s `SetDataObject(struct rasterview *self, struct
  dataobject *ras)` body accessed `ras->subraster` directly. Added an
  inline `((struct raster *) ras)->subraster` cast at the one call
  site that needed it (a second, unrelated occurrence of the same
  text elsewhere in the file already had a correctly-typed local
  `ras`, left untouched).

**Checklist-item-8 fixes:** `rastoolv.c`'s comma-list of 7 forward
decls (5 with narrow `short accnum`); `rastvaux.c`'s non-static
`DrawPanHighlight(struct rasterview *self, short g)` — checked
whether this cross-file, non-static function's other declaring site
(`rasterv.c:137`, its own separate `extern void DrawPanHighlight();`)
would also break; force-recompiled `rasterv.o` directly (real check,
not stale) and confirmed clean — a same-translation-unit empty-parens
extern declaration doesn't trigger a compile-time conflict just
because a *different* file's real definition gained a narrow
parameter (that only matters within one TU).

## 4. Checklist items 4 and 6, malloc/stdlib blind spot

**Item 4** (installed-header grep for converted non-static helpers):
ran per directory; every hit was either the directory's own installed
`.ih` or a substring false-positive (`proctable__` matching
`table__`, `sunraster__` matching `raster__`). No genuine
cross-directory header touch found anywhere in this batch.

**Item 6** (concurrent-commit check): not applicable — no other
fossil activity happened during this session (solo background run,
`fossil status` before I started showed a clean tree at the parent
checkin).

**Anchored `malloc`/`free`/`realloc`/`calloc` grep**: run per
directory, cross-checked against `#include <stdlib.h>` presence
(clang treats these as builtins, so a missing declaration is
invisible to any `-Werror` census — the known blind spot). Found and
fixed:
- `atk/eq`: all 4 files with real `.c` malloc/free calls
  (`eq.c`, `eqv.c`, `eqvcmds.c`, `draweqv.c`) were missing
  `<stdlib.h>` — added to all 4.
- `atk/figure`: 12 of 17 files missing `<stdlib.h>` — added to all
  12 (`figattr.c`, `figio.c`, `figobj.c`, `figtoolv.c`, `figospli.c`,
  `fontsel.c`, `figoplin.c`, `fontsamp.c`, `figure.c`, `fontselv.c`,
  `figv.c`, `figotext.c`).
- `atk/chart`: 6 files missing `<stdlib.h>` — added to all 6
  (`chartdot.c`, `chart.c`, `chartpie.c`, `charthst.c`, `chartobj.c`,
  `chartv.c`).
- `atk/table`: already present in all 6 malloc-using files — no
  action.
- `atk/rofftext`: already present in all 6 malloc-using files — no
  action.
- `atk/raster/cmd`: 2 files missing `<stdlib.h>` — added to both
  (`rastimg.c`, `rastoolv.c`).

## 5. Gate results

Every directory: `make -C <path> clean`, `make -C <path> depend`,
`make -C <path> -k install`, each as separate calls, twice. Zero
`error:` lines both cycles, every directory:

| Directory | Gate 1 | Gate 2 |
|---|---|---|
| `atk/eq` | clean | clean |
| `atk/figure` | clean (after 2 more staleness-trap fixes found via real `-k` build) | clean |
| `atk/chart` | clean | clean |
| `atk/table` | clean | clean |
| `atk/rofftext` | clean (`depend` confirmed regenerating `num.c`/`num.h`) | clean |
| `atk/raster/cmd` | clean (`rasterviewtest` correctly absent from output both times) | clean |

**Wave-end tree-wide gate**: `make -C /Users/wdc/src/AUIS/andrew-6.4/src
dependInstall`, twice (note: the target lives in `src/Makefile`, not a
tree-root Makefile — `make -C <tree-root> dependInstall` itself fails
with "No rule to make target", corrected to `make -C
<tree-root>/src dependInstall`). Both runs: exit code differed (2 vs
0) but **zero `error:` lines differ from the known baseline** — both
runs show exactly the same 2 pre-existing `contrib/zip/utility/ltapp.c`
`error:` lines (`ltapp.c:115:27`, `ltapp.c:123:32`, both
`incompatible integer to pointer conversion` on `boolean`→`void *`),
matching the Wave-1/2 retroactive baseline documented in the runbook
exactly. Confirmed all 6 batch directories appear as
`building (dependInstall) (.../src/atk/...)` in the log. No new
tree-wide error.

## 6. `fossil status`/`fossil extras`

`fossil status`: 72 `EDITED` files (6 Imakefiles, 6 `.ch` files, 60
`.c` files) across the 6 directories — `eq` 7, `figure` 21, `chart`
16, `table` 11, `rofftext` 9, `raster/cmd` 8 — no `ADDED`/`DELETED`,
full list is in the session diff. No commit made.

`fossil extras`: only expected untracked build residue (`Makefile`,
`.o`, `.eh`, `.ih`, `install.time`, `install.doc`, `,help.alias`, and
the two directories' generated-source pairs `eq/eqparse.{c,h}`,
`rofftext/num.{c,h,tab.gra}`/`tmac.m`). One pre-existing untracked
file noticed but **not created by this session** and not touched:
`src/atk/figure/fontselv.c.orig`, dated July 24 (a week before this
session started) — flagging its existence for the orchestrator, not
cleaning it up myself since it predates this batch.

## 7. Anything that surprised you / didn't match expectations

- The include-order enum-scoping bug in `atk/figure` (§3, "New
  finding") is the most significant surprise — it's a genuinely new
  fallout *shape* for the M3 taxonomy (not `ansify` DRIFT, not a
  parser bailout, not a stranded forward declaration — a C-standard
  prototype-scope quirk that only manifests once `-pe` emits typed
  enum parameters), affected 7 of `atk/figure`'s 17 files, and is
  exactly the kind of thing the "give `atk/figure` more scrutiny"
  instruction was hedging against, just not the specific three Pilot
  B patterns it named.
- The individually-stale-`.o` methodological trap (Pilot A finding #3)
  bit me for real mid-batch (`figorect.c`/`figoins.c` in
  `atk/figure`) — worth restating in the runbook as a hard rule for
  future batches, not just a caution: an untouched file's `make X.o`
  "success" mid-session proves nothing; only a post-`clean` gate does.
- `atk/chart`'s missing M2 `COMPILERFLAGS` override (§3) was
  unexpected — no batch doc flagged it, and it means this directory
  has run without an implicit-declaration net this whole time. Out of
  scope to fix here, flagging for whoever next touches `atk/chart`.
- The `chartapp.ch` classpp `-D` dump bug (blank class tag for a
  self-referential parameter type) and the `chart.ch` `Apply`
  cast-style-syntax `ansify` rendering bug are both new, bounded,
  single-instance tool-adjacent findings — neither warranted a tool
  fix (per Delegation: tool construction stays top-level, and both
  are single-occurrence), but both are worth the orchestrator knowing
  about in case the same shapes recur in a later wave (self-referential
  covariant classproc types; cast-style K&R function-pointer
  parameters).
- Everything else matched the mature O1–I1 taxonomy closely: rock
  idiom (Pilot B), generic-param-plus-cast (I1's `srctextv.ch`),
  mistyped-self-to-sibling-class (I1's `cmapv.ch`), stranded
  narrow-param forward declarations (T1/I1, including the
  comma-list variant I1 already knew to expect), dead/unwired
  classprocs (I1's file-absent-from-DOBJS precedent, here at
  function granularity instead), and the `ansify`-gate-reaches-
  non-built-files false trigger (O1).

## 8. Suggested runtime checks for wdc

All 6 directories build dynamically-loaded `.do` insets (confirmed via
`ls build/dlib/atk/{eq,figure,chart,table,rofftext,raster}.do` — all
present) plus standalone binaries in `build/bin/` (`eq`, `figure`,
`chart`, `table`, `rofftext`, `raster`, all symlinked through
`runapp`). Per `rollout-procedure.md`'s Runtime check rules: never
launch these from this session; the orchestrator or wdc should run
them from a real terminal (not VS Code/sandboxed).

- **`eq`**: `runapp -d eq` (or open a test document containing an eq
  inset — insert via `<ESC><TAB>eq` in `ez`). Exercise formula
  editing, save/read round-trip (`eq__Read`/`eq__WriteFILE`, both
  touched this batch), and the eqview scrollbar/keymap-heavy commands
  in `eqvcmds.c` (Save/Print/Cut/Copy/Paste).
- **`figure`**: `runapp -d figure`, or insert a figure inset in `ez`.
  Given the directory's history, specifically exercise: creating and
  reparenting a group object (`figogrp`'s `SetParent`, fixed this
  batch), any tool that uses `Build`/`Reshape`/`AddParts`/`DeleteParts`
  on a rounded-rectangle or plain-rectangle object (the include-order
  enum fix touches all mouse-driven shape tools), and the font
  selector panel (`fontselv.c`'s `ObservedChanged` cast fix — change
  family/size/style and confirm the UI updates).
- **`chart`**: `runapp -d chart`, or `chart` standalone. Exercise
  `Create` with a real specification list (attribute/value pairs —
  the typeless-`.ch` fix), and `Apply` if any chart-tool UI path
  calls it (the cast-syntax fix). Histogram/pie/line/scatter/stacked
  variants all touched (`charthst`/`chartpie`/`chartlin`/`chartstk`/
  `chartdot`/`chartcsn`).
- **`table`**: `runapp -d table`, or a spread/table inset in `ez`.
  Exercise keyboard navigation (all 14 `k_*` handlers in `keyboard.c`
  retyped this batch — arrows, tab, backspace, home/end, top/bottom),
  and printing a table with a formula value (`print.c`'s `printVal`).
- **`rofftext`**: a document using a roff/man/mm-formatted inset, or
  `rofftext` standalone against a real troff-style source file.
  Exercise the `.TS`/table-format commands specifically (`gettblopts`/
  `gettblfmt` in `roffcmds.c`, the 2-D-array parser bailout).
- **`raster`**: `runapp -d raster`, or a raster/bitmap inset in `ez`.
  Exercise: loading/replacing an image (`ObservedChanged`'s cast fix
  in `raster.c`), the raster tool palette (`rastoolv.c`'s narrow-param
  command handlers — paint/paste/text/line/circle/etc.), and
  drag-panning/display-box interaction (`rastvaux.c`'s
  `SetDataObject`/`DrawPanHighlight` fixes). Do **not** try to build
  or run `rasterviewtest` — confirmed pre-existing broken, out of
  scope, not part of the real install target.

No saves against unversioned fixtures — point any save-path testing
at scratch copies, not tree-controlled test documents.
