# `ansify` brace-glued-parameter fix + retrospective recheck across 7 M3 directories

## 1. Status

Stopped at the prompt's single gate: fix applied, unit-verified, all 7
directories re-run and gated **twice each**, all green. **No fossil
commit made.** `fossil diff > m3-ansify-brace-glued-fix-session.diff`
written to the tree root; this report to
`revival/doc/claude-history/`.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`, and this task's
   own prompt in full. Read `m3-rollout-runbook.md`'s B1 and B2 entries
   (searched both for "brace-glued") for full background before
   touching anything — `porting-assessment.md` itself has no
   "brace-glued" hit (the pattern lives entirely in the runbook's B1/B2
   findings, not in the assessment doc).
2. Confirmed starting state: `fossil status` clean, checkout at
   `b4e33020d1d3` (matches the runbook's latest committed state,
   `classpp FinalizeObject fix writeup...`).
3. Applied the fix exactly as specified in the prompt — added
   `BRACE_GLUED` regex and the new branch in `parse_decl_block`, in the
   two locations the prompt named (~line 199, ~line 216).
4. **Confirmed the fix on scratch tests before touching the real tree**
   (§3 below) — both the prompt's own stated unit-test cases and a
   full `convert_file` end-to-end pass on a synthetic brace-glued
   helper function.
5. Ran the 7-directory retrospective recheck in the prescribed order
   (§4 below), each directory: dry-run → investigate every DRIFT/skip
   → real run → investigate any compile failure → gate twice.
6. **Mid-session, found and fixed a second, previously-undiscovered bug**
   in the fix itself, surfaced by `atk/support`'s `mark.c` (§5) — a
   destructive-mutation-on-skip defect that the prompt's own unit tests
   didn't cover (they only tested the always-accepted path, not
   accepted-by-parser-then-rejected-by-DB-lookup). Fixed, re-verified
   with corrected scratch tests plus a new reproduction test, then
   confirmed the 2 directories already completed with the buggy version
   (`atk/basics/common`, `atk/value`) were unaffected (their skips were
   never brace-glued-in-a-changed-file, so the corruption path was never
   live for them) via a fresh dry-run re-check with the corrected tool.
7. Completed the remaining 5 directories with the corrected tool.
8. Final `fossil status`/`fossil extras` sanity check (§7) — exactly the
   expected 20 source files changed across the 7 directories, plus the
   tool itself, no stray backups, no unexpected extras.

## 3. The fix, and my own scratch confirmation before touching the real tree

### 3a. Exact diff to `revival/tools/ansify` (final, corrected version)

```diff
--- a/revival/tools/ansify
+++ b/revival/tools/ansify
@@ -195,17 +195,29 @@
     r'^(?P<ret>[A-Za-z_][\w \t]*?[\w*][ \t*]+)?'
     r'(?P<name>[A-Za-z_]\w*)\s*\('
     r'(?P<params>[^)]*)\)\s*(?:/\*.*?\*/\s*)?$')
 TYPEONLY = re.compile(r'^(static\s+|register\s+)*[A-Za-z_][\w \t]*\**\s*$')
 DECL_LINE = re.compile(r'^\s*(register\s+)?[A-Za-z_][\w \t,*\[\]()]*;\s*(/\*.*?\*/\s*)?$')
+BRACE_GLUED = re.compile(
+    r'^(?P<decl>\s*(register\s+)?[A-Za-z_][\w \t,*\[\]()]*;)\s*\{\s*$')
 
 RESERVED = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'do', 'else'}
 
 
 def parse_decl_block(lines, start):
     """Consume K&R parameter declarations from lines[start:]; return
-    (index_of_brace_line, decl_text) or None if this isn't a K&R body."""
+    (index_of_brace_line, decl_text) or None if this isn't a K&R body.
+
+    Does not mutate `lines` -- a brace-glued line (last param declaration
+    and the opening brace on one physical line) is only *identified* here;
+    the caller rewrites it to a bare '{' itself, and only once it has
+    committed to actually using this candidate (see convert_file). This
+    candidate can still be rejected downstream (no DB signature, DRIFT,
+    unhandled type, unparseable helper decls) -- mutating here unconditionally
+    would silently destroy the glued declaration's text even on that
+    rejection path, since nothing else in the file re-derives it.
+    """
     i = start
     decls = []
     while i < len(lines):
         s = lines[i].strip()
         if s.startswith('{'):
@@ -215,10 +227,14 @@
             continue
         if DECL_LINE.match(lines[i].rstrip('\n')):
             decls.append(s)
             i += 1
             continue
+        bg = BRACE_GLUED.match(lines[i].rstrip('\n'))
+        if bg:
+            decls.append(bg.group('decl').strip())
+            return i, '\n'.join(decls)
         return None
     return None
 
 
 def parse_local_decls(decl_text):
@@ -357,10 +373,16 @@
             continue
 
         if prev_type:
             out.pop()           # the type-only line is folded into new_hdr
         out.append(new_hdr)
+        # A brace-glued decl block (last param and '{' on one physical
+        # line) is only rewritten to a bare '{' now that this candidate is
+        # definitely being used -- see parse_decl_block's docstring for why
+        # this can't happen speculatively.
+        if lines[brace_idx].strip() != '{':
+            lines[brace_idx] = '{'
         i = brace_idx           # skip the K&R decl block entirely
         changed = True
 
     if not changed:
         return None, rep
```

Note this is **not identical** to the prompt's specified patch: the
prompt's version puts `lines[i] = '{'` directly inside
`parse_decl_block`, executed unconditionally the moment a brace-glued
line is recognized. I applied that version first, byte-for-byte as
specified, and it passed every unit test in the prompt (§3b). It was
only during the real 7-directory recheck (`atk/support/mark.c`) that a
second defect surfaced that the prompt's unit tests didn't exercise —
full detail and reproduction in §5. The corrected version above
defers the `lines[]` mutation from `parse_decl_block` (which only
*identifies* the brace-glued line now) to `convert_file`'s accept path
(which performs the mutation only once a candidate is definitely being
used). Behavior for every case the prompt's own unit tests covered is
unchanged; only the previously-untested skip-after-accept path differs
(correctly, per §5).

### 3b. Scratch confirmation, before touching the real tree

Ran three scratch scripts under
`/private/tmp/claude-501/.../scratchpad/` (loaded `ansify` via
`importlib.machinery.SourceFileLoader` since it has no `.py`
extension):

1. **The prompt's own stated unit-test cases** — `BRACE_GLUED` regex
   against `'struct cursor *self; {'` and
   `'register struct observable *observer;  {'`, confirming correct
   `decl` capture; confirmed `'    int normal_decl;'` and `'{'` remain
   unaffected (still routed through the ordinary `DECL_LINE`/bare-brace
   branches); confirmed `parse_decl_block` on a full synthetic function
   body (`cursor__ChangeShape`) returns the correct `(brace_idx,
   decl_text)`. All passed against the first (prompt-exact) version of
   the fix.
2. **A full `convert_file` end-to-end pass** on a synthetic brace-glued
   helper function (`helper_func`, not a class method — no DB lookup
   needed), confirming the whole pipeline (not just the regex/parser in
   isolation) produces the correct typed signature:
   `static int helper_func(struct point *a, int b)` on its own line,
   followed by a bare `{`.
3. **After finding and fixing the corruption bug (§5)**, re-ran both of
   the above against the corrected version (test 1 updated to assert
   `parse_decl_block` no longer mutates `lines[]` itself — this is an
   intentional, correct behavior change, not a regression) — all still
   pass — plus a new fourth reproduction script proving the corruption
   is fixed: a synthetic file with a brace-glued class-method candidate
   whose DB lookup fails (mimicking `InitializeObject`) alongside a
   second, ordinary helper that legitimately converts. Before the
   correction, this reproduced `mark.c`'s exact failure mode (the
   skipped candidate's parameter declaration silently deleted from the
   output). After the correction, the skipped candidate's original text
   is preserved byte-for-byte, while the legitimate helper still
   converts correctly.

Only after all of this passed did I touch any file under `src/`.

## 4. Retrospective recheck — per directory

Order: `overhead/class/testing`, `atk/basics/common`, `atk/value`,
`atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x` (as
specified). Every directory: dry-run → investigate findings → real run
→ investigate any failure → `make -C <dir> clean` / `depend` / `-k
install`, **twice**, as separate absolute-path calls (no `cd`, no
chaining — this session's harness resets cwd between Bash calls, so
`make -C` was used throughout rather than relying on cwd persistence).

### `overhead/class/testing` — confirmed genuine no-op

Dry-run: 3 files, 0 conversions, 0 DRIFT, 0 skips — matches the
prompt's "should be a no-op" expectation exactly. Real run identical.
Gate 1 and gate 2 both clean (0 `error:` lines each; only benign
`makedepend` "cannot find include file stdio.h/stdlib.h/errno.h"
noise, expected — `makedepend` doesn't search the macOS SDK path by
default).

### `atk/basics/common` — 10 files converted, matches census exactly

Dry-run found exactly the 10 candidate files the prompt predicted:
`cursor.c` (1 method), `describe.c` (1 helper), `im.c` (1 method + 3
classprocs), `observe.c` (5 methods + 1 helper), `owatch.c` (1
method), `physical.c` (10 helpers), `point.c` (10 helpers), `rect.c`
(20 helpers), `region.c` (13 methods), `view.c` (2 methods). 0 DRIFT.

Two skip findings, both pre-existing and already resolved by the B1
session (not new, not brace-glued — verified their `{` is on its own
line, not glued):
- `im__PlayActions: no signature in DB` — confirmed dead code (B1
  report: "confirmed dead (never called anywhere in the tree)"),
  re-confirmed no callers anywhere in `src/`.
- `view__InsertGraphic: no signature in DB` — same, confirmed dead in
  B1, re-confirmed.

Real run: all 10 files converted, 0 compile failures. One incidental
compile failure during the run for `messitem.c` (not one of the 10
candidate files) — matches the exact, already-documented
`fix-missing-static-decl` non-idempotency issue from B1's report item
7 (the tool re-fires against an already-correctly-typed declaration
and correctly reverts). Confirmed `messitem.c` does not appear in
`fossil status` (untouched, correctly auto-restored) and its
`messitem_Create` declaration at line 32 is already the correct typed
form from B1.

**A new observation, not a live bug, worth noting**: `point.c` and
`rect.c` each have a block of dead code inside a `/* ... */` comment
("These have been macrofied") that also got its signatures rewritten,
since `ansify`'s line-scanner has no comment-state tracking — it
converts any HDR-matching text regardless of whether it's inside a
comment. Verified the comment delimiters remain intact (no `/*`/`*/`
corruption) and the gate is clean either way, since compiled-away
comment text has zero behavioral effect. Not fixed (out of scope, pure
cosmetic, zero risk) but flagging since a future session touching
`ansify`'s parser might want to add comment-state tracking.

**A second observation, investigated and confirmed harmless**:
`describe.c`'s `describer_InitializeObject` (single underscore, every
sibling class in the directory uses the double-underscore `__`
convention) got converted as a **file-local helper** rather than a
class method, since the `__` naming regex didn't match it. Traced this
all the way through `overhead/class/pp/class.c`: the `initializeobject`
wiring flag (which controls whether the generated `.eh` ever calls
`CLASS__InitializeObject`) is only set when a class either restates
`InitializeObject` in its `.ch` or has a non-empty `data:` section —
`describer[describe]`'s `.ch` has neither. Confirmed directly against
the regenerated `describe.eh`: zero references to `InitializeObject`
anywhere in it. Confirmed zero callers of
`describer_InitializeObject`/`describer__InitializeObject` anywhere in
`src/`. This function is genuinely, unconditionally dead regardless of
its naming — same category as `im__PlayActions`/`view__InsertGraphic`.
No fix applied (matches established precedent of leaving confirmed-dead
code alone).

Gate 1: clean, 0 errors. Gate 2: clean, 0 errors. Spot-checked diffs
for `observe.c` (per the prompt's specific caution about this file's
history — LP64 Observer-destroy UAF was a different bug in a different
part of this same file; this diff is pure syntax, zero semantic
change), `im.c`, `view.c`, `cursor.c`, `describe.c`, `owatch.c`,
`region.c`, `physical.c`, `point.c`, `rect.c` — all clean pure-syntax
conversions.

### `atk/value` — 3 files converted, matches census exactly

Dry-run found exactly the 3 candidates: `entrint.c`, `entrtext.c`,
`menttext.c`, each converting exactly 1 method. 0 DRIFT. One
pre-existing skip (`valueview__GetCenter: no signature in DB` in
`valuev.c`) matches B2's already-documented total of 10 skips for this
directory; the file itself shows "no K&R definitions converted"
overall (nothing else in it changed), so — consistent with what §5
below establishes — this skip was never at risk of the corruption bug
regardless of which version of the fix was in use at the time.

Real run: all 3 converted, 0 compile failures.

**Checked the prompt's named risk area directly**: none of the 3
newly-converted methods is `FinalizeObject` — all three are
`DeleteCharacters` (`enterint__DeleteCharacters`,
`entertext__DeleteCharacters`, `mentertext__DeleteCharacters`). The
7 `FinalizeObject`-with-non-`void`-return classes named in the prompt's
risk section are present in this directory but none of their
`FinalizeObject` methods happened to be in brace-glued style, so this
risk area doesn't actually apply here — confirmed by direct inspection
of the diffs, not assumed.

Gate 1: clean. Gate 2: clean. Also re-ran a dry-run against this
directory again after the mid-session tool correction (§5) — identical
baseline, confirming this directory's earlier (pre-correction) work is
unaffected.

### `atk/support` — 5 files converted, matches census exactly (after the mid-session tool fix)

Dry-run found exactly the 5 candidates: `mark.c` (1 method + 1
classproc), `nstdmark.c` (8 methods + 1 helper), `rectlist.c` (2
classprocs + 1 helper), `style.c` (5 methods + 3 helpers), `tree23.c`
(13 methods). 0 DRIFT. Skip findings: `mark__InitializeObject`,
`nestedmark__InitializeObject`, `tree23int__InitializeObject` (all "no
signature in DB", matching the established B1 InitializeObject-skip
pattern) plus `environment__Dump` in `envrment.c` (file otherwise
untouched, matches B2's already-documented single skip for this
directory).

**This directory is where the mid-session tool bug surfaced live** —
full diagnosis, reproduction, and fix in §5. After the fix: real run
converted `rectlist.c` and `style.c` cleanly on the first (buggy-tool)
pass already (their skips, if any, weren't in brace-glued-plus-changed
files); `mark.c`, `nstdmark.c`, `tree23.c` failed to compile under the
buggy tool version, succeeded cleanly (0 compile failures) after the
correction.

**Hand-folded the 3 still-K&R `InitializeObject` methods**
(`mark__InitializeObject`, `nestedmark__InitializeObject`,
`tree23int__InitializeObject`) to the established ANSI convention
(`boolean CLASS__InitializeObject(struct classheader *classID, struct
CLASS *self)`), matching B1's precedent for `event.c`/`keystate.c`/
`init.c` — `ansify` itself can never convert these (the DB lookup for
`InitializeObject` unconditionally fails; it's not a regular
`.ch`-declared method), and B1 already established hand-folding as the
correct resolution for this recurring category. Verified each
individually with a direct `make <base>.o` (0 `error:` lines, only the
expected benign `-Wdeprecated-non-prototype` warnings from other
still-K&R functions in the same translation units) before running the
directory-wide gate.

Gate 1: clean. Gate 2: clean.

### `atk/supportviews` — 1 file converted, matches census exactly

Dry-run found the 1 candidate (`oscroll.c`, 1 helper). 0 DRIFT. 7
pre-existing skips total (4 in `oscroll.c`: `normal_scroll__Update`,
`motif_scroll__Update`, `normal_scroll__Hit`, `motif_scroll__Hit`; 1 in
`matte.c`: `matte__SetDrawing`; 2 in `sbutton.c`:
`sbutton__WriteDataPart`, `sbutton__ReadDataPart`) — matches B2's
already-documented total of 7 for this directory exactly.
`ClearChain`/`SetDrawing`-style skips (checked directly against their
`.ch` files) follow the well-established "override of a base classproc
not restated in this subclass's own `.ch`" pattern (same root cause as
the `InitializeObject`/`FinalizeObject` DB-miss pattern: `ansify`'s DB
lookup is per-class, not inheritance-aware) — already understood, not
new, no action needed.

Real run: `oscroll.c` converted cleanly, 0 compile failures for it.
Two incidental compile failures during the run (`lprrulv.c`,
`strtbl.c`, neither a candidate file) — both the same known
`fix-missing-static-decl` non-idempotency pattern seen in
`atk/basics/common`; confirmed both auto-restored correctly (absent
from `fossil status`).

Gate 1: clean. Gate 2: clean.

### `atk/adew` — confirmed genuine no-op

Dry-run: 13 files, 0 conversions, 0 DRIFT — matches the prompt's
"should be a no-op" expectation. 3 pre-existing skips (`cel.c`:
`cel__FinializeObject`, `cel__ClearChain`; `celv.c`:
`celview__SetDrawing`), none actionable:
- **`cel__FinializeObject`** (note the misspelling — "Finialize", not
  "Finalize") is a genuinely dead, misspelled duplicate: a
  correctly-spelled, fully-typed, real `cel__FinalizeObject` already
  exists separately at `cel.c:78` with real cleanup logic (dataobject
  teardown, chain unlinking); the misspelled one at line 698 has an
  **empty body** and is never called (the class machinery looks for
  the exact spelling `cel__FinalizeObject`). A genuine ~35-year-old
  copy/paste-and-rename leftover, but inert — flagging per the
  prompt's "flag prominently" guidance for any real latent bug found,
  though this one requires no fix since it's provably dead either way.
- `cel__ClearChain`, `celview__SetDrawing` — same
  override-of-base-classproc pattern as `atk/supportviews` above.

Real run identical to dry-run (0 conversions). Confirmed via `fossil
status` that no files in this directory changed. Gate 1: clean. Gate
2: clean.

### `atk/basics/x` — 1 file converted, matches census exactly; confirms `xgraphic.c` fully clean

Dry-run found exactly the 1 candidate the prompt predicted: `xim.c` (1
helper). **`xgraphic.c` shows zero brace-glued instances** — confirms
B2's hand-fix of all 32 of `xgraphic.c`'s brace-glued lines is complete
and durable; the fixed tool finds nothing left to do there, exactly as
the prompt anticipated ("if it does find something in xgraphic.c,
that's worth a closer look" — it found nothing).

Real run: `xim.c` converted cleanly, 0 compile failures for it. One
incidental compile failure (`xgraphic.c`, not the candidate file) —
same known `fix-missing-static-decl` non-idempotency pattern again;
confirmed auto-restored (absent from `fossil status`).

Gate 1: clean. Gate 2: clean.

## 5. The mid-session tool bug — full detail (this matters most, per the prompt's instructions)

### Symptom

Real run against `atk/support` failed to compile 3 files
(`mark.c`, `nstdmark.c`, `tree23.c`) with errors like:

```
mark.c:73:9: error: conflicting types for 'mark__InitializeObject'
./mark.eh:227:9: note: previous declaration is here
mark.c:76:11: error: member reference type 'int' is not a pointer
```

`ansify`'s own compile-gate-and-restore correctly caught this and
reverted all 3 files — **no corrupted content ever reached disk in the
checked-out tree**, so this was a correctness bug in the tool, not data
loss in the repository.

### Root cause

`mark__InitializeObject` is itself a brace-glued K&R function:

```c
boolean mark__InitializeObject(classID, self)
struct classheader *classID;
struct mark *self;  {
    self->next = NULL;
    ...
```

`parse_decl_block` (as specified in the prompt, first version) correctly
identifies this as brace-glued and — as literally specified —
immediately mutates `lines[i] = '{'`, destroying the text `struct mark
*self;  {` before returning. But `InitializeObject` **always** fails
the class-method DB lookup (it's a lifecycle hook, not a
`.ch`-declared regular method — the same reason B1 had to hand-fold
`event.c`/`keystate.c`/`init.c`), so `convert_file` ultimately rejects
this candidate (`skipped ... no signature in DB`) — but the mutation
had *already happened*, unconditionally, as a side effect of merely
*recognizing* the shape, before the accept/reject decision was made.

Because `mark.c` also had two other, unrelated candidates
(`mark__SetStyle`, `mark__NewWithStyle`) that *did* successfully
convert, `changed` became `True` for the file, so `convert_file`
returned the full rewritten text — which included the corrupted
`InitializeObject` block: its header stayed K&R-style
(`mark__InitializeObject(classID, self)`, never rewritten, since
`new_hdr` was `None`), `struct classheader *classID;` stayed intact
(a separate, non-glued line, untouched), but the `struct mark *self;`
declaration was gone, replaced by a bare `{` — leaving `self` an
implicit-`int` K&R parameter. Hence `self->next` failing to compile
("member reference type 'int' is not a pointer") and the "conflicting
types" error against the `.eh`'s typed prototype.

This is a **general defect**, not narrow to `InitializeObject`: any
brace-glued class method or helper whose candidate is later rejected
for *any* reason (DB miss, DRIFT param-count mismatch, "unhandled
type", or "unparseable K&R declarations" for a helper) would suffer the
same corruption, *provided* something else in the same file
legitimately changes (otherwise `convert_file` returns `None` early and
discards the corrupted-but-unused `out`/`lines` state, so the
corruption never becomes externally visible — this is why
`atk/basics/common`'s `im__PlayActions`/`view__InsertGraphic` and
`atk/value`'s `valueview__GetCenter` were never at risk: neither is
itself brace-glued, or the file had nothing else changing).

Reproduced this precisely in isolation with a synthetic two-function
file (one brace-glued candidate with a deliberately-missing DB entry,
one ordinary helper) — before the fix, output showed the exact same
corruption shape (glued declaration silently deleted); after the fix,
the skipped candidate's original text is preserved byte-for-byte.

### Fix applied

Deferred the `lines[]` mutation from `parse_decl_block` (which now only
*identifies* a brace-glued line, returning its index and decl text
without touching `lines`) to `convert_file`'s accept path — the
mutation now happens only at the point where `new_hdr is not None` is
already established, immediately before `i = brace_idx`. This preserves
identical behavior for every case the prompt's original unit tests
covered (a brace-glued candidate that's actually accepted still gets
its line correctly rewritten to `{`) while fixing the previously-untested
skip-after-accept interaction. Full diff in §3a; full test evidence in
§3b (point 3).

### Blast-radius check on already-completed work

Before continuing, checked whether `atk/basics/common` or `atk/value`
(both already fully converted and gated under the *buggy* first
version of the fix) were affected:
- `atk/basics/common`'s only two skips (`im__PlayActions`,
  `view__InsertGraphic`) are **not** brace-glued (their `{` is on its
  own line in the original source) — confirmed by direct inspection —
  so the buggy mutation path was never triggered for them regardless.
- `atk/value`'s only skip (`valueview__GetCenter`) is in a file
  (`valuev.c`) where nothing else changed (`changed` stayed `False`
  for that file), so even if it *had* been brace-glued, `convert_file`
  would have returned `None` early and discarded the corrupted `out`
  before it could reach disk.
- Re-ran a fresh dry-run against both directories with the *corrected*
  tool after the fix: identical baseline in both cases (same file
  counts, same conversions, same skips) — confirms no regression and
  no missed corruption.

**Conclusion: neither already-completed directory needed to be redone.**
Only `atk/support` (discovered live) was affected, and it's now
correctly converted with the fixed tool.

## 6. Real `.ch`-vs-`.c` bugs found

**None.** Every DRIFT count across all 7 directories was 0, both before
and after conversion. The one genuine bug found this session (§5) is a
defect in the `ansify` tool itself, not a `.ch`-vs-`.c` disagreement in
the ported codebase — flagging it prominently per the prompt's
instruction regardless, since it's the same spirit of "latent bug
surfaced by finally forcing a real rebuild."

The `cel__FinializeObject` misspelled-duplicate-with-empty-body found
in `atk/adew` (§4) is a genuine ~35-year-old bug in the original 1988
codebase, but it's provably inert (empty body, never called under
either spelling), so it doesn't rise to the level of the `im.c`
colormap dereference or the B2 `.ch` typos — no behavior is affected
either before or after this session. Noted for `revival.md`'s "Old
bugs never found till now" if the orchestrator wants it recorded, but
it changes nothing about the tree's current behavior.

## 7. Files touched — `fossil status` (before any commit)

```
EDITED     ../revival/tools/ansify
EDITED     atk/basics/common/cursor.c
EDITED     atk/basics/common/describe.c
EDITED     atk/basics/common/im.c
EDITED     atk/basics/common/observe.c
EDITED     atk/basics/common/owatch.c
EDITED     atk/basics/common/physical.c
EDITED     atk/basics/common/point.c
EDITED     atk/basics/common/rect.c
EDITED     atk/basics/common/region.c
EDITED     atk/basics/common/view.c
EDITED     atk/basics/x/xim.c
EDITED     atk/support/mark.c
EDITED     atk/support/nstdmark.c
EDITED     atk/support/rectlist.c
EDITED     atk/support/style.c
EDITED     atk/support/tree23.c
EDITED     atk/supportviews/oscroll.c
EDITED     atk/value/entrint.c
EDITED     atk/value/entrtext.c
EDITED     atk/value/menttext.c
```

21 files total (1 tool + 20 source files), exactly matching the
prompt's per-directory census (10 + 3 + 5 + 1 + 0 + 0 + 1 = 20 source
files across the 7 directories — `overhead/class/testing` and
`atk/adew` both confirmed genuine no-ops, 0 files each).

`fossil extras` (filtered to exclude `build/` artifacts, which are
always untracked noise, and other sessions' pre-existing `.diff`/
`-REPORT.md`/`-prompt.md` files, which predate this session): only
ordinary generated build byproducts within the touched directories
(`index` files from `makedepend`, compiled binaries like `atkatoms`/
`genarb`/`createcon`/`testobj.so`, `install.time`/`install.doc`,
generated `Makefile`s, `.doc` files in `build/doc/`) — nothing
unexpected, no leftover `.ansify-orig` backup files, no stray new
source files.

Every touched file compiled cleanly (0 `error:` lines) at least twice
(once per directory's double gate); the 5 files hand-touched beyond
`ansify`'s own automated conversion (`mark.c`, `nstdmark.c`,
`tree23.c` for the `InitializeObject` hand-fold) were each additionally
verified individually with a direct `make <base>.o` before the
directory-wide gate.

## 8. Anything that surprised you / didn't match expectations

1. **The prompt's own fix, as literally specified, had a real defect**
   not caught by its stated unit tests — full detail in §5. This isn't
   a criticism of the pre-diagnosis (the regex/branch-placement design
   is correct and the prompt was explicit that trust should go there,
   not into re-deriving it) — it's that the *unit tests described* only
   covered the always-accepted path, and the real tree immediately
   exercised the untested skip-after-accept path on the very first
   `InitializeObject` candidate encountered. Worth folding into
   `porting-assessment.md` or wherever `ansify`'s own defect history is
   tracked, since any *future* change to `parse_decl_block` should keep
   this "don't mutate speculatively" invariant in mind.
2. **The comment-blind conversion of dead code inside `/* ... */`
   blocks** in `point.c`/`rect.c` (§4) — harmless today, but a genuine
   tool limitation (no comment-state tracking) that a future `ansify`
   change might want to address if it ever starts mattering.
3. **`describer_InitializeObject`'s single-underscore naming** (§4)
   required tracing all the way into `overhead/class/pp/class.c`'s
   `initializeobject` flag logic to confirm it's genuinely inert rather
   than a live wiring bug — a deeper rabbit hole than expected for what
   turned out to be a non-issue, but the prompt's "verify rather than
   assume" framing for the `FinalizeObject` risk area made this feel
   like the right level of rigor to apply generally.
4. **Every directory's census matched the prompt's predicted counts
   exactly** — no surprises in scope, only in the two findings above
   plus the tool-bug in §5.
