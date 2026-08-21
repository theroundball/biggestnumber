#include "score_count_system.h"

#include "bn_string.h"

#include "game_context.h"
#include "score_pop_system.h"
#include "scoring.h"

namespace
{
    int score_count_index(TrinketScoreField field)
    {
        return field == TrinketScoreField::ROUND ? 0 : 1;
    }

    int score_count_step(int delta)
    {
        if(delta <= SCORE_COUNT_MAX_FRAMES)
        {
            return 1;
        }

        return (delta + SCORE_COUNT_MAX_FRAMES - 1) / SCORE_COUNT_MAX_FRAMES;
    }

    ScoreCountFxState& score_count_fx(GameContext& ctx, TrinketScoreField field)
    {
        return ctx.score_count_fx[score_count_index(field)];
    }

    const ScoreCountFxState& score_count_fx(const GameContext& ctx, TrinketScoreField field)
    {
        return ctx.score_count_fx[score_count_index(field)];
    }

    void start_count(GameContext& ctx, const ScoreCountRequest& request)
    {
        ScoreCountFxState& fx = score_count_fx(ctx, request.field);
        const int delta = request.after - request.before;

        fx.active = true;
        fx.to = request.after;
        fx.displayed = request.before;
        fx.step = score_count_step(delta);

        if(request.field == TrinketScoreField::ROUND)
        {
            fx.end_multiplier = ctx.state.round.end_multiplier;
            ctx.show_round_score_running(fx.displayed, fx.end_multiplier);
        }
        else
        {
            ctx.show_total_score_value(fx.displayed);
        }
    }

    void try_start_pending(GameContext& ctx, TrinketScoreField field)
    {
        if(score_count_fx(ctx, field).active)
        {
            return;
        }

        for(int index = 0; index < ctx.state.pending_score_counts.size(); ++index)
        {
            const ScoreCountRequest& request = ctx.state.pending_score_counts[index];

            if(request.field != field)
            {
                continue;
            }

            start_count(ctx, request);
            ctx.state.pending_score_counts.erase(ctx.state.pending_score_counts.begin() + index);
            return;
        }
    }

    void tick_field(GameContext& ctx, TrinketScoreField field)
    {
        ScoreCountFxState& fx = score_count_fx(ctx, field);

        if(!fx.active)
        {
            try_start_pending(ctx, field);
            return;
        }

        if(fx.displayed < fx.to)
        {
            for(int tick = 0; tick < SCORE_COUNT_STEPS_PER_TICK && fx.displayed < fx.to; ++tick)
            {
                fx.displayed += fx.step;

                if(fx.displayed > fx.to)
                {
                    fx.displayed = fx.to;
                }
            }
        }

        if(field == TrinketScoreField::ROUND)
        {
            ctx.show_round_score_running(fx.displayed, fx.end_multiplier);

            if(fx.displayed >= fx.to)
            {
                fx.active = false;
                ctx.finalize_round_score_display();
                try_start_pending(ctx, field);

                if(!score_count_fx(ctx, field).active &&
                   !score_pop_blocks_score_finalize(ctx, field))
                {
                    ctx.show_round_score_running(ctx.state.round.running, ctx.state.round.end_multiplier);
                }
            }
        }
        else
        {
            ctx.show_total_score_value(fx.displayed);

            if(fx.displayed >= fx.to)
            {
                fx.active = false;
                ctx.finalize_total_score_display();
                try_start_pending(ctx, field);

                if(!score_count_fx(ctx, field).active &&
                   !score_pop_blocks_score_finalize(ctx, field))
                {
                    ctx.show_total_score_value(ctx.state.total_score);
                }
            }
        }
    }
}

void score_count_queue(GameState& state, TrinketScoreField field, int before, int after)
{
    if(after <= before)
    {
        return;
    }

    if(state.pending_score_counts.full())
    {
        state.pending_score_counts.erase(state.pending_score_counts.begin());
    }

    ScoreCountRequest request;
    request.field = field;
    request.before = before;
    request.after = after;
    state.pending_score_counts.push_back(request);
}

void score_count_process_pending(GameContext& ctx)
{
    try_start_pending(ctx, TrinketScoreField::ROUND);
    try_start_pending(ctx, TrinketScoreField::TOTAL);
}

void score_count_tick(GameContext& ctx)
{
    tick_field(ctx, TrinketScoreField::ROUND);
    tick_field(ctx, TrinketScoreField::TOTAL);
}

void score_count_cancel(GameContext& ctx, TrinketScoreField field)
{
    score_count_fx(ctx, field).active = false;

    for(int index = ctx.state.pending_score_counts.size() - 1; index >= 0; --index)
    {
        if(ctx.state.pending_score_counts[index].field == field)
        {
            ctx.state.pending_score_counts.erase(ctx.state.pending_score_counts.begin() + index);
        }
    }
}

bool score_count_is_active(const GameContext& ctx, TrinketScoreField field)
{
    return score_count_fx(ctx, field).active;
}
