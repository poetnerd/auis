# M2 rollout point 4d: `atk/rofftext`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (most recently corrected 2026-07-24 for
the `atk/text` session: the `cd`-splitting trick only works if your
session's Bash tool actually persists working directory across calls,
which is NOT universal — check early with a throwaway `cd`+`pwd` pair;
if it doesn't persist, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's note on the `malloc`/`realloc`/`free`/`calloc`
clang-builtin blind spot (upgraded 2026-07-24 from "check files
already getting an edit" to "grep every file in the directory,
unconditionally" — `atk/text`'s session found 106 invisible instances
across 18 files with zero prior symptom, more than double its
census-visible count, despite that count matching the stale estimate
exactly) — `m2-rollout-runbook.md`, and all seven prior rollout
reports (`claude-history/m2-pilot-eq-REPORT.md`, `claude-history/
m2-batch2-REPORT.md`, `claude-history/m2-batch3a-REPORT.md`,
`claude-history/m2-batch3b-REPORT.md`, `claude-history/
m2-utillib-REPORT.md`, `claude-history/m2-metamail-REPORT.md`,
`claude-history/m2-text-REPORT.md`) before starting. The taxonomy has
three validated categories with sub-cases plus two batch-B sub-shapes
— see the runbook's "Fallout taxonomy" section in full. "Possible
genuine bug/typo" is still empty across 1313 instances/22 directories.
Don't assume it stays empty — still `grep` to confirm before writing
any declaration.

**This directory has a confirmed generated-source gap**: its Imakefile
has `Parser(num,none)` (line 37). Per the pilot's original finding
(`m2-pilot-eq-REPORT.md`) and `rollout-procedure.md`'s "Session/build
rhythm" section, `make depend` must run before `install` on every
subtree-local build here, or the generated header's absence masks
real M2 warnings behind an unrelated fatal error. This is confirmed,
not something to re-derive — but still verify the generated file(s)
actually appear after `depend` before trusting the rest of the build.

This is a single-directory session (bucket 4's fourth), not a batch.
It's also the smallest bucket-4 directory so far (9 `.c` files,
stale estimate 47) — don't let the smaller size relax the
malloc-family sweep discipline; `atk/text` showed directory size
doesn't predict whether this blind spot is present.

## Task

1. Confirm the `Parser(num,none)` generated-source gap by reading the
   Imakefile yourself (don't just trust this prompt's citation),
   identify what file(s) it generates.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them (see the command-style note above for
   which mechanism to use).
5. Fix fallout per the runbook's taxonomy.
6. **Unconditionally sweep every `.c` file in the directory** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, regardless of whether
   the census-visible error count looks complete or matches the stale
   estimate — per the upgraded guidance above.
7. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — per the gate-scope ruling recorded
2026-07-24 in `m2-rollout-runbook.md`. Do not run the full tree-wide
gate. If something about this directory's actual linkage makes you
doubt that (check via `nm -g`, one target per call, never
piped/chained/redirected in the same call — three prior sessions now
found piped/redirected `nm` calls behave inconsistently across session
types, sometimes denied outright; a bare `nm -g build/bin/<target>` by
itself, or `cd <tree-root> && nm -g build/bin/<target>` as a single
call, has reliably gone through), say so in the report rather than
silently running the tree-wide gate anyway or silently dropping the
concern.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-rofftext-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 47), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case.
- Confirmation of the generated-source handling (what `Parser(num,
  none)` produces, and that `depend` was run before `install`).
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc.** `atk/rofftext` is roff/nroff
  text-format conversion — identify actual consumers via `nm -g`
  against `build/bin/runapp` and any `.do` files, not assumed.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied in your session
  type and whether it held up.
