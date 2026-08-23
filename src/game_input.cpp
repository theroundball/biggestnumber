#include "game_context.h"

#include "bn_keypad.h"
#include "bn_utility.h"

#include "combo_system.h"
#include "game_events.h"
#include "game_helpers.h"
#include "game_ui.h"
#include "play_resolution.h"
#include "score_swap_system.h"
#include "swivel_system.h"
#include "trinket_system.h"
#include "ui_inspect.h"
#include "ui_common.h"

namespace
{
    bool side_panel_escape_pressed()
    {
        return bn::keypad::a_pressed() || bn::keypad::b_pressed();
    }

    void nudge_cursor(int& cursor, int direction, int steps, int min_index, int max_index)
    {
        if(direction == 0 || steps <= 0 || max_index < min_index)
        {
            return;
        }

        cursor += direction * steps;

        if(cursor < min_index)
        {
            cursor = min_index;
        }
        else if(cursor > max_index)
        {
            cursor = max_index;
        }
    }
}

void GameContext::tick_combo()
{
    if(mode != GameMode::COMBO)
    {
        return;
    }

    switch(combo_focus)
    {
    case ComboFocusPhase::PAN_IN:
        if(panel_transition_active())
        {
            return;
        }

        combo_focus = ComboFocusPhase::REVEAL;
        combo_focus_frame = 0;
        return;

    case ComboFocusPhase::REVEAL:
        ++combo_focus_frame;

        if(combo_focus_frame < COMBO_FOCUS_REVEAL_FRAMES)
        {
            return;
        }

        combo_focus = ComboFocusPhase::PLAYING;
        combo_focus_anchor_x = graveyard_panel_offset_x();
        break;

    case ComboFocusPhase::PAN_OUT:
        if(panel_transition_active())
        {
            return;
        }

        combo_focus = ComboFocusPhase::NONE;
        combo_focus_panel_opened = false;
        resume_after_combo();
        return;

    default:
        break;
    }

    if(!state.combo_cinematic.active)
    {
        return;
    }

    ++state.combo_cinematic.frame;

    if(state.combo_cinematic.frame == COMBO_GATHER_FRAMES)
    {
        combo_apply_score_bonus(state);
        draw_round_score();
        // Slide back now so the cards land on a score that is on screen again.
        begin_combo_focus_return();
    }

    if(state.combo_cinematic.frame >= COMBO_TOTAL_FRAMES)
    {
        finish_combo_cinematic();
    }
}

bool GameContext::poll_direction(int& current_direction, bool scrolling, bool& direction_triggered,
                                 int& direction_steps)
{
    return poll_direction_repeat(DirectionAxis::HORIZONTAL, direction_repeat, scrolling,
                                 current_direction, direction_triggered, direction_steps);
}

void GameContext::handle_input()
{
    if(!confirm_input_armed)
    {
        if(!bn::keypad::a_held() && !bn::keypad::up_held())
        {
            confirm_input_armed = true;
        }
    }

    const bool row_scrolling = !inspecting && row_scroll_x != target_row_scroll_x;
    const bool scrolling = !inspecting && (scroll_x != target_scroll_x || row_scrolling);
    int current_direction = 0;
    bool direction_triggered = false;
    int direction_steps = 1;

    if(mode == GameMode::SCORE_SWAP)
    {
        score_swap_handle_input(*this);
        return;
    }

    poll_direction(current_direction, scrolling, direction_triggered, direction_steps);

    handle_input_inspect_toggle();

    // A graveyard combo holds the browse panel open while it plays: swallow panel input
    // so the player cannot scroll or close the view mid-cinematic.
    if(mode == GameMode::COMBO)
    {
        handle_input_presentation();
    }
    else if(!panel_transition_active() && side_panel != SidePanel::NONE)
    {
        handle_input_side_panel(current_direction, direction_triggered, direction_steps, row_scrolling);
    }
    else if(handle_input_presentation())
    {
        return;
    }
    else
    {
        switch(mode)
        {
            case GameMode::NORMAL:
                handle_input_normal(current_direction, direction_triggered, direction_steps, scrolling);
                break;

            case GameMode::DISCARD_TARGET:
                handle_input_discard_target(current_direction, direction_triggered, direction_steps,
                                            scrolling);
                break;

            case GameMode::GRAVEYARD_TARGET:
                handle_input_graveyard_target(current_direction, direction_triggered, direction_steps,
                                              row_scrolling);
                break;

            case GameMode::DECK_SEARCH:
                handle_input_deck_search(current_direction, direction_triggered, direction_steps,
                                         row_scrolling);
                break;

            case GameMode::GRAVEYARD_PICK:
                handle_input_graveyard_pick(current_direction, direction_triggered, direction_steps,
                                            row_scrolling);
                break;

            case GameMode::SCRY:
                handle_input_scry(current_direction, direction_triggered, direction_steps, row_scrolling);
                break;

            case GameMode::COMBO:
            default:
                break;
        }
    }
}

void GameContext::handle_input_inspect_toggle()
{
    const bool inspect_toggle_pressed = bn::keypad::select_pressed();

    // Select / Down toggles the inspect view for the highlighted card (in any card
    // view: hand, discard target, or graveyard picker).
    const bool removal_blocks_inspect =
        removing_card && removal_phase != PlayRemovalPhase::WAIT_PRESENTATION;

    if (inspect_toggle_pressed && !removal_blocks_inspect && !graveyard_card_fx_active &&
        !hand_draw_fx_blocking() &&
        !swapping_card && !game_over &&
        mode != GameMode::COMBO && side_panel != SidePanel::DETAILS &&
        !panel_transition_active())
    {
        if (inspecting)
        {
            clear_inspect();
            snap_active_scroll();
        }
        else if (current_highlight_type() >= 0)
        {
            inspecting = true;
            inspect_shown_index = -1; // force a draw in the refresh step below
        }
    }
}

void GameContext::handle_input_side_panel(int current_direction, bool direction_triggered,
                                          int direction_steps, bool row_scrolling)
{
    if(bn::keypad::l_pressed())
    {
        cycle_side_panel(1);
        return;
    }

    if(bn::keypad::r_pressed())
    {
        cycle_side_panel(-1);
        return;
    }

    if(side_panel_escape_pressed())
    {
        if(inspecting)
        {
            clear_inspect();
            snap_active_scroll();
        }

        if(side_panel == SidePanel::DETAILS)
        {
            begin_panel_transition(PanelTransition::CLOSE_DETAILS);
        }
        else if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
        {
            begin_panel_transition(PanelTransition::CLOSE_GRAVEYARD);
        }

        return;
    }

    if((side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE) &&
       mode == GameMode::NORMAL && !game_over && !row_scrolling && direction_triggered)
    {
        const bn::span<const CardRef> cards = active_browse_cards();

        if(!cards.empty())
        {
            nudge_cursor(browse_cursor, current_direction, direction_steps, 0, cards.size() - 1);
            sync_target_row_scroll(browse_cursor, cards.size());
        }
    }
}

bool GameContext::handle_input_presentation()
{
    if(hand_draw_fx_blocking())
    {
        return true;
    }

    if(graveyard_card_fx_active)
    {
        ++graveyard_card_fx_frame;

        if(graveyard_card_fx_frame >= graveyard_card_fx_frame_count())
        {
            complete_graveyard_card_fx();
        }

        return true;
    }

    if(removing_card)
    {
        // Removal timing is driven from the main loop so animations finish even when
        // handle_input is skipped (lucky sevens, deck-search resolve, etc.).
        if(removal_phase == PlayRemovalPhase::WAIT_PRESENTATION && mode == GameMode::NORMAL)
        {
            return false;
        }

        return true;
    }

    if(swapping_card)
    {
        ++swap_frame;

        if (swap_frame >= game_layout::SWAP_FRAMES)
        {
            if(mode == GameMode::SCRY)
            {
                const int cursor = state.selection.cursor;
                bn::swap(state.selection.scry_buffer[cursor],
                         state.selection.scry_buffer[cursor + swap_direction]);
                bn::swap(card_raise_offset[cursor], card_raise_offset[cursor + swap_direction]);
            }
            else
            {
                swap_cards(state, selected_card, selected_card + swap_direction);
                bn::swap(card_raise_offset[selected_card], card_raise_offset[selected_card + swap_direction]);
            }

            const bool still_held = bn::keypad::b_held() &&
                                    (swap_direction == 1 ? bn::keypad::right_held() : bn::keypad::left_held());

            if (swap_first_step && !still_held)
            {
                swapping_card = false;
                swap_direction = 0;

                if(mode == GameMode::SCRY)
                {
                    combo_check_zone(state, ComboZone::REVEALED);

                    if(try_start_pending_combo())
                    {
                        draw_round_score();
                    }
                }
                else if(try_start_pending_combo())
                {
                    draw_round_score();
                }
                else
                {
                    draw_total_score();
                }
            }
            else
            {
                if(mode == GameMode::SCRY)
                {
                    state.selection.cursor += swap_direction;
                    sync_target_row_scroll(state.selection.cursor, state.selection.scry_buffer.size());
                }
                else
                {
                    selected_card += swap_direction;
                    update_target_scroll();
                }

                swap_first_step = false;

                const bool can_continue = mode == GameMode::SCRY
                    ? (swap_direction == 1
                        ? state.selection.cursor < state.selection.scry_buffer.size() - 1
                        : state.selection.cursor > 0)
                    : (swap_direction == 1 ? selected_card < state.hand.size() - 1 : selected_card > 0);

                if (still_held && can_continue)
                {
                    swap_frame = 0;
                }
                else
                {
                    swapping_card = false;
                    swap_direction = 0;

                    if(mode == GameMode::SCRY)
                    {
                        combo_check_zone(state, ComboZone::REVEALED);

                        if(try_start_pending_combo())
                        {
                            draw_round_score();
                        }
                    }
                    else if(try_start_pending_combo())
                    {
                        draw_round_score();
                    }
                    else
                    {
                        draw_total_score();
                    }
                }
            }
        }

        return true;
    }

    return false;
}

void GameContext::handle_input_normal(int current_direction, bool direction_triggered, int direction_steps,
                                      bool scrolling)
{
    const bool score_wait_locks_hand_actions =
        removing_card && removal_phase == PlayRemovalPhase::WAIT_PRESENTATION;
    const int slot_count = playable_slot_count(state);
    const bool live_selected = selected_card >= 0 && selected_card < state.hand.size();
    const bool flashback_selected = selected_card >= state.hand.size() && selected_card < slot_count;

    if(!game_over && !scrolling && !inspecting &&
       side_panel == SidePanel::NONE && !panel_transition_active() && state.hand.size() && bn::keypad::b_held() &&
       !score_wait_locks_hand_actions &&
       bn::keypad::right_pressed() && live_selected && selected_card < state.hand.size() - 1)
    {
        swapping_card = true;
        swap_frame = 0;
        swap_direction = 1;
        swap_first_step = true; // new
    }
    else if(!game_over && !scrolling && !inspecting &&
            side_panel == SidePanel::NONE && !panel_transition_active() && state.hand.size() && bn::keypad::b_held() &&
            !score_wait_locks_hand_actions &&
            bn::keypad::left_pressed() && live_selected && selected_card > 0)
    {
        swapping_card = true;
        swap_frame = 0;
        swap_direction = -1;
        swap_first_step = true;
    }
    else if(!game_over && !scrolling && !inspecting &&
            side_panel == SidePanel::NONE && !panel_transition_active() && bn::keypad::l_pressed())
    {
        cycle_side_panel(1);
    }
    else if(!game_over && !scrolling && !inspecting &&
            side_panel == SidePanel::NONE && !panel_transition_active() && bn::keypad::r_pressed())
    {
        cycle_side_panel(-1);
    }
    else if(!game_over && !scrolling && side_panel == SidePanel::NONE &&
            !panel_transition_active() && slot_count && !bn::keypad::b_held() && direction_triggered)
    {
        nudge_cursor(selected_card, current_direction, direction_steps, 0, slot_count - 1);
        update_target_scroll();
    }
    else if(!game_over && !scrolling && !inspecting &&
            side_panel == SidePanel::NONE && !panel_transition_active() && live_selected &&
            !score_wait_locks_hand_actions && bn::keypad::down_pressed() &&
            card_has_cycle(state.hand[selected_card].type))
    {
        removal_cycle_draw = true;

        if(card_has_discard_effect(state.hand[selected_card].type))
        {
            begin_discard_presentation(selected_card);
        }
        else
        {
            capture_removal_start();
            begin_direct_removal(removal_start_x, removal_start_y, RemovalStyle::TO_GRAVEYARD, true);
        }
    }
    else if(!game_over && !scrolling && !inspecting &&
            side_panel == SidePanel::NONE && !panel_transition_active() && slot_count &&
            !score_wait_locks_hand_actions && confirm_pressed())
    {
        if(inspecting)
        {
            clear_inspect();
        }
        else if(flashback_selected)
        {
            const int gy_index = playable_slot_graveyard_index(state, selected_card);
            const CardRef played_ref = playable_slot_card(state, selected_card);

            if(gy_index < 0)
            {
                return;
            }

            const int main_x = main_panel_offset_x();
            PlayResolutionContext context;
            context.source = PlaySource::FLASHBACK;
            context.hand_index = gy_index;
            context.selected_card = &selected_card;
            context.apply_destination = false;

            echo_play_badge_active = state.echo_first_play_active() &&
                                     card_has_play_effect(state, played_ref);
            begin_play_presentation(
                played_ref,
                card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x),
                card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y),
                PlayPresentOrigin::GRAVEYARD, context, RemovalStyle::EXILE_DISSIPATE);
        }
        else if(swivel_is_waiting(*this) && live_selected)
        {
            const CardRef played_ref = state.hand[selected_card];
            capture_removal_start();

            PlayResolutionContext context;
            context.source = PlaySource::HAND;
            context.hand_index = selected_card;
            context.selected_card = &selected_card;
            context.apply_destination = false;

            removal_swivel_follow = true;
            state.swivel_waiting = false;
            echo_play_badge_active = state.echo_first_play_active() &&
                                     card_has_play_effect(state, played_ref);
            begin_play_presentation(played_ref, removal_start_x, removal_start_y, PlayPresentOrigin::HAND,
                                  context, RemovalStyle::TO_DECK_TOP);
        }
        else if(live_selected)
        {
            const CardRef played_ref = state.hand[selected_card];
            echo_play_badge_active = state.echo_first_play_active() &&
                                     card_has_play_effect(state, played_ref);
            capture_removal_start();

            PlayResolutionContext context;
            context.source = PlaySource::HAND;
            context.hand_index = selected_card;
            context.selected_card = &selected_card;
            context.apply_destination = false;
            const RemovalStyle style = removal_style_for_hand_play(played_ref.type);

            if(played_ref.type == CardType::SWIVEL)
            {
                removal_style = style;
                removal_is_discard = false;
                removal_swivel_follow = false;
                begin_direct_removal(removal_start_x, removal_start_y, style, false);
            }
            else if(card_has_play_effect(state, played_ref))
            {
                begin_play_presentation(played_ref, removal_start_x, removal_start_y,
                                        PlayPresentOrigin::HAND, context, style);
            }
            else
            {
                removal_style = style;
                removal_is_discard = false;
                begin_direct_removal(removal_start_x, removal_start_y, style, false);
            }
        }
    }
}

void GameContext::handle_input_discard_target(int current_direction, bool direction_triggered,
                                              int direction_steps, bool scrolling)
{
    // --- DISCARD_TARGET: pick a hand card to discard (Up) ---
    if(!scrolling && direction_triggered)
    {
        nudge_cursor(selected_card, current_direction, direction_steps, 0, state.hand.size() - 1);
        update_target_scroll();
    }
    else if(!scrolling && !inspecting && state.hand.size() &&
            (confirm_pressed() || bn::keypad::down_pressed()))
    {
        if(state.selection.type == PendingActionType::PUT_HAND_ON_DECK_TOP)
        {
            capture_removal_start();
            begin_direct_removal(removal_start_x, removal_start_y, RemovalStyle::TO_DECK_TOP, false);
        }
        else if(card_has_discard_effect(state.hand[selected_card].type))
        {
            begin_discard_presentation(selected_card);
        }
        else
        {
            capture_removal_start();
            begin_direct_removal(removal_start_x, removal_start_y, RemovalStyle::TO_GRAVEYARD, true);
        }
    }
}

void GameContext::handle_input_graveyard_target(int current_direction, bool direction_triggered,
                                                int direction_steps, bool row_scrolling)
{
    // --- GRAVEYARD_TARGET: pick a graveyard card to return to hand (Up) ---
    if(!row_scrolling && !graveyard_card_fx_active && direction_triggered && current_direction != 0)
    {
        for(int step = 0; step < direction_steps; ++step)
        {
            state.selection.cursor = advance_graveyard_cursor(
                state, state.selection.cursor, current_direction, state.selection.graveyard_exclude);
        }

        sync_target_row_scroll(state.selection.cursor, state.graveyard.size());
    }
    else if(!inspecting &&
            state.selection.type == PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY &&
            bn::keypad::b_pressed())
    {
        begin_next_pending_or_finish();
    }
    else if(!inspecting &&
            state.selection.type == PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT &&
            bn::keypad::b_pressed())
    {
        if(state.selection.exiled_count > 0)
        {
            state.mul_from_card(state.selection.exiled_count);
            draw_round_score();
        }

        begin_next_pending_or_finish();
    }
    else if(!inspecting && !graveyard_card_fx_active &&
            !state.graveyard.empty() && confirm_pressed())
    {
        state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
        const CardRef card = state.graveyard[state.selection.cursor];
        const CardType type = card.type;

        if(state.selection.type == PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT)
        {
            begin_graveyard_card_fx(GraveyardExilePickKind::MULTIPLY_BY_COUNT);
        }
        else if(state.selection.type == PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY)
        {
            begin_graveyard_card_fx(GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY);
        }
        else if(state.selection.type == PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP)
        {
            begin_graveyard_card_fx(GraveyardExilePickKind::TO_DECK_TOP);
        }
        else if(state.selection.type == PendingActionType::RETRIEVE_FROM_GRAVEYARD)
        {
            begin_graveyard_card_fx(GraveyardExilePickKind::TO_HAND);
        }
        else if(state.selection.type == PendingActionType::GRAVEYARD_PAIR_SWAP)
        {
            const int cursor = state.selection.cursor;

            if(state.selection.graveyard_swap_first < 0)
            {
                state.selection.graveyard_swap_first = cursor;
            }
            else if(cursor == state.selection.graveyard_swap_first)
            {
                // Confirm again on the locked card to unlock it.
                state.selection.graveyard_swap_first = -1;
            }
            else
            {
                graveyard_swap_at(state, state.selection.graveyard_swap_first, cursor);
                state.selection.graveyard_swap_first = -1;
                --state.selection.remaining_picks;

                if(try_start_pending_combo())
                {
                    action_prompt_sprites.clear();
                    return;
                }

                if(state.selection.remaining_picks <= 0)
                {
                    action_prompt_sprites.clear();
                    begin_next_pending_or_finish();
                }
            }
        }
        else if(type != state.selection.graveyard_exclude)
        {
            begin_next_pending_or_finish();
        }
    }
}

void GameContext::handle_input_deck_search(int current_direction, bool direction_triggered,
                                           int direction_steps, bool row_scrolling)
{
    // --- DECK_SEARCH: pick a deck card to play immediately (Up) ---
    if(!row_scrolling && direction_triggered)
    {
        nudge_cursor(state.selection.cursor, current_direction, direction_steps, 0,
                     state.selection.deck_search_buffer.size() - 1);
        sync_target_row_scroll(state.selection.cursor, state.selection.deck_search_buffer.size());
    }
    else if(!inspecting &&
            !state.selection.deck_search_buffer.empty() && confirm_pressed())
    {
        const bool swivel_follow = swivel_is_waiting(*this);
        const int cursor = state.selection.cursor;
        const int buffer_size = state.selection.deck_search_buffer.size();
        const int main_x = main_panel_offset_x();
        int pick_x = 0;
        int pick_y = 0;

        const int cursor_raise = cursor < card_raise_offset.size() ? card_raise_offset[cursor] : 0;

        graveyard_cursor_screen_position(cursor, buffer_size, game_layout::GRAVE_SPACING,
                                         game_layout::GRAVE_Y, cursor_raise,
                                         row_scroll_x, main_x, pick_x, pick_y);

        const PreparedDeckPlay prepared = deck_search_prepare_play(state);

        if(prepared.card.type == CardType::COUNT)
        {
            begin_next_pending_or_finish();
            return;
        }

        removal_swivel_follow = swivel_follow;

        if(swivel_follow)
        {
            state.swivel_waiting = false;
        }

        PlayResolutionContext context;
        context.source = PlaySource::DECK_SEARCH;
        context.apply_destination = false;

        const RemovalStyle style = removal_style_for_hand_play(prepared.card.type);
        const RemovalStyle resolved_style = swivel_follow ? RemovalStyle::TO_DECK_TOP : style;

        begin_play_presentation(
            prepared.card, pick_x, pick_y, PlayPresentOrigin::DECK_SEARCH, context, resolved_style,
            prepared.miracle_from_top);

        state.selection = SelectionSession{};
        mode = GameMode::NORMAL;
        row_scroll_x = 0;
        target_row_scroll_x = 0;
        target_row_scroll_index = 0;
    }
}

void GameContext::handle_input_graveyard_pick(int current_direction, bool direction_triggered,
                                              int direction_steps, bool row_scrolling)
{
    // --- GRAVEYARD_PICK: pick up to N graveyard cards, in order, to the bottom of the deck ---
    if(!row_scrolling && direction_triggered)
    {
        nudge_cursor(state.selection.cursor, current_direction, direction_steps, 0,
                     state.graveyard.size() - 1);
        sync_target_row_scroll(state.selection.cursor, state.graveyard.size());
    }
    else if(!inspecting && !state.graveyard.empty() && confirm_pressed())
    {
        const CardRef picked = state.graveyard[state.selection.cursor];
        graveyard_remove_at(state, state.selection.cursor);
        state.selection.picked_ordered.push_back(picked);
        --state.selection.remaining_picks;

        if (state.selection.cursor >= state.graveyard.size() && state.selection.cursor > 0)
        {
            --state.selection.cursor;
        }

        sync_target_row_scroll(state.selection.cursor, state.graveyard.size());

        if(state.selection.remaining_picks > 0 && !state.graveyard.empty())
        {
            if(try_start_pending_combo())
            {
                return;
            }
        }

        if (state.selection.remaining_picks <= 0 || state.graveyard.empty())
        {
            if(state.selection.type == PendingActionType::GRAVEYARD_PICK_TO_TOP)
            {
                for(CardRef card : state.selection.picked_ordered)
                {
                    apply_card_relocated(state, card.type);
                    state.deck.insert_top(card);
                }
            }
            else
            {
                // Replay in pick order: add_card appends deepest-last, so this gives
                // "first picked = shallowest of the batch, last picked = truly bottom."
                for(CardRef card : state.selection.picked_ordered)
                {
                    apply_card_relocated(state, card.type);
                    state.deck.add_card(card);
                }
            }

            draw_round_score();

            state.selection.picked_ordered.clear();
            begin_next_pending_or_finish();
        }
    }
}

void GameContext::handle_input_scry(int current_direction, bool direction_triggered, int direction_steps,
                                    bool row_scrolling)
{
    // --- SCRY: reorder peeked cards (B+Left/Right), play one (Up), rest return to deck ---
    if(!row_scrolling && !inspecting && bn::keypad::b_held() &&
       bn::keypad::right_pressed() && state.selection.cursor < state.selection.scry_buffer.size() - 1)
    {
        swapping_card = true;
        swap_frame = 0;
        swap_direction = 1;
        swap_first_step = true;
    }
    else if(!row_scrolling && !inspecting && bn::keypad::b_held() &&
            bn::keypad::left_pressed() && state.selection.cursor > 0)
    {
        swapping_card = true;
        swap_frame = 0;
        swap_direction = -1;
        swap_first_step = true;
    }
    else if(!row_scrolling && !swapping_card && !bn::keypad::b_held() && direction_triggered)
    {
        nudge_cursor(state.selection.cursor, current_direction, direction_steps, 0,
                     state.selection.scry_buffer.size() - 1);
        sync_target_row_scroll(state.selection.cursor, state.selection.scry_buffer.size());
    }
    else if(!inspecting && !swapping_card && !state.selection.scry_buffer.empty() && confirm_pressed())
    {
        const bool swivel_follow = swivel_is_waiting(*this);
        const int scry_cursor = state.selection.cursor;
        const int scry_count = state.selection.scry_buffer.size();
        const int main_x = main_panel_offset_x();
        int pick_x = main_x;
        int pick_y = game_layout::SCRY_Y;

        if(scry_count > 0)
        {
            const int cursor_raise = scry_cursor < card_raise_offset.size()
                ? card_raise_offset[scry_cursor] : 0;

            graveyard_cursor_screen_position(scry_cursor, scry_count, game_layout::SCRY_SPACING,
                                             game_layout::SCRY_Y, cursor_raise,
                                             row_scroll_x, main_x, pick_x, pick_y);
        }

        const PreparedDeckPlay prepared = scry_prepare_play(state);
        removal_swivel_follow = swivel_follow;

        if(swivel_follow)
        {
            state.swivel_waiting = false;
        }

        PlayResolutionContext context;
        context.source = PlaySource::SCRY;
        context.apply_destination = false;

        const RemovalStyle style = removal_style_for_hand_play(prepared.card.type);
        const RemovalStyle resolved_style = swivel_follow ? RemovalStyle::TO_DECK_TOP : style;

        begin_play_presentation(
            prepared.card, pick_x, pick_y, PlayPresentOrigin::SCRY, context, resolved_style,
            prepared.miracle_from_top);

        state.selection = SelectionSession{};
        mode = GameMode::NORMAL;
        row_scroll_x = 0;
        target_row_scroll_x = 0;
        target_row_scroll_index = 0;
    }
}

void GameContext::sync_inspect_panel()
{
    if(inspecting)
    {
        const int highlight = current_highlight_type();
        const int index = current_highlight_index();

        if(highlight < 0)
        {
            clear_inspect();
            snap_active_scroll();
        }
        else if(index != inspect_shown_index)
        {
            snap_active_scroll();
            draw_inspect(CardType(highlight));
            inspect_shown_index = index;
        }
    }
}
