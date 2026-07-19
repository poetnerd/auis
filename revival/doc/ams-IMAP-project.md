# Enable IMAP as mail store for AMS: messages, cui — rev 2

**Status: milestones 1 (SMTP send), 2 (IMAP spike), 3a (transport
hardening + `imap_prot` protocol layer), and 3b (one-way mirror)
COMPLETE as of 2026-07-18. `imapsync` mirrors the real INBOX into
`~/.IMAP/fastmail/.MESSAGES/`, and `messages` browses it via an mspath
element (`$default:~/.IMAP/fastmail/.MESSAGES`), confirmed by hand.
Next: milestone 3c — messages-GUI acceptance. Known 3c items from the
first real browse: mirrored folders only appear after Message Folders →
Expose All (subscription defaults for mirrored roots); metamail is
launched for MIME display but shows nothing (pre-existing platform
gap, out of AMS-IMAP scope). See §10.**

Goal: teach the AMS clients about IMAP as a mail store and SMTP as a mail
sender, with OAuth2-capable authentication. Test platform: Fastmail.

Rev 2 (2026-07-16) folds in the results of a source-level survey of
`src/ams`. The survey answered the open questions in rev 1; sections below
that were speculation are now stated as fact with file references.

## 0. Corrected premises and key findings

* **Build flags.** `AMS_ENV` is ON in our build (`src/config/site.h`) — it is
  what builds the clients and libraries. The flag we keep OFF is
  `AMS_DELIVERY_ENV`, which gates `delivery/` and `utils/` (queuemail,
  trymail, switchmail — AMDS proper, the distributed-filesystem transport).
  `SNAP_ENV` is also off, so no standalone message-server process is built.
  AMDS was only ever the *transport*; the mail-store API is fully alive.

* **The store seam.** Clients never touch the store directly. Everything goes
  through the `MS_*` API (~75 entry points in `ams/libs/ms/libmssrv.a`),
  linked in-process via the `ams/libs/nosnap` shim. The interface is
  documented by Borenstein in `ams/libs/ms/ServerCalls.d`, converted for the
  revival as **`revival/doc/ams-server-client-interface.md`** — read that
  first. The union of MS_ calls actually used by cuilib + messages + cui is
  ~55; the live store surface is **25–30 functions**; the rest are
  AMDS/bboard maintenance we can stub.

* **The send seam.** `MS_SubmitMessage` (`ams/libs/ms/submsg.c`) sanitizes
  headers, spools an RFC822 file, and calls `dropoff()`
  (`overhead/mail/lib/dropoff.c`). With `AMS_DELIVERY_ENV` off, dropoff takes
  the `DT_NONAMS` branch: pipe to a sendmail binary via `qopen`. Recipients
  arrive as a pre-validated vector plus a message file — exactly SMTP's
  shape. **Replacing that one branch with an SMTP module captures the entire
  send path.**

* **The packaging seam.** The messages GUI codes against an ATK class `ams`
  (`atkams/messages/lib/ams.ch`, ~60 methods). Two implementations exist:
  `amsn.do` (direct-linked nosnap — what we run) and `amss.do` (SNAP RPC, not
  built), selected at runtime (`SetWantSnap`). A third backend is the
  architecturally sanctioned extension point.

* **Design properties in our favor.** The spec declares all MS_ calls
  idempotent and the architecture built so "a server can die at any moment,
  with the client automatically reconnecting ... without any loss of
  context." `MS_GetPartialBody`'s offset/remaining/count semantics are
  IMAP partial FETCH, near verbatim. The store also assumes concurrent
  multi-process writers (AFS heritage: advisory locking, master update
  files, `MS_ReconstructDirectory`), which matters for the sync
  architecture in §2.

## 1. Operation mapping (was: "confirm AMS does these")

All confirmed. The mapping table:

| AMS operation | IMAP equivalent |
|---|---|
| Message directory (full path name) | Mailbox; synthesize path-like names, e.g. `/IMAP/fastmail/INBOX` |
| `MS_HeadersSince(dir, date64)` → snapshots | `UID FETCH` ENVELOPE/FLAGS/INTERNALDATE; build snapshots client-side (caption recipe: `bldcapt.c`) |
| `MS_GetPartialBody(dir, id, off, ...)` | `UID FETCH BODY.PEEK[]<offset.length>` |
| `MS_AlterSnapshot` (OR/AND attribute codes) | `UID STORE ±FLAGS` |
| `MS_CloneMessage` (COPY / COPYDEL / APPEND...) | `UID COPY` / MOVE / COPY + `\Deleted` |
| `MS_AppendFileToFolder` | `APPEND` |
| `MS_PurgeDeletedMessages` | `EXPUNGE` — AMS's mark-then-purge model is identical to IMAP's |
| `MS_CreateNewMessageDirectory` / `MS_RenameDir` / `MS_RemoveDirectory` | `CREATE` / `RENAME` / `DELETE` |
| Subscriptions (`MS_Get/SetSubscriptionEntry`, map files) | `LSUB` / `SUBSCRIBE` / `UNSUBSCRIBE` |
| `MS_GetNewMessageCount`, `MS_NameChangedMapFile` | `STATUS (MESSAGES UNSEEN)` per subscribed folder |
| Attributes UNSEEN / DELETED / REPLIEDTO | `\Seen` (inverted) / `\Deleted` / `\Answered` |
| 168 user-defined attributes per folder | IMAP keywords |
| `~/Mailbox` ingestion → `MS_ProcessNewMessages` | INBOX polling (messages' timer/checkpoint infra already polls) |
| Send: `MS_SubmitMessage` → `dropoff()` | SMTP submission |

Drafts, tmp files, and the raw-file calls (`MS_GetPartialFile`,
`MS_StorePartialFile`, `MS_UnlinkFile`) stay on the local filesystem exactly
as nosnap does today; IMAP is not involved.

## 2. Store architecture: local-cache sync vs live backend

Two viable architectures. **Decision: start with A (cache/sync); keep B as a
stretch goal.**

### A. Local cache + sync agent (the Thunderbird model) — chosen

Keep `libmssrv.a` and the on-disk store (`.MS_MsgDir` index + body files)
untouched: the existing local store *is* the cache. A new sync module mirrors
IMAP mailboxes into a dedicated mspath element (e.g. `~/.IMAP/fastmail/...`)
and pushes local changes back up.

Note the structural rhyme: the AMS store is already what later clients
reinvented as their cache layer — one body file per message plus a fast
index (`.MS_MsgDir` ≈ maildir + Thunderbird's index database). We are not
bolting a cache onto AMS; we are recognizing that AMS ships one.

* messages/cui need **zero changes** to browse synced mail; the folder tree,
  subscriptions, new-mail counts all work through existing machinery.
* All UI latency stays local (the GUI assumes snapshot scans are fast).
* Offline reading is free.
* The store's multi-writer design (locks, master update files) means an
  external sync process writing while messages reads is within the original
  design envelope — this was daily reality on AFS. The only new store-side
  code is the change journal below.
* Precedent: the MIT Athena deployment already did the ingest half of this
  with a forked fetchmail depositing into `~/Mailbox`.

**Local + IMAP coexistence comes free.** In this architecture there is only
ever one backend (`amsn.do`/`libmssrv`), so local folders and any number of
IMAP accounts coexist as **sibling mspath roots** — the mspath was designed
to present multiple message-directory trees side by side (that is exactly
how `$LOCAL`/`$EXTERNAL`/`$OFFICIAL` bboards coexisted with personal mail).
No dispatch layer, no per-process backend switch.

**Mirror into a sibling root, not into `~/.MESSAGES` itself.**
`~/.MESSAGES` is the personal-mail root with its own Mailbox-ingestion
semantics. A separate root per account (`~/.IMAP/fastmail/`) gives clean
account boundaries, makes a UIDVALIDITY-forced re-mirror a contained event,
and leaves local/legacy mail untouched.

**INBOX policy — the one open decision for milestone 3.** Either:

* *Mirror* INBOX like any other folder — server stays authoritative,
  flags/deletes sync both ways. Cleaner steady state; start here.
* *Ingest* — keep the fetchmail model: new INBOX mail is moved into
  `~/.MESSAGES/mail`, which is where `MS_ProcessNewMessages` and the FLAMES
  classification hooks run. Preserves FLAMES filtering at the cost of
  emptying the server-side INBOX.

Could become a per-account preference later; v1 picks mirroring (one less
moving part), with FLAMES-on-mirrored-folders revisited afterward.

Sync state: because AMS ids are synthesized from UIDVALIDITY+UID (§3), the
id itself carries the server mapping — no separate mapping database.
Writeback needs change capture at the few local mutation points
(`MS_AlterSnapshot`, `MS_CloneMessage`, `MS_PurgeDeletedMessages`,
`MS_AppendFileToFolder`): a small per-account change journal appended by
libmssrv, consumed by the sync agent. Phase one can defer writeback
entirely (one-way sync, flags pushed later).

Known costs: duplicate storage; a staleness window; UIDVALIDITY resets
force a folder re-mirror; conflict policy (server wins, journal replays on
top) must be written down early.

### B. Live IMAP backend behind MS_* (`amsi.do`) — stretch goal

Implement the 25–30 live MS_ calls directly against IMAP in a new
`libmsimap.a`, packaged as `amsi.do` beside `amsn.do`. True thin client, no
duplicate storage, flags visible instantly. Costs: GUI latency tied to the
network; offline breaks; must faithfully reproduce the chunked snapshot
protocol and random-access calls (`MS_GetNthSnapshot`); a per-process
backend switch means local + IMAP folders don't coexist in one session
unless a dispatch layer routes by path prefix. Architecture A makes
coexistence trivial (both are just mspath roots), which is why it wins for
v1.

## 3. Message identity

Corrected: AMS ids are not 64-bit integers. An id is an **18-character
base64 string** (~108 bits; see `ams_genid`, spec in the interface doc).
That is *more* than enough to deterministically encode UIDVALIDITY (32) +
UID (32) + an account/mailbox tag — so for synced messages the AMS id can
*be* the server identity, no mapping table needed.

* Never treat IMAP sequence numbers as stable; UID-only.
* `AMS_DATE` (6-char base64) is the per-folder ordering/watermark key used
  by `MS_HeadersSince` and the "associated time" profile calls. Back it
  internally with a highest-UID watermark per mailbox; IMAP `SEARCH SINCE`
  is only day-granular.
* On UIDVALIDITY change: re-mirror the folder (ids change; captions/dates
  preserve user context).

## 4. MIME

AMS itself has an RFC822 *header* parser (`hdrparse`) but no MIME body
parser — its native rich text is ATK datastream, flagged by
`AMS_ATT_FORMATTED`. However the 6.3-era messages already composes MIME
(`sendmsg.c`, `writeone.c`) and displays foreign types by shelling out to
**metamail** (`mailobj.c`) — Borenstein's own reference implementation.

* v1: fetch full RFC822 (`UID FETCH ... BODY.PEEK[]`) and feed the existing
  pipeline unchanged. No MIME parser work.
* Side quest: get metamail building on macOS for non-Andrew attachment
  display.
* Later optimization only if needed: BODYSTRUCTURE + selective part fetch.
* 2026-07-19: inline HTML display (MIME body plumbing + htmlview
  modernization) is now a post-milestone-5 objective of its own —
  see roadmap.md → "Objective: HTML mail rendering".

## 5. Connection handling and performance

* Persistent connection per account; one selected mailbox at a time.
* Reconnect-and-retry wrapper around every command — matches the MS layer's
  own idempotency doctrine.
* Cache per mailbox: UIDVALIDITY + highest-UID watermark (in the synced
  folder, alongside the store's own associated-time profile data).
* Incremental sync via `UID FETCH <watermark+1>:* (FLAGS ENVELOPE
  INTERNALDATE)`. IDLE, CONDSTORE/QRESYNC are later niceties, not v1.

## 6. Authentication and OAuth2

Nothing reusable exists (`MS_CheckAuthentication` is AFS/Kerberos-era).
Token handling is new code. The `ams` class already has
`GetStringFromUser(..., IsPassword)` for prompting.

Milestone order:

1. **App password + `LOGIN` / `AUTH PLAIN` first.** Fastmail requires app
   passwords for third-party IMAP/SMTP clients; its OAuth client
   registration is not generally open to third parties (re-verify current
   policy when we get there). This gets end-to-end working credentials with
   zero OAuth infrastructure.
2. **XOAUTH2 second** (`AUTHENTICATE XOAUTH2` / SMTP `AUTH XOAUTH2`, payload
   `user=<u>^Aauth=Bearer <tok>^A^A`), with refresh handled by the auth
   module; needed for Gmail-class providers. Design `imap_auth.c` so both
   modes sit behind one interface from day one.
3. Re-auth mid-session without losing mailbox state (the reconnect wrapper
   from §5 gives this for free).

Fastmail endpoints: `imap.fastmail.com:993` (SSL), `smtp.fastmail.com:465`
(SSL) or `:587` (STARTTLS); proxy hostnames `imaps-proxy` /
`smtps-proxy.fastmail.com` if ports are blocked.

## 7. Implementation modules, grouped by milestone dependency

The modules below are NOT a prerequisite chain; each milestone needs only
its own group. Sending and storing are independent seams.

### Milestone 1 — SMTP send (self-contained; no IMAP modules involved)

* `tlscon.c` (`overhead/mail/lib`) — protocol-neutral TLS socket wrapper:
  connect, `SSL_connect`, line-buffered read/write, timeouts. The shared
  transport module (earlier drafts called it `imap_transport.c`); IMAP
  uses it at milestone 2 but SMTP is its first client. TLS via Homebrew
  OpenSSL (consistent with the XQuartz dependency posture). v1 uses port
  465 implicit TLS — no STARTTLS state machine.
* `netrc.c` (`overhead/mail/lib`) — ~/.netrc credential lookup with
  permission checking; shared with IMAP at milestone 2.
* `smtpsub.c` — the dialogue (EHLO/AUTH PLAIN/MAIL/RCPT/DATA),
  dot-stuffing + CRLF conversion, reply parsing, mapping onto the `D_*`
  codes `dropoff()`'s callers expect. Preference gate + credential-file
  location via the existing `getprofile()` preference machinery.
  Implementation spec: `revival/doc/claude-history/smtp-send-prompt.md`.
* The `dropoff()` hook — a few lines in the `DT_NONAMS` branch of
  `overhead/mail/lib/dropoff.c`, behind a preference, with the sendmail
  pipe kept as fallback.
* Imakefile plumbing for OpenSSL include/lib paths.

Testable with a standalone driver program before cui or messages enter the
picture.

### Milestone 2–3 — IMAP browse + one-way sync

* `imap_protocol.c` — tagged commands, line/literal parsing, response
  routing. (Depends on `imap_transport.c` only.)
* `imap_sync.c` — mailbox mirror into the account's mspath root
  (architecture A), snapshot synthesis, watermark tracking.

### Milestone 4 — writeback

* Change journal: small additions at the four mutation points in
  `ams/libs/ms` (§2A); journal replay added to `imap_sync.c`.

### Milestone 5 — OAuth2

* `imap_auth.c` — materializes only here: app-password and XOAUTH2 behind
  one interface, token refresh. Until then auth stays inline.

What we do NOT need at any milestone: AMDS delivery, SNAP, and the
maintenance surface (`MS_Rebuild*`, `MS_Scavenge*`, `MS_ConvertOldMail`,
`MS_Epoch`, bboard machinery) — stub or leave untouched since libmssrv
remains in place.

## 8. Library question — DECIDED 2026-07-17: hand-roll

The milestone-2 spike (`src/overhead/mail/lib/imapspike.c`, spec at
`revival/doc/claude-history/imap-spike-prompt.md`) ran the full sequence — CAPABILITY,
LOGIN, LIST, EXAMINE, UID SEARCH, ENVELOPE fetch, full-body fetch through
a real 9,686-byte literal — against the live Fastmail account. Verdict:
nothing IMAP-inherent was hard enough to justify a library. Tagged/untagged
routing was easy; literals were mechanical once `tlscon_ReadBytes` existed
(the one additive transport extension, regression-proven); ENVELOPE
parsing took a ~120-line scanner. libetpan (BSD-3, drifting) and UW
c-client (Apache 2.0, unmaintained) would each bring porting baggage
comparable to the hand-rolled effort without solving the one real gap
found (below).

Spike findings that set milestone 3's opening tasks:

* **Transport gap (fix first):** `tlscon`'s fixed ~4KB line buffer
  overflows on real-mailbox responses (`UID SEARCH ALL` on a
  3,939-message INBOX returns every UID on one line), and a failed
  ReadLine leaves the connection unrecoverably wedged — no resync
  primitive. `imap_protocol.c` needs a growable line buffer and a
  connection-recovery design before anything else is built on it.
* **Sync should use the modern extensions Fastmail advertises:**
  `ESEARCH` (compact ranges instead of exhaustive UID lists) and
  `CONDSTORE` (`HIGHESTMODSEQ` arrives free in the EXAMINE response) —
  not the naive SEARCH-ALL/FETCH-ALL pattern the spike used.
* **Known stub:** a literal appearing *inside* an ENVELOPE response is
  unhandled (never occurred against Fastmail); the real parser must
  weave byte-counted reads into token-level parsing.
* Revised milestone-3 estimate from the spike: ~1,500–2,500 LOC for
  `imap_protocol.c`, ~1,000–2,000 for `imap_sync.c` (where the design
  risk lives); multiple days, not one.

## 9. Test strategy

* **cui** (`build/bin/cui`, already built) is a scriptable command-line
  client exercising the same CUI_/MS_ surface — the harness for driving
  sync and store behavior end-to-end before the GUI is in the loop.
* vui is not built (`MK_VUI` off) and stays out of scope.
* Fastmail test account with app password; a scratch folder tree so
  UIDVALIDITY and expunge cases can be forced.

## 10. Milestones

1. **SMTP send** — `smtpsub.c` behind `dropoff()`, app password, against
   Fastmail. Small, self-contained, useful immediately, required by every
   architecture. **DONE 2026-07-17**: end-to-end cui→SMTP→Fastmail
   acceptance passing (`revival/tools/smtp-send-test`). The acceptance
   push also surfaced and fixed three latent bugs: NULL address-validation
   results crashing cui, an arm64 variadic-ABI corruption via fdplumb.h's
   open-rename macro, and obsolete client-side dest-host DNS validation
   (now defaults off when `smtphost` is set; `validatedesthosts` pref
   overrides).
2. **Spike** — hand-rolled IMAP transport+protocol: connect, LOGIN, LIST,
   SELECT, FETCH one message. Go/no-go on §8. **DONE 2026-07-17**:
   full sequence ran against live Fastmail (read-only), decision is
   hand-roll; transport findings recorded in §8 as milestone 3's
   opening tasks. Spike driver committed as
   `src/overhead/mail/lib/imapspike.c` (`make imapspike.test`).
3. **One-way sync** — split into three sub-deliverables (2026-07-17
   pacing decision):
   * **3a — transport + protocol layer. DONE 2026-07-18.**
     `tlscon_ReadLineAlloc` (growable, drain-before-refill — the
     spike's wedge case `UID SEARCH ALL` now survives a 22KB response
     line); `imap_prot.[ch]`, the tree's first born-ANSI module
     (spec: `revival/doc/claude-history/imap-protocol-prompt.md` — full-prototype
     header, scanf banned, ESEARCH-aware UID search, streamed body
     fetch, IMAP_DEAD/imap_Reopen reconnect contract with
     UIDVALIDITY verification); `revival/tests/imap-protocol-tests`
     9/9 live, both SMTP suites still green.
   * **3b — DONE 2026-07-18** (spec: `revival/doc/claude-history/imap-sync-prompt.md`;
     commits ea8a6eb389, 28ff9f9e47, 852d887bb1 + close-out).
     `imapsync` (`src/ams/msclients/imapsync/`) mirrors folders into
     `<root>/.MESSAGES/<folder>` through the store's own code via one
     additive MS entry point (`MS_AppendFileToFolderWithId`).
     Deterministic ids f(UIDVALIDITY,UID) in base32hex — mixed-case
     base64 collided on APFS case-insensitive filenames. Flags via
     `MS_AlterSnapshot`; CONDSTORE/HIGHESTMODSEQ skip (modseq compare
     only); `-full-check` expunge marking (never purges); empty-body
     fetches (live Fastmail expunge-during-FETCH race) are
     per-message skip-and-retry with the uid watermark capped below
     the skipped uid. `revival/tests/imap-sync-tests` 6 cases live,
     incl. scripted cui browse; real-mailbox browse in `messages`
     confirmed by hand via `mspath: $default:~/.IMAP/fastmail/.MESSAGES`.
   * **3c (next)** — messages-GUI acceptance: folder visibility
     without Expose All (subscription defaults for mirrored roots),
     broader browse/read behavior; metamail MIME display noted as a
     separate pre-existing platform gap.
4. **Writeback** — change journal + replay: flags, copy/move, append,
   delete/expunge.
5. **XOAUTH2** auth module + token refresh.
6. Stretch: `amsi.do` live backend (§2B); IDLE for push-style new-mail.
