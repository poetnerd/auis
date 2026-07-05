# AUIS runtime debugging guide

Reusable methodology for investigating runtime bugs (crashes, hangs, wrong
rendering, wrong behavior) in this AUIS revival on macOS arm64. Distilled
from two sessions on 2026-07-04: tracking down a `help` list-panel scroll
bug down to an LP64 zero-extension bug, a follow-up audit that found and
fixed six sibling instances, and stumbling onto a live Xlib deadlock along
the way. None of this is bug-specific — it's what made those
investigations slow the first time, so the next one doesn't have to
relearn it. See memory `project_helpa_scrollpos_bug`,
`project_xlib_display_lock_deadlock`, and `feedback_lldb_debugging_workflow`
for the specific findings this guide was extracted from.

## Investigation discipline

- **Confirm the root cause via lldb before changing any code.** A theory
  that "looks right" from reading source is not confirmed until you've
  seen the actual value/call stack at runtime — static reading alone
  produced a wrong "ruled out" conclusion in the case this guide comes
  from.
- **Verify a candidate fix at runtime before editing source.** Inject it
  with `expr`/register writes, re-trigger the symptom, confirm it's gone
  — *then* make the real source change.
- For anything UI-visible, get the user's plain-language read ("still
  wrong" / "looks right") rather than asking them to relay raw values —
  you can get those yourself over lldb.
- Fix at the narrowest correct scope. A blunt fix that clears state in a
  shared code path (e.g. disabling a whole feature for every caller to
  make one caller's bug go away) proves the mechanism but isn't the real
  fix — trace back to the actual source and fix it there.
- Compile-verify each touched file individually before considering a fix
  done, not just the final link.
- Rebuild only the affected library/`.do` module and reinstall properly;
  relink the app binary only if a *statically* linked library changed.
- Leave the Fossil commit to the user unless they've asked you to do it.
  Record what you found/fixed in the project roadmap and durable lessons
  to memory before you're done — the value of a hard-won debugging
  session is largely lost if the next session has to rediscover it.

## Environment basics

- App binaries at `build/bin/<app>` (e.g. `build/bin/ez`, `build/bin/helpa`
  if present) are symlinks to `build/bin/runapp`, which picks its app
  identity from `argv[0]` or an explicit app-name argument
  (`runapp helpa -d`, not `runapp -d helpa` — `-d` is parsed by the app
  layer, and the wrong order forks). Prefer invoking via the symlink or
  the documented argument order.
- `DISPLAY=:0;` (semicolon terminator) before an X11 command, not
  `DISPLAY=:0 cmd` inline — the inline form doesn't reliably propagate in
  this shell setup.
- Install built artifacts with `install -m 755 src dst`, never a plain
  `cp` over an existing binary — in-place `cp` invalidates macOS's
  codesign vnode cache and the next launch gets `SIGKILL`ed with no
  useful error.
- **Single-instance forwarding gotcha:** a stray process from an earlier
  session can silently hijack a fresh launch — you'll see "Sent request
  to existing \<app\> window", the new process exits immediately, and any
  breakpoints you set never fire. Check `ps -ef | grep '<app> -'` for a
  leftover process *before* concluding a breakpoint "isn't hit."

## lldb cookbook

**No debug info by default.** This build's `CDEBUGFLAGS` is empty —
`build/bin/runapp` has zero `__DWARF` content (check with `otool -lv` /
`dwarfdump`). To get source-level breakpoints/variable inspection on a
specific file:

1. Find its real compile command (`cd src/atk/<subsys> && rm -f foo.o &&
   make -n foo.o`), copy it, and add `-g -O0`. **`-O0` is not optional** —
   under the default `-O`, `frame variable`/`expr` frequently report
   "variable not available" or fail with no visible reason, which looks
   exactly like a silent/hung breakpoint if you're not watching for the
   error text.
2. Rebuild the containing artifact and reinstall:
   - Static library (`.a`): rebuild via that source dir's own `make
     libfoo.a` (their Makefiles already do `rm $@ && ar ... && ranlib` —
     safe to call directly; never `ar clq` an existing archive by hand,
     it clobbers every other object in it), copy to `build/lib/atk/`,
     `ranlib`, then relink the app (`make runapp` in `src/atk/apps/`,
     `install -m 755`).
   - Dynamically-loaded module (`.do`): check the subsystem's `Makefile`
     for `all:: foo.do` (vs. a `LibraryTarget`) — rebuild with `make
     foo.do`, then `install -m 755 foo.do build/dlib/atk/foo.do`. No
     relink needed.
   - Grep the target `Makefile` for both patterns before assuming which
     one applies; get it wrong and you'll rebuild the wrong thing.
   - **Check `nm -g build/bin/runapp | grep <symbol>` before trusting a
     `.do`-only rebuild** — a class can have a `.do` target in its
     Makefile that's dead weight because the class is *also* statically
     linked (confirmed for `text`, `simpletext`, `textview`, `matte` —
     unlike `help`/`figure`, which genuinely are dynamic; see
     `project_runapp_static_link` memory). Rebuilding/installing such a
     `.do` is a silent no-op; `T <symbol>` in the `nm` output means you
     must relink `runapp` instead. Losing sight of this cost a multi-hour
     debugging detour on 2026-07-04.
3. macOS doesn't embed DWARF in the final linked binary — it uses a
   debug-map (stabs) pointing back at the `.o` files on disk. As long as
   the `.o` stays where it was built, lldb resolves file:line breakpoints
   fine against the otherwise-symbol-only binary.
4. **Class-dispatch macros aren't necessarily callable via `expr`.**
   Before assuming `expr (void)some_Macro(...)` will work, check with
   `nm build/bin/runapp | grep <name>`:
   - Real symbol found (`T <name>`, typically the two-underscore
     `class__Method` form): callable, but needs an explicit
     function-pointer cast since there's no prototype in scope, e.g.
     `expr (void)((void(*)(void*,long))some_class__Method)(self, -1)`.
   - Nothing found: it's a pure inline accessor macro (e.g.
     `mark_SetPos(self,pos)` is literally `((self)->pos = position)`, no
     linkable symbol at all). Read the macro body in its `.ih` file and
     read/write the real struct field directly instead.
5. **Force a pty**: `script -q <a-real-file.log> lldb ...` — a real file
   path, not `/dev/null` (behaves differently, breaks this) and not plain
   `>` redirection (lldb full-buffers stdout when it detects a non-tty,
   so per-breakpoint auto-command output can sit unflushed until the
   process exits, making working commands look like they silently
   vanished).
6. **A failed `expr`/`frame variable` aborts the rest of that
   breakpoint's queued commands** ("Aborting reading of commands after
   command #N"). If a breakpoint's commands seem to produce nothing,
   re-run with one cheap command first (`register read x0`) to confirm
   the breakpoint fires at all, then add the suspect command back and
   look for its actual error text before concluding it's a buffering
   issue.
7. `breakpoint set --name X`, then `breakpoint command add N` /
   `<commands...>` / `DONE`, ending the block with `continue`,
   auto-continues through repeated hits unattended — good for confirming
   a value across an entire run without babysitting each stop.
8. **A statically-linked function's own `fprintf` can stay completely
   silent even when `lldb` proves the function executes** (breakpoint
   hits repeatedly, sane backtrace and register state, no crash). Hit
   this 2026-07-04 debugging `matte__Create`: instrumented, rebuilt,
   relinked, confirmed the marker string was in the binary via `strings`
   — zero output across many runs, while an `lldb` breakpoint on the
   same symbol fired every time. Root cause not fully identified. Don't
   treat "no fprintf output" as proof a function never ran; confirm with
   a breakpoint first.
9. **Reading struct fields with zero debug info, without a `-g -O0`
   recompile**: `expr self->field`/`frame variable` need type info this
   build doesn't have and fail outright, even for a live, correctly-typed
   pointer. Instead: `register read x0` (or x1/x2 for the 2nd/3rd arg,
   per AAPCS64) for the raw pointer, then `memory read -s8 -fx -c<N>
   $x0` for a raw word dump. To know which field is which without an
   offset table, find a **known-good anchor value** in the dump — a
   pointer you already captured elsewhere (e.g. a `dataobject` address
   logged by a *working* `fprintf`) or a `#define`d constant matching one
   of the struct's fields — then count fields backward/forward from that
   anchor using the struct's declared order in its `.ih`/`.ch`. This is
   how the `figview` `originx` corruption (`0x100000000` instead of `0`)
   was found — see `project_figure_status` memory.

## Killing processes cleanly

Prefer telling lldb to kill its own inferior over a raw `kill -9`:

- lldb still attached: `kill <lldb-pid>` (plain SIGTERM, not `-9`). This
  often orphans the debuggee (reparented to init) rather than killing it
  — check for the orphan next.
- Orphan present: `lldb -b -o "process attach --pid <pid>" -o "process
  kill" -o "quit"` — a debugger-driven kill, not a raw signal.
- A process stuck in macOS's `UE` state (uninterruptible sleep, "trying
  to exit") ignores every signal including `-9`, and `lldb process
  attach` fails with "could not pause execution" (`sample <pid>` hangs
  too — don't bother). If it's holding a dead XQuartz/X11 connection,
  restarting XQuartz frees *new* launches from single-instance forwarding
  even though the wedged process itself lingers harmlessly — don't spend
  more time on it than that.
- This codebase's apps do **not** persist state across invocations (no
  save-file, no restored-position convention) — if behavior seems
  non-deterministic run to run, suspect lldb/output-capture flakiness
  (above) before inventing a "restored from disk" theory.

## Inspecting an already-hung process without killing it

**Don't send SIGINT to a batch-mode (`-s`/`-o`) lldb session that has
already exhausted its scripted commands** to try to interrupt a running
process and inspect it live — once the script is exhausted and input
hits EOF, lldb's implicit quit **kills the inferior as a side effect**,
destroying the very hang state you wanted to inspect.

**Instead:** if the target is already stuck (confirmed via `ps` — still
alive, making no progress), just attach fresh:
```sh
lldb -b -o "process attach --pid <pid>" -o "thread backtrace all"
```
No `run` needed, no interrupt needed — attaching to an already-blocked
process shows its current state immediately and safely. Don't add
`-o "quit"`/`-o "continue"` unless you're sure you want to end the
session; a bare attach + backtrace leaves the process paused (not
killed) for further inspection. `process kill` explicitly and separately
once you're actually done with it.

## The LP64 bug-class checklist

This codebase's class-dispatch macros almost universally use an
**untyped** function-pointer cast: `((*((RETTYPE (*)())(...)))(args...))`
— no parameter types declared. On LP64 (64-bit `long`, still 32-bit
`int`), a bare `int` constant argument (most often a literal `-1` used as
an "unset"/"do everything" sentinel) is not sign-extended to 64 bits at
the call site, and the callee's `long` parameter reads garbage — visibly,
a huge value near a power of two (e.g. `4294967295` = `0xFFFFFFFF`
zero-extended, or similar with garbage upper bits). This exact bug class
has been found and fixed **7 times** across this revival so far (see
`project_classprocedure_truncation_bug` and `project_helpa_scrollpos_bug`
in memory for the full list). If you hit an unexplained wrong-branch or
wrong-value bug and see anything like this in lldb, check this pattern
first:

1. Find the suspicious `long` value (register or `frame variable`); if
   it's implausibly large / near a power of two, suspect this bug class
   immediately.
2. Trace it back to where it was set — a `#define` constant or a bare
   call-site literal (usually `-1`) — and confirm the macro it passed
   through is untyped dispatch (check the `.ih` for `(TYPE (*)())`).
3. Confirm the *receiver* actually depends on the exact value (a sign
   check `< 0`/`== -1`, or arithmetic) — if the parameter is ignored, or
   only used in a way insensitive to corruption, it's not an observable
   bug even though the same mechanism is technically present.
4. Fix: cast the literal at the call site, e.g. `text_CreateMark(self,
   (long)-1, 0)`. **This cast is a complete no-op on 32-bit (ILP32)
   platforms** — `int` and `long` are the same size and bit pattern
   there, so it only changes (correctly) how the value crosses the
   untyped dispatch boundary on LP64. Safe to apply liberally, including
   to code paths you can't easily runtime-verify.
5. Consider sweeping for siblings. A workable grep to narrow a broad
   search down to a manually-triageable list:
   ```sh
   grep -rEn '\b[a-z][a-zA-Z0-9]*_[A-Z][a-zA-Z0-9]*\([^;()]*(,|\() *-1 *(,|\))[^;]*\)' \
     src/ --include=*.c | grep -v "(long)-1\|(long) -1\|== *-1\|!= *-1"
   ```
   This found 22 candidates from ~925 raw `-1`-near-parens hits in one
   sweep. Triage each by steps 2-3 above — several call sites pass the
   same untyped mechanism but the receiver ignores the value entirely
   (confirmed harmless), so don't fix mechanically without checking the
   receiver.

## Fossil commit convention

One short line, no paragraph, matching existing timeline style:
```
fossil commit -m "fix <app> <short description>"
```
