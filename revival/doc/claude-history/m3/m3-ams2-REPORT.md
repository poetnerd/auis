# M3 Wave 6, Batch AMS2: `atkams/messages/lib` + 3 AMS support libs — REPORT

## 1. Status

**Stopped at Gate 0**, as instructed. Pre-diagnosis/classification only
— no real `ansify --dir` run, no permanent `-pe`, no `.ch`/`.c` edits,
no fossil commits. `fossil status` is clean (nothing changed in the
working tree relative to session start); every temporary experiment
(see §2) touched only a transient `CLASSFLAGS += -pe` line in
`atkams/messages/lib/Imakefile`, force-regenerated `.eh`/`.ih`
scratch files, and `.o` scratch files — all reverted/deleted before
moving to the next check. `fossil status` confirmed clean after each
revert.

**Zero UNCLASSIFIED items.** Every finding below matches an existing
taxonomy entry in `m3-rollout-runbook.md` or `porting-assessment.md`
§14/§17, most confirmed with a live compile check rather than a
read-by-hand alone. I am stopping and waiting for the orchestrator's
go-ahead per the prompt's instructions regardless.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `m3-rollout-runbook.md` in full (the "Current standing per-batch
   checklist" — all 9 active items — and "Session structure going
   forward" sections, plus the AMS1 findings entry directly above this
   batch), `porting-assessment.md` §14 (Delegation, tool design) and
   §17 (the `InitializeClass`/`InitializeObject`/`FinalizeObject`
   special-casing and its two documented exceptions), and
   `m3-batches.md`'s Wave 6 section, per `m3-ams2-prompt.md`'s
   instructions.
2. Confirmed the `.ch`/`.c` counts myself (standing check 1): 17 `.ch`
   + 23 `.c` in `atkams/messages/lib`; 0 `.ch` + 7/3/1 `.c` in
   `ams/libs/shr`/`cui`/`nosnap` respectively — matches the prompt's
   scope note exactly. Confirmed no `Parser(` macro in any of the 4
   Imakefiles (no bison-generated files in scope) and confirmed the
   `COMPILERFLAGS` guard is present in `messages/lib` and `cui`,
   absent in `shr`/`nosnap`, matching the prompt's pre-confirmed notes.
3. Confirmed the signature DB (`build/desc/*.desc`) is fresh (all
   relevant `.desc` files dated 2026-07-31 16:28, from the same-day
   `ansify` same-classname-collision fix's rebuild) — no `--build-db`
   rebuild needed.
4. Ran `ansify --dry-run --dir` on all 4 directories, recorded every
   DRIFT/skip finding.
5. Ran the rest of the standing checklist (items 2-9) against all 4
   directories: predefined-macro-typo grep, empty-parens
   lifecycle-method grep, installed-header grep for converted
   non-static helpers (built a name census script reusing `ansify`'s
   own `HDR`/`parse_decl_block`, then grepped `build/include/**/*.h`
   and `*.ih` for empty-parens declarations of any of the ~390
   candidate names), restated-lifecycle-param `.ch` check (read every
   one of the 17 `.ch` files' `InitializeClass`/`InitializeObject`/
   `FinalizeObject` declarations by hand and cross-checked each
   against its real `.c` definition's actual self type, not just
   param count), `fossil status` concurrent-commit check (re-checked
   at the end too — clean throughout), the milestone-agnostic checks
   (liveness census via `Imakefile`/`Makefile` `OBJS`/`DOBJS`
   inspection plus `site.h`/`allsys.h` macro state, anchored
   `malloc`/`free`/`realloc`/`calloc` grep), the broadened
   stranded-forward-declaration check (both `static` and `extern`,
   wrote a script reusing `ansify`'s own `parse_local_decls` to
   resolve every real K&R definition's parameter types and flag any
   narrow-by-value conflict against a matching empty-parens forward
   declaration), and item 9 (imported `ansify`'s own `HDR`/
   `parse_decl_block` directly via Python and ran it against every `.c`
   file in all 4 directories to find silently-dropped multi-line/
   multi-declaration K&R blocks).
6. For every classification that genuinely needed a compile check
   rather than a read-by-hand (the `fldtreev.ch` lifecycle-param
   question the prompt specifically flagged, and the `msgsa.c`/
   `text822.c` "no signature in DB" skips), I added
   `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `atkams/messages/lib`'s
   Imakefile, ran `make Makefile`, force-regenerated the specific
   `.eh` file(s), inspected the generated prototypes and (for the
   3 self-type-typo findings) ran an isolated `make <file>.o` to
   confirm an actual compile error, then `fossil revert`ed the
   Imakefile and deleted every scratch `.eh`/`.ih`/`.o` file before
   moving to the next check. `-pe` was never left live between checks
   or directories.
7. Scratch Python scripts used for items 4/8/9 live in the session
   scratchpad (`completeness_scan.py`, `item8_scan.py`,
   `item4_names.py`), not in the repo — none were written into
   `revival/tools/` or committed.

## 3. Per-directory classification

### 3.1 `atkams/messages/lib` (23 files, 17 `.ch`)

`ansify --dry-run` baseline: 23 files, 0 compile failures, **3 DRIFT
findings** (all `fldtreev.c`), **2 skips** (`msgsa.c`, `text822.c`).

**Finding A — `fldtreev.ch`'s 3 DRIFT findings, the exact instance the
prompt pre-flagged as "not yet checked for which direction."** Matches
`porting-assessment.md` §17's "second real exception" (B3 finding) and
its own "false positive" mechanism, both already-documented taxonomy.
Resolved definitively via a live `-pe` compile check
(`fldtreev.eh` force-regenerated, inspected, then reverted):

- `InitializeClass`: `.ch` restates `classID` by name (1 declared arg)
  → classpp's ordinary classproc loop (not hardcoded for this name)
  produces a 2-param exported prototype
  (`boolean foldertreev__InitializeClass(struct classheader *, struct
  classheader *);`) against the real `.c` definition's 1 param
  (`foldertreev__InitializeClass(classID)`). **Broken** — same shape
  as B3's `unknownv.ch` fix. Fix direction: simplify `.ch` to
  `InitializeClass() returns boolean;` (empty parens), matching the
  true convention every other `.ch` in this directory already uses.
- `InitializeObject`: `.ch` restates both `classID` and `self`
  (correctly typed as `struct foldertreev *`) — irrelevant, since
  `InitializeObject` is always hardcoded by classpp to the 2-arg
  `(struct classheader *, struct foldertreev *)` prototype regardless
  of what `.ch` says (`class.c:1121-1122`), and the real `.c`
  definition already takes exactly those 2 params. **False positive,
  confirmed safe** — no fix needed. Confirmed directly:
  `boolean foldertreev__InitializeObject(struct classheader *, struct
  foldertreev *);` in the regenerated `.eh`, matching the real
  definition exactly.
- `FinalizeObject`: `.ch` restates **both** `classID` and `self` — not
  the hardcoded case (only `InitializeObject` gets that), so it goes
  through the ordinary loop and picks up both restated args on top of
  the automatic `classID` prefix: confirmed directly in the
  regenerated `.eh`,
  `void foldertreev__FinalizeObject(struct classheader *, struct
  classheader *, struct foldertreev *);` — **3 params** — against the
  real `.c` definition's 2 params (`classID, self`). Worse than a
  cross-file conflict: the `.eh`'s own internal `__Finalize` call site
  (`foldertreev__FinalizeObject(classID, self);`, 2 args) is
  self-inconsistent with the 3-param prototype directly above it in
  the same generated file — this would fail to compile inside the
  `.eh` itself, independent of the real `.c`. **Broken** — same shape
  as B3's `suiteev.ch` fix. Fix direction: restate `self` only
  (`FinalizeObject(struct foldertreev *self);`), matching the B2
  `buttonv.ch`/`sliderv.ch` workaround and `suiteev.ch`'s own
  established precedent in this same codebase.

**Finding B — 3 more `.ch` self-type copy/paste typos, found via a
full manual audit of all 17 `.ch` files' lifecycle self-types against
each class's real internal name** (not surfaced by `ansify`'s own
DRIFT check at all, since DRIFT only checks argument *count*, never
type — matches `porting-assessment.md`'s B2 finding 2 exactly: "wrong
class name substituted... invisible until `-pe` actually type-checked
them"):

- `messages.ch`'s `InitializeObject`/`FinalizeObject` both restate
  `self` as `struct messwind *` — but `messages.ch` declares `class
  messages: textview[textv] {`, and the real `.c` definitions
  (`messages.c:1727`, `messaux.c:118`) both use `struct messages
  *self`. `messwind` is an unrelated sibling class in the same
  directory (`messwind.ch`) — an easy same-directory copy/paste
  mistake. **`InitializeObject` half is dead documentation** (classpp
  hardcodes the real class name regardless — confirmed in the
  regenerated `.eh`: `boolean messages__InitializeObject(struct
  classheader *, struct messages *);`, correct despite the `.ch`'s
  wrong text). **`FinalizeObject` half is live but non-blocking**:
  confirmed via live compile test (`make messaux.o` — the file that
  actually defines `messages__FinalizeObject`) that this produces only
  a `-Wincompatible-pointer-types` **warning**, not a hard error,
  because `messaux.c` never itself `#include`s `messages.eh` (only
  `messages.c` does, and `messages.c` doesn't define
  `FinalizeObject`) — so the wrong-typed prototype and the real
  definition are never visible together in one translation unit. Still
  a real ~35-year-old `.ch` bug, worth fixing at Gate 1 for
  correctness/consistency, but not a build blocker.
- `mailobjv.ch`'s `InitializeObject`/`FinalizeObject` both restate
  `self` as `struct messwind *` — same typo, but `mailobjv.ch` is
  `class mailobjv : sbuttonv {`, real `.c` definitions
  (`mailobjv.c:114,123`) use `struct mailobjv *self`. **Confirmed live
  and compile-blocking** via an isolated `make mailobjv.o` under a
  temporary `-pe` regen: `error: conflicting types for
  'mailobjv__FinalizeObject'` (declared `struct messwind *`, defined
  `struct mailobjv *`, both visible in the same file). `InitializeObject`
  half is dead documentation, same mechanism as above.
- `text822v.ch`'s `InitializeObject` restates `self` as `struct
  textview *`, and `FinalizeObject` restates it as `struct captions
  *` — two *different* wrong types in the same file. Real internal
  class name is `t822view` (`class t822view[text822v]: messages {`),
  real `.c` definitions (`text822v.c:132,284`) both use `struct
  t822view *self`. **Confirmed live and compile-blocking** the same
  way: `error: conflicting types for 't822view__FinalizeObject'`
  (declared `struct captions *`, defined `struct t822view *`).
  `InitializeObject` half again dead documentation (classpp ignores
  it).

All 3 match B2 finding 2's established taxonomy precisely (same
"`InitializeObject` half dead / `FinalizeObject` half live" split B2
itself documented for 5 of its 6 instances). Fix direction for all 3:
correct the `.ch`'s restated self type to match the real class name
(verified unambiguously against each class's own `.c` definitions,
which all already agree with each other — same zero-blast-radius
methodology as every prior batch's `.ch` fix).

**Finding C — 2 "no signature in DB" skips (`msgsa.c`
`messagesapp__InitializeObject`, `text822.c` `text822__InitializeObject`),
root-caused precisely.** Matches I2's `chartx1a.ch`/`chartx1app`
finding exactly ("a genuine `ansify` signature-DB coverage gap... 
rather than dead code"), and I traced the *exact* mechanism this time:
`overhead/class/pp/class.c:2814` sets the `initializeobject` flag TRUE
for **any** class with a non-empty `data:` section, independent of
whether `InitializeObject` is declared in the `.ch`'s
`classprocedures` section at all (the flag's own comment at line 165:
`"TRUE if this class has data or initializeobject procedure was
found"`). Both `msgsa.ch` (`messagesapp`, no `classprocedures` section
at all) and `text822.ch` (declares only `InitializeClass`, not
`InitializeObject`) have real `data:` sections, so classpp
auto-wires a call to their `InitializeObject` regardless. Confirmed
directly via a live `-pe` regen of both `.eh` files: both show the
hardcoded 2-arg export prototype **and** the internal `__Initialize`
call site wiring it in
(`boolean text822__InitializeObject(struct classheader *, struct
text822 *);` / `if(!text822__InitializeObject(classID, self)){`, same
shape for `messagesapp`). **Both functions are live**, not dead code —
`ansify`'s DB-lookup mechanism structurally can't see this (the DB is
built purely from `.ch` `classprocedures` declarations), so both will
be reported as "skipped: no signature in DB" by the real run too.
Neither is currently a *compile* blocker (both real definitions use
only pointer params, not subject to default-argument promotion, so
they stay compatible with the K&R-tolerant declaration either way) —
but should be hand-converted to ANSI directly at Gate 1 for
consistency, matching I2's `chartx1a.c` precedent exactly (bypass the
DB lookup, weave the known-correct signature by hand:
`boolean text822__InitializeObject(struct classheader *classID, struct
text822 *self)` / `boolean messagesapp__InitializeObject(struct
classheader *classID, struct messagesapp *self)`).

**Finding D — 2 same-file stranded-forward-declaration conflicts
(standing checklist item 8).** Wrote a script importing `ansify`'s own
`parse_local_decls` to resolve every real K&R definition's parameter
types (862 found directory-wide) and cross-checked against all 168
bare empty-parens `static`/`extern` forward declarations found. Two
real, same-file, narrow-by-value-param conflicts — matches T1/AMS1's
established item-8 taxonomy exactly:

- `char64` — `mailobj.c:53` declares `static int char64();`; the real
  definition at `mailobj.c:323` takes `char c` (narrow by-value,
  default-argument-promotion conflict once ANSI'd).
- `AddToClassList` — `ams.c:70` declares `extern int
  AddToClassList();` (with its own comment: `/* same-file forward
  reference -- defined later in this file */`); the real definition at
  `ams.c:1300` takes `(char *TempName, char *FullName, Boolean
  CheckDups)` — `Boolean` (`typedef short Boolean` in `ams/libs/hdrs/
  cui.h`/`ms.h`) is the narrow-typedef case item 8's broadened wording
  specifically calls out.

Both same-file (declaration and definition in one translation unit) —
confirmed real, Gate-1-blocking conflicts, not just theoretical.

**Finding E — 2 more narrow-by-value stranded declarations, confirmed
non-blocking because cross-file (not previously distinguished in the
item-8 write-ups, all of which so far documented only same-file
instances):**

- `ConsiderResettingDescription` — declared `extern int
  ConsiderResettingDescription();` in `folders.c:92`; the real
  definition (`foldaux.c:500`, `Boolean FirstTime` narrow param) lives
  in a **different file** (`foldaux.c`), and `foldaux.c` carries no
  forward declaration of its own for this name. Since `folders.c` and
  `foldaux.c` compile as separate translation units (linked together
  only via `DynamicMultiObject(folders.do, folders.o foldaux.o,...)`),
  the stale empty-parens declaration and the new ANSI prototype are
  never visible together in one `.c` file — no compile-time
  conflicting-types error. Confirmed no hidden shared header declares
  it either.
- `WriteOneFile` — same shape: declared `extern int WriteOneFile();`
  in `sendmsg.c:89`; real definition (`writeone.c:83`, four `Boolean`
  narrow params) lives in `writeone.c`, no same-file forward
  declaration there either, linked via `DynamicMultiObject(sendmsg.do,
  sendmsg.o sendaux.o writeone.o,...)`.

Neither needs a Gate-1 fix for the compile gate to pass — noting both
here as a genuine, if minor, refinement of item 8's scope (the hazard
is specifically same-translation-unit, not "anywhere in the
directory"), not as new taxonomy requiring a ruling.

**Finding F — item 9 (silent multi-line/multi-declaration parser-gap
re-scan): 0 misses in this directory.** Imported `ansify`'s own `HDR`/
`parse_decl_block` and ran them directly against all 23 files; every
K&R candidate header line's declaration block parsed successfully.

**Finding G — liveness: `amss.c`/`amss.ch` are dead in this build.**
`S_DOBJS`/`S_IHFILES` (which include `amss.do`/`amss.ih`) are gated
`#ifdef SNAP_ENV`; confirmed `SNAP_ENV` is commented out in
`build/include/allsys.h:116` and not defined anywhere in
`build/include/site.h` — off. Same shape as O1's WHITEPAGES/M1's
`site.h`-overrides-`allsys.h` precedent. Not a blocker: `ansify
--dry-run` already shows a clean conversion for `amss.c` (0
DRIFT/skip) and `NormalObjectRule()`'s generic pattern rule means
`make amss.o` still works standalone even though it's excluded from
the real `install`/`all` targets — the file will convert and gate
cleanly either way, it just has zero effect on the linked library.
Noted for completeness, no action needed.

**Items 2 (macro-typo grep), 3 (empty-parens lifecycle grep), 4
(installed-header grep), 6 (concurrent-commit check), 7b
(`malloc`/`free`/`realloc`/`calloc` anchored grep): all clean in this
directory** — 0 macro typos; the only empty-parens-lifecycle-shaped
grep hits were ordinary K&R extern forward-declaration stubs in
`messages.c` (`extern boolean messages__InitializeClass();` etc.), not
truly-empty-parens *definitions*, and both real lifecycle definitions
they reference take normal named pointer params (not the O4/B3 danger
shape); 0 installed-header hits for this directory's own non-static
helper names (the one tree-wide hit, `vfclose`, originates in
`ams/libs/cui/pcmchs.c` — see §3.3); `fossil status` clean throughout;
all files with `malloc`/`free`/`realloc`/`calloc` calls already
`#include <stdlib.h>`.

### 3.2 `ams/libs/shr` (7 files, 0 `.ch`)

`ansify --dry-run`: 7 files, 0 compile failures, 0 DRIFT (structurally
impossible, no `.ch`). 3 files ("no K&R definitions converted"):
`amserr.c`, `mimepart.c`, `mimeparttest.c` — already ANSI or trivial.
4 files with helpers to convert: `brkdown.c` (3), `findroot.c` (1),
`tildes.c` (1), `utils.c` (8).

- **Item 7 (`malloc`/`free` anchored grep): 2 files missing
  `#include <stdlib.h>`** despite calling `malloc`/`free`:
  `brkdown.c` (1 `malloc` call) and `tildes.c` (1 `malloc`, 4 `free`
  calls). **`tildes.c` also has its own stale K&R redeclaration**
  (`extern char *malloc();`, line 43) that will conflict with
  `<stdlib.h>`'s `void *malloc(size_t)` once the include is added —
  matches A1's `doc/mkbrowse/browser.c` finding exactly ("a stale
  `char *malloc()` K&R redeclaration... removed"). Fix at Gate 1: add
  `#include <stdlib.h>` to both files, remove `tildes.c`'s stale
  local redeclaration.
- **Item 8: 0 conflicts** (no `static`/`extern` empty-parens forward
  declarations at all in this directory).
- **Item 9: 1 silent parser-gap miss**, matching AMS1's
  already-ruled multi-line-declaration parser gap exactly (explicitly
  pre-authorized by the prompt — no fresh ruling needed):
  `utils.c:135`, `BuildNickName(FullName, NickName)` — the parameter
  declaration block spans two physical lines (`char *FullName,\n
  *NickName;`), which `ansify`'s `parse_decl_block` requires to be one
  complete statement per physical line. Hand-convert at Gate 1 per
  AMS1's precedent (`char *FullName, char *NickName` are both plain
  pointers, no promotion risk either way).
- Items 2/6: clean.

### 3.3 `ams/libs/cui` (3 files, 0 `.ch`)

`ansify --dry-run`: 3 files, 0 compile failures, 0 DRIFT. `andmchs.c`
(8 helpers), `cuilib.c` (84 helpers), `pcmchs.c` (6 helpers).

**Finding H — `pcmchs.c` is dead code, not linked into `libcui.a`.**
The Imakefile's `OBJS = cuilib.o andmchs.o` never references
`pcmchs.o` at all (confirmed reading the full Imakefile — no `#ifdef`
gating, it's just absent). `NormalObjectRule()`'s generic `.c.o:`
pattern rule still lets `make pcmchs.o` succeed as a standalone
target (confirmed via `Makefile`'s auto-generated dependency lines for
`pcmchs.o`), which is why `ansify`'s directory-wide compile gate still
reaches it — but it carries zero real runtime blast radius, matching
O1's WHITEPAGES precedent and I1/I2's "present in directory, absent
from real build" dead-file pattern exactly. Everything below sourced
from `pcmchs.c` is inert either way; still worth converting for
consistency (matching how prior batches treated similarly-dead files),
just not build-critical.

- **Item 4 (installed-header grep): 1 tree-wide hit, cross-directory,
  confirmed safe.** `pcmchs.c:217` defines a non-static `vfclose(fp)
  FILE *fp;`. `build/include/util.h:244` (installed from the
  unrelated, out-of-batch `overhead/util/lib`) has a stale `extern int
  vfclose();` empty-parens declaration. Matches O3's
  `scan822.c`/`mail.h` precedent exactly — a real cross-directory
  header touch, flagged prominently per that established rule, but
  confirmed **safe**: `FILE *` is a pointer, not subject to
  default-argument promotion, so the empty-parens declaration stays
  fully compatible with the new ANSI prototype regardless. (Also moot
  in practice since `pcmchs.c` itself is dead code — see Finding H —
  but the mechanism is worth recording regardless of this instance's
  liveness, per O3's own reasoning.)
- **Item 7: `pcmchs.c` also missing `#include <stdlib.h>`** despite 2
  `malloc` + 1 `free` call, **and has its own stale `char *malloc();`
  local redeclaration** (line 56) — same A1/mkbrowse shape as
  `ams/libs/shr/tildes.c` above. Optional cleanup given the file is
  dead code, but harmless and consistent to fix at Gate 1 alongside
  the live fixes.
- **Item 8: 0 conflicts.** 3 bare empty-parens forward declarations
  exist in this directory (`andmchs.c`: `fpacheck`, `MS_FastUpdateState`,
  `CUI_GenTmpFileName`), none match a narrow-param real definition
  found in-directory.
- **Item 9: 11 silent parser-gap misses**, all matching AMS1's
  already-ruled multi-line/crammed-declaration parser gap (no fresh
  ruling needed): 10 in `cuilib.c`
  (`CUI_CreateNewMessageDirectory`, `CUI_GetHeaders`, `CUI_GetCuid`,
  `CUI_GetAMSID`, `CUI_GetPartialBody`, `CUI_NameReplyFile`,
  `CUI_AlterSnapshot`, `CUI_GetHeaderContents`, `GetHeaderContents`,
  `CUI_BuildNickName`) and 1 in `pcmchs.c` (`amsconfig`, the
  crammed-one-physical-line variant: `int argc; char **argv, *name;`
  on one line). Worth noting: `CUI_GetHeaders` is
  `porting-assessment.md` §12's canonical historic
  `long`/`int`-mismatch example — confirmed its current parameter
  types (`long startbyte, *nbytes, *status;`) already reflect that
  earlier LP64 fix; this finding is purely a fresh `ansify`
  parser-gap miss on top of already-correct types, not a recurrence of
  the historic bug. All 11 hand-convert at Gate 1 per AMS1's
  precedent.
- Items 2/6: clean.

### 3.4 `ams/libs/nosnap` (1 file, 0 `.ch`)

`ansify --dry-run`: 1 file, 0 compile failures, 0 DRIFT, "would
convert: 0 methods, 0 classprocs, 2 helpers".

- **Item 7: missing `#include <stdlib.h>`** despite 4 `malloc` + 1
  `free` call sites (`nosnap.c`). No stale local `malloc()`
  redeclaration found (checked). Fix at Gate 1: add the include.
- **Items 2/4/6/8/9: all clean.**

## 4. Summary table

| Directory | `.ch` | DRIFT (raw) | Skips (raw) | Real findings needing Gate-1 fix | UNCLASSIFIED |
|---|---|---|---|---|---|
| `atkams/messages/lib` | 17 | 3 (all `fldtreev.ch`, both known-taxonomy shapes) | 2 (`msgsa.c`, `text822.c` — I2 `chartx1a` shape) | `fldtreev.ch` (2 of 3 lifecycle members), 3 self-type typos (`messages.ch`, `mailobjv.ch`, `text822v.ch`), 2 hand-converts (DB-gap), 2 same-file stranded-decl conflicts (`char64`, `AddToClassList`), 2 cross-file narrow-param notes (non-blocking) | 0 |
| `ams/libs/shr` | 0 | 0 | 0 | 2 missing `#include <stdlib.h>` (+1 stale `malloc()` redecl), 1 parser-gap hand-convert | 0 |
| `ams/libs/cui` | 0 | 0 | 0 | 1 dead-file note (`pcmchs.c`), 1 cross-directory header touch (safe), 1 missing `#include <stdlib.h>` (+1 stale `malloc()` redecl, optional), 11 parser-gap hand-converts | 0 |
| `ams/libs/nosnap` | 0 | 0 | 0 | 1 missing `#include <stdlib.h>` | 0 |

**Zero UNCLASSIFIED findings across all 4 directories.** Every finding
matches an existing taxonomy entry (cited inline above per finding),
most confirmed with a live `-pe` compile check rather than a
read-by-hand alone. No new `ansify`-tool-level bug found (the 12
parser-gap misses are all instances of AMS1's already-ruled,
deliberately-unfixed multi-line/crammed-declaration gap).

## 5. Files touched

None — Gate 0 only. Every scratch `Imakefile`/`.eh`/`.ih`/`.o` edit
made during compile-check experiments was reverted/deleted before
moving to the next check. `fossil status` confirms:

```
ADDED      revival/doc/m3-ams2-prompt.md
```

(pre-existing from before this session started; this report add is
new and expected). No other files changed.

## 6. Open questions / what surprised me

- The `messages.ch`/`mailobjv.ch`/`text822v.ch` self-type-typo cluster
  (3 instances in one directory) is a higher density of this B2-shape
  bug than any prior batch has hit in one place — worth knowing if a
  systematic sweep for this pattern (grep every `.ch`'s lifecycle
  self-type against its own class declaration line, tree-wide) turns
  out worthwhile later; I did not attempt that here, only this batch's
  4 in-scope directories.
- The `msgsa.c`/`text822.c` "skipped: no signature in DB" root cause
  (`class.c:2814`'s data-triggers-`initializeobject` mechanism) is a
  cleaner, more precise explanation than I2's `chartx1a.c` writeup
  gave (which cited only the observable *effect* — the `.eh`'s `New()`
  wiring — not the mechanism). Might be worth folding this precise
  citation into `porting-assessment.md` §17 the next time that section
  gets a substantive edit, so future batches don't have to
  re-derive it.
- Finding E (cross-file narrow-param stranded declarations are
  non-blocking, unlike same-file ones) is a small but real refinement
  of standing checklist item 8's scope that I don't think has been
  written up explicitly before — flagging it here in case it's worth
  folding into the runbook's item-8 wording.
- `pcmchs.c` being entirely dead code inside a 3-file directory (1/3
  of `ams/libs/cui`'s file count) was not anticipated by the prompt
  and took some digging to confirm (the `OBJS` line doesn't `#ifdef`-gate
  it, it's just silently absent) — worth double-checking at Gate 1
  that converting it doesn't somehow get skipped/mishandled given it's
  never actually built by the real install target.

## 7. Suggested runtime checks for wdc

Deferred to the Gate 1 update to this report, once real fixes are
in place and there's something concrete to exercise — the prompt's
own guidance is to identify checks that exercise the *specific*
functions hand-fixed, and none are fixed yet at Gate 0.

---

**Stopped at Gate 0. Waiting for the orchestrator's ruling before
proceeding to Gate 1 — though as noted above, I found zero items that
need one; every finding matches existing taxonomy. Please confirm
whether to proceed to the real `ansify --dir` run as scoped in the
prompt, or whether anything above needs a different ruling than the
one I've proposed inline.**

---

## GATE 1 — real run (orchestrator ruling received, proceeded as scoped)

### 8. Status

**Gate 1 complete.** All 4 directories converted, all fallout resolved
or explicitly left as documented dead-code/pre-existing exceptions, all
4 subtree-local gates clean twice, tree-wide gate clean twice (log at
`andrew-6.4/dependInstall.log`, only the 2 known pre-existing
`contrib/zip/utility/ltapp.c` errors, unrelated, queued for Wave 7 C2).
No fossil commit made. No GUI/terminal binary launched.

Mid-task note: this session was interrupted once (an unrelated
orchestrator-side usage-limit event, not a problem with the work) right
after the very first real `ansify --dir` pass on `atkams/messages/lib`
and 3 manual fixes to `ams.c`. Resumed by re-deriving state from
`fossil status`/`fossil diff` before continuing — confirmed the 4
`.ch` fixes and the first `ansify --dir` pass were already on disk,
picked up exactly where left off, no rework and no skipped steps.

### 9. What I did, in order

1. **`.ch` fixes, applied before any real `ansify --dir` run** (so the
   signature DB and the mechanical conversion both pick up the correct
   types from the start, rather than needing a second pass):
   - `fldtreev.ch`: `InitializeClass()` simplified to empty parens
     (was restating `classID` by name); `FinalizeObject` changed to
     restate `self` only (was restating both `classID` and `self`,
     producing a 3-param prototype against the real 2-param
     definition); `InitializeObject` left untouched exactly as ruled.
   - `messages.ch`, `mailobjv.ch`, `text822v.ch`: corrected the
     self-type typo in **both** the `InitializeObject` and
     `FinalizeObject` restatements (not just the live `FinalizeObject`
     half) to each class's real name (`messages`, `mailobjv`,
     `t822view`) — fixing `InitializeObject`'s half too, even though
     it's dead documentation, matters here: leaving it wrong would
     have made `ansify`'s own DB-lookup conversion weave the **wrong
     type directly into the real ANSI-converted `.c` definition**
     (worse than the original K&R, where the wrong `.ch` type was at
     least inert). Confirmed this reasoning by checking `weave()`'s
     mechanics in the tool source before editing.
2. Rebuilt the signature DB (`ansify --build-db`) — 496 classes, 70
   pre-existing failures (all confined to `atk/examples`,
   `contrib/atkbook`, `rdemo/hide`, and the already-known
   `sliderv`/`sliderV` case-collision — matches the already-documented
   baseline from the 2026-07-31 same-classname-collision fix exactly,
   confirming the `.ch` edits introduced no new collision). Verified
   all 4 edited classes' `.desc` entries updated correctly before
   proceeding.
3. Added `CLASSFLAGS = $(CLASSINCLUDES) -pe` for real to
   `atkams/messages/lib/Imakefile`, regenerated `Makefile`, force-
   regenerated all 17 `.eh` files, spot-checked the 4 edited classes'
   emitted prototypes directly (all correct — `fldtreev`'s 1/2/2-param
   split now matches its real `.c`; the 3 self-type classes now emit
   the right struct tags).
4. Ran the real (non-dry-run) `ansify --dir src/atkams/messages/lib`.
   Result: 1 DRIFT (the expected, already-classified `fldtreev.ch`
   `InitializeObject` false positive — left K&R, harmless, matches
   prediction exactly), 7 compile failures, all auto-reverted.
5. Diagnosed each of the 7 failures with a scratch diagnostic script
   (imports `ansify`'s own `convert_file`, runs it plus the real fix
   tools against the real file, compiles, prints the **full** compiler
   output rather than `ansify`'s own 12-line-truncated summary, then
   always restores the original — never left a failed conversion on
   disk). Findings, all fixed by hand before letting `ansify` reconvert
   the file for real:
   - **`ams.c`** — 3 issues: (a) a real rock-idiom bug,
     `ams__SetCUIRock`'s `.ch`-declared `void *r` assigned directly to
     `static long MyRock` — fixed with `(long)` cast, matching the
     established B1/I2 rock-idiom precedent; (b) the already-classified
     `AddToClassList` same-file stranded declaration — retyped to the
     real definition's `(char *TempName, char *FullName, Boolean
     CheckDups)`; (c) a **new, not-Gate-0-predicted** finding: a plain
     classproc (`ams__CountAMSViews`, not one of the three special
     lifecycle names) written with **truly empty parens and zero
     parameters at all** — invisible to `ansify`'s candidate detector
     for the same structural reason as O4/B3's lifecycle-method finding
     (`BARE_PARAMS` requires at least one bare identifier), but here
     generalized to an ordinary classproc for the first time in this
     project. Hand-folded to ANSI with a named `classID` param, same
     fix shape as every prior instance of this species.
   - **`mailobj.c`** — the already-classified `char64` same-file
     stranded declaration, **plus** a second, new-shape instance of the
     same underlying hazard: `fix-missing-static-decl` inserted a
     **fresh** empty-parens stub for `hexchar` (which had no
     pre-existing declaration at all) that then conflicted with
     `hexchar`'s own narrow `char c` real definition — the exact
     "same-run, tool-self-inflicted" variant AMS1 already documented
     for `atk/help/src/helpa.c`. Fixed both by hand-retyping full
     prototypes ahead of the real run (`char64(char c)`,
     `hexchar(char c)`), confirmed via the diagnostic script that this
     resolves cleanly before trusting the real run.
   - **`sendmsg.c`** — same-file stranded declaration, `Submit`
     (`Boolean Unformat, UseMultipartFormat` narrow params) — retyped.
   - **`stubs.c`** — same-file stranded declaration, but in a
     **multi-name comma-list** (`extern int SubtleDialogs(),
     ChooseFromList(), WriteOutUserEnvironment(), ...` — 15 names on
     one declaration). Checked all 15 real definitions; only
     `WriteOutUserEnvironment` (`Boolean IsAboutMessages`) has a narrow
     param. Split it out of the list into its own full prototype,
     left the other 14 untouched (all `int`/`char*`, no promotion
     risk).
   - **`text822.c`** — **new finding, not in Gate 0**: `ReadIntoText`
     and `ResetGlobalStyle` are both `text822`-specific classprocs (not
     overrides) whose `.ch` correctly declares the general parent type
     (`struct text *`, matching every real caller — confirmed by
     checking actual caller argument types in `capaux.c`/`sendmsg.c`,
     which genuinely pass `struct text *` values), but whose real `.c`
     definitions narrow the parameter to `struct text822 *` for
     internal field access. Same species as B2 finding 5/I1's
     `srctextv.ch` finding ("override typed to the general base
     interface, but the body needs the concrete subclass"), just on a
     plain classproc rather than a formal override. Fixed with the
     established pattern: renamed the parameter to `..._generic` (kept
     at the correct general type), added a local
     `struct text822 *d = (struct text822 *) d_generic;` cast for body
     use. Also hand-converted the already-known DB-gap
     `text822__InitializeObject` (matches I2's `chartx1a.c` precedent
     — `text822.ch` declares only `InitializeClass`, not
     `InitializeObject`, but the class has a `data:` section so
     classpp auto-wires it live regardless).
   - **`text822v.c`** — same generic-param-plus-cast shape, one
     instance: `DeleteApplicationLayer`'s `.ch` type (`struct view
     *scrollbar`) is the correct general interface (confirmed against
     3 sibling classes' identical convention — `atk/chart/chartv.c`,
     `atk/text/textv.c`, `atk/preview/preview.c` all declare this
     override as plain `struct view *`), but the real definition
     narrowed it to `struct scroll *`. Fixed the same way (`_generic`
     rename + local cast).
   - **`amss.c`** — investigated and **deliberately left K&R,
     unconverted**, matching Gate 0's prediction exactly: the
     implicit-declaration cascade (~19+ errors before clang's error
     limit truncated further output) is not `ansify`/M3 fallout at
     all — `amss.c` calls dozens of `CUI_*` functions that its sibling
     `amsn.c` declares via ~50 lines of local `extern` declarations
     (visible directly in the `amsn.c`/`amss.c` diff), but `amss.c`
     itself never received that same M2-era treatment because it has
     never once been compiled in this checkout (`SNAP_ENV` off,
     confirmed in Gate 0). This is O1's "activating a gated-off
     directory" playbook territory (add the missing declarations,
     catch up its own M2 sweep, *then* ansify it fresh, as its own
     small project) — clearly out of scope for a `-pe`-only M3 batch.
     `ansify`'s own auto-revert-on-failure handled this exactly as
     designed; no manual intervention needed or taken.
   - Also hand-converted the second DB-gap skip, `msgsa.c`'s
     `messagesapp__InitializeObject`, same mechanism/precedent as
     `text822.c`'s above.
6. Re-ran the real `ansify --dir src/atkams/messages/lib` with all the
   above fixes in place. Result: **1 compile failure (`amss.c`, exactly
   the predicted, deliberately-unfixed exception), 1 DRIFT (the
   expected `fldtreev.ch` `InitializeObject` false positive), 0 other
   failures** — every other file (21 of 22 real, non-dead `.c` files)
   converted clean on this pass.
7. Subtree-local gate for `atkams/messages/lib`: `make clean`, `make
   depend`, `make -k install` — twice, separate calls, absolute paths.
   Both clean, 0 `error:` lines, 22 `cc` invocations each time (matches
   22 real `.c` files; `amss.c` correctly excluded from `DOBJS` by its
   own `#ifdef SNAP_ENV` gate).
8. **`ams/libs/shr`** (0 `.ch`, no `-pe` needed): applied Gate 0's
   3 predicted fixes first — hand-converted the `utils.c`
   `BuildNickName` parser-gap miss (multi-line K&R block, both params
   plain `char *`, no promotion risk); added `#include <stdlib.h>` to
   `brkdown.c` and `tildes.c`; removed `tildes.c`'s stale
   `#if !POSIX_ENV / extern char *malloc(); / #endif` block entirely
   (confirmed via direct preprocessing that `POSIX_ENV` is defined in
   this build, so the block was already inert — removed anyway per the
   ruling, for consistency and to avoid future confusion). Ran the real
   `ansify --dir` — 0 compile failures, 0 DRIFT, exactly as predicted.
   Gated clean twice.
9. **`ams/libs/cui`** (0 `.ch`): applied Gate 0's predicted fixes
   first (`pcmchs.c`'s `amsconfig` parser-gap hand-conversion,
   `#include <stdlib.h>`, removed its `#ifndef _IBMR2 / char
   *malloc(); / #endif` stale block), then hand-converted the other 10
   `cuilib.c` parser-gap misses (all multi-line/two-line-return-type
   K&R blocks — `CUI_CreateNewMessageDirectory`, `CUI_GetHeaders`
   (`long`-returning, two-line form), `CUI_GetCuid`, `CUI_GetAMSID`,
   `CUI_GetPartialBody`, `CUI_NameReplyFile`, `CUI_AlterSnapshot`
   (`long`-returning, two-line form), `CUI_GetHeaderContents`,
   `GetHeaderContents`, `CUI_BuildNickName`). Ran the real `ansify
   --dir` — surfaced **5 more same-run, tool-self-inflicted stranded-
   declaration conflicts, not predicted at Gate 0**:
   `CUI_PurgeMarkedDirectories`, `GetViceFileToNewString`, `pfclose`,
   `CUI_FixAttribute`, `CUI_FixAttributeByNumber` — all narrow
   `Boolean` params, all pre-existing in **two large multi-name
   comma-list declarations** (17 and 19 names respectively) that Gate
   0's item-8 script structurally could not see (it only matched
   single-name empty-parens declarations, not comma lists — the same
   blind spot as the `stubs.c` `WriteOutUserEnvironment` case above,
   just not yet hit at Gate 0 since dry-run gives no visibility into
   which names have narrow real definitions). Checked every name in
   both lists (24 total) against its real definition; only these 5 are
   narrow. Split them out into individual full prototypes, left the
   other 19 untouched. Re-ran — **`cuilib.c` and `andmchs.c` both
   clean**; **`pcmchs.c` still fails**, but for a reason entirely
   unrelated to M3: `pcmchs.c` is explicitly commented "Machine
   dependent modules -- PC Version" and calls SNAP-specific
   (`SNAP_ClientInit`, `SNAP_BeginConv`, `SNAP_CPARMS`, ...) and
   IBMPC-specific (`spawnvp`, `P_WAIT`) identifiers that only become
   visible when `SNAP_ENV`/`IBMPC` are defined — neither is, in this
   build. **Same species as `amss.c` above** (pre-existing,
   unrelated, out-of-scope gated-dead-code), confirmed via the same
   kind of full-error diagnostic. My earlier hand-fixes to `pcmchs.c`
   (`stdlib.h`, the stale `malloc()` removal, the `amsconfig`
   parser-gap conversion) all **survived** `ansify`'s auto-revert,
   since they were on disk before `ansify` took its own backup —
   confirmed directly by re-reading the file after the failed run.
   **`pcmchs.c` is not linked into `libcui.a` at all** (`OBJS =
   cuilib.o andmchs.o` in the Imakefile, confirmed again directly) —
   its conversion status has zero effect on the real build; flagging
   this explicitly here (as requested) so it isn't mistaken for a
   live-build change during independent re-verification. Gated clean
   twice (`ar clq libcui.a cuilib.o andmchs.o` both times — `pcmchs.o`
   correctly never attempted by the real install target).
10. **`ams/libs/nosnap`** (0 `.ch`): added `#include <stdlib.h>` to
    `nosnap.c`. Ran the real `ansify --dir` — 0 compile failures, 0
    DRIFT, exactly as predicted. Gated clean twice.
11. Tree-wide gate (`make -C src dependInstall`, twice, logged to
    `andrew-6.4/dependInstall.log` per the standing convention — note
    the correct invocation is `make -C src dependInstall`, **not**
    `make dependInstall` from the tree root, which has no such target;
    worth a corrected note for the next batch if this trips anyone else
    up). Both runs clean and byte-for-byte identical except line
    numbers: the same 2 pre-existing `contrib/zip/utility/ltapp.c`
    errors (`boolean`-to-`void*` int-conversion, unrelated to this
    batch, already queued for Wave 7 C2), 0 other `error:` lines. All 4
    of this batch's directories confirmed descended
    (`building (dependInstall) (...)"` present for all 4 in the log).

### 10. Per-file `ansify` conversion counts (final, real run)

**`atkams/messages/lib`** (17 `.ch` files, all `-pe`'d and confirmed —
`ams`, `amsn`, `amss`, `amsutil`, `captions`, `fldtreev`, `folders`,
`mailobj`, `mailobjv`, `messages`, `messwind`, `msgsa`, `nbutterv`,
`options`, `sendmsg`, `text822`, `text822v`):

| File | methods | classprocs | helpers | notes |
|---|---|---|---|---|
| ams.c | 95 | 31 | 9 | + 3 hand fixes (rock cast, stranded decl, empty-parens classproc) |
| amsn.c | 95 | 1 | 0 | |
| amss.c | — | — | — | **left K&R** (pre-existing, SNAP_ENV-gated, out of scope) |
| amsutil.c | 0 | 30 | 1 | |
| capaux.c | 11 | 0 | 18 | |
| captions.c | 36 | 3 | 20 | |
| fldtreev.c | 3 | 2 | 2 | InitializeClass+FinalizeObject now convert; InitializeObject DRIFT (expected FP) |
| foldaux.c | 6 | 0 | 17 | |
| folders.c | 23 | 3 | 17 | |
| mailobj.c | 7 | 5 | 7 | + char64/hexchar stranded-decl fixes |
| mailobjv.c | 3 | 3 | 1 | |
| messages.c | 4 | 1 | 94 | |
| messaux.c | 2 | 1 | 19 | |
| messwind.c | 3 | 3 | 5 | |
| msgsa.c | 2 | 0 | 0 | + 1 hand-converted (InitializeObject DB-gap) |
| nbutterv.c | 1 | 0 | 0 | |
| options.c | 0 | 1 | 5 | |
| sendaux.c | 3 | 0 | 31 | |
| sendmsg.c | 24 | 3 | 35 | + Submit stranded-decl fix |
| stubs.c | 0 | 0 | 29 | + WriteOutUserEnvironment stranded-decl fix |
| text822.c | 7 | 3 | 24 | + 1 hand-converted (InitializeObject DB-gap) + 2 generic-param-plus-cast fixes |
| text822v.c | 10 | 3 | 5 | + 1 generic-param-plus-cast fix |
| writeone.c | 0 | 0 | 1 | |

**`ams/libs/shr`**: brkdown.c 3 helpers, findroot.c 1, tildes.c 1,
utils.c 8 (+1 hand-converted, `BuildNickName`); amserr.c/mimepart.c/
mimeparttest.c already ANSI (0).

**`ams/libs/cui`**: cuilib.c 84 helpers (+10 hand-converted parser-gap
+ 5 stranded-decl retypes); andmchs.c already ANSI (0); pcmchs.c 6
helpers converted by `ansify` on the failed attempt but reverted (left
K&R except the 1 hand-converted `amsconfig` and the `stdlib.h`/
`malloc()` fixes, which survive independently of the compile gate).

**`ams/libs/nosnap`**: nosnap.c 2 helpers.

### 11. Gate results

- `atkams/messages/lib`: clean × 2 (0 errors each).
- `ams/libs/shr`: clean × 2 (0 errors each).
- `ams/libs/cui`: clean × 2 (0 errors each; `pcmchs.o` never attempted
  by the real install target).
- `ams/libs/nosnap`: clean × 2 (0 errors each).
- Tree-wide (`make -C src dependInstall`): clean × 2, identical results
  both times — only the 2 known pre-existing `contrib/zip/utility/
  ltapp.c` errors (unrelated, queued for Wave 7 C2).

### 12. `fossil status` / `fossil extras`

`fossil status` (37 files edited, no commit):

```
ADDED      revival/doc/m3-ams2-prompt.md          (pre-existing, before this session)
EDITED     src/ams/libs/cui/andmchs.c
EDITED     src/ams/libs/cui/cuilib.c
EDITED     src/ams/libs/cui/pcmchs.c
EDITED     src/ams/libs/nosnap/nosnap.c
EDITED     src/ams/libs/shr/brkdown.c
EDITED     src/ams/libs/shr/findroot.c
EDITED     src/ams/libs/shr/mimepart.c
EDITED     src/ams/libs/shr/mimeparttest.c
EDITED     src/ams/libs/shr/tildes.c
EDITED     src/ams/libs/shr/utils.c
EDITED     src/atkams/messages/lib/Imakefile
EDITED     src/atkams/messages/lib/{ams,amsn,amsutil,capaux,captions,
            fldtreev,foldaux,folders,mailobj,mailobjv,messages,
            messaux,messwind,msgsa,nbutterv,options,sendaux,sendmsg,
            stubs,text822,text822v,writeone}.c
EDITED     src/atkams/messages/lib/{fldtreev,mailobjv,messages,
            text822v}.ch
```

22 `.c` + 4 `.ch` + 1 `Imakefile` in `atkams/messages/lib`, 6 `.c` in
`ams/libs/shr`, 3 `.c` in `ams/libs/cui`, 1 `.c` in `ams/libs/nosnap` =
37 files, matching the 23−1(`amss.c`, deliberately untouched) + 4 +
6 + 3 + 1 arithmetic exactly.

`fossil extras`, filtered to this batch's directories: exclusively
ordinary generated build byproducts (`.o`, `.do`, `.eh`, `.ih`,
`Makefile`, `Makefile.BAK`, `install.time`/`install.doc`, the 3
installed `.a` libraries, and `atkams/messages/lib/index` — a normal
generated class-index file produced by `InstallClassFiles`, freshly
dated from this session's own gate runs). No stray pre-existing
untracked files noticed in this batch's directories (unlike I2's
`fontselv.c.orig` — nothing analogous found here).

### 13. Anything that surprised me / doc-fold suggestions (flagging again per instruction, not acting on them)

- Two genuinely new fallout shapes not in Gate 0's prediction, both
  now matching (or extending) established taxonomy rather than needing
  a fresh ruling: (a) a **plain classproc** (not one of the 3 special
  lifecycle names) written with truly-empty K&R parens
  (`ams__CountAMSViews`) — the O4/B3 "invisible to `ansify`'s candidate
  detector" finding generalizes beyond `InitializeClass`/
  `InitializeObject`/`FinalizeObject` to any classproc/method with zero
  named parameters; (b) **multi-name comma-list stranded declarations**
  are invisible to Gate 0's item-8 script (and to the checklist's own
  suggested single-name regex) — found 3 real instances this way
  (`stubs.c`'s 15-name list, `cuilib.c`'s two lists of 17 and 19) that
  a narrower single-name grep structurally cannot see. Recommend
  broadening item 8's standing grep guidance to explicitly call out
  comma-list declarations as a shape to check, not just
  `NAME();`-per-line.
- Repeating from the Gate 0 section, now with more confidence after
  seeing it hold up in the real run: the `msgsa.c`/`text822.c`
  `InitializeObject` DB-gap root cause (`class.c:2814`'s
  data-triggers-`initializeobject` mechanism, independent of whether
  `InitializeObject` is declared in the `.ch`) is worth folding into
  `porting-assessment.md` §17 as the precise mechanism behind I2's
  `chartx1a.c` finding, which only described the effect.
- Also repeating: standing checklist item 8's same-file-vs-cross-file
  distinction (a stranded declaration only becomes a real compile
  blocker when the declaration and the narrow-typed definition share a
  translation unit — confirmed concretely this batch via
  `folders.c`/`foldaux.c`'s `ConsiderResettingDescription` and
  `sendmsg.c`/`writeone.c`'s `WriteOneFile`, both cross-file and
  confirmed non-blocking) is worth folding into the runbook's item-8
  wording.
- The tree-wide gate's correct invocation is `make -C src
  dependInstall`, not `make dependInstall` from the tree root (the
  latter fails immediately with "No rule to make target" — there's no
  top-level `Makefile` in `andrew-6.4/`, only in `andrew-6.4/src/`).
  Not a new finding, just a corrected invocation note in case it's not
  already written down somewhere central.
- `pcmchs.c` turning out to require a from-scratch "PC/SNAP subtree
  activation" (same shape as `amss.c`, just discovered independently
  and for a different reason) inside a 3-file directory was not
  anticipated — worth remembering that "0 `.ch` files, small directory"
  doesn't guarantee a uniformly easy batch.

### 14. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` first to
confirm live consumers, never launch GUI apps from the session, no
saves against unversioned fixtures.

```
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/amsn.do | grep -E ' T (ams__SetCUIRock|ams__CountAMSViews|foldertreev__|messages__|mailobjv__|t822view__|text822__|messagesapp__)'
```

`atkams/messages/lib` is the `messages` GUI backend — every fix this
batch made lives in code paths a normal `messages` session exercises.
Specific, targeted suggestions (not a generic "open messages"):

- **`fldtreev.ch` fix (folder tree lifecycle)**: open `messages`,
  expand/collapse a folder tree entry (the "Folders" pane, if visible)
  or trigger a fresh folder-tree redraw. Exercises
  `foldertreev__InitializeClass`/`FinalizeObject`, now correctly typed.
  A crash or garbage folder-tree display here would indicate the
  `.ch` fix direction was wrong (unlikely — the real `.c` definitions
  were the ground truth throughout — but this is the one fix in this
  batch with a `-pe`-live self-inconsistency risk if wrong).
- **`messages.ch`/`mailobjv.ch`/`text822v.ch` self-type fixes**: open
  and close a "Bodies" (text822v) window and a mail-object display
  (mailobjv) — e.g. read a message with an attachment/body view, then
  close the window (exercises `FinalizeObject`) and reopen one
  (exercises `InitializeObject`). These 3 fixes are pure interface
  hygiene (the real `.c` bodies never changed), so no behavior change
  is expected — the check is "does it still work exactly as before,"
  not "does something new happen."
- **`ams.c`'s `ams__SetCUIRock`/`MyRock` cast fix**: this stores/
  retrieves an opaque rock value used during `cui` client
  initialization — hard to exercise directly from `messages` UI
  interaction; the gate's own clean compile plus the fact the fix is a
  type-only cast (no logic change) is the practical extent of
  verification available without deeper `cui` client tracing. Flagging
  explicitly as **not independently runtime-verified** — same category
  as AMS1's still-open `FreeMessageContents` item.
- **`ams.c`'s `AddToClassList`, `mailobj.c`'s `char64`/`hexchar`,
  `sendmsg.c`'s `Submit`, `stubs.c`'s `WriteOutUserEnvironment`,
  `cuilib.c`'s 5 stranded-declaration retypes**: all pure
  declaration-only fixes (retype a forward declaration to match an
  unchanged real definition) — no behavior change by construction, so
  general `messages`/`cui` non-regression (composing/sending a
  message, which exercises `Submit`'s real caller path in
  `sendmsg.c`) is the appropriate check, not a fix-specific one.
- **`text822.c`'s `ReadIntoText`/`ResetGlobalStyle` and
  `text822v.c`'s `DeleteApplicationLayer` generic-param-plus-cast
  fixes**: open a message body (exercises `ReadIntoText` — this is the
  core "parse an 822 body into a text object" path, used every time a
  message is displayed), and close a Bodies window with its scrollbar
  present (exercises `DeleteApplicationLayer`). These are read-only
  view operations — safe to exercise against any real mailbox/message,
  no fixture-save risk.
- **`cui`/`libcui.a`/`libmsshr.a`/`libcuin.a` (the 3 support-library
  directories)**: `cuin`'s `dirinfo`/`flag`/`unflag` commands (already
  established as clean in A1/AMS1) remain a reasonable general
  smoke-test for `libcui.a`/`libmsshr.a` linkage, but per AMS1's own
  corrected lesson, **do not assume `dirinfo` exercises any of this
  batch's specific fixes** — none of them are on that code path. No
  specific `cuin`/`libmsshr.a` fix in this batch needs its own targeted
  check beyond the gate's own clean compile (all of `ams/libs/shr`'s
  and 4 of `ams/libs/cui`'s 5 fixes are declaration-only/`stdlib.h`
  fixes with no behavior change).
- **`amss.c`/`pcmchs.c` (left K&R, dead code)**: no runtime check
  applicable or needed — neither is linked into any binary in this
  build (`SNAP_ENV`/`IBMPC` both off).

Suggest wdc run a normal `messages` session (compose, read a message
with a body, browse folders) as the general non-regression pass, plus
the two body-view-specific actions above (open a message, open/close
a Bodies window) as the closest available exercise of the two
generic-param-plus-cast fixes.

---

**Gate 1 complete. Stopping per the prompt's instructions — no commit
made, no GUI/terminal binary launched interactively. Ready for the
orchestrator's independent re-verification.**
