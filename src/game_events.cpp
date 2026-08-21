#include "game_events.h"

#include "bn_algorithm.h"
#include "card.h"
#include "card_data.h"
#include "combo_system.h"
#include "game_state.h"

namespace
{
    void check_birds_of_a_feather(GameState& state)
    {
        const int threshold = 1 + state.birds_played_count;
        bn::vector<int, 50> return_indices;

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

            const int run_length = end - start;

            if(run_length >= threshold)
            {
                for(int index = start; index < end; ++index)
                {
                    return_indices.push_back(index);
                }
            }

            start = end;
        }

        if(return_indices.empty())
        {
            return;
        }

        for(int reverse_index = return_indices.size() - 1; reverse_index >= 0; --reverse_index)
        {
            const int index = return_indices[reverse_index];

            if(!state.hand.full())
            {
                state.hand.push_back(state.graveyard[index]);
            }

            state.graveyard.erase(state.graveyard.begin() + index);
        }

        combo_check_zone(state, ComboZone::HAND);
        combo_check_zone(state, ComboZone::GRAVEYARD);
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

bool hand_add_card(GameState& state, CardRef card, bool from_deck_draw)
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
        state.pending_hand_draws.push_back(PendingHandDraw{card});
        return true;
    }

    state.hand.push_back(card);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);

    return true;
}

bool hand_add_card(GameState& state, CardType type, bool from_deck_draw)
{
    return hand_add_card(state, CardRef{type, NO_INSTANCE}, from_deck_draw);
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

    if(state.hand.empty())
    {
        selected_card = 0;
    }
    else if(selected_card >= state.hand.size())
    {
        selected_card = state.hand.size() - 1;
    }

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
    trigger_discard_effect_if_any(state, removed.type);

    if(state.hand.empty())
    {
        selected_card = 0;
    }
    else if(selected_card >= state.hand.size())
    {
        selected_card = state.hand.size() - 1;
    }

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

    if(state.hand.empty())
    {
        selected_card = 0;
    }
    else if(selected_card >= state.hand.size())
    {
        selected_card = state.hand.size() - 1;
    }

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

    if(state.hand.empty())
    {
        selected_card = 0;
    }
    else if(selected_card >= state.hand.size())
    {
        selected_card = state.hand.size() - 1;
    }

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

void exile_push(GameState& state, CardRef card)
{
    if(state.exile.full())
    {
        return;
    }

    state.exile.push_back(card);
}

void exile_push(GameState& state, CardType type)
{
    exile_push(state, CardRef{type, NO_INSTANCE});
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
        state.deck.add_card(card);
    }

    if(!cards.empty())
    {
        game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
    }
}


void game_events_dispatch(GameState& state, GameEvent event)
{
    if(event == GameEvent::HAND_CHANGED)
    {
        combo_check_zone(state, ComboZone::HAND);
    }
    else
    {
        combo_check_zone(state, ComboZone::GRAVEYARD);
        check_birds_of_a_feather(state);
    }
}
