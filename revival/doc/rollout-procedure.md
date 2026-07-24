# Shared rollout procedure (M1–M4)

Milestone-agnostic rhythm for any "flag a directory, fix fallout,
gate, commit" rollout point in the ANSI C conversion plan
(`roadmap.md` → Medium-term → ANSI C conversion, M1–M4). Extracted
2026-07-24 from `claude-history/m1-rollout-runbook.md` once M2 needed
the same rhythm under a different flag/taxonomy — read alongside
`sonnet-playbook.md` (the standing delegation briefing) and whichever
milestone-specific runbook applies (`m2-rollout-runbook.md`, etc.).
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
