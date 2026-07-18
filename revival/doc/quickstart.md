# AUIS Revival: Running ez on macOS/Darwin

This guide covers building and running `ez` and other AUIS applications
from the `andrew-6.4` revival checkout on macOS with XQuartz. It
reflects the state of the revival as of mid-2026; consult
`porting-changelog.md` for the history behind each decision.

For getting `messages`/`cui` working against a real IMAP/SMTP mail
account, see `mail-quickstart.md`.

## Prerequisites

**XQuartz** — the X11 server for macOS. Install via:
```
brew install --cask xquartz
```
Or download from xquartz.org. XQuartz must be running for any AUIS
application to open a window. Modern XQuartz sets `$DISPLAY`
automatically via a launchd socket (`/private/tmp/com.apple.launchd.*/
org.xquartz:0`); you do not normally need to set it by hand.

**Build tools** — Xcode Command Line Tools, plus:
```
brew install bison flex imake makedepend
```
- `bison` and `flex`: the build uses system versions (the vendored Andrew
  bison hangs on Darwin/arm64 — see `porting-changelog.md` 2026-06-29)
- `imake`: required to regenerate Makefiles on a fresh checkout
- `makedepend`: required for the `make depend` phase

## Building

### Incremental build (normal case)

From `andrew-6.4/src/`:
```
make dependInstall 2>&1 | tee ../dependInstall.log
```
A clean build produces 278 `.do` files and 602 headers with zero errors.
`-k` is not needed; the tree is clean. Do not run concurrent builds.

### Fresh checkout bootstrap

On a brand-new fossil clone with no existing `build/` directory, the
generated Makefiles and `build/include/` tree do not yet exist. Follow
the bootstrap sequence from `src/README.ascii`:

```bash
cd andrew-6.4/src

# Step 1: generate the top-level Makefile via imake
imake -I. -I./config -Timake.tmpl -s Makefile -DTOPDIR=.

# Step 2: World = make Makefiles (all subdirs) + make dependInstall
make World 2>&1 | tee ../dependInstall.log
```

`make World` regenerates all subdirectory Makefiles via imake, creates
the `build/` directory tree, builds all libraries and binaries, generates
`.ih`/`.eh` headers from `.ch` class specs, and installs everything.
On a working tree, `make dependInstall` is equivalent and faster (skips
the Makefile regeneration step).

**On a genuinely empty `build/`** (not just a fresh clone — also any
time `build/` has been deliberately wiped, e.g. to bisect an old
revision), run `../revival/tools/prime-class-header` first, *before*
`make World`. `overhead/class/Imakefile` builds `machdep` before `lib`,
but `lib` is what installs `class.h` into `build/include/` —
`machdep/darwin/classproc.c` needs it already there. This is invisible
once `build/` has existed for a while (which is every normal case), so
it only bites the first `make World` against a truly empty tree. See
`porting-assessment.md` → "Primary build environment" for the full
writeup. Full recipe for testing a specific historical revision
(fossil's mtimes make incremental rebuilds untrustworthy across
checkouts): `porting-assessment.md` → "Fossil checkout timestamps make
incremental rebuilds untrustworthy...".

Check for LP64 warnings before running new code paths:
```
grep "warning: cast to '.*\*' from smaller integer type" dependInstall.log
```
Any hit is a real bug (pointer-returning function called without a
prototype, return value truncated to 32 bits). Fix with a local
`extern TYPE FunctionName();` declaration. See `porting-changelog.md`
and `porting-assessment.md` §LP64 for the full pattern.

## Terminal setup — important

**Always run AUIS binaries from native Terminal.app or iTerm2**, not from
a VS Code integrated terminal or any VS Code-attached shell.

VS Code's terminal inherits a Mach exception-port relationship that
intercepts crash signals from child processes and never services them.
Any AUIS binary that crashes while launched from a VS Code terminal will
enter an unkillable `UE` (uninterruptible zombie) state that survives
`kill -9` and requires a reboot to clear. The same bug hides real crashes
behind an apparently-hung process, making debugging impossible. Running
from native Terminal.app, the same crash exits normally and can be caught
by `lldb`. See `porting-changelog.md` 2026-06-30 for the full diagnosis.

## Font path setup

AUIS needs its compiled fonts to be visible to the X server. The build
places 40 PCF font files in `build/X11fonts/`. Add this directory to
XQuartz's font path once per XQuartz session (it is lost when XQuartz
quits):

```
xset fp+ /Users/wdc/src/AUIS/andrew-6.4/build/X11fonts
xset fp rehash
```

**Why this is still needed even with Xft text rendering:** the revival
uses a hybrid font approach:

| Rendering path | Font mechanism |
|---|---|
| Body text | Xft / fontconfig (client-side, no X font path needed) |
| Menus | Xft (phase 2, in progress as of 2026-07) |
| Symbol characters (bullets, math marks) | Andy `symba*.pcf` — X font path required |
| Cursor shapes | Andy cursor PCF — X font path required |

The `fonts.alias` file in `build/X11fonts/` maps Andy text font names
(`andysans`, `andytype`) to standard Adobe Helvetica/Courier XLFD names
already present in XQuartz, so no Andy text bitmap fonts need to be
installed. The `symba*.pcf` files (5 sizes: 8, 10, 12, 16, 22 point) are
custom CMU symbol fonts with no standard substitute — without them,
bullet characters render as `7` and other symbol characters are wrong.

A future setup step may automate the `xset fp+` call, either via a
wrapper script or by installing the PCF files into XQuartz's default font
path (`/opt/X11/share/fonts/`).

**`con10`/`con12` (console-app icon fonts) are not among the 40 PCF
files `make World` installs**, because `MK_CONSOLE` gates out all of
`atk/console` (including `console/fonts`) and this revival leaves it
off intentionally. These two fonts are referenced by things outside
`console` too — e.g. `ams/demo/d10`'s `fad` (animation) inset (see Test
documents below) uses `con10` for an icon — so their absence isn't
just "the console app doesn't work," it silently breaks icon rendering
elsewhere. Run
`../revival/tools/install-console-fonts` once per fresh/wiped `build/`
to build and install just these two fonts, without building the rest
of `console`. See `porting-assessment.md` → "`MK_CONSOLE` being off
silently breaks `con10`/`con12`..." for the full story.

## Running ez

Change to the `build/` directory and run:

```
cd /Users/wdc/src/AUIS/andrew-6.4/build
bin/ez -d                          # open a blank document
bin/ez -d doc/README.ez            # open an existing .ez file
bin/ez -d ~/src/AUIS/NEWSLETTERS/EZ/95Summer.ez   # richer test document
```

The `-d` flag prevents `ez` from forking to background, which is useful
during development. Without `-d`, `ez` daemonizes and the shell returns
immediately; crash output goes nowhere visible.

`ez` is a symlink to `runapp`, the generic AUIS application launcher. All
ATK applications (`help`, `bush`, `figure`, `table`, etc.) are symlinks
to the same binary.

## Running help

```
cd /Users/wdc/src/AUIS/andrew-6.4/build
bin/helpa -d
```

Note the argument order: application name before flags (`helpa -d`, not
`-d helpa`). The `-d` flag is parsed by `app.c`'s `ParseArgs` after
`runapp` has identified the target application; flags before the app name
are not seen by the application.

## Known issues (as of 2026-07)

**Figure insets** load but render incorrectly (messy screen). The figure
`.do` object loads successfully via the dynamic class loader; the rendering
bug is inside `atk/figure/`. Under investigation.

**`zip` insets** are not supported — `ez` will display an error for
documents containing them (e.g., some `contrib/mit/neos/doc/` files).

**`ness.gra` bison extension** — the Ness scripting language grammar uses
a multi-character string token extension specific to the Andrew bison fork.
The grammar does not build with system bison. Ness functionality is
unavailable until this is resolved.

## Debugging crashes

Use `lldb` from native Terminal.app:

```
cd /Users/wdc/src/AUIS/andrew-6.4/build
lldb bin/ez -- -d path/to/file.ez
(lldb) run
... wait for crash ...
(lldb) bt
(lldb) frame select N
(lldb) frame variable
```

When a new crash is encountered, check `dependInstall.log` for a
`-Wint-to-pointer-cast` warning at the crashing function before assuming
a new bug type — it is likely an LP64 missing-prototype truncation, the
most common crash class in this codebase. See `porting-assessment.md`
for all known LP64 bug patterns.

## Test documents

Good `ez` test files, in order of increasing complexity:

| File | What it tests |
|---|---|
| `revival/testing.ez` | text, note, calc, eq, table insets; used for the Media-menu regression-test checklist in `roadmap.md` ("ez application (menus)") |
| `ams/demo/d10` | `fad` (animation) inset; exercises the `con10` console icon font — see Font path setup |
| `doc/README.ez` | Basic text, page break insets (bp/bpv) |
| `src/contrib/mit/rtf2/test.ez` | Rich paragraph styles, no insets |
| `src/FAQ.ez` | Long document, footnote insets |
| `src/contrib/mit/neos/doc/history.ez` | Raster image insets |
| `src/contrib/mit/neos/doc/NEOS_stud.ez` | 6 rasters + zip inset |
| `~/src/AUIS/NEWSLETTERS/EZ/95Summer.ez` | Figure insets (messy, under investigation) |
| `~/src/AUIS/NEWSLETTERS/EZ/92Sep.ez` | Raster + footnote insets |
| `~/src/AUIS/PAPERS/atk/Boren.CACM` | Non-.ez format; full justification, multiple font sizes |
