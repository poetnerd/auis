# M3 Wave 1 batch O2: `overhead/image/jpeg` + `overhead/image/tiff`

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — the
"Command style" section matters for an unattended session),
`m3-rollout-runbook.md` (including its "Findings from real sessions"
section, added after batch O1), `m3-batches.md`, and
`porting-assessment.md` §14 and §17 (the `ansify` tool design and its
known DRIFT false-positive) before starting. Also skim
`m3-o1-utillib-REPORT.md` (the first real M3 session,
same directory family shape as this one — no `.ch` files, pure
helper-conversion, a couple of hand-fixable parser edge cases) for
what "normal" looks like.

This is the second real M3 batch. Two directories, one session, one
gate — same batching this codebase's M1/M2 rollouts used for small/
related directories.

## What's different about this batch vs O1

Both directories are **vendored third-party codec libraries** (libjpeg
and libtiff derivatives), not AUIS-native 1990s code. Two consequences:

1. Vendored 1990s portable-C libraries often already carry their own
   dual `#ifdef __STDC__` / K&R declaration blocks (a typed prototype
   under one branch, an empty-parens K&R declaration under the other)
   — AUIS-native code rarely does this. Where one exists, it's the
   ground truth for a hand-conversion, same idea as the M1 pilot's
   observation that a `.ch` file is ground truth, just a different
   place to find it.
2. Expect a much higher fraction of "no K&R definitions converted" —
   some of this code may already be ANSI-style from upstream. That's
   normal here, not a sign anything is wrong.

## Directory-specific findings (already done, don't redo)

Confirmed 2026-07-25: neither directory has any `.ch` files (`find
src/overhead/image/jpeg -maxdepth 1 -name '*.ch'` and the `tiff`
equivalent both return nothing) — same as O1, the `-pe`/`.eh`-regen
step does not apply to either. Verify this yourself but don't spend
time on it if it holds.

Tree-wide dry-run census (`ansify --dry-run --dir src`, 2026-07-25)
baselines:

| | `overhead/image/jpeg` | `overhead/image/tiff` |
|---|---|---|
| Files | 46 | 32 |
| Methods / classprocs | 0 / 0 | 0 / 0 |
| Helpers | 30 | 206 |
| DRIFT | 0 | 0 |
| Skipped/bailout | 0 | **5** |

`jpeg` is expected to be close to a non-event. `tiff`'s 5 skipped
items are parser bailouts, already root-caused below — not DRIFT, not
a real interface disagreement, just two narrow syntax shapes the
strict file-local-helper parser doesn't recognize. **This has already
had its "human read" (`m3-rollout-runbook.md`'s stated requirement for
a parser bailout) — apply the prescribed fixes below rather than
re-deriving them, but do verify each one compiles as described.**

### The 5 pre-diagnosed `tif_fax3.c`/`mkg3states.c` bailouts

**`tif_fax3.c`** already carries the classic vendored-library dual-
declaration idiom near the top of the file (~line 65-95): a typed
`#ifdef` branch with full prototypes, a K&R empty-parens `#else`
branch. Three static helpers' *definitions* further down use a
parenthesized-`const` style (`tableentry const *te`, not `const
tableentry *te`) that the parser's declaration regex doesn't
recognize (it only accepts `const`/`unsigned`/etc. as a prefix, not a
postfix qualifier) — bails on the whole declaration block for each.
The typed prototypes already 700-ish lines earlier are ground truth;
mirror them into each definition header, adding parameter names from
the existing K&R block:

- **Line ~769** (`putcode`): existing prototype (line ~77)
  `static void putcode(TIFF *, tableentry const *);` → convert the
  definition to `static void putcode(TIFF *tif, tableentry const
  *te)`.
- **Line ~783** (`putspan`): existing prototype (line ~78)
  `static void putspan(TIFF *, int, tableentry const *);` → `static
  void putspan(TIFF *tif, int span, tableentry const *tab)`.
- **Line ~1101** (`findspan`): existing prototype (line ~73) `static
  int findspan(u_char **, int, int, u_char const *);` → `static int
  findspan(u_char **bpp, int bs, int be, u_char const *tab)` (the
  `register` storage-class keywords on the K&R parameter lines are
  legal but meaningless on parameters in a modern definition — drop
  them, matching what `ansify`'s own converter already does elsewhere
  in this codebase; check a converted-and-working example from O1 or
  elsewhere in this same file if unsure).

**`mkg3states.c`** — the `FILE* fd` no-space-before-star style trips
the same parser (it requires whitespace between the base type token
and the rest of the declarator list):

- **Line ~423** (`extern_table`): no existing forward declaration
  anywhere in the file — derive purely from its own K&R block (`FILE*
  fd; char* name;`) and body (one `fprintf` call, no `return`, same
  shape as its sibling `write_tables` which is declared `void`) →
  `void extern_table(FILE *fd, char *name)`.
- **Line ~431** (`write_tables`): existing prototype (line ~252)
  `void write_tables(FILE*);` → `void write_tables(FILE *fd)`.

**Sequencing**: hand-apply these 5 fixes to the pristine K&R source
*before* running `ansify --dir` on `tiff`, the same order O1 used for
`encode.c`'s pre-existing forward declarations — once the definition
header itself is already full ANSI, `ansify` will correctly report
"no K&R definitions converted" for these 5 and convert the rest of
each file normally. Verify each with a direct `make <base>.o` after
the hand-fix, before running `ansify --dir` on the whole tree, so a
mistake here doesn't get buried under the directory's other 200
conversions.

## Task

1. Confirm no `.ch` files in either directory (see above).
2. Apply the 5 hand-fixes in `tif_fax3.c`/`mkg3states.c` (`overhead/
   image/tiff`) exactly as specified above. Verify each file still
   compiles (`make tif_fax3.o`, `make mkg3states.o`) before proceeding
   — these two files carry real fallout risk if the hand-fix is
   slightly wrong; everything else in this batch is expected to be
   mechanical.
3. `ansify --dir src/overhead/image/jpeg` — for real.
4. `ansify --dir src/overhead/image/tiff` — for real. Confirm the 5
   previously-skipped functions no longer appear in either the
   skipped or DRIFT sections of the report, and that the total helper
   count for `tiff` is close to the 206 baseline (the 5 hand-fixed
   ones won't be counted by `ansify` itself since they're already
   ANSI going in — expect roughly 201 converted by the tool + 5
   hand-converted = ~206 total, not an exact match requirement, just a
   sanity check).
5. If either real run surfaces DRIFT or skipped/bailout findings
   beyond the 5 already diagnosed, **stop and report before doing
   anything else** — don't try to resolve a genuinely new finding
   yourself.
6. Fix any ordinary `COMPILE FAILED — restoring original` fallout the
   real runs report, same spirit as O1 (missing includes, the
   macro-parameter-capture issue from Pilot A if it recurs, etc.) —
   this part is yours to resolve, not escalate, unless it looks like a
   genuine semantic disagreement.
7. Subtree-local gate for **both** directories: `make -C
   /Users/wdc/src/AUIS/andrew-6.4/src/overhead/image/jpeg clean`, then
   `depend`, then `-k install`; same three steps for `.../tiff` —
   separate calls, absolute paths, not chained. Rebuild each clean
   twice to confirm determinism.
8. Do not run the tree-wide gate — neither directory is
   `atkams/messages/lib` or `contrib/zip/lib`.

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary
interactively.** Identify real consumers via `nm -g`/Imakefile `LIBS`
lines (these codec libraries likely feed the `raster` inset family
and/or `convertraster` — confirm rather than assume) and give wdc the
exact runtime-check command(s) in your report. Stop after both gates
are green (twice each, for determinism) and say you have stopped.

## Report

Write `m3-o2-imagecodecs-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance counts vs. the dry-run baselines above for both
  directories, and confirmation the 5 pre-diagnosed bailouts are
  resolved.
- Each hand-fix and each compile-gate fallout fix: file:line, what was
  wrong, the fix, compile confirmation.
- Exact runtime-check command(s) for wdc, with the consumer(s)
  identified.
- `fossil status` output confirming exactly which files changed
  (expect files in both `overhead/image/jpeg` and `overhead/image/
  tiff`, nowhere else), no commit made.
- Any new `ansify` fallout pattern not already described in
  `porting-assessment.md` §14/§17 or `m3-rollout-runbook.md`'s
  findings section — vendored third-party code may turn up shapes
  AUIS-native code doesn't (unusual macro use, platform-conditional
  code, etc.).
