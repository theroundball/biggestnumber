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

    // Right-rail row order: RPS top, straw/sticks/bricks middle, PB&J bottom.
    constexpr int kDisplayRowComboIndices[3] = { 0, 2, 1 };

    bool deck_has_all_combo_pieces(bn::span<const CardRef> cards, const ComboDef& combo)
    {
        int counts[int(CardType::COUNT)] = {};

        for(const CardRef& card : cards)
        {
            ++counts[int(card.type)];
        }

        for(int index = 0; index < combo.length; ++index)
        {
            if(counts[int(combo.sequence[index])] <= 0)
            {
                return false;
            }
        }

        return true;
    }

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

    bool find_graveyard_multiset_match(const GameState& state, const ComboDef& combo, PendingCombo& out_match)
    {
        bool found[int(CardType::COUNT)] = {};

        for(int index = 0; index < combo.length; ++index)
        {
            found[int(combo.sequence[index])] = false;
            out_match.match_indices[index] = -1;
        }

        for(int graveyard_index = 0; graveyard_index < state.graveyard.size(); ++graveyard_index)
        {
            const CardType graveyard_type = state.graveyard[graveyard_index].type;

            for(int piece_index = 0; piece_index < combo.length; ++piece_index)
            {
                if(graveyard_type != combo.sequence[piece_index] ||
                   found[int(graveyard_type)])
                {
                    continue;
                }

                out_match.match_indices[piece_index] = graveyard_index;
                found[int(graveyard_type)] = true;
                break;
            }
        }

        for(int index = 0; index < combo.length; ++index)
        {
            if(!found[int(combo.sequence[index])])
            {
                return false;
            }
        }

        out_match.use_match_indices = true;
        out_match.start_index = 0;
        out_match.length = combo.length;
        return true;
    }

    bool find_graveyard_match(const GameState& state, const ComboDef& combo, PendingCombo& out_match)
    {
        return find_graveyard_multiset_match(state, combo, out_match);
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

    void exile_combo_cards(GameState& state, const bn::array<CardRef, 4>& cards, int count)
    {
        for(int index = 0; index < count; ++index)
        {
            exile_push(state, cards[index]);
        }
    }

    void relocate_resolved_combo_cards(GameState& state, const bn::array<CardRef, 4>& cards, int count)
    {
        exile_combo_cards(state, cards, count);
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
    (void)zone;

    if(state.combo_cinematic.active)
    {
        return;
    }

    if(state.pending_combo.length > 0 && !combo_still_valid(state, state.pending_combo))
    {
        invalidate_pending_combo(state);
    }
}

int combo_ready_count(const GameState& state)
{
    int ready = 0;

    for(const ComboDef& combo : kCombos)
    {
        PendingCombo match{};

        if(find_graveyard_multiset_match(state, combo, match))
        {
            ++ready;
        }
    }

    return ready;
}

uint8_t combo_ready_id_by_ordinal(const GameState& state, int ordinal)
{
    int seen = 0;

    for(const ComboDef& combo : kCombos)
    {
        PendingCombo match{};

        if(!find_graveyard_multiset_match(state, combo, match))
        {
            continue;
        }

        if(seen == ordinal)
        {
            return combo.id;
        }

        ++seen;
    }

    return 0;
}

int combo_ready_multiplier(uint8_t combo_id)
{
    const ComboDef* combo = combo_by_id(combo_id);

    return combo ? combo->total_score_multiplier : 0;
}

CardType combo_ready_display_type(uint8_t combo_id)
{
    const ComboDef* combo = combo_by_id(combo_id);

    if(!combo || combo->length <= 0)
    {
        return CardType::COUNT;
    }

    return combo->sequence[combo->length - 1];
}

bool combo_graveyard_has_type(const GameState& state, CardType type)
{
    for(const CardRef& card : state.graveyard)
    {
        if(card.type == type)
        {
            return true;
        }
    }

    return false;
}

void combo_init_progress_availability(GameState& state)
{
    const bn::span<const CardRef> cards = state.deck.undrawn_span();

    for(int combo_index = 0; combo_index < 3; ++combo_index)
    {
        state.combo_progress_enabled[combo_index] =
            deck_has_all_combo_pieces(cards, kCombos[combo_index]);
    }
}

bool combo_any_progress_bar_enabled(const GameState& state)
{
    for(int combo_index = 0; combo_index < 3; ++combo_index)
    {
        if(state.combo_progress_enabled[combo_index])
        {
            return true;
        }
    }

    return false;
}

void combo_collect_bar_states(const GameState& state, bn::array<ComboBarState, 3>& out)
{
    for(int row_index = 0; row_index < 3; ++row_index)
    {
        const int combo_index = kDisplayRowComboIndices[row_index];
        ComboBarState& bar = out[row_index];

        if(!state.combo_progress_enabled[combo_index])
        {
            bar.length = 0;
            bar.filled = 0;
            continue;
        }

        const ComboDef& combo = kCombos[combo_index];
        bar.length = uint8_t(combo.length);
        bar.filled = 0;

        for(int piece_index = 0; piece_index < combo.length; ++piece_index)
        {
            if(combo_graveyard_has_type(state, combo.sequence[piece_index]))
            {
                ++bar.filled;
            }
        }
    }
}

bool combo_start_player_triggered(GameState& state, uint8_t combo_id)
{
    if(state.combo_cinematic.active)
    {
        return false;
    }

    const ComboDef* combo = combo_by_id(combo_id);

    if(!combo)
    {
        return false;
    }

    PendingCombo match{};

    if(!find_graveyard_multiset_match(state, *combo, match))
    {
        return false;
    }

    invalidate_pending_combo(state);

    match.combo_id = combo_id;
    match.zone = ComboZone::GRAVEYARD;
    state.pending_combo = match;
    state.pending_actions.push_back(PendingAction{PendingActionType::COMBO_CINEMATIC});
    return true;
}

bool combo_would_complete_in_graveyard_with(const GameState& state, uint8_t combo_id, CardType incoming)
{
    const ComboDef* combo = combo_by_id(combo_id);

    if(!combo)
    {
        return false;
    }

    int counts[int(CardType::COUNT)] = {};

    for(const CardRef& card : state.graveyard)
    {
        ++counts[int(card.type)];
    }

    ++counts[int(incoming)];

    for(int index = 0; index < combo->length; ++index)
    {
        if(counts[int(combo->sequence[index])] < 1)
        {
            return false;
        }
    }

    return true;
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
    const int new_total = multiplied > 2147483647 ? 2147483647 : int(multiplied);

    if(!state.try_set_total_score(new_total))
    {
        return;
    }

    score_pop_queue(state, combo->total_score_multiplier, false, TrinketScoreField::TOTAL, true);
    score_count_queue(state, TrinketScoreField::TOTAL, before, state.total_score);
    trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, state.total_score);
}

bool combo_resolves_to_exile(ComboZone zone)
{
    (void)zone;
    return true;
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

    relocate_resolved_combo_cards(state, removed, removed_count);
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
