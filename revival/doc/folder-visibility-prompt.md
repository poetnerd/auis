# Task: mirrored IMAP folders should be visible without "Expose All"

Read `revival/doc/sonnet-playbook.md` first; its rules apply.

## Background

`imapsync` mirrors IMAP folders into a local AMS message store
(`~/.IMAP/fastmail/.MESSAGES/...`), and `~/preferences` adds that
root to `mspath`. `messages` can browse the mirrored INBOX — but
only after the user clicks **Message Folders → Expose All**; by
default the folder list does not show it. Goal: folders in a
mirrored root appear by default, without destroying the
subscription customizations AMS users could historically make.

**Known crash you may hit (2026-07-19, tracked in roadmap.md M3c —
NOT yours to fix in this session):** messages crashes on exit —
`EXC_BAD_ACCESS` in `MS_SetAssociatedTime` (amsn.do), via
`ams__CommitState` → `captions__MakeCachedUpdates`, faulting address
32-bit-truncated (LP64 pointer-truncation signature). If it fires
during your runs: it does not invalidate folder-list findings made
before the quit keystroke; capture the backtrace in your report if
you can, note the folder set that was open, and move on.

Starting points:
- The subscription API surfaces in `ams/libs/cui/cuilib.c`
  (`CUI_GetSubscriptionEntry` / `CUI_SetSubscriptionEntry` /
  `CUI_MergeDirectories` — see the declaration block near line 85)
  and whatever MS_ calls they wrap in `ams/libs/ms/`.
- The messages-side folder list is built in
  `atkams/messages/lib/folders.c`.
- The mirror program is `src/ams/msclients/imapsync/imap_sync.c`; it
  creates folder directories in the mirror root and could plausibly
  mark them subscribed at creation time, if that is how
  subscriptions persist.

## Gate 1 — investigation only

Answer with file:line evidence:
1. Where does the default folder list come from, and what exactly
   does "Expose All" change (transient view state vs persisted
   subscription data)?
2. How are subscriptions persisted — what file(s), what format,
   keyed how? (Look for a "subscription map" notion in ms/cuilib.)
3. Why does a freshly mirrored folder default to hidden — is it
   "unsubscribed", or simply unknown to the subscription data?
4. Proposal: the minimal change that makes mirrored folders visible
   by default. Candidate shapes to evaluate against what you find:
   (a) imapsync writes/updates subscription entries when it creates
   a folder; (b) the MS layer treats folders under an mspath element
   with no subscription data as subscribed-by-default; (c) a
   messages-side default change. State which you recommend and what
   it would break for a hypothetical classic-AMS user.

Use a scratch mirror root for experiments
(`build/bin/imapsync -root /tmp/<scratch> -v`, then point a scratch
PROFILES mspath at it for cui) — do not modify the real
`~/.IMAP/fastmail` state or the user's real subscription data. cui
folder-listing commands are the non-GUI way to observe visibility;
the final GUI check in messages belongs to wdc.

STOP at Gate 1 with the report and proposal.

## Gate 2 — implementation (only after approval)

Implement the approved proposal; rebuild per the playbook's relink
notes; verify with the scratch root + cui and, if the change touches
imapsync, run `revival/tests/imap-sync-tests`. No commits;
`fossil diff > folder-visibility-session.diff` plus the report.
