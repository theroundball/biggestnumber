#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include "bn_seed_random.h"
#include "bn_vector.h"

#include "campaign_types.h"
#include "card_instance.h"
#include "game_scene.h"
#include "save_data.h"
#include "world_data.h"

struct CampaignBattleSetup
{
    CampaignMode mode = CampaignMode::NONE;
    int peak_before = 0;
    int band_score = 0;
    int same_number_target = 0;
    int number_now_scoring_round = 1;
    int number_now_round_peak = 0;
    int npc_index = -1;
};

bool campaign_needs_starter_setup(const SaveData& save);
bool campaign_create_starter_deck(SaveData& save, CardType utility_pick);
void campaign_repair_active_deck_from_library(SaveData& save);

void campaign_prepare_same_number_target(SaveData& save, bn::seed_random& rng);
CampaignBattleSetup campaign_battle_setup(const SaveData& save, CampaignMode mode, bn::seed_random& rng,
                                          int battle_deck_size = -1, int npc_index = -1);

bool campaign_evaluate_win(const SaveData& save, CampaignMode mode, const GameSceneResult& result,
                           int peak_before, int same_number_target, int number_now_round_peak,
                           int number_now_scoring_round);

void campaign_apply_win(SaveData& save, CampaignMode mode, const GameSceneResult& result,
                        int number_now_scoring_round, bn::seed_random& rng, int npc_index = -1);

int campaign_npc_best_score(const SaveData& save, int npc_index);
void campaign_seed_npc_best_scores(SaveData& save);
void campaign_clear_npc_best_scores(SaveData& save);

constexpr int NPC_BENCHMARK_COUNT = 12;
constexpr int NPC_BENCHMARKS[NPC_BENCHMARK_COUNT] = {
    10, 20, 35, 55, 90, 140, 220, 340, 500, 700, 850, 1000,
};

int campaign_npc_benchmark_taken(const SaveData& save, int npc_index);
int campaign_npc_benchmark_rung(const SaveData& save, int npc_index);
int campaign_npc_next_benchmark_rung(const SaveData& save, int npc_index);

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

bool campaign_apply_sell_collection(SaveData& save, CardType nostalgia_card, CardType utility_pick);

int campaign_library_total_cards(const SaveData& save);

// Sell Collection requires at least one copy of every collectible card type.
int campaign_collection_required_count();
int campaign_collection_unique_owned(const SaveData& save);
bool campaign_collection_complete(const SaveData& save);

void campaign_init_npc_collections(SaveData& save);
void campaign_sanitize_npc_collections(SaveData& save);
int campaign_npc_card_count(const SaveData& save, int npc_index, CardType type);
int campaign_npc_total_cards(const SaveData& save, int npc_index);
bool campaign_npc_has_card(const SaveData& save, int npc_index, CardType type);
bool campaign_npc_remove_card(SaveData& save, int npc_index, CardType type);
void campaign_flatten_npc_loaner(const SaveData& save, int npc_index, bn::vector<CardRef, 50>& out);
int campaign_npc_collectible_cards(const SaveData& save, int npc_index, bn::vector<CardType, NPC_MAX_COLLECTION_CARDS>& out);
bool campaign_npc_has_takeable_card(const SaveData& save, int npc_index);
int campaign_npc_first_takeable_index(const SaveData& save);
bool campaign_apply_npc_card_take(SaveData& save, int npc_index, CardType type);

#endif
