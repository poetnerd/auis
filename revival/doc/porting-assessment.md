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

## Strategic decision: compiler leniency over wholesale modernization

Early in the revival, we tried mass-converting the K&R C source to ANSI C
(explicit prototypes, typed parameters) using an automated tool
(`revival/tools/modernize`). This was unreliable at scale — edge cases
in K&R parsing (multi-name declarations, function pointers, split-line
definitions) caused the tool to silently introduce bugs across hundreds
of files. A single mass-modernization pass took the error count from
roughly zero to over 2000.

The working strategy instead: **leave the K&R source untouched and use
compiler flags to relax modern clang/gcc's strict defaults** back to
behavior compatible with 1990s C compilers:

```
COMPILERFLAGS = -Wno-implicit-int -Wno-implicit-function-declaration \
                -Wno-incompatible-function-pointer-types
```

This took the build from ~1062 errors (after the modernizer revert) down
to ~344 — and critically, those 344 are *real* portability problems
(`sys_errlist` removed from libc, `Display`/`FILE` struct internals
hidden by modern headers) rather than self-inflicted tool damage.

**Current policy:** Do not run the modernizer across the tree. Only
hand-edit or modernize a file when:
- Build forces it (a structural incompatibility, e.g. `<a.out.h>`
  doesn't exist on Darwin) — fix narrowly, not wholesale
- We are touching that file for an unrelated reason anyway

**Follow-on effort (not now):** A deliberate, careful pass to bring the
codebase to full ANSI/POSIX C — once the system builds and runs, with
test coverage or working application behavior to validate against. At
that point, modernizing one file at a time with a working build to test
against is much safer than modernizing blind. The `modernize` tool
remains useful for that future effort, but its bugs (see ROADMAP "Known
limitations") should be fixed first.

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

This is one of the few K&R-era issues compiler flags cannot paper over —
writing into a string literal is undefined behavior, not just a stricter
diagnostic. Per the strategic decision above, defer fixing this broadly;
address it only in files we touch for other reasons, until the follow-on
ANSI/POSIX modernization effort.

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

### 7a. Deferred: legacy sgtty-based terminal clients — tm, vui (LOW priority, defer)

`contrib/tm` and `ams/msclients/vui` are curses-style terminal mail clients
built in part on the pre-POSIX BSD `sgtty` tty API (`TIOCGETP`, `TIOCREMOTE`,
`struct sgttyb`, `CBREAK`/`RAW`/`CRMOD` modes). That ioctl interface was
removed from the kernel decades ago (macOS keeps the `#define`s in
`<sys/ioctl_compat.h>` for source compatibility only — the ioctls themselves
are gone), so even a clean compile wouldn't produce a client with working raw
terminal input. Making these actually function means rewriting the tty layer
to use `termios` (`tcgetattr`/`tcsetattr`, `cfmakeraw`), which is a real
porting project in its own right, not a mechanical fix. `vui` additionally
calls curses `raw()`/termcap globals (`CM`, `SO`, ...) that modern ncurses
no longer exports directly — a separate curses-port problem on top of the
sgtty one.

`messages` (the GUI ez client, built on `atkams/`) is the primary
destination for mail in this revival, so `tm`/`vui` are low priority. They
remain conditionalized out of the build rather than patched to merely
compile:

- `contrib/Imakefile`: `TM` still requires `#define MK_TM` (was unconditional
  except on SGI)
- `ams/msclients/Imakefile`: `VUI` still requires `#define MK_VUI` (was
  unconditional)

Revisit as a dedicated termios/curses-port task if a terminal-based mail
client is ever wanted alongside `messages`.

**`ams/msclients/cui` turned out not to belong on this list.** It was
originally grouped here on the assumption that it shared `vui`/`tm`'s sgtty
dependency. In fact `cui` doesn't use curses at all (only `vui` does), and
its one BSD-sgtty reference — a `#ifdef POSIX_ENV`/`#else` fallback in
`GetBodyFromCUID()`, under the rarely-built `METAMAIL_ENV` — was already dead
code on this platform: `POSIX_ENV` is unconditionally defined in
`config/darwin/system.h`, so the `termios` branch was the one actually
compiling. `cui` was still failing to build, but for an unrelated reason —
its `Imakefile` never got the `${RESOLVER_LIB}` link fix that `nns` received
on 2026-07-05 (§ above). Fixed 2026-07-07: `${RESOLVER_LIB}` added to
`ams/msclients/cui/Imakefile`'s `ProgramTarget` lines; `cuin` now compiles,
links, and installs cleanly. `MK_CUI` is enabled in `config/site.h`. Full
detail in `porting-changelog.md`'s 2026-07-07 entry.

### 7b. Deferred: contrib/bdffont (LOW priority, defer)

`contrib/bdffont`'s parser splits bison's output across two files: a
generated `bdfparse.tab.c` plus a hand-maintained `bdfparse.act`
containing the grammar's C action bodies (`#include`d separately at
`bdffont.c`). `bdfparse.act` does not exist anywhere in the source tree —
no fossil history, no Imakefile rule that generates it. `overhead/mkparser`
is a working, already-fixed tool for a related scheme, but it emits one
merged `prefix.c`/`prefix.h`, not this split `.tab.c`+`.act` convention, so
it doesn't apply here. Reconstructing `bdfparse.act` means hand-writing the
parser's semantic actions from `bdfparse.y`'s grammar with nothing to
verify against — exploratory reverse-engineering, not a mechanical fix.

(The same broken split-file convention also appears in `atk/ness/type`,
`atk/ness/objects`, and `atk/syntax/parse`'s `testparse` test target, but
none of those are currently reachable: `atk/ness` requires
`MK_NESS`/`MK_AUTHORING`, which isn't defined, and `testparse` isn't part
of `make install`. `bdffont` was the only one actually blocking the
build, since `contrib/Imakefile` listed it unconditionally.)

Conditionalized out of the build rather than patched to merely compile:

- `contrib/Imakefile`: `BDFFONT` now requires `#define MK_BDFFONT` (was
  unconditional)

Revisit if `bdfparse.act` can be recovered from an original CMU
distribution, or if someone is willing to hand-write it against the
grammar.

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

### 9. Font system (in progress as of 2026-07)

AUIS was written for the X core font protocol — server-side bitmap font
rendering with XLFD naming, custom "Andy" bitmap fonts (BDF/PCF format),
and integer glyph metrics. The revival is migrating to client-side Xft
rendering in phases, while retaining the Andy symbol fonts for characters
that have no standard substitute.

#### What was found

The build tree includes 40 compiled PCF fonts in `build/X11fonts/`. The
`fonts.alias` file in that directory maps all Andy text font names
(`andysans*`, `andytype*`) to standard Adobe Helvetica/Courier XLFD names
already present in XQuartz — so no custom Andy text bitmaps need to be
installed separately for text rendering. The symbol fonts (`symba*.pcf`,
five sizes: 8, 10, 12, 16, 22 point) are CMU-custom with no standard
substitute; without them, bullet characters render as `7`.

Text rendering quality with the original X core path (before Xft) was
surprisingly good — font appearance was correct, and bold/italic/size
changes all worked properly. The visible rendering problem was a frame-size
reporting bug in the `help` application causing text to appear clipped at
the right margin; this was unrelated to fonts.

#### Current approach: hybrid Xft + X core

Two rendering paths exist in the revival; Xft is being introduced
conditionally:

| Rendering path | Status |
|---|---|
| Body text | Migrated to Xft (2026-07, phase 1 complete) |
| Menus | Xft migration in progress (2026-07, phase 2) |
| Symbol characters (bullets, math) | Andy `symba*.pcf` via X core — permanent |
| Cursor shapes | Andy cursor PCF via X core — permanent |

Because the symbol and cursor fonts have no Xft/fontconfig equivalent,
`xset fp+ build/X11fonts && xset fp rehash` remains a required setup step
even after the Xft text migration is complete. See `quickstart.md` for the
exact invocation.

#### Key source files

- `atk/basics/x/xgraphic.c`, `xfontd.c` — font abstraction and X drawing
- `atk/text/` — text measurement and drawing
- `atk/support/` — style and font selection
- `build/X11fonts/fonts.alias` — Andy→Adobe XLFD name mappings

### 11. `%d` / `%ld` mismatch in scanf family (MEDIUM effort, systemic)

AUIS was written for ILP32 platforms where `sizeof(int) == sizeof(long)
== 4`. Throughout the codebase, `long` variables used for sizes, IDs, and
dimensions are read with `%d` format specifiers in `scanf`/`fscanf`/
`sscanf`. On LP64 arm64, `long` is 64 bits but `%d` tells `fscanf` to
write only 32 bits — the upper 32 bits of each `long` remain as stack
garbage.

Unlike `printf` mismatches (wrong output, no memory write), `scanf`
mismatches corrupt the stack frame: dimensions come back as plausibly-
small values when garbage upper bits happen to be zero, and as wildly
wrong large values otherwise, causing crashes or corrupt rendering.

**Known instances (confirmed 2026-07-02):**
- `atk/raster/lib/raster.c:660` — `long version, width, height` read
  with `%d %d %d`
- `atk/raster/lib/rasterio.c:391` — identical pattern

**Fix:** Change `%d` to `%ld` for `long *` arguments. Also initialize
`long` locals to `0` before the `fscanf` so a partial write leaves `0`
in the upper bytes rather than stack garbage.

**To audit the full tree:**
```
grep -rn 'fscanf\|sscanf\|scanf' src/ --include="*.c" | grep '%d'
```
For each hit, verify whether the corresponding argument is `int *` (correct)
or `long *` (needs `%ld`). This pattern is likely widespread given the
codebase's ILP32 heritage; every graphical inset that reads dimension or
ID values from a file is a candidate.

This is LP64 variant #4, distinct from the three fixed earlier:
- #1 Undeclared function → implicit `int` return, pointer truncated
- #2 >8-arg call through `(void(*)())` → stack argument dropped
- #3 `int` constant through untyped dispatch → zero-extended, comparison fails

### 12. LP64 untyped dispatch: `long` parameter / `int` argument mismatch (MEDIUM effort, systemic)

#### Root cause

The ATK class system generates method dispatch macros of the form:

```c
#define lpair_Init(self,l1,l2,x) \
    ((* ((void (*)())((self)->header.lpair_methods->routines[59]))) (self,l1,l2,x))
```

Every virtual method call goes through an untyped `void (*)()` cast. Because
the compiler sees no parameter types at the call site, it cannot insert the
sign-extension or zero-extension instructions it would emit for a typed call.

On arm64 (LP64), when an `int` value of `-N` is passed through such a dispatch
to a function that declares the receiving parameter as `long`:

- The caller stores the 32-bit value in a register: `0xFFFFFFE7` (for -25)
- The upper 32 bits of the 64-bit register are **zero**, not sign-extended
- The callee reads a `long` from that register: `0x00000000FFFFFFFFE7` = **4,294,967,271** (positive)

The net effect: any function that (a) is dispatched through the class vtable,
(b) declares a parameter as `long`, and (c) is called with a negative `int`
expression will receive a large positive value instead of the intended negative
one. Sign-dependent logic (`if (x < 0)`) silently takes the wrong branch.

This is LP64 variant #3 (extended). Earlier LP64 variants:
- **#1** Undeclared pointer-returning function → implicit `int` return → pointer truncated to 32 bits
- **#2** >8-arg call through `void (*)()` → 9th+ args spilled to stack with wrong ABI, dropped
- **#3** `int` constant through untyped dispatch → zero-extended, sign-dependent branch fails
- **#4** `%d` with `long *` in scanf → only 32 bits written, upper 32 bits garbage (see §11)

#### Confirmed instances (as of 2026-07-02)

| File | Function | Parameter | Effect when wrong |
|---|---|---|---|
| `atk/supportviews/lpair.c` | `lpair__Init` | `long x` receiving `int -MAINPCT` | calls `VFixed` instead of `VSplit`; PERCENTAGE→BOTTOMFIXED; panel gets 0 width |
| `atk/textobjects/panel.c` | call to `style__SetNewIndentation` | `Operand` declared `long`, called with `int -16384` | indentation becomes +4 billion units; all panel text rendered off-screen |

The `lpair__Init` fix changed the parameter declaration from `long` to `int`.
The `panel.c` fix cast the literal to `(long)-16384` at the call site.

#### New sub-variant (2026-07-05): mismatch isn't limited to dispatch-macro call sites

`CUI_GetHeaders` (`ams/libs/cui/cuilib.c`, reached from `atkams/messages/lib`'s
`captions.c`/`capaux.c` via the `ams_CUI_GetHeaders` class dispatch) showed the
same root mechanism firing across a **plain unprototyped C function call**,
not just the `void (*)()` vtable macros — worth recording since it means the
grep-for-dispatch-macros audit query in this section won't find every
instance:

- **By-value case**: `startbyte` was `long` in `cuilib.c`'s `CUI_GetHeaders`
  and in the already-`long` `ams/libs/snap/cuisnap.c` (dormant SNAP-networked
  variant), but `int` in the `.ch` class interfaces (`ams.ch`/`amsn.ch`/
  `amss.ch`), their `.c` implementations, and the real caller
  (`atkams/messages/lib/capaux.c`'s `totalbytes`, accumulated across a header-
  read loop in `InsertUpdatesInDocument`). Register-garbage-dependent — worked
  on one host, segfaulted immediately on a second host (`spoon`) with the
  same fossil checkout, confirming the "upper 32 bits are whatever was there
  before" mechanism rather than a deterministic value bug.
- **By-pointer case (new, more dangerous)**: `nbytes`/`status` had the *same*
  int-vs-long mismatch, but as pointer types (`int *` vs `long *`) rather than
  by-value. `ms/libs/ms/headers.c`'s `MS_HeadersSince` (the actual local-
  mailbox implementation that writes through these pointers) declared
  `int *numbytes, *bytesleft`; everything above it in the active call chain
  read them back as `long *`. This isn't just a misread value — a write
  through the narrower-than-expected type only fills half the register-width
  the reader expects, so the reader picks up genuine adjacent-memory garbage
  in the upper bits, same failure mode as the by-value case, but the
  underlying hazard (writer and reader disagreeing on a pointer's pointee
  size) is the more serious member of this family: if the size relationship
  were reversed (writer wider than the true allocation), it would be an
  actual out-of-bounds write, not just a garbage read. `ams/libs/snap/cuisnap.c`
  already had the correct `long *` throughout, confirming `long` was always
  the intended type and `headers.c` was the file that never got updated.

Fixed by widening the `int`/`int *` side to `long`/`long *` everywhere in the
active chain (`headers.c`, `.ch` interfaces, `.c` implementations, `capaux.c`,
`foldaux.c`), plus the dormant SNAP-server side (`ams/ms/ms.c`) for
forward-compatibility. Full file list in `porting-changelog.md`'s 2026-07-05
entries.

#### Strategic options

**Option A — Fix the dispatch mechanism**: Change the generated `.ih` macros
from `void (*)()` to properly typed function pointers. Correct in principle;
requires modifying the class preprocessor in `overhead/class/`, regenerating
all `.ih` files, and careful verification across the whole system. High risk,
high reward.

**Option B — Systematic parameter audit**: Identify all vtable methods whose
implementations declare `long` parameters, then audit call sites for negative
`int` arguments. Fix either by changing `long → int` in the implementation
(appropriate for values that will never exceed 32-bit range: pixel sizes,
percentages, style margins) or by casting to `(long)` at the call site
(appropriate when the value genuinely needs 64-bit range). This is mechanical
and can be driven by grep.

**Option C — Fix on contact** (current approach): Fix each instance as it
manifests as a visual or runtime bug. Low risk per fix, high whack-a-mole
factor.

#### Recommendation

**Option B — systematic audit** is the right next step after the immediate
runtime issues are stable. It is a one-time effort that eliminates the
entire class of bugs without the risk of touching the dispatch mechanism.

The audit query:

```sh
# Find functions with 'long' parameter declarations (K&R style)
grep -rn "^long\b" src/atk/supportviews/lpair.c src/atk/support/style.c \
    src/atk/text/ src/atk/textobjects/ src/atk/supportviews/ \
    src/atk/frame/ src/atk/basics/

# Cross-reference against vtable method list in each class's .ih
# Focus on parameters that are passed negative int literals at any call site
grep -rn "lpair_Init\|lpair_VSplit\|style_SetNew" src/ --include="*.c" | grep '\-[0-9]'
```

For each hit: if the parameter is used only for small integers (sizes in
pixels or percent, style units), `long → int` in the implementation is
the right fix. If it needs 64-bit range (file offsets, text positions,
accumulated sizes), keep `long` and cast at the call site instead.

### 10. Messages with IMAP backend (UNKNOWN effort, needs investigation)

**Resolved 2026-07-04 for the local-store case — see `roadmap.md` Near-term →
Messages application prerequisites, Stream 2/3.** The build already has a
clean seam: `AMS_ENV` on with `AMS_DELIVERY_ENV`/`SNAP_ENV`/`WHITEPAGES_ENV`
left off builds `messages` against a local, non-networked mbox-backed
message store (`ams/libs/ms`), with none of the AFS/AMDS delivery machinery
involved. An IMAP adapter remains a viable fallback (notes below still
apply to that scenario) but is no longer the near-term plan.

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

### Vendored bison (Andrew Bison A2.6) disabled on Darwin/arm64

`overhead/bison/` bundles CMU's own bison fork (derived from GNU Bison
1.24) so that `mkparser` can post-process its output into Andrew's
shared-object-code parser runtime. On Darwin/arm64 the built binary
hangs in an uninterruptible kernel wait when run on real grammars —
not killable even with `kill -9`. Not worth chasing; the vendored
bison's own README (2002) already recommended moving to stock FSF
bison. `overhead/bison/Imakefile` now builds it (for reference) but
does not install it, so the system `bison` is used instead. Required
one matching fix: `config/andrew.rls`'s `Parser()` macro now passes
`-o classname.tab.c` explicitly, since modern bison derives output
names from the input extension and AUIS's grammars use the nonstandard
`.gra` extension. See `porting-changelog.md` (2026-06-29) for the full
investigation. One grammar, `atk/ness/objects/ness.gra`, uses the fork's
multi-character-string-token extension and isn't yet handled.

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
