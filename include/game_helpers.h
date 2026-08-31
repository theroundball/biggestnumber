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

enum class CardStatColor : uint8_t
{
    DEFAULT,
    GREEN,
    GOLD,
};

struct CardFaceStatSegment
{
    bn::string<8> text;
    CardStatColor color = CardStatColor::DEFAULT;
};

void format_card_face_stat(const GameState* state, CardRef ref, const CardInstance* instance,
                           bool in_graveyard, bn::vector<CardFaceStatSegment, 4>& out);

bool selection_sends_to_library(PendingActionType type);

int initial_graveyard_cursor(const GameState& state, CardType exclude);
int clamp_graveyard_cursor(int cursor, int graveyard_size);
int advance_graveyard_cursor(const GameState& state, int cursor, int direction, CardType exclude);

bool card_has_play_effect(CardType type);
bool card_has_play_effect(const GameState& state, CardRef card);
bool card_has_discard_effect(CardType type);
bool card_has_cycle(CardType type);
bool card_has_ghost(CardType type);
bool card_increases_current_number(CardType type);
bool card_increases_current_number(const GameState& state, CardRef card);
bool card_can_add_or_multiply(CardType type);
bool card_can_add_or_multiply(const GameState& state, CardRef card);
bool mill_reveal_card_hits(const GameState& state, CardRef card, bool waterfall);
bool waterfall_would_make_bigger(const GameState& state, CardRef card);
int card_preview_plus(const GameState& state, CardRef card, bool ghost);
int ghost_count(const GameState& state);
bool has_optional_ghost_plays(const GameState& state);
bool playable_slot_is_combine_offer(const GameState& state, int visual_index);
int playable_slot_combine_ordinal(const GameState& state, int visual_index);
bool empty_hand_triggers_round_end(const GameState& state);
void try_finish_roll_over_substitution(GameState& state, int* selected_card);
bool roll_over_pick_active(const GameState& state);
bool begin_roll_over_substitution(GameState& state, int* selected_card);
void roll_over_restore_stashed_hand(GameState& state);
void roll_over_finish_sequence(GameState& state, int* selected_card);
int playable_slot_count(const GameState& state);
int longsleeve_count(const GameState& state);
bool playable_slot_is_longsleeve(const GameState& state, int visual_index);
int playable_slot_longsleeve_index(const GameState& state, int visual_index);
bool playable_slot_is_ghost(const GameState& state, int visual_index);
int playable_slot_hand_index(const GameState& state, int visual_index);
int playable_slot_graveyard_index(const GameState& state, int visual_index);
CardRef playable_slot_card(const GameState& state, int visual_index);
void build_a_number_try_queue_digit_placement(GameState& state, CardRef card, PlaySource source);
int build_a_number_card_digit(const GameState& state, CardRef card, PlaySource source);
bool build_a_number_can_play_card(const GameState& state, CardRef card, PlaySource source);
void bind_bounty_copy(GameState& state, CardRef& card);
int bounty_instance_play_count(const GameState& state, CardRef card);
int bounty_play_plus(const GameState& state);
int bounty_play_plus_for_card(const GameState& state, CardRef card);
int bounty_increment_play(GameState& state, CardRef card);
void bounty_update_return_threshold_for_plays(GameState& state, CardRef card);
void sync_bounty_card_overlay(const GameState& state, Card& card, CardRef ref, bool in_graveyard,
                              bn::sprite_text_generator* generator);
struct DigitMovePlan
{
    int source_digit_index = -1;
    int dest_digit_index = -1;
    int new_total = 0;
};

bool plan_minor_fall(int score, DigitMovePlan& plan);
bool plan_major_lift(int score, DigitMovePlan& plan);
bool plan_palindrome_wrap(int score, bool applying_double_adds, int& out_new_score);
bool try_palindrome_play(GameState& state);
bool try_minor_fall(GameState& state);
bool try_major_lift(GameState& state);
int bounty_return_threshold_for(const GameState& state, CardRef card);
int bounty_return_progress_for(const GameState& state, CardRef card);
void bounty_on_enter_graveyard(GameState& state, CardRef card);
void bounty_on_round_start(GameState& state);
void check_bounty_return(GameState& state);
void queue_effect_draw(GameState& state, int count, bool miracle_on_first);
bool try_draw_one_to_hand(GameState& state);
void maybe_draw_if_solo(GameState& state, CardType type);
void play_miracle_bonus(GameState& state, int amount);
void finish_played_card_from_hand(int hand_index, int& selected_card, GameState& state);
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
                              int visible_window = 0,
                              const GameState* bounty_overlay_state = nullptr,
                              bool bounty_graveyard_overlay = false);

bool graveyard_cursor_screen_position(int cursor, int graveyard_size, int spacing, int y, int selected_raise,
                                      int scroll_x, int main_x, int& out_x, int& out_y);

void deal_next_hand(Deck& deck, GameState& state, int& selected_card);
unsigned make_battle_random_seed(const bn::vector<CardType, 50>& collection);
unsigned make_battle_random_seed(const bn::vector<CardRef, 50>& collection);
Deck build_battle_deck(const bn::vector<CardRef, 50>& collection, bn::seed_random& random_engine,
                       const InstancePool& pool);
Deck build_battle_deck(const bn::vector<CardType, 50>& collection, bn::seed_random& random_engine);

#endif
