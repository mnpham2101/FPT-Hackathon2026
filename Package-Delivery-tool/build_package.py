#!/usr/bin/env python3
"""Build the outgoing delivery package: Hackathon-Delivery/ and Hackathon-Delivery.zip,
both written inside Package-Delivery-tool/.

The package ships HTML, the demo video, and GitHub links — nothing else. The
repo's markdown stays in the repo: documents are read through the wiki and the
decks, and every code or document reference the HTML carries either resolves
in-bundle or points at the public GitHub repository.

Steps, in order:
  1. Rebuild every deck HTML from its markdown source (presentation/**/*-deck.md),
     so the decks the landing page and the wiki link are current.
  2. Rebuild the wiki and write its self-contained bundle into Hackathon-Delivery/
     (python website/build-pages.py --clean --bundle <dest>) — both build modes
     render website/pages/ strictly from the markdown under documents/ and the
     other crawl roots; --bundle makes the output serverless.
  3. Overlay the landing page (delivery-index.html beside this script) as
     index.html, and video-evidence/system-test.mp4 as video/system-test.mp4.
  4. Prune what must not ship: the code trees the crawl pulled in (CODE_DIRS)
     and every raw markdown mirror, then remove emptied folders.
  5. Rewrite shipped HTML: links into pruned code trees and links to pruned
     markdown files point at the GitHub copy; links to the video point at the
     one packaged file.
  6. Verify: landing-page targets exist, no internal or code paths ship, no
     markdown ships, exactly one video copy, and EVERY relative link in every
     shipped HTML page resolves. Then zip to Hackathon-Delivery.zip.

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
from urllib.parse import unquote

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
# and every shipped link into them is rewritten to the GitHub copy.
CODE_DIRS = ("ADA_ECU", "IVI_ECU", "Scenario_Player", "V2X_ECU", "contracts", "tools")

REL_LINK = re.compile(r'(?P<attr>href|src)="(?P<raw>[^"]+)"')

# The demo video ships exactly once, at this package path. Documents link its
# repo path (video-evidence/…, git-ignored — too large for the git history).
VIDEO_PKG_REL = "video/system-test.mp4"
VIDEO_LINK = re.compile(r'(?P<attr>href|src)="(?:\.\./)*video-evidence/system-test\.mp4"')

# Internal-only paths that must never ship. The bundler pulls in whatever the
# pages link, so one of these appearing means a document links it — unlink at
# the markdown source (precedent: commit 6dce4c6), never by deleting here.
FORBIDDEN = (".claude", ".github", "CLAUDE.md")

# The organizers' reference file carries two logo <img> refs that never shipped
# with it; it is inherited verbatim and excluded from the link gate.
LINK_GATE_EXCLUDED = ("Car-Sky-Platform.html",)


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
    dest = OUT / VIDEO_PKG_REL
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(VIDEO, dest)
    print("--- overlay: landing page + demo video")


def prune():
    code_files = 0
    for d in CODE_DIRS:
        tree = OUT / d
        if tree.exists():
            code_files += sum(1 for f in tree.rglob("*") if f.is_file())
            shutil.rmtree(tree)
    # The crawl copies the linked video at its repo path; the packaged copy at
    # VIDEO_PKG_REL is the only one that ships.
    dup = OUT / "video-evidence"
    if dup.exists():
        shutil.rmtree(dup)
    md_files = 0
    for md in sorted(OUT.rglob("*.md")):
        md.unlink()
        md_files += 1
    for d in sorted((p for p in OUT.rglob("*") if p.is_dir()), reverse=True):
        if not any(d.iterdir()):
            d.rmdir()
    print(f"--- pruned {code_files} code files and {md_files} raw markdown files; "
          "documents ship as HTML, code ships from GitHub")


def rewrite_links():
    """Point every shipped HTML link at something that resolves: the video goes
    to the one packaged file; any other relative link whose target was pruned
    from the bundle but exists in the repo goes to the GitHub copy."""
    n_github = n_video = pages = 0
    for page in sorted(OUT.rglob("*.html")):
        text = page.read_text(encoding="utf-8", errors="ignore")
        video_rel = Path(os.path.relpath(OUT / VIDEO_PKG_REL, page.parent)).as_posix()

        def unresolved_to_github(m):
            nonlocal n_github
            raw = m.group("raw")
            if raw.startswith(LINK_SKIP):
                return m.group(0)
            target = unquote(raw.split("#", 1)[0].split("?", 1)[0])
            if not target or (page.parent / target).exists():
                return m.group(0)
            resolved = (page.parent / target).resolve()
            try:
                rel = resolved.relative_to(OUT.resolve()).as_posix()
            except ValueError:
                return m.group(0)  # escapes the bundle; leave for verify()
            src = REPO / rel
            if not src.exists():
                return m.group(0)  # not a repo file either; leave for verify()
            kind = "tree" if src.is_dir() else "blob"
            n_github += 1
            return f'{m.group("attr")}="{GITHUB_URL}/{kind}/main/{rel}"'

        new, c = VIDEO_LINK.subn(lambda m: f'{m.group("attr")}="{video_rel}"', text)
        new = REL_LINK.sub(unresolved_to_github, new)
        if new != text:
            page.write_text(new, encoding="utf-8")
            n_video += c
            pages += 1
    print(f"--- rewrote links across {pages} pages: {n_github} pruned targets -> GitHub, "
          f"{n_video} video -> {VIDEO_PKG_REL}")


HREF = re.compile(r'(?:href|src)="([^"]+)"')
LINK_SKIP = ("http://", "https://", "mailto:", "data:", "javascript:", "#")


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
    stray_md = [str(p.relative_to(OUT)) for p in OUT.rglob("*.md")]
    if stray_md:
        sys.exit("raw markdown still shipped: " + ", ".join(stray_md))
    stray_mp4 = [str(p.relative_to(OUT)) for p in OUT.rglob("*.mp4")
                 if p.relative_to(OUT).as_posix() != VIDEO_PKG_REL]
    if stray_mp4:
        sys.exit("video copies outside " + VIDEO_PKG_REL + ": " + ", ".join(stray_mp4))
    # The judge's click test: every relative link in every shipped page resolves.
    broken = []
    for page in sorted(OUT.rglob("*.html")):
        if page.name in LINK_GATE_EXCLUDED:
            continue
        for raw in HREF.findall(page.read_text(encoding="utf-8", errors="ignore")):
            if raw.startswith(LINK_SKIP) or not raw:
                continue
            target = unquote(raw.split("#", 1)[0].split("?", 1)[0])
            if not target:
                continue
            if not (page.parent / target).exists():
                broken.append(f"{page.relative_to(OUT)} -> {raw}")
    if broken:
        sys.exit(f"broken relative links in shipped HTML ({len(broken)}):\n  "
                 + "\n  ".join(broken[:40]))
    print("--- verified: landing targets, no internal/code/markdown paths, "
          "one video copy, every relative HTML link resolves")


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
    prune()
    rewrite_links()
    verify()
    make_zip()
    print(f"done -> {OUT} and {ZIP}")


if __name__ == "__main__":
    main()
