# M2 pilot: `atk/eq` — `-Werror=implicit-function-declaration`

## 1. Status

Stopped at Gate 1 (the prompt's only gate). No commits made. Working
tree left as-is (5 files edited: `Imakefile` + 4 `.c` files under
`src/atk/eq`). Session diff at tree root: `m2-pilot-eq-session.diff`.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `m2-rollout-runbook.md`, `m2-pilot-eq-prompt.md` in full.

2. Flagged `src/atk/eq/Imakefile` with the runbook's exact
   `COMPILERFLAGS` override (restating the full four-flag line, per
   the runbook's "mechanical difference from M1" warning):
   ```
   /* M2 ANSI sweep: catch implicit function declarations (revival/doc/roadmap.md, M2) */
   COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
   ```

3. Regenerated the Makefile: `make Makefile` (imake). Verified with
   `grep -n COMPILERFLAGS Makefile`:
   ```
   177:COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration ...   (system.mcr default)
   291:COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration ...  (our override, last — wins)
   ```
   Re-verified this after the tree-wide gate regenerated the Makefile
   again — override still wins.

4. `make clean && make -k install` in `src/atk/eq`. First attempt hit
   a **build-order problem unrelated to M2** (see Finding A below):
   `symbols.c:47:10: fatal error: 'eqparse.h' file not found`. Worked
   around it with `make depend` (regenerates the bison-derived
   `eqparse.h`/`eqparse.c`), then re-ran `make -k install`, which
   surfaced all 10 real implicit-function-declaration errors in one
   pass:
   ```
   eq.c:453:8: error: implicitly declaring library function 'strlen' ...
   eq.c:595:6: error: implicitly declaring library function 'strncmp' ...
   eq.c:597:10: error: implicitly declaring library function 'strcmp' ...
   symbols.c:324:29: error: implicitly declaring library function 'strcmp' ...
   eqv.c:168:2: error: implicit declaration of function 'eqview_Format' ...
   eqv.c:315:5: error: implicit declaration of function 'eqview_Format' ...
   eqv.c:344:9: error: implicitly declaring library function 'strcmp' ...
   eqvcmds.c:915:7: error: implicitly declaring library function 'strncmp' ...
   eqvcmds.c:1018:9: error: implicitly declaring library function 'strcmp' ...
   eqvcmds.c:1058:9: error: implicitly declaring library function 'strlen' ...
   ```
   Exactly 10, matching the census. All logged in
   `~/src/AUIS/andrew-6.4/dependInstall.log` (canonical path, per
   `rollout-procedure.md`'s Logging section — overwritten at each
   rebuild in this session).

5. Fixed fallout (see §3 for per-instance detail):
   - `eq.c`, `symbols.c`, `eqvcmds.c`: added `#include <string.h>`.
   - `eqv.c`: added `#include <string.h>` plus a file-scope forward
     declaration for `eqview_Format` (defined in `draweqv.c`, no
     header declares it anywhere in the tree).

6. Rebuilt: `make clean; make depend; make -k install` in
   `src/atk/eq` → exit 0, zero `error:` lines, `eq.do`/`eqv.do`
   installed to `build/dlib/atk`, `eq` symlink relinked in
   `build/bin`. Repeated the clean/depend/install cycle a second time
   to confirm determinism — identical clean result both times.

7. Ran the full tree-wide gate as directed (safer option, first-ever
   M2 session): `cd src && make Clean` (exit 0), then `make
   dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1`
   (background, ~50 min real time watched via process liveness, not
   polled). Confirmed completion via `pgrep -f 'make dependInstall'`
   returning empty and the background-task completion notification
   (exit code 0).

8. Verified the gate log:
   ```
   $ grep -c "error:" dependInstall.log
   4
   $ grep -n "error:" dependInstall.log
   109395:  197 |    ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
   172658:clang: error: linker command failed with exit code 1 (use -v to see invocation)
   223020:ltapp.c:115:27: error: incompatible integer to pointer conversion ...
   223032:ltapp.c:123:32: error: incompatible integer to pointer conversion ...
   ```
   All 4 are pre-existing baseline, unrelated to `atk/eq`: the known
   false positive (`Internal error: unknown recognizer type` inside a
   `-Wdeprecated-non-prototype` warning), the `ams/msclients/nns`
   SSLLIB link failure, and `contrib/zip/utility/ltapp.c`'s two
   pre-existing int-conversion errors. Confirmed `atk/eq` itself
   descended and built with zero warnings/errors attributable to it:
   ```
   $ grep -n "building (dependInstall) (.*atk/eq)" dependInstall.log
   128406:building (dependInstall) (/Users/wdc/src/AUIS/andrew-6.4/src/atk/eq)
   ```
   No `error:` or new-warning lines appear between that line and the
   next directory's descent line. Gate is green.

9. `fossil status` confirms exactly the 5 files touched (Imakefile +
   4 `.c` files; the regenerated `Makefile` is not fossil-tracked so
   doesn't appear):
   ```
   EDITED     src/atk/eq/Imakefile
   EDITED     src/atk/eq/eq.c
   EDITED     src/atk/eq/eqv.c
   EDITED     src/atk/eq/eqvcmds.c
   EDITED     src/atk/eq/symbols.c
   ```
   `fossil diff > m2-pilot-eq-session.diff` written to the tree root.
   No commit made.

## 3. Per-instance findings (the 10 census hits)

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `eq.c:453` | `strlen` | Missing standard-library header |
| 2 | `eq.c:595` | `strncmp` | Missing standard-library header |
| 3 | `eq.c:597` | `strcmp` | Missing standard-library header |
| 4 | `symbols.c:324` | `strcmp` | Missing standard-library header (masked at first by Finding A below) |
| 5 | `eqv.c:168` | `eqview_Format` | Same-directory forward reference |
| 6 | `eqv.c:315` | `eqview_Format` | Same-directory forward reference |
| 7 | `eqv.c:344` | `strcmp` | Missing standard-library header |
| 8 | `eqvcmds.c:915` | `strncmp` | Missing standard-library header |
| 9 | `eqvcmds.c:1018` | `strcmp` | Missing standard-library header |
| 10 | `eqvcmds.c:1058` | `strlen` | Missing standard-library header |

8/10 are plain `<string.h>` misses (category 1, exactly as the
runbook predicted — zero semantic risk, matched what the sibling
`atkams/messages/lib`-era file `msjournal.c` already does: `string.h`
grouped with the other system headers at the end of the include
block).

2/10 are `eqview_Format`, called from `eqv.c` (lines 168 and 315) but
**defined in `draweqv.c`** (line 788), a sibling file in the same
directory, with no header declaring it anywhere in the tree
(`eqv.ch`, `eqv.ih`, `eqv.eh` all silent on it — it is not a Class
method, just a plain C helper that happens to take `self` as its
first argument). This is exactly the shape the runbook's own taxonomy
text names by example (`m2-rollout-runbook.md`'s "Same-file/
same-directory forward reference" bullet literally cites this
function). Confirmed the real signature from the definition:
```
struct formula *eqview_Format(self, eqptr, leftf, f, rightf, eqstyle)
struct eqview *self;
struct eq *eqptr;
struct formula *leftf, *f, *rightf;
enum eqstyle eqstyle;
```
The only existing forward-reference precedent for this function
anywhere in the tree is inside `draweqv.c` itself, at its own call
site in `eqview_FormatGroup` (line 615): a local, K&R-style, **untyped**
declaration —`struct formula *eqview_Format();` — placed in the
calling function's body just before use, ahead of the real
definition later in the same file. `eqv.c` had no pre-existing
forward-declaration convention of its own (grepped, no matches). I
added a file-scope declaration in `eqv.c`, matching that same
untyped K&R style (return type only, no parameter list), since that
is the only precedent found and it directly closes the real bug
(implicit `int` return silently truncating the returned pointer on
LP64 — exactly the class M2 exists to close) without inventing typed
parameter positions the directory has never written down anywhere.

## 4. Finding A — a runbook gap, not an M2 fallout category

`m2-rollout-runbook.md` step 3 prescribes `make clean && make -k
install` as the per-directory rebuild. For `atk/eq` this is
**incomplete**: `eqparse.h`/`eqparse.c` are bison-generated
(`Parser(eqparse,-d)` in the Imakefile) and wired into the Makefile
only under the `depend:: eqparse.h eqparse.c` target — **not** as a
prerequisite of `all`/`install`/`symbols.o`. `make clean` deletes
them; a bare `make -k install` afterward never regenerates them, so
`symbols.c`'s `#include <eqparse.h>` fails with a *fatal* error
before the compiler ever reaches the real
`-Wimplicit-function-declaration` diagnostic at line 324 — silently
hiding one of the 10 census instances behind an unrelated failure.

This isn't new: the top-level `src/Makefile` / `imake.tmpl` wire
`dependInstall:: depend` (confirmed: `src/config/imake.tmpl:198`), so
the tree-wide gate (`make dependInstall`, which is what actually ran
for every directory including `atk/eq`, confirmed by the
`building (dependInstall)` census line) never hits this gap — `depend`
always runs before `install` in that path. It's specifically the
runbook's *subtree-local* recipe (`make clean && make -k install`,
skipping `depend`) that's exposed to it, and only in directories with
generated sources (`Parser()`/bison, and presumably any directory
using `LexFile`/similar). Worked around here with an explicit `make
depend` between `clean` and `install`. **Recommend**: revise
`m2-rollout-runbook.md`/`rollout-procedure.md`'s step 3 to `make
clean && make depend && make -k install` (or equivalently `make -k
dependInstall` for the subtree), so a future directory with generated
sources doesn't lose a census instance the same way.

## 5. Subtree-local gate vs. tree-wide gate

With the corrected recipe (`make depend` before `install`), the
subtree-local build in `src/atk/eq` alone surfaced **all 10** real
implicit-function-declaration errors — nothing was left for the
tree-wide pass to find. The tree-wide gate's only value here was
confirming the *absence* of cross-directory fallout: zero new
`error:` lines anywhere else in the tree (4 total, all pre-existing
baseline, none touching `atk/eq` or anything that includes it). This
is the first real data point for the "is a lighter M2 gate
sufficient" question the runbook leaves unruled: for this pilot, yes
— a subtree-local `make -k install` (with `depend` included) would
have been sufficient on its own, matching the runbook's own
prediction that M2's flag doesn't cross directories the way M1's
typed casts did. One data point, one small/leaf directory — I'd want
at least one more sample (ideally one that exercises a directory
whose objects get relinked elsewhere, per the runbook's own hedge)
before ruling this settled tree-wide.

## 6. Taxonomy corrections/gaps

- The two population categories in the runbook (missing standard
  header; same-directory forward reference) were both hit exactly as
  predicted, with real counts matching the census exactly once the
  build-order gap above was worked around. No new fallout category
  appeared.
- The "missing in-tree/project header" and "possible genuine bug"
  categories were not exercised by this pilot (all 4 files) — no
  data either way on their fixes.
- The gap in §4 above (generated-source directories need `depend`
  before the subtree-local `install`) is a **procedure/mechanics**
  finding, not a fallout-fixing taxonomy category — it belongs in
  `rollout-procedure.md`'s step-order text or `m2-rollout-runbook.md`,
  not in the fallout taxonomy itself.

## 7. Is `eq`'s forward-declaration convention reusable?

Treat it as a one-off, not a directory-wide pattern to expect
elsewhere. The only precedent found anywhere in `atk/eq` (or grepped
tree-wide) for "function used across files in one directory with no
header" is this single case (`eqview_Format`), and the style used
(untyped K&R forward declaration, no parameter list) is simply "the
minimal declaration that satisfies the compiler without inventing
information nobody wrote down" — not evidence of a directory-specific
house style other M2 directories should expect to find. Later
directories hitting this taxonomy category should still search their
own directory/tree for whatever local precedent (if any) exists per
the runbook's existing guidance, and treat "no precedent found" as
its own finding rather than assuming this session's style transfers.

## 8. Files touched (compile status)

- `src/atk/eq/Imakefile` — added `COMPILERFLAGS` override. Verified
  via `make Makefile` + `grep -n COMPILERFLAGS Makefile` (override
  line wins, both after the initial regen and after the tree-wide
  gate's regen).
- `src/atk/eq/eq.c` — added `#include <string.h>`. Compiles clean (0
  errors; pre-existing `-Wdeprecated-non-prototype` warnings remain,
  unrelated to M2).
- `src/atk/eq/symbols.c` — added `#include <string.h>`. Compiles
  clean.
- `src/atk/eq/eqv.c` — added `#include <string.h>` and a file-scope
  forward declaration for `eqview_Format`. Compiles clean.
- `src/atk/eq/eqvcmds.c` — added `#include <string.h>`. Compiles
  clean.
- `src/atk/eq/Makefile` — regenerated (not fossil-tracked, not a
  source edit).

All four `.c` files, plus the directory as a whole, build with exit 0
and zero `-Werror=implicit-function-declaration` diagnostics; `eq.do`
and `eqv.do` installed successfully to `build/dlib/atk`, `eq` symlink
relinked in `build/bin`. Tree-wide gate (`make Clean; make
dependInstall`) is green: 4 pre-existing baseline errors, 0 new.

## 9. Open questions / surprises

- The `depend`-before-`install` gap (§4) surprised me — it's a
  latent fragility in `atk/eq`'s own Makefile (any Parser()-using
  directory shares it) that has nothing to do with M2, but the M2
  rollout procedure's subtree-local recipe is exactly the kind of
  command that walks into it. Worth deciding whether to fix it as a
  standing Imakefile issue (wire `eqparse.h`/`eqparse.c` as a real
  prerequisite of `symbols.o`/`all`, not just `depend`) versus just
  documenting the workaround — I did not touch the dependency graph
  itself, only worked around it via an extra `make depend` call, per
  the M2 hard-stop rule against editing anything beyond the fallout
  fix itself without escalating first.
- Only one small pilot directory's worth of evidence exists so far
  for "subtree-local gate is sufficient" (§5) — I'd treat that as a
  lean, not a ruling, until a second directory (ideally one with more
  external consumers) confirms it.
- I did not run the AUIS GUI and made no commits, per the prompt.
