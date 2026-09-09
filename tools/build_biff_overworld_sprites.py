#!/usr/bin/env python3
"""Build Butano-ready Biff overworld sprites from graphics/source/biff*.png.

Pads 24x32 art into 32x32 frames (GBA-valid size), strips the Mother green
background, and emits indexed BMP + JSON into graphics/.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "graphics" / "source"
OUT_DIR = ROOT / "graphics"

FRAME_W = 32
FRAME_H = 32
SRC_W = 24
SRC_H = 32
CHROMA_KEY = (255, 0, 255)
MOTHER_BG = (57, 68, 57)
BG_TOLERANCE = 12

# Each entry is (filename, flip_horizontal). Right-facing dirs flip at runtime.
SHEETS: dict[str, list[tuple[str, bool]]] = {
    "biff_idle_down": [("biffstationaryfront.png", False)],
    "biff_idle_up": [("biffstationaryup.png", False)],
    "biff_idle_side": [("biffstationaryleft.png", False)],
    "biff_idle_diag_dl": [("biffstationarydownleft.png", False)],
    "biff_idle_diag_ul": [("biffstationaryupleft.png", False)],
    # Down/up only have one drawn frame — duplicate for a static 2-step cycle.
    "biff_walk_down": [("biffwalkingdownstep.png", False), ("biffwalkingdownstep.png", False)],
    "biff_walk_up": [("biffwalkingupstep1.png", False), ("biffwalkingupstep1.png", False)],
    "biff_walk_side": [("biffwalkingleftstep1.png", False), ("biffwalkingleftstep2.png", False)],
    "biff_walk_diag_dl": [("biffwalkingdownleftstep1.png", False), ("biffwalkingdownleftstep2.png", False)],
    "biff_walk_diag_ul": [("biffwalkingupleftstep1.png", False), ("biffwalkingupleftstep2.png", False)],
    "guy": [("guy.png", False)],
}


def _matches_bg(rgb: tuple[int, int, int]) -> bool:
    return all(abs(rgb[index] - MOTHER_BG[index]) <= BG_TOLERANCE for index in range(3))


def load_frame(path: Path, flip_horizontal: bool = False) -> Image.Image:
    rgba = Image.open(path).convert("RGBA")
    px = rgba.load()
    width, height = rgba.size

    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = px[x, y]
            if alpha < 16 or _matches_bg((red, green, blue)):
                px[x, y] = (*CHROMA_KEY, 255)

    frame = Image.new("RGBA", (FRAME_W, FRAME_H), (*CHROMA_KEY, 255))
    paste_x = (FRAME_W - min(width, SRC_W)) // 2
    paste_y = FRAME_H - min(height, SRC_H)
    crop = rgba.crop((0, 0, min(width, SRC_W), min(height, SRC_H)))
    frame.paste(crop, (paste_x, paste_y), crop)

    if flip_horizontal:
        frame = ImageOps.mirror(frame)

    return frame


def compose_sheet(stem: str, frame_specs: list[tuple[str, bool]]) -> Image.Image:
    frames = [load_frame(SOURCE_DIR / name, flip) for name, flip in frame_specs]
    sheet = Image.new("RGBA", (FRAME_W * len(frames), FRAME_H), (*CHROMA_KEY, 255))

    for index, frame in enumerate(frames):
        sheet.paste(frame, (index * FRAME_W, 0), frame)

    return sheet


def collect_samples(sheet: Image.Image) -> list[tuple[int, int, int]]:
    rgba = sheet.convert("RGBA")
    px = rgba.load()
    samples: list[tuple[int, int, int]] = []

    for y in range(rgba.height):
        for x in range(rgba.width):
            red, green, blue, alpha = px[x, y]
            if alpha < 16:
                continue
            if (red, green, blue) == CHROMA_KEY:
                continue
            samples.append((red, green, blue))

    return samples


def build_palette(samples: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    palette: list[tuple[int, int, int]] = [(0, 0, 0)] * 16
    palette[0] = CHROMA_KEY

    if not samples:
        return palette

    color_count = min(15, len(set(samples)))
    strip = Image.new("RGB", (len(samples), 1))
    strip.putdata(samples)
    quantized = strip.quantize(colors=color_count, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    strip_palette = quantized.getpalette() or []

    for slot in range(color_count):
        palette[slot + 1] = tuple(strip_palette[slot * 3:(slot + 1) * 3])

    return palette


def nearest_index(rgb: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    best_index = 1
    best_distance = sum((rgb[channel] - palette[best_index][channel]) ** 2 for channel in range(3))

    for slot in range(1, 16):
        distance = sum((rgb[channel] - palette[slot][channel]) ** 2 for channel in range(3))
        if distance < best_distance:
            best_distance = distance
            best_index = slot

    return best_index


def save_indexed_bmp(rgba: Image.Image, path: Path, palette: list[tuple[int, int, int]]) -> None:
    px = rgba.convert("RGBA").load()
    width, height = rgba.size
    indexed = Image.new("P", (width, height), 0)
    out_px = indexed.load()
    flat_palette = [channel for rgb in palette for channel in rgb]
    indexed.putpalette(flat_palette)

    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = px[x, y]
            if alpha < 16 or (red, green, blue) == CHROMA_KEY:
                out_px[x, y] = 0
            else:
                out_px[x, y] = nearest_index((red, green, blue), palette)

    path.parent.mkdir(parents=True, exist_ok=True)
    indexed.save(path, format="BMP")


def write_json(path: Path, frame_count: int, palette_item: str | None) -> None:
    payload: dict[str, object] = {
        "type": "sprite",
        "width": FRAME_W,
        "height": FRAME_H,
        "bpp_mode": "bpp_4",
    }
    if palette_item is not None:
        payload["palette_item"] = palette_item
    path.write_text(json.dumps(payload, indent=4) + "\n", encoding="utf-8")


def main() -> int:
    sys.path.insert(0, str(ROOT / "tools"))
    missing = [
        name
        for names in SHEETS.values()
        for name, _flip in names
        if not (SOURCE_DIR / name).exists()
    ]

    if missing:
        print("Missing source files:")
        for name in sorted(set(missing)):
            print(f"  {name}")
        return 1

    all_samples: list[tuple[int, int, int]] = []
    composed: dict[str, Image.Image] = {}

    for stem, filenames in SHEETS.items():
        sheet = compose_sheet(stem, filenames)
        composed[stem] = sheet
        all_samples.extend(collect_samples(sheet))

    palette = build_palette(all_samples)
    palette_owner = "biff_idle_down"

    for stem, sheet in composed.items():
        bmp_path = OUT_DIR / f"{stem}.bmp"
        save_indexed_bmp(sheet, bmp_path, palette)
        owner = None if stem == palette_owner else palette_owner
        write_json(bmp_path.with_suffix(".json"), len(SHEETS[stem]), owner)
        print(f"  {stem}.bmp  {sheet.size[0]}x{sheet.size[1]}  frames={len(SHEETS[stem])}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
