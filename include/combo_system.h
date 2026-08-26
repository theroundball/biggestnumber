#ifndef COMBO_SYSTEM_H
#define COMBO_SYSTEM_H

#include "combo_types.h"

struct GameState;

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
void combo_cinematic_begin(GameState& state);
void combo_apply_score_bonus(GameState& state);
void combo_remove_resolved_cards(GameState& state, int& selected_card);
void combo_resolve(GameState& state, int& selected_card);
bool combo_resolves_to_exile(ComboZone zone);
bool try_start_combo_cinematic(GameState& state);
bool combo_start_cinematic_if_valid(GameState& state);
bool combo_try_start_pending(GameState& state);

#endif
