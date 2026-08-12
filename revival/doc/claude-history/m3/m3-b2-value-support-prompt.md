# M3 Wave 2 batch B2: `atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full), `m3-rollout-runbook.md` (including its full "Findings from real sessions" — O1 through O4 and B1 are all there now), `m3-batches.md`, and `porting-assessment.md` §14 and §17 before starting. Also skim `m3-b1-basics-common-REPORT.md` — B1 just did the first large-scale (41-class) `-pe`/`.eh` rollout and found real new patterns you'll need.

This is Wave 2's second batch: 5 directories, 88 files, ~81 classes total — the widest single M3 batch by class count so far (more than B1's 41). All 5 get `-pe` added and are class-heavy, so treat this with the same care B1 needed, not like a Wave-1-style grab-bag.

## Pre-diagnosis: every DRIFT/skip finding in this batch is already resolved below — do not re-derive

A dry-run (`ansify --dry-run --dir`, 2026-07-30) plus manual investigation of every finding was done before writing this prompt. **0 compile failures in the dry run** (expected — dry runs never compile). Findings by directory: `atk/value` 12 DRIFT + 10 skipped; `atk/support` 1 skipped; `atk/supportviews` 7 skipped; `atk/adew` 1 DRIFT + 5 skipped; `atk/basics/x` 1 skipped. **Every one of these 37 findings falls into one of 5 already-understood categories** — read this section fully before running anything for real.

### Category 1: `InitializeObject`/`FinalizeObject` empty-parens false positive (`porting-assessment.md` §17) — 14 instances in `atk/value`, 1 in `atk/adew`

Same mechanism as B1: classpp hardcodes a 2-param (`classID`, `self`) convention for these two names regardless of what the `.ch` says. All are already fully typed K&R (confirmed by direct inspection) — hand-fold each to a single ANSI line, same as B1's `init.c`/`keystate.c`/`event.c`:

- `atk/value`: `bargrphv.c` (`bargraphV__InitializeObject`), `buttonv.c` (`buttonV__InitializeObject` **and** `buttonV__FinalizeObject`), `checkv.c`, `controlv.c`, `getrecv.c`, `pianov.c`, `sliderv.c` (**both** Init+Final), `stringv.c`, `thumbv.c`, `updateq.c` (**both** Init+Final), `valuev.c` (`valueview__InitializeObject`) — 14 total.
- `atk/adew`: `cel.c`'s `cel__InitializeObject` (DRIFT), `runadewa.c`'s `runadewapp__InitializeObject`, `wincelv.c`'s `wincelview__InitializeObject` — all confirmed already-typed K&R (`classID`, `self`), same fold.

### Category 2: `InitializeClass` restated-param false positive (`porting-assessment.md` §17, the other shape) — 3 instances, `atk/value`

`eintv.c`/`etextv.c`/`metextv.c` each have `boolean <class>__InitializeClass(classID)` / `struct classheader *classID;` already fully typed K&R — their `.ch` restates the param as `InitializeClass(struct <class> *self)`, which classpp ignores for codegen (it always uses its own hardcoded 1-param, `classID`-only convention for `InitializeClass`, dispatched via an untyped cast — same as documented in §17 and reconfirmed in B1's write-up). Hand-fold each to `boolean <class>__InitializeClass(struct classheader *classID)`.

### Category 3: NEW pattern — declared parameter the implementation doesn't use, safe to pad (not a behavior bug) — 4 instances, `atk/value`

This is genuinely new (not seen in O1-O4/B1) and **is not covered by classpp's Initialize*/Finalize* special-casing** — it's an ordinary virtual method where the `.c` implementation simply ignores one or more parameters its own `.ch` declares. Investigated all 4 individually; in every case the parameter is provably unused/safe to add without changing behavior:

- **`valuev.c`'s `valueview__DrawFromScratch(self)`** vs. `valuev.ch`'s `DrawFromScratch(long x, long y, long width, long height)`: this is `valueview`'s own **base-class stub**, body is literally `/* Subclass responsibility */` (a no-op). Confirmed every real subclass (`bargrphv.c`, `checkv.c`, `controlv.c`, `buttonv.c`, `getrecv.c`, `pianov.c`, `stringv.c`, `sliderv.c`, `thumbv.c`) already overrides it with the full, correct `(self,x,y,width,height)` signature — the base stub is intentionally an abstract-method placeholder. Pad: `void valueview__DrawFromScratch(struct valueview *self, long x, long y, long width, long height) { /* Subclass responsibility */ }` — no body change.
- **`valuev.c`'s `valueview__Changed(self)`** vs. `valuev.ch`'s `Changed(long status)`: same shape — base stub, body is a no-op comment ("subclasses should override this..."). No subclass in this batch overrides with the full signature (three subclasses — `entrint.c`/`entrtext.c`/`menttext.c` — override `Changed` with their own, *different*, correctly-`.ch`-matched 0-arg signature, which is fine and not part of this finding at all). Pad the base stub the same way: add `long status` as an unused parameter, no body change.
- **`clklistv.c`'s `clicklistV__WantInputFocus(self)`** vs. its own `clklistv.ch`'s `WantInputFocus(struct view *requestor)`: body unconditionally redirects focus to `self->cltextview`, never reading `requestor`. Pad: add `struct view *requestor` as an unused parameter, no body change.
- **`mentstrv.c`'s `menterstrV__WantInputFocus(self)`** vs. its own `mentstrv.ch`'s `WantInputFocus(struct view *requestor)`: same shape as the previous one, same fix.

For all 4: this is a **type-safety-only padding fix**, not a behavior change — do not try to make the implementations actually *use* the newly-added parameters, that would be a real feature change out of scope here. If you want to flag it for wdc as worth a deeper look later (whether any of these methods *should* use their declared parameter), note it in the report, but the padding fix itself is what's needed for `-pe` to succeed and is safe on its own.

### Category 4: dead/orphaned methods, not declared in their own `.ch`, never called — leave K&R, no action required — remainder of the skips

Confirmed via `grep` (both a same-directory `.ch` declaration search and a tree-wide dispatch-macro-call search) that none of these are reachable through class dispatch, and most are already fully typed K&R:

- `atk/value`: `valuev.c`'s `valueview__GetCenter(self, x, y)` — never called anywhere.
- `atk/support`: `envrment.c`'s `environment__Dump(self, level)` — never called anywhere.
- `atk/supportviews`: `matte.c`'s `matte__SetDrawing`, `oscroll.c`'s `normal_scroll__Update`/`motif_scroll__Update`/`normal_scroll__Hit`/`motif_scroll__Hit`, `sbutton.c`'s `sbutton__WriteDataPart`/`sbutton__ReadDataPart` — all 7 are called **only by their own exact C name within the same file** (an internal direct call bypassing class dispatch, same idiom seen in B1's `DrawFromScratch`/O-series precedent), never through a `class_Method(...)` dispatch macro, and none are declared in their own `.ch`. Since nothing declares them, `-pe`'s `.eh` regen cannot generate a conflicting prototype for them either (confirmed: no `.eh` conflict risk).
- `atk/adew`: `cel.c`'s `cel__ClearChain(self)`, `celv.c`'s `celview__SetDrawing(self,key)` — same shape as above.

No action required on any of these — leave K&R, exactly like B1's `im__PlayActions`/`view__InsertGraphic` precedent. If you want the consistency of converting them (they're all already fully typed), that's fine too, but it's optional.

### A genuine, harmless ~35-year-old dead-code duplicate, found during pre-diagnosis — worth noting, not fixing

`atk/adew/cel.c` has **two** functions with almost-identical names: the real, correctly-spelled, correctly-declared (in `cel.ch`) `cel__FinalizeObject` (line 82 — does real cleanup work, destroys the object's dataObject, unlinks it from its chain) and a **separate, misspelled** `cel__FinializeObject` (line 774 — note "Finialize", transposed letters) whose entire body is empty (`{ }`). The misspelled twin is never declared in `cel.ch`, never called anywhere, and does nothing even if it somehow were called — a harmless historical leftover, not a live bug (unlike O3's `regcomp`/`regexec`, where the misspelling caused the *wrong* function to actually run). This is why it shows up as a separate "no signature in DB" skip. No fix needed; mention it in your report for completeness, same spirit as other "found, logged, not fixed" curiosities.

### One genuinely new parser-bailout shape — `atk/basics/x`

`xim.c:4478`'s `RequestSelection(self, cutBuff, xfree)` is `skipped: unparseable K&R declarations`. Root cause: its first parameter's declaration is `struct xim* self;` — the `*` glued to the type name rather than the variable name (`xim*` not `xim *`), a spacing shape not yet seen in any prior batch's parser-gap taxonomy. Already fully typed, safe hand-fold:
```c
static boolean RequestSelection(struct xim *self, struct expandstring *cutBuff, boolean *xfree)
```
Verify with `make xim.o` (this is a big, slow-to-compile file — expect it to take a while).

### `-pe`-invisible empty-parens lifecycle method — found, must fix before `-pe` regen — `atk/adew`

Unlike B1 (which checked clean), this batch has a real instance of O4's "truly empty parens, invisible to `ansify`'s dry-run entirely" pattern: `atk/adew/runadewa.c:252`'s `runadewapp__InitializeClass()` — **zero** parens content, not even `classID`. `runadewa.ch` declares `InitializeClass() returns boolean;`. This will not show up as DRIFT or a skip in any dry-run census (confirmed — it doesn't), but **will** conflict with `-pe`'s regenerated `.eh` (which emits classpp's own hardcoded 1-param `classID`-only prototype for ordinary classprocs). Fix **before** running the `-pe` regen, same order B1 used for its pre-diagnosed fixes:
```c
boolean runadewapp__InitializeClass(struct classheader *classID)
```
(unused parameter, body unchanged — matches the `eq.c`/`testobj.c` precedent exactly).

### Standing checks (do these yourself too, don't just trust this list)

- **Empty-parens lifecycle methods** (`grep -rEn "__(InitializeClass|InitializeObject|FinalizeObject)\(\)"` across all 5 directories): only the one `runadewa.c` instance above found — re-verify yourself, since dry-run census cannot surface these at all.
- **`_STDC_`-style macro typos**: none found in this batch (one incidental match was just a comment in `xim.c` referencing O4's already-fixed `cmenu.h` bug, not a live occurrence here).
- **Brace-glued-to-last-parameter K&R blocks** (B1's new finding, `grep -lE ';[ \t]*\{[ \t]*$'`): present in `entrtext.c`, `entrint.c`, `menttext.c` (`atk/value`), `mark.c`, `nstdmark.c`, `rectlist.c`, `style.c`, `tree23.c` (`atk/support`), `oscroll.c` (`atk/supportviews`), `xim.c`, `xgraphic.c` (`atk/basics/x`) — expect some functions in these files to silently stay K&R with no report, same bounded (compile-error-visible, not silent) risk B1 already established. Don't try to hand-fix every instance; let the subtree-local gate prove nothing real is affected, same as B1's resolution. If the gate does surface a conflict traceable to this pattern, that's expected — fix it as ordinary fallout, not an escalation.

## The `-pe`/`.eh` rollout, all 5 directories

None of the 5 currently has a `CLASSFLAGS` line (all inherit the tree default). For each: add `CLASSFLAGS = $(CLASSINCLUDES) -pe`, regenerate the Makefile, confirm the flag landed in the `.ch.eh:` rule, force-regenerate every `.eh` in that directory. Apply the pre-diagnosed hand-fixes above **before** running `ansify --dir` for real on each directory (same required order B1 established) — the `runadewa.c` empty-parens fix in particular must land before `atk/adew`'s `-pe` regen, not after.

## Task

1. Confirm every count above (`.ch`/`.c` file counts, the 37 findings, the categories) via your own fresh `ansify --dry-run --dir` per directory — flag and stop if anything disagrees.
2. Re-run the 3 standing greps yourself.
3. Apply all pre-diagnosed hand-fixes (Categories 1-3, the `RequestSelection` fold, the `runadewapp__InitializeClass` fix), verifying each with a direct `make <base>.o` before moving on. Leave Category 4 (dead code) K&R unless you want the consistency — optional either way.
4. Do the `-pe`/`.eh` rollout for each of the 5 directories in turn (flag, regen, verify the flag landed).
5. Run `ansify --dir` for real on each directory. Expect the DRIFT/skip counts to match the dry-run baseline exactly (all pre-resolved by hand already) — if a real run surfaces anything genuinely new, **stop and report before doing anything else**, same standing rule as every M3 batch.
6. Fix ordinary `COMPILE FAILED — restoring original` fallout as it comes up (rock-idiom casts, `fix-missing-static-decl` non-idempotency, cross-`.ch` rock disagreements — all documented patterns from O1-O4/B1, not things to re-derive). Watch especially for B1's cross-`.ch` rock-type-disagreement shape, since this batch has many more classes interacting with each other (`atk/value`'s subclasses all inherit from `valueview`, `atk/adew`'s `cel`/`celview`/`wincelview` family, etc.) than any prior batch.
7. Subtree-local gate for **each** of the 5 directories separately: `make -C <absolute-path> clean`, then `depend`, then `-k install` — separate calls, absolute paths, not chained. Rebuild each clean twice to confirm determinism.
8. Do not run the tree-wide gate — none of these 5 is `atkams/messages/lib` or `contrib/zip/lib`.

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively.** `atk/value` (form/dialog widgets), `atk/support`/`atk/supportviews` (scrollbars, buttons, matte borders — widely consumed), `atk/adew` (a specific app framework, narrower fan-out), and `atk/basics/x` (X11-specific window-system glue, including `xim.c`, already touched once by O4) all have real, identifiable consumers — use `nm -g`/Imakefile `LIBS` lines per directory, same as every prior session, and give wdc the exact runtime-check commands. Stop after all 5 gates are green (twice each) and say you have stopped.

## Report

Write `m3-b2-value-support-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- Real instance counts vs. the dry-run baseline for all 5 directories, confirmation all 37 pre-diagnosed findings resolved as expected.
- Full detail on the `runadewapp__InitializeClass` `-pe`-invisible fix and confirmation no other instance of this pattern turned up elsewhere in the batch.
- Any new cross-`.ch` rock-type disagreements or other new fallout patterns, given this batch's much denser class-interaction graph than any prior batch.
- Whether the brace-glued-to-semicolon pattern's bounded risk held (i.e., the double-clean gate proves nothing real was silently broken) — or, if it didn't, full detail on what surfaced.
- Each hand-fix and each ordinary compile-gate fallout fix: file:line, what was wrong, the fix, compile confirmation.
- Exact runtime-check command(s) for wdc, with consumers identified, per directory.
- `fossil status` output confirming exactly which files changed, no commit made.
