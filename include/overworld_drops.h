#ifndef OVERWORLD_DROPS_H
#define OVERWORLD_DROPS_H

#include "bn_fixed.h"
#include "bn_fixed_point.h"
#include "bn_seed_random.h"

#include "campaign_types.h"

// Queue loot after an overworld battle (call before returning to the hub).
void overworld_drops_set_spawn(bn::fixed world_x, bn::fixed world_y);
void overworld_drops_clear_entity_blocks();
void overworld_drops_add_entity_block(bn::fixed world_x, bn::fixed world_y, bn::fixed half_w,
                                      bn::fixed half_h);
void overworld_drops_queue_from_battle(CampaignMode mode, bool won, int peak_before, int band_score,
                                       bn::seed_random& rng, int npc_index = -1);

bool overworld_drops_active();
bool overworld_drops_inspect_open();
bool overworld_drops_has_selection();

// Returns true when the overworld should skip normal movement/input this frame.
bool overworld_drops_tick(bn::fixed player_x, bn::fixed player_y, const bn::fixed_point& camera);

void overworld_drops_clear();

#endif
