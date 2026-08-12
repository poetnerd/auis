# Updating revival.md — prompt for later execution

Paste this whole file as the instructions for a fresh session when it's time
to refresh `revival/doc/revival.md` against newer work. It's written to be
self-contained — the executing session has no memory of this one.

## What revival.md is, and isn't

`revival.md` is a **narrative distillation**, not a mirror of the detailed
docs. Its audience is two segments: readers who already know ATK (CMU/Andrew
alumni, toolkit-history enthusiasts) and readers who have never heard of it
and are being invited to understand what was technically distinctive about
it. Neither segment wants breezy or folksy prose — write it as a measured
technical retrospective, not a blog post. Concretely: no rhetorical
asides ("here's the fun part"), no false suspense/reveal structure, no
exclamation-adjacent enthusiasm. State what happened and why it mattered,
directly. It should still be readable in about ten minutes and should not
turn into a changelog, reference manual, or exhaustive bug list. It
deliberately omits file:line references, exact grep commands, commit
hashes, and blow-by-blow debugging narration; all of that stays in the
detailed docs it summarizes:

- `porting-changelog.md` — chronological fix history
- `porting-assessment.md` — bug-class analysis, strategy, the ANSI-C
  conversion plan (§14 especially)
- `roadmap.md` — current status, open items, milestone list
- `claude-history/m1-rollout-runbook.md` / `claude-history/m1-point10-batches.md` — the ANSI-C rollout
  procedure and batch-by-batch findings
- `quickstart.md`, `version-comparison.md`, `runtime-debugging-guide.md`
- Any standalone investigation write-ups (e.g. `claude-history/zip-black-render-investigation.md`, `claude-history/calc-text-rendering-investigation.md`)

`revival.md` has eight fixed sections, in this order, and should keep this
shape rather than growing new top-level structure casually:

1. **What AUIS is** — what AUIS/ATK are, why 6.3.1, target platform, and a
   paragraph aimed at readers unfamiliar with ATK explaining what was
   technically distinctive about it (the recursive inset/compound-document
   model, the "Class" object system, the shared application framework) —
   this is the section that does the inviting-newcomers-in work, so don't
   cut it for brevity. Otherwise essentially static; touch the factual parts
   only if the basic framing changes.
2. **Modernizing** — the deliberate tool/infrastructure swaps made to adapt
   AUIS to a 2026 environment, plus every bug surfaced *by* one of those
   swaps, whether the buggy code is new or is original 1990s code that made
   assumptions about a specific tool version. Comes right after "What AUIS
   is." Currently three subsections; add a new one, following the same
   pattern, for a future swap that grows its own bug stories rather than
   folding it into an existing subsection:
   - **Required to run at all** — a change without which the software does
     not build or does not start (the fortified-libc/overlapping-strcpy
     fix, `dlopen()`/`dlsym()` replacing the old loader, the
     compiler-leniency strategy). If removing the change would mean "this
     no longer works on a current machine," it goes here, as a bullet.
   - **Bison and flex** — the parser-generator tool swap (vendored bison
     hangs on current hardware; flex's *generated code* and hand-written
     wrapper code assumed 1990s flex/bison behavior). Narrative form: what
     was swapped and why, then the specific bugs the swap surfaced as a
     short list (currently: the mkparser fixed-width table bug, the flex
     `yy_init` polarity reversal). These bugs live in original 1990s code,
     not new code — they belong here anyway, because their story is
     inseparable from explaining the tool-version swap that surfaced them.
   - **Adopted by choice: anti-aliased text** — a change made to improve on
     something that still technically functioned (Xft replacing server-side
     bitmap fonts), including the client-side vs. server-side X fonting
     background and any bugs the *new* Xft code introduced (the calculator
     inset's rendering defects). If a newer optional-modernization effort
     happens (e.g. a future menu Xft migration), fold its story in here or
     add a sibling subsection, following the same pattern: what the old
     approach was, why it still worked, why the change was made anyway, and
     what it cost to get right.
3. **Old bugs never found till now** — a curated set of *confirmed, closed*
   old bugs (not this year's new mistakes, not new code written during the
   revival, not anything still under investigation — see "Open issues"
   below for that) that modernization work exposed, that are NOT primarily
   word-size/sign-extension defects (those belong in "Word size issues" —
   see the classification note below). Each entry is 2-5 sentences: what
   broke, why it had never surfaced before, what the mechanism was, and
   that it's fixed — stated plainly, not as a story with a twist.
4. **Word size issues** — the ILP32→LP64 conceptual explanation, the
   bug-variant taxonomy, AND any concrete bug whose root cause is a
   width/sign-extension/data-corruption mismatch (scanf `%d` into a 64-bit
   variable, a corrupted `-1` sentinel, a signed/unsigned declaration
   mismatch, a pointer truncated into a narrower field, etc.), even if the
   *symptom* was a rendering glitch or crash in some inset. Classify by
   mechanism, not by which application showed the symptom. Update the
   taxonomy itself only when a genuinely new variant or mechanism appears
   (not for every new instance of an existing variant), but do add new
   concrete examples here as they're found.
5. **Migrating to ANSI C** — the M1–M4 plan and current progress.
6. **Where things stand today** — a short, current-as-of-dated status
   paragraph on what runs. This is also where deliberate *scope*
   decisions belong (a subsystem intentionally left out of the build, and
   why) — a scope decision is not a bug, old or new, and doesn't belong in
   any bug section even when it produces a bug-shaped symptom elsewhere
   (e.g. a font that's silently missing because the subsystem that builds
   it was left out on purpose).
7. **Open issues** — problems that are still unresolved: either the root
   cause isn't confirmed (a live deadlock nobody has traced to a specific
   call site) or a workaround is in active use without a real upstream fix
   (a manual build step whose necessity isn't fully settled). The tell that
   something belongs here rather than in "Old bugs": if the entry would
   need to say "probably," "the leading explanation," or "hasn't been
   determined," it's an open issue, not a closed one. Move an entry from
   here to "Old bugs" (or "Word size issues"/"Modernizing," per the usual
   rules) once it's actually root-caused and fixed.
8. **Further reading** — links to the detailed docs. Add a new link only if
   a genuinely new detailed doc appears in `revival/doc/`.

## Procedure

1. Read `revival.md`'s "Last distilled: DATE" line at the top.
2. Read `roadmap.md` in full (it's the living status doc and links out to
   the rest) and skim `porting-changelog.md`'s chronological log for
   entries **dated after** the "Last distilled" date. Also check for any
   new standalone investigation docs in `revival/doc/` that didn't exist
   last time.
3. For each new item found, ask: **does this illuminate something real about
   either the codebase's history or the mechanics of the port?** Most
   day-to-day fixes (a missing `#include`, a routine `.ch` typing pass with
   no new mechanism) do not clear this bar and should not be added. For
   anything that does, classify it before drafting — in this order, since
   each check pre-empts the ones after it:
   1. **Is this a deliberate scope decision** (a subsystem intentionally
      left out of the build) rather than a defect at all, even if it
      produces a bug-shaped symptom elsewhere? → a brief note in "Where
      things stand today," never a bug section, regardless of how
      interesting the downstream symptom is.
   2. **Is the root cause still unconfirmed, or is a workaround in place
      without a real fix?** → "Open issues," not "Old bugs" — "Old bugs"
      is for closed cases only. Re-classify into "Old bugs,"
      "Word size issues," or "Modernizing" once it's actually root-caused
      and fixed.
   3. **Did this bug surface specifically because of a deliberate
      tool/infrastructure swap documented under "Modernizing"** — a
      parser-generator version swap, a new rendering path, a hardened
      system library, a replaced loader — rather than through general
      interface type-checking (the ANSI C conversion) or general exercise
      of old, dormant code? → "Modernizing," in whichever subsection
      matches the swap that surfaced it (or a new subsection, if it's a
      new kind of swap). This test does NOT require the buggy code itself
      to be new: the bison/mkparser and flex bugs both live in original
      1990s code, but belong under "Modernizing" anyway, because their
      story is inseparable from explaining the bison/flex tool swap that
      surfaced them. What matters is *why the bug was found*, not *how old
      the buggy code is*.
   4. Otherwise, **is the root cause a word-size, sign-extension, or
      similar data-corruption mismatch** (`int`/`long` confusion, a
      corrupted `-1` sentinel, a signed/unsigned declaration mismatch, a
      pointer truncated into a narrower field)? → "Word size issues,"
      regardless of which application showed the symptom. This is the
      check most likely to be missed, since the *symptom* (a rendering
      glitch, a crash, a scroll position) often reads like a generic
      old-bug story when the *cause* is squarely a width/sign bug.
   5. Otherwise, if it's a genuinely old, *closed* defect (interface
      transposition, library-version-skew, libc-strictness break, etc.) →
      "Old bugs never found till now."
   6. A newly-adopted modern replacement for an old subsystem, with no
      interesting bug story attached → still goes under "Modernizing,"
      just as a bullet with no accompanying defect narrative.
   7. Progress on M1–M4 (a stage completing, a stage starting) → update
      "Migrating to ANSI C" and the status paragraph.
4. **Prefer replacing over appending.** If a newer bug is a clearer or more
   illustrative example of a mechanism already in the document, swap it in
   and drop the older one rather than listing both — the document should
   stay roughly its current length, not grow monotonically. Use judgment:
   the goal is the best ~10-13 stories, not the most.
5. Update "Where things stand today" to reflect current reality (what runs,
   what's still open) and bump the "Last distilled" date at the top.
6. Write in the same voice as the existing document: measured, declarative
   technical prose — present tense for current state, past tense for what
   happened — not chatty or blog-style. No rhetorical asides, no
   reveal/twist framing, no exclamation-adjacent enthusiasm. Explain jargon
   briefly on first use rather than assuming it (a reader new to ATK
   doesn't know what LP64, `.ch` files, or classpp are), but don't
   over-explain to the point of condescension for readers who already know
   the codebase. No tables, no code blocks except the rare short
   illustrative snippet, no bullet list deeper than one level.
7. Re-read the whole document once after editing for tone consistency and
   length (it should stay in the same ballpark — a 10-minute read, not a
   reference manual).

## Scope and constraints

- Edit only `revival/doc/revival.md`. Never edit the detailed source docs
  as part of this task.
- Do not add new top-level sections without a clear reason — if something
  doesn't fit the existing five sections, it probably belongs in the
  detailed docs instead, or the fit needs more thought before inventing a
  new section.
- If asked to commit, follow this project's Fossil convention: one terse
  line, no multi-paragraph message, `fossil status` before and after.
- End by summarizing in plain terms what changed in this pass (what was
  added, what was replaced, what the new status paragraph says) — keep this
  summary short, it's for the user, not for the document itself.
