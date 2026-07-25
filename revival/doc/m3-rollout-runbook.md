# M3 rollout runbook: `ansify` per-subtree K&R→ANSI conversion

Procedure for executing one "M3 rollout point" (`roadmap.md` →
Medium-term → ANSI C conversion → M3 — Definition conversion). Read
`rollout-procedure.md` first (shared M1–M4 session rhythm) and
`porting-assessment.md` §14 (tool design, the `ansify` pipeline, the
delegation ruling) before starting. This file covers only what's
specific to M3: the flag/tool mechanics, the gate-scope reasoning, the
ordering, and lessons carried forward from M2. Written 2026-07-25,
planning only — no M3 execution has started yet (see "Resource note"
at the end).

## What's different from M2

M2 fixed missing declarations by hand: grep the real definition, then
write or extend an `extern`/`#include` — a discovery-and-judgment task
per instance, which is why it needed 13 delegated sessions across 29
directories. M3 converts K&R definitions to ANSI using `ansify`, a
tool with its own per-file compile gate and auto-restore
(`porting-assessment.md` §14) — mechanically closer to "run the tool,
read its report, triage what it flags" than "investigate and write
code" for the bulk of instances. Two judgment categories remain and
are explicitly NOT delegable (§14's "Delegation" ruling):

- **DRIFT** — a `.ch`-vs-`.c` argument-count or type disagreement.
  These are real bugs (the `CUI_GetHeaders` class of finding), not
  conversion noise; the fix direction needs a human ruling on which
  side is stale, same as any M1/M2 hard stop.
- **Parser bailout** — the strict file-local-helper parser refuses to
  guess and reports instead of converting. Needs a human read of the
  actual declaration before deciding how to proceed.

## Mechanics (per subtree)

1. Add `-pe` to the directory's `CLASSFLAGS`, alongside the `-pi` M1
   already left tree-wide. Check first whether `CLASSFLAGS` composes
   here the way M1 assumed (`$(CLASSINCLUDES) -pi`) or needs a
   restated line the way M2's `COMPILERFLAGS` did — confirm via
   `grep -n CLASSFLAGS Makefile` after regen, same verification M1/M2
   both used.
2. Force `.eh` regeneration (remove the stale `.eh` before `depend`/
   `install` — same class of step as M1's `.ih` force-regen).
3. `ansify --dir <directory>` — runs the pipeline: `fix-static-methods`
   → `fix-missing-static-decl` → class-method/classproc rewrite by
   signature-DB lookup (never inference) → file-local helper
   conversion from the file's own K&R declaration block → per-file
   `make base.o` gate with automatic restore on failure.
4. Triage the DRIFT report and any parser-bailout report. These are
   the only outputs needing top-level (non-delegated) judgment.
5. Subtree-local gate: `make clean && make depend && make -k install`
   — see "Gate scope" below for when a tree-wide gate is also
   required.
6. Runtime check → user confirmation → commit (src, then docs — same
   two-commit convention as M1/M2).

## Gate scope — stronger locality guarantee than M2 had at the start

`.eh` files are never installed to `build/include` (`andrew.rls` has
only the `.ch.eh:` *build* rule, no install rule for `.eh`; `build/
include` contains zero `.eh` files) and are reached only via a
same-directory quoted include — confirmed in `atk/eq/eq.c`:
`#include "eq.eh"`. `-pe`'s typed prototypes can only ever be checked
against the class's own `.c` file(s) in the same directory: there is
no mechanism for this flag's fallout to reach another directory's
build, structurally, not just empirically. This is a stronger
guarantee than M2 had at the outset — M2 needed four real batches
before ruling subtree-local gating sufficient. M3 can start from
subtree-local-only as the default and skip that proof phase.

Recommend keeping a tree-wide gate for the same two directories M2
singled out for the same reason (belt-and-braces insurance on
consequence/defect-density, not expected mechanism leakage):
`atkams/messages/lib` (the `messages` GUI backend) and `contrib/zip/
lib` (tree's highest-defect-density directory) — plus fixed milestone
checkpoints (end of each dependency-order wave below, and end of M3
overall, which doubles as M3's completion gate before M4). Everything
else: subtree-local only.

## Ordering — dependency order matters here, unlike M2

M2 ruled ordering didn't matter (proven no cross-directory blast
radius either direction). M3's `roadmap.md` entry already specifies a
dependency order: overhead → atk/basics+support → atk/text → insets →
apps → atkams/ams → contrib. Worth keeping even though the `.eh`
locality argument above means it isn't strictly required for build
correctness — reason to keep it anyway: a DRIFT finding in a
foundational directory is more likely to reveal a genuine
`.ch`-vs-real-implementation bug whose fix pattern then recurs in
dependent directories, so tackling foundational directories first
means later waves benefit from an already-established fix precedent —
the same value M1's consumer-count survey provided, for a different
reason.

## Batching — M3's per-instance cost is lower, so batches can start bigger

M2 converged on ~13 real delegated sessions for 29 directories,
batching small/leaf directories together (up to 8 per session) once
early sessions proved zero cross-directory fallout. M3's per-instance
labor is lower — the tool does the mechanical rewrite; the session's
job is mostly running `ansify --dir`, reading its report, and triaging
DRIFT/bailouts — closer to M2's later, high-confidence batches from
the very first session, not M2's early cautious ones. Propose sizing
M3 sessions by the 7 dependency-order waves directly, splitting a wave
into multiple sessions only where its own directory count or DRIFT
volume turns out large (mirroring M2's bucket-4 treatment of its 3
biggest directories) — rather than pre-committing to a small/mid/large
split before real per-wave data exists.

## First concrete step — not yet done

Get real per-directory instance counts before committing to session
sizing: `ansify --build-db` (rebuild fresh — the signature DB is from
2026-07-08 and may be stale against any file changes since) followed
by a dry-run/count pass over the tree, the M3 equivalent of M2's point
0 census. `atk/eq`'s 2026-07-08 validation (20 methods, 3 classprocs,
2 helpers converted, zero DRIFT, all artifacts restored — a dry run,
never committed) already proved the pipeline works; propose re-running
it for real as M3's actual rollout point 1 (pilot), committing this
time, to get a live session-cost/duration data point before scaling to
the rest of the plan.

## Answering the two open questions (2026-07-25)

- **Does M3 need the same subset roll-out as M2?** Yes in shape — flag
  a directory, fix fallout, gate, runtime check, commit is the same
  shared rhythm (`rollout-procedure.md`) — but the gate-scope caution
  can be lighter from the start; see "Gate scope" above.
- **Are there lessons learned that enable fewer subsets?** Likely yes,
  for two independent reasons: (1) the `.eh`-locality argument removes
  the need for M2's four-data-point proof phase, so there's no "go
  slow to build confidence" period; (2) `ansify`'s own per-file compile
  gate does most of the discovery work M2's sessions had to do by
  hand, so a session can plausibly cover a full dependency-order wave
  rather than M2's finer small/mid/large split. The first-step census
  above will confirm or correct this once real counts exist.

## Resource note (2026-07-25, wdc)

Evening-of-2026-07-22-to-now work (M2's back half plus this planning)
has consumed 78% of the weekly resource budget. This planning doc
lands now — cheap, no delegated sessions — but actual M3 execution
(the census re-run, the `atk/eq` pilot recommit, or any wave) should
wait for an explicit go-ahead rather than starting immediately behind
M2's close-out.
