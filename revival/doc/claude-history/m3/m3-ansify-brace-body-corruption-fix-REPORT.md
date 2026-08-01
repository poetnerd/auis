# `ansify` fix: brace-glued-to-local-variable-declaration corruption

## 1. Status

Stopped at the task's single gate (per the prompt's own "Gate"
section): fix applied, own scratch verification passes (all three
cases), both target directories gate clean twice each. **No fossil
commit made.**

## 2. What I did, in order

1. Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
   starting, per the prompt's own instruction.
2. Located the exact code in `revival/tools/ansify` (`parse_decl_block`
   at line 206, the two `convert_file` call sites at lines 318 and
   382-383) and confirmed it matched the prompt's diagnosis
   byte-for-byte before touching anything.
3. Applied the fix exactly as specified in the prompt: added
   `needs_brace_split` as a third element of `parse_decl_block`'s
   return tuple (`True` only on the brace-glued-parameter path, `False`
   on the plain bare-brace path), updated both `convert_file` call
   sites to destructure and use the 3-tuple. No re-derivation — applied
   the prompt's own diff verbatim.
4. Ran my own scratch verification (three synthetic cases, via
   `importlib.machinery.SourceFileLoader` to import `ansify` as a
   module and call `convert_file` directly — same method the prompt
   describes) **before touching anything under `src/`**:
   - `case1_corruption.c` — brace + local var decl on one line
     (`{ register int result = 0;`) — **fixed**: local declaration
     preserved byte-for-byte in the converted output.
   - `case2_plain_brace.c` — plain `{` alone on its own line —
     unaffected, as expected.
   - `case3_sig_glued.c` — signature-glued last param + brace
     (`int b; {`) — **still works**: brace still split onto its own
     line, matching this morning's original fix target.
   - Additionally compiled the converted `case1` output under the
     project's exact flags (`-std=gnu89 -Wno-implicit-int
     -Werror=implicit-function-declaration
     -Wno-incompatible-function-pointer-types -Wno-return-type`) —
     compiles clean (exit 0). Also compiled a hand-built simulation of
     the *pre-fix* corrupted output (declaration line squashed away)
     under the same flags to confirm the regression's failure mode is
     real: `error: use of undeclared identifier 'result'` (x2), exit 1.
   These four checks together are stronger than the prompt strictly
   asked for (it asked for behavioral confirmation of the three cases;
   I added the compile-gate proof on both sides of the fix for my own
   confidence) but didn't require re-deriving any part of the fix
   itself.
5. Checked `fossil status`/`fossil timeline -n 15`: M3 batch B3 is
   already committed (`40d79265c3`, 2026-07-30 20:13:02, followed by a
   docs-only commit `7adca71a6e` at 20:15:02, now `*CURRENT*`). Only
   `revival/tools/ansify` shows `EDITED`. This means `atk/apt/suite`
   and `atk/apt/tree` are in their final, already-fixed-up-with-the-
   manual-stopgap state, matching the prompt's "if not yet committed"
   branch's *else* case.
6. Ran `python3 revival/tools/ansify --dir src/atk/apt/suite --dry-run`
   and the same for `src/atk/apt/tree`, and compared against
   `m3-b3-leaf-dirs-REPORT.md`'s documented final state (§5's
   conversion-count table). See §3 below for the comparison.
7. Ran the subtree-local gate (`make -C <path> clean`, then `depend`,
   then `-k install`, separate calls, absolute paths, not chained)
   twice each for `atk/apt/suite` and `atk/apt/tree`. All four install
   cycles: `grep -c "error:" dependInstall.log` → `0`. See §4.
8. `fossil status` — confirmed only `revival/tools/ansify` changed, no
   commit made. `fossil diff > m3-ansify-brace-body-corruption-fix-
   session.diff` written to the tree root (77 lines).

## 3. `atk/apt/suite` / `atk/apt/tree` dry-run comparison against B3

B3's report (`m3-b3-leaf-dirs-REPORT.md` §5) documents the *final*
per-directory conversion counts after all of B3's own fixes (including
the manual brace-split stopgap for the corruption shape this task
fixes properly):

| Directory | Files | methods/classprocs/helpers (final) | Gate x2 |
|---|---|---|---|
| `atk/apt/suite` | 6 | 103/8/92 | clean |
| `atk/apt/tree` | 2 | 68/6/61 | clean |

Since B3 is already committed, these directories are already in their
final, fully-converted state — a dry-run today should find **nothing
left to convert**, and that's exactly what happened:

`atk/apt/suite --dry-run`:
- All 6 files report "no K&R definitions converted".
- 4 DRIFT findings (`suite__InitializeObject`, `suite__FinalizeObject`,
  `suitecv__InitializeObject`, `suiteev__InitializeObject` — all
  `.c has N params, .ch has M+1` param-count mismatches).
- 2 skips (`suite__GetActiveItemCaptionColor`,
  `suite__GetPassiveItemCaptionColor` — "no signature in DB").
- `6 file(s); 0 compile failure(s); 4 DRIFT finding(s)`.

`atk/apt/tree --dry-run`:
- Both files report "no K&R definitions converted".
- 2 skips (`tree__SetNodeModified`, `tree__NodeModified` — "no
  signature in DB").
- `2 file(s); 0 compile failure(s); 0 DRIFT finding(s)`.

I checked whether these specific DRIFT/skip names appear anywhere in
B3's report as items B3 itself fixed or flagged as new — they don't
(grepped for each function name and for "no signature in DB");
`InitializeObject`/`FinalizeObject` param-count DRIFT and "no
signature in DB" skips are both long-standing, unremarkable categories
seen throughout M1-M3 (not specific to this directory or this fix),
consistent with B3 having left them as expected, harmless, permanent
skips rather than something newly broken by today's ansify change.
Zero new conversions + the same unremarkable DRIFT/skip categories is
exactly the expected outcome for a directory already at its final
state — this confirms the fix doesn't change behavior for code that
doesn't hit the corruption shape.

## 4. Gate results (both directories, twice each)

All four cycles used separate, unchained `Bash` calls with absolute
`make -C <path>` invocations, output to
`~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten each run, per
convention), checked with `grep -c "error:"`.

| Directory | Cycle | clean | depend | install (`-k`) | `error:` count |
|---|---|---|---|---|---|
| `atk/apt/suite` | 1 | ok | ok (benign makedepend system-header warnings only, matches B3 §5's documented false positive) | exit 0 | 0 |
| `atk/apt/suite` | 2 | ok | ok | exit 0 | 0 |
| `atk/apt/tree` | 1 | ok | ok | exit 0 | 0 |
| `atk/apt/tree` | 2 | ok | ok | exit 0 | 0 |

Both directories gate clean, twice each.

## 5. `fossil status`

```
EDITED     revival/tools/ansify
```

Only `revival/tools/ansify` changed. No commit made. Checkout remains
at `7adca71a6e` (the same commit it started at).

## 6. Files touched

`revival/tools/ansify` only — Python script, not compiled; verified
instead via the three scratch cases in §2 plus the two directories'
gates in §4. No `src/` files touched (the task explicitly says this is
a tool fix, not a batch — `atk/apt/suite`/`atk/apt/tree` source was
only read/dry-run-tested, never written).

## 7. The exact diff to `revival/tools/ansify`

```diff
Index: revival/tools/ansify
==================================================================
--- revival/tools/ansify
+++ revival/tools/ansify
@@ -203,11 +203,18 @@
 RESERVED = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'do', 'else'}
 
 
 def parse_decl_block(lines, start):
     """Consume K&R parameter declarations from lines[start:]; return
-    (index_of_brace_line, decl_text) or None if this isn't a K&R body.
+    (index_of_brace_line, decl_text, needs_brace_split) or None if this
+    isn't a K&R body. needs_brace_split is True only when the returned
+    line is a brace-glued last-parameter declaration (safe to rewrite to
+    a bare '{' once the candidate is accepted, since its text was already
+    captured into decls); False when the line was matched by the plain
+    bare-brace check, which may have arbitrary trailing body content
+    (e.g. a local variable declaration sharing the line with the brace)
+    that must never be touched.
 
     Does not mutate `lines` -- a brace-glued line (last param declaration
     and the opening brace on one physical line) is only *identified* here;
     the caller rewrites it to a bare '{' itself, and only once it has
     committed to actually using this candidate (see convert_file). This
@@ -219,11 +226,11 @@
     i = start
     decls = []
     while i < len(lines):
         s = lines[i].strip()
         if s.startswith('{'):
-            return i, '\n'.join(decls)
+            return i, '\n'.join(decls), False
         if s == '' or s.startswith('/*') or s.startswith('*'):
             i += 1
             continue
         if DECL_LINE.match(lines[i].rstrip('\n')):
             decls.append(s)
@@ -230,11 +237,11 @@
             i += 1
             continue
         bg = BRACE_GLUED.match(lines[i].rstrip('\n'))
         if bg:
             decls.append(bg.group('decl').strip())
-            return i, '\n'.join(decls)
+            return i, '\n'.join(decls), True
         return None
     return None
 
 
 def parse_local_decls(decl_text):
@@ -313,11 +320,11 @@
         if not cand:
             out.append(line)
             i += 1
             continue
 
-        m, ret, prev_type, (brace_idx, decl_text) = cand
+        m, ret, prev_type, (brace_idx, decl_text, needs_brace_split) = cand
         fname = m.group('name')
         params = [p.strip() for p in m.group('params').split(',') if p.strip()]
 
         new_hdr = None
         clsm = re.match(r'^(\w+)__(\w+)$', fname)
@@ -377,11 +384,11 @@
         out.append(new_hdr)
         # A brace-glued decl block (last param and '{' on one physical
         # line) is only rewritten to a bare '{' now that this candidate is
         # definitely being used -- see parse_decl_block's docstring for why
         # this can't happen speculatively.
-        if lines[brace_idx].strip() != '{':
+        if needs_brace_split:
             lines[brace_idx] = '{'
         i = brace_idx           # skip the K&R decl block entirely
         changed = True
 
     if not changed:
```

(Identical content to `m3-ansify-brace-body-corruption-fix-session.diff`
in the tree root.)

## 8. Anything that surprised you / didn't match this prompt's expectations

Nothing surprising or mismatched. Everything matched the prompt's
diagnosis and prescribed fix exactly:
- The code at all three sites matched the prompt's quoted excerpts
  verbatim before I touched anything.
- All three scratch cases behaved exactly as predicted.
- B3 turned out to already be committed (one of the two branches the
  prompt anticipated), so the dry-run comparison was against the
  directories' true final state rather than a pre-commit intermediate
  one — a slightly cleaner check than the alternative branch would have
  given, since "zero new conversions" is an unambiguous signal.
- Both directories gated clean on the first try, twice each, with no
  fallout to chase.
