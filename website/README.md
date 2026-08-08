# KIS — Keep It Simple, static site

A dependency-free static website built from the hand-drawn concept page (`Designer.png`): the KIS logo sits in a large round-edged rectangle at the center of the home page, and four nodes (Concepts, Proposal, Design, Delivery) stand around it at the ring's corners. Every node is a page of its own.

The folder is top-level, beside [presentation/](../presentation/) and for the same reason: both are human-facing publications of the project, each self-contained with its own content, design system and generator. It is not node product code, and not the test equipment [tools/](../tools/) is for.

It renders [documents/](../documents/) but does not own it. That folder stays where every rule and design document expects it, and the decks read it too — the site is one view of that content, not its home.

## Run it

Open `website/index.html` directly, or serve **the repository root**:

```bash
python -m http.server 8080        # from the repo root, not from website/
# then open http://localhost:8080/website/index.html
```

**The root is the repo, not this folder.** A document page links out to the deck exports in [../presentation/](../presentation/) and to schemas and diagrams elsewhere in the repo, because only images are copied in (§ What happens to each link target). Serving `website/` alone puts those targets above the web root, and every one of them 404s — the "Abridged version" links on the HLD pages are the ones a reader hits first. Opening the files directly works for the same reason: `file://` has no root to climb above. The repo's root [index.html](../index.html) redirects here, so `http://localhost:8080/` finds the site without anyone having to know that rule.

## Sending it to someone

`--bundle` writes a self-contained copy that needs neither a server nor Python:

```bash
python website/build-pages.py --bundle          # writes dist/
python website/build-pages.py --bundle out/     # or anywhere else
```

Zip it and send it; the reader double-clicks `index.html`. Roughly 20 MB, and generated — `dist/` is gitignored, and is rebuilt rather than committed.

**The bundle mirrors the repository's own layout** for every file the site reaches, rather than flattening it. That is what lets it copy everything and rewrite almost nothing: a deck's `../assets/…` and a page's `../../presentation/…` both still resolve, because their relative positions are unchanged. The repo's root `index.html` rides along as the entry point, which is why `<bundle>/index.html` opens the site.

One thing is rewritten, and only in the copy: a deck linking a `.md` file is repointed at the page built from it, so a reader clicking "IVI ECU — high-level design" inside a deck gets the rendered page instead of raw markdown. The deck in `presentation/` is untouched — the bundle's copy is a derived artifact, exactly as the export itself is.

What does not survive the bundle: **a link to a folder rather than a file** — one, `contracts/samples/`. The bundle copies files, and a directory link has nothing to resolve to once the repository is not there.

The document pages in `pages/` are generated. Rebuild them after any change to the markdown they render:

```bash
pip install -r website/requirements.txt   # once
python website/build-pages.py             # from the repo root
python website/build-pages.py --clean     # drop stale pages first
python website/build-pages.py documents/Design/README.md   # just one
```

## Interaction model

A node is just a `PageLink` — an icon + label `<div>` built by `createPageLink()`. What clicking one does depends only on whether it has children in `js/site-data.js`, and where it's rendered:

- **On the home page** (mind map, `PageLink--node` style):
  - **A node with children (a branch)** — click toggles it open, revealing its children with connecting arrows drawn to each one (animated per the active theme); click again to collapse. It never navigates anywhere by itself.
  - **A node with no children (a leaf)** — click opens its own page in a new tab immediately.
  - The four root nodes (Concepts, Proposal, Design, Delivery) are visible from load, positioned at `WarningRing`'s corners; they draw no connecting arrow back to the logo.
- **On a node's own page** (the "Explore" grid, `PageLink--card` style): every card there is a leaf-like link — clicking it opens that page in a new tab directly. There's no expand/collapse concept on these pages; children are just listed.
- **On a generated document page** every link out of the document opens in a new tab; the breadcrumb and the logo navigate in place, since they are how a reader gets back rather than onward.
- `prefers-reduced-motion` disables all animation.

## Layout

| Path | What it is |
|---|---|
| `index.html` | The home page: `NodeCanvas` + `ConnectorLayer` SVG; behaviour in `js/mindmap.js`. Carries no table of contents — it is a mind map, not a document |
| `pages/` | **Generated in full — gitignored, never committed, never hand-edited.** Two kinds in one flat folder: the document pages rendered from markdown (via `js/doc-page.js`), and a mount page per site node that opens no document (via `js/page.js`). Run the build after cloning |
| `build-pages.py` | The markdown → HTML generator |
| `requirements.txt` | Build-time Python packages; the site itself stays dependency-free |
| `assets/` | The logo SVGs (`kis-logo.svg`, `kis-logo-blueprint.svg`), `icons/*.svg`, and the `bg-*.jpg` backgrounds |
| `asset-tools/` | Generators for the derived assets in `assets/` — run by hand, never at page load |
| `css/base.css` | Reset + design tokens (CSS variables) |
| `css/components.css` | The reusable component classes |
| `css/doc.css` | `DocLayout` and `Markdown` — the generated pages' shell and body typography |
| `css/themes.css` | One `[data-theme=…]` variable block per theme |
| `js/site-data.js` | The site graph: every node's id, parent, label, icon, href, canvas position, summary |
| `js/theme.js` | The theme registry and manager (`KIS.THEMES`, `KIS.theme`) |
| `js/components.js` | The component factories |
| `js/mindmap.js` | Home-page expand/collapse and animated connector drawing |
| `js/page.js` | Shared renderer for every node page |
| `js/doc-page.js` | Shared renderer for every generated document page |

## Generated document pages

`build-pages.py` renders the project's markdown into pages of this site — same themes, same components, same tokens. Two functions, usable separately:

| Function | What it does |
|---|---|
| `build_page(rel)` | One markdown file → one page in `pages/`, with an id and a permalink on every heading |
| `traverse(entries)` | Builds a page for every markdown file under `documents/` |

A leaf node with a folder of its own opens that folder's document:

| Node | Source | Page |
|---|---|---|
| Knowledge Base | `documents/KnowledgeBase/README.md` | `pages/documents-knowledgebase-readme.html` |
| Requirements | `documents/Requirements/README.md` | `pages/documents-requirements-readme.html` |
| Plans | `documents/Plan/milestone1_high_level_plan.md` | `pages/documents-plan-milestone1-high-level-plan.html` |
| Proposals | `documents/Proposals/README.md` | `pages/documents-proposals-readme.html` |
| Module Design | `documents/Design/README.md` | `pages/documents-design-readme.html` |

The same pairs are `ENTRIES` in `build-pages.py` and the `href`/`source` pair on the node in `js/site-data.js`; change one and you must change the other. The `href` filename is `page_name(source)` — derived, never chosen by hand.

A node with no folder of its own — System Design, Acceptance Evidence, Test Guide — still gets a page: a mount point `js/page.js` fills from the site graph at load time. Those are generated from `js/site-data.js` too, which is what makes `pages/` derived in full and safe to gitignore. Adding a node to the graph is enough; no file is written by hand.

`--clean` deletes every page carrying this tool's `<meta name="generator">` mark, which is now all of them, so a page whose source document was deleted goes with it.

### Everything lands flat in `pages/`

A flat folder makes every link between two generated pages a plain same-level `href`, with no relative depth to compute and nothing to break when a document moves inside the repo. Flat also needs unique names, so **a page is named for its whole repo path**, not its basename: `documents/Design/README.md` → `documents-design-readme.html`. There are five `README.md` files in this repo and they cannot all be `README.html`. The name is a pure function of the path, so a rebuild never renumbers anything.

### The traversal, and where it stops

`CRAWL_ROOTS` bounds it to `documents/`. **Markdown under there becomes a page; markdown anywhere else is linked where it lives**, exactly as a schema or a diagram source is. Without that bound the crawl followed links wherever they led and built 120 pages — deprecated HLDs, phase plans, the agent process rules — and reported *their* stale links as if the site were responsible for them. A link that leaves `documents/` is a reference to the repository, not another page.

1. Seed the queue with each entry document, tagged with the site node that opens it.
2. Take a document off the queue and render it. Rendering rewrites its links, and every markdown link inside `documents/` is enqueued at that moment with the same tag — so a document inherits the breadcrumb of whichever entry reached it first.
3. Drain, then sweep `documents/` for anything not yet built and drain again.

`seen` is checked when a document is *enqueued*, not when it is rendered, so a document linked from twenty pages is queued once. That is also what terminates the walk on the cycles these documents are full of — an HLD links its decision record, which links back to the HLD.

The sweep in step 3 is why the count matches the folder rather than what happens to be linked. Reachability alone leaves a document invisible the moment the page that linked it is deleted — which is exactly how `Delivery/README.md` fell out of the site when `documents/README.md` was removed.

### What happens to each link target

| Outcome | Applies to | Becomes |
|---|---|---|
| **page** | a markdown file | a sibling in `pages/` — a plain same-level `href` |
| **asset** | an image the page renders inline | copied into `pages/` under the same naming scheme |
| **file** | anything else — decks, PDFs, schemas, source files | a relative link to where it already lives in the repo |
| **dead** | nothing at that path | the anchor loses its `href` and renders as struck-through muted text, so a broken link looks broken instead of 404ing when clicked |

Only images are copied, because only images have to be *inside* the site to render. Everything else is linked in place — copying it would make `pages/` a second, staler copy of the repository. **This is what fixes the web root at the repo rather than at this folder** (§ Run it): a link to a deck reaches out of `website/`, so `website/` cannot be the root.

`presentation/` is never re-rendered. Its decks are Marp markdown with an HTML export already built beside them by `slide-build-tool/build-slides.py`; a link to a deck's `.md` is retargeted to that export, and neither file is touched.

The build prints every unresolved link with its reason. What shows up there is stale links in the source markdown — a document pointing at a file that has since moved or been deleted — and those are fixed in the markdown, never in the builder.

## Reusable components

All markup is produced by factories in `js/components.js`; pages and the mind map compose them. Every factory takes one props object with the **common property set**:

| Prop | Meaning |
|---|---|
| `label` | visible text |
| `image` | icon id under `assets/icons/`, or a logo asset path |
| `href` | the page the component links to (opened in a new tab) |
| `style` | visual variant, mapped to a CSS modifier class (`PageLink--node`, `PageLink--card`) |
| `animation` | `{ enter: 'pop'|'rise'|'fade'|'none', delay: ms }` |

Components: `createPageLink` (the icon + text node), `createLogoFrame` (the logo in its rounded rectangle), `createTableOfContents`, `createThemeSwitch`, `mountTopBar`.

`createTableOfContents({ scope, label, minLevel, maxLevel, minItems, spy })` indexes the headings inside `scope` and returns a nested `<nav>`, with the entry for the section currently on screen marked `is-active`. It reads the DOM rather than taking a list of headings, so any page can hand it a region and get an index — a generated document body, a hand-written page, or a region another component built. Headings with no `id` are skipped, since an entry that cannot be linked to is not an entry, and a section permalink is stripped from the entry's label rather than dragged into it. Below `minItems` headings it returns `null` and the caller mounts nothing. The home page mounts none: a mind map has no sections to index.

CSS classes mirror the components one-to-one — `PageLink`, `LogoFrame`, `NodeCanvas`, `ConnectorLayer`, `CardGrid`, `PageHeader`, `TableOfContents`, `ThemeSwitch` — with BEM-style `__element` / `--modifier` names. Components read design tokens (`--accent`, `--node-bg`, …) and never hardcode a color. Icons are applied as CSS masks (`--icon-url`), so one SVG file recolors under every theme.

## Theme management

A theme is one name mapping to three things, and the two built-in themes (`sketch`, `blueprint`) are the worked examples:

1. **a CSS variable set** — a `[data-theme="name"]` block in `css/themes.css`;
2. **an asset set** — `KIS.THEMES[name].assets` (which logo file, which icon directory);
3. **an animation set** — `KIS.THEMES[name].animation` (connector draw duration/easing/dash, node enter animation and stagger).

`KIS.theme.apply(name)` stamps `data-theme` on the root, swaps every mounted logo, persists the choice, and emits `kis:theme` (the mind map listens and redraws its connectors). To add a theme, add the `KIS.THEMES` entry and the matching `themes.css` block — nothing else changes.

**One thing a theme may not change: the typeface on a generated document page.** `--font-ui` is a theming token — sketch swaps it to handwriting — but `css/doc.css` gives those pages `--font-doc` and `--font-doc-mono` instead, which `base.css` defines once and no `[data-theme]` block overrides. A long technical page reflows into a different shape when the face changes under it, so its headings land in different places and every measurement a reader took of the page is wrong. The theme still recolors these pages; only their text metrics are held still.
