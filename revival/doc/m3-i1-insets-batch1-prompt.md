# M3 Wave 4, Batch I1: 10 inset-adjacent directories — ansify + `-pe`/`.eh` rollout

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting. Also read `m3-rollout-runbook.md` in full, in particular:
the "Current standing per-batch checklist" section (all 8 active
items — you must run every one, per directory), and the "Session
structure going forward" section, which defines the two-gate shape
this batch uses. Skim the O1-T1 findings entries below that section
for the fallout taxonomy you'll be classifying against — the more of
that vocabulary you recognize on sight, the less you'll need to
treat as novel. Also read `porting-assessment.md`'s §14/§17 (the
`InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing
and its two documented exceptions) and `m3-batches.md`'s Wave 4
section for scope confirmation.

**This is a routine batch (not one of the four flagged-risky ones —
T1, I2, AMS1, C1 — that keep full orchestrator pre-diagnosis).** Per
the amendment, the pre-diagnosis legwork is yours to do, not the
orchestrator's. This prompt does not pre-diagnose anything for you.

## Scope

10 directories, 70 files, in this order (matches `m3-batches.md`'s
I1 entry and is a reasonable dependency order — `raster/lib` before
its two consumers `raster/scan`/`raster/convert`):

1. `atk/image` (22)
2. `atk/srctext` (20 — judgment call, grouped here as a
   content-display inset rather than core text infra)
3. `atk/raster/lib` (7)
4. `atk/layout` (6)
5. `atk/hyplink` (4)
6. `atk/org` (3 — judgment call, outline/tree navigation, grouped as
   inset-adjacent rather than basics)
7. `atk/bush` (3)
8. `atk/raster/scan` (2)
9. `atk/fad` (2)
10. `atk/raster/convert` (1)

Like every M3 batch, this does both halves: `ansify` (K&R→ANSI) and
the `-pe`/`.eh` Export rollout (`CLASSFLAGS = $(CLASSINCLUDES) -pe`
in each directory's Imakefile, force-regenerate `.eh`, gate).

## Gate 0 — pre-diagnosis and classification (do this first, for all 10 directories, then STOP)

For each of the 10 directories, in the order above:

1. `find <dir> -maxdepth 1 -name '*.ch'` (standing check 1) — note
   directories with zero `.ch` files; DRIFT is structurally
   impossible there, you can skip the rest of the per-directory
   checklist for it but still run the real `ansify` pass on it later.
2. `ansify --dry-run --dir src/atk/<dir>` and record every DRIFT/skip
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
   `.o` files before moving to the next directory — same technique
   the orchestrator used for T1's pre-diagnosis. Don't leave `-pe`
   live between directories.
5. Anything that does not cleanly match an existing taxonomy entry —
   a genuinely new DRIFT/skip shape, a `.ch`-vs-real-usage
   disagreement, a caller/callee argument-count mismatch, anything
   that looks like a real ~35-year-old bug rather than conversion
   noise — flag clearly as **UNCLASSIFIED, needs orchestrator ruling**
   and do not attempt to fix it yourself. Per `rollout-procedure.md`'s
   Delegation section, retype/signature rulings and hard-stop
   adjudication stay top-level.

Write the report (`revival/doc/claude-history/m3-i1-insets-batch1-REPORT.md`,
per `sonnet-playbook.md`'s format) with a per-directory classification
table, then **STOP at Gate 0**. Do not run the real `ansify --dir`
pass, do not add `-pe` permanently anywhere, do not touch any `.ch`/
`.c` file. Say clearly that you have stopped at Gate 0 and are
waiting for the orchestrator's ruling on any UNCLASSIFIED items
(if none, say that explicitly too — you still stop and wait either
way).

## Gate 1 — real run (only after the orchestrator resumes you with a ruling)

Once resumed: for each of the 10 directories, in the same order,
apply Step order per `rollout-procedure.md`: real `ansify --dir`,
`CLASSFLAGS = $(CLASSINCLUDES) -pe` added for real, force-regenerate
`.eh`, fix any fallout (including anything the orchestrator ruled on),
subtree-local gate — `make -C <absolute-path> clean`, then `depend`,
then `-k install`, separate calls, absolute path, twice for
determinism, per directory. Builds stay strictly serial (one
directory's gate clean before starting the next). **No tree-wide gate
in this batch** — I1 and I2 are both Wave 4, and the wave-end
tree-wide gate happens once, after I2 closes the wave, matching how
T1 (alone in Wave 3) was the only batch that had to run its own
wave-end gate.

Stop after all 10 directories gate clean, twice each. **Do NOT
commit. Do NOT run any AUIS GUI/terminal binary interactively** — the
orchestrator will present runtime-check suggestions to wdc separately
after independently re-verifying your work.

## Report

Update `m3-i1-insets-batch1-REPORT.md` (same file, append rather than
rewrite the Gate 0 section) with:
- Gate 1 confirmation: real `ansify --dir` results per directory vs.
  what Gate 0 predicted (flag any drift from the prediction).
- Any new DRIFT/skip/compile-fallout finding not covered at Gate 0,
  and how you resolved it (or, if genuinely unresolved, clearly
  flagged for the orchestrator).
- Per-file `ansify` conversion counts (methods/classprocs/helpers)
  per directory.
- Gate results (twice) per directory.
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do` to find live consumers first; never
  launch GUI apps from the session; no saves against unversioned
  fixtures).
- Anything that surprised you or didn't match this prompt's
  expectations.
