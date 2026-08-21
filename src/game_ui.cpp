#include "game_ui.h"

#include "bn_color.h"
#include "bn_optional.h"
#include "bn_sprite_item.h"
#include "bn_sprite_items_hud_trinket_echo.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_tile.h"
#include "game_types.h"

namespace
{
    constexpr bn::array<bn::color, 16> FADE_COLORS = {
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item FADE_PALETTE(
        bn::span<const bn::color>(FADE_COLORS.data(), FADE_COLORS.size()), bn::bpp_mode::BPP_4);

    constexpr bn::array<bn::color, 16> X_MARKER_COLORS = {
        bn::color(0, 0, 0), bn::color(31, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item X_MARKER_PALETTE(
        bn::span<const bn::color>(X_MARKER_COLORS.data(), X_MARKER_COLORS.size()), bn::bpp_mode::BPP_4);

    constexpr bn::array<bn::color, 16> LOCK_MARKER_COLORS = {
        bn::color(0, 0, 0), bn::color(6, 28, 12), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item LOCK_MARKER_PALETTE(
        bn::span<const bn::color>(LOCK_MARKER_COLORS.data(), LOCK_MARKER_COLORS.size()),
        bn::bpp_mode::BPP_4);

    constexpr bn::array<bn::color, 16> PLACEHOLDER_COLORS = {
        bn::color(0, 0, 0), bn::color(20, 20, 24), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item PLACEHOLDER_PALETTE(
        bn::span<const bn::color>(PLACEHOLDER_COLORS.data(), PLACEHOLDER_COLORS.size()),
        bn::bpp_mode::BPP_4);

    constexpr int PLACEHOLDER_TILES_PER_AXIS = 4;
    constexpr int PLACEHOLDER_TILE_COUNT =
        PLACEHOLDER_TILES_PER_AXIS * PLACEHOLDER_TILES_PER_AXIS;

    uint32_t solid_tile_row(int color_index)
    {
        uint32_t row = 0;

        for(int px = 0; px < 8; ++px)
        {
            row |= uint32_t(color_index) << (px * 4);
        }

        return row;
    }

    bool marker_x_pixel(int x, int y)
    {
        const int a = x - y >= 0 ? x - y : y - x;
        const int b = x - (15 - y) >= 0 ? x - (15 - y) : (15 - y) - x;
        return a <= 1 || b <= 1;
    }

    // Checkmark for Roll Over's locked first pick.
    bool marker_lock_pixel(int x, int y)
    {
        // Stem
        if(x >= 3 && x <= 6 && y >= 8 && y <= 12)
        {
            const int t = y - 8;
            return x >= 3 + t / 2 && x <= 5 + t / 2;
        }

        // Arm
        if(y >= 4 && y <= 9)
        {
            const int t = 9 - y;
            return x >= 6 + t && x <= 8 + t && x <= 13;
        }

        return false;
    }

    constexpr bn::array<bn::color, 16> SWIVEL_BADGE_COLORS = {
        bn::color(0, 0, 0), bn::color(22, 10, 28), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item SWIVEL_BADGE_PALETTE(
        bn::span<const bn::color>(SWIVEL_BADGE_COLORS.data(), SWIVEL_BADGE_COLORS.size()),
        bn::bpp_mode::BPP_4);

    bool swivel_badge_pixel(int x, int y)
    {
        const int center_x = 7;
        const int center_y = 7;
        const int dx = x - center_x;
        const int dy = y - center_y;
        const int distance_sq = dx * dx + dy * dy;

        if(distance_sq >= 12 && distance_sq <= 30)
        {
            return true;
        }

        return (x == 2 && y >= 5 && y <= 9) || (x == 13 && y >= 6 && y <= 10);
    }
}

GameFadeBand::GameFadeBand(int x, int color_index, int card_y) :
    _tiles(bn::sprite_tiles_ptr::allocate(4, bn::bpp_mode::BPP_4)),
    _palette(bn::sprite_palette_ptr::create(FADE_PALETTE)),
    _top_sprite(bn::sprite_ptr::create(x, card_y + 16, bn::sprite_shape_size(8, 32), _tiles, _palette)),
    _bottom_sprite(bn::sprite_ptr::create(x, card_y + 48, bn::sprite_shape_size(8, 32), _tiles, _palette))
{
    auto vram = _tiles.vram();
    auto* tile_span = vram.get();

    if(tile_span)
    {
        for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
        {
            bn::tile& tile = (*tile_span)[tile_index];

            for(int row_index = 0; row_index < 8; ++row_index)
            {
                uint32_t row = 0;

                for(int pixel_index = 0; pixel_index < 8; ++pixel_index)
                {
                    const bool opaque = (pixel_index + row_index + color_index) % 5 < color_index;
                    row |= uint32_t(opaque) << (pixel_index * 4);
                }

                tile.data[row_index] = row;
            }
        }
    }

    _top_sprite.set_blending_enabled(true);
    _bottom_sprite.set_blending_enabled(true);
    _top_sprite.set_z_order(-1);
    _bottom_sprite.set_z_order(-1);
    _top_sprite.set_visible(false);
    _bottom_sprite.set_visible(false);
}

void GameFadeBand::set_visible(bool visible)
{
    _top_sprite.set_visible(visible);
    _bottom_sprite.set_visible(visible);
}

void GameFadeBand::set_position_y(int card_y)
{
    _top_sprite.set_y(card_y + 16);
    _bottom_sprite.set_y(card_y + 48);
}

void GameFadeBand::set_position_x(int x)
{
    _top_sprite.set_x(x);
    _bottom_sprite.set_x(x);
}

GameMarker::GameMarker(Style style) :
    _style(style),
    _tiles(bn::sprite_tiles_ptr::allocate(4, bn::bpp_mode::BPP_4)),
    _palette(bn::sprite_palette_ptr::create(style == Style::LOCK ? LOCK_MARKER_PALETTE : X_MARKER_PALETTE)),
    _sprite(bn::sprite_ptr::create(0, 0, bn::sprite_shape_size(16, 16), _tiles, _palette))
{
    _sprite.set_z_order(game_layout::MARKER_Z_ORDER);
    _sprite.set_bg_priority(0);
    paint();
    _sprite.set_visible(false);
}

void GameMarker::set_position(int x, int y)
{
    _sprite.set_position(x, y);
}

void GameMarker::set_visible(bool visible)
{
    _sprite.set_visible(visible);
}

void GameMarker::paint()
{
    auto vram = _tiles.vram();
    auto* tile_span = vram.get();

    if(! tile_span)
    {
        return;
    }

    for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
    {
        bn::tile& tile = (*tile_span)[tile_index];
        const int tile_col = tile_index % 2;
        const int tile_row = tile_index / 2;

        for(int py = 0; py < 8; ++py)
        {
            uint32_t row = 0;

            for(int px = 0; px < 8; ++px)
            {
                const int x = tile_col * 8 + px;
                const int y = tile_row * 8 + py;
                const bool on = _style == Style::LOCK ? marker_lock_pixel(x, y) : marker_x_pixel(x, y);
                row |= uint32_t(on ? 1 : 0) << (px * 4);
            }

            tile.data[py] = row;
        }
    }
}

GraveyardPickPlaceholder::GraveyardPickPlaceholder() :
    _tiles(bn::sprite_tiles_ptr::allocate(PLACEHOLDER_TILE_COUNT, bn::bpp_mode::BPP_4)),
    _palette(bn::sprite_palette_ptr::create(PLACEHOLDER_PALETTE)),
    _sprite(bn::sprite_ptr::create(
        0, game_layout::GRAVE_SELECTION_PLACEHOLDER_Y,
        bn::sprite_shape_size(game_layout::GRAVE_SELECTION_PLACEHOLDER_SIZE,
                              game_layout::GRAVE_SELECTION_PLACEHOLDER_SIZE),
        _tiles, _palette))
{
    auto vram = _tiles.vram();
    auto* tile_span = vram.get();

    if(tile_span)
    {
        const uint32_t row = solid_tile_row(1);

        for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
        {
            bn::tile& tile = (*tile_span)[tile_index];

            for(int py = 0; py < 8; ++py)
            {
                tile.data[py] = row;
            }
        }
    }

    _sprite.set_z_order(-4);
    _sprite.set_visible(false);
}

void GraveyardPickPlaceholder::set_position(int x, int y)
{
    _sprite.set_position(x, y);
}

void GraveyardPickPlaceholder::set_visible(bool visible)
{
    _sprite.set_visible(visible);
}

const bn::sprite_item* CardEffectBadge::sprite_item_for(Kind kind)
{
    switch(kind)
    {
    case Kind::ECHO:
        return &bn::sprite_items::hud_trinket_echo;

    case Kind::SWIVEL:
        // Replace with hud_effect_swivel when the asset exists.
        return nullptr;

    default:
        return nullptr;
    }
}

CardEffectBadge::CardEffectBadge()
{
    clear();
}

void CardEffectBadge::clear()
{
    _kind = Kind::NONE;
    _sprite.reset();
    _owned_tiles.reset();
    _owned_palette.reset();
}

void CardEffectBadge::rebuild(Kind kind)
{
    clear();
    _kind = kind;

    if(kind == Kind::NONE)
    {
        return;
    }

    const bn::sprite_item* item = sprite_item_for(kind);

    if(item != nullptr)
    {
        _sprite = item->create_sprite(0, 0);
        _sprite->set_z_order(1);
        _sprite->set_visible(false);
        return;
    }

    _owned_tiles = bn::sprite_tiles_ptr::allocate(4, bn::bpp_mode::BPP_4);
    _owned_palette = bn::sprite_palette_ptr::create(SWIVEL_BADGE_PALETTE);
    _sprite = bn::sprite_ptr::create(0, 0, bn::sprite_shape_size(16, 16), *_owned_tiles, *_owned_palette);

    auto vram = _owned_tiles->vram();
    auto* tile_span = vram.get();

    if(tile_span)
    {
        for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
        {
            bn::tile& tile = (*tile_span)[tile_index];
            const int tile_col = tile_index % 2;
            const int tile_row = tile_index / 2;

            for(int py = 0; py < 8; ++py)
            {
                uint32_t row = 0;

                for(int px = 0; px < 8; ++px)
                {
                    const int x = tile_col * 8 + px;
                    const int y = tile_row * 8 + py;
                    const bool on = swivel_badge_pixel(x, y);
                    row |= uint32_t(on ? 1 : 0) << (px * 4);
                }

                tile.data[py] = row;
            }
        }
    }

    _sprite->set_z_order(1);
    _sprite->set_visible(false);
}

void CardEffectBadge::set_kind(Kind kind)
{
    if(kind == _kind && _sprite.has_value())
    {
        return;
    }

    rebuild(kind);
}

void CardEffectBadge::set_position_above_card(int card_x, int card_y)
{
    if(!_sprite.has_value())
    {
        return;
    }

    _sprite->set_position(card_x + game_layout::CARD_DISPLAY_WIDTH / 2,
                          card_y + game_layout::CARD_EFFECT_BADGE_Y);
}

void CardEffectBadge::set_visible(bool visible)
{
    if(_sprite.has_value())
    {
        _sprite->set_visible(visible && _kind != Kind::NONE);
    }
}

void apply_row_fade_bands(bn::array<GameFadeBand, 4>& bands, int card_y, bool has_left, bool has_right)
{
    for(GameFadeBand& band : bands)
    {
        band.set_position_y(card_y);
    }

    bands[0].set_visible(has_left);
    bands[1].set_visible(has_left);
    bands[2].set_visible(has_right);
    bands[3].set_visible(has_right);
}

void hide_fade_bands(bn::array<GameFadeBand, 4>& bands)
{
    for(GameFadeBand& band : bands)
    {
        band.set_visible(false);
    }
}

namespace
{
    int flight_eased_progress(int frame, int total_frames)
    {
        if(total_frames <= 1)
        {
            return 256;
        }

        int progress = frame * 256 / (total_frames - 1);

        if(progress > 256)
        {
            progress = 256;
        }

        if(progress < 128)
        {
            return progress * progress / 128;
        }

        const int inverse = 256 - progress;
        return 256 - inverse * inverse / 128;
    }

    bn::fixed lerp_fixed(bn::fixed from, bn::fixed to, int eased_progress)
    {
        return from + (to - from) * bn::fixed(eased_progress) / bn::fixed(256);
    }
}

int card_target_x_for_hud_icon(int hud_icon_x, int main_x)
{
    return hud_icon_x + main_x - game_layout::CARD_DISPLAY_WIDTH / 2;
}

int card_target_y_for_hud_icon(int hud_icon_y)
{
    return hud_icon_y - 32;
}

int card_target_x_for_score_center(int main_x)
{
    return main_x - game_layout::CARD_DISPLAY_WIDTH / 2;
}

int card_target_y_for_play_presentation()
{
    return game_layout::PLAY_PRESENTATION_CENTER_Y;
}

int card_target_y_for_score_center()
{
    const int hand_card_top = game_layout::HAND_Y - game_layout::CARD_BODY_WIDTH / 2;
    return (game_layout::SCREEN_TOP + hand_card_top) / 2;
}

CardFlightSample sample_card_flight(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames,
                                    bn::fixed from_rotation, bn::fixed to_rotation,
                                    bn::fixed from_scale, bn::fixed to_scale)
{
    const int eased = flight_eased_progress(frame, total_frames);
    CardFlightSample sample;
    sample.x = from_x + (to_x - from_x) * eased / 256;
    sample.y = from_y + (to_y - from_y) * eased / 256;
    sample.scale = lerp_fixed(from_scale, to_scale, eased);
    sample.rotation = lerp_fixed(from_rotation, to_rotation, eased);
    sample.alpha = 1;
    return sample;
}

CardFlightSample sample_card_exile_dissipate(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames)
{
    const int eased = flight_eased_progress(frame, total_frames);
    const bn::fixed min_scale = bn::fixed(game_layout::EXILE_MIN_SCALE) /
                                bn::fixed(game_layout::EXILE_MIN_SCALE_DIVISOR);
    CardFlightSample sample;
    sample.x = from_x + (to_x - from_x) * eased / 256;
    sample.y = from_y + (to_y - from_y) * eased / 256;
    sample.scale = lerp_fixed(1, min_scale, eased);
    sample.rotation = 0;
    sample.alpha = eased < 96 ? bn::fixed(1)
                              : bn::fixed(256 - (eased - 96) * 256 / 160) / bn::fixed(256);
    return sample;
}

CardFlightSample sample_card_to_deck(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames)
{
    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    CardFlightSample sample = sample_card_flight(
        from_x, from_y, to_x, to_y, frame, total_frames, 0, 0, 1, min_scale);
    const int eased = flight_eased_progress(frame, total_frames);
    sample.alpha = eased < 128 ? bn::fixed(1)
                               : bn::fixed(256 - (eased - 128) * 256 / 128) / bn::fixed(256);
    return sample;
}

CardFlightSample sample_deck_to_hand_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                              int total_frames)
{
    return sample_graveyard_to_hand_flight(from_x, from_y, to_x, to_y, frame, total_frames);
}

CardFlightSample sample_graveyard_to_hand_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                                 int total_frames)
{
    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    return sample_card_flight(from_x, from_y, to_x, to_y, frame, total_frames, 0, 0, min_scale, 1);
}

CardFlightSample sample_graveyard_to_deck_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                                 int total_frames)
{
    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    CardFlightSample sample;
    const int eased = flight_eased_progress(frame, total_frames);
    sample.x = from_x + (to_x - from_x) * eased / 256;
    sample.y = from_y + (to_y - from_y) * eased / 256;
    sample.alpha = 1;
    sample.rotation = 0;

    if(total_frames <= 1)
    {
        sample.scale = 1;
        return sample;
    }

    const int progress = frame * 100 / (total_frames - 1);

    if(progress < 40)
    {
        sample.scale = lerp_fixed(min_scale, 1, progress * 256 / 40);
    }
    else if(progress <= 60)
    {
        sample.scale = 1;
    }
    else
    {
        sample.scale = lerp_fixed(1, min_scale, (progress - 60) * 256 / 40);
    }

    return sample;
}

void ease_raise_toward(int& current, int target)
{
    if(current == target)
    {
        return;
    }

    const int delta = target - current;
    int step = delta / game_layout::HAND_RAISE_EASE_DIVISOR;

    if(step == 0)
    {
        step = delta > 0 ? 1 : -1;
    }

    current += step;

    if((delta > 0 && current >= target) || (delta < 0 && current <= target))
    {
        current = target;
    }
}

int swap_vertical_arc(int frame, int total_frames, int peak)
{
    if(total_frames <= 1 || peak <= 0)
    {
        return 0;
    }

    const int denominator = total_frames - 1;
    return peak * 4 * frame * (denominator - frame) / (denominator * denominator);
}

int hand_swap_wave_raise(int card_index, int selected_card, int swap_direction, bool swapping_card, int swap_frame)
{
    if(!swapping_card)
    {
        return 0;
    }

    const int partner = selected_card + swap_direction;
    const int arc = swap_vertical_arc(swap_frame, game_layout::SWAP_FRAMES, game_layout::SWAP_ARC_PEAK);

    if(card_index == selected_card || card_index == partner)
    {
        return arc;
    }

    return 0;
}

int row_scroll_pair_raise(int card_index, int cursor, int scroll_x, int target_scroll_x, int spacing, int count)
{
    if(scroll_x == target_scroll_x || spacing <= 0 || count <= 0)
    {
        return 0;
    }

    const int scroll_dir = target_scroll_x > scroll_x ? 1 : -1;
    const int partner = cursor - scroll_dir;

    if(card_index != cursor && card_index != partner)
    {
        return 0;
    }

    if(partner < 0 || partner >= count)
    {
        if(card_index != cursor)
        {
            return 0;
        }
    }

    const int abs_remaining = target_scroll_x - scroll_x;
    const int distance = abs_remaining < 0 ? -abs_remaining : abs_remaining;

    if(distance >= spacing)
    {
        return 0;
    }

    const int progress = spacing - distance;
    const int frame = progress * (game_layout::SWAP_FRAMES - 1) / spacing;
    return swap_vertical_arc(frame, game_layout::SWAP_FRAMES, game_layout::SWAP_ARC_PEAK);
}
