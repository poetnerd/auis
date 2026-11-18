# revival/tests/html-synth

Synthetic HTML mail bodies for the HTML-mail renderers and their test
harnesses (`html-parse-tests`, `html-totext-tests`), and for viewing in
`htmlprv`. Each fixture isolates one structural property that real
mail has and the renderers must survive. All content is invented;
every name is fictional and every domain is an RFC 2606 example
domain, so the corpus can be published freely.

The files are written by `make-fixtures`, deterministically: edit the
generator, rerun it, and commit both. Do not edit the `.html` files by
hand.

These replace an earlier corpus of 16 real messages harvested from an
INBOX (`revival/tests/html-fixtures`, withdrawn 2026-11-17 because most
of it was private mail). Each fixture here reproduces the property its
original was chosen for, numbered to match. The population statistics
gathered from that harvest (body sizes, nesting depth, how common
`<font>`, `cid:` images and mismatched table tags are) are kept at the
end of this file.

Real messages that particular roadmap items were fixed against live in
`revival/tests` itself (`amazon.html`, `economist.html`,
`linkedin-*.html`, `LinkedIn`, `national-grid.html`, `bookrack.html`),
with the recipient's details and tracking links scrubbed.

| Fixture | Exercises | Notes |
|---|---|---|
| `s01-deep-table-nesting-28.html` | 28 levels of well-formed `border="0"` layout tables (the deepest seen in real mail, a shipment notice), remote images, a 1x1 tracking pixel | 4 KB |
| `s02-large-heavy-tables-font.html` | A large body full of tables at once: 370 listing tables, `<font>` beside `style=` | 209 KB |
| `s03-large-body-receipt.html` | A large body with modest structure: 21 tables (one nested 9 deep), one remote image, long boilerplate | 223 KB |
| `s04-minimal-body.html` | The minimal extreme: one short `<div>` | 59 bytes |
| `s05-cid-png-colspan.html` | 21 inline `cid:` images (PNG in the original) in a `colspan` grid | |
| `s06-cid-webp-font.html` | An inline WEBP `cid:` image (unsupported), `<font>`; a mailing-list footer that quotes `<a href=...>` as text | |
| `s07-table-extra-open.html` | Malformed: 10 `<table>` opens, 9 closes | `html-parse-tests` |
| `s08-table-extra-close-font.html` | Malformed the other way: 35 opens, 36 closes, plus `<font>` | `html-parse-tests` |
| `s09-digest-font-colspan.html` | A social-digest template: `<font>`, `colspan`, nesting 9 deep | |
| `s10-no-doctype-border0-alt.html` | No `<!DOCTYPE>`, `border="0"` layout tables, an image whose alt text must survive as a placeholder | `html-totext-tests` |
| `s11-cid-supported-formats.html` | Inline `cid:` images in supported formats (GIF, JPEG), the contrast case for s05/s06/s15 | |
| `s12-colspan-tracking-pixels.html` | `colspan`, two 1x1 tracking-pixel images | |
| `s13-mixed-cid-remote-colspan.html` | A typical mid-sized message: `colspan`, one `cid:` and one remote image, nesting 4 deep | |
| `s14-plain-prose-links.html` | Plain-text-shaped HTML: no tables, `style=` or `<font>`; a link and a `mailto:` | `html-totext-tests` |
| `s15-svg-and-webp.html` | SVG and WEBP in one message, both unsupported | |
| `s16-deep-nesting-23-divs.html` | 23 levels alternating `<div>` and `<table>`, a second deep case unlike s01 | |

The `cid:` fixtures are bare HTML bodies with no MIME envelope, as the
originals were, so their images are unresolved by design: they test
the placeholder path. `revival/tests/cid-image-test.eml` and
`mime-display-tests` cover resolution against real MIME parts.

## Findings from the original harvest (2026-08-14)

Computed over all 3223 messages with a `text/html` part, not just the
16 fixtures selected then — the fixtures are examples, these numbers are
the actual population stats.

**Size caps:** body sizes range from 2 bytes to 240 KB. Median 11.6 KB,
90th percentile 70 KB, 99th percentile 125 KB, max 240 KB. The design
doc's placeholder guess of "2MB/document" is generous by roughly an
order of magnitude against real observed traffic — a cap around
**256–512 KB** would already sit above the 99th percentile with
headroom, and would still reject only the most pathological outliers.
Recommend lowering the placeholder rather than keeping 2MB.

**Table nesting depth:** median depth 0 (most mail has no tables, or
uses them shallowly), 90th percentile 9, 99th percentile 19, max
**28** (the original of s01; corroborated by the original of s16 at 23 from an unrelated
sender, so this isn't one template's quirk). The design doc's guess of
"3, revisit" is far too low for real mail — Amazon's shipment-tracking
template alone would blow through a depth-3 cap on every single such
email, and shipment notifications are common, everyday mail, not an
edge case. Recommend either raising the cap substantially (~20-25,
covering the 99th percentile) or reconsidering whether a hard
*rejection* is the right response to exceeding it at all, versus
just flattening the excess depth gracefully (which the design doc
already allows for as the degrade behavior — the question is really
just where the flattening threshold kicks in, and it should be well
above 3).

**PNG support:** 5.4% of HTML-bearing messages (174/3223) include a
PNG image part. Not rare — worth treating as a real gap rather than a
theoretical one, consistent with the design doc's existing lean toward
adding PNG support. Two other unsupported formats turned up that
weren't in the design doc's radar at all: **SVG** and **WEBP**
(the originals of s15 and s06) — worth adding to the
same "should `image.c` support this" discussion, WEBP especially,
since it's increasingly common as a default export format from mail
composers.

**Malformed table tags:** 99/3223 messages (3%) have a `<table>`/
`</table>` count mismatch — a real, non-negligible fraction. Fixtures
s07 and s08 cover both imbalance directions (one extra open, one extra
close). This confirms graceful-degradation-on-malformed-markup isn't
a defensive nicety, it's needed for ~1 in 33 real HTML messages.

**`style=` and `<font>` prevalence:** `style=` attributes appear in
89.4% of HTML mail — overwhelmingly the common case, confirming
inline-style parsing is core work, not a nice-to-have. `<font>` tags
still appear in 14.5% despite being long-deprecated — worth keeping
in the allowlist as the design doc already does, this confirms real
mail still emits it.

**Remote vs. inline images:** 49.2% of HTML mail references at least
one remote (`http`/`https`) image, 11.8% has at least one `cid:`
inline image. Tracking-pixel-shaped images (literal `width="1"
height="1"`, or WordPress-style `pixel.wp.com`/`b.gif`/`t.gif` beacon
URLs) were trivially easy to find (the originals of s01 and s12 both have them
unprompted, from unrelated senders) — the design doc's default-block
posture for remote images is well justified by real traffic, not a
hypothetical concern.
