#ifndef COMBO_SYSTEM_H
#define COMBO_SYSTEM_H

#include "combo_types.h"

#include "bn_string.h"
#include "bn_vector.h"

struct GameState;

struct ComboPipLine
{
    bn::string<24> text;
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
int combo_ready_count(const GameState& state);
uint8_t combo_ready_id_by_ordinal(const GameState& state, int ordinal);
int combo_ready_multiplier(uint8_t combo_id);
CardType combo_ready_display_type(uint8_t combo_id);
bool combo_start_player_triggered(GameState& state, uint8_t combo_id);
bool combo_would_complete_in_graveyard_with(const GameState& state, uint8_t combo_id, CardType incoming);
int combo_staging_collect_pip_lines(const GameState& state, bn::vector<ComboPipLine, 3>& out);
void combo_cinematic_begin(GameState& state);
void combo_apply_score_bonus(GameState& state);
void combo_remove_resolved_cards(GameState& state, int& selected_card);
void combo_resolve(GameState& state, int& selected_card);
bool combo_resolves_to_exile(ComboZone zone);
bool try_start_combo_cinematic(GameState& state);
bool combo_start_cinematic_if_valid(GameState& state);
bool combo_try_start_pending(GameState& state);

#endif
