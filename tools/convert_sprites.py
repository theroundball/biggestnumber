#!/usr/bin/env python3
"""Convert PNG/BMP art into Butano-ready indexed BMP + JSON sprites.

Drop everything into one folder (default: graphics/source/) — the script picks
the conversion from image dimensions, using the filename when size alone is
ambiguous (e.g. 16x16 trinkets vs HUD icons).

  40x64          -> split into {name}_body / _accent_top / _accent_bottom
                  (magenta #ff00ff chroma-key; all card art shares one 16-color palette;
                   orange #ff7e00 frame is reserved at palette index 4 for rarity borders)

Fill transparent/empty areas with magenta (#ff00ff) before converting. Any pixel
matching the chroma key becomes palette index 0 (transparent on GBA).
  32x64          -> {name}_body.bmp
  8x32           -> card accent (name ends with _accent_*) or ui_edge_fade
  32x32          -> ui_{name}.bmp
  16x16          -> hud / trinket / ui marker (from filename)
  width x 8/16/32/64  -> font sheet pass-through

Usage:
  python3 tools/convert_sprites.py
  python3 tools/convert_sprites.py graphics/source --out graphics/output

Subfolders (cards/, trinkets/, etc.) still work as optional overrides.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path

from PIL import Image

PALETTE_COLORS = 16
DEFAULT_CHROMA_KEY = (255, 0, 255)
# Exact chroma-key match only.
KEY_COLOR_TOLERANCE = 0

# Reserved card-frame color. Runtime rarity borders recolor this palette index only.
BORDER_RGB = (255, 126, 0)
BORDER_INDEX = 4
# Exact orange (and near-quantization noise) maps to BORDER_INDEX; other art does not.
BORDER_MATCH_DISTANCE = 80

CARD_W = 40
CARD_H = 64
BODY_W = 32
ACCENT_W = 8
ACCENT_H = 32

SIZE_HUD = (16, 16)
SIZE_UI_PLACEHOLDER = (32, 32)
SIZE_UI_EDGE_FADE = (8, 32)
SIZE_CARD_BODY = (32, 64)
SIZE_CARD_ACCENT = (8, 32)
FONT_HEIGHTS = {8, 16, 32, 64}

TRINKET_SHORT_NAMES = {
    "morel",
    "lucky_7s",
    "lucky_sevens",
    "echo",
    "get_with_the_times",
    "empty",
}

HUD_SHORT_NAMES = {
    "deck",
    "graveyard",
    "turtle_timer",
    "turtle",
}

UI_SHORT_NAMES = {
    "marker_x",
    "marker_shovel",
    "graveyard_placeholder",
    "edge_fade",
    "x",
    "shovel",
}

UNIFIED_SOURCE_DIR = Path("graphics/source")
UNIFIED_OUTPUT_DIR = Path("graphics/output")
LEGACY_CARDS_SOURCE = Path("graphics/cards_source")
LEGACY_CARDS_OUTPUT = Path("graphics/cards_split")

# Butano graphics file names: lowercase letters, numbers, and underscores only.
_ASSET_STEM_RE = re.compile(r"[^a-z0-9_]+")


def sanitize_asset_stem(stem: str) -> str:
    normalized = stem.lower().replace("-", "_").replace(" ", "_")
    normalized = _ASSET_STEM_RE.sub("_", normalized)
    normalized = re.sub(r"_+", "_", normalized).strip("_")
    if not normalized:
        raise ValueError(f"invalid asset name: {stem!r}")
    return normalized


class Profile(Enum):
    CARD_SPLIT = auto()
    HUD = auto()
    TRINKET = auto()
    UI = auto()
    FONT = auto()
    CARD_PART = auto()


@dataclass(frozen=True)
class OutputAsset:
    stem: str
    image: Image.Image
    transparent_key: tuple[int, int, int] | None = None


def _matches_key_rgb(red: int, green: int, blue: int, key_rgb: tuple[int, int, int]) -> bool:
    key_r, key_g, key_b = key_rgb
    return (
        abs(red - key_r) <= KEY_COLOR_TOLERANCE
        and abs(green - key_g) <= KEY_COLOR_TOLERANCE
        and abs(blue - key_b) <= KEY_COLOR_TOLERANCE
    )


def _is_transparent_pixel(pixel: tuple[int, int, int, int], key_rgb: tuple[int, int, int]) -> bool:
    red, green, blue, _alpha = pixel
    return _matches_key_rgb(red, green, blue, key_rgb)


def parse_chroma_key(text: str) -> tuple[int, int, int]:
    cleaned = text.strip().lstrip("#")
    if not re.fullmatch(r"[0-9a-fA-F]{6}", cleaned):
        raise ValueError(f"invalid key color {text!r}; use RRGGBB like ff00ff")
    return (
        int(cleaned[0:2], 16),
        int(cleaned[2:4], 16),
        int(cleaned[4:6], 16),
    )


SharedPalette = list[tuple[int, int, int]]


def _rgb_distance(left: tuple[int, int, int], right: tuple[int, int, int]) -> int:
    return sum((left[index] - right[index]) ** 2 for index in range(3))


def _is_border_rgb(rgb: tuple[int, int, int]) -> bool:
    return _rgb_distance(rgb, BORDER_RGB) <= BORDER_MATCH_DISTANCE


def _nearest_palette_index(rgb: tuple[int, int, int], shared_palette: SharedPalette) -> int:
    if _is_border_rgb(rgb):
        return BORDER_INDEX

    best_index = 1 if BORDER_INDEX != 1 else 2
    best_distance = _rgb_distance(rgb, shared_palette[best_index])

    for index in range(1, PALETTE_COLORS):
        if index == BORDER_INDEX:
            continue

        distance = _rgb_distance(rgb, shared_palette[index])
        if distance < best_distance:
            best_distance = distance
            best_index = index

    return best_index


def build_shared_palette_from_samples(
    opaque_samples: list[tuple[int, int, int]],
    key_rgb: tuple[int, int, int],
) -> SharedPalette:
    shared_palette: SharedPalette = [(0, 0, 0)] * PALETTE_COLORS
    shared_palette[0] = key_rgb
    shared_palette[BORDER_INDEX] = BORDER_RGB

    # Reserve BORDER_INDEX for the frame; quantize illustration colors into the rest.
    art_samples = [sample for sample in opaque_samples if not _is_border_rgb(sample)]

    if not art_samples:
        return shared_palette

    color_count = min(PALETTE_COLORS - 2, len(set(art_samples)))
    strip = Image.new("RGB", (len(art_samples), 1))
    strip.putdata(art_samples)
    quantized = strip.quantize(
        colors=color_count,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )

    strip_palette = quantized.getpalette() or []
    write_slot = 1

    for slot in range(color_count):
        if write_slot == BORDER_INDEX:
            write_slot += 1

        shared_palette[write_slot] = tuple(strip_palette[slot * 3:(slot + 1) * 3])
        write_slot += 1

    return shared_palette


def collect_card_palette_samples(
    source_dir: Path,
    files: list[Path],
    key_rgb: tuple[int, int, int],
) -> list[tuple[int, int, int]]:
    opaque_samples: list[tuple[int, int, int]] = []

    for src in files:
        try:
            image = Image.open(src).convert("RGBA")
            stem = src.stem
            profile = resolve_profile(source_dir, stem, image.size)
        except ValueError:
            continue

        if profile not in {Profile.CARD_SPLIT, Profile.CARD_PART}:
            continue

        pixels = image.load()
        width, height = image.size
        for y in range(height):
            for x in range(width):
                if _is_transparent_pixel(pixels[x, y], key_rgb):
                    continue
                opaque_samples.append(pixels[x, y][:3])

    return opaque_samples


def save_gba_bmp(
    rgba_image: Image.Image,
    filepath: Path,
    transparent_key: tuple[int, int, int] | None = None,
    shared_palette: SharedPalette | None = None,
) -> None:
    """Save a 4bpp BMP where palette index 0 is always transparent.

    transparent_key defaults to magenta (#ff00ff) for all sprites.

    When shared_palette is provided (card art), every card BMP uses the same
    16-color palette so the GBA can reuse one OBJ palette slot for all cards.

    Any pixel whose RGB matches the key color is written as index 0.
    All other pixels map into palette slots 1..15.

    PNG alpha is ignored on purpose — fill empty areas with the key color in
    your art tool rather than relying on transparency layers.
    """
    rgba = rgba_image.convert("RGBA")
    px = rgba.load()
    width, height = rgba.size

    if transparent_key is None:
        key_r, key_g, key_b = DEFAULT_CHROMA_KEY
    else:
        key_r, key_g, key_b = transparent_key

    key_rgb = (key_r, key_g, key_b)
    indexed = Image.new("P", (width, height), 0)
    out_px = indexed.load()

    if shared_palette is not None:
        flat_palette = [channel for rgb in shared_palette for channel in rgb]
        indexed.putpalette(flat_palette)

        for y in range(height):
            for x in range(width):
                if _is_transparent_pixel(px[x, y], key_rgb):
                    out_px[x, y] = 0
                else:
                    out_px[x, y] = _nearest_palette_index(px[x, y][:3], shared_palette)

        filepath.parent.mkdir(parents=True, exist_ok=True)
        indexed.save(filepath, format="BMP")
        return

    opaque_samples: list[tuple[int, int, int]] = []
    opaque_positions: list[tuple[int, int]] = []

    for y in range(height):
        for x in range(width):
            if _is_transparent_pixel(px[x, y], key_rgb):
                continue

            rgb = px[x, y][:3]
            opaque_samples.append(rgb)
            opaque_positions.append((x, y))

    palette = [0] * 48
    palette[0:3] = [key_r, key_g, key_b]

    if opaque_samples:
        local_palette = build_shared_palette_from_samples(opaque_samples, key_rgb)
        flat_palette = [channel for rgb in local_palette for channel in rgb]
        indexed.putpalette(flat_palette)

        for index, (x, y) in enumerate(opaque_positions):
            out_px[x, y] = _nearest_palette_index(opaque_samples[index], local_palette)
    else:
        indexed.putpalette(palette)

    filepath.parent.mkdir(parents=True, exist_ok=True)
    indexed.save(filepath, format="BMP")


def accent_palette_item(stem: str) -> str | None:
    if stem.endswith("_accent_top") or stem.endswith("_accent_bottom"):
        return f"{stem.rsplit('_accent_', 1)[0]}_body"
    return None


def write_sprite_json(bmp_path: Path, palette_item: str | None = None) -> None:
    json_path = bmp_path.with_suffix(".json")
    payload: dict[str, str] = {"type": "sprite", "bpp_mode": "bpp_4"}
    if palette_item is not None:
        payload["palette_item"] = palette_item
    json_path.write_text(json.dumps(payload, indent=4) + "\n", encoding="utf-8")


def write_assets(
    assets: list[OutputAsset],
    out_dir: Path,
    shared_card_palette: SharedPalette | None = None,
) -> None:
    for asset in assets:
        bmp_path = out_dir / f"{asset.stem}.bmp"
        palette = shared_card_palette if asset.transparent_key is not None else None
        save_gba_bmp(asset.image, bmp_path, asset.transparent_key, palette)
        write_sprite_json(bmp_path, accent_palette_item(asset.stem))
        width, height = asset.image.size
        key_text = ""
        if asset.transparent_key is not None:
            key_text = f"  key={asset.transparent_key}"
        print(f"  {asset.stem}.bmp  {width}x{height}{key_text}")


def normalize_stem(stem: str, prefix: str) -> str:
    if stem.startswith(prefix):
        return stem
    return f"{prefix}{stem}"


def trinket_output_stem(stem: str) -> str:
    if stem.startswith("hud_trinket_"):
        return stem
    if stem.startswith("trinket_"):
        stem = stem.removeprefix("trinket_")
    return normalize_stem(stem, "hud_trinket_")


def hud_output_stem(stem: str) -> str:
    if stem.startswith("hud_"):
        return stem
    if stem in HUD_SHORT_NAMES:
        return normalize_stem(stem if stem != "turtle" else "turtle_timer", "hud_")
    return normalize_stem(stem, "hud_")


def ui_output_stem(stem: str) -> str:
    if stem.startswith("ui_"):
        return stem
    aliases = {
        "x": "marker_x",
        "shovel": "marker_shovel",
        "graveyard_placeholder": "graveyard_placeholder",
        "edge_fade": "edge_fade",
    }
    if stem in aliases:
        return normalize_stem(aliases[stem], "ui_")
    if stem.startswith("marker_"):
        return normalize_stem(stem, "ui_")
    return normalize_stem(stem, "ui_")


def is_generic_source_folder(folder: Path) -> bool:
    return folder.name.lower() in {"source", "inbox", "input", "all"}


def profile_for_folder(folder: Path) -> Profile | None:
    if is_generic_source_folder(folder):
        return None

    name = folder.name.lower()
    if name in {"cards", "cards_source", "card"}:
        return Profile.CARD_SPLIT
    if name in {"trinkets", "trinket"}:
        return Profile.TRINKET
    if name == "hud":
        return Profile.HUD
    if name == "ui":
        return Profile.UI
    if name in {"fonts", "font"}:
        return Profile.FONT
    if name in {"card_parts", "cards_parts"}:
        return Profile.CARD_PART
    return None


def profile_from_filename(stem: str, size: tuple[int, int]) -> Profile | None:
    lower = stem.lower()

    if lower.startswith("hud_trinket_") or lower.startswith("trinket_") or lower in TRINKET_SHORT_NAMES:
        return Profile.TRINKET
    if lower.startswith("ui_") or lower.startswith("marker_") or lower in UI_SHORT_NAMES:
        return Profile.UI
    if lower.startswith("hud_") or lower in HUD_SHORT_NAMES:
        return Profile.HUD
    if lower.endswith("_body") or lower.endswith("_accent_top") or lower.endswith("_accent_bottom"):
        return Profile.CARD_PART
    if "edge_fade" in lower:
        return Profile.UI
    if lower.endswith("_font") or lower.startswith("font_"):
        return Profile.FONT

    if size == SIZE_HUD:
        return Profile.HUD
    if size == SIZE_UI_PLACEHOLDER:
        return Profile.UI
    if size == SIZE_UI_EDGE_FADE:
        return Profile.UI
    if size == SIZE_CARD_BODY or size == SIZE_CARD_ACCENT:
        return Profile.CARD_PART

    return None


def profile_for_size(stem: str, size: tuple[int, int]) -> Profile:
    if size == (CARD_W, CARD_H):
        return Profile.CARD_SPLIT
    if size == SIZE_CARD_BODY or size == SIZE_CARD_ACCENT:
        return Profile.CARD_PART
    if size == SIZE_HUD:
        return Profile.HUD
    if size == SIZE_UI_PLACEHOLDER or size == SIZE_UI_EDGE_FADE:
        return Profile.UI
    if size[1] in FONT_HEIGHTS and size[0] != CARD_W:
        return Profile.FONT
    raise ValueError(f"unsupported size {size[0]}x{size[1]}")


def resolve_profile(folder: Path, stem: str, size: tuple[int, int]) -> Profile:
    folder_profile = profile_for_folder(folder)
    if folder_profile is not None:
        return folder_profile

    filename_profile = profile_from_filename(stem, size)
    if filename_profile is not None:
        return filename_profile

    return profile_for_size(stem, size)


def card_part_output_stem(stem: str, size: tuple[int, int]) -> str:
    if stem.endswith("_body") or stem.endswith("_accent_top") or stem.endswith("_accent_bottom"):
        return stem
    if size == SIZE_CARD_BODY:
        return f"{stem}_body"
    if size == SIZE_UI_EDGE_FADE and "edge_fade" in stem.lower():
        return ui_output_stem(stem)
    if size == SIZE_CARD_ACCENT:
        raise ValueError(
            f"{stem}: 8x32 accent needs _accent_top or _accent_bottom in the filename "
            "(or use a 40x64 composite instead)"
        )
    raise ValueError(f"{stem}: unrecognized card part size {size[0]}x{size[1]}")


def split_card_image(im: Image.Image, stem: str, key_rgb: tuple[int, int, int]) -> list[OutputAsset]:
    width, height = im.size
    if (width, height) != (CARD_W, CARD_H):
        raise ValueError(f"{stem}: card composite must be {CARD_W}x{CARD_H}, got {width}x{height}")

    return [
        OutputAsset(f"{stem}_body", im.crop((0, 0, BODY_W, CARD_H)), key_rgb),
        OutputAsset(f"{stem}_accent_top", im.crop((BODY_W, 0, CARD_W, ACCENT_H)), key_rgb),
        OutputAsset(
            f"{stem}_accent_bottom",
            im.crop((BODY_W, ACCENT_H, CARD_W, CARD_H)),
            key_rgb,
        ),
    ]


def assets_for_image(
    src: Path,
    profile: Profile,
    source_dir: Path,
    key_rgb: tuple[int, int, int],
) -> list[OutputAsset]:
    im = Image.open(src).convert("RGBA")
    stem = sanitize_asset_stem(src.stem)

    if profile == Profile.CARD_SPLIT:
        print(f"  transparent key RGB={key_rgb}")
        return split_card_image(im, stem, key_rgb)

    if profile == Profile.CARD_PART:
        output_stem = card_part_output_stem(stem, im.size)
        return [OutputAsset(output_stem, im, key_rgb)]

    if profile == Profile.TRINKET:
        if im.size != SIZE_HUD:
            raise ValueError(f"{stem}: trinkets must be 16x16, got {im.size[0]}x{im.size[1]}")
        return [OutputAsset(trinket_output_stem(stem), im)]

    if profile == Profile.HUD:
        if im.size != SIZE_HUD:
            raise ValueError(f"{stem}: HUD icons must be 16x16, got {im.size[0]}x{im.size[1]}")
        return [OutputAsset(hud_output_stem(stem), im)]

    if profile == Profile.UI:
        allowed = {SIZE_HUD, SIZE_UI_PLACEHOLDER, SIZE_UI_EDGE_FADE}
        if im.size not in allowed:
            allowed_text = ", ".join(f"{w}x{h}" for w, h in sorted(allowed))
            raise ValueError(f"{stem}: UI sprite must be one of [{allowed_text}]")
        return [OutputAsset(ui_output_stem(stem), im)]

    if profile == Profile.FONT:
        if im.size[1] not in FONT_HEIGHTS:
            heights = ", ".join(str(value) for value in sorted(FONT_HEIGHTS))
            raise ValueError(f"{stem}: font height must be one of [{heights}], got {im.size[1]}")
        return [OutputAsset(stem, im)]

    raise ValueError(f"unknown profile for {src}")


def default_output_dir(source_dir: Path) -> Path:
    if source_dir == LEGACY_CARDS_SOURCE or source_dir.name.lower() == "cards_source":
        return LEGACY_CARDS_OUTPUT

    if is_generic_source_folder(source_dir) or has_direct_source_files(UNIFIED_SOURCE_DIR):
        return UNIFIED_OUTPUT_DIR

    name = source_dir.name.lower()
    category_map = {
        "cards": UNIFIED_OUTPUT_DIR,
        "trinkets": UNIFIED_OUTPUT_DIR,
        "trinket": UNIFIED_OUTPUT_DIR,
        "hud": UNIFIED_OUTPUT_DIR,
        "ui": UNIFIED_OUTPUT_DIR,
        "fonts": UNIFIED_OUTPUT_DIR,
        "font": UNIFIED_OUTPUT_DIR,
        "card_parts": UNIFIED_OUTPUT_DIR,
        "cards_parts": UNIFIED_OUTPUT_DIR,
    }
    if name in category_map and source_dir.parent.name == "source":
        return category_map[name]

    return UNIFIED_OUTPUT_DIR


def iter_source_files(source_dir: Path) -> list[Path]:
    return sorted(
        path
        for path in source_dir.iterdir()
        if path.is_file() and path.suffix.lower() in {".png", ".bmp"}
    )


def has_direct_source_files(source_dir: Path) -> bool:
    return source_dir.is_dir() and bool(iter_source_files(source_dir))


def convert_folder(
    source_dir: Path,
    out_dir: Path | None = None,
    key_rgb: tuple[int, int, int] = DEFAULT_CHROMA_KEY,
) -> int:
    if not source_dir.is_dir():
        raise SystemExit(f"Not a directory: {source_dir}")

    files = iter_source_files(source_dir)
    if not files:
        print(f"  (no PNG/BMP files in {source_dir})")
        return 0

    output_dir = out_dir or default_output_dir(source_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"{source_dir} -> {output_dir}")
    converted = 0

    card_samples = collect_card_palette_samples(source_dir, files, key_rgb)
    shared_card_palette: SharedPalette | None = None
    if card_samples:
        shared_card_palette = build_shared_palette_from_samples(card_samples, key_rgb)
        unique_colors = len(set(card_samples))
        print(
            f"  shared card palette: 16 slots from {unique_colors} unique color(s) "
            f"across card art (GBA OBJ palette reuse)"
        )

    for src in files:
        try:
            im = Image.open(src).convert("RGBA")
            profile = resolve_profile(source_dir, src.stem, im.size)
            print(f"{src.name}  [{profile.name}, {im.size[0]}x{im.size[1]}]")
            assets = assets_for_image(src, profile, source_dir, key_rgb)
            write_assets(assets, output_dir, shared_card_palette)
            converted += 1
        except ValueError as error:
            print(f"  skip: {error}", file=sys.stderr)

    return converted


def default_source_roots() -> list[Path]:
    if has_direct_source_files(UNIFIED_SOURCE_DIR):
        return [UNIFIED_SOURCE_DIR]

    roots: list[Path] = []
    if UNIFIED_SOURCE_DIR.is_dir():
        for child in sorted(UNIFIED_SOURCE_DIR.iterdir()):
            if child.is_dir():
                roots.append(child)

    if LEGACY_CARDS_SOURCE.is_dir() and LEGACY_CARDS_SOURCE not in roots:
        roots.append(LEGACY_CARDS_SOURCE)

    return roots


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source",
        nargs="*",
        type=Path,
        help="Source folder(s). Default: graphics/source/ (one flat inbox)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        help="Output folder (only valid with a single source folder)",
    )
    parser.add_argument(
        "--key",
        type=str,
        default="ff00ff",
        help="Chroma-key color as RRGGBB (default: ff00ff magenta)",
    )
    args = parser.parse_args()
    key_rgb = parse_chroma_key(args.key)

    sources = args.source if args.source else default_source_roots()
    if not sources:
        raise SystemExit(
            "No source files found. Drop PNG/BMP files into graphics/source/ "
            "or pass a folder explicitly."
        )

    if args.out is not None and len(sources) != 1:
        raise SystemExit("--out requires exactly one source folder")

    total = 0
    for source_dir in sources:
        total += convert_folder(source_dir, args.out, key_rgb)

    print(f"\nDone — {total} source file(s) converted.")
    print("Copy each .bmp + .json pair into your Butano project's graphics/ folder.")


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1) from error
