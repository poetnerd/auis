# M2 pilot: `atk/eq` — `-Werror=implicit-function-declaration`

Read `sonnet-playbook.md`, `rollout-procedure.md`, and
`m2-rollout-runbook.md` in full before starting — this prompt assumes
all three. This is the first M2 rollout point ever executed; the
runbook's taxonomy is a prior from a classification census, not yet
proven by a real fixing pass. Your job is as much to validate/correct
the runbook as to fix `atk/eq`. One gate.

## Task

Execute the M2 rollout procedure (`m2-rollout-runbook.md`) on
`src/atk/eq` only. A tree-wide `make -k` census (2026-07-24,
`~/src/AUIS/andrew-6.4/dependInstall.log` reflects a *different*,
already-restored build — do not trust its current contents, re-run
your own) found 10 `-Wimplicit-function-declaration` instances there,
across `eq.c`, `symbols.c`, `eqv.c`, `eqvcmds.c`. Expect roughly:
missing `<string.h>` for `strlen`/`strcmp`/`strncmp` in three of the
four files, and one same-directory forward-reference
(`eqv.c`'s `eqview_Format`, apparently defined later in `atk/eq`'s
own sources — confirm where, and what the directory's existing
convention is for forward declarations, before adding one).
Treat these as a starting hypothesis, not ground truth — the real
warnings when you build may differ from this stale summary.

## Gate 1 (only gate)

1. Flag `src/atk/eq/Imakefile` per the runbook's exact
   `COMPILERFLAGS` line.
2. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`
   — override must win).
3. `make clean && make -k install` in `src/atk/eq`. Fix fallout per
   the runbook's taxonomy — and **write down whatever the taxonomy
   got wrong or missed**, that's real signal for revising the
   runbook.
4. Gate: your call which scope to use (subtree-local vs. full
   `make Clean; make dependInstall`) — the runbook flags this as an
   unruled relaxation of the M1 gate discipline. Pick the safer
   option (full tree-wide gate) for this first-ever M2 session so we
   get a real data point on whether the lighter gate would actually
   have caught everything; note in your report which errors, if any,
   only showed up in the tree-wide pass vs. the subtree-local one.
5. Do NOT commit. Do NOT run the AUIS GUI. Stop here and report.

## Report, in addition to the standard `sonnet-playbook.md` format

- For each of the 10 (or however many you actually find) instances:
  file:line, the missing declaration, and which taxonomy category it
  fell into (or didn't).
- Whether the subtree-local gate would have been sufficient, i.e.
  did the tree-wide gate surface anything the subtree-local build
  didn't already show.
- Any taxonomy category the runbook is missing, or got wrong.
- Whether `eq`'s existing forward-declaration convention (if any) is
  something later M2 directories should expect to see reused, or was
  a one-off.
