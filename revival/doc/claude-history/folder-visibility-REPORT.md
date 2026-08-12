# folder-visibility Gate 1 report

## Status

Stopped at Gate 1, as instructed. Investigation only — no files touched
(tree stays clean; `fossil status` at session end matches session
start, checkout `0db753c0f9`). No scratch-root experiment was needed;
the 2026-07-22 status update's own real data (`~/.AMS.prof`, read
only, never modified) was sufficient to trace the mechanism precisely.

## Reframing

The prompt's premise — default visibility is gated by subscription
status ("is it unsubscribed, or simply unknown?") — does not hold.
`INBOX` is fully subscribed (`status 2`, `AMS_ALWAYSSUBSCRIBED`) and
still doesn't show at startup. The real gate is a second, independent
mechanism: per-folder "have I read this" bookkeeping that the default
startup view filters on, layered on top of (not instead of)
subscription. Evidence below.

## 1. Where the default list comes from; what each Expose command changes

Four listing modes, all built by `SetupList()`
([folders.c:316](../../src/atkams/messages/lib/folders.c#L316)),
triggered by `folders_Reconfigure`/`folders_UpdateMsgs`
([foldaux.c:219](../../src/atkams/messages/lib/foldaux.c#L219),
[foldaux.c:232](../../src/atkams/messages/lib/foldaux.c#L232)) from
these menu handlers
([messaux.c:235-257](../../src/atkams/messages/lib/messaux.c#L235)):

| Menu item          | code               | pathindex                          | ShowUnsubscribed | HasNew filter |
|--------------------|--------------------|-------------------------------------|:---:|:---:|
| Expose New (default at startup) | `LIST_NEWONLY`     | all of `SubsInPathOrder` (profile) | n/a | **yes** |
| Expose Personal    | `LIST_MAIL_FOLDERS`| `AMS_MAILPATH` only (`$HOME/.MESSAGES`, hardcoded) | 1 | no |
| Expose All         | `LIST_ALL_FOLDERS` | every mspath element, on-disk walk | 1 | no |
| Expose Subscribed  | `LIST_SUBSCRIBED`  | profile entries only               | 0 | no |

- **Expose All** walks the filesystem
  (`ams_MS_NameSubscriptionMapFile` →
  [namemap.c:41](../../src/ams/libs/ms/namemap.c#L41) →
  `SubsTreeWalk`,
  [subswalk.c:50](../../src/ams/libs/ms/subswalk.c#L50)), which just
  lists every subdirectory under each mspath root — existence, not
  subscription. `ShowUnsubscribed=1`
  ([folders.c:336](../../src/atkams/messages/lib/folders.c#L336))
  means the filter at
  [folders.c:472](../../src/atkams/messages/lib/folders.c#L472)
  never excludes anything. All of this is transient view state:
  `folders_Reconfigure` just sets `self->HasSetUp=0` and reruns
  `SetupList` ([foldaux.c:225](../../src/atkams/messages/lib/foldaux.c#L225));
  nothing is written to `~/.AMS.prof`.
- **Expose Personal** uses the *same* per-root walk, but `pathindex`
  is hardcoded to `AMS_MAILPATH`
  ([folders.c:346](../../src/atkams/messages/lib/folders.c#L346)),
  which `MS_GetSearchPathEntry`
  ([getpathe.c:53](../../src/ams/libs/ms/getpathe.c#L53)) resolves to
  literally `$HOME` + `MAILSEARCHPATHTEMPLATE` (`"/.MESSAGES"`,
  [util.c:50](../../src/ams/libs/ms/util.c#L50)) — **not** one of the
  mspath entries from `~/preferences`. This is why a mirrored root
  (`~/.IMAP/fastmail/.MESSAGES`, a *different* directory than
  `$HOME/.MESSAGES`) can never appear under Expose Personal no matter
  what its subscription state is — verified: `mail` and `amsdemo` are
  both physically under `$HOME/.MESSAGES` (`ls ~/.MESSAGES`), INBOX is
  under `~/.IMAP/fastmail/.MESSAGES`.
- **Expose Subscribed** doesn't walk the filesystem at all; for an
  empty `Root` argument, `MS_NameSubscriptionMapFile`
  ([namemap.c:50](../../src/ams/libs/ms/namemap.c#L50)) calls
  `WriteSimpleSubsMap`
  ([subs.c:948](../../src/ams/libs/ms/subs.c#L948)), which just dumps
  every profile entry with `status != AMS_UNSUBSCRIBED`
  ([subs.c:961](../../src/ams/libs/ms/subs.c#L961)) — no HasNew field
  in the output, so `folders.c`'s parser defaults `HasNew=1`
  ([folders.c:459](../../src/atkams/messages/lib/folders.c#L459)),
  which always passes the filter at
  [folders.c:473](../../src/atkams/messages/lib/folders.c#L473).
  This is the mode INBOX *does* show under, once subscribed.
- **Expose New** (the real startup default — see below) is the odd
  one out: it's the only mode where `HasNew` is a real, computed
  value rather than a default.

## 2. Where subscriptions persist

`~/.AMS.prof` (`AMS_SUBSPROFFILE`,
[ms.h:189](../../src/ams/libs/hdrs/ms.h#L189)), one line per folder,
written by `WriteSubs()`
([subs.c:174](../../src/ams/libs/ms/subs.c#L174), format at
[subs.c:240](../../src/ams/libs/ms/subs.c#L240)):

```
<sname> <fullpath> <status> <time64> <filedate>
```

Actual file content (read only, not modified this session):

```
$mail /Users/wdc/.MESSAGES
$official /afs/andrew.cmu.edu/usr0/bb/off/.MESSAGES
$local /afs/andrew.cmu.edu/usr0/bb/.MESSAGES
$external /afs/andrew.cmu.edu/usr0/netbb/.MESSAGES
amsdemo /Users/wdc/.MESSAGES/amsdemo 0 1eIRJW 0
mail /Users/wdc/.MESSAGES/mail 2 000000 0
INBOX /Users/wdc/.IMAP/fastmail/.MESSAGES/INBOX 2 1XoBum 0
```

`status` is `AMS_UNSUBSCRIBED=0` / `ASKSUBSCRIBED=1` /
`ALWAYSSUBSCRIBED=2` / `SHOWALLSUBSCRIBED=3` / `PRINTSUBSCRIBED=4`
([ams.h:130-134](../../src/ams/libs/hdrs/ams.h#L130)). This part
matches the original premise: `mail`/`INBOX` are subscribed,
`amsdemo` is not.

**The catch is columns 4-5.** `time64`/`filedate` are *not*
"subscribed since" — they are the per-folder **last-read marker**,
written by `MS_SetAssociatedTime` →`SetProfileEntry`
([setasct.c:69](../../src/ams/libs/ms/setasct.c#L69),
[subs.c:741](../../src/ams/libs/ms/subs.c#L741)) every time a folder
is actually browsed (opened in captions view), and read back by
`MS_GetAssociatedTime`→`GetAssocTime`
([getasct.c:13](../../src/ams/libs/ms/getasct.c#L13),
[subs.c:928](../../src/ams/libs/ms/subs.c#L928)) — same struct field,
same file. Subscribing a folder (`MS_SetSubscriptionEntry` →
`SetSubsEntry`, [setsubs.c:63](../../src/ams/libs/ms/setsubs.c#L63))
calls `SetFullProfileEntry(DoSubs=TRUE, DoProf=FALSE, ...)`
([subs.c:738](../../src/ams/libs/ms/subs.c#L738)) — it only ever
touches `status`, never `time64`.

## 3. Why a subscribed folder still defaults to hidden

Real startup path, traced end to end:

1. `messagesapp__Start` (no CLI folder args) calls
   `folders_UpdateMsgs(fold, m->MailOnly /* FALSE */, NULL, FALSE)`
   ([msgsa.c:160](../../src/atkams/messages/lib/msgsa.c#L160));
   `m->MailOnly` defaults `FALSE`
   ([msgsa.c:63](../../src/atkams/messages/lib/msgsa.c#L63)).
2. `folders__UpdateMsgs` sets `self->MailOnlyMode = mailonly` (FALSE)
   and, since `thingstoread` is NULL, calls
   `SetupList(self, LIST_NEWONLY, NULL)`
   ([foldaux.c:249-261](../../src/atkams/messages/lib/foldaux.c#L249)).
3. `SetupList`'s `LIST_NEWONLY` case calls
   `ams_MS_NameChangedMapFile(ams_GetAMS(), MapFile, ci->MailOnlyMode,
   TRUE, ...)` ([folders.c:363](../../src/atkams/messages/lib/folders.c#L363))
   — `ListAll=TRUE` is a hardcoded literal, `MailOnlyMode=FALSE`.
4. `MS_NameChangedMapFile`
   ([namechg.c:82](../../src/ams/libs/ms/namechg.c#L82)) iterates
   **every** entry in `SubsInPathOrder` (the whole profile, not
   filtered by mspath root since `MailOnly` is false — the
   `mailpathelt` checks at
   [namechg.c:129](../../src/ams/libs/ms/namechg.c#L129) and
   [namechg.c:268](../../src/ams/libs/ms/namechg.c#L268) don't
   trigger). It skips `AMS_UNSUBSCRIBED` entries
   ([namechg.c:128](../../src/ams/libs/ms/namechg.c#L128)) — this is
   as far as the original "unsubscribed vs unknown" theory goes. For
   everything else it computes a real `HasChanged` (new-mail) flag
   per folder and, because `ListAll=TRUE`, writes **every**
   non-unsubscribed entry to the map with that real flag
   ([namechg.c:280-281](../../src/ams/libs/ms/namechg.c#L280)):
   `fprintf(outfp, "%s:%s %d %d\n", sname, key, status, HasChanged?1:0)`.
5. Back in `folders.c`'s parser, line
   [folders.c:473](../../src/atkams/messages/lib/folders.c#L473):
   `if (!HasNew && substatus != AMS_ASKSUBSCRIBED && substatus !=
   AMS_SHOWALLSUBSCRIBED) continue;` — **a subscribed
   (`ALWAYSSUBSCRIBED`) folder is only listed if it currently has new
   mail.** This is the actual default-visibility gate, and it's
   orthogonal to subscription.
6. "Has new mail" is computed by `MS_GetNewMessageCount`
   ([getnmct.c:38](../../src/ams/libs/ms/getnmct.c#L38)), which reads
   the folder's `AssociatedTime` (`time64`) and short-circuits:
   - `AssTime[0]=='\0' || conv64tolong(AssTime)==0` → **"first time
     ever"** branch ([getnmct.c:80](../../src/ams/libs/ms/getnmct.c#L80)):
     `*numnew = min(10, numtotal)` — always reports new mail, as long
     as the folder is non-empty, regardless of actual freshness.
   - Otherwise, it does a real binary search against message dates
     vs. the stored `AssTime` ([getnmct.c:91-133](../../src/ams/libs/ms/getnmct.c#L91)) —
     genuine "anything newer than last read" logic.
7. New profile entries default `time64 = "000000"`
   ([subs.c:811](../../src/ams/libs/ms/subs.c#L811), in
   `SetFullProfileEntry`'s "IsNew" branch) — the literal sentinel
   `MS_GetNewMessageCount` treats as "never read."

Putting 2+3+6+7 together against the actual `.AMS.prof` data:

- **`mail`**: `time64 = 000000` — never been browsed in this
  environment → permanently in the "first time ever" branch →
  *always* reports as new (up to 10 msgs) → always shows in the
  default view, independent of whether anything is actually
  unread. This is why "mail" looks special — it isn't; it's just
  never been read yet.
- **`INBOX`**: `time64 = 1XoBum` — a real, non-zero associated time.
  Per the prompt's own background, INBOX was browsed earlier this
  investigation (the session that hit the now-fixed
  `MS_SetAssociatedTime`/`FindInDirCache` exit crash). That browse
  called `SetProfileEntry`
  ([setasct.c:69](../../src/ams/libs/ms/setasct.c#L69)) and moved
  `time64` off `000000`, *before* the later explicit Subscribe action
  (which only ever touches `status`, never `time64` —
  [setsubs.c:63](../../src/ams/libs/ms/setsubs.c#L63)). Since imapsync
  is a one-way, on-demand mirror (nothing newer has been pulled in
  since), `MS_GetNewMessageCount` now correctly computes zero new
  messages for INBOX → filtered out of Expose New. **This is not a
  mirroring bug and not a subscription bug — it is the "new mail"
  view correctly reporting no new mail**, for a folder that happens
  to have been read once already.
- **`amsdemo`**: `time64 = 1eIRJW` (also previously read) *and*
  `status = 0` (unsubscribed) — doubly excluded from Expose New, and
  from Expose Subscribed (status check); shows under Expose
  All/Personal only because those ignore both gates. Matches the
  observed "amsdemo shows via Personal, not via Subscribed."

All four exposure-mode behaviors reported in the task description are
now fully explained, self-consistently, by these two independent
gates (mspath-root reachability, and subscription+AssociatedTime) —
no remaining unexplained cases.

## 4. Proposal

The minimal change is to make imapsync auto-subscribe a folder
**at the moment it first creates the local mirror directory** — never
on a later re-sync of an existing folder — by calling
`MS_SetSubscriptionEntry(path, NULL, AMS_ALWAYSSUBSCRIBED)` right
after the successful `MS_CreateNewMessageDirectory` call in
`ensure_folder_dir()`
([imap_sync.c:253](../../src/ams/msclients/imapsync/imap_sync.c#L253)),
guarded by the same "not yet a message directory" branch that gates
`MS_CreateNewMessageDirectory` itself
([imap_sync.c:250-257](../../src/ams/msclients/imapsync/imap_sync.c#L250)) —
i.e. exactly once per folder, never touching a folder a user later
chose to unsubscribe from by hand.

Because `SetFullProfileEntry`'s "new entry" branch initializes
`time64 = "000000"` unconditionally
([subs.c:811](../../src/ams/libs/ms/subs.c#L811)), and
`MS_SetSubscriptionEntry` never touches `time64`
([setsubs.c:63](../../src/ams/libs/ms/setsubs.c#L63)), this single
call gets both gates right together: `status=ALWAYSSUBSCRIBED` *and*
`time64="000000"` land atomically on first creation, reproducing
`mail`'s exact "always shows in Expose New until first read" behavior
— no separate AssociatedTime handling needed.

This is not a new idea for this codebase — it is the same pattern
`ams/libs/ms/direx.c:118` already uses to auto-subscribe a
newly-created native mail folder
(`MS_SetSubscriptionEntry(MailDir, Nick, AMS_ALWAYSSUBSCRIBED)`).
imapsync would simply be doing, at the one point it creates a
directory the store doesn't know about yet, what `direx.c` already
does at the equivalent point for locally-created folders.

**Rejected alternatives:**
- *(b) MS layer treats no-subscription-data folders under any mspath
  root as subscribed-by-default*: this was built on the original
  (now-disproven) premise that subscription status alone is the
  gate. Even done perfectly it wouldn't fix default visibility unless
  it also arranged for `time64` to stay `"000000"` on the same entry
  — at which point it collapses into the same fix as (a), just
  applied at the wrong layer (blanket policy for every mspath root,
  including AFS bboard roots this revival doesn't even touch, versus
  a two-line addition scoped to the one tool that actually creates
  these directories).
- *(c) messages-side default change* (e.g. defaulting the startup
  view to Expose Subscribed instead of Expose New): would show every
  subscribed folder always, not just new ones — for a classic AMS
  user with many subscribed bboards this defeats the entire purpose
  of Expose New (decluttering to "what's actually new"), a real
  regression for exactly the audience the prompt asks to protect.

**What (a) would break for a classic-AMS user:** nothing found. It
only fires the first time imapsync creates a given mirror directory,
mirrors an existing, long-standing convention (`direx.c`) for
"freshly created folder starts subscribed," and never touches
`status` or `time64` again after that first write — a user's later
manual Unsubscribe (or their read of the folder, which naturally ages
`time64` off `000000`) behaves exactly as it does for any other AMS
folder today.

## Files touched

None (investigation only, as directed by the gate).

## Open questions

- Not yet verified end-to-end against a scratch mirror root (the
  prompt's suggested `build/bin/imapsync -root /tmp/<scratch>`
  procedure) — Gate 1 was answerable from the existing real-data
  evidence (`~/.AMS.prof`, read-only) plus static tracing, so no
  experiment was run. Worth doing at Gate 2 to confirm
  `ensure_folder_dir`'s "already a message directory" check
  (`stat(.MS_MsgDir)`) really does distinguish first-creation from
  re-sync the way this proposal assumes, for every code path that
  reaches it (initial full sync vs. incremental).
- Did not check whether `imap-sync-tests` (`revival/tests/`) already
  exercises `ensure_folder_dir`'s creation branch in a way the Gate 2
  change would need to extend.

## Correction (2026-07-22, same day, live debugging session)

**The proposal above (imapsync auto-subscribe at creation time) does
not work, and was never going to.** wdc tested it live: manually set
INBOX to `AMS_SHOWALLSUBSCRIBED` (3) via Alter Subscription, then
`AMS_ASKSUBSCRIBED` (1) — both are supposed to bypass the HasNew
filter at `folders.c:473` unconditionally. Neither made INBOX appear
at startup. That falsified the premise that subscription-status
tuning could fix this at all.

Root-caused by instrumentation, not more static reading (static
reading of `folders.c`/`namechg.c` was, in isolation, correct but
answering the wrong question). Two temporary debug builds were used
and reverted after:
- `folders.c`'s existing `foldersDebugging` knob (normally `0`) set to
  `4` to print `SetupList`'s raw parsed map-file lines
  ([folders.c:70](../../src/atkams/messages/lib/folders.c#L70),
  `mdebug(4, ...)` calls already in the code at lines 421/447/451).
- `nosnap.c`'s `MSDebugging` (normally `0`, the global this binary
  actually links — `amsn.do` pulls `libcuin.a`, built from
  `ams/libs/nosnap/nosnap.c`, *not* `ams/ms/ms.c`'s copy of the same
  global) set to `-1` to enable every `debug(n, ...)` call in
  `ams/libs/ms`, including `namechg.c`'s per-entry trace.

The trace (`namechg.c`'s `MS_NameChangedMapFile`) showed INBOX's
subscription entry being read correctly from `~/.AMS.prof` (status,
pathelt, time64 all correct), entering the per-entry loop
(`"Processing subs entry .../INBOX status 1"`), and then producing
**zero further output** — no master-update-file attempt, no
`MS_GetNewMessageCount` call, nothing — before the parser moved on.
That is the signature of an early, silent `continue` with no adjacent
debug call. The only candidate that fits is
[namechg.c:129](../../src/ams/libs/ms/namechg.c#L129) (and again in
the output-writing loop at
[namechg.c:268](../../src/ams/libs/ms/namechg.c#L268)):

```c
if (MailOnly && SubsInPathOrder[i]->pathelt != mailpathelt) continue;
```

**`MailOnly` was `TRUE` the entire time**, despite `messages` being
launched with no `-m` flag. `folders__UpdateMsgs`
([foldaux.c:250](../../src/atkams/messages/lib/foldaux.c#L250)) does
`if (ams_OnlyMail(ams_GetAMS())) mailonly = 1;` — this **overrides**
the command-line flag with the site-wide config global
`AMS_OnlyMail`. That global defaults to `1`
([mailconf.c:367-371](../../src/overhead/mail/lib/mailconf.c#L367))
unless the tree is built with `RUN_AMDS_ENV`, which this revival's
`allsys.h:101` leaves commented out. Per its own doc comment
(`mailconf.h`, `setup.help:573-582`): *"used by IBM to inhibit the use
of bboards on the system."* With it `1`, **both** loops in
`MS_NameChangedMapFile` — the "did it change" loop and the final
output-writing loop — skip every subscription entry whose mspath root
isn't `$HOME/.MESSAGES`, unconditionally, before subscription status
(`ASKSUBSCRIBED`/`SHOWALLSUBSCRIBED`/anything) is ever consulted. This
is why `ListAll=TRUE` (folders.c:363, meant to force-write every
non-unsubscribed entry) never got a chance to matter for INBOX: the
entry never reached that check.

This also explains why `mail` was the only visible folder even though
its own map-file line showed `HasNew=0` (folder is empty,
`MessageCount=0` — genuinely nothing to show): `mail` isn't actually
passing the Expose-New filter at all. It displays via a *completely
separate*, hardcoded fallback in `folders__UpdateMsgs`
([foldaux.c:274-277](../../src/atkams/messages/lib/foldaux.c#L274)):
when the New-mail folder list comes up empty, it calls
`folders_ReadMail`, which disambiguates the literal string `"mail"`
(falling back to `"misc"`) and opens its captions view directly —
nothing generalizable, nothing to do with subscription or exposure.
The AssociatedTime/`time64="000000"` mechanism documented above is all
real and correct, but was never actually the operative mechanism for
why `mail` shows and INBOX doesn't; it was answering a question that
`AMS_OnlyMail` had already made moot.

**Actual fix applied, with wdc's approval:** `AMS_OnlyMail: No` added
to `build/etc/AndrewSetup` (a hand-authored, runtime-read config file,
not a build artifact — `GetConfiguration`'s search path ends at
`${ANDREWDIR}/etc/AndrewSetup`, which `site.h` points at the build
tree, and no earlier candidate in the search list exists on this
machine). No rebuild needed; this is read fresh at every process
start. `AMS_OnlyMail` is documented `f_cell` in `mailconf.c`'s config
table, i.e. genuinely meant to be site-configurable, not a compiled-in
constant. Since `AndrewSetup` lives inside `build/` it does not
survive `make Clean`; `revival/tools/write-andrewsetup` was added to
regenerate it, and both `quickstart.md` ("AndrewSetup settings") and
`mail-quickstart.md` (steps 3 and 6, limitations table) were updated
to document it and point at that tool.

With `AMS_OnlyMail: No`, the path-based exclusion is gone, but the
*second*, independent gate documented in the original Gate 1 findings
above is still real: a folder subscribed at plain
`AMS_ALWAYSSUBSCRIBED` only appears in Expose New when it has genuine
unread mail (`HasNew`, driven by `time64`/AssociatedTime); only
`AMS_ASKSUBSCRIBED`/`AMS_SHOWALLSUBSCRIBED` bypass that check
unconditionally. Both mechanisms are needed for the full picture; only
`AMS_OnlyMail` was the one actually blocking wdc's live test.

End-to-end confirmation (relaunch `messages`, verify INBOX visible by
default) was requested but not yet reported back as of this writing.
