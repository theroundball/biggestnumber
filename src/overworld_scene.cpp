#include "overworld_scene.h"

#include "bn_array.h"
#include "bn_backdrop.h"
#include "bn_blending.h"
#include "bn_string.h"
#include "bn_core.h"
#include "bn_fixed.h"
#include "bn_fixed_point.h"
#include "bn_keypad.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_seed_random.h"
#include "bn_sprite_text_generator.h"

#include <new>

#ifndef BN_DATA_EWRAM_BSS
    #define BN_DATA_EWRAM_BSS __attribute__((section(".sbss")))
#endif

#include "battle_backdrop.h"
#include "campaign.h"
#include "card.h"
#include "scene_graphics.h"
#include "campaign_scenes.h"
#include "common_variable_8x16_sprite_font.h"
#include "menu_scenes.h"
#include "overworld_drops.h"
#include "save_data.h"
#include "game_types.h"
#include "ui_common.h"
#include "world_data.h"

#include "bn_sprite_items_biff_idle_diag_dl.h"
#include "bn_sprite_items_biff_idle_diag_ul.h"
#include "bn_sprite_items_biff_idle_down.h"
#include "bn_sprite_items_biff_idle_side.h"
#include "bn_sprite_items_biff_idle_up.h"
#include "bn_sprite_items_biff_walk_diag_dl.h"
#include "bn_sprite_items_biff_walk_diag_ul.h"
#include "bn_sprite_items_biff_walk_down.h"
#include "bn_sprite_items_biff_walk_side.h"
#include "bn_sprite_items_biff_walk_up.h"
#include "bn_sprite_items_guy.h"

namespace
{
    constexpr int MAP_TILES_W = 32;
    constexpr int MAP_TILES_H = 32;
    constexpr int TILE_SIZE = 8;
    constexpr int MAP_PIXEL_W = MAP_TILES_W * TILE_SIZE;
    constexpr int MAP_PIXEL_H = MAP_TILES_H * TILE_SIZE;
    constexpr bn::fixed SCREEN_HALF_W = 120;
    constexpr bn::fixed SCREEN_HALF_H = 80;
    constexpr bn::fixed PLAYER_SPEED = 1;
    constexpr bn::fixed DIAGONAL_SCALE = 0.71;
    constexpr int WALK_ANIM_DELAY = 10;
    constexpr bn::fixed NPC_INTERACT_RANGE_X = 20;
    constexpr bn::fixed NPC_INTERACT_RANGE_Y = 24;
    constexpr int OVERWORLD_NPC_COUNT = WORLD_NPC_COUNT + 1;
    constexpr int OVERWORLD_SHOP_NPC_INDEX = WORLD_NPC_COUNT;
    constexpr bn::fixed SHOP_NPC_X = 128;
    constexpr bn::fixed SHOP_NPC_Y = 152;

    enum class Facing8
    {
        DOWN,
        UP,
        LEFT,
        RIGHT,
        DOWN_LEFT,
        DOWN_RIGHT,
        UP_LEFT,
        UP_RIGHT,
    };

    bn::optional<bn::fixed_point> g_resume_position;
    bool g_pending_npc_battle = false;
    CampaignMode g_pending_npc_battle_mode = CampaignMode::NONE;
    int g_pending_npc_battle_index = -1;
    bool g_pending_npc_battle_loaner = false;

    struct NpcGuy
    {
        bn::optional<bn::sprite_ptr> sprite;
        bn::fixed x = 128;
        bn::fixed y = 72;
        int npc_index = -1;
    };

    struct PlayerState
    {
        bn::optional<bn::sprite_ptr> sprite;
        bn::fixed x = MAP_PIXEL_W / 2;
        bn::fixed y = MAP_PIXEL_H / 2;
        Facing8 facing = Facing8::DOWN;
        bool moving = false;
        int anim_frame = 0;
        int anim_timer = 0;
    };

    struct OverworldUi
    {
        bn::sprite_text_generator dialogue_generator;
        SceneText dialogue_text;
        TextBoxPanel dialogue_panel;
        SelectorGlyph dialogue_selector;

        OverworldUi()
        : dialogue_generator(common::variable_8x16_sprite_font),
          dialogue_text(dialogue_generator),
          dialogue_selector(dialogue_generator, -56)
        {}
    };

    alignas(OverworldUi) BN_DATA_EWRAM_BSS char overworld_ui_storage[sizeof(OverworldUi)];
    bool overworld_ui_ready = false;

    OverworldUi& overworld_ui()
    {
        if(!overworld_ui_ready)
        {
            new(reinterpret_cast<OverworldUi*>(overworld_ui_storage)) OverworldUi();
            overworld_ui_ready = true;
        }

        return *reinterpret_cast<OverworldUi*>(overworld_ui_storage);
    }

    void destroy_overworld_ui()
    {
        if(!overworld_ui_ready)
        {
            return;
        }

        reinterpret_cast<OverworldUi*>(overworld_ui_storage)->~OverworldUi();
        overworld_ui_ready = false;
    }

    void apply_player_visual(PlayerState& player);

    void ensure_player_sprite(PlayerState& player)
    {
        if(!player.sprite.has_value())
        {
            player.sprite = bn::sprite_items::biff_idle_down.create_sprite(0, 0);
            player.sprite->set_bg_priority(0);
            player.sprite->set_blending_enabled(false);
            player.sprite->set_visible(true);
        }
    }

    void ensure_npc_sprite(NpcGuy& npc)
    {
        if(!npc.sprite.has_value())
        {
            npc.sprite = bn::sprite_items::guy.create_sprite(0, 0);
            npc.sprite->set_bg_priority(0);
            npc.sprite->set_blending_enabled(false);
            npc.sprite->set_visible(true);
        }
    }

    bn::fixed_point world_to_screen(bn::fixed world_x, bn::fixed world_y, const bn::fixed_point& camera)
    {
        return bn::fixed_point(
            world_x - camera.x() - SCREEN_HALF_W,
            world_y - camera.y() - SCREEN_HALF_H);
    }

    void draw_overworld_frame(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs,
                              const bn::fixed_point& camera)
    {
        ensure_player_sprite(player);
        player.sprite->set_position(world_to_screen(player.x, player.y, camera));
        player.sprite->set_z_order(overworld_depth_z_order(player.y));

        if(!overworld_drops_active())
        {
            for(NpcGuy& npc : npcs)
            {
                ensure_npc_sprite(npc);
                npc.sprite->set_position(world_to_screen(npc.x, npc.y, camera));
                npc.sprite->set_z_order(overworld_depth_z_order(npc.y));
            }
        }
    }

    bool player_near_npc(const PlayerState& player, const NpcGuy& npc)
    {
        const bn::fixed dx = player.x - npc.x;

        if(dx < -NPC_INTERACT_RANGE_X || dx > NPC_INTERACT_RANGE_X)
        {
            return false;
        }

        const bn::fixed dy = player.y - npc.y;
        return dy >= -NPC_INTERACT_RANGE_Y && dy <= NPC_INTERACT_RANGE_Y;
    }

    int find_interact_npc_index(const PlayerState& player, const bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs)
    {
        int best_index = -1;
        bn::fixed best_distance = NPC_INTERACT_RANGE_X + NPC_INTERACT_RANGE_Y;

        for(int index = 0; index < OVERWORLD_NPC_COUNT; ++index)
        {
            if(!player_near_npc(player, npcs[index]))
            {
                continue;
            }

            const bn::fixed dx = player.x - npcs[index].x;
            const bn::fixed dy = player.y - npcs[index].y;
            const bn::fixed distance = bn::sqrt(dx * dx + dy * dy);

            if(best_index < 0 || distance < best_distance)
            {
                best_index = index;
                best_distance = distance;
            }
        }

        return best_index;
    }

    void init_overworld_npcs(bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs)
    {
        for(int index = 0; index < WORLD_NPC_COUNT; ++index)
        {
            const NpcDef& def = world_npc_def(index);
            npcs[index].x = def.x;
            npcs[index].y = def.y;
            npcs[index].npc_index = index;
        }

        npcs[OVERWORLD_SHOP_NPC_INDEX].x = SHOP_NPC_X;
        npcs[OVERWORLD_SHOP_NPC_INDEX].y = SHOP_NPC_Y;
        npcs[OVERWORLD_SHOP_NPC_INDEX].npc_index = -1;
    }

    void refresh_npc_entity_blocks(const bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs)
    {
        overworld_drops_clear_entity_blocks();

        for(const NpcGuy& npc : npcs)
        {
            overworld_drops_add_entity_block(npc.x, npc.y, 16, 22);
        }
    }

    enum class NpcDialogueResult
    {
        CLOSED,
        BATTLE_LOANER,
        BATTLE_OWN,
        OPEN_SHOP,
    };

    enum class YesNoResult
    {
        YES,
        NO,
        CANCELLED,
    };

    void clear_npc_dialogue(TextBoxPanel& panel, SceneText& scene_text, SelectorGlyph& selector)
    {
        panel.clear();
        scene_text.clear();
        selector.set_visible(false);
    }

    void setup_npc_dialogue_depth(TextBoxPanel& panel, SceneText& scene_text, SelectorGlyph& selector)
    {
        panel.set_z_order(game_layout::TEXT_BOX_Z);
        panel.set_bg_priority(game_layout::TEXT_BOX_BG_PRIORITY);
        scene_text.set_z_order(game_layout::OVERLAY_TEXT_Z);
        scene_text.set_bg_priority(game_layout::OVERLAY_TEXT_BG_PRIORITY);
        selector.set_z_order(game_layout::OVERLAY_TEXT_Z);
        selector.set_bg_priority(game_layout::OVERLAY_TEXT_BG_PRIORITY);
    }

    void release_overworld_characters(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs)
    {
        player.sprite.reset();

        for(NpcGuy& npc : npcs)
        {
            npc.sprite.reset();
        }
    }

    void restore_overworld_characters(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs)
    {
        ensure_player_sprite(player);
        player.sprite->set_visible(true);

        for(NpcGuy& npc : npcs)
        {
            ensure_npc_sprite(npc);
            npc.sprite->set_visible(true);
        }

        apply_player_visual(player);
    }

    void clear_overworld_overlay_ui()
    {
        if(!overworld_ui_ready)
        {
            return;
        }

        OverworldUi& ui = overworld_ui();
        ui.dialogue_text.clear();
        ui.dialogue_panel.clear();
        ui.dialogue_selector.set_visible(false);
    }

    void restore_overworld_world_state(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs,
                                       const bn::fixed_point& camera)
    {
        clear_overworld_overlay_ui();
        restore_overworld_characters(player, npcs);
        refresh_npc_entity_blocks(npcs);
        bn::blending::set_transparency_alpha(bn::fixed(1));
        battle_backdrop_set_visible(true);
        bn::backdrop::set_color(bn::color(12, 18, 12));
        for(int frame = 0; frame < 2; ++frame)
        {
            draw_overworld_frame(player, npcs, camera);
            bn::core::update();
        }
    }

    void set_overworld_characters_visible(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs,
                                          bool visible)
    {
        ensure_player_sprite(player);
        player.sprite->set_visible(visible);

        for(NpcGuy& npc : npcs)
        {
            ensure_npc_sprite(npc);
            npc.sprite->set_visible(visible);
        }
    }

    YesNoResult run_npc_yes_no(PlayerState& player, NpcGuy& npc, bn::fixed_point camera,
                               bn::sprite_text_generator& text_generator, SceneText& scene_text,
                               TextBoxPanel& panel, SelectorGlyph& selector,
                               const bn::string_view& line0, const bn::string_view& line1)
    {
        (void)player;
        (void)npc;
        (void)camera;

        wait_for_keypad_clear();

        int cursor = 0;
        constexpr int prompt_y_0 = -64;
        constexpr int prompt_y_1 = -48;
        constexpr int option_y_0 = -16;
        constexpr int option_y_1 = 0;
        constexpr int bottom_y = 12;
        setup_npc_dialogue_depth(panel, scene_text, selector);

        panel.draw_full_width_top(bottom_y);

        if(!line1.empty())
        {
            scene_text.draw_centered_line(prompt_y_0, line0);
            scene_text.draw_centered_line(prompt_y_1, line1);
        }
        else
        {
            scene_text.draw_centered_line(-56, line0);
        }

        scene_text.draw_left_line(-40, option_y_0, "Yes");
        scene_text.draw_left_line(-40, option_y_1, "No");

        while(true)
        {
            selector.set_position(-56, cursor == 0 ? option_y_0 : option_y_1);
            selector.set_visible(true);

            if(bn::keypad::up_pressed() && cursor > 0)
            {
                --cursor;
            }

            if(bn::keypad::down_pressed() && cursor < 1)
            {
                ++cursor;
            }

            if(bn::keypad::a_pressed())
            {
                clear_npc_dialogue(panel, scene_text, selector);
                return cursor == 0 ? YesNoResult::YES : YesNoResult::NO;
            }

            if(bn::keypad::b_pressed())
            {
                clear_npc_dialogue(panel, scene_text, selector);
                return YesNoResult::CANCELLED;
            }

            bn::core::update();
        }
    }

    void run_npc_ack(PlayerState& player, NpcGuy& npc, bn::fixed_point camera,
                     bn::sprite_text_generator& text_generator, SceneText& scene_text,
                     TextBoxPanel& panel, SelectorGlyph& selector, const bn::string_view& line0,
                     const bn::string_view& line1)
    {
        (void)player;
        (void)npc;
        (void)camera;

        wait_for_keypad_clear();

        constexpr int prompt_y_0 = -64;
        constexpr int prompt_y_1 = -48;
        const int bottom_y = !line1.empty() ? -20 : -36;
        setup_npc_dialogue_depth(panel, scene_text, selector);

        panel.draw_full_width_top(bottom_y);
        scene_text.draw_centered_line(prompt_y_0, line0);

        if(!line1.empty())
        {
            scene_text.draw_centered_line(prompt_y_1, line1);
        }

        selector.set_visible(false);

        while(true)
        {
            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                clear_npc_dialogue(panel, scene_text, selector);
                return;
            }

            bn::core::update();
        }
    }

    NpcDialogueResult run_shop_npc_dialogue(PlayerState& player, NpcGuy& npc, bn::fixed_point camera)
    {
        OverworldUi& ui = overworld_ui();
        ui.dialogue_text.clear();
        ui.dialogue_panel.clear();

        switch(run_npc_yes_no(player, npc, camera, ui.dialogue_generator, ui.dialogue_text, ui.dialogue_panel,
                              ui.dialogue_selector, "Need sticker paper", "upgrades?"))
        {
        case YesNoResult::YES:
            return NpcDialogueResult::OPEN_SHOP;

        case YesNoResult::CANCELLED:
        case YesNoResult::NO:
        default:
            return NpcDialogueResult::CLOSED;
        }
    }

    void show_tapped_out_hint(PlayerState& player, NpcGuy& npc, bn::fixed_point camera,
                              bn::sprite_text_generator& text_generator, SceneText& scene_text,
                              TextBoxPanel& panel, SelectorGlyph& selector, int npc_index)
    {
        const int alt_index = campaign_npc_first_takeable_index(save_data_get());
        bn::string<32> line1 = "I'm tapped out -";
        bn::string<32> line2 = "try someone else.";

        if(alt_index >= 0 && alt_index != npc_index)
        {
            line2 = "try ";
            line2.append(world_npc_def(alt_index).name);
            line2.append(".");
        }

        run_npc_ack(player, npc, camera, text_generator, scene_text, panel, selector, line1, line2);
    }

    NpcDialogueResult run_battle_npc_dialogue(PlayerState& player, NpcGuy& npc, bn::fixed_point camera,
                                              int npc_index)
    {
        OverworldUi& ui = overworld_ui();
        ui.dialogue_text.clear();
        ui.dialogue_panel.clear();
        const SaveData& save = save_data_get();
        const NpcDef& def = world_npc_def(npc_index);
        const bool has_takeable = campaign_npc_has_takeable_card(save, npc_index);
        const bool has_loaner = campaign_npc_total_cards(save, npc_index) > 0;

        if(!has_takeable)
        {
            show_tapped_out_hint(player, npc, camera, ui.dialogue_generator, ui.dialogue_text,
                                 ui.dialogue_panel, ui.dialogue_selector, npc_index);
        }

        if(has_loaner)
        {
            bn::string<32> borrow_line = "Borrow ";
            borrow_line.append(def.name);
            borrow_line.append("'s deck?");

            switch(run_npc_yes_no(player, npc, camera, ui.dialogue_generator, ui.dialogue_text,
                                  ui.dialogue_panel, ui.dialogue_selector, borrow_line, ""))
            {
            case YesNoResult::YES:
                return NpcDialogueResult::BATTLE_LOANER;

            case YesNoResult::CANCELLED:
                return NpcDialogueResult::CLOSED;

            case YesNoResult::NO:
            default:
                break;
            }
        }

        switch(run_npc_yes_no(player, npc, camera, ui.dialogue_generator, ui.dialogue_text,
                              ui.dialogue_panel, ui.dialogue_selector, "Battle with your", "own deck?"))
        {
        case YesNoResult::YES:
            return NpcDialogueResult::BATTLE_OWN;

        case YesNoResult::CANCELLED:
            return NpcDialogueResult::CLOSED;

        case YesNoResult::NO:
        default:
            return NpcDialogueResult::CLOSED;
        }
    }

    NpcDialogueResult run_npc_dialogue(PlayerState& player, bn::array<NpcGuy, OVERWORLD_NPC_COUNT>& npcs,
                                       int interact_index, bn::fixed_point camera)
    {
        release_overworld_characters(player, npcs);
        NpcGuy& npc = npcs[interact_index];

        if(interact_index == OVERWORLD_SHOP_NPC_INDEX)
        {
            return run_shop_npc_dialogue(player, npc, camera);
        }

        return run_battle_npc_dialogue(player, npc, camera, npcs[interact_index].npc_index);
    }

    bool facing_flipped(Facing8 facing)
    {
        return facing == Facing8::RIGHT || facing == Facing8::DOWN_RIGHT || facing == Facing8::UP_RIGHT;
    }

    Facing8 facing_from_delta(int dx, int dy)
    {
        if(dx < 0)
        {
            if(dy < 0)
            {
                return Facing8::UP_LEFT;
            }

            if(dy > 0)
            {
                return Facing8::DOWN_LEFT;
            }

            return Facing8::LEFT;
        }

        if(dx > 0)
        {
            if(dy < 0)
            {
                return Facing8::UP_RIGHT;
            }

            if(dy > 0)
            {
                return Facing8::DOWN_RIGHT;
            }

            return Facing8::RIGHT;
        }

        if(dy < 0)
        {
            return Facing8::UP;
        }

        return Facing8::DOWN;
    }

    const bn::sprite_item& idle_item_for(Facing8 facing)
    {
        switch(facing)
        {
        case Facing8::DOWN:
        case Facing8::DOWN_LEFT:
        case Facing8::DOWN_RIGHT:
            if(facing == Facing8::DOWN_LEFT)
            {
                return bn::sprite_items::biff_idle_diag_dl;
            }

            if(facing == Facing8::DOWN_RIGHT)
            {
                return bn::sprite_items::biff_idle_diag_dl;
            }

            return bn::sprite_items::biff_idle_down;

        case Facing8::UP:
        case Facing8::UP_LEFT:
        case Facing8::UP_RIGHT:
            if(facing == Facing8::UP_LEFT)
            {
                return bn::sprite_items::biff_idle_diag_ul;
            }

            if(facing == Facing8::UP_RIGHT)
            {
                return bn::sprite_items::biff_idle_diag_ul;
            }

            return bn::sprite_items::biff_idle_up;

        case Facing8::LEFT:
        case Facing8::RIGHT:
        default:
            return bn::sprite_items::biff_idle_side;
        }
    }

    const bn::sprite_item& walk_item_for(Facing8 facing)
    {
        switch(facing)
        {
        case Facing8::DOWN:
        case Facing8::DOWN_LEFT:
        case Facing8::DOWN_RIGHT:
            if(facing == Facing8::DOWN_LEFT || facing == Facing8::DOWN_RIGHT)
            {
                return bn::sprite_items::biff_walk_diag_dl;
            }

            return bn::sprite_items::biff_walk_down;

        case Facing8::UP:
        case Facing8::UP_LEFT:
        case Facing8::UP_RIGHT:
            if(facing == Facing8::UP_LEFT || facing == Facing8::UP_RIGHT)
            {
                return bn::sprite_items::biff_walk_diag_ul;
            }

            return bn::sprite_items::biff_walk_up;

        case Facing8::LEFT:
        case Facing8::RIGHT:
        default:
            return bn::sprite_items::biff_walk_side;
        }
    }

    void apply_player_visual(PlayerState& player)
    {
        const bn::sprite_item& item =
            player.moving ? walk_item_for(player.facing) : idle_item_for(player.facing);
        const int frame = player.moving ? player.anim_frame : 0;

        player.sprite->set_item(item, frame);
        player.sprite->set_horizontal_flip(facing_flipped(player.facing));
    }

    void tick_player_animation(PlayerState& player)
    {
        if(!player.moving)
        {
            player.anim_frame = 0;
            player.anim_timer = 0;
            return;
        }

        ++player.anim_timer;

        if(player.anim_timer >= WALK_ANIM_DELAY)
        {
            player.anim_timer = 0;
            player.anim_frame = 1 - player.anim_frame;
        }
    }

    bn::fixed clamp_player_coord(bn::fixed value, int map_pixels)
    {
        const bn::fixed min_value = 16;
        const bn::fixed max_value = map_pixels - 16;

        if(value < min_value)
        {
            return min_value;
        }

        if(value > max_value)
        {
            return max_value;
        }

        return value;
    }

    bn::fixed_point update_camera(const PlayerState& player)
    {
        return bn::fixed_point(player.x - SCREEN_HALF_W, player.y - SCREEN_HALF_H);
    }
}

bool overworld_take_pending_npc_battle(OverworldNpcBattleRequest& out_request)
{
    if(!g_pending_npc_battle)
    {
        return false;
    }

    out_request.mode = g_pending_npc_battle_mode;
    out_request.npc_index = g_pending_npc_battle_index;
    out_request.use_loaner_deck = g_pending_npc_battle_loaner;
    g_pending_npc_battle = false;
    return true;
}

OverworldSceneResult run_overworld_scene()
{
    scene_graphics_reclaim_all();

    while(bn::keypad::any_held())
    {
        battle_backdrop_tick();
        bn::core::update();
    }

    battle_backdrop_set_visible(true);
    bn::backdrop::set_color(bn::color(12, 18, 12));

    PlayerState player;
    bn::array<NpcGuy, OVERWORLD_NPC_COUNT> npcs;
    init_overworld_npcs(npcs);
    ensure_player_sprite(player);

    for(NpcGuy& npc : npcs)
    {
        ensure_npc_sprite(npc);
    }

    refresh_npc_entity_blocks(npcs);

    if(g_resume_position.has_value())
    {
        player.x = g_resume_position->x();
        player.y = g_resume_position->y();
        g_resume_position.reset();
    }

    apply_player_visual(player);

    bn::fixed_point camera = update_camera(player);
    draw_overworld_frame(player, npcs, camera);
    battle_backdrop_tick();
    bn::core::update();
    bool was_drops_active = false;

    while(true)
    {
        if(!overworld_drops_active() && !overworld_drops_inspect_open() && bn::keypad::start_pressed())
        {
            release_overworld_characters(player, npcs);
            clear_overworld_overlay_ui();
            overworld_drops_hide_inspect_card();
            run_deck_list_build_scene(true);
            scene_graphics_reclaim_all();
            wait_for_keypad_clear();
            camera = update_camera(player);
            restore_overworld_world_state(player, npcs, camera);
            was_drops_active = overworld_drops_active();
            continue;
        }
        else if(!overworld_drops_inspect_open() && bn::keypad::a_pressed())
        {
            const bool drop_pickup_pending =
                overworld_drops_active() && overworld_drops_has_selection();

            if(!drop_pickup_pending)
            {
                if(overworld_drops_active())
                {
                    overworld_drops_finalize_for_dialogue();
                    was_drops_active = false;
                    camera = update_camera(player);
                    restore_overworld_world_state(player, npcs, camera);
                }

                const int interact_index = find_interact_npc_index(player, npcs);

                if(interact_index >= 0)
                {
                    camera = update_camera(player);
                    clear_overworld_overlay_ui();

                    const NpcDialogueResult dialogue_result =
                        run_npc_dialogue(player, npcs, interact_index, camera);
                    restore_overworld_characters(player, npcs);

                    switch(dialogue_result)
                    {
                    case NpcDialogueResult::BATTLE_LOANER:
                    case NpcDialogueResult::BATTLE_OWN:
                    {
                        const int npc_index = npcs[interact_index].npc_index;

                        if(npc_index < 0 || npc_index >= WORLD_NPC_COUNT)
                        {
                            wait_for_keypad_clear();
                            break;
                        }

                        const NpcDef& def = world_npc_def(npc_index);
                        g_resume_position = bn::fixed_point(player.x, player.y);
                        overworld_drops_set_spawn(player.x, player.y);
                        clear_overworld_overlay_ui();
                        release_overworld_characters(player, npcs);
                        overworld_drops_hide_inspect_card();
                        destroy_overworld_ui();
                        g_pending_npc_battle_mode = def.mode;
                        g_pending_npc_battle_index = npc_index;
                        g_pending_npc_battle_loaner =
                            dialogue_result == NpcDialogueResult::BATTLE_LOANER;
                        g_pending_npc_battle = true;
                        return OverworldSceneResult::START_NPC_BATTLE;
                    }

                case NpcDialogueResult::OPEN_SHOP:
                {
                    release_overworld_characters(player, npcs);
                    bn::seed_random shop_rng(bn::core::current_cpu_ticks() | 1u);
                    run_campaign_shop_scene(shop_rng);
                    camera = update_camera(player);
                    restore_overworld_world_state(player, npcs, camera);
                        wait_for_keypad_clear();
                        break;
                    }

                    case NpcDialogueResult::CLOSED:
                    default:
                        wait_for_keypad_clear();
                        break;
                    }
                }
            }
        }

        camera = update_camera(player);

        const bool drops_active = overworld_drops_active();

        if(was_drops_active && !drops_active)
        {
            restore_overworld_world_state(player, npcs, camera);
        }

        was_drops_active = drops_active;

        const bool block_movement = drops_active && overworld_drops_tick(player.x, player.y, camera);

        if(!block_movement)
        {
            int dx = 0;
            int dy = 0;

            if(bn::keypad::left_held())
            {
                --dx;
            }

            if(bn::keypad::right_held())
            {
                ++dx;
            }

            if(bn::keypad::up_held())
            {
                --dy;
            }

            if(bn::keypad::down_held())
            {
                ++dy;
            }

            player.moving = dx != 0 || dy != 0;

            if(player.moving)
            {
                player.facing = facing_from_delta(dx, dy);

                bn::fixed move_x(dx);
                bn::fixed move_y(dy);

                if(dx != 0 && dy != 0)
                {
                    move_x *= DIAGONAL_SCALE;
                    move_y *= DIAGONAL_SCALE;
                }

                move_x *= PLAYER_SPEED;
                move_y *= PLAYER_SPEED;

                player.x = clamp_player_coord(player.x + move_x, MAP_PIXEL_W);
                player.y = clamp_player_coord(player.y + move_y, MAP_PIXEL_H);
            }

            tick_player_animation(player);
            apply_player_visual(player);
            camera = update_camera(player);
        }
        else if(drops_active)
        {
            tick_player_animation(player);
            apply_player_visual(player);
        }

        if(overworld_drops_inspect_open())
        {
            set_overworld_characters_visible(player, npcs, false);
        }
        else if(drops_active)
        {
            ensure_player_sprite(player);
            player.sprite->set_visible(true);

            for(NpcGuy& npc : npcs)
            {
                npc.sprite.reset();
            }
        }
        else
        {
            set_overworld_characters_visible(player, npcs, true);
        }

        draw_overworld_frame(player, npcs, camera);
        battle_backdrop_tick();
        bn::core::update();
    }
}
