# AUIS Revival Roadmap

Current as of 2026-07-04. See `porting-changelog.md` for the detailed
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
- Fnote insets: marker glyph centered correctly (Xft metrics fix, 2026-07-04); **proven working** — footnote marker and popup text display correctly in `Cattey.Writing`
- `help` app: fully functional — Programs list panel scroll fixed (LP64 fix, 2026-07-04); regression tests below
- Bp (page break) insets: **proven working** — visible as page-break rule in `Cattey.Writing`
- Srctext insets: **proven working** — indentation and syntax coloring render correctly (LP64 audit)
- Figure insets: **proven working** — `95Summer.ez` figure renders correctly (see Chronological log 2026-07-04). Root cause was not LP64: `figattr.c` rejected any attribute name it didn't recognize (`arrowsize`/`arrowpos`/`arrow`/`linestyle`, added by a later figure-format version) with `dataobject_BADFORMAT`, and `simpletext__HandleBegindata` ignored `dataobject_Read`'s error return, so the parser desynced and dumped the rest of the document as raw text. Fixed by applying official CMU `patch.633` (figattr tolerates unknown attributes) plus new hardening in `smpltext.c` (failed inset reads now fall back to a raw `unknown` object instead of corrupting the rest of the parse)
- `Sherman.Alloc` integration test: text, eq, fad, cel/arbiter spreadsheet
  all render; zip unsupported (expected); calc engine presumed working if
  cel displays correctly
- Xft phase 1: body text rendering via client-side Xft (anti-aliased)
- Andy symbol fonts (`symba*.pcf`) built and installed in `build/X11fonts/`
- `overhead/malloc/malloc.ci` `addarena` arena-size pointer-arithmetic bug fixed (`patches/contrib/malloc.ci.auis6.3.diff`, 2026-07-04) — source-correctness only, see Historical patches audit below for why it has no runtime effect here

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
  would grossly mis-size a rectangle rather than silently no-op. (Not the
  cause of the `95Summer.ez` "messy screen" case — that turned out to be
  a figure-attribute version mismatch, see Completed; this remains an
  untriaged latent risk.)
- `environ_GetProfileInt(...,-1)` (messages/atkams, several sites) and
  `cwp_Search(...,-1,...)` (ams/delivery) — likely safe (looks like a
  plain `int`-returning function, not virtual dispatch) but unverified;
  deprioritized since `messages`/AMS revival is long-term, not part of
  the active inset sweep.
- `tlex_RecentPosition(...,-1 or -2,...)` (ness) — moot until the `ness`
  bison grammar extension blocker is resolved; the code doesn't run yet.

---

## Historical patches audit (`patches/`) — complete, 2026-07-04

CMU's official 6.3.x point-release patches plus community/site contrib
patches from the 1990s live in `patches/official/` and `patches/contrib/`
(the same set is duplicated verbatim under `andrew-6.4/patches/`,
`trunk/patches/`, and the top-level `AUIS/patches/` — they're identical,
no need to check more than one copy). This was triggered by finding that
CMU had already patched the exact figure-attribute-version bug hit while
fixing `95Summer.ez` (see Completed, above) — worth checking here *before*
deep-diving into a new bug, since CMU or a site admin may have already
found and fixed it decades ago. Every file every patch touches was
diffed against current source (not just the patch descriptions) to
determine actual relevance.

**Applied:**
- `official/patch.633` — "make figure accept figures created with later
  versions, including C++ 7.2+." Makes `figattr__Read` tolerate unknown
  attribute names instead of returning `dataobject_BADFORMAT`. Applied
  2026-07-04 as part of the `95Summer.ez` figure fix (see Completed).
- `contrib/malloc.ci.auis6.3.diff` — `overhead/malloc/malloc.ci`
  `addarena`: `A.arenaend - A.arenastart` is `struct freehdr *`
  subtraction, which the C standard defines in units of
  `sizeof(struct freehdr)`, not bytes — undercounts the arena-growth
  heuristic by ~20-24x. Fixed by casting both to `char *` before
  subtracting. **No observable runtime effect**: `ANDREW_MALLOC_ENV` is
  `#undef`'d in `config/site.h` and there is no `malloc.o`/`libmalloc.a`
  anywhere in `build/` — this codebase runs on system `malloc` via libc,
  Andrew's custom allocator is dead code here. Fixed anyway for source
  correctness (submitted originally by the current user, `wdc@mit.edu`,
  in 1995) in case `ANDREW_MALLOC_ENV` is ever revisited. `malloc.ci` is
  `#include`d into `malloc.c`/`pmalloc.c`, which currently fail to
  compile for unrelated pre-existing reasons (`AbortFullMessage`
  static/non-static prototype conflict) — out of scope, this directory
  isn't part of the active build either way.

**Investigated, found already fixed in our 6.3.1 baseline** (no action
needed — but worth knowing these bug patterns were already closed before
this project started, so don't waste time rediscovering them):
- `official/patch.631`, `atk/figure/figospli.c` hunk — guards
  `ctemp[last+1].t4 /= ctemp[last+1].t2` division (spline math for
  polyline figures) with `if (last >= 0)` to avoid an uninitialized-value
  divide on degenerate (too-few-point) curves. Already present at both
  call sites (lines ~148, ~210).
- `official/patch.631`, `atk/textobjects/unknown.c` (the exact file our
  new `smpltext.c` hardening depends on) — all 3 hunks already applied:
  `self->odata==NULL` typo (comparison instead of assignment, would have
  left a dangling pointer after `text_Destroy`) is correctly `=`; `static
  int tungetc` is correctly `static void`; `unknown__Read` already
  `return`s `ret` at the end.
- `official/patch.631`, `atk/support/hash.c` hunk — use-after-free fix
  (`return egg->value` after `free(egg)`) already applied; value is
  copied to a local before the free.
- `official/patch.631`, `atk/text/tabs.c` hunk — `PrevTab >= 0` bounds
  check already extended to `PrevTab >= 0 && PrevTab < self->number`.

**Not applicable:**
- `official/patch.631`, `atk/basics/common/im.c` hunk — `#ifdef
  hpux`-only, doesn't affect Darwin.
- `official/patch.632` — AMS `parseadd.c` fix, `official/patch.634` —
  AMS/`eatmail` coredump fix. Both out of scope; AMS/Messages revival is
  long-term (see Medium-term below), not part of the active inset sweep.
- `contrib/symlink.patch` — makes a shared-lib install symlink relative
  instead of absolute in `overhead/class/lib/Imakefile`. Confirmed dead
  code path for us: it's inside `#else /* LIBDL_ENV */`, and Darwin's
  config never defines `LIBDL_ENV` — we build a static `libclass.a`, not
  the shared `libclass.so` this patch targets.
- All SGI/NetBSD/Solaris/HP-UX/RS6000 platform ports (`SGI-port.6.3.3.*`,
  `NetBSD*.README`/`.diffs`, `Solaris*.diffs`/`.README`,
  `patch.631-hp-only`, `dvi2disp.patch`, `mit.patch`) — assembler flags,
  `stty`/`gtty` variants, `mode_t` sizing for Irix, `sys_errlist` typing,
  etc. for platforms unrelated to macOS/arm64.

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
    |        +-- fnote         [PROVEN — Cattey.Writing: footnote marker + popup]
    |        +-- textref / texttag  [PROVEN — ex14/ex14.doc cross-ref page numbers]
    |        +-- rofftext             (extends text)
    |        +-- srctext      [PROVEN — indentation + syntax coloring confirmed]
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
    +-- figure -- figview             [PROVEN — 95Summer.ez renders correctly]
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

| # | Inset / App | Test document | What it proves | Search string |
|---|-------------|--------------|----------------|---------------|
| 1 | fnote | **[PROVEN]** `PAPERS/atk/Cattey.Writing` | inline text-in-text insets | look for superscript footnote markers in body text; click to expand |
| 2 | **textref / texttag** | **[PROVEN]** `src/atk/examples/ex14/ex14.doc` + `ex15/ex15.doc` | cross-ref insets; page-number references that update dynamically | in ex14: search "Program Listing for Example 14 at the end of this section on p." — the number after "p." is a live textref pointing to the texttag at the listing |
| 3 | **eq** | `ia-archive/dec.91` | **[PROVEN]** equation editor; text marks confirmed | look for rendered equations with fractions and subscripts |
| 4 | **table** | `ia-archive/aug.90` | **[PROVEN]** cell text visible | spreadsheet cells with numbers and formulas |
| 5 | value (slider/button) | `ia-archive/sep.90` or `ia-archive/jan.90` | value views; valueview dispatch chain | slider or button widgets embedded in text |
| 6 | lset | `ia-archive/nov.91` or `ia-archive/jan.90` | scrollable list widget | scrollable selection list inset |
| 7 | pushbutton / link | `PAPERS/conf/1995/widgets.ez` | hyplink chain: pushbutton→link→linkview | clickable button insets |
| 8 | **fad** | `src/atk/adew/Title.doc` | **[PROVEN]** LP64 fix complete | animated/fading text title inset |
| 9 | org | `src/atk/org/example1.org` | outliner; proves apt→aptv base | outline nodes with expand/collapse |
| 10 | rofftext | `bin/rofftext -d <manpage>` | roff formatter on top of text | formatted man page output |
| 11 | chart | `build/doc/atk/classes.org` or synthesize | proves apt branch independently of org | bar or line chart inset |
| 12 | **cel / adew** | `src/atk/adew/vallist` | **[PROVEN via Sherman.Alloc]** ADEW stack: value+text+cel+arbiter renders | spreadsheet cells with live calculation |
| 13 | typescript | `bin/typescript -d` | terminal emulator; **crashes "Can't connect subchannel"** — likely macOS PTY compat issue, not LP64 | terminal window inset |
| 14 | bush | `bin/bush -d` | shell application | interactive shell |
| 15 | **figure** | `NEWSLETTERS/EZ/95Summer.ez` | **[PROVEN]** root cause was version-skew in figure attributes + a swallowed Read error in `simpletext`, not LP64; fixed 2026-07-04 | drawing/diagram insets in newsletter |
| 16 | **Sherman.Alloc** | `PAPERS/atk/Sherman.Alloc` | **[PROVEN]** text+eq+fad+cel/arbiter all render; zip unsupported (expected) | multi-inset compound document |

**No good test document exists for:** `lookz`, `launchapp`, `prefed`
— these need synthetic test files or targeted app launches.
`srctext` is now [PROVEN]; `Cattey.Writing` contains srctext insets (source code blocks).

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

## Regression test checklists

### help application
Run: `DISPLAY=:0; build/bin/runapp helpa -d`

1. **Startup**: window opens showing "A Guided Tour of Andrew" in the main panel
2. **Overviews pane**: right panel top section shows entries (Andrew Tour, Multimedia, Mail, Programming); text is readable
3. **Programs pane**: right panel bottom section shows programs list starting from the **top** of the list (first entry visible, scrollbar at top) — regression for LP64 frameDot bug
4. **Overviews link**: click "Andrew Tour" in Overviews → main panel changes to that document
5. **Programs link**: click any entry in Programs → main panel changes to that help topic
6. **In-text links**: click a cross-reference link in the main panel text → navigates to linked topic
7. **Expand/Shrink Programs pane**: use Panels menu → "Expand Programs" / "Shrink Programs" → pane resizes correctly
8. **Show History**: use Panels menu → "Show History" → history pane appears listing previously visited topics
9. **History links**: click an entry in the History pane → main panel navigates to that topic

---

## Active (instances running)

- **Xft phase 2: menu rendering** — deprioritized; menus are working
  acceptably with the current rendering path

---

## Near-term

### ~~LP64 positioning sweep~~ — complete
All five LP64 variant classes identified, swept, and committed. `Sherman.Alloc`
and `95Summer.ez` used as integration tests; both render correctly.

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

### ~~Integration test: `Sherman.Alloc`~~ — proven
All insets in `Sherman.Alloc` render correctly (fad, cel, arbiter, eq, table);
zip unsupported as expected. Multi-inset compound documents confirmed working.

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
