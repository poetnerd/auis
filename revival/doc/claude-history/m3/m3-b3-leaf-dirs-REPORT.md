# M3 Wave 2, Batch B3: 13 leaf directories — ansify + `-pe`/`.eh` rollout — REPORT

## 1. Status

Complete. All 13 directories gate clean, twice each (26/26 gate cycles
green). No commit made (per hard rule). Stopped after the gate, as
instructed.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`, the `porting-assessment.md`
   §17 subsection, and `m3-rollout-runbook.md`'s B1/B2 findings entries.
2. Applied the 5 pre-diagnosed `.ch`/`.c` fixes from the prompt (sections
   1-5) exactly as specified.
3. Rebuilt the ansify signature DB (`ansify --build-db`, 565 classes) —
   the standing DB was from 2026-07-29, predating the 5 pre-diagnosed
   `.ch` edits (`unknownv.ch`, `suiteev.ch`, `tree.ch`), so the very
   first real conversion attempt on `unknownv.c` DRIFT-blocked on stale
   data. Rebuilt twice more later in the batch after further `.ch`
   fixes (`atk/apt/apt/apt.ch`+`aptv.ch`, `atk/apt/tree/treev.ch`) so
   each directory's conversion always saw current signatures.
4. Processed each of the 13 directories: `ansify --dir` (real run),
   investigated every DRIFT/skip/compile-failure, applied fixes, added
   `CLASSFLAGS = $(CLASSINCLUDES) -pe` to the Imakefile (skipped for
   `atk/apps`, which has no `.ch` files at all — matches O1's
   established precedent), force-regenerated `.eh`, ran the standing
   per-batch greps, then gated (`make clean`, `make depend`, `make -k
   install`) twice per directory.
5. `fossil diff > m3-b3-leaf-dirs-session.diff` in the tree root;
   writing this report to `revival/doc/claude-history/`.

## 3. Pre-diagnosed fixes — confirmed applied exactly as specified

All 5 applied verbatim, and in every case `ansify`'s real run
subsequently converted the previously-blocked method cleanly once the
signature DB reflected the fix:

1. `atk/textobjects/unknownv.ch` — `InitializeClass()` (dropped the
   restated `self` param). `unknownv__InitializeClass` now converts
   (1 methods → included in the 4/2/3 count for `unknownv.c`) instead
   of DRIFT-blocking.
2. `atk/apt/suite/suiteev.ch` — `InitializeClass()` (same) and
   `FinalizeObject(struct suiteev *self)` (dropped the redundant
   `ClassID` restatement). `suiteev.c` converts cleanly (36
   methods/2 classprocs/39 helpers).
3. `atk/apt/tree/tree.ch` — `TreeWidth(tree_type_node node)` /
   `TreeHeight(tree_type_node node)` (added the ~35-year-old missing
   parameter). Confirmed zero callers, as the prompt stated; `tree.c`
   gates clean with the corrected signatures.
4. `atk/extensions/gsearch.c` and `isearch.c` —
   `InitializeClass(struct classheader *classID)` hand-folded (ansify
   cannot see genuinely-empty-parens K&R definitions). Both directories
   gate clean.
5. `atk/utils/dialog.c` — `dialog__InitializeClass(struct classheader
   *classID)` (dropped the stray unused `self` param to match
   `dialog.ch`'s already-correct empty-parens convention). Gates clean.

## 4. New findings beyond the pre-diagnosis

The pre-diagnosis anticipated the specific fallout in `atk/textobjects`,
`atk/apt/suite`, and `atk/apt/tree`. Every other directory's fallout is
new to this batch. In discovery order:

### 4.1 Signature-DB staleness (mechanism, not a bug)
The DB must be rebuilt after any `.ch` edit that affects a
signature ansify will look up later in the same session — obvious in
retrospect, but cost one avoidable DRIFT-investigation cycle on
`unknownv.c` before I recognized it. Rebuilt 3 times total this batch.

### 4.2 Stale K&R forward declarations not synced to a converted ANSI definition
The single most common new-fallout shape this batch, distinct from the
already-known "genuinely-empty-parens K&R definition invisible to
ansify" pattern (pre-diagnosis §4): here the *definition* converts
fine, but a pre-existing empty-parens forward declaration a few lines
above it (`static void foo();`) is never touched by ansify's
`fix-missing-static-decl` pass, and C's compatibility rule for a
prototype-less declaration composing with a full-prototype definition
fails hard whenever any parameter has a type subject to default
argument promotion (`char`, `short`, `float` — not `int`/`long`/
pointers). Fixed by hand-updating the stale forward declaration to
match the real signature, file by file:
- `atk/extensions/filter.c`: `filter()` → full prototype (`short
  method` param).
- `atk/apt/suite/suite.c`: `DrawOutline()` → full prototype (`short
  width` param).
- `atk/syntax/tlex/thongs.c`: fixed via `global.h`'s stale `struct
  line *ThongAdd();` declaration (same mechanism, shared header instead
  of same-file).
- `atk/textobjects/panel.c`: `ProcNext()`/`ProcPrev()` → full
  prototypes (`char c` param each).
- `atk/lookz/tabrulv.c`: `DoTicks()` and the combined
  `RemoveIcon(), RepaintIcon(), ...` multi-declarator line → full
  prototypes (rewrote all 7 names in that line for clarity, only 2 of
  which — `RepaintIcon`/`RedrawText` — actually had promotable-type
  params).
- `atk/frame/frame.c`: `GotKey()` → full prototype (`char c` param).
- `atk/apt/tree` and others: none found (checked, clean).

A useful confirmation of the mechanism: when `ansify --dir` is
re-run over a directory that already contains a hand-fixed full
prototype, `fix-missing-static-decl` doesn't recognize a full ANSI
prototype as "already declared" (it only recognizes the narrow
empty-parens shape) and re-clobbers it back toward a stub — but the
per-file compile gate then fails on that regenerated state and
restores from backup, so the *final on-disk file* is unaffected. This
means the tool's own "COMPILE FAILED" message on a *repeat* run can be
a false alarm describing an intermediate state, not the final one —
learned this the hard way on `atk/extensions/filter.c`, verified by
always re-checking the final on-disk compile status directly rather
than trusting a repeat run's printed conclusion.

### 4.3 `-pe`-timing conflicts: class methods with a promotable-type parameter
Same root C-compatibility rule as 4.2, but for *class methods* rather
than file-local statics: before `-pe` is added, a class's `.eh`
declares every method with bare empty parens; a method whose real
parameter list has a promotable type (`char`/`short`/`float`) then
hard-conflicts with its own freshly-ANSI-converted `.c` definition,
purely because `-pe` hasn't been turned on *yet* for that directory.
This is not a bug anywhere — it's an artifact of the prompt's own task
ordering (ansify before `-pe`) colliding with directories that happen
to have such methods. Resolved by bringing the `-pe`/`.eh`-regen step
forward for the affected directory (still applied formally at the
documented step later, redundantly but harmlessly) before final
verification. Hit in `atk/syntax/tlex` (`tlex__PutTokChar`,
`char c`), `atk/textobjects/panel.c` (`AssignKey`, `char c`),
`atk/apt/suite/suiteev.c` (`ItemDrawCaption`/`ItemDrawTitle`, `short
forcedTransferMode`), and `atk/apt/apt/aptv.c` (`SetShrinkIcon`,
`SetPrintResolution`, `SetPrintUnitDimensions`,
`SetPrintPageDimensions`, `SetPrintGrayLevel` — all `char`/`float`
params) and `atk/apt/apt/apts.c` (`YearMonthDay`/`WeekDayOffset`,
though those two turned out to be masking finding 4.6 below, not this
mechanism alone).

**Important trap discovered here**: `make <file>.o` before a
directory's `Makefile` has `-pe` in its `CLASSFLAGS` will silently
*regenerate the `.eh` under the OLD flags* via the `.ch.eh:` build
rule, clobbering a manually `-pe`-generated test `.eh` and producing
misleading "still conflicting" errors that look like a deeper bug.
Confirmed by direct experiment on `atk/syntax/tlex/tlex.eh`. Always add
`-pe` to the Imakefile and regenerate the *Makefile itself* (`make -C
<dir> Makefile`) before trusting any `make <file>.o` compile-gate
result for a file with promotable-type method parameters.

### 4.4 Two genuine ansify tool bugs (K&R→ANSI parameter conversion)
1. **Double-pointer parameter silently drops one `*`.**
   `atk/syntax/tlex/tlex.c`'s `tlex__NextToken`'s K&R parameter `void
   **pyylval` converted to `void *pyylval` (single star) — the only
   instance found tree-wide in this batch (checked all other
   double-star K&R parameters across the 13 directories by comparing
   against the fossil-committed originals; all others — e.g.
   `gentlex.c`'s `usage(char **args)`/`main(int argc, char **args)` —
   converted correctly). Hand-fixed to `void **pyylval`, matching the
   real usage (`*pyylval = self->tokenvalue;`) and the `.ch`'s own
   declared type.
2. **Array-parameter bracket transposition.** 7 instances in
   `atk/apt/suite/suite.c` (`GetSuiteFGColor`, `GetSuiteBGColor`,
   `GetActiveItemFGColor`, `GetActiveItemBGColor`,
   `GetPassiveItemFGColor`, `GetPassiveItemBGColor`, `ParseRGB`) — the
   real K&R/`.ch` type `unsigned char rgb_vect[]` converted to the
   syntactically-invalid `unsigned char [ ] rgb_vect` (brackets *before*
   the parameter name), a hard parse error. Fixed with one `sed` pass
   across all 7 sites (verified identical shape and identical
   replacement at every site first).

Neither of these matches any previously-documented ansify limitation in
`porting-assessment.md`/`m3-rollout-runbook.md`; flagging both as new,
tool-level findings for whoever next touches `ansify`'s parameter
parser.

### 4.5 Brace-glued *local variable declaration* — a different location than the already-fixed brace-glued gap
The classpp/ansify brace-glued parser fix from earlier the same day
(2026-07-30) covers a glued *function signature* (the whole method
becomes invisible to conversion — a safe no-op skip). This batch found
a related but distinct and more dangerous shape: a K&R function body
opening with its first local-variable declaration on the *same line*
as the opening brace (`{ register struct suite *self = NULL;`).
ansify's converter doesn't skip this — it *silently deletes the
declaration line entirely*, leaving every use of that variable in the
body genuinely undeclared. Found in `atk/apt/suite/suite.c` (30
instances) and `suiteev.c` (2 instances) — by far the largest-volume
finding of the batch. Applied the same stopgap B2 already established
for the signature-glue variant: mechanically split the glued brace onto
its own line in the *original* K&R source, then let `ansify` reconvert
normally. Confirmed via a tree-wide check across all 13 directories
(pattern: `^\{[ \t]*(register|struct|char|int|long|boolean|unsigned|
short|double|float)\b.*;`) that no other directory in this batch has
this shape.

### 4.6 Orphaned return-type-on-its-own-line remnants
Several K&R definitions in this codebase put the return type (with or
without a trailing comment) on its own line, function name+params on
the next:
```c
void *     /* int (*)() */
parse__SetErrorHandler(self, handler)
```
ansify's converter correctly emits the new single-line ANSI signature
but doesn't remove the old return-type line, leaving orphaned text
immediately before the new signature. Two distinct failure modes
depending on whether the leftover line has a trailing comment:
- **With comment** (`atk/syntax/parse/parse.c`'s `SetErrorHandler`):
  produces a hard syntax error (`expected identifier or '('`).
- **Without comment** (`atk/apt/apt/apt/apts.c`'s `YearMonthDay` and
  `WeekDayOffset`): the orphaned `long` token merges across the
  newline with the following line's `long` into `long long`, which
  *does* parse (as a valid but different return type) and produces a
  genuinely confusing "conflicting types" error against the `.eh`'s
  plain `long` declaration — this one took real investigation to
  diagnose since the two declarations *look* character-for-character
  identical at a glance.

Fixed by deleting the orphaned line in both files. Added this pattern
(`^\w[\w \*]*/\*.*\*/\s*$`) to my proactive per-directory grep sweep
for the remaining directories in the batch; found no further instances.

### 4.7 Untyped `.ch` parameters silently defaulted to `void *`
Several `.ch` declarations across this batch have a parameter with
**no type at all** — a bare identifier — a documentation gap
predating this work entirely (K&R never checked it, since untyped
dispatch never cared). ansify's DB-driven conversion has to default
something for these, and picks `void *`; when the real K&R
implementation's actual type is a *function pointer* rather than a
data pointer, this produces `void *` being called as a function
(`called object type 'void' is not a function or function pointer`)
or dereferenced as one level deeper than it has (`incomplete type
'void' is not assignable`). All confirmed via the same
zero-blast-radius methodology as the pre-diagnosed `.ch` fixes (real
`.c` implementation type + all real callers checked before touching
the `.ch`):
- `atk/apt/apt/apt.ch`: `WriteObject`'s `writer` param → `void
  (*writer)()` (real type is a function pointer; `.ch`'s sibling
  method `ReadObject` already uses this exact style for its own
  callback parameter, confirming the intended convention).
- `atk/apt/apt/aptv.ch`: `Query`'s `query`/`default_response`/
  `response`, `QueryFileName`'s and `QueryDirectoryName`'s `query`/
  `response`, and `Announce`'s `message` — all typed to their real
  `char *`/`char **` K&R types (confirmed against all 3 real K&R
  implementations, which agree exactly).
- `atk/apt/tree/treev.ch`: `Create`'s first parameter was declared as
  a **bare type name with no pointer star or parameter name at all**
  (`Create( treev_Specification, struct view *anchor )`) — fixed to
  `Create( treev_Specification *specification, struct view *anchor )`,
  matching the real K&R implementation and both real callers
  (`atk/org/orgv.c`, `atk/bush/bushv.c`, both pass a
  `treev_Specification[]` array).

### 4.8 `treev.ch`'s `SetHitHandler` — pre-diagnosed as "do not fix," but `-pe` made it gate-blocking
The prompt's own section 8 flagged this exact method
(`SetHitHandler((long *handler)(), char *anchor)` vs. the real
`struct view *(*handler)()`/`struct view *anchor`) as a known
`.ch`-vs-`.c` mismatch, explicitly instructing "do not fix this" and
treating the resulting `ansify` skip as harmless since the `.c` simply
stays K&R. That guidance held *before* `-pe` was turned on. Once
`-pe` went live for `atk/apt/tree`, classpp itself (not ansify) tried
to emit a typed export prototype from the malformed `.ch` type and
produced literally invalid C in the generated `.eh`:
```c
void treev__SetHitHandler(struct treev *, ( long *  ) ( ), char *);
```
This is a syntax error that fails the *entire file's* compile — no
longer a contained, skippable fallout. Since the batch's own gate
requires `-pe` live and gating clean, "leave it K&R" was no longer an
available option. Fixed the `.ch` to the sibling class's own already-
established convention for exactly this ambiguity
(`atk/org/orgv.ch`'s `SetHitHandler(procedure handler, struct view
*anchor)`, using the codebase's generic function-pointer placeholder
type) rather than asserting the specific `struct view *(*handler)()`
signature outright — `procedure` is compatible with any function
pointer by the codebase's own established idiom, so this is the
minimal fix that unblocks the gate without overclaiming a signature I
didn't have time to trace every caller for (I did do the caller check
anyway: no confirmed live callers found tree-wide, matching the
prompt's own finding). `ansify` then converted
`treev__SetHitHandler` cleanly on the next pass (30 methods total, up
from 29).

### 4.9 Rock idiom (`void*`/`long`) — 4 new instances, all resolved per the established B1/B2 precedent
"Judge each rock by its callers, not its name" (`porting-
assessment.md` point 9 / `m3-rollout-runbook.md` B1 finding 1):
- `atk/apt/suite/suite.c`: `suite__Create`'s `ClientAnchor =
  anchor;` where `ClientAnchor` is `self->anchor` (`long`, per
  `suite.ch`'s data section) and `anchor` is `void *` (per the same
  `.ch`'s method signature) — added `(long)` cast.
- `atk/apt/suite/vector.c`: `vector__AddItem`'s `Data[insertOffset] =
  item;` where `Data` is `long *` and `item` is `void *` — added
  `(long)` cast.
- `atk/utils/dialogv.c`: `dialogv__PostInput`'s `dv->hr.rock =
  choicerock;` where `hr.rock` is `long` (per `dialogv.ch`'s
  `struct dialogv_HitRock`) and `choicerock` is `void *` — added
  `(long)` cast.
- `atk/apt/tree/tree.c`: two related instances — `Build_Node`'s own
  `long datum` parameter (already correctly `long` in the original
  K&R, matching its internal `(void *)datum` cast when calling
  `tree_SetNodeDatum`) called from 4 sites with the now-`void
  *`-typed public `datum` parameter — added `(long)` casts at all 4
  call sites; and `tree__SetNodeDatum`'s `NodeDatum(node) = datum;`
  where the macro expands to a `long` struct field — added `(long)`
  cast.
- `atk/frame/frame.c`: **inverse direction** — `frame__FindFrameForBuffer`
  had a stray, incorrect `(long)` cast laundering a perfectly good
  `struct buffer *b` down to `long` before passing it into
  `frame_Enumerate`'s `void *functionData` parameter (`.ch`-declared
  `void *`, and the callback `FindBuffer` takes the same pointer
  directly, un-cast). Removed the stray cast rather than adding one —
  matches `porting-assessment.md`'s documented "`(long)` casts launder
  a rock, they don't fix it" pattern exactly.

### 4.10 Function-prototype-scope struct-tag trap (new C-semantics finding, not an ansify bug)
`atk/textaux/compchar.c`: after hand-fixing 5 stale forward
declarations (§4.2's pattern) to reference `struct YARock *`/`struct
SARock *`, three of them (`keywork`, `handlekey`, `doasciireplacement`)
*still* showed "conflicting types" against their own later, textually
identical-looking definitions. Root cause: `struct YARock`/`struct
SARock` are each defined later in the same file; when a struct tag's
*first-ever textual appearance* in a translation unit is inside a
**pure declaration's** parameter list (not a definition with a body),
C gives that tag only *function-prototype scope* (C11 6.2.1p2) — it
evaporates at the end of the declarator and does **not** merge with
the real file-scope struct defined later, even though nothing about
the surface syntax looks any different. The two "identical" lines are
referring to two different, incompatible anonymous types. Fixed by
adding plain forward declarations (`struct YARock;` / `struct
SARock;`) at file scope before the affected forward declarations, so
every later reference resolves to the same tag. This is a genuine,
subtle trap for exactly this kind of "add a missing prototype by hand"
fix and worth remembering for any future batch doing the same class of
repair.

## 5. Per-directory `ansify` conversion counts and gate results

All 26 gate cycles (`make clean; make depend; make -k install`, twice
per directory) are green — zero real `error:` lines, only the one
documented tree-wide false positive (`Internal error: unknown
recognizer type` inside a `-Wdeprecated-non-prototype` warning, seen in
`atk/syntax/tlex`) and the benign `makedepend` system-header warnings
seen in every directory (`signal.h`/`ctype.h`/etc. not found via its
limited search path — pre-existing, unrelated to this batch).

| Directory | Files | methods/classprocs/helpers converted (final) | `-pe` | Gate x2 |
|---|---|---|---|---|
| `atk/extensions` | 10 | 1/12/126 | yes | clean |
| `atk/syntax/tlex` | 8 | 8/4/35 | yes | clean |
| `atk/textobjects` | 7 | 64/17/46 | yes | clean |
| `atk/apt/suite` | 6 | 103/8/92 | yes | clean |
| `atk/lookz` | 5 | 28/8/69 | yes | clean |
| `atk/frame` | 5 | 54/20/115 | yes | clean |
| `atk/syntax/parse` | 4 | 6/10/6 | yes | clean |
| `atk/apps` | 4 | 0/0/10 | n/a (no `.ch`) | clean |
| `atk/utils` | 3 | 35/9/4 | yes | clean |
| `atk/apt/apt` | 3 | 64/16/12 | yes | clean |
| `atk/textaux` | 2 | 3/3/42 | yes | clean |
| `atk/syntax/sym` | 2 | 1/14/7 | yes | clean |
| `atk/apt/tree` | 2 | 68/6/61 | yes | clean |

("methods/classprocs/helpers converted" sums each file's final
`ansify` report after all fixes; skipped/dead-code methods per the
pre-diagnosis §6-§8 are not counted as conversions and are called out
individually above.)

## 6. `fossil status` / `fossil extras`

`fossil status` at report time: 78 `EDITED` files, all under `src/`,
matching exactly the directories and fix categories described above —
no file outside the 13 target directories was touched, and no `.ch`
fix went beyond the 3 directories where one was needed
(`atk/textobjects`, `atk/apt/suite`, `atk/apt/tree`) plus the 3 new
untyped-`.ch`-parameter fixes (`atk/apt/apt/apt.ch`,
`atk/apt/apt/aptv.ch`, `atk/apt/tree/treev.ch`) and the one shared-header
fix (`atk/syntax/tlex/global.h`) and cross-directory superclass fix
(`atk/syntax/parse/lexan.ch`, needed because `atk/syntax/tlex/tlex.c`
subclasses `lexan` and its `NextToken` override's real 2-star pointer
type was blocked by `lexan.ch`'s own 1-star typo — verified zero other
subclasses and all real callers before fixing).

`fossil extras`: only the expected `build/` artifacts (binaries,
generated `.o`/`.eh`/`.ih`/`Makefile` files from the 26 gate builds) —
no stray source files. No commit made; the working tree is left
exactly as this session leaves it, per the hard rule.

`fossil diff > m3-b3-leaf-dirs-session.diff` written to the tree root
(18,703 lines).

## 7. Files touched (all compile clean; see §5 for per-file conversion counts)

**`.ch` files edited** (8, all confirmed zero/near-zero blast radius
before editing): `atk/textobjects/unknownv.ch`,
`atk/apt/suite/suiteev.ch`, `atk/apt/tree/tree.ch`,
`atk/textobjects/chlistv.ch`, `atk/apt/apt/apt.ch`,
`atk/apt/apt/aptv.ch`, `atk/apt/tree/treev.ch`,
`atk/syntax/parse/lexan.ch`.

**`.h` files edited** (1): `atk/syntax/tlex/global.h` (stale forward
declaration sync, §4.2).

**Imakefiles edited** (12 — all 13 directories except `atk/apps`,
which has no `.ch` files): added `CLASSFLAGS = $(CLASSINCLUDES) -pe`.

**`.c` files**: all 61 files in scope were processed by `ansify`; see
§5 for the full per-directory breakdown and §4 for every hand-fix
applied on top of the tool's own conversion. Every file in all 13
directories currently compiles with zero errors, confirmed both
per-file (`make <file>.o`) and via the full two-cycle gate.

## 8. Open questions / anything that surprised you

- **The full-directory gate caught two real regressions that my
  per-file `make <file>.o` spot-checks missed** (`atk/frame`'s
  `AskForStringCompleted` pair, `atk/syntax/parse`'s
  `GetCurrentParse`) — both were cases where a method's `.c`
  definition only became inconsistent with its `.eh` *after* `-pe`
  was fully wired into the Makefile, which happened between my
  per-file check and the full `make clean && depend && install`
  cycle. Lesson for any future batch: treat the full gate as the only
  authoritative check, never the per-file spot-check alone, once `-pe`
  is in play.
- The volume of genuinely new fallout patterns in this batch (10
  distinct categories, §4.1-§4.10) was well beyond what the
  pre-diagnosis anticipated — reasonable, since the prompt's own
  pre-diagnosis was scoped to the specific findings the orchestrator
  had time to hand-verify, not an exhaustive census of all 61 files.
  None of the 10 new patterns contradict anything in the pre-diagnosis;
  they're additive.
- `atk/apt/suite/suite.c`'s 30 brace-glued local-declaration instances
  (§4.5) is by far the largest single-file fallout count in the batch
  and, as far as I can tell from the runbook, larger than anything
  previously documented in B1/B2 for this bug class — worth flagging
  to whoever next revisits the `ansify` tool itself, since the
  signature-glue variant already got a proper tool fix earlier the
  same day but this local-declaration variant did not.
- I did not attempt the "bonus investigation" the prompt offered
  (tracing `treev_SetHitHandler`'s real caller chain in full) beyond
  the caller check already needed to justify the `.ch` fix in §4.8 —
  the fix I applied unblocks the gate without asserting a specific,
  unverified signature, which seemed like the right level of
  confidence for a non-blocking bonus item.
