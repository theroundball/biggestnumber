#include "ui_common.h"

#include "bn_core.h"
#include "bn_keypad.h"

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
    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_z_order(z);
    }
}

void SceneText::set_bg_priority(int priority)
{
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
    _generator.set_center_alignment();
    _generator.generate(x, y, text, _sprites);
    _generator.set_left_alignment();
}

void SceneText::draw_left_line(int x, int y, const bn::string_view& text)
{
    _generator.generate(x, y, text, _sprites);
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
