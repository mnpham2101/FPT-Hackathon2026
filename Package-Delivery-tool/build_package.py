#!/usr/bin/env python3
"""Build the outgoing delivery package: Hackathon-Delivery/ and Hackathon-Delivery.zip,
both written inside Package-Delivery-tool/.

Steps, in order:
  1. Rebuild every deck HTML from its markdown source (presentation/**/*-deck.md).
  2. Rebuild the wiki and write its self-contained bundle into Hackathon-Delivery/
     (python website/build-pages.py --bundle) — the bundle mirrors the repo layout
     and pulls in every deck and asset the site references, so it opens from disk
     with no server.
  3. Overlay the package extras: the landing page (delivery-index.html beside this
     script) as index.html, and video-evidence/system-test.mp4 as
     video/system-test.mp4.
  4. Zip the result to Hackathon-Delivery.zip beside it.

Usage, from the repo root:
    python Package-Delivery-tool/build_package.py
"""

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

# Paths the landing page links; the build fails if any is absent from the package.
LANDING_TARGETS = (
    "video/system-test.mp4",
    "presentation/system-design/system-design-deck.html",
    "presentation/phase6-systemIntegration/phase6-system-delivery-deck.html",
    "website/index.html",
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
    print("--- verified: all landing-page links resolve, no internal paths shipped")


def make_zip():
    if ZIP.exists():
        ZIP.unlink()
    files = sorted(f for f in OUT.rglob("*") if f.is_file())
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in files:
            zf.write(f, Path(OUT.name) / f.relative_to(OUT))
    print(f"--- {ZIP.name}: {len(files)} files, {ZIP.stat().st_size / 1e6:.1f} MB")


def main():
    build_decks()
    build_bundle()
    overlay()
    verify()
    make_zip()
    print(f"done -> {OUT} and {ZIP}")


if __name__ == "__main__":
    main()
