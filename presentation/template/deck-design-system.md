---
name: deck-design-system
description: Reusable design system for this project's Marp decks and their static HTML exports — colors, background images, CSS class catalog, slide-canvas mechanics, navigation JS, and a copy-paste skeleton. Read before authoring a new presentation instead of re-deriving the design from an existing deck.
---

# Deck Design System — Template & Reference

Every human-facing presentation in this repo is two files: a **Marp markdown source** (`<slug>-deck.md`) and a **hand-authored static HTML export** (`<slug>-deck.html`) — not a `marp-cli` build output. This doc is the design system behind both, extracted from [m1-proposal-deck.md](../m1-proposal-deck.md)/[.html](../m1-proposal-deck.html) (full proposal) and [phase0/phase0-smoke-test-deck.md](../phase0/phase0-smoke-test-deck.md)/[.html](../phase0/phase0-smoke-test-deck.html) (report deck). Read this instead of re-reading either full HTML file to rebuild the design from scratch.

The animated slide-canvas mechanics below are current as of the phase0 deck; the root `m1-proposal-deck.html` predates them and still hard-toggles `display: none`/`flex` with no transition — treat the phase0 deck, not the root one, as the transition reference.

## Folder placement

- One subfolder per deck under `presentation/`: `presentation/<deck-slug>/<deck-slug>-deck.{md,html}`. The top-level `m1-proposal-deck.*` predates this convention and stays at the root.
- Shared assets (background images, team photos, SVG diagrams) live once in `presentation/assets/`, referenced by every deck via a relative `../assets/…` path — never copied per-deck, never inlined as base64. Base64-embedded backgrounds bloat the file (the root proposal deck's `.html` is ~1.2 MB for this reason) and are not diffable; a new deck should not repeat that.

## Color palette

CSS custom properties on `:root` — every deck defines the same set:

| Variable | Hex | Use |
|---|---|---|
| `--fpt-orange` | `#F37021` | primary accent — headings' left rule, bullet marks, active nav, badges |
| `--fpt-green` | `#50B848` | secondary accent — 2nd card/TOC-item color, "good/closed" badges |
| `--fpt-blue` | `#034EA2` | tertiary accent — 3rd card/TOC-item color |
| `--fpt-teal` | `#33B2C1` | quaternary accent — 4th card/TOC-item color |
| `--fpt-navy` | `#19226D` | headings, table headers, `divider` slide background base |
| `--navy-deep` | `#0E1540` | darkest accent — log blocks, deep card borders |
| `--ink` | `#080808` | body text on light slides |
| `--ink-soft` | `#3d3d46` | secondary/paragraph text on light slides |
| `--paper` | `#f4f4f6` | slide canvas background (`.slide` default) |

The four-accent strip (`orange, green, blue, teal`) appears as a thin bar at the bottom of every `cover`/`divider`/`thanks` slide — the deck's signature, not optional per-slide styling.

## Background images (`presentation/assets/`)

| File | Slide type | Applied as |
|---|---|---|
| `bg-title-city.jpg` | Title/cover | `.cover-photo` — dark gradient wash `rgba(10,14,40,.55→.82)` over the photo |
| `bg-navy-motif.png` | Section dividers | `.divider` — semi-transparent navy gradient (`rgba(16,23,63,.85–.9)`) over the motif, so it reads through instead of being hidden by an opaque gradient |
| `bg-fpt-tower.jpg` | Closing/"Thank you" | `.thanks` — same dark wash treatment as the cover |
| Team photos (`*.png`/`*.JPG`) | Team-info slide only | `.team .member img`, circular, accent-colored border per column |

Rule: a gradient layered over a photo must use `rgba(...)` alpha, not opaque hex — an opaque gradient fully hides the image underneath it.

## Slide canvas mechanics

Fixed-size slides (`1280×720`), stacked absolutely, one `.active` at a time, scaled to fit the viewport — this is what makes the deck behave the same in a browser window, fullscreen, or print. Transitions are opacity + scale + a slight vertical settle, driven entirely by CSS so `show()` only ever toggles the `.active` class:

```css
.deck { position: fixed; inset: 0; display: flex; align-items: center; justify-content: center; overflow: hidden; }
.slide {
  width: 1280px; height: 720px; position: absolute; display: flex; flex-direction: column;
  transform-origin: center center; background: var(--paper); color: var(--ink);
  box-shadow: 0 18px 60px rgba(0,0,0,.45); overflow: hidden;
  opacity: 0; pointer-events: none; z-index: 1;
  transform: scale(var(--s, 1)) scale(.96) translateY(10px);
  transition: opacity .45s ease, transform .5s cubic-bezier(.22,.75,.32,1);
}
.slide.active {
  opacity: 1; pointer-events: auto; z-index: 2;
  transform: scale(var(--s, 1)) scale(1) translateY(0);
}
@media (prefers-reduced-motion: reduce) {
  .slide { transition: opacity .01s linear; transform: scale(var(--s, 1)); }
}
```

The viewport-fit scale lives in the `--s` custom property (not an inline `transform`) precisely so the stylesheet can compose it with the per-slide pop/settle transform above — setting `sl.style.transform` directly, as older decks in this repo do, would overwrite that composition and kill the animation:

```js
function fit() {
  const s = Math.min(innerWidth / 1280, innerHeight / 720);
  document.documentElement.style.setProperty('--s', s);
}
```

`@media print` must additionally force `opacity: 1 !important; transition: none !important;` on `.slide` — otherwise every non-active slide prints blank, since the opacity-based hide is the default state.

## Navigation & controls

Copy-paste as-is — every deck in this repo uses the identical script (see either `.html` file's closing `<script>` block for the full version):

- **Keyboard:** `→`/`PageDown`/`Space`/`Enter` next, `←`/`PageUp`/`Backspace` prev, `Home`/`End` jump, `F` fullscreen.
- **Touch:** horizontal swipe (>50px) advances/retreats.
- **Deep link:** `location.hash` tracks the current slide (`#3` = 3rd slide) — reload-safe.
- **HUD:** fixed bottom-right `‹ N/total ›` buttons; fixed bottom-left orange progress bar; fixed bottom-left keyboard hint text.
- **Print:** `@media print` disables the fixed positioning/scaling and the HUD, one slide per page, sized `13.55in × 7.65in`.

## Slide-type catalog

One class combo per slide `<section>`; pick by content shape, not by section:

| Slide need | Classes on `<section>` | Structure |
|---|---|---|
| Title | `slide cover-photo cover active` | `.eyebrow`, `h1`, `.rule`, `.tagline`, `.meta`, `.accents` |
| Table of contents | `slide bg-light content` | `.slide-head` + `.toc` grid of `.item` cards (`.n` number, `h3`, `p`) |
| Section divider | `slide divider` | `.sec-num` (2-digit), `h1`, `.rule`, `.accents` — one per major section, no body content |
| Bullet-point content | `slide bg-light content` | `.slide-head` + `ul.points` (one `<li>` per idea, `<strong>` lead-in) |
| Enumerable facts / comparison | `slide bg-light content` | `.slide-head` + `table.fpt` (`<th>` navy header row, `.k` class on the key column) |
| 3-way comparison or goals | `slide bg-light content` | `.slide-head` + `.cards3` grid of `.card`/`.card.g`/`.card.b` |
| Labeled callouts (who does what) | `slide bg-light content` | `.slide-head` + `.seams` stack of `.seam`/`.seam.b`/`.seam.t`/`.seam.g`/`.seam.n` (`.who` label + `p`) |
| Process/data flow | `slide bg-light content` | `.chain` row of `.link`/`.link.hot` boxes joined by `.arr` — each `.link` can carry a `<small>` sub-label |
| Terminal/log evidence | `slide bg-light content` | `pre.log` (dark `--navy-deep` block; `.ok`/`.dim` spans for coloring specific lines) |
| Status/fact strip | anywhere inside `.content` | `.strip` (navy gradient bar) or `.badges` (`.badge`/`.badge.g` pill row) |
| Closing | `slide thanks` | `h1`, `.rule`, `p`, `.accents` |

Every content slide ends with `.footer` (`<span>` deck name/context left, `<span class="page">` number right, auto-filled by the shared JS).

## Marp source conventions (the `.md`)

- Frontmatter: `marp: true`, `theme: default`, `paginate: true`, `title:`, `description:`.
- `---` separates slides; `<!-- _class: lead -->` + `<!-- _paginate: false -->` marks the title/closing slides; `![bg](path)` sets a full-bleed background image on those.
- One idea per slide — if a bullet list needs a sub-point that isn't itself a new idea, it's still one slide; if it's a genuinely new idea, it's a new slide.
- Table of contents slide numbers each section; section-divider slides in the body echo those same numbers (`01`, `02`, …) so the HTML's `.sec-num`/`.toc .n` stay traceable to the same list.

## When authoring a new deck

1. Copy the shared boilerplate above (root variables, `.deck`/`.slide` mechanics, nav JS) verbatim — do not re-derive it.
2. Pick slide types from the catalog table per idea, not the other way around — don't force content into a mismatched layout.
3. Reference `../assets/` images by relative path; add a new asset to `presentation/assets/` rather than inlining base64.
4. For CSS class definitions not reproduced above (`.toc`, `.cards3`/`.card`, `table.fpt`, `.seams`/`.seam`, `.chain`, `pre.log`, `.badges`, `.footer`, `.hud`/`.progress`/`.hint`), copy the block straight out of [phase0/phase0-smoke-test-deck.html](../phase0/phase0-smoke-test-deck.html)'s `<style>` — it carries the full, currently-correct set without the root deck's base64 bloat.

## How to apply

Referenced from [markdown-writing-style](../../.claude/skills/markdown-writing-style/SKILL.md) § Audience, human-facing-presentation row — read this doc before authoring or exporting a new deck, in addition to following an existing deck as a worked example.
