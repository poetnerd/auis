# AUIS Revival: Running ez on macOS/Darwin

This guide covers building and running `ez` and other AUIS applications
from the `andrew-6.4` revival checkout on macOS with XQuartz. It
reflects the state of the revival as of mid-2026; consult
`porting-changelog.md` for the history behind each decision.

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
brew install bison flex
```
The build uses system `bison` (not the vendored Andrew bison, which hangs
on Darwin/arm64 — see `porting-changelog.md` 2026-06-29).

**imake** — bundled in the source tree; no separate install needed.

## Building

From the `andrew-6.4/` checkout root:
```
make dependInstall 2>&1 | tee dependInstall.log
```
A clean build produces 278 `.do` files and 602 headers with zero errors.
`-k` is not needed; the tree is clean. Do not run concurrent builds.

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
bin/help -d
```

## Known issues (as of 2026-07)

**Dialog boxes** do not appear as visible popup windows. They are created
and functional but remain invisible as top-level windows. Workaround:
dialog interactions are accessible via the menu bar. Under investigation.

**Help shows only one frame** instead of the normal two (topic navigator +
content). The multi-window creation path has a bug; the single content
frame works correctly. Under investigation.

**Figure insets** load but render incorrectly (messy screen). The figure
`.do` object loads successfully via the dynamic class loader; the rendering
bug is inside `atk/figure/`. Under investigation.

**`zip` insets** are not supported — `ez` will display an error for
documents containing them (e.g., some `contrib/mit/neos/doc/` files).

**Frame size reporting in help** causes text to appear clipped at the
right margin. Body text rendering itself is correct (confirmed via `ez`);
the clipping is a geometry bug in how help reports its window size to the
text formatter.

**Cursor displays as letter `a`** — XQuartz is substituting a default
cursor glyph. This is an Andy cursor font rendering artifact. Adding the
Andy font path (see above) may improve this; full resolution is part of
the ongoing font work.

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
| `doc/README.ez` | Basic text, page break insets (bp/bpv) |
| `src/contrib/mit/rtf2/test.ez` | Rich paragraph styles, no insets |
| `src/FAQ.ez` | Long document, footnote insets |
| `src/contrib/mit/neos/doc/history.ez` | Raster image insets |
| `src/contrib/mit/neos/doc/NEOS_stud.ez` | 6 rasters + zip inset |
| `~/src/AUIS/NEWSLETTERS/EZ/95Summer.ez` | Figure insets (messy, under investigation) |
| `~/src/AUIS/NEWSLETTERS/EZ/92Sep.ez` | Raster + footnote insets |
| `~/src/AUIS/PAPERS/atk/Boren.CACM` | Non-.ez format; full justification, multiple font sizes |
