#include "library_grid_pick_scene.h"

#include "bn_array.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_span.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "battle_backdrop.h"
#include "card.h"
#include "common_variable_8x16_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "game_helpers.h"
#include "game_types.h"
#include "ui_common.h"
#include "ui_inspect.h"

namespace
{
    // Match deck_editor_scene: pool only the two fully visible rows (VRAM).
    constexpr int GRID_COLS = 5;
    constexpr int GRID_FULL_ROWS = 2;
    constexpr int GRID_ROW_PITCH = 64;
    constexpr int GRID_COL_PITCH = 42;
    constexpr int GRID_TOP = -54;
    constexpr int GRID_BOTTOM = GRID_TOP + GRID_FULL_ROWS * GRID_ROW_PITCH;
    constexpr int GRID_POOL_SIZE = GRID_COLS * GRID_FULL_ROWS;
    constexpr int HEADER_Y = -72;

    int grid_left_x()
    {
        const int span = (GRID_COLS - 1) * GRID_COL_PITCH + game_layout::CARD_DISPLAY_WIDTH;
        return -span / 2;
    }

    int slot_row(int slot_index)
    {
        return slot_index / GRID_COLS;
    }

    int slot_col(int slot_index)
    {
        return slot_index % GRID_COLS;
    }

    int row_count_for(int slot_count)
    {
        return slot_count <= 0 ? 0 : (slot_count + GRID_COLS - 1) / GRID_COLS;
    }

    int max_scroll_for(int slot_count)
    {
        const int rows = row_count_for(slot_count);
        const int extra = rows - GRID_FULL_ROWS;
        return extra > 0 ? extra * GRID_ROW_PITCH : 0;
    }

    void update_grid_scroll_target(int cursor, int slot_count, int& target_scroll_y)
    {
        const int max_scroll = max_scroll_for(slot_count);
        const int row = slot_row(cursor);
        const int row_top = row * GRID_ROW_PITCH;
        const int window = GRID_FULL_ROWS * GRID_ROW_PITCH;

        if(row_top < target_scroll_y)
        {
            target_scroll_y = row_top;
        }
        else if(row_top + GRID_ROW_PITCH > target_scroll_y + window)
        {
            target_scroll_y = row_top + GRID_ROW_PITCH - window;
        }

        if(target_scroll_y < 0)
        {
            target_scroll_y = 0;
        }
        else if(target_scroll_y > max_scroll)
        {
            target_scroll_y = max_scroll;
        }
    }

    void move_grid_cursor(int dx, int dy, int steps, int slot_count, int& cursor, int& target_scroll_y)
    {
        if(slot_count <= 0 || steps <= 0)
        {
            return;
        }

        for(int step = 0; step < steps; ++step)
        {
            int row = slot_row(cursor);
            int col = slot_col(cursor);
            const int rows = row_count_for(slot_count);

            col += dx;
            row += dy;

            if(col < 0)
            {
                col = 0;
            }
            else if(col >= GRID_COLS)
            {
                col = GRID_COLS - 1;
            }

            if(row < 0)
            {
                row = 0;
            }
            else if(row >= rows)
            {
                row = rows - 1;
            }

            int next = row * GRID_COLS + col;

            if(next >= slot_count)
            {
                next = slot_count - 1;
            }

            if(next == cursor)
            {
                break;
            }

            cursor = next;
        }

        update_grid_scroll_target(cursor, slot_count, target_scroll_y);
    }

    void release_card_pool(bn::span<Card> pool)
    {
        for(Card& card : pool)
        {
            release_card_display_tiles(card);
        }
    }
}

LibraryGridPickResult run_library_grid_pick_scene(const char* title, const char* confirm_hint,
                                                  bn::span<const CardType> types)
{
    wait_for_keypad_clear();

    LibraryGridPickResult result;

    if(types.empty())
    {
        return result;
    }

    const int slot_count = types.size();
    const int grid_left = grid_left_x();

    bn::array<Card, GRID_POOL_SIZE> catalog_cards;

    for(Card& card : catalog_cards)
    {
        card.set_visible(false);
    }

    bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
    bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);
    bn::sprite_text_generator chrome_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(chrome_generator);
    bn::vector<bn::sprite_ptr, 64> inspect_sprites;

    int cursor = 0;
    int scroll_y = 0;
    int target_scroll_y = 0;
    update_grid_scroll_target(cursor, slot_count, target_scroll_y);
    scroll_y = target_scroll_y;

    bool inspecting = false;
    int inspect_shown_for = -1;
    DirectionRepeatState vertical_repeat;
    DirectionRepeatState horizontal_repeat;

    while(true)
    {
        move_toward(scroll_y, target_scroll_y, game_layout::SCROLL_SPEED);

        battle_backdrop_set_visible(true);
        scene_text.clear();

        if(inspecting)
        {
            release_card_pool(bn::span<Card>(catalog_cards.data(), catalog_cards.size()));
            show_inspect_card(catalog_cards[0], types[cursor], nullptr, &body_generator);

            if(inspect_shown_for != cursor)
            {
                draw_card_inspect(types[cursor], title_generator, body_generator, inspect_sprites,
                                  inspect_layout::TITLE_Y);
                inspect_shown_for = cursor;
            }
        }
        else
        {
            if(inspect_shown_for >= 0)
            {
                inspect_sprites.clear();
                inspect_shown_for = -1;
                hide_inspect_card(catalog_cards[0]);
            }

            scene_text.draw_centered_line(HEADER_Y, title);
            scene_text.draw_centered_line(68, confirm_hint);

            int pool_slot = 0;
            const int clip_top = GRID_TOP;
            const int clip_bottom = GRID_BOTTOM;

            for(int slot_index = 0; slot_index < slot_count; ++slot_index)
            {
                const int row = slot_row(slot_index);
                const int col = slot_col(slot_index);
                const int card_x = grid_left + col * GRID_COL_PITCH;
                const int card_y = GRID_TOP + row * GRID_ROW_PITCH - scroll_y;
                const int card_bottom = card_y + 64;

                if(card_bottom <= clip_top || card_y >= clip_bottom)
                {
                    continue;
                }

                if(card_x >= 120 || card_x + game_layout::CARD_DISPLAY_WIDTH <= -120)
                {
                    continue;
                }

                if(pool_slot >= catalog_cards.size())
                {
                    break;
                }

                const bool selected = slot_index == cursor;
                const int raise = selected ? game_layout::SELECTED_RAISE / 2 : 0;

                Card& card = catalog_cards[pool_slot];
                card.set_type(types[slot_index]);
                card.set_position(card_x, card_y - raise);
                card.set_visible(true);
                ++pool_slot;
            }

            for(int unused = pool_slot; unused < catalog_cards.size(); ++unused)
            {
                release_card_display_tiles(catalog_cards[unused]);
            }
        }

        int vertical_direction = 0;
        bool vertical_triggered = false;
        int vertical_steps = 1;
        int horizontal_direction = 0;
        bool horizontal_triggered = false;
        int horizontal_steps = 1;

        poll_direction_repeat(DirectionAxis::VERTICAL, vertical_repeat, false,
                              vertical_direction, vertical_triggered, vertical_steps);
        poll_direction_repeat(DirectionAxis::HORIZONTAL, horizontal_repeat, false,
                              horizontal_direction, horizontal_triggered, horizontal_steps);

        if(vertical_triggered)
        {
            move_grid_cursor(0, vertical_direction, vertical_steps, slot_count, cursor, target_scroll_y);
        }
        else if(horizontal_triggered)
        {
            move_grid_cursor(horizontal_direction, 0, horizontal_steps, slot_count, cursor, target_scroll_y);
        }

        const bool inspect_toggle = bn::keypad::select_pressed();

        if(inspect_toggle)
        {
            inspecting = !inspecting;

            if(!inspecting)
            {
                inspect_sprites.clear();
                inspect_shown_for = -1;
                hide_inspect_card(catalog_cards[0]);
            }
        }

        if(!inspecting && bn::keypad::a_pressed())
        {
            release_card_pool(bn::span<Card>(catalog_cards.data(), catalog_cards.size()));
            battle_backdrop_set_visible(true);
            result.picked = true;
            result.card = types[cursor];
            return result;
        }

        if(bn::keypad::b_pressed())
        {
            if(inspecting)
            {
                inspecting = false;
                inspect_sprites.clear();
                inspect_shown_for = -1;
                hide_inspect_card(catalog_cards[0]);
            }
            else
            {
                release_card_pool(bn::span<Card>(catalog_cards.data(), catalog_cards.size()));
                battle_backdrop_set_visible(true);
                return result;
            }
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}
