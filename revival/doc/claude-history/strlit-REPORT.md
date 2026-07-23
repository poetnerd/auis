> **STATUS: superseded as an action item, 2026-07-23.** Root cause
> turned out to be simpler than a per-site sweep: `config/darwin/system.mcr`
> never set `-fwritable-strings`, and Apple clang (unlike real gcc since
> 4.0) still implements it. Added `CC = cc -fwritable-strings`, did a
> full clean rebuild (confirmed the flag reached all 893 compiles), and
> verified live (`char *p = "hello"; p[0]='H';` bus-errors without the
> flag, clean with it). A follow-up `-Wwrite-strings` compile-only scan
> found 26,628 literal→`char*` sites tree-wide (13,937 inert generated
> classheader boilerplate, 12,691 in hand-written code across 444 files,
> mostly static descriptor/keybinding tables that are never mutated).
> None of it is live risk with the flag in place. Full writeup:
> `revival/doc/porting-assessment.md` issue #1. The per-function detail
> below remains useful only if the flag is ever dropped.
>
> **Belt-and-suspenders update, same day:** the three confirmed
> reachable-by-literal call sites below (§1 help/`WMDEFAULTFILE`, §2
> cui/`~/.cuirc`, §5 fdbbdf/`hexout("")`) were fixed directly — literal
> replaced with a writable local copy at each call site, atomlist-style
> — despite not being required now that the flag covers them. Compiled
> and relinked clean (`helpa.do`, `cuin`, `fdbbdf`). §3 (`ProcessCommand`)
> and §4 (`HandleAddress`) were left as-is: no literal reaches them
> today, so there's nothing concrete to fix yet.

# Writable-string-literal sweep — Gate 1 report (Sonnet session, 2026-07-23)

Task: `strlit-sweep-prompt.md` (then in `revival/doc/`, retired
alongside this report to `claude-history/` once the task closed).
Gate 1 only: census, no code changes. Stopped at Gate 1 per the
prompt.

## Method

Read the fixed exemplar (`atk/basics/common/atomlist.c`
`atomlist__StringToAtomlist`, fossil `f91cb255`) for the target
pattern: a function computes a pointer into a `char *` parameter and
stores through it, and some call site can hand it a string literal
(read-only on modern clang/macOS).

Grepped `src/` (excluding `contrib/`) for three families:
- `strtok(` on a parameter — zero hits outside `contrib/`.
- `index(`/`strchr(` results later assigned `'\0'` or another char —
  ~150 raw hits.
- in-place case-folding (`tolower`/`toupper` through a pointer) and
  literal single-char replacement loops — ~200 raw hits.

Wrote a small Python filter (matches the mutated variable against the
enclosing K&R parameter-declaration block, restricted to `char *`
params) to cut the ~350 raw hits down to candidates where the mutated
pointer traces to a real function parameter rather than a local
stack/malloc buffer — the overwhelming majority of hits were local
buffers (`LineBuf`, copies made via `strncpy`/`malloc` before
mutation, out-parameters documented to receive output) and are not
this bug class. Remaining candidates were checked by hand: found the
enclosing function, then grepped every call site of that function to
see whether a literal (or a `#define`d literal, or an
`environ_GetProfile`-style default) can reach the parameter.

## Findings

Sorted by likelihood of being hit in ez/messages/cui/help.

### 1. `LowerCase()` + `MapParens()` — help app, reachable by a literal on the plain no-argument invocation path

- `atk/help/src/help.c:292` `LowerCase(astring)` — lowercases in
  place, no restore (duplicated near-identically at
  `atk/help/src/helpdb.c:735` and `atk/help/maint/mkindex.c:56`).
- `atk/help/src/help.c:311` `MapParens(s)` — turns `string(n)` into
  `string.n` in place (`*lpp = '.'; *rpp = '\0';`), no restore.
- Both called on the parameter in
  `atk/help/src/help.c:806` `help_GetHelpOn(self, aname, ...)` at
  lines 827-828: `LowerCase(aname); MapParens(aname);`
- `atk/help/src/helpaux.c:770`, inside
  `help__GetHelpOnTerminal(classID, akey, ...)`:
  `strncpy(tbuffer, LowerCase(akey), HNSIZE);` — mutates `akey`
  in place as a side effect of the call.
- Call-site evidence a literal reaches these: `atk/help/src/helpa.c:114`
  `static char *helpKey="";` is reassigned directly to a `#define`d
  literal when no topic is given on the command line:
  `helpa.c:527`: `helpKey = (moreMode) ? NONWMDEFAULTFILE : WMDEFAULTFILE;`
  where `atk/help/src/config.h:211,216` define
  `WMDEFAULTFILE "tour"` / `NONWMDEFAULTFILE "tour"` — genuine string
  literals, not copies. `helpKey` (now pointing at the literal) is
  then passed straight into `help_HelpappGetHelpOn(helpKey, ...)`
  (`helpa.c:566,571`) → `help_GetHelpOn` → `LowerCase`/`MapParens`,
  and into `help_GetHelpOnTerminal(helpKey, ...)` (`helpa.c:546`) →
  `LowerCase(akey)`.
- **This is the default, argument-less invocation of `help`/`helpa`**
  — i.e. `runapp helpa -d` with no topic, one of the four apps we run.
- Currently harmless: `"tour"` is already all-lowercase and contains
  no `(`/`)`, so neither mutator's guarded write (`if (isupper(...))`,
  `if (rindex(s,'(') && rindex(s,')') ...)`) ever fires — no store
  actually happens today. But the literal genuinely reaches two
  in-place mutators with no defensive copy anywhere on the path;
  changing the default topic string to anything with an uppercase
  letter or parenthesis (e.g. a future rename to `"Tour"` or a
  versioned `"tour(2)"`) reintroduces the exact atomlist-class crash
  on the app's most common startup path.
- Severity: **medium** (latent, not currently triggering, but on the
  most-traveled code path of a running app, and the trigger condition
  is a one-line `#define` edit away).

### 2. `StripWhiteEnds()` / `ReduceWhiteSpace()` — documented-contract mutators, one confirmed literal reachable

- `ams/libs/shr/utils.c:50` `StripWhiteEnds(string)` — the function's
  own comment says it: "It is destructive in the sense that the old
  string will also appear with the trailing white space deleted."
  Writes `stripped[len] = '\0'` for each trailing whitespace char.
- `ams/libs/shr/utils.c:79` `ReduceWhiteSpace(string)` — always writes
  `*new = '\0'` at the end (even a no-op write can still fault if the
  destination is read-only) and rewrites the string in place to
  collapse whitespace runs.
- These are called from ~90 sites across `ams/libs/cui/cuilib.c`,
  `ams/msclients/cui/cui.c`/`cuifns.c`, `ams/libs/ms/*.c`, and via the
  `atkams/messages/lib/amsutil.c` wrappers
  (`amsutil_StripWhiteEnds`/`amsutil_ReduceWhiteSpace`) from
  `messages/lib/{ams,sendmsg,sendaux,msgsa}.c`. The large majority
  trace to a mutable buffer: either a local array filled by
  `GetLine()`/`fgets`, or the interactive command's `arg` string
  (parsed out of a mutable command-line buffer), or an explicit
  `strncpy`-into-local-array copy made first (e.g.
  `ams/libs/ms/disambig.c` already copies `source` into local
  `CopyName` before mutating it — a correct instance of the exemplar
  pattern already in the tree; `atk/basics/wm/wim.c:373` and
  `atk/basics/x/xim.c:998` similarly malloc a private copy
  (`whost`/`xhost`) before lowercasing it in place — also already
  correct).
- One direct literal call site found:
  `ams/msclients/cui/cui.c:490`: `ReadCommandsFromFile("~/.cuirc", 0);`
  reaches `cui.c:694`: `filenam = StripWhiteEnds(arg);` inside
  `ReadCommandsFromFile`. This runs on every `cui`/`messages` startup
  (auto-sourcing `~/.cuirc`).
- Currently harmless: `"~/.cuirc"` has no leading/trailing whitespace,
  so the destructive branch of `StripWhiteEnds` never writes.
- Per the prompt's guidance, functions whose documented contract is
  in-place mutation are a caller-side problem, not a callee fix —
  listed separately here rather than proposed for the atomlist-style
  private-copy treatment. The one live literal call site
  (`ReadCommandsFromFile("~/.cuirc", 0)`) is narrow enough that, if
  Gate 2 is approved for this item, the natural fix is at that one
  call site (pass a small on-stack copy), not inside
  `StripWhiteEnds`/`ReduceWhiteSpace` themselves.
- Severity: **low** (one confirmed literal reachable, currently inert,
  narrow blast radius).

### 3. `ProcessCommand()` — cui's command dispatcher, not currently reachable by a literal

- `ams/msclients/cui/cui.c:556`: mutates its `CurrentCommand`
  parameter in place — lowercases the first word and writes a `\0`
  separator (`cui.c:573`, `cui.c:578`).
- Only two callers (`cui.c:525`, `cui.c:551`), both pass the local
  stack array `CurrentCommand[CMDBUF]` filled by `GetNextCommand()` —
  never a literal today.
- Severity: **informational** — same shape as the bug class, but no
  reachable literal found. Worth a second look if `ProcessCommand` ever
  grows a caller that dispatches a fixed command string.

### 4. `HandleAddress()` — cuilib.c, asymmetric restore, not currently reachable by a literal

- `ams/libs/cui/cuilib.c:1666`. In the `MSWP_CREATABLEMSDIR` case
  (`cuilib.c:1726-1737`) the function temporarily NULs a char inside
  `oldaddr` and explicitly restores it (`*s = c;`). In the sibling
  `MSWP_BADNETMAIL`/`MSWP_UNKNOWNNETMAIL` case
  (`cuilib.c:1809-1811`), `*paren = '\0';` is never restored — but
  nothing reads `oldaddr` again after that switch arm returns, so this
  looks like a dead-end asymmetry rather than a live bug.
- Only caller (`cuilib.c:1533`, inside `RewriteHeaderLine`) passes
  `ThisAddr`, a local stack array filled by
  `MS_ValidateAndReplaceChunk` — never a literal.
- Severity: **informational** — flagging the asymmetry for awareness;
  not part of this bug class since no literal reaches it.

### 5. `hexout("")` — fdbbdf.c, standalone build tool, lowest priority

- `overhead/fonts/cmd/fdbbdf.c:227` `hexout(c,f)` lowercases and
  echoes `c` byte-by-byte in place. Called with a literal empty string
  at `fdbbdf.c:383,421`: `hexout("",fout);` — harmless today because
  the loop condition (`while(*c != '\0')`) never enters on an empty
  string, so no write occurs.
- `fdbbdf` is a standalone BDF font-compilation command-line tool, not
  one of ez/messages/cui/help. Lowest priority; noted for completeness
  only.

## Addendum: `contrib/` scope (added after initial Gate 1 review)

The original sweep excluded `contrib/` per the prompt's suggested
scope. Repeated the same three-family grep + parameter filter against
`contrib/` only. Raw hits: 5 (`strtok`), ~35 (`index`/`strchr` +
NUL-write), 20 (case-folding/char-replace). As expected, very few
survived — `contrib/` is a grab-bag of largely self-contained
third-party/MIT-derived tools (`mit/`, `zip/`, `calc/`, `snap2/`,
`srctext/html/`) that don't share the AMS/cui utility functions
(`StripWhiteEnds`, `ReduceWhiteSpace`, `LowerCase`) responsible for
most of the core-tree findings above — grepped for those names inside
`contrib/`, zero hits, so `contrib/` code isn't exposed to that
particular family at all.

One genuine parameter-mutating function found, not currently
literal-reachable:

- `contrib/srctext/html/html.c:661` `html__ChangeAttribute(self, tv,
  env, attr, value)` — an exported Class Procedure
  (`contrib/srctext/html/html.ch:49`) — passes `attr` straight into
  `storeVar(style, key, value)` (`html.c:484`), which lowercases `key`
  in place with no restore (`html.c:490-494`, no copy). Both current
  callers are in `contrib/srctext/html/htmlview.c`
  (`htmlview_EditAttributes`, lines 288 and 297): one passes an
  attribute name pulled from `html_GetAttributeList` (heap, sourced
  from the style's own keylist — writable), the other passes `answer`,
  a local `char answer[MAXPATHLEN]` stack buffer filled by
  `message_AskForString`. Neither is a literal today. Same shape as
  the core tree's `ProcessCommand`/`HandleAddress` findings —
  informational only.

Checked and ruled out (local buffers or writable heap memory, not
literals):

- `contrib/snap2/pcserver/pcsutils.c` `FoldLower(name)` — both call
  sites (`pcsutils.c:117`) pass `struct pcname` fields (fixed-size
  struct arrays), not literals.
- `contrib/mit/fxlib/clients/full_name.c:76` and
  `contrib/mit/neos/papers.c:172` — both parse a Hesiod-resolved
  passwd entry (`hes_resolve()`'s malloc'd result), which is writable
  heap memory, not a string literal; the in-place `*tmp = '\0'` edits
  there are unrelated to this bug class.
- `contrib/mit/neos/papers.c:1474` `SetSortOrder()` calls `strtok()` on
  a variable that is `environ_GetProfile()`'s result *or*, when no
  profile is set, a literal default (`papertypes[j][1]`) — but the
  code already copies that literal into the local `tmpstring` buffer
  first (`strcpy(tmpstring, papertypes[j][1]); string = tmpstring;`)
  before calling `strtok`, exactly matching the atomlist-exemplar fix.
  Already correct.
- `contrib/calc/calcv.c` `Shrink(string)` mutates its parameter, but
  every call site passes `Operand1`/`Expression`, which are
  `self->operand_1`-style in-object buffers, not literals.
- `contrib/srctext/html/html.c:989` (token-parsing loop) and
  `contrib/srctext/html/html.c:868` `addVars()`'s `vars` argument — both
  trace to a local `char vars[...]`/`char entity[...]` stack buffer
  filled by `strncpy` earlier in the same function, not a literal.
- Remaining raw hits (`zip/lib/zip*.c`, `eatmail/eatmail.c`,
  `mit/fxlib/clients/dump.c`/`return.c`, `mit/com_err/compile_et.c`,
  `contrib/srctext/html/html.c:352` `strtok(buf,...)`) all mutate
  local stack buffers filled by `fgets`/`strncpy`/`sprintf` earlier in
  the same function.

No live literal-reachable finding in `contrib/`. This matches the
"expect very few" prediction — worth noting *why*: the bug class
needs a shared, widely-called mutator plus a literal default feeding
it, and `contrib/`'s tools are mostly single-purpose programs with
their own local parsing buffers rather than library functions called
from dozens of sites the way `StripWhiteEnds`/`LowerCase` are in the
core tree.

## Not this bug class (checked and ruled out)

- `ams/libs/ms/disambig.c` `CopyName` — already copies before
  mutating (matches the atomlist fix pattern).
- `atk/basics/wm/wim.c` / `atk/basics/x/xim.c` host-lowercasing —
  already mallocs a private copy (`whost`/`xhost`) before mutating.
- `atk/basics/common/path.c` `FoldName(path)` — both call sites
  (`TruncatePath`, the `$VAR`-expansion helper) `strcpy` into a local
  buffer first, then fold that copy.
- Everything else in the ~350 raw grep hits: local buffers
  (`LineBuf`, `Arglist`, `NewContent`, `CellName`, etc., all
  `malloc`'d or stack arrays owned by the function itself) or
  documented output parameters (`newaddr`, `*ct`, `*remaining`).
- `overhead/mail/metamail/metamail/*.c` case-folding hits — treated as
  vendored third-party code (Bellcore metamail), excluded per the
  prompt's scope.

## Files touched

None — Gate 1 is census-only.

## Open questions / what surprised me

- The two live findings (help's `WMDEFAULTFILE`/`"tour"`, cui's
  `"~/.cuirc"`) are both currently harmless by coincidence of the
  literal's exact contents (no uppercase/parens; no whitespace) rather
  than by any defensive coding. That makes them different in kind from
  the atomlist crash (which fired unconditionally): these are
  regressions waiting for someone to edit an unrelated `#define` or
  string constant, not live crashes today. Worth deciding at Gate 2
  review whether "currently-inert but one string-constant edit from
  crashing" clears the bar for a fix, or whether to only fix live
  crashes.
- `StripWhiteEnds`/`ReduceWhiteSpace`/`LowerCase`/`MapParens` are
  genuinely different in kind from the atomlist bug: their whole
  purpose is destructive in-place editing (unlike
  `atomlist__StringToAtomlist`, which mutated in place only as an
  implementation accident of the original tokenizer). Per the
  playbook's framing this makes them "caller's contract to supply
  writable memory" bugs, not "callee should own a private copy" bugs
  — I've listed them separately rather than proposing the atomlist
  treatment for the ~90 call sites of `StripWhiteEnds` wholesale.
