#ifndef GAME_EVENTS_H
#define GAME_EVENTS_H

#include "card_instance.h"
#include "card_type.h"

struct GameState;

enum class GameEvent
{
    HAND_CHANGED,
    GRAVEYARD_CHANGED,
};

int hand_scheduled_count(const GameState& state, int in_flight_deck_draws);
bool run_should_end(const GameState& state, int in_flight_deck_draws);
int deck_hud_display_count(const GameState& state, int in_flight_deck_draws);

bool hand_add_card(GameState& state, CardRef card, bool from_deck_draw = false,
                   bool miracle_auto_play = false);
bool hand_add_card(GameState& state, CardType type, bool from_deck_draw = false,
                   bool miracle_auto_play = false);
void hand_swap_cards(GameState& state, int first_card, int second_card);
void hand_remove_at_to_graveyard(GameState& state, int index, int& selected_card);
void hand_remove_at_to_graveyard_played(GameState& state, int index, int& selected_card);
void hand_remove_at_exiled(GameState& state, int index, int& selected_card);
void hand_remove_at_to_deck_top(GameState& state, int index, int& selected_card);
void hand_stash_played_card(GameState& state, int index, int& selected_card);
void finalize_held_played_card(GameState& state);
void trigger_discard_effect_if_any(GameState& state, CardType type);

void graveyard_push(GameState& state, CardRef card);
void graveyard_push(GameState& state, CardType type);
void graveyard_remove_at(GameState& state, int index);
void graveyard_swap_at(GameState& state, int first_index, int second_index);
void graveyard_clear(GameState& state);
void graveyard_apply_gravity(GameState& state);

void exile_push(GameState& state, CardRef card, bool from_graveyard = false);
void exile_push(GameState& state, CardType type, bool from_graveyard = false);

void necromancy_shuffle_graveyard_to_deck(GameState& state);
bool birds_find_return_run(const GameState& state, int& out_start, int& out_end);
void birds_return_run_to_deck(GameState& state, int return_start, int return_end);
void game_events_dispatch(GameState& state, GameEvent event);

void battle_stats_reset(GameState& state);
void battle_stat_record_exile(GameState& state);
void battle_stat_record_draw_to_hand(GameState& state);
void battle_stat_record_keyword_discard(GameState& state);
void battle_stat_record_cycle(GameState& state);
void battle_stat_record_ghost(GameState& state);

#endif
