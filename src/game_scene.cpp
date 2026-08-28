#include "game_scene.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_text_generator.h"

#include <new>

#ifndef BN_DATA_EWRAM_BSS
    #define BN_DATA_EWRAM_BSS __attribute__((section(".sbss")))
#endif

#include "battle_backdrop.h"
#include "card_instance.h"
#include "common_variable_8x16_sprite_font.h"
#include "game_context.h"
#include "game_events.h"
#include "score_count_system.h"
#include "score_pop_system.h"
#include "score_swap_system.h"
#include "trinket_system.h"
#include "ui_common.h"

#include "save_data.h"

namespace
{
    alignas(GameContext) BN_DATA_EWRAM_BSS char game_context_storage[sizeof(GameContext)];
    GameContext* active_game_context = nullptr;

    bool run_game_pause_menu()
    {
        wait_for_keypad_clear();

        bn::sprite_text_generator generator(common::variable_8x16_sprite_font);
        SceneText text(generator);
        text.set_z_order(-32767);
        text.set_bg_priority(0);
        int cursor = 0;

        while(true)
        {
            text.clear();
            text.draw_centered_line(-32, "Paused");
            text.draw_centered_line(-8, cursor == 0 ? "> Continue" : "  Continue");
            text.draw_centered_line(8, cursor == 1 ? "> Exit Game" : "  Exit Game");

            if(bn::keypad::up_pressed() || bn::keypad::down_pressed())
            {
                cursor = 1 - cursor;
            }

            if(bn::keypad::a_pressed())
            {
                const bool exit_game = cursor == 1;
                text.clear();
                wait_for_keypad_clear();
                return exit_game;
            }

            if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
            {
                text.clear();
                wait_for_keypad_clear();
                return false;
            }

            battle_backdrop_tick();
            bn::core::update();
        }
    }
}

GameSceneResult run_game_scene(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch)
{
    // GameContext is ~23KB; GBA main stack is ~16KB. EWRAM placement avoids stack overflow
    // corrupting palette/text generator state (manifests as "Unknown compression type: 229").
    if(active_game_context)
    {
        active_game_context->shutdown_for_exit();
        active_game_context->~GameContext();
        active_game_context = nullptr;
    }

    active_game_context = new(game_context_storage) GameContext(collection, launch);
    GameContext& ctx = *active_game_context;

    while(! ctx.run_finished)
    {
        if(bn::keypad::start_pressed())
        {
            if(run_game_pause_menu())
            {
                ctx.scene_result.exited_early = true;
                ctx.run_finished = true;
                ctx.shutdown_for_exit();
                break;
            }
        }

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
        ctx.tick_evaluate_ghost_steps();
        ctx.tick_score_wiggles();
        ctx.render_frame();

        ctx.hud.set_visible(ctx.mode != GameMode::COMBO &&
                            !ctx.card_selection_ui_active() &&
                            !ctx.inspecting &&
                            ctx.side_panel == SidePanel::NONE && !ctx.panel_transition_active());
        ctx.hud.update(ctx.state, deck_hud_display_count(ctx.state, ctx.hand_draw_fx_active));

        battle_backdrop_tick();
        bn::core::update();
    }

    GameSceneResult result = ctx.scene_result;
    ctx.shutdown_for_exit();
    ctx.~GameContext();
    active_game_context = nullptr;
    return result;
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
