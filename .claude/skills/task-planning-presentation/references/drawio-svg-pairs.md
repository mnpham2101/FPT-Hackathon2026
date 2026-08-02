# Authoring draw.io / SVG Pairs by Hand

Reference for step 5 of [task-planning-presentation](../SKILL.md). Applies to any diagram authored for a deck in [presentation/](../../../../presentation/).

## Why by hand

**There is no draw.io CLI on the dev host** (verified 2026-08-02: no `drawio` on PATH, no desktop install). Nothing can convert `.drawio` → `.svg` automatically, so both halves are authored and both are verified. Do not plan around a converter that is not there, and do not ship a `.drawio` without its `.svg` — the deck embeds the SVG, so a missing export means a missing diagram.

Worked reference pair, already in the repo and correct in every respect: [system-design.drawio](../../../../requirements/system-design.drawio) and [system-design.svg](../../../../requirements/system-design.svg). Open both before authoring the first diagram.

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

- **Canvas 1320×720** matches the deck's slide geometry; a diagram authored at a different aspect ratio letterboxes or overflows.
- **Palette and typography** come from [template.md](../../../../presentation/template/template.md); the house reference for diagram style is [m1-phase-timeline.svg](../../../../presentation/assets/m1-phase-timeline.svg). Do not introduce a colour the deck does not already use.
- **Minimum ~13px type** on a 1320-wide canvas. If the content will not fit legibly, split the diagram across slides — never shrink to fit.
- **Escape `&`, `<`, `>` in shape labels** as `&amp;`, `&lt;`, `&gt;`. An unescaped ampersand in a subtask label is the most common way one of these files stops parsing.

## Verification — all four, every diagram

1. **Both files parse as XML.** A truncated `content` attribute still parses as an SVG while being useless to draw.io — check the extracted source parses too.
2. **The `content` attribute is byte-identical to the `.drawio`** after unescaping. This is what proves the pair cannot have drifted.
3. **The SVG renders non-empty.** Count painted elements; a file with a valid root and zero rects/paths is a blank diagram that passes every syntax check.
4. **Rasterise it and look at the image.** Headless Chrome (`--headless --screenshot`) is sufficient. This is the only check that catches text overflowing its shape, overlapping nodes, or content clipped past the viewBox — and it is the check most worth not skipping, because all three failures render as a plausible-looking file.

An automated geometry assertion — every text span fits inside its shape and inside the canvas — catches most of (4) cheaply and is worth adding to the generator once more than a couple of diagrams are in play.
