#include "trinket_system.h"

#include "score_pop_system.h"
#include "score_count_system.h"

#include "bn_math.h"
#include "bn_string.h"

#include "game_context.h"
#include "hud.h"

namespace
{
    int roll_lucky_sevens(bn::seed_random& rng)
    {
        const int span = LUCKY_SEVENS_ROLL_MAX - LUCKY_SEVENS_ROLL_MIN + 1;
        return LUCKY_SEVENS_ROLL_MIN + (rng.get() % span);
    }

    bool lucky_sevens_should_trigger(const PendingScoreCheck& check)
    {
        if(!score_contains_seven(check.after))
        {
            return false;
        }

        if(check.from_lucky_sevens_roll && score_contains_seven(check.lucky_sevens_added))
        {
            return false;
        }

        return check.before != check.after;
    }

    bool prime_time_score_is_prime(const GameState& state, const PendingScoreCheck& check)
    {
        if(check.field == TrinketScoreField::TOTAL)
        {
            return score_is_prime(check.after);
        }

        return score_is_prime(state.round.committed());
    }

    bool prime_time_should_trigger(const GameState& state, const PendingScoreCheck& check)
    {
        if(!state.has_trinket(TrinketType::PRIME_TIME))
        {
            return false;
        }

        if(check.before == check.after)
        {
            return false;
        }

        return prime_time_score_is_prime(state, check);
    }

    void apply_prime_time_proc(GameContext& ctx, TrinketScoreField field)
    {
        int& proc_count = field == TrinketScoreField::TOTAL ? ctx.state.prime_time_total_procs
                                                            : ctx.state.prime_time_round_procs;
        ++proc_count;

        const int before = ctx.state.round.running;
        ctx.state.round.add(proc_count);
        score_pop_queue_from_trinket(ctx.state, proc_count, TrinketType::PRIME_TIME, TrinketScoreField::ROUND,
                                     before, ctx.state.round.running, true, false);
        trinket_queue_proc(ctx.state, TrinketType::PRIME_TIME);
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, before, ctx.state.round.running);

        const int slot = trinket_slot_index(ctx.state, TrinketType::PRIME_TIME);

        if(slot >= 0)
        {
            ctx.hud.start_trinket_wiggle(slot);
        }
    }

    void sort_trinket_procs(bn::vector<TrinketType, 8>& procs, const GameState& state)
    {
        for(int first = 0; first < procs.size(); ++first)
        {
            for(int second = first + 1; second < procs.size(); ++second)
            {
                const int first_slot = trinket_slot_index(state, procs[first]);
                const int second_slot = trinket_slot_index(state, procs[second]);

                if(first_slot > second_slot)
                {
                    const TrinketType temp = procs[first];
                    procs[first] = procs[second];
                    procs[second] = temp;
                }
            }
        }
    }

    void begin_lucky_sevens_fx(GameContext& ctx, TrinketScoreField field)
    {
        if(!ctx.state.has_trinket(TrinketType::LUCKY_SEVENS))
        {
            return;
        }

        const int slot = trinket_slot_index(ctx.state, TrinketType::LUCKY_SEVENS);

        if(slot < 0)
        {
            return;
        }

        LuckySevensFxState& fx = ctx.lucky_sevens_fx;
        fx.active = true;
        fx.frame = 0;
        fx.slot = slot;
        fx.field = field;
        fx.final_roll = roll_lucky_sevens(ctx.random_engine);
        fx.displayed_roll = fx.final_roll;
        fx.rendered_roll = -1;
        ctx.hud.start_trinket_wiggle(slot);
    }

    void start_next_lucky_sevens_fx(GameContext& ctx)
    {
        if(ctx.pending_lucky_sevens.empty())
        {
            return;
        }

        const TrinketScoreField field = ctx.pending_lucky_sevens.front();
        ctx.pending_lucky_sevens.erase(ctx.pending_lucky_sevens.begin());
        begin_lucky_sevens_fx(ctx, field);
    }
}

bool score_contains_seven(int value)
{
    int digits = value < 0 ? -value : value;

    while(digits > 0)
    {
        if(digits % 10 == 7)
        {
            return true;
        }

        digits /= 10;
    }

    return false;
}

bool score_is_prime(int value)
{
    if(value < 2)
    {
        return false;
    }

    if(value == 2)
    {
        return true;
    }

    if(value % 2 == 0)
    {
        return false;
    }

    for(int divisor = 3; divisor * divisor <= value; divisor += 2)
    {
        if(value % divisor == 0)
        {
            return false;
        }
    }

    return true;
}

int trinket_slot_index(const GameState& state, TrinketType type)
{
    for(int slot = 0; slot < state.trinkets.size(); ++slot)
    {
        if(state.trinkets[slot] == type)
        {
            return slot;
        }
    }

    return -1;
}

void trinket_queue_proc(GameState& state, TrinketType type)
{
    if(type == TrinketType::NONE || !state.has_trinket(type))
    {
        return;
    }

    if(state.pending_trinket_procs.full())
    {
        state.pending_trinket_procs.erase(state.pending_trinket_procs.begin());
    }

    state.pending_trinket_procs.push_back(type);
}

void trinket_queue_score_check(GameState& state, TrinketScoreField field, int before, int after,
                               bool from_lucky_sevens_roll, int lucky_sevens_added)
{
    PendingScoreCheck check;
    check.field = field;
    check.before = before;
    check.after = after;
    check.from_lucky_sevens_roll = from_lucky_sevens_roll;
    check.lucky_sevens_added = lucky_sevens_added;

    if(state.pending_score_checks.full())
    {
        state.pending_score_checks.erase(state.pending_score_checks.begin());
    }

    state.pending_score_checks.push_back(check);
}

bool trinket_score_reaction_idle(const GameContext& ctx)
{
    if(ctx.lucky_sevens_fx.active || !ctx.pending_lucky_sevens.empty())
    {
        return false;
    }

    if(!ctx.state.pending_score_checks.empty())
    {
        return false;
    }

    for(const ScorePop& pop : ctx.score_pops)
    {
        if(pop.apply_on_arrive && !pop.arrived)
        {
            return false;
        }
    }

    for(const ScorePopRequest& request : ctx.state.pending_score_pops)
    {
        if(request.apply_on_arrive)
        {
            return false;
        }
    }

    return true;
}

namespace
{
    void drain_pending_score_checks(GameContext& ctx)
    {
        for(const PendingScoreCheck& check : ctx.state.pending_score_checks)
        {
            if(lucky_sevens_should_trigger(check))
            {
                if(ctx.pending_lucky_sevens.full())
                {
                    ctx.pending_lucky_sevens.erase(ctx.pending_lucky_sevens.begin());
                }

                ctx.pending_lucky_sevens.push_back(check.field);
            }

            if(prime_time_should_trigger(ctx.state, check))
            {
                apply_prime_time_proc(ctx, check.field);
            }
        }

        ctx.state.pending_score_checks.clear();

        if(!ctx.lucky_sevens_fx.active && !ctx.pending_lucky_sevens.empty())
        {
            start_next_lucky_sevens_fx(ctx);
        }
    }
}

void trinket_flush_deferred_modifiers(GameContext& ctx)
{
    while(ctx.state.deferred_morel_count > 0 && trinket_score_reaction_idle(ctx))
    {
        --ctx.state.deferred_morel_count;

        const int before = ctx.state.round.running;
        ctx.state.round.add(2);
        score_pop_queue_from_trinket(ctx.state, 2, TrinketType::MOREL, TrinketScoreField::ROUND, before,
                                     ctx.state.round.running, true, false);
        trinket_queue_proc(ctx.state, TrinketType::MOREL);
        trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, before, ctx.state.round.running);

        // Morel's addition is itself a stack entry — evaluate procs before the next deferred +2.
        drain_pending_score_checks(ctx);
    }
}

namespace
{
    void flush_pending_trinket_wiggles(GameContext& ctx)
    {
        if(ctx.state.pending_trinket_procs.empty())
        {
            return;
        }

        sort_trinket_procs(ctx.state.pending_trinket_procs, ctx.state);

        for(TrinketType type : ctx.state.pending_trinket_procs)
        {
            const int slot = trinket_slot_index(ctx.state, type);

            if(slot >= 0)
            {
                ctx.hud.start_trinket_wiggle(slot);
            }
        }

        ctx.state.pending_trinket_procs.clear();
    }
}

void trinket_process_queues(GameContext& ctx)
{
    if(ctx.lucky_sevens_fx.active)
    {
        return;
    }

    flush_pending_trinket_wiggles(ctx);
    drain_pending_score_checks(ctx);
    trinket_flush_deferred_modifiers(ctx);
    // Morel (and any other) procs queued during flush.
    flush_pending_trinket_wiggles(ctx);
}

void trinket_tick_fx(GameContext& ctx)
{
    ctx.hud.tick_trinket_wiggles();

    if(!ctx.lucky_sevens_fx.active)
    {
        return;
    }

    LuckySevensFxState& fx = ctx.lucky_sevens_fx;
    ++fx.frame;

    if(fx.frame < LUCKY_SEVENS_ROULETTE_FRAMES - LUCKY_SEVENS_ROULETTE_SETTLE_FRAMES)
    {
        fx.displayed_roll = roll_lucky_sevens(ctx.random_engine);
    }
    else
    {
        fx.displayed_roll = fx.final_roll;
    }

    if(fx.frame >= LUCKY_SEVENS_ROULETTE_FRAMES)
    {
        // Roulette finished beside the trinket — fly the result to the score, then apply.
        score_pop_queue_from_trinket(ctx.state, fx.final_roll, TrinketType::LUCKY_SEVENS, fx.field,
                                     0, 0, false, true);
        fx.active = false;
        ctx.trinket_fx_sprites.clear();
        fx.rendered_roll = -1;

        if(!ctx.pending_lucky_sevens.empty())
        {
            start_next_lucky_sevens_fx(ctx);
        }
        else
        {
            trinket_process_queues(ctx);
        }
    }
}

void trinket_render_fx(GameContext& ctx)
{
    LuckySevensFxState& fx = ctx.lucky_sevens_fx;

    if(!fx.active)
    {
        if(!ctx.trinket_fx_sprites.empty())
        {
            ctx.trinket_fx_sprites.clear();
            fx.rendered_roll = -1;
        }

        return;
    }

    int anchor_x = 0;
    int anchor_y = -16;

    if(ctx.hud.trinket_slot_position(fx.slot, anchor_x, anchor_y))
    {
        anchor_x -= 18;
    }
    else
    {
        anchor_x = 0;
        anchor_y = -16;
    }

    // Rebuild every frame so the spinning number tracks the wiggling trinket icon.
    fx.rendered_roll = fx.displayed_roll;
    ctx.trinket_fx_sprites.clear();
    ctx.round_text_generator.set_center_alignment();
    ctx.round_text_generator.generate(anchor_x, anchor_y, bn::to_string<8>(fx.displayed_roll),
                                      ctx.trinket_fx_sprites);
    ctx.round_text_generator.set_left_alignment();
}

void staircase_reset(GameState& state)
{
    state.staircase_last_plus = 0;
    state.staircase_length = 0;
    state.staircase_sum = 0;
}

void staircase_flush_climb(GameState& state)
{
    if(!state.has_trinket(TrinketType::STAIRCASE))
    {
        staircase_reset(state);
        return;
    }

    if(state.staircase_length < 2)
    {
        staircase_reset(state);
        return;
    }

    const int bonus = state.staircase_sum * state.staircase_length;
    const int before = state.round.running;
    state.round.add(bonus);
    score_pop_queue_from_trinket(state, bonus, TrinketType::STAIRCASE, TrinketScoreField::ROUND, before,
                                 state.round.running, true, false);
    trinket_queue_proc(state, TrinketType::STAIRCASE);
    trinket_queue_score_check(state, TrinketScoreField::ROUND, before, state.round.running);
    staircase_reset(state);
}

void staircase_on_card_plus(GameState& state, int plus)
{
    if(!state.has_trinket(TrinketType::STAIRCASE) || plus <= 0)
    {
        return;
    }

    if(state.staircase_length == 0)
    {
        state.staircase_length = 1;
        state.staircase_sum = plus;
        state.staircase_last_plus = plus;
        return;
    }

    if(plus > state.staircase_last_plus)
    {
        ++state.staircase_length;
        state.staircase_sum += plus;
        state.staircase_last_plus = plus;
        return;
    }

    staircase_flush_climb(state);
    state.staircase_length = 1;
    state.staircase_sum = plus;
    state.staircase_last_plus = plus;
}
