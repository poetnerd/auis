# Task: mixed-attachment placeholder renders as bare "?" (mime-attachment-icon)

Read `revival/doc/sonnet-playbook.md` first; its rules apply. This is
a follow-on to the just-closed `mime-display` task — read
`revival/doc/claude-history/mime-display-REPORT.md` in full before
starting, especially its Gate 3 "By-hand testing" section: two
unrelated pre-existing bugs (CRLF header/body-boundary blindness in
`GetHeader`, and a `text822.do` link-line underlink causing a crash on
first use of any new library symbol) were found there by the exact
method this task should reuse if code review doesn't turn up the
answer quickly — temporary `fprintf(stderr, "...")` instrumentation,
rebuilt, with wdc running `messages -d 2>/tmp/some.log` against a real
mirrored message and reporting back what the log shows. Do not assume
this bug is the same shape as either of those; it may not be.

## Symptom

`mime-display`'s Gate 3 added `multipart/mixed` handling in
`ReadMessage` (`src/atkams/messages/lib/text822.c`, the `if
(!IsAlternative && !IsDigest) { ... }` block starting around line 906
as of this writing — line numbers will have shifted): find the first
displayable text part, render it inline, and call
`InsertAttachmentLine()` (defined ~line 1653) for every other
top-level part, which should insert a line reading
`[attachment: <filename> (<type/subtype>, <n> bytes)]`.

wdc tested a real `multipart/mixed` message with one attachment. The
text part rendered correctly (paragraphs, font, content all right —
same confirmed-working machinery as the single-part case). The
attachment did not produce the expected `[attachment: ...]` line.
Instead: **a single literal `?` character, with nothing else around
it at all** — no brackets, no "attachment" text, no type or size info.
wdc's own words, verbatim, when asked to describe exactly what was
there: "literally a single '?' with nothing else around it."

## What's already ruled out

- `InsertAttachmentLine()` itself, read carefully, looks correct: a
  bounds-safe `sprintf` into a 600-byte stack buffer with `%.200s`/
  `%.100s`/`%ld` width limits, then a single
  `text822_AlwaysInsertCharacters` call. Nothing in it should ever
  produce output that's just `"?"` — its shortest possible output
  (all-NULL/empty inputs) is still `"[attachment: unnamed (unknown, 0
  bytes)]\n"`.
- Grepped `src/atkams/messages/lib/*.c` for any other literal `"?"`
  placeholder-glyph insertion (the kind of thing a broken-image/
  unknown-icon fallback might use) — found nothing relevant, only
  unrelated hits (a menu key-binding label, an unrelated string-build
  in `stubs.c`).

Given "nothing else around it," the working hypothesis — **not yet
confirmed** — is that `InsertAttachmentLine()` for this part is never
actually being reached at all, and the `"?"` comes from somewhere
else entirely (an old, unrelated code path; a font/glyph substitution
for something ATK can't render; or the attachment part failing to
parse/route the way the mixed-handling loop assumes). Do not assume
the hypothesis is right — confirm it.

## Suggested starting point

The `mime-display` session's own diagnostic method worked well twice
in a row; reuse it rather than guessing from code alone:

1. Add temporary `fprintf(stderr, ...)` instrumentation at the top of
   the `if (!IsAlternative && !IsDigest) { ... }` block, dumping
   `MixedCount` and, for each `MixedParts[mi]`, its `type`,
   `bodylen`, and whether it was selected as `textidx`. Add another
   print immediately before the `InsertAttachmentLine` call itself
   (inside the `for` loop that lists non-text parts) so you can see
   whether it's actually invoked for the attachment part, and with
   what `fname`/`ctype`/`nbytes`.
2. `cd src/atkams/messages/lib && make install` (rebuilds just this
   directory's `.do` files — see the playbook's dynamic-loading
   notes; if you add any *new* library symbol calls, check with `nm
   -m build/dlib/atk/text822.do | grep <symbol>` that it resolves,
   the way the underlink bug was caught last time — `text822.do`'s
   `Imakefile` link line was already fixed this session, but don't
   assume every future addition is safe by default given the pattern
   found).
3. Ask wdc to relaunch `messages -d 2>/tmp/attach-debug.log`, reopen
   the same test message (they'll know which one — the one they just
   tested with an attachment), and report back what the log shows.
4. Remove the instrumentation before the final report, same as last
   time (grep the final diff for your debug marker string to confirm
   zero occurrences).

## Ground rules

Same as `mime-display-prompt.md`: no live network use; you never open
real mail yourself (wdc reproduces and reports back, as above); no
fossil commits (session diff + report, not committed, per the
standing rule — reports go to `revival/doc/claude-history/` per the
amended `sonnet-playbook.md`, session diff stays in the tree root);
compile-verify every touched file against its `fossil cat` original
for new warnings before calling anything done; no concurrent builds;
never `cp` over an installed binary (`install -m 755` instead).

## Gates

* **Gate 1 (STOP and report):** root cause found, with evidence
  (debug output, not speculation). No fix yet.
* **Gate 2 (STOP and report, end of task):** fix implemented,
  verified against the same real message wdc used to report the bug,
  `revival/tests/mime-display-tests` still green (extend it with a
  fixture case for whatever the root cause turns out to be, if it's
  something `mimepart.c`'s parser could have caught), instrumentation
  removed, report + diff.
