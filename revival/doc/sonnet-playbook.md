# Delegated-session playbook (read this first)

You are a delegated instance working on the AUIS/Andrew 6.4 revival.
This file is the standing briefing; a companion `*-prompt.md` file
gives your specific task. Read both fully before doing anything.

**How wdc launches a session with this:** start a fresh Sonnet
session in `/Users/wdc/src/AUIS/andrew-6.4` and say:
"Read revival/doc/sonnet-playbook.md and revival/doc/<task>-prompt.md,
then begin. Stop at Gate 1." One task per session; do not mix tasks.

## The project in three sentences

AUIS (Andrew User Interface System) is a 1980s–90s CMU C codebase —
compound-document editor (`ez`), mail reader (`messages`/`cui`), a
Class object system with generated headers — being revived on macOS
arm64 with XQuartz. The tree is `/Users/wdc/src/AUIS/andrew-6.4`
(Fossil SCM; `src/` sources, `build/` the installed tree,
`revival/` docs/tests/tools). Most bugs found this year are LP64
porting classes: 1991 code that assumed int == long == pointer.

## Hard rules

- **No fossil commits, ever.** At session end run
  `fossil diff > <task>-session.diff` (in the tree root) and write
  `<task>-REPORT.md` beside it. Leave the working tree as-is; revert
  nothing unless your prompt says so.
- **Gates are STOP points.** Task prompts number their gates. At each
  gate: write/update the report, then stop and say you have stopped.
  Do not continue past a gate without new instructions, even if the
  next step seems obvious.
- **Never** edit `~/preferences` or `~/.netrc`; never print
  credentials or AUTH lines; test emails go ONLY to wdc@fastmail.com;
  anything touching IMAP is strictly read-only on the server side.
- C style: match the surrounding file (K&R definitions, tabs).
  Brand-new standalone code is ANSI C89. The scanf family is banned
  in new code. Code comments must stand alone: no references to
  milestones, gates, prompts, revival/doc, or "the spec".
- Builds: never run two builds concurrently. Subtree rebuild is
  `cd src/<dir> && make install`. A full build
  (`make dependInstall >& dependInstall.log`, ~4 min, from the tree
  root) is rare — ask at a gate before doing one.
- Never `cp` over an installed binary (macOS codesign cache kills the
  process); use `install -m 755 src dst`. No `kill -9`. Run GUI AUIS
  apps from native Terminal.app, not an IDE terminal. X11 commands:
  `DISPLAY=:0; command` (separate statement, not inline env).
- `messages`, `ez`, `help` are symlinks to `runapp`; `cui` → `cuin`.
  Dynamic code loads from `build/dlib/atk/*.do` (the mail store logic
  used by messages is in `amsn.do`). After changing
  `ams/libs/ms` or `overhead/mail/lib`: `make install` there, then
  `make install` in `atkams/messages/lib` (relinks amsn.do) and
  `ams/msclients/cui` (relinks cuin).

## LP64 bug classes to keep in mind

When a value reads as a huge positive number, or lldb shows
`0xFFFFFFFFxxxxxxxx`, suspect these before inventing a new theory:
1. Undeclared pointer-returning function → return truncated to int.
2. `%d` with a `long*` in scanf-family → writes 32 of 64 bits.
3. Untyped Class dispatch sign-extension (fixed tree-wide, but old
   call sites can linger).
4. Callee declares `int *` out-param, caller passes `long *` → store
   fills the low half only (found 2026-07-18 in
   `MS_GetConfigurationParameters`; no compiler warning exists).
5. Writable-string-literal mutation → EXC_BAD_ACCESS code=2 at a
   `strb` instruction, faulting address inside a readable C string.

## Verification tools

- Suites (Python, all under `revival/tests/`): `imap-sync-tests`,
  `imap-protocol-tests`, `smtp-protocol-tests`. Run them with the
  interpreter directly; they print PASS/FAIL per case.
- `revival/tools/smtp-send-test wdc@fastmail.com` drives the real cui
  binary through a complete SMTP send.
- `AMS_SMTP_TRACE=1` in the environment makes any AMS client print
  the SMTP dialogue to stderr (AUTH redacted, body omitted).
- Crash capture without debug info:
  `lldb --batch -o run -k 'thread backtrace' -k 'register read x0 x1 x2 x19 x20 x21 x22' -k quit <binary> -- <args>`.
  To read globals at a point: `-o 'b <function>' -o run -o finish -o
  'p (long)<global>'`. Prefer register/memory reads over `expr` with
  struct types (no debug info in these binaries).

## Report format (`<task>-REPORT.md`)

1. Status: which gate you are stopped at.
2. What you did, in order, with the actual commands and the relevant
   output excerpts (evidence, not narrative).
3. Findings/answers to the questions the prompt poses.
4. Files touched (with per-file compile status) — or "none".
5. Open questions / anything that surprised you.
