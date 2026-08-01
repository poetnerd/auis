# M3 Wave 7, Batch C2: `contrib/zip/utility` + 10 small `contrib` directories — ansify + `-pe`/`.eh` rollout — closes Wave 7, closes M3, hands off to M4

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting. Also read `m3-rollout-runbook.md` in full, in particular:
the "Current standing per-batch checklist" section (**all 9 active
items**), the "Session structure going forward" section (this batch
uses the two-gate shape it defines), and the C1 findings entry
(directly above this batch in the runbook) — C1 just closed
`contrib/zip/lib`, the sibling directory to this batch's
`contrib/zip/utility`, and corrected a real mistake in the
orchestrator's own prior guidance on `FinalizeObject`'s codegen
(read it — the corrected rule is what item 5 now says, and this batch
has two directories where it's directly relevant, see below). Also
read `porting-assessment.md`'s §14/§17 (the `InitializeClass`/
`InitializeObject`/`FinalizeObject` special-casing) and point 4's
"untyped `.ch` parameter" finding (the very first bullet under that
point — directly relevant to a known issue in this batch's scope, see
below) and `m3-batches.md`'s Wave 7 section for scope confirmation.

**This is a routine batch (not one of the four flagged-risky ones —
T1, I2, AMS1, C1 — that keep full orchestrator pre-diagnosis).** Per
the amendment, the pre-diagnosis legwork is yours to do, not the
orchestrator's. This prompt does not pre-diagnose the batch for you,
but does record some context the orchestrator already confirmed by
reading the actual source, so you don't have to re-derive it.

**This batch closes Wave 7 and closes M3 entirely — the last M3
session before handoff to M4.** Treat the end-of-batch gate
accordingly (see "Gate 1" below): this is not just a wave-close, it's
M3's own completion gate, and per the runbook's 2026-08-01 wave-close
amendment the bar here is a full clean rebuild, not just an
incremental tree-wide `dependInstall`.

## Scope

11 directories, 40 files, in this order (matches `m3-batches.md`'s C2
entry; file counts independently re-confirmed by the orchestrator
against the real tree, not just copied from the batch map):

1. `contrib/mit/annot` (9 `.c`, 9 `.ch`)
2. `contrib/zip/utility` (6 `.c`, 6 `.ch`) — `lt`/`ltv`/`sched`/
   `schedv`/`ltapp`/`schedapp`; see the two confirmed items below
   before you start this one.
3. `contrib/time` (6 `.c`, 6 `.ch`)
4. `contrib/mit/util` (6 `.c`, 5 `.ch`)
5. `contrib/srctext/html` (3 `.c`, 2 `.ch`)
6. `contrib/srctext/ptext` (2 `.c`, 2 `.ch`)
7. `contrib/srctext/ltext` (2 `.c`, 2 `.ch`)
8. `contrib/demos/circlepi` (2 `.c`, 2 `.ch`)
9. `contrib/calc` (2 `.c`, 2 `.ch`) — the classic AA-erase ghost-text
   bug lives here (`project_calc_inset_status`-equivalent write-up in
   `roadmap.md`), unrelated to this batch's scope; don't be surprised
   to see the file, no action needed on that bug.
10. `contrib/wpedit` (1 `.c`, 1 `.ch`)
11. `contrib/eatmail` (1 `.c`, 0 `.ch`) — the only directory in this
    batch with no `.ch` files; confirm this yourself (checklist item 1)
    rather than trusting this note blindly, same as every prior batch's
    practice.

All 10 of the other directories have at least one `.ch` file, so this
is a substantial `-pe`/`.eh` rollout across nearly the whole batch, not
a handful of plain-`ansify`-only directories like most routine batches
so far.

## Context already confirmed by the orchestrator (saves you the legwork)

- **`contrib/zip/utility`'s `Set_Debug` classproc is untyped in all
  four `.ch` files that declare it** (`lt.ch:54`, `ltv.ch:66`,
  `sched.ch:54`, `schedv.ch:62` — all read the same: `Set_Debug(
  debug );`, no type keyword at all). This is the exact shape
  `porting-assessment.md` point 4 finding 1 already documents: an
  untyped `.ch` parameter gets rewritten by classpp to `void *` in the
  typed cast, and "an all-`void *` cast for a method whose callers pass
  integers usually means the `.ch` never had types, not that the
  callers are wrong." Confirmed here directly: all four real
  implementations (`lt.c:63`, `ltv.c:252`, `sched.c:56`, `schedv.c:179`)
  and all four call sites (`ltapp.c:115,123`, `schedapp.c:105,110`)
  agree the real value is `boolean` (every call site passes a
  `static boolean debug` variable; `lt.c`/`sched.c` each keep their own
  `static boolean debug` mirroring it internally). **This is the exact,
  already-diagnosed `contrib/zip/utility/ltapp.c:115,123` gate blocker
  `roadmap.md` has carried since 2026-07-11** ("same untyped-K&R-`.ch`
  gap as the rest of `contrib/zip/lib` before its M1-style typing
  pass... needs the same `.ch`-typing treatment") — resolving it here,
  the way every other class's untyped parameter has been resolved all
  through M3, closes that standing roadmap item. Fix direction: type
  all four `.ch` declarations `Set_Debug( boolean debug );`, not `int`
  or `void *`. **One wrinkle to watch**: none of the four `.c`
  definitions (`lt__Set_Debug`, `ltv__Set_Debug`, `sched__Set_Debug`,
  `schedv__Set_Debug`) actually declare their second parameter's type
  in the K&R declaration block at all — the parameter (`mode`) is used
  directly with no preceding `register TYPE mode;` line, which is
  legal K&R (implicit `int`) but means `ansify`'s own signature
  converter has nothing to read for this parameter locally; it will
  need the `.ch`'s (now-typed) `boolean` to drive the ANSI
  conversion, same as any ordinary DB-signature-driven class-method
  conversion — hand-verify this parameter converts to `boolean mode`
  in all four `.c` files, don't assume `ansify` infers it correctly on
  its own the first time.
- **`schedv.ch`/`ltv.ch`'s `FinalizeObject`/`InitializeClass`
  restatements were flagged in the runbook as "known unresolved
  instances" for this batch (checklist item 5) — orchestrator already
  checked both and they are the safe shape, not the C1-corrected
  broken shape.** Both files restate `InitializeClass( struct
  classheader *classID )` (1 param, matches the true implicit param
  exactly — not an over-count) and both restate `InitializeObject`/
  `FinalizeObject` as the full `( struct classheader *classID, struct
  CLASSNAME *self )` (2 params each), which matches their real `.c`
  definitions' actual 2-param signatures exactly (`schedv.c:186,198,217`,
  `ltv.c:260,272,300` all take `classID, self`). This is unlike C1's
  `zipstatus__FinalizeObject` (which restated 2 params in the `.ch` but
  only had 1 in the `.c`) — here `.ch` and `.c` already agree, so no
  fix is expected. Still run item 5's check yourself rather than
  trusting this note blindly (same standing practice as every other
  confirmed item in this section) — this note only says what the
  orchestrator found, not a license to skip the check.
- No bison/`Parser()`-generated files anywhere in this batch (confirmed
  via grep: no `Parser(` macro in any of the 11 Imakefiles) — nothing
  needs excluding from `ansify` on those grounds.
- None of these 11 directories currently carry `CLASSFLAGS = $(CLASSINCLUDES)
  -pe` (confirmed via grep) — add it for real to the 10 `.ch`-bearing
  ones as part of this batch's normal Gate 1 mechanics, same as every
  prior `-pe` rollout.
- `contrib/zip/utility` already has stray `.eh`/`.ih`/`.o` build
  artifacts sitting in the source directory from an earlier,
  uncommitted manual investigation (the 2026-07-11 `MK_CALC`-gating
  session that first found the `ltapp.c` blocker). These are untracked
  build output, not fossil-tracked source — `fossil status` will not
  show them as EDITED. Don't assume they reflect a completed `-pe` pass;
  treat the directory as a normal fresh `-pe` rollout and let the real
  `make depend`/`install` regenerate them properly.

## Gate 0 — pre-diagnosis and classification (do this first, for all 11 directories, then STOP)

For each of the 11 directories, in the order above:

1. `find <dir> -maxdepth 1 -name '*.ch'` (standing check 1) — confirm
   the `.ch` counts noted above yourself.
2. `ansify --dry-run --dir <dir>` and record every DRIFT/skip finding.
3. Run the rest of the standing checklist (items 2-9) against the
   directory: predefined-macro typo grep, empty-parens
   lifecycle-method grep, installed-header grep for converted
   non-static helpers, restated-lifecycle-param `.ch` check (the
   corrected `FinalizeObject`-vs-`InitializeClass` rule — read the real
   `.c` param count by hand, and remember `FinalizeObject`'s hardcoded
   2-param special case doesn't apply the same way `InitializeClass`'s
   does), concurrent-commit merge check (check `fossil status` anyway),
   the milestone-agnostic checks in `rollout-procedure.md` (liveness
   census, anchored `malloc`/`free`/`realloc`/`calloc` grep), the
   broadened stranded-forward-declaration check (`grep -nE
   '(static|extern)\s+\w[\w ]*\s+\w+\(\s*\)\s*;' <dir>/*.c`,
   cross-check any hit's parameter types against its real definition,
   resolving typedefs — `Boolean`/`boolean` and now `float` are known
   blind spots if read literally, and check block-scope/function-local
   hits too, not just file-top-level), and item 9 (import `ansify`'s
   own `HDR`/`parse_decl_block` logic and run it directly against every
   file in each directory — C1 confirmed this is load-bearing, not
   belt-and-braces, and found real silent misses including a live class
   method; don't skip it here just because this batch's directories are
   individually smaller than C1's).
4. Classify every finding from steps 2-3 against the documented
   taxonomy — cite the specific runbook finding (by batch letter) or
   `porting-assessment.md` section it matches. The two `contrib/zip/
   utility` items above are already classified for you (cite this
   prompt's "Context already confirmed" section); you do not need a
   fresh ruling on either unless your own check contradicts what's
   written here. Where a classification genuinely needs a compile
   check to resolve (not just a `.ch`/`.c` read-by-hand), you may
   temporarily add `-pe` to a directory's Imakefile, `make Makefile`,
   force-generate the relevant `.eh` file(s), inspect/compile-check,
   then `fossil revert` the Imakefile and delete the scratch `.eh`/
   `.ih`/`.o` files before moving to the next directory. Don't leave
   `-pe` live between directories during Gate 0.
5. Anything that does not cleanly match an existing taxonomy entry —
   a genuinely new DRIFT/skip shape, a `.ch`-vs-real-usage
   disagreement, a caller/callee argument-count mismatch, anything
   that looks like a real ~35-year-old bug rather than conversion
   noise — flag clearly as **UNCLASSIFIED, needs orchestrator ruling**
   and do not attempt to fix it yourself. Per `rollout-procedure.md`'s
   Delegation section, retype/signature rulings and hard-stop
   adjudication stay top-level.

Write the report (`revival/doc/claude-history/m3-c2-REPORT.md`, per
`sonnet-playbook.md`'s format) with a per-directory classification
table, then **STOP at Gate 0**. Do not run the real `ansify --dir`
pass, do not add `-pe` permanently anywhere, do not touch any `.ch`/
`.c` file. Say clearly that you have stopped at Gate 0 and are waiting
for the orchestrator's ruling on any UNCLASSIFIED items (if none, say
that explicitly too — you still stop and wait either way).

## Gate 1 — real run (only after the orchestrator resumes you with a ruling)

Once resumed: for each of the 11 directories, in the same order, apply
step order per `rollout-procedure.md`: real `ansify --dir`, `CLASSFLAGS
= $(CLASSINCLUDES) -pe` added for real to the 10 `.ch`-bearing
directories, force-regenerate `.eh`, fix any fallout (including
anything the orchestrator ruled on — in particular the `Set_Debug`
retyping across all four `contrib/zip/utility` classes), subtree-local
gate — `make -C <absolute-path> clean`, then `depend`, then `-k
install`, separate calls, absolute path, twice for determinism, per
directory. Builds stay strictly serial (one directory's gate clean
before starting the next).

**This batch closes Wave 7 and closes M3 — run the tree-wide
`dependInstall` gate once after all 11 directories are individually
clean twice, the same as every prior wave close, and confirm the
`contrib/zip/utility/ltapp.c` 2-error baseline is now GONE** (this
batch is expected to fix it, not just document it again — if it's
still there after your `Set_Debug` retyping, that's a real problem to
flag, not an expected baseline to note and move past). **Then, because
this is M3's own completion gate and the handoff point to M4, also run
a full clean rebuild from the tree root**: `nohup make Clean; make
World` (backgrounded, per the standing build-invocation convention —
log to `andrew-6.4/dependInstall.log`, poll with `pgrep`, never run
concurrent builds, never use `make -k` for this one since the tree is
expected fully clean at this checkpoint). This is a materially
stronger check than the incremental tree-wide gate and has not
actually been run since Wave 6's close — expect it to take a while;
budget real time for it rather than treating it as routine.

Stop after both the tree-wide `dependInstall` gate and the full clean
`make World` are clean. **Do NOT commit. Do NOT run any AUIS GUI/
terminal binary interactively** — the orchestrator will present
runtime-check suggestions to wdc separately after independently
re-verifying your work.

## Report

Update `m3-c2-REPORT.md` (same file, append rather than rewrite the
Gate 0 section) with:
- Gate 1 confirmation: real `ansify --dir` results per directory vs.
  what Gate 0 predicted (flag any drift from the prediction).
- Explicit confirmation of the `Set_Debug` fix across all four
  `contrib/zip/utility` classes and that the `ltapp.c` 2-error baseline
  is gone.
- Any new DRIFT/skip/compile-fallout finding not covered at Gate 0,
  and how you resolved it (or, if genuinely unresolved, clearly
  flagged for the orchestrator).
- Per-file `ansify` conversion counts (methods/classprocs/helpers) per
  directory.
- Gate results (twice per directory, plus the tree-wide gate, plus the
  full `make Clean; make World` result).
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do` to find live consumers first; never
  launch GUI apps from the session; no saves against unversioned
  fixtures). Identify specific, concrete user-facing actions that
  exercise whatever you actually hand-fixed or touched meaningfully in
  each directory — not generic "open the app" suggestions. If any of
  these 11 directories turns out to have no live/GUI-visible consumer
  in the current build (several are small/leaf utilities — check `nm
  -g` against `runapp` and the relevant `.do` files before assuming),
  say so plainly rather than inventing a check.
- Anything that surprised you or didn't match this prompt's
  expectations.
- Since this closes M3: a short closing summary suitable for lifting
  into `m3-batches.md`'s session-count summary and
  `m3-rollout-runbook.md`'s "Resource note" — total M3 session count,
  confirmation that all 91 directories are accounted for, and anything
  worth flagging as a lesson for M4's own kickoff.
