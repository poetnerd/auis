# M3 Wave 4, Batch I1: 10 inset-adjacent directories — ansify + `-pe`/`.eh` rollout — REPORT

## 1. Status

Stopped at Gate 0, as instructed. Pre-diagnosis/classification only —
no real `ansify --dir` run, no permanent `-pe`, no `.ch`/`.c` edits, no
fossil commits. `fossil status` is clean (nothing changed in the
working tree); the one temporary experiment (see §3.1) touched a
scratch copy outside the repo, never the real source file.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `m3-i1-insets-batch1-prompt.md`,
   `rollout-procedure.md`, `m3-rollout-runbook.md` in full (the
   "Current standing per-batch checklist" and "Session structure going
   forward" sections plus all findings entries O1 through T1), and
   `porting-assessment.md` §14 (Delegation) and §17 (the
   `InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing
   and its two documented exceptions), plus `m3-batches.md`'s Wave 4
   section, per the prompt's instructions.
2. Confirmed the `ansify` signature DB (`build/desc/`) was fresh
   (timestamps from earlier the same day) — no `--build-db` rebuild
   needed.
3. For each of the 10 directories, in the order the prompt specifies:
   `.ch` presence check → `ansify --dry-run --dir` → the full standing
   checklist (macro-typo grep, empty-parens-lifecycle grep,
   installed-header grep for non-static helpers, restated-lifecycle-
   param `.ch` check, `fossil status` concurrent-commit check,
   liveness census, anchored `malloc`/`free`/`realloc`/`calloc` grep,
   stranded-forward-declaration grep with real-definition parameter
   cross-check) → classified every finding against the documented
   taxonomy.
4. For the stranded-forward-declaration check specifically, I did not
   stop at "does a `static ... name();` line exist" — for every hit I
   located the real K&R definition and read its actual parameter types
   by hand, since only a narrow by-value type (`char`/`short`/
   `unsigned char`) turns a stale empty-parens declaration into a real
   conflicting-types error once `ansify` converts the definition (item
   8 in the standing checklist, from T1). This is where most of this
   report's real findings came from.
5. One finding (atk/image's `sliderv`/`sliderV` DB collision, §3.1)
   needed a deeper investigation than a grep — I traced it through the
   `ansify` tool source and confirmed it with a read-only experiment:
   copied `src/atk/image/sliderv.c` to a scratch temp directory and
   called `ansify`'s `convert_file()` directly against it (pointing at
   the real, unmodified `build/desc/` DB) to see exactly what text it
   would produce. The real source tree was never touched by this test.
6. Ran a tree-wide class-name case-collision scan (lowercase every
   `class NAME` declaration across all `.ch` files in `src/`, group by
   lowercased name) to bound the scope of the §3.1 finding — see that
   section for the result (exactly one colliding pair exists anywhere
   in the tree, and it's in this batch).

## 3. Findings, by significance

### 3.1 UNCLASSIFIED, needs orchestrator ruling: `ansify` signature-DB filename collision on case-insensitive filesystems (`sliderv`/`sliderV`)

**This is the one finding in this batch that is a genuinely new shape,
not a documented pattern, and not safe to route around by an ordinary
per-instance fix.** It's a tool-infrastructure bug, not a source bug.

**Mechanism.** `ansify --build-db` (`revival/tools/ansify:100`) writes
one `.desc` file per class, named after the class's own name as
reported by `class -D`'s output (`shutil.copyfile(desc,
os.path.join(dbdir, classname + '.desc'))`), with `chfiles` processed
in sorted-path order (`for ch in sorted(chfiles)`, line 84) and *no*
collision check. `load_class_desc()` (line 136) looks up by the exact
same `classname + '.desc'` path when a later `ansify --dir` run needs
a signature. Both the write and the read are case-sensitive string
operations — but this filesystem (macOS default APFS/HFS+) is
case-*insensitive*, so two classes whose names differ only in case
resolve to the **same file on disk**. Confirmed directly:
`os.path.exists('build/desc/sliderV.desc')` returns `True` even though
only `sliderv.desc` (lowercase) is listed by `ls`.

**The two colliding classes.** `src/atk/image/sliderv.ch` declares
`class sliderv : view` (lowercase, unrelated small image-viewer slider
widget). `src/atk/value/sliderv.ch` (already `-pe`'d and committed in
M3 batch B2) declares `class sliderV[sliderv] : valueview[valuev]`
(capital V, the general-purpose slider value-view widget). Different
classes, different files, different directories, same file-basename
*and* case-colliding class name. `sorted(chfiles)` processes
`atk/image/sliderv.ch` before `atk/value/sliderv.ch` (`i` < `v`), so
atk/value's data — written later — currently occupies
`build/desc/sliderv.desc`, silently overwriting atk/image's.
Confirmed directly: the live `sliderv.desc`'s header line reads
`Class: sliderV` and `Subclass of: valueview, view, observable,
traced` — that's atk/value's inheritance chain, not atk/image's
(`class sliderv : view`, no `valueview` in its chain at all).

**Concrete consequence for this batch.** `ansify --dry-run --dir
src/atk/image` reports:

```
DRIFT   sliderv__InitializeObject: .c has 2 params, .ch has 0+1
```

This *looks* exactly like the already-documented §17 "empty-parens"
false positive (same message shape). It is not that — it's the DB
collision. I confirmed by direct experiment (§2.5) that
`sliderv__FinalizeObject` (which the dry run reports as successfully
converting, no DRIFT/skip) would actually be converted to:

```c
void sliderv__FinalizeObject(struct classheader *classID, struct sliderV *self)
```

using `struct sliderV *` — atk/value's capitalized struct tag, which
does not exist in atk/image/sliderv.c's scope (only lowercase `struct
sliderv` is defined there). This is not a cosmetic issue; it would be
a real, hard compile error at Gate 1 (`sliderv__SetCurval`/
`GetCurval`/`SetCallback`/`DoCallback` correctly show "no signature in
DB" and are left alone safely, because those method names don't exist
in atk/value's sliderV at all — but `FinalizeObject` and
`InitializeObject` *do* exist in both classes, under names that
collide, so the lookup silently returns the wrong class's data instead
of failing loudly). The 7 methods the dry run shows converting cleanly
for this file are all inherited overrides from the common ancestor
`view` (`GetInterface`, `FullUpdate`, `Update`, `WantUpdate`,
`LinkTree`, `UnlinkNotification`, `Hit`) — safe by coincidence, because
both colliding classes inherit identical real signatures for those
names from the same ancestor.

**Scope, tree-wide.** I ran a full lowercase-collision scan of every
`class NAME` declaration across all of `src/`. `sliderv`/`sliderV` is
the **only** colliding pair anywhere in the tree. So this is bounded —
it affects exactly one file in this batch (`atk/image/sliderv.c`) —
but it's a real gap in the `.eh`-locality gate-scope argument's older
sibling, the *signature-DB* locality assumption, which no prior batch
finding has documented. Given `ansify --build-db` was last run before
B2 committed `atk/value` (their file timestamps are consistent with a
single same-day rebuild), it's possible this collision has been latent
in the DB since B2, silently ready to misfire the moment any directory
touching the losing side (`atk/image`) ran a real `ansify --dir` — this
batch is the first to reach it.

**Why I'm not fixing this myself.** Per the Gate 0 instructions and
the Delegation ruling (`porting-assessment.md` §14, `rollout-
procedure.md`'s Delegation section): tool construction stays top-level,
and this is squarely a tool bug, not a per-instance source fix. Not
patching around it in `atk/image/sliderv.c` (e.g., by hand-fixing
`FinalizeObject` after the fact) is also correct — that would leave the
DB collision itself live for any future rebuild.

**Recommendation for the ruling:** the cleanest fix is in
`ansify --build-db` itself — detect a destination-path collision
before `shutil.copyfile()` (case-insensitive-aware, e.g. compare
`os.path.realpath`/`os.path.normcase` results, or just track written
lowercase keys and refuse silently on a second write) and report both
colliding `.ch` sources by name rather than silently letting the
alphabetically-later one win. Given the scope is exactly one pair
tree-wide, a narrower workaround (rebuild the DB after `atk/image` is
processed, or manually verify `sliderv.desc`'s content matches
`atk/image` before this directory's real run) would also work for this
batch specifically, but wouldn't close the general gap for any future
same-name-different-case pair.

### 3.2 Already-documented, safe, no action needed at Gate 0

- **`atk/org/orga.c`: `orgapp__FinalizeObject` DRIFT** (`.c has 2
  params, .ch has 0+1`) — this is the exact §17 "empty-parens" shape
  the runbook already named by file: `orga.ch:72` declares
  `FinalizeObject();` with literal empty parens. Confirmed the real
  `.c` definition takes the true 2-param convention (`classID, self`).
  Matches `m3-rollout-runbook.md`'s "classpp `FinalizeObject` fix"
  entry verbatim ("One directory not yet `-pe`'d already known to have
  the empty-parens shape live: `atk/org/orga.ch:72` — no action needed
  now, will just work once that directory's wave arrives"). No ruling
  needed.
- **`atk/bush/bush.c`: `bush__InitializeObject` — "no signature in
  DB"** — a related but distinct manifestation of the same §17 root
  cause: `bush.ch` doesn't mention `InitializeObject` at all (no
  restatement in any form), so the signature DB has no entry to look
  up (a total miss, not a count mismatch). Confirmed the real `.c`
  definition takes the true 2-param convention. Since a K&R definition
  with only pointer parameters is compatible with any well-typed
  prototype under C89 (no narrow-type promotion issue), leaving this
  skipped is safe — it will compile clean against `-pe`'s hardcoded
  2-arg `.eh` prototype even while still K&R. Same self-healing
  category as O4's function-pointer-returning-pointer gap.
- **`atk/hyplink/pshbttn.c`: `WriteDataPart`/`ReadDataPart` — "no
  signature in DB"** — these use the `Class__Method` naming convention
  but are not declared anywhere in `pushbutton`'s `.ch` or its parent
  chain (`dataobject`); they're plain internal helpers styled with
  double-underscore names, not real polymorphic methods. `ansify`
  correctly can't find them in the DB and safely skips. Ordinary,
  already-standard "no signature in DB" behavior — not a new shape.
- **Multi-dim-array parser gap, `atk/image/rle.c`** — 3 skipped
  helpers (`dithermap`, `make_square`, `make_magic`) all involve a
  `TYPE name[N][N]` (or `TYPE name[][N]`) parameter — matches B1's
  already-documented "gifin_load_cmap multi-dim-array gap" exactly
  (`parse_local_decls`'s declarator regex only handles one bracket
  group). Safe skip, no action needed.

### 3.3 New (but self-healing) parser-bailout shape — noting for the record, not blocking

- **`atk/image/tif.c`: `pickTileContigCase`/`pickTileSeparateCase`
  skipped as "unparseable K&R declarations."** Traced this to the
  tool's `parse_local_decls` regex requiring literal whitespace
  between a declaration's type token and the rest
  (`r'...\s+(?P<rest>.+)$'`) — it fails when the pointer star is glued
  to the type name with the space *after* instead (`RGBvalue* Map;`,
  vs. this codebase's more common `RGBvalue *Map;`). Not one of the
  previously-documented shapes (O2's `(void)`/`DECLARE<N>` misparses,
  O4's function-pointer-return gap, B1's multi-dim-array gap, B3's
  double-pointer-drop/array-bracket-transposition). Per the prompt's
  instruction to flag any genuinely new skip shape, I'm noting it here
  rather than silently folding it into "self-healing, ignore" — but
  functionally it behaves exactly like every other documented parser
  bailout: the file is correctly left K&R, no corruption risk, no
  compile-gate exposure (the skip message appears in both dry-run and
  real-run identically). My recommendation is the same disposition O2's
  `(void)`-misparse and `DECLARE<N>` findings got: log it, no fix
  needed to proceed with this batch, revisit if it costs a future
  session real triage time.

### 3.4 Ordinary fallout to fix during Gate 1 (matches standing checklist item 8 exactly, no ruling needed)

Stranded old-style forward declarations whose real definitions use a
narrow by-value parameter (`char`/`short`) — will become a genuine ISO
C conflicting-types error once `ansify` converts the definition, per
the T1-documented pattern. Found by reading every stranded
forward-declaration hit's real definition by hand (not just checking
for the declaration's existence):

| File | Function(s) | Narrow param |
|---|---|---|
| `atk/image/img.c` | `IMG_WriteByte` | `unsigned char c` |
| `atk/srctext/srctextv.c` | `paren`, `selfinsert`, `selfinsertreindent`, `endComment`, `startComment`, `startLineComment`, `styleLabel`, `styleString` | `char key` (8 functions) |
| `atk/srctext/cpptextv.c` | `slash` | `char key` |
| `atk/srctext/ctextv.c` | `startPreproc` | `char key` |
| `atk/srctext/modtextv.c` | `startPreproc` | `char key` |
| `atk/srctext/m3textv.c` | `asterisk` | `char key` |
| `atk/srctext/mtextv.c` | `asterisk` | `char key` |
| `atk/fad/fadv.c` | `MySetStandardCursor` | `short i` |

13 instances total, 3 directories (`atk/image`, `atk/srctext`,
`atk/fad`). All other stranded forward declarations found in this
batch (atk/image: ~45 more across `fbm.c`/`imagev.c`/`ps.c`/`pcx.c`/
`sliderv.c`/`tif.c`; atk/hyplink: 3; atk/bush: 10) were individually
checked and use only pointer/`int`/`long`/`enum`/`boolean` (typedef'd
`int`) parameters — safe, no forward-declaration conflict. Full detail
in §4's per-directory table.

Every file's macro-predefined-typo grep and empty-parens-lifecycle
grep (checklist items 2 and 3) came back clean across all 10
directories — no findings to report there.

One coincidental, harmless observation not requiring action: `atk/
image/tif.c` defines a non-static `setorientation()`; a completely
unrelated, already-ANSI `setorientation()` also exists in `overhead/
image/tiff/tif_getimage.c` (O2 batch, separate library). Neither is
declared in any installed header, so there's no compile-time
collision risk from this batch's conversion — noted only because it's
an unusual coincidence, not flagged as a finding.

## 4. Per-directory classification table

| # | Directory | `.ch` files | Dry-run result | Findings | Classification |
|---|---|---|---|---|---|
| 1 | `atk/image` | 18 | 22 files; 0 compile failures; **1 DRIFT** (`sliderv__InitializeObject`) | DB collision (§3.1, **UNCLASSIFIED**); 3 multi-dim-array skips in `rle.c` (§3.2, safe); 2 new-shape parser bailouts in `tif.c` (§3.3, safe); 1 narrow-param stranded fwd-decl (`IMG_WriteByte`, §3.4) | Mixed — 1 UNCLASSIFIED item blocks trusting this directory's dry-run at face value |
| 2 | `atk/srctext` | 19 | 20 files; 0 compile failures; 0 DRIFT | 12 narrow-param stranded fwd-decls across 6 files (§3.4) | Clean except ordinary Gate-1 fallout |
| 3 | `atk/raster/lib` | 7 | 7 files; 0 compile failures; 0 DRIFT | 3 stranded fwd-decls, all safe (int/pointer params) | Clean |
| 4 | `atk/layout` | 6 | 6 files; 0 compile failures; 0 DRIFT | none | Clean |
| 5 | `atk/hyplink` | 4 | 4 files; 0 compile failures; 0 DRIFT | 2 "no signature in DB" skips (§3.2, safe — not real methods); 3 stranded fwd-decls, all safe | Clean |
| 6 | `atk/org` | 3 | 3 files; 0 compile failures; **1 DRIFT** (`orgapp__FinalizeObject`) | Matches documented §17 empty-parens shape exactly, already anticipated by runbook (§3.2) | Clean — DRIFT is a known, already-classified false positive |
| 7 | `atk/bush` | 3 | 3 files; 0 compile failures; 0 DRIFT | 1 "no signature in DB" skip (§3.2, safe); 10 stranded fwd-decls, all safe | Clean |
| 8 | `atk/raster/scan` | 0 | 2 files; 0 compile failures; 0 DRIFT | 2 stranded fwd-decls, both safe | Clean (no `.ch` — DRIFT structurally impossible) |
| 9 | `atk/fad` | 2 | 2 files; 0 compile failures; 0 DRIFT | 1 narrow-param stranded fwd-decl (`MySetStandardCursor`, §3.4); ~25 other stranded fwd-decls, all safe | Clean except ordinary Gate-1 fallout |
| 10 | `atk/raster/convert` | 0 | 1 file; 0 compile failures; 0 DRIFT | none | Clean (no `.ch`) |

**Liveness**: all 10 directories confirmed via `building
(dependInstall) (.../src/atk/<dir>)` in `dependInstall.log`.

**Malloc-family grep** (informational, matches the already-documented
clang-builtin blind spot, not blocking): several files across `atk/
image`, `atk/layout`, `atk/fad`, `atk/raster/convert` call
`malloc`/`free`/`realloc`/`calloc` without a local `<stdlib.h>`
include (most rely on `<andrewos.h>`, which itself does not pull in
`<stdlib.h>` or declare these; `atk/image/g3.c` includes neither).
Compiles clean today because these are clang builtins; not M3 fallout,
not actionable by this rollout point.

**Concurrent-commit check**: `fossil status`/`fossil timeline` show no
commits landed by any other session during this Gate 0 pass — clean.

## 5. Files touched

None. No `.ch`/`.c`/Imakefile edits. The one experiment (§2.5) used a
scratch copy in a temp directory outside the repository; the real
`src/atk/image/sliderv.c` was never modified. `fossil status` is
clean.

## 6. Open questions / what surprised me

1. **The `sliderv`/`sliderV` signature-DB collision (§3.1) is the one
   item I'm explicitly asking for a ruling on.** Everything else in
   this report I was able to classify against the existing taxonomy
   with confidence. This one is a new tool-infrastructure bug class
   (case-insensitive-filesystem filename collision in `build/desc/`),
   bounded to exactly one class-name pair tree-wide, but with a real,
   confirmed (not hypothetical) silent-corruption consequence for
   `atk/image/sliderv.c`'s `FinalizeObject` if this directory's real
   `ansify --dir` run proceeded today without a fix or workaround.
2. The `tif.c` glued-star parser bailout (§3.3) is new but low-stakes
   (self-healing, same as every other documented parser gap) — flagged
   per the prompt's instruction to name any genuinely new shape, not
   because I think it needs a ruling before Gate 1 can proceed.
3. Everything else matched the existing taxonomy cleanly — this batch
   had noticeably less real fallout than B1/B2/B3 (no `.ch` type
   bugs, no classpp-interaction surprises beyond the two already-named
   §17 instances), closer to T1's "unusually clean" character. The
   narrow-by-value stranded-forward-declaration pattern (item 8) was
   the single largest source of real, expected Gate-1 work, concentrated
   almost entirely in `atk/srctext/srctextv.c`'s keystroke-handler
   family (8 of the 13 instances) — makes sense in hindsight, since
   self-insert-style command handlers taking the literal typed
   character are exactly where a `char`-by-value K&R parameter is a
   natural, idiomatic choice.

## Ruling on §3.1 (orchestrator, 2026-07-30)

Confirmed independently (read the live `build/desc/sliderv.desc`
directly before any fix landed: its `FinalizeObject` entry held
`args: struct sliderV *` against the real `atk/image/sliderv.c`
definition's `struct sliderv *self` — a genuine, not hypothetical,
mismatch). Fixed at the tool level in `ansify --build-db`'s
`build_db()`: track a lowercased-classname collision map, refuse to
silently overwrite a second class's `.desc` file under a colliding
name, report it through the existing `failed` list instead.
Committed standalone (`33b3a289` tool fix, `3e7e26ca` docs) ahead of
Gate 1, per the same "tool fixes commit independently of the batch"
convention every prior M3 tool bug used.

## 7. Gate 1 (orchestrator ran this directly, not the delegate)

**Status: Gate 1 complete, all 10 directories converted and gated
clean, twice each.** Execution note: the delegated session (this
same task, resumed after the §3.1 ruling) hit a hard block attempting
the real, file-mutating `ansify --dir` call — Claude Code's auto-mode
permission classifier denied it outright with no interactive prompt
available in a background subagent session. The orchestrator hit the
identical block running the same command directly, confirmed
transient (a session resource-limit reset, not a durable policy), and
completed Gate 1 personally once the block cleared, rather than
re-attempting delegation. All actual conversion/fix/gate work below
was done and independently verified in the same pass — there is no
separate "independent re-verification" step to report beyond this,
since the orchestrator is the one who did it.

### Per-directory Gate 1 results

1. **`atk/image`** (22 files, 18 converted, 3 dead/unbuilt files left
   K&R) — three real bugs found and fixed:
   - `cmapv.ch` restated `InitializeObject`/`FinalizeObject`'s `self`
     as `struct colormap *` (an unrelated real class,
     `atk/basics/common/cmap.ch`, already `-pe`'d in B1) instead of
     its own `struct colormapv *` — confirmed via the real `.c`
     implementation and the resulting "no member X in struct
     colormap" compile errors. Same species as T1's `ViewMove`
     finding. Fixed by correcting both restated types.
   - `img.c`'s `IMG_WriteByte` stranded forward declaration (standing
     checklist item 8) — fixed per the T1 pattern (full prototype).
   - `pbm.c`'s `pbm__Ident` called the file-local `isPBM` helper with
     a stray, always-ignored 6th argument (`(unsigned int)1`) at one
     of its two call sites; the other call site (`pbm__Load`) always
     used the correct 5-arg form. Same species as T1's
     `HandleSelection` finding — a K&R-tolerated wrong-arg-count
     caller bug, invisible until ANSI prototyping. Fixed by dropping
     the stray argument.
   - `g3.c`, `sliderv.c`, `xpixmap.c` all fail to compile even in
     their original, unconverted K&R form (confirmed directly,
     baseline `make <f>.o` before any ansify touch) and are not
     referenced anywhere in the Imakefile (no `DOBJS`/
     `ClassProgramTarget`/`DynamicObject` entry) — pre-existing dead
     code, not part of the live build, not M3 fallout. `ansify`
     correctly reverted all three to their original K&R text; left
     as-is.
   - Two new **tool** bugs found and fixed here, both committed
     standalone (see `m3-rollout-runbook.md`'s findings for full
     detail): `fix-missing-static-decl` didn't recognize an
     already-upgraded full prototype as "already declared" and
     inserted a conflicting duplicate (`d6b52e03`/`25f56793`); the
     `ansify` signature-DB case-collision bug from §3.1 above.
2. **`atk/srctext`** (20 files) — the largest fallout in the batch:
   - `asmtextv.ch` and `srctextv.ch` both restated `SetDataObject`'s
     parameter as a covariant, class-specific type (`struct asmtext
     *`, `struct srctext *`) rather than the base `view` class's
     `struct dataobject *`. Confirmed against the established, already
     -pe'd-and-committed safe pattern (`atk/adew/celv.c`,
     `atk/text/textv.c`: both declare the generic base type and cast
     internally) — classpp's own `-pe`-generated `.eh` always exports
     the *base* class's parameter type for an overridden method
     regardless of what the override's own `.ch` declares, so the
     covariant `.ch` declaration was always going to be misleading
     about the real exported interface; K&R never checked it. Fixed
     both `.ch` files to declare the generic type and added an
     internal cast (`struct asmtext *ct = (struct asmtext *)
     dataobj;`, and analogously for `srctext`) at each function's
     first line, matching the codebase's own established idiom for this exact
     situation elsewhere in the same files.
   - Two narrow-by-value-param stranded forward declarations not
     caught at Gate 0: `cpptext.c`'s `isOperatorOverload` and
     `srctext.c`'s `base64value` (Gate 0's grep found the file's
     *other* narrow-param instances correctly but missed these two —
     both are the standard, expected checklist-item-8 shape, fixed
     the same way).
   - `m3textv.c` and `mtextv.c` each had `asterisk` (and, in
     `m3textv.c`, `m3pragma` too) declared *twice* — once in an
     individual empty-parens line, once again in a shared
     comma-separated forward-declaration block — a pre-existing,
     harmless-under-K&R redundancy now needing both instances fixed
     consistently. `srctextv.c` had a similar, larger case: a 23-name
     shared comma-list at the top of the file was a complete,
     redundant duplicate of an already-present set of individual
     per-name declarations directly above it; deleted the redundant
     block entirely rather than fixing it twice.
   - Two new **tool** bugs, both committed standalone: `weave()`
     mis-rendered an array-of-pointer classproc parameter
     (`srctext__HashInsert`/`BuildTable`) because classpp's own `-D`
     dump renders the type as `T  [ ]` (space between brackets) —
     `weave()` only recognized bracket-adjacent `T[]`
     (`bda581c2`/`8a39c68e`). And the big one: `TYPEONLY` didn't
     tolerate a trailing comment on a two-line
     `RETTYPE  /* comment */` \ `name(...)` K&R declaration split — a
     common idiom in this codebase — so the old return-type line was
     never recognized as "already handled" and a *second*, fully-typed
     header got emitted right below it, guaranteeing a parse error
     that cascaded through the rest of the file. Found in `atk/layout`
     (below), fixed here first since `atk/srctext` was still
     in-progress when it was found; see that entry for detail
     (`4caa5b0d`/`cd05b37e`).
3. **`atk/raster/lib`** (7 files) — clean on the first real pass, no
   fixes needed.
4. **`atk/layout`** (6 files) — where the `TYPEONLY` two-line-comment
   bug above was actually found: all 6 files reverted on the first
   real pass, every one with cascading parse errors traced back to
   the same duplicated-return-type root cause (5 of the 6 files use
   the `RETTYPE  /* comment */` \ `name(...)` idiom). Fixed at the
   tool level (not per-file); all 6 converted and gated clean
   immediately after.
5. **`atk/hyplink`** (4 files) — clean on the first real pass.
6. **`atk/org`** (3 files) — `orga.c`'s `orgapp__FinalizeObject` DRIFT
   fired exactly as Gate 0 predicted (the already-documented §17
   empty-parens false positive from the `classpp FinalizeObject fix`
   entry). Confirmed `orga.c` is, like `atk/image`'s three dead files,
   not referenced anywhere in the Imakefile (no `DOBJS` entry) — dead
   code, left reverted/K&R, no consequence.
7. **`atk/bush`** (3 files) — one narrow-param stranded forward
   declaration not caught at Gate 0: `bushv.c`'s `Format_Tags` (takes
   `u_short tag`). Fixed the same way as every other checklist-item-8
   instance.
8. **`atk/raster/scan`** (2 files, no `.ch`) — clean on the first real
   pass.
9. **`atk/fad`** (2 files) — `fadv.c`'s `MySetStandardCursor` narrow-
   param stranded declaration fired exactly as Gate 0 predicted;
   fixed the same way.
10. **`atk/raster/convert`** (1 file, no `.ch`) — clean on the first
    real pass.

### Gate results

All 10 directories: `make -C <dir> clean && make -C <dir> depend &&
make -C <dir> -k install`, twice each, zero `error:` lines both times,
every directory. A final `ansify --dry-run --dir` sweep across all 10
directories after every fix landed confirms 0 compile failures
everywhere and exactly 1 DRIFT finding tree-wide (`atk/org`'s already
-explained `orgapp__FinalizeObject`, dead code).

Per the I1 prompt: **no tree-wide gate in this batch** — I1 and I2
are both Wave 4; the wave-end tree-wide gate is deferred to whenever
I2 closes the wave, matching how T1 (alone in Wave 3) was the only
batch that had to run its own.

### `fossil status`/`fossil extras`

76 files edited: 10 Imakefiles (the `-pe` flag), 3 `.ch` files
(`cmapv.ch`, `asmtextv.ch`, `srctextv.ch`), 63 `.c` files. No commit
made by this session. `fossil extras` shows only ordinary pre-existing
build byproducts, nothing new/untracked. Two tool files
(`revival/tools/ansify`, `revival/tools/fix-missing-static-decl`)
were edited and already committed standalone, independently of this
batch's own (still-pending) commit.

### Suggested runtime checks for wdc

`atk/image`, `atk/srctext`, `atk/raster/lib`, `atk/layout`,
`atk/hyplink`, `atk/org`, `atk/bush`, `atk/raster/scan`, `atk/fad`,
`atk/raster/convert` are all dynamically-loaded `.do` inset/app
libraries, not statically linked into `runapp` — confirmed via
`nm -g build/bin/runapp | grep -i " T .*\(image\|srctext\|layout\|bush\)"`
(no hits) vs. `ls build/dlib/atk/*.do` (each of these directories'
`.do` files present there). Live consumers, exercised via the normal
`ez`/`eza` GUI flow (never launched from this session):
- `atk/image`: insert an image inset (`<ESC><TAB>image` or similar,
  per `ez`'s insert-object menu) into a scratch document; try at
  least one raster format actually exercised by this directory's live
  `DOBJS` (`pbm`/`sunraster`/`tif`/`xwd`/`rle`/`pcx`/`fbm`/`mcidas`/
  `cmuwm`/`colorv`/`cmapv`/`img`/`mac`/`faces`/`imagev`) — `pbm.c` in
  particular got a real behavior fix (the stray-argument `isPBM`
  call), worth specifically loading/identifying a `.pbm` file if one
  is handy.
- `atk/srctext`: open a `.c`/`.cpp`/`.mod`/`.m3`/`.asm` file in `ez`'s
  source-text editing mode; type near a comment-start/preprocessor-
  start/paren/asterisk/slash character in each of `ctext`
  (`startPreproc`, `#`), `cpptext` (`slash`, `/`), `m3text`/`mtext`
  (`asterisk`, `*`), and plain `srctext` (`paren`, `selfinsert`,
  `startComment`, `endComment`, `startLineComment`, `styleLabel`,
  `styleString`) — all of these had their stranded-declaration fixed
  and are exactly the interactive keystroke path that exercises the
  fix. Also worth toggling Force-Upper-Case-Keywords/checking the
  too-long-line indicator once (exercises `srctextview__SetDataObject`
  directly). For `asmtextv.c`: open an assembly-mode buffer if a
  `.ezinit`-configured "bang comment" character is set up, or just
  confirm a plain asm buffer opens and accepts input at all
  (exercises `asmtextview__SetDataObject`).
- `atk/layout`/`atk/fad`: open any document containing a `filler`/
  `box`/`fad` (frame-animation-diagram) inset; for `fad` specifically,
  try the standard/wait/box/cross-hair cursor transitions (hover in
  and out of the inset, start/stop an animation if one is present) —
  exercises `MySetStandardCursor` directly.
- `atk/hyplink`: open a document with a hyperlink/pushbutton inset;
  click it.
- `atk/org`, `atk/bush`: `org` and `bush` are also standalone apps
  (`InstallLink(runapp, .../bin/org)` / `.../bin/bush`) — `runapp -d
  org` / `runapp -d bush` from native Terminal.app (not this session)
  to confirm each launches and displays; browse a directory in `bush`
  (its file-permission-tag display, `Format_Tags`, got touched).
- `atk/raster/lib`/`atk/raster/scan`/`atk/raster/convert`: CLI-only
  (`ezscan`, `convertraster` programs, plus the `heximage`/`oldrf`/
  `xbm`/`xwdio`/`rasterio` raster codec library) — a byte-diff
  before/after battery isn't set up for these; simplest check is
  `runapp -d ezscan`/the `convertraster` binary against a scratch
  raster file, from native Terminal.app.
