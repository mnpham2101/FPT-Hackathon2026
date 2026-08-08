"""KIS site — markdown -> HTML generator.

Renders a markdown file into a page of the KIS site: same themes, same
components, same CSS tokens.  Run it from the repo root:

    python tools/website-builder/build-pages.py documents/Design/README.md

Every heading comes out with an id and a permalink beside it, so the page's
table of contents can address each section and a reader can link to one.

Pages land flat in pages/, beside the hand-written node pages.  A flat
folder is what makes every link between two generated pages a plain
same-level href, with no relative depth to compute and nothing to break when
a document moves inside the repo.  Flat also needs unique names, so a page
is named for its whole repo path (§ page_name) rather than its basename --
there are five README.md files in this repo and they cannot all be
README.html.
"""

import argparse
import html
import os
import re
import shutil
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import quote, unquote

try:
    import markdown
except ImportError:
    sys.exit("python-markdown is required:  pip install -r tools/website-builder/requirements.txt")


SITE = Path(__file__).resolve().parent
ROOT = SITE.parents[1]
PAGES = SITE / "pages"

# Copied into pages/ so the page can render them.  Anything else a document
# links to is linked where it already lives (§ resolve_target).
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

# Stamped into every generated page.  pages/ also holds the hand-written node
# pages, so anything that sweeps this folder has to test for a mark this tool
# wrote rather than infer one from the markup.
GENERATOR = "kis-build-pages"

ATTR_RE = re.compile(r'\b(href|src)\s*=\s*"([^"]*)"')
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


# --- rendering ------------------------------------------------------------

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


def resolve_target(source_rel, raw):
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
        # A deck is represented by the export already built beside it, and
        # neither the source nor the export is touched.
        if full.suffix == ".md" and full.with_suffix(".html").is_file():
            rel = repo_rel(full.with_suffix(".html"))
        return out_of_pages(rel), "file"

    if full.suffix.lower() == ".md":
        return page_name(rel), "page"

    if full.suffix.lower() in INLINE_SUFFIXES:
        shutil.copy2(full, PAGES / asset_name(rel))
        return asset_name(rel), "asset"

    return out_of_pages(rel), "file"


def rewrite_links(body_html, source_rel, dead=None):
    """Point every relative href/src at what the build made of it."""

    def sub(m):
        attr, target = m.group(1), m.group(2)
        if not target or target.startswith("#") or target.startswith(SKIP_SCHEMES):
            return m.group(0)

        path, tail = split_target(target)
        if not path:
            return m.group(0)

        href, kind = resolve_target(source_rel, path)
        if href is None:
            if dead is not None:
                dead.append((str(source_rel), target, kind))
            # An <img> keeps its src so the gap shows; an <a> loses its href,
            # which is what renders it as muted, unclickable text.
            if attr == "src":
                return m.group(0)
            return 'data-missing="%s"' % html.escape(target, quote=True)

        return '%s="%s%s"' % (attr, href, tail)

    return ATTR_RE.sub(sub, body_html)


def build_page(rel, node="", dead=None):
    """Render one repo-relative markdown file into pages/."""
    src = ROOT / str(rel)
    body = render_markdown(src.read_text(encoding="utf-8"))
    body = rewrite_links(body, rel, dead)
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


# --- driver ---------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("source", help="the markdown file to render")
    args = ap.parse_args()

    PAGES.mkdir(parents=True, exist_ok=True)
    rel = repo_rel(Path(args.source))
    if rel is None or not (ROOT / str(rel)).is_file():
        sys.exit("not a file in this repository: %s" % args.source)

    dead = []
    dest = build_page(rel, dead=dead)
    print("built   %s  ->  pages/%s" % (rel, dest.name))
    if dead:
        print("\nunresolved links (%d) — rendered as muted text:" % len(dead))
        for source, target, why in sorted(set(dead)):
            print("  %-46s %s" % (target, why))


if __name__ == "__main__":
    main()
