# KIS — Keep It Simple, static site

A dependency-free static website built from the hand-drawn concept page (`Designer.png`): the KIS logo sits in a large round-edged rectangle at the center of the home page, and four nodes (Concepts, Proposal, Design, Delivery) stand around it at the ring's corners. Every node is a page of its own.

The folder sits outside the four R5 node folders because it is a human-facing hub site, not node product code or test equipment.

## Run it

Open `index.html` directly, or serve the folder:

```bash
python -m http.server 8080   # from website-builder/
```

## Interaction model

A node is just a `PageLink` — an icon + label `<div>` built by `createPageLink()`. What clicking one does depends only on whether it has children in `js/site-data.js`, and where it's rendered:

- **On the home page** (mind map, `PageLink--node` style):
  - **A node with children (a branch)** — click toggles it open, revealing its children with connecting arrows drawn to each one (animated per the active theme); click again to collapse. It never navigates anywhere by itself.
  - **A node with no children (a leaf)** — click opens its own page in a new tab immediately.
  - The four root nodes (Concepts, Proposal, Design, Delivery) are visible from load, positioned at `WarningRing`'s corners; they draw no connecting arrow back to the logo.
- **On a node's own page** (the "Explore" grid, `PageLink--card` style): every card there is a leaf-like link — clicking it opens that page in a new tab directly. There's no expand/collapse concept on these pages; children are just listed.
- `prefers-reduced-motion` disables all animation.

## Layout

| Path | What it is |
|---|---|
| `index.html` | The home page: `NodeCanvas` + `ConnectorLayer` SVG; behaviour in `js/mindmap.js` |
| `pages/*.html` | One boilerplate page per node; all content renders from `js/site-data.js` via `js/page.js` |
| `assets/` | The logo SVGs (`kis-logo.svg`, `kis-logo-blueprint.svg`), `icons/*.svg`, and the `bg-*.jpg` backgrounds |
| `asset-tools/` | Generators for the derived assets in `assets/` — run by hand, never at page load |
| `css/base.css` | Reset + design tokens (CSS variables) |
| `css/components.css` | The reusable component classes |
| `css/themes.css` | One `[data-theme=…]` variable block per theme |
| `js/site-data.js` | The site graph: every node's id, parent, label, icon, href, canvas position, summary |
| `js/theme.js` | The theme registry and manager (`KIS.THEMES`, `KIS.theme`) |
| `js/components.js` | The component factories |
| `js/mindmap.js` | Home-page expand/collapse and animated connector drawing |
| `js/page.js` | Shared renderer for every node page |

## Reusable components

All markup is produced by factories in `js/components.js`; pages and the mind map compose them. Every factory takes one props object with the **common property set**:

| Prop | Meaning |
|---|---|
| `label` | visible text |
| `image` | icon id under `assets/icons/`, or a logo asset path |
| `href` | the page the component links to (opened in a new tab) |
| `style` | visual variant, mapped to a CSS modifier class (`PageLink--node`, `PageLink--card`) |
| `animation` | `{ enter: 'pop'|'rise'|'fade'|'none', delay: ms }` |

Components: `createPageLink` (the icon + text node), `createLogoFrame` (the logo in its rounded rectangle), `createThemeSwitch`, `mountTopBar`.

CSS classes mirror the components one-to-one — `PageLink`, `LogoFrame`, `NodeCanvas`, `ConnectorLayer`, `CardGrid`, `PageHeader`, `ThemeSwitch` — with BEM-style `__element` / `--modifier` names. Components read design tokens (`--accent`, `--node-bg`, …) and never hardcode a color. Icons are applied as CSS masks (`--icon-url`), so one SVG file recolors under every theme.

## Theme management

A theme is one name mapping to three things, and the two built-in themes (`sketch`, `blueprint`) are the worked examples:

1. **a CSS variable set** — a `[data-theme="name"]` block in `css/themes.css`;
2. **an asset set** — `KIS.THEMES[name].assets` (which logo file, which icon directory);
3. **an animation set** — `KIS.THEMES[name].animation` (connector draw duration/easing/dash, node enter animation and stagger).

`KIS.theme.apply(name)` stamps `data-theme` on the root, swaps every mounted logo, persists the choice, and emits `kis:theme` (the mind map listens and redraws its connectors). To add a theme, add the `KIS.THEMES` entry and the matching `themes.css` block — nothing else changes.
