#include "game_scene.h"

#include "bn_core.h"

#include "battle_backdrop.h"
#include "card_instance.h"
#include "game_context.h"
#include "score_count_system.h"
#include "score_pop_system.h"
#include "score_swap_system.h"
#include "trinket_system.h"

#include "save_data.h"

GameSceneResult run_game_scene(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch)
{
    GameContext ctx(collection, launch);

    while(! ctx.run_finished)
    {
        ctx.tick_combo();
        ctx.tick_panel();

        if(!ctx.lucky_sevens_fx.active && !ctx.deck_search_resolve_active())
        {
            ctx.handle_input();
        }

        ctx.sync_inspect_panel();
        ctx.tick_scroll();
        ctx.tick_card_raise();
        trinket_process_queues(ctx);
        score_pop_tick(ctx);
        // Re-drain after apply-on-arrive flights so Morel can flush the same frame Lucky lands.
        trinket_process_queues(ctx);
        score_pop_process_pending(ctx);
        score_count_tick(ctx);
        score_swap_tick(ctx);
        if(ctx.removing_card)
        {
            ctx.tick_removal_fx();
        }
        ctx.tick_round_end_pending();
        ctx.tick_echo_pending();
        swivel_tick_stall_recovery(ctx);
        trinket_tick_fx(ctx);
        score_pop_process_pending(ctx);
        ctx.tick_deck_search_resolve();
        ctx.tick_hand_draw_fx();
        ctx.tick_score_wiggles();
        ctx.render_frame();

        ctx.hud.set_visible(ctx.mode != GameMode::COMBO &&
                            !ctx.card_selection_ui_active() &&
                            !ctx.inspecting &&
                            ctx.side_panel == SidePanel::NONE && !ctx.panel_transition_active());
        ctx.hud.update(ctx.state);

        battle_backdrop_tick();
        bn::core::update();
    }

    return ctx.scene_result;
}

GameSceneResult run_game_scene(const bn::vector<CardType, 50>& collection, const BattleLaunch& launch)
{
    bn::vector<CardRef, 50> refs;

    for(int index = 0; index < collection.size(); ++index)
    {
        refs.push_back(CardRef{collection[index], NO_INSTANCE});
    }

    return run_game_scene(refs, launch);
}

GameSceneResult run_game_scene(const bn::vector<CardType, 50>& collection, int deck_index)
{
    BattleLaunch launch;
    launch.deck_index = deck_index;
    return run_game_scene(collection, launch);
}
