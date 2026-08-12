# M2 rollout point 2: small/leaf directory batch

Read `sonnet-playbook.md`, `rollout-procedure.md`, and
`m2-rollout-runbook.md` in full before starting — this prompt assumes
all three, and assumes the `atk/eq` pilot's findings (rollout point
1, see `claude-history/m2-pilot-eq-REPORT.md`): both taxonomy
categories it validated (missing standard header, same-directory
forward reference) held with zero surprises; the one real gap found
was mechanical, not taxonomic — directories with bison/lex-generated
sources need `make depend` before a subtree-local `install`, or the
missing generated header masks real warnings behind a fatal error
(now folded into `rollout-procedure.md`'s step-order text — read it).

This is a **batch**: one session, one gate, per M1 point 10's
batching precedent (`rollout-procedure.md`'s "Session/build rhythm").
Census-first per directory still applies inside the batch.

## Task

Execute the M2 rollout procedure on these 8 directories, each
independently (own Imakefile flag, own local fallout fix), gated
together at the end:

| Directory | Census count (2026-07-24, `-k`) |
|---|---|
| `src/atk/frame` | 8 |
| `src/atk/adew` | 7 |
| `src/atk/value` | 6 |
| `src/atk/lookz` | 7 |
| `src/atk/help/src` | 7 |
| `src/atk/extensions` | 9 |
| `src/overhead/cmenu` | 18 |
| `src/overhead/fonts/cmd` | 8 |

These counts are from the stale census run before the `atk/eq` fix
(reverted afterward — the flag was never left on tree-wide). Treat
them as an expectation, not ground truth: re-derive the real warnings
per directory from your own `make -k` build after flagging it, the
same way the `atk/eq` pilot's actual 10 matched but weren't assumed
in advance.

For each directory, in any order:
1. Flag its `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement, not an appended flag).
2. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
3. `make clean && make depend && make -k install` (the `depend` step
   matters if the directory has any generated sources — check its
   Imakefile for `Parser()`/`LexFile` before assuming it doesn't).
4. Fix fallout per the runbook's taxonomy. Watch for taxonomy
   categories the `eq` pilot didn't exercise — "missing in-tree/
   project header" and "possible genuine bug/typo" are both still
   unvalidated; if you hit either, that's real signal, document it
   clearly in the report even if (especially if) it doesn't match the
   runbook's prediction.
5. Rebuild clean, twice, to confirm determinism.

## Gate (only gate)

Full tree-wide gate: `cd src && make Clean && make dependInstall`
(background it, canonical log path). This is still the second data
point for the "is a lighter subtree-local gate sufficient" question
(`m2-rollout-runbook.md`'s "Gate scope" section) — report explicitly
whether the tree-wide pass found anything beyond what your 8
subtree-local builds already showed. Success = zero real `error:`
lines beyond the 4 known pre-existing ones (the false-positive
recognizer-type warning inside a `-Wdeprecated-non-prototype`
warning, the `ams/msclients/nns` SSLLIB link failure, and
`contrib/zip/utility/ltapp.c`'s two int-conversion errors) — never
the exit code.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from these
directories interactively** — that's for wdc to do, after your report,
from a native terminal. Stop here and report.

## Report

Write `revival/doc/claude-history/m2-batch2-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Per directory: real instance count found (vs. the stale table
  above), each fix with file:line + taxonomy category.
- Whether the tree-wide gate found anything the 8 subtree-local
  builds didn't.
- Any new taxonomy category, or anything that contradicts the
  runbook's current taxonomy/predictions.
- **A list of exact runtime-check commands for wdc to run**, covering
  what these 8 directories actually affect — likely `ez` (frame,
  value, extensions), `help` (help/src), `messages` or general UI
  chrome (frame is windows/menus/scrollbars — check what consumes it),
  and note anything with no obvious runtime fixture (e.g. `overhead/
  fonts/cmd`, `overhead/cmenu` if it's a build tool rather than a
  linked runtime component — check before assuming). This list is
  load-bearing: wdc must run and confirm it before anything from this
  batch gets committed.
- `fossil status` output confirming exactly which files changed, no
  commit made.
