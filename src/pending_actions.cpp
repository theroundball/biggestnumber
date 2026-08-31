#include "game_context.h"

#include "combo_system.h"
#include "card_data.h"
#include "game_events.h"
#include "game_helpers.h"
#include "play_resolution.h"
#include "score_swap_system.h"
#include "swivel_system.h"
#include "trinket_system.h"

// Pending-action mode entry.
// Order in the queue matters: combo cinematics and miracle auto-plays can
// interleave with player selections; each starter either enters a GameMode
// (stop) or fizzles / finishes instantly (keep draining).

namespace
{
    enum class PendingStartResult
    {
        FIZZLE,        // Skip — keep draining the queue
        ENTERED_MODE,  // Interactive mode started — stop the loop
        INSTANT_DONE,  // Side effects applied — keep draining
    };

    void begin_selection(GameContext& ctx, PendingActionType type)
    {
        ctx.state.selection = SelectionSession{};
        ctx.state.selection.type = type;
    }

    void begin_graveyard_target(GameContext& ctx, const PendingAction& action, int remaining_picks,
                                int multiply_factor)
    {
        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = remaining_picks;
        ctx.state.selection.multiply_factor = multiply_factor;
        ctx.state.selection.exiled_count = 0;
        ctx.state.selection.graveyard_exclude = action.graveyard_exclude;
        ctx.state.selection.cursor = initial_graveyard_cursor(ctx.state, action.graveyard_exclude);
        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor, ctx.state.graveyard.size(),
                                     game_layout::GRAVE_SPACING);
        ctx.mode = GameMode::GRAVEYARD_TARGET;
    }

    PendingStartResult start_combo_cinematic(GameContext& ctx, const PendingAction&)
    {
        if(combo_start_cinematic_if_valid(ctx.state))
        {
            ctx.enter_combo_mode();
            return PendingStartResult::ENTERED_MODE;
        }

        return PendingStartResult::FIZZLE;
    }

    PendingStartResult start_play_deck_top(GameContext& ctx, const PendingAction&)
    {
        CardRef top;

        if(!ctx.state.deck.draw(top))
        {
            return PendingStartResult::FIZZLE;
        }

        const int main_x = ctx.main_panel_offset_x();
        PlayResolutionContext context;
        context.source = PlaySource::DECK_TOP;
        context.apply_destination = false;

        const bool miracle = top.type == CardType::MIRACLE;

        if(miracle)
        {
            ctx.state.first_deck_draw_this_round = false;
        }

        ctx.begin_play_presentation(
            top,
            card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x),
            card_target_y_for_hud_icon(game_layout::HUD_DECK_Y),
            PlayPresentOrigin::DECK,
            context,
            removal_style_for_hand_play(top.type),
            miracle);

        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_mill_reveal(GameContext& ctx, const PendingAction& action)
    {
        CardRef top;

        if(!ctx.state.deck.draw(top))
        {
            return PendingStartResult::FIZZLE;
        }

        const bool flex = action.count == 1;
        const bool waterfall = action.count == 2;
        const bool hit = mill_reveal_card_hits(ctx.state, top, waterfall);
        const int main_x = ctx.main_panel_offset_x();

        PlayResolutionContext context;
        context.source = PlaySource::DECK_TOP;
        context.apply_destination = false;

        const bool miracle = hit && top.type == CardType::MIRACLE;

        if(miracle)
        {
            ctx.state.first_deck_draw_this_round = false;
        }

        ctx.begin_play_presentation(
            top,
            card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x),
            card_target_y_for_hud_icon(game_layout::HUD_DECK_Y),
            PlayPresentOrigin::DECK,
            context,
            hit ? removal_style_for_hand_play(top.type) : RemovalStyle::TO_GRAVEYARD,
            miracle);

        if(PlayFlight* flight = ctx.latest_play_flight())
        {
            flight->mill_without_play = !hit;
            flight->mill_reveal_flex_continue = flex && !hit;
            flight->mill_reveal_waterfall_continue = waterfall && hit;
            flight->mill_reveal_draw_on_hit = (waterfall || !flex) && hit;

            if(!hit)
            {
                flight->center_beat = false;
            }
        }

        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_roll_over_substitute(GameContext& ctx, const PendingAction&)
    {
        if(!begin_roll_over_substitution(ctx.state, &ctx.selected_card))
        {
            return PendingStartResult::FIZZLE;
        }

        ctx.update_target_scroll();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_play_random_graveyard(GameContext& ctx, const PendingAction&)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        const int index = ctx.state.rng.get_int(ctx.state.graveyard.size());
        const CardRef card = ctx.state.graveyard[index];
        graveyard_remove_at(ctx.state, index);

        const int main_x = ctx.main_panel_offset_x();
        int start_x = 0;
        int start_y = 0;

        if(!graveyard_cursor_screen_position(index, ctx.state.graveyard.size() + 1,
                                             game_layout::GRAVE_SPACING,
                                             game_layout::GRAVEYARD_BROWSE_Y, 0, ctx.row_scroll_x,
                                             main_x, start_x, start_y))
        {
            start_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
            start_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);
        }

        PlayResolutionContext context;
        context.source = PlaySource::HAND;
        context.apply_destination = false;

        ctx.begin_play_presentation(
            card,
            start_x,
            start_y,
            PlayPresentOrigin::HAND,
            context,
            removal_style_for_hand_play(card.type));

        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_miracle_auto_play(GameContext& ctx, const PendingAction& action)
    {
        if(action.hand_index >= 0 && action.hand_index < ctx.state.hand.size())
        {
            play_miracle_bonus(ctx.state, 10);
            hand_remove_at_to_graveyard(ctx.state, action.hand_index, ctx.selected_card);
            ctx.draw_round_score();
        }

        return PendingStartResult::INSTANT_DONE;
    }

    // Rags to Riches — optional exile picks, then × count. Mode: GRAVEYARD_TARGET.
    PendingStartResult start_exile_graveyard_multiply_by_count(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_graveyard_target(ctx, action, 0, 0);
        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_exile_from_graveyard(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_graveyard_target(ctx, action, 1, 0);
        return PendingStartResult::ENTERED_MODE;
    }

    // Clover — exile exactly N, then ×N. Mode: GRAVEYARD_TARGET.
    PendingStartResult start_exile_from_graveyard_then_multiply(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.size() < action.count)
        {
            return PendingStartResult::FIZZLE;
        }

        begin_graveyard_target(ctx, action, action.count, action.count);
        return PendingStartResult::ENTERED_MODE;
    }

    // Big Kurosawa — discard one hand card, then ×factor. Mode: DISCARD_TARGET.
    PendingStartResult start_discard_from_hand_then_multiply(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.hand.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = 1;
        ctx.state.selection.multiply_factor = action.count;
        ctx.mode = GameMode::DISCARD_TARGET;
        ctx.prepare_hand_selection_mode();
        return PendingStartResult::ENTERED_MODE;
    }

    // Overclock — optional discard chain; each pick multiplies by count (3, 4, 5…).
    PendingStartResult start_overclock_discard_prompt(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.hand.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = 1;
        ctx.state.selection.multiply_factor = action.count;
        ctx.mode = GameMode::DISCARD_TARGET;
        ctx.prepare_hand_selection_mode();
        return PendingStartResult::ENTERED_MODE;
    }

    // Discard or put-on-deck-top from hand. Mode: DISCARD_TARGET.
    PendingStartResult start_hand_card_select(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.hand.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = 1;
        ctx.mode = GameMode::DISCARD_TARGET;
        ctx.prepare_hand_selection_mode();
        return PendingStartResult::ENTERED_MODE;
    }

    // Retrieve from GY to hand or deck top. Mode: GRAVEYARD_TARGET.
    PendingStartResult start_retrieve_from_graveyard(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        begin_graveyard_target(ctx, action, 1, 0);
        return PendingStartResult::ENTERED_MODE;
    }

    // Seeds / ordered GY picks. Mode: GRAVEYARD_PICK.
    PendingStartResult start_graveyard_pick(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        if(action.type == PendingActionType::GRAVEYARD_PICK_TO_TOP &&
           ctx.state.graveyard.size() < action.count)
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.graveyard_exclude = action.graveyard_exclude;
        ctx.state.selection.cursor = initial_graveyard_cursor(ctx.state, action.graveyard_exclude);
        ctx.state.selection.remaining_picks = action.count;
        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor, ctx.state.graveyard.size(),
                                     game_layout::GRAVE_SPACING);
        ctx.mode = GameMode::GRAVEYARD_PICK;
        return PendingStartResult::ENTERED_MODE;
    }

    // Roll Over — swap two GY cards, repeated. Mode: GRAVEYARD_TARGET.
    PendingStartResult start_graveyard_pair_swap(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.graveyard.size() < 2)
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = action.count;
        ctx.state.selection.graveyard_swap_first = -1;
        ctx.state.selection.cursor = ctx.state.graveyard.size() - 1;
        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor, ctx.state.graveyard.size(),
                                     game_layout::GRAVE_SPACING);
        ctx.mode = GameMode::GRAVEYARD_TARGET;
        return PendingStartResult::ENTERED_MODE;
    }

    // Hacker — browse undrawn deck. Mode: DECK_SEARCH (or COMBO if revealed match).
    PendingStartResult start_deck_search(GameContext& ctx, const PendingAction& action)
    {
        ctx.state.deck.compact();
        begin_selection(ctx, action.type);
        ctx.state.selection.cursor = 0;

        for(int deck_index = 0; deck_index < ctx.state.deck.remaining(); ++deck_index)
        {
            ctx.state.selection.deck_search_buffer.push_back(ctx.state.deck.peek_undrawn_ref(deck_index));
        }

        if(ctx.state.selection.deck_search_buffer.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor,
                                     ctx.state.selection.deck_search_buffer.size(),
                                     game_layout::GRAVE_SPACING);
        combo_check_zone(ctx.state, ComboZone::REVEALED);

        if(combo_try_start_pending(ctx.state))
        {
            ctx.enter_combo_mode();
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.mode = GameMode::DECK_SEARCH;
        return PendingStartResult::ENTERED_MODE;
    }

    // Peanut Butter — scry 3, auto-top Jelly when present.
    PendingStartResult start_peanut_butter_scry(GameContext& ctx, const PendingAction& action)
    {
        ctx.state.deck.compact();
        const int count = action.count < ctx.state.deck.remaining() ? action.count : ctx.state.deck.remaining();

        if(count <= 0)
        {
            return PendingStartResult::FIZZLE;
        }

        bn::vector<CardRef, 3> peeked;

        for(int peek_index = 0; peek_index < count; ++peek_index)
        {
            CardRef drawn;

            if(!ctx.state.deck.draw(drawn))
            {
                break;
            }

            peeked.push_back(drawn);
        }

        int jelly_index = -1;

        for(int index = 0; index < peeked.size(); ++index)
        {
            if(peeked[index].type == CardType::JELLY)
            {
                jelly_index = index;
                break;
            }
        }

        if(jelly_index >= 0)
        {
            for(int index = peeked.size() - 1; index >= 0; --index)
            {
                if(index != jelly_index)
                {
                    ctx.state.deck.insert_top(peeked[index]);
                }
            }

            ctx.state.deck.insert_top(peeked[jelly_index]);
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, PendingActionType::SCRY);

        for(CardRef card : peeked)
        {
            ctx.state.selection.scry_buffer.push_back(card);
        }

        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor, ctx.state.selection.scry_buffer.size(),
                                     game_layout::SCRY_SPACING);
        combo_check_zone(ctx.state, ComboZone::REVEALED);

        if(combo_try_start_pending(ctx.state))
        {
            ctx.enter_combo_mode();
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.mode = GameMode::SCRY;
        return PendingStartResult::ENTERED_MODE;
    }

    // Paper — pick a hand card to swap with the slot Paper occupied.
    PendingStartResult start_paper_swap_hand(GameContext& ctx, const PendingAction& action)
    {
        if(ctx.state.hand.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        int anchor = action.count;

        if(anchor < 0)
        {
            anchor = 0;
        }

        if(anchor >= ctx.state.hand.size())
        {
            anchor = ctx.state.hand.size() - 1;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.remaining_picks = 1;
        ctx.state.selection.multiply_factor = anchor;
        ctx.state.selection.cursor = anchor == 0 ? (ctx.state.hand.size() > 1 ? 1 : 0) : 0;
        ctx.mode = GameMode::DISCARD_TARGET;
        ctx.prepare_hand_selection_mode();
        return PendingStartResult::ENTERED_MODE;
    }

    // Pilot / Librarian peek. Mode: SCRY (or COMBO if revealed match).
    PendingStartResult start_scry(GameContext& ctx, const PendingAction& action)
    {
        ctx.state.deck.compact();
        begin_selection(ctx, action.type);

        for(int peek_index = 0; peek_index < action.count; ++peek_index)
        {
            CardRef peeked;

            if(!ctx.state.deck.draw(peeked))
            {
                break;
            }

            ctx.state.selection.scry_buffer.push_back(peeked);
        }

        if(ctx.state.selection.scry_buffer.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        ctx.sync_row_scroll_for_mode(ctx.state.selection.cursor, ctx.state.selection.scry_buffer.size(),
                                     game_layout::SCRY_SPACING);
        combo_check_zone(ctx.state, ComboZone::REVEALED);

        if(combo_try_start_pending(ctx.state))
        {
            ctx.enter_combo_mode();
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.mode = GameMode::SCRY;
        return PendingStartResult::ENTERED_MODE;
    }

    // Build a Number — pick a digit slot after playing a +1..+9 card.
    PendingStartResult start_build_number_place_digit(GameContext& ctx, const PendingAction& action)
    {
        if(!ctx.state.build_a_number_active)
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);

        for(int slot = 0; slot < 3; ++slot)
        {
            if(ctx.state.build_digits[slot] < 0)
            {
                ctx.state.selection.cursor = slot;
                break;
            }
        }

        ctx.state.selection.multiply_factor = action.count;
        ctx._round_score_initialized = false;
        ctx.mode = GameMode::BUILD_NUMBER_DIGIT;
        ctx.draw_round_score();
        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_poker_hand_place_digit(GameContext& ctx, const PendingAction& action)
    {
        if(!ctx.state.poker_hand_active)
        {
            return PendingStartResult::FIZZLE;
        }

        begin_selection(ctx, action.type);
        ctx.state.selection.cursor = 0;
        ctx.state.selection.multiply_factor = action.count;
        ctx._round_score_initialized = false;
        ctx.mode = GameMode::POKER_HAND_DIGIT;
        ctx.draw_round_score();
        return PendingStartResult::ENTERED_MODE;
    }

    // Swap — digit swap on round + total score. Mode: SCORE_SWAP.
    PendingStartResult start_swap_total_score_digits(GameContext& ctx, const PendingAction&)
    {
        if(score_swap_try_begin(ctx))
        {
            ctx.mode = GameMode::SCORE_SWAP;
            return PendingStartResult::ENTERED_MODE;
        }

        return PendingStartResult::FIZZLE;
    }

    PendingStartResult start_move_four_total_digit(GameContext& ctx, const PendingAction&)
    {
        if(score_fourth_try_begin(ctx))
        {
            ctx.mode = GameMode::SCORE_SWAP;
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.state.add_from_card(4);
        ctx.draw_round_score();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_replace_total_digit_with_five(GameContext& ctx, const PendingAction&)
    {
        if(score_fifth_try_begin(ctx))
        {
            ctx.mode = GameMode::SCORE_SWAP;
            return PendingStartResult::ENTERED_MODE;
        }

        return PendingStartResult::FIZZLE;
    }

    PendingStartResult start_major_lift_total_digit(GameContext& ctx, const PendingAction&)
    {
        if(score_major_lift_try_begin(ctx))
        {
            ctx.mode = GameMode::SCORE_SWAP;
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.state.add_from_card(3);
        ctx.draw_round_score();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_minor_fall_total_digit(GameContext& ctx, const PendingAction&)
    {
        if(score_minor_fall_try_begin(ctx))
        {
            ctx.mode = GameMode::SCORE_SWAP;
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.state.add_from_card(3);
        ctx.draw_round_score();
        return PendingStartResult::INSTANT_DONE;
    }

    // Lifeline — runs after the played card is already in the GY so it is shuffled too.
    PendingStartResult start_necromancy_shuffle(GameContext& ctx, const PendingAction&)
    {
        if(ctx.state.graveyard.empty())
        {
            return PendingStartResult::FIZZLE;
        }

        ctx.state.selection.cursor = ctx.state.graveyard.size() - 1;
        ctx.begin_graveyard_card_fx(GraveyardExilePickKind::SHUFFLE_TO_DECK);
        return PendingStartResult::ENTERED_MODE;
    }

    PendingStartResult start_birds_return(GameContext& ctx, const PendingAction&)
    {
        int return_start = -1;
        int return_end = -1;

        if(!birds_find_return_run(ctx.state, return_start, return_end))
        {
            ctx.state.birds_return_start = -1;
            ctx.state.birds_return_count = 0;
            return PendingStartResult::FIZZLE;
        }

        ctx.state.birds_return_start = return_start;
        ctx.state.birds_return_count = return_end - return_start;

        if(ctx.try_begin_birds_return_fx())
        {
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.resolve_birds_return_instantly();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_reclaim_graveyard(GameContext& ctx, const PendingAction&)
    {
        (void)ctx;
        return PendingStartResult::FIZZLE;
    }

    PendingStartResult start_evaluate_ghost_step(GameContext& ctx, const PendingAction& action)
    {
        const int step = action.hand_index;

        if(step == 4)
        {
            for(int index = 0; index < 3; ++index)
            {
                ctx.state.future_mods[index] = RoundModifier{};
                ctx.state.keep_going_returns[index] = 0;
            }

            if(ctx.state.turtle_rounds_remaining > 0)
            {
                ctx.state.turtle_rounds_remaining = 0;
                ctx.state.flush_staircase_climb();
            }
        }
        else if(step == 3)
        {
            const int slot_index = (ctx.state.next_mod_index + action.count) % 3;
            ctx.state.future_mods[slot_index] = RoundModifier{};
            ctx.state.keep_going_returns[slot_index] = 0;
        }
        else
        {
            const int slot_index = (ctx.state.next_mod_index + action.count) % 3;
            RoundModifier& mod = ctx.state.future_mods[slot_index];

            if(step == 0 && mod.positive)
            {
                ctx.state.add_from_card(mod.positive);
                mod.positive = 0;
            }
            else if(step == 1 && mod.multiply)
            {
                ctx.state.apply_seed_multiply(mod.multiply);
                mod.multiply = 0;
            }
            else if(step == 2 && mod.draw_at_start)
            {
                queue_effect_draw(ctx.state, mod.draw_at_start, false);
                mod.draw_at_start = 0;
            }
        }

        ctx.draw_round_score();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_effect_deck_draw(GameContext& ctx, const PendingAction& action)
    {
        ctx.state.effect_draw_remaining = action.count;
        ctx.state.effect_draw_miracle_first = action.hand_index != 0;
        ctx.state.effect_draw_miracle_chaining = false;
        ctx.advance_effect_draw();
        return PendingStartResult::INSTANT_DONE;
    }

    PendingStartResult start_pending_action(GameContext& ctx, const PendingAction& action)
    {
        switch(action.type)
        {
        case PendingActionType::COMBO_CINEMATIC:
            return start_combo_cinematic(ctx, action);
        case PendingActionType::MIRACLE_AUTO_PLAY:
            return start_miracle_auto_play(ctx, action);
        case PendingActionType::EFFECT_DECK_DRAW:
            return start_effect_deck_draw(ctx, action);
        case PendingActionType::EXILE_FROM_GRAVEYARD:
            return start_exile_from_graveyard(ctx, action);
        case PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT:
            return start_exile_graveyard_multiply_by_count(ctx, action);
        case PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY:
            return start_exile_from_graveyard_then_multiply(ctx, action);
        case PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY:
            return start_discard_from_hand_then_multiply(ctx, action);
        case PendingActionType::OVERCLOCK_DISCARD_PROMPT:
            return start_overclock_discard_prompt(ctx, action);
        case PendingActionType::DISCARD_FROM_HAND:
        case PendingActionType::EXILE_FROM_HAND:
        case PendingActionType::PUT_HAND_ON_DECK_TOP:
            return start_hand_card_select(ctx, action);
        case PendingActionType::RETRIEVE_FROM_GRAVEYARD:
        case PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP:
            return start_retrieve_from_graveyard(ctx, action);
        case PendingActionType::GRAVEYARD_PICK_TO_BOTTOM:
        case PendingActionType::GRAVEYARD_PICK_TO_TOP:
            return start_graveyard_pick(ctx, action);
        case PendingActionType::GRAVEYARD_PAIR_SWAP:
            return start_graveyard_pair_swap(ctx, action);
        case PendingActionType::DECK_SEARCH:
            return start_deck_search(ctx, action);
        case PendingActionType::PLAY_DECK_TOP:
            return start_play_deck_top(ctx, action);
        case PendingActionType::PLAY_RANDOM_GRAVEYARD:
            return start_play_random_graveyard(ctx, action);
        case PendingActionType::ROLL_OVER_SUBSTITUTE:
            return start_roll_over_substitute(ctx, action);
        case PendingActionType::MILL_REVEAL:
            return start_mill_reveal(ctx, action);
        case PendingActionType::SCRY:
            return start_scry(ctx, action);
        case PendingActionType::SWAP_TOTAL_SCORE_DIGITS:
            return start_swap_total_score_digits(ctx, action);
        case PendingActionType::MOVE_FOUR_TOTAL_DIGIT:
            return start_move_four_total_digit(ctx, action);
        case PendingActionType::REPLACE_TOTAL_DIGIT_WITH_FIVE:
            return start_replace_total_digit_with_five(ctx, action);
        case PendingActionType::MAJOR_LIFT_TOTAL_DIGIT:
            return start_major_lift_total_digit(ctx, action);
        case PendingActionType::MINOR_FALL_TOTAL_DIGIT:
            return start_minor_fall_total_digit(ctx, action);
        case PendingActionType::BUILD_A_NUMBER_PLACE_DIGIT:
            return start_build_number_place_digit(ctx, action);
        case PendingActionType::POKER_HAND_PLACE_DIGIT:
            return start_poker_hand_place_digit(ctx, action);
        case PendingActionType::EVALUATE_GHOST_STEP:
            return start_evaluate_ghost_step(ctx, action);
        case PendingActionType::PAPER_SWAP_HAND:
            return start_paper_swap_hand(ctx, action);
        case PendingActionType::PEANUT_BUTTER_SCRY:
            return start_peanut_butter_scry(ctx, action);
        case PendingActionType::BIRDS_RETURN:
            return start_birds_return(ctx, action);
        case PendingActionType::RECLAIM_GRAVEYARD:
            return start_reclaim_graveyard(ctx, action);
        case PendingActionType::NECROMANCY_SHUFFLE:
            return start_necromancy_shuffle(ctx, action);
        case PendingActionType::NONE:
        default:
            return PendingStartResult::FIZZLE;
        }
    }

    void finish_pending_queue(GameContext& ctx)
    {
        finalize_held_played_card(ctx.state);

        // combo_try_start_pending (via try_start_pending_combo) starts a match even
        // when COMBO_CINEMATIC is not at queue front; re-scan covers finalize → GY.
        if(ctx.state.pending_combo.length <= 0)
        {
            combo_check_zone(ctx.state, ComboZone::HAND);
            combo_check_zone(ctx.state, ComboZone::GRAVEYARD);
            combo_check_zone(ctx.state, ComboZone::DECK);
        }

        if(ctx.try_start_pending_combo())
        {
            ctx.update_target_scroll();
            return;
        }

        // Echo idle gate: drain when FX / swivel / pending are clear. If armed but
        // not ready, stay in NORMAL and do not end the round on an empty hand.
        if(ctx.state.echo_pending_replay)
        {
            if(ctx.try_drain_echo_replay())
            {
                if(!ctx.state.pending_actions.empty())
                {
                    ctx.begin_next_pending_or_finish();
                    return;
                }

                if(ctx.try_start_pending_combo())
                {
                    ctx.update_target_scroll();
                    return;
                }
            }
            else
            {
                ctx.mode = GameMode::NORMAL;
                ctx.state.selection = SelectionSession{};
                ctx.row_scroll_x = 0;
                ctx.target_row_scroll_x = 0;
                ctx.target_row_scroll_index = 0;

                swivel_clear_wait_if_hand_empty(ctx);

                if(empty_hand_triggers_round_end(ctx.state))
                {
                    ctx.finish_empty_hand_round();
                }

                ctx.sync_hand_selection();
                return;
            }
        }

        ctx.mode = GameMode::NORMAL;
        ctx.state.selection = SelectionSession{};
        ctx.row_scroll_x = 0;
        ctx.target_row_scroll_x = 0;
        ctx.target_row_scroll_index = 0;

        ctx.try_advance_roll_over_sequence();

        if(empty_hand_triggers_round_end(ctx.state))
        {
            swivel_clear_wait_if_hand_empty(ctx);
            ctx.finish_empty_hand_round();
        }

        ctx.sync_hand_selection();
    }
}

void GameContext::begin_next_pending_or_finish(bool close_selection)
{
    while(!state.pending_actions.empty())
    {
        const PendingAction action = state.pending_actions.front();
        state.pending_actions.erase(state.pending_actions.begin());

        const PendingStartResult result = start_pending_action(*this, action);

        if(result == PendingStartResult::ENTERED_MODE)
        {
            return;
        }

        // Draw-then-select chains (Shells) must wait for the card to land in hand.
        if(hand_draw_fx_blocking())
        {
            return;
        }

        if(action.type == PendingActionType::EVALUATE_GHOST_STEP &&
           !state.pending_actions.empty())
        {
            return;
        }
    }

    // Interactive actions are popped when their mode opens. Automatic callers (e.g. play
    // flight completion) must not clear an in-progress pick/scry. Explicit close_selection
    // is used when the player or an effect handler finishes the current selection.
    if(!close_selection && selection_blocks_pending_finish())
    {
        return;
    }

    finish_pending_queue(*this);
}

void GameContext::tick_evaluate_ghost_steps()
{
    if(state.pending_actions.empty() ||
       state.pending_actions.front().type != PendingActionType::EVALUATE_GHOST_STEP)
    {
        return;
    }

    if(hand_draw_fx_blocking())
    {
        return;
    }

    begin_next_pending_or_finish();
}
