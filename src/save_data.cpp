#include "save_data.h"

#include "bn_sram.h"

#include "campaign.h"
#include "card_instance.h"
#include "card_meta.h"
#include "game_types.h"

namespace
{
    SaveData _save_data;

    constexpr int SAVE_DATA_VERSION_V8 = 8;
    constexpr int SAVE_DATA_VERSION_V9 = 9;
    constexpr int SAVE_DATA_VERSION_V10 = 10;
    constexpr int SAVE_DATA_CARD_COUNT_V10 = 63;
    constexpr int SAVE_DATA_TRINKET_COUNT_V10 = 6;
    static_assert(int(CardType::KEEP_GOING) == SAVE_DATA_CARD_COUNT_V10);
    static_assert(int(TrinketType::FIBONACCI) == SAVE_DATA_TRINKET_COUNT_V10);
    constexpr int SAVE_DATA_CARD_COUNT_V9 = 51;
    constexpr int SAVE_DATA_CARD_COUNT_V5 = 53;
    constexpr int SAVE_DATA_REMOVED_CARD_INDEX_V5 = 36;
    constexpr int SAVE_DATA_VERSION_V5 = 5;
    // Card roster size before One More Time was removed (CardType::ONE_MORE_TIME was index 36).
    constexpr int SAVE_DATA_CARD_COUNT_V7 = 52;
    constexpr int SAVE_DATA_REMOVED_CARD_INDEX_V7 = 36;
    constexpr int SAVE_DATA_VERSION_V7 = 7;
    constexpr int SAVE_DATA_VERSION_V6 = 6;

    void clear_same_number_used(SaveData& data)
    {
        data.same_number_used_count = 0;

        for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
        {
            data.same_number_used_targets[index] = 0;
        }
    }

    void seed_same_number_used_from_target(SaveData& data)
    {
        clear_same_number_used(data);

        if(data.same_number_target > 0)
        {
            data.same_number_used_targets[0] = data.same_number_target;
            data.same_number_used_count = 1;
        }
    }

    struct SavedDeckV5
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V5] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
    };

    struct SaveDataV5
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
        uint8_t campaign_ready = 0;
        uint8_t reserved = 0;
        int32_t biggest_number_record = 0;
        int32_t total_wins = 0;
        int32_t same_number_wins = 0;
        int16_t same_number_target = 0;
        int16_t reserved_score = 0;
        int32_t number_now_round_best[CAMPAIGN_NUMBER_NOW_ROUNDS] = {};
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V5] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV5 decks[MAX_SAVED_DECKS] = {};
    };

    // Pre–same-number used-target tracking (version 6). Fixed 52-card roster on disk.
    struct SavedDeckV6
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V7] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
    };

    struct SaveDataV6
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
        uint8_t campaign_ready = 0;
        uint8_t reserved = 0;
        int32_t biggest_number_record = 0;
        int32_t total_wins = 0;
        int32_t same_number_wins = 0;
        int16_t same_number_target = 0;
        int16_t reserved_score = 0;
        int32_t number_now_round_best[CAMPAIGN_NUMBER_NOW_ROUNDS] = {};
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V7] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV6 decks[MAX_SAVED_DECKS] = {};
    };

    struct SavedDeckV7
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V7] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
    };

    struct SaveDataV7
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
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
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V7] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV7 decks[MAX_SAVED_DECKS] = {};
    };

    int saved_deck_v7_total_cards(const SavedDeckV7& deck)
    {
        int total = 0;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V7; ++type_index)
        {
            total += deck.counts[type_index];
        }

        return total;
    }

    int saved_deck_v6_total_cards(const SavedDeckV6& deck)
    {
        int total = 0;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V7; ++type_index)
        {
            total += deck.counts[type_index];
        }

        return total;
    }

    void save_data_remap_counts_remove_one_more_time(const uint8_t* old_counts, uint8_t* new_counts)
    {
        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            new_counts[type_index] = 0;
        }

        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V7; ++type_index)
        {
            new_counts[type_index] = old_counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V7 + 1;
            type_index < SAVE_DATA_CARD_COUNT_V7; ++type_index)
        {
            new_counts[type_index - 1] = old_counts[type_index];
        }
    }

    void save_data_migrate_instance_pool_remove_one_more_time(InstancePool& pool)
    {
        uint8_t write = 0;

        for(uint8_t read = 0; read < pool.count; ++read)
        {
            const int old_base = int(pool.entries[read].base);

            if(old_base == SAVE_DATA_REMOVED_CARD_INDEX_V7)
            {
                continue;
            }

            pool.entries[write] = pool.entries[read];

            if(old_base > SAVE_DATA_REMOVED_CARD_INDEX_V7)
            {
                pool.entries[write].base = CardType(old_base - 1);
            }

            ++write;
        }

        pool.count = write;
    }

    void save_data_migrate_deck_v7_to_v8(SavedDeck& deck, const SavedDeckV7& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        deck.unrestricted_build = 0;

        save_data_remap_counts_remove_one_more_time(old_deck.counts, deck.counts);
    }

    void save_data_migrate_deck_v6_to_v8(SavedDeck& deck, const SavedDeckV6& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        deck.unrestricted_build = 0;

        save_data_remap_counts_remove_one_more_time(old_deck.counts, deck.counts);
    }

    struct SavedDeckV8
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V9] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
    };

    struct SaveDataV8
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
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
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V9] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV8 decks[MAX_SAVED_DECKS] = {};
    };

    struct SavedDeckV9
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V9] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
        uint8_t unrestricted_build = 0;
    };

    struct SaveDataV9
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
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
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V9] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV9 decks[MAX_SAVED_DECKS] = {};
    };

    struct SavedDeckV10
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V10] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
        uint8_t unrestricted_build = 0;
    };

    struct SaveDataV10
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
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
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V10] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        InstancePool instance_pool{};
        SavedDeckV10 decks[MAX_SAVED_DECKS] = {};
    };

    void save_data_migrate_deck_v8_to_v9(SavedDeck& deck, const SavedDeckV8& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        deck.unrestricted_build = 0;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_CARD_COUNT_V9; type_index < int(CardType::COUNT); ++type_index)
        {
            deck.counts[type_index] = 0;
        }
    }

    void save_data_migrate_v8_to_v9(SaveData& data, const SaveDataV8& old_data)
    {
        data.magic = old_data.magic;
        data.version = SAVE_DATA_VERSION;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved_pad;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        data.same_number_used_count = old_data.same_number_used_count;
        data.reserved = old_data.reserved;

        for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
        {
            data.same_number_used_targets[index] = old_data.same_number_used_targets[index];
        }

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            data.library_counts[type_index] = old_data.library_counts[type_index];
        }

        for(int type_index = SAVE_DATA_CARD_COUNT_V9; type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = 0;
        }

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        data.instance_pool = old_data.instance_pool;

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v8_to_v9(data.decks[deck_index], old_data.decks[deck_index]);
        }
    }

    void save_data_migrate_deck_v9_to_v10(SavedDeck& deck, const SavedDeckV9& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        deck.unrestricted_build = old_deck.unrestricted_build;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_CARD_COUNT_V9; type_index < int(CardType::COUNT); ++type_index)
        {
            deck.counts[type_index] = 0;
        }
    }

    void save_data_migrate_v9_to_v10(SaveData& data, const SaveDataV9& old_data)
    {
        data.magic = old_data.magic;
        data.version = SAVE_DATA_VERSION;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved_pad;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        data.same_number_used_count = old_data.same_number_used_count;
        data.reserved = old_data.reserved;

        for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
        {
            data.same_number_used_targets[index] = old_data.same_number_used_targets[index];
        }

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            data.library_counts[type_index] = old_data.library_counts[type_index];
        }

        for(int type_index = SAVE_DATA_CARD_COUNT_V9; type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = 0;
        }

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        data.instance_pool = old_data.instance_pool;

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v9_to_v10(data.decks[deck_index], old_data.decks[deck_index]);
        }
    }

    void save_data_migrate_deck_v10_to_v11(SavedDeck& deck, const SavedDeckV10& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;
        deck.unrestricted_build = old_deck.unrestricted_build;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V10; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_CARD_COUNT_V10; type_index < int(CardType::COUNT); ++type_index)
        {
            deck.counts[type_index] = 0;
        }
    }

    void save_data_migrate_v10_to_v11(SaveData& data, const SaveDataV10& old_data)
    {
        data = SaveData{};
        data.magic = old_data.magic;
        data.version = SAVE_DATA_VERSION;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved_pad;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        data.same_number_used_count = old_data.same_number_used_count;
        data.reserved = old_data.reserved;

        for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
        {
            data.same_number_used_targets[index] = old_data.same_number_used_targets[index];
        }

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V10; ++type_index)
        {
            data.library_counts[type_index] = old_data.library_counts[type_index];
        }

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        data.instance_pool = old_data.instance_pool;

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v10_to_v11(data.decks[deck_index], old_data.decks[deck_index]);
        }
    }

    // Card roster size before Gravedigger was removed (CardType::GRAVEDIGGER was index 36).
    constexpr int SAVE_DATA_CARD_COUNT_V2 = 42;
    constexpr int SAVE_DATA_REMOVED_CARD_INDEX_V2 = 36;

    struct SavedDeckV2
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V2] = {};
        int32_t highest_score = 0;
    };

    struct SaveDataV2
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t reserved = 0;
        SavedDeckV2 decks[MAX_SAVED_DECKS] = {};
    };

constexpr int SAVE_DATA_VERSION_V3 = 3;
constexpr int SAVE_DATA_CARD_COUNT_V3 = 52; // CardType::COUNT before TOPPINGS

    struct SavedDeckV3
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V3] = {};
        int32_t highest_score = 0;
    };

    struct SaveDataV3
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t reserved = 0;
        SavedDeckV3 decks[8] = {};
    };

    struct SavedDeckV4
    {
        char name[16] = {};
        uint8_t counts[SAVE_DATA_CARD_COUNT_V5] = {};
        int32_t highest_score = 0;
        uint8_t trinkets[CAMPAIGN_TRINKET_SLOTS] = {
            uint8_t(TrinketType::NONE),
            uint8_t(TrinketType::NONE),
        };
    };

    struct SaveDataV4
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint8_t deck_count = 0;
        uint8_t active_deck_index = 0;
        uint8_t campaign_ready = 0;
        uint8_t reserved = 0;
        int32_t biggest_number_record = 0;
        int32_t total_wins = 0;
        int32_t same_number_wins = 0;
        int16_t same_number_target = 0;
        int16_t reserved_score = 0;
        int32_t number_now_round_best[CAMPAIGN_NUMBER_NOW_ROUNDS] = {};
        uint8_t library_counts[SAVE_DATA_CARD_COUNT_V5] = {};
        uint8_t trinket_owned[SAVE_DATA_TRINKET_COUNT_V10] = {};
        SavedDeckV4 decks[MAX_SAVED_DECKS] = {};
    };

    void save_data_reset()
    {
        _save_data = SaveData{};
        _save_data.magic = SAVE_DATA_MAGIC;
        _save_data.version = SAVE_DATA_VERSION;
        _save_data.deck_count = 0;
    }

    bool save_data_valid(const SaveData& data)
    {
        if(data.magic != SAVE_DATA_MAGIC)
        {
            return false;
        }

        if(data.version != SAVE_DATA_VERSION)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const SavedDeck& deck = data.decks[deck_index];
            const int total = saved_deck_total_cards(deck);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }

            if(saved_deck_unrestricted_build(deck))
            {
                continue;
            }

            for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
            {
                const int in_deck = deck.counts[type_index];
                const int in_library = data.library_counts[type_index];

                if(in_deck > in_library || in_deck > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }
            }
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            if(data.library_counts[type_index] > LIBRARY_COPY_LIMIT)
            {
                return false;
            }
        }

        return true;
    }

    int saved_deck_v8_total_cards(const SavedDeckV8& deck)
    {
        int total = 0;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            total += deck.counts[type_index];
        }

        return total;
    }

    bool save_data_valid_v8(const SaveDataV8& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V8)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const int total = saved_deck_v8_total_cards(data.decks[deck_index]);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
            {
                const int in_deck = data.decks[deck_index].counts[type_index];
                const int in_library = data.library_counts[type_index];

                if(in_deck > in_library || in_deck > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }
            }
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            if(data.library_counts[type_index] > LIBRARY_COPY_LIMIT)
            {
                return false;
            }
        }

        return true;
    }

    int saved_deck_v9_total_cards(const SavedDeckV9& deck)
    {
        int total = 0;

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            total += deck.counts[type_index];
        }

        return total;
    }

    bool save_data_valid_v9(const SaveDataV9& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V9)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const int total = saved_deck_v9_total_cards(data.decks[deck_index]);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
            {
                const int in_deck = data.decks[deck_index].counts[type_index];
                const int in_library = data.library_counts[type_index];

                if(in_deck > in_library || in_deck > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }
            }
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V9; ++type_index)
        {
            if(data.library_counts[type_index] > LIBRARY_COPY_LIMIT)
            {
                return false;
            }
        }

        return true;
    }

    bool save_data_valid_v10(const SaveDataV10& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V10 ||
           data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const SavedDeckV10& deck = data.decks[deck_index];
            int total = 0;

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V10; ++type_index)
            {
                total += deck.counts[type_index];
            }

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }

            if(deck.unrestricted_build != 0)
            {
                continue;
            }

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V10; ++type_index)
            {
                const int in_deck = deck.counts[type_index];
                const int in_library = data.library_counts[type_index];

                if(in_deck > in_library || in_deck > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }
            }
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V10; ++type_index)
        {
            if(data.library_counts[type_index] > LIBRARY_COPY_LIMIT)
            {
                return false;
            }
        }

        return true;
    }

    bool save_data_valid_v4(const SaveDataV4& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != 4)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            int total = 0;

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
            {
                const int in_deck = data.decks[deck_index].counts[type_index];
                const int in_library = data.library_counts[type_index];

                if(in_deck > in_library || in_deck > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }

                total += in_deck;
            }

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }
        }

        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
        {
            if(data.library_counts[type_index] > LIBRARY_COPY_LIMIT)
            {
                return false;
            }
        }

        return true;
    }

    void save_data_migrate_v4_to_v5(SaveData& data, const SaveDataV4& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        seed_same_number_used_from_target(data);
        data.reserved = 0;

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = 0;
        }

        // V4 library used the pre-removal roster (53 slots with It's Comin' Up at 36).
        // Copy with that index dropped so later types stay aligned.
        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V5 &&
             type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = old_data.library_counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V5 + 2;
            type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
        {
            data.library_counts[type_index - 2] = old_data.library_counts[type_index];
        }

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            SavedDeck& deck = data.decks[deck_index];
            const SavedDeckV4& old_deck = old_data.decks[deck_index];

            for(int index = 0; index < 16; ++index)
            {
                deck.name[index] = old_deck.name[index];
            }

            deck.highest_score = old_deck.highest_score;

            for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
            {
                deck.trinkets[slot] = old_deck.trinkets[slot];
            }

            for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
            {
                deck.counts[type_index] = 0;
            }

            for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V5; ++type_index)
            {
                deck.counts[type_index] = old_deck.counts[type_index];
            }

            for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V5 + 2;
                type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
            {
                deck.counts[type_index - 2] = old_deck.counts[type_index];
            }
        }

        data.instance_pool = InstancePool{};
        data.version = SAVE_DATA_VERSION;
        campaign_rebuild_instance_pool(data);
    }

    bool save_data_valid_v5(const SaveDataV5& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V5)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.active_deck_index >= data.deck_count && data.deck_count > 0)
        {
            return false;
        }

        return true;
    }

    void save_data_migrate_deck_v5_to_v6(SavedDeck& deck, const SavedDeckV5& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = old_deck.trinkets[slot];
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            deck.counts[type_index] = 0;
        }

        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V5; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V5 + 2;
            type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
        {
            deck.counts[type_index - 2] = old_deck.counts[type_index];
        }
    }

    void save_data_migrate_v5_to_v6(SaveData& data, const SaveDataV5& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        seed_same_number_used_from_target(data);
        data.reserved = 0;

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = 0;
        }

        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V5; ++type_index)
        {
            data.library_counts[type_index] = old_data.library_counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V5 + 2;
            type_index < SAVE_DATA_CARD_COUNT_V5; ++type_index)
        {
            data.library_counts[type_index - 2] = old_data.library_counts[type_index];
        }

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v5_to_v6(data.decks[deck_index], old_data.decks[deck_index]);
        }

        data.instance_pool = InstancePool{};
        data.version = SAVE_DATA_VERSION;
        campaign_rebuild_instance_pool(data);
    }

    bool save_data_valid_v6(const SaveDataV6& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V6)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.deck_count > 0 && data.active_deck_index >= data.deck_count)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const int total = saved_deck_v6_total_cards(data.decks[deck_index]);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }
        }

        return true;
    }

    bool save_data_valid_v7(const SaveDataV7& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V7)
        {
            return false;
        }

        if(data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        if(data.deck_count > 0 && data.active_deck_index >= data.deck_count)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const int total = saved_deck_v7_total_cards(data.decks[deck_index]);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }
        }

        return true;
    }

    void save_data_migrate_v7_to_v8(SaveData& data, const SaveDataV7& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved_pad;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        data.same_number_used_count = old_data.same_number_used_count;
        data.reserved = old_data.reserved;

        for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
        {
            data.same_number_used_targets[index] = old_data.same_number_used_targets[index];
        }

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        save_data_remap_counts_remove_one_more_time(old_data.library_counts, data.library_counts);

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        data.instance_pool = old_data.instance_pool;
        save_data_migrate_instance_pool_remove_one_more_time(data.instance_pool);

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v7_to_v8(data.decks[deck_index], old_data.decks[deck_index]);
        }

        for(int deck_index = data.deck_count; deck_index < MAX_SAVED_DECKS; ++deck_index)
        {
            data.decks[deck_index] = SavedDeck{};
        }

        data.version = SAVE_DATA_VERSION;
    }

    void save_data_migrate_v6_to_v7(SaveData& data, const SaveDataV6& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count;
        data.active_deck_index = old_data.active_deck_index;
        data.campaign_ready = old_data.campaign_ready;
        data.reserved_pad = old_data.reserved;
        data.biggest_number_record = old_data.biggest_number_record;
        data.total_wins = old_data.total_wins;
        data.same_number_wins = old_data.same_number_wins;
        data.same_number_target = old_data.same_number_target;
        seed_same_number_used_from_target(data);
        data.reserved = 0;

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = old_data.number_now_round_best[round_index];
        }

        save_data_remap_counts_remove_one_more_time(old_data.library_counts, data.library_counts);

        for(int trinket_index = 0; trinket_index < SAVE_DATA_TRINKET_COUNT_V10; ++trinket_index)
        {
            data.trinket_owned[trinket_index] = old_data.trinket_owned[trinket_index];
        }

        data.instance_pool = old_data.instance_pool;
        save_data_migrate_instance_pool_remove_one_more_time(data.instance_pool);

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v6_to_v8(data.decks[deck_index], old_data.decks[deck_index]);
        }

        for(int deck_index = data.deck_count; deck_index < MAX_SAVED_DECKS; ++deck_index)
        {
            data.decks[deck_index] = SavedDeck{};
        }

        data.version = SAVE_DATA_VERSION;
    }

    bool save_data_valid_v3_decks(const SaveDataV3& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != SAVE_DATA_VERSION_V3 ||
           data.deck_count > 8)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            int total = 0;

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V3; ++type_index)
            {
                if(data.decks[deck_index].counts[type_index] > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }

                total += data.decks[deck_index].counts[type_index];
            }

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }
        }

        return true;
    }

    void save_data_migrate_deck_v3_to_v4(SavedDeck& deck, const SavedDeckV3& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            deck.counts[type_index] = 0;
        }

        // V3 roster included It's Comin' Up at index 36 and had no Toppings yet.
        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V5; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V5 + 1;
            type_index < SAVE_DATA_CARD_COUNT_V3; ++type_index)
        {
            deck.counts[type_index - 1] = old_deck.counts[type_index];
        }

        deck.counts[int(CardType::TOPPINGS)] = 0;

        for(int slot = 0; slot < CAMPAIGN_TRINKET_SLOTS; ++slot)
        {
            deck.trinkets[slot] = uint8_t(TrinketType::NONE);
        }
    }

    void save_data_migrate_v3_to_v4(SaveData& data, const SaveDataV3& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count > MAX_SAVED_DECKS ? MAX_SAVED_DECKS : old_data.deck_count;
        data.active_deck_index = 0;
        data.campaign_ready = data.deck_count > 0 ? 1 : 0;
        data.reserved_pad = 0;
        data.biggest_number_record = 0;
        data.total_wins = 0;
        data.same_number_wins = 0;
        data.same_number_target = 0;
        clear_same_number_used(data);
        data.instance_pool = InstancePool{};

        for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
        {
            data.number_now_round_best[round_index] = 0;
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            data.library_counts[type_index] = 0;
        }

        for(int trinket_index = 0; trinket_index < int(TrinketType::COUNT); ++trinket_index)
        {
            data.trinket_owned[trinket_index] = 0;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v3_to_v4(data.decks[deck_index], old_data.decks[deck_index]);

            if(data.decks[deck_index].highest_score > data.biggest_number_record)
            {
                data.biggest_number_record = data.decks[deck_index].highest_score;
            }

            for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
            {
                if(data.decks[deck_index].counts[type_index] > data.library_counts[type_index])
                {
                    data.library_counts[type_index] = data.decks[deck_index].counts[type_index];
                }
            }
        }

        data.version = SAVE_DATA_VERSION;
        campaign_rebuild_instance_pool(data);
    }

    bool save_data_valid_v1_decks(const SaveData& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != 1 || data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            const int total = saved_deck_total_cards(data.decks[deck_index]);

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }

            for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
            {
                if(data.decks[deck_index].counts[type_index] > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool save_data_valid_v2_decks(const SaveDataV2& data)
    {
        if(data.magic != SAVE_DATA_MAGIC || data.version != 2 || data.deck_count > MAX_SAVED_DECKS)
        {
            return false;
        }

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            int total = 0;

            for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V2; ++type_index)
            {
                if(data.decks[deck_index].counts[type_index] > LIBRARY_COPY_LIMIT)
                {
                    return false;
                }

                total += data.decks[deck_index].counts[type_index];
            }

            if(total < DECK_MIN_CARDS || total > DECK_LEGACY_MAX_CARDS)
            {
                return false;
            }
        }

        return true;
    }

    void save_data_migrate_deck_v2_to_v3(SavedDeckV3& deck, const SavedDeckV2& old_deck)
    {
        for(int index = 0; index < 16; ++index)
        {
            deck.name[index] = old_deck.name[index];
        }

        deck.highest_score = old_deck.highest_score;

        for(int type_index = 0; type_index < SAVE_DATA_REMOVED_CARD_INDEX_V2; ++type_index)
        {
            deck.counts[type_index] = old_deck.counts[type_index];
        }

        for(int type_index = SAVE_DATA_REMOVED_CARD_INDEX_V2 + 1;
            type_index < SAVE_DATA_CARD_COUNT_V2;
            ++type_index)
        {
            deck.counts[type_index - 1] = old_deck.counts[type_index];
        }
    }

    void save_data_migrate_v2_to_v3(SaveDataV3& data, const SaveDataV2& old_data)
    {
        data.magic = old_data.magic;
        data.deck_count = old_data.deck_count;
        data.reserved = old_data.reserved;

        for(int deck_index = 0; deck_index < data.deck_count; ++deck_index)
        {
            save_data_migrate_deck_v2_to_v3(data.decks[deck_index], old_data.decks[deck_index]);
        }

        data.version = SAVE_DATA_VERSION_V3;
    }
}

void save_data_init()
{
    SaveData loaded;
    bn::sram::read(loaded);

    if(save_data_valid(loaded))
    {
        _save_data = loaded;

        for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
        {
            saved_deck_sanitize_name(_save_data.decks[deck_index]);
        }
    }
    else
    {
        SaveDataV10 loaded_v10;
        bn::sram::read(loaded_v10);

        if(save_data_valid_v10(loaded_v10))
        {
            save_data_migrate_v10_to_v11(_save_data, loaded_v10);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV9 loaded_v9;
        bn::sram::read(loaded_v9);

        if(save_data_valid_v9(loaded_v9))
        {
            save_data_migrate_v9_to_v10(_save_data, loaded_v9);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV8 loaded_v8;
        bn::sram::read(loaded_v8);

        if(save_data_valid_v8(loaded_v8))
        {
            save_data_migrate_v8_to_v9(_save_data, loaded_v8);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV7 loaded_v7;
        bn::sram::read(loaded_v7);

        if(save_data_valid_v7(loaded_v7))
        {
            save_data_migrate_v7_to_v8(_save_data, loaded_v7);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV6 loaded_v6;
        bn::sram::read(loaded_v6);

        if(save_data_valid_v6(loaded_v6))
        {
            save_data_migrate_v6_to_v7(_save_data, loaded_v6);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV5 loaded_v5;
        bn::sram::read(loaded_v5);

        if(save_data_valid_v5(loaded_v5))
        {
            save_data_migrate_v5_to_v6(_save_data, loaded_v5);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
        SaveDataV4 loaded_v4;
        bn::sram::read(loaded_v4);

        if(save_data_valid_v4(loaded_v4))
        {
            save_data_migrate_v4_to_v5(_save_data, loaded_v4);

            for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
            {
                saved_deck_sanitize_name(_save_data.decks[deck_index]);
            }
        }
        else
        {
            SaveDataV3 loaded_v3;
            bn::sram::read(loaded_v3);

            if(save_data_valid_v3_decks(loaded_v3))
            {
                save_data_migrate_v3_to_v4(_save_data, loaded_v3);

                for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
                {
                    saved_deck_sanitize_name(_save_data.decks[deck_index]);
                }
            }
            else
            {
                SaveDataV2 loaded_v2;
                bn::sram::read(loaded_v2);

                if(save_data_valid_v2_decks(loaded_v2))
                {
                    SaveDataV3 as_v3;
                    save_data_migrate_v2_to_v3(as_v3, loaded_v2);
                    save_data_migrate_v3_to_v4(_save_data, as_v3);

                    for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
                    {
                        saved_deck_sanitize_name(_save_data.decks[deck_index]);
                    }
                }
                else if(save_data_valid_v1_decks(loaded))
                {
                    SaveDataV2 as_v2;
                    as_v2.magic = loaded.magic;
                    as_v2.version = 2;
                    as_v2.deck_count = loaded.deck_count;
                    as_v2.reserved = loaded.reserved;

                    for(int deck_index = 0; deck_index < loaded.deck_count; ++deck_index)
                    {
                        for(int index = 0; index < 16; ++index)
                        {
                            as_v2.decks[deck_index].name[index] = loaded.decks[deck_index].name[index];
                        }

                        as_v2.decks[deck_index].highest_score = 0;

                        for(int type_index = 0; type_index < SAVE_DATA_CARD_COUNT_V2; ++type_index)
                        {
                            as_v2.decks[deck_index].counts[type_index] = loaded.decks[deck_index].counts[type_index];
                        }
                    }

                    SaveDataV3 as_v3;
                    save_data_migrate_v2_to_v3(as_v3, as_v2);
                    save_data_migrate_v3_to_v4(_save_data, as_v3);

                    for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
                    {
                        saved_deck_sanitize_name(_save_data.decks[deck_index]);
                    }
                }
                else
                {
                    save_data_reset();
                }
            }
        }
        }
        }
        }
        }
        }
        }
    }

    save_data_validate(_save_data);
    save_data_write();
}

SaveData& save_data_mut()
{
    return _save_data;
}

const SaveData& save_data_get()
{
    return _save_data;
}

void save_data_write()
{
    save_data_validate(_save_data);

    for(int deck_index = 0; deck_index < _save_data.deck_count; ++deck_index)
    {
        saved_deck_sanitize_name(_save_data.decks[deck_index]);
    }

    bn::sram::write(_save_data);
}

SavedDeck saved_deck_make_new()
{
    SavedDeck deck;
    deck.name[0] = '\0';
    deck.unrestricted_build = 0;
    return deck;
}

SavedDeck saved_deck_make_debug()
{
    SavedDeck deck = saved_deck_make_new();
    deck.unrestricted_build = 1;

    const char debug_name[] = "Debug";
    constexpr int name_length = sizeof(debug_name) - 1;

    for(int index = 0; index < name_length; ++index)
    {
        deck.name[index] = debug_name[index];
    }

    deck.name[name_length] = '\0';
    return deck;
}

bool saved_deck_unrestricted_build(const SavedDeck& deck)
{
    return deck.unrestricted_build != 0;
}

bool save_data_has_unrestricted_deck(const SaveData& save)
{
    return save_data_unrestricted_deck_index(save) >= 0;
}

int save_data_unrestricted_deck_index(const SaveData& save)
{
    for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
    {
        if(saved_deck_unrestricted_build(save.decks[deck_index]))
        {
            return deck_index;
        }
    }

    return -1;
}

namespace
{
    bool saved_deck_name_characters_equal(char a, char b)
    {
        if(a >= 'A' && a <= 'Z')
        {
            a = char(a - 'A' + 'a');
        }

        if(b >= 'A' && b <= 'Z')
        {
            b = char(b - 'A' + 'a');
        }

        return a == b;
    }

    bool saved_deck_stored_name_equals(const SavedDeck& deck, const bn::string_view& name)
    {
        int index = 0;

        for(; index < 16 && deck.name[index] != '\0'; ++index)
        {
            if(index >= name.size())
            {
                return false;
            }

            if(! saved_deck_name_characters_equal(deck.name[index], name[index]))
            {
                return false;
            }
        }

        return index == name.size() && deck.name[index] == '\0';
    }
}

bool saved_deck_name_in_use(const SaveData& save, const bn::string_view& name, int ignore_deck_index)
{
    for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
    {
        if(deck_index == ignore_deck_index)
        {
            continue;
        }

        if(saved_deck_stored_name_equals(save.decks[deck_index], name))
        {
            return true;
        }
    }

    return false;
}

void saved_deck_assign_unique_name(SavedDeck& deck, const SaveData& save, int ignore_deck_index)
{
    const bn::string_view prefix = "deck";

    for(int number = 1; number < 1000; ++number)
    {
        bn::string<16> candidate;
        candidate.append(prefix);
        candidate.append(bn::to_string<4>(number));

        if(! saved_deck_name_in_use(save, candidate, ignore_deck_index))
        {
            for(int index = 0; index < candidate.size() && index < 15; ++index)
            {
                deck.name[index] = candidate[index];
            }

            deck.name[candidate.size() < 15 ? candidate.size() : 15] = '\0';
            saved_deck_sanitize_name(deck);
            return;
        }
    }

    deck.name[0] = 'd';
    deck.name[1] = 'e';
    deck.name[2] = 'c';
    deck.name[3] = 'k';
    deck.name[4] = '\0';
}

void saved_deck_sanitize_name(SavedDeck& deck)
{
    deck.name[15] = '\0';

    int write_index = 0;

    for(int read_index = 0; read_index < 16 && deck.name[read_index] != '\0'; ++read_index)
    {
        const unsigned char character = deck.name[read_index];

        if(character >= ' ' && character <= '~')
        {
            deck.name[write_index++] = char(character);
        }
    }

    deck.name[write_index] = '\0';

    if(write_index == 0)
    {
        deck.name[0] = 'd';
        deck.name[1] = 'e';
        deck.name[2] = 'c';
        deck.name[3] = 'k';
        deck.name[4] = '\0';
    }
}

bn::string<16> saved_deck_display_name(const SavedDeck& deck)
{
    bn::string<16> name;

    for(int index = 0; index < 16 && deck.name[index] != '\0'; ++index)
    {
        const unsigned char character = deck.name[index];

        if(character >= ' ' && character <= '~')
        {
            name.push_back(char(character));
        }
    }

    if(name.empty())
    {
        name = "deck";
    }

    return name;
}

int saved_deck_total_cards(const SavedDeck& deck)
{
    int total = 0;

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        total += deck.counts[type_index];
    }

    return total;
}

bool saved_deck_can_add(const SavedDeck& deck, CardType type)
{
    if(saved_deck_total_cards(deck) >= DECK_MAX_CARDS)
    {
        return false;
    }

    const int max_copies = card_meta(type).max_copies;

    if(max_copies <= 0)
    {
        return false;
    }

    if(saved_deck_unrestricted_build(deck))
    {
        return true;
    }

    return deck.counts[int(type)] < max_copies;
}

bool saved_deck_add_card(SavedDeck& deck, CardType type)
{
    if(! saved_deck_can_add(deck, type))
    {
        return false;
    }

    ++deck.counts[int(type)];
    return true;
}

bool saved_deck_remove_card(SavedDeck& deck, CardType type)
{
    if(deck.counts[int(type)] == 0)
    {
        return false;
    }

    --deck.counts[int(type)];
    return true;
}

int library_available_for_deck_edit(const SaveData& save, int deck_index, const SavedDeck& working,
                                    CardType type)
{
    (void)deck_index;
    const int owned = library_total_owned(save, type);
    const int in_deck = working.counts[int(type)];

    return owned > in_deck ? owned - in_deck : 0;
}

bool saved_deck_try_add_card(const SaveData& save, int deck_index, SavedDeck& working, CardType type)
{
    if(!saved_deck_can_add(working, type))
    {
        return false;
    }

    // Each deck may use up to library-owned copies; decks do not share a pool.
    if(!saved_deck_unrestricted_build(working) &&
       library_available_for_deck_edit(save, deck_index, working, type) <= 0)
    {
        return false;
    }

    ++working.counts[int(type)];
    return true;
}

bool saved_deck_try_remove_card(SavedDeck& working, CardType type)
{
    return saved_deck_remove_card(working, type);
}

void saved_deck_flatten(const SavedDeck& deck, bn::vector<CardType, 50>& out)
{
    out.clear();

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        for(int copy = 0; copy < deck.counts[type_index]; ++copy)
        {
            out.push_back(CardType(type_index));
        }
    }
}

bool saved_deck_try_update_high_score(SavedDeck& deck, int score)
{
    if(score <= deck.highest_score)
    {
        return false;
    }

    deck.highest_score = score;
    return true;
}

int saved_deck_high_score(const SavedDeck& deck)
{
    return deck.highest_score;
}

int library_total_owned(const SaveData& save, CardType type)
{
    return save.library_counts[int(type)];
}

int library_count_in_decks(const SaveData& save, CardType type)
{
    int total = 0;

    for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
    {
        total += save.decks[deck_index].counts[int(type)];
    }

    return total;
}

int library_available(const SaveData& save, CardType type)
{
    // Legacy helper: remaining copies that can still be added to a single empty deck slot.
    return library_total_owned(save, type);
}

bool library_can_add(const SaveData& save, CardType type)
{
    const int max_copies = card_meta(type).max_copies;

    if(max_copies <= 0)
    {
        return false;
    }

    return library_total_owned(save, type) < max_copies;
}

bool library_add_card(SaveData& save, CardType type)
{
    if(!library_can_add(save, type))
    {
        return false;
    }

    if(instance_pool_add(save.instance_pool, type) == NO_INSTANCE)
    {
        return false;
    }

    ++save.library_counts[int(type)];
    return true;
}

bool campaign_is_ready(const SaveData& save)
{
    return save.campaign_ready != 0 && save.deck_count > 0;
}

void campaign_set_active_deck(SaveData& save, int deck_index)
{
    if(deck_index >= 0 && deck_index < save.deck_count)
    {
        save.active_deck_index = uint8_t(deck_index);
    }
}

void save_data_validate(SaveData& save)
{
    if(save.deck_count > MAX_SAVED_DECKS)
    {
        save.deck_count = MAX_SAVED_DECKS;
    }

    if(save.deck_count == 0)
    {
        save.active_deck_index = 0;
        save.campaign_ready = 0;
    }
    else if(save.active_deck_index >= save.deck_count)
    {
        save.active_deck_index = 0;
    }

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        const CardType type = CardType(type_index);
        const int max_owned = card_meta(type).max_copies;

        if(max_owned <= 0)
        {
            save.library_counts[type_index] = 0;
        }
        else if(save.library_counts[type_index] > max_owned)
        {
            save.library_counts[type_index] = uint8_t(max_owned);
        }

        for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
        {
            SavedDeck& deck = save.decks[deck_index];

            if(saved_deck_unrestricted_build(deck))
            {
                continue;
            }

            if(deck.counts[type_index] > max_owned)
            {
                deck.counts[type_index] = uint8_t(max_owned);
            }
        }
    }

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        const int owned = save.library_counts[type_index];

        for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
        {
            SavedDeck& deck = save.decks[deck_index];

            if(saved_deck_unrestricted_build(deck))
            {
                continue;
            }

            if(deck.counts[type_index] > owned)
            {
                deck.counts[type_index] = uint8_t(owned);
            }
        }
    }

    for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
    {
        SavedDeck& deck = save.decks[deck_index];
        int total = saved_deck_total_cards(deck);

        while(total > DECK_MAX_CARDS)
        {
            for(int type_index = int(CardType::COUNT) - 1; type_index >= 0 && total > DECK_MAX_CARDS; --type_index)
            {
                if(deck.counts[type_index] > 0)
                {
                    --deck.counts[type_index];
                    --total;
                }
            }
        }

        saved_deck_sanitize_name(deck);
    }

    if(save.same_number_wins < 0)
    {
        save.same_number_wins = 0;
    }

    if(save.same_number_used_count > SAME_NUMBER_USED_CAPACITY)
    {
        save.same_number_used_count = SAME_NUMBER_USED_CAPACITY;
    }

    if(save.total_wins < 0)
    {
        save.total_wins = 0;
    }

    if(save.biggest_number_record < 0)
    {
        save.biggest_number_record = 0;
    }

    for(int round_index = 0; round_index < 10; ++round_index)
    {
        if(save.number_now_round_best[round_index] < 0)
        {
            save.number_now_round_best[round_index] = 0;
        }
    }

    for(int trinket_index = 0; trinket_index < int(TrinketType::COUNT); ++trinket_index)
    {
        if(save.trinket_owned[trinket_index] > 1)
        {
            save.trinket_owned[trinket_index] = 1;
        }
    }

    for(int deck_index = 0; deck_index < save.deck_count; ++deck_index)
    {
        SavedDeck& deck = save.decks[deck_index];

        for(int slot = 0; slot < 2; ++slot)
        {
            if(deck.trinkets[slot] >= int(TrinketType::COUNT))
            {
                deck.trinkets[slot] = 0;
            }
        }
    }

    int pool_instances_by_type[int(CardType::COUNT)] = {};

    for(uint8_t id = 0; id < save.instance_pool.count; ++id)
    {
        const CardType type = save.instance_pool.entries[id].base;

        if(int(type) >= 0 && int(type) < int(CardType::COUNT))
        {
            ++pool_instances_by_type[int(type)];
        }
    }

    bool pool_matches_library = true;

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        if(pool_instances_by_type[type_index] != save.library_counts[type_index])
        {
            pool_matches_library = false;
            break;
        }
    }

    // Never shrink the library to fit a stale/full instance pool — rebuild the pool
    // from library ownership instead (sell-collection and prize writes depend on this).
    if(!pool_matches_library)
    {
        instance_pool_clear(save.instance_pool);

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = CardType(type_index);
            const int owned = save.library_counts[type_index];

            for(int copy = 0; copy < owned; ++copy)
            {
                if(instance_pool_add(save.instance_pool, type) == NO_INSTANCE)
                {
                    break;
                }
            }
        }
    }

    if(save.instance_pool.count > InstancePool::CAPACITY)
    {
        save.instance_pool.count = InstancePool::CAPACITY;
    }
}
