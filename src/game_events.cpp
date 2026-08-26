#include "game_events.h"

#include "bn_algorithm.h"
#include "card.h"
#include "card_data.h"
#include "combo_system.h"
#include "game_state.h"

namespace
{
    bool birds_return_already_queued(const GameState& state)
    {
        if(state.birds_return_count > 0)
        {
            return true;
        }

        for(int index = 0; index < state.pending_actions.size(); ++index)
        {
            if(state.pending_actions[index].type == PendingActionType::BIRDS_RETURN)
            {
                return true;
            }
        }

        return false;
    }

    void adjust_selected_after_hand_remove(GameState& state, int removed_index, int& selected_card)
    {
        if(state.hand.empty())
        {
            selected_card = 0;
            return;
        }

        if(selected_card > removed_index)
        {
            --selected_card;
        }

        if(selected_card >= state.hand.size())
        {
            selected_card = state.hand.size() - 1;
        }

        if(selected_card < 0)
        {
            selected_card = 0;
        }
    }

    void check_birds_of_a_feather(GameState& state)
    {
        if(state.birds_return_threshold > 5 || birds_return_already_queued(state))
        {
            return;
        }

        int return_start = -1;
        int return_end = -1;

        if(!birds_find_return_run(state, return_start, return_end))
        {
            return;
        }

        if(state.pending_actions.full())
        {
            birds_return_run_to_deck(state, return_start, return_end);
            return;
        }

        state.pending_actions.insert(state.pending_actions.begin(),
                                     PendingAction{PendingActionType::BIRDS_RETURN});
    }

    void queue_miracle_auto_play(GameState& state)
    {
        if(state.hand.empty())
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::MIRACLE_AUTO_PLAY;
        action.hand_index = state.hand.size() - 1;
        state.pending_actions.push_back(action);
    }
}

int hand_scheduled_count(const GameState& state, bool include_in_flight_draw)
{
    int count = state.hand.size() + state.pending_hand_draws.size();

    if(include_in_flight_draw)
    {
        // Reserved for callers that track an in-flight draw outside pending_hand_draws.
        ++count;
    }

    return count;
}

bool hand_add_card(GameState& state, CardRef card, bool from_deck_draw, bool miracle_auto_play)
{
    if(state.hand.full())
    {
        return false;
    }

    if(from_deck_draw)
    {
        if(state.pending_hand_draws.full())
        {
            return false;
        }

        state.first_deck_draw_this_round = false;
        state.pending_hand_draws.push_back(PendingHandDraw{card, miracle_auto_play});
        return true;
    }

    state.hand.push_back(card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);

    return true;
}

bool hand_add_card(GameState& state, CardType type, bool from_deck_draw, bool miracle_auto_play)
{
    return hand_add_card(state, CardRef{type, NO_INSTANCE}, from_deck_draw, miracle_auto_play);
}

void hand_swap_cards(GameState& state, int first_card, int second_card)
{
    bn::swap(state.hand[first_card], state.hand[second_card]);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);
}

void hand_remove_at_to_graveyard_played(GameState& state, int index, int& selected_card)
{
    if(index < 0 || index >= state.hand.size())
    {
        return;
    }

    const CardRef removed = state.hand[index];
    graveyard_push(state, removed);
    state.hand.erase(state.hand.begin() + index);
    adjust_selected_after_hand_remove(state, index, selected_card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);
}

void hand_remove_at_to_graveyard(GameState& state, int index, int& selected_card)
{
    if(index < 0 || index >= state.hand.size())
    {
        return;
    }

    const CardRef removed = state.hand[index];
    graveyard_push(state, removed);
    state.hand.erase(state.hand.begin() + index);
    battle_stat_record_keyword_discard(state);
    trigger_discard_effect_if_any(state, removed.type);
    adjust_selected_after_hand_remove(state, index, selected_card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);
}

void hand_remove_at_exiled(GameState& state, int index, int& selected_card)
{
    if(index < 0 || index >= state.hand.size())
    {
        return;
    }

    const CardRef removed = state.hand[index];
    state.hand.erase(state.hand.begin() + index);
    exile_push(state, removed);
    adjust_selected_after_hand_remove(state, index, selected_card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);
}

void hand_remove_at_to_deck_top(GameState& state, int index, int& selected_card)
{
    if(index < 0 || index >= state.hand.size())
    {
        return;
    }

    state.deck.insert_top(state.hand[index]);
    state.deck.apply_gravity(state.instance_pool);
    state.hand.erase(state.hand.begin() + index);
    adjust_selected_after_hand_remove(state, index, selected_card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);
}

void trigger_discard_effect_if_any(GameState& state, CardType type)
{
    const CardData& data = card_data(type);

    if(data.on_discard)
    {
        apply_card_discard(state, type);
    }
}

void graveyard_push(GameState& state, CardRef card)
{
    if(state.graveyard.full())
    {
        return;
    }

    state.graveyard.push_back(card);
    graveyard_apply_gravity(state);
    game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
}

void graveyard_push(GameState& state, CardType type)
{
    graveyard_push(state, CardRef{type, NO_INSTANCE});
}

void graveyard_remove_at(GameState& state, int index)
{
    if(index < 0 || index >= state.graveyard.size())
    {
        return;
    }

    state.graveyard.erase(state.graveyard.begin() + index);
    game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
}

void graveyard_swap_at(GameState& state, int first_index, int second_index)
{
    if(first_index < 0 || second_index < 0 ||
       first_index >= state.graveyard.size() || second_index >= state.graveyard.size() ||
       first_index == second_index)
    {
        return;
    }

    bn::swap(state.graveyard[first_index], state.graveyard[second_index]);
    graveyard_apply_gravity(state);
    game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
}

void graveyard_clear(GameState& state)
{
    if(state.graveyard.empty())
    {
        return;
    }

    state.graveyard.clear();
    game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
}

void exile_push(GameState& state, CardRef card, bool from_graveyard)
{
    (void)from_graveyard;

    if(state.exile.full())
    {
        return;
    }

    state.exile.push_back(card);

    battle_stat_record_exile(state);

    const CardData& data = card_data(card.type);

    if(data.on_exile)
    {
        data.on_exile(state);
    }
}

void exile_push(GameState& state, CardType type, bool from_graveyard)
{
    exile_push(state, CardRef{type, NO_INSTANCE}, from_graveyard);
}

void graveyard_apply_gravity(GameState& state)
{
    const int count = state.graveyard.size();

    if(count <= 1 || state.instance_pool.count == 0)
    {
        return;
    }

    // Graveyard vector back = most recent / "top" for cursor defaults — Yeast toward back.
    bn::vector<CardRef, 50> yeast;
    bn::vector<CardRef, 50> plain;
    bn::vector<CardRef, 50> lead;

    for(int index = 0; index < count; ++index)
    {
        const CardRef card = state.graveyard[index];
        const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

        if(instance && instance->gravity == Gravity::YEAST)
        {
            yeast.push_back(card);
        }
        else if(instance && instance->gravity == Gravity::LEAD)
        {
            lead.push_back(card);
        }
        else
        {
            plain.push_back(card);
        }
    }

    state.graveyard.clear();

    for(int index = 0; index < lead.size(); ++index)
    {
        state.graveyard.push_back(lead[index]);
    }

    for(int index = 0; index < plain.size(); ++index)
    {
        state.graveyard.push_back(plain[index]);
    }

    for(int index = 0; index < yeast.size(); ++index)
    {
        state.graveyard.push_back(yeast[index]);
    }
}

bool birds_find_return_run(const GameState& state, int& out_start, int& out_end)
{
    const int threshold = state.birds_return_threshold;
    out_start = -1;
    out_end = -1;

    if(threshold > 5)
    {
        return false;
    }

    for(int start = 0; start < state.graveyard.size(); )
    {
        if(state.graveyard[start].type != CardType::BIRDS_OF_A_FEATHER)
        {
            ++start;
            continue;
        }

        int end = start;

        while(end < state.graveyard.size() &&
              state.graveyard[end].type == CardType::BIRDS_OF_A_FEATHER)
        {
            ++end;
        }

        if(end - start >= threshold)
        {
            out_start = start;
            out_end = end;
            return true;
        }

        start = end;
    }

    return false;
}

void birds_return_run_to_deck(GameState& state, int return_start, int return_end)
{
    if(return_start < 0 || return_end <= return_start || return_end > state.graveyard.size())
    {
        return;
    }

    state.deck.compact();

    for(int index = return_start; index < return_end; ++index)
    {
        apply_card_relocated(state, state.graveyard[index].type);
        state.deck.add_card(state.graveyard[index]);
    }

    for(int index = return_end - 1; index >= return_start; --index)
    {
        state.graveyard.erase(state.graveyard.begin() + index);
    }

    state.deck.apply_gravity(state.instance_pool);
    state.birds_return_threshold = state.birds_return_threshold == 5 ? 6 : state.birds_return_threshold + 1;
    combo_check_zone(state, ComboZone::GRAVEYARD);
}

void necromancy_shuffle_graveyard_to_deck(GameState& state)
{
    state.deck.compact();

    bn::vector<CardRef, 50> cards;

    while(!state.graveyard.empty())
    {
        cards.push_back(state.graveyard.back());
        state.graveyard.pop_back();
    }

    for(int index = cards.size() - 1; index > 0; --index)
    {
        const int swap_index = state.rng.get_int(index + 1);
        bn::swap(cards[index], cards[swap_index]);
    }

    for(CardRef card : cards)
    {
        apply_card_relocated(state, card.type);
        state.deck.add_card(card);
    }

    if(!cards.empty())
    {
        game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
    }

    combo_check_zone(state, ComboZone::DECK);
}


void battle_stats_reset(GameState& state)
{
    state.battle_stats = BattleStats{};
}

void battle_stat_record_exile(GameState& state)
{
    ++state.battle_stats.cards_exiled;
}

void battle_stat_record_draw_to_hand(GameState& state)
{
    ++state.battle_stats.cards_drawn_this_round;
}

void battle_stat_record_keyword_discard(GameState& state)
{
    ++state.battle_stats.keyword_discards;
}

void battle_stat_record_cycle(GameState& state)
{
    ++state.battle_stats.cycles;
}

void battle_stat_record_flashback(GameState& state)
{
    ++state.battle_stats.flashbacks;
}

void game_events_dispatch(GameState& state, GameEvent event)
{
    if(event == GameEvent::HAND_CHANGED)
    {
        combo_check_zone(state, ComboZone::HAND);
        check_birds_of_a_feather(state);
    }
    else
    {
        combo_check_zone(state, ComboZone::GRAVEYARD);
        check_birds_of_a_feather(state);
    }
}
