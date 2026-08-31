#include "card.h"

#include "bn_affine_mat_attributes.h"
#include "bn_array.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_compression_type.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_span.h"
#include "bn_sprite_item.h"
#include "bn_span.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_tile.h"
#include "card_data.h"
#include "card_instance.h"
#include "game_events.h"
#include "game_helpers.h"
#include "game_types.h"
#include "card_meta.h"
#include "game_state.h"
#include "trinket_system.h"
#include "scoring.h"

namespace
{
    constexpr bn::fixed BODY_W = 32;
    constexpr bn::fixed BODY_H = 64;
    constexpr bn::fixed ACCENT_W = 8;
    constexpr bn::fixed ACCENT_H = 32;

    // Source card art paints the frame with orange RGB(255,126,0) → GBA (31,15,0).
    constexpr bn::color BORDER_ORANGE(31, 15, 0);
    constexpr bn::color BORDER_COMMON(4, 12, 6);
    constexpr bn::color BORDER_UNCOMMON(4, 8, 22);
    constexpr bn::color BORDER_RARE(28, 22, 6);

    constexpr bn::color border_color_for(CardRarity rarity)
    {
        switch(rarity)
        {
        case CardRarity::UNCOMMON:
            return BORDER_UNCOMMON;
        case CardRarity::RARE:
            return BORDER_RARE;
        case CardRarity::COMMON:
        default:
            return BORDER_COMMON;
        }
    }

    constexpr int color_distance_sq(bn::color left, bn::color right)
    {
        const int dr = left.red() - right.red();
        const int dg = left.green() - right.green();
        const int db = left.blue() - right.blue();
        return dr * dr + dg * dg + db * db;
    }

    int find_border_palette_index(bn::span<const bn::color> colors)
    {
        int best_index = -1;
        int best_distance = 0x7FFFFFFF;

        for(int index = 1; index < colors.size() && index < 16; ++index)
        {
            const int distance = color_distance_sq(colors[index], BORDER_ORANGE);

            if(distance < best_distance)
            {
                best_distance = distance;
                best_index = index;
            }
        }

        // Reject near-misses so non-orange art is not recolored.
        constexpr int MAX_ORANGE_DISTANCE = 40;
        return best_distance <= MAX_ORANGE_DISTANCE ? best_index : -1;
    }

    void apply_rarity_border_palette(bn::sprite_ptr& body,
                                     bn::sprite_ptr& accent_top,
                                     bn::sprite_ptr& accent_bottom,
                                     CardType type)
    {
        bn::span<const bn::color> source = body.palette().colors();
        bn::array<bn::color, 16> colors;

        for(int index = 0; index < 16; ++index)
        {
            colors[index] = index < source.size() ? source[index] : bn::color();
        }

        const int border_index = find_border_palette_index(
            bn::span<const bn::color>(colors.data(), colors.size()));

        if(border_index > 0)
        {
            colors[border_index] = border_color_for(card_meta(type).rarity);
        }

        const bn::sprite_palette_item item(
            bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4,
            bn::compression_type::NONE);
        const bn::sprite_palette_ptr palette = bn::sprite_palette_ptr::create(item);

        body.set_palette(palette);
        accent_top.set_palette(palette);
        accent_bottom.set_palette(palette);
    }

    constexpr int TEXT_CARD_TILES_W = 4;
    constexpr int TEXT_CARD_TILES_H = 8;
    constexpr int TEXT_CARD_PIXEL_W = 32;
    constexpr int TEXT_CARD_TILE_COUNT = TEXT_CARD_TILES_W * TEXT_CARD_TILES_H;

    constexpr int TEXT_CARD_RARITY_COUNT = 3;

    bool g_text_card_tiles_ready = false;
    bn::optional<bn::sprite_tiles_ptr> g_text_card_tiles_ptr;
    bn::array<bn::color, 16> g_text_card_palette_colors_by_rarity[TEXT_CARD_RARITY_COUNT];
    bn::optional<bn::sprite_palette_ptr> g_text_card_palettes_by_rarity[TEXT_CARD_RARITY_COUNT];
    bool g_text_card_palette_colors_ready = false;

    void init_text_card_palette_colors()
    {
        if(g_text_card_palette_colors_ready)
        {
            return;
        }

        for(int rarity_index = 0; rarity_index < TEXT_CARD_RARITY_COUNT; ++rarity_index)
        {
            bn::array<bn::color, 16>& colors = g_text_card_palette_colors_by_rarity[rarity_index];
            colors[0] = bn::color(0, 0, 0);
            colors[1] = bn::color(14, 14, 18);
            colors[2] = border_color_for(CardRarity(rarity_index));

            for(int index = 3; index < 16; ++index)
            {
                colors[index] = bn::color(0, 0, 0);
            }
        }

        g_text_card_palette_colors_ready = true;
    }

    const bn::sprite_palette_ptr& text_card_palette_for(CardRarity rarity)
    {
        init_text_card_palette_colors();

        int rarity_index = int(rarity);

        if(rarity_index < 0 || rarity_index >= TEXT_CARD_RARITY_COUNT)
        {
            rarity_index = int(CardRarity::COMMON);
        }

        if(!g_text_card_palettes_by_rarity[rarity_index])
        {
            const bn::sprite_palette_item palette_item(
                bn::span<const bn::color>(g_text_card_palette_colors_by_rarity[rarity_index].data(), 16),
                bn::bpp_mode::BPP_4, bn::compression_type::NONE);
            g_text_card_palettes_by_rarity[rarity_index] = bn::sprite_palette_ptr::create(palette_item);
        }

        return g_text_card_palettes_by_rarity[rarity_index].value();
    }

    bool text_card_border_pixel(int global_x, int global_y)
    {
        return global_x == 0 || global_x == TEXT_CARD_PIXEL_W - 1 || global_y == 0 || global_y == 63;
    }

    void write_text_card_tiles(bn::span<bn::tile> dest)
    {
        for(int ty = 0; ty < TEXT_CARD_TILES_H; ++ty)
        {
            for(int tx = 0; tx < TEXT_CARD_TILES_W; ++tx)
            {
                bn::tile& tile = dest[ty * TEXT_CARD_TILES_W + tx];

                for(int py = 0; py < 8; ++py)
                {
                    uint32_t row = 0;

                    for(int px = 0; px < 8; ++px)
                    {
                        const int global_x = tx * 8 + px;
                        const int global_y = ty * 8 + py;
                        const int color_index = text_card_border_pixel(global_x, global_y) ? 2 : 1;
                        row |= uint32_t(color_index) << (px * 4);
                    }

                    tile.data[py] = row;
                }
            }
        }
    }

    void ensure_text_card_tiles()
    {
        if(g_text_card_tiles_ready)
        {
            return;
        }

        g_text_card_tiles_ptr = bn::sprite_tiles_ptr::allocate(TEXT_CARD_TILE_COUNT, bn::bpp_mode::BPP_4);
        bn::span<bn::tile> tiles_vram = g_text_card_tiles_ptr->vram().value();
        write_text_card_tiles(tiles_vram);
        g_text_card_tiles_ready = true;
    }

    void apply_text_card_body_tiles(bn::sprite_ptr& body, CardType type)
    {
        ensure_text_card_tiles();
        body.set_tiles_and_palette(bn::sprite_shape_size(32, 64), g_text_card_tiles_ptr.value(),
                                   text_card_palette_for(card_meta(type).rarity));
    }

    int face_line_char_limit()
    {
        return 10;
    }

    void append_face_word(bn::string<32>& line, bn::string<32>& next_line, const bn::string_view& word)
    {
        if(word.empty())
        {
            return;
        }

        const int extra = line.empty() ? word.size() : word.size() + 1;

        if(line.size() + extra <= face_line_char_limit())
        {
            if(!line.empty())
            {
                line.append(' ');
            }

            line.append(word);
            return;
        }

        if(next_line.empty() && word.size() <= face_line_char_limit())
        {
            next_line.append(word);
            return;
        }

        if(next_line.size() + extra <= face_line_char_limit())
        {
            if(!next_line.empty())
            {
                next_line.append(' ');
            }

            next_line.append(word);
        }
    }

    void format_face_name_lines(const char* name, bn::string<32>& line_a, bn::string<32>& line_b)
    {
        line_a.clear();
        line_b.clear();

        if(!name || name[0] == '\0')
        {
            return;
        }

        bn::string<32> word;

        for(int index = 0; name[index] != '\0'; ++index)
        {
            const char character = name[index];

            if(character == ' ')
            {
                append_face_word(line_a, line_b, word);
                word.clear();
            }
            else
            {
                word.push_back(character);
            }
        }

        append_face_word(line_a, line_b, word);
    }


    bn::string<32> serialize_face_stat_segments(const bn::span<const CardFaceStatSegment>& segments)
    {
        bn::string<32> key;

        for(int index = 0; index < segments.size(); ++index)
        {
            if(index > 0)
            {
                key.append('|');
            }

            key.append(char('0' + int(segments[index].color)));
            key.append(':');
            key.append(segments[index].text);
        }

        return key;
    }

    const bn::sprite_palette_ptr& card_stat_green_palette(const bn::sprite_ptr& sample)
    {
        static bn::optional<bn::sprite_palette_ptr> palette;
        constexpr bn::color CARD_STAT_GREEN(6, 28, 10);

        if(!palette.has_value())
        {
            bn::array<bn::color, 16> colors;
            const bn::span<const bn::color> source = sample.palette().colors();

            for(int index = 0; index < 16; ++index)
            {
                colors[index] = index < source.size() ? source[index] : bn::color();
            }

            for(int index = 1; index < 16; ++index)
            {
                if(colors[index].red() + colors[index].green() + colors[index].blue() > 24)
                {
                    colors[index] = CARD_STAT_GREEN;
                }
            }

            const bn::sprite_palette_item item(
                bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
            palette = bn::sprite_palette_ptr::create(item);
        }

        return *palette;
    }

    const bn::sprite_palette_ptr& card_stat_gold_palette(const bn::sprite_ptr& sample)
    {
        static bn::optional<bn::sprite_palette_ptr> palette;
        constexpr bn::color CARD_STAT_GOLD(31, 25, 5);

        if(!palette.has_value())
        {
            bn::array<bn::color, 16> colors;
            const bn::span<const bn::color> source = sample.palette().colors();

            for(int index = 0; index < 16; ++index)
            {
                colors[index] = index < source.size() ? source[index] : bn::color();
            }

            for(int index = 1; index < 16; ++index)
            {
                if(colors[index].red() + colors[index].green() + colors[index].blue() > 24)
                {
                    colors[index] = CARD_STAT_GOLD;
                }
            }

            const bn::sprite_palette_item item(
                bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
            palette = bn::sprite_palette_ptr::create(item);
        }

        return *palette;
    }

    void apply_card_stat_palette(bn::span<bn::sprite_ptr> sprites, CardStatColor color)
    {
        if(sprites.empty() || color == CardStatColor::DEFAULT)
        {
            return;
        }

        const bn::sprite_palette_ptr& palette =
            color == CardStatColor::GREEN ? card_stat_green_palette(sprites[0])
                                          : card_stat_gold_palette(sprites[0]);

        for(bn::sprite_ptr& sprite : sprites)
        {
            sprite.set_palette(palette);
        }
    }

    void generate_face_stat_segments(bn::sprite_text_generator& generator, int center_x, int center_y,
                                     const bn::span<const CardFaceStatSegment>& segments,
                                     bn::vector<bn::sprite_ptr, 8>& output_sprites)
    {
        if(segments.empty())
        {
            return;
        }

        int total_width = 0;

        for(int index = 0; index < segments.size(); ++index)
        {
            total_width += generator.width(segments[index].text);

            if(index + 1 < segments.size())
            {
                total_width += generator.font().space_between_characters();
            }
        }

        int cursor_x = center_x - total_width / 2;

        for(int index = 0; index < segments.size(); ++index)
        {
            const CardFaceStatSegment& segment = segments[index];

            if(segment.text.empty())
            {
                continue;
            }

            const int before = output_sprites.size();
            generator.set_left_alignment();
            generator.generate(cursor_x, center_y, segment.text, output_sprites);
            generator.set_center_alignment();

            const int added = output_sprites.size() - before;

            if(added > 0)
            {
                apply_card_stat_palette(
                    bn::span<bn::sprite_ptr>(output_sprites.data() + before, added), segment.color);
            }

            cursor_x += generator.width(segment.text);

            if(index + 1 < segments.size())
            {
                cursor_x += generator.font().space_between_characters();
            }
        }
    }
}

Card::Card() :
    _type(CardType::SIPS),
    _x(0),
    _y(0),
    _body(card_data(CardType::SIPS).body_item->create_sprite(0, 0)),
    _accent_top(card_data(CardType::SIPS).accent_top_item->create_sprite(0, 0)),
    _accent_bottom(card_data(CardType::SIPS).accent_bottom_item->create_sprite(0, 0))
{
    apply_rarity_border_palette(_body, _accent_top, _accent_bottom, _type);
    apply_draw_layering();
    set_visible(false);
}

Card::Card(CardType type, bn::fixed x, bn::fixed y) :
    _type(type),
    _x(x),
    _y(y),
    _body(card_data(type).body_item->create_sprite(x + BODY_W / 2, y + BODY_H / 2)),
    _accent_top(card_data(type).accent_top_item->create_sprite(x + BODY_W + ACCENT_W / 2,
                                                               y + ACCENT_H / 2)),
    _accent_bottom(card_data(type).accent_bottom_item->create_sprite(x + BODY_W + ACCENT_W / 2,
                                                                     y + BODY_H - ACCENT_H / 2))
{
    apply_rarity_border_palette(_body, _accent_top, _accent_bottom, _type);
    apply_draw_layering();
}

void apply_card_play(GameState& state, CardType type)
{
    apply_card_play(state, CardRef{type, NO_INSTANCE}, PlaySource::HAND);
}

void apply_card_play(GameState& state, CardRef card)
{
    apply_card_play(state, card, PlaySource::HAND);
}

void apply_card_play(GameState& state, CardRef card, PlaySource source)
{
    if(state.build_a_number_active && !state.applying_build_a_number_payout)
    {
        return;
    }

    if(source == PlaySource::GHOST)
    {
        battle_stat_record_ghost(state);
    }

    const CardData& d = card_data(card.type);
    const bool consume_double = state.pending_double_adds;
    state.applying_double_adds = consume_double;

    if(consume_double)
    {
        state.pending_double_adds = false;
    }

    int plus = (source == PlaySource::GHOST && d.has_ghost) ? d.ghost_plus : d.immediate_plus;
    int multiply = d.immediate_multiply;

    if(source != PlaySource::GHOST)
    {
        if(const CardInstance* instance = instance_at(state.instance_pool, card.instance_id))
        {
            plus = effective_immediate_plus(*instance);
            multiply = effective_immediate_multiply(*instance);
        }
    }

    if(plus)
    {
        state.add_from_card(plus);
        staircase_on_card_plus(state, plus);
    }

    if(multiply)
    {
        state.mul_from_card(multiply);
    }

    for(int index = 0; index < 3; ++index)
    {
        RoundModifier future = d.future[index];

        if(state.applying_double_adds && future.positive)
        {
            future.positive *= 2;
        }

        if(index == 0)
        {
            state.mod_next().accumulate(future);
        }
        else if(index == 1)
        {
            state.mod_after_next().accumulate(future);
        }
        else
        {
            state.mod_third().accumulate(future);
        }
    }

    state.play_effect_card = card;

    if(card.type == CardType::LIFELINE && source == PlaySource::GHOST)
    {
        lifeline_ghost_play(state);
    }
    else if(card.type == CardType::EVALUATE && source == PlaySource::GHOST)
    {
        state.queue_evaluate_ghost_steps();
    }
    else if(d.on_play)
    {
        d.on_play(state);
    }

    state.play_effect_card = {};
    state.applying_double_adds = false;
}

void apply_card_discard(GameState& state, CardType type)
{
    const CardData& data = card_data(type);

    if(data.on_discard)
    {
        data.on_discard(state);
    }
}

void apply_card_relocated(GameState& state, CardType type)
{
    if(type != CardType::GET_ME_OUTA_HERE)
    {
        return;
    }

    state.add_from_card(card_data(type).immediate_plus);
}

void apply_card_relocated_from_play(GameState& state, CardType type, PlaySource source)
{
    switch(source)
    {
    case PlaySource::DECK_TOP:
    case PlaySource::SCRY:
    case PlaySource::DECK_SEARCH:
        apply_card_relocated(state, type);
        break;

    default:
        break;
    }
}

void Card::play(GameState& state) const
{
    apply_card_play(state, _type);
}

void Card::discard(GameState& state) const
{
    apply_card_discard(state, _type);
}

void Card::set_type(CardType type)
{
    if(_type == type)
    {
        apply_draw_layering();
        return;
    }

    _type = type;
    clear_face_labels();

    const CardData& data = card_data(type);

    if(data.text_only)
    {
        apply_text_card_body_tiles(_body, _type);
    }
    else
    {
        _body.set_item(*data.body_item);
        _accent_top.set_item(*data.accent_top_item);
        _accent_bottom.set_item(*data.accent_bottom_item);
        apply_rarity_border_palette(_body, _accent_top, _accent_bottom, _type);
    }

    reposition_parts();
    apply_draw_layering();
}

void Card::sync_part_visibility()
{
    const bool show_accents = _visible && !card_data(_type).text_only;

    _body.set_visible(_visible);
    _accent_top.set_visible(show_accents);
    _accent_bottom.set_visible(show_accents);
    sync_upgrade_pip_visibility();
    sync_amount_overlay_visibility();
    sync_face_label_visibility();
}

void Card::reposition_parts()
{
    _body.set_position(_x + BODY_W / 2, _y + BODY_H / 2);
    _accent_top.set_position(_x + BODY_W + ACCENT_W / 2, _y + ACCENT_H / 2);
    _accent_bottom.set_position(_x + BODY_W + ACCENT_W / 2,
                                _y + BODY_H - ACCENT_H / 2);
    reposition_upgrade_pips();
    reposition_amount_overlay();
    reposition_face_labels();
}

void Card::sync_upgrade_pip_visibility()
{
    const bool show = _visible && (!_visual_active || _visual_overlays);

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        sprite.set_visible(show);
    }
}

void Card::reposition_upgrade_pips()
{
    if(_upgrade_pips.empty())
    {
        return;
    }

    const bn::fixed new_anchor_x = _x + 2;
    const bn::fixed new_anchor_y = _y + 2;
    const bn::fixed dx = new_anchor_x - _pip_anchor_x;
    const bn::fixed dy = new_anchor_y - _pip_anchor_y;

    if(dx != 0 || dy != 0)
    {
        for(bn::sprite_ptr& sprite : _upgrade_pips)
        {
            sprite.set_position(sprite.x() + dx, sprite.y() + dy);
        }

        _pip_anchor_x = new_anchor_x;
        _pip_anchor_y = new_anchor_y;
    }
}

void Card::sync_amount_overlay_visibility()
{
    const bool show = _visible && (!_visual_active || _visual_overlays);

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        sprite.set_visible(show);
    }
}

void Card::reposition_amount_overlay()
{
    if(_amount_overlay.empty())
    {
        return;
    }

    const bn::fixed new_anchor_x = _x + 4;
    const bn::fixed new_anchor_y = _y + 48;
    const bn::fixed dx = new_anchor_x - _amount_anchor_x;
    const bn::fixed dy = new_anchor_y - _amount_anchor_y;

    if(dx != 0 || dy != 0)
    {
        for(bn::sprite_ptr& sprite : _amount_overlay)
        {
            sprite.set_position(sprite.x() + dx, sprite.y() + dy);
        }

        _amount_anchor_x = new_anchor_x;
        _amount_anchor_y = new_anchor_y;
    }
}

void Card::clear_face_labels()
{
    _face_name_sprites.clear();
    _face_stat_sprites.clear();
    _face_name_text.clear();
    _face_stat_text.clear();
    _face_label_generator = nullptr;
    _face_anchor_x = 0;
    _face_anchor_y = 0;
}

void Card::sync_face_label_visibility()
{
    const bool show = _visible && (!_visual_active || _visual_overlays) && card_data(_type).text_only;

    for(bn::sprite_ptr& sprite : _face_name_sprites)
    {
        sprite.set_visible(show);
    }

    for(bn::sprite_ptr& sprite : _face_stat_sprites)
    {
        sprite.set_visible(show);
    }
}

void Card::reposition_face_labels()
{
    if(_face_name_sprites.empty() && _face_stat_sprites.empty())
    {
        return;
    }

    const bn::fixed dx = _x - _face_anchor_x;
    const bn::fixed dy = _y - _face_anchor_y;

    if(dx == 0 && dy == 0)
    {
        return;
    }

    for(bn::sprite_ptr& sprite : _face_name_sprites)
    {
        sprite.set_position(sprite.x() + dx, sprite.y() + dy);
    }

    for(bn::sprite_ptr& sprite : _face_stat_sprites)
    {
        sprite.set_position(sprite.x() + dx, sprite.y() + dy);
    }

    _face_anchor_x = _x;
    _face_anchor_y = _y;
}

void Card::sync_face_labels(bn::sprite_text_generator* generator, const GameState* state, CardRef ref,
                            const CardInstance* instance, bool in_graveyard)
{
    if(!generator || !card_data(_type).text_only)
    {
        clear_face_labels();
        return;
    }

    bn::string<32> line_a;
    bn::string<32> line_b;
    format_face_name_lines(card_data(_type).name, line_a, line_b);

    bn::vector<CardFaceStatSegment, 4> stat_segments;
    format_card_face_stat(state, ref, instance, in_graveyard, stat_segments);
    const bn::string<32> stat_line =
        serialize_face_stat_segments(bn::span<const CardFaceStatSegment>(stat_segments.data(), stat_segments.size()));

    bn::string<40> name_block;

    if(!line_a.empty())
    {
        name_block.append(line_a);

        if(!line_b.empty())
        {
            name_block.append('|');
            name_block.append(line_b);
        }
    }

    if(generator == _face_label_generator && name_block == _face_name_text && stat_line == _face_stat_text &&
       !_face_name_sprites.empty())
    {
        reposition_face_labels();
        sync_face_label_visibility();
        apply_draw_layering();
        return;
    }

    _face_name_sprites.clear();
    _face_stat_sprites.clear();
    _face_name_text = name_block;
    _face_stat_text = stat_line;
    _face_label_generator = generator;
    _face_anchor_x = _x;
    _face_anchor_y = _y;

    generator->set_center_alignment();

    if(!line_a.empty())
    {
        generator->generate(_x.integer() + BODY_W.integer() / 2, _y.integer() + 12, line_a, _face_name_sprites);
    }

    if(!line_b.empty())
    {
        bn::vector<bn::sprite_ptr, 12> second_line;
        generator->generate(_x.integer() + BODY_W.integer() / 2, _y.integer() + 22, line_b, second_line);

        for(bn::sprite_ptr& sprite : second_line)
        {
            if(_face_name_sprites.full())
            {
                break;
            }

            _face_name_sprites.push_back(sprite);
        }
    }

    if(!stat_segments.empty())
    {
        generate_face_stat_segments(
            *generator, _x.integer() + BODY_W.integer() / 2, _y.integer() + 52,
            bn::span<const CardFaceStatSegment>(stat_segments.data(), stat_segments.size()), _face_stat_sprites);
    }

    sync_face_label_visibility();
    apply_draw_layering();
}

void Card::clear_amount_overlay()
{
    _amount_overlay.clear();
    _amount_overlay_text.clear();
    _amount_overlay_generator = nullptr;
    _amount_anchor_x = 0;
    _amount_anchor_y = 0;
}

void Card::set_amount_overlay(bn::sprite_text_generator* generator, const bn::string<8>& text)
{
    if(!generator || text.empty())
    {
        clear_amount_overlay();
        return;
    }

    if(generator == _amount_overlay_generator && text == _amount_overlay_text && !_amount_overlay.empty())
    {
        reposition_amount_overlay();
        sync_amount_overlay_visibility();
        return;
    }

    _amount_overlay.clear();
    _amount_overlay_text = text;
    _amount_overlay_generator = generator;
    _amount_anchor_x = _x + 4;
    _amount_anchor_y = _y + 48;
    generator->set_left_alignment();
    generator->generate(_amount_anchor_x.integer(), _amount_anchor_y.integer(), text, _amount_overlay);

    sync_amount_overlay_visibility();
    apply_draw_layering();
}

void Card::clear_upgrade_pips()
{
    _upgrade_pips.clear();
    _upgrade_pip_text.clear();
    _upgrade_pip_generator = nullptr;
    _pip_anchor_x = 0;
    _pip_anchor_y = 0;
}

void Card::set_upgrade_pips(bn::sprite_text_generator* generator, const CardInstance* instance)
{
    if(!generator || !instance || !instance_has_upgrades(*instance))
    {
        clear_upgrade_pips();
        return;
    }

    bn::string<8> pips;
    format_instance_upgrade_pips(*instance, pips);

    if(pips.empty())
    {
        clear_upgrade_pips();
        return;
    }

    if(generator == _upgrade_pip_generator && pips == _upgrade_pip_text && !_upgrade_pips.empty())
    {
        reposition_upgrade_pips();
        sync_upgrade_pip_visibility();
        apply_draw_layering();
        return;
    }

    _upgrade_pips.clear();
    _upgrade_pip_text = pips;
    _upgrade_pip_generator = generator;
    _pip_anchor_x = _x + 2;
    _pip_anchor_y = _y + 2;
    generator->set_left_alignment();
    generator->generate(_pip_anchor_x.integer(), _pip_anchor_y.integer(), pips, _upgrade_pips);

    sync_upgrade_pip_visibility();
    apply_draw_layering();
}

void Card::set_position(bn::fixed x, bn::fixed y)
{
    _x = x;
    _y = y;

    if(_visual_active)
    {
        apply_visual_transform();
        return;
    }

    reposition_parts();
}

void Card::set_visible(bool visible)
{
    _visible = visible;

    if(!visible && _visual_active)
    {
        clear_visual();
        return;
    }

    sync_part_visibility();
}

void Card::set_blending_enabled(bool blending_enabled)
{
    _body.set_blending_enabled(blending_enabled);
    _accent_top.set_blending_enabled(blending_enabled);
    _accent_bottom.set_blending_enabled(blending_enabled);

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        sprite.set_blending_enabled(blending_enabled);
    }

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        sprite.set_blending_enabled(blending_enabled);
    }
}

void Card::apply_draw_layering()
{
    const int z = _draw_on_top ? game_layout::PLAY_PRESENTATION_CARD_Z : game_layout::HAND_CARD_Z;
    const int face_z = _draw_on_top ? game_layout::PLAY_PRESENTATION_CARD_Z + 1 : game_layout::CARD_FACE_TEXT_Z;
    const int body_bg_priority = _draw_on_top ? 0 : 2;
    const int face_bg_priority = _draw_on_top ? 0 : 1;
    const int overlay_z = _draw_on_top ? game_layout::PLAY_PRESENTATION_CARD_Z - 1 : game_layout::HAND_CARD_Z;

    _body.set_z_order(z);
    _body.set_bg_priority(body_bg_priority);
    _accent_top.set_z_order(z);
    _accent_top.set_bg_priority(body_bg_priority);
    _accent_bottom.set_z_order(z);
    _accent_bottom.set_bg_priority(body_bg_priority);

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        sprite.set_z_order(overlay_z);
        sprite.set_bg_priority(body_bg_priority);
    }

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        sprite.set_z_order(overlay_z);
        sprite.set_bg_priority(body_bg_priority);
    }

    for(bn::sprite_ptr& sprite : _face_name_sprites)
    {
        sprite.set_z_order(face_z);
        sprite.set_bg_priority(face_bg_priority);
    }

    for(bn::sprite_ptr& sprite : _face_stat_sprites)
    {
        sprite.set_z_order(face_z);
        sprite.set_bg_priority(face_bg_priority);
    }
}

void Card::set_draw_on_top(bool on_top)
{
    _draw_on_top = on_top;
    apply_draw_layering();
}

void Card::set_visual(bn::fixed scale, bn::fixed rotation_degrees)
{
    if(scale == 1 && rotation_degrees == 0)
    {
        clear_visual();
        return;
    }

    _visual_scale = scale > bn::fixed(0.05) ? scale : bn::fixed(0.05);
    _visual_rotation = rotation_degrees;
    _visual_active = true;
    apply_visual_transform();
}

void Card::set_inspect_visual(bn::fixed scale)
{
    _visual_overlays = true;
    set_visual(scale, 0);
}

void Card::apply_visual_transform()
{
    if(!_visual_active)
    {
        return;
    }

    const bn::fixed pivot_x = _x + (BODY_W + ACCENT_W) / 2;
    const bn::fixed pivot_y = _y + BODY_H / 2;
    const bn::fixed body_ox = BODY_W / 2 - (BODY_W + ACCENT_W) / 2;
    const bn::fixed accent_ox = BODY_W + ACCENT_W / 2 - (BODY_W + ACCENT_W) / 2;
    const bn::fixed accent_top_oy = ACCENT_H / 2 - BODY_H / 2;
    const bn::fixed accent_bottom_oy = (BODY_H - ACCENT_H / 2) - BODY_H / 2;

    if(_visual_rotation == 0)
    {
        if(!_affine_mat.has_value())
        {
            _affine_mat = bn::sprite_affine_mat_ptr::create();
        }

        bn::affine_mat_attributes attrs;
        attrs.set_scale(_visual_scale);
        _affine_mat->set_attributes(attrs);

        auto place_scaled = [&](bn::sprite_ptr& sprite, bn::fixed offset_x, bn::fixed offset_y)
        {
            sprite.set_position(pivot_x + offset_x * _visual_scale, pivot_y + offset_y * _visual_scale);
            sprite.set_affine_mat(*_affine_mat);
        };

        place_scaled(_body, body_ox, 0);
        place_scaled(_accent_top, accent_ox, accent_top_oy);
        place_scaled(_accent_bottom, accent_ox, accent_bottom_oy);

        if(_visual_overlays)
        {
            auto place_overlay = [&](bn::sprite_ptr& sprite)
            {
                const bn::fixed offset_x = sprite.x() - pivot_x;
                const bn::fixed offset_y = sprite.y() - pivot_y;
                sprite.set_position(pivot_x + offset_x * _visual_scale,
                                    pivot_y + offset_y * _visual_scale);
                sprite.set_affine_mat(*_affine_mat);
            };

            for(bn::sprite_ptr& sprite : _upgrade_pips)
            {
                place_overlay(sprite);
            }

            for(bn::sprite_ptr& sprite : _amount_overlay)
            {
                place_overlay(sprite);
            }
        }

        sync_part_visibility();
        return;
    }

    _body.remove_affine_mat();
    _accent_top.remove_affine_mat();
    _accent_bottom.remove_affine_mat();

    if(!_affine_mat.has_value())
    {
        _affine_mat = bn::sprite_affine_mat_ptr::create();
    }

    bn::affine_mat_attributes attrs;
    attrs.set_scale(_visual_scale);
    attrs.set_rotation_angle(_visual_rotation);
    _affine_mat->set_attributes(attrs);

    bn::fixed sin = bn::sin(_visual_rotation);
    bn::fixed cos = bn::cos(_visual_rotation);

    auto place_part = [&](bn::sprite_ptr& sprite, bn::fixed offset_x, bn::fixed offset_y)
    {
        const bn::fixed rotated_x = offset_x * cos - offset_y * sin;
        const bn::fixed rotated_y = offset_x * sin + offset_y * cos;
        sprite.set_position(pivot_x + rotated_x, pivot_y + rotated_y);
        sprite.set_affine_mat(*_affine_mat);
    };

    place_part(_body, body_ox, 0);
    place_part(_accent_top, accent_ox, accent_top_oy);
    place_part(_accent_bottom, accent_ox, accent_bottom_oy);
    sync_part_visibility();
}

void Card::clear_visual()
{
    const bn::fixed previous_scale = _visual_scale;
    const bool restore_overlays = _visual_active && _visual_overlays &&
                                  _visual_rotation == 0 && previous_scale != 0;
    const bn::fixed pivot_x = _x + (BODY_W + ACCENT_W) / 2;
    const bn::fixed pivot_y = _y + BODY_H / 2;

    auto restore_overlay = [&](bn::sprite_ptr& sprite)
    {
        if(restore_overlays)
        {
            sprite.set_position(pivot_x + (sprite.x() - pivot_x) / previous_scale,
                                pivot_y + (sprite.y() - pivot_y) / previous_scale);
        }

        sprite.remove_affine_mat();
    };

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        restore_overlay(sprite);
    }

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        restore_overlay(sprite);
    }

    _visual_active = false;
    _visual_overlays = false;
    _visual_scale = 1;
    _visual_rotation = 0;
    _affine_mat.reset();
    _body.remove_affine_mat();
    _accent_top.remove_affine_mat();
    _accent_bottom.remove_affine_mat();
    reposition_parts();
    sync_part_visibility();
}

void release_card_display_tiles(Card& card)
{
    card.clear_upgrade_pips();
    card.clear_amount_overlay();
    card.clear_face_labels();
    card.set_visible(false);
    card.clear_visual();
    card.set_blending_enabled(false);
    card.set_draw_on_top(false);
    card.set_type(CARD_DISPLAY_PLACEHOLDER);
}
