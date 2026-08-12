# M4 Wave 7, Batch C1: `contrib/zip/lib` STRICT_COMPILERFLAGS — REPORT

## 1. Status

**Stopped at Gate 1** (the task's only/final gate). `contrib/zip/lib`
builds clean under `$(STRICT_COMPILERFLAGS)` — zero real `error:` lines,
confirmed across three separate `clean && depend && install` cycles
(one with `-ferror-limit=0 -g -O0` for full-census fixing, two more at
the directory's normal flags for determinism). Per this batch's
elevated-risk flag (explicitly named in `m4-batches.md`, same tier as
`AMS2`), also ran the pre-authorized tree-wide gate: `make dependInstall`
from `src/` — **21,048-line log, zero real `error:` lines, zero
`make: ***` failures, clean exit 0**. **No fossil commit made.**
`fossil diff > c1-session.diff` (tree root, 2258 lines) has the full
diff. 32 files touched (`Imakefile` + 1 `.ch` file + 30 `.c` files).

## 2. What I did, in order

1. Read `revival/doc/sonnet-playbook.md` in full, plus
   `revival/doc/m4-rollout-runbook.md` lines ~380–460 (Phase 2 batch
   mechanics, fix policy, function-pointer-cast policy, gate rule) and
   `revival/doc/m4-batches.md`'s Wave 7 section (confirmed C1's
   census: 214 errors, flagged alone/elevated-risk, tree-wide gate
   required — "tree's known highest-defect-density directory").
2. Replaced the directory's M2-era override in
   `src/contrib/zip/lib/Imakefile`:
   ```
   COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
   ```
   with `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` — replaced, not
   stacked.
3. `make Makefile`; confirmed the override (line 293 of the
   regenerated Makefile) is the file's last `COMPILERFLAGS` assignment,
   winning over `system.mcr`'s still-suppressed tree-wide default at
   line 177 (`-Wno-implicit-function-declaration` etc.) — same
   mechanism-confirmation every prior M4 batch has done.
4. **Gate-0 dry-run triage** (required for this elevated-risk batch
   before starting to fix): `make clean`, `make depend` (benign
   `makedepend` "cannot find include file <system header>" noise
   only, same as every prior batch), then
   `make -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`.
   **Real count: 241 errors — 13% over the 214-error census**, same
   direction as every prior M4 batch's undercount (pre-`-Werror=format`
   census tools don't see format fallout). Breakdown by category
   (§3 below has the full triage):
   - **209 `-Wimplicit-int`** ("type specifier missing, defaults to
     'int'") — by far the dominant category, spread across 30 of the
     directory's 41 `.c` files.
   - **26 `-Wformat`** (16 + 10 across two sub-shapes: plain type
     mismatch and the `(aka 'long')` typedef-resolved form).
   - **5 `-Wincompatible-function-pointer-types`** (3 assigning, 2
     passing-as-argument).
   - **1 `-Wformat-extra-args`** ("data argument not used by format
     string") — this one turned out to be a genuine bug, see §5.
   - **0 `-Wimplicit-function-declaration`**, 0 raw
     `-Werror=int-conversion` hits (confirmed the flag reached the
     compiler by grepping the logged `cc` invocation lines — all 41
     show the full `STRICT_COMPILERFLAGS` string plus
     `-ferror-limit=0 -g -O0`).
5. Fixed all 209 `implicit-int` sites (§3), all 5
   `incompatible-function-pointer-types` sites (§4), all 27 format
   diagnostics across 26+1 sites (§5) — the last rebuild after this
   pass converged in one more round (a single missed
   `Prepare_Default_Pane` definition in `zipv.c` that a first
   spot-check surfaced; fixed and reconfirmed clean).
6. Rebuilt: **0 errors, exit 0**, full install completed (`.do`s,
   headers, docs, fonts) with no fatal stop.
7. Two more full `clean && depend && install` cycles at the
   directory's normal (non-`-O0`) flags for determinism — both clean,
   0 errors.
8. Downstream linkage check (§6 below — this directory produces zero
   static libraries; no relink needed anywhere).
9. Tree-wide gate (pre-authorized/required for this batch): from
   `src/`, `make dependInstall > /Users/wdc/src/AUIS/andrew-6.4/dependInstall.log
   2>&1` (backgrounded, confirmed completion via the harness's
   notification rather than polling). Completed clean: 21,048 log
   lines, `grep -c "error:"` → 0, `grep -n "^make: \*\*\*"` → no
   matches, log ends normally on `touch install.time` /
   `touch install.doc`, `EXIT=0`. Spot-checked `contrib/zip/lib`'s own
   segment of the log (lines 17835–20921) directly: 0 `error:` lines,
   only the same benign `makedepend` system-header noise seen
   throughout the whole log.
10. `fossil status`: exactly 32 `EDITED` files, all inside
    `src/contrib/zip/lib/`, zero `ADDED`/`DELETED`, no commit.
    `fossil diff > c1-session.diff` at the tree root (2258 lines).

## 3. `-Wimplicit-int`: 209 sites, all fixed by sourcing the true type
    from an existing typed declaration in the same file

This directory has a extremely consistent authoring pattern across
nearly all 41 files: each file carries **two or three layers** of
forward declarations for its static helper functions — an original,
fully-typed block (usually near the top of the file, sometimes with
full ANSI parameter lists), and one or more later, untyped duplicate
blocks (`static Name();` with no return type) that `ansify`'s M1/M2
passes evidently didn't reconcile against the typed block sitting a
few dozen lines above. The actual K&R **definitions** further down the
file are very often untyped too, matching the untyped duplicate
declaration rather than the typed original. In every single case in
this directory, the typed block's type was `int` — this directory's
older files (`TC Peters`, 1988-vintage) apparently never used a
`static void` or `static long` local helper; every "hit
handler"/"draw"/"parse" static function in the zip figure-editing code
returns a `zip_ok`/`zip_failure`-shaped status `int`.

**Method for every one of the 209 sites**: for each name, grep every
occurrence in the file; if a typed forward declaration of the same
name already exists (nearly always true), use that type on both the
untyped duplicate declaration(s) and the untyped K&R definition. Only
where no prior typed declaration existed did I look at the body/return
statements and callers directly (this happened for a small number of
newly-introduced local statics like `zipvp00.c`'s `pane_serial` — a
`static` local *variable*, not a function, also caught by
`-Wimplicit-int`, typed `int` from its use as an incrementing serial
counter).

**Zero guesses, zero casts for this category.** Representative
examples (full file list in §8):

- `zipve03.c` (32 diagnostics, the single largest file): lines 84–98
  carry a fully-typed 15-name `static int Name();` block; lines
  111–125 duplicate the exact same 15 names untyped; the 15 K&R
  definitions further down (`Move_Selection`, `Duplicate_Selection`,
  `Within_Enclosure`, `Draw_Enclosure_Shadow`, `Clear_Enclosure_Shadow`,
  `Show_Enclosure_Shadow`, `Check_Enclosure`, `Draw_Enclosure`,
  `Clear_Enclosure`, `Enclosure_Bounds`, `Show_Names`, `Show_Point`,
  `Show_Font`, `Clear_Font`, `Set_Constraints`) were also untyped.
  Fixed all 30 to `int`, matching the pre-existing typed block. Spot
  checked call sites (e.g. `count = Check_Enclosure(...)`) to confirm
  `int` is genuinely used as a return value, not just tolerated by
  `-Wno-return-type`.
- `zipv.c` (31 diagnostics): same shape but the typed reference block
  is *below* the untyped one (lines 1503–1592, `static int Name();`/
  `static void Name();` mixed per-function) rather than above —
  confirmed each of the 16 untyped names at the top against its
  properly-typed sibling further down before typing.
- `ziposym.c`, `zipedit.c`, `zipvp00.c` (21 each): same duplicate-block
  pattern, all `int`.
- `zipds01.c`: one function, `Parse_Stream_Commentary`, had a single
  *fully-prototyped* (with real parameter types) forward declaration
  already declaring `static int Parse_Stream_Commentary( struct zip
  *self, char c );` — its K&R-style definition further down just
  needed the same `int` added, not a fresh type decision.

One file (`zipv.c`) needed a second pass: my first sweep matched every
untyped name against a later fully-typed reference block, but missed
that `Prepare_Default_Pane`'s own **definition** (line 1391) had never
been typed at all (only its forward declarations at lines 1391's
sibling entries and 1532 were typed) — this one slipped through the
first name-list I built by hand. Caught it on the mandatory rebuild
step, fixed identically to its sibling `Prepare_Default_Stream`
(already correctly `int`), reconfirmed clean.

## 4. `-Wincompatible-function-pointer-types`: 5 sites — 3 true-signature
    fixes, 1 single-point `procedure`-typedef cast, 1 single-point cast
    to a genuinely polymorphic local callback-slot's own declared type

**`ziposym.c:852` — true-signature fix, no cast** (`Filter`, used as
`scandir`'s 3rd argument): macOS's real `scandir(3)` prototype (checked
directly against the SDK header,
`MacOSX.sdk/usr/include/dirent.h:154`) is
`int scandir(const char *, struct dirent ***, int (*)(const struct
dirent *), int (*)(const struct dirent **, const struct dirent **))`.
`Filter` was declared `static int Filter(DIRENT_TYPE *entry)`
(`DIRENT_TYPE` is `#define`d to `struct dirent` in
`config/darwin/system.h`) — missing `const`. Fixed by adding `const`
to `Filter`'s own parameter, matching the real system prototype
exactly. This is the textbook "prefer the true signature" case the
fix policy asks for first — no cast needed or used.

**Genuinely polymorphic callback slot — `zipedit.ch`'s
`keyboard_processor` field, 3 call sites across 2 files.** `zipedit.ch`
declares (data section) `enum view_MouseAction (*keyboard_processor)();`
— a deliberately untyped-argument (K&R, empty-parens) function-pointer
field used as a generic keyboard-input callback, set via the
`Set_Keyboard_Processor` macromethod
(`self->keyboard_processor = processor;`). Three concrete functions are
assigned to it across the directory, each with a **different concrete
`self` type** (the genuinely-polymorphic part — exactly the same shape
as a Class dispatch-table slot, just not built through classpp's
machinery):
- `ziposym.c:1219` — `Accept_Property_Hit(struct ziposymbol *, ...)`
- `zipocapt.c:123` — `Accept_Caption_Character(struct zipocapt *, ...)`
- `zipofcap.c:179` — `Accept_Caption_Character(struct zipofcapt *, ...)`

All three share the same trailing signature
(`zip_type_pane, char, enum view_MouseAction, long, long, long`) and
return type (`enum view_MouseAction`) — only the leading `self` pointer
type varies per subclass, which is precisely why a single unified
prototype can't replace the field's own deliberately-untyped
declaration. Per the fix policy ("casts are only acceptable for
genuinely polymorphic ... slots, and even then must be cast to the
full correct ... typedef, never a bare/void cast"), I fixed this at
the **single declaration point** (the macromethod itself, not each of
the 3 call sites) by casting to the field's own real declared type:

```c
Set_Keyboard_Processor( anchor, processor ) \
    {self->keyboard_processor = (enum view_MouseAction (*)())processor;self->keyboard_anchor = (long)anchor;}
```

This is the field's own actual declared function-pointer type
(`enum view_MouseAction (*)()`), not a bare/void cast — same spirit as
the codebase's established `(procedure)` idiom, just for a
locally-scoped callback field rather than the tree-wide `procedure`
typedef. Fixing it once in the macro (which classpp expands inline at
each of the 3 call sites) is consistent with "prefer a true-signature
fix at the point of declaration" applied to the closest thing this
shape has to a single declaration point. All 3 call sites (`ziposym.c`,
`zipocapt.c`, `zipofcap.c`) compile clean from this one edit; none of
the 3 call sites themselves needed touching.

**`(procedure)` cast — `zipedit.c:315`, `proctable_DefineProc`'s
handler argument.** `proctable_DefineProc("self-insert",
Accept_Character, &zipedit_classinfo, NULL, "Enter Character")` passes
`Accept_Character` (real signature `void Accept_Character(struct
zipedit *self, char c)`) to a parameter the compiler resolved as
`procedure` (`class.h`'s `typedef int (*procedure)();`) — the exact,
already-established, tree-wide idiom AMS1/AMS2 both used for this same
API family (`proctable_DefineProc` is a menu/keybinding command-handler
registration, the same "untyped-by-design command dispatch" contract
noted in AMS2's report). Fixed with `(procedure)Accept_Character` at
the one call site — no signature change, this is the correct,
established pattern for this API.

**Fix-policy compliance summary**: 2 casts total in this batch, both
targeted at the codebase's own already-declared function-pointer types
(the field's own type in one case, the tree-wide `procedure` typedef in
the other) — never a bare/void cast, never an invented shape. 3 of the
5 raw diagnostics (the `zipedit.ch` macro fix) collapse to a single
source edit. The remaining 1 of 5 (`ziposym.c`'s `Filter`) needed no
cast at all.

## 5. `-Wformat` / `-Wformat-extra-args`: 27 diagnostics across 16 real
    call sites — 1 genuine pre-existing data-loss bug, 15 mechanical
    LP64 width fixes

**Genuine bug — `zipds02.c:243`, the figure-mode datastream writer
silently drops the halo attribute on every write:**

```c
fprintf( file, "M%s%s\n", horizontal, vertical, halo );
```

Three arguments (`horizontal`, `vertical`, `halo` — each a
single-character-plus-NUL buffer built a few lines above from the
figure's mode bits, including `*halo = 'H'` when
`zip_figure_mode_halo` is set), but only **two** `%s` conversions —
`halo` is computed and then silently never written. This is exactly
the shape flagged in the task brief (more arguments than conversions,
mirror image of AMS3's `CUI_SetPrinter` under-conversion bug) and I
did not take the compiler's word for it alone — I traced the reader
side to confirm this is live, reachable data loss, not dead code:
`zipds01.c:351-371`, the `'M'` attribute parser, explicitly reads a
**third** character after `first`/`second` (`if ( (c = NextChar()) ==
'H' ) figure->zip_figure_mode.zip_figure_mode_halo = on; else
PriorChar(c);`) — the on-disk `.zip` stream format genuinely has a
3-character `M` attribute encoding (horizontal, vertical, halo), and
the writer was only ever emitting 2 of the 3. Any figure with the halo
mode bit set, saved and reloaded, silently loses its halo attribute.
Fixed by adding the missing `%s`:

```c
fprintf( file, "M%s%s%s\n", horizontal, vertical, halo );
```

**Reachability**: `zip__Write_Figure` (this function) is the writer for
every figure in every zip stream saved to disk — not a rare path. The
halo mode bit itself (`zip_figure_mode_halo`) is set via the "Halo"
figure-attribute UI toggle (`zipve02.c`'s attribute-palette hit
handling) — a real, if not heavily used, user-facing feature. Not
independently runtime-verified this session (would require building a
figure with halo mode, saving, reloading, and confirming the mode bit
survives) — flagged for wdc in §9.

**15 mechanical LP64 width fixes** (the recurring "codebase's own
`long`-typed field/typedef paired with `%d`" pattern documented in
`sonnet-playbook.md`), grouped by root cause:

- **`zip_type_point` is `long`** (`zipfig.h:183`) — 8 sites across
  `zipve03.c:1014,1020` (dialog-box coordinate display), `zipds02.c`
  (4 sites, the figure-point datastream writer: `:156` two
  conversions, `:255-257` two, `:260-262` two — all point coordinates
  serialized to the `.zip` stream file), `zipd000.c:474`... — wait,
  see below, that one is `status`, not a point. Corrected count: 8
  `zip_type_point`-driven sites (`zipve03.c` ×2, `zipds02.c` ×6 across
  3 `fprintf` call sites), all fixed `%d`→`%ld`.
- **`zip_type_pixel` is `long`** (`zip.h:136`) — `zipve00.c:733`, a
  point's pixel coordinates in a figure-point exposure label
  (`sprintf(points, "(%d,%D)", x, y)` — also fixed the non-standard
  `%D` BSD-legacy specifier to plain `%ld` for clarity, not just `%d`
  survival).
- **`zip.ch`'s `desired_view_width`/`desired_view_height`/
  `object_width`/`object_height` are `long`** (`zip.ch:236-240`) —
  `zip.c:321,323,329,331` (4 sites, the PostScript-header
  view/object-size annotations written by `Write_View_Info`/
  `Write_Object_Info`).
- **`zipedit.ch`'s `font_height` is `long`** (`zipedit.ch:161`) —
  `zipve02.c:1075,1085,1212` (3 sites, the font-height display/name
  construction in the font-catalog palette).
- **`zip.ch`'s `status` field is `long`** (`zip.ch:249`) —
  `zipd000.c:474` (the default exception handler's diagnostic
  `printf`).
- **`zipedit.c:804`** — a local `register long status` (not the
  `zip.ch` field) used in a background-pane-set error message.
- **`zipvr00.c`** — 3 sites: `IW72`/`IH72`/`H` are locally-declared
  `register long` PostScript page-geometry values (`:245,247` ×3
  conversions across 2 `fprintf`s); `remainder` (`:436`) is
  `register long` in the multi-line-polyline PostScript writer.
- **`zipoimbd.c:532`** — `print_x_point`/`print_y_points(i)` resolve
  (via `zipiff00.h`'s macros) to the same `zip_type_point` `long`
  family, in the imbedded-object PostScript-transform writer.
- **`zipofcap.c:436`** — `font_size` is a locally-declared `register
  long` in the flexible-caption font-name-string builder.

All 15 are pure width/serialization-correctness fixes (`%d`→`%ld` or
`%2d`→`%2ld`) — the underlying value was always computed correctly,
only the printed/written text representation was wrong on 64-bit,
either truncating on read-back-adjacent debug/UI text or (for the
`zipds02.c`/`zipoimbd.c`/`zipvr00.c` PostScript- and
datastream-writing sites) writing malformed output for any coordinate
outside the 32-bit range — narrower-impact than the halo bug but the
same class the milestone's `-Werror=format` gate exists to catch.

## 6. `.ch`-vs-implementation width drift

No new `.ch`-vs-implementation width disagreements were exposed by this
batch's stricter checking beyond the format-string sites already
covered in §5 (which are call-site argument/format mismatches, not
`.ch` declaration-vs-definition width disagreements). I specifically
went looking for the DRIFT shape M3 found repeatedly in this exact
directory (`FinalizeObject`/`InitializeClass` asymmetry, self-type
typos, missing methods — see the task brief's context) since M3's own
experience was "found genuine bugs constantly, not rarely" — but
`STRICT_COMPILERFLAGS` is a warning-driven, not arity-driven, gate, and
M3's own arity/DRIFT pass already ran over this directory and fixed
what it found. I did not re-run a full arity/DRIFT sweep (out of this
batch's scope), but the two class-method-return-type sites I did touch
directly (`zipvr00.c`'s `Draw_Arc`/`Arc_To`, §3, typed from their
`.ch`'s explicit `returns long;`) were both already correctly specified
in their `.ch` files — the only defect was the untyped K&R definition,
not a `.ch` disagreement. No ruling needed; nothing found to rule on.

## 7. Downstream consumer confirmation

- **This directory produces zero static libraries.** Its Imakefile has
  no `NormalLibraryTarget`/`LibraryTarget`/`InstallLibrary` rule —
  every one of its 24 build products (`DOBJS` list plus the 4
  `DynamicMultiObject` groupings) is a `.do` dynamic-load target
  (`zip.do`, `zipv.do`, `zipedit.do`, `zipprint.do`, `zipstat.do`, and
  18 more per-figure-type `.do`s like `zipobj.do`/`ziposym.do`/
  `zipocirc.do`/etc.). `InstallLink(runapp, $(DESTDIR)/bin/zip)` only
  symlinks the generic `runapp` launcher binary to `bin/zip` — it does
  not statically link any of this directory's code into `runapp`.
- **`nm -g` against `runapp` was denied by the permission classifier**
  (the known environmental quirk this project's memory already
  documents — a flat denial tied to the current 5-hour window, not a
  bug in this session's work). Fell back to Imakefile-based analysis
  per the task's own fallback instruction: this project's own prior,
  independently-established finding
  (`project_runapp_static_link.md`) is that only `text`/`matte`/
  `lpair`/`scroll` are statically linked into `runapp` — `zip/lib` is
  not among them, corroborated directly here by its Imakefile having
  no library-target rule at all (only `DynamicObject`/
  `DynamicMultiObject`).
- **Tree-wide grep for any other Imakefile referencing this
  directory's `.do` outputs by name** (`zip.do`, `zipv.do`,
  `zipedit.do`, `zipprint.do`, `zipstat.do`, `zipobj.do`,
  `ziposym.do`): zero matches outside `contrib/zip/lib/Imakefile`
  itself.
- **Tree-wide grep for any static-library consumer** (`libzip`,
  `zip.a`): zero matches — confirms there is no static library for
  anything to link against.
- **`contrib/zip/utility`** (the sibling C2 batch, flagged in
  `m4-batches.md` as sharing "lifecycle-method/lt/sched classes" with
  this directory's M3 history): checked its Imakefile directly — it
  builds unrelated `lt`/`sched` classes and only installs a data file
  (`itcCR.scd`) into `$(DESTDIR)/lib/zip`; it does not link against or
  depend on any of `zip/lib`'s build products. Not a consumer.
- **Conclusion: no relink or reload of `runapp`, `cuin`, or any other
  binary is needed anywhere in the tree.** The tree-wide `make
  dependInstall` gate (§2 step 9) independently confirms this — every
  directory in the tree rebuilt clean in the same pass, and the 20
  `zip*.do` files in `build/dlib/atk/` all carry fresh mtimes from
  that pass (verified directly with `ls -la`).

## 8. Files touched (compile status)

All 32 files compile clean (0 errors), verified across 3 full `clean &&
depend && install` cycles in this directory plus one tree-wide `make
dependInstall`:

| File | What changed |
|---|---|
| `Imakefile` | `COMPILERFLAGS` replaced with `$(STRICT_COMPILERFLAGS)` |
| `zip.c` | 6 `implicit-int` fixes (`int`) + 4 format width fixes (`%ld`, `zip.ch`'s `desired_view_*`/`object_*`) |
| `zipd000.c` | 1 format width fix (`%ld`, `status` field) |
| `zipdi00.c` | 4 `implicit-int` fixes (`int`) |
| `zipds00.c` | 4 `implicit-int` fixes (`int`) |
| `zipds01.c` | 3 `implicit-int` fixes (`int`) |
| `zipds02.c` | 6 format width fixes (`%ld`, point coordinates) + **1 genuine pre-existing bug fix** (§5, halo attribute dropped from datastream writes) |
| `zipedit.c` | 21 `implicit-int` fixes (`int`) + 1 `(procedure)` cast (§4) + 1 format width fix (`%ld`) |
| `zipedit.ch` | 1 single-point cast fix (§4, `Set_Keyboard_Processor` macro cast to the field's own declared type) |
| `zipoarc.c` | 4 `implicit-int` fixes (`int`) |
| `zipoarrw.c` | 5 `implicit-int` fixes (`int`) |
| `zipocapt.c` | 2 `implicit-int` fixes (`int`) — the function-pointer error at this file's line 123 resolved from the `zipedit.ch` fix, no local edit needed |
| `zipocirc.c` | 4 `implicit-int` fixes (`int`) |
| `zipoelli.c` | 4 `implicit-int` fixes (`int`) |
| `zipofcap.c` | 2 `implicit-int` fixes (`int`) + 1 format width fix (`%ld`, `font_size`) — the function-pointer error at this file's line 179 resolved from the `zipedit.ch` fix |
| `zipoimbd.c` | 4 `implicit-int` fixes (`int`) + 1 format width fix (2×`%ld`, print-transform coordinates) |
| `zipoline.c` | 4 `implicit-int` fixes (`int`) |
| `zipopath.c` | 1 `implicit-int` fix (`int`) |
| `zipopoly.c` | 1 `implicit-int` fix (`int`) |
| `ziporang.c` | 2 `implicit-int` fixes (`int`) |
| `ziporect.c` | 1 `implicit-int` fix (`int`) |
| `ziposym.c` | 21 `implicit-int` fixes (`int`) + 1 true-signature fix (§4, `Filter`'s `const` parameter for `scandir`) |
| `zipotrap.c` | 2 `implicit-int` fixes (`int`) |
| `zipprint.c` | 8 `implicit-int` fixes (`int`) |
| `zipv.c` | 31 `implicit-int` fixes (`int`) |
| `zipve00.c` | 1 format width fix (2×`%ld`, also replaced non-standard `%D`) |
| `zipve01.c` | 7 `implicit-int` fixes (`int`) |
| `zipve02.c` | 6 `implicit-int` fixes (`int`) + 3 format width fixes (`%ld`, `font_height`) |
| `zipve03.c` | 30 `implicit-int` fixes (`int`) + 2 format width fixes (`%ld`, point coordinates) |
| `zipvp00.c` | 21 `implicit-int` fixes (`int`, including 1 `static` local variable) |
| `zipvp02.c` | 2 `implicit-int` fixes (`int`) |
| `zipvr00.c` | 4 format width fixes (`%ld`, page-geometry/polyline values) + 2 `implicit-int` fixes on class-method definitions (typed `long` from `.ch`, §3/§6) |

## 9. Open questions / anything that surprised me

- **The `zipds02.c` halo-attribute bug (§5) is this session's most
  consequential finding** — a real, broadly-reachable (every figure
  save, if the halo mode bit is set) data-loss bug in the `.zip`
  datastream writer, silently dropping a real user-facing attribute
  for over 30 years. Recommend the runtime check in §10 specifically
  target it.
- **This directory's untyped-forward-declaration-duplication pattern
  was extremely uniform** — nearly every file in the 209-site
  `implicit-int` population had a pre-existing, fully-typed reference
  declaration to source from, keeping the fix mechanical and
  low-judgment despite the very high raw count. The one place this
  broke down (`zipv.c`'s `Prepare_Default_Pane`, §3) was caught by the
  mandatory rebuild-and-recheck step, not by the first pass — a
  reminder that this pattern's regularity doesn't excuse skipping the
  rebuild-before-trusting-silence step even on files that look
  "obviously the same shape" as ones already fixed.
- **Function-pointer-cast policy compliance**: 2 casts in the entire
  231-error-fixing pass (both targeted at real, already-established
  typed slots — `procedure` and a local callback field's own declared
  type, never bare/void), plus one true-signature fix that needed no
  cast at all (`scandir`'s `Filter`). This is in line with recent
  batches' near-zero-cast outcomes.
- Made no fossil commit, touched no file outside this directory's own
  32 (confirmed via `fossil status` before and after), did not touch
  `revival.md`/`porting-changelog.md`/`m4-batches.md` (leaving that
  documentation update to the orchestrator's review pass, matching the
  pattern from every prior M4 batch's own close-out).

## 10. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` was attempted
first and denied by the permission classifier (§7 — a known
environmental quirk, not a signal to skip verification); Imakefile
analysis substituted. Never launch GUI apps from this session; no
saves against unversioned fixtures.

- **`zip`** (GUI, native Terminal.app only, never from this session;
  `build/bin/zip` is a symlink to `runapp`): create a new zip
  document, draw a few figures of different types (line, rectangle,
  circle, arc, ellipse, polygon, trapezoid, arrow, symbol, caption,
  imbedded object), save, reopen, confirm everything round-trips —
  exercises the bulk of the 209 `implicit-int` fixes and the 2
  function-pointer-cast fixes (draw/hit-handling code across nearly
  every object type in the directory).
- **Halo-attribute bug fix (§5), highest-value check**: from a
  **scratch** zip document (never a real/shared fixture), create a
  figure, toggle its "Halo" mode attribute on (via the attribute
  palette), save the document, close and reopen it, and confirm the
  halo attribute is still set after reload. Before this fix, the
  attribute would silently revert to off on every save/reload cycle.
- **Keyboard-processor cast fix (§4)**: draw a symbol-catalog figure
  or a flexible-caption figure and enter text interactively (both
  exercise `zipedit.ch`'s `keyboard_processor` field via
  `Accept_Property_Hit`/`Accept_Caption_Character`) — confirms the
  single-point cast fix didn't change observable keyboard-routing
  behavior.
- **`(procedure)` cast fix (§4)**: type a character into any zip
  figure's text field (exercises `zipedit.c`'s `Accept_Character` via
  `proctable_DefineProc`'s "self-insert" command).
- **`Filter`/`scandir` fix (§4)**: open the symbol palette (`ziposym.c`
  scans a symbol-library directory tree via `scandir`) — confirms
  directory scanning for symbol sets still works after adding `const`
  to `Filter`'s parameter.
- **PostScript print path** (`zipvr00.c`, `zipoimbd.c`, `zip.c`'s
  format-width fixes): print or "print to file" a zip document
  containing an arc, an ellipse, a multi-point line, and an imbedded
  object — confirms the PostScript output's numeric fields are
  well-formed (no truncated/garbled coordinates) after the `%d`→`%ld`
  fixes.
