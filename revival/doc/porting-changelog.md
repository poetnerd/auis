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

The codebase was written for ILP32 (32-bit int, long, pointer). Five distinct bug classes emerged on arm64 LP64. All five have been identified and swept. See `porting-assessment.md` §12 for full analysis.

| Variant | Root cause | Scope |
|---|---|---|
| #1 Pointer truncation | Undeclared pointer-returning fn → implicit `int` return → 32-bit truncation | 23 sites fixed with local `extern` decls |
| #2 Stack spill | >8 args through `void(*)()` → arm64 drops 9th+ arg | classpp: typed casts for argcount ≥ 8 |
| #3 Zero-extension | `int -1` const through untyped dispatch, `long` receiver sees `0xFFFFFFFF` | `(-1L)` in observe.ch, value.ch, class.h |
| #4 scanf %d | `%d` writes 32 bits into `long*`; upper 32 bits garbage | Full tree audit; 11 real bugs fixed |
| #5 Dispatch int/long | Negative `int` arg through untyped dispatch to `long` param, sign bit lost | lpair, panel, dialog, dialogv, table, fad, srctext, eq, metax; full sweep committed |

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
