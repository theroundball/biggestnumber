#ifndef WORLD_DATA_H
#define WORLD_DATA_H

#include "bn_fixed.h"

#include "campaign_types.h"
#include "card_type.h"

constexpr int WORLD_NPC_COUNT = 8;
constexpr int NPC_MAX_COLLECTION_CARDS = 13;

struct NpcDef
{
    const char* name = "";
    CampaignMode mode = CampaignMode::NONE;
    bn::fixed x = 0;
    bn::fixed y = 0;
    const CardType* collection = nullptr;
    int collection_count = 0;
};

const NpcDef& world_npc_def(int npc_index);
int world_npc_default_collection_count(int npc_index, CardType type);

#endif
