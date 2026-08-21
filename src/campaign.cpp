#include "campaign.h"

namespace
{
    constexpr CardType STARTER_WHEELS[] = {
        CardType::LONGBOARD,
        CardType::HEELYS,
        CardType::SCOOTER,
        CardType::SKATEBOARD,
        CardType::TOPPINGS,
    };
}

bool campaign_needs_starter_setup(const SaveData& save)
{
    return !campaign_is_ready(save);
}

bool campaign_create_starter_deck(SaveData& save, CardType utility_pick)
{
    if(save.deck_count >= MAX_SAVED_DECKS)
    {
        return false;
    }

    SavedDeck& deck = save.decks[save.deck_count];
    deck = saved_deck_make_new();
    saved_deck_assign_unique_name(deck, save, -1);

    for(CardType wheel : STARTER_WHEELS)
    {
        if(!library_add_card(save, wheel) || !saved_deck_add_card(deck, wheel))
        {
            return false;
        }
    }

    if(!library_add_card(save, utility_pick) || !saved_deck_add_card(deck, utility_pick))
    {
        return false;
    }

    ++save.deck_count;
    save.active_deck_index = 0;
    save.campaign_ready = 1;
    save.same_number_target = 0;
    save.same_number_used_count = 0;

    for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
    {
        save.same_number_used_targets[index] = 0;
    }

    save_data_write();
    return true;
}

void campaign_prepare_same_number_target(SaveData& save, bn::seed_random& rng)
{
    if(save.same_number_target > 0)
    {
        return;
    }

    // New bracket every 5 wins — clear prior used targets.
    if(save.same_number_wins % 5 == 0)
    {
        save.same_number_used_count = 0;
    }

    const int bracket = save.same_number_wins / 5;
    const int min_target = bracket * SAME_NUMBER_BRACKET_SIZE + 1;
    int pool[SAME_NUMBER_BRACKET_SIZE];
    int pool_size = 0;

    for(int offset = 0; offset < SAME_NUMBER_BRACKET_SIZE; ++offset)
    {
        const int candidate = min_target + offset;
        bool used = false;

        for(int index = 0; index < save.same_number_used_count; ++index)
        {
            if(save.same_number_used_targets[index] == candidate)
            {
                used = true;
                break;
            }
        }

        if(!used)
        {
            pool[pool_size] = candidate;
            ++pool_size;
        }
    }

    if(pool_size <= 0)
    {
        save.same_number_used_count = 0;

        for(int offset = 0; offset < SAME_NUMBER_BRACKET_SIZE; ++offset)
        {
            pool[pool_size] = min_target + offset;
            ++pool_size;
        }
    }

    const int chosen = pool[rng.get_int(pool_size)];
    save.same_number_target = int16_t(chosen);

    if(save.same_number_used_count < SAME_NUMBER_USED_CAPACITY)
    {
        save.same_number_used_targets[save.same_number_used_count] = int16_t(chosen);
        ++save.same_number_used_count;
    }

    save_data_write();
}

int campaign_number_now_round_count(int deck_size)
{
    if(deck_size <= 0)
    {
        return 1;
    }

    return (deck_size - 1) / 5 + 1;
}

CampaignBattleSetup campaign_battle_setup(const SaveData& save, CampaignMode mode, bn::seed_random& rng)
{
    CampaignBattleSetup setup;
    setup.mode = mode;

    if(save.deck_count <= 0)
    {
        return setup;
    }

    const SavedDeck& deck = save.decks[save.active_deck_index];
    const int deck_size = saved_deck_total_cards(deck);

    switch(mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        setup.peak_before = save.biggest_number_record;
        setup.band_score = setup.peak_before;
        break;

    case CampaignMode::SAME_NUMBER:
        setup.same_number_target = save.same_number_target;
        setup.peak_before = 0;
        setup.band_score = 0;
        break;

    case CampaignMode::NUMBER_NOW:
    {
        const int round_count = campaign_number_now_round_count(deck_size);

        if(round_count <= 0)
        {
            setup.number_now_scoring_round = 1;
        }
        else
        {
            setup.number_now_scoring_round = 1 + rng.get_int(round_count);
        }

        const int round_index = setup.number_now_scoring_round - 1;

        if(round_index >= 0 && round_index < CAMPAIGN_NUMBER_NOW_ROUNDS)
        {
            setup.number_now_round_peak = save.number_now_round_best[round_index];
        }

        setup.peak_before = setup.number_now_round_peak;
        setup.band_score = setup.number_now_round_peak;
        break;
    }

    default:
        break;
    }

    return setup;
}

bool campaign_evaluate_win(const SaveData& save, CampaignMode mode, const GameSceneResult& result,
                           int peak_before, int same_number_target, int number_now_round_peak,
                           int number_now_scoring_round)
{
    (void)save;

    switch(mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        return result.final_score > peak_before;

    case CampaignMode::SAME_NUMBER:
        // Require a locked challenge; never treat target 0 as a match.
        return same_number_target > 0 && result.final_score == same_number_target;

    case CampaignMode::NUMBER_NOW:
        return result.last_round_number == number_now_scoring_round &&
               result.last_round_score > number_now_round_peak;

    default:
        return false;
    }
}

void campaign_apply_win(SaveData& save, CampaignMode mode, const GameSceneResult& result,
                        int number_now_scoring_round, bn::seed_random& rng)
{
    ++save.total_wins;

    switch(mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        if(result.final_score > save.biggest_number_record)
        {
            save.biggest_number_record = result.final_score;
        }
        break;

    case CampaignMode::SAME_NUMBER:
        ++save.same_number_wins;
        // Roll the next challenge immediately so the target never sits at 0 between runs.
        save.same_number_target = 0;
        campaign_prepare_same_number_target(save, rng);
        break;

    case CampaignMode::NUMBER_NOW:
    {
        const int round_index = number_now_scoring_round - 1;

        if(round_index >= 0 && round_index < CAMPAIGN_NUMBER_NOW_ROUNDS &&
           result.last_round_score > save.number_now_round_best[round_index])
        {
            save.number_now_round_best[round_index] = result.last_round_score;
        }

        break;
    }

    default:
        break;
    }

    save_data_write();
}

int campaign_wins_until_upgrade(const SaveData& save)
{
    const int mod = save.total_wins % 5;
    return mod == 0 ? 0 : 5 - mod;
}

int campaign_wins_until_trinket(const SaveData& save)
{
    const int mod = save.total_wins % 10;
    return mod == 0 ? 0 : 10 - mod;
}

void campaign_rebuild_instance_pool(SaveData& save)
{
    instance_pool_clear(save.instance_pool);

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        const CardType type = CardType(type_index);
        const int owned = library_total_owned(save, type);

        for(int copy = 0; copy < owned; ++copy)
        {
            if(instance_pool_add(save.instance_pool, type) == NO_INSTANCE)
            {
                break;
            }
        }
    }
}

void campaign_flatten_deck(const SaveData& save, int deck_index, bn::vector<CardRef, 50>& out)
{
    out.clear();

    if(deck_index < 0 || deck_index >= save.deck_count)
    {
        return;
    }

    const SavedDeck& deck = save.decks[deck_index];

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        const CardType type = CardType(type_index);
        const int copies = deck.counts[type_index];

        for(int copy = 0; copy < copies; ++copy)
        {
            // Each deck maps its Nth copy of a type to the Nth owned instance.
            // Decks share the collection; they do not consume distinct instances.
            int seen = 0;
            uint8_t instance_id = NO_INSTANCE;

            for(uint8_t id = 0; id < save.instance_pool.count; ++id)
            {
                if(save.instance_pool.entries[id].base == type)
                {
                    if(seen == copy)
                    {
                        instance_id = id;
                        break;
                    }

                    ++seen;
                }
            }

            out.push_back(CardRef{type, instance_id});
        }
    }
}

bool campaign_apply_prize_card(SaveData& save, CardType type)
{
    if(!library_add_card(save, type))
    {
        return false;
    }

    save_data_write();
    return true;
}

bool campaign_apply_prize_upgrade(SaveData& save, PrizeOfferKind kind, uint8_t instance_id,
                                  bn::seed_random& rng)
{
    CardInstance* instance = instance_at_mut(save.instance_pool, instance_id);

    if(!instance)
    {
        return false;
    }

    bool applied = false;

    switch(kind)
    {
    case PrizeOfferKind::UPGRADE_PLUS_DIGIT:
    {
        const uint8_t digit = static_cast<uint8_t>(1 + rng.get_int(9));
        applied = instance_apply_plus_digit(*instance, digit);
        break;
    }

    case PrizeOfferKind::UPGRADE_INCREMENT_MULT:
        applied = instance_apply_increment_mult(*instance);
        break;

    case PrizeOfferKind::UPGRADE_LEAD:
        applied = instance_apply_gravity(*instance, Gravity::LEAD);
        break;

    case PrizeOfferKind::UPGRADE_YEAST:
        applied = instance_apply_gravity(*instance, Gravity::YEAST);
        break;

    default:
        break;
    }

    if(applied)
    {
        save_data_write();
    }

    return applied;
}

bool campaign_apply_prize_trinket(SaveData& save, TrinketType type)
{
    if(type == TrinketType::NONE || int(type) >= int(TrinketType::COUNT))
    {
        return false;
    }

    if(save.trinket_owned[int(type)] != 0)
    {
        return false;
    }

    save.trinket_owned[int(type)] = 1;
    save_data_write();
    return true;
}

int campaign_library_total_cards(const SaveData& save)
{
    int total = 0;

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        total += save.library_counts[type_index];
    }

    return total;
}

bool campaign_is_starter_wheel(CardType type)
{
    for(CardType wheel : STARTER_WHEELS)
    {
        if(type == wheel)
        {
            return true;
        }
    }

    return false;
}

bool campaign_apply_sell_collection(SaveData& save, CardType nostalgia_card, CardType utility_pick)
{
    if(int(nostalgia_card) < 0 || nostalgia_card >= CardType::COUNT ||
       int(utility_pick) < 0 || utility_pick >= CardType::COUNT)
    {
        return false;
    }

    save.total_wins = 0;
    save.same_number_wins = 0;
    save.same_number_target = 0;
    save.same_number_used_count = 0;
    save.biggest_number_record = 0;

    for(int index = 0; index < SAME_NUMBER_USED_CAPACITY; ++index)
    {
        save.same_number_used_targets[index] = 0;
    }

    for(int round_index = 0; round_index < CAMPAIGN_NUMBER_NOW_ROUNDS; ++round_index)
    {
        save.number_now_round_best[round_index] = 0;
    }

    for(int trinket_index = 0; trinket_index < int(TrinketType::COUNT); ++trinket_index)
    {
        save.trinket_owned[trinket_index] = 0;
    }

    instance_pool_clear(save.instance_pool);

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        save.library_counts[type_index] = 0;
    }

    for(CardType wheel : STARTER_WHEELS)
    {
        if(!library_add_card(save, wheel))
        {
            return false;
        }
    }

    // Always keep the chosen card in the new library. Wheels are already present;
    // non-wheels must be added. Use a direct ensure so max-copy edge cases cannot
    // drop the nostalgia pick after the wipe.
    if(save.library_counts[int(nostalgia_card)] == 0)
    {
        if(!library_add_card(save, nostalgia_card))
        {
            save.library_counts[int(nostalgia_card)] = 1;
        }
    }

    if(!library_add_card(save, utility_pick))
    {
        // Utility may already be owned (kept the same card). Ensure at least one.
        if(save.library_counts[int(utility_pick)] == 0)
        {
            save.library_counts[int(utility_pick)] = 1;
        }
    }

    // Fresh collection: one rebuilt deck so empty leftover decks cannot hide the keep.
    save.deck_count = 1;
    save.active_deck_index = 0;
    save.campaign_ready = 1;
    save.decks[0] = saved_deck_make_new();
    saved_deck_assign_unique_name(save.decks[0], save, -1);

    for(int deck_index = 1; deck_index < MAX_SAVED_DECKS; ++deck_index)
    {
        save.decks[deck_index] = SavedDeck{};
    }

    SavedDeck& active = save.decks[0];

    for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
    {
        const CardType type = CardType(type_index);
        const int copies = save.library_counts[type_index];

        for(int copy = 0; copy < copies; ++copy)
        {
            if(!saved_deck_add_card(active, type))
            {
                return false;
            }
        }
    }

    campaign_rebuild_instance_pool(save);
    save_data_write();
    return true;
}
