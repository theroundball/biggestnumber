#ifndef OVERWORLD_SCENE_H
#define OVERWORLD_SCENE_H

#include "bn_fixed.h"

#include "campaign_types.h"

enum class OverworldSceneResult
{
    STAY,
    OPEN_PLAY_MENU,
    START_NPC_BATTLE,
};

struct OverworldNpcBattleRequest
{
    CampaignMode mode = CampaignMode::NONE;
    int npc_index = -1;
    bool use_loaner_deck = false;
};

bool overworld_take_pending_npc_battle(OverworldNpcBattleRequest& out_request);

// Lower on screen (larger world Y) sorts in front for top-down overlap.
// Butano draws higher z_order first (behind), so larger Y must map to lower z.
// Bucket Y into a small z range so we stay within BN_CFG_SPRITES_MAX_SORT_LAYERS.
inline int overworld_depth_z_order(bn::fixed world_y)
{
    constexpr int Z_BEHIND = -8;
    constexpr int Z_IN_FRONT = -24;
    constexpr int MAP_MIN_Y = 16;
    constexpr int MAP_MAX_Y = 240;

    int y = world_y.integer();

    if(y < MAP_MIN_Y)
    {
        y = MAP_MIN_Y;
    }
    else if(y > MAP_MAX_Y)
    {
        y = MAP_MAX_Y;
    }

    const int span = MAP_MAX_Y - MAP_MIN_Y;
    const int z_span = Z_IN_FRONT - Z_BEHIND;
    return Z_BEHIND + (y - MAP_MIN_Y) * z_span / span;
}

OverworldSceneResult run_overworld_scene();

#endif
