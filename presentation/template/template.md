# Slide Deck Template

The template consumed by [build-slides.py](../slide-build-tool/build-slides.py). Every block below marked `<!-- block: NAME -->` is extracted by the builder and filled by replacing `{{PLACEHOLDER}}` tokens. Edit a block here and every deck rebuilt from it changes — the builder holds no markup of its own.

Design source: [m1-proposal-deck.html](../m1-proposal-deck.html). Authoring conventions — file placement, the asset policy, the build workflow — are in [deck-authoring-conventions.md](../../.claude/rules/deck-authoring-conventions.md).

## Slide canvas

Fixed `1280×720`, absolutely stacked inside a flex-centered `.deck`, exactly one `.active` at a time via `display`, scaled to the viewport by an inline `transform` the nav script writes. Inactive slides are `display: none` — **not** hidden by opacity. Keeping them in the layout to animate them makes Chromium mis-size the active slide; transitions were tried and reverted.

## Layouts

| Layout | Section classes | Source markdown |
|---|---|---|
| Cover | `slide cover-photo cover active` | first `_class: lead` slide + `![bg]()` |
| Section page | `slide divider` | later `_class: lead` slide, `# 01 · Title` |
| Slide page | `slide bg-light content` | any other slide, `# Title` + body |
| Closing | `slide thanks` | `_class: lead` slide whose title starts "Thank" |

Body markdown maps onto components by shape: leading paragraph → `p.lead`, `-` list → `ul.points`, pipe table → `table.fpt`, numbered list on the table-of-contents slide → `.toc` grid, `![h:NNN caption](path)` → `.imgbox`, `> quote` → `.strip`.

---

## Document skeleton

<!-- block: document -->
```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{{TITLE}}</title>
<meta name="description" content="{{DESCRIPTION}}">
<style>
{{STYLES}}
</style>
</head>
<body>

<div class="deck">
{{SLIDES}}
</div>

{{NAVIGATION}}

<script>
{{SCRIPT}}
</script>

</body>
</html>
```

---

## Styles

<!-- block: styles -->
```css
  :root {
    --fpt-orange: {{COLOR_ORANGE}};
    --fpt-green: {{COLOR_GREEN}};
    --fpt-blue: {{COLOR_BLUE}};
    --fpt-teal: {{COLOR_TEAL}};
    --fpt-navy: {{COLOR_NAVY}};
    --navy-deep: {{COLOR_NAVY_DEEP}};
    --ink: {{COLOR_INK}};
    --ink-soft: {{COLOR_INK_SOFT}};
    --paper: {{COLOR_PAPER}};
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  html, body { height: 100%; background: #14183a; font-family: {{FONT_DEFAULT}}; }

  .deck { position: fixed; inset: 0; display: flex; align-items: center; justify-content: center; overflow: hidden; }

  .slide {
    width: 1280px; height: 720px;
    position: absolute;
    display: none;
    flex-direction: column;
    transform-origin: center center;
    background: var(--paper);
    color: var(--ink);
    box-shadow: 0 18px 60px rgba(0,0,0,.45);
    overflow: hidden;
  }
  .slide.active { display: flex; }

  /* ---------- backgrounds ---------- */
  .bg-light { background: linear-gradient(135deg, #f6f6f8 0%, #ececef 55%, #e0e1e6 100%); }
  .bg-light::before {
    content: ""; position: absolute; inset: 0; pointer-events: none;
    background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1280 720"><g fill="none" stroke-width="2"><g transform="skewX(-16)"><rect x="240" y="60" width="380" height="460" rx="70" stroke="rgba(255,255,255,0.85)"/><rect x="1010" y="210" width="400" height="470" rx="75" stroke="rgba(25,34,109,0.05)"/><rect x="520" y="400" width="330" height="380" rx="60" stroke="rgba(25,34,109,0.06)"/></g><line x1="880" y1="-40" x2="640" y2="780" stroke="rgba(255,255,255,0.9)"/><line x1="1180" y1="-40" x2="940" y2="780" stroke="rgba(25,34,109,0.05)"/></g></svg>');
    background-size: cover;
  }
  .bg-navy { background: linear-gradient(135deg, #10173f 0%, #151d55 45%, var(--fpt-navy) 100%); color: #fff; }
  .bg-navy::before {
    content: ""; position: absolute; inset: 0; pointer-events: none;
    background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1280 720"><g fill="none" stroke-width="2"><g transform="skewX(-16)"><rect x="240" y="60" width="380" height="460" rx="70" stroke="rgba(120,180,255,0.20)"/><rect x="1010" y="210" width="400" height="470" rx="75" stroke="rgba(255,255,255,0.10)"/><rect x="520" y="400" width="330" height="380" rx="60" stroke="rgba(120,180,255,0.10)"/></g><line x1="880" y1="-40" x2="640" y2="780" stroke="rgba(120,180,255,0.30)"/><line x1="1180" y1="-40" x2="940" y2="780" stroke="rgba(120,180,255,0.16)"/></g></svg>');
    background-size: cover;
  }

  /* cover / divider / thanks carry their photo as an inline background-image */
  .cover-photo { background-size: cover; background-position: center; color: #fff; }
  .divider { background-size: cover; background-position: center; color: #fff; justify-content: center; padding: 0 110px; }
  .divider .sec-num { font-size: 120px; font-weight: 700; color: var(--fpt-orange); line-height: 1; opacity: .95; }
  .divider h1 { font-size: 56px; font-weight: 700; color: #fff; margin-top: 10px; }
  .divider .rule { width: 110px; height: 6px; background: var(--fpt-orange); border-radius: 3px; margin-top: 26px; }
  .thanks { background-size: cover; background-position: center 20%; color: #fff; justify-content: center; align-items: center; text-align: center; }
  .thanks h1 { font-size: 84px; font-weight: 700; }
  .thanks .rule { width: 130px; height: 6px; background: var(--fpt-orange); border-radius: 3px; margin: 30px auto; }
  .thanks p { font-size: 22px; color: #cdd6f7; }

  /* ---------- title / closing text ---------- */
  .cover { justify-content: center; padding: 0 110px; }
  .cover .eyebrow { font-size: 20px; letter-spacing: .35em; text-transform: uppercase; color: var(--fpt-orange); font-weight: 600; margin-bottom: 26px; }
  .cover h1 { font-size: 66px; line-height: 1.08; font-weight: 700; color: #fff; max-width: 900px; }
  .cover .rule { width: 130px; height: 6px; background: var(--fpt-orange); border-radius: 3px; margin: 30px 0; }
  .cover .tagline { font-size: 30px; font-weight: 300; color: #dfe6ff; margin-bottom: 14px; }
  .cover .meta { font-size: 18px; color: #b9c2ea; }
  .cover .meta a { color: #b9c2ea; }
  .cover .accents, .divider .accents, .thanks .accents { position: absolute; bottom: 0; left: 0; right: 0; height: 8px; display: flex; }
  .accents span { flex: 1; }

  /* ---------- content slide chrome ---------- */
  .content { padding: 44px 64px 58px; position: relative; }
  .content > * { position: relative; }
  .slide-head { display: flex; align-items: baseline; gap: 18px; border-bottom: 3px solid transparent; padding-bottom: 12px; margin-bottom: 18px;
    border-image: linear-gradient(90deg, var(--fpt-orange) 0 90px, rgba(25,34,109,.15) 90px 100%) 1; }
  .slide-head .num { font-size: 40px; font-weight: 700; color: var(--fpt-orange); line-height: 1; }
  .slide-head h2 { font-size: 31px; font-weight: 700; color: var(--fpt-navy); line-height: 1.15; }
  .lead { font-size: 19px; color: var(--ink-soft); margin-bottom: 16px; }
  .lead:last-of-type { margin-bottom: 0; }
  .lead strong, .lead b { color: var(--ink); }
  .content a { color: var(--fpt-blue); }

  .footer { position: absolute; left: 64px; right: 64px; bottom: 18px; display: flex; justify-content: space-between; align-items: center; font-size: 13px; color: #8a8d99; }
  .footer .page { background: var(--fpt-navy); color: #fff; border-radius: 4px; padding: 2px 10px; font-weight: 600; }

  ul.points { list-style: none; display: flex; flex-direction: column; gap: 12px; margin-bottom: 16px; }
  ul.points:last-child { margin-bottom: 0; }
  ul.points li { font-size: 18.5px; line-height: 1.42; color: var(--ink-soft); padding-left: 26px; position: relative; }
  ul.points li::before { content: ""; position: absolute; left: 0; top: 9px; width: 12px; height: 12px; background: var(--fpt-orange); border-radius: 3px; transform: skewX(-12deg); }
  ul.points li strong { color: var(--fpt-navy); }
  ul.points li code, .content code { font-family: {{FONT_MONO}}; font-size: .92em; background: rgba(25,34,109,.07); border-radius: 4px; padding: 1px 6px; }

  .imgbox { display: flex; flex-direction: column; align-items: center; gap: 8px; margin: 6px 0 14px; }
  .imgbox img { border-radius: 10px; box-shadow: 0 6px 22px rgba(15,20,60,.16); background: #fff; max-width: 100%; }
  .imgbox .cap { font-size: 13.5px; color: #8a8d99; font-style: italic; }
  .imgbox img.flat { box-shadow: none; background: transparent; }

  .cols { display: flex; gap: 34px; align-items: center; }

  /* TOC */
  .toc { display: grid; grid-template-columns: 1fr 1fr; gap: 18px 26px; margin-top: 8px; }
  .toc .item { display: flex; gap: 18px; align-items: baseline; background: #fff; border-radius: 10px; padding: 18px 22px; box-shadow: 0 5px 18px rgba(15,20,60,.10); border-left: 6px solid var(--fpt-orange); height: 100%; }
  .toc .item:nth-child(2) { border-left-color: var(--fpt-green); }
  .toc .item:nth-child(3) { border-left-color: var(--fpt-blue); }
  .toc .item:nth-child(4) { border-left-color: var(--fpt-teal); }
  .toc .item:nth-child(5) { border-left-color: var(--fpt-navy); }
  .toc .item:nth-child(6) { border-left-color: var(--fpt-orange); }
  .toc .n { font-size: 34px; font-weight: 700; color: var(--fpt-orange); min-width: 52px; }
  .toc .item:nth-child(2) .n { color: var(--fpt-green); }
  .toc .item:nth-child(3) .n { color: var(--fpt-blue); }
  .toc .item:nth-child(4) .n { color: var(--fpt-teal); }
  .toc .item:nth-child(5) .n { color: var(--fpt-navy); }
  .toc h3 { font-size: 21px; color: var(--fpt-navy); margin-bottom: 4px; }
  .toc p { font-size: 15px; color: var(--ink-soft); line-height: 1.35; }

  /* cards */
  .cards3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 20px; margin: 6px 0 18px; }
  .card { background: #fff; border-radius: 10px; padding: 16px 18px 14px; box-shadow: 0 5px 18px rgba(15,20,60,.10); border-top: 5px solid var(--fpt-orange); }
  .card.g { border-top-color: var(--fpt-green); }
  .card.b { border-top-color: var(--fpt-blue); }
  .card h3 { font-size: 19px; color: var(--fpt-navy); margin-bottom: 6px; }
  .card p { font-size: 15.5px; line-height: 1.4; color: var(--ink-soft); }

  /* tables */
  table.fpt { width: 100%; border-collapse: collapse; background: #fff; border-radius: 10px; overflow: hidden; box-shadow: 0 5px 18px rgba(15,20,60,.10); margin-bottom: 14px; }
  table.fpt th { background: var(--fpt-navy); color: #fff; text-align: left; font-size: 16px; padding: 9px 16px; }
  table.fpt td { font-size: 15.5px; line-height: 1.35; padding: 8.5px 16px; border-bottom: 1px solid #e7e8ee; color: var(--ink-soft); vertical-align: top; }
  table.fpt tr:last-child td { border-bottom: none; }
  table.fpt td strong { color: var(--fpt-navy); white-space: nowrap; }
  .tag { display: inline-block; padding: 1px 9px; border-radius: 4px; color: #fff; font-size: 14px; font-weight: 600; }
  .tag.o { background: var(--fpt-orange); } .tag.b { background: var(--fpt-blue); } .tag.t { background: var(--fpt-teal); } .tag.g { background: var(--fpt-green); } .tag.n { background: var(--fpt-navy); }

  /* seam cards */
  .seams { display: flex; flex-direction: column; gap: 13px; margin-bottom: 16px; }
  .seam { background: #fff; border-radius: 8px; border-left: 6px solid var(--fpt-orange); box-shadow: 0 4px 14px rgba(15,20,60,.09); padding: 12px 18px; display: flex; gap: 16px; align-items: baseline; }
  .seam.b { border-left-color: var(--fpt-blue); } .seam.t { border-left-color: var(--fpt-teal); }
  .seam .who { font-weight: 700; color: var(--fpt-navy); font-size: 17.5px; white-space: nowrap; min-width: 200px; }
  .seam p { font-size: 16px; line-height: 1.4; color: var(--ink-soft); }
  .strip { background: linear-gradient(90deg, var(--fpt-navy), #2a35a0); color: #dfe6ff; border-radius: 8px; padding: 12px 20px; font-size: 15.5px; line-height: 1.45; margin-bottom: 14px; }
  .strip strong { color: #fff; }
  .strip:last-child { margin-bottom: 0; }

  /* chain visual */
  .chain { display: flex; align-items: center; justify-content: center; gap: 10px; margin: 10px 0 22px; flex-wrap: nowrap; }
  .chain .link { background: var(--fpt-navy); color: #fff; font-size: 16.5px; font-weight: 600; padding: 9px 20px; border-radius: 7px; transform: skewX(-10deg); box-shadow: 0 4px 12px rgba(15,20,60,.18); }
  .chain .link > span { display: inline-block; transform: skewX(10deg); }
  .chain .link.hot { background: var(--fpt-orange); }
  .chain .arr { color: var(--fpt-orange); font-size: 22px; font-weight: 700; }
  .chain .link small { display: block; font-size: 12px; font-weight: 400; opacity: .8; transform: skewX(10deg); }

  .badges { display: flex; gap: 10px; justify-content: center; margin: 0 0 20px; flex-wrap: wrap; }
  .badge { border: 2px solid var(--fpt-orange); color: #b34e0e; background: rgba(243,112,33,.07); font-size: 14.5px; font-weight: 600; padding: 5px 14px; border-radius: 999px; }
  .badge.g { border-color: var(--fpt-green); color: #2c7a25; background: rgba(80,184,72,.09); }

  /* terminal / log evidence */
  pre.log { background: var(--navy-deep); color: #cfd6f5; font-family: {{FONT_MONO}}; font-size: 13.5px; line-height: 1.5; border-radius: 8px; padding: 14px 18px; overflow: hidden; margin-bottom: 14px; }
  pre.log .ok { color: #7ee787; }
  pre.log .dim { color: #7f88ad; }

  /* ---------- controls ---------- */
  .hint { position: fixed; left: 18px; bottom: 12px; color: rgba(255,255,255,.45); font-size: 12.5px; z-index: 50; }
  .hud { position: fixed; right: 18px; bottom: 14px; z-index: 50; display: flex; gap: 8px; align-items: center; font-family: inherit; }
  .hud button { background: rgba(255,255,255,.12); color: #fff; border: 1px solid rgba(255,255,255,.25); border-radius: 6px; padding: 5px 12px; font-size: 15px; cursor: pointer; }
  .hud button:hover { background: var(--fpt-orange); border-color: var(--fpt-orange); }
  .hud .ctr { color: rgba(255,255,255,.75); font-size: 14px; min-width: 60px; text-align: center; }
  .progress { position: fixed; left: 0; bottom: 0; height: 4px; background: var(--fpt-orange); width: 0; z-index: 60; transition: width .25s ease; }

  @media print {
    html, body { background: #fff; height: auto; }
    .deck { position: static; display: block; }
    .slide { display: flex !important; position: relative; transform: none !important; box-shadow: none; page-break-after: always; margin: 0 auto; }
    .hud, .progress, .hint { display: none !important; }
    @page { size: 13.55in 7.65in; margin: 0; }
  }
```

---

## HTML components

### Navigation

<!-- block: navigation -->
```html
<div class="hint">← → navigate · F fullscreen · print for PDF</div>
<div class="hud">
  <button id="prev" aria-label="Previous slide">‹</button>
  <span class="ctr" id="ctr"></span>
  <button id="next" aria-label="Next slide">›</button>
</div>
<div class="progress" id="prog"></div>
```

<!-- block: script -->
```js
  const slides = Array.from(document.querySelectorAll('.slide'));
  slides.forEach((sl, i) => { const p = sl.querySelector('.page'); if (p) p.textContent = i + 1; });
  let cur = 0;

  function fit() {
    const s = Math.min(innerWidth / 1280, innerHeight / 720);
    slides.forEach(sl => sl.style.transform = `scale(${s})`);
  }
  function show(i) {
    cur = Math.max(0, Math.min(slides.length - 1, i));
    slides.forEach((sl, k) => sl.classList.toggle('active', k === cur));
    document.getElementById('ctr').textContent = `${cur + 1} / ${slides.length}`;
    document.getElementById('prog').style.width = `${(cur + 1) / slides.length * 100}%`;
    location.hash = cur ? `#${cur + 1}` : '';
  }
  addEventListener('resize', fit);
  addEventListener('keydown', e => {
    if (['ArrowRight', 'PageDown', ' ', 'Enter'].includes(e.key)) { e.preventDefault(); show(cur + 1); }
    else if (['ArrowLeft', 'PageUp', 'Backspace'].includes(e.key)) { e.preventDefault(); show(cur - 1); }
    else if (e.key === 'Home') show(0);
    else if (e.key === 'End') show(slides.length - 1);
    else if (e.key.toLowerCase() === 'f') document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen();
  });
  document.getElementById('prev').onclick = () => show(cur - 1);
  document.getElementById('next').onclick = () => show(cur + 1);
  let tx = null;
  addEventListener('touchstart', e => tx = e.touches[0].clientX);
  addEventListener('touchend', e => { if (tx !== null) { const dx = e.changedTouches[0].clientX - tx; if (Math.abs(dx) > 50) show(cur + (dx < 0 ? 1 : -1)); tx = null; } });

  const h = parseInt((location.hash || '').slice(1), 10);
  fit(); show(isNaN(h) ? 0 : h - 1);
```

### Cover page

Photo background carried inline so the deck can sit at any folder depth; the gradient wash keeps the title legible over it.

<!-- block: cover -->
```html
  <section class="slide cover-photo cover active"{{BG}}>
    <div class="eyebrow">{{EYEBROW}}</div>
    <h1>{{TITLE}}</h1>
    <div class="rule"></div>
    <div class="tagline">{{TAGLINE}}</div>
    <div class="meta">{{META}}</div>
{{ACCENTS}}
  </section>
```

<!-- block: cover-gradient -->
```text
linear-gradient(90deg, rgba(8,12,38,.92) 0%, rgba(8,12,38,.72) 40%, rgba(8,12,38,.18) 100%)
```

### Section page

<!-- block: section -->
```html
  <section class="slide divider"{{BG}}>
    <div class="sec-num">{{NUMBER}}</div>
    <h1>{{TITLE}}</h1>
    <div class="rule"></div>
{{ACCENTS}}
  </section>
```

<!-- block: section-gradient -->
```text
linear-gradient(rgba(16,23,63,.85), rgba(16,23,63,.90))
```

### Slide page

<!-- block: slide -->
```html
  <section class="slide bg-light content">
    <div class="slide-head"><div class="num">{{NUMBER}}</div><h2>{{TITLE}}</h2></div>
{{BODY}}
{{FOOTER}}
  </section>
```

<!-- block: footer -->
```html
    <div class="footer"><span>{{DECK}}</span><span class="page"></span></div>
```

### Closing page

<!-- block: thanks -->
```html
  <section class="slide thanks"{{BG}}>
    <h1>{{TITLE}}</h1>
    <div class="rule"></div>
    <p>{{BODY}}</p>
{{ACCENTS}}
  </section>
```

<!-- block: thanks-gradient -->
```text
linear-gradient(rgba(8,12,38,.66), rgba(14,21,64,.86))
```

### Accent strip

The four-accent bar at the bottom of every cover, section, and closing page — the deck's signature, not per-slide styling.

<!-- block: accents -->
```html
    <div class="accents"><span style="background:var(--fpt-orange)"></span><span style="background:var(--fpt-green)"></span><span style="background:var(--fpt-blue)"></span><span style="background:var(--fpt-teal)"></span></div>
```

### Table of contents

<!-- block: toc -->
```html
    <div class="toc">
{{ITEMS}}
    </div>
```

<!-- block: toc-item -->
```html
      <div class="item"><div class="n">{{NUMBER}}</div><div><h3>{{TITLE}}</h3><p>{{DESCRIPTION}}</p></div></div>
```

### Image box

<!-- block: imgbox -->
```html
    <div class="imgbox"><img src="{{SRC}}"{{STYLE}} alt="{{CAPTION}}"><div class="cap">{{CAPTION}}</div></div>
```

---

## Source markdown format

Marp-flavoured markdown — the same source that renders in a Marp preview also builds here.

```markdown
---
title: Deck title
description: One-line description
deck: Footer text shown on every slide page
---

<!-- _class: lead -->
![bg](../assets/bg-title-city.jpg)

# Deck title

## Tagline under the title

**Eyebrow line above the title**

Meta line · date · source links

---

<!-- _class: lead -->
![bg](../assets/bg-navy-motif.png)

# 01 · Section name

---

# Slide title

Leading paragraph becomes `p.lead`.

- **Lead-in:** a bullet becomes a `ul.points` item.

| Column | Column |
| ------ | ------ |
| Cell   | Cell   |

![h:260 Caption text](../assets/diagram.svg)
```

- `---` on its own line separates slides.
- `<!-- _class: lead -->` marks a cover / section / closing page; the first one is the cover, a title starting "Thank" is the closing, the rest are section pages.
- `![bg](path)` sets that page's photo background. Paths are relative to the markdown file and are copied through unchanged.
- `# 01 · Name` on a section page splits into `.sec-num` `01` and heading `Name`.
- Slide pages inherit their `.slide-head .num` from the most recent section page; the table-of-contents slide uses `§`.
- `![h:260 Caption](path)` sets the rendered image height in px and the caption under it.
- `> quoted line` becomes a `.strip` callout.

## Frontmatter overrides

```yaml
---
title: Deck title
description: One-line description
deck: Footer text
colors:
  orange: "#F37021"
  green: "#50B848"
  blue: "#034EA2"
  teal: "#33B2C1"
  navy: "#19226D"
  navy_deep: "#0E1540"
  ink: "#080808"
  ink_soft: "#3d3d46"
  paper: "#f4f4f6"
fonts:
  default: '"Segoe UI", "Segoe UI Variable", "Helvetica Neue", Arial, sans-serif'
  mono: '"Consolas", "Courier New", monospace'
---
```

## Building

```bash
python presentation/slide-build-tool/build-slides.py presentation/phase0/phase0-smoke-test-deck.md
```

The HTML is always written beside the markdown, same basename: `presentation/phase0/phase0-smoke-test-deck.html`.
