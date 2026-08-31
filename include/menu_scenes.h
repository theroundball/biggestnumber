#ifndef MENU_SCENES_H
#define MENU_SCENES_H

#include "save_data.h"

enum class MenuSceneResult
{
    STAY,
    MAIN_MENU,
    DECK_LIST_BUILD,
    DECK_LIST_PLAY,
    RUN_GAME,
    SELL_COLLECTION,
    CAMPAIGN_SHOP,
};

struct DeckListResult
{
    MenuSceneResult next = MenuSceneResult::STAY;
    int deck_index = -1;
};

struct DeckEditorResult
{
    MenuSceneResult next = MenuSceneResult::STAY;
    bool ephemeral_confirmed = false;
};

MenuSceneResult run_main_menu_scene();
DeckListResult run_deck_list_build_scene();
DeckEditorResult run_deck_editor_scene(int deck_index, bool create_debug_deck = false,
                                       bool ephemeral_session = false);

#endif
