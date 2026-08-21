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
            ctx.mode = GameMode::COMBO;
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

        begin_graveyard_target(ctx, action, 0, 0);
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
        ctx.state.selection.cursor = ctx.state.graveyard.size() - 1;
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
            ctx.mode = GameMode::COMBO;
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.mode = GameMode::DECK_SEARCH;
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
            ctx.mode = GameMode::COMBO;
            return PendingStartResult::ENTERED_MODE;
        }

        ctx.mode = GameMode::SCRY;
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

    PendingStartResult start_reclaim_graveyard(GameContext& ctx, const PendingAction&)
    {
        reclaim_graveyard_into_deck(ctx.state);
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
        case PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT:
            return start_exile_graveyard_multiply_by_count(ctx, action);
        case PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY:
            return start_exile_from_graveyard_then_multiply(ctx, action);
        case PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY:
            return start_discard_from_hand_then_multiply(ctx, action);
        case PendingActionType::DISCARD_FROM_HAND:
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
        case PendingActionType::SCRY:
            return start_scry(ctx, action);
        case PendingActionType::SWAP_TOTAL_SCORE_DIGITS:
            return start_swap_total_score_digits(ctx, action);
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
        // combo_try_start_pending (via try_start_pending_combo) starts a match even
        // when COMBO_CINEMATIC is not at queue front; re-scan covers finalize → GY.
        if(ctx.state.pending_combo.length <= 0)
        {
            combo_check_zone(ctx.state, ComboZone::HAND);
            combo_check_zone(ctx.state, ComboZone::GRAVEYARD);
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
            if(ctx.state.hand.empty())
            {
                ctx.state.echo_pending_replay = false;
                ctx.state.echo_replay_card = CardType::COUNT;
                ctx.echo_play_badge_active = false;
            }
            else if(ctx.try_drain_echo_replay())
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

                if(ctx.state.hand.empty())
                {
                    if(ctx.block_round_end_for_combo())
                    {
                        ctx.update_target_scroll();
                        return;
                    }

                    if(ctx.presentation_fx_blocking() || ctx.hand_draw_fx_blocking() || ctx.removing_card)
                    {
                        ctx.round_end_pending = true;
                    }
                    else
                    {
                        ctx.finish_empty_hand_round();
                    }
                }

                ctx.update_target_scroll();
                return;
            }
        }

        ctx.mode = GameMode::NORMAL;
        ctx.state.selection = SelectionSession{};
        ctx.row_scroll_x = 0;
        ctx.target_row_scroll_x = 0;
        ctx.target_row_scroll_index = 0;

        if(ctx.state.hand.size() == 0)
        {
            swivel_clear_wait_if_hand_empty(ctx);

            if(ctx.block_round_end_for_combo())
            {
                return;
            }

            if(ctx.presentation_fx_blocking() || ctx.hand_draw_fx_blocking() || ctx.removing_card)
            {
                ctx.round_end_pending = true;
                ctx.update_target_scroll();
                return;
            }

            ctx.finish_empty_hand_round();
        }

        ctx.update_target_scroll();
    }
}

void GameContext::begin_next_pending_or_finish()
{
    if(hand_draw_fx_blocking() || removing_card)
    {
        return;
    }

    while(!state.pending_actions.empty())
    {
        const PendingAction action = state.pending_actions.front();
        state.pending_actions.erase(state.pending_actions.begin());

        const PendingStartResult result = start_pending_action(*this, action);

        if(result == PendingStartResult::ENTERED_MODE)
        {
            return;
        }
    }

    finish_pending_queue(*this);
}
