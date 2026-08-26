#include "combo_system.h"

#include "card.h"
#include "game_events.h"
#include "game_state.h"
#include "score_pop_system.h"
#include "score_count_system.h"
#include "trinket_system.h"

#include "bn_span.h"

namespace
{
    constexpr CardType kRpsShootSequence[] = {
        CardType::ROCK,
        CardType::PAPER,
        CardType::SCISSORS,
        CardType::SHOOT,
    };

    constexpr CardType kPbJellySequence[] = {
        CardType::PEANUT_BUTTER,
        CardType::JELLY,
    };

    constexpr CardType kStrawSticksBricksSequence[] = {
        CardType::STRAW,
        CardType::STICKS,
        CardType::BRICKS,
    };

    constexpr ComboDef kCombos[] = {
        { kRpsShootSequence, 4, 4, 1, true },
        { kPbJellySequence, 2, 2, 2, true },
        { kStrawSticksBricksSequence, 3, 3, 3, true },
    };

    bool window_matches(bn::span<const CardRef> cards, int start, const ComboDef& combo)
    {
        if(start + combo.length > cards.size())
        {
            return false;
        }

        if(! combo.order_independent)
        {
            for(int offset = 0; offset < combo.length; ++offset)
            {
                if(cards[start + offset].type != combo.sequence[offset])
                {
                    return false;
                }
            }

            return true;
        }

        int counts[int(CardType::COUNT)] = {};

        for(int offset = 0; offset < combo.length; ++offset)
        {
            ++counts[int(cards[start + offset].type)];
        }

        for(int index = 0; index < combo.length; ++index)
        {
            if(counts[int(combo.sequence[index])] != 1)
            {
                return false;
            }
        }

        return true;
    }

    bool find_match(bn::span<const CardRef> cards, const ComboDef& combo, int& out_start)
    {
        const int count = cards.size();

        if(count < combo.length)
        {
            return false;
        }

        for(int start = 0; start <= count - combo.length; ++start)
        {
            if(window_matches(cards, start, combo))
            {
                out_start = start;
                return true;
            }
        }

        return false;
    }

    bool window_matches_hand(const GameState& state, int start, const ComboDef& combo)
    {
        if(start + combo.length > state.hand.size())
        {
            return false;
        }

        if(! combo.order_independent)
        {
            for(int offset = 0; offset < combo.length; ++offset)
            {
                if(state.hand[start + offset].type != combo.sequence[offset])
                {
                    return false;
                }
            }

            return true;
        }

        int counts[int(CardType::COUNT)] = {};

        for(int offset = 0; offset < combo.length; ++offset)
        {
            ++counts[int(state.hand[start + offset].type)];
        }

        for(int index = 0; index < combo.length; ++index)
        {
            if(counts[int(combo.sequence[index])] != 1)
            {
                return false;
            }
        }

        return true;
    }

    bool find_hand_match(const GameState& state, const ComboDef& combo, int& out_start)
    {
        const int count = state.hand.size();

        if(count < combo.length)
        {
            return false;
        }

        for(int start = 0; start <= count - combo.length; ++start)
        {
            if(window_matches_hand(state, start, combo))
            {
                out_start = start;
                return true;
            }
        }

        return false;
    }

    bool find_deck_match(const GameState& state, const ComboDef& combo, PendingCombo& out_match)
    {
        int start = 0;
        const bn::span<const CardRef> cards = state.deck.undrawn_span();

        if(!find_match(cards, combo, start))
        {
            return false;
        }

        out_match.use_match_indices = false;
        out_match.start_index = start;
        out_match.length = combo.length;
        return true;
    }

    bool find_graveyard_match(const GameState& state, const ComboDef& combo, PendingCombo& out_match)
    {
        int start = 0;
        const bn::span<const CardRef> cards(state.graveyard.data(), state.graveyard.size());

        if(!find_match(cards, combo, start))
        {
            return false;
        }

        out_match.use_match_indices = false;
        out_match.start_index = start;
        out_match.length = combo.length;
        return true;
    }

    bool revealed_buffer_match(const GameState& state, const ComboDef& combo, PendingCombo& out_match)
    {
        bn::span<const CardRef> cards;

        if(state.selection.type == PendingActionType::SCRY)
        {
            cards = bn::span<const CardRef>(state.selection.scry_buffer.data(),
                                            state.selection.scry_buffer.size());
        }
        else if(state.selection.type == PendingActionType::DECK_SEARCH)
        {
            cards = bn::span<const CardRef>(state.selection.deck_search_buffer.data(),
                                            state.selection.deck_search_buffer.size());
        }
        else
        {
            return false;
        }

        int start = 0;

        if(!find_match(cards, combo, start))
        {
            return false;
        }

        out_match.use_match_indices = false;
        out_match.start_index = start;
        out_match.length = combo.length;
        return true;
    }

    void remove_deck_range(GameState& state, int start, int length)
    {
        if(length <= 0)
        {
            return;
        }

        for(int index = start + length - 1; index >= start; --index)
        {
            state.deck.remove_undrawn_at(index);
        }
    }

    void remove_revealed_range(GameState& state, int start, int length)
    {
        if(length <= 0)
        {
            return;
        }

        if(state.selection.type == PendingActionType::SCRY)
        {
            for(int index = start + length - 1; index >= start; --index)
            {
                if(index >= 0 && index < state.selection.scry_buffer.size())
                {
                    state.selection.scry_buffer.erase(state.selection.scry_buffer.begin() + index);
                }
            }

            return;
        }

        if(state.selection.type == PendingActionType::DECK_SEARCH)
        {
            for(int index = start + length - 1; index >= start; --index)
            {
                state.deck.remove_undrawn_at(index);

                if(index >= 0 && index < state.selection.deck_search_buffer.size())
                {
                    state.selection.deck_search_buffer.erase(state.selection.deck_search_buffer.begin() + index);
                }
            }
        }
    }

    void shuffle_combo_cards_into_deck(GameState& state, const bn::array<CardRef, 4>& cards, int count)
    {
        for(int index = 0; index < count; ++index)
        {
            apply_card_relocated(state, cards[index].type);
            state.deck.add_card(cards[index]);
        }

        if(count > 0)
        {
            state.deck.shuffle(state.rng);
            state.deck.apply_gravity(state.instance_pool);
            combo_check_zone(state, ComboZone::DECK);
        }
    }

    void exile_combo_cards(GameState& state, const bn::array<CardRef, 4>& cards, int count)
    {
        for(int index = 0; index < count; ++index)
        {
            exile_push(state, cards[index]);
        }
    }

    void collect_hand_range(const GameState& state, int start, int length,
                            bn::array<CardRef, 4>& out_cards, int& out_count)
    {
        out_count = 0;

        for(int index = start; index < start + length; ++index)
        {
            if(index >= 0 && index < state.hand.size() && out_count < 4)
            {
                out_cards[out_count++] = state.hand[index];
            }
        }
    }

    void collect_graveyard_indices(const GameState& state, const bn::array<int, 4>& indices, int length,
                                   bn::array<CardRef, 4>& out_cards, int& out_count)
    {
        out_count = 0;

        for(int index = 0; index < length; ++index)
        {
            const int graveyard_index = indices[index];

            if(graveyard_index >= 0 && graveyard_index < state.graveyard.size() && out_count < 4)
            {
                out_cards[out_count++] = state.graveyard[graveyard_index];
            }
        }
    }

    void collect_deck_range(const GameState& state, int start, int length,
                            bn::array<CardRef, 4>& out_cards, int& out_count)
    {
        out_count = 0;
        const int remaining = state.deck.remaining();

        for(int index = start; index < start + length; ++index)
        {
            if(index >= 0 && index < remaining && out_count < 4)
            {
                out_cards[out_count++] = state.deck.peek_undrawn_ref(index);
            }
        }
    }

    void collect_revealed_range(const GameState& state, int start, int length,
                                bn::array<CardRef, 4>& out_cards, int& out_count)
    {
        out_count = 0;
        bn::span<const CardRef> cards;

        if(state.selection.type == PendingActionType::SCRY)
        {
            cards = bn::span<const CardRef>(state.selection.scry_buffer.data(),
                                            state.selection.scry_buffer.size());
        }
        else if(state.selection.type == PendingActionType::DECK_SEARCH)
        {
            cards = bn::span<const CardRef>(state.selection.deck_search_buffer.data(),
                                            state.selection.deck_search_buffer.size());
        }
        else
        {
            return;
        }

        for(int index = start; index < start + length; ++index)
        {
            if(index >= 0 && index < cards.size() && out_count < 4)
            {
                out_cards[out_count++] = cards[index];
            }
        }
    }

    void relocate_resolved_combo_cards(GameState& state, ComboZone zone,
                                       const bn::array<CardRef, 4>& cards, int count)
    {
        if(combo_resolves_to_exile(zone))
        {
            exile_combo_cards(state, cards, count);
            return;
        }

        shuffle_combo_cards_into_deck(state, cards, count);
    }

    const ComboDef* combo_by_id(uint8_t id)
    {
        for(const ComboDef& combo : kCombos)
        {
            if(combo.id == id)
            {
                return &combo;
            }
        }

        return nullptr;
    }

    void remove_hand_range(GameState& state, int start, int length, int& selected_card)
    {
        for(int index = start + length - 1; index >= start; --index)
        {
            state.hand.erase(state.hand.begin() + index);

            if(selected_card > index)
            {
                --selected_card;
            }
            else if(selected_card == index)
            {
                selected_card = state.hand.empty() ? 0 : selected_card;
            }
        }

        if(state.hand.empty())
        {
            selected_card = 0;
        }
        else if(selected_card >= state.hand.size())
        {
            selected_card = state.hand.size() - 1;
        }
    }

    void remove_graveyard_indices(GameState& state, const bn::array<int, 4>& indices, int length)
    {
        if(length <= 0)
        {
            return;
        }

        bn::array<int, 4> sorted = indices;

        for(int first = 0; first < length; ++first)
        {
            for(int second = first + 1; second < length; ++second)
            {
                if(sorted[second] > sorted[first])
                {
                    const int temp = sorted[first];
                    sorted[first] = sorted[second];
                    sorted[second] = temp;
                }
            }
        }

        for(int index = 0; index < length; ++index)
        {
            const int graveyard_index = sorted[index];

            if(graveyard_index >= 0 && graveyard_index < state.graveyard.size())
            {
                state.graveyard.erase(state.graveyard.begin() + graveyard_index);
            }
        }

        game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
    }

    bool combo_still_valid(GameState& state, PendingCombo& pending)
    {
        if(pending.length <= 0)
        {
            return false;
        }

        const ComboDef* combo = combo_by_id(pending.combo_id);

        if(!combo)
        {
            return false;
        }

        if(pending.zone == ComboZone::HAND)
        {
            int start = 0;

            if(!find_hand_match(state, *combo, start))
            {
                return false;
            }

            pending.use_match_indices = false;
            pending.start_index = start;
            pending.length = combo->length;
            return true;
        }

        if(pending.zone == ComboZone::REVEALED)
        {
            PendingCombo fresh{};

            if(!revealed_buffer_match(state, *combo, fresh))
            {
                return false;
            }

            pending.use_match_indices = fresh.use_match_indices;
            pending.start_index = fresh.start_index;
            pending.length = fresh.length;
            pending.match_indices = fresh.match_indices;
            return true;
        }

        if(pending.zone == ComboZone::DECK)
        {
            PendingCombo fresh{};

            if(!find_deck_match(state, *combo, fresh))
            {
                return false;
            }

            pending.use_match_indices = fresh.use_match_indices;
            pending.start_index = fresh.start_index;
            pending.length = fresh.length;
            pending.match_indices = fresh.match_indices;
            return true;
        }

        PendingCombo fresh{};

        if(!find_graveyard_match(state, *combo, fresh))
        {
            return false;
        }

        pending.use_match_indices = fresh.use_match_indices;
        pending.start_index = fresh.start_index;
        pending.length = fresh.length;
        pending.match_indices = fresh.match_indices;
        return true;
    }

    void purge_combo_cinematic_actions(GameState& state)
    {
        for(int index = 0; index < state.pending_actions.size(); )
        {
            if(state.pending_actions[index].type == PendingActionType::COMBO_CINEMATIC)
            {
                state.pending_actions.erase(state.pending_actions.begin() + index);
            }
            else
            {
                ++index;
            }
        }
    }

    void invalidate_pending_combo(GameState& state)
    {
        state.pending_combo = PendingCombo{};
        purge_combo_cinematic_actions(state);
    }
}

void combo_check_zone(GameState& state, ComboZone zone)
{
    if(state.combo_cinematic.active)
    {
        return;
    }

    if(state.pending_combo.length > 0 && !combo_still_valid(state, state.pending_combo))
    {
        invalidate_pending_combo(state);
    }

    if(state.pending_combo.length > 0)
    {
        return;
    }

    for(const ComboDef& combo : kCombos)
    {
        PendingCombo candidate{};

        const bool matched = zone == ComboZone::HAND
            ? [&]() {
                int start = 0;
                if(!find_hand_match(state, combo, start))
                {
                    return false;
                }

                candidate.use_match_indices = false;
                candidate.start_index = start;
                candidate.length = combo.length;
                return true;
            }()
            : zone == ComboZone::REVEALED
                ? revealed_buffer_match(state, combo, candidate)
                : zone == ComboZone::DECK
                    ? find_deck_match(state, combo, candidate)
                    : find_graveyard_match(state, combo, candidate);

        if(!matched)
        {
            continue;
        }

        candidate.combo_id = combo.id;
        candidate.zone = zone;
        state.pending_combo = candidate;
        state.pending_actions.push_back(PendingAction{PendingActionType::COMBO_CINEMATIC});
        return;
    }
}

void combo_cinematic_begin(GameState& state)
{
    const PendingCombo& match = state.pending_combo;

    state.combo_cinematic.frame = 0;
    state.combo_cinematic.cards.clear();

    for(int index = 0; index < match.length; ++index)
    {
        if(match.zone == ComboZone::HAND)
        {
            const int hand_index = match.start_index + index;

            if(hand_index < 0 || hand_index >= state.hand.size())
            {
                continue;
            }

            state.combo_cinematic.cards.push_back(state.hand[hand_index].type);
        }
        else if(match.zone == ComboZone::DECK)
        {
            const int deck_index = match.start_index + index;

            if(deck_index < 0 || deck_index >= state.deck.remaining())
            {
                continue;
            }

            state.combo_cinematic.cards.push_back(state.deck.peek_undrawn(deck_index));
        }
        else if(match.zone == ComboZone::REVEALED)
        {
            const int revealed_index = match.start_index + index;

            if(state.selection.type == PendingActionType::SCRY)
            {
                if(revealed_index < 0 || revealed_index >= state.selection.scry_buffer.size())
                {
                    continue;
                }

                state.combo_cinematic.cards.push_back(state.selection.scry_buffer[revealed_index].type);
            }
            else if(state.selection.type == PendingActionType::DECK_SEARCH)
            {
                if(revealed_index < 0 || revealed_index >= state.selection.deck_search_buffer.size())
                {
                    continue;
                }

                state.combo_cinematic.cards.push_back(state.selection.deck_search_buffer[revealed_index].type);
            }
        }
        else if(match.use_match_indices)
        {
            const int graveyard_index = match.match_indices[index];

            if(graveyard_index < 0 || graveyard_index >= state.graveyard.size())
            {
                continue;
            }

            state.combo_cinematic.cards.push_back(state.graveyard[graveyard_index].type);
        }
        else
        {
            const int graveyard_index = match.start_index + index;

            if(graveyard_index < 0 || graveyard_index >= state.graveyard.size())
            {
                continue;
            }

            state.combo_cinematic.cards.push_back(state.graveyard[graveyard_index].type);
        }
    }

    if(state.combo_cinematic.cards.size() != match.length)
    {
        state.combo_cinematic.active = false;
        state.combo_cinematic.card_count = 0;
        state.combo_cinematic.cards.clear();
        return;
    }

    state.combo_cinematic.active = true;
    state.combo_cinematic.card_count = match.length;
}

void combo_apply_score_bonus(GameState& state)
{
    const ComboDef* combo = combo_by_id(state.pending_combo.combo_id);

    if(!combo)
    {
        return;
    }

    const int before = state.total_score;
    const long long multiplied =
        static_cast<long long>(before) * combo->total_score_multiplier;
    state.total_score = multiplied > 2147483647 ? 2147483647 : int(multiplied);
    score_pop_queue(state, combo->total_score_multiplier, false, TrinketScoreField::TOTAL, true);
    score_count_queue(state, TrinketScoreField::TOTAL, before, state.total_score);
    trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, state.total_score);
}

bool combo_resolves_to_exile(ComboZone zone)
{
    // Consecutive matches in the undrawn library (or scry / deck search) leave the game.
    // Hand and graveyard matches shuffle back into the library.
    return zone == ComboZone::REVEALED || zone == ComboZone::DECK;
}

void combo_remove_resolved_cards(GameState& state, int& selected_card)
{
    const PendingCombo match = state.pending_combo;
    state.pending_combo = PendingCombo{};

    const ComboDef* combo = combo_by_id(match.combo_id);

    if(!combo)
    {
        return;
    }

    bn::array<CardRef, 4> removed = {};
    int removed_count = 0;

    if(match.zone == ComboZone::HAND)
    {
        collect_hand_range(state, match.start_index, match.length, removed, removed_count);
        remove_hand_range(state, match.start_index, match.length, selected_card);
    }
    else if(match.zone == ComboZone::DECK)
    {
        collect_deck_range(state, match.start_index, match.length, removed, removed_count);
        remove_deck_range(state, match.start_index, match.length);
    }
    else if(match.zone == ComboZone::REVEALED)
    {
        collect_revealed_range(state, match.start_index, match.length, removed, removed_count);
        remove_revealed_range(state, match.start_index, match.length);
    }
    else if(match.use_match_indices)
    {
        collect_graveyard_indices(state, match.match_indices, match.length, removed, removed_count);
        remove_graveyard_indices(state, match.match_indices, match.length);
    }
    else
    {
        bn::array<int, 4> indices = {};

        for(int index = 0; index < match.length; ++index)
        {
            indices[index] = match.start_index + index;
        }

        collect_graveyard_indices(state, indices, match.length, removed, removed_count);
        remove_graveyard_indices(state, indices, match.length);
    }

    relocate_resolved_combo_cards(state, match.zone, removed, removed_count);
}

void combo_resolve(GameState& state, int& selected_card)
{
    combo_apply_score_bonus(state);
    combo_remove_resolved_cards(state, selected_card);
}

bool try_start_combo_cinematic(GameState& state)
{
    if(state.pending_actions.empty())
    {
        return false;
    }

    if(state.pending_actions.front().type != PendingActionType::COMBO_CINEMATIC)
    {
        return false;
    }

    if(state.pending_combo.length <= 0)
    {
        state.pending_actions.erase(state.pending_actions.begin());
        return false;
    }

    state.pending_actions.erase(state.pending_actions.begin());
    return combo_start_cinematic_if_valid(state);
}

bool combo_start_cinematic_if_valid(GameState& state)
{
    if(state.combo_cinematic.active)
    {
        return false;
    }

    if(state.pending_combo.length <= 0)
    {
        return false;
    }

    if(!combo_still_valid(state, state.pending_combo))
    {
        invalidate_pending_combo(state);
        return false;
    }

    combo_cinematic_begin(state);

    if(!state.combo_cinematic.active)
    {
        invalidate_pending_combo(state);
        return false;
    }

    return true;
}

bool combo_try_start_pending(GameState& state)
{
    if(state.combo_cinematic.active)
    {
        return false;
    }

    if(state.pending_combo.length <= 0)
    {
        return false;
    }

    for(int index = 0; index < state.pending_actions.size(); )
    {
        if(state.pending_actions[index].type == PendingActionType::COMBO_CINEMATIC)
        {
            state.pending_actions.erase(state.pending_actions.begin() + index);
        }
        else
        {
            ++index;
        }
    }

    if(!combo_start_cinematic_if_valid(state))
    {
        return false;
    }

    return true;
}
