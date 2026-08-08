"""KIS site — markdown -> HTML generator.

Renders the project's markdown into pages of the KIS site: same themes, same
components, same CSS tokens.  Two functions, usable separately:

    build_page(rel)      one markdown file -> one page in pages/
    traverse(entries)    follow the links out of the entry documents and
                         build a page for every markdown they reach

Run either from the repo root:

    python website/build-pages.py documents/Design/README.md
    python website/build-pages.py                  # traverse
    python website/build-pages.py --clean          # + drop old

Everything lands flat in pages/, beside the hand-written node pages.  A flat
folder is what makes every link between two generated pages a plain
same-level href, with no relative depth to compute and nothing to break when
a document moves inside the repo.  Flat also needs unique names, so a page is
named for its whole repo path (§ page_name) rather than its basename -- there
are five README.md files in this repo and they cannot all be README.html.
"""

import argparse
import html
import os
import re
import shutil
import sys
from collections import deque
from pathlib import Path, PurePosixPath
from urllib.parse import quote, unquote

try:
    import markdown
except ImportError:
    sys.exit("python-markdown is required:  pip install -r website/requirements.txt")


SITE = Path(__file__).resolve().parent
ROOT = SITE.parents[0]
PAGES = SITE / "pages"

# One entry per KIS site leaf node (js/site-data.js `id`) that opens a
# document.  The source is the folder's index -- README.md where the folder
# has one, and the folder's single document where it does not.
ENTRIES = [
    ("knowledge-base", "documents/KnowledgeBase/README.md"),
    ("requirements", "documents/Requirements/README.md"),
    ("plans", "documents/Plan/milestone1_high_level_plan.md"),
    ("proposal-presentation", "documents/Proposals/README.md"),
    ("module-design", "documents/Design/README.md"),
]

# Copied into pages/ so the page can render them.  Anything else a document
# links to is linked where it already lives (§ classify).
INLINE_SUFFIXES = {".svg", ".png", ".jpg", ".jpeg", ".gif", ".webp"}

# Never re-rendered: presentation/ decks are Marp markdown whose HTML export
# is already built beside them by slide-build-tool/build-slides.py.
PRESERVE_PREFIXES = ("presentation/",)

MD_EXTENSIONS = [
    "extra",         # tables, fenced code, attr_list, def_list, footnotes, md_in_html
    "sane_lists",
    "toc",           # an id on every heading, and the permalink beside it
]
MD_CONFIG = {
    # anchorlink puts the whole heading inside an <a>, which swallows the
    # heading text into a link; permalink appends a separate glyph instead,
    # so the text stays text and the section is still addressable.
    "toc": {"anchorlink": False, "permalink": "¶", "permalink_title": "Link to this section"},
}

# Stamped into every generated page, and the only thing --clean matches on.
# pages/ also holds the hand-written node pages, and deleting one of those
# would destroy a file no build can put back -- so the test has to be an
# explicit mark this tool wrote, not an inference from the page's markup.
GENERATOR = "kis-build-pages"

ATTR_RE = re.compile(r'\b(href|src)\s*=\s*"([^"]*)"')
# url(...) inside a <style> block -- how a deck names its background images,
# which an href/src scan alone would miss and leave out of a bundle.
CSS_URL_RE = re.compile(r"""url\(\s*['"]?([^'")]+)""")
SKIP_SCHEMES = ("http://", "https://", "mailto:", "tel:", "data:", "//")


# --- naming ---------------------------------------------------------------


def page_name(rel):
    """Repo path -> the flat filename it takes in pages/.

    'documents/Design/README.md' -> 'documents-design-readme.html'

    Named for the whole path, not the basename: pages/ is flat, and this
    repo has five README.md files that would otherwise collide into one.
    The name is a pure function of the path, so a rebuild never renumbers
    anything and a link written in one run is still valid in the next."""
    p = PurePosixPath(str(rel))
    slug = "-".join(list(p.parent.parts) + [p.stem]).lower()
    return re.sub(r"[^a-z0-9]+", "-", slug).strip("-") + ".html"


def asset_name(rel):
    """Same scheme, for an image copied into pages/ beside the pages."""
    p = PurePosixPath(str(rel))
    slug = "-".join(list(p.parent.parts) + [p.stem]).lower()
    return re.sub(r"[^a-z0-9]+", "-", slug).strip("-") + p.suffix.lower()


def repo_rel(path: Path):
    """POSIX repo-relative path, or None if `path` escaped the repo."""
    try:
        return PurePosixPath(path.resolve().relative_to(ROOT).as_posix())
    except ValueError:
        return None


def out_of_pages(rel):
    """Relative href from pages/ to a repo file left where it lives."""
    return quote(os.path.relpath(ROOT / str(rel), PAGES).replace(os.sep, "/"), safe="/")


def split_target(target):
    """'a/b.md#sec' -> ('a/b.md', '#sec').  A query rides with the tail."""
    for sep in ("#", "?"):
        i = target.find(sep)
        if i != -1:
            return target[:i], target[i:]
    return target, ""


# =========================================================================
#  1 · build_page — one markdown file -> one page
# =========================================================================

PAGE = """<!doctype html>
<html lang="en" data-theme="sketch">
<head>
  <meta charset="utf-8">
  <meta name="generator" content="{marker}">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} — KIS</title>
  <link rel="stylesheet" href="../css/base.css">
  <link rel="stylesheet" href="../css/components.css">
  <link rel="stylesheet" href="../css/doc.css">
  <link rel="stylesheet" href="../css/themes.css">
</head>
<body class="DocBody" data-root=".." data-doc-node="{node}" data-doc-title="{title}" data-doc-source="{source}">
  <div class="DocLayout">
    <aside class="DocLayout__aside" id="doc-toc"></aside>
    <main class="Page DocPage">
      <header class="PageHeader" id="doc-header"></header>
      <article class="Markdown" id="doc-body">
{body}
      </article>
    </main>
  </div>
  <script src="../js/site-data.js"></script>
  <script src="../js/theme.js"></script>
  <script src="../js/components.js"></script>
  <script src="../js/doc-page.js"></script>
</body>
</html>
"""


def render_markdown(text):
    """Markdown -> body HTML, with an id and a permalink on every heading."""
    md = markdown.Markdown(extensions=MD_EXTENSIONS, extension_configs=MD_CONFIG)
    out = md.convert(text)
    # The wide requirement and component tables in these documents scroll
    # inside their own box rather than widening the page.
    out = out.replace("<table>", '<div class="Markdown__scroll"><table>')
    return out.replace("</table>", "</table></div>")


def doc_title(body_html, rel):
    m = re.search(r"<h1[^>]*>(.*?)</h1>", body_html, re.S)
    if m:
        text = re.sub(r'<a[^>]*class="headerlink".*?</a>', "", m.group(1), flags=re.S)
        return html.unescape(re.sub(r"<[^>]+>", "", text)).strip()
    return PurePosixPath(str(rel)).stem.replace("-", " ").replace("_", " ")


def build_page(rel, node="", build=None):
    """Render one repo-relative markdown file into pages/.

    `build` is the traversal's state (§ 2) when this runs as part of a crawl;
    on its own it is None, links are still rewritten, and any markdown they
    point at simply is not built."""
    src = ROOT / str(rel)
    body = render_markdown(src.read_text(encoding="utf-8"))
    body = rewrite_links(body, rel, build, node)
    title = doc_title(body, rel)

    dest = PAGES / page_name(rel)
    dest.write_text(
        PAGE.format(
            marker=GENERATOR,
            title=html.escape(title, quote=True),
            node=html.escape(node, quote=True),
            source=html.escape(str(rel), quote=True),
            body=body,
        ),
        encoding="utf-8",
    )
    return dest


# =========================================================================
#  2 · traverse — follow the links and build every document they reach
# =========================================================================


class Build:
    """The traversal's state: what is queued, what is done, what did not
    resolve.  Held in one object so build_page() can call back into
    classify() while rendering, which is what makes the crawl a single pass
    -- a link is discovered and scheduled at the moment it is rewritten."""

    def __init__(self):
        self.queue = deque()     # (rel, node) markdown still to build
        self.seen = set()        # repo paths already queued or handled
        self.built = []          # markdown rendered into pages/
        self.assets = []         # images copied into pages/
        self.dead = []           # (source, target, why)

    def enqueue_markdown(self, rel, node):
        key = str(rel)
        if key not in self.seen:
            self.seen.add(key)
            self.queue.append((rel, node))

    def copy_asset(self, rel):
        key = str(rel)
        if key not in self.seen:
            self.seen.add(key)
            shutil.copy2(ROOT / str(rel), PAGES / asset_name(rel))
            self.assets.append(key)

    def classify(self, source_rel, raw, node):
        """Resolve one link target.  Returns (href, kind) or (None, reason).

        Four outcomes, and which one applies is decided here alone:

          page    markdown  -> a sibling page in pages/, plain same-level href
          asset   an image  -> copied into pages/, plain same-level href
          file    anything else -> linked where it already lives, so the site
                  does not become a second copy of the repository
          dead    nothing there -> the caller strips the href
        """
        # Written links are percent-encoded ('apk%20uploader'); the filesystem
        # wants the decoded name.
        target = (ROOT / str(source_rel)).parent / unquote(raw)
        rel = repo_rel(target)
        if rel is None:
            return None, "outside the repository"

        full = ROOT / str(rel)

        # A folder link resolves to its README, the file a reader would open.
        if full.is_dir():
            index = full / "README.md"
            if not index.is_file():
                return out_of_pages(rel) + "/", "file"
            rel, full = repo_rel(index), index

        if not full.is_file():
            return None, "no such file"

        if str(rel).startswith(PRESERVE_PREFIXES):
            # A deck is represented by the export already built beside it,
            # and neither the source nor the export is touched.
            if full.suffix == ".md" and full.with_suffix(".html").is_file():
                rel = repo_rel(full.with_suffix(".html"))
            return out_of_pages(rel), "file"

        if full.suffix.lower() == ".md":
            self.enqueue_markdown(rel, node)
            return page_name(rel), "page"

        if full.suffix.lower() in INLINE_SUFFIXES:
            self.copy_asset(rel)
            return asset_name(rel), "asset"

        return out_of_pages(rel), "file"


def rewrite_links(body_html, source_rel, build, node=""):
    """Point every relative href/src at what the build made of it.

    `node` is the site node this document sits under; it is passed on to
    every markdown link discovered here, which is how a document reached by
    the crawl inherits its breadcrumb from the entry that found it."""

    def sub(m):
        attr, target = m.group(1), m.group(2)
        if not target or target.startswith("#") or target.startswith(SKIP_SCHEMES):
            return m.group(0)

        path, tail = split_target(target)
        if not path:
            return m.group(0)

        if build is None:
            return m.group(0)

        href, kind = build.classify(source_rel, path, node)
        if href is None:
            build.dead.append((str(source_rel), target, kind))
            # An <img> keeps its src so the gap shows; an <a> loses its href,
            # which is what renders it as muted, unclickable text.
            if attr == "src":
                return m.group(0)
            return 'data-missing="%s"' % html.escape(target, quote=True)

        return '%s="%s%s"' % (attr, href, tail)

    return ATTR_RE.sub(sub, body_html)


def traverse(entries):
    """Breadth-first over the link graph, starting at the entry documents.

    The algorithm:

      1. Seed the queue with each entry document, tagged with the site node
         that opens it.
      2. Take a document off the queue and render it (§ 1).  Rendering
         rewrites its links, and every markdown link it rewrites is enqueued
         at that moment, tagged with the same node -- so a document inherits
         the breadcrumb of whichever entry reached it first.
      3. Repeat until the queue empties.

    `seen` is checked at enqueue time, not at render time, so a document
    linked from twenty pages is queued once.  That is also what terminates
    the walk on the cycles these documents are full of -- an HLD links its
    decision record, which links back to the HLD."""
    build = Build()
    for node, source in entries:
        if not (ROOT / source).is_file():
            sys.exit("entry source missing: %s (node '%s')" % (source, node))
        build.enqueue_markdown(PurePosixPath(source), node)

    while build.queue:
        rel, node = build.queue.popleft()
        build_page(rel, node, build)
        build.built.append(str(rel))

    return build


# =========================================================================
#  3 · bundle — a copy of the site that opens with no server
# =========================================================================

# Copied out of website/ into a bundle.  The generator, its requirements and
# the asset scripts are how the site is produced, not part of it.
SITE_RUNTIME = ("index.html", "css", "js", "assets", "pages")


def reachable_outside(site_root: Path):
    """Every repo file the built pages reach outside website/, transitively.

    Transitively matters: a page links a deck, and the deck needs its own
    backgrounds and diagrams. Following only one hop produces a bundle whose
    decks open with every image missing."""
    found, frontier = set(), set()

    def links_of(f: Path):
        txt = f.read_text(encoding="utf-8", errors="replace")
        out = set(ATTR_RE.findall(txt))
        return {t for _, t in out} | set(CSS_URL_RE.findall(txt))

    def collect(f: Path, targets):
        for raw in targets:
            if not raw or raw.startswith("#") or raw.startswith(SKIP_SCHEMES):
                continue
            p = (f.parent / unquote(split_target(raw)[0])).resolve()
            if p.is_file() and site_root not in p.parents and repo_rel(p):
                if p not in found:
                    found.add(p)
                    frontier.add(p)

    for page in (site_root / "pages").glob("*.html"):
        collect(page, links_of(page))
    while frontier:
        f = frontier.pop()
        if f.suffix.lower() in (".html", ".htm"):
            collect(f, links_of(f))
    return found


def retarget_markdown_links(text, deck: Path, dest: Path, dest_root: Path):
    """In a bundled deck, point a link that names a .md at the page built
    from it.

    The deck in presentation/ is never touched -- this rewrites the bundle's
    own copy, which is a derived artifact exactly as the export itself is.
    Without it a reader who clicks 'IVI ECU high-level design' inside a deck
    gets raw markdown rendered as plain text."""
    def sub(m):
        attr, target = m.group(1), m.group(2)
        if not target or target.startswith("#") or target.startswith(SKIP_SCHEMES):
            return m.group(0)
        path, tail = split_target(target)
        src = (deck.parent / unquote(path)).resolve()
        if src.suffix.lower() != ".md" or not src.is_file():
            return m.group(0)
        rel = repo_rel(src)
        if rel is None or not (PAGES / page_name(rel)).is_file():
            return m.group(0)
        # Anchored at the bundle root, never derived from the deck's own
        # depth: presentation/ holds decks at two nesting levels, and a
        # ../.. guessed from one of them is wrong for the other.
        page = dest_root / "website" / "pages" / page_name(rel)
        return '%s="%s%s"' % (attr, href_of(dest, page), tail)

    return ATTR_RE.sub(sub, text)


def href_of(from_file: Path, to_file: Path):
    return quote(os.path.relpath(to_file, from_file.parent).replace(os.sep, "/"), safe="/")


def bundle(dest_root: Path):
    """Write a self-contained copy of the site that needs no web server.

    The bundle mirrors the repository's own layout for every file the site
    reaches, so nothing is rewritten and nothing has to be: a deck's
    ../assets/ still resolves, and a page's ../../presentation/ still
    resolves, because their relative positions are unchanged. The repo's
    root index.html rides along as the bundle's entry point, which is what
    makes <bundle>/index.html open the site on a double-click."""
    # Best effort: a cloud-synced checkout can keep a directory handle open
    # after its files are gone, so the tree may not vanish completely. Every
    # write below overwrites by name and tolerates a directory that survived,
    # which is why a partial delete is not a failure.
    if dest_root.exists():
        shutil.rmtree(dest_root, ignore_errors=True)

    for name in SITE_RUNTIME:
        src = SITE / name
        out = dest_root / "website" / name
        out.parent.mkdir(parents=True, exist_ok=True)
        if src.is_dir():
            shutil.copytree(src, out, dirs_exist_ok=True)
        else:
            shutil.copy2(src, out)

    outside = reachable_outside(SITE)
    for src in sorted(outside):
        rel = repo_rel(src)
        out = dest_root / str(rel)
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, out)
        if str(rel).startswith(PRESERVE_PREFIXES) and src.suffix.lower() == ".html":
            out.write_text(
                retarget_markdown_links(
                    src.read_text(encoding="utf-8", errors="replace"), src, out, dest_root),
                encoding="utf-8",
            )

    entry = ROOT / "index.html"
    if entry.is_file():
        shutil.copy2(entry, dest_root / "index.html")

    total = sum(f.stat().st_size for f in dest_root.rglob("*") if f.is_file())
    count = sum(1 for f in dest_root.rglob("*") if f.is_file())
    return count, total, len(outside)


# --- driver ---------------------------------------------------------------


def generated_pages():
    """Pages this tool owns: those carrying the GENERATOR meta tag.  The
    hand-written node pages live in the same folder and must survive
    --clean, so the whole file is searched rather than a prefix of it -- a
    marker missed by a length cutoff leaves a deleted document's page behind
    for ever, which is the one failure --clean exists to prevent."""
    for f in PAGES.glob("*.html"):
        if GENERATOR in f.read_text(encoding="utf-8", errors="replace"):
            yield f


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("source", nargs="?", help="one markdown file to render; omit to traverse")
    ap.add_argument("--clean", action="store_true", help="remove previously generated pages first")
    ap.add_argument("--bundle", metavar="DIR", nargs="?", const="dist",
                    help="also write a self-contained copy to DIR (default: dist/)")
    args = ap.parse_args()

    PAGES.mkdir(parents=True, exist_ok=True)
    if args.clean:
        for f in list(generated_pages()):
            f.unlink()

    if args.source:
        rel = repo_rel(Path(args.source))
        if rel is None or not (ROOT / str(rel)).is_file():
            sys.exit("not a file in this repository: %s" % args.source)
        dest = build_page(rel, build=Build())
        print("built   %s  ->  pages/%s" % (rel, dest.name))
        return

    build = traverse(ENTRIES)
    print("pages   %d" % len(build.built))
    print("assets  %d" % len(build.assets))
    if args.bundle:
        dest = (ROOT / args.bundle).resolve()
        count, total, pulled = bundle(dest)
        print("bundle  %d files, %.1f MB, %d pulled in from the repo -> %s/index.html"
              % (count, total / 1e6, pulled, dest.relative_to(ROOT).as_posix()))
    print("output  %s" % PAGES.relative_to(ROOT).as_posix())
    if build.dead:
        seen = sorted(set(build.dead))
        print("\nunresolved links (%d) — rendered as muted text:" % len(seen))
        for source, target, why in seen:
            print("  %-56s %-46s %s" % (source, target, why))


if __name__ == "__main__":
    main()
