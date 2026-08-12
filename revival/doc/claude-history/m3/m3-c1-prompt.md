# M3 Wave 7, Batch C1: `contrib/zip/lib` — ansify K&R→ANSI + `-pe`/`.eh` rollout

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting (same hard rules as every M3 batch: no fossil commits ever,
stop at the gate, write `m3-c1-session.diff` in the tree root and
`m3-c1-REPORT.md` in `revival/doc/claude-history/`, command style for
an unattended session). Also read `m3-rollout-runbook.md`'s "Current
standing per-batch checklist" section in full. This is one of the four
flagged-risky batches (T1, I2, AMS1, C1) that keep full orchestrator
pre-diagnosis instead of a delegate-side Gate 0 — the orchestrator has
already done that legwork below (including all 8 real DRIFT findings
`ansify --dry-run` reports); apply it and go straight to the real run.
Do not re-derive it.

**Read `project_zip_inset_status` background first** (summarized
below, full detail was in a prior session's memory): this directory is
the tree's known highest-defect-density subtree (same M2 flagged it
for), and its `.ch` files were already hand-typed for M1's `-pi`
dispatch on 2026-07-11 (a real, live, working rendering pipeline —
`Cattey.turnin`/`dragon.zip` confirmed correct at default `-O`). That
work is orthogonal to this batch: M1's `-pi` (typed `.ih`, instance
dispatch) is unrelated to M3's `-pe` (typed `.eh`, export dispatch) —
this batch adds `-pe` for the first time here. Don't confuse the two;
the `.ch` files are already well-formed and consistent with their real
`.c` implementations (that 2026-07-11 session already fixed several
real `.ch`-vs-`.c` bugs while typing them), which is exactly why this
batch's own DRIFT count (8) is low for a directory this size and this
defect-dense.

## Scope

**One directory, `contrib/zip/lib`** — 41 `.c` files, 21 `.ch` files
(every `.c` file here belongs to a class; no plain-C helper files).
Tree-wide gate is kept for this batch regardless of Wave 7's own
close-out gate — `contrib/zip/lib` is one of the two directories
`m3-rollout-runbook.md`'s "Gate scope" section singles out for a
standing tree-wide check (the other is `atkams/messages/lib`, already
closed at AMS2), independent of wave boundaries.

`COMPILERFLAGS` (M2's implicit-declaration guard) is already present
in the Imakefile — confirmed. No `CLASSFLAGS` line exists yet; add
`CLASSFLAGS = $(CLASSINCLUDES) -pe` (exact form already used in
`atk/eq/Imakefile` and `atkams/messages/lib/Imakefile` — match it,
don't invent a different composition). All 21 `.eh` files already
exist on disk as *untyped* boilerplate (the ordinary `.ch.eh:` suffix
rule runs regardless of `-pe`) — force-remove and regenerate them once
`-pe` is live, same as every prior `-pe` rollout batch.

## Pre-diagnosis already done

### `ansify --dry-run --dir src/contrib/zip/lib` result: 41 files, 0 compile failures, 8 DRIFT findings, 14 "no signature in DB" skips

### 1. The 8 DRIFT findings — all individually investigated and ruled by the orchestrator; apply these fixes directly, do not re-derive

**`zipstat.c`/`zipstat.ch` — 1 false positive, 2 real bugs:**

- `zipstatus__FinalizeObject`: false positive, same shape as the
  `fldtreev.ch` bug AMS2 already fixed (Wave 6) and B3's
  `unknownv.ch`/`suiteev.ch` before it — `zipstat.ch` restates
  `FinalizeObject`'s single implicit `self` param
  (`FinalizeObject( struct zipstatus *self );`), which
  classpp's ordinary (non-hardcoded) prototype loop then over-counts
  against the real `.c` definition's already-correct single param.
  **Fix: simplify the `.ch` line to `FinalizeObject();`** (empty
  parens, matching `suite.ch`'s established pattern) — do not touch
  the `.c` definition, it's already correct.
- `zipstatus__Issue_Status_Message` / `zipstatus__Acknowledge_Status_Message`:
  real `.ch`-vs-`.c` signature mismatch, **not** the lifecycle
  false-positive shape (these are ordinary methods). `.ch` declares
  `( char *msg )`; the real `.c` definitions take `(self, facility,
  status)` — two `long`s — and internally call a `Format_Message`
  helper to build the string before dispatching to the real
  `Issue_Message`/`Acknowledge_Message` classprocs. **Confirmed zero
  callers anywhere in the tree** for either name (`grep -rln
  "Issue_Status_Message\|Acknowledge_Status_Message" src` matches only
  `zipstat.c`/`zipstat.ch` themselves) — this is dead-from-callers but
  not dead code (still classpp-dispatchable), same shape as several
  prior `.ch`-vs-implementation copy/paste drifts (B2 finding 2,
  AMS2's 3 self-type typos), just on the parameter list instead of the
  self type. The `.c` signature is self-consistent with the file's own
  `Issue_Figure_Status_Message`/`Issue_Image_Status_Message`/etc.
  siblings (all take typed/numeric args, not raw strings) — the `.ch`'s
  `char *msg` looks like a stale copy-paste from `Issue_Message`
  just above it in the same `.ch`, never updated when this
  facility/status variant was implemented. **Fix: correct the `.ch`
  to `Issue_Status_Message( long facility, long status ) returns
  long;`** (and the `Acknowledge_` sibling identically) — matching the
  real, working `.c` implementation. Confirm zero callers still holds
  before committing to this direction; if you find a caller this
  pre-diagnosis missed, stop and flag it rather than proceeding.

**`zipobj.c`/`zipobj.ch` — all 5 real, one fix pattern:**

`zipobject__Print_Object`, `Highlight_Object_Points`,
`Normalize_Object_Points`, `Expose_Object_Points`, `Hide_Object_Points`:
the `.ch` declares all 5 with `( zip_type_figure object, zip_type_pane
pane )` (2 explicit args). The base `zipobject` class's own `.c`
definitions are `/**** NULL ****/` no-op stubs (`return zip_failure;`
unconditionally) taking only `(self, figure)` — missing `pane`
entirely. **Confirmed real**: grepped every one of these 5 method
names across the other 15 concrete subclasses in this directory
(`zipoarc`, `zipoarrw`, `zipocapt`, `zipocirc`, `zipoelli`, `zipofcap`,
`zipoimbd`, `zipoline`, `zipopath`, `zipopoly`, `ziporang`, `ziporect`,
`ziposym`, `zipotrap`, plus others) — every single real override
implements the full 3-param `(self, figure, pane)` form matching the
`.ch`; only the abstract base's stub is short. Classic "K&R base stub
silently drops an arg its body never reads, harmless until a typed
export prototype is generated" pattern. **Fix: widen all 5 stub
definitions in `zipobj.c` to add the `pane` parameter** (matching the
`.ch`, matching every real override), left genuinely unused in the
stub body (same convention as other NULL-stub methods in this
codebase) — do not touch the `.ch` or any of the 15 subclass files.

### 2. The 14 "no signature in DB" skips — confirmed dead code, safe to hand-convert; verify all 14 individually before converting, don't extrapolate from the sample

Spot-checked 4 of the 14 (`zip__Show_Statistics`, `zip__Destroy_Stream`,
`ziporect__Object_Attributes`, `zipview__Within_Which_Image`): none
appear in *any* `.ch` file's `methods:`/`classprocedures:` section
anywhere in the directory (`ansify`'s signature DB is built from every
`.ch`, so a real classproc would have an entry), and none are called
anywhere else in the directory (only their own `IN()`/`OUT()` debug
macros reference the name, which is normal self-reference, not a real
call site). This is genuinely dead code using the `Class__Method`
naming convention for consistency, not live class dispatch — same
category as O1's `#ifdef`-gated dead code and AMS2's `pcmchs.c`
(confirmed unreachable, still safe to hand-convert to ANSI). The
remaining 10 (`zipedit__Highlight_Pane_Points`,
`zipedit__Normalize_Pane_Points`, `zipedit__Delete_Stream`,
`zipedit__Undelete_Stream`, `zipedit__Highlight_Stream_Points`,
`zipedit__Normalize_Stream_Points`, `zipedit__Hide_Stream_Points`,
`zipedit__Expose_Stream_Points`, `zipview__Hide_Stream`,
`zipview__Expose_Stream`, `zipview__Within_Which_Stream`) were not
individually re-checked by the orchestrator — **do this yourself for
all 14 before hand-converting any of them**, the same grep-both-ways
(no `.ch` declaration anywhere + no caller anywhere) pattern used
above. If any one of the 14 turns out to have a caller or a `.ch`
declaration this sample missed, stop and flag it — that would change
the fix from "plain hand-convert" to a real DRIFT needing its own
ruling.

### 3. Standing checklist, already run once by the orchestrator — re-confirm as part of your own pass

- **`.ch` presence** (check 1): 21, confirmed above.
- **Predefined-macro typo grep** (check 2): clean, zero hits.
- **Empty-parens lifecycle-method grep** (check 3): clean, zero hits
  — no B3-style deterministic `-pe` compile-failure risk from this
  category.
- **Restated-lifecycle-param `.ch` check** (check 5): one instance
  found and ruled above (`zipstat.ch`'s `FinalizeObject`). No other
  `InitializeClass`/`InitializeObject`/`FinalizeObject` restated-param
  instances found in the other 20 `.ch` files by a manual read, but
  re-run this check for real once you're converting — the DRIFT report
  itself is the authoritative signal, this grep is only a
  cross-check.
- **Stranded old-style forward declaration vs. narrow ANSI param**
  (check 8, both `static` and `extern` per the AMS-broadened pattern):
  90 raw hits from `grep -nE '(static|extern)\s+\w[\w ]*\s+\w+\(\);'
  *.c` — not individually cross-checked by the orchestrator, this
  directory is too large for that to be efficient pre-diagnosis; **all
  90 need your real cross-check against their matching definitions**,
  same method as every prior batch (resolve typedefs, don't read the
  literal keyword — `Boolean`/`boolean` is the standing trap). Also
  apply the AMS2-discovered comma-list blind spot (check every name in
  a multi-name `extern A(), B(), C();` line, not just lines that
  already look like a single bare declaration) and the same-file-only
  scope rule (cross-file stranded pairs are not blockers).
- **Installed-header grep for converted non-static helpers** (check
  4): not pre-run — `contrib/zip/lib` doesn't install a shared header
  the way `overhead/mail/hdrs`/`ams/libs/hdrs` do (check whether
  `zip.h`/`zipfig.h`/`zipimage.h` etc. are actually installed via the
  Imakefile's `InstallMultipleFlags`/similar before ruling this
  N/A — don't assume, verify).
- **Concurrent-commit merge check** (check 6) and the milestone-agnostic
  checks (liveness census, `-k` on fallout collection, anchored
  `malloc`/`free`/`realloc`/`calloc` grep, runtime-check rules): not
  pre-run, yours to do during the real pass as usual.

### 4. Definitive completeness re-scan — required for this directory specifically, per the runbook

`m3-rollout-runbook.md` check 9 explicitly names `contrib/zip/lib` as
a directory that **must** run the AMS1-derived definitive completeness
re-scan (import `ansify`'s own `HDR`/`parse_decl_block` directly, run
against every file in the directory, diff against what the real
`ansify` pass actually converted) before considering this directory
converted — a manual per-function review isn't practical at 41 files.
Do not substitute an ad-hoc grep for this step; ad-hoc greps already
proved unreliable at AMS1's scale (three successive misses before the
definitive method closed it).

## Task

1. Add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `contrib/zip/lib/Imakefile`.
   Confirm via `grep -n CLASSFLAGS Makefile` after regen that it composed
   the way you expect.
2. Force-remove all 21 existing (untyped) `.eh` files before `depend`/
   `install` so they regenerate typed.
3. Real `ansify --dir src/contrib/zip/lib`.
4. Apply the 3 DRIFT fixes above exactly as specified (2 `.ch` edits in
   `zipstat.ch`, 5 stub-widening edits in `zipobj.c`).
5. Verify and hand-convert all 14 "no signature in DB" skips per
   section 2 above.
6. Run checklist items 4, 6, and 8 for real (item 8's 90 raw hits need
   full cross-checking), plus the milestone-agnostic checks.
7. Run the definitive `parse_decl_block` completeness re-scan (section
   4) — required, not optional, for this directory.
8. Subtree-local gate: `make -C <absolute-path> clean && make -C
   <absolute-path> depend && make -C <absolute-path> -k install` —
   separate calls, absolute path, twice for determinism.
9. **Tree-wide gate**: `make -C <absolute-path-to-src> dependInstall`,
   logged, twice, proper `pgrep`-based waiting (do not background-and-
   assume-done in separate tool calls — a prior session in this project
   nearly mis-reported a stale mid-build log this way). Expect it clean
   except the two already-known, already-queued
   `contrib/zip/utility/ltapp.c` errors (Wave 7 C2's problem, not
   yours — confirm the error count/identity matches exactly, don't
   just eyeball "some errors, probably fine").

## Gate

Stop once the subtree gate is clean twice, the tree-wide gate matches
the expected known-error baseline exactly, and the definitive
completeness re-scan is done. **Do NOT commit. Do NOT run any AUIS
GUI/terminal binary interactively** — the orchestrator will present
runtime-check suggestions to wdc separately after independently
re-verifying your work.

## Report

Write `m3-c1-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- `ansify --dir` results vs. the pre-diagnosis above (flag any drift
  from what's predicted here).
- Confirmation of all 3 DRIFT fixes applied exactly as specified, plus
  full disposition of all 14 "no signature in DB" skips (which of the
  14 you independently confirmed dead-and-safe vs. any that turned out
  to need a different ruling).
- Full results of checklist items 4, 6, 8 (item 8: how many of the 90
  raw hits were real conflicts after cross-check, and their fixes).
- Full results of the definitive `parse_decl_block` completeness
  re-scan — any instance found that the real `ansify` pass missed.
- Per-file `ansify` conversion counts (methods/classprocs/helpers).
- Gate results (subtree twice, tree-wide twice, matched against the
  known `ltapp.c` baseline).
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules — `nm -g`/`.do` lookup
  for live consumers first (the zip inset is a known, previously
  runtime-confirmed-working live consumer — `Cattey.turnin`/
  `contrib/zip/samples/dragon.zip` are the established test fixtures
  from the 2026-07-11 session; suggest re-exercising the same ones,
  don't invent new ones); never launch GUI apps from the session; no
  saves against unversioned fixtures.
- Anything that surprised you or didn't match this prompt's
  expectations.
