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

**Status (2026-07-24):** pilot (`atk/eq`, rollout point 1) closed its
gate the same day. 10/10 predicted instances confirmed, both
predicted categories hit (missing standard header, same-directory
forward reference), gate green tree-wide and subtree-local. One
process gap found and folded into `rollout-procedure.md`: directories
with generated sources (`Parser()`/bison, presumably `LexFile` too)
need `make depend` before a subtree-local `install`, or the missing
generated header masks real fallout behind a fatal error. The
"missing in-tree/project header" and "possible genuine bug" taxonomy
categories below are still unexercised — one pilot, one small/leaf
directory, all standard-library or single-precedent fallout. Treat
those two categories as a prior until a directory that hits them
closes its own gate.

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
every single one. **One data point so far, not yet a ruling**: the
`atk/eq` pilot ran the full tree-wide gate anyway (first-ever M2
session) and it caught nothing the corrected subtree-local build
hadn't already shown — supports the relaxation, but from one small
leaf directory with no other directories relinking against its
objects. Still run the full tree-wide gate for at least the next
directory or two, ideally one with more external consumers, before
treating this as settled.

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
- **Missing in-tree/project header.** Project-local functions defined
  in another module (`errprintf`, `ReportError`, `dbg_fclose` family,
  `mb_Destroy`, `index_Close`, `recordset_Free`, ...). Fix: find the
  header that already declares it (grep the tree for its prototype)
  and add that `#include`, matching how other callers in the tree
  already reach it. If no header declares it, that itself is a
  finding — escalate rather than hand-writing an extern from
  inference.
- **Same-file/same-directory forward reference.** A function defined
  later in the same file, or in a sibling file in the same directory
  with no shared header (M1 pilot found this shape too, in a
  different guise — `atk/eq/eqv.c`'s `eqview_Format`, defined further
  down in the same directory's sources but called before any
  declaration is visible). Fix: whatever convention the directory
  already uses for its other forward-declared functions — a
  file-local prototype block, typically — not a new pattern.
- **Possible genuine bug / typo — hard stop.** A called function that
  doesn't exist anywhere in the tree, or the name is a close
  misspelling of a real one (unconfirmed candidate from the census:
  `pref.c:71`'s `FoldedEQ`). Before writing any declaration, `grep`
  the tree for the exact name; if it isn't defined anywhere, this is
  a real bug (dead code path, or a caller expecting a function that
  was renamed/removed), not an M2 mechanical fix — report, don't
  paper over it with a plausible-looking extern.
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
2. Small/leaf directories next (roughly 2–20 instances each): `atk/
   figure`, `atk/frame`, `atk/adew`, `atk/value`, `atk/lookz`,
   `atk/help/src`, `atk/extensions`, `overhead/cmenu`, `overhead/
   fonts/cmd`, etc. — batchable once the pilot proves the taxonomy,
   same batching precedent as M1 point 10.
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
