# Deck Authoring Conventions

Governs every human-facing presentation in [presentation/](../../presentation/). Referenced from [markdown-writing-style § Audience](../skills/markdown-writing-style/SKILL.md), human-facing-presentation row — read before authoring a deck or regenerating its HTML.

The design system itself — layouts, styles, and HTML components — lives in [template.md](../../presentation/template/template.md), not here. This document carries only what the template cannot: where deck files go, how they are built, and the decisions an author would otherwise re-litigate.

## Two files per deck, one generated

- **Source:** `<deck-slug>-deck.md`, a Marp-flavoured markdown file. The only file an author edits.
- **Export:** `<deck-slug>-deck.html`, produced by [build-slides.py](../../presentation/slide-build-tool/build-slides.py) beside the markdown, same basename. Never hand-edited — the next build overwrites it.

```bash
python presentation/slide-build-tool/build-slides.py presentation/<deck-slug>/<deck-slug>-deck.md
```

A design change belongs in [template.md](../../presentation/template/template.md) followed by a rebuild, never in the generated HTML.

## Folder placement

- One subfolder per deck: `presentation/<deck-slug>/<deck-slug>-deck.{md,html}`. The top-level `m1-proposal-deck.*` predates this convention and stays at the root.
- Shared assets — background images, team photos, SVG diagrams — live once in [presentation/assets/](../../presentation/assets/), referenced by every deck through a relative `../assets/…` path.

## Assets are referenced, never embedded

Base64-inlining an image bloats the export beyond usefulness and makes it undiffable — it is why the root proposal deck's HTML is ~1.2 MB. Reference assets by relative path; add a new file to `presentation/assets/` rather than inlining it.

| Asset | Page type |
|---|---|
| `bg-title-city.jpg` | cover |
| `bg-navy-motif.png` | section pages |
| `bg-fpt-tower.jpg` | closing |

A gradient layered over a photo must use `rgba(…)` alpha, not opaque hex — an opaque gradient hides the image entirely.

## How to apply

Any agent authoring or updating a deck:

1. Write the markdown source only — follow [m1-proposal-deck.md](../../presentation/m1-proposal-deck.md) as the worked example, and [template.md § Source markdown format](../../presentation/template/template.md) for the directives the builder understands.
2. Run the builder. Do not hand-author or hand-patch the HTML.
3. Commit the source and its regenerated export together, so the two never drift.
