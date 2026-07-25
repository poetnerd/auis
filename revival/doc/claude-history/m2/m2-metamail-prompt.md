# M2 rollout point 4b: `overhead/mail/metamail/metamail`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (minimizing permission-prompt
interruptions) and the "Logging" section's note on `malloc`/`realloc`/
`free`/`calloc` being clang builtins that don't trigger the M2
diagnostic even undeclared (found in the prior session, easy to miss)
— `m2-rollout-runbook.md`, and all five prior rollout reports
(`claude-history/m2-pilot-eq-REPORT.md`, `claude-history/
m2-batch2-REPORT.md`, `claude-history/m2-batch3a-REPORT.md`,
`claude-history/m2-batch3b-REPORT.md`, `claude-history/
m2-utillib-REPORT.md`) before starting. The taxonomy has three
validated categories with sub-cases plus two batch-B sub-shapes — see
the runbook's "Fallout taxonomy" section in full. "Possible genuine
bug/typo" is still empty across 819 instances/20 directories. Don't
assume it stays empty — still `grep` to confirm before writing any
declaration.

On command style specifically: the prior session
(`m2-utillib-REPORT.md` §11) got zero permission prompts across ~90
tool calls by issuing build steps as separate calls and using one
`grep`/`Read` call per file/pattern instead of shell loops — but its
two piped/chained `nm` calls were denied outright (not prompted).
Follow the same discipline: no `&&`/`;` chains, no `for`/`while`
loops, no pipes into another command, for anything beyond the
already-allowed build-cycle verbs.

This is a single-directory session (bucket 4's second), not a batch.

## Task

1. Check the Imakefile for `Parser()`/`LexFile` before assuming no
   generated-source gap.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. `make clean && make depend && make -k install` as **separate**
   calls, per the command-style guidance. Pass
   `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the fix-surfacing pass —
   the stale census estimate (70) is close enough to the prior
   directory's real count matching its estimate that a large single
   file hitting the 20-error cap is plausible.
5. Fix fallout per the runbook's taxonomy. Remember the clang-builtin
   blind spot for `malloc`/`realloc`/`free`/`calloc`: whenever a file
   gets a `<stdlib.h>` edit for an unrelated reason, also grep it for
   bare calls to that family before moving on — don't trust "not in
   the error list" as proof.
6. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — per the gate-scope ruling recorded
2026-07-24 in `m2-rollout-runbook.md`. Do not run the full tree-wide
gate. If something about this directory's actual linkage makes you
doubt that (check via `nm -g`, one target at a time, never piped/
chained), say so in the report rather than silently running the
tree-wide gate anyway or silently dropping the concern.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-metamail-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 70), each fix with
  file:line + taxonomy category/sub-case.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc.** This directory builds
  `metamail`, `mailto`, `mmencode`, `splitmail` — identify which are
  standalone CLI tools (same pattern as batch B's `richtext`/
  `richtoatk`) vs. invoked by something else, via `nm -g`/Imakefile
  `LIBS` lines, not assumed.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this
  session — any new denial/prompt shapes beyond what
  `m2-utillib-REPORT.md` §11 already found.
