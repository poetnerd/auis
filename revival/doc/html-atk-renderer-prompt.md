# Task: ATK-styled HTML renderer (html-atk-renderer)

Read `revival/doc/sonnet-playbook.md` first; its rules apply. This is
Stage 3 of the HTML-mail-rendering project — read
`revival/doc/html-mail-rendering-design.md` in full, and skim the
`htmlpart.h`/`htmltext.h` header comments (Stages 1/2, already
committed on this branch) before doing anything else. You're building
directly on top of both; this task does not touch either one.

**Delegation-model note, since this differs from older prompts you
may see referenced elsewhere in this doc tree:** you are one
continuous background session, not an interactively-supervised one —
there is no human watching and unblocking you gate-by-gate in real
time. Treat each "Gate" below as an internal phase with its own
verification (compile clean, tests green) before moving to the next,
not a literal stop-and-wait. Work all the way through every gate you
can in this one session and give one comprehensive final report
covering all of them, the same way the Stage 1 (`htmlpart.c`) and
Stage 2 (`htmltext.c`) sessions on this branch did. If you get
genuinely stuck or find something that contradicts the design doc
badly enough to need a real decision, stop early and report what you
have rather than guessing past it.

## Scope

Stage 1 (`src/ams/libs/shr/htmlpart.c`) parses+sanitizes `text/html`
MIME bodies into a `struct htmlnode *` tree. Stage 2
(`src/ams/libs/shr/htmltext.c`) walks that tree into flat plain text.
This task walks the *same* tree into real ATK objects: styled text
runs, embedded `table` insets, embedded `image` insets, and clickable
links — per the design doc's "Table strategy", "Images", and "Links:
click-to-launch" sections, which you should treat as the spec. Do not
touch `text822.c`, `cui.c`, or wire this into any live rendering path
until Gate 5 — everything through Gate 4 is a standalone module,
buildable and testable on its own, exactly like Stages 1 and 2 were.

This is ATK code, not ATK-independent — it links against ATK's class
system (`class.h`, `text`/`environment`/`style` objects), unlike
Stages 1/2 which deliberately didn't. Expect to need `classpp` if you
add any new `.ch` class definitions (you likely won't need a new
class at all — see Gate 1 — but if you do, mirror an existing small
inset class rather than inventing conventions).

**Build note:** this checkout already has its own isolated `build/`
tree (fixed 2026-08-15, see `revival/doc/sonnet-playbook.md`'s "2a"
step and `src/config/site.h`) — you don't need to redo any of that
bootstrap work, just build normally.

## Gate 1 (research — no code): map the exact ATK mechanics

You are extending unfamiliar territory (real ATK object construction,
not just C string/tree manipulation like Stages 1/2), so do this
research pass with file:line evidence before writing anything, the
same way `mime-display-prompt.md`'s Gate 1 did for the original MIME
work:

1. **Styled text insertion.** `src/atkams/messages/lib/text822.c`
   already inserts text with varying styles (header highlighting —
   search for `HighlightThisOne`) into the same kind of ATK
   text/environment object this task needs to write into. Find the
   exact function(s) it uses (likely something in the
   `..._AlwaysInsertCharacters` family — `fnote_AlwaysInsertCharacters`
   is used nearby for footnote collapsing, per this project's own
   history) and document the exact call shape: how a style gets
   selected/created and attached to a run of inserted text.
2. **Tag-to-stylesheet mapping.** `src/contrib/srctext/html/html.c`
   already does exactly this for the old `htmlview` parser —
   `style_AddAttribute`, `style_GetAttribute`, and a per-tag
   `styleTypes`-shaped table (`entityBold`, `entityItalic`, etc. —
   search for the `entity*` table, roughly line 184 onward as of this
   writing) are real, working precedent. Document how a style object
   actually gets created/looked up and reused (you don't want to
   allocate a fresh style per run).
3. **Embedding a dataobject inset inside message text.** This
   codebase already embeds at least one kind of inset (an `image`) in
   rendered mail — there's a known, already-logged bug about it
   (`revival/doc/roadmap.md`'s "Image Inset Blank Import" note: "GIF/
   JPEG-into-image-inset renders blank; confirmed pre-existing"),
   which means the embedding *mechanism* itself exists and mostly
   works even though a specific format currently renders blank. Find
   this code (likely `text822.c` or `mailobj.c` in the same
   directory) and document the exact mechanism: how a child
   dataobject gets created, attached to the parent text/environment
   object, and made to appear inline. This is your primary precedent
   for both table and image embedding in Gates 3/4 — closer to what
   you need than anything in `table.ch`/`image.ch` alone, since those
   just define the objects, not how something else embeds one.
4. **Table object mechanics**, to complement what's already in the
   design doc: `src/atk/table/table.ch` (the dataobject) and
   `src/atk/table/spread.ch` (its view) are already read and
   summarized in `html-mail-rendering-design.md`'s "Table strategy"
   section — confirm the `Imbed()` method's exact signature and the
   `JOINED`/`GHOST`/`SUPPRESSED` boundary-color macros
   (`IsJoinedAbove`/`IsJoinedToLeft`/`SetInterior`/`SetBoundary`) by
   reading `table.c`'s implementations, not just the header.
5. **Image object mechanics**, similarly: confirm
   `image__ReadOtherFormat`'s exact call contract by reading
   `src/atk/basics/common/image.c` around line 2314 (already cited in
   the design doc) — what exactly does the caller need to provide,
   and what does a successful vs. failed call leave behind.
6. **Link click-handling.** `contrib/srctext/html/htmlview.ch`/
   `htmlview.c`'s `htmlview__Hit` already detects a click landing on
   an anchor and echoes the URL to the message line — find it and
   document exactly how it identifies "this click landed on anchor
   X" (this is the mechanism Gate 4 needs to complete into an actual
   `popen("open ...")` launch, applied to your own rendered output
   instead of `htmlview`'s).

**Deliverable:** a short written map (file:line citations, not just
"I looked at it") covering all six points above, plus a one-paragraph
plan for how Gates 2–4 will use each mechanism. If any of the six
turns out not to exist or not to work the way the design doc assumes,
say so clearly here rather than discovering it silently later.

## Gate 2: core styled-text renderer (no tables/images/links yet)

A new module (mirror Stage 1/2's placement:
`src/ams/libs/shr/htmlatk.c` + `src/ams/libs/hdrs/htmlatk.h`, or
adjust if Gate 1's findings suggest ATK-linked code belongs elsewhere
in this tree — your call, justify it) that walks a `struct htmlnode
*` tree and inserts real ATK styled text into a caller-supplied
text/environment object: paragraph/block breaks (same block-vs-inline
classification Stage 2 already worked out — reuse that judgment,
don't re-derive it from scratch), bold/italic/underline from `b`/
`i`/`u`/`strong`/`em`, color/weight/decoration from the `style`
allowlist (`htmlpart_GetStyleProp`) and `font` tag, using the
tag→stylesheet mechanism from Gate 1.

Tables and images: for this gate only, borrow Stage 2's logic outright
(call into `htmltext.c`'s per-subtree rendering, or replicate its
placeholder behavior) rather than leaving them unhandled — a
`<table>` becomes the same degraded sequential text Stage 2 produces,
an `<img>` becomes the same `[image: alt]`-style placeholder, both
inserted as plain (unstyled or minimally-styled) text runs. This
proves out the real ATK insertion mechanics — the newest, riskiest
part — before Gates 3/4 add embedded objects on top.

Links: apply a distinct style (e.g. underlined) to the anchor text so
it's visually a link, but no click behavior yet — that's Gate 4.

Test: a standalone test harness needs an actual ATK text/environment
object to insert into, which means this can't be a pure offline C
test the way Stages 1/2 were. Figure out the smallest way to
instantiate one for testing (look at how existing standalone `.test`
drivers elsewhere in this tree that touch ATK objects do it, if any
do — `mimeparttest.c`/`htmlparttest.c` didn't need to, since they're
ATK-independent, so this is genuinely new ground; report what you
find). At minimum, verify against the real fixture corpus
(`revival/tests/html-fixtures/`) that nothing crashes and that a
dumped representation of the resulting styled text (however you
choose to inspect/serialize it for a test) shows the expected style
runs for a few known fixtures — you decide the exact test shape and
document your reasoning, same as Gate 1's report should.

## Gate 3: table embedding

Replace Gate 2's table placeholder with a real `table`/`spread`
embed, per the design doc's "Table strategy" section and Gate 1's
findings: `<table>` → `table_New` + `table_ChangeSize`; `<td>`/`<th>`
→ `table_TextCell` or `table_ImbeddedObject`; `colspan`/`rowspan` →
`JOINED` boundary colors across the spanned chunk; `border="0"` →
`GHOST`/`SUPPRESSED` rather than `BLACK`. No depth cap (per the design
doc's "No fixed size or depth caps" section) — test specifically
against fixture 01 (28 levels of real nesting) and fixture 16 (23
levels, different sender) to prove this, not just a shallow synthetic
case. Also test against fixtures 07/08 (malformed `<table>`/`</table>`
tag-count mismatches) to confirm degrade-not-crash still holds once
real objects are involved, not just text.

## Gate 4: image embedding + clickable links

Images: `<img>` → attempt real embedding via the `image` object +
`image__ReadOtherFormat`'s capability check (per Gate 1's findings on
the existing embedding mechanism). On an unsupported format or
unfetched remote content, fall back to the same text placeholder Gate
2 used — same idea as `PrintAttachmentLine` in `cui.c` already does
for non-renderable MIME parts at the whole-message granularity, just
per-image here. Test against fixtures 05/06/11/15 (PNG/WEBP/SVG —
all currently unsupported formats, should placeholder cleanly) and
whichever real fixtures carry `cid:` images in already-supported
formats (check `revival/tests/html-fixtures/README.md`'s table).

Links: complete `htmlview__Hit`'s pattern into an actual action — a
`Hit()` view-method override on the containing view that detects a
click inside a link-styled run (Gate 2 already applies the style) and
`popen("open <url>", "r")`s it. The href scheme is already restricted
to `http`/`https`/`mailto` by Stage 1's sanitization, so nothing
unexpected should ever reach `popen` — don't add a second scheme
check here, but do add a test confirming a stripped/absent href
(Stage 1 already handles this — the `<a>` node just has no `href`
attribute) doesn't crash the `Hit()` handler.

## Gate 5: wire into text822.c, retire the old shim

Only after Gates 1–4 are solid: make this the primary HTML rendering
path in `src/atkams/messages/lib/text822.c`, replacing the current
`mimepart_HtmlToText`-based shim. Implement the design doc's
renderer-level fallback contract: if this renderer hits a condition
it can't recover from (not "unknown tag" — Stage 1 already handles
that — but a real construction failure, or parsing taking too long),
the **whole message** falls back to Stage 2's plain-text renderer,
never a half-rendered document. `cui` keeps using Stage 2 directly
and is unaffected by this gate.

Regression-test: `messages`' existing behavior on non-HTML mail
(plain text, ATK-native datastreams) must be byte-for-byte unchanged
— this new path only engages where the old shim did. Re-run every
existing suite this touches (`revival/tests/mime-display-tests` at
minimum) and confirm still green. You do not have interactive X11
access and should not attempt to launch `messages` yourself — give
wdc a short by-hand acceptance note (which real fixture-corpus-shaped
messages to expect the richest results from) instead, same as prior
delegated tasks in this tree have done when live-UI verification was
needed.

## Ground rules

**No fossil commits, no `fossil add`.** Leave everything uncommitted
in the checkout for direct review — same pattern as Stages 1/2 on
this branch (not the older `*-session.diff`/`*-REPORT.md`-in-tree-root
handoff you may see referenced in other prompts in this directory;
that convention doesn't apply here, this branch's actual practice is
leave-in-place-and-review). Compile-verify every touched/new file (no
new warning classes vs. a clean baseline). No concurrent builds. Use
`install -m 755`, never `cp`, if you ever need to replace an installed
binary (you likely won't for this task). Never touch
`~/.IMAP`/`~/preferences`/`~/.netrc`; the real fixture corpus in
`revival/tests/html-fixtures/` is your only real-world test input,
already harvested and read-only — don't touch it, add new synthetic
fixtures alongside it if you need coverage it doesn't provide (same
as Stages 1/2 did). Don't modify `htmlpart.c`/`htmlpart.h`/
`htmltext.c`/`htmltext.h` or the design doc itself — if Gate 1 or
later finds an actual defect in either (not just an API you wish were
shaped differently), stop and report it clearly rather than silently
patching around it, same rule Stage 2 worked under for Stage 1.

## Final report

Covering every gate reached: what you built (file list), the Gate 1
API map, test suite structure + pass/fail status per gate, the
by-hand acceptance note for Gate 5 if you got that far, and any
judgment calls or design-doc ambiguities — log those inline in the
new module's header comment (the way `htmlpart.h`/`htmltext.h` did;
wdc specifically reviews and likes judgment calls living in the code,
not a separate file) and summarize them in your final report too.
