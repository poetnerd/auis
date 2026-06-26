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

### Remaining compile errors

~15 files across overhead and ATK fail with the same patterns: missing
`<stdlib.h>`/`<string.h>` includes and implicit-int declarations. These
are not architectural — just individual file fixes. Once resolved, the
full `make dependInstall` will populate `build/dlib/atk/` with `.do`
files and `build/include/atk/` with `.ih` headers.
