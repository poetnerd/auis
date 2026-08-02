# M4 rollout runbook: tree-wide compiler strictness

Procedure for M4 (`roadmap.md` → Medium-term → ANSI C conversion →
M4 — Global strictness). Read `rollout-procedure.md` first (shared
M1–M4 session rhythm) — it still applies once real fixing batches
exist, just not from session 1 the way M2/M3 needed it. Written
2026-08-01, immediately after M3's close-out; no M4 execution has
started yet.

## Verified starting state (2026-08-01, checked against the real tree, not assumed)

`src/config/darwin/system.mcr`'s global `COMPILERFLAGS` still fully
suppresses all four target warnings:

```
COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
```

The "ratchet each completed subtree" language in `roadmap.md`'s M3
bullet never actually executed — `grep -rl "strict-prototypes"
--include=Imakefile src/` returns zero matches, tree-wide. M2 left 28
directories with a narrower per-directory override
(`-Werror=implicit-function-declaration` only, the other three flags
still suppressed); M3 didn't add to or change that. **M4 starts from a
clean slate, not a partially-done ratchet** — don't assume any
directory is pre-verified against the full flag set just because it
went through M2 or M3.

## Why this milestone didn't PRE-build a directory batch map (unlike M2/M3) — updated 2026-08-01

The original plan below argued M4 might not need a directory batch map
at all: it's flipping a global default in one file, on a tree that's
already had two full milestones of preparation (M1's typed dispatch,
M3's ANSI conversion), so the flip might land close to clean. **Phase 1
ran and disproved that** — 1,778 real errors across 83 of 91
directories, comparable in scale to a meaningful fraction of M2's
original census, not a small residual. Per this section's own original
fallback plan ("if Phase 1 turns up enough fallout to need batching,
build that map then, from real per-directory counts"), that map now
exists: **`m4-batches.md`**, built entirely from real Phase 1 error
counts, not a guessed proxy. 24 batches across the same 7
dependency-order waves M3 used.

(Original reasoning, kept for the record: M2 and M3 needed per-directory
rollout because each directory's fallout was independent and
unpredictable in advance, and the fix itself required per-instance
judgment. `m3-batches.md`'s own first draft over-plans before checking
real scope — 24 batches, more checkpoints than M2's 13, before anyone
had verified M3's actual footprint was even comparable to M2's — and
had to be redone once corrected. M4's plan tried to apply that lesson
by not pre-building a map at all, deferring to a real census first.
That part of the lesson held: the map wasn't built until real numbers
existed. The part that didn't hold was the *guess* that M4's residual
would be small — coincidentally, the corrected batch count landed on
the same number, 24, as `m3-batches.md`'s own over-eager first draft,
this time for real reasons: 8 outlier directories each needing their
own session, not an estimation error.)

## Execution mechanism corrected 2026-08-02 (wdc) — per-batch overrides, not a standing global flip

Phase 1 (below) flipped `system.mcr`'s **global** default directly to
get the real census. That's a fine one-time diagnostic technique — it
answers "what does the whole tree look like under the new flags" in a
single build — but it is the **wrong mechanism for landing the actual
fix**, and using it that way is what produced the broken-tree state
wdc had me revert on 2026-08-02: a global default change makes every
one of the 91 directories fail at once, so the tree stays fully
unbuildable from the moment it's committed until every one of the 24
batches is fixed — a single long unbuildable window with no safe place
to pause, unlike M2 or M3.

M2 and M3 never had this problem because they never touched the global
default until the very end (M2 still hasn't — see "Verified starting
state" above). Each flagged only its own directory via a **per-directory
`COMPILERFLAGS` override that restates the whole flag set** (`COMPILERFLAGS`
doesn't compose the way `CLASSFLAGS` does — a directory override must
restate all of it, not just append, or it silently loses `-std=gnu89`
and the other suppressions; same mechanical note M2's own runbook
carried). **M4 now uses that same mechanism for Phase 2** — the global
flip moves to the very end (Phase 3), after every batch is already
fixed, where it's a no-op confirmation rather than a breaking event.
Consequence: the tree stays fully buildable after every single M4
commit, the same property M2/M3 always had, and Phase 2 can pause
between any two batches indefinitely (across a resource-budget
boundary, a week, whatever) with zero risk — exactly the concern that
prompted this correction.

Per-batch override line (restate in full, same four flags Phase 1
proved safe minus `strict-prototypes`):

```
COMPILERFLAGS = -std=gnu89 -Wno-return-type -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-function-pointer-types -Werror=implicit-function-declaration -Wformat
```

Note this is a **separate `-Werror=` flag per warning name, not a
comma-separated list** — `-Werror=a,b,c` is silently treated as one
unrecognized warning option and does nothing (found and fixed live
during Phase 1's first attempt; the "unknown warning option" text
appears as a warning, not an error, so it's easy to miss — always spot
check one real file compiles clean with the exact flag string before
trusting a build's silence).

## Task breakdown

### Phase 0 — Pre-flip audit — RUN 2026-08-01, results below

Specific known risks flagged in M3's own closing lessons
(`claude-history/m3/m3-c2-REPORT.md` §21), scoped before touching any
flag:

- **`ams/libs/ms`'s silent parser gap — CLOSED.** Read
  `m3-ams1-REPORT.md` §4 directly: all 16 functions were hand-converted
  that same session (verified against `ansify`'s own formatting
  conventions), plus 16 more stale cross-file forward declarations for
  those same functions. Both subtree gates (this directory and
  `ams/msclients/cui`) ran clean twice. Nothing left to do here.
- **`FreeMessageContents` — FIXED, ruling CONFIRMED (2026-08-01, wdc).**
  §6 of `m3-ams1-REPORT.md`: giving the function its real 2-arg
  prototype surfaced a real ~30-year-old bug — `unscrib.c:163`'s
  `UnformatMessage` was calling it with only 1 argument (the 2nd,
  `FreeSnapshot`, was reading garbage under K&R's no-arity-check
  calling convention). Fixed as `FreeMessageContents(Msg, FALSE)` —
  don't free the snapshot during an in-place reformat, since `Msg`
  survives and is reused and the snapshot summary shouldn't be
  invalidated by a reformat that doesn't change the message's actual
  content. wdc's ruling confirms this reading and adds the likely
  history: `FreeSnapshot` was probably added to the signature later for
  other call sites that do need it, and this one call site simply never
  got updated — a missed update at one site, not an unfinished
  `UnformatMessage`. Verified still applied in the tree
  (`unscrib.c:163`). Closed, no further action.
- **`fix-missing-static-decl`'s non-idempotency — ruled out of scope
  for M4.** This is an `ansify`-pipeline bug; M4 does not re-run
  `ansify` or perform any further K&R *conversion* (that was M3's job,
  now closed). The bug is real and worth fixing before any *future*
  K&R conversion work, but nothing in M4's own task list exercises it.
  No action needed now.
- **Directory liveness — RE-CHECKED, no new surprises.** A `make -n
  install` dry run is not, on its own, a reliable liveness signal — it
  reads as trivial ("touch install.time / install.doc") for directories
  that are fully up to date from the last build, live or not (confirmed
  directly: `overhead/util/lib`, the most heavily-worked M2/M3
  directory, gives the identical trivial output). Corrected to a real
  signal instead: does the directory's own `.c` source actually have
  matching `.o` output from the last full build. Across all 91
  directories, only 4 show zero `.o` files: `contrib/wpedit` and
  `overhead/malloc` (both already known-inert, flagged during M3's
  O4/C2) plus `inst` (same O4 finding — grouped with `overhead/malloc`
  there) — all three reconfirmed, nothing new. One minor, harmless new
  observation: `atkams/messages/cmd/dumpbin.c` exists but is never
  compiled by the directory's `all::` target (only `NormalObjectRule()`
  is present, no `ProgramTarget` invoking it) — the directory itself is
  live (installs real fonts/templates for `messages`), this one file is
  just dead weight. No action needed.
- **NEW finding, not in M3's carried-forward list — `-Wstrict-prototypes`
  has a scope M4's original flag set didn't account for.** See its own
  section below; this is the one Phase 0 result that changes the plan.

### `-Wstrict-prototypes` — scope finding, 2026-08-01, changes the flag set below

`roadmap.md`'s M4 bullet named `strict-prototypes` as one of the four
"type-safety set" flags to flip alongside `implicit-int`,
`int-conversion`, and `incompatible-function-pointer-types`. Checked
directly before including it: `-Wstrict-prototypes` doesn't only flag
leftover K&R-style function *definitions* (the small, real residual
list — `atk/image/tif.c`'s two functions, `overhead/class/lib`'s
`class_EnterInfo`, both already known from M3 as harmless "self-healing"
parser bailouts) — it *also* flags every plain empty-parens
`extern int foo();` **declaration**, which this codebase uses
extensively and deliberately as the C89-legal "unspecified arguments"
idiom. M2 and M3 both used this idiom repeatedly and correctly on
purpose — `AMS1`'s own `moreprintf`/`errprintf2` fix (`m3-ams1-REPORT.md`
§7.3) picked it *specifically because* it's the C-standard-correct
answer for a pseudo-variadic function compatible with a later full
prototype. Flipping `-Werror=strict-prototypes` globally would turn
every one of those intentional declarations into a build error too —
there's no compiler-flag granularity that separates "leftover K&R
definition" from "deliberate unspecified-args declaration"; `clang`
raises the identical diagnostic for both.

**Measured, not guessed**: a declaration-shaped grep
(`^\s*(extern|static)?\s*(void|int|char|long|short|unsigned|struct\s+\w+
|boolean|Boolean|float|double)\s*\*{0,2}\s*\w+\s*\(\s*\)\s*;`) finds
**~6,024 matches tree-wide, ~1,055 inside installed headers alone**.
Directly confirmed with the compiler, not just the grep: compiling
`atk/value/entrtext.c` with `-Werror=strict-prototypes` fails
immediately — not from anything in the file itself, but from
`build/include/class.h`'s own `extern int class_EnterInfo();` and three
sibling declarations, a foundational header included nearly everywhere.
**The other three flags, checked the same way on three real files
across three different risk tiers (`atk/value/entrtext.c`,
`ams/libs/ms/freemsg.c`, `contrib/zip/lib/zipobj.c`, all compiled with
`-Werror=implicit-int,int-conversion,incompatible-function-pointer-types`),
all exit 0** — only benign `-Wdeprecated-non-prototype` informational
warnings, nothing blocking. Those three flags behave exactly like M4's
original "small residual" expectation. `strict-prototypes` alone does
not.

**Recommendation, not yet ruled on**: drop `strict-prototypes` from
M4's initial global flip. It doesn't close a correctness bug class the
way the other three do (M2/M3's whole finding pattern was real
~30-to-35-year-old bugs; `strict-prototypes`'s fallout here is almost
entirely intentional, correct 1988-era idiom, not latent defects) —
converting ~6,000 legitimate declarations to satisfy it would be
exactly the "wholesale modernization" this project's own strategic
decision (`porting-assessment.md` §"compiler leniency over wholesale
modernization") already chose not to do elsewhere (writable-strings,
c99). If ever wanted, it's its own separately-scoped effort, sized
similarly to M2/M3, not a same-day addition to M4's flip. The small
*definition*-only residual list (`tif.c`, `class_EnterInfo`) can be
hand-fixed on its own regardless of this ruling — those are real
leftover K&R, not idiom, and are cheap either way.

### Phase 1 — One-time census probe — RUN 2026-08-01/02, then REVERTED, results preserved below

Temporarily edited `system.mcr`'s global default directly (see
"Execution mechanism corrected" above for why this was a probe, not
the landing mechanism) to get one real, accurate, tree-wide error
count:

```
COMPILERFLAGS = -std=gnu89 -Wno-return-type -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-function-pointer-types -Werror=implicit-function-declaration -Wformat
```

`make Clean && make -k dependInstall CDEBUGFLAGS="-O -ferror-limit=0"`
(always `-k`, per `rollout-procedure.md`'s "Logging" section; always
`-ferror-limit=0` for a census — a single file's diagnostics silently
truncate past clang's default cap of 20, undercounting real volume).

**Result: 1,778 real errors across 83 of 91 directories** —
`implicit-int` 1,079, `incompatible-function-pointer-types` 515,
`implicit-function-declaration` 183, `int-conversion` 0 (clean,
confirmed tree-wide). Far past the "small residual" this plan
originally hoped for. Full per-directory counts and the batch map
built from them: `m4-batches.md`.

**Also found and fixed a real prerequisite blocker along the way**:
classpp itself (`overhead/class/pp/class.c` and `overhead/class/lib/
class.c`) failed to compile under the new flags — `static pathopen();`
missing a return type in both files, plus classpp calling `exit`/
`qsort`/`free` with no `#include <stdlib.h>` and calling `PushFile`/
`PopFile` (real functions in `classpp.l`) with no declaration anywhere
reachable from `class.c`. This cascaded into `overhead/class/testing`
failing with "command not found" since the `class` binary never got
built — masking whatever that directory's own real fallout would have
been. Fixed (mechanical, same category as any other file's fallout —
not the circular "running `ansify` on classpp" problem M3 correctly
avoided): added `#include <stdlib.h>`, two `extern` forward
declarations, two `int` return types. Verified: `class.c` compiles
clean in both directories, `class` builds and installs, `overhead/
class/testing`'s cascade failure is gone. **This fix should be
reapplied as its own first small batch when Phase 2 resumes** (it's
independent of any specific wave/batch — do it first, since every
other `-pe`/`-pi` batch depends on classpp actually building).

**Reverted 2026-08-02** (wdc, resource-budget concern — M4 alone had
already consumed 75% of the week's budget and Phase 2 couldn't
complete this week): all 33 edited files (`system.mcr`, the 29 M2-era
Imakefile overrides, both `class.c` fixes, two fossil-tracked generated
`machdep` Makefiles) reverted via `fossil revert` to the last commit.
Verified via a full rebuild afterward: back to the exact pre-M4
baseline, zero real errors (only the known `"Internal error: unknown
recognizer type"` string-literal false positive). **Nothing was lost**
— every number, every fix, and the full batch map are captured here
and in `m4-batches.md`; Phase 2 can re-derive the exact same starting
point from this doc alone.

### Phase 2 — Per-batch rollout (the actual fixing work, not yet started)

For each of `m4-batches.md`'s 24 batches, same shape as every M2/M3
session:

1. Re-apply the classpp fix first if not already done this session
   (see Phase 1 above) — everything else depends on it.
2. Add the per-directory `COMPILERFLAGS` override (full flag set
   above) to each directory in the batch's own `Imakefile`. `system.mcr`
   stays untouched until Phase 3 — every directory outside the current
   batch keeps building under the old suppressed default the whole
   time.
3. Force regen (`make clean` picks up the new `COMPILERFLAGS` on next
   build).
4. Fix that batch's real fallout. Expected shapes, per M3's own
   pattern: genuine remaining K&R spots `ansify` silently missed (the
   recurring parser-gap theme — multi-line declarations, brace-glued/
   star-glued styles, comment-continuation lines — `atk/image/tif.c`'s
   `pickTileContigCase`/`pickTileSeparateCase` already known from Phase
   0), real format-string width bugs from `-Wformat`, and any last
   `.ch`-vs-implementation DRIFT-shaped disagreements this milestone's
   stricter type checking newly exposes. M3's own experience: arity/
   type checking found genuine ~30-to-35-year-old interface bugs
   constantly, not rarely — budget for this, don't treat a "routine"
   batch as unlikely to find anything, per the C2 session's explicit
   lesson.
5. Gate: subtree-local (`make clean && make depend && make -k install`)
   always required. Tree-wide gate reserved for the same class of
   elevated-risk batch M2/M3 both used it for — `m4-batches.md` already
   flags `C1` (`contrib/zip/lib`) and `AMS2` (`atkams/messages/lib`) for
   this — plus a checkpoint at the end of each wave. `COMPILERFLAGS`
   doesn't propagate to consumers the way M1's typed `.ih` casts did
   (it only affects `.c` files compiled inside the flagged directory
   itself), so subtree-local is structurally sufficient for everything
   else, same reasoning M2 established and reused by M3.
6. Runtime check, user confirmation, commit — two commits (src, then
   docs ticking the batch in `m4-batches.md`), same convention as
   every prior milestone.

Tree stays fully buildable after every one of these commits — a
resource-budget pause between any two batches is always safe, which is
the entire point of this correction.

### Phase 3 — Global flip + cleanup (only after all 24 batches are done)

Once every batch in `m4-batches.md` is checked off: flip `system.mcr`'s
global default to the real flag set (folding `implicit-function-
declaration` in too, not just the M2-flagged directories), then delete
all the now-redundant per-directory `COMPILERFLAGS` overrides
(mirroring the cleanup M2 still owes `system.mcr` for its own
`implicit-function-declaration`-only overrides — do both at once,
they're the same mechanical cleanup) in one commit, verified by a
regenerated-Makefile diff showing no behavior change. Follow with a
full `make Clean && make dependInstall` tree-wide gate — at this point
it should be a pure confirmation (everything already fixed
directory-by-directory), not a discovery step.

### Phase 4 — Parked, not blocking M4 completion

- **c99 migration** — roadmap says "consider c99 after" conversion
  completes; M3's completion makes this newly relevant to *evaluate*,
  but it's a separate decision, not part of closing M4.
- **Writable-strings** — already deferred, unchanged by M4.

### Phase 5 — Completion gate

`make Clean && make World` (the same handoff-gate pattern M3 used at
each wave boundary and at its own close) plus a broader-than-usual
runtime pass — this is the tree's first-ever fully-strict build, wider
blast radius than any single M2/M3 batch, so the runtime check should
cover a representative sample across apps and insets (`ez`, `help`,
`messages`, `cuin`, `table`, `zip`, `calc`, `html`), not just the
directories touched in Phase 2's fixing pass.

## Carried forward from M3's closing lessons (`m3-c2-REPORT.md` §21)

- A "routine, Gate 0 only" delegation shape (dry-run first, real
  fixing pass second) earned its keep repeatedly in M3 — keep it for
  any Phase 2 batch large enough to warrant two gates.
- Directory liveness needs a positive check every time (Phase 0
  above), not an occasional deeper dive.
- Always confirm a backgrounded build is genuinely alive (`pgrep` +
  log-tail) immediately after launching it, before waiting on it — a
  tree-root-vs-`src/`-root mistake cost real time late in M3.
