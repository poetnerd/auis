# M3 Wave 5, Batch A1: 9 app/leaf directories — ansify + `-pe`/`.eh` rollout

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting. Also read `m3-rollout-runbook.md` in full, in particular:
the "Current standing per-batch checklist" section (all 8 active
items — you must run every one, per directory), and the "Session
structure going forward" section, which defines the two-gate shape
this batch uses. Skim the O1-I2 findings entries below that section
for the fallout taxonomy you'll be classifying against — the more of
that vocabulary you recognize on sight, the less you'll need to
treat as novel. Also read `porting-assessment.md`'s §14/§17 (the
`InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing
and its two documented exceptions) and `m3-batches.md`'s Wave 5
section for scope confirmation.

**This is a routine batch (not one of the four flagged-risky ones —
T1, I2, AMS1, C1 — that keep full orchestrator pre-diagnosis).** Per
the amendment, the pre-diagnosis legwork is yours to do, not the
orchestrator's. This prompt does not pre-diagnose anything for you.

## Scope

9 directories, 29 files, in this order (matches `m3-batches.md`'s A1
entry; all are small/leaf — no directory here is a dependency of
another in this batch):

1. `ams/msclients/nns` (10)
2. `atk/typescript` (5)
3. `atk/help/src` (5)
4. `ams/msclients/cui` (4)
5. `atk/help/maint` (1)
6. `atkams/messages/cmd` (1)
7. `ams/msclients/imapsync` (1)
8. `atk/ez` (1)
9. `doc/mkbrowse` (1)

Like every M3 batch, this does both halves: `ansify` (K&R→ANSI) and
the `-pe`/`.eh` Export rollout (`CLASSFLAGS = $(CLASSINCLUDES) -pe`
in each directory's Imakefile, force-regenerate `.eh`, gate) —
**but only where a directory actually has `.ch` files.** Several of
these (`atk/help/maint`, `atkams/messages/cmd`, `ams/msclients/
imapsync`, `atk/ez`, `doc/mkbrowse`) are plain command/doc-tool
directories and may turn out to have zero classes — confirm with
step 1 of the Gate 0 checklist below rather than assuming either way.

Note the AMS1 history flag from the runbook does NOT apply here:
`ams/libs/ms` (Wave 6, AMS1) is a separate, much larger directory
with its own `fdplumb` include-order history. `ams/msclients/nns`/
`cui`/`imapsync` in this batch are the mail-client-side consumers,
not that library itself — still worth a normal look at any `fdplumb`-
wrapped calls per the standing checklist, but not a flagged-risky
directory.

## Gate 0 — pre-diagnosis and classification (do this first, for all 9 directories, then STOP)

For each of the 9 directories, in the order above:

1. `find <dir> -maxdepth 1 -name '*.ch'` (standing check 1) — note
   directories with zero `.ch` files; DRIFT and the `-pe`/`.eh` half
   are both structurally inapplicable there (skip straight to the
   plain `ansify` pass for those, no class-export rollout to do).
2. `ansify --dry-run --dir <dir>` and record every DRIFT/skip
   finding.
3. Run the rest of the standing checklist (items 2-8) against the
   directory: predefined-macro typo grep, empty-parens
   lifecycle-method grep, installed-header grep for converted
   non-static helpers, restated-lifecycle-param `.ch` check (read the
   real `.c` param count by hand, don't just count the `.ch` text),
   concurrent-commit merge check (not expected to trigger, check
   `fossil status` anyway), the milestone-agnostic checks in
   `rollout-procedure.md` (liveness census, anchored `malloc`/`free`/
   `realloc`/`calloc` grep), and the stranded-forward-declaration grep
   (`grep -nE 'static\s+\w[\w ]*\s+\w+\(\);' <dir>/*.c`, cross-check
   any hit's parameter types against its real definition).
4. Classify every finding from steps 2-3 against the documented
   taxonomy — cite the specific runbook finding (by batch letter,
   e.g. "matches B3 finding 3") or `porting-assessment.md` section it
   matches. Where a classification genuinely needs a compile check to
   resolve (not just a `.ch`/`.c` read-by-hand), you may temporarily
   add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to that directory's
   Imakefile, `make Makefile`, force-generate the relevant `.eh`
   file(s) with the real `class` binary, inspect/compile-check, then
   `fossil revert` the Imakefile and delete the scratch `.eh`/`.ih`/
   `.o` files before moving to the next directory. Don't leave `-pe`
   live between directories.
5. Anything that does not cleanly match an existing taxonomy entry —
   a genuinely new DRIFT/skip shape, a `.ch`-vs-real-usage
   disagreement, a caller/callee argument-count mismatch, anything
   that looks like a real ~35-year-old bug rather than conversion
   noise — flag clearly as **UNCLASSIFIED, needs orchestrator ruling**
   and do not attempt to fix it yourself. Per `rollout-procedure.md`'s
   Delegation section, retype/signature rulings and hard-stop
   adjudication stay top-level.

Write the report (`revival/doc/claude-history/m3-a1-apps-REPORT.md`,
per `sonnet-playbook.md`'s format) with a per-directory classification
table, then **STOP at Gate 0**. Do not run the real `ansify --dir`
pass, do not add `-pe` permanently anywhere, do not touch any `.ch`/
`.c` file. Say clearly that you have stopped at Gate 0 and are
waiting for the orchestrator's ruling on any UNCLASSIFIED items
(if none, say that explicitly too — you still stop and wait either
way).

## Gate 1 — real run (only after the orchestrator resumes you with a ruling)

Once resumed: for each of the 9 directories, in the same order,
apply Step order per `rollout-procedure.md`: real `ansify --dir`,
`CLASSFLAGS = $(CLASSINCLUDES) -pe` added for real where the
directory has `.ch` files, force-regenerate `.eh`, fix any fallout
(including anything the orchestrator ruled on), subtree-local gate —
`make -C <absolute-path> clean`, then `depend`, then `-k install`,
separate calls, absolute path, twice for determinism, per directory.
Builds stay strictly serial (one directory's gate clean before
starting the next).

**This batch is the only batch in Wave 5, so it also closes the
wave — run the tree-wide gate once at the end, after all 9
directories are individually clean twice**, same pattern T1 used as
the only batch in Wave 3. Log the tree-wide gate to
`andrew-6.4/dependInstall.log`, not the scratchpad, per the standing
build-invocation convention.

Stop after the tree-wide gate is clean. **Do NOT commit. Do NOT run
any AUIS GUI/terminal binary interactively** — the orchestrator will
present runtime-check suggestions to wdc separately after
independently re-verifying your work.

## Report

Update `m3-a1-apps-REPORT.md` (same file, append rather than rewrite
the Gate 0 section) with:
- Gate 1 confirmation: real `ansify --dir` results per directory vs.
  what Gate 0 predicted (flag any drift from the prediction).
- Any new DRIFT/skip/compile-fallout finding not covered at Gate 0,
  and how you resolved it (or, if genuinely unresolved, clearly
  flagged for the orchestrator).
- Per-file `ansify` conversion counts (methods/classprocs/helpers)
  per directory; note explicitly any directory that had zero `.ch`
  files and so skipped the `-pe`/`.eh` half entirely.
- Gate results (twice per directory, plus the one wave-end tree-wide
  gate).
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do` to find live consumers first; never
  launch GUI apps from the session; no saves against unversioned
  fixtures). Note that some of these directories (`doc/mkbrowse`,
  `atk/help/maint`) may turn out to be build-time/doc tooling with no
  live GUI-visible consumer at all — say so plainly if `nm -g` and a
  `runapp`/`.do` search come up empty, per `m3-batches.md`'s note
  that some batches drop to a report-only check.
- Anything that surprised you or didn't match this prompt's
  expectations.
