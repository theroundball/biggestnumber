#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include "bn_seed_random.h"
#include "bn_vector.h"

#include "campaign_types.h"
#include "card_instance.h"
#include "game_scene.h"
#include "save_data.h"

struct CampaignBattleSetup
{
    CampaignMode mode = CampaignMode::NONE;
    int peak_before = 0;
    int band_score = 0;
    int same_number_target = 0;
    int number_now_scoring_round = 1;
    int number_now_round_peak = 0;
};

bool campaign_needs_starter_setup(const SaveData& save);
bool campaign_create_starter_deck(SaveData& save, CardType utility_pick);

void campaign_prepare_same_number_target(SaveData& save, bn::seed_random& rng);
CampaignBattleSetup campaign_battle_setup(const SaveData& save, CampaignMode mode, bn::seed_random& rng);

bool campaign_evaluate_win(const SaveData& save, CampaignMode mode, const GameSceneResult& result,
                           int peak_before, int same_number_target, int number_now_round_peak,
                           int number_now_scoring_round);

void campaign_apply_win(SaveData& save, CampaignMode mode, const GameSceneResult& result,
                        int number_now_scoring_round, bn::seed_random& rng);

int campaign_wins_until_trinket(const SaveData& save);

void campaign_grant_sticker_paper(SaveData& save, int amount = 1);
bool campaign_spend_sticker_paper(SaveData& save, int amount);

void campaign_rebuild_instance_pool(SaveData& save);
void campaign_flatten_deck(const SaveData& save, int deck_index, bn::vector<CardRef, 50>& out);
void campaign_flatten_saved_deck(const SaveData& save, const SavedDeck& deck, bn::vector<CardRef, 50>& out);

bool campaign_apply_prize_card(SaveData& save, CardType type);
bool campaign_apply_prize_upgrade(SaveData& save, PrizeOfferKind kind, uint8_t instance_id,
                                  bn::seed_random& rng);
bool campaign_apply_prize_trinket(SaveData& save, TrinketType type);

int campaign_number_now_round_count(int deck_size);

void campaign_set_ephemeral_battle_deck(const SavedDeck& deck);
bool campaign_take_ephemeral_battle_deck(const SaveData& save, SavedDeck& out_deck);
void campaign_clear_ephemeral_battle_deck();

bool campaign_apply_sell_collection(SaveData& save, CardType nostalgia_card, CardType utility_pick);

int campaign_library_total_cards(const SaveData& save);

#endif
