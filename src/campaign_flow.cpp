#include "campaign_flow.h"

#include "bn_core.h"
#include "bn_keypad.h"

#include <new>

#ifndef BN_DATA_EWRAM_BSS
    #define BN_DATA_EWRAM_BSS __attribute__((section(".sbss")))
#endif

#include "battle_backdrop.h"
#include "campaign.h"
#include "campaign_scenes.h"
#include "campaign_types.h"
#include "card_instance.h"
#include "common_variable_8x16_sprite_font.h"
#include "game_scene.h"
#include "game_types.h"
#include "menu_scenes.h"
#include "overworld_drops.h"
#include "save_data.h"
#include "ui_common.h"

namespace
{
    alignas(BattleLaunch) BN_DATA_EWRAM_BSS char battle_launch_storage[sizeof(BattleLaunch)];
    bool battle_launch_ready = false;

    BattleLaunch& battle_launch()
    {
        if(!battle_launch_ready)
        {
            new(reinterpret_cast<BattleLaunch*>(battle_launch_storage)) BattleLaunch();
            battle_launch_ready = true;
        }

        return *reinterpret_cast<BattleLaunch*>(battle_launch_storage);
    }

    void reset_battle_launch()
    {
        battle_launch() = BattleLaunch{};
    }

    void campaign_load_trinkets(const SavedDeck& deck, bn::array<TrinketType, 3>& out_trinkets)
    {
        out_trinkets[0] = static_cast<TrinketType>(deck.trinkets[0]);
        out_trinkets[1] = static_cast<TrinketType>(deck.trinkets[1]);
        out_trinkets[2] = TrinketType::NONE;
    }

    void populate_launch_ui(const SaveData& save, CampaignMode mode, const CampaignBattleSetup& setup,
                            BattleLaunch& launch)
    {
        launch.campaign_ui.mode = mode;
        launch.campaign_ui.biggest_number_record =
            mode == CampaignMode::BIGGEST_NUMBER ? setup.peak_before : save.biggest_number_record;
        launch.campaign_ui.same_number_target = setup.same_number_target;
        launch.campaign_ui.number_now_scoring_round = setup.number_now_scoring_round;
        launch.campaign_ui.number_now_round_peak = setup.number_now_round_peak;
        launch.campaign_ui.aint_got_time_record =
            mode == CampaignMode::AINT_GOT_TIME ? setup.peak_before : save.aint_got_time_record;
        launch.campaign_ui.sharing_is_caring_record =
            mode == CampaignMode::SHARING_IS_CARING ? setup.peak_before : save.sharing_is_caring_record;
        launch.campaign_ui.poker_hand_record =
            mode == CampaignMode::POKER_HAND ? setup.peak_before : save.poker_hand_record;
        launch.campaign_ui.y2k_record = mode == CampaignMode::Y2K ? setup.peak_before : save.y2k_record;
    }

    bool campaign_ensure_starter_setup(bn::seed_random& rng)
    {
        (void)rng;

        if(!campaign_needs_starter_setup(save_data_get()))
        {
            return true;
        }

        CardType utility = CardType::TOPPINGS;

        if(run_campaign_starter_pick_scene(utility) == MenuSceneResult::MAIN_MENU)
        {
            return false;
        }

        return campaign_create_starter_deck(save_data_mut(), utility);
    }

    void campaign_show_message_scene(const bn::string_view& line0, const bn::string_view& line1 = "")
    {
        wait_for_keypad_clear();

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        SceneText scene_text(text_generator);
        scene_text.set_z_order(game_layout::OVERLAY_TEXT_Z);
        scene_text.set_bg_priority(game_layout::OVERLAY_TEXT_BG_PRIORITY);
        battle_backdrop_set_visible(true);

        while(true)
        {
            scene_text.clear();
            scene_text.draw_centered_line(-20, line0);

            if(!line1.empty())
            {
                scene_text.draw_centered_line(-4, line1);
            }

            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                return;
            }

            battle_backdrop_tick();
            bn::core::update();
        }
    }

    void run_campaign_battle(CampaignMode mode, bn::seed_random& rng, bool overworld_drops, int npc_index,
                               bool use_loaner_deck)
    {
        SaveData& save = save_data_mut();

        if(save.deck_count <= 0)
        {
            campaign_show_message_scene("No deck found", "Finish starter setup");
            return;
        }

        if(save.active_deck_index >= save.deck_count)
        {
            save.active_deck_index = 0;
        }

        if(npc_index >= WORLD_NPC_COUNT)
        {
            npc_index = -1;
        }

        const bool loaner_battle = use_loaner_deck && npc_index >= 0;

        if(loaner_battle && campaign_npc_total_cards(save, npc_index) <= 0)
        {
            campaign_show_message_scene("Loaner deck empty", "");
            return;
        }

        if(!loaner_battle)
        {
            campaign_repair_active_deck_from_library(save);
        }

        if(mode == CampaignMode::SAME_NUMBER && npc_index < 0)
        {
            campaign_prepare_same_number_target(save, rng);
        }

        SavedDeck battle_deck_state = save.decks[save.active_deck_index];
        bn::vector<CardRef, 50> battle_deck;

        if(loaner_battle)
        {
            campaign_flatten_npc_loaner(save, npc_index, battle_deck);
        }
        else
        {
            campaign_flatten_saved_deck(save, battle_deck_state, battle_deck);
        }

        if(battle_deck.empty())
        {
            campaign_show_message_scene("Deck has no cards", "Build a deck first");
            return;
        }

        const CampaignBattleSetup setup =
            campaign_battle_setup(save, mode, rng, battle_deck.size(), npc_index);

        if(overworld_drops && npc_index >= 0)
        {
            const int benchmark_rung = campaign_npc_benchmark_rung(save, npc_index);

            if(run_benchmark_pre_battle_scene(mode, benchmark_rung) == MenuSceneResult::MAIN_MENU)
            {
                return;
            }
        }

        if(!overworld_drops)
        {
            CampaignUiContext intro_ctx;
            intro_ctx.mode = mode;
            intro_ctx.biggest_number_record = setup.peak_before;
            intro_ctx.same_number_target = setup.same_number_target;
            intro_ctx.number_now_scoring_round = setup.number_now_scoring_round;
            intro_ctx.number_now_round_peak = setup.number_now_round_peak;
            intro_ctx.aint_got_time_record = setup.peak_before;
            intro_ctx.sharing_is_caring_record = setup.peak_before;
            intro_ctx.poker_hand_record = setup.peak_before;
            intro_ctx.y2k_record = setup.peak_before;

            if(run_mode_intro_scene(mode, intro_ctx) == MenuSceneResult::MAIN_MENU)
            {
                return;
            }
        }

        battle_backdrop_set_visible(true);

        reset_battle_launch();
        BattleLaunch& launch = battle_launch();
        launch.deck_index = loaner_battle ? -1 : save.active_deck_index;
        launch.score_to_beat = setup.peak_before;
        populate_launch_ui(save, mode, setup, launch);
        launch.campaign_ui.number_now_round_count =
            loaner_battle ? campaign_number_now_round_count(battle_deck.size())
                          : campaign_number_now_round_count(saved_deck_total_cards(battle_deck_state));

        launch.campaign_mode = mode;
        launch.npc_index = npc_index;
        launch.same_number_target = setup.same_number_target;
        launch.number_now_scoring_round = setup.number_now_scoring_round;
        launch.number_now_round_peak = setup.number_now_round_peak;

        if(loaner_battle)
        {
            launch.instance_pool = InstancePool{};
            launch.trinkets[0] = TrinketType::NONE;
            launch.trinkets[1] = TrinketType::NONE;
            launch.trinkets[2] = TrinketType::NONE;
            launch.longsleeve_cards[0] = CardRef{};
            launch.longsleeve_cards[1] = CardRef{};
        }
        else
        {
            instance_pool_clamp(save.instance_pool);
            launch.instance_pool = save.instance_pool;
            campaign_load_trinkets(battle_deck_state, launch.trinkets);
            saved_deck_resolve_longsleeve_cards(battle_deck_state, save.instance_pool, launch.longsleeve_cards);
        }

        const GameSceneResult game = run_game_scene(battle_deck, launch);

        if(game.exited_early)
        {
            return;
        }

        if(!overworld_drops)
        {
            campaign_grant_sticker_paper(save, 1);
        }

        const bool won =
            campaign_evaluate_win(save, mode, game, setup.peak_before, setup.same_number_target,
                                  setup.number_now_round_peak, setup.number_now_scoring_round);

        if(overworld_drops)
        {
            const int benchmark_rung =
                npc_index >= 0 ? campaign_npc_benchmark_rung(save, npc_index) : -1;
            const int next_benchmark_rung =
                npc_index >= 0 ? campaign_npc_next_benchmark_rung(save, npc_index) : -1;
            bool continue_to_overworld = false;
            run_campaign_battle_results_scene(mode, game, won, setup.same_number_target, continue_to_overworld,
                                              false, true, benchmark_rung, next_benchmark_rung);

            const bool counts_for_progress =
                won && !(loaner_battle ? false : saved_deck_unrestricted_build(battle_deck_state));

            if(counts_for_progress)
            {
                campaign_apply_win(save, mode, game, setup.number_now_scoring_round, rng, npc_index);
            }

            const int band_score =
                mode == CampaignMode::NUMBER_NOW ? game.last_round_score : game.final_score;
            overworld_drops_queue_from_battle(mode, won, setup.peak_before, band_score, rng, npc_index);
            return;
        }

        bool to_prize = false;
        run_campaign_battle_results_scene(mode, game, won, setup.same_number_target, to_prize,
                                          !overworld_drops);

        if(!won || !to_prize)
        {
            return;
        }

        if(saved_deck_unrestricted_build(battle_deck_state))
        {
            return;
        }

        campaign_apply_win(save, mode, game, setup.number_now_scoring_round, rng);

        const int band_score =
            mode == CampaignMode::NUMBER_NOW ? game.last_round_score : game.final_score;
        run_campaign_prize_scene(mode, setup.peak_before, band_score, rng);

        if(save.total_wins > 0 && save.total_wins % 10 == 0)
        {
            run_campaign_trinket_prize_scene(rng);
        }
    }
}

void campaign_run_boot_setup(bn::seed_random& rng)
{
    (void)rng;

    while(campaign_needs_starter_setup(save_data_get()))
    {
        CardType utility = CardType::TOPPINGS;

        if(run_campaign_starter_pick_scene(utility) == MenuSceneResult::MAIN_MENU)
        {
            continue;
        }

        if(campaign_create_starter_deck(save_data_mut(), utility))
        {
            break;
        }
    }
}

void campaign_run_play_flow(bn::seed_random& rng)
{
    if(!campaign_ensure_starter_setup(rng))
    {
        return;
    }

    while(true)
    {
        const CampaignPlayMenuResult menu = run_campaign_play_menu_scene(rng);

        if(menu.next == MenuSceneResult::MAIN_MENU)
        {
            return;
        }

        if(menu.next == MenuSceneResult::DECK_LIST_BUILD)
        {
            run_deck_list_build_scene();
            continue;
        }

        if(menu.next == MenuSceneResult::RUN_GAME && menu.mode != CampaignMode::NONE)
        {
            run_campaign_battle(menu.mode, rng, false, -1, false);
        }
    }
}

void campaign_run_overworld_play_flow(bn::seed_random& rng)
{
    if(!campaign_ensure_starter_setup(rng))
    {
        return;
    }

    const CampaignPlayMenuResult menu = run_campaign_play_menu_scene(rng);

    if(menu.next == MenuSceneResult::MAIN_MENU)
    {
        return;
    }

    if(menu.next == MenuSceneResult::DECK_LIST_BUILD)
    {
        run_deck_list_build_scene();
        return;
    }

    if(menu.next == MenuSceneResult::RUN_GAME && menu.mode != CampaignMode::NONE)
    {
        run_campaign_battle(menu.mode, rng, true, -1, false);
    }
}

void campaign_run_overworld_battle(bn::seed_random& rng, CampaignMode mode, int npc_index,
                                   bool use_loaner_deck)
{
    if(!campaign_ensure_starter_setup(rng) || mode == CampaignMode::NONE)
    {
        return;
    }

    run_campaign_battle(mode, rng, true, npc_index, use_loaner_deck);
}
