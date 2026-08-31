#include "longsleeve_pick_scene.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "battle_backdrop.h"
#include "campaign.h"
#include "card.h"
#include "card_data.h"
#include "card_instance.h"
#include "common_variable_8x16_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "game_helpers.h"
#include "game_types.h"
#include "menu_scenes.h"
#include "ui_common.h"
#include "ui_inspect.h"

namespace
{
    void build_out_of_deck_refs(const SaveData& save, int deck_index, const SavedDeck* working_deck,
                                bn::vector<CardRef, InstancePool::CAPACITY>& out_refs)
    {
        out_refs.clear();

        bool in_deck[InstancePool::CAPACITY] = {};

        bn::vector<CardRef, 50> deck_refs;

        if(working_deck != nullptr)
        {
            campaign_flatten_saved_deck(save, *working_deck, deck_refs);
        }
        else if(deck_index >= 0 && deck_index < save.deck_count)
        {
            campaign_flatten_deck(save, deck_index, deck_refs);
        }

        for(const CardRef& ref : deck_refs)
        {
            if(ref.instance_id < InstancePool::CAPACITY)
            {
                in_deck[ref.instance_id] = true;
            }
        }

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = CardType(type_index);

            for(uint8_t id = 0; id < save.instance_pool.count; ++id)
            {
                const CardInstance& instance = save.instance_pool.entries[id];

                if(instance.base != type || in_deck[id])
                {
                    continue;
                }

                out_refs.push_back(CardRef{type, id});
            }
        }
    }

    void filter_existing_picks(bn::vector<CardRef, InstancePool::CAPACITY>& refs,
                               const bn::array<uint8_t, 2>& existing, int pick_index)
    {
        bn::vector<CardRef, InstancePool::CAPACITY> filtered;

        for(int index = 0; index < refs.size(); ++index)
        {
            bool duplicate = false;

            for(int prior = 0; prior < pick_index; ++prior)
            {
                if(existing[prior] != NO_INSTANCE && refs[index].instance_id == existing[prior])
                {
                    duplicate = true;
                    break;
                }
            }

            if(!duplicate)
            {
                filtered.push_back(refs[index]);
            }
        }

        refs = filtered;
    }

    bool pick_one_longsleeve(SaveData& save, int deck_index, const SavedDeck* working_deck,
                             const bn::array<uint8_t, 2>& existing, int pick_index,
                             uint8_t& out_instance_id)
    {
        wait_for_keypad_clear();

        bn::vector<CardRef, InstancePool::CAPACITY> refs;
        build_out_of_deck_refs(save, deck_index, working_deck, refs);
        filter_existing_picks(refs, existing, pick_index);

        if(refs.empty())
        {
            return false;
        }

        constexpr int WINDOW = game_layout::CARD_CAROUSEL_VISIBLE;
        constexpr int POOL = game_layout::CARD_CAROUSEL_POOL;
        constexpr int SPACING = game_layout::HAND_SPACING;
        constexpr int ROW_Y = game_layout::GRAVE_Y;
        constexpr int RAISE = game_layout::GRAVE_SELECTED_RAISE;

        bn::array<Card, POOL> card_pool;
        bn::array<int, InstancePool::CAPACITY> raise_offsets{};

        bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
        bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);
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
            const int count = refs.size();
            const bool scrolling = scroll_x != target_scroll_x;
            move_toward_int(scroll_x, target_scroll_x, 6);

            for(int index = 0; index < count; ++index)
            {
                const int target_raise = (!inspecting && index == cursor) ? RAISE : 0;
                move_toward_raise(raise_offsets[index], target_raise);
            }

            battle_backdrop_set_visible(true);
            scene_text.clear();

            if(inspecting)
            {
                const CardRef& ref = refs[cursor];
                const CardInstance* instance = instance_at(save.instance_pool, ref.instance_id);
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

                bn::string<32> title = "Longsleeves ";
                title.append(bn::to_string<1>(pick_index + 1));
                title.append("/2");
                scene_text.draw_centered_line(-68, title);
                scene_text.draw_centered_line(-56, "Collection card not in deck");

                render_card_row(bn::span<Card>(card_pool.data(), card_pool.size()),
                                bn::span<const CardRef>(refs.data(), refs.size()), cursor, SPACING, ROW_Y,
                                scroll_x, target_scroll_x, 0,
                                bn::span<const int>(raise_offsets.data(), count), &save.instance_pool,
                                &body_generator, WINDOW);

                bn::string<32> status = "Pick ";
                status.append(bn::to_string<4>(cursor + 1));
                status.append("/");
                status.append(bn::to_string<4>(count));
                scene_text.draw_centered_line(32, status);
                scene_text.draw_centered_line(44, "A pick  Select inspect  B cancel");
            }

            int direction = 0;
            bool direction_triggered = false;
            int direction_steps = 1;
            poll_direction_repeat(DirectionAxis::HORIZONTAL, direction_repeat, scrolling, direction,
                                  direction_triggered, direction_steps);

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

            if(bn::keypad::select_pressed())
            {
                inspecting = !inspecting;
            }

            if(bn::keypad::a_pressed() && !inspecting)
            {
                out_instance_id = refs[cursor].instance_id;
                inspect_sprites.clear();
                return true;
            }

            if(bn::keypad::b_pressed())
            {
                if(inspecting)
                {
                    inspecting = false;
                    inspect_sprites.clear();
                    inspect_shown_for = -1;
                }
                else
                {
                    inspect_sprites.clear();
                    return false;
                }
            }

            battle_backdrop_tick();
            bn::core::update();
        }
    }
}

bool run_longsleeve_deck_pick_scene(SaveData& save, int deck_index, const SavedDeck* working_deck,
                                    bn::array<uint8_t, 2>& out_instance_ids)
{
    out_instance_ids[0] = NO_INSTANCE;
    out_instance_ids[1] = NO_INSTANCE;

    if(!pick_one_longsleeve(save, deck_index, working_deck, out_instance_ids, 0, out_instance_ids[0]))
    {
        return false;
    }

    if(!pick_one_longsleeve(save, deck_index, working_deck, out_instance_ids, 1, out_instance_ids[1]))
    {
        out_instance_ids[0] = NO_INSTANCE;
        return false;
    }

    return true;
}
