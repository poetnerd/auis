# M2 rollout point 4f: `overhead/mail/lib`

Read `sonnet-playbook.md`, `rollout-procedure.md` in full — including
its "Command style" section (the `cd`-persistence check: verify early
with a throwaway `cd`+`pwd` pair; if it doesn't persist across Bash
calls in your session, default to `make -C <absolute-path> <target>`
for every build step instead of `cd`-then-bare-command) and the
"Logging" section's malloc-family blind-spot note, **including the
2026-07-24 sweep-pattern correction from the `atk/table` session**:
use `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`
(word-boundary-anchored, space-before-paren tolerant), not a plain
substring `grep "malloc("`/`"realloc("` — a directory with its own
`*alloc`-suffixed wrapper name (e.g. `atk/table`'s `myrealloc()`) will
false-positive/false-negative under the naive form, and you won't know
until you've checked whether this directory has one — read
`m2-rollout-runbook.md`, and all nine prior rollout reports
(`claude-history/m2-pilot-eq-REPORT.md`, `claude-history/
m2-batch2-REPORT.md`, `claude-history/m2-batch3a-REPORT.md`,
`claude-history/m2-batch3b-REPORT.md`, `claude-history/
m2-utillib-REPORT.md`, `claude-history/m2-metamail-REPORT.md`,
`claude-history/m2-text-REPORT.md`, `claude-history/
m2-rofftext-REPORT.md`, `claude-history/m2-table-REPORT.md`) before
starting. The taxonomy has three validated categories with sub-cases
plus several later sub-shapes — see the runbook's "Fallout taxonomy"
section in full, including `atk/table`'s new `AUXMODULE`-guarded
double-underscore-method sub-shape (a real, complete, correctly-typed
declaration that's structurally unreachable from the one file that
needs it — check for this if a class-method macro here ever calls a
double-underscore name directly). "Possible genuine bug/typo" is still
empty across 1603 instances/24 directories. Don't assume it stays
empty — still `grep` to confirm before writing any declaration.

**This directory has a confirmed generated-source gap**, different in
shape from `atk/rofftext`'s: `FlexOrLexFileRule(parsel)` +
`Parser(parsey,-d)` + `NormalLexRule()` + `LexWithReplacement(parsel,
mail_parseyy)` (read the Imakefile yourself to confirm the exact
mechanics — don't just trust this citation). `parsel.o: parsey.h` is
an explicit make dependency in the Imakefile itself, meaning the
lexer's generated source needs the parser's generated header before it
can compile — `make depend` before `install` is required here even
more directly than in past generated-source cases; confirm the
generated file(s) actually appear after `depend` before trusting the
rest of the build, and don't be surprised if `parsel`-related fallout
looks different from ordinary same-directory fallout until the
generated header is confirmed present.

**Structural note, different gate-scope shape from the last two
sessions**: `atk/table` and `atk/rofftext` both built only
`DynamicMultiObject`s (no `LibraryTarget`, cross-directory fallout
structurally impossible). This directory is the opposite shape: it
builds a real static archive, `LibraryTarget(libmail.a, $(OBJS))`,
installed to `$(DESTDIR)/lib`, structurally identical to batch B's
`ams/libs/cui` (largest single-session volume so far at 528 instances)
and batch A's `atk/basics/common`. A tree-wide `grep -rl "libmail.a"`
across the source tree's Imakefiles found **~24 consumer directories**
across `ams/`, `atkams/messages/lib`, `contrib/`, `overhead/mail/cmd`,
`overhead/wpi`, `overhead/wputil`, and `rdemo/messages` — this is
almost certainly the widest fan-out of any bucket-4 directory examined
so far. **The gate-scope ruling still applies (subtree-local gate
only, not tree-wide)** — the ruling was made with `ams/libs/cui`'s
comparably wide fan-out already in evidence, and M2's flag only
changes diagnostic severity inside the flagged directory itself, not
any installed header or `.a` member's ABI (see the runbook's "Gate
scope" section for the full reasoning) — but given the fan-out size,
put extra care into the `nm -g` verification step: check at least two
or three real consumer binaries/`.do`s (e.g. `messages`'s `amsn.do`,
`cui`, `mailto` — confirm the actual install names via each
consumer's own Imakefile rather than guessing) for zero
`overhead/mail/lib`-distinctive symbol leakage, not just one.

This is a single-directory session (bucket 4's sixth), not a batch. 32
`.c` files total, though the Imakefile's `OBJS` list (used to build
`libmail.a` itself) only covers 30 of them — `imapspike.c`,
`imaptest.c`, and `smtptest.c` are `.test`-target-only sources (see
`TestingOnlyTestingRule`), not part of the installed library. Confirm
this distinction yourself by reading the Imakefile rather than
assuming all 32 files are equally in scope the same way. Stale
estimate for this directory is 112 — given `atk/text` (156 real, 68%
over its 50-estimate) and `atk/rofftext` (104 real, 121% over) and
`atk/table` (186 real, 65% over) all ran well past their stale
numbers, expect the same pattern here; treat 112 as a floor.

## Task

1. Confirm the generated-source mechanics (`Parser`/`Lex`) by reading
   the Imakefile yourself, and confirm which of the 32 `.c` files are
   actually part of `libmail.a`'s `OBJS` vs. test-only.
2. Flag the `Imakefile` with the runbook's exact `COMPILERFLAGS`
   override (full four-flag restatement).
3. Regenerate + verify the Makefile (`grep -n COMPILERFLAGS Makefile`).
4. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, each its own call, no `cd`
   chained onto any of them (see the command-style note above for
   which mechanism to use).
5. Fix fallout per the runbook's taxonomy.
6. **Unconditionally sweep every `.c` file in the directory** (all 32,
   not just the `OBJS` 30) for bare `malloc(`/`free(`/`realloc(`/
   `calloc(` calls, using the word-boundary-anchored pattern above,
   regardless of whether the census-visible error count looks complete
   or matches the stale estimate.
7. Rebuild clean, twice, to confirm determinism.

## Gate

**Subtree-local gate only** — per the gate-scope ruling recorded
2026-07-24 in `m2-rollout-runbook.md`. Do not run the full tree-wide
gate. Given this directory's unusually wide consumer fan-out (~24
directories, see above), be more thorough than usual in the `nm -g`
verification: check multiple real consumer binaries/`.do`s, one target
per `nm -g` call, never piped/chained/redirected in the same call
(prior sessions found piped/redirected `nm` calls behave inconsistently
across session types, sometimes denied outright — a bare `nm -g
build/bin/<target>` by itself, or `cd <tree-root> && nm -g
build/bin/<target>` as a single call, has reliably gone through). If
anything about the actual linkage or the fan-out's size makes you doubt
subtree-local is sufficient here, say so explicitly in the report
rather than silently running the tree-wide gate anyway or silently
dropping the concern — this is exactly the kind of judgment call worth
surfacing even though the ruling already covers it.

**Do NOT commit. Do NOT run the AUIS GUI or any binary from this
directory interactively.** Stop here and report.

## Report

Write `revival/doc/claude-history/m2-mail-lib-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance count found (vs. the stale table's 112), split into
  census-visible vs. malloc-blind-spot-only, each fix with file:line +
  taxonomy category/sub-case.
- Confirmation of the generated-source handling (what `Parser(parsey,
  -d)`/`FlexOrLexFileRule(parsel)` produce, and that `depend` was run
  before `install`, and that `parsel.o: parsey.h`'s explicit dependency
  held).
- Which of the 32 `.c` files are `OBJS`-library members vs. test-only,
  and whether the test-only files needed any fixes too (in scope or
  not — say which and why).
- Any new taxonomy category or sub-case, or anything that contradicts
  current taxonomy/predictions.
- **The `nm -g` fan-out verification results** — which consumer
  binaries/`.do`s were checked, and confirmation of zero
  `overhead/mail/lib`-distinctive symbol leakage in each.
- **Exact runtime-check commands for wdc.** `overhead/mail/lib` backs
  mail-address parsing/SMTP/auth/dropoff logic consumed by `messages`,
  `cui`, `mailto`, and others — identify the most direct, lowest-risk
  real exercise path (not necessarily full mail send/receive; note
  what would actually need live SMTP/AFS/AMS infrastructure this
  revival doesn't have, vs. what's testable standalone, e.g. a `.test`
  target).
- `fossil status` output confirming exactly which files changed, no
  commit made.
- A brief note on how the command-style discipline went this session,
  including which `cd`-vs-`make -C` mechanism applied in your session
  type and whether it held up.
