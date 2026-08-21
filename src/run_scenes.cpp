#include "run_scenes.h"

#include "bn_array.h"
#include "bn_color.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_optional.h"
#include "bn_span.h"
#include "bn_sprite_item.h"
#include "bn_sprite_items_hud_lucky7.h"
#include "bn_sprite_items_hud_trinket_echo.h"
#include "bn_sprite_items_hud_trinket_morel.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "battle_backdrop.h"
#include "card.h"
#include "card_data.h"
#include "card_instance.h"
#include "card_meta.h"
#include "common_variable_8x16_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "game_helpers.h"
#include "game_types.h"
#include "ui_common.h"
#include "ui_inspect.h"

namespace
{
    constexpr TrinketType TRINKET_CATALOG[] = {
        TrinketType::MOREL,
        TrinketType::LUCKY_SEVENS,
        TrinketType::ECHO,
        TrinketType::GET_WITH_THE_TIMES,
        TrinketType::PRIME_TIME,
    };

    constexpr int TRINKET_CATALOG_COUNT = sizeof(TRINKET_CATALOG) / sizeof(TRINKET_CATALOG[0]);
    constexpr int MAX_RUN_TRINKETS = 3;
    constexpr int DROP_SPACING = game_layout::HAND_SPACING;
    constexpr int DROP_Y = game_layout::GRAVE_Y;
    constexpr int DROP_RAISE = game_layout::GRAVE_SELECTED_RAISE;

    const char* trinket_name(TrinketType type)
    {
        switch(type)
        {
        case TrinketType::MOREL:
            return "Morel";
        case TrinketType::LUCKY_SEVENS:
            return "Lucky 7";
        case TrinketType::ECHO:
            return "Echo";
        case TrinketType::GET_WITH_THE_TIMES:
            return "Get With The Times";
        case TrinketType::PRIME_TIME:
            return "Prime Time";
        default:
            return "None";
        }
    }

    template<int MaxSize>
    void tint_sprites_green(bn::vector<bn::sprite_ptr, MaxSize>& sprites)
    {
        if(sprites.empty())
        {
            return;
        }

        bn::span<const bn::color> source = sprites[0].palette().colors();
        bn::array<bn::color, 16> colors;

        for(int index = 0; index < 16; ++index)
        {
            colors[index] = index < source.size() ? source[index] : bn::color();
        }

        constexpr bn::color SCORE_GREEN(6, 28, 10);

        for(int index = 1; index < 16; ++index)
        {
            const bn::color& color = colors[index];

            if(color.red() + color.green() + color.blue() > 24)
            {
                colors[index] = SCORE_GREEN;
            }
        }

        const bn::sprite_palette_item item(
            bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
        const bn::sprite_palette_ptr green_palette = bn::sprite_palette_ptr::create_new(item);

        for(bn::sprite_ptr& sprite : sprites)
        {
            sprite.set_palette(green_palette);
        }
    }

    const bn::sprite_item* trinket_sprite_item(TrinketType type)
    {
        switch(type)
        {
        case TrinketType::MOREL:
            return &bn::sprite_items::hud_trinket_morel;
        case TrinketType::ECHO:
            return &bn::sprite_items::hud_trinket_echo;
        case TrinketType::LUCKY_SEVENS:
            return &bn::sprite_items::hud_lucky7;
        default:
            return nullptr;
        }
    }

    int count_enabled(const bool enabled[TRINKET_CATALOG_COUNT])
    {
        int count = 0;

        for(int index = 0; index < TRINKET_CATALOG_COUNT; ++index)
        {
            if(enabled[index])
            {
                ++count;
            }
        }

        return count;
    }

    void pack_trinkets(const bool enabled[TRINKET_CATALOG_COUNT], bn::array<TrinketType, 3>& out)
    {
        out = {TrinketType::NONE, TrinketType::NONE, TrinketType::NONE};
        int slot = 0;

        for(int index = 0; index < TRINKET_CATALOG_COUNT && slot < MAX_RUN_TRINKETS; ++index)
        {
            if(enabled[index])
            {
                out[slot++] = TRINKET_CATALOG[index];
            }
        }
    }

    using InstanceEligibleFn = bool (*)(const CardInstance&);

    bool eligible_any(const CardInstance&)
    {
        return true;
    }

    bool eligible_plus_digit(const CardInstance& instance)
    {
        return instance_can_plus_digit(instance);
    }

    bool eligible_increment_mult(const CardInstance& instance)
    {
        return instance_can_increment_mult(instance);
    }

    bool eligible_gravity(const CardInstance& instance)
    {
        return instance_can_gravity(instance);
    }

    void build_eligible_deck_refs(const RunState& run, InstanceEligibleFn eligible,
                                  bn::vector<CardRef, 50>& out_refs,
                                  bn::vector<int, 50>& out_deck_indices)
    {
        out_refs.clear();
        out_deck_indices.clear();

        for(int index = 0; index < run.deck_ids.size(); ++index)
        {
            const CardRef ref = run_deck_ref(run, index);
            const CardInstance* instance = instance_at(run.pool, ref.instance_id);

            if(!instance)
            {
                continue;
            }

            if(eligible && !eligible(*instance))
            {
                continue;
            }

            out_refs.push_back(ref);
            out_deck_indices.push_back(index);
        }
    }

    // Pick a run-deck card. Returns original deck_ids index, or -1 if cancelled / none.
    int pick_run_deck_card(RunState& run, const char* title, const char* confirm_line,
                           InstanceEligibleFn eligible)
    {
        wait_for_keypad_clear();

        bn::vector<CardRef, 50> refs;
        bn::vector<int, 50> deck_indices;
        build_eligible_deck_refs(run, eligible, refs, deck_indices);

        if(refs.empty())
        {
            return -1;
        }

        constexpr int WINDOW = game_layout::CARD_CAROUSEL_VISIBLE;
        constexpr int POOL = game_layout::CARD_CAROUSEL_POOL;
        constexpr int SPACING = game_layout::HAND_SPACING;
        constexpr int ROW_Y = game_layout::GRAVE_Y;
        constexpr int RAISE = game_layout::GRAVE_SELECTED_RAISE;
        constexpr int BADGE_Y = 12;

        bn::array<Card, POOL> card_pool;
        bn::array<int, 50> raise_offsets{};

        bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
        bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);
        bn::sprite_text_generator chrome_generator(common::variable_8x16_sprite_font);
        SceneText scene_text(chrome_generator);
        bn::vector<bn::sprite_ptr, 64> inspect_sprites;
        bn::vector<bn::sprite_ptr, 16> badge_sprites;

        int cursor = 0;
        int scroll_x = 0;
        int target_scroll_x = 0;
        bool inspecting = false;
        int inspect_shown_for = -1;
        DirectionRepeatState direction_repeat;

        while(true)
        {
            const int count = refs.size();
            const bool scrolling = scroll_x != target_scroll_x;
            move_toward_int(scroll_x, target_scroll_x, 6);

            for(int index = 0; index < count; ++index)
            {
                const int target_raise = (!inspecting && index == cursor) ? RAISE : 0;
                move_toward_int(raise_offsets[index], target_raise, 2);
            }

            scene_text.clear();
            badge_sprites.clear();

            if(inspecting)
            {
                for(Card& card : card_pool)
                {
                    release_card_display_tiles(card);
                }

                const CardRef& ref = refs[cursor];
                const CardInstance* instance = instance_at(run.pool, ref.instance_id);
                show_inspect_card(card_pool[0], ref.type, instance, &body_generator);

                if(inspect_shown_for != cursor)
                {
                    draw_card_inspect(ref.type, title_generator, body_generator, inspect_sprites,
                                      inspect_layout::TITLE_Y, instance);
                    inspect_shown_for = cursor;
                }
            }
            else
            {
                if(inspect_shown_for >= 0)
                {
                    inspect_sprites.clear();
                    inspect_shown_for = -1;
                }

                scene_text.draw_centered_line(-68, title);

                render_card_row(bn::span<Card>(card_pool.data(), card_pool.size()),
                                bn::span<const CardRef>(refs.data(), refs.size()), cursor, SPACING, ROW_Y,
                                scroll_x, target_scroll_x, 0,
                                bn::span<const int>(raise_offsets.data(), count),
                                &run.pool, &body_generator, WINDOW);

                const CardRef& selected = refs[cursor];
                const CardInstance* instance = instance_at(run.pool, selected.instance_id);

                if(instance)
                {
                    bn::string<32> suffix;
                    format_instance_upgrade_suffix(*instance, suffix);

                    if(!suffix.empty())
                    {
                        body_generator.set_center_alignment();
                        body_generator.generate(0, BADGE_Y, suffix, badge_sprites);
                        body_generator.set_left_alignment();
                    }
                }

                scene_text.draw_centered_line(52, confirm_line);
            }

            int direction = 0;
            bool direction_triggered = false;
            int direction_steps = 1;
            poll_direction_repeat(DirectionAxis::HORIZONTAL, direction_repeat, scrolling,
                                  direction, direction_triggered, direction_steps);

            if(direction_triggered)
            {
                cursor += direction * direction_steps;

                if(cursor < 0)
                {
                    cursor = 0;
                }
                else if(cursor >= count)
                {
                    cursor = count - 1;
                }

                target_scroll_x = first_visible_index(cursor, count, WINDOW) * SPACING;
            }

            const bool inspect_toggle = bn::keypad::select_pressed() || bn::keypad::down_pressed();

            if(inspect_toggle)
            {
                inspecting = !inspecting;

                if(!inspecting)
                {
                    inspect_sprites.clear();
                }
            }

            if(!inspecting && (bn::keypad::a_pressed() || bn::keypad::up_pressed()))
            {
                for(Card& card : card_pool)
                {
                    release_card_display_tiles(card);
                }

                return deck_indices[cursor];
            }

            if(bn::keypad::b_pressed())
            {
                if(inspecting)
                {
                    inspecting = false;
                    inspect_sprites.clear();
                }
                else
                {
                    for(Card& card : card_pool)
                    {
                        release_card_display_tiles(card);
                    }

                    return -1;
                }
            }

            battle_backdrop_tick();
            bn::core::update();
        }
    }

    bool apply_upgrade_gravity_at(RunState& run, int deck_index, Gravity gravity)
    {
        const CardRef ref = run_deck_ref(run, deck_index);
        CardInstance* instance = instance_at_mut(run.pool, ref.instance_id);

        if(!instance)
        {
            return false;
        }

        return instance_apply_gravity(*instance, gravity);
    }

    bool apply_plus_digit_at(RunState& run, int deck_index, bn::seed_random& rng)
    {
        const CardRef ref = run_deck_ref(run, deck_index);
        CardInstance* instance = instance_at_mut(run.pool, ref.instance_id);

        if(!instance)
        {
            return false;
        }

        const uint8_t digit = static_cast<uint8_t>(1 + rng.get_int(9));
        return instance_apply_plus_digit(*instance, digit);
    }

    bool apply_increment_mult_at(RunState& run, int deck_index)
    {
        const CardRef ref = run_deck_ref(run, deck_index);
        CardInstance* instance = instance_at_mut(run.pool, ref.instance_id);

        if(!instance)
        {
            return false;
        }

        return instance_apply_increment_mult(*instance);
    }
}

MenuSceneResult run_trinket_pick_scene(bn::array<TrinketType, 3>& out_trinkets)
{
    wait_for_keypad_clear();

    // Default loadout matches classic battles.
    bool enabled[TRINKET_CATALOG_COUNT] = {true, true, false, false, true};

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    // Separate generator so SceneText redraws never disturb the cursor glyph.
    bn::sprite_text_generator cursor_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(cursor_generator, -110);

    bn::array<bn::optional<bn::sprite_ptr>, TRINKET_CATALOG_COUNT> icons;

    for(int index = 0; index < TRINKET_CATALOG_COUNT; ++index)
    {
        if(const bn::sprite_item* item = trinket_sprite_item(TRINKET_CATALOG[index]))
        {
            icons[index] = item->create_sprite(-80, -36 + index * 18);
        }
    }

    int cursor = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        constexpr int ROW0 = -36;
        constexpr int ROW_PITCH = 18;
        constexpr int TEXT_X = -64;

        scene_text.clear();
        scene_text.draw_centered_line(-64, "Trinkets");
        scene_text.draw_centered_line(-52, "A toggle  Start go");

        for(int index = 0; index < TRINKET_CATALOG_COUNT; ++index)
        {
            const int y = ROW0 + index * ROW_PITCH;

            if(icons[index])
            {
                icons[index]->set_position(-80, y);
                icons[index]->set_visible(true);
            }

            bn::string<40> line;
            line.append(index == cursor ? "> " : "  ");
            line.append(enabled[index] ? "[ON] " : "[  ] ");
            line.append(trinket_name(TRINKET_CATALOG[index]));
            scene_text.draw_left_line(TEXT_X, y, line);
        }

        bn::string<24> count_line = "Equipped ";
        count_line.append(bn::to_string<4>(count_enabled(enabled)));
        count_line.append("/3");
        scene_text.draw_centered_line(60, count_line);

        // Keep glyph in sync with the in-line "> " (belt and suspenders).
        selector.set_position(TEXT_X - 12, ROW0 + cursor * ROW_PITCH);
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= TRINKET_CATALOG_COUNT)
            {
                cursor = TRINKET_CATALOG_COUNT - 1;
            }
        }

        if(bn::keypad::a_pressed())
        {
            if(enabled[cursor])
            {
                enabled[cursor] = false;
            }
            else if(count_enabled(enabled) < MAX_RUN_TRINKETS)
            {
                enabled[cursor] = true;
            }
        }

        if(bn::keypad::start_pressed())
        {
            pack_trinkets(enabled, out_trinkets);
            return MenuSceneResult::STAY;
        }

        if(bn::keypad::b_pressed())
        {
            return MenuSceneResult::MAIN_MENU;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_between_fights_scene(const RunState& run, int battle_score, bool new_peak)
{
    wait_for_keypad_clear();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    bn::vector<bn::sprite_ptr, 16> score_sprites;
    bool score_drawn = false;

    while(true)
    {
        scene_text.clear();
        scene_text.draw_centered_line(-40, "Fight score");

        if(!score_drawn)
        {
            text_generator.set_center_alignment();
            text_generator.generate(0, -24, bn::to_string<12>(battle_score), score_sprites);
            text_generator.set_left_alignment();

            if(new_peak)
            {
                tint_sprites_green(score_sprites);
            }

            score_drawn = true;
        }

        scene_text.draw_centered_line(-4, new_peak ? "New biggest number!" : "No new biggest");

        bn::string<32> peak_line = "Biggest Number ";
        peak_line.append(bn::to_string<12>(run.run_peak));
        scene_text.draw_centered_line(16, peak_line);

        bn::string<24> miss_line = "Misses ";
        miss_line.append(bn::to_string<4>(run.miss_streak));
        miss_line.append("/2");
        scene_text.draw_centered_line(32, miss_line);

        scene_text.draw_centered_line(52, "A continue");

        if(bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::start_pressed())
        {
            return MenuSceneResult::STAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_drop_pick_scene(RunState& run, int peak_before, int battle_score,
                                    bn::seed_random& rng)
{
    wait_for_keypad_clear();

    CardRarity slots[3];
    drop_merged_slot_rarities(peak_before, battle_score, slots);

    bn::vector<CardType, 50> type_deck;

    for(int index = 0; index < run.deck_ids.size(); ++index)
    {
        type_deck.push_back(run_deck_ref(run, index).type);
    }

    CardType offers[3];
    roll_drop_offers(type_deck, slots, rng, offers);

    bn::array<CardRef, 3> offer_refs = {
        CardRef{offers[0], NO_INSTANCE},
        CardRef{offers[1], NO_INSTANCE},
        CardRef{offers[2], NO_INSTANCE},
    };
    bn::array<Card, 3> card_pool;
    bn::array<int, 3> raise_offsets{};

    bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
    bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);
    // Scene chrome uses its own generator so inspect text isn't mixed with prompts.
    bn::sprite_text_generator chrome_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(chrome_generator);
    bn::vector<bn::sprite_ptr, 64> inspect_sprites;

    int cursor = 0;
    int scroll_x = 0;
    int target_scroll_x = 0;
    bool inspecting = false;
    int inspect_shown_for = -1;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        const bool scrolling = scroll_x != target_scroll_x;
        move_toward_int(scroll_x, target_scroll_x, 6);

        for(int index = 0; index < 3; ++index)
        {
            const int target_raise = (!inspecting && index == cursor) ? DROP_RAISE : 0;
            move_toward_raise(raise_offsets[index], target_raise);
        }

        scene_text.clear();

        if(inspecting)
        {
            for(Card& card : card_pool)
            {
                release_card_display_tiles(card);
            }

            show_inspect_card(card_pool[0], offer_refs[cursor].type, nullptr, &body_generator);

            if(inspect_shown_for != cursor)
            {
                draw_card_inspect(offer_refs[cursor].type, title_generator, body_generator, inspect_sprites,
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
            }

            scene_text.draw_centered_line(-68, "Choose a card");

            render_card_row(bn::span<Card>(card_pool.data(), card_pool.size()),
                            bn::span<const CardRef>(offer_refs.data(), offer_refs.size()), cursor,
                            DROP_SPACING, DROP_Y, scroll_x, target_scroll_x, 0,
                            bn::span<const int>(raise_offsets.data(), raise_offsets.size()));

            scene_text.draw_centered_line(52, "A pick  Select info");
        }

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::HORIZONTAL, direction_repeat, scrolling,
                              direction, direction_triggered, direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor > 2)
            {
                cursor = 2;
            }

            target_scroll_x = first_visible_index(cursor, 3, 3) * DROP_SPACING;
        }

        const bool inspect_toggle = bn::keypad::select_pressed() || bn::keypad::down_pressed();

        if(inspect_toggle)
        {
            inspecting = !inspecting;

            if(!inspecting)
            {
                inspect_sprites.clear();
            }
        }

        if(!inspecting && (bn::keypad::a_pressed() || bn::keypad::up_pressed()))
        {
            for(Card& card : card_pool)
            {
                release_card_display_tiles(card);
            }

            run_add_card(run, offer_refs[cursor].type);
            return MenuSceneResult::STAY;
        }

        if(bn::keypad::b_pressed())
        {
            if(inspecting)
            {
                inspecting = false;
                inspect_sprites.clear();
            }
            else
            {
                for(Card& card : card_pool)
                {
                    release_card_display_tiles(card);
                }

                return MenuSceneResult::MAIN_MENU;
            }
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_upgrade_node_scene(RunState& run, bn::seed_random& rng)
{
    wait_for_keypad_clear();

    constexpr const char* ITEMS[] = {
        "Remove a card",
        "+Digit",
        "x2",
        "Lead",
        "Yeast",
    };
    constexpr int ITEM_COUNT = 5;
    constexpr int ROW0 = -28;
    constexpr int ROW_PITCH = 16;
    constexpr int TEXT_X = -70;

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    bn::sprite_text_generator cursor_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(cursor_generator, TEXT_X - 14);

    int cursor = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        scene_text.clear();
        scene_text.draw_centered_line(-60, "Upgrade");
        scene_text.draw_centered_line(-46, "B skip");

        for(int index = 0; index < ITEM_COUNT; ++index)
        {
            bn::string<24> line;
            line.append(index == cursor ? "> " : "  ");
            line.append(ITEMS[index]);
            scene_text.draw_left_line(TEXT_X, ROW0 + index * ROW_PITCH, line);
        }

        selector.set_position(TEXT_X - 14, ROW0 + cursor * ROW_PITCH);
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= ITEM_COUNT)
            {
                cursor = ITEM_COUNT - 1;
            }
        }

        if(bn::keypad::a_pressed())
        {
            // Dismiss this menu's chrome before nested card-pick / inspect UI.
            scene_text.clear();
            selector.set_visible(false);
            bool applied = false;

            switch(cursor)
            {
            case 0:
            {
                if(run.deck_ids.size() <= 1)
                {
                    break;
                }

                const int size_before = run.deck_ids.size();
                const int picked = pick_run_deck_card(run, "Remove a card", "A remove  Select info",
                                                      eligible_any);

                if(picked >= 0 && run_remove_card_at(run, picked) &&
                   run.deck_ids.size() < size_before)
                {
                    applied = true;
                }

                break;
            }

            case 1:
            {
                const int picked = pick_run_deck_card(run, "+Digit", "A upgrade  Select info",
                                                      eligible_plus_digit);

                if(picked >= 0)
                {
                    applied = apply_plus_digit_at(run, picked, rng);
                }

                break;
            }

            case 2:
            {
                const int picked = pick_run_deck_card(run, "x2", "A upgrade  Select info",
                                                      eligible_increment_mult);

                if(picked >= 0)
                {
                    applied = apply_increment_mult_at(run, picked);
                }

                break;
            }

            case 3:
            {
                const int picked = pick_run_deck_card(run, "Lead", "A upgrade  Select info",
                                                      eligible_gravity);

                if(picked >= 0)
                {
                    applied = apply_upgrade_gravity_at(run, picked, Gravity::LEAD);
                }

                break;
            }

            case 4:
            {
                const int picked = pick_run_deck_card(run, "Yeast", "A upgrade  Select info",
                                                      eligible_gravity);

                if(picked >= 0)
                {
                    applied = apply_upgrade_gravity_at(run, picked, Gravity::YEAST);
                }

                break;
            }

            default:
                break;
            }

            if(applied)
            {
                return MenuSceneResult::STAY;
            }

            wait_for_keypad_clear();
            selector.set_visible(true);
        }

        if(bn::keypad::b_pressed())
        {
            return MenuSceneResult::STAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_trinket_offer_scene(RunState& run, bn::seed_random& rng)
{
    wait_for_keypad_clear();

    bn::vector<TrinketType, TRINKET_CATALOG_COUNT> candidates;

    for(int index = 0; index < TRINKET_CATALOG_COUNT; ++index)
    {
        const TrinketType type = TRINKET_CATALOG[index];
        bool owned = false;

        for(TrinketType equipped : run.trinkets)
        {
            if(equipped == type)
            {
                owned = true;
                break;
            }
        }

        if(!owned)
        {
            candidates.push_back(type);
        }
    }

    if(candidates.empty())
    {
        return MenuSceneResult::STAY;
    }

    TrinketType offers[2] = {TrinketType::NONE, TrinketType::NONE};
    int offer_count = 0;

    while(offer_count < 2 && !candidates.empty())
    {
        const int pick = rng.get_int(candidates.size());
        offers[offer_count++] = candidates[pick];
        candidates.erase(candidates.begin() + pick);
    }

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    bn::sprite_text_generator cursor_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(cursor_generator, -100);

    bn::array<bn::optional<bn::sprite_ptr>, 2> icons;

    for(int index = 0; index < offer_count; ++index)
    {
        if(const bn::sprite_item* item = trinket_sprite_item(offers[index]))
        {
            icons[index] = item->create_sprite(-72, -20 + index * 28);
        }
    }

    // Phase 0: pick offer. Phase 1: pick slot to replace (if no empty slot).
    int phase = 0;
    int cursor = 0;
    TrinketType chosen = TrinketType::NONE;
    DirectionRepeatState direction_repeat;

    auto hide_offer_icons = [&]()
    {
        for(int index = 0; index < 2; ++index)
        {
            if(icons[index])
            {
                icons[index]->set_visible(false);
            }
        }
    };

    while(true)
    {
        scene_text.clear();

        int row_count = 0;
        constexpr int ROW0 = -20;
        constexpr int ROW_PITCH = 28;
        constexpr int TEXT_X = -52;

        if(phase == 0)
        {
            scene_text.draw_centered_line(-56, "Trinket reward");
            scene_text.draw_centered_line(-40, "A take  B skip");
            row_count = offer_count;

            for(int index = 0; index < offer_count; ++index)
            {
                const int y = ROW0 + index * ROW_PITCH;

                if(icons[index])
                {
                    icons[index]->set_position(-72, y);
                    icons[index]->set_visible(true);
                }

                bn::string<36> line;
                line.append(index == cursor ? "> " : "  ");
                line.append(trinket_name(offers[index]));
                scene_text.draw_left_line(TEXT_X, y, line);
            }

            selector.set_position(TEXT_X - 12, ROW0 + cursor * ROW_PITCH);
            selector.set_visible(true);
        }
        else
        {
            hide_offer_icons();
            scene_text.draw_centered_line(-56, "Replace which?");
            scene_text.draw_centered_line(-40, "A swap  B back");
            row_count = MAX_RUN_TRINKETS;

            for(int index = 0; index < MAX_RUN_TRINKETS; ++index)
            {
                const int y = ROW0 + index * 20;
                bn::string<36> line;
                line.append(index == cursor ? "> " : "  ");
                line.append(trinket_name(run.trinkets[index]));
                scene_text.draw_left_line(TEXT_X, y, line);
            }

            selector.set_position(TEXT_X - 12, ROW0 + cursor * 20);
            selector.set_visible(true);
        }

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered && row_count > 0)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= row_count)
            {
                cursor = row_count - 1;
            }
        }

        if(bn::keypad::a_pressed())
        {
            if(phase == 0)
            {
                chosen = offers[cursor];
                int empty_slot = -1;

                for(int slot = 0; slot < MAX_RUN_TRINKETS; ++slot)
                {
                    if(run.trinkets[slot] == TrinketType::NONE)
                    {
                        empty_slot = slot;
                        break;
                    }
                }

                if(empty_slot >= 0)
                {
                    run.trinkets[empty_slot] = chosen;
                    hide_offer_icons();
                    selector.set_visible(false);
                    return MenuSceneResult::STAY;
                }

                phase = 1;
                cursor = 0;
                wait_for_keypad_clear();
            }
            else
            {
                run.trinkets[cursor] = chosen;
                hide_offer_icons();
                selector.set_visible(false);
                return MenuSceneResult::STAY;
            }
        }

        if(bn::keypad::b_pressed())
        {
            if(phase == 1)
            {
                phase = 0;
                cursor = 0;
                wait_for_keypad_clear();
            }
            else
            {
                hide_offer_icons();
                selector.set_visible(false);
                return MenuSceneResult::STAY;
            }
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_run_over_scene(const RunState& run)
{
    wait_for_keypad_clear();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);

    while(true)
    {
        scene_text.clear();
        scene_text.draw_centered_line(-24, "Run over");

        bn::string<32> peak_line = "Biggest Number ";
        peak_line.append(bn::to_string<12>(run.run_peak));
        scene_text.draw_centered_line(-4, peak_line);

        bn::string<24> cards_line = "Cards ";
        cards_line.append(bn::to_string<4>(run.deck_ids.size()));
        scene_text.draw_centered_line(20, cards_line);

        scene_text.draw_centered_line(44, "Any button");

        if(bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::start_pressed() ||
           bn::keypad::select_pressed() || bn::keypad::up_pressed() || bn::keypad::down_pressed() ||
           bn::keypad::left_pressed() || bn::keypad::right_pressed())
        {
            return MenuSceneResult::MAIN_MENU;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}
