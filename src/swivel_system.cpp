#include "swivel_system.h"

#include "combo_system.h"
#include "game_context.h"
#include "game_helpers.h"

bool swivel_is_waiting(const GameContext& ctx)
{
    return ctx.state.swivel_waiting;
}

void swivel_on_swivel_played(GameContext& ctx)
{
    if(ctx.state.hand.empty())
    {
        return;
    }

    ctx.state.swivel_waiting = true;
    ctx.swivel_fx.swivel_type = CardType::SWIVEL;
}

namespace
{
    void finish_empty_hand_if_still_empty(GameContext& ctx)
    {
        if(!empty_hand_triggers_round_end(ctx.state) || ctx.mode != GameMode::NORMAL ||
           ctx.game_over || ctx.run_finished || ctx.removing_card || ctx.hand_draw_fx_blocking())
        {
            return;
        }

        if(ctx.block_round_end_for_combo())
        {
            return;
        }

        if(ctx.presentation_fx_blocking())
        {
            ctx.round_end_pending = true;
            return;
        }

        ctx.finish_empty_hand_round();
    }
}

void swivel_tick_stall_recovery(GameContext& ctx)
{
    if(!ctx.state.hand.empty() || ctx.removing_card || ctx.hand_draw_fx_blocking() ||
       ctx.mode != GameMode::NORMAL || ctx.game_over)
    {
        return;
    }

    swivel_clear_wait_if_hand_empty(ctx);

    if(ctx.presentation_fx_blocking() || ctx.hand_draw_fx_blocking())
    {
        if(empty_hand_triggers_round_end(ctx.state))
        {
            ctx.round_end_pending = true;
        }

        return;
    }

    ctx.begin_next_pending_or_finish();

    if(empty_hand_triggers_round_end(ctx.state))
    {
        finish_empty_hand_if_still_empty(ctx);
    }
}

void swivel_abandon_wait(GameContext& ctx)
{
    ctx.state.swivel_waiting = false;
    ctx.swivel_fx.swivel_type = CardType::COUNT;
}

void swivel_complete_follow(GameContext& ctx)
{
    ctx.state.swivel_waiting = false;
    ctx.swivel_fx.swivel_type = CardType::COUNT;

    // Echo of Swivel is drained by the shared idle gate after follow completes —
    // no card-specific echo_swivel_pending path.
    if(try_start_combo_cinematic(ctx.state))
    {
        ctx.enter_combo_mode();
        return;
    }

    ctx.begin_next_pending_or_finish();
    finish_empty_hand_if_still_empty(ctx);
}

void swivel_clear_wait_if_hand_empty(GameContext& ctx)
{
    if(ctx.state.hand.empty() && swivel_is_waiting(ctx))
    {
        swivel_abandon_wait(ctx);
    }
}
