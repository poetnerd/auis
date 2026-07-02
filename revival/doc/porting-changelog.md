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

### Remaining error categories cleared: 101 → 0

Worked through everything left after the mkparser fixes, file by file,
compile-verifying each before moving on:

- **`figio.c`, `fad.c`** — K&R parameter-type declaration missing
  entirely (parameter silently defaults to `int`); added the missing
  `FILE *fl;` / `char *name;` lines. Same root cause as several fixes
  in earlier sessions, just not yet reached by the build.
- **`xcursor.c`, `unknown.c`, `srctext.c`** — called `NewString()`
  (declared in `overhead/util/hdrs/util.h`, defined in
  `overhead/util/lib/newstr.c`) without including `<util.h>`. Added it.
- **`mkcon.c`, `helpdb.c`** — called `close()` (POSIX fd-based) on
  `FILE *`/`DIR *` handles instead of `fclose()`/`closedir()`. Genuine
  pre-existing bugs, not porting artifacts — undefined behavior on any
  platform, just silently tolerated by old lenient compilers. Fixed to
  use the correct cleanup function for each handle type.
- **`xgraphic.c`** — `NULL` (a real pointer on modern systems) assigned
  to `Drawable` (an X11 integer XID typedef) and passed to an `int`
  parameter. Changed to `0`, the semantically correct null value for
  integer ID types.
- **`org.c`, `suiteta.c`, `srctextv.c`** — generic "long used to smuggle
  a pointer through a void-typed attribute slot" pattern (`tree_NodeDatum`,
  `suite_ItemAttribute`, the `rock` parameter documented in-line as
  "actually a string") passed directly to `free()`/`atoi()`. Added
  explicit casts to restore the original behavior under strict typing.
- **`cel.c`** — same `getline()`-collides-with-POSIX issue as `lset.c`
  in an earlier session; renamed to `cel_GetLine` (also resolved 4
  "too few arguments" errors at its call sites, same root cause: clang
  was type-checking calls against POSIX's 3-arg `getline` once the
  conflict made that the visible declaration).
- **`spread.c`, `table.c`, `tabio.c`, `box.c`** — same `malloc`/`realloc`
  K&R extern-decl-conflicts-with-stdlib.h pattern fixed for other files
  earlier in this session; just hadn't been reached by the build yet.
- **`m3textv.c`, `chart.c`, `srctextv.c`/`cpptextv.c`/`ctextv.c`/`modtextv.c`**
  — more "static follows non-static" instances, some newly reached by
  the build (`chart.c`), some missed in earlier passes because the
  declaration line had a trailing comment the tool didn't yet tolerate
  at the time it ran (`m3textv.c`). Found and fixed a real idempotency
  bug in `fix-missing-static-decl` along the way: `try_parse_decl_block`
  returned early for blocks that were *already* static without
  recording their names as matched, so re-running the tool on an
  already-fixed file treated those names as having no declaration at
  all and inserted duplicates. Fixed by parsing already-static blocks
  too (just not rewriting them) so their names are always recorded.
- **`im.c`'s `im__HandleRedraw`** — a *second* instance of the
  `fix-static-methods` Pattern-A truncation bug (see above), missed in
  the original sweep because that sweep's trunk-diff check used `head -6`
  per file and this file's corruption hunk happened to fall past that
  cutoff. Re-ran a complete, untruncated trunk-diff across all 21
  originally-affected files plus a structural grep (any line that's a
  class-method name immediately followed by a bare, unclosed `(`) to
  confirm no further instances exist anywhere in that set.
- **`atk/basics/common/profile.c`** — genuinely platform-incompatible
  code, not a quick textual fix: implements BSD-style `a.out`/`gprof`
  CPU profiling (`struct phdr`, `profil()`, raw text-segment addresses),
  which has no equivalent on Mach-O/Darwin. The file already had a clean
  `#ifndef SYSV ... #else return FALSE ... #endif` fallback for systems
  without this support; extended all 5 guards to `!defined(SYSV) &&
  !defined(sys_darwin)` so Darwin takes the same already-implemented
  "profiling unavailable" path. Also needed the `<a.out.h>`/
  `<sys/gprof.h>` includes guarded out the same way, and the *order* of
  `#include <class.h>` / `<andrewos.h>` swapped — `sys_darwin` is only
  defined once `andrewos.h` pulls in Darwin's `system.h`, so a guard
  checking it needs to come after that include, not before.
- **`atk/basics/lib/Imakefile`** — this directory compiles sibling
  sources from `../common`, `../x`, `../wm` into `libbasics.a` (a static
  build of the same code that's also built as individual `.do` files in
  those directories), but only `.ih` files get installed centrally
  (`InstallClassFiles` doesn't install `.eh`) — so the angle-bracket
  `#include <profile.eh>` couldn't be found at all from this directory.
  Added `LOCALINCLUDES = -I../common -I../x -I../wm` so the compiler
  looks for `.eh` files next to their `.c` siblings, matching how this
  already works for the canonical (non-`lib/`) build of the same files.
- **`atk/chart/chart.c`** — `<values.h>` doesn't exist on Darwin
  (obsolete SysV/BSD header providing `MAXINT` etc., superseded by
  `<limits.h>`); the file already had a defensive `#ifndef MAXINT
  #define MAXINT ... #endif` fallback right after the include, so just
  guarded the include itself with `&& !defined(sys_darwin)` and let the
  existing fallback do its job.

### Final result: 278 `.do` files, 602 headers, effectively zero errors

`make Clean; make -k dependInstall` from scratch. The previous
high-water mark (before this session) was 62 `.do` files. The single
remaining grep hit for "error:" in the build log is a false positive —
a string literal in `defaults.c` (`"Internal error: unknown recognizer
type"`), not a diagnostic. No `fatal error:` lines remain at all.

### `-k` no longer needed; found and fixed one more gap while verifying

Tested `make dependInstall` without `-k` (only used originally to push
past real blocking errors and maximize `.do`/header counts despite
them) — it completed cleanly, same 278 `.do` files, exit 0.

Verifying this surfaced one real gap: `atk/basics/lib/` independently
compiles `../x/xgraphic.c` a second time (into the static `libbasics.a`,
separate from the `.do` built directly in `atk/basics/x/`) but failed
there with "incomplete definition of type 'Display'". Root cause:
`atk/basics/x/Imakefile` sets `-DXLIB_ILLEGAL_ACCESS`, which is what
makes modern Xlib.h expose `Display`'s full struct instead of just a
forward declaration (`struct _XDisplay;` — Xlib.h hides the real
definition without it, by design, to push callers toward accessor
macros). `basics/lib/Imakefile` has its own separate `DEFINES`/Makefile
and wasn't setting this flag, so its independent compile of the same
source saw only the opaque type. Added the same `-DXLIB_ILLEGAL_ACCESS`
there. This was very likely already broken before today (silently
masked — this directory's build never got far enough to reach it until
the `.eh`-lookup fix earlier in this session). `make Clean; make
dependInstall` (no `-k`) now succeeds end to end: 278 `.do` files, 0
errors, exit 0.

## 2026-06-30 (runtime debugging session)

First end-to-end runtime smoke test of the binaries produced by the
now-clean compile phase: does `runapp`/`ez` actually launch.

### `./build/bin/ez` unkillable hang — misdiagnosed, then correctly root-caused

`ez -help` hung in an unkillable kernel wait (`UE` state in `ps`, `lldb -p
<pid>` couldn't even attach). Initial diagnosis pointed at Bitdefender's
EndpointSecurity system extension stalling on `AUTH_EXEC`/`AUTH_OPEN`
kernel callouts — supported at the time by `sample <pid>` showing 100% of
samples parked in `class_ProcessClassPath` (the classpath/`.do`-index
scanning code, `overhead/class/lib/class.c:477`) and by two unrelated
processes (a different project's Python interpreters) also stuck in `UE`
state, which seemed to confirm a host-wide problem rather than an AUIS bug.

A `spindump` capture (root not required for a live/recent process)
overturned that theory. `runapp` showed as `(suspended) (zombie)` — already
terminated, stuck mid-teardown — with its blocking turnstile chain
resolving to `Code [808]` (VS Code), via `Responsible: Code [808]` (set
because the binary was launched from a VS Code-owned terminal/Bash tool).
The same two stray Python zombies that had seemed like host-wide proof
*also* had `Responsible: Code [808]` and terminated on the exact same
kernel blocking primitive, despite reaching it via a completely different
path (a `dyld4::halt`/`abort_with_payload` crash, not a file-open stall).
No turnstile chain anywhere in the ~110k-line spindump named Bitdefender.
Conclusion: VS Code's integrated-terminal responsible-process/exception-port
relationship was intercepting the SIGSEGV's exception delivery and never
completing the crash-reporter handshake, leaving the process suspended
forever instead of actually crashing and exiting. Confirmed by running
`ez -help` from a native Terminal.app window instead: no hang at all,
just `Segmentation fault: 11`.

**Practical implication, not yet fixed or further investigated:** any future
crash in a binary launched from VS Code's integrated terminal (including an
AI-extension Bash tool) can plausibly hang unkillable the same way. Build
and run AUIS binaries from native Terminal.app/iTerm2 when testing for
exactly this reason, not as a general habit.

### `runapp.c`'s `AndrewDir()` call — the actual segfault, and a systemic bug class

`lldb` (run natively) caught the real crash: `EXC_BAD_ACCESS` at
`class_ProcessClassPath + 64` (`ldrb w8, [x20]`), `x20 == 0x29ec0c0` — a
garbage pointer dereferenced on the function's very first line (`*path ==
'\0'`). Root cause: `atk/apps/runapp.c:81` calls `AndrewDir("/dlib/atk")`
with no prototype in scope anywhere (`andrdir.h` only defines the
`QUOTED_DEFAULT_*` macros, never declares `AndrewDir`'s return type). Under
K&R rules an undeclared function defaults to returning `int`; this build's
`-Wno-implicit-int -Wno-implicit-function-declaration` (needed to get this
codebase through modern clang at all) silently re-enables that default
instead of erroring. On LP64/arm64 that truncates `AndrewDir`'s real 64-bit
heap-pointer return value to 32 bits; the `(char *)` cast back zero-extends
the truncated value into a garbage pointer — exactly matching the fault
address. This is the same general hazard class as the `malloc`/`getenv`
K&R-extern-decl fixes from earlier sessions, just on functions whose return
type happens to be a pointer rather than `int`, so it manifests as a wild
pointer instead of a silently-wrong number.

Grepped the build log for the same compiler tell
(`-Wint-to-pointer-cast`, i.e. `warning: cast to 'TYPE *' from smaller
integer type 'int'`) and found 22 more instances of the identical pattern.
Fixed by adding a local `extern <ReturnType> FunctionName();` declaration
near the top of each calling file — matching this codebase's own existing
convention (several files already do exactly this for `getenv`/`malloc`,
e.g. `overhead/util/lib/andrwdir.c`) rather than introducing header-based
prototypes, since the relevant headers (`andrdir.h`) are build-generated
with machine-specific macros and not a natural home for this. Fixed in:
`overhead/util/lib/andrwdir.c`, `andydir.c`, `xbasedir.c`, `localdir.c`
(each needed `extern char *GetConfiguration();`), `config.c`, `profile.c`
(each needed `extern char *getenv();`), `atk/basics/common/environ.c`
(needed externs for `GetConfiguration`/`AndrewDir`/`LocalDir`/
`ReadConfigureFile`/`GetConfig` — these back the `environ__*` Class
methods), `atk/apps/genstatl.c` (`AndrewDir`/`XBaseDir`), `atk/apps/runapp.c`
(`AndrewDir` — the fix that resolved the `ez` crash), `overhead/util/lib/setprof.c`
(`AndrewDir`), `overhead/class/cmd/whichdo.c` (`getenv`/`AndrewDir`),
`overhead/util/lib/fdplumb4.c` (`qopen`, which already has a correct
prototype in `util.h` — `fdplumbi.h`, the header this file actually
includes, doesn't pull that in, so added a local extern instead of
switching headers).

Two warning sites were deliberately **not** treated as the same bug:
`init.c`'s `(char *) atoi(...)` and `tif_packbits.c`'s `tif->tif_data =
(char *) TIFFScanlineSize(tif)` both cast a genuinely-`int` value into a
pointer-sized slot on purpose (int→pointer, the safe direction — not the
dangerous pointer-truncated-to-int-then-back-to-pointer round trip). Two
more were left unfixed as lower priority rather than not-a-bug:
`bison/files.c`'s `tryopen` (already has a `FILE *tryopen();` forward
declaration at file scope, yet two of its dozen-plus call sites still warn
— not fully root-caused; `bison` is a vendored build-time tool, not part of
the AUIS runtime) and vendored `tif_packbits.c`/libtiff more generally
(not AUIS-authored code).

Rebuilt via `make dependInstall` (no `-k`) — exit 0, all 23 originally-found
`-Wint-to-pointer-cast` warnings for the 12 fixed files gone from the log.

### Result: `ez` launches

`./build/bin/ez` (run from native Terminal.app, not VS Code) now gets past
`class_Init`/`class_ProcessClassPath` entirely: `Starting ez (Version 7.0,
ATK 6.3.1); please wait...`, then into X11/XIM connection logic. New, much
more minor issue surfaced there, not yet investigated: `Could not find
host /private/tmp/com.apple.launchd.*/org.xquartz in ez(xim)` and a
`xfontdesc_CvtCharToGraphic: 0 width character` warning.

### `ez` draws a real window — first time in this revival — then crashes in scrollbar rendering

`ez` forks and the parent always `exit(0)`s immediately
(`application__Run`/`application__Fork` in `atk/basics/common/app.c:367-395`
— normal background-the-app design, not a bug); the actual work happens in
the forked child, which inherits the open X connection. `application__ParseArgs`
(`app.c:140-143`) has an undocumented bare `-d` switch that sets
`self->fork=FALSE`, keeping the app in the foreground for debugging —
useful enough to note here for next time. `ez -d`, run natively, drew an
actual window onscreen (confirmed by the user watching XQuartz/the Dock)
before crashing with `Bus error: 10`.

Caught under `lldb` (native terminal): `EXC_BAD_ACCESS` inside
`sbuttonv__DrawBorder` (`atk/supportviews/sbuttonv.c`), called from
`draw_elevator` in `atk/supportviews/scroll.c:1164` — a 10-argument call
(`sbuttonv_DrawBorder(self, r.left, r.top, r.width, r.height,
self->elevatorprefs, sbuttonv_BORDEROUT, TRUE, NULL)`, classID implicit).
`image lookup -a <fault-addr>` showed the crash was a write through the
function's `interior` parameter (the 10th/last argument, passed as literal
`NULL` at the call site) — except the value it actually held at runtime was
the address of a *different, unrelated function*
(`xgraphic__SetFGColor + 308`), not a null or valid `struct rectangle *`.

Disassembling both the callee's prologue and the caller's argument setup
(see [[project_classpp_vtable_codegen_bug]] for the full register-by-register
walkthrough) pinned this down precisely: `scroll.c` dispatches `DrawBorder`
*virtually*, through this codebase's generic class-procedure-table
mechanism (`sbuttonv_CLASSPROCEDURES->routines[10]`, cast to `(void (*)())`
— a K&R "unspecified parameters" function pointer type, the core dispatch
mechanism used everywhere in this Class/OOP system, auto-generated into
every class's `.ih`/`.eh` by the class preprocessor). The callee correctly
expects its 10th argument on the stack at `[SP+8]` (confirmed in its
prologue). The caller correctly places arguments 1-9 (registers x0-x7, plus
an explicit stack store for argument 9) — but **never emits any instruction
storing argument 10 to `[SP+8]`** before the `blr`. The callee reads stale,
unrelated stack content left over from an earlier call in that same stack
region. Per the C standard this calling pattern is well-defined (not UB) —
real callee is also K&R-style/non-prototyped, argument types match after
default promotion.

**Leading hypothesis, formed from this one call site only, not yet
independently verified:** a genuine clang arm64 codegen bug that drops the
store for the *last* stack-spilled argument when calling through a
`(void (*)())`-typed pointer with more than 8 total arguments.
`DrawBorder` is unusually wide (10 args) for a class method, plausibly why
this is the first place the revival has hit it — but nothing yet confirms
this by reproducing it at a second, independent >=9-argument virtual call
site, or via a minimal standalone repro outside the AUIS tree. Not caught
at compile time either way; there is currently no compiler warning for
this pattern.

**Not yet fixed.** User's chosen direction, contingent on the hypothesis
holding up under further verification: fix it at the root, in the class
preprocessor (`overhead/class/pp/class.c`, the tool that generates
`.ih`/`.eh` from `.ch` class specs) so it emits properly, fully-prototyped
function pointer casts (matching each method's real declared signature,
which the preprocessor already knows from the `.ch` spec) instead of the
generic `(void (*)())`. Flagged as a separate, large task — likely a fresh
session/agent, to avoid re-deriving this investigation's context cold. Full
details, the exact disassembly evidence, and pointers for where to start in
`class.c` are in the project memory file
`project_classpp_vtable_codegen_bug.md`.

## 2026-06-30 (class preprocessor ABI fix)

### Root cause confirmed and fixed — no standalone repro needed

The standalone repro (minimal C file calling through `(void (*)())` with 10
args) did not reproduce the missing stack store at `-O` or `-O0`, confirming
the bug is context-sensitive rather than a clean isolated compiler bug. The
decision was made to fix the class preprocessor regardless: emitting a
prototyped function pointer cast is unambiguously correct and makes the
dispatch independent of whatever assumptions the compiler makes for
unprototyped calls.

### Design: `usePrototypes` split into Import and Export halves

The original `usePrototypes` flag in `class.c` controlled both dispatch
macro casts (in `.ih` and `.eh`) and function forward declarations (in
`.eh`). These are separate concerns with opposite requirements for this
codebase:

- **`.ih` dispatch macros** need prototyped casts so the compiler knows the
  full argument list and emits correct stack stores for args that spill
  beyond the 8 arm64 integer registers. This is the ABI fix.
- **`.eh` function declarations** conflict with unconverted K&R definitions
  in the object `.c` files if typed with ANSI prototypes. They must stay
  K&R `()` for compatibility.

`usePrototypes` was split into two independent flags:

```c
static int usePrototypesImport;  /* prototyped casts in dispatch macros */
static int usePrototypesExport;  /* prototyped decls of user functions  */
```

Defaults: `usePrototypesImport = TRUE` (always — this is the fix),
`usePrototypesExport = FALSE` (K&R declarations for mixed-codebase
compatibility). The `-p` flag sets both TRUE for a fully-modernized tree.

### Threshold: only prototype dispatch casts for 9+ argument methods

Enabling prototyped casts on all methods exposed pre-existing K&R type
abuses at call sites (pointers passed as `long`, longs as pointers, the
ATK suite attribute macro expansion pattern where a single macro arg expands
to two comma-separated values). These are real issues but orthogonal to the
ABI bug, which only manifests when arguments spill beyond the 8 arm64
integer registers (x0–x7).

The fix: dispatch macro casts use a prototyped type only when
`mp->argcount >= 8` (total args ≥ 9, past the register limit). Methods with
≤8 total args keep the K&R `(void (*)())` cast — all their arguments fit in
registers regardless, so no stack store is needed and no ABI bug exists.
This is an acknowledged workaround; a future pass should bring all call
sites up to properly typed dispatch.

### Additional class preprocessor fixes made in the same pass

Several secondary issues surfaced during the prototype-enable process and
were fixed:

- **Forward declarations for struct types**: `GenerateForwardDecls()` added
  to `class.c`, scanning all method `realargtypes` for `struct X` patterns
  and emitting `struct X;` into both `.ih` and `.eh` before the dispatch
  macros reference them.
- **Untyped parameters**: `.ch` parameters declared without a type (e.g.
  `printer` in `PrintObject(..., printer)`) previously emitted as `unknown`,
  changed to `void *` (pointer-sized, accepts any pointer arg without
  conversion error).
- **`InitializeObject` prototype**: the special-block declaration was always
  emitting a 1-param prototype (`struct classheader *` only), but class.c
  always calls `__InitializeObject(classID, self)` with 2 args. Fixed to
  emit the correct 2-param prototype in both `usePrototypesExport` branches.
- **`FinalizeObject` prototype**: removed from the special block (which
  hardcoded `void` return) and delegated to the classproc loop, which uses
  `mp->methodtype` and correctly reflects the declared return type (some
  classes declare `FinalizeObject() returns boolean`).
- **`classhdr` alias**: 41 `.c` files use `struct classhdr *ClassID`
  (original informal abbreviation) instead of `struct classheader *`.
  Rather than mass-editing those files, added `#define classhdr classheader`
  to `overhead/class/lib/class.h`.
- **Unnamed `.ch` parameters**: `event.ch` `Cancel(struct event *)` and
  `sbutton.ch` `SetPrefs(int ind, struct sbutton_prefs *)` and
  `SetLayout(int rows, int cols, enum sbutton_sizepolicy)` had unnamed
  parameters — the class preprocessor's parser treats the last identifier as
  the parameter name and strips it, leaving incomplete types (`struct  *`,
  `enum`). Fixed by adding explicit parameter names in the `.ch` files.

### Call-site fixes for the 12-arg `message_AskForStringCompleted`

`AskForStringCompleted` has 11 user args (12 total with classID), past the
8-register threshold, so it now gets a prototyped dispatch cast. Its
`functionData` cookie parameter (typed `long` in `message.ch` — the ATK
convention for a value that holds either integers or pointers) was being
passed struct pointers without a cast in three files. Fixed with LP64-safe
`(long)(void *)ptr` two-step casts in `metax.c`, `helpaux.c`,
`txtvcsty.c`, and `compchar.c`.

### Build result

Zero errors. 278 `.do` files, clean compile. The `sbuttonv_DrawBorder`
dispatch macro now emits:

```c
(*((void (*)(struct classheader *, void *, long, long, long, long,
             void *, boolean, boolean, void *))
   (sbuttonv_CLASSPROCEDURES->routines[10])))
  (&sbuttonv_classheader, v, x, y, w, h, prefs, lit, draw, interior)
```

The compiler sees 10 explicit arguments, generates correct register
assignments for x0–x7 (first 8) and explicit `str` instructions to
`[SP+0]` and `[SP+8]` (arguments 9 and 10). The `interior` parameter
arrives at `sbuttonv__DrawBorder` as `NULL` as intended, not as stale stack
content from a prior call. Runtime test pending (run `ez -d` from native
Terminal.app).

## 2026-07-01

### LP64 int/long sign mismatch in untyped observer dispatch

A third variant of the LP64 bug class, distinct from the pointer-truncation
and stack-spill bugs fixed earlier:

**Mechanism:** A plain `int` constant (`-1`) passed as an argument through
an untyped `(void (*)())` virtual dispatch macro is placed in a 32-bit `w`
register by the compiler. ARM64 zero-extends `w` to `x` on any write, so the
receiver's 64-bit `long changeType` parameter sees `0x00000000FFFFFFFF`
(4294967295) instead of `0xFFFFFFFFFFFFFFFF` (-1L). Comparisons against
the defining `#define` (which promotes to ULONG_MAX in the unsigned context)
silently fail.

**Crash:** `ez` hung after the first file was closed and the editor sat idle
for ~30 seconds. Root cause: `observable__FinalizeObject` calls
`observable_NotifyObservers(self, observable_OBJECTDESTROYED)` to tell
observers the object is going away. With `observable_OBJECTDESTROYED` defined
as plain `-1`, the dispatched value was `4294967295`, not `-1L`.
`bufferlist__ObservedChanged` compares `changeType == observable_OBJECTDESTROYED`
— never matched — so `bufferlist__RemoveBuffer` never fired. The freed buffer
remained in the bufferlist as a dangling pointer. The checkpoint timer
(`FindCkpBuffer`) later iterated the list, dereferenced the freed buffer's
fields, and crashed. Diagnosed by adding `fprintf` instrumentation to
`bufferlist.c` and observing `type=4294967295` in the log; confirmed by
disassembling `observable__FinalizeObject` and seeing `mov w1, #-0x1`
(32-bit) before the fix vs. `mov x1, #-0x1` (64-bit) after.

**Fixes:**
- `src/atk/basics/common/observe.ch`: `#define observable_OBJECTDESTROYED -1`
  → `(-1L)`
- `src/atk/value/value.ch`: `#define value_OBJECTDESTROYED -1` → `(-1L)`
- `src/overhead/class/lib/class.h`: `#define class_VERSIONNOTKNOWN -1`
  → `(-1L)` — same pattern; `class_VERSIONNOTKNOWN` flows through the untyped
  `(*entrypoint)()` call in `doindex.c:410` and through K&R-unprototyped
  `RetrieveByName`/`RetrieveByKey` calls in the class loader.

**Audit:** Grepped all 13 `.ch` files that define their own `NotifyObservers`
variants for plain negative `#define`s. Only `observe.ch` and `value.ch` had
the `OBJECTDESTROYED (-1)` pattern. `ness.ch`'s `UNSPECIFIEDSYNTAXLEVEL -1`
is stored in a struct field (typed assignment, no truncation), not passed
through untyped dispatch — safe.

**Result:** `ez` now runs stably; checkpoint timer fires repeatedly without
crashing. Menus, character input, and object insertion all work.

## 2026-07-01 – 2026-07-02

### Xft anti-aliased text rendering (`atk/basics/x/xfontd.c`, `xgraphic.c`)

Added Xft-based anti-aliased text rendering as an overlay on top of the
existing X11 bitmap path. Body text in ez now renders with full AA using
the system's TrueType fonts; the bitmap path remains as fallback.

**`src/config/darwin/system.mcr`** — added three new entries:

```
#define HAVE_XFT 1
XFTLIB = -lXft
XFTINCDIR = $(XINCDIR)/freetype2
STD_DEFINES = -DHAVE_XFT
```

`HAVE_XFT` gates all Xft code in the tree; `XFTINCDIR` gives the compiler
FreeType2 headers needed by `<X11/Xft/Xft.h>`.

**`xfontd.c`** — added an `XftFont *xft` field to `struct fcache` (the
per-display font cache entry). On each font load, after the existing
`XLoadQueryFont` call, the XLFD is retrieved and tested: only fonts whose
XLFD charset-registry field is `iso8859*` or `iso10646*` are opened with
`XftFontOpenXlfd`. Fonts with `adobe-fontspecific` encoding (e.g.
`andysymbol`) are deliberately excluded — their byte values are Adobe
Symbol encoding, not Unicode, and Xft would misinterpret them. Added
`xfontdesc_GetXftFont()` accessor and `XftFontClose` in `FinalizeObject`.
`TextSize`, `StringSize`, and `WidthTable` remain on the X11 metric path
throughout; the widthTable is the source of truth for all layout and
cursor positioning.

**`xgraphic.c`** — added an `#ifdef HAVE_XFT` block in the `MoveTo`/draw
path. When an Xft font is available and the transfer mode is not XOR, a
temporary `XftDraw` is created and each character in the string is
rendered individually with `XftDrawString8`, clipped to its advance-width
cell via `XftDrawSetClipRectangles` before each draw.

The per-character clip is the key correctness invariant: Xft glyphs'
anti-aliased ink can bleed slightly beyond their nominal advance width
(right-side bearing overflow). Without clipping, deleting a character
erases the boundary pixels of the preceding character's glyph, visible as
corruption at end-of-paragraph. Clipping each glyph to its advance cell
makes AA rendering behave like a bitmap font from AUIS's perspective —
incremental erase-and-redraw stays correct. The visual cost is that the
1–2px anti-aliased fringe at each glyph's right edge is clipped; this is
imperceptible at body text sizes and far preferable to editing artifacts.

Each character is placed at its X11 widthTable-derived X position (not
Xft's own advance), so cursor movement, selection, and deletion — which
all use widthTable for coordinate computation — remain consistent with
what is rendered on screen. The spaceShim is added to space characters to
preserve AUIS's justified-text spacing.

**Menu Xft work not included**: an attempt to extend Xft to menu text was
made and reverted. Menu rendering in `cmenu/cmdraw.c` and `menubar.c`
involves per-line measurement and layout that did not converge; those
files were restored to their pre-Xft state.

### Symbola font pipeline (bullets, math symbols)

AUIS documents use `andysymbola` for bullet and worm list markers. This
maps through `fonts.alias` to `symbola*`, which must be resolved from
`symba*.pcf` files compiled from AUIS's own `symba*.fdb` sources.

**`src/config/darwin/system.mcr`** — added `#define ISO80_FONTS_ENV 1`.
Under this flag, `xmkfontd/Imakefile` installs `non-andrew.fonts.alias`
as `build/X11fonts/fonts.alias` instead of running `mkfntdir.csh`. The
alias file maps all `andy*` logical font names to XQuartz scalable fonts
and includes `andysymbola8..22 → symbola8..22`.

**`src/overhead/fonts/fonts/adobe/Imakefile`** — `DeclareFont(symba8)`
through `DeclareFont(symba22)` were placed outside the
`#ifndef ISO80_FONTS_ENV` guard so the PCF files are built even when the
main Andrew bitmap fonts are suppressed.

**Build pipeline:** `symba*.fdb` → `fdbbdf` → BDF → `bdftopcf` →
`symba*.pcf`. `mkfontdir` registers them as `symbola*` (from the `FONT
SymbolA10` field in the BDF headers). The alias chain is then complete:
`andysymbola10` → `symbola10` (fonts.alias) → `symba10.pcf` (fonts.dir).

### Font build infrastructure (`config/darwin/system.mcr`)

The default from `allsys.mcr`:

```
XFC        = $(XBASEDIR)/$(XBINDIR)/bdftopcf
XMKFONTDIR = $(XBASEDIR)/$(XBINDIR)/mkfontdir
```

resolved to `/opt/X11//usr/bin/X11/bdftopcf` on Darwin — a nonexistent
path — because `XBINDIR` was never overridden from its default
`/usr/bin/X11`. On XQuartz, these tools live at `/opt/X11/bin/`.

Fixed by adding to `darwin/system.mcr`:

```
XBINDIR = bin
```

This single override corrects both `XFC` and `XMKFONTDIR` in every
generated Makefile. `FONTS_TO_PCF_ENV` is already defined globally in
`allsys.h` (PCF is the modern default); this fix only addresses the tool
path, not the format selection.

`make Makefiles` from the top of the source tree propagated `XBINDIR = bin`
to all font-building directories (`overhead/fonts/fonts/adobe/`,
`atk/eq/`, `atk/value/`, `atk/table/`, `xmkfontd/`).

### `fonts.alias` install mode (`xmkfontd/Imakefile`)

`fonts.alias` was being installed with `${INSTINCFLAGS}` (mode `0444`,
read-only). On the second `make dependInstall`, `install` could not
overwrite the existing read-only file, silently leaving a stale alias
table. Changed to `${INSTLIBFLAGS}` (mode `0664`).

### Post-install X server steps (not automatable)

After `make dependInstall`, the running X server must be told about the
font directory before `ez` can use the new fonts:

```sh
xset fp+ /path/to/andrew-6.4/build/X11fonts
xset fp rehash
```

These steps cannot be part of the build; they belong in `.xinitrc` or
equivalent session startup.

## 2026-07-01

### LP64 `%d`→`%ld` fixes in raster readers (`raster.c`, `rasterio.c`)

**Symptom:** raster images in `src/contrib/mit/neos/doc/history.ez` were
rendered as raw pixel data text instead of images. The document also
truncated early and hung on "go to end of file".

**Root cause (two-stage):**

`raster__Read()` (`src/atk/raster/cmd/raster.c`) and
`rasterio__ReadImage()` (`src/atk/raster/lib/rasterio.c`) declare their
`objectid`, `width`, `height`, and `version` variables as `long`, but
all their `fscanf` calls used `%d`/`%x` format specifiers. On
LP64/arm64 `%d` writes only 4 bytes into an 8-byte `long`, leaving the
upper 4 bytes as whatever stack garbage was present.

`history.ez` contains a `\begindata{zip,...}` inset *before* the first
raster. `zip` falls through to `unknown__Read()`, which calls deeply
into the text subsystem, extensively dirtying stack frames. When
`raster__Read()` is then entered, the garbage upper bytes in `width`
and `height` cause the range check `if (width > 1000000 || height >
1000000)` to fire (raster.c:662). The resulting `break` exits the
`'b'` case without reaching the `\enddata{raster,...}` scan at lines
686–691.

With the enddata scan skipped, all 405 pixel-data rows plus the
`\enddata{raster,...}` line remain in the file stream. The text reader
consumes them as literal characters. When it hits `\enddata{raster,...}`,
`endcount` drops to 0, terminating text reading for the entire document.
The remaining document content is never read; unclosed style environments
cause a hang on "go to end".

**Fix:** changed all `%d`/`%x` specifiers paired with `long *` arguments
to `%ld`/`%lx`. Seven edits across two files:

`src/atk/raster/cmd/raster.c`:
- line 610: `fscanf(…, " %ld ", &objectid)`
- line 624: `fscanf(…, " %ld %lx ", &pid, &addr)`
- line 639: `fscanf(…, "%ld %255s %1023s ", &objectid, …)`
- line 660: `fscanf(…, " %ld %ld %ld ", &objectid, &width, &height)`

`src/atk/raster/lib/rasterio.c`:
- line 374: `fscanf(…, " %ld ", &version)`
- line 391: `fscanf(…, " %ld %ld %ld ", &objectid, &width, &height)`
- line 414: `fscanf(…, "enddata{raster,%ld", &discardid)`

Documents that never triggered the bug (e.g. `NEOS_stud.ez`) have
smaller raster IDs and no complex inset preceding the first raster, so
their stack frames were clean when `raster__Read()` was entered and the
garbage upper bytes happened to be zero — luck, not correctness.

**Broader audit:** grep across all `.c` files under `src/` for `%d`
paired with `long *` found additional candidates in `layout.c`,
`label.c`, `nesst.c`, `xbm.c`, `print.c`, `annot/icon.c`, `annot/ps.c`,
`tm.c`, `txttroff.c`, and `2rtf` — not fixed here, need individual type
verification before changing.
