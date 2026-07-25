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
volume yet). **Ruled 2026-07-24 (wdc)**: checkpoint schedule for the
rest of M2 rather than a tree-wide gate every session — see "Gate
scope" below for the full per-directory schedule. Two process gaps
found and folded into
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
m2-batch3b-REPORT.md` §10, not acted on (out of M2's scope). Bucket
4's first directory, `overhead/util/lib`, closed 2026-07-24: 74/74
instances fixed (first bucket-4 directory where the real count
matched the stale estimate exactly), subtree-local gate only per the
ruling — see `claude-history/m2-utillib-REPORT.md`. Found a new
methodology blind spot, folded into `rollout-procedure.md`'s
"Logging" section: `malloc`/`realloc`/`free` are clang builtins, so a
call with zero declaration anywhere doesn't trigger
`-Werror=implicit-function-declaration` at all — invisible to the
`-k` census (caught via a `svcconf.c` near-miss before it became a
bug). Also the first live test of `rollout-procedure.md`'s new
"Command style" guidance (separate tool calls instead of chained/
looped shell commands): zero permission prompts across ~90 tool
calls in that session, strongly positive — though the dedicated Grep/
Glob tools the guidance recommends weren't actually available to that
delegated session (only `Read` and individual `Bash grep` calls were;
both worked with zero prompts), so the guidance's tool-substitution
wording was softened to not assume Grep/Glob availability. Bucket 4's
second directory, `overhead/mail/metamail/metamail`, closed
2026-07-24: 338/338 instances fixed across 7 files — far past the
stale estimate of 70, mostly `metamail.c`/`mailto.c`'s own large
same-file forward-reference populations (both large single-file K&R
programs with dozens of helpers defined after `main()`) — see
`claude-history/m2-metamail-REPORT.md`. Structurally the strongest
gate-scope data point yet: this directory builds only
`ProgramTarget`s, no library at all, so cross-directory fallout is
structurally impossible, not just empirically absent. Runtime check
found metamail crashes at runtime (root cause identified: `SIGTTOU`
in `ExecuteMailcapEntry`'s `ioctl` call, 1980s BSD terminal
job-control code, unrelated to this fix) and `mailto`/`splitmail`
fail on a missing `/usr/lib/sendmail` — both confirmed pre-existing
and out of scope (see `roadmap.md`'s "Objective: HTML mail
rendering"), not blockers to committing the M2 fix itself. Bucket 4's
third directory, `atk/text`, closed 2026-07-24: 50 census-visible
instances (matched the stale estimate exactly — but see below) plus
106 more from the malloc-family blind spot across 18 files, real
total 156 across 23 files — see `claude-history/m2-text-REPORT.md`.
Two findings folded into `rollout-procedure.md`: (1) the malloc-family
blind spot can *dominate* a directory's fallout with zero prior
symptom, not just cause a near-miss — the existing "grep files
already getting an edit" guidance is upgraded to "grep every file
unconditionally"; (2) the `cd`-splitting command-style fix from the
metamail session doesn't hold in every session type — a delegated
background session's harness resets cwd between Bash calls entirely
(confirmed by that session hitting an immediate "No rule to make
target" failure) — `make -C <dir>` is now the portable default,
`cd`-then-bare-command only where cwd persistence is confirmed to
hold. Cleanest gate-scope data point yet: every function touched
confirmed statically linked directly into `runapp`. User-verified, no
regressions; `indexpro.c`'s pre-existing `gets()` call triggers a
macOS runtime deprecation warning, unrelated, logged as a minor
finding. Bucket 4's fourth directory, `atk/rofftext`, closed
2026-07-24: 54 census-visible instances (vs. stale estimate 47) plus
50 more from the now-mandatory malloc-blind-spot sweep, real total
104 across 9 files — see `claude-history/m2-rofftext-REPORT.md`.
Second data point (after `atk/text`) where the blind spot is
comparable in scale to the census-visible count, and the first where
two individual files had zero census-visible errors yet real
malloc-family fallout — the unconditional per-file sweep is doing
real work, not just covering a hypothetical. New taxonomy sub-shape,
folded into "Fallout taxonomy" below: a same-file forward reference
whose function already has a real declaration sitting in an
in-directory header (the defining file just never included its own
header) should get that `#include`, not a hand-written duplicate
extern. Structurally the cleanest gate-scope case yet: no
`LibraryTarget` at all, confirmed zero symbols in `runapp`.
User-verified (`help`, standalone `rofftext` converter), no
regressions. Bucket 4's fifth directory, `atk/table`, closed
2026-07-24: 154 census-visible instances (vs. stale estimate 113) plus
32 more from the malloc-blind-spot sweep, real total 186 across 9 of
10 files (`print.c` needed zero fixes of any kind — the first
bucket-4 file to come through completely clean) — see
`claude-history/m2-table-REPORT.md`. Two findings folded in: (1) a
malloc-sweep methodology gap — `table.c`'s own `myrealloc()` wrapper
produces a false positive under a naive `grep "realloc("` substring
match; the sweep pattern is now specified as word-boundary-anchored
and space-before-paren-tolerant (`grep -nE "\bmalloc *\(|\bfree
*\(|\brealloc *\(|\bcalloc *\("`) in `rollout-procedure.md`'s "Logging"
section; (2) a new taxonomy sub-shape — see "Fallout taxonomy" below.
Structurally the same `DynamicMultiObject`-only shape as `atk/rofftext`
(no `LibraryTarget`), confirmed via `nm -g build/bin/runapp` showing
zero symbols from either `.do`. User-verified (menu-inserted spreadsheet,
formula evaluation including a deliberate malformed-formula error case,
save/reload round-trip, direct `table` binary launch, `.table`
file-type auto-load), no regressions — one non-regression observation
(launching the bare `table` binary with no file opens a plain-text `ez`
buffer with no visible way to insert a table; opening a `.table` file
directly, or inserting via the **Media > Spreadsheet** menu, both work
correctly) noted but not investigated further, consistent with `ez`'s
own bare-launch default behavior. Bucket 4's sixth directory,
`overhead/mail/lib`, closed 2026-07-25: 112 census-visible instances
(matching the stale estimate exactly) plus 12 more from the
malloc-blind-spot sweep, real total 124 across 22 of 33 files — see
`claude-history/m2-mail-lib-REPORT.md`. Structurally the
widest-fan-out directory examined yet: a real `LibraryTarget`
(`libmail.a`) consumed by ~25 external directories. Extra `nm -g`
verification (per the prompt's explicit ask given the fan-out size)
rebuilt and checked 3 structurally distinct real consumers
(`atkams/messages/lib`'s `amsn.do`, `ams/msclients/cui`'s `cuin`, and
`overhead/mail/cmd`'s standalone tools) — zero symbol leakage in any,
confirming the subtree-local-gate ruling holds even at this fan-out
scale. Caught and fixed its own self-inflicted bug immediately (a
literal `*/` inside an explanatory comment's prose prematurely closed
a C comment; caught by the next rebuild). No new taxonomy category;
malloc-sweep findings split cleanly into three shapes (genuinely
invisible blind spot; a stale wrong-typed extern that became a hard
conflict only once `<stdlib.h>` was added for an unrelated reason;
completely dead/unused stale externs) — worth keeping distinct in
future reports. User-verified (`arpadate` standalone date-format
check, `cuin` startup/config-load, fresh `messages` process folder-list
load), no regressions. Bucket 4's seventh directory, `atkams/
messages/lib`, closed 2026-07-25: 212 census-visible instances (vs.
stale estimate 140) plus 124 more from the malloc-blind-spot sweep,
real total 336 across 18 of 23 files — see `claude-history/
m2-messageslib-REPORT.md`. This is the `messages` GUI app's actual
backend (~15 separate `DynamicObject`/`DynamicMultiObject` classes,
100% dynamically loaded, confirmed zero symbols in `runapp`), so per
the gate-scope ruling it required **both** the subtree-local gate and
the full tree-wide gate (`make Clean && make dependInstall`,
233,099-line log, exactly the same 4 pre-existing baseline errors
every prior session has documented, zero new ones, confirmed this
directory's own build span contains none of them). Confirmed
`SNAP_ENV` disabled empirically (`allsys.h`, commented out; `amss.c`/
`amss.do` never enter the build graph). Concrete LP64 finding: 6
functions (`CUI_DisambiguateDir`, `CUI_GetHeaders`, `CUI_Initialize`,
`MS_GetDirInfo`, `MS_MatchFolderName`, `MS_UnlinkFile`) are
`long`-returning at their real definitions while ~65 sibling functions
in the same two families default to `int` — sourced from two
independent places (the real library definitions and this class's own
already-typed wrapper methods) before declaring. No new taxonomy
category; explicitly checked for and ruled out a recurrence of
`atk/table`'s `AUXMODULE` sub-case (every undeclared function here is
a plain C function, not a class-internal double-underscore method).
One process-management lesson: a `nohup ... &`-wrapped build inside a
backgrounded Bash call caused a false "completed" notification (the
launching shell exited, not the build) — caught via `ps aux`,
corrected with a proper PID-polling wait; future sessions should rely
on the Bash tool's own backgrounding, not an added shell-level `&`.
On the live auto-mode question (see "Command style" note below): no
detectable change in prompt/denial pattern from this session's
vantage point — the agent noted it has no way to distinguish "nothing
changed" from "the human wasn't prompted for allow-listed calls" from
inside a single delegated session. User-verified (fresh `messages`
process with a real IMAP-backed folder list, opening a message,
composing/sending to `wdc@fastmail.com`, folder tree, scrolling,
options panel), no regressions.

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

## Gate scope — ruled 2026-07-24, checkpoint schedule below

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
date.

**Ruling (2026-07-24, wdc):** relax to a checkpoint schedule for the
remainder of M2 (bucket 4 + `ams/libs/ms`) rather than a full
tree-wide gate on every session. Every directory still always gets its
own subtree-local gate (`make clean && make depend && make -k
install`) — that's never skipped. The full tree-wide gate
(`make Clean && make dependInstall`) is skipped for routine bucket-4
sessions and kept for sessions carrying elevated risk:

- **Skip the tree-wide gate** (subtree-local only) for: `overhead/
  util/lib`, `overhead/mail/metamail/metamail`, `atk/text`, `atk/
  rofftext`, `atk/table`, `overhead/mail/lib` — structurally the same
  shape as the four already-proven data points.
- **Keep the tree-wide gate** for: `atkams/messages/lib` (the
  `messages` app's actual backend, not just a linked consumer —
  highest consequence if anything leaks), `contrib/zip/lib` (the
  tree's own history flags this as the highest-defect-density
  directory — see `porting-assessment.md` §17's Xft bug history), and
  `ams/libs/ms` gated **both before and after** its session (~38% of
  the entire M2 census, fdplumb include-order history — see the
  Census section above).
- **Fixed milestone checkpoints regardless of which directories
  preceded them**: end of bucket 4 (before starting `ams/libs/ms`),
  and end of `ams/libs/ms` — which is also the end of M2 entirely, so
  it doubles as the definitive "M2 complete" gate before M3 starts.

Net: roughly 5 of the remaining 9 sessions skip the full rebuild, the
other 4 keep it, bracketing the riskiest/most consequential remaining
work with full verification while cutting the routine cost elsewhere.

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
fdplumb history first: `claude-history/fdplumb-REPORT.md` in full
(the include-order ABI hazard, the fd-name ledger/`dbg_dup2` bug, and
the fdplumb-unrelated preference-blackout investigation are all
there). **Correction (2026-07-24, found by `overhead/util/lib`'s
session):** the `porting-assessment.md` §18 citation this section
used to carry is stale — that section contains zero occurrences of
"fdplumb" and is actually about a different, related bug (the
variadic-caller ABI hazard, `project_variadic_caller_abi_hazard`).
Use `fdplumb-REPORT.md` as the sole background source, not
`porting-assessment.md`. Also: `overhead/util/lib` (where `fdplumb.c`
itself lives) is 6 files, `fdplumb.c` through `fdplumb6.c`, not 4 —
found by the same session; its own report also found a second,
`fdplumb.h`-unrelated gap worth reading before `ams/libs/ms`'s
session: the implementation files' internal `RegisterOpenFile`/
`RegisterCloseFile`/real-I/O-primitive calls are themselves
undeclared same-directory forward references (`m2-utillib-REPORT.md`
§7). Likely a single missing/misordered `#include` fixes most of
`ams/libs/ms`'s 892 at once, but confirm that empirically rather than
assuming.

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
  **Sub-shape found by `atk/rofftext` (2026-07-24)**: check whether
  the function already has a real, complete declaration sitting in an
  in-directory header before writing a fresh local extern — the
  defining file may simply never have included its own header
  (`roffcmds.c` defines the whole `*_cmd` family and declares them all
  in its own `roffcmds.h`, already used correctly by a sibling file,
  but never included the header itself; fix was `#include
  <roffcmds.h>` in `roffcmds.c`, not a hand-written duplicate).
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
- **New taxonomy sub-case found by `atk/table` (2026-07-24): a
  class-internal (double-underscore) method's declaration exists in
  its own generated `.eh`, but only outside an `AUXMODULE` guard,
  making it structurally unreachable from the one file that needs
  it.** Distinct from every "missing header" sub-case above — nothing
  is missing, incomplete, or misspelled. Some class-method macros
  (unusually) call the double-underscore implementation directly
  rather than dispatching through the class's routine table; if the
  real declaration for that implementation sits inside `#ifndef
  AUXMODULE ... #endif` in the generated `.eh` (there to avoid
  re-emitting static method tables in every file compiled into a
  multi-file `.do`), any `AUXMODULE`-consumer file that calls the
  macro is walled off from seeing its own declaration by the same
  header's own design. `atk/table`'s instance: `update.c` (an
  `AUXMODULE` consumer) calls the `spread_WantLimitedHighlighting()`
  macro, which expands to a direct call of
  `spread__WantLimitedHighlighting()`; `spread.eh` declares the latter
  but only inside its `AUXMODULE`-excluded block. Fix: local `extern`,
  same as ordinary sub-case 3, sourced from the real definition — but
  worth a comment noting the `AUXMODULE` mechanics so a future reader
  doesn't mistake it for a plain gap and try to "fix" it by editing the
  generated `.eh`/`.ch` (out of scope, and already correct for their
  own intended purpose). Check for this shape whenever a class method's
  generated macro calls a double-underscore name directly instead of
  through `classname_CLASSPROCEDURES->routines[n]`.
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
4. Large, dedicated-session territory (~70–140). Gate per the "Gate
   scope" ruling above — subtree-local gate always required; tree-wide
   gate only where marked:
   - `overhead/util/lib` — subtree-local gate only. **Done
     2026-07-24**, see "Status" above and `claude-history/
     m2-utillib-REPORT.md`.
   - `overhead/mail/metamail/metamail` — subtree-local gate only.
     **Done 2026-07-24**, see `claude-history/m2-metamail-REPORT.md`.
   - `atk/text` — subtree-local gate only. **Done 2026-07-24**, see
     `claude-history/m2-text-REPORT.md`.
   - `atk/rofftext` — subtree-local gate only. **Done 2026-07-24**,
     see `claude-history/m2-rofftext-REPORT.md`.
   - `atk/table` — subtree-local gate only. **Done 2026-07-24**, see
     `claude-history/m2-table-REPORT.md`.
   - `overhead/mail/lib` — subtree-local gate only. **Done
     2026-07-25**, see `claude-history/m2-mail-lib-REPORT.md`.
   - `atkams/messages/lib` — **tree-wide gate required** (the
     `messages` app's actual backend). **Done 2026-07-25**, see
     `claude-history/m2-messageslib-REPORT.md`.
   - `contrib/zip/lib` — **tree-wide gate required** (tree's
     highest-defect-density directory).
   - After the last bucket-4 directory, regardless of which one it
     was: **tree-wide gate required** (fixed milestone checkpoint,
     before starting `ams/libs/ms`).
5. **`ams/libs/ms` (892) last**, its own dedicated session, whoever
   takes it briefed on the fdplumb history first. **Tree-wide gate
   required both before starting and after finishing** — the second
   of these is also M2's own completion gate, before M3 starts.

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
