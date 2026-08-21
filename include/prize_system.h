#ifndef PRIZE_SYSTEM_H
#define PRIZE_SYSTEM_H

#include "bn_seed_random.h"

#include "campaign_types.h"
#include "card_meta.h"
#include "save_data.h"

void prize_slot_rarities(CampaignMode mode, int peak_before, int band_score,
                          CardRarity out_slots[CAMPAIGN_PRIZE_SLOT_COUNT]);

CardType prize_combo_next(const SaveData& save);

bool prize_should_include_upgrade(const SaveData& save);

bool prize_build_offers(const SaveData& save, CampaignMode mode, int peak_before, int band_score,
                         bool include_upgrade, bn::seed_random& rng,
                         PrizeOffer out_offers[CAMPAIGN_PRIZE_SLOT_COUNT]);

TrinketType prize_roll_trinket(const SaveData& save, bn::seed_random& rng);

#endif
