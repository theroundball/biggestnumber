#!/usr/bin/env python3
"""
Build a PDF of every card grouped by verb / mechanical role.

Cards with sprites get composited art; text-only cards show name + description.

Usage (from repo root):
    pip install pillow fpdf2
    python tools/generate_verb_card_pdf.py
    python tools/generate_verb_card_pdf.py --output docs/cards_by_verb.pdf
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from fpdf import FPDF
except ImportError:
    print("Missing dependency: pip install fpdf2", file=sys.stderr)
    raise SystemExit(1)

from generate_card_pdf import (
    DEFAULT_CARD_DATA,
    DEFAULT_CARD_TYPES,
    DEFAULT_GRAPHICS_DIRS,
    ROOT,
    CardRecord,
    CardPdf,
    composite_card_sprite,
    draw_card_cell,
    parse_card_records,
)

# (section title, short blurb, enum names in display order)
VERB_SECTIONS: list[tuple[str, str, list[str]]] = [
    (
        "Transport (+N ladder)",
        "Immediate round score. Higher transport = bigger +N. Strong upgrade targets (+digit concat).",
        [
            "LONGBOARD",
            "HEELYS",
            "SCOOTER",
            "SKATEBOARD",
            "ROLLER_BLADES",
            "WAGON",
            "STOLLER",
            "RIP_STICK",
            "BIKE",
        ],
    ),
    (
        "Combo - Rock / Paper / Scissors / Shoot",
        "No solo play effect. All four together: ×4 total score (cinematic).",
        ["ROCK", "PAPER", "SCISSORS", "SHOOT"],
    ),
    (
        "Combo - Peanut Butter & Jelly",
        "No solo play effect. Pair together: ×2 total score.",
        ["PEANUT_BUTTER", "JELLY"],
    ),
    (
        "Combo - Straw / Sticks / Bricks",
        "No solo play effect. Trio together: ×3 total score.",
        ["STRAW", "STICKS", "BRICKS"],
    ),
    (
        "Future seeds & round setup",
        "Schedule +N, ×N, or draw changes on upcoming rounds.",
        [
            "SIPS",
            "SNAIL_MAIL",
            "TIME_IS_TOO_EXPENSIVE",
            "TIME_IS_MONEY",
            "DEAD_RISING",
            "SEVEN_FEET_DEEP",
        ],
    ),
    (
        "Round timing & flow",
        "Delay commits, pull modifiers early, or end the run on empty hand.",
        ["TURTLE_MODE", "EVALUATE", "FINALE"],
    ),
    (
        "Draw & cycling",
        "Refill hand or exile-to-draw without a graveyard focus.",
        ["WISHES", "CATNIP", "CYCLE", "CYCLE_SEVEN", "SOLO"],
    ),
    (
        "Multipliers & costs",
        "Big ×N swings that cost discards, graveyard cards, or hand depth.",
        ["CLOVER", "BIG_KUROSAWA_BURGER", "OVERCLOCK", "TOPPINGS", "TRIPTYCH"],
    ),
    (
        "Graveyard - scaling & triggers",
        "Score from graveyard size, discards, or cards played this round.",
        ["BONES", "BUSTED", "THRESHOLD", "TOMBSTONES", "JOURNAL"],
    ),
    (
        "Graveyard - manipulation & retrieval",
        "Move, exile, shuffle, or return graveyard cards.",
        [
            "JACKS",
            "FISHING_POLE",
            "SHELLS",
            "ROLL_OVER",
            "LIFELINE",
            "NECROMANCY",
            "RAGS_TO_RICHES",
            "BIRDS_OF_A_FEATHER",
        ],
    ),
    (
        "Deck search & topdeck",
        "Peek, reorder, play from deck top, or mill until a hit.",
        [
            "SWIVEL",
            "HACKER",
            "LIBRARIAN",
            "PILOT",
            "MIRACLE",
            "SPECULATIVE",
            "FLEX",
        ],
    ),
    (
        "Digit & score shape",
        "Move, swap, replace, or round digits in total / round score.",
        [
            "SWAP",
            "THE_FOURTH",
            "THE_FIFTH",
            "PALINDROME",
            "BUILD_A_NUMBER",
            "MINOR_FALL",
            "MAJOR_LIFT",
            "ROUNDUP",
            "DILLA",
        ],
    ),
    (
        "Conditionals & scaling spikes",
        "Big payoffs when round state, primes, or per-copy counters align.",
        ["SEMAPHORE", "BOUNTY"],
    ),
    (
        "Ghost plays",
        "Optional graveyard replays during the round (exile after ghost).",
        ["COMEBACK", "ENCORE"],
    ),
    (
        "Reactive movement",
        "Triggers when this card is discarded, exiled, or relocated.",
        ["GET_ME_OUTA_HERE"],
    ),
]

DEFAULT_OUTPUT = ROOT / "docs" / "cards_by_verb.pdf"


def build_section_records(
    all_records: list[CardRecord],
) -> list[tuple[str, str, list[CardRecord]]]:
    by_enum = {record.enum_name: record for record in all_records}
    sections: list[tuple[str, str, list[CardRecord]]] = []
    assigned: set[str] = set()

    for title, blurb, enum_names in VERB_SECTIONS:
        section_records: list[CardRecord] = []
        for enum_name in enum_names:
            if enum_name not in by_enum:
                raise ValueError(f"Unknown card enum in verb map: {enum_name}")
            section_records.append(by_enum[enum_name])
            assigned.add(enum_name)
        sections.append((title, blurb, section_records))

    missing = [record.enum_name for record in all_records if record.enum_name not in assigned]
    if missing:
        raise ValueError(f"Cards not assigned to any verb section: {', '.join(missing)}")

    return sections


def draw_section_header(pdf: FPDF, title: str, blurb: str, y: float) -> float:
    pdf.set_xy(pdf.l_margin, y)
    pdf.set_font("Helvetica", "B", 14)
    pdf.set_text_color(25, 25, 25)
    pdf.cell(0, 7, title, new_x="LMARGIN", new_y="NEXT")

    pdf.set_x(pdf.l_margin)
    pdf.set_font("Helvetica", "", 9)
    pdf.set_text_color(80, 80, 80)
    pdf.multi_cell(0, 4.2, blurb)
    return pdf.get_y() + 2.0


def render_verb_pdf(
    sections: list[tuple[str, str, list[CardRecord]]],
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
    gutter_x = 4.0
    gutter_y = 3.0
    cell_width = (page_width - gutter_x * (columns - 1)) / columns
    cell_height = 52.0
    cards_per_page = columns * rows
    header_reserve = 18.0

    missing_sprites = 0
    with_sprites = 0

    pdf.add_page()
    pdf.set_xy(pdf.l_margin, pdf.t_margin)
    pdf.set_font("Helvetica", "B", 18)
    pdf.set_text_color(20, 20, 20)
    pdf.cell(0, 10, "Biggest Number - Cards by Verb", new_x="LMARGIN", new_y="NEXT")
    pdf.set_font("Helvetica", "", 10)
    pdf.set_text_color(90, 90, 90)
    pdf.multi_cell(
        0,
        5,
        "Grouped by mechanical role. Sprite art where available; otherwise name + in-game description.",
    )

    for title, blurb, records in sections:
        page_cursor_y = pdf.t_margin
        card_index = 0

        while card_index < len(records):
            if card_index == 0 or page_cursor_y + header_reserve + cell_height > pdf.h - pdf.b_margin:
                pdf.add_page()
                page_cursor_y = draw_section_header(pdf, title, blurb, pdf.t_margin)
                if card_index > 0:
                    pdf.set_font("Helvetica", "I", 8)
                    pdf.set_text_color(120, 120, 120)
                    pdf.set_x(pdf.l_margin)
                    pdf.cell(0, 4, "(continued)", new_x="LMARGIN", new_y="NEXT")
                    page_cursor_y = pdf.get_y() + 1.0

            slots_this_page = min(cards_per_page, len(records) - card_index)
            for slot in range(slots_this_page):
                record = records[card_index + slot]
                col = slot % columns
                row = slot // columns
                cell_x = pdf.l_margin + col * (cell_width + gutter_x)
                cell_y = page_cursor_y + row * (cell_height + gutter_y)

                sprite_image = None
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

            card_index += slots_this_page

    output_path.parent.mkdir(parents=True, exist_ok=True)
    pdf.output(str(output_path))
    return with_sprites, missing_sprites


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate verb-grouped card reference PDF.")
    parser.add_argument("--card-data", type=Path, default=DEFAULT_CARD_DATA)
    parser.add_argument("--card-types", type=Path, default=DEFAULT_CARD_TYPES)
    parser.add_argument("--graphics", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--columns", type=int, default=2)
    parser.add_argument("--rows", type=int, default=3)
    args = parser.parse_args()

    graphics_dirs = tuple(args.graphics) if args.graphics else DEFAULT_GRAPHICS_DIRS
    all_records = parse_card_records(args.card_data, args.card_types)
    sections = build_section_records(all_records)

    total_cards = sum(len(section[2]) for section in sections)
    print(f"Parsed {len(all_records)} cards into {len(sections)} verb sections ({total_cards} assigned)")

    with_sprites, missing_sprites = render_verb_pdf(
        sections,
        args.output,
        graphics_dirs,
        max(1, args.scale),
        max(1, args.columns),
        max(1, args.rows),
    )

    if missing_sprites:
        print(f"Warning: {missing_sprites} sprite card(s) missing art layers", file=sys.stderr)

    print(f"Wrote {args.output} ({with_sprites} sprites embedded)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
