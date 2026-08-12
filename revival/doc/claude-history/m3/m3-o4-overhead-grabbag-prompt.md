# M3 Wave 1 batch O4: overhead grab-bag (19 directories, 58 files) — completes Wave 1

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — the
"Command style" section matters for an unattended session),
`m3-rollout-runbook.md` (including its full "Findings from real
sessions" section — O1, O2, O3 are all there now), `m3-batches.md`,
and `porting-assessment.md` §14 and §17 before starting. Also skim
`m3-o1-utillib-REPORT.md`, `m3-o2-imagecodecs-REPORT.md`,
and `m3-o3-mail-eli-bison-REPORT.md` for what these sessions look like
in practice.

This is the fourth and final Wave 1 batch: 19 unrelated small/leaf
directories in one session, the largest directory-count batch so far
but also the cleanest dry-run baseline of any M3 batch to date (see
below) — same "unrelated families bundled in one session" shape as O3
and M2's own rollout point 2.

The 19 directories: `overhead/cmenu`, `overhead/mail/metamail/metamail`,
`overhead/mail/metamail/richmail`, `overhead/malloc`, `overhead/index`,
`overhead/class/lib`, `overhead/class/testing`, `overhead/class/cmd`,
`overhead/class/machdep/darwin`, `overhead/rxp`, `overhead/fonts/cmd`,
`overhead/util/cmd`, `overhead/sys`, `overhead/mkparser`,
`overhead/util/hdrs`, `overhead/errors`, `overhead/addalias`,
`ossupport`, `inst`.

**Important naming note:** the "class-support family" directories in
this batch (`overhead/class/lib`, `overhead/class/testing`,
`overhead/class/cmd`, `overhead/class/machdep/darwin`) are support code
*around* the class system — NOT `overhead/class/pp` (classpp itself,
`class.c`), which is permanently excluded from all M3 batches (the M1
codegen tool; converting it with the tool that depends on it is
circular — see `m3-batches.md`'s exclusion note and
`porting-assessment.md` §14). Do not touch `overhead/class/pp` for any
reason.

## Directory-specific findings (already done, don't redo)

A tree-wide-scoped dry-run (`ansify --dry-run --dir src/<dir>`,
2026-07-26) was run for all 19 directories. Baseline:

| Directory | Files | Helpers | Methods | DRIFT | Skipped |
|---|---|---|---|---|---|
| `overhead/cmenu` | 8 | ~8+ | 0 | 0 | 0 |
| `overhead/mail/metamail/metamail` | 7 | ~13+ | 0 | 0 | 0 |
| `overhead/mail/metamail/richmail` | 6 | ~25+ | 0 | 0 | 0 |
| `overhead/malloc` | 4 | 0 (3 of 4 already ANSI) | 0 | 0 | 0 |
| `overhead/index` | 4 | ~24+ | 0 | 0 | 0 |
| `overhead/class/lib` | 4 | ~26+ (1 of 4 already ANSI) | 0 | 0 | 0 |
| `overhead/class/testing` | 3 | ~2 | **2** | 0 | 0 |
| `overhead/class/cmd` | 3 | ~10 | 0 | 0 | 0 |
| `overhead/class/machdep/darwin` | 2 | ~1 (1 of 2 already ANSI) | 0 | 0 | 0 |
| `overhead/rxp` | 3 | ~20 | 0 | 0 | 0 |
| `overhead/fonts/cmd` | 3 | ~22 | 0 | 0 | 0 |
| `overhead/util/cmd` | 2 | ~4 | 0 | 0 | 0 |
| `overhead/sys` | 2 | ~1 (1 of 2 already ANSI) | 0 | 0 | 0 |
| `overhead/mkparser` | 2 | ~14 (1 of 2 already ANSI) | 0 | 0 | 0 |
| `overhead/util/hdrs` | 1 | ~1 | 0 | 0 | 0 |
| `overhead/errors` | 1 | ~1 | 0 | 0 | 0 |
| `overhead/addalias` | 1 | ~6 | 0 | 0 | 0 |
| `ossupport` | 1 (own-level only, see below) | ~5 | 0 | 0 | 0 |
| `inst` | 1 | ~4 | 0 | 0 | 0 |

**Zero DRIFT and zero skipped/bailout findings across all 19
directories** — the cleanest batch baseline of the four Wave 1 batches.
No pre-diagnosed hand-fixes are needed going in (unlike O3's `getopt`
findings). This does not mean the *real* run will be equally clean —
`ansify`'s compile gate only exercises on a real run, same caveat as
every prior batch — but there is no known judgment call to carry in.

### `overhead/class/testing` is a special case: this batch's first real `-pe`/`.eh` rollout

Every other directory in every M3 batch so far (O1, O2, O3, and the
other 18 directories here) has had zero `.ch` files, so
`m3-rollout-runbook.md`'s "Mechanics" steps 1-2 (add `-pe` to
`CLASSFLAGS`, force `.eh` regen) have never actually been exercised for
real yet in this project. `overhead/class/testing` has two: `testobj.ch`
and `testobj2.ch`, each backing one class method in `testobj.c`/
`testobj2.c` respectively (1 method each, confirmed by the dry-run
above).

The runbook's `atk/eq` "pilot" (Wave 4) is the intended controlled
first trial of this mechanic, but it is a *dry run only* (2026-07-08,
never committed) — and `m3-batches.md`'s own dependency-order argument
means `overhead` (Wave 1) legitimately precedes `atk/eq` (Wave 4)
regardless of which one is labeled "pilot." Given that: (a) the exact
procedure this needs is already validated in detail on `atk/eq` itself
(`porting-assessment.md`, the paragraph beginning "Validated
2026-07-08 on `atk/eq/eq.c`"), and (b) the blast radius here is minimal
— a 3-file testing/demo directory, not a real inset or app — treat this
as the first *real* (committed) `-pe` application in the project, using
the already-validated procedure below. This is a judgment call being
made here rather than deferring to wdc; flag it clearly in your report
so wdc can confirm it after the fact, and note in `m3-batches.md`'s Wave
4 entry afterward that the eq "pilot" framing is now stale (this batch
already proved the mechanic live) if wdc agrees.

Procedure (from `m3-rollout-runbook.md`'s "Mechanics" section,
concretized for this directory):

1. `overhead/class/testing/Imakefile` currently has no `CLASSFLAGS`
   line at all (confirmed 2026-07-26 — it inherits the tree default,
   `CLASSFLAGS = $(CLASSINCLUDES)`, from `config/imake.tmpl`). Add a
   line: `CLASSFLAGS = $(CLASSINCLUDES) -pe`. Re-run `imake`/`make
   Makefile` (however this tree's convention regenerates a Makefile
   from an edited Imakefile — check `rollout-procedure.md` if unsure)
   and confirm via `grep -n CLASSFLAGS Makefile` that the flag actually
   landed in the generated Makefile's `.ch.eh:` rule line, the same
   verification M1/M2 both used before trusting a flag change.
2. Force `.eh` regeneration: remove `testobj.eh` and `testobj2.eh` (or
   touch the `.ch` files) before the next build, since the suffix rule
   only fires on `.ch`-newer-than-`.eh`.
3. Expect the same specific failure mode `atk/eq` hit if a narrow-type
   (`char`/`short`-by-value) parameter exists in either class's method
   — the already-diagnosed fix is simply "the `-pe`-regenerated `.eh`
   now carries a typed prototype instead of the old typeless one,"
   nothing else should be needed. If something *else* fails here, don't
   assume it's the same eq shape — treat it as a new finding worth
   describing in the report.
4. Then proceed with `ansify --dir` on this directory same as the other
   18 (its class-method conversion path uses the signature DB lookup,
   same mechanism O1-O3 already exercised for helpers, just now also
   exercising the method-conversion half of the pipeline for the first
   time for real).

All other 18 directories: no `.ch` files, so steps 1-3 above don't
apply to them — same as every directory in O1-O3.

### `ossupport`: only the own-level file is in scope

`ossupport` recurses into several platform-specific subdirectories
(`hpx`, `aix3`, `scox`, `vaxx`, `i386_bsd`, `telmat`, `solaris`) for
non-Darwin Unix variants. **Only `ossupport/oscommon.c` (the one
own-level file) is in scope for this batch** — matching `m3-batches.md`'s
own-level-file-count convention (it lists `ossupport (1)`, not the
platform subdirectories). The dry-run does show a few "skipped scandir:
unparseable K&R declarations" findings in `ossupport/telmat/ossup.c`,
`ossupport/solaris/ucb.c`, and `ossupport/hpx/scandir.c` — these are
all inside the out-of-scope platform subdirectories, not `oscommon.c`
itself, and should not be touched or reported as batch findings. Only
escalate if `oscommon.c` itself (the in-scope file) produces a real
finding.

### A few files in this batch are already fully ANSI

The dry-run reports "no K&R definitions converted" (as opposed to
"would convert: N helpers") for: `overhead/malloc/plumber.c`,
`pmalloc.c`, `pplumber.c`; `overhead/class/lib/classproc.c`;
`overhead/class/machdep/darwin/classproc.c`; `overhead/sys/conftest.c`;
`overhead/mkparser/testparse.c`. This is expected, not a bug — `ansify`
correctly finds nothing to do on files that are already ANSI C. No
action needed on these beyond letting `ansify --dir` run over the whole
directory normally (it will just report zero conversions for these
specific files).

### `overhead/rxp` is where `reg_comp`/`reg_exec` are actually defined

For context on O3's `prmtives.c` fix (the `regcomp`/`regexec` →
`reg_comp`/`reg_exec` typo rename, `revival.md` "Old bugs never found
till now"): the real function definitions live here
(`overhead/rxp/regexp.c:228` `reg_comp`, `:736` `reg_exec`), declared
K&R-empty-parens in `regexp.h` (`extern regexp *reg_comp(); extern int
reg_exec();`). Converting `regexp.c`'s definitions to ANSI here does
not by itself change `regexp.h`'s K&R-style extern declarations — that
would be `fix-missing-static-decl`/prototype-generation territory only
if `ansify` chooses to touch the header, which it may or may not do
depending on whether `regexp.h` is treated as part of this directory's
conversion scope (it's a same-directory header, so plausibly yes). Not
a pre-diagnosed finding, just relevant background — note in your report
what `ansify` actually does with `regexp.h`, if anything.

## Task

1. Confirm `.ch` file presence per directory (`find <dir> -maxdepth 1
   -name '*.ch'`) — should match: only `overhead/class/testing` has any
   (2), all other 18 have zero. If this disagrees with what's stated
   above, stop and report before proceeding on the affected directory.
2. For `overhead/class/testing`: apply the `-pe`/`.eh` procedure above
   first, verify it compiles, *then* run `ansify --dir` on it.
3. `ansify --dir` each of the other 18 directories for real, one at a
   time.
4. If any directory's real run surfaces DRIFT or a skipped/bailout
   finding not already covered above, **stop and report before doing
   anything else** — don't try to resolve a genuinely new finding
   yourself.
5. Fix any ordinary `COMPILE FAILED — restoring original` fallout —
   yours to resolve, not escalate, unless it looks like a genuine
   semantic disagreement. Stay alert for the two still-open `ansify`
   parser bugs from O2 (`(void)`-as-bare-parameter misparse,
   `DECLARE<N>`-macro misparse) and the `fix-missing-static-decl`
   non-idempotency pattern from O1/O3 (typed prototype not recognized
   as "already declared") in case any recur here — all three are
   documented workarounds in `m3-rollout-runbook.md`'s findings section,
   not things to re-derive from scratch.
6. Subtree-local gate for **each** of the 19 directories separately:
   `make -C <absolute-path> clean`, then `depend`, then `-k install` —
   separate calls, absolute paths, not chained. Rebuild each clean
   twice to confirm determinism. (No directory in this batch generates
   source via `FlexOrLexFileRule`/`Parser()` per an Imakefile check
   already done, but running `depend` uniformly is cheap insurance
   anyway, same as O3's own note.)
7. Do not run the tree-wide gate — none of these 19 is
   `atkams/messages/lib` or `contrib/zip/lib`. This batch does complete
   Wave 1 overall, though — say so in your report; wdc may choose to
   run a milestone tree-wide gate separately (that's a top-level
   decision, not yours to trigger).

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively.**
Identify real consumers via `nm -g`/Imakefile `LIBS` lines for each
directory and give wdc the exact runtime-check command(s) in your
report. Stop after all 19 gates are green (twice each, for determinism)
and say you have stopped.

## Report

Write `m3-o4-overhead-grabbag-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance counts vs. the dry-run baselines above for all 19
  directories.
- Full detail on the `overhead/class/testing` `-pe`/`.eh` rollout:
  exact Imakefile diff, confirmation the flag landed in the generated
  Makefile, the `.eh` regen, and whether the eq-shape narrow-type
  failure recurred or not (and if something else failed instead,
  describe it in full — don't guess at a fix that isn't the pre-
  diagnosed one).
- What (if anything) happened to `overhead/rxp/regexp.h` during that
  directory's conversion.
- Each compile-gate fallout fix: file:line, what was wrong, the fix,
  compile confirmation.
- Exact runtime-check command(s) for wdc, with consumers identified,
  per directory.
- `fossil status` output confirming exactly which files changed
  (expect files only inside these 19 directories — and confirm no
  `ossupport` platform-subdirectory file was touched), no commit made.
- Explicit confirmation this completes Wave 1 (O1-O4 all done).
- Any new `ansify` fallout pattern not already described in
  `porting-assessment.md` §14/§17 or `m3-rollout-runbook.md`'s findings
  section (O1, O2, O3).
