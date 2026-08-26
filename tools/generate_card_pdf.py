#!/usr/bin/env python3
"""
Build a printable PDF of every card from card_data.cpp.

Cards with CARD_SPRITES(...) get their body + accent layers composited from
graphics/*.bmp. Text-only cards (no CARD_SPRITES) render name + description only.

Usage (from biggestnumber/):
    pip install pillow fpdf2
    python tools/generate_card_pdf.py
    python tools/generate_card_pdf.py --output docs/cards.pdf --scale 4
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from io import BytesIO
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Missing dependency: pip install pillow", file=sys.stderr)
    raise SystemExit(1)

try:
    from fpdf import FPDF
except ImportError:
    print("Missing dependency: pip install fpdf2", file=sys.stderr)
    raise SystemExit(1)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CARD_DATA = ROOT / "src" / "card_data.cpp"
DEFAULT_CARD_TYPES = ROOT / "include" / "card_type.h"
DEFAULT_GRAPHICS_DIRS = (ROOT / "graphics", ROOT / "graphics" / "output")
DEFAULT_OUTPUT = ROOT / "docs" / "cards.pdf"

BODY_W = 32
BODY_H = 64
ACCENT_W = 8
ACCENT_H = 32
CARD_W = 40
CARD_H = 64
CHROMA_KEY = (255, 0, 255)
CHROMA_TOLERANCE = 8

# Known asset slug aliases (CARD_SPRITES name -> on-disk stem).
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


def _is_chroma(red: int, green: int, blue: int) -> bool:
    key_r, key_g, key_b = CHROMA_KEY
    return (
        abs(red - key_r) <= CHROMA_TOLERANCE
        and abs(green - key_g) <= CHROMA_TOLERANCE
        and abs(blue - key_b) <= CHROMA_TOLERANCE
    )


def load_rgba(path: Path) -> Image.Image:
    image = Image.open(path)
    if image.mode == "P":
        image = image.convert("RGBA")
    elif image.mode != "RGBA":
        image = image.convert("RGBA")

    pixels = image.load()
    width, height = image.size
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0 or _is_chroma(red, green, blue):
                pixels[x, y] = (red, green, blue, 0)
    return image


def resolve_sprite_stems(slug: str) -> list[str]:
    return SPRITE_ALIASES.get(slug, [slug])


def find_layer_path(graphics_dirs: tuple[Path, ...], stem: str, layer: str) -> Path | None:
    for directory in graphics_dirs:
        if not directory.is_dir():
            continue
        for extension in (".bmp", ".png"):
            candidate = directory / f"{stem}_{layer}{extension}"
            if candidate.is_file():
                return candidate
    return None


def composite_card_sprite(slug: str, graphics_dirs: tuple[Path, ...]) -> Image.Image | None:
    body_path = accent_top_path = accent_bottom_path = None
    for stem in resolve_sprite_stems(slug):
        body_path = find_layer_path(graphics_dirs, stem, "body")
        accent_top_path = find_layer_path(graphics_dirs, stem, "accent_top")
        accent_bottom_path = find_layer_path(graphics_dirs, stem, "accent_bottom")
        if body_path:
            break

    if not body_path:
        return None

    canvas = Image.new("RGBA", (CARD_W, CARD_H), (0, 0, 0, 0))
    body = load_rgba(body_path)
    canvas.paste(body, (0, 0), body)

    if accent_top_path:
        accent_top = load_rgba(accent_top_path)
        canvas.paste(accent_top, (BODY_W, 0), accent_top)

    if accent_bottom_path:
        accent_bottom = load_rgba(accent_bottom_path)
        canvas.paste(accent_bottom, (BODY_W, ACCENT_H), accent_bottom)

    return canvas


class CardPdf(FPDF):
    def footer(self) -> None:
        self.set_y(-12)
        self.set_font("Helvetica", "", 8)
        self.set_text_color(120, 120, 120)
        self.cell(0, 8, f"Page {self.page_no()}/{{nb}}", align="C")


def wrap_text(pdf: FPDF, text: str, width_mm: float) -> list[str]:
    words = text.split()
    if not words:
        return [""]

    lines: list[str] = []
    current = words[0]
    for word in words[1:]:
        trial = f"{current} {word}"
        if pdf.get_string_width(trial) <= width_mm:
            current = trial
        else:
            lines.append(current)
            current = word
    lines.append(current)
    return lines


def truncate_to_width(pdf: FPDF, text: str, width_mm: float) -> str:
    if pdf.get_string_width(text) <= width_mm:
        return text
    ellipsis = "..."
    while text and pdf.get_string_width(text + ellipsis) > width_mm:
        text = text[:-1]
    return text + ellipsis if text else ellipsis


def draw_card_cell(
    pdf: FPDF,
    x: float,
    y: float,
    width: float,
    height: float,
    record: CardRecord,
    sprite_image: Image.Image | None,
    scale: int,
) -> None:
    padding = 2.0
    content_x = x + padding
    content_y = y + padding
    content_w = width - padding * 2
    content_h = height - padding * 2
    cursor_y = content_y

    if sprite_image is not None:
        max_sprite_w = min(content_w, (CARD_W * scale) * 25.4 / 96.0)
        max_sprite_h = content_h * 0.42
        sprite_w_mm = max_sprite_w
        sprite_h_mm = sprite_w_mm * (CARD_H / CARD_W)
        if sprite_h_mm > max_sprite_h:
            sprite_h_mm = max_sprite_h
            sprite_w_mm = sprite_h_mm * (CARD_W / CARD_H)

        scaled = sprite_image.resize(
            (max(1, int(sprite_w_mm * 96 / 25.4)), max(1, int(sprite_h_mm * 96 / 25.4))),
            Image.Resampling.NEAREST,
        )
        buffer = BytesIO()
        scaled.save(buffer, format="PNG")
        buffer.seek(0)
        sprite_x = content_x + (content_w - sprite_w_mm) / 2
        pdf.image(buffer, x=sprite_x, y=cursor_y, w=sprite_w_mm, h=sprite_h_mm)
        cursor_y += sprite_h_mm + 1.5

    pdf.set_xy(content_x, cursor_y)
    pdf.set_font("Helvetica", "B", 8.5)
    pdf.set_text_color(20, 20, 20)
    pdf.cell(content_w, 4, truncate_to_width(pdf, record.name, content_w), new_x="LMARGIN", new_y="NEXT")

    cursor_y = pdf.get_y() + 0.5
    remaining_h = y + height - padding - cursor_y
    line_h = 3.2
    max_lines = max(1, int(remaining_h // line_h))

    pdf.set_xy(content_x, cursor_y)
    pdf.set_font("Helvetica", "", 7)
    pdf.set_text_color(55, 55, 55)
    lines = wrap_text(pdf, record.description, content_w)[:max_lines]
    if len(wrap_text(pdf, record.description, content_w)) > max_lines:
        if lines:
            lines[-1] = truncate_to_width(pdf, lines[-1], content_w - pdf.get_string_width("…")) + "…"

    for line in lines:
        pdf.set_x(content_x)
        pdf.cell(content_w, line_h, line, new_x="LMARGIN", new_y="NEXT")


def render_pdf(
    records: list[CardRecord],
    output_path: Path,
    graphics_dirs: tuple[Path, ...],
    scale: int,
    columns: int,
    rows: int,
) -> tuple[int, int]:
    pdf = CardPdf(format="Letter", unit="mm")
    pdf.set_auto_page_break(auto=False)
    pdf.alias_nb_pages()
    pdf.set_margins(12, 12, 14)

    page_width = pdf.w - pdf.l_margin - pdf.r_margin
    page_height = pdf.h - pdf.t_margin - pdf.b_margin - 8
    gutter_x = 4.0
    gutter_y = 3.0
    cell_width = (page_width - gutter_x * (columns - 1)) / columns
    cell_height = (page_height - gutter_y * (rows - 1)) / rows
    cards_per_page = columns * rows

    missing_sprites = 0
    with_sprites = 0

    for page_start in range(0, len(records), cards_per_page):
        pdf.add_page()
        page_records = records[page_start : page_start + cards_per_page]

        for index, record in enumerate(page_records):
            col = index % columns
            row = index // columns
            cell_x = pdf.l_margin + col * (cell_width + gutter_x)
            cell_y = pdf.t_margin + row * (cell_height + gutter_y)

            sprite_image: Image.Image | None = None
            if record.sprite_slug:
                sprite_image = composite_card_sprite(record.sprite_slug, graphics_dirs)
                if sprite_image:
                    with_sprites += 1
                else:
                    missing_sprites += 1

            draw_card_cell(
                pdf,
                cell_x,
                cell_y,
                cell_width,
                cell_height,
                record,
                sprite_image,
                scale,
            )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    pdf.output(str(output_path))
    return with_sprites, missing_sprites


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a PDF card reference from game data.")
    parser.add_argument(
        "--card-data",
        type=Path,
        default=DEFAULT_CARD_DATA,
        help=f"Path to card_data.cpp (default: {DEFAULT_CARD_DATA.relative_to(ROOT)})",
    )
    parser.add_argument(
        "--card-types",
        type=Path,
        default=DEFAULT_CARD_TYPES,
        help=f"Path to card_type.h (default: {DEFAULT_CARD_TYPES.relative_to(ROOT)})",
    )
    parser.add_argument(
        "--graphics",
        type=Path,
        action="append",
        default=[],
        help="Directory to search for sprite BMP/PNG (repeatable)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output PDF path (default: {DEFAULT_OUTPUT.relative_to(ROOT)})",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=2,
        help="Nearest-neighbor upscale for card sprites (default: 2)",
    )
    parser.add_argument(
        "--columns",
        type=int,
        default=2,
        help="Grid columns per page (default: 2)",
    )
    parser.add_argument(
        "--rows",
        type=int,
        default=0,
        help="Grid rows per page (default: derived from --per-page)",
    )
    parser.add_argument(
        "--per-page",
        type=int,
        choices=(6, 8),
        default=8,
        help="Cards per page: 8 = 2x4 grid, 6 = 2x3 grid (default: 8)",
    )
    args = parser.parse_args()

    graphics_dirs = tuple(args.graphics) if args.graphics else DEFAULT_GRAPHICS_DIRS
    records = parse_card_records(args.card_data, args.card_types)

    sprite_cards = sum(1 for record in records if record.sprite_slug)
    text_only_cards = len(records) - sprite_cards
    print(f"Parsed {len(records)} cards ({sprite_cards} with art, {text_only_cards} text-only)")

    columns = max(1, args.columns)
    rows = args.rows if args.rows > 0 else args.per_page // columns

    with_sprites, missing_sprites = render_pdf(
        records,
        args.output,
        graphics_dirs,
        max(1, args.scale),
        columns,
        max(1, rows),
    )

    if missing_sprites:
        print(
            f"Warning: {missing_sprites} sprite card(s) missing BMP/PNG layers under:",
            file=sys.stderr,
        )
        for directory in graphics_dirs:
            print(f"  - {directory}", file=sys.stderr)

    print(f"Wrote {args.output} ({with_sprites} sprites embedded)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
