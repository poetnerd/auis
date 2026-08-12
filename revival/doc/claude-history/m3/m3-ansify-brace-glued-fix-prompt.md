# `ansify` fix: brace-glued-to-last-K&R-parameter parser gap + retrospective recheck

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before starting (same hard rules as every M3 task: no fossil commits ever, stop at the gate, write `<task>-session.diff` in the tree root and `<task>-REPORT.md` in `revival/doc/claude-history/`, command style for an unattended session). This is **not an ordinary ansify batch on new directories** — it's a fix to the `ansify` tool itself (`revival/tools/ansify`), followed by a retrospective re-run of `ansify` against directories that were **already converted and already committed** in M3 batches O1-O4/B1/B2. Read `porting-assessment.md`'s and `m3-rollout-runbook.md`'s B1 and B2 findings entries (search each for "brace-glued") for full background before touching anything.

## The bug, already fully diagnosed and fix already unit-tested — do not re-derive

`revival/tools/ansify`'s `parse_decl_block()` (around line 204) consumes a function's K&R parameter-declaration lines one at a time, looking for the line that opens the function body:

```python
def parse_decl_block(lines, start):
    i = start
    decls = []
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith('{'):
            return i, '\n'.join(decls)
        if s == '' or s.startswith('/*') or s.startswith('*'):
            i += 1
            continue
        if DECL_LINE.match(lines[i].rstrip('\n')):
            decls.append(s)
            i += 1
            continue
        return None
    return None
```

`DECL_LINE` (line ~199) requires the *entire* stripped line to be a declaration ending in `;` (optionally followed by a `/* comment */`), nothing more:
```python
DECL_LINE = re.compile(r'^\s*(register\s+)?[A-Za-z_][\w \t,*\[\]()]*;\s*(/\*.*?\*/\s*)?$')
```

When a K&R parameter's declaration and the function's opening brace sit on the **same physical line** — a common style in this 1980s/90s codebase, e.g. `struct cursor *self; {` — the line matches **neither** `s.startswith('{')` (it starts with `struct`) **nor** `DECL_LINE` (the trailing `{` isn't in `DECL_LINE`'s allowed character class). The function falls through to `return None`: the entire declaration block is silently treated as "not a K&R body," and the whole function definition is left completely untouched — no DRIFT report, no skip message, nothing. It's invisible to every dry-run census that greps `ansify`'s own output.

This was found in M3 batch B1 (10 instances in `atk/basics/common`, initially judged harmless via a clean double-gate) and independently in B2 (`atk/basics/x`'s `xgraphic.c`, 32 instances, found and hand-fixed locally in that file only). **B1's "harmless" judgment was later proven wrong**: it was verified empirically (2026-07-30, using this project's exact compiler flags `-std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type`) that a K&R function definition matching its typed prototype compiles with only a `-Wdeprecated-non-prototype` *warning*, not an error — so a subtree gate showing "0 errors" does **not** prove no brace-glued class method was silently left un-converted. `atk/basics/common`'s own already-committed code still has ~20+ real class methods sitting unconverted this way (`cursor__ChangeShape`, several `im__*`, `observable__*`, `region__*`, `view__*`, etc.).

## The fix — already designed and unit-tested in isolation, apply as specified

Add a second regex and a second branch to the `while` loop, so a K&R decl line with a glued trailing brace is recognized, its declaration text captured, and **the line itself is rewritten in place to just `{`** so the rest of the pipeline (which expects `lines[brace_idx]` to be a bare brace line once `parse_decl_block` returns) needs no other changes:

```python
BRACE_GLUED = re.compile(
    r'^(?P<decl>\s*(register\s+)?[A-Za-z_][\w \t,*\[\]()]*;)\s*\{\s*$')

def parse_decl_block(lines, start):
    i = start
    decls = []
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith('{'):
            return i, '\n'.join(decls)
        if s == '' or s.startswith('/*') or s.startswith('*'):
            i += 1
            continue
        if DECL_LINE.match(lines[i].rstrip('\n')):
            decls.append(s)
            i += 1
            continue
        bg = BRACE_GLUED.match(lines[i].rstrip('\n'))
        if bg:
            decls.append(bg.group('decl').strip())
            lines[i] = '{'
            return i, '\n'.join(decls)
        return None
    return None
```

Place the new `BRACE_GLUED` regex definition next to the existing `DECL_LINE`/`HDR`/`TYPEONLY` regexes (~line 199), and the new branch immediately after the existing `DECL_LINE` branch, before the final `return None`.

**This has already been unit-tested standalone** (not just reasoned about) against real lines pulled from the tree, e.g.:
```
'struct cursor *self; {'                 -> decl='struct cursor *self;', lines[i] mutated to '{'
'register struct observable *observer;  {' -> decl='register struct observable *observer;', lines[i] mutated to '{'
'    int normal_decl;'                   -> unaffected, still goes through the ordinary DECL_LINE branch
'{'                                       -> unaffected, still the ordinary bare-brace branch
```
and against a full synthetic function body (`cursor__ChangeShape(self, shape)\nstruct cursor *self; {\n...`), confirming `parse_decl_block` returns the correct `(brace_idx, decl_text)` and that `convert_file`'s existing `i = brace_idx` / `out.append(line)` flow at lines ~302/362 handles the mutated line correctly with **no other code changes needed anywhere else in the file**. Trust this design; spend your verification effort on applying it correctly and on the retrospective recheck below, not on re-deriving the regex.

## Retrospective recheck — the real scope of this task

Once the fix is applied and works on a scratch test of your own (confirm before touching the real 7 directories), re-run `ansify` against all 7 directories that already have `-pe` on and are already committed: `overhead/class/testing`, `atk/basics/common`, `atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`. This is the actual point of the fix — these directories currently contain silently-unconverted K&R class methods that this bug hid from every prior batch's dry-run census.

A rough grep-based census (regex `;\s*\{\s*$`, over-inclusive — the real ansify run is authoritative, this is just to set your expectations for scope) found **20 candidate files** across the 7 directories:
- `overhead/class/testing`: 0
- `atk/basics/common`: 10 — `cursor.c`, `describe.c`, `im.c`, `observe.c`, `owatch.c`, `point.c`, `physical.c`, `rect.c`, `region.c`, `view.c`
- `atk/value`: 3 — `entrint.c`, `entrtext.c`, `menttext.c`
- `atk/support`: 5 — `mark.c`, `rectlist.c`, `nstdmark.c`, `tree23.c`, `style.c`
- `atk/supportviews`: 1 — `oscroll.c`
- `atk/adew`: 0
- `atk/basics/x`: 1 — `xim.c` (note: **not** `xgraphic.c` — B2 already hand-fixed all 32 of `xgraphic.c`'s instances locally, so the fixed tool should find nothing left there; if it does find something in `xgraphic.c`, that's worth a closer look, not an assumption of a re-count error)

Procedure per directory:
1. `ansify --dry-run --dir <absolute-path-to-dir>` first, to survey what the fixed parser now sees without touching any file. Read every line of output.
2. For every DRIFT finding: investigate the same way every prior M3 batch has (compare `.ch` declared signature against the `.c` definition's actual param count/types) and write the resolution into your own working notes before converting — same standard as every batch's pre-diagnosis step, except here you're doing it live since this is a single delegated session rather than a separate pre-diagnosis pass.
3. For every skip finding ("no signature in DB", "unhandled type", "unparseable K&R declarations"): investigate and resolve the same way prior batches have (check `build/desc/`, check the actual `.ch`, hand-fix if the pattern matches a previously-seen case like the "override typed to base interface" pattern from B2, or report clearly if it's genuinely new).
4. Once you understand what the dry-run surfaced, run `ansify --dir <absolute-path-to-dir>` for real (no `--dry-run`) — this converts file-by-file with `ansify`'s own per-file compile-gate-and-restore already built in (`compile_gate()` in the tool, calls `make <base>.o` per file and reverts on failure), so a failure at this stage restores that one file automatically; investigate any restored file the same as a DRIFT/skip finding.
5. Subtree-local gate for the directory: `make -C <absolute-path> clean`, then `depend`, then `-k install` — separate calls, absolute paths, not chained, twice for determinism, same procedure as every M3 batch.
6. Move to the next directory. Do all 7 in one session, in the order listed above (roughly small-to-large, `overhead/class/testing` and `atk/adew` should be no-ops given the census above — confirm they're genuinely no-ops rather than skipping them).

## Known risk areas to watch for, based on prior batches

- **`atk/value`'s 7 `FinalizeObject`-with-non-`void`-return classes** (`menttext.ch`, `entrtext.ch`, `clklistv.ch`, `entrintv.ch`, `entrint.ch`, `mentstrv.ch`, `entrstrv.ch`) are in this directory. `entrint.c`/`entrtext.c`/`menttext.c` are in the brace-glued candidate list above — if any of the newly-converted methods happen to be `FinalizeObject`, double check the resulting typed signature still matches what's declared in the `.ch` (a real `boolean` return, not `void`) and that nothing here interacts badly with the classpp `FinalizeObject` fix committed earlier today (it shouldn't — that fix only affects code generation from `.ch`/`.eh`, not `ansify`'s `.c`-side conversion — but verify rather than assume).
- **The "override typed to base interface, body needs subclass type" pattern** (named in B2): if a newly-converted method is an override whose `.ch`-declared param type is a base class but the body needs the concrete subtype, apply the same established fix — rename the param to `..._generic`, add a local cast to the concrete subtype for body use, keep `super_*` forwarding calls on the `_generic` name. Don't re-derive this from scratch if you hit it; it's already a known pattern.
- **`observe.c`** appears in the `atk/basics/common` candidate list with multiple hits (`observer` parameter, `register struct observable *observer;`) — this file was the site of the LP64 Observer-destroy UAF bug fixed earlier in the project (unrelated class, unrelated bug, just noting it's a file with a history of subtle issues, worth extra care reading the diff before trusting a clean gate).
- If any newly-surfaced DRIFT looks like a real `.ch`-vs-`.c` signature bug (not a tooling artifact) similar to B1's `im.c` `*cmap` dereference or B2's 6 `.ch` type-name typos, treat it exactly as those were treated: fix it, document it clearly in your report with full detail (this is exactly the kind of ~35-year-old latent bug this whole M3 effort has been surfacing), and flag it prominently for the orchestrator to add to `revival.md`.

## Gate

**Do NOT commit.** Stop after the fix is applied, unit-verified, and all 7 directories have been re-run and gated twice each (green). Say you have stopped.

## Report

Write `m3-ansify-brace-glued-fix-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- The exact diff to `revival/tools/ansify`.
- Your own confirmation of the fix on a scratch test, before touching the real tree.
- Per-directory: dry-run output summary, every DRIFT/skip finding and how you resolved it, real-run conversion counts (methods/classprocs/helpers), gate results (twice each).
- Full detail on any real `.ch`-vs-`.c` bug found (not a tooling artifact) — these matter most.
- `fossil status`/`fossil extras` output confirming exactly which files changed across all 7 directories, no commit made.
- Anything that surprised you or didn't match this prompt's expectations.
