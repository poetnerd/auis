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
work queue". As of 2026-07-19:

1. `strlit-sweep-prompt.md` — writable-string-literal census + fixes
2. `m2-census-prompt.md` — M2 point-0 warning classification
3. `bcc-direct-insertion-prompt.md` — blind-copy direct-insertion
   root cause
4. `folder-visibility-prompt.md` — mirrored-folder default
   visibility
5. `imap-writeback-prompt.md` — Milestone 4 change-journal writeback

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
`bcc-direct-insertion`, `folder-visibility` (all still active), and:

- `fdplumb-prompt.md` (2026-07-19, retired same day) — the
  fd-ledger criticals / preference-blackout investigation. Gate 1
  was closed by a Fable static-analysis session before any Sonnet
  session ran it (see tree-root `fdplumb-REPORT.md`): fdplumb
  exonerated, wrappers proven detect-only, profile.c
  failure-latching fixed. Retired with its premise corrections
  banner intact — a case study in a prompt being answered rather
  than executed.

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
