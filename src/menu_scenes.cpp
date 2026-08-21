#include "menu_scenes.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_seed_random.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"

#include "battle_backdrop.h"

#include "campaign_scenes.h"
#include "common_variable_8x8_sprite_font.h"
#include "common_variable_8x16_sprite_font.h"
#include "save_data.h"
#include "ui_common.h"

namespace
{
    constexpr int SCROLL_SPEED = 6;
    constexpr int LIST_LINE_HEIGHT = 16;
    constexpr int LIST_START_Y = -20;

    int list_y_for_slot(int slot, int first_visible)
    {
        return LIST_START_Y + (slot - first_visible) * LIST_LINE_HEIGHT;
    }

    void dismiss_scene_ui(SceneText& scene_text, SelectorGlyph& selector)
    {
        scene_text.clear();
        selector.set_visible(false);
    }

    void apply_vertical_list_cursor(int direction, int steps, int& cursor, int row_count)
    {
        if(direction == 0 || steps <= 0 || row_count <= 0)
        {
            return;
        }

        cursor += direction * steps;

        if(cursor < 0)
        {
            cursor = 0;
        }
        else if(cursor >= row_count)
        {
            cursor = row_count - 1;
        }
    }
}

MenuSceneResult run_main_menu_scene()
{
    wait_for_keypad_clear();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(text_generator, -100);

    int cursor = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        MenuSceneResult status_result;

        if(campaign_poll_lr_status(status_result, scene_text, selector))
        {
            if(status_result == MenuSceneResult::SELL_COLLECTION)
            {
                bn::seed_random sell_rng(bn::core::current_cpu_ticks() | 1u);
                run_campaign_sell_collection_flow(sell_rng);
            }

            continue;
        }

        const bool show_build_deck = campaign_is_ready(save_data_get());
        const int item_count = show_build_deck ? 2 : 1;

        if(cursor >= item_count)
        {
            cursor = item_count - 1;
        }

        scene_text.clear();

        if(show_build_deck)
        {
            scene_text.draw_centered_line(LIST_START_Y, "Build Deck");
            scene_text.draw_centered_line(LIST_START_Y + LIST_LINE_HEIGHT, "Play Game");
        }
        else
        {
            scene_text.draw_centered_line(LIST_START_Y, "Play Game");
        }

        selector.set_position(list_y_for_slot(cursor, 0));
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            apply_vertical_list_cursor(direction, direction_steps, cursor, item_count);
        }

        if(bn::keypad::a_pressed())
        {
            if(show_build_deck)
            {
                if(cursor == 0)
                {
                    return MenuSceneResult::DECK_LIST_BUILD;
                }

                return MenuSceneResult::DECK_LIST_PLAY;
            }

            return MenuSceneResult::DECK_LIST_PLAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

DeckListResult run_deck_list_build_scene()
{
    wait_for_keypad_clear();

    DeckListResult result;

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(text_generator, -100);

    int cursor = 0;
    DirectionRepeatState direction_repeat;
    int scroll_y = 0;
    int target_scroll_y = 0;

    while(true)
    {
        MenuSceneResult status_result;

        if(campaign_poll_lr_status(status_result, scene_text, selector))
        {
            if(status_result == MenuSceneResult::SELL_COLLECTION)
            {
                bn::seed_random sell_rng(bn::core::current_cpu_ticks() | 1u);
                run_campaign_sell_collection_flow(sell_rng);
            }

            continue;
        }

        const SaveData& save = save_data_get();
        const bool show_debug_deck_row =
            !save_data_has_unrestricted_deck(save) && save.deck_count < MAX_SAVED_DECKS;
        const int header_row_count = 1 + (show_debug_deck_row ? 1 : 0);
        const int row_count = header_row_count + save.deck_count;
        const int first_visible = first_visible_index(cursor, row_count, 5);
        target_scroll_y = first_visible * LIST_LINE_HEIGHT;
        move_toward_int(scroll_y, target_scroll_y, SCROLL_SPEED);

        scene_text.clear();

        for(int slot = 0; slot < 5; ++slot)
        {
            const int row = first_visible + slot;

            if(row >= row_count)
            {
                break;
            }

            const int y = LIST_START_Y + slot * LIST_LINE_HEIGHT - (scroll_y - target_scroll_y);

            if(row == 0)
            {
                scene_text.draw_centered_line(y, "+ New Deck");
            }
            else if(show_debug_deck_row && row == 1)
            {
                scene_text.draw_centered_line(y, "+ Debug Deck");
            }
            else
            {
                const int deck_row = row - header_row_count;
                bn::string<20> line;

                if(deck_row == save.active_deck_index)
                {
                    line = "* ";
                }

                line.append(saved_deck_display_name(save.decks[deck_row]));
                scene_text.draw_centered_line(y, line);
            }
        }

        const int selector_slot = cursor - first_visible;

        if(selector_slot >= 0 && selector_slot < 5)
        {
            selector.set_position(LIST_START_Y + selector_slot * LIST_LINE_HEIGHT - (scroll_y - target_scroll_y));
            selector.set_visible(true);
        }
        else
        {
            selector.set_visible(false);
        }

        const bool scrolling = scroll_y != target_scroll_y;
        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, scrolling, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            apply_vertical_list_cursor(direction, direction_steps, cursor, row_count);
        }

        if(bn::keypad::a_pressed())
        {
            if(cursor == 0)
            {
                if(save.deck_count >= MAX_SAVED_DECKS)
                {
                    continue;
                }

                dismiss_scene_ui(scene_text, selector);
                run_deck_editor_scene(-1, false);
                return result;
            }

            if(show_debug_deck_row && cursor == 1)
            {
                dismiss_scene_ui(scene_text, selector);
                run_deck_editor_scene(-1, true);
                return result;
            }

            dismiss_scene_ui(scene_text, selector);
            campaign_set_active_deck(save_data_mut(), cursor - header_row_count);
            save_data_write();
            run_deck_editor_scene(cursor - header_row_count, false);
            return result;
        }

        if(bn::keypad::b_pressed())
        {
            result.next = MenuSceneResult::MAIN_MENU;
            return result;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}
