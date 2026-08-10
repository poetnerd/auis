# AUIS Revival Roadmap

Last updated: 2026-08-07

This document is intended primarily for AUIS revival participants,
with a summary of what's running, what's active, what's next, and the
high level rationale for the various tradeoffs made.

You can make a deep dive into the details in the documents below:

| Doc | Description |
|---|---|
| `revival.md` | What's been accomplished, told as a narrative, for readers who already know AUIS or are meeting it for the first time |
| `porting-assessment.md` | Bug-class root-cause analysis and the full ANSI C conversion plan |
| `porting-changelog.md` | Dated log of every fix |
| `roadmap-old.md` | This document's full pre-2026-08-07 history — retired, not maintained, kept for provenance |
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
| Core embedded insets — `eq`, `table`, `fad`, `fnote`, `bp`, `srctext`, `figure`, `value`/slider/button, `pushbutton`/`link`, `lset`, `org`, `apt`/`cel`/`arbiter` | Fully working | `figure` has one open cosmetic bug: italic-text sizing is non-monotonic (10pt renders bigger than 12pt) |
| `calc`, `zip` (contrib) | Fully working | |
| `clock` | Fully working | |
| `raster` (as an embedded inset) | Fully working | |
| `contentv` (Table of Contents) | Fully working | An earlier report of it ignoring enumerated headings was a false alarm — root cause was input focus being inside an embedded inset rather than the document itself when the ToC view was opened |
| `convertraster` (standalone CLI) | Fully working | Fully tested 2026-08-08; three bugs found and fixed (see `porting-changelog.md`) |
| `image` (JPEG/TIFF import) | Fully working | Fixed 2026-08-08: TIFF import was totally broken (four LP64 struct/stride bugs in vendored `libtiff`); JPEG/TIFF solid-color render was an unrelated `xgraphic.c` variable mixup — see `porting-changelog.md`. GIF import shares the same render path so is likely also fixed, but wasn't retested |
| `htmlview` | Rough | No longer crashes, but real-world HTML mostly fails to render — see Projects → HTML mail — htmlview modernization |
| `eq`'s integral symbol | Minor bug | Glyph missing; suspect the font pipeline, not `eq` itself |

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

### HTML mail — MIME parsing

- **Description:** `messages` used to just offer a button to run
  `metamail` on any non-text part; it now has a real MIME parser, and
  most mail is readable without that fallback.
- **Next step:** Close fidelity gaps — e.g. links currently render as
  plain text instead of clickable link insets.

### HTML mail — `htmlview` modernization

- **Description:** Real inline HTML rendering depends on bringing the
  ~1994 `htmlview` parser up to handle modern markup. Right now it
  gives up and renders nothing at all on some tags rather than
  degrading gracefully; with those tags stripped by hand, what's left
  renders, but not well.
- **Next step:** Investigate the parser itself — what it chokes on and
  why — before deciding whether this is a few gating bugs or a deeper
  rewrite.

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
- **Next step:** `figure` italic-text sizing, `eq`'s integral symbol
  (see table above); `convertraster` CLI bugs and `image` JPEG/TIFF
  import fixed 2026-08-08.

---

## Open issues

Smaller items that don't fit the tables above.

**Little annoyances**
- Arrow keys and a "Meta" key aren't bound yet
- `make Clean` transiently deletes `src/atk/adew/Arb` (case-insensitive-filesystem collision with a sibling `arb`); self-heals on the next `dependInstall`
- `messages`: intermittent, non-fatal `X_OpenFont BadValue` X errors during normal folder browsing — not root-caused
- `filetype.c DeleteEntry`: a bogus-free risk and an apparently-inverted condition, flagged by the compiler, never observed to actually fire
- `runapp -d` with no app-class argument segfaults instead of printing usage — pre-existing since the 1988 source, not a regression

**Heisenbugs** (intermittent, low reproducibility)
- Xlib display-lock self-deadlock: reproduced once, root cause identified (`_XLockDisplay` re-entered from inside `XRefreshKeyboardMapping`, triggered by a keyboard-mapping-change event) but not yet fixed. Current best explanation for the older, harder-to-pin-down "`^V` scroll hang."
- A rare report of text drawing into the menu area — not reproduced since first observed

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

---

## Further reading

Testing procedure material that used to live in this file (the
subsystem dependency lattice, the per-inset test-document table, the
regression checklists) is preserved in `roadmap-old.md` for now; it
may get a dedicated home of its own in a later documentation pass
rather than staying only in the retired file.
