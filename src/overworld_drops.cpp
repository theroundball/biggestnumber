#include "overworld_drops.h"

#include "bn_blending.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_compression_type.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_string.h"
#include "bn_tile.h"
#include "bn_vector.h"

#include "bn_array.h"

#include "campaign.h"
#include "card.h"
#include "card_data.h"
#include "card_meta.h"
#include "common_variable_8x16_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "game_types.h"
#include "prize_system.h"
#include "overworld_scene.h"
#include "save_data.h"
#include "ui_inspect.h"

namespace
{
    constexpr int MAX_CARD_DROPS = CAMPAIGN_PRIZE_SLOT_COUNT;
    constexpr int MAX_DROPS = MAX_CARD_DROPS + 1;
    constexpr int TOSS_FRAMES = 24;
    constexpr int TOSS_STAGGER_FRAMES = 6;
    constexpr int TOSS_DROP_HEIGHT = 28;
    constexpr int POOF_FRAMES = 18;
    constexpr bn::fixed PICKUP_RANGE = 32;
    constexpr int LABEL_OFFSET_Y = -18;
    constexpr int CHIP_PAD_X = 2;
    constexpr int CHIP_PAD_Y = 1;
    constexpr bn::fixed DROP_UNSELECTED_ALPHA = bn::fixed(55) / 100;
    constexpr int MAP_PIXEL_W = 256;
    constexpr int MAP_PIXEL_H = 256;
    constexpr bn::fixed MAP_MARGIN = 20;
    constexpr bn::fixed DROP_MIN_SEPARATION = 40;
    constexpr bn::fixed DROP_ENTITY_PADDING = 10;

    constexpr int PAL_TRANSPARENT = 0;
    constexpr int PAL_CARD_MARKER = 1;
    constexpr int PAL_PAPER_MARKER = 2;
    constexpr int PAL_CHIP_COMMON_DIM = 3;
    constexpr int PAL_CHIP_COMMON = 4;
    constexpr int PAL_CHIP_UNCOMMON_DIM = 5;
    constexpr int PAL_CHIP_UNCOMMON = 6;
    constexpr int PAL_CHIP_RARE_DIM = 7;
    constexpr int PAL_CHIP_RARE = 8;
    constexpr int PAL_CHIP_PAPER_DIM = 9;
    constexpr int PAL_CHIP_PAPER = 10;
    constexpr int PAL_CARD_MARKER_DIM = 11;
    constexpr int PAL_PAPER_MARKER_DIM = 12;
    constexpr int CHIP_TILE_COUNT = 8;

    constexpr bn::color COLOR_OVERWORLD_BG(12, 18, 12);
    constexpr bn::color COLOR_CARD_MARKER(20, 28, 31);
    constexpr bn::color COLOR_PAPER_MARKER(28, 26, 20);
    constexpr bn::color COLOR_TEXT_DIM(16, 16, 18);
    constexpr bn::color COLOR_TEXT_SELECTED(31, 27, 6);
    constexpr bn::color COLOR_RARITY_COMMON(4, 12, 6);
    constexpr bn::color COLOR_RARITY_UNCOMMON(4, 8, 22);
    constexpr bn::color COLOR_RARITY_RARE(28, 22, 6);
    constexpr bn::color COLOR_PAPER_NEUTRAL(14, 12, 10);
    constexpr bn::color COLOR_PAPER_BRIGHT(22, 20, 14);

    enum class DropKind
    {
        CARD,
        PAPER,
    };

    struct DropEntry
    {
        DropKind kind = DropKind::CARD;
        PrizeOffer offer;
        bn::fixed world_x = 0;
        bn::fixed landing_y = 0;
        int toss_delay = 0;
        int toss_frame = 0;
        int poof_frame = 0;
        bool active = false;
        bool poofing = false;
        bool previewed = false;
    };

    struct DropSession
    {
        bool active = false;
        bn::fixed spawn_x = 0;
        bn::fixed spawn_y = 0;
        bn::array<DropEntry, MAX_DROPS> drops;
        int drop_count = 0;
        int selected_index = -1;
        int inspect_index = -1;
        bool inspect_open = false;
        bn::vector<bn::sprite_ptr, 64> inspect_sprites;
    };

    struct SpriteBinding
    {
        bn::sprite_ptr sprite;
        bn::fixed offset_x = 0;
        bn::fixed offset_y = 0;
    };

    struct DropVisualSlot
    {
        bn::optional<bn::sprite_ptr> marker;
        bn::vector<SpriteBinding, 16> label_sprites;
        bn::vector<SpriteBinding, 16> chip_sprites;
        bool marker_selected = false;
        bool marker_is_paper = false;
        bool label_selected = false;
        bool label_is_paper = false;
        CardRarity label_rarity = CardRarity::COMMON;
        const char* label_text = nullptr;
    };

    DropSession g_session;
    bn::array<DropVisualSlot, MAX_DROPS> g_visual_slots;
    bn::optional<Card> g_inspect_card;
    bn::optional<bn::sprite_text_generator> g_inspect_pip_generator;

    Card& inspect_card()
    {
        if(!g_inspect_card.has_value())
        {
            g_inspect_card.emplace();
        }

        return *g_inspect_card;
    }

    bn::sprite_text_generator& inspect_pip_generator()
    {
        if(!g_inspect_pip_generator.has_value())
        {
            g_inspect_pip_generator.emplace(common::variable_8x8_sprite_font);
        }

        return *g_inspect_pip_generator;
    }

    struct EntityBlock
    {
        bn::fixed x = 0;
        bn::fixed y = 0;
        bn::fixed half_w = 0;
        bn::fixed half_h = 0;
    };

    bn::vector<EntityBlock, 4> g_entity_blocks;

    bn::color dim_color(bn::color color)
    {
        return bn::color(color.red() / 2, color.green() / 2, color.blue() / 2);
    }

    bn::color fade_toward_bg(bn::color color, int bg_weight = 96)
    {
        constexpr int total = 256;

        return bn::color(
            (color.red() * (total - bg_weight) + COLOR_OVERWORLD_BG.red() * bg_weight) / total,
            (color.green() * (total - bg_weight) + COLOR_OVERWORLD_BG.green() * bg_weight) / total,
            (color.blue() * (total - bg_weight) + COLOR_OVERWORLD_BG.blue() * bg_weight) / total);
    }

    int glyph_palette_index()
    {
        constexpr int FALLBACK = 15;
        bn::sprite_text_generator bootstrap(common::variable_8x8_sprite_font);
        bn::vector<bn::sprite_ptr, 1> bootstrap_sprites;
        bootstrap.generate(0, 0, "A", bootstrap_sprites);

        if(bootstrap_sprites.empty())
        {
            return FALLBACK;
        }

        bn::span<const bn::color> colors = bootstrap_sprites[0].palette().colors();
        int best_index = FALLBACK;
        int best_luma = -1;

        for(int index = 1; index < colors.size(); ++index)
        {
            const bn::color& color = colors[index];
            const int luma = color.red() + color.green() + color.blue();

            if(luma > best_luma)
            {
                best_luma = luma;
                best_index = index;
            }
        }

        return best_index;
    }

    bn::fixed abs_fixed(bn::fixed value)
    {
        return value < 0 ? -value : value;
    }

    uint32_t solid_tile_row(int color_index)
    {
        uint32_t row = 0;

        for(int px = 0; px < 8; ++px)
        {
            row |= uint32_t(color_index) << (px * 4);
        }

        return row;
    }

    void paint_solid_tile(bn::tile& tile, int color_index)
    {
        const uint32_t fill = solid_tile_row(color_index);

        for(int row_index = 0; row_index < 8; ++row_index)
        {
            tile.data[row_index] = fill;
        }
    }

    void paint_solid_tiles(bn::sprite_tiles_ptr& tiles, int color_index)
    {
        auto vram = tiles.vram();
        auto* tile_span = vram.get();

        if(!tile_span)
        {
            return;
        }

        const uint32_t fill = solid_tile_row(color_index);

        for(bn::tile& tile : *tile_span)
        {
            for(int row_index = 0; row_index < 8; ++row_index)
            {
                tile.data[row_index] = fill;
            }
        }
    }

    int align_down_8(int value)
    {
        return value >= 0 ? (value / 8) * 8 : -(((-value) + 7) / 8) * 8;
    }

    int align_up_8(int value)
    {
        return value >= 0 ? ((value + 7) / 8) * 8 : (value / 8) * 8;
    }

    constexpr int kSpriteDims[] = {64, 32, 16, 8};

    bool valid_sprite_size(int width, int height)
    {
        const bool square = (width == 8 && height == 8) || (width == 16 && height == 16) ||
                            (width == 32 && height == 32) || (width == 64 && height == 64);
        const bool wide = (width == 16 && height == 8) || (width == 32 && height == 8) ||
                          (width == 32 && height == 16) || (width == 64 && height == 32);
        const bool tall = (width == 8 && height == 16) || (width == 8 && height == 32) ||
                          (width == 16 && height == 32) || (width == 32 && height == 64);
        return square || wide || tall;
    }

    int largest_valid_sprite_width(int max_width, int height)
    {
        for(int width : kSpriteDims)
        {
            if(width <= max_width && valid_sprite_size(width, height))
            {
                return width;
            }
        }

        return 0;
    }

    bool can_tile_row(int width, int height)
    {
        int x = 0;

        while(x < width)
        {
            const int tile_width = largest_valid_sprite_width(width - x, height);

            if(tile_width <= 0)
            {
                return false;
            }

            x += tile_width;
        }

        return true;
    }

    int choose_row_height(int remaining_height, int width)
    {
        for(int height : kSpriteDims)
        {
            if(height <= remaining_height && can_tile_row(width, height))
            {
                return height;
            }
        }

        return 8;
    }

    void paint_shape_tile(bn::tile& tile, bool (*pixel_fn)(int, int), int color_index)
    {
        for(int py = 0; py < 8; ++py)
        {
            uint32_t row = 0;

            for(int px = 0; px < 8; ++px)
            {
                const int index = pixel_fn(px, py) ? color_index : PAL_TRANSPARENT;
                row |= uint32_t(index) << (px * 4);
            }

            tile.data[py] = row;
        }
    }

    bool card_marker_pixel(int px, int py)
    {
        return px >= 1 && px <= 6 && py >= 0 && py <= 7;
    }

    bool paper_marker_pixel(int px, int py)
    {
        return px >= 2 && px <= 5 && py >= 2 && py <= 5;
    }

    struct LabelTextPalettes
    {
        bn::array<bn::color, 16> dim_colors;
        bn::array<bn::color, 16> bright_colors;
        bn::optional<bn::sprite_palette_item> dim_item;
        bn::optional<bn::sprite_palette_item> bright_item;
        bn::optional<bn::sprite_text_generator> dim_generator;
        bn::optional<bn::sprite_text_generator> bright_generator;
        bool ready = false;

        void ensure()
        {
            if(ready)
            {
                return;
            }

            bn::sprite_text_generator bootstrap(common::variable_8x8_sprite_font);
            bn::vector<bn::sprite_ptr, 1> bootstrap_sprites;
            bootstrap.generate(0, 0, "A", bootstrap_sprites);
            bn::span<const bn::color> source = bootstrap_sprites[0].palette().colors();

            for(int index = 0; index < 16; ++index)
            {
                dim_colors[index] = index < source.size() ? source[index] : bn::color();
                bright_colors[index] = dim_colors[index];
            }

            const int glyph_index = glyph_palette_index();
            dim_colors[glyph_index] = COLOR_TEXT_DIM;
            bright_colors[glyph_index] = COLOR_TEXT_SELECTED;

            dim_item = bn::sprite_palette_item(
                bn::span<const bn::color>(dim_colors.data(), dim_colors.size()), bn::bpp_mode::BPP_4,
                bn::compression_type::NONE);
            bright_item = bn::sprite_palette_item(
                bn::span<const bn::color>(bright_colors.data(), bright_colors.size()),
                bn::bpp_mode::BPP_4, bn::compression_type::NONE);
            dim_generator.emplace(common::variable_8x8_sprite_font, dim_item.value());
            bright_generator.emplace(common::variable_8x8_sprite_font, bright_item.value());
            ready = true;
        }
    };

    LabelTextPalettes& label_text_palettes()
    {
        static LabelTextPalettes palettes;
        palettes.ensure();
        return palettes;
    }

    void apply_drop_sprite_style(bn::sprite_ptr& sprite, bool selected)
    {
        sprite.set_bg_priority(game_layout::OVERLAY_TEXT_BG_PRIORITY);

        if(selected)
        {
            sprite.set_blending_enabled(false);
            return;
        }

        sprite.set_blending_enabled(true);
    }

    struct DropVisualAssets
    {
        bn::optional<bn::sprite_tiles_ptr> card_marker_tiles;
        bn::optional<bn::sprite_tiles_ptr> card_marker_dim_tiles;
        bn::optional<bn::sprite_tiles_ptr> paper_marker_tiles;
        bn::optional<bn::sprite_tiles_ptr> paper_marker_dim_tiles;
        bn::array<bn::optional<bn::sprite_tiles_ptr>, CHIP_TILE_COUNT> chip_tiles;
        bn::optional<bn::sprite_palette_ptr> palette;
        bool ready = false;

        void ensure()
        {
            if(ready)
            {
                return;
            }

            card_marker_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);
            card_marker_dim_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);
            paper_marker_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);
            paper_marker_dim_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);

            if(auto vram = card_marker_tiles->vram().get())
            {
                paint_shape_tile((*vram)[0], card_marker_pixel, PAL_CARD_MARKER);
            }

            if(auto vram = card_marker_dim_tiles->vram().get())
            {
                paint_shape_tile((*vram)[0], card_marker_pixel, PAL_CARD_MARKER_DIM);
            }

            if(auto vram = paper_marker_tiles->vram().get())
            {
                paint_shape_tile((*vram)[0], paper_marker_pixel, PAL_PAPER_MARKER);
            }

            if(auto vram = paper_marker_dim_tiles->vram().get())
            {
                paint_shape_tile((*vram)[0], paper_marker_pixel, PAL_PAPER_MARKER_DIM);
            }

            constexpr int chip_color_indices[CHIP_TILE_COUNT] = {
                PAL_CHIP_COMMON_DIM, PAL_CHIP_COMMON,     PAL_CHIP_UNCOMMON_DIM, PAL_CHIP_UNCOMMON,
                PAL_CHIP_RARE_DIM,   PAL_CHIP_RARE,       PAL_CHIP_PAPER_DIM,    PAL_CHIP_PAPER,
            };

            for(int tile_index = 0; tile_index < CHIP_TILE_COUNT; ++tile_index)
            {
                chip_tiles[tile_index] = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);

                if(auto vram = chip_tiles[tile_index]->vram().get())
                {
                    paint_solid_tile((*vram)[0], chip_color_indices[tile_index]);
                }
            }

            bn::array<bn::color, 16> colors;
            colors[PAL_TRANSPARENT] = bn::color(0, 0, 0);
            colors[PAL_CARD_MARKER] = COLOR_CARD_MARKER;
            colors[PAL_PAPER_MARKER] = COLOR_PAPER_MARKER;
            colors[PAL_CHIP_COMMON_DIM] = fade_toward_bg(dim_color(COLOR_RARITY_COMMON));
            colors[PAL_CHIP_COMMON] = COLOR_RARITY_COMMON;
            colors[PAL_CHIP_UNCOMMON_DIM] = fade_toward_bg(dim_color(COLOR_RARITY_UNCOMMON));
            colors[PAL_CHIP_UNCOMMON] = COLOR_RARITY_UNCOMMON;
            colors[PAL_CHIP_RARE_DIM] = fade_toward_bg(dim_color(COLOR_RARITY_RARE));
            colors[PAL_CHIP_RARE] = COLOR_RARITY_RARE;
            colors[PAL_CHIP_PAPER_DIM] = fade_toward_bg(COLOR_PAPER_NEUTRAL);
            colors[PAL_CHIP_PAPER] = COLOR_PAPER_BRIGHT;
            colors[PAL_CARD_MARKER_DIM] = fade_toward_bg(COLOR_CARD_MARKER);
            colors[PAL_PAPER_MARKER_DIM] = fade_toward_bg(COLOR_PAPER_MARKER);

            const bn::sprite_palette_item item(
                bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4,
                bn::compression_type::NONE);
            palette = bn::sprite_palette_ptr::create(item);

            ready = true;
        }
    };

    DropVisualAssets& drop_visual_assets()
    {
        static DropVisualAssets assets;
        assets.ensure();
        return assets;
    }

    int chip_tile_index(CardRarity rarity, bool selected, bool is_paper)
    {
        if(is_paper)
        {
            return 6 + (selected ? 1 : 0);
        }

        const int base = int(rarity) * 2;
        return base + (selected ? 1 : 0);
    }

    void release_label_visuals(DropVisualSlot& slot)
    {
        slot.label_sprites.clear();
        slot.chip_sprites.clear();
        slot.label_text = nullptr;
    }

    void release_visual_slot(DropVisualSlot& slot)
    {
        slot.marker.reset();
        release_label_visuals(slot);
    }

    void release_all_visual_slots()
    {
        for(DropVisualSlot& slot : g_visual_slots)
        {
            release_visual_slot(slot);
        }
    }

    void clear_inspect()
    {
        g_session.inspect_sprites.clear();
        g_session.inspect_open = false;
        g_session.inspect_index = -1;
        hide_inspect_card(inspect_card());
    }

    void clear_drop_visuals()
    {
        release_all_visual_slots();
    }

    void reset_drop_session()
    {
        clear_inspect();
        clear_drop_visuals();

        g_session.active = false;
        g_session.spawn_x = 0;
        g_session.spawn_y = 0;
        g_session.drop_count = 0;
        g_session.selected_index = -1;
        g_session.inspect_index = -1;
        g_session.inspect_open = false;

        for(DropEntry& drop : g_session.drops)
        {
            drop = DropEntry{};
        }
    }

    bn::fixed toss_height(const DropEntry& drop)
    {
        if(drop.toss_delay > 0 || drop.toss_frame < TOSS_FRAMES)
        {
            if(drop.toss_frame >= TOSS_FRAMES)
            {
                return bn::fixed(TOSS_DROP_HEIGHT);
            }

            const bn::fixed t = bn::fixed(drop.toss_frame) / bn::fixed(TOSS_FRAMES);
            const bn::fixed inv = bn::fixed(1) - t;
            return inv * inv * bn::fixed(TOSS_DROP_HEIGHT);
        }

        return 0;
    }

    CardType offer_card_type(const PrizeOffer& offer)
    {
        if(offer.kind == PrizeOfferKind::CARD && offer.card != CardType::COUNT)
        {
            return offer.card;
        }

        return CardType::COUNT;
    }

    void sync_inspect_visuals()
    {
        if(!g_session.inspect_open || g_session.inspect_index < 0 ||
           g_session.inspect_index >= g_session.drop_count)
        {
            return;
        }

        const DropEntry& drop = g_session.drops[g_session.inspect_index];

        if(!drop.active || drop.poofing)
        {
            return;
        }

        if(drop.kind == DropKind::CARD)
        {
            const CardType type = offer_card_type(drop.offer);

            if(type != CardType::COUNT)
            {
                show_inspect_card(inspect_card(), type, nullptr, &inspect_pip_generator());
            }
        }
        else
        {
            hide_inspect_card(inspect_card());
        }
    }

    int active_drop_count()
    {
        int count = 0;

        for(int index = 0; index < g_session.drop_count; ++index)
        {
            if(g_session.drops[index].active)
            {
                ++count;
            }
        }

        return count;
    }

    void fill_chip_rect(DropVisualSlot& slot, int box_left, int box_top, int box_right, int box_bottom,
                        bn::fixed anchor_x, bn::fixed anchor_y, int color_index, bool selected)
    {
        const int width = box_right - box_left;
        const int height = box_bottom - box_top;

        if(width <= 0 || height <= 0)
        {
            return;
        }

        DropVisualAssets& assets = drop_visual_assets();
        int y = 0;

        while(y < height)
        {
            const int row_height = choose_row_height(height - y, width);
            int x = 0;

            while(x < width)
            {
                const int tile_width = largest_valid_sprite_width(width - x, row_height);

                if(tile_width <= 0)
                {
                    break;
                }

                const bn::sprite_shape_size shape_size(tile_width, row_height);
                const int tiles_count = shape_size.tiles_count(bn::bpp_mode::BPP_4);
                bn::sprite_tiles_ptr piece_tiles =
                    bn::sprite_tiles_ptr::allocate(tiles_count, bn::bpp_mode::BPP_4);
                paint_solid_tiles(piece_tiles, color_index);

                const bn::fixed center_x = bn::fixed(box_left + x + tile_width / 2);
                const bn::fixed center_y = bn::fixed(box_top + y + row_height / 2);
                bn::sprite_ptr sprite = bn::sprite_ptr::create(center_x, center_y, shape_size,
                                                               piece_tiles, *assets.palette);
                sprite.set_z_order(game_layout::OVERLAY_TEXT_Z);
                apply_drop_sprite_style(sprite, selected);

                if(!slot.chip_sprites.full())
                {
                    slot.chip_sprites.push_back(
                        SpriteBinding{sprite, center_x - anchor_x, center_y - anchor_y});
                }

                x += tile_width;
            }

            y += row_height;
        }
    }

    void build_drop_label(DropVisualSlot& slot, bn::fixed anchor_x, bn::fixed anchor_y,
                          const char* text, bool selected, CardRarity rarity, bool is_paper)
    {
        release_label_visuals(slot);
        slot.label_selected = selected;
        slot.label_is_paper = is_paper;
        slot.label_rarity = rarity;
        slot.label_text = text;

        LabelTextPalettes& text_palettes = label_text_palettes();
        bn::sprite_text_generator& label_generator =
            selected ? text_palettes.bright_generator.value() : text_palettes.dim_generator.value();
        const bn::fixed text_y = anchor_y + bn::fixed(LABEL_OFFSET_Y);

        bn::vector<bn::sprite_ptr, 16> generated_text;
        label_generator.set_center_alignment();
        label_generator.generate(anchor_x, text_y, text, generated_text);
        label_generator.set_left_alignment();

        if(generated_text.empty())
        {
            return;
        }

        bn::fixed min_x = generated_text[0].x();
        bn::fixed max_x = generated_text[0].x();
        bn::fixed min_y = generated_text[0].y();
        bn::fixed max_y = generated_text[0].y();

        for(const bn::sprite_ptr& sprite : generated_text)
        {
            const bn::sprite_shape_size shape = sprite.shape_size();
            const bn::fixed half_w = bn::fixed(shape.width()) / 2;
            const bn::fixed half_h = bn::fixed(shape.height()) / 2;
            const bn::fixed glyph_left = sprite.x() - half_w;
            const bn::fixed glyph_right = sprite.x() + half_w;
            const bn::fixed glyph_top = sprite.y() - half_h;
            const bn::fixed glyph_bottom = sprite.y() + half_h;

            if(!slot.label_sprites.full())
            {
                slot.label_sprites.push_back(
                    SpriteBinding{sprite, sprite.x() - anchor_x, sprite.y() - anchor_y});
            }

            if(glyph_left < min_x)
            {
                min_x = glyph_left;
            }

            if(glyph_right > max_x)
            {
                max_x = glyph_right;
            }

            if(glyph_top < min_y)
            {
                min_y = glyph_top;
            }

            if(glyph_bottom > max_y)
            {
                max_y = glyph_bottom;
            }
        }

        const int box_left = align_down_8(int(min_x) - CHIP_PAD_X);
        const int box_right = align_up_8(int(max_x) + CHIP_PAD_X);
        const int box_top = align_down_8(int(min_y) - CHIP_PAD_Y);
        const int box_bottom = align_up_8(int(max_y) + CHIP_PAD_Y);

        fill_chip_rect(slot, box_left, box_top, box_right, box_bottom, anchor_x, anchor_y,
                       chip_tile_index(rarity, selected, is_paper), selected);

        for(SpriteBinding& binding : slot.label_sprites)
        {
            binding.sprite.set_z_order(game_layout::OVERLAY_TEXT_Z);
            apply_drop_sprite_style(binding.sprite, selected);
        }
    }

    void reposition_visual_slot(DropVisualSlot& slot, bn::fixed anchor_x, bn::fixed anchor_y)
    {
        if(slot.marker.has_value())
        {
            slot.marker->set_position(anchor_x, anchor_y);
        }

        for(SpriteBinding& binding : slot.label_sprites)
        {
            binding.sprite.set_position(anchor_x + binding.offset_x, anchor_y + binding.offset_y);
        }

        for(SpriteBinding& binding : slot.chip_sprites)
        {
            binding.sprite.set_position(anchor_x + binding.offset_x, anchor_y + binding.offset_y);
        }
    }

    bool label_needs_rebuild(const DropVisualSlot& slot, bool selected, CardRarity rarity,
                             bool is_paper, const char* text)
    {
        return slot.label_text != text || slot.label_selected != selected ||
               slot.label_rarity != rarity || slot.label_is_paper != is_paper ||
               slot.label_sprites.empty();
    }

    void ensure_drop_marker(DropVisualSlot& slot, bool is_paper, bool selected,
                            const bn::sprite_tiles_ptr& bright_tiles,
                            const bn::sprite_tiles_ptr& dim_tiles, const bn::sprite_palette_ptr& palette,
                            bn::fixed screen_x, bn::fixed screen_y, bn::fixed landing_y)
    {
        const bool marker_changed =
            !slot.marker.has_value() || slot.marker_is_paper != is_paper ||
            slot.marker_selected != selected;

        if(marker_changed)
        {
            const bn::sprite_tiles_ptr& marker_tiles = selected ? bright_tiles : dim_tiles;
            slot.marker = bn::sprite_ptr::create(screen_x, screen_y, bn::sprite_shape_size(8, 8),
                                               marker_tiles, palette);
            slot.marker_is_paper = is_paper;
            slot.marker_selected = selected;
        }
        else
        {
            slot.marker->set_position(screen_x, screen_y);
        }

        slot.marker->set_z_order(overworld_depth_z_order(landing_y));
        slot.marker->set_bg_priority(0);
        apply_drop_sprite_style(*slot.marker, selected);
    }

    void update_drop_visuals(const bn::fixed_point& camera)
    {
        DropVisualAssets& assets = drop_visual_assets();

        if(g_session.inspect_open)
        {
            release_all_visual_slots();
            return;
        }

        bool has_unselected = false;

        for(int index = 0; index < g_session.drop_count; ++index)
        {
            const DropEntry& drop = g_session.drops[index];

            if(drop.active && !drop.poofing && index != g_session.selected_index)
            {
                has_unselected = true;
            }
        }

        if(has_unselected)
        {
            bn::blending::set_transparency_alpha(DROP_UNSELECTED_ALPHA);
        }

        for(int index = 0; index < MAX_DROPS; ++index)
        {
            DropVisualSlot& slot = g_visual_slots[index];

            if(index >= g_session.drop_count || !g_session.drops[index].active)
            {
                if(slot.marker.has_value() || !slot.label_sprites.empty())
                {
                    release_visual_slot(slot);
                }

                continue;
            }

            const DropEntry& drop = g_session.drops[index];
            const bn::fixed screen_x = drop.world_x - camera.x() - 120;
            bn::fixed screen_y = drop.landing_y - toss_height(drop) - camera.y() - 80;

            if(drop.poofing)
            {
                screen_y -= bn::fixed(drop.poof_frame * 2);
            }

            const bool selected = index == g_session.selected_index;
            const bool marker_visible = !drop.poofing || (drop.poof_frame % 2) == 0;
            const bool is_paper = drop.kind == DropKind::PAPER;

            if(drop.poofing)
            {
                release_label_visuals(slot);

                if(marker_visible)
                {
                    ensure_drop_marker(
                        slot, is_paper, selected,
                        is_paper ? *assets.paper_marker_tiles : *assets.card_marker_tiles,
                        is_paper ? *assets.paper_marker_dim_tiles : *assets.card_marker_dim_tiles,
                        *assets.palette, screen_x, screen_y, drop.landing_y);
                    slot.marker->set_visible(true);
                }
                else if(slot.marker.has_value())
                {
                    slot.marker->set_visible(false);
                }

                continue;
            }

            ensure_drop_marker(
                slot, is_paper, selected,
                is_paper ? *assets.paper_marker_tiles : *assets.card_marker_tiles,
                is_paper ? *assets.paper_marker_dim_tiles : *assets.card_marker_dim_tiles,
                *assets.palette, screen_x, screen_y, drop.landing_y);
            slot.marker->set_visible(true);

            CardRarity rarity = CardRarity::COMMON;
            const char* label = "Paper";

            if(!is_paper)
            {
                const CardType type = offer_card_type(drop.offer);

                if(type == CardType::COUNT)
                {
                    continue;
                }

                rarity = card_meta(type).rarity;
                label = card_data(type).name;
            }

            if(label_needs_rebuild(slot, selected, rarity, is_paper, label))
            {
                build_drop_label(slot, screen_x, screen_y, label, selected, rarity, is_paper);
            }
            else
            {
                reposition_visual_slot(slot, screen_x, screen_y);
            }
        }

        bn::blending::set_transparency_alpha(bn::fixed(1));
    }

    int find_nearest_drop(bn::fixed player_x, bn::fixed player_y)
    {
        int best_index = -1;
        bn::fixed best_distance = PICKUP_RANGE;

        for(int index = 0; index < g_session.drop_count; ++index)
        {
            const DropEntry& drop = g_session.drops[index];

            if(!drop.active || drop.poofing)
            {
                continue;
            }

            const bn::fixed dx = player_x - drop.world_x;
            const bn::fixed dy = player_y - drop.landing_y;
            const bn::fixed distance = bn::sqrt(dx * dx + dy * dy);

            if(distance <= best_distance)
            {
                best_distance = distance;
                best_index = index;
            }
        }

        return best_index;
    }

    void open_inspect(int index)
    {
        if(index < 0 || index >= g_session.drop_count)
        {
            return;
        }

        DropEntry& drop = g_session.drops[index];

        if(!drop.active || drop.poofing)
        {
            return;
        }

        clear_inspect();
        drop.previewed = true;
        g_session.inspect_index = index;

        bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
        bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);

        if(drop.kind == DropKind::PAPER)
        {
            hide_inspect_card(inspect_card());
            draw_text_inspect("Sticker paper", "Pick up to add +1 paper.", title_generator,
                              body_generator, g_session.inspect_sprites);
        }
        else
        {
            const CardType type = offer_card_type(drop.offer);

            if(type == CardType::COUNT)
            {
                return;
            }

            show_inspect_card(inspect_card(), type, nullptr, &inspect_pip_generator());
            draw_card_inspect(type, title_generator, body_generator, g_session.inspect_sprites);
        }

        g_session.inspect_open = true;
    }

    void start_poof(DropEntry& drop)
    {
        drop.poofing = true;
        drop.poof_frame = 0;
    }

    void poof_other_cards(int picked_index)
    {
        for(int index = 0; index < g_session.drop_count; ++index)
        {
            if(index == picked_index)
            {
                continue;
            }

            DropEntry& drop = g_session.drops[index];

            if(drop.active && drop.kind == DropKind::CARD)
            {
                start_poof(drop);
            }
        }
    }

    bool try_pickup(int index)
    {
        if(index < 0 || index >= g_session.drop_count)
        {
            return false;
        }

        DropEntry& drop = g_session.drops[index];

        if(!drop.active || drop.poofing)
        {
            return false;
        }

        SaveData& save = save_data_mut();

        if(drop.kind == DropKind::PAPER)
        {
            campaign_grant_sticker_paper(save, 1);
            drop.active = false;
            release_visual_slot(g_visual_slots[index]);
            return true;
        }

        const CardType type = offer_card_type(drop.offer);

        if(type == CardType::COUNT)
        {
            campaign_grant_sticker_paper(save, 1);
            drop.active = false;
            return true;
        }

        if(!campaign_apply_prize_card(save, type))
        {
            campaign_grant_sticker_paper(save, 1);
        }

        drop.active = false;
        release_visual_slot(g_visual_slots[index]);
        poof_other_cards(index);
        return true;
    }

    void finish_session_if_empty()
    {
        if(active_drop_count() == 0)
        {
            reset_drop_session();
        }
    }

    void tick_toss_motion()
    {
        for(int index = 0; index < g_session.drop_count; ++index)
        {
            DropEntry& drop = g_session.drops[index];

            if(!drop.active || drop.poofing)
            {
                continue;
            }

            if(drop.toss_delay > 0)
            {
                --drop.toss_delay;
                continue;
            }

            if(drop.toss_frame < TOSS_FRAMES)
            {
                ++drop.toss_frame;
            }
        }
    }

    void advance_poof_frames()
    {
        for(int index = 0; index < g_session.drop_count; ++index)
        {
            DropEntry& drop = g_session.drops[index];

            if(!drop.active || !drop.poofing)
            {
                continue;
            }

            ++drop.poof_frame;

            if(drop.poof_frame >= POOF_FRAMES)
            {
                drop.poofing = false;
                drop.active = false;
                release_visual_slot(g_visual_slots[index]);
            }
        }
    }

    bool drop_position_in_bounds(bn::fixed x, bn::fixed y)
    {
        return x >= MAP_MARGIN && x <= bn::fixed(MAP_PIXEL_W) - MAP_MARGIN &&
               y >= MAP_MARGIN && y <= bn::fixed(MAP_PIXEL_H) - MAP_MARGIN;
    }

    bool drop_position_blocked(bn::fixed x, bn::fixed y)
    {
        for(const EntityBlock& block : g_entity_blocks)
        {
            const bn::fixed dx = abs_fixed(x - block.x);
            const bn::fixed dy = abs_fixed(y - block.y);

            if(dx < block.half_w + DROP_ENTITY_PADDING && dy < block.half_h + DROP_ENTITY_PADDING)
            {
                return true;
            }
        }

        return false;
    }

    bool drop_position_far_enough(bn::fixed x, bn::fixed y, int placed_count)
    {
        for(int index = 0; index < placed_count; ++index)
        {
            const DropEntry& placed = g_session.drops[index];
            const bn::fixed dx = x - placed.world_x;
            const bn::fixed dy = y - placed.landing_y;
            const bn::fixed distance = bn::sqrt(dx * dx + dy * dy);

            if(distance < DROP_MIN_SEPARATION)
            {
                return false;
            }
        }

        return true;
    }

    bool drop_position_valid(bn::fixed x, bn::fixed y, int placed_count)
    {
        return drop_position_in_bounds(x, y) && !drop_position_blocked(x, y) &&
               drop_position_far_enough(x, y, placed_count);
    }

    bn::fixed_point pick_drop_offset(bn::seed_random& rng, int slot_index, int slot_count)
    {
        constexpr bn::fixed BASE_RADIUS = 44;
        constexpr bn::fixed EXTRA_RADIUS = 12;
        constexpr int MAX_ATTEMPTS = 24;

        for(int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
        {
            const int angle_step = 360 / (slot_count > 0 ? slot_count : 1);
            const int angle_deg =
                slot_index * angle_step + rng.get_int(angle_step) - angle_step / 2;
            const bn::fixed angle = bn::fixed(angle_deg);
            const bn::fixed radius =
                BASE_RADIUS + bn::fixed(slot_index % 2) * EXTRA_RADIUS + bn::fixed(rng.get_int(8));
            const bn::fixed offset_x = bn::degrees_lut_cos_safe(angle) * radius;
            const bn::fixed offset_y = bn::degrees_lut_sin_safe(angle) * radius;
            const bn::fixed world_x = g_session.spawn_x + offset_x;
            const bn::fixed world_y = g_session.spawn_y + offset_y;

            if(drop_position_valid(world_x, world_y, slot_index))
            {
                return bn::fixed_point(offset_x, offset_y);
            }
        }

        const bn::fixed fallback_x = bn::fixed((slot_index - 1)) * DROP_MIN_SEPARATION;
        const bn::fixed fallback_y = bn::fixed(36 + slot_index * 8);
        return bn::fixed_point(fallback_x, fallback_y);
    }

    void add_card_drop(const PrizeOffer& offer, bn::fixed world_x, bn::fixed landing_y,
                       int toss_delay)
    {
        if(g_session.drop_count >= MAX_DROPS || offer_card_type(offer) == CardType::COUNT)
        {
            return;
        }

        DropEntry& drop = g_session.drops[g_session.drop_count++];
        drop.kind = DropKind::CARD;
        drop.offer = offer;
        drop.world_x = world_x;
        drop.landing_y = landing_y;
        drop.toss_delay = toss_delay;
        drop.toss_frame = 0;
        drop.poof_frame = 0;
        drop.active = true;
        drop.poofing = false;
        drop.previewed = false;
    }

    void add_paper_drop(bn::fixed world_x, bn::fixed landing_y, int toss_delay)
    {
        if(g_session.drop_count >= MAX_DROPS)
        {
            return;
        }

        DropEntry& drop = g_session.drops[g_session.drop_count++];
        drop.kind = DropKind::PAPER;
        drop.world_x = world_x;
        drop.landing_y = landing_y;
        drop.toss_delay = toss_delay;
        drop.toss_frame = 0;
        drop.poof_frame = 0;
        drop.active = true;
        drop.poofing = false;
        drop.previewed = false;
    }
}

void overworld_drops_set_spawn(bn::fixed world_x, bn::fixed world_y)
{
    g_session.spawn_x = world_x;
    g_session.spawn_y = world_y;
}

void overworld_drops_clear_entity_blocks()
{
    g_entity_blocks.clear();
}

void overworld_drops_add_entity_block(bn::fixed world_x, bn::fixed world_y, bn::fixed half_w,
                                      bn::fixed half_h)
{
    if(g_entity_blocks.full())
    {
        return;
    }

    g_entity_blocks.push_back(EntityBlock{world_x, world_y, half_w, half_h});
}

void overworld_drops_queue_from_battle(CampaignMode mode, bool won, int peak_before, int band_score,
                                       bn::seed_random& rng)
{
    const bn::fixed spawn_x = g_session.spawn_x;
    const bn::fixed spawn_y = g_session.spawn_y;
    const bn::vector<EntityBlock, 4> saved_blocks = g_entity_blocks;
    overworld_drops_clear();
    g_session.spawn_x = spawn_x;
    g_session.spawn_y = spawn_y;
    g_entity_blocks = saved_blocks;

    const SaveData& save = save_data_get();
    int card_slots = 0;
    PrizeOffer offers[CAMPAIGN_PRIZE_SLOT_COUNT];

    if(won && !saved_deck_unrestricted_build(save.decks[save.active_deck_index]))
    {
        prize_build_offers(save, mode, peak_before, band_score, rng, offers);

        for(int slot = 0; slot < CAMPAIGN_PRIZE_SLOT_COUNT; ++slot)
        {
            if(offer_card_type(offers[slot]) != CardType::COUNT)
            {
                ++card_slots;
            }
        }
    }

    g_session.active = true;
    g_session.drop_count = 0;

    const int total_drop_slots = card_slots + 1;
    int placed_cards = 0;

    for(int slot = 0; slot < CAMPAIGN_PRIZE_SLOT_COUNT && placed_cards < card_slots; ++slot)
    {
        const CardType type = offer_card_type(offers[slot]);

        if(type == CardType::COUNT)
        {
            continue;
        }

        const bn::fixed_point offset = pick_drop_offset(rng, placed_cards, total_drop_slots);
        add_card_drop(offers[slot], g_session.spawn_x + offset.x(), g_session.spawn_y + offset.y(),
                      placed_cards * TOSS_STAGGER_FRAMES);
        ++placed_cards;
    }

    const bn::fixed_point paper_offset = pick_drop_offset(rng, placed_cards, total_drop_slots);
    add_paper_drop(g_session.spawn_x + paper_offset.x(), g_session.spawn_y + paper_offset.y(),
                   placed_cards * TOSS_STAGGER_FRAMES);
    g_session.selected_index = g_session.drop_count > 0 ? 0 : -1;
}

bool overworld_drops_active()
{
    return g_session.active;
}

bool overworld_drops_inspect_open()
{
    return g_session.inspect_open;
}

bool overworld_drops_has_selection()
{
    return g_session.active && g_session.selected_index >= 0;
}

bool overworld_drops_tick(bn::fixed player_x, bn::fixed player_y, const bn::fixed_point& camera)
{
    if(!g_session.active)
    {
        return false;
    }

    tick_toss_motion();
    g_session.selected_index = find_nearest_drop(player_x, player_y);

    if(g_session.inspect_open)
    {
        sync_inspect_visuals();

        if(bn::keypad::b_pressed() || bn::keypad::select_pressed())
        {
            clear_inspect();
        }
        else if(bn::keypad::a_pressed())
        {
            const int index = g_session.inspect_index;
            clear_inspect();
            try_pickup(index);
        }
    }
    else
    {
        if(bn::keypad::select_pressed() && g_session.selected_index >= 0)
        {
            open_inspect(g_session.selected_index);
        }
        else if(bn::keypad::a_pressed() && g_session.selected_index >= 0)
        {
            try_pickup(g_session.selected_index);
        }
    }

    update_drop_visuals(camera);
    advance_poof_frames();
    finish_session_if_empty();

    return g_session.inspect_open;
}

void overworld_drops_clear()
{
    reset_drop_session();
            hide_inspect_card(inspect_card());
}
