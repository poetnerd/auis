# m2-census Gate 1 report

## Status

Stopped at Gate 1, as instructed. Census/classification only — no
fixes attempted, no files touched. `fossil status` is clean except
for the regenerated `dependInstall.log` (untracked build artifact,
tree root).

## What I did, in order

1. Read `sonnet-playbook.md` and the roadmap's "M2 point 0 —
   `-Wincompatible-pointer-types` triage" section (`roadmap.md`
   line ~1437) for the taxonomy and existing rulings.
2. **Gate 0.** `dependInstall.log` in the tree root predated the
   latest fossil commit (17:42 log vs. 21:54:15 commit). Per the
   prompt I stopped and asked before rebuilding; wdc approved a
   fresh build.
   - First attempt (`make dependInstall` from the tree root) failed
     immediately — the Makefile lives in `src/`, not the tree root
     (my error, not a tree problem). This clobbered the stale log
     with a one-line error; no information was lost since the log
     was being replaced anyway.
   - Corrected: `cd src && make dependInstall >& ../dependInstall.log`
     — succeeded, but produced an *incremental* log (973 KB, most
     files already built from the prior session, so most warnings
     don't re-fire).
   - Realized the census needs every warning, so did a full
     `make Clean` (from `src/`, ~30s) followed by
     `make dependInstall >& ../dependInstall.log` (from `src/`, ran
     to completion in the background). Result: 227,344-line log,
     483 `-Wincompatible-pointer-types` warnings — matches the
     roadmap's census count exactly, confirming this is the same
     complete warning set the M2-point-0 section was written from.
     `make Clean; make dependInstall` is the established
     gold-standard verification pattern per `porting-assessment.md`;
     4 pre-existing, unrelated `error:` lines survive (the known
     `ams/msclients/nns` `$(SSLLIB)` link failure and two
     `contrib/zip/utility/ltapp.c` int-conversion errors), consistent
     with prior full-build logs.
3. **Gate 1.** Extracted and classified all 67 `int */long*`
   instances and all 18 `char ** → char *` instances (below), then
   ran the stretch-goal grep over `ams/libs/ms` and `ams/libs/cui`.

No source files were edited. `dependInstall.log` (tree root,
untracked) is left at its fresh, full-build state for whoever picks
up the fixing pass.

---

## Cluster 1: `int */long *`, 67 instances

All 67 resolve to 13 shared root shapes — one callee (or callee
pair) each. Direction matters: **int-declared-but-long*-expected**
(9 shapes, 45 instances) means the callee's 8-byte store overruns a
4-byte stack slot and can stomp *adjacent* locals — this is worse
than a truncation, it's an out-of-bounds write. **long-declared-but-
int*-expected** (1 shape, 22 instances, all one callee) means the
callee only fills the low 4 bytes, leaving the caller's upper 4
bytes whatever they already were — the exact shape of the
already-documented `MS_GetConfigurationParameters` bug (playbook LP64
class 4), just with a warning attached here because it crosses a
classpp typed-dispatch boundary instead of a bare K&R call.

### Group A — int local, long* callee (stack-overrun direction)

| # | Callee (real `.ch`/K&R signature) | Sites (file:line) | Caller expression | Reasoning |
|---|---|---|---|---|
| 4 | `fontdesc_ExplodeFontName`/local `XExplodeFontName`, both `(..., long *fontStyle, long *fontSize)` | [xfontd.c:372](../../src/atk/basics/x/xfontd.c#L372), [xfontd.c:375](../../src/atk/basics/x/xfontd.c#L375) | `&style, &size` — both declared `int style; int size;` in `ClosestFonts` ([xfontd.c:353-354](../../src/atk/basics/x/xfontd.c#L353)) | **suspected-real.** Same call shape used twice (class method + local X11-name-parsing fallback), same two `int` locals. An 8-byte write to `&style` overruns into `size`'s storage (or padding) each of the 4 times it fires. |
| 8 | `message_MultipleChoiceQuestion(..., long *result, ...)` (`message.ch:78`) | [sbttnav.c:458](../../src/atk/supportviews/sbttnav.c#L458), [pshbttnv.c:1047](../../src/atk/hyplink/pshbttnv.c#L1047), [linkv.c:475](../../src/atk/hyplink/linkv.c#L475), [imagev.c:2068](../../src/atk/image/imagev.c#L2068), [bushv.c:1288](../../src/atk/bush/bushv.c#L1288), [bushv.c:1595](../../src/atk/bush/bushv.c#L1595), [bushv.c:2122](../../src/atk/bush/bushv.c#L2122), [stubs.c:181](../../src/atkams/messages/lib/stubs.c#L181) | `&choice`/`&answer`/`&result`/`&myans`, each declared `int` at the call site | **suspected-real, one root shape.** Same out-param, 8 independent call sites, all with an `int` local. Dialog/menu-choice result stomps whatever stack slot follows it; in most of these functions that's another `int` (e.g. `bushv.c:2122`'s `sort`/`current_mode`). |
| 2 | `chlist_GetIndexByPosition(..., long *regionID, ...)` (`chlist.ch:62`) | [chlistv.c:102](../../src/atk/textobjects/chlistv.c#L102), [chlistv.c:120](../../src/atk/textobjects/chlistv.c#L120) | `&region`, declared `int region;` in both `chlistview__Hit` and `chlistview__ActivateItem` | **suspected-real**, same shape both sites. |
| 6 | `filetype_Lookup(..., long *objectID, ...)` (`filetype.ch:49`) | [cel.c:398](../../src/atk/adew/cel.c#L398), [celv.c:467](../../src/atk/adew/celv.c#L467), [lsetv.c:427](../../src/atk/adew/lsetv.c#L427), [layoutv.c:1183](../../src/atk/layout/layoutv.c#L1183), [boxview.c:468](../../src/atk/layout/boxview.c#L468), [foldaux.c:393](../../src/atkams/messages/lib/foldaux.c#L393) | `&objectID` (5 sites) / `&myid` (foldaux.c), all declared `int` | **suspected-real, one root shape** across 6 independent files/views (cel/celv/lsetv share the exact same idiom, comment and all: `/* For now, ignore attributes. */`). |
| 7 | `fontdesc_StringSize`/`fontdesc_TextSize`, both `(..., long *, long *)` (`fontdesc.ch:91,93`) | [headrtv.c:458](../../src/contrib/mit/util/headrtv.c#L458) (×2 args), [headrtv.c:465](../../src/contrib/mit/util/headrtv.c#L465) (×2), [zipve00.c:883](../../src/contrib/zip/lib/zipve00.c#L883) (×2), [fillerv.c:205](../../src/atk/layout/fillerv.c#L205) (×1, 2nd arg is `NULL`) | `&width,&junk` (headrtv, `int width,junk,left;` — adjacent), `&xp,&yp` (zipve00, `int xp, yp;` — adjacent), `&w` (fillerv, `int w;` next to `int i;`) | **suspected-real.** headrtv.c is the sharpest case: `width`, `junk`, `left` are three adjacent `int`s and the 8-byte write through `&width` directly overlaps `junk`'s storage — this one is not hypothetical, the corruption target is a named sibling variable one word away. |
| 14 | `ams_MS_GetDirInfo`/`MS_GetNewMessageCount`/`MS_GetSubscriptionEntry`/`MS_NameChangedMapFile`, all `long *` out-params (`ams.ch:135-140`) | [capaux.c:413](../../src/atkams/messages/lib/capaux.c#L413) (×2), [capaux.c:425](../../src/atkams/messages/lib/capaux.c#L425) (×2), [capaux.c:595](../../src/atkams/messages/lib/capaux.c#L595), [folders.c:363](../../src/atkams/messages/lib/folders.c#L363) (×5), [folders.c:774](../../src/atkams/messages/lib/folders.c#L774), [foldaux.c:309](../../src/atkams/messages/lib/foldaux.c#L309) (×2), [foldaux.c:327](../../src/atkams/messages/lib/foldaux.c#L327) | all `int` locals, several declared on one shared line: `capaux.c:389: int PositionDot = 1, NewCt, TotalCt, ProtCode;`; `folders.c:363`'s block: `int changed, unavail, missing, slow, fast;` | **suspected-real, highest-severity site in this cluster is `folders.c:363`**: five `int`s declared adjacently, all five addresses passed to one call that treats each as `long *` — each 8-byte write partially overlaps the *next* declared variable, so the five out-values corrupt each other in sequence, not just abstractly "adjacent stack." `capaux.c:413/425` is the same pattern with `ProtCode`/`TotalCt`/`NewCt` sharing one declaration line. |
| 1 | `text_GetBuf(..., long *lenp)` (`smpltext.ch:88`) | [captions.c:227](../../src/atkams/messages/lib/captions.c#L227) | `&retlen`, declared `int dot, len, tmpdot, retlen;` | **suspected-real**; `retlen` is last in its declaration group, so the overrun lands on whatever the compiler placed next (not a named sibling here, but still an out-of-bounds 4-byte write past a 4-byte object). |
| 3 | `suite_SelectedItems(long *number)` (`suite.ch:154`) | [bushv.c:1371](../../src/atk/bush/bushv.c#L1371), [bushv.c:1776](../../src/atk/bush/bushv.c#L1776), [bushv.c:2281](../../src/atk/bush/bushv.c#L2281) | `&count`, each `int count = 0;` (or `int msg_status = 0, count = 0;` at line 2281) declared separately per function | **suspected-real**, same shape 3×, independent local scopes. |

Group A subtotal: 4+8+2+6+7+14+1+3 = **45 instances**.

### Group B — long local, int* callee (half-fill direction)

| # | Callee (real `.ch` signature) | Sites | Caller expression | Reasoning |
|---|---|---|---|---|
| 22 | `fontdesc_StringBoundingBox(struct graphic*, char*, int *width, int *height)` — **the odd one out**: its fontdesc siblings `StringSize`/`TextSize` both take `long *` (`fontdesc.ch:91-93`); only `StringBoundingBox` (`fontdesc.ch:92`) is still `int *` | [suite.c:1004](../../src/atk/apt/suite/suite.c#L1004), [suite.c:1008](../../src/atk/apt/suite/suite.c#L1008), [suiteev.c:782](../../src/atk/apt/suite/suiteev.c#L782), [suiteev.c:2039](../../src/atk/apt/suite/suiteev.c#L2039), [suiteev.c:2311](../../src/atk/apt/suite/suiteev.c#L2311), [suiteev.c:2326](../../src/atk/apt/suite/suiteev.c#L2326), [suiteev.c:2345](../../src/atk/apt/suite/suiteev.c#L2345), [suiteev.c:2371](../../src/atk/apt/suite/suiteev.c#L2371), [suiteev.c:2382](../../src/atk/apt/suite/suiteev.c#L2382), [suiteev.c:2398](../../src/atk/apt/suite/suiteev.c#L2398), [lookzv.c:1220](../../src/atk/lookz/lookzv.c#L1220) — 11 sites × 2 args | All `long` locals, and in every site I checked they are **zero-initialized** before use (`long XWidth = 0, YWidth = 0;` in `suite.c`/`suiteev.c`; `static long hcache = 13;` in `lookzv.c`, whole-word literal init) and reassigned every loop iteration right before being read | **suspected-real, but currently non-corrupting given today's call pattern** — same underlying defect class as the already-documented `MS_GetConfigurationParameters` bug (LP64 class 4: callee `int*` out-param, caller `long*`, store fills only the low half), just caught here by the compiler because it crosses classpp's typed dispatch instead of a bare K&R prototype. It doesn't visibly misbehave today only because (a) every site zero-inits before first use and (b) on-screen pixel widths/heights never approach 2^31, so the untouched upper 32 bits staying zero happens to be correct. That's fragile by construction, not fixed by construction — the classification is real, not benign, but low urgency unless `StringBoundingBox`'s `.ch` signature stays `int*` while callers keep migrating to `long`. |

Group B subtotal: **22 instances**.

**45 + 22 = 67**, matching the grep count exactly.

---

## Cluster 2: `char ** → char *`; remove `&`, 18 instances

All 18 collapse to **one root cause**, and it is the opposite of
what the compiler's own "remove `&`" fix-it hint suggests.

| Callee | `.ch` signature (wrong) | Real K&R implementation (`ams/libs/cui/cuilib.c`) | Sites |
|---|---|---|---|
| `CUI_DisambiguateDir` | `(char *shortname, char *longname)` — `ams.ch:86` | [cuilib.c:1035](../../src/ams/libs/cui/cuilib.c#L1035): `char *shortname; char **longname;` — does `*longname = DNtmp->FullName;`, a genuine out-pointer, no buffer copy | [messages.c:611](../../src/atkams/messages/lib/messages.c#L611), [messages.c:640](../../src/atkams/messages/lib/messages.c#L640), [messages.c:1349](../../src/atkams/messages/lib/messages.c#L1349), [messages.c:1371](../../src/atkams/messages/lib/messages.c#L1371), [messages.c:1628](../../src/atkams/messages/lib/messages.c#L1628), [capaux.c:394](../../src/atkams/messages/lib/capaux.c#L394), [capaux.c:582](../../src/atkams/messages/lib/capaux.c#L582), [sendmsg.c:1395](../../src/atkams/messages/lib/sendmsg.c#L1395), [sendaux.c:293](../../src/atkams/messages/lib/sendaux.c#L293), [folders.c:821](../../src/atkams/messages/lib/folders.c#L821), [folders.c:989](../../src/atkams/messages/lib/folders.c#L989), [folders.c:990](../../src/atkams/messages/lib/folders.c#L990), [folders.c:993](../../src/atkams/messages/lib/folders.c#L993) — 13 sites |
| `CUI_RewriteHeaderLine` | `(char *addr, char *newaddr)` — `ams.ch:115` | [cuilib.c:1478](../../src/ams/libs/cui/cuilib.c#L1478): `char *text, **newtext;` — `*newtext = NULL;` then `GetViceFileToNewString(FileName, newtext, TRUE)` allocates and stores a fresh pointer | [messages.c:1239](../../src/atkams/messages/lib/messages.c#L1239), [sendaux.c:167](../../src/atkams/messages/lib/sendaux.c#L167), [sendaux.c:197](../../src/atkams/messages/lib/sendaux.c#L197), [sendaux.c:313](../../src/atkams/messages/lib/sendaux.c#L313) — 4 sites |
| `CUI_RewriteHeaderLineInternal` | `(..., char *newaddr, ...)` — `ams.ch:116` | [cuilib.c:1486](../../src/ams/libs/cui/cuilib.c#L1486): `char *text, **newtext;` — same as above, called by `CUI_RewriteHeaderLine` itself | [sendmsg.c:1272](../../src/atkams/messages/lib/sendmsg.c#L1272) — 1 site |

**Classification: suspected-real, but the bug is in the `.ch` class
spec, not at any of the 18 call sites.** Every caller I sampled
(`&FullName`/`&DirName`/`&valaddr`/`&realname`/`&name`, each declared
`char *` at its call site) is doing exactly what the real
implementation needs — passing the address of a `char *` so the
callee can hand back a pointer it owns (a cache entry for
`DisambiguateDir`, a freshly `malloc`'d string for
`RewriteHeaderLine`). The three `.ch` entries were typed `char *`
instead of `char **`, almost certainly a drive-by clerical slip from
the same class of `.ch`/K&R signature drift M1 spent its whole
rollout finding and fixing elsewhere — these three methods just
never got a live pilot fix. **If anyone (or any tool) follows clang's
own "remove &" suggestion, it will break all 18 call sites**: the
callee would then read/write through the caller's `char *` as if it
were the string buffer itself (`CUI_DisambiguateDir` dereferencing a
possibly-uninitialized `char *` as `*longname = ...` — wait, with the
`&` removed the caller passes a `char *`, but the callee still does
`*longname = ...` through what the compiler now types as `char *`,
i.e. writes a byte where a pointer used to go — an immediate,
silent-until-crash type-pun). The fix belongs in `ams.ch` (three
`char *` → `char **`), not at any call site — flagging for whoever
picks up the fixing pass so they don't take the compiler's fix-it
literally.

---

## Stretch goal: variant #5 sweep (`ams/libs/ms`, `ams/libs/cui`)

Grepped both directories for K&R function definitions whose next
line declares an `int *` parameter (the shape that produces *no*
warning when a caller passes a `long *`, since the call crosses an
untyped/implicitly-declared boundary — see `MS_GetConfigurationParameters`,
fixed 2026-07-18, `init.c`). Found 8 such functions
(`MS_CheckAuthentication`, `MS_DoIHaveMail`, `MS_ConvertOldMail`,
`FlushClosableDir`, `MS_Initialize`, `MS_DomainHandlesFormatting`,
`MS_RebuildMasterUpdateFiles`, `CheckForOldFashionedSubscriptions`),
then checked every caller.

Most are fine: `ams/msclients/cui/cuifns.c`, `ams/msclients/vui/vuibase.c`,
and `ams/ms/ms.c` all call these directly (not through class dispatch)
and every caller I found declares a matching `int` local
(`Authenticated`, `newmail`, `mc`, `good`/`bad`, `fastgood`/`slowgood`/
`bad`/`absent`/`probablygood`, `NumGood`/`NumBad` — all `int`). No
mismatch there.

Two functions are also exposed as **class methods typed `long *`** in
`ams.ch` (the M1 conversion widened every class-layer out-param to
`long *` tree-wide), and their `amss__`/`ams__`/`amsn__` wrappers
forward that `long *` straight into the real `int *` implementation
with no cast and no warning (K&R call, no prototype in scope):

- **`MS_CheckAuthentication`** — `.ch` says `long *auth`
  ([ams.ch:131](../../src/atkams/messages/lib/ams.ch#L131)); real
  implementation is `int *Authenticated`
  ([autherr.c](../../src/ams/libs/ms/autherr.c)). Wrapper:
  [amss.c:429-434](../../src/atkams/messages/lib/amss.c#L429).
  **No caller currently invokes it through class dispatch**
  (`ams_MS_CheckAuthentication` has zero call sites outside
  `ams.c`/`amss.c`/`amsn.c` themselves) — dormant, not live. Suspect,
  low urgency.
- **`MS_DomainHandlesFormatting`** — `.ch` says `long *retval`
  ([ams.ch:150](../../src/atkams/messages/lib/ams.ch#L150)); real
  implementation is `int *codeP`
  ([mswp.c](../../src/ams/libs/ms/mswp.c)). Wrapper:
  [amss.c:591-596](../../src/atkams/messages/lib/amss.c#L591). Same
  situation — **dormant**, no class-dispatch caller found.

**One is live and confirmed real** — the strongest single finding of
this stretch section:

- **`MS_ParseDate`** — `.ch` declares all eight out-params `long *`
  ([ams.ch:142](../../src/atkams/messages/lib/ams.ch#L142)); the real
  implementation takes `int *year, *month, *day, *hour, *min, *sec,
  *wday, *gtm`
  ([msparse.c:39-41](../../src/ams/libs/ms/msparse.c#L39)). The wrapper
  ([amss.c:530-538](../../src/atkams/messages/lib/amss.c#L530))
  forwards the `long *`s straight through, no warning (K&R call).
  **This is reached**, twice, from
  [captions.c:1170](../../src/atkams/messages/lib/captions.c#L1170)
  and [captions.c:1184](../../src/atkams/messages/lib/captions.c#L1184)
  (`captions__MarkRangeOfMessages`, the "mark messages since/through
  date" feature), via
  `ams_MS_ParseDate(ams_GetAMS(), DBuf, &year, &month, &day, &hour,
  &min, &sec, &wday, &gtm)`. Unlike the Group B `StringBoundingBox`
  sites above, **the locals here are not zero-initialized**:
  [captions.c:1158](../../src/atkams/messages/lib/captions.c#L1158)
  declares `long year, month, day, hour, min, sec, wday, gtm;` with no
  initializer, so after the call each variable's low 32 bits hold the
  real parsed value and its high 32 bits hold whatever garbage was on
  the stack — not the "happens to stay zero" mitigation Group B relies
  on. `gtm` is then fed straight to `amsutil_convlongto64(gtm, 0)` to
  build the search-range date string. **This one should probably move
  straight to the fixing queue** rather than wait for a general M2
  sweep — it's a live, user-triggered feature (mark-by-date) computing
  a wrong value from uninitialized stack garbage, exactly the shape
  the roadmap already treats as a confirmed bug class, just not yet
  caught by grep because the compiler can't see across the K&R
  boundary.

`ams/libs/cui`'s remaining `long *`/`int *` `.ch` entries
(`CUI_GetHeaders`'s `nbytes`/`status`, `CUI_ReallyGetBodyToLocalFile`'s
`ShouldDelete`, `CUI_GetCuid`'s `isdup`) were spot-checked and are
internally consistent — real implementation and `.ch` agree, callers
declare the matching width. No further suspects found in the time
available.

---

## Files touched

None. `dependInstall.log` (tree root) was regenerated per Gate 0 and
left in place as a fresh, full-build artifact — untracked, not a
fossil-tracked file.

## Open questions / next steps for the fixing pass

1. Whether to fix the `char**` cluster (three `.ch` line edits) before
   or alongside the `int*/long*` clusters — it's the cheapest fix in
   this whole census (3 signature lines fix all 18 warnings at once)
   and, unlike the compiler's own suggestion, the *only* correct fix
   is in `ams.ch`, not at any call site.
2. `MS_ParseDate` (stretch section) looks urgent enough to jump the
   queue — it's live, reachable, and uses uninitialized stack memory,
   not just a latent half-fill.
3. Group B (`fontdesc_StringBoundingBox`) is real but not urgent under
   current call patterns; worth deciding whether the fix is widening
   `StringBoundingBox`'s `.ch` signature to `long *` (matching its
   siblings) or leaving it `int *` and instead fixing callers — the
   `.ch` fix is one line and matches the sibling methods, so probably
   preferred, but that's a decision for the fixing pass, not this
   census.
4. Didn't get to a byte-for-byte accounting of *which* adjacent
   variable each Group A overrun actually lands on beyond the ones I
   could read straight off the declaration (`headrtv.c`'s
   `width`/`junk`, `folders.c:363`'s five-in-a-row) — a fixing pass
   should re-check stack layout empirically (e.g. via `-S` or lldb)
   rather than trust declaration order, which the compiler is free to
   reorder.
