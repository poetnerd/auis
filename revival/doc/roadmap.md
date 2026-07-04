# AUIS Revival Roadmap

Current as of 2026-07-03. See `porting-changelog.md` for the detailed
history behind each completed item.

---

## Completed

- `ez2md` converter (`revival/tools/ez2md.py`): converts `.ez` files to
  Markdown; handles styles, footnotes, page breaks, nested insets; tested
  against 51 archive documents
- Full build: 278 `.do` files, 0 errors, `make dependInstall` exit 0
- `ez` launches, renders documents, scrollbar/menus/keyboard input all work
- `help` application: multiple frames, topic navigation working
- Dialog boxes: visible, correctly positioned, text readable
- Raster insets: rendering correctly
- Table insets: cell text visible (LP64 `update.c` fix)
- Eq insets: complex equations render correctly
- Fad insets: fully working
- Srctext: indentation rendering fixed (LP64 audit)
- `Sherman.Alloc` integration test: text, eq, fad, cel/arbiter spreadsheet
  all render; zip unsupported (expected); calc engine presumed working if
  cel displays correctly
- Xft phase 1: body text rendering via client-side Xft (anti-aliased)
- Andy symbol fonts (`symba*.pcf`) built and installed in `build/X11fonts/`

**LP64 bug classes identified and swept:**
- Variant 1: Missing prototypes / pointer return truncation (23 sites)
- Variant 2: >8-arg untyped dispatch stack spill (classpp fix)
- Variant 3: `int` constant zero-extended through untyped dispatch
  (`observable_OBJECTDESTROYED`, `value_OBJECTDESTROYED`,
  `class_VERSIONNOTKNOWN`)
- Variant 4: `%d` with `long *` in scanf family (full tree audit done)
- Variant 5: `long`/`int` mismatch in display positioning through untyped
  dispatch (lpair, panel, dialog, table, fad, srctext, eq, metax, toez,
  typescript margins; style__ReadAttr operand; figure_NULLREF sentinel; full audit committed)

---

## Subsystem dependency lattice

Indentation shows inheritance / dependency. `[PROVEN]` means confirmed
working through runtime testing. Unlabelled leaves are untested.

```
overhead/class  [PROVEN — loader, everything depends on this]

atk/basics  [PROVEN — view, graphic, scroll, lpair, panel, sbuttonv, xgraphic, xfontd, xim]

dataobject [PROVEN]          view [PROVEN]
    |                            |
    +-- bp / bpv  [PROVEN]       +-- scroll / sbuttonv  [PROVEN]
    +-- raster    [PROVEN]       +-- lpair              [PROVEN]
    +-- text ---- textview  [PROVEN]
    |        +-- fnote                (text-in-text; trivially proven)
    |        +-- textref / texttag    (cross-refs; text mark system proven by eq)
    |        +-- rofftext             (extends text)
    |        +-- srctext              (extends text)
    |
    +-- eq -- eqview  [PROVEN — complex equation rendered from ia-archive/dec.91]
    +-- table         [PROVEN — cell text visible after LP64 fix]
    +-- fad -- fadview  [PROVEN]
    +-- pushbutton -- pushbuttonview
    |        +-- link -- linkview     (hyplink)
    +-- lookz -- lookzview
    +-- value -- valueview
    |        +-- sliderv
    |        |      +-- bargraphV
    |        +-- buttonV
    |               +-- controlV, enterintV, enterstrV, fourwayV
    |        +-- stringV -- clklistV
    +-- apt -- aptv
    |        +-- org -- orgv          (outliner)
    |        +-- chart                (charts)
    +-- cel                           (ADEW spreadsheet cell; needs value+text)
    |        +-- arbiter              (ADEW application builder)
    +-- lset                          (scrollable list view)
    +-- figure -- figview             [BROKEN — messy screen]
               +-- figobj hierarchy
                      (figorect, figoplin, figoell, figogrp, figotext...)

application [PROVEN via ez, help]
    +-- ez          [PROVEN]
    +-- help        [PROVEN — multiple frames, topic nav, frame size correct]
    +-- fad         [PROVEN]
    +-- typescript / pipescript       (terminal emulator)
    +-- bush                          (shell)
    +-- org (orga)                    (outliner as standalone app)
    +-- chart (chartapp)
    +-- launchapp
```

Note: `help` does not use `lset` — confirmed by source audit. `lset` is
unproven and requires its own test.

---

## Inset testing sequence

Ordered by dependency depth; each step proves a layer the next relies on.
`[PROVEN]` items are already confirmed; start from the first unconfirmed entry.

| # | Inset / App | Test document | What it proves |
|---|-------------|--------------|----------------|
| 1 | fnote | `NEWSLETTERS/EZ/92Sep.ez` | inline text-in-text insets |
| 2 | textref / texttag | `PAPERS/atk/Cattey.Writing` | cross-ref insets; text mark system (likely works — eq proved marks) |
| 3 | **eq** | `ia-archive/dec.91` | **[PROVEN]** equation editor; text marks confirmed |
| 4 | **table** | `ia-archive/aug.90` | **[PROVEN]** cell text visible |
| 5 | value (slider/button) | `ia-archive/sep.90` or `ia-archive/jan.90` | value views; valueview dispatch chain |
| 6 | lset | `ia-archive/nov.91` or `ia-archive/jan.90` | scrollable list widget |
| 7 | pushbutton / link | `PAPERS/conf/1995/widgets.ez` | hyplink chain: pushbutton→link→linkview |
| 8 | **fad** | `src/atk/adew/Title.doc` | **[PROVEN]** LP64 fix complete |
| 9 | org | `src/atk/org/example1.org` | outliner; proves apt→aptv base |
| 10 | rofftext | `bin/rofftext -d <manpage>` | roff formatter on top of text |
| 11 | chart | `build/doc/atk/classes.org` or synthesize | proves apt branch independently of org |
| 12 | **cel / adew** | `src/atk/adew/vallist` | **[PROVEN via Sherman.Alloc]** ADEW stack: value+text+cel+arbiter renders |
| 13 | typescript | `bin/typescript -d` | terminal emulator; **crashes "Can't connect subchannel"** — likely macOS PTY compat issue, not LP64 |
| 14 | bush | `bin/bush -d` | shell application |
| 15 | **figure** | `NEWSLETTERS/EZ/95Summer.ez` | **[BROKEN — messy screen]** figure_NULLREF (-1L) fix committed; retest after commit |
| 16 | **Sherman.Alloc** | `PAPERS/atk/Sherman.Alloc` | **[PROVEN]** text+eq+fad+cel/arbiter all render; zip unsupported (expected) |

**No good test document exists for:** `lookz`, `srctext`, `launchapp`, `prefed`
— these need synthetic test files or targeted app launches.

**Known non-starters:** `ness` (bison extension), `zip` (unsupported),
`clock`/`writestamp`/`timeoday` (contrib, lower priority).

---

## Active (instances running)

- **Xft phase 2: menu rendering** — deprioritized; menus are working
  acceptably with the current rendering path

---

## Near-term

### LP64 positioning sweep completion
Complete the audit across all remaining `src/atk/` insets. Once the
active audit instance reports back, triage any unclear cases and verify
with `Sherman.Alloc` and `95Summer.ez` as integration test documents.

### Figure inset rendering
Figure insets load but render incorrectly ("messy screen"). The
`figure_NULLREF (-1L)` fix from the LP64 audit may partially address it —
retest after committing. If still broken, needs a dedicated debugging
instance.

### typescript "Can't connect subchannel"
`bin/typescript -d` crashes immediately after "please wait..." with
"Can't connect subchannel" + SIGSEGV. The error occurs before any
rendering, pointing to a PTY/pipe setup failure (macOS compat, not LP64).
Find the error string in `src/atk/typescript/` to identify the failing
syscall.

### printf/fprintf %d/%ld audit
2,597 printf-family hits with `long` values and `%d` format specifiers
logged during the scanf audit. These produce wrong output for large values
but do not corrupt memory. Lower priority than the positioning fixes;
address as a batch once the inset sweep is complete.

### Xft phase 2 (deferred)
Menu text rendering via Xft. Menus are currently acceptable without it;
defer until after higher-priority inset work. Once done, the only
remaining X core font path dependency would be the Andy symbol and
cursor fonts.

### Update quickstart.md
Remove resolved known-issues entries as each fix lands.

---

## Medium-term

### Integration test: `Sherman.Alloc`
Open `PAPERS/atk/Sherman.Alloc` in `ez` — contains `fad`, `calc`, `cel`,
`eq`, `table`, and `zip` insets. A clean render of this document is a
meaningful milestone for multi-inset support.

### zip inset
Currently reports "not supported." Investigate whether the zip inset
(compressed compound document container) is buildable from the 6.3.1
source and what it would take to enable.

### ness.gra bison extension
`atk/ness/objects/ness.gra` uses a multi-character string token extension
specific to the Andrew bison fork. Ness scripting is unavailable until
this is resolved. Options: implement the extension in a bison `%skeleton`
or rewrite the affected grammar rules.

### Andy font path automation
`xset fp+ build/X11fonts && xset fp rehash` is currently a manual step
required each XQuartz session. Automate via a wrapper script or by
installing the PCF files into XQuartz's default font path
(`/opt/X11/share/fonts/`).

### ~~Frame size reporting in help~~ — fixed

---

## Long-term / architectural

### Classpp: typed dispatch for all methods
The classpp fix (2026-06-30) emits typed casts only for methods with ≥9
arguments. Extending this to all methods would eliminate the entire LP64
variant-5 dispatch zero-extension class permanently, without requiring
any changes to `.c` implementation files. This is the right structural
fix; the current per-inset sweep is the interim approach.

### ANSI C modernization
A deliberate, file-by-file pass to add prototypes, convert K&R parameter
declarations, and fix implicit-int patterns -- now that the system runs
and application behavior provides a validation baseline. The `modernize`
tool in `revival/tools/` is a starting point but has known parsing bugs
that should be fixed before use at scale.

### Messages application

`messages` is the AUIS mail/bulletin-board client. Full AMS revival is
off the table — the AFS/shared-filesystem delivery model is a dead end.
Two viable paths, in order of investigation:

**Step 1 — find the local mail store backend (fast, high potential):**
The user ran `messages` with a local mail store at MIT Athena in the
1990s (using a forked `fetchmail` to bypass AFS delivery). This means a
local-store backend likely already exists in `ams/` — look for `#ifdef`
variants, `site.h` configuration options, or alternate delivery paths in
`ams/libs/` and `ams/delivery/`. If found and revivable, this is the
shortest path to a working `messages`.

**Step 2 — audit the `atkams/` interface boundary:**
Understand what `messages` calls into `atkams/`, and what `atkams/` calls
into `ams/`. If the boundary is clean, an IMAP adapter behind `atkams/`
is viable. If AMS internals are deeply mixed in, a stub AMS layer (even
for IMAP) may be needed regardless.

**Prerequisite:** all ATK insets working (particularly text, raster, eq,
fad, table) — `messages` renders mail as ATK compound documents, so inset
rendering quality directly affects mail readability.

---

## Stretch goals

### Pie menus
AUIS's menu architecture is well-suited to Don Hopkins' pie menu design.
The menu system is clean and the attachment points are known. A
collaborative implementation project once the core system is stable.

### Additional applications
`bush` (shell), `typescript` (terminal emulator), `org` (outliner),
`chart`, `layout` -- each is a symlink to `runapp` and built; exercise
and fix as interest warrants after core insets are stable.
