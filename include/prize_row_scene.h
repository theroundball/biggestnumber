#ifndef PRIZE_ROW_SCENE_H
#define PRIZE_ROW_SCENE_H

#include "bn_seed_random.h"

#include "campaign_types.h"
#include "menu_scenes.h"
#include "save_data.h"

struct PrizeRowResult
{
    bool picked = false;
    PrizeOffer offer;
};

PrizeRowResult run_prize_row_scene(const char* title, const PrizeOffer* offers, int offer_count,
                                   const char* confirm_hint);

bool run_upgrade_target_scene(SaveData& save, PrizeOfferKind upgrade_kind, bn::seed_random& rng,
                              uint8_t& out_instance_id);

#endif
