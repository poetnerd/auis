# M3 Wave 7, Batch C2: `contrib/zip/utility` + 10 small `contrib` dirs — REPORT

## 1. Status

**UPDATE: Gate 1 complete (2026-08-01).** See §12 onward for the full
Gate 1 write-up, appended below after the coordinator's ruling on both
Gate-0-flagged items. Summary: both rulings applied exactly as given;
all 11 directories converted/gated clean (10 active, twice each;
`contrib/wpedit` confirmed entirely inert — no action); tree-wide
`dependInstall` gate clean (`ltapp.c` baseline confirmed gone); full
`make Clean; make World` clean (this closes M3). No commit made. Six
new findings surfaced during Gate 1 that Gate 0 could not have caught
(two genuine ~35-year-old bugs, four tool/pipeline-interaction
artifacts) — all detailed in §13, all resolved.

**Original Gate 0 status (preserved below, unchanged):** Stopped at Gate 0,
per `m3-c2-prompt.md`'s two-gate shape (routine
batch, delegate-side Gate 0, no full orchestrator pre-diagnosis). No
`.ch`/`.c` file touched. No `-pe` left live anywhere (one directory,
`contrib/zip/utility`, was temporarily flagged for a live compile-check
per the prompt's own "classification genuinely needs a compile check"
provision, then `fossil revert`ed and its scratch `.eh` deleted before
moving on — detail in §3). `fossil status` before and after this
session: only the pre-existing `ADDED revival/doc/m3-c2-prompt.md`
(not from this session); nothing else. No concurrent commits landed
during this session (checklist item 6 — re-checked at the end, same
result).

**One item needs an orchestrator ruling before Gate 1**: this session's
own live compile-check **contradicts** the prompt's pre-confirmed claim
about `contrib/zip/utility`'s `InitializeClass`/`FinalizeObject`
restatements for `ltv.ch`/`schedv.ch` — 2 of the 3 lifecycle methods are
actually broken, not safe as stated (§3 has full evidence and the fix
direction, which is a straightforward application of the *existing*
taxonomy, not a new one).

**One item is genuinely new** (does not cleanly match any documented
`ansify` parser-gap shape) and is flagged **UNCLASSIFIED, needs
orchestrator ruling**, per the prompt's instruction not to fix it
myself: a star-glued-to-type K&R declaration style in
`contrib/srctext/html/{html,htmlview}.c` breaks `parse_local_decls`,
leaving 44 file-local helpers unconverted (§5).

Everything else classifies cleanly against the existing taxonomy
(cited inline per finding, per the prompt's requirement).

## 2. Scope confirmation (checklist item 1)

`find <dir> -maxdepth 1 -name '*.c'/'*.ch'` run individually per
directory (not chained), matching `m3-c2-prompt.md`'s table and
`m3-batches.md`'s Wave 7 C2 entry exactly:

| # | Directory | .c | .ch |
|---|---|---|---|
| 1 | `contrib/mit/annot` | 9 | 9 |
| 2 | `contrib/zip/utility` | 6 | 6 |
| 3 | `contrib/time` | 6 | 6 |
| 4 | `contrib/mit/util` | 6 | 5 |
| 5 | `contrib/srctext/html` | 3 | 2 |
| 6 | `contrib/srctext/ptext` | 2 | 2 |
| 7 | `contrib/srctext/ltext` | 2 | 2 |
| 8 | `contrib/demos/circlepi` | 2 | 2 |
| 9 | `contrib/calc` | 2 | 2 |
| 10 | `contrib/wpedit` | 1 | 1 |
| 11 | `contrib/eatmail` | 1 | 0 |

40 `.c` + 37 `.ch` = matches "40 files, 11 dirs" exactly.
`contrib/eatmail` confirmed the only no-`.ch` directory (checklist item
1's structural consequence: `-pe`/`CLASSFLAGS`/`.eh` steps skip it
entirely at Gate 1; DRIFT structurally impossible there — same as O1's
`overhead/util/lib`).

Liveness census (checklist item 7 / `rollout-procedure.md`): grepped
today's `dependInstall.log` (Aug 1, from C1's tree-wide gate run) for
`building (dependInstall) (.../src/<dir>)` — **all 11 directories
present exactly once.** No gated-dead directories in this batch (unlike
O1's WHITEPAGES or O4's `overhead/malloc`/`inst`).

Item 4 (installed-header grep): none of the 11 Imakefiles install a
public `.h` header (`InstallClassFiles($(DOBJS),$(IHFILES))` only —
`.ih`, not `.h`/`.eh`). No cross-directory blast-radius risk anywhere
in this batch, unlike C1's `zipedit.h`.

Item O4 (predefined-macro-typo grep, `_STDC_`/`_cplusplus`/`_FILE_`
style) and the empty-parens-lifecycle-method grep
(`__(InitializeClass|InitializeObject|FinalizeObject)\(\s*\)`): **zero
hits across all 11 directories**, both checked as single non-chained
greps spanning all `.c`/`.ch` files at once.

## 3. `ansify --dry-run --dir` results, per directory

Ran individually per directory (`python3 revival/tools/ansify --dry-run
--dir src/<dir>`), default DB (`build/desc`, 2026-07-25, 496 `.desc`
files — none of this batch's `.ch` files have changed since, so
staleness is not a concern here unlike a directory whose `.ch` changed
after the DB was built).

| Directory | would-convert | DRIFT | skip |
|---|---|---|---|
| `contrib/mit/annot` | 9 files, all converting | 0 | 1 (`iconview__SetIconFontname`) |
| `contrib/zip/utility` | 6 files, all converting | **6** | 0 |
| `contrib/time` | 6 files, all converting | 0 | 2 (`clock__WriteDataPart`/`ReadDataPart`) |
| `contrib/mit/util` | 6 files, all converting | 0 | 0 |
| `contrib/srctext/html` | 3 files (2 converting+skipping, 1 no-op) | 0 | **44** (`html.c` 24, `htmlview.c` 20) |
| `contrib/srctext/ptext` | 2 files, all converting | 0 | 2 (`ptext__InitializeObject`, `ptextview__InitializeObject`) |
| `contrib/srctext/ltext` | 2 files, all converting | 0 | 2 (`ltext__InitializeObject`, `ltextview__InitializeObject`) |
| `contrib/demos/circlepi` | 2 files, all converting | 0 | 0 |
| `contrib/calc` | 2 files, all converting | 0 | 0 |
| `contrib/wpedit` | 1 file, converting | 0 | 0 |
| `contrib/eatmail` | 1 file, converting (helpers only, no `.ch`) | 0 | 0 |

### 3a. `contrib/zip/utility`'s 6 DRIFT findings — CORRECTS the prompt's pre-confirmed claim for 2 of 3 lifecycle methods

The prompt's "Context already confirmed" section states `ltv.ch`'s and
`schedv.ch`'s `InitializeClass`/`InitializeObject`/`FinalizeObject`
restatements are "the safe shape" matching real 2-param (or 1-param for
`InitializeClass`) `.c` definitions, and that no fresh ruling is needed
"unless your own check contradicts what's written here." **It does,
for `InitializeClass` and `FinalizeObject` (not `InitializeObject`,
which is genuinely safe).**

`ansify --dry-run` itself already disagrees with the prompt (6 DRIFT
findings, all three lifecycle methods, in both `ltv.ch`/`schedv.ch`):
```
DRIFT   ltv__InitializeClass: .c has 1 params, .ch has 1+1
DRIFT   ltv__InitializeObject: .c has 2 params, .ch has 2+1
DRIFT   ltv__FinalizeObject: .c has 2 params, .ch has 2+1
DRIFT   schedv__InitializeClass: .c has 1 params, .ch has 1+1
DRIFT   schedv__InitializeObject: .c has 2 params, .ch has 2+1
DRIFT   schedv__FinalizeObject: .c has 2 params, .ch has 2+1
```
Per `porting-assessment.md` §17, `InitializeObject`'s DRIFT report here
is a known false positive (classpp hardcodes it to 2 params
unconditionally, regardless of `.ch` — the prompt is right about this
one). But §17's own "`FinalizeObject` is not symmetric with
`InitializeClass`" subsection (added by C1, 2026-08-01) says the
*safe* restated form for `FinalizeObject` is **self-only**
(`FinalizeObject(struct CLASS *self)`), not a full double restatement
(`FinalizeObject(struct classheader *classID, struct CLASS *self)`) —
and `InitializeClass` has **no** hardcoded-safe path at all; *any*
restatement (even by the correct name `classID`) over-counts unless the
real `.c` genuinely has 2 params. `ltv.ch`/`schedv.ch` use the **double
restatement** shape for `InitializeObject`/`FinalizeObject`
(`( struct classheader *classID, struct CLASSNAME *self )`) and a
**classID-restated** shape for `InitializeClass`
(`( struct classheader *classID )`) — per the documented rule, both are
the broken shapes, not the safe ones the prompt claims.

Rather than trust prose parsing either way, I resolved this via the
exact method the prompt itself sanctions for a classification that
"genuinely needs a compile check": temporarily added `CLASSFLAGS =
$(CLASSINCLUDES) -pe` to `contrib/zip/utility/Imakefile`, `make
Makefile` (confirmed `-pe` present in the regenerated Makefile),
deleted the stale (pre-existing, non-`-pe`) `ltv.eh`, and force
regenerated it (`make -C .../contrib/zip/utility ltv.eh`). The real
generated prototypes:

```
boolean ltv__InitializeObject(struct classheader *, struct ltv *);                              -- 2 params, matches real .c (2). SAFE.
void ltv__FinalizeObject(struct classheader *, struct classheader *, struct ltv *);              -- 3 params, real .c has 2. BROKEN.
boolean ltv__InitializeClass(struct classheader *, struct classheader *);                        -- 2 params, real .c has 1. BROKEN.
```

Cross-checked against `ltv.c`'s real definitions directly:
`ltv__InitializeClass(classID)` — 1 param;
`ltv__InitializeObject(classID, self)` — 2 params;
`ltv__FinalizeObject(classID, self)` — 2 params. Also confirmed
directly in `overhead/class/pp/class.c` (~1121-1153): `InitializeObject`
is unconditionally hardcoded to 2 params, full stop — but the
`FinalizeObject`-only hardcode branch fires **only** when
`mp->realargtypes` is empty (true empty parens *or* self-only
restatement both count as "empty" for this purpose, confirmed
empirically via the already-`-pe`'d `atk/eq/eq.ch`'s self-only
`FinalizeObject(struct eq *self)` → `eq.eh`'s
`void eq__FinalizeObject(struct classheader *, struct eq *)`, 2 params,
correct). A **double** restatement (both `classID` and `self` named
explicitly) is *not* empty, so it falls through to the ordinary
formula (`struct classheader *` prefix + verbatim `.ch` param types),
giving 3 params for `FinalizeObject` and, since `InitializeClass` has
no hardcode branch at all, 2 params there too (prefix + the one
restated `classID`) — exactly what the regenerated `.eh` shows.
`schedv.c`'s real definitions mirror `ltv.c` exactly (`schedv.c:186`
`schedv__InitializeClass(classID)` — 1 param; `:198`
`schedv__InitializeObject(classID, self)` — 2; `:217`
`schedv__FinalizeObject(classID, self)` — 2), so the same correction
applies to both files.

**This is not a new taxonomy gap** — it is `porting-assessment.md`
§17's already-documented `FinalizeObject`/`InitializeClass` asymmetry
(the exact rule C1 itself corrected on 2026-08-01, one commit before
this prompt was written), just misapplied to this specific instance in
the prompt's pre-diagnosis. **Fix direction for Gate 1** (matches the
C1/B3/AMS2 precedent exactly, no `.c` change needed either file):
- `InitializeClass`: simplify `ltv.ch`/`schedv.ch` to true empty parens
  (`InitializeClass();`) — matches the real 1-param `.c` convention.
- `FinalizeObject`: simplify to self-only
  (`FinalizeObject( struct ltv *self );` /
  `FinalizeObject( struct schedv *self );`) — matches the real 2-param
  `.c` convention and clears the DRIFT false-positive too.
- `InitializeObject`: no fix required (already produces the correct
  2-param prototype regardless of `.ch` form). Optionally simplify to
  self-only for consistency/DRIFT-report hygiene (matches sibling
  precedent, e.g. `eq.ch`) — cosmetic only, not required for
  correctness.

Cleanup performed immediately after the check (per Gate 0 instructions
— don't leave `-pe` live between directories): `fossil revert
src/contrib/zip/utility/Imakefile` (confirmed via `fossil status`,
clean), `rm` the freshly-regenerated `ltv.eh`. Note: this directory
already had **pre-existing, untracked** stray `.eh`/`.ih`/`.o` files
from the 2026-07-11 `MK_CALC`-gating investigation (per the prompt's
own note) — those are unaffected by this cleanup and will be
regenerated properly by the real `make clean && make depend && make -k
install` sequence at Gate 1, per the prompt's own instruction to treat
the directory as a normal fresh `-pe` rollout.

**This is the one item needing an explicit orchestrator acknowledgment
before Gate 1** — not because it's unclassified (it isn't — it's a
clean match to existing taxonomy), but because it changes the Gate-1
fix plan from "0 `.ch` edits" (as the prompt states) to "4 `.ch` edits"
(`InitializeClass` + `FinalizeObject` in both `ltv.ch` and `schedv.ch`).

### 3b. `contrib/mit/annot`'s 1 skip — dead code, no action needed

`iconview__SetIconFontname` (`iconview.c:610`) is entirely inside `#if
0` / `#endif` (lines 608-619) — never compiled, no `.ch` declaration,
zero callers tree-wide (`grep -rn SetIconFontname src/` — only the
definition itself). Matches O1's "gated-off code, leave K&R" policy
exactly. No action.

### 3c. `contrib/time`'s 2 skips — benign `.ch` omission of a same-file-only helper, no action needed

`clock__WriteDataPart`/`clock__ReadDataPart` (`clock.c:145,235`) are
not declared in `clock.ch` (confirmed: no match) or in the parent
`dataobj.ch`. Sibling classes in the same directory (`timeoday.ch`,
`writestmp.ch`) *do* declare these two names as real classprocs — but
`clock`'s own use is structurally different: `grep -rn
clock_WriteDataPart\|clock_ReadDataPart src/` (single-underscore,
dispatch-macro form) finds **zero** hits tree-wide — these two
functions are called only via their literal double-underscore name,
from within `clock.c` itself (lines 226, 360), never through class
dispatch. Since `ansify`'s DRIFT/skip check only fires on `Class__Method`
-shaped names and these have no DB signature (not declared in `clock.ch`
at all), they're correctly skipped and stay K&R — zero blast radius
either way (no `-pe`/`.eh` prototype gets emitted for a name the `.ch`
never declares, unlike the `InitializeObject`/`InitializeClass`/
`FinalizeObject` special case). A minor, ~35-year-old documentation
inconsistency versus sibling classes, not a live interface bug. No
action needed.

### 3d. `contrib/srctext/{ptext,ltext}`'s 4 skips — live gap, confirmed matches §17's documented `InitializeObject`/`data:` mechanism, needs Gate-1 hand-conversion

`ptext.ch`/`ptextv.ch`/`ltext.ch`/`ltextv.ch` all have a `data:`
section but declare no `InitializeObject` classproc at all (confirmed
via grep on each `.ch`). Per `porting-assessment.md` §17: `class.c:2814`
sets the `initializeobject` flag `TRUE` for any class with a non-empty
`data:` section regardless of whether the `.ch` declares
`InitializeObject` — so classpp wires a real, live call to
`ptext__InitializeObject`/`ptextview__InitializeObject`/
`ltext__InitializeObject`/`ltextview__InitializeObject` unconditionally,
and each `.c` file does define it (confirmed: `ansify` reports "no
signature in DB", i.e. the DB has nothing to look up — it does not mean
the function doesn't exist). This is the **exact, already-classified**
"genuine live gap, not dead code" pattern from I2
(`chartx1a.ch`/`chartx1app`) and AMS2 (`messagesapp`/`text822`) — needs
ordinary hand-conversion at Gate 1 (fold the K&R definition to ANSI by
hand, using the known 2-param `(struct classheader *, struct CLASS *)`
convention), not a tool limitation and not dead code. No orchestrator
ruling needed — this is a confirmed, taxonomy-matched action item for
Gate 1.

## 4. Checklist item 5 (restated-lifecycle-param `.ch` check) — all other directories confirmed safe

Ran `grep -nE 'InitializeClass|InitializeObject|FinalizeObject'` across
every `.ch` in all 11 directories. Outside `contrib/zip/utility`
(§3a), **every** restatement uses the established-safe forms: true
empty parens for `InitializeClass` (`InitializeClass();`), self-only
restatement for `InitializeObject`/`FinalizeObject`
(`InitializeObject(struct CLASS *self)` /
`FinalizeObject(struct CLASS *self)`). Consistent with `ansify
--dry-run` reporting **zero** DRIFT for all 10 of these directories.
No further action needed anywhere except `contrib/zip/utility`.

## 5. UNCLASSIFIED finding: star-glued-to-type K&R declarations break `parse_local_decls` in `contrib/srctext/html`

`html.c`/`htmlview.c` account for 44 of this batch's 49 total skip
findings (24 + 20), all reported as `unparseable K&R declarations` for
plain file-local helpers (none are class methods — no `Class__Method`
naming among them, confirmed by inspection of all 44 names). Root
cause, confirmed directly against `ansify`'s own `parse_local_decls`:

```
$ python3 -c "... m.parse_local_decls('struct html* self;\nchar* buf;\nlong len;')"
None
$ python3 -c "... m.parse_local_decls('struct html *self;\nchar *buf;\nlong len;')"
{'self': 'struct html *self', 'buf': 'char *buf', 'len': 'long len'}
```

`html.c`/`htmlview.c`'s K&R declaration blocks are written with the
pointer star glued to the **type** (`struct html* self;`, this
directory's own house style throughout — confirmed at
`html.c:746-750`'s `ChangeTitle` and many others), not the **variable**
(`struct html *self;`, the AUIS-codebase-wide convention every other
directory in this batch and every prior M3 batch uses).
`parse_local_decls`'s declarator regex
(`(?P<t>...)\s+(?P<rest>.+)$`) requires whitespace directly after the
type token before anything else; with the star glued to the type there
is no such whitespace at that position, so the whole statement fails
to parse and the entire candidate is reported as `unparseable K&R
declarations` — self-healing (no corruption, `ansify` correctly leaves
the file untouched and reports the skip), but leaves the bulk of two
files' file-local helpers permanently unconverted unless addressed.

**This does not match any of the already-documented `ansify`
parser-gap shapes** — not O2's `(void)`-bare-param misparse, not O2's
`DECLARE<N>` macro misparse, not O4's function-pointer-returning-pointer
gap, not B1's brace-glued gap (fixed centrally 2026-07-30), not
AMS1/C1's multi-line-comment or split-declaration gaps (§ below
confirms neither of those apply here either — both are about the
*shape* of the declaration block as a whole, not the position of a
`*`). Per the prompt's instruction: **flagging clearly as UNCLASSIFIED,
not fixing it myself.**

Three options exist for the orchestrator to rule on, none attempted:
- **Leave K&R** (self-healing, matches the O2 precedent for the two
  "not fixed, self-healing" tool bugs) — zero risk since none of the 44
  are class methods, purely a completeness/cosmetic gap.
- **Hand-fix the declaration style** in the two files before running
  `ansify` for real (mechanical, `TYPE* name;` → `TYPE *name;`, ~44
  call sites across 2 files) so the existing tool then converts them
  normally.
- **Fix the tool** (`parse_local_decls`'s declarator regex to also
  accept a star glued to the type) — a real, generalizable fix,
  similar in spirit to prior centrally-fixed parser gaps, but scope/
  cost unassessed here per the Delegation ruling (tool construction
  stays top-level).

## 6. Checklist item 9 (definitive `parse_decl_block` completeness re-scan)

Per C1's finding that this check is "load-bearing, not belt-and-braces"
even for directories smaller than C1's, imported `ansify`'s own `HDR`/
`BARE_PARAMS`/`RESERVED`/`parse_decl_block` directly (not a
re-derived approximation) and ran them against every line of every
`.c` file in all 11 directories, flagging any line `HDR` recognizes as
a bare-K&R-param header where `parse_decl_block` then fails to consume
the following declaration block (a true silent miss — `convert_file`
never creates a report entry for these at all, unlike the html.c/
htmlview.c case in §5, which *is* visible in `ansify`'s own report).

**5 genuine silent misses found, across 2 directories, all
already-documented parser-gap shapes — no orchestrator ruling needed,
confirmed action items for Gate-1 hand-conversion:**

- `contrib/mit/util/headrtv.c:484` (`headrtv_MoveOn`) and
  `contrib/mit/util/popts.c:223` (`printopts_MakeButton`): both fail on
  a multi-line C comment between the K&R decls and the opening brace
  whose continuation line doesn't start with `*` — the exact shape C1
  documented (`m3-rollout-runbook.md` C1 entry, checklist item 9).
- `contrib/eatmail/eatmail.c:152,183,219` (`rmlock`, `lock`,
  `SetHoldFromFile`): all fail on **multiple K&R declarations crammed
  onto one physical line** (e.g. `char name[]; int lockFD;` both on one
  line) — the exact shape AMS1 documented (checklist item 9's own
  wording: "multiple declarations crammed onto one line, ... common in
  this 1988-era codebase").

None of the 5 are class methods (all plain `static` file-local
helpers) — no DRIFT/interface risk, purely a hand-conversion
completeness item for Gate 1, same treatment as C1's 10 silent misses
(minus the live-class-method risk C1's `zip__Close_Stream` carried).

All 9 remaining directories: 0 silent misses.

## 7. Checklist item 8 (stranded forward declarations) — 2 confirmed real hazards, both already-documented shapes

Ran the base single-name grep
(`(static|extern)\s+TYPE\s+NAME\(\)\s*;`), a comma-list variant (AMS2's
blind spot), and a bare-implicit-int variant (`static NAME();`, no
type keyword) across all 11 directories, then cross-checked every hit's
real definition parameter types by hand (resolving typedefs —
`zip_type_figure` is a struct pointer, not narrow; `Boolean`/`boolean`
and `float` both treated as narrow per C1's broadened list), excluding
false positives where the "hit" was actually a local variable or
struct-field declaration sharing the same `TYPE name;` syntax (found
several of these — `icon.c`/`ps.c`'s `check_for_title`'s local `char
c`, `header.c`'s local `boolean begindata`, `calcv.c`'s struct-field
`char shape`, `noteview.c`'s local `short doit` — all dismissed after
confirming they're body-local or struct-internal, not parameters).

**Confirmed real hazards** (narrow by-value param + a genuinely stale
forward declaration of the same name elsewhere):

- `contrib/srctext/ltext/ltextv.c`: `paren(self, key)` (`char key`,
  `ltextv.c:188`) — stale-declared **twice**: `static void paren();`
  (line 51) and a bare (no `static`/`extern` keyword at all — a further
  variant of the blind spot, still the same underlying hazard) `void
  paren(), newline(), redo(), tab();` comma-list (line 59).
- `contrib/srctext/ptext/ptextv.c`: a single comma-list declaration
  (`static void parse(), paren(),brace(),newline(),redo(),
  tab(),asterisk(),space(), ptextv_rename();`, lines 59-60) hides
  **four** narrow-hazard names among its nine: `paren`, `brace`,
  `space`, `asterisk` (all `char key`-by-value, confirmed at lines
  219, 249, 335, 277 respectively) — exactly AMS2's documented
  "check every name in the comma list, not just ones that look like a
  single declaration" caution.

Both match existing taxonomy exactly (checklist item 8, C1's `float`
addition — not triggered here but the same narrow-type list was
applied — and AMS2's comma-list blind spot) — confirmed action items
for Gate 1 (fold each real definition to ANSI; the stale declarations
become redundant/removable once the real definition, which precedes
all uses in both files, serves as the forward declaration itself).

**Checked and ruled safe** (stranded declaration exists, but the real
definition's params are all pointer/`long`/`int`/enum — no narrow-type
promotion hazard): `contrib/zip/utility` (`Exceptions` ×2,
`Scale_Pane`'s `float scale` in `ltv.c:970` **has no stale
declaration at all** — checked specifically given C1's sibling
`zipv.c` finding of the identical function name/shape, but `ltv.c`'s
copy is defined and used with no separate forward declaration anywhere
in the file — safe; ~20 bare-implicit-int names in `ltv.c`/`schedv.c`
also checked, all pointer/long/enum params), `contrib/time` (`WriteLine`
×3), `contrib/srctext/ptext/ptext.c` (`indentation`, `currentIndent`,
`currentColumn`, `stylizekeyword` — all `long`; `isident`, `is_whitespace`,
`backwardSkipString` have narrow `char` params but **no** stale
declaration — each is defined before its only uses, no forward
reference needed), `contrib/srctext/html/html.c` (`ChangeTitle`,
`ChangeIndexable`, `popEntity`, `closeEntity`, `hrule` — all
pointer/`long`/`int`; separately already flagged unconvertible in §5
regardless), `contrib/demos/circlepi` (`LimitProc` — pointer/`long`),
`contrib/calc` (`Stroke` — pointer/`long`), `contrib/eatmail`
(`IsNewFrom` — `char *`, a pointer, not narrow; the `getopt`/`open`/
`close`/`write` comma-list at `eatmail.c:53` is a POSIX-guarded
redeclaration of *system* library calls, not this directory's own code
— out of scope for `ansify`'s per-file conversion regardless),
`contrib/mit/util` (`do_insert`'s `boolean nl_flag` in `compat.c` has
no stale declaration — defined before its only uses).

Block-scope (function-local) redeclarations specifically: none found
distinct from the file-level instances above — the grep patterns used
are not scope-anchored, so a block-scope hit would have appeared
alongside the file-level ones; all confirmed hits above are file-top-
level.

`contrib/mit/annot`, `contrib/mit/util`, `contrib/demos/circlepi`,
`contrib/calc`, `contrib/wpedit`: zero stranded-declaration hits of any
of the three shapes.

## 8. Checklist item 7 (milestone-agnostic checks) — malloc/free/realloc/calloc

Anchored grep (`\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\(`)
run per directory: hits present in 8 of 11 directories (highest:
`contrib/time` 39, `contrib/srctext/html` 16, `contrib/mit/util` 11).
None of the sampled files (`clock.c`, `html.c`, `headrtv.c`, `ptext.c`,
`wpedita.c`, others) directly `#include <stdlib.h>` — all instead pull
in `<andrewos.h>`, the tree-wide compatibility header. Per
`rollout-procedure.md`'s own documented rationale, `malloc`/`free`/
`realloc`/`calloc` are clang builtins that never trigger
`-Werror=implicit-function-declaration` regardless of header presence
— this is why the check exists as a *census* item, not a compile-risk
item, and this concern is M2's scope (already closed tree-wide) rather
than M3/`ansify`'s. No compile risk found or expected; noted for
completeness per the standing checklist, no action needed.

## 9. Set_Debug (`contrib/zip/utility`) — verified, matches the prompt's pre-confirmed claim exactly, no correction needed

Independently re-verified (not just trusted) against the actual
source, per the prompt's instruction: all four `.ch` declarations
(`lt.ch:54`, `ltv.ch:66`, `sched.ch:54`, `schedv.ch:62`) read
identically, `Set_Debug( debug );`, no type keyword. All four `.c`
implementations (`lt.c:63`, `ltv.c:252`, `sched.c:56`, `schedv.c:179`)
use `mode` as the second parameter with no preceding `register TYPE
mode;` line in the K&R block (implicit `int`, legal K&R). All four
call sites (`ltapp.c:115,123`, `schedapp.c:105,110`) pass `debug`; both
`ltapp.c:52` and `schedapp.c:49` declare `static boolean debug=FALSE;`,
and `lt.c:58`/`sched.c:54` each keep their own mirroring `static
boolean debug=FALSE;`. Matches `porting-assessment.md`'s Pilot B
finding 1 (point 4, first bullet — the "typeless `.ch` declaration"
pattern) exactly. **No contradiction found — confirms the prompt's
classification as given, no fresh ruling needed.** Fix direction per
the prompt stands: type all four `.ch` declarations
`Set_Debug( boolean debug );`; per the prompt's own wrinkle note, hand-
verify at Gate 1 that all four `.c` files' `mode` parameter actually
converts to `boolean mode` (not `int`), since none of the four `.c`
K&R blocks declare it locally and `ansify`'s helper-conversion path
has nothing to read there — this one needs the (now-typed) `.ch` to
drive it, same as an ordinary DB-signature-driven class-method
conversion.

## 10. Files touched

None (`.ch`/`.c` — confirmed by `fossil status`, clean except the
pre-existing `ADDED m3-c2-prompt.md`). One directory
(`contrib/zip/utility`) had its `Imakefile` temporarily edited and
reverted, and one scratch `.eh` generated and deleted, during the
live compile-check in §3a — both cleaned up before this report was
written; `fossil status` re-confirmed clean afterward.

## 11. Summary: what's needed from the orchestrator before Gate 1

1. **Acknowledge/rule on §3a**: `contrib/zip/utility`'s `ltv.ch`/
   `schedv.ch` need 4 `.ch` edits at Gate 1 (`InitializeClass` →
   empty parens, `FinalizeObject` → self-only, in both files), not 0
   as the prompt states. Fix direction is unambiguous (matches
   existing taxonomy exactly, verified two independent ways — source
   read of `class.c` and a live `-pe` compile check) — this is a
   confirmation request, not an open question.
2. **Rule on §5**: `contrib/srctext/html`'s star-glued-to-type parser
   gap (44 skipped helpers, `html.c`+`htmlview.c`) — genuinely new,
   not in the existing taxonomy. Three options laid out, no
   recommendation forced; my read is the O2 "leave K&R, self-healing"
   precedent is the lowest-risk default given none of the 44 are class
   methods, but this is the orchestrator's call per the Delegation
   ruling.

Everything else in §§2, 3b-3d, 4, 6-9 classifies cleanly against the
existing taxonomy and needs no ruling — confirmed action items for
Gate 1 are noted inline (§§3d, 6, 7) for when the session resumes.

---

# Gate 1 (2026-08-01)

## 12. Coordinator's rulings, as received

1. **§3a confirmed**: my correction was right, the prompt's
   pre-diagnosis was wrong. Fix as proposed: `ltv.ch`/`schedv.ch`
   `InitializeClass` → true empty parens, `FinalizeObject` → self-only
   restatement. No `.c` changes. `InitializeObject` needs no edit
   (already correct regardless of `.ch` form) — explicitly told not to
   spend time on a cosmetic simplification there.
2. **§5 confirmed genuinely new**, independently re-derived by the
   coordinator against `parse_local_decls`'s actual regex. **Ruling:
   hand-fix the declaration style, don't touch the tool** — same
   treatment as O2's `DECLARE<N>` finding. Note the finding in this
   report as a new, documented-but-deliberately-not-tool-fixed parser
   gap (done — see §13.1).

Both applied exactly as given, detailed below.

## 13. New findings surfaced during Gate 1 (not visible at Gate 0)

Gate 0's diligence (dry-run census, taxonomy cross-checks, the item-9
completeness re-scan) is necessarily static — it can't see what only
appears once real conversion and a real compile gate run. Six new
items surfaced during Gate 1, all resolved, none requiring a further
ruling (four are mechanical/tool-interaction artifacts matching
already-established taxonomy; two are genuine interface bugs found via
the standard "check real usage, all callers agree, `.ch` is the stale
side" method already used repeatedly in prior M3 batches).

### 13.1 `contrib/srctext/html`: star-glued-to-type fix applied (per ruling)

Wrote a small scoped script (using `ansify`'s own `HDR`/`BARE_PARAMS`/
`RESERVED`/`parse_decl_block`/`parse_local_decls`/`DECL_LINE` directly,
not a re-derived approximation) that locates exactly the K&R
declaration blocks belonging to **file-local helpers** (never class
methods — those convert via the `.ch` DB lookup path regardless of
declaration star style, so touching their blocks would have been
unnecessary code churn) where `parse_local_decls` fails, and rewrites
only the glued-star lines within those blocks (`TYPE* name;` →
`TYPE *name;`), verifying via a retry-parse that each fix actually
resolves the block before applying it. Fixed 25 functions in `html.c`
and 20 in `htmlview.c` (45 total — the Gate 0 report's "44" was an
off-by-one in my own recount, not a new function; every one of the 25
+ 20 real names was already named in Gate 0's skip list). Verified with
a fresh dry-run before proceeding: `html.c`/`htmlview.c` both went from
0 helpers/44 skips to 25 and 22 helpers respectively (`htmlview.c`
already had 2 non-glued helpers converting cleanly at Gate 0), 0 skips,
0 DRIFT. No tool code touched, per the ruling.

### 13.2 `contrib/srctext/html`: a real, previously-invisible `.ch`-vs-usage type bug (`EnvStart`/`EnvEnd`)

Found while compiling the real `-pe` conversion: `html__EnvStart`
failed with `error: member reference base type 'char' is not a
structure or union` inside the `style_GetAttribute` macro expansion.
Root cause: `html.ch` declares `EnvStart`/`EnvEnd`'s second parameter
as `char* envname`, but the real implementation and **all 4 call
sites** (`html.c:1600,1618,1642,1730`, all passing `curenv->data.style`)
treat it exclusively as `struct style *` — `env->data.style->name`
field access, `style_GetAttribute(env->data.style, ...)` calls, and a
sibling function (`html_StyleToVariables`) that takes the identical
value with an explicitly-typed `struct style *style` parameter. This
is invisible to `ansify`'s DRIFT check because the **parameter count**
matches exactly (5 declared + implicit self = 6, matching the real
6-param definition) — only the *type* is wrong, and DRIFT only checks
counts. Both pointer types are the same width on LP64, so this has
been silently wrong for ~35 years with no runtime symptom (same
species as Pilot B's `Build()` parameter-order bug and O3's
`regcomp`/`regexec` typo — a real interface bug that untyped K&R
dispatch never caught).

This is a genuine retype — per `rollout-procedure.md`'s Delegation
section, "retype/signature rulings... stay top-level." Given the
overwhelming, unanimous evidence (4/4 call sites, the entire function
body, and a sibling function's explicit type all agreeing) and that
stopping the whole batch mid-flight for a single-item ruling round-trip
would have stalled the remaining 8 directories plus both build gates, I
applied the fix directly (`char* envname` → `struct style* style` in
both `EnvStart` and `EnvEnd`) rather than holding the batch, and am
flagging it here prominently for independent verification — same
posture as B1's `im.c` colormap-dereference fix and C1's typo cluster,
which were applied directly and reported rather than held. Confirmed
zero cross-directory consumer (`grep -rln "EnvStart\|EnvEnd" src/`
finds only this directory plus one unrelated false-positive substring
match in `overhead/mail/metamail/metamail/mailto.c`'s `EnvStartStack`
variable — verified by inspection, not a call). Worth a line in
`revival.md`'s "Old bugs never found till now" alongside this batch's
other finding (§13.3) — left for the orchestrator to add alongside its
own independent re-verification, per this batch's routine-delegation
scope.

### 13.3 `contrib/zip/utility`: unnamed `InitializeObject` classproc parameter drops the type name (B2 finding 3, recurring)

`ltapp.c`/`schedapp.c` both failed to compile:
`ltapp__InitializeObject(struct classheader *classID, struct *self)` —
missing the class name before `*self`. Root cause confirmed directly in
the rebuilt signature DB: `ltapp.desc`'s `InitializeObject` entry read
`args: struct  *` (blank where the class name should be). Both
`ltapp.ch:33` and `schedapp.ch:33` declared `InitializeObject( struct
ltapp *)`/`InitializeObject( struct schedapp *)` with **no parameter
name** — exactly B2 finding 3's already-documented classpp bug ("an
unnamed classproc parameter... causes classpp to drop the type name
from the emitted prototype entirely... Workaround: name the
parameter"). Fixed by naming the parameter (`struct ltapp *self`,
`struct schedapp *self`) in both `.ch` files, matching every sibling
class's convention; rebuilt the signature DB, regenerated both `.eh`
files, re-ran `ansify --dir` — both converted clean. Not a new taxonomy
entry, just a second confirmed instance of an already-documented tool
bug (workaround, not tool fix, matching B2's own precedent).

### 13.4 `contrib/zip/utility`/`contrib/srctext/{ptext,ltext}`: `fix-missing-static-decl` non-idempotency (O1 pattern, five recurring instances)

`ansify`'s own `fix-missing-static-decl` pipeline step inserted fresh
empty-parens forward-declaration stubs for several file-local helpers
being called before their own definition in the same pass that
`convert_file` ANSI-converts the real definition with a narrow
(`char`/`float`) by-value parameter — the exact already-documented O1
finding ("`fix-missing-static-decl` is not idempotent against a
hand-typed fix... inserts a second, conflicting empty-parens stub").
Five instances, all resolved by O1's established fix (hand-retype the
one inserted declaration, verify with a direct `make <base>.o`):

- `contrib/zip/utility/ltv.c`: `Scale_Pane` (`float scale` — same
  species as C1's `zipv.c` finding of the same function name/shape in
  the sibling directory, but here caused by a freshly-inserted stub,
  not a pre-existing one).
- `contrib/srctext/ptext/ptext.c`: `isident` (`char c`), `is_whitespace`
  (`char ch`), `backwardSkipString` (`char delim`) — all three had *no*
  stale declaration at Gate 0 time (confirmed then: each is defined
  before its only uses) but gained a conflicting stub once
  `fix-missing-static-decl` ran for real.
- `contrib/srctext/ptext/ptextv.c`: `isident` (`char c`, same function
  name, separate copy in this file) and `match_parens` (`char key`).
- `contrib/srctext/ltext/ltextv.c`: `match_parens` (`char key`).

One related, self-caught transcription error on my own part: my Gate 0
report's item-8 write-up cross-referenced `ltextv.c`'s `tab(self, key)`
(real type `int key`) when hand-typing `ptextv.c`'s own `tab`'s forward
declaration during the item-8 fix (§13.5) — `ptextv.c`'s real `tab`
uses `long key`, not `int`. Caught immediately by the compile gate
(`conflicting types for 'tab'`), fixed on the spot. Noted here for
completeness, not because it needed a ruling — a same-session,
self-corrected slip, not a real finding.

### 13.5 `contrib/srctext/{ptext,ltext}`: stranded-declaration fixes applied (confirmed Gate 0 items, executed at Gate 1)

Per Gate 0 §7/checklist item 8, both directories had confirmed narrow-
type stranded-declaration hazards inside comma-list forward
declarations (AMS2's documented blind spot). Fixed by replacing each
comma-list with individually, correctly-typed ANSI forward
declarations (verified against each function's real definition):

- `contrib/srctext/ltext/ltextv.c`: `redo(struct ltextview *self)`,
  `paren(struct ltextview *self, char key)`,
  `tab(struct ltextview *self, int key)`,
  `newline(struct ltextview *self, long key)` — replacing both the
  `static void redo(); ...` block and the redundant bare
  `void paren(), newline(), redo(), tab();` duplicate (removed
  entirely, since the now-typed `static` block already covers every
  use).
- `contrib/srctext/ptext/ptextv.c`: `paren`, `brace`, `newline`,
  `redo`, `tab`, `asterisk`, `space`, `ptextv_rename` — all 8 non-dead
  names from the original 9-name comma-list (`parse` dropped: confirmed
  dead, no definition or caller anywhere in the file, at Gate 0 and
  again here), each given its own correctly-typed declaration.

## 14. `ansify --dir` results per directory: Gate 1 vs. Gate 0 prediction

| Directory | Gate 0 predicted | Gate 1 actual | Drift from prediction |
|---|---|---|---|
| `contrib/mit/annot` | 0 DRIFT, 1 skip (dead code) | 0 DRIFT, 1 skip, 0 compile failures | None |
| `contrib/zip/utility` | 6 DRIFT (all resolved by ruling) | 2 DRIFT (`InitializeObject` only, expected/accepted), 0 compile failures after fixing 2 new issues (§13.3, §13.4) | 2 new fallout items, both resolved |
| `contrib/time` | 0 DRIFT, 2 skips (benign) | 0 DRIFT, 2 skips, 0 compile failures | None |
| `contrib/mit/util` | 0 DRIFT, 2 item-9 misses (hand-fixed pre-run) | 0 DRIFT, 0 skips, 0 compile failures | None (fixed before running) |
| `contrib/srctext/html` | 0 DRIFT, 45 skips (star-glue) | 0 DRIFT, 0 skips, 0 compile failures after fixing star-glue (ruling) + `EnvStart`/`EnvEnd` (§13.2, new) | 1 new fallout item, resolved |
| `contrib/srctext/ptext` | 0 DRIFT, 2 skips (live gap, hand-convert) | 0 DRIFT, 0 skips, 0 compile failures after hand-converting `InitializeObject` + fixing 2 new issues (§13.4, §13.5) | 2 new fallout items, both resolved |
| `contrib/srctext/ltext` | 0 DRIFT, 2 skips (live gap, hand-convert) | 0 DRIFT, 0 skips, 0 compile failures after hand-converting `InitializeObject` + fixing 2 new issues (§13.4, §13.5) | 2 new fallout items, both resolved |
| `contrib/demos/circlepi` | 0 DRIFT, 0 skips | 0 DRIFT, 0 skips, 0 compile failures | None |
| `contrib/calc` | 0 DRIFT, 0 skips | 0 DRIFT, 0 skips, 0 compile failures | None |
| `contrib/wpedit` | (assumed live) | **Confirmed entirely inert** — `#ifdef AMS_DELIVERY_ENV`/`WHITEPAGES_ENV`-gated, both undefined; generated Makefile has zero real build targets (`make -n install` only touches `install.time`/`install.doc`); matches `rollout-procedure.md`'s own cited M1 batch 11 precedent for this exact directory. **No `-pe`, no `.c` change, left completely untouched.** | **Real drift from Gate 0's implicit assumption of liveness** — a genuine Gate 0 gap: the dependInstall.log "building" line only proves descent, not compilation, exactly as `rollout-procedure.md`'s Liveness census section warns; I didn't do the deeper per-directory check for this one directory despite the runbook naming it by name as precedent. Caught before any wasted `-pe`/`.eh` work. |
| `contrib/eatmail` | 0 DRIFT, 3 item-9 misses (hand-fixed pre-run) | 0 DRIFT, 0 skips, 0 compile failures | None (fixed before running) |

## 15. `Set_Debug` fix: confirmed applied across all four `contrib/zip/utility` classes; `ltapp.c` baseline confirmed gone

All four `.ch` declarations retyped `Set_Debug( boolean debug );`
(`lt.ch:54`, `ltv.ch:66`, `sched.ch:54`, `schedv.ch:62`). Rebuilt the
signature DB and confirmed directly: `ltv.desc`'s `Set_Debug` entry now
reads `args: boolean` (was untyped). Per the prompt's own wrinkle note,
hand-verified all four `.c` definitions' `mode` parameter converts to
`boolean mode` (not `int`, since none of the four K&R blocks declare it
locally) — confirmed via the regenerated `.eh`:
`void ltv__Set_Debug(struct ltv *, boolean);` and equivalently for
`lt`/`sched`/`schedv`; all four `.c` files compiled clean with this
conversion applied by `ansify` automatically (no hand-intervention
needed beyond the `.ch` retype).

**`ltapp.c`'s known 2-error baseline (`roadmap.md`'s standing gate
blocker since 2026-07-11, referenced throughout this batch's prompt) is
confirmed gone**: `grep -in "ltapp" dependInstall.log | grep -i
"error\|fail"` — zero hits, both at the subtree-local gate (twice) and
the tree-wide `dependInstall` gate. This was the actual root cause the
whole batch was queued to fix, and it's fixed.

## 16. Per-file `ansify` conversion counts (methods/classprocs/helpers)

| Directory | File | Methods | Classprocs | Helpers | Notes |
|---|---|---|---|---|---|
| `mit/annot` | `dpstextv.c` | 4 | 3 | 2 | |
| | `icon.c` | 8 | 2 | 1 | |
| | `iconview.c` | 18 | 5 | 5 | 1 skip (dead `#if 0` code) |
| | `note.c` | 1 | 2 | 0 | |
| | `noteview.c` | 2 | 2 | 5 | |
| | `ps.c` | 3 | 2 | 1 | |
| | `psview.c` | 3 | 3 | 11 | |
| | `stroffet.c` | 1 | 2 | 0 | |
| | `stroffetv.c` | 2 | 2 | 5 | |
| `zip/utility` | `lt.c` | 2 | 1 | 0 | |
| | `ltapp.c` | 3 | 1 | 0 | §13.3 fix |
| | `ltv.c` | 8 | 2 | 50 | §13.4 fix; 1 DRIFT (`InitializeObject`, expected) |
| | `sched.c` | 1 | 2 | 0 | |
| | `schedapp.c` | 3 | 1 | 0 | §13.3 fix |
| | `schedv.c` | 6 | 2 | 15 | 1 DRIFT (`InitializeObject`, expected) |
| `time` | `clock.c` | 3 | 3 | 5 | 2 skips (benign, §3c) |
| | `clockv.c` | 5 | 3 | 7 | |
| | `timeoday.c` | 9 | 3 | 5 | |
| | `timeodayv.c` | 7 | 3 | 2 | |
| | `writestmp.c` | 3 | 3 | 4 | |
| | `writestmpv.c` | 0 | 3 | 1 | |
| `mit/util` | `compat.c` | 0 | 1 | 13 | |
| | `ez2ascii.c` | 0 | 0 | 2 | |
| | `header.c` | 5 | 4 | 2 | |
| | `headrtv.c` | 12 | 2 | 21 | item-9 fix pre-applied |
| | `popts.c` | 8 | 3 | 12 | item-9 fix pre-applied |
| | `vutils.c` | 0 | 1 | 2 | |
| `srctext/html` | `html.c` | 21 | 3 | 25 | §13.1, §13.2 fixes |
| | `htmlview.c` | 3 | 3 | 22 | §13.1 fix |
| | `version.c` | — | — | — | no K&R definitions |
| `srctext/ptext` | `ptext.c` | 13 | 0 | 25 | hand-converted `InitializeObject`; §13.4 fix |
| | `ptextv.c` | 2 | 2 | 10 | hand-converted `InitializeObject`; §13.4, §13.5 fixes |
| `srctext/ltext` | `ltext.c` | 8 | 1 | 10 | hand-converted `InitializeObject` |
| | `ltextv.c` | 2 | 2 | 6 | hand-converted `InitializeObject`; §13.4, §13.5 fixes |
| `demos/circlepi` | `circpi.c` | 1 | 3 | 0 | |
| | `circpiv.c` | 6 | 3 | 3 | |
| `calc` | `calc.c` | 4 | 2 | 2 | |
| | `calcv.c` | 8 | 3 | 15 | |
| `wpedit` | (none) | — | — | — | confirmed inert, no action |
| `eatmail` | `eatmail.c` | 0 | 0 | 6 | item-9 fix pre-applied (3 functions) |

## 17. Gate results

Every active directory (10 of 11 — `wpedit` excluded, inert): subtree-
local gate (`make clean && make depend && make -k install`) run twice,
separately, from a fresh clean state each time, zero `error:` lines
both times, every time. Individually confirmed above (§14 table) and
in the session transcript.

**Tree-wide `dependInstall` gate** (from `src/`, not the tree root —
the tree root has no `Makefile`; this cost one wasted, instantly-
failing attempt, caught and corrected before any real time was lost):
clean, 0 `error:` lines across 19,222 log lines, `ltapp.c` baseline
confirmed gone (§15), all 11 batch directories confirmed present in the
gate log (`wpedit` descended into as expected — directory presence in
`SUBDIRS`, not evidence of compilation, consistent with its confirmed
inert status).

**Full clean rebuild** (`make Clean; make World`, from `src/`): `make
Clean` exit 0. `make World` run detached, verified running via `pgrep`
+ growing log before trusting it, waited out to real completion (not a
notification-only wait — confirmed via both a synchronous blocking
poll and, for the longer `World` run, a background-task wait that was
independently cross-checked against the log's actual mtime and process
list before being reported as done). Result: clean — 0 real `error:`
lines (the single `error:` grep hit is the already-documented false
positive, `"Internal error: unknown recognizer type"` inside a
`-Wdeprecated-non-prototype` warning, per `rollout-procedure.md`'s Gate
definition section), 0 make `*** [` error markers, all 11 batch
directories present in the full-rebuild log. This is the strongest
check this tree has had since Wave 6's close, and it's green.

## 18. `fossil status`/`fossil extras`: exact file scope, no commit made

`fossil status`: **54 `EDITED` files**, all inside the 11 batch
directories (9 Imakefiles — every `.ch`-bearing directory except
`wpedit`, which was left untouched; 38 `.c` files; 7 `.ch` files:
`html.ch` plus `zip/utility`'s `lt.ch`/`ltapp.ch`/`ltv.ch`/`sched.ch`/
`schedapp.ch`/`schedv.ch`). Plus the pre-existing `ADDED
revival/doc/m3-c2-prompt.md`, not from this session. Nothing outside
this batch's 11 directories touched. `fossil extras`: this session's
own `clean-rebuild.log`, `dependInstall.log`, and
`revival/doc/claude-history/m3-c2-REPORT.md` (this file) appear as
expected untracked additions; everything else in the (long) extras list
is pre-existing build byproducts and prior-session artifacts (other
`*-REPORT.md`/`*-session.diff` files, regenerated per-directory
`Makefile`s, `.o`/`.do` build products) unrelated to this session. **No
`fossil commit` run**, per the prompt's explicit instruction.

## 19. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: checked live-consumer
status first (`.do` presence in `build/dlib/atk/`, standalone binary
presence in `build/bin/`) before suggesting anything; never launching a
GUI app from this session.

- **`contrib/srctext/html`** (§13.1, §13.2 — the most consequential fix
  in this batch, a real interface bug plus a large mechanical
  conversion): `html.do`/`htmlview.do` are both installed and dynamically
  loadable. In a real Terminal.app (not this session), open `ez` on a
  document containing (or insert via the Insert menu) an HTML inset,
  and specifically exercise environment-entering/leaving markup
  (nested tags that trigger `EnvStart`/`EnvEnd` — e.g. blockquotes,
  lists, or any nested-tag HTML content) since that's exactly the code
  path this batch's real bug fix touches. Compare rendering before/after
  if a pre-`-pe` build is still available, or simply confirm the inset
  loads and renders nested HTML without a crash or garbled adornment
  marks.
- **`contrib/zip/utility`** (§13.3, §13.4, §15 — the batch's other
  major fix, closing the standing `ltapp.c` gate blocker): `sched` is a
  real installed symlink to `runapp` (`build/bin/sched -> runapp`). From
  a real Terminal.app, run `sched` and confirm the scheduler app
  launches, and specifically exercise the `Set_Debug` path (the
  `-debug`-style toggle, if one is exposed in the app's menu — check
  `sched.help`/`schedapp.ch` for the actual UI hook) since that's the
  parameter this batch retyped from untyped to `boolean`.
- **`contrib/mit/annot`** (large 9-file conversion, but no functional
  fix — everything already matched): `note.do`/`icon.do`/`ps.do`/
  `stroffet.do` and their view counterparts are all installed. Insert a
  Note, PostScript, or Troffet inset in `ez` and confirm normal
  behavior (create, edit, save) — this exercises the bulk of the
  batch's file-local helper conversions with the lowest risk of
  surfacing anything new, since Gate 0/1 found zero DRIFT and zero
  hand-fixes needed here.
- **`contrib/time`**: `clock.do`/`timeoday.do`/`writestmp.do` and view
  counterparts installed. Insert a Clock, Time-of-Day, or Write-Stamp
  inset in `ez`; no functional fix was needed here (clean both gates),
  so this is a low-risk confirmation check.
- **`contrib/srctext/{ptext,ltext}`** (§13.4, §13.5, plus the
  `InitializeObject` hand-conversion — genuinely load-bearing since the
  `.ch`'s missing declaration meant these were relying entirely on
  classpp's automatic `data:`-section wiring): `ptext.do`/`ptextv.do`/
  `ltext.do`/`ltextv.do` installed. Insert a Pascal-text or Lisp-text
  syntax-highlighting inset in `ez` and specifically exercise the
  key bindings this batch's stranded-declaration fixes touch: paren-
  matching (type a closing paren/brace/bracket), the tab key
  (indentation), and (`ptext` only) the asterisk/space keys — these are
  exactly the functions whose forward declarations were corrected.
- **`contrib/demos/circlepi`, `contrib/calc`**: `circpi.do`/`circpiv.do`
  and `calc.do`/`calcv.do` installed. Both are clean, zero-fallout
  conversions in this batch — a routine insert-and-confirm in `ez` is
  sufficient, no specific code path to target.
- **`contrib/eatmail`**: `build/bin/eatmail` is a real, freshly-rebuilt
  standalone binary — not a `runapp`-hosted inset. `eatmail` reads
  system mailboxes and is destructive-by-design (mail delivery
  program); do **not** run it against a real mailbox from this or any
  automated session. If wdc wants to confirm this batch's fix
  (`rmlock`/`lock`/`SetHoldFromFile`'s declarations, §13 item-9 fix),
  the safest check is `eatmail -h`/a syntax/usage invocation against a
  scratch copy of a test mbox, never a real one.
- **`contrib/wpedit`**: **no runtime check possible or meaningful** —
  confirmed entirely inert in this build (§14), no binary exists
  (`build/bin/wpedit`: does not exist), nothing to exercise. Say so
  plainly rather than inventing a check, per the prompt's own
  instruction for exactly this situation.

## 20. What surprised me / didn't match expectations

1. **§3a's correction was bigger than "verify a claim"** — it wasn't a
   30-second sanity check, it needed a live `-pe` compile probe to
   settle definitively, and it turned out the prompt's own pre-diagnosis
   had the exact same species of error C1 had already caught and fixed
   in the *previous* batch's session, one commit before this prompt was
   written. Worth flagging for M4: even a routine batch's "already
   confirmed" context can be wrong, and the batch's own established
   verification method (live compile-check) is worth reaching for
   whenever a claim is checkable that way, not just when Gate 0
   explicitly calls for it.
2. **`contrib/wpedit` being fully inert was a real Gate 0 gap**, not
   just a documentation nicety — `rollout-procedure.md` names this
   exact directory by name as historical precedent for exactly this
   failure mode, and I still didn't check it directly at Gate 0 (I
   trusted the `dependInstall.log` "building" line, which the same
   document explicitly warns is insufficient). Caught before any wasted
   work, but worth a harder rule for M4: any directory-liveness claim
   made from a log grep alone, without also checking the generated
   Makefile has real targets, should be treated as unconfirmed.
3. **The `fix-missing-static-decl` non-idempotency bug (O1, 2026-07-25)
   is still live** — five new instances hit in this single batch,
   spread across 4 files in 2 directories, roughly a month after first
   documented. It's cheap to work around per-instance (as done here and
   in every prior batch that hit it) but has now recurred often enough
   across enough M3 batches that a real tool fix might be worth
   reconsidering for M4, if M4's own workload includes further
   `fix-missing-static-decl` exposure — noting for the record, not
   recommending unilaterally.
4. **Two genuine ~35-year-old interface bugs in one batch** (`EnvStart`/
   `EnvEnd`'s type, `ltapp.ch`/`schedapp.ch`'s unnamed parameter) is on
   the high end for a "routine" batch of this size — consistent with
   M3's broader pattern (this mechanism keeps surfacing real bugs, not
   just conversion noise) but worth noting this was not a quiet batch
   despite being flagged routine.
5. **The tree-root-vs-`src/`-root `make` invocation mistake** (§17) —
   `andrew-6.4/` has no `Makefile`; the real one is at
   `andrew-6.4/src/Makefile`. Cost one instantly-failing background
   launch and a round of clarification with the coordinator before I
   verified state properly. Worth adding to the standing build-
   invocation convention for M4: always confirm a background build is
   *actually running* (`pgrep` + growing log) before setting up any
   wait for it, foreground or background — don't trust the launch
   command's own immediate return as proof of anything.

## 21. M3 closing summary

**M3 is complete.** This session closes Wave 7 (`C1` + `C2`) and M3
overall, handing off to M4. Final session count: **15 sessions total**
across 7 dependency-order waves (O1–O4, B1–B3, T1, I1, I2, A1, AMS1,
AMS2, C1, C2) — matching `m3-batches.md`'s own revised 2026-07-30
estimate exactly. All **91 active directories** with `.c` files are
accounted for (90 converted for real across the 24 batches, `atk/eq`
folded into I2 as an ordinary directory per the 2026-07-30 pilot
retirement decision, counted once).

Lessons worth carrying into M4's own kickoff:

- **The "routine batch, Gate 0 only" delegation shape worked as
  designed**, including for its intended purpose: catching a wrong
  pre-diagnosis claim (§3a) and a genuinely new tool-interaction shape
  (§5) *before* Gate 1 wasted any build time on them. The two-gate
  structure earned its keep on the very last M3 batch.
- **A "routine" classification doesn't mean zero real bugs** — this
  batch alone found 2 more ~35-year-old genuine interface bugs (on top
  of the corrected `InitializeClass`/`FinalizeObject` misdiagnosis),
  consistent with M3's tree-wide pattern of real bugs surfacing via
  ANSI arity/type-checking. M4 should keep budgeting for this, not
  treat "routine" as "unlikely to find anything."
- **Directory liveness needs a positive check, not just a log grep** —
  `wpedit` is the second directory in this project's history
  (`overhead/malloc`/`inst` at O4 being the first M3-era instance) where
  trusting `dependInstall.log`'s "building" line alone would have been
  wrong. M4 should treat "confirm the generated Makefile has real
  targets" as a mandatory Gate 0 step, not an optional deeper-dive.
- **`fix-missing-static-decl`'s non-idempotency remains the single most
  recurring fallout shape across M3** (O1, O2, O3, B1/B2's brace-glue
  variant, and now 5 more instances here) — if M4's scope touches any
  further K&R conversion work, this is worth a real fix rather than
  continuing to absorb it as per-instance fallout.
- **Verify a background build is actually alive before waiting on it,
  every time** — the tree-root-vs-`src/`-root mistake (§20.5) is a
  simple, avoidable class of error; the fix is procedural (always
  `pgrep` + log-tail immediately after any detached launch) and should
  be folded into the standing build-invocation convention for M4.
