#ifndef GAME_HELPERS_H
#define GAME_HELPERS_H

#include "bn_seed_random.h"
#include "bn_span.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "card.h"
#include "card_instance.h"
#include "card_type.h"
#include "deck.h"
#include "game_state.h"
#include "game_types.h"

int initial_graveyard_cursor(const GameState& state, CardType exclude);
int clamp_graveyard_cursor(int cursor, int graveyard_size);
int advance_graveyard_cursor(const GameState& state, int cursor, int direction, CardType exclude);

bool card_has_play_effect(CardType type);
bool card_has_play_effect(const GameState& state, CardRef card);
bool card_has_discard_effect(CardType type);
bool card_has_cycle(CardType type);
bool card_has_flashback(CardType type);
bool card_increases_current_number(CardType type);
bool card_increases_current_number(const GameState& state, CardRef card);
bool card_can_add_or_multiply(CardType type);
bool card_can_add_or_multiply(const GameState& state, CardRef card);
int card_preview_plus(const GameState& state, CardRef card, bool flashback);
int flashback_ghost_count(const GameState& state);
int playable_slot_count(const GameState& state);
bool playable_slot_is_flashback(const GameState& state, int visual_index);
int playable_slot_hand_index(const GameState& state, int visual_index);
int playable_slot_graveyard_index(const GameState& state, int visual_index);
CardRef playable_slot_card(const GameState& state, int visual_index);
void queue_effect_draw(GameState& state, int count, bool miracle_on_first);
bool try_draw_one_to_hand(GameState& state);
void maybe_draw_if_solo(GameState& state, CardType type);
void play_miracle_bonus(GameState& state, int amount);
void finish_played_card_from_hand(int& selected_card, GameState& state);
RemovalStyle removal_style_for_hand_play(CardType type);

struct PreparedDeckPlay
{
    CardRef card;
    bool miracle_from_top = false;
};

PreparedDeckPlay deck_search_prepare_play(GameState& state);
PreparedDeckPlay scry_prepare_play(GameState& state);

void deck_search_play_selected(GameState& state);
CardRef deck_search_play_selected_type(GameState& state);

void remove_selected_card(GameState& state, int& selected_card);
void discard_card(GameState& state, int& selected_card);
void swap_cards(GameState& state, int first_card, int second_card);
void scry_play_selected(GameState& state);
CardRef scry_play_selected_type(GameState& state);

void move_toward(int& value, int target, int max_step);

CardRowResult render_card_row(bn::span<Card> pool, bn::span<const CardRef> source,
                              int cursor, int spacing, int y, int scroll_x, int target_scroll_x,
                              int x_offset, bn::span<const int> raise_offsets,
                              const InstancePool* instances = nullptr,
                              bn::sprite_text_generator* pip_generator = nullptr,
                              int visible_window = 0);

bool graveyard_cursor_screen_position(int cursor, int graveyard_size, int spacing, int y, int selected_raise,
                                      int scroll_x, int main_x, int& out_x, int& out_y);

void deal_next_hand(Deck& deck, GameState& state, int& selected_card);
unsigned make_battle_random_seed(const bn::vector<CardType, 50>& collection);
unsigned make_battle_random_seed(const bn::vector<CardRef, 50>& collection);
Deck build_battle_deck(const bn::vector<CardRef, 50>& collection, bn::seed_random& random_engine,
                       const InstancePool& pool);
Deck build_battle_deck(const bn::vector<CardType, 50>& collection, bn::seed_random& random_engine);

#endif
