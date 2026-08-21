#include "score_swap_system.h"

#include "bn_keypad.h"
#include "bn_string.h"
#include "bn_utility.h"

#include "game_context.h"
#include "game_ui.h"
#include "score_count_system.h"
#include "trinket_system.h"

namespace
{
    int round_running_digit_sprite_index(int end_multiplier, int digit_index)
    {
        if(end_multiplier == 1)
        {
            return digit_index;
        }

        return bn::to_string<12>(end_multiplier).size() + 1 + digit_index;
    }

    int swap_eased_shift(int spacing, int frame)
    {
        if(game_layout::SWAP_FRAMES <= 1)
        {
            return spacing;
        }

        const int progress = frame * game_layout::SWAP_EASE_SCALE / (game_layout::SWAP_FRAMES - 1);
        const int eased = progress < game_layout::SWAP_EASE_SCALE / 2
                              ? (2 * progress * progress / game_layout::SWAP_EASE_SCALE)
                              : (game_layout::SWAP_EASE_SCALE -
                                 2 * (game_layout::SWAP_EASE_SCALE - progress) *
                                     (game_layout::SWAP_EASE_SCALE - progress) /
                                     game_layout::SWAP_EASE_SCALE);

        return spacing * eased / game_layout::SWAP_EASE_SCALE;
    }

    bool slot_is_selected(const ScoreSwapFxState& fx, int slot_index)
    {
        for(int slot = 0; slot < fx.selected_count; ++slot)
        {
            if(fx.selected_slots[slot] == slot_index)
            {
                return true;
            }
        }

        return false;
    }

    int slot_for_field_digit(const ScoreSwapFxState& fx, SwapScoreField field, int digit_index)
    {
        int best_slot = -1;
        int best_distance = 999;

        for(int index = 0; index < fx.slot_count; ++index)
        {
            if(fx.slots[index].field != field)
            {
                continue;
            }

            const int distance = bn::abs(fx.slots[index].digit_index - digit_index);

            if(best_slot < 0 || distance < best_distance)
            {
                best_slot = index;
                best_distance = distance;
            }
        }

        return best_slot >= 0 ? best_slot : fx.cursor_slot;
    }

    int digit_raise_target(const ScoreSwapFxState& fx, int slot_index)
    {
        if(fx.swapping || slot_index < 0 || slot_index >= fx.slot_count)
        {
            return 0;
        }

        if(slot_is_selected(fx, slot_index) || slot_index == fx.cursor_slot)
        {
            return game_layout::SELECTED_RAISE;
        }

        return 0;
    }

    int parse_score_digits(const bn::string_view& digits)
    {
        int value = 0;

        for(int index = 0; index < digits.size(); ++index)
        {
            value = value * 10 + (digits[index] - '0');
        }

        return value;
    }

    bn::sprite_ptr* sprite_for_slot(GameContext& ctx, const ScoreSwapDigitSlot& slot)
    {
        if(slot.field == SwapScoreField::ROUND)
        {
            if(slot.sprite_index < 0 || slot.sprite_index >= ctx.round_text_sprites.size())
            {
                return nullptr;
            }

            return &ctx.round_text_sprites[slot.sprite_index];
        }

        if(slot.sprite_index < 0 || slot.sprite_index >= ctx.text_sprites.size())
        {
            return nullptr;
        }

        return &ctx.text_sprites[slot.sprite_index];
    }

    int field_digit_min(const ScoreSwapFxState& fx, SwapScoreField field)
    {
        int min_index = -1;

        for(int index = 0; index < fx.slot_count; ++index)
        {
            if(fx.slots[index].field != field)
            {
                continue;
            }

            if(min_index < 0 || fx.slots[index].digit_index < min_index)
            {
                min_index = fx.slots[index].digit_index;
            }
        }

        return min_index;
    }

    int field_digit_max(const ScoreSwapFxState& fx, SwapScoreField field)
    {
        int max_index = -1;

        for(int index = 0; index < fx.slot_count; ++index)
        {
            if(fx.slots[index].field != field)
            {
                continue;
            }

            if(max_index < 0 || fx.slots[index].digit_index > max_index)
            {
                max_index = fx.slots[index].digit_index;
            }
        }

        return max_index;
    }

    void move_cursor_in_field(ScoreSwapFxState& fx, int delta)
    {
        const SwapScoreField field = fx.slots[fx.cursor_slot].field;
        int digit_index = fx.slots[fx.cursor_slot].digit_index;
        const int min_digit = field_digit_min(fx, field);
        const int max_digit = field_digit_max(fx, field);

        if(min_digit < 0 || max_digit < 0)
        {
            return;
        }

        digit_index += delta;

        if(digit_index < min_digit)
        {
            digit_index = min_digit;
        }
        else if(digit_index > max_digit)
        {
            digit_index = max_digit;
        }

        fx.cursor_slot = slot_for_field_digit(fx, field, digit_index);
    }

    bool compute_swapped_scores(const GameContext& ctx, int slot_a, int slot_b, int& out_round, int& out_total)
    {
        if(slot_a < 0 || slot_b < 0 || slot_a >= ctx.score_swap_fx.slot_count ||
           slot_b >= ctx.score_swap_fx.slot_count)
        {
            return false;
        }

        const ScoreSwapDigitSlot& first = ctx.score_swap_fx.slots[slot_a];
        const ScoreSwapDigitSlot& second = ctx.score_swap_fx.slots[slot_b];
        bn::string<12> round_digits = bn::to_string<12>(ctx.state.round.running);
        bn::string<12> total_digits = bn::to_string<12>(ctx.state.total_score);

        if(first.field == SwapScoreField::ROUND)
        {
            if(first.digit_index < 0 || first.digit_index >= round_digits.size())
            {
                return false;
            }
        }
        else if(first.digit_index < 0 || first.digit_index >= total_digits.size())
        {
            return false;
        }

        if(second.field == SwapScoreField::ROUND)
        {
            if(second.digit_index < 0 || second.digit_index >= round_digits.size())
            {
                return false;
            }
        }
        else if(second.digit_index < 0 || second.digit_index >= total_digits.size())
        {
            return false;
        }

        char temp;

        if(first.field == SwapScoreField::ROUND)
        {
            temp = round_digits[first.digit_index];
            round_digits[first.digit_index] = second.field == SwapScoreField::ROUND
                                                  ? round_digits[second.digit_index]
                                                  : total_digits[second.digit_index];
        }
        else
        {
            temp = total_digits[first.digit_index];
            total_digits[first.digit_index] = second.field == SwapScoreField::ROUND
                                                  ? round_digits[second.digit_index]
                                                  : total_digits[second.digit_index];
        }

        if(second.field == SwapScoreField::ROUND)
        {
            round_digits[second.digit_index] = temp;
        }
        else
        {
            total_digits[second.digit_index] = temp;
        }

        out_round = parse_score_digits(round_digits);
        out_total = parse_score_digits(total_digits);
        return true;
    }

    bool swap_keeps_total_score(const GameContext& ctx, int slot_a, int slot_b)
    {
        int new_round = 0;
        int new_total = 0;

        if(!compute_swapped_scores(ctx, slot_a, slot_b, new_round, new_total))
        {
            return false;
        }

        (void)new_round;
        return new_total >= ctx.state.total_score;
    }

    bool apply_digit_swap(GameContext& ctx, int slot_a, int slot_b)
    {
        int new_round = 0;
        int new_total = 0;

        if(!compute_swapped_scores(ctx, slot_a, slot_b, new_round, new_total))
        {
            return false;
        }

        const int total_before = ctx.state.total_score;

        if(new_total < total_before)
        {
            return false;
        }

        const int round_before = ctx.state.round.running;
        ctx.state.round.running = new_round;
        ctx.state.total_score = new_total;
        score_count_queue(ctx.state, TrinketScoreField::ROUND, round_before, new_round);
        score_count_queue(ctx.state, TrinketScoreField::TOTAL, total_before, new_total);
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, round_before, new_round);
        trinket_queue_score_check(ctx.state, TrinketScoreField::TOTAL, total_before, new_total);
        return true;
    }

    void cache_digit_layout(GameContext& ctx, ScoreSwapFxState& fx)
    {
        fx.slot_count = 0;

        const bn::string<12> round_digits = bn::to_string<12>(ctx.state.round.running);
        const bn::string<12> total_digits = bn::to_string<12>(ctx.state.total_score);

        for(int index = 0; index < round_digits.size() && fx.slot_count < fx.slots.size(); ++index)
        {
            const int sprite_index =
                round_running_digit_sprite_index(ctx.state.round.end_multiplier, index);

            if(sprite_index < 0 || sprite_index >= ctx.round_text_sprites.size())
            {
                continue;
            }

            ScoreSwapDigitSlot& slot = fx.slots[fx.slot_count];
            slot.field = SwapScoreField::ROUND;
            slot.digit_index = index;
            slot.sprite_index = sprite_index;
            ++fx.slot_count;
        }

        for(int index = 0; index < total_digits.size() && fx.slot_count < fx.slots.size(); ++index)
        {
            if(index >= ctx.text_sprites.size())
            {
                continue;
            }

            ScoreSwapDigitSlot& slot = fx.slots[fx.slot_count];
            slot.field = SwapScoreField::TOTAL;
            slot.digit_index = index;
            slot.sprite_index = index;
            ++fx.slot_count;
        }

        for(int index = 0; index < fx.slot_count; ++index)
        {
            bn::sprite_ptr* sprite = sprite_for_slot(ctx, fx.slots[index]);

            if(!sprite)
            {
                continue;
            }

            fx.base_sprite_x[index] = sprite->x().integer();
            fx.base_sprite_y[index] = sprite->y().integer();
            fx.digit_raise[index] = 0;
        }
    }

    void finish_score_swap(GameContext& ctx)
    {
        ctx.score_swap_fx = ScoreSwapFxState{};
        ctx.draw_round_score();
        ctx.draw_total_score();
        ctx.begin_next_pending_or_finish();
    }

    void begin_digit_swap_animation(ScoreSwapFxState& fx)
    {
        fx.swap_slot_a = fx.selected_slots[0];
        fx.swap_slot_b = fx.selected_slots[1];

        if(fx.swap_slot_a > fx.swap_slot_b)
        {
            bn::swap(fx.swap_slot_a, fx.swap_slot_b);
        }

        fx.swapping = true;
        fx.swap_frame = 0;
        fx.selected_count = 0;
        fx.selected_slots[0] = -1;
        fx.selected_slots[1] = -1;
    }

    void deselect_slot(ScoreSwapFxState& fx, int slot_index)
    {
        for(int slot = 0; slot < fx.selected_count; ++slot)
        {
            if(fx.selected_slots[slot] == slot_index)
            {
                for(int shift = slot; shift < fx.selected_count - 1; ++shift)
                {
                    fx.selected_slots[shift] = fx.selected_slots[shift + 1];
                }

                --fx.selected_count;
                fx.selected_slots[fx.selected_count] = -1;
                fx.digit_raise[slot_index] = 0;
                return;
            }
        }
    }

    void try_select_slot(GameContext& ctx, int slot_index)
    {
        ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(slot_index < 0 || slot_index >= fx.slot_count || fx.swapping)
        {
            return;
        }

        if(slot_is_selected(fx, slot_index))
        {
            return;
        }

        if(fx.selected_count >= 2)
        {
            return;
        }

        fx.selected_slots[fx.selected_count] = slot_index;
        ++fx.selected_count;

        if(fx.selected_count == 2)
        {
            if(!swap_keeps_total_score(ctx, fx.selected_slots[0], fx.selected_slots[1]))
            {
                deselect_slot(fx, fx.selected_slots[1]);
                return;
            }

            begin_digit_swap_animation(fx);
        }
    }

    void update_digit_sprite_positions(GameContext& ctx)
    {
        ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(! fx.active || fx.slot_count <= 0)
        {
            return;
        }

        for(int index = 0; index < fx.slot_count; ++index)
        {
            ease_raise_toward(fx.digit_raise[index], digit_raise_target(fx, index));
        }

        if(fx.swapping)
        {
            const int spacing = fx.base_sprite_x[fx.swap_slot_b] - fx.base_sprite_x[fx.swap_slot_a];
            const int shift = swap_eased_shift(spacing, fx.swap_frame);
            const int arc = swap_vertical_arc(fx.swap_frame, game_layout::SWAP_FRAMES, game_layout::SWAP_ARC_PEAK);
            const bool cross_field = fx.slots[fx.swap_slot_a].field != fx.slots[fx.swap_slot_b].field;
            const int y_spacing = fx.base_sprite_y[fx.swap_slot_b] - fx.base_sprite_y[fx.swap_slot_a];
            const int y_shift = cross_field ? swap_eased_shift(y_spacing, fx.swap_frame) : 0;

            for(int index = 0; index < fx.slot_count; ++index)
            {
                bn::sprite_ptr* sprite = sprite_for_slot(ctx, fx.slots[index]);

                if(!sprite)
                {
                    continue;
                }

                int x = fx.base_sprite_x[index];
                int y = fx.base_sprite_y[index] - fx.digit_raise[index];

                if(index == fx.swap_slot_a)
                {
                    x += shift;
                    y -= y_shift;
                    y -= arc;
                }
                else if(index == fx.swap_slot_b)
                {
                    x -= shift;
                    y += y_shift;
                    y -= arc;
                }

                sprite->set_position(x, y);
            }

            return;
        }

        for(int index = 0; index < fx.slot_count; ++index)
        {
            bn::sprite_ptr* sprite = sprite_for_slot(ctx, fx.slots[index]);

            if(!sprite)
            {
                continue;
            }

            sprite->set_position(fx.base_sprite_x[index],
                                 fx.base_sprite_y[index] - fx.digit_raise[index]);
        }
    }
}

bool score_swap_try_begin(GameContext& ctx)
{
    ctx.draw_round_score();
    ctx.show_total_score_value(ctx.state.total_score);

    const bn::string<12> round_digits = bn::to_string<12>(ctx.state.round.running);
    const bn::string<12> total_digits = bn::to_string<12>(ctx.state.total_score);

    if(round_digits.size() + total_digits.size() < 2)
    {
        return false;
    }

    ctx.round_score_wiggle_frames = 0;
    ctx.total_score_wiggle_frames = 0;
    ctx._round_wiggle_x = 0;
    ctx._round_wiggle_y = 0;
    ctx._total_wiggle_x = 0;
    ctx._total_wiggle_y = 0;

    ScoreSwapFxState& fx = ctx.score_swap_fx;
    fx = ScoreSwapFxState{};
    fx.active = true;
    fx.cursor_slot = 0;
    cache_digit_layout(ctx, fx);

    if(fx.slot_count < 2)
    {
        fx = ScoreSwapFxState{};
        return false;
    }

    return true;
}

void score_swap_handle_input(GameContext& ctx)
{
    ScoreSwapFxState& fx = ctx.score_swap_fx;

    if(! fx.active || fx.swapping)
    {
        return;
    }

    if(bn::keypad::left_pressed())
    {
        move_cursor_in_field(fx, -1);
    }

    if(bn::keypad::right_pressed())
    {
        move_cursor_in_field(fx, 1);
    }

    if(bn::keypad::up_pressed() || bn::keypad::down_pressed())
    {
        const SwapScoreField current_field = fx.slots[fx.cursor_slot].field;
        const int digit_index = fx.slots[fx.cursor_slot].digit_index;
        SwapScoreField target_field = current_field;

        if(bn::keypad::up_pressed() && current_field == SwapScoreField::ROUND)
        {
            target_field = SwapScoreField::TOTAL;
        }
        else if(bn::keypad::down_pressed() && current_field == SwapScoreField::TOTAL)
        {
            target_field = SwapScoreField::ROUND;
        }

        if(target_field != current_field)
        {
            fx.cursor_slot = slot_for_field_digit(fx, target_field, digit_index);
        }
    }

    if(ctx.confirm_pressed())
    {
        try_select_slot(ctx, fx.cursor_slot);
    }

    if(bn::keypad::b_pressed())
    {
        if(slot_is_selected(fx, fx.cursor_slot))
        {
            deselect_slot(fx, fx.cursor_slot);
        }
        else if(fx.selected_count == 0)
        {
            finish_score_swap(ctx);
        }
    }
}

void score_swap_tick(GameContext& ctx)
{
    ScoreSwapFxState& fx = ctx.score_swap_fx;

    if(! fx.active)
    {
        return;
    }

    if(fx.swapping)
    {
        ++fx.swap_frame;

        if(fx.swap_frame >= game_layout::SWAP_FRAMES)
        {
            if(apply_digit_swap(ctx, fx.swap_slot_a, fx.swap_slot_b))
            {
                finish_score_swap(ctx);
            }
            else
            {
                fx.swapping = false;
                fx.swap_frame = 0;
                fx.swap_slot_a = -1;
                fx.swap_slot_b = -1;
                fx.selected_count = 0;
                fx.selected_slots[0] = -1;
                fx.selected_slots[1] = -1;
                cache_digit_layout(ctx, fx);
            }

            return;
        }
    }

    update_digit_sprite_positions(ctx);
}

bool score_swap_is_active(const GameContext& ctx)
{
    return ctx.score_swap_fx.active;
}
