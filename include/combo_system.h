#ifndef COMBO_SYSTEM_H
#define COMBO_SYSTEM_H

#include "combo_types.h"

#include "bn_array.h"

struct GameState;

struct ComboBarState
{
    uint8_t length = 0;
    uint8_t filled = 0;
};

// Combo matching + cinematic control. Depends on GameState; do not include this
// from the rules core header (game_state.h) — include combo_types.h there instead.

struct ComboDef
{
    const CardType* sequence;
    int length;
    int total_score_multiplier;
    uint8_t id;
    bool order_independent = false;
};

void combo_check_zone(GameState& state, ComboZone zone);
void combo_init_progress_availability(GameState& state);
[[nodiscard]] bool combo_any_progress_bar_enabled(const GameState& state);
void combo_collect_bar_states(const GameState& state, bn::array<ComboBarState, 3>& out);
int combo_ready_count(const GameState& state);
uint8_t combo_ready_id_by_ordinal(const GameState& state, int ordinal);
int combo_ready_multiplier(uint8_t combo_id);
CardType combo_ready_display_type(uint8_t combo_id);
bool combo_start_player_triggered(GameState& state, uint8_t combo_id);
bool combo_would_complete_in_graveyard_with(const GameState& state, uint8_t combo_id, CardType incoming);
void combo_cinematic_begin(GameState& state);
void combo_apply_score_bonus(GameState& state);
void combo_remove_resolved_cards(GameState& state, int& selected_card);
void combo_resolve(GameState& state, int& selected_card);
bool combo_resolves_to_exile(ComboZone zone);
bool try_start_combo_cinematic(GameState& state);
bool combo_start_cinematic_if_valid(GameState& state);
bool combo_try_start_pending(GameState& state);

#endif
