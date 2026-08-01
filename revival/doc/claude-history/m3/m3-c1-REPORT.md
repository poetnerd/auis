# M3 Wave 7, Batch C1: `contrib/zip/lib` — REPORT

## 1. Status

**Gate complete.** Subtree-local gate clean twice, tree-wide gate clean
twice (matches the known `contrib/zip/utility/ltapp.c` baseline
exactly), definitive `parse_decl_block` completeness re-scan clean (0
misses after fixes). No `fossil commit` made. No GUI/terminal AUIS
binary launched. Session diff at `andrew-6.4/m3-c1-session.diff` (15222
lines), this report at
`m3-c1-REPORT.md`.

This batch is one of the four flagged-risky batches (full orchestrator
pre-diagnosis, no delegate-side Gate 0) — went straight to the real
run per the prompt. The pre-diagnosis's 3 ruled DRIFT fixes and 14
dead-code skips were largely correct, but the real run surfaced a
meaningfully larger fallout surface than predicted (see §2). One of
the 3 pre-ruled DRIFT fixes (`zipstatus__FinalizeObject`) turned out
to be **wrong** as originally specified and needed correction — flagged
prominently in §3.

## 2. `ansify --dry-run` results vs. pre-diagnosis

Ran `ansify --dry-run --dir src/contrib/zip/lib` first, before any
edits: **41 files, 0 compile failures (dry-run never compiles), 8
DRIFT findings, 15 "no signature in DB" skips.**

- **DRIFT count (8) matches the pre-diagnosis exactly** — same 8
  functions, same shapes (`zipstat.c`'s 3, `zipobj.c`'s 5).
- **Skip count is 15, not 14** — a real, if minor, drift from the
  pre-diagnosis. Tracing the prompt's own prose: it names 4
  spot-checked functions plus "the remaining 10," but actually lists
  11 names in that second group (4 + 11 = 15, matching what `ansify`
  really reports). This looks like an off-by-one in the prompt's own
  recount, not a new function appearing — every one of the 15 names
  `ansify` actually reports is named somewhere in the prompt's own
  list. All 15 individually verified dead (see §3).

## 3. DRIFT fixes: 2 applied as specified, 1 corrected after independent verification

### `zipobj.c`'s 5 fixes — applied exactly as specified

Widened the abstract base class's 5 `NULL`-stub methods
(`Print_Object`, `Highlight_Object_Points`, `Normalize_Object_Points`,
`Expose_Object_Points`, `Hide_Object_Points`) to add the missing
`zip_type_pane pane` parameter, matching the `.ch` and every real
subclass override. No `.ch` or subclass file touched. Confirmed via the
regenerated `zipobj.eh`: `long zipobject__Print_Object(struct
zipobject *, zip_type_figure, zip_type_pane);` now matches the widened
definition.

### `zipstat.ch`'s `Issue_Status_Message`/`Acknowledge_Status_Message` — applied exactly as specified

Changed both to `( long facility, long status )`, matching the real
`.c` definitions (which call a `Format_Message` helper internally).
Re-confirmed zero callers tree-wide before applying
(`grep -rln "Issue_Status_Message\|Acknowledge_Status_Message" src`
matches only `zipstat.c`/`.ch`/`.ih`/`.eh` — all self-referential
generated/source files, no external caller). No drift from the
pre-diagnosis here.

### `zipstat.ch`'s `FinalizeObject` — pre-diagnosis was WRONG, corrected

The prompt specified simplifying `.ch` to empty parens
(`FinalizeObject();`), matching `suite.ch`'s pattern, and leaving the
`.c` untouched ("already correct single param"). **This is incorrect.**
Tracing `overhead/class/pp/class.c` lines 1146–1153 directly: when a
`.ch`'s `FinalizeObject` has empty/no `realargtypes`, classpp
**hardcodes** the exported prototype to **2 params**
(`struct classheader *, struct CLASSNAME *`) — not 1. Confirmed
empirically: every already-`-pe`-converted `FinalizeObject` in the tree
(`atk/eq/eq.c`, `atkams/messages/lib/fldtreev.c`,
`atkams/messages/lib/mailobjv.c`, 8 more) uses exactly this 2-param
form. The AMS2 runbook entry (`m3-rollout-runbook.md` line ~1930-1932)
confirms the real established fix for `FinalizeObject` specifically is
**"self-only" restatement** (`FinalizeObject( struct CLASS *self );`),
not empty parens — empty parens is only correct for `InitializeClass`.

`zipstat.c`'s real `FinalizeObject` definition had only 1 param
(`self`, missing `classID` entirely) — the same "K&R stub silently
drops an arg its body never reads" shape as the 5 `zipobj.c` fixes in
the same batch. **Fix applied:** restored `.ch` to
`FinalizeObject( struct zipstatus *self );` (self-only, matching
`fldtreev.ch`'s established convention) and widened the `.c`
definition to `void zipstatus__FinalizeObject(struct classheader
*classID, struct zipstatus *self)`. Confirmed via the regenerated
`.eh`: matches the 2-param hardcoded form, and both the export
prototype and the internal `__Finalize` call site
(`zipstatus__FinalizeObject(classID, self);`) are now consistent.

**This finding should be independently re-verified** — it directly
contradicts the prompt's explicit instruction, and while the evidence
(classpp source + 11 real precedents + the runbook's own AMS2 entry)
is strong, this is exactly the kind of ruling that belongs at the
orchestrator level per `rollout-procedure.md`'s Delegation section.

## 4. Disposition of all 15 "no signature in DB" skips

All 15 individually verified via the grep-both-ways method (no `.ch`
declaration in *any* `.ch` in the directory + no caller anywhere in the
directory, cross-checked tree-wide too): `zip__Show_Statistics`,
`zip__Destroy_Stream`, `ziporect__Object_Attributes`,
`zipview__Within_Which_Image`, `zipedit__Highlight_Pane_Points`,
`zipedit__Normalize_Pane_Points`, `zipedit__Delete_Stream`,
`zipedit__Undelete_Stream`, `zipedit__Highlight_Stream_Points`,
`zipedit__Normalize_Stream_Points`, `zipedit__Hide_Stream_Points`,
`zipedit__Expose_Stream_Points`, `zipview__Hide_Stream`,
`zipview__Expose_Stream`, `zipview__Within_Which_Stream`.

**All 15 confirmed genuinely dead** — none appear in any `.ch`'s
`methods:`/`classprocedures:` section (checked the owning class's own
`.ch` plus grepped every `.ch` in the directory), and
`grep -rln <name>` across both the directory and the whole `src` tree
matches only the function's own definition/`IN()`/`OUT()` self-
reference. All 15 hand-converted to ANSI (see per-file table in §7),
matching `ansify`'s own weave/tidy_type conventions (bare parameter
names get their real K&R-declared types; `register` dropped, matching
the tool's own `parse_local_decls` convention). No skip needed a
different ruling.

## 5. Checklist items 4, 6, 8 — full results

### Item 4 (installed-header grep)

`contrib/zip/lib` **does** install headers
(`InstallMultiple($(INCFILES), ..., $(DESTDIR)/include/atk)` in the
Imakefile) — not N/A, contrary to how it might look at a glance.
Checked all 11 installed headers (`zip.h`, `zipedit.h`, `zipefc00.h`,
`zipefn00.h`, `zipfig.h`, `zipiff00.h`, `zipifm00.h`, `zipimage.h`,
`zippane.h`, `zipprint.h`, `zipstrm.h`) for bare empty-parens function
declarations. **1 real hit**: `zipedit.h` declared 23 non-static
helper functions (`zipedit_Next_Selected_Figure`,
`zipedit_Cancel_Enclosure`, `zipedit_Display_Background_Pane`, and 20
more) with stale K&R empty parens; all 23 have real, already-converted
ANSI definitions across `zipedit.c`/`zipve00.c`/`zipve03.c`, all
wide-typed (`struct*`/`zip_type_pane`/`zip_type_figure`/`enum`/`long`/
`int` — no narrow-promotion risk). Checked tree-wide: **zero
cross-directory consumers currently include `zipedit.h`** (only used
within `contrib/zip/lib` itself), so this was not a live compile
blocker anywhere — but updated all 23 declarations to full ANSI
prototypes anyway, for correctness/consistency now that `zipedit.h` is
an installed header a future directory could pick up. Flagging this
prominently per the checklist's instruction, even though it's currently
inert. Other 10 headers: 0 hits.

### Item 6 (concurrent-commit merge check)

`fossil status` before starting and throughout showed checkout at
`13212a462d` (the tip at session start) with no concurrent commits
landing during the session. All `EDITED` files in the final `fossil
status` are exactly what this session touched. No merge risk.

### Item 8 (stranded old-style forward declaration vs. narrow ANSI param)

Pre-diagnosis estimated 90 raw hits (not individually cross-checked, by
design — "this directory is too large for that to be efficient
pre-diagnosis"). Real counts, measured directly:

- **Pre-`ansify` (pure K&R state)**: 161 raw hits from
  `grep -nE '(static|extern)\s+\w[\w ]*\s+\w+\(\);' *.c` — already 79%
  higher than the 90 estimate.
- **Post-real-run** (after `ansify`'s own `fix-missing-static-decl`
  sibling tool ran, which inserts *new* bare empty-parens declarations
  for previously-undeclared static helpers — a real, if minor, source
  of raw-count growth documented in AMS2's findings): 255 raw hits.
- **Final** (after this session's fixes): 249 raw hits remain.

**Real, compile-blocking conflicts found and fixed: 10 function names
across 9 files**, all found by iterating `make <file>.o` directly
(untruncated compiler output) on the 12 files `ansify`'s real
compile-gate rejected, rather than trying to pre-classify all raw hits
by hand:

| File | Function(s) | Narrow param causing the conflict |
|---|---|---|
| `zipds01.c` | `Parse_Stream_Commentary` (2 decls), `PriorChar` (1, inside a comma-list) | `char c` |
| `zipedit.c` | `Accept_Character` (2 decls, same text) | `char c` |
| `zipoarrw.c` | `Draw_Basic_Style` (2 decls) | `char fill` |
| `zipocapt.c` | `Accept_Caption_Character` (1 decl) | `char c` |
| `zipofcap.c` | `Accept_Caption_Character` (2 decls — one file-scope, one **block-scope inside `Build_Object`**) | `char c` |
| `zipopath.c` | `Draw` (2 decls) | `short action` |
| `zipopoly.c` | `Draw` (2 decls) | `short action` |
| `ziporect.c` | `Draw` (2 decls) | `short action` |
| `ziposym.c` | `Accept_Property_Hit` (1 decl) | `char c` |
| `zipv.c` | `Scale_Pane` (1 decl) | `float scale` (**new promotion-risk type** — `float` promotes to `double`, not previously seen in this project's item-8 findings, which so far only named `char`/`short`/`Boolean`) |

All fixed by widening the stale declaration(s) to the real ANSI
prototype, same pattern as every prior batch.

**13 more non-blocking hygiene fixes**, found via the definitive
completeness re-scan (§6) rather than the compile gate, since all
params involved are wide types (`zip_type_pixel` is `typedef long`, not
narrow) — updated anyway for consistency once the underlying
definitions were hand-converted: `apt_MM_Compare`'s same-file forward
declaration in `zipprint.c` (1), `Compute_Handle_Positions`'s 2 stale
declarations each in `zipoarc.c`/`zipocapt.c`/`zipofcap.c`/
`ziporect.c`/`zipotrap.c` (10), `Feather_Points`'s 2 stale declarations
in `zipoarrw.c` (2).

**2 self-type `.ch` bugs found via this same compile-driven process,
not part of item 8 but adjacent** — see §3-equivalent finding below.

### New finding: 2 more self-type typos, same shape as `porting-assessment.md`'s B2 finding 2

While fixing `zipoarrw.c`'s compile failure, found
`zipoarrw.ch`'s `InitializeObject( struct zipobject *self )` uses the
**parent** class's type instead of its own (`zipoarrow`) — confirmed
real via `self->tolerance` (a `zipoarrow`-only data field) failing to
compile against the wrong type. Checked all 21 `.ch` files' declared
`InitializeObject` self types against their own class names: found one
more instance, `zipoplin.ch` (same `struct zipobject *self` typo,
same `self->tolerance` field-access tell). Both fixed to their real
class name (`struct zipoarrow *self` / `struct zipoplin *self`); the
corresponding `.c` definitions (which `ansify` had already woven with
the *old*, wrong type from the stale signature DB) were hand-corrected
to match after rebuilding the DB. No other `.ch` in the directory has
this bug (checked all 21).

## 6. Definitive `parse_decl_block` completeness re-scan — required for this directory, ran it

Imported `ansify`'s own `HDR`/`parse_decl_block`/`RESERVED`/
`BARE_PARAMS` directly (not a re-derived approximation) and ran them
against every `.c` file in the directory, both for remaining
convertible candidates (0, confirming the real `ansify` run left no
simple stragglers) and — critically — for `HDR` matches where
`parse_decl_block` returns **`None`** (the AMS1-documented silent-miss
signature: a K&R candidate that `ansify`'s own report never mentions
at all, zero DRIFT, zero skip).

**Found 10 genuine silent misses**, two distinct parser-gap shapes:

1. **Multi-line C comments whose continuation lines don't start with
   `*`** (`parse_decl_block`'s naive `s.startswith('/*') or
   s.startswith('*')` comment-skip check doesn't validate the comment
   actually closes, so a continuation line like `"s2" must be
   shifted to lower-case` — no leading `*` — breaks the scan): `apt_MM_Compare`
   (identical function defined independently in `zip.c`, `zipprint.c`,
   `zipv.c` — 3 instances) and `zip__Close_Stream` in `zipds00.c` (a
   **real, live class method** — declared in `zip.ch`, not dead code).
2. **A single K&R parameter declaration spanning two physical lines**
   (`register zip_type_pixel *X1, *X2, ...,\n *Y1, *Y2, *Y3;` — no
   semicolon on the first line, so `DECL_LINE` doesn't match and
   `parse_decl_block` bails): `Compute_Handle_Positions` (5 files:
   `zipoarc.c`, `zipocapt.c`, `zipofcap.c`, `ziporect.c`,
   `zipotrap.c`) and `Feather_Points` (`zipoarrw.c`).

All 10 hand-converted (see §7 for `apt_MM_Compare`/
`zip__Close_Stream`'s types; `Compute_Handle_Positions`/
`Feather_Points` use `zip_type_pixel` throughout, a plain `long`
typedef). `zip__Close_Stream` converted per its DB signature
(`struct zip_stream *stream`, matching `zip.ch`'s declaration exactly)
rather than freehand, since it's a real dispatched method.
Re-ran the re-scan after fixing: **0 candidates, 0 None-result
matches** — confirms completeness.

## 7. Per-file `ansify` conversion counts

Counts below are from the real (non-dry-run) `ansify --dir` pass;
"+hand" notes the additional functions this session converted
manually (dead-code skips + completeness-re-scan misses) that
`ansify` itself never touched.

| File | methods | classprocs | helpers | +hand |
|---|---|---|---|---|
| zip.c | 4 | 3 | 7 | +2 (`Show_Statistics`, `apt_MM_Compare`) |
| zipd000.c | 16 | 0 | 12 | |
| zipdf00.c | 27 | 0 | 0 | |
| zipdf01.c | 11 | 0 | 0 | |
| zipdi00.c | 24 | 0 | 6 | |
| zipds00.c | 18 | 0 | 16 | +2 (`Destroy_Stream`, `Close_Stream`) |
| zipds01.c | 4 | 0 | 13 | +2 (`Parse_Stream_Commentary`, `PriorChar` — item 8) |
| zipds02.c | 1 | 0 | 4 | |
| zipedit.c | 8 | 3 | 49 | |
| zipoarc.c | 17 | 0 | 1 | +1 (`Compute_Handle_Positions` — item 9) |
| zipoarrw.c | 19 | 1 | 3 | self-type fix + `Draw_Basic_Style`/`Feather_Points` (item 8/9) |
| zipobj.c | 37 | 1 | 0 | 5 DRIFT stub-widenings |
| zipocapt.c | 18 | 0 | 2 | +1 (`Compute_Handle_Positions`) |
| zipocirc.c | 20 | 0 | 2 | |
| zipoelli.c | 19 | 0 | 2 | |
| zipofcap.c | 13 | 0 | 2 | +1 (`Compute_Handle_Positions`) |
| zipoimbd.c | 14 | 1 | 2 | |
| zipoline.c | 16 | 0 | 2 | |
| zipopath.c | 21 | 0 | 4 | |
| zipoplin.c | 5 | 1 | 0 | self-type fix |
| zipopoly.c | 8 | 0 | 2 | |
| ziporang.c | 8 | 0 | 1 | |
| ziporect.c | 21 | 0 | 1 | +2 (`Object_Attributes`, `Compute_Handle_Positions`) |
| ziposym.c | 8 | 2 | 22 | |
| zipotrap.c | 14 | 0 | 1 | +1 (`Compute_Handle_Positions`) |
| zipprint.c | 15 | 2 | 5 | +1 (`apt_MM_Compare`) |
| zipstat.c | 13 | 2 | 5 | FinalizeObject arity fix (§3) |
| zipv.c | 30 | 3 | 42 | +1 (`apt_MM_Compare`) |
| zipv000.c | 14 | 0 | 2 | |
| zipve00.c | 30 | 0 | 6 | +8 (all dead skips) |
| zipve01.c | 3 | 0 | 4 | |
| zipve02.c | 4 | 0 | 35 | |
| zipve03.c | 1 | 0 | 27 | |
| zipvf00.c | 9 | 0 | 1 | |
| zipvi00.c | 7 | 0 | 5 | +1 (`Within_Which_Image`) |
| zipvp00.c | 39 | 0 | 14 | |
| zipvp01.c | 5 | 0 | 0 | |
| zipvp02.c | 6 | 0 | 2 | |
| zipvp03.c | 8 | 0 | 6 | |
| zipvr00.c | 17 | 0 | 3 | |
| zipvs00.c | 5 | 0 | 0 | +3 (all dead skips) |

## 8. Gate results

- **Subtree-local** (`make -C .../contrib/zip/lib clean/depend/-k
  install`): clean × 2, 0 `error:` lines each time, full install
  completed both times.
- **Tree-wide** (`make -C .../src dependInstall`): clean × 2,
  identical results both runs — exactly 2 `error:` lines, both
  `contrib/zip/utility/ltapp.c:115`/`123`, "incompatible integer to
  pointer conversion passing 'boolean' ... to parameter of type 'void
  *'" — matches the expected known baseline exactly (Wave 7 C2's
  problem). `contrib/zip/lib`'s own build section (lines 17496–17966
  of the log both runs) has 0 `error:` lines both times. Liveness
  confirmed: `building (dependInstall)
  (.../src/contrib/zip/lib)` present in the log both runs.

## 9. `fossil status` / `fossil extras`

`fossil status`: **46 files EDITED, 0 ADDED/DELETED, no commit made**:

```
EDITED src/contrib/zip/lib/Imakefile
EDITED src/contrib/zip/lib/zipedit.h
EDITED src/contrib/zip/lib/zipoarrw.ch
EDITED src/contrib/zip/lib/zipoplin.ch
EDITED src/contrib/zip/lib/zipstat.ch
EDITED src/contrib/zip/lib/{all 41 .c files}
```

(1 Imakefile + 1 installed header + 3 `.ch` + 41 `.c` = 46.)

`fossil extras`, filtered to `src/contrib/zip/lib/`: exclusively
ordinary generated build byproducts (`.o`, `.do`, `.eh`, `.ih`,
`Makefile`, `Makefile.BAK`, `install.time`/`install.doc`, `index`, the
4 `.pcf` fonts). No stray `.ansify-orig` backups or other unexpected
files.

## 10. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g`/`.do` lookup
for live consumers first, never launch GUI apps from this session, no
saves against unversioned fixtures.

```
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/zipobj.do 2>/dev/null | grep -E ' T zipobject__(Print_Object|Highlight_Object_Points|Normalize_Object_Points|Expose_Object_Points|Hide_Object_Points)'
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/zipstat.do 2>/dev/null | grep -E ' T zipstatus__(FinalizeObject|Issue_Status_Message|Acknowledge_Status_Message)'
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/zipoarrw.do 2>/dev/null | grep ' T zipoarrow__InitializeObject'
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/zipoplin.do 2>/dev/null | grep ' T zipoplin__InitializeObject'
```

The zip inset is a known, previously runtime-confirmed-working live
consumer (2026-07-11 session, `project_zip_inset_status` memory) — per
the prompt's guidance, re-exercise the **same established fixtures**
rather than inventing new ones:

- **`Cattey.turnin`** and **`contrib/zip/samples/dragon.zip`** — open
  each in `ez` at default `-O` and confirm the drawing still renders
  correctly (same visual baseline as the 2026-07-11 session). This
  batch's changes are declaration-level (types/prototypes), not
  logic-level, so no rendering difference is expected — the check is
  "does it still look exactly the same," not "does something new
  happen."
- **Specific fixes with plausible runtime exposure**:
  - `zipoarrw.ch`/`zipoplin.ch`'s self-type fixes affect
    `zipoarrow`/`zipoplin`'s `InitializeObject`, which now correctly
    types `self` and can access `self->tolerance` — if either sample
    document contains an **arrow** or **polyline** figure, drawing or
    editing one exercises this path directly (a crash or garbage
    `tolerance`-driven hit-testing behavior on arrow/polyline creation
    would indicate a problem).
  - The 5 `zipobj.c` stub-widenings (`Print_Object`,
    `Highlight_Object_Points`, `Normalize_Object_Points`,
    `Expose_Object_Points`, `Hide_Object_Points`) are abstract-base
    `NULL` stubs — every real figure type overrides them, so these
    stubs are only reached for a figure type with **no override at
    all**. Neither sample fixture is expected to exercise this path;
    flagging as **not independently runtime-verified**, consistent
    with how the equivalent no-op stub fixes were treated in prior
    batches.
  - `zipstat.c`'s `FinalizeObject` fix (the corrected pre-diagnosis,
    §3) fires when a `zipstatus` object is destroyed — normal
    open/close of any zip figure editing session exercises this
    indirectly. No crash expected (the original 1-param body was
    already a no-op; the fix only widens the signature to match the
    real 2-param dispatch convention).
  - All other fixes (item 8's stranded declarations, item 9's
    `Compute_Handle_Positions`/`Feather_Points`/`apt_MM_Compare`/
    `zip__Close_Stream`) are declaration-only or straightforward
    K&R→ANSI folds with unchanged bodies — general non-regression via
    the two sample fixtures above is the appropriate check, not a
    fix-specific one. `apt_MM_Compare` (case-insensitive string
    compare used by `zipprint.c` for PostScript/Troff processor-name
    matching) has no exposure via the `ez` drawing samples at all —
    would need a print-dialog exercise specifically to touch it, which
    isn't among the established fixtures; flagging as **not
    independently runtime-verified**.

## 11. Anything that surprised me / didn't match the prompt's expectations

- **The `zipstatus__FinalizeObject` pre-diagnosis was wrong** (§3) —
  the only one of the 3 pre-ruled DRIFT fixes that didn't hold up under
  direct verification against `class.c`'s own source and 11 real
  precedents elsewhere in the tree. Everything else in the
  pre-diagnosis (the other 2 DRIFT rulings, the dead-code-skip
  classification method, the "8 DRIFT findings" count) held up exactly
  as specified.
- **The skip count (15) and item-8 raw-hit count (161 pre-`ansify`,
  not 90) both came in noticeably higher than pre-diagnosed** — neither
  changes the fix approach, but both are worth noting for calibrating
  future flagged-risky-batch pre-diagnosis estimates on directories
  this size.
- **2 more self-type `.ch` typos** (`zipoarrw.ch`, `zipoplin.ch`),
  same shape as `porting-assessment.md`'s B2 finding 2 / AMS2's
  3-instance cluster, not mentioned in the pre-diagnosis at all — found
  only because the compile gate caught them (a wrong-but-arity-matching
  self type is invisible to `ansify`'s own DRIFT check, which only
  compares argument *count*).
- **`float` as a new promotion-risk type for item 8** (`zipv.c`'s
  `Scale_Pane`) — every prior batch's item-8 findings documented only
  `char`/`short`/`Boolean`; this is the first `float` instance in the
  project's history of this bug class (also subject to default
  argument promotion, to `double`).
- **A block-scope (function-local) stale K&R declaration**
  (`zipofcap.c`'s `Compute_Handle_Positions`... actually
  `Accept_Caption_Character`, redeclared inside `Build_Object`'s own
  body) is a shape not previously documented in the item-8 findings
  narrative — worth a mention if `m3-rollout-runbook.md`'s item 8
  wording gets broadened again.
- **The definitive completeness re-scan (§6) was essential, not
  belt-and-braces** — the runbook flagged this directory as needing it
  "without question," and it earned that: 10 genuine silent misses,
  including one live class method (`zip__Close_Stream`) that would
  otherwise have stayed silently K&R with zero trace in any report.
  Both parser-gap shapes found (unterminated-looking multi-line
  comments, and semicolon-on-a-later-line multi-line declarations) are
  variants of AMS1's already-documented hazard, not new tool bugs.
- **`class`'s single invocation emits both `.ih` and `.eh` together**
  when `CLASSFLAGS` carries `-pe` — worth knowing for future batches:
  a bare `make X.ih` target (as `depend.csh` issues) will also
  regenerate `X.eh` as a side effect once `-pe` is live, but *only* for
  `.ch` files that are themselves stale relative to their `.ih`. This
  meant the first `make depend` after removing all 21 `.eh` files
  only regenerated the one `.eh` whose `.ch` I'd just edited
  (`zipstat.eh`) — I had to force-generate the other 20 `.eh` files
  explicitly (`make zip.eh zipedit.eh ...`) before the real `ansify`
  compile-gate could see them, otherwise the dependency tracking
  (`.depends`, built by `makedepend` scanning `#include "X.eh"`) would
  have silently omitted the prerequisite for any `.eh` that didn't yet
  exist at `depend`-time. Re-running `make depend` afterward correctly
  picked up all 21 as real prerequisites once they existed on disk.

---

**Gate complete. Stopping per the prompt's instructions — no commit
made, no GUI/terminal binary launched. Ready for the orchestrator's
independent re-verification, especially of the corrected
`zipstatus__FinalizeObject` fix (§3) and the 2 new self-type typo
fixes (§5).**
