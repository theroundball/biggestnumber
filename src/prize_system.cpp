#include "prize_system.h"

#include "bn_vector.h"

namespace
{
    uint8_t prize_cycle_offered[int(CardType::COUNT)] = {};

    bool prize_collection_eligible(const SaveData& save, CardType type)
    {
        const CardMeta& meta = card_meta(type);

        if(meta.max_copies == 0)
        {
            return false;
        }

        if(card_is_combo_piece(type))
        {
            return library_total_owned(save, type) < 1;
        }

        return library_total_owned(save, type) < meta.max_copies;
    }

    void prize_cycle_reset()
    {
        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            prize_cycle_offered[type_index] = 0;
        }
    }

    void prize_build_pool(const SaveData& save, bool respect_cycle, const CardType already_offered[3],
                          int offered_count, bn::vector<CardType, int(CardType::COUNT)>& out)
    {
        out.clear();

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = static_cast<CardType>(type_index);

            if(!prize_collection_eligible(save, type))
            {
                continue;
            }

            if(respect_cycle && prize_cycle_offered[type_index] != 0)
            {
                continue;
            }

            bool duplicate = false;

            for(int prior = 0; prior < offered_count; ++prior)
            {
                if(already_offered[prior] == type)
                {
                    duplicate = true;
                    break;
                }
            }

            if(duplicate)
            {
                continue;
            }

            out.push_back(type);
        }
    }

    CardType pick_prize_card(const SaveData& save, bn::seed_random& rng, const CardType already_offered[3],
                             int offered_count)
    {
        bn::vector<CardType, int(CardType::COUNT)> pool;
        prize_build_pool(save, true, already_offered, offered_count, pool);

        if(pool.empty())
        {
            prize_cycle_reset();
            prize_build_pool(save, false, already_offered, offered_count, pool);
        }

        if(pool.empty())
        {
            return CardType::LONGBOARD;
        }

        const CardType picked = pool[rng.get_int(pool.size())];
        prize_cycle_offered[int(picked)] = 1;
        return picked;
    }
}

void prize_testing_cycle_reset()
{
    prize_cycle_reset();
}

void prize_slot_rarities(CampaignMode mode, int peak_before, int band_score,
                          CardRarity out_slots[CAMPAIGN_PRIZE_SLOT_COUNT])
{
    constexpr int NUMBER_NOW_BAND_SCALE = 10;
    CardRarity merged[CAMPAIGN_PRIZE_SLOT_COUNT];

    switch(mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        drop_merged_slot_rarities(peak_before, band_score, merged);
        out_slots[0] = merged[0];
        out_slots[1] = CardRarity::COMMON;
        out_slots[2] = merged[2];
        break;

    case CampaignMode::NUMBER_NOW:
        drop_merged_slot_rarities(peak_before / NUMBER_NOW_BAND_SCALE, band_score / NUMBER_NOW_BAND_SCALE,
                                  merged);
        out_slots[0] = merged[0];
        out_slots[1] = CardRarity::COMMON;
        out_slots[2] = merged[2];
        break;

    case CampaignMode::SAME_NUMBER:
    default:
        out_slots[0] = CardRarity::COMMON;
        out_slots[1] = CardRarity::COMMON;
        out_slots[2] = CardRarity::COMMON;
        break;
    }
}

CardType prize_combo_next(const SaveData& save)
{
    constexpr CardType COMBO_PRIZE_SEQUENCE[] = {
        CardType::PEANUT_BUTTER,
        CardType::JELLY,
        CardType::STRAW,
        CardType::STICKS,
        CardType::BRICKS,
        CardType::ROCK,
        CardType::PAPER,
        CardType::SCISSORS,
        CardType::SHOOT,
    };
    constexpr int COMBO_PRIZE_SEQUENCE_COUNT =
        sizeof(COMBO_PRIZE_SEQUENCE) / sizeof(COMBO_PRIZE_SEQUENCE[0]);

    for(int index = 0; index < COMBO_PRIZE_SEQUENCE_COUNT; ++index)
    {
        const CardType type = COMBO_PRIZE_SEQUENCE[index];

        if(library_total_owned(save, type) == 0)
        {
            return type;
        }
    }

    return CardType::COUNT;
}

bool prize_build_offers(const SaveData& save, CampaignMode mode, int peak_before, int band_score,
                         bn::seed_random& rng,
                         PrizeOffer out_offers[CAMPAIGN_PRIZE_SLOT_COUNT])
{
    (void)mode;
    (void)peak_before;
    (void)band_score;

    CardType picked_cards[CAMPAIGN_PRIZE_SLOT_COUNT] = {
        CardType::COUNT,
        CardType::COUNT,
        CardType::COUNT,
    };

    for(int slot = 0; slot < CAMPAIGN_PRIZE_SLOT_COUNT; ++slot)
    {
        const CardType card = pick_prize_card(save, rng, picked_cards, slot);
        out_offers[slot].kind = PrizeOfferKind::CARD;
        out_offers[slot].card = card;
        picked_cards[slot] = card;
    }

    return true;
}

TrinketType prize_roll_trinket(const SaveData& save, bn::seed_random& rng)
{
    bn::vector<TrinketType, int(TrinketType::COUNT)> candidates;

    for(int index = int(TrinketType::NONE) + 1; index < int(TrinketType::COUNT); ++index)
    {
        if(save.trinket_owned[index] == 0)
        {
            candidates.push_back(static_cast<TrinketType>(index));
        }
    }

    if(candidates.empty())
    {
        return TrinketType::NONE;
    }

    return candidates[rng.get_int(candidates.size())];
}
