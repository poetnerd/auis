# M2 rollout point 3, batch A: `atk/basics/x`, `atk/basics/common`, `atk/figure`, `atk/syntax/tlex`, `atk/raster/cmd`

## 1. Status

**RESOLVED, back to the gate. A regression was reported against
`atk/figure` after wdc's runtime checks (§9 below is the original
"all clear" summary, superseded then re-confirmed — see §10 for the
investigation and §11 for the resolution).** Root-caused as
**pre-existing, not caused by this batch**: a controlled
revert/rebuild/fresh-restart test (§10.3, §11) proved the
text-rendering corruption is present even with `atk/figure` fully
reverted to committed, pre-M2 code. **This is a distinct, not-yet-
root-caused text-rendering bug in the figure/figotext family** — §10.2
originally floated `porting-assessment.md` §17's Xft incremental-
redraw bug as a plausible explanation, but §17's "still open" text
turned out to be stale (`contrib/calc`'s own instance of that bug is
now fully fixed and confirmed working live), so no shared root cause
with calc is asserted here; treat figure's corruption as its own open
question needing separate investigation later. The `atk/figure` fix
has been **reapplied** (§11) and the full tree-wide gate re-run (§11)
to get back to a fully verified-green state across all 5 directories.
`atk/figure`'s own rendering bug stays **open, out of scope for
M2, not investigated further, not fixed here.**

No commits made, at any point. Working tree: 34 files edited (5
`Imakefile`s + 29 `.c`/`.h` files) across all 5 directories, matching
the original batch state exactly (byte-identical `fossil diff`,
confirmed in §11). Session diff at tree root: `m2-batch3a-session.diff`
(772 lines — current again after the revert/reapply round-trip). A
second file, `m2-batch3a-post-figure-revert.diff`, is a leftover
intermediate snapshot from the revert window (§10) — stale, kept only
as an artifact of the investigation trail, superseded by the reapply.
Full
gate log at the canonical path `~/src/AUIS/andrew-6.4/dependInstall.log`
(also stale/overwritten several times since by the investigation in
§10 — the tree-wide gate result reported in §5 is still valid, just not
re-derivable from the log file's *current* contents).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `m2-rollout-runbook.md`, `m2-batch3a-prompt.md`, and the two prior
   rollout points' reports (`m2-pilot-eq-REPORT.md`,
   `m2-batch2-REPORT.md`) in full.
2. Confirmed none of the 5 directories use `Parser()`/`LexFile`
   (grepped each Imakefile) — including `atk/syntax/tlex`, which
   sounds lexer-adjacent by name but has no generated-source gap for
   its `install` target (it does invoke `../bison/bison` for two
   `test::`-only targets, `testtlex`/`testness`, which are not part of
   `install` and were not touched).
3. For each directory: flagged its `Imakefile` with the runbook's
   exact four-flag `COMPILERFLAGS` override, verified via `make
   Makefile` + `grep -n COMPILERFLAGS Makefile` (override line always
   appears after `system.mcr`'s default, confirmed for all 5), then
   `make clean && make depend && make -k install` with
   `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the first pass of each
   directory (see §6 — `atk/basics/x`'s `xim.c` has more than clang's
   default 20-error cap worth of instances, which silently hid one
   real instance on the default-limit pilot/batch2 recipe; raising the
   cap up front avoided repeating that near-miss), fixed fallout, then
   rebuilt clean twice more with the plain recipe to confirm
   determinism (three-plus total clean builds per directory).
4. Ran the full tree-wide gate: `cd src && make Clean` (exit 0), then
   backgrounded `make dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1`,
   confirmed completion via `pgrep -f 'make dependInstall'` returning
   empty (independently re-verified, not just taken on the
   coordinator's word — see §5).
5. After the gate, noticed one over-cautious edit (an unused
   `extern int errprintf();` added to `menubar.c`, which never
   actually calls `errprintf`) and removed it, re-verifying
   `atk/basics/x` alone rebuilds clean twice more (§7 explains why the
   full tree-wide gate was not re-run for this one subtractive,
   dead-code-only cleanup).
6. `fossil status` (below) confirms exactly the 34 files touched. No
   commit made. `fossil diff > m2-batch3a-session.diff` written to the
   tree root (regenerated after the §5 cleanup; file list unchanged).

## 3. Per-directory findings

### `src/atk/basics/x` — 26 real instances (stale estimate: 25)

The one discrepancy this batch, and it's a methodology artifact, not a
real surprise: clang's default `-ferror-limit=20` truncated `xim.c`'s
diagnostics on the first pass (19 shown, 1 more — `mb_SetGetDefault` at
line 3742 — hidden behind the cap until the first 19 were fixed and
the file was rebuilt). Total real count once fully surfaced: 26.

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1–6 | `xim.c:702,708,1193,1199,1334,1342,3215` | `errprintf` | Missing in-tree/project header — none declares it anywhere (only precedent found tree-wide: `ams.h:170`'s `extern int errprintf();`) |
| 7 | `xim.c:1912` | `mb_SetKeys` | In-directory header (`menubar.h`) declares siblings from the same file, not this one — extended |
| 8,10,12,15 | `xim.c:2026,2177,2216,2429` | `mb_Destroy` | Same as #7 |
| 9,11,13 | `xim.c:2030,2181,2220` | `cmenu_Destroy` | Header exists (`overhead/cmenu/cmenu.h`, outside the flagged directory) but its typed prototypes are gated `#ifdef _STDC_` — never defined anywhere in the tree (the standard macro is `__STDC__`), so the `#else` branch (which omits this function) is always taken — effectively "no header reaches it"; local extern |
| 14,17 | `xim.c:2231,2824` | `mb_RefitMenubar` | Same as #7 |
| 16 | `xim.c:2438` | `mb_Finalize` | Same as #7 |
| 18 | `xim.c:3212` | `cmenu_Activate` | Same as #9 (`_STDC_` bug) |
| 19 | `xim.c:3742` | `mb_SetGetDefault` | Same as #7 (hidden behind the error-limit cap on the first pass, see above) |
| 20 | `menubar.c:122` | `atoi` | Missing standard-library header |
| 21 | `menubar.c:197` | `free` | Missing standard-library header |
| 22 | `menubar.c:257` | `malloc` | Missing standard-library header |
| 23 | `menubar.c:484` | `realloc` | Missing standard-library header |
| 24 | `menubar.c:574` | `qsort` | Missing standard-library header |
| 25 | `menubar.c` | (see below) | not a real instance — see note |
| 26 | `atkatoms.c:52` | `xim_SetupAtoms` | In-tree header exists (`atk/basics/x/atoms.h`), declares it, just wasn't included in `atkatoms.c` |

Fix: `<stdlib.h>` in `menubar.c`. Four new `mb_SetKeys`/`mb_RefitMenubar`/
`mb_Finalize`/`mb_SetGetDefault` declarations added to `menubar.h`
(in-directory header, already declaring 12 other siblings from the
same file in the same style — `extern void mb_Foo();` /
`extern GetDefaultsFunction mb_SetGetDefault();`, both the `__STDC__`
and non-`__STDC__` branches). Two `extern int cmenu_Destroy();` /
`extern int cmenu_Activate();` added locally in `xim.c` (the declaring
header is outside the flagged directory and unreachable due to its own
bug — not edited, per the M2 hard-stop). One `#include "atoms.h"`
added to `atkatoms.c`.

**Note on item 25**: I initially also added an unused
`extern int errprintf();` to `menubar.c` on the assumption it would
need the same treatment as `xim.c` — it doesn't; `menubar.c` never
calls `errprintf` despite including `<errprntf.h>`. Caught this after
the gate had already passed with the harmless extra declaration in
place, removed it, and re-verified `atk/basics/x` alone rebuilds clean
twice more (§7). Real count is 26 call-site instances, not 27 files-with-a-symptom.

### `src/atk/basics/common` — 23 (matches stale estimate of 23 exactly)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `dataobj.c:200` | `strlen` | Missing standard-library header |
| 2 | `dataobj.c:205` | `strcpy` | Missing standard-library header |
| 3 | `environ.c:64` | `strcpy` | Missing standard-library header |
| 4 | `environ.c:123` | `strlen` | Missing standard-library header |
| 5 | `environ.c:167` | `getprofileswitch` | Header exists (`overhead/util/hdrs/util.h`) and declares it, but `environ.c` has its own long-standing local-extern block (`extern char *AndrewDir(); extern char *LocalDir(); ...`) reaching several *other* `util.h`-declared functions without including the header — matched that habit instead |
| 6 | `environ.c:176` | `getprofileint` | Same as #5 |
| 7 | `environ.c:184` | `profileentryexists` | No header anywhere declares it (only the `FOLDEDEQ`-style situation: `util.h` doesn't mention it at all) — local extern, same block as #5/#6 |
| 8 | `environ.c:228` | `FreeConfigureList` | Same as #5 (declared in `util.h`, reached via the file's local-extern habit) |
| 9,10 | `event.c:172,242` | `osi_GetTimes` | No header anywhere declares it (confirmed again this batch — third time, see §4) |
| 11 | `proctbl.c:232` | `FoldedEQ` | No header anywhere declares it (confirmed again — fourth time; `proctbl.c` already includes `util.h`, which has the `FOLDEDEQ` *macro* calling this function internally but never prototypes it) |
| 12,15 | `image.c:419`, (also feeds osi_GetTimes) | `osi_GetTimes` | Same as #9 |
| 13 | `image.c:422` | `getaddr` | Header exists (`util.h:311`) and declares it, just wasn't included — added `#include <util.h>` (matches `atk/basics/common/path.c` and `proctbl.c`'s own existing "just include util.h" habit, a second, coexisting local convention in this same directory alongside environ.c's local-extern style) |
| 14,17 | `image.c:510,2434` (to64) and `image.c:690,2468` (from64) | `to64`/`from64` | No header anywhere declares them (`overhead/util/lib/encode.c`) |
| 16 | `image.c:976` | `pow` | Missing standard-library header (`math.h`) |
| 18 | `image.c:2470` | `fromqp` | Same as #14/#17 |
| 19 | `jpeg.c:310` | `LoadJFIF` | Same-file forward reference (defined at line 314, used at 310) — matched the file's existing `static void jselwxv(); static int writeJFIF();` forward-declare convention |
| 20 | `im.c:788` | `fchmod` | Missing standard-library header (`<sys/stat.h>` on Darwin, confirmed via the SDK header — not `<unistd.h>`) |
| 21,22 | `graphic.c:842,844` | `strlen`/`strcpy` | Missing standard-library header |

Fix summary: `<string.h>` in `dataobj.c`/`environ.c`/`graphic.c`;
`<math.h>` + `<util.h>` in `image.c`; `<sys/stat.h>` in `im.c`; local
externs for `profileentryexists`/`osi_GetTimes`/`FoldedEQ`/`to64`/
`from64`/`fromqp` (all "no header anywhere" instances, matching each
file's own existing forward-declare style where one already existed —
`environ.c` and `image.c` both already had a top-of-file `extern`
block, `event.c` and `proctbl.c` did not, so those two got a small new
one next to the file's other declarations); `getprofileswitch`/
`getprofileint`/`FreeConfigureList` added to `environ.c`'s existing
local-extern block rather than including `util.h` fresh, matching that
file's own established habit.

**Former M1 rollout point 9 directory, checked as the prompt asked**:
`atk/basics/common` was M1's largest-blast-radius directory (41
classes, 2,351 external `.ih` includes). No sign of that history
showing up here — every one of the 23 M2 instances is a plain
compile-unit-local fix (missing standard header, or a project function
with no reachable declaration), none of it touches typed `.ih`
signatures or anything M1 changed. Confirms the runbook's "Gate scope"
prediction that M2's fallout doesn't cross directories the way M1's
did, even for M1's own biggest directory.

### `src/atk/figure` — 23 (matches stale estimate of 23 exactly)

Purest directory in the batch — single taxonomy category throughout,
zero surprises.

| File | Instances | Missing declarations |
|---|---|---|
| `figure.c` | 2 | `strcmp`, `strncmp` |
| `figobj.c` | 4 | `strlen`, `strcpy`, `strncmp`, `strcmp` |
| `figoplin.c` | 1 | `strlen` |
| `figoins.c` | 2 | `strlen`, `strncmp` |
| `figv.c` | 3 | `strcmp`, `strlen`, `strcpy` |
| `figtoolv.c` | 3 | `strlen`, `strcpy`, `strcmp` |
| `fontsel.c` | 3 | `strcmp`, `strlen`, `strcpy` |
| `fontselv.c` | 3 | `strlen`, `strcpy`, `strcmp` |
| `fontsamp.c` | 2 | `strlen`, `strcpy` |

Fix: `#include <string.h>` added to all 9 files (none had it). All
Missing standard-library header, category 1.

### `src/atk/syntax/tlex` — 28 real instances (matches stale estimate
of 28 once the known false positive is excluded — the grep-based `29`
raw count included one hit of the pre-existing `Internal error:
unknown recognizer type` string embedded in `defaults.c`'s own
`ErrorA(FATAL, "Internal error: unknown recognizer type", buff);` call
— *this* file is where that false positive's source text actually
lives)

| File | Instances | Missing declarations |
|---|---|---|
| `tlex.c` | 1 | `strlen` |
| `gentlex.c` | 8 | `strlen`, `malloc`, `strncpy`, `exit`, `strcmp`, `strrchr`, `strcpy`, `strcat` |
| `charset.c` | 1 | `malloc` |
| `defaults.c` | 4 | `sprintf`, `exit`, `strcmp`, `malloc` |
| `readtabc.c` | 7 | `strncpy`, `strcmp`, `strlen`, `malloc`, `exit`, `atoi`, `realloc` |
| `readtlx.c` | 5 | `strncmp`, `strlen`, `malloc`, `memmove`, `strncpy` |
| `thongs.c` | 2 | `malloc`, `strcmp` |

Fix: `<string.h>` in `tlex.c`; `<stdlib.h>` + `<string.h>` in
`gentlex.c`/`defaults.c`/`readtabc.c`/`readtlx.c`/`thongs.c`;
`<stdlib.h>` only in `charset.c` (didn't need `string.h`). All Missing
standard-library header, category 1 — 100% of this directory's fallout
is the same single category, like `atk/figure`.

**Confirms the runbook's own caution**: this directory's name sounds
lexer-adjacent, and it does shell out to `../bison/bison` for its
`test::`-only targets (`ness.tab.c`, consumed only by `testtlex`/
`testness`) — but `install`'s own dependency graph never touches
generated sources, so no `depend`-before-`install` gap applied here
(confirmed by checking, not assuming, per the prompt's explicit
instruction).

### `src/atk/raster/cmd` — 37 (matches stale estimate of 37 exactly)

Most structurally interesting directory this batch — the first batch-3a
directory with real same-directory forward-reference fallout beyond
plain standard-library misses (matches `atk/eq`'s and batch 2's
`overhead/cmenu`/`overhead/fonts/cmd` shape, just larger).

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1,2,4,5 | `raster.c:371,625,821,853` | `getpid` | Missing standard-library header |
| 3 | `raster.c:625` | `strcmp` | Missing standard-library header |
| 6,7 | `raster.c:689,690` | `strncmp`, `strlen` | Missing standard-library header |
| 8 | `raster.c:830` | `to64` | No header anywhere declares it (`overhead/util/lib/encode.c` — same finding as `atk/basics/common/image.c` above, cross-directory recurrence) |
| 9,12 | `raster.c:832,873` | `unlink` | Missing standard-library header |
| 10 | `raster.c:857` | `from64` | Same as #8 |
| 11 | `raster.c:859` | `fromqp` | Same as #8 |
| 13,14 | `rastimg.c:111` (×2) | `strcpy`, `strlen` | Missing standard-library header |
| 15,17 | `rasterv.c:420,491` | `DisplayBoxBlitOverlap` | In-directory header (`dispbox.h`) declares 7 other siblings from `dispbox.c` in the same file, not this one — extended |
| 16 | `rasterv.c:428` | `DisplayBoxHide` | Same as #15 |
| 18 | `rasterv.c:914` | `os_system` | No header anywhere declares it (`overhead/util/lib/system.c`) |
| 19–25 | `rastvaux.c:162,170,172,178,184,190,196` | `CenterCommand`/`ZoomInCommand`/`ZoomOutCommand`/`ToolCommand`/`PanCommand`/`RegionSelectCommand`/`TouchUpCommand` | Same-directory forward reference (all 7 defined in `rasterv.c`, no header) |
| 26 | `rastvaux.c:300` | `ReflectChangesInExpansion` | Same-directory forward reference (defined in `rasterv.c`) |
| 27 | `rastvaux.c:786` | `FinishMovingDisplayBox` | Same as #19–25 |
| 28 | `rastvaux.c:799` | `MoveDisplayBoxCommand` | Same as #19–25 |
| 29 | `rastvaux.c:1369` | `DisplayBoxHideOverlappingHighlight` | Same as #15 (`dispbox.h`) |
| 30–32 | `rastvaux.c:1486,1494,1537` | `DrawHighlightBehindDisplayBox` | Same as #15 |
| 33 | `rastvaux.c:1835` | `SetPixelBehindDisplayBox` | Same as #15 |
| 34 | `rastvaux.c:2060` | `DisplayBoxBlitOverlap` | Same as #15 |
| 35,36 | `rastvauy.c:86,133` | `SetPixelBehindDisplayBox` | Same as #15 |
| 37 | `dispbox.c:301` | `ReflectChangesInExpansion` | Same as #26 (defined in `rasterv.c`, called from `dispbox.c` this time) |

Fix: `<string.h>` + `<unistd.h>` in `raster.c`; `<string.h>` in
`rastimg.c`; local `extern int to64(), from64(), fromqp();` in
`raster.c`. `dispbox.h` (in-directory, already declares 7 other
`dispbox.c` functions in a distinctive `void Foo(/* args */);` style)
extended with the 5 missing siblings
(`DisplayBoxBlitOverlap`/`DisplayBoxHide`/
`DisplayBoxHideOverlappingHighlight`/`DrawHighlightBehindDisplayBox`/
`SetPixelBehindDisplayBox`) in the same style — all four consuming
files (`rasterv.c`, `rastvaux.c`, `rastvauy.c`, `dispbox.c` itself)
already `#include <dispbox.h>`, so this one header edit closed 11 of
the 37 instances at once. `rasterv.c`-defined command functions with
no header at all (`CenterCommand` through `TouchUpCommand`,
`ReflectChangesInExpansion`, `FinishMovingDisplayBox`,
`MoveDisplayBoxCommand`) got local `extern void Foo();` declarations
in `rastvaux.c`, extending that file's own **pre-existing** local-extern
block (`extern void PostMenus(); extern void RotateCommand();` was
already there before this session) — direct, unprompted confirmation
of the "match the flagged directory's own local-extern habit" rule.
`dispbox.c`'s single `ReflectChangesInExpansion` call got its own
one-line local extern (no pre-existing block in that file to extend).
`os_system` (no header anywhere, cross-directory, `overhead/util/lib`)
got a one-line local extern in `rasterv.c`.

**Correction to the prompt's own text**: the prompt says "`atk/
raster/cmd` builds `convertraster` and friends" — checked, and this is
not accurate. `convertraster` is built from the sibling directory
`atk/raster/convert` (`src/atk/raster/convert/Imakefile`), consuming
only `atk/raster/lib`, not anything in `atk/raster/cmd`. A pre-existing
test battery even documents this exact distinction in its own header
comment (see §8 runtime-check section). `atk/raster/cmd` itself builds
only dynamically-loaded `.do`s (`raster`, `rastimg`, `rasterv`,
`rastoolv`) — no standalone CLI of its own.

## 4. Taxonomy findings / corrections

- **No new category.** All fallout across all 5 directories fit the
  three already-established categories (missing standard header;
  missing in-tree/project header with its 3 sub-cases; same-file/
  same-directory forward reference). "Possible genuine bug/typo"
  remains empty — every function name that looked unfamiliar resolved
  to a real, correctly-spelled definition somewhere in the tree once
  grepped.
- **New recurring pattern, not a new category**: `osi_GetTimes` (now
  4 sites: pilot's sibling batch found it in `atk/frame`; this batch
  found it 3 more times in `atk/basics/common`) and `to64`/`from64`/
  `fromqp` (now found in both `atk/basics/common/image.c` and `atk/
  raster/cmd/raster.c` — same functions, same "no header anywhere"
  finding, two unrelated consumer directories) are turning into a
  small, recognizable family of `overhead/util/lib` functions that
  have never had a declaring header, cross-directory. Worth a
  standing note for whoever eventually flags `overhead/util/lib`
  itself: these functions could get real prototypes in `util.h` at
  that point, retiring several M2-era local externs across multiple
  already-flagged directories at once. Not done here — out of scope
  for a consuming directory's M2 pass, per the existing hard-stop
  rule.
- **New nuance on sub-case 2 (header exists, declares siblings, not
  this one)**: batch 2's two examples (`cmdraw.h`, `index.h`) were
  both resolved by matching an existing local-extern habit rather than
  extending the header, and the taxonomy text (as revised after batch
  2) reads as "don't extend the header, match local precedent"
  fairly generally. This batch had two more instances
  (`atk/basics/x/menubar.h`, `atk/raster/cmd/dispbox.h`) where I
  extended the header directly instead, because in both cases the
  header lives *in* the flagged directory, is small and clearly
  dedicated to exactly the one file's public API (not a
  many-callers utility header like `util.h`/`index.h`), and there was
  no competing local-extern habit already established in the
  *specific* consuming file for *that specific* function family (only
  for unrelated ones — `rastvaux.c`'s pre-existing block was for
  `PostMenus`/`RotateCommand`, not for any `DisplayBox*` function).
  Recommend the taxonomy note be read as "check both the header's
  scope (dedicated single-file API vs. broad utility) and the
  specific consuming file's specific local-extern precedent for *this*
  function family before choosing" rather than a blanket "always
  local-extern, never extend" rule — this batch found real instances
  on both sides of that distinction.
- **First identified header bug in this M2 pass, not just an
  incomplete header**: `overhead/cmenu/cmenu.h` guards its typed
  `cmenu_Destroy`/`cmenu_Activate` (and 3 others) prototypes behind
  `#ifdef _STDC_` — not the standard `__STDC__` macro, which is what
  every ANSI-conforming compiler actually predefines. `_STDC_` is
  defined nowhere in the tree (`grep -rn "\-D_STDC_" config/` — no
  hits), so the guarded branch is dead on every real build; the
  `#else` branch is what actually compiles, and it's missing
  `cmenu_Destroy`/`cmenu_Activate` (present) vs. the 3 it does
  redeclare. This is the same *symptom* as the "incomplete header"
  sub-case (some siblings declared, not others) but a different root
  *cause* — worth distinguishing if `overhead/cmenu` itself gets a
  housekeeping pass, since the fix there would be `s/_STDC_/__STDC__/`
  in the header, not adding more functions to the `#else` branch.

## 5. Subtree-local gate vs. tree-wide gate

Third data point for `m2-rollout-runbook.md`'s "Gate scope" question,
independently re-verified (not just taken on the coordinator's
report):

```
$ grep -c "error:" dependInstall.log
4
$ grep -n "error:" dependInstall.log
109981:  200 |                 ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
173355:clang: error: linker command failed with exit code 1 (use -v to see invocation)
223717:ltapp.c:115:27: error: incompatible integer to pointer conversion ...
223729:ltapp.c:123:32: error: incompatible integer to pointer conversion ...
```

All 4 are the known pre-existing baseline (recognizer-type false
positive — this time its source text is literally inside one of this
batch's own files, `atk/syntax/tlex/defaults.c`, see §3 — the
`ams/msclients/nns` SSLLIB link failure, and `contrib/zip/utility/
ltapp.c`'s two int-conversion errors), identical set to the pilot's
and batch 2's baseline. Confirmed all 5 directories actually descended
during the gate:

```
27916:building (dependInstall) (.../src/atk/basics/common)
40065:building (dependInstall) (.../src/atk/basics/x)
99346:building (dependInstall) (.../src/atk/raster/cmd)
109461:building (dependInstall) (.../src/atk/syntax/tlex)
149328:building (dependInstall) (.../src/atk/figure)
```

**The tree-wide gate found nothing beyond what the 5 subtree-local
builds already showed** — same result as the pilot and batch 2, now
with a third data point at roughly double batch 2's volume (137 vs.
70 instances) and including a directory (`atk/basics/x`/`atk/basics/
common`) statically linked into every ATK app via `runapp`, plus the
former M1 rollout-point-9 directory with the largest header-propagation
blast radius in the whole M1 rollout. Recommend this settles the
"subtree-local gate is sufficient for M2" question — three
independent data points now, including the two structurally most
concerning candidates for cross-directory fallout (static linkage,
largest historical blast radius), both showing zero.

## 6. A build-mechanics note, not a fallout category

`atk/basics/x/xim.c` has more implicit-function-declaration errors in
one file than clang's default `-ferror-limit=20`, which silently
truncates further diagnostics once the cap is hit (the file still
fails to compile, but only the first 20 are shown — the M1/M2 `-k`
lesson about undercounting applies at the *per-file* diagnostic level,
not just the per-directory build-stops-at-first-failure level). This
hid one real instance (`mb_SetGetDefault`) until the first 19 were
fixed and the file rebuilt. Worked around by passing
`CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on each directory's first
fix-surfacing pass this batch (not the two follow-up determinism
passes, which use the plain recipe and were already error-free).
Recommend folding into `rollout-procedure.md`'s "Logging" section
alongside the existing `-k` warning: for directories with especially
large single files, consider `-ferror-limit=0` on the first
census/fix pass too.

## 7. Files touched (compile status)

All 34 edited files compile clean (0 errors) in their final state,
confirmed by clean `make clean && make depend && make -k install`
passes per directory (at least three per directory: the fix-surfacing
pass plus two more back-to-back clean passes; `atk/basics/x` got two
additional clean passes after the §1.5/§3 `menubar.c` cleanup, for a
total of five) plus the tree-wide gate.

- `src/atk/basics/x/Imakefile`, `atkatoms.c`, `menubar.c`, `menubar.h`, `xim.c`
- `src/atk/basics/common/Imakefile`, `dataobj.c`, `environ.c`, `event.c`, `graphic.c`, `im.c`, `image.c`, `jpeg.c`, `proctbl.c`
- `src/atk/figure/Imakefile`, `figobj.c`, `figoins.c`, `figoplin.c`, `figtoolv.c`, `figure.c`, `figv.c`, `fontsamp.c`, `fontsel.c`, `fontselv.c`
- `src/atk/syntax/tlex/Imakefile`, `charset.c`, `defaults.c`, `gentlex.c`, `readtabc.c`, `readtlx.c`, `thongs.c`, `tlex.c`
- `src/atk/raster/cmd/Imakefile`, `dispbox.c`, `dispbox.h`, `raster.c`, `rasterv.c`, `rastimg.c`, `rastvaux.c`

Regenerated `Makefile`s in all 5 directories are not fossil-tracked,
not source edits.

**On not re-running the full tree-wide gate after the post-gate
`menubar.c` cleanup (§1 step 5)**: the only change was *removing* an
unused, never-called `extern int errprintf();` — a strictly
subtractive edit to a file that already compiled with 0 errors both
before and after (verified by two more clean subtree-local rebuilds of
`atk/basics/x` alone, see §1). An unused declaration cannot have
contributed to the gate's green result, and removing it cannot
introduce a new error in this file or any other — there is no
mechanism by which deleting an unreferenced `extern` changes any
`.o`'s symbol table or ABI. Judged the risk of re-running a ~15+ minute
full gate for a change with zero plausible failure mode not worth the
time; flagging this reasoning explicitly so wdc can override it if he
wants the extra certainty regardless.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     00f76e973e61bab4610155a4d9dc6305fc33f962 2026-07-24 05:09:33 UTC
parent:       789544e5ba48a35ec062935ae5812ebc517dc8bf 2026-07-24 05:09:28 UTC
tags:         andrew-6.4
comment:      add m2-batch2-REPORT.md: M2 rollout point 2 (8-directory batch) session report (user: wdc)
EDITED     src/atk/basics/common/Imakefile
EDITED     src/atk/basics/common/dataobj.c
EDITED     src/atk/basics/common/environ.c
EDITED     src/atk/basics/common/event.c
EDITED     src/atk/basics/common/graphic.c
EDITED     src/atk/basics/common/im.c
EDITED     src/atk/basics/common/image.c
EDITED     src/atk/basics/common/jpeg.c
EDITED     src/atk/basics/common/proctbl.c
EDITED     src/atk/basics/x/Imakefile
EDITED     src/atk/basics/x/atkatoms.c
EDITED     src/atk/basics/x/menubar.c
EDITED     src/atk/basics/x/menubar.h
EDITED     src/atk/basics/x/xim.c
EDITED     src/atk/figure/Imakefile
EDITED     src/atk/figure/figobj.c
EDITED     src/atk/figure/figoins.c
EDITED     src/atk/figure/figoplin.c
EDITED     src/atk/figure/figtoolv.c
EDITED     src/atk/figure/figure.c
EDITED     src/atk/figure/figv.c
EDITED     src/atk/figure/fontsamp.c
EDITED     src/atk/figure/fontsel.c
EDITED     src/atk/figure/fontselv.c
EDITED     src/atk/raster/cmd/Imakefile
EDITED     src/atk/raster/cmd/dispbox.c
EDITED     src/atk/raster/cmd/dispbox.h
EDITED     src/atk/raster/cmd/raster.c
EDITED     src/atk/raster/cmd/rasterv.c
EDITED     src/atk/raster/cmd/rastimg.c
EDITED     src/atk/raster/cmd/rastvaux.c
EDITED     src/atk/syntax/tlex/Imakefile
EDITED     src/atk/syntax/tlex/charset.c
EDITED     src/atk/syntax/tlex/defaults.c
EDITED     src/atk/syntax/tlex/gentlex.c
EDITED     src/atk/syntax/tlex/readtabc.c
EDITED     src/atk/syntax/tlex/readtlx.c
EDITED     src/atk/syntax/tlex/thongs.c
EDITED     src/atk/syntax/tlex/tlex.c
```

No commit made. `m2-batch3a-session.diff` (772 lines) written to the
tree root.

## 8. Runtime-check commands for wdc

**Load-bearing**: nothing from this batch should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. Static/dynamic
linkage confirmed via `nm -g build/bin/runapp` and `ls build/dlib/atk`,
not assumed:

- **`atk/basics/x` and `atk/basics/common`**: both **statically linked
  into `runapp`** (confirmed: `environ__*`, `mb_*`, `cmenu_Destroy`,
  `cmenu_Activate` etc. all present directly in `nm -g build/bin/
  runapp`'s symbol table) — every ATK app (`ez`, `messages`, `help`,
  `figure`, `raster`, ...) exercises both directories just by starting
  up, since they're the X11 plumbing (`xim`, `menubar`, `xws`) and the
  41-class core (`im`, `view`, `environ`, `message`, `dataobj`,
  `graphic`, `event`, `proctbl`, `image`, `jpeg`) respectively.
  `atk/basics/x` also builds one standalone CLI, `atkatoms` (confirmed
  no `.do`/symlink involvement — it's a plain installed binary).
- **`atk/figure`**: fully dynamic — `figure.do`, `figv.do`,
  `figtoolv.do`, `figattr.do`, `fontsel.do`, `fontselv.do`,
  `fontsamp.do`, `figio.do` all in `build/dlib/atk`, zero symbols in
  `runapp` itself. `build/bin/figure` is a symlink to `runapp` (like
  `ez`/`messages`), not a separate binary.
- **`atk/syntax/tlex`**: dynamic — `tlex.do` in `build/dlib/atk`, zero
  symbols in `runapp`. Also builds one standalone build-time CLI tool,
  `gentlex` (confirmed no `.do`/symlink involvement, same pattern as
  batch 2's `fdbbdf`).
- **`atk/raster/cmd`**: dynamic — `raster.do`, `rastimg.do`,
  `rasterv.do`, `rastoolv.do` in `build/dlib/atk`, zero symbols in
  `runapp`. `build/bin/raster` is a `runapp` symlink. **Correction to
  the prompt's own framing**: `atk/raster/cmd` does *not* build
  `convertraster` — that's a separate sibling directory,
  `atk/raster/convert`, consuming only `atk/raster/lib`. A pre-existing
  test battery documents this exact distinction in its own header
  comment (see below) — I did not conflate the two.

1. **`atk/basics/x` + `atk/basics/common`** (foundational — every app
   launch exercises both; given their size and static linkage, worth
   more than "does it open"):
   ```
   DISPLAY=:0; ez
   ```
   Confirm the window opens, draws its menubar (exercises `menubar.c`'s
   `mb_*` fixes directly), and a right-mouse popup menu appears and is
   selectable/cancelable (exercises `cmenu_Destroy`/`cmenu_Activate` —
   these fire on every popup-menu dismiss/activate, so this is a
   direct, not incidental, exercise of the riskiest fix in the batch,
   the `_STDC_`-guarded `cmenu.h` workaround). Open a plain text file,
   type a few characters, resize the window (exercises `environ.c`,
   `event.c`'s `osi_GetTimes` via redraw/timer paths, `graphic.c`).
   Then, separately:
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/PAPERS/atk/Sherman.Alloc
   ```
   (open **read-only** — do not save; per `rollout-procedure.md`, this
   fixture is not under source control and a save silently overwrites
   it) — this document has an embedded raster/JPEG-adjacent image
   pipeline reachable through `atk/basics/common/image.c`'s
   `to64`/`from64`/`osi_GetTimes` fixes if it contains any image
   insets; even if not, opening a real multi-inset document (it has
   `bp`/`cel`/`table`/`calc`/`eq`/`fad`/`zip` insets) exercises
   `dataobj.c`/`proctbl.c` broadly.
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/atkatoms
   ```
   (standalone CLI, exercises `atkatoms.c`'s `xim_SetupAtoms` fix
   directly — confirm it exits cleanly, no crash; it talks to the X
   server so needs `DISPLAY` set, run it as `DISPLAY=:0;
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/atkatoms`).

2. **`atk/figure`** (the figure/drawing inset — confirmed **not**
   present in the `Sherman.Alloc` fixture, so it needs its own
   document; two real fixtures with figure insets exist in `PAPERS`):
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/PAPERS/conf/1993/Inglett
   ```
   (open **read-only**, do not save — same caution as above) or
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/PAPERS/conf/1994/Tut.lv
   ```
   Confirm the figure inset renders (lines/shapes/text as drawn
   originally, no garbage or blank box) and scrolls into view cleanly.
   For a more direct, deliberate poke at the fixed code paths
   specifically (string handling across all 9 touched files is fairly
   generic, so a full exercise of the figure toolbar is worth it):
   `<ESC><TAB>` and type `figure` in a scratch `ez` buffer to insert a
   fresh empty figure inset, then try the drawing toolbar (rectangle,
   line, text, font-selection dialog — exercises `fontsel.c`/
   `fontselv.c`/`fontsamp.c` directly) and confirm no crash on a few
   basic draw/undo operations.

3. **`atk/syntax/tlex`** (ctext syntax coloring/indent — historically
   verified via a scratch `.c` file in `ez`, per the prompt):
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/andrew-6.4/src/atk/eq/eq.c
   ```
   (open **read-only** — it's a real source file in the tree; no need
   to save) Confirm syntax coloring/indentation activates and renders
   sensibly (keywords/comments highlighted, no crash, no garbled
   colors) — this loads `tlex.do` and exercises the `gentlex.c`/
   `charset.c`/`defaults.c`/`readtabc.c`/`readtlx.c`/`thongs.c`/
   `tlex.c` fixes as a group, since they're all internal machinery
   behind the single syntax-table-driven coloring pipeline.
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/gentlex
   ```
   run with no args (standalone build-time CLI tool, confirms it at
   least starts and prints usage/doesn't crash — it's not part of any
   app's runtime, only used to regenerate `.tlc` syntax tables at
   build time, so a full functional exercise isn't warranted here).

4. **`atk/raster/cmd`** (raster/bitmap image inset — dynamic, no
   standalone CLI of its own; existing byte-diff batteries exist for
   the *separate* `convertraster` tool and are useful due-diligence
   but are NOT a direct test of this directory's own code — read their
   own header comments, reproduced here for clarity):
   ```
   sh /Users/wdc/src/AUIS/test-baselines/raster-cmd-pi/run-battery.sh before
   ```
   This script's own comment says it best: *"convrast.c (convertraster)
   does NOT #include any of raster/cmd's .ih files... This battery is
   a due-diligence regression check, not a direct verification of the
   raster/cmd flag; the real consumer check for raster/cmd is the ez
   raster-inset visual check."* Run it once before trusting any of
   this batch's `atk/raster/cmd` fixes and diff against a pre-existing
   `before` capture if one exists from an earlier M1 rollout point, to
   confirm nothing regressed incidentally. For the actual direct check:
   ```
   DISPLAY=:0; ez
   ```
   `<ESC><TAB>` and type `raster` to insert a fresh raster inset, or
   open any document with an embedded raster/bitmap image if one is
   on hand. Confirm it renders, and specifically exercise the
   `dispbox.h`-family fixes (the riskiest/most-edited part of this
   directory): resize the raster inset's display box (exercises
   `DisplayBoxBlitOverlap`/`DisplayBoxHide`/`SetPixelBehindDisplayBox`),
   select a region and confirm the highlight draws and clears cleanly
   (exercises `DrawHighlightBehindDisplayBox`/
   `DisplayBoxHideOverlappingHighlight`), and try the zoom-in/zoom-out/
   pan/tool-select/region-select/touch-up toolbar buttons if present
   (exercises `CenterCommand` through `TouchUpCommand`, all newly
   locally-declared in `rastvaux.c`). No crash on any of these is the
   success condition.

## 9. Open questions / anything that surprised you

- The `-ferror-limit=20` truncation on `xim.c` (§6) is a real
  near-miss worth the runbook note — it's the same *shape* of problem
  as the `-k` undercounting lesson (partial visibility hiding real
  fallout behind a different limiting mechanism), just at the
  per-file rather than per-directory level. Recommend folding into
  `rollout-procedure.md`.
- The `cmenu.h` `_STDC_`/`__STDC__` typo (§4) is the first *bug*, as
  opposed to *incompleteness*, found in an outside header this M2
  pass — worth flagging to wdc directly since it's a one-character fix
  with a clear, confirmable root cause (no `_STDC_` define anywhere in
  the tree) whenever `overhead/cmenu` gets any kind of housekeeping
  attention, M2-related or not.
- The prompt's own text about `atk/raster/cmd` building `convertraster`
  was simply wrong (§3/§8) — checked before trusting it, per the
  prompt's own explicit instruction to verify rather than assume.
  Surfacing this in case it affects planning for later batches that
  reference the same assumption.
- I found and fixed one over-cautious edit of my own after the gate
  (§1/§7's `menubar.c` `errprintf` extern) — mentioning it in detail
  rather than quietly cleaning it up, since the reasoning for not
  re-running the full gate afterward is a judgment call wdc should be
  able to see and override.
- No hard stops triggered: no called-but-undefined function anywhere,
  no header-vs-usage type disagreement, and every fix stayed inside
  its own flagged directory's `.c`/`.h` files (the `cmenu.h` situation
  was the closest call — resolved by local-extern in the *consuming*
  file, `xim.c`, not by editing the outside header).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.

## 10. Post-gate regression report and investigation: `atk/figure` label rendering

**Reported by wdc after running §8's runtime checks**: `atk/basics/x`+
`atk/basics/common`, `atk/syntax/tlex`, and `atk/raster/cmd` all passed
clean. `atk/figure` did not — figure insets' text labels render as
"stray single characters/garbage instead of the real text" (e.g.
garbled style names), confirmed in two documents
(`/Users/wdc/src/AUIS/PAPERS/conf/1993/Inglett` and
`/Users/wdc/src/AUIS/NEWSLETTERS/EZ/95Summer.ez`, the latter a document
wdc knows well, specifically re-checked to rule out it being
fixture-specific). Boxes/lines position correctly — only the text
*content* inside is wrong, not a layout bug.

### 10.1 Initial investigation: the leading hypothesis (strlen signedness) and what it turned up

The instructed leading hypothesis was: `atk/figure`'s entire diff is 9
one-line `#include <string.h>` additions (confirmed via `fossil diff`
— nothing else changed in this directory), and `strlen`'s return type
flips from implicit `int` (signed) to real `size_t` (unsigned) once
declared, which could change the result of `strlen(x) - N`-shaped
arithmetic when it would go negative.

**Ruled out, both mathematically and empirically.** Grepped every
`strlen` call site across all 9 files (21 total) — every one is either
an addition (`malloc(strlen(x)+1)`, never sign-sensitive) or a
comparison against 0 (`strlen(x)==0`/`!=0`, identical truth value
under either signedness) except one real subtraction,
`figtoolv.c`'s `WhiteKillString`:
`for (cx = buf + (strlen(buf)-1); cx>=buf && !isgraph(*cx); cx--);`.
Worked through the bit-level arithmetic by hand (a 32-bit signed `-1`
sign-extends to the identical 64-bit bit pattern as 64-bit unsigned
`SIZE_MAX`, so `buf + (either)` lands on the same address either way),
then **empirically confirmed with a standalone extracted copy of the
function**, compiled both with and without `<string.h>` at the real
project's exact flags (`-std=gnu89 -Wno-implicit-int ... -O` — `-O`
matters, confirmed via `grep CDEBUGFLAGS`/`system.mcr`, in case an
optimizer exploited the technically-UB pointer arithmetic
differently): byte-identical output across empty/all-space/leading/
trailing/single-char test inputs, at both `-O0` and `-O1`. Also
audited `atk/basics/common/dataobj.c`'s `dataobject__ViewName`
(touched this same session, a plausible cross-directory link since
it's the generic mechanism ATK uses to resolve which view class
renders a dataobject) for the same `strlen(x)-strlen(y)` shape —
same result, empirically byte-identical before/after across a battery
of realistic and pathological class names.

**Found something real but ultimately a dead end**: adding
`<string.h>` does change generated code — clang's fortify-source
builtin transforms some `strcpy(dest, "compile-time-literal")` calls
into `__strcpy_chk(dest, src, N)` (a checked variant that aborts, via
`Trace/BPT trap`, confirmed with a deliberately-mismatched standalone
test) once it can see the real prototype and trace a matching
`malloc(strlen(literal)+1)` immediately before. Found via `objdump -r`
diffing before/after `.o`s for all 9 files (5 showed size deltas: a
uniform offset shift consistent with one new instruction per
transformed call site, not per-function logic changes) and confirmed
against the real, already-installed `figure.do`
(`nm -u build/dlib/atk/figure.do` does list `___strcpy_chk`, resolved
against `libSystem.B.dylib`). Traced all 9 transformed call sites
(`figobj.c` ×2 — `figattr_SetColor(self->iattr,"black")` and
`figattr_SetFontFamily(self->iattr, fontsel_default_Family)`, both
macro-expanded from `figattr.ih`; `figv.c` ×1; `figtoolv.c` ×2;
`fontselv.c` ×1; `fontsamp.c` ×3, all tracing to the same
`INITTESTSTRING`/`malloc`/`strcpy` triple, just duplicated by
inlining) back to source: every single one copies a compile-time
literal whose length exactly matches (or is comfortably smaller than)
the destination's traced size — provably safe, and consistent with
zero crash being reported (a real mismatch aborts the whole process,
not garbles one inset's text). Also checked: this fortification isn't
even new to the tree from this session — the **unmodified, reverted**
`figure.do` (see §10.3) still links `___strcpy_chk` from files never
touched this session (`figotext.c` etc., which already had a working
`strlen`/`strcpy` declaration reaching them transitively before this
batch even started); and every `.do` in `build/dlib/atk` already links
`__strcpy_chk`/`__sprintf_chk`/etc. tree-wide, unrelated to M2. No
struct-field/macro name collision with any `<string.h>` identifier
found anywhere in the 9 files either (grepped explicitly, per the
prompt's second-hypothesis ask).

### 10.2 Coordinator redirect: a known, open, unrelated bug fits the symptom better

Before going further down the signedness/fortification path, the
coordinator relayed wdc's own lead plus a check of
`porting-assessment.md` §17 that changes the picture entirely:

- §17 documents an **already-open, unresolved** bug, found 2026-07-12
  (three days *after* `atk/figure`'s rendering was last visually
  confirmed correct, at M1 pilot B on 2026-07-09 — nobody has
  re-checked figure's text rendering since): during incremental
  Xft redraws, `xgraphic_DrawChars`'s erase-by-redraw path shows only
  a **suffix** of the correct string — leading characters go missing
  (the doc's own example: typing `123+4=` shows `1`, `2`, `23`, `3+`,
  `23+4` instead of the correct progressive string). For a short
  label like a style name, "only the suffix survives" reads exactly
  like "stray single characters" — a strong symptom match.
- This bug lives in the one shared function *all* Xft text rendering
  in the tree funnels through — confirmed `fontsamp.c` (one of our 9
  touched files) calls `fontsample_DrawString`, which funnels through
  that same shared path, so `atk/figure` is a plausible victim,
  unrelated to anything in this M2 batch's diff.

**Correction, added after §11's follow-up investigation**: this §17
lead turned out to be a dead end, not a confirmation — see §11.1. Left
this subsection as originally written (reasoning at the time, not
wrong given what was known then) rather than rewritten in hindsight;
§11 is the section with the corrected, final framing of what
`atk/figure`'s bug actually is (unknown) and isn't (§17's bug, or
caused by this M2 batch).

### 10.3 Decisive test performed (per the coordinator's exact instructions)

1. `fossil revert` on exactly the 10 `atk/figure` files this session
   touched (`Imakefile` + 9 `.c` files) — **and only those**; the other
   4 directories' fixes were left untouched.
   ```
   $ fossil revert src/atk/figure/Imakefile src/atk/figure/figobj.c \
       src/atk/figure/figoins.c src/atk/figure/figoplin.c \
       src/atk/figure/figtoolv.c src/atk/figure/figure.c \
       src/atk/figure/figv.c src/atk/figure/fontsamp.c \
       src/atk/figure/fontsel.c src/atk/figure/fontselv.c
   REVERT   src/atk/figure/Imakefile
   REVERT   src/atk/figure/figobj.c
   REVERT   src/atk/figure/figoins.c
   REVERT   src/atk/figure/figoplin.c
   REVERT   src/atk/figure/figtoolv.c
   REVERT   src/atk/figure/figure.c
   REVERT   src/atk/figure/figv.c
   REVERT   src/atk/figure/fontsamp.c
   REVERT   src/atk/figure/fontsel.c
   REVERT   src/atk/figure/fontselv.c
   ```
2. Rebuilt `atk/figure` clean with the original, un-flagged code:
   `make clean && make depend && make -k install` — exit 0, 0 errors.
   Explicitly regenerated the Makefile (`make Makefile`) and confirmed
   `COMPILERFLAGS` is back to the bare `system.mcr` default (the
   `-Wno-implicit-function-declaration` suppression, not this batch's
   `-Werror` override) — i.e. this is a genuine rebuild of the
   *original* pre-M2 code, not just a no-op flag check. Ran the full
   `clean && depend && install` cycle a second time for good measure
   — exit 0, 0 errors both times. New `figure.do`/`figv.do`/
   `fontsamp.do`/etc. installed to `build/dlib/atk`.
3. Did **not** attempt to re-test in the GUI myself, per the explicit
   instruction — this needs wdc's visual confirmation.

**Current tree state** (confirmed via `fossil status`, reproduced in
full):
```
$ fossil status
...
EDITED     src/atk/basics/common/Imakefile
EDITED     src/atk/basics/common/dataobj.c
EDITED     src/atk/basics/common/environ.c
EDITED     src/atk/basics/common/event.c
EDITED     src/atk/basics/common/graphic.c
EDITED     src/atk/basics/common/im.c
EDITED     src/atk/basics/common/image.c
EDITED     src/atk/basics/common/jpeg.c
EDITED     src/atk/basics/common/proctbl.c
EDITED     src/atk/basics/x/Imakefile
EDITED     src/atk/basics/x/atkatoms.c
EDITED     src/atk/basics/x/menubar.c
EDITED     src/atk/basics/x/menubar.h
EDITED     src/atk/basics/x/xim.c
EDITED     src/atk/raster/cmd/Imakefile
EDITED     src/atk/raster/cmd/dispbox.c
EDITED     src/atk/raster/cmd/dispbox.h
EDITED     src/atk/raster/cmd/raster.c
EDITED     src/atk/raster/cmd/rasterv.c
EDITED     src/atk/raster/cmd/rastimg.c
EDITED     src/atk/raster/cmd/rastvaux.c
EDITED     src/atk/syntax/tlex/Imakefile
EDITED     src/atk/syntax/tlex/charset.c
EDITED     src/atk/syntax/tlex/defaults.c
EDITED     src/atk/syntax/tlex/gentlex.c
EDITED     src/atk/syntax/tlex/readtabc.c
EDITED     src/atk/syntax/tlex/readtlx.c
EDITED     src/atk/syntax/tlex/thongs.c
EDITED     src/atk/syntax/tlex/tlex.c
```
No `src/atk/figure/*` entries — fully back to committed form. 27 files
edited across the other 4 directories, exactly as before. No commits
made, at any point in this session.

Two diff files now sit at the tree root: `m2-batch3a-session.diff`
(the original, 772-line diff including the now-reverted `atk/figure`
fix — kept as the exact reference to reapply from if wdc confirms the
bug is pre-existing) and `m2-batch3a-post-figure-revert.diff` (600
lines, the *current* tree state — the other 4 directories only, no
`atk/figure`).

### 10.4 What happens next (as originally written — superseded by §11, kept for the record)

*(This subsection was written before wdc's controlled re-test result
came back; it lays out both branches of what might happen. §11 is
the actual outcome — the first branch below is what happened, with
one correction: see §11.1, do not read the §17-attribution part of the
first bullet below as confirmed.)*

- **If the garbled-label symptom persists** against this reverted,
  original `atk/figure` code: conclusively pre-existing, unrelated to
  M2, ~~almost certainly the §17 Xft suffix-only-redraw bug (or a
  sibling of it)~~ — re-apply this session's `atk/figure` fix (the 9
  one-line `#include <string.h>` additions plus the `Imakefile` flag,
  all reconstructable directly from this report's §3 `atk/figure`
  table, or from `m2-batch3a-session.diff`'s `atk/figure` hunks, which
  still has them even though the working tree no longer does), and
  this batch can proceed toward commit with the bug tracked separately
  (not fixed here — out of scope for M2).
- **If the symptom disappears** with the original code: it really was
  something in this session's fix, and the investigation in §10.1
  needs to go further (it did not find a confirmed mechanism, only
  ruled out the two hypotheses given) — likely warranting live `lldb`
  debugging against a reproducing document rather than further static/
  binary analysis, which has now been exhausted for the signedness and
  fortification angles.
- Either way: **do not commit** until this is resolved one way or the
  other.

**This is what happened**: the first branch (symptom persists,
pre-existing, unrelated to M2) — see §11 for the actual result, the
reapplied fix, and the corrected (non-§17) framing of the finding.

## 11. Resolution: confirmed pre-existing, fix reapplied, gate re-verified

### 11.1 Controlled re-test result

wdc's first re-test (against the §10.3 reverted build) was invalidated
on review: it used an already-running `ez` process, and `.do` files are
loaded via `dlopen` at first use and then cached for the life of the
process (per `sonnet-playbook.md`'s dynamic-load note) — so that first
attempt was almost certainly still exercising the *old, pre-revert*
`figure.do` already resident in the running process, not the freshly
rebuilt one on disk. Corrected with a **fresh `ez` restart** against
the reverted build (confirmed via `figv.do`'s on-disk timestamp
matching the §10.3 rebuild, ruling out a second stale-cache read): the
text-rendering corruption is **still present** with `atk/figure` fully
reverted to committed, pre-M2 code.

This is decisive on the one question that actually matters for this
M2 batch: since the corruption survives a full revert of every line
this session touched in `atk/figure`, **it cannot be caused by this
session's fix — it was already there**, regardless of what its actual
root cause turns out to be.

**Correction to §10.2's framing, before this gets treated as settled**:
§10.2 floated `porting-assessment.md` §17's Xft incremental-redraw bug
as the likely explanation, reasoning from a textual match (§17
describes a string's *suffix* surviving while leading characters go
missing) and a code-path match (`fontsamp.c` funnels through the same
shared `xgraphic_DrawChars` erase/redraw path §17 describes). That
lead does not hold up: wdc confirmed live (via `Sherman.Alloc`,
`calc`+`zip` both rendering perfectly) that `contrib/calc`'s own
instance of the §17 bug is now **fully fixed** — §17's document text
describing it as "still open" is itself stale, independently confirmed
by a Haiku audit. With calc's side of that bug closed, asserting
`atk/figure`'s corruption is "the same bug, a second manifestation" is
not supported — that would be inferring a shared root cause from a
surface symptom match alone, and the one bug we could compare against
no longer exists in the form that symptom match was based on. **Correct
framing: `atk/figure`'s text-rendering corruption is a distinct,
not-yet-root-caused bug**, confirmed pre-existing (via this section's
controlled revert/rebuild/fresh-restart test) and unrelated to this
M2 batch's `#include <string.h>` fix, but with its own root cause
still unknown — it needs its own fresh investigation, not an inherited
diagnosis from calc's now-closed bug. §10.1's exhaustive static/binary
check did rule out every mechanism traceable to this session's actual
diff (signedness, name collision, and the newly-discovered but
provably-safe `__strcpy_chk` fortification), which is what matters for
this batch's purposes: whatever is corrupting figure's text, it is not
in the 9-file, 9-line diff audited here.

### 11.2 Fix reapplied

Reapplied exactly the original `atk/figure` fix — not redone from
memory, but replayed from the saved `m2-batch3a-session.diff`'s
`atk/figure` hunks (`Imakefile` + the 9 `.c` files' single
`#include <string.h>` lines each), to guarantee byte-for-byte identity
with the original, already-reasoned-through fix:

```
$ sed -n '301,470p' m2-batch3a-session.diff > /tmp/figure-reapply.diff
$ patch -p0 --dry-run < /tmp/figure-reapply.diff    # clean dry-run first
$ patch -p0 < /tmp/figure-reapply.diff
patching file 'src/atk/figure/Imakefile'
patching file 'src/atk/figure/figobj.c'
patching file 'src/atk/figure/figoins.c'
patching file 'src/atk/figure/figoplin.c'
patching file 'src/atk/figure/figtoolv.c'
patching file 'src/atk/figure/figure.c'
patching file 'src/atk/figure/figv.c'
patching file 'src/atk/figure/fontsamp.c'
patching file 'src/atk/figure/fontsel.c'
patching file 'src/atk/figure/fontselv.c'
```

Verified byte-identical to the original full session diff:

```
$ fossil diff > /tmp/current-full.diff
$ diff /tmp/current-full.diff m2-batch3a-session.diff
$ echo $?
0
```

Rebuilt `atk/figure` clean twice to reconfirm determinism (same
recipe as every other directory this batch):

```
== pass 1 == EXIT=0, 0 errors
== pass 2 == EXIT=0, 0 errors
```

`grep -n COMPILERFLAGS Makefile` reconfirmed the M2 override (line
291) still wins over `system.mcr`'s default (line 177), same as every
other directory.

### 11.3 Tree-wide gate re-run

The revert/reapply round-trip disturbed the tree (a `make Clean`
followed by a partial-then-full `dependInstall` cycle happened mid-
investigation), so the full tree-wide gate was re-run once more to get
back to a cleanly verified state across all 5 directories together,
exactly as `m2-batch3a-prompt.md` specifies:

```
$ cd src && make Clean   # exit 0
$ make dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1   # backgrounded
```

Confirmed complete via `pgrep -f 'make dependInstall'` returning empty,
independently re-verified against the log rather than taken on trust:

```
$ grep -c "error:" dependInstall.log
4
$ grep -n "error:" dependInstall.log
109981:  200 |                 ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
173355:clang: error: linker command failed with exit code 1 (use -v to see invocation)
223717:ltapp.c:115:27: error: incompatible integer to pointer conversion ...
223729:ltapp.c:123:32: error: incompatible integer to pointer conversion ...
$ grep -n "building (dependInstall) (.*\(atk/basics/x\|atk/basics/common\|atk/figure\|atk/syntax/tlex\|atk/raster/cmd\))" dependInstall.log
27916:building (dependInstall) (.../src/atk/basics/common)
40065:building (dependInstall) (.../src/atk/basics/x)
99346:building (dependInstall) (.../src/atk/raster/cmd)
109461:building (dependInstall) (.../src/atk/syntax/tlex)
149328:building (dependInstall) (.../src/atk/figure)
```

Same 4 pre-existing baseline errors as every prior gate this batch
(recognizer-type false positive, `ams/msclients/nns` SSLLIB link
failure, `contrib/zip/utility/ltapp.c`'s two int-conversion errors),
all 5 directories confirmed descended. **Gate is green.**

### 11.4 Final state

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     00f76e973e61bab4610155a4d9dc6305fc33f962 2026-07-24 05:09:33 UTC
parent:       789544e5ba48a35ec062935ae5812ebc517dc8bf 2026-07-24 05:09:28 UTC
tags:         andrew-6.4
comment:      add m2-batch2-REPORT.md: M2 rollout point 2 (8-directory batch) session report (user: wdc)
EDITED     src/atk/basics/common/Imakefile
EDITED     src/atk/basics/common/dataobj.c
EDITED     src/atk/basics/common/environ.c
EDITED     src/atk/basics/common/event.c
EDITED     src/atk/basics/common/graphic.c
EDITED     src/atk/basics/common/im.c
EDITED     src/atk/basics/common/image.c
EDITED     src/atk/basics/common/jpeg.c
EDITED     src/atk/basics/common/proctbl.c
EDITED     src/atk/basics/x/Imakefile
EDITED     src/atk/basics/x/atkatoms.c
EDITED     src/atk/basics/x/menubar.c
EDITED     src/atk/basics/x/menubar.h
EDITED     src/atk/basics/x/xim.c
EDITED     src/atk/figure/Imakefile
EDITED     src/atk/figure/figobj.c
EDITED     src/atk/figure/figoins.c
EDITED     src/atk/figure/figoplin.c
EDITED     src/atk/figure/figtoolv.c
EDITED     src/atk/figure/figure.c
EDITED     src/atk/figure/figv.c
EDITED     src/atk/figure/fontsamp.c
EDITED     src/atk/figure/fontsel.c
EDITED     src/atk/figure/fontselv.c
EDITED     src/atk/raster/cmd/Imakefile
EDITED     src/atk/raster/cmd/dispbox.c
EDITED     src/atk/raster/cmd/dispbox.h
EDITED     src/atk/raster/cmd/raster.c
EDITED     src/atk/raster/cmd/rasterv.c
EDITED     src/atk/raster/cmd/rastimg.c
EDITED     src/atk/raster/cmd/rastvaux.c
EDITED     src/atk/syntax/tlex/Imakefile
EDITED     src/atk/syntax/tlex/charset.c
EDITED     src/atk/syntax/tlex/defaults.c
EDITED     src/atk/syntax/tlex/gentlex.c
EDITED     src/atk/syntax/tlex/readtabc.c
EDITED     src/atk/syntax/tlex/readtlx.c
EDITED     src/atk/syntax/tlex/thongs.c
EDITED     src/atk/syntax/tlex/tlex.c
```

34 files across all 5 directories, exactly matching the original batch
(same file list as §7's table, byte-identical `fossil diff` against
`m2-batch3a-session.diff`, confirmed in §11.2).

All 5 directories' fixes are in place, matching the original batch
exactly (confirmed byte-identical via `diff` against
`m2-batch3a-session.diff` in §11.2, itself unaffected by the gate
re-run since the gate doesn't touch source files). No commits made, at
any point in this session — that's still explicitly the coordinator's
call, pending wdc's final sign-off on this gate result.

### 11.5 Recommended follow-up (not done here)

- `atk/figure`'s text-rendering corruption (label text in the
  `figotext` family, and/or `fontsamp.c`'s font-preview string —
  §10's original report didn't pin down which specific rendering path
  is misbehaving, only that the symptom is real and pre-existing)
  needs its **own** fresh investigation, likely starting with live
  debugging against a reproducing document (`Inglett` or
  `95Summer.ez`) rather than more static analysis — §10.1 already
  exhausted the static/binary-level checks available from just reading
  this session's diff, and that diff has now been confirmed *not* to
  be the cause. Do **not** start that investigation from the
  `porting-assessment.md` §17 write-up as a prior — that lead was
  explored here and didn't hold up once calc's own instance of §17's
  bug turned out to already be fixed (§11.1). `porting-assessment.md`
  itself may need a §17 correction/addendum (the "still open" text is
  stale per wdc's live confirmation, independently flagged by a Haiku
  audit) — not done here, out of scope for this M2 session.
- Treat this as a **new, distinct, not-yet-root-caused finding**,
  worth its own `porting-assessment.md` entry once someone investigates
  it properly, rather than folding it into §17's (now largely resolved)
  write-up.
- Out of scope for M2 either way: this session's `atk/figure` diff is
  exactly the 9 `#include <string.h>` lines audited throughout
  §3/§10/§11, nothing more, and it has been conclusively ruled out as
  the cause.
