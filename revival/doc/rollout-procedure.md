# Shared rollout procedure (M1–M4)

Milestone-agnostic rhythm for any "flag a directory, fix fallout,
gate, commit" rollout point in the ANSI C conversion plan
(`roadmap.md` → Medium-term → ANSI C conversion, M1–M4). Extracted
2026-07-24 from `claude-history/m1-rollout-runbook.md` once M2 needed
the same rhythm under a different flag/taxonomy — read alongside
`sonnet-playbook.md` (the standing delegation briefing) and whichever
milestone-specific runbook applies (`claude-history/m2/m2-rollout-runbook.md`, etc.).
Each milestone runbook covers what's specific to it: which flag/tool,
its own fallout taxonomy, its own hard stops. This file covers what
isn't specific to any of them.

## Session/build rhythm

One directory per session. Never run two builds at once. A batch of
related directories may share one session and one gate when they're
small/uniform enough (ruled 2026-07-09, M1 point 10) — census-first
per directory still applies even inside a batch, and builds stay
strictly serial.

Step order, regardless of milestone: flag → force regen → local
build (with `-k`) → fix fallout → gate → runtime check → commit. Each
milestone runbook fills in the mechanics of "flag" and "force regen"
for its own tool, and the fallout taxonomy for "fix fallout" — the
surrounding rhythm doesn't change.

**Generated-source directories need `depend` before a subtree-local
build.** Found M2 pilot session, `atk/eq`, 2026-07-24: a directory
using `Parser()`/bison (or, presumably, `LexFile`/flex) wires its
generated header/source only into the `depend::` Makefile target, not
`all`/`install`. `make clean` deletes the generated files; a bare
`make -k install` afterward hits a *fatal* "file not found" on the
generated header before the compiler ever reaches whatever real
diagnostic the rollout point is looking for — silently hiding
fallout behind an unrelated failure. The tree-wide gate never hits
this (`dependInstall:: depend` in `config/imake.tmpl` always runs
`depend` first), only a subtree-local recipe that skips straight to
`install`. Use `make clean && make depend && make -k install` (or
`make -k dependInstall`) for any directory-local rebuild, not just
`make clean && make -k install`.

## Command style — minimize permission-prompt interruptions

wdc is supervising these sessions live and wants to be able to let a
session run longer unattended. The single biggest lever for that is
*how* commands are issued, not which commands — confirmed empirically
2026-07-24 across the M2 pilot through batch 3B: the user's
`settings.json` allow-list covers every individual verb this rhythm
needs (`cd`, `make clean/depend/install/dependInstall`, `grep`, `sed
-n`/`sed -E`, `echo`, `cat`, `wc`, `head`/`tail`, `nm`, `otool`, `ps`/
`pgrep`, `sort`/`uniq`/`cut`, `fossil status`/`diff`/`extras`), but a
compound command — anything chained with `;`/`&&`, a `for`/`while`
loop, or a heredoc — is matched as one opaque string against the
allow-list, not decomposed into its parts. A `;`-chain built entirely
from individually-allowed verbs still prompts, every time, because the
whole string doesn't itself match any single rule. A prompt-type
PreToolUse hook was tried as a fix and failed (it can restrict but
can't grant extra trust to suppress the dialog) — there is no config
lever left, only a command-shape one:

- **Issue build steps as separate tool calls, not chained with `;` or
  `&&` — and that includes `cd`.** `make clean`, `make depend`, `make
  -k install`, `grep -c "error:" ...`, `grep -n "error:" ...` each
  individually match an allow rule and won't prompt *if issued as
  separate Bash calls*. Chaining them into one `cd DIR && make clean
  && make depend && ...` line turns all of them into a single
  unmatched string. **Correction (2026-07-24, `overhead/mail/
  metamail/metamail` session):** even a bare two-part `cd DIR && make
  -k install ...` still prompts, every time, despite both halves
  individually matching an allow rule — the session's own transcript
  showed this exact shape repeated ~10 times, all prompted. The Bash
  tool's working directory *can* persist across separate calls
  (confirmed 2026-07-24 in an interactive orchestrator session: a bare
  `cd` call followed by an unrelated later call with no `cd` in it
  landed in the same directory) — so `cd DIR` into its own call, once
  per directory, then issue every subsequent command with no `cd`
  prefix at all, relying on the persisted cwd, *where that persistence
  holds*. **Correction (2026-07-24, `atk/text` session):** it does
  NOT hold in every session type — a delegated background session
  found its own harness resets the working directory between every
  Bash call (its own system instructions said so explicitly: "cwd
  reset between bash calls... use absolute file paths"), so a bare
  `cd` followed by a bare `make Makefile` failed with "No rule to make
  target" because the cwd had already reverted. **Default to `make -C
  <absolute-path> <target>` (and absolute paths generally) instead of
  `cd`-then-bare-command** — it achieves the same "one unchained
  command per call" goal without depending on cwd persistence, and
  works whether or not persistence holds. If you're not sure which
  kind of session you're in, check early (a throwaway `cd`+`pwd` pair,
  before any real build step) rather than assuming either way.
- **Prefer the Read/Grep/Glob tools over shell `grep`/`sed -n`/`cat`
  for census and file inspection, if they're available in your
  session.** They're a different permission class from Bash and don't
  hit this gate at all. **Correction (2026-07-24, `overhead/util/lib`
  session):** Grep/Glob were not available in that delegated session
  (`ToolSearch` found neither) — only `Read` was. Don't assume Grep/
  Glob are on offer; check with `ToolSearch` early if unsure. Either
  way, the fallback that matters is the next bullet.
- **Avoid shell `for`/`while` loops and heredocs — issue one `Bash
  grep`/`sed -n` call per file or pattern instead of looping.** This
  is the fix for the single largest source of prompts seen so far
  (multi-file `for`-loop census commands like batch 3B's), and it
  works whether or not Grep/Glob are available: `overhead/util/lib`'s
  session had neither tool, used ~25 separate `Bash grep` calls (one
  pattern/file per call, never a loop) plus ~40 `Read` calls instead,
  and got zero permission prompts on any of them. A loop or heredoc is
  also the one shape that can't safely be blanket-allowed even in
  principle (a dangerous command buried inside a loop body wouldn't be
  caught by any prefix-anchored `deny` rule), so it will always need
  either an approval or a rewrite, never a config fix.
- When a genuine multi-step shell pipeline is unavoidable (e.g.,
  `grep | sed | sort > file` to build a dedup table from a build log),
  don't assume it'll just prompt once and pass — `overhead/util/lib`'s
  session saw two piped/chained `nm` calls get denied outright with no
  prompt at all (apparently how an unmatched compound command resolves
  in a non-interactive/background session, not the interactive
  "prompt and wait" behavior this doc originally assumed). Treat
  "avoid chaining" as a hard rule in delegated sessions, not a
  courtesy — reissue as separate calls rather than expecting a
  pipeline to go through after one prompt.

## Logging

ALL build output — local rebuilds and the tree-wide gate alike —
goes to the one canonical file
`~/src/AUIS/andrew-6.4/dependInstall.log`, overwritten each build.
Never invent per-step log names: the fixed path is already in the
user's permission allow-list, so reusing it keeps an unattended run
moving, and gives the user one known place to watch progress.

Always build with `-k` when collecting fallout or running a census,
not just at the tree-wide gate: without it, a directory's build stops
at its first failing file, hiding every later file's fallout until
the blocker is fixed — confirmed twice now (M1 point 10 batch 1's
`suite`, took four cycles without `-k`; M2's own pilot census,
undercounted 367/74 vs. the real 2,353/396 on the first, non-`-k`
attempt).

A census can look complete and silently not be, via more than one
mechanism — treat "the error count is 0/matches the stale estimate"
as a reason to double-check, not to stop looking. Known so far,
specific to M2's `-Werror=implicit-function-declaration` flag: (1)
clang's default `-ferror-limit=20` truncates a single file's
diagnostics past the 20th (see `claude-history/m2/m2-rollout-runbook.md`'s Census
section — pass `-ferror-limit=0` on the fix-surfacing pass); (2)
`malloc`/`realloc`/`free`/`calloc` are clang builtins, so a call with
*zero* declaration anywhere in scope does not trigger the diagnostic
at all — it's invisible to the census, not just truncated (found
`overhead/util/lib` session, 2026-07-24, via a `svcconf.c` near-miss:
four undeclared `malloc` calls, none in the error list). **This blind
spot can dominate a directory's real fallout, not just cause a
near-miss** — `atk/text`'s session (2026-07-24) found 106 more call
sites across 18 of 30 files this way, more than double the 50
census-visible instances, with *zero* prior symptom (no partial
warning, no stale wrong-typed extern to trip over — the directory's
census happened to contain no ordinary `<stdlib.h>` misses at all, so
there was no file "already getting an edit for an unrelated reason"
to trigger a look). Practical consequence, upgraded from a
conditional check to an unconditional one: **grep every file in the
directory for bare `malloc(`/`free(`/`realloc(`/`calloc(` call sites
before closing out a rollout point, not just files already getting a
`<stdlib.h>`/`<string.h>` edit** — a clean-looking census (even one
that matches the stale estimate exactly) is not evidence this
category is absent; `atk/text`'s census-visible count matched its
stale estimate exactly while missing two-thirds of the directory's
real fallout.

**Sweep pattern, corrected 2026-07-24 (`atk/table` session):** a naive
`grep -n "malloc("`/`"realloc("` substring search false-positives on
any project-local wrapper whose name ends in one of these words —
`atk/table/table.c` defines its own `myrealloc()`, and `"realloc("` is
a literal substring of `"myrealloc("`. It also misses the
space-before-paren call style (`malloc (n)`), which coexists with
`malloc(n)` even within a single file in this codebase. Use a
word-boundary-anchored, space-tolerant pattern instead: `grep -nE
"\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`, one call per
file. A directory with its own `*alloc`-suffixed wrapper (`xrealloc`,
`saferealloc`, etc.) is a plausible recurring shape, not unique to
`atk/table` — always use the anchored pattern, not a plain substring
match.

## Liveness census

A directory is in the active tree iff the gate log contains
`building (dependInstall) (.../src/<dir>)` — grep
`~/src/AUIS/andrew-6.4/dependInstall.log` after a full gate. Makefile
presence is NOT evidence: stale Makefiles from before a subtree was
conditionalized out survive indefinitely (`atkbook`, `tm`, `bdffont`,
`prefed`), and `site.h`/`allsys.h` must be read together (`site.h`
overrides — a mis-census of exactly this produced M1 batch 7's
"CONTRIB_ENV off" error, corrected same day). A `building
(dependInstall)` line proves directory DESCENT, not compilation — a
directory can be in `SUBDIRS` while its entire Imakefile body is
`#ifdef`-gated off, yielding a generated Makefile with no real
targets (M1 batch 11, `wpedit`). Confirm liveness by checking the
regenerated Makefile for real build targets, or `make -n install`
doing more than touching `install.time`.

## Gate definition

"Gate" means whatever build scope the milestone runbook specifies is
the definition of done for a rollout point — for M1 this is always a
full tree-wide `make Clean; make dependInstall` (its typed casts
propagate to every consumer); other milestones may justify a lighter,
subtree-local gate if their own runbook argues the blast radius
doesn't cross directories — that argument belongs in the
milestone-specific runbook, not here. Whatever the scope, success is
ZERO real `error:` lines in the log, **never the exit code** —
`dependInstall` keeps walking directories after one fails and has
exited 0 over real errors before (M1 point 10 batch 1). Known false
positive to ignore: the string `Internal error: unknown recognizer
type` inside a `-Wdeprecated-non-prototype` warning. Before blaming
any process for a hang, check provenance
(`ps -axo pid,stat,etime,ppid,comm`, filtering Google/crashpad noise)
— some demo steps emit alarming-but-nonfatal messages. A long-running
gate legitimately takes minutes; do not kill it.

## Wave/milestone close-out gate — full clean rebuild (adopted 2026-08-01, wdc)

Whatever gate scope a milestone's own runbook specifies for individual
batches (subtree-local, tree-wide `dependInstall`, etc.), the
close-out of each wave/milestone boundary must **additionally**
include a genuine full clean rebuild — `make Clean; make World` (or
whatever the milestone's own from-scratch equivalent is) run from the
tree root, not just the incremental tree-wide `dependInstall` used for
per-batch/wave-end gates. `dependInstall`, even run tree-wide, only
rebuilds what its own dependency tracking considers stale — it is not
proof the whole tree still builds from nothing. A full clean rebuild
is a meaningfully bigger operation (minutes, not seconds) and is not
required at every batch, only at wave/milestone boundaries. This
convention is milestone-agnostic and expected to persist past M3 into
M4 and beyond, regardless of how future milestones get batched — don't
let it get dropped just because a milestone-specific runbook doesn't
happen to repeat it.

## Runtime check

Give the user the exact command(s) to try and STOP until they confirm
visually. NEVER launch AUIS GUI/terminal apps from the harness — they
can hang unkillable (`UE` state) under sandboxed shells. Where the
directory has CLI consumers, prefer a byte-diff before/after battery
(pattern: `~/src/AUIS/test-baselines/raster-pi/run-battery.sh` —
capture baseline BEFORE flagging, run twice to prove determinism,
diff after). Log any newly noticed pre-existing bug in `roadmap.md`
Little Annoyances BEFORE flipping the flag, so it can't be mistaken
for a regression. Two more caveats (found M1 point 8, 2026-07-09):
- If directing the user to a spot in a test document via a search
  string, the string must be in the surrounding *text*, not inside an
  inset — `ez`'s search command does not descend into insets.
- Never direct the user to run a save/write action against
  `ia-archive`/`PAPERS`/etc. test documents — they are not under
  source control, and a save silently overwrites the fixture on disk
  (M1 point 8 required a Time Machine restore). Point at a scratch
  copy if a save path genuinely needs runtime coverage.

## Commit conventions

After user confirmation, two commits, terse one-line messages
matching the fossil timeline style:
1. src: the flag + any fallout fixes
2. docs: tick the rollout point in `roadmap.md`; add findings to the
   milestone runbook or `porting-assessment.md` §14 only if a
   genuinely NEW pattern appeared

`fossil status` before and after each commit.

## Delegation

Census and mechanical edits delegate well (Sonnet-class): give exact
file:line + expected text + replacement, require skip-and-report on
any mismatch. Pure audits/dry-run triage are Haiku-class. Kept at the
top level, every milestone: retype/signature rulings, hard-stop
adjudication, and anything that looks like a genuine `.ch`-vs-impl
(or declaration-vs-implementation) disagreement — those are real bugs
being found, not conversion noise, and the fix direction (which side
is stale) needs a human judgment call, not a mechanical rule. See
`porting-assessment.md` §14 "Delegation" for the original ruling this
generalizes from.

## Hard-stop reporting

Every milestone runbook lists its own hard-stop triggers, but the
handling is the same: stop, don't paper over it, report the disagreement
with both sides (what the declaration says, what the real usage/
implementation does) and let the operator rule on which side is
stale. A hard stop is a finding, not a blocker to route around.
