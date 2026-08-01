# Activating a new subtree (standing playbook)

Procedure for bringing a currently-inert directory — gated off by an
`MK_*`/site.h macro, an Imakefile-level file-family macro (like
`WHITEPAGES_ENV`), or excluded some other way — into the live build for
the first time in this revival. Generalizes
`claude-history/new-tree-porting-prompt.md` (2026-07-05, the AMS_ENV/
CONTRIB activation — the original instance of this, done before M1/M2/
M3 existed as concepts) into a repeatable playbook, updated with what
those milestones now mean for a fresh activation. Standing doc — never
retire on completion, this recurs every time another gated subtree
comes alive. Read alongside `rollout-procedure.md` (the shared session
rhythm) and `sonnet-playbook.md` (delegation hard rules) if any step is
delegated.

**Core point this doc exists to make:** activating a subtree is not
just "make it compile and run." M1's typed-dispatch default is
tree-wide and automatic, so a newly-activated directory gets that for
free — but M2's implicit-declaration sweep and M3's ANSI conversion
were each a **point-in-time census** over the directories active when
that milestone ran. A directory that was inert then was never swept.
Skip steps 4–5 below and the "revival is ANSI-clean"/"revival has no
implicit declarations" claims quietly stop being tree-wide true.

## Step 0: confirm it's actually inert, not just quiet

Liveness census rule (`rollout-procedure.md`): a directory is active
iff a full gate's log contains `building (dependInstall)
(.../src/<dir>)`. Makefile presence is NOT evidence — stale Makefiles
from before a subtree was conditionalized out survive indefinitely.
`site.h`/`allsys.h` must be read together (`site.h` can override
`allsys.h`'s defaults). A `building` line proves directory *descent*,
not compilation — an imake-level file-list macro can still leave the
directory's real object list empty (`WHITEPAGES_ENV`'s
`WHITEPAGESFILES =` inside an otherwise-visited `overhead/util/lib`,
found during M3 batch O1 — the directory descends every time, only the
file family is dark).

## Step 1: flip the gate

Find the exact macro and where it's set — `config/site.h`
(`MK_*`/`*_ENV` style) or an `#ifdef` inside the directory's own
Imakefile (file-family style, like `WHITEPAGES_ENV`). Turn it on.
Regenerate Makefiles (`make Makefile` or a `make World`-adjacent step,
per whatever the surrounding build normally requires — see
`project_clean_clone_build`-class notes if starting from a very cold
tree).

## Step 2: first compile pass — the LP64 taxonomy

Same five variants `new-tree-porting-prompt.md` catalogued for the
original AMS_ENV/CONTRIB activation, still the right triage order for
a fresh subtree's first compile:

1. **Missing prototype / pointer-return truncation** — implicit-
   declaration warnings on a function that really returns a pointer.
   Fix: prototype it (header or local `extern`). Critical case.
2. **>8-arg untyped-dispatch stack spill — already fixed globally.**
   M1's tree-wide typed-dispatch default (`overhead/class/pp/class.c`,
   `usePrototypesImport` always on) means this class cannot recur in
   any newly-activated directory. No action needed — kept in the list
   only so a fresh session doesn't go looking for it.
3. **Bare `-1` literal through untyped dispatch** — no compiler
   signal; grep after it compiles (see the original prompt's grep
   pattern). Fix: `(long)-1`.
4. **`%d` with `long`/`long *` in scanf/printf** — scanf-family is
   memory corruption (fix immediately), printf-family is wrong output
   only.
5. **`long`/`int` mismatch in display positioning through untyped
   dispatch** — only relevant to UI code (view/graphic/lpair-adjacent);
   check after a visual smoke test if something draws at the wrong
   place/size.

Also check the **classpp typed-dispatch signedness mismatch** class
(`porting-assessment.md` §16) if the subtree defines any classes: a
`.ch` declaring a narrow `returns char`/`short` (or the unsigned
variants) that disagrees with the real implementation's signedness is
invisible at compile time and only breaks at higher optimization
levels. Sweep any new `.ch` files the same way §16 describes.

## Step 3: runtime check

Get it running through its real consumer app. Standard AUIS-app
cautions apply (`rollout-procedure.md`): native Terminal.app, not an
IDE terminal; AUIS terminal apps can go unkillable (`UE` state) under
sandboxed shells, so attribute hangs carefully (check `ps` etime/PPID
before assuming a build step is stuck); never save/write into a
non-source-controlled test fixture.

## Step 4: catch it up to M2 — implicit-function-declaration sweep

M2's tree-wide `-Werror=implicit-function-declaration` closeout
(2026-07-25) covered every directory active *at the time*. A
newly-activated subtree was not part of that census and starts with
zero of M2's protection. Give it the same treatment any M2 rollout
point got:

1. Add the M2 `COMPILERFLAGS` restatement (`-std=gnu89
   -Wno-implicit-int -Werror=implicit-function-declaration
   -Wno-incompatible-function-pointer-types -Wno-return-type`) — check
   whether the directory already composes this by default or needs it
   restated, same verification either milestone's runbook describes.
2. Run a `-k` build with `-ferror-limit=0` (the default 20-diagnostic
   cap truncates a single busy file otherwise).
3. **Explicitly grep the word-boundary-anchored malloc-family
   pattern** (`grep -nE '\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc
   *\('`) even if the compiler census comes back clean — these are
   clang builtins and a call with *zero* declarations anywhere in
   scope is invisible to the diagnostic itself, not just truncated.
   This blind spot has dominated a directory's real fallout before
   with no partial-warning symptom to trip over first.
4. Fix fallout per the taxonomy in `claude-history/m2/
   m2-rollout-runbook.md` (categories, sub-shapes, and the
   `AUXMODULE`-guarded-declaration case are all precedented there).

## Step 5: catch it up to M3 — ansify conversion

Same reasoning as step 4, for the K&R→ANSI conversion instead of
declaration coverage. A subtree inert when M3's per-directory batches
ran was never `ansify`-converted.

1. Confirm the signature DB is current: `ansify --build-db` if the
   subtree adds or changes any `.ch` files (the DB is a point-in-time
   snapshot of `class -D -N` over every `.ch`; it does not
   auto-update).
2. Check for `.ch` files in the directory. If none: skip straight to
   the `ansify --dir` helper-conversion pass, no `-pe`/`.eh` step
   needed (see `claude-history/m3/m3-rollout-runbook.md`'s O1 finding — not every
   directory is a class directory). If it has `.ch` files: add `-pe`
   to `CLASSFLAGS`, force `.eh` regeneration, same as any other M3
   batch.
3. `ansify --dir <subtree>` — real run, not `--dry-run` (dry-run can't
   surface compile-gate fallout, only DRIFT/instance counts).
4. Triage DRIFT and parser-bailout findings per `porting-assessment.md`
   §14/§17 — §17 in particular documents that `InitializeClass`/
   `InitializeObject`/`FinalizeObject` DRIFT reports are usually a
   tool-side false positive (classpp special-cases these three names;
   `ansify`'s DRIFT check doesn't know that), not a real interface bug
   — check the shape against §17 before escalating.
5. Subtree-local gate (`make clean && make depend && make -k
   install`, twice for determinism). Tree-wide gate only if the
   subtree turns out to be one of the two directories M2/M3 already
   singled out for extra caution (`atkams/messages/lib`,
   `contrib/zip/lib`) or shows unexpected cross-directory reach.

## Step 6: same rhythm and conventions throughout

Every step above still follows `rollout-procedure.md`'s shared rhythm
(flag → force regen → local build with `-k` → fix fallout → gate →
runtime check → commit) and its command-style guidance (separate Bash
calls, no `&&`/`;` chains, no loops/heredocs, absolute paths over
`cd`). If any step is delegated, `sonnet-playbook.md`'s hard rules
apply unchanged: no fossil commits, ever; STOP at each gate; a
`<task>-session.diff` + `<task>-REPORT.md` for review. Two-commit
convention once approved: src (the activation + fallout fixes), then
docs (tick whatever tracking doc listed the subtree as inert, add any
genuinely new finding to the relevant runbook/`porting-assessment.md`).

## Known current candidates (as of 2026-07-25)

Directories/families confirmed gated off, with their specific blocker
— not a to-do list, just what's known so a future activation doesn't
have to re-derive the reason from scratch:

- **CMU Whitepages** (`bt*`/`wp*` family inside `overhead/util/lib`,
  `WHITEPAGESFILES`) — gated on `WHITEPAGES_ENV`, never defined in
  this build. Reviving it means standing up the X.500-ish directory-
  lookup backend the format assumes, a separate project from a
  mechanical activation. `verbose.c` in the same directory is
  adjacent but distinct: excluded from `$(OBJS)` directly (not by this
  macro), old `varargs.h`-style variadic code with no mechanical ANSI
  path — needs a real rewrite to `stdarg.h`, not just activation, if
  ever revived.
- **`ness`** (`atk/ness/objects`, `atk/ness/type`) — blocked on a
  bison grammar extension (a multi-character string token specific to
  the Andrew bison fork) that has nothing to do with LP64/ANSI;
  needs a `%skeleton` implementation or a grammar rewrite before this
  playbook's steps even become relevant. `celv`'s only callers live in
  `ness/objects`.
- **`atk/controllers`, `atk/prefed`, `atk/console`, `atk/ezprint`** —
  confirmed `MK_*`/site.h-gated inert (M1 point 10's inert-directory
  survey); no specific blocker investigated yet beyond "off by
  default."
- **`contrib/mit/fxlib/server`** — missing a generated
  `fxserver_err.h` from a Kerberos code-gen step never run in this
  checkout; the file family itself (`.@`-realm-stripping fix) is
  already mechanically correct and waiting, just compile-unverified.
- **`contrib/atkbook`, `tm`, `ams/msclients/vui`/`cui`-adjacent dead
  paths, `contrib/bdffont`** — conditionalized out per
  `porting-assessment.md` §7a/§7b; `bdffont` specifically is missing
  its `bdfparse.act` action file with no surviving generator/history.

## What this doc replaces

`claude-history/new-tree-porting-prompt.md` remains as the case-study
record of the first activation (AMS_ENV/CONTRIB, 2026-07-05) — it
predates M1/M2/M3 and its own "recommended triage order" stops at the
LP64 variants (steps 0–3 above); it does not and cannot mention the
M2/M3 catch-up steps, which didn't exist yet. Point any future
activation at this doc, not that one; keep that one only as history.
