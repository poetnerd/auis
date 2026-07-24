# M2 rollout runbook: enabling `-Werror=implicit-function-declaration` per directory

Procedure for executing one "M2 rollout point" from `roadmap.md`
(Medium-term → ANSI C conversion → M2 — Prototype sweep). Written
2026-07-24 from a tree-wide `-Wimplicit-function-declaration` census
(not yet a fixing pass — see "Status" below) plus M2 point 0's
already-completed triage. Background: `porting-assessment.md` §14.

**Read `rollout-procedure.md` first** — the session rhythm, logging
discipline, gate definition, runtime-check cautions, commit
conventions, and delegation rules are shared across M1–M4 and live
there, extracted from `claude-history/m1-rollout-runbook.md` once M2
needed the identical rhythm under a different flag. This file covers
only what's specific to M2: the flag mechanics, the fallout taxonomy,
the census, and ordering.

**Status (2026-07-24):** rollout points 1 (`atk/eq` pilot), 2 (8-
directory small/leaf batch), 3 batch A (5-directory mid-size batch),
and 3 batch B (5-directory mid-size batch) have all closed their gates
the same day. 745/745 combined predicted-or-derived instances
confirmed across 19 directories (528 of those batch B's, where no
reliable stale count existed — see below), no volume surprises except
batch B's own `ams/libs/cui` (350 actual vs. the runbook's stale
"1–42" bucket) and `ams/msclients/nns` (51, also over that bucket).
All three taxonomy categories are validated by real fixing passes;
sub-case 2 of "missing in-tree/project header" was refined at batch A
after batch 2 under-specified it, then re-confirmed by batch B's
`overhead/index`/`index.h` re-evaluation (see the taxonomy section and
`claude-history/m2-batch3b-REPORT.md` §4). Gate scope ("subtree-local
is sufficient") now has **four** data points, including
`overhead/cmenu` (statically linked), `atk/basics/x`/`atk/basics/
common` (statically linked AND M1's own former largest-blast-radius
directory), and batch B's `ams/libs/cui` (linked directly into
`messages`'s `amsn.do`, at 528 instances the largest single-batch
volume yet) — recommend settled, see "Gate scope" below; ruling still
wdc's to make explicitly. Two process gaps found and folded into
`rollout-procedure.md`: directories with generated sources
(`Parser()`/bison) need `make depend` before a subtree-local
`install` (pilot, reconfirmed by batch B's `overhead/eli/lib`); files
with 20+ diagnostics in one file need `-ferror-limit=0` on the
fix-surfacing pass, or clang's default cap silently truncates the list
(batch A, reconfirmed by batch B's `ams/libs/cui`). Batch A's runtime
check also surfaced a real, pre-existing `atk/figure` text-rendering
bug — confirmed via controlled revert/rebuild/fresh-restart test to be
unrelated to this M2 work, root-caused and **fixed** 2026-07-24
(`fontdesc.c` LP64 cast bug; see `roadmap.md`). Batch B found
`overhead/eli/lib` has zero live runtime consumers in this build
(`ams/ms`/`SNAP_ENV` disabled, `rdemo` unreferenced) — a structural
finding, not a testing gap; flagged in `claude-history/
m2-batch3b-REPORT.md` §10, not acted on (out of M2's scope).

## What the flag does

`-Wno-implicit-function-declaration` is in the tree-wide
`COMPILERFLAGS` (`src/config/darwin/system.mcr`), fully suppressing
the diagnostic — calling an undeclared function compiles silently.
Removing the suppression (Apple clang promotes
`-Wimplicit-function-declaration` to an error by default, but write
`-Werror=implicit-function-declaration` explicitly rather than rely
on that default) makes every call to a function with no visible
declaration a compile error. This is LP64 bug class 1 in
`sonnet-playbook.md`'s list — an undeclared function returning a
pointer gets an implicit `int` return type, truncating the pointer on
LP64 — closed structurally, tree-wide, the same way M1 closed
variants 2/3/5.

## Mechanical difference from M1 — read this before flagging anything

M1's `CLASSFLAGS` override *composes*: `CLASSFLAGS = $(CLASSINCLUDES)
-pi` adds one flag on top of the computed default. `COMPILERFLAGS`
has no such composed default — `system.mcr` sets it directly to a
fixed four-flag string. **A directory override must restate the
whole flag set**, not just append the new one, or the directory
silently loses `-std=gnu89` and the other three suppressions:

```
/* M2 ANSI sweep: catch implicit function declarations (revival/doc/roadmap.md, M2) */
COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
```

Keep this line's other three flags byte-identical to `system.mcr`'s
current value — if a future change edits the global line, every
per-directory override needs the same edit, and a mismatched override
is easy to miss. (Consider, when proposing this rollout to wdc: a
lighter option is one shared macro, e.g. `M2_COMPILERFLAGS`, defined
once in a shared `.mcr` fragment and referenced from each flagged
Imakefile — avoids the restate-in-full risk entirely. Not implemented
here; a design choice worth deciding before the first flag lands.)
Verify the same way as M1's `CLASSFLAGS`: regenerate the Makefile,
then `grep -n COMPILERFLAGS Makefile` and confirm the override line
appears after `system.mcr`'s default (last assignment wins).

## Gate scope — recommended settled, three data points

`rollout-procedure.md`'s "Gate definition" leaves the actual scope to
each milestone. M1's typed `.ih` casts are installed to
`build/include` and affect *every consumer* of a flagged directory's
classes — that's why M1 always gates on a full tree-wide `make Clean;
make dependInstall`. M2's flag only changes diagnostic severity for
`.c` files compiled *inside* the flagged directory; the resulting
`.o` and any installed headers are otherwise unchanged, so a flagged
directory's fallout is local to itself. **Three data points now,
recommend treating this as settled (2026-07-24, not yet an explicit
ruling)**: the `atk/eq` pilot, the
rollout-point-2 batch, and rollout-point-3 batch A all ran the full
tree-wide gate anyway and it caught nothing the corrected
subtree-local builds hadn't already shown. Batch A is the strongest
point — it includes `atk/basics/x`/`atk/basics/common`, statically
linked into every ATK app via `runapp` (confirmed via `nm -g
build/bin/runapp`), AND `atk/basics/common` is M1's own former
largest-blast-radius directory (41 classes, 2,351 external `.ih`
includes) — and still showed zero fallout beyond its own subtree.
Future rollout points use subtree-local `make clean && make depend &&
make -k install` (the `depend` step is required — see
`rollout-procedure.md`) as the per-directory gate, with a full
tree-wide gate at coarser checkpoints (session end, or every few
directories) rather than after every single one.

**Update (2026-07-24, batch B):** fourth data point, again zero
fallout beyond the subtree-local builds — this time including
`ams/libs/cui`, linked into `atkams/messages/lib`'s `amsn.do` (the GUI
`messages` app's actual mail-store logic), at 528 instances the
largest single batch by volume so far. Four-for-four, including both
of the two heaviest/most-widely-consumed directories examined to
date. Strengthens the case for relaxing to per-checkpoint tree-wide
gates rather than per-batch; still pending wdc's explicit sign-off
before acting on it.

## Census (2026-07-24, `make -k`, tree-wide, not yet acted on)

2,353 instances across 396 files. First attempt (without `-k`)
undercounted at 367/74 — `rollout-procedure.md`'s "Logging" section
now carries this as the general warning: always census/gate with
`-k`, not just at M1's multi-file fallout collection.

**A second, per-file variant of the same under-counting risk** (found
rollout point 3 batch A, 2026-07-24): clang's default
`-ferror-limit=20` silently truncates diagnostics once one file hits
20 errors — the file still fails to compile, but only the first 20
show. `atk/basics/x/xim.c` had 20 real instances; the 20th
(`mb_SetGetDefault`) was invisible until the first 19 were fixed and
the file rebuilt. For directories with an unusually large single
file, pass `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the
fix-surfacing pass (not needed on the follow-up determinism passes,
which should already be error-free).

By directory (`.` = `src/`), heaviest first:

| Directory | Instances |
|---|---|
| `ams/libs/ms` | 892 |
| `contrib/zip/lib` | 141 |
| `atkams/messages/lib` | 140 |
| `atk/table` | 113 |
| `overhead/mail/lib` | 112 |
| `overhead/util/lib` | 74 |
| `overhead/mail/metamail/metamail` | 70 |
| `atk/text` | 50 |
| `atk/rofftext` | 47 |
| `atk/basics/lib` | 46 |
| `ams/msclients/cui` | 45 |
| ~60 more directories | 1–42 each |

`ams/libs/ms` alone is ~38% of the tree-wide total. Sampling its
instances shows most trace to a handful of project-local functions
(`NonfatalBizarreError`, `dbg_fclose`/`dbg_close`/`dbg_closedir`,
`CloseMSDir`, `FreeMessage`, ...) missing across ~100 files — the
`dbg_*` names are the `fdplumb` wrapper family (see
`project_fdplumb_include_order_abi` — the include-order ABI hazard
already found there). **Do not treat `ams/libs/ms` as a naive
pilot or early batch target** — whoever takes it should read the
fdplumb history first (`fdplumb-REPORT.md`, `porting-assessment.md`
§18) given the file's prior form with this exact function family.
Likely a single missing/misordered `#include` fixes most of its 892
at once, but confirm that empirically rather than assuming.

## Fallout taxonomy (seeded from census text, unvalidated)

Order of trust, same rule as M1: **the real implementation is truth;
a missing declaration is what's being fixed.** Never invent a
signature — find the real one (a header, or the defining file) before
writing any declaration by hand.

- **Missing standard-library header (majority category).** `strlen`,
  `strcmp`, `strncmp`, `strcpy`, `malloc`, `free`, `exit`, `realloc`,
  `atoi`, `qsort`, `unlink`, `getpid`, `printf`, `index` account for
  most of the 2,353. Fix: add the standard header the function lives
  in (`string.h`, `stdlib.h`, `unistd.h`, `stdio.h`) — match what
  sibling files in the same directory already include, don't
  introduce a new include style. Zero semantic risk: these are
  well-known signatures, not guesses.
- **Missing in-tree/project header — three sub-cases, all validated
  by rollout point 2 (2026-07-24).** Project-local functions defined
  in another module. Check which sub-case applies before picking a
  fix:
  1. **A header exists, declares it, just isn't included.** Fix: add
     the `#include`, matching how other callers in the tree already
     reach it (`atk/adew/mkcon.c`'s `getprofileswitch`, declared in
     `overhead/util/hdrs/util.h`, already reached the same way by
     `atk/basics/common/environ.c`).
  2. **A header declares sibling functions from the same file, but
     not this one.** Two shapes, distinguished by rollout point 3
     batch A (2026-07-24) after point 2 under-specified this as a
     single rule — **check both the header's scope and the specific
     consuming file's own local-extern precedent for *this* function
     family before choosing**:
     - *Header lives outside the flagged directory* (or is a broad,
       many-callers utility header like `util.h`/`index.h`), **and**
       the flagged directory already has its own local-`extern` habit
       for this exact gap: don't extend the outside header (the M2
       hard-stop against editing files outside the flagged directory
       still applies) — match the existing local precedent instead.
       Point 2's two instances: `overhead/cmenu`'s `cmdraw.h` doesn't
       declare `FlipButton`/`DrawMenus`/`SelectionPtrToNum` (defined
       in sibling files `cmdraw.c`/`cmmanip.c`), but `cmdraw.c` itself
       already had a hand-written `extern int SelectionPtrToNum();`
       — followed that. `atk/help/src`'s `helpdb.c` already includes
       `overhead/index/index.h` (11 sibling `index_*`/`recordset_*`
       declared, 4 not) — added local `extern`s instead of editing
       `index.h`. Both: the outside header looks stale/incomplete as
       its own finding, worth housekeeping whenever *that* directory
       is flagged, not an M2 fix.
     - *Header lives IN the flagged directory itself*, is small and
       clearly a dedicated single-file public API (not a broad
       utility header), and there's no *competing* local-extern habit
       already established in the specific consuming file for *this*
       specific function family: extend the header directly instead.
       Point 3 batch A's two instances: `atk/basics/x/menubar.h`
       (already declares 12 sibling `mb_*` functions in the same
       style, in-directory) extended with 5 more; `atk/raster/cmd/
       dispbox.h` (already declares 7 sibling `DisplayBox*`/
       highlight functions) extended with 5 more, closing 11 of that
       directory's 37 instances in one edit since all 4 consuming
       files already included it. Neither directory had a competing
       local-extern habit for these specific function names (batch A
       also confirmed no such conflict by checking).
  3. **No header anywhere in the tree declares it, full stop.** Same
     as the pilot's `eqview_Format`, just cross-directory instead of
     same-directory: `osi_GetTimes` (`overhead/util/lib/times.c`,
     called from `atk/frame`), `FoldedEQ` (`overhead/util/lib/
     foldedeq.c`, called from `atk/lookz` — see below, this also
     resolves the "possible typo" entry). Fix: local `extern`,
     sourced from the real definition, same as the pilot's approach.
  In all three sub-cases: confirm via `grep` before deciding — never
  assume "no header found" without checking, and never hand-write a
  declaration's types from inference when a real definition or
  existing header can be read directly.
- **Same-file/same-directory forward reference.** A function defined
  later in the same file, or in a sibling file in the same directory
  with no shared header (pilot: `atk/eq/eqv.c`'s `eqview_Format`,
  same-directory; rollout point 2 added several same-*file* instances
  — `atk/frame/framecmd.c`'s `frame_VisitFilePrompting`/
  `frame_VisitNamedFile`/`frame_WriteFile`, `atk/lookz/tabrulv.c`'s
  `FindClosestTab`, `overhead/fonts/cmd/fdbbdf.c`'s `fontcount`/
  `fontcvt`). Fix: whatever convention the directory/file already
  uses for its other forward-declared functions — a file-local
  prototype block, typically — not a new pattern. Where no local
  precedent exists in a single-file program (`fdbbdf.c`), plain
  untyped K&R style matching the rest of the batch is the default.
- **Possible genuine bug / typo — hard stop, category now EMPTY as of
  rollout point 2.** A called function that doesn't exist anywhere in
  the tree, or is a close misspelling of a real one. The runbook's
  one named candidate, `pref.c:71`'s `FoldedEQ`, **is not a typo** —
  confirmed real, correctly spelled, defined in
  `overhead/util/lib/foldedeq.c`, just undeclared anywhere (see
  "missing in-tree/project header" sub-case 3 above). No other
  candidate turned up across 80 instances/9 directories so far. Still
  empty after batch B's 528 more instances (`claude-history/
  m2-batch3b-REPORT.md` §7) — every unfamiliar name resolved to a
  real, correctly-spelled definition once grepped.
  Before writing any declaration, still `grep` the tree for the exact
  name to confirm it resolves to sub-case 3, not this category — the
  check matters even though the category is currently empty.
- **New sub-shape found by batch B (2026-07-24): a wrapper-family
  header exists but only declares *some* of the family, and the gap
  recurs across directories with no shared local-extern habit between
  them.** `overhead/util/hdrs/fdplumb.h` `#define`s 16 `dbg_*` I/O
  wrapper names but only declares 6 of them — the other 10 (`dbg_close`,
  `dbg_fclose`, `dbg_dup2`, `dbg_pipe`, `dbg_pclose`, `dbg_vfclose`,
  among others) are silently undeclared wherever used. Hit
  independently in 3 of batch B's 5 directories (`overhead/eli/lib`,
  `ams/libs/cui`, `ams/msclients/nns`), none of which had any
  pre-existing local-extern precedent for the gap — unlike the two
  previously-documented sub-case-2 shapes, which both matched an
  established local habit. Fix is still local-extern by default (the
  M2 hard-stop against editing outside headers still applies), but
  "check for existing local precedent first" doesn't fully cover this
  shape since there often isn't one yet. `fdplumb.h` itself is a good
  housekeeping candidate (declare the missing 10 names) whenever a
  session touches `overhead/util/lib`/`overhead/util/hdrs` or a
  file-I/O-heavy directory — not done as part of M2.
- **New sub-shape found by batch B: consumer-supplied callback
  interfaces with *zero* declaring header anywhere** (distinct from
  ordinary sub-case 3, where one real definition exists). `ams/libs/
  cui`'s `ReportError`/`ReportSuccess`/`ChooseFromList`/etc. and
  `overhead/mail/metamail/richmail`'s `controloutput`/`controlputc`:
  each is implemented separately by every program that links the
  library, with no canonical definition and no header, ever. Resolved
  the same way as sub-case 3 (local extern, sourced from any one
  consistent real implementation) since all found implementations
  agreed on the untyped K&R shape — but "which implementation is the
  real one" doesn't have a single answer the way it does for ordinary
  sub-case 3, so flagging it as its own recognizable shape.
- **Caution carried over from tonight's M2 point-0 session
  (`porting-assessment.md` §19):** a hand-written declaration with a
  wrong width/type (e.g. `int` vs `long`) compiles cleanly and fails
  silently on LP64 — exactly the bug class M2 exists to close, so
  introducing a fresh instance of it while fixing an M2 warning would
  be a self-defeating regression. Always source a declaration's exact
  signature from the real implementation or an existing header, never
  by inference from the call site's argument types.

## Ordering

M1's consumer-count survey doesn't apply here — M2's fallout doesn't
cross directories the way M1's typed casts did (see "Gate scope"
above), so there's no equivalent asymmetry to sequence around.
Proposed order, pending wdc's sign-off:

1. **Pilot: `atk/eq`** (10 instances, 4 files: `eq.c`, `symbols.c`,
   `eqv.c`, `eqvcmds.c`) — **done 2026-07-24**, see "Status" above.
2. Small/leaf directories: `atk/frame` (8), `atk/adew` (7), `atk/
   value` (6), `atk/lookz` (7), `atk/help/src` (7), `atk/extensions`
   (9), `overhead/cmenu` (18), `overhead/fonts/cmd` (8) — **done
   2026-07-24 as one batch**, see "Status" above and
   `claude-history/m2-batch2-REPORT.md`. (Corrected 2026-07-24 before
   the batch ran: an earlier draft of this list also included
   `atk/figure` here, using a stale count from the first,
   undercounted non-`-k` census pass — its real count is 23, mid-size;
   moved to bucket 3 below, where it already also appeared.)
3. Mid-size (~20–70), split into two batches of 5:
   - Batch A — `atk/basics/x`, `atk/basics/common`, `atk/figure`,
     `atk/syntax/tlex`, `atk/raster/cmd` — **done 2026-07-24**, see
     "Status" above and `claude-history/m2-batch3a-REPORT.md`.
   - Batch B — `overhead/eli/lib`, `ams/libs/cui`,
     `ams/msclients/nns`, `overhead/mail/metamail/richmail`,
     `overhead/index` — **done 2026-07-24** (528 instances, well past
     this bucket's stale "mid-size" estimate — `ams/libs/cui` alone
     was 350), see "Status" above and
     `claude-history/m2-batch3b-REPORT.md`.
4. Large, dedicated-session territory (~70–140): `overhead/util/lib`,
   `overhead/mail/metamail/metamail`, `atk/text`, `atk/rofftext`,
   `atk/table`, `overhead/mail/lib`, `atkams/messages/lib`,
   `contrib/zip/lib`.
5. **`ams/libs/ms` (892) last**, its own dedicated session, whoever
   takes it briefed on the fdplumb history first.

## Hard stops specific to M2

(General reporting rule in `rollout-procedure.md`'s "Hard-stop
reporting.") In addition to that:

- A called function doesn't exist anywhere in the tree (see taxonomy).
- The real declaration's type disagrees with how callers already use
  the value (a live bug, not conversion noise — same rule as M1).
- A fix appears to require editing generated files, `classpp` itself,
  or anything outside the flagged directory's own `.c`/`.h` files
  (importing a header is fine; editing one outside the directory is
  not, without escalating first).
- `ams/libs/ms` findings that look like they touch the `fdplumb`
  include-order hazard directly — escalate rather than guessing at
  include order.
