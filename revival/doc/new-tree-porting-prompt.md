# AUIS AMS/CONTRIB Porting Prompt

Fresh-instance guide for fixing compile errors in the newly activated
`AMS_ENV` and `CONTRIB` trees. Written 2026-07-05.

---

## Project context

AUIS (Andrew User Interface System) is a CMU compound-document toolkit from
the late 1980s/early 1990s. We are reviving the 6.3.1 C codebase on
macOS arm64 (Darwin/XQuartz). The canonical checkout is at:

```
~/src/AUIS/andrew-6.4/
```

The build system uses Imake. The primary build command is:

```sh
cd ~/src/AUIS/andrew-6.4
make dependInstall 2>&1 | tee dependInstall.log
```

**Never** run `make -k`; the tree is clean enough that stopping on first
real error is desirable. **Never** run concurrent builds. The log goes to
`andrew-6.4/dependInstall.log`, not `/tmp` or a scratchpad.

The main ATK tree (ez, help, all insets through figure) is **fully working**.
The AMS (mail system) and CONTRIB trees were just activated and have fresh
compile errors — your job is to fix them.

Source control is **Fossil**. Commit style: one terse line, no multi-paragraph
messages. Verify with `fossil status` before and after each commit. Group
logically related fixes into one commit.

---

## The AUIS Class system (essential background)

AUIS uses a C-based OOP system. Every object's first word is a pointer to
a `struct basicobject_methods` containing a vtable of `long (*routines[])()` —
all slots are the same untyped function pointer type. Calling a virtual method
goes through a macro that casts the slot to the specific signature needed and
calls it. This "untyped dispatch" is the source of several LP64 bugs:

```c
/* typical class-method macro expansion — the cast erases types */
#define text_CreateMark(self, pos, len) \
    ((struct mark *(*)(struct text *, long, long)) \
     (self)->methods->routines[text_CreateMark_slot])((self), (pos), (len))
```

When an `int` value (e.g., bare `-1`) is passed through this cast on arm64,
the compiler zero-extends the 32-bit `w` register to the 64-bit `x` register,
giving `0x00000000FFFFFFFF` instead of `0xFFFFFFFFFFFFFFFF`. Receivers that
check `val == -1` or `val < 0` then silently fail.

---

## The five LP64 bug variants — error patterns and fixes

### Variant 1: Missing prototype / pointer return truncation

**Compiler signals:**
```
warning: implicit declaration of function 'foo'
warning: implicit declaration of function 'foo'; returning 'int'
warning: -Wint-to-pointer-cast (assignment makes pointer from integer)
error: implicit declaration of function 'foo' is invalid in C99
```

**Root cause:** ILP32-era K&R code omitted prototypes. On arm64 (LP64),
a function with no prototype is assumed to return `int`. If it actually
returns a pointer (64-bit), the return value is truncated to 32 bits before
being stored — instant memory corruption or segfault.

**Fix:** Add a prototype with the correct return type. Either `#include` the
right header, or add a local `extern` declaration at the top of the `.c` file:

```c
extern struct foo *some_function(int arg1, char *arg2);
```

Pointer-returning functions are the **critical** case. `int`-returning
functions with no prototype are usually harmless but should still be fixed.

**Proactive grep:**
```sh
grep -n 'Wint-to-pointer-cast\|implicit declaration' dependInstall.log
```

---

### Variant 2: >8-arg untyped dispatch stack spill

**Already fixed** in `overhead/class/pp/class.c` (the classpp codegen fix,
2026-06-30). All `.c` files in the new trees use the same `class.h`/classpp
infrastructure, so this bug class does **not** reappear. No action needed.

---

### Variant 3: Bare `-1` literal through untyped dispatch (silent runtime bug)

**No compiler signal.** This is a silent runtime bug that must be found by
grep after compilation succeeds.

**Root cause:** A bare `int` literal `-1` passed as a `long` parameter through
an untyped class-dispatch macro. arm64 zero-extends the 32-bit `-1` to
`0x00000000FFFFFFFF`. Receivers that check `val == -1` or `val < 0` silently
fail — wrong behavior, no crash.

**Grep to find candidates:**
```sh
grep -rEn '\b[a-z][a-zA-Z0-9]*_[A-Z][a-zA-Z0-9]*\([^;()]*(,|\() *-1 *(,|\))[^;]*\)' \
  src/ams/ src/contrib/ --include=*.c \
  | grep -v "(long)-1\|(long) -1\|== *-1\|!= *-1"
```

**Triage each hit manually:**
1. Find the macro definition — is it an untyped `(TYPE (*)())` cast (risky)
   or a plain field-assignment or prototyped C function (safe)?
2. Find the receiver — does it consume the `-1` in a way sensitive to its
   bit pattern (`== -1`, `< 0`, arithmetic) or ignore it entirely?

**Fix:** Cast the literal: `(long)-1`

Common sentinel patterns to look for in AMS code specifically:
- Message/folder IDs that use -1 as "not found" or "end of list"
- File offset sentinels (`lseek` return value checks)
- `environ_GetProfileInt(..., -1)` — verify whether the underlying function
  is prototyped as returning `int` (safe) or dispatched through class macros (risky)

---

### Variant 4: `%d` with `long *` in scanf / `%d` with `long` in printf

**Compiler signal (when `-Wformat` is active):**
```
format '%d' expects argument of type 'int', but argument N has type 'long'
format '%d' expects argument of type 'int *', but argument N has type 'long *'
```

**Root cause:** ILP32 code where `int` and `long` were the same size. On LP64,
`long` is 64 bits but `%d` only reads/writes 32 bits.

- **scanf family** (critical): `sscanf(buf, "%d", &long_var)` writes only the
  low 32 bits, leaving upper 32 bits as stack garbage. This is memory
  corruption — the variable silently gets a wrong value.
- **printf family** (wrong output only): `printf("%d", long_val)` prints the
  truncated value. Wrong output but no memory corruption.

**Fix:**
```c
/* scanf — memory corruption risk: fix immediately */
sscanf(buf, "%d %d", &val1, &val2);   /* WRONG if val1/val2 are long */
sscanf(buf, "%ld %ld", &val1, &val2); /* correct */

/* printf — wrong output: fix in batch */
printf("%d", long_val);   /* WRONG */
printf("%ld", long_val);  /* correct */
```

**Grep for scanf risk (highest priority):**
```sh
grep -rn 'sscanf\|fscanf\|scanf' src/ams/ src/contrib/ --include=*.c \
  | grep '%d'
```

Then for each hit, check the type of the receiving variable. If it's `long`,
`off_t`, `size_t`, or a pointer-sized type — fix it.

AMS is particularly likely to have these: message UIDs, folder sizes, file
offsets, and date/time values all tend to be `long` in this codebase.

---

### Variant 5: `long`/`int` mismatch in display positioning through untyped dispatch

**No compiler signal.** Silent runtime bug in UI/view-adjacent code.

**Root cause:** View methods like `view__FullUpdate`, `view__Update`,
`lpair__VTYpe`, etc. take `long` coordinate/size parameters, but are called
with `int` locals or literals. Through untyped dispatch, upper 32 bits of
coordinates are garbage — objects draw in the wrong place or at the wrong size.

**Scope:** Only relevant in UI code (files that `#include` view.ih, graphic.ih,
lpair.ih, etc.). Pure AMS backend code (`ams/libs/`, `ams/delivery/`) is
unlikely to have this. The `atkams/` interface layer and any `ams/` UI code
may be affected.

**Approach:** After fixing Variants 1–4, do a visual smoke test. If insets or
panels render at wildly wrong positions or sizes, check coordinate variables
flowing into view dispatch calls.

---

## Related-but-distinct bug class: classpp typed-dispatch signedness mismatch

**Not one of the five LP64 variants above** — this one reproduces
identically on ILP32 too, so don't file it under LP64. Flagging it here
because it's exactly the kind of thing a fresh instance doing bulk `.ch`
conversion/typing work (which is what activating a new subtree's classes
usually involves) is positioned to reintroduce or rediscover.

**Root cause:** M1's typed-dispatch conversion generates each method's
caller-side macro by casting the vtable slot to whatever return/parameter
types the `.ch` spec declares. Nothing enforces that the `.ch` declaration
actually matches the real C implementation's type. If a `.ch` declares a
narrow return type as plain (signed) `char`/`short` but the implementation
is actually `unsigned char`/`unsigned short` (or vice versa), and that
method uses a sentinel value whose sign-extended and zero-extended bit
patterns differ (e.g. `255` as `unsigned char` vs. `-1` as signed `char`
— same bits, different comparison target), the optimizer can generate a
caller-side comparison based on the *declared* (wrong) signedness. This
silently breaks the sentinel check at higher optimization levels while
still working at `-O0` (naive truncate-then-compare masks it). No compiler
warning, no crash — just wrong behavior that looks environment/build-flag
dependent.

**No compiler signal.** Like Variant 3/5, this must be found by comparing
declarations, not by reading warnings.

**Grep/audit approach:** for every `.ch` method declared `returns char;`,
`returns short;`, `returns unsigned char;`, or `returns unsigned short;`,
find its real implementation (`grep -n "^ClassName__MethodName" *.c` in
the same directory — **resolve `ClassName` from that `.ch` file's own
`class NAME[...] : parent` line, not the filename**; several classes in
this codebase differ, e.g. `zipofcap.ch` declares `class zipofcapt`) and
check whether the explicit return-type token on the implementation's
definition line matches the `.ch` declaration's signedness. Full case
study, the scan methodology, and confirmation this was swept clean
tree-wide (566 `.ch` files, zero other instances as of 2026-07-11) are in
`porting-assessment.md` §16 and `zip-black-render-investigation.md`.

**When to re-check:** any time you bulk-convert or hand-write a `.ch`
spec's `returns` clauses for a subtree that wasn't part of M1's original
active-tree pass (per `roadmap.md` §12's census — CONTRIB and AMS
directories that were inert when M1 ran are exactly this situation), or
whenever a figure/inset/view renders wrong or paints an oversized/wrong
region only at some optimization levels.

---

## Recommended triage order

1. **Read the make log first** — `andrew-6.4/dependInstall.log` has all errors
   and warnings. Categorize by file/directory before touching any source.

2. **Fix Variant 1 errors** (implicit declarations, pointer truncation warnings)
   file by file. Rebuild each file after fixing to verify it compiles clean.

3. **Fix Variant 4 scanf** (format mismatches in scanf family). These are the
   highest-risk silent bugs in backend code.

4. **Fix Variant 4 printf** (format mismatches in printf family). Lower priority
   than scanf — wrong output, not memory corruption.

5. **Run Variant 3 grep sweep** after the tree compiles clean. Triage each hit.

6. **Variant 5** only if visual testing shows UI coordinate anomalies in new
   UI code.

---

## Build and verify workflow

After fixing a file or group of files:

```sh
# Recompile just the affected directory (faster than full make):
cd ~/src/AUIS/andrew-6.4
make dependInstall 2>&1 | tee dependInstall.log | grep -E 'error:|warning:|Error'
```

For runtime testing of AMS code, coordinate with the planner instance on
what app to launch and what to exercise. The messages app is the primary
target.

---

## Check-in conventions

- Verify with `fossil status` before staging
- Group logically related fixes (e.g., all Variant 4 scanf fixes in ams/libs/)
  into one commit
- Commit message style — one terse line:
  ```
  fix LP64 %d/%ld scanf mismatches in ams/libs
  fix implicit declarations in contrib/tm
  ```
- No multi-paragraph explanations in commit messages
- `fossil status` again after commit to confirm clean state

---

## AMS-specific notes

- `cwp_Search(..., -1, ...)` in `ams/delivery/` — the `-1` sentinel; verify
  whether `cwp_Search` is prototyped as `int`-returning (safe) or dispatched
  through class macros (Variant 3 risk)
- `environ_GetProfileInt(..., -1)` — appears in `messages/atkams/` several
  times; same triage needed
- Message body offsets and sizes: historically `long` in AMS, likely to hit
  Variant 4 scanf in message parsing code
- Date/time arithmetic: `time_t` is 64-bit on LP64 Darwin; any `int` receiving
  a `time_t` value is a latent bug even if not triggering a compiler error yet

---

## STATUS as of 2026-07-05 (`messages` is running — read this first)

The compile-error phase described above is **done**, and `messages` is
**running** — "mail (Private BB; 0 new of 0)" confirmed in the folder panel,
three-pane layout/menus/help text all rendering. All fixes described below
are committed to fossil (14 fix commits + 1 docs commit, tip
`3fe9133343bed197206cf8e362a39ff122ce19ba` at time of writing — check
`fossil timeline -n 1` for the current tip).

`contrib/tm` and `ams/msclients/vui`/`cui` were conditionalized out of the
build (`MK_TM`/`MK_VUI`/`MK_CUI`), and `contrib/bdffont` too
(`MK_BDFFONT`, missing `bdfparse.act` action file with no generator/history)
— none mechanically fixable, none needed for the GUI `messages` path.
Rationale in `porting-assessment.md` §7a/§7b.

### Runtime crash class #1 (fixed): `AndrewDir()`/`LocalDir()` missing prototype

First runtime test of `messages` segfaulted in `overhead/mail/lib/mailconf.c`'s
`CkAMSCellConfig` — `AndrewDir(NULL)`/`LocalDir(NULL)` called with no
prototype in scope, the same LP64 Variant 1 pointer-truncation bug already
fixed at 23 sites on 2026-06-30, recurring because `overhead/mail` was never
built/audited before `AMS_ENV` went on. Swept every tree touched this
session for the same bare (non-`environ_`-wrapped) call pattern; fixed 6
files total (`mailconf.c`, `ams/libs/ms/{init,hdlnew}.c`,
`atkams/messages/lib/stubs.c`, `overhead/mail/metamail/metamail/{metamail,mailto}.c`,
`overhead/eli/lib/prims1.c`). Full detail in `porting-changelog.md`'s
2026-07-05 entries. **This is done — no more files pending.**

### Runtime crash class #2 (fixed): `CUI_GetHeaders` `long`/`int` mismatch, found on a second host

Rebuilding the identical fossil tip on a second host (`spoon`) produced an
immediate real `SIGSEGV` — same tip, same source, different crash, proving
compiling+running clean on one machine isn't sufficient evidence a bug is
gone (this one is register-garbage-dependent, so it's host/binary-layout
sensitive). lldb backtrace: `CUI_GetHeaders` (`ams/libs/cui/cuilib.c`) →
`amsn__CUI_GetHeaders` → `captions_InsertCaptions`/`InsertUpdatesInDocument`
→ `folders__ReadMail`/`folders__UpdateMsgs` → `messagesapp__Start`.

Two mismatches in `CUI_GetHeaders`'s signature, both invisible at compile
time (no prototype in scope at the mismatched boundaries — this is the same
"LP64 Variant 5" family as `lpair__Init`/`style__SetNewIndentation` in
`porting-assessment.md` §12, but crossing a plain unprototyped C call, not
just a `void (*)()` vtable macro):

- `startbyte` (by value): `long` in `cuilib.c`, `int` in the `.ch`
  interfaces/`.c` implementations and the real driving variable
  (`capaux.c`'s `totalbytes`).
- `nbytes`/`status` (by pointer, more dangerous): `long *` in `cuilib.c`,
  `int *` in `ms/libs/ms/headers.c`'s `MS_HeadersSince` — the function that
  actually writes through them for the local-mailbox path.

`ams/libs/snap/cuisnap.c` (the dormant SNAP-networked variant) already had
`long`/`long *` throughout — confirms `headers.c` was the file that never got
updated, not the other way around. Fixed end-to-end: `headers.c`,
`ams.ch`/`amsn.ch`/`amss.ch`, `ams.c`/`amsn.c`/`amss.c`, `capaux.c`,
`foldaux.c`, and the dormant SNAP-server side `ams/ms/ms.c` (forward-compat,
`SNAP_ENV` is off). All rebuilt clean. Full detail in `porting-changelog.md`'s
2026-07-05 entries and `porting-assessment.md` §12. **This is done.**
**Next step:** retest on `spoon`.

**If `messages` crashes again on any host:** the methodology that found both
bugs above is the template — get a real lldb backtrace (`lldb ./messages`,
`run -d`, `bt`; if lldb reports "process is gone" with no stop reason, that's
a sign the process was kernel-killed before a debugger could attach, not a
real crash — check `~/Library/Logs/DiagnosticReports/` or retry in a plain
remote-login shell instead of a GUI-launched terminal), then read the crashing
function's source for a `long`/`int` mismatch or a missing prototype on a
pointer-returning function — both patterns have now recurred multiple times
in newly-activated code specifically because `-Wno-implicit-function-declaration`
and untyped `void (*)()` dispatch make them silent at compile time.

---

## Where things live

```
src/ams/            — AMS mail backend (libs/, delivery/, ms/, etc.)
src/atkams/         — ATK/AMS interface layer (what messages calls into)
src/messages/       — messages app UI
src/contrib/        — contributed code (varied, check subdirs)
revival/doc/        — porting docs (porting-changelog.md has full fix history)
revival/doc/roadmap.md  — current status and near-term plan
dependInstall.log   — build output (in andrew-6.4/, not scratchpad)
```

The porting-changelog.md in `revival/doc/` has the detailed history of every
fix applied to the main ATK tree, with before/after diffs — a useful reference
when a new error pattern looks familiar.
