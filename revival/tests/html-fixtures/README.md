# revival/tests/html-fixtures

A corpus of real-world `text/html` MIME bodies, harvested from the
mirrored INBOX (`~/.IMAP/fastmail/.MESSAGES/INBOX/`, ~3970 messages)
to inform and later test the HTML-mail-rendering project (see
`revival/doc/html-mail-rendering-design.md` and `roadmap.md`'s "HTML
mail rendering" project). Unlike `mime-display-tests` (synthetic,
offline-only fixtures for the MIME *parser*), this corpus is real mail
bodies, meant to stand in for the messy input the *HTML* parser and
renderers will actually see.

These are decoded HTML bodies only (quoted-printable/base64 already
undone), not raw RFC822 files — each carries an HTML comment header
noting which mirrored-mailbox file it came from, its original Subject
and Date, for traceability back to the source if needed. No fixture
here contains private correspondence: selection deliberately favored
commercial mail (shipping notifications, receipts, newsletters,
automated digests) and public mailing-list traffic (xbbn, senior_house,
BGMC — alumni/hobby lists, not 1:1 personal mail) over anything
personal in content. Two fixtures (08, 16) contain a phone number;
both are the sender's own published business/support line embedded in
a commercial template, not a private number.

Scanned 3973 files; 3223 (81%) contained at least one `text/html`
MIME part. 16 were selected for the corpus below, chosen to cover the
specific open questions in the design doc plus the structural gaps
`htmlview` is already known to choke on. Total corpus size: ~836 KB.

## Fixtures

| # | File | Exercises | Stats |
|---|------|-----------|-------|
| 01 | `01-amazon-shipment-deep-nested-tables.html` | Deepest observed table nesting (28 levels), well-formed (158 open/158 close), `border="0"` layout tables, remote images including a literal 1x1 tracking pixel | 127 KB, 158 tables, 15 remote imgs |
| 02 | `02-boston-art-review-large-font-heavy-tables.html` | Largest fixture with heavy table use simultaneously — stresses size cap *and* table strategy together; `<font>` tags alongside `style=` | 213 KB, 226 tables, depth 12, font+style both present |
| 03 | `03-morningstar-receipt-large-body.html` | Large body (220 KB), only 1 remote image — tests size handling independent of table/image complexity | 220 KB, 21 tables, depth 9 |
| 04 | `04-waltham-pest-minimal-body.html` | Minimal-content baseline at the opposite extreme — a 51-byte HTML body (a receipt confirmation) | 51 bytes, no tables |
| 05 | `05-sec-test-png-inline-image.html` | PNG image via `cid:` (a format `image__ReadOtherFormat` does **not** support — see Findings), 21 inline images, `colspan` | 24 KB, PNG unsupported-format case |
| 06 | `06-webp-inline-image.html` | WEBP via `cid:` (also unsupported), `<font>` tag. Subject is literally "I hope you can all read this" — a mailing-list thread about email rendering itself | 8 KB, WEBP unsupported-format case |
| 07 | `07-bakadesuyo-mismatched-table-tags.html` | Malformed markup: 10 `<table>` opens, 9 `</table>` closes — the parser-robustness case the design doc names as the direct `htmlview`-DOCTYPE-bug lesson | 59 KB, table count mismatch |
| 08 | `08-fidelity-mismatched-table-plus-font.html` | Same malformed-table class (35 open/36 close — extra close this time, the opposite imbalance direction from #07) plus `<font>` | 30 KB, table count mismatch (inverse) |
| 09 | `09-facebook-digest-font-tag-table.html` | Automated social-network digest template: `<font>`, `colspan`, moderate nesting (depth 9) | 15 KB |
| 10 | `10-economist-no-doctype-border0-table.html` | No `<!DOCTYPE>` at all (the exact structural class the current `htmlview` mishandles) combined with `border="0"` layout tables | 13 KB, no DOCTYPE |
| 11 | `11-cid-inline-images-supported-format.html` | `cid:` inline images in **supported** formats (contrast case for #05/#06) — confirms the happy path exists in the corpus, not just failure cases | 32 KB, 2 cid images, no unsupported types |
| 12 | `12-wordpress-colspan-tracking-pixel.html` | `colspan`, and two explicit tracking-pixel `<img>`s (`width="1" height="1"`, WordPress stats beacons) — informs the remote-image-blocking discussion with a concrete real example | 49 KB, colspan, literal tracking pixels |
| 13 | `13-bulletin-board-colspan-cid-image.html` | Small/typical case: `colspan` + one `cid:` image + one remote image together, shallow nesting (depth 4) — a "normal" mid-complexity message rather than an extreme | 10 KB |
| 14 | `14-simple-baseline-no-table-no-style.html` | The other baseline: real prose content, zero tables, zero `style=`, zero `<font>` — plain-text-shaped HTML mail, should be nearly free to render | 1.2 KB |
| 15 | `15-svg-and-webp-both-unsupported.html` | Both SVG and WEBP referenced in the same message — two unsupported formats at once | 7 KB, SVG+WEBP unsupported |
| 16 | `16-podcast-newsletter-deep-table-nesting.html` | Second-deepest nesting case (23 levels) from a non-Amazon sender — confirms #01's depth isn't an Amazon-template-specific artifact | 46 KB, 94 tables, depth 23, colspan |

`_manifest.json` in this directory has the same data in machine-readable
form (source file, subject, from, size, phone-pattern hit count) for
anything that wants to consume it programmatically.

## Findings relevant to the design doc's open questions

Computed over all 3223 messages with a `text/html` part, not just the
16 selected fixtures — the fixtures are examples, these numbers are
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
**28** (fixture 01; corroborated by fixture 16 at 23 from an unrelated
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
(fixture 15 has both, fixture 06 has WEBP alone) — worth adding to the
same "should `image.c` support this" discussion, WEBP especially,
since it's increasingly common as a default export format from mail
composers.

**Malformed table tags:** 99/3223 messages (3%) have a `<table>`/
`</table>` count mismatch — a real, non-negligible fraction. Fixtures
07 and 08 cover both imbalance directions (one extra open, one extra
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
URLs) were trivially easy to find (fixtures 01 and 12 both have them
unprompted, from unrelated senders) — the design doc's default-block
posture for remote images is well justified by real traffic, not a
hypothetical concern.
