#include "ui_common.h"

#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_compression_type.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_optional.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_tile.h"

#include "battle_backdrop.h"

void move_toward_int(int& value, int target, int max_step)
{
    const int distance = target - value;

    if(distance > 0)
    {
        const int step = distance / 4 > 0 ? distance / 4 : 1;
        value += step < max_step ? step : max_step;
    }
    else if(distance < 0)
    {
        const int step = -distance / 4 > 0 ? -distance / 4 : 1;
        value -= step < max_step ? step : max_step;
    }
}

void move_toward_raise(int& value, int target)
{
    move_toward_int(value, target, 2);
}

int first_visible_index(int cursor, int count, int window)
{
    const int radius = window / 2;
    const int max_start = count > window ? count - window : 0;

    if(count == 0 || cursor < radius)
    {
        return 0;
    }

    if(cursor - radius < max_start)
    {
        return cursor - radius;
    }

    return max_start;
}

void wait_for_keypad_clear()
{
    while(bn::keypad::any_held())
    {
        battle_backdrop_tick();
        bn::core::update();
    }
}

namespace
{
    int direction_repeat_interval(int held_total_frames)
    {
        int interval = game_layout::DIRECTION_REPEAT_INTERVAL;

        if(held_total_frames > 75)
        {
            interval = 1;
        }
        else if(held_total_frames > 60)
        {
            interval = 1;
        }
        else if(held_total_frames > 45)
        {
            interval = 2;
        }
        else if(held_total_frames > 22)
        {
            interval = 3;
        }

        return interval < game_layout::DIRECTION_REPEAT_INTERVAL_MIN
                   ? game_layout::DIRECTION_REPEAT_INTERVAL_MIN
                   : interval;
    }

    int direction_accel_steps(int held_total_frames)
    {
        if(held_total_frames > 75)
        {
            return game_layout::DIRECTION_ACCEL_STEPS_MAX;
        }

        if(held_total_frames > 45)
        {
            return 4;
        }

        if(held_total_frames > 30)
        {
            return 3;
        }

        if(held_total_frames > 15)
        {
            return 2;
        }

        return 1;
    }

    constexpr int TEXT_BOX_FILL_COLOR = 1;
    constexpr int TEXT_BOX_BORDER_COLOR = 2;
    constexpr bn::color TEXT_BOX_FILL(26, 14, 6);
    constexpr bn::color TEXT_BOX_BORDER(18, 8, 4);

    struct TextBoxAssets
    {
        bn::optional<bn::sprite_tiles_ptr> fill_tiles;
        bn::optional<bn::sprite_tiles_ptr> border_tiles;
        bn::optional<bn::sprite_palette_ptr> palette;

        void ensure()
        {
            if(fill_tiles.has_value())
            {
                return;
            }

            bn::array<bn::color, 16> colors;
            colors[0] = bn::color(0, 0, 0);
            colors[TEXT_BOX_FILL_COLOR] = TEXT_BOX_FILL;
            colors[TEXT_BOX_BORDER_COLOR] = TEXT_BOX_BORDER;

            const bn::sprite_palette_item palette_item(
                bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4,
                bn::compression_type::NONE);
            palette = bn::sprite_palette_ptr::create(palette_item);

            fill_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);
            border_tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);

            auto paint_solid = [](bn::sprite_tiles_ptr& tiles, int color_index)
            {
                auto vram = tiles.vram();
                auto* tile_span = vram.get();

                if(!tile_span)
                {
                    return;
                }

                uint32_t row = 0;

                for(int px = 0; px < 8; ++px)
                {
                    row |= uint32_t(color_index) << (px * 4);
                }

                for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
                {
                    bn::tile& tile = (*tile_span)[tile_index];

                    for(int row_index = 0; row_index < 8; ++row_index)
                    {
                        tile.data[row_index] = row;
                    }
                }
            };

            paint_solid(*fill_tiles, TEXT_BOX_FILL_COLOR);
            paint_solid(*border_tiles, TEXT_BOX_BORDER_COLOR);
        }
    };

    TextBoxAssets& text_box_assets()
    {
        static TextBoxAssets assets;
        return assets;
    }

    int align_down_8(int value)
    {
        return value >= 0 ? (value / 8) * 8 : -(((-value) + 7) / 8) * 8;
    }

    int align_up_8(int value)
    {
        return value >= 0 ? ((value + 7) / 8) * 8 : (value / 8) * 8;
    }
}

bool poll_direction_repeat(DirectionAxis axis, DirectionRepeatState& state, bool scrolling,
                           int& out_direction, bool& out_triggered, int& out_steps)
{
    if(axis == DirectionAxis::HORIZONTAL)
    {
        out_direction = bn::keypad::right_held() ? 1 : bn::keypad::left_held() ? -1 : 0;
    }
    else
    {
        out_direction = bn::keypad::down_held() ? 1 : bn::keypad::up_held() ? -1 : 0;
    }

    out_triggered = false;
    out_steps = 1;

    if(out_direction != state.held_direction)
    {
        state.held_direction = out_direction;
        state.held_direction_frames = 0;
        state.held_total_frames = 0;
        state.direction_initial_delay = true;
        out_triggered = out_direction != 0;
        return out_triggered;
    }

    if(!out_direction)
    {
        return false;
    }

    ++state.held_total_frames;

    if(scrolling)
    {
        return false;
    }

    ++state.held_direction_frames;

    const int repeat_delay = state.direction_initial_delay
                                 ? game_layout::DIRECTION_INITIAL_DELAY
                                 : direction_repeat_interval(state.held_total_frames);

    if(state.held_direction_frames >= repeat_delay)
    {
        state.held_direction_frames = 0;
        state.direction_initial_delay = false;
        out_triggered = true;
        out_steps = direction_accel_steps(state.held_total_frames);
    }

    return out_triggered;
}

SceneText::SceneText(bn::sprite_text_generator& generator) :
    _generator(generator)
{
}

void SceneText::apply_depth_to_range(int first_index)
{
    for(int index = first_index; index < _sprites.size(); ++index)
    {
        if(_z_order.has_value())
        {
            _sprites[index].set_z_order(_z_order.value());
        }

        if(_bg_priority.has_value())
        {
            _sprites[index].set_bg_priority(_bg_priority.value());
        }
    }
}

void SceneText::clear()
{
    _sprites.clear();
}

void SceneText::set_visible(bool visible)
{
    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_visible(visible);
    }
}

void SceneText::set_z_order(int z)
{
    _z_order = z;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_z_order(z);
    }
}

void SceneText::set_bg_priority(int priority)
{
    _bg_priority = priority;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_bg_priority(priority);
    }
}

void SceneText::draw_centered_line(int y, const bn::string_view& text)
{
    draw_centered_line(0, y, text);
}

void SceneText::draw_centered_line(int x, int y, const bn::string_view& text)
{
    const int first_index = _sprites.size();
    _generator.set_center_alignment();
    _generator.generate(x, y, text, _sprites);
    _generator.set_left_alignment();
    apply_depth_to_range(first_index);
}

void SceneText::draw_left_line(int x, int y, const bn::string_view& text)
{
    const int first_index = _sprites.size();
    _generator.generate(x, y, text, _sprites);
    apply_depth_to_range(first_index);
}

void TextBoxPanel::clear()
{
    _sprites.clear();
}

void TextBoxPanel::set_z_order(int z)
{
    _z_order = z;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_z_order(z);
    }
}

void TextBoxPanel::set_bg_priority(int priority)
{
    _bg_priority = priority;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_bg_priority(priority);
    }
}

void TextBoxPanel::draw_around_lines(int center_x, int top_y, int bottom_y, int content_half_width,
                                     int padding_x, int padding_y)
{
    clear();

    TextBoxAssets& assets = text_box_assets();
    assets.ensure();

    if(!assets.palette.has_value() || !assets.fill_tiles.has_value() || !assets.border_tiles.has_value())
    {
        return;
    }

    constexpr int line_half_height = 8;
    const int content_left = center_x - content_half_width;
    const int content_right = center_x + content_half_width;
    const int content_top = top_y - line_half_height;
    const int content_bottom = bottom_y + line_half_height;

    const int box_left = align_down_8(content_left - padding_x);
    const int box_right = align_up_8(content_right + padding_x);
    const int box_top = align_down_8(content_top - padding_y);
    const int box_bottom = align_up_8(content_bottom + padding_y);

    const int cols = (box_right - box_left) / 8;
    const int rows = (box_bottom - box_top) / 8;

    if(cols <= 0 || rows <= 0)
    {
        return;
    }

    const bn::sprite_palette_ptr& palette = *assets.palette;
    const bn::sprite_tiles_ptr& fill_tiles = *assets.fill_tiles;
    const bn::sprite_tiles_ptr& border_tiles = *assets.border_tiles;
    const bn::sprite_shape_size shape(8, 8);

    for(int row = 0; row < rows; ++row)
    {
        for(int col = 0; col < cols; ++col)
        {
            const bool edge = row == 0 || row == rows - 1 || col == 0 || col == cols - 1;
            const int x = box_left + col * 8 + 4;
            const int y = box_top + row * 8 + 4;
            bn::sprite_ptr sprite =
                bn::sprite_ptr::create(x, y, shape, edge ? border_tiles : fill_tiles, palette);
            sprite.set_z_order(_z_order.has_value() ? _z_order.value() : game_layout::TEXT_BOX_Z);
            sprite.set_bg_priority(_bg_priority.has_value() ? _bg_priority.value()
                                                            : game_layout::TEXT_BOX_BG_PRIORITY);
            _sprites.push_back(sprite);
        }
    }
}

SelectorGlyph::SelectorGlyph(bn::sprite_text_generator& generator, int anchor_x) :
    _anchor_x(anchor_x)
{
    generator.generate(_anchor_x, 0, ">", _sprites);

    for(bn::sprite_ptr& sprite : _sprites)
    {
        // Draw above menu text / icons.
        sprite.set_z_order(-10);
    }

    set_visible(false);
}

void SelectorGlyph::set_position(int y)
{
    set_position(_anchor_x, y);
}

void SelectorGlyph::set_position(int x, int y)
{
    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_position(x, y);
    }
}

void SelectorGlyph::set_visible(bool visible)
{
    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_visible(visible);
    }
}
