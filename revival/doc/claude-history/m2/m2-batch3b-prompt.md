# M2 rollout point 3, batch B: mid-size `ams/*`/`overhead/*` directories

Read `sonnet-playbook.md`, `rollout-procedure.md`, and
`m2-rollout-runbook.md` in full before starting — this prompt assumes
all three, plus all prior rollout points' reports
(`claude-history/m2-pilot-eq-REPORT.md`, `claude-history/
m2-batch2-REPORT.md`, `claude-history/m2-batch3a-REPORT.md`). By now
the taxonomy has three validated categories with sub-cases (missing
standard header; missing in-tree/project header — itself three
sub-cases, see the runbook's "Fallout taxonomy" section in full,
including batch A's refinement of sub-case 2 into two shapes
depending on where the header lives and whether the consuming file
already has a competing local-extern habit; same-file-or-directory
forward reference) and the "possible genuine bug/typo" category is
still empty across 217 instances/14 directories. Don't assume it
stays empty — still `grep` to confirm before writing any declaration.

This is a **batch** (`rollout-procedure.md`'s "Session/build rhythm"):
one session, one gate, census-first per directory.

## Task

Execute the M2 rollout procedure on these 5 directories, each
independently, gated together at the end:

| Directory |
|---|
| `src/overhead/eli/lib` |
| `src/ams/libs/cui` |
| `src/ams/msclients/nns` |
| `src/overhead/mail/metamail/richmail` |
| `src/overhead/index` |

No reliable stale per-directory count exists for this batch — the
runbook's census table only lists the ~10 heaviest directories by
name; these 5 are folded into its "~60 more directories, 1–42 each"
line. Derive the real count for each directory yourself from your own
`make -k` build; don't guess from the table.

**Note on `overhead/index`**: `atk/help/src/helpdb.c` (fixed in
rollout point 2) already includes `overhead/index/index.h` and added
local `extern`s for 4 undeclared `index_*`/`recordset_*` functions
rather than editing that header — see the runbook's taxonomy sub-case
2 for why (header outside the flagged directory at the time, with a
competing local-extern habit). Now that `overhead/index` itself is
the flagged directory, re-evaluate `index.h` on its own terms: check
whether *it* has its own instances of the same gap, and whether
extending `index.h` directly is correct now that the directory being
fixed IS where the header lives (taxonomy sub-case 2's second shape,
not the first) — don't assume rollout point 2's file-outside-the-
directory reasoning still applies here.

For each directory, in any order:
1. Check its Imakefile for `Parser()`/`LexFile` before assuming no
   generated-source gap.
2. Flag its `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. `make clean && make depend && make -k install` (pass
   `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the fix-surfacing pass if
   any single file looks unusually large — see the runbook's Census
   section on the `-ferror-limit=20` per-file truncation risk).
5. Fix fallout per the runbook's taxonomy — including its sub-cases.
6. Rebuild clean, twice, to confirm determinism.

## Gate (only gate)

Full tree-wide gate: `cd src && make Clean && make dependInstall`
(background it, canonical log path `~/src/AUIS/andrew-6.4/
dependInstall.log`). Three prior data points (pilot, batch 2, batch
3A — including both a statically-linked, tree-wide-consumed directory
and M1's own former largest-blast-radius directory) all found nothing
beyond their subtree-local builds, so this is likely the last batch
where the full tree-wide gate is required before that relaxation gets
an explicit ruling from wdc — run it in full this batch too, and
report explicitly whether it found anything beyond the 5 subtree-local
builds (a fourth data point either way is valuable).

Success = zero real `error:` lines beyond the 4 known pre-existing
ones (recognizer-type false positive inside a
`-Wdeprecated-non-prototype` warning, `ams/msclients/nns`'s own SSLLIB
link failure — note `ams/msclients/nns` is one of this batch's 5
target directories, so confirm whether your fix work touches that
same failure or is unrelated to it, `contrib/zip/utility/ltapp.c`'s
two int-conversion errors) — never the exit code.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from these
directories interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-batch3b-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Per directory: real instance count found, each fix with file:line +
  taxonomy category/sub-case.
- Whether the tree-wide gate found anything the 5 subtree-local builds
  didn't (fourth data point for the gate-scope question).
- Whether `ams/msclients/nns`'s pre-existing SSLLIB link failure
  (listed as a known pre-existing error above) is affected by, related
  to, or independent of your M2 fix work there.
- The `overhead/index`/`index.h` re-evaluation called out above —
  what you found and which taxonomy sub-case actually applied.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc**, covering what these 5
  directories actually affect. `ams/libs/cui`/`ams/msclients/nns` are
  messaging-adjacent (CUI delivery / NNS name service client) —
  identify what actually exercises them (which app, which config)
  rather than assuming `messages` alone covers it. `overhead/mail/
  metamail/richmail` and `overhead/index` — same, identify the actual
  consumer before proposing a check. Confirm statically (`nm -g
  build/bin/runapp`, `build/dlib/atk`, `build/bin`) which of these are
  static vs. dynamic vs. standalone-CLI, same as prior batch reports.
- `fossil status` output confirming exactly which files changed, no
  commit made.
