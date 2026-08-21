#!/usr/bin/env python3
"""Fill transparent PNG pixels with a solid chroma-key color (one-time art fix).

Editing tools often store "empty" areas as alpha=0 with RGB black. This script
replaces every pixel with alpha < 128 using the chroma-key color (default
magenta #ff00ff), then saves opaque PNGs ready for convert_sprites.py.

Usage:
  python3 tools/fill_transparent_png.py graphics/source
  python3 tools/fill_transparent_png.py graphics/source --key ff00ff
  python3 tools/fill_transparent_png.py graphics/source --in-place

By default writes to graphics/source_filled/ so originals stay untouched.
The key color defaults to ff00ff (magenta); override with --key RRGGBB.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image

ALPHA_CUTOFF = 128


DEFAULT_CHROMA_KEY = (255, 0, 255)


def parse_key_color(text: str) -> tuple[int, int, int]:
    cleaned = text.strip().lstrip("#")
    if not re.fullmatch(r"[0-9a-fA-F]{6}", cleaned):
        raise ValueError(f"invalid key color {text!r}; use RRGGBB like ff00ff")
    return (
        int(cleaned[0:2], 16),
        int(cleaned[2:4], 16),
        int(cleaned[4:6], 16),
    )


def key_from_top_left(im: Image.Image) -> tuple[int, int, int]:
    red, green, blue, alpha = im.convert("RGBA").getpixel((0, 0))
    if alpha >= ALPHA_CUTOFF:
        return (red, green, blue)
    raise ValueError(
        "Top-left pixel is transparent. Pass --key RRGGBB (e.g. --key ff00ff) "
        "or paint an opaque key color at pixel (0,0) first."
    )


def fill_transparent(im: Image.Image, key_rgb: tuple[int, int, int]) -> tuple[Image.Image, int]:
    rgba = im.convert("RGBA")
    px = rgba.load()
    width, height = rgba.size
    filled = 0

    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = px[x, y]
            if alpha >= ALPHA_CUTOFF:
                continue
            px[x, y] = (key_rgb[0], key_rgb[1], key_rgb[2], 255)
            filled += 1

    return rgba, filled


def iter_images(folder: Path) -> list[Path]:
    return sorted(
        path
        for path in folder.iterdir()
        if path.is_file() and path.suffix.lower() in {".png", ".bmp"}
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=Path, help="Folder of PNG/BMP composites")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("graphics/source_filled"),
        help="Output folder (default: graphics/source_filled)",
    )
    parser.add_argument(
        "--key",
        type=str,
        default="ff00ff",
        help="Chroma-key color as RRGGBB (default: ff00ff magenta)",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Overwrite files in source_dir instead of writing to --out",
    )
    args = parser.parse_args()

    if not args.source_dir.is_dir():
        raise SystemExit(f"Not a directory: {args.source_dir}")

    files = iter_images(args.source_dir)
    if not files:
        raise SystemExit(f"No PNG/BMP files in {args.source_dir}")

    out_dir = args.source_dir if args.in_place else args.out
    if not args.in_place:
        out_dir.mkdir(parents=True, exist_ok=True)

    total_filled = 0
    for src in files:
        im = Image.open(src)
        key_rgb = parse_key_color(args.key) if args.key else DEFAULT_CHROMA_KEY

        filled_im, filled_count = fill_transparent(im, key_rgb)
        dest = out_dir / src.name
        filled_im.save(dest, format="PNG")
        total_filled += filled_count
        print(f"{src.name}  key={key_rgb}  filled {filled_count} px  ->  {dest}")

    print(f"\nDone — {len(files)} file(s), {total_filled} pixels filled.")
    if not args.in_place:
        print(f"Next: python3 tools/convert_sprites.py {out_dir}")


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1) from error
