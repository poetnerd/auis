# M2 rollout point 4f (bucket 4, session 6): `overhead/mail/lib`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, confirmed twice more for
determinism, per the runbook's "Gate scope" ruling (tree-wide gate NOT
required for this directory, though see §9 for an explicit judgment
call on whether that's still right given the fan-out size). No commits
made. Working tree: 24 files edited (`Imakefile` + `parsey.gra` + 22
`.c` files). Session diff at tree root: `m2-mail-lib-session.diff` (636
lines). Full gate log at the canonical path
`~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten across this
session's several build passes; the final two determinism passes are
both clean, 0 `error:` lines, reproduced in §6).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   "Command style" section's `cd`-persistence correction and the
   "Logging" section's now-unconditional, word-boundary-anchored
   malloc-family sweep mandate), `m2-rollout-runbook.md`,
   `m2-mail-lib-prompt.md`, and all nine prior rollout reports
   (`m2-pilot-eq-REPORT.md`, `m2-batch2-REPORT.md`,
   `m2-batch3a-REPORT.md`, `m2-batch3b-REPORT.md`,
   `m2-utillib-REPORT.md`, `m2-metamail-REPORT.md`, `m2-text-REPORT.md`,
   `m2-rofftext-REPORT.md`, `m2-table-REPORT.md`) in full before
   starting.
2. **Checked command-style/cwd-persistence early**: a throwaway `cd
   .../src/overhead/mail/lib` call followed by a bare `pwd` in the next
   tool call showed the cwd had reverted to the tree root — cwd does
   **not** persist across Bash calls in this session (same as the
   `atk/text`/`atk/rofftext`/`atk/table` sessions). Used `make -C
   <absolute-path> <target>` for every build step for the rest of the
   session, one command per call, never chained with `&&`/`;`.
3. Read `src/overhead/mail/lib/Imakefile` directly and confirmed the
   generated-source mechanics (see §7) and the `OBJS`-vs-test-only file
   split (see §8) — both **corrections to the prompt's own text**, not
   just confirmations.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 291 wins over
   `system.mcr`'s default at line 177 — re-verified after both later
   `depend`-triggered Makefile regenerations too).
5. Fix-surfacing pass: `make -C ... clean`, `make -C ... depend`
   (confirmed `parsey.h`/`parsey.c` regenerated, `parsel.c` absent until
   the ordinary build graph produces it later — see §7), `make -C ...
   -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"` — each a separate
   Bash call. Result: **112 real errors, matching the stale estimate
   exactly** — but see §5 (below) and the prompt's own explicit warning
   that an exact match is not evidence the malloc-family blind spot is
   absent; it wasn't.
6. Investigated every project-local function's real definition via
   `Read`/`Bash grep` before writing any declaration — see §4 for the
   full per-instance table.
7. Fixed all 112 census-visible instances across 21 files (20 `.c`
   files + `parsey.gra`, the real hand-written source behind the
   generated `parsey.c`/`parsey.h`). One self-inflicted regression
   found and fixed immediately: a doc-comment in `authret.c` containing
   a literal `*/` inside its text prematurely closed the C comment,
   producing a real syntax error (`unknown type name 'cell'`) on
   rebuild — caught by the very next build, fixed by rewording the
   comment, not by escaping or removing the explanation.
8. Rebuilt — 0 errors, exit 0.
9. **Unconditionally swept all 33 `.c` files** (not just the 30 in
   `OBJS`, and not just files already getting a census-driven edit) for
   bare `malloc(`/`free(`/`realloc(`/`calloc(` calls, using the
   corrected word-boundary-anchored, space-tolerant pattern (`grep -nE
   "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`), one call
   covering multiple files at a time (never a shell loop) — found 12
   genuinely invisible blind-spot instances across 2 files plus a
   related, distinct cleanup opportunity in 3 more files (stale
   wrong-typed or entirely dead `malloc`/`realloc` externs) — see §5.
10. Fixed all of the above. Rebuilt — 0 errors, exit 0, 0
    `-Wincompatible-library-redeclaration` warnings.
11. Ran the subtree-local gate twice more for determinism (`make -C
    ... clean`, `make -C ... depend`, `make -C ... -k install`, plain
    recipe, each its own call) — both passes exit 0, 0 errors,
    `parsey.h`/`parsey.c` regenerated fresh each time, `COMPILERFLAGS`
    override re-verified after each regeneration.
12. **Confirmed the 3 test-only files (`imapspike.c`, `imaptest.c`,
    `smtptest.c`) need zero fixes of any kind** — built each `.test`
    target explicitly (`make -C ... imapspike.test
    CDEBUGFLAGS="-ferror-limit=0 -g -O0"`, etc.) since the `install`
    target never compiles them; all three already `#include
    <stdlib.h>` and had zero implicit-declaration errors under the M2
    flag, zero malloc-family blind-spot instances. See §8.
13. Identified real consumers via a tree-wide `grep -rl "libmail.a"
    --include=Imakefile` (24 directories, confirming the prompt's
    fan-out estimate) and rebuilt 3 structurally distinct real
    consumers against the updated `libmail.a`, then `nm -g`-verified
    each — see §9.
14. `fossil status` (§10) confirms exactly the 24 files touched. No
    commit made. `fossil diff > m2-mail-lib-session.diff` written to
    the tree root.

## 3. Generated-source handling (prompt's explicit ask)

Confirmed by reading the Imakefile directly (lines 56–62), not just
citing the prompt:

```
FlexOrLexFileRule(parsel)
Parser(parsey,-d)
NormalLexRule()

parsel.o: parsey.h

LexWithReplacement(parsel,mail_parseyy)
```

Two **different** generated-source mechanisms coexist here, with
different implications for `depend`:

- **`Parser(parsey,-d)`** — bison grammar. Expands (via
  `config/andrew.rls`) to `depend:: parsey.h parsey.c`, wired **only**
  into the `depend::` target, exactly like the `atk/eq` pilot and
  `overhead/eli/lib`'s `Parser(eliy,-d)`. `make clean` deletes
  `parsey.h`/`parsey.c` (confirmed: absent after `clean`, both present
  with fresh mtimes after `depend`, both regenerated on all three
  `depend` runs this session). **`make depend` before `install` is
  required**, confirmed by the explicit `parsel.o: parsey.h`
  dependency line holding: the lexer's generated source
  (`parsel.c`, produced later, at compile time) `#include`s
  `parsey.h`'s token definitions, and this Makefile line is what forces
  `parsey.h` to exist before `parsel.o`'s own suffix rule ever runs,
  regardless of ordering elsewhere in the Makefile.
- **`FlexOrLexFileRule(parsel)` + `NormalLexRule()` +
  `LexWithReplacement(parsel, mail_parseyy)`** — flex lexer, but
  **not** wired into `depend::` at all. Reading the regenerated
  Makefile directly (not assuming): `parsel.l: parsel.flex` (a symlink
  rule) and a custom `parsel.o: parsel.l` rule (running `flex`, then
  `sed -e '/yy/s//mail_parseyy/g'` to rename every `yy`-prefixed
  identifier flex emits, then compiling) are ordinary dependency-graph
  rules reachable from `all`/`install`, not `depend`. `parsel.c` is
  legitimately absent right after `clean`+`depend` (confirmed) and
  only appears once `install`'s own build graph reaches `parsel.o` —
  this is expected, not a second generated-source gap, and does not
  need any extra `depend`-style workaround.

Net: `make depend` before `install` is required here, and even more
directly than the pilot's case — the explicit `parsel.o: parsey.h`
Makefile line is the literal enforcement mechanism, not just an
ordering convention. Held across all three `clean`+`depend`+`install`
cycles this session; `parsel`-related fallout (the single
`mail_parseyylex` instance, §4) was never obscured by a `parsey.h`
file-not-found failure once the corrected recipe was used from the
start.

## 4. Per-instance findings: the 112 census-visible errors

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `amsauth.c:82` | `CheckAMSConfiguration` | No header anywhere (`mailconf.c`) |
| 2 | `amsauth.c:231` | `CheckServiceConfiguration` | No header anywhere (`overhead/util/lib/svcconf.c`) |
| 3 | `amsconf.c:53` | `CheckAMSConfiguration` | No header anywhere |
| 4 | `amsconf.c:57` | `GetConfiguration` | Missing in-tree header, sub-case 1 (`util.h:100`, not included) |
| 5 | `amsconf.c:64` | `errprintf` | No header anywhere (`errprntf.h` only defines `ERR_*` macros; only tree-wide precedent is `ams.h:170`) |
| 6 | `arpadate.c:89` | `sprintf` | Missing standard-library header (`<stdio.h>` only reachable via an `#ifdef TESTINGONLYTESTING`-guarded include further down the file) |
| 7,8 | `authret.c:68,121` | `CheckServiceConfiguration` | No header anywhere (`svcconf.h` only declares the AMS_*/cell-service globals, not the function) |
| 9 | `capture.c:71` | `BracketField` | Missing in-tree header, sub-case 1 (`mail.h:122`, declared, just not included — 12 sibling files already reach it this way) |
| 10–18 | `ckamsdel.c:369,432,466,493,520,547,574,601,629` | `CheckAMSConfiguration` | No header anywhere |
| 19,20 | `dropoff.c:176,634` | `CheckAMSConfiguration` | No header anywhere |
| 21 | `dropoff.c:670` | `malloc` | Missing standard-library header |
| 22,23,25,26 | `dropoff.c:672,688,696,702` | `dbg_close` | Missing in-tree header, partial-wrapper-family sub-shape (`fdplumb.h` `#define`s `close`→`dbg_close` but only declares 6 of 16 `dbg_*` names; `dbg_close` isn't one) |
| 24 | `dropoff.c:689` | `free` | Missing standard-library header |
| 27,28 | `fwdvalid.c:119,122` | `malloc`, `free` | Missing standard-library header |
| 29 | `fwdvalid.c:179` | `AddHost` | No header anywhere (`parseadd.c`) |
| 30 | `fwdvalid.c:241` | `la_FreeMD` | No header anywhere (`locnamex.c`) |
| 31 | `fwdvalid.c:563` | `UnparseAddressList` | No header anywhere (`parseadd.c`) |
| 32 | `fwdvalid.c:594` | `ParseAddressList` | No header anywhere (`parseadd.c`) |
| 33 | `fwdvalid.c:660` | `FreeAddressList` | No header anywhere (`parseadd.c`) |
| 34 | `genid.c:127` | `osi_GetTimes` | No header anywhere (`overhead/util/lib/times.c` — recurring cross-directory pattern, now hit in 5+ directories tree-wide) |
| 35 | `genid.c:129` | `CheckAMSConfiguration` | No header anywhere |
| 36 | `genid.c:131` | `getaddr` | Missing in-tree header, sub-case 1 (`util.h:311`) |
| 37 | `genid.c:133` | `initstate` | Missing standard-library header (`<stdlib.h>`) |
| 38 | `locname.c:63` | `malloc` | Missing standard-library header |
| 39 | `locname.c:90` | `free` | Missing standard-library header |
| 40,41,43 | `locname.c:132,139,191` | `la_FreeMD` | No header anywhere |
| 42 | `locname.c:169` | `CheckAMSConfiguration` | No header anywhere |
| 44 | `locname.c:198` | `FreeHost` | No header anywhere (`parseadd.c`) |
| 45 | `locname.c:225` | `Unquote` | No header anywhere (`parseadd.c`) |
| 46 | `locname.c:283` | `CheckAMSConfiguration` | No header anywhere |
| 47 | `locnamex.c:50` | `free` | Missing standard-library header |
| 48 | `mailconf.c:682` | `malloc` | Missing standard-library header |
| 49 | `mailconf.c:685` | `LCappend` | No header anywhere (`overhead/util/lib/lcappend.c`) |
| 50 | `mailconf.c:691` | `free` | Missing standard-library header |
| 51 | `mailconf.c:782` | `atoi` | Missing standard-library header |
| 52 | `mailconf.c:858` | `CheckServiceConfiguration` | No header anywhere |
| 53 | `parseadd.c:73` | `mail_parseyylex` | No header anywhere (generated `parsel.c`, flex's renamed `yylex`) |
| 54,55 | `parseadd.c:95,96` | `SetNextLine`, `pareset_lexer` | No header anywhere (`parsel.flex`) |
| 56 | `parseadd.c:135` | `UnparseAddressList` | Same-file forward reference (defined 164) |
| 57 | `parseadd.c:143` | `free` | Missing standard-library header |
| 58,59,60 | `parseadd.c:214,739,765,792` | `StrFree` | No header anywhere (`parseutl.c`) |
| 61 | `parseadd.c:277` | `PrintSimpleAddress` | Same-file forward reference (defined 387) |
| 62 | `parseadd.c:738` | `RemHost` | Same-file forward reference (defined 851) |
| 63 | `parseadd.c:774` | `FreeAddressList` | Same-file forward reference (defined 815) |
| 64 | `parseadd.c:788` | `RemAddress` | Same-file forward reference (defined 833) |
| 65 | `parseadd.c:799` | `la_FreeMD` | No header anywhere |
| 66 | `parseadd.c:869` | `malloc` | Missing standard-library header |
| 67 | `parseadd.c:871` | `NoStorage` | No header anywhere (`parseutl.c`) |
| 68 | `parseadd.c:886` | `AppendHosts` | No header anywhere (`parseutl.c`) — **pointer-returning** (`ADDRESS_HOST *`), sourced from the real definition, not guessed |
| 69–72 | `parseutl.c:62,67,82,90` | `strlen`,`strcpy`,`strcat`,`free` | Missing standard-library header |
| 73,74 | `parsey.gra:216,242` | `StrFree` | No header anywhere — the real hand-written source behind generated `parsey.c` |
| 75,76,78 | `qmail.c:200,395,412` | `CheckAMSConfiguration` | No header anywhere |
| 77 | `qmail.c:205` | `GetAuthInfo` | No header anywhere (`authret.c`) |
| 79,80 | `qmail.c:308,376` | `dbg_closedir` | Partial-wrapper-family sub-shape (`fdplumb6.c`) |
| 81 | `qmail.c:397` | `LCappend` | No header anywhere |
| 82–84 | `qmail.c:538,554,563` | `dbg_fclose` | Partial-wrapper-family sub-shape |
| 85 | `stats.c:93` | `errprintf` | No header anywhere |
| 86 | `stats.c:104` | `GetHostDomainName` | Missing in-tree header, sub-case 1 (`util.h:74`) |
| 87 | `stats.c:176` | `atoi` | Missing standard-library header |
| 88 | `stats.c:203` | `dbg_fclose` | Partial-wrapper-family sub-shape |
| 89 | `stats.c:238` | `dbg_socket` | Partial-wrapper-family sub-shape |
| 90 | `stats.c:313` | `dbg_close` | Partial-wrapper-family sub-shape |
| 91 | `stats.c:370` | `osi_GetTimes` | No header anywhere |
| 92 | `usysex.c:58` | `sprintf` | Missing standard-library header |
| 93 | `valhost.c:117` | `cptres_search` | No header anywhere (`rsearch.c`) |
| 94 | `valhost.c:270` | `inet_addr` | Missing standard-library header (`<arpa/inet.h>`) |
| 95 | `valhost.c:344` | `LCappend` | No header anywhere |
| 96 | `valhost.c:378` | `CheckAMSConfiguration` | No header anywhere |
| 97,98,100,101,103 | `vmail.c:123,147,151,156,572,609` | `dbg_close` | Partial-wrapper-family sub-shape |
| 99 | `vmail.c:150` | `IsOnVice` | No header anywhere (`overhead/util/lib/vclose.c`) |
| 102,104 | `vmail.c:271,306` | `dbg_fclose` | Partial-wrapper-family sub-shape |
| 105 | `vmail.c:433` | `CheckAMSConfiguration` | No header anywhere |
| 106 | `vmail.c:436` | `GetRetPath` | No header anywhere (`authret.c`) |
| 107 | `vmail.c:450` | `GetAuthInfo` | No header anywhere |
| 108 | `smtpsub.c:123` | `ParseAddressList` | No header anywhere |
| 109 | `smtpsub.c:145` | `UnparseOneAddress` | No header anywhere (`parseadd.c`) |
| 110 | `smtpsub.c:152` | `FreeAddressList` | No header anywhere |

(Table collapses repeated same-function/same-file call sites into one
row with all line numbers, per the precedent set by `ams/libs/cui`'s
report — full per-call-site detail is in `m2-mail-lib-session.diff`.
112 individual call sites total, confirmed by `grep -c "error:"`
against the fix-surfacing pass log.)

**"Possible genuine bug/typo" category: still empty.** Every
unfamiliar-looking name resolved to a real, correctly-spelled
definition somewhere in the tree once grepped — including `AppendHosts`
(pointer-returning, sourced exactly from its real signature in
`parseutl.c`, not inferred) and `mail_parseyylex`/`SetNextLine`/
`pareset_lexer` (all real, defined in `parsel.flex`, the readable
source behind the generated lexer).

Fix summary by category:
- **Missing standard-library header**: `<stdio.h>` (`arpadate.c`,
  `usysex.c`); `<stdlib.h>` (`fwdvalid.c`, `genid.c`, `locname.c`,
  `locnamex.c`, `mailconf.c`, `parseadd.c`, `parseutl.c`, `dropoff.c`);
  `<string.h>` (`parseutl.c`); `<arpa/inet.h>` (`valhost.c`).
- **Missing in-tree header, sub-case 1** (header exists, declares it,
  just not included): `#include <mail.h>` in `capture.c`
  (`BracketField`); `#include <util.h>` in `amsconf.c`
  (`GetConfiguration`), `genid.c` (`getaddr`), `stats.c`
  (`GetHostDomainName`).
- **No header anywhere in the tree** (largest category by instance
  count): local `extern` declarations, each sourced from the real
  definition, placed next to each file's own existing local-extern
  precedent where one existed (`amsauth.c`'s new block sits where its
  `#ifndef _IBMR2 extern char *malloc()` block used to be;
  `fwdvalid.c`'s new block extends the file's existing `extern
  PARSED_ADDRESS *SingleAddress();`; `dropoff.c`/`qmail.c`/`vmail.c`
  each extend a pre-existing `/* External declarations */`-style
  block). Where no precedent existed in a file, added a small new
  block right after the file's own `#include`s.
- **Same-file forward reference** (`parseadd.c` only, 5 instances):
  `UnparseAddressList`, `PrintSimpleAddress`, `RemHost`,
  `FreeAddressList`, `RemAddress` — all non-`static` (each is also
  called from a sibling file, confirmed by grep before deciding not to
  mark them `static`), declared via a small forward-declare block
  matching the file's own existing `static int Foo();` convention in
  spirit if not in linkage.
- **New sub-shape recurrence, not new to the taxonomy**: the
  `fdplumb.h` partial-wrapper-family gap (`dbg_close`, `dbg_closedir`,
  `dbg_fclose`, `dbg_socket` — 4 of the 10 undeclared `dbg_*` names,
  hit across `dropoff.c`, `qmail.c`, `stats.c`, `vmail.c`) recurs here
  exactly as documented in batch B and `overhead/util/lib` — this
  directory is a heavy file-I/O consumer (mail delivery/dropoff), so
  the recurrence is unsurprising and reinforces the existing
  recommendation that `fdplumb.h` itself deserves a real housekeeping
  pass someday.

## 5. Malloc-family blind-spot sweep: 12 genuinely invisible instances across 2 files, plus 3 files needing related cleanup

Swept all 33 `.c` files (not just the 30 `OBJS` members) with the
corrected pattern `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
*\(|\bcalloc *\("`, one call covering several files at a time (never a
per-file loop, never relying on parallel-call output ordering — see
§11 for a methodology note on why). Three distinct findings came out
of it, worth keeping separate:

**(a) Genuinely invisible blind-spot instances (zero declaration of
any kind reaching the call site before this session) — 12 total:**

| File | Function | Lines | Notes |
|---|---|---|---|
| `addrambig.c` | `free` | 74, 79, 83 | Not in `OBJS`'s census at all — this file has zero census-visible errors of its own (all its real logic is `#ifdef WHITEPAGES_ENV`-guarded, but the `#include` fix applies unconditionally); a third data point (after `atk/text`, `atk/rofftext`) that per-file cleanliness doesn't predict blind-spot absence. |
| `ckamsdel.c` | `free` | 94, 129, 151, 217, 341, 342, 344, 355, 404 | This file **does** have 9 census-visible errors (all `CheckAMSConfiguration`) — the blind spot hid entirely behind a different, unrelated fallout category in the same file. |

Fix: `#include <stdlib.h>` added to both.

**(b) Related but distinct: stale wrong-typed `extern char
*malloc()`/`realloc()` declarations that suppressed the
`-Werror=implicit-function-declaration` diagnostic without being
correct — 2 files:**

- `ckamsdel.c:54-55` — `#ifndef _IBMR2 extern char *malloc(); extern
  char *realloc(); #endif`, present alongside 5 real `malloc()` call
  sites (89,127,198,303,388). Because a (wrong-typed) declaration
  already existed, `malloc()` itself never appeared in the 112-error
  census — only `free()` (undeclared even in the stale form) did. This
  is the same "self-inflicted-looking but actually pre-existing"
  pattern `overhead/util/lib` and the metamail session both
  documented: once `<stdlib.h>` is added for the free()-driven fix,
  the stale declaration becomes a hard `conflicting types` compile
  error, not just latent — removed it.
- `scan822.c:40-42` — same shape, `#ifndef _IBMR2 extern char
  *malloc(), *realloc(); #endif`, 2 real `malloc()` call sites (280,
  308), zero `free()`/`realloc()` calls. Not in the 112-error census
  for the same reason. Fixed the same way: `#include <stdlib.h>`
  added, stale extern removed.

**(c) Distinct again: completely dead (never-called) stale
`malloc`/`realloc` externs — 2 files, pure housekeeping, not a fallout
instance of any kind:**

- `amsauth.c:52-55`, `authret.c:53-56` — both had a `#ifndef _IBMR2
  extern char *malloc(); extern char *realloc(); #endif` block that
  the file never actually calls (confirmed via the same sweep pattern
  finding zero real call sites beyond the declarations themselves in
  either file). Removed both, matching the precedent `m2-batch3a`
  set for an unused `errprintf` extern in `menubar.c` — dead code
  cannot contribute to any diagnostic, removing it cannot introduce
  one either.

**Real total fallout for this directory: 112 census-visible + 12
malloc-blind-spot instances = 124**, 11% over the stale table's 112 —
a much smaller overshoot than `atk/text` (208%) or `atk/rofftext`
(93%), but still confirms the runbook's now-standing warning that an
exact census match (this directory's 112-vs-112 was as clean a match
as `overhead/util/lib`'s was) is not evidence the blind spot is
absent. The (b)/(c) findings above are not counted in the 124 (per the
precedent set by `overhead/util/lib`'s and metamail's reports, which
treat "stale wrong-typed declaration becomes a conflict" and "unused
dead declaration" as cleanup findings, not new fallout instances) —
flagging them explicitly here rather than silently folding them into
the headline count.

## 6. Final clean builds (twice, for determinism)

```
$ make -C .../overhead/mail/lib clean && make -C .../overhead/mail/lib depend && make -C .../overhead/mail/lib -k install   [pass 1]
EXIT=0, 0 "error:" lines
$ make -C .../overhead/mail/lib clean && make -C .../overhead/mail/lib depend && make -C .../overhead/mail/lib -k install   [pass 2]
EXIT=0, 0 "error:" lines
```

`libmail.a` installed successfully to `build/lib/` both times.
`COMPILERFLAGS` re-verified after both passes (`grep -n COMPILERFLAGS
Makefile` — override line still wins at line 291 over `system.mcr`'s
default at 177). `parsey.h`/`parsey.c` regenerated fresh on both
`depend` runs, confirmed via `ls -la` timestamps. Zero
`-Wincompatible-library-redeclaration` or `conflicting types` warnings
in either final pass (both were present transiently during the
malloc-sweep fix cycle, from the stale externs in §5(b)/(c), and are
gone once those were removed).

## 7. `OBJS` members vs. test-only files (prompt's explicit ask)

**Correction to the prompt's own count**: the directory has **33**
`.c` files total, not 32 — confirmed by `ls *.c | wc -l`. 30 are listed
in the Imakefile's `OBJS` (used to build `libmail.a`):

```
amsauth amsconf arpadate authret capture canon ckamsdel dropoff
fwdvalid genid locname locnamex mailconf parseadd parsel parseutl
parsey qmail rsearch scan822 stats sysexits usysex valhost vmail
addrambig tlscon netrc smtpsub imap_prot
```

The other 3 — `imapspike.c`, `imaptest.c`, `smtptest.c` — are
`TestingOnlyTestingRule`-driven `.test`-target-only sources (confirmed
via the Imakefile's `TESTLIBS`/`TestingOnlyTestingRule` lines and the
regenerated Makefile's `.c.test:` suffix rule), never compiled by
`install`/`all` at all (confirmed empirically: none of the three
appear anywhere in the `install` build log). 30 + 3 = 33.

**Did the test-only files need fixes? No — confirmed empirically, not
assumed.** Built each of the three `.test` targets explicitly under
the M2 flag (`make -C ... imapspike.test CDEBUGFLAGS="-ferror-limit=0
-g -O0"`, same for `imaptest.test`/`smtptest.test`): all three built
with **exit 0, 0 `error:` lines** (only pre-existing
`-Wdeprecated-non-prototype` warnings, unrelated to M2). All three
already `#include <stdlib.h>` (confirmed before assuming), so the
malloc-family sweep found real call sites in `imap_prot.c`/
`imapspike.c`/`imaptest.c` that were already correctly covered — no
edit needed. `smtptest.c` has zero malloc-family calls of its own.
This makes sense structurally: `imap_prot.c`, `tlscon.c`, `netrc.c`,
and the three `.test` files are visibly the newest code in this
directory (part of the IMAP/TLS/netrc writeback work referenced in
`project_ams_imap_project`/`project_imap_writeback_dispatch` memory,
not 1990s-era originals) — written with modern header discipline
already in place, unlike the rest of the directory's K&R-era files.

## 8. Structural note: this directory has no Class system at all

Confirmed via `ls *.ch *.eh` returning nothing: `overhead/mail/lib` is
a plain C library, not a Class-based (`classpp`-generated) one. The
runbook's `AUXMODULE`-guarded double-underscore-method sub-shape
(found in `atk/table`) **cannot occur here** — there is no generated
`.eh`, no class-method macro, no double-underscore dispatch of any
kind anywhere in this directory. Checked per the prompt's explicit
instruction rather than assumed silently.

## 9. `nm -g` fan-out verification (prompt's explicit ask — extra care given the fan-out size)

Tree-wide `grep -rl "libmail.a" --include=Imakefile` found **26**
Imakefiles referencing `libmail.a` (including `overhead/mail/lib`'s
own Imakefile, in a comment) — 25 real external consumer directories,
consistent with the prompt's "~24" estimate. **Correction to the
prompt's own example**: `mailto` (`overhead/mail/metamail/metamail`)
does **not** actually link `libmail.a` — checked its Imakefile
directly: `ProgramTarget(mailto, $(MAILTO_OBJS), $(UTILLIB), $(TERMLIB))`
links only `$(UTILLIB)`, not `libmail.a`. Substituted a real, verified
consumer instead (see below) rather than testing a non-consumer.

Rebuilt and `nm -g`-verified **3 structurally distinct real
consumers**, each confirmed via its own Imakefile before rebuilding
(not assumed):

1. **`atkams/messages/lib`** → `amsn.do` (`DynamicMultiObject`,
   dynamically loaded, the actual `messages`-app mail-store backend
   per `sonnet-playbook.md`'s own note). `make -C
   .../atkams/messages/lib install` — exit 0, relinked cleanly against
   the updated `libmail.a`.
2. **`ams/msclients/cui`** → `cuin` (`ProgramTarget`, statically
   linked terminal client; `cui` is its symlink; the SNAP variant
   `cuis` isn't built in this config, matching every prior session's
   finding). `make -C .../ams/msclients/cui install` — exit 0,
   relinked and reinstalled (`cuin` + `cui` symlink) cleanly.
3. **`overhead/mail/cmd`** → `arpadate`/`decode_id`/`ams_genid`
   (three small standalone `ProgramTarget`s, each linking `libmail.a`
   directly with no other AMS library in between — the simplest
   possible real consumer shape). `make -C .../overhead/mail/cmd
   install` — exit 0, all three relinked and reinstalled to
   `build/etc/`.

`nm -g` on each (one target per call, per the standing caution about
piped/chained `nm` calls; the `cd <tree-root> && nm -g
build/bin/<target>` carve-out shape was needed once — a bare `nm -g
/abs/path` with a `2>&1` redirect got denied, the tree-root-relative
form with no redirect went through and its large output was
automatically persisted to a tool-result file, grepped in a separate
follow-up call):

- `amsn.do`: `_AppendHosts`, `_CheckAMSConfiguration`,
  `_CheckServiceConfiguration`, `_StrFree`, `_dbg_close`,
  `_dbg_closedir`, `_dbg_fclose` — each appears **exactly once**, all
  type `T` (defined), no duplicates.
- `cuin`: identical set, identical result — each exactly once, type
  `T`.
- `arpadate`: `_arpadate` present as expected; small binary, didn't
  separately re-verify every symbol name (the two larger consumers
  above are the load-bearing check for symbol-table health; this one's
  real value is confirming the link succeeds at all against the
  updated archive, which it did, exit 0).

**Zero `overhead/mail/lib`-distinctive symbol leakage or duplication
in any of the three.** This is expected, not just hoped for: every fix
this session was either a `#include` addition or a local (non-defining)
`extern` declaration — nothing added, removed, or retyped an actual
function *definition*, so there is no mechanism by which any
consumer's link step could see a new or duplicate symbol.

**Judgment call on gate scope, surfaced explicitly per the prompt's
request**: I do not think anything found this session casts doubt on
the "subtree-local gate only" ruling. The mechanism argument (M2's
flag only changes diagnostic severity inside `.o` files compiled in
the flagged directory; no installed header or archive member's ABI
changes) applies here exactly as it did for `ams/libs/cui`'s
comparably wide fan-out. The empirical check — 3 structurally distinct
real consumers (a `.do`, a statically-linked `ProgramTarget`, and 3
minimal standalone tools), all rebuilding clean with zero symbol-table
anomalies — is consistent with, not just silent on, that ruling. I did
not run the tree-wide gate. If wdc wants a fourth data point given the
fan-out size (e.g. `ams/msclients/vui` or one of the `contrib/`
consumers), that's a cheap follow-up, not a sign anything here is
actually in doubt.

## 10. Files touched (compile status)

All 24 edited files compile clean (0 errors of any kind) in their
final state, confirmed by two full `make -C ... clean && make -C ...
depend && make -C ... -k install` passes back-to-back after the
fix-surfacing pass (three-plus total per file, given the intermediate
malloc-sweep and `authret.c` comment-bug correction cycles).

- `Imakefile` — `COMPILERFLAGS` override added.
- `addrambig.c` — `<stdlib.h>` added (malloc-family blind spot only;
  zero census-visible errors in this file).
- `amsauth.c` — local externs for `CheckAMSConfiguration`/
  `CheckServiceConfiguration` added; dead `#ifndef _IBMR2 extern char
  *malloc/realloc()` block removed (never called).
- `amsconf.c` — `<util.h>` added (`GetConfiguration`); local externs
  for `CheckAMSConfiguration`/`errprintf` added.
- `arpadate.c` — unconditional `<stdio.h>` added (`sprintf`).
- `authret.c` — local extern for `CheckServiceConfiguration` added;
  dead `#ifndef _IBMR2 extern char *malloc/realloc()` block removed
  (never called) — also where the self-inflicted `*/`-inside-comment
  syntax error (§2 step 7) was introduced and fixed.
- `capture.c` — `<mail.h>` added (`BracketField`).
- `ckamsdel.c` — `<stdlib.h>` added (covers 9 blind-spot `free()`
  sites); local extern for `CheckAMSConfiguration` added; stale
  `#ifndef _IBMR2 extern char *malloc/realloc()` removed (real calls
  existed, now covered correctly by `<stdlib.h>`).
- `dropoff.c` — `<stdlib.h>` added; local externs for
  `CheckAMSConfiguration`/`dbg_close` added.
- `fwdvalid.c` — `<stdlib.h>` added; local externs for `AddHost`/
  `la_FreeMD`/`UnparseAddressList`/`ParseAddressList`/
  `FreeAddressList` added.
- `genid.c` — `<stdlib.h>` + `<util.h>` added; local externs for
  `CheckAMSConfiguration`/`osi_GetTimes` added.
- `locname.c` — `<stdlib.h>` added; local externs for
  `CheckAMSConfiguration`/`la_FreeMD`/`FreeHost`/`Unquote` added.
- `locnamex.c` — `<stdlib.h>` added.
- `mailconf.c` — `<stdlib.h>` added; local externs for `LCappend`/
  `CheckServiceConfiguration` added.
- `parseadd.c` — `<stdlib.h>` added; same-file forward-declare block
  for `UnparseAddressList`/`PrintSimpleAddress`/`RemHost`/
  `FreeAddressList`/`RemAddress`; local externs for `StrFree`/
  `NoStorage`/`AppendHosts`/`la_FreeMD`/`mail_parseyylex`/
  `SetNextLine`/`pareset_lexer` added.
- `parseutl.c` — `<stdlib.h>` + `<string.h>` added; stale `#ifndef
  _IBMR2 extern char *malloc()` removed.
- `parsey.gra` — local extern for `StrFree` added to its existing
  prologue extern block (the real, hand-editable source behind
  generated `parsey.c`/`parsey.h`).
- `qmail.c` — local externs for `CheckAMSConfiguration`/`GetAuthInfo`/
  `LCappend`/`dbg_closedir`/`dbg_fclose` added.
- `scan822.c` — `<stdlib.h>` added (malloc-family blind-spot cleanup;
  zero census-visible errors in this file); stale `#ifndef _IBMR2
  extern char *malloc(), *realloc()` removed.
- `smtpsub.c` — local externs for `ParseAddressList`/
  `UnparseOneAddress`/`FreeAddressList` added (already had
  `<stdlib.h>`/`<string.h>`, so no header change needed here).
- `stats.c` — `<stdlib.h>` + `<util.h>` added; local externs for
  `errprintf`/`dbg_fclose`/`dbg_socket`/`dbg_close`/`osi_GetTimes`
  added.
- `usysex.c` — `<stdio.h>` added.
- `valhost.c` — `<arpa/inet.h>` added; local externs for
  `cptres_search`/`LCappend`/`CheckAMSConfiguration` added.
- `vmail.c` — local externs for `CheckAMSConfiguration`/`dbg_close`/
  `dbg_fclose`/`IsOnVice`/`GetRetPath`/`GetAuthInfo` added.

**9 files needed zero fixes of any kind** (confirmed clean under both
the `-Werror` census and the malloc-family sweep): `canon.c`,
`imap_prot.c`, `imapspike.c`, `imaptest.c`, `netrc.c`, `rsearch.c`,
`smtptest.c`, `sysexits.c`, `tlscon.c`.

Regenerated `Makefile` and generated `parsey.h`/`parsey.c`/`parsel.c`
are not fossil-tracked, not source edits.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     d5fd52a82f024dbde264823a36edf527d9f443dd 2026-07-25 03:10:01 UTC
parent:       0eef5ed18ab1496ab5e202fb462de69c822f27aa 2026-07-25 03:08:36 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout 4f prompt (overhead/mail/lib) (user: wdc)
EDITED     src/overhead/mail/lib/Imakefile
EDITED     src/overhead/mail/lib/addrambig.c
EDITED     src/overhead/mail/lib/amsauth.c
EDITED     src/overhead/mail/lib/amsconf.c
EDITED     src/overhead/mail/lib/arpadate.c
EDITED     src/overhead/mail/lib/authret.c
EDITED     src/overhead/mail/lib/capture.c
EDITED     src/overhead/mail/lib/ckamsdel.c
EDITED     src/overhead/mail/lib/dropoff.c
EDITED     src/overhead/mail/lib/fwdvalid.c
EDITED     src/overhead/mail/lib/genid.c
EDITED     src/overhead/mail/lib/locname.c
EDITED     src/overhead/mail/lib/locnamex.c
EDITED     src/overhead/mail/lib/mailconf.c
EDITED     src/overhead/mail/lib/parseadd.c
EDITED     src/overhead/mail/lib/parseutl.c
EDITED     src/overhead/mail/lib/parsey.gra
EDITED     src/overhead/mail/lib/qmail.c
EDITED     src/overhead/mail/lib/scan822.c
EDITED     src/overhead/mail/lib/smtpsub.c
EDITED     src/overhead/mail/lib/stats.c
EDITED     src/overhead/mail/lib/usysex.c
EDITED     src/overhead/mail/lib/valhost.c
EDITED     src/overhead/mail/lib/vmail.c
```

Exactly 24 files, matching the list above. No commit made.
`m2-mail-lib-session.diff` (636 lines) written to the tree root.

## 11. New taxonomy findings / corrections

- **No new fallout category.** All 124 real instances fit the three
  already-established categories (missing standard header; missing
  in-tree/project header with its sub-cases, including the
  now-familiar `fdplumb.h` partial-wrapper-family sub-shape; same-file
  forward reference). "Possible genuine bug/typo" remains empty.
- **No new taxonomy sub-case**, but two process/methodology notes
  worth carrying forward:
  1. **A self-inflicted `*/`-inside-a-comment syntax error** (§2 step
     7, `authret.c`) — writing an explanatory comment that happens to
     contain a literal `*/` substring (mine read "...declares the
     AMS_*/cell-service globals...") silently truncates the C comment
     early, and the remaining comment text gets parsed as code. Caught
     immediately by the very next rebuild (a real, loud syntax error,
     not a silent miscompile), but worth naming explicitly since it's
     an easy mistake to repeat when writing explanatory comments next
     to hand-written `extern` declarations in future M2 sessions —
     avoid `*/` appearing anywhere inside a comment's prose, including
     inside en-dash-separated asides.
  2. **§5's three-way split of the malloc-family sweep's findings**
     (genuinely invisible blind spot; stale-but-present wrong-typed
     declaration that only became a *conflict* once `<stdlib.h>` was
     added for an unrelated reason; completely dead/unused stale
     declaration) is not new in kind — each individual shape was
     already documented by `overhead/util/lib`'s and the metamail
     session's reports — but this is the first directory where **all
     three shapes appeared together, cleanly separable, in the same
     small sweep**. Worth keeping the three-way distinction explicit
     in future reports rather than collapsing them into one "cleanup"
     bucket, since only the first shape is real fallout-instance
     volume; the other two are hygiene.
- **`AppendHosts`'s pointer-returning signature** (§4, `ADDRESS_HOST
  *AppendHosts()`) is a direct, concrete instance of exactly the LP64
  bug class M2 exists to close: sourced the declaration from the real
  definition rather than guessing `int`, per the standing caution in
  `m2-rollout-runbook.md`'s taxonomy section.

## 12. Exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. `.do` files
are cached for the life of a process — use a **fresh** process for
each GUI check.

`overhead/mail/lib` backs mail-address parsing/SMTP/auth/dropoff logic
with **no GUI of its own** — it's a pure backend library. The
lowest-risk real exercise paths, from least to most infrastructure
required:

1. **Lowest-risk, no live mail infrastructure needed — address
   parsing round-trip via `arpadate`/`decode_id`** (the two tiny
   standalone tools rebuilt and `nm`-verified in §9):
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/etc/arpadate
   ```
   Confirm it prints a plausible RFC821-format date string
   (`Fri, 25 Jul 2026 HH:MM:SS +HHMM (...)`) and exits 0 — this
   exercises `arpadate.c`'s `sprintf` fix directly, with zero
   dependency on any mail server, AFS, or AMS infrastructure.

2. **Still standalone, no live server needed — `cuin`'s offline
   commands**: `cuin` needs a real AMS/mail-server connection for most
   of its functionality (out of reach in this environment per
   `sonnet-playbook.md`'s own framing of what this revival doesn't
   have), but it links and starts:
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/cuin
   ```
   Confirm it starts without crashing and reaches its own prompt or
   help text (`quit`/`?` should work even with no configured mail
   store) — this is a load-bearing exercise of `CheckAMSConfiguration`/
   `CheckServiceConfiguration`/`GetConfiguration` (§4's largest
   category), all of which run during `cuin`'s own startup
   configuration-loading sequence, before any network connection is
   attempted.

3. **The real load-bearing check — a fresh `messages` process,
   touching `amsn.do` directly** (per §9, this is the actual
   dynamically-loaded backend the GUI mail reader uses):
   ```
   DISPLAY=:0; messages
   ```
   Confirm the folder list loads (exercises the
   `CheckAMSConfiguration`/`dbg_*` family via normal directory/file
   I/O) and open a message if any test mailbox is configured. **What
   this needs that this revival doesn't have, confirmed rather than
   assumed**: a live AMS mail store or IMAP backend to show real
   folder contents — if none is configured, "does the app start and
   show an empty/error-free folder list rather than crashing" is
   still a meaningful, low-risk check of this session's fixes (all of
   which run during connection setup, not just during message
   delivery).
4. **What genuinely needs live infrastructure this revival doesn't
   have, and is explicitly NOT proposed here**: actually sending mail
   via `dropoff`/`qmail`/`smtpsub`'s SMTP path needs a real SMTP
   relay or `/usr/lib/sendmail` (confirmed absent per the metamail
   session's own finding); `vmail.c`'s AFS/Vice delivery path needs
   `AFS_ENV` (confirmed off in this build, per `overhead/util/lib`'s
   session); full address-validation (`fwdvalid.c`) against real
   White Pages needs `WHITEPAGES_ENV` infrastructure. None of these
   are testable standalone in this environment, and none needed to be
   for this session's fixes to be verified — every one of the 124
   instances is either a compile-time declaration fix or a
   `<stdlib.h>`/`<util.h>`/`<mail.h>`/`<arpa/inet.h>` include, with no
   semantic behavior change; #1–#3 above already exercise the large
   majority of the touched functions (everything in the
   `CheckAMSConfiguration`/`CheckServiceConfiguration`/`dbg_*`/
   `GetConfiguration` families, which together account for the bulk of
   the 124 instances) through ordinary startup/connection-setup code
   paths that run unconditionally, before any network or AFS access is
   attempted.

## 13. `fossil status` before/after, no commit

Reproduced in full in §10 — 24 files `EDITED`, none `ADDED`/`DELETED`,
no commit made at any point in this session. `fossil diff >
m2-mail-lib-session.diff` (636 lines) is the exact, complete record of
every change.

## 14. Command-style discipline — how it went this session

Followed the `atk/text`/`atk/rofftext`/`atk/table` sessions'
established adaptation exactly, having checked early per the prompt's
instruction: this session's Bash tool does **not** persist working
directory across calls (confirmed via a throwaway `cd`+`pwd` pair
before any real build step — identical failure mode to the three prior
bucket-4 sessions that checked this). Used `make -C <absolute-path>
<target>` for every build step (roughly 20 build-step calls across the
fix-surfacing pass, the two malloc-sweep-triggered rebuilds, the
`authret.c` comment-bug rebuild, two full determinism cycles, three
`.test`-target builds, and three consumer-directory rebuilds), each
its own unchained call.

**Zero permission prompts or denials on every build-step call.** The
`nm -g` calls needed the now-familiar two-step handling: a bare `nm -g
/absolute/path 2>&1` was denied outright (matching `overhead/util/lib`'s
and the metamail session's findings, not `atk/text`'s one-off contrary
result), reissued as `nm -g build/bin/<target>` with **no** `cd`
prefix at all this time (this session's default cwd already resolved
to the tree root for bare relative paths, so the `cd <tree-root> &&`
half of the established carve-out wasn's even necessary here) — went
through immediately, with the large output automatically persisted to
a tool-result file, grepped in a separate follow-up call rather than
piping. Confirms the pattern's core lesson (avoid the pipe/redirect,
not necessarily the whole compound shape) generalizes cleanly.

**One new methodology finding, not a command-style issue per se**:
early in the malloc-family sweep, I issued ~15 separate single-file
`grep` calls in one batch and tried to map each result back to its
source file by output content alone (variable names, string literals)
rather than by an explicit filename tag — this produced at least one
wrong attribution (misread a `netrc.c`-shaped result as `smtpsub.c`)
that I only caught by spot-checking two files directly afterward.
Switched to passing multiple files as arguments to a single `grep -n
...` call (still one Bash tool call, still zero loops) so `grep`'s own
`filename:line:text` prefixing does the attribution instead of
positional inference — recommend this as the default sweep shape for
future sessions instead of one-file-per-call with positional-order
trust, since parallel tool-call results are not guaranteed to preserve
invocation order in this harness (confirmed empirically here, not
merely suspected).

## 15. Open questions / anything that surprised you

- §2 step 7's self-inflicted `*/`-in-comment syntax error and §14's
  parallel-call result-ordering mismatch are both worth flagging
  together as the same underlying lesson: this session, more than any
  prior bucket-4 session, involved writing a lot of explanatory
  comments and cross-referencing a lot of parallel grep output at
  once (124 instances across 24 files, versus `atk/table`'s 186 across
  10) — both mistakes were caught immediately by the next build or a
  spot-check, not silently shipped, but the *rate* of this kind of
  slip seems to scale with instance/file count in a way worth watching
  for future large-directory sessions.
- The `AppendHosts`/pointer-return finding (§11) is a good concrete
  example of why M2 exists, worth keeping as a citable instance
  alongside the runbook's other named examples.
- §9's `mailto`-doesn't-actually-link-`libmail.a` correction is a
  second data point (after `atk/raster/cmd`'s `convertraster`
  correction in batch A) that a prompt's own example consumer name
  should be verified against the real Imakefile before testing against
  it, not trusted at face value — worth normalizing as a standing
  expectation rather than a recurring surprise.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled, findable
  definition), no header-vs-usage type disagreement requiring
  escalation (the `AppendHosts` pointer-return was resolved by sourcing
  the real signature, not a disagreement needing a ruling), and every
  fix stayed inside this directory's own `.c`/`.gra` files (no header
  outside `overhead/mail/lib`/`overhead/mail/hdrs` was ever edited —
  `capture.c`'s `#include <mail.h>` and `genid.c`/`stats.c`'s
  `#include <util.h>` all reach *existing* declarations in headers
  that live outside the flagged directory, which is explicitly fine
  per the taxonomy's own rule; only `overhead/mail/lib`'s own `.c`
  files and `parsey.gra` were ever written to).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
