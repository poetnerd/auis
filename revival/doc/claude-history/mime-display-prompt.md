# Task: MIME body display in messages (mime-display)

Read `revival/doc/sonnet-playbook.md` first; its rules apply.

Scope: make `messages` display the body of real-world MIME mail
inline. Today a mirrored Fastmail message that is `multipart/*` (or
quoted-printable/base64 encoded) shows nothing useful — the display
path punts to metamail, which is nonfunctional on this platform.
Moved out of the HTML-rendering objective 2026-07-19 (roadmap →
"Objective: HTML mail rendering") because it doesn't need htmlview:
most HTML mail is `multipart/alternative` carrying a `text/plain`
sibling, and preferring that part makes the bulk of real mail
readable immediately. Inline *rendering* of text/html stays in the
HTML objective; this task only has to do something sane when
text/html is the only choice.

## Background and starting points

- Mirrored messages are complete RFC822 files in the local store
  (`~/.IMAP/fastmail/.MESSAGES/...`, and scratch mirrors via
  `imapsync -root`) — so this work needs **no live IMAP at all**;
  fixtures are local files.
- The display seam: `atkams/messages/lib/mailobj.c` is where foreign
  (non-Andrew) types shell out to metamail. How a body travels from
  `MS_GetPartialBody` to the visible text pane, and where the
  metamail decision happens, is Gate 1's first question.
- AMS has a header parser (`hdrparse`) but no MIME body parser
  (`revival/doc/ams-IMAP-project.md` §4). The compose side
  (`sendmsg.c`, `writeone.c`) already emits MIME — check it (and any
  metamail sources in the tree) for existing Content-Type/boundary/
  base64/quoted-printable helpers before writing new ones.
- The Andrew-native path is sacred: bodies flagged
  `AMS_ATT_FORMATTED` (ATK datastream) and plain non-MIME text must
  render exactly as they do today. The MIME path engages only when
  the headers say MIME (MIME-Version / a Content-Type that needs
  it).

## What v1 must do

1. Parse multipart structure (nested multiparts included) and each
   part's Content-Type + parameters (boundary, charset) and
   Content-Transfer-Encoding.
2. Decode quoted-printable and base64 text parts.
3. Part selection for display:
   - `multipart/alternative`: prefer `text/plain`; else fall through
     to `text/html`.
   - `multipart/mixed` (and other multiparts): display the first
     displayable text part, then one line per remaining part:
     `[attachment: <filename> (<type/subtype>, <n> bytes)]`.
     Saving attachments is out of scope.
   - `text/html` as the only choice: decode it and apply a
     **deliberately dumb interim strip** — drop `<...>` tags, drop
     `<style>`/`<script>`/`<head>` content entirely, decode the
     common entities (`&amp; &lt; &gt; &quot; &#39; &nbsp;` and
     numeric forms), collapse runs of blank lines. Label nothing;
     just show the text. This shim is explicitly scheduled for
     replacement by the HTML objective's htmlview work — keep it
     small and isolated so it can be deleted.
4. Charset: ATK's text machinery is Latin-1-era. Pass ISO-8859-1
   and US-ASCII through; convert UTF-8 to Latin-1 with `?` for
   unmappable characters (a tiny hand-rolled decoder is fine;
   no iconv dependency). Other charsets: pass through as bytes.
5. The metamail button/path stays available as the fallback for
   parts this code doesn't display — do not remove it.
6. RFC 2047 encoded-words in *headers* (Subject/From display) are
   OUT of scope for v1 unless Gate 1 finds them trivially wired at
   the same seam — if so, propose it at the gate, don't just do it.

## Placement (Gate 1 decides between these two, with reasons)

New born-ANSI module `mimepart.[ch]` (C89 prototypes, scanf banned —
same policy as `imap_prot.c`), either:
(a) in `atkams/messages/lib/` beside its consumer, or
(b) in an ams library directory where cui could later share it.
Smallest honest footprint wins; the parser must be testable without
X (a standalone test driver, not a .do).

## Testing

New suite `revival/tests/mime-display-tests` (playbook/README
conventions). Fixtures are **synthetic** RFC822 files written by the
suite — never real mail, never anything from the user's mailbox, and
no fixtures containing real addresses get committed. Cases at
minimum: alternative→plain preferred; QP decode (incl. soft breaks,
`=3D`); base64 text part; nested alternative-inside-mixed;
mixed-with-attachment listing line; html-only strip (tags/style
dropped, entities decoded); UTF-8→Latin-1 (accented chars mapped,
emoji → `?`); non-MIME message byte-identical to today's display;
ATK-datastream message untouched. For by-hand acceptance only, the
report gives the user a two-line instruction for checking a real
mirrored INBOX message in `messages` — you never open real mail
yourself.

## Gates

* **Gate 1 (STOP and report):** the display-path map with file:line
  evidence (from MS_GetPartialBody to pane; where metamail engages;
  what existing helpers the tree already has), placement decision
  (a) vs (b), and the integration plan as a list of intended edit
  sites. No code yet.
* **Gate 2 (STOP and report):** `mimepart` module + standalone test
  driver green on the fixture cases, and the display path wired for
  alternative→plain and CTE decoding; suite passing; diffs.
* **Gate 3 (STOP and report, end of task):** html-only shim,
  charset handling, attachment listing; full suite; prior suites
  (imap-sync, imap-protocol, both SMTP) still green; by-hand
  acceptance instructions; `mime-display-session.diff` +
  `mime-display-REPORT.md` in the tree root.

## Ground rules

No fossil commits. Never write outside: the chosen module location,
the minimal display-path edit sites identified at Gate 1,
`revival/tests/`, scratch dirs. Never touch `~/preferences`,
`~/.netrc`, or the real `~/.IMAP`; no live network use at all. No
concurrent builds; compile-verify every touched file against its
`fossil cat` original for new warnings. If the display path turns
out to require MS-layer or store changes, STOP and report — that's a
design conversation.

Maintain a breadcrumb handoff file at
`<scratchpad>/MIME-DISPLAY-HANDOFF.md` (coordinator supplies the
path), updated after each meaningful step.
