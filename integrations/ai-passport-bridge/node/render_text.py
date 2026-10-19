#!/usr/bin/env python3
"""Render a reply to a 1bpp bitmap for the AI Passport screen.

Usage: render_text.py <text-file> <out-file>
Output: 224x168 pixels, row-packed MSB first, 28 bytes per row (4704 bytes).
The device only needs to hand these bytes to an LVGL I1 canvas, so the fixed
font subset on the device stays small.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 224, 168
SIZE, LINE = 16, 20
MARGIN = 8
MAX_LINES = (HEIGHT - 6) // LINE
FONTS = [
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
]
# Never start a line with these; keep them attached to the previous line.
NO_LINE_START = set("，。！？；：、）】》」』…,.!?;:)”’")


def load_font():
    for path in FONTS:
        try:
            return ImageFont.truetype(path, SIZE, index=0)
        except OSError:
            continue
    raise SystemExit("no CJK font available")


def wrap(font, text):
    lines = []
    for paragraph in text.split("\n"):
        current = ""
        for ch in paragraph:
            if current and font.getlength(current + ch) > WIDTH - 2 * MARGIN - 2:
                if ch in NO_LINE_START and len(current) > 1:
                    lines.append(current[:-1])
                    current = current[-1] + ch
                else:
                    lines.append(current)
                    current = ch
            else:
                current += ch
        lines.append(current)
    return lines


def main():
    text_path, out_path = sys.argv[1], sys.argv[2]
    text = Path(text_path).read_text(encoding="utf-8", errors="replace").strip()
    text = " ".join(line.strip() for line in text.splitlines() if line.strip())
    font = load_font()
    lines = wrap(font, text)
    if len(lines) > MAX_LINES:
        lines = lines[:MAX_LINES]
        while lines[-1] and font.getlength(lines[-1] + "…") > WIDTH - 2 * MARGIN:
            lines[-1] = lines[-1][:-1]
        lines[-1] += "…"
    image = Image.new("1", (WIDTH, HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    y = max(6, (HEIGHT - len(lines) * LINE) // 2)
    for line in lines:
        draw.text((MARGIN, y), line, font=font, fill=1, anchor="la")
        y += LINE
    Path(out_path).write_bytes(image.tobytes())


if __name__ == "__main__":
    main()
