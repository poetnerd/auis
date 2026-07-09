# AUIS Revival Roadmap

Current as of 2026-07-07. See `porting-changelog.md` for the detailed
history behind each completed item.

---

## Current action plan

### Objective: messages demo a.k.a.amsdemo

- ~~Prerequisite: cui~~ — done 2026-07-07, was a missing RESOLVER_LIB link flag, not sgtty
- ~~gendemo~~ — done 2026-07-07: cwd bug (needs to run from `src/ams/demo/`)
  plus a `cui recon` segfault (modern-flex init-flag polarity bug, new bug
  class, see `porting-assessment.md` §13); both fixed, demo folder populates
  and reconstructs cleanly
- Then find the bugs in the demos. **In progress:** a second, unrelated,
  intermittent crash already turned up in the date-header parser
  (`prsdate.c`/`parser_Parse`, `memmove` overrun) — not yet root-caused.
   - Inter-line spaceing in folders and message header panes is double wide.
   - Timestamps are incorrect affects display and message ordering. "7-Jul-126" isn't right.

### Objective: Reliable operation

- Let's get all the function prototypes live with ANSI — plan of record
  now at Medium-term → ANSI C conversion (M1–M4)

---

## Little Annoyances to clean up

### Keymap:

- The arrow keys don't work yet
- We don't have a "Meta" key active yet

---

## Questions

- How do I get a right click with my magic pad?
- How do I get drag scroll working without having to click and drag the elevator?

---

## Major milestones

- **2026-07-05**: `messages` application running with local mail store —
  "mail (Private BB; 0 new of 0)" confirmed in the folder panel. All three
  prerequisite streams closed simultaneously: AMS local backend alive,
  atkams/ interface working, ATK insets (lset, value, pushbutton/link)
  sufficient for the messages UI to render.

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
- Figure insets: **proven working** — `95Summer.ez`'s figure renders correctly end to end (see Chronological log 2026-07-04). Two independent bugs stacked on top of each other: (1) parser-desync — official CMU `patch.633` (figattr tolerates unknown attributes from later format versions) plus new `smpltext.c` hardening (failed inset reads fall back to a raw `unknown` object instead of corrupting the rest of the parse); (2) LP64 — `figure__Read`'s `$origin` line was parsed with `sscanf(buf, "$origin %d %d", &val1, &val2)` into `long val1, val2`, the same %d-into-long pattern as the other LP64 audits, leaving stack garbage in the upper 32 bits of `originx`. That corrupted `originx` flows straight into `figview`'s `panx` (`SetDataObject` does `panx = originx`), pushing the entire figure's rendering ~4 billion pixels off-screen — content was being drawn, just nowhere near the visible clip region. Confirmed live via `lldb`: found `originx == 0x100000000` in the running `figview`'s memory. Fixed both `figure__Read` and the analogous `figure__ReadPartial`.
- `Sherman.Alloc` integration test: text, eq, fad, cel/arbiter spreadsheet
  all render; zip unsupported (expected); calc engine presumed working if
  cel displays correctly
- Xft phase 1: body text rendering via client-side Xft (anti-aliased)
- Andy symbol fonts (`symba*.pcf`) built and installed in `build/X11fonts/`
- `overhead/malloc/malloc.ci` `addarena` arena-size pointer-arithmetic bug fixed (`patches/contrib/malloc.ci.auis6.3.diff`, 2026-07-04) — source-correctness only, see Historical patches audit below for why it has no runtime effect here
- `messages` application: **running** (2026-07-05) — local mail store (`Private BB`) visible in folder panel; three-pane layout, menus, and help text all rendering; AMS local backend + atkams/ interface + ATK insets all working

**LP64 bug classes identified and swept:**
- Variant 1: Missing prototypes / pointer return truncation (23 sites)
- Variant 2: >8-arg untyped dispatch stack spill (classpp fix)
- Variant 3: `int` constant zero-extended through untyped dispatch
  (`observable_OBJECTDESTROYED`, `value_OBJECTDESTROYED`,
  `class_VERSIONNOTKNOWN`)
- Variant 4: `%d` with `long *` in scanf family (full tree audit done; one straggler found later and fixed — `figure.c`'s `$origin` parsing, see Completed and Chronological log 2026-07-04)
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

contrib
    +-- writestamp [PROVEN]
    +-- calc
```

Note: `help` does not use `lset` — confirmed by source audit. `lset` is
unproven and requires its own test.

---

## Inset testing sequence

Ordered by dependency depth; each step proves a layer the next relies on.
`[PROVEN]` items are already confirmed; start from the first unconfirmed entry.

| # | Inset / App | Test document | What it proves | Search string |
|---|-------------|--------------|----------------|---------------|
| 1 | **fnote** | **[PROVEN]** `PAPERS/atk/Cattey.Writing` | inline text-in-text insets | look for superscript footnote markers in body text; click to expand |
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
| 15 | **figure** | `NEWSLETTERS/EZ/95Summer.ez` | **[PROVEN]** two stacked bugs fixed: parser desync (patch.633 + smpltext.c) and LP64 `$origin` scanf corruption (figure.c); renders correctly end to end | drawing/diagram insets in newsletter |
| 16 | **Sherman.Alloc** | `PAPERS/atk/Sherman.Alloc` | **[PROVEN]** text+eq+fad+cel/arbiter all render; zip unsupported (expected); calc inset from contrib not yet working | multi-inset compound document |
| 17 | **Cattey.Writing** | `PAPERS/atk/Cattey.Writing` | **[PROVEN]** writestamp, fnote, raster, |

**No good test document exists for:** `lookz`, `launchapp`, `prefed`
— these need synthetic test files or targeted app launches.
`srctext` is now [PROVEN].

**Known non-starters:** `ness` (bison extension), `zip` (unsupported),
`clock`/`calc`/`timeoday` (contrib, lower priority).

---

## Heisenbugs (intermittent, not currently reproducible)

### Display mess

**Observed:** On rare occasions, text will draw into the menu area. 
But I can't reproduce it reliably.

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

### Xlib display-lock self-deadlock — **deferred** (reproduced 2026-07-04, root cause identified)

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

### ~~Messages application prerequisites~~ — **DONE (2026-07-05)**

Goal: get `messages` running with a local mail store. Three streams of work:

**Stream 1 — remaining ATK inset prerequisites (unproven):**
These insets appear in the messages UI and/or in rendered mail:
- `lset` (scrollable list) — mail folder/message list display; test with `ia-archive/nov.91` or `jan.90`
- `value` / `valueview` (slider, button) — UI controls; test with `ia-archive/sep.90` or `jan.90`
- `pushbutton` / `link` / `linkview` — hyplink navigation; test with `PAPERS/conf/1995/widgets.ez`

**Stream 2 — AMS local mail store: found, 2026-07-04.** The build already
separates the message store/UI from the AMDS delivery daemon via independent
Imake flags:

| Flag | Gates | Needed for local-store `messages`? |
|---|---|---|
| `AMS_ENV` | whether `ams/`/`atkams/` build at all (`Imakefile:37-39`), plus `overhead/mail` (`libmail.a`, `libcparser.a`) | **yes** — currently `#undef`'d in `config/site.h`, overriding `allsys.h`'s default of 1 |
| `AMS_DELIVERY_ENV` | `ams/delivery/` (sendmail/vicemail/queuem/trymail) and `ams/utils/` (nntp/muserver/purge/reauth/undigest) — the actual AMDS transport | no — leave off |
| `SNAP_ENV` | networked "remote message server" variant (`ams/ms`, `libcuis.a`) vs. the local `nosnap` path (`libcuin.a`) | no — leave off, local path is the default |
| `WHITEPAGES_ENV` | `overhead/wpi`/`wputil`; auto-forced only by `AFS_ENV` or `AMS_DELIVERY_ENV` | no — stays off since neither of those is on |

With only `AMS_ENV` on, `atkams/messages/lib`'s Imakefile builds `amsn.do`
against `NLIBS` (`libcui.a`, `libcuin.a`, `libmssrv.a`, `libeli.a`,
`librxp.a`) — the local, non-networked message store — and never touches
delivery or white-pages code.

The local-mailbox backend the user remembers is real and already in the
source: `ams/libs/ms/newmail.c`'s `ProcessNewMail(..., PROCESSNEW_MBOX, ...)`
imports from a plain mailbox file returned by `GetPersonalMailbox()` in
`ams/libs/ms/findmbox.c` — a standard Unix mbox (`~/mailbox`, or the
`mailboxdir` profile override), with no AFS/AMDS/white-pages involved. This
is almost certainly the exact seam the user's fetchmail fork fed into at
MIT Athena. `AFS_ENV`/`WHITEPAGES_ENV` references in `ams/libs/ms/mswp.c`,
`init.c`, and `atkams/messages/lib/{ams,stubs}.c` are all `#ifdef`-optional
(`mswp.c:980` even has an explicit `#ifndef WHITEPAGES_ENV` fallback path),
confirming this is a first-class supported configuration, not a hack.

**Next action:** ~~flip `#undef AMS_ENV` → `#define AMS_ENV 1` in
`config/site.h`~~ — **done, 2026-07-05.** `AMS_ENV`/`CONTRIB_ENV` are on in
`config/site.h`. All newly-exposed subtrees (`atkams/messages/lib`,
`overhead/mail`, `overhead/eli`, `ams/libs/cui`, `ams/libs/ms`,
`contrib/srctext`/`eatmail`/`time`) fixed and verified compiling/linking
clean per-directory. User's first full top-level `make dependInstall`
(no `-k`) surfaced exactly 2 more link-time errors (`nns`'s `getla()`
needing `getloadavg()` instead of dead `/dev/kmem`+`nlist()`, and a
missing `${RESOLVER_LIB}` on `nns`'s link line) — both fixed same-day,
full details in `porting-changelog.md`'s 2026-07-05 entry. `ams/msclients/vui`
and `contrib/tm` — curses terminal clients on the removed BSD `sgtty` API —
remain conditionalized out of the build (`MK_VUI`/`MK_TM`) rather than fixed;
not needed for the GUI `messages` path. Rationale in `porting-assessment.md`
§7a. `ams/msclients/cui` was originally grouped with them but didn't actually
share that dependency — see the 2026-07-07 fix below.

First runtime test of `messages` (2026-07-05) segfaulted: `EXC_BAD_ACCESS`
in `_platform_strlen` via `mailconf.c`'s `CkAMSCellConfig` (`AndrewDir`/
`LocalDir` called with no prototype in scope — same LP64 #1 pattern fixed
at 23 sites on 2026-06-30, recurring because `overhead/mail` was never
built/audited before `AMS_ENV` went on). Fixed `mailconf.c` plus a sweep
of 5 more active files with the same bare-call pattern (`ams/libs/ms/init.c`,
`hdlnew.c`; `atkams/messages/lib/stubs.c`; `overhead/mail/metamail/metamail/{metamail,mailto}.c`;
`overhead/eli/lib/prims1.c`); all rebuilt clean. Full details in
`porting-changelog.md`. Separately, `contrib/bdffont` turned out to be
unbuildable (missing `bdfparse.act`, no generator, no fossil history) and
was conditionalized out (`MK_BDFFONT`); see `porting-assessment.md` §7b.
**✓ MILESTONE (2026-07-05):** `messages` is running. "mail (Private BB; 0 new
of 0)" confirmed in the folder panel. Three-pane layout, menu bar, and help
text all rendering correctly.

**Follow-up (2026-07-05): same-tip rebuild on host `spoon` segfaulted immediately.**
Real `SIGSEGV` (confirmed via `lldb`, not a codesigning/kernel-kill artifact),
in `CUI_GetHeaders`'s header-scanning loop. Two `long`/`int` mismatches in
`CUI_GetHeaders`'s `startbyte`/`nbytes`/`status` params, both invisible at
compile time (no prototype in scope at the mismatched boundaries) — same
LP64 Variant 5 family as `lpair__Init`/`style__SetNewIndentation`, but this
time crossing a plain unprototyped C call, not just the `void (*)()` vtable
macros. `ms/libs/ms/headers.c`'s `MS_HeadersSince` (`int`/`int *`) was the
outlier; `cuilib.c`'s `CUI_GetHeaders` and the dormant SNAP variant
(`cuisnap.c`) already agreed on `long`/`long *`. Fixed end-to-end
(`headers.c`, `ams.ch`/`amsn.ch`/`amss.ch`, `ams.c`/`amsn.c`/`amss.c`,
`capaux.c`, `foldaux.c`, dormant `ams/ms/ms.c`); rebuilt clean. Full detail
in `porting-assessment.md` §12 and `porting-changelog.md`. **Next up:**
retest on `spoon`.

**Stream 3 — atkams/ interface audit: resolved by the Stream 2 survey.**
`ams/Imakefile` and `atkams/messages/lib/Imakefile` show the boundary is
exactly the Imake flags above — `messages` links against the local
`libmssrv.a`/`libcuin.a` regardless of whether AMDS is present. No IMAP
adapter is needed for the local-store path; that fallback remains available
later if the mbox approach hits a wall.

**Contrib objects:** `CONTRIB_ENV` on brings in `calc demos gestures wpedit
time eatmail mit srctext` (see `contrib/Imakefile`); `tm` and `bdffont`
deferred (§ above). Still TBD whether any of these besides `srctext`
(already a proven inset, see Completed) matter for the messages path
specifically.

### gendemo — done; demo folder populates and reconstructs cleanly

**✓ (2026-07-07)** `cui` builds/links/installs — the blocker was a missing
`${RESOLVER_LIB}` link flag on its Imakefile (same bug class as `nns`'s
2026-07-05 fix), not the BSD `sgtty` API as originally assumed. `cui` doesn't
use curses at all, and its one sgtty reference was already dead code
(`POSIX_ENV` is unconditionally on for darwin). Full detail in
`porting-changelog.md`'s 2026-07-07 entry.

**✓ (2026-07-07)** `gendemo` itself has two independent bugs, both fixed:

1. It reads its 23 demo posts (`d1`/`d1.heads` ... `d23`/`d23.heads`)
   relative to the current directory, not `$ANDREWDIR` — those files live
   only in `src/ams/demo/` and are never installed. Must `cd src/ams/demo`
   before invoking it (`ANDREWDIR` is only used to find `cui`/`arpadate`).
2. With cwd fixed, `cui`'s final `recon` step segfaulted on the very first
   address caption it tried to build. Root cause: a **new bug class** — flex
   regenerates `overhead/mail/lib/parsel.c` at build time (no fossil
   history), and modern flex inverted the meaning of an internal init flag
   that a hand-written `pareset_lexer()` was poking directly instead of
   using flex's real public API. Fixed (`yyrestart(yyin)`); swept the whole
   tree and found/fixed one sibling instance (`overhead/eli/lib/elil.flex`,
   the ELI/FLAMES filter-language lexer — pre-emptive, no confirmed crash
   yet). Full writeup in `porting-assessment.md` §13, session detail in
   `porting-changelog.md`'s 2026-07-07 entry. Verified: `recon` completes
   ("Reconstructed folder ~/.MESSAGES/amsdemo with twenty-three entries")
   repeatably, no crash.

**Next up — two threads:**

1. **New, unrelated intermittent crash found while re-verifying `recon`:**
   `parsedate` → `parser_Parse` → `memmove` heap overrun building the date
   field (`BuildDateField`/`MS_ReconstructDirectory`). Bison-generated
   (`ams/libs/ms/prsdate.c` from `prsdate.gra`, also untracked in fossil) —
   different generator, different subsystem, data-dependent rather than a
   fixed polarity bug. Not yet root-caused; needs its own lldb session.
2. Once `recon` is fully stable, verify `messages` can actually browse and
   read the populated `amsdemo` folder end-to-end (captions, dates, bodies).

### IMAP / AMS backend investigation (week of 2026-07-14)

With `messages` running against the local mbox backend, investigate whether
an IMAP adapter behind `atkams/` is viable for connecting to a live mail
server. The `atkams/`–`ams/` boundary is already audited (see Stream 3 above);
this is the next architectural step toward real-world mail use.

### printf/fprintf %d/%ld audit
2,597 printf-family hits with `long` values and `%d` format specifiers
logged during the scanf audit. These produce wrong output for large values
but do not corrupt memory. Address as a batch; not blocking messages work.

### Xft phase 2 (deferred)
Menu text rendering via Xft. Menus are currently acceptable without it.
Only remaining X core font path dependency is Andy symbol and cursor fonts.

### Update quickstart.md
Remove resolved known-issues entries as each fix lands.

### Not current focus
- `typescript` "Can't connect subchannel" crash — macOS PTY compat issue, not LP64; defer
- `bush` shell application — defer until after messages
- `org`, `chart`, `launchapp` — defer

---

## Medium-term

### ANSI C conversion (M1–M4) — plan of record, assessed 2026-07-08

Absorbs the former "Prototype sweep" and "Classpp typed dispatch" entries
here plus the long-term "ANSI C modernization" entry into one ordered
plan. Analysis — June mass-conversion postmortem, keystone finding, tool
verdicts, delegation guardrails — in `porting-assessment.md` §14.

Keystone: `.ch` files already carry full ANSI method signatures; classpp
parses and discards them. Emitting them (M1) type-checks every method
call site and definition tree-wide *before* any mass file editing starts
— the compiler becomes the auditor instead of grep.

- **M1 — classpp emits types.** Typed prototypes in `.eh` (today:
  `long text__Read();`) and typed casts in all `.ih` dispatch macros
  (extends the 2026-06-30 ≥9-arg fix to every method). Kills LP64
  Variants 2/3/5 structurally and catches signature drift (the
  `CUI_GetHeaders` class of bug) at compile time. The machinery
  already exists in classpp (`usePrototypesImport`/`Export`, `-p`,
  `-D` signature/`.desc` files, `$(CLASSFLAGS)` hook in andrew.rls —
  see porting-assessment §14 "M1 mechanics"); the code change is just
  splitting `-p` into `-pi`/`-pe` and dropping the `>= 8` gates behind
  `-pi`, defaults untouched. The real work is the per-directory
  rollout — see "M1 rollout points" below.
- **M2 — Prototype sweep.** `-Werror=implicit-function-declaration`
  subtree-by-subtree (`src/config/darwin/system.mcr` COMPILERFLAGS);
  fix by adding `#include`s or `extern` declarations at call sites.
  Closes Variant 1 permanently — it has cost debugging time on every
  subtree activation so far.
- **M3 — Definition conversion.** New `ansify` pipeline
  (porting-assessment §14: static-fix tools → class methods by
  signature-DB lookup → cproto for file-local functions → per-file
  compile gate), one subtree per commit, dependency order: overhead →
  atk/basics+support → atk/text → insets → apps → atkams/ams → contrib.
  Ratchet each completed subtree from `-Wno-*` to
  `-Werror=implicit-int,strict-prototypes,int-conversion,incompatible-function-pointer-types`.
- **M4 — Global strictness.** Tree-wide `-Werror` on the type-safety
  set; `-Wformat` then catches any remaining scanf `%d`/`%ld` (Variant
  4) automatically. Keep `-std=gnu89` until conversion completes;
  consider c99 after. Writable-strings stays deferred.

Scale: ~13,700 K&R definitions across ~1,301 of 1,544 `.c` files; ~5,100
are class methods converted by `.ch` lookup, not inference. M2/M3 runs
are delegable (Sonnet-class) under the §14 guardrails; M1 and
`.ch`-vs-`.c` signature disagreements stay top-level.

#### M1 rollout points (Import half: `CLASSFLAGS = -pi` per directory)

Rollout state lives only in committed Imakefiles; classpp defaults
never change until step 10. Per-step rhythm: set `CLASSFLAGS` → force
regen (delete the directory's generated `.ih`/`.eh` or touch its
`.ch`s) → `make Clean; make dependInstall` → fix consumer fallout →
runtime spot-check → commit. Clean build passing is the definition of
done. Ordering is by external-consumer count (survey 2026-07-08,
porting-assessment §14), not directory nesting: pilots on
zero-consumer leaves, then the core, largest last.

1. [ ] classpp: `-pi`/`-pe` split, `>= 8` gates dropped under `-pi`
2. [ ] Pilot A — `atk/eq` (2 classes, 0 external consumers; runtime
       check: eq inset in `Sherman.Alloc`)
3. [ ] Pilot B — `atk/figure` (17 classes, 0 external; runtime check:
       `95Summer.ez` figure; recently debugged, well understood)
4. [ ] First cross-directory step — `atk/raster/lib` (7 classes, 17
       external includers; runtime check: raster insets)
5. [ ] `atk/frame` (5 classes, 95 external)
6. [ ] `atk/supportviews` (17 classes, 178 external)
7. [ ] `atk/text` (21 classes, 321 external)
8. [ ] `atk/support` (19 classes, 450 external)
9. [ ] `atk/basics/common` (41 classes, 2,351 external — `im`, `view`,
       `fontdesc`, `environ`, `message`, `proctbl`, `menulist`,
       `graphic`, `dataobj`, `keymap`; the LP64 Variant-3/5 epicenter.
       Largest burndown, largest payoff: every consumer tree-wide is
       protected once this lands)
10. [ ] Breadth: remaining atk (`value`, `adew`, `apt`, `basics/wm`,
       `basics/x`, `hyplink`, `syntax/parse`, ...), then `atkams`/
       `ams`, `contrib` (`zip/lib` first), `examples` — delegable
       batches
11. [ ] Flip classpp default: Import-all becomes compiled-in, delete
       the per-directory `-pi` flags (single mechanical commit)
12. [ ] Export (`-pe`) is *not* sequenced here — it rides with each
       subtree's M3 conversion, since its blast radius is only the
       implementing directory

Steps 2–4 are top-level work (learning the fix patterns); 5–10 are
increasingly delegable once the patterns are documented.

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

### ANSI C modernization (full K&R conversion)

**Elevated to medium-term** — see Medium-term → ANSI C conversion
(M1–M4). The `modernize` tool is no longer the starting point; see
`porting-assessment.md` §14 tool verdicts.

### Messages application

**Elevated to near-term focus** — see Near-term section for the active
work plan. Moved here for architectural notes only.

`messages` is the AUIS mail/bulletin-board client. Full AMS revival is
off the table — the AFS/shared-filesystem delivery model is a dead end.
See Near-term → Messages prerequisites for the two viable backend paths
(local store vs. IMAP adapter).

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
