# fdplumb / preference-blackout — Gate 1 report (Fable session, 2026-07-19)

> **STATUS:** all three staged fixes described below were reviewed,
> committed, and runtime-verified via dependInstall on 2026-07-19;
> `fdplumb-session.diff` is gone (superseded by the fossil history).
> The "Untouched hazards" list at the end remains live and is
> documented nowhere else.

Task: `revival/doc/fdplumb-prompt.md`. This was a static-analysis pass
(no runtime repro attempted — the symptom is known non-reproducible on
demand and budget was constrained). Gate 1's mechanism map is complete
and several of its questions are answered definitively from source.
Three small fixes are staged in `fdplumb-session.diff` (NOT committed);
all three compile clean via `make profile.o fdplumb.o setprof.o` in
`src/overhead/util/lib`. **libutil.a and dependents have NOT been
relinked** — a normal `make dependInstall` will pick the fresh .o files
up.

## Mechanism map (Gate 1 item 1) — definitive answers

* **The wrappers never act on an fd.** `RegisterOpenFile`/
  `RegisterCloseFile` (fdplumb.c) only maintain a `char *[getdtablesize()]`
  name table and print. No wrapper closes, reuses, or modifies any
  descriptor on a mismatch. The Gate-3 candidate "demote fdplumb to
  detect-only" is moot — it already is detect-only.
* **`dbg_fopen` cannot fail where fopen would succeed.** It calls real
  fopen first and only then registers. Registration failure paths only
  print (one exception: `RegisterOpenFile` doesn't check the malloc for
  the name copy before strcpy — a crash-under-OOM hazard, not a blackout
  mechanism; left unfixed, noted here).
* **The trailing "2" in the critical message is NOT stderr.** The
  message format is `(%d, %s, %d)` = (fd, newly-opened path, opencode).
  Opencode 2 = `FDLEAK_OPENCODE_FOPEN` (fdplumbi.h:33). So
  `(5, /tmp/te..., 2)` reads: "fopen just returned fd 5 for /tmp/te*,
  but the ledger thought fd 5 still held /tmp/de*". No stderr
  involvement; the prompt's premise on this point should be corrected.
* **Why stale entries happen at all: per-TU instrumentation.** The
  wrappers exist only in translation units that include fdplumb.h. Any
  registered open that is later closed by code in a TU *without*
  fdplumb.h (most of the tree, all libc/Xft/fontconfig internals)
  leaves a stale ledger entry; the next wrapped open that lands on that
  fd prints the critical. This makes "File descriptor replaced!"
  **structurally prone to false positives** and NOT trustworthy
  evidence of a real double-close. The observed /tmp/de* → /tmp/te*
  sequence is exactly a draft file opened via a wrapped fopen
  (submsg.c includes fdplumb.h), closed somewhere unwrapped, then a
  temp file reusing fd 5.
* **Real ledger bug found: `dbg_dup2` tests `res == 0`.** dup2 returns
  the *new fd* on success, so every successful `dup2(old, new)` with
  new != 0 was recorded as a close of `new` but never as an open —
  silently corrupting the ledger (and any dup2 target looks
  permanently closed). Fixed in the staged diff (`res >= 0`).

## The preference blackout — fdplumb is (almost certainly) exonerated

* **The blackout fopen is not dbg_fopen.** profile.c itself never calls
  fopen. The actual open happens in `ReadConfigureFile`
  (`config.c:310`) — and **config.c does not include fdplumb.h**, so
  that fopen is raw libc. The prompt's premise "that fopen is fdplumb's
  dbg_fopen" is wrong. fdplumb cannot have perturbed the preferences
  load.
* **Confirmed poison mechanism** (as suspected in the roadmap):
  `openprofile()` returning NULL latches `inited = 1` in each of four
  entry points; every later `getprofile` returns NULL for the life of
  the process.
* **`ReadConfigureFile` returns NULL for three indistinguishable
  reasons:** fopen failure (any errno), malloc failure mid-parse (also
  leaks the partial list), and — noteworthy — **a successfully-opened
  file containing zero parseable entries**. An empty-at-read-time
  preferences file would blackout exactly like a failed open.
* **Rewrite race ruled out for the standard writer.** `setprofilestring`
  (setprof.c) writes `preferences.NEW` and `rename()`s it over the
  original — atomic; no window where the file is absent or empty. (If
  anything else rewrites ~/preferences non-atomically, the empty-file
  case above becomes live again; prefed should be checked if it has its
  own save path.)
* **Additional dormant bug found in openprofile's `~` expansion:**
  `if(*name=='~' && (name++,namelen--,homelen>0))` mutates
  name/namelen *before* testing homelen, so if `gethome(NULL)` ever
  returned NULL the fallback would try to open `/preferences` (leading
  path component silently dropped) — every candidate fails, permanent
  blackout. `gethome` on macOS effectively always succeeds ($HOME set),
  so this is dormant here; also note `gethome`'s 100-byte static
  buffer would silently truncate very long home paths. Left unfixed,
  logged.
* **Second latent crash found in setprof.c:** if the initial
  `fopen(profileFileName, osi_F_READLOCK)` fails with errno != ENOENT
  (e.g. EMFILE), `oldR` stays NULL and control reaches
  `osi_ExclusiveLockNoBlock(fileno(oldR))` → NULL dereference. Guard
  added in the staged diff.

## Ranked hypotheses for the 2026-07-18 blackout run

1. **Transient fopen failure, most plausibly EMFILE** (fd table full at
   the moment of first `getprofile`). Fits: intermittent, self-healing
   across runs, both cui and messages affected, and fdplumb's whole
   reason for existing is that this codebase historically leaked fds.
   The staged profile.c fix makes this class self-healing AND visible
   (one stderr warning per failed attempt, with errno).
2. **Environment anomaly in that one run** — `PROFILES` set/empty/wrong
   in the launching environment (test harnesses set fixture
   environments; a leaked env would blackout deterministically for that
   process while the file "parses fine standalone"). Not directly
   checkable after the fact; the new warning line won't fire for this
   case (a wrong-but-readable PROFILES loads successfully), but
   `GetProfileFileName()` would name the wrong file — cheap to log if
   it recurs.
3. **malloc failure mid-parse** (ReadConfigureFile NULL + list leak).
   Now covered by the retry fix (ENOMEM != ENOENT → not latched).

## Staged fixes (fdplumb-session.diff — all compile clean, none committed)

1. **profile.c** — don't latch a failed load when any candidate file
   failed for a reason other than ENOENT; print one
   `<warning:profile>` line with the errno and retry on the next
   access. Genuinely-missing files still latch (no retry storm for
   preference-less users). The four duplicated init blocks are folded
   into one `initprofiles()` helper; the retry path frees a
   half-loaded global list to avoid a leak. This is the Gate-3
   candidate the prompt already called "small, arguably correct
   regardless".
2. **fdplumb.c** — `dbg_dup2` success test `res == 0` → `res >= 0`.
3. **setprof.c** — NULL-oldR guard before `fileno(oldR)`.

## Suggested next steps (not done)

* Review + commit the three fixes; they ride into binaries on the next
  dependInstall.
* Keep the Sonnet fdplumb task open only for the runtime half: with the
  warning line in place, the next blackout occurrence self-identifies
  (errno on stderr) — that single line of output will decide between
  hypotheses 1 and 2.
* The "File descriptor replaced!" criticals can be de-noised any time
  by accepting they are false positives from partial instrumentation;
  a real double-close hunt should not start from them.
* Untouched hazards logged above: RegisterOpenFile unchecked malloc,
  openprofile `~`-expansion comma bug, gethome 100-byte buffer,
  prefed's save path unverified.
