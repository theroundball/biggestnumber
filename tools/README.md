# Sprite conversion tools (PNG → Butano graphics)

Portable art pipeline for **Biggest Number**. Run from the **repo root** (`bn/`), not inside `biggestnumber/`.

## What to copy to another machine

Copy this entire folder plus the graphics inbox (optional):

```text
bn/
├── tools/
│   ├── README.md                 ← this file
│   ├── convert_sprites.py        ← main converter (required)
│   ├── fill_transparent_png.py   ← optional pre-step for alpha PNGs
│   └── split_card_sprites.py     ← legacy wrapper; use convert_sprites.py
└── graphics/
    └── source/                   ← drop raw PNGs here (create if missing)
```

You do **not** need to copy `graphics/output/` — the script regenerates it.

## Requirements

- Python 3.9+ (3.10+ recommended)
- Pillow:

```bash
pip3 install pillow
```

## Quick start

From repo root:

```bash
# 1. Put PNGs in graphics/source/ (see sizes below)
# 2. Optional: fix transparent pixels → magenta chroma key
python3 tools/fill_transparent_png.py graphics/source

# 3. Convert to Butano .bmp + .json
python3 tools/convert_sprites.py

# 4. Copy outputs into the game project
cp graphics/output/*.bmp graphics/output/*.json biggestnumber/graphics/
```

Or with explicit paths:

```bash
python3 tools/convert_sprites.py graphics/source --out graphics/output
```

Then `make` from `biggestnumber/` so Butano picks up new graphics.

## Chroma key (transparency)

- Default key color: **magenta `#ff00ff`**
- Paint transparent areas with magenta before export, **or** run `fill_transparent_png.py` on PNGs that use real alpha.
- Pixels matching the key become palette index 0 (transparent on GBA).
- Override key: `--key RRGGBB` on either script.

```bash
python3 tools/fill_transparent_png.py graphics/source --in-place
python3 tools/convert_sprites.py graphics/source --key ff00ff
```

`fill_transparent_png.py` writes to `graphics/source_filled/` by default (originals preserved).

## How `convert_sprites.py` routes files

Drop files into **one flat folder** (`graphics/source/`). The script picks behavior from **dimensions** and **filename**:

| Input size (W × H) | Output | Notes |
|--------------------|--------|--------|
| **40 × 64** | `{name}_body`, `_accent_top`, `_accent_bottom` | Card composite; shared 16-color palette across all cards |
| **32 × 64** | `{name}_body.bmp` | Single body strip |
| **8 × 32** | accent or `ui_edge_fade` | Name ends with `_accent_*` |
| **32 × 32** | `ui_{name}.bmp` | UI placeholder |
| **16 × 16** | HUD / trinket / UI marker | From filename (`hud_*`, trinket names, etc.) |
| width × **8 / 16 / 32 / 64** | Font sheet pass-through | Height must match font row |

**Card art:** draw one **40 × 64** PNG per card. Filename stem becomes the asset prefix (e.g. `sips.png` → `sips_body.bmp`, …).

**Sizes should be multiples of 8.**

Output file names: lowercase letters, numbers, underscores only (Butano rule).

## Art reference

Full asset checklist and display sizes: `SPRITE_TODO.md` at repo root (card list, HUD icons, fonts).

After importing new card art, wire sprites in `biggestnumber/src/card_data.cpp` (`CARD_SPRITES(...)`).

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `No module named 'PIL'` | `pip3 install pillow` |
| `No source files found` | Create `graphics/source/` and add `.png` or `.bmp` |
| Wrong output folder | Default out is `graphics/output/`; use `--out` for one source dir |
| Colors look wrong on GBA | Use magenta key; avoid semi-transparent pixels — run `fill_transparent_png.py` |
| Build doesn't see new art | Copy `.bmp` + `.json` into `biggestnumber/graphics/`, then `make clean && make` |

## Scripts

### `convert_sprites.py` (main)

Converts PNG/BMP → indexed BMP + Butano JSON metadata.

```bash
python3 tools/convert_sprites.py
python3 tools/convert_sprites.py graphics/source --out graphics/output
python3 tools/convert_sprites.py graphics/source --key ff00ff
```

### `fill_transparent_png.py` (optional)

One-time fix for PNGs exported with alpha instead of magenta fill.

```bash
python3 tools/fill_transparent_png.py graphics/source
python3 tools/fill_transparent_png.py graphics/source --in-place
python3 tools/fill_transparent_png.py graphics/source --key ff00ff
```

### `split_card_sprites.py` (legacy)

Thin wrapper that calls `convert_sprites.py`. Prefer `convert_sprites.py` directly.

### `generate_card_pdf.py`

Builds a printable PDF of every card from `src/card_data.cpp` — sprite art (when
`CARD_SPRITES` is set) plus name and UI description. Text-only cards omit art.

Run from `biggestnumber/`:

```bash
pip install pillow fpdf2
python tools/generate_card_pdf.py
python tools/generate_card_pdf.py --output docs/cards.pdf --per-page 8
python tools/generate_card_pdf.py --per-page 6   # 2x3 grid
```

Looks for `{slug}_body.bmp`, `_accent_top`, and `_accent_bottom` under `graphics/`
(and `graphics/output/`). Re-run whenever card text or art changes.
