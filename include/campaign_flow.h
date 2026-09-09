#ifndef CAMPAIGN_FLOW_H
#define CAMPAIGN_FLOW_H

#include "bn_seed_random.h"

#include "campaign_types.h"

// Campaign play loop: starter setup, play submenu, battles, prizes.
void campaign_run_play_flow(bn::seed_random& rng);

// One battle from the play submenu; loot spawns on the overworld instead of the prize scene.
void campaign_run_overworld_play_flow(bn::seed_random& rng);

// One overworld-triggered battle (e.g. NPC); skips the play submenu.
void campaign_run_overworld_battle(bn::seed_random& rng, CampaignMode mode);

#endif
