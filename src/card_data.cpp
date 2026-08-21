#include "card_data.h"

#include "game_events.h"
#include "game_state.h"
#include "score_pop_system.h"
#include "score_count_system.h"
#include "trinket_system.h"

#include "bn_utility.h"
#include "bn_string.h"

#include "bn_sprite_items_sips_body.h"
#include "bn_sprite_items_sips_accent_top.h"
#include "bn_sprite_items_sips_accent_bottom.h"
#include "bn_sprite_items_longboard_body.h"
#include "bn_sprite_items_longboard_accent_top.h"
#include "bn_sprite_items_longboard_accent_bottom.h"
#include "bn_sprite_items_heelys_body.h"
#include "bn_sprite_items_heelys_accent_top.h"
#include "bn_sprite_items_heelys_accent_bottom.h"
#include "bn_sprite_items_scooter_body.h"
#include "bn_sprite_items_scooter_accent_top.h"
#include "bn_sprite_items_scooter_accent_bottom.h"
#include "bn_sprite_items_skateboard_body.h"
#include "bn_sprite_items_skateboard_accent_top.h"
#include "bn_sprite_items_skateboard_accent_bottom.h"
#include "bn_sprite_items_rollerblades_body.h"
#include "bn_sprite_items_rollerblades_accent_top.h"
#include "bn_sprite_items_rollerblades_accent_bottom.h"
#include "bn_sprite_items_wagon_body.h"
#include "bn_sprite_items_wagon_accent_top.h"
#include "bn_sprite_items_wagon_accent_bottom.h"
#include "bn_sprite_items_ripstik_body.h"
#include "bn_sprite_items_ripstik_accent_top.h"
#include "bn_sprite_items_ripstik_accent_bottom.h"
#include "bn_sprite_items_bike_body.h"
#include "bn_sprite_items_bike_accent_top.h"
#include "bn_sprite_items_bike_accent_bottom.h"
#include "bn_sprite_items_clover_body.h"
#include "bn_sprite_items_clover_accent_top.h"
#include "bn_sprite_items_clover_accent_bottom.h"
#include "bn_sprite_items_bigkurosawaburger_body.h"
#include "bn_sprite_items_bigkurosawaburger_accent_top.h"
#include "bn_sprite_items_bigkurosawaburger_accent_bottom.h"
#include "bn_sprite_items_rock_body.h"
#include "bn_sprite_items_rock_accent_top.h"
#include "bn_sprite_items_rock_accent_bottom.h"
#include "bn_sprite_items_paper_body.h"
#include "bn_sprite_items_paper_accent_top.h"
#include "bn_sprite_items_paper_accent_bottom.h"
#include "bn_sprite_items_scissors_body.h"
#include "bn_sprite_items_scissors_accent_top.h"
#include "bn_sprite_items_scissors_accent_bottom.h"
#include "bn_sprite_items_shoot_body.h"
#include "bn_sprite_items_shoot_accent_top.h"
#include "bn_sprite_items_shoot_accent_bottom.h"
#include "bn_sprite_items_lifeline_body.h"
#include "bn_sprite_items_lifeline_accent_top.h"
#include "bn_sprite_items_lifeline_accent_bottom.h"
#include "bn_sprite_items_snail_mail_body.h"
#include "bn_sprite_items_snail_mail_accent_top.h"
#include "bn_sprite_items_snail_mail_accent_bottom.h"
#include "bn_sprite_items_wishes_body.h"
#include "bn_sprite_items_wishes_accent_top.h"
#include "bn_sprite_items_wishes_accent_bottom.h"
#include "bn_sprite_items_swivel_body.h"
#include "bn_sprite_items_swivel_accent_top.h"
#include "bn_sprite_items_swivel_accent_bottom.h"
#include "bn_sprite_items_hacker_body.h"
#include "bn_sprite_items_hacker_accent_top.h"
#include "bn_sprite_items_hacker_accent_bottom.h"
#include "bn_sprite_items_pilot_body.h"
#include "bn_sprite_items_pilot_accent_top.h"
#include "bn_sprite_items_pilot_accent_bottom.h"
#include "bn_sprite_items_peanut_butter_body.h"
#include "bn_sprite_items_peanut_butter_accent_top.h"
#include "bn_sprite_items_peanut_butter_accent_bottom.h"
#include "bn_sprite_items_jelly_body.h"
#include "bn_sprite_items_jelly_accent_top.h"
#include "bn_sprite_items_jelly_accent_bottom.h"
#include "bn_sprite_items_straw_body.h"
#include "bn_sprite_items_straw_accent_top.h"
#include "bn_sprite_items_straw_accent_bottom.h"
#include "bn_sprite_items_stoller_body.h"
#include "bn_sprite_items_stoller_accent_top.h"
#include "bn_sprite_items_stoller_accent_bottom.h"
#include "bn_sprite_items_sticks_body.h"
#include "bn_sprite_items_sticks_accent_top.h"
#include "bn_sprite_items_sticks_accent_bottom.h"
#include "bn_sprite_items_bricks_body.h"
#include "bn_sprite_items_bricks_accent_top.h"
#include "bn_sprite_items_bricks_accent_bottom.h"
#include "bn_sprite_items_busted_body.h"
#include "bn_sprite_items_busted_accent_top.h"
#include "bn_sprite_items_busted_accent_bottom.h"
#include "bn_sprite_items_roundup_body.h"
#include "bn_sprite_items_roundup_accent_top.h"
#include "bn_sprite_items_roundup_accent_bottom.h"
#include "bn_sprite_items_librarian_body.h"
#include "bn_sprite_items_librarian_accent_top.h"
#include "bn_sprite_items_librarian_accent_bottom.h"
#include "bn_sprite_items_turtle_mode_body.h"
#include "bn_sprite_items_turtle_mode_accent_top.h"
#include "bn_sprite_items_turtle_mode_accent_bottom.h"
#include "bn_sprite_items_time_is_too_expensive_body.h"
#include "bn_sprite_items_time_is_too_expensive_accent_top.h"
#include "bn_sprite_items_time_is_too_expensive_accent_bottom.h"
#include "bn_sprite_items_birds_of_a_feather_body.h"
#include "bn_sprite_items_birds_of_a_feather_accent_top.h"
#include "bn_sprite_items_birds_of_a_feather_accent_bottom.h"
#include "bn_sprite_items_jacks_body.h"
#include "bn_sprite_items_jacks_accent_top.h"
#include "bn_sprite_items_jacks_accent_bottom.h"
#include "bn_sprite_items_fishing_pole_body.h"
#include "bn_sprite_items_fishing_pole_accent_top.h"
#include "bn_sprite_items_fishing_pole_accent_bottom.h"
#include "bn_sprite_items_cups_body.h"
#include "bn_sprite_items_cups_accent_top.h"
#include "bn_sprite_items_cups_accent_bottom.h"
#include "bn_sprite_items_roll_over_body.h"
#include "bn_sprite_items_roll_over_accent_top.h"
#include "bn_sprite_items_roll_over_accent_bottom.h"
#include "bn_sprite_items_toppings_body.h"
#include "bn_sprite_items_toppings_accent_top.h"
#include "bn_sprite_items_toppings_accent_bottom.h"

namespace
{
    #define SPRITE_BODY(name) &bn::sprite_items::name##_body
    #define SPRITE_ACCENT_TOP(name) &bn::sprite_items::name##_accent_top
    #define SPRITE_ACCENT_BOTTOM(name) &bn::sprite_items::name##_accent_bottom
    #define CARD_SPRITES(name) SPRITE_BODY(name), SPRITE_ACCENT_TOP(name), SPRITE_ACCENT_BOTTOM(name)
    void effect_draw1(GameState& state)
    {
        CardRef drawn;

        if(state.deck.draw(drawn))
        {
            hand_add_card(state, drawn, true);
        }
    }

    void effect_draw3(GameState& state)
    {
        for(int i = 0; i < 3; ++i)
        {
            CardRef drawn;

            if(!state.deck.draw(drawn))
            {
                break;
            }

            hand_add_card(state, drawn, true);
        }
    }

    void effect_scry_three(GameState& state)
    {
        const int count = state.deck.remaining() < 3 ? state.deck.remaining() : 3;

        if(count > 0)
        {
            state.pending_actions.push_back(PendingAction{PendingActionType::SCRY, count});
        }
    }

    void effect_scry_seven(GameState& state)
    {
        const int count = state.deck.size() < 7 ? state.deck.size() : 7;

        if(count > 0)
        {
            state.pending_actions.push_back(PendingAction{PendingActionType::SCRY, count});
        }
    }

    void effect_reclaim(GameState& state)
    {
        // Defer until after Lifeline itself is routed to the GY so it is included
        // in the shuffle (see SESSION_HANDOFF_LIFELINE / BRD §5.3 ordering).
        state.pending_actions.push_back(PendingAction{PendingActionType::RECLAIM_GRAVEYARD, 1});
    }

    void effect_clover(GameState& state)
    {
        if(state.graveyard.size() < 3)
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY;
        action.count = 3;
        state.pending_actions.push_back(action);
    }

    void effect_big_kurosawa_burger(GameState& state)
    {
        // Playing card is still in hand when on_play runs; need another card to discard.
        if(state.hand.size() <= 1)
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY;
        action.count = 4;
        state.pending_actions.push_back(action);
    }

    void effect_rags_to_riches(GameState& state)
    {
        if(state.graveyard.empty())
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT;
        state.pending_actions.push_back(action);
    }

    void effect_swivel(GameState& state)
    {
        // Playing card is still in hand when on_play runs; need another card to follow.
        if(state.hand.size() <= 1)
        {
            return;
        }

        state.swivel_waiting = true;
    }

    void effect_hacker(GameState& state)
    {
        state.deck.compact();

        if(state.deck.remaining() > 0)
        {
            state.pending_actions.push_back(PendingAction{PendingActionType::DECK_SEARCH});
        }
    }

    void effect_roundup(GameState& state)
    {
        ++state.roundup_play_count;

        int divisor = 10;

        if(state.roundup_play_count == 2)
        {
            divisor = 100;
        }
        else if(state.roundup_play_count >= 3)
        {
            divisor = 1000;
        }

        const int before = state.total_score;
        state.total_score = ((state.total_score + divisor - 1) / divisor) * divisor;
        const int roundup_gain = state.total_score - before;

        if(roundup_gain > 0)
        {
            score_pop_queue(state, roundup_gain, false, TrinketScoreField::TOTAL);
            score_count_queue(state, TrinketScoreField::TOTAL, before, state.total_score);
        }

        trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, state.total_score);
    }

    void effect_turtle_mode(GameState& state)
    {
        state.turtle_rounds_remaining = 3;
    }

    void effect_time_is_too_expensive(GameState& state)
    {
        state.add_from_card(state.current_round * 2);
    }

    void effect_busted_discard(GameState& state)
    {
        state.add_from_card(10);
    }

    void effect_birds_of_a_feather_play(GameState& state)
    {
        state.add_from_card(5);
    }

    void effect_necromancy(GameState& state)
    {
        if(state.graveyard.empty())
        {
            return;
        }

        state.pending_actions.push_back(PendingAction{PendingActionType::NECROMANCY_SHUFFLE});
    }

    void effect_miracle_play(GameState& state)
    {
        state.add_from_card(3);
    }

    bool score_contains_digit(int value, int digit)
    {
        if(value == 0)
        {
            return digit == 0;
        }

        int digits = value < 0 ? -value : value;

        while(digits > 0)
        {
            if(digits % 10 == digit)
            {
                return true;
            }

            digits /= 10;
        }

        return false;
    }

    void effect_jacks(GameState& state)
    {
        // Playing card is still in hand when on_play runs; need another card to discard.
        if(state.hand.size() <= 1 || state.graveyard.empty())
        {
            return;
        }

        state.pending_actions.push_back(PendingAction{PendingActionType::DISCARD_FROM_HAND});
        state.pending_actions.push_back(PendingAction{PendingActionType::RETRIEVE_FROM_GRAVEYARD});
    }

    void effect_fishing_pole(GameState& state)
    {
        if(state.hand.size() <= 1 || state.graveyard.empty())
        {
            return;
        }

        state.pending_actions.push_back(PendingAction{PendingActionType::DISCARD_FROM_HAND});
        state.pending_actions.push_back(PendingAction{PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP});
    }

    void effect_cups(GameState& state)
    {
        CardRef drawn;

        if(!state.deck.draw(drawn))
        {
            return;
        }

        hand_add_card(state, drawn, true);
        state.pending_actions.push_back(PendingAction{PendingActionType::DISCARD_FROM_HAND});

        if(!state.graveyard.empty())
        {
            state.pending_actions.push_back(PendingAction{PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP});
        }
    }

    void effect_swap(GameState& state)
    {
        const bn::string<12> round_digits = bn::to_string<12>(state.round.running);
        const bn::string<12> total_digits = bn::to_string<12>(state.total_score);

        if(round_digits.size() + total_digits.size() < 2)
        {
            return;
        }

        state.pending_actions.push_back(PendingAction{PendingActionType::SWAP_TOTAL_SCORE_DIGITS});
    }

    void effect_journal(GameState& state)
    {
        state.add_from_card(state.cards_played_this_round + 1);
    }

    void effect_triptych(GameState& state)
    {
        if(state.round.committed() % 3 == 0)
        {
            state.mul_from_card(3);
        }
    }

    void effect_seeds(GameState& state)
    {
        if(state.deck.remaining() < 3 || state.graveyard.size() < 3)
        {
            return;
        }

        state.deck.compact();
        state.deck.exile_random_undrawn(3, state.rng, state.exile);

        PendingAction action;
        action.type = PendingActionType::GRAVEYARD_PICK_TO_TOP;
        action.count = 3;
        state.pending_actions.push_back(action);
    }

    void effect_dilla(GameState& state)
    {
        const int amount = score_contains_digit(state.round.committed(), 0) ? 18 : 8;
        state.add_from_card(amount);
    }

    void effect_semaphore(GameState& state)
    {
        if(state.current_round == 1 && state.cards_played_this_round == 0)
        {
            state.add_from_card(100);
            return;
        }

        if(state.hand.size() == 1 && state.deck.empty())
        {
            const int factor = state.graveyard.size();

            if(factor > 1)
            {
                state.mul_from_card(factor);
            }

            return;
        }

        state.add_from_card(3);
    }

    void effect_bones_discard(GameState& state)
    {
        // Base: +2 per GY card (including this Bones). Each extra Bones already
        // in the GY adds another full +1-per-card pass on top.
        int bones_count = 0;

        for(const CardRef& card : state.graveyard)
        {
            if(card.type == CardType::BONES)
            {
                ++bones_count;
            }
        }

        const int amount = state.graveyard.size() * (1 + bones_count);

        if(amount > 0)
        {
            state.add_from_card(amount);
        }
    }

    void effect_threshold_play(GameState& state)
    {
        state.add_from_card(3);
    }

    void effect_threshold_discard(GameState& state)
    {
        if(state.graveyard.size() > 7)
        {
            CardRef drawn;

            if(state.deck.draw(drawn))
            {
                hand_add_card(state, drawn, true);
            }

            state.mul_from_card(3);
            return;
        }

        state.add_from_card(3);
    }

    void effect_tombstones_play(GameState& state)
    {
        state.add_from_card(3);
    }

    void effect_tombstones_discard(GameState& state)
    {
        const int factor = count_unique_graveyard_types(state);

        if(factor > 1)
        {
            state.mul_from_card(factor);
        }
    }

    void effect_roll_over_play(GameState& state)
    {
        state.add_from_card(3);

        if(state.graveyard.size() < 2)
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::GRAVEYARD_PAIR_SWAP;
        action.count = 3;
        state.pending_actions.push_back(action);
    }

    void effect_roll_over_discard(GameState& state)
    {
        (void)state;
    }

    void effect_toppings(GameState& state)
    {
        if(state.deck.remaining() == 0)
        {
            return;
        }

        PendingAction action;
        action.type = PendingActionType::PLAY_DECK_TOP;
        state.pending_actions.push_back(action);
    }

    CardData make_card(const char* name, const char* description,
                       int immediate_plus = 0, int immediate_multiply = 0,
                       RoundModifier future_0 = {}, RoundModifier future_1 = {},
                       RoundModifier future_2 = {},
                       void (*on_play)(GameState&) = nullptr,
                       void (*on_discard)(GameState&) = nullptr,
                       bool defer_graveyard_until_pending = false,
                       bool exiles_self_on_play = false,
                       const bn::sprite_item* body_item = nullptr,
                       const bn::sprite_item* accent_top_item = nullptr,
                       const bn::sprite_item* accent_bottom_item = nullptr)
    {
        return CardData{
            .name = name,
            .description = description,
            .body_item = body_item ? body_item : SPRITE_BODY(sips),
            .accent_top_item = accent_top_item ? accent_top_item : SPRITE_ACCENT_TOP(sips),
            .accent_bottom_item = accent_bottom_item ? accent_bottom_item : SPRITE_ACCENT_BOTTOM(sips),
            .immediate_plus = immediate_plus,
            .immediate_multiply = immediate_multiply,
            .future = { future_0, future_1, future_2 },
            .on_play = on_play,
            .on_discard = on_discard,
            .defer_graveyard_until_pending = defer_graveyard_until_pending,
            .exiles_self_on_play = exiles_self_on_play,
        };
    }
}

const CardData& card_data(CardType type)
{
    static const CardData table[] = {
        make_card("Sips", "+2 for each of the next 3 rounds.",
                  0, 0, RoundModifier{2, 0}, RoundModifier{2, 0}, RoundModifier{2, 0},
                  nullptr, nullptr, false, false, CARD_SPRITES(sips)),
        make_card("Longboard", "+1", 1, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(longboard)),
        make_card("Heelys", "+2", 2, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(heelys)),
        make_card("Scooter", "+3", 3, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(scooter)),
        make_card("Skateboard", "+4", 4, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(skateboard)),
        make_card("Roller Blades", "+5", 5, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(rollerblades)),
        make_card("Wagon", "+6", 6, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(wagon)),
        make_card("Stoller", "+7", 7, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(stoller)),
        make_card("Rip Stick", "+8", 8, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(ripstik)),
        make_card("Bike", "+9", 9, 0, {}, {}, {},
                  nullptr, nullptr, false, false, CARD_SPRITES(bike)),
        make_card("Clover", "Exile 3 cards from your graveyard, multiply round score by 3. Nothing happens when played if you don't have enough cards in your graveyard.",
                  0, 0, {}, {}, {}, effect_clover, nullptr, false, false, CARD_SPRITES(clover)),
        make_card("Big Kurosawa Burger", "Discard a hand card to multiply this round by 4. Nothing if this is your only card.",
                  0, 0, {}, {}, {}, effect_big_kurosawa_burger, nullptr, false, false, CARD_SPRITES(bigkurosawaburger)),
        make_card("Rock", "Part of the Rock-Paper-Scissors-Shoot combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(rock)),
        make_card("Paper", "Part of the Rock-Paper-Scissors-Shoot combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(paper)),
        make_card("Scissors", "Part of the Rock-Paper-Scissors-Shoot combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(scissors)),
        make_card("Shoot", "Complete Rock-Paper-Scissors-Shoot for +100.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(shoot)),
        make_card("Peanut Butter", "Part of the Peanut Butter and Jelly combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(peanut_butter)),
        make_card("Jelly", "Complete Peanut Butter and Jelly for +25.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(jelly)),
        make_card("Straw", "Part of the Straw, Sticks, and Bricks combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(straw)),
        make_card("Sticks", "Part of the Straw, Sticks, and Bricks combo.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(sticks)),
        make_card("Bricks", "Complete Straw, Sticks, and Bricks for +50.",
                  0, 0, {}, {}, {}, nullptr, nullptr, false, false, CARD_SPRITES(bricks)),
        make_card("Lifeline", "Shuffle your graveyard into deck, then exile 5 cards from your deck.",
                  0, 0, {}, {}, {}, effect_reclaim, nullptr, false, false, CARD_SPRITES(lifeline)),
        make_card("Snail Mail", "Draw 1 an extra card next round, draw an extra card two rounds from now, add 5 to your end of round multiplier three rounds from now.",
                  0, 0,
                  RoundModifier{0, 0, 1},
                  RoundModifier{0, 0, 1},
                  RoundModifier{0, 5},
                  nullptr, nullptr, false, false, CARD_SPRITES(snail_mail)),
        make_card("Wishes", "Draw 3 cards.", 0, 0, {}, {}, {}, effect_draw3, nullptr, false, false, CARD_SPRITES(wishes)),
        make_card("Busted", "+3 when played. +10 when discarded",
                  3, 0, {}, {}, {}, nullptr, effect_busted_discard, false, false, CARD_SPRITES(busted)),
        make_card("Swivel", "The next card played is put on top of your deck instead of in your graveyard.",
                  0, 0, {}, {}, {}, effect_swivel, nullptr, false, false, CARD_SPRITES(swivel)),
        make_card("Roundup", "Round total score up to the next highest tens place, then hundreds place, then thousandths place, and so on, incrementing each time this card is played.",
                  0, 0, {}, {}, {}, effect_roundup, nullptr, false, false, CARD_SPRITES(roundup)),
        make_card("Hacker", "Play any card from your deck.",
                  0, 0, {}, {}, {}, effect_hacker, nullptr, false, false, CARD_SPRITES(hacker)),
        make_card("Librarian", "Look at the top 7 cards of your deck, reorder them, and choose and play one.",
                  0, 0, {}, {}, {}, effect_scry_seven, nullptr, false, false, CARD_SPRITES(librarian)),
        make_card("Pilot", "Look at the top 3 cards of your deck, reorder them, and choose and play one. Reorder, play 1.", 0, 0, {}, {}, {}, effect_scry_three, nullptr, false, false, CARD_SPRITES(pilot)),
        make_card("Turtle Mode", "Delay round score evaluation for 3 rounds.",
                  0, 0, {}, {}, {}, effect_turtle_mode, nullptr, false, false, CARD_SPRITES(turtle_mode)),
        make_card("Time is Too Expensive", "Add 2 times the current round number to this round's score.",
                  0, 0, {}, {}, {}, effect_time_is_too_expensive, nullptr, false, false,
                  CARD_SPRITES(time_is_too_expensive)),
        make_card("Birds of a Feather", "+5 Return birds of a feather to you hand when there are n consecutive cards named Birds of a feather in your graveyard, where n = 1 + the number of times a card named birds of a feather has been played this game.",
                  0, 0, {}, {}, {}, effect_birds_of_a_feather_play, nullptr, false, false,
                  CARD_SPRITES(birds_of_a_feather)),
        make_card("Necromancy", "Exile this Necromancy, shuffle your graveyard and add it to the bottom of your deck.",
                  0, 0, {}, {}, {}, effect_necromancy, nullptr, false, true),
        make_card("Miracle", "Auto-plays for +10 if it is the first card drawn in a round or played from the top of your deck; else +3.",
                  0, 0, {}, {}, {}, effect_miracle_play),
        make_card("Rags to Riches", "Exile n cards from your graveyard cards, then multiply your round total by the number of cards exiled this way.",
                  0, 0, {}, {}, {}, effect_rags_to_riches),
        make_card("Jacks", "Discard another card, then put a card from your graveyard into your hand. Nothing happens if you have no other card to discard.",
                  0, 0, {}, {}, {}, effect_jacks, nullptr, false, false, CARD_SPRITES(jacks)),
        make_card("Fishing Pole", "Discard a card, then put a card from your graveyard on top of your deck. Nothing happens if you have no other card to discard.",
                  0, 0, {}, {}, {}, effect_fishing_pole, nullptr, false, false, CARD_SPRITES(fishing_pole)),
        make_card("Cups", "Draw 1, discard 1, then put a card from your graveyard on top of your deck. Nothing happens if you have no cards to draw.",
                  0, 0, {}, {}, {}, effect_cups, nullptr, false, false, CARD_SPRITES(cups)),
        make_card("Swap", "Choose two digits in your total or round score and swap them. The resulting number cannot be smaller",
                  0, 0, {}, {}, {}, effect_swap),
        make_card("Catnip", "+1 to this round. Draw 1.",
                  1, 0, {}, {}, {}, effect_draw1),
        make_card("Journal", "+n to this round, where n is cards played this round.",
                  0, 0, {}, {}, {}, effect_journal),
        make_card("Triptych", "+3 to this round. Multiply by 3 if the total is divisible by 3.",
                  3, 0, {}, {}, {}, effect_triptych),
        make_card("Seeds", "Exile 3 random cards from your deck, then pick 3 cards from your graveyard to put on top of your deck. Nothing happens if you have less than 3 cards in your deck.",
                  0, 0, {}, {}, {}, effect_seeds, nullptr, false),
        make_card("Dilla", "+18 to this round if your round score contains a 0. Otherwise +8.",
                  0, 0, {}, {}, {}, effect_dilla),
        make_card("Semaphore", "If Semaphore is the first card played of round 1: +100. Last card in hand with an empty deck: add multiplier of n, where n is the number of cards in your graveyard. Otherwise +3.",
                  0, 0, {}, {}, {}, effect_semaphore),
        make_card("Bones", "Add 1+n to your round score for each card in your graveyard, where n is the number of Bones cards in your graveyard.",
                  0, 0, {}, {}, {}, nullptr, effect_bones_discard),
        make_card("Threshold", "+3 when played. When discarded, if there are 7 or more cards in your graveyard draw 1 card and multiplyer your round score by 3.",
                  0, 0, {}, {}, {}, effect_threshold_play, effect_threshold_discard),
        make_card("Tombstones", "+3 when played. multiply your round score by n when discarded, where n is unique card names in your graveyard.",
                  0, 0, {}, {}, {}, effect_tombstones_play, effect_tombstones_discard),
        make_card("Roll Over", "+3 when played. And swap two graveyard cards, 3 times.",
                  0, 0, {}, {}, {}, effect_roll_over_play, effect_roll_over_discard, false, false,
                  CARD_SPRITES(roll_over)),
        make_card("Toppings", "Multiply this round by 2, then play the top card in your deck.",
                  0, 2, {}, {}, {}, effect_toppings, nullptr, false, false, CARD_SPRITES(toppings)),
    };

    static_assert(sizeof(table) / sizeof(table[0]) == int(CardType::COUNT),
                  "card_data table is out of sync with the CardType enum");

    return table[int(type)];
}

int count_unique_graveyard_types(const GameState& state)
{
    bool seen[int(CardType::COUNT)] = {};
    int unique_count = 0;

    for(const CardRef& card : state.graveyard)
    {
        if(!seen[int(card.type)])
        {
            seen[int(card.type)] = true;
            ++unique_count;
        }
    }

    return unique_count;
}

void reclaim_graveyard_into_deck(GameState& state)
{
    state.deck.compact();

    const bool had_graveyard = !state.graveyard.empty();

    while(!state.graveyard.empty())
    {
        state.deck.add_card(state.graveyard.back());
        state.graveyard.pop_back();
    }

    if(had_graveyard)
    {
        game_events_dispatch(state, GameEvent::GRAVEYARD_CHANGED);
    }

    state.deck.shuffle(state.rng);
    state.deck.apply_gravity(state.instance_pool);
    state.deck.exile_undrawn_end(LIFELINE_EXILE_COUNT, state.exile);
}
