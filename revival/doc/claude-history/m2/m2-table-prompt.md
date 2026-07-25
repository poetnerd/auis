# M2 rollout point 4e: `atk/table`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (most recently corrected 2026-07-24 for
the `atk/text` session: the `cd`-splitting trick only works if your
session's Bash tool actually persists working directory across calls,
which is NOT universal — check early with a throwaway `cd`+`pwd` pair;
if it doesn't persist, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's note on the `malloc`/`realloc`/`free`/`calloc`
clang-builtin blind spot (upgraded 2026-07-24 to "grep every file in
the directory, unconditionally" — this blind spot has now dominated
or matched the census-visible count in two of the last two bucket-4
directories, `atk/text` and `atk/rofftext` — do not assume a small or
clean-looking directory is exempt) — `m2-rollout-runbook.md`, and all
eight prior rollout reports (`claude-history/m2-pilot-eq-REPORT.md`,
`claude-history/m2-batch2-REPORT.md`, `claude-history/
m2-batch3a-REPORT.md`, `claude-history/m2-batch3b-REPORT.md`,
`claude-history/m2-utillib-REPORT.md`, `claude-history/
m2-metamail-REPORT.md`, `claude-history/m2-text-REPORT.md`,
`claude-history/m2-rofftext-REPORT.md`) before starting. The taxonomy
has three validated categories with sub-cases plus several later
sub-shapes — see the runbook's "Fallout taxonomy" section in full,
including the `atk/rofftext`-found sub-shape (prefer `#include`-ing a
file's own already-complete header over a hand-written duplicate
extern, when one exists). "Possible genuine bug/typo" is still empty
across 1417 instances/23 directories. Don't assume it stays empty —
still `grep` to confirm before writing any declaration.

**No generated-source gap in this directory** — its `Imakefile` has no
`Parser()` or other generator directive (confirmed by reading it
directly before writing this prompt). Still run `make depend` before
`install` anyway, per the standard rhythm in `rollout-procedure.md`;
just don't expect a generated-header surprise here.

This is a single-directory session (bucket 4's fifth), not a batch. 10
`.c` files (`eval.c`, `funs.c`, `hit.c`, `keyboard.c`, `menu.c`,
`print.c`, `spread.c`, `tabio.c`, `table.c`, `update.c`), stale
estimate 113 — the largest bucket-4 directory so far except
`overhead/mail/metamail/metamail` (338 real) and roughly on par with
`overhead/mail/lib`'s stale estimate (112), which follows this one.
Given `atk/text` (156 real, 68% over its 50-estimate once the
malloc-blind-spot was counted) and `atk/rofftext` (104 real, 121% over
its 47-estimate), expect the real count here to run well past 113 too
— treat the stale table as a floor, not a target.

**Structural note on this directory's build shape**: unlike `atk/text`
and `atk/rofftext` (no `LibraryTarget`, structurally impossible to leak
fallout elsewhere), `atk/table` builds two `DynamicMultiObject`
targets, `spread.do` and `table.do` — dynamically loaded `.do` objects,
the same shape as `help`/`figure` in `project_runapp_static_link`, NOT
statically linked into `runapp`. Confirm this empirically (`nm -g
build/bin/runapp` should show zero `table`/`spread`-specific symbols)
rather than assuming from this note — the gate-scope ruling depends on
fallout staying local to the directory, and this shape is different
enough from the last two sessions that it's worth verifying, not just
citing.

## Task

1. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
2. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
3. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them (see the command-style note above for
   which mechanism to use).
4. Fix fallout per the runbook's taxonomy.
5. **Unconditionally sweep every `.c` file in the directory** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, regardless of whether
   the census-visible error count looks complete or matches the stale
   estimate — per the upgraded guidance above.
6. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — per the gate-scope ruling recorded
2026-07-24 in `m2-rollout-runbook.md`. Do not run the full tree-wide
gate. If something about this directory's actual linkage makes you
doubt that (check via `nm -g`, one target per call, never
piped/chained/redirected in the same call — prior sessions found
piped/redirected `nm` calls behave inconsistently across session
types, sometimes denied outright; a bare `nm -g build/bin/<target>` by
itself, or `cd <tree-root> && nm -g build/bin/<target>` as a single
call, has reliably gone through), say so in the report rather than
silently running the tree-wide gate anyway or silently dropping the
concern.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-table-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 113), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case.
- Confirmation of the `.do`-dynamic (not static) linkage shape via
  `nm -g`, and which consumer(s) actually load `table.do`/`spread.do`
  at runtime (check `.ezinit`/help-alias/menu wiring if not obvious
  from `nm` alone).
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc.** `atk/table` is the
  spreadsheet/table inset — identify how to invoke it fresh (new
  `ez`/`runapp` process, per the standing rule that `.do` files are
  cached for the life of a process).
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied in your session
  type and whether it held up.
