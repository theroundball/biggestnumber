#include "game_helpers.h"

#include "bn_core.h"
#include "bn_seed_random.h"
#include "bn_span.h"
#include "bn_string.h"

#include "card_data.h"
#include "card.h"
#include "card_instance.h"
#include "game_events.h"
#include "game_ui.h"
#include "play_resolution.h"
#include "ui_common.h"

int initial_graveyard_cursor(const GameState& state, CardType exclude)
{
    (void)exclude;

    return state.graveyard.empty() ? 0 : state.graveyard.size() - 1;
}

int clamp_graveyard_cursor(int cursor, int graveyard_size)
{
    if(graveyard_size <= 0)
    {
        return 0;
    }

    if(cursor < 0)
    {
        return 0;
    }

    if(cursor >= graveyard_size)
    {
        return graveyard_size - 1;
    }

    return cursor;
}

int advance_graveyard_cursor(const GameState& state, int cursor, int direction, CardType exclude)
{
    (void)exclude;

    if(state.graveyard.empty())
    {
        return 0;
    }

    const int next_cursor = cursor + direction;

    if(next_cursor < 0)
    {
        return 0;
    }

    if(next_cursor >= state.graveyard.size())
    {
        return state.graveyard.size() - 1;
    }

    return next_cursor;
}

bool card_has_play_effect(CardType type)
{
    const CardData& data = card_data(type);

    if(data.immediate_plus != 0 || data.immediate_multiply != 0)
    {
        return true;
    }

    if(data.on_play != nullptr)
    {
        return true;
    }

    for(int index = 0; index < 3; ++index)
    {
        const RoundModifier& mod = data.future[index];

        if(mod.positive != 0 || mod.multiply != 0 || mod.draw_at_start != 0)
        {
            return true;
        }
    }

    return false;
}

bool card_has_play_effect(const GameState& state, CardRef card)
{
    if(card_has_play_effect(card.type))
    {
        return true;
    }

    if(!card.has_instance())
    {
        return false;
    }

    const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

    if(!instance)
    {
        return false;
    }

    return instance->plus_digit != 0 || instance->increment_mult;
}

bool card_has_discard_effect(CardType type)
{
    return card_data(type).on_discard != nullptr;
}

bool card_has_cycle(CardType type)
{
    return card_data(type).has_cycle;
}

bool card_has_flashback(CardType type)
{
    return card_data(type).has_flashback;
}

bool card_numeric_play_override(CardType type)
{
    switch(type)
    {
    case CardType::MIRACLE:
    case CardType::JOURNAL:
    case CardType::TIME_IS_TOO_EXPENSIVE:
    case CardType::DILLA:
    case CardType::SEMAPHORE:
    case CardType::THRESHOLD:
    case CardType::TOMBSTONES:
    case CardType::BIRDS_OF_A_FEATHER:
    case CardType::ROUNDUP:
    case CardType::CLOVER:
    case CardType::BIG_KUROSAWA_BURGER:
    case CardType::RAGS_TO_RICHES:
    case CardType::BONES:
        return true;
    default:
        return false;
    }
}

bool card_increases_current_number(CardType type)
{
    const CardData& data = card_data(type);

    if(data.immediate_plus > 0 || data.immediate_multiply > 1)
    {
        return true;
    }

    return card_numeric_play_override(type);
}

bool card_increases_current_number(const GameState& state, CardRef card)
{
    if(card_preview_plus(state, card, false) > 0)
    {
        return true;
    }

    const CardData& data = card_data(card.type);

    if(data.immediate_multiply > 1)
    {
        return true;
    }

    return card_numeric_play_override(card.type);
}

bool card_can_add_or_multiply(CardType type)
{
    if(card_increases_current_number(type))
    {
        return true;
    }

    const CardData& data = card_data(type);

    for(int index = 0; index < 3; ++index)
    {
        if(data.future[index].positive > 0 || data.future[index].multiply > 1)
        {
            return true;
        }
    }

    return false;
}

bool card_can_add_or_multiply(const GameState& state, CardRef card)
{
    if(card_increases_current_number(state, card))
    {
        return true;
    }

    return card_can_add_or_multiply(card.type);
}

namespace
{
    bool score_contains_digit_below(int value, int below)
    {
        if(below <= 0)
        {
            return false;
        }

        int digits = value < 0 ? -value : value;

        if(digits == 0)
        {
            return 0 < below;
        }

        while(digits > 0)
        {
            if(digits % 10 < below)
            {
                return true;
            }

            digits /= 10;
        }

        return false;
    }

    bool total_score_is_palindrome(int total)
    {
        const bn::string<12> digits = bn::to_string<12>(total);

        if(digits.size() > 9)
        {
            return false;
        }

        for(int left = 0, right = digits.size() - 1; left < right; ++left, --right)
        {
            if(digits[left] != digits[right])
            {
                return false;
            }
        }

        return true;
    }

    bool palindrome_would_increase_total(int total, bool applying_double_adds)
    {
        if(!total_score_is_palindrome(total))
        {
            return false;
        }

        const bn::string<12> digits = bn::to_string<12>(total);
        long long outer_place = 10;

        for(int index = 0; index < digits.size(); ++index)
        {
            outer_place *= 10;
        }

        const int wrapper = applying_double_adds ? 2 : 1;
        const long long after = static_cast<long long>(wrapper) * outer_place +
                                static_cast<long long>(total) * 10 + wrapper;

        return after <= 2147483647 && after > total;
    }

    bool roundup_would_increase_total(const GameState& state)
    {
        const int count = state.roundup_play_count + 1;
        int divisor = 10;

        if(count == 2)
        {
            divisor = 100;
        }
        else if(count >= 3)
        {
            divisor = 1000;
        }

        const int after = ((state.total_score + divisor - 1) / divisor) * divisor;
        return after > state.total_score;
    }

    bool waterfall_immediate_round_increase(const GameState& state, CardRef card)
    {
        if(card_preview_plus(state, card, false) > 0)
        {
            return true;
        }

        int multiply = card_data(card.type).immediate_multiply;

        if(card.has_instance())
        {
            if(const CardInstance* instance = instance_at(state.instance_pool, card.instance_id))
            {
                multiply = effective_immediate_multiply(*instance);
            }
        }

        return multiply > 1 && state.round.running > 0;
    }
}

bool mill_reveal_card_hits(const GameState& state, CardRef card, bool waterfall)
{
    return waterfall ? waterfall_would_make_bigger(state, card)
                     : card_can_add_or_multiply(state, card);
}

bool waterfall_would_make_bigger(const GameState& state, CardRef card)
{
    switch(card.type)
    {
    case CardType::WISHES:
    case CardType::SIPS:
    case CardType::SNAIL_MAIL:
    case CardType::SWAP:
    case CardType::HACKER:
    case CardType::LIBRARIAN:
    case CardType::PILOT:
    case CardType::NECROMANCY:
    case CardType::JACKS:
    case CardType::FISHING_POLE:
    case CardType::CUPS:
    case CardType::ROLL_OVER:
    case CardType::FLEX:
    case CardType::SPECULATIVE:
    case CardType::KEEP_GOING:
    case CardType::LIFELINE:
    case CardType::SWIVEL:
    case CardType::TURTLE_MODE:
    case CardType::RAGS_TO_RICHES:
        return false;

    case CardType::MIRACLE:
        return true;

    case CardType::THE_FOURTH:
        return true;

    case CardType::THE_FIFTH:
        return score_contains_digit_below(state.total_score, 5);

    case CardType::PALINDROME:
        return palindrome_would_increase_total(state.total_score, state.applying_double_adds);

    case CardType::ROUNDUP:
        return roundup_would_increase_total(state);

    case CardType::CLOVER:
        return state.graveyard.size() >= 3;

    case CardType::BIG_KUROSAWA_BURGER:
        return state.hand.size() > 1;

    case CardType::THRESHOLD:
    case CardType::TOMBSTONES:
    case CardType::BIRDS_OF_A_FEATHER:
    case CardType::TIME_IS_TOO_EXPENSIVE:
        return true;

    case CardType::BONES:
        return state.graveyard.size() > 0;

    case CardType::JOURNAL:
        return state.cards_played_this_round > 0 && state.round.running > 0;

    case CardType::SEMAPHORE:
        if(state.current_round == 1 && state.cards_played_this_round == 0)
        {
            return true;
        }

        if(state.hand.size() == 1 && state.deck.empty())
        {
            return state.graveyard.size() > 1;
        }

        return true;

    case CardType::TRIPTYCH:
        return true;

    default:
        break;
    }

    return waterfall_immediate_round_increase(state, card);
}

int card_preview_plus(const GameState& state, CardRef card, bool flashback)
{
    const CardData& data = card_data(card.type);
    int plus = (flashback && data.has_flashback) ? data.flashback_plus : data.immediate_plus;

    if(!flashback)
    {
        if(const CardInstance* instance = instance_at(state.instance_pool, card.instance_id))
        {
            plus = effective_immediate_plus(*instance);
        }
    }

    if(state.pending_double_adds && plus > 0)
    {
        plus *= 2;
    }

    return plus;
}

int flashback_ghost_count(const GameState& state)
{
    int count = 0;

    for(int index = 0; index < state.graveyard.size(); ++index)
    {
        if(card_has_flashback(state.graveyard[index].type))
        {
            ++count;
        }
    }

    return count;
}

bool empty_hand_triggers_round_end(const GameState& state)
{
    return state.hand.empty() && flashback_ghost_count(state) == 0;
}

int playable_slot_count(const GameState& state)
{
    if(state.hand.empty())
    {
        return flashback_ghost_count(state);
    }

    return state.hand.size() + flashback_ghost_count(state);
}

bool playable_slot_is_flashback(const GameState& state, int visual_index)
{
    return visual_index >= state.hand.size();
}

int playable_slot_hand_index(const GameState& state, int visual_index)
{
    if(visual_index < 0 || visual_index >= state.hand.size())
    {
        return -1;
    }

    return visual_index;
}

int playable_slot_graveyard_index(const GameState& state, int visual_index)
{
    const int ghost_ordinal = visual_index - state.hand.size();

    if(ghost_ordinal < 0)
    {
        return -1;
    }

    int seen = 0;

    for(int index = 0; index < state.graveyard.size(); ++index)
    {
        if(!card_has_flashback(state.graveyard[index].type))
        {
            continue;
        }

        if(seen == ghost_ordinal)
        {
            return index;
        }

        ++seen;
    }

    return -1;
}

CardRef playable_slot_card(const GameState& state, int visual_index)
{
    if(visual_index < 0)
    {
        return CardRef{};
    }

    if(visual_index < state.hand.size())
    {
        return state.hand[visual_index];
    }

    const int gy_index = playable_slot_graveyard_index(state, visual_index);

    if(gy_index < 0)
    {
        return CardRef{};
    }

    return state.graveyard[gy_index];
}

bool try_draw_one_to_hand(GameState& state)
{
    if(state.deck.remaining() == 0)
    {
        return false;
    }

    queue_effect_draw(state, 1, true);
    return true;
}

void queue_effect_draw(GameState& state, int count, bool miracle_on_first)
{
    if(count <= 0)
    {
        return;
    }

    PendingAction action;
    action.type = PendingActionType::EFFECT_DECK_DRAW;
    action.count = count;
    action.hand_index = miracle_on_first ? 1 : 0;
    state.pending_actions.push_back(action);
}

void maybe_draw_if_solo(GameState& state, CardType type)
{
    if(type != CardType::SOLO || !state.hand.empty())
    {
        return;
    }

    try_draw_one_to_hand(state);
}

void play_miracle_bonus(GameState& state, int amount)
{
    state.add_from_card(amount);
    ++state.cards_played_this_round;
}

void remove_selected_card(GameState &state, int &selected_card)
{
    hand_remove_at_to_graveyard(state, selected_card, selected_card);
}

void finish_played_card_from_hand(int& selected_card, GameState& state)
{
    if(selected_card < 0 || selected_card >= state.hand.size())
    {
        return;
    }

    hand_remove_at_to_deck_top(state, selected_card, selected_card);
}

RemovalStyle removal_style_for_hand_play(CardType type)
{
    if(card_data(type).exiles_self_on_play)
    {
        return RemovalStyle::EXILE_DISSIPATE;
    }

    return RemovalStyle::TO_GRAVEYARD;
}

void discard_card(GameState &state, int &selected_card)
{
    if(state.hand.empty() || selected_card >= state.hand.size())
    {
        return;
    }

    hand_remove_at_to_graveyard(state, selected_card, selected_card);
}

void swap_cards(GameState &state, int first_card, int second_card)
{
    hand_swap_cards(state, first_card, second_card);
}

CardRef scry_play_selected_type(GameState& state)
{
    const PreparedDeckPlay prepared = scry_prepare_play(state);

    if(prepared.miracle_from_top)
    {
        play_miracle_bonus(state, 10);
    }
    else
    {
        PlayResolutionContext context;
        context.source = PlaySource::SCRY;
        context.apply_destination = !state.swivel_waiting;
        resolve_played_card(state, prepared.card, context);
    }

    return prepared.card;
}

PreparedDeckPlay scry_prepare_play(GameState& state)
{
    PreparedDeckPlay result;
    const int cursor = state.selection.cursor;
    result.card = state.selection.scry_buffer[cursor];
    const bool played_from_deck_top = cursor + 1 == state.selection.scry_buffer.size();
    state.selection.scry_buffer.erase(state.selection.scry_buffer.begin() + cursor);

    for(int index = 0; index < state.selection.scry_buffer.size(); ++index)
    {
        state.deck.insert_top(state.selection.scry_buffer[index]);
    }

    state.selection.scry_buffer.clear();
    result.miracle_from_top = result.card.type == CardType::MIRACLE && played_from_deck_top;

    if(result.miracle_from_top)
    {
        state.first_deck_draw_this_round = false;
    }

    return result;
}

void scry_play_selected(GameState& state)
{
    scry_play_selected_type(state);
}

PreparedDeckPlay deck_search_prepare_play(GameState& state)
{
    PreparedDeckPlay result;
    const int cursor = state.selection.cursor;
    state.deck.move_undrawn_to_top(cursor);

    if(!state.deck.draw(result.card))
    {
        result.card = CardRef{CardType::COUNT, NO_INSTANCE};
        state.selection.deck_search_buffer.clear();
        return result;
    }

    result.miracle_from_top = cursor == 0 && result.card.type == CardType::MIRACLE;
    state.selection.deck_search_buffer.clear();
    return result;
}

CardRef deck_search_play_selected_type(GameState& state)
{
    const PreparedDeckPlay prepared = deck_search_prepare_play(state);

    if(prepared.card.type == CardType::COUNT)
    {
        return prepared.card;
    }

    if(prepared.miracle_from_top)
    {
        play_miracle_bonus(state, 10);
    }
    else
    {
        PlayResolutionContext context;
        context.source = PlaySource::DECK_SEARCH;
        context.apply_destination = !state.swivel_waiting;
        resolve_played_card(state, prepared.card, context);
    }

    return prepared.card;
}

void deck_search_play_selected(GameState& state)
{
    deck_search_play_selected_type(state);
}

void move_toward(int &value, int target, int max_step)
{
    const int distance = target - value;

    if(distance == 0)
    {
        return;
    }

    if(distance > 0)
    {
        value += distance < max_step ? distance : max_step;
    }
    else
    {
        value -= -distance < max_step ? -distance : max_step;
    }
}

// Renders `source` windowed around `cursor` into `pool`. `visible_window` is the
// steady on-screen count used for centering/scroll math; pool may be larger for peeks.
CardRowResult render_card_row(bn::span<Card> pool, bn::span<const CardRef> source,
                              int cursor, int spacing, int y, int scroll_x, int target_scroll_x,
                              int x_offset, bn::span<const int> raise_offsets,
                              const InstancePool* instances, bn::sprite_text_generator* pip_generator,
                              int visible_window)
{
    const int window = visible_window > 0 ? visible_window : pool.size();
    const int count = source.size();
    const int visible_count = count < window ? count : window;
    const int row_start_x = -(visible_count * spacing) / 2;
    const int logical_first = first_visible_index(cursor, count, window);
    const int first_visible = scroll_x / spacing;
    const int scroll_sub = scroll_x - first_visible * spacing;

    for(int slot = 0; slot < pool.size(); ++slot)
    {
        const int index = first_visible + slot;

        if(index < 0 || index >= count)
        {
            release_card_display_tiles(pool[slot]);
            continue;
        }

        pool[slot].set_type(source[index].type);

        const int card_x = row_start_x + slot * spacing - scroll_sub + x_offset;
        const int eased_raise = index < raise_offsets.size() ? raise_offsets[index] : 0;
        const int wave_raise = row_scroll_pair_raise(index, cursor, scroll_x, target_scroll_x, spacing, count);
        const int card_y = y - eased_raise - wave_raise;

        pool[slot].set_position(card_x, card_y);
        pool[slot].set_visible(true);
        pool[slot].set_blending_enabled(false);

        const CardInstance* instance = nullptr;

        if(instances && source[index].has_instance())
        {
            instance = instance_at(*instances, source[index].instance_id);
        }

        if(pip_generator && card_data(source[index].type).text_only)
        {
            pool[slot].sync_face_labels(pip_generator, instance);
        }
        else
        {
            pool[slot].clear_face_labels();
        }

        if(instances && pip_generator && source[index].has_instance())
        {
            pool[slot].set_upgrade_pips(pip_generator, instance);
        }
        else
        {
            pool[slot].clear_upgrade_pips();
        }
    }

    CardRowResult result;
    result.row_start_x = row_start_x;
    result.cursor_slot = count == 0 ? -1 : cursor - logical_first;
    result.visible_count = visible_count;
    result.scroll_sub = scroll_sub;
    result.has_left = logical_first > 0;
    result.has_right = logical_first + visible_count < count;
    return result;
}

bool graveyard_cursor_screen_position(int cursor, int graveyard_size, int spacing, int y, int selected_raise,
                                      int scroll_x, int main_x, int& out_x, int& out_y)
{
    if(graveyard_size <= 0 || cursor < 0 || cursor >= graveyard_size)
    {
        return false;
    }

    const int window = game_layout::VISIBLE_CARD_COUNT;
    const int visible_count = graveyard_size < window ? graveyard_size : window;
    const int row_start_x = -(visible_count * spacing) / 2;
    const int logical_first = first_visible_index(cursor, graveyard_size, window);
    const int first_visible = scroll_x / spacing;
    const int scroll_sub = scroll_x - first_visible * spacing;
    const int cursor_slot = cursor - logical_first;

    if(cursor_slot < 0 || cursor_slot >= visible_count)
    {
        return false;
    }

    out_x = row_start_x + cursor_slot * spacing - scroll_sub + main_x;
    out_y = y - selected_raise;
    return true;
}

void deal_next_hand(Deck &deck, GameState &state, int &selected_card)
{
    state.hand.clear();

    int draw_count = deck.remaining() < 5 ? deck.remaining() : 5;

    for (int card_index = 0; card_index < draw_count; ++card_index)
    {
        CardRef card;

        if (!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    for (int extra_draw = 0; extra_draw < state.round_start_extra_draws; ++extra_draw)
    {
        CardRef card;

        if (!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    state.round_start_extra_draws = 0;
    selected_card = 0;
}

// Build a fresh, shuffled battle deck from the player's persistent collection.
// Each Biggest Number game plays on this disposable copy; in-battle effects
// (draw, graveyard, RECLAIM's exile) mutate only the copy, so the collection is
// never touched and every owned card returns for the next game instance.
unsigned make_battle_random_seed(const bn::vector<CardType, 50>& collection)
{
    unsigned seed = static_cast<unsigned>(bn::core::current_cpu_ticks());
    seed ^= static_cast<unsigned>(bn::core::last_cpu_ticks()) << 16;
    seed ^= static_cast<unsigned>(collection.size()) * 2654435761u;

    for(int index = 0; index < collection.size(); ++index)
    {
        seed ^= static_cast<unsigned>(collection[index]) * (index + 1u);
        seed = (seed << 5) | (seed >> 27);
    }

    bn::seed_random mixer(seed);

    for(int index = 0; index < 4; ++index)
    {
        mixer.update();
        seed ^= mixer.get();
    }

    if(seed == 0)
    {
        seed = 1;
    }

    return seed;
}

unsigned make_battle_random_seed(const bn::vector<CardRef, 50>& collection)
{
    bn::vector<CardType, 50> types;

    for(int index = 0; index < collection.size(); ++index)
    {
        types.push_back(collection[index].type);
    }

    return make_battle_random_seed(types);
}

Deck build_battle_deck(const bn::vector<CardRef, 50>& collection, bn::seed_random& random_engine,
                       const InstancePool& pool)
{
    Deck deck(0);

    for(int index = 0; index < collection.size(); ++index)
    {
        deck.add_card(collection[index]);
    }

    deck.shuffle(random_engine);
    deck.apply_gravity(pool);
    return deck;
}

Deck build_battle_deck(const bn::vector<CardType, 50>& collection, bn::seed_random& random_engine)
{
    bn::vector<CardRef, 50> refs;

    for(CardType card_type : collection)
    {
        refs.push_back(CardRef{card_type, NO_INSTANCE});
    }

    InstancePool empty_pool;
    return build_battle_deck(refs, random_engine, empty_pool);
}
