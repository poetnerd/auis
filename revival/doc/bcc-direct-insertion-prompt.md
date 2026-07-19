# Task: why does the blind-copy direct insertion fail?

Read `revival/doc/sonnet-playbook.md` first; its rules apply.

## Background

With the "keep blind copies" option on, sending mail printed:

    Sending your BCC through the mail after error in direct insertion. (…errno, cause, via…)

The message comes from `src/ams/libs/ms/submsg.c` (search for
"after error in direct insertion", around line 391). The intended
behavior: the message store appends a copy of the outgoing message
*directly* into a local folder (the code above that message, roughly
lines 330–386, ending in `CloseMSDir(Dir, MD_APPEND)`); only when
that direct insertion fails does it fall back to mailing the user a
copy of their own message. On this system the direct path always
fails and the fallback always runs. (Historical note: until
2026-07-18 the fallback itself was broken — it put a display-form
address into the SMTP envelope — so the failure sank whole sends.
That is fixed in `overhead/mail/lib/smtpsub.c`; this task is about
making the *primary* path work.)

Setup facts you'll need: this is a non-AFS, non-AMS-delivery
machine (delivery type DT_NONAMS; SMTP via the `smtphost`
preference). The user's mail store is `~/.MESSAGES`; a read-only
IMAP mirror also exists under `~/.IMAP/fastmail/.MESSAGES` — do not
write into the mirror. The three error numbers printed in the
message are AMS_ERRNO / AMS_ERRCAUSE / AMS_ERRVIA — decode them
(`ams/libs/shr/amserr.c` and the EIN_/EVIA_ tables) rather than
guessing.

## Gate 1 — root cause with evidence

Questions to answer, in order:
1. What folder does the direct insertion target? Trace `BCCto`:
   where is it initialized (grep `BCCto` in `ams/libs/ms/`), what
   preference or default names it, and what path does it resolve to
   on this machine? Does that folder exist?
2. Reproduce the failure outside the GUI if you can. cui exercises
   the same libms in-process; check whether cui has a blind-copy
   option (grep cui sources for bcc/blind) — a cui repro beats a GUI
   one. Capture the printed error triple and decode it.
3. Root cause: missing folder? path resolution (`ResolveTildes`)?
   permission? an LP64 class bug (see playbook list)? Show the
   evidence — the failing syscall or MS error path, not a theory.
4. Propose the minimal fix. If the answer is "the BCC folder was
   never created", the proposal should say who ought to create it
   (first-use auto-create in the MS layer? documented one-time user
   step?) and why, with the exact code site.

STOP at Gate 1 with the report. Do not implement the fix in this
session unless instructed after review.
