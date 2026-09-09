#include "overworld_scene.h"

#include "bn_array.h"
#include "bn_backdrop.h"
#include "bn_core.h"
#include "bn_fixed.h"
#include "bn_fixed_point.h"
#include "bn_keypad.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_seed_random.h"
#include "bn_sprite_text_generator.h"

#include "battle_backdrop.h"
#include "campaign_scenes.h"
#include "common_variable_8x16_sprite_font.h"
#include "menu_scenes.h"
#include "overworld_drops.h"
#include "save_data.h"
#include "game_types.h"
#include "ui_common.h"

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

    struct NpcGuy
    {
        bn::optional<bn::sprite_ptr> sprite;
        bn::fixed x = 128;
        bn::fixed y = 72;
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

    void apply_player_visual(PlayerState& player);

    void ensure_player_sprite(PlayerState& player)
    {
        if(!player.sprite.has_value())
        {
            player.sprite = bn::sprite_items::biff_idle_down.create_sprite(0, 0);
        }
    }

    void ensure_npc_sprite(NpcGuy& npc)
    {
        if(!npc.sprite.has_value())
        {
            npc.sprite = bn::sprite_items::guy.create_sprite(0, 0);
        }
    }

    bn::fixed_point world_to_screen(bn::fixed world_x, bn::fixed world_y, const bn::fixed_point& camera)
    {
        return bn::fixed_point(
            world_x - camera.x() - SCREEN_HALF_W,
            world_y - camera.y() - SCREEN_HALF_H);
    }

    void draw_overworld_frame(PlayerState& player, NpcGuy& npc, const bn::fixed_point& camera)
    {
        ensure_player_sprite(player);
        ensure_npc_sprite(npc);
        player.sprite->set_position(world_to_screen(player.x, player.y, camera));
        npc.sprite->set_position(world_to_screen(npc.x, npc.y, camera));
        player.sprite->set_z_order(overworld_depth_z_order(player.y));
        npc.sprite->set_z_order(overworld_depth_z_order(npc.y));
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

    enum class NpcDialogueResult
    {
        CLOSED,
        OPEN_PLAY_MENU,
        OPEN_SHOP,
    };

    enum class YesNoResult
    {
        YES,
        NO,
        CANCELLED,
    };

    int dialogue_half_width(bn::sprite_text_generator& text_generator, const char* line_a,
                            const char* line_b, const char* option_label)
    {
        int half_width = text_generator.width(line_a) / 2;

        if(line_b)
        {
            const int half = text_generator.width(line_b) / 2;
            half_width = half_width > half ? half_width : half;
        }

        if(option_label)
        {
            const int half = text_generator.width(option_label) / 2;
            half_width = half_width > half ? half_width : half;
        }

        return half_width;
    }

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

    void tick_dialogue_backdrop()
    {
        battle_backdrop_tick();
    }

    void release_overworld_characters(PlayerState& player, NpcGuy& npc)
    {
        player.sprite.reset();
        npc.sprite.reset();
    }

    void restore_overworld_characters(PlayerState& player, NpcGuy& npc)
    {
        ensure_player_sprite(player);
        ensure_npc_sprite(npc);
        apply_player_visual(player);
    }

    void set_overworld_characters_visible(PlayerState& player, NpcGuy& npc, bool visible)
    {
        ensure_player_sprite(player);
        ensure_npc_sprite(npc);
        player.sprite->set_visible(visible);
        npc.sprite->set_visible(visible);
    }

    YesNoResult run_npc_yes_no(PlayerState& player, NpcGuy& npc, bn::fixed_point camera,
                               bn::sprite_text_generator& text_generator, SceneText& scene_text,
                               TextBoxPanel& panel, SelectorGlyph& selector, const char* line0,
                               const char* line1)
    {
        (void)player;
        (void)npc;
        (void)camera;

        wait_for_keypad_clear();

        int cursor = 0;
        constexpr int top_y = -52;
        constexpr int bottom_y = 28;
        constexpr int option_y_0 = 4;
        constexpr int option_y_1 = 20;
        setup_npc_dialogue_depth(panel, scene_text, selector);

        const int content_half_width =
            dialogue_half_width(text_generator, line0, line1, "Yes") + 4;
        panel.draw_around_lines(0, top_y, bottom_y, content_half_width);
        scene_text.draw_centered_line(top_y, line0);

        if(line1)
        {
            scene_text.draw_centered_line(top_y + 16, line1);
        }

        scene_text.draw_left_line(-40, option_y_0, "Yes");
        scene_text.draw_left_line(-40, option_y_1, "No");

        while(true)
        {
            tick_dialogue_backdrop();
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
                     TextBoxPanel& panel, SelectorGlyph& selector, const char* line0,
                     const char* line1)
    {
        (void)player;
        (void)npc;
        (void)camera;

        wait_for_keypad_clear();

        const int top_y = -52;
        const int bottom_y = line1 ? -20 : -36;
        setup_npc_dialogue_depth(panel, scene_text, selector);

        const int content_half_width = dialogue_half_width(text_generator, line0, line1, nullptr) + 4;
        panel.draw_around_lines(0, top_y, bottom_y, content_half_width);
        scene_text.draw_centered_line(top_y, line0);

        if(line1)
        {
            scene_text.draw_centered_line(top_y + 16, line1);
        }

        selector.set_visible(false);

        while(true)
        {
            tick_dialogue_backdrop();

            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                clear_npc_dialogue(panel, scene_text, selector);
                return;
            }

            bn::core::update();
        }
    }

    NpcDialogueResult run_npc_dialogue(PlayerState& player, NpcGuy& npc, bn::fixed_point camera)
    {
        release_overworld_characters(player, npc);

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        SceneText scene_text(text_generator);
        TextBoxPanel panel;
        SelectorGlyph selector(text_generator, -56);

        switch(run_npc_yes_no(player, npc, camera, text_generator, scene_text, panel, selector,
                              "So you think you know", "the biggest number, eh?"))
        {
        case YesNoResult::YES:
            return NpcDialogueResult::OPEN_PLAY_MENU;

        case YesNoResult::CANCELLED:
            return NpcDialogueResult::CLOSED;

        case YesNoResult::NO:
        default:
            break;
        }

        switch(run_npc_yes_no(player, npc, camera, text_generator, scene_text, panel, selector,
                              "You here to make", "stickers then?"))
        {
        case YesNoResult::YES:
            return NpcDialogueResult::OPEN_SHOP;

        case YesNoResult::CANCELLED:
            return NpcDialogueResult::CLOSED;

        case YesNoResult::NO:
        default:
            break;
        }

        run_npc_ack(player, npc, camera, text_generator, scene_text, panel, selector,
                    "A joke then. What's the", "tastiest number?");
        run_npc_ack(player, npc, camera, text_generator, scene_text, panel, selector, "...", nullptr);
        run_npc_ack(player, npc, camera, text_generator, scene_text, panel, selector, "PI!", nullptr);
        return NpcDialogueResult::CLOSED;
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
        bn::fixed camera_x = player.x - SCREEN_HALF_W;
        bn::fixed camera_y = player.y - SCREEN_HALF_H;

        const bn::fixed max_camera_x = MAP_PIXEL_W - 240;
        const bn::fixed max_camera_y = MAP_PIXEL_H - 160;

        if(camera_x < 0)
        {
            camera_x = 0;
        }
        else if(camera_x > max_camera_x)
        {
            camera_x = max_camera_x;
        }

        if(camera_y < 0)
        {
            camera_y = 0;
        }
        else if(camera_y > max_camera_y)
        {
            camera_y = max_camera_y;
        }

        return bn::fixed_point(camera_x, camera_y);
    }
}

OverworldSceneResult run_overworld_scene()
{
    static bool boot_services_ready = false;

    if(!boot_services_ready)
    {
        save_data_init();
        battle_backdrop_init();
        boot_services_ready = true;
    }

    while(bn::keypad::any_held())
    {
        battle_backdrop_tick();
        bn::core::update();
    }

    battle_backdrop_set_visible(true);
    bn::backdrop::set_color(bn::color(12, 18, 12));

    PlayerState player;
    NpcGuy npc;
    ensure_player_sprite(player);
    ensure_npc_sprite(npc);
    overworld_drops_clear_entity_blocks();
    overworld_drops_add_entity_block(npc.x, npc.y, 16, 22);
    player.sprite->set_z_order(overworld_depth_z_order(player.y));
    npc.sprite->set_z_order(overworld_depth_z_order(npc.y));

    if(g_resume_position.has_value())
    {
        player.x = g_resume_position->x();
        player.y = g_resume_position->y();
        g_resume_position.reset();
    }

    apply_player_visual(player);

    bn::fixed_point camera = update_camera(player);
    draw_overworld_frame(player, npc, camera);

    while(true)
    {
        if(!overworld_drops_active() && !overworld_drops_inspect_open() && bn::keypad::start_pressed())
        {
            set_overworld_characters_visible(player, npc, false);
            run_deck_list_build_scene(true);
            wait_for_keypad_clear();
        }
        else if(player_near_npc(player, npc) && !overworld_drops_active() && bn::keypad::a_pressed())
        {
            camera = update_camera(player);

            const NpcDialogueResult dialogue_result = run_npc_dialogue(player, npc, camera);
            restore_overworld_characters(player, npc);

            switch(dialogue_result)
            {
            case NpcDialogueResult::OPEN_PLAY_MENU:
                g_resume_position = bn::fixed_point(player.x, player.y);
                overworld_drops_set_spawn(player.x, player.y);
                set_overworld_characters_visible(player, npc, false);
                return OverworldSceneResult::OPEN_PLAY_MENU;

            case NpcDialogueResult::OPEN_SHOP:
            {
                set_overworld_characters_visible(player, npc, false);
                bn::seed_random shop_rng(bn::core::current_cpu_ticks() | 1u);
                run_campaign_shop_scene(shop_rng);
                wait_for_keypad_clear();
                break;
            }

            case NpcDialogueResult::CLOSED:
            default:
                wait_for_keypad_clear();
                break;
            }
        }

        camera = update_camera(player);

        const bool block_movement =
            overworld_drops_active() && overworld_drops_tick(player.x, player.y, camera);

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
        else if(overworld_drops_active())
        {
            tick_player_animation(player);
            apply_player_visual(player);
        }

        if(overworld_drops_inspect_open())
        {
            set_overworld_characters_visible(player, npc, false);
        }
        else
        {
            set_overworld_characters_visible(player, npc, true);
        }

        draw_overworld_frame(player, npc, camera);

        battle_backdrop_tick();
        bn::core::update();
    }
}
