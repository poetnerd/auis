# M2 rollout point 3, batch A: mid-size `atk/*` directories

Read `sonnet-playbook.md`, `rollout-procedure.md`, and
`m2-rollout-runbook.md` in full before starting — this prompt assumes
all three, plus the two prior rollout points' reports
(`claude-history/m2-pilot-eq-REPORT.md`,
`claude-history/m2-batch2-REPORT.md`). By now the taxonomy has three
validated categories with sub-cases (missing standard header;
missing in-tree/project header, itself three sub-cases — not
included/stale-incomplete-with-local-precedent/no-header-anywhere;
same-file-or-directory forward reference) and the "possible genuine
bug/typo" category is empty so far across 80 instances. Don't assume
it stays empty — still `grep` to confirm before writing any
declaration.

This is a **batch** (`rollout-procedure.md`'s "Session/build rhythm"):
one session, one gate, census-first per directory.

## Task

Execute the M2 rollout procedure on these 5 directories, each
independently, gated together at the end:

| Directory | Census count (2026-07-24, `-k`, stale — re-derive per directory) |
|---|---|
| `src/atk/basics/x` | 25 |
| `src/atk/basics/common` | 23 |
| `src/atk/figure` | 23 |
| `src/atk/syntax/tlex` | 28 |
| `src/atk/raster/cmd` | 37 |

~136 instances total, roughly double rollout point 2's volume — budget
accordingly. As always, re-derive the real count per directory from
your own `make -k` build; the table above is a stale planning
estimate, not ground truth (rollout point 1 and 2 both matched their
stale counts exactly, but don't assume that holds a third time).

For each directory, in any order:
1. Check its Imakefile for `Parser()`/`LexFile` before assuming no
   generated-source gap — `atk/syntax/tlex` in particular sounds
   lexer-adjacent by name; confirm either way, don't guess from the
   name alone.
2. Flag its `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. `make clean && make depend && make -k install` (the `depend` step
   only matters if step 1 found generated sources, but running it
   unconditionally is harmless and cheap).
5. Fix fallout per the runbook's taxonomy — including its sub-cases.
   `atk/basics/common` is a former M1 rollout point 9 directory (41
   classes, 2,351 external `.ih` includes) — the largest-blast-radius
   directory touched by M1's rollout; note if that history shows up
   in any way here (it shouldn't, since M2's fallout is compile-unit
   local, not header-propagated, per the runbook's "Gate scope"
   section — but flag it if this directory's size breaks that
   assumption).
6. Rebuild clean, twice, to confirm determinism.

## Gate (only gate)

Full tree-wide gate: `cd src && make Clean && make dependInstall`
(background it, canonical log path `~/src/AUIS/andrew-6.4/
dependInstall.log`). The runbook's "Gate scope" section recommends a
lighter subtree-local-only gate is probably sufficient going forward,
based on two prior data points — but that relaxation isn't
wdc-approved yet, so still run the full tree-wide gate this batch too.
Report explicitly whether it found anything beyond the 5 subtree-local
builds (a third data point either way is valuable). Success = zero
real `error:` lines beyond the 4 known pre-existing ones (recognizer-
type false positive inside a `-Wdeprecated-non-prototype` warning,
`ams/msclients/nns` SSLLIB link failure, `contrib/zip/utility/
ltapp.c`'s two int-conversion errors) — never the exit code.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from these
directories interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-batch3a-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Per directory: real instance count found (vs. the stale table
  above), each fix with file:line + taxonomy category/sub-case.
- Whether the tree-wide gate found anything the 5 subtree-local
  builds didn't (third data point for the gate-scope question).
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc**, covering what these 5
  directories actually affect. `atk/figure` is the figure/diagram
  inset (verified historically via `Sherman.Alloc`'s fad/cel/arbiter/
  eq/table insets — check if figure is also in there, or needs its
  own fixture). `atk/basics/x`/`atk/basics/common` are foundational
  (X11 plumbing / the 41-class core: `im`, `view`, `fontdesc`,
  `environ`, `message`, `menulist`, etc.) — exercised by any app
  launch, but check whether anything more targeted is warranted given
  the size. `atk/syntax/tlex` is ctext syntax coloring/indent
  (verified historically via a scratch `.c` file in `ez`). `atk/
  raster/cmd` builds `convertraster` and friends — check for the
  existing byte-diff battery pattern (`test-baselines/raster-*`) as
  an alternative/supplement to a manual GUI check. Confirm statically
  (e.g. `nm -g build/bin/runapp`, checking `build/dlib/atk`) which of
  these are static vs. dynamic vs. standalone-CLI, same as the batch 2
  report did — don't assume.
- `fossil status` output confirming exactly which files changed, no
  commit made.
