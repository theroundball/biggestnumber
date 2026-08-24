#include "card_meta.h"

namespace
{
    constexpr CardRarity max_rarity(CardRarity a, CardRarity b)
    {
        return static_cast<CardRarity>(
            static_cast<int>(a) > static_cast<int>(b) ? static_cast<int>(a) : static_cast<int>(b));
    }

    void fill_delta_slots(int delta, CardRarity out_slots[3])
    {
        // Testing-scale thresholds (peaks often <300 early). Re-tune later.
        if(delta >= 100)
        {
            out_slots[0] = CardRarity::COMMON;
            out_slots[1] = CardRarity::UNCOMMON;
            out_slots[2] = CardRarity::RARE;
        }
        else if(delta >= 30)
        {
            out_slots[0] = CardRarity::COMMON;
            out_slots[1] = CardRarity::COMMON;
            out_slots[2] = CardRarity::UNCOMMON;
        }
        else
        {
            out_slots[0] = CardRarity::COMMON;
            out_slots[1] = CardRarity::COMMON;
            out_slots[2] = CardRarity::COMMON;
        }
    }

    bool fill_baseline_slots(int run_peak_before, CardRarity out_slots[3])
    {
        if(run_peak_before >= 250)
        {
            out_slots[0] = CardRarity::COMMON;
            out_slots[1] = CardRarity::UNCOMMON;
            out_slots[2] = CardRarity::RARE;
            return true;
        }

        if(run_peak_before >= 100)
        {
            out_slots[0] = CardRarity::COMMON;
            out_slots[1] = CardRarity::COMMON;
            out_slots[2] = CardRarity::UNCOMMON;
            return true;
        }

        return false;
    }

    // Card rarity and copy limits (see docs/CAMPAIGN_IMPLEMENTATION_PLAN.md).
    constexpr CardMeta META_TABLE[] = {
        {CardRarity::UNCOMMON, 5}, // SIPS
        {CardRarity::COMMON, 5},   // LONGBOARD
        {CardRarity::COMMON, 5},   // HEELYS
        {CardRarity::COMMON, 5},   // SCOOTER
        {CardRarity::COMMON, 5},   // SKATEBOARD
        {CardRarity::COMMON, 5},   // ROLLER_BLADES
        {CardRarity::COMMON, 5},   // WAGON
        {CardRarity::COMMON, 5},   // STOLLER
        {CardRarity::UNCOMMON, 5}, // RIP_STICK
        {CardRarity::UNCOMMON, 5}, // BIKE
        {CardRarity::RARE, 5},     // CLOVER
        {CardRarity::RARE, 5},     // BIG_KUROSAWA_BURGER
        {CardRarity::RARE, 1},     // ROCK
        {CardRarity::RARE, 1},     // PAPER
        {CardRarity::RARE, 1},     // SCISSORS
        {CardRarity::RARE, 1},     // SHOOT
        {CardRarity::COMMON, 3},   // PEANUT_BUTTER
        {CardRarity::COMMON, 3},   // JELLY
        {CardRarity::UNCOMMON, 3}, // STRAW
        {CardRarity::UNCOMMON, 3}, // STICKS
        {CardRarity::UNCOMMON, 3}, // BRICKS
        {CardRarity::UNCOMMON, 1}, // LIFELINE
        {CardRarity::UNCOMMON, 5}, // SNAIL_MAIL
        {CardRarity::UNCOMMON, 2}, // WISHES
        {CardRarity::COMMON, 3},   // BUSTED
        {CardRarity::UNCOMMON, 1}, // SWIVEL
        {CardRarity::RARE, 1},     // ROUNDUP
        {CardRarity::RARE, 1},     // HACKER
        {CardRarity::RARE, 1},     // LIBRARIAN
        {CardRarity::UNCOMMON, 1}, // PILOT
        {CardRarity::RARE, 1},     // TURTLE_MODE
        {CardRarity::UNCOMMON, 2}, // TIME_IS_TOO_EXPENSIVE
        {CardRarity::UNCOMMON, 5}, // BIRDS_OF_A_FEATHER
        {CardRarity::RARE, 1},     // NECROMANCY
        {CardRarity::UNCOMMON, 5}, // MIRACLE
        {CardRarity::RARE, 1},     // RAGS_TO_RICHES
        {CardRarity::RARE, 1},     // JACKS
        {CardRarity::RARE, 1},     // FISHING_POLE
        {CardRarity::RARE, 1},     // CUPS
        {CardRarity::RARE, 1},     // SWAP
        {CardRarity::COMMON, 3},   // CATNIP
        {CardRarity::UNCOMMON, 5}, // JOURNAL
        {CardRarity::UNCOMMON, 5}, // TRIPTYCH
        {CardRarity::UNCOMMON, 2}, // SEEDS
        {CardRarity::UNCOMMON, 2}, // DILLA
        {CardRarity::RARE, 1},     // SEMAPHORE
        {CardRarity::COMMON, 3},   // BONES
        {CardRarity::UNCOMMON, 2}, // THRESHOLD
        {CardRarity::UNCOMMON, 2}, // TOMBSTONES
        {CardRarity::UNCOMMON, 2}, // ROLL_OVER
        {CardRarity::COMMON, 5},   // TOPPINGS
        {CardRarity::COMMON, 5},   // CYCLE
        {CardRarity::UNCOMMON, 3}, // CYCLE_SEVEN
        {CardRarity::UNCOMMON, 3}, // GET_ME_OUTA_HERE
        {CardRarity::UNCOMMON, 3}, // COMEBACK
        {CardRarity::UNCOMMON, 3}, // ENCORE
        {CardRarity::UNCOMMON, 2}, // DOUBLE_TIME
        {CardRarity::COMMON, 5},   // THE_FOURTH
        {CardRarity::COMMON, 5},   // PALINDROME
        {CardRarity::COMMON, 5},   // THE_FIFTH
        {CardRarity::UNCOMMON, 3}, // SOLO
        {CardRarity::RARE, 1},     // SPECULATIVE
        {CardRarity::RARE, 1},     // FLEX
        {CardRarity::UNCOMMON, 3}, // KEEP_GOING
        {CardRarity::UNCOMMON, 5}, // STAT_CYCLES_ADD
        {CardRarity::UNCOMMON, 5}, // STAT_CYCLES_MUL
        {CardRarity::UNCOMMON, 5}, // STAT_FLASH_ADD
        {CardRarity::UNCOMMON, 5}, // STAT_FLASH_MUL
        {CardRarity::UNCOMMON, 5}, // STAT_DRAWN_ADD
        {CardRarity::UNCOMMON, 5}, // STAT_DRAWN_MUL
        {CardRarity::UNCOMMON, 5}, // STAT_EXILE_ADD
        {CardRarity::UNCOMMON, 5}, // STAT_EXILE_MUL
        {CardRarity::UNCOMMON, 5}, // STAT_DISCARD_ADD
        {CardRarity::UNCOMMON, 5}, // STAT_DISCARD_MUL
    };

    static_assert(sizeof(META_TABLE) / sizeof(META_TABLE[0]) == int(CardType::COUNT),
                  "card_meta table out of sync with CardType");
}

const CardMeta& card_meta(CardType type)
{
    return META_TABLE[int(type)];
}

bool card_is_combo_piece(CardType type)
{
    switch(type)
    {
    case CardType::ROCK:
    case CardType::PAPER:
    case CardType::SCISSORS:
    case CardType::SHOOT:
    case CardType::PEANUT_BUTTER:
    case CardType::JELLY:
    case CardType::STRAW:
    case CardType::STICKS:
    case CardType::BRICKS:
        return true;
    default:
        return false;
    }
}

void drop_merged_slot_rarities(int run_peak_before, int battle_score, CardRarity out_slots[3])
{
    const int delta = battle_score - run_peak_before;
    CardRarity delta_slots[3];
    fill_delta_slots(delta, delta_slots);

    CardRarity baseline_slots[3];

    if(!fill_baseline_slots(run_peak_before, baseline_slots))
    {
        out_slots[0] = delta_slots[0];
        out_slots[1] = delta_slots[1];
        out_slots[2] = delta_slots[2];
        return;
    }

    out_slots[0] = max_rarity(baseline_slots[0], delta_slots[0]);
    out_slots[1] = max_rarity(baseline_slots[1], delta_slots[1]);
    out_slots[2] = max_rarity(baseline_slots[2], delta_slots[2]);
}

namespace
{
    int count_type_in_deck(const bn::vector<CardType, 50>& run_deck, CardType type)
    {
        int count = 0;

        for(int index = 0; index < run_deck.size(); ++index)
        {
            if(run_deck[index] == type)
            {
                ++count;
            }
        }

        return count;
    }

    bool drop_copy_limit_reached(const bn::vector<CardType, 50>& run_deck, CardType type)
    {
        if(card_is_combo_piece(type))
        {
            return count_type_in_deck(run_deck, type) >= 1;
        }

        return count_type_in_deck(run_deck, type) >= card_meta(type).max_copies;
    }

    bool collect_eligible(const bn::vector<CardType, 50>& run_deck, CardRarity rarity,
                          bn::vector<CardType, 64>& out)
    {
        out.clear();

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = static_cast<CardType>(type_index);
            const CardMeta& meta = card_meta(type);

            if(meta.max_copies == 0 || meta.rarity != rarity)
            {
                continue;
            }

            if(drop_copy_limit_reached(run_deck, type))
            {
                continue;
            }

            out.push_back(type);
        }

        return !out.empty();
    }

    CardType pick_for_slot(const bn::vector<CardType, 50>& run_deck, CardRarity slot_rarity,
                           bn::seed_random& rng, const CardType already[3], int already_count)
    {
        bn::vector<CardType, 64> pool;

        for(int step = 0; step < 3; ++step)
        {
            const int rarity_value = static_cast<int>(slot_rarity) - step;

            if(rarity_value < 0)
            {
                break;
            }

            if(!collect_eligible(run_deck, static_cast<CardRarity>(rarity_value), pool))
            {
                continue;
            }

            bn::vector<CardType, 64> unique_pool;

            for(int index = 0; index < pool.size(); ++index)
            {
                bool duplicate = false;

                for(int prior = 0; prior < already_count; ++prior)
                {
                    if(already[prior] == pool[index])
                    {
                        duplicate = true;
                        break;
                    }
                }

                if(!duplicate)
                {
                    unique_pool.push_back(pool[index]);
                }
            }

            const bn::vector<CardType, 64>& pick_from = unique_pool.empty() ? pool : unique_pool;
            return pick_from[rng.get_int(pick_from.size())];
        }

        return CardType::LONGBOARD;
    }
}

void roll_drop_offers(const bn::vector<CardType, 50>& run_deck, const CardRarity slots[3],
                      bn::seed_random& rng, CardType out_offers[3])
{
    for(int slot = 0; slot < 3; ++slot)
    {
        out_offers[slot] = pick_for_slot(run_deck, slots[slot], rng, out_offers, slot);
    }
}
