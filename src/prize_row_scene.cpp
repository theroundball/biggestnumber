#include "prize_row_scene.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "battle_backdrop.h"
#include "campaign.h"
#include "card.h"
#include "common_variable_8x16_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "game_helpers.h"
#include "ui_common.h"
#include "ui_inspect.h"

namespace
{
    constexpr int DROP_SPACING = game_layout::HAND_SPACING;
    constexpr int DROP_Y = game_layout::GRAVE_Y;
    constexpr int DROP_RAISE = game_layout::GRAVE_SELECTED_RAISE;

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

    InstanceEligibleFn eligibility_for_upgrade(PrizeOfferKind kind)
    {
        switch(kind)
        {
        case PrizeOfferKind::UPGRADE_PLUS_DIGIT:
            return eligible_plus_digit;
        case PrizeOfferKind::UPGRADE_INCREMENT_MULT:
            return eligible_increment_mult;
        case PrizeOfferKind::UPGRADE_LEAD:
        case PrizeOfferKind::UPGRADE_YEAST:
            return eligible_gravity;
        default:
            return eligible_any;
        }
    }

    void release_card_pool(bn::span<Card> pool)
    {
        for(Card& card : pool)
        {
            release_card_display_tiles(card);
        }
    }

    void build_eligible_deck_refs(const SaveData& save, int deck_index, InstanceEligibleFn eligible,
                                  bn::vector<CardRef, 50>& out_refs,
                                  bn::vector<uint8_t, 50>& out_instance_ids)
    {
        out_refs.clear();
        out_instance_ids.clear();

        bn::vector<CardRef, 50> deck_refs;
        campaign_flatten_deck(save, deck_index, deck_refs);

        for(int index = 0; index < deck_refs.size(); ++index)
        {
            const CardRef& ref = deck_refs[index];
            const CardInstance* instance = instance_at(save.instance_pool, ref.instance_id);

            if(!instance)
            {
                continue;
            }

            if(eligible && !eligible(*instance))
            {
                continue;
            }

            out_refs.push_back(ref);
            out_instance_ids.push_back(ref.instance_id);
        }
    }

    CardType offer_card_type(const PrizeOffer& offer)
    {
        if(offer.kind == PrizeOfferKind::CARD)
        {
            return offer.card;
        }

        // Placeholder art until dedicated prize sprites exist.
        return CardType::SKATEBOARD;
    }
}

PrizeRowResult run_prize_row_scene(const char* title, const PrizeOffer* offers, int offer_count,
                                   const char* confirm_hint)
{
    wait_for_keypad_clear();

    PrizeRowResult result;

    if(offer_count <= 0 || !offers)
    {
        return result;
    }

    bn::array<CardRef, CAMPAIGN_PRIZE_SLOT_COUNT> offer_refs;
    bn::array<Card, CAMPAIGN_PRIZE_SLOT_COUNT> card_pool;
    bn::array<int, CAMPAIGN_PRIZE_SLOT_COUNT> raise_offsets{};

    for(int index = 0; index < offer_count; ++index)
    {
        offer_refs[index] = CardRef{offer_card_type(offers[index]), NO_INSTANCE};
    }

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
        const bool scrolling = scroll_x != target_scroll_x;
        move_toward_int(scroll_x, target_scroll_x, 6);

        for(int index = 0; index < offer_count; ++index)
        {
            const int target_raise = (!inspecting && index == cursor) ? DROP_RAISE : 0;
            move_toward_raise(raise_offsets[index], target_raise);
        }

        battle_backdrop_set_visible(true);
        scene_text.clear();

        if(inspecting)
        {
            // Drop the whole prize row, then rebuild only the inspect card.
            release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
            show_inspect_card(card_pool[0], offer_card_type(offers[cursor]), nullptr, &body_generator);

            if(inspect_shown_for != cursor)
            {
                draw_prize_offer_inspect(offers[cursor], title_generator, body_generator, inspect_sprites,
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

            scene_text.draw_centered_line(-68, title);
            render_card_row(bn::span<Card>(card_pool.data(), offer_count),
                            bn::span<const CardRef>(offer_refs.data(), offer_count), cursor, DROP_SPACING, DROP_Y,
                            scroll_x, target_scroll_x, 0,
                            bn::span<const int>(raise_offsets.data(), offer_count));
            scene_text.draw_centered_line(52, confirm_hint);
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
            else if(cursor >= offer_count)
            {
                cursor = offer_count - 1;
            }

            target_scroll_x = first_visible_index(cursor, offer_count, offer_count) * DROP_SPACING;
        }

        const bool inspect_toggle = bn::keypad::select_pressed() || bn::keypad::down_pressed();

        if(inspect_toggle)
        {
            inspecting = !inspecting;

            if(!inspecting)
            {
                inspect_sprites.clear();
                inspect_shown_for = -1;
            }
        }

        if(!inspecting && bn::keypad::a_pressed())
        {
            release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
            battle_backdrop_set_visible(true);

            result.picked = true;
            result.offer = offers[cursor];
            return result;
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
                release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
                battle_backdrop_set_visible(true);
                return result;
            }
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

bool run_upgrade_target_scene(SaveData& save, PrizeOfferKind upgrade_kind, bn::seed_random& rng,
                              uint8_t& out_instance_id)
{
    (void)rng;

    wait_for_keypad_clear();

    out_instance_id = NO_INSTANCE;

    if(save.deck_count <= 0)
    {
        return false;
    }

    const int deck_index = save.active_deck_index;
    InstanceEligibleFn eligible = eligibility_for_upgrade(upgrade_kind);

    bn::vector<CardRef, 50> refs;
    bn::vector<uint8_t, 50> instance_ids;
    build_eligible_deck_refs(save, deck_index, eligible, refs, instance_ids);

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
    bn::array<int, 50> raise_offsets{};

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
            release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
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

            scene_text.draw_centered_line(-68, upgrade_inspect_title(upgrade_kind));

            render_card_row(bn::span<Card>(card_pool.data(), card_pool.size()),
                            bn::span<const CardRef>(refs.data(), refs.size()), cursor, SPACING, ROW_Y, scroll_x,
                            target_scroll_x, 0, bn::span<const int>(raise_offsets.data(), count),
                            &save.instance_pool, &body_generator, WINDOW);

            scene_text.draw_centered_line(52, "A upgrade  Select info");
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

        const bool inspect_toggle = bn::keypad::select_pressed() || bn::keypad::down_pressed();

        if(inspect_toggle)
        {
            inspecting = !inspecting;

            if(!inspecting)
            {
                inspect_sprites.clear();
                inspect_shown_for = -1;
            }
        }

        if(!inspecting && bn::keypad::a_pressed())
        {
            release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
            battle_backdrop_set_visible(true);
            out_instance_id = instance_ids[cursor];
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
                release_card_pool(bn::span<Card>(card_pool.data(), card_pool.size()));
                battle_backdrop_set_visible(true);
                return false;
            }
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}
