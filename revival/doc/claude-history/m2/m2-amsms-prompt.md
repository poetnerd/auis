# M2 rollout point 5: `ams/libs/ms` — final M2 directory

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (the `cd`-persistence check: verify early
with a throwaway `cd`+`pwd` pair; if it doesn't persist across Bash
calls in your session, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's malloc-family blind-spot note — use
`grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`
(word-boundary-anchored, space-before-paren tolerant), not a plain
substring `grep "malloc("`/`"realloc("`. Read `m2-rollout-runbook.md`
in full — especially its "Census" section on this directory — and all
twelve prior rollout reports (`claude-history/m2-pilot-eq-REPORT.md`,
`claude-history/m2-batch2-REPORT.md`, `claude-history/
m2-batch3a-REPORT.md`, `claude-history/m2-batch3b-REPORT.md`,
`claude-history/m2-utillib-REPORT.md`, `claude-history/
m2-metamail-REPORT.md`, `claude-history/m2-text-REPORT.md`,
`claude-history/m2-rofftext-REPORT.md`, `claude-history/
m2-table-REPORT.md`, `claude-history/m2-mail-lib-REPORT.md`,
`claude-history/m2-messageslib-REPORT.md`, `claude-history/
m2-ziplib-REPORT.md`) before starting — `m2-mail-lib-REPORT.md` and
`m2-messageslib-REPORT.md` especially: both already catalogued the
`overhead/util/hdrs/fdplumb.h` partial-wrapper-family gap (`dbg_close`,
`dbg_fclose`, `dbg_closedir`, `dbg_socket`, `dbg_vclose`, `dbg_vfclose`,
among others — the header `#define`s 16 `dbg_*` I/O wrapper names but
only declares 6) recurring from the *caller* side; this directory is
where that family is defined-adjacent and used most heavily, so expect
it to dominate the fallout here too. The taxonomy has three validated
categories with sub-cases plus several later sub-shapes — see the
runbook's "Fallout taxonomy" section in full. "Possible genuine
bug/typo" is still empty across 2319 instances/28 directories. Don't
assume it stays empty — still `grep` to confirm before writing any
declaration.

## Required background: read `claude-history/fdplumb-REPORT.md` in full before touching anything

This is not optional citation-reading — read the actual file. It is
the Gate-1 mechanism map for the `fdplumb` file-descriptor-tracking
wrapper family (`RegisterOpenFile`/`RegisterCloseFile`, the `dbg_*`
name family, the `open`→`dbg_open` macro rename) and a related
preference-blackout investigation, both already resolved and
committed 2026-07-19. Key facts you need before writing any
declaration touching this family:

- The wrappers are detect-only (a name ledger + logging), never act on
  a real fd — so a missing declaration here is an ordinary M2
  ANSI-conversion gap, not a sign of some deeper runtime hazard.
- The **historical** root hazard (documented in `revival.md`'s "Old
  bugs never found till now" and memory `project_fdplumb_include_order_abi`)
  was an include-order bug where `#define open dbg_open` could rewrite
  `fcntl.h`'s own declaration of `open()` if `fdplumb.h` was included
  before `fcntl.h` — already fixed **at the root**, inside `fdplumb.h`
  itself (it now parses `fcntl.h` before performing the rename), so
  correctly-behaving callers that just `#include <fdplumb.h>` are safe
  regardless of their own include order today.
- **Do not re-derive or second-guess this mechanism from scratch** —
  it's settled. Your job in this directory is the same as every prior
  M2 session: find each undeclared function's real definition and add
  the correct declaration. The one thing to actually watch for,
  per `m2-rollout-runbook.md`'s hard stops: if anything you find
  *looks* like it touches the include-order hazard directly (not just
  an ordinary missing `dbg_*` declaration) — escalate in your report
  rather than guessing at include order yourself.

## Scale — this is the largest M2 directory by a wide margin

892 predicted instances (stale census, ~38% of the entire tree-wide M2
total), 114 `.c` files — roughly 3x the file count of the next-largest
bucket-4 directory (`contrib/zip/lib`, 41 files, 256 real instances).
Every prior directory's pattern has been "the stale estimate is a
floor, not a target" — budget for this session running long and
needing many correction rounds; that's expected at this scale, not a
sign of trouble. The runbook's own census sampling found most
instances trace to a handful of repeating project-local functions
(`NonfatalBizarreError`, `dbg_fclose`/`dbg_close`/`dbg_closedir`,
`CloseMSDir`, `FreeMessage`, ...) missing across roughly 100 files —
confirm this empirically as you go (it's a documented hypothesis, not
a shortcut to skip individually verifying each file), and note in your
report whether a small number of fixes really did collapse a large
fraction of the total, the way the runbook's census sampling predicted.

**This directory builds a real `LibraryTarget`** (`libmssrv.a`),
consumed by 7 external directories (`ams/ms`, `ams/msclients/cui`,
`ams/msclients/vui`, `ams/msclients/imapsync`, `ams/msclients/nns`,
`atkams/messages/lib`, `rdemo/messages`) — the widest fan-out of any
M2 directory, wider even than `overhead/mail/lib`'s ~25. This is
exactly why the gate-scope ruling requires the tree-wide gate for this
session (see "Gate" below), not a smaller-scope check.

**This directory has a generated-source gap**, same shape as
`overhead/mail/lib`'s: `Parser(prsdate,)` — the live grammar source is
`prsdate.gra` (bison, not the stale inline-comment-documented
`prsdate.y` recipe from decades ago, which no longer exists in this
directory — confirm this yourself by reading the Imakefile and `ls
prsdate*` rather than trusting the Imakefile's own outdated comment).
`make depend` before `install` is required, per the standard rhythm.

## Task

1. Confirm the generated-source mechanics (read the Imakefile and
   confirm `prsdate.gra` is the real source, not `prsdate.y`) and the
   `LibraryTarget`/consumer-fan-out shape described above.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them. Given the file count, consider whether
   working through fallout in logical groups (e.g. by shared function
   family, mirroring how `m2-messageslib-REPORT.md`/`m2-ziplib-REPORT.md`
   organized their taxonomy tables) makes the pass more tractable —
   your call, but don't skip individually confirming each fix's real
   definition regardless of how you batch the work.
5. Fix fallout per the runbook's taxonomy. Confirm/refute the census's
   own "a handful of functions dominate" hypothesis empirically.
6. **Unconditionally sweep every `.c` file in the directory** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, using the
   word-boundary-anchored pattern above — at this file count, expect a
   large blind-spot population; don't let the size discourage doing
   the sweep exhaustively.
7. Rebuild clean (subtree-local), twice, to confirm determinism.
8. **The tree-wide "before" gate for this session was already run as
   the bucket-4 completion checkpoint (2026-07-25, by the
   orchestrator, green — see `m2-rollout-runbook.md`'s Status section)
   — you do not need to repeat it at the start.** You DO need to run
   the full tree-wide gate again at the **end**, after this
   directory's fixes are complete (see "Gate" below) — this is
   required both by the standing `ams/libs/ms` exception and because
   it doubles as **M2's own completion gate** before M3 can start.
   Never run multiple simultaneous build invocations against this
   checkout (check `ps aux` first). If you background this build, use
   the Bash tool's own `run_in_background` mechanism directly — do NOT
   wrap it in an additional shell-level `nohup ... &`, which defeats
   the tool's own completion tracking (a real mistake one prior
   session made and had to self-correct via `ps aux` PID-polling).

## Gate

**Both gates required for this directory**, per the explicit exception
in `m2-rollout-runbook.md`: subtree-local (`make clean && make depend
&& make -k install`, twice for determinism) AND the full tree-wide
gate (`make Clean && make dependInstall`) at the end. Do not skip the
tree-wide gate here. Confirm the same 4 known pre-existing baseline
errors every M2 gate has documented (recognizer-type false positive,
`ams/msclients/nns`'s SSLLIB link failure, `contrib/zip/utility/
ltapp.c`'s two int-conversion errors) and zero new ones — and given
`ams/msclients/nns` is one of THIS directory's own consumers, take
extra care confirming that baseline error is genuinely unrelated to
anything you touched (it's a pre-existing SSLLIB link issue,
independent of `ams/libs/ms`'s own fixes per `m2-batch3b-REPORT.md`
§6 — but re-verify given the direct consumer relationship here).

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-amsms-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 892), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case. Given the volume, a per-function/family
  grouping (like `m2-messageslib-REPORT.md`'s §4) is more useful than
  892 individual rows — use your judgment on the right level of detail,
  but don't omit the taxonomy category for any fix.
- Explicit confirmation (or refutation) of the "a handful of functions
  dominate the fallout" hypothesis from the runbook's Census section.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- Anything that looked like it touched the fdplumb include-order
  hazard directly, per the hard-stop above — even if you resolved it
  confidently, flag it explicitly rather than silently handling it.
- Full tree-wide gate results — exit status, `error:` line count
  (should be exactly the 4 known baseline errors, confirm each by
  citation), and confirmation this directory's own build span contains
  none of them.
- **Exact runtime-check commands for wdc.** This directory backs the
  message-server library consumed by `messages`, `cui`, `vui`,
  `imapsync`, `nns`, and `rdemo/messages` — identify the most
  informative real exercise path(s) across a couple of these
  consumers, not just one.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied, and whether you
  noticed any difference in permission-prompt frequency (context: auto
  mode has been active in the orchestrating session since the
  `atkams/messages/lib` session and has shown zero subagent permission
  prompts across the last two bucket-4 sessions).
- **Explicitly note that this closes M2** (pending the final tree-wide
  gate's confirmation) — this is the last planned M2 rollout point.
