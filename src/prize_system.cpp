#include "prize_system.h"

#include "bn_vector.h"

namespace
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
    constexpr int COMBO_PRIZE_SEQUENCE_COUNT = sizeof(COMBO_PRIZE_SEQUENCE) / sizeof(COMBO_PRIZE_SEQUENCE[0]);

    constexpr int SAME_NUMBER_SLOT3_COMMON_WEIGHT = 60;
    constexpr int SAME_NUMBER_SLOT3_UNCOMMON_WEIGHT = 35;
    constexpr int NUMBER_NOW_BAND_SCALE = 10;

    int scaled_peak(int peak)
    {
        return peak / NUMBER_NOW_BAND_SCALE;
    }

    int scaled_score(int score)
    {
        return score / NUMBER_NOW_BAND_SCALE;
    }

    bool library_copy_limit_reached(const SaveData& save, CardType type)
    {
        if(card_is_combo_piece(type))
        {
            return library_total_owned(save, type) >= 1;
        }

        return library_total_owned(save, type) >= card_meta(type).max_copies;
    }

    bool collect_eligible_library(const SaveData& save, CardRarity rarity, bn::vector<CardType, 52>& out)
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

            if(card_is_combo_piece(type))
            {
                continue;
            }

            if(library_copy_limit_reached(save, type))
            {
                continue;
            }

            if(type == CardType::PALINDROME && !palindrome_prize_eligible(save))
            {
                continue;
            }

            out.push_back(type);
        }

        return !out.empty();
    }

    CardType pick_library_card(const SaveData& save, CardRarity slot_rarity, bn::seed_random& rng,
                               const CardType already_offered[3], int offered_count)
    {
        bn::vector<CardType, 52> pool;

        for(int step = 0; step < 3; ++step)
        {
            const int rarity_value = static_cast<int>(slot_rarity) - step;

            if(rarity_value < 0)
            {
                break;
            }

            if(!collect_eligible_library(save, static_cast<CardRarity>(rarity_value), pool))
            {
                continue;
            }

            bn::vector<CardType, 52> unique_pool;

            for(int index = 0; index < pool.size(); ++index)
            {
                bool duplicate = false;

                for(int prior = 0; prior < offered_count; ++prior)
                {
                    if(already_offered[prior] == pool[index])
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

            const bn::vector<CardType, 52>& pick_from = unique_pool.empty() ? pool : unique_pool;
            return pick_from[rng.get_int(pick_from.size())];
        }

        return CardType::LONGBOARD;
    }

    CardRarity roll_same_number_slot3_rarity(bn::seed_random& rng)
    {
        const int roll = rng.get_int(100);

        if(roll < SAME_NUMBER_SLOT3_COMMON_WEIGHT)
        {
            return CardRarity::COMMON;
        }

        if(roll < SAME_NUMBER_SLOT3_COMMON_WEIGHT + SAME_NUMBER_SLOT3_UNCOMMON_WEIGHT)
        {
            return CardRarity::UNCOMMON;
        }

        return CardRarity::RARE;
    }

    PrizeOfferKind random_upgrade_kind(bn::seed_random& rng)
    {
        switch(rng.get_int(4))
        {
        case 0:
            return PrizeOfferKind::UPGRADE_PLUS_DIGIT;
        case 1:
            return PrizeOfferKind::UPGRADE_INCREMENT_MULT;
        case 2:
            return PrizeOfferKind::UPGRADE_LEAD;
        default:
            return PrizeOfferKind::UPGRADE_YEAST;
        }
    }
}

void prize_slot_rarities(CampaignMode mode, int peak_before, int band_score,
                          CardRarity out_slots[CAMPAIGN_PRIZE_SLOT_COUNT])
{
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
        drop_merged_slot_rarities(scaled_peak(peak_before), scaled_score(band_score), merged);
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
    CardRarity slot_rarities[CAMPAIGN_PRIZE_SLOT_COUNT];
    prize_slot_rarities(mode, peak_before, band_score, slot_rarities);

    if(mode == CampaignMode::SAME_NUMBER)
    {
        slot_rarities[2] = roll_same_number_slot3_rarity(rng);
    }

    CardType picked_cards[CAMPAIGN_PRIZE_SLOT_COUNT] = {
        CardType::COUNT,
        CardType::COUNT,
        CardType::COUNT,
    };

    const CardType combo_next = prize_combo_next(save);

    if(combo_next != CardType::COUNT)
    {
        out_offers[CAMPAIGN_FLEX_SLOT_INDEX].kind = PrizeOfferKind::CARD;
        out_offers[CAMPAIGN_FLEX_SLOT_INDEX].card = combo_next;
        picked_cards[CAMPAIGN_FLEX_SLOT_INDEX] = combo_next;
    }
    else
    {
        const CardType flex_card =
            pick_library_card(save, CardRarity::COMMON, rng, picked_cards, CAMPAIGN_FLEX_SLOT_INDEX);
        out_offers[CAMPAIGN_FLEX_SLOT_INDEX].kind = PrizeOfferKind::CARD;
        out_offers[CAMPAIGN_FLEX_SLOT_INDEX].card = flex_card;
        picked_cards[CAMPAIGN_FLEX_SLOT_INDEX] = flex_card;
    }

    for(int slot = 0; slot < CAMPAIGN_PRIZE_SLOT_COUNT; ++slot)
    {
        if(slot == CAMPAIGN_FLEX_SLOT_INDEX)
        {
            continue;
        }

        const CardType card = pick_library_card(save, slot_rarities[slot], rng, picked_cards, slot);
        out_offers[slot].kind = PrizeOfferKind::CARD;
        out_offers[slot].card = card;
        picked_cards[slot] = card;
    }

    return true;
}

TrinketType prize_roll_trinket(const SaveData& save, bn::seed_random& rng)
{
    bn::vector<TrinketType, 8> candidates;

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
