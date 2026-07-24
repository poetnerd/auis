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

**Status (2026-07-24):** rollout points 1 (`atk/eq` pilot) and 2 (8-
directory small/leaf batch) both closed their gates the same day.
80/80 combined predicted instances confirmed across 9 directories, no
volume surprises anywhere. All three taxonomy categories below are
now validated by real fixing passes, not just the census-seeded prior
they started as — see the taxonomy section for what changed. Gate
scope ("subtree-local is sufficient") now has two data points,
including one statically-linked, tree-wide-consumed directory
(`overhead/cmenu`) with zero fallout beyond itself — see "Gate scope"
below, recommended settled pending wdc's sign-off. One process gap
found at the pilot and folded into `rollout-procedure.md`: directories
with generated sources (`Parser()`/bison, presumably `LexFile` too)
need `make depend` before a subtree-local `install`, or the missing
generated header masks real fallout behind a fatal error.

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

## Gate scope — likely lighter than M1's, needs a ruling

`rollout-procedure.md`'s "Gate definition" leaves the actual scope to
each milestone. M1's typed `.ih` casts are installed to
`build/include` and affect *every consumer* of a flagged directory's
classes — that's why M1 always gates on a full tree-wide `make Clean;
make dependInstall`. M2's flag only changes diagnostic severity for
`.c` files compiled *inside* the flagged directory; the resulting
`.o` and any installed headers are otherwise unchanged, so a flagged
directory's fallout is local to itself. Corollary: a subtree-local
`make clean && make depend && make -k install` (the `depend` step is
required — see `rollout-procedure.md`) is plausibly a sufficient
per-directory gate, with a full tree-wide gate only at coarser
checkpoints (session end, or every few directories) rather than after
every single one. **Two data points now, recommend settled pending
wdc's sign-off**: the `atk/eq` pilot and the rollout-point-2 batch
(2026-07-24) both ran the full tree-wide gate anyway and it caught
nothing the corrected subtree-local builds hadn't already shown. The
batch is the stronger point — `overhead/cmenu` is statically linked
into every ATK app via `runapp` (confirmed via `nm -g build/bin/
runapp`), not a zero-consumer leaf like the pilot, and still showed
zero fallout beyond its own subtree. Future rollout points can use
subtree-local `make clean && make depend && make -k install` as the
per-directory gate, with a full tree-wide gate at coarser checkpoints
(session end, or every few directories) rather than after every
single one — unless a future directory's fallout pattern gives reason
to revisit.

## Census (2026-07-24, `make -k`, tree-wide, not yet acted on)

2,353 instances across 396 files. First attempt (without `-k`)
undercounted at 367/74 — `rollout-procedure.md`'s "Logging" section
now carries this as the general warning: always census/gate with
`-k`, not just at M1's multi-file fallout collection.

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
  2. **A header exists in the flagged directory, declares sibling
     functions from the same file, but not this one — and the
     flagged directory already has its own local-`extern` habit for
     that exact gap.** Don't extend the header from outside its own
     directory (the M2 hard-stop against editing files outside the
     flagged directory still applies) — match the existing local
     precedent instead. Two confirmed instances: `overhead/cmenu`'s
     `cmdraw.h` doesn't declare `FlipButton`/`DrawMenus`/
     `SelectionPtrToNum` (all defined in sibling files `cmdraw.c`/
     `cmmanip.c`), but `cmdraw.c` itself already had a hand-written
     `extern int SelectionPtrToNum();` rather than adding it to the
     header — followed that. `atk/help/src`'s `helpdb.c` already
     includes `overhead/index/index.h`, which declares 11 sibling
     `index_*`/`recordset_*` functions but not `index_Close`/
     `index_Enumerate`/`index_GetData`/`recordset_Free` — added local
     `extern`s in `helpdb.c` instead of editing `index.h`. Both cases:
     the outside header looks stale/incomplete as its own finding,
     worth a housekeeping pass whenever *that* directory is flagged,
     but not an M2 fix.
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
  candidate turned up across 80 instances/9 directories so far.
  Before writing any declaration, still `grep` the tree for the exact
  name to confirm it resolves to sub-case 3, not this category — the
  check matters even though the category is currently empty.
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
3. Mid-size (~20–70): `atk/basics/x`, `atk/basics/common`, `atk/
   figure`, `atk/syntax/tlex`, `atk/raster/cmd`, `overhead/eli/lib`,
   `ams/libs/cui`, `ams/msclients/nns`, `overhead/mail/metamail/
   richmail`, `overhead/index`.
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
