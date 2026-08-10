# Package-Delivery-tool

Builds the outgoing delivery package — `Hackathon-Delivery/` and `Hackathon-Delivery.zip` at the repo root, both gitignored. One command, from the repo root:

```bash
python Package-Delivery-tool/build_package.py
```

## What it does, in order

1. **Rebuilds every deck** — runs [presentation/slide-build-tool/build-slides.py](../presentation/slide-build-tool/build-slides.py) on every `presentation/**/*-deck.md`, so the shipped HTML is fresh from source.
2. **Rebuilds the wiki and bundles it** — runs `python website/build-pages.py --clean --bundle Hackathon-Delivery`. The bundle is self-contained: it mirrors the repo layout for every file the site references (decks, diagrams, documents), so it opens on a double-click with no server.
3. **Overlays the package extras** — [delivery-index.html](delivery-index.html) becomes the package's `index.html` (the landing page), and `video-evidence/system-test.mp4` is copied to `video/system-test.mp4`.
4. **Verifies** every landing-page link resolves to a file in the package, then **zips** the result to `Hackathon-Delivery.zip` — the file to send out.

## The landing page

[delivery-index.html](delivery-index.html) carries four cards, in this order:

1. Video demo — `video/system-test.mp4`
2. System Design Report — `presentation/system-design/system-design-deck.html`
3. System Test Delivery Report — `presentation/phase6-systemIntegration/phase6-system-delivery-deck.html`
4. Project wiki — `website/index.html`

Edit it here, then rerun the build — never edit `Hackathon-Delivery/index.html`, which every build overwrites.
