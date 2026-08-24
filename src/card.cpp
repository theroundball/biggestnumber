#include "card.h"

#include "bn_affine_mat_attributes.h"
#include "bn_array.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_span.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "card_data.h"
#include "card_instance.h"
#include "game_events.h"
#include "game_types.h"
#include "card_meta.h"
#include "game_state.h"
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
            bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
        // create() reuses an existing bank when colors already match (per rarity).
        const bn::sprite_palette_ptr palette = bn::sprite_palette_ptr::create(item);

        body.set_palette(palette);
        accent_top.set_palette(palette);
        accent_bottom.set_palette(palette);
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
    if(source == PlaySource::FLASHBACK)
    {
        battle_stat_record_flashback(state);
    }

    const CardData& d = card_data(card.type);
    const bool consume_double = state.pending_double_adds;
    state.applying_double_adds = consume_double;

    if(consume_double)
    {
        state.pending_double_adds = false;
    }

    int plus = (source == PlaySource::FLASHBACK && d.has_flashback) ? d.flashback_plus : d.immediate_plus;
    int multiply = d.immediate_multiply;

    if(source != PlaySource::FLASHBACK)
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

    if(d.on_play)
    {
        d.on_play(state);
    }

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
        return;
    }

    _type = type;

    const CardData& data = card_data(type);
    _body.set_item(*data.body_item);
    _accent_top.set_item(*data.accent_top_item);
    _accent_bottom.set_item(*data.accent_bottom_item);
    apply_rarity_border_palette(_body, _accent_top, _accent_bottom, _type);
}

void Card::sync_part_visibility()
{
    _body.set_visible(_visible);
    _accent_top.set_visible(_visible);
    _accent_bottom.set_visible(_visible);
    sync_upgrade_pip_visibility();
    sync_amount_overlay_visibility();
}

void Card::reposition_parts()
{
    _body.set_position(_x + BODY_W / 2, _y + BODY_H / 2);
    _accent_top.set_position(_x + BODY_W + ACCENT_W / 2, _y + ACCENT_H / 2);
    _accent_bottom.set_position(_x + BODY_W + ACCENT_W / 2,
                                _y + BODY_H - ACCENT_H / 2);
    reposition_upgrade_pips();
    reposition_amount_overlay();
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

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        sprite.set_z_order(game_layout::HAND_CARD_Z);
    }

    sync_amount_overlay_visibility();
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
        return;
    }

    _upgrade_pips.clear();
    _upgrade_pip_text = pips;
    _upgrade_pip_generator = generator;
    _pip_anchor_x = _x + 2;
    _pip_anchor_y = _y + 2;
    generator->set_left_alignment();
    generator->generate(_pip_anchor_x.integer(), _pip_anchor_y.integer(), pips, _upgrade_pips);

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        sprite.set_z_order(game_layout::HAND_CARD_Z);
    }

    sync_upgrade_pip_visibility();
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

void Card::set_draw_on_top(bool on_top)
{
    const int z = on_top ? game_layout::PLAY_PRESENTATION_CARD_Z : game_layout::HAND_CARD_Z;
    const int bg_priority = on_top ? 0 : 2;

    _body.set_z_order(z);
    _body.set_bg_priority(bg_priority);
    _accent_top.set_z_order(z);
    _accent_top.set_bg_priority(bg_priority);
    _accent_bottom.set_z_order(z);
    _accent_bottom.set_bg_priority(bg_priority);

    for(bn::sprite_ptr& sprite : _upgrade_pips)
    {
        sprite.set_z_order(on_top ? game_layout::PLAY_PRESENTATION_CARD_Z - 1 : game_layout::HAND_CARD_Z);
        sprite.set_bg_priority(bg_priority);
    }

    for(bn::sprite_ptr& sprite : _amount_overlay)
    {
        sprite.set_z_order(on_top ? game_layout::PLAY_PRESENTATION_CARD_Z - 1 : game_layout::HAND_CARD_Z);
        sprite.set_bg_priority(bg_priority);
    }
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
    card.set_visible(false);
    card.clear_visual();
    card.set_blending_enabled(false);
    card.set_draw_on_top(false);
    card.set_type(CARD_DISPLAY_PLACEHOLDER);
}
