# M3 Wave 4, Batch I2: `atk/eq`, `atk/figure`, `atk/chart`, `atk/table`, `atk/rofftext`, `atk/raster/cmd` — ansify + `-pe`/`.eh` rollout, closes Wave 4

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting (same hard rules as every M3 batch: no fossil commits ever,
stop at the gate, write `m3-i2-insets-batch2-session.diff` in the
tree root and `m3-i2-insets-batch2-REPORT.md` in
`revival/doc/claude-history/`, command style for an unattended
session). Also read `m3-rollout-runbook.md`'s "Current standing
per-batch checklist" section in full and its I1 findings entry (the
fallout taxonomy is now mature — image/srctext/layout/etc. all found
the same handful of shapes; expect I2 to look similar plus the two
new items below). This is one of the four flagged-risky batches (per
the 2026-07-30 amendment) that keep full orchestrator pre-diagnosis
instead of a delegate-side Gate 0 — the orchestrator has already done
that legwork; apply the findings below and go straight to the real
run. Do not re-derive them.

## Scope

6 directories, in this order (matches `m3-batches.md`'s I2 entry;
closes Wave 4, so this batch also runs the wave-end tree-wide gate):

1. `atk/eq` (6 nominal — see generated-file note below)
2. `atk/figure` (17) — **known history**: M1 Pilot B (2026-07-09)
   found and fixed three real `.ch`-vs-implementation bugs here
   (typeless `MoveHandle` declarations, the `ToolName`/`Instantiate`
   "rock" idiom, and a ~35-year-old `Build(action, v, ...)` vs.
   `Build(v, action, ...)` parameter transposition — see
   `porting-assessment.md`'s "Pilot B findings" section). Confirmed
   during pre-diagnosis that all three fixes are still in place and
   correct in the current tree (`figobj.ch`'s `Build` reads
   `(struct figview *v, enum view_MouseAction action, ...)`,
   `EnumerateSelection` is `void *rock`, `ToolName`/`ToolModify`/
   `Instantiate` correctly keep `long rock` per the counter-example
   ruling). Not expected to resurface, but given the history, give
   `atk/figure` more scrutiny than the other five directories if
   anything looks off.
3. `atk/chart` (13)
4. `atk/table` (10)
5. `atk/rofftext` (9 nominal — see generated-file note below)
6. `atk/raster/cmd` (8)

Like every M3 batch, this does both halves: `ansify` (K&R→ANSI) and
the `-pe`/`.eh` Export rollout (`CLASSFLAGS = $(CLASSINCLUDES) -pe`
in each directory's Imakefile, force-regenerate `.eh`, gate).

## Pre-diagnosis already done

### 1. Generated-source exclusion — `atk/eq/eqparse.c` and `atk/rofftext/num.c` are NOT real source, do not run `ansify` on them

Both directories use the `Parser(classname,bisonargs)` Imake macro
(`src/config/andrew.rls:285`), same mechanism M2's own pilot
(`atk/eq`) and its `atk/rofftext` batch already found and documented
in full (`claude-history/m2/m2-pilot-eq-REPORT.md` §4,
`m2-rofftext-REPORT.md` §6). Confirmed directly this session:

- `fossil finfo src/atk/eq/eqparse.c` and `fossil finfo
  src/atk/rofftext/num.c` both return "no history for file" — neither
  is fossil-tracked. They (and their `.h` counterparts) are
  bison-generated from the real, fossil-tracked grammar sources
  (`eqparse.gra`, `num.gra`) via `mkparser`, wired only into each
  directory's `depend::` target, and deleted by `clean::`.
- **Do not run `ansify` against `eqparse.c` or `num.c`** — any K&R
  patterns in them come from the bison/mkparser toolchain, not from
  AUIS source, and hand-editing a regenerated build artifact is
  wasted work that a subsequent `make clean && make depend` silently
  discards anyway. If `ansify --dir` picks them up as candidates,
  exclude them from the real run (skip those two files specifically;
  everything else in the two directories proceeds normally).
- Per the M2 runbook-gap finding, the *subtree-local* gate recipe
  needs an explicit `make depend` between `clean` and `install` for
  these two directories specifically (`make clean && make depend &&
  make -k install`, not `make clean && make -k install`) — otherwise
  `eqparse.h`/`num.h` don't get regenerated and downstream files that
  `#include` them fail with a misleading fatal error before ever
  reaching real fallout. This is the standing gate recipe with one
  extra step, not a special procedure. Confirmed via mtime check that
  `make depend` actually regenerates both pairs.
- This is also almost certainly why `m3-batches.md`'s file counts for
  these two directories (6 for `eq`, 9 for `rofftext`) are lower than
  a naive `.c`+`.ch` directory listing — the convention has already
  been excluding the generated files. Don't be surprised if your real
  `ansify --dir` candidate count differs slightly from the nominal
  count above; trust the tool's own accounting over the batch-doc
  number.

### 2. Standing checklist run once already, findings below — re-confirm quickly as part of your own pass, but nothing further to fix before the real run

- **`.ch` presence** (check 1): all 6 directories have `.ch` files
  (counts: eq 2, figure 17, chart 12, table 2, rofftext 4, raster/cmd
  4) — DRIFT is possible everywhere, no directory skips this.
- **Predefined-macro typo grep** (check 2): clean. The only hits are
  legitimate double-underscore `__STDC__`/`__cplusplus` bison
  boilerplate inside `eqparse.c`/`num.c` themselves (out of scope per
  §1 above) — no single-underscore typos anywhere in real source.
- **Empty-parens lifecycle-method grep** (check 3,
  `__(InitializeClass|InitializeObject|FinalizeObject)\(\s*\)` in
  `.c` files): zero hits across all 6 directories. Clean.
- **Restated-lifecycle-param `.ch` check** (check 5): looked at every
  `InitializeClass`/`InitializeObject`/`FinalizeObject` declaration
  in all 6 directories' `.ch` files. One pattern worth flagging so it
  isn't mistaken for something new: `atk/chart` has six classes
  (`chartapp.ch`, `chartdot.ch`, `chartlin.ch`, `chartcsn.ch`,
  `charthst.ch`, `chartstk.ch`) that declare bare `FinalizeObject();`
  with **zero** restated params — this is the exact shape the B2
  `classpp FinalizeObject` fix (2026-07-30, centrally fixed, see
  runbook's retired-checks list) now handles correctly regardless of
  what's restated; it is not the broken "restates both implicit
  params" shape B3 found in `unknownv.ch`/`suiteev.ch`. No fix
  needed. `raster/cmd`'s lifecycle declarations also have old
  commented-out `/* struct classhdr *ClassID, */` remnants ahead of
  the real param — harmless comments, don't miscount them as a
  restated param.
- **Stranded old-style empty-parens forward declarations vs. narrow
  by-value params** (check 8): present in every directory except
  `raster/cmd` (1 hit there) — expect this standing T1/I1-class
  fallout at real-conversion time in all 6 directories, same
  triage as I1 (cross-check each hit's real definition's parameter
  types once `ansify` converts it; only the narrow-by-value cases
  — `char`/`short`/`unsigned char` — actually break). Rough hit
  counts from a pre-diagnosis grep, so you know what order of
  magnitude to expect: `eq` 4, `figure` 8, `chart` 34 (concentrated
  in `chart.c`/`chartobj.c`, the two biggest files), `table` ~30
  (spread across `hit.c`/`funs.c`/`eval.c`/`update.c`/`table.c`),
  `rofftext` 16, `raster/cmd` 1.
- **Installed-header grep for converted non-static helpers** (check
  4) and the concurrent-commit merge check (check 6): not
  pre-run (they depend on knowing the exact set of converted
  helpers, which only exists after `ansify` actually runs) — run
  these yourself during the real pass, same as every prior batch.
- The milestone-agnostic checks in `rollout-procedure.md` (liveness
  census, anchored `malloc`/`free`/`realloc`/`calloc` grep) — also
  yours to run during the real pass.

## Task

1. For each of the 6 directories, in the order above: real
   `ansify --dir src/atk/<dir>` (excluding `eqparse.c`/`num.c` per
   §1 for `eq`/`rofftext`). Investigate any DRIFT/skip finding the
   same way every prior M3 batch has — check `.ch` vs `.c` by hand,
   consult `porting-assessment.md`/`m3-rollout-runbook.md` for a
   matching pattern first, cite the specific finding you're matching
   against.
2. Run checklist items 4 and 6 (installed-header grep, concurrent-
   commit check) plus the milestone-agnostic checks per directory,
   as noted above.
3. Add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to each directory's
   Imakefile (same placement convention as every other `-pe`'d
   directory — directly after the existing `COMPILERFLAGS` line).
4. Force-regenerate all `.eh` files per directory with the rebuilt
   classpp (remove existing `.eh`, `make <name>.eh` per class,
   explicit target list, same mechanic as every prior `-pe` batch).
5. Investigate and fix any new compile fallout from turning `-pe` on
   for real — same triage process as every prior batch, including
   the stranded-forward-decl class flagged above.
6. Subtree-local gate per directory: `make -C <absolute-path> clean`,
   then `depend`, then `-k install` — separate calls, absolute path,
   not chained, **twice** for determinism. For `eq` and `rofftext`
   specifically, `depend` is load-bearing (§1) — do not skip it or
   collapse it into a combined command.
7. Builds stay strictly serial (one directory's gate clean, twice,
   before starting the next).
8. After all 6 directories gate clean individually: this batch closes
   Wave 4, so also run the wave-end tree-wide gate — `make -C
   <tree-root> dependInstall` (or the milestone-agnostic tree-wide
   recipe `rollout-procedure.md` specifies), twice for determinism.
   Compare against the Wave-1/2 retroactive tree-wide baseline
   already documented in the runbook (clean except the two
   pre-existing, already-queued `contrib/zip/utility/ltapp.c` errors
   and the already-fixed `nns`/`tlscon`/OpenSSL link issue) — flag
   any *new* tree-wide error as a hard stop for the orchestrator, do
   not attempt to fix anything outside this batch's 6 directories
   yourself.

## Gate

Stop after all 6 directories gate clean (twice each) and the wave-end
tree-wide gate is clean (twice). **Do NOT commit. Do NOT run any AUIS
GUI/terminal binary interactively** — the orchestrator will present
runtime-check suggestions to wdc separately after independently
re-verifying your work.

## Report

Write `revival/doc/claude-history/m3-i2-insets-batch2-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Per-directory `ansify --dir` results vs. the pre-diagnosis above
  (flag any drift from what was expected, especially in `atk/figure`
  given its history).
- Confirmation that `eqparse.c`/`num.c` were excluded from the real
  `ansify` run and that `make depend` was run before `install` for
  `eq`/`rofftext`'s gates.
- Any new DRIFT/skip/compile-fallout finding not covered above, and
  how you resolved it (or, if genuinely unresolved, clearly flagged
  for the orchestrator — retype/signature rulings and hard-stop
  adjudication stay top-level per `rollout-procedure.md`'s Delegation
  section).
- Per-file `ansify` conversion counts (methods/classprocs/helpers)
  per directory.
- Gate results (twice) per directory, plus the wave-end tree-wide
  gate result (twice).
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do` to find live consumers first; never
  launch GUI apps from the session; no saves against unversioned
  fixtures).
- Anything that surprised you or didn't match this prompt's
  expectations.
