# Porting Changelog

Compact record of changes made porting AUIS 6.3.1 to macOS/Darwin arm64.
See `porting-assessment.md` for bug-class analysis and strategy; `roadmap.md` for current status and open work.

---

## Key decisions

- **Compiler leniency, not mass modernization** — leave K&R source as-is; use `-std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types` to suppress expected K&R noise. One mass-modernization attempt (automated tool across 916 files) was tried and reverted; it introduced silent bugs faster than it fixed real ones.
- **dlopen() dynamic loader** — replaced platform-specific a.out loaders with POSIX `dlopen`/`dlsym` (~70 lines vs ~500). `.do` files are Mach-O dylibs built with `-dynamiclib -undefined dynamic_lookup`.
- **System bison** — the vendored Andrew bison fork hangs on arm64. `overhead/bison/Imakefile` still builds it but does not install it. System `/usr/bin/bison` used instead, with `mkparser` patched to match modern bison's output format.
- **Typed dispatch for 9+ arg methods** — the class preprocessor (`overhead/class/pp/class.c`) was patched to emit fully-typed function pointer casts for methods whose argument count exceeds the 8 arm64 integer registers, fixing the LP64 variant-2 stack-spill ABI bug.
- **Run AUIS binaries from native Terminal.app** — VS Code's integrated terminal intercepts SIGSEGV exception delivery and leaves crashing processes suspended/unkillable. Use Terminal.app or iTerm2 for all `ez`/`runapp` testing.

---

## LP64 bug classes fixed

The codebase was written for ILP32 (32-bit int, long, pointer). Six distinct bug classes have emerged on arm64 LP64 so far; the first five have been identified and swept, the sixth is confirmed but not yet tree-wide audited. See `porting-assessment.md` §12–§19 for full analysis (the table below predates that section numbering; §19 covers variant #6).

| Variant | Root cause | Scope |
|---|---|---|
| #1 Pointer truncation | Undeclared pointer-returning fn → implicit `int` return → 32-bit truncation | 23 sites fixed with local `extern` decls |
| #2 Stack spill | >8 args through `void(*)()` → arm64 drops 9th+ arg | classpp: typed casts for argcount ≥ 8 |
| #3 Zero-extension | `int -1` const through untyped dispatch, `long` receiver sees `0xFFFFFFFF` | `(-1L)` in observe.ch, value.ch, class.h |
| #4 scanf %d | `%d` writes 32 bits into `long*`; upper 32 bits garbage | Full tree audit; 11 real bugs fixed |
| #5 Dispatch int/long | Negative `int` arg through untyped dispatch to `long` param, sign bit lost | lpair, panel, dialog, dialogv, table, fad, srctext, eq, metax; full sweep committed |
| #6 `.ch`/wrapper vs. real-impl width drift | Class wrapper forwards a `.ch`-typed pointer to an independently-declared bare K&R function whose real out-param width disagrees; invisible, no compiler warning either direction | 5 confirmed instances so far (`MS_GetConfigurationParameters` 07-18, `MS_ParseDate` + `MS_GetDirInfo`/`MS_GetNewMessageCount`/`MS_GetSubscriptionEntry`/`MS_NameChangedMapFile` 07-24), all in `ams/libs/ms`; not yet tree-wide audited — see porting-assessment.md §19 |

---

## Chronological log

### 2026-06-24 — Darwin platform bootstrap

- Created `config/darwin/system.h` (BSD/POSIX, derived from i386_Linux)
- Created `config/darwin/system.mcr` (XQuartz at `/opt/X11`, Homebrew at `/opt/homebrew/bin`, `LEXLIB = -ll`)
- Created `config/site.h` (empty placeholder), `config/site.mcr` (`#undef AMS_ENV`, `#undef ANDREW_MALLOC_ENV`)
- `overhead/class/pp/class.c`: replaced manual `malloc`/`realloc` decls with `#include <stdlib.h>`
- Fixed `platform.tmpl`: imake defines `__DARWIN__` not `__MACH__` — platform detection was silently never matching
- Set `DEFAULT_ANDREWDIR_ENV` to `andrew-6.4/build/` for in-tree bootstrap
- Class preprocessor compiles, runs, and correctly processes `bp.ch` through the full class hierarchy

### 2026-06-25 — Base class chain, full tree, dynamic loader

- `overhead/class/lib/class.h`: replaced `#ifdef _IBMR2` block with clean `#include <stdlib.h>` + `#include <errno.h>`
- `atk/basics/common/dataobj.c`: added missing `#include <andrewos.h>`
- Base class hierarchy compiles: `traced.c` → `observe.c` → `dataobj.c` → `bp.c`
- Darwin `system.h` fixes: `NEWPGRP()` takes no args (POSIX), `FILE_HAS_IO` uses `_r` not `_cnt`
- `make dependInstall` runs end-to-end; 62 `.do` files built
- Created `overhead/class/machdep/darwin/`: `doload.c` (dlopen/dlsym), `doload.h`, `makedo.csh`, `classproc.c`
- `class/lib/Imakefile`: exclude `entry.o`/`globals.o` on Darwin (not needed with dlopen)
- Added `CLASS_CTRAMPOLINE_ENV` to `darwin/system.h` (C-based lazy class loading, no x86 asm trampolines)
- Class preprocessor: enabled `usePrototypes = TRUE`; added `GenerateForwardDecls()` to auto-emit `struct X;` forward declarations

**Mass modernization (attempted and reverted)**
- Ran `revival/tools/modernize` across all 916 `.c` files → silently damaged 16+ files; error count jumped to >2000
- Reverted all `.c` files to trunk
- Policy going forward: fix surgically; do not run mass modernizer

### 2026-06-29 — Surgical fixes, static methods, bison

- Added `-std=gnu89 -Wno-return-type` to `config/darwin/system.mcr`
- `overhead/util/lib/` fixes for zero-error overhead: `fdplumb6.c` (`dirfd()` replaces `d->dd_fd`), `times.c` (Darwin tz macros conflict), `usignal.c` (const `sys_siglist`), `profile.c` (missing `extern char *AndrewDir()`)
- Created `revival/tools/fix-static-methods`: strips `static` from class method definitions so `dlsym` can find them; 21 files
- `overhead/util/hdrs/util.h`: fixed implicit-int extern declarations
- `sys_errlist`/`sys_nerr`: removed stale K&R extern decls across 10 files; replaced with `strerror(errno)`
- `extern char *malloc()`/`realloc()`: removed stale K&R decls from 8+ files that conflicted with `<stdlib.h>`
- Vendored bison disabled: `overhead/bison/Imakefile` wraps `InstallProgram` in `#ifndef sys_darwin`
- `config/andrew.rls` `Parser()` macro: added `-o classname.tab.c` so modern bison produces the expected filename (`.gra` extension was causing silent mismatch)
- Note: `atk/ness/objects/ness.gra` uses Andrew bison's multi-character-string-token extension; not yet handled

### 2026-06-30 — Compile phase complete: 278 .do files, zero errors

- Created `revival/tools/fix-missing-static-decl`: adds `static` to K&R forward declarations missing it; handles block-scope exclusion, `#ifdef` depth, trailing comments
  - Three names skipped (collide with stdlib): `abs` (deleted; libc version identical), `fmax`/`fmin` → `funs_Max`/`funs_Min`, `getline` → `lset_GetLine`
- Found and fixed `fix-static-methods` bug: Pattern A regex silently dropped K&R parameter lists in 16 files; repaired via trunk diff comparison
- **Lesson**: `make -k` exit 0 does not mean zero errors. Compile-verify each individual file a tool touches before committing.
- `mkparser` patched for modern bison output (3 incompatibilities): `YYFLAG` → `YYPACT_NINF`, `reduceActions` end-marker, Allman-style brace on separate line
- `eqparse.gra`: hand-fixed `union ptr x(), value;` split into two statements; removed duplicate static decl
- Remaining errors cleared: `figio.c`, `fad.c` (missing K&R param type decls); `xcursor.c`, `unknown.c`, `srctext.c` (missing `#include <util.h>`); `mkcon.c`, `helpdb.c` (`close()` on FILE*/DIR* → `fclose()`/`closedir()`); `xgraphic.c` (NULL assigned to XID → 0); `org.c`, `suiteta.c`, `srctextv.c` (long/pointer attribute casts); `cel.c` (`getline` rename); `spread.c`, `table.c`, `tabio.c`, `box.c` (malloc extern decls); `chart.c` (`<values.h>` guarded out); `profile.c` (BSD a.out profiling guarded out); `atk/basics/lib/Imakefile` (added `LOCALINCLUDES`); `basics/x/Imakefile` (`-DXLIB_ILLEGAL_ACCESS`)
- `make dependInstall` (no `-k`): **278 `.do` files, 602 headers, exit 0**

### 2026-06-30 — Runtime: ez launches

- VS Code terminal masks SIGSEGV as unkillable hang — always run from native Terminal.app
- LP64 #1: `AndrewDir()` called without prototype → implicit `int` return → pointer truncated on arm64; fixed 23 sites with local `extern char *FunctionName()` declarations
  - Key files: `andrwdir.c`, `andydir.c`, `xbasedir.c`, `localdir.c`, `environ.c`, `genstatl.c`, `runapp.c`, `setprof.c`, `whichdo.c`, `fdplumb4.c`
- `ez` now launches past `class_Init` and draws a real window

### 2026-06-30 — Class preprocessor ABI fix (LP64 #2)

- Root cause confirmed: arm64 never emits `str` for the last stack-spilled argument when calling through `void(*)()` with >8 args; `sbuttonv__DrawBorder` (10 args) received stale stack content
- `usePrototypes` split into `usePrototypesImport` (dispatch macro casts, default TRUE) and `usePrototypesExport` (forward decls, default FALSE for K&R compatibility)
- Typed casts emitted only for methods with `argcount >= 8` (9+ total args); below that threshold, all args fit in registers, no stack store needed
- Additional fixes in same pass: `InitializeObject` prototype (2 params), `FinalizeObject` delegated to classproc loop, `classhdr` alias added to `class.h`, unnamed `.ch` parameters fixed (`event.ch`, `sbutton.ch`)
- Call sites: `(long)(void *)ptr` casts added in `metax.c`, `helpaux.c`, `txtvcsty.c`, `compchar.c` for `AskForStringCompleted` cookie params

### 2026-07-01 — LP64 #3: observer destroy zero-extension; UAF crash

- `observable_OBJECTDESTROYED = -1` passed as `int` through untyped dispatch; arm64 zero-extends to `0xFFFFFFFF`; `bufferlist__ObservedChanged` comparison never matched; freed buffer stayed in list; checkpoint timer UAF crash ~30s after file close
- Fixes: `observe.ch` → `(-1L)`, `value.ch` → `(-1L)`, `class.h class_VERSIONNOTKNOWN` → `(-1L)`
- `ez` now runs stably; checkpoint timer fires repeatedly without crashing

### 2026-07-01–07-02 — Xft anti-aliased rendering + symbol fonts

- `config/darwin/system.mcr`: added `HAVE_XFT`, `XFTLIB`, `XFTINCDIR`, `STD_DEFINES`, `XBINDIR = bin` (fixes bdftopcf/mkfontdir paths)
- `xfontd.c`: added `XftFont *xft` field to `struct fcache`; open Xft for iso8859/iso10646 fonts; exclude `adobe-fontspecific` (Symbol encoding, not Unicode)
- `xgraphic.c`: Xft rendering path in `DrawChars`; per-character clip to advance-width cell (prevents right-side bearing bleed on incremental erase); `graphic_INVERT` excluded from Xft gate (XOR erase must use X11 GC path)
- Xft menu rendering attempted and reverted; menus work acceptably on X core path
- Symbol fonts: `overhead/fonts/fonts/adobe/Imakefile` — `symba8..22` DeclareFont outside `ISO80_FONTS_ENV` guard; `xmkfontd/Imakefile` uses `INSTLIBFLAGS` (0664) not `INSTINCFLAGS` (0444)
- `xset fp+ build/X11fonts && xset fp rehash` required after each build (cannot automate — X server live state)

### 2026-07-01 — LP64 #4 (first instance): raster %d/%ld

- `raster__Read`/`rasterio__ReadImage`: `long width, height` read with `%d` → upper 32 bits garbage; caused range-check false positive, skipped `\enddata` scan, consumed remaining document as literal text, hung on "go to end"
- Fixed 7 `fscanf` calls across `raster.c` and `rasterio.c`: `%d`→`%ld`, `%x`→`%lx`

### 2026-07-02 — LP64 #4 full audit + LP64 #5 sweep

**scanf audit (141 hits; 11 real bugs):**
- `layout.c:235` — `long left,top,width,height` with `%d` → `%ld`
- `label.c:106` — `long style,size` with `%d` → `%ld`
- `raster/lib/xbm.c:175` — `long value` with `%d` → `%ld`
- `annot/icon.c:228`, `annot/ps.c:206` — `long haschild` with `%d` → `%ld`
- `nesst.c:131` — `long id` with `%d` → `%ld`
- `dialog.c:202,217` — `long textid` reading `\begindata` IDs with `%d` → `%ld`
- `be1be2.c:373,382` — `long TabCount,TabLoc` with `%d` → `%ld`
- `ness.c:598` — `long syntaxlevel` with `%d` → `%ld`
- `ex11–13,16/hello.c`, `ex17–19/hello.c` — `long hw->x,hw->y,dobjObjId` with `%d` → `%ld`

**LP64 #5 dispatch int/long fixes:**
- `lpair.c lpair__Init`: `long x` → `int x` (value never needs 64-bit range; sign extension then correct)
- `panel.c`: `(long)16384` and `(long)-16384` casts at `style_SetNewLeftMargin`/`SetNewIndentation` call sites
- `xim.c DoTransientGeometry`: `int *left,top,width,height` → `long *`; `DoCreateTransientWindow` locals same; `ReConfigurePopups`: intermediate `int` for XGetGeometry, copy to `long` before config call
- `dialogv.c dialogv__DesiredSize`: `int oldheight` → `long`
- `table/update.c updateValue`: `int x,y,rightshim` → `long`
- `fad.c fad__Read/Write`: `int p1x..p2y` → `long`; `%d`→`%ld` in fscanf/fprintf; `(long)LABELFLAG` casts
- `fadv.c`: `(long)` casts at 5 `fad_setpoint` call sites using LABELFLAG/fad_iconnum/char codes

**fad animation fixes:**
- XOR ghost: `graphic_INVERT` added to XFT bypass (`graphic_INVERT ≠ graphic_XOR`; Xft alpha-blends instead of XOR-canceling)
- `$T 0` timing: `queup()` applies 30ms floor for zero-delay frames (modern hardware runs <1ms/frame, draining animation invisibly)

### 2026-07-03 — Clean-clone bootstrap fix; LP64 audit round 2

- `overhead/class/machdep/darwin/doload.c`, `classproc.c`, and generated Makefiles were not in fossil; added. Fresh clones now build. Note: first-time clone needs `make World`, not `make dependInstall`.

**LP64 variant-5 audit round 2 — style attribute reads, eq marks, figure sentinel:**
- `style.c style__ReadAttr`: `int operand` → `long operand`; all 11 style dispatch calls in the `.ez` attribute reader (`SetNewLeftMargin`, `SetNewRightMargin`, `SetNewIndentation`, top/bottom margins, interline/interparagraph spacing, above/below, font size, font script, tab) now correctly sign-extend negative operands from `atoi`. Verified by `otool` disassembly: unfixed binary emits `mov x2, x0` (zero-extends), fixed emits `sxtw x2, w0` (sign-extends) after each `bl atoi`.
- `figure.ch figure_NULLREF`: `-1` → `-1L`; sentinel used in parent-pointer comparison through untyped dispatch (LP64 #3 pattern, same as `observable_OBJECTDESTROYED`)
- `eq.c eq__Insert`: `mark_UpdateMarks(..., -1)` → `(long)-1`; delete-size sentinel zero-extended to `4294967295` without the cast, corrupting all marks in the equation editor on each character delete
- `tscript.c`, `srctext.c`, `metax.c`: `(long)` casts on negative literals at `style_SetNewLeftMargin`/`style_SetNewIndentation` call sites (missed in round 1 sweep)
- `readscrb.c`: `(long)-2` at `style_SetFontScript` call (file not in active build; pre-emptive)

### 2026-07-04 — fnotev rendering fix

- `fnotev.c DesiredSize/DoUpdate`: box height from fontdesc newlineHeight; marker glyph centered (BETWEENTOPANDBOTTOM|BETWEENLEFTANDRIGHT) — old ATTOP|ATLEFT pushed baseline past box bottom on scalable fonts

### 2026-07-04 — helpa: Shrink Programs List fix

- `help.c ToggleProgramListSize` (shrink branch): fallback directory lookup used `SETUP_PANELSDIR` → `DEFAULT_PANELSDIR` (`/help`), but `help.programs` actually lives under the lib dir. Since `HelpPanelsDir` is never configured, the small-list file was never found, silently falling through to the extension-enumeration fallback over `/help` — producing a big list that looked just like "expand," so "Shrink Programs List" appeared to do nothing.
- Fixed to match the working init-time chain in `helpaux.c` (`InitializeObject`): `SETUP_PANELSDIR` → `SETUP_LIBDIR` → `DEFAULT_LIBDIR` (`/lib`).

### 2026-07-04 — helpa: list-panel scroll position (LP64 #3 pattern, call-site variant)

- `textv.c textview__InitializeObject`: `self->frameDot = text_CreateMark(dataObject, -1, 0)` — `text_CreateMark` dispatches through the untyped `(struct mark *(*)())` macro; the bare `int` literal `-1` isn't sign-extended to the `long pos` field on LP64, so `frameDot->pos` held garbage (`4294967295`, `8589934591`, etc. observed in lldb) instead of the sentinel `-1`.
- `DoUpdate`'s `mark_GetPos(self->frameDot) != -1` check (`textv.c:1231`) then spuriously fired on every first redraw, computing a scroll position from garbage input that landed near the end of the document — this is what made `helpa`'s Programs list open scrolled to the bottom instead of the top.
- Same root mechanism as LP64 #3 (`observable_OBJECTDESTROYED`), but the `-1` is a bare call-site literal rather than a named `#define`d sentinel, so it didn't show up in a grep for the earlier fix's constant names.
- Fix: `text_CreateMark(dataObject, (long)-1, 0)`.

### 2026-07-04 — LP64 #3 call-site audit: content enumerate, figure zoom, raster negative

Following the `frameDot` fix above, swept for sibling "bare `-1` literal through untyped class-dispatch macro" call sites (grep pattern and full writeup in `roadmap-old.md`; methodology also in `revival/doc/runtime-debugging-guide.md`). Narrowed ~925 raw hits to 22 candidates; confirmed 6 real bugs (receiver actually sign-checks the corrupted value) and fixed all with `(long)-1` casts:

- `content.c`/`contentv.c` (`content__Enumerate`/`content__Denumerate`): `opos`/`pos < 0` is the "enumerate everything" sentinel; corrupted value read as huge positive, silently skipping that path. 3 call sites (`content.c:649`, `contentv.c:134,186`).
- `figv.c` (`ChangeZoomProc`): `rock<0`/`rock>0` decides zoom out vs. zoom in; "Zoom Out" (menu item + `Esc-z` keybinding) passed `-1` through `menulist_AddToML`/`keymap_BindToKey`. Corrupted, Zoom Out would zoom in instead. 2 call sites (`figv.c:129-130`).
- `rasterv.c` (`ModifyCommand`): `rock == -1` selects "invert selection" ("Negative" menu item + `Esc-n` keybinding). Corrupted, Negative would silently do nothing (falls through all the `==` branches). 2 call sites (`rasterv.c:1634-1635`).

Several other candidates from the same grep sweep were confirmed harmless despite passing through the same untyped mechanism (e.g. `view_FullUpdate(...,-1,-1)` width/height args in `figv.c`/`rastvaux*.c` are ignored entirely by the receiving `FullUpdate` overrides, which recompute geometry from the view instead) — not fixed, no observable bug. A few lower-priority candidates (`rectangle_InsetRect` unprototyped-arg risk in `figv.c`'s clip-region code, `environ_GetProfileInt`/`cwp_Search` in the deprioritized messages/AMS subsystem, `tlex_RecentPosition` in the not-yet-working `ness` extension) were left untriaged — see `roadmap-old.md`'s "Variant 3 follow-up audit" section.

**Also found, not fixed:** a live Xlib `_XLockDisplay` self-deadlock (single-thread re-entrancy triggered by `MappingNotify`/`XRefreshKeyboardMapping`), discovered incidentally while testing the figure-inset fix above. This supersedes the old "checkpoint timer UAF" theory for the `^V` scroll-hang heisenbug — see `roadmap.md`'s Heisenbugs section.

### 2026-07-04 — figure inset "messy screen": not LP64, a version-skew + error-swallowing bug

`95Summer.ez`'s figure inset (line 674) rendered as raw literal text ("`arrowpos:0 arrow:5 linestyle:0 } $ 0 117 -55287130 451 55288120 $endatt`" visible on screen) instead of a figure, and nothing after it in the document rendered either. Not an LP64 bug — confirmed via `fossil finfo` that `figattr.c`/`figattr.ch` are unmodified since initial import.

- **Root cause 1 — `figattr.c figattr__Read`**: the file's figure objects carry attributes (`arrowsize`, `arrowpos`, `arrow`, `linestyle`) that this 6.3.1 tree's `figattr_NumAttributes`/`attribute_names[]` (only 8 entries: Shade, Color, LineWidth, RRectCorner, FontSize, FontStyle, FontFamily, TextPos) never implemented — a genuine 30-year-old gap (note the unfilled `/* ##new */` placeholder in the read/write switches). The file was authored by a later/extended figure editor. `figattr__Read` returned `dataobject_BADFORMAT` on the first unrecognized attribute name.
- **Root cause 2 — `smpltext.c simpletext__HandleBegindata`**: called `dataobject_Read(newobject, file, objectid)` without checking the return value. When the figure's Read failed partway through, the file position was left mid-object-body (right after the last successfully-read line); the code pressed on regardless, registering the half-read object and resuming plain-text parsing from that misaligned position — which is exactly what dumped the rest of the figure's raw datastream (and desynced everything after it) onto the screen as literal text. Confirmed the same bug exists verbatim in the untested C++ line (`andrew-8.0/atk/text/simpletext.C:938`, `(newobject)->Read(file,objectid);` — same missing check), so no reference fix to port from there.
- **Investigated `/Users/wdc/src/AUIS/patches/official/` before writing new code**: found CMU's own `patch.633` — "Patch to V6 figure to make it accept figures created with later versions, including C++ versions 7.2 and higher" — which is the officially-sanctioned fix for root cause 1. Applied verbatim: commented out the `BADFORMAT` return in the unknown-attribute-name check, added `case figattr_NumAttributes: /* unknown attribute -- ignore */`. No official patch addresses root cause 2 (the `simpletext` error-swallowing); none exists in the C++ line either, so this hardening is new.
- **`smpltext.c` fix**: on a non-`NOREADERROR` return from `dataobject_Read`, destroy the partially-read object, create a fresh `unknown` object (the same fallback class already used when `class_NewObject(objectname)` can't find the type at all), and re-invoke `dataobject_Read` on it. `unknown__Read`'s `RealRead` scans forward honoring nested `\begindata`/`\enddata` depth until it finds the *real* `\enddata{objectname,id}`, so the parser resynchronizes and the rest of the document renders — instead of every failed/version-skewed inset silently corrupting everything after it. Verified the added dispatch calls (`dataobject_Destroy`, `class_NewObject`, the retry `dataobject_Read`) don't reintroduce the LP64 int/-1-through-untyped-dispatch pattern: no new int literals cross a dispatch boundary, `objectid` is the same pre-existing `long` variable passed through unchanged, and `dataobject_Read`'s macro (`dataobj.ih:202-203`) already casts to `long (*)()` so capturing its return in `long readret` is correctly typed.
- Rebuilt and installed `figattr.do` and `smpltext.do`. Verified in `ez`: `95Summer.ez`'s figure now renders correctly instead of raw text.

### 2026-07-04 — Patches directory audit; malloc.ci arena-size fix

Audited `patches/official/` and `patches/contrib/` in full (every file every patch touches diffed against current source) after finding `patch.633` already fixed the `95Summer.ez` figure bug above — full findings in `roadmap-old.md`'s "Historical patches audit" section. Two outcomes:

- Applied `contrib/malloc.ci.auis6.3.diff`: `overhead/malloc/malloc.ci` `addarena` computed `x = (A.arenaend - A.arenastart)>>3` where both are `struct freehdr *` — pointer subtraction is in units of `sizeof(struct freehdr)`, not bytes, undercounting the arena-growth heuristic ~20-24x. Fixed with `(char *)` casts. No runtime effect on this build: `ANDREW_MALLOC_ENV` is `#undef`'d in `config/site.h`, no `malloc.o`/`libmalloc.a` exists anywhere in `build/` — we run on system malloc, this file is dead code here. Fixed for source correctness anyway (originally submitted by the current user in 1995). Confirmed `malloc.c`/`pmalloc.c` (which `#include` this file) don't currently compile for an unrelated pre-existing reason (`AbortFullMessage` static/non-static conflict) — out of scope, not touched.
- Everything else already fixed upstream in our 6.3.1 baseline (`figospli.c` spline guard, `unknown.c`'s 3 hunks, `hash.c` use-after-free, `tabs.c` bounds check — all from `patch.631`) or genuinely inapplicable (AMS/`eatmail` patches, `symlink.patch` targeting a `LIBDL_ENV` code path Darwin never defines, and the SGI/NetBSD/Solaris/HP-UX platform ports).

### 2026-07-04 — Figure inset second bug: LP64 `$origin` scanf corruption ("lala land" rendering)

After the `figattr`/`smpltext` parser fixes above, `95Summer.ez`'s figure stopped dumping raw text but rendered as an empty bordered box — correctly sized, nothing drawn inside. A long, methodical elimination process (documented here so it isn't repeated):

- Added temporary `fprintf` instrumentation across the suspected rendering chain (`figv.c`'s `FullUpdate`/`Update`/`InitializeClass`/`UpdateCache`/`RedrawGroup`, `matte.c`'s `matte__Create`, `text.c`'s `"view"` keyword handler, `drawtxtv.c`'s `GenerateLineItems`/`LineRedraw`, `textv.c`'s `InitChildren`/`CreateMatte`) — all showed **zero output**, even after a full clean `make World` rebuild ruled out stale build artifacts.
- Root cause of the false negative: `text__HandleKeyWord`/`matte__Create` are **statically linked into `runapp`** (confirmed via `nm -g runapp`), unlike `figview` which is a genuine dynamic `.do`. Rebuilding/reinstalling `text.do`/`matte.do` had zero effect on the running binary — needed the full incremental-relink recipe (rebuild `.o` → rebuild the owning `.a` via its own `make libfoo.a`, never `ar clq` an existing archive directly → copy to `build/lib/atk/` → `ranlib` → `make runapp` in `src/atk/apps/` → `install -m 755`) documented in memory `feedback_incremental_rebuild`.
- Even after relinking, `matte__Create`'s own `fprintf` never printed despite `lldb` proving the function *does* execute (see below) — an unresolved observability mystery (this build has no `-g`, so `-O`'d code and a debug-info-less `fprintf` apparently don't mix reliably here). Lesson: when program-side logging goes silent but you don't trust "it's not called," switch to `lldb` breakpoints/register reads instead of adding more prints.
- Used `lldb` directly on the running process (`breakpoint set --name matte__Create`, matched hits to specific viewrefs by reading `vr->dataObject` at raw offset `0x38` via `register read`/`memory read`, since no debug info means no named-field `expr` access) and confirmed: `matte__Create` and `figview__FullUpdate` **do** get called correctly for the figure's viewref. The mechanism was never broken.
- Dumped raw memory of the live `figview` instance (`memory read -s8 -fx -c50 $x0`) and found `originx`/`panx` (identified by their position relative to the known-good anchor `scale == figview_NormScale == 8`) holding `0x0000000100000000` — 4,294,967,296 — instead of `0`.
- Root cause: `figure__Read` parses `$origin 0 0` via `sscanf(buf, "$origin %d %d", &val1, &val2)` into `long val1, val2` — the exact `%d`-into-`long` pattern from LP64 bug class #4, flagged early in this investigation and set aside since it wasn't the cause of the *first* bug. `%d` writes only the low 32 bits; a stray `1` in stack garbage landed in the upper 32 bits. `figview__SetDataObject` then does `self->panx = self->originx`, so the corrupted origin propagates straight into the pan offset, translating the entire figure's rendering ~4 billion pixels off-screen. Content was being drawn correctly — just nowhere near any visible clip region.
- Fixed both `figure__Read`'s and the sibling `figure__ReadPartial`'s `$origin` parsing to `%ld %ld`. Swept the rest of the figure package's `sscanf` calls (`figio.c`, `figogrp.c`, `figobj.c`, `figorect.c`, `figoplin.c`, `figotext.c`, `figv.c`) — every other one already correctly used `%ld`; these two `$origin` lines were the only stragglers.
- Removed all temporary `fprintf` instrumentation (`grep -rl DEBUGFIG src/` clean) before final rebuild; verified `strings build/bin/runapp | grep -c DEBUGFIG` is `0`.
- Rebuilt `figure.do` and reinstalled; relinked `runapp` after reverting the (already-removed) debug prints in the statically-linked files. Verified in `ez`: `95Summer.ez`'s figure now renders completely and correctly — boxes, ellipses, connecting lines, all visible.

### 2026-07-05 — AMS_ENV/CONTRIB_ENV newly-activated subtrees: compile fixes and scope decisions

`site.h` turned on `AMS_ENV`/`CONTRIB_ENV`, exposing `ams/`, `atkams/`, `overhead/mail/`, `overhead/eli/`, and several `contrib/` directories to the build for the first time — fresh compile errors never seen before since these trees weren't previously built. Fixed subtree by subtree, verified incrementally (`make install` per directory, not full `dependInstall`), not yet committed pending user's own test build.

**New bug class: static K&R forward-declaration missing `static`** — ~90 instances across 25+ files (`text822.c`, `ams.c`, `stubs.c`, `sendmsg.c`, `parseadd.c`, `qmail.c`, `stats.c`, `vmail.c`, `cuilib.c`, most of `ams/libs/ms/*.c`, `prsdate.gra`, `ltextv.c`, `ptext.c`, `eatmail.c`, others). A `static` function used earlier in the file than its definition, with no prior forward declaration, is implicitly declared with external linkage at first use; the later `static` definition then conflicts (real ISO C rule, not a new compiler behavior — old K&R compilers just didn't enforce it). Fixed by adding `static RetType Name();` forward declarations near the top of each file, matching the existing convention already used elsewhere in these files (e.g. `text822.c`'s pre-existing `static char *fgetsdecoding(), *UnquoteString();`).
- One look-alike that is **not** this pattern: `contrib/time/writestmp.c writestamp__UpdateTime` — a class method, correctly declared non-static (exported, in the vtable) in the auto-generated `.eh`; the `.c` definition was wrongly marked `static`. Fixed by removing `static` from the definition, not by adding a forward decl (the opposite fix). Swept the rest of today's static/non-static hits against their `.eh`/`.ch` for the same class-method mistake — none found.

**Missing prototypes for pointer-returning functions (LP64 #1 pattern)** — `ap_Shorten`, `AndrewDir`, `convlongto64`, `StripWhiteEnds` called without a declaration in scope in several `ams/libs/ms/*.c` and `ams/libs/cui/cuilib.c` files; added local `extern char *Name();` declarations. `overhead/util/hdrs/fdplumb.h`'s fd-leak-tracking macros (`#define opendir dbg_opendir`, `fopen`→`dbg_fopen`, `popen`→`dbg_popen`, `topen`→`dbg_topen`) redirected calls to `dbg_*` wrapper functions with no prototypes anywhere in the tree — fixed once in the shared header (`extern DIR *dbg_opendir(); extern FILE *dbg_fopen(); ...`), clearing ~10 "DIR\*/FILE\* from int" errors across `ams/libs/ms` and `overhead/mail/lib` in one fix.

**`sys_errlist`/`sys_nerr` redeclared with the wrong (non-const) type** — `andmchs.c`, `cui.c` (msclients), `hdlnew.c`, `prims2.c` each had `extern char *sys_errlist[]; extern int sys_nerr;`, conflicting with modern libc's `extern const char *const sys_errlist[]`. Removed the stale redeclarations (already visible via `<stdio.h>`); `andmchs.c`'s `unix_sys_errlist` assignment needed a `(char **)` cast to discard constness for that back-compat global.

**`errno` used as a variable/parameter name** — `ams__vdown`/`amsn__vdown` in `ams.c`/`amsn.c` declared `int errno` as a K&R parameter name, which macro-expands via `<sys/errno.h>`'s `#define errno (*__error())` into invalid declarator syntax. Renamed the parameter to `errnum` in both. (The same token also appears as a plain `extern int errno;` top-level declaration in a few files — harmless there, since it macro-expands into a redundant but compatible re-declaration of `__error()`.)

**Stale BSD `sprintf`-returns-`char*` assumption** — `stubs.c` had `strcat(ErrorText, sprintf(NumDum, "%d", errcause))`, relying on pre-ANSI 4.2BSD `sprintf` returning a pointer to the buffer instead of an int. Split into two statements at both call sites.

**Genuine pre-existing bug, not LP64-related** — `ams/libs/ms/rmvdir.c`: `strcat(NewPref, ':')` passed a `char` literal where a string was intended; should be `":"`. Fixed.

**Class interface signature stale relative to its own implementation** — `ams.ch`/`amsn.ch`/`amss.ch` declared `CUI_RewriteHeaderLineInternal`'s `format`/`strip`/`trust` params as by-value `int`, but the real implementation (`ams/libs/cui/cuilib.c`) and every caller (including `sendmsg.c`'s `ValidateHeader`) always passed `int *`. Fixed the three `.ch` files to declare `int *format, *strip, *trust`, matching reality, rather than changing working call sites to match a wrong interface.

**Non-mechanical fixes:**
- `atk/syntax/parse/Imakefile` hardcoded `$(BASEDIR)/bin/bison` (the vendored, disabled fork) for its `testparse` test program's `foo.tab.c` rule, instead of using the already-established system-bison approach (see 2026-06-29 entry above). Changed to plain `bison`.
- `config/darwin/system.mcr` was missing `RESOLVER_LIB`. macOS renames resolver symbols (`res_init`/`res_send`/`res_mkquery`/etc.) to `res_9_*` in `<resolv.h>` unconditionally (not gated by `BIND_8_COMPAT`) and ships them in `libresolv`, not `libc`; anything statically linking `overhead/mail/lib/rsearch.c`/`valhost.c`'s DNS host-validation (reachable from `ams/libs/ms/mswp.c`'s `ValidateMailHostName`) needs it. Added `RESOLVER_LIB = -lresolv`.
- `overhead/mail/lib/rsearch.c`/`valhost.c`: old BIND4-style resolver type names (`HEADER`, `PACKETSZ`, `MAXDNAME`, `QUERY`, `NOERROR`, `NXDOMAIN`, `C_IN`) aren't defined by `<arpa/nameser.h>` on Apple platforms unless `BIND_8_COMPAT` (or `BIND_4_COMPAT`) is defined before the include (`#ifndef __APPLE__` guards the automatic define). Added `#define BIND_8_COMPAT` before the include in both files.
- `overhead/mail/metamail/metamail/uue.c`: a local `getline(buf, size, fp)` helper (K&R signature: `char *, int, FILE *`) collided with POSIX.1-2008's `ssize_t getline(char **, size_t *, FILE *)`, now declared by `<stdio.h>`. Renamed to `uue_getline`.
- `overhead/mail/cmd/decodeid.c`: `main(argc, argv)` declared `argv` as `unsigned char **`, which modern clang rejects (`main`'s second parameter must be `char **`). Changed to `char **`; the two call sites already cast to `(char *)`.
- `overhead/eli/lib/elil.flex` (symlinked from `elil.l`), `overhead/eli/lib/prims2.c`: same stale `extern char *realloc()` / `sys_errlist` patterns as above.

**Deferred, conditionalized out of the build (not mechanically fixable):** `contrib/tm`, `ams/msclients/vui`, `ams/msclients/cui` — curses-style terminal mail clients on the pre-POSIX BSD `sgtty` API (`TIOCGETP`, `struct sgttyb`, `CBREAK`/`RAW`/`CRMOD`), removed from the kernel decades ago; a clean compile wouldn't produce working terminal I/O without a real `termios` port. `messages` (the GUI `ez` client) is the primary destination, so these are low priority. Gated behind `MK_TM`/`MK_CUI`/`MK_VUI` in `contrib/Imakefile`/`ams/msclients/Imakefile` (all previously unconditional or near-unconditional). Full rationale in `porting-assessment.md` §7a.

**Variant 3 sweep (bare `-1` through untyped class dispatch), AMS subsystem** — grep sweep of today's touched trees found 6 call sites: `environ_GetProfileInt("*.width"/"*.height", -1)` window-size defaults in `messwind.c`, `msgsa.c` (×2), `folders.c`, `sendmsg.c` (dispatched through `environ.ih`'s `(long (*)())` cast to a `long defaultValue` parameter); and `ams/delivery/trymail/trymail.c`'s `cwp_Search(..., -1, ...)` (flagged by name in `claude-history/new-tree-porting-prompt.md`, in the currently-inactive `AMS_DELIVERY_ENV`/`WHITEPAGES_ENV` code path). All fixed with `(long)-1` casts. In practice the `GetProfileInt` sites likely weren't causing visible bugs — the corrupted value gets truncated back to a 32-bit `int` by the next dispatch hop (`ams_InstallInNewWindow`'s `int w, h`), which happens to recover the correct `-1` bit pattern by coincidence — but fixed anyway per the doc's prescribed remedy rather than relying on that coincidence.

**Follow-up: two link-time errors found by the user's first full `make dependInstall` (no `-k`) since these fixes** — both in `ams/msclients/nns`, the one `msclients` subdir left in the build (`cui`/`vui` deferred, see above):
- `overhead/util/lib/getla.c`'s `getla()` read system load average via `/dev/kmem` + `nlist("/vmunix", ...)` — 1980s BSD kernel-introspection technique; Apple's libc ships the `<nlist.h>` header for source compatibility but no `_nlist` symbol at all, so anything calling it fails at link time, not compile time. Added an `__APPLE__` branch using the portable `getloadavg(3)` (`<stdlib.h>`), following the file's existing precedent of a platform-specific stub (`#ifdef NeXT` already returned a dummy `0.0`). `getla_ShutDown()` (closes the `/dev/kmem` fd) is a no-op on the new path since `getloadavg()` is stateless.
- `ams/msclients/nns/Imakefile`'s `ProgramTarget` link line didn't reference `${RESOLVER_LIB}` (see the `RESOLVER_LIB` fix above) — `nns` is the one other `libmail.a` consumer besides `messages` that reaches `valhost.c`/`rsearch.o`'s `res_9_*` symbols. Added `${RESOLVER_LIB}` to its link libs.

Both errors were pre-existing bugs the day's other fixes exposed by getting far enough to reach the link step — not regressions introduced by anything above. Verified each fix by rebuilding its directory in isolation (`overhead/util/lib`, then `ams/msclients/nns`); full-tree `dependInstall` re-run pending.

**Follow-up: runtime segfault in `messages`, LP64 #1 pattern recurring in never-before-built code** — user ran `messages` after the above fixes and hit `EXC_BAD_ACCESS` in `_platform_strlen`, called from `amsn.do`'s `CkAMSCellConfig` via `CheckAMSConfiguration`/`amsn__InitializeClass`. Root cause: `overhead/mail/lib/mailconf.c`'s `CkAMSCellConfig` calls `AndrewDir(NULL)`/`LocalDir(NULL)` with no prototype in scope — the same missing-prototype pointer-truncation bug already fixed at 23 other sites on 2026-06-30, but `overhead/mail` was never built/audited before `AMS_ENV` was turned on this session, so this instance was never caught. Fixed with local `extern char *AndrewDir(); extern char *LocalDir();` declarations. Swept all trees touched this session for the same bare (non-`environ_`-wrapped) call pattern and fixed 5 more active files: `ams/libs/ms/init.c` (both), `ams/libs/ms/hdlnew.c`, `atkams/messages/lib/stubs.c` (`LocalDir` only), `overhead/mail/metamail/metamail/metamail.c`, `overhead/mail/metamail/metamail/mailto.c`, `overhead/eli/lib/prims1.c`. Calls through the `environ_AndrewDir(...)` macro (`sendaux.c`, `ams.c`, `contrib/srctext/html/html.c`) were already safe — that macro dispatches through the class vtable with an explicit `(char *)` cast, the correct pattern, not the bug. All 6 fixed files rebuilt clean (`make install` per directory); `messages` retest pending.

**`messages` runs (2026-07-05)** — "mail (Private BB; 0 new of 0)" confirmed in the folder panel on the primary host. All 14 fix commits plus a docs commit checked into fossil.

**Follow-up: `messages` segfaults on second host (`spoon`), same fossil checkout — two more LP64 bugs found via lldb** — user rebuilt the identical tip revision on `spoon` and got an immediate real `SIGSEGV` (confirmed via plain `lldb ./messages` / `run -d` / `bt` in a remote-login shell; the earlier "process is gone" symptom was specific to however it was launched before, not a codesigning issue). Backtrace: `amsn.do CUI_GetHeaders` (in `ams/libs/cui/cuilib.c`) → `amsn__CUI_GetHeaders` → `captions.do captions_InsertCaptions`/`captions__InsertUpdatesInDocument` → `folders.do folders__ReadMail`/`folders__UpdateMsgs` → `msgsa.do messagesapp__Start`. Crash instruction (`ldurb w8, [x22, #-0x15]`) decoded to `AMS_GET_ATTRIBUTE`/`AMS_ID` arithmetic on a wild `s` pointer inside `CUI_GetHeaders`'s header-scanning loop, address `0x16fe00014` (stack-range, consistent with `s` having walked far past its buffer). Two distinct `long`/`int` mismatches in `CUI_GetHeaders`'s signature, both invisible at compile time since none of the crossings have a real prototype in scope:

- **`startbyte` (by value)**: `long` in `cuilib.c`'s `CUI_GetHeaders` (and in the dormant, already-correct `ams/libs/snap/cuisnap.c`), but `int` in `ams.ch`/`amsn.ch`/`amss.ch`, their `.c` implementations, and the real driving variable — `atkams/messages/lib/capaux.c`'s `totalbytes`, accumulated across `InsertUpdatesInDocument`'s header-read loop. Register-garbage-dependent, hence host-specific: fine here, crashed on `spoon`.
- **`nbytes`/`status` (by pointer, worse)**: same `long` vs `int` split, but as pointee types (`long *` vs `int *`). `ms/libs/ms/headers.c`'s `MS_HeadersSince` — the function that actually fills these in for the local-mailbox path — declared `int *numbytes, *bytesleft`; `cuilib.c`'s `CUI_GetHeaders` and everyone above it read them back as `long *`. `cuisnap.c`'s dormant SNAP variant already had `long *` throughout, confirming `headers.c` was the file that never got updated, not the other way around.

Fixed end to end: `ms/libs/ms/headers.c` (`MS_HeadersSince` signature + one `%d`→`%ld` debug format), `ams.ch`/`amsn.ch`/`amss.ch`, `ams.c`/`amsn.c`/`amss.c`, `atkams/messages/lib/capaux.c` (`totalbytes`/`numbytes`/`status` → `long`), `foldaux.c` (`numbytes`/`bytesleft` → `long`), and — for forward-compatibility since `MS_HeadersSince`'s signature changed — the dormant SNAP-server side `ams/ms/ms.c` (`SNAP_ENV` currently off). All rebuilt clean (`make install`, zero errors, warnings only). `messages` retest on `spoon` pending.

**Deferred, conditionalized out of the build: `contrib/bdffont`** — its parser splits bison's output across a generated `bdfparse.tab.c` and a hand-maintained `bdfparse.act` (grammar action bodies) that doesn't exist anywhere in the tree — no fossil history, no generating rule. `overhead/mkparser` is a working tool but for a different, merged-output scheme; doesn't apply. Reconstructing `bdfparse.act` means hand-writing parser actions from the grammar with nothing to verify against — not a mechanical fix. (Same broken convention also exists in `atk/ness/type`, `atk/ness/objects`, and `atk/syntax/parse`'s `testparse`, but none of those are currently reachable — `atk/ness` needs `MK_NESS`/`MK_AUTHORING`, undefined; `testparse` isn't part of `make install`.) Along the way, fixed `contrib/bdffont/Imakefile`'s bison invocation itself (`bison -d -r -n` — modern bison's `-r`/`--report` requires an argument and was swallowing the following `-n`, a separate flag error unrelated to which bison binary runs) to `bison -d -v`; this let the build reach the real blocker. Gated `bdffont` behind `MK_BDFFONT` in `contrib/Imakefile` (was unconditional). Full rationale in `porting-assessment.md` §7b.

### 2026-07-07 — cui builds: sgtty was a red herring, real bug was a missing link flag

User enabled `MK_CUI` in `config/site.h` and ran a full build; `cui.c`,
`cuifns.c`, `unixmach.c`, and `morprntf.c` all compiled clean (only the usual
K&R deprecation warnings) — the failure was at the final `cuin` link step,
with the exact same undefined symbols as `nns`'s 2026-07-05 fix:
`_res_9_dn_expand`, `_res_9_dn_skipname`, `_res_9_init`, `_res_9_mkquery`,
`_res_9_send`, all referenced from `libmail.a` (`valhost.o`, `rsearch.o`),
pulled in transitively through `libcui.a`/`libcuin.a`.

Before applying that fix, checked whether `cui`'s BSD-`sgtty` reference
(the reason it was deferred/conditionalized in the first place, see
`porting-assessment.md` §7a) was actually the blocker. It wasn't: the only
sgtty code anywhere in `ams/msclients/cui/*.c` is a `#ifdef POSIX_ENV`/
`#else` fallback in `cui.c`'s `GetBodyFromCUID()` (under the rarely-built
`METAMAIL_ENV`), and `POSIX_ENV` is unconditionally defined by
`config/darwin/system.h:79-80` — so the `termios` branch is the one that's
always compiled on this platform; the sgtty `#else` was already dead code.
`cui` also doesn't use curses at all (unlike `vui`), so the "curses-style
sgtty client" description in `porting-assessment.md` §7a was never accurate
for `cui` specifically — it was written generically for the `tm`/`vui`/`cui`
group without checking each one.

Real fix: `ams/msclients/cui/Imakefile`'s `ProgramTarget` lines for `cuin`/
`cuis` never got `${RESOLVER_LIB}` appended, unlike `nns` — presumably missed
on 2026-07-05 because `cui` was still gated off (`MK_CUI` undefined) and
untested at the time. Added `${RESOLVER_LIB}` to both lines, regenerated the
Makefile (`make Makefile`), relinked (`make cuin`), installed (`make
install`). Zero errors; `build/bin/cuin` is a real arm64 Mach-O, `build/bin/cui`
symlinked to it as before.

Applied the identical one-line fix to `ams/msclients/vui/Imakefile`'s
`vuis`/`vuin` lines for consistency (same gap, same cause). `vui` does not
yet build far enough to benefit from it, though — it fails at compile time in
`andpnlm.c` on undeclared `CM`/`SO` termcap globals, a real and separate
curses-port issue, deferred per `porting-assessment.md` §7a. `contrib/tm`
untouched.

Net effect: `cui` is no longer deferred. It builds, links, and installs.
Cleared the way for the "cui + gendemo" roadmap item — `gendemo` needed a
working `cui` to populate the `amsdemo` demo folder, not a `termios` port.

Updated `porting-assessment.md` §7a to drop `cui` from the sgtty-deferred
list and record the real story.

### 2026-07-07 — gendemo: cwd bug, then a new bug class (modern-flex init-flag polarity)

`gendemo` (`build/etc/gendemo`, a csh script) appeared to run — processed
all 23 demo posts, "Reconstructed folder" message printed — but `messages`
showed the folder empty. Two independent bugs, found in sequence:

**Bug 1 — `gendemo` reads its post data relative to cwd, not `$ANDREWDIR`.**
The script's `while (! -e d$i.heads) break` loop looks for `d1.heads`,
`d1`, ... `d23.heads`, `d23` in the current directory. Those files live only
in `src/ams/demo/` and are never installed anywhere (`Imakefile` only
installs the script itself and `amsdemo`, not the `d*` data files). Running
`gendemo` from anywhere else silently breaks out of the loop on the first
iteration, creates the folder via `cui ... create`, and produces zero posts.
Fix (for this session): `cd src/ams/demo` before invoking `gendemo`.
`ANDREWDIR` is only used to locate the `cui` binary and `etc/arpadate`, not
the post data — the "wizard mode gendemo" doc paragraph in the Imakefile
implies but doesn't state the cwd requirement.

**Bug 2 — `cui`'s final `recon` step segfaulted.** With cwd fixed, `gendemo`
copied all 23 posts, then crashed during the `recon` step's caption-building
pass. `lldb` backtrace: `mail_parseyylex` (`overhead/mail/lib/parsel.c`) →
`locallexer` → `parser_Parse` → `ParseAddressList` → `FindPrettiestFromString`
→ `FindPrettiestName` → `BuildCaption` → `MS_ReconstructDirectory`. Crash
instruction: `strb w8, [x21]` writing through a NULL `mail_parseyy_c_buf_p`.

Root cause: **`parsel.c` has no fossil history** — it's regenerated at build
time from `parsel.flex` (`FlexOrLexFileRule`/`LexWithReplacement(parsel,
mail_parseyy)`) — and modern flex (2.6.x, what's on this Mac) inverted the
meaning of its internal `mail_parseyy_init` flag relative to the flex this
code was written against in 1994. Old flex: nonzero meant "please
reinitialize on next call." Modern flex: nonzero means "already initialized,
skip buffer setup." `parseadd.c`'s `ParseAddressList` calls `pareset_lexer()`
before every parse (`overhead/mail/lib/parsel.flex:220`), which did
`yy_init = 1;` — under modern flex this poisons the flag *before the first
lex call ever happens in the process*, so `mail_parseyylex()` skips creating
its scan buffer entirely, leaving the static `mail_parseyy_c_buf_p` at NULL.
First address parse in the process, first line of the function, NULL write.

Fix: `pareset_lexer()` → `yyrestart(yyin)` (renamed by the build's sed step
to `mail_parseyyrestart(mail_parseyyin)`). `yyrestart` is flex's actual
public API for "reinitialize the scanner for new input" and has been stable
across every flex version since the code was written — the bug was bypassing
that stable API to poke a private, version-dependent internal flag. Rebuilt
`libmail.a`, relinked `cuin`/`cui`. `recon` completes cleanly and repeatably
now ("Reconstructed folder ~/.MESSAGES/amsdemo with twenty-three entries").

**Same bug, second instance: `overhead/eli/lib/elil.flex`.** ELI ("Embedded
Lisp Interpreter," the basis for FLAMES, AMS's mail-filtering language) has
an identical hand-written `reset_lexer()` (called from `eliy.gra` before
every parse) doing the same `yy_init = 1;` poke, generated via the same
`FlexOrLexFileRule`/`LexWithReplacement(elil, eliyy)` machinery — also
untracked in fossil, also regenerated by modern flex. Same fix:
`eliyyrestart(eliyyin)`. `libeli.a` is linked into `cui`, `amsn.do`
(messages), `nns`, and `overhead/eli/bglisp` (ELI's own test REPL) — rebuilt
`libeli.a` and relinked all four. (`vui` still doesn't compile for the
unrelated pre-existing curses/termcap reason noted above; `ams/ms` has no
generated Makefile yet, not part of the current build.)

**Verification:** static — confirmed the sed-based prefix rename produces
correct code (`mail_parseyyrestart(mail_parseyyin)`,
`eliyyrestart(eliyyin)`) and both compile clean. Dynamic — `cui`'s `recon`
verified crash-free across multiple repeated runs. `bglisp` (which would
exercise the ELI lexer directly) hangs uninterruptibly at startup in this
sandbox even with `/dev/null` on stdin — a separate, pre-existing issue
unrelated to this fix; not chased further.

**Legacy-platform safety, checked explicitly:** `FlexOrLexFileRule` picks
between the `.flex` source (used when `FLEX_ENV` is defined — unconditional
on Darwin, `system.h:75-76`) and a parallel `.lex` source (for sites without
flex). The `.lex` siblings (`parsel.lex`, `elil.lex` — genuine 1994 fossil
history, not generated) were checked: their `pareset_lexer()`/`reset_lexer()`
are no-ops (`{ return 0; }` / `{}`) — plain AT&T lex has no persistent
scan-buffer state to reset in the first place, so the bug (and the fix)
simply doesn't apply there. The two code paths are selected at Imake time
and can't interact, so this fix carries no risk to any non-flex legacy build.

**Wider sweep, whole tree:** searched all of `andrew-6.4` (not just `src/`)
for `.flex`/`.lex`/`.ll` files and cross-checked against every Imakefile
referencing flex/lex build rules. Exactly four flex-based lexers exist,
total, and all four are now accounted for:

| Lexer | Subsystem | Status |
|---|---|---|
| `overhead/mail/lib/parsel.flex` | mail address parsing | fixed |
| `overhead/eli/lib/elil.flex` | ELI/FLAMES filter language | fixed |
| `doc/mkbrowse/browserpp.flex` | doc browser generator | already correct (`yyrestart(yyin)`) |
| `overhead/class/pp/classpp.flex` | Class preprocessor | no reset-lexer pattern — lexes once per invocation, not exposed |

This bug class is closed out; see `porting-assessment.md` §13 for the
generalized writeup.

**New bug found, not yet fixed:** while re-verifying `recon` repeatedly, hit
an intermittent (not every run) crash in a completely different, unrelated
lexer/parser: `parsedate` → `parser_Parse` → `memmove` heap overrun, building
the date field (`BuildDateField` → `MS_ReconstructDirectory`). This is
bison-generated (`ams/libs/ms/prsdate.c` from `prsdate.gra`, also untracked
in fossil) — a different generator, different subsystem, different failure
shape (data/size-dependent, not a fixed polarity flag) from the flex bug
above. Not investigated further this session; flagged in `roadmap-old.md`'s
gendemo section as the next thing to chase.

### 2026-07-12 — calc inset "missing leading characters": rootless XQuartz Xft recomposite lag

Follow-on to the same session's ghost-text fix (see `claude-history/calc-text-rendering-investigation.md`, open bug #1, and its `Outcome` section). After the
AA-erase-by-overdraw fix, a new symptom appeared: typing a multi-character
expression showed only a trailing suffix of the correct string (e.g. typing
`123` displayed `1`, `2`, `23` instead of `1`, `12`, `123`), though the
final `=` result and any single-character string always drew correctly.
Key diagnostic clue from the user: switching window focus away and back
always corrected the display immediately.

Root-caused via a live `XGetImage` framebuffer readback added directly
inside `xgraphic_DrawChars`'s Xft path (`src/atk/basics/x/xgraphic.c`):
after each glyph draw, `XSync` then read back the actual server-side pixel
at the glyph's position. The readback consistently showed the *correct*
painted pixel even at the exact moments the user visually saw the
character missing on screen. That's conclusive: the X server's drawable
genuinely has the right content — the bug is downstream, in getting that
content onto the visible native window surface. Root cause: rootless
XQuartz does not reliably recomposite the native window surface for
Xft/Render-extension draws on their own; a focus change (or anything else
that forces a full recomposite) is what was making the correct pixels
visible.

This also explains a second, broader symptom the user found independently
while root-causing this: typing text on the line above a calc inset (an
ordinary core-X/Xft mixed redraw, forcing a multi-line relayout near the
inset) could leave text invisible until unrelated nearby redraw activity
(typing more text) incrementally "revealed" it. Same root cause, not
calc-specific — calc's display is just unusually exposed to it because
nothing else nearby generates incidental redraw traffic to mask the lag.

**Fix:** in `xgraphic_DrawChars`'s Xft per-glyph-width-table loop, after
`XftDrawDestroy`, issue a self-`XCopyArea` (source and destination both the
drawn region of the target window, through the *core* X11 path) to force
the compositor to recognize the region as dirty and recomposite it. Core-X
draws were already confirmed to repaint reliably (calc's buttons/borders
render fine), so kicking Xft-drawn regions through that same path is a
targeted, low-risk workaround — no change to *what* gets drawn, only an
extra no-op-content copy to trigger recomposite. Scoped to the
widths-table loop (the path calc and all normal text drawing use); the
`widths == NULL` string-extents fallback was not touched (rare path, not
observed to hit this symptom in testing).

Confirmed fixed by the user for both the calc digit-display symptom and
the text-near-inset symptom. Diagnostic scaffolding (the XGetImage
readback and two prior-session `XGDEBUG`-gated logging blocks in the same
function) was removed after confirmation; only the `XCopyArea` kick and
its explanatory comment remain.

### 2026-07-17 — Milestone 1: SMTP send, real mail sent end-to-end

Kickoff of the AMS-over-IMAP/SMTP project (plan of record:
`ams-IMAP-project.md`; architecture decision: the local `.MS_MsgDir`
store stays the cache, a sync agent mirrors IMAP, AMDS delivery stays
excluded). `tlscon`/`netrc`/`smtpsub` modules added to `overhead/mail/lib`;
`dropoff()` now routes through `smtp_dropoff()` when the `smtphost`
preference is set, falling back to the legacy sendmail pipe otherwise.
Implementation delegated to a Sonnet session against
`claude-history/smtp-send-prompt.md` (three stages, two review gates).
End-to-end acceptance (`revival/tools/smtp-send-test`): a scripted `cui`
composes and submits over TLS to Fastmail, real send confirmed, captured
as a durable regression suite (`revival/tests/smtp-protocol-tests`).

The acceptance push exercised code paths nothing had reached before and
found three latent bugs, all pre-existing:

- **`cui` NULL address-validation crashes** — 7 call sites segfaulted at
  the `CC:` prompt on certain inputs; guarded.
- **`fdplumb.h`'s `open`→`dbg_open` rename macro, applied before the
  system's `fcntl.h` was parsed, corrupted `open`'s variadic signature**
  (see `revival.md`, "Old bugs never found till now" — the debug-tracing
  macro entry). Fixed by parsing `fcntl.h` before performing the rename,
  in the shared header, so the ordering hazard can't recur regardless of
  which file includes it first. Also normalized include order across
  `ams/libs/ms`.
- **Client-side destination-host DNS validation** rejected real-world
  mail hosts; now defaults off whenever `smtphost` is configured, with a
  `validatedesthosts` preference to force it back on.

### 2026-07-17 — Milestone 2: IMAP spike; decision to hand-roll the client

A spike driver (`overhead/mail/lib/imapspike.c`) ran a full
CAPABILITY/LOGIN/LIST/EXAMINE/SEARCH/FETCH sequence, read-only, against
live Fastmail, including a real 9.6KB body literal. Decision made and
recorded (`ams-IMAP-project.md` §8): **hand-roll** the IMAP client rather
than adopt an existing library. Key transport finding: `tlscon`'s fixed
4KB line buffer, plus no resync primitive, wedges the connection on large
single-line responses — hit via `UID SEARCH ALL` against a 3,939-message
mailbox. Fixing that (a growable buffer, plus a reconnect design) became
milestone 3's first task; sync will target Fastmail's `ESEARCH`/
`CONDSTORE` extensions instead of naive `SEARCH ALL`. `tlscon_ReadBytes`
added (additive; both SMTP regression suites re-passed).

### 2026-07-18 — Milestone 3a: tlscon hardened, imap_prot lands

`tlscon_ReadLineAlloc` replaces the fixed-buffer line reader with a
growable one (drain-before-refill) — the spike's `UID SEARCH ALL` wedge
case now survives. `imap_prot.[ch]` landed alongside it: the tree's first
born-ANSI module (a full-prototype header from the start, `scanf` banned
in favor of `strtoul`/`strcasecmp`), ESEARCH-aware, with streaming body
fetch and a reconnect-with-UIDVALIDITY-check contract.
`revival/tests/imap-protocol-tests` passes 9/9 live against Fastmail;
both SMTP suites still green.

### 2026-07-18 — Milestone 3b: imapsync, a one-way IMAP mirror

`ams/msclients/imapsync/` mirrors a live IMAP mailbox one-way into a
local `mspath` root (`~/.IMAP/fastmail/.MESSAGES/...`) through the
store's own code, via one additive MS entry point
(`MS_AppendFileToFolderWithId`, caller-supplied id/date). Mirrored
message ids are deterministic, `f(UIDVALIDITY,UID)`, encoded in
base32hex rather than mixed-case base64 — plain base64 encoding collided
as filenames on APFS's case-insensitive filesystem (two live pairs hit
during testing, e.g. `...GvA`/`...Gva`; see `revival.md`'s "message
store's id scheme assumed case-sensitive filenames" entry). The native
store's own `ams_genid()` ids are mixed-case base64 too, so they carry
the same, much rarer, collision risk on this platform — not fixed, left
as a documented hazard, revisit if a native-store collision is ever
observed. Flags map through `MS_AlterSnapshot`; CONDSTORE/HIGHESTMODSEQ
drives incremental refresh skip; `-full-check` handles expunge marking;
empty body fetches (a live Fastmail expunge-during-FETCH race) get
skip-and-retry. `revival/tests/imap-sync-tests`, 6 live cases including a
scripted `cui` browse; a real-mailbox browse in `messages` confirmed by
hand.

The close-out regression run surfaced two more pre-existing bugs, both
fixed same day:

- **`WritePureFile` (`ams/libs/ms/rawdb.c`) unlinked its target on *open*
  failure.** Under `O_CREAT|O_EXCL`, an `EEXIST` collision therefore
  deleted an *existing* message's body file instead of the (nonexistent)
  partial write the cleanup was written for — a 35-year data-loss bug
  (see `revival.md`, "An error-cleanup path deleted the wrong file on a
  name collision"). Also freed the in-memory `Msg` on the other
  duplicate-append exit paths that had been leaking it.
- **RFC 3501's `UID n:*` range always includes the highest existing
  UID**, so an idempotent re-run could re-present the top
  already-mirrored message as a new candidate. `imapsync` now filters
  candidates at or below its watermark and pre-checks the deterministic
  `+<id>` body file before appending, robust even when the store's
  Message-ID-based duplicate check can't catch a re-append.

### 2026-07-19 — First real-send bugs: from-address, formatted-send default, RCPT TO

Sending real mail from `messages` for the first time surfaced three
bugs, all pre-existing and all fixed same day:

- **From-address showed as `wdc@Mac-mini.lan`.** `MS_SubmitMessage`
  (`ams/libs/ms/submsg.c`) deletes any user-supplied From and stamps
  `Me@MyMailDomain`; `MyMailDomain` resolves to the AndrewSetup key
  `ThisDomain` (`overhead/util/lib/svcconf.c`), falling back to the
  hostname when no AndrewSetup exists at all — hence the `.lan` address
  and Fastmail's `551 5.7.1 Not authorised` on external relay. Fixed
  with no code change: the AndrewSetup search path ends at
  `${ANDREWDIR}/etc/AndrewSetup`, so a `build/etc/AndrewSetup` containing
  `ThisDomain: fastmail.com` corrects every AMS client at once. Verified
  live: `cui` send now arrives as `William Cattey <wdc@fastmail.com>`.
- **Formatted send was the default even for plain text.** Root cause:
  `MS_GetConfigurationParameters`'s out-parameters are declared `long`
  in the class layer but `int` in the real implementation — the LP64
  variant #6 `.ch`/wrapper width-drift pattern (`porting-assessment.md`
  §19) — leaving garbage in the caller's `long` globals and forcing
  every send down the raw-ATK path regardless of content. Plain bodies
  now auto-strip; formatted bodies offer a choice via the
  `mailsendingformat` preference.
- **`RCPT TO` was built from a display-form address.** `dropoff()`
  callers pass full RFC 822 addresses; the kept-blind-copy fallback
  (`submsg.c`) appends `MyPrettyAddress` (`William Cattey
  <wdc@fastmail.com>`) verbatim to the envelope vector when direct
  insertion of the blind copy fails, and `smtpsub.c` then wrapped that
  in a second bracket pair — `RCPT TO:<William Cattey
  <wdc@fastmail.com>>`. Fastmail accepts at RCPT time but fails the
  whole transaction after DATA with `501 5.1.3 Bad recipient address
  syntax`, so every recipient reads as bad — caught by the user reading
  an `AMS_SMTP_TRACE=1` transcript. Fixed with `smtp_addrspec()` in
  `smtpsub.c`, reducing each `tolist` entry to a bare addr-spec
  (`ParseAddressList`, strip comments and display phrase, unparse
  unfolded) at the protocol boundary, healing all callers. Verified with
  `smtptest.test` and a display-form recipient: 501 before, queued
  after. Left open: why the blind copy's *direct* insertion fails in
  this setup at all — with the envelope fixed, keep-blind now mails a
  copy instead of failing the whole send, but the direct-file path
  should work and doesn't yet.

### 2026-07-19 — fdplumb Gate 1: preferences load failures, not fdplumb

Investigated the `<critical:fdplumb>` "File descriptor replaced!" message
and an accompanying transient preferences blackout, both observed during
IMAP testing. Closed by a Fable session's static analysis
(`claude-history/fdplumb-REPORT.md`): preferences load through raw libc
`fopen`, not the `dbg_fopen` wrapper `fdplumb.h` redirects to, so
fdplumb itself is exonerated for the blackout. Fixed anyway, on general
robustness grounds: `profile.c` now retries transient load failures and
logs `errno` instead of failing silently, and two `dbg_dup2`/`setprof.c`
crash paths are fixed. Remaining: runtime monitoring of the new errno
log to catch a real recurrence, low priority, not yet done.

### 2026-07-19 — messages crashes on exit: LP64 pointer truncation in FindInDirCache

`EXC_BAD_ACCESS` in `MS_SetAssociatedTime` (`amsn.do`), reached from
`captions__MakeCachedUpdates` ← `ams__CommitState` during the quit
keystroke. Root-caused same day: `FindInDirCache` (`msdir.c:680`,
returns `struct MS_Directory *`) is declared in no header;
`setasct.c:48` calls it undeclared, so its pointer return truncates to
32 bits and the later cast re-extends it — the standard LP64 variant #1
pointer-truncation signature (`porting-assessment.md` §12), long-latent
(1991), not a regression. The cache-miss sentinel
`(struct MS_Directory *) -1` happens to survive truncation intact, so
nothing crashes until the directory is actually in the cache — i.e.,
until a folder has actually been visited — which is why the trigger was
specifically "click into the mirrored INBOX's captions." Fixed with a
one-line `extern struct MS_Directory *FindInDirCache();` in `setasct.c`,
its sole external caller.

### 2026-07-21 — MIME body display in messages

New `mimepart` module (`ams/libs/hdrs/mimepart.h` +
`ams/libs/shr/mimepart.c`), wired into `text822.c`:
multipart/alternative prefers `text/plain`, html-only mail gets an
interim tag-strip shim, multipart/mixed lists non-text parts as
`[attachment: ...]` lines, and UTF-8 `text/plain` finally renders
instead of falling through to a dead metamail button. Three gates, all
closed (`claude-history/mime-display-REPORT.md`); a 9-case synthetic
fixture suite (`revival/tests/mime-display-tests`) covers the parser.
By-hand acceptance against a real mailbox (Gate 3) found and fixed two
more pre-existing, unrelated bugs blocking this from working at all:

- **`GetHeader`'s header/body-boundary check was CRLF-blind** — it
  scanned only for a bare `\n` line, so a genuine CRLF-terminated body
  (real IMAP mail; local mail had always arrived already normalized to
  bare LF) was swallowed whole into the "minor headers" display. This
  is what produced the wall-of-headers/tiny-font/undecoded-`=20`/
  stray-bold symptoms seen first. Fixed to recognize CRLF as well as LF
  at both the boundary check and the quoted-printable soft-line-break
  decoder one call downstream.
- **`text822.do`'s Imakefile link line silently omitted
  `libmsshr.a`.** `-undefined dynamic_lookup` lets a missing library
  pass at build time; the first real call to a `mimepart_*` symbol
  crashed at runtime instead. Fixed and verified with `nm -m`.

One follow-on left open: a `multipart/mixed` attachment renders as a
bare `?` instead of the expected `[attachment: ...]` line — not yet
root-caused.

### 2026-07-22 — Subscribe/Unsubscribe crash: stale directory-cache pointer

`atkams/messages/lib/folders.c`: clicking a folder's "What do you want
to do with 'X'?" menu, then choosing Subscribe or Unsubscribe, walked
freed memory and crashed. The function held the folder's name strings as
raw pointers straight out of the directory cache across the (non-modal)
dialog wait; if anything freed that cache entry in the meantime — the
IMAP mirror's refresh traffic makes this the normal case rather than a
rare one — the pointers went stale in place (see `revival.md`, "A
folder-action dialog held pointers into a cache that could be freed
while it waited"). A sibling path in the same function, reached by "see
the messages," already duplicated its strings first; the fix extends the
same heap-copy discipline to subscribe/unsubscribe and "alter
subscription status." Subscribing alone did not yet fix default folder
visibility (mirrored INBOX still hidden on restart) — tracked separately,
resolved the next day, below.

### 2026-07-23 — Folder visibility resolved: a site-config flag, not subscriptions

Root cause was `AMS_OnlyMail`, a site-config global defaulting to `1`
without `RUN_AMDS_ENV`, which restricted the default "Expose New" view to
`$HOME/.MESSAGES` regardless of subscription status — not the
subscription-defaults theory the investigation started from. That
earlier theory (imapsync auto-subscribing mirrored folders at creation
time) was tested live and falsified before this was found; see
`claude-history/folder-visibility-REPORT.md`'s "Correction" section.
Fixed via `AMS_OnlyMail: No` in `build/etc/AndrewSetup`; a new tool,
`revival/tools/write-andrewsetup`, regenerates that file after `make
Clean` so the setting survives a clean rebuild. Mirrored folders now
need that setting plus Ask/Show-All subscription (plain Subscribe is not
enough) to appear by default; documented in `quickstart.md` and
`mail-quickstart.md`.

### 2026-07-23 — Milestone 4: IMAP writeback, three gates

Local edits to a mirrored mailbox now replay back to the real IMAP
server, closing out AMS-over-IMAP as feature-complete (capture,
suppression, flags/purge/append replay, crash-safe resume), confined in
testing to a dedicated `Revival/WritebackTest` mailbox.

- **Gate 1** (`fb4876a`): per-folder change-journal capture at the four
  MS mutation points, plus suppression so a local write triggered by the
  sync agent's own mirroring doesn't re-journal itself.
- **Gate 2** (`83dc58c`, report `6879cdf`): `imap_prot` write entry
  points, flags/purge replay, replay-then-mirror server-wins ordering.
  **One real incident happened here and was fully resolved same day**: a
  Gate 1 test suite that was safe when written became unsafe once Gate
  2's replay went live, and it permanently deleted one real, roughly
  2009-vintage message from the live account. Not recoverable; the user
  chose not to pursue further recovery. The suite was retargeted to the
  dedicated `Revival/WritebackTest` sandbox and its suppression-semantics
  assertions fixed.
- **Gate 3** (`df2a94c`, report `164f736`): append-record replay, the
  final gate, plus CRLF-normalizing APPEND bodies for server
  compatibility.

Also found and recorded this milestone, mechanically distinct from the
LP64 family: a K&R-style empty-parens `extern` declaration of a new
variadic function (`MSJournal_Record`) crashes on arm64 — the ABI
passes variadic arguments on the stack and fixed arguments in registers,
so an under-declared call site emits the wrong calling convention
regardless of word width (`porting-assessment.md` §18). Fixed with a
full `...`-prototyped extern at the call site.

Remaining: a `messages`-GUI hand test (two-line instruction in the Gate
3 report) is the user's to run, not automated.

### 2026-07-23 — `-fwritable-strings`: the real fix for writable-string-literal crashes

Closes out the `strlit-sweep` task queued after the atomlist Set-Options
crash (a literal string mutated in place, faulting on Apple's read-only
`.rodata`). The suspected root cause — scattered in-place literal
mutation needing a file-by-file sweep — wasn't it:
`config/darwin/system.mcr` had simply never set `-fwritable-strings`,
and Apple clang, unlike real gcc since 4.0, still implements that flag.
One-line fix, full rebuild, verified live
(`porting-assessment.md` issue #1). A `-Wwrite-strings` scan afterward
quantified the scope for the record: 26,628 literal→`char*` sites
tree-wide, almost all inert boilerplate or never-mutated static tables;
a full source cleanup was considered and rejected as disproportionate
given the flag fix. The three call sites already confirmed
reachable-by-literal (`help`'s default topic, `~/.cuirc`, `fdbbdf`'s
hexout pad) got belt-and-suspenders fixes anyway, copying to writable
local buffers before mutation.


### 2026-07-24 — M2 point 0: `-Wincompatible-pointer-types` census, three fixes, and the Group A rollout (with a live correction)

**Note on the remaining gap:** the 07-15–07-23 entries above (AMS-over-IMAP
through milestone 4, folder-visibility, mime-display, fdplumb,
`-fwritable-strings`) were backfilled from `roadmap-old.md` and
`claude-history/*-REPORT.md`. Still not backfilled: 07-09–07-11 (the tail
end of the M1 rollout) and the gap after this entry — 07-25 (M2
completion), 07-30–08-02 (M3), and 08-03–08-07 (M4 plus the
strict-prototypes census/retype/triage). Same sourcing plan applies:
`roadmap-old.md`, `porting-assessment.md`, and `claude-history/` batch
reports.

**Census** (`revival/doc/claude-history/m2-census-REPORT.md`): classified
all 483 `-Wincompatible-pointer-types` warnings from a fresh full build.
67 `int*/long*` instances collapsed into 13 shared root shapes; 18
`char** → char*` instances collapsed into one root cause. A stretch-goal
sweep for LP64 variant #6 (see the table above) found `MS_ParseDate` live
and reachable with uninitialized locals.

**Three same-day fixes**, verified by full rebuild plus a live smoke test,
committed separately (`0a6cf595ef`, `f4a9d6909b`, `c496c2a9ea`):
- `fontdesc_StringBoundingBox`'s `.ch` signature widened to `long *`,
  matching its `StringSize`/`TextSize` siblings (`fontdesc.ch`,
  `fontdesc.c`); one caller (`atk/figure/figotext.c`) that genuinely used
  `int` locals matching the *old* signature was widened to match, to
  avoid turning a correct call into a new bug.
- `MS_ParseDate` (LP64 variant #6, instance 2) — see porting-assessment.md
  §19 for the full writeup.
- `CUI_DisambiguateDir`/`CUI_RewriteHeaderLine`/`CUI_RewriteHeaderLineInternal`
  — root cause was in `ams.ch` itself: three methods typed `char *` when
  their real `ams/libs/cui/cuilib.c` implementations take `char **`.
  Every one of the 18 callers was already correct; clang's own "remove &"
  fix-it suggestion would have broken all of them. Fixed by widening the
  three `.ch` signatures, not touching any caller.

**Group A rollout**: the remaining 45 `int*/long*` instances (9 shapes,
20 files) — caller declares `int`, callee's typed dispatch wants `long *`
— fixed by widening each caller's local to `long`, after manually
checking every secondary use of each variable (comparisons, array
indexing, struct-field assignments, format strings) for width
assumptions. Found and fixed 4 pre-existing `%d`-vs-the-new-`long`
format-string mismatches along the way (`capaux.c` ×2, `folders.c` ×2,
`foldaux.c`, `bushv.c`).

**Live correction, same session:** wdc caught a garbled live message
("Zero of your two subscriptions have changed, (-<huge number>) have
nothing new") after the Group A rollout, from `folders.c`'s
`MS_NameChangedMapFile` call. Root-caused to LP64 variant #6 (see table
above and porting-assessment.md §19, instances 3–4): four `ams/libs/ms`
functions (`MS_GetDirInfo`, `MS_GetNewMessageCount`,
`MS_GetSubscriptionEntry`, `MS_NameChangedMapFile`) have real `int *`
out-params despite `.ch` saying `long *` — invisible to the compiler,
and previously harmless only because the affected callers still declared
`int` (an accidental width match masking the `.ch` bug). `fossil blame`
traces both the `.ch` spec and the real implementations to the initial
2026-06-24 import (`b28115fb2e`) — original 1990s source, dormant on
ILP32 (`int`==`long`==32 bits there) for ~35 years, not something any
prior porting pass introduced. This session's own Group A rollout
(above, same entry) is what widened the callers and made it live.
Widening those
callers to `long` (correctly fixing the *visible* warning) removed the
accidental masking and exposed the *invisible* one: a 4-byte store into
now-8-byte, uninitialized stack slots. Corrected by narrowing `.ch` and
the three class wrappers back to `int *` (matching the real
implementations and every non-class-dispatch caller in the tree) and
reverting the three affected callers (`capaux.c`, `folders.c`,
`foldaux.c`) back to `int`, rather than fixing the real implementations
— unlike `MS_ParseDate`, here every other caller already agreed on
`int`, so the `.ch` spec was the one that had drifted. After the
correction, all other 7 Group A shapes were re-verified directly against
their real implementations (not just `.ch`) as a precaution; no further
instances of variant #6 were found among them. Full rebuild clean both
before and after the correction; wdc confirmed live via Inbox that the
message reads correctly post-fix.

As of this entry: not yet committed (pending final smoke-test
confirmation); the corrected state is described above, not the
intermediate broken one.

### 2026-08-08 — convertraster: full functional test pass, three bugs found and fixed

Standalone app, not previously exercised. Tested every switch and format
documented in `convras.help` (raster/RF/MacPaint/PostScript/Xwd/Xbitmap
conversion, `-n`/`-u`/`-l`/`-r`, `-c` crop, `-p` PS scale, stdin/stdout,
default-type inference) against `dragon.raster`, verifying output by
decoding the raster format directly (script, not eyeballing) and
comparing pixel data byte-for-byte across round trips. Three real bugs
found, all in code nobody had run on this port before now; two are new
LP64 variants distinct from the six already catalogued above.

- **RF read: garbage width/height, runaway allocation (worst — total
  failure, not just a bad image).** `struct RasterHeader` in
  `atk/raster/lib/rastfile.h` declared `Magic`/`width`/`height` as
  `long`, but the on-disk format is a fixed 14-byte header (three 4-byte
  fields + a 2-byte depth) and `oldrf.c` reads/writes it with a hardcoded
  `14`. On LP64, `long` is 8 bytes, so the struct is 32 bytes and every
  field lands at the wrong offset; `fread(&hdr, 14, ...)` only fills part
  of the `Magic`/`width` fields and never touches `height` at all, which
  is left as uninitialized stack garbage. Reading any RF file back
  (`intype=RF`/`ras`) fed that garbage straight into
  `pixelimage_Resize()`, which tried to allocate a raster of essentially
  random size — observed consuming multiple GB of RSS before being
  killed. This is the same bug *class* as LP64 variant #1/#4 (32-bit
  on-disk format vs. 64-bit `long`), not a variant already in the table.
  Fixed by giving `RasterHeader` explicit `int32_t` fields (`<stdint.h>`),
  matching the byte layout the `htonl`/`SWAL` conversions already assume.
- **RF read: first 4 and last ~3 bytes of every row corrupted.**
  `oldRF__ReadRow`'s color-inversion step hand-rolled a "process 4 bytes
  at a time" loop using `unsigned long *`, with alignment arithmetic
  (`& ~3`, `-4`) hardcoded for a 4-byte word. On LP64 each `*lx = ~*lx`
  touches 8 bytes, so the loop's per-row byte accounting is wrong in two
  ways at once: it never reaches the row's first 4 bytes (left
  un-inverted, wrong polarity) and its last 8-byte chunk overruns the
  row by 2–3 bytes into whatever memory follows (the next row, or past
  the buffer entirely on the image's last row). Confirmed empirically by
  decoding a round-tripped image and diffing it byte-for-byte against the
  original: every row differed at exactly byte columns 0–3 and 38–40 (of
  41), matching the predicted miss/overrun exactly. A second, independent
  defect rode along in the same function: a "preserve the pre-existing
  bits beyond the real image width" step read `savebyte` *before*
  `fread()`, meaning on a freshly-allocated (never-before-written)
  buffer it preserved malloc leftovers instead of anything meaningful —
  redundant besides, since the writer side already zero-pads that tail
  via `pixelimage_GetRow`'s own white-fill. Fixed by replacing the whole
  aligned-word dance with a plain byte-at-a-time complement loop over
  exactly the bytes read, and deleting the `savebyte` fix-up entirely.
  Verified with a scripted byte-for-byte diff: raster → RF → raster is
  now bit-identical to the source, and PostScript/MacPaint/Xwd/Xbitmap
  round trips (which don't go through this function) were already clean
  and remain so.
- **Crop past the image edge silently reads out of bounds.**
  `-c'(x,y,w,h)'` was never checked against the source image's actual
  width/height before `ProcessPix()` looped `pixelimage_GetRow()` over
  it; `pixelimage.c`'s own `GetRow`/`GetColumn` do no bounds checking of
  their own (by design — see the "NOT initialized" contract on
  `pixelimage_Create`). `-c'(0,0,9999,9999)` against the 321×294 test
  image produced an 18MB output raster instead of an error, reading far
  past the end of the allocated bitmap. Fixed with an explicit bounds
  check in `ProcessPix()` (`convrast.c`) that fails cleanly — matching
  the tool's existing error-handling style — for a negative origin/size
  or a crop rectangle extending past the image's right or bottom edge.

Every other documented switch/format combination worked correctly,
including three items the help file itself lists under "Bugs" that were
re-confirmed rather than newly discovered: `outtype`/`intype` really are
not inferred from file extension (an omitted `outtype` against a
`outfile=x.mp` fails rather than guessing), Sun raster really is
unsupported (`intype=sunraster` → "unknown type"; a same-named class
exists elsewhere in the tree but this program never references it), and
the "pixelimage not found" report couldn't be reproduced with `ANDREWDIR`
and the `.do` search path set up correctly — most likely an
environment/install-path issue on whatever system originally reported
it, not something reachable from here.

Fixed and rebuilt (`oldrf.do` relinked in place, `convertraster` static
binary relinked); full regression pass after all three fixes — identity
round-trip, RF/MacPaint/Xwd/Xbitmap round trips, 4×90° rotation
identity, PostScript scale factor, and a valid crop — all still pass.
Not yet committed to fossil.
