#ifndef CAMPAIGN_SCENES_H
#define CAMPAIGN_SCENES_H

#include "bn_seed_random.h"

#include "campaign_types.h"
#include "game_types.h"
#include "menu_scenes.h"
#include "ui_common.h"

// Rod / Jacks / Shells pick for a new campaign deck.
MenuSceneResult run_campaign_starter_pick_scene(CardType& out_utility);

// Play submenu: change deck + three modes.
struct CampaignPlayMenuResult
{
    MenuSceneResult next = MenuSceneResult::STAY;
    CampaignMode mode = CampaignMode::NONE;
};

CampaignPlayMenuResult run_campaign_play_menu_scene(bn::seed_random& rng);

// Post-battle results — returns next scene hint via out_to_prize.
// same_number_target is the challenge used for that battle (may differ from save after a win).
MenuSceneResult run_campaign_battle_results_scene(CampaignMode mode, const GameSceneResult& result,
                                                  bool won, int same_number_target, bool& out_to_prize);

MenuSceneResult run_campaign_prize_scene(CampaignMode mode, int peak_before, int band_score,
                                         bn::seed_random& rng);

MenuSceneResult run_campaign_trinket_prize_scene(bn::seed_random& rng);

MenuSceneResult run_campaign_status_scene();

MenuSceneResult run_campaign_sell_collection_flow(bn::seed_random& rng);

// L opens campaign status (R on status returns to the host menu).
// Clears host menu sprites first so they don't overlap.
bool campaign_poll_lr_status(MenuSceneResult& out_result, SceneText& host_text, SelectorGlyph& host_selector);

#endif
