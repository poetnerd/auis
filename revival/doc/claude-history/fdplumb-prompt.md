# Task: fdplumb "File descriptor replaced!" — investigation

> **STATUS 2026-07-19: Gate 1 CLOSED — do not run this prompt as
> written.** A Fable session answered Gate 1 statically (report:
> `fdplumb-REPORT.md`, same directory; fixes committed). Key corrections:
> the preferences fopen is raw libc, NOT dbg_fopen (config.c doesn't
> include fdplumb.h) — evidence item 2's premise below is wrong; the
> wrappers are detect-only; the "2" in the critical triple is
> opencode FOPEN, not stderr; the criticals are a known
> false-positive class (per-TU instrumentation). profile.c no longer
> latches transient load failures and warns with errno. Remaining
> scope, only if the warning ever fires: identify what the reported
> errno implicates (EMFILE vs. environment anomaly).

Read `revival/doc/sonnet-playbook.md` first; its rules apply.
**This is the subtlest of the queued tasks.** It is
investigation-gated: no fixes without explicit approval, and "I
could not reproduce it; here is what I ruled out" is an acceptable
and useful Gate-1 outcome. Do not force a theory.

## The evidence so far

1. During compose/send in `messages`, the launching terminal prints:
   `<critical:fdplumb>File descriptor replaced! Did you close
   /tmp/deKyQ1sE0U004No0UM? (5, /tmp/teKyQQ4E0U00ENo0pk, 2)`
   — `/tmp/de*` and `/tmp/te*` are draft/temp files of the send
   path; note fd **2** (stderr) appearing in the triple.
2. Separately, on 2026-07-18 one cui run had `getprofile("smtphost")`
   return NULL at submit time (send fell back to the nonexistent
   `/usr/lib/sendmail`) although the preferences file was intact and
   parsed fine standalone; the same day, a running messages re-saved
   the user's option bitmask as all-zeros (i.e. it *read* zeros).
   Both fit one mechanism: `overhead/util/lib/profile.c` caches a
   FAILED preference load for the life of the process
   (`openprofile()` returns NULL, `inited = 1`, and every later
   `getprofile()` silently returns NULL). One failed
   `fopen("~/preferences")` poisons the process permanently — and
   that fopen is fdplumb's `dbg_fopen` (profile.c includes
   fdplumb.h; the wrappers are unconditionally active).
   Full narrative: roadmap, M3c item 3 ("preference blackout").
3. A cousin bug already found and fixed in this area:
   `fdplumb.h`'s `#define open dbg_open` used to rename libc's
   *variadic* `open` declaration if fcntl.h was parsed after the
   define — silent garbage `mode` argument on arm64. The header now
   carries a comment block about it. Other renamed functions were
   judged safe then, but treat that judgment as re-checkable.

## Gate 1 — mechanism map and reproduction attempt

1. Map the ledger: read `overhead/util/lib/fdplumb.c` (and
   `fdplumb4.c`). What state does it keep per fd, what exact
   condition prints "File descriptor replaced!", and — critically —
   do the wrappers ever *act* on a mismatch (close/reuse an fd) or
   only report? Can `dbg_fopen` fail where real fopen would have
   succeeded?
2. Who are fds 5 and 2 in the message? Instrument or reason from the
   send path (`ams/libs/ms/submsg.c` GenTempName/fopen/dropoff;
   `overhead/mail/lib/smtpsub.c` opens a socket via tlscon): plot
   the open/close sequence of a send and find where a stale ledger
   entry or a genuine double-close could arise. Remember Xft/
   fontconfig and X11 open fds fdplumb never saw opened.
3. Try to reproduce both symptoms non-GUI: loop
   `revival/tools/smtp-send-test wdc@fastmail.com` (keep it to a
   handful of real sends — they land in the user's inbox; prefer a
   repro that fails before DATA, or use the suites' local fakes in
   `revival/tests/` if applicable) and watch stderr for fdplumb
   criticals; separately, exercise a send while forcing
   `refreshprofile()`-adjacent paths if you find a plausible
   trigger.
4. Report: the mechanism map, the fd timeline of one send, repro
   results, and your ranked hypotheses with the evidence for each.

STOP at Gate 1.

## Later gates (only after review)

Gate 2: confirmed root cause with a deterministic repro or an lldb
capture. Gate 3: proposed fix — candidates to evaluate honestly:
fix the actual double-close if one exists; make profile.c retry
instead of caching a failed load (small, arguably correct
regardless); demote fdplumb wrappers to detect-only if they are
found to act destructively. No implementation without approval.
