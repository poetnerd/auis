# M3 Wave 6, Batch AMS2: `atkams/messages/lib` + 3 AMS support libs — ansify + `-pe`/`.eh` rollout — closes Wave 6

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting. Also read `m3-rollout-runbook.md` in full, in particular:
the "Current standing per-batch checklist" section (**all 9 active
items** — item 8 was broadened and item 9 was newly added after
AMS1's Gate 1, both apply here) and the "Session structure going
forward" section, which defines the two-gate shape this batch uses.
Read the AMS1 findings entry in the runbook (directly above this
batch) for the fallout taxonomy and the new parser-gap finding —
recognize that vocabulary rather than treating it as novel if you see
it again. Also read `porting-assessment.md`'s §14/§17 (the
`InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing)
and `m3-batches.md`'s Wave 6 section for scope confirmation.

**This is a routine batch (not one of the four flagged-risky ones —
T1, I2, AMS1, C1 — that keep full orchestrator pre-diagnosis).** Per
the amendment, the pre-diagnosis legwork is yours to do, not the
orchestrator's. This prompt does not pre-diagnose anything for you,
but does record some context the orchestrator already confirmed so
you don't have to re-derive it.

## Scope

4 directories, 34 files, in this order (matches `m3-batches.md`'s
AMS2 entry):

1. `atkams/messages/lib` (23) — the `messages` GUI app's actual
   backend; has 17 `.ch` files, so this is a real `-pe`/`.eh` rollout
   directory, not just a plain `ansify` pass.
2. `ams/libs/shr` (7) — `libmsshr.a`, plain C, 0 `.ch` files.
3. `ams/libs/cui` (3) — `libcui.a`, plain C, 0 `.ch` files.
4. `ams/libs/nosnap` (1) — `libcuin.a`, plain C, 0 `.ch` files.

Only `atkams/messages/lib` does the `-pe`/`.eh` half; the other three
are plain `ansify` passes (confirm this yourself with step 1 of the
Gate 0 checklist rather than trusting this note blindly, per the
standing checklist's own point 1).

**Do not confuse this with AMS1's `ams/msclients/cui`** — that was a
different directory (the mail-client-side consumer, converted in
Wave 6 AMS1 along with `ams/libs/ms`). This batch's directory 3 is
`ams/libs/cui`, the CUI library implementation itself.

## Context already confirmed by the orchestrator (saves you the legwork)

- `atkams/messages/lib/Imakefile` and `ams/libs/cui/Imakefile` **already
  carry** the M2-era `COMPILERFLAGS = -std=gnu89 -Wno-implicit-int
  -Werror=implicit-function-declaration
  -Wno-incompatible-function-pointer-types -Wno-return-type` guard.
  `ams/libs/shr/Imakefile` and `ams/libs/nosnap/Imakefile` do not yet
  — add it for real only if Gate 1 fallout actually needs it (implicit
  declaration errors), same as every prior batch's practice, not
  unconditionally.
- `ams/libs/cui/cuilib.c` was already noted in AMS1's writeup as a
  ready-made cross-check for CUI/MS function return types (it declares
  the large majority of the same undeclared-function set AMS1's
  `ams/msclients/cui` Task 2 had to resolve). If you hit similar
  undeclared-function fallout here, check `cuilib.c`'s own
  already-converted declarations first before inventing new ones.
- `fdplumb`/`andrewos.h` include-order is **not a live risk** in this
  batch despite wide `fdplumb`-wrapped-call usage across
  `atkams/messages/lib` and `ams/libs/shr` (confirmed via grep: 20+
  files reference `fdplumb`/`dbg_open`/`andrewos.h`). The 2026-07-17
  fossil commit `6782de786a` made `fdplumb.h` self-contained
  (`#include <sys/types.h>`/`<fcntl.h>` inside its own header, before
  the `#define open dbg_open` block) — this neutralizes the hazard
  regardless of any caller's own include order. Don't spend time
  reordering includes here; it would be unnecessary churn. See
  `project_fdplumb_include_order_abi.md`-equivalent writeup in the
  runbook's AMS1 entry if you want the full history.
- No bison/`Parser()`-generated files in this batch (confirmed: no
  `Parser(` macro in any of the 4 Imakefiles) — unlike AMS1's
  `prsdate.c`, nothing here needs excluding from `ansify` on those
  grounds.
- `atkams/messages/lib` is one of the two directories (along with
  `contrib/zip/lib`) that keep a **tree-wide gate always**, per the
  runbook's "Gate scope" section — not just because this batch
  happens to close Wave 6. Keep the tree-wide gate regardless.

## Gate 0 — pre-diagnosis and classification (do this first, for all 4 directories, then STOP)

For each of the 4 directories, in the order above:

1. `find <dir> -maxdepth 1 -name '*.ch'` (standing check 1) — confirm
   the `.ch` counts noted above yourself.
2. `ansify --dry-run --dir <dir>` and record every DRIFT/skip
   finding.
3. Run the rest of the standing checklist (items 2-9) against the
   directory: predefined-macro typo grep, empty-parens
   lifecycle-method grep, installed-header grep for converted
   non-static helpers, restated-lifecycle-param `.ch` check (read the
   real `.c` param count by hand), concurrent-commit merge check
   (check `fossil status` anyway), the milestone-agnostic checks in
   `rollout-procedure.md` (liveness census, anchored `malloc`/`free`/
   `realloc`/`calloc` grep), the broadened stranded-forward-declaration
   check (`grep -nE '(static|extern)\s+\w[\w ]*\s+\w+\(\s*\)\s*;'
   <dir>/*.c`, cross-check any hit's parameter types against its real
   definition, **resolving typedefs** — don't read literal keywords
   only, `Boolean`/similar aliases are a known blind spot), and item 9
   (import `ansify`'s own `HDR`/`parse_decl_block` logic and run it
   directly against every file in the directory to catch declaration
   blocks spanning multiple physical lines or multiple declarations
   crammed onto one physical line — AMS1 found these are silently
   dropped by the tool's own dry-run report with zero trace, so a
   plain `--dry-run` reading is not sufficient by itself here either).
4. Classify every finding from steps 2-3 against the documented
   taxonomy — cite the specific runbook finding (by batch letter,
   e.g. "matches B3 finding 3" or "matches AMS1 finding 4, the
   ansify parser gap") or `porting-assessment.md` section it matches.
   Where a classification genuinely needs a compile check to resolve
   (not just a `.ch`/`.c` read-by-hand), you may temporarily add
   `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `atkams/messages/lib`'s
   Imakefile, `make Makefile`, force-generate the relevant `.eh`
   file(s), inspect/compile-check, then `fossil revert` the Imakefile
   and delete the scratch `.eh`/`.ih`/`.o` files before moving on.
   Don't leave `-pe` live between directories.
5. Anything that does not cleanly match an existing taxonomy entry —
   a genuinely new DRIFT/skip shape, a `.ch`-vs-real-usage
   disagreement, a caller/callee argument-count mismatch, anything
   that looks like a real ~35-year-old bug rather than conversion
   noise — flag clearly as **UNCLASSIFIED, needs orchestrator ruling**
   and do not attempt to fix it yourself. Per `rollout-procedure.md`'s
   Delegation section, retype/signature rulings and hard-stop
   adjudication stay top-level. (If you find another instance of the
   known-but-deliberately-unfixed `ansify` parser gap from AMS1, that
   is already classified/ruled — hand-fix the conversion for that one
   function per AMS1's precedent, note it in the report, but you do
   NOT need a fresh ruling on whether to patch the tool; that
   question is already closed.)

Write the report (`revival/doc/claude-history/m3-ams2-REPORT.md`, per
`sonnet-playbook.md`'s format) with a per-directory classification
table, then **STOP at Gate 0**. Do not run the real `ansify --dir`
pass, do not add `-pe` permanently anywhere, do not touch any `.ch`/
`.c` file. Say clearly that you have stopped at Gate 0 and are
waiting for the orchestrator's ruling on any UNCLASSIFIED items (if
none, say that explicitly too — you still stop and wait either way).

## Gate 1 — real run (only after the orchestrator resumes you with a ruling)

Once resumed: for each of the 4 directories, in the same order, apply
Step order per `rollout-procedure.md`: real `ansify --dir`,
`CLASSFLAGS = $(CLASSINCLUDES) -pe` added for real to
`atkams/messages/lib` only (the other three have 0 `.ch` files), force-
regenerate `.eh`, fix any fallout (including anything the orchestrator
ruled on), subtree-local gate — `make -C <absolute-path> clean`, then
`depend`, then `-k install`, separate calls, absolute path, twice for
determinism, per directory. Builds stay strictly serial (one
directory's gate clean before starting the next).

**This batch closes Wave 6 and `atkams/messages/lib` is a
permanent tree-wide-gate directory — run the tree-wide gate once at
the end, after all 4 directories are individually clean twice.** Log
the tree-wide gate to `andrew-6.4/dependInstall.log`, not the
scratchpad, per the standing build-invocation convention.

Stop after the tree-wide gate is clean. **Do NOT commit. Do NOT run
any AUIS GUI/terminal binary interactively** — the orchestrator will
present runtime-check suggestions to wdc separately after
independently re-verifying your work.

## Report

Update `m3-ams2-REPORT.md` (same file, append rather than rewrite the
Gate 0 section) with:
- Gate 1 confirmation: real `ansify --dir` results per directory vs.
  what Gate 0 predicted (flag any drift from the prediction).
- Any new DRIFT/skip/compile-fallout finding not covered at Gate 0,
  and how you resolved it (or, if genuinely unresolved, clearly
  flagged for the orchestrator).
- Per-file `ansify` conversion counts (methods/classprocs/helpers) per
  directory; confirm `atkams/messages/lib`'s `-pe`/`.eh` results
  explicitly (17 `.ch` files expected).
- Gate results (twice per directory, plus the one tree-wide gate).
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do` to find live consumers first; never
  launch GUI apps from the session; no saves against unversioned
  fixtures). `atkams/messages/lib` is the `messages` GUI backend —
  identify specific, concrete user-facing actions in `messages` that
  exercise the functions you actually hand-fixed or touched
  meaningfully (not just a generic "open messages" suggestion),
  learning from the AMS1 mistake where a suggested check
  (`cuin dirinfo`) was later found not to actually exercise the fix it
  was attached to. Be precise about which fix each suggested check
  does and does not cover.
- Anything that surprised you or didn't match this prompt's
  expectations.
