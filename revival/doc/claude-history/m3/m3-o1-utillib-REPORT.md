# M3 Wave 1 batch O1: `overhead/util/lib`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, twice for determinism, per the
prompt's ruling that this directory doesn't need the tree-wide gate.
No commits made. Working tree: 58 files edited, all `.c`, all inside
`src/overhead/util/lib/` (no Imakefile edit needed — unlike M2, this
directory has no `.ch` files, so the M1 `-pe`/`.eh`-regen step the
runbook describes for class directories never applied here). Session
diff at tree root: `m3-o1-utillib-session.diff` (2,838 lines, 58 file
sections). Final two gate passes both logged to the canonical
`~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten; the file
reflects only the second/last pass — both were 0-error, exit 0).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   command-style section), `m3-rollout-runbook.md`, `m3-batches.md`,
   `porting-assessment.md` §14 in full, and
   `claude-history/m2/m2-utillib-REPORT.md` (the M2 session on this
   same directory), all before starting, per the prompt.
2. Confirmed no `.ch` files: `find src/overhead/util/lib -maxdepth 1
   -name '*.ch'` returns nothing. This directory is not a class
   directory — the M1 `-pe`/force-`.eh`-regen step does not apply.
3. Confirmed the signature DB (`build/desc`, 496 entries) already
   exists from 2026-07-25 per the prompt's note; did not rebuild it.
4. Ran `ansify --dir src/overhead/util/lib` for real (not `--dry-run`).
   Output: **83 files; 16 compile failure(s); 0 DRIFT finding(s)**.
   Summed the per-file helper counts across the whole report (`256`,
   including files that later failed and reverted) — matches the
   dry-run baseline's `256 file-local helpers` exactly. 0 methods, 0
   classprocs, 0 DRIFT, no `skipped`/bailout lines anywhere in the
   output — all match the dry-run baseline (83/0/0/256/0/0). The one
   number the dry-run couldn't predict (it never compiles) is the 16
   compile failures, addressed in §4 below — this was expected
   territory per the prompt's own framing ("Fix any COMPILE FAILED...
   fallout"), not a stop-condition.
5. Investigated all 16 compile failures by reproducing each one
   outside ansify's own auto-revert (a small script importing
   `ansify.py`'s `convert_file`/`compile_gate`/`run_fix_tools`
   directly, so I could see the *full* compiler output — ansify's own
   printed report truncates to the first 12 lines, which was not
   enough to see the real error in several cases). Found two
   categories — see §4 for the full breakdown — and fixed the two
   files that are actually part of the real build (`encode.c`,
   `unscribe.c`); left the other 14 untouched (correctly, they're
   pre-existing dead/excluded code, not M3 fallout — see §4.1).
6. Subtree-local gate, each step a separate Bash call, absolute paths,
   never chained: `make clean`, `make depend`, `make -k install`
   (logged to `dependInstall.log`) — 0 errors, exit 0. Repeated the
   same three calls a second time for determinism — 0 errors, exit 0
   again, `libutil.a` and `libafs.a` both installed both times.
7. Identified real runtime consumers via `nm -g` against `runapp` and
   `amsn.do` (§6) — did not run any AUIS GUI/terminal binary.
8. `fossil status` (§7) confirms exactly 58 edited files, all `.c`,
   all inside this directory. No commit made. `fossil diff >
   m3-o1-utillib-session.diff` written to the tree root. Confirmed via
   `fossil extras` that no stray backup files
   (`*.ansify-orig`/`*.inspect-orig`) or unexpected artifacts were
   left behind by the investigation scripts — only the usual untracked
   build byproducts (`Makefile`, `*.o`, `libutil.a`, `libafs.a`,
   `install.time`/`install.doc`), all removed and regenerated cleanly
   by the gate's own `make clean`.

## 3. Real instance count vs. dry-run baseline

| | Dry-run (census) | Real run |
|---|---|---|
| Files | 83 | 83 |
| Methods | 0 | 0 |
| Classprocs | 0 | 0 |
| File-local helpers | 256 | 256 (summed across all per-file reports, incl. the 16 that later failed and reverted) |
| DRIFT | 0 | 0 |
| Skipped/bailout | 0 | 0 |
| Compile failures | n/a (dry-run never compiles) | 16 |

Every number the dry-run could predict matches exactly. No
discrepancy to flag per the prompt's stop-condition — the 16 compile
failures are new information the dry-run structurally couldn't have
had, not a disagreement with it.

## 4. The 16 compile failures: two real fixes, 14 pre-existing dead code

Reproduced each failure outside ansify's auto-revert (importing its
`convert_file`/`run_fix_tools`/`compile_gate` functions directly, in a
scratch script, always restoring the file from my own backup in a
`finally` block) to see full compiler output instead of ansify's own
12-line-truncated report.

### 4.1 Thirteen files: pre-existing dead code, `WHITEPAGES_ENV` never defined — NOT M3 fallout

`btcreate.c`, `btr.c`, `btr1.c`, `btr2.c`, `btr3.c`, `btr4.c`,
`btstat.c`, `btw.c`, `wpbase.c`, `wpcgen.c`, `wpfuzz.c`, `wpname.c`,
`wpstat.c` all fail with `unknown type name 'bt_ErrorCode'` (or
`wp_ErrorCode`/`wp_PrimeKey`) and similar — these types are declared in
`overhead/util/hdrs/bt.h`, reached via `#include <btint.h>`, but every
one of these files gates that include behind
`#ifdef WHITEPAGES_ENV` (`btcreate.c:50-52`, same shape in the others)
**and the Imakefile itself gates the whole file-list behind the same
macro at the imake level**:

```
#ifdef WHITEPAGES_ENV
WHITEPAGESFILES =  btcreate.o btr.o btr1.o btr2.o btr3.o btr4.o btstat.o btw.o btwp.o \
	getcpw.o gtvpwent.o gtvpwkey.o gtvpwnam.o gtvpwuid.o getvuid.o  \
	wpbase.o wpcbase.o wpcgen.o wpcname.o wpfuzz.o wpgen.o wpname.o wpstat.o
#else /* WHITEPAGES_ENV */
WHITEPAGESFILES =
#endif /* WHITEPAGES_ENV */
...
LibraryTarget(libutil.a, $(OBJS) ${WHITEPAGESFILES})
```

Confirmed via the generated `Makefile` (`grep -n WHITEPAGESFILES`):
`WHITEPAGESFILES =` is **empty** in this build — `WHITEPAGES_ENV` is
not defined, same class of thing as `AFS_ENV`/`MK_CONSOLE` being off
elsewhere in the tree. These 13 objects never enter `libutil.a` and
are never built by `make -k install`; `ansify`'s own per-file compile
gate (`make base.o`) reaches them anyway because
`NormalObjectRule()` provides a generic `.c.o:` pattern rule for any
file in the directory, in or out of `$(OBJS)`. Confirmed this isn't
new breakage: `make btcreate.o` on the **unmodified, pristine**
(pre-ansify) file fails with the identical error — this file has never
compiled on this port; `ansify` neither caused nor could fix it.
**Not touched.** Reviving the CMU Whitepages (X.500-ish directory
lookup) subsystem is out of scope for a K&R→ANSI conversion session;
`ansify` correctly auto-reverted all 13 to their original (already
broken, already excluded from the real build) K&R state.

`verbose.c` is the 14th of this "pre-existing, out of scope" group,
but by a different, already-documented mechanism: it's excluded
directly from `$(OBJS)` by a comment already in the Imakefile
(`/* verbose.o: obsolete since it cannot be ported to ANSI C */`). Its
real error confirms that comment: `#error "Please use <stdarg.h>
instead of <varargs.h>"` from old-style `va_alist`/`va_dcl` varargs —
a real, already-known, already-excluded issue, not new fallout.
**Not touched.**

### 4.2 Two files: real fallout, fixed — narrow-type forward-declaration mismatch

`encode.c` and `unscribe.c` are both genuinely part of the build
(`OBJS`, not `WHITEPAGESFILES`) and both hit the same underlying
mechanism, a variant of the Pilot A `eq__WriteFILE`/`char sep` finding
(`porting-assessment.md` §14): a pre-existing **non-prototype**
forward declaration (`int char64();`, empty parens — C89's "unspecified
arguments" form) is compatible with a later *prototyped* declaration
only if every parameter type is unchanged by default argument
promotion. `char` promotes to `int`, so a prototype with a bare `char`
parameter is NOT compatible with an empty-parens declaration of the
same name — clang reports `error: conflicting types for 'char64'`
once ansify's helper conversion turns the K&R *definition*
(`char64(c) char c; {...}` → `int char64(char c)`) into a real
prototype, because the file also had that old declaration sitting
untouched nearby. Int/pointer-typed siblings in the same two files
(`output64chunk`, `to64`, `from64`, `hexchar`, `hash`, `findstyle`,
`usVersion`, `HandleKeyword`, `HandleClose`, `StartFrag`,
`UnScribeInit`, `UnScribe`, `UnScribeFlush`, `UnScribeAbort`,
`PrintMaybeFoldQuotingFormatting`, `PrintQuotingFormatting`) only
produced benign `-Wdeprecated-non-prototype` *warnings* (int and
pointer types are unaffected by default promotion, so old- and
new-style declarations stay compatible) — narrow (`char`) value
parameters are the specific trigger, same species as Pilot A.

- **`encode.c:43-44`**: pre-existing file-scope forward declarations
  `int output64chunk();` / `int char64();` predate any ansify
  involvement (not inserted by any tool this session). Fixed by hand,
  in the pristine K&R source, *before* running ansify on this file:
  retyped both to full prototypes matching their real definitions
  (`int output64chunk(int c1, int c2, int c3, int pads, FILE
  *outfile);`, `int char64(char c);`). Re-ran `ansify
  src/overhead/util/lib/encode.c` afterward — clean pass, 0 compile
  failures, 6 helpers converted.
- **`unscribe.c:54`**: this one is different in an important way — see
  §5, a new pipeline-interaction finding. The conflicting declaration
  (`static int WriteFrag();`) does not exist in the pristine file at
  all; it's inserted fresh by `fix-missing-static-decl` (step 1 of
  ansify's own pipeline) on *every* run, because `WriteFrag` is
  called via macro (`ADDCHAR`, line 98) before its K&R definition
  (line 333). Fixed by running `fix-missing-static-decl` for real,
  then hand-retyping the declaration it inserted to
  `static int WriteFrag(struct ScribeState *State, FILE *fPtr, char
  Chr);`, then running ansify's helper-conversion step (not the
  top-level `ansify` driver — see §5 for why) to convert the
  definition. Verified with a direct `make unscribe.o`: 22 benign
  `-Wdeprecated-non-prototype`/`-Wparentheses`/`-Wdangling-else`
  warnings, 0 errors, `unscribe.o` built.

## 5. New `ansify` fallout pattern: `fix-missing-static-decl`'s forward-declaration parser is not idempotent against a hand-typed fix

Not documented in `porting-assessment.md` §14 or the M1/M2 taxonomies
— worth flagging per the prompt's explicit ask, since this is the
first real (non-dry-run, non-pilot) M3 session.

`fix-missing-static-decl` (a separate, already-"keep as-is" tool that
ansify calls as its own step 1) recognizes an *existing* forward
declaration only via a narrow regex requiring **literally empty
parens** — `NAME()` — to decide a name is "already declared" and skip
inserting its own stub (`revival/tools/fix-missing-static-decl`,
`try_parse_decl_block`, the `term_m` regex). A full ANSI prototype
(`WriteFrag(struct ScribeState *State, FILE *fPtr, char Chr)`) does
**not** match that pattern, so the tool doesn't recognize it as a
declaration for `WriteFrag` at all — it still finds `WriteFrag`
"missing" and inserts a second, conflicting, empty-parens stub
`static int WriteFrag();` right after the `#include` block, in
addition to the one already there. Confirmed by direct reproduction:
re-running the full `ansify src/overhead/util/lib/unscribe.c` command
a second time, *after* I'd already hand-fixed the file, still printed
`[fix-missing-static-decl] Modified` and inserted a duplicate stub at
a different line, and the compile failed again — same error, now
against the duplicate rather than the original.

**Consequence**: for any file where the compile-gate failure is a
narrow-type mismatch against a declaration that `fix-missing-static-decl`
itself inserted (as opposed to a pre-existing declaration in the
pristine source, like `encode.c`'s), the fix is **not** "retype the
declaration, then re-run `ansify` on the file" as the prompt's general
guidance describes — that reintroduces the same failure non-
deterministically (a fresh duplicate, in a new spot). The working
sequence instead is: run `fix-missing-static-decl` once for real, hand
-retype the one declaration it inserted, then run only ansify's
helper-*conversion* step (not the top-level driver, which would
re-invoke `fix-missing-static-decl` and duplicate the stub), then
verify with a direct `make <base>.o`. This is a real, reproducible
tool limitation (not a one-off), worth fixing in
`fix-missing-static-decl`'s parser eventually (recognize a typed
prototype for a name as satisfying "already declared", not just the
empty-parens form) — flagging here rather than fixing the tool itself,
since tool construction is explicitly kept at the top level per
`porting-assessment.md` §14's Delegation ruling. Only `unscribe.c` hit
this in this directory (the one case where a narrow-typed helper is
both forward-referenced *and* had no pre-existing declaration of its
own) — but the mechanism will recur anywhere else in the tree with the
same shape (a `char`- or `short`-by-value static helper called before
its definition, no existing forward declaration).

## 6. Runtime consumers (identified via `nm -g`, not run)

`overhead/util/lib` builds two libraries, `libutil.a` (this session's
real product) and `libafs.a` (`lcstring.o`/`ucstring.o` only — both
were part of this session's 58 converted files but neither had
fallout).

- **`runapp`** (the binary `ez`/`messages`/`help`/`figure` symlink to)
  **statically links `encode.c`'s functions directly**, confirmed via
  `nm -g build/bin/runapp`: `_to64`, `_from64`, `_char64`,
  `_output64chunk` all defined (`T`) in `runapp`'s own symbol table,
  alongside the general profile/directory functions this directory
  always provided (`_AndrewDir`, `_getprofile`, `_FoldedEQ`, etc. —
  same set M2's session found). No `_UnScribe*`/`_WriteFrag` symbols
  in `runapp` — the unscribe/Scribe-format family isn't reached from
  this path.
- **`amsn.do`** (`build/dlib/atk/amsn.do`, the `messages` app's
  dynamically-loaded mail-store backend) **statically archives the
  `unscribe.c` family**, confirmed via `nm -g build/dlib/atk/amsn.do`:
  `_UnScribe`, `_UnScribeInit`, `_UnScribeFlush`, `_UnScribeAbort` all
  defined (`T`) directly in `amsn.do`'s own symbol table (`WriteFrag`
  itself is `static`, so it doesn't appear as a global symbol, but its
  only callers, `UnScribe`/`UnScribeFlush`, are confirmed present).
  This makes `messages` the load-bearing runtime check specifically
  for the `unscribe.c` fix (§4.2) — nothing in `runapp`/`ez` alone
  exercises it. `amsn.do` also still carries the `dbg_*` fdplumb
  family from this directory (already established by M2's session);
  unaffected by this session (no fdplumb file had any fallout this
  time — see §8).

### Exact runtime-check commands for wdc

Native Terminal.app, not an IDE terminal; `DISPLAY=:0;` as its own
statement first; use a **fresh** process for each check (`.do` files
cache for the life of a process).

1. **Baseline — every app's startup/general util path** (exercises
   `AndrewDir`/`getprofile`/profile parsing, all of which this
   directory's other 56 converted files touch, same as M2's
   equivalent check):
   ```
   DISPLAY=:0; ez
   ```
   Confirm the window opens normally.

2. **The load-bearing check for this session's real fix** (§4.2,
   `unscribe.c`/`WriteFrag` — CMU Scribe-format message-body decoding,
   the `X-Andrew-ScribeFormat:` header path):
   ```
   DISPLAY=:0; messages
   ```
   Open a message whose body uses Scribe formatting (bold/italic runs,
   if any test mailbox has one) or, at minimum, open and read any
   message body normally — `UnScribeInit`/`UnScribe`/`UnScribeFlush`
   all run on that path. Confirm no crash and the message body renders
   with its formatting intact (a `WriteFrag` regression would most
   likely show as garbled or truncated formatted text, not a crash,
   since the bug this session fixed was a compile-time type conflict,
   not a runtime logic change — the ANSI conversion is
   behavior-preserving by construction).

3. `encode.c`'s `to64`/`from64`/`char64`/`output64chunk` (base64-style
   encode/decode) are statically linked into every app via `runapp`
   but I did not find a specific UI action that isolates this path
   from general use — no dedicated command proposed beyond #1/#2;
   flagging here in case wdc knows of an AUIS feature that explicitly
   round-trips through this encoding (possibly related to
   `PrintQuotingFormatting`/attachment encoding in `messages`, given
   its neighbor `unscribe.c`'s subject matter, but not confirmed).

## 7. Files touched (58, all compile clean)

All 58 edited files compile clean (0 real errors) in their final
state, confirmed by two full `make clean && make depend && make -k
install` passes.

- 56 files converted straight through by `ansify --dir` with no
  fallout: `abbrpath.c`, `alquery.c`, `andrwdir.c`, `andydir.c`,
  `cawp.c`, `cellauth.c`, `config.c`, `errhdlr.c`, `fdplumb.c`,
  `fdplumb2.c`, `fdplumb3.c`, `fdplumb4.c`, `fdplumb5.c`, `fdplumb6.c`,
  `foldedeq.c`, `fselect.c`, `fwrtallc.c`, `getaddr.c`, `getcpw.c`,
  `gethome.c`, `getla.c`, `getpty.c`, `gtime.c`, `gtvpwkey.c`,
  `gtvpwnam.c`, `gtvpwuid.c`, `lcappend.c`, `lcstring.c`, `lineprom.c`,
  `localdir.c`, `newstr.c`, `nicetime.c`, `pause.c`, `procstuf.c`,
  `profile.c`, `readlink.c`, `setprof.c`, `system.c`, `t2open.c`,
  `tfail.c`, `thiscell.c`, `times.c`, `titles.c`, `tokpak.c`,
  `tokunpak.c`, `topen.c`, `ucstring.c`, `uerror.c`, `ulsindex.c`,
  `ulstlmat.c`, `ulstrcmp.c`, `usignal.c`, `vclose.c`, `venusop.c`,
  `writeall.c`, `xbasedir.c`. (`getcpw.c`/`gtvpwkey.c`/`gtvpwnam.c`/
  `gtvpwuid.c` are in the WHITEPAGES *file family* by name but are not
  gated by `#ifdef WHITEPAGES_ENV` inside their own source the way the
  bt/wp-prefixed ones are, and none of the four is in the Imakefile's
  `WHITEPAGESFILES` list at all — they're ordinary `$(OBJS)` members
  that happen to share a naming convention with the dead family, and
  ansify converted and compiled them without incident.)
- `encode.c` — pre-existing (non-ansify-inserted) forward declarations
  for `output64chunk`/`char64` retyped from empty-parens K&R form to
  full ANSI prototypes (§4.2), then ansify-converted (6 helpers).
- `unscribe.c` — `fix-missing-static-decl`-inserted forward
  declaration for `WriteFrag` retyped to a full ANSI prototype (§4.2,
  §5), then ansify-converted (14 helpers).
- 14 files touched by `ansify --dir` and then correctly auto-reverted
  by the tool itself (compile-gate failure, pre-existing/out-of-scope
  per §4.1): `btcreate.c`, `btr.c`, `btr1.c`, `btr2.c`, `btr3.c`,
  `btr4.c`, `btstat.c`, `btw.c`, `verbose.c`, `wpbase.c`, `wpcgen.c`,
  `wpfuzz.c`, `wpname.c`, `wpstat.c` — **not** in the fossil-status
  list below (correctly untouched in the final working tree).
- 11 files ansify reported "no K&R definitions converted" (nothing to
  do, no diff): `btwp.c`, `desym.c`, `fpacheck.c`, `getmyhom.c`,
  `getvuid.c`, `gtvpwent.c`, `hname.c`, `svcconf.c`, `wpcbase.c`,
  `wpcname.c`, `wpgen.c`.

Regenerated `Makefile` (and `Makefile.BAK`, `*.o`, `libutil.a`,
`libafs.a`, `install.time`/`install.doc`) are build byproducts, not
fossil-tracked, not source edits — confirmed via `fossil extras`; all
removed and regenerated cleanly by the gate's two `make clean` passes,
no stray files left by the investigation scripts.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     5eb1c0f32fd20051c3fb764fc8221d898f695575 2026-07-25 17:44:20 UTC
parent:       1e7dd1a3aed717ffed56c06ae11d5c3d7fa60c4c 2026-07-25 17:36:15 UTC
tags:         andrew-6.4
comment:      docs: consolidate m3-batches.md 24->16 batches (17 sessions w/ pilot) to match M2's batching aggressiveness; correct the scope comparison (M2 touched 28 dirs, not the whole tree) (user: wdc)
EDITED     src/overhead/util/lib/abbrpath.c
EDITED     src/overhead/util/lib/alquery.c
EDITED     src/overhead/util/lib/andrwdir.c
EDITED     src/overhead/util/lib/andydir.c
EDITED     src/overhead/util/lib/cawp.c
EDITED     src/overhead/util/lib/cellauth.c
EDITED     src/overhead/util/lib/config.c
EDITED     src/overhead/util/lib/encode.c
EDITED     src/overhead/util/lib/errhdlr.c
EDITED     src/overhead/util/lib/fdplumb.c
EDITED     src/overhead/util/lib/fdplumb2.c
EDITED     src/overhead/util/lib/fdplumb3.c
EDITED     src/overhead/util/lib/fdplumb4.c
EDITED     src/overhead/util/lib/fdplumb5.c
EDITED     src/overhead/util/lib/fdplumb6.c
EDITED     src/overhead/util/lib/foldedeq.c
EDITED     src/overhead/util/lib/fselect.c
EDITED     src/overhead/util/lib/fwrtallc.c
EDITED     src/overhead/util/lib/getaddr.c
EDITED     src/overhead/util/lib/getcpw.c
EDITED     src/overhead/util/lib/gethome.c
EDITED     src/overhead/util/lib/getla.c
EDITED     src/overhead/util/lib/getpty.c
EDITED     src/overhead/util/lib/gtime.c
EDITED     src/overhead/util/lib/gtvpwkey.c
EDITED     src/overhead/util/lib/gtvpwnam.c
EDITED     src/overhead/util/lib/gtvpwuid.c
EDITED     src/overhead/util/lib/lcappend.c
EDITED     src/overhead/util/lib/lcstring.c
EDITED     src/overhead/util/lib/lineprom.c
EDITED     src/overhead/util/lib/localdir.c
EDITED     src/overhead/util/lib/newstr.c
EDITED     src/overhead/util/lib/nicetime.c
EDITED     src/overhead/util/lib/pause.c
EDITED     src/overhead/util/lib/procstuf.c
EDITED     src/overhead/util/lib/profile.c
EDITED     src/overhead/util/lib/readlink.c
EDITED     src/overhead/util/lib/setprof.c
EDITED     src/overhead/util/lib/system.c
EDITED     src/overhead/util/lib/t2open.c
EDITED     src/overhead/util/lib/tfail.c
EDITED     src/overhead/util/lib/thiscell.c
EDITED     src/overhead/util/lib/times.c
EDITED     src/overhead/util/lib/titles.c
EDITED     src/overhead/util/lib/tokpak.c
EDITED     src/overhead/util/lib/tokunpak.c
EDITED     src/overhead/util/lib/topen.c
EDITED     src/overhead/util/lib/ucstring.c
EDITED     src/overhead/util/lib/uerror.c
EDITED     src/overhead/util/lib/ulsindex.c
EDITED     src/overhead/util/lib/ulstlmat.c
EDITED     src/overhead/util/lib/ulstrcmp.c
EDITED     src/overhead/util/lib/unscribe.c
EDITED     src/overhead/util/lib/usignal.c
EDITED     src/overhead/util/lib/vclose.c
EDITED     src/overhead/util/lib/venusop.c
EDITED     src/overhead/util/lib/writeall.c
EDITED     src/overhead/util/lib/xbasedir.c
```

Exactly 58 files, matching this section's list (56 clean-converted + 2
hand-fixed). No commit made. `m3-o1-utillib-session.diff` (2,838
lines) written to the tree root.

## 8. fdplumb family (prompt's explicit ask)

`fdplumb.c` through `fdplumb6.c` (all six, confirmed present, same as
M2's session found) converted cleanly with **zero** fallout this
session — no compile failures, no DRIFT, nothing narrow-typed among
their helpers. Since M3 only converts function *definitions* (not the
`#define dbg_open ...` macro renaming in `fdplumb.h`/`fdplumbi.h`,
which M2's session already examined and left as a housekeeping
recommendation), and since ansify's helper-conversion pass operates
purely on each file's own K&R declaration block, there was nothing for
this session to find or fix in this family — the two real fixes this
session produced (§4.2) are unrelated files (`encode.c`, `unscribe.c`),
not fdplumb. No new fdplumb-relevant finding to report beyond
confirming the family converted cleanly.

## 9. Open questions / anything that surprised you

- §5's `fix-missing-static-decl` non-idempotency-against-a-hand-fix
  finding is the main surprise and the most useful signal for sizing
  the rest of the M3 rollout — it's a real, reproducible tool
  limitation (not a one-off), and I'd expect it to recur in any other
  directory with a `char`/`short`-by-value static helper that's
  forward-referenced with no pre-existing declaration. Recommend
  folding this into `porting-assessment.md` §14 or a new M3 runbook
  fallout-taxonomy section before the next batch, so a future session
  recognizes the shape immediately (duplicate stub after a
  seemingly-correct hand-fix, same error recurring at a new line
  number) instead of re-deriving it.
- §4.1's "compile-gate reaches files the real build doesn't build at
  all" finding (the `WHITEPAGES_ENV`-gated dead family) is
  directory-specific (this is the one M3 batch the runbook already
  flagged as "expect DRIFT/complexity here again, not a routine
  batch," and it was right, just not in the DRIFT dimension) but worth
  keeping in mind for any other directory with an imake-level
  `#ifdef`-gated file-list macro: `ansify`'s per-file `make base.o`
  gate uses the directory's generic pattern rule, which doesn't know
  or care whether the target file is actually reachable from
  `install`/`all` — a compile failure there is not automatically real
  fallout, and conversely a compile *success* there doesn't mean the
  file matters either (harmless either way, but worth a quick
  `grep`/Makefile check before spending time on a failure in a file
  whose name suggests it belongs to a `#ifdef`-gated family).
- No DRIFT and no parser-bailout this session — both of the prompt's
  named "stop and report, don't resolve" categories were genuinely
  absent, consistent with this directory having zero `.ch` files
  (structurally impossible for DRIFT) and the file-local helper parser
  never refusing to guess on anything here.
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
