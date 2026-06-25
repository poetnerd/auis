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
