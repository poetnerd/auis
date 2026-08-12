# `ansify` fix: brace-glued-to-local-variable-declaration corruption (regression in this morning's own fix)

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before starting (same hard rules as every M3 task: no fossil commits ever, stop at the gate, write `m3-ansify-brace-body-corruption-fix-session.diff` in the tree root and `m3-ansify-brace-body-corruption-fix-REPORT.md` in `revival/doc/claude-history/`, command style for an unattended session). This is a fix to `revival/tools/ansify` itself, **not** an ansify batch on new directories. Read `m3-rollout-runbook.md`'s "ansify brace-glued parser fix + retrospective recheck" entry (2026-07-30) for background on the *original* brace-glued bug and fix before touching anything — this task fixes a narrow regression introduced by that same fix, found later the same day during M3 batch B3 (`m3-b3-leaf-dirs-REPORT.md`, §4.5).

## The bug, already fully diagnosed and the fix already verified end-to-end — do not re-derive

This morning's brace-glued-parameter fix added a two-step design to `parse_decl_block`/`convert_file`: `parse_decl_block` *identifies* a brace-glued last-parameter line without mutating anything, and `convert_file` rewrites that specific line to a bare `{` only once the candidate is accepted:

```python
        if lines[brace_idx].strip() != '{':
            lines[brace_idx] = '{'
```

This condition is too broad. `parse_decl_block` can return `brace_idx` through **two different paths**, and this code can't tell them apart:

1. The **new** branch (the actual brace-glued-parameter fix target): a line like `struct point * LogicalPoint; {` — the declaration text was captured into `decls` before returning, so squashing this line down to a bare `{` is correct and lossless.
2. The **original, pre-existing** bare-brace check (`if s.startswith('{'): return i, '\n'.join(decls)`), which fires for *any* line starting with `{` — including a line where the opening brace is immediately followed, on the same physical line, by the function body's **first local variable declaration**: `{ register struct suite *self = NULL;`. Nothing about this line was ever captured into `decls` — it's real body code that happens to share a line with the brace.

Since the `!= '{'` check can't distinguish these two cases, case 2 gets squashed exactly like case 1 — **silently deleting the local variable declaration**, leaving every later reference to that variable genuinely undeclared.

**This is confirmed, not theoretical** — reproduced in isolation with a synthetic helper function:
```c
static int helper_func(a, b)
int a;
int b;
{ register int result = 0;
    result = a + b;
    return result;
}
```
converts (with today's fix, before this correction) to:
```c
static int helper_func(int a, int b)
{
    result = a + b;
    return result;
}
```
— `result` silently undeclared. Compiling this under the project's exact flags (`-std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type`) produces a hard, unsuppressible `use of undeclared identifier 'result'` error — confirmed directly, not assumed. (This is why the bug is safe for already-committed code: `ansify`'s own per-file compile gate always catches it and reverts the whole file — it can never silently land. But it does mean any file with this shape gets its *entire* conversion attempt thrown away rather than partially succeeding, which is exactly what forced M3 batch B3 to hand-fix 32 instances across two files as a manual stopgap. See `m3-b3-leaf-dirs-REPORT.md` §4.5 for the batch that found this.)

## The fix — already designed and verified end-to-end, apply as specified

The real fix: `parse_decl_block` should tell its caller *which* path produced the returned `brace_idx`, so `convert_file` only squashes the line when it's actually safe to. Add a third element to the returned tuple — a boolean, `True` only for the brace-glued-parameter branch, `False` for the plain bare-brace check:

```python
def parse_decl_block(lines, start):
    """Consume K&R parameter declarations from lines[start:]; return
    (index_of_brace_line, decl_text, needs_brace_split) or None if this
    isn't a K&R body. needs_brace_split is True only when the returned
    line is a brace-glued last-parameter declaration (safe to rewrite to
    a bare '{' once the candidate is accepted, since its text was already
    captured into decls); False when the line was matched by the plain
    bare-brace check, which may have arbitrary trailing body content
    (e.g. a local variable declaration sharing the line with the brace)
    that must never be touched.
    """
    i = start
    decls = []
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith('{'):
            return i, '\n'.join(decls), False
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
            return i, '\n'.join(decls), True
        return None
    return None
```

Two call sites in `convert_file` need to change to match the new 3-tuple:

```python
        m, ret, prev_type, (brace_idx, decl_text, needs_brace_split) = cand
```
(was: `(brace_idx, decl_text) = cand`)

```python
        if needs_brace_split:
            lines[brace_idx] = '{'
```
(replaces: `if lines[brace_idx].strip() != '{': lines[brace_idx] = '{'`)

There are exactly these two call sites plus the `parse_decl_block` definition itself — confirmed by grepping the whole file for `parse_decl_block`/`brace_idx`, nothing else references either.

**Already verified end-to-end** (not just reasoned about): patched a scratch copy of the real tool and ran `convert_file` directly against three synthetic cases:
1. The corruption repro above (brace + local var decl) — **fixed**, local declaration now preserved byte-for-byte.
2. A plain `{` alone on its own line (with trailing whitespace) — unaffected, as expected.
3. A signature-glued case (`int a; {` — the *original* brace-glued-parameter bug this morning's fix targets) — **still works correctly**, brace still gets split onto its own line. No regression to the case this morning's fix was written for.

Trust this design; spend your verification effort on applying it correctly and on the retrospective check below, not on re-deriving it.

## Why no retrospective recheck of already-committed directories is needed

Already established and worth restating so you don't redo this analysis: this corruption is **always** caught by `ansify`'s own per-file compile gate (`use of undeclared identifier` is a hard, unsuppressible error under every flag this project uses — confirmed by direct compilation, not assumed) — so it can never silently reach committed code. It can only ever manifest as a *reverted file* (an entire file's conversion thrown away, not corrupted), which would show up as an unexplained "COMPILE FAILED — restoring" in a batch's own report. `atk/basics/common`, `atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`, and `overhead/class/testing` (the 7 directories from this morning's retrospective recheck, the only ones that ran with the vulnerable code — B1 and B2 themselves ran *before* this morning's fix existed, so they were never exposed at all) were all independently re-verified clean with expected file counts and no unexplained reversions. **Do not re-run or re-gate those 7 directories as part of this task** — there is nothing to find there.

## Task

1. Apply the fix exactly as specified above to `revival/tools/ansify`.
2. Confirm your own scratch reproduction of both the original corruption (now fixed) and the signature-glued case (still working) before touching anything under `src/`, same as the three cases already described — this is a cheap, fast confirmation, do it before moving on.
3. Rebuild is not applicable — `ansify` is a Python script, not compiled.
4. **Regression-check against the two directories that most recently exercised the vulnerable code path**: `atk/apt/suite` and `atk/apt/tree` (M3 batch B3, committed... check `fossil status`/timeline for whether B3 has been committed yet by the time you run this — if not yet committed, these are the two directories with the manual "split the glued brace in the original source" stopgap already applied for the corruption cases found there). Run `ansify --dry-run --dir` on both and confirm the dry-run output is identical to what B3's own report documents (same conversion counts, same DRIFT/skip findings) — this doesn't prove anything new was broken, but confirms the fix doesn't change behavior for a directory that's already known-good under the current (pre-fix) tool state on the parts that don't involve this exact corruption shape.
5. Subtree-local gate for `atk/apt/suite` and `atk/apt/tree` only (not all 7+13 directories — this is a narrow correctness fix, not a rollout batch): `make -C <absolute-path> clean`, then `depend`, then `-k install` — separate calls, absolute paths, not chained, twice each for determinism.

## Gate

**Do NOT commit.** Stop after the fix is applied, your own scratch verification passes, and both directories' gates are green (twice each). Say you have stopped.

## Report

Write `m3-ansify-brace-body-corruption-fix-REPORT.md` per `sonnet-playbook.md`'s standard format, plus:
- The exact diff to `revival/tools/ansify`.
- Your own confirmation of all three scratch test cases (corruption case fixed, plain-brace case unaffected, signature-glued case still works).
- The `atk/apt/suite`/`atk/apt/tree` dry-run comparison against B3's documented findings.
- Gate results for both directories, twice each.
- `fossil status` confirming only `revival/tools/ansify` changed, no commit made.
- Anything that surprised you or didn't match this prompt's expectations.
