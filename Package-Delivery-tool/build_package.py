#!/usr/bin/env python3
"""Build the outgoing delivery package: Hackathon-Delivery/ and Hackathon-Delivery.zip,
both written inside Package-Delivery-tool/.

The package is documentation only: the landing page, the presentations it links,
the self-contained wiki, and the demo video. Code is never copied in — the
package points at the GitHub repository instead, and every wiki or deck link
into a code folder is rewritten to the GitHub copy.

Steps, in order:
  1. Rebuild every deck HTML from its markdown source (presentation/**/*-deck.md),
     so the decks the landing page and the wiki link are current.
  2. Rebuild the wiki and write its self-contained bundle into Hackathon-Delivery/
     (python website/build-pages.py --bundle) — the bundle mirrors the repo layout
     and pulls in every deck and asset the site references, so it opens from disk
     with no server.
  3. Overlay the package extras: the landing page (delivery-index.html beside this
     script) as index.html, and video-evidence/system-test.mp4 as
     video/system-test.mp4.
  4. Prune every code tree the wiki crawl pulled in (CODE_DIRS), then rewrite
     each shipped HTML link into those trees to the matching GitHub URL.
  5. Zip the result to Hackathon-Delivery.zip beside it.

Usage, from the repo root:
    python Package-Delivery-tool/build_package.py
"""

import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
OUT = HERE / "Hackathon-Delivery"
ZIP = HERE / "Hackathon-Delivery.zip"
LANDING = HERE / "delivery-index.html"
VIDEO = REPO / "video-evidence" / "system-test.mp4"
SLIDE_BUILDER = REPO / "presentation" / "slide-build-tool" / "build-slides.py"
SITE_BUILDER = REPO / "website" / "build-pages.py"

GITHUB_URL = "https://github.com/mnpham2101/FPT-Hackathon2026"

# Paths the landing page links; the build fails if any is absent from the package.
LANDING_TARGETS = (
    "video/system-test.mp4",
    "presentation/m1-business-delivery/m1-business-delivery-deck.html",
    "presentation/phase6-systemIntegration/phase6-system-delivery-deck.html",
    "presentation/system-design/system-design-deck.html",
    "website/index.html",
)

# Code ships from GitHub only. The wiki crawl pulls in whatever files its pages
# link, so these top-level trees are pruned from the bundle after it is written,
# and every shipped link into them is rewritten to the GitHub copy. A partial
# code copy reads as a broken codebase; a link to the repository does not.
CODE_DIRS = ("ADA_ECU", "IVI_ECU", "Scenario_Player", "V2X_ECU", "contracts", "tools")

# href="../../V2X_ECU/main.cpp" and any other (../)*<code-dir>/<path> form,
# in href or src, as the wiki pages and deck exports write them.
CODE_LINK = re.compile(
    r'(?P<attr>href|src)="(?:\.\./)*(?P<rel>(?:' + "|".join(CODE_DIRS) + r')(?:/[^"]*)?)"'
)


def run(cmd, what):
    print(f"--- {what}")
    r = subprocess.run([sys.executable, *map(str, cmd)], cwd=REPO)
    if r.returncode != 0:
        sys.exit(f"FAILED ({what}): {' '.join(map(str, cmd))}")


def build_decks():
    decks = sorted(REPO.glob("presentation/**/*-deck.md"))
    if not decks:
        sys.exit("no deck sources found under presentation/")
    for md in decks:
        run([SLIDE_BUILDER, md], f"deck {md.relative_to(REPO)}")


def build_bundle():
    run([SITE_BUILDER, "--clean", "--bundle", str(OUT)], "wiki bundle")


def overlay():
    if not VIDEO.is_file():
        sys.exit(f"demo video missing: {VIDEO}")
    shutil.copy2(LANDING, OUT / "index.html")
    dest = OUT / "video" / "system-test.mp4"
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(VIDEO, dest)
    print("--- overlay: landing page + demo video")


MD_IMAGE = re.compile(r"!\[[^\]]*\]\(([^)#\s]+)")

# The demo video ships exactly once, at this package path. Documents link its
# repo path (video-evidence/…, git-ignored — too large for the git history);
# every shipped copy of such a link is repointed at the packaged file.
VIDEO_PKG_REL = "video/system-test.mp4"
VIDEO_HTML_LINK = re.compile(r'(?P<attr>href|src)="(?:\.\./)*video-evidence/system-test\.mp4"')
VIDEO_MD_LINK = re.compile(r"\]\((?:\.\./)*video-evidence/system-test\.mp4\)")


def rewrite_video_links():
    dup = OUT / "video-evidence"
    if dup.exists():
        shutil.rmtree(dup)
    target = OUT / VIDEO_PKG_REL
    links = pages = 0
    for page in sorted(OUT.rglob("*")):
        if not page.is_file() or page.suffix.lower() not in (".html", ".md"):
            continue
        rel = Path(os.path.relpath(target, page.parent)).as_posix()
        text = page.read_text(encoding="utf-8")
        new, a = VIDEO_HTML_LINK.subn(lambda m: f'{m.group("attr")}="{rel}"', text)
        new, b = VIDEO_MD_LINK.subn(f"]({rel})", new)
        if a + b:
            page.write_text(new, encoding="utf-8")
            links += a + b
            pages += 1
    print(f"--- rewrote {links} video links across {pages} pages")


def overlay_md_media():
    """Raw markdown ships beside its wiki render (the final report, the HLDs,
    the guides). Each must open standalone, so every relative image a shipped
    .md embeds is copied from the repo to the same path in the package."""
    count = 0
    for md in sorted(OUT.rglob("*.md")):
        rel_md = md.relative_to(OUT)
        for ref in MD_IMAGE.findall(md.read_text(encoding="utf-8")):
            if ref.startswith(("http://", "https://", "data:")):
                continue
            target = md.parent / ref
            if target.is_file():
                continue
            src = (REPO / rel_md).parent / ref
            if not src.is_file():
                continue  # verify() reports it with the md that embeds it
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
            count += 1
    print(f"--- overlay: {count} images embedded by shipped markdown")


def prune_code():
    removed = 0
    for d in CODE_DIRS:
        tree = OUT / d
        if tree.exists():
            removed += sum(1 for f in tree.rglob("*") if f.is_file())
            shutil.rmtree(tree)
    print(f"--- pruned {removed} code files; code ships from GitHub only")


def rewrite_code_links():
    """Point every shipped link into a pruned code tree at the GitHub copy.
    A trailing slash names a folder (GitHub tree view); anything else a file
    (blob view)."""
    changed_links = 0
    changed_pages = 0
    for page in sorted(OUT.rglob("*.html")):
        text = page.read_text(encoding="utf-8")

        def to_github(m):
            rel = m.group("rel")
            if rel.endswith("/"):
                url = f"{GITHUB_URL}/tree/main/{rel.rstrip('/')}"
            else:
                url = f"{GITHUB_URL}/blob/main/{rel}"
            return f'{m.group("attr")}="{url}"'

        new, n = CODE_LINK.subn(to_github, text)
        if n:
            page.write_text(new, encoding="utf-8")
            changed_links += n
            changed_pages += 1
    print(f"--- rewrote {changed_links} code links to GitHub across {changed_pages} pages")


# Internal-only paths that must never ship. The bundler pulls in whatever the
# pages link, so one of these appearing means a document links it — unlink at
# the markdown source (precedent: commit 6dce4c6), never by deleting here.
FORBIDDEN = (".claude", ".github", "CLAUDE.md")


def verify():
    missing = [t for t in LANDING_TARGETS if not (OUT / t).is_file()]
    if missing:
        sys.exit("landing-page links with no file in the package:\n  " + "\n  ".join(missing))
    shipped = [p for p in FORBIDDEN if (OUT / p).exists()]
    if shipped:
        sys.exit(
            "internal paths pulled into the package: " + ", ".join(shipped)
            + "\nsome shipped page still links them — find it with:"
            + "\n  grep -rlE '\\.claude/|\\.github/|CLAUDE\\.md' --include=*.html " + str(OUT)
            + "\nthen unlink at the markdown source and rebuild."
        )
    leftover = [d for d in CODE_DIRS if (OUT / d).exists()]
    if leftover:
        sys.exit("code trees still present after pruning: " + ", ".join(leftover))
    # Every shipped raw markdown must render standalone: each relative image
    # it embeds must exist beside it in the package.
    broken = []
    for md in sorted(OUT.rglob("*.md")):
        for ref in MD_IMAGE.findall(md.read_text(encoding="utf-8")):
            if ref.startswith(("http://", "https://", "data:")):
                continue
            if not (md.parent / ref).is_file():
                broken.append(f"{md.relative_to(OUT)} -> {ref}")
    if broken:
        sys.exit("shipped markdown embeds images absent from the package:\n  " + "\n  ".join(broken))
    stray = [str(p.relative_to(OUT)) for p in OUT.rglob("*.mp4")
             if p.relative_to(OUT).as_posix() != VIDEO_PKG_REL]
    if stray:
        sys.exit("video copies outside " + VIDEO_PKG_REL + ": " + ", ".join(stray))
    print("--- verified: landing-page links resolve, no internal paths, no code shipped, markdown images whole, one video copy")


def make_zip():
    if ZIP.exists():
        ZIP.unlink()
    files = sorted(f for f in OUT.rglob("*") if f.is_file())
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in files:
            zf.write(f, Path(OUT.name) / f.relative_to(OUT))
    print(f"--- {ZIP.name}: {len(files)} files, {ZIP.stat().st_size / 1e6:.1f} MB")


def main():
    # OUT is wholly derived; a stale tree would carry files an earlier build
    # shipped and this one no longer does.
    if OUT.exists():
        shutil.rmtree(OUT)
    build_decks()
    build_bundle()
    overlay()
    prune_code()
    overlay_md_media()
    rewrite_code_links()
    rewrite_video_links()
    verify()
    make_zip()
    print(f"done -> {OUT} and {ZIP}")


if __name__ == "__main__":
    main()
