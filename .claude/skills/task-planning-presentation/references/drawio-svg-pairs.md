# Authoring draw.io / SVG Pairs by Hand

Reference for step 5 of [task-planning-presentation](../SKILL.md). Applies to any diagram authored for a deck in [presentation/](../../../../presentation/).

## Why by hand

**There is no draw.io CLI on the dev host** (verified 2026-08-02: no `drawio` on PATH, no desktop install). Nothing can convert `.drawio` → `.svg` automatically, so both halves are authored and both are verified. Do not plan around a converter that is not there, and do not ship a `.drawio` without its `.svg` — the deck embeds the SVG, so a missing export means a missing diagram.

Worked reference pair, already in the repo and correct in every respect: [system-design.drawio](../../../../documents/Requirements/system-design.drawio) and [system-design.svg](../../../../documents/Requirements/system-design.svg). Open both before authoring the first diagram.

## The two files

**`<name>.drawio`** — plain mxGraphModel XML, uncompressed so it stays diffable:

```xml
<mxfile host="app.diagrams.net">
  <diagram name="<name>" id="<name>">
    <mxGraphModel dx="1368" dy="788" grid="1" gridSize="10" page="1" pageWidth="1320" pageHeight="720" math="0" shadow="0">
      <root>
        <mxCell id="0" />
        <mxCell id="1" parent="0" />
        <!-- shapes and edges, each parent="1" -->
      </root>
    </mxGraphModel>
  </diagram>
</mxfile>
```

**`<name>.svg`** — a standalone SVG that draw.io can also reopen, because the whole `mxfile` above is XML-escaped into the root `content` attribute:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Editable SVG: the diagram source lives in the root svg content attribute; open with draw.io to edit. Paired source: <name>.drawio -->
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1"
     width="1320px" height="720px" viewBox="0 0 1320 720"
     font-family="Segoe UI, Arial, sans-serif"
     content="&lt;mxfile host=&quot;app.diagrams.net&quot;&gt;…&lt;/mxfile&gt;">
  <!-- painted geometry: rects, paths, text -->
</svg>
```

The SVG carries *both* the escaped source and real painted geometry. Only the geometry renders; only the `content` attribute reopens in draw.io. Omitting either breaks one of the two consumers.

## Generate both from one shape list

Hand-maintaining two representations of the same picture guarantees they drift. Write the diagram once as a list of shapes and edges in a throwaway script, then emit the `.drawio` XML and the `.svg` geometry from that same list. The two files then cannot disagree, and the `content` attribute is produced by escaping the emitted `.drawio` text rather than by transcribing it.

Keep the generator in the scratchpad, not the repo — it is a means of production, not a deliverable.

## Sizing and style

- **Canvas ~1200 px wide**, at an aspect ratio the render box below accepts — the deck's assets are 1200×540–660, which render close to 1:1. A taller canvas is scaled down until its type stops being readable.
- **Palette and typography** come from [template.md](../../../../presentation/template/template.md); the house reference for diagram style is [m1-phase-timeline.svg](../../../../presentation/assets/m1-phase-timeline.svg). Do not introduce a colour the deck does not already use.
- **Minimum ~13px type** on a 1320-wide canvas. If the content will not fit legibly, split the diagram across slides — never shrink to fit.
- **Escape `&`, `<`, `>`, `"` in shape labels** as `&amp;`, `&lt;`, `&gt;`, `&quot;` — labels land in an XML *attribute* value (`<mxCell value="…">`), where a bare quote closes the attribute early. An unescaped ampersand is the most common way one of these files stops parsing; an unescaped quote is the most deceptive, because it corrupts the `.drawio` while the `.svg` still renders perfectly and passes every visual check.

## The render box, and fitting an existing diagram into it

A slide is 1280×720 and its content area is smaller. An image on a content slide gets **at most ~1152 px wide and ~520 px tall** — the width after the 64 px side padding, and what is left of the height once the 44/58 px padding, the slide header and the caption are taken. `h:495`–`h:520` is therefore the working range, and the deck's own assets are ~1200 px wide so they render near 1:1.

**The rendered type size is what decides whether a diagram is usable**, and it follows from the canvas, not from the `h:` value:

```
scale = min(1152 / canvas_width, 520 / canvas_height)
rendered_pt = source_font_size × scale        # keep ≥ 10 px; below ~8 px is unreadable
```

An HLD component diagram is drawn for a page — 1700–1900 px wide with 12–17 px type — and lands at 6–7 px on a slide. A PlantUML render of a sequence `.puml` is worse and in the wrong visual language. Neither is dropped onto a slide as-is.

To derive a deck copy from an HLD diagram, keeping the HLD copy authoritative:

1. **Copy both halves** into `presentation/assets/` under a `phase<N>-` name, and add a header comment naming the source file and stating that the HLD copy is authoritative.
2. **Drop the diagram's own title, subtitle and legend** from both halves — the slide title carries the first, and the legend becomes its own slide. Drop by identity, not by eye: cells whose geometry lies entirely inside the legend band, and text whose `y` is entirely above the first content row.
3. **Re-crop.** Recompute the content bounding box from what survived, translate it to a 20 px margin, and set `pageWidth`/`pageHeight` on the `.drawio` and `width`/`height`/`viewBox` on the `.svg` to match. Wrapping the surviving SVG elements in one `<g transform="translate(…)">` is enough; do not renumber coordinates by hand.
4. **Re-inject** the edited `.drawio` into the `.svg` `content` attribute, so verification 2 still holds.
5. **Diff the label sets** of source and copy, and confirm that everything lost is title or legend. This is what proves a component was not silently dropped.
6. **Recompute the scale.** Still below the floor, split the diagram across two slides along a subsystem boundary rather than accepting unreadable type.

Do the whole transform in a throwaway script, for the same reason the pair is generated from one shape list: hand-editing two files toward one geometry drifts.

## Verification — all four, every diagram

1. **Both files parse as XML.** A truncated `content` attribute still parses as an SVG while being useless to draw.io — check the extracted source parses too.
2. **The `content` attribute is byte-identical to the `.drawio`** after unescaping. This is what proves the pair cannot have drifted.
3. **The SVG renders non-empty.** Count painted elements; a file with a valid root and zero rects/paths is a blank diagram that passes every syntax check.
4. **Rasterise it and look at the image.** Headless Chrome (`--headless --screenshot`) is sufficient. This is the only check that catches text overflowing its shape, overlapping nodes, or content clipped past the viewBox — and it is the check most worth not skipping, because all three failures render as a plausible-looking file.

An automated geometry assertion — every text span fits inside its shape and inside the canvas — catches most of (4) cheaply and is worth adding to the generator once more than a couple of diagrams are in play.
