# M2 rollout point 2: small/leaf directory batch — `atk/frame`, `atk/adew`, `atk/value`, `atk/lookz`, `atk/help/src`, `atk/extensions`, `overhead/cmenu`, `overhead/fonts/cmd`

## 1. Status

Stopped at the (only) gate. No commits made. Working tree left as-is:
32 files edited (8 `Imakefile`s + 24 `.c` files) across the 8 directories,
plus one pre-existing unrelated edit to `revival/doc/m2-rollout-runbook.md`
from before this session (not touched here). Session diff at tree root:
`m2-batch2-session.diff`. Full gate log at the canonical path
`~/src/AUIS/andrew-6.4/dependInstall.log`.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`, `m2-rollout-runbook.md`,
   `m2-batch2-prompt.md`, and the `atk/eq` pilot report in full.
2. Confirmed none of the 8 directories use `Parser()`/`LexFile`
   (grepped each Imakefile) — no generated-source gap to work around,
   unlike the pilot.
3. For each directory: flagged its `Imakefile` with the runbook's exact
   four-flag `COMPILERFLAGS` override, verified via `make Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override line always appears after
   `system.mcr`'s default, confirmed for all 8), then
   `make clean && make depend && make -k install`, fixed fallout, then
   rebuilt clean twice more to confirm determinism (three total clean
   builds per directory: first pass surfacing errors, then two
   error-free passes back to back).
4. Ran the full tree-wide gate: `cd src && make Clean` (exit 0), then
   `make dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1`
   (backgrounded, confirmed completion via `pgrep -f 'make dependInstall'`
   returning empty).
5. Verified the gate log independently:
   ```
   $ grep -c "error:" dependInstall.log
   4
   $ grep -n "error:" dependInstall.log
   109590:  197 |    ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
   172910:clang: error: linker command failed with exit code 1 (use -v to see invocation)
   223272:ltapp.c:115:27: error: incompatible integer to pointer conversion ...
   223284:ltapp.c:123:32: error: incompatible integer to pointer conversion ...
   ```
   All 4 are the known pre-existing baseline named in the prompt (recognizer-type
   false positive, `ams/msclients/nns` SSLLIB link failure, `contrib/zip/utility/ltapp.c`'s
   two int-conversion errors) — identical to the `atk/eq` pilot's baseline. Confirmed all
   8 directories actually descended during the gate:
   ```
   66838:building (dependInstall) (.../src/atk/frame)
   92230:building (dependInstall) (.../src/atk/adew)
   87087:building (dependInstall) (.../src/atk/value)
   145196:building (dependInstall) (.../src/atk/lookz)
   135882:building (dependInstall) (.../src/atk/help/src)
   72811:building (dependInstall) (.../src/atk/extensions)
   21681:building (dependInstall) (.../src/overhead/cmenu)
   21310:building (dependInstall) (.../src/overhead/fonts/cmd)
   ```
   Gate is green: zero new `error:` lines anywhere in a 227,153-line log,
   the only 4 in the whole file being the known baseline (none of the 4
   file paths belong to any of these 8 directories or their consumers).
6. `fossil status` (below) confirms exactly the files touched. No commit made.

## 3. Per-directory findings

Every directory's real, `-k`-derived instance count matched the stale
census table exactly — no surprises in volume, several in taxonomy (see
§4). Total: 70 instances across 24 files.

### `src/atk/frame` — 8 (matches stale count of 8)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `framecmd.c:643` | `frame_VisitFilePrompting` | Same-file forward reference (defined at 1377/1382) |
| 2 | `framecmd.c:1328` | `frame_VisitNamedFile` | Same-file forward reference (defined at 1400/1405) |
| 3 | `framecmd.c:1397` | `frame_VisitNamedFile` | Same-file forward reference |
| 4 | `framecmd.c:1576` | `frame_WriteFile` | Same-file forward reference (defined at 1584/1589) |
| 5 | `framecmd.c:1580` | `frame_WriteFile` | Same-file forward reference |
| 6 | `framecmd.c:1900` (now 1904 after edit) | `osi_GetTimes` | **Missing in-tree/project header — none exists anywhere** (new sub-case, see §4) |
| 7 | `framev.c:332` | `strlen` | Missing standard-library header |
| 8 | `helptxtv.c:102` | `strlen` | Missing standard-library header |

Fix: added `#include <string.h>` to `framev.c`/`helptxtv.c`; added a
small forward-declaration block in `framecmd.c` next to the file's own
existing `static int LocalReadFile();` forward-declare convention
(lines 1-3 non-`static`, since `frame_VisitFilePrompting`/
`frame_VisitNamedFile`/`frame_WriteFile` are explicitly commented
"Not static so it can be used from eza.c" at their real definitions),
plus a typed `extern int osi_GetTimes();` sourced from the real
definition in `overhead/util/lib/times.c:44`
(`int osi_GetTimes(blk) struct osi_Times *blk;`).

### `src/atk/adew` — 7 (matches stale count of 7)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `arbiter.c:123` | `strcmp` | Missing standard-library header |
| 2 | `pagev.c:506` | `strcpy` | Missing standard-library header |
| 3 | `pagev.c:573` | `strcmp` | Missing standard-library header |
| 4 | `genarb.c:53` | `exit` | Missing standard-library header |
| 5 | `mkcon.c:435` | `getprofileswitch` | Missing in-tree/project header (declared in `overhead/util/hdrs/util.h:105`) |
| 6 | `mkcon.c:647` | `chmod` | Missing standard-library header |
| 7 | `mkcon.c:667` | `chmod` | Missing standard-library header |

Fix: `<string.h>` to `arbiter.c`/`pagev.c`; `<stdlib.h>` to `genarb.c`;
`<sys/stat.h>` + `<util.h>` to `mkcon.c` (confirmed `util.h` is the
real declaring header via `grep`, and is the same header
`atk/basics/common/environ.c` already reaches `getprofileswitch`
through).

### `src/atk/value` — 6 (matches stale count of 6)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `controlv.c:88` | `strcpy` | Missing standard-library header |
| 2 | `controlv.c:90` | `strcat` | Missing standard-library header |
| 3 | `entrstrv.c:92` | `strcmp` | Missing standard-library header |
| 4 | `entrstrv.c:93` | `strlen` | Missing standard-library header |
| 5 | `mentstrv.c:94` | `strcmp` | Missing standard-library header |
| 6 | `mentstrv.c:95` | `strlen` | Missing standard-library header |

Fix: `#include <string.h>` in all three files. Zero surprises — purest
directory in the batch, single taxonomy category throughout.

### `src/atk/lookz` — 7 (matches stale count of 7)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `tabrulv.c:318` | `strcpy` | Missing standard-library header |
| 2 | `tabrulv.c:538` | `FindClosestTab` | Same-file forward reference (defined at 669/674) |
| 3 | `tabrulv.c:575` | `FindClosestTab` | Same-file forward reference |
| 4 | `lookz.c:97` | `strncmp` | Missing standard-library header |
| 5 | `lookz.c:97` | `strlen` | Missing standard-library header |
| 6 | `lookzv.c:304` | `FoldedEQ` | Missing in-tree/project header — none exists anywhere (see §4) |
| 7 | `lookzv.c:1943` | `FoldedEQ` | Same as #6 |

**Notable: resolves the runbook's own open flag.** `m2-rollout-runbook.md`'s
taxonomy section names `pref.c:71`'s `FoldedEQ` as an "unconfirmed
candidate" for the "possible genuine bug/typo — hard stop" category.
It is not a typo: `FoldedEQ` is real, correctly spelled, and defined
at `overhead/util/lib/foldedeq.c:89` (`FoldedEQ(s1, s2)`, implicit
`int` return, two `unsigned char *` args) — also called from
`atk/support/print.c` and `overhead/util/lib/config.c`. No header
anywhere in the tree declares the bare function (only the `FOLDEDEQ`
*macro* in `overhead/util/hdrs/util.h:69`, which calls it internally
but doesn't prototype it) — confirmed by `grep -rn FoldedEQ --include=*.h .`
returning zero prototype hits. Fixed with a local
`extern int FoldedEQ();` in `lookzv.c` sourced from the real
definition, following the eq pilot's precedent for functions with no
declaring header anywhere. **Recommend downgrading `pref.c:71`'s
`FoldedEQ` from "possible bug" to "known-real, needs the same local-extern
treatment" when that directory is flagged.**

### `src/atk/help/src` — 7 (matches stale count of 7)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `helpdb.c:174` | `index_Close` | Missing in-tree/project header — exists but incomplete (see §4) |
| 2 | `helpdb.c:310` | `index_Enumerate` | Same as #1 |
| 3 | `helpdb.c:320` | `index_Enumerate` | Same as #1 |
| 4 | `helpdb.c:842` | `index_GetData` | Same as #1 |
| 5 | `helpdb.c:900` | `recordset_Free` | Same as #1 |
| 6 | `hlptextv.c:124` | `strcmp` | Missing standard-library header |
| 7 | `hlptextv.c:131` | `strncpy` | Missing standard-library header |

**First real exercise of "missing in-tree/project header" as the runbook
originally described it** (a real header exists, just doesn't declare
everything). `helpdb.c` already includes `<index.h>`
(`overhead/index/index.h`), which declares 11 sibling `index_*`/
`recordset_*` functions via untyped `extern` — but not these 4. Two of
the runbook's own example names for this taxonomy bucket
(`index_Close`, `recordset_Free`) are exactly what showed up here.
Since `index.h` lives outside the flagged directory (`overhead/index`,
not `atk/help/src`), I did not edit it — per the M2 hard-stop rule
("editing one outside the directory is not, without escalating first"),
I added 4 local `extern int` declarations in `helpdb.c` itself, sourced
from the real definitions in `overhead/index/index.c` and
`overhead/index/rcordset.c` (all four are implicit/K&R `int`-returning).
**Flagging for wdc**: `overhead/index/index.h` itself looks stale/incomplete
independent of M2 — worth a housekeeping pass whenever `overhead/index`
is flagged, so future consumers don't have to re-derive these 4 locally.

### `src/atk/extensions` — 9 (matches stale count of 9)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `dsearch.c:178` | `strcmp` | Missing standard-library header |
| 2 | `dsearch.c:191` | `strcpy` | Missing standard-library header |
| 3 | `gsearch.c:180` | `strlen` | Missing standard-library header |
| 4 | `gsearch.c:183` | `strcpy` | Missing standard-library header |
| 5 | `gsearch.c:194` | `strcat` | Missing standard-library header |
| 6 | `gsearch.c:647` | `strncmp` | Missing standard-library header |
| 7 | `isearch.c:92` | `strlen` | Missing standard-library header |
| 8 | `isearch.c:92` | `strcpy` | Missing standard-library header |
| 9 | `isearch.c:147` | `strcmp` | Missing standard-library header |

Fix: `#include <string.h>` in `dsearch.c`, `gsearch.c`, `isearch.c`.
Single taxonomy category, zero surprises.

### `src/overhead/cmenu` — 18 (matches stale count of 18)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `cmactiv.c:166` | `FlipButton` | Missing in-tree/project header — declared nowhere (defined in `cmdraw.c`) |
| 2 | `cmactiv.c:207` | `FlipButton` | Same as #1 |
| 3 | `cmactiv.c:244` | `DrawMenus` | Same as #1 (defined in `cmdraw.c`) |
| 4 | `cmactiv.c:328` | `SelectionPtrToNum` | Same as #1 (defined in `cmmanip.c`) |
| 5 | `cmcreate.c:341` | `getprofileint` | Missing in-tree/project header (declared in `util.h`, but directory convention is local extern — see below) |
| 6 | `cmcreate.c:365` | `malloc` | Missing standard-library header |
| 7 | `cmcreate.c:386` | `atoi` | Missing standard-library header |
| 8 | `cmcreate.c:450` | `atoi` | Missing standard-library header |
| 9 | `cmcreate.c:602` | `free` | Missing standard-library header |
| 10 | `cmerror.c:71` | `sprintf` | Missing standard-library header |
| 11 | `cmmanip.c:49` | `malloc` | Missing standard-library header |
| 12 | `cmmanip.c:216` | `free` | Missing standard-library header |
| 13 | `scache.c:48` | `bzero` | Missing standard-library header (`<strings.h>` on Darwin, not `<string.h>`) |
| 14 | `scache.c:61` | `strlen` | Missing standard-library header |
| 15 | `scache.c:69` | `strcmp` | Missing standard-library header |
| 16 | `scache.c:79` | `realloc` | Missing standard-library header |
| 17 | `scache.c:80` | `malloc` | Missing standard-library header |
| 18 | `scache.c:92` | `strcpy` | Missing standard-library header |

Richest directory in the batch — all 3 live taxonomy categories in one
place. `FlipButton`/`DrawMenus`/`SelectionPtrToNum` are defined in
sibling files (`cmdraw.c`, `cmmanip.c`) within the same directory, with
no shared header declaring them; `cmdraw.h` (this directory's own
public header) already declares several *other* `cmdraw.c` functions
but not these three, **and** `cmdraw.c` itself already has a local
precedent for exactly this situation — a hand-written
`extern int SelectionPtrToNum();`, alongside 3 more locally-declared
sibling-file functions, sitting right above its own use, rather than
adding them to `cmdraw.h`. I followed that existing in-directory
convention rather than editing `cmdraw.h`, adding a 3-line local
`extern` block to `cmactiv.c`. Likewise `cmcreate.c` already locally
declares `getprofileswitch`/`getprofile` (real header `util.h` exists
and declares all three, but isn't included here) — I matched that
existing local-declaration style for `getprofileint` rather than
switching the file over to `#include <util.h>`. `bzero` needed
`<strings.h>` specifically (BSD/Darwin location), not `<string.h>`;
added both since `scache.c` also needed `strlen`/`strcmp`/`strcpy`.

### `src/overhead/fonts/cmd` — 8 (matches stale count of 8)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `fdbbdf.c:135` | `exit` | Missing standard-library header |
| 2 | `fdbbdf.c:143` | `strlen` | Missing standard-library header |
| 3 | `fdbbdf.c:178` | `atoi` | Missing standard-library header |
| 4 | `fdbbdf.c:185` | `fontcount` | Same-file forward reference (defined at 292) |
| 5 | `fdbbdf.c:187` | `fontcvt` | Same-file forward reference (defined at 317) |
| 6 | `fdbbdf.c:196` | `fontcvt` | Same-file forward reference |
| 7 | `fdbbdf.c:206` | `strncmp` | Missing standard-library header |
| 8 | `fdbbdf.c:375` | `strcat` | Missing standard-library header |
| — | (`fdbwm.c`/`wmfdb.c` never compiled — `WM_ENV` not defined in this build; only `X11_ENV`'s `fdbbdf` target is live) | | |

Fix: `<stdlib.h>` + `<string.h>` added; local forward declarations for
`fontcount`/`fontcvt` added directly above `main()` (no pre-existing
forward-declare convention in this single-file program, so I used the
plain untyped K&R style consistent with the rest of the batch).

## 4. Taxonomy findings / corrections

- **All three of the runbook's live taxonomy categories were exercised
  this batch** (the pilot only hit two). "Missing in-tree/project
  header" showed up in three distinct shapes, worth distinguishing for
  future directories:
  1. **Header exists and declares it, just isn't included**
     (`getprofileswitch` in `atk/adew`, real fix = add `#include <util.h>`).
  2. **Header exists, declares siblings, but is stale/incomplete for
     this function** (`index_Close`/`index_Enumerate`/`index_GetData`/
     `recordset_Free` vs. `overhead/index/index.h`; `FlipButton`/
     `DrawMenus`/`SelectionPtrToNum` vs. `overhead/cmenu/cmdraw.h`) —
     in both cases the *directory itself* already had a local-extern
     workaround precedent for this exact shape (`cmcreate.c`'s existing
     `getprofileswitch`/`getprofile` declarations; `cmdraw.c`'s existing
     `SelectionPtrToNum` declaration), which is strong evidence this is
     a known, lived-with gap rather than something to "fix properly" by
     editing the header as part of an M2 pass.
  3. **No header anywhere in the tree declares it, full stop**
     (`osi_GetTimes`, `FoldedEQ`) — the pilot's `eqview_Format` was
     actually this same sub-case, just same-directory. This session
     found it cross-directory twice (`osi_GetTimes` defined in
     `overhead/util/lib`, called from `atk/frame`; `FoldedEQ` defined in
     `overhead/util/lib`, called from `atk/lookz`).
- **The "possible genuine bug/typo" category is now empty, not just
  unconfirmed.** The runbook's one named candidate, `FoldedEQ`, is
  real — see §3's `atk/lookz` entry. No other candidate for this
  category turned up in 70 instances across 8 directories.
- **New mechanical observation, not a fallout category**: in two
  directories (`atk/help/src`'s `index.h`, `overhead/cmenu`'s
  `cmdraw.h`) a project header *sitting right there, already included*
  turned out to be the wrong place to fix the gap because it declares
  some-but-not-all of a function family and the directory had already
  established a "declare locally instead" habit for the gap. Worth
  folding into the runbook: when a real header exists but is
  incomplete, check whether the *consuming* directory already has a
  local-declaration precedent for that exact function family before
  deciding whether to extend the header or match the local pattern —
  both of this batch's incomplete-header cases had a local precedent,
  and using it kept every fix inside the flagged directory's own files
  (no hard-stop escalation needed).

## 5. Subtree-local gate vs. tree-wide gate

Second data point for `m2-rollout-runbook.md`'s "Gate scope" question.
Same result as the pilot: **the tree-wide gate found nothing beyond
what the 8 subtree-local `make clean && make depend && make -k install`
builds already showed.** Every one of the 70 instances was caught and
fixed at the subtree-local stage; the tree-wide gate's only contribution
was confirming zero cross-directory fallout (4 pre-existing baseline
errors, identical set to the pilot's baseline, none touching any of
these 8 directories or anything that consumes them). This batch is a
stronger data point than the pilot specifically because `overhead/cmenu`
*is* a directory with external consumers (`cmenu_*` symbols are
statically linked into `runapp`, i.e. every ATK app) — and it still
showed zero fallout beyond its own subtree build. Recommend treating
"subtree-local `make clean && make depend && make -k install` is a
sufficient per-directory gate, tree-wide gate only at coarser
checkpoints" as settled for M2, pending wdc's sign-off — two data
points now, one of them a statically-linked, tree-wide-consumed library.

## 6. Files touched (compile status)

All 32 edited files compile clean (0 errors) in their final state,
confirmed by two consecutive clean `make clean && make depend && make -k
install` passes per directory (three total per directory counting the
first, fallout-surfacing pass) plus the tree-wide gate.

- `src/atk/frame/Imakefile`, `framecmd.c`, `framev.c`, `helptxtv.c`
- `src/atk/adew/Imakefile`, `arbiter.c`, `genarb.c`, `mkcon.c`, `pagev.c`
- `src/atk/value/Imakefile`, `controlv.c`, `entrstrv.c`, `mentstrv.c`
- `src/atk/lookz/Imakefile`, `lookz.c`, `lookzv.c`, `tabrulv.c`
- `src/atk/help/src/Imakefile`, `helpdb.c`, `hlptextv.c`
- `src/atk/extensions/Imakefile`, `dsearch.c`, `gsearch.c`, `isearch.c`
- `src/overhead/cmenu/Imakefile`, `cmactiv.c`, `cmcreate.c`, `cmerror.c`, `cmmanip.c`, `scache.c`
- `src/overhead/fonts/cmd/Imakefile`, `fdbbdf.c`

Regenerated `Makefile`s in all 8 directories are not fossil-tracked, not
source edits. `revival/doc/m2-rollout-runbook.md` shows as EDITED in
`fossil status` but predates this session (untouched by me) —
carried over from the pilot session's process-gap writeup.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     ce319610b39532aabad085486ff212bdb9d7585f 2026-07-24 04:16:34 UTC
parent:       ef8db121cbf04ee2dbf19ea01b236ee8cc5357dd 2026-07-24 04:16:16 UTC
tags:         andrew-6.4
comment:      add m2-pilot-eq-REPORT.md: M2 rollout point 1 (atk/eq) session report (user: wdc)
EDITED     revival/doc/m2-rollout-runbook.md      <- pre-existing, not this session
EDITED     src/atk/adew/Imakefile
EDITED     src/atk/adew/arbiter.c
EDITED     src/atk/adew/genarb.c
EDITED     src/atk/adew/mkcon.c
EDITED     src/atk/adew/pagev.c
EDITED     src/atk/extensions/Imakefile
EDITED     src/atk/extensions/dsearch.c
EDITED     src/atk/extensions/gsearch.c
EDITED     src/atk/extensions/isearch.c
EDITED     src/atk/frame/Imakefile
EDITED     src/atk/frame/framecmd.c
EDITED     src/atk/frame/framev.c
EDITED     src/atk/frame/helptxtv.c
EDITED     src/atk/help/src/Imakefile
EDITED     src/atk/help/src/helpdb.c
EDITED     src/atk/help/src/hlptextv.c
EDITED     src/atk/lookz/Imakefile
EDITED     src/atk/lookz/lookz.c
EDITED     src/atk/lookz/lookzv.c
EDITED     src/atk/lookz/tabrulv.c
EDITED     src/atk/value/Imakefile
EDITED     src/atk/value/controlv.c
EDITED     src/atk/value/entrstrv.c
EDITED     src/atk/value/mentstrv.c
EDITED     src/overhead/cmenu/Imakefile
EDITED     src/overhead/cmenu/cmactiv.c
EDITED     src/overhead/cmenu/cmcreate.c
EDITED     src/overhead/cmenu/cmerror.c
EDITED     src/overhead/cmenu/cmmanip.c
EDITED     src/overhead/cmenu/scache.c
EDITED     src/overhead/fonts/cmd/Imakefile
EDITED     src/overhead/fonts/cmd/fdbbdf.c
```

No commit made. `m2-batch2-session.diff` (622 lines) written to the tree
root.

## 7. Runtime-check commands for wdc

**Load-bearing**: nothing from this batch should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. None of these
write to a tracked fixture — where a save is involved, save to a new
scratch path, never back over a test document already in the tree.

I confirmed statically which of the 8 directories actually link into
`runapp` (the binary `ez`/`messages`/`help`/etc. all symlink to) via
`nm -g build/bin/runapp`: `atk/frame` and `overhead/cmenu` are baked in
statically (their symbols appear directly in `runapp`); `atk/value`,
`atk/lookz`, `atk/adew`, `atk/extensions`, `atk/help/src` are
dynamically loaded `.do`s from `build/dlib/atk` on demand, confirmed
present there. `overhead/fonts/cmd` (`fdbbdf`) links into nothing —
it's a standalone CLI tool.

1. **`atk/frame`** (window/frame chrome, statically linked — exercised
   by simply having any window open, but the specific fixed functions
   need a deliberate poke):
   ```
   DISPLAY=:0; ez
   ```
   In the ez window: use **File > Visit File...** to open any existing
   text file (exercises `frame_VisitFilePrompting`/`frame_VisitNamedFile`),
   make a trivial edit, then **File > Write File** and save to a new
   scratch path such as `/tmp/m2-batch2-frame-test.txt` (exercises
   `frame_WriteFile`). Confirm the file round-trips correctly and no
   crash/hang. `framev.c`/`helptxtv.c` (`strlen`) are exercised just by
   the window drawing/redrawing normally — no special action needed,
   but watch for any visual glitch while scrolling or resizing.

2. **`atk/value`** (button/slider/checkbox/text-entry view classes,
   dynamically loaded, no dedicated top-level app):
   ```
   DISPLAY=:0; ez
   ```
   Insert a value-derived inset — press `<ESC><TAB>` in the ez buffer
   and type `button` (or `slider`) to insert one, per the same
   insertion pattern already used for clock/calc insets this project.
   Click the button/slider a few times to confirm it responds and
   redraws. Alternatively, if a known-good calc test document is
   still on hand from the calc-ghost-fix work, reopening it also
   exercises this code path (calc's "=" button uses this class family).

3. **`atk/lookz`** (style-sheet / tab-ruler editor, dynamically loaded):
   ```
   DISPLAY=:0; ez
   ```
   Open the style/edit-style command — typically under the **Format**
   or **Edit** menu, "Edit Style Sheet" or similar (help alias confirms
   it's reachable as "edit-styles"/"editstyle"). Confirm the style
   editor pane opens, the tab ruler renders, and typing in it doesn't
   crash. This is the same class family behind the "Set Options" UI
   the writable-string-literal fix touched, so also worth toggling a
   couple of style options if the panel offers them.

4. **`atk/adew`** (arbiter/cell/page container documents, dynamically
   loaded, help alias "ado"):
   ```
   DISPLAY=:0; ez
   ```
   In ez, `<ESC><TAB>` and type `page` (or `arbiter`) to insert an ADEW
   container inset, same pattern as #2. This is a less common code
   path than the others in this batch (multimedia container documents),
   so a simple "does it insert without crashing, does it redraw"
   check is sufficient. `arb`/`createcon` are also now-rebuilt
   standalone CLI tools if a more direct exercise is wanted:
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/arb
   ```
   (prints its generated Ness fragment to stdout — confirm no crash,
   inspect output looks sane).

5. **`atk/help/src`** (the `help` app itself, plus `hlptextv`'s text
   view used inside it):
   ```
   DISPLAY=:0; help
   ```
   Confirm the help browser launches, displays its default page, and
   a search/navigation action within it (e.g. following a link, or the
   help index) doesn't crash — that path exercises `helpdb.c`'s
   `index_*`/`recordset_Free` calls directly (help's on-disk index).

6. **`atk/extensions`** (interactive search/meta-x/spell/tags/etc.,
   statically-reachable via `ez`'s standard bindings):
   ```
   DISPLAY=:0; ez
   ```
   With some text loaded in a buffer: incremental search (usually
   `C-s`) exercises `isearch.c`; the **Search** menu command (non-incremental)
   exercises `gsearch.c`; Meta-X style command entry (usually `ESC x` or
   a "Do Command" menu item) exercises `metax.do`. `dsearch.c`
   ("directory search"/recursive-visit-file variant) is reachable via
   its help-aliased command — check the Search submenu for a
   "recursive" or directory-search entry if present.

7. **`overhead/cmenu`** (popup/command-menu widget, statically linked
   into every app):
   ```
   DISPLAY=:0; ez
   ```
   Trigger a popup/command menu — classically the right mouse button
   (or whatever XQuartz has that button mapped to) held down over the
   text area brings up ATK's Commands popup menu. Confirm it appears,
   items are selectable, and dismissing it (click elsewhere or release
   off-menu) doesn't crash. This is the single highest-blast-radius
   directory in the batch (statically linked, used by every app via
   `runapp`), so it's worth a slightly more thorough poke — try both
   popping the menu up and cancelling it, and popping it up and
   actually selecting an item.

8. **`overhead/fonts/cmd`** (`fdbbdf` — standalone CLI font converter,
   **no GUI runtime fixture**, confirmed: it links into nothing, no
   Imakefile/script anywhere in the tree invokes it). CLI smoke test
   instead:
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/fdbbdf /Users/wdc/src/AUIS/andrew-6.4/src/atk/eq/sym12.fdb > /tmp/m2-batch2-fdbbdf-test.bdf
   ```
   Confirm it exits 0 and `/tmp/m2-batch2-fdbbdf-test.bdf` contains a
   plausible-looking BDF font (starts with `STARTFONT`, has multiple
   `STARTCHAR`/`ENDCHAR` blocks, ends with `ENDFONT`) rather than being
   empty or truncated.

## 8. Open questions / anything that surprised you

- `FoldedEQ` turning out to be real (not the runbook's suspected typo)
  is worth a small runbook edit — see §3/§4's recommendation to
  reclassify it before whichever directory hits `pref.c:71`.
- The two "incomplete project header, but the consuming directory
  already has a local-declare habit for it" cases (`index.h`,
  `cmdraw.h`) were not anticipated by the runbook's taxonomy text as
  written — it describes "find the header that declares it" as the
  default move, but in both real instances here the better match was
  "the flagged directory has already been doing this locally for
  years, do the same" rather than editing a header outside the
  directory. Recommend folding this into `m2-rollout-runbook.md`'s
  taxonomy section (see §4's last bullet) before the next batch hits
  it again.
- `overhead/fonts/cmd` genuinely has no GUI runtime fixture, confirmed
  empirically rather than assumed, per the prompt's explicit
  instruction to check rather than assume — it doesn't even appear as
  a dependency of anything else `grep`-able in the tree's Imakefiles/
  scripts.
- No hard stops triggered: no called-but-undefined function, no
  header-vs-usage type disagreement, no fix required editing anything
  outside a flagged directory's own files (the two closest calls —
  `index.h`, `cmdraw.h` — were both resolved by staying inside the
  flagged directory, matching existing local precedent, rather than
  needing to edit the outside header).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
