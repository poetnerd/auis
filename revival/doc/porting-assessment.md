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

## Getting K&R-era source to build under a modern compiler

The goal throughout was minimal code change: get a ~1990s K&R codebase
building and running correctly under today's compilers without
rewriting it. That goal held, but reaching it wasn't trivial — it took
two phases and, in the end, real (if narrowly scoped) changes
throughout the tree.

**First attempt, reverted:** mass-converting the K&R source to ANSI C
(explicit prototypes, typed parameters) in one pass with an automated
tool (`revival/tools/modernize`). Unreliable at scale — edge cases in
K&R parsing (multi-name declarations, function pointers, split-line
definitions) caused the tool to silently introduce bugs across
hundreds of files, taking the error count from roughly zero to over
2000. Reverted in full.

**Phase 1 (2026-06-29) — get it building at all, leniently.** Left the
K&R source untouched and relaxed the compiler's modern strict defaults
back to behavior compatible with 1990s C:

```
COMPILERFLAGS = -Wno-implicit-int -Wno-implicit-function-declaration \
                -Wno-incompatible-function-pointer-types
```

This took the build from ~1062 errors (after the modernizer revert)
down to ~344 — critically, those 344 were *real* portability problems
(`sys_errlist` removed from libc, `Display`/`FILE` struct internals
hidden by modern headers), not self-inflicted tool damage. Policy while
this lasted: no mass modernization; hand-edit only when the build
forced it (a structural incompatibility, e.g. `<a.out.h>` doesn't exist
on Darwin) or the file was already being touched for an unrelated
reason.

**Phase 2 (2026-07-08 through 2026-08-07) — replace leniency with real
correctness, tool-assisted, one subtree at a time.** The `.ch`
class-interface files already carried every method's true argument
types — they're the input to the class preprocessor, which had simply
been discarding that information. Emitting it instead turned the
compiler into an auditor: any place a real implementation disagreed
with its own declared interface became a located compile error rather
than a bug waiting to happen at runtime. `revival/tools/ansify`, a
per-file conversion tool built for this specifically (looking up real
signatures from that data rather than inferring them, unlike the tool
that failed in the first attempt), carried out the actual rewrite,
subtree by subtree, over milestones M1–M4. Full plan and rollout
mechanics: §14.

The same mechanism also closed out most of the tree's LP64 (64-bit
`long`) correctness problems. 1990s AUIS assumed `int` and `long` are
the same width — true on the platforms it originally shipped on, false
on arm64. The first instances of that assumption breaking were found
and fixed by hand starting 2026-07-02, before this conversion effort
existed (§11, §12); once real type checking landed with M1, the same
bug shape anywhere else in the tree became a located compile error
instead of silent runtime corruption, and more instances turned up
structurally during the M2/M3 rollout (§16, §19, §21, §22).

**Current state, as of 2026-08-07:** the entire active codebase
compiles clean under strict settings. The Phase 1 leniency flags above
are gone — `config/darwin/system.mcr`'s `COMPILERFLAGS` now reads
`-Werror=implicit-int -Werror=int-conversion
-Werror=incompatible-function-pointer-types
-Werror=implicit-function-declaration -Werror=format`, the exact
opposite of where Phase 1 left it. Two narrow accommodations remain,
both deliberate and unrelated to the leniency era: `-std=gnu89` (see
next section for why) and `-Wno-return-type` (K&R functions routinely
omit an explicit `return` in a non-`void` function; not worth
chasing). `modernize`'s regex K&R converter was never the vehicle for
Phase 2 (§14 explains why) — its "known limitations" were moot from
the start, not a to-fix list. Milestone-by-milestone record:
`roadmap.md` → "Major milestones" (current status) and
`claude-history/README.md` (per-session detail: prompts, findings,
fossil commit IDs). The Phase 1 hand-edit policy above still applies to any
currently-inert subtree (e.g. `ness`, blocked on an unrelated bison
issue) if one is ever activated, since those sat outside the
tree-wide conversion's scope.

**Not planned for the foreseeable future:** migrating from `gnu89` to
`c99`/`c11`. That would be a materially more intrusive pass — a
tree-wide audit and rename of every identifier that collides with a
C99-reserved word (`restrict` is the one confirmed hit so far; see
below for the first gate that work would need to clear).

### Why `gnu89`, not `gnu99`/`c99` — verified 2026-08-07

Asked directly after the M4 flip landed: is `-std=gnu89` load-bearing,
or just inertia? Tested empirically (same three leniency flags held
constant, only `-std=` varied):

- **Implicit int is not the blocker it looks like.** K&R declarations
  with no type specifier — pervasive across ~13,700 K&R definitions,
  and how classpp's typeless `.eh` prototypes and `(void(*)())`
  vtable-dispatch casts work — compile identically under `gnu89` and
  `gnu99` as long as `-Wno-implicit-int` is set. A minimal K&R
  function plus an untyped-dispatch cast produced the same
  warning-only, exit-0 result under both standards. classpp's own
  dispatch idiom is not what forces `gnu89`.
- **The real, unfixable-by-flags blocker is reserved-keyword
  collisions.** `atk/eq/draweqv.c:904` declares a parameter literally
  named `restrict` — a keyword C99 reserves. Compiled with the exact
  same three leniency flags: `gnu89` compiles clean; `gnu99` produces
  6 hard parse errors (`restrict requires a pointer or reference`,
  cascading `expected expression`), none suppressible by any warning
  flag — it's a grammar-level rejection, not a diagnostic.

**Conclusion:** `gnu89` isn't chosen because some classpp mechanism
requires it — it's chosen because it's the standard that actually
matches ~35 years of K&R-era identifier choices. The reserved-word
audit above (`restrict` confirmed; `inline` appears ~15 times
tree-wide but all in comments so far, not live identifiers) is a first
pass, not a complete one — it's the actual first gate a `c99`
migration would need to clear, not a formality, should one ever be
pursued.

## Original build-up plan (2026-06) vs. what actually happened

Early planning called for five staged milestones, static-first. The real
build-up diverged from that plan almost immediately and never went back
to it:

### Phase 1: Static build of core ATK (smallest surface) — skipped

The plan: disable `AMS_ENV`/`ANDREW_MALLOC_ENV`, build a minimal static
core with `genstatl`, defer dynamic loading to Phase 3. In practice,
Phase 3's `dlopen()` loader was built on day two of the port
(2026-06-25) rather than deferred, so there was never a separate
static-only milestone — the tree went straight to a normally-linked
build with dynamic loading in place from the start.

### Phase 2: ez running — done

`ez` starts and edits documents.

### Phase 3: Dynamic loading via dlopen() — done, earlier than planned

Implemented 2026-06-25, before Phase 1's static core or most insets
existed, not after them as sequenced here. See §3.

### Phase 4: Additional insets and applications — done

Insets and applications were brought up one at a time as planned; see
`roadmap.md`'s Applications and insets table for current status — nearly
everything is fully working.

### Phase 5 (optional): AMS and other subsystems — done, beyond "optional"

Went further than this plan anticipated: full AMS-over-IMAP/SMTP mail
integration (read, browse, send, writeback) was built, not just the
original local-mailbox AMS. See `roadmap.md` → Projects → AMS over
IMAP/SMTP and `ams-IMAP-project.md`.

## Issues to address

Index — search for `### N.` to jump to a section (anchor links aren't
reliable across Fossil's and GitHub's renderers, so this is a plain
lookup table, not a set of links):

| § | Issue | Status |
|---|---|---|
| 1 | `gcc -fwritable-strings` | RESOLVED 2026-07-23 |
| 2 | glibc `FILE` struct internals | RESOLVED 2026-06-25 |
| 3 | Dynamic object loader | RESOLVED 2026-06-25 (native `dlopen()`) |
| 4 | Andrew custom malloc | RESOLVED (disabled via config) |
| 5 | Platform configuration cleanup | LOW effort, not yet addressed |
| 6 | X11 paths and libraries | RESOLVED 2026-06-24 |
| 7a | Deferred: legacy sgtty terminal clients (`tm`, `vui`) | LOW priority, deferred |
| 7b | Deferred: `contrib/bdffont` | LOW priority, deferred |
| 7 | Console/stats module | LOW priority, deferred |
| 8 | Misc POSIX drift | superseded — Linux-specific, moot on Darwin |
| 9 | Font system | in progress as of 2026-07 |
| 10 | Messages with IMAP backend | RESOLVED — see `roadmap.md` → Projects → AMS over IMAP/SMTP |
| 11 | `%d`/`%ld` mismatch in scanf family | RESOLVED 2026-07-02 (full tree audit, 11 bugs fixed) |
| 12 | LP64 untyped dispatch: `long` param / `int` arg mismatch | RESOLVED — subsumed into M1 (§14) |
| 13 | Modern flex generator/init-flag polarity mismatch | closed 2026-07-07 |
| 14 | ANSI C Migration | COMPLETE 2026-08-07 |
| 15 | mkparser/cparser.c fixed-width table assumption | closed 2026-07-11 |
| 16 | classpp typed-dispatch signedness mismatch | closed 2026-07-11 |
| 17 | Xft "erase by redraw" stale foreground color | RESOLVED 2026-07-12 |
| 18 | Variadic function called through a K&R extern | RESOLVED 2026-07-22 |
| 19 | `.ch`/wrapper vs. real K&R out-param width drift | 5 known instances fixed; tree-wide sweep still open |
| 20 | `ansify` DRIFT false-positive on `InitializeClass`/`InitializeObject`/`FinalizeObject` | documented 2026-07-25; 2 instances unchecked |
| 21 | `%d`/`%ld` mismatch in the write direction (printf/fprintf) | RESOLVED 2026-07-26 |
| 22 | On-disk binary formats hard-coded around a 4-byte `long` | closed 2026-08-08 |
| 23 | `eq`: multi-line equation cleanup can't reduce to one line | open, long-standing |

### 1. `gcc -fwritable-strings`

**Status:** RESOLVED 2026-07-23, by re-enabling the flag.

**Correction to the original assessment below: this was not, in fact,
one of the issues a compiler flag can't paper over.** The Linux
`system.mcr` specifies `CC = gcc -fwritable-strings`; real gcc dropped
the flag in 4.0 (2005), which is what led to the "HIGH effort,
tedious-but-mechanical, defer it" verdict this section originally
recorded. But **Apple clang (the `cc` this tree actually builds
with on Darwin) still implements `-fwritable-strings`** — verified
directly: `char *p = "hello"; p[0] = 'H';` bus-errors when compiled
plain, exits clean with `-fwritable-strings` added. `config/darwin/system.mcr`
had never set it (only the legacy `i386_Linux`/`i386_bsdi`/`i386_bsd`/
`i386_mach` configs did), which is exactly why the Gate-1 hand sweep
below (`revival/doc/claude-history/strlit-sweep-prompt.md` →
`strlit-REPORT.md`, both retired there together) kept finding
live-if-narrow instances. Fix applied: `CC = cc -fwritable-strings`
in `config/darwin/system.mcr`, full clean rebuild (`make Clean`,
`rm -rf build`, `make World`) — confirmed the flag reached all 893
compile invocations in the rebuild log. Two link/type-error failures
surfaced by the full wipe (`ams/msclients/nns` missing `$(SSLLIB)`,
`contrib/zip/utility/ltapp.c` incompatible-pointer-conversion) are
pre-existing and unrelated to this flag; ez/messages/cui/help/runapp
all built clean.

**Scope, quantified after the fact** with a `-Wwrite-strings`
(`-Wincompatible-pointer-types-discards-qualifiers`) compile-only scan
across the same 849 translation units: 26,628 raw literal→`char*`
sites; 13,937 of those are `struct classheader`-style fields in
generated `.ih`/`.eh` (every class-based `.c` file inherits a couple
from `traced.ih`/`observe.ih`/`atom.ih`/etc. — inert boilerplate, would
need a `classpp` template change, not source-by-source fixes, to
clean up "properly"). The remaining 12,691 are anchored in
hand-written `.c` code across 444 files — but spot-checking the
heaviest hitters (`atk/text/txtvcmds.c` 816, `atk/eq/symbols.c` 399)
shows these are dominated by large static descriptor tables
(keybinding tables, symbol tables, command tables: `{"name", "key",
..., "description"}` rows) whose `char *` fields are never written
back through — a struct-typing habit, not a mutation. The much
smaller subset that's an actual mutate-through-the-pointer bug (the
class the Gate-1 sweep targeted by tracing real mutator functions —
`StripWhiteEnds`, `LowerCase`, `MapParens`, etc.) is a few dozen call
sites at most, tree-wide.

With the flag in place none of this is live risk anymore; the counts
above are kept only as context for why the flag is load-bearing. The
Gate-1 sweep findings in `claude-history/strlit-REPORT.md` are
superseded as action items by this fix, but its per-site detail would
matter again if a future toolchain migration ever dropped the flag.

**Considered and rejected: a full cleanup pass to make the tree
`-fwritable-strings`-independent.** The 26,628 warnings break into
three tiers of very different cost: (1) `struct classheader`'s two
fields, centralized, ~13,937 warnings, one struct + one classpp
codegen template — bounded and mechanical, reusable via the same
byte-identical-header diffing the M1 rollout already proved out; (2) a
dozen-ish table-owning struct types (`bind_Description` etc.) behind
most of the remaining volume — per-type auditing, moderate effort; (3)
the actual mutate-through-the-pointer functions (`StripWhiteEnds`,
`ReduceWhiteSpace`, `LowerCase`, `MapParens`, `ProcessCommand`,
`HandleAddress`, `hexout`, `html__ChangeAttribute`, ~150 call sites
combined) — these can't just be retyped `const`, since they genuinely
write through the pointer; each needs the atomlist private-copy
treatment (fossil `f91cb255`) and per-call-site behavior verification,
since several reuse the pointer in place
(`arg = StripWhiteEnds(arg);`) and a copy-based rewrite changes memory
ownership. Tier 3 alone is essentially Gate 2 of the original
`strlit-sweep-prompt.md`. Net judgment: multiple sessions of real
engineering effort to reach a state the compiler flag already gives
for one line and ten minutes. Deferred — not needed unless/until a
future toolchain genuinely can't provide an equivalent flag.

**Future Linux port: does the safety net survive?** The concern that
prompted the above: real gcc dropped `-fwritable-strings` in 4.0
(2005), and a Linux port might default to gcc, silently losing the
protection this section relies on. Resolution: `-fwritable-strings` is
a standard upstream Clang driver flag (from LLVM's own `Options.td`,
not an Apple SDK patch), so a Linux `clang` should have it too —
distro packages (`apt install clang` / `dnf install clang`) are the
same LLVM frontend for this purpose. **Not yet verified on an actual
Linux box** (none available from this environment) — a 30-second
`clang --help | grep writable-strings` check the day a Linux config is
revived should confirm it before relying on it. If confirmed, the fix
for that future config is the same one-line move made here:
`CC = clang -fwritable-strings` instead of assuming the distro's
default `gcc`, mirroring how `config/i386_Linux/system.mcr` already
hardcoded a specific compiler for its own reasons. This is the same
risk category as this build's existing reliance on `-std=gnu89` and
other legacy-compatibility flags — not a new kind of fragility.

<details>
<summary>Original assessment (superseded, kept for history)</summary>

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

</details>

### 2. glibc `FILE` struct internals

**Status:** RESOLVED 2026-06-25. Written against a hypothetical Linux
port and superseded once Darwin became the primary platform — Darwin's
own `config/darwin/system.h` was written fresh rather than derived from
`i386_Linux`'s, and never carried this problem.

```c
#define FILE_HAS_IO(f) ((f)->_IO_read_end - (f)->_IO_read_ptr)
```

This reaches into glibc's internal `FILE` struct layout, which has changed.
Modern glibc hides these fields. Replace with a portable alternative or
remove the optimization (it's a buffering check).

Similarly, `FILE_NEEDS_FLUSH` is defined to always return 1, which is
already the safe/portable behavior.

`config/darwin/system.h` defines `FILE_HAS_IO(f)` as `((f)->_r)` — the
field name Darwin/BSD libc's `FILE` struct actually exposes — and keeps
`FILE_NEEDS_FLUSH` at the same safe `1` the original assessment already
recommended. A future Linux port would need the glibc-specific fix
above; this build never needed it.

### 3. Dynamic object loader

**Status:** RESOLVED 2026-06-25 — replaced with native `dlopen()`.

The `overhead/class/` directory contains AUIS's custom dynamic loader for
`.do` (dynamic object) files. This was the hardest part of every port —
it had to understand each platform's object file format.

Implemented as `overhead/class/machdep/darwin/doload.c`, a thin wrapper
around `dlopen()`/`dlsym()`/`dlclose()` (`<dlfcn.h>`), standard POSIX.
`.do` files are ordinary Mach-O dylibs, built with `-dynamiclib
-undefined dynamic_lookup`. Simpler than the original per-platform
loaders it replaced — roughly 70 lines versus ~500. A handful of core
classes (`text`, `matte`, `lpair`, `scroll`) remain statically linked
into `runapp` rather than built as `.do` files; that's an existing build
convention, not a fallback this fix needed.

### 4. Andrew custom malloc

**Status:** RESOLVED — disabled via config, not needed.

`ANDREW_MALLOC_ENV` is defined by default. This custom allocator may
conflict with modern allocators and address space layout randomization.
Can be disabled by `#undef ANDREW_MALLOC_ENV` in `site.h`.

`config/site.h` `#undef`s `ANDREW_MALLOC_ENV` (done from the very first
Darwin bootstrap commit, 2026-06-25), so the system allocator is used
tree-wide and this conflict never arose.

### 5. Platform configuration cleanup

**Status:** Not yet addressed. LOW effort.

The `config/` directory has 30+ platform directories (VAX, RT, Apollo,
Mac II, NeXT, etc.) and `platform.tmpl` is a 220-line cascade of
`#ifdef` blocks for all of them. For the revival, strip everything
except the Linux path. This is just cleanup, not a porting problem.

Still true as written — all 30+ legacy platform directories are still
present; `platform.tmpl` was never trimmed. Harmless clutter, not a
blocker.

### 6. X11 paths and libraries

**Status:** RESOLVED 2026-06-24. Moot for the Linux paths originally
described, since Darwin/XQuartz became the primary target instead.

The `system.mcr` hardcodes:
```
XUTILDIR = /usr/bin/X11
XLIBDIR = /usr/lib
XLIB = -L$(XLIBDIR) -lX11
```

Modern Linux puts X11 in `/usr/include/X11`, `/usr/lib/x86_64-linux-gnu`,
etc. Fix the paths in `site.mcr` or update the defaults. The X11 API
itself hasn't changed in the ways that matter — Xlib is remarkably stable.

`config/darwin/system.mcr` was written fresh with XQuartz's actual
paths (`XUTILDIR = /opt/homebrew/bin`, `XLIBDIR = /opt/X11/lib`) from
the initial Darwin bootstrap — the Linux-specific paths above were
never carried over. A future Linux port would still need the fix as
originally described.

### 7a. Deferred: legacy sgtty-based terminal clients — tm, vui

**Status:** LOW priority, deferred.

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

### 7b. Deferred: contrib/bdffont

**Status:** LOW priority, deferred.

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

### 7. Console/stats module

**Status:** LOW priority, deferred.

`atk/console/stats/i386_Linux/` contains platform-specific code for
reading system statistics from `/dev/kmem` and `/proc`. The kernel
interfaces have changed completely. This module is not essential —
defer or disable it.

### 8. Misc POSIX drift

**Status:** Superseded — written against the original Linux-port
assessment. Darwin's `config/darwin/system.h` was written fresh rather
than derived from `i386_Linux`'s, and carries none of the mappings
below; none of these ever became live issues on this port. Kept for
reference in case a Linux config is revived.

- `setreuid(r,e)` is mapped to `setuid(r)` — modern Linux has `setreuid()`
- `osi_vfork()` maps to `fork()` — fine, vfork() is deprecated anyway
- `_setjmp`/`_longjmp` — still available but may need review
- `FNDELAY` mapped to `O_NONBLOCK` — correct
- `getwd()` mapped to `getcwd()` — correct
- `NDEBUG` is forced on to work around a missing `___eprintf()` in shared
  libs — this is long since fixed in modern glibc, remove the `#define`

### 9. Font system

**Status:** in progress as of 2026-07.

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
| Body text | Migrated to Xft (2026-07, phase 1 complete) -- antialiasing only, still resolved via the legacy X-core-font match; see "Update 2026-08-20" below |
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

#### Update 2026-08-20: "migrated to Xft" still resolves fonts through the bitmap-era X core path

Found live while adding real CSS font-size/font-family support to
htmlatk.c's HTML-mail renderer (revival/doc/html-mail-rendering-design.md):
wdc flagged rendered text as visibly ugly -- "I think we're interpolating
Andrew bitmap fonts instead of scalable fonts" -- and traced it to
confirm that's exactly right, despite body text supposedly being on the
"Migrated to Xft" row of the table above.

`xfontdesc_GetXftFont` (`xfontd.c:1004-1020`) never independently
resolves a font by family/size through Xft/fontconfig. It calls
`xfontdesc_LoadXFont` first, which is the *original, unmodified*
X-core-font-protocol matching chain -- `XLoadQueryFont`/`XListFonts`
against XLFD wildcard patterns (`xfontd.c:450-633`), the same lookup
AUIS used before Xft existed. *Only after* a core font is matched does
the code take *that* font's own XLFD and hand it to `XftFontOpenXlfd`
(`xfontd.c:654-676`) to get an antialiased handle for the *same*
resource. That's the whole "Xft migration": antialiasing bolted onto
whichever core-font-protocol match won, not a real fontconfig
family/size resolution.

This matters because the X core font protocol and fontconfig are two
separate font registries on a modern system. XQuartz's core-font path
(what `XLoadQueryFont` searches) is the small set of classic bundled
X11 fonts -- essentially just bitmap Adobe Helvetica/Times/Courier
PCF/BDF resources at fixed traditional sizes (8, 10, 12, 14, 18, 24...).
It does NOT expose the system's real TrueType/OpenType catalog --
that's only reachable through `XftFontOpenName`, a different API this
code never calls. `xfontdesc_LoadXFont` does attempt a scalable request
first (`pixel_size=0` in the XLFD pattern, `xfontd.c:538-549`, "selects
Type1/TrueType over bitmap PCFs" per its own comment -- an earlier
session's attempt at this exact problem), but it can only succeed if
XQuartz's core-font path happens to have a genuine scalable resource
registered under that XLFD, which in practice it usually doesn't; the
match falls through to one of the fixed-pixel-size bitmap attempts
below it instead. `XftFontOpenXlfd` on a bitmap-only resource still
returns a usable handle -- Xft can draw PCF/BDF glyphs -- it just
can't make them genuinely scalable; a size that doesn't match one of
the font's native bitmap sizes gets the nearest bitmap stretched to
fit, which is the "ugly interpolated bitmap" look.

Why this wasn't visible before today: the *only* pre-existing font-size
input was the legacy `<font size="1".."7">` scale, mapped to a fixed
table of exactly `{8, 10, 12, 14, 18, 24, 32}` points
(`htmlatk.c`'s `FontSizeStyleFor`) -- not a coincidence; those are the
traditional bitmap font sizes, so every request already landed on a
native resource with no stretching. Today's session added real CSS
`font-size` support (arbitrary px/pt values, converted to point sizes
that essentially never land on that fixed set) specifically because
that's how virtually all real HTML mail sets text size -- which is
what turned a previously-invisible limitation into constantly-visible
blocky text.

**Recommended fix** (not implemented -- scoped here for a dedicated
session, per wdc: "I should probably spawn a new instance, maybe even
Fable to clean that up"): give `xfontdesc_GetXftFont` (or
`xfontdesc_LoadXFont`) a genuine fontconfig-driven path that doesn't
depend on the X-core-font match succeeding first -- build a fontconfig
pattern string directly from the requested family/weight/slant/size
(the same inputs `xfontdesc_LoadXFont` already computes as `xfamily`/
`weight`/`slant`/`desiredSize`, `xfontd.c:502-536`) and call
`XftFontOpenName` on it, letting fontconfig substitute against the
real system font catalog the way every other modern Xft/fontconfig
application does. The existing X-core-font chain would stay as the
fallback for environments without Xft, and the Andy symbol/cursor PCF
fonts are unaffected either way (no fontconfig equivalent, permanent
X-core dependency per the table above). Scope note: `xfontd.c` is
core, shared ATK code -- every application that draws text goes
through it, not just messages/htmlatk.c, so this is a bigger and
more widely-felt change than anything in this HTML-rendering
project so far.

### 10. Messages with IMAP backend

**Status:** RESOLVED. `messages` runs against a local mbox store with a
one-way IMAP mirror layered on top — full design and status in
`ams-IMAP-project.md`; current-status summary in `roadmap.md` → Projects
→ AMS over IMAP/SMTP. Read/browse/send/writeback all work; only XOAUTH2
auth and folding the mirror-sync step into the normal workflow remain.

This section originally posed the investigation as open — the questions
below (how thick is the `atkams`/AMS bridge? is there a storage
abstraction? does `messages` talk to AMS through a clean boundary?) have
since been answered directly by reading the source, not by guesswork:
AMS's `MS_*` API (~75 entry points, ~25-30 actually used by the live
store surface) is exactly the clean seam this section was hoping for.
Kept below for the historical record of the original open question.

<details>
<summary>Original 2026-06 assessment (superseded, kept for history)</summary>

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

</details>

### 11. `%d` / `%ld` mismatch in scanf family

**Status:** RESOLVED 2026-07-02. Full tree audit (141 `scanf`/`fscanf`/
`sscanf` hits against `%d`) found and fixed 11 real bugs beyond the two
below; full instance list in `porting-changelog.md`'s 2026-07-02 entry.
The audit-query section below is kept for anyone re-running the check
after future changes.

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

### 12. LP64 untyped dispatch: `long` parameter / `int` argument mismatch

**Status:** RESOLVED — the root mechanism (untyped `void (*)()` class
dispatch) was fixed structurally by the M1 typed-dispatch conversion
(§14), complete tree-wide as of 2026-08-07. The specific instances,
audit methodology, and options analysis below remain as reference for
recognizing the same bug shape anywhere it could still occur outside
that conversion's scope.

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

#### New sub-variant (2026-07-24): explicit cast masks the mismatch from the compiler

The M2 census (`revival/doc/claude-history/m2-census-REPORT.md`) found this
family's by-pointer shape mechanically, via `-Wincompatible-pointer-types`
warnings against typed `.ch`/`.ih` signatures — but that method has a blind
spot: an explicit pointer cast at the call site silences the warning outright,
so a real instance can sit in the tree indefinitely without ever showing up in
a compiler-driven sweep.

Found by hand while root-causing the `figotext` label-rendering corruption
(figure-inset text showing as stray single characters, position correct):
`fontdesc.c`'s `fontdesc__StringBoundingBox` calls its sibling method
`fontdesc_StringSize` like this:

```c
int w, a, d, ascent, descent, junk;
...
fontdesc_StringSize (font, graphic, string, (long *) &w, (long *) &junk);
```

`StringSize`'s real implementation (`xfontdesc__StringSize`, `atk/basics/x/
xfontd.c:960`) writes through both out-params as genuine `long *` (8-byte
stores: `*XWidth = retWidth; *YWidth = 0;`). `w` and `junk` are `int` — 4-byte
stack slots — so every call overflows 4 bytes past each one. Confirmed via
`fossil artifact` against the original trunk import
(`90afc0c28e`, both the 2026-06-24 initial import and the 2026-06-29
"revert all .c to trunk" commit): this cast is verbatim 1990s CMU source, not
something introduced during this port. On the original 32-bit platforms
`int`/`long` were the same width, so the cast was always a no-op there; LP64
is what turns it into a live stack overflow.

**Census method for this sub-variant** (a compiler-warning sweep won't find
it — it has to be syntactic): grep the tree for the masking shape itself,
then manually check the target variable's real declared type at each hit.

```
grep -rnE '\((long|int|short)\s*\*\)\s*&[A-Za-z_]' --include=*.c .
```

Run tree-wide 2026-07-24: 21 hits total.
- **1 confirmed bug**: `fontdesc.c:378` (above).
- **14 safe**: `rm.c` (×4), `dataobj.c` (×2), `view.c` (×2), `frame/
  framecmd.c` (×1, plus `osi_Times.Secs` which is already `unsigned long`)
  all cast a `struct * pointer` to `(long *)` — pointers and `long` are both
  8 bytes on this LP64 build, so these are the ordinary "store a pointer in
  a long-typed slot" (`rock`-style) idiom, not a mismatch.
- **6 hits are dead code**, confirmed not built in this configuration (no
  `.o` under `build/` for any of them): `contrib/mit/fxlib/rpc3.9/*` (old
  RPC library) and `overhead/class/machdep/next_mach/doload.c` (NeXT-only
  machdep, never selected by `config/site.h` on Darwin).

Narrow in this tree as of 2026-07-24 — one real site — but worth re-running
this grep any time more of the tree gets ANSI-converted, since a `.ch`
signature widening (like `StringBoundingBox`'s own `int*`→`long*` fix earlier
the same day, `m2-census-REPORT.md` row 22) is exactly the kind of change
that turns a previously-matched, harmless cast into a fresh mismatch. This
method only catches concealment via an explicit narrowing/widening cast; it
doesn't add coverage beyond what `m2-census`'s compiler-warning method
already found for uncast mismatches.

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

### 13. Modern flex generator/init-flag polarity mismatch

**Status:** closed 2026-07-07. LOW effort.

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

### 14. ANSI C Migration

**Status:** COMPLETE 2026-08-07 — entire active codebase compiles clean
under strict settings. See "Getting K&R-era source to build under a
modern compiler" above for the completed-state summary.

This was a major effort. The decision to take it on was based on the
belief that it would be a quicker route to finding and fixing more of
the subtle word-size and type-conversion bugs that had already cost
significant effort to track down by hand. Originally it was hoped that
a `modernize` tool would be simple to write and could supply the
necessary changes automatically. That failed (checkin `5e57549713`, 779
files, reverted in `99fe31066c`), for reasons detailed below in "Why
the June mass conversion failed." So a meticulous, multi-phase review
of all active code was carried out instead, to discover stylistic
peculiarities and the source-to-source translations they required —
some of which defied simple automation.

This multi-phase update was done as four milestones. The ordered work
plan and full rollout checklists live in `claude-history/README.md`,
kept for provenance; nothing below requires opening it.

**M1 — The compiler becomes the auditor** (complete 2026-07-10). Switch
classpp, the class preprocessor, to emit ANSI-compliant headers — real
typed prototypes and typed dispatch casts, generated from the same
`.ch` signatures classpp had always parsed but thrown away — and fix
the resulting fallout, directory by directory.

This is where the real bug-hunting methodology got invented, and where
the risk was highest: a flagged directory's fallout routinely reached
call sites in *other*, unflagged directories (a `.ih` is installed
tree-wide; a `.eh` isn't), so a naive "fix whatever fails to compile"
approach kept rediscovering the same problem one gate cycle at a time
instead of once. The fix — a static, tree-wide census of every call
site before touching code, rather than trusting the compiler's own
gate log, which only ever shows a directory's *first* failure — is
what let the later milestones scale at all. M1 is also where most of
the recurring bug shapes got named for the first time (the "rock"
idiom, whole-parameter transposition, typeless `.ch` declarations,
DRIFT); having a name and a known fix for each is what made M2 and M3
fast rather than a re-investigation every time. The "Pilot A/B" and
"Point N"/"batch N" labels in the subsections below are checkpoints on
M1's own ordered rollout checklist.

**M2 — Function prototype sweep** (complete 2026-07-25), for missing
prototypes specifically: any call to a function with no declaration in
scope, which silently truncates a pointer return value to 32 bits on
LP64 (variant #1 of the LP64 bug family, §12). `-Werror=implicit-function-declaration`
turned on subtree by subtree; each fallout site got a missing
`#include` or `extern` declaration. Closed that variant permanently —
real total came in well past the original census, 3,888 instances
across 29 directories, once a malloc-family blind spot in the first
count was found.

**M3 — Function definitions converted** (complete 2026-08-01). The
`ansify` tool rewrote K&R function definitions to real ANSI ones, one
subtree per commit — looking up each class method's true signature from
the `.ch`-derived signature database built for this purpose, never
inferring types the way the failed June attempt had. `-pe` (typed
`.eh` prototypes) went on per directory in the same step as its
conversion. A per-file compile gate with automatic restore-on-failure
was the guardrail the original mass-conversion attempt never had.

**M4 — Global enforcement** (complete 2026-08-07). The full strict-C
`-Werror` set — implicit-int, int-conversion,
incompatible-function-pointer-types, implicit-function-declaration, and
(added mid-milestone, after a real bug — a datastream write silently
truncating a `long` id via `%d`, §21) format — turned on directory by
directory, then flipped to the tree-wide compiler default. Every batch
surfaced at least one genuine decades-old bug beyond ordinary compiler
noise; full catalog in `revival.md`.

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
The ordered rollout checklist lives in `claude-history/README.md` →
"Retired top-level docs" → `roadmap-old.md` → M1 rollout points.

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

#### Point 9 findings (atk/basics/common, 2026-07-09)

The core: 41 classes, 2,351 external consumers, four gate cycles.
The directory's own `.ch`s were accurate (zero local fallout — like
raster/lib, remarkable for the oldest interfaces in the tree); ALL
fallout was consumers colliding with newly-typed rocks. New patterns,
all now in the runbook:

1. **The census supersedes the gate log.** A directory's build stops
   at its first failing file, so each gate cycle reveals only the
   shallowest error per directory (`tmv.c`'s bare-pointer
   `SetOverride` hid behind `tm.c`'s zombie-handler error, etc.).
   And `long` rocks accept laundered casts and integers silently —
   the gate only ever shows bare-pointer callers. Decision basis for
   every retype was therefore a static tree-wide call-site census
   (~600 sites classified bare-pointer / laundered / integer / zero
   across 23 methods), which also proved delegable to a
   cheaper-model agent, as did the resulting ~100 mechanical
   call-site edits (93/93 applied without deviation from spec).
2. **Integer-majority rock** — `keymap_BindToKey`: 161 integer/zero
   callers vs one bare pointer. The rock keeps `long`; the pointer
   site gets `(long)`. Mirror of the point-8 dual-use ruling.
   Contrast `menulist_AddToML` (236 sites: 139 pointer-ish, 51
   integer) which went `void *` with `(void *)` casts at the
   integer sites. Both directions now precedented; majority rules.
3. **Dual-use attribute values** — `suite`/`chart` `ItemAttribute()`
   returns `long` that is sometimes a string pointer; sites feeding
   it to typed `char *` params get `(char *)` casts. Sibling: magic
   int constants for struct-pointer params (`graphic_BLACK`=0xFF as
   a Fill Tile) — cast at the site, convention preserved.
4. **First M2-class catch:** `clockv.c` used `NewString()` (returns
   `char *`) with no declaration in scope — implicit-int truncation
   of the pointer on LP64, invisible until the argument position
   became typed. Fixed with the `#include <util.h>` its sibling
   files already had.
5. **First live caller bug requiring a semantic `.c` fix:**
   `htmlview.c` passed `DisplayString(self, "msg", 0)` — priority
   and string transposed; the messages have never displayed. Ruled:
   fix the caller, separate commit. (Transposition class previously
   seen interface-side in Pilot B's `Build`; this is the caller-side
   variant.)
6. **`-pi` types struct-pointer method args as `void *`** (self and
   scalar/char types are fully typed). So the rollout catches every
   int/pointer confusion — the LP64 killer class — but not
   wrong-struct-pointer mistakes; and typed checking reaches
   unflagged directories through macromethods that expand into a
   flagged class's dispatch (`chartobj.ih` → `graphic_DrawString`).

#### Point 10 batch 1 findings (atk/value+adew+apt/{apt,suite,tree}+controllers, 2026-07-09)

First batch under the point-10 batch amendment (one session, one gate,
census-first per directory). atk/controllers is flagged but inert —
not in the default build (`MK_AUTHORING`/`MK_CONTROLLERS` off), like
ness/objects (celv's only external callers). contrib/mit IS in the
build (popts.c compiled and needed `(void *)` casts at two
`PostResource` integer sites — the point-9 retype reaching contrib).
Consumer fallout arrived in RINGS across three gates: gate 1 exposed
org/bushv (treev pair macros), gate 2 exposed chart + popts
(CaptureString/PostResource one ring further out) — the gate walks on
past a failing directory, so its log is complete per-ring but blind to
files behind each directory's first failure; local `make -k` rebuilds
(runbook step-3 amendment, adopted mid-batch) flush a directory in one
pass. Findings:

1. **atk/value came in as predicted near-zero** (census-first works):
   the lone rock, `value.ch AddCallBackObserver`, is integer-unanimous
   across ~55 sites → stays `long`, zero edits; `SetValueType`'s
   by-design dual-use rock is fully laundered by its own macromethods.
   Actual fallout was two typeless override decls (`DrawButtonText`
   `pushd` in entrstrv/entrintv vs the typed defining decl in
   buttonv.ch) and one real LP64 bug: `enterstrV__LinkTree` (and the
   unbuilt entrintv sibling) never declared `parent` — implicit int
   truncated every view pointer passed through. Extends the clockv
   precedent to missing K&R param declarations.
2. **Typeless-decl cluster in apt**: `apt.ch` SetAreaSpread* `mode`,
   `ReadObject` `reader`; `aptv.ch` `PrintObject` `printer`,
   `OpenPrintStream` (3 of 4 params); `cel.ch` `SetVisibilityBit`;
   plus `apts.ch CaptureString` declaring `char *target` where the
   impl takes `char **` — worth noting that classpp never validated
   `.ch` against impls, so a wrong declaration was FREE until now.
3. **Unsigned rock — a live-bug variant of the rock idiom**
   (`suite.ch` `Apply(unsigned anchor, unsigned datum)`,
   `Create(..., unsigned anchor)`): unlike `long`, `unsigned` is
   32-bit on LP64, and the impls declared it too, so every suite-based
   control panel (bush, chart, org, cmap, zip ltv) handed truncated
   `self` pointers to its Hit/Sort/Title/Exception handlers on arm64.
   Ruled: `.ch` → `void *` AND impl K&R decls → `long` (separate
   live-bug commit). A third truncation of the same species was found
   laundered as `(unsigned int)item` at a suite.c vector call site.
4. **Variadic-by-macro-convention attribute family** — the biggest
   structural find of the rollout so far; see the runbook's new
   signature-drift bullet for the mechanism (attribute-pair macros
   riding one macro argument into unprototyped calls) and resolution
   (true `.ch` arity + 95 dispatch sites mechanically expanded across
   12 files; pair macros fenced for spec-table use only). Getter
   sites had passed a never-read dummy pair (`suite_ItemName(0)`)
   for 35 years. `treev.ch` turned out to carry its own copy of the
   convention — its parameter was literally named
   `attribute_codevalue` — caught by the gate one consumer ring out
   (orgv/bushv), a reminder to sweep EVERY `.ch` in a flagged
   directory for `#define X(x) code, (long)(x)` macros up front.
   Caller bugs that fell out, each a separate caller-bug commit:
   `bushv.c` passed a bare string as the attribute CODE (`"No
   Current Directory"` title — silent no-op since 1989), and
   `chartv.c` passed `*LabelFontName`/`*ScaleFontName`/
   `*TitleFontName` (the string's first CHAR) where CaptureString
   takes a `char **` target — memory corruption whenever those font
   attribute cases executed; siblings correctly pass `&X`.
   Ruling extends to atk/chart's identical `chart_ItemAttribute`
   family when that directory is flagged.
5. **Datum-rock families**: `tree.ch` Create*Node/NodeOfDatum/
   SetNodeDatum datum → `void *` (pointer-unanimous incl. laundered
   `(long)text` in org, `(char*)dir` in bush); `vector.ch` item
   methods → `void *` (dual-use: 6 pointer callers, 1 genuine offset
   in suiteev gets `(void *)`); `suite.ch` CreateItem/ItemOfDatum
   datum STAYS `long` (integer-majority: ~35 code sites vs bushv's 5
   pointer sites, which get `(long)` casts). Both `Create(spec, ...)`
   classprocs also declared their spec param by-value where impls
   take pointers (`tree_Specification *`, `suite_Specification *`).

#### Point 10 batch 3 findings (atkams/messages/lib, 2026-07-10)

Gate green first pass; all fallout was local (19 errors, one ring),
none of the 95 external consumers broke. No pair macros. Seven `.ch`
drift fixes, mostly known taxonomy (cvEng typeless params;
DisplayNewBody bare params; fldtreev PostMenus by-value unnamed
struct; folders AlterSubscriptionStatus declared `(dir, shortname,
status)` vs impl+all-four-callers `(dir, status, shortname)` — the
pilot-B transposition class again; SetCUIRock `long` rock → `void *`
with the laundered forward at ams.c:120 getting a pre-authorized
`(char *)` cast). Two new mechanisms:

1. **Unknown type tokens emit implicit-`int` cast params — a typed
   cast that lies.** `CUI_Initialize(proc TimerFunction, ...)` (ams,
   amsn, amss) and `orgv.ch SetHitHandler((long *handler)(), ...)`:
   classpp copies an unrecognized type token verbatim into the cast,
   where gnu89 parses the bare identifier as a parameter name with
   implicit `int` — so the "typed" parameter is 32-bit and would
   truncate any function pointer passed on LP64 (callers passing a
   real function error visibly; callers passing NULL/0 compile
   silently against the lying cast). Same species: `sendmsg.ch`
   declared `Boolean`, a typedef private to sendmsg.c — any type
   name in a `.ch` must resolve in EVERY consumer's translation
   unit. Fix: the class-system typedef `procedure` for function
   pointers (`keystate.ch`/`value.ch` are the existing convention),
   the underlying public type (`short`) for private typedefs.
   Census rule added: sweep flagged `.ch`s for type tokens that are
   not C keywords / `struct|enum|union` / `FILE` / `boolean` /
   `procedure`.
2. **classpp does NOT comma-share types, and override macros take
   the defining class's decl.** Minimal-case verified: `Foo(int a,
   b, c)` emits `(int, void *, void *)` — each bare identifier is
   treated as an unknown TYPE (pilot-B MoveHandle behavior), never
   as a second name under the first type. Batch 2's contrary census
   note (`GrayPattern(short a, b)` → `(short, short)`) observed a
   different mechanism: that decl is an OVERRIDE in xgraphic.ch,
   and a subclass's `.ih` macros for inherited/overridden methods
   carry the DEFINING class's declared signature (graphic.ch, fully
   typed). Consequence for the rollout: flagging a directory types
   the casts of every ANCESTOR method into the subclass `.ih`, so a
   malformed decl in an unflagged parent directory (orgv.ch here)
   surfaces under the child's flag; and since classpp resolves
   parent `.ch`s from the INSTALLED include tree, such a fix only
   takes effect after `make install` in the parent's directory.

### 15. mkparser/cparser.c: fixed-width table assumption vs. modern bison's per-table type narrowing

**Status:** closed 2026-07-11. MEDIUM effort.

#### Root cause

`overhead/mkparser/` is a code-sharing layer, not a from-scratch parser
generator: `mkparser` (an awk-based shell script) post-processes bison's own
generated `.tab.c` output, stripping bison's `yyparse()` and rewriting its
LALR tables into a `struct parser_tables` consumed by one shared,
hand-written engine (`cparser.c`'s `parser_Parse()`). Every AUIS grammar
compiles down to one instance of this same table struct plus one shared
`parser_Parse()` — the point is to avoid N copies of parser machinery for N
grammars.

`cparser.h`'s `struct parser_tables` declares every table pointer uniformly
as `short *`, matching the (Andrew-patched, circa-1994) bison version this
was written against, which always emitted plain `short` for every generated
LALR table regardless of its actual value range. Modern bison (2.3, verified
on this build) does not: it picks the *narrowest* C integer type that fits
each table's value range, per table, per grammar — `yytype_uint8` (1 byte)
when all values fit 0–255, `yytype_int16` (2 bytes, i.e. an actual `short`)
otherwise, and so on. For `ams/libs/ms/prsdate.gra` specifically, three
tables (`yyr1`, `yyr2`, `yydefact`) narrow to `yytype_uint8` because their
values (rule numbers, RHS lengths) are all small, while five others
(`yypact`, `yypgoto`, `yydefgoto`, `yytable`, `yycheck`) still need the full
16 bits and stay `yytype_int16`. `mkparser`'s generated struct initializer
casts *all eight* to `(short *)` unconditionally:

```c
(short *)yydefact,	/* defred */
```

Reading a 1-byte array through a pointer with 2-byte element stride merges
each *pair* of adjacent `yytype_uint8` entries into one bogus value. Every
indexed access into any table modern bison happened to narrow returns
effectively random data — for `prsdate.gra`, this is `lhs` (from `yyr1`),
`rhssz` (from `yyr2`), and — the one that broke every single parse —
`defred` (from `yydefact`): a state whose only valid action is "always
reduce by rule 125" instead read back as rule 0 ("no action"), which the
engine (before the fix below) misinterpreted as "no valid transition,"
raising a false syntax error on a token sequence bison's own tables prove is
perfectly well-formed.

Two more bugs turned up in the same investigation, in the *hand-written
engine logic* rather than the type-width mismatch, but from the same root
pattern (code written against one bison version's exact conventions,
silently violated by a later one):

- **Goto-table (`nextx`/`yypgoto`) short-circuit.** The engine tested
  `nextx[lhs] == defflag` (`defflag` holds `YYPACT_NINF`) and, if true,
  skipped straight to the default-goto table without ever consulting the
  compressed goto table. Bison's own `yyparse()` has no such short-circuit
  for the goto table at all — it always computes `yypgoto[lhs] + *yyssp`,
  bounds-checks it against `yycheck`, and only falls back to the default
  (`yydefgoto`) if that specific check fails. `yypgoto` is not gated by
  `YYPACT_NINF` the way `yypact` is; it can coincidentally *equal* that
  sentinel value for a nonterminal with no special meaning at all. Confirmed
  by direct inspection of `prsdate.gra`'s generated tables: 21 of 47
  nonterminals collide with `YYPACT_NINF`, including — critically — `date`,
  `yearday`, `partial_date`, `months`, `years`, and `days`, i.e. almost
  every nonterminal a real date parse passes through. The short-circuit sent
  the parser to the single generic default-goto state instead of the
  context-correct one for all of them, whenever the specific state on the
  stack wasn't the statistically-most-common case bison chose as the
  default.
- **Action-table sentinel conflation.** Bison uses *two different*
  constants for two different "no entry" meanings: `YYPACT_NINF` (the
  action table `yypact`'s "always use the default reduction" flag) and
  `YYTABLE_NINF` (the compressed table `yytable`'s own, separate "no valid
  action here — syntax error" flag) — `-179` and `-149` respectively for
  `prsdate.gra`, i.e. genuinely different values. The engine only knew about
  one (`defflag`/`YYPACT_NINF`) and used it to test a `yytable` lookup
  result, so a real `YYTABLE_NINF` entry (which should mean "syntax error")
  was never recognized as such and fell through to being (mis)treated as a
  negative rule number to reduce by. Separately, the engine treated a raw
  `yytable` entry of exactly `0` as "fall back to the default reduction" —
  bison's own `yyparse()` treats `0` as a syntax error too, not a fallback.

#### Fix

- `struct parser_tables` (`cparser.h`) gained a new field, `tblflag`,
  holding `YYTABLE_NINF` — distinct from the pre-existing `defflag`
  (`YYPACT_NINF`). `mkparser`'s generated table initializer now emits both.
- The goto-table lookup in `cparser.c` no longer special-cases `defflag` at
  all; it now matches bison's own formula exactly — always index, bounds-
  check, and consult `valid[]`/`yycheck`, falling back to `defnext`/
  `yydefgoto` only when that check fails.
- The action-table decode now treats `tact == 0 || tact == desc->tblflag`
  as the syntax-error case (matching bison), instead of testing against
  `defflag` and instead of treating `0` as "use the default reduction."
- `mkparser`'s awk post-processor gained a rule that force-normalizes *any*
  of the eight table declarations it pointer-casts to `(short *)` — `yyr1`,
  `yyr2`, `yydefact`, `yypact`, `yypgoto`, `yydefgoto`, `yytable`, `yycheck`,
  plus the debug-only `yyprhs`/`yyrhs`/`yyrline` — back to plain `short`,
  regardless of which narrower `yytype_*` type modern bison assigned each
  one for a given grammar:
  ```awk
  /yytype_u?int(8|16|32) yy(r1|r2|defact|pact|pgoto|defgoto|table|check|prhs|rhs|rline)\[\]/ {
      sub(/yytype_u?int(8|16|32)/, "short")
  }
  ```
  This makes the fix generic across grammars and bison versions/value-range
  combinations, rather than a one-off patch for `prsdate.gra`'s specific
  narrowing choices — a different grammar could have bison narrow a
  completely different subset of these eight tables, and the fix still
  holds because it forces *all* of them back to a consistent width.

#### Scope: every mkparser-based grammar, tree-wide

Because all AUIS grammars share this one engine, the fix applies tree-wide
by construction — but the *generated* `.c` file for each grammar still had
to be regenerated against the fixed `mkparser`, since Makefiles depend on
each grammar's own `.gra` source, not on the `mkparser` tool itself; `make`
had no reason to know these files were stale. Five grammars use `mkparser`,
found by grepping every Imakefile for `bin/mkparser`:

| Grammar | Location | Subsystem | Dynamically verified? |
|---|---|---|---|
| `prsdate` | `ams/libs/ms` | AMS date-header parsing | **Yes** — end-to-end via `gendemo`/`amsdemo`, captions and message ordering both confirmed correct |
| `eliy` | `overhead/eli/lib` | ELI (Embedded Lisp Interpreter), the basis for FLAMES, AMS's mail-filtering/scripting language; linked into `ms`, `cui`, `vui`, `nns`, `messages` (`amsn.do`), and ELI's own `bglisp` test REPL | No — compiles/links clean against the fixed engine, not separately exercised. `bglisp` was noted hanging uninterruptibly at startup during §13's verification (2026-07-07), dismissed then as "a separate, pre-existing issue" — given how closely that symptom (uninterruptible hang, not a clean crash) matches this bug's signature, it may well be the same root cause. Not confirmed. |
| `parsey`/`parseadd` | `overhead/mail/lib` | RFC822 address parsing (`ParseAddressList`) — alias/address-book resolution, self-address stripping, `From:`-header "pretty name" extraction for message captions, forwarding validation, `trymail`/`eatmail` delivery | No — same as above. Notable: the *original* `gendemo` crash found in this revival (§13, 2026-07-07) was in this exact code path (`ParseAddressList` → `FindPrettiestFromString` → `BuildCaption`), for an unrelated flex reason at the time; this engine bug may have been a latent second failure mode in the same call path all along. |
| `eqparse` | `atk/eq` | Equation/math-formula typesetting inset grammar (ATK's analogue of troff's `eqn`) | No |
| `num` | `atk/rofftext` | Numeric-expression evaluation inside roff-formatted document text (register arithmetic, akin to troff's `\n` expressions) | No |

The other four are flagged as follow-up work if any user-facing symptom
turns up in ELI/FLAMES filtering, address parsing, equation rendering, or
roff numeric expressions — there is no reason to expect they *don't* work
now that the shared engine is fixed, but none of them were exercised this
session beyond a clean rebuild.

#### How this was found

Started from `claude-history/roadmap-old.md`'s amsdemo thread: caption dates displaying wrong
("7-Jul-126") and demo message ordering (Part 1…23) scrambled. Two real,
smaller bugs were found and fixed first — a `tm_year % 100` Y2K display bug
in `bldcapt.c`/`shrkdate.c`, and a missing tiebreak in `recon.c`'s
sort-by-time comparator (`MsgListEntry_CompareTimes`) for messages sharing
the same one-second-resolution `AMS_DATE`. Neither fully explained the
ordering symptom: instrumenting `blddate.c` directly showed
`parsedateheader()` returning failure for all 23 real `gendemo` messages,
every time — not an occasional/data-dependent failure. A standalone test
harness (linking a byte-for-byte-identical-to-deployed sandbox rebuild of
`prsdate.c` against `libcparser.a`) reproduced the same universal failure
outside the full application, confirming it wasn't specific to the `recon`
call path. `parser_SetDebug(1)` state-machine tracing, cross-referenced
against bison's own `-v`/`.output` state and rule listings and the raw
generated tables (`yypact`, `yytable`, `yycheck`, `yydefact`, `yypgoto`,
`yydefgoto`, `yyr1`), pinpointed each of the three engine bugs above by
direct comparison against bison's documented/generated reference behavior
for the identical grammar.

#### Relationship to the LP64 bug family (§12)

A close cousin, not a member of that family — same underlying *shape*, a
different mechanism. Both are: 1990s C code that hardcoded an assumption
about a fixed data width or representation, which a *different* tool
further down the modern toolchain silently violated decades later, and
which an untyped/blind access (here, a raw pointer cast across a width
mismatch; there, register-level zero-extension through an untyped `void
(*)()` vtable dispatch on an LP64 ABI) then corrupts without any compiler
error to flag it. The lesson is identical even though the failure mode
isn't: code that assumes "this will always be N bits" is fragile against
*any* link in the toolchain — compiler, ABI, or in this case, a
code-generator's own storage-optimization choices — deciding otherwise for
reasons invisible at the point where the assumption was written.

#### Verification

Static: all five grammars' generated `.c` files recompile clean against the
fixed engine (0 `error:` lines across a full `dependInstall`). Dynamic:
`prsdate` verified end-to-end via `gendemo` — every previously-failing
test input (including the literal `arpadate()` output format, `"Jul 10"`
with no year, and pure-numeric `"7/10/1992"`) now parses to the correct
`tm_year`/`tm_mon`/`tm_mday`/`tm_hour`/`tm_min`/`tm_sec`, both in the
standalone harness and against the actual deployed `build/lib` libraries.
The other four grammars (`eliy`, `parsey`, `eqparse`, `num`) are unverified
beyond a clean compile/link — see the scope table above.

### 16. classpp typed-dispatch signedness mismatch: `.ch` declared type vs. implementation's actual type

**Status:** closed 2026-07-11. MEDIUM effort.

#### Root cause

M1's classpp typed-dispatch conversion (§12 point 11) generates each
method's caller-side dispatch macro by casting the vtable slot to the
return/parameter types declared in that class's `.ch` spec — e.g.
`Superior_Image_Line_Width(zip_type_image image) returns char;` in
`contrib/zip/lib/zip.ch` generates:

```c
#define zip_Superior_Image_Line_Width(self,_a1) \
    ((* ((char (*)(struct zip *, zip_type_image))(...))) (self,_a1))
```

This assumes the `.ch` declaration and the real C implementation agree on
type — but nothing enforces that. `zip__Superior_Image_Line_Width`
(`zipdf01.c`) is explicitly defined `unsigned char`-returning, using `255`
as its "nothing configured" sentinel, walking a figure→image→
superior-image→stream inheritance chain. The `.ch` said plain (signed)
`char`.

At default optimization, the caller
(`zip__Contextual_Figure_Line_Width`, `zipd000.c`) computed its
`(width = zip_Superior_Image_Line_Width(...)) != 255` check by exploiting
the *declared* signedness: since a signed `char` value of `255` sign-
extends to `-1`, the optimizer rewrote the comparison as `cmn w0, #1`
(`== -1`) instead of the plain `cmp w0, #0xff` used a few lines earlier
for the figure's/image's own direct (correctly `unsigned char`-typed)
field reads. But the callee's *actual* `unsigned char` return zero-extends
`255` to `0x000000FF` in the return register, not `0xFFFFFFFF` — the two
never compare equal, so the caller always concluded "found a real width"
and returned **255** even when nothing was configured anywhere in the
chain. That `255` flowed straight into `zipview_SetLineWidth(self, 255)`
in `zipv000.c`'s `Ensure_Line_Attributes`, producing a 255-pixel-wide
stroke that filled the entire figure with solid foreground color — the
zip inset "solid black rectangle" bug (see `claude-history/roadmap-old.md`
→ Insets to Repair → zip, and `claude-history/zip-black-render-investigation.md` for the full
bisection trail).

`-O0` "fixed" the symptom by accident: at that optimization level the
compiler emits a naive truncate-the-return-value-then-compare-as-unsigned
sequence instead of the sign-extension shortcut, which happens to still
produce the right answer regardless of the declared/actual signedness
disagreement — which is exactly why this class of bug is easy to miss:
the code *looks* correct, compiles clean, and only misbehaves once the
optimizer is aggressive enough to exploit the (wrong) signedness contract
implied by the `.ch` declaration.

#### Fix

One-line type correction in the `.ch` spec to match the implementation:
```
-  Superior_Image_Line_Width( zip_type_image image )   returns char;
+  Superior_Image_Line_Width( zip_type_image image )   returns unsigned char;
```
followed by `make zip.eh zip.ih` (classpp regeneration) and a normal
rebuild — no source change needed in either the caller or the callee,
since both were internally consistent; only the *contract between them*
(the `.ch` declaration) was wrong. Confirmed working end-to-end at normal
default optimization, no per-file/per-function flags needed anywhere.

#### Scope: tree-wide, not zip-specific

This is a systemic risk of the M1 typed-dispatch conversion itself, not a
zip peculiarity — any class whose `.ch` narrow-type declaration
(`char`/`short`) disagrees in signedness with its C implementation's
actual return type is a candidate, *if* that method also uses a sentinel
value whose sign-extended and zero-extended bit patterns differ (a
narrow-type equivalent of the LP64 family's core failure shape, but not
an LP64/pointer-width issue at all — this reproduces identically on
ILP32). Swept tree-wide: compared all `returns char;`/`returns short;`
(and the mirror, `returns unsigned char;`/`returns unsigned short;`)
declarations across all 566 `.ch` files in the tree against their actual
implementations' explicit return-type declarations, in both directions.
**Zero other live instances found** — `Superior_Image_Line_Width` was the
only one. Re-run after any future bulk `.ch` editing or when enabling a
previously-inert subtree (M1's typed-dispatch flip only covers the
*active* tree — see §12's census — so a `.ch`/implementation mismatch in
an inert directory wouldn't have been caught by compilation and could
still be latent there).

**Audit query** (compares declared vs. actual return type for every
narrow-typed method; adapt the risky-type set and search directories as
needed):
```
# see revival/tools or ask for the scan script used for this sweep —
# not yet checked in as a standalone tool; walks every .ch file's
# "Method(...) returns TYPE;" lines for TYPE in {char, short, unsigned
# char, unsigned short}, resolves the real class name from that .ch's
# own "class NAME[...] : parent" line (NOT the filename — several zip
# classes differ, e.g. zipofcap.ch declares class zipofcapt), and
# diffs against the explicit return-type token on the implementation's
# definition line in every .c file in the same directory.
```

#### Relationship to the LP64 bug family (§12)

Same shape as §15's relationship to that family: a close cousin, not a
member. The LP64 family (§12) is about *register width* — a 32-bit value
crossing an untyped dispatch boundary into a 64-bit-declared parameter
without the sign-extension a typed call would have inserted. This bug is
about *signedness*, at a fixed width (8 bits) — a value crossing a
*typed* (post-M1) dispatch boundary where the type itself, not just the
call site's type-erasure, disagrees between declaration and
implementation. Different mechanism, same lesson: any place where two
sides of a call boundary can silently disagree about how to interpret the
same bits is a latent bug, whether the disagreement is over width (LP64
family) or signedness (this one) — and both are invisible until an
optimizer is willing to exploit the (wrong) contract.

#### Verification

Static: tree-wide scan (566 `.ch` files, both signedness directions)
found zero other instances after the fix. Dynamic: zip inset renders
correctly at normal default `-O` (previously required `-O0` or
per-function `__attribute__((optnone))` to work at all), confirmed
end-to-end against both `src/doc/papers/atk/Cattey.turnin` and
`contrib/zip/samples/dragon.zip`, and against a full `make Clean; make
dependInstall` world rebuild.

### 17. Xft "erase by redraw" uses stale foreground color in WHITE transfer mode

**Status:** RESOLVED 2026-07-12. All three bugs that surfaced during this
investigation — the stale-foreground-color bug below, the anti-aliasing
ghost-residue bug, and the incremental-redraw "missing leading
characters" bug it led to — were root-caused and fixed the same day. See
"Follow-up 2026-07-12" below.

#### Root cause

`graphic_WHITE` transfer mode is the "erase by redrawing in background
color" convention used tree-wide (e.g. `aptv__ClearBoundedString`).
`xgraphic_LocalSetTransferFunction` (`xgraphic.c`, ~line 408-417)
implements the color swap for the *core X* rendering path only —
`XSetForeground(GC, self->backgroundpixel)` — which changes the X
graphics context's current foreground pixel, but never touches
`self->foregroundpixel` itself. Xft rendering doesn't use the GC's
foreground pixel at all; `GetXftForeColor` (`xgraphic.c`, ~line 68) read
`self->foregroundpixel` directly, unconditionally — meaning a WHITE-mode
Xft text draw was silently using the **old, unswapped foreground color**
(typically black). "Erasing" text by redrawing it in white was actually
redrawing it in black — reinforcing the old text rather than erasing it.

Found via `contrib/calc`'s display area (see `claude-history/roadmap-old.md`
→ Insets to Repair → calc, and `claude-history/calc-text-rendering-investigation.md`), which
exercises frequent `Clear`-then-`Draw` cycles as its digit display
updates — a pattern uncommon enough elsewhere in the active tree that
this had gone unnoticed.

#### Fix

`GetXftForeColor` now checks the current transfer mode and substitutes
`self->backgroundpixel` when in `graphic_WHITE` mode:
```c
unsigned long fgpixel = (self->header.graphic.transferMode == graphic_WHITE)
    ? self->backgroundpixel : self->foregroundpixel;
```
**Confirmed via live lldb trace**, not just static reasoning: breakpointed
`XQueryColor` (the point where the resolved pixel value is actually used)
across a full calc keystroke sequence — every `ClearBoundedString` call
now requests `0xFFFFFF`, every `DrawBoundedString` call requests
`0x000000`, perfectly alternating. This part of the bug is resolved.

#### Scope: tree-wide, not calc-specific

Like §16, this is a general core-ATK bug, not specific to the inset that
happened to surface it. Any Xft-rendered view using WHITE-mode
erase-by-redraw was affected. A related symptom — a faint "ghost" of
prior text remaining visible after an erase/redraw cycle — persisted in
`contrib/calc`'s display area even after this fix, and was proven (via
the same lldb-trace methodology) to **not** be a further instance of
this same color bug, nor a content or draw/erase-position bug at the
API level traced (`aptv__DrawBoundedString`/`ClearBoundedString`
arguments were correct and self-consistent in every case checked) — see
the follow-up below, where it was root-caused and fixed separately.

#### Follow-up 2026-07-12: ghost root-caused and fixed; new redraw bug surfaced

The residual ghost turned out to be a *different* bug in the same
function: `xgraphic_DrawChars`'s Xft path erases text by redrawing the
same glyphs in the background color, which only exactly restores pixels
where a glyph's anti-aliasing alpha is 1 — partially-covered edge
pixels stay gray forever and accumulate across draw/erase cycles. Fixed
by filling the glyph's advance-cell rectangle with the background color
(`XftDrawRect`) instead of redrawing glyph shapes, whenever
`transferMode == graphic_WHITE`. User-confirmed in `ez`: the ghost is
gone.

That fix immediately surfaced a **new, distinct** bug: during incremental
multi-keystroke redraws, the calc display showed only a *suffix* of the
correct string (leading characters missing — e.g. typing `123+4=` showed
`1`, `2`, `23`, `3+`, `23+4` instead of `1`, `12`, `123`, `123+`,
`123+4`). The final `=` result always drew correctly, and a forced full
repaint (window focus-loss/regain) showed the correct string, so calc's
own value tracking was fine — this was purely a defect in the incremental
Clear/Draw redraw path. See `claude-history/calc-ghost-fix-prompt.md`'s
"Outcome" section and `claude-history/calc-text-rendering-investigation.md`
for the full trail.

**Root-caused and fixed the same day.** Traced (via a live `XGetImage`
framebuffer readback in `xgraphic_DrawChars`'s Xft path) to rootless
XQuartz not reliably recompositing the native window surface for
Xft/Render draws on their own — the server-side pixels were always
correct, but didn't reach the visible window until some unrelated event
(a focus change) forced a full recomposite. Fixed with a self-`XCopyArea`
kick (through the core-X path) after each Xft draw, forcing the
compositor to recognize the region as dirty. Confirmed by the user for
both the calc digit-display symptom and a second, broader instance of
the same root cause (text typed above a calc inset going temporarily
invisible during relayout). Full detail: `porting-changelog.md`'s
2026-07-12 "calc inset missing leading characters" entry.

No tree-wide audit for *other* latent instances of the specific
`GetXftForeColor` bug has been done (unlike §16's exhaustive `.ch` sweep)
— the fix is in the single shared function all Xft text rendering
funnels through, so no other call sites need auditing; this note is
about the fix's blast radius (affects every WHITE-mode Xft draw
tree-wide) rather than about finding more instances.

#### Verification

Static: single fix in the one shared function
(`xgraphic.c GetXftForeColor`) all Xft rendering funnels through — no
other call sites to audit. Dynamic: `XQueryColor` lldb trace across a
full `123+4=` keystroke sequence in `contrib/calc`, confirmed correct
color alternation at every step. Full `make Clean && make dependInstall`
world rebuild done 2026-07-12, zero new errors introduced (one
pre-existing, unrelated `contrib/zip/utility/ltapp.c` error remains, see
`claude-history/roadmap-old.md` → Insets to Repair → zip). The ghost-residue and
incremental-redraw follow-on fixes were each confirmed live by the user
in `ez` the same day, as described above.

### 18. Variadic function called through a K&R (empty-parens) extern declaration — arm64 calling-convention mismatch

**Status:** RESOLVED 2026-07-22. The one instance found was fixed and
confirmed via repeated live repro; kept here as a bug-class reference for
any future new variadic function (see Scope below).

#### Root cause

Apple's arm64 (AAPCS64) ABI passes variadic arguments differently from
fixed arguments at the call site: named/fixed arguments go in registers,
but once a call crosses into its variadic tail, the compiler must know
that *at the call site* to generate the stack-passing code the callee's
`va_start`/`va_arg` machinery expects. A K&R-style empty-parens
declaration (`extern int Foo();`) gives the compiler no arity or
variadic information, so a call through it is code-generated as if every
argument were fixed (registers). If the actual definition is genuinely
variadic (`Foo(fmt, ...)`, reading with `va_arg`), the callee reads
arguments from the stack that the caller never put there — silent
garbage, not a crash at the call site itself, and no compiler warning
(an empty-parens extern is legal, unprototyped C).

Found building the IMAP writeback change-journal (`ams/libs/ms/msjournal.c`,
`revival/doc/ams-IMAP-project.md` Milestone 4): a first-cut K&R extern for
the new variadic `MSJournal_Record(dir, fmt, ...)` compiled clean but
crashed `cui` live on the first real folder mutation — `EXC_BAD_ACCESS`
inside `vsnprintf`, called from `MSJournal_Record`, called from
`MS_AlterSnapshot`. Confirmed via lldb backtrace; see
`revival/doc/claude-history/imap-writeback-REPORT.md`.

#### Symptom signature

Distinct from the LP64 truncation family below: not a huge-positive-number
value, but a crash *inside* a variadic libc function
(`vsnprintf`/`vfprintf`/`vprintf`) one frame below a plausible, otherwise-
correct-looking call site. The call site itself never looks wrong in the
source.

#### Fix

A full prototype with `...` at every call site of the new function
(`extern void MSJournal_Record(const char *dir, const char *fmt, ...);`),
not just at its definition. Confirmed fixed by rebuilding and repeating
the exact live crash repro cleanly, multiple times.

#### Scope

Applies to any *new* variadic function added anywhere in this tree, not
to existing ones (existing variadic libc/AUIS functions already have
correct prototypes wherever they're currently called, or they'd already
be crashing). Relevant going forward whenever a delegated session or
future porting work introduces a new function with a `...` parameter —
check every call site has the real prototype in scope, not an
empty-parens or otherwise unprototyped declaration.

#### Relationship to the LP64 bug family (§12)

Adjacent but distinct: §12 and its relatives are about `int`/`long`/
pointer *width* mismatches surviving 32-to-64-bit widening. This bug is a
*calling-convention* mismatch (register vs. stack argument passing) —
it would reproduce identically even on a hypothetical ILP64 rebuild of
the original 1991 code, because the defect is "declaration doesn't say
variadic," not "declaration says the wrong width." Grouped here because
both families share the same root pathology: K&R-era declarations
carrying too little type information for a modern ABI to code-generate
correctly. Recorded in `revival/doc/sonnet-playbook.md`'s LP64
bug-class list as item 6 (with the same distinguishing note) since that
list is what delegated sessions read first when debugging a crash.

#### Verification

Live lldb backtrace showing the fault inside `vsnprintf` before the fix;
clean repeated live repro (real folder mutations via `cui` against a
mirrored folder) after the fix, no further crashes. No tree-wide audit
needed — this is the only new variadic function added in this work, and
no existing variadic function in the tree was touched.

### 19. `.ch`/wrapper vs. real K&R implementation out-param width drift — invisible across the untyped call boundary

**Status:** 5 known instances found and fixed. Not resolved tree-wide —
see Scope below: the sweep that found instances 2–4 was narrow and had
to be re-run once already after missing two on a declaration-style
technicality, so a thorough, purpose-built sweep of every `ams/libs/ms`
function's real parameter declaration against its `.ch` entry is still
open. MEDIUM effort.

#### Root cause

Several `ams/libs/ms` functions are exposed as class methods: a hand-written
K&R wrapper (`amss__Foo`/`ams__Foo`/`amsn__Foo`) whose parameter types are
copied from the class's `.ch` spec, forwarding unchanged to a bare global
function (`Foo(...)` in `ams/libs/ms/*.c`) that has its *own*,
independently-written K&R parameter declaration. Nothing checks these two
declarations agree — the wrapper-to-bare-function call is unprototyped
K&R, so if the `.ch` spec says `long *x` and the real function says
`int *x`, the compiler has no way to see the conflict. classpp's typed
dispatch (§12/§16) only checks *caller → wrapper*; it has no visibility
past the wrapper into the real implementation.

**Confirmed via `fossil blame` to be original 1990s source, not a
porting-introduced regression**: for every instance below, both the
`.ch` `long *` declaration and the real function's `int *` parameter
trace to `b28115fb2e`, "Initial import of AUIS sources" (2026-06-24) —
before any Darwin/LP64 porting work existed. On the original ILP32
targets this code shipped on, `int` and `long` are both 4 bytes, so the
mismatch was byte-for-byte harmless there; it only becomes live on a
platform where `sizeof(long) > sizeof(int)`, i.e. this LP64 port. A
~35-year-old inconsistency, dormant until now.

When this drifts, the real function only ever writes the low 4 bytes at
the address it's given, regardless of what the caller allocated:
- If the caller's local is genuinely `int`-sized (matching the *real*
  function, not `.ch`), the write is correct by accident — this is how
  three of the five instances below survived unnoticed until this port:
  the `.ch`/wrapper mismatch produces a compiler warning at the *caller*
  (`-Wincompatible-pointer-types`, since the caller's `int*` doesn't match
  the wrapper's typed `long*`), but the actual runtime write was harmless
  because the real function never touched more than 4 bytes anyway. This
  warning is exactly what the M2 point-0 census's Group A category
  catches and, by its general rule, fixes by widening the caller to
  `long` to match `.ch`.
- If the caller's local *is* widened to `long` — whether by hand, or (as
  happened live in this tree) by mechanically applying Group A's general
  rule without first checking the real implementation — the low 4 bytes
  get the correct value and the upper 4 bytes keep whatever was already
  on the stack: silently correct if the local happened to be
  zero-initialized and the value never exceeds 2^31, silently wrong (a
  huge garbage number) otherwise. **No compiler warning either way** —
  this is the dangerous direction, and the one that bit this tree live
  (see instances 3–4 below): the general Group A rule is only safe once
  the real implementation has been checked, which the stretch-goal sweep
  exists to do, but initially missed these four on a grep technicality.

#### Symptom signature

None at compile time. At runtime: either no symptom (accidental width
match), or a value that reads as garbage — typically a huge number,
sometimes negative — coming out of a `long` local that was passed by
address to one of the affected functions and never itself written to
afterward. Distinct from LP64 variant #4 (scanf) in that no `%d`/`%ld`
format string is involved; distinct from §16 (classpp signedness) in
that the mismatch is one hop further out, past the wrapper, into a
second independently-declared K&R function.

#### Instances found (chronological)

1. **`MS_GetConfigurationParameters`** (`ams/libs/ms/init.c`) — first
   found 2026-07-18. Recorded in `sonnet-playbook.md`'s LP64 bug-class
   list as item 4, but never added here or to `porting-changelog.md`
   until now.
2. **`MS_ParseDate`** (`ams/libs/ms/msparse.c`) — found 2026-07-24
   during the M2 point-0 census's stretch-goal sweep (grepping
   `ams/libs/ms`/`ams/libs/cui` for `int *` out-params with no matching
   warning). Real implementation: `int *year, *month, *day, *hour,
   *min, *sec, *wday`, but `int *gtm` too — except every other caller in
   the tree (`ms.c`, `cui.c` ×3, `vuibase.c`, `vuipnl.c`) already
   declares `gtm` as `long` (it holds a `time_t`/`gtime()` result, which
   genuinely needs 64 bits), leaving only `year`..`wday` as `int`.
   **Fixed by widening the real implementation's `gtm` parameter to
   `long *`** (matching `gtime()`'s actual return width and every
   existing bare caller), and widening `.ch`/the three wrappers to
   match, keeping `year`..`wday` as `int *` throughout. The one caller
   reached through class dispatch (`captions__MarkRangeOfMessages`) had
   all eight locals declared `long` — confirmed via `fossil blame` to be
   original 1990s source (same initial-import commit as everything
   else here), not a later widening — with **uninitialized** `year`..`wday`
   locals, worse than the usual accidental-zero-init survival case, and
   live (reached by the "mark messages since/through date" feature).
3–4. **`MS_GetDirInfo`, `MS_GetNewMessageCount`, `MS_GetSubscriptionEntry`,
   `MS_NameChangedMapFile`** (`ams/libs/ms/getdiri.c`, `getnmct.c`,
   `getsubs.c`, `namechg.c`) — found 2026-07-24, initially *missed* by
   the same stretch-goal grep because it only checked the line
   immediately following a function's signature line; these four
   declare their `int *` parameters two or more lines down (some behind
   a `long    Foo (...)` return-type-prefixed signature line, which the
   grep's pattern didn't match at all). All four real implementations
   take `int *` out-params; `.ch` had all four typed `long *` — both
   confirmed via `fossil blame` to be original 1990s source (same
   initial-import commit), not a Darwin-era change. Every *other* caller
   in the tree (`ms.c`, `cui.c`, `cuifns.c`, `vuibase.c`) already
   correctly used `int` locals, matching the real functions — that's
   exactly what let the mismatch sit dormant: it only produces a
   compiler warning at the three class-dispatch callers
   (`atkams/messages/lib/capaux.c`, `folders.c`, `foldaux.c`), which
   originally also used `int` (matching the real functions and
   accidentally correct). **This session's own Group A fixing pass**
   — applying the M2 point-0 census's general rule of widening a
   warning-flagged caller to match `.ch`'s `long *` — widened those
   three callers' locals to `long` without first checking the real
   implementation, which is exactly what turned the dormant mismatch
   live. **Corrected the opposite way from `MS_ParseDate`: narrowed
   `.ch` and the three wrappers back to `int *`** (since the real
   implementations and every bare-call caller were unanimous), and
   reverted the three `atkams/messages/lib` callers' locals back to
   `int`, undoing that same-session widening. Confirmed live: after the
   erroneous widening, opening `messages` and viewing Inbox produced
   "Zero of your two subscriptions have changed, (-<huge number>) have
   nothing new" from `MS_NameChangedMapFile`'s uninitialized upper 32
   bits; after the correction, wdc confirmed the message reads correctly
   ("Zero of your two subscriptions have changed. (2 have nothing
   new.)").

#### Fix

No single fix — this bug class requires checking the *real* K&R
implementation's declared types directly, not trusting `.ch`, before
touching any out-param width. Direction determines the correct fix:
- If the real implementation's width is idiosyncratic and every other
  caller already independently agrees on the wider type for a specific
  parameter (as `gtm` did), widen the real implementation to match —
  the `.ch`/wrapper were "ahead of" a stale real body.
- If the real implementation and every other caller agree on the
  narrower type, and only the `.ch`-derived class-dispatch caller(s)
  disagree, narrow `.ch`/the wrappers back — `.ch` was wrong from the
  start (original source, not a porting artifact), and widening the
  caller to match it (the general Group A rule) was the mistake, not
  the real body.
Either way, the caller-side local width must end up matching the *real*
K&R function's declared width, not the class spec's, since it's the real
function that ultimately performs the store.

#### Scope

Confirmed limited to `ams/libs/ms` functions exposed through the
`amss`/`ams`/`amsn` class wrappers — this is where the wrapper-forwards-
to-independently-declared-bare-function shape occurs. Not yet swept
tree-wide; the two grep passes that found instances 2–4 (stretch-goal
sweep during M2 point 0, `revival/doc/claude-history/m2-census-REPORT.md`)
were both narrow and had to be run twice after the first missed instances
3–4 on a declaration-style technicality. A thorough sweep would need to
check every `ams/libs/ms` function's real parameter declaration against
its `.ch` entry directly (e.g. diffing extracted signatures), not grep
for a specific K&R declaration shape.

#### Relationship to the LP64 bug family (§12)

Same underlying pathology as items 4/6 in `sonnet-playbook.md`'s LP64
list (`§12`'s cousins) — a width or ABI mismatch invisible to the
compiler because it crosses an unprototyped K&R call. Distinguishing
feature here: the mismatch is specifically between a `.ch` spec (and its
mechanically-generated wrapper) and a *separately hand-written* real
implementation one call further out, not between a caller and its
immediate callee.

#### Verification

Instance 2 (`MS_ParseDate`): full rebuild clean, zero new warnings; live
smoke test of "mark messages since/through date" not independently
re-verified after the fix (recommended before relying on it further).
Instances 3–4: full rebuild clean, zero new warnings; live smoke test by
wdc — Inbox's subscription-status message read correctly after the
correction, confirmed garbled before it (see above).

### 20. `ansify` DRIFT false-positive: classpp's own `InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing

**Status:** Documented 2026-07-25 (M3 tree-wide census). Not itself an
action item beyond consulting this section during future DRIFT triage —
the tool-side fix in `ansify` is deliberately deferred (§14's delegation
ruling keeps tool construction top-level). Two live instances remain
unchecked: `contrib/zip/utility/schedv.ch`/`ltv.ch` (Wave 7).

A tree-wide `ansify --dry-run --dir src` census (M3's "first concrete
step," run before any batch execution) found 56 DRIFT findings across
1,486 files. Not all are real `.ch`-vs-`.c` bugs — a majority are a
tool-side false positive with a confirmed, code-level root cause.

#### Root cause

`ansify`'s DRIFT check (`revival/tools/ansify`, `convert_file`) assumes
exactly **one** implicit leading parameter for every class method/
classproc — `self` for methods, `classID` for classprocs — and compares
`len(.c params)` against `len(.ch declared args) + 1`. That convention
is what classpp itself uses for ordinary methods and classprocs, but
**not** for three specially-named classprocs, confirmed directly in
`overhead/class/pp/class.c`:

- `InitializeObject`/`FinalizeObject`: classpp hardcodes a full 2-arg
  prototype (`struct classheader *`, `struct <class> *` — i.e. both
  `classID` AND `self`) unconditionally (`class.c:1122`,
  `"boolean %s__InitializeObject(struct classheader *, struct %s *);"`),
  regardless of what the `.ch` declares. Every real implementation
  therefore takes 2 implicit params, whether or not the `.ch` restates
  them.
- `InitializeClass`: dispatched via a fully untyped `(boolean (*)())`
  cast (`class.c:1202`) with no compiler-enforced arg count at all —
  the real-world convention is 1 implicit param (`classID` only, no
  specific instance exists yet at class-init time), but nothing
  enforces it.

A related but distinct mechanism, traced precisely by AMS2
(2026-08-01): whether `InitializeObject` gets a live, auto-wired call
at all — independent of the DRIFT-vs-signature question above —
is controlled by `class.c:2814`, which sets the `initializeobject`
flag `TRUE` for **any** class with a non-empty `data:` section,
whether or not `InitializeObject` appears in the `.ch`'s
`classprocedures` section (`class.c:165`'s own comment: `"TRUE if this
class has data or initializeobject procedure was found"`). This is the
exact mechanism behind `ansify`'s "no signature in DB" skip for a
class with a `data:` section but no declared `InitializeObject`
(`atk/apt/chart/chartx1a.ch`/`chartx1app`, I2;
`atkams/messages/lib`'s `messagesapp`/`text822`, AMS2 — see
`claude-history/m3/m3-rollout-runbook.md`'s I2 and AMS2 entries) — the DB only captures classprocs a `.ch` explicitly
declares, but classpp wires the call regardless of the `.ch`, so these
are genuine live gaps, not dead code, and need hand-conversion.

`ansify`'s DRIFT check doesn't know about this special-casing, so it
misfires in two shapes depending on how the `.ch` happens to be
written:

- **`.ch` restates the implicit param by name** (e.g. `fldtreev.ch`'s
  `InitializeClass(struct classheader *classID) returns boolean;`):
  the tool counts `classID` as a real declared arg, then expects one
  *more* on top (`dbargs + 1`) — one too many. Reported as `.c has N
  params, .ch has N+1`.
- **`.ch` uses empty parens** (the more common, "undecorated" form,
  e.g. `suite.ch`'s `InitializeObject() returns boolean;`): the tool
  expects only the single default implicit param, but
  `InitializeObject`/`FinalizeObject`'s true convention is 2 — one too
  few. Reported as `.c has 2 params, .ch has 0+1`.

#### Scope of the false positive in the 2026-07-25 census

42 of the 56 DRIFT findings name exactly `InitializeClass`,
`InitializeObject`, or `FinalizeObject`. The mechanism above was
directly verified against two of them end-to-end (`.ch`, `.c`, and the
`class.c` codegen source all cross-checked) —
`foldertreev__InitializeClass`/`InitializeObject`/`FinalizeObject`
(`atkams/messages/lib/fldtreev.c`, restated-param shape) and
`suite__InitializeObject`/`FinalizeObject` (`atk/apt/suite/suite.c`,
empty-parens shape). The remaining ~39 share the identical DRIFT-
message shape against the same three method names and are almost
certainly the same mechanism, but were not each individually
hand-verified — treat as very likely false positives, not certain
ones, and do a 30-second sanity check (does the `.c` definition's real
param count match 1 for `InitializeClass` or 2 for
`InitializeObject`/`FinalizeObject`?) rather than blind-trusting the
label when a batch actually reaches one.

**One confirmed real exception**: `dialog__InitializeClass`
(`atk/utils/dialog.c`) genuinely defines `(classID, self)` — 2 params
— where the ordinary `InitializeClass` convention is 1. Since
`InitializeClass` dispatch is untyped, this never mattered at runtime
(the generated call site only ever passes `classID`; the extra `self`
parameter reads whatever garbage is in that argument slot, but the
implementation returns `TRUE` unconditionally without touching it) —
a real, ~35-year-old, benign interface inconsistency, not a DRIFT-
shaped tool artifact. Logged here, not fixed (out of scope, no
observable effect).

#### A second real exception, the opposite direction — found M3 batch B3 (2026-07-30)

Unlike `InitializeObject`, `InitializeClass` is **not** hardcoded by
classpp — it goes through the ordinary classproc-emission loop, which
always prefixes `struct classheader *` and appends whatever the `.ch`
declares verbatim. A `.ch` that restates the implicit param by name
(`InitializeClass(struct foo *self) returns boolean;`) therefore gets
counted as an *extra* argument on top of the automatic prefix,
producing a 2-param exported prototype. Confirmed by directly
test-compiling `atk/textobjects/unknownv.eh` under a temporary `-pe`
flag: `boolean unknownv__InitializeClass(struct classheader *, struct
unknownv *);` — 2 params — against `unknownv.c`'s real, correct,
1-param definition (`unknownv__InitializeClass(c) struct classheader
*c;`). Same mechanism hits `FinalizeObject` when a `.ch` restates
*both* implicit params instead of just `self` (`atk/apt/suite/suiteev.ch`
declared `FinalizeObject(struct classheader *ClassID, struct suiteev
*self)`, producing a 3-param prototype against a real 2-param
definition — confirmed the same way).

**This is not universal — check the real `.c` param count before
assuming either direction.** Three pre-existing instances of the
restated-`InitializeClass` shape in `atk/value`
(`metextv.ch`/`eintv.ch`/`etextv.ch`) are *not* bugs: their `.c`
definitions already, correctly, take the full 2 real params (already
`-pe`'d and committed in B2) — the unused 2nd param just reads
garbage, exactly the same harmless shape as `dialog__InitializeClass`
above. The difference between "safe" and "broken" is entirely whether
the real `.c` definition happens to match the inflated count, which
must be checked per instance, not assumed from the `.ch` shape alone.

#### `FinalizeObject` is not symmetric with `InitializeClass` — found M3 batch C1 (2026-08-01)

`InitializeClass` and `FinalizeObject` look like the same shape (both
go through the ordinary classproc-emission loop, both get an automatic
`struct classheader *` prefix) but their *safe* `.ch` forms differ, and
conflating them produces a wrong fix. Traced directly against
`class.c` (~line 1146-1153): for `FinalizeObject` specifically, when
the `.ch`'s restated arg list is empty (`mp->realargtypes == NULL ||
mp->realargtypes[0] == '\0'`), classpp takes a **special hardcoded
branch** that emits `struct classheader *, struct CLASSNAME *self` —
2 params — regardless of whether the `.ch` used true empty parens
(`FinalizeObject();`) or restated just `self`
(`FinalizeObject( struct CLASSNAME *self );`); both produce identical
`realargtypes` state and hit the same branch. `InitializeClass` has no
such hardcoded branch for the ordinary classproc loop — empty parens
there really does mean 1 param (`classID` only), and *any* restatement
(even self-only) genuinely over-counts. **The safe/established `.ch`
form for `FinalizeObject` is the self-only restatement, not empty
parens** — confirmed against 11 real precedents tree-wide (`atk/eq/
eq.c`, `atkams/messages/lib/fldtreev.c`/`mailobjv.c`, and 8 more), all
using the 2-param form with a self-only-restated `.ch`. A `.ch` whose
`FinalizeObject` genuinely has only 1 real `.c` param needs the `.c`
widened to 2 (`classID` unused) — not the `.ch` simplified further to
empty parens, which would change nothing about the exported prototype
and leave the arity mismatch in place. (`contrib/zip/lib/zipstat.ch`
was exactly this case: already correctly self-only-restated; only
`zipstat.c`'s definition needed widening from 1 param to 2.)

Fixed (B3) by simplifying the 3 broken `.ch` declarations
(`atk/textobjects/unknownv.ch`'s `InitializeClass`,
`atk/apt/suite/suiteev.ch`'s `InitializeClass` and `FinalizeObject`)
back to the true convention, rather than a classpp-level fix — a
tree-wide grep confirmed only 8 total restated-`InitializeClass`
instances and 1 double-restated-`FinalizeObject` instance exist
anywhere in the source tree, small and bounded enough not to warrant
touching the tool a third time in one day. **`atkams/messages/lib/
fldtreev.ch` (Wave 6) resolved 2026-08-01 (AMS2): broken, same shape
as `unknownv.ch`/`suiteev.ch`** — `InitializeClass` restated `classID`
by name (1 param) producing a 2-param exported prototype against the
real 1-param definition, and `FinalizeObject` restated both
`classID`/`self` (matching the double-restatement shape) producing 3
params against the real 2. Both fixed to the true convention (empty
parens / `self`-only), confirmed via a live `-pe` compile check before
the real run. **2 more live instances remain unchecked** —
`contrib/zip/utility/schedv.ch`/`ltv.ch` (Wave 7) — not yet checked
for which direction (safe like `metextv.ch`, or broken like
`unknownv.ch`) they resolve to; worth a 30-second per-instance check
(matching the method above) whenever that wave is prepared.

#### The other 14 DRIFT findings — mixed, ordinary per-batch triage

The remaining 14 (not one of the three special names) are NOT covered
by the mechanism above and should get normal DRIFT triage when their
directory's batch runs. Two were checked now because they were cheap
and instructive:

- **`tree__TreeWidth`/`TreeHeight`** (`atk/apt/tree/tree.c`) —
  confirmed genuine: `tree.ch` declares `TreeWidth() returns long;`
  (zero explicit args) but the implementation takes `(self, node)` —
  the interface is simply missing a real parameter, same species as
  Pilot B's typeless-`.ch` findings.
- **`menterstrV__WantInputFocus`/`clicklistV__WantInputFocus`**
  (`atk/value/mentstrv.c`, `clklistv.c`) — confirmed genuine:
  `view.ch`/`im.ch` declare `WantInputFocus(struct view *requestor)`,
  but both overrides define only `(self)` and hardcode
  `self->etextview` as the requestor argument to their delegated call
  instead of accepting the caller's real `requestor` — the interface
  parameter is silently dropped. Likely benign (compound-view focus
  forwarding) but a real, live interface violation, not a tool
  artifact — worth a closer look whenever `atk/value` is batched.

**`valueview__Changed`/`valueview__DrawFromScratch` (`atk/value`) — now
checked, M3 batch B2 (2026-07-30)**: same species as the
`WantInputFocus` pair above (a base-class virtual-method stub ignoring
a declared parameter it doesn't need — `DrawFromScratch`'s body is
literally `/* Subclass responsibility */`; every real subclass already
overrides it with the full, correct signature; `Changed`'s body is
similarly a no-op comment). Resolved with the same padding fix as
`WantInputFocus` (add the unused parameter, no body change) — see
`claude-history/m3/m3-rollout-runbook.md`'s B2 findings for full detail on all 4 of this
shape found in that batch.

Not yet individually checked: `type__GetDeclaration`
(directory TBD), `zipobject__Print_Object`/
`Normalize_Object_Points`/`Highlight_Object_Points`/
`Expose_Object_Points`/`Hide_Object_Points`, and
`zipstatus__Issue_Status_Message`/`Acknowledge_Status_Message`
(`contrib/zip/lib`, Wave 7) — no reason to believe these are tool
artifacts (none match the three-special-name pattern); handle as
ordinary DRIFT when those batches run.

#### Tool fix (not done)

The real fix belongs in `ansify` itself — special-case
`InitializeClass`/`InitializeObject`/`FinalizeObject` the same way
classpp's own codegen does, rather than assuming a uniform
one-implicit-param rule. Not fixed here, per the Delegation ruling
(§14): tool construction stays top-level. Until fixed, any M3 session
encountering a DRIFT report for one of these three names should
consult this section before escalating it as a real interface bug.

#### Related, but distinct: classpp's `FinalizeObject` prototype/call-site inconsistency (found M3 batch B2, 2026-07-30)

Not a DRIFT false positive — a genuine classpp codegen bug, confirmed
directly in `overhead/class/pp/class.c`. `InitializeObject` gets a
fully hardcoded 2-arg (`classID`, `self`) **exported prototype**
(`class.c:1121-1122`) regardless of what the `.ch` declares — this is
the mechanism §20 above documents. `FinalizeObject` does **not** get
the same treatment: its prototype is explicitly *not* skipped from the
ordinary classproc-emission loop (`class.c:1139-1142`, comment:
`"FinalizeObject is NOT skipped: it may have a non-void return type,
so it must go through the loop to pick up the correct
mp->methodtype"`), so its **prototype** is built from whatever the
`.ch` actually declares. But the **internal call site** inside the
generated `__Finalize` function (`class.c:1334`,
`"    %s__FinalizeObject(classID, self);\n"`) is unconditionally
hardcoded to pass 2 arguments, with no check against what the
prototype above it declared. Any class using the ordinary empty-parens
`FinalizeObject()` `.ch` convention (0 declared args → 1-arg
prototype, `classID` only) therefore gets a **self-inconsistent
`.eh`** the moment `-pe` is on: a hard compile error *inside the `.eh`
itself* (too many arguments to the internal call), independent of
anything the class's own `.c` does. Confirmed in `atk/value/buttonv.ch`/
`sliderv.ch`.

**Workaround** (applied twice so far, both in batch B2): restate
`FinalizeObject`'s `self` parameter explicitly in the `.ch`, matching
the class's real type (`FinalizeObject(struct buttonV *self);`) — this
routes the prototype through the same ordinary-classproc path but now
producing the matching 2-arg signature. Pure interface-side fix, no
`.c`/runtime-behavior change.

**Fixed centrally in classpp, 2026-07-30 (wdc's ruling)**: once the
retest scope was bounded precisely (the bug lives entirely behind
`usePrototypesExport`, i.e. `-pe`, which only 7 directories had turned
on at the time — every other directory in the tree is structurally
unreachable by this code path), fixing the tool once was clearly lower
total cost/risk than re-diagnosing this per-`.ch` for the rest of M3.

**The fix is narrower than mirroring `InitializeObject`'s full
hardcoded treatment** — that would have been wrong. 7 classes in
`atk/value` (`menttext.ch`, `entrtext.ch`, `clklistv.ch`, `entrintv.ch`,
`entrint.ch`, `mentstrv.ch`, `entrstrv.ch`) declare
`FinalizeObject(...) returns boolean` — a real, non-`void` return type
already handled correctly by the *existing* ordinary-classproc-loop
path (this is exactly what the code's own comment about
`FinalizeObject`'s non-`void` return type was warning about). The fix
only synthesizes the missing `self` parameter when `FinalizeObject` is
declared with **literal empty parens** (`mp->realargtypes` empty),
leaving every class that already restates `self` — with any return
type — going through the unchanged path:

```diff
 	    if (usePrototypesExport) {
-		sprintf(proto, "struct classheader *%s", mp->realargtypes);
+		if (strcmp(mp->name, "FinalizeObject") == 0
+		    && (mp->realargtypes == NULL || mp->realargtypes[0] == '\0')) {
+		    sprintf(proto, "struct classheader *, struct %s *", FinalClassName);
+		}
+		else {
+		    sprintf(proto, "struct classheader *%s", mp->realargtypes);
+		}
 	    }
```

**Verified two ways** (`claude-history/m3-classpp-finalizeobject-fix-REPORT.md`,
independently re-verified by the orchestrator): (1) zero retroactive
effect — all 124 `.eh` files across the 7 already-`-pe`'d directories
regenerate byte-identical before/after the fix, including `atk/value`'s
7 non-`void` overrides; (2) the fix actually works — a standalone toy
class reproduced the exact diagnosed compile error before the fix and
compiled clean after. One more live empty-parens instance found beyond
the two already known (`atk/org/orga.ch:72`) — not yet `-pe`'d, so not
live fallout, but this fix will make it self-consistent for free
whenever that directory's wave arrives.

#### A third, distinct classpp bug: an unnamed classproc parameter loses its type entirely (found M3 batch B2, 2026-07-30)

`atk/basics/x/xfontd.ch` declared `Deallocate(struct xfontdesc *);` —
correctly typed, but with no parameter *name*. `Allocate`/`Deallocate`
are (like `FinalizeObject`) call-site-hardcoded by classpp
(`class.c:1316`/`1391`); the unnamed parameter confuses classpp's
`realargtypes` construction enough that the emitted `.eh` prototype
came out as `void xfontdesc__Deallocate(struct classheader *, struct
*);` — the type name itself dropped, an uncompilable bare `struct *`.
Root cause not traced further into classpp's parser internals (out of
scope, same boundary as the finding above). **Workaround, confirmed
working**: give the parameter a name (`Deallocate(struct xfontdesc
*self);`) — routes it through classpp's normal named-parameter path
and produces the correct prototype. Checked for the same shape
elsewhere in batch B2 — not found again; may be rare (an unnamed
classproc parameter is unusual style to begin with), but worth the
same "note it if you see it" awareness as the other two classpp
findings above whenever a future batch's `-pe` rollout hits it.

### 21. `%d` / `%ld` mismatch in the write direction — printf/fprintf family

**Status:** RESOLVED 2026-07-26. Tree-wide sweep, ~60 sites across ~30
files fixed; see "Confirmed systemic" below.

Section 11 above (`scanf` family) noted in passing that "unlike `printf`
mismatches (wrong output, no memory write), `scanf` mismatches corrupt
the stack frame" — implying the `printf`-side mismatch was assumed
lower-stakes, cosmetic at worst. That assumption doesn't hold for one
important category: **ids that get written out and then read back and
matched against each other later.** For those, "wrong output" *is* data
loss, silently.

#### Root cause

The same ILP32-heritage mistake as #11, mirrored on the write side: code
that serializes a `long`-typed unique id (`dataobject_UniqueID()` —
literally `(long)(self)`, the object's own pointer — or
`dataobject_GetID()`/a `self->header.dataobject.id` field, also `long`)
into a `.ez` datastream's `\begindata{classname,ID}` / `\enddata{...}` /
view-tag markers via `fprintf`/`sprintf`, using `%d` instead of `%ld`. On
LP64 arm64, the caller still passes the full 64-bit value, but `%d` only
reads back the low 32 bits, silently truncating the id to its bottom
half. Harmless on the ILP32 platforms this shipped on (`long`/`int` both
32 bits); live only on LP64.

#### Why this one *does* corrupt data, unlike an ordinary printf typo

Found while root-causing calc/zip insets vanishing when embedded in a
mixed document and getting lost on save (`claude-history/roadmap-old.md`'s "Insets to
Repair" → zip/calc entry). The shared `apt__WriteObject` helper
(`atk/apt/apt/apt.c:524`, used by `calc`) wrote a **truncated** id into
`calc`'s own `\begindata`/`\enddata` tags, while the unrelated code that
writes the paired `\view{calcv,id,...}` reference (`atk/text/
text.c:1332`) computed the **full, untruncated** id fresh from the same
live pointer. Verified numerically: `4346500352 = 0x103125500` (view,
full) vs. `51533056 = 0x03125500` (calc's own tag, truncated) — identical
bits, missing top word. On reload, `dictionary_Insert` (keyed by the
truncated id) and the `\view{...}` line's `dictionary_LookUp` (keyed by
the full id) never match — the lookup fails **completely silently**
(`text.c:705-709`, a bare `return 0` on miss, no stderr) — so the object
just vanishes, and a subsequent save has nothing left to write back. No
crash, no error message, no stack corruption — just quietly wrong output
in exactly the field whose correctness the rest of the file format
depends on for cross-referencing. `zip` had an independent instance of
the identical mistake in its own `zip__Write` (`contrib/zip/lib/
zip.c:307,324`) — not shared code with `apt.c`, just the same K&R idiom
copy-pasted into a second file.

#### Confirmed systemic — tree-wide sweep (2026-07-26)

Grepping for the same `\begindata{%s,%d}` / `\enddata{%s,%d}` idiom
tree-wide turned up **~60 call sites across ~30 files** — essentially
every inset type's `Write()` implementation that was hand-written against
this same original template. All fixed (`%d`→`%ld`, spot-checked against
each site's declared type rather than blind-replaced — a few sites have
unrelated, genuinely-`int` `%d`s in the same format string, e.g. "
`Datastream version: %d`", that must stay untouched):

`atk/org/org.c`, `atk/eq/eqvcmds.c` (eq's Cut/Copy-to-cutbuffer path, not
its ordinary save — eq's normal save was already correct), `atk/lookz/
lookz.c`, `atk/utils/dialog.c`, `atk/supportviews/{label,lprruler,
sbutton,strtbl}.c`, `atk/createinset/null/null.c`, `atk/hyplink/{link,
pshbttn}.c`, `atk/bush/bush.c`, `atk/text/text.c` (the outer `text`
wrapper's own begindata/enddata pair — same bit-split arithmetic
confirmed independently), `atk/ness/objects/ness.c`, `atk/raster/cmd/
raster.c` (6 sites — including `raster__Write`, raster's own independent
main save path, meaning an embedded raster inset almost certainly had
the identical vanish-on-embed symptom as calc/zip, just never
previously reported/tested), `atkams/messages/lib/text822.c`,
`contrib/{alink,time/clock,time/timeoday,tm,zip/lib,champ/{chimp,
month},mit/util/header}`, and the `atk/examples/ex{11,12,13,16,17,18,19}/
hello.c` tutorial files. One sibling bug found by checking parameter
types rather than just format strings: `atk/basics/common/image.c`'s
`SendEndData`'s `id` parameter was declared plain `int` in both the
`.c` and the `.ch` class declaration, truncating the id before it ever
reached the `%d` — retyped to `long` in both files.

**Fix:** `%d`→`%ld` at each confirmed site; `image.ch`/`image.c`
additionally needed `int id`→`long id`. Full file/line inventory and
per-site skip/false-positive reasoning: `project_lp64_printf_id_truncation`
memory (Claude auto-memory, this project).

**To audit for regressions or missed sites:**
```
grep -rn '\\\\begindata{%s,%d}\|\\\\enddata{%s,%d}\|"data{%s, *%d}' src/ --include="*.c"
```
For each hit, confirm the argument bound to `%d` is genuinely `int` (fine)
vs. a `long`/`dataobject_UniqueID()`/`*_GetID()` call (needs `%ld`) — do
not blind-replace, several sites mix a genuine `int` (a version number, a
count) with the `long` id in the same format string.

### 22. On-disk binary formats hard-coded around a 4-byte `long` — struct layout and hand-rolled word loops

**Status:** closed 2026-08-08. MEDIUM effort.

#### Root cause

Distinct from every LP64 variant above (§11, §12, §19, §21), which are
all about a *value* crossing some boundary — a call, a `printf`/`scanf`
format, a class-dispatch parameter — where the two sides disagree about
width. This one is about *reading and writing a fixed binary layout*:
1990s code that hard-coded `long` for a file format's on-disk fields,
back when that was indistinguishable from hard-coding "4 bytes." Once
`sizeof(long)` stops being 4, the code is no longer describing the file
format it was written against — it silently describes a different,
wider one, with no crossing boundary for a compiler to typecheck at all
(the struct's own C definition and its `fread`/`fwrite` calls are
self-consistent; they just don't agree with the byte stream on disk
that predates them).

Found in the standalone `convertraster` CLI tool
(`atk/raster/convert/convrast.c`, `atk/raster/lib/oldrf.c`), which had
never been run on this port before a 2026-08-08 test pass — so, like
several bugs elsewhere in this document, dormant rather than newly
introduced. Two independent instances in the same file pair:

- **`struct RasterHeader`** (`atk/raster/lib/rastfile.h`) declared
  `Magic`/`width`/`height` as `long`, three fields the format's actual
  on-disk header packs into 4 bytes apiece (14 bytes total, plus a
  2-byte depth field) — `oldrf.c` reads and writes it with a hardcoded
  `fread(&hdr, 14, 1, file)`. On LP64 the struct is 32 bytes, so `Magic`
  gets the first 8 file bytes (only 4 of which are really `Magic`), and
  `height` is never touched by the 14-byte read at all — left as
  whatever was on the stack. That garbage height then drove
  `pixelimage_Resize()`, attempting to allocate a raster of essentially
  random size.
- **`oldRF__ReadRow`'s color-inversion step** hand-optimized a "flip
  every bit in this row" operation into a "complement 4 bytes at a
  time" loop, walking a pointer cast to `unsigned long *` with address
  arithmetic (`(row+W-4) & ~3`, decrementing by pointer-width each
  iteration) sized for a 4-byte word. On LP64 each `*lx = ~*lx` silently
  complements 8 bytes per step: the loop's last iteration reads/writes
  2–4 bytes past the row it's supposed to stay inside (into the next
  row's buffer, or off the end of the allocation on the image's last
  row), and — because the loop's lower bound was computed for 4-byte
  strides — it stops 4 bytes short at the *other* end, leaving the
  row's first bytes never inverted at all.

#### Symptom signature

The struct-layout instance: a read that appears to succeed (no error
return) but is immediately followed by a very large or negative-looking
allocation request — check `pixelimage_Resize`/`malloc` arguments before
assuming a leak or a runaway loop elsewhere. The word-loop instance is
more specific and worth recognizing on sight: decode the format
independently and diff against a known-good copy byte-for-byte (a
rendered-image comparison is not sensitive enough to catch this reliably
— see `revival.md`); real instances corrupt a small, *fixed* number of
bytes at each end of every row (here: byte columns 0–3 and 38–40 of a
41-byte row), never the middle — a signature specific enough to point
straight at word-width pointer arithmetic once seen.

#### Fix

`RasterHeader`'s three fields retyped to `int32_t` (`<stdint.h>`),
matching the byte layout the format's own `htonl`/`SWAL` byte-swap
conversions already assumed. The hand-rolled word loop replaced outright
with a plain byte-at-a-time complement over exactly the bytes read — at
this row length (tens of bytes) there is no performance case for the
word-at-a-time version, and removing it also deleted a second,
independent defect riding along in the same function: a "preserve bits
beyond the real row width" step that read its preservation source
*before* the row was first populated, so on a freshly allocated (never
previously written) buffer it preserved `malloc` leftovers instead of
anything meaningful.

#### Scope

Not swept tree-wide — found in one previously-unexercised standalone
tool, not via a systemic grep. Worth checking for deliberately in any
other binary-format reader/writer that predates this porting effort and
hasn't yet been exercised on this port, particularly ones with their own
fixed-size file headers (candidates: the `image` inset's format
importers — `gif.c`, `tif.c`, `pcx.c`, `sunraster.c` under
`atk/image/` and `atk/basics/common/` — currently under separate
investigation for an unrelated-looking "renders solid black/white"
import bug that has not yet been root-caused; worth checking for this
exact pattern first, given how well it matches "a copy loop that
silently never runs across the real buffer, leaving only the surrounding
memory's contents visible").

#### Relationship to the LP64 bug family (§12)

Adjacent but distinct, in the same sense as §18: §12 and its relatives
are about a *value* losing width or sign as it crosses a call boundary
the compiler could have checked but didn't. This bug class has no call
boundary at all — it's a C struct's byte layout silently drifting away
from an external, unversioned binary format the struct was written to
describe by convention only. No amount of stricter call-site typechecking
would have caught it; only comparing the struct's field sizes against
the format's actual documented byte layout, or noticing a hardcoded
`fread`/`fwrite` length that doesn't match `sizeof()`, would.

#### Verification

Scripted, not visual: an independent Python decoder for the `.raster`
text format (already available as `revival/tools/ez2md`'s embedded-raster
support) used to decode both the original test image and every
round-tripped output, then compared byte-for-byte. Confirmed the
struct-layout fix stopped the runaway allocation; confirmed the word-loop
fix made a full raster → RF → raster round trip bit-identical to the
source (previously: every row differed at the predicted fixed offsets).
Full regression pass after both fixes — identity round-trip,
RF/MacPaint/Xwd/Xbitmap round trips, 4×90° rotation identity, PostScript
scale factor, valid crop — all still pass. Detail: `porting-changelog.md`
2026-08-08 entry.

### 23. `eq`: multi-line equation cleanup can't reduce to a single line

**Status:** open, long-standing. Not an LP64/porting bug — a genuine
functional defect in `eq`'s editing behavior, present in the original
CMU source. wdc's recollection: MIT fixed this more than once over the
years, and it kept regressing upstream.

**Symptom:** deleting the lines of a multi-line equation only ever
reduces each line down to a single "zilch" placeholder character — it's
possible to end up with several lines, each holding nothing but a zilch,
but not to merge/delete down to one line with one zilch the way deleting
within a single line of ordinary text works. Correct behavior: deleting
across a multi-line equation's line breaks should collapse it the same
way deleting text does anywhere else in `ez` — down to one line, not
down to N empty lines.

Not yet root-caused in this revival — no investigation has been done
into which of `eq`'s line-management routines (`atk/eq/eqvcmds.c` is the
likely area, given it already owns the Cut/Copy-to-cutbuffer path fixed
in §21) is responsible. Distinct from a missing-integral-symbol concern
`roadmap.md` previously carried — that turned out to be a
misunderstanding, not a real bug: testing the documented way to insert
an integral into an equation works correctly, and `eq` is otherwise
fully working (see `roadmap.md`'s Applications and insets table).

### 24. `lset`/`lpair` pressed into service as an HTML table-layout engine — width/height computations never exercised at this scale before

**Status:** resolved. Four distinct bugs in the `lset`/`lpair` table
layout itself found and fixed 2026-08-17/18 (commits `c5f535ca9b`,
`b38e9fda90`, `a9a82d66f1`). Fixing them exposed seven further,
pre-existing bugs elsewhere in core ATK (`textview` — f/g/h/i; `image`/
`imagev` — j/k/l), also found and fixed 2026-08-18/19. A further
`htmlatk.c`-level layout gap (m., a real HTML idiom this renderer had
no representation for at all, not a core-ATK bug) found and fixed
2026-08-19 while working the National Grid header's remaining
placement issues, followed the same day by one more core-ATK bug
(n., `image__Zoom` pixel-count truncation), a new `lset`/`lpair`
capability (o., suppressing the divider bar), and a refinement of m.
(p., a fixed-pixel icon column so a floated-pair's icon side doesn't
scale down with the rest of the row). A further icon-clipping bug
found live 2026-08-20 testing p. (q.) turned out to be neither n. nor
j. recurring, but a `textview` embedded-border reservation the
fixed-column mechanism didn't account for — root-caused and fixed the
same day, commit `e0eb4f3946`. One further, unrelated issue found the
same day is **not yet resolved** — see this section's closing note.

`atkams/messages/lib/htmlatk.c`'s HTML-mail renderer builds each
`<table>` as a tree of `lset`/`lsetview` objects (`atk/adew/lset.ch`,
`lsetv.c`) — a persistable dataobject layer over `lpair`
(`atk/supportviews/lpair.c`), the general split-pane view class ATK
uses tree-wide for scrollbar-adjacent splits, window chrome, etc.
Nothing about this combination is new, but rendering an arbitrary real
HTML table this way — potentially deeply nested, with cells that need
literal pixel widths (spacer/decoration `<td>`s) alongside proportional
ones, and rows/sections that can flow to many times a screen's height
with no per-cell scroller — exercises code paths `lset`/`lpair` had
never been asked to handle before. All four bugs below were found
working a single real fixture (a Shelf Awareness "Book Rack" newsletter
and a National Grid utility mailer) from "renders as a few blank bars"
through to "mostly right, pagination close but not exact."

#### a. `width=` silently stripped before `htmlatk.c` ever saw it

`ams/libs/shr/htmlpart.c`'s `attr_allowed()` per-tag attribute allowlist
only let `colspan`/`rowspan`/`border` through for `table`/`td`/`th` —
`width` was never added, so every `width=` attribute in real mail was
discarded during sanitization regardless of what the renderer tried to
do with it. One-line allowlist fix.

#### b. `lset` had no fixed-pixel split type, only percentage

`lpair` already had a `lpair_TOPFIXED`/`BOTTOMFIXED` sizeform (an
absolute pixel `bsize` for one side, remainder to the other) alongside
`lpair_PERCENTAGE`, but `lsetview`'s `initkids()` only ever dispatched
to the percentage-based `HSplit`/`VSplit`. Added
`lsetview_MakeHorzFixed`/`MakeVertFixed` (`adew/lsetv.ch`, values 11/12,
chosen to not collide with `lsetv.c`'s file-local `lsetview_NeedLink`
=10), dispatching to `lpair_HTFixed`/`VTFixed`, with `ls->pct`
reinterpreted as a pixel `bsize` instead of a 0–100 percentage for these
two types only. `htmlatk.c`'s `BuildLsetChain` gives this treatment
**only** to small, visually-empty decoration/spacer cells (confirmed
via `CellFixedPixelWidth`) — giving it to a large content-bearing cell
(e.g. a `<td>` wrapping a `width=640` nested table) was tried, live-
tested, and reverted the same day: `lpair_TOPFIXED`'s
`min(totalsize,bsize)` cap has no graceful degradation, so a fixed
request larger than the real window can zero out the complement side
entirely — and `lpair`'s `DoFullUpdate` skips `view_FullUpdate` on a
zero-width rectangle, silently dropping everything nested inside it.

#### c. `lpair__DesiredSize`'s 2048px "pathological content" clamp

```c
if (d1 > 2048) {
    if (d0 > 2048) { *desiredheight = STARTHEIGHT; return(view_Fixed); }
    *desiredheight = d0;      /* picks the SMALL side's height for the WHOLE split */
    return(view_Fixed);
}
```
Original intent: content taller than 2048px was assumed to be a
runaway/pathological value (an oversized image, say) that shouldn't
dictate a pane's height — reasonable when such content is always
wrapped in its own scroller before reaching `lpair`, which is true of
every pre-existing caller. `htmlatk.c` flows a whole message body
through nested `lpair` splits with no per-cell scroller, so ordinary
mail routinely exceeds 2048px. When that content was paired (via
`lpair_TOPFIXED`) against a small fixed-pixel decoration, this branch
reported the *decoration's* ~25px height for the entire row, collapsing
real content to a sliver regardless of window size (Book Rack: a few
thin decoration bars, nothing else, no matter how the window was
resized). Renamed to `MAXSANEHEIGHT` and raised to 1,000,000 — the
original safety-net shape (and the `STARTHEIGHT` double-overflow
fallback) is unchanged, just practically unreachable for real content.

#### d. `lpair__DesiredSize`'s HORIZONTAL (stacked) branch never summed

Distinct bug in the *other* branch — `typex==lpair_HORIZONTAL`, used
for top/bottom stacking (`lsetview_MakeVert`/`MakeVertFixed`, a table's
rows):
```c
view_DesiredSize(self->obj[0], width, self->objcvt[0], view_HeightSet, &d0, desiredheight);
view_DesiredSize(self->obj[1], width, self->objcvt[1], view_HeightSet, &d1, desiredheight);
...
*desiredheight = (height > MAXSANEHEIGHT) ? STARTHEIGHT : height;   /* echoes the OFFERED height, ignores d0/d1 entirely */
*desiredwidth = max(d0, d1);
```
The sibling `VERTICAL` (side-by-side) branch *does* correctly compute
`max(d0,d1)` from real per-child `DesiredSize` results for its free
dimension (height, there) — this branch's free dimension is width
(correctly `max(d0,d1)`), but its height was never computed at all,
just parroted back from whatever was offered. Found live-testing the
c. fix: a too-tall stacked row-chain reported its *offered* probe
height (`drawtxtv.c` offers 16384 as an effectively-unconstrained
ceiling when asking an embedded view "how tall do you really want to
be") instead of summing its rows' real heights, so `textview`'s
pixel-based page-forward walk (`textv.c:2045` `textview__MoveForward`,
`textview_MoveByPixels`) thought a whole screen's worth fit and jumped
straight past unread content — `<space>` paging through a long HTML
message would reach the last *visible* line and then skip straight to
the next message, never showing the rest. (Confirmed this is not a
general "can't page through one big embedded view" limitation: a
raster inset taller than a window pages through it fine, screen by
screen, because raster's own `DesiredSize` doesn't go through `lpair`
and correctly reports its true height every time — the bug was
specific to `lpair`'s HORIZONTAL branch.) Fixed by only taking the
split/echo path when `pass==view_HeightSet` (a real imposed budget);
any other pass now queries each child with the *shared, unsplit* width
and sums their real, independently-queried heights.

#### e. `htmlatk.c`'s row/cell splice-through could flatten stacked sections into one side-by-side chain

Found live-testing d.: after b–d, National Grid pages to within a few
pixels of correct, but Book Rack still reports its first screenful as
the *entire* document. Dumping Book Rack's actual built tree
(`htmlatktest.test dump`) shows why — split-type census across the
whole message:
```
165 type=0   (leaves)
 62 type=1   (MakeHorz  -- side-by-side, VERTICAL orientation)
 25 type=11  (MakeHorzFixed -- side-by-side, fixed-pixel)
  0 type=2, type=12   (MakeVert/MakeVertFixed -- stacked, HORIZONTAL orientation)
```
87 splits total (matching the original "87-level nested" diagnosis
exactly) and **zero** are the stacked/`MakeVert` type — the entire
message, every section that should sit below the previous one, is one
long chain of *side-by-side* splits. `lpair__DesiredSize`'s VERTICAL
branch correctly takes `max(d0,d1)` for that orientation (right for
genuinely-adjacent cells) — so the reported height of the whole 87-deep
chain is just the height of its single tallest link, not the sum of the
whole document. Root cause believed to be `htmlatk.c`'s
`NodeIsSoleNestedTable` splice-through: its *cell-level* rule (a `<td>`
whose only content is a single-row nested table gets that row's own
lset chain attached directly as the cell's own leaf, added originally
to skip a redundant wrapper around one cell's content) also fires when
the "wrapped" content is actually an unrelated subsequent section of
the newsletter — a very common email-authoring pattern (each section
individually wrapped in its own single-row single-column spacing
table) — folding what should be a fresh stacked (`MakeVert`)
relationship into "more of the same side-by-side chain" instead. Fixed
by only allowing the splice-through when the nested table is a genuine
single-cell leaf (`left==NULL`, no real side-by-side content of its own)
— confirmed via `htmlatktest.test dump`: nested `lset` view count went
from 78 to 102 (+24), matching the leaf-count increase (165→189)
exactly, while the flat split-type census stayed identical (the same
subtrees moved one level deeper rather than being flat-spliced).

#### f. `textview_Visible` treated text-position containment as proof of pixel-level visibility

Found live-testing e.'s fix: Book Rack's forward scrolling now worked,
but "go to end" (`Escape >`) silently did nothing. `textview__Visible`
(`atk/text/textv.c`) answers "is this position on screen," but for a
position at the very end of the last laid-out line it only checked
whether the position fell within that line's text range — never
whether the line's own bottom edge fit inside the viewport. For
ordinary short lines the two questions have the same answer; for a
document whose entire content is one embedded view many screens tall,
only partway scrolled into view, they diverge, and `textview_FrameDot`'s
consumer skipped re-scrolling because `Visible()` said "already
visible." Fixed by also requiring the last line's bottom edge fit
within the viewport, mirroring a check the scrollbar code (`getinfo`,
same file) already made.

#### g. `BackSpace`'s pixel-budget search never landed inside the very first, tallest line

Found live-testing f.'s fix: `Visible()` now correctly detected "not on
screen" and asked `BackSpace` (`atk/text/textv.c`) to scroll the target
into view, but the target landed at absolute position 0 (the very top)
instead of near the bottom where it belonged. `BackSpace`'s pixel-budget
search only credits the height of lines strictly *before* the one
containing the target — correct when the target sits at its own line's
top edge (the normal case, since the search is normally driven off the
current top-of-screen mark), but not when the target sits at the line's
*end*, as happens for the very end of a document that is one line. With
no preceding lines to credit, the search always concluded there wasn't
enough room and fell through to "can't go any farther," clamping to
position 0. Fixed by also checking whether the line's own height covers
the remaining budget, landing partway into it when so.

#### h. `MoveForward`'s forward-paging landed the scroll-top on a position with no line of its own

Found live-testing f./g.'s fixes: with "go to end" now working, paging
forward one more screen (`^v`, `textview_NextScreenCmd`) from that
already-at-the-end state produced a totally blank window instead of a
no-op. `textview__MoveForward`'s `MoveByPixels` branch (`atk/text/textv.c`),
when a forward page consumed the *entire* remaining height of the
document's last line, unconditionally advanced the scroll-top position to
`text_GetLength()` -- one past all real content. `DoUpdate`/
`GenerateLineItems` has no fallback for that: it formats one empty line
there and whites out the rest of the view. This isn't specific to the
pathological one-giant-embedded-view case -- it's latent for any document
where forward-paging exactly exhausts the last line -- it just took a
document like this project's HTML tables (a line many screens tall) to
actually trigger it in ordinary use. Likely the real mechanism behind the
project's original motivating complaint, that paging through a message
with `<space>` repeatedly skips its last chunk and jumps to the next
message. Fixed by refusing to advance past the last line; instead reveal
further into it (same math the existing partial-landing branch uses),
capped so its bottom never scrolls past the viewport's bottom edge --
once reached, further forward-paging is a genuine no-op rather than a
blank screen.

#### i. Scrollbar bottom-endzone click blanked the screen on a held click, via a `BackSpace` fast-path applied outside its own stated precondition

Found chasing the scrollbar endzone-click bug logged in `revival.md`'s
Open issues (itself found live-testing f./g./h.'s fixes). A single
discrete click already worked; a click held past one auto-repeat tick
(~100ms, `scroll.c`'s default `ButtonRepeatTime`) reproduced a blank
white window. Confirmed live with targeted `write(2)` tracing in both
`scroll.c` and `textv.c`: the click's release event is correctly
filtered out by `endzone()`'s own action check, but the auto-repeat
timer fires a *second* `view_LeftDown`-flavored call before release,
re-running the "jump to end" `setframe()` request identically.
`setframe()`'s second internal `MoveBack` call (the `MoveByPixels`
backward offset) hit `BackSpace`'s "stay within the current line"
fast-path shortcut (`atk/text/textv.c`) — a shortcut whose own comment
already documented its precondition as operating on the *current top
line*, but which was applied unconditionally to whatever `pos` was
passed in. The bottom-endzone jump's `pos` is never the current top; it's
a freshly-computed target line near the end of the document. After the
first (correct) landing already set `pixelsReadyToBeOffTop` to
approximately the same distance being requested again, the second call's
shortcut fired, comparing the request against that unrelated leftover
value, and returned `pos` (`text_GetLength()`) completely unmoved —
landing the scroll-top exactly at end-of-document, the same
no-line-of-its-own position h. above already established renders as a
blank screen. Fixed by only taking the shortcut when
`pos == textview_GetTopPosition(self)`, matching the precondition the
shortcut's own comment already claimed.

#### j. Every embedded `image` draws a 5px bevelled "window pane" frame across its whole allocated rectangle, not just around the picture

Found live-testing image fetching/PNG support against the National Grid
fixture: a thin decorative divider image ("Blue Brand Line", 350×16px)
sitting side-by-side with a much taller 5-icon social row rendered as a
large blank bevelled box, not a thin line. `imagev`'s `DrawBorder()`
(`atk/image/imagev.c`) always bezels the *entire* view rectangle
(`self->bordersize`, default 5px each side, via `sbuttonv_SafeDrawButton`)
— correct, even desirable, styling for a normal user-embedded `ez`
image (a "pane" affordance for an editable object), but wrong for a
flat inline decorative image whose `lset`/`lpair` cell has been
stretched taller than its own content to match a side-by-side sibling
(`lpair`'s side-by-side split correctly gives both sides the same
height — see k. below for why the mismatch existed at all). Fixed by
adding a `noBorder` flag to `image` (`basics/common/image.ch`), checked
in `imagev__SetDataObject` to zero `bordersize` for that instance only;
`htmlatk.c`'s `RenderImageInline` sets it on every image it inserts.
Normal image usage elsewhere (`ez`'s own Insert Image, `messages`'
plain MIME-attachment image display) is unaffected — the flag defaults
off.

#### k. HTML `<img width=/height=>` was never applied — every image displayed at its raw decoded pixel size

Found live-testing j.'s fix: the "blank box" was gone, but the row was
still far taller than it should be. Root cause: `RenderImageInline`
(`htmlatk.c`) never read an `<img>`'s `width=`/`height=` attributes at
all (only `LooksLikeBeacon()` reads them, for tracking-pixel detection,
not display sizing) — every image always displayed at whatever pixel
size it happened to decode to. Real marketing HTML relies on the
browser for this: the National Grid fixture's 5 social icons all
declare `width="30"`, but the actual source PNGs are inconsistently
sized (Facebook/YouTube/Instagram are 100×100px native; Twitter/LinkedIn
happen to be small pre-sized files) — nothing wrong with the markup,
just normal real-world asset reuse. Undersized-relative-to-declared
never showed (nothing in the corpus does that), but the oversized case
made 3 of 5 icons render at ~3× the intended size, and because `lpair`'s
side-by-side split correctly reports `max(d0, d1)` for its free
dimension (not a bug — see the design doc's Table strategy section),
the oversized icons dragged the *entire row's* height up to match, not
just their own cells. Fixed: a new `ScaleImageToDeclaredSize()` reads
`width=`/`height=` (aspect-preserving if only one is given) and calls
`image_Zoom()` (percentage-based, `image.ch`) to produce a correctly-
sized replacement before insertion, whenever the decoded size differs
from the declared one.

#### l. `image__Zoom`'s `ITRUE` (truecolor) case read an uninitialized pointer

Found live-testing k.'s fix: scaled icons rendered as solid black boxes
instead of shrunk pictures. `image__Zoom` (`basics/common/image.c`) is
one function handling four source image types via a `switch`; the
`IGREYSCALE`/`IRGB` cases build a fresh `newimage` and *fall through*
into the `ITRUE` case, which then checks `if (!RGBP(newimage))` before
deciding whether to reuse it or allocate its own. But a genuine `ITRUE`
source (a decoded truecolor PNG — the common case for real inline
HTML-mail images, and apparently the first real caller of `Zoom()` on a
plain truecolor image in this revival) enters the `ITRUE` case
*directly*, with `newimage` never assigned — reading and dereferencing
an uninitialized stack pointer, undefined behavior that happened to
manifest as corrupted (black) output rather than an outright crash.
Latent since whenever `Zoom()` was written; never triggered before
because nothing in this codebase previously zoomed a plain truecolor
image. Fixed by initializing `newimage = NULL` at declaration and
checking `!newimage || !RGBP(newimage)`, matching the code's own
evident intent.

#### m. `align="left"`/`align="right"` floated adjacent `<table>`s had no side-by-side representation at all

Not a core-ATK bug like f-l above — a real gap in `htmlatk.c`'s own
table-layout coverage. Real marketing HTML routinely places two things
side by side not with one `<tr>` holding two `<td>`s (the case
`BuildLsetGrid`/`BuildLsetChain` already handled), but with two
*sibling* `<table>`s, one `align="left"` and the next `align="right"`,
relying on a browser's float layout to place them beside each other.
Confirmed live in National Grid's gas-meter row: a `width="10%"
align="left"` table holding just the icon, immediately followed by a
`width="85%" align="right"` table holding the paragraph
(`revival/tests/national-grid.html`, ~line 279-297).

Both of those tables are individually a single row/single cell — i.e.
exactly what `TableIsTrivialWrapper` (added for #24 item e.'s spacer-
table problem) treats as pure structural boilerplate and unwraps into
plain flowing content. That unwrapping is what broke this idiom:
once unwrapped, the icon and paragraph both ended up as ordinary
inline content of the *same* surrounding `textview` (confirmed via
`htmlatktest.test dump`: the row came out as one plain leaf, the
image's view character followed directly by the paragraph's text
runs, no `lset` split anywhere) — and `textview` has no CSS-style
float layout of its own, so every line after the image's own line
started back at the column's full left edge, below the image, not
beside it. Visually: the paragraph text wrapped above and below the
icon instead of to its right.

Fixed with two changes in `htmlatk.c`: an `align="left"`/`"right"`
table is now exempted from the trivial-wrapper unwrap (its align *is*
real visual intent, not boilerplate); and when such a table is popped
off the main walk stack, `TryPairFloatedTables` peeks at its next real
sibling (skipping whitespace-only text nodes) and, if that sibling is
also a `<table>`, builds a genuine 2-column `lset` split from the pair
— left/right chosen from the floated table's own `align` (not the
partner's, so a partner with no `align` of its own still lands
correctly, matching ordinary CSS float semantics), weighted by each
side's declared `width=` percentage (falling back to 50/50 if neither
declares one). A floated table with no pairable sibling still renders
via the ordinary single-column path rather than vanishing. Also
required allowing `align` through `htmlpart.c`'s attribute allowlist
for `table`/`td`/`th` (previously stripped during sanitization like
`width` was before #24 item a.'s fix). Verified via
`htmlatktest.test dump`: the row now comes out as a real `type=1
pct=89` `lset` split (icon leaf left, paragraph leaf right), matching
the 10%/85% declared widths.

#### n. `image__Zoom`'s pixel-count rounding truncated instead of rounding, clipping the scaled image's right/bottom edge

A core-ATK bug, not `htmlatk.c`-specific — `imagev.c`'s own interactive
zoom-in/out feature was equally affected, just never exercised at
percentages that made the truncation visible. `image__Zoom`
(`atk/basics/common/image.c`) computes its output pixel dimensions via
`buildZoomIndex`, which did `*rwidth = fzoom * width` — a plain
`float`-to-`unsigned int` assignment, truncating toward zero rather
than rounding. Combined with `htmlatk.c`'s item k. fix
(`ScaleImageToDeclaredSize`) computing the zoom percentage itself via
truncating integer division, real icon dimensions routinely hit a
double truncation: e.g. a 31px-native icon scaled to a declared
`width="30"` computes `zoom=(30*100)/31=96` (truncated), and
`floor(0.96*31)=floor(29.76)=29` — one column short of the requested
30. Found live 2026-08-20 as "the rightmost column of the icon looks
clipped," reproduced on National Grid's gas-meter icon and several of
its social-row icons. Fixed by rounding (`+ 0.5` before the cast)
instead of truncating, plus a defensive clamp on the per-pixel source
index (a rounded-up output size means the highest index this loop
computes is correspondingly larger, so it's clamped into range rather
than trusted not to ever read one pixel past the source buffer).
Verified with a throwaway standalone harness (`image_Zoom` called
directly, off-screen, on synthetic images at the exact ratios above)
rather than through `htmlatktest.test`, since that tool's `dump` mode
only ever inspects `lset`/`text` structure, never decoded pixel data.

#### o. Every `lset`/`lpair` split drew a visible divider bar, with no way to suppress it

Noted as an accepted, unfixed limitation when item m. shipped (same
day): a real browser never draws a resize bar between table cells, but
`lpair`'s `DoFullUpdate` (`atk/supportviews/lpair.c`) draws one
unconditionally on every split, and `lsetview`'s `initkids()`
hardcoded every split it built as draggable with no data-level way to
turn the bar off. Surfaced live as visually noisy vertical lines
between National Grid's 5 social-row icons.

The first attempt at a fix added a real `boolean barvisible` field to
`lpair`'s data section. That changed `sizeof(struct lpair)`, and
because `frame.ch` (window chrome, used by nearly every ATK view)
embeds `struct lpair` inline via inheritance, every already-compiled
`.o` referencing `frame`/`lpair` silently disagreed on layout with the
freshly-recompiled ones — caught live as `"Incompatible version of
lpair requested!"` immediately followed by a segfault in `messages`.
(classpp auto-computes each class's version number from `sizeof(struct
%s)`, folded into a compile-time constant per translation unit — see
`overhead/class/pp/class.c`'s `%s_VERSION` codegen — so two `.o`s
built against different struct layouts for the same class disagree the
instant one of them allocates and the other's code touches it.)

Fixed instead by packing the bit onto the *existing* `movable`
field/parameter (a new `lpair_NOBAR` bit, `lpair.ch`) rather than
growing the struct at all — zero layout change, so no wider rebuild
than `lpair.c`/`lsetv.c` themselves. `lset.ch` gained a genuinely new
persisted field, `nobar` (safe — nothing subclasses `lset`, unlike
`lpair`), read/written under a bumped `\V 2` on-disk version with the
old field count still accepted for `\V 1` data. `BuildLsetChain`
(`htmlatk.c`) now sets `nobar` on every split it creates, since a real
browser never shows one here regardless of table shape.

#### p. Floated-pair icon column used a percentage split, so it visibly shrank (and its bitmap clipped) in a narrow window

A refinement of m., found live 2026-08-19 right after m. shipped:
`TryPairFloatedTables` weighted both sides of a floated-table pair by
their declared `width=` percentages, including the icon side. A
percentage split shrinks both sides together as the container narrows
— fine for the text side, wrong for the icon side, since a bitmap
doesn't scale down to fit a smaller view the way flowing text does.
Fixed by giving the icon side a **fixed** pixel width instead,
reusing `BuildLsetChain`'s existing `fixedpx` mechanism (previously
only used for empty spacer `<td>`s, item b.): a new
`FloatTableSoleImageWidth` helper recognizes when a floated table's
only real content is a single `<img>` (ignoring whitespace and
pass-through wrapper tags like `<a>`/`<center>`), and uses that
image's own declared pixel width as the fixed `bsize`, leaving the
text side to take whatever's left. Only works when the image-only side
lands in `BuildLsetChain`'s left/top slot (its `fixedpx` convention
folds a fixed cell in there specifically, never the last slot) — not
seen any other way in the corpus so far, but a right-hand fixed icon
would just fall through to the pre-existing proportional split rather
than being silently wrong.

#### q. Fixed-pixel icon columns (n./p.) still lost pixels: `textview` reserves its own embedded border, uncounted by the column's exact-fit sizing

Found live 2026-08-20, testing p.: the gas-meter icon (and, separately,
National Grid's 5-icon social row once it got the same fixed-column
treatment as p. — see below) still lost a few pixels off the right edge
even though n. and p. were both already shipped and, individually,
correct. Two false leads chased first and ruled out with direct
evidence before finding the real cause: (1) `image_Zoom`'s scaling math
— re-verified live via a temporary trace and found exact in every case
checked, and moot anyway for the gas-meter icon specifically, whose
native decoded size already matched its declared width, so no scaling
ever ran; (2) `imagev`'s bevel border (item j.) reappearing — a
`bordersize` reading that looked like `SetNoBorder` had stopped working
turned out to be an artifact of testing through a new `writedsr` debug
tool that round-trips a fixture through a real ATK datastream file;
`image__Read`/`image__Write` never persist the transient `noBorder`
flag, so of course it came back `FALSE` after a disk round-trip — the
live `messages` path never serializes the image at all, so this never
applied there.

The real cause: the fixed lset column sizes the `lpair` box to exactly
the icon's own pixel width, but the `text` object rendered inside it
goes through `textview`, and `textv.c`'s line-layout unconditionally
reserves a small border (`EBX`, 2px) on each side when computing its
own drawable width — even for a bare cell wrapper never meant to be
edited. Live-traced (temporary instrumentation added to and then
removed from `drawtxtv.c`/`textv.c`/`imagev.c`, core ATK text/image
view code) against two independent real icons — the 81px gas-meter
icon and the 30px social-row icons — and both showed the exact same
**-6px** shortfall regardless of size, ruling out anything proportional.
Fixed entirely in `htmlatk.c` (not core `textview`, which is
widely-shared): a new `ICONCELL_WIDTH_PAD` constant pads the fixed
column's requested width by 6px at both call sites. While chasing this,
also extended n./p.'s fixed-column mechanism from just the floated-pair
icon to any `<td>` whose sole content is one appropriately-sized `<img>`
(so the National Grid social-icon row, previously relying on an
equal-weight percentage split that could round a given icon a pixel or
two short, gets the same treatment) — capped to icon-scale widths
(`ICONCELL_MAXFIXEDPX=120`) after an uncapped first attempt
immediately regressed live: a 350px decorative divider image sitting in
the row's own `<tr>` got a fixed column too, starving its sibling and
squeezing the whole icon row down with it.

Live-confirmed fixed by wdc ("YES! gas meter icon perfect. Facebook and
the other icons back to their proper size and placement.").

**Not yet resolved:** a separate bug, also found live 2026-08-20 while
testing p. before q. was root-caused — widening the message window lets
ordinary paragraph text reflow to fill the new width, but content
inside an `lset`-rendered HTML table (including this same floated-pair
row) stays visibly narrow, as if still laid out for the window's
earlier, narrower size. Not investigated further yet — logged here to
pick up next session. Also not yet addressed: `<td style=
"background-color:...">` isn't honored at all (noticed via a
Thunderbird comparison of the gas-meter icon's cell background).

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

### `overhead/class/Imakefile` SUBDIRS order breaks a truly clean build

`SUBDIRS = machdep lib cmd pp testing doc` — `machdep` (whose
`machdep/darwin/classproc.c` does `#include <class.h>`, no relative
include path of its own) is built *before* `lib`, which is the
directory whose `InstallFile(class.h, ...)` actually populates
`build/include/`. On a `build/` that already exists (the normal case
— nobody has wiped it since the tree was first bootstrapped), this is
invisible: `class.h` is already sitting in `build/include/` from long
ago. On a genuinely empty `build/`, `make World` fails immediately
with `classproc.c:12:10: fatal error: 'class.h' file not found` and
`libclass.a` never gets built, cascading into dozens of unrelated
"file not found" errors elsewhere. Confirmed present since initial
import (`fossil finfo` on the Imakefile shows no changes ever).
Workaround, not yet fixed upstream: seed it manually before `make
World` —
```
mkdir -p build/include
cp src/overhead/class/lib/class.h build/include/class.h
```
one-time per empty `build/`; the real `InstallFile` step overwrites it
correctly once `overhead/class/lib` is reached.

### `gendemo`'s `cui recon` step wedges unkillably on any checkout before 2026-07-11

Same underlying bug family as the vendored-bison note above, different
consumer: `prsdate.c` (mkparser-generated) segfaults inside `cui
recon` on any checkout before the fixed-width-table fix
(`abdc97546c`, 2026-07-11). The `cui` child process lands in
uninterruptible (`UE`) kernel-wait state and **does not respond to
`kill -9`** — confirmed by direct testing, not just inferred from the
bison note. `make World`/`make dependInstall` on `ams/demo` blocks on
it indefinitely. Fix: don't try to kill `cui` itself; `SIGTERM` the
parent `/bin/csh -f gendemo` wrapper instead — the Imakefile's
`install.time::` recipe line has a leading `-` (ignore exit status),
so `make` continues past the now-failed step normally. The orphaned
`cui` process is harmless afterward (detached, uses no CPU) and can be
left running. Already fixed at and after `abdc97546c`; only bites
when deliberately checking out something earlier.

### Fossil checkout timestamps make incremental rebuilds untrustworthy across revisions

Fossil sets a file's mtime to its **commit** timestamp, not to
checkout wall-clock time. Every commit in this repo's history is
necessarily "in the past" relative to whenever a rebuild session is
actually happening — so after `fossil update <any-rev>`, any source
file that changed relative to what's currently compiled gets an mtime
*older* than the already-compiled `.o`/`.do` sitting in the tree from
today. `make`'s timestamp-based dependency check then wrongly
concludes the object is up to date and skips recompiling it — silently
leaving stale, wrong-revision code linked into the binary. This isn't
directional (doesn't matter whether the checkout moves forward or
backward through history) and isn't hypothetical: it produced a
confirmed false reading during the 2026-07-12 Media-menu bisection
(an incremental rebuild at `f46de124ed` showed the bug present; a
subsequent from-scratch rebuild of the identical revision showed it
absent). It's most visible for dynamically-loaded `.do` classes gated
by a build flag (e.g. `zip.do`/`calc.do` from `MK_ZIP`/`MK_CALC`
persisting in `build/dlib/atk/` and loading successfully at a revision
where they're not supposed to exist at all — `make Clean`'s recursive
descent follows the *current* Imakefile's `SUBDIRS`, so it never even
visits a now-degated directory to clean its old outputs), but applies
to any statically-linked file too. **The only trustworthy way to test
a different revision is a full wipe**: move or remove `build/` and
`src/`'s generated `.o`/`.do`/`.eh`/`.ih`/`.a` (`make Clean`) before
every `make World`, every time, regardless of which direction the
checkout moves. A full from-scratch `make World` (tree already
Imake-bootstrapped, just `build/` and generated files cleared) took
**3m36s real** (1m20s user, 58s sys) on this machine as of
2026-07-12 — cheap enough that there's no excuse to skip it when
bisecting. Run `revival/tools/prime-class-header` (codifies the
`class.h` seed above) right after clearing `build/`, before `make
World`.

### `MK_CONSOLE` being off silently breaks `con10`/`con12` icon fonts used outside `console`

`atk/Imakefile:132` gates the *entire* `atk/console` subtree (including
`console/fonts`, not just `console/lib`/`console/cmd`) behind
`#ifdef MK_CONSOLE`, which is undefined in this revival (`console` is
an intentionally inert subsystem — see the directory census in
`claude-history/roadmap-old.md`). `console/fonts/Imakefile` already correctly declares
`DeclareFont(con10)`/`DeclareFont(con12)` — nothing wrong with that
recipe — but since the whole directory is never visited, `con10.fdb`/
`con12.fdb` (custom `.fdb`-format icon fonts, a different format from
the `.pcf` bitmap fonts everything else uses) never get compiled or
installed to `build/X11fonts/`, and the fonts are silently absent.

**Symptom (found 2026-07-12):** the `fad` (animation) inset in
`ams/demo/d10` declares `con10` as one of its two icon fonts (`$N
con10` — see `atk/fad/fad.c:341`, `fad__iconnum`) to draw a console/
terminal-shaped icon for its "Client Program" node. With `con10`
unresolvable, `fontdesc_Create` falls back to some default font, and
the intended icon glyph code renders as a literal fallback-font
character — looked exactly like a `fad`-view drawing bug (a wrong
glyph appeared as "M") and was initially suspected as one during a
bisection, before being traced to this font gap. `fad` itself has no
drawing defect; once `con10` resolves, the animation renders and plays
correctly — confirmed by direct testing, closing out that bisection.

**Codified fix: `revival/tools/install-console-fonts`.** Builds
*only* `atk/console/fonts` (bypassing the `MK_CONSOLE` gate entirely —
never touches `console/lib`/`cmd`/`stats`/`consoles`), installs both
`con10.pcf` and `con12.pcf` to `build/X11fonts/`, regenerates
`fonts.dir` via `mkfontdir` (not a hand-edited line count — `fonts.dir`
itself isn't Imake-generated or tracked in source anywhere; it's
always been a manually-maintained artifact, `mkfontdir` is the correct
tool for it), and runs `xset fp rehash`. `console/fonts/Makefile` is
checked in as an empty stub (the parent Imakefile never regenerates
it, same root cause as the gate itself), so the script regenerates it
in place with a direct `imake` invocation the first time it's run —
`MK_CONSOLE` is irrelevant to that step since it only affects whether
the *parent* recurses here, not this leaf directory's own contents.
Safe to re-run any time after a clean rebuild wipes `build/X11fonts/`.

**Still not truly permanent:** this only fixes `con10`/`con12`
specifically; any *other* file outside `console` that happens to
reference a console-only resource would still be silently broken, and
nobody has checked for that. The real upstream fix, not yet decided
between: (a) define `MK_CONSOLE` to bring back the whole console/
terminal-emulator subsystem, much bigger scope than needed just for
two fonts; or (b) carve `console/fonts` out of the `MK_CONSOLE` gate in
`atk/Imakefile` so it always builds regardless of whether `console`
itself does — the smaller, more targeted fix.

### clock inset's `InitializeObject` depends on the `icon12` font resolving, via its cursor

Clock was manually bisected across 9 checkpoints from `6338ade7de`
(2026-07-07, before the M1 rollout) through HEAD, each a full
from-scratch rebuild, and found blank at every single one — logged as
a confirmed pre-existing, not-yet-root-caused bug (see
`claude-history/roadmap-old.md` → Insets to Repair → clock). Reopened the same day: inserting a fresh
clock via ez's `<ESC><TAB>clock` ("insert inset by name") rendered
correctly in the same session and build where a *parsed* clock (from
serialized datastream text in the test file) had been failing.

Found the mechanism: `clockview__InitializeObject`
(`contrib/time/clockv.c:288-289`) does
```c
if (!(self->cursor = cursor_Create(self))) return(FALSE);
cursor_SetStandard(self->cursor, Cursor_Gunsight);
```
and `xcursor__SetStandard` (`atk/basics/x/xcursor.c:97-107`) hardcodes
```c
#define DEFAULTFONTNAME "icon"
#define DEFAULTFONTSIZE 12
...
self->header.cursor.fillFont = fontdesc_Create(DEFAULTFONTNAME,0,DEFAULTFONTSIZE);
```
— i.e. clock's cursor shape (the "gunsight" cursor shown while
interacting with it) depends on the `icon12` font resolving. If that
lookup fails, `cursor_Create` returns null and `InitializeObject`
returns `FALSE` immediately — the clockview never finishes
initializing, so nothing in `Redraw` (hands, face, labels) ever runs.
This matches the observed symptom precisely: a completely blank
clock, not just missing labels.

**Not fully closed.** `icon12` is not `MK_CONSOLE`-gated like
`con10`/`con12` (different root cause than the note above) and was
already confirmed present in `fonts.dir` and resolvable via `xlsfonts`
*before* any of today's console-font work — so it isn't obviously
"the font was missing" in the same way. Best current explanation: a
transient X-server font-cache/session-state issue specific to
whatever long-running `ez` process the old, broken clock instance
lived in, cleared up as a side effect of today's repeated
`mkfontdir`/`xset fp rehash` work for `con10`/`con12` — not a
permanent gap, not a code defect, but not confirmed either. Not yet
isolated: whether the *old* serialized-datastream clock (as opposed to
one freshly created by name) also now renders correctly in a fresh
`ez` launch, which would settle whether this is pure session/font-cache
state versus something specific to how a clock gets instantiated from
parsed data versus by-name insertion.

### Intermittent ~500ms menu-bar posting delay — confirmed XQuartz-side, not AUIS (found and inventoried 2026-08-09, unresolved)

**Symptom:** posting a menu-bar card (clicking a title like `ez` to open
its pulldown) sometimes draws immediately and sometimes takes roughly
half a second — reproducibly, but not on every post, and not tied to a
fixed pattern of which card or how many prior posts happened in the
session. Visually: the pulldown window appears instantly (blank), then
its text fills in after the delay. Reports of multi-second delays are
consecutive single-card stalls perceived together (e.g. two cards
posted back-to-back, each individually costing ~510ms), not one longer
stall — see the quantization note below.

**Methodology.** Temporary `mdbg()` tracing (writes to
`/tmp/menudbg_direct.log`, bypassing `stderr` — see the next paragraph)
was added to `xim__PostMenus`/`updateMenus` (`atk/basics/x/xim.c`),
`BringUpMenu`/`DrawMenuItems`/`MakeGCs` (`atk/basics/x/menubar.c`), and
`cmenu_Activate` (`overhead/cmenu/cmactiv.c`), bracketing every step
with `gettimeofday()`. This instrumentation is left in the tree
(committed) for anyone picking this back up, but is off by default:
`xim.c`/`menubar.c` gate it behind the `MenuDebugTrace` profile switch
(`environ_GetProfileSwitch`, the same idiom as `UseBackingStore` etc.),
while `cmactiv.c` — a lower-level library with no dependency on
`atk/basics`'s `environ` class, since its own standalone `testmenu`
build target doesn't link it — gates it behind a plain `MENUDBGTRACE`
environment variable instead. See `menubar.help`'s Preferences section
for exactly how to turn each on.

**Dead end found along the way, fixed while debugging this:**
`xim_EstablishConsole` (`xim.c:757`, called unconditionally from
`SetupDisplay` at the top of `xim__CreateWindow`) attempts a UDP
"console log" connection — CMU Andrew's original 1988 campus
error-console protocol, `environ_GetProfileSwitch("ErrorsToConsole",
TRUE)` on by default. Since UDP `connect()` succeeds locally even with
no listener, this reliably fires, and does `fclose(stderr); dup2(fd,
2);` followed by an `fdopen(2, "w")` whose result is never saved
anywhere — silently breaking every later `fprintf(stderr, ...)` in the
process (ours and the application's own) for the rest of its life. Not
fixed (out of scope for this investigation), but worth flagging as a
real, independent diagnosability bug: any error message logged after
the first window opens goes nowhere. Our tracing was rerouted around it
with a `fopen`-per-call helper instead of relying on `stderr`.

**Ruled out, in order tried:**
1. **`MenubarCardDelay` preference** (the intentional hover-delay knob,
   `menubar.c:1670`) — confirmed resolving to `0` every time via direct
   trace; not a stray config value.
2. **The whole-menu-cache-flush path** in `xim__PostMenus`
   (`imself->init->version != imself->initversion`) — traced and ruled
   out; `PostMenus`/`updateMenus` only run once per session in the
   reproduction, not per-click, so nothing there can explain a per-post
   delay.
3. **XQuartz pasteboard/clipboard sync** — disabled via XQuartz
   Preferences → Pasteboard; delay persisted unchanged.
4. **Rootless-XQuartz Xft recomposite lag** — the same mechanism fixed
   for the calc inset above (2026-07-12): applied the identical
   self-`XCopyArea` nudge (through the core-X path) to
   `DrawMenuItems`. No effect. (Menu text is drawn via plain core-X
   `XDrawString` already, not Xft, so this was always a long shot — the
   calc fix was specifically for the Xft/Render path.)
5. **Window focus** — reproduced even after clicking into the document
   body first to force focus before posting a card.
6. **Decoupling the draw from `DoMenuLoop`'s `XGrabPointer`** — added an
   explicit `XFlush` immediately after `DrawMenuItems`, before returning
   to the caller that issues the (reply-requiring, and therefore
   flush-triggering) `XGrabPointer`. No visible improvement — the
   server appears to gate compositing on its own schedule, not on
   request bundling with the grab.
7. **Standalone reproduction attempts** — a from-scratch, zero-AUIS-code
   C program (`XOpenDisplay`/`XCreateSimpleWindow`/`XDrawString`/
   `XSync` in a loop) run against the *same live XQuartz server*, in
   several increasingly faithful variants, never reproduced the stall:
   plain draw-and-sync (40 iterations, all 0-10ms); with real
   `XWarpPointer`-driven pointer motion across the window between draws
   (all 0-10ms); with an override-redirect + `CWSaveUnder` popup window
   mapped/unmapped each iteration, matching `BringUpMenu`'s exact
   window-management sequence (all 0-8ms); and with the menu system's
   actual font loaded — `andy12b`/`andy12bi` (menubar.c's item/keys font
   defaults) turn out to be aliases in `build/X11fonts/fonts.alias` for
   a **scalable, wildcard-pixel-size XLFD pattern**
   (`-adobe-times-bold-r-normal--*-140-*-*-p-*-iso8859-1`), which
   requires the X server's legacy scalable-core-font renderer to
   synthesize the glyphs rather than blit a native bitmap strike — a
   real, credible lead, but drawing that exact font with varying text
   across 40 iterations produced only one 81ms blip (first-use metrics
   computation) and nothing resembling the ~500ms pattern.

**What is confirmed, not just suspected:** live sampling of the actual
`eza` process (macOS `sample <pid> <duration>`) during a real
reproduction caught it red-handed — the call stack during the stall is
```
BringUpMenu → XSync → _XReply → xcb_wait_for_reply64 →
  wait_for_reply → _xcb_conn_wait → poll
```
i.e. genuinely blocked in the kernel `poll()` syscall waiting for the
X server's reply packet — not spinning, not stuck in AUIS/Xlib
client-side logic. Every client-side step measured by the `mdbg` tracing
(`SetTitleSelection`, `ComputeMenuPositioning`, `XMoveResizeWindow`,
issuing the `XDrawString` calls) was 0ms on every single measurement,
with 100% of the delay isolated to this one round-trip wait.

**Conclusion:** this is server-side latency in XQuartz's response to
`eza`'s specific connection, not a defect in AUIS's menu code — six
independent theories for *why* were tested and eliminated, and the
delay was reproducible only through the real application, never
through a minimal client hitting the same live server the same way.
This part is solid: the live-sampling evidence above is a clean
localization regardless of which theory for *why* turns out to be
right.

**Quantization — a fact the first pass of this write-up didn't
confront (flagged by an external second-opinion review of this
section, worth recording as a real correction, not just a footnote).**
Every stall logged across every session was 509-516ms; every non-stall
was 0-41ms. Nothing in between was ever observed, and nothing exceeded
516ms for a single post. That's the signature of a fixed timer,
timeout, or wakeup cadence — not of work whose cost scales
continuously with client state. It directly undercuts the resource-
footprint hypothesis this section originally led with: growing
resource count predicts a cost that trends upward over a session (and
varies continuously), and the logs instead show runs of consistently
fast posts and runs of consistently ~510ms posts, i.e. the server
toggling between two discrete states, not drifting. The
resource-footprint theory is downgraded from "leading hypothesis" to
"not well supported by the data" on this basis, though not disproven
outright (it was never directly tested — see below).

**Better-fitting alternative, not yet tested:** the standalone
reproduction attempts (above) all drove 40 tight, back-to-back
iterations; real reproductions came from human-paced clicks with
seconds of idle X traffic in between. If XQuartz (or whatever thread
pairs its X server side with AppKit) is subject to idle-throttling —
App Nap, timer coalescing, or an internal wakeup path on a ~500ms
cadence after a period of inactivity — a tight loop would never
reproduce the stall no matter how faithfully it mimicked window
attributes, exactly what was observed. This fits the quantization and
the runs-of-slow/runs-of-fast pattern better than the resource theory
does, and reframes the whole "can't reproduce standalone" finding: the
missing variable may have been idle time between requests, not
anything about window/resource setup.

**Suggested follow-up tests, none requiring a second X server**
(roughly ordered by information gained per unit of effort):
1. **Sample the other side.** This investigation sampled `eza` but
   never XQuartz itself. `sample`/`spindump` the XQuartz server process
   during a live stall: threads idle in an event wait points at a
   timeout/wakeup bug; threads busy walking server-side structures
   points at the resource-bookkeeping theory. Single most decisive test
   available.
2. **App Nap A/B.** `defaults write org.xquartz.X11
   NSAppSleepDisabled -bool YES`, restart XQuartz, retest; also check
   the App Nap column in Activity Monitor during a stall. Minutes of
   work — if stalls vanish, this is very likely it.
3. **Idle-time correlation.** Add wall-clock timestamps to the `mdbg`
   lines and check whether stalls correlate with time since the last X
   traffic; then add random multi-second sleeps between iterations of
   the standalone repro. If that makes it start stalling, the "only
   reproducible through the real application" finding above no longer
   holds, and the minimal repro becomes the test vehicle for everything
   downstream.
4. **Protocol-level timing.** Run `eza` through `xtrace`/`x11trace` (a
   client-side proxy, works fine against XQuartz alone) to see
   timestamped request/reply flow — whether the server read the
   requests promptly and sat on the reply, or didn't read the socket at
   all for ~500ms, and what (if anything) precedes stall onset.
5. **Rule out `quartz-wm` holding a server grab.** The stall could be
   the server busy servicing another client for those 500ms —
   `quartz-wm` reacts to window maps and could plausibly
   `XGrabServer()` around one, which would serialize `eza` exactly like
   this even though the menu windows are override-redirect. Try
   `killall quartz-wm` and reproduce.
6. **Bisect the real app downward** instead of building the minimal
   client upward: toggle `save_under` (`menubar.c:1842-1853`) and
   `UseBackingStore` off one at a time in `eza` itself, rather than in
   a standalone analogue — attribute effects can depend on what's
   underneath the popup (eza's deep window stack vs. a test program's
   bare root window), so matching attributes in a minimal client
   doesn't necessarily cover this.
7. **Grep XQuartz's source.** `xorg-server`'s `hw/xquartz` is open
   source; a ~500ms literal in the `darwinEvents`/fd-handling or
   dix↔AppKit handoff paths would turn the hypothesis into a citation
   instead of an inference.

**Follow-up test results (2026-08-11).**

*Test 2 (App Nap A/B) — negative.* `defaults write org.xquartz.X11
NSAppSleepDisabled -bool YES`, XQuartz restarted, retested: stalls
persisted unchanged (511-516ms). This rules out *that specific*
mechanism, though not the broader idle-throttling category — App Nap
is one opt-out among several possible (timer coalescing and other
wakeup-cadence mechanisms aren't controlled by this default).

*Test 1 (sample the other side) — positive, and more specific than
expected.* First attempt was invalidated by a timing bug: `sample` is
synchronous for its full duration, and starting it inline in the same
turn as asking for reproduction clicks meant the window had already
started elapsing before the request was even read — that capture
showed the X server's protocol thread (`dix_main → Dispatch →
WaitForSomething`) 100% idle throughout, i.e. no clicks landed inside
it. Rerun with `sample` launched in the background first, confirming
the window was live, *then* asking for reproduction clicks
(`sample 76082 25 -file /tmp/xquartz_sample2.txt`, XQuartz's `X11.bin`,
PID 76082) caught real activity: of 20985 total samples, 20709 were
idle in `WaitForSomething`, but 267 were genuine dispatch work. Of
those 267, 238 sat in one specific chain:

```
ProcClearToBackground → miClearToBackground → RootlessPaintWindow →
  RootlessStartDrawing → xprStartDrawing → xp_lock_window →
  SLSLockWindowRectBits → SLSGetWindowBackingStoreInformation →
  CGSWindowUpdateBackingStoreData → SLSCopyWindowClipShape →
  SLSConnectionSynchronizeSLSCATransaction → _SLSTransactionWaitSource →
  mach_msg (blocked)
```

`ProcClearToBackground` is the server-side handler for an
`XClearWindow` request — exactly the call `DrawMenuItems`
(`menubar.c`) makes immediately before drawing menu text. Servicing it
on the rootless-XQuartz path requires locking the window's backing
store through SkyLight (macOS's private window-server/Core Animation
API), and that lock requires the X server to synchronize a Core
Animation transaction with the actual WindowServer process — a
genuine cross-process wait (`mach_msg`), entirely outside XQuartz's or
AUIS's own code. Smaller branches showed the same pattern from other
request types: `ProcConfigureWindow → RootlessResizeWindow` (25
samples) and one each of `ProcMapWindow`/`ProcPolySegment` also
transited `SLSLockWindowRectBits`/the SkyLight sync path.

This closes the loop from our own code down to a concrete OS
mechanism, and narrows "macOS-side idle-throttling" from a category
guess to a named call path: if the WindowServer paces/batches Core
Animation transaction commits for a non-frontmost app's windows (XQuartz,
launched from a terminal, would typically not be key/frontmost), a
menu post that lands between commits would block in exactly this
`_SLSTransactionWaitSource` wait for however long until the next
commit — a plausible, external explanation for a fixed ~510ms quantum
that neither AUIS nor XQuartz controls directly. Caveat: `sample`'s
~1ms interval makes 238 samples ≈ 238ms of wall time in this wait
during the 25s window, which is suggestive but not yet proven to
correspond 1:1 to a single logged ~510ms stall — the natural next
confirmation step is correlating `mdbg` log wall-clock timestamps
against this sample file's timestamps directly, rather than inferring
from sample counts alone.

**Confirmed by direct correlation (2026-08-11).** The `mdbg` tracing
was extended to prefix every line with a `[HH:MM:SS.mmm]` wall-clock
timestamp (all three tracing sites: `xim.c`, `menubar.c`,
`cmactiv.c`), specifically to answer the caveat above — whether the
SkyLight wait actually accounts for the logged stall time, not just
correlates with it in aggregate. A `sample 76082 45
-file /tmp/xquartz_sample3.txt` capture was started first (its exact
start time recorded from the report header:
`2026-08-11 16:54:27.088`), confirmed running, then menu cards were
posted live. The `mdbg` log for that run showed exactly 3 stalls:

```
[16:54:38.685] ... XSync(serverpaint)=510ms TOTAL=510ms
[16:54:40.036] ... XSync(serverpaint)=512ms TOTAL=512ms
[16:54:41.569] ... XSync(serverpaint)=511ms TOTAL=511ms
```

1533ms of independently measured, ground-truth stall time. The sample
of X11.bin's `dix_main` thread, for the same window, showed:

- 37970 total 1ms samples; 36023 idle in `WaitForSomething`; 1947
  samples (~1.95s) of real dispatch work.
- Of that real work, **`ProcClearToBackground → RootlessStartDrawing →
  xp_lock_window → ... → _SLSTransactionWaitSource`: 1241 samples,
  1240 of them actually inside the wait** — the same chain found in
  the first capture, now driven by the exact `XClearWindow` calls this
  run's `BringUpMenu`/`DrawMenuItems` issued.
- Smaller amounts of the identical wait under `ProcMapWindow` (23 of
  26 samples) and `ProcConfigureWindow` (15 of 22 samples).

Total time inside `_SLSTransactionWaitSource` across all three request
types: ~1278ms, against 1533ms measured independently — roughly 80-85%
direct overlap, with the remainder plausibly attributable to
setup/mapping overhead during the same window. This is a materially
tighter, ground-truth-anchored match than the first capture's
aggregate-count inference, and is treated as confirmation rather than
correlation: **the delay is genuine, server-side time blocked waiting
for macOS's own window compositor (SkyLight/Core Animation) to
synchronize a transaction before XQuartz is permitted to lock the
popup window's backing store and paint into it.** A second,
non-blocking `xp_lock_window` variant (469 samples, different call
offset, no `_SLSTransactionWaitSource` beneath it) was also present in
the same capture — a plausible explanation for why *most* draws
outside a stall are instant: ordinary redraws that don't require a
fresh Core Animation commit take a fast lock path instead.

**Frontmost-app theory tested and ruled out.** The leading candidate
for *why* the wait is sometimes ~510ms and sometimes ~0ms was that
WindowServer paces Core Animation commits differently for a
non-frontmost app's windows, and XQuartz (launched from a terminal) is
typically not key/frontmost. Tested directly: clicked the XQuartz Dock
icon to force it frontmost immediately before posting menus, repeated
several times. Stalls persisted unchanged (510-515ms). Methodological
note: this test is weaker than it looks, since clicking any menu-bar
control at all necessarily makes XQuartz frontmost as part of the
click (background apps can't receive that click without being
activated first) — so every prior test was already conducted with
XQuartz frontmost at the moment of posting, and this test didn't
introduce a genuinely new condition. It's still useful as a negative
data point (the explicit, deliberate extra activation step changed
nothing), but the frontmost/background distinction should be
considered untested cleanly rather than disproven with high
confidence.

**Two questions closed without new testing, from existing evidence:**
- *Is this caused by AUIS's `XGrabPointer` call (needed for
  cross-window pointer tracking while dragging through the
  menu/submenu stack)?* No — already answered by ruled-out item 6
  above: decoupling the draw from the grab (explicit `XFlush` before
  `DoMenuLoop` issues `XGrabPointer`) made no difference, and the
  newly found wait lives entirely inside the server's own handling of
  `XClearWindow` (`ProcClearToBackground`, on `dix_main`), upstream of
  any client synchronization choice. A client that never called
  `XSync`/`XGrabPointer` at all would still not have its drawn text
  appear on screen until the server finished this same locked,
  SkyLight-gated paint. `XGrabPointer` remains structurally necessary
  for menu interaction regardless.
- *Could multiple pre-warmed/`save_under` popup windows finesse this?*
  No — `save_under` governs restoring pixels that were *behind* a
  popup on unmap, not the lock/sync cost of drawing *into* the popup,
  so it doesn't touch this code path at all. A "keep windows warm"
  strategy is also unsupported by the data: the very first post into a
  freshly created window was fast (23ms), while every post after it —
  including re-posting the same card roughly a second later — was
  slow, which is the opposite of what a warm-cache theory predicts. A
  persistent-window (map/unmap instead of clear/redraw) variant of the
  idea is also not obviously better: the `ProcMapWindow` samples above
  went through the identical wait 23 of 26 times, suggesting mapping
  alone can trigger it too, not just clearing/drawing.

**Where this leaves things.** The *where* is proven: this is real
time blocked inside macOS's own window-compositor synchronization,
triggered by XQuartz's rootless-drawing lock path, external to both
AUIS and XQuartz's own code. The *why* — what makes that
synchronization sometimes instant and sometimes ~510ms — remains
unresolved; App Nap and frontmost/background status are both ruled
out as the mechanism, leaving some other internal SkyLight/WindowServer
cadence (a periodic commit/heartbeat interval, unrelated to which app
or window is involved) as the remaining plausible explanation, not
confirmed. No fix is available from the AUIS side: the delay
originates entirely in a macOS system component neither AUIS nor
XQuartz controls, and the client-side mitigations considered (grab
decoupling, save-under, window pre-warming) either don't touch the
responsible code path or aren't supported by the data. Treated as an
accepted platform characteristic of running X11 through XQuartz's
rootless architecture on modern macOS, not a defect to keep chasing —
revisit only if a second X server (for an X.org comparison) or public
documentation of SkyLight's transaction-commit scheduling becomes
available.

**Not pursued further this session.** The original next step proposed
here — running the same minimal reproduction C program against a real
X.org server instead of XQuartz, to determine whether this is
XQuartz-specific (in which case a demo on Linux/X.org would sidestep
it entirely) — still needs a second X server, not available in this
environment (XQuartz is the only X server on the Mac used for this
revival; no Linux VM/container tooling was set up at the time). The
seven tests above don't have that dependency and are better first
moves regardless; revisit the X.org comparison after them, or when a
second server becomes available.

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
