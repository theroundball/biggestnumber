#include "game_helpers.h"

#include "bn_core.h"
#include "bn_seed_random.h"
#include "bn_span.h"

#include "card_data.h"
#include "card.h"
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

        if(instances && pip_generator && source[index].has_instance())
        {
            pool[slot].set_upgrade_pips(pip_generator, instance_at(*instances, source[index].instance_id));
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
