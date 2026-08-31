#include "score_swap_system.h"

#include "bn_color.h"
#include "bn_keypad.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_string.h"
#include "bn_tile.h"
#include "bn_utility.h"

#include "game_context.h"
#include "game_helpers.h"
#include "game_ui.h"
#include "score_count_system.h"
#include "scoring.h"
#include "trinket_system.h"

namespace
{
    constexpr int SCORE_DIGIT_RAISE = 12;

    bool is_digit_slide_mode(ScoreDigitEditMode edit_mode)
    {
        return edit_mode == ScoreDigitEditMode::MOVE_FOUR ||
               edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL ||
               edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT;
    }

    bool is_four_pick_mode(ScoreDigitEditMode edit_mode)
    {
        return edit_mode == ScoreDigitEditMode::MOVE_FOUR ||
               edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL;
    }

    bool is_interactive_digit_edit(ScoreDigitEditMode edit_mode)
    {
        return edit_mode == ScoreDigitEditMode::SWAP || is_four_pick_mode(edit_mode) ||
               edit_mode == ScoreDigitEditMode::REPLACE_WITH_FIVE;
    }

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

        if(is_digit_slide_mode(fx.edit_mode))
        {
            // Slide modes own lift animation during the move.
            return 0;
        }

        if(slot_is_selected(fx, slot_index) || slot_index == fx.cursor_slot)
        {
            return SCORE_DIGIT_RAISE;
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

    bool has_non_decreasing_four_move(const bn::string<12>& source_digits, int current_total)
    {
        for(int source = 0; source < source_digits.size(); ++source)
        {
            if(source_digits[source] != '4')
            {
                continue;
            }

            for(int destination = 0; destination < source_digits.size(); ++destination)
            {
                if(destination == source)
                {
                    continue;
                }

                bn::string<12> digits = source_digits;

                if(source < destination)
                {
                    for(int index = source; index < destination; ++index)
                    {
                        digits[index] = digits[index + 1];
                    }
                }
                else
                {
                    for(int index = source; index > destination; --index)
                    {
                        digits[index] = digits[index - 1];
                    }
                }

                digits[destination] = '4';

                if(parse_score_digits(digits) >= current_total)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool compute_lift_four_round_to_total(int old_round, int old_total, int round_four_index,
                                          int total_dest_index, int& out_round, int& out_total)
    {
        const bn::string<12> round_digits = bn::to_string<12>(old_round);
        const bn::string<12> total_digits = bn::to_string<12>(old_total);

        if(round_four_index < 0 || round_four_index >= round_digits.size() ||
           round_digits[round_four_index] != '4' || total_dest_index < 0 ||
           total_dest_index > total_digits.size() || total_digits.empty())
        {
            return false;
        }

        const char ones_digit = total_digits[total_digits.size() - 1];
        bn::string<12> round_after;

        for(int index = 0; index < round_digits.size(); ++index)
        {
            if(index != round_four_index)
            {
                round_after.push_back(round_digits[index]);
            }
        }

        bn::string<12> new_total_digits;

        for(int index = 0; index < total_dest_index; ++index)
        {
            new_total_digits.push_back(total_digits[index]);
        }

        new_total_digits.push_back('4');

        for(int index = total_dest_index; index < total_digits.size(); ++index)
        {
            new_total_digits.push_back(total_digits[index]);
        }

        bn::string<12> new_round_digits;
        new_round_digits.push_back(ones_digit);

        for(int index = 0; index < round_after.size(); ++index)
        {
            new_round_digits.push_back(round_after[index]);
        }

        out_round = parse_score_digits(new_round_digits);
        out_total = parse_score_digits(new_total_digits);
        return out_round + out_total > old_round + old_total;
    }

    bool has_valid_lift_four_round_to_total(int old_round, int old_total)
    {
        const bn::string<12> round_digits = bn::to_string<12>(old_round);
        const bn::string<12> total_digits = bn::to_string<12>(old_total);
        int new_round = 0;
        int new_total = 0;

        for(int round_index = 0; round_index < round_digits.size(); ++round_index)
        {
            if(round_digits[round_index] != '4')
            {
                continue;
            }

            for(int total_index = 0; total_index <= total_digits.size(); ++total_index)
            {
                if(compute_lift_four_round_to_total(old_round, old_total, round_index, total_index,
                                                    new_round, new_total))
                {
                    return true;
                }
            }
        }

        return false;
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

    bn::sprite_ptr create_score_marker(int x, int y, bn::color color)
    {
        bn::sprite_tiles_ptr tiles = bn::sprite_tiles_ptr::allocate(1, bn::bpp_mode::BPP_4);
        auto vram = tiles.vram();
        auto* tile_span = vram.get();

        if(tile_span)
        {
            bn::tile& tile = (*tile_span)[0];

            for(int row = 0; row < 8; ++row)
            {
                tile.data[row] = (row == 3 || row == 4) ? 0x11111111 : 0;
            }
        }

        const bn::array<bn::color, 16> colors = {
            bn::color(), color, bn::color(), bn::color(),
            bn::color(), bn::color(), bn::color(), bn::color(),
            bn::color(), bn::color(), bn::color(), bn::color(),
            bn::color(), bn::color(), bn::color(), bn::color(),
        };
        const bn::sprite_palette_item palette_item(
            bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
        const bn::sprite_palette_ptr palette = bn::sprite_palette_ptr::create(palette_item);
        bn::sprite_ptr marker =
            bn::sprite_ptr::create(x, y, bn::sprite_shape_size(8, 8), tiles, palette);
        marker.set_z_order(-32767);
        marker.set_bg_priority(0);
        return marker;
    }

    int marker_y_for_slot(const ScoreSwapFxState& fx, int slot_index)
    {
        const int half_height =
            fx.slots[slot_index].field == SwapScoreField::TOTAL ? 32 : 8;
        return fx.base_sprite_y[slot_index] - SCORE_DIGIT_RAISE + half_height + 2;
    }

    void refresh_selected_markers(GameContext& ctx)
    {
        ScoreSwapFxState& fx = ctx.score_swap_fx;
        ctx.score_swap_marker_sprites.clear();
        constexpr bn::color SELECTED_GREEN(6, 28, 10);
        constexpr bn::color CURSOR_YELLOW(31, 25, 5);

        if(!is_interactive_digit_edit(fx.edit_mode))
        {
            return;
        }

        if(fx.edit_mode != ScoreDigitEditMode::MOVE_FOUR &&
           fx.edit_mode != ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL)
        {
            for(int selected = 0; selected < fx.selected_count; ++selected)
            {
                const int slot_index = fx.selected_slots[selected];

                if(slot_index < 0 || slot_index >= fx.slot_count)
                {
                    continue;
                }

                ctx.score_swap_marker_sprites.push_back(
                    create_score_marker(fx.base_sprite_x[slot_index],
                                        marker_y_for_slot(fx, slot_index), SELECTED_GREEN));
            }
        }

        if(fx.edit_mode != ScoreDigitEditMode::MOVE_FOUR &&
           fx.edit_mode != ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL &&
           fx.cursor_slot >= 0 && fx.cursor_slot < fx.slot_count &&
           !slot_is_selected(fx, fx.cursor_slot))
        {
            ctx.score_swap_marker_sprites.push_back(
                create_score_marker(fx.base_sprite_x[fx.cursor_slot],
                                    marker_y_for_slot(fx, fx.cursor_slot), CURSOR_YELLOW));
        }
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

    void reset_move_four_preview(ScoreSwapFxState& fx)
    {
        for(int index = 0; index < fx.slot_count; ++index)
        {
            fx.digit_preview_x[index] = fx.base_sprite_x[index];
        }
    }

    int next_four_slot(const GameContext& ctx, int current_slot, int delta, SwapScoreField field)
    {
        const ScoreSwapFxState& fx = ctx.score_swap_fx;
        const int score_value = field == SwapScoreField::ROUND ? ctx.state.round.running
                                                               : ctx.state.total_score;
        const bn::string<12> digits = bn::to_string<12>(score_value);

        for(int index = current_slot + delta; index >= 0 && index < fx.slot_count; index += delta)
        {
            if(fx.slots[index].field != field)
            {
                continue;
            }

            const int digit_index = fx.slots[index].digit_index;

            if(digit_index >= 0 && digit_index < digits.size() && digits[digit_index] == '4')
            {
                return index;
            }
        }

        return current_slot;
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

    bool compute_moved_digit_for_field(const GameContext& ctx, SwapScoreField field, int source_slot,
                                       int destination_slot, int& out_value)
    {
        const ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(source_slot < 0 || destination_slot < 0 || source_slot >= fx.slot_count ||
           destination_slot >= fx.slot_count || source_slot == destination_slot)
        {
            return false;
        }

        const int source_index = fx.slots[source_slot].digit_index;
        const int destination_index = fx.slots[destination_slot].digit_index;
        const int score_value = field == SwapScoreField::ROUND ? ctx.state.round.running
                                                               : ctx.state.total_score;
        bn::string<12> digits = bn::to_string<12>(score_value);

        if(source_index < 0 || destination_index < 0 ||
           source_index >= digits.size() || destination_index >= digits.size())
        {
            return false;
        }

        const char moved = digits[source_index];

        if(source_index < destination_index)
        {
            for(int index = source_index; index < destination_index; ++index)
            {
                digits[index] = digits[index + 1];
            }
        }
        else
        {
            for(int index = source_index; index > destination_index; --index)
            {
                digits[index] = digits[index - 1];
            }
        }

        digits[destination_index] = moved;
        out_value = parse_score_digits(digits);
        return true;
    }

    bool compute_moved_digit_total(const GameContext& ctx, int source_slot, int destination_slot, int& out_total)
    {
        return compute_moved_digit_for_field(ctx, SwapScoreField::TOTAL, source_slot, destination_slot, out_total);
    }

    bool compute_moved_four_total(const GameContext& ctx, int source_slot, int destination_slot, int& out_total)
    {
        const ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(source_slot < 0 || destination_slot < 0 || source_slot >= fx.slot_count ||
           destination_slot >= fx.slot_count || source_slot == destination_slot)
        {
            return false;
        }

        const int source_index = fx.slots[source_slot].digit_index;
        const bn::string<12> digits = bn::to_string<12>(ctx.state.total_score);

        if(source_index < 0 || source_index >= digits.size() || digits[source_index] != '4')
        {
            return false;
        }

        return compute_moved_digit_total(ctx, source_slot, destination_slot, out_total);
    }

    void apply_round_edit(GameContext& ctx, int new_round)
    {
        const int round_before = ctx.state.round.running;
        ctx.state.round.running = new_round;
        score_count_queue(ctx.state, TrinketScoreField::ROUND, round_before, new_round);
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, round_before, new_round);
    }

    void apply_total_edit(GameContext& ctx, int new_total)
    {
        const int total_before = ctx.state.total_score;
        ctx.state.total_score = new_total;
        score_count_queue(ctx.state, TrinketScoreField::TOTAL, total_before, new_total);
        trinket_queue_score_check(ctx.state, TrinketScoreField::TOTAL, total_before, new_total);
    }

    bool apply_moved_digit(GameContext& ctx, int source_slot, int destination_slot)
    {
        const SwapScoreField field = ctx.score_swap_fx.auto_move_field;
        int new_value = 0;

        if(!compute_moved_digit_for_field(ctx, field, source_slot, destination_slot, new_value))
        {
            return false;
        }

        if(field == SwapScoreField::ROUND)
        {
            apply_round_edit(ctx, new_value);
        }
        else
        {
            apply_total_edit(ctx, new_value);
        }

        return true;
    }

    bool apply_lift_four_round_to_total(GameContext& ctx, int round_source_slot, int total_dest_slot)
    {
        const ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(round_source_slot < 0 || total_dest_slot < 0 || round_source_slot >= fx.slot_count ||
           total_dest_slot >= fx.slot_count)
        {
            return false;
        }

        const ScoreSwapDigitSlot& round_slot = fx.slots[round_source_slot];
        const ScoreSwapDigitSlot& total_slot = fx.slots[total_dest_slot];

        if(round_slot.field != SwapScoreField::ROUND || total_slot.field != SwapScoreField::TOTAL)
        {
            return false;
        }

        const int old_round = ctx.state.round.running;
        const int old_total = ctx.state.total_score;
        int new_round = 0;
        int new_total = 0;

        if(!compute_lift_four_round_to_total(old_round, old_total, round_slot.digit_index,
                                            total_slot.digit_index, new_round, new_total))
        {
            return false;
        }

        ctx.state.round.running = new_round;
        ctx.state.total_score = new_total;
        score_count_queue(ctx.state, TrinketScoreField::ROUND, old_round, new_round);
        score_count_queue(ctx.state, TrinketScoreField::TOTAL, old_total, new_total);
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, old_round, new_round);
        trinket_queue_score_check(ctx.state, TrinketScoreField::TOTAL, old_total, new_total);
        return true;
    }

    bool apply_moved_four(GameContext& ctx, int source_slot, int destination_slot)
    {
        int new_total = 0;

        if(!compute_moved_four_total(ctx, source_slot, destination_slot, new_total))
        {
            return false;
        }

        if(new_total < ctx.state.total_score)
        {
            return false;
        }

        apply_total_edit(ctx, new_total);
        return true;
    }

    bool apply_replace_with_five(GameContext& ctx, int slot_index)
    {
        const ScoreSwapFxState& fx = ctx.score_swap_fx;

        if(slot_index < 0 || slot_index >= fx.slot_count)
        {
            return false;
        }

        const int digit_index = fx.slots[slot_index].digit_index;
        bn::string<12> digits = bn::to_string<12>(ctx.state.total_score);

        if(digit_index < 0 || digit_index >= digits.size())
        {
            return false;
        }

        digits[digit_index] = '5';
        apply_total_edit(ctx, parse_score_digits(digits));
        return true;
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
        const int round_before = ctx.state.round.running;
        ctx.state.round.running = new_round;
        ctx.state.total_score = new_total;
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, round_before, new_round);
        trinket_queue_score_check(ctx.state, TrinketScoreField::TOTAL, total_before, new_total);
        return true;
    }

    void cache_digit_layout(GameContext& ctx, ScoreSwapFxState& fx)
    {
        fx.slot_count = 0;

        const bn::string<12> round_digits = bn::to_string<12>(ctx.state.round.running);
        const bn::string<12> total_digits = bn::to_string<12>(ctx.state.total_score);
        const bool cache_round = fx.edit_mode == ScoreDigitEditMode::SWAP ||
                                 fx.edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL ||
                                 (fx.edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT &&
                                  fx.auto_move_field == SwapScoreField::ROUND);
        const bool cache_total = fx.edit_mode == ScoreDigitEditMode::SWAP ||
                                 fx.edit_mode == ScoreDigitEditMode::MOVE_FOUR ||
                                 fx.edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL ||
                                 fx.edit_mode == ScoreDigitEditMode::REPLACE_WITH_FIVE ||
                                 (fx.edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT &&
                                  fx.auto_move_field == SwapScoreField::TOTAL);

        if(cache_round)
        {
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
        }

        if(cache_total)
        {
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
            fx.digit_preview_x[index] = fx.base_sprite_x[index];
            fx.digit_raise[index] = 0;
        }
    }

    void finish_score_swap(GameContext& ctx)
    {
        ctx.score_swap_marker_sprites.clear();
        ctx.score_swap_fx = ScoreSwapFxState{};
        ctx.round_text_generator.set_one_sprite_per_character(false);

        // Digit-swap animation moves sprites manually; always rebuild at canonical layout.
        score_count_cancel(ctx, TrinketScoreField::ROUND);
        score_count_cancel(ctx, TrinketScoreField::TOTAL);

        ctx._round_wiggle_x = 0;
        ctx._round_wiggle_y = 0;
        ctx._total_wiggle_x = 0;
        ctx._total_wiggle_y = 0;
        ctx.round_score_wiggle_frames = 0;
        ctx.total_score_wiggle_frames = 0;

        ctx._round_score_initialized = false;
        ctx._cached_round_score_text = "";
        ctx._total_score_initialized = false;
        ctx._cached_total_score = 0;

        ctx.round_text_sprites.clear();
        ctx.text_sprites.clear();
        ctx.last_round_sprite_offset = 0;
        ctx.last_main_sprite_offset = 0;

        ctx.show_round_score_running(ctx.state.round.running, ctx.state.round.end_multiplier);
        ctx.show_total_score_value(ctx.state.total_score);

        ctx._round_score_initialized = true;
        ctx._cached_round_score_text = format_round_score(ctx.state.round);
        ctx._total_score_initialized = true;
        ctx._cached_total_score = ctx.state.total_score;

        ctx.begin_next_pending_or_finish(true);
    }

    void begin_digit_swap_animation(ScoreSwapFxState& fx)
    {
        fx.swap_slot_a = fx.selected_slots[0];
        fx.swap_slot_b = fx.selected_slots[1];

        if(fx.edit_mode == ScoreDigitEditMode::SWAP && fx.swap_slot_a > fx.swap_slot_b)
        {
            bn::swap(fx.swap_slot_a, fx.swap_slot_b);
        }

        fx.swapping = true;
        fx.swap_frame = 0;
        fx.selected_count = 0;
        fx.selected_slots[0] = -1;
        fx.selected_slots[1] = -1;
    }

    void deselect_slot(GameContext& ctx, int slot_index)
    {
        ScoreSwapFxState& fx = ctx.score_swap_fx;

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
                refresh_selected_markers(ctx);
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

        if(fx.edit_mode == ScoreDigitEditMode::REPLACE_WITH_FIVE)
        {
            if(apply_replace_with_five(ctx, slot_index))
            {
                finish_score_swap(ctx);
            }

            return;
        }

        if(fx.edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL && fx.selected_count == 1)
        {
            if(fx.slots[slot_index].field == SwapScoreField::TOTAL &&
               apply_lift_four_round_to_total(ctx, fx.selected_slots[0], slot_index))
            {
                finish_score_swap(ctx);
            }

            return;
        }

        if(fx.edit_mode == ScoreDigitEditMode::MOVE_FOUR && fx.selected_count == 1)
        {
            if(slot_index != fx.selected_slots[0] &&
               apply_moved_four(ctx, fx.selected_slots[0], slot_index))
            {
                finish_score_swap(ctx);
            }

            return;
        }

        if(slot_is_selected(fx, slot_index))
        {
            return;
        }

        if(fx.edit_mode == ScoreDigitEditMode::MOVE_FOUR && fx.selected_count == 0)
        {
            const bn::string<12> digits = bn::to_string<12>(ctx.state.total_score);
            const int digit_index = fx.slots[slot_index].digit_index;

            if(digit_index < 0 || digit_index >= digits.size() || digits[digit_index] != '4')
            {
                return;
            }
        }

        if(fx.selected_count >= 2)
        {
            return;
        }

        fx.selected_slots[fx.selected_count] = slot_index;
        ++fx.selected_count;
        refresh_selected_markers(ctx);

        if(fx.selected_count == 2)
        {
            ctx.score_swap_marker_sprites.clear();
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
            if(is_digit_slide_mode(fx.edit_mode))
            {
                const int source = fx.swap_slot_a;
                const int destination = fx.swap_slot_b;
                const int source_x = fx.base_sprite_x[source];
                const int destination_x = fx.base_sprite_x[destination];
                const int source_shift = swap_eased_shift(destination_x - source_x, fx.swap_frame);
                const int arc =
                    swap_vertical_arc(fx.swap_frame, game_layout::SWAP_FRAMES, game_layout::SWAP_ARC_PEAK);

                for(int index = 0; index < fx.slot_count; ++index)
                {
                    bn::sprite_ptr* sprite = sprite_for_slot(ctx, fx.slots[index]);

                    if(!sprite)
                    {
                        continue;
                    }

                    int x = fx.base_sprite_x[index];
                    int y = fx.base_sprite_y[index];

                    if(index == source)
                    {
                        x += source_shift;
                        y -= arc;
                    }
                    else if(source < destination && index > source && index <= destination)
                    {
                        x += swap_eased_shift(fx.base_sprite_x[index - 1] - x, fx.swap_frame);
                    }
                    else if(source > destination && index >= destination && index < source)
                    {
                        x += swap_eased_shift(fx.base_sprite_x[index + 1] - x, fx.swap_frame);
                    }

                    sprite->set_position(x, y);
                }

                return;
            }

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
                    y += y_shift;
                    y -= arc;
                }
                else if(index == fx.swap_slot_b)
                {
                    x -= shift;
                    y -= y_shift;
                    y -= arc;
                }

                sprite->set_position(x, y);
            }

            return;
        }

        if(is_four_pick_mode(fx.edit_mode) && fx.selected_count == 1)
        {
            const int source = fx.selected_slots[0];
            const int destination = fx.cursor_slot;
            constexpr int TAKEOFF_FRAMES = 48;
            const bool taking_off = fx.hover_frame < TAKEOFF_FRAMES;
            bn::fixed selected_lift = SCORE_DIGIT_RAISE;
            bn::fixed tilt_angle = 10;

            if(taking_off)
            {
                const bn::fixed time = bn::fixed(fx.hover_frame) / TAKEOFF_FRAMES;
                const bn::fixed eased = time * time * (3 - 2 * time);
                selected_lift = eased * SCORE_DIGIT_RAISE;
                tilt_angle = eased * 10;
            }
            else
            {
                // A balanced, smooth loop: ease up and down without discrete height steps.
                const int hover_phase = (fx.hover_frame - TAKEOFF_FRAMES) % 98;
                bn::fixed hover_eased;

                if(hover_phase <= 49)
                {
                    const bn::fixed time = bn::fixed(hover_phase) / 49;
                    hover_eased = time * time * (3 - 2 * time);
                }
                else
                {
                    const bn::fixed time = bn::fixed(hover_phase - 49) / 48;
                    const bn::fixed eased = time * time * (3 - 2 * time);
                    hover_eased = 1 - eased;
                }

                selected_lift += hover_eased * 4;
                tilt_angle -= hover_eased;
            }

            for(int index = 0; index < fx.slot_count; ++index)
            {
                int target_x = fx.base_sprite_x[index];

                if(index == source)
                {
                    target_x = fx.base_sprite_x[destination];
                }
                else if(source < destination && index > source && index <= destination)
                {
                    target_x = fx.base_sprite_x[index - 1];
                }
                else if(source > destination && index >= destination && index < source)
                {
                    target_x = fx.base_sprite_x[index + 1];
                }

                const int delta = target_x - fx.digit_preview_x[index];

                if(delta > 3)
                {
                    fx.digit_preview_x[index] += 3;
                }
                else if(delta < -3)
                {
                    fx.digit_preview_x[index] -= 3;
                }
                else
                {
                    fx.digit_preview_x[index] = target_x;
                }

                bn::sprite_ptr* sprite = sprite_for_slot(ctx, fx.slots[index]);

                if(sprite)
                {
                    if(index == source)
                    {
                        sprite->set_rotation_angle(tilt_angle);
                    }

                    const bn::fixed lift = index == source ? selected_lift : 0;
                    sprite->set_position(
                        fx.digit_preview_x[index],
                        bn::fixed(fx.base_sprite_y[index] - fx.digit_raise[index]) - lift);
                }
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

    bool begin_score_digit_edit(GameContext& ctx, ScoreDigitEditMode edit_mode,
                                SwapScoreField auto_move_field = SwapScoreField::TOTAL)
    {
        if(edit_mode == ScoreDigitEditMode::SWAP ||
           edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL ||
           (edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT && auto_move_field == SwapScoreField::ROUND))
        {
            // Normal HUD text packs several small round-score characters into one
            // sprite. Digit edits need a stable sprite and position per character.
            ctx.round_text_generator.set_one_sprite_per_character(true);
            ctx._round_score_initialized = false;
            ctx.show_round_score_running(ctx.state.round.running, ctx.state.round.end_multiplier);
        }
        else
        {
            ctx.draw_round_score();
        }

        ctx.show_total_score_value(ctx.state.total_score);

        const bn::string<12> round_digits = bn::to_string<12>(ctx.state.round.running);
        const bn::string<12> total_digits = bn::to_string<12>(ctx.state.total_score);

        if(edit_mode == ScoreDigitEditMode::SWAP &&
           round_digits.size() + total_digits.size() < 2)
        {
            return false;
        }

        if(edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL)
        {
            bool found_four = false;

            for(char digit : round_digits)
            {
                if(digit == '4')
                {
                    found_four = true;
                    break;
                }
            }

            if(!found_four || total_digits.empty() ||
               !has_valid_lift_four_round_to_total(ctx.state.round.running, ctx.state.total_score))
            {
                return false;
            }
        }

        if(edit_mode == ScoreDigitEditMode::MOVE_FOUR)
        {
            bool found_four = false;

            for(char digit : total_digits)
            {
                if(digit == '4')
                {
                    found_four = true;
                    break;
                }
            }

            if(!found_four || total_digits.size() < 2 ||
               !has_non_decreasing_four_move(total_digits, ctx.state.total_score))
            {
                return false;
            }
        }

        ctx.round_score_wiggle_frames = 0;
        ctx.total_score_wiggle_frames = 0;
        ctx._round_wiggle_x = 0;
        ctx._round_wiggle_y = 0;
        ctx._total_wiggle_x = 0;
        ctx._total_wiggle_y = 0;

        ScoreSwapFxState& fx = ctx.score_swap_fx;
        ctx.score_swap_marker_sprites.clear();
        fx = ScoreSwapFxState{};
        fx.active = true;
        fx.edit_mode = edit_mode;
        fx.auto_move_field = auto_move_field;
        cache_digit_layout(ctx, fx);

        const int minimum_slots = edit_mode == ScoreDigitEditMode::REPLACE_WITH_FIVE ? 1 : 2;

        if(fx.slot_count < minimum_slots)
        {
            fx = ScoreSwapFxState{};
            return false;
        }

        fx.cursor_slot = 0;

        if(edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL)
        {
            for(int index = fx.slot_count - 1; index >= 0; --index)
            {
                if(fx.slots[index].field != SwapScoreField::ROUND)
                {
                    continue;
                }

                if(round_digits[fx.slots[index].digit_index] == '4')
                {
                    fx.selected_slots[0] = index;
                    fx.selected_count = 1;
                    fx.cursor_slot = slot_for_field_digit(fx, SwapScoreField::TOTAL, 0);
                    break;
                }
            }
        }
        else if(edit_mode == ScoreDigitEditMode::MOVE_FOUR)
        {
            for(int index = fx.slot_count - 1; index >= 0; --index)
            {
                if(total_digits[fx.slots[index].digit_index] == '4')
                {
                    fx.cursor_slot = index;
                    fx.selected_slots[0] = index;
                    fx.selected_count = 1;
                    break;
                }
            }
        }

        refresh_selected_markers(ctx);
        return true;
    }

    int slot_for_exact_field_digit(const ScoreSwapFxState& fx, SwapScoreField field, int digit_index)
    {
        for(int index = 0; index < fx.slot_count; ++index)
        {
            if(fx.slots[index].field == field && fx.slots[index].digit_index == digit_index)
            {
                return index;
            }
        }

        return -1;
    }

    bool begin_auto_digit_move(GameContext& ctx, SwapScoreField field, int source_digit_index,
                               int dest_digit_index)
    {
        if(!begin_score_digit_edit(ctx, ScoreDigitEditMode::AUTO_MOVE_DIGIT, field))
        {
            return false;
        }

        ScoreSwapFxState& fx = ctx.score_swap_fx;
        const int source_slot = slot_for_exact_field_digit(fx, field, source_digit_index);
        const int dest_slot = slot_for_exact_field_digit(fx, field, dest_digit_index);

        if(source_slot < 0 || dest_slot < 0 || source_slot == dest_slot)
        {
            fx = ScoreSwapFxState{};
            return false;
        }

        fx.swap_slot_a = source_slot;
        fx.swap_slot_b = dest_slot;
        fx.swapping = true;
        fx.swap_frame = 0;
        return true;
    }

    bool begin_lift_or_fall_move(GameContext& ctx, bool major_lift)
    {
        DigitMovePlan plan;

        if(major_lift ? plan_major_lift(ctx.state.total_score, plan)
                      : plan_minor_fall(ctx.state.total_score, plan))
        {
            if(begin_auto_digit_move(ctx, SwapScoreField::TOTAL, plan.source_digit_index,
                                     plan.dest_digit_index))
            {
                return true;
            }
        }

        if(major_lift ? plan_major_lift(ctx.state.round.running, plan)
                      : plan_minor_fall(ctx.state.round.running, plan))
        {
            if(begin_auto_digit_move(ctx, SwapScoreField::ROUND, plan.source_digit_index,
                                     plan.dest_digit_index))
            {
                return true;
            }
        }

        return false;
    }
}

bool score_swap_try_begin(GameContext& ctx)
{
    return begin_score_digit_edit(ctx, ScoreDigitEditMode::SWAP);
}

bool score_fourth_try_begin(GameContext& ctx)
{
    return begin_score_digit_edit(ctx, ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL);
}

bool score_fifth_try_begin(GameContext& ctx)
{
    return begin_score_digit_edit(ctx, ScoreDigitEditMode::REPLACE_WITH_FIVE);
}

bool score_major_lift_try_begin(GameContext& ctx)
{
    return begin_lift_or_fall_move(ctx, true);
}

bool score_minor_fall_try_begin(GameContext& ctx)
{
    return begin_lift_or_fall_move(ctx, false);
}

void score_swap_handle_input(GameContext& ctx)
{
    ScoreSwapFxState& fx = ctx.score_swap_fx;

    if(!fx.active || fx.swapping || fx.edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT)
    {
        return;
    }

    const int cursor_before = fx.cursor_slot;
    const int horizontal = bn::keypad::left_pressed() ? -1 :
                           bn::keypad::right_pressed() ? 1 : 0;

    if(is_four_pick_mode(fx.edit_mode) && bn::keypad::b_held() && horizontal != 0)
    {
        const int source = fx.selected_slots[0];
        const SwapScoreField field = fx.edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL
                                         ? SwapScoreField::ROUND
                                         : SwapScoreField::TOTAL;
        const int next_source = next_four_slot(ctx, source, horizontal, field);

        if(next_source != source)
        {
            if(bn::sprite_ptr* old_source_sprite = sprite_for_slot(ctx, fx.slots[source]))
            {
                old_source_sprite->set_rotation_angle(0);
            }

            fx.selected_slots[0] = next_source;
            if(fx.edit_mode == ScoreDigitEditMode::MOVE_FOUR)
            {
                fx.cursor_slot = next_source;
            }

            fx.hover_frame = 0;
            reset_move_four_preview(fx);
        }
    }
    else if(horizontal != 0)
    {
        move_cursor_in_field(fx, horizontal);

        if(fx.edit_mode == ScoreDigitEditMode::LIFT_FOUR_ROUND_TO_TOTAL)
        {
            ctx.sync_score_digit_view(SwapScoreField::TOTAL, fx.slots[fx.cursor_slot].digit_index);
        }
    }

    if(fx.edit_mode == ScoreDigitEditMode::SWAP &&
       (bn::keypad::up_pressed() || bn::keypad::down_pressed()))
    {
        const SwapScoreField current_field = fx.slots[fx.cursor_slot].field;
        const int digit_index = fx.slots[fx.cursor_slot].digit_index;
        const SwapScoreField target_field =
            current_field == SwapScoreField::ROUND ? SwapScoreField::TOTAL : SwapScoreField::ROUND;
        fx.cursor_slot = slot_for_field_digit(fx, target_field, digit_index);
    }

    if(fx.cursor_slot != cursor_before)
    {
        refresh_selected_markers(ctx);
    }

    // Up is a general gameplay confirm shortcut, but in this mode it belongs
    // exclusively to row navigation. Digit picks are intentionally A-only.
    if(bn::keypad::a_pressed())
    {
        try_select_slot(ctx, fx.cursor_slot);
    }

    if(bn::keypad::b_pressed())
    {
        if(is_four_pick_mode(fx.edit_mode))
        {
            return;
        }

        if(slot_is_selected(fx, fx.cursor_slot))
        {
            deselect_slot(ctx, fx.cursor_slot);
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

    if(is_four_pick_mode(fx.edit_mode))
    {
        ++fx.hover_frame;
    }

    if(fx.swapping)
    {
        ++fx.swap_frame;

        if(fx.swap_frame >= game_layout::SWAP_FRAMES)
        {
            bool applied = false;

            if(fx.edit_mode == ScoreDigitEditMode::MOVE_FOUR)
            {
                applied = apply_moved_four(ctx, fx.swap_slot_a, fx.swap_slot_b);
            }
            else if(fx.edit_mode == ScoreDigitEditMode::AUTO_MOVE_DIGIT)
            {
                applied = apply_moved_digit(ctx, fx.swap_slot_a, fx.swap_slot_b);
            }
            else
            {
                applied = apply_digit_swap(ctx, fx.swap_slot_a, fx.swap_slot_b);
            }

            if(applied)
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
