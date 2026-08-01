#!/usr/bin/env python3
"""Build a static HTML slide deck from a Marp-flavoured markdown source.

    python build-slides.py <deck.md>

The HTML is always written beside the markdown, same basename. All markup and
styling comes from presentation/template/template.md — this script holds none of
its own; it parses the source, picks a layout per slide, and fills the template
blocks.
"""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None

TEMPLATE = Path(__file__).resolve().parent.parent / "template" / "template.md"

DEFAULT_COLORS = {
    "orange": "#F37021",
    "green": "#50B848",
    "blue": "#034EA2",
    "teal": "#33B2C1",
    "navy": "#19226D",
    "navy_deep": "#0E1540",
    "ink": "#080808",
    "ink_soft": "#3d3d46",
    "paper": "#f4f4f6",
}

DEFAULT_FONTS = {
    "default": '"Segoe UI", "Segoe UI Variable", "Helvetica Neue", Arial, sans-serif',
    "mono": '"Consolas", "Courier New", monospace',
}


# --------------------------------------------------------------------------- #
# template
# --------------------------------------------------------------------------- #

def load_blocks(path: Path) -> dict[str, str]:
    """Extract every ``<!-- block: NAME -->`` + fenced code block from template.md."""
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"<!--\s*block:\s*([\w-]+)\s*-->\s*\n```[\w]*\n(.*?)\n```",
        re.DOTALL,
    )
    blocks = {m.group(1): m.group(2) for m in pattern.finditer(text)}
    if not blocks:
        raise SystemExit(f"no template blocks found in {path}")
    return blocks


def fill(template: str, values: dict[str, str]) -> str:
    """Replace every ``{{KEY}}`` in one pass so substituted content is never rescanned."""
    return re.sub(
        r"\{\{(\w+)\}\}",
        lambda m: values.get(m.group(1), m.group(0)),
        template,
    )


# --------------------------------------------------------------------------- #
# inline markdown
# --------------------------------------------------------------------------- #

def inline(text: str) -> str:
    """Convert inline markdown to HTML, escaping everything that isn't markup."""
    placeholders: list[str] = []

    def stash(raw: str) -> str:
        placeholders.append(raw)
        return f"\x00{len(placeholders) - 1}\x00"

    # protect code spans before escaping so backtick content stays literal
    text = re.sub(r"`([^`]+)`", lambda m: stash(f"<code>{html.escape(m.group(1))}</code>"), text)
    # protect explicit line breaks authored as raw HTML
    text = re.sub(r"<br\s*/?>", lambda _: stash("<br>"), text, flags=re.I)

    text = html.escape(text, quote=False)

    text = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        lambda m: f'<a href="{html.escape(m.group(2), quote=True)}">{m.group(1)}</a>',
        text,
    )
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"(?<![\w\\])_([^_\n]+)_(?![\w])", r"<em>\1</em>", text)

    return re.sub(r"\x00(\d+)\x00", lambda m: placeholders[int(m.group(1))], text)


IMAGE_RE = re.compile(r"^!\[([^\]]*)\]\(([^)]+)\)\s*$")


def parse_image(line: str) -> tuple[str, str, str] | None:
    """``![h:260 Caption](src)`` -> (src, caption, inline style)."""
    m = IMAGE_RE.match(line.strip())
    if not m:
        return None
    alt, src = m.group(1), m.group(2)
    style = ""
    height = re.match(r"^h:(\d+)\s*(.*)$", alt)
    if height:
        style = f' style="height:{height.group(1)}px"'
        alt = height.group(2)
    return src, alt.strip(), style


# --------------------------------------------------------------------------- #
# slide model
# --------------------------------------------------------------------------- #

class Slide:
    def __init__(self, raw: str):
        self.directives: dict[str, str] = {}
        self.background: str | None = None
        self.lines: list[str] = []
        self._parse(raw)

    def _parse(self, raw: str) -> None:
        for line in raw.split("\n"):
            stripped = line.strip()
            directive = re.match(r"^<!--\s*_(\w+):?\s*(.*?)\s*-->$", stripped)
            if directive:
                self.directives[directive.group(1)] = directive.group(2)
                continue
            if re.match(r"^<!--.*-->$", stripped):  # plain authoring comment
                continue
            bg = re.match(r"^!\[bg[^\]]*\]\(([^)]+)\)\s*$", stripped)
            if bg:
                self.background = bg.group(1)
                continue
            self.lines.append(line)
        while self.lines and not self.lines[0].strip():
            self.lines.pop(0)
        while self.lines and not self.lines[-1].strip():
            self.lines.pop()

    @property
    def is_lead(self) -> bool:
        return self.directives.get("class") == "lead"

    @property
    def heading(self) -> str:
        for line in self.lines:
            if line.startswith("# "):
                return line[2:].strip()
        return ""

    def body_lines(self) -> list[str]:
        out, seen_h1 = [], False
        for line in self.lines:
            if not seen_h1 and line.startswith("# "):
                seen_h1 = True
                continue
            out.append(line)
        return out


def split_slides(body: str) -> list[Slide]:
    parts = re.split(r"(?m)^-{3,}\s*$", body)
    return [Slide(p) for p in parts if p.strip()]


# --------------------------------------------------------------------------- #
# body rendering
# --------------------------------------------------------------------------- #

def group_blocks(lines: list[str]) -> list[tuple[str, list[str]]]:
    """Split body lines into (kind, lines) blocks."""
    blocks: list[tuple[str, list[str]]] = []
    buf: list[str] = []
    kind = ""

    def flush() -> None:
        nonlocal buf, kind
        if buf:
            blocks.append((kind, buf))
        buf, kind = [], ""

    fence = False
    for line in lines:
        stripped = line.strip()

        if fence:
            if stripped.startswith("```"):
                flush()
                fence = False
            else:
                buf.append(line)
            continue
        if stripped.startswith("```"):
            flush()
            fence, kind = True, "code"
            continue

        if not stripped:
            flush()
            continue

        if stripped.startswith("## "):
            this = "subhead"
        elif stripped.startswith("|"):
            this = "table"
        elif re.match(r"^[-*]\s+", stripped):
            this = "list"
        elif re.match(r"^\d+\.\s+", stripped):
            this = "olist"
        elif stripped.startswith(">"):
            this = "quote"
        elif IMAGE_RE.match(stripped):
            this = "image"
        elif stripped.startswith("<"):
            this = "raw"
        else:
            this = "para"

        # images and subheads never merge with a neighbouring block
        if kind != this or this in ("image", "subhead"):
            flush()
            kind = this
        buf.append(line)

    flush()
    return blocks


def render_list(lines: list[str]) -> str:
    items, current = [], ""
    for line in lines:
        m = re.match(r"^\s*[-*]\s+(.*)$", line)
        if m:
            if current:
                items.append(current)
            current = m.group(1)
        else:
            current += " " + line.strip()
    if current:
        items.append(current)
    rendered = "\n".join(f"      <li>{inline(i)}</li>" for i in items)
    return f'    <ul class="points">\n{rendered}\n    </ul>'


def render_table(lines: list[str]) -> str:
    rows = []
    for line in lines:
        stripped = line.strip()
        if re.match(r"^[\s|:-]+$", stripped):  # the |---|---| separator row
            continue
        if stripped.startswith("|"):
            stripped = stripped[1:]
        if stripped.endswith("|") and not stripped.endswith(r"\|"):
            stripped = stripped[:-1]
        cells = re.split(r"(?<!\\)\|", stripped)
        rows.append([c.strip().replace(r"\|", "|") for c in cells])
    if not rows:
        return ""
    head, body = rows[0], rows[1:]
    headers = "".join(f"<th>{inline(c)}</th>" for c in head)
    out = [
        '    <table class="fpt">',
        f"      <thead><tr>{headers}</tr></thead>",
        "      <tbody>",
    ]
    for row in body:
        cells = "".join(f"<td>{inline(c)}</td>" for c in row)
        out.append(f"        <tr>{cells}</tr>")
    out.append("      </tbody>")
    out.append("    </table>")
    return "\n".join(out)


def render_toc(lines: list[str], blocks: dict[str, str]) -> str:
    items = []
    for line in lines:
        m = re.match(r"^\s*(\d+)\.\s+(.*)$", line)
        if not m:
            continue
        number, rest = m.group(1), m.group(2)
        title, _, description = rest.partition("—")
        items.append(
            fill(
                blocks["toc-item"],
                {
                    "NUMBER": number.zfill(2),
                    "TITLE": inline(title.strip().strip("*")),
                    "DESCRIPTION": inline(description.strip()),
                },
            )
        )
    return fill(blocks["toc"], {"ITEMS": "\n".join(items)})


def render_body(slide: Slide, blocks: dict[str, str], is_toc: bool) -> str:
    parts: list[str] = []
    for kind, lines in group_blocks(slide.body_lines()):
        if kind == "list":
            parts.append(render_list(lines))
        elif kind == "olist":
            parts.append(render_toc(lines, blocks) if is_toc else render_list(lines))
        elif kind == "table":
            parts.append(render_table(lines))
        elif kind == "quote":
            text = " ".join(l.strip().lstrip(">").strip() for l in lines)
            parts.append(f'    <div class="strip">{inline(text)}</div>')
        elif kind == "code":
            body = "\n".join(html.escape(l) for l in lines)
            parts.append(f'    <pre class="log">{body}</pre>')
        elif kind == "image":
            parsed = parse_image(lines[0])
            if parsed:
                src, caption, style = parsed
                parts.append(
                    fill(
                        blocks["imgbox"],
                        {
                            "SRC": html.escape(src, quote=True),
                            "STYLE": style,
                            "CAPTION": html.escape(caption, quote=True),
                        },
                    )
                )
        elif kind == "subhead":
            text = lines[0].strip()[3:]
            parts.append(f'    <p class="lead"><strong>{inline(text)}</strong></p>')
        elif kind == "raw":
            parts.append("\n".join(f"    {l.strip()}" for l in lines))
        else:
            text = " ".join(l.strip() for l in lines)
            parts.append(f'    <p class="lead">{inline(text)}</p>')

    if slide.directives.get("layout") == "cols":
        parts = fold_columns(parts)
    return "\n".join(p for p in parts if p)


def fold_columns(parts: list[str]) -> list[str]:
    """Pair the first points list with the first image side by side."""
    li = next((i for i, p in enumerate(parts) if 'class="points"' in p), None)
    ii = next((i for i, p in enumerate(parts) if 'class="imgbox"' in p), None)
    if li is None or ii is None:
        return parts
    col = (
        '    <div class="cols">\n'
        + parts[li].replace('<ul class="points">', '<ul class="points" style="flex:0 0 470px">')
        + "\n"
        + parts[ii].replace('<div class="imgbox">', '<div class="imgbox" style="flex:1">')
        + "\n    </div>"
    )
    out = [p for i, p in enumerate(parts) if i not in (li, ii)]
    out.insert(min(li, ii), col)
    return out


# --------------------------------------------------------------------------- #
# page rendering
# --------------------------------------------------------------------------- #

def background_style(path: str | None, gradient: str) -> str:
    if not path:
        return ""
    url = html.escape(path, quote=True)
    return f" style=\"background-image: {gradient}, url('{url}')\""


def render_cover(slide: Slide, blocks: dict[str, str]) -> str:
    eyebrow, tagline, meta = "", "", []
    for kind, lines in group_blocks(slide.body_lines()):
        text = " ".join(l.strip() for l in lines)
        if kind == "subhead":
            tagline = text[3:].strip()
        elif not eyebrow and re.fullmatch(r"\*\*.+\*\*", text.strip()):
            eyebrow = text.strip().strip("*")
        else:
            meta.append(text)
    return fill(
        blocks["cover"],
        {
            "BG": background_style(slide.background, blocks["cover-gradient"].strip()),
            "EYEBROW": inline(eyebrow),
            "TITLE": inline(slide.heading),
            "TAGLINE": inline(tagline),
            "META": "<br>".join(inline(m) for m in meta),
            "ACCENTS": blocks["accents"],
        },
    )


def render_section(slide: Slide, blocks: dict[str, str]) -> tuple[str, str]:
    number, title = "", slide.heading
    m = re.match(r"^(\d+)\s*[·.\-–]?\s*(.*)$", title)
    if m:
        number, title = m.group(1).zfill(2), m.group(2)
    html_out = fill(
        blocks["section"],
        {
            "BG": background_style(slide.background, blocks["section-gradient"].strip()),
            "NUMBER": number,
            "TITLE": inline(title),
            "ACCENTS": blocks["accents"],
        },
    )
    return html_out, number


def render_thanks(slide: Slide, blocks: dict[str, str]) -> str:
    body = " ".join(l.strip() for l in slide.body_lines() if l.strip())
    return fill(
        blocks["thanks"],
        {
            "BG": background_style(slide.background, blocks["thanks-gradient"].strip()),
            "TITLE": inline(slide.heading),
            "BODY": inline(body),
            "ACCENTS": blocks["accents"],
        },
    )


def render_slide(slide: Slide, blocks: dict[str, str], number: str, deck: str) -> str:
    is_toc = "table of contents" in slide.heading.lower()
    return fill(
        blocks["slide"],
        {
            "NUMBER": "§" if is_toc else number,
            "TITLE": inline(slide.heading),
            "BODY": render_body(slide, blocks, is_toc),
            "FOOTER": fill(blocks["footer"], {"DECK": html.escape(deck)}),
        },
    )


# --------------------------------------------------------------------------- #
# build
# --------------------------------------------------------------------------- #

def parse_frontmatter(text: str) -> tuple[dict, str]:
    m = re.match(r"^---\n(.*?)\n---\n", text, re.DOTALL)
    if not m:
        return {}, text
    raw = m.group(1)
    if yaml is None:
        meta = {}
        for line in raw.split("\n"):
            k, sep, v = line.partition(":")
            if sep and not line.startswith(" "):
                meta[k.strip()] = v.strip().strip("\"'")
    else:
        meta = yaml.safe_load(raw) or {}
    return meta, text[m.end():]


def build(md_path: Path) -> str:
    blocks = load_blocks(TEMPLATE)
    meta, body = parse_frontmatter(md_path.read_text(encoding="utf-8"))

    title = str(meta.get("title", md_path.stem))
    deck = str(meta.get("deck", title))
    colors = {**DEFAULT_COLORS, **(meta.get("colors") or {})}
    fonts = {**DEFAULT_FONTS, **(meta.get("fonts") or {})}

    slides = split_slides(body)
    rendered: list[str] = []
    section = ""
    cover_done = False

    for slide in slides:
        if slide.is_lead and not cover_done:
            rendered.append(render_cover(slide, blocks))
            cover_done = True
        elif slide.is_lead and slide.heading.lower().startswith("thank"):
            rendered.append(render_thanks(slide, blocks))
        elif slide.is_lead:
            page, number = render_section(slide, blocks)
            rendered.append(page)
            section = number or section
        else:
            rendered.append(render_slide(slide, blocks, section, deck))

    styles = fill(
        blocks["styles"],
        {
            "COLOR_ORANGE": colors["orange"],
            "COLOR_GREEN": colors["green"],
            "COLOR_BLUE": colors["blue"],
            "COLOR_TEAL": colors["teal"],
            "COLOR_NAVY": colors["navy"],
            "COLOR_NAVY_DEEP": colors["navy_deep"],
            "COLOR_INK": colors["ink"],
            "COLOR_INK_SOFT": colors["ink_soft"],
            "COLOR_PAPER": colors["paper"],
            "FONT_DEFAULT": fonts["default"],
            "FONT_MONO": fonts["mono"],
        },
    )

    return fill(
        blocks["document"],
        {
            "TITLE": html.escape(title),
            "DESCRIPTION": html.escape(str(meta.get("description", "")), quote=True),
            "STYLES": styles,
            "SLIDES": "\n".join(rendered),
            "NAVIGATION": blocks["navigation"],
            "SCRIPT": blocks["script"],
        },
    )


def main() -> None:
    if len(sys.argv) != 2:
        print(__doc__)
        raise SystemExit(2)

    md_path = Path(sys.argv[1]).resolve()
    if not md_path.exists():
        raise SystemExit(f"not found: {md_path}")

    out_path = md_path.with_suffix(".html")
    out_path.write_text(build(md_path), encoding="utf-8")
    print(f"{md_path.name} -> {out_path}")


if __name__ == "__main__":
    main()
