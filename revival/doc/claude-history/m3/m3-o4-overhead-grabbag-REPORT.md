# M3 Wave 1 batch O4: overhead grab-bag (19 directories) — completes Wave 1

## 1. Status

Stopped at the (only) gate — subtree-local `make -C <dir> clean`, then
`depend`, then `-k install`, each a separate call, run twice per
directory for determinism. All 19 directories green both times, zero
real `error:` lines. No tree-wide gate run (none of these 19 is
`atkams/messages/lib` or `contrib/zip/lib`, per the prompt). No commits
made. Working tree: 47 files edited, all inside the 19 batch
directories (including two same-directory public headers,
`overhead/cmenu/shadows.h` and `overhead/index/index.h` — in scope,
not cross-directory). `overhead/class/testing/Imakefile` also edited
(the `-pe` flag line). Session diff at tree root:
`m3-o4-overhead-grabbag-session.diff` (5,974 lines). This report lives
in `revival/doc/claude-history/` per the standard convention.

This completes **Wave 1 of the M3 ANSI-C conversion milestone** (O1,
O2, O3, O4 all done) — see §9.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including its
   "Command style" section), `m3-rollout-runbook.md` (including its
   full "Findings from real sessions" — O1, O2, O3), `m3-batches.md`,
   and `porting-assessment.md` §14 and §17 in full, plus skimmed
   `m3-o1-utillib-REPORT.md`, `-o2-imagecodecs-REPORT.md`,
   and `-o3-mail-eli-bison-REPORT.md` — all before starting.
2. Confirmed `.ch` presence per directory (`find <dir> -maxdepth 1
   -name '*.ch'`): only `overhead/class/testing` has any (2 —
   `testobj.ch`, `testobj2.ch`); all other 18 have zero. Matches the
   prompt's expectation exactly.
3. Applied the `-pe`/`.eh` procedure to `overhead/class/testing` first
   (§3 below), verified it compiles, then ran `ansify --dir` on it.
4. `ansify --dir` (or, for `ossupport`, a single-file `ansify
   <file>` call — see §3's `ossupport` note) each of the other 18
   directories for real, one at a time, investigating every compile
   failure with a scratch script importing `ansify`'s own
   `run_fix_tools`/`convert_file`/`compile_gate` directly (same
   technique O1–O3 used) to see full compiler output instead of the
   tool's own 12-line-truncated report.
5. Fixed all ordinary compile-gate fallout by hand (§4); left every
   pre-existing-dead-code compile failure untouched, confirming each
   with the pristine (unmodified) file and the generated Makefile
   (§5) before moving on.
6. Subtree-local gate for each of the 19 directories, twice each, as
   separate `make -C <absolute-path> clean`/`depend`/`-k install`
   calls, never chained.
7. Identified real runtime consumers via `nm -g` on `.a`/`.do` files
   (direct `nm`/`ls` on `runapp`/`ez`/`messages`/`cui`/binaries is
   denied by the harness, same restriction O3's session hit) and
   Imakefile `LIBS`/`ProgramTarget` lines (§7).
8. `fossil status`/`fossil extras` (§8) — 47 files edited, all inside
   the 19 directories (plus the one Imakefile), no stray files, no
   commit made. `m3-o4-overhead-grabbag-session.diff` written to the
   tree root.

## 3. `overhead/class/testing` — the batch's first real `-pe`/`.eh` rollout

**Judgment call, flagged per the prompt's own framing**: treated this
as the first *real* (committed-pending) `-pe` application in the
project, per the prompt's reasoning (Wave 1 legitimately precedes the
`atk/eq` Wave-4 "pilot" in dependency order; blast radius here is a
3-file testing/demo directory). Flagging for wdc to confirm after the
fact, and noting that `m3-batches.md`'s Wave 4 "eq pilot" framing is
now stale if wdc agrees (this batch already proved the mechanic live).

### Imakefile diff

```diff
 CLASS=../pp/class
 MAKEDO=$(BASEDIR)/bin/makedo
 LOCALINCLUDES= -I../lib -I../machdep/machine -I.
+CLASSFLAGS = $(CLASSINCLUDES) -pe
```

Confirmed the flag landed in the generated Makefile's `.ch.eh:` rule
(`make -C src/overhead/class/testing Makefile`, then `grep -n
CLASSFLAGS Makefile`):

```
294:CLASSFLAGS = $(CLASSINCLUDES) -pe
305:.ch.ih: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
306:.ch.eh: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
```

(Line 39 shows the tree-default `CLASSFLAGS = $(CLASSINCLUDES)` from
`imake.tmpl`; line 294's restatement — from the Imakefile — wins,
since make uses the last definition. No prior directory in the tree
had a `CLASSFLAGS` override at all: `grep -rl CLASSFLAGS
--include=Imakefile src` found nothing before this edit, confirming
this really is the first live use of the mechanic.)

### `.eh` regen

Removed `testobj.eh`/`testobj2.eh`, then `make -C ... testobj.eh
testobj2.eh` to force regeneration under the new flag.

### The eq-shape failure did NOT recur — something else did (as the prompt anticipated)

Built `testobj.o` against the freshly `-pe`-regenerated `.eh` *before*
running `ansify` at all, to isolate flag-fallout from conversion-tool
fallout. Result: **two real compile errors**, neither the eq
narrow-type-promotion shape:

```
testobj.c:39:9: error: conflicting types for 'testobject__InitializeClass'
   39 | boolean testobject__InitializeClass()
      |         ^
./testobj.eh:159:9: note: previous declaration is here
  159 | boolean testobject__InitializeClass(struct classheader *);
testobj.c:45:9: error: conflicting types for 'testobject__InitializeObject'
   45 | boolean testobject__InitializeObject()
      |         ^
./testobj.eh:158:9: note: previous declaration is here
  158 | boolean testobject__InitializeObject(struct classheader *, struct testobject *);
```

Root cause: `testobj.c`'s `InitializeClass`/`InitializeObject`
definitions were written with **literally empty parens** — no
parameter names at all, not even the implicit `classID`/`self` — an
old-style shape that ANSI C treats as a true zero-parameter
*definition* (unlike a mere *declaration* with empty parens, which
means "unspecified arguments"). `-pe` makes classpp emit a fully typed
prototype for these two names regardless — 1 param (`classID`) for
ordinary classprocs like `InitializeClass`, and classpp's own
hardcoded 2-param convention (`classID`, `self`) for
`InitializeObject`/`FinalizeObject` specifically (verified directly in
`overhead/class/pp/class.c:1122`, matching `porting-assessment.md`
§17's write-up of that special-casing) — so the true-zero-arg
definition now conflicts.

This is **not** `ansify`'s own doing and not a DRIFT/skip report
either: `ansify`'s helper-conversion parser requires at least one bare
identifier in the parens to treat a definition as a K&R candidate
(`BARE_PARAMS` regex needs `[A-Za-z_]\w*`, which a literally-empty
parameter list never satisfies) — so it silently leaves
`InitializeClass`/`InitializeObject` alone entirely, no candidate, no
skip message, nothing to report. Confirmed by running `ansify --dir`
on the directory: 0 DRIFT, 0 skipped, matching the dry-run baseline
exactly (which never depends on `-pe` at all, since DRIFT/skip
detection is `-pe`-independent). The conflict is purely a `-pe`/`.eh`
regen fallout, invisible to `ansify`'s own reporting.

**Fix** (hand-applied to both `testobj.c` and `testobj2.c`, matching
class.c's own codegen convention — the same source of truth `ansify`
uses for method conversion): added the implicit parameters explicitly,
converting to full ANSI headers:

```c
boolean testobject__InitializeClass(struct classheader *classID)
boolean testobject__InitializeObject(struct classheader *classID, struct testobject *self)
```

(and the `testobject2`/`struct testobject2` equivalents in
`testobj2.c`). Verified with direct `make testobj.o`/`testobj2.o`: 0
errors, only benign `-Wdeprecated-non-prototype` warnings from
`Diddle` (still K&R at that point) and classpp's own generated
`GetClassInfo` code in the `.eh`.

Then ran `ansify --dir src/overhead/class/testing` for real:

```
src/overhead/class/testing/testmain.c:
    converted: 0 methods, 0 classprocs, 2 helpers
src/overhead/class/testing/testobj.c:
    converted: 1 methods, 0 classprocs, 0 helpers
src/overhead/class/testing/testobj2.c:
    converted: 1 methods, 0 classprocs, 0 helpers

3 file(s); 0 compile failure(s); 0 DRIFT finding(s)
```

`Diddle` (the one real class method in each `.ch`, matching the
dry-run baseline's "2 methods") converted cleanly via the signature-DB
lookup path — the method-conversion half of the pipeline exercised for
real for the first time, working exactly as designed once the
InitializeClass/InitializeObject fallout was cleared by hand.
`testmain.c` (a plain helper file, no `.ch` involvement) converted 2
helpers with no issues.

**New fallout pattern for the taxonomy** (not previously documented in
O1–O3 or `porting-assessment.md` §14/§17): *a class lifecycle method
written with a truly-empty parameter list (no implicit
`classID`/`self` placeholder at all) is invisible to `ansify`'s own
candidate detection, but still conflicts with a `-pe`-regenerated `.eh`
prototype.* This is a `-pe`/`.eh`-regen-side fallout, not an
`ansify --dir`-side one — worth checking for on sight (grep a
`-pe`-flagged directory's `.c` files for `Name__Method()` with zero
characters between the parens) before assuming a clean `ansify --dir`
report means the flag change itself is risk-free.

### `overhead/class/testing` gate

`testmenu`... — no, this directory's `test::`-only artifact is
`testmain` (only reachable via `make test`, never `install`); the
prompt bans running it interactively, and `ansify`'s own compile gate
already confirmed `testmain.o` builds clean (0 compile failures in the
report above), so no further action was needed. `install` itself only
builds `testobj.do`/`testobj2.do`. Gate (`clean`/`depend`/`-k install`)
green twice, deterministic, both `.do` files rebuilt identically both
times.

## 4. Compile-gate fallout: 4 real fixes across 3 directories

### `overhead/cmenu/shadows.c` + `shadows.h` — two narrow-type
declarations, one same-directory public header

`fix-missing-static-decl` inserted tool-side stubs
`static void Darken();` / `static void Lighten();` (both called via
forward reference before their K&R definitions) — narrow
`unsigned short`-by-value parameters, the familiar
tool-inserted-empty-parens non-idempotency shape from O1/O2/O3. Fixed
with the established sequence: ran `fix-missing-static-decl` once for
real, hand-retyped both inserted stubs to full prototypes, then ran
only `convert_file` (not the top-level `ansify` driver, which would
re-invoke `fix-missing-static-decl` and duplicate the stub — confirmed
this recurrence directly when I later re-ran the full `--dir` driver a
second time by habit; auto-revert correctly restored my fix, no harm
done, but confirms the non-idempotency bug is exactly as
O1/O2/O3 described and still not tool-fixed).

Second, separate conflict in the same file: `shadows_ComputeColor`
(narrow `unsigned short` params) is declared, non-static, in
`shadows.h` — a **same-directory but externally-consumed** public
header (`overhead/cmenu/shadows.h`), included by two files *outside*
this batch: `atk/basics/common/graphic.c` and `atk/basics/x/menubar.c`
(matches O3's "ordinary public headers are not `.eh`-locality-
guaranteed" finding, same mechanism, this time within the batch's own
directory rather than reaching into an unbatched one — still worth
verifying downstream). Checked blast radius: all 6 call sites across
`cmcreate.c`/`menubar.c`/`graphic.c` pass either already-`unsigned
short`-typed fields (`XColor.red/green/blue`) or explicit
`(unsigned short)`-cast values — no bare-`long` caller a retype would
silently truncate incompatibly. Retyped `shadows.h`'s two declarations
to full prototypes, reinstalled the header
(`install -m 644 src/overhead/cmenu/shadows.h build/include/shadows.h`,
matching O3's `mail.h` precedent — proper `install`, not `cp`), then
verified both external consumers directly: `make -C
src/atk/basics/common graphic.o` and `make -C src/atk/basics/x
menubar.o`, both 0 errors. `shadows.o` itself: 0 errors, only benign
warnings.

### `overhead/mail/metamail/metamail/mailto.c` — `PutQP`, pre-existing
multi-name declaration

Pre-existing (not tool-inserted) declaration list `extern int PutQP(),
EmitAddresses(), DeAlias(), fputsquoting();` — only `PutQP`'s
definition takes a narrow `unsigned char c` parameter (the other
three take pointer/int types, stay compatible). Split `PutQP` into its
own retyped declaration, left the other three untouched:
```c
extern int PutQP(unsigned char c, FILE *fp);
extern int EmitAddresses(), DeAlias(), fputsquoting();
```
Verified: `ansify src/overhead/mail/metamail/metamail/mailto.c` then
clean, 0 compile failures, 48 helpers converted.

### `overhead/index/index.c` + `index.h` — `index_Hash`, same-directory
header

`index.h:127` had `extern long index_Hash();` (empty parens) against
a real definition taking a narrow `short hashSize` parameter. Checked
consumers (`grep -rln index_Hash src`): only within-directory
(`index.c`, `indexio.c`), both already passing
`ai->hashTableSize`, itself declared `short` in `index.h` — safe.
Retyped: `extern long index_Hash(char *astring, short hashSize);`.
Verified: `ansify --dir` clean, 0 compile failures, 18 helpers
converted for `index.c`.

### `overhead/rxp/regexp.c` — three narrow-type declarations,
`STATIC`-macro forward decls

Pre-existing forward declarations under the file's own `#define
STATIC static` convention: `STATIC char *regnode();`, `STATIC void
regc();`, `STATIC void reginsert();` — all conflict against real
`char`-by-value definitions (`regnode(char op)`, `regc(char b)`,
`reginsert(char op, char *opnd)`). Retyped all three in place. Also
hit the tool-driver-reintroduces-duplicate-stub trap a second time
here (running the full `ansify --dir` again after my hand-fix
reinserted a duplicate untyped `regnode()` stub via
`fix-missing-static-decl`, auto-reverted correctly) — used the
`convert_file`-only bypass to finish the conversion cleanly (18
helpers, including the dead `#ifdef STRCSPN`-gated `strcspn` — that
whole block is never compiled in this build, `STRCSPN` is undefined
tree-wide, matching the benign "SKIPPED (libc name collision)"
message `fix-missing-static-decl` printed for it). Verified: `make
regexp.o` clean, 0 errors.

## 5. `overhead/rxp/regexp.h` — untouched, confirmed safe (prompt's explicit ask)

`regexp.h`'s two public declarations, `extern regexp *reg_comp();` and
`extern int reg_exec();`, were **not modified by anything in this
session**. Confirmed why: `reg_comp`'s real definition is `regexp *
reg_comp(char *exp)` and `reg_exec`'s is `int reg_exec(regexp *prog,
char *string)` — both parameters in both functions are pointer types,
not subject to C's default-argument promotion, so an empty-parens
declaration stays compatible with the fully-typed definition under
C89 rules (only the *narrow* `char`-by-value helpers inside the same
file — `regnode`/`regc`/`reginsert`, all `static`, none reachable from
`regexp.h` — needed retyping; see §4). `ansify`'s helper-conversion
pass only ever touches the `.c` file it's converting, never a header,
so there was never a mechanism by which it could have touched
`regexp.h` even if a conflict had existed. No action needed, matching
the prompt's framing exactly.

## 6. Pre-existing, out-of-M3-scope compile failures — left untouched (3 new instances of the O1 WHITEPAGES pattern, one variant)

All confirmed via the pristine (unmodified) file failing identically,
plus the generated Makefile showing the target absent from
`all::`/`install::` (or, for `inst`/`overhead/malloc`, the *entire*
directory's Makefile having no real targets at all).

- **`overhead/fonts/cmd/fdbwm.c` + `wmfdb.c`** — both
  `#include <font.h>`/`fntmanip.h` inside `#ifdef WM_ENV` (source-level,
  "to avoid makedepend errors" per the code's own comment) *and* both
  `ProgramTarget`/`InstallProgram` lines gated behind `#ifdef WM_ENV`
  at the **Imakefile** level (`WM_ENV` commented out in `allsys.h`).
  Neither file is in the generated Makefile's `all::` at all —
  `ansify`'s own generic `.c.o:` pattern rule (`NormalObjectRule()`)
  is what let its compile gate reach them anyway, exactly the O1
  WHITEPAGES mechanism. 200+ cascading "undeclared identifier"/
  "incomplete type" errors, all downstream of the one missing header.
  Not touched — reviving the WM (window-manager bitmap font) subsystem
  is out of scope.
- **`overhead/util/cmd/newbt.c`** — same shape, gated behind `#ifdef
  WHITEPAGES_ENV` (off), `bt.h` include and `ProgramTarget`/
  `InstallProgram` both skipped. Not touched.
- **`overhead/malloc` (all 4 files) and `inst/inst.c`** — a *stronger*
  variant: the **entire Imakefile body** is gated (`overhead/malloc`
  behind `#ifdef ANDREW_MALLOC_ENV`, `inst` behind `#ifdef
  LINKINSTALL_ENV`/`BUILDANDREWINSTALL_ENV`), so the generated
  Makefile has **no `.c.o:` rule at all** — not even the generic one.
  `ansify`'s `make base.o` gate still reached these files via GNU
  Make's built-in implicit `.c.o:` rule (confirmed: `inst.c` failed on
  `fatal error: 'a.out.h' file not found`, a pre-Mach-O a.out-header
  dependency that cannot exist on Darwin at all; `overhead/malloc`'s 4
  files happened to have nothing left to convert, so `ansify` never
  even invoked its compile gate on them — the dry-run baseline's "3 of
  4 already ANSI" framing is coincidentally consistent but the real
  reason for zero real fallout is directory-inertness, not file
  ANSI-ness). Matches `rollout-procedure.md`'s Liveness census
  "wpedit" precedent (a directory can be in `SUBDIRS` while its whole
  Imakefile body is `#ifdef`-gated off) exactly — this is the same
  shape recurring at the single-directory level rather than at a
  parent's `SUBDIRS` line.
  **`overhead/malloc` has a site.h-override wrinkle worth flagging**:
  `allsys.h:128` `#define ANDREW_MALLOC_ENV 1`, but `site.h:11`
  `#undef ANDREW_MALLOC_ENV` — the exact "site.h overrides allsys.h,
  read them together" caution `rollout-procedure.md`'s Liveness census
  section already warns about (citing M1 batch 7's own mis-census from
  the same mechanism), now confirmed recurring here. Neither file was
  touched; both directories' gates are legitimately trivial no-ops
  (confirmed clean twice).
- **`overhead/cmenu/testmenu.c`** — `test::`-only (never `install`),
  fails independently of anything M3 touches:
  `implicit declaration of function 'cmenu_SetActive'`/`'exit'`/
  `'cmenu_Activate'`. Root-caused (not required to fix, but worth
  recording): `cmenu.h` guards its typed declarations with `#ifdef
  _STDC_` — the pre-standard, single-underscore macro name, never the
  standard `__STDC__` clang actually defines — so the `#else` untyped
  branch has been unconditionally live for the directory's entire
  history, and that untyped branch is *missing* `cmenu_Activate`/
  `cmenu_SetActive` entirely. Within `overhead/cmenu` itself neither
  function is ever *called* (only defined), so the real `install`
  target never hits this; the one real external consumer,
  `atk/basics/x/xim.c`, already carries its own local
  `extern int cmenu_Activate();` and doesn't hit it either. A genuine
  ~35-year-old dead-branch typo (same species as O3's
  `regcomp`/`regexec` finding, different mechanism — a macro-name typo
  rather than a function-name typo) with **zero observable effect** in
  this build (only a `test::`-only file's fixed-arity mismatch is
  affected) — noted here for wdc's awareness, not fixed (out of scope,
  no functional change on the table, touching `cmenu.h`'s `#ifdef` for
  zero benefit felt like unwarranted risk).

## 7. New `ansify` fallout pattern found this session

The `-pe`/`.eh`-regen-side finding from §3
(`InitializeClass`/`InitializeObject` with a truly-empty parameter
list, invisible to `ansify`'s candidate detector but conflicting with
the regenerated `.eh`) is the one genuinely new pattern from this
batch, not previously described in `porting-assessment.md` §14/§17 or
`m3-rollout-runbook.md`'s O1/O2/O3 findings. Every other fallout this
session (tool-inserted-stub non-idempotency, pre-existing-declaration
narrow-type conflicts, same-directory-header blast radius, Imakefile-
gated dead code reached by the generic compile rule) is a recurrence
of an already-documented O1/O2/O3 shape, just in new files.

## 8. Real instance counts vs. dry-run baselines

| Directory | Dry-run (files/helpers/methods/DRIFT/skipped) | Real run |
|---|---|---|
| `overhead/cmenu` | 8/~8+/0/0/0 | 8 files; 0 DRIFT; 0 skipped; **2 compile-gate fixes** (`shadows.c`+`shadows.h`); `testmenu.c` pre-existing dead (test-only), untouched |
| `overhead/mail/metamail/metamail` | 7/~13+/0/0/0 | 7 files; 0 DRIFT; 0 skipped; **1 compile-gate fix** (`mailto.c`/`PutQP`) |
| `overhead/mail/metamail/richmail` | 6/~25+/0/0/0 | 6 files; 0 DRIFT; 0 skipped; 0 fallout — clean |
| `overhead/malloc` | 4/0/0/0/0 | 4 files; **entire directory inert** (`ANDREW_MALLOC_ENV` off via `site.h` override) — 0 real conversions, not "already ANSI" as the baseline framed it |
| `overhead/index` | 4/~24+/0/0/0 | 4 files + `index.h`; 0 DRIFT; 0 skipped; **1 compile-gate fix** (`index.c`+`index.h`/`index_Hash`) |
| `overhead/class/lib` | 4/~26+/0/0/0 | 4 files; 0 DRIFT; **1 new parser-bailout** (`class_EnterInfo` — function-pointer-returning-pointer regex gap, self-healing, left K&R); 0 compile fallout |
| `overhead/class/testing` | 3/~2/2/0/0 | 3 files; 2 methods (Diddle x2) converted via signature-DB lookup; 0 DRIFT; 0 skipped; **`-pe`/`.eh` rollout + 2 hand-fixed lifecycle methods**, see §3 |
| `overhead/class/cmd` | 3/~10/0/0/0 | 3 files; 0 DRIFT; 0 skipped; 0 fallout — clean |
| `overhead/class/machdep/darwin` | 2/~1/0/0/0 | 2 files (`classproc.c` already ANSI); 0 fallout — clean |
| `overhead/rxp` | 3/~20/0/0/0 | 3 files; 0 DRIFT; 0 skipped; **3 compile-gate fixes** (`regexp.c`: `regnode`/`regc`/`reginsert`); `regexp.h` untouched (§5) |
| `overhead/fonts/cmd` | 3/~22/0/0/0 | 3 files; 1 converted (`fdbbdf.c`); `fdbwm.c`/`wmfdb.c` pre-existing dead (`WM_ENV` off), untouched |
| `overhead/util/cmd` | 2/~4/0/0/0 | 2 files; 1 converted (`pref.c`); `newbt.c` pre-existing dead (`WHITEPAGES_ENV` off), untouched |
| `overhead/sys` | 2/~1/0/0/0 | 2 files (`conftest.c` already ANSI); 0 fallout — clean |
| `overhead/mkparser` | 2/~14/0/0/0 | 2 files (`testparse.c` already ANSI); 0 fallout — clean |
| `overhead/util/hdrs` | 1/~1/0/0/0 | 1 file; 0 fallout — clean |
| `overhead/errors` | 1/~1/0/0/0 | 1 file; 0 fallout — clean |
| `overhead/addalias` | 1/~6/0/0/0 | 1 file; 0 fallout — clean |
| `ossupport` (own-level only) | 1/~5/0/0/0 | 1 file (`oscommon.c`); 0 fallout — clean; confirmed no platform-subdirectory file touched |
| `inst` | 1/~4/0/0/0 | 1 file; **entire directory inert** (`LINKINSTALL_ENV`/`BUILDANDREWINSTALL_ENV` both off) — `inst.c` reverted (`a.out.h` doesn't exist on Darwin), zero net change |

Every dry-run-predictable number (files, DRIFT, skipped, method/
classproc counts where non-zero) matches exactly; the only
discrepancies are compile-gate fallout counts and the two directories
whose "clean" numbers turned out to mean "entirely inert" rather than
"already ANSI" — both structurally invisible to a dry run, same
framing as every prior M3 batch.

## 9. `fossil status` — 47 files changed, all inside the 19 directories; no commit made

```
$ fossil status | grep -c "^EDITED"
47
```

Breakdown by directory: `overhead/cmenu` 7 (cmactiv/cmcreate/cmdraw/
cmmanip/scache/shadows.c/shadows.h), `overhead/mail/metamail/metamail`
7 (all), `overhead/mail/metamail/richmail` 6 (all),
`overhead/malloc` 0 (directory inert), `overhead/index` 5
(index.c/index.h/indexio.c/rcordset.c/testidx.c), `overhead/class/lib`
3 (class.c/classind.c/mapping.c — classproc.c already ANSI),
`overhead/class/testing` 4 (Imakefile/testmain.c/testobj.c/testobj2.c),
`overhead/class/cmd` 3 (all), `overhead/class/machdep/darwin` 1
(doload.c — classproc.c already ANSI), `overhead/rxp` 3 (all),
`overhead/fonts/cmd` 1 (fdbbdf.c only), `overhead/util/cmd` 1
(pref.c only), `overhead/sys` 1 (sys.c — conftest.c already ANSI),
`overhead/mkparser` 1 (cparser.c — testparse.c already ANSI),
`overhead/util/hdrs` 1, `overhead/errors` 1, `overhead/addalias` 1,
`ossupport` 1 (oscommon.c — confirmed no `hpx`/`aix3`/`scox`/`vaxx`/
`i386_bsd`/`telmat`/`solaris` file touched), `inst` 0 (directory
inert, `inst.c` correctly reverted, zero net diff). Sums to 47,
matching `fossil status`'s count exactly. `fossil extras` checked: no
`.ansify-orig`/scratch-diagnostic stray files, only expected build
byproducts (`Makefile`, `*.o`, `*.a`, `install.time`/`install.doc`,
generated `.do`/`.eh`/`.ih`, and pre-existing untracked build/test
artifacts already present before this session). No commit made.

**This session completes Wave 1 of the M3 ANSI-C conversion milestone
(O1, O2, O3, O4 all done, all committed except this one, which is
awaiting wdc's review at this gate).** wdc may choose to run a
milestone tree-wide gate separately as a top-level decision — not
triggered by this session, per the prompt.

## 10. Runtime consumers and exact check commands for wdc

Direct `nm`/`ls`/any Bash access to
`build/bin/{runapp,ez,messages,messagesn,cui,cuin}` is denied by the
harness outright (same restriction O3's session hit) — `nm -g` on
library archives (`build/lib/*.a`) and dynamic objects
(`build/dlib/atk/*.do`) works fine and was used throughout, backed by
Imakefile `LIBS`/`ProgramTarget` lines for the rest.

- **`overhead/class/lib` + `overhead/class/machdep/darwin`
  (`libclass.a`)** — the foundational Class dispatch runtime, built
  from both directories together (`class.o`/`mapping.o`/`classind.o`
  from `class/lib`, `doload.o`/`classproc.o` from `class/machdep/
  darwin` via the `machine -> darwin` symlink). Referenced by 8
  Imakefiles tree-wide; confirmed via `nm -g build/lib/libclass.a`
  (`_class_EnterInfo`, `_doload` both present) and `nm -g
  build/dlib/atk/amsn.do` (`_class_EnterInfo`/`_doload` defined
  directly in `amsn.do`'s own symbol table). This is the
  highest-consequence pair in the batch — every AUIS class-based
  binary depends on it.
  ```
  DISPLAY=:0; ez
  ```
  Confirm the window opens and any inset (menu, scrollbar) responds —
  exercises class dispatch/dynamic loading on every call.
- **`overhead/cmenu` (`libcmenu.a`, `libshadows.a`)** — referenced by
  `atk/basics/x`, `atk/apps`, `atk/basics/common` (shadows only, for
  3D button/shadow rendering), plus `atk/createinset/null`,
  `atk/ness/objects`. `menubar.c`'s `shadows_ComputeColor` call (the
  load-bearing check for this session's `shadows.h` fix) is exercised
  by any window's popup/context menu rendering with 3D shading.
  ```
  DISPLAY=:0; ez
  ```
  Open a popup menu (right-click / context menu in a window) and
  confirm menu panes render with correct shadow/3D shading, no crash.
- **`overhead/mail/metamail/{metamail,richmail}`** — standalone MIME
  helper binaries (`metamail`, `mmencode`, `mailto`, `splitmail`,
  `richtext`, `richtoatk`), invoked externally (fork/exec) by
  `messages`: confirmed via `grep -rl metamail
  atkams/messages/lib/mailobj.c`. Load-bearing check for this
  session's `mailto.c`/`PutQP` fix (quoted-printable body encoding):
  ```
  DISPLAY=:0; messages
  ```
  Open a MIME-multipart message with a non-plain-text part (HTML,
  image, or an 8-bit-body message) to trigger the external metamail
  path. Confirm no crash and correct rendering/decoding.
- **`overhead/index` (`libindex.a`)** — referenced by `atk/help/src`,
  `atk/help/maint`. Load-bearing check for the `index_Hash` fix:
  ```
  DISPLAY=:0; help
  ```
  Open the help browser and use its search/index feature (exercises
  `index_Hash`/hash-table lookups over the help index).
- **`overhead/rxp` (`librxp.a`)** — linked into `ams/ms`,
  `ams/msclients/{cui,vui,nns,imapsync}`, `atkams/messages/lib`,
  `atk/help/src`, `overhead/eli/bglisp` (the same consumer set O3
  identified for the `regcomp`/`regexec` finding). Confirmed via `nm
  -g build/dlib/atk/amsn.do`: `_reg_comp`/`_reg_exec` both defined
  directly. Ordinary mail delivery/filing already exercises the regex
  engine indirectly (address/header pattern matching); no dedicated
  UI action isolates it further.
  ```
  DISPLAY=:0; messages
  ```
  Open and read mail normally; confirm no crash.
- **`overhead/class/testing`** — standalone dynamic-loader test
  fixtures (`testobj.do`/`testobj2.do`), no other Imakefile references
  `testobj`/`testobject` anywhere in the tree (confirmed via grep).
  Gate-only, no runtime check applicable — matches the pattern
  `m3-batches.md` already flagged for `atk/srctext`/`dired`/`chlist`
  in M1.
- **`overhead/malloc`, `inst`** — entire directories inert in this
  build (§6). Gate-only, no runtime check possible or applicable.
- **`overhead/fonts/cmd` (`fdbbdf` only — `fdbwm`/`wmfdb` inert)** —
  `fdbbdf` is a standalone bitmap-font-to-BDF converter, not invoked by
  any GUI app. Gate-only.
- **`overhead/util/cmd` (`pref` only — `newbt` inert)** — standalone
  profile/preference CLI utility. Gate-only.
- **`overhead/sys`, `overhead/mkparser`, `overhead/util/hdrs`,
  `overhead/errors` (`liberrors.a`), `overhead/addalias`,
  `ossupport` (`libatkos.a`)** — `liberrors.a` fans out widely (28+
  Imakefiles), but is a thin printf-wrapper library with no
  distinguishing UI action beyond "the app didn't crash printing an
  error" — covered by the general `ez`/`messages` checks above.
  `libatkos.a`/`sys`/`mkparser`/`genhdr`/`addalias` are build-time
  tools or thin OS-shim libraries with no GUI-visible behavior of
  their own — gate-only for all of these, matching the pattern already
  established for `overhead/bison` in O3.
- **`overhead/class/cmd` (`doindex`/`dotest`/`whichdo`)** — build-time
  class-system maintenance tools (dynamic-object indexer, class
  loader tester, `.do` file locator), not reached from any GUI app at
  runtime. Gate-only.

## 11. Open questions / anything that surprised you

- The `overhead/class/testing` `-pe` rollout is a judgment call per
  the prompt's own framing — flagging for wdc's confirmation, and
  noting `m3-batches.md`'s Wave 4 "eq pilot" framing is now stale if
  wdc agrees the mechanic is proven live by this batch.
- §3's new fallout pattern (truly-empty-parens lifecycle-method
  definitions, invisible to `ansify`'s candidate detector, conflicting
  with a `-pe`-regenerated `.eh`) is the one genuinely new taxonomy
  entry from this session — worth folding into
  `m3-rollout-runbook.md`'s findings section before the next
  `-pe`-flagged directory (Wave 4's `atk/eq`, or any class directory
  in Waves 2/3) in case it recurs.
- `overhead/malloc`'s `site.h`-overrides-`allsys.h` wrinkle
  (`ANDREW_MALLOC_ENV` defined in one, undefined in the other) is a
  second real-world confirmation of the exact mechanism
  `rollout-procedure.md`'s Liveness census section already warns
  about from M1 batch 7 — worth citing this batch as a second data
  point if that section ever gets revised.
- `overhead/cmenu/cmenu.h`'s `_STDC_` (vs. standard `__STDC__`) macro
  typo is a genuine ~35-year-old dead-branch bug with zero observable
  effect in this build — noted for awareness, not fixed, since no
  functional change is on the table and the blast radius of touching
  a widely-`#include`d header for zero benefit felt like unwarranted
  risk. Happy to fix it in a follow-up if wdc wants it addressed.
- I did not run any AUIS GUI or terminal binary interactively, and
  made no commits, per the prompt. The one destructive-adjacent action
  taken outside the 19 directories was building (not editing)
  `atk/basics/common/graphic.o` and `atk/basics/x/menubar.o` to verify
  the `shadows.h` fix's blast radius — object files only, no source
  changes, and both directories' own Makefiles will rebuild them
  fresh, cleanly, whenever their own M3 batch runs.

## 12. Addendum (applied at orchestrator review, 2026-07-26, wdc's request)

Per wdc's explicit instruction after reviewing this report: fixed the
`overhead/cmenu/cmenu.h` `_STDC_`/`__STDC__` macro typo (§6's
"genuine ~35-year-old dead-branch bug... noted for awareness, not
fixed") for real, and documented it in `revival.md`'s "Old bugs never
found till now."

Flipping the guard to `__STDC__` made the header's previously-dead
typed branch live for the first time, which surfaced a **second**,
independent latent bug in that same branch: `cmenu_Destroy`'s
declaration (`extern cmenu_Destroy(struct cmenu *menu);`) has no
return type (defaults to `int`), but the real definition
(`cmmanip.c:203`, already ANSI from this same batch's conversion) is
`void cmenu_Destroy(struct cmenu *menu)` — a conflicting-types compile
error the moment the branch went live. Fixed by adding `void` to the
declaration.

Rebuilding `overhead/cmenu` clean twice with both fixes: green,
deterministic. Checked all 5 tree-wide consumers of `cmenu.h`
(`atk/basics/x/{xfontd,xws,xinit,xim}.c`, `atk/apps/statload.c` — via
`grep -rl '"cmenu.h"'`) by direct per-file `make <name>.o`: four
compiled clean immediately. `atk/basics/x/xim.c` did not, for two
reasons, both now fixed:

- `xim.c` carried its own local workaround declarations
  (`extern int cmenu_Destroy(); extern int cmenu_Activate();`, with a
  comment explicitly citing the `_STDC_` bug as the reason) — now
  redundant and, worse, conflicting with the header's now-live typed
  versions (wrong return type for `Destroy`, again). Removed both
  lines; the header now supplies correct declarations directly.
- `xim.c:1917`'s call to `cmenu_AddSelection` passed a `struct
  seldata *` where the now-visible typed prototype expects `long`
  (an intentional "stuff a pointer in a generic data slot" idiom
  already used elsewhere in this same file — see the `(struct
  seldata *)idata` cast at `xim.c:2143`, the round-trip back). This
  was always exactly what happened at runtime (long and pointer are
  both 8 bytes on this LP64/arm64 target, so no truncation, ever) —
  the untyped K&R declaration just never let the compiler notice the
  implicit conversion. Added the missing explicit cast,
  `(long)selectionData`, at the one call site; no behavior change,
  just making an always-safe-on-this-platform conversion explicit.

Rebuilt `xim.o` clean after both fixes; rebuilt `overhead/cmenu` clean
twice more (deterministic) and reconfirmed all 5 consumers compile via
direct per-file `make <name>.o` (a full `atk/basics/x` directory
`install` was not attempted — its `.do` link step depends on
`atk/basics/common/{region,rect}.o`, which don't exist in this tree
yet because that directory, Wave 2's `B1`, hasn't been built this
session; unrelated to this fix and out of scope here).

Also ran two direct terminal tests of the batch's own
`overhead/mail/metamail/metamail` binary (`build/bin/metamail`, built
by this batch) at wdc's request, investigating why `messages` no
longer visibly calls out to metamail for ordinary multipart mail — a
single-part `text/plain` message and a two-part `multipart/mixed`
message, both via `timeout 10 ... </dev/null`, non-interactive. Both
decoded and printed correctly, exit 0 — confirming this batch's
`PutQP`/`mailto.c` and `splitmail.c` fallout fixes didn't break the
binary itself. Root cause of the "no callout" observation (not a
regression, not this batch's doing): `atkams/messages/lib/text822.c`'s
multipart/alternative handling now picks a winning part and renders
`text/plain`/`text/html` inline natively, and only falls back to the
clickable `mailobj` object (whose `RunMetamail` is triggered by an
explicit user click, `mailobjv.c:144`) for a part type neither
built-in path can render — an intentional, already-shipped mime-display
feature (`mime-display-REPORT.md`/`mime-display-prompt.md`), predating
this session, superseding the old one-metamail-button-per-alternative
UI. Ordinary text-only multipart mail simply never reaches the
metamail path anymore; this is expected, not a defect.

`fossil status` after this addendum: 49 files changed total (the
original 47 plus `overhead/cmenu/cmenu.h` and `atk/basics/x/xim.c`),
confirmed via `fossil diff`.
