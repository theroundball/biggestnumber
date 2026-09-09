#!/usr/bin/env python3
"""Pack every card into one sprite sheet, then split it back into art files.

Each card on the GBA is a 40x64 composite:
  32x64 body (left) + 8x32 accent_top (top-right) + 8x32 accent_bottom (bottom-right)

This script:
  pack  — one cell per CardType. Existing body/accent BMPs are composited in;
           cards with no art get a magenta (#ff00ff) slot to paint in.
  split — crop each cell, skip still-empty magenta slots, write 40x64 composites
           and the three layer PNGs. Then run convert_sprites.py for Butano BMPs.

Layout JSON is written next to the PNG so split stays aligned even if you upscale
the sheet to paint (use --scale, or keep the JSON in sync).

Usage (from biggestnumber/):
    pip install pillow
    python tools/card_sprite_sheet.py pack
    python tools/card_sprite_sheet.py pack --scale 4
    python tools/card_sprite_sheet.py split
    python tools/card_sprite_sheet.py split --sheet graphics/card_sheet/card_sprite_sheet.png
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Missing dependency: pip install pillow", file=sys.stderr)
    raise SystemExit(1)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CARD_DATA = ROOT / "src" / "card_data.cpp"
DEFAULT_CARD_TYPES = ROOT / "include" / "card_type.h"
DEFAULT_GRAPHICS_DIRS = (ROOT / "graphics", ROOT / "graphics" / "output")
DEFAULT_SHEET_DIR = ROOT / "graphics" / "card_sheet"
DEFAULT_SHEET_PNG = DEFAULT_SHEET_DIR / "card_sprite_sheet.png"
DEFAULT_SHEET_JSON = DEFAULT_SHEET_DIR / "card_sprite_sheet.json"
DEFAULT_COMPOSITE_DIR = DEFAULT_SHEET_DIR / "composites"
DEFAULT_PARTS_DIR = DEFAULT_SHEET_DIR / "parts"

BODY_W = 32
BODY_H = 64
ACCENT_W = 8
ACCENT_H = 32
CARD_W = 40
CARD_H = 64
CHROMA_KEY = (255, 0, 255)
CHROMA_TOLERANCE = 8
GUTTER_RGB = (32, 32, 32)
LABEL_BG_RGB = (16, 16, 16)
LABEL_FG_RGB = (230, 230, 230)
GUIDE_RGB = (255, 126, 0)
LAYOUT_VERSION = 1

SPRITE_ALIASES: dict[str, list[str]] = {
    "bigkurosawaburger": ["bigkurosawaburger", "bigkurasawaburger"],
    "snail_mail": ["snail_mail", "snailmail"],
}


@dataclass(frozen=True)
class CardRecord:
    enum_name: str
    name: str
    description: str
    sprite_slug: str | None


def parse_chroma_key(text: str) -> tuple[int, int, int]:
    cleaned = text.strip().lstrip("#")
    if not re.fullmatch(r"[0-9a-fA-F]{6}", cleaned):
        raise ValueError(f"invalid key color {text!r}; use RRGGBB like ff00ff")
    return (
        int(cleaned[0:2], 16),
        int(cleaned[2:4], 16),
        int(cleaned[4:6], 16),
    )


def rgb_to_hex(rgb: tuple[int, int, int]) -> str:
    return f"#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}"


def parse_card_type_names(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"enum\s+class\s+CardType\s*\{([^}]*)\}", text, re.DOTALL)
    if not match:
        raise ValueError(f"Could not find CardType enum in {path}")

    names: list[str] = []
    for line in match.group(1).splitlines():
        line = line.split("//", 1)[0].strip()
        if not line or line.startswith("//"):
            continue
        token = line.rstrip(",").strip()
        if token == "COUNT":
            continue
        names.append(token)
    return names


def extract_make_card_calls(source: str) -> list[str]:
    calls: list[str] = []
    marker = "make_card("
    index = 0
    while True:
        start = source.find(marker, index)
        if start == -1:
            break

        depth = 0
        cursor = start + len(marker) - 1
        while cursor < len(source):
            char = source[cursor]
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    calls.append(source[start : cursor + 1])
                    index = cursor + 1
                    break
            cursor += 1
        else:
            raise ValueError("Unterminated make_card(...) call in card_data.cpp")
    return calls


def parse_quoted_strings(fragment: str) -> list[str]:
    strings: list[str] = []
    index = 0
    while index < len(fragment):
        if fragment[index] != '"':
            index += 1
            continue

        index += 1
        chars: list[str] = []
        while index < len(fragment):
            char = fragment[index]
            if char == "\\" and index + 1 < len(fragment):
                chars.append(fragment[index + 1])
                index += 2
            elif char == '"':
                strings.append("".join(chars))
                index += 1
                break
            else:
                chars.append(char)
                index += 1
    return strings


def parse_card_records(card_data_path: Path, card_types_path: Path) -> list[CardRecord]:
    source = card_data_path.read_text(encoding="utf-8")
    table_start = source.find("static const CardData table[]")
    if table_start == -1:
        raise ValueError("Could not find card_data table in card_data.cpp")

    calls = extract_make_card_calls(source[table_start:])
    enum_names = parse_card_type_names(card_types_path)
    if len(calls) != len(enum_names):
        raise ValueError(
            f"Card count mismatch: {len(enum_names)} CardType entries vs "
            f"{len(calls)} make_card rows"
        )

    records: list[CardRecord] = []
    for enum_name, call in zip(enum_names, calls):
        strings = parse_quoted_strings(call)
        if len(strings) < 2:
            raise ValueError(f"Could not parse name/description for {enum_name}")

        sprite_match = re.search(r"CARD_SPRITES\((\w+)\)", call)
        records.append(
            CardRecord(
                enum_name=enum_name,
                name=strings[0],
                description=strings[1],
                sprite_slug=sprite_match.group(1) if sprite_match else None,
            )
        )
    return records


def sanitize_stem(stem: str) -> str:
    normalized = stem.lower().replace("-", "_").replace(" ", "_")
    normalized = re.sub(r"[^a-z0-9_]+", "_", normalized)
    normalized = re.sub(r"_+", "_", normalized).strip("_")
    if not normalized:
        raise ValueError(f"invalid asset name: {stem!r}")
    return normalized


def resolve_sprite_stems(slug: str) -> list[str]:
    return SPRITE_ALIASES.get(slug, [slug])


def unique_export_stems(records: list[CardRecord]) -> list[str]:
    """One filename stem per card. Shared CARD_SPRITES keep the slug on the first card."""
    slug_first_index: dict[str, int] = {}
    stems: list[str] = []
    for index, record in enumerate(records):
        if record.sprite_slug:
            users = [i for i, other in enumerate(records) if other.sprite_slug == record.sprite_slug]
            if len(users) == 1 or slug_first_index.setdefault(record.sprite_slug, index) == index:
                stems.append(sanitize_stem(resolve_sprite_stems(record.sprite_slug)[0]))
            else:
                stems.append(sanitize_stem(record.enum_name))
        else:
            stems.append(sanitize_stem(record.enum_name))
    return stems


def _is_chroma(red: int, green: int, blue: int, key: tuple[int, int, int] = CHROMA_KEY) -> bool:
    key_r, key_g, key_b = key
    return (
        abs(red - key_r) <= CHROMA_TOLERANCE
        and abs(green - key_g) <= CHROMA_TOLERANCE
        and abs(blue - key_b) <= CHROMA_TOLERANCE
    )


def load_rgba(path: Path) -> Image.Image:
    image = Image.open(path)
    if image.mode != "RGBA":
        image = image.convert("RGBA")

    pixels = image.load()
    width, height = image.size
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0 or _is_chroma(red, green, blue):
                pixels[x, y] = (red, green, blue, 0)
    return image


def find_layer_path(graphics_dirs: tuple[Path, ...], stem: str, layer: str) -> Path | None:
    for directory in graphics_dirs:
        if not directory.is_dir():
            continue
        for extension in (".bmp", ".png"):
            candidate = directory / f"{stem}_{layer}{extension}"
            if candidate.is_file():
                return candidate
    return None


def find_composite_path(stem: str, extra_dirs: tuple[Path, ...]) -> Path | None:
    for directory in extra_dirs:
        if not directory.is_dir():
            continue
        for extension in (".png", ".bmp"):
            candidate = directory / f"{stem}{extension}"
            if candidate.is_file():
                return candidate
    return None


def composite_card_sprite(
    slug: str,
    graphics_dirs: tuple[Path, ...],
    extra_composite_dirs: tuple[Path, ...] = (),
) -> Image.Image | None:
    for stem in resolve_sprite_stems(slug):
        composite_path = find_composite_path(stem, extra_composite_dirs)
        if composite_path:
            image = load_rgba(composite_path)
            if image.size != (CARD_W, CARD_H):
                image = image.resize((CARD_W, CARD_H), Image.Resampling.NEAREST)
            return image

        body_path = find_layer_path(graphics_dirs, stem, "body")
        if not body_path:
            continue

        canvas = Image.new("RGBA", (CARD_W, CARD_H), (0, 0, 0, 0))
        body = load_rgba(body_path)
        canvas.paste(body, (0, 0), body)

        accent_top_path = find_layer_path(graphics_dirs, stem, "accent_top")
        if accent_top_path:
            accent_top = load_rgba(accent_top_path)
            canvas.paste(accent_top, (BODY_W, 0), accent_top)

        accent_bottom_path = find_layer_path(graphics_dirs, stem, "accent_bottom")
        if accent_bottom_path:
            accent_bottom = load_rgba(accent_bottom_path)
            canvas.paste(accent_bottom, (BODY_W, ACCENT_H), accent_bottom)

        return canvas

    return None


def chroma_canvas(size: tuple[int, int], key: tuple[int, int, int] = CHROMA_KEY) -> Image.Image:
    return Image.new("RGBA", size, (*key, 255))


def paste_on_chroma(art: Image.Image, key: tuple[int, int, int] = CHROMA_KEY) -> Image.Image:
    canvas = chroma_canvas(art.size, key)
    if art.mode != "RGBA":
        art = art.convert("RGBA")
    canvas.paste(art, (0, 0), art)
    return canvas


def split_card_layers(composite: Image.Image) -> tuple[Image.Image, Image.Image, Image.Image]:
    if composite.size != (CARD_W, CARD_H):
        raise ValueError(f"composite must be {CARD_W}x{CARD_H}, got {composite.size}")
    body = composite.crop((0, 0, BODY_W, CARD_H))
    accent_top = composite.crop((BODY_W, 0, CARD_W, ACCENT_H))
    accent_bottom = composite.crop((BODY_W, ACCENT_H, CARD_W, CARD_H))
    return body, accent_top, accent_bottom


def _is_guide_pixel(red: int, green: int, blue: int, key: tuple[int, int, int]) -> bool:
    if _is_chroma(red, green, blue, key):
        return True
    # Empty-slot hints: orange frame + slightly-off magenta split lines.
    if abs(red - GUIDE_RGB[0]) <= 8 and abs(green - GUIDE_RGB[1]) <= 8 and abs(blue - GUIDE_RGB[2]) <= 8:
        return True
    if red >= 250 and green <= 16 and blue >= 230:
        return True
    return False


def cell_is_empty(cell: Image.Image, key: tuple[int, int, int] = CHROMA_KEY) -> bool:
    pixels = cell.convert("RGBA").load()
    width, height = cell.size
    painted = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha < 16:
                continue
            if _is_guide_pixel(red, green, blue, key):
                continue
            painted += 1
            if painted > 4:
                return False
    return True


def load_label_font(pixel_size: int) -> ImageFont.ImageFont:
    candidates = (
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    )
    for path in candidates:
        try:
            return ImageFont.truetype(path, max(8, pixel_size))
        except OSError:
            continue
    return ImageFont.load_default()


def draw_empty_guides(cell: Image.Image, scale: int) -> None:
    """Light layout hints on empty slots; still chroma-key so split treats them empty."""
    draw = ImageDraw.Draw(cell)
    width, height = cell.size
    split_x = BODY_W * scale
    mid_y = ACCENT_H * scale
    # Keep guides on chroma so empty detection still works: use a slightly-off magenta.
    guide = (255, 0, 240)
    draw.line([(split_x, 0), (split_x, height - 1)], fill=(*guide, 255))
    draw.line([(split_x, mid_y), (width - 1, mid_y)], fill=(*guide, 255))
    inset = max(1, scale)
    draw.rectangle(
        [inset, inset, width - 1 - inset, height - 1 - inset],
        outline=(*GUIDE_RGB, 255),
    )


def label_text(record: CardRecord, stem: str) -> str:
    if record.sprite_slug and sanitize_stem(record.sprite_slug) != stem:
        return f"{record.name} [{stem}]"
    return record.name


def pack_sheet(
    records: list[CardRecord],
    stems: list[str],
    graphics_dirs: tuple[Path, ...],
    extra_composite_dirs: tuple[Path, ...],
    columns: int,
    scale: int,
    gutter: int,
    label_h: int,
    key: tuple[int, int, int],
) -> tuple[Image.Image, dict]:
    count = len(records)
    columns = max(1, columns)
    rows = (count + columns - 1) // columns
    cell_w = CARD_W * scale
    cell_h = CARD_H * scale
    label_h = max(0, label_h)
    step_x = cell_w + gutter
    step_y = cell_h + label_h + gutter
    sheet_w = gutter + columns * step_x
    sheet_h = gutter + rows * step_y
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (*GUTTER_RGB, 255))
    draw = ImageDraw.Draw(sheet)
    font = load_label_font(max(8, label_h - 4) if label_h else 10)

    cards_meta: list[dict] = []
    filled = 0
    empty = 0

    for index, (record, stem) in enumerate(zip(records, stems)):
        col = index % columns
        row = index // columns
        x = gutter + col * step_x
        y = gutter + row * step_y

        art = None
        if record.sprite_slug:
            art = composite_card_sprite(record.sprite_slug, graphics_dirs, extra_composite_dirs)
        if art is None:
            art = composite_card_sprite(stem, graphics_dirs, extra_composite_dirs)

        had_art = art is not None
        if art is None:
            cell = chroma_canvas((cell_w, cell_h), key)
            draw_empty_guides(cell, scale)
            empty += 1
        else:
            if art.size != (CARD_W, CARD_H):
                art = art.resize((CARD_W, CARD_H), Image.Resampling.NEAREST)
            cell = paste_on_chroma(art, key)
            if scale != 1:
                cell = cell.resize((cell_w, cell_h), Image.Resampling.NEAREST)
            filled += 1

        sheet.paste(cell, (x, y))

        if label_h > 0:
            label_box = (x, y + cell_h, x + cell_w, y + cell_h + label_h)
            draw.rectangle(label_box, fill=(*LABEL_BG_RGB, 255))
            text = label_text(record, stem)
            text_y = y + cell_h + max(0, (label_h - (font.size if hasattr(font, "size") else 10)) // 2)
            draw.text((x + 2, text_y), text, fill=(*LABEL_FG_RGB, 255), font=font)

        cards_meta.append(
            {
                "index": index,
                "enum": record.enum_name,
                "name": record.name,
                "slug": stem,
                "sprite_slug": record.sprite_slug,
                "col": col,
                "row": row,
                "had_art": had_art,
            }
        )

    layout = {
        "version": LAYOUT_VERSION,
        "card_w": CARD_W,
        "card_h": CARD_H,
        "scale": scale,
        "columns": columns,
        "rows": rows,
        "gutter": gutter,
        "label_h": label_h,
        "chroma": rgb_to_hex(key),
        "cards": cards_meta,
        "filled": filled,
        "empty": empty,
    }
    return sheet, layout


def layout_json_path(sheet_path: Path) -> Path:
    return sheet_path.with_suffix(".json")


def save_layout(path: Path, layout: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(layout, indent=2) + "\n", encoding="utf-8")


def load_layout(path: Path) -> dict:
    layout = json.loads(path.read_text(encoding="utf-8"))
    if int(layout.get("version", 0)) != LAYOUT_VERSION:
        raise ValueError(f"unsupported sheet layout version in {path}")
    return layout


def infer_layout_from_sheet(sheet: Image.Image, card_count: int, columns: int, scale: int, gutter: int, label_h: int) -> dict:
    columns = max(1, columns)
    rows = (card_count + columns - 1) // columns
    return {
        "version": LAYOUT_VERSION,
        "card_w": CARD_W,
        "card_h": CARD_H,
        "scale": scale,
        "columns": columns,
        "rows": rows,
        "gutter": gutter,
        "label_h": label_h,
        "chroma": rgb_to_hex(CHROMA_KEY),
        "cards": [],
    }


def crop_cell(sheet: Image.Image, col: int, row: int, layout: dict) -> Image.Image:
    scale = int(layout["scale"])
    gutter = int(layout["gutter"])
    label_h = int(layout["label_h"])
    cell_w = int(layout["card_w"]) * scale
    cell_h = int(layout["card_h"]) * scale
    step_x = cell_w + gutter
    step_y = cell_h + label_h + gutter
    x = gutter + col * step_x
    y = gutter + row * step_y
    cell = sheet.crop((x, y, x + cell_w, y + cell_h))
    if scale != 1:
        cell = cell.resize((CARD_W, CARD_H), Image.Resampling.NEAREST)
    return cell.convert("RGBA")


def write_png(path: Path, image: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG")


def cmd_pack(args: argparse.Namespace) -> int:
    records = parse_card_records(args.card_data, args.card_types)
    stems = unique_export_stems(records)
    graphics_dirs = tuple(args.graphics) if args.graphics else DEFAULT_GRAPHICS_DIRS
    extra_dirs = (DEFAULT_COMPOSITE_DIR, ROOT / "graphics" / "source" / "cards")
    key = parse_chroma_key(args.key) if args.key else CHROMA_KEY

    sheet, layout = pack_sheet(
        records,
        stems,
        graphics_dirs,
        extra_dirs,
        columns=args.columns,
        scale=max(1, args.scale),
        gutter=args.gutter,
        label_h=args.label_height,
        key=key,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output, format="PNG")
    json_path = args.layout or layout_json_path(args.output)
    save_layout(json_path, layout)

    print(
        f"Wrote {args.output} ({sheet.size[0]}x{sheet.size[1]}, "
        f"{layout['columns']}x{layout['rows']} cells, scale={layout['scale']})"
    )
    print(f"Wrote {json_path}")
    print(f"{layout['filled']} cards with art, {layout['empty']} empty slots")
    shared = [
        f"{record.name} -> {stem} (currently CARD_SPRITES({record.sprite_slug}))"
        for record, stem in zip(records, stems)
        if record.sprite_slug and stem != sanitize_stem(record.sprite_slug)
    ]
    if shared:
        print("Unique slots for cards that currently reuse another card's art:")
        for line in shared:
            print(f"  {line}")
    return 0


def cmd_split(args: argparse.Namespace) -> int:
    sheet_path = args.sheet
    if not sheet_path.is_file():
        raise SystemExit(f"Sheet not found: {sheet_path}")

    json_path = args.layout or layout_json_path(sheet_path)
    records = parse_card_records(args.card_data, args.card_types)
    stems = unique_export_stems(records)
    sheet = Image.open(sheet_path).convert("RGBA")
    key = parse_chroma_key(args.key) if args.key else CHROMA_KEY

    if json_path.is_file():
        layout = load_layout(json_path)
    else:
        print(f"No layout JSON at {json_path}; using --columns/--scale/--gutter/--label-height")
        layout = infer_layout_from_sheet(
            sheet,
            len(records),
            args.columns,
            max(1, args.scale),
            args.gutter,
            args.label_height,
        )

    if layout.get("cards"):
        entries = layout["cards"]
        if len(entries) != len(records):
            print(
                f"Warning: layout has {len(entries)} cells, card_data has {len(records)}; "
                "using min count",
                file=sys.stderr,
            )
    else:
        columns = int(layout["columns"])
        entries = [
            {
                "index": index,
                "enum": record.enum_name,
                "name": record.name,
                "slug": stem,
                "col": index % columns,
                "row": index // columns,
            }
            for index, (record, stem) in enumerate(zip(records, stems))
        ]

    composite_dir = args.composites
    parts_dir = args.parts
    exported = 0
    skipped_empty = 0
    skipped_force = 0

    for entry, record, stem in zip(entries, records, stems):
        slug = entry.get("slug") or stem
        col = int(entry["col"])
        row = int(entry["row"])
        cell = crop_cell(sheet, col, row, layout)
        empty = cell_is_empty(cell, key)
        if empty and not args.include_empty:
            skipped_empty += 1
            continue

        if empty:
            skipped_force += 1

        cell = paste_on_chroma(cell, key)
        write_png(composite_dir / f"{slug}.png", cell)
        body, accent_top, accent_bottom = split_card_layers(cell)
        write_png(parts_dir / f"{slug}_body.png", body)
        write_png(parts_dir / f"{slug}_accent_top.png", accent_top)
        write_png(parts_dir / f"{slug}_accent_bottom.png", accent_bottom)
        exported += 1

        if record.sprite_slug and slug != sanitize_stem(record.sprite_slug):
            print(
                f"  {record.name}: wrote {slug}.png "
                f"(card_data still uses CARD_SPRITES({record.sprite_slug}))"
            )

    print(f"Exported {exported} card(s) to {composite_dir} and {parts_dir}")
    if skipped_empty:
        print(f"Skipped {skipped_empty} empty magenta slot(s) (pass --include-empty to write them)")

    if args.convert and exported:
        convert_script = Path(__file__).with_name("convert_sprites.py")
        if not convert_script.is_file():
            print(f"convert_sprites.py not found at {convert_script}", file=sys.stderr)
            return 1
        import runpy

        out_dir = args.butano_out
        previous_argv = sys.argv
        sys.argv = [str(convert_script), str(composite_dir), "--out", str(out_dir), "--key", rgb_to_hex(key)[1:]]
        print(f"Running convert_sprites.py {composite_dir} --out {out_dir}")
        try:
            runpy.run_path(str(convert_script), run_name="__main__")
        finally:
            sys.argv = previous_argv

    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    pack = sub.add_parser("pack", help="Build a sprite sheet of every card")
    pack.add_argument("--card-data", type=Path, default=DEFAULT_CARD_DATA)
    pack.add_argument("--card-types", type=Path, default=DEFAULT_CARD_TYPES)
    pack.add_argument("--graphics", type=Path, action="append", default=[])
    pack.add_argument("--output", type=Path, default=DEFAULT_SHEET_PNG)
    pack.add_argument("--layout", type=Path, default=None)
    pack.add_argument("--columns", type=int, default=10)
    pack.add_argument("--scale", type=int, default=4, help="Nearest-neighbor upscale for painting (default: 4)")
    pack.add_argument("--gutter", type=int, default=8)
    pack.add_argument("--label-height", type=int, default=16)
    pack.add_argument("--key", type=str, default="ff00ff")

    split = sub.add_parser("split", help="Cut the sheet back into 40x64 cards and 3 layer sprites")
    split.add_argument("--sheet", type=Path, default=DEFAULT_SHEET_PNG)
    split.add_argument("--layout", type=Path, default=None)
    split.add_argument("--card-data", type=Path, default=DEFAULT_CARD_DATA)
    split.add_argument("--card-types", type=Path, default=DEFAULT_CARD_TYPES)
    split.add_argument("--composites", type=Path, default=DEFAULT_COMPOSITE_DIR)
    split.add_argument("--parts", type=Path, default=DEFAULT_PARTS_DIR)
    split.add_argument("--include-empty", action="store_true")
    split.add_argument("--convert", action="store_true", help="Also run convert_sprites.py on composites")
    split.add_argument("--butano-out", type=Path, default=ROOT / "graphics" / "output")
    split.add_argument("--columns", type=int, default=10)
    split.add_argument("--scale", type=int, default=4)
    split.add_argument("--gutter", type=int, default=8)
    split.add_argument("--label-height", type=int, default=16)
    split.add_argument("--key", type=str, default="ff00ff")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.command == "pack":
        return cmd_pack(args)
    if args.command == "split":
        return cmd_split(args)
    parser.error("unknown command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
