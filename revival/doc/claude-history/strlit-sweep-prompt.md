# Task: writable-string-literal sweep

Read `revival/doc/sonnet-playbook.md` first; its rules apply.

## The bug class

1991 code freely mutated caller-supplied strings in place; 1991
compilers put string literals in writable data, so passing a literal
to such a function worked. Modern clang puts literals in read-only
`__cstring`, so the first `*p = '\0'` on a literal is an instant
EXC_BAD_ACCESS (code=2, a write-protect fault at a `strb`
instruction; lldb `register read` shows the pointer resolves to a
perfectly readable C string).

**Fixed exemplar** (fossil commit `f91cb255`, 2026-07-18):
`atk/basics/common/atomlist.c` `atomlist__StringToAtomlist` tokenized
dotted names ("buttonV.label", passed as a literal by options code)
by NUL-ing each '.' in place — and restoring it afterward, so 35
years passed without symptoms until literals became read-only. The
fix pattern: **make the callee parse a private malloc'd copy** (copy
in, tokenize the copy, free it). Fixing the callee heals every call
site at once. Do not "fix" call sites by copying at the caller.

## Gate 1 — census only, no code changes

Find more members of the class. Suggested hunting patterns (refine as
you go; these are starting points, not an exhaustive spec):

- Functions that compute a pointer into a `char *` parameter and then
  store through it. High-signal greps across `src/` (exclude
  `contrib/`, `build/`, vendored code):
  - `index(`/`strchr(` result later assigned `'\0'` or another char;
  - `strtok(` on a parameter (strtok mutates its argument);
  - loops doing case-folding or character replacement through a
    parameter (`*s = tolower`, `*p = '.'`, `*p = c`).
- For each candidate function, decide whether any call site can pass
  a string literal (grep the function name; look for `"`-quoted
  arguments, or arguments that trace back to `environ_GetProfile`
  defaults which are often literals).

Produce in the report a table: file:line, function, what it mutates,
call-site evidence that a literal (or other read-only memory) can
reach it, and your severity judgment. Sort by likelihood of being hit
in the apps we actually run (ez, messages, cui, help). If a function
mutates its argument *as its documented contract* (the caller expects
the modification), list it separately — those need caller-side
thought and are not yours to fix.

STOP at Gate 1 and wait for review of the list.

## Gate 2 — fixes (only for entries approved at Gate 1)

Apply the private-copy pattern per the exemplar. For each file:
match the file's existing style; add a brief standalone comment
explaining why the copy exists (see atomlist.c's comment for tone —
it explains callers pass literals now in read-only memory, nothing
about milestones or docs). Compile-verify each touched file via its
subtree `make`, then `make install` for affected subtrees per the
playbook's relink notes. Run any of the suites that exercise touched
code. Report per-file compile status. No commits; end with
`fossil diff > strlit-session.diff` and the report.
