# classpp fix: `FinalizeObject` empty-parens `-pe` self-inconsistency

## 1. Status

Stopped at the gate. Fix applied to `overhead/class/pp/class.c`,
`class` binary rebuilt, both verification passes complete, gate green
for all 7 already-`-pe`'d directories (twice each) plus
`overhead/class/pp` itself (twice) — 16 gate cycles total, zero real
`error:` lines anywhere. No commit made.

Working tree: 1 file edited — `src/overhead/class/pp/class.c`. Session
diff at tree root: `m3-classpp-finalizeobject-fix-session.diff` (24
lines). This report lives in `revival/doc/claude-history/` per the
standard convention.

## 2. What I did, in order

1. Read `sonnet-playbook.md` and `rollout-procedure.md` in full, then
   `m3-classpp-finalizeobject-fix-prompt.md`.
2. Located the exact code in `src/overhead/class/pp/class.c`:
   - `InitializeObject`'s hardcoded 2-arg export prototype: lines
     1115-1122 (emitted once, unconditionally, regardless of the
     `.ch`).
   - The classproc-emission loop with the "skip `InitializeObject`"
     comment/check: lines 1135-1155 (fix target at 1146-1151).
   - The internal `__Finalize` call site: line 1335,
     `"    %s__FinalizeObject(classID, self);\n"` — unconditionally
     2-arg.
   - `mp->realargtypes`'s assignment at parse time: line 2167,
     `CurrentMethod->realargtypes = (argcount) ? realargstr : "";` —
     confirmed empty string (not NULL) is this codebase's actual
     convention for "0 declared args," matching the prompt's
     expectation.
   All line numbers matched the prompt's estimates almost exactly
   (off by at most a few lines from drift since the prompt was
   written); no surprises in the diagnosis.
3. Applied the fix exactly as specified — see full diff in §3.
4. Rebuilt `class` via `make -C .../overhead/class/pp clean`, `depend`,
   `-k install`. Clean build (only the pre-existing tree-wide
   `-Wdeprecated-non-prototype` K&R warnings, no errors).
5. Verification part 1 (byte-identical regen) — §4.
6. Verification part 2 (positive test) — §5.
7. Gate: 7 directories × 2 cycles + `overhead/class/pp` × 2 cycles = 16
   gate cycles, all green — §6.
8. Confirmed `fossil status` shows only `class.c` edited, no commit
   made — §7.

## 3. The exact diff to `class.c`

```diff
Index: src/overhead/class/pp/class.c
==================================================================
--- src/overhead/class/pp/class.c
+++ src/overhead/class/pp/class.c
@@ -1142,11 +1142,17 @@
 	     * go through the loop to pick up the correct mp->methodtype. */
 	    if (strcmp(mp->name, "InitializeObject") == 0)
 		continue;
 
 	    if (usePrototypesExport) {
-		sprintf(proto, "struct classheader *%s", mp->realargtypes);
+		if (strcmp(mp->name, "FinalizeObject") == 0
+		    && (mp->realargtypes == NULL || mp->realargtypes[0] == '\0')) {
+		    sprintf(proto, "struct classheader *, struct %s *", FinalClassName);
+		}
+		else {
+		    sprintf(proto, "struct classheader *%s", mp->realargtypes);
+		}
 	    }
 	    else {
 		proto[0] = '\0';
 	    }
```

The `InitializeObject` skip line (line 1143) was left untouched, as
specified — `FinalizeObject` still goes through this loop for every
class, including the 7 `atk/value` overrides with real non-`void`
return types.

## 4. Verification part 1 — byte-identical `.eh` regeneration

Re-grepped all 7 directories for literal `FinalizeObject()` (empty
parens) in their `.ch` files first, to confirm the prompt's "zero
remaining instances" claim was still accurate: confirmed, zero hits.
(One live example of the empty-parens shape does exist elsewhere in
the tree, outside these 7 directories and not yet `-pe`'d:
`atk/org/orga.ch` line 72 — used as the model for the positive test in
§5, not touched.)

Procedure per directory: snapshot the currently-generated `.eh` files
to a scratch location (plain `cp`, not `fossil`, since these are
untracked build artifacts) → `rm` the `.eh` files → force-regenerate
every one with the newly-rebuilt `class` binary (`make -C <dir>
<name1>.eh <name2>.eh ...`, full explicit target list per directory,
no wildcard) → per-file `diff -q` against the snapshot.

| Directory | .ch/.eh count | Diffs found |
|---|---|---|
| `overhead/class/testing` | 2 | 0 |
| `atk/basics/common` | 41 | 0 |
| `atk/value` | 27 | 0 |
| `atk/support` | 19 | 0 |
| `atk/supportviews` | 17 | 0 |
| `atk/adew` | 11 | 0 |
| `atk/basics/x` | 7 | 0 |
| **Total** | **124** | **0** |

All 124 files byte-identical before/after the fix, confirmed via
individual `diff -q` per file (not just a directory-level `diff -rq`,
which BSD `diff` on this machine doesn't support the same way as
GNU). Zero retroactive effect proven — the fix's value is entirely
prospective. The `atk/value` result is the one that mattered most to
verify carefully: this directory holds the 7 classes
(`menttext.ch`, `entrtext.ch`, `clklistv.ch`, `entrintv.ch`,
`entrint.ch`, `mentstrv.ch`, `entrstrv.ch`) that already restate
`self` with a real non-`void` `FinalizeObject` return type — these are
untouched by the new `strcmp`/empty-check branch (they take the
`else` arm, exactly as before), and the diff confirms it.

## 5. Verification part 2 — positive test (toy class)

Built a standalone scratch `.ch` (in the scratchpad, never in the
source tree), modeled on the real empty-parens shape found in
`atk/org/orga.ch`:

```c
/* scratch positive-test class for the classpp FinalizeObject -pe fix */

class toyobject[toyobj] {

classprocedures:
    InitializeObject() returns boolean;
    FinalizeObject();

data:
    int dummy;

};

/* end of toyobj.ch */
```

To get a real "before" data point (not just recall the diagnosis), I
temporarily reverted `class.c` to the pre-fix state in place, rebuilt
`class`, ran `class -s -pe` on the toy `.ch`, saved the `.eh` output,
then restored the fix, rebuilt again, and re-ran. `class.c` was
confirmed byte-identical to the fixed version after restoring (`diff`
exit 0) before any gate cycle ran, so no gate result below reflects
the temporarily-reverted state.

**Before the fix** (`toyobject__FinalizeObject`, relevant lines):
```c
void toyobject__FinalizeObject(struct classheader *);      /* line 152: 1-arg prototype */
...
    toyobject__FinalizeObject(classID, self);               /* line 216: 2-arg call site */
```
Compiling this `.eh` standalone with `cc -c -std=gnu89 ...`:
```
./toyobj.eh:216:40: error: too many arguments to function call, expected 1, have 2
  216 |     toyobject__FinalizeObject(classID, self);
      |     ~~~~~~~~~~~~~~~~~~~~~~~~~          ^~~~
./toyobj.eh:152:6: note: 'toyobject__FinalizeObject' declared here
  152 | void toyobject__FinalizeObject(struct classheader *);
      |      ^                         ~~~~~~~~~~~~~~~~~~~~
```
Exactly the diagnosed failure, reproduced from a clean toy class, not
just asserted.

**After the fix** (same toy `.ch`, rebuilt `class` binary):
```c
void toyobject__FinalizeObject(struct classheader *, struct toyobject *);  /* line 152: 2-arg prototype */
...
    toyobject__FinalizeObject(classID, self);                              /* line 216: 2-arg call site */
```
Prototype and call site now agree. Compiling the same way:
```
./toyobj.eh:225:19: warning: a function definition without a prototype is deprecated ...
./toyobj.eh:241:19: warning: a function definition without a prototype is deprecated ...
2 warnings generated.
```
Compiles clean — only the two pre-existing, unrelated K&R
`GetClassInfo` deprecation warnings that appear in every `-pe`'d `.eh`
in this codebase, no errors.

Scratch files (`toyobj.ch`, `toyobj.eh`, `toyobj.ih`, `.o`, the
before/after snapshots, and the temporary reverted-binary artifacts)
were all deleted from the scratchpad after the test; none were ever
in the source tree.

## 6. Gate results

Procedure per directory, twice: `make -C <dir> clean`, then `make -C
<dir> depend`, then `make -C <dir> -k install`, each a separate Bash
call, absolute paths, no chaining. Success measured by zero `error:`
lines in the `-k install` output (never exit code), per
`rollout-procedure.md`.

| Directory | Cycle 1 | Cycle 2 |
|---|---|---|
| `overhead/class/testing` | green | green |
| `atk/basics/common` | green | green |
| `atk/value` | green | green |
| `atk/support` | green | green |
| `atk/supportviews` | green | green |
| `atk/adew` | green | green |
| `atk/basics/x` | green | green |
| `overhead/class/pp` | green | green |

16/16 gate cycles green, zero real `error:` lines in any of them.
`atk/basics/x`'s `depend` step emitted 3 `makedepend` warnings about
`#error` directives inside `freetype2/freetype/config/integer-types.h`
— this is `makedepend`'s parser choking on FreeType's own
preprocessor guards while building the dependency graph, not a real
compile error; pre-existing and unrelated to this change (it appeared
identically in both cycles, and the actual `-k install` compile step
that followed was clean). `atk/support`'s `-k install` produced one
`WARNING:bufferlist.ch: - long class names should use name keys` from
`classpp` itself — also pre-existing, unrelated, a `classpp` style
warning not an error.

## 7. `fossil status`

Before and after all work, only:
```
EDITED     src/overhead/class/pp/class.c
```
No other file changed. The rebuilt `class` binary
(`build/bin/class`, and `src/overhead/class/pp/class`,
`class.o`, `classpp.o`, `classpp.c`, `classpp.l`) are build artifacts,
none fossil-tracked — confirmed via `fossil status` not listing them.
No commit made.

## 8. Files touched

- `src/overhead/class/pp/class.c` — the fix (§3). Compiles clean
  (verified via the `overhead/class/pp` gate, §6, twice).

## 9. Anything that surprised me / didn't match the prompt

- Nothing substantive. Line numbers in the live file differed by a
  handful of lines from the prompt's `~1121-1122`/`~1146-1151`/`~1334`
  estimates (actual: 1115-1122/1146-1151/1335) — normal drift, not a
  discrepancy in the diagnosis itself; the code shape at each location
  matched the prompt's description exactly, including the explanatory
  comment at ~1139-1142 about `FinalizeObject`'s non-`void` return
  type, which was already present verbatim.
- One additional live empty-parens `FinalizeObject()` instance exists
  in the tree beyond the two historical ones the prompt already knew
  about (`atk/org/orga.ch` line 72) — not in any of the 7 already-`-pe`'d
  directories, so it doesn't affect the "zero retroactive effect"
  claim, and `atk/org` isn't `-pe`'d yet so it isn't live fallout
  either. Worth knowing for whichever future wave flags `atk/org`:
  this fix will make that class's `-pe` output self-consistent for
  free when that day comes. Used as the real-world model for the toy
  positive-test class since it's the same convention.
- BSD `diff` (this machine's default) doesn't support `--include` for
  directory diffs the way GNU `diff` does; switched to a per-file
  `diff -q` loop instead of a single `diff -rq`, which turned out to
  be more explicit/auditable anyway (per-file confirmation, not just a
  directory-level summary).
