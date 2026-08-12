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

Following the `frameDot` fix above, swept for sibling "bare `-1` literal through untyped class-dispatch macro" call sites (grep pattern and full writeup in `claude-history/roadmap-old.md`; methodology also in `revival/doc/runtime-debugging-guide.md`). Narrowed ~925 raw hits to 22 candidates; confirmed 6 real bugs (receiver actually sign-checks the corrupted value) and fixed all with `(long)-1` casts:

- `content.c`/`contentv.c` (`content__Enumerate`/`content__Denumerate`): `opos`/`pos < 0` is the "enumerate everything" sentinel; corrupted value read as huge positive, silently skipping that path. 3 call sites (`content.c:649`, `contentv.c:134,186`).
- `figv.c` (`ChangeZoomProc`): `rock<0`/`rock>0` decides zoom out vs. zoom in; "Zoom Out" (menu item + `Esc-z` keybinding) passed `-1` through `menulist_AddToML`/`keymap_BindToKey`. Corrupted, Zoom Out would zoom in instead. 2 call sites (`figv.c:129-130`).
- `rasterv.c` (`ModifyCommand`): `rock == -1` selects "invert selection" ("Negative" menu item + `Esc-n` keybinding). Corrupted, Negative would silently do nothing (falls through all the `==` branches). 2 call sites (`rasterv.c:1634-1635`).

Several other candidates from the same grep sweep were confirmed harmless despite passing through the same untyped mechanism (e.g. `view_FullUpdate(...,-1,-1)` width/height args in `figv.c`/`rastvaux*.c` are ignored entirely by the receiving `FullUpdate` overrides, which recompute geometry from the view instead) — not fixed, no observable bug. A few lower-priority candidates (`rectangle_InsetRect` unprototyped-arg risk in `figv.c`'s clip-region code, `environ_GetProfileInt`/`cwp_Search` in the deprioritized messages/AMS subsystem, `tlex_RecentPosition` in the not-yet-working `ness` extension) were left untriaged — see `claude-history/roadmap-old.md`'s "Variant 3 follow-up audit" section.

**Also found, not fixed:** a live Xlib `_XLockDisplay` self-deadlock (single-thread re-entrancy triggered by `MappingNotify`/`XRefreshKeyboardMapping`), discovered incidentally while testing the figure-inset fix above. This supersedes the old "checkpoint timer UAF" theory for the `^V` scroll-hang heisenbug — see `roadmap.md`'s Heisenbugs section.

### 2026-07-04 — figure inset "messy screen": not LP64, a version-skew + error-swallowing bug

`95Summer.ez`'s figure inset (line 674) rendered as raw literal text ("`arrowpos:0 arrow:5 linestyle:0 } $ 0 117 -55287130 451 55288120 $endatt`" visible on screen) instead of a figure, and nothing after it in the document rendered either. Not an LP64 bug — confirmed via `fossil finfo` that `figattr.c`/`figattr.ch` are unmodified since initial import.

- **Root cause 1 — `figattr.c figattr__Read`**: the file's figure objects carry attributes (`arrowsize`, `arrowpos`, `arrow`, `linestyle`) that this 6.3.1 tree's `figattr_NumAttributes`/`attribute_names[]` (only 8 entries: Shade, Color, LineWidth, RRectCorner, FontSize, FontStyle, FontFamily, TextPos) never implemented — a genuine 30-year-old gap (note the unfilled `/* ##new */` placeholder in the read/write switches). The file was authored by a later/extended figure editor. `figattr__Read` returned `dataobject_BADFORMAT` on the first unrecognized attribute name.
- **Root cause 2 — `smpltext.c simpletext__HandleBegindata`**: called `dataobject_Read(newobject, file, objectid)` without checking the return value. When the figure's Read failed partway through, the file position was left mid-object-body (right after the last successfully-read line); the code pressed on regardless, registering the half-read object and resuming plain-text parsing from that misaligned position — which is exactly what dumped the rest of the figure's raw datastream (and desynced everything after it) onto the screen as literal text. Confirmed the same bug exists verbatim in the untested C++ line (`andrew-8.0/atk/text/simpletext.C:938`, `(newobject)->Read(file,objectid);` — same missing check), so no reference fix to port from there.
- **Investigated `/Users/wdc/src/AUIS/patches/official/` before writing new code**: found CMU's own `patch.633` — "Patch to V6 figure to make it accept figures created with later versions, including C++ versions 7.2 and higher" — which is the officially-sanctioned fix for root cause 1. Applied verbatim: commented out the `BADFORMAT` return in the unknown-attribute-name check, added `case figattr_NumAttributes: /* unknown attribute -- ignore */`. No official patch addresses root cause 2 (the `simpletext` error-swallowing); none exists in the C++ line either, so this hardening is new.
- **`smpltext.c` fix**: on a non-`NOREADERROR` return from `dataobject_Read`, destroy the partially-read object, create a fresh `unknown` object (the same fallback class already used when `class_NewObject(objectname)` can't find the type at all), and re-invoke `dataobject_Read` on it. `unknown__Read`'s `RealRead` scans forward honoring nested `\begindata`/`\enddata` depth until it finds the *real* `\enddata{objectname,id}`, so the parser resynchronizes and the rest of the document renders — instead of every failed/version-skewed inset silently corrupting everything after it. Verified the added dispatch calls (`dataobject_Destroy`, `class_NewObject`, the retry `dataobject_Read`) don't reintroduce the LP64 int/-1-through-untyped-dispatch pattern: no new int literals cross a dispatch boundary, `objectid` is the same pre-existing `long` variable passed through unchanged, and `dataobject_Read`'s macro (`dataobj.ih:202-203`) already casts to `long (*)()` so capturing its return in `long readret` is correctly typed.
- Rebuilt and installed `figattr.do` and `smpltext.do`. Verified in `ez`: `95Summer.ez`'s figure now renders correctly instead of raw text.

### 2026-07-04 — Patches directory audit; malloc.ci arena-size fix

Audited `patches/official/` and `patches/contrib/` in full (every file every patch touches diffed against current source) after finding `patch.633` already fixed the `95Summer.ez` figure bug above — full findings in `claude-history/roadmap-old.md`'s "Historical patches audit" section. Two outcomes:

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
above. Not investigated further this session; flagged in `claude-history/roadmap-old.md`'s
gendemo section as the next thing to chase.

### 2026-07-09 — M1 rollout begins: classpp `-pi`/`-pe` split, typed-dispatch pilots

`class.c` split `-p` into `-pi` (typed casts on the import/consumer side)
and `-pe` (typed exports) — verified byte-identical default output before
turning either on. This established the M1 rollout protocol used for the
rest of the tree: set `CLASSFLAGS = -pi` for one directory, force header
regeneration, `make Clean; make dependInstall`, fix consumer fallout,
runtime spot-check, commit. Ordered by external-consumer count (fewest
first), not directory nesting — pilots on zero-consumer leaves, core
directories last.

- **Pilot A — `atk/eq`.** Caught a macro-parameter-capture bug in classpp
  itself (positional macro args collided with type tokens under `-pi`),
  fixed structurally in the preprocessor. Also caught a ~35-year-old
  typo: `eq.ch`'s `DoScript` declared a stray `*` on a parameter the
  implementation and every caller treat as by-value.
- **Pilot B — `atk/figure`.** `MoveHandle` was typeless in the `.ch`,
  typed to match the implementation. Established the rock-idiom fix used
  throughout the rest of M1: interface params that carry an opaque
  pointer through a `long` ("rock" parameters) get retyped to `void *`.
  Found a six-file `Build(action, v)` vs. the runtime's `(v, action)`
  argument-order transposition, wrong since 1994, fixed at the
  interface. Logged a new pre-existing figure menu-focus bug (not
  fixed).
- **`atk/raster/lib`.** Zero fallout — all seven codec `.ch`s were
  already accurate. First before/after regression protocol: a
  `convertraster` battery run byte-identical to a saved pre-rollout
  baseline.
- **`atk/frame` through `atk/basics/common` (rollout points 5-9).** The
  rock-idiom retype recurred across `supportviews`, `text`, `support`,
  and `basics/common` — 16 rocks retyped `long`→`void *` across 10
  `.ch` files, ~100 call-site edits in ~50 consumer files. Two
  standalone bugs surfaced along the way: `LinkTree` was missing a
  parent-pointer declaration (`entrstrv`/`entrintv` silently passed
  garbage); `chartv`'s `CaptureString` took the address of the wrong
  variable (`*X` where `&X` was meant). `atk/support`'s `list.ch
  Enumerate` rock turned out to be genuinely dual-use — two callers
  (`dired.c`, `buttonv.c`) pass real `long` integers, ~40 others pass
  pointers — resolved by retyping to `void *` with an explicit cast at
  the two integer call sites, the first rock that wasn't cleanly one
  type or the other. `htmlview.c`'s `message_DisplayString` call had
  its arguments transposed (messages had never displayed); `clockv.c`
  was missing a prototype for a pointer-returning function, the classic
  LP64 truncation pattern.

Full batch-by-batch detail: `porting-assessment.md` §14.

### 2026-07-10 — M1 rollout point 10 (11 batches, whole tree) and point 11: default flip, M1 complete

Point 10 carried the same typed-dispatch rollout through the remaining
`atk` subtrees, `atkams`/`messages`, and `contrib`, delegated batch by
batch (one session + one gate + one runtime check per batch). Recurring
bug shape: a `.ch`'s `InitializeObject`/`FinalizeObject` typed against a
*neighboring* class's self param instead of its own — hit repeatedly
across `rofftext`, `srctext`, `utils`, and `help/src`. Standalone bugs of
note:

- `lexan.c`'s `ParseNumber` passed a `long *` where `TransEscape` takes
  `int *` — a live LP64 bug, fixed with an `int` temporary.
- `noteview`/`stroffetv` defined `ICONSTYLE`/`TITLESTYLE` as the string
  literal `"fontdesc_Plain"` instead of the symbol — a ~35-year
  copy/paste bug truncating a pointer into every note/troff inset's
  font-style `int`. Fixed to the bare symbol plus the missing
  `<fontdesc.ih>` include.
- `ams`/`amsn`/`amss.ch` used `proc`, a file-private typedef unknown
  outside its own file; classpp silently cast it to `int` under
  `gnu89`, truncating function pointers on LP64. Renamed `proc` →
  `procedure`, including in `orgv.ch` (which inherits it — classpp
  reads the *installed* parent `.ch`, so the fix needed a `make
  install` in `atk/org` too).
- `htmlview` surfaced a new pre-existing crash during this point's
  runtime check — see the overlapping-`strcpy` entry below.

**Point 11 (M1 complete):** classpp's typed-dispatch import casts
(`-pi` behavior) became the default; all 50 per-directory `CLASSFLAGS =
-pi` overrides deleted. Every live class in the default build now goes
through typed function-pointer casts — LP64 variant #2 (stack-spill) is
closed tree-wide.

Full batch-by-batch detail: `porting-assessment.md` §14.

### 2026-07-11 — Overlapping-`strcpy` crashes: nine call sites, plus a Y2K caption bug

`htmlview`'s crash from the point-10 runtime check and `bush`'s
already-logged `InitTree` crash turned out to share one root cause:
`strcpy` called with overlapping source and destination ranges (self-
referential buffer touch-ups), undefined behavior 1994-era libc
tolerated but Apple's fortified `strcpy` on arm64 does not. Found and
fixed nine sites total — `bush`, `org`, `htmlview`, `strtbl`, `label`,
`readtlx`, `calcv`, `commands` — each replaced with `memmove`. (`org`'s
crash had been mis-attributed in an earlier log entry; corrected here.)

Separately: `bldcapt.c`/`shrkdate.c` printed `tm_year` (years since
1900) straight through `%02d` instead of applying `%02d` to `tm_year %
100` in the message caption date — harmless before 2000, wrong (e.g.
"7-Jul-126") once `tm_year` exceeds 99. Fixed both call sites. While re-verifying with
`gendemo`, also staggered its synthetic posts' `Date:` headers and made
`recon.c`'s `MsgListEntry_CompareTimes` break `AMS_DATE` ties using
`AMS_ID`, for deterministic ordering once caption dates could collide at
second granularity.

### 2026-07-11 — `mkparser`/`cparser.c`: fixed-width bison table assumption

Root-caused via `amsdemo`'s caption dates rendering out of order.
`mkparser` (an awk post-processor that adapts bison's generated tables
to Andrew's shared parser engine, used by all five AUIS grammars)
assumed every bison table was a 16-bit `short`. Modern bison narrows
some tables to a single byte per grammar when the value range allows
it; `mkparser`'s generated struct initializer didn't account for the
narrower width, corrupting lookups into the narrowed tables. Fixed
generically in the shared engine rather than per grammar. All five
grammars using `mkparser` (`prsdate`, `eliy`, `eqparse`, `num`,
`parsey`) needed regenerating, since their Makefiles depend on each
grammar's own `.gra` source, not on the `mkparser` tool — `make` had no
reason to know they were stale. See `porting-assessment.md` §15.

### 2026-07-11–07-12 — zip inset enabled, typed for M1 dispatch, solid-black render fixed

`MK_ZIP` enabled in `config/site.h`, bringing `contrib/zip` into the
build for the first time. `contrib/zip/lib`'s ~21 `.ch` files were still
100% untyped 1990s K&R style — the directory had been inert, so the M1
rollout had never reached it. Typing them against the real
implementations took ~479 compile errors to zero, plus six real bugs:
missing K&R parameter declarations in three files, a pointer laundered
through a `long` rock in `zipedit.c`, a transposed-argument bug in
`zipve00.c`'s `DrawString` call, and 13 files' `Build_Object` stub
`peer` params retyped from `long`/`int` to `zip_type_figure`.

Separately, a general core-ATK bug surfaced while gating this work:
`xgraphic.c`'s Xft (anti-aliased text) drawing path never applied the
pane's GC clip to its `XftDraw`, so zoomed-in text in any Xft-rendered,
clip-relying view could bleed outside its own bounds. Fixed with a new
`xgraphic_GetClipBoundingRect` helper mirroring the existing clip
computation; required relinking `libbasics.a`/`runapp` (statically
linked).

zip then rendered as a solid black rectangle at default optimization
(correct at `-O0`). Root cause, found the next day: a classpp
typed-dispatch signedness mismatch — `zip.ch` declared
`Superior_Image_Line_Width` as returning `char` where the implementation
returns `unsigned char`. One-line `.ch` fix plus classpp regeneration; a
tree-wide scan of all 566 `.ch` files found no other instance of the
same mismatch. Same session also fixed a `symtab_add` LP64 NULL check
and a `Line_Dash` uninitialized-output bug. See `porting-assessment.md`
§16.

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

**Backfill complete as of 2026-08-09.** The 07-09–08-07 entries above
(the M1 rollout tail, AMS-over-IMAP through milestone 4,
folder-visibility, mime-display, fdplumb, `-fwritable-strings`, M2
completion, all of M3, and all of M4 including the strict-prototypes
side quest and the final global-strictness flip) were backfilled from
`fossil sql` commit timestamps, `claude-history/roadmap-old.md`, `porting-assessment.md`,
this project's own memory notes, and `claude-history/*-REPORT.md`. This
log now has continuous coverage from the 2026-06-24 bootstrap through
M4's completion.

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

### 2026-07-25 — M2 buckets 4b-4h: implicit-declaration rollout continues, malloc/realloc/free blind spot found

Continuing the `-Wincompatible-pointer-types`/implicit-declaration
sweep begun 07-24: `overhead/mail/metamail/metamail` (338 instances, 7
files — far past the stale estimate of 70), `atk/text` (156, 50
census-visible + 106 more from a directory-wide blind-spot sweep, see
below), `atk/rofftext` (104), `atk/table` (186), `overhead/mail/lib`
(124, the widest-fan-out directory examined yet — `libmail.a`, ~25
consumer directories), `atkams/messages/lib` (336, the `messages` app's
actual backend, required a tree-wide gate), `contrib/zip/lib` (256, the
tree's highest-defect-density directory, also tree-wide-gated). Each
directory got its own delegated session, subtree-local gate first, then
(for the two directories with cross-tree reach) a full tree-wide
`make Clean && make dependInstall` — same 4 pre-existing baseline
errors every M2 gate has documented, zero new ones anywhere.

**Methodology finding, `atk/text`:** `malloc`/`realloc`/`free` are
clang builtins, so an undeclared call to one of them produces no
warning at all — invisible to the `-Wincompatible-pointer-types` census
that drives the rest of M2. A directory-wide grep sweep for the three
names, added as a mandatory step for every remaining M2 directory,
found 106 more call sites in `atk/text` alone; every bucket-4 directory
after this one reports two counts (census-visible vs. real total)
because of it. `atk/table`'s sweep also caught a second-order gap: a
naive substring grep on `realloc` false-positived on `table.c`'s own
`myrealloc()` wrapper — the sweep pattern was tightened to be
word-boundary-anchored.

**Real bugs found during runtime verification, not part of the
declaration fixes themselves:**
- `metamail`'s non-functionality (previously known only as "launches,
  displays nothing") was root-caused: a plain `text/plain` body run
  directly through metamail crashes with a Bus error before any
  display happens. Under `lldb`, the actual first-hit signal is
  `SIGTTOU` (terminal job-control), stopped inside
  `ExecuteMailcapEntry`'s `ioctl()` call (`metamail.c`) — 1980s/90s BSD
  job-control code that doesn't survive contact with modern macOS
  process-group/terminal semantics. Root cause identified, not fixed —
  metamail was already a known non-functional side quest; this gives
  whoever picks it up a starting point instead of the mailcap-execution
  code in general.
- `contrib/zip/lib`'s directory logged two new, confirmed-pre-existing
  findings for dedicated follow-up: zip/calc insets fail to load when
  embedded in a mixed-content document (later root-caused and fixed
  2026-07-26, an LP64 `%d`/`%ld` id-truncation bug — see
  `porting-assessment.md` #21), and `ez` horizontal text-block drag
  locks at position 0 after the first drag.

`atkams/messages/lib` and `contrib/zip/lib` each also surfaced a
concrete LP64 mixed-width finding (functions that are `long`-returning
at their real definition while sibling functions in the same family
default to `int`) — logged for the tree-wide variant-#6 audit, not
independently fixed this session.

### 2026-07-25 — M2 point 5: `ams/libs/ms` (1569 instances) closes M2 tree-wide

`ams/libs/ms` — 114 `.c` files, ~38% of the original tree-wide census,
by far the largest single directory in the sweep — was done as its own
dedicated session. 1567 census-visible instances (against a stale
estimate of 892) plus 2 more from the malloc-blind-spot sweep, real
total 1569 across 102 files. Both the subtree-local and full tree-wide
gates were required and clean; the tree-wide gate doubles as **M2's own
completion gate**.

Confirmed the rollout's own "a handful of functions dominate" working
assumption: the top 10 functions account for 45.6% of all 1569
instances. The single largest contributor was the `fdplumb.h`
partial-wrapper-family gap (300 instances on its own). Two more
concrete LP64 mixed-width findings (`conv64tolong`, `KRHash`, both
`unsigned long`).

**M2 complete tree-wide:** 3888 implicit-declaration/incompatible-
pointer-type instances fixed across 29 directories, in total across
07-24 and 07-25. M2's working prompts, per-directory reports, and
rollout runbook (29 files) were retired into
`claude-history/m2/`, with cross-references repointed.

### 2026-07-25 — M3 begins: `ansify` DRIFT false-positive on classpp's own `InitializeObject`/`FinalizeObject` convention

M3's first concrete step — a tree-wide `ansify --dry-run` census before
any conversion batch ran — found 56 DRIFT findings (`.ch` argument
count disagreeing with the real implementation's) across 1,486 files.
42 of the 56 name exactly `InitializeClass`, `InitializeObject`, or
`FinalizeObject`, and turned out to share one tool-side false positive,
not 42 separate bugs: classpp special-cases these three classprocs.
`InitializeObject`/`FinalizeObject` always get a hardcoded 2-argument
prototype (`classID` and `self`) regardless of what the `.ch` declares
(`class.c:1122`); `InitializeClass` is dispatched through a fully
untyped cast with no compiler-enforced argument count at all. `ansify`'s
DRIFT check assumed the universal one-implicit-parameter convention
that holds for every other class method, so it misfires in either
direction depending on how the `.ch` happens to be written (an extra
expected argument if the `.ch` restates the implicit param by name; one
too few if it uses empty parens). Verified end-to-end (`.ch`, `.c`, and
the `class.c` codegen source all cross-checked) against two instances
(`foldertreev`, `atkams/messages/lib/fldtreev.c`; `suite`,
`atk/apt/suite/suite.c`); the remaining ~39 share the identical
message shape and are almost certainly the same mechanism. See
`porting-assessment.md` #20. Same day, M3's directory batch plan
(`overhead/util/lib`, 58 files) started separately as O1.

### 2026-07-30 — M3 Waves 1-2: `ansify` K&R→ANSI conversion resumes; classpp `FinalizeObject` `-pe` self-inconsistency

M3 converts every K&R function *definition* tree-wide to real ANSI
argument types (M1 only retyped class-dispatch call sites; M3 retypes
the definitions themselves, using the `ansify` driver built for the
abandoned June attempt, see `porting-assessment.md` §14). Batch B1
(`atk/basics/common`, 41 classes/45 files) opened the `-pe`/`.eh`
rollout (typed *exports*, the counterpart to M1's typed imports) at
scale for the first time; found a stray `*cmap` dereference bug in
`im.c` along the way. Batch B2 (`atk/value`+`support`+`supportviews`+
`adew`+`basics/x`, 81 classes/93 files) surfaced a real classpp
codegen bug: `FinalizeObject`'s exported `-pe` prototype follows the
*ordinary* classproc emission path (whatever the `.ch` declares), but
the internally generated call site inside `__Finalize` is unconditionally
hardcoded to pass 2 arguments (`classID`, `self`) regardless — so an
empty-parens `FinalizeObject()` `.ch` declaration became self-inconsistent
with its own generated `.eh` the moment `-pe` turned on. Fixed in
classpp itself, verified as zero retroactive effect on the 7 directories
already using `-pe` (byte-identical `.eh` regeneration). The same batch
also found 6 separate ~35-year-old copy/paste typos — a wrong sibling
class name substituted for the true enclosing class in a restated
`InitializeObject`/`FinalizeObject` parameter type — invisible until
`-pe` actually type-checked them.

Also this window: `ansify` had a silent parser gap on "brace-glued"
K&R parameter declarations (a function's opening `{` on the same line
as its last parameter's type, with no whitespace separating them) —
first found and fixed as a silent-skip bug, then found to have a second
half, a corruption-on-reject defect discovered while fixing the first;
both fixed, plus a retrospective re-check across the 7 `-pe` directories
already converted (20 files needed re-conversion once the gap was
closed). Batch B3 (13 leaf directories, 61 files, closes Wave 2) hit
the gap's third variant — brace-glued to a *local variable* declaration
inside a function body, not a parameter — plus its own crop of drift
bugs: an `InitializeClass`/`FinalizeObject` restated-parameter classpp
bug, a missing parameter in `tree.ch`, a `SetHitHandler` `.ch`/`.c`
mismatch, a struct-tag scope trap, and 4 more rock-idiom instances.
Full per-batch detail: `claude-history/m3/m3-b1-basics-common-REPORT.md`,
`m3-b2-value-support-REPORT.md`, `m3-b3-leaf-dirs-REPORT.md`.

### 2026-07-30–07-31 — M3 Wave 3: `atk/text` (30 files/21 classes)

Closes Wave 3. Found a `textv.ch` `ViewMove` parameter typed to the
wrong type, a `textv.c` `HandleSelection` call passing a stray extra
argument, and 3 stranded forward declarations left over from earlier
rollouts. Full detail: `claude-history/m3/m3-t1-atk-text-REPORT.md`.

### 2026-07-31 — M3 Wave 4: 10 inset-adjacent directories, then eq/figure/chart/table/rofftext/raster-cmd

I1 (`atk/image`, `srctext`, `raster/lib`, `layout`, `hyplink`, `org`,
`bush`, `raster/scan`, `fad`, `raster/convert`, 70 files) hit four
`ansify` tool bugs along the way — a case-insensitive filename
collision in the signature database (`sliderv`/`sliderV`, refused
rather than silently overwritten), `fix-missing-static-decl` inserting
a duplicate empty-parens declaration for an already-fully-prototyped
function, a bracket-placement bug in array-parameter formatting
(classpp's `-D` output emits `T [ ]` with a space `ansify` didn't
expect), and a two-line return-type declaration being duplicated when
`ansify`'s `TYPEONLY` handling met a trailing comment. Real bugs: a
`cmapv.ch` self-type mismatch, a stray argument in `pbm.c`, and two
covariant `SetDataObject` override mismatches (`asmtextv.ch`,
`srctextv.ch`). Runtime-confirmed clean, including a fixed `bush`
leaf-node rendering regression caught during verification.

I2 (`eq`, `figure`, `chart`, `table`, `rofftext`, `raster/cmd`) closed
out the wave. Separately, `atk/chart` needed an M2-era `COMPILERFLAGS`
guard added (it had never gotten one) — closing it surfaced 10
cross-file implicit-declaration gaps and one dual-use-attribute cast.
Full detail: `claude-history/m3/m3-i1-insets-batch1-REPORT.md`, `m3-i2-insets-batch2-REPORT.md`.

### 2026-07-31 — M3 Wave 5: 9 app/leaf directories (A1)

`ansify` gained a fifth tool fix here: a signature-database collision
between two same-named classes, one live and one in a dead subtree,
where the dead one was silently winning — refused rather than silently
overwritten, same defensive pattern as the I1 filename collision. Real
bug: a live LP64 pointer-truncation crash in `cvEng()`
(`ams/libs/shr/utils.c`, returns `char *`) — called from `cuifns.c`'s
`GetDirInfo`/`dirinfo` command with no declaration anywhere in
`ams/msclients/cui` (this directory had never gotten the M2-era
`COMPILERFLAGS` guard either), so it silently implicit-declared as
`int cvEng()`, truncating the real pointer on arm64. Only call site
tree-wide reaching `cvEng`; every other implicitly-declared function in
the directory turned out to genuinely return `int`/`void`/`boolean`
(checked systematically, all ~140). Fixed with an explicit
`extern char *cvEng();`, matching the declaration style
`ams/libs/cui/cuilib.c` already used for the same function.
Runtime-confirmed clean after the fix. Full detail:
`claude-history/m3/m3-a1-apps-REPORT.md`.

### 2026-08-01 — M3 Wave 6: `ams/libs/ms` (113 files) and `messages/lib`; `FreeMessageContents` — a real ~30-year-old arg-count bug

AMS1 converted `ams/libs/ms` (Wave 6's largest directory) and closed a
COMPILERFLAGS gap in `ams/msclients/cui` left over from Wave 5. Giving
`FreeMessageContents` its real 2-argument prototype
(`struct MS_Message *Msg, Boolean FreeSnapshot`) surfaced a genuine bug
invisible under K&R's unspecified-argument-count declarations:
`unscrib.c`'s `UnformatMessage` called it with only one argument — the
only call site tree-wide missing the second. Examining the function
body: `FreeSnapshot` controls only whether `Msg->Snapshot` is freed;
`UnformatMessage` reformats a message in place and reuses the `Msg`
struct afterward, so freeing the snapshot here looks semantically
wrong, but the pre-existing call's real second-argument value was
whatever garbage happened to be in that register — genuinely undefined
behavior, not a reliable default. Fixed conservatively as
`FreeMessageContents(Msg, FALSE)` (don't free the snapshot — a stale
snapshot is a smaller failure mode than a use-after-free) and flagged
for a human ruling rather than presented as unambiguous; wdc confirmed
the choice the same day.

AMS2 converted `messages/lib` plus 3 more AMS libraries, closing Wave
6. Full detail: `claude-history/m3/m3-ams1-REPORT.md`,
`m3-ams2-REPORT.md`.

### 2026-08-01 — M3 Wave 7 closes M3 tree-wide: `contrib/zip/lib` and 11 more contrib directories

C1 converted `contrib/zip/lib`; C2 converted the remaining 11 contrib
directories, closing both Wave 7 and M3 itself. C2 resolved the
`contrib/zip/utility/ltapp.c` `Set_Debug` blocker logged back on
07-11 (an untyped-`.ch`-vs-`boolean`-argument mismatch in
`contrib/zip/lib`, which had blocked a full top-to-bottom gate ever
since) and found a new bug: `EnvStart`/`EnvEnd` argument handling.

**M3 complete (2026-08-01):** every K&R function definition tree-wide
now carries real ANSI argument types — 15 sessions, all 91 active
directories. M3's prompts, per-directory reports, rollout runbook, and
batch map (38 files) were retired into `claude-history/m3/`, with
cross-references repointed.

### 2026-08-01–08-02 — M4 begins: Phase 0 pre-flip audit, `strict-prototypes` dropped from the global flip, Batch 0

M4 is the final ANSI-conversion milestone: flip the compiler's
strictness flags (`implicit-int`, `implicit-function-declaration`,
`int-conversion`, `incompatible-function-pointer-types`, `format`) to
`-Werror` by default, directory by directory, after M1-M3 already
retyped the dispatch layer and the definitions underneath it. Phase 0
audited the known risks M3 had flagged for follow-up — all closed
except one new finding that changed the plan: `-Wstrict-prototypes`
doesn't only flag leftover K&R-style function *definitions* (a small,
already-known residual list); it also flags every plain empty-parens
`extern int foo();` **declaration** — a C89-legal "unspecified
arguments" idiom this codebase uses extensively and deliberately (M2
and M3 both picked it on purpose in places, e.g. AMS1's own
`moreprintf`/`errprintf2` fix). A declaration-shaped grep found ~6,024
such declarations tree-wide, ~1,055 inside installed headers alone;
compiling a real file (`atk/value/entrtext.c`) with
`-Werror=strict-prototypes` failed immediately from `class.h`'s own
`extern int class_EnterInfo();` — a foundational header included
nearly everywhere. There's no compiler-flag granularity separating
"leftover K&R definition" from "deliberate unspecified-args
declaration"; clang raises the identical diagnostic for both. Dropped
`strict-prototypes` from the global flip (the other four flags all
checked clean the same way, on three files across three risk tiers).

Batch map: a real Phase 1 census (not a pre-built guess, unlike M2/M3)
found 1,778 errors across 83 of 91 directories once the full flag set
was actually applied — the "might land close to clean" hope from the
original plan was disproved; 24 batches across 7 waves were mapped
from it. Execution mechanism: per-batch `COMPILERFLAGS` overrides
(mirroring M2's approach) via a new `STRICT_COMPILERFLAGS` macro in
`system.mcr`, global flip deferred to a final cleanup step — keeps the
tree buildable and pausable throughout. Batch 0 (2026-08-02) verified
the mechanism itself: reapplied a classpp fix (`stdlib.h`,
`PushFile`/`PopFile` externs, `pathopen`'s return type), fixed
`-Werror=format` findings including an LP64 id-truncation bug, and
added `STRICT_COMPILERFLAGS` to `system.mcr`.

### 2026-08-02 — M4 Wave 1: `overhead/mail` (richmail, eli, metamail), bison/index/util/misc

O1 (`overhead/mail/metamail/richmail`) fixed an implicit-int cleanup
plus a live `sprintf` format-string bug. O2 (`eli/lib`+`bglisp`,
`mail/lib`+`cmd`+`testing`, `metamail`) fixed two more format-string
injection bugs and a `UnixError` `%d`-format bug. O3 (bison, index,
util, misc leaf directories, closes Wave 1) found `overhead/bison/
files.c`'s `AndrewDir()` — a pointer-returning function invoked via
the `XPFILE`/`XPFILE1` macros with no declaration anywhere in
scope — the same LP64 pointer-truncation class fixed repeatedly
elsewhere in the tree, a separate call site from the one already fixed
earlier in the project. Fixed with `extern char *AndrewDir();`. Also:
`overhead/index`'s on-disk hash-bucket (`H%d`) and version (`V%d.%d`)
filenames were built/parsed with `%d`/`sscanf %d` against `long`
fields, fixed to `%ld`; `tif_print.c` had 11 `%u`-vs-`u_long`
mismatches in its debug-only `TIFFPrintDirectory` path;
`overhead/addalias/addalias.c`'s `qsLineCompare` was rewritten to a
real `(const void *, const void *)` qsort comparator instead of a bare
cast. `overhead/bison` confirmed first-party-maintained (the Andrew
Consortium's own fork, not vendored upstream) — fixed like any other
directory, its bootstrap-generated `lex.c`/`gram.c` needed no changes.

### 2026-08-02 — M4 Wave 2: `atk/apt/tree`, then 9 more directories, then 9 more — closes Wave 2

B1 (`atk/apt/tree`) removed a dead duplicate K&R declaration block and
fixed 42 implicit-int instances, 7 LP64 `%ld` format fixes, and 3
vtable dispatch casts. B2 (`adew`, `apps`, `apt/suite`,
`basics/{common,x,lib}`, `syntax/{tlex,parse,sym}`, `utils`) fixed 5
real LP64/signal-handler bugs. B3 (`value`, `apt/apt`, `textaux`,
`supportviews`, `support`, `lookz`, `textobjects`, `extensions`,
`frame`, closes Wave 2) fixed 11 real LP64/signal-handler/writable-
string bugs. None individually escalated to a human ruling; full
per-directory detail in the M4 batch/wave working files
(`claude-history/m4-batches.md`, `claude-history/m4-rollout-runbook.md`).

### 2026-08-03 — M4 Wave 3: `atk/text` closes Wave 3

`atk/text` alone under `STRICT_COMPILERFLAGS` (the tree-wide flip to
`-Werror` on implicit-int, incompatible pointer/function-pointer types,
and format strings, batched directory by directory — M4's whole
purpose, after M1-M3 already retyped the dispatch layer and the K&R
definitions underneath it). Real error count (102) ran 79% over the
pre-flag census (57), almost entirely because the census predated the
`-Werror=format` ruling: 45 of the 102 were format errors, concentrated
in `txttroff.c`'s ~2000-line troff-stream writer. Two standalone bugs:
`content.c`'s `erestingstyle()` forward declaration was a ~30-year-old
typo that never matched the real function `interestingstyle`, silently
unused; `be1be2a.c` (the standalone BE1→BE2 CLI converter) had two
broken diagnostics, one printing a filename via `%d`, one missing its
`progName` argument entirely.

### 2026-08-06 — M4 Wave 4: `raster/cmd`, `table`, `figure`+`chart`, 12 leaf directories — closes Wave 4, largest datastream-bug haul so far

I1 (`atk/raster/cmd`) found the wave's highest-impact bug:
`raster__WriteShare` silently dropped the `height` field from its
shared-memory datastream write (7 `%ld` conversions for 8 arguments),
corrupting the next line's parse for every reader — the exact class
the `-Werror=format` ruling exists to catch, since AUIS uses `printf`
as a real serialization path, not just display. I2 (`atk/table`) was
mostly debug-trace-only, plus one signal-handler dead-branch bug in
`eval.c` (same root cause as earlier waves' `im.c`/`framecmd.c`
findings — a `SIGFPE` handler compiled under a guard macro that's
never actually defined). I3 (`atk/figure` + `atk/chart`, flagged as a
risk batch for its history) earned that flag: 11 genuine on-disk
datastream bugs, the largest single-batch haul of the milestone.
Highest-impact: `figobj__WriteBody` — the **base class** inherited by
nearly every figure object type (rect, text, polyline, ...) — truncated
the `x`/`y` position fields on every save. Also: `figure__Write`'s
`$origin` field was still `%d %d` on the *write* side though the
*read* side had already been fixed in an earlier milestone (see
`porting-assessment.md`'s figure-inset history) — a reminder to
re-check both directions of a read/write pair even on findings already
marked fixed. `atk/chart`'s `Parse_Item_Field` read a chart item's
value into an **uninitialized `long`** via `sscanf %d` — real memory
corruption on read, not just truncation. I4 (12 small/leaf directories,
closes Wave 4) found 4 more: `atk/raster/lib/rasterio.c`'s shared
raster-datastream reader read the `long options` field with `%u`;
`atk/hyplink/link.c` wrote `pos`/`len` with `%d`; `atk/raster/convert/
convrast.c`'s own crop-option CLI parser read four `long` locals with
`%d`. **Wave 4 checkpoint**: wdc ran a full `make Clean; make World`
plus a broad smoke pass — no regressions from this wave's own fixes,
but found and logged two new pre-existing bugs: `contentv` (Table of
Contents) shows nothing for documents using enumerated/auto-numbered
heading styles, and `convertraster`'s crop option (this wave's own fix
target) now parses correctly but produces a blank cropped image.

### 2026-08-06 — M4 Wave 5: 8 app/tool directories closes Wave 5

`ams/msclients/imapsync`, `ams/msclients/cui`, `atk/typescript`,
`atk/help/src`, `doc/mkbrowse`, `atk/help/maint`, `atk/ez`,
`ams/msclients/nns`. A shift from Wave 4's datastream-corruption
pattern to UI/diagnostic-text bugs, since this batch is mostly
CLI/UI-facing code, not file-format code: `cui.c`'s epoch
delete-confirmation prompt dropped the seconds field from its message;
`mkindex.c` had two diagnostics' `code`/`lineNo` fields swapped
relative to their own labels; `helpa.c`'s duplicate-index error path
dropped the actual offending switch name from its error text; `cui.c`'s
`SubscriptionChangeHook` was declared as a bare 0-argument stub despite
every real call site passing 4 arguments (tolerated only by K&R's
no-arity-check convention). Also the third, fourth, and fifth
recurrence of the "no header declares this" pattern first found in
Batch 0: several files (`imap_sync.c`, `tscript.c`, `eza.c`,
`mkbrowse/browser.c`) called functions with zero declaration anywhere
in scope, invisible until this wave's stricter flags.

### 2026-08-06 — M4 Wave 6: `ams/libs/ms`, `messages/lib`, `ams/libs/{cui,nosnap,shr}` closes Wave 6

AMS1 (`ams/libs/ms` alone, the tree's worst prior history of silent
width-mismatch bugs) found: `epoch.c`'s local `extern int
DescribeTimeInterval()` disagreed with the real function's `char *`
return (LP64 pointer-truncation pattern #1, live in the
single-old-message deletion path); `subs.c`'s empty-subscription-name
recovery branch had a duplicated `fprintf` argument, misaligning every
later conversion in the message (a pointer printed via `%d`, an `int`
dereferenced as `%s`); and `init.c`'s `DieYouHeathenSwine` signal
handler was typed to return `int` against POSIX `signal()`'s real
`void(*)(int)` contract. AMS2 (`atkams/messages/lib`, `messages`' own
GUI backend, tree-wide gate required) found `stubs.c`'s `SnarfFile`
diagnostic dropped its filename argument entirely from a 2-conversion
format string. AMS3 (`ams/libs/{cui,nosnap,shr}`, closes Wave 6) found
`cuilib.c`'s `CUI_SetPrinter` catch-all failure path had zero `%`
conversions despite `printername` being passed as an argument — the
printer name was silently missing from the user-visible error. Wave 6
checkpoint (`make Clean; make World` plus a smoke pass) ran clean.

### 2026-08-06–08-07 — M4 Wave 7 closes M4's directory rollout: `contrib/zip/lib`, `contrib/zip/utility`, 9 small contrib directories

C1 (`contrib/zip/lib` alone, the tree's known highest-defect-density
directory, tree-wide gate required) found `zipds02.c`'s figure-mode
attribute writer built three one-character flag strings
(horizontal/vertical/halo) but only ever printed two — the missing
`halo` conversion confirmed live against the reader side, which
explicitly parses a third character. Fixing it exposed a **second,
previously dormant bug** the same day, found by wdc's own testing:
the three flag buffers were `static char foo[2]="?"` sentinel scratch,
only ever overwritten when their flag was on and never reset when off
— since `halo` had never actually been printed before the first fix,
its stale `"?"` was invisible; the moment it started printing, every
figure with halo off got a literal `?` appended to its Mode line,
corrupting the datastream for every inset after it (confirmed live via
`revival/testing.ez`). Fixed by dropping `static` and initializing to
`""` — 1988 K&R never allowed automatic-variable initializers, so
`static` was likely there only to make the `="?"` initializer legal in
the first place, not for intentional persistence; C89 allows a plain
local initializer directly, giving the fresh-every-call behavior the
code seems to have always wanted.

C2 (`contrib/zip/utility`: `lt`/`ltapp`/`ltv`, `sched`/`schedapp`/
`schedv`) found `sched.c`'s "Unable to Open" diagnostic printed the
`struct zip_stream *` pointer itself instead of the actual filename
field — every failed-open message for the `sched` class has shown a
garbage/unrelated value since the file was written. This batch also
resolved the `ltapp.c` `Set_Debug` blocker first logged back on
07-11 (an untyped-`.ch`-vs-`boolean` mismatch that had blocked a full
top-to-bottom gate ever since). C3 (9 small contrib directories, closes
Wave 7 and all 18 sessions/7 waves of M4's directory-by-directory
rollout) found one more LP64 memory-corruption bug:
`contrib/mit/annot/psview.c`'s PostScript bounding-box parser read four
`long` fields with `sscanf %d`, corrupting both the on-screen inset
size and a `translate` command written back into the document.

### 2026-08-07 — Strict-prototypes side quest: tree-wide census, pilot, mechanical retype, and triage

Found while diagnosing a zip figure-drag X-axis-lock bug (a stale
untyped `Set_Constraints` forward declaration, fixed the same day, not
itself part of the M4 wave sequence): a static helper's
forward declaration can remain old-style/argument-less
(`static int Foo();`) even after its real definition is fully
ANSI-typed, if an earlier conversion pass retyped the definition but
never reconciled a separate forward declaration above it in the same
file — legal ANSI C, invisible to every M1-M4 diagnostic, since
`-Werror=int-conversion`/`incompatible-function-pointer-types` only
fire when a real prototype is in scope to compare a call against, and
an argument-less declaration isn't one.

A diagnostic-only tree-wide `-Wstrict-prototypes` sizing pass (not
`-Werror`, nothing committed to any Imakefile) found 6,715 hits
anchored in real project `.c` files across all 91 directories, tracking
M4's own worst-defect-density directories closely (`ams/libs/ms` 913,
`atkams/messages/lib` 529, `contrib/zip/lib` 368, `atk/text` 359). A
pilot on `contrib/calc` established that a hit means only "no real
prototype in scope," not "definitely broken" — of 10 census hits there,
6 were real local declarations (retyped, zero actual bugs found once
rebuilt against `-Wincompatible-pointer-types`) and 4 were classpp-
generated-header noise or deliberate generic dispatch, not fixable or
not bugs.

A tree-wide mechanical pass (77 remaining directories, ~4,600 stale
forward declarations retyped across 508 files, nothing else changed,
fixing zero bugs by design — just making every real signature mismatch
visible to the compiler) found a handful of confirmed real bugs, then
fixed them as a separate triage step: `ams/msclients/cui/cui.c`'s
`MS_GetPartialFile`/`MS_GetPartialBody` passed `&bytesunfetched`
(`long`) for an `int *remaining` out-param at 6 call sites (an LP64
truncation bug, plus a 7th sibling bug found only once the rebuild
after the first fix exposed it — `MS_ProcessNewMessages`'s
`FirstError`); `ams/libs/ms/reply.c`'s `PrintFwdHeaders` was defined
with `int fp` where every real caller passes a `FILE *`; 9 more
confirmed argument-count/type mismatches (missing or extra arguments,
mostly copy-paste drift) across `bldcapt.c`, `mswp.c`/`submsg.c`,
`zipdi00.c`, `txtvcmsc.c`, `table.c`, `rastvauy.c`, `nns.c`, `fdbbdf.c`;
`atk/image/ps.c`'s `epsPreview` turned out to be genuinely dead code
(its only call site was permanently `if (FALSE)`-gated with arguments
that didn't even match the real signature) — deleted rather than
fixed; `contrib/zip/lib/zipve02.c`'s `Create_Shade_Palette` had its
`containing_pane` parameter typed `int` where all 8 sibling
palette-creation functions declare it `zip_type_pane`. All fixed and
verified: every touched directory rebuilt clean individually, then a
full tree-wide `make dependInstall`; wdc functionally verified the two
highest-value fixes live (browsing the real IMAP INBOX, which exercises
`MS_GetPartialFile`/`PrintFwdHeaders`/`BuildDateField`; the zip shade
palette rendering correctly on save/reload).

### 2026-08-07 — M4 complete: `STRICT_COMPILERFLAGS` becomes the tree-wide default

With the directory-by-directory rollout and the strict-prototypes side
quest both done, `system.mcr`'s default `COMPILERFLAGS` was flipped to
`STRICT_COMPILERFLAGS` and the 83 now-redundant per-directory Imakefile
overrides removed. One last bug surfaced by the flip itself, in a
directory that had never been individually strict-flagged before:
`overhead/class/lib/mapping.c`'s `DumpMappingInfo` had a `%08x`/
`unsigned long` format mismatch, caught by its first-ever strict
compile. **M4 complete** — every one of the 91 active directories now
builds under full compiler strictness (implicit-int, incompatible
pointer/function-pointer types, and format strings all `-Werror`;
`strict-prototypes` deliberately excluded from the global flip — see
the 2026-08-01 M4-begins entry above for why). This closes the ANSI C conversion
plan begun with M1 on 2026-07-08: four milestones, dispatch-layer
typing through full compiler strictness, dozens of decades-old bugs
found and fixed along the way. Full per-wave/per-batch detail:
`claude-history/m4-batches.md`, `claude-history/m4-rollout-runbook.md`;
strict-prototypes detail: `revival/doc/strict-prototypes-census.md`,
`claude-history/strict-prototypes-retype-REPORT.md`.

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
Committed (`fb799285e2`).

### 2026-08-08 — image inset: JPEG/TIFF import, four more bugs, root cause found and fixed live

Follow-on same-day session, picking up the `image` inset's long-open
"JPEG/GIF import renders solid black, TIFF renders solid white"
bug (`claude-history/roadmap-old.md`, found 2026-07-26, never root-caused). Same
diagnostic method as the `convertraster` entry above: an independent
standalone test harness (`class_Init` + `class_NewObject("jpeg"/"tif")`
+ `image_Load`, bypassing X11 entirely) to isolate decode correctness
from display correctness, using real test files (`revival/tests/
netmap.jpg`, `netmap.tiff`) rather than the tiny synthetic ones the
2026-07-26 investigation had on hand.

**JPEG decode: already correct.** The harness showed `jpeg.c`'s decoder
producing a correct `ITRUE` image — right dimensions, real varying
pixel data — on the first try. Whatever caused the original "solid
black" report, it wasn't the JPEG decoder itself.

**TIFF import: totally broken, four bugs, all the same LP64 struct/
stride family as `porting-assessment.md` §22 (`convertraster`'s
`RasterHeader`), just in the vendored `libtiff` this time:**
- `TIFFHeader` (`tiff.h`) declared its `tiff_diroff` field `unsigned
  long`; on-disk it's a 4-byte TIFF LONG. Doubled the struct to 16
  bytes against an 8-byte real header, misreading the initial
  directory offset from the wrong file bytes. `TIFFOpen` failed
  outright — "Can not read TIFF directory count" — for every TIFF
  tried, including a minimal uncompressed one.
- `TIFFDirEntry` had the identical problem (`tdir_count`/`tdir_offset`
  both `unsigned long` against a 12-byte real entry), corrupting every
  directory-entry read once the header was fixed.
- `TIFFSwabArrayOfLong` (`tif_swab.c`) correctly swaps 4 bytes per
  element but advances its pointer by `sizeof(unsigned long)` (8 on
  LP64) between elements — every call with n>1 swaps the right bytes
  once, then corrupts whatever memory follows. One call site
  (`&dp->tdir_count, 2`) was swapping half of every directory entry's
  count/offset pair and stomping the *next* entry's tag/type with
  garbage — the source of a wall of "unknown field"/"wrong data type"
  warnings once the two struct fixes above let the file open at all.
- `TIFFFetchLongArray` read disk-packed 4-byte LONGs directly into the
  caller's native `u_long v[]` (8 bytes/slot on LP64) — used for
  `StripOffsets`/`StripByteCounts`, so every strip's file offset came
  out as two real values jammed into one 64-bit slot. This is what
  actually blocked pixel data: `TIFFReadEncodedStrip` failed
  ("Read error at scanline -1") even after the file opened cleanly.
  Fixed the same way the pre-existing short→long expansion case in
  `TIFFFetchStripThing` already did it: stage into a real `uint32_t`
  buffer, then widen each element into the destination.

All four fixed, `libtiff.a`/`tif.do` rebuilt; confirmed via the harness
that TIFF now decodes identically to JPEG — real varying pixel data,
survives `image_Duplicate`/`image_Compress` intact.

**The actual "solid color" bug: `xgraphic.c`, nothing to do with LP64
at all.** With both decoders proven correct, a live test (embedding
each file as a real `image`/`imagev` inset datastream, `xwd`-capturing
the actual `ez` window — `ImageMagick` has no xwd decoder, converted
by hand from the raw XWD format instead) still showed a solid color
fill, confirming the bug was purely in on-screen rendering. Root cause
in `imageToXImage` (`atk/basics/x/xgraphic.c`)'s TrueColor/DirectColor
color-allocation loop: it calls `xcolormap_AllocColor(...)` and stores
the live result in `xc`, but fills the `redvalue[]`/`greenvalue[]`/
`bluevalue[]` lookup tables (used to translate every decoded pixel to
its on-screen value) by reading `xcolor.pixel` — a *different*, merely
flag-initialized local variable whose `.pixel` field is never written
anywhere in this function. Every one of the 256 entries in all three
tables ends up holding the same uninitialized stack garbage, so every
real pixel — regardless of its actual decoded color — looks up to the
same output value: a solid fill, whichever color that garbage decoded
to (matching the historically-inconsistent reports of black, white,
and, live in this session, blue). Fixed by reading `xcolor_Pixel(xc)`
instead, the same accessor already used correctly a few lines away
in the PseudoColor branch of the same function. Not LP64-related —
a plain pre-existing variable mix-up, apparently never exercised
until this was the first time anyone tried displaying a real photo
through this port.

`xgraphic.o` rebuilt, `libbasics.a` and static `runapp` relinked
(wdc installed the new binary; a live `ez` window from before the
relink still showed the old broken behavior, confirming `.do`/binary
files cache for a process's lifetime as documented elsewhere in this
project). Confirmed live, both formats, fresh `ez` process: JPEG and
TIFF versions of `netmap` both render the actual photographed page
content, correctly.

### 2026-08-09 — `cui` gains MIME body display; a matching `metamail` bug found and fixed

**Symptom:** `cuin`, reading a real IMAP-mirrored INBOX, could not
`type` the very first message in it — `metamail: Could not find end of
mail headers`, no body shown, for any message whose top-level
Content-Type wasn't exactly `text/plain; charset=us-ascii` (i.e. most
real mail: multipart, HTML-only, or any other charset). `cui` had never
been given MIME awareness in the earlier `messages`/`text822.c` MIME
work (see `claude-history/mime-display-REPORT.md`) — only relinked
against the new `libmsshr.a` so it wouldn't go stale, never wired to
call `mimepart_*` itself.

**Fix:** `GetBodyFromCUID()` (`src/ams/msclients/cui/cui.c`) now tries
a new `DisplayMimeBody()` path before falling back to `metamail`/the
raw unscribed-body dump: fetches the message via the existing
`CUI_GetBodyToLocalFile()`, parses it with the shared `mimepart.c`
(same module `text822.c` uses), and picks a displayable part —
`multipart/alternative` resolves via `mimepart_SelectAlternative()`,
other multipart types (mixed, signed, ...) scan top-level children in
wire order for the first `text/plain`/`text/html` one. `text/html`
goes through `mimepart_HtmlToText()`; unlike `messages`' ATK Text
widget (Latin-1 glyphs only), `cui` writes to a real, UTF-8-capable
terminal, so UTF-8 bytes pass through unconverted rather than folding
to Latin-1/`?`. Every other top-level part in a `multipart/mixed` (or
similar) gets one `[attachment: <filename> (<type>, <n> bytes)]` line
instead of being rendered or silently dropped. Declines (falls through
to the untouched old behavior) for anything with no text part at all —
`x-be2`/`application/andrew-inset` native-format mail, or a bare
image/attachment message — so those paths are exactly as they were
before this change.

Verified live against wdc's real mirrored INBOX (3949 messages):
message 1 (`multipart/alternative`, the one that started this) now
renders its `text/plain` body correctly; a `multipart/mixed` message
with a trailing footer part shows `[attachment: unnamed (text/plain,
135 bytes)]`; a `multipart/signed` (PGP-signed mailing-list mail, common
in this mailbox) correctly shows the signed `text/plain` body plus
`[attachment: signature.asc (application/pgp-signature, 499 bytes)]`;
a bare `text/html` message strips to readable text. `revival/tests/
mime-display-tests` (9/9, unaffected — `mimepart.c` itself wasn't
touched) re-run clean.

**The `metamail` bug this surfaced along the way:** with the mimepart
path now covering essentially all real mail, `metamail` is only ever
reached for genuinely non-text content with no text part at all — but
it would have hit the exact same CRLF-blindness bug already fixed in
`text822.c`'s `GetHeader()` months earlier (see the 2026-07-21 entry
above), independently, since `metamail.c`'s `Read822Prefix()` has its
own from-scratch header/body boundary scan that also only recognized a
bare `'\n'`. On any CRLF-terminated message with no text alternative,
it would read straight to EOF and exit with "Could not find end of
mail headers" instead of falling back gracefully. Fixed the same way:
a bare `'\r'` no longer resets the "just saw a newline" state, so
`"\r\n\r\n"` is recognized exactly like `"\n\n"`. See the narrative
writeup in `revival.md`'s "Old bugs never found till now" for the
fuller story of both parsers making the same assumption independently.
Confirmed directly (not just via `cui`): `metamail -m cui` against a
synthetic CRLF-terminated `text/plain` message now finds the boundary
and prints the body instead of exiting -1.

Files touched: `src/ams/msclients/cui/cui.c` (new `DisplayMimeBody()`
and five small static helpers, ahead of `GetBodyFromCUID()`; one new
`#include <mimepart.h>`), `src/overhead/mail/metamail/metamail/
metamail.c` (one-line fix in `Read822Prefix()`, `+8` comment). Both
compile-verified against their `fossil cat` originals (deduplicated
warning-message diff, not raw counts): `cui.c` gains only more
instances of the pre-existing `-Wdeprecated-non-prototype` class (the
new `moreprintf()` call sites), no new warning class; `metamail.c`
gains none. `cuin`/`cui` relinked (no `libmsshr.a` rebuild needed —
`mimepart.c` itself wasn't touched), `metamail` rebuilt, both
installed.

### 2026-08-10 — `messages`: double-spaced headers, a third CRLF-blindness site

**Symptom:** wdc reported message headers displaying "double spaced"
in `messages` (unlike `cui`, which was unaffected). Suspected the same
CRLF line-ending issue already fixed twice elsewhere in this project
(the `text822.c` `GetHeader()` blank-line fix and the `metamail.c`
`Read822Prefix()` fix, both logged above).

**Root cause:** a third, distinct spot in the same function. `GetHeader()`
reads each header's physical line(s) via `fgets()` — already fixed to
recognize a `"\r\n"` blank line as end-of-headers — but never stripped
the `\r` itself from the line content it returns. On CRLF-terminated
mail (any real IMAP-fetched message), every `LineBuf` handed back to
`ReadMessage()`'s header loop still ends `"...\r\n"`, and that literal
`\r` byte then gets inserted as-is into the ATK `text` object right
along with the rest of the line (`text822_AlwaysInsertCharacters`,
`fnote_AlwaysInsertCharacters` — every insertion call site in the
header loop uses `linelen = strlen(LineBuf)` directly). A `\r` isn't a
line separator ATK's text widget understands; it renders as its own
character, producing the double-spaced look. Not visible in `cui`
because `cui`'s own header-printing loop (`PrintMimeHeaders`,
2026-08-09 entry above) reads lines via a plain `fgets()` loop of its
own and only cares about locating the blank-line terminator — it never
specifically strips the `\r` either, but a raw `\r` in a terminal just
returns the cursor to the start of the line rather than rendering a
visible glyph, so the symptom never showed up there the same way.

**Fix:** `GetHeader()` now strips a trailing `\r` off *every* physical
line right after each `fgets()` call, not just once at the end — a
folded header (very common in real mail: `Received:`,
`DKIM-Signature:`, `Authentication-Results:`, `X-Microsoft-Antispam-*`
all routinely wrap across several physical lines) is assembled into one
logical `LineBuf` across multiple `fgets()` calls, so a fix that only
handled a trailing `\r` at the very end would still have left one
embedded mid-buffer per wrapped line. Compile-verified against the
`fossil cat` original: *zero* warning-set delta, not even additional
counts of an existing class. `text822.do` rebuilt and installed.
wdc confirmed live: header block is single-spaced now.

Three independent sites in this codebase have now made the same wrong
assumption about line endings (`text822.c` `GetHeader`'s blank-line
check, `metamail.c` `Read822Prefix`'s blank-line check, and this one —
the same function's failure to strip `\r` from line *content* once
found). See `revival.md`'s "Old bugs never found till now" for the
narrative writeup of the first two; this third one is the same root
cause (CRLF wire format meeting LF-only-authored 1990s parsing code)
recurring a level deeper in code this project had already touched
once.
