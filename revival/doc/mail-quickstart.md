# AUIS Revival: IMAP/SMTP Mail Quickstart

This guide takes a built `andrew-6.4` tree from zero to working mail:
`messages` (or `cui`) reading your real IMAP mailbox and sending through
your provider's SMTP submission server. It reflects the state of the
revival as of 2026-07-18 (milestone 3b of the AMS/IMAP project; see
`ams-IMAP-project.md` for the architecture and roadmap).

**How it works, in one paragraph.** The historical AMS delivery system
(AMDS) stays off. Instead, a sync program (`imapsync`) mirrors your IMAP
folders one-way into a local AMS message store under `~/.IMAP/<account>/`,
which `messages` and `cui` browse through the completely unmodified store
machinery — the local store *is* the cache, as in Thunderbird's model.
Outgoing mail bypasses delivery entirely: when the `smtphost` preference
is set, the send path speaks SMTP (with STARTTLS and authentication)
directly to your provider. The mirror is currently **one-way**: local
flag changes are overwritten by the server's view on the next sync, and
writeback is a later milestone.

Fastmail is the reference provider throughout; any provider offering
IMAP + SMTP submission with app passwords should work the same way.

## Prerequisites

- A built tree — see `quickstart.md` for build and X11/font setup.
  The mail pieces are built by the normal `make World`/`dependInstall`;
  `imapsync` installs to `build/bin/imapsync`.
- An **app password** from your provider (Fastmail: Settings → Privacy &
  Security → Integrations → New app password, scope "Mail (IMAP/SMTP)").
  Regular account passwords will not work with third-party clients.

## Step 1: credentials in ~/.netrc

Create `~/.netrc` **with mode 600** — the mail code refuses to read it if
it is group- or world-readable. One stanza per server, same app password
in both:

```
machine smtp.fastmail.com login wdc@fastmail.com password <app-password>
machine imap.fastmail.com login wdc@fastmail.com password <app-password>
```

```
chmod 600 ~/.netrc
```

Note the stanzas are keyed by *server hostname*, so both are required
even though the credentials are identical.

## Step 2: preferences

Add to `~/preferences`:

```
*.smtphost: smtp.fastmail.com
```

This one preference is the master switch for SMTP sending — with it set,
every AMS client's send path goes to that server instead of the
historical delivery system. It also disables the (obsolete, and on
modern resolvers unreliable) client-side DNS validation of destination
hosts; set `*.validatedesthosts: 1` if you ever want that back.

Do **not** add the `mspath` line yet — the store refuses unknown path
elements, so the mirror directory must exist first (step 4).

## Step 3: identity — the From address

AMS stamps every outgoing message's `From:` header itself (any From you
type is deleted and replaced), as `<login>@<MyMailDomain>` — and the
domain comes from the `ThisDomain` key of the **AndrewSetup**
configuration file, falling back to your machine's hostname if no such
file exists. An unset ThisDomain therefore produces
`you@your-machine.lan`, which your provider will refuse to relay to
external addresses (Fastmail: `551 5.7.1 Not authorised to send from
this header address`).

The AndrewSetup search path ends at `${ANDREWDIR}/etc/AndrewSetup`,
which the revival's `site.h` points at the build tree — so no root
access is needed. Create `build/etc/AndrewSetup` containing:

```
ThisDomain: fastmail.com
```

Your UNIX login name must match the local part of your mail address for
this to compose correctly (`wdc` + `fastmail.com` → `wdc@fastmail.com`).
The full name in the From display comes from your account's GECOS field.

## Step 4: first mirror

```
build/bin/imapsync -v
```

Defaults: account root `~/.IMAP/fastmail`, folder `INBOX`, credentials
via the `imap.fastmail.com` netrc stanza. The first run fetches every
message (a few minutes for a few thousand messages); subsequent runs are
incremental and near-instant, so re-run it whenever you want new mail.
There is no daemon mode yet — cron or a by-hand run is the current
answer.

Options:
- more folders: `-folders INBOX,Archive,Sent` on the command line, or
  the `imapsyncfolders` preference (comma-separated IMAP names)
- `-full-check`: additionally detect messages expunged on the server
  and mark the local copies deleted (never purges them); costs a full
  mailbox scan, so it is opt-in
- `-root <dir>`: mirror somewhere else (used by the test suites)

The sync is strictly read-only on the IMAP side — nothing it does can
alter or destroy anything on the server.

## Step 5: point mspath at the mirror

Now add to `~/preferences` (colon-joined with whatever your mspath
already has; if you have no mspath line, this is the whole thing):

```
mspath: $default:~/.IMAP/fastmail/.MESSAGES
```

The `.MESSAGES`-suffixed root is required — that literal path component
is how the store recognizes a message-directory tree (it is the same
convention as your personal `~/.MESSAGES`). An optional label prefix,
`$default:[fastmail]~/.IMAP/fastmail/.MESSAGES`, names the tree in
folder listings.

## Step 6: browse

```
cd build && bin/messages
```

The mirrored INBOX does not appear in the folder list by default yet:
use **Message Folders → Expose All**. (Subscription defaults for
mirrored folders are a known work item.) Captions, bodies, and
seen/unseen state all come from the mirror; re-run `imapsync` to bring
in new mail and updated flags.

## Step 7: send

Compose and send from `messages` or `cui` as normal. With `smtphost`
set, "Your message has been sent" means the provider's submission
server accepted it.

**Plain messages go out as plain text automatically.** A message with
no formatting is silently stripped and sent as ordinary text that any
mail client can read. (If your sends arrive as an empty body with a
small attachment of ATK markup, you are running a binary from before
2026-07-18 — that was a 64-bit porting bug in
`MS_GetConfigurationParameters` that made clients believe an AMS
delivery system would down-convert for them; rebuild.)

**For messages with formatting** (fonts, insets), messages asks per
send: remove the formatting, send Andrew format, or send MIME. Set

```
mailsendingformat: mime
```

in `~/preferences` to pre-answer the Andrew-vs-MIME half of that
question — MIME sends multipart/alternative with a plain-text first
part, so non-ATK readers still get something readable. Recognized
values: `mime`, `atk`, `ask` (the default).

If you want explicit per-message **Send Formatted / Send Unformatted**
menu items, enable "Send Formatted/Unformatted menus" in Set Options.
There is no hand-editable preference line for option checkboxes:
messages persists them as an opaque hex bitmask
(`messages.BinaryOptions` in `~/preferences`, written by the Set
Options interface), so the GUI is the supported way to set them.

A good first test is a message to yourself, then `imapsync -v` and
watch it appear in the mirrored INBOX.

## Current limitations

| Limitation | Status |
|---|---|
| One-way sync: local flag changes revert to the server's view on next sync; don't file into or delete from mirror folders | Writeback is milestone 4 |
| App passwords only | XOAUTH2 is milestone 5 |
| Mirrored folders need Expose All | M3c work item |
| MIME display shells out to metamail, which does nothing on this platform | Pre-existing gap, separate from the IMAP project |
| No sync daemon | Re-run `imapsync` by hand or from cron |

## Troubleshooting

- **`551 5.7.1 Not authorised to send from this header address`** —
  your From domain is wrong; see step 3.
- **`The mspath element ... does not exist`** — run `imapsync` before
  adding the mspath element, and check the path ends in `.MESSAGES`.
- **netrc silently not used / auth failures** — check `~/.netrc` is
  mode 600 and has a stanza for the exact server hostname being
  contacted (SMTP and IMAP stanzas are separate).
- **Seeing exactly what is said to the server** — set `AMS_SMTP_TRACE=1`
  in the environment before launching `messages` or `cui` and the whole
  SMTP dialogue (EHLO, MAIL FROM, each RCPT reply, the final verdict) is
  printed to stderr, with the AUTH credential line redacted and the
  message body omitted. This is the fastest way to see which identity
  the server is objecting to when it refuses a send.
- **`Can't write message to /usr/lib/sendmail`** — the send path
  believed `smtphost` was unset and fell back to the (nonexistent)
  sendmail binary. If your preferences file plainly has the line, the
  process's preference reads have gone bad; quit and restart the
  application (a failed preference load is cached for the life of the
  process) and retry.
- **Sends work to your own address but not externally** — that is the
  step-3 From-domain problem again; external relay is where providers
  enforce the header From.
