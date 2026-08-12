# M2 rollout point 4g: `atkams/messages/lib`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (the `cd`-persistence check: verify early
with a throwaway `cd`+`pwd` pair; if it doesn't persist across Bash
calls in your session, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's malloc-family blind-spot note, **including the
2026-07-24 sweep-pattern correction from the `atk/table` session**:
use `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`
(word-boundary-anchored, space-before-paren tolerant), not a plain
substring `grep "malloc("`/`"realloc("`. Read `m2-rollout-runbook.md`
in full, and all ten prior rollout reports (`claude-history/
m2-pilot-eq-REPORT.md`, `claude-history/m2-batch2-REPORT.md`,
`claude-history/m2-batch3a-REPORT.md`, `claude-history/
m2-batch3b-REPORT.md`, `claude-history/m2-utillib-REPORT.md`,
`claude-history/m2-metamail-REPORT.md`, `claude-history/
m2-text-REPORT.md`, `claude-history/m2-rofftext-REPORT.md`,
`claude-history/m2-table-REPORT.md`, `claude-history/
m2-mail-lib-REPORT.md`) before starting — `m2-mail-lib-REPORT.md`
especially: this directory links `libmail.a` and `libmsshr.a`
directly, so expect the same cross-directory "no header anywhere,
local extern" pattern for `CheckAMSConfiguration`/`dbg_*`/`osi_*`-style
functions that session catalogued in detail, likely recurring here
from the caller side. The taxonomy has three validated categories with
sub-cases plus several later sub-shapes — see the runbook's "Fallout
taxonomy" section in full. "Possible genuine bug/typo" is still empty
across 1727 instances/25 directories. Don't assume it stays empty —
still `grep` to confirm before writing any declaration.

**This is the highest-scrutiny bucket-4 directory so far** — not
because of size alone (23 `.c` files, 17 `.ch` classes, stale estimate
140 — third-highest in the entire M2 census after `ams/libs/ms` and
`contrib/zip/lib`), but because it **is** the `messages` GUI app's
actual backend (not just a linked consumer of it), per the gate-scope
ruling in `m2-rollout-runbook.md`. Treat every fix here with
correspondingly more care than a routine bucket-4 directory: confirm
every signature from a real definition, never guess, and flag anything
remotely uncertain rather than pushing through it.

**No generated-source gap** — no `Parser()`/`Lex` directives in the
Imakefile (confirmed by reading it directly before writing this
prompt; still verify yourself, don't just trust this citation).

**Structural note — this directory builds ~15 separate
`DynamicObject`/`DynamicMultiObject` classes, not one library**:
`text822.do`, `amsutil.do`, `ams.do`, `amsn.do`, `fldtreev.do`,
`messages.do`, `captions.do`, `msgsa.do`, `sendmsg.do`, `folders.do`,
`text822v.do`, `options.do`, `messwind.do`, `nbutterv.do`,
`mailobj.do`, `mailobjv.do` — each its own dynamically-loaded class,
most sharing helper `.o` files with a sibling (e.g. `folders.do` =
`folders.o` + `foldaux.o`; `sendmsg.do` = `sendmsg.o` + `sendaux.o` +
`writeone.o`). **`amss.do`/`amss.c` and the `S_DOBJS`/`SLIBS` are
`#ifdef SNAP_ENV`-guarded** — per `m2-rollout-runbook.md`'s own
citation of the `overhead/eli/lib` session's finding, `SNAP_ENV` is
disabled in this build. **Confirm this yourself empirically** (e.g.
check the regenerated Makefile for whether `amss.o`/`amss.do` targets
even appear) rather than trusting the citation — if it's confirmed
disabled, `amss.c` is out of scope for this session's fix (unreachable
code, matching the established "don't fix what can't compile in this
build" precedent from prior sessions' dead-`#ifdef` findings), but say
so explicitly in the report either way.

**Also worth knowing going in**: per `project_ams_imap_project`/
`project_imap_writeback_dispatch` background (referenced in prior
session context, not this directory's own docs) — this build has real
IMAP-backed mail store integration wired up as of a recent milestone,
unlike `overhead/mail/lib`'s session where "no live mail store" was
the assumption. Don't assume the runtime check has nothing real to
exercise — ask in your report whether a configured test mailbox is
expected to be available, rather than assuming there isn't one.

## Task

1. Confirm the `SNAP_ENV` disabled status and the no-generated-source
   status by reading the Imakefile and regenerated Makefile yourself.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them (see the command-style note above for
   which mechanism to use).
5. Fix fallout per the runbook's taxonomy. Given the directory's size,
   expect this to take multiple correction rounds — that's normal, not
   a sign of trouble.
6. **Unconditionally sweep every `.c` file in the directory** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, using the
   word-boundary-anchored pattern above, regardless of whether the
   census-visible error count looks complete or matches the stale
   estimate.
7. Rebuild clean (subtree-local), twice, to confirm determinism — this
   is still required here, same as every other bucket-4 directory.
8. **Then run the full tree-wide gate**: `make Clean && make
   dependInstall` from `andrew-6.4/src/` (per `rollout-procedure.md`'s
   canonical logging convention — redirect to the fixed
   `andrew-6.4/dependInstall.log` path, never a scratch path), and
   confirm 0 `error:` lines. This is the one directory-specific
   deviation from the routine bucket-4 gate — required here per the
   ruling, not optional. Never run multiple simultaneous build
   invocations against this checkout (check `ps aux` for existing
   `make`/`bison` processes first if anything seems slow or stuck).

## Gate

**Both gates required for this directory**: subtree-local (`make
clean && make depend && make -k install`, twice for determinism) AND
the full tree-wide gate (`make Clean && make dependInstall`) at the
end — per the explicit exception carved out for `atkams/messages/lib`
in the gate-scope ruling (`m2-rollout-runbook.md`). Do not skip the
tree-wide gate here the way routine bucket-4 directories do.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-messageslib-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 140), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case.
- Confirmation of the `SNAP_ENV`-disabled finding and whether
  `amss.c` needed any attention as a result (in scope or not, and why).
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Full tree-wide gate results** — exit status, `error:` line count,
  confirmation the fixed log path was used, and whether anything
  outside this directory was affected (it shouldn't be, per M2's own
  mechanism argument, but this is exactly the directory where that
  argument gets its most rigorous test yet).
- **Exact runtime-check commands for wdc.** This directory backs the
  `messages` GUI app directly — identify the most informative real
  exercise path (folder list, opening a message, composing/replying,
  the address-book/folder-tree UI elements each `.do` here backs), and
  note explicitly whether a live/configured mail store is expected
  to be available in this environment or not (ask rather than assume,
  per the IMAP note above).
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied in your session
  type and whether it held up, **and whether you noticed anything
  different about permission-prompt frequency this session** (context:
  the orchestrating session just had "auto mode" activated partway
  through this rollout; if your own session's tool calls seemed to
  need less manual approval than prior sessions' reports described,
  say so — this is a live, first-of-its-kind data point, not something
  to assume either way).
