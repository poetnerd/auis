# M2 rollout point 4a: `overhead/util/lib`

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — it now
has a "Command style" section, added 2026-07-24, on minimizing
permission-prompt interruptions; follow it), `m2-rollout-runbook.md`,
and all four prior rollout reports (`claude-history/
m2-pilot-eq-REPORT.md`, `claude-history/m2-batch2-REPORT.md`,
`claude-history/m2-batch3a-REPORT.md`, `claude-history/
m2-batch3b-REPORT.md`) before starting. The taxonomy has three
validated categories with sub-cases plus two new sub-shapes found by
batch B (a wrapper-family header that only declares part of its
family; consumer-supplied callback interfaces with no declaring header
anywhere) — see the runbook's "Fallout taxonomy" section in full. The
"possible genuine bug/typo" category is still empty across 745
instances/19 directories. Don't assume it stays empty — still `grep`
to confirm before writing any declaration.

This is a single-directory session (bucket 4's first), not a batch.

## Important context specific to this directory

`overhead/util/lib` is where `fdplumb.c`/`fdplumb2.c`/`fdplumb3.c`/
`fdplumb4.c` actually live — the implementation behind the `dbg_*`
wrapper family that batch B found only partially declared
(`overhead/util/hdrs/fdplumb.h` `#define`s 16 names but only declares
6; see runbook taxonomy, new sub-shape). Read
`project_fdplumb_include_order_abi` background (search
`porting-assessment.md` for "fdplumb") before touching anything here —
this family has a real history of a caller-side ABI hazard from
include ordering, unrelated to M2 but worth knowing before editing
adjacent code. If fixing this directory's own M2 fallout naturally
puts you in `fdplumb.h`/`fdplumbi.h` (e.g. one of this directory's own
files calls a `dbg_*` function not declared for it), that's in scope —
but do not go out of your way to "complete" the header's declarations
for functions this directory doesn't itself call undeclared; that's a
separate housekeeping task, not this session's job, unless it falls
out naturally.

## Task

1. Check the Imakefile for `Parser()`/`LexFile` before assuming no
   generated-source gap.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. `make clean && make depend && make -k install` — issue these as
   **separate** tool calls per `rollout-procedure.md`'s new command-
   style guidance, not chained with `&&`/`;`. Pass
   `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on the fix-surfacing pass
   given this directory's stale census estimate (74) puts it well
   within range of hitting the 20-error-per-file cap in its larger
   files.
5. Fix fallout per the runbook's taxonomy, including both of batch B's
   new sub-shapes if they recur here.
6. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — `make clean && make depend && make -k
install`, zero real errors, twice for determinism. Per the gate-scope
ruling recorded 2026-07-24 in `m2-rollout-runbook.md`, this directory
does NOT require the full tree-wide gate. (If you find anything that
makes you doubt that ruling applies here — e.g. this directory turns
out to be linked somewhere unexpected — say so in the report rather
than silently running the tree-wide gate anyway or silently skipping
a concern.)

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-utillib-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 74), each fix with
  file:line + taxonomy category/sub-case.
- Anything relevant to the `fdplumb.h`/`fdplumb.c` connection noted
  above.
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **Exact runtime-check commands for wdc** — `overhead/util/lib` is a
  broad utility library (`util.h` — string/path helpers, B-tree
  routines, profile parsing, etc.); identify its actual consumers via
  `nm -g`/Imakefile `LIBS` lines rather than assuming, same as prior
  reports.
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the new command-style guidance worked in
  practice — did issuing build steps as separate calls and using
  Grep/Read instead of shell loops actually reduce permission
  prompts? This is the first session testing that guidance; the
  feedback is wanted.
