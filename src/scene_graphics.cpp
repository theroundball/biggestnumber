#include "scene_graphics.h"

#include "game_context.h"
#include "overworld_drops.h"
#include "score_pop_system.h"

void scene_graphics_reclaim_all()
{
    overworld_drops_hide_inspect_card();
    reclaim_scene_graphics_state();
    reset_score_pop_palette_cache();
    reset_victory_green_palette_cache();
}

void scene_graphics_release_card_pool(bn::span<Card> cards)
{
    for(Card& card : cards)
    {
        release_card_display_tiles(card);
        card.set_visible(false);
    }
}

void scene_graphics_leave_card_scene()
{
    scene_graphics_reclaim_all();
}

void scene_graphics_prepare_battle()
{
    scene_graphics_reclaim_all();
}
