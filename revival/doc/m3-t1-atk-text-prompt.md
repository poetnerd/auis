# M3 Wave 3, Batch T1: `atk/text` — ansify + `-pe`/`.eh` rollout

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before starting (same hard rules as every M3 batch: no fossil commits ever, stop at the gate, write `m3-t1-atk-text-session.diff` in the tree root and `m3-t1-atk-text-REPORT.md` in `revival/doc/claude-history/`, command style for an unattended session). Read `porting-assessment.md`'s §17 subsection (search "classpp's own `InitializeClass`/`InitializeObject`/`FinalizeObject` special-casing", including the "second real exception" subsection added by B3) and `m3-rollout-runbook.md`'s B1/B2/B3 findings entries for background — this batch has been fully pre-diagnosed against those known patterns and came back unusually clean; see below.

## Scope

1 directory, 30 files, 21 classes: `atk/text` — alone, its own wave per the plan's dependency order (highest-consumed single directory in the insets/apps waves that follow T1). Like B1/B2/B3, this batch does both halves of M3: `ansify` (K&R→ANSI) **and** the `-pe`/`.eh` Export rollout (add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `atk/text/Imakefile`, force-regenerate `.eh`, gate).

## Pre-diagnosis already done — unusually clean, no `.ch`/`.c` fixes required before the real run

The orchestrator ran the full standing checklist (`m3-rollout-runbook.md`'s "Current standing per-batch checklist") plus a temporary `-pe` test-build (added `CLASSFLAGS = $(CLASSINCLUDES) -pe` to the Imakefile, force-generated all 21 `.eh` files with the real `class` binary, inspected them directly, then reverted) before writing this prompt. Unlike B1/B2/B3, **zero `.ch`/`.c` pre-fixes are needed** — every finding below resolved to an already-established safe pattern. Do not re-derive any of this; apply the guidance and move straight to the real run.

### 1. Restated-lifecycle-param check (B3's finding class) — all 13 instances confirmed safe, no fix needed

`atk/text` has 13 `.ch` restatements of `InitializeClass`/`FinalizeObject`'s implicit params:

- **7 `InitializeClass()` with plain empty parens** (`fnote.ch`, `fnotev.ch`, `pcompch.ch`, `textrefv.ch`, `texttagv.ch`, `textv.ch`, `tindex.ch`) — this is the safe, undecorated convention (§17): since `InitializeClass` is **not** hardcoded by classpp, empty parens means the ordinary classproc-emission loop appends nothing beyond its automatic `struct classheader *` prefix, producing a 1-param exported prototype. Confirmed by direct `-pe` test-build (all 7 emit `boolean <class>__InitializeClass(struct classheader *);`) **and** by checking each real `.c` definition by hand — all 7 take exactly 1 param (`classID`/`ClassID`). Matches the true convention exactly; nothing to fix.
- **6 `FinalizeObject(struct <class> *self)` restating only `self`** (`cltextv.ch`, `content.ch`, `tabs.ch`, `smpltext.ch`, `text.ch`, `textv.ch`) — this is the other safe convention (same shape as B2's `buttonv.ch`/`sliderv.ch` workaround, now just harmless redundant documentation per the classpp `FinalizeObject` fix): the ordinary loop's prefix (`struct classheader *`) plus the one declared arg (`struct <class> *self`) makes exactly 2 params. Confirmed by direct `-pe` test-build (all 6 emit a correctly 2-param prototype) **and** by checking each real `.c` definition by hand — all 6 take exactly 2 params (`classID, self`). Matches the true convention exactly; nothing to fix.

**None of these 13 restate *both* implicit params** (the actual broken shape B3 found in `unknownv.ch`/`suiteev.ch`) — that shape does not occur anywhere in this directory. No `.ch` edits needed for lifecycle methods in this batch.

### 2. Confirmed-safe skips — expect these, no fix needed

- `bpv.c`: `bpv__InitializeObject` — ordinary `InitializeObject` DB-miss (classpp fully hardcodes this classproc's signature regardless of `.ch`/DB; the "no signature in DB" skip is expected and harmless, same as every other class in the tree).
- `readscr.c`: `readscr__InitializeObject` — same, ordinary DB-miss skip.
- `txttroff.c`: `texttroff__CompileNotes` — **confirmed genuinely dead/orphaned**: not declared anywhere in `txttroff.ch` (grepped the full file), and grepping the whole tree for `texttroff__CompileNotes`/`CompileNotes(` outside this file finds zero external callers. (There is also a same-named `static struct text *CompileNotes(...)` file-local helper in the same file with a completely different 4-arg signature — an unrelated, unexported, unambiguous C-scope shadow, not a conflict.) Will report `skipped: no signature in DB`. Leave as-is — no interface to fix, no caller to break.

### 3. Double-pointer K&R parameters — investigated as a specific risk, confirmed clean

B3 found a genuine `ansify` tool bug where a `void **` K&R parameter silently lost one `*` during conversion (1 instance tree-wide at the time, `atk/syntax/tlex/tlex.c`). `atk/text` has substantially more double-pointer parameters than that single prior instance — real K&R parameter declarations of this shape exist in `be1be2a.c` (`char **argv`), `content.c` (`char **lst`, x2 functions), `pcompch.c` (`char **fromChars`, `char **str`), `search.c` (`struct SearchPattern **result`, `unsigned char **s` — both also brace-glued to their `{`, see below), `text.c` (`struct dataobject **list`, in `#if 0`/`#else` dead-and-live pair), `tindex.c` (`char *term, **error`, mixed single/double on one line), `txtvcmds.c` (`struct menulist **normalMenus`/`**Menus`, 4 functions, plus `char **fromChars`), and `txttroff.c` (`char **lst`, x2 functions) — roughly a dozen real instances.

Given the volume, the orchestrator ran `ansify`'s `convert_file` directly (in-memory, via the same `importlib.machinery.SourceFileLoader` technique the tool-fix tasks used) against all of these files and diffed the `**name` occurrence count before/after for every double-pointer parameter name. **All matched exactly — no instance of the star-drop bug triggered anywhere in this directory.** Spot-checked `search.c`'s `CompilePattern`/`TryMatch` directly (the two brace-glued instances) and confirmed both converted correctly with their double-pointer types intact. Treat this as resolved, not as something to re-verify — but if the real run somehow does find a single-`*`-where-double-expected compile failure despite this, it will be an easy, obvious hand-fix (add the missing `*` back), not a mystery.

### 4. Two files will hit the (now-fixed) brace-glued-to-parameter shape — expected, self-healing

`smpltext.c` and `search.c` each have at least one K&R declaration block with the last parameter and the opening `{` on the same physical line (the shape `parse_decl_block`/`convert_file` were fixed for earlier in M3 — see the runbook's "ansify brace-glued parser fix" and "brace-body corruption fix" entries). This is expected and requires no manual intervention — the tool now handles both the parameter-glue and body-glue variants correctly on its own. Noted here only so it isn't mistaken for something new.

## Task

1. Run `ansify --dir src/atk/text` for real (not dry-run). Expect the same shape as the orchestrator's dry-run: 0 DRIFT, 3 skips (`bpv__InitializeObject`, `readscr__InitializeObject`, `texttroff__CompileNotes` — all confirmed safe above). Investigate any *new* DRIFT/skip finding the same way every prior M3 batch has (check `.ch` vs `.c` by hand, consult `porting-assessment.md`/`m3-rollout-runbook.md` for known patterns first).
2. Standing per-batch checks (already run once by the orchestrator, confirmed clean — re-confirm quickly as part of your own pass): macro-typo grep (clean), empty-parens-lifecycle-method grep (clean, none found). The restated-lifecycle-param check and the brace-glued-parser grep are **no longer needed as separate standing tasks** for this directory — both are already investigated in full above (§1, §4) and the parser gap itself is tool-fixed and self-healing.
3. Add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `atk/text/Imakefile` (same placement convention as every other `-pe`'d directory — see `atk/basics/common/Imakefile` or `atk/value/Imakefile` for the exact pattern; insert directly after the existing `COMPILERFLAGS` line).
4. Force-regenerate all 21 `.eh` files with the rebuilt classpp (remove existing `.eh`, `make <name>.eh` per class, explicit target list, same mechanic as B1/B2/B3).
5. Investigate and fix any new compile fallout from turning `-pe` on for real (beyond what's already anticipated above) — same triage process as every prior batch. Given the double-pointer investigation in §3, pay attention to any compile failure that looks like a pointer-count mismatch specifically — it should not happen, but if it does, it is the known star-drop shape and the fix is simply restoring the missing `*`.
6. Subtree-local gate: `make -C <absolute-path-to-atk/text> clean`, then `depend`, then `-k install` — separate calls, absolute path, not chained, **twice** for determinism.

## Gate

Stop after `atk/text` gates clean, twice. **Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively** — the orchestrator will present runtime-check suggestions to wdc separately after independently re-verifying your work.

## Report

Write `revival/doc/claude-history/m3-t1-atk-text-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- Confirmation that the real `ansify --dir` run matched the pre-diagnosed shape (0 DRIFT, the 3 confirmed-safe skips) — or, if it didn't, full detail on what differed.
- Confirmation that the double-pointer-parameter investigation in §3 held up under the real (non-dry-run, compile-gated) conversion — i.e., that none of the ~12 double-pointer parameters lost a `*`.
- Any new DRIFT/skip/compile-fallout finding not covered above, and how you resolved (or, if genuinely unresolved, clearly flagged) it.
- Per-file `ansify` conversion counts (methods/classprocs/helpers) and gate results (twice).
- `fossil status`/`fossil extras` confirming exactly which files changed, no commit made.
- Anything that surprised you or didn't match this prompt's expectations.
