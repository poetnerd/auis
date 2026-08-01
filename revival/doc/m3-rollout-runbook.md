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

## Current standing per-batch checklist (live — edit in place)

Added 2026-07-30 (wdc-approved amendment): the net list of standing
checks a batch must run, maintained here so a fresh session doesn't
have to reconstruct it from the findings narrative below. When a
check is added or retired, edit this list in the same commit that
documents the finding.

Active:

1. **`.ch` presence** (O1): `find <dir> -maxdepth 1 -name '*.ch'` —
   a directory with no `.ch` files skips the `-pe`/`CLASSFLAGS` and
   `.eh` force-regen steps entirely; DRIFT is structurally impossible
   there.
2. **Predefined-macro typo grep** (O4): single-underscore
   misspellings of compiler-predefined macros (`_STDC_`,
   `_cplusplus`, `_FILE_`, ...) — fix on sight as ordinary fallout;
   a dead `#ifdef` branch flipped live gets full fallout triage, not
   a free pass.
3. **Empty-parens lifecycle-method grep** (O4; confirmed a real,
   deterministic compile failure under `-pe` in B3):
   `grep -nE '__(InitializeClass|InitializeObject|FinalizeObject)\(\s*\)' <dir>/*.c`
   — invisible to `ansify`'s candidate detector; hand-fold to ANSI
   with a named (unused) `classID` param.
4. **Installed-header grep for converted non-static helpers** (O3 —
   the one documented hole in the `.eh`-locality gate-scope argument
   below): before treating a directory's blast radius as accounted
   for, grep the installed header tree for an empty-parens
   declaration of any non-static helper being converted; flag any
   cross-directory header touch prominently in the report.
5. **Restated-lifecycle-param `.ch` check** (B3): a `.ch` restating
   `InitializeClass`'s (or both of `FinalizeObject`'s) implicit
   param(s) over-counts through classpp's ordinary prototype loop —
   check the real `.c` param count before ruling either way. Known
   unresolved instances ahead: `fldtreev.ch` (Wave 6), `schedv.ch`/
   `ltv.ch` (Wave 7).
6. **Concurrent-commit merge check** (B1): if unrelated commits land
   while a batch's review is in progress, spot-check the current
   content of any file both touched before trusting the auto-merge
   and committing.
7. The milestone-agnostic standing checks in `rollout-procedure.md`
   still apply (liveness census, `-k` on fallout collection, the
   anchored `malloc`/`free`/`realloc`/`calloc` grep before
   close-out, runtime-check rules).
8. **Stranded old-style forward declaration vs. newly-ANSI'd narrow
   parameter** (T1; grep broadened AMS1 2026-07-31; comma-list blind
   spot and same-file/cross-file scope added AMS2 2026-08-01): `ansify`
   converts K&R function *definitions* to ANSI but doesn't touch
   separate, old-style (empty-parens) forward *declarations* of the
   same name earlier in the file. Normally harmless — but once the
   definition gains a narrow by-value parameter (`char`, `short`,
   `unsigned char`, or a `typedef` of one — **resolve typedefs, don't
   read the literal keyword**; `Boolean` (`typedef short Boolean`) is
   the standing example), the stale declaration and the new definition
   become a real ISO C conflicting-types error (default-argument-
   promotion incompatibility). Grep a directory for
   `(static|extern)\s+\w[\w ]*\s+\w+\(\);` (bare empty-parens forward
   declarations — **both keywords, not just `static`**; AMS1 found 10
   of 12 real conflicts were `extern`-prefixed, which the original
   `static`-only pattern structurally could not match) before or after
   a batch's `ansify` pass, and cross-check any hit against its
   matching definition's parameter types. **The grep only catches
   single-name declarations** (`NAME();`) — AMS2 found real conflicts
   hiding inside **multi-name comma-list declarations**
   (`extern int A(), B(), C(), ...;`, up to 19 names on one line in
   `ams/libs/cui/cuilib.c`) that a name-per-line pattern structurally
   can't see; check every name in any comma list against its real
   definition, not just lines that already look like a single bare
   declaration. **The hazard is same-translation-unit only**: AMS2
   found two cross-file instances (declaration in one `.c`, narrow-
   typed definition in another, e.g. `folders.c`'s forward reference to
   `foldaux.c`'s `ConsiderResettingDescription`) that never conflict at
   compile time because the two types are never visible together in
   one file — worth noting for completeness but not a Gate-1 blocker;
   only flag same-file pairs as must-fix.
9. **Definitive completeness re-scan for large directories** (AMS1,
   2026-07-31): `ansify`'s own report of what it converted/skipped/
   drifted is not proof of completeness — `parse_decl_block` silently
   drops (zero skip, zero DRIFT, no trace at all) any K&R declaration
   block that isn't exactly "one complete, semicolon-terminated
   statement per physical line" (a single declaration spanning
   multiple lines, or multiple declarations crammed onto one line, both
   qualify and are common in this 1988-era codebase). Ad-hoc greps for
   this shape are unreliable — AMS1 needed three successive attempts,
   each missing real instances, before a **definitive** check closed
   it: import `ansify`'s own `HDR`/`parse_decl_block` (the real
   functions, not a re-derived approximation) and run them directly
   against every file in the directory, diffing against what the real
   `ansify` pass actually converted. Any directory large enough that a
   manual per-function review isn't practical should run this before
   considering the directory converted — **C1 (`contrib/zip/lib`,
   already flagged as the tree's highest-defect-density directory)
   should use this without question.** Not itself a tool fix — see
   `ansify`'s parser-gap findings entry under AMS1 above for why a
   real `parse_decl_block` rewrite was deliberately deferred instead of
   attempted.

Retired (do NOT re-run; listed so older findings entries below don't
mislead):

- Brace-glued-`{` grep (B1) — tool fixed and retrospectively
  recleared 2026-07-30; `ansify` now handles both the
  parameter-glue and body-glue shapes itself.
- `FinalizeObject()` empty-parens pre-grep (B2) — classpp fixed
  centrally 2026-07-30.

## Session structure going forward (amended 2026-07-30, wdc-approved)

Adopted after B3 closed Wave 2, to move pre-diagnosis legwork off the
top level now that the fallout taxonomy is mature (O1–B3) and the
2026-07-30 tool fixes are in:

- **Routine batches get a Gate 0 instead of full orchestrator
  pre-diagnosis.** The delegated session itself runs
  `ansify --dry-run --dir` on every directory in the batch plus the
  standing checklist above, classifies every DRIFT/skip finding
  against the documented taxonomy (citing the specific runbook/
  `porting-assessment.md` entry it matches), test-compiles `.eh`
  files under a temporary `-pe` flag where classification needs it,
  and STOPs at Gate 0 with the classification in its report. The
  orchestrator rules only on findings classified genuinely new — the
  Delegation ruling (rulings stay top-level) is unchanged; only the
  legwork moves. After the ruling(s), the session proceeds to the
  real run and the subtree gates as before.
- **Full orchestrator pre-diagnosis stays for the flagged-risky
  batches**: T1 (`atk/text`), I2 (`atk/figure` history), AMS1
  (`ams/libs/ms`), C1 (`contrib/zip/lib`).
- **The delegate drafts the runtime-check guidance.** Every batch
  REPORT ends with a "Suggested runtime checks for wdc" section —
  exact commands, per `rollout-procedure.md`'s Runtime check rules
  (`nm -g` against `runapp`/the relevant `.do` to find live
  consumers first; never launch GUI apps from the session; no saves
  against unversioned fixtures). The orchestrator vets that section
  rather than authoring it from scratch, then presents it to wdc
  before commit.

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
else: subtree-local only. **Wave/milestone checkpoints get a full
clean rebuild, not just tree-wide `dependInstall`** — see below.

**Wave-end gate status**: Waves 1 and 2 both closed without their
tree-wide checkpoint at the time (O4's report explicitly deferred it
as a top-level decision; B3 ran subtree-local only). Run retroactively
2026-07-30 during the amendment pass — result: clean except (a) the
two documented, pre-existing `contrib/zip/utility/ltapp.c` errors
(roadmap's known standing gate blocker, queued for Wave 7 C2's
`.ch`-typing treatment), and (b) a pre-existing `nns` link failure —
`libmail.a`'s `tlscon.o` (added 2026-07-17 for SMTP/IMAP) needs
OpenSSL, and `ams/msclients/nns`'s `ProgramTarget` lacked `${SSLLIB}`
while `cui`'s had it; first exposed here because this was the first
tree-wide relink of `nns` since the `dropoff`→`tlscon` dependency
appeared. Fixed same day (one-line Imakefile change mirroring `cui`),
`nns` relinks and installs clean. Not M3 fallout in either case — M3
changes are declaration-level only. This gate doubles as T1's
pre-flight baseline.

**Full clean rebuild now also required at every wave close, adopted
2026-08-01 (wdc), during Wave 6's close-out** — see
`rollout-procedure.md`'s new "Wave/milestone close-out gate" section
for the milestone-agnostic version (this convention is meant to
outlive M3). The tree-wide `dependInstall` gate described above is
**not** a substitute: it's incremental, and only proves the specific
directories `ansify`/`-pe` touched still build against what was
already sitting in `build/`. A `make Clean; make World` from the tree
root is a materially stronger check — it has never actually been run
at any prior wave boundary in M3 (Waves 1-5's tree-wide checks were
all `dependInstall`, including the retroactive Waves 1+2 run above),
so Wave 6 is the first wave to get one. Going forward: run it at every
wave close, and at M3's own completion (Wave 7's C2, which doubles as
the M3-to-M4 handoff gate) in particular — the bar for handing off to
M4 should be higher than a mid-wave checkpoint's.

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

**Exhaustive batch map, built 2026-07-25**: `m3-batches.md` — all 90
active directories with `.c` files, grouped into 24 batches across the
7 waves (plus the `atk/eq` pilot), sized by own-level `.c` file count
(a proxy — no real `ansify` census exists yet, see "First concrete
step" below). Built from the last full tree-wide gate log rather than
guessed, so a fresh session can start executing immediately instead of
re-deriving the directory tree.

## First concrete step — OVERTAKEN BY EVENTS (marked 2026-07-30)

Superseded in practice: the per-batch `ansify --dry-run` passes became
the census (O1–B3), and O4/B1/B2 proved the `-pe`/`.eh` mechanic live
at up to 81-class scale, fully serving the pilot's purpose. wdc
confirmed O4's judgment call 2026-07-30: the standalone `atk/eq`
pilot is retired and `atk/eq` folds into batch I2 as an ordinary
directory (see `m3-batches.md`). Original text kept below for the
record.

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

## Findings from real sessions

### O1 (`overhead/util/lib`, 2026-07-25) — first real (non-dry-run, non-pilot) M3 session

- **Directories with no `.ch` files skip steps 1-2 entirely.** Not every
  directory in the batch map is a class directory — `overhead/util/lib`
  (83 files, all file-local helpers) has zero `.ch` files, so there is
  no `-pe`/`CLASSFLAGS` line to add and no `.eh` to force-regenerate;
  the whole rollout point reduces to `ansify --dir` plus its compile-gate
  fallout. DRIFT is structurally impossible in such a directory (it only
  fires on `__` class methods). Check `.ch` presence
  (`find <dir> -maxdepth 1 -name '*.ch'`) before assuming the mechanics
  section's steps 1-2 apply.

- **`fix-missing-static-decl` (ansify's own pipeline step 1) is not
  idempotent against a hand-typed fix.** It recognizes an existing
  forward declaration as "already declared" only via a narrow
  empty-parens pattern (`NAME()`); a full ANSI prototype for the same
  name doesn't match, so it inserts a second, conflicting empty-parens
  stub even when a correct typed declaration is already present.
  Concretely: if a compile-gate failure is a promotion-narrowing
  conflict (`char`/`short`-by-value parameter, C89 default-argument-
  promotion mismatch — same species as the Pilot A `eq__WriteFILE`
  finding) against a declaration that `fix-missing-static-decl` itself
  inserted (a static helper forward-referenced with no pre-existing
  declaration of its own), the general "retype the declaration, re-run
  `ansify` on the file" guidance does NOT work — it reintroduces the
  same failure at a new duplicate-stub location. Working sequence
  instead: run `fix-missing-static-decl` once for real, hand-retype the
  one declaration it inserted, then run only ansify's helper-
  *conversion* step (not the top-level driver, which re-invokes
  `fix-missing-static-decl` and duplicates the stub) and verify with a
  direct `make <base>.o`. Found in `unscribe.c`'s `WriteFrag`; expect
  it to recur anywhere else in the tree with the same shape. Tool fix
  (recognize a typed prototype as satisfying "already declared") is
  real but not done — flagging here per the Delegation ruling (tool
  construction stays top-level), not fixed inline by the session that
  found it.

- **`ansify`'s per-file compile gate reaches files the real build
  doesn't build at all — a compile failure there is not automatically
  real fallout.** `NormalObjectRule()` provides a generic `.c.o:`
  pattern rule for every file in a directory regardless of whether it's
  in `$(OBJS)`, so `ansify --dir`'s `make base.o` gate will attempt
  (and can fail on) files an `#ifdef`-gated Imakefile macro excludes
  from the actual library/binary. O1 found 13 such files (the CMU
  Whitepages family, gated behind `WHITEPAGES_ENV`, which is not
  defined in this build — `WHITEPAGESFILES` is empty in the generated
  Makefile) plus one more excluded a different way (`verbose.c`,
  already commented out of `$(OBJS)` — old `varargs.h`-style variadic
  code that cannot be mechanically converted at all, ANSI or otherwise,
  without a rewrite). `ansify` correctly auto-reverted all 14; this is
  the tool working as designed, not a gap needing a fix.

  **Policy for such files, tree-wide, going forward:** leave them K&R,
  same as `ansify` already does automatically. Do not use `--no-compile`
  to force a conversion through — that discards the per-file compile
  gate, the exact safety property that distinguishes `ansify` from the
  June 2026 mass-conversion failure (§14), on files that structurally
  cannot be typechecked with the gating macro off (the Whitepages case:
  the type declarations themselves are `#ifdef`-gated, so nothing short
  of a real build with `WHITEPAGES_ENV` on could verify a conversion).
  If a gated subtree is ever activated for real, the right sequence is
  activation-then-ansify, not ansify-then-activation — now written up
  in full as its own standing playbook, `activating-new-subtree.md`
  (bring the macro on, fix real compile fallout using its LP64
  taxonomy, catch up to M2's declaration sweep, confirm it builds and
  runs, *then* run `ansify --dir` on it fresh as its own small
  M3-style rollout point) — the same order every other M3 batch
  directory already implicitly satisfies (all were live in the build
  before any M1/M2/M3 milestone touched them). No action item from
  this file for the current M3 pass; noted so a future batch with a
  similar gated-dead-file shape doesn't re-derive this from scratch.

### O2 (`overhead/image/jpeg` + `overhead/image/tiff`, 2026-07-25/26) — vendored third-party codec libraries

Full findings and file:line detail: `claude-history/m3-o2-imagecodecs-REPORT.md`.
Three new `ansify` bugs, all specific to how vendored 1990s portable-C
libraries write dual K&R/ANSI declarations — none seen in AUIS-native
code through O1:

1. **`(void)`-as-bare-parameter misparse — not fixed, self-healing.**
   `convert_file`'s helper matcher reads an already-ANSI zero-argument
   definition (`NAME (void)`) as K&R with one undeclared parameter
   literally named `void`, producing `NAME(int void)` — a syntax
   error. Always caught by the compile gate and auto-reverted (no
   corruption risk), but wastes triage time inflating both the
   dry-run census and the real-run failure count with false
   positives. Found in `overhead/image/jpeg` (13 files, all already
   fully ANSI, zero real changes needed — worked around with the
   tool's own `--no-helpers` flag, no tool edit). Left unfixed
   (2026-07-26, wdc's explicit call, resource-budget-driven): revisit
   whenever it next costs a session real triage time.
2. **`DECLARE<N>(name, type, arg, ...)` macro-invocation idiom
   misparse — not fixed, self-healing.** Vendored libtiff's own
   `USE_PROTOTYPES`-conditional prototype macro (`DECLARE2(f, t1, a1,
   t2, a2)` expands to `f(t1 a1, t2 a2)`) is misread by both
   `convert_file` (when every macro type-argument is a single bare
   word) and `fix-missing-static-decl` (whenever `static` precedes
   the invocation, regardless of argument types) as if the macro name
   itself (`DECLARE2`, `DECLARE3`, ...) were the one real function
   being defined, shared across every use of that arity in the file.
   Also caught by the compile gate in the common case (see #3 for the
   exception). Found in `overhead/image/tiff`, 8 files, ~30 instances
   — resolved by hand-converting each invocation to its literal
   expansion (the macro's own `USE_PROTOTYPES=1` output is ground
   truth). Left unfixed in the tool itself, same resource-budget call
   as #1.
3. **Compile gate silently skipped when only `fix-missing-static-decl`/
   `fix-static-methods` modify a file — FIXED 2026-07-26
   (`948a6569`).** The one finding in this batch that rises above
   "annoying but self-healing" to "can silently leave broken code on
   disk with a success-looking report": `process()` took an early
   return whenever `convert_file` found nothing to convert, without
   checking whether the earlier `run_fix_tools()` step had already
   changed the file on disk — so a file whose *only* needed change
   came from `fix-missing-static-decl` (e.g. `tif_dirwrite.c`, which
   already had its own correct, hand-written typed prototype block
   that `fix-missing-static-decl`'s empty-parens-only recognizer
   doesn't credit — the same underlying recognizer limitation as O1's
   `unscribe.c` finding, different trigger) got the duplicate-stub
   conflict inserted with **zero** compile verification and **zero**
   auto-revert. Confirmed via isolated repro (a `char`-parameter
   static helper with a pre-existing correct prototype): before the
   fix, `ansify` reported "0 compile failures" while leaving a
   conflicting-types duplicate declaration on disk; after, it
   correctly fails the gate and restores. Fix: `process()` now also
   gates whenever the file differs from its own backup post-
   `run_fix_tools()`, even if `convert_file` itself found nothing.
   Verified this doesn't change behavior for the ordinary cases
   (nothing changed anywhere → still no gate call; a harmless
   `fix-missing-static-decl` insertion with no corresponding
   `convert_file` work → now gated, passes, kept — a stricter check,
   not a new false-positive). This was a real gap in the safety
   property that distinguishes `ansify` from the June 2026
   mass-conversion failure (§14) — treat any `ansify` output from
   before this fix (i.e., before verifying against a tree checked out
   at or after `948a6569`) as needing a fresh compile-gate check on
   any file where `fix-missing-static-decl` reported a modification,
   even if `ansify`'s own report claimed success.

### O3 (`overhead/mail/lib`+`cmd`+`testing`, `overhead/eli/lib`+`bglisp`, `overhead/bison`, 2026-07-26)

Full findings: `claude-history/m3-o3-mail-eli-bison-REPORT.md`. Six
unrelated directories, one session (M2 rollout-point-2 style
batching). Five new patterns, all mechanical/self-healing except the
last:

1. **`fix-missing-static-decl` non-idempotency, variadic sub-case.**
   O1's `unscribe.c` finding already showed the tool's "already
   declared" recognizer only matches literal empty parens, so it can
   insert a conflicting stub even over a correct existing declaration.
   When the real definition is **variadic**, the inserted empty-parens
   stub isn't just narrow-type-incompatible (fixable by retyping) — no
   empty-parens declaration can ever be compatible with a `...` tail,
   so the only fix is a full variadic prototype. Same repair sequence
   as the non-variadic case, worth recognizing on sight.
2. **Same non-idempotency bug, pre-existing-declaration trigger.** O1
   and O2 both saw this misfire against a declaration the tool itself
   had inserted on a prior pass. O3 found the identical failure
   triggered by a **pre-existing, hand-written** empty-parens
   declaration already in the pristine K&R source: retyping it to fix
   an unrelated narrow-type-promotion conflict makes a second
   `fix-missing-static-decl` pass fail to recognize the fix and insert
   a fresh duplicate anyway. Confirms the bug isn't limited to
   tool-inserted declarations — any full-prototype forward declaration
   without a matching empty-parens block trips it, regardless of
   origin.
3. **Ordinary public C headers are not `.eh`-locality-guaranteed.**
   This runbook's "Gate scope" section argues M3's blast radius stays
   directory-local via the `.eh` mechanism specifically. That argument
   does not extend to hand-maintained public headers: a non-static,
   narrow-typed helper's ANSI conversion can surface a stale
   empty-parens declaration in a **different, unbatched directory's**
   header (`overhead/mail/lib/scan822.c`'s `IsOK822Atom` against
   `overhead/mail/hdrs/mail.h`, safe and narrow here — one real
   consumer, confirmed dead-code everywhere else — but a real gap in
   the gate-scope argument). **New rule, effective now**: before
   considering a directory's Import-side blast radius accounted for,
   grep the installed header tree for a matching empty-parens
   declaration of any non-static helper being converted, the same
   spirit as M1's Import-fallout caution, now shown to apply to M3 too
   via a different mechanism. Any such cross-directory header touch
   should be flagged prominently in the session's report (as O3 did),
   even when the fix is small and low-risk — treat it as worth a
   second look, not routine, until this rule has more data points.
4. **Poor-man's-varargs (`/*VARARGS1*/`) fixed-arity K&R functions.** A
   K&R function declared with N fixed parameters but conventionally
   *called* with fewer, relying on old-style calling convention to
   silently ignore unsupplied trailing arguments. Once ANSI-typed, every
   call site needs the full arity — `error: too few arguments...
   expected N, have M`. Not a tool bug, not DRIFT, just the expected
   consequence of arity-checking a function that was never really
   variadic. Fix: pad every call site to full arity with a zero/NULL
   sentinel already in scope (`overhead/mail/lib/stats.c`'s `warning`,
   9 call sites). Likely to recur — this codebase has other
   `/*VARARGS1*/`-annotated functions.
5. **A real, non-`.ch` interface bug surfaced by ANSI arity-checking —
   held for a ruling, now resolved.** `overhead/eli/lib/prmtives.c`
   called `regcomp`/`regexec`, functions that don't exist anywhere in
   this codebase — a ~35-year-old misspelling of the codebase's own
   `reg_comp`/`reg_exec` (`overhead/rxp/regexp.h`), silently linked for
   decades against macOS's own incompatible POSIX `regcomp`/`regexec`
   instead, via K&R's implicit-declaration/linker-resolves-anything
   behavior. This is the DRIFT/hard-stop taxonomy's "real bug, needs a
   human ruling" class, but with **no `.ch` involved at all** — worth
   remembering that this class isn't limited to class-method interface
   disagreements; a plain call-site name typo against a
   similarly-named sibling function is the same species. **Ruling
   2026-07-26 (wdc): renamed to `reg_comp`/`reg_exec`.** Fixed and
   verified (directory gated clean twice, deterministic); written up in
   `revival.md`'s "Old bugs never found till now" per wdc's request.
   Expect more instances as later waves reach older, less-visited
   corners of the tree — this is exactly the kind of finding M3's own
   mechanism (turning implicit declarations into real ones) is
   positioned to surface.

### O4 (overhead grab-bag, 19 directories, 2026-07-26) — completes Wave 1

Full findings: `claude-history/m3-o4-overhead-grabbag-REPORT.md`.
Cleanest dry-run baseline of any Wave 1 batch (0 DRIFT, 0 skipped
tree-wide) but the batch that finally exercised the `-pe`/`.eh`
mechanic for real, plus a standing action item for every batch after
this one:

1. **First real `-pe`/`.eh` rollout, and a new fallout shape it
   surfaced.** `overhead/class/testing` was this project's first
   directory to actually flip `CLASSFLAGS += -pe` and force a real
   (non-dry-run) `.eh` regen — a judgment call to run this ahead of the
   `atk/eq` Wave-4 "pilot," on the reasoning that Wave 1 legitimately
   precedes Wave 4 in dependency order and the directory's blast radius
   is a 3-file test/demo fixture. **Confirmed by wdc 2026-07-30** —
   `m3-batches.md`'s Wave 4 "eq pilot" framing is retired (`atk/eq`
   folded into I2 as an ordinary directory; see "First concrete step"
   above). The eq-shape narrow-type-promotion failure
   (`porting-assessment.md`'s 2026-07-08 validation) did **not**
   recur; instead: a class lifecycle method (`InitializeClass`/
   `InitializeObject`) written with a **truly empty parameter list** —
   no implicit `classID`/`self` placeholder at all — is invisible to
   `ansify`'s own candidate detector (its parser requires at least one
   bare identifier in the parens to treat a definition as K&R), so it
   silently leaves such definitions alone with no DRIFT/skip report —
   but the definition still conflicts with a `-pe`-regenerated `.eh`,
   which emits a typed prototype for these names regardless (1 param
   for ordinary classprocs, classpp's own hardcoded 2-param convention
   for `InitializeObject`/`FinalizeObject` specifically — see
   `porting-assessment.md` §17). This is `-pe`/`.eh`-regen-side fallout,
   invisible to `ansify --dir`'s own report — before trusting a clean
   `ansify --dir` result on a freshly-`-pe`-flagged directory, grep its
   `.c` files for `Name__Method()` with nothing between the parens.
2. **A misspelled preprocessor guard (`_STDC_` for `__STDC__`) had left
   a header's typed declaration branch permanently dead for the
   directory's entire history** (`overhead/cmenu/cmenu.h`) — written up
   in full in `revival.md`'s "Old bugs never found till now." Two
   things worth generalizing from this one instance: (a) it was found
   by a session doing ordinary compile-fallout triage, not by any
   automated check, the same way O3's `regcomp`/`regexec` finding was;
   (b) **fixing a dead branch like this can surface a second,
   independent bug inside that same branch**, because a branch that
   has never once compiled has never had any of its own declarations
   checked against anything — here, a lifecycle-adjacent function's
   return type had been silently defaulting to `int` against a real
   `void` definition, invisible until the branch went live. Treat any
   `#ifdef`-gated-off branch you flip live as needing the same fallout
   triage as a freshly-converted file, not a free pass because "it's
   just a macro rename."
3. **New standing task, effective now: grep every directory before/
   during its M3 batch for single-underscore misspellings of standard
   predefined macros** (`_STDC_` for `__STDC__` being the concrete
   instance found; also worth checking for `_cplusplus`/`_FILE_`/other
   common one-underscore-short typos of compiler-predefined macros)
   **and fix them on sight, the same as any other ordinary compile-gate
   fallout** — don't defer to a separate pass. This is now a permanent
   line item in every M3 batch's task list going forward (Waves 2-7),
   not a one-off. Rationale: unlike most K&R→ANSI fallout, a dead
   `#ifdef` branch costs nothing to check (a grep, not a conversion)
   but can hide an arbitrarily old, arbitrarily deep second bug the way
   this one did — worth the small fixed cost on every remaining batch
   rather than hoping the next one surfaces on its own.
4. Two directories the dry-run's "0 helpers, already ANSI" framing made
   look like ordinary clean conversions turned out to be **entirely
   inert in this build** instead (`overhead/malloc`, `inst`) — gated
   off tree-wide by their whole Imakefile body, not just individual
   `#ifdef`s within it, a stronger variant of O1's WHITEPAGES pattern.
   `overhead/malloc` in particular reconfirms `rollout-procedure.md`'s
   Liveness-census caution about `site.h` overriding `allsys.h`
   (`ANDREW_MALLOC_ENV` defined in one, `#undef`'d in the other) — a
   second real occurrence of the exact mechanism M1 batch 7 first found.
5. Four ordinary compile-gate fallout fixes (`overhead/cmenu/shadows.c`
   +`shadows.h`, `overhead/mail/metamail/metamail/mailto.c`,
   `overhead/index/index.c`+`index.h`, `overhead/rxp/regexp.c`) — all
   recurrences of the already-documented non-idempotency/narrow-type/
   same-directory-header shapes from O1-O3, just new files. One new,
   self-healing parser-bailout shape (`overhead/class/lib`: a function-
   pointer parameter whose return type is itself a pointer, e.g.
   `struct classinfo *(*proc)()`, not recognized by the helper-
   declaration regex — left K&R correctly, no action needed).

### B1 (`atk/basics/common`, 41 classes, 2026-07-26) — opens Wave 2, first large-scale `-pe`/`.eh` rollout

Full findings: `claude-history/m3-b1-basics-common-REPORT.md`. M1's
own former largest-blast-radius directory; the first M3 batch where
`-pe` converted a real, sizeable class population (41, vs. O4's
2-class trial) rather than a handful. All 6 pre-diagnosed dry-run
findings resolved exactly as pre-computed (see the batch prompt,
`m3-b1-basics-common-prompt.md`, for the diagnosis detail — worth
reading alongside this entry since it shows the pre-flight-triage
approach at real scale). Three genuinely new patterns:

1. **A silent, unreported `ansify` helper-parser gap: K&R declaration
   blocks with the opening brace glued to the last parameter's `;`.**
   `parse_decl_block`'s line-by-line scanner only recognizes a bare `{`
   line as the end of a K&R declaration block; a very common
   1988-era style in this directory's oldest files puts the brace on
   the *same* physical line as the last parameter (`long width;{` or
   `struct point * LogicalPoint; {`). Neither the bare-`{` check nor the
   `DECL_LINE` end-of-string check matches, so the candidate is silently
   dropped — **no skip message, no DRIFT report, nothing at all**,
   different from every previously-documented parser gap (O2's `(void)`
   and `DECLARE<N>` misparses, O4's function-pointer-returning-pointer
   gap, this same batch's own `gifin_load_cmap` multi-dim-array gap),
   all of which at least produce a `skipped: ...` line. Confirmed in
   the tool source (`parse_decl_block`, ~line 204). Found via two files
   converting to literally nothing (`physical.c`, `point.c` — every
   real function in each uses this brace style) plus partial conversion
   in 8 others (`cursor.c`, `describe.c`, `im.c`, `observe.c`,
   `owatch.c`, `rect.c`, `region.c`, `view.c`).
   **Why it's bounded, not silently dangerous**: the same early-stage
   gate runs before `convert_file` distinguishes a class method from a
   file-local helper, so a `__`-named method written in this brace
   style would be equally invisible — but that only means it stays K&R
   through the `ansify --dir` pass; the *next* step (`-pe`'s `.eh`
   regen) still emits a typed prototype for it regardless, so a real
   instance surfaces as an ordinary compile failure at the subtree-local
   gate, not a silent runtime bug.

   At the time, this was treated as a **new standing task**: before a
   directory's `ansify --dir` run, grep it for `grep -lE ';[ \t]*\{[
   \t]*$'` and treat any match as worth a manual look, the same spirit
   as the O4 macro-typo check.

   **Correction (2026-07-30, orchestrator)**: the claim that "B1's own
   directory has none — the double-clean gate proves it" was **wrong**.
   Verified empirically that a K&R definition already matching its typed
   prototype compiles clean under this project's flags with only a
   `-Wdeprecated-non-prototype` *warning*, not an error — so "0 errors"
   never actually proved nothing was silently left unconverted. B1's
   own `atk/basics/common` in fact had ~20+ real class methods sitting
   unconverted this way the whole time. Fixed in the tool itself and
   retroactively cleared from all 7 already-`-pe`'d directories,
   including this one — full detail in the new "ansify brace-glued
   parser fix + retrospective recheck" entry below. **The standing grep
   task above is now retired** for any directory processed after
   2026-07-30 (the tool catches this itself); it remains useful
   historical record for why B1/B2 needed it by hand.
2. **Cross-`.ch` "rock" (`void *`/`long` opaque-data) type
   disagreements** — a new sub-pattern of the already-documented rock
   idiom (`porting-assessment.md` Pilot B/point-9): not a class's own
   `.ch` vs. its `.c` (ordinary DRIFT), and not a parent/child override
   disagreement (`point-10-batch-3`'s documented shape), but **two
   unrelated classes'** `.ch` files disagreeing about the type of a
   value passed between them at a call site (`im.c`'s `PostResource`
   forwarding to `rm_PostResource`: `view.ch` says `void *`, `rm.ch`
   says `long`, for what is semantically the same value; likewise
   `message.c`'s `AskForStringCompleted` forwarding to
   `msghandler_AskForStringCompleted`, `message.ch` vs. `msghndlr.ch`).
   Both resolved with an ordinary `(long)` cast at the forwarding call
   site, no `.ch` edit — but worth watching for at scale, since neither
   shows up as DRIFT (each class's own `.ch`/`.c` agree internally) or
   as a skip; it only surfaces as an ordinary compile failure once both
   sides go typed.
3. **A real ~35-year-old caller bug, found by the same mechanism O1
   point-9 already established**: `im.c`'s `im__WantColormap` called
   `im_InstallColormap(self, *cmap)`/`view_ReceiveColormap(requestor,
   *cmap)` where `cmap` is already `struct colormap *cmap` — an extra,
   incorrect dereference passing a struct by value where both macros
   (typed since M1's tree-wide `-pi` rollout, 2026-07-10) expect a
   pointer. Invisible the whole time because nothing had forced a clean
   rebuild of `im.o` since M1 went live. Fixed (dropped the stray `*`
   at both call sites); the function's third, structurally different
   call (`im_InstallColormap(self, *inherited)`, where `inherited` is
   `struct colormap **`) was correctly left alone.

Also confirmed (2026-07-26, independently, by the orchestrator):
re-ran the subtree-local gate twice more, spot-checked the colormap
fix's correctness against a sibling call in the same function
(`view_SetColormap(requestor, cmap)`, already using the un-dereferenced
form — confirms the fix direction), and confirmed the new parser-gap
claim directly (`grep -nE ';[ \t]*\{[ \t]*$' point.c` reproduces
exactly as described).

**Mid-batch complication, handled**: a concurrent, unrelated session
landed 11 commits (an LP64 `%d`→`%ld` datastream-write fix, "widespread"
per wdc) to the shared fossil repository while B1's review was in
progress, including two commits touching `atk/basics/common/image.c`/
`image.ch` — a file this batch also converted. `fossil`'s auto-sync
fast-forwarded the local checkout and 3-way-merged the incoming commits
against B1's uncommitted local edits automatically. Verified this
merge was correct before proceeding to commit: confirmed both the
concurrent session's `%ld` fix and B1's K&R→ANSI signature conversion
are present together in the merged `image.c` (`image__SendEndData`'s
parameter went from K&R `int id` all the way to ANSI `long id` — both
changes, correctly composed), and re-ran the subtree-local gate twice
more post-merge (clean both times). **Standing note for future
batches**: if a long-running batch's review coincides with concurrent
commits elsewhere in the tree, don't assume a clean `fossil status`
(EDITED, not CONFLICT) is sufficient proof of a correct merge on its
own — check whether any concurrently-landed commit touched a file this
batch also touched, and if so, spot-check that file's current content
directly before trusting the merge and committing.

wdc separately re-tested the `image` inset bug (`roadmap.md`) after B1
and found GIF import's failure symptom had changed (black → white,
with a different test file); investigated and **not attributed to
B1** — `gif.c`'s B1 diff is pure syntax with zero semantic change, and
the concurrent session's `image.c` fix touches only the datastream
*write* path, not `gif__Load`'s *import* path. Written up in
`roadmap.md`'s `image` entry as a new data point, still unconfirmed as
to root cause.

### B2 (`atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`, 81 classes, 2026-07-30)

Full findings: `claude-history/m3-b2-value-support-REPORT.md`. The
widest single M3 batch by class count so far (81 vs. B1's 41) and the
**first batch to actually edit `.ch` files** — every prior batch's
fixes stayed inside `.c`. Orchestrator pre-diagnosis resolved 37
DRIFT/skip findings across all 5 directories before delegating (all
confirmed exactly matching on the real run); the session itself found
6 more genuinely new patterns:

1. **A real classpp codegen bug, distinct from the DRIFT false-positive
   already documented**: `FinalizeObject`'s exported `-pe` prototype is
   built from whatever the `.ch` declares (the ordinary classproc
   path), but the internal generated `__Finalize` call site
   unconditionally passes 2 hardcoded args — self-inconsistent the
   moment a class uses the ordinary empty-parens `FinalizeObject()`
   convention. Confirmed directly in `overhead/class/pp/class.c`.
   Full write-up and the open "fix classpp centrally vs. keep
   patching `.ch` per-directory" question: `porting-assessment.md`,
   new subsection after §17's Tool-fix note.
2. **6 real ~35-year-old `.ch` copy/paste typos**, found via a
   systematic audit triggered by investigating (1) — wrong class name
   substituted in a restated `InitializeObject`/`FinalizeObject`/
   ordinary-method parameter type, invisible until `-pe` actually
   type-checked them. One (`celv.ch`'s `PromptForInfo`) was a live bug
   (an ordinary method, not lifecycle-special-cased); the other five
   were technically dead documentation for their `InitializeObject`
   half (classpp ignores it) but live for `FinalizeObject`. Full table
   in the B2 report §6; written up in `revival.md`'s "Old bugs never
   found till now."
3. **A third classpp bug**: an unnamed classproc parameter
   (`xfontd.ch`'s `Deallocate(struct xfontdesc *)`, no parameter name)
   causes classpp to drop the type name from the emitted prototype
   entirely (`struct  *`, uncompilable). Workaround: name the
   parameter. `porting-assessment.md`, same new subsection as (1).
4. **B1's predicted brace-glued parser gap, confirmed live for the
   first time**: `xgraphic.c` had 32 real class methods silently
   invisible to `ansify` (B1 found the pattern; B2 hit a real,
   `-pe`-blocking instance — B1's own directory turned out to have the
   same gap too, just not yet blocking anything, see the correction
   above and the retrospective-fix entry below). Fixed locally here
   by mechanically splitting the glued brace onto its own line, then
   letting `ansify` reconvert normally — confirms this is a viable
   stopgap for any future directory hitting the same gap without
   needing the tool itself fixed first.
5. **A new, named recurring shape: "override typed to the general base
   interface, but the body needs the concrete subclass"** — 8 instances
   across 4 directories (`buffer.c`, `sbuttonv.c`, `celv.c`,
   `arbiterv.c`, `lsetv.c`, `xcmap.c`, `xim.c`). Distinct from both the
   rock idiom (`void*`/`long`) and B1's cross-`.ch` disagreement
   pattern: here the `.ch` is *correct* (the override's parameter really
   is the general interface type, e.g. `struct color *`), but the
   implementation always needs the concrete subtype for its own fields.
   Resolved uniformly: rename the parameter to `..._generic`, add a
   local cast to the concrete type for body use, keep `super_*`
   forwarding calls on the `_generic` (correctly-typed) name. Zero
   behavior change, same as every other type-safety-only fix in this
   taxonomy.
6. One real caller bug found alongside the above (`sbuttonv.c`'s
   `sbuttonv__ObservedChanged` called `sbutton_GetTrigger(b, ...)` using
   the method's own `b` parameter instead of the already-verified-equal
   local `b2` four lines below where the two were confirmed identical)
   — same species as B1's `im.c` colormap dereference: invisible under
   K&R, surfaced the moment the call got typed for real.

Also confirmed (2026-07-30, independently, by the orchestrator): full
gate re-run (all 5 directories, twice each) came back clean and
deterministic; verified the classpp `FinalizeObject` mechanism directly
against `class.c`'s source (both the hardcoded call site and the
non-hardcoded prototype loop); spot-checked 4 of the 6 `.ch` typo
diffs directly; confirmed **zero cross-directory consumers** of any of
the 6 corrected classes' `InitializeObject`/`FinalizeObject`/
`PromptForInfo` (so the `.ch`/`.ih` fixes, despite being the first
`.ch`-level edits in M3, carry no tree-wide blast radius in practice);
confirmed `xgraphic.c` now has zero remaining brace-glued lines.

### classpp `FinalizeObject` fix (tool fix, not a batch, 2026-07-30)

B2's classpp `FinalizeObject` bug (above) is fixed centrally, not
per-`.ch` — full detail in `porting-assessment.md`'s classpp-bugs
subsection. **Standing task removed**: the "grep `FinalizeObject()`
before each future `-pe` rollout" caution from B2's findings is no
longer needed — any directory's `-pe` rollout from here on
automatically gets a self-consistent `.eh` for the ordinary
empty-parens `FinalizeObject()` convention, the same way
`InitializeObject` always has. `buttonv.ch`/`sliderv.ch`'s existing
explicit restatements (B2's workaround) don't need reverting — they're
harmless, redundant documentation now, same status as
`InitializeObject`'s own restated types per §17. One directory not yet
`-pe`'d already known to have the empty-parens shape live:
`atk/org/orga.ch:72` — no action needed now, will just work once that
directory's wave arrives.

### ansify brace-glued parser fix + retrospective recheck (tool fix, not a batch, 2026-07-30)

Fixed the parser gap named in B1's finding (1) and hit live in B2's
finding (4) above, then re-ran `ansify --dir` across all 7 already-`-pe`'d
directories to clear out whatever it had been silently leaving K&R the
whole time. Full detail: `claude-history/m3-ansify-brace-glued-fix-REPORT.md`.

- **The fix**: `parse_decl_block` now *recognizes* a brace-glued K&R
  line (last parameter declaration and the opening `{` on one physical
  line) without mutating anything; `convert_file` only rewrites that
  line to a bare `{` once it has actually committed to using the
  candidate. **This two-step design is not what was originally
  specified** — the first version (specified and unit-tested by the
  orchestrator before delegating) mutated the line the moment the shape
  was recognized, unconditionally. The delegate found this had a real
  defect the specified unit tests didn't cover: if a brace-glued
  candidate was later *rejected* (no DB signature — which is exactly
  what happens to every `InitializeObject`, the single most common
  brace-glued shape in this codebase) in a file where something else
  legitimately converted, the rejected candidate's parameter
  declaration was silently deleted from the output, corrupting the
  file. Caught immediately by `ansify`'s own per-file compile-gate-
  and-restore (`atk/support/mark.c`, first real instance) — no
  corrupted content ever reached disk — but the tool would have been
  unable to convert *any* file hitting this combination without the
  correction. Confirmed neither already-completed directory
  (`atk/basics/common`, `atk/value`) was exposed to it: both directories'
  only skips were either not brace-glued at all, or in files where
  nothing else changed (so the corrupted-but-unused state was
  discarded before ever reaching disk either way).
- **Retrospective recheck results**: `overhead/class/testing` and
  `atk/adew` confirmed genuine no-ops (0 conversions, matching what
  each directory's original batch already implied). The other 5
  directories had real, previously-invisible K&R left over: 10 files
  in `atk/basics/common` (~20+ methods, e.g. `cursor__ChangeShape`,
  several `im__*`/`observable__*`/`region__*`/`view__*`), 5 in
  `atk/support` (incl. 3 more `InitializeObject` hand-folds, same
  precedent as B1's `event.c`/`keystate.c`/`init.c`), 3 in `atk/value`,
  1 each in `atk/supportviews` (`oscroll.c`) and `atk/basics/x`
  (`xim.c` — confirmed `xgraphic.c` itself has zero remaining
  brace-glued lines, B2's local hand-fix holds up). Zero DRIFT found
  anywhere — every newly-surfaced candidate either converted cleanly
  or was an already-understood `InitializeObject`/override-of-base-
  classproc DB-miss skip.
- **A genuine ~35-year-old bug found, but inert**: `atk/adew/cel.c`
  has a second, misspelled, empty-bodied `cel__FinializeObject`
  ("Finialize" for "Finalize") alongside the real, working
  `cel__FinalizeObject` — never wired, never called, doesn't affect
  behavior. A second instance of the same misspelling exists in
  `atk/textaux/contentv.c:79` (`contentv__FinializeObject`) — outside
  this task's 7 directories, **not yet fixed, not going to be fixed by
  `ansify` either** (the misspelling means it will always fail the DB
  lookup and get silently skipped, same as `cel.c`'s copy) — worth
  knowing when B3 (which includes `atk/textaux`) gets pre-diagnosed, so
  it isn't mistaken for something new. Both written up in `revival.md`'s
  "Old bugs never found till now."
- Independently re-verified by the orchestrator (2026-07-30): traced
  the corrected `parse_decl_block`/`convert_file` interaction directly
  in the tool source to confirm no other code path reads `lines[brace_idx]`
  before the accept decision; spot-checked the `mark.c` and `point.c`
  diffs directly; ran fresh double-clean gates on `atk/support` and
  `atk/basics/common` (the two most consequential) and single clean
  gates on the remaining 5; confirmed `fossil status`/`fossil extras`
  match the report exactly, no stray files.

### B3 (13 leaf directories, 61 files, closes Wave 2, 2026-07-30)

Full findings: `claude-history/m3-b3-leaf-dirs-REPORT.md`. The heaviest
pre-diagnosis of any M3 batch so far: rather than relying on
`ansify --dry-run --dir`'s DRIFT/skip labels alone, the orchestrator
test-compiled several `.eh` files under a temporary `-pe` flag (added to
a directory's Imakefile, reverted after) to empirically confirm which
DRIFT findings were real compile-blocking bugs versus already-documented
tool false positives, before writing the batch prompt.

1. **A new classpp-interaction bug class, sibling to the `FinalizeObject`
   bug fixed earlier the same day**: `InitializeClass`'s real convention
   is 1 implicit parameter (`classID` only, untyped dispatch) — but
   unlike `InitializeObject`, classpp does not hardcode this; it runs
   `InitializeClass` through the *ordinary* classproc-emission loop,
   which always prefixes `struct classheader *` and appends whatever
   the `.ch` declares verbatim. A `.ch` that "helpfully" restates the
   implicit param by name (`InitializeClass(struct foo *self)`) gets
   counted as an *extra* argument on top of the automatic prefix,
   producing a 2-param exported prototype against a real 1-param `.c`
   definition — confirmed by an actual failed test-compile of
   `atk/textobjects/unknownv.eh`. Same mechanism hits `FinalizeObject`
   when a `.ch` restates *both* implicit params (`atk/apt/suite/suiteev.ch`
   restated `ClassID` *and* `self`, producing a 3-param prototype against
   a real 2-param definition). **Not universal** — 3 pre-existing
   instances in `atk/value` (`metextv.ch`/`eintv.ch`/`etextv.ch`) restate
   `InitializeClass` the same way but are *not* bugs, because their `.c`
   definitions already, correctly, take the full 2 real params (already
   `-pe`'d in B2, confirmed safe by inspection — the unused 2nd param
   just reads garbage, same harmless shape as `dialog__InitializeClass`).
   Always check the real `.c` param count before assuming either shape is
   safe. Fixed by simplifying the 3 broken `.ch` declarations
   (`unknownv.ch`, `suiteev.ch` ×2) back to the true convention — no
   tool-level fix, the instance count was small and bounded (tree-wide
   grep confirmed exactly 8 total restated-`InitializeClass` instances
   and exactly 1 double-restated-`FinalizeObject` instance exist anywhere
   in the source tree; 3 more live outside B3's scope — `fldtreev.ch`
   (Wave 6), `schedv.ch`/`ltv.ch` (Wave 7) — not yet checked for which
   direction they resolve, worth a 30-second look when those waves
   arrive).
2. **A real ~35-year-old missing parameter**: `atk/apt/tree/tree.ch`'s
   `TreeWidth`/`TreeHeight` declared zero explicit args; the real
   implementation always took `(self, node)`. Zero callers anywhere in
   the tree, so purely latent — fixed by adding the missing
   `tree_type_node node` parameter to the `.ch`, matching the
   always-correct implementation.
3. **Two more empty-parens-lifecycle-method instances**
   (`atk/extensions/gsearch.c`/`isearch.c`'s `InitializeClass()`,
   literally zero named parameters) — same standing-task category as
   B2's `runadewa.c`, invisible to `ansify`'s own HDR regex. Confirmed
   for the first time (this batch) that this shape is a **real,
   deterministic compile failure** under `-pe`, not merely a documented
   caution — test-compiled directly, got `conflicting types`. (A K&R
   definition with *named* parameters is compatible with nothing but an
   exact prototype match; a K&R definition with *literally no* names is
   not the universally-lenient case one might assume either — verified,
   don't assume otherwise.) Hand-folded both to ANSI form with a named,
   unused `classID` param.
4. **Two genuine new `ansify` tool bugs**, unrelated to any previously
   documented limitation: a double-pointer K&R parameter (`void
   **pyylval`) silently lost one `*` during conversion
   (`atk/syntax/tlex/tlex.c`, 1 instance, only occurrence tree-wide);
   an array-parameter's brackets landed *before* the parameter name
   instead of after (`unsigned char [ ] rgb_vect` instead of `unsigned
   char rgb_vect[]`, a hard parse error — `atk/apt/suite/suite.c`, 7
   instances, one file). Both hand-fixed per-instance; neither given a
   tool-level fix (small, bounded instance counts) — flagged for
   whoever next touches `ansify`'s parameter parser.
5. **A more dangerous variant of the same-day brace-glued gap — traced
   back to a regression in that morning's own fix.** Distinct from the
   signature-glue shape (safe no-op skip): here a K&R function body
   opens with its *first local variable declaration* on the same
   physical line as the brace (`{ register struct suite *self = NULL;`).
   `ansify`'s existing bare-brace check (`s.startswith('{')`, present
   since before any of this day's fixes) correctly identifies the brace
   line, but this morning's own two-step mutation fix — added to solve
   the signature-glue case — cannot tell "a brace-glued *parameter*
   line, safe to squash" apart from "a bare-brace-check hit with
   unrelated trailing body content" and squashes both, **silently
   deleting the local variable declaration**. Found in
   `atk/apt/suite/suite.c` (30 instances) and `suiteev.c` (2) — by far
   the largest-volume single finding of the day. Confirmed this can
   never silently reach committed code: the resulting undeclared-
   identifier reference is a hard, unsuppressible compile error under
   every flag this project uses, always caught by `ansify`'s own
   per-file compile-gate-and-restore. So B1/B2 (committed before this
   morning's fix existed) were never exposed, and this morning's own
   7-directory retrospective recheck (which *did* run with the
   vulnerable code) is confirmed safe by its own clean, unreverted gate
   results — **no retrospective recheck needed**. B3 itself worked
   around it with the same stopgap mechanism established for the
   signature-glue variant (split the glued line in the original K&R
   source, let `ansify` reconvert). **Fixed and committed same day** —
   see the "`ansify` brace-body corruption fix" entry below for the
   completed tool fix.
6. **A real ~35-year-old `.ch`-vs-`.c` type mismatch, exposed only once
   `-pe` went live**: `atk/apt/tree/treev.ch`'s `SetHitHandler` declared
   its callback as `(long *handler)()`/`char *anchor`; the real `.c`
   implementation takes `struct view *(*handler)()`/`struct view
   *anchor`. Originally pre-diagnosed as "leave K&R, don't fix" (a safe
   `ansify` skip, no confirmed live callers found) — but once `-pe` was
   live for the directory, classpp itself emitted syntactically invalid
   C from the malformed `.ch` type directly into the `.eh`
   (`( long *  ) ( )` is not valid C), failing the whole file's compile
   — no longer a containable skip once the batch's own gate requires
   `-pe` live. Fixed using a sibling class's already-established
   convention for exactly this ambiguity (`atk/org/orgv.ch`'s
   `SetHitHandler(procedure handler, struct view *anchor)`, the
   codebase's generic function-pointer placeholder type) rather than
   asserting an unverified concrete signature.
7. Several untyped `.ch` parameters (bare identifiers, no type at all —
   predates this work, K&R never checked it) defaulted by `ansify` to
   `void *`, wrong when the real type is a function pointer — fixed
   per-instance in `atk/apt/apt/apt.ch`/`aptv.ch`/`atk/apt/tree/treev.ch`
   (one of which, `treev.ch`'s `Create`, was missing not just a type but
   the parameter name entirely) using the same zero-blast-radius
   methodology as every other `.ch` fix this project has made
   (real `.c` implementation type + all real callers checked first).
8. A function-prototype-scope struct-tag trap (`atk/textaux/compchar.c`)
   — a genuine, subtle C-semantics gotcha (not an `ansify` bug): a
   struct tag's first-ever appearance inside a bare declaration's
   parameter list gets only function-prototype scope (C11 6.2.1p2) and
   doesn't merge with the same tag defined later at file scope, even
   though nothing looks different about the surface syntax. Fixed by
   adding plain file-scope forward declarations before the affected
   hand-fixed prototypes. Worth remembering for any future "add a
   missing prototype by hand" repair.
9. 4 more rock-idiom (`void*`/`long`) instances, all resolved per the
   established B1/B2 precedent — one in the *opposite* direction from
   usual (`atk/frame/frame.c`'s `FindFrameForBuffer` had a stray,
   incorrect `(long)` cast laundering a good pointer down to `long`
   before an unrelated `void *` parameter; removed the cast rather than
   adding one).

Independently re-verified by the orchestrator (2026-07-30): all 5
pre-diagnosed fixes confirmed applied exactly as specified; both new
`ansify` tool-bug fixes spot-checked directly in the diff; the 32-instance
brace-glued-local-declaration fix spot-checked directly (confirmed the
local variable and its rock-idiom cast both survived correctly); the
`treev.eh` `SetHitHandler` fix confirmed to now emit valid C; the
`compchar.c` struct-tag fix confirmed present; full gate re-run — double
clean cycles on `atk/apt/suite` and `atk/apt/tree` (highest risk), single
clean cycles on the remaining 11 (one apparent `error:` hit in
`atk/syntax/tlex` traced to a string literal inside a benign warning, not
a real error); `fossil status` matched exactly (78 edited files); `fossil
extras` showed only ordinary build byproducts plus two pre-existing,
unrelated untracked files predating this session by weeks.

### `ansify` brace-body corruption fix (tool fix, not a batch, 2026-07-30)

B3's finding 5 above (the brace-glued-to-local-variable-declaration
corruption) is fixed. Root cause was a narrow regression in that
morning's own brace-glued-parameter fix: `parse_decl_block` can return
`brace_idx` via two different code paths — the new brace-glued-parameter
branch (safe to squash to a bare `{`, since the declaration text was
already captured) or the original, pre-existing bare-brace check (which
may have arbitrary trailing body content, like a local variable
declaration, that was never captured and must not be touched) — and
`convert_file`'s mutation-on-accept step couldn't tell them apart, so it
squashed both. Fixed by having `parse_decl_block` return an explicit
`needs_brace_split` boolean (`True` only for the brace-glued-parameter
path) instead of inferring intent from the line's content after the
fact. Verified end-to-end by the orchestrator *before* writing the task
prompt (three scratch cases against a patched copy of the tool: the
corruption case now preserves the local declaration, the plain-bare-brace
case is unaffected, the original signature-glued case still splits
correctly) and independently re-verified after the delegate's own
application: diff matches exactly what was specified; the same three
cases re-run directly against the real, on-disk fixed tool; `atk/apt/suite`
and `atk/apt/tree` (the two directories that hit this corruption live in
B3, already committed with B3's manual stopgap in place) both gated clean,
twice each, both when the delegate ran it and independently by the
orchestrator afterward. `fossil status` showed only `revival/tools/ansify`
edited. **No retrospective recheck of any other directory is needed** —
established when the bug was first found (see B3 finding 5): the
corruption is always a hard, unsuppressible compile error, never able to
silently reach committed code, so B1/B2 (predating this morning's
vulnerable code) and the classpp-fix's own 7-directory retrospective
recheck (which ran with the vulnerable code but gated clean throughout)
are both confirmed unaffected.

### T1 (`atk/text`, 30 files, 21 classes, 2026-07-30) — opens Wave 3

Full findings: `claude-history/m3-t1-atk-text-REPORT.md`. The first
full-pre-diagnosis batch since B3 (per the "Session structure going
forward" section above) — the orchestrator's pre-diagnosis found this
directory unusually clean (zero required `.ch`/`.c` fixes before the
real run, unlike B1/B2/B3), and the real run confirmed it on every
metric the dry-run pass could see (0 DRIFT, the 3 pre-diagnosed
confirmed-safe skips, all ~12 double-pointer parameters intact — the
orchestrator's specific investigation of that risk, prompted by this
directory having far more double-pointer K&R parameters than B3's
single `tlex.c` instance, held up under the real compile-gated
conversion too). Two things a dry-run structurally cannot see surfaced
once the real, `-pe`-live, compile-gated run actually happened:

1. **Real bug: `textv.ch`'s `ViewMove` declared the wrong parameter
   type.** `struct mark *currentLine` in the `.ch`, but the real
   implementation and all 7 real call sites always used
   `struct linedesc *` (an unrelated structure — the implementation
   dereferences fields, `data`/`nChars`/`height`/`xMax`/`containsView`,
   that exist only on `linedesc`). Same species as B3's `treev.ch`
   `SetHitHandler` finding: a `.ch` parameter *type* mismatch, not a
   DRIFT-shaped argument-*count* mismatch, so invisible to `ansify`'s
   own check and only caught once `-pe` type-checked the exported
   prototype against real usage. Fixed by correcting the `.ch` to match
   the implementation and every caller (which all already agreed with
   each other); zero behavior change.
2. **Real bug: `textv.c`'s file-local `HandleSelection` called with a
   stray extra argument at all 5 call sites.** Not a `.ch` issue at
   all — a plain file-local static helper whose real 2-parameter
   definition never referenced a 3rd argument every caller nonetheless
   passed, silently tolerated by K&R's lenient calling convention for
   the function's entire life. Same species as B1's `im.c`
   colormap-dereference finding and B2's `sbuttonv.c` wrong-variable
   finding: a real caller bug, invisible under K&R, that becomes a hard
   "too many arguments" compile error the moment the callee gets a real
   ANSI prototype. Fixed by dropping the stray argument at all 5 call
   sites (matching the function's own always-correct behavior) rather
   than adding an unused parameter to the callee.
3. **New tooling-interaction pattern, not a source bug: stranded
   old-style forward declarations vs. newly-ANSI'd narrow parameters.**
   `ansify` converts K&R *definitions* to ANSI but does not touch
   separate, old-style (empty-parens) forward *declarations* of the
   same function earlier in the file. Normally harmless — an
   unprototyped declaration is compatible with any later definition —
   but once the definition gains a narrow by-value parameter (`char`,
   `short`, `unsigned char`), the two become a genuine ISO C
   conflicting-types error (default-argument-promotion incompatibility,
   not a compiler quirk or tool bug). Found in exactly 3 isolated
   instances (`drawtxtv.c`'s `CharToOctal`, `pcompch.c`'s `scanerr`,
   `txttroff.c`'s `quote`), each fixed by hand-updating the stale
   forward declaration to match its real definition. Confirmed via a
   full `make -k` across the whole directory that this was the complete
   set. Not tool-patched (3 bounded instances, same proportionality
   precedent as B3's array-bracket/double-pointer hand-fixes) — now a
   permanent standing checklist item (see item 8 above) for future
   batches.
4. **A non-deterministic `ansify` compile-gate flake, not a code bug**:
   across two otherwise-identical real `ansify --dir` runs against the
   same `-pe`-live environment, the same 3 files reported
   `COMPILE FAILED` both times but against a *different* set of
   specific functions each time, while isolated single-file re-tests of
   the exact same converted content consistently compiled clean.
   Root cause not chased down (the authoritative signal — the full
   directory gate, run twice — was unambiguous and clean both times);
   flagged for whoever next investigates `ansify`'s compile-gate
   reliability at scale, not blocking.
5. **A build-state gap specific to a directory's very first M3 session
   in this checkout**: `atk/text` had never had a local build pass run
   over it before, so `ansify --dir`'s own compile-gate had no
   `.eh`/`.ih` to test against at all (`fatal error: file not found`
   across 29/30 files on the first attempt) — fixed by running a plain
   `make depend && make -k install` baseline first. Not previously
   anticipated by any prior batch (all of which happened to already
   have a build present); worth remembering for any future batch that
   turns out to be a directory's first-ever M3 touch.

Also confirmed (2026-07-30, independently, by the orchestrator): both
`textv.ch`'s `ViewMove` fix and `textv.c`'s `HandleSelection` fix
verified directly against the real implementation and every real call
site (not just the diff); the 3 forward-declaration fixes confirmed to
match their definitions exactly; full gate re-run — double clean cycles
on `atk/text` itself, both independently reproducing the delegate's
result; `-pe` confirmed live and all lifecycle/double-pointer prototypes
confirmed correctly typed in the regenerated `.eh` files; `fossil
status` matched exactly (32 edited files, no commit made); a retroactive
tree-wide `dependInstall` gate run to close out Wave 3 (per "Gate
scope"'s wave-end-checkpoint rule) came back clean except for the same
two already-documented, pre-existing `contrib/zip/utility/ltapp.c`
errors noted at the Wave 1/2 retroactive gate above (still queued for
Wave 7 C2, not T1 fallout).

### `ansify` signature-DB case-collision fix (tool fix, not a batch, 2026-07-30)

Found at I1's Gate 0 (the first batch to use the delegate-side Gate 0
pattern): `ansify --build-db` names each class's `.desc` file after
the class name with no collision check
(`shutil.copyfile(desc, os.path.join(dbdir, classname + '.desc'))`),
and `build/desc/` lives on this checkout's default case-insensitive
macOS filesystem. Two classes whose names differ only in case collide
onto the same file on disk, and whichever `.ch` is processed later in
`sorted(chfiles)` order silently wins — the earlier one's signature
data is gone with no error. Found live: `atk/image/sliderv.ch`
(`class sliderv : view`) and `atk/value/sliderv.ch` (already `-pe`'d
in B2, `class sliderV[sliderv] : valueview[valuev]`) both map to
`sliderv.desc`; since `atk/value` sorts after `atk/image`, the DB held
atk/value's data (`Class: sliderV`, `Subclass of: valueview, view,
observable, traced`) under atk/image's filename. Confirmed a real,
non-cosmetic consequence, not just a display artifact: the delegate
ran `convert_file()` directly against a scratch copy of
`atk/image/sliderv.c` and it emitted
`void sliderv__FinalizeObject(struct classheader *classID, struct sliderV *self)`
— atk/value's capitalized struct tag, undefined in atk/image's scope,
a hard compile error had the real batch run proceeded. The
orchestrator independently re-verified by reading the live
`build/desc/sliderv.desc` directly (its `FinalizeObject` entry read
`args: struct sliderV *`, confirmed wrong against the real `.c`
definition's `struct sliderv *self`) before ruling.

Fixed in `build_db()`: track a lowercased-classname → (real classname,
source `.ch`) map; on a second write to the same lowercased key from a
different real classname, refuse the overwrite and report it through
the existing `failed` list (visible in `--build-db`'s normal output,
not silent) instead of clobbering. First-processed-in-sort-order class
keeps its `.desc` file. A tree-wide lowercase-collision scan (both by
the delegate and re-confirmed by the orchestrator) found this is the
**only** colliding pair anywhere in `src/` — `atk/image/sliderv`
against `atk/value/sliderV` — so the fix's practical effect is exactly
one file: rerunning `--build-db` now correctly gives `atk/image` its
own data (verified: `sliderv.desc` now reads `Class: sliderv`,
`FinalizeObject` args `struct sliderv *`). No regression risk to
`atk/value`'s already-committed B2 conversion — that batch's `-pe`/
`.eh` rollout went through classpp directly against the `.ch` file,
never through this DB, so it never depended on `sliderv.desc` holding
its data. One unrelated pre-existing failure surfaced by the same
rebuild, confirmed harmless: `contrib/atkbook/console/disk1.ch` fails
classpp itself (`can not open the file getstatsob.ch`) — `atkbook` is
one of the already-documented dead/conditionalized-out directories
(`rollout-procedure.md`'s Liveness census list), not M3 fallout, not
new.

### `ansify` signature-DB same-classname collision fix (tool fix, not a batch, 2026-07-31)

Found at A1's Gate 0 (Wave 5): the 2026-07-30 case-collision fix above
only guards `prior[0] != classname` — two *different* classnames that
collide once lowercased. It does not catch two `.ch` files declaring
the **identical** classname, which the original code (and the fixed
code, unchanged in this respect) treats as an ordinary re-write: no
case-difference means the guard's `if` is false, so `seen_lower[lower]
= (classname, ch)` and `shutil.copyfile(...)` run unconditionally, and
`sorted(chfiles)` order silently decides the winner — with zero
`failed`-list entry, unlike the case-collision variant, which was at
least visible in the tool's own output.

Live instance: `src/rdemo/hide/tscript.ch` and `src/rdemo/hide/
typetext.ch` — a dead/hidden demo tree (the directory is literally
named `hide`, not in `m3-batches.md`'s 90-directory active list) —
declare `class typescript[tscript]` and `class typetext`, the exact
same classnames as the real, active `atk/typescript/tscript.ch`/
`typetext.ch` (this batch's own scope). Because `rdemo` sorts after
`atk`, the dead tree's stale content silently overwrote the real
one's `.desc` entries with no error: `typescript.desc` lost 6 genuine
signatures (`SetTitle`, `GetTitle`, `SetFrame`, `GetFrame`, `Create`,
`CreatePipescript`) that the dead copy's older revision never had, and
`typetext.desc` gained 3 fabricated lifecycle entries (`InitializeObject`/
`InitializeClass`/`FinalizeObject`, `defined by:` with no `declared
by:` line — a shape no genuine DB entry has) from the dead copy's
different `.ch` structure. The delegate found this by a tree-wide
isolated-vs-real diff over all 566 `.ch` files: 27 mismatches, 100%
correlated with `.ch` files whose bare filename recurs elsewhere in
the tree — hand-patched just the 2 in-scope `.desc` files (non-source,
non-fossil-tracked derived artifacts) to unblock its own Gate 0 read,
then correctly stopped and flagged the general mechanism as
UNCLASSIFIED tool-construction work per the Delegation ruling, rather
than guessing at a source-level fix.

The orchestrator traced the root cause before ruling (confirmed via
`head` on both file pairs: identical `class typescript[tscript]`/
`class typetext` declarations) and fixed `build_db()` for real,
mirroring the existing case-collision guard's shape rather than
inventing a new mechanism: on a same-lowercased-key hit, branch on
whether the classname is identical (this fix, new) or merely
case-different (2026-07-30's fix, unchanged) — both branches now
report through the `failed` list instead of one of them clobbering
silently. Verified with a real `--build-db` run: 496 classes written,
70 reported failures (up from the pre-fix run's 564 ok/2 failed — most
of the newly-caught 70 were previously silent same-classname
overwrites, not new problems). Confirmed all 70 are confined to 4
non-active-batch directory trees (`atk/examples`, `contrib/atkbook`,
`rdemo/hide` — all dead/demo, none in the 90-directory active list —
plus `atk/value`'s already-known, already-documented `sliderv`/
`sliderV` case-collision, unaffected by this change). `typescript.desc`/
`typetext.desc` now regenerate correctly and directly from the real
tool (112 and 78 `Method:`/`Class Procedure:` lines respectively,
matching the delegate's hand-patched read) — superseding the
delegate's temporary hand-patch, and lifting its "don't rerun
`--build-db`" operational caveat: the DB can be freely rebuilt from
here on. Committed standalone (`fa0705b6`), same pattern as the
2026-07-30 fix and every other tool-construction fix this rollout —
tool changes stay top-level, never inside a batch's delegate session.

### `fix-missing-static-decl` duplicate-declaration fix (tool fix, not a batch, 2026-07-30)

Found continuing I1's Gate 1 on `atk/image` (the orchestrator ran this
directly rather than through a delegate — see below): fixing a
standing-checklist-item-8 stranded forward declaration by hand (`img.c`'s
`IMG_WriteByte`, upgrading `static void IMG_WriteByte();` to the real
full prototype, T1's established fix pattern) before running `ansify
--dir` for real caused a *second*, unrelated tool bug to fire.
`run_fix_tools()` runs `fix-missing-static-decl` before `convert_file`
on every real (non-dry-run) `ansify` invocation; that tool's own
"is this name already declared" scan
(`try_parse_decl_block`'s inner regex) only recognizes the traditional
K&R empty-parens shape (`NAME();`), by design (its rewrite path
re-emits recognized entries as bare `NAME()`, so teaching it to also
*rewrite* non-empty-parens declarations would risk silently dropping
real parameter types — deliberately out of scope). A full, already-fixed
ANSI prototype therefore doesn't match, falls through as "no forward
declaration found," and the tool inserts a brand-new *duplicate*
`static void IMG_WriteByte();` right after the `#include` block —
conflicting with the very declaration that was just hand-fixed.
Confirmed via the standard `.ansify-orig`-backup / isolated-recompile
technique (backup gets deleted on failure so the isolated repro used
`fix-missing-static-decl` directly against a scratch copy to see the
duplicate insertion, then a second scratch test to confirm the full
`fix-tools → convert_file → compile` pipeline reproduced the same
"conflicting types" error the batch run showed).

This is not a one-off: it would recur for every one of I1's other
already-catalogued narrow-param stranded-forward-declaration instances
(`atk/srctext`'s 12, `atk/fad`'s `MySetStandardCursor`, etc.) the
moment any of them got the same hand-fix-then-reconvert treatment, so
worth a tool-level fix rather than a per-instance workaround. Fixed by
adding a second, independent recognizer,
`find_full_prototype_names()` — a brace-depth-gated, file-scope-only
scan for a complete single-line `TYPE NAME(args);` declaration — whose
results are unioned into `matched_names` before the "missing declare"
set is computed. Deliberately kept separate from
`try_parse_decl_block`'s own matching/rewrite path rather than
broadening that regex in place, so the existing (working, already
retrospectively-verified) empty-parens rewrite behavior is untouched;
this only ever *removes* names from the "missing" set, so it cannot
regress a case where a real declaration was genuinely absent and
needed inserting. Verified: `img.c` reprocessed through
`fix-missing-static-decl` standalone now reports zero changes (correctly
recognizes the existing prototype), and the real `ansify --dir
src/atk/image` run immediately after converts `img.c` cleanly with no
compile failure. A first version of the fix used `\s*;\s*$` without
accounting for a trailing `/* comment */` (this codebase's very common
style on a forward-declaration line, including `img.c`'s own line) and
so didn't fire at all on the first attempt — corrected by borrowing the
file's existing `TRAILING_COMMENT` pattern, the same fix already used
elsewhere in this file for exactly this reason.

### `ansify` array-parameter `weave()` fix (tool fix, not a batch, 2026-07-30)

Found continuing I1's Gate 1 on `atk/srctext`: the real (non-dry-run)
`ansify --dir` run reverted `srctext.c` entirely with a hard parse
error (`expected ')'`, cascading into several bogus "undeclared
identifier" errors past that point). Traced to `srctext__HashInsert`
and `srctext__BuildTable`, both declared in `srctext.ch` as
`classprocedures` taking an array-of-pointer parameter
(`HashInsert(Dict *hashTable[], Dict *word);`). `convert_file`'s
`weave()` helper (responsible for combining a signature-DB types-only
string with a parameter name for exported classprocs) mis-rendered
the array parameter as `Dict * [ ] hashTable` — brackets floating
*before* the name, invalid C. Root cause is one level further back
than `ansify` itself: classpp's own `-D` describe output renders an
array-of-T parameter's type as `T  [ ]` (**with a space between the
brackets**, confirmed directly in `build/desc/srctext.desc`:
`args:		Dict *  [ ], Dict *`) rather than the bracket-adjacent `T[]`
`weave()` was written to expect (`t.endswith('[]')`) — so the
recognizer silently missed it and fell through to the generic
`f"{t} {name}"` case, which is correct for an ordinary type but wrong
for a floating array suffix. Fixed by matching loosely
(`re.search(r'\[\s*\]\s*$', t)`) instead of a literal `endswith('[]')`,
splitting on the match position rather than a hardcoded 2-character
slice. Verified: `weave('Dict *  [ ]', 'hashTable')` now returns
`'Dict * hashTable[]'`; the real (non-dry-run) `ansify --dir
src/atk/srctext` run converts `srctext.c` cleanly, no compile failure.

Not a one-off inside this one file: a tree-wide grep of `build/desc/
*.desc` for this shape found roughly a dozen more array-of-`char`
class-procedure parameters, concentrated in the same `atk/srctext`
class family (`asmtextview`, `ctextview`, `cpptext`, `m3textview`,
`mtextview`, `modtextview`, `srctextview` — all in this same I1
batch) plus one instance in `folders` (a not-yet-converted directory,
queued for later — the fix is already in place for whenever that
batch arrives). No prior batch could have depended on the old broken
behavior succeeding: it always produced invalid syntax, which the
compile-gate's restore-on-failure property would have caught and
reverted every time, the same way it caught this one.

### `ansify` two-line-return-type comment fix (tool fix, not a batch, 2026-07-30)

Found continuing I1's Gate 1 on `atk/layout`: the real `ansify --dir`
run reverted all 6 files in the directory, every one with cascading
parse errors (`redefinition of 'boolean' as different kind of
symbol`, `expected identifier or '('`, etc.) starting from a literal
duplicated return-type token. Traced to this codebase's common K&R
idiom of putting a classproc's return type alone on its own line with
a trailing comment, name and params starting the next line:

    boolean					/* always returns TRUE */
    layoutview__InitializeClass(classID)
    struct classheader *classID;
    {

`convert_file` already has a mechanism for this exact "two-line form"
(`prev_type` detection against the immediately preceding emitted
line, `TYPEONLY.match(out[-1].strip())`, popping that line once a new
typed header replaces it) — but `TYPEONLY`'s regex required the
previous line to be *only* a type token with nothing else on it, no
tolerance for a trailing comment. `HDR` (the main declaration-header
regex, three lines above `TYPEONLY`) already tolerates exactly this
shape (`(?:/\*.*?\*/\s*)?$`); `TYPEONLY` was simply never given the
same allowance. So the old `boolean .../* comment */` line was never
recognized as "the type half of a two-line declaration," never
popped, and a second, fully-typed header line got emitted right below
it — two `boolean` tokens back to back, guaranteed invalid C, and
(because this parse failure cascades) enough to corrupt the compiler's
sense of every following top-level declaration in the file too. Fixed
by widening `TYPEONLY` to accept the same optional trailing-comment
suffix `HDR` already does. Verified: `layoutview__InitializeClass`
converts to a single correctly-typed header line with no duplication;
all 6 files in `atk/layout` (5 of the 6 hit this shape) now convert
and compile cleanly where all 6 failed before.

Same character as the `weave()` array-bracket fix above: a
long-standing idiom in this codebase (return type alone on its own
commented line) that no earlier M3 batch happened to contain,
surfaced by I1 rather than being new. Worth a standing-checklist watch
item for any later batch that reverts *every* file in a directory at
once with `boolean`/`void`/etc.-adjacent duplication-flavored parse
errors — the fix is now in place tree-wide, but any earlier-processed
directory that happens to share this idiom would only have hit it if
it also happened to use the two-line-with-comment return style, which
no committed batch (O1-T1) did.

### I1 (10 inset-adjacent directories, 70 files, 2026-07-30) — opens Wave 4

First batch to actually run under the delegate-side Gate 0 pattern
(the amendment was adopted after B3 but T1, its first candidate, was
itself one of the flagged-risky batches and got full orchestrator
pre-diagnosis instead). Gate 0 went as designed: the delegate ran the
standing checklist plus `ansify --dry-run` across all 10 directories
and classified everything against the documented taxonomy, correctly
flagging exactly one genuinely new item for a ruling — see the
`ansify` signature-DB case-collision fix entry above (§3.1 in the
batch's own report), found and ruled on before Gate 1.

**Gate 1 was run by the orchestrator directly, not the delegate.**
Resuming the delegated session after the §3.1 ruling, the real
(non-dry-run, file-mutating) `ansify --dir` invocation was denied
outright by Claude Code's auto-mode permission classifier, with no
interactive prompt available in a background subagent session. The
orchestrator hit the identical block attempting the same command
directly — confirmed transient (cleared after a session resource-limit
reset, not a durable policy) — and completed the rest of Gate 1
personally rather than re-attempting delegation. Noted here as a
process fact, not a new finding class: unlike every prior M3 batch,
this one has no separate "delegate result, independently re-verified
by the orchestrator" structure, because the orchestrator did the work
firsthand.

Three more tool bugs found and fixed during Gate 1, all committed
standalone ahead of this batch's own commit (see their own entries
above for full detail): `fix-missing-static-decl` inserting a
duplicate declaration on top of an already-fixed full prototype
(`d6b52e03`/`25f56793`); `weave()` mis-rendering an array-of-pointer
classproc parameter because classpp's own `-D` dump puts a space
inside the brackets (`bda581c2`/`8a39c68e`); and the most
consequential, `TYPEONLY` not tolerating a trailing comment on a
two-line K&R return-type declaration, which reverted every file in
`atk/layout` at once (`4caa5b0d`/`cd05b37e`). Combined with the
`sliderv`/`sliderV` DB-collision fix from Gate 0, **I1 found four
tool bugs**, more than any prior single M3 batch — plausibly because
it's the first batch spanning enough directories and stylistic
variety at once (10 directories, several distinct authors' idioms) to
hit shapes a single-directory batch wouldn't surface.

Real (`.ch`-vs-implementation or caller-vs-callee) bugs found, same
species as T1's `ViewMove`/`HandleSelection` and B1/B2's caller-bug
findings:
- `atk/image/cmapv.ch` restated `InitializeObject`/`FinalizeObject`'s
  `self` as `struct colormap *` — a real, unrelated, already-`-pe`'d
  class (`atk/basics/common/cmap.ch`) — instead of its own `struct
  colormapv *`.
- `atk/image/pbm.c`'s `pbm__Ident` called the file-local `isPBM` with
  a stray, always-ignored 6th argument at one of its two call sites.
- `atk/srctext/asmtextv.ch` and `srctextv.ch` both restated
  `SetDataObject`'s parameter as a covariant, class-specific type
  rather than the base `view` class's generic `struct dataobject *`
  every other already-`-pe`'d class in the tree uses for this same
  override (`atk/adew/celv.c`, `atk/text/textv.c` both confirmed use
  the safe, generic-type-plus-internal-cast pattern). classpp's own
  `-pe` always exports the base type for an overridden method
  regardless of what the override's `.ch` claims, so the covariant
  declaration was always going to be misleading — K&R just never
  checked it.

Ordinary standing-checklist-item-8 (stranded narrow-param forward
declaration) fallout, mostly anticipated at Gate 0 but with a few
misses corrected during Gate 1: `atk/image/img.c`,
`atk/srctext/cpptext.c` (`isOperatorOverload`, missed at Gate 0),
`atk/srctext/srctext.c` (`base64value`, missed at Gate 0),
`atk/srctext/{cpptextv,ctextv,m3textv,modtextv,mtextv,srctextv}.c`
(the 12 Gate-0-predicted instances, all confirmed), `atk/bush/bushv.c`
(`Format_Tags`, missed at Gate 0), `atk/fad/fadv.c`
(`MySetStandardCursor`, Gate-0-predicted). `m3textv.c`/`mtextv.c` each
also had a duplicate declaration (once alone, once again in a shared
comma-list) for the same name — pre-existing, harmless-under-K&R
redundancy, both instances fixed; `srctextv.c` had a full 23-name
comma-list block that was entirely redundant with individual
declarations already present above it, deleted outright.

Four confirmed pre-existing dead-code instances, not M3 fallout:
`atk/image/g3.c`, `sliderv.c`, `xpixmap.c` (fail to compile even
unconverted, absent from the Imakefile's `DOBJS`) and
`atk/org/orga.c` (the already-anticipated §17 empty-parens DRIFT,
also absent from its Imakefile) — same "present in the directory,
absent from the real build" shape as the dead-file discovery in
`atk/image`.

All 10 directories gated clean, twice each, after fixes landed. Per
the "Gate scope" section, no tree-wide gate for this batch — I1 and
I2 together are Wave 4; the wave-end tree-wide gate is deferred to
whenever I2 closes the wave. Full per-directory detail, gate output,
and the "Suggested runtime checks for wdc" section are in
`claude-history/m3-i1-insets-batch1-REPORT.md`.

### I2 (`atk/eq`, `atk/figure`, `atk/chart`, `atk/table`, `atk/rofftext`, `atk/raster/cmd`, 6 directories, 72 files edited, 2026-07-31) — closes Wave 4

One of the four flagged-risky batches (full orchestrator pre-diagnosis,
not delegate-side Gate 0), chosen for `atk/figure`'s M1 Pilot B
history. Pre-diagnosis's single most consequential finding: `atk/eq`'s
`eqparse.c`/`.h` and `atk/rofftext`'s `num.c`/`.h` are bison-generated
build artifacts (`Parser()` Imake macro, confirmed via `fossil finfo`
returning "no history for file" for both) — same shape M2's own pilot
and rofftext batch already documented. Excluded from `ansify`
entirely; the subtree-local gate recipe needs an explicit `make
depend` between `clean` and `install` for these two directories
specifically, or the regenerated `.h` never reappears and downstream
`#include` failures mask real fallout. Independently re-confirmed by
the orchestrator post-batch: `make clean` deletes both generated
pairs, `make depend` regenerates them, both directories gate clean.

`atk/figure` got the extra scrutiny its history warranted: all three
of Pilot B's original fixes (typeless `MoveHandle`, the `ToolName`/
`Instantiate` rock idiom, `Build`'s `(v, action, ...)` parameter
order) confirmed still intact, no regression. It surfaced six new real
bugs anyway (below) plus the batch's one genuinely new fallout shape:

**New fallout shape — enum prototype-scope vs. include order (not
previously in the M3 taxonomy).** Seven `atk/figure` files `#include`d
their own `.eh` *before* the header fully defining an `enum` type used
in one of their typed method prototypes (`enum view_MouseAction` from
`view.ih`, `enum figobj_HitVal` from `figobj.ih`). Harmless under K&R;
under `-pe`'s typed prototypes, C's rule that an enum tag first
mentioned inside a function-prototype parameter list gets prototype
scope means the early mention creates a distinct incomplete type from
the one the real definition later uses — `conflicting types` on every
method touching that enum. Fixed by moving each file's own `.eh`
include to the end of its include block, matching the already-`-pe`'d
working precedent (`atk/text/textv.c` includes `view.ih` before
`textv.eh`). Two of the seven instances (`figorect.c`, `figoins.c`)
were missed on a first pass because an individual `make X.o`
spot-check "succeeded" against a stale `.o` — restates Pilot A finding
#3 as a hard rule: an untouched file's isolated `make X.o` proves
nothing, only a post-`clean` full gate does.

Real `.ch`-vs-implementation bugs, same species as prior batches'
mistyped-self and generic-param-plus-cast findings: `eqv.ch`'s
`InitializeObject` restated `self` as `struct eq *` instead of `struct
eqview *`; `figobj.ch`/`figogrp.ch`'s `SetParent` both had a malformed
`struct *fig ancestor` (missing the actual struct tag, should be
`struct figure *`); `figorrec.ch`'s `InitializeObject` restated `self`
as a sibling class's type (`struct figoell *`, should be `struct
figorrec *`); `figv.c`'s `SetDataObject` and `fontselv.c`'s
`ObservedChanged` both accessed covariant-typed fields through the
generic base-class parameter `-pe` actually exports (same rule I1's
`srctextv.ch`/`asmtextv.ch` finding established), fixed with a local
cast, matching `celv.c`'s convention; two file-local helpers in
`figv.c`/`figure.c` had their own `rock` parameter still `long` where
the `.ch`-declared method they're called from already correctly took
`void *rock` (the file-local-helper counterpart of Pilot B's original
rock-idiom finding). `atk/chart`: `chart.ch`/`chartv.ch`'s `Create`
classprocs had a bare, untyped specification-typedef token as their
parameter (Pilot B finding #1's typeless-`.ch` shape, confirmed real
typedefs with real K&R pointer usage). `atk/raster/cmd`:
`raster.c`'s `ObservedChanged` and `rastvaux.c`'s `SetDataObject` both
had the same generic-param-plus-cast shape as `atk/figure` above.

**Two new tool-adjacent quirks, both bounded/single-instance, neither
tool-fixed** (worked around by hand in the affected `.ch`/`.c` file
instead — per Delegation, tool construction stays top-level and a
one-occurrence shape doesn't justify a standalone fix commit; flagged
here in case either shape recurs in a later wave):
- **`classpp -D`'s signature dump blanks the class tag when a
  classproc parameter's declared type textually equals the class's
  own self type.** `chartapp.ch`'s `InitializeObject(struct chartapp
  *)` (type given, no parameter name) round-trips through the dump as
  `args: struct  *` — the tag itself goes missing, confirmed directly
  in `build/desc/chartapp.desc`. `ansify` faithfully converts the
  malformed text it's handed; the bug is upstream, in `classpp -D`'s
  dump itself. Worked around by hand-fixing `chartapp.c`'s definition
  directly rather than chasing a fix in `class.c`.
- **`ansify` mis-renders cast-style K&R function-pointer parameter
  syntax.** `chart.ch`'s `Apply` used `(long(*)())proc` (a cast-style
  K&R declarator, not the conventional `long (*proc)()`); both
  `classpp` and `ansify` render this awkwardly with redundant grouping
  parens in the generated `.c` definition (`( long (*proc) () )`),
  which C parses as an unnamed abstract declarator — `proc` never
  becomes a real parameter name, a genuine parse-level failure. Worked
  around by rewriting `chart.ch`'s declaration to the conventional
  syntax before conversion; not investigated as a tool fix (single
  instance, tree-wide grep for the same cast-style shape elsewhere
  turned up nothing).

Two confirmed-dead/confirmed-live edge cases, same "declared-vs-wired"
family as I1's dead-file findings but at function granularity:
`figospli.c` defines `figospli__InitializeClass`, but `figospli.ch`
never declares `InitializeClass` and (unlike `InitializeObject`/
`FinalizeObject`, always auto-wired per the §17 mechanics note)
`InitializeClass` is only wired if declared — confirmed dead via
`figospli.eh` having zero mentions, left as K&R, flagged as a
possible-but-out-of-scope behavior question (a once-per-class setup
that silently never runs). `chartx1a.ch` has no `classprocedures`
section at all, yet `chartx1a.c` defines
`chartx1app__InitializeObject` — this one **is live** (confirmed via
`chartx1a.eh`'s standard classpp-generated `New()` wiring, same M1
mechanics note), a genuine `ansify` signature-DB coverage gap (the DB
only captures classprocs a `.ch` explicitly declares) rather than dead
code; hand-converted to ANSI directly. **Precise mechanism traced by
AMS2 (2026-08-01), confirming two more instances**
(`msgsa.ch`/`messagesapp`, `text822.ch`/`text822`): `class.c:2814` sets
the `initializeobject` flag `TRUE` for **any** class with a non-empty
`data:` section, independent of whether `InitializeObject` appears in
the `.ch`'s `classprocedures` section at all (the flag's own comment,
`class.c:165`: `"TRUE if this class has data or initializeobject
procedure was found"`). Any class with real instance data therefore
gets a live, auto-wired `InitializeObject` call regardless of what its
`.ch` declares — the same DB-coverage gap as `chartx1a`, now with the
exact codegen trigger identified rather than just the observable
effect.

Also confirmed pre-existing, not M3 fallout: `atk/raster/cmd`'s
`rastervt.c` (`rasterviewtest`, wired only into the Imakefile's
`test::` target, never `install::`/`all::`) fails to compile even in
its pristine committed K&R form — an `environ_AndrewDir` macro
suppressed by its own `class_StaticEntriesOnly` define, contradicting
its own comment — same "ansify's gate reaches a file the real build
never builds" shape as O1's precedent; left untouched, the subtree
gate never builds it either way. And, unrelated to M3: `atk/chart`'s
Imakefile has never had the M2-era `COMPILERFLAGS =
-Werror=implicit-function-declaration` guard every other `-pe`'d
directory has — no `m2-*-REPORT.md` mentions it, so this directory has
run without an implicit-declaration compiler net this whole time.
Out of scope for a `-pe`-only batch to fix. **Closed out same day as a
standalone follow-up (`0b40b488`, 2026-07-31), not folded into I2**:
scouted first (temporarily flipped the flag, gated, reverted) rather
than guessed at — turned out much smaller than the "could be a real
investigation" caution above implied. 10 `implicit-function-declaration`
errors, all cross-file, same-directory, non-static helper calls with
no shared declaration: `chartv.c` calls 4 functions defined in
`chartp.c` (`Destroy_Palette`, `Activate_Viewer`, `Hide_Palette`,
`Expose_Palette`); `chartp.c` calls 5 defined in `chartv.c`
(`chartv_Add_Command`, `chartv_Delete_Command`, `chartv_ReChart`,
`chartv_Print_Command`, `chartv_Save_Command`); `chartx1a.c`'s `Query`
is used before its own later same-file definition. All 10 return
`int`/`void` (no pointer-returning functions), so no actual LP64
truncation risk in this set — fixed by adding proper ANSI forward
declarations at each site. One knock-on fix once `chartv_ReChart`
became properly typed: `chartp.c:568` passed `suite_ItemAttribute()`'s
`long` return value (a dual-use attribute value, same pattern as B1
finding #3) directly to `chartv_ReChart`'s `char *moniker` parameter —
added the precedented `(char *)` cast. Gated clean, twice. wdc smoke
tested the `chart` app's palette (add/delete/rechart/print/save/hide/
show) plus the `chartx1a` "Home Budget" example program directly — all
passed.

Anchored `malloc`/`free`/`realloc`/`calloc` grep (checklist item, the
clang-builtin blind spot) found and fixed 24 files across `eq`
(4/4), `figure` (12/17), `chart` (6/13), and `raster/cmd` (2/8) missing
`#include <stdlib.h>`; `table`/`rofftext` already had it everywhere it
was needed.

All 6 directories gated clean, twice each. Wave-end tree-wide gate
(`make -C src dependInstall`, twice) clean except the two
already-documented, already-queued `contrib/zip/utility/ltapp.c`
errors (Wave-1/2 baseline, unchanged) — closes Wave 4. Independently
re-verified by the orchestrator: re-ran the tree-wide gate directly
(same 2 baseline errors, nothing new), did full from-scratch
`clean`/`depend`/`install` rebuilds of `atk/figure`, `atk/chart`,
`atk/eq`, `atk/rofftext` personally (all clean), and spot-checked
roughly a dozen of the specific claimed fixes directly against the
diff (all matched exactly). wdc ran every suggested runtime check in
§8 of the batch report and confirmed all passed; also removed a
pre-existing untracked `fontselv.c.orig` stray file (dated 2026-07-24,
predates this batch) noticed during the report's `fossil extras`
check. Full per-directory detail, gate output, and the runtime-check
list are in `claude-history/m3-i2-insets-batch2-REPORT.md`.

### A1 (`ams/msclients/nns`, `atk/typescript`, `atk/help/src`, `ams/msclients/cui`, `atk/help/maint`, `atkams/messages/cmd`, `ams/msclients/imapsync`, `atk/ez`, `doc/mkbrowse`, 9 directories, 29 files, 2026-07-31) — opens and closes Wave 5

First batch to run the delegate-side Gate 0 pattern for a wave with
only one batch — closes Wave 5 on its own. `-pe`/`.eh` added for real
to the 3 directories with `.ch` files (`atk/typescript`, `atk/help/
src`, `atk/ez`); the other 6 had none, matching the prompt's own
prediction exactly.

Gate 0 surfaced a genuinely new, more severe variant of the
2026-07-30 `ansify` signature-DB case-collision bug (see that entry
above): `build_db()`'s guard only catches classnames differing by
*case*; it does not catch two `.ch` files declaring the **identical**
classname, which fall through the guard entirely and silently
overwrite with zero `failed`-list entry — worse than the case-collision
variant, which was at least visible in the tool's own output. Live
instance: `src/rdemo/hide/{tscript,typetext}.ch` — a dead/hidden demo
tree — declare the same classnames as this batch's real `atk/
typescript/{tscript,typetext}.ch`; `rdemo` sorting after `atk`
silently corrupted `typescript.desc`/`typetext.desc` (6 real
signatures dropped, 3 fabricated). The delegate found this via a
tree-wide isolated-vs-real diff (27 mismatches, 100% correlated with
duplicate `.ch` basenames), hand-patched the 2 in-scope `.desc` files
to unblock its own Gate 0 read, and correctly stopped rather than
guessing at a source-level fix (Delegation ruling: tool construction
stays top-level). The orchestrator traced the root cause and fixed
`build_db()` for real (branch on identical-vs-case-differing classname,
both now report through `failed`) — see the dedicated "`ansify`
signature-DB same-classname collision fix" entry above for full detail.
Committed standalone (`fa0705b6`/`214801d5`), superseding the
delegate's temporary hand-patch. A second Gate 0 finding (a new,
bounded `ansify` parser-gap shape — `struct TYPE* name;`, asterisk
glued to the type rather than the variable, 4 functions in `atk/
help/src/help.c`) was ruled routine and self-classifiable (same
family as prior bounded parser gaps: safe reported skip, zero-risk
hand-fix, tool patch deferred until instance counts justify it) and
fixed by hand at Gate 1.

Gate 1 found further fallout invisible to Gate 0's static/dry-run
pass, all matching already-documented species recurring in a new
form: `atk/typescript/tscript.c`'s 3 predicted narrow-param stranded
forward declarations (T1's shape) became real compile errors once
`-pe` went live; `atk/help/src/helpa.c` hit a `fix-missing-static-decl`
non-idempotency variant (the tool inserted 5 fresh empty-parens stubs
for functions with *no* prior declaration at all, one of which
immediately conflicted with its own narrow-param real definition — a
same-run version of the standing item-8 hazard, self-inflicted by the
tool rather than pre-existing); `ams/msclients/cui/{cui,cuifns}.c`
had two ~35-year-old zero-forward-declaration gaps (`GetHeadersFn`,
`MaybeFlagSomething`, called long before their real definitions with
no declaration anywhere); `ams/msclients/imapsync/imap_sync.c` — the
one file in this batch already almost entirely hand-written ANSI C —
had a 25-entry stale K&R forward-declaration block hiding a variadic-
stub conflict (O3's shape) and B3's function-prototype-scope
struct-tag trap (`struct sync_state` first appearing inside a new
forward declaration's parameter list); `doc/mkbrowse/browser.c`'s
`<stdlib.h>` fix exposed a stale `char *malloc()` K&R redeclaration
the file's own comment already flagged as wrong, removed. All 9
directories gated clean twice, serially; wave-end tree-wide gate
(twice) clean except the two already-documented, already-queued
`contrib/zip/utility/ltapp.c` errors (unrelated, still queued for
Wave 7 C2) — closes Wave 5.

Independently re-verified by the orchestrator: `fossil status` matched
exactly (32 files: 29 batch source + 3 Imakefiles), ~6 specific
claimed fixes spot-checked directly against the diff (all matched),
full from-scratch `clean`/`depend`/`install` rebuilds of the 3 most
complex directories (`ams/msclients/imapsync`, `atk/typescript`,
`atk/help/src`) done personally (all clean), and the wave-end
tree-wide gate re-run directly (same 2 baseline errors, nothing new).

wdc's runtime checks found one real bug the static/compile-time gates
could not have caught: `cuin`'s `dirinfo` command crashed
(`malloc: pointer being freed was not allocated`). Root-caused by the
orchestrator: `cvEng()` (`ams/libs/shr/utils.c`, returns `char *`) is
called in `cuifns.c` with **zero declaration anywhere in `ams/
msclients/cui`** — this directory has never carried the M2-era
`COMPILERFLAGS` implicit-declaration guard, so the call silently
implicit-declared as `int cvEng()`, truncating the real pointer on
arm64 — the same LP64-missing-prototype species this project has hit
repeatedly, now caught live rather than latent. `dirinfo` was the
first exercised path in this batch's checks to reach `cvEng`; no other
tested command shares that code path. Checked systematically: of 141
functions this directory calls without any declaration (scouted by
temporarily flipping the COMPILERFLAGS guard on, the same technique
used for `atk/chart`'s follow-up), `cvEng` is the **only** one that
returns a pointer anywhere in the tree — everything else genuinely
returns `int`/`void`/`boolean`, matching K&R's implicit-int default
exactly, no truncation risk. Fixed with a single `extern char
*cvEng();` declaration in `cuifns.c`, matching the file's own existing
style and `ams/libs/cui/cuilib.c`'s identical declaration of the same
function. Rebuilt clean twice. wdc re-ran `dirinfo` and confirmed
clean output (and, incidentally, that an earlier `flag`/`unflag` test
had genuinely persisted). **Correction to this batch's own COMPILERFLAGS
scope note**: `ams/msclients/cui`'s gap is far larger than `atk/chart`'s
was — 141 undeclared functions (essentially the whole `MS_*`/`CUI_*`
AMS API, neither `ms.h` nor a full `cui.h` prototype set is included
in any of this directory's 4 files) vs. chart's 10 same-directory
helper calls — closer to `AMS1`'s scale than a quick same-day
follow-up; not attempted in this batch, flagged for whoever scopes it.

`typescript`/`ez`'s suggested runtime check in the batch report was
wrong and corrected: `typescript` is not wired into `ez`; it's its
own standalone `runapp`-loaded app (`build/bin/typescript -> runapp`,
installed from `atk/apps/Imakefile`), unrelated to `ez`. It doesn't
launch — traced to an already-known, already-tracked, pre-existing
bug (`roadmap.md`'s "typescript — crashes on launch," first found
2026-07-10, explicitly "not a regression" even then); confirmed the
exact crash site (`GetPtyandName`/"Can't connect subchannel" at
`tscript.c:1273-74`) is byte-identical, untouched by this batch's
diff. Not a checkin blocker. `help`, `imapsync`, `mkbrowse`, and
`mkindex` (no shipped docs — orchestrator read the source directly
and gave wdc a safe `-n`/fake-it-mode example) all ran clean; `nns`
untestable this session (no netnews access). Full per-directory
detail, the `cvEng` fix, and the runtime-check writeup are in
`claude-history/m3-a1-apps-REPORT.md`.

### AMS1 (`ams/libs/ms`, 113 real files, plus the `ams/msclients/cui`
COMPILERFLAGS closure add-on, 2026-07-31) — opens Wave 6

One of the four flagged-risky batches (full orchestrator
pre-diagnosis, not delegate-side Gate 0). Pre-diagnosis found
`prsdate.c` is bison-generated (same `Parser()`-macro mechanism as
I2's `eqparse.c`/`num.c`, excluded from `ansify`) and that the old
`fdplumb` caller-include-order concern this directory's history flags
is now stale — the 2026-07-17 fix (`6782de786a`) made `fdplumb.h`
self-contained (it includes `fcntl.h`/`sys/types.h` itself before its
own rename macros), so caller include order no longer matters; a
17-file audit finding "wrong" order was confirmed not a live risk.

Pre-diagnosis also scouted the folded-in `ams/msclients/cui`
COMPILERFLAGS amendment (from A1) and found its originally-proposed
mechanism doesn't work: `ms.h`/`cui.h` contain zero function
prototypes between them (both are type/struct/macro-only), so
`#include`ing them fixes nothing — verified empirically before
delegating. The real fix is scattered per-file `extern RETTYPE
FuncName();` declarations, the same idiom the `cvEng` fix already
used, cross-referenced against `ams/libs/ms`'s real signatures once
converted. See `m3-batches.md`'s Wave 6 section for the full
correction writeup.

**Gate 1 found a genuinely new, more dangerous `ansify` parser gap**:
K&R declarations spanning multiple physical lines (only the last line
terminated by `;`), or multiple complete declaration statements
crammed onto one physical line, are silently dropped by
`parse_decl_block` — **zero skip message, zero DRIFT entry, nothing**
in `ansify`'s own report. Worse than every prior parser-gap variant
(`(void)`-misparse, `DECLARE<N>` macro, brace-glued, double-pointer,
array-brackets, space-before-asterisk), all of which at least
self-report as a skip. 16 functions across 11 files were silently
left K&R this way; three successive hand-written greps each missed
real instances before the session imported `ansify`'s own
`HDR`/`parse_decl_block` logic and ran it directly against all 113
files to definitively close the sweep (0 misses on re-scan, confirmed
twice). All 16 hand-converted, matching `ansify`'s own weave/tidy_type
conventions exactly, plus ~16 more stale cross-file forward
declarations for the same 16 functions fixed for consistency (not
required for the gate).

**Orchestrator ruling (2026-07-31): do NOT fix `ansify`'s parser for
this** — unlike bug 2 (brace-glued) or bug 4 (brace-body corruption),
neither of which self-reported and both of which risked *silently
deleting or corrupting* real code, this gap's failure mode is a
compiling-but-incomplete conversion (the K&R definition still compiles
fine under `-std=gnu89` with no `-pe` requiring typed prototypes) —
annoying and effort-costly to find, but not a correctness landmine.
A real fix means reworking `parse_decl_block` from
one-physical-line-is-one-statement matching into a statement/
continuation-accumulation model — real regression risk to a tool
500+ files already depend on, to fix a completeness bug rather than a
corrupting one. **Standing mitigation instead**: promote the
delegate's "import `ansify`'s own `HDR`/`parse_decl_block` and re-scan
directly" technique to the standing checklist (new item 9, below) for
any future large batch — C1 (`contrib/zip/lib`, already flagged as the
tree's highest-defect-density directory) should use it without
question.

**Standing item-8 check had two real blind spots**, found via 12 real
stranded-declaration compile failures the pre-diagnosis's grep
predicted zero of: the grep only matched `static`-prefixed
declarations (10 of the 12 real conflicts were `extern`-prefixed), and
it read parameter types literally without resolving `typedef`s (2 more
used `Boolean`, `typedef short Boolean` — a narrow, promotion-risky
type invisible to a literal-keyword read). Item 8's wording is now
broadened below.

**One genuine ~30-year-old bug found**: `unscrib.c`'s
`UnformatMessage` called `FreeMessageContents(Msg)` with 1 of its real
2 arguments (surfaced only once `FreeMessageContents` got a real
2-arg prototype via the item-8 fix above — invisible under K&R's
unchecked argument count). Fixed as `FreeMessageContents(Msg, FALSE)`
— `FreeSnapshot` controls only whether `Msg->Snapshot` gets freed, and
`UnformatMessage` reuses `Msg` afterward, so the conservative
(don't-free) choice was applied and flagged for confirmation.
**Still open, not yet runtime-verified** — corrected 2026-08-01 after
an earlier pass through this doc wrongly conflated wdc's actual
runtime checks (`cuin dirinfo`, which exercises the unrelated
`MS_GetDirInfo` fix, and general `messages` non-regression) with
confirmation of this specific fix. Neither check reaches
`UnformatMessage`/`FreeMessageContents`. Needs either a real exercise
of a reformat/edit-in-place `cuin` command against a live message, or
a direct ruling from wdc on the original 1988 semantics (does a
message's cached snapshot summary survive an in-place reformat?).

**Task 2 (`ams/msclients/cui` COMPILERFLAGS closure) completed in the
same session**, no split needed: all 140 undeclared calls categorized
(36 `ams/libs/ms`, 46 `ams/libs/cui`, 55 same-directory helpers — this
directory was already `ansify`-converted by A1, not mentioned in this
batch's prompt, discovered directly — 7 `overhead/util/lib`, 3 other
AMS libs, 1 libc) and declared. Hit one real complication:
`moreprintf`/`errprintf2` are 1988-era pseudo-variadic functions (many
fixed named params, called with far fewer args, relying on K&R's
unchecked-trailing-args tolerance) — a typed or `...`-variadic
declaration broke 177 call sites and conflicted with `errprintf2`'s
real fixed-13-arg ANSI definition in the same file. **Resolved with
old-style empty-parens "unspecified arguments" declarations**
(`extern int moreprintf();` / `extern int errprintf2();`) — confirmed
by the orchestrator to be C-standard-correct, not a shortcut: this is
the only declaration form simultaneously compatible with a fixed-arity
ANSI definition in the same translation unit *and* call sites
supplying fewer arguments than the full parameter list. Both
directories gated clean twice each; `cuin` confirmed relinked and
reinstalled against the converted `libmssrv.a` both times.
Independently re-verified by the orchestrator: both gates re-run
clean directly (not just trusted from the delegate's own run), ~6
specific fixes spot-checked against the diff (all matched exactly),
`fossil status` confirmed exact file scope (115 files: 110 in
`ams/libs/ms`, 5 in `ams/msclients/cui`). Full detail in
`claude-history/m3-ams1-REPORT.md`.

### AMS2 (`atkams/messages/lib`, `ams/libs/shr`, `ams/libs/cui`,
`ams/libs/nosnap`, 34 files, 2026-08-01) — closes Wave 6

Routine Gate-0 batch, zero UNCLASSIFIED items at Gate 0 (every finding
matched existing taxonomy, several confirmed with a live `-pe` compile
check rather than a read-by-hand alone). `atkams/messages/lib` (23
files, 17 `.ch`, real `-pe`/`.eh` rollout) is one of the two permanent
tree-wide-gate directories (with `contrib/zip/lib`), kept regardless
of wave-end status; this batch also happens to close Wave 6.

**Two `.ch`-vs-real-usage bugs, both confirmed via live `-pe` compile
checks before the real run**:

- `fldtreev.ch`'s `InitializeClass`/`FinalizeObject` restated the
  auto-supplied `classID` argument by name — same mechanism as B3's
  `unknownv.ch`/`suiteev.ch` finding (porting-assessment.md §17,
  "second real exception"), doubling `classID` in the exported
  prototype against the real 1-param/2-param definitions. Fixed by
  removing the redundant restatement (empty parens for
  `InitializeClass`, `self`-only for `FinalizeObject`), matching every
  other `.ch` in the directory's own convention. `InitializeObject`'s
  own restatement was left untouched — irrelevant, since classpp
  hardcodes that one's exported signature regardless of `.ch` text.
- 3 more self-type copy/paste typos (`messages.ch`, `mailobjv.ch`,
  `text822v.ch`, all restating `self` as the wrong sibling/unrelated
  class) — matches B2 finding 2 exactly. 2 of 3 were live and
  compile-blocking (`mailobjv.ch`, `text822v.ch` — the `.eh` and the
  real definition are visible together in the same file); the third
  (`messages.ch`) was live but non-blocking, only a warning (`
  messaux.c`, which defines the real `FinalizeObject`, never itself
  `#include`s `messages.eh`). All 3 corrected to their real class
  name.

**Two genuinely new fallout shapes, both resolved by extending
existing taxonomy rather than needing a fresh ruling** (found only
once the real, non-dry-run `ansify --dir` and `-pe` gate actually ran
— not visible at Gate 0):

- A **plain classproc** with truly-empty K&R parens and zero
  parameters (`ams__CountAMSViews`) — generalizes the O4/B3
  "invisible to `ansify`'s candidate detector" finding
  (`BARE_PARAMS` requires at least one bare identifier) beyond the
  three special lifecycle names to an ordinary classproc for the
  first time.
- A same-run, tool-self-inflicted stranded-declaration conflict:
  `fix-missing-static-decl` inserted a **fresh** empty-parens stub for
  `hexchar` (no pre-existing declaration at all) that then conflicted
  with `hexchar`'s own narrow `char c` real definition — the same
  "same-run, tool-self-inflicted" variant AMS1 already documented for
  `atk/help/src/helpa.c`.

**Item 8 blind spots found for real** (both folded into the standing
checklist's item 8 wording above): multi-name comma-list stranded
declarations (`stubs.c`'s 15-name list, `cuilib.c`'s two lists of 17
and 19 names — 6 real narrow-param conflicts hiding inside them,
invisible to a single-name grep) and confirmation that the hazard is
same-translation-unit only (2 cross-file instances found non-blocking:
`ConsiderResettingDescription`, `WriteOneFile`).

Two "generic-param-plus-cast" fixes, same species as B2 finding 5/I1's
`srctextv.ch` finding (override/classproc typed to the general base
interface for correct caller compatibility, body needs the concrete
subclass): `text822.c`'s `ReadIntoText`/`ResetGlobalStyle`,
`text822v.c`'s `DeleteApplicationLayer` — confirmed against real
caller argument types and sibling-class convention before fixing, not
a guess.

`amss.c` and `ams/libs/cui/pcmchs.c` both confirmed dead code
requiring their own out-of-scope "subtree activation"
(`SNAP_ENV`/`IBMPC` never defined in this build) — left K&R exactly as
predicted; `pcmchs.c`'s stdlib.h/stale-`malloc()`/parser-gap fixes
were applied anyway for consistency (harmless, file is unlinked from
`libcui.a`'s real `OBJS`) and confirmed to survive `ansify`'s
auto-revert-on-failure independently.

All 4 subtrees gated clean twice each; tree-wide gate clean twice
(only the 2 known pre-existing `contrib/zip/utility/ltapp.c` errors,
unrelated, queued for Wave 7 C2). Independently re-verified by the
orchestrator: all 4 subtree gates and the tree-wide gate re-run
clean directly (not just trusted from the delegate's own run), 7
specific fixes spot-checked against the diff (`fldtreev.ch`,
`messages.ch`, `ams.c`'s rock-cast/`AddToClassList`, `mailobj.c`'s
`char64`/`hexchar`, `stubs.c`'s comma-list split, `text822.c`'s
generic-param-plus-cast fix — all matched exactly), `fossil status`
confirmed exact file scope (37 files: 27 in `atkams/messages/lib`
[22 `.c` + 4 `.ch` + 1 `Imakefile`], 6 in `ams/libs/shr`, 3 in
`ams/libs/cui`, 1 in `ams/libs/nosnap`).
Session was interrupted once mid-Gate-1 by an unrelated
orchestrator-side usage-limit event; resumed cleanly by re-deriving
state from `fossil status`/`fossil diff` before continuing, no rework.
Full detail in `claude-history/m3-ams2-REPORT.md`.

**Wave 6 complete.** M3 remaining: Wave 7 (C1, C2).

## Resource note (2026-07-25, wdc)

Evening-of-2026-07-22-to-now work (M2's back half plus this planning)
has consumed 78% of the weekly resource budget. This planning doc
lands now — cheap, no delegated sessions — but actual M3 execution
(the census re-run, the `atk/eq` pilot recommit, or any wave) should
wait for an explicit go-ahead rather than starting immediately behind
M2's close-out.
