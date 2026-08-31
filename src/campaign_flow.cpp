#include "campaign_flow.h"

#include "campaign.h"
#include "campaign_scenes.h"
#include "campaign_types.h"
#include "card_instance.h"
#include "game_scene.h"
#include "menu_scenes.h"
#include "save_data.h"

namespace
{
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
        launch.campaign_ui.biggest_number_record = save.biggest_number_record;
        launch.campaign_ui.same_number_target = setup.same_number_target;
        launch.campaign_ui.number_now_scoring_round = setup.number_now_scoring_round;
        launch.campaign_ui.number_now_round_peak = setup.number_now_round_peak;
        launch.campaign_ui.aint_got_time_record = save.aint_got_time_record;
        launch.campaign_ui.sharing_is_caring_record = save.sharing_is_caring_record;
        launch.campaign_ui.poker_hand_record = save.poker_hand_record;
        launch.campaign_ui.y2k_record = save.y2k_record;
    }

    void run_campaign_battle(CampaignMode mode, bn::seed_random& rng)
    {
        SaveData& save = save_data_mut();

        if(save.deck_count <= 0)
        {
            return;
        }

        if(mode == CampaignMode::SAME_NUMBER)
        {
            campaign_prepare_same_number_target(save, rng);
        }

        const CampaignBattleSetup setup = campaign_battle_setup(save, mode, rng);

        CampaignUiContext intro_ctx;
        intro_ctx.mode = mode;
        intro_ctx.biggest_number_record = save.biggest_number_record;
        intro_ctx.same_number_target = setup.same_number_target;
        intro_ctx.number_now_scoring_round = setup.number_now_scoring_round;
        intro_ctx.number_now_round_peak = setup.number_now_round_peak;
        intro_ctx.aint_got_time_record = save.aint_got_time_record;
        intro_ctx.sharing_is_caring_record = save.sharing_is_caring_record;
        intro_ctx.poker_hand_record = save.poker_hand_record;
        intro_ctx.y2k_record = save.y2k_record;

        if(run_mode_intro_scene(mode, intro_ctx) == MenuSceneResult::MAIN_MENU)
        {
            return;
        }

        SavedDeck battle_deck_state = save.decks[save.active_deck_index];

        if(run_campaign_battle_deck_scene() != MenuSceneResult::STAY)
        {
            campaign_clear_ephemeral_battle_deck();
            return;
        }

        if(!campaign_take_ephemeral_battle_deck(save, battle_deck_state))
        {
            return;
        }

        bn::vector<CardRef, 50> battle_deck;
        campaign_flatten_saved_deck(save, battle_deck_state, battle_deck);

        BattleLaunch launch;
        launch.deck_index = save.active_deck_index;
        launch.score_to_beat = setup.peak_before;
        populate_launch_ui(save, mode, setup, launch);
        launch.campaign_ui.number_now_round_count =
            campaign_number_now_round_count(saved_deck_total_cards(battle_deck_state));

        launch.campaign_mode = mode;
        launch.same_number_target = setup.same_number_target;
        launch.number_now_scoring_round = setup.number_now_scoring_round;
        launch.number_now_round_peak = setup.number_now_round_peak;
        instance_pool_clamp(save.instance_pool);
        launch.instance_pool = save.instance_pool;
        campaign_load_trinkets(battle_deck_state, launch.trinkets);
        saved_deck_resolve_longsleeve_cards(battle_deck_state, save.instance_pool, launch.longsleeve_cards);

        const GameSceneResult game = run_game_scene(battle_deck, launch);

        if(game.exited_early)
        {
            campaign_clear_ephemeral_battle_deck();
            return;
        }

        campaign_grant_sticker_paper(save, 1);

        const bool won =
            campaign_evaluate_win(save, mode, game, setup.peak_before, setup.same_number_target,
                                  setup.number_now_round_peak, setup.number_now_scoring_round);

        bool to_prize = false;
        run_campaign_battle_results_scene(mode, game, won, setup.same_number_target, to_prize, true);

        campaign_clear_ephemeral_battle_deck();

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

void campaign_run_play_flow(bn::seed_random& rng)
{
    if(campaign_needs_starter_setup(save_data_get()))
    {
        CardType utility = CardType::JACKS;

        if(run_campaign_starter_pick_scene(utility) == MenuSceneResult::MAIN_MENU)
        {
            return;
        }

        if(!campaign_create_starter_deck(save_data_mut(), utility))
        {
            return;
        }
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
            run_campaign_battle(menu.mode, rng);
        }
    }
}
