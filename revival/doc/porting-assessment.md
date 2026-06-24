# Porting Assessment: AUIS 6.3.1 on Modern Linux

An assessment of what it would take to build the AUIS 6.3.1 C codebase
on a current Linux distribution.

## Good news: Linux support already exists

The 6.3.1 tree includes a Linux port in `config/i386_Linux/` with
`system.h` and `system.mcr`. The `platform.tmpl` wires it in via
`#if defined(i386) && defined(linux)`. The port uses gcc, flex, POSIX
calls, and standard X11 paths. The `system.h` is only 137 lines — mostly
mapping AUIS's `osi_` portability wrappers to standard POSIX functions.

## Build system

AUIS uses imake (from X11) driven by `Imakefile`s, `platform.tmpl`,
`allsys.mcr`, per-platform `system.mcr`, and `site.mcr`/`site.h` for
local customization. imake still exists in most distributions
(package `xutils-dev` on Debian/Ubuntu). A CMake or plain Makefile
migration might be worthwhile eventually but isn't necessary to get started.

## Issues to address

### 1. `gcc -fwritable-strings` (HIGH effort)

The Linux `system.mcr` specifies `CC = gcc -fwritable-strings`. This flag
was removed from gcc in version 4.0 (2005). It allowed code to modify
string literal contents in place, e.g.:

```c
char *p = "hello";
p[0] = 'H';  /* undefined behavior without the flag */
```

This implies such patterns exist throughout the codebase. Finding and
fixing all instances is tedious but mechanical — change string literals
to `char[]` arrays or allocate writable copies. A modern compiler will
warn or crash on the unfixed ones, so they're findable.

### 2. glibc `FILE` struct internals (LOW effort)

```c
#define FILE_HAS_IO(f) ((f)->_IO_read_end - (f)->_IO_read_ptr)
```

This reaches into glibc's internal `FILE` struct layout, which has changed.
Modern glibc hides these fields. Replace with a portable alternative or
remove the optimization (it's a buffering check).

Similarly, `FILE_NEEDS_FLUSH` is defined to always return 1, which is
already the safe/portable behavior.

### 3. Dynamic object loader (MEDIUM effort, but a simplification)

The `overhead/class/` directory contains AUIS's custom dynamic loader for
`.do` (dynamic object) files. This was the hardest part of every port —
it had to understand each platform's object file format.

Modern replacement: `dlopen()`/`dlsym()`/`dlclose()` from `<dlfcn.h>`,
which is standard POSIX and works everywhere. The `.do` files become
standard `.so` shared objects. This is actually a simplification over
the original, but touches the core of the system — every inset is loaded
through this machinery.

Note: `genstatl` already exists for building without dynamic loading.
A static-linked build could be the first milestone, deferring the
`dlopen()` migration.

### 4. Andrew custom malloc (LOW effort)

`ANDREW_MALLOC_ENV` is defined by default. This custom allocator may
conflict with modern allocators and address space layout randomization.
Can be disabled by `#undef ANDREW_MALLOC_ENV` in `site.h`.

### 5. Platform configuration cleanup (LOW effort)

The `config/` directory has 30+ platform directories (VAX, RT, Apollo,
Mac II, NeXT, etc.) and `platform.tmpl` is a 220-line cascade of
`#ifdef` blocks for all of them. For the revival, strip everything
except the Linux path. This is just cleanup, not a porting problem.

### 6. X11 paths and libraries (LOW effort)

The `system.mcr` hardcodes:
```
XUTILDIR = /usr/bin/X11
XLIBDIR = /usr/lib
XLIB = -L$(XLIBDIR) -lX11
```

Modern Linux puts X11 in `/usr/include/X11`, `/usr/lib/x86_64-linux-gnu`,
etc. Fix the paths in `site.mcr` or update the defaults. The X11 API
itself hasn't changed in the ways that matter — Xlib is remarkably stable.

### 7. Console/stats module (LOW priority, defer)

`atk/console/stats/i386_Linux/` contains platform-specific code for
reading system statistics from `/dev/kmem` and `/proc`. The kernel
interfaces have changed completely. This module is not essential —
defer or disable it.

### 8. Misc POSIX drift (LOW-MEDIUM effort)

- `setreuid(r,e)` is mapped to `setuid(r)` — modern Linux has `setreuid()`
- `osi_vfork()` maps to `fork()` — fine, vfork() is deprecated anyway
- `_setjmp`/`_longjmp` — still available but may need review
- `FNDELAY` mapped to `O_NONBLOCK` — correct
- `getwd()` mapped to `getcwd()` — correct
- `NDEBUG` is forced on to work around a missing `___eprintf()` in shared
  libs — this is long since fixed in modern glibc, remove the `#define`

## Recommended build-up strategy

### Phase 1: Static build of core ATK (smallest surface)

Disable in `site.h`:
```c
#undef AMS_ENV           /* skip mail system entirely */
#undef ANDREW_MALLOC_ENV /* use system malloc */
```

Build target: `overhead/` (minus class dynamic loader) + `atk/basics/` +
`atk/text/` + `atk/support/`. Use `genstatl` for static linking.
Goal: get the class preprocessor and core text objects compiling.

### Phase 2: ez running

Add `atk/ez/`, `atk/frame/`, `atk/supportviews/`, `atk/textaux/`,
`atk/textobjects/`. Goal: ez starts and can edit a document.

### Phase 3: Dynamic loading via dlopen()

Replace the `overhead/class/` loader with a `dlopen()` wrapper.
Insets become standard `.so` files. Goal: ez can load insets dynamically.

### Phase 4: Additional insets and applications

Bring up figure, raster, table, help, etc. one at a time.
Each is relatively independent once dynamic loading works.

### Phase 5 (optional): AMS and other subsystems

The Andrew Message System, if desired. This has its own large set of
dependencies (mail delivery, white pages, etc.) and could reasonably
be left for much later or not at all.

### 9. Font system (HIGH effort, but deferrable)

AUIS uses the X core font protocol — server-side bitmap font rendering
with XLFD naming, custom "Andy" bitmap fonts (BDF format), and integer
glyph metrics. The entire font landscape has changed:

- Modern Linux uses client-side rendering: FreeType + fontconfig + Xft
- Fonts are TrueType/OpenType, not BDF bitmaps
- Anti-aliasing and subpixel rendering are expected
- Unicode and complex text layout (Pango/HarfBuzz) are standard

Key source files to investigate (once available):
- `atk/basics/` — font abstraction, `xgraphic.c`, `xfontd.c`
- `atk/text/` — text measurement and drawing
- `atk/support/` — style and font selection
- `overhead/fonts/` — the Andy bitmap font distribution

The X core font protocol still works on modern X11. A pragmatic approach:
get ez running with bitmap fonts first (functional but ugly), then migrate
to Xft/fontconfig as a separate phase. The critical question is how well
the font abstraction in `atk/basics/` isolates the rest of the system
from X font specifics.

Key questions to investigate once the full source is available:
- How thick is the font abstraction in `atk/basics/`?
- Where do XLFD strings and bitmap metrics leak out into the text and
  view code?
- What does the drawing path look like:
  `text object → view → graphic → X calls`?
- How separable is the print rendering from the screen rendering?

### 10. Messages with IMAP backend (UNKNOWN effort, needs investigation)

The `messages` application is the UI for mail and bulletin boards. It
sits on top of AMS, which implements its own storage, delivery, and
locking model based on shared filesystems (AFS). The question is whether
`messages` can be separated from AMS and connected to an IMAP server.

Key source areas to investigate once the full source is available:
- `atkams/` — the bridge between ATK and AMS; how thick is this interface?
- `ams/` — where does the storage abstraction live, and is there one?
- `atk/ez/` and the messages application — does the UI talk to AMS
  directly, or through a clean API boundary?
- What assumptions does `messages` make about the message store?
  (e.g., local files, specific directory structures, AFS locking
  primitives, white pages integration)
- How much of AMS is delivery/transport (replaceable by SMTP) vs.
  storage/retrieval (replaceable by IMAP) vs. tightly coupled to both?

The value proposition is significant: a mail client that renders rich
compound documents inline with embedded ATK objects. But the feasibility
depends entirely on whether there's a seam between the UI and the store.
Previous experience suggests AMS internals are deeply complex — approach
with caution and investigate the interface boundaries before committing.

## Primary build environment: macOS/Darwin

The initial development platform is macOS (POSIX Darwin), not Linux.

### X11 on macOS

XQuartz is the X11 server for macOS (formerly X11.app). Available at
xquartz.org or via `brew install --cask xquartz`. It works well but is
a separate install that needs to be set up.

### No macOS platform config in 6.3.1

There is no `config/darwin/` or `config/macos/` in the original tree.
The closest starting points for a new `system.h` and `system.mcr`:
- `config/i386_bsd/` — Darwin's userland descends from FreeBSD/NetBSD
- `config/next_mach20/` — macOS descends from NeXTSTEP/Mach

Either way, a new platform config will be needed.

### Apple clang vs gcc

macOS's `gcc` is actually clang in disguise. The `-fwritable-strings`
problem still applies but diagnostics will differ. Real gcc is available
via `brew install gcc` if needed.

### Mach-O vs ELF

macOS uses Mach-O object format, not ELF. This affects the dynamic loader
replacement: `dlopen()` works on macOS but shared objects are `.dylib`
not `.so`, and linking flags differ (`-dynamiclib` instead of `-shared`).

### Source control

The revival codebase will be managed under Fossil SCM, not git.

## Archive fetch: missing files (404)

The following files were not available when the archive was mirrored from
CMU on 2026-06-24. None are AUIS source code.

```
web/amz.html                          — web page (not source)
misc/FACTS.andrewis.1.gif             — brochure illustration
FACTS/EZ/FACTS.andrewis.1.gif         — duplicate reference to same
andrew-8.0/WWW/Protocols/HTTP/Methods.html
andrew-8.0/WWW/Library/Implementation/HTEpToClient.c
andrew-8.0/WWW/Protocols/rfc1341/5_Content-Transfer-Encoding.html
andrew-8.0/WWW/Daemon/Inplementation/HTSUtils.c
andrew-8.0/WWW/Library/Implementation/HTMLDTD.c
andrew-8.0/WWW/Library/Implementation/HTStream.c
andrew-8.0/WWW/Daemon/WAISGate.html
```

The `andrew-8.0/WWW/` files are bundled copies of early CERN/W3C libwww
source and HTTP protocol documentation — part of the v8.0 web browser
support, not original AUIS code. Not relevant to the 6.3.1 revival.

## Sources

- `config/i386_Linux/system.h` — the 6.3.1 Linux system definitions
- `config/i386_Linux/system.mcr` — the 6.3.1 Linux build macros
- `config/allsys.h` — system-independent feature flags
- `config/platform.tmpl` — platform detection and dispatch
- `ANNOUNCE/ANNOUNCE.6.2.changes` — documents POSIX work and `genstatl`
