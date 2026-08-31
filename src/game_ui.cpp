#include "game_ui.h"

#include "bn_color.h"
#include "bn_compression_type.h"
#include "bn_optional.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_tile.h"
#include "game_types.h"

namespace ui_palette
{
    constexpr bn::array<bn::color, 16> COLORS = {
        bn::color(0, 0, 0), bn::color(31, 0, 0), bn::color(6, 28, 12), bn::color(20, 20, 24),
        bn::color(22, 10, 28), bn::color(18, 8, 24), bn::color(0, 16, 31), bn::color(12, 12, 16),
        bn::color(20, 20, 24), bn::color(26, 18, 10), bn::color(28, 10, 22), bn::color(24, 18, 4),
        bn::color(10, 22, 14), bn::color(8, 20, 10), bn::color(0, 0, 0), bn::color(31, 25, 5),
    };

    const bn::sprite_palette_ptr& shared()
    {
        static bn::optional<bn::sprite_palette_ptr> palette;

        if(!palette.has_value())
        {
            const bn::sprite_palette_item item(
                bn::span<const bn::color>(COLORS.data(), COLORS.size()), bn::bpp_mode::BPP_4,
                bn::compression_type::NONE);
            palette = bn::sprite_palette_ptr::create(item);
        }

        return *palette;
    }
}

namespace
{
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

    void paint_score_bar_tile(bn::tile& tile, int color_index)
    {
        for(int row_index = 0; row_index < 8; ++row_index)
        {
            tile.data[row_index] =
                row_index == 3 || row_index == 4 ? solid_tile_row(color_index) : 0;
        }
    }

    int score_bar_px(int score, int goal)
    {
        if(goal <= 0 || score <= 0)
        {
            return 0;
        }

        const int width = game_layout::SCORE_BAR_WIDTH;
        const int px = int((int64_t(score) * width) / goal);

        return px > width ? width : px;
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

    bool echo_badge_pixel(int x, int y)
    {
        const int center_x = 7;
        const int center_y = 7;
        const int dx = x - center_x;
        const int dy = y - center_y;
        return dx * dx + dy * dy <= 36;
    }
}

GameFadeBand::GameFadeBand(int x, int color_index, int card_y) :
    _tiles(bn::sprite_tiles_ptr::allocate(4, bn::bpp_mode::BPP_4)),
    _palette(ui_palette::shared()),
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
    _palette(ui_palette::shared()),
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
                const int color_index =
                    on ? (_style == Style::LOCK ? ui_palette::MARKER_LOCK : ui_palette::MARKER_X)
                       : ui_palette::TRANSPARENT;
                row |= uint32_t(color_index) << (px * 4);
            }

            tile.data[py] = row;
        }
    }
}

GraveyardPickPlaceholder::GraveyardPickPlaceholder() :
    _tiles(bn::sprite_tiles_ptr::allocate(PLACEHOLDER_TILE_COUNT, bn::bpp_mode::BPP_4)),
    _palette(ui_palette::shared()),
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
        const uint32_t row = solid_tile_row(ui_palette::PLACEHOLDER);

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

ScoreProgressBar::ScoreProgressBar() :
    _track_tile(bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4)),
    _gold_tile(bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4)),
    _round_tile(bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4)),
    _palette(ui_palette::shared())
{
    auto paint_tile = [](bn::sprite_tiles_ptr& tiles, int color_index) {
        auto vram = tiles.vram();
        auto* tile_span = vram.get();

        if(tile_span && !tile_span->empty())
        {
            paint_score_bar_tile((*tile_span)[0], color_index);
        }
    };

    paint_tile(_track_tile, ui_palette::PLACEHOLDER);
    paint_tile(_gold_tile, ui_palette::SCORE_BAR_GOLD);
    paint_tile(_round_tile, ui_palette::GRAVEYARD);

    for(int segment_index = 0; segment_index < game_layout::SCORE_BAR_SEGMENT_COUNT; ++segment_index)
    {
        bn::sprite_ptr track_sprite = bn::sprite_ptr::create(
            segment_center_x(segment_index), game_layout::SCORE_BAR_Y, bn::sprite_shape_size(8, 8),
            _track_tile, _palette);
        track_sprite.set_z_order(game_layout::PLAY_PRESENTATION_SCORE_Z - 1);
        track_sprite.set_visible(false);
        _track_sprites.push_back(track_sprite);
    }
}

int ScoreProgressBar::segment_center_x(int segment_index) const
{
    return -game_layout::SCORE_BAR_WIDTH / 2 + game_layout::SCORE_BAR_SEGMENT / 2 +
           segment_index * game_layout::SCORE_BAR_SEGMENT + _x_offset;
}

void ScoreProgressBar::reposition_segments()
{
    for(int segment_index = 0; segment_index < _track_sprites.size(); ++segment_index)
    {
        _track_sprites[segment_index].set_x(segment_center_x(segment_index));
    }

    for(int segment_index = 0; segment_index < _gold_sprites.size(); ++segment_index)
    {
        _gold_sprites[segment_index].set_x(segment_center_x(segment_index));
    }

    const int round_offset = _gold_sprites.size();

    for(int segment_index = 0; segment_index < _round_sprites.size(); ++segment_index)
    {
        _round_sprites[segment_index].set_x(segment_center_x(round_offset + segment_index));
    }
}

void ScoreProgressBar::rebuild_fill_sprites(
    bn::vector<bn::sprite_ptr, game_layout::SCORE_BAR_SEGMENT_COUNT>& sprites, int segment_count,
    int tile_index, int segment_offset)
{
    while(sprites.size() > segment_count)
    {
        sprites.pop_back();
    }

    const bn::sprite_tiles_ptr& tile = tile_index == 1 ? _gold_tile : _round_tile;

    while(sprites.size() < segment_count)
    {
        const int segment_index = segment_offset + sprites.size();
        bn::sprite_ptr fill_sprite = bn::sprite_ptr::create(
            segment_center_x(segment_index), game_layout::SCORE_BAR_Y, bn::sprite_shape_size(8, 8),
            tile, _palette);
        fill_sprite.set_z_order(game_layout::PLAY_PRESENTATION_SCORE_Z - 1);
        fill_sprite.set_visible(_visible);
        sprites.push_back(fill_sprite);
    }
}

void ScoreProgressBar::sync(int total, int round_committed, int goal)
{
    if(goal <= 0)
    {
        _last_goal = 0;
        _last_total_px = -1;
        _last_combined_px = -1;
        return;
    }

    const int total_px = score_bar_px(total, goal);
    const int combined_px = score_bar_px(total + round_committed, goal);

    if(total_px == _last_total_px && combined_px == _last_combined_px && goal == _last_goal)
    {
        return;
    }

    _last_total_px = total_px;
    _last_combined_px = combined_px;
    _last_goal = goal;

    const int gold_segments =
        (total_px + game_layout::SCORE_BAR_SEGMENT - 1) / game_layout::SCORE_BAR_SEGMENT;
    const int combined_segments =
        (combined_px + game_layout::SCORE_BAR_SEGMENT - 1) / game_layout::SCORE_BAR_SEGMENT;
    const int round_segments = combined_segments - gold_segments;

    rebuild_fill_sprites(_gold_sprites, gold_segments, 1, 0);
    rebuild_fill_sprites(_round_sprites, round_segments, 2, gold_segments);
    reposition_segments();
}

void ScoreProgressBar::set_visible(bool visible)
{
    if(_visible == visible)
    {
        return;
    }

    _visible = visible;

    for(bn::sprite_ptr& sprite : _track_sprites)
    {
        sprite.set_visible(visible);
    }

    for(bn::sprite_ptr& sprite : _gold_sprites)
    {
        sprite.set_visible(visible);
    }

    for(bn::sprite_ptr& sprite : _round_sprites)
    {
        sprite.set_visible(visible);
    }
}

void ScoreProgressBar::set_x_offset(int panel_offset)
{
    if(_x_offset == panel_offset)
    {
        return;
    }

    _x_offset = panel_offset;
    reposition_segments();
}

ComboProgressBars::ComboProgressBars() :
    _track_tile(bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4)),
    _fill_tile(bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4)),
    _palette(ui_palette::shared())
{
    auto paint_tile = [](bn::sprite_tiles_ptr& tiles, int color_index) {
        auto vram = tiles.vram();
        auto* tile_span = vram.get();

        if(tile_span && !tile_span->empty())
        {
            paint_score_bar_tile((*tile_span)[0], color_index);
        }
    };

    paint_tile(_track_tile, ui_palette::PLACEHOLDER);
    paint_tile(_fill_tile, ui_palette::SCORE_BAR_GOLD);
}

int ComboProgressBars::segment_center_x(int segment_index, int segment_count) const
{
    const int width = segment_count * game_layout::COMBO_BAR_SEGMENT;
    return game_layout::COMBO_BAR_X - width / 2 + game_layout::COMBO_BAR_SEGMENT / 2 +
           segment_index * game_layout::COMBO_BAR_SEGMENT;
}

int ComboProgressBars::row_center_y(int row_index) const
{
    return game_layout::COMBO_BAR_Y0 + row_index * game_layout::COMBO_BAR_ROW_STEP;
}

void ComboProgressBars::sync_row(int row_index, int segment_count, int filled_count)
{
    if(segment_count < 0)
    {
        segment_count = 0;
    }

    if(segment_count > game_layout::COMBO_BAR_MAX_SEGMENTS)
    {
        segment_count = game_layout::COMBO_BAR_MAX_SEGMENTS;
    }

    if(filled_count < 0)
    {
        filled_count = 0;
    }

    if(filled_count > segment_count)
    {
        filled_count = segment_count;
    }

    Row& row = _rows[row_index];

    if(segment_count == row.last_length && filled_count == row.last_filled)
    {
        return;
    }

    row.last_length = segment_count;
    row.last_filled = filled_count;

    while(row.track_sprites.size() > segment_count)
    {
        row.track_sprites.pop_back();
    }

    while(row.track_sprites.size() < segment_count)
    {
        const int segment_index = row.track_sprites.size();
        bn::sprite_ptr track_sprite = bn::sprite_ptr::create(
            segment_center_x(segment_index, segment_count), row_center_y(row_index),
            bn::sprite_shape_size(8, 8), _track_tile, _palette);
        track_sprite.set_z_order(game_layout::PLAY_PRESENTATION_SCORE_Z - 1);
        track_sprite.set_visible(_visible);
        row.track_sprites.push_back(track_sprite);
    }

    for(int segment_index = 0; segment_index < row.track_sprites.size(); ++segment_index)
    {
        row.track_sprites[segment_index].set_x(segment_center_x(segment_index, segment_count));
        row.track_sprites[segment_index].set_y(row_center_y(row_index));
        row.track_sprites[segment_index].set_visible(_visible);
    }

    while(row.fill_sprites.size() > filled_count)
    {
        row.fill_sprites.pop_back();
    }

    while(row.fill_sprites.size() < filled_count)
    {
        const int segment_index = row.fill_sprites.size();
        bn::sprite_ptr fill_sprite = bn::sprite_ptr::create(
            segment_center_x(segment_index, segment_count), row_center_y(row_index),
            bn::sprite_shape_size(8, 8), _fill_tile, _palette);
        fill_sprite.set_z_order(game_layout::PLAY_PRESENTATION_SCORE_Z - 1);
        fill_sprite.set_visible(_visible);
        row.fill_sprites.push_back(fill_sprite);
    }

    for(int segment_index = 0; segment_index < row.fill_sprites.size(); ++segment_index)
    {
        row.fill_sprites[segment_index].set_x(segment_center_x(segment_index, segment_count));
        row.fill_sprites[segment_index].set_y(row_center_y(row_index));
        row.fill_sprites[segment_index].set_visible(_visible);
    }
}

void ComboProgressBars::sync(const bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT>& lengths,
                             const bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT>& filled)
{
    for(int row_index = 0; row_index < game_layout::COMBO_BAR_ROW_COUNT; ++row_index)
    {
        sync_row(row_index, lengths[row_index], filled[row_index]);
    }
}

void ComboProgressBars::set_visible(bool visible)
{
    if(_visible == visible)
    {
        return;
    }

    _visible = visible;

    for(Row& row : _rows)
    {
        for(bn::sprite_ptr& sprite : row.track_sprites)
        {
            sprite.set_visible(visible);
        }

        for(bn::sprite_ptr& sprite : row.fill_sprites)
        {
            sprite.set_visible(visible);
        }
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
}

void CardEffectBadge::paint(Kind kind)
{
    if(!_owned_tiles.has_value() || !_sprite.has_value())
    {
        return;
    }

    auto vram = _owned_tiles->vram();
    auto* tile_span = vram.get();

    if(!tile_span)
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
                bool on = false;

                if(kind == Kind::SWIVEL)
                {
                    on = swivel_badge_pixel(x, y);
                }
                else if(kind == Kind::ECHO)
                {
                    on = echo_badge_pixel(x, y);
                }

                const int color_index = on ? (kind == Kind::ECHO ? ui_palette::ECHO_BADGE : ui_palette::SWIVEL_BADGE)
                                           : ui_palette::TRANSPARENT;
                row |= uint32_t(color_index) << (px * 4);
            }

            tile.data[py] = row;
        }
    }
}

void CardEffectBadge::rebuild(Kind kind)
{
    clear();
    _kind = kind;

    if(kind == Kind::NONE)
    {
        return;
    }

    _owned_tiles = bn::sprite_tiles_ptr::allocate(4, bn::bpp_mode::BPP_4);
    _sprite = bn::sprite_ptr::create(
        0, 0, bn::sprite_shape_size(16, 16), *_owned_tiles, ui_palette::shared());
    paint(kind);
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
    CardFlightSample sample;
    sample.rotation = 0;
    sample.alpha = 1;

    if(total_frames <= 1)
    {
        sample.x = to_x;
        sample.y = to_y;
        sample.scale = 1;
        return sample;
    }

    int linear = frame * 256 / (total_frames - 1);

    if(linear > 256)
    {
        linear = 256;
    }

    // Ease-out along the path: quick off the deck, direct into the slot.
    const int inv = 256 - linear;
    const int t = 256 - inv * inv / 256;

    // Bow down and to the right from the deck icon toward the hand row.
    const int ctrl_x = from_x + (to_x - from_x) / 3 + 10;
    const int ctrl_y = from_y + game_layout::DECK_DRAW_ARC_DROP;
    const int u = 256 - t;

    sample.x = (u * u / 256 * from_x + 2 * u * t / 256 * ctrl_x + t * t / 256 * to_x) / 256;
    sample.y = (u * u / 256 * from_y + 2 * u * t / 256 * ctrl_y + t * t / 256 * to_y) / 256;

    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    const int scale_progress = t < 128 ? t * 256 / 128 : 256;
    sample.scale = min_scale + (bn::fixed(1) - min_scale) * bn::fixed(scale_progress) / bn::fixed(256);
    return sample;
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

CardFlightSample sample_hud_via_center_flight(int from_x, int from_y, int center_x, int center_y,
                                              int to_x, int to_y, int frame, int total_frames)
{
    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    CardFlightSample sample;

    if(total_frames <= 1)
    {
        sample.x = to_x;
        sample.y = to_y;
        sample.scale = min_scale;
        sample.alpha = 1;
        sample.rotation = 0;
        return sample;
    }

    const int progress = frame * 100 / (total_frames - 1);

    if(progress < 40)
    {
        const int segment_progress = progress * 256 / 40;
        sample.x = from_x + (center_x - from_x) * segment_progress / 256;
        sample.y = from_y + (center_y - from_y) * segment_progress / 256;
        sample.scale = lerp_fixed(min_scale, 1, segment_progress);
    }
    else if(progress <= 60)
    {
        sample.x = center_x;
        sample.y = center_y;
        sample.scale = 1;
    }
    else
    {
        const int segment_progress = (progress - 60) * 256 / 40;
        sample.x = center_x + (to_x - center_x) * segment_progress / 256;
        sample.y = center_y + (to_y - center_y) * segment_progress / 256;
        sample.scale = lerp_fixed(1, min_scale, segment_progress);
    }

    sample.alpha = 1;
    sample.rotation = 0;
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
