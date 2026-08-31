#include "game_context.h"

#include "bn_blending.h"
#include "card_data.h"
#include "combo_system.h"
#include "game_events.h"
#include "game_helpers.h"
#include "game_ui.h"

namespace
{
    bool is_gy_transit_kind(TransitKind kind)
    {
        return kind == TransitKind::GY_TO_HAND || kind == TransitKind::GY_TO_DECK ||
               kind == TransitKind::GY_BIRDS_TO_DECK || kind == TransitKind::GY_EXILE;
    }

    void setup_transit_card_visual(Card& fx_card, CardRef card, const GameContext& ctx,
                                   TransitKind kind, bn::sprite_text_generator& text_gen)
    {
        fx_card.set_type(card.type);

        const CardInstance* instance =
            card.has_instance() ? instance_at(ctx.state.instance_pool, card.instance_id) : nullptr;

        if(card_data(card.type).text_only)
        {
            fx_card.sync_face_labels(&text_gen, &ctx.state, card, instance, is_gy_transit_kind(kind));
        }
        else
        {
            fx_card.clear_face_labels();
        }

        if(card.has_instance())
        {
            fx_card.set_upgrade_pips(&text_gen, instance);
        }
        else
        {
            fx_card.clear_upgrade_pips();
        }
    }

    int gy_transit_stagger_slot(const GameContext& ctx)
    {
        int count = 0;

        for(const TransitFlight& flight : ctx.transit_flights)
        {
            if(flight.active && is_gy_transit_kind(flight.kind))
            {
                ++count;
            }
        }

        return count;
    }
}

void GameContext::prepare_transit_flight_visual(TransitFlight& flight)
{
    setup_transit_card_visual(flight.fx_card, flight.card, *this, flight.kind, hud_count_generator);
}

void GameContext::clear_transit_flight(TransitFlight& flight)
{
    release_card_display_tiles(flight.fx_card);
    flight = TransitFlight{};
}

TransitFlight* GameContext::alloc_transit_flight()
{
    for(TransitFlight& flight : transit_flights)
    {
        if(!flight.active)
        {
            return &flight;
        }
    }

    return nullptr;
}

int GameContext::active_transit_count() const
{
    int count = 0;

    for(const TransitFlight& flight : transit_flights)
    {
        if(flight.active)
        {
            ++count;
        }
    }

    return count;
}

int GameContext::in_flight_deck_draw_count() const
{
    int count = 0;

    for(const TransitFlight& flight : transit_flights)
    {
        if(flight.active && flight.kind == TransitKind::DECK_TO_HAND)
        {
            ++count;
        }
    }

    return count;
}

void GameContext::sync_transit_flags()
{
    hand_draw_fx_active = in_flight_deck_draw_count() > 0;
    graveyard_card_fx_active = zone_transit_active();
}

bool GameContext::zone_transit_active() const
{
    for(const TransitFlight& flight : transit_flights)
    {
        if(flight.active && is_gy_transit_kind(flight.kind))
        {
            return true;
        }
    }

    return false;
}

bool GameContext::graveyard_slot_hidden_by_transit(int graveyard_index) const
{
    if(graveyard_slot_hidden_by_flight(graveyard_index))
    {
        return true;
    }

    for(const TransitFlight& flight : transit_flights)
    {
        if(!flight.active || flight.graveyard_index != graveyard_index)
        {
            continue;
        }

        if(is_gy_transit_kind(flight.kind))
        {
            return true;
        }
    }

    return false;
}

int GameContext::graveyard_card_fx_frame_count(const TransitFlight& flight) const
{
    switch(flight.gy_kind)
    {
    case GraveyardExilePickKind::TO_HAND:
    case GraveyardExilePickKind::TO_DECK_TOP:
    case GraveyardExilePickKind::SHUFFLE_TO_DECK:
    case GraveyardExilePickKind::BIRDS_TO_DECK:
        return game_layout::ZONE_TRANSFER_FRAMES;

    case GraveyardExilePickKind::MULTIPLY_BY_COUNT:
    case GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY:
    case GraveyardExilePickKind::NONE:
    default:
        return game_layout::REMOVAL_FRAMES;
    }
}

void GameContext::try_start_pending_transits()
{
    if(deck_search_resolve_active() || lucky_sevens_fx.active)
    {
        return;
    }

    while(!state.pending_hand_draws.empty())
    {
        if(active_transit_count() >= game_layout::MAX_TRANSIT_FLIGHTS)
        {
            break;
        }

        const int deck_slot = in_flight_deck_draw_count();
        const int layout_count = hand_scheduled_count(state, deck_slot);
        TransitFlight* flight = alloc_transit_flight();

        if(!flight)
        {
            break;
        }

        const PendingHandDraw pending = state.pending_hand_draws.front();
        state.pending_hand_draws.erase(state.pending_hand_draws.begin());

        const int main_x = main_panel_offset_x();
        flight->active = true;
        flight->frame = 0;
        flight->delay_frames = deck_slot * game_layout::DECK_DRAW_STAGGER_FRAMES;
        flight->kind = TransitKind::DECK_TO_HAND;
        flight->card = pending.card;
        flight->miracle_auto_play = pending.miracle_auto_play;
        flight->dest_hand_index = state.hand.size() + deck_slot;
        flight->start_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
        flight->start_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
        hand_slot_screen_position(flight->dest_hand_index, main_x, flight->dest_x, flight->dest_y,
                                  layout_count);
        prepare_transit_flight_visual(*flight);
    }

    while(!pending_graveyard_exile_fx.empty())
    {
        if(active_transit_count() >= game_layout::MAX_TRANSIT_FLIGHTS)
        {
            break;
        }

        const int gy_slot = gy_transit_stagger_slot(*this);
        TransitFlight* flight = alloc_transit_flight();

        if(!flight)
        {
            break;
        }

        const PendingGraveyardExileFx pending = pending_graveyard_exile_fx.front();
        pending_graveyard_exile_fx.erase(pending_graveyard_exile_fx.begin());

        const int main_x = main_panel_offset_x();
        flight->active = true;
        flight->frame = 0;
        flight->delay_frames = gy_slot * game_layout::TRANSIT_STAGGER_FRAMES;
        flight->card = pending.card;
        flight->start_x = pending.start_x;
        flight->start_y = pending.start_y;
        flight->graveyard_index = -1;
        flight->gy_kind = pending.kind;
        flight->state_applied = true;

        if(pending.kind == GraveyardExilePickKind::TO_HAND)
        {
            flight->kind = TransitKind::GY_TO_HAND;
            flight->style = RemovalStyle::TO_GRAVEYARD;
            hand_slot_screen_position(state.hand.size(), main_x, flight->dest_x, flight->dest_y);
        }
        else if(pending.kind == GraveyardExilePickKind::TO_DECK_TOP ||
                pending.kind == GraveyardExilePickKind::SHUFFLE_TO_DECK)
        {
            flight->kind = TransitKind::GY_TO_DECK;
            flight->style = RemovalStyle::TO_DECK_TOP;
            flight->dest_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
            flight->dest_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
        }
        else
        {
            flight->kind = TransitKind::GY_EXILE;
            flight->style = RemovalStyle::EXILE_DISSIPATE;
            flight->dest_x = card_target_x_for_score_center(main_x);
            flight->dest_y = card_target_y_for_score_center();
        }

        prepare_transit_flight_visual(*flight);
    }

    sync_transit_flags();
}

void GameContext::complete_deck_to_hand_transit(TransitFlight& flight)
{
    release_card_display_tiles(flight.fx_card);

    if(state.hand.full())
    {
        clear_transit_flight(flight);
        return;
    }

    const bool miracle_auto_play =
        flight.miracle_auto_play && flight.card.type == CardType::MIRACLE;

    bind_bounty_copy(state, flight.card);
    state.hand.push_back(flight.card);
    battle_stat_record_draw_to_hand(state);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);

    if(state.hand.size() == 1)
    {
        selected_card = 0;
        update_target_scroll();
    }

    if(miracle_auto_play)
    {
        const int miracle_index = state.hand.size() - 1;
        play_miracle_bonus(state, 10);
        hand_remove_at_to_graveyard(state, miracle_index, selected_card);
        draw_round_score();
    }

    clear_transit_flight(flight);
    try_start_pending_transits();

    if(!hand_draw_fx_blocking())
    {
        continue_effect_draw_batch();

        if(!hand_draw_fx_blocking())
        {
            begin_next_pending_or_finish(true);
        }

        end_run_if_needed();

        if(!run_finished)
        {
            state.waive_optional_ghost_plays = false;
        }
    }
}

void GameContext::complete_graveyard_transit(TransitFlight& flight)
{
    const GraveyardExilePickKind kind = flight.gy_kind;
    const bool state_applied = flight.state_applied;
    const int graveyard_index = flight.graveyard_index;
    release_card_display_tiles(flight.fx_card);
    clear_transit_flight(flight);
    sync_transit_flags();

    if(state_applied)
    {
        try_start_pending_transits();

        if(rags_exile_deferred_finish && !zone_transit_active() && pending_graveyard_exile_fx.empty())
        {
            rags_exile_deferred_finish = false;
            begin_next_pending_or_finish(true);
        }

        return;
    }

    if(kind == GraveyardExilePickKind::SHUFFLE_TO_DECK)
    {
        necromancy_shuffle_graveyard_to_deck(state);
        begin_next_pending_or_finish(true);
        return;
    }

    if(graveyard_index < 0 || graveyard_index >= state.graveyard.size())
    {
        if(keep_going_transfer_active)
        {
            --keep_going_transfers_remaining;

            if(keep_going_transfers_remaining > 0 && !state.graveyard.empty())
            {
                try_begin_keep_going_transfer();
            }
            else if(keep_going_transfers_remaining <= 0 && !zone_transit_active())
            {
                keep_going_transfer_active = false;
                state.deck.apply_gravity(state.instance_pool);
                finish_deferred_round_start();
            }
        }

        begin_next_pending_or_finish(true);
        return;
    }

    const CardRef card = state.graveyard[graveyard_index];
    graveyard_remove_at(state, graveyard_index);

    if(kind == GraveyardExilePickKind::TO_HAND)
    {
        apply_card_relocated(state, card.type);

        if(!state.hand.full())
        {
            hand_add_card(state, card);
        }

        draw_round_score();
        begin_next_pending_or_finish(true);
        return;
    }

    if(kind == GraveyardExilePickKind::TO_DECK_TOP || kind == GraveyardExilePickKind::BIRDS_TO_DECK)
    {
        apply_card_relocated(state, card.type);

        if(kind == GraveyardExilePickKind::BIRDS_TO_DECK ||
           state.selection.type == PendingActionType::BIRDS_RETURN)
        {
            state.deck.add_card(card);
        }
        else
        {
            state.deck.insert_top(card);
        }

        draw_round_score();

        if(keep_going_transfer_active)
        {
            --keep_going_transfers_remaining;

            if(keep_going_transfers_remaining > 0 && !state.graveyard.empty())
            {
                try_begin_keep_going_transfer();
            }

            if(keep_going_transfers_remaining <= 0 && !zone_transit_active())
            {
                keep_going_transfer_active = false;
                state.deck.apply_gravity(state.instance_pool);
                finish_deferred_round_start();
            }

            return;
        }

        if(kind == GraveyardExilePickKind::BIRDS_TO_DECK)
        {
            --state.birds_return_count;

            if(try_begin_birds_return_fx())
            {
                return;
            }

            if(state.birds_return_count > 0)
            {
                resolve_birds_return_instantly();
            }
            else
            {
                complete_birds_return();
            }

            begin_next_pending_or_finish(true);
            return;
        }

        if(state.selection.type == PendingActionType::BIRDS_RETURN)
        {
            --state.birds_return_count;

            if(try_begin_birds_return_fx())
            {
                return;
            }

            if(state.birds_return_count > 0)
            {
                resolve_birds_return_instantly();
            }
            else
            {
                complete_birds_return();
            }

            begin_next_pending_or_finish(true);
            return;
        }

        begin_next_pending_or_finish(true);
        return;
    }

    exile_push(state, card, true);
    ++state.selection.exiled_count;

    if(state.graveyard.empty())
    {
        state.selection.cursor = 0;
    }
    else
    {
        state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
    }

    sync_target_row_scroll(state.selection.cursor, state.graveyard.size());

    if(kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY)
    {
        --state.selection.remaining_picks;

        if(state.selection.remaining_picks == 0 && state.selection.multiply_factor > 0)
        {
            state.mul_from_card(state.selection.multiply_factor);
            state.selection.multiply_factor = 0;
            draw_round_score();
        }
    }

    if(kind == GraveyardExilePickKind::MULTIPLY_BY_COUNT ||
       kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY)
    {
        if(combo_try_start_pending(state))
        {
            if(kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY &&
               state.selection.remaining_picks <= 0)
            {
                state.selection.type = PendingActionType::NONE;
            }

            enter_combo_mode();
            return;
        }
    }

    if(kind == GraveyardExilePickKind::EXILE_ONE)
    {
        draw_round_score();
        begin_next_pending_or_finish(true);
    }
    else if(kind == GraveyardExilePickKind::MULTIPLY_BY_COUNT)
    {
        if(state.graveyard.empty())
        {
            if(state.selection.exiled_count > 0)
            {
                state.mul_from_card(state.selection.exiled_count);
                state.selection.exiled_count = 0;
                draw_round_score();
            }

            begin_next_pending_or_finish(true);
        }
    }
    else if(kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY)
    {
        if(state.selection.remaining_picks <= 0 || state.graveyard.empty())
        {
            begin_next_pending_or_finish(true);
        }
    }
}

void GameContext::complete_transit_flight(TransitFlight& flight)
{
    if(flight.kind == TransitKind::DECK_TO_HAND)
    {
        complete_deck_to_hand_transit(flight);
        return;
    }

    if(is_gy_transit_kind(flight.kind))
    {
        complete_graveyard_transit(flight);
    }
}

void GameContext::tick_transit_flights()
{
    try_start_pending_transits();

    for(TransitFlight& flight : transit_flights)
    {
        if(!flight.active)
        {
            continue;
        }

        if(flight.delay_frames > 0)
        {
            --flight.delay_frames;
            continue;
        }

        ++flight.frame;

        const int frame_count = flight.kind == TransitKind::DECK_TO_HAND
                                    ? game_layout::HAND_DRAW_FRAMES
                                    : graveyard_card_fx_frame_count(flight);

        if(flight.frame >= frame_count)
        {
            complete_transit_flight(flight);
        }
    }

    sync_transit_flags();

    if(hand_draw_fx_blocking())
    {
        update_target_scroll();
        hand_x = target_hand_x;
    }
}

void GameContext::render_transit_flights(int main_x)
{
    const int deck_target_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
    const int deck_target_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    const int score_target_x = card_target_x_for_score_center(main_x);
    const int score_target_y = card_target_y_for_score_center();
    const int graveyard_target_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
    const int graveyard_target_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);

    for(TransitFlight& flight : transit_flights)
    {
        if(!flight.active || flight.delay_frames > 0)
        {
            flight.fx_card.set_visible(false);
            continue;
        }

        CardFlightSample sample;
        const int frame_count = flight.kind == TransitKind::DECK_TO_HAND
                                    ? game_layout::HAND_DRAW_FRAMES
                                    : graveyard_card_fx_frame_count(flight);

        if(flight.kind == TransitKind::DECK_TO_HAND)
        {
            const int layout_count = hand_scheduled_count(state, in_flight_deck_draw_count());
            hand_slot_screen_position(flight.dest_hand_index, main_x, flight.dest_x, flight.dest_y,
                                      layout_count);
            sample = sample_deck_to_hand_flight(
                flight.start_x, flight.start_y, flight.dest_x, flight.dest_y, flight.frame, frame_count);
        }
        else if(flight.kind == TransitKind::GY_TO_HAND)
        {
            sample = sample_graveyard_to_hand_flight(
                flight.start_x, flight.start_y, flight.dest_x, flight.dest_y, flight.frame, frame_count);
        }
        else if(flight.kind == TransitKind::GY_TO_DECK)
        {
            sample = sample_graveyard_to_deck_flight(
                flight.start_x, flight.start_y, flight.dest_x, flight.dest_y, flight.frame, frame_count);
        }
        else if(flight.kind == TransitKind::GY_BIRDS_TO_DECK)
        {
            sample = sample_hud_via_center_flight(
                flight.start_x, flight.start_y, score_target_x, score_target_y, flight.dest_x, flight.dest_y,
                flight.frame, frame_count);
        }
        else if(flight.style == RemovalStyle::TO_DECK_TOP)
        {
            sample = sample_card_to_deck(
                flight.start_x, flight.start_y, deck_target_x, deck_target_y, flight.frame, frame_count);
        }
        else if(flight.style == RemovalStyle::EXILE_DISSIPATE)
        {
            sample = sample_card_exile_dissipate(
                flight.start_x, flight.start_y, score_target_x, score_target_y, flight.frame, frame_count);
        }
        else
        {
            sample = sample_card_flight(
                flight.start_x, flight.start_y, graveyard_target_x, graveyard_target_y, flight.frame,
                frame_count, 0, 0, 1,
                bn::fixed(game_layout::REMOVAL_MIN_SCALE) / bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR));
        }

        flight.fx_card.set_position(sample.x, sample.y);
        flight.fx_card.set_visual(sample.scale, 0);
        flight.fx_card.set_draw_on_top(true);
        flight.fx_card.set_visible(sample.alpha > 0);

        if(sample.alpha < 1)
        {
            bn::blending::set_transparency_alpha(
                sample.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : sample.alpha);
            flight.fx_card.set_blending_enabled(true);
        }
        else
        {
            flight.fx_card.set_blending_enabled(false);
        }
    }
}
