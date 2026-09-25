# AUIS Revival Roadmap

Last updated: 2026-09-16

This document is intended primarily for AUIS revival participants,
with a summary of what's running, what's active, what's next, and the
high level rationale for the various tradeoffs made.

You can make a deep dive into the details in the documents below:

| Doc | Description |
|---|---|
| `revival.md` | What's been accomplished, told as a narrative, for readers who already know AUIS or are meeting it for the first time |
| `porting-assessment.md` | Bug-class root-cause analysis and the full ANSI C conversion plan |
| `porting-changelog.md` | Dated log of every fix |
| `claude-history/README.md` | Index of retired working docs — task prompts, investigation write-ups, rollout runbooks, and this document's full pre-2026-08-07 history (`roadmap-old.md`) |
| `quickstart.md` | Build and run instructions |
| `mail-quickstart.md` | IMAP/SMTP mail setup |

---

## Status

`ez`, `help`, and `messages` all run. The codebase has been through a
multi-month modernization effort — what that involved and why is in
Key tradeoffs, below — which is now done for its original scope. Most
applications and insets work well; the table below tracks what's
solid versus still rough. Active work is listed under Projects.

---

## Applications and insets

| Component | Status | Notes |
|---|---|---|
| `ez`, `help` | Fully working | |
| `bush`, `org`, `chart`, `layout` | Fully working | |
| `messages` | Mostly working | Runs against a local mailbox and against a live IMAP mirror, but the IMAP side still needs a manual `imapsync` step — see Projects → AMS over IMAP/SMTP |
| `typescript` | Broken | Crashes on launch: PTY allocation fails, and a missing NULL check turns that failure into a crash instead of a clean error |
| `ness` (scripting) | Inert | Blocked on a bison grammar extension specific to the (disabled-on-arm64) Andrew bison fork |
| Core embedded insets — `eq`, `table`, `fad`, `fnote`, `bp`, `srctext`, `figure`, `value`/slider/button, `pushbutton`/`link`, `lset`, `org`, `apt`/`cel`/`arbiter` | Fully working | `figure` has one open cosmetic bug: italic-text sizing is non-monotonic (10pt renders bigger than 12pt). `eq` has one open bug: deleting a multi-line equation's lines only reduces each to a single "zilch" placeholder rather than merging down to one line — a long-standing bug, not new to this port (see `porting-assessment.md` §23). `arbiter`: a complex arb demo (an embedded piano keyboard) had a 35-year `short`-overflow (fixed 2026-08-21) plus an empty-spacer sizing bug and an architectural mismatch between old 1988 layout data and the corrected 2026-08-18 stacked-split summing behavior, both fixed 2026-09-01/03 — renders correctly now, no known open issues (see `revival.md` → "Old bugs never found till now") |
| `calc`, `zip` (contrib) | Fully working | |
| `clock` | Fully working | |
| `raster` (as an embedded inset) | Fully working | |
| `contentv` (Table of Contents) | Fully working | An earlier report of it ignoring enumerated headings was a false alarm — root cause was input focus being inside an embedded inset rather than the document itself when the ToC view was opened |
| `convertraster` (standalone CLI) | Fully working | Fully tested 2026-08-08; three bugs found and fixed (see `porting-changelog.md`) |
| `image` (GIF/JPEG/TIFF/PNG import) | Fully working | Fixed 2026-08-08: TIFF import was totally broken (four LP64 struct/stride bugs in vendored `libtiff`); JPEG/TIFF solid-color render was an unrelated `xgraphic.c` variable mixup — see `porting-changelog.md`. GIF import shares the same render path so is likely also fixed, but wasn't retested. **PNG support added 2026-08-19** (`src/atk/basics/common/png.c`/`png.ch`, decode-only, hand-rolled chunk/filter parsing over system zlib — no vendored libpng) — a genuinely new format for this codebase, not a bugfix; wired into the image inset's Import menu and into HTML-mail image dispatch (see "HTML mail rendering" below). Verified against synthetic and real-world mail images, live in both `ez` and `messages` |
| `htmlview` | N/A — superseded | The standalone viewer itself hasn't changed; real-world HTML mail rendering now goes through a separate, purpose-built parser+renderer wired into `messages`/`cui` instead — see Projects → HTML mail rendering. Retargeting `htmlview` onto that same parser is an optional later step, not yet done |

---

## Projects

Active work. Each entry starts with a short description and next step;
details will fill in as each project actually unfolds, rather than
being front-loaded here.

### AMS over IMAP/SMTP

- **Description:** Local-mailbox AMS talking to a real IMAP/SMTP
  server through a one-way mirror. Read, browse, send, and writeback
  all work.
- **Next step:** The mirror integration is still manual and clunky
  (a separate `imapsync` step) — thread it into the normal workflow.
  After that, XOAUTH2 auth.

### HTML mail rendering

- **Design doc:** `html-mail-rendering-design.md` — sanitization
  allowlist, table strategy (see below — pivoted mid-project), image
  strategy (`image__ReadOtherFormat` capability check + fallback,
  including PNG — see below), and the explicit renderer-level fallback
  contract. Work happens on the `html` branch (`~/src/AUIS/html/`),
  not `trunk/`.
- **Status: core rendering done, verified live against real mail.**
  MIME body decoding was already solid (see `porting-changelog.md`'s
  2026-08-09/2026-08-10 entries); the actual rendering pipeline is now
  built and wired in, in three stages, mirroring how `mimepart.c`
  already sits outside ATK so both `cui` and `messages` can share it:
  1. **Parser** (`htmlpart.c`, 2026-08-15) — ATK-independent
     tokenizer + sanitization-allowlist tree builder. 20/20 tests.
  2. **Plain-text renderer** (`htmltext.c`, 2026-08-15) — walks the
     parse tree to text; what `cui` always uses. 13/13 tests.
  3. **ATK-styled renderer** (`htmlatk.c`, 2026-08-16) — real clickable
     links, inline images, styled text; wired into `text822.c`/`cui`
     the same day, retiring the old `mimepart_HtmlToText` shim.
  Midway through Stage 3, the originally planned table strategy
  (Andrew's `table`/`spread` grid object) turned out not to fit real
  mail: it's a fixed-pixel grid that doesn't reflow with window width,
  the way marketing HTML's near-universal wrapper-table boilerplate
  needs. Pivoted to `lset`/`lpair` instead (percentage-based, does
  reflow), plus a "peeling" heuristic that collapses that wrapper
  boilerplate down to real content — see the design doc's "Table
  strategy" section for the full reasoning and known imperfections.
  That pivot surfaced (and fixed) a real core-ATK bug along the way —
  `BackSpace`'s end-of-file line padding could infinite-loop `^V`
  scrolling against a disproportionately tall embedded view — plus,
  today, two more real-mail rendering bugs (isolated blank spacer rows
  never coalescing, hidden `display:none` preheader text rendering as
  visible body text), both found live against a real National Grid
  message (now saved at `revival/tests/national-grid.html`) and fixed.
  **PNG images now render inline, 2026-08-19** — the `image` inset
  itself gained a real PNG decoder (see the `image` row above), and
  `htmlatk.c`/`text822.c`'s image dispatch was updated to route
  `image/png` to it (previously fell through to `raster`, which
  can't decode PNG, so real mail's PNG logos/graphics — the single
  most common inline-image format — always showed as a text
  placeholder). Confirmed live against real-world mail images.
  **Link clicks now work, 2026-09-16** — a `Hit()` override
  (`htmlatk_HandleLinkHit`, `htmlatk.c`) shared by `t822view`
  (message body) and the new `htmllinkview` class (table-cell
  leaves, `htmllinkv.ch`/`.c`) makes left-click launch a link's URL
  and right-click copy it to the X cut buffer. Covers plain text
  links (top-level and nested in tables) and `<a href><img></a>`
  image links alike; the image case needed its own fix — core ATK's
  `imagev` requests input focus on click, which swallows the
  matching button-release, so that path acts on mouse-down instead
  of mouse-up (see `porting-assessment.md` for the full mechanism).
  Confirmed live against all five National Grid links (three footer
  links, one body link, one header wordmark) and all nine of its
  image-wrapped links.
  **`cid:` embedded-image resolution now works, 2026-09-17** —
  `mimepart.c` now parses each MIME part's `Content-ID` header
  (`struct mimepart`'s new `contentid` field, stripped of `<>` and
  whitespace) and exposes `mimepart_FindByContentID()` to look one up;
  `text822.c`'s `RenderHtmlPart()` passes its sibling-part scope
  through a new combined resolver (`ResolveImage()`) that tries `cid:`
  first (always, no opt-in — the bytes are already in the message, no
  network involved) and falls back to the existing `http(s)://`
  resolver only when remote loading is on. Confirmed against a real
  inbox message (Microsoft Teams sender-avatar notification: a 32×32
  circular `cid:`-referenced image renders correctly instead of a
  placeholder) — found via `grep -rlia '^Content-ID:' ~/.IMAP | xargs
  grep -lia 'src="cid:'` against the local IMAP mirror, rather than
  a synthetic test message. A synthetic fixture also exists,
  `revival/tests/cid-image-test.eml` (`multipart/related`, one
  embedded 4×4 PNG), for offline/regression use.
- **Open items** (this is the canonical list — add here when found,
  remove when fixed; detailed root-cause writeups live elsewhere and
  are linked below, but status itself lives only here):
  1. Superseded by a correct diagnosis (2026-09-22, `revival/doc/
     html-scroll-plan.md`): a `<table>`'s own `width=` already declares
     intent — `100%`/unset means "flow" (unchanged, still reflows), a
     literal pixel value means the author wants that real width even
     if it doesn't fit, matching Thunderbird. Implemented as two new
     classes, `lsetscrollview`/`lsetscrollcontent`
     (`src/atkams/messages/lib/lsetscrlv.c`/`lsetscrlc.c`), that wrap
     just a pixel-width table's own outermost row in a self-contained
     horizontally-scrollable inset — `lpair.c`/`textv.c`/`drawtxtv.c`
     untouched. `lset.minwidth` (`lset.ch`) carries the bottom-up
     natural width computation `htmlatk.c` needs to decide which
     tables get wrapped. Border/scrollbar chrome show only when a
     table doesn't fit, decided per-`FullUpdate` from real placed
     geometry — this needed two small, generic additions to core
     `scroll.ch`/`.c` (`SetDrawBorder`/`GetDrawBorder`, and a
     `drawborderoverridden` flag so `scroll`'s own lazily-initialized
     `InitPrefs` doesn't clobber an explicit per-instance override on
     an object's first paint), both pure additions that only affect
     instances that call them. bookrack.html's crash-during-page-
     forward (recursing `DesiredSize` into an unlinked nested table,
     landing in real color-allocation code) and a "two frames, one or
     neither clears on navigation" chrome-flicker bug (view_Remove
     teardown passes and the `InitPrefs` clobber above, respectively)
     are both fixed and live-verified. The national-grid.html "large
     block of blank vertical space" bug (2026-09-23) was also fixed:
     root cause was `htmlatk.c`'s `CellMinwidth` folding a purely local
     icon-cell sizing hint (`fixedpx`) into the same signal
     `BuildLsetGrid`'s tableau-floor detection uses, so any row with so
     much as one icon `<td>` got wrongly treated as declaring a
     table-wide pixel design width — crushing unrelated paragraph
     content down to the icon's own width. Fixed by excluding `fixedpx`
     from that composition; `fixedpx` still flows to `BuildLsetChain`'s
     own split-sizing, unchanged, via its own independent field.

     **Fully fixed 2026-09-25, wdc-confirmed working end to end**
     (found immediately after the whitespace bug above, via the same
     live-testing): the document-wide vertical scrollbar's **elevator
     drag** didn't track correctly once a table this large was on-screen
     — root cause was `textv.c`'s `getinfo()`/`position()` treating
     scrollbar position as proportional to **character count**, which
     stops correlating with real pixel position once a single
     "character" (one embedded HTML table) is several thousand pixels
     tall — confirmed generalizable, not HTML-specific: an ordinary
     `.ez` file with a large embedded raster shows the same class of
     symptom via drag. Fixed by giving oversized `containsView` lines a
     proportional weight in the scrollbar's position encoding, derived
     from their real rendered height and the view's current width/font
     metrics — not a document-wide pixel-height rewrite, which stayed
     out of scope as infeasible without defeating `textview`'s lazy
     rendering. Live-verified: dragging reaches genuine top and bottom
     content in national-grid.html, lands correctly in both endzones,
     and legacy `.ez` scrolling (`revival/testing.ez`) is unaffected.
     Plain left/right *click* scrolling was already fixed earlier and
     remains confirmed working throughout this change.

     Five real bugs were found and fixed live during this work: a
     `dictionary_*` string-literal key silently never matching across
     call sites; an early version's flat weight-addition creating a huge
     unreachable "dead zone" in the track; an LP64 `int`/`long`
     truncation in `scroll.c`'s `set_frame()` (positions now routinely
     exceed `INT_MAX`); an out-of-bounds `self->lines[-1]` read in
     `textv.c`'s `DoUpdate` scroll-shortcut when the viewport is filled
     by a single oversized line; and the actual root cause of the
     endzone mis-landing — `setframe()`'s `off`-revalidation was
     unreachable when a drag decoded exactly to the document's end
     (`off` already `0`, so its own correction never ran). See
     `html-scroll-plan.md`'s **Step 3** for the full design and all five
     bugs. `porting-assessment.md` item q.'s trailing note.

     **A further chain of five bugs found and fixed 2026-09-25**,
     live-testing the `lsetscrollview`/`lsetscrollcontent` tableau
     feature against `bookrack.html` (a genuinely nested multi-table
     message, more demanding than `national-grid.html`'s single floor
     table): (a) a crash on first click — `textview__LinkTree`/
     `FinalizeObject` iterated a textview's viewref dictionary and
     dispatched `view_LinkTree`/`Destroy` on every entry, including the
     elevator-drag weight blob the fix above stashed there under its own
     sentinel key, which isn't a viewref at all; fixed by skipping that
     key in both loops (`textv.c`). (b) Nested tables each independently
     decided to wrap themselves in `lsetscrollview`, producing two
     scrollbars, one nested inside the other; fixed by threading a
     `nested` flag through `htmlatk.c`'s recursive renderer so only the
     outermost tableau row gets wrapped. (c) A drag-scroll crash in core
     `matte.c`'s `DesiredSize`, recursing into a child view before it was
     linked (`view_GetIM` still NULL) and reaching real color-allocation
     code; guarded the same way `lsetscrollcontent__DesiredSize` already
     guarded its own equivalent recursion. (d) The tableau's horizontal
     scrollbar drew but didn't respond to click/drag, and endzone-left
     misdrew stale content: `lsetview__FullUpdate` forwarded a
     `view_FullRedraw` caller's possibly-0×0 rectangle straight through
     to real content (`lpair.c` already had an identical guard for this
     exact case; `lsetview` never got one), and separately
     `lsetscrollcontent`'s own repaint idiom never cleared its background
     before redrawing at a new pan offset, the way `lpair__Update`'s own
     `WantUpdate`-driven path does; both fixed. (e) The **messages
     captions list's** own vertical scrollbar — unrelated to any
     embedded HTML view, but only ever noticed once bookrack-scale
     testing prompted scrolling further through captions than usual —
     started collapsing to a whole-document, non-functional elevator
     after one "scroll back" command in some mailboxes. Root cause: the
     elevator-drag fix above widened `textv.c`'s internal `FINESCROLL`
     scrollbar-position-encoding shift from 7 to 14 bits, but
     `textv.ch`'s `EncodePosition`/`DecodePosition` **macromethods**
     still hardcoded the old literal 7 independently — a sibling site
     the original change's own grep missed because it's a macro body,
     not a read of the `FINESCROLL` symbol. Every `textview` subclass
     inherits those macros; `messages`' `captions` class is the one that
     actually calls them, to decode a `getinfo()`-returned
     (now-14-bit-shifted) position back to a raw character offset —
     silently decoding with the stale 7-bit shift left the result about
     128× too large, which `captions.c`'s own `GetInfo` treated as "off
     the end of the document." Fixed by promoting the shift to one
     shared `textview_FINESCROLL` constant read by both the `#define`
     and the macromethods. Chasing this also surfaced a build-process
     gap worth recording here since it cost real time: classpp inlines
     an inherited macromethod's body from the ancestor class's
     **installed** `build/include/atk/*.ch` copy, not the source tree,
     and that copy is only resynced by `make install`/`dependInstall` in
     the ancestor's own directory — a narrower `make foo.o` leaves it
     stale, so a subclass elsewhere can regenerate its own header and
     silently bake in the *old* macro body even after the source fix is
     correct and compiled. All five: bookrack now renders as one
     correctly-nested tableau, doesn't crash on click or on drag-scroll,
     and both its own horizontal scrollbar and the unrelated captions
     vertical scrollbar are confirmed fully functional.

     **Two more bugs, found the same evening in regression-testing the
     five above:** (f) `lsetview__DesiredSize`'s leaf case had the same
     unlinked-child recursion hazard as (c)'s `matte.c` fix and
     `lsetscrollcontent__DesiredSize`'s own pre-existing guard, just
     reached via a click/page landing on a not-yet-rendered part of the
     tableau rather than drag-scroll; fixed the same way (`lsetv.c`).
     (g) A real crash, unrelated to any of this session's own changes:
     `observable`'s `nObservers`/`maxObservers` (`observe.ch`) were
     `short` since 1988, and `xcolormap`'s observer list — a single
     process-wide cache of every distinct color ever allocated for the
     life of the display connection, never released per-message since
     nothing currently calls `xgraphic__ClearColors` on message-switch —
     overflowed it after enough distinct colors accumulated (a single
     rich photo can exceed 32767 on its own). Root-caused live via
     `lldb` attached to the running process (not a printf trace) after
     several rounds of the crash not being reproducible in any of the
     functions already being traced. Widened to `int` plus added
     missing `malloc`/`realloc` failure checks in `AddObserver`
     (`observe.c`); `observable` is embedded almost everywhere, so this
     needed a full `make Clean; make World` rather than a targeted
     rebuild. Releasing a message's colors when it's replaced (instead
     of only ever accumulating) is a real, separate follow-up, not done
     here — the widened counter just moves the ceiling far out of
     practical reach.
  2. `<td style="background-color:...">` isn't honored at all.
     `porting-assessment.md` item q.'s trailing note.
  3. Paging isn't pixel-accurate — landing positions are approximate,
     not exact. Distinct from the forward-paging double-count bug
     fixed 2026-09-13 (`porting-assessment.md` item r.) — this is the
     underlying character-count-based positioning model's residual
     imprecision, present even with that fixed, only mitigated by the
     table strategy's "peeling" heuristic, not fixed at the root.
  4. Three National Grid header-row cosmetic issues, not yet
     investigated: a link sits too high above its divider bar, that
     link uses the wrong font/weight, and the divider bar is top-
     rather than vertically-centered against its icon row.
  5. Hovering a link posts a distinctive cursor (`Cursor_Gunsight`,
     the same one hyplink's pushbutton view uses) for table-cell links
     — both image-wrapped and plain-text — fixed 2026-09-16 using
     `im_PostCursor`/`RetractCursor` (`im.c`/`xim.c`), the same idiom
     `imagev.c` and `pshbttnv.c` already use for their own cursors:
     `xim__PostCursor` creates a small invisible child X window over
     the given rectangle with that cursor shape assigned via X11's
     native per-window cursor property, so the X server shows it
     automatically as the pointer enters/exits, no motion-event
     polling needed. `htmllinkview` (`htmllinkv.c`) now owns a
     `struct cursor *` and re-posts it over its own bounds from
     `FullUpdate`, gated by the new `htmlatk_TextHasLink()`
     (`htmlatk.c`) so a cell with no link in it at all gets no cursor
     change (initial version applied the cursor to every table cell
     unconditionally — reported live, fixed same day).

     Known remaining imprecision, deliberately deferred (not a
     fundamental limitation — see below): `htmlatk_TextHasLink()` is a
     *view-level* gate, not per-run. A table cell that mixes an href
     run with plain text (e.g. the "schedule an appointment" paragraph
     — only the underlined phrase is a link) shows the cursor over the
     *whole* cell, not just the link run. A whole-paragraph link (e.g.
     "National Grid") is unaffected since the view's bounds and the
     link's bounds coincide. The exact fix is scoped, not just
     theorized: `DrawBar()` in `src/atk/text/drawtxtv.c` (~line 922)
     already computes the precise per-run on-screen rectangle used to
     draw that same underline (`bx, by, width` plus font ascent/
     descent) — hooking that to also post a cursor when the run's
     style carries our "href" attribute would give pixel-exact
     coverage. Deferred because it means editing core text-rendering
     code (drawtxtv.c, shared by every ATK app) rather than this
     project's own `htmlatk.c`/`htmllinkv.c`, and a link that wraps
     across multiple visual lines would need one `struct cursor` per
     line-segment (each `PostCursor` call only tracks one rectangle) —
     meaningfully bigger scope than anything else in this item.

     Separately, inline text links in the *main message body*
     (`t822view`, not a table cell) still have no hover cursor at all
     — they're a character range inside one large view, not their own
     view, so even the coarse whole-view gate doesn't apply; that case
     needs the same `DrawBar()` hook described above to be fixed at
     all, not just made precise.
  6. *Optional, low priority:* retarget `htmlview`'s own standalone
     viewer onto this same shared parser, so there's one HTML engine
     in the tree rather than two. `htmlview`'s composition/authoring
     side (hand-building a document, not parsing untrusted wire HTML)
     is a separate concern and doesn't need to change.

  Structural gaps, not bugs to fix — logged in the design doc's "Open
  questions" rather than here since they're undecided direction, not
  todo items: non-Latin scripts (need a real Unicode text model), SVG
  image support, no embedded image can render truly invisibly (border +
  no transparency), image cache isn't persistent across reopens,
  whether `font`-element support is worth its own code path.
  With core rendering done and verified, the open question is whether
  to keep hardening on the `html` branch or merge it back into `trunk`
  at its current, already-useful state (the `andrew-6.4` precedent) and
  pick up the rest later — not yet decided.

### Coverage inventory

- **Description:** Most apps and insets are confirmed either working
  or broken (see the table above), but a handful have simply never
  been exercised, and aren't distinguished from either group yet.
- **Next step:** Inventory untested apps and insets.

### Application work

- **Description:** Track and fix applications known broken.
- **Next step:** `typescript`'s PTY crash is the one broken
  application left (see table above).

### Inset work

- **Description:** Track and fix insets known broken or buggy.
- **Next step:** `figure` italic-text sizing, `eq`'s multi-line cleanup
  bug (see table above); `convertraster` CLI bugs and `image` JPEG/TIFF
  import fixed 2026-08-08.

---

## Open issues

Smaller items that don't fit the tables above.

**Little annoyances**
- Arrow keys and a "Meta" key aren't bound yet
- ~~`make Clean` transiently deletes `src/atk/adew/Arb`~~ **RESOLVED 2026-08-12**: this undersold it — a build had actually clobbered the tracked `.ez` arbiter document with the generated `arb` launcher (same directory entry on case-insensitive filesystems) back in June, and the corruption had been committed, not transient. Restored from the original import on both branches; `Imakefile` now builds the launcher under a collision-proof name and renames it only at install time. See `revival.md` → "Old bugs never found till now."
- `messages`: intermittent, non-fatal `X_OpenFont BadValue` X errors during normal folder browsing — not root-caused
- `filetype.c DeleteEntry`: a bogus-free risk and an apparently-inverted condition, flagged by the compiler, never observed to actually fire
- `runapp -d` with no app-class argument segfaults instead of printing usage — pre-existing since the 1988 source, not a regression
- **RESOLVED 2026-08-12 (docs):** a fresh checkout has two separate hardcoded-path spots, not one — `site.h`'s `DEFAULT_ANDREWDIR_ENV` was documented, but `config/Makefile`'s `BASEDIR` (baked in from `site.h` by imake at Step 2, and not reliably self-regenerated afterward — see the fossil-mtime caveat elsewhere in this file) wasn't, and silently stays stale if `site.h` gets fixed after Step 2 has already run once. Reported independently by an outside builder hitting exactly this. `quickstart.md`'s "Site configuration" section now covers both.
- **RESOLVED 2026-08-19:** clicking a scrollbar endzone to jump to the end of a document blanked the whole window on a held click, past the endzone button's ~100ms auto-repeat — `BackSpace`'s `MoveByPixels` "stay within the current line" fast-path was applied regardless of whether `pos` was actually the current top position, and the endzone's repeat re-issued an identical jump request while stale state from the first click was still around, tripping the shortcut against an unrelated value and landing on a position with no line of its own. Fixed by gating the shortcut on `pos == textview_GetTopPosition(self)` (`492e9b6e24`). Found chasing an HTML-mail-rendering report but was a general ATK scrollbar bug, not HTML-specific. See `revival.md` → "Old bugs never found till now" for the full writeup.

**Heisenbugs** (intermittent, low reproducibility)
- Xlib display-lock self-deadlock: reproduced once, root cause identified (`_XLockDisplay` re-entered from inside `XRefreshKeyboardMapping`, triggered by a keyboard-mapping-change event) but not yet fixed. Current best explanation for the older, harder-to-pin-down "`^V` scroll hang."
- A rare report of text drawing into the menu area — not reproduced since first observed
- Menu-bar posting sometimes takes a fixed ~510ms quantum to draw (perceived multi-second delays are two or more of these back-to-back, not one longer stall): **root cause confirmed 2026-08-11, not an AUIS defect, no fix available or needed.** Live process sampling of XQuartz's own `X11.bin`, correlated directly against `mdbg`-logged stall timestamps, showed real dispatch time (~1278ms) closely matching independently measured stall time (1533ms across 3 stalls in the same window) inside one specific chain: `ProcClearToBackground` (servicing `DrawMenuItems`'s `XClearWindow`) → `RootlessStartDrawing` → `xp_lock_window` → `SLSConnectionSynchronizeSLSCATransaction` → `_SLSTransactionWaitSource` — XQuartz blocked waiting for macOS's own SkyLight/Core Animation window compositor to synchronize a transaction before it's allowed to lock the popup window's backing store and paint. This is genuine, external, server-side latency in XQuartz's rootless-drawing pipeline, confirmed unrelated to AUIS's `XGrabPointer` usage (already ruled out) and to App Nap or frontmost/background app status (both tested and ruled out as the *why* behind the ~510ms specifically). The *why* the wait is sometimes instant and sometimes ~510ms remains unresolved (some undocumented SkyLight/WindowServer-internal cadence, not app-focus-dependent) but the *where* is proven and outside AUIS's or XQuartz's control — treated as an accepted characteristic of XQuartz on modern macOS, not pursued further absent a second X server or SkyLight documentation. See `porting-assessment.md` → "Intermittent ~500ms menu-bar posting delay" for the full inventory. Diagnostic tracing left in the tree, off by default (`MenuDebugTrace` profile switch; `MENUDBGTRACE` env var for the popup-menu path), now timestamped for future correlation work.

---

## Key tradeoffs

**Modernization strategy** — how the code itself was brought forward:

- **Compiler leniency, not mass modernization, as the base strategy.**
  Leave 1990s K&R source as-is; suppress the resulting compiler noise
  with flags (`-Wno-implicit-int` etc.) instead of rewriting the tree.
  *Why:* one automated mass-conversion attempt was tried first and
  reverted — it introduced bugs faster than it fixed real ones.
  `porting-assessment.md` → "Getting K&R-era source to build under a
  modern compiler."
- **The later ANSI C conversion (see Major milestones) went
  directory-by-directory with the compiler as auditor, not via a
  rewrite tool.** *Why:* `.ch` class-interface files already carried
  full ANSI signatures that the class preprocessor was silently
  discarding; emitting them let the compiler type-check every call
  site tree-wide before any file was hand-edited, catching decades of
  signature drift as compile errors instead of grep hits.
  `porting-assessment.md` §14.
- **`gnu89`, not `gnu99`/`c99`.** *Why:* verified empirically — the
  actual blocker isn't classpp's untyped-dispatch idiom (compiles
  identically under either standard), it's reserved-keyword collisions
  from 35 years of K&R-era identifiers (e.g. a parameter literally
  named `restrict`). A `c99` migration is possible later but needs a
  tree-wide reserved-word audit first. `porting-assessment.md` →
  "Why gnu89."
- **`dlopen()`/`dlsym()` instead of reviving the original
  platform-specific loaders.** *Why:* ~70 lines replacing ~500;
  POSIX-standard, no per-platform loader code to maintain.
- **System `bison`, not the vendored Andrew bison fork.** *Why:* the
  vendored fork hangs on arm64. `mkparser` was patched to match modern
  bison's output format instead.

**Scope decisions** — what's deliberately not part of this build:

- **Console (terminal-emulator) subsystem excluded.** *Why:* its
  interprocess-communication layer would need its own rewrite; set
  aside rather than taken on alongside everything else. Known side
  effect: two icon fonts an unrelated animation inset depends on
  happen to live in Console's own build directory, so they go unbuilt
  too unless Console (or just its font directory) is built standalone.
- **`vui`/`tm` (curses terminal clients on the removed BSD `sgtty`
  API) and `bdffont` left out rather than fixed.** *Why:* not needed
  for the GUI `messages` path; `bdffont` additionally has no working
  build machinery left in this checkout at all.

**Mail architecture** — how AMS talks to the outside world:

- **Local mbox store plus a one-way IMAP mirror (Thunderbird-style
  cache), not a revived AMDS delivery daemon.** *Why:* AMDS's
  shared-filesystem delivery model is a dead end on a modern
  single-user Mac — there's no shared AFS cell to deliver into. The
  local store stays authoritative; an IMAP sync agent mirrors a real
  mailbox into it. Full design: `ams-IMAP-project.md`.
- **Hand-rolled IMAP client, not an existing library.** Decision and
  reasoning recorded in `ams-IMAP-project.md` §8.

---

## Major milestones

- **2026-08-16** — HTML mail rendering's ATK-styled renderer (Stage 3)
  wired into `messages`/`cui`, retiring the old plain-text-only shim —
  real-world mail now gets clickable links, inline images, and styled
  text instead of stripped tags. Table strategy pivoted mid-stage from
  Andrew's `table`/`spread` object to `lset`/`lpair` (the former
  doesn't reflow with window width); a real core-ATK scrolling bug and
  two more real-mail rendering bugs found and fixed the same day,
  verified live against an actual message.
- **2026-08-07** — The ANSI C modernization effort (see Key tradeoffs)
  reached its conclusion: the entire active codebase now compiles
  clean under full strict-C compiler settings, closing out a
  systematic, decades-overdue type-safety pass across the whole tree.
  Dozens of long-latent bugs found and fixed along the way are
  cataloged in `revival.md`.
- **2026-07-23** — AMS-over-IMAP writeback complete: local edits to a
  mirrored mailbox replay back to the real IMAP server. Feature-complete
  except OAuth2.
- **2026-07-17** — First SMTP mail sent by AMS: a scripted client
  composes and submits over TLS to a real mail provider, authenticated,
  end-to-end — roughly 35 years after AMDS last moved a message by
  copying files through a shared filesystem.
- **2026-07-10** — Every method call site in the active tree became
  compiler-checked for the first time, via typed class dispatch — the
  first step of the ANSI C modernization effort above.
- **2026-07-05** — `messages` runs against a local mail store for the
  first time.

---

## Stretch goals

- **Pie menus** — AUIS's menu architecture suits Don Hopkins' pie-menu
  design well; a collaborative implementation project once other work
  settles down.
- **`ez2md` improvements** — table/eq/figure/fad/image/link objects
  still render as placeholder comments in the `.ez`-to-Markdown
  converter; batch-converting the archive's FAQ/newsletters/papers to
  Markdown is unstarted.
- **`c99`/`c11` migration** — not planned for the foreseeable future.
  `gnu89` is load-bearing today only because of ~35 years of K&R-era
  identifiers that collide with later reserved words (`restrict` is the
  one confirmed hit); a migration needs a tree-wide reserved-word audit
  first. `porting-assessment.md` → "Why `gnu89`."

---

## Further reading

Testing procedure material that used to live in this file (the
subsystem dependency lattice, the per-inset test-document table, the
regression checklists) is preserved in `claude-history/README.md` →
"Retired top-level docs" → `roadmap-old.md`. It may get a dedicated
home of its own in a later documentation pass rather than staying only
in the retired file.
