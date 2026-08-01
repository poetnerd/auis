# M3 Wave 1 batch O1: `overhead/util/lib`

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — the
"Command style" section on minimizing permission-prompt interruptions
matters for an unattended session), `m3-rollout-runbook.md`,
`m3-batches.md`, and `porting-assessment.md` §14 (the `ansify` tool
design and the Delegation ruling) before starting. Also skim
`claude-history/m2/m2-utillib-REPORT.md` — the M2 session on this same
directory — for the fdplumb background below; you do not need the rest
of the M2 runbook, M3 uses a different flag/tool.

This is the first real M3 batch (Wave 1 of 7). The `atk/eq` "pilot" in
the runbook is a already-completed dry-run validation from 2026-07-08,
not a queued action — do not touch `atk/eq`, it is out of scope for
this session.

## What's different about M3 vs the M2 session you can read for context

M2 fixed missing declarations by hand (discovery + judgment per
instance). M3 uses `ansify` (`revival/tools/ansify`), a tool with its
own per-file compile gate and auto-restore-on-failure — mostly "run
it, read its report, fix ordinary fallout" rather than "investigate
and write code." Two finding categories are explicitly NOT yours to
resolve — stop and report instead:

- **DRIFT** — a `.ch`-vs-`.c` argument-count/type disagreement on a
  class method. Real bug, needs a human ruling on which side is stale.
- **Parser bailout you can't resolve from the file's own K&R
  declaration block** — the tool's helper-conversion parser is
  deliberately strict and reports rather than guessing.

## Directory-specific findings (already done, don't redo)

- The `ansify` signature DB is already rebuilt fresh (2026-07-25,
  `build/desc`, 565/566 classes — the one failure,
  `contrib/atkbook/console/disk1.ch`, is a known pre-existing
  unresolvable superclass, not your concern). Do not run
  `ansify --build-db` again unless `build/desc` is missing.
- A tree-wide `ansify --dry-run --dir src` census (safe: pure parse,
  no compiles, no writes — confirm this understanding by rereading
  `ansify`'s `process()` before you rely on it) already ran. For
  `overhead/util/lib` specifically: **83 files, 0 methods, 0
  classprocs, 256 file-local helpers, 0 DRIFT, 0 skipped.**
- **This directory has zero `.ch` files** (`find src/overhead/util/lib
  -maxdepth 1 -name '*.ch'` returns nothing) — confirmed 2026-07-25.
  It is not a class directory. That means:
  - The runbook's step 1 (add `-pe` to `CLASSFLAGS`) and step 2 (force
    `.eh` regen) **do not apply here** — there is nothing for `-pe` to
    affect. Verify this yourself (don't just trust this note — a
    directory's file list can change) but don't waste time adding a
    flag that has no `.ch` to act on.
  - DRIFT is structurally impossible here (it only fires on `__`
    class methods, and there are none) — the dry-run's 0 DRIFT is
    expected, not lucky. If a real run somehow produces DRIFT here,
    that is surprising enough to be its own stop-and-report item.
  - So this session's real content is entirely the file-local-helper
    conversion path plus whatever compile-gate fallout the *real* run
    (which the dry-run doesn't exercise) turns up.

## Important context: fdplumb family lives here

`fdplumb.c`/`fdplumb2.c`/`fdplumb3.c`/`fdplumb4.c` (the `dbg_*` wrapper
family) live in this directory and have real history: a caller-side
ABI hazard from include ordering (`project_fdplumb_include_order_abi`)
and a separate variadic-through-K&R-extern hazard elsewhere in the
tree (bug class 6 in `sonnet-playbook.md`'s LP64 list — a K&R/empty-
parens extern for a variadic function silently corrupts the arm64
calling convention). `ansify` converting a K&R helper declaration to a
full ANSI prototype could plausibly *fix* a latent instance of that
class if one exists in this directory's own helpers — note it in the
report if you see anything like it, but do not go hunting for it
outside what naturally falls out of the conversion.

## Task

1. Confirm no `.ch` files (see above).
2. `ansify --dir src/overhead/util/lib` — **for real, not `--dry-run`
   this time.**
3. Read its own report. Expect numbers close to the dry-run baseline
   above (83 files / 256 helpers / 0 DRIFT / 0 skipped) — if the real
   numbers disagree meaningfully with the dry-run baseline, or if
   *any* DRIFT or skipped/bailout finding appears, **stop and report
   before doing anything else**; don't try to resolve it yourself.
4. Fix any `COMPILE FAILED — restoring original` fallout `ansify`
   reports (it auto-restores on failure, so a failed file reverts to
   K&R — your job is to fix the real cause, e.g. a missing include, a
   type not in scope, the `.ch`-parameter-name-collides-with-a-type-
   token macro-capture issue from Pilot A if it recurs anywhere — then
   re-run `ansify` on that one file). This part is ordinary mechanical
   fallout-fixing, same spirit as M1/M2 sessions — you're expected to
   resolve it, not escalate it, unless it looks like a genuine
   semantic disagreement rather than a missing declaration/include.
5. Subtree-local gate: `make -C
   /Users/wdc/src/AUIS/andrew-6.4/src/overhead/util/lib clean`, then
   `depend`, then `-k install` — as **separate** tool calls, absolute
   paths, not chained with `&&`/`;`, per `rollout-procedure.md`'s
   command-style section. Rebuild clean twice to confirm determinism.
6. Do not run the tree-wide gate — `m3-rollout-runbook.md`'s gate-
   scope argument (the `.eh`-locality guarantee) says subtree-local is
   sufficient for every directory except `atkams/messages/lib` and
   `contrib/zip/lib`, neither of which this is.

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary
interactively — identify this directory's actual consumers via `nm
-g`/Imakefile `LIBS` lines instead, same as prior reports, and give
wdc the exact runtime-check command(s) in your report rather than
running them yourself.** Stop after the subtree gate is green (twice,
for determinism) and say you have stopped.

## Report

Write `m3-o1-utillib-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count vs. the dry-run baseline above, and an
  explanation for any discrepancy.
- Each compile-gate fallout fix: file:line, what was wrong, the fix.
- Confirmation this directory truly has no `.ch` files (or a
  correction if it turns out to).
- Anything relevant to the fdplumb family noted above.
- Exact runtime-check command(s) for wdc, with the consumer(s)
  identified.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- Any new `ansify` fallout pattern not already described in
  `porting-assessment.md` §14 or the M1/M2 taxonomies — this is the
  first real (non-dry-run, non-pilot) M3 session, so a genuinely new
  pattern here is useful signal for sizing the rest of the rollout.
