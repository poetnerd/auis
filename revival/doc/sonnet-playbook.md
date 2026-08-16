# Delegated-session playbook (read this first)

You are a delegated instance working on the AUIS/Andrew 6.4 revival.
This file is the standing briefing; a companion `*-prompt.md` file
gives your specific task. Read both fully before doing anything.

**Two kinds of session use this playbook — check which one you are
before doing anything else:**

- **Debug/bugfix session** (the common case: one bug, one directory,
  a rollout-style batch, etc.) — work directly in
  `/Users/wdc/src/AUIS/trunk`, the canonical checkout. This is the
  default if your `<task>-prompt.md` doesn't say otherwise.
- **Project session** (larger, multi-session initiative — e.g. an
  "html" rendering effort or an "imap" effort; ask wdc if your prompt
  doesn't already name one) — do NOT work in `trunk` directly. First
  confirm with wdc which project this is for, then:
  1. `fossil branch list` — if a branch by that project's name
     doesn't exist yet, stop and ask wdc whether to create one (as a
     new branch off `trunk`'s tip) before doing anything else.
  2. If the branch exists but has no checkout yet, create one
     alongside `trunk/` and `andrew-6.4/` — e.g.
     `mkdir /Users/wdc/src/AUIS/<project> && cd
     /Users/wdc/src/AUIS/<project> && fossil open
     ~/museum/auis.fossil && fossil update <project>` — mirroring how
     `trunk/` and `andrew-6.4/` are each their own independent
     checkout directory.
  2a. **Immediately after opening a new project checkout, give it its
      own `build/` tree before running any `make` in it.** A fresh
      checkout has no `build/` at all, and skipping this step doesn't
      fail loudly — it silently builds against `trunk/build` instead
      (found and fixed the hard way in the `html` checkout, 2026-08-15;
      `fossil status` on trunk stays clean since only `build/`, never
      source, gets written, but it defeats the entire point of a
      separate branch checkout). Do this:
      1. Edit `src/config/site.h` in the new checkout: change
         `DEFAULT_ANDREWDIR_ENV` from whatever path it currently has to
         this checkout's own `<project>/build` (this file is
         fossil-tracked but the edit is checkout-local, like any
         uncommitted change — it won't affect `trunk/` unless
         deliberately committed).

         **MERGE WARNING, remember this now for later:** this commit
         to `site.h` must be committed on the project branch (so the
         branch's own build stays isolated and reproducible for
         anyone else who checks it out), but when this branch
         eventually merges back into `trunk`, the `site.h` change must
         NOT come with it — `fossil merge` will offer it like any
         other changed file, but taking it would repoint trunk's own
         `DEFAULT_ANDREWDIR_ENV` at the project checkout's `build/`
         instead of trunk's own, breaking trunk's runtime fallback for
         anyone without `ANDREWDIR` explicitly set. When merging back:
         review the merge diff for `src/config/site.h` specifically
         and revert/exclude that hunk (`fossil revert
         src/config/site.h` after the merge, before committing the
         merge, restores trunk's own version) rather than accepting it
         wholesale with everything else the merge brings in.
      2. **Do not run `make World`/`xmkmf` from scratch to populate
         `build/`.** A genuinely empty `build/` hits a real chain of
         historical Imake `SUBDIRS`-ordering bugs (`class.h` needed by
         `machdep` before `lib` installs it; then the same pattern
         again for `andrewos.h`'s whole family; more layers kept
         surfacing each time the previous one was patched) — this
         project has apparently never been bootstrapped from a truly
         virgin `build/` before, only ever incrementally rebuilt on
         top of one that already existed. Chasing this layer-by-layer
         is a real time sink with no guaranteed bottom.
      3. Instead: copy `trunk/build` wholesale (`cp -R
         /Users/wdc/src/AUIS/trunk/build build` — `trunk/build` is
         ~58MB, cheap to copy, and byte-identical in content to what a
         correct `<project>/build` would contain, since all source is
         shared except `site.h`). Then reinstall this checkout's own
         `site.h` on top: `chmod u+w build/include/site.h && cp
         src/config/site.h build/include/site.h` (trunk's installed
         headers are read-only, so a plain `cp` without the `chmod`
         fails).
      4. Regenerate every subdirectory's `Makefile` so they pick up
         the corrected `BASEDIR`: `cd src && make Makefiles`.
      5. Verify isolation before trusting it — don't just assume the
         fix took: `grep BASEDIR` in some subdirectory's freshly
         regenerated `Makefile` (e.g.
         `src/ams/libs/shr/Makefile`) and confirm it names this
         checkout's own `build/`, not `trunk/build`. Only after that
         should you build/test anything for real.
      One caveat that's expected, not a bug: XQuartz's X11 font path is
      a server-wide resource, not per-checkout, and will refuse to
      register a second directory of same-named fonts. Whichever
      checkout's fonts got registered first (normally `trunk/`'s) is
      what X11 actually uses for on-screen rendering regardless of
      which checkout a given app binary was built/run from — this is
      fine to leave as-is (a real "split brain" between trunk and
      branch for fonts specifically, everything else fully isolated),
      not worth chasing further.
  3. If a checkout for that project already exists, `cd` into it and
     confirm `fossil status` shows you on the right branch before
     starting work — and confirm its `build/` isolation (2a.5 above)
     is actually in place before assuming it is; don't take it on
     faith just because the checkout itself exists.
  As of 2026-08-15, `html` (an HTML-mail-rendering effort) exists as a
  real branch+checkout with its own isolated `build/`, see
  `revival/doc/html-mail-rendering-design.md`. `imap` (deeper
  AMS-over-IMAP work) is still just a candidate, not created.

**How wdc launches a session with this:** start a fresh Sonnet
session in the tree identified above (`trunk/` for a debug session,
the project's own checkout for a project session) and say:
"Read revival/doc/sonnet-playbook.md and revival/doc/<task>-prompt.md,
then begin. Stop at Gate 1." One task per session; do not mix tasks.

## The project in three sentences

AUIS (Andrew User Interface System) is a 1980s–90s CMU C codebase —
compound-document editor (`ez`), mail reader (`messages`/`cui`), a
Class object system with generated headers — being revived on macOS
arm64 with XQuartz. The canonical tree is `/Users/wdc/src/AUIS/trunk`
(Fossil SCM; `src/` sources, `build/` the installed tree,
`revival/` docs/tests/tools) — project-branch work happens in its own
sibling checkout directory instead, see above. Most bugs found this
year are LP64 porting classes: 1991 code that assumed int == long ==
pointer.

## Hard rules

- **No fossil commits, ever.** At session end run
  `fossil diff > <task>-session.diff` (in the tree root) and write
  `<task>-REPORT.md` to `revival/doc/claude-history/` (not the tree
  root — that's where findings live, active or retired; see that
  directory's `README.md`). Leave the working tree as-is; revert
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
  `cd src/<dir> && make install` — but that only updates the running
  binary for **dynamically-loaded** code (`.do` files). For a change
  inside `atk/basics`, `atk/support`, `atk/supportviews`,
  `atk/utilviews`, `atk/text`, `atk/frame`, `atk/ez`, or `atk/tscript`,
  `runapp` statically links that subtree's `.a` (see `ATKLIBS` in
  `src/atk/apps/Imakefile`) — `make install` rebuilds the archive but
  does NOT relink `build/bin/runapp`. See the relink recipe below.
  (Unsure whether a directory is static or dynamic? `nm -g
  build/bin/runapp | grep YourChangedFunction` — a defined `T` means
  static, no match/`U` means it's loaded from a `.do`.) A full build
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
- **Relinking `runapp` after a statically-linked-subtree change**
  (previous bullet): don't run a full `dependInstall` just to test
  one file. Faster incremental sequence, e.g. for
  `atk/basics/x/xgraphic.c`:
  1. `cd src/atk/basics/x && make xgraphic.o`
  2. Rebuild that subtree's `.a`. Same directory as the `.o` for most
     of the list above (`make libfoo.a`) — except `atk/basics`, whose
     `.o`s are split across `x/`/`wm/` but assembled in the sibling
     `lib/`: `cd ../lib && rm libbasics.a && make libbasics.a`. Never
     `ar clq libbasics.a x/xgraphic.o` directly on the existing
     archive — it clobbers every other object already in it; always
     go through the owning directory's `make libfoo.a`, which does
     `rm $@` then rebuilds from every object.
  3. `cp` the rebuilt `.a` to `build/lib/atk/` and `ranlib` it there
     (a plain `.a` isn't the codesign-cache case above, `cp` is fine).
  4. `cd src/atk/apps && make runapp`, then
     `install -m 755 runapp ../../../build/bin/runapp` (from the tree
     root: `build/bin/runapp`) — not `chmod`+`cp`, same reason as
     above.
  If the sandbox denies that install step, hand the exact command to
  the user to run rather than retrying or asking whether a rebuild
  recipe exists somewhere — this is it.

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
6. A new variadic function called through a K&R (empty-parens) extern
   declaration → arm64 passes variadic args on the stack but fixed
   args in registers, so an under-declared call site emits the wrong
   calling convention: silent garbage args, no compiler warning.
   Different symptom signature from 1-5 above: a crash *inside*
   `vsnprintf`/`vfprintf`/similar, one frame below a plausible call
   site. Fix is a full prototype with `...` at every call site, not
   just the definition (found 2026-07-22 in `MSJournal_Record`).
7. A binary file format's on-disk header struct declared with `long`
   fields, read/written with a hardcoded byte count (not `sizeof()`) →
   on LP64 the struct is 2x the size the format's real bytes need,
   every field after the first lands at the wrong offset, and any
   field the hardcoded count doesn't reach (e.g. a trailing `height`)
   is left as raw stack/heap garbage. Symptom: a read that returns
   success but is immediately followed by a huge or negative-looking
   allocation request — check the size argument before chasing a leak
   elsewhere. A close cousin: a hand-rolled "N bytes at a time" loop
   using a pointer cast to `long *`/`unsigned long *` with address
   arithmetic sized for 4 bytes (`& ~3`, `-4`, decrementing by pointer
   width) — on LP64 each step silently touches 8 bytes, corrupting a
   small *fixed* number of bytes at each end of a buffer while leaving
   the middle correct. Neither has a call boundary for the compiler to
   typecheck — no warning either way. Found 2026-08-08 in
   `convertraster`'s RF-format reader (`oldrf.c`/`rastfile.h`); see
   porting-assessment.md §22. Worth checking first in any other
   binary-format reader that predates this port and hasn't been
   exercised yet — a good candidate list is the `image` inset's format
   importers (`gif.c`, `tif.c`, `pcx.c`, `sunraster.c`).

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
- **When a headless test driver constructs real ATK dataobjects**
  (tables, styled text, embedded views -- anything beyond plain
  string/tree manipulation), give it a subcommand that calls the
  object's real `Write()` (see `htmlatktest.test writeds` in
  `src/atkams/messages/lib/htmlatktest.c` for a working example: parse/
  render into a fresh object, then `text_Write(obj, fopen(outpath,
  "w"), 1L, 0)`) to emit an actual, directly-openable ATK datastream
  file, not just an internal text-dump format. This matters for two
  reasons, both learned the hard way during the html-atk-renderer
  Gate-5-follow-up bug hunt (2026-08-16, National Grid mail
  mis-rendering): (1) a human can open the file straight in `ez` for
  real visual inspection, which caught a real bug (stray UTF-8 control
  bytes) that the driver's own internal dump missed entirely --
  printf's `%s` silently truncates at embedded NULs, hiding exactly
  this class of defect from a text-based dump no matter how carefully
  you read its output; (2) reproducing a live-app bug via a *file*
  that opens identically in two independent apps (there, `messages`
  vs `ez`) is a fast, decisive way to tell whether a bug is general
  (ATK's own object/view code) or specific to the one app you first
  saw it in -- far cheaper than chasing it live in the original,
  more complex app. A companion `roundtrip` subcommand (`filetype_Lookup`
  + `class_NewObject` + `dataobject_Read`, then reuse the same dump
  logic) is equally cheap to add and isolates Read()-time bugs from
  draw-time-only ones, entirely offline, no X11 needed for either.

## Report format (`<task>-REPORT.md`)

1. Status: which gate you are stopped at.
2. What you did, in order, with the actual commands and the relevant
   output excerpts (evidence, not narrative).
3. Findings/answers to the questions the prompt poses.
4. Files touched (with per-file compile status) — or "none".
5. Open questions / anything that surprised you.
6. For rollout-batch tasks (M3 etc.): end with a "Suggested runtime
   checks for wdc" section — the exact commands wdc can run to
   exercise this batch's directories, per `rollout-procedure.md`'s
   Runtime check rules (`nm -g` against `runapp`/the relevant `.do`
   first to find live consumers; never launch GUI apps yourself; no
   saves against unversioned fixtures — point at scratch copies).
