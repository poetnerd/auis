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

Fixed last 3 compilation errors: missing `<stdlib.h>` in
`overhead/util/hdrs/genhdr.c` and missing `<stdlib.h>`/`<string.h>`
in `overhead/image/tiff/mkg3states.c`.

Install phase incomplete — `.ih` headers and binaries not fully
populating `build/`. Likely related to `makedo`/`doindex` toolchain
(the dynamic object packaging layer) not being built for Darwin.

`make Makefiles` generates Makefiles recursively throughout the tree.
`make` in `overhead/class/pp` builds and links the class preprocessor.
