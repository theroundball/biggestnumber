#include "campaign_flow.h"

#include "campaign.h"
#include "campaign_scenes.h"
#include "campaign_types.h"
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

    void run_campaign_battle(CampaignMode mode, bn::seed_random& rng)
    {
        SaveData& save = save_data_mut();

        if(save.deck_count <= 0)
        {
            return;
        }

        // Lock (or keep) the Same Number challenge before battle — never re-roll while > 0.
        if(mode == CampaignMode::SAME_NUMBER)
        {
            campaign_prepare_same_number_target(save, rng);
        }

        const CampaignBattleSetup setup = campaign_battle_setup(save, mode, rng);
        const SavedDeck& deck = save.decks[save.active_deck_index];

        bn::vector<CardRef, 50> battle_deck;
        campaign_flatten_deck(save, save.active_deck_index, battle_deck);

        BattleLaunch launch;
        launch.deck_index = save.active_deck_index;
        launch.score_to_beat = setup.peak_before;
        launch.campaign_ui.mode = mode;
        launch.campaign_ui.biggest_number_record = save.biggest_number_record;
        launch.campaign_ui.same_number_target = setup.same_number_target;
        launch.campaign_ui.number_now_scoring_round = setup.number_now_scoring_round;
        launch.campaign_ui.number_now_round_peak = setup.number_now_round_peak;
        launch.campaign_ui.number_now_round_count =
            campaign_number_now_round_count(saved_deck_total_cards(deck));

        launch.campaign_mode = mode;
        launch.same_number_target = setup.same_number_target;
        launch.number_now_scoring_round = setup.number_now_scoring_round;
        launch.number_now_round_peak = setup.number_now_round_peak;
        launch.instance_pool = save.instance_pool;
        campaign_load_trinkets(deck, launch.trinkets);

        const GameSceneResult game = run_game_scene(battle_deck, launch);

        if(game.exited_early)
        {
            return;
        }

        const bool won =
            campaign_evaluate_win(save, mode, game, setup.peak_before, setup.same_number_target,
                                  setup.number_now_round_peak, setup.number_now_scoring_round);

        bool to_prize = false;
        run_campaign_battle_results_scene(mode, game, won, setup.same_number_target, to_prize);

        if(!won)
        {
            return;
        }

        if(saved_deck_unrestricted_build(deck))
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
