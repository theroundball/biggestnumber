#ifndef RUN_SCENES_H
#define RUN_SCENES_H

#include "bn_array.h"
#include "bn_seed_random.h"

#include "game_state.h"
#include "menu_scenes.h"
#include "run_state.h"

// Pick up to 3 trinkets before the run. Start confirms; B cancels to main menu.
MenuSceneResult run_trinket_pick_scene(bn::array<TrinketType, 3>& out_trinkets);

// Between-fight status (score vs peak / miss streak). A/B continues.
MenuSceneResult run_between_fights_scene(const RunState& run, int battle_score, bool new_peak);

// Three drop offers as a card row; Select/Down inspects. Returns MAIN_MENU if aborted (B).
MenuSceneResult run_drop_pick_scene(RunState& run, int peak_before, int battle_score,
                                    bn::seed_random& rng);

// Upgrade node: remove, +digit, ×2, Lead, or Yeast. Needs rng for +digit rolls.
// B skips the node (run continues).
MenuSceneResult run_upgrade_node_scene(RunState& run, bn::seed_random& rng);

// After every 5 battles: pick 1 of 2 random unequipped trinkets (replace a slot if full).
// B skips.
MenuSceneResult run_trinket_offer_scene(RunState& run, bn::seed_random& rng);

// Run over — show peak. Any button → main menu.
MenuSceneResult run_run_over_scene(const RunState& run);

#endif
