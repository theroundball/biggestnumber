#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include <cstdint>

#include "bn_array.h"
#include "bn_vector.h"
#include "bn_string.h"
#include "card_meta.h"
#include "card_instance.h"
#include "card_type.h"
#include "game_state.h"

constexpr int SAVE_DATA_MAGIC = 0x424E554D; // 'BNUM'
constexpr int SAVE_DATA_VERSION = 16;
constexpr int MAX_SAVED_DECKS = 6;
constexpr int LIBRARY_COPY_LIMIT = 5;
constexpr int DECK_MIN_CARDS = 1;
constexpr int DECK_MAX_CARDS = 25;
// Saves written before the 25-card cap may hold larger decks. Loaders accept up to this
// many so the save still parses; save_data_validate then trims them to DECK_MAX_CARDS.
constexpr int DECK_LEGACY_MAX_CARDS = 50;
constexpr int CAMPAIGN_TRINKET_SLOTS = 2;
constexpr int CAMPAIGN_NUMBER_NOW_ROUNDS = 10;
constexpr int SAME_NUMBER_BRACKET_SIZE = 50;
constexpr int SAME_NUMBER_USED_CAPACITY = 5;
constexpr int CAMPAIGN_STICKER_PAPER_UPGRADE_COST = 10;
constexpr int POKER_HAND_RANK_COUNT = 9;

struct SavedDeck
{
    char name[16] = {};
    uint8_t counts[int(CardType::COUNT)] = {};
    int32_t highest_score = 0;
    uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
        uint8_t(TrinketType::NONE),
        uint8_t(TrinketType::NONE),
    };
    // 1 = sandbox/debug deck: full catalog, no library/per-type limits (max DECK_MAX_CARDS).
    uint8_t unrestricted_build = 0;
    uint8_t longsleeve_instance_ids[2] = {NO_INSTANCE, NO_INSTANCE};
};

struct SaveData
{
    uint32_t magic = 0;
    uint16_t version = 0;
    uint8_t deck_count = 0;
    uint8_t active_deck_index = 0;
  // 1 after starter deck (wheels + utility pick) exists.
    uint8_t campaign_ready = 0;
    uint8_t reserved_pad = 0;
    int32_t biggest_number_record = 0;
    int32_t total_wins = 0;
    int32_t same_number_wins = 0;
    int16_t same_number_target = 0;
    uint8_t same_number_used_count = 0;
    uint8_t reserved = 0;
    int16_t same_number_used_targets[SAME_NUMBER_USED_CAPACITY] = {};
    int32_t number_now_round_best[CAMPAIGN_NUMBER_NOW_ROUNDS] = {};
    uint8_t library_counts[int(CardType::COUNT)] = {};
    uint8_t trinket_owned[int(TrinketType::COUNT)] = {};
    InstancePool instance_pool{};
    SavedDeck decks[MAX_SAVED_DECKS] = {};
    uint16_t sticker_paper = 0;
    int32_t aint_got_time_record = 0;
    int32_t sharing_is_caring_record = 0;
    int32_t poker_hand_record[POKER_HAND_RANK_COUNT] = {};
};

void save_data_init();
SaveData& save_data_mut();
const SaveData& save_data_get();
void save_data_write();
void save_data_validate(SaveData& save);

SavedDeck saved_deck_make_new();
SavedDeck saved_deck_make_debug();
bool saved_deck_unrestricted_build(const SavedDeck& deck);
bool save_data_has_unrestricted_deck(const SaveData& save);
int save_data_unrestricted_deck_index(const SaveData& save);
void saved_deck_sanitize_name(SavedDeck& deck);
bool saved_deck_name_in_use(const SaveData& save, const bn::string_view& name, int ignore_deck_index = -1);
void saved_deck_assign_unique_name(SavedDeck& deck, const SaveData& save, int ignore_deck_index = -1);
bn::string<16> saved_deck_display_name(const SavedDeck& deck);
int saved_deck_total_cards(const SavedDeck& deck);
void saved_deck_trim_to_max(SavedDeck& deck, int max_cards);
bool saved_deck_can_add(const SavedDeck& deck, CardType type);
bool saved_deck_add_card(SavedDeck& deck, CardType type);
bool saved_deck_remove_card(SavedDeck& deck, CardType type);
bool saved_deck_try_add_card(const SaveData& save, int deck_index, SavedDeck& working, CardType type);
bool saved_deck_try_remove_card(SavedDeck& working, CardType type);
void saved_deck_flatten(const SavedDeck& deck, bn::vector<CardType, 50>& out);
void saved_deck_clear_longsleeves(SavedDeck& deck);
bool saved_deck_has_longsleeves_equipped(const SavedDeck& deck);
void saved_deck_resolve_longsleeve_cards(const SavedDeck& deck, const InstancePool& pool,
                                           bn::array<CardRef, 2>& out_cards);
bool saved_deck_try_update_high_score(SavedDeck& deck, int score);
int saved_deck_high_score(const SavedDeck& deck);

int library_total_owned(const SaveData& save, CardType type);
int library_count_in_decks(const SaveData& save, CardType type);
int library_available(const SaveData& save, CardType type);
int library_available_for_deck_edit(const SaveData& save, int deck_index, const SavedDeck& working,
                                    CardType type);
bool library_can_add(const SaveData& save, CardType type);
bool library_add_card(SaveData& save, CardType type);

bool campaign_is_ready(const SaveData& save);
void campaign_set_active_deck(SaveData& save, int deck_index);

#endif
