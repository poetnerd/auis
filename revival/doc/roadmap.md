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
- Fad insets: fully working (LP64 fix + Xft XOR ghost fix + 30ms timing floor)
- Fnote insets: marker glyph centered correctly (Xft metrics fix, 2026-07-04)
- `help` app: Programs list panel now shows the top of the list on first
  display, instead of scrolling to the end (LP64 fix, 2026-07-04)
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

**Variant 3 follow-up audit (2026-07-04) — bare `-1` literals at call sites,
not just `#define`d sentinels:**

Found via `help`'s list-panel scroll bug: `textv.c:455`,
`self->frameDot = text_CreateMark((struct text *) dataObject, -1, 0)`.
`text_CreateMark` dispatches through the untyped `(struct mark *(*)())`
class-method macro, so the bare `int` literal `-1` isn't sign-extended to
the `long pos` field on LP64 — `frameDot->pos` ends up as garbage (observed:
`4294967295`, `4294967335`, `8589934591` in lldb — all the "low 32 bits look
like -1, upper 32 bits are register garbage" signature). `DoUpdate`'s
`mark_GetPos(self->frameDot) != -1` check then spuriously fires on every
first redraw. **Fixed**: `(long)-1` cast at the call site.

This is the same root mechanism as Variant 3 (`observable_OBJECTDESTROYED`
etc.) but the `-1` is a bare call-site literal rather than a named
`#define`d sentinel, so grepping for the constant's name doesn't find it —
it only shows up by grepping call sites directly. A full sweep (see
methodology below) turned up **6 more confirmed instances**, all fixed with
the same `(long)-1` cast, all compile-clean and rebuilt/reinstalled:

- `src/atk/text/content.c:649`, `src/atk/textaux/contentv.c:134,186` —
  `content_Enumerate`/`content_Denumerate`'s `opos`/`pos` (`long`) is
  checked `< 0` as an "enumerate everything" sentinel in
  `src/atk/text/content.c` (`content__Enumerate`/`content__Denumerate`);
  the corrupted value would read as a huge positive number instead,
  silently skipping the enumerate-all path.
- `src/atk/figure/figv.c:129-130` — `ChangeZoomProc`'s `rock` (`long`) is
  checked `rock<0`/`rock>0` to decide zoom in vs. zoom out; the "Zoom Out"
  menu item and `Esc-z` keybinding both passed a bare `-1` through
  `menulist_AddToML`/`keymap_BindToKey` (both untyped dispatch). If
  corrupted, **Zoom Out would zoom in instead**.
- `src/atk/raster/cmd/rasterv.c:1634-1635` — `ModifyCommand`'s `rock`
  (`long`) is checked `rock == -1` (exact equality) for "invert
  selection"; the "Negative" menu item and `Esc-n` keybinding both passed
  a bare `-1` through the same two untyped-dispatch macros. If corrupted,
  **Negative would silently do nothing** (falls through all the `==`
  branches).

**Audit methodology** (repeatable for future sweeps):
```sh
# class-dispatch-style calls (lower_Upper(...)) with a bare -1 argument
grep -rEn '\b[a-z][a-zA-Z0-9]*_[A-Z][a-zA-Z0-9]*\([^;()]*(,|\() *-1 *(,|\))[^;]*\)' \
  src/ --include=*.c | grep -v "(long)-1\|(long) -1\|== *-1\|!= *-1"
```
This narrowed ~925 raw `-1`-near-parens hits down to 22 candidates. Each
candidate needs manual triage: (1) find the macro's dispatch — untyped
`(TYPE (*)())` cast is the risky pattern, a plain field-assignment macro
(e.g. `mark_SetPos`, `rectangle_SetRectSize`) or a normal prototyped C
function is safe; (2) find the receiver and check whether it actually
*consumes* the value in a way sensitive to its exact bit pattern (a sign
check `< 0`/`== -1`, or arithmetic) vs. ignoring the parameter entirely
(several `view_FullUpdate(...,-1,-1)` width/height args in `figv.c` and
`rastvaux*.c` are ignored by both `figview__FullUpdate` and
`rasterview__FullUpdate`, which recompute geometry from the view instead —
confirmed harmless despite passing through the same untyped mechanism).

**Deferred / not yet triaged** (lower priority, left as future audit
targets, not confirmed either way):
- `src/atk/basics/common/rect.c` / `figv.c:905` — `rectangle_InsetRect`
  is a plain function but its header prototype
  (`rect.h:73: void rectangle_InsetRect(/*LHS, DeltaX, DeltaY*/);`) has no
  parameter types, so call sites don't widen `-1` to the real `long
  DeltaX, DeltaY`. Unlike the confirmed bugs above, the receiver does
  arithmetic (`+=`/`-=`) rather than a sign check, so a corrupted value
  would grossly mis-size a rectangle rather than silently no-op — worth
  checking given `figure` insets are the one confirmed-broken "messy
  screen" case in this document.
- `environ_GetProfileInt(...,-1)` (messages/atkams, several sites) and
  `cwp_Search(...,-1,...)` (ams/delivery) — likely safe (looks like a
  plain `int`-returning function, not virtual dispatch) but unverified;
  deprioritized since `messages`/AMS revival is long-term, not part of
  the active inset sweep.
- `tlex_RecentPosition(...,-1 or -2,...)` (ness) — moot until the `ness`
  bison grammar extension blocker is resolved; the code doesn't run yet.

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

## Heisenbugs (intermittent, not currently reproducible)

### ^V scroll hang on fresh window (spoon host)
**Observed:** on host `spoon`, repeatedly: opening `help` or `ez` and
pressing `^V` to scroll the default window caused the process to die
(confirmed dead in debugger) while the X window persisted until XQuartz
was restarted. `help` would still believe a server process was available.
Triggered even on the default help window with no special document.
Stopped reproducing spontaneously once debugging attempts began.

**Not reproduced on:** Mac-mini. Not triggered by VS Code terminal (was
running from native Terminal.app).

**Possible cause (superseded, see below):** the checkpoint timer UAF
(`observable_OBJECTDESTROYED` zero-extension bug, fixed 2026-06-30) was a
plausible match — scrolling a fresh window can trigger a checkpoint, and
the UAF produced a crash rather than a hang.

### Xlib display-lock self-deadlock (reproduced 2026-07-04, root cause identified)

Reproduced by accident during the figure-inset LP64 audit: scrolling in
both `ez` (viewing `NEWSLETTERS/EZ/95Summer.ez`) and, independently, a
`help` window hung the same session. **Not a crash this time** — attaching
lldb to the stuck `help` process (no relaunch, no interrupt needed — it
was already wedged) showed:

```
frame #0: libsystem_kernel.dylib`__psynch_mutexwait
frame #1: libsystem_pthread.dylib`_pthread_mutex_firstfit_lock_wait
frame #2: libsystem_pthread.dylib`_pthread_mutex_firstfit_lock_slow
frame #3: libX11.6.dylib`_XLockDisplay
frame #4: libX11.6.dylib`XkbGetUpdatedMap
frame #5: libX11.6.dylib`XkbKeysymToModifiers
frame #6: libX11.6.dylib`XRefreshKeyboardMapping
frame #7: runapp`HandleWindowEvent
frame #8: runapp`xim__HandleFiles
frame #9: runapp`im__Interact
frame #10: runapp`im__KeyboardProcessor
frame #11: runapp`application__Run
frame #12: helpa.do`helpapp__Run
frame #13: runapp`main
```

Only **one thread exists** in the process, and it's blocked forever trying
to acquire Xlib's own display-connection mutex (`_XLockDisplay`) — a
self-deadlock, not a cross-thread one. `HandleWindowEvent` is responding
to an X `MappingNotify` (keyboard mapping changed) by calling
`XRefreshKeyboardMapping`, which tries to lock the display — but something
earlier in the *same* call chain already holds that lock (almost
certainly Xlib's own event-dispatch machinery calling back into
`xim__HandleFiles`/`HandleWindowEvent` while still holding it internally),
and the lock isn't held recursively. This matches the "intermittent,
input-related, seems tied to fresh windows" character of the original
report far better than the checkpoint-timer theory, and is **not** fixed
by any patch committed so far — this is a live, distinct bug.

**Trigger appears to be keyboard-mapping churn** (`MappingNotify`), not
scrolling logic itself — consistent with why `^V` specifically was the
original trigger (a modifier-involving key combo) and why it's
intermittent (depends on X server-side keymap-change timing, not app
state).

**Not yet investigated:** which AUIS/Xlib call site re-enters
`_XLockDisplay` while already holding it; whether this is triggerable
deliberately (vs. needing to wait for an incidental `MappingNotify`);
whether it's an AUIS-side bug (calling into Xlib from inside a callback
that already holds the lock) or an XQuartz/libX11 packaging issue specific
to this environment. Needs a dedicated debugging session — see
`revival/doc/runtime-debugging-guide.md` for the general lldb debugging
process/cookbook developed for this project.

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
instance. Also check `rectangle_InsetRect(&inrec, -1, -1)` at `figv.c:905`
— see the Variant 3 follow-up audit above; its unprototyped
`(DeltaX, DeltaY)` args do arithmetic rather than a sign check, so a
corrupted `-1` would grossly mis-size a rectangle rather than silently
no-op, a plausible contributor to "messy screen."

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
