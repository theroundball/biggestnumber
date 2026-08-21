#include "play_resolution.h"

#include "card.h"
#include "card_data.h"
#include "card_type.h"
#include "game_events.h"
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

    void apply_tombstones_gy_entry(GameState& state)
    {
        const int factor = count_unique_graveyard_types(state);

        if(factor > 1)
        {
            state.mul_from_card(factor);
        }
    }
}

PostPlayDestination route_played_card(const GameState& state, CardType type, PlaySource source,
                                      bool swivel_follow)
{
    (void)state;
    const CardData& data = card_data(type);

    if(source == PlaySource::ECHO)
    {
        return PostPlayDestination::NONE;
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
    const bool swivel_follow = state.swivel_waiting;

    PlayResolutionResult result;
    result.dest = route_played_card(state, card.type, context.source, swivel_follow);
    result.increment_cards_played = context.source != PlaySource::ECHO;

    if(should_apply_play_zone(result.dest, context.source, context.apply_destination))
    {
        apply_post_play_destination(state, card, context.source, result.dest, context.hand_index,
                                    context.selected_card, false);
    }

    apply_card_play(state, card);

    if(result.dest == PostPlayDestination::GRAVEYARD && card.type == CardType::TOMBSTONES &&
       context.source != PlaySource::ECHO)
    {
        apply_tombstones_gy_entry(state);
    }

    if(result.increment_cards_played)
    {
        ++state.cards_played_this_round;

        if(card.type == CardType::BIRDS_OF_A_FEATHER)
        {
            ++state.birds_played_count;
        }
    }

    return result;
}
