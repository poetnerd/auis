# classpp fix: `FinalizeObject` empty-parens `-pe` self-inconsistency

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before starting (same hard rules as every M3 batch: no fossil commits ever, stop at the gate, write `<task>-session.diff` in the tree root and `<task>-REPORT.md` in `revival/doc/claude-history/`, command style for an unattended session). This is **not** an `ansify` batch — it's a fix to `overhead/class/pp/class.c`, the M1 codegen tool itself, plus a verification pass. Read `porting-assessment.md`'s new subsection (search for "classpp's `FinalizeObject` prototype/call-site inconsistency") and `m3-rollout-runbook.md`'s B2 findings entry for full background before touching anything.

## The bug, already fully diagnosed — do not re-derive

`overhead/class/pp/class.c` hardcodes a 2-arg (`classID`, `self`) **exported prototype** for `InitializeObject` (lines ~1121-1122) regardless of what a class's `.ch` declares. It does **not** do the same for `FinalizeObject`: `FinalizeObject`'s prototype is built from whatever the `.ch` actually declares (the ordinary classproc-emission loop, ~line 1146: `sprintf(proto, "struct classheader *%s", mp->realargtypes)`), while the **internal generated call site** inside `__Finalize` (line ~1334: `"    %s__FinalizeObject(classID, self);\n"`) is unconditionally hardcoded to pass 2 arguments. A class whose `.ch` declares the ordinary empty-parens `FinalizeObject();` convention (0 declared args → a 1-arg prototype, `classID` only) gets a self-inconsistent `.eh` the moment `-pe` is on: the internal 2-arg call conflicts with the 1-arg prototype above it, a hard compile error **inside the `.eh` itself**.

## The fix must be narrower than mirroring `InitializeObject` — this is the critical part, verified already

**Do not** copy `InitializeObject`'s treatment wholesale (hardcoding both the arg list *and* a `void` return type, and adding `FinalizeObject` to the "skip in the ordinary loop" list at line ~1143). That would be wrong and would **break already-committed code**: 7 classes in `atk/value` (`menttext.ch`, `entrtext.ch`, `clklistv.ch`, `entrintv.ch`, `entrint.ch`, `mentstrv.ch`, `entrstrv.ch`) declare `FinalizeObject(struct <class> *self) returns boolean;` — a real, non-`void` return type, already correctly handled by the existing ordinary-loop path (confirmed: the code comment at line ~1140-1142 exists specifically because of this — `FinalizeObject`'s return type is *not* always `void`, unlike `InitializeObject`'s, which always returns `boolean`). These 7 classes already work correctly today because they explicitly restate `self`'s type — they are not the bug, and any fix that ignores their declared return type will break them.

**The correct, narrow fix**: only when `FinalizeObject` is declared with **literal empty parens** (`mp->realargtypes` is empty/NULL — i.e., 0 explicit declared args), synthesize the missing `self` parameter type into the prototype string, while still using `mp->methodtype` for the return type exactly as the existing code already does. Every class that already explicitly restates `self` (with any return type) must go through the exact same code path as today, completely unaffected.

Concretely, in the classproc-emission loop (currently, ~line 1146-1151):
```c
if (usePrototypesExport) {
    sprintf(proto, "struct classheader *%s", mp->realargtypes);
}
else {
    proto[0] = '\0';
}
```
change to:
```c
if (usePrototypesExport) {
    if (strcmp(mp->name, "FinalizeObject") == 0
        && (mp->realargtypes == NULL || mp->realargtypes[0] == '\0')) {
        sprintf(proto, "struct classheader *, struct %s *", FinalClassName);
    }
    else {
        sprintf(proto, "struct classheader *%s", mp->realargtypes);
    }
}
else {
    proto[0] = '\0';
}
```
Do **not** touch the `if (strcmp(mp->name, "InitializeObject") == 0) continue;` skip line, and do **not** add `FinalizeObject` to it — `FinalizeObject` must keep going through this loop for every class, including the 7 `atk/value` overrides, exactly as today. Verify your understanding of `mp->realargtypes`'s empty-vs-populated state by reading how it's set at parse time (~line 2167, `CurrentMethod->realargtypes = (argcount) ? realargstr : "";`) before writing the fix, to make sure the empty-string check is correct for this codebase's actual convention (it should be — `""` for zero declared args, per that line — but confirm rather than assume).

## Already confirmed, don't redo

- Every `.ch` file across all 90 M3-active directories was already grepped for `FinalizeObject()` (literal empty parens): **zero remaining instances** exist in the 7 directories that currently have `-pe` on (`overhead/class/testing`, `atk/basics/common`, `atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`) — the only two known historical instances (`atk/value/buttonv.ch`, `atk/value/sliderv.ch`) were already hand-fixed in M3 batch B2 by restating `self` explicitly, and remain that way (harmless either way once this fix lands — their explicit restatement just becomes one more ordinary case, not a special one). This means **the fix has no retroactive behavioral effect on any already-`-pe`'d directory** — its value is entirely prospective, for Waves 3-7. Confirm this understanding is still accurate (re-grep yourself) but do not expect to find or need to fix any live instance in these 7 directories.
- The 7-directory list above is exhaustive as of 2026-07-30 — no other directory has `-pe` in its `CLASSFLAGS` yet.

## Task

1. Apply the fix exactly as specified above to `overhead/class/pp/class.c`.
2. Rebuild the `class` binary (`make -C overhead/class/pp` or the equivalent this tree uses — check how prior M1/M3 sessions rebuilt this tool if unsure). Confirm it builds clean.
3. **Verification, part 1 — prove zero retroactive effect (byte-identical `.eh` regen).** Before touching anything else: for each of the 7 already-`-pe`'d directories, copy the currently-generated `.eh` files aside (a snapshot — these are untracked build artifacts, not fossil-tracked, so `fossil` won't help you diff them; use a plain file copy to a scratch location). Then force-regenerate every `.eh` in all 7 directories using the newly-rebuilt `class` binary (remove existing `.eh`, `make <name>.eh <name>.eh ...` per directory, same mechanic every M3 `-pe` batch has used). Diff the before/after `.eh` sets file-by-file. **Expect byte-identical output for every file in all 7 directories** — if anything differs, stop and report before proceeding; a real difference means either the fix is broader than intended or something else changed, and needs investigation before this is trusted.
4. **Verification, part 2 — prove the fix actually works (positive test).** Since no current directory exercises the fixed code path, write a small standalone test: create a scratch `.ch` file (outside the real source tree, e.g. in the scratchpad) for a toy class declaring `FinalizeObject();` with literal empty parens, run the rebuilt `class -s -pe` on it, and confirm the emitted `.eh` now declares a 2-arg (`classID`, `self`-typed) prototype that matches what the internal `__Finalize` call site generates — i.e., confirm a toy class using this shape would now compile clean under `-pe`, where it would have failed before your fix. Clean up the scratch test files afterward (don't leave them in the tree).
5. Subtree-local gate for each of the 7 directories: `make -C <absolute-path> clean`, then `depend`, then `-k install` — separate calls, absolute paths, not chained, twice each for determinism (14 gate cycles total, same procedure as every M3 batch). Given part 1's byte-identical proof, this should be a formality, but do it anyway as standard due diligence on a shared-tool change.
6. Also rebuild and gate `overhead/class/pp` itself (the directory the fix lives in) to confirm the tool's own build is unaffected.

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively.** This change affects a foundational, shared tool (`classpp`), not an application — there is no "runtime check" in the usual sense; the verification in steps 3-4 above (byte-identical regen + a working positive test) *is* the check. Stop after all gates are green and say you have stopped.

## Report

Write `m3-classpp-finalizeobject-fix-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- The exact diff to `class.c`.
- Full detail on the byte-identical `.eh` verification (which files were compared, confirmation of zero differences, or full detail on any difference found).
- Full detail on the positive test (the toy `.ch` used, the `.eh` it produced before and after the fix).
- Gate results for all 7 directories plus `overhead/class/pp` itself, twice each.
- `fossil status` output confirming exactly which files changed (expect only `overhead/class/pp/class.c`, plus the already-rebuilt `class` binary if that's tracked — check), no commit made.
- Anything that surprised you or didn't match this prompt's expectations.
