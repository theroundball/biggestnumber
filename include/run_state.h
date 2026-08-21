#ifndef RUN_STATE_H
#define RUN_STATE_H

#include "bn_array.h"
#include "bn_seed_random.h"
#include "bn_vector.h"

#include "card_instance.h"
#include "game_state.h"

// Roguelike run — instance deck + peak chase (docs/ROGUELIKE_RUN_HANDOFF.md).
struct RunState
{
    InstancePool pool;
    bn::vector<uint8_t, 50> deck_ids;
    bn::array<TrinketType, 3> trinkets = {
        TrinketType::MOREL,
        TrinketType::LUCKY_SEVENS,
        TrinketType::PRIME_TIME,
    };
    int run_peak = 0;
    int miss_streak = 0;
    int battles_completed = 0;
};

void run_begin(RunState& run, bn::seed_random& rng);

bool run_apply_play_result(RunState& run, int peak_before, int battle_score);
bool run_is_over(const RunState& run);

uint8_t run_add_card(RunState& run, CardType type);
bool run_remove_card_at(RunState& run, int deck_index);

CardRef run_deck_ref(const RunState& run, int deck_index);
void run_flatten_deck(const RunState& run, bn::vector<CardRef, 50>& out);

int run_count_type(const RunState& run, CardType type);

#endif
