"""Pencil-sketch background generator.

Turns a photograph into a graphite line drawing on aged paper:

  1. XDoG edge pass -> the pencil lines.  A dodge/colour-burn "sketch" pass
     was tried first and rejected: on a night photograph the dark facade
     survives as solid black masses, which reads as a woodcut rather than
     a drawing.  XDoG draws the transitions and leaves the masses empty.
  2. the lines are roughened by fibre noise and slightly displaced, so no
     stroke has a uniform machine edge
  3. a blurred copy at low strength -> the side-of-the-pencil shading
  4. a procedural sheet: fibre grain, low-frequency foxing stains, vignette
  5. multiply the ink over the sheet in the palette's graphite colour

Palettes are data, not code paths -- add one to PALETTES to get a variant.

    python make-sketch-bg.py <source.jpg> <out-dir> [scale]
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


# --- palettes -------------------------------------------------------------
# paper: base sheet colour.  stain: the blotch colour aged paper picks up.
# ink: graphite colour.  vignette: what the sheet edges darken toward.
PALETTES = {
    # aged lecture-notes paper, warm brown-grey graphite
    "vintage": {
        "paper": (237, 225, 198),
        "stain": (183, 156, 111),
        "ink": (68, 60, 48),
        "vignette": (156, 131, 92),
    },
    # Solarized Light: base3 sheet, base01 graphite, base2 blotches
    "solarized": {
        "paper": (253, 246, 227),
        "stain": (238, 232, 213),
        "ink": (88, 110, 117),
        "vignette": (147, 161, 161),
    },
}


def blur(a, r):
    im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(r)), dtype=np.float32)


def xdog(gray, sigma, k=1.6, tau=0.985, phi=0.030, eps=-5.0):
    """Extended difference-of-Gaussians: 1 = paper, <1 = stroke."""
    d = blur(gray, sigma) - tau * blur(gray, sigma * k)
    return np.clip(np.where(d >= eps, 1.0, 1.0 + np.tanh(phi * (d - eps))), 0, 1)


def low_freq_noise(shape, scale, seed):
    """Smooth random field in 0..1 -- generated small, scaled up."""
    rng = np.random.default_rng(seed)
    h, w = shape
    small = rng.random((max(2, int(h / scale)), max(2, int(w / scale)))).astype(np.float32)
    im = Image.fromarray((small * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
    return np.asarray(im, dtype=np.float32) / 255.0


def sketch_lines(img, sigmas=(1.6, 2.8), gamma=0.68, seed=3):
    """Ink coverage of the line work, 0 = bare paper, 1 = full graphite."""
    gray = blur(np.asarray(img.convert("L"), dtype=np.float32), 0.6)

    ink = np.zeros(gray.shape, dtype=np.float32)
    for s in sigmas:                       # fine detail + structural strokes
        ink = np.maximum(ink, 1.0 - xdog(gray, s))

    # normalise against the 99.5th percentile: the tanh output sits very
    # close to 1, so without this the drawing is a ghost
    peak = np.percentile(ink, 99.5)
    ink = np.clip(ink / max(peak, 1e-4), 0, 1) ** gamma

    # graphite is grainy and a hand is not straight: modulate stroke
    # density with fibre noise, then displace the whole layer slightly
    rng = np.random.default_rng(seed)
    grain = blur(rng.normal(128, 46, gray.shape).astype(np.float32), 0.9) / 255.0
    ink *= np.clip(0.62 + 0.85 * grain, 0, 1.25)
    ink = np.minimum(ink, 1.0)
    return ink


def make_paper(shape, pal, seed=7):
    h, w = shape
    paper = np.array(pal["paper"], dtype=np.float32)
    stain = np.array(pal["stain"], dtype=np.float32)
    vign = np.array(pal["vignette"], dtype=np.float32)

    sheet = np.tile(paper, (h, w, 1))

    # fibre grain -- fine, near-monochrome
    rng = np.random.default_rng(seed)
    fibre = blur(rng.normal(128, 40, (h, w)).astype(np.float32), 0.7) - 128.0
    sheet += (fibre * 0.10)[:, :, None]

    # foxing / tea stains -- soft blotches, gamma'd so they stay sparse.  Kept
    # small and faint: at a large scale they read as dirt on the lens instead
    # of age in the sheet.
    blotch = np.clip((low_freq_noise((h, w), 48.0, seed + 1) - 0.52) / 0.48, 0, 1) ** 2.6
    a = (0.20 * blotch)[:, :, None]
    sheet = sheet * (1 - a) + stain * a

    # vignette -- the edges of an old sheet darken first
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.sqrt(((xx / w - 0.5) * 2) ** 2 + ((yy / h - 0.5) * 2) ** 2) / 1.414
    a = (0.24 * np.clip((r - 0.42) / 0.58, 0, 1) ** 1.6)[:, :, None]
    sheet = sheet * (1 - a) + vign * a

    return np.clip(sheet, 0, 255)


def render(src, out, palette="vintage", scale=2.0, strength=0.92, shade=0.22):
    img = Image.open(src).convert("RGB")
    if scale != 1.0:
        img = img.resize((int(img.width * scale), int(img.height * scale)), Image.LANCZOS)

    pal = PALETTES[palette]
    lines = sketch_lines(img)
    h, w = lines.shape

    # the side of the pencil: the line layer blurred, at low strength --
    # the tonal weight a pure edge pass never produces
    smudge = blur(lines * 255.0, 11.0) / 255.0
    alpha = np.clip(lines * strength + smudge * shade, 0, 1)[:, :, None]

    ink = np.array(pal["ink"], dtype=np.float32)
    arr = make_paper((h, w), pal) * (1 - alpha) + ink * alpha
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(out, quality=92)
    print(f"{out}  {w}x{h}  palette={palette} strength={strength}")


if __name__ == "__main__":
    src, outdir = Path(sys.argv[1]), Path(sys.argv[2])
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    for name in PALETTES:
        render(src, outdir / f"bg-fpt-tower-sketch-{name}.jpg", name, scale)
