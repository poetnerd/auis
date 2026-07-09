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

**Follow-on effort (now underway):** A deliberate, careful pass to bring
the codebase to full ANSI/POSIX C — the working runtime baseline this
paragraph was waiting for now exists. Assessed 2026-07-08; full plan in
§14. Note the tool verdict changed: `modernize`'s regex K&R converter is
*not* the vehicle for that pass (§14 explains why), so its "known
limitations" are moot rather than a to-fix list. Leniency flags remain in
force per subtree until that subtree is converted and its flags ratcheted
to errors.

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

**Update 2026-07-08: superseded by the ANSI conversion plan (§14).**
Option A is milestone M1 there — extended beyond typed dispatch casts to
typed `.eh` prototypes, both generated from the `.ch` signatures classpp
already parses. With types emitted by the generator, Option B's audit
becomes a set of located compile errors rather than a grep exercise.

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

### 13. Modern flex generator/init-flag polarity mismatch (LOW effort, closed 2026-07-07)

#### Root cause

Several subsystems embed a flex-generated lexer alongside hand-written C
that calls back into flex's generated internals directly, rather than
through flex's public API. Two of these (`overhead/mail/lib/parsel.flex`,
`overhead/eli/lib/elil.flex`) define a small `reset_lexer`-style function,
called before every parse to force the scanner to discard state and start
fresh on new input:

```c
int pareset_lexer()
{
  yy_init = 1;
}
```

`yy_init` is not part of flex's public interface — it's a private
implementation detail of the generated scanner, and its *meaning* changed
between the flex version this code was written against (circa 1994) and
modern flex (2.6.x, what ships on macOS/Homebrew today):

- **Old flex:** `yy_init` nonzero means "please (re)initialize on next call."
- **Modern flex:** `yy_init` nonzero means "already initialized, buffer
  exists, skip setup."

Neither `parsel.c` nor `elil.c` has fossil history — both are regenerated
at build time from their `.flex` source via `FlexOrLexFileRule`/
`LexWithReplacement`, so this build's use of a modern flex silently changed
the behavior of code that hadn't been touched in 30 years. Forcing
`yy_init = 1` before the *first-ever* lex call in a process now makes the
generated `yylex()` skip creating its scan buffer entirely, leaving the
static buffer-position pointer at NULL. The first character read or write
of the very first parse in the process dereferences that NULL pointer.

#### Confirmed instances (2026-07-07)

| File | Caller | Symptom |
|---|---|---|
| `overhead/mail/lib/parsel.flex` (`pareset_lexer`) | `parseadd.c ParseAddressList`, called from `BuildCaption`/`MS_ReconstructDirectory` | `cui`'s `recon` command (used by `gendemo`) segfaulted on the very first address-caption build |
| `overhead/eli/lib/elil.flex` (`reset_lexer`) | `eliy.gra`, ELI/FLAMES filter-language parser | not yet observed as a runtime crash (no currently-built code path reliably exercises it — see below), but structurally identical and pre-emptively fixed |

Fix: replace the direct flag poke with flex's actual public, version-stable
API, `yyrestart(yyin)` (renamed by each file's build-time sed step to
`mail_parseyyrestart(mail_parseyyin)` / `eliyyrestart(eliyyin)`).
`yyrestart` has meant "discard current buffer, start fresh" since flex's
earliest releases — using it is strictly *more* portable across flex
versions than poking `yy_init`, not less.

#### Swept, not affected

- `doc/mkbrowse/browserpp.flex` already calls `yyrestart(yyin)` correctly.
- `overhead/class/pp/classpp.flex` has no reset-lexer pattern at all — the
  class preprocessor lexes exactly once per invocation, so the bug's
  precondition (reusing one process's scanner across multiple parses)
  never arises.
- A full-tree search (`andrew-6.4/`, not just `src/`) for `.flex`/`.lex`/`.ll`
  files, cross-checked against every Imakefile referencing flex/lex build
  rules, confirms these four are the *only* flex-based lexers in the tree.
  This bug class is fully swept, not just fixed where noticed.

#### Legacy-platform interaction: none

`FlexOrLexFileRule` (`config/andrew.rls`) selects between the `.flex` source
(when `FLEX_ENV` is defined — unconditional on Darwin, `config/darwin/
system.h:75-76`) and a parallel, separately-fossil-tracked `.lex` source
(for sites without flex, i.e. genuine AT&T lex). The `.lex` siblings'
equivalent functions are no-ops:

```c
int pareset_lexer() { return 0;}   /* parsel.lex */
int reset_lexer() {}                /* elil.lex */
```

Plain lex's generated scanner has no persistent multi-buffer state to reset
in the first place — the whole `yy_init` trick, and the bug in it, is
specific to flex's buffering model. The two code paths are chosen at
Imake-configuration time and cannot interact, so this fix has no bearing on
non-flex legacy builds either way.

#### Verification

Static: confirmed correct post-rename output in generated `parsel.c`/
`elil.c`; both compile clean. Dynamic: `cui`'s `recon` verified crash-free
across repeated runs after the `parsel.flex` fix. The `elil.flex` fix could
not be dynamically exercised — `bglisp` (ELI's own test REPL, and the most
direct way to drive its lexer) hangs uninterruptibly at process startup in
the sandbox used for this session, independent of any input (reproduces
with `/dev/null` on stdin) — a separate, pre-existing issue, not caused by
or diagnostic of this fix. Confidence rests on the byte-for-byte identical
mechanism and generator to the dynamically-proven `parsel.flex` fix.

### 14. ANSI C conversion plan (assessed 2026-07-08)

How to complete the conversion abandoned in June (checkin `5e57549713`,
779 files, reverted in `99fe31066c`). Analysis lives here; the ordered
work plan (milestones M1–M4) lives in `roadmap.md` → Medium-term →
ANSI C conversion.

#### Why the June mass conversion failed

Three compounding causes:

1. `modernize` *inferred* parameter types from K&R declaration blocks
   with regexes — silently mis-converts split-line definitions, macro
   types, and multi-name declarations.
2. All 779 files landed in one commit with no per-file compile gate —
   ~2000 errors arrived at once, with no way to bisect tool damage from
   real findings.
3. Converted definitions conflicted with the typeless declarations in
   generated `.eh` files.

Cause 3 is the pivotal observation: those conflicts were the compiler
correctly cross-checking two declarations of the same function — but
neither side was authoritative, so the errors were noise. Invert it: emit
the true `.ch` signature into the `.eh`, and every such conflict becomes a
located, genuine bug report.

#### Keystone: .ch files already carry full ANSI signatures

`atk/text/text.ch:46` reads `Read(FILE *file, long id) returns long;` —
classpp parses this, then throws the types away, emitting
`long text__Read();` into the `.eh` and `(void (*)())` casts into the
`.ih` dispatch macros. Emitting what it already knows gives whole-tree
type checking with zero hand edits:

- **Typed `.ih` casts** → the compiler converts arguments correctly at
  every method call site. Kills LP64 Variants 2/3/5 structurally
  (§12 Option A, extended to all methods, not just ≥9-arg).
- **Typed `.eh` prototypes** → every method *definition*, even while
  still K&R, is checked against the `.ch` truth (C89
  promoted-compatibility rule). Signature drift like the
  `CUI_GetHeaders` long/int mess (§12) becomes a compile error instead
  of a host-dependent segfault.

#### Scale

1,544 `.c` files in `src/`; ~1,301 of them contain ~13,700 K&R
definitions (same-line-name heuristic; split-line forms push the true
count toward ~15k). ~5,100 (37%) are `__` class methods — every one has
its authoritative signature in a `.ch` file, so no type inference is
needed for the entire highest-risk cohort.

#### Tool verdicts

| Tool | Verdict |
|---|---|
| `modernize` | Discard the regex K&R→ANSI core — it is what failed in June and cannot be patched into reliability. The include-adding passes are marginal; compile errors drive the same fixes more safely. |
| `fix-static-methods` | Keep as-is. Correct diagnosis (class methods need external linkage for the dispatch table and dynamic loader), narrow, line-based. |
| `fix-missing-static-decl` | Keep as-is. Idempotent, brace-depth aware, libc-collision skip list, splits multi-name declaration lists. |

Replacement is the `ansify` driver (`revival/tools/ansify`, built
2026-07-08) — a per-file pipeline, not a merge of the old code:

1. `fix-static-methods`, then `fix-missing-static-decl`
2. Class methods and class procedures (`__` names): rewrite the
   definition header by *lookup* in the signature database
   (`ansify --build-db` runs `class -D -N` over every `.ch` into
   `build/desc/`; 565/566 classes covered — the one failure is
   `contrib/atkbook/console/disk1.ch`, unresolvable superclass).
   Never inference. Implicit first parameters are supplied by
   convention (`struct CLASS *self` for methods, `struct classheader
   *classID` for classprocs). A `.ch`-vs-`.c` argument-count mismatch
   is reported as **DRIFT** and left unconverted — historically these
   are real bugs (§12's `CUI_GetHeaders`).
3. File-local helpers: converted from their own K&R declaration
   block, which is authoritative for file-scope functions; the parser
   is strict and bails with a report rather than guessing. (`cproto`
   was evaluated for this job and rejected: its internal parser cannot
   read modern macOS SDK headers — chokes on `__darwin_size_t` and
   private includes.)
4. Compile gate: `make base.o` in the file's directory; on failure
   the original file is restored automatically.

The per-file compile gate is the guardrail the June attempt lacked.

**Validated 2026-07-08 on `atk/eq/eq.c`** (pilot-A directory): 20
methods, 3 classprocs, 2 helpers converted; zero DRIFT. The first run
failed the compile gate and auto-restored — exactly as designed —
because `eq__WriteFILE`'s `char sep` is a promotable narrow type, so
its ANSI definition conflicts with the typeless `.eh` declaration.
Regenerating `eq.eh` with `-pe` resolved it: zero errors. This
confirms the M3↔`-pe` coupling concretely: converting a class's `.c`
requires regenerating its `.eh` with `-pe` in the same step (the
roadmap already sequences them together). All test artifacts were
restored; the committed tree is unchanged by the validation.

#### Delegation

M2 sweeps and M3 subtree conversion runs are delegable to smaller models
(Sonnet class) under these guardrails: per-file compile gate, signature
DB as ground truth, §12's long-vs-int policy, one subtree per commit, no
edits to generated files, no concurrent builds. Pure audits and dry-run
triage are Haiku class. Kept at the top level: the classpp codegen change
(M1), `ansify` construction, and adjudicating `.ch`-vs-`.c` signature
disagreements — those are real bugs, not conversion noise.

#### M1 mechanics (clarified 2026-07-08)

`fossil annotate` shows the classpp machinery already exists — checkin
`f4bf876da4` (2026-07-01) built both halves and deliberately throttled
them:

- `usePrototypesImport` (typed casts in `.ih` dispatch macros): default
  **on**, but the three general emitters (method macros, classprocs,
  `super_`) are gated `argcount >= 8` — the minimum for the arm64 ABI
  fix. The special classprocs (`New`/`Initialize`/`Destroy`/`Finalize`)
  already emit full typed prototypes ungated.
- `usePrototypesExport` (ANSI prototypes for method implementations in
  the `.eh`, from `realargtypes`): fully implemented, default **off**,
  commented `/* K&R decls: compatible with unconverted .c files */`.
- `-p` switches both fully on. `-D` writes `classname.desc` — method
  name, return type, full argument list, defined-by, vtable index —
  which *is* the signature database `ansify` needs.
- The `.ch.ih`/`.ch.eh` suffix rules in `config/andrew.rls` already pass
  `$(CLASSFLAGS)`, so per-directory opt-in is one Imakefile variable.

So M1's code component is small: split `-p` into `-pi` (Import, all
methods — drops the `>= 8` gates) and `-pe` (Export), keep `-p` as both,
defaults unchanged. The substance of M1 is the rollout below.

**Design constraint (the one rule):** classpp's compiled-in defaults do
not change until the whole tree is opted in. All rollout state lives in
committed Imakefile `CLASSFLAGS`. Consequences:

- Top-level builds — including `make Clean; make dependInstall` — are
  always safe: every directory regenerates with its own committed
  flags, so a clean build deterministically reproduces the committed
  converted/unconverted mix. The clean build is the gold-standard
  verification for each step, not a hazard. (Generated `.ih`/`.eh` are
  untracked in-tree build products; regeneration cannot dirty fossil.)
- The June failure mode cannot recur here: only generated, uncommitted
  output changes; a mistake is cured by resetting a flag and
  regenerating.

**Timestamp wrinkle:** the suffix rules fire on `.ch`-newer-than-`.ih`,
so flipping `CLASSFLAGS` does *not* regenerate by itself. Each opt-in
step must force regeneration (delete the directory's generated
`.ih`/`.eh`, or touch its `.ch` files). Step rhythm: set flag → force
regen → clean build → fix fallout → runtime spot-check → commit.

**Blast-radius asymmetry (why `-pi`/`-pe` split):** a class's `.eh` is
included only by its own implementation files, so Export fallout is
local to the flagged directory — it rides along with M3's per-subtree
conversion. But its `.ih` is installed to `build/include` and included
by every consumer tree-wide, so Import fallout surfaces at *call sites
in other directories*: missing type visibility (`FILE *`, typedefs)
where the `.ih` is included, and pointer-through-`long`-rock arguments
that clang treats as errors once the cast is prototyped.

**Import ordering — by consumer count, not directory tree:** classpp
reads the whole parent `.ch` chain when generating a subclass's `.ih`,
so a flagged leaf directory gets typed casts for inherited-method
macros without its parents being flagged; nothing structurally forces
bottom-up order. Fallout size of flagging a directory ≈ how many files
include the `.ih`s it generates. A 2026-07-08 survey (counting
`#include <X.ih>` across `src/` against each class's defining
directory) ranks the tree:

| Directory | external `.ih` includes | classes |
|---|---|---|
| `atk/basics/common` | 2,351 | 41 (`im` 257, `view` 245, `fontdesc` 187, `environ` 176, `message` 174, `proctbl` 148, `menulist` 135, ...) |
| `atk/support` | 450 | 19 (`style` 94, `envrment` 81, `buffer` 60) |
| `atk/text` | 321 | 21 (`text` 169, `textv` 94) |
| `atk/supportviews` | 178 | 17 |
| `atk/frame` | 95 | 5 (`frame` 85) |
| ~50 leaf directories | 0 | (pilot candidates) |

Strategy: pilot on zero-consumer leaves to learn the fix patterns
cheaply, then invert to the most-consumed core — that is where LP64
Variants 3/5 actually lived, and typing those `.ih`s protects all
consumers tree-wide at once, including directories not yet converted.
The ordered rollout checklist lives in `roadmap.md` → M1 rollout
points.

#### Pilot A findings (atk/eq, 2026-07-08)

Gate green (clean `make Clean; make dependInstall`, zero real compile
errors, mixed typed/typeless install state verified), eq inset
visually verified in `Sherman.Alloc`. Four findings for the runbook:

1. **Macro-parameter capture (new bug class, fixed structurally).**
   A `.ch` parameter name can collide with a type token in the typed
   cast: `Changed(enum changed changed)` made the macro parameter
   `changed` substitute the caller's argument into the cast's `enum
   changed` (→ `enum EQVIEW_caret`, incomplete-type error; and
   `DoScript`'s parameter `script` vs tag `enum script` would mangle
   the same way). Struct tags are immune only because classpp already
   rewrites `struct X *` cast params to `void *`. Fix: under `-pi`,
   classpp emits positional macro parameters (`_a1, _a2, ...`) for
   non-macrodef entries; macrodef macros keep declared names (their
   hand-written bodies reference them). Default output remains
   byte-identical — sanitization only activates with `-pi`, and the
   legacy ≥8-arg typed casts pick up the protection when Import-all
   becomes the default (rollout step 11).
2. **First DRIFT catch, in the very first directory:** `eq.ch`
   declared `DoScript(long pos, enum script *script, ...)` but the
   implementation and all six callers use `enum script` **by value**
   — a stray `*` in the interface, unnoticed for ~35 years because
   nothing ever type-checked dispatch. Fixed in `eq.ch`. Rate so far:
   one real interface bug per directory.
3. **Make wrinkle:** `.o` files do not depend on the local `.ih`/`.eh`
   in the generated Makefiles, so deleting the generated headers and
   re-running `make` rebuilds *nothing*. A flagged directory needs
   `make clean` locally (which also removes generated parser files —
   run the parser target first if the directory has one) or the full
   tree Clean. A local `make` alone can silently validate stale
   objects.
4. **Unkillable AUIS terminal apps are real — attribute hangs
   carefully.** During the gate, a `cui` in unkillable
   uninterruptible-exit state (`UE`, immune to all signals) was
   found and initially blamed on the build's demo-reconstruction
   step; it was actually a zombie from an earlier interactive test
   whose start time happened to coincide with the build window. The
   build completed exit-0 on its own (`ms` checkpoints past its
   known pre-existing date-parser segfault during the demo step).
   Two standing lessons: AUIS terminal apps can go `UE`-unkillable
   under VS Code/sandboxed shells (`bglisp` precedent, §13), so
   runtime checks belong in a real user terminal — the "user
   verifies visually" protocol is the default for rollout steps;
   and before killing anything during a slow gate, check provenance
   (`ps` etime/PPID) — `dependInstall` legitimately takes a long
   time, and its demo step emits alarming-but-nonfatal `cui`/`ms`
   messages.

#### Pilot B findings (atk/figure, 2026-07-09)

Three new fallout patterns for the runbook, all interface-side (`.ch`
fixes only, no `.c` changes needed):

1. **Typeless `.ch` declarations exist** — a third DRIFT flavor.
   `MoveHandle(x, y, ptref)` was declared with *no types at all* in
   five `.ch` files (figobj + four overriders); classpp silently
   treats each unknown token as a type name and rewrites it to
   `void *` in the typed cast, so callers passing `long`s failed.
   All five implementations agree on `long x, y, ptref`; typed the
   declarations to match. Runbook rule: an all-`void *` cast for a
   method whose callers pass integers usually means the `.ch` never
   had types, not that the callers are wrong.
2. **The rock idiom:** parameters declared `long rock` where every
   caller passes a pointer (`figure.ch`
   `Enumerate{Objects,ObjectGroup,ObjectTree}`, `figv.ch`
   `EnumerateSelection` — 13 call sites). Changed to `void *rock`,
   the LP64-correct direction; the K&R implementations keep `long`
   internally, which round-trips pointers on LP64 and gets cleaned
   up at M3. Counter-example in the same directory: `ToolName`/
   `ToolModify`/`Instantiate` rocks are fed from a `long` field in
   `objectlayout[]` — genuinely integers, left alone. Judge each
   rock by its callers, not its name.
3. **Whole-parameter transposition — the biggest DRIFT class yet.**
   `figobj.ch` (and five subclass `.ch`s) declared
   `Build(enum view_MouseAction action, struct figview *v, ...)`,
   but all six implementations *and every dispatch caller* use
   `(v, action, ...)`. The declaration has been wrong for ~35 years;
   runtime was consistent because nothing ever read the `.ch` order.
   Fixed the six declarations to the runtime truth. Consequence
   worth stating: had M3 run before M1, the `.desc` signature DB
   would have handed ansify the transposed order for every
   `figobj__Build` conversion — the pilots are validating the
   M1-before-M3 sequencing in exactly the way we hoped.

Also observed (benign): `-Wincompatible-pointer-types` warnings where
a subclass pointer is passed to a typed cast whose `self` parameter is
the *defining* superclass (`struct traced *`). That is the class
system's prefix-layout subtyping working as designed; these warnings
are expected wherever inherited methods are dispatched and are not
fallout.

#### Point 5 findings (atk/frame, 2026-07-09)

One new fallout pattern, and the first confirmation that Import fallout
reaches outside the flagged directory:

1. **`(long)` casts launder a rock through the interface, they don't
   change its meaning.** `frame.ch Enumerate`'s `long functionData` is
   a rock: every `mapFunction` callback treats it as a pointer, and
   every real caller passes one — but four of the six call sites wrote
   `frame_Enumerate(fn, (long) &x)`, matching the old `long` decl with
   an explicit cast rather than passing the pointer bare. Retyping the
   `.ch` to `void *` (the correct rock-idiom fix) turned those four
   casts into `-Wint-conversion` errors, since a pointer laundered
   through `long` no longer converts to `void *` implicitly. The
   fifth call site (`framecmd.c:768`) had *no* cast at all — passing
   the pointer bare, disagreeing with its four siblings — and was
   what surfaced the pattern first, under the "implementations
   disagree with dispatch callers" hard stop. Resolution (2026-07-09):
   treat the cast as noise, not a second caller's type — delete it
   along with the retype. Updated the runbook's rock-idiom rule to
   pre-authorize this narrow class of `.c` edit (delete-only, rock
   argument only) as part of the interface fix, rather than a full
   hard stop each time.
2. **Import fallout crossed into other directories, as predicted.**
   Three more call sites of the same `(long)`-laundered pattern turned
   up during the gate, outside `atk/frame` entirely:
   `atk/textaux/contentv.c`, `atk/extensions/compile.c` (x2), and (one
   more gate cycle later) `atk/extensions/tags.c` and
   `atk/extensions/deskey.c` — `tags.c` and `compile.c` share a
   near-identical `ViewEqual`/`FindByView` helper pair, evidently
   copy-pasted at some point. This is the blast-radius asymmetry
   `.ih`-vs-`.eh` split predicted above made concrete: a tree-wide
   `grep` for the rock's call sites after the first fix, rather than
   waiting for each to surface one gate cycle at a time, would have
   caught all six in one pass. Recommendation for future rock-idiom
   fixes: grep for the classproc's call sites tree-wide immediately
   after retyping the `.ch`, not just within the flagged directory.

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
