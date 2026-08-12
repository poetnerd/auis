# AUIS Revival: Running ez on macOS/Darwin

This guide covers getting the source, building, and running `ez` and
other AUIS applications on macOS with XQuartz. See `roadmap.md` for
current status and `porting-changelog.md` for the history behind each
decision.

For getting `messages`/`cui` working against a real IMAP/SMTP mail
account, see `mail-quickstart.md`.

## Getting the source

Two mirrors exist; either works. Both use a branch/checkout named
`andrew-6.4` — most of this guide's commands assume you're inside a
directory with that name, and the tree itself contains hardcoded
references to a specific absolute path (fixed in Site configuration,
below).

**GitHub** (mirror, easiest for a one-off checkout):
```bash
git clone -b andrew-6.4 https://github.com/poetnerd/auis.git andrew-6.4
cd andrew-6.4
```
The `-b andrew-6.4` flag checks out that branch directly during the
clone — without it, `git clone` gives you the repo's default branch,
which is not this one.

**Fossil** (canonical repo, needed if you intend to contribute changes
back):
```bash
mkdir andrew-6.4
cd andrew-6.4
fossil clone https://poetnerd.com/wdc/auis auis.fossil
fossil open auis.fossil
fossil update andrew-6.4
```
`fossil clone` downloads the whole repository (history included) into
the `auis.fossil` file; `fossil open` checks out its tip into the
current directory; `fossil update andrew-6.4` makes sure you land on
the `andrew-6.4` branch specifically, since a fresh open doesn't
always land there by default.

Either way, you should now be in an `andrew-6.4/` directory containing
`src/`, `revival/`, `patches/`, and `bison/`. Everything below assumes
you're starting from there.

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

## Site configuration — required before your first build

`src/config/site.h` hardcodes `DEFAULT_ANDREWDIR_ENV`, a path compiled
into every AUIS binary as the fallback location for its own resource
tree (fonts, `etc/AndrewSetup`, help files — everything under `build/`)
whenever the `ANDREWDIR` environment variable isn't set at runtime. As
checked in, it points at the original developer's own checkout path.
**If your checkout isn't at that exact path, edit it before building:**

```
#define DEFAULT_ANDREWDIR_ENV /path/to/your/andrew-6.4/build
```

This only needs doing once, before the first `make World` — the value
gets compiled in, so changing it later means rebuilding. (If you'd
rather not touch a tracked file, setting the `ANDREWDIR` environment
variable to the same path at runtime overrides this default without a
rebuild — but you'd need to set it in every shell session that runs an
AUIS binary, so editing `site.h` once is usually less friction.)

## Building

### First build (fresh checkout)

From the `andrew-6.4/` checkout root, run all three steps below in
order — the first is easy to miss because its symptom is a *fatal*
compile error (`classproc.c:12:10: fatal error: 'class.h' file not
found`) that shows up two steps later, in a part of the build that
looks unrelated:

```bash
cd src

# Step 1: class.h must exist in build/include/ before machdep can build
# against it. On a brand-new checkout nothing has been installed yet,
# so this has to run first, by hand, exactly once.
../revival/tools/prime-class-header

# Step 2: generate the top-level Makefile via imake
imake -I. -I./config -Timake.tmpl -s Makefile -DTOPDIR=.

# Step 3: World = regenerate all subdirectory Makefiles + full build + install
make World 2>&1 | tee ../dependInstall.log
```

`make World` regenerates all subdirectory Makefiles via imake, creates
the `build/` directory tree, builds all libraries and binaries,
generates `.ih`/`.eh` headers from `.ch` class specs, and installs
everything. Expect it to take a few minutes. A clean build produces
278 `.do` files and 602 headers with zero errors.

**Expect thousands of `makedepend` "cannot find include file" warnings
along the way — that's normal, not a sign anything is missing.**
`make World` runs as two full tree-wide passes, `depend` then
`install` (`config/imake.tmpl`'s `world::` target) — every directory's
dependency scan happens before *any* directory's generated `.ih`/`.eh`
headers are actually installed into `build/include/`. So `makedepend`
routinely can't find a sibling directory's generated header yet and
prints `warning: ... cannot find include file "foo.ih"`; a known-good
build log here has over 5,000 of these. They're warnings, not errors —
`make` continues past them, and the header shows up once its own
directory reaches the `install` pass later in the same run. This is a
different problem from Step 1's `class.h` issue above (that one is a
hard compile failure, not a `makedepend` warning). If a wall of these
warnings scrolls by, that's expected; what tells you whether the build
actually worked is whether the run ends with the zero-errors success
line above, not whether these warnings appeared.

Why step 1 is needed: `overhead/class/Imakefile` builds `machdep`
before `lib`, but `lib` is what installs `class.h` into
`build/include/` — `machdep/darwin/classproc.c` needs it already
there, and on a truly empty `build/` nothing has installed it yet.
This only bites the very first build; every build after this one
already has `class.h` in place. See `porting-assessment.md` → "Primary
build environment" for the full writeup. (The same three steps apply
any time `build/` is deliberately wiped from scratch, e.g. to bisect
an old revision — not just on a fresh checkout. Full recipe for
testing a specific historical revision, since fossil's mtimes make
incremental rebuilds untrustworthy across checkouts:
`porting-assessment.md` → "Fossil checkout timestamps make incremental
rebuilds untrustworthy...".)

Check for LP64 warnings before running new code paths:
```
grep "warning: cast to '.*\*' from smaller integer type" ../dependInstall.log
```
Any hit is a real bug (pointer-returning function called without a
prototype, return value truncated to 32 bits). Fix with a local
`extern TYPE FunctionName();` declaration. See `porting-changelog.md`
and `porting-assessment.md` §LP64 for the full pattern.

### Incremental rebuilds (after the first build)

Once `build/` exists and the first `make World` has succeeded, day-to-day
rebuilds are simpler. From `andrew-6.4/src/`:
```
make dependInstall 2>&1 | tee ../dependInstall.log
```
`-k` is not needed; the tree is clean. Do not run concurrent builds.

## AndrewSetup settings

`build/etc/AndrewSetup` is a hand-authored site-config file, read at
runtime by every AMS/ATK client (`GetConfiguration`, searching a fixed
list of paths ending in `${ANDREWDIR}/etc/AndrewSetup` — this
revival's `site.h` points `ANDREWDIR` at the build tree, so no root
access is needed and no other candidate path on this list exists). It
is **not a build artifact** — no Imakefile creates, installs, or
regenerates it — but it lives inside `build/`, so `make Clean` (or any
other wipe of `build/`) deletes it silently along with everything
else, with no error or warning that it's gone.

Run `revival/tools/write-andrewsetup` any time after a clean to
recreate it (refuses to overwrite an existing file; pass `-f` to
force). Current contents:

```
# AUIS revival local configuration
ThisDomain: fastmail.com

AMS_OnlyMail: No
```

- **`ThisDomain`** — the domain AMS stamps into outgoing `From:`
  headers (`<unix-login>@<ThisDomain>`; any From you type is replaced).
  Without it, mail goes out as `you@your-machine.lan`, which real SMTP
  providers refuse to relay externally. See `mail-quickstart.md` step 3.
- **`AMS_OnlyMail`** — boolean (`Yes`/`No`/`1`/`0`), defaults to `Yes`
  unless the tree is built with `RUN_AMDS_ENV` (this revival never
  defines it — see `allsys.h`). `Yes` hard-restricts the default
  "Expose New" startup folder view to `$HOME/.MESSAGES` only,
  excluding every other mspath root (e.g. an IMAP mirror) *regardless
  of subscription status* — the check happens before subscription is
  ever consulted. Sites that actually want bboards suppressed should
  leave this `Yes`; a personal single-user setup wants `No`. Full
  trace of how this was found: `claude-history/
  folder-visibility-REPORT.md`.

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
xset fp+ /path/to/your/andrew-6.4/build/X11fonts
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
cd /path/to/your/andrew-6.4/build
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
cd /path/to/your/andrew-6.4/build
bin/helpa -d
```

Note the argument order: application name before flags (`helpa -d`, not
`-d helpa`). The `-d` flag is parsed by `app.c`'s `ParseArgs` after
`runapp` has identified the target application; flags before the app name
are not seen by the application.

## Known issues

Status of individual applications and insets, open bugs, and active
work all live in `roadmap.md` — see its "Applications and insets" and
"Open issues" sections, which stay current. This file used to
duplicate that list; it drifted stale, so it doesn't anymore.

## Debugging crashes

Use `lldb` from native Terminal.app:

```
cd /path/to/your/andrew-6.4/build
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

Good `ez` test files, in order of increasing complexity. The last
three rows are historical CMU AUIS newsletters/papers, not part of
this checkout — useful if you have a copy, skippable if you don't:

| File | What it tests |
|---|---|
| `revival/testing.ez` | text, note, calc, eq, table insets; used for the Media-menu regression-test checklist in `claude-history/roadmap-old.md` ("ez application (menus)") |
| `ams/demo/d10` | `fad` (animation) inset; exercises the `con10` console icon font — see Font path setup |
| `doc/README.ez` | Basic text, page break insets (bp/bpv) |
| `src/contrib/mit/rtf2/test.ez` | Rich paragraph styles, no insets |
| `src/FAQ.ez` | Long document, footnote insets |
| `src/contrib/mit/neos/doc/history.ez` | Raster image insets |
| `src/contrib/mit/neos/doc/NEOS_stud.ez` | 6 rasters + zip inset |
| `~/src/AUIS/NEWSLETTERS/EZ/95Summer.ez` | Figure insets |
| `~/src/AUIS/NEWSLETTERS/EZ/92Sep.ez` | Raster + footnote insets |
| `~/src/AUIS/PAPERS/atk/Boren.CACM` | Non-.ez format; full justification, multiple font sizes |
