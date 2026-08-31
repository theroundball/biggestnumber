#include "play_resolution.h"

#include "card.h"
#include "card_data.h"
#include "card_type.h"
#include "game_events.h"
#include "game_helpers.h"
#include "game_state.h"

namespace
{
    void hand_remove(GameState& state, int hand_index, int* selected_card, bool to_graveyard, bool trigger_discard)
    {
        if(hand_index < 0 || hand_index >= state.hand.size())
        {
            return;
        }

        int cursor = selected_card ? *selected_card : hand_index;

        if(to_graveyard)
        {
            if(trigger_discard)
            {
                hand_remove_at_to_graveyard(state, hand_index, cursor);
            }
            else
            {
                hand_remove_at_to_graveyard_played(state, hand_index, cursor);
            }
        }
        else
        {
            hand_remove_at_exiled(state, hand_index, cursor);
        }

        if(selected_card)
        {
            *selected_card = cursor;
        }
    }

    bool should_apply_play_zone(PostPlayDestination dest, PlaySource source, bool apply_destination)
    {
        if(source == PlaySource::ECHO)
        {
            return false;
        }

        if(dest == PostPlayDestination::NONE)
        {
            return false;
        }

        if(dest == PostPlayDestination::DECK_TOP)
        {
            return apply_destination;
        }

        return true;
    }

    void apply_bones_gy_entry(GameState& state)
    {
        int bones_count = 0;

        for(const CardRef& card : state.graveyard)
        {
            if(card.type == CardType::BONES)
            {
                ++bones_count;
            }
        }

        const int factor = bones_count + 1;
        state.mul_from_card(factor);
    }

    void apply_tombstones_gy_entry(GameState& state)
    {
        const int n = state.graveyard.size();
        state.add_from_card(n);
    }
}

PostPlayDestination route_played_card(const GameState& state, CardType type, PlaySource source,
                                      bool swivel_follow)
{
    (void)state;
    const CardData& data = card_data(type);

    if(source == PlaySource::ECHO || source == PlaySource::LONGSLEEVE)
    {
        return PostPlayDestination::NONE;
    }

    if(source == PlaySource::GHOST)
    {
        return PostPlayDestination::EXILE;
    }

    if(data.exiles_self_on_play)
    {
        return source == PlaySource::HAND ? PostPlayDestination::EXILE : PostPlayDestination::NONE;
    }

    if(swivel_follow)
    {
        return PostPlayDestination::DECK_TOP;
    }

    return PostPlayDestination::GRAVEYARD;
}

void apply_post_play_destination(GameState& state, CardRef card, PlaySource source,
                                 PostPlayDestination dest, int hand_index, int* selected_card,
                                 bool trigger_discard_on_graveyard)
{
    switch(dest)
    {
    case PostPlayDestination::EXILE:
        if(source == PlaySource::HAND)
        {
            hand_remove(state, hand_index, selected_card, false, false);
        }
        else if(source == PlaySource::GHOST)
        {
            if(hand_index >= 0 && hand_index < state.graveyard.size())
            {
                const CardRef removed = state.graveyard[hand_index];
                graveyard_remove_at(state, hand_index);
                exile_push(state, removed, true);
            }
        }

        break;

    case PostPlayDestination::GRAVEYARD:
        if(source == PlaySource::HAND)
        {
            hand_remove(state, hand_index, selected_card, true, trigger_discard_on_graveyard);
        }
        else if(source != PlaySource::ECHO)
        {
            graveyard_push(state, card);

            if(trigger_discard_on_graveyard)
            {
                trigger_discard_effect_if_any(state, card.type);
            }
        }

        break;
    case PostPlayDestination::NONE:
    default:
        break;
    }
}

PlayResolutionResult resolve_played_card(GameState& state, CardRef card,
                                         const PlayResolutionContext& context)
{
    bind_bounty_copy(state, card);

    const bool swivel_follow = state.swivel_waiting;

    PlayResolutionResult result;
    result.dest = route_played_card(state, card.type, context.source, swivel_follow);
    result.increment_cards_played = true;

    if(should_apply_play_zone(result.dest, context.source, context.apply_destination))
    {
        apply_post_play_destination(state, card, context.source, result.dest, context.hand_index,
                                    context.selected_card, false);
    }

    apply_card_relocated_from_play(state, card.type, context.source);

    if(card.type == CardType::PAPER && context.source == PlaySource::HAND)
    {
        state.paper_swap_hand_index = context.hand_index;
    }

    apply_card_play(state, card, context.source);

    build_a_number_try_queue_digit_placement(state, card, context.source);
    poker_hand_try_queue_digit_placement(state, card, context.source);

    if(result.dest == PostPlayDestination::GRAVEYARD && context.source != PlaySource::ECHO)
    {
        if(card.type == CardType::BONES)
        {
            apply_bones_gy_entry(state);
        }
        else if(card.type == CardType::TOMBSTONES)
        {
            apply_tombstones_gy_entry(state);
        }
    }

    if(result.increment_cards_played)
    {
        ++state.cards_played_this_round;
        state.sharing_tick_mult_after_play();
    }

    maybe_draw_if_solo(state, card.type);

    return result;
}
