# Porting Changelog

Changes made while porting AUIS 6.3.1 to modern platforms.

## 2026-06-24

### Created `config/darwin/system.h`

New platform config for macOS/Darwin, derived from `config/i386_Linux/system.h`.
BSD-style (`SY_B43`), POSIX, flex, ANSI compiler. Omits `setlinebuf` macro
(macOS provides it natively).

### Created `config/site.h`

Empty placeholder required by the include chain.

### `overhead/class/pp/class.c`

- Replaced manual `malloc()`/`realloc()` declarations with `#include <stdlib.h>`
- Added forward declarations for `PushFile()` and `PopFile()` (defined in
  `classpp.flex`, called from `class.c`)

### Result

Class preprocessor compiles, links, and runs on macOS/Darwin.
38 warnings remain (K&R prototypes, format strings) — all non-fatal.

### Temporary build command (pre-imake)

```bash
cd src/overhead/class/pp
flex -o classpp.c classpp.flex
cc -I. -I../../../config/darwin -I../../../config -DFLEX_ENV -DPOSIX_ENV -c classpp.c
cc -I. -I../../../config/darwin -I../../../config -DFLEX_ENV -DPOSIX_ENV -c class.c
cc -o class class.o classpp.o -ll
```

### Tested: processing `atk/text/bp.ch`

Successfully processed `bp.ch` (a minimal 16-line class definition inheriting
from `dataobject`) into `bp.ih` (191 lines) and `bp.eh` (431 lines).
The preprocessor walked the class hierarchy through `dataobject` → `observable`
→ `traced` → `basicobject`, generating vtables, dispatch macros, and `super_`
macros at each level.

```bash
./class -I../../../atk/text -I../../../atk/basics/common ../../../atk/text/bp.ch
```

## 2026-06-25

### `overhead/class/lib/class.h`

Replaced `#ifdef _IBMR2` block (conditional stdlib.h, manual `free()` declaration,
AIX `#define void char` hack) with unconditional `#include <stdlib.h>` and
`#include <errno.h>`.

### `atk/basics/common/dataobj.c`

- Added `#include <andrewos.h>` (was missing, needed for `strlen`/`strcpy`)
- Manually converted `dataobject__ListCurrentViews()` from implicit-int K&R
  to ANSI prototype (split-line definition not caught by modernizer)

### Base class chain compiles

All four files in the hierarchy compile on macOS/Darwin:
- `traced.c` — root object (3.3K)
- `observe.c` — observable pattern (9.7K)
- `dataobj.c` — data object base (10K)
- `bp.c` — test inset (4K)

```bash
CFLAGS="-I. -I../../../config/darwin -I../../../config -I../../../overhead/class/lib -I../../../overhead/cmenu -DFLEX_ENV -DPOSIX_ENV"
cc $CFLAGS -c traced.c observe.c dataobj.c
cc $CFLAGS -I../../text -c ../../text/bp.c
```

### `overhead/class/pp/class.c` — manual fixes after modernizer

- Simplified `errorhandler()` — removed `#ifdef POSIX_ENV`/`ANSI_C_SOURCE`
  three-way block, replaced with plain `void errorhandler(int sig)`
- Fixed `CompareMethods()` signature for `qsort()` — changed from
  `struct methods **` params to `const void *` (required by modern `qsort`)

### `overhead/util/hdrs/util.h`

Fixed three implicit-int extern declarations: `FreeConfigureList()`,
`refreshprofile()`, `setprofilestring()`.

### `atk/text/text.c`

- Added `static int` to two bare static variable declarations
- Converted six split-line K&R functions to ANSI prototypes (not caught
  by modernizer): `text__AlwaysWrapViewChar`, `text__WriteSubString`,
  `WrapStyle`, `PlayTabs`, `text__EnumerateEnvironments`, `PushLevel`,
  `ComingNext`
- Removed incorrect `static` on three functions that the `.eh` file
  declares as non-static
- Added `#include <util.h>` for `ULstrncmp`
- Added forward declarations for `PushFile`, `ComingNext`
- Fixed bare `return;` in `text__WriteOtherFormat` (non-void function)

### `atk/text/textv.c`

Added forward declarations for `charType()` and `InitializeMod()`
(defined in companion files `txtvcmds.c` and `txtvcsty.c`).

### Text object and view compile

Full text stack compiles on macOS/Darwin:
- `simpletext.c` — 27K (text storage/manipulation)
- `text.c` — 52K (styled text with environments)
- `textv.c` — 59K (text rendering/display)
- `bpv.c` — 4.9K (page break view)

### modernize tool

- Fixed split-line regex bug that was truncating function names when
  return type was on a separate line from the function name
- Fixed multi-name declaration parsing (`char *a, *b, *c;`)
- Added `union` to type recognition (was only matching `struct`/`enum`)
- Added `extern int errno;` → `#include <errno.h>` conversion

### Support library: 17 of 20 files compile

Added `#include <andrewos.h>` to files that called string functions
without declarations (legal in K&R C, errors in modern C). Fixed
`qsort` comparator in `list.c`, removed obsolete `sys_errlist` in
`buffer.c`, fixed function pointer parameters mangled by modernizer,
added `class_EnterInfo` declaration to `class.h`, fixed `union`
type in `envrment.c`.

Remaining: `complete.c`, `except.c`, `print.c` — not on critical path.

### imake build system works on Darwin

Created `config/darwin/system.mcr` with macOS paths: XQuartz at
`/opt/X11`, Homebrew tools at `/opt/homebrew/bin`, `LEXLIB = -ll`.

Fixed platform detection in `platform.tmpl`: imake/tradcpp defines
`__DARWIN__`, not `__MACH__`. The original `#if defined(__APPLE__) &&
defined(__MACH__)` never matched.

Created `site.mcr` and `site.h` with `#undef AMS_ENV` and
`#undef ANDREW_MALLOC_ENV`.

### Proper BASEDIR bootstrap

Removed source-tree INCLUDES/CLASS overrides from `site.mcr`. Set
`DEFAULT_ANDREWDIR_ENV` in `site.h` to `andrew-6.4/build/`. The build
system creates this directory structure and installs headers, libraries,
and binaries incrementally as each layer builds — `make dependInstall`
at the top level drives the whole process.

### Darwin system.h fixes

- `NEWPGRP()`: macOS `setpgrp()` takes no arguments (POSIX, not BSD)
- `LEXLIB = -ll`: macOS has `-ll`, not `-lfl`

### Full tree compiles

`make dependInstall` runs end-to-end through the entire AUIS source:
overhead (sys, util, class, bison, image, fonts, cmenu) and all of ATK
(basics, support, text, frame, ez, extensions, ness, raster, table,
console, figure, help, srctext, org, bush, chart, eq, fad, layout,
lookz, syntax, apt, image).

Fixed remaining compilation errors:
- `overhead/util/hdrs/genhdr.c` — missing `<stdlib.h>`
- `overhead/image/tiff/mkg3states.c` — missing `<stdlib.h>`/`<string.h>`
- `overhead/sys/sys.c` — missing `<stdio.h>`/`<stdlib.h>`, implicit-int `main`
- `overhead/addalias/addalias.c` — missing `<stdlib.h>`

### Proper BASEDIR bootstrap verified

Removed source-tree INCLUDES/CLASS overrides from `site.mcr`. The build
system creates `build/` and populates it incrementally: config headers,
`libatkos.a`, `class` preprocessor, `genmake`, help files, templates,
raster samples, and font data all install correctly.

### Install gap: `.ih` headers and `.do` objects

The `.ih` header files (generated by the class preprocessor) are not
being installed to `build/include/atk/`. These are installed by the
`InstallClassFiles()` rule which depends on `makedo` — the tool that
packages compiled objects into dynamically loadable `.do` files and
installs their headers alongside them.

`makedo` is part of `overhead/class/cmd/` and depends on the platform-
specific dynamic loading code in `overhead/class/machdep/`. There is no
Darwin machdep directory. This is the dynamic loader issue identified in
the porting assessment — it's now the critical path item.

### Darwin dynamic loader (`overhead/class/machdep/darwin/`)

Created a new machdep for Darwin using POSIX `dlopen()`/`dlsym()`:

- `doload.c` — 70 lines replacing ~500 lines of platform-specific a.out
  parsing. Loads `.do` files via `dlopen()`, finds `classname__GetClassInfo`
  via `dlsym()`. Falls back to underscore-prefixed symbol for Mach-O.
- `doload.h` — minimal header, defines `doload_extension` as `.do`
- `makedo.csh` — creates `.do` files via `cc -dynamiclib -undefined
  dynamic_lookup`. Uses `.do` extension for compatibility with existing
  build rules.
- `Imakefile` — builds only `doload.o` (no entry.o/globals.o trampolines
  needed with dlopen)

Modified `class/lib/Imakefile` to exclude `entry.o`/`globals.o` on Darwin.
Modified `class/lib/class.c` to skip `<a.out.h>` on Darwin, fixed
`<signal.h>` include, fixed `pathopen` implicit int, fixed function
pointer parameter in `class_EnterInfo`, fixed `class_IsType` multi-name
parameter mangling.

### Build tools all installed

Five tools in `build/bin/`: `class`, `makedo`, `doindex`, `cregister`,
`genmake`. Fixed `doindex.c`: skip `<a.out.h>` on Darwin, replaced
`sys_errlist` with `strerror()`.

### Dynamic loading pipeline verified

Tested end-to-end: `bp.do` created by `makedo` (`cc -dynamiclib`),
loaded by `dlopen()`, entry point `bp__GetClassInfo` resolved by
`dlsym()`. Multiple ATK `.do` files built during `make dependInstall`
(app, atom, atomlist, bind, cursor, dataobj, describe).

### `CLASS_CTRAMPOLINE_ENV` enabled

Added to `darwin/system.h`. Uses C-based lazy loading (checks if class
is loaded, calls `class_Lookup` if not) instead of x86 assembler
trampolines. Added `class_Lookup` declaration to `class.h`. Created
`classproc.c` in Darwin machdep with null `class_RoutineStruct`.

### ANSI prototypes in class preprocessor

Changed `usePrototypes` default from `FALSE` to `TRUE` — the prototype
generation code was already implemented behind the `-p` flag, just never
enabled on any platform. The RS/6000 (AIX) worked around the lack of
prototypes with `-D_NO_PROTO` instead.

Enabling prototypes exposed incomplete-type conflicts: struct types used
in method parameters need to be declared at file scope before the
prototypes reference them. Fixed by adding automatic forward declaration
emission to the class preprocessor — it scans all method `realargtypes`
for `struct X` patterns and emits `struct X;` at file scope in both
`.ih` and `.eh` files. No manual `.ch` file edits needed.

### `etext` symbol

macOS doesn't provide the linker `etext` symbol. Added `#ifdef sys_darwin`
guard in `class.c` to provide a static dummy.

### modernize: missing-include detection and function pointer fix

Added detection of calls to standard library functions (`strlen`, `exit`,
`printf`, etc.) and automatic insertion of the appropriate `#include`.
Fixed function pointer parameter handling — `type (*name)()` was being
converted to `int name`.

### `make -k dependInstall` results

Using `make -k` (keep going on errors) allows the build to continue past
overhead utility failures and reach ATK. Results:

- **ATK compiles with zero errors**
- 62 `.do` shared library files created during build
- 583 ATK headers installed to `build/include/atk/`
- Build tools: `class`, `doindex`, `makedo`, `cregister`, `genmake`,
  `mkparser`, `sys`

Applications in `build/bin/` (`ez`, `bush`, `figure`, `table`, etc.)
are **broken symlinks** to `runapp` — the generic application launcher
which was not built. `runapp` uses `genstatl` to statically link the
requested insets. Building a working `runapp` (or a statically-linked
`ez` directly via `genstatl`) is the next step.

The `.do` files are created during the build but cleaned by
`dependInstallClean` before they can be installed to `build/dlib/atk/`.
The install rules fire but `doindex` runs during install and the timing
of clean vs install needs investigation.

### Class preprocessor: `unknown` → `int` for untyped parameters

Method parameters in `.ch` files declared without a type (e.g. `printer`
in `PrintObject(..., printer)`) were emitted as `unknown` in ANSI
prototypes. Changed to `int` (the C default for untyped parameters).

### Mass modernization — attempted and reverted

Ran the improved modernizer across all 916 `.c` files. This exposed a
deeper problem: earlier modernizer runs (with buggy multi-name parameter
parsing and missing function pointer handling) had committed damaged
code. The accumulated damage from multiple partial modernization passes
made the tree unbuildable.

Reverted all `.c` files, but the revert target (`aa26555472`) itself
contained modernizer damage from earlier commits.

### Plan for next session

The `.c` files need a fresh start:

1. Revert ALL `.c` files under `src/atk/` and `src/overhead/` to **trunk**
   (the pristine 6.3.1 originals)
2. Re-apply the small number of intentional hand-edits documented in this
   changelog (class.c runtime, doindex.c, sys.c, etc.)
3. Run the now-fully-fixed modernizer in a single clean pass
4. Build with `make -k dependInstall`

Non-`.c` changes are safe and should be kept: class preprocessor
(`usePrototypes`, forward declarations, `int` for unknown types),
Darwin platform config, `class.h`, `doload.c`/`doload.h`, `makedo.csh`,
`classproc.c`, `site.h`, `site.mcr`, `util.h`, Imakefiles.

## 2026-06-29

Followed the plan from the previous session: reverted all `.c` files to
trunk, re-applied minimal compiler leniency instead of mass modernization,
then fixed errors surgically as they appeared.

### `config/darwin/system.mcr`

Added `-std=gnu89 -Wno-return-type` to `COMPILERFLAGS` (previously had
only the implicit-int/implicit-function-declaration/incompatible-pointer
warnings disabled). Note: `-std=gnu89` does **not** suppress "static
declaration follows non-static declaration" — that required a real fix,
see `fix-static-methods` below.

### `config/darwin/system.h`

Added `FILE_HAS_IO(f)` override: `(f)->_r` instead of the Linux/glibc
`_cnt` field — BSD/Darwin's `struct __sFILE` uses `_r` for buffered-bytes-
remaining.

### `overhead/util/lib/` fixes (got `overhead/` building with zero errors)

- `fdplumb6.c` — `d->dd_fd` doesn't exist on Darwin's `DIR` struct;
  replaced with `dirfd(d)`.
- `times.c` — guarded out the `osi_ZoneNames`/`osi_SecondsWest`/
  `osi_IsEverDaylight`/`osi_SetZone` block with `&& !defined(sys_darwin)`;
  Darwin's `system.h` already provides these via `tzname`/`timezone`/
  `daylight`/`tzset()` macros, and the two definitions conflicted.
- `usignal.c` — removed `extern char *sys_siglist[];` (macOS declares it
  `const`); cast the return value instead.
- `profile.c` — added `extern char *AndrewDir();` after the `andrewos.h`
  include; the `GLOBALPROFILE` macro called it before any declaration was
  in scope, causing an implicit-int return truncated to a pointer.

### `overhead/class/cmd/whichdo.c`, `overhead/cmenu/cmdraw.c`

"Static declaration follows non-static declaration" on plain (non-class)
static helpers — fixed with forward declarations instead of stripping
`static`, since these aren't class methods and don't need external
linkage:
- `whichdo.c` — forward decl of `pathopen()` was missing `static`,
  didn't match the later `static` definition.
- `cmdraw.c` — `DrawWormHole`'s forward declaration needs to go before
  its caller (`ShowAPane`), not just before its own definition (first
  attempt put it in the wrong place and didn't fix the error).

### `sys_errlist`/`sys_nerr` removal (ATK-wide)

Same pattern as the `doindex.c` fix from the previous session, applied
across `atk/org/orgv.c`, `chart/chartv.c`, `textobjects/diredv.c`,
`text/be1be2a.c`, `bush/bush.c`, `bush/bushv.c`, `launchapp/launchapp.c`,
`console/cmd/mailmon.c`, `console/lib/{console.h,venusmon.c,mailmonf.c,
setup.c}`: removed `extern char *sys_errlist[]` / `extern int sys_nerr`
declarations (macOS declares these `const`), replaced
`sys_errlist[errno]` usage with `strerror(errno)`.

### `malloc`/`realloc` extern removal (ATK-wide)

Removed stale `extern char *malloc();`/`extern char *realloc();`
K&R declarations (conflict with `<stdlib.h>`'s real prototypes) from
`basics/common/{rm.c,atomlist.c}`, `layout/{boxview.c,layoutv.c,
fillerv.c}`, `extensions/gsearch.c`, `raster/cmd/rastoolv.c` (file-scope),
and `value/clklistv.c`, `adew/arbiterv.c`, `typescript/tscript.c`,
`text/content.c` (local/in-function scope).

### New tool: `revival/tools/fix-static-methods`

The class preprocessor emits non-static extern declarations for every
class method in `.eh` files. Where the `.c` implementation defines a
method as `static` (legal K&R, since the class dispatch table found it
some other way), modern clang rejects it outright. Wrote a script to
strip `static` from class method definitions (functions matching
`classname__Method`) — this is the *correct* fix, not a workaround,
since these methods need external linkage for `dlopen`/`dlsym` to find
them. Run across `src/atk/`, fixed 21 files. Does not touch single-name
static helpers (e.g. `initself`, `DrawWormHole`) — those need forward
declarations instead (see `whichdo.c`/`cmdraw.c` above).

One straggler missed by the `__`-only regex: `basics/x/xgraphic.c`'s
`xgraphic_ApproximateColor` uses a single underscore, fixed manually.

### Vendored bison hang — root cause and fix

`make -k dependInstall` reproducibly hung (uninterruptible, unkillable
even with `kill -9`) every single run, always at the same spot: bison
parsing `atk/eq/eqparse.gra`. This had been silently causing the
"multiple make processes" confusion in earlier sessions — every build
attempt was hitting this exact same dead end, not actually colliding
with other sessions.

Root cause: AUIS bundles its own bison fork, "Andrew Bison A2.6"
(`overhead/bison/`, derived from GNU Bison 1.24, by Wilfred J. Hansen /
Andrew Consortium — see `overhead/bison/../../../bison/README` for the
original 1995/2002 documentation). It adds real extensions over stock
bison: multi-character string tokens (`"<="` as a bare grammar
terminal), and critically a "no symbol conflicts / shared parser
object code" design — `overhead/mkparser` post-processes bison's
`.tab.c`/`.tab.h` output into Andrew's own table-driven `cparser`
runtime so multiple bison-generated parsers can link into one binary
without colliding on `yyparse`/`yylval`/etc. This vendored bison gets
built and installed to `build/bin/bison`, shadowing the system bison on
`PATH` for the rest of the build. On Darwin/arm64 the vendored binary
itself hangs in the kernel (likely a 32-bit-era pointer/int-size bug) —
not worth chasing, since the README itself (written in 2002) already
recommended switching to stock FSF bison and only worried that
`mkparser` hadn't been tested against it.

Verified system bison (`/usr/bin/bison`, GNU Bison 2.3) + the existing
`mkparser` work correctly together, with one wrinkle: modern bison
derives output filenames from the input file's extension when no
`-o`/`--defines` is given, and old bison always hardcoded `.tab.c`/
`.tab.h` regardless of extension. Since AUIS grammar files use the
nonstandard `.gra` extension (not `.y`), modern bison was silently
producing `eqparse.tab.gra` instead of `eqparse.tab.c`/`.h`, which
`mkparser` couldn't find. Fixed:

- `config/andrew.rls` — `Parser()` macro now passes `-o classname.tab.c`
  explicitly, pinning the output filename regardless of input extension.
- `overhead/bison/Imakefile` — wrapped `InstallProgram(bison, ...)` in
  `#ifndef sys_darwin` so the vendored binary is still built (harmless,
  useful for reference) but never installed to `build/bin`, so it can't
  shadow the system bison.

Verified end-to-end: `make eqparse.h eqparse.c` in `atk/eq/` now
completes in under a second using system bison, producing correct
output that `mkparser` processes without error.

Confirmed via grep that only `ness.gra` (`atk/ness/objects/ness.gra`)
actually uses the multi-character-string-token extension (`"<="`,
`">="`, `":="`, `"~:="` as bare terminals in `%nonassoc`/`%left` and
rule bodies) — the other seven `.gra` files in the tree (`eqparse.gra`,
`num.gra`, `parse.gra`, `parsey.gra`, `eliy.gra`, `prsdate.gra`,
`getdate.gra`) only have ordinary C string literals inside action code,
which stock bison handles fine. `ness.gra` will need either a rewrite
to use named `%token` declarations instead of inline string-literal
terminals, or a working copy of the Andrew bison fork — deferred, not
on the critical path for `eq`/ATK core.

## 2026-06-30

### Add `fix-missing-static-decl` tool, fix remaining 'static follows
non-static' errors (127 → 0)

Two distinct sub-problems behind the same clang error, needing opposite
fixes — kept as two separate tools rather than merged, since they apply
to disjoint symbol categories (class methods vs. private file-scope
helpers):

- **Forward declaration missing `static`**: K&R source often puts a
  block of forward declarations near the top of a `.c` file without
  `static`, while the real definition further down *is* static (a
  private helper). New tool `revival/tools/fix-missing-static-decl`
  fixes this two ways: (1) splits/rewrites existing forward-declaration
  blocks (single-name-per-line and multi-name comma-list styles) to add
  `static` only to the names that need it; (2) inserts a brand-new
  `static TYPE NAME();` after the file's `#include` block when no
  forward declaration exists at all (the call-site-creates-an-implicit-
  declaration case, e.g. `chartobj.c`'s `Generate_Shadows`,
  `fadv.c`'s `DoAnimation`).
- Three names collided with standard library functions pulled in
  transitively (`hit.c`'s `abs`, `funs.c`'s `fmax`/`fmin`, `lset.c`'s
  `getline`) — auto-insertion would have masked the real conflict, so
  the tool maintains a skip-list and these were fixed by hand: `hit.c`'s
  reimplementation of `abs` was simply deleted (libc's is identical and
  already in scope); `funs.c`'s `fmax`/`fmin` renamed to `funs_Max`/
  `funs_Min`; `lset.c`'s `getline` renamed to `lset_GetLine` (its 2-arg
  signature was also being type-checked against POSIX's 3-arg
  `getline`, which is what the separate "too few arguments" errors in
  that file actually were — same root cause, no separate fix needed).
- Two tool bugs found and fixed during rollout: (1) the insertion logic
  initially placed new declarations after the textually-last
  `#include`, without tracking `#ifdef` nesting — landed inside a
  still-open `#ifdef UTMP_MUNGE` block in `tscript.c`, hiding the
  declaration from the actual Darwin build. Fixed by tracking
  `#if`/`#ifdef`/`#ifndef`/`#endif` depth and only inserting at depth 0.
  (2) The declaration-block rewriter didn't track brace depth either,
  so it matched a *block-scope* (local, inside-a-function) forward
  declaration in `im.c` and `compile.c` and incorrectly added `static`
  to it — block-scope function declarations cannot be `static` in C,
  a stricter rule than file scope, so this introduced a new error
  ("function declared in block scope cannot have 'static' storage
  class"). Fixed by tracking `{`/`}` depth and only treating lines as
  file-scope declaration blocks at depth 0. Also added tolerance for
  trailing `/* comment */` text on declaration/definition lines, which
  the original anchored regexes rejected outright (silently failing to
  detect `mtextv.c`'s `definition`/`implementation`, both commented
  `/*RSKadd*/`).

### Critical bug found in `fix-static-methods` (already committed) —
silently truncated 16 files

While chasing the above, found that `fix-static-methods`'s Pattern A
regex (`static ... classname__Method(` on one line) captured only up to
the opening paren and discarded everything after it when rewriting the
line — silently dropping the K&R parameter-name list and the closing
paren whenever a method's `static TYPE NAME(params)` was written on a
single line (as opposed to the 3-line K&R split, which was handled
correctly). E.g. `complete.c`'s

```c
static long completion__FindCommon(classID, string1, string2)
```

became

```c
long completion__FindCommon(
```

— invalid C, surfacing as "expected `)`" / "expected identifier or `(`"
errors against the *next* line. This had already been committed (the
"Add fix-static-methods tool... 21 files" commit) and pushed.

Found the full blast radius by diffing `andrew-6.4/src` against the
`trunk` checkout (andrew-6.4 is a branch off trunk, so any file
untouched by revival work should be byte-identical to trunk) rather than
a blanket `fossil revert`, which would also have discarded legitimate
later edits stacked on the same files (e.g. `content.c`'s separate
`malloc` extern cleanup). 16 of the 21 files were affected, 32 functions
total. Fixed the tool itself (capture the prefix as a zero-width
lookahead, slice the original line instead of reconstructing it from
matched groups — preserves everything after `static `), then wrote a
one-off repair script comparing each corrupted line against its trunk
counterpart (minus `static `) to restore the dropped parameter lists
exactly, leaving every other edit on those files untouched. Verified via
a fresh trunk diff (only the intended `static`-stripping remained) and
by compiling all 16 files individually.

**Lesson**: a full-tree build completing with `make -k` (exit 0) does
not mean zero errors — `-k` means "keep going past errors." Compile-
verify every individual file a tool touches before committing, not just
that the overall build process finishes.

### Clean rebuild after all fixes

`make Clean; make -k dependInstall` from scratch: **87 `.do` files**
(previous high-water mark was 62), **602 headers installed** in
`build/include/atk/` (up from 583), errors down to 101 from 284.

## 2026-06-30 (continued)

### Fixed `mkparser`/modern-bison incompatibilities: `eqparse.c` and
`num.c` now compile clean

The earlier YYDEBUG fix (above) got `eqparse.c` and `num.c` further but
not all the way — investigated the rest. Both grammars now compile with
zero errors. Three more bugs in `overhead/mkparser/mkparser`, all the
same shape: the script was written against 1990s Bison's exact textual
output and relies on brittle markers (specific comment strings, filename
patterns) that modern Bison (2.3) doesn't reproduce, even though the
underlying table-compression algorithm and overall structure are
unchanged.

- **`YYFLAG` undeclared.** `cparser.h`'s `struct parser_tables.defflag`
  is a real runtime sentinel — `cparser.c` compares `actx[s] == defflag`
  to decide "use the default reduction" while walking the LALR tables.
  `actx` is mkparser's name for `yypact` (confirmed: `(short *)yypact,
  /* actx */`), and old Bison generated a `YYFLAG` macro holding exactly
  this sentinel. Modern Bison still has the identical sentinel, with the
  identical role (confirmed via its own usage, e.g. `if (yyn ==
  YYPACT_NINF)`), just renamed to `YYPACT_NINF` — and critically, its
  *value* is grammar-specific (`-16` for `num.gra`, `-83` for
  `eqparse.gra`), so the fix could not be a hardcoded literal. Changed
  mkparser to emit `YYPACT_NINF` (relying on Bison's own already-emitted
  `#define`) instead of the now-nonexistent `YYFLAG`. (`nextx`/`yypgoto`
  has no equivalent sentinel in modern Bison's output, but `cparser.c`'s
  algorithm already falls back safely to its regular table-check path
  when the shortcut doesn't apply, so no corresponding fix was needed
  there.)

- **`reduceActions` left unclosed, swallowing the rest of the file.**
  mkparser extracts just the grammar-specific rule actions from Bison's
  `switch (yyn) { ... }` dispatch and re-wraps them as a standalone
  `reduceActions()` function (since `cparser`'s `parser_Parse` supplies
  its own driver loop and only needs the raw actions). It used to detect
  the end of that switch via a literal comment old Bison left in its
  output ("the action file gets copied in in place of this dollarsign");
  modern Bison doesn't emit that text at all, so the rule never fired,
  and `reduceActions`'s function body and switch were never closed —
  swallowing all of Bison's own remaining `yyparse()` driver code, and
  the grammar's entire epilogue (every action routine after `%%`,
  e.g. `eqparse.gra`'s `eq__Parse`, `num.gra`'s `locallexer`) into one
  giant unclosed block. That's what was actually behind the "function
  declared in block scope cannot have 'static' storage class" /
  "parameter list without types" / "use of undeclared identifier 'self'"
  errors reported against `eqparse.gra`/`num.gra` lines — the grammar
  source was never the problem. Replaced the comment-based marker with
  Bison's `default: break;` (the dispatch switch's always-present
  fallback case, immediately followed by the switch's own closing
  brace) — present in every Bison version since it's part of the literal
  switch-statement boilerplate, not skeleton-specific text.

- **Bison's own `switch (yyn)` opening brace leaking through.** Old
  Bison wrote `switch (yyn) {` on one line, so mkparser's rule matching
  that line and discarding it (`next`) was sufficient. Modern Bison
  puts the brace on its own following line (Allman style) — the match
  rule still fired and discarded the `switch (yyn)` line, but the
  brace line right after it was untouched by that rule and fell through
  to mkparser's default print-everything behavior, leaking an extra,
  never-closed `{` into the output (on top of the two mkparser injects
  for its own `reduceActions(...) {` and `switch (i) {`). Added a
  `getline` to consume and discard that line too. Combined with a
  matching second `}` in the new-marker fix above (the old code only
  ever emitted one closing brace, closing the switch but never the
  function), brace balance across the whole generated file is now
  verified at exactly 0 (was +2/+3 before these fixes, by direct count).

- **`#if YYDEBUG` guard incorrectly stripped** (see prior entry above,
  same root cause family): mkparser's `intables` heuristic — "we've seen
  an `#include <stdio.h>` so we must be near the table declarations" —
  breaks because modern Bison's restructured skeleton has an earlier,
  unrelated `<stdio.h>` include for its `YYFPRINTF` debug-macro setup.
  Rather than patching the heuristic further, removed the now-redundant
  stripping rule entirely: modern Bison already correctly self-guards
  this content with its own `#if YYDEBUG`/`#endif` (the table-declaration
  guard-*injection* logic that follows, for `yyprhs`/`yyrhs`/`yyrline`,
  is untouched and simply no-ops for modern Bison's output, since its
  table declarations don't match the old `short yyprhs` text pattern
  it looks for — also harmless, since those tables are already
  natively guarded too).

- **`eqparse.gra` itself had one real bug**, unmasked once the above
  were fixed: `union ptr x(), value;` mixes a function declarator and a
  variable declarator sharing one type in a single statement — a shape
  `fix-missing-static-decl` doesn't parse (it only handles statements
  made entirely of `name()`-shaped entries), so it inserted a redundant
  second declaration instead of fixing this one. Split by hand into
  `static union ptr x(); union ptr value;`. Also removed a duplicate
  `static int yylex();` the tool inserted — a non-static-block static
  declaration for the same name already existed further down in the
  file, which the tool doesn't currently check for before deciding a
  declaration is "missing".

Verified both `eqparse.c` (`atk/eq/`, the equation-editor parser) and
`num.c` (`atk/rofftext/`, the roff numeric-expression parser) compile
with zero errors end to end, including a fresh bison + mkparser
regeneration each time (not just incremental edits to stale generated
output).
