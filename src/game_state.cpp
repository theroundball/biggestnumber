#include "game_state.h"

#include "score_pop_system.h"
#include "score_count_system.h"
#include "trinket_system.h"

int GameState::add_from_card(int amount)
{
    if(amount <= 0)
    {
        round.add(amount);
        return amount;
    }

    if(has_trinket(TrinketType::GET_WITH_THE_TIMES))
    {
        score_pop_queue(*this, 0, false, TrinketScoreField::ROUND, false, true);
        return 0;
    }

    const int before = round.running;

    round.add(amount);
    score_pop_queue(*this, amount, false);
    score_count_queue(*this, TrinketScoreField::ROUND, before, round.running);

    // Evaluate card add first (Lucky 7 / Prime Time). Morel +2 is deferred until that
    // reaction stack is idle — see trinket_flush_deferred_modifiers.
    trinket_queue_score_check(*this, TrinketScoreField::ROUND, before, round.running);

    if(has_trinket(TrinketType::MOREL))
    {
        ++deferred_morel_count;
    }

    return amount;
}

void GameState::apply_round_start_trinkets()
{
    if(!has_trinket(TrinketType::GET_WITH_THE_TIMES))
    {
        return;
    }

    const int before = round.running;
    round.add(6);
    score_pop_queue_from_trinket(*this, 6, TrinketType::GET_WITH_THE_TIMES, TrinketScoreField::ROUND,
                                 before, round.running, true, false);
    trinket_queue_proc(*this, TrinketType::GET_WITH_THE_TIMES);
    trinket_queue_score_check(*this, TrinketScoreField::ROUND, before, round.running);
}

void GameState::mul_from_card(int factor)
{
    if(factor <= 1)
    {
        return;
    }

    const int before_running = round.running;
    const int before_committed = round.committed();
    round.running *= factor;
    const int after_running = round.running;
    const int after_committed = round.committed();

    score_pop_queue(*this, factor, false, TrinketScoreField::ROUND, true);

    if(after_running > before_running)
    {
        score_count_queue(*this, TrinketScoreField::ROUND, before_running, after_running);
    }
    else
    {
        score_pop_queue(*this, 0, false, TrinketScoreField::ROUND, false, true);
    }

    trinket_queue_score_check(*this, TrinketScoreField::ROUND, before_committed, after_committed);
}

void GameState::apply_seed_multiply(int factor)
{
    if(factor <= 1)
    {
        return;
    }

    const int before = round.committed();
    round.schedule_end_multiply(factor);
    score_pop_queue(*this, factor, false, TrinketScoreField::ROUND, true);
    trinket_queue_score_check(*this, TrinketScoreField::ROUND, before, round.committed());
}
