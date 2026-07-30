# M3 Wave 2, Batch B3: 13 small/leaf directories — ansify + `-pe`/`.eh` rollout

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before starting (same hard rules as every M3 batch: no fossil commits ever, stop at the gate, write `m3-b3-leaf-dirs-session.diff` in the tree root and `m3-b3-leaf-dirs-REPORT.md` in `revival/doc/claude-history/`, command style for an unattended session). Read `porting-assessment.md`'s §17 subsection (search "classpp's own `InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing") and `m3-rollout-runbook.md`'s B1/B2 findings entries for background before touching anything — this batch hits several instances of patterns already established there.

## Scope

13 directories, 61 files: `atk/extensions` (10), `atk/syntax/tlex` (8), `atk/textobjects` (7), `atk/apt/suite` (6), `atk/lookz` (5), `atk/frame` (5), `atk/syntax/parse` (4), `atk/apps` (4), `atk/utils` (3), `atk/apt/apt` (3), `atk/textaux` (2), `atk/syntax/sym` (2), `atk/apt/tree` (2).

Like B1/B2, this batch does both halves of M3: `ansify` (K&R→ANSI) **and** the `-pe`/`.eh` Export rollout (add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to each of the 13 Imakefiles, force-regenerate `.eh`, gate).

## Pre-diagnosis already done — apply these fixes exactly, do not re-derive

The orchestrator ran `ansify --dry-run --dir` on all 13 directories and investigated every DRIFT/skip finding by hand — including test-generating several `.eh` files under a temporary `-pe` flag to empirically confirm which findings are real compile-blocking bugs versus already-known tool false positives. Apply the fixes below **before** running `ansify` for real, so its DB-lookup-based conversion sees the corrected signatures and converts these methods automatically rather than leaving them DRIFT-blocked.

### 1. Two `.ch` fixes — drop an incorrectly-restated `InitializeClass` parameter

`InitializeClass`'s real, universal convention (confirmed in `porting-assessment.md` §17 and re-confirmed here by direct `-pe` test-builds) is **1 implicit parameter only** (`classID`) — it's dispatched via an untyped function-pointer cast, no instance exists yet at class-init time. classpp does **not** special-case `InitializeClass` the way it does `InitializeObject` (which is fully hardcoded) — it runs `InitializeClass` through the *ordinary* classproc-emission loop, which always prefixes `struct classheader *` and then appends verbatim whatever extra argument(s) the `.ch` declares. So a `.ch` that "helpfully" restates the implicit param by name gets counted as an *extra* declared argument on top of the automatic prefix, producing a 2-param exported prototype — which conflicts with a `.c` definition that (correctly, per the real convention) only has 1 param. This is empirically confirmed, not theoretical: test-generating `unknownv.eh` under `-pe` produced `boolean unknownv__InitializeClass(struct classheader *, struct unknownv *);` while `unknownv.c`'s real definition is `unknownv__InitializeClass(c) struct classheader *c;` — a guaranteed `conflicting types` compile error the moment `-pe` goes on. (Three other tree-wide instances of this same restated-`InitializeClass` shape — `atk/value/metextv.ch`/`eintv.ch`/`etextv.ch` — are *not* bugs: their `.c` definitions already, correctly, take the full 2 real params, matching what their `.ch` declares; already `-pe`'d and committed in B2, confirmed safe by inspection. The difference is per-instance, not universal — always check the real `.c` param count before assuming either shape is fine.)

- **`atk/textobjects/unknownv.ch`** line 28: change
  ```
  InitializeClass(struct unknownv *self) returns boolean;
  ```
  to
  ```
  InitializeClass() returns boolean;
  ```
  (matches `unknownv.c`'s real, already-correct 1-param definition — no `.c` change needed here, just let `ansify` convert it normally afterward.)

- **`atk/apt/suite/suiteev.ch`** line 69: change
  ```
  InitializeClass(struct classheader *ClassID) returns boolean;
  ```
  to
  ```
  InitializeClass() returns boolean;
  ```
  (matches `suiteev.c`'s real, already-correct 1-param definition — same as above, no `.c` change needed.)

### 2. One more `.ch` fix — same mechanism, hits `FinalizeObject` instead

`FinalizeObject` is *partially* special-cased (fixed earlier today, 2026-07-30, in `overhead/class/pp/class.c` — see the classpp-fix entry in the runbook): the fix only synthesizes the missing `self` param for the **literal empty-parens** case. It does nothing for a `.ch` that restates *both* implicit params by name — that shape still goes through the ordinary loop and gets over-counted exactly like `InitializeClass` above. Confirmed by direct `-pe` test-build: `suiteev.eh` generates `void suiteev__FinalizeObject(struct classheader *, struct classheader *, struct suiteev *);` — 3 params — against `suiteev.c`'s real 2-param definition. (Tree-wide grep confirms this exact double-restated shape — `FinalizeObject(struct classheader *...)` — occurs **only** in this one file; no other instance exists anywhere in the source tree.)

- **`atk/apt/suite/suiteev.ch`** line 71: change
  ```
  FinalizeObject(struct classheader *ClassID, struct suiteev *self);
  ```
  to
  ```
  FinalizeObject(struct suiteev *self);
  ```
  (drops the redundant `ClassID` restatement, matching the same safe single-restatement convention every other class in the tree already uses for `FinalizeObject`/`InitializeObject` — no `.c` change needed, `suiteev.c`'s real 2-param definition is already correct.)

(`suiteev__InitializeObject`'s own DRIFT finding, and `suite.c`/`suitecv.c`'s `InitializeObject`/`FinalizeObject` DRIFT findings, are all **confirmed false positives** — `InitializeObject` is fully hardcoded by classpp regardless of `.ch`, always safe; `suite.ch`/`suitecv.ch` use plain empty parens for `FinalizeObject`, which today's earlier classpp fix already makes safe. Verified by direct `-pe` test-build for `suiteev__InitializeObject` specifically (came out correctly 2-param). No action needed on any of these three.)

### 3. One `.ch` fix — a real ~35-year-old missing parameter (already flagged in `porting-assessment.md`)

- **`atk/apt/tree/tree.ch`** lines 75-76: change
  ```
  TreeWidth()							returns long;
  TreeHeight()							returns long;
  ```
  to
  ```
  TreeWidth(tree_type_node node)					returns long;
  TreeHeight(tree_type_node node)				returns long;
  ```
  `tree.c`'s real implementations of both take `(self, node)` — the `.ch` has been missing the `node` parameter since it was written; K&R never checked. Confirmed **zero callers anywhere in the tree** (grepped for `tree_TreeWidth`/`tree_TreeHeight`), so there is no call-site blast radius to worry about — this purely corrects decades-wrong documentation to match the always-correct implementation. `tree_type_node` is already `typedef`'d in `tree.ch` (line 160) as `struct tree_node *`.

### 4. Two manual K&R→ANSI hand-folds — `ansify` cannot see these at all

Genuinely-empty-parens K&R *definitions* (`T foo() { ... }`, zero named parameters) are invisible to `ansify`'s own HDR-matching regex (it requires at least one parameter name to recognize a candidate) — this is the same "empty-parens lifecycle method" gap the standing per-batch grep task exists for (added after O4, previously confirmed real in B2's `runadewa.c`). Confirmed empirically again here by test-compiling under `-pe`: both produce a real `conflicting types` error against their exported 1-param prototype (a K&R definition with a *named* parameter list is compatible with nothing but an exact count match; a K&R definition with *literally no* names is, perhaps counter-intuitively, **not** universally lenient under this compiler/mode — verified directly, don't assume otherwise).

- **`atk/extensions/gsearch.c`** line 843: change
  ```c
  boolean         gsearch__InitializeClass()
  {
  ```
  to
  ```c
  boolean gsearch__InitializeClass(struct classheader *classID)
  {
  ```
  (body unchanged — `classID` is unused, same as `dialog__InitializeClass` below; that's fine and expected for this classproc.)

- **`atk/extensions/isearch.c`** line 224: change
  ```c
  boolean incsearch__InitializeClass()
  {
  ```
  to
  ```c
  boolean incsearch__InitializeClass(struct classheader *classID)
  {
  ```
  (same treatment.)

### 5. The already-known `dialog__InitializeClass` fix (§17's "one confirmed real exception")

- **`atk/utils/dialog.c`** lines 49-54: change
  ```c
  boolean dialog__InitializeClass(classID, self)
  struct classheader *classID;
  struct dialog *self;
  {
      return TRUE;
  }
  ```
  to
  ```c
  boolean dialog__InitializeClass(struct classheader *classID)
  {
      return TRUE;
  }
  ```
  `dialog.ch` already correctly declares plain `InitializeClass() returns boolean;` (empty parens, matching the true 1-param convention) — it's the `.c` that has the stray extra `self` param, never touched by the body. This is a real, ~35-year-old, harmless interface inconsistency (documented in `porting-assessment.md` §17) — dropping the unused param makes `.c` match what `.ch` already correctly says.

### 6. Confirmed dead code — no action needed, just don't be surprised by the skip

- `atk/apt/suite/suite.c`: `suite__GetActiveItemCaptionColor`/`GetPassiveItemCaptionColor` — not declared in `suite.ch` at all, zero callers anywhere in the tree. Will report `skipped: no signature in DB`. Leave as-is (same treatment as `im__PlayActions`/`view__InsertGraphic` from B1).
- `atk/apt/tree/tree.c`: `tree__SetNodeModified`/`NodeModified` — same: not declared in `tree.ch`, zero callers anywhere. Leave as-is.
- `atk/textaux/contentv.c`: `contentv__InitializeObject` will skip (`no signature in DB` — ordinary lifecycle-hook DB-miss, same as every other class; not a hand-fold candidate here since B3 isn't touching `-pe` self-consistency for `InitializeObject`, that's already fully hardcoded and safe). `contentv__FinializeObject` will also skip — this is the **already-documented dead, misspelled duplicate destructor** ("Finialize" for "Finalize") described in `revival.md`'s "A destructor that never woke up, twice" entry (the `atk/adew/cel.c` twin was found 2026-07-30). Confirmed still dead (empty body, zero callers under either spelling) — no fix, this was already anticipated.

### 7. Two safe "unparseable K&R declarations" skips — file-local helpers, no fix needed

- `atk/apt/apt/apt.c`: `Free_Vector`'s single parameter uses a genuinely exotic declarator (`register char *((*vector)[]);` — pointer to array of pointers), which `ansify`'s strict local-decl parser correctly refuses to guess at. Leave as K&R (still compiles fine as-is; this is a file-local helper, not a class method, so there's no `-pe` exposure).
- `atk/textaux/compchar.c`: `SelfInsertCmd`'s declaration block uses the no-space-before-`*` style (`struct textview*tv;`) that `ansify`'s local-decl regex doesn't tolerate (it requires whitespace between the type and the declarator). Leave as K&R, same reasoning — file-local helper, safe skip, harmless tool limitation (not worth a tool fix for one instance).

### 8. One "unhandled type" skip — flag but do not fix

- `atk/apt/tree/treev.c`: `treev__SetHitHandler` — `.ch` declares `SetHitHandler((long *handler)(), char *anchor)`, but `.c`'s real definition takes `struct view *(*handler)()` and `struct view *anchor` — a real type mismatch between `.ch` and `.c` for both parameters (worth noting: a sibling class, `atk/org/orgv.ch`, declares the same method as `SetHitHandler(procedure handler, struct view *anchor)` — using this codebase's generic-function-pointer placeholder type for `handler` and the correct `struct view *` for `anchor`, suggesting `treev.ch`'s declaration is simply stale/wrong documentation, same species as B2's six `.ch` typos). **Do not fix this** — zero confirmed callers of `treev_SetHitHandler` specifically were found in the time available (only a different class's `foldertreev_SetHitHandler` macro is called anywhere, which may or may not resolve to this same implementation depending on the class hierarchy — not fully traced). `ansify` will safely skip it and leave it K&R (file-local to a `.c` that has many other real conversions — the skip doesn't block anything). If you have spare time after the rest of this batch is done and gated clean, tracing the real caller chain and confirming whether this is worth fixing (either now or flagging for a future wave) would be a reasonable bonus investigation — but do not let it block or delay the batch.

## Task

1. Apply all the `.ch`/`.c` fixes in sections 1-5 above.
2. Run `ansify --dir <path>` (real run, not dry-run) on each of the 13 directories. Confirm the previously-DRIFT-blocked methods (`unknownv__InitializeClass`, `suiteev__InitializeClass`, `suiteev__FinalizeObject`, `tree__TreeWidth`, `tree__TreeHeight`) now convert cleanly instead of DRIFT-skipping. Investigate any *new* DRIFT/skip finding that doesn't match one of the categories above the same way every prior M3 batch has (check `.ch` vs `.c` by hand, consult `porting-assessment.md`/`m3-rollout-runbook.md` for known patterns first).
3. Standing per-batch checks (already run once by the orchestrator, confirmed clean — re-confirm quickly as part of your own pass): grep for single-underscore `_STDC_`-style macro typos (clean), grep for other empty-parens lifecycle methods beyond the two already found (`grep -nE '__(InitializeClass|InitializeObject|FinalizeObject)\(\s*\)' <dir>/*.c`) in case the dry-run/real-run conversion surfaces anything the orchestrator's single pass missed. The brace-glued-parser-gap grep is **no longer needed** — that gap was fixed in the tool itself earlier today (2026-07-30) and confirmed to catch this shape automatically now.
4. Add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to each of the 13 directories' Imakefiles (same placement convention as every other `-pe`'d directory — see `atk/basics/common/Imakefile` or `atk/value/Imakefile` for the exact pattern).
5. Force-regenerate every `.eh` in all 13 directories with the rebuilt classpp (remove existing `.eh`, `make <name>.eh <name>.eh ...` per directory, explicit target list, same mechanic as B1/B2).
6. Investigate and fix any new compile fallout from turning `-pe` on for real (beyond what's already anticipated above) — same triage process as every prior batch.
7. Subtree-local gate for each of the 13 directories: `make -C <absolute-path> clean`, then `depend`, then `-k install` — separate calls, absolute paths, not chained, **twice each** for determinism (26 gate cycles total).

## Gate

Stop after all 13 directories gate clean, twice each. **Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively** — the orchestrator will present runtime-check suggestions to wdc separately after independently re-verifying your work.

## Report

Write `revival/doc/claude-history/m3-b3-leaf-dirs-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- Confirmation that each of the 5 pre-diagnosed `.ch`/`.c` fixes was applied exactly as specified, and that `ansify`'s real run subsequently converted the previously-blocked methods cleanly.
- Any new DRIFT/skip finding not covered above, and how you resolved (or, if genuinely unresolved, clearly flagged) it.
- Per-directory `ansify` conversion counts (methods/classprocs/helpers) and gate results (twice each, all 13).
- `fossil status`/`fossil extras` confirming exactly which files changed, no commit made.
- Anything that surprised you or didn't match this prompt's expectations.
