#!/usr/bin/env python3
"""Serve the story-telling pages over HTTP.

story-telling/ is a self-contained client-side app: index.html loads
js/main.js as an ES module, which fetches each page's content/*.md at
runtime and renders it into a Three.js scene. There is no markdown-to-HTML
build step the way website/ or presentation/ have — every page renders
itself from content/*.md and assets/ already sitting on disk. What it does
need is an HTTP server: browsers refuse to fetch() local files or load ES
modules from a file:// URL, so double-clicking index.html will not work.

Usage, from anywhere:
    python story-telling/serve-story.py [port]

Defaults to port 8080. Open the printed URL in a browser.
"""

import http.server
import socketserver
import sys
from functools import partial
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    handler = partial(http.server.SimpleHTTPRequestHandler, directory=str(HERE))
    with socketserver.TCPServer(("", port), handler) as httpd:
        print(f"serving {HERE} at http://localhost:{port}/")
        print("Ctrl+C to stop")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
