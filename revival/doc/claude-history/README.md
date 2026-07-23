# claude-history — retired Claude-written working docs

This directory holds Claude-written working documents whose work is
**complete or retired**: task prompts (specs written for delegated
sessions), investigation write-ups, and rollout runbooks. They are
kept because they document how each piece was actually specified,
investigated, and gated — often the best surviving design document
for the module or fix they produced.

**Placement policy (decided 2026-07-19):** active prompts live
directly in `revival/doc/` so a fresh instance finds its task where
the roadmap's Delegated work queue and `sonnet-playbook.md` say it
is. Moving a prompt here (`fossil mv`, then repoint any references)
is part of retiring it, done during the close-out doc update.

## Active Prompts (in `revival/doc/`, not here)

Current queue and suggested order live in `roadmap.md` → "Delegated
work queue". As of 2026-07-22:

1. `strlit-sweep-prompt.md` — writable-string-literal census + fixes
2. `m2-census-prompt.md` — M2 point-0 warning classification
3. `bcc-direct-insertion-prompt.md` — blind-copy direct-insertion
   root cause
4. `imap-writeback-prompt.md` — Milestone 4 change-journal writeback
5. `mime-attachment-icon-prompt.md` — root-cause a `multipart/mixed`
   attachment rendering as a bare `?` instead of an
   `[attachment: ...]` line (added 2026-07-21, follow-on to the
   just-retired `mime-display` task below)

Standing (never retire on completion; they recur):
`sonnet-playbook.md` (delegation briefing),
`update-revival-doc-workflow.md` (revival.md refresh procedure).

## Prompt History

The prompts fall into four project chunks; within each, the order
they were developed:

### 1. Tree build-out (July 5)

- `new-tree-porting-prompt.md` (2026-07-05) — fresh-instance guide
  for compile-fixing the newly enabled `AMS_ENV`/`CONTRIB` trees.
  The earliest delegation-style prompt; that porting phase completed
  with the `messages` launch (2026-07-05) and the calc/zip inset
  build-outs (2026-07-11).

### 2. Inset fix specs (July 12)

- `calc-ghost-fix-prompt.md` (2026-07-12) — spec for the calc inset
  ghost-text fix (XftDrawRect band-fill in `graphic_WHITE` mode).
  Fix landed and was user-confirmed the same day; lived in the tree
  root until this directory was created.

### 3. AMS-over-IMAP milestone specs (July 16–19)

Written one per milestone as the project advanced (plan of record:
`../ams-IMAP-project.md`); each was executed by a delegated session
against live Fastmail and closed with its milestone:

- `smtp-send-prompt.md` (~2026-07-16) — Milestone 1, SMTP send
  behind `dropoff()`; three stages, two gates. Done 2026-07-17 —
  first SMTP mail ever sent by AMS.
- `imap-spike-prompt.md` (2026-07-17) — Milestone 2, go/no-go
  spike; produced the hand-roll decision and the transport findings
  that shaped 3a. Done same day.
- `imap-protocol-prompt.md` (2026-07-17/18) — Milestone 3a, tlscon
  hardening + `imap_prot`, the tree's first born-ANSI module. Done
  2026-07-18.
- `imap-sync-prompt.md` (2026-07-18) — Milestone 3b, `imapsync`
  one-way mirror through the store's own code. Done 2026-07-18.
- (`imap-writeback-prompt.md`, 2026-07-19, Milestone 4 — still
  active, see above; will move here when M4 closes.)

### 4. Budget-crunch delegation queue (July 19)

Five gated prompts plus the standing `sonnet-playbook.md`, written
in one pass (commit 8e565c9fda) so Sonnet-class sessions could
carry work forward independently: `strlit-sweep`, `m2-census`,
`bcc-direct-insertion` (still active), and:

- `folder-visibility-prompt.md` (2026-07-19, retired 2026-07-22) —
  mirrored-folder default visibility. Gate 1's own premise
  (subscription status gates default visibility) was investigated,
  found real but incomplete, and a fix proposed
  (`folder-visibility-REPORT.md`) — then *falsified* the same day by
  wdc directly testing it live (Ask and Show-All subscription both
  failed to make INBOX visible). Root-caused instead via live
  instrumentation (temporarily hardcoded `foldersDebugging`/
  `MSDebugging` debug levels, reverted after): the real gate is
  `AMS_OnlyMail`, a site-config global defaulting to `1` without
  `RUN_AMDS_ENV` (this revival never defines it), which silently
  restricts the default "Expose New" view to `$HOME/.MESSAGES` only,
  excluding every other mspath root before subscription status is
  even checked. Fixed with `AMS_OnlyMail: No` in `build/etc/
  AndrewSetup` (no rebuild needed); `revival/tools/write-andrewsetup`
  added since that file lives outside `build/`'s tracked tree and
  doesn't survive `make Clean`. Confirmed live: INBOX visible by
  default with either Ask or Show-All subscription (Show-All chosen
  — see the report's addendum for the semantic difference). See
  `folder-visibility-REPORT.md`'s "Correction" section for the full
  falsification-and-recovery trail — a second case study (after
  fdplumb) in a prompt's own investigation needing to be corrected by
  direct testing rather than more static reading.
- `fdplumb-prompt.md` (2026-07-19, retired same day) — the
  fd-ledger criticals / preference-blackout investigation. Gate 1
  was closed by a Fable static-analysis session before any Sonnet
  session ran it (see `fdplumb-REPORT.md`, kept here): fdplumb
  exonerated, wrappers proven detect-only, profile.c
  failure-latching fixed. Retired with its premise corrections
  banner intact — a case study in a prompt being answered rather
  than executed.
- `mime-display-prompt.md` (2026-07-19, retired 2026-07-21) — MIME
  body display in `messages`: new born-ANSI `mimepart` module
  (`src/ams/libs/hdrs/mimepart.h` + `src/ams/libs/shr/mimepart.c`,
  multipart split/QP/base64/alternative-select/html-strip/UTF-8
  conversion, standalone-tested), wired into `text822.c`'s
  `ReadMessage`: multipart/alternative prefers text/plain, html-only
  gets the interim strip shim, multipart/mixed lists non-text parts
  as `[attachment: ...]` lines, UTF-8 text/plain finally renders
  instead of falling to a metamail button. Three gates, all closed
  (see `mime-display-REPORT.md`, kept here). Gate 3's by-hand
  acceptance against wdc's real mailbox found and fixed two more
  pre-existing, unrelated bugs load-bearing for this feature actually
  working: `GetHeader`'s header/body-boundary check was CRLF-blind,
  and `text822.do`'s Imakefile link line silently omitted
  `libmsshr.a` (`-undefined dynamic_lookup` masked it at build time;
  first real call to a new library symbol crashed at runtime). One
  known follow-on left open, queued as its own prompt:
  `../mime-attachment-icon-prompt.md` — a `multipart/mixed`
  attachment renders as a bare `?` instead of the expected
  `[attachment: ...]` line; root cause not yet found.

## Runbook & Investigation History

Retired here 2026-07-19; in development order:

- `m1-rollout-runbook.md` (~2026-07-08) — the per-directory
  procedure for the M1 typed-dispatch rollout, with the empirical
  fallout taxonomy from the pilot directories. M1 completed
  tree-wide 2026-07-10. **Residual use:** this remains the
  procedure to follow when an inert directory (e.g.
  `contrib/zip/utility`, ness) is first brought into the build and
  needs its `.ch`-typing pass.
- `m1-point10-batches.md` (2026-07-09) — point 10's exhaustive
  batch list and paste-ready session prompts; all batches ticked
  complete 2026-07-10. Its inert-directory survey is still the best
  inventory of what's outside the default build.
- `zip-black-render-investigation.md` (2026-07-11) — the
  solid-black-at-`-O` bisection trail, ending at the signed/unsigned
  `returns char` `.ch` declaration mismatch. Resolved and committed
  2026-07-11.
- `calc-text-rendering-investigation.md` (2026-07-11/12) — the calc
  inset's crash + three Xft rendering bugs, including the
  AA-erase-by-overdraw ghost and the XQuartz recomposite-lag
  discovery (fixed with the self-XCopyArea kick, a general fix not
  calc-specific). All items resolved 2026-07-12; the one open
  cosmetic item ("=" reverse video) is tracked in `roadmap.md`, not
  here.
- `fdplumb-REPORT.md` (2026-07-19) — the Gate-1 report that
  answered `fdplumb-prompt.md` (above) by static analysis. Fixes it
  staged were committed and runtime-verified the same day. Unlike
  the other closed investigations, part of it is still live: its
  "Untouched hazards" list (unchecked malloc in RegisterOpenFile,
  openprofile's `~`-expansion comma bug, gethome's 100-byte buffer,
  prefed's unverified save path) is documented nowhere else.
