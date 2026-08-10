# Package-Delivery-tool

Builds the outgoing delivery package — `Hackathon-Delivery/` and `Hackathon-Delivery.zip` inside this folder, both gitignored. One command, from the repo root:

```bash
python Package-Delivery-tool/build_package.py
```

The package ships **HTML, the demo video, and GitHub links — nothing else**. The repo's markdown stays in the repo; documents are read through the wiki and the decks.

## What it does, in order

1. **Rebuilds every deck** — runs [presentation/slide-build-tool/build-slides.py](../presentation/slide-build-tool/build-slides.py) on every `presentation/**/*-deck.md`, so the shipped HTML is fresh from source.
2. **Rebuilds the wiki and bundles it** — runs `python website/build-pages.py --clean --bundle` targeting `Package-Delivery-tool/Hackathon-Delivery/`. Both build modes render `website/pages/` strictly from the markdown under `documents/` and the other crawl roots; `--bundle` makes the output self-contained, so it opens on a double-click with no server.
3. **Overlays the package extras** — [delivery-index.html](delivery-index.html) becomes the package's `index.html` (the landing page), and `video-evidence/system-test.mp4` is copied to `video/system-test.mp4`.
4. **Prunes what must not ship** — the code trees the wiki crawl pulled in, every raw markdown mirror, and the crawler's duplicate of the video.
5. **Rewrites shipped HTML links** — the video everywhere to the one packaged copy; any link whose target was pruned to its GitHub copy.
6. **Verifies** — landing-page targets exist, no internal/code/markdown paths ship, exactly one video copy, and every relative link in every shipped HTML page resolves — then **zips** the result to `Hackathon-Delivery.zip`, the file to send out.

## The landing page

[delivery-index.html](delivery-index.html) carries five cards, in this order:

1. Video demo — `video/system-test.mp4`
2. Product & Delivery (business view) — `presentation/m1-business-delivery/m1-business-delivery-deck.html`
3. System Test Delivery Report — `presentation/phase6-systemIntegration/phase6-system-delivery-deck.html`
4. System Design Report — `presentation/system-design/system-design-deck.html`
5. Project wiki — `website/index.html`

Edit it here, then rerun the build — never edit `Hackathon-Delivery/index.html`, which every build overwrites.
