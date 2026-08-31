#include "game_state.h"
#include "game_events.h"
#include "game_helpers.h"
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

    if(build_a_number_active && !applying_build_a_number_payout)
    {
        return 0;
    }

    if(has_trinket(TrinketType::GET_WITH_THE_TIMES))
    {
        score_pop_queue(*this, 0, false, TrinketScoreField::ROUND, false, true);
        return 0;
    }

    if(applying_double_adds)
    {
        amount *= 2;
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

    check_bounty_return(*this);

    return amount;
}

namespace
{
    int fibonacci_at_round(int round)
    {
        if(round <= 0)
        {
            return 0;
        }

        if(round <= 2)
        {
            return 1;
        }

        int previous = 1;
        int current = 1;

        for(int index = 3; index <= round; ++index)
        {
            const long long next = static_cast<long long>(previous) + current;
            previous = current;
            current = next > 2147483647 ? 2147483647 : int(next);
        }

        return current;
    }
}

void GameState::apply_round_start_trinkets()
{
    if(build_a_number_active)
    {
        return;
    }

    if(has_trinket(TrinketType::GET_WITH_THE_TIMES))
    {
        const int before = round.running;
        round.add(6);
        score_pop_queue_from_trinket(*this, 6, TrinketType::GET_WITH_THE_TIMES,
                                     TrinketScoreField::ROUND, before, round.running, true, false);
        trinket_queue_proc(*this, TrinketType::GET_WITH_THE_TIMES);
        trinket_queue_score_check(*this, TrinketScoreField::ROUND, before, round.running);
    }

    if(has_trinket(TrinketType::FIBONACCI))
    {
        const int amount = fibonacci_at_round(current_round);
        const int before = round.running;
        const long long after = static_cast<long long>(before) + amount;
        round.running = after > 2147483647 ? 2147483647 : int(after);
        const int applied = round.running - before;
        score_pop_queue_from_trinket(*this, applied, TrinketType::FIBONACCI,
                                     TrinketScoreField::ROUND, before, round.running, true, false);
        trinket_queue_proc(*this, TrinketType::FIBONACCI);
        trinket_queue_score_check(*this, TrinketScoreField::ROUND, before, round.running);
    }
}

void GameState::apply_round_start_adds()
{
    apply_round_start_trinkets();

    if(round_start_seed.positive)
    {
        add_from_card(round_start_seed.positive);
    }
}

void GameState::apply_round_start_multiply()
{
    if(round_start_seed.multiply)
    {
        apply_seed_multiply(round_start_seed.multiply);
    }
}

bool GameState::apply_round_start_turtle_step()
{
    if(turtle_rounds_remaining <= 0)
    {
        return false;
    }

    --turtle_rounds_remaining;

    if(turtle_rounds_remaining == 0)
    {
        flush_staircase_climb();
    }

    return turtle_rounds_remaining == 0;
}

void GameState::flush_staircase_climb()
{
    staircase_flush_climb(*this);
}

void GameState::evaluate_apply_ui_slot(int ui_slot_offset, bool clear_after)
{
    if(ui_slot_offset < 0 || ui_slot_offset >= 3)
    {
        return;
    }

    const int slot_index = (next_mod_index + ui_slot_offset) % 3;
    RoundModifier& mod = future_mods[slot_index];

    if(mod.positive)
    {
        add_from_card(mod.positive);

        if(clear_after)
        {
            mod.positive = 0;
        }
    }

    if(mod.multiply)
    {
        apply_seed_multiply(mod.multiply);

        if(clear_after)
        {
            mod.multiply = 0;
        }
    }

    if(mod.draw_at_start)
    {
        queue_effect_draw(*this, mod.draw_at_start, false);

        if(clear_after)
        {
            mod.draw_at_start = 0;
        }
    }
}

void GameState::evaluate_apply_future_modifiers(bool clear_after)
{
    evaluate_apply_ui_slot(0, clear_after);
}

void GameState::queue_evaluate_ghost_steps()
{
    for(int ui_slot = 0; ui_slot < 3; ++ui_slot)
    {
        const int slot_index = (next_mod_index + ui_slot) % 3;
        const RoundModifier& mod = future_mods[slot_index];

        if(mod.positive)
        {
            PendingAction action;
            action.type = PendingActionType::EVALUATE_GHOST_STEP;
            action.count = ui_slot;
            action.hand_index = 0;
            pending_actions.push_back(action);
        }

        if(mod.multiply)
        {
            PendingAction action;
            action.type = PendingActionType::EVALUATE_GHOST_STEP;
            action.count = ui_slot;
            action.hand_index = 1;
            pending_actions.push_back(action);
        }

        if(mod.draw_at_start)
        {
            PendingAction action;
            action.type = PendingActionType::EVALUATE_GHOST_STEP;
            action.count = ui_slot;
            action.hand_index = 2;
            pending_actions.push_back(action);
        }

        PendingAction clear_row;
        clear_row.type = PendingActionType::EVALUATE_GHOST_STEP;
        clear_row.count = ui_slot;
        clear_row.hand_index = 3;
        pending_actions.push_back(clear_row);
    }

    PendingAction finish;
    finish.type = PendingActionType::EVALUATE_GHOST_STEP;
    finish.hand_index = 4;
    pending_actions.push_back(finish);
}

void GameState::build_a_number_activate()
{
    if(build_a_number_active)
    {
        return;
    }

    build_pre_running = round.running;
    build_pre_end_multiplier = round.end_multiplier;
    build_a_number_active = true;

    for(int index = 0; index < 3; ++index)
    {
        build_digits[index] = -1;
    }

    round.running = 0;
}

void GameState::build_a_number_reset()
{
    build_a_number_active = false;
    build_pre_running = 0;
    build_pre_end_multiplier = 1;
    applying_build_a_number_payout = false;

    for(int index = 0; index < 3; ++index)
    {
        build_digits[index] = -1;
    }
}

bool GameState::build_a_number_all_digits_filled() const
{
    for(int index = 0; index < 3; ++index)
    {
        if(build_digits[index] < 0 || build_digits[index] > 9)
        {
            return false;
        }
    }

    return true;
}

int GameState::build_a_number_assembled_value() const
{
    return build_digits[0] * 100 + build_digits[1] * 10 + build_digits[2];
}

void GameState::build_a_number_complete_payout()
{
    if(!build_a_number_all_digits_filled())
    {
        return;
    }

    const int payout = build_a_number_assembled_value();
    build_a_number_reset();
    applying_build_a_number_payout = true;
    add_from_card(payout);
    applying_build_a_number_payout = false;
}

int GameState::build_a_number_commit_prebuild()
{
    int committed = 0;

    if(build_a_number_active && (build_pre_running != 0 || build_pre_end_multiplier != 1))
    {
        RoundScore snapshot;
        snapshot.running = build_pre_running;
        snapshot.end_multiplier = build_pre_end_multiplier;
        committed = snapshot.committed();
        total_score += committed;
        build_pre_running = 0;
        build_pre_end_multiplier = 1;
    }

    round.reset();
    return committed;
}

void GameState::schedule_keep_going()
{
    for(int offset = 0; offset < 3; ++offset)
    {
        uint8_t& returns = keep_going_returns[(next_mod_index + offset) % 3];
        const int next = int(returns) + DEAD_RISING_GY_RETURNS_PER_ROUND;
        returns = next > 255 ? 255 : uint8_t(next);
    }
}

void GameState::resolve_keep_going_round_start()
{
    const int returns = keep_going_returns[next_mod_index];
    keep_going_returns[next_mod_index] = 0;

    if(returns > 0)
    {
        deck.compact();
    }

    for(int count = 0; count < returns && !graveyard.empty(); ++count)
    {
        const int index = rng.get_int(graveyard.size());
        const CardRef card = graveyard[index];
        graveyard_remove_at(*this, index);

        if(card.type == CardType::GET_ME_OUTA_HERE)
        {
            ++keep_going_relocation_triggers;
        }

        deck.insert_top(card);
    }

    if(returns > 0)
    {
        deck.apply_gravity(instance_pool);
    }
}

void GameState::finish_keep_going_round_start()
{
    while(keep_going_relocation_triggers > 0)
    {
        apply_card_relocated(*this, CardType::GET_ME_OUTA_HERE);
        --keep_going_relocation_triggers;
    }
}

void GameState::mul_from_card(int factor)
{
    if(factor <= 1)
    {
        return;
    }

    if(build_a_number_active && !applying_build_a_number_payout)
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

    trinket_queue_score_check(*this, TrinketScoreField::ROUND, before_committed, after_committed);

    check_bounty_return(*this);
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
