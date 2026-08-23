#include "run_state.h"

#include "save_data.h"

namespace
{
    constexpr CardType VANILLA_PLUS[] = {
        CardType::LONGBOARD,
        CardType::HEELYS,
        CardType::SCOOTER,
        CardType::SKATEBOARD,
        CardType::ROLLER_BLADES,
        CardType::WAGON,
        CardType::STOLLER,
        CardType::RIP_STICK,
        CardType::BIKE,
    };

    constexpr int VANILLA_PLUS_COUNT = sizeof(VANILLA_PLUS) / sizeof(VANILLA_PLUS[0]);
}

void run_begin(RunState& run, bn::seed_random& rng)
{
    instance_pool_clear(run.pool);
    run.deck_ids.clear();
    run.run_peak = 0;
    run.miss_streak = 0;
    run.battles_completed = 0;

    bn::vector<CardType, VANILLA_PLUS_COUNT> pool;

    for(int index = 0; index < VANILLA_PLUS_COUNT; ++index)
    {
        pool.push_back(VANILLA_PLUS[index]);
    }

    for(int pick = 0; pick < 5; ++pick)
    {
        const int index = rng.get_int(pool.size());
        run_add_card(run, pool[index]);
        pool.erase(pool.begin() + index);
    }

    run_add_card(run, CardType::BIG_KUROSAWA_BURGER);
}

bool run_apply_play_result(RunState& run, int peak_before, int battle_score)
{
    ++run.battles_completed;

    if(battle_score > peak_before)
    {
        run.run_peak = battle_score;
        run.miss_streak = 0;
        return true;
    }

    ++run.miss_streak;
    return false;
}

bool run_is_over(const RunState& run)
{
    return run.miss_streak >= 2;
}

uint8_t run_add_card(RunState& run, CardType type)
{
    if(run.deck_ids.full() || run.deck_ids.size() >= DECK_MAX_CARDS)
    {
        return NO_INSTANCE;
    }

    const uint8_t id = instance_pool_add(run.pool, type);

    if(id == NO_INSTANCE)
    {
        return NO_INSTANCE;
    }

    run.deck_ids.push_back(id);
    return id;
}

bool run_remove_card_at(RunState& run, int deck_index)
{
    if(deck_index < 0 || deck_index >= run.deck_ids.size() || run.deck_ids.size() <= 1)
    {
        return false;
    }

    // Leave the pool entry intact so other battle copies aren't renumbered mid-run;
    // the id is simply dropped from the run deck list.
    run.deck_ids.erase(run.deck_ids.begin() + deck_index);
    return true;
}

CardRef run_deck_ref(const RunState& run, int deck_index)
{
    if(deck_index < 0 || deck_index >= run.deck_ids.size())
    {
        return CardRef{};
    }

    const uint8_t id = run.deck_ids[deck_index];
    const CardInstance* instance = instance_at(run.pool, id);

    if(!instance)
    {
        return CardRef{};
    }

    return CardRef{instance->base, id};
}

void run_flatten_deck(const RunState& run, bn::vector<CardRef, 50>& out)
{
    out.clear();

    for(int index = 0; index < run.deck_ids.size(); ++index)
    {
        out.push_back(run_deck_ref(run, index));
    }
}

int run_count_type(const RunState& run, CardType type)
{
    int count = 0;

    for(int index = 0; index < run.deck_ids.size(); ++index)
    {
        if(run_deck_ref(run, index).type == type)
        {
            ++count;
        }
    }

    return count;
}
