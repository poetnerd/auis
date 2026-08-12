# M2 rollout point 4c: `atk/text`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (minimizing permission-prompt
interruptions, most recently corrected 2026-07-24: split `cd` into
its own call too, not just build steps from each other — a bare `cd
DIR && make -k install ...` two-part chain still prompts every time
even though both halves individually match an allow rule; issue `cd
DIR` alone once, then every subsequent command with no `cd` prefix at
all, relying on the Bash tool's persisted working directory) and the
"Logging" section's note on `malloc`/`realloc`/`free`/`calloc` being
clang builtins that don't trigger the M2 diagnostic even undeclared —
`m2-rollout-runbook.md`, and all six prior rollout reports
(`claude-history/m2-pilot-eq-REPORT.md`, `claude-history/
m2-batch2-REPORT.md`, `claude-history/m2-batch3a-REPORT.md`,
`claude-history/m2-batch3b-REPORT.md`, `claude-history/
m2-utillib-REPORT.md`, `claude-history/m2-metamail-REPORT.md`) before
starting. The taxonomy has three validated categories with sub-cases
plus two batch-B sub-shapes — see the runbook's "Fallout taxonomy"
section in full. "Possible genuine bug/typo" is still empty across
1157 instances/21 directories. Don't assume it stays empty — still
`grep` to confirm before writing any declaration.

Two process lessons from the two prior bucket-4 sessions, both worth
re-reading before you start:
- `m2-utillib-REPORT.md` §6: `malloc`/`realloc`/`free`/`calloc` calls
  with *zero* declaration anywhere don't trigger the M2 diagnostic at
  all (they're clang builtins) — invisible to the `-k` census. Any
  file getting a `<stdlib.h>` edit for an unrelated reason should also
  get a manual grep for bare calls to that family before you move on.
- `m2-metamail-REPORT.md` §5: large single-file K&R programs can have
  same-file forward reference as their *dominant* fallout category,
  not a minor one — don't let smaller directories' pattern (same-file
  forward reference as a handful of instances) set an expectation that
  becomes a blind spot. `atk/text` has several sizeable files; census
  each file's own forward-reference population as thoroughly as the
  standard-library and cross-file ones on the first pass.

This is a single-directory session (bucket 4's third), not a batch.

## Task

1. Check the Imakefile for `Parser()`/`LexFile` before assuming no
   generated-source gap (none found by a quick check, but confirm
   yourself by reading the Imakefile directly, not just trusting this
   note).
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. `cd` into the directory as its own call, then `make clean`, `make
   depend`, `make -k install` each as their own subsequent calls (no
   `cd` prefix on any of them) — per the command-style guidance. Pass
   `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the fix-surfacing pass;
   this directory has ~30 `.c` files and the stale estimate (50) is
   close enough to the last two bucket-4 directories' real counts
   (74 exact, 338 vs. 70 stale) that a large file hitting the
   20-error cap is plausible either way.
5. Fix fallout per the runbook's taxonomy, watching for both process
   lessons above.
6. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — per the gate-scope ruling recorded
2026-07-24 in `m2-rollout-runbook.md`. Do not run the full tree-wide
gate. If something about this directory's actual linkage makes you
doubt that (check via `nm -g`, one target at a time, never
piped/chained/redirected — `m2-utillib-REPORT.md` §12 and
`m2-metamail-REPORT.md` §12 both found piped/redirected `nm` calls get
denied outright, while `cd <tree-root>` [its own call] then a bare
`nm -g build/bin/<target>` goes through cleanly), say so in the report
rather than silently running the tree-wide gate anyway or silently
dropping the concern.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-text-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 50), each fix with
  file:line + taxonomy category/sub-case.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc.** `atk/text` is core ATK
  text-editing/display machinery (used by essentially every text
  inset/view in the tree) — identify actual consumers via `nm -g`
  against `build/bin/runapp` and any `.do` files, not assumed, and
  propose a check that actually exercises this directory's specific
  fixed functions rather than just "open ez."
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  specifically whether splitting `cd` into its own call (new guidance)
  actually eliminated the `cd DIR && make ...` prompts the prior
  session hit.
