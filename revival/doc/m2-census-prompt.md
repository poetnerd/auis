# Task: M2 point-0 warning census (classification only, no fixes)

Read `revival/doc/sonnet-playbook.md` first; its rules apply.
Then read the roadmap section "M2 point 0 —
`-Wincompatible-pointer-types` triage" in `revival/doc/roadmap.md`
(search for "M2 point 0") — it defines the taxonomy this task
extends. This is the census work that section explicitly marks as
delegable; the fixes are not part of this task.

## Gate 0 — a fresh build log

You need a current `dependInstall.log` in the tree root. If it is
missing or older than the latest fossil commit, ask before running
the full build (`make dependInstall >& dependInstall.log` from
`/Users/wdc/src/AUIS/andrew-6.4`, ~4 minutes, nothing else running).
STOP and ask; do not start a build unprompted.

## Gate 1 — census the two uninvestigated clusters

From the log, extract and classify **every** instance of:

1. The `int */long*` cluster (~67 instances):
   `grep -E "passing 'int \*' to parameter of type 'long \*'|passing 'long \*' to parameter of type 'int \*'" dependInstall.log`
2. The `char ** → char *` extra-`&` cluster (~18 instances).

For each instance record: file:line, caller → callee, the exact
argument expression, and a classification with one line of reasoning:

- **benign-idiom** — matches a ruling already in the roadmap's M2
  point 0 section (generic `char *`-as-void*, prefix-layout
  subtyping) or is a same-width alias that cannot corrupt;
- **suspected-real** — a genuine width or indirection mismatch that
  can corrupt on LP64 (the `oldrf.c` and `fselect.c` writeups in the
  roadmap show what a real one looks like);
- **unclear** — say what you would need to look at to decide.

Where several instances share one root shape (same callee, same
struct), group them and say so — the M1 runbook style: exact
locations plus expected text, so a later fixing pass can verify it is
editing the right thing and skip-and-report on any mismatch.

**Stretch goal, time permitting (report-only):** today's variant #5
has *no* warning — a callee declaring `int *` out-params while
callers pass addresses of `long` globals (see
`MS_GetConfigurationParameters` in `ams/libs/ms/init.c`, fixed
2026-07-18, for the shape). Grep `ams/libs/ms` and
`ams/libs/cui` for exported functions taking `int *` parameters and
check what their cuilib/amsn callers actually pass. List any
suspects; touch nothing.

STOP at Gate 1. Deliver the classified tables in the report. There is
no Gate 2 in this task — fixes are decided elsewhere.
