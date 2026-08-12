# M2 rollout point 4h: `contrib/zip/lib`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (the `cd`-persistence check: verify early
with a throwaway `cd`+`pwd` pair; if it doesn't persist across Bash
calls in your session, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's malloc-family blind-spot note — use
`grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`
(word-boundary-anchored, space-before-paren tolerant), not a plain
substring `grep "malloc("`/`"realloc("`, per the `atk/table` session's
correction. Read `m2-rollout-runbook.md` in full, and all eleven prior
rollout reports (`claude-history/m2-pilot-eq-REPORT.md`,
`claude-history/m2-batch2-REPORT.md`, `claude-history/
m2-batch3a-REPORT.md`, `claude-history/m2-batch3b-REPORT.md`,
`claude-history/m2-utillib-REPORT.md`, `claude-history/
m2-metamail-REPORT.md`, `claude-history/m2-text-REPORT.md`,
`claude-history/m2-rofftext-REPORT.md`, `claude-history/
m2-table-REPORT.md`, `claude-history/m2-mail-lib-REPORT.md`,
`claude-history/m2-messageslib-REPORT.md`) before starting. The
taxonomy has three validated categories with sub-cases plus several
later sub-shapes — see the runbook's "Fallout taxonomy" section in
full, including `atk/table`'s `AUXMODULE`-guarded double-underscore
sub-case (this directory has many small classes with their own `.ch`/
`.eh` files — check for this shape if a class-method macro here ever
calls a double-underscore name directly rather than dispatching
indirectly). "Possible genuine bug/typo" is still empty across 2063
instances/26 directories. Don't assume it stays empty — still `grep`
to confirm before writing any declaration.

**This is the tree's known highest-defect-density directory** — per
`porting-assessment.md` §17's Xft bug history, not a casual
description. There is also a **known, unresolved, pre-existing
rendering bug** here (a solid-black render at `-O` optimization only,
found during figure/inset rendering work, root cause not found after
extensive prior `lldb` tracing — see the `project_zip_inset_status`
history if you have access to it, otherwise trust this citation).
**This bug is a rendering/runtime issue, unrelated to M2's
declaration-only fixes, and out of scope for this session** — do not
attempt to investigate or fix it. If your own fix-surfacing or
determinism builds behave strangely in a way that looks related
(e.g. a crash specifically tied to `-O` vs `-O0`), stop and flag it
explicitly in the report rather than either chasing it or silently
ignoring it — M2's own build passes use `CDEBUGFLAGS="-ferror-limit=0
-g -O0"` for the fix-surfacing pass specifically, which should sidestep
this pre-existing issue, but confirm the final determinism builds
(which use the directory's normal, unmodified optimization level)
don't newly trip over it either.

**No generated-source gap** — no `Parser()`/`Lex` directives in the
Imakefile (confirmed by reading it directly before writing this
prompt; still verify yourself).

**Structural note**: this is the largest bucket-4 directory by file
count so far — 41 `.c` files, stale estimate 141 (comparable to
`atkams/messages/lib`'s 140, which came in at 336 real). Structurally
similar to `atk/table`/`atkams/messages/lib`: 20 separate
`DynamicObject`/`DynamicMultiObject` class targets (`zip.do`,
`zipv.do`, `zipedit.do`, `zipprint.do`, `zipstat.do`, `zipobj.do`, and
14 more single-class `.do`s for individual drawing-primitive types —
`ziporect`, `zipocirc`, `zipoelli`, etc.), no `LibraryTarget` at all.
Expect the same "several files behind one `.do`, sharing no internal
header" cross-file pattern documented repeatedly in prior sessions.
Given the file count, expect this to take multiple correction rounds —
normal, not a sign of trouble.

## Task

1. Confirm the no-generated-source status and the all-dynamic build
   shape by reading the Imakefile yourself.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them.
5. Fix fallout per the runbook's taxonomy.
6. **Unconditionally sweep every `.c` file in the directory** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, using the
   word-boundary-anchored pattern above.
7. Rebuild clean (subtree-local), twice, to confirm determinism.
8. **Then run the full tree-wide gate**: `make Clean && make
   dependInstall` from `andrew-6.4/src/`, logged to the fixed
   `andrew-6.4/dependInstall.log` path (never a scratch path), and
   confirm 0 `error:` lines beyond the 4 known pre-existing baseline
   errors every prior M2 session has documented (recognizer-type false
   positive, `ams/msclients/nns`'s SSLLIB link failure,
   `contrib/zip/utility/ltapp.c`'s two int-conversion errors — note
   the last one is in `contrib/zip/utility`, a *different* directory
   from this session's `contrib/zip/lib`, don't confuse the two or
   assume it's yours to fix). Never run multiple simultaneous build
   invocations against this checkout (check `ps aux` first). If you
   background this build, use the Bash tool's own `run_in_background`
   mechanism directly — do NOT wrap it in an additional shell-level
   `nohup ... &`, which defeats the tool's own completion tracking (a
   real mistake the `atkams/messages/lib` session made and had to
   self-correct via `ps aux` PID-polling).

## Gate

**Both gates required for this directory**, per the explicit exception
in `m2-rollout-runbook.md`: subtree-local (`make clean && make depend
&& make -k install`, twice for determinism) AND the full tree-wide
gate (`make Clean && make dependInstall`) at the end. Do not skip the
tree-wide gate here.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-ziplib-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 141), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions — specifically confirm whether
  `atk/table`'s `AUXMODULE` sub-case recurs here, given this
  directory's many small classes.
- Full tree-wide gate results — exit status, `error:` line count
  (should be exactly the 4 known baseline errors, confirm each by
  citation), and confirmation this directory's own build span contains
  none of them.
- **Exact runtime-check commands for wdc.** This directory backs the
  `zip` drawing/CAD inset — identify the most informative real
  exercise path (inserting the inset, drawing a few primitive shapes
  matching a sample of the touched `.do`s, save/reload). Explicitly
  remind wdc that the known solid-black-at-`-O` rendering bug is
  pre-existing and unrelated to this session's fix, so it shouldn't be
  mistaken for a regression if encountered.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied, and whether you
  noticed any difference in permission-prompt frequency (context: the
  orchestrating session confirmed auto mode eliminated subagent
  permission prompts entirely as of the prior session — if that holds
  here too, say so; if not, describe exactly what differed).
