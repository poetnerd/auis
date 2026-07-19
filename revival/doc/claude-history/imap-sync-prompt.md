# Milestone 3b spec: imapsync — one-way IMAP mirror into the AMS store

Scope: second of three M3 sub-deliverables. Deliver `imapsync`, a
command-line AMS client that mirrors IMAP folders one-way (server →
local) into a new mspath root, so cui can browse IMAP mail through the
completely unmodified AMS store machinery. NO writeback (milestone 4),
NO messages-GUI acceptance (milestone 3c), IMAP strictly read-only
(EXAMINE / BODY.PEEK[] only, as in 3a).

Read first, in order:
1. `revival/doc/ams-IMAP-project.md` — §2A (architecture: the local
   store IS the cache; INBOX is mirrored, not ingested), §3 (identity),
   §10 (milestones).
2. `src/overhead/mail/hdrs/imap_prot.h` — the protocol contract you
   code against. It is complete for this milestone; if it proves
   insufficient, STOP and report rather than changing it.
3. `revival/doc/ams-server-client-interface.md` — the MS_* store API.
4. `src/ams/libs/ms/apndfile.c`, `msdir.c`, `bldcapt.c` — how the store
   appends a message and builds snapshots today.

Language level: new code is ANSI C (C89 prototypes), scanf family
banned — same policy as `imap_prot.c`, same rationale.

## Design decisions (made; not yours to re-open)

1. **Write through the MS layer, not raw files.** `imapsync` links
   libmssrv/libcui via nosnap exactly like cui does, and lets the
   store's own code maintain `.MS_MsgDir`, captions, chains, and master
   update files. Do NOT hand-write snapshot/index bytes.
2. **One additive MS-layer entry point is authorized.** The existing
   append path (`MS_AppendFileToFolder`) invents its own message id and
   stamps "now" as the date. Sync needs caller-supplied identity and
   ordering, so add ONE new function in `src/ams/libs/ms` (natural home:
   `apndfile.c`), e.g.:
   `MS_AppendFileToFolderWithId(char *file, char *folder, char *id, char *date64)`
   — identical to the existing path except: uses the supplied 18-char
   id instead of `ams_genid()`, and the supplied base-64 date (derived
   from INTERNALDATE) as `AMS_DATE`. Everything else (parsing, caption,
   chain hashing, index update) stays the store's own code. Existing
   functions unchanged; K&R style in that file; the new entry point
   gets a prototyped declaration where the tree's conventions put MS_
   externs (find where cui/messages get them and follow suit).
3. **AMS id = f(UIDVALIDITY, UID), deterministic.** Use the tree's own
   base-64 helpers (`convlongto64` etc.) to encode UIDVALIDITY (32
   bits) + UID (32 bits) into a fixed-width id in the store's id
   charset, with a distinctive prefix so synthesized ids are
   recognizable (document the exact format in the code and your
   report). Same message ⇒ same id on every run — idempotency depends
   on it. The id must be exactly AMS's id width (see AMS_ID /
   `ams_genid` for the convention).
4. **Mirror root**: `~/.IMAP/fastmail/`, folders as subdirectories
   following the IMAP hierarchy (delimiter from LIST → `/`). INBOX is
   a folder like any other. The real root is used only in the final
   acceptance step; all development/testing runs against a scratch
   root via a flag (see Testing).
5. **Folder selection**: preference `imapsyncfolders` (comma-separated
   IMAP names, default `INBOX`). v1 does not auto-mirror the whole
   LIST tree (the account has ~300 mailboxes; opt-in keeps first sync
   bounded).
6. **Per-folder sync state** lives in a small text file
   `.MS_IMAPSync` inside each mirrored folder directory (alongside
   `.MS_MsgDir`): uidvalidity, highest-synced uid, highestmodseq (0 if
   unused), one `key value` per line, written atomically
   (write-temp + rename). Human-readable on purpose.
7. **Sync algorithm** (per folder, one-shot run):
   - EXAMINE; if UIDVALIDITY differs from state: log loudly, remove
     the folder's mirrored messages and state, re-mirror from scratch
     (the ids encode UIDVALIDITY, so stale messages are detectable).
   - New mail: `UID SEARCH UID <last+1>:*` (ESEARCH used by
     imap_prot automatically), then `imap_UidFetchMeta` in batches
     (~100 UIDs per uidset) + `imap_UidFetchBody` per message to a
     temp file → `MS_AppendFileToFolderWithId` → set attributes.
   - Flags: for already-mirrored messages, fetch FLAGS for the synced
     uid range in batches and apply changes via `MS_AlterSnapshot`
     (attribute codes: `\Seen`→clear `AMS_ATT_UNSEEN`, `\Answered`→
     `AMS_ATT_REPLIEDTO`, `\Deleted`→`AMS_ATT_DELETED`). If CONDSTORE
     is available, use `HIGHESTMODSEQ` to skip the flags pass when
     nothing changed (spell out in the report whether you used
     `CHANGEDSINCE` on the FETCH or a modseq comparison only).
   - Server-expunged messages (uid present locally, absent from a
     full-uid-list check): mark `AMS_ATT_DELETED` locally; do not
     purge in v1. This check may be limited to an explicit
     `--full-check` flag if per-run cost is high; say what you chose.
8. **Invocation**: `imapsync [-root <dir>] [-folders <list>] [-v]`
   one-shot; exits nonzero on any folder failure. Daemon/timer modes
   are out of scope. Program lives in
   `src/ams/msclients/imapsync/` (new directory, Imakefile modeled on
   a sibling msclient; note MK_CUI gates that subtree — follow how
   cui's Imakefile is wired in).
9. **Reconnect**: on `IMAP_DEAD`, one `imap_Reopen` retry per folder
   pass; `IMAP_UIDCHANGED` funnels into the UIDVALIDITY path above.

## Testing

`revival/tests/imap-sync-tests` (same conventions as the existing
suites; update `revival/tests/README.md`). Uses a scratch mirror root
(mktemp -d) — the real `~/.IMAP` is never touched by the suite. Live
IMAP via the netrc stanza, read-only, INBOX only. Cases at minimum:

1. Fresh mirror of INBOX into scratch root: message count matches
   EXAMINE's EXISTS; spot-check one known marker message
   (STAGE-3-CUI-TEST) — caption present, body file readable, id has
   the documented synthesized format.
2. Idempotency: immediate second run adds nothing, alters nothing
   (compare `.MS_MsgDir` mtime/size or message count before/after).
3. Incremental: send one marker message to self via the committed SMTP
   path (`smtptest.test`, recipient wdc@fastmail.com only), poll-sync
   until it appears (bounded retries, tolerant of delivery delay);
   exactly one new message added, and it is the marker.
4. Flags mapping: the newly-arrived unseen message has AMS_ATT_UNSEEN
   set locally; the old marker message (seen on the server) does not.
5. UIDVALIDITY change drill: corrupt the state file's uidvalidity to a
   wrong value, re-run, verify loud re-mirror and correct final state.
6. cui browse (scripted, pty pattern from smtp-send-test): with mspath
   extended to the scratch root (scratch PROFILES file — never the
   real ~/preferences), cui can list the folder and display the marker
   message's caption and body.

Suites `imap-protocol-tests` and both SMTP suites must still pass at
Gate 2 (you're linking new territory; prove no regression).

## Gates

* **Gate 1 (STOP and report):** fresh mirror + idempotent re-run of
  INBOX into a scratch root work (test cases 1–2), snapshots visibly
  correct (paste a few captions), `MS_AppendFileToFolderWithId` diff
  shown. Before any flags/expunge/incremental work.
* **Gate 2 (STOP and report, end of M3b):** full suite 6/6, all prior
  suites green, diffs, the user-facing "how to point real mspath at
  ~/.IMAP/fastmail" instructions (determine the exact mspath
  preference syntax from the code and write it down for the user —
  do not edit the user's preferences yourself), fossil status.

## Ground rules

No fossil commits. IMAP strictly read-only. Never write outside: the
scratch roots, `src/ams/msclients/imapsync/`, the one authorized
`apndfile.c` addition (+ its extern declaration site),
`revival/tests/`. Never edit ~/preferences or ~/.netrc (read netrc via
netrc_Lookup only). Password redacted everywhere. Sends (test case 3
only) go only to wdc@fastmail.com. No concurrent builds; compile-verify
everything touched, no new warning categories (measure against
`fossil cat` originals for edited files). If the MS-layer integration
turns out to need more than the single authorized entry point, STOP
and report — that's a design conversation, not a judgment call.

Maintain a breadcrumb handoff file at
`<scratchpad>/M3B-HANDOFF.md` (the coordinator supplies the scratchpad
path) updated after each meaningful step, so a usage-limit interruption
can be recovered by a fresh instance from spec + fossil diff + handoff
instead of a transcript replay.
