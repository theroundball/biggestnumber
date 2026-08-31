#include "game_helpers.h"

#include "bn_core.h"
#include "bn_seed_random.h"
#include "bn_span.h"
#include "bn_string.h"

#include "card_data.h"
#include "card.h"
#include "card_instance.h"
#include "combo_system.h"
#include "game_events.h"
#include "game_ui.h"
#include "play_resolution.h"
#include "score_count_system.h"
#include "trinket_system.h"
#include "ui_common.h"

int build_a_number_card_digit(const GameState& state, CardRef card, PlaySource source);
bool build_a_number_can_play_card(const GameState& state, CardRef card, PlaySource source);

bool selection_sends_to_library(PendingActionType type)
{
    return type == PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP ||
           type == PendingActionType::GRAVEYARD_PICK_TO_TOP ||
           type == PendingActionType::GRAVEYARD_PICK_TO_BOTTOM ||
           type == PendingActionType::PUT_HAND_ON_DECK_TOP;
}

int initial_graveyard_cursor(const GameState& state, CardType exclude)
{
    (void)exclude;

    return state.graveyard.empty() ? 0 : state.graveyard.size() - 1;
}

int clamp_graveyard_cursor(int cursor, int graveyard_size)
{
    if(graveyard_size <= 0)
    {
        return 0;
    }

    if(cursor < 0)
    {
        return 0;
    }

    if(cursor >= graveyard_size)
    {
        return graveyard_size - 1;
    }

    return cursor;
}

int advance_graveyard_cursor(const GameState& state, int cursor, int direction, CardType exclude)
{
    (void)exclude;

    if(state.graveyard.empty())
    {
        return 0;
    }

    const int next_cursor = cursor + direction;

    if(next_cursor < 0)
    {
        return 0;
    }

    if(next_cursor >= state.graveyard.size())
    {
        return state.graveyard.size() - 1;
    }

    return next_cursor;
}

bool card_has_play_effect(CardType type)
{
    const CardData& data = card_data(type);

    if(data.immediate_plus != 0 || data.immediate_multiply != 0)
    {
        return true;
    }

    if(data.on_play != nullptr)
    {
        return true;
    }

    if(type == CardType::BONES)
    {
        return true;
    }

    for(int index = 0; index < 3; ++index)
    {
        const RoundModifier& mod = data.future[index];

        if(mod.positive != 0 || mod.multiply != 0 || mod.draw_at_start != 0 ||
           mod.opening_draw_count != 0)
        {
            return true;
        }
    }

    return false;
}

bool card_has_play_effect(const GameState& state, CardRef card)
{
    if(state.build_a_number_active && build_a_number_can_play_card(state, card, PlaySource::HAND))
    {
        return true;
    }

    if(state.poker_hand_active && poker_hand_card_digit(state, card, PlaySource::HAND) >= 1)
    {
        return true;
    }

    if(card_has_play_effect(card.type))
    {
        return true;
    }

    if(!card.has_instance())
    {
        return false;
    }

    const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

    if(!instance)
    {
        return false;
    }

    return instance->plus_digit != 0 || instance->increment_mult;
}

bool card_has_discard_effect(CardType type)
{
    return card_data(type).on_discard != nullptr;
}

bool card_has_cycle(CardType type)
{
    return card_data(type).has_cycle;
}

bool card_has_ghost(CardType type)
{
    return card_data(type).has_ghost;
}

bool card_numeric_play_override(CardType type)
{
    switch(type)
    {
    case CardType::MIRACLE:
    case CardType::JOURNAL:
    case CardType::TIME_IS_TOO_EXPENSIVE:
    case CardType::TIME_IS_MONEY:
    case CardType::DILLA:
    case CardType::SEMAPHORE:
    case CardType::THRESHOLD:
    case CardType::TOMBSTONES:
    case CardType::BIRDS_OF_A_FEATHER:
    case CardType::ROUNDUP:
    case CardType::CLOVER:
    case CardType::BIG_KUROSAWA_BURGER:
    case CardType::RAGS_TO_RICHES:
    case CardType::BONES:
        return true;
    default:
        return false;
    }
}

bool card_increases_current_number(CardType type)
{
    const CardData& data = card_data(type);

    if(data.immediate_plus > 0 || data.immediate_multiply > 1)
    {
        return true;
    }

    return card_numeric_play_override(type);
}

bool card_increases_current_number(const GameState& state, CardRef card)
{
    if(card_preview_plus(state, card, false) > 0)
    {
        return true;
    }

    const CardData& data = card_data(card.type);

    if(data.immediate_multiply > 1)
    {
        return true;
    }

    return card_numeric_play_override(card.type);
}

bool card_can_add_or_multiply(CardType type)
{
    if(card_increases_current_number(type))
    {
        return true;
    }

    const CardData& data = card_data(type);

    for(int index = 0; index < 3; ++index)
    {
        if(data.future[index].positive > 0 || data.future[index].multiply > 1)
        {
            return true;
        }
    }

    return false;
}

bool card_can_add_or_multiply(const GameState& state, CardRef card)
{
    if(card_increases_current_number(state, card))
    {
        return true;
    }

    return card_can_add_or_multiply(card.type);
}

namespace
{
    bool score_contains_digit_below(int value, int below)
    {
        if(below <= 0)
        {
            return false;
        }

        int digits = value < 0 ? -value : value;

        if(digits == 0)
        {
            return 0 < below;
        }

        while(digits > 0)
        {
            if(digits % 10 < below)
            {
                return true;
            }

            digits /= 10;
        }

        return false;
    }

    bool roundup_would_increase(const GameState& state)
    {
        const int count = state.roundup_play_count + 1;
        int divisor = 10;

        if(count == 2)
        {
            divisor = 100;
        }
        else if(count >= 3)
        {
            divisor = 1000;
        }

        const int total_after = ((state.total_score + divisor - 1) / divisor) * divisor;

        if(total_after > state.total_score)
        {
            return true;
        }

        const int round_after = ((state.round.running + divisor - 1) / divisor) * divisor;
        return round_after > state.round.running;
    }

    bool waterfall_immediate_round_increase(const GameState& state, CardRef card)
    {
        if(card_preview_plus(state, card, false) > 0)
        {
            return true;
        }

        int multiply = card_data(card.type).immediate_multiply;

        if(card.has_instance())
        {
            if(const CardInstance* instance = instance_at(state.instance_pool, card.instance_id))
            {
                multiply = effective_immediate_multiply(*instance);
            }
        }

        return multiply > 1 && state.round.running > 0;
    }
}

bool mill_reveal_card_hits(const GameState& state, CardRef card, bool waterfall)
{
    return waterfall ? waterfall_would_make_bigger(state, card)
                     : card_can_add_or_multiply(state, card);
}

bool waterfall_would_make_bigger(const GameState& state, CardRef card)
{
    switch(card.type)
    {
    case CardType::WISHES:
    case CardType::SIPS:
    case CardType::SNAIL_MAIL:
    case CardType::SWAP:
    case CardType::HACKER:
    case CardType::LIBRARIAN:
    case CardType::PILOT:
    case CardType::NECROMANCY:
    case CardType::JACKS:
    case CardType::FISHING_POLE:
    case CardType::SHELLS:
    case CardType::ROLL_OVER:
    case CardType::FLEX:
    case CardType::SPECULATIVE:
    case CardType::DEAD_RISING:
    case CardType::SEVEN_FEET_DEEP:
    case CardType::SWIVEL:
    case CardType::TURTLE_MODE:
    case CardType::RAGS_TO_RICHES:
        return false;

    case CardType::LIFELINE:
        return count_lifeline_pickable_graveyard(state) > 0;

    case CardType::MIRACLE:
        return true;

    case CardType::THE_FOURTH:
        return true;

    case CardType::THE_FIFTH:
        return score_contains_digit_below(state.total_score, 5);

    case CardType::PALINDROME:
    case CardType::MINOR_FALL:
    case CardType::MAJOR_LIFT:
        return true;

    case CardType::ROUNDUP:
        return roundup_would_increase(state);

    case CardType::CLOVER:
        return state.graveyard.size() >= 3;

    case CardType::BIG_KUROSAWA_BURGER:
        return state.hand.size() > 1;

    case CardType::THRESHOLD:
    case CardType::TOMBSTONES:
    case CardType::BIRDS_OF_A_FEATHER:
    case CardType::TIME_IS_TOO_EXPENSIVE:
    case CardType::TIME_IS_MONEY:
        return true;

    case CardType::BONES:
        return true;

    case CardType::JOURNAL:
        return state.cards_played_this_round > 0 && state.round.running > 0;

    case CardType::SEMAPHORE:
        if(state.current_round == 1 && state.cards_played_this_round == 0)
        {
            return true;
        }

        if(state.hand.size() == 1 && state.deck.empty())
        {
            return state.graveyard.size() > 1;
        }

        return true;

    case CardType::TRIPTYCH:
        return true;

    default:
        break;
    }

    return waterfall_immediate_round_increase(state, card);
}

int card_preview_plus(const GameState& state, CardRef card, bool ghost)
{
    if(!ghost && card.type == CardType::BOUNTY)
    {
        int plus = bounty_play_plus_for_card(state, card);

        if(state.pending_double_adds && plus > 0)
        {
            plus *= 2;
        }

        return plus;
    }

    const CardData& data = card_data(card.type);
    int plus = (ghost && data.has_ghost) ? data.ghost_plus : data.immediate_plus;

    if(!ghost)
    {
        if(const CardInstance* instance = instance_at(state.instance_pool, card.instance_id))
        {
            plus = effective_immediate_plus(*instance);
        }
    }

    if(state.pending_double_adds && plus > 0)
    {
        plus *= 2;
    }

    return plus;
}

int ghost_count(const GameState& state)
{
    int count = 0;

    for(int index = 0; index < state.graveyard.size(); ++index)
    {
        if(card_has_ghost(state.graveyard[index].type))
        {
            ++count;
        }
    }

    return count;
}

bool has_optional_ghost_plays(const GameState& state)
{
    return ghost_count(state) > 0 || combo_ready_count(state) > 0;
}

bool empty_hand_triggers_round_end(const GameState& state)
{
    if(state.roll_over_substitution_active)
    {
        return false;
    }

    if(state.hand.empty() && has_optional_ghost_plays(state) && !state.waive_optional_ghost_plays)
    {
        return false;
    }

    return state.hand.empty();
}

namespace
{
    constexpr int bounty_return_threshold_base = 10;
    constexpr int bounty_escalation_cap = 100000000;

    int count_bones_in_graveyard(const GameState& state);
    int tombstones_multiplier_preview(const GameState& state);

    int played_card_digit_plus(const GameState& state, CardRef card, PlaySource source)
    {
        const CardData& data = card_data(card.type);
        int plus = (source == PlaySource::GHOST && data.has_ghost) ? data.ghost_plus
                                                                           : data.immediate_plus;

        if(source != PlaySource::GHOST && card.has_instance())
        {
            const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

            if(instance)
            {
                plus = effective_immediate_plus(*instance);
            }
        }

        return plus;
    }

    int played_card_digit_multiply(const GameState& state, CardRef card, PlaySource source)
    {
        if(source == PlaySource::GHOST)
        {
            return 0;
        }

        const CardData& data = card_data(card.type);
        int multiply = data.immediate_multiply;

        if(card.has_instance())
        {
            const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

            if(instance)
            {
                multiply = effective_immediate_multiply(*instance);
            }
        }

        return multiply > 1 ? multiply : 0;
    }

    int digit_from_build_value(int value)
    {
        if(value <= 0)
        {
            return 0;
        }

        if(value <= 9)
        {
            return value;
        }

        int highest = 0;
        int remaining = value;

        while(remaining > 0)
        {
            const int digit = remaining % 10;

            if(digit > highest)
            {
                highest = digit;
            }

            remaining /= 10;
        }

        return highest;
    }

    int build_a_number_plus_value(const GameState& state, CardRef card, PlaySource source)
    {
        switch(card.type)
        {
        case CardType::TOMBSTONES:
            return state.graveyard.size() + 1;

        case CardType::THRESHOLD:
            return 3;

        case CardType::SEMAPHORE:
            if(state.current_round == 1 && state.cards_played_this_round == 0)
            {
                return 100;
            }

            return 3;

        case CardType::TIME_IS_TOO_EXPENSIVE:
            return state.current_round * 2;

        case CardType::TRIPTYCH:
            return 3;

        case CardType::BIRDS_OF_A_FEATHER:
            return 5;

        case CardType::DILLA:
            return 8;

        case CardType::MIRACLE:
            return 10;

        case CardType::CATNIP:
            return 1;

        case CardType::CYCLE:
            return 2;

        case CardType::CYCLE_SEVEN:
            return 7;

        case CardType::COMEBACK:
            return 3;

        case CardType::GET_ME_OUTA_HERE:
            return 9;

        case CardType::BONES:
            return count_bones_in_graveyard(state) + 1;

        default:
            return played_card_digit_plus(state, card, source);
        }
    }

    int build_a_number_multiply_value(const GameState& state, CardRef card, PlaySource source)
    {
        if(source == PlaySource::GHOST)
        {
            return 0;
        }

        switch(card.type)
        {
        case CardType::JOURNAL:
            return state.cards_played_this_round + 1;

        case CardType::CLOVER:
            return 3;

        case CardType::BIG_KUROSAWA_BURGER:
            return 4;

        case CardType::TIME_IS_MONEY:
            return state.current_round * 2;

        case CardType::BONES:
            return 0;

        case CardType::TOMBSTONES:
            return 0;

        case CardType::THRESHOLD:
            return 3;

        case CardType::SEMAPHORE:
            return 0;

        case CardType::TRIPTYCH:
            return 3;

        default:
            return played_card_digit_multiply(state, card, source);
        }
    }

    int build_a_number_play_value(const GameState& state, CardRef card, PlaySource source)
    {
        const int plus = build_a_number_plus_value(state, card, source);

        if(plus > 0)
        {
            return plus;
        }

        return build_a_number_multiply_value(state, card, source);
    }
}

int build_a_number_card_digit(const GameState& state, CardRef card, PlaySource source)
{
    return digit_from_build_value(build_a_number_play_value(state, card, source));
}

bool build_a_number_can_play_card(const GameState& state, CardRef card, PlaySource source)
{
    return state.build_a_number_active && card.type != CardType::BUILD_A_NUMBER &&
           build_a_number_card_digit(state, card, source) >= 1;
}

bool slot_mode_card_playable(const GameState& state, CardRef card, PlaySource source)
{
    if(state.build_a_number_active)
    {
        if(build_a_number_can_play_card(state, card, source))
        {
            return true;
        }
    }
    else if(state.poker_hand_active)
    {
        if(poker_hand_card_digit(state, card, source) >= 1)
        {
            return true;
        }
    }
    else
    {
        return true;
    }

    if(card_has_play_effect(card.type))
    {
        return true;
    }

    if(source != PlaySource::GHOST && card.has_instance())
    {
        const CardInstance* instance = instance_at(state.instance_pool, card.instance_id);

        if(instance && (instance->plus_digit != 0 || instance->increment_mult))
        {
            return true;
        }
    }

    return false;
}

void build_a_number_try_queue_digit_placement(GameState& state, CardRef card, PlaySource source)
{
    if(!state.build_a_number_active || card.type == CardType::BUILD_A_NUMBER)
    {
        return;
    }

    const int digit = build_a_number_card_digit(state, card, source);

    if(digit < 1)
    {
        return;
    }

    PendingAction action;
    action.type = PendingActionType::BUILD_A_NUMBER_PLACE_DIGIT;
    action.count = digit;
    state.pending_actions.push_back(action);
}

void poker_hand_try_queue_digit_placement(GameState& state, CardRef card, PlaySource source)
{
    if(!state.poker_hand_active)
    {
        return;
    }

    const int digit = poker_hand_card_digit(state, card, source);

    if(digit < 1)
    {
        return;
    }

    PendingAction action;
    action.type = PendingActionType::POKER_HAND_PLACE_DIGIT;
    action.count = digit;
    state.pending_actions.push_back(action);
}

int poker_hand_card_digit(const GameState& state, CardRef card, PlaySource source)
{
    return build_a_number_card_digit(state, card, source);
}

namespace
{
    bool bounty_copy_bound(const CardRef& card)
    {
        return card.type == CardType::BOUNTY && card.bounty_id != NO_INSTANCE &&
               card.bounty_id < InstancePool::CAPACITY;
    }
}

void bind_bounty_copy(GameState& state, CardRef& card)
{
    if(card.type != CardType::BOUNTY || bounty_copy_bound(card))
    {
        return;
    }

    if(state.bounty_next_id >= InstancePool::CAPACITY)
    {
        return;
    }

    card.bounty_id = state.bounty_next_id;
    ++state.bounty_next_id;
}

int bounty_instance_play_count(const GameState& state, CardRef card)
{
    if(bounty_copy_bound(card))
    {
        return state.bounty_instance_plays[card.bounty_id];
    }

    return 0;
}

int bounty_play_plus(const GameState& state)
{
    return bounty_play_plus_for_card(state, state.play_effect_card);
}

int bounty_play_plus_for_card(const GameState& state, CardRef card)
{
    return bounty_instance_play_count(state, card) + 1;
}

int bounty_increment_play(GameState& state, CardRef card)
{
    if(!bounty_copy_bound(card))
    {
        return 1;
    }

    uint8_t& plays = state.bounty_instance_plays[card.bounty_id];

    if(plays < 255)
    {
        ++plays;
    }

    return plays;
}

namespace
{
    void push_stat_segment(bn::vector<CardFaceStatSegment, 4>& out, const bn::string_view& text,
                           CardStatColor color)
    {
        if(text.empty() || out.full())
        {
            return;
        }

        CardFaceStatSegment& segment = out.emplace_back();
        segment.text = text;
        segment.color = color;
    }

    void push_plus_stat(bn::vector<CardFaceStatSegment, 4>& out, int value)
    {
        if(value == 0)
        {
            return;
        }

        bn::string<8> text;
        text.append(value > 0 ? "+" : "-");
        text.append(bn::to_string<6>(value > 0 ? value : -value));
        push_stat_segment(out, text, CardStatColor::GREEN);
    }

    void push_mult_stat(bn::vector<CardFaceStatSegment, 4>& out, int value)
    {
        if(value <= 1)
        {
            return;
        }

        bn::string<8> text = "x";
        text.append(bn::to_string<6>(value));
        push_stat_segment(out, text, CardStatColor::GOLD);
    }

    int count_bones_in_graveyard(const GameState& state)
    {
        int count = 0;

        for(const CardRef& card : state.graveyard)
        {
            if(card.type == CardType::BONES)
            {
                ++count;
            }
        }

        return count;
    }

    bool graveyard_has_type(const GameState& state, CardType type)
    {
        for(const CardRef& card : state.graveyard)
        {
            if(card.type == type)
            {
                return true;
            }
        }

        return false;
    }

    int tombstones_multiplier_preview(const GameState& state)
    {
        int unique = count_unique_graveyard_types(state);

        if(!graveyard_has_type(state, CardType::TOMBSTONES))
        {
            ++unique;
        }

        return unique;
    }

    void append_static_face_stat(const CardData& data, const CardInstance* instance,
                                 bn::vector<CardFaceStatSegment, 4>& out)
    {
        int plus = data.immediate_plus;
        int multiply = data.immediate_multiply;

        if(instance)
        {
            plus = effective_immediate_plus(*instance);
            multiply = effective_immediate_multiply(*instance);
        }

        push_plus_stat(out, plus);
        push_mult_stat(out, multiply);

        if(data.has_cycle)
        {
            push_stat_segment(out, "Cyc", CardStatColor::DEFAULT);
        }

        if(data.has_ghost)
        {
            bn::string<8> ghost = "Gh";

            if(data.ghost_plus != 0)
            {
                ghost.append(data.ghost_plus > 0 ? "+" : "-");
                ghost.append(bn::to_string<4>(data.ghost_plus > 0 ? data.ghost_plus : -data.ghost_plus));
            }

            push_stat_segment(out, ghost, CardStatColor::DEFAULT);
        }

        if(out.empty() && data.on_play != nullptr && data.immediate_plus == 0 && data.immediate_multiply <= 1)
        {
            push_stat_segment(out, "FX", CardStatColor::DEFAULT);
        }
    }
}

void format_card_face_stat(const GameState* state, CardRef ref, const CardInstance* instance,
                           bool in_graveyard, bn::vector<CardFaceStatSegment, 4>& out)
{
    out.clear();

    const CardData& data = card_data(ref.type);

    if(!state)
    {
        append_static_face_stat(data, instance, out);
        return;
    }

    if(state->build_a_number_active)
    {
        const int digit = build_a_number_card_digit(*state, ref, PlaySource::HAND);

        if(digit >= 1)
        {
            push_stat_segment(out, bn::to_string<1>(digit), CardStatColor::DEFAULT);
            return;
        }
    }

    if(state->poker_hand_active)
    {
        const int digit = poker_hand_card_digit(*state, ref, PlaySource::HAND);

        if(digit >= 1)
        {
            push_stat_segment(out, bn::to_string<1>(digit), CardStatColor::DEFAULT);
            return;
        }
    }

    switch(ref.type)
    {
    case CardType::BOUNTY:
        if(in_graveyard)
        {
            bn::string<8> text = bn::to_string<4>(bounty_return_progress_for(*state, ref));
            text.append('/');
            text.append(bn::to_string<4>(bounty_return_threshold_for(*state, ref)));
            push_stat_segment(out, text, CardStatColor::DEFAULT);
        }
        else
        {
            push_stat_segment(out, bn::to_string<4>(bounty_play_plus_for_card(*state, ref)),
                              CardStatColor::GREEN);
        }
        break;

    case CardType::JOURNAL:
        push_mult_stat(out, state->cards_played_this_round + 1);
        break;

    case CardType::TOMBSTONES:
        push_plus_stat(out, state->graveyard.size() + (in_graveyard ? 0 : 1));
        break;

    case CardType::BONES:
        {
            const int bones_after = count_bones_in_graveyard(*state) + (in_graveyard ? 0 : 1);
            push_mult_stat(out, bones_after + 1);
        }
        break;

    case CardType::THRESHOLD:
        if(in_graveyard && state->graveyard.size() > 7)
        {
            push_mult_stat(out, 3);
        }
        else
        {
            push_plus_stat(out, 3);
        }
        break;

    case CardType::SEMAPHORE:
        if(state->current_round == 1 && state->cards_played_this_round == 0)
        {
            push_plus_stat(out, 100);
        }
        else if(state->hand.size() == 1 && state->deck.empty())
        {
            push_mult_stat(out, state->graveyard.size());
        }
        else
        {
            push_plus_stat(out, 3);
        }
        break;

    case CardType::TIME_IS_TOO_EXPENSIVE:
        push_plus_stat(out, state->current_round * 2);
        break;

    case CardType::TIME_IS_MONEY:
        push_mult_stat(out, state->current_round * 2);
        break;

    case CardType::TRIPTYCH:
        push_plus_stat(out, 3);

        if(state->round.committed() % 3 == 0)
        {
            push_mult_stat(out, 3);
        }
        break;

    case CardType::BIRDS_OF_A_FEATHER:
        push_plus_stat(out, 5);
        break;

    case CardType::DILLA:
        push_plus_stat(out, 8);
        break;

    case CardType::MIRACLE:
        push_plus_stat(out, 10);
        break;

    case CardType::CATNIP:
        push_plus_stat(out, 1);
        break;

    case CardType::CYCLE:
        push_plus_stat(out, 2);
        break;

    case CardType::CYCLE_SEVEN:
        push_plus_stat(out, 7);
        break;

    case CardType::COMEBACK:
        push_plus_stat(out, 3);
        break;

    case CardType::GET_ME_OUTA_HERE:
        push_plus_stat(out, 9);
        break;

    default:
        append_static_face_stat(data, instance, out);
        break;
    }
}

void sync_bounty_card_overlay(const GameState& state, Card& card, CardRef ref, bool in_graveyard,
                              bn::sprite_text_generator* generator)
{
    if(ref.type != CardType::BOUNTY || generator == nullptr)
    {
        return;
    }

    bn::string<8> text;

    if(in_graveyard)
    {
        text = bn::to_string<4>(bounty_return_progress_for(state, ref));
        text.append('/');
        text.append(bn::to_string<4>(bounty_return_threshold_for(state, ref)));
    }
    else
    {
        text = bn::to_string<4>(bounty_play_plus_for_card(state, ref));
    }

    card.set_amount_overlay(generator, text);
}

namespace
{
    int parse_score_digits(const bn::string_view& digits)
    {
        int value = 0;

        for(int index = 0; index < digits.size(); ++index)
        {
            value = value * 10 + (digits[index] - '0');
        }

        return value;
    }

    bool compute_shifted_digit_total(const bn::string<12>& digits, int source_index, int dest_index, int& out_total)
    {
        if(source_index < 0 || dest_index < 0 || source_index >= digits.size() || dest_index >= digits.size() ||
           source_index == dest_index)
        {
            return false;
        }

        bn::string<12> shifted = digits;
        const char moved = shifted[source_index];

        if(source_index < dest_index)
        {
            for(int index = source_index; index < dest_index; ++index)
            {
                shifted[index] = shifted[index + 1];
            }
        }
        else
        {
            for(int index = source_index; index > dest_index; --index)
            {
                shifted[index] = shifted[index - 1];
            }
        }

        shifted[dest_index] = moved;
        out_total = parse_score_digits(shifted);
        return true;
    }
}

bool plan_minor_fall(int score, DigitMovePlan& plan)
{
    const bn::string<12> digits = bn::to_string<12>(score);

    if(digits.empty())
    {
        return false;
    }

    int smallest = 10;
    int source_index = -1;

    for(int index = digits.size() - 1; index >= 0; --index)
    {
        const int digit = digits[index] - '0';

        if(digit <= smallest)
        {
            smallest = digit;
            source_index = index;
        }
    }

    const int dest_index = digits.size() - 1;

    if(source_index < 0 || source_index == dest_index)
    {
        return false;
    }

    plan.source_digit_index = source_index;
    plan.dest_digit_index = dest_index;

    return compute_shifted_digit_total(digits, source_index, dest_index, plan.new_total);
}

bool plan_major_lift(int score, DigitMovePlan& plan)
{
    const bn::string<12> digits = bn::to_string<12>(score);

    if(digits.empty())
    {
        return false;
    }

    int largest = -1;
    int source_index = -1;

    for(int index = 0; index < digits.size(); ++index)
    {
        const int digit = digits[index] - '0';

        if(digit >= largest)
        {
            largest = digit;
            source_index = index;
        }
    }

    if(source_index <= 0)
    {
        return false;
    }

    plan.source_digit_index = source_index;
    plan.dest_digit_index = 0;

    return compute_shifted_digit_total(digits, source_index, 0, plan.new_total);
}

bool plan_palindrome_wrap(int score, bool applying_double_adds, int& out_new_score)
{
    const bn::string<12> digits = bn::to_string<12>(score);

    if(digits.size() > 9)
    {
        return false;
    }

    for(int left = 0, right = digits.size() - 1; left < right; ++left, --right)
    {
        if(digits[left] != digits[right])
        {
            return false;
        }
    }

    long long outer_place = 10;

    for(int index = 0; index < digits.size(); ++index)
    {
        outer_place *= 10;
    }

    const int wrapper = applying_double_adds ? 2 : 1;
    const long long after = static_cast<long long>(wrapper) * outer_place +
                            static_cast<long long>(score) * 10 + wrapper;

    if(after > 2147483647 || after <= score)
    {
        return false;
    }

    out_new_score = int(after);
    return true;
}

bool try_palindrome_play(GameState& state)
{
    int new_score = 0;

    if(plan_palindrome_wrap(state.total_score, state.applying_double_adds, new_score))
    {
        const int before = state.total_score;
        state.total_score = new_score;
        score_count_queue(state, TrinketScoreField::TOTAL, before, new_score);
        trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, new_score);
        return true;
    }

    if(plan_palindrome_wrap(state.round.running, state.applying_double_adds, new_score))
    {
        const int before = state.round.running;
        state.round.running = new_score;
        score_count_queue(state, TrinketScoreField::ROUND, before, new_score);
        trinket_queue_score_check(state, TrinketScoreField::ROUND, before, new_score);
        return true;
    }

    return false;
}

bool try_minor_fall(GameState& state)
{
    DigitMovePlan plan;

    if(!plan_minor_fall(state.total_score, plan))
    {
        return false;
    }

    const int before = state.total_score;
    state.total_score = plan.new_total;
    score_count_queue(state, TrinketScoreField::TOTAL, before, plan.new_total);
    trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, plan.new_total);
    return true;
}

bool try_major_lift(GameState& state)
{
    DigitMovePlan plan;

    if(!plan_major_lift(state.total_score, plan))
    {
        return false;
    }

    const int before = state.total_score;
    state.total_score = plan.new_total;
    score_count_queue(state, TrinketScoreField::TOTAL, before, plan.new_total);
    trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, plan.new_total);
    return true;
}

int bounty_return_threshold_for(const GameState& state, CardRef card)
{
    if(bounty_copy_bound(card))
    {
        const int threshold = state.bounty_instance_return_threshold[card.bounty_id];

        return threshold > 0 ? threshold : bounty_return_threshold_base;
    }

    return bounty_return_threshold_base;
}

int bounty_return_progress_for(const GameState& state, CardRef card)
{
    int anchor = 0;

    if(bounty_copy_bound(card))
    {
        anchor = state.bounty_instance_return_anchor[card.bounty_id];
    }

    int progress = state.round.running - anchor;

    if(progress < 0)
    {
        progress = 0;
    }

    return progress;
}

void bounty_on_enter_graveyard(GameState& state, CardRef card)
{
    if(card.type != CardType::BOUNTY)
    {
        return;
    }

    if(bounty_copy_bound(card))
    {
        state.bounty_instance_return_anchor[card.bounty_id] = state.round.running;
    }
}

void bounty_on_round_start(GameState& state)
{
    for(const CardRef& card : state.graveyard)
    {
        if(card.type == CardType::BOUNTY)
        {
            bounty_on_enter_graveyard(state, card);
        }
    }
}

void bounty_update_return_threshold_for_plays(GameState& state, CardRef card)
{
    if(!bounty_copy_bound(card))
    {
        return;
    }

    const int plays = bounty_instance_play_count(state, card);
    int& threshold = state.bounty_instance_return_threshold[card.bounty_id];

    if(threshold <= 0)
    {
        threshold = bounty_return_threshold_base;
    }

    while(plays >= threshold && threshold <= bounty_escalation_cap / 10)
    {
        threshold *= 10;
    }
}

void check_bounty_return(GameState& state)
{
    for(int index = state.graveyard.size() - 1; index >= 0; --index)
    {
        const CardRef card = state.graveyard[index];

        if(card.type != CardType::BOUNTY)
        {
            continue;
        }

        if(bounty_return_progress_for(state, card) < bounty_return_threshold_for(state, card))
        {
            continue;
        }

        graveyard_remove_at(state, index);

        if(!state.hand.full())
        {
            hand_add_card(state, card);
        }
        else
        {
            state.deck.insert_top(card);
        }
    }
}

bool roll_over_pick_active(const GameState& state)
{
    return state.roll_over_pick_active && state.roll_over_choice_count > 0;
}

void roll_over_restore_stashed_hand(GameState& state)
{
    for(const CardRef& card : state.roll_over_stashed_hand)
    {
        if(!state.hand.full())
        {
            state.hand.push_back(card);
        }
    }

    state.roll_over_stashed_hand.clear();
}

void roll_over_finish_sequence(GameState& state, int* selected_card)
{
    state.roll_over_substitution_active = false;
    state.roll_over_pick_active = false;
    state.roll_over_awaiting_follow_up = false;
    state.roll_over_choice_count = 0;
    state.roll_over_choices = {};
    state.roll_over_follow_up_card = CardRef{};
    state.roll_over_stashed_hand.clear();

    if(selected_card)
    {
        if(state.hand.empty())
        {
            *selected_card = 0;
        }
        else if(*selected_card < 0 || *selected_card >= state.hand.size())
        {
            *selected_card = state.hand.size() - 1;
        }
    }
}

void try_finish_roll_over_substitution(GameState& state, int* selected_card)
{
    if(!state.roll_over_substitution_active || state.roll_over_pick_active)
    {
        return;
    }

    if(state.roll_over_follow_up_card.type != CardType::COUNT || state.roll_over_awaiting_follow_up)
    {
        return;
    }

    roll_over_finish_sequence(state, selected_card);
}

bool begin_roll_over_substitution(GameState& state, int* selected_card)
{
    if(state.roll_over_substitution_active)
    {
        return false;
    }

    bn::vector<int, 50> candidates;

    for(int index = 0; index < state.graveyard.size(); ++index)
    {
        if(state.graveyard[index].type != CardType::ROLL_OVER)
        {
            candidates.push_back(index);
        }
    }

    if(candidates.size() < 2)
    {
        return false;
    }

    const int first_pick = state.rng.get_int(candidates.size());
    const int first_index = candidates[first_pick];
    candidates.erase(candidates.begin() + first_pick);

    const int second_pick = state.rng.get_int(candidates.size());
    const int second_index = candidates[second_pick];

    const CardRef first = state.graveyard[first_index];
    const CardRef second = state.graveyard[second_index];

    const int remove_high = first_index > second_index ? first_index : second_index;
    const int remove_low = first_index > second_index ? second_index : first_index;
    graveyard_remove_at(state, remove_high);
    graveyard_remove_at(state, remove_low);

    state.roll_over_stashed_hand.clear();

    for(const CardRef& card : state.hand)
    {
        if(!state.roll_over_stashed_hand.full())
        {
            state.roll_over_stashed_hand.push_back(card);
        }
    }

    state.hand.clear();
    state.roll_over_choices[0] = first;
    state.roll_over_choices[1] = second;
    state.roll_over_choice_count = 2;
    state.roll_over_pick_active = true;
    state.roll_over_awaiting_follow_up = false;
    state.roll_over_follow_up_card = CardRef{};
    state.roll_over_substitution_active = true;

    if(selected_card)
    {
        *selected_card = 0;
    }

    return true;
}

namespace
{
    int visible_ghost_count(const GameState& state)
    {
        if(state.waive_optional_ghost_plays)
        {
            return 0;
        }

        return ghost_count(state);
    }

    int visible_combo_ready_count(const GameState& state)
    {
        if(state.waive_optional_ghost_plays)
        {
            return 0;
        }

        return combo_ready_count(state);
    }
}

int playable_slot_count(const GameState& state)
{
    if(roll_over_pick_active(state))
    {
        return state.roll_over_choice_count;
    }

    return state.hand.size() + longsleeve_count(state) + visible_ghost_count(state) +
           visible_combo_ready_count(state);
}

int longsleeve_count(const GameState& state)
{
    if(!state.has_longsleeves())
    {
        return 0;
    }

    return state.longsleeve_cards.size();
}

bool playable_slot_is_longsleeve(const GameState& state, int visual_index)
{
    if(visual_index < state.hand.size())
    {
        return false;
    }

    const int longsleeve_index = visual_index - state.hand.size();

    return longsleeve_index >= 0 && longsleeve_index < longsleeve_count(state);
}

int playable_slot_longsleeve_index(const GameState& state, int visual_index)
{
    if(!playable_slot_is_longsleeve(state, visual_index))
    {
        return -1;
    }

    return visual_index - state.hand.size();
}

bool playable_slot_is_ghost(const GameState& state, int visual_index)
{
    if(visual_index < state.hand.size() || playable_slot_is_longsleeve(state, visual_index))
    {
        return false;
    }

    const int ghost_index = visual_index - state.hand.size() - longsleeve_count(state);

    return ghost_index >= 0 && ghost_index < visible_ghost_count(state);
}

bool playable_slot_is_combine_offer(const GameState& state, int visual_index)
{
    if(visual_index < state.hand.size() || playable_slot_is_longsleeve(state, visual_index))
    {
        return false;
    }

    const int ghost_index = visual_index - state.hand.size() - longsleeve_count(state);
    const int ghost_offer_count = visible_ghost_count(state);

    return ghost_index >= ghost_offer_count &&
           ghost_index < ghost_offer_count + visible_combo_ready_count(state);
}

int playable_slot_combine_ordinal(const GameState& state, int visual_index)
{
    return visual_index - state.hand.size() - longsleeve_count(state) - visible_ghost_count(state);
}

int playable_slot_hand_index(const GameState& state, int visual_index)
{
    if(visual_index < 0 || visual_index >= state.hand.size())
    {
        return -1;
    }

    return visual_index;
}

int playable_slot_graveyard_index(const GameState& state, int visual_index)
{
    const int ghost_ordinal = visual_index - state.hand.size() - longsleeve_count(state);

    if(ghost_ordinal < 0)
    {
        return -1;
    }

    int seen = 0;

    for(int index = 0; index < state.graveyard.size(); ++index)
    {
        if(!card_has_ghost(state.graveyard[index].type))
        {
            continue;
        }

        if(seen == ghost_ordinal)
        {
            return index;
        }

        ++seen;
    }

    return -1;
}

CardRef playable_slot_card(const GameState& state, int visual_index)
{
    if(visual_index < 0)
    {
        return CardRef{};
    }

    if(roll_over_pick_active(state))
    {
        if(visual_index < state.roll_over_choice_count)
        {
            return state.roll_over_choices[visual_index];
        }

        return CardRef{};
    }

    if(visual_index < state.hand.size())
    {
        return state.hand[visual_index];
    }

    if(playable_slot_is_longsleeve(state, visual_index))
    {
        const int longsleeve_index = playable_slot_longsleeve_index(state, visual_index);

        if(longsleeve_index >= 0 && longsleeve_index < state.longsleeve_cards.size())
        {
            return state.longsleeve_cards[longsleeve_index];
        }

        return CardRef{};
    }

    if(playable_slot_is_combine_offer(state, visual_index))
    {
        const int ordinal = playable_slot_combine_ordinal(state, visual_index);
        const uint8_t combo_id = combo_ready_id_by_ordinal(state, ordinal);
        return CardRef{combo_ready_display_type(combo_id), NO_INSTANCE};
    }

    const int gy_index = playable_slot_graveyard_index(state, visual_index);

    if(gy_index < 0)
    {
        return CardRef{};
    }

    return state.graveyard[gy_index];
}

bool try_draw_one_to_hand(GameState& state)
{
    if(state.deck.remaining() == 0)
    {
        return false;
    }

    queue_effect_draw(state, 1, true);
    return true;
}

void queue_effect_draw(GameState& state, int count, bool miracle_on_first)
{
    if(count <= 0)
    {
        return;
    }

    PendingAction action;
    action.type = PendingActionType::EFFECT_DECK_DRAW;
    action.count = count;
    action.hand_index = miracle_on_first ? 1 : 0;
    state.pending_actions.push_back(action);
}

void maybe_draw_if_solo(GameState& state, CardType type)
{
    if(state.build_a_number_active || type != CardType::SOLO || !state.hand.empty())
    {
        return;
    }

    try_draw_one_to_hand(state);
}

void play_miracle_bonus(GameState& state, int amount)
{
    state.add_from_card(amount);
    ++state.cards_played_this_round;
    state.sharing_tick_mult_after_play();
}

void remove_selected_card(GameState &state, int &selected_card)
{
    hand_remove_at_to_graveyard(state, selected_card, selected_card);
}

void finish_played_card_from_hand(int hand_index, int& selected_card, GameState& state)
{
    if(hand_index < 0 || hand_index >= state.hand.size())
    {
        return;
    }

    hand_remove_at_to_deck_top(state, hand_index, selected_card);
}

RemovalStyle removal_style_for_hand_play(CardType type)
{
    if(card_data(type).exiles_self_on_play)
    {
        return RemovalStyle::EXILE_DISSIPATE;
    }

    return RemovalStyle::TO_GRAVEYARD;
}

void discard_card(GameState &state, int &selected_card)
{
    if(state.hand.empty() || selected_card >= state.hand.size())
    {
        return;
    }

    hand_remove_at_to_graveyard(state, selected_card, selected_card);
}

void swap_cards(GameState &state, int first_card, int second_card)
{
    hand_swap_cards(state, first_card, second_card);
}

CardRef scry_play_selected_type(GameState& state)
{
    const PreparedDeckPlay prepared = scry_prepare_play(state);

    if(prepared.miracle_from_top)
    {
        play_miracle_bonus(state, 10);
    }
    else
    {
        PlayResolutionContext context;
        context.source = PlaySource::SCRY;
        context.apply_destination = !state.swivel_waiting;
        resolve_played_card(state, prepared.card, context);
    }

    return prepared.card;
}

PreparedDeckPlay scry_prepare_play(GameState& state)
{
    PreparedDeckPlay result;
    const int cursor = state.selection.cursor;
    result.card = state.selection.scry_buffer[cursor];
    const bool played_from_deck_top = cursor + 1 == state.selection.scry_buffer.size();
    state.selection.scry_buffer.erase(state.selection.scry_buffer.begin() + cursor);

    for(int index = 0; index < state.selection.scry_buffer.size(); ++index)
    {
        state.deck.insert_top(state.selection.scry_buffer[index]);
    }

    state.selection.scry_buffer.clear();
    result.miracle_from_top = result.card.type == CardType::MIRACLE && played_from_deck_top;

    if(result.miracle_from_top)
    {
        state.first_deck_draw_this_round = false;
    }

    return result;
}

void scry_play_selected(GameState& state)
{
    scry_play_selected_type(state);
}

PreparedDeckPlay deck_search_prepare_play(GameState& state)
{
    PreparedDeckPlay result;
    const int cursor = state.selection.cursor;
    state.deck.move_undrawn_to_top(cursor);

    if(!state.deck.draw(result.card))
    {
        result.card = CardRef{CardType::COUNT, NO_INSTANCE};
        state.selection.deck_search_buffer.clear();
        return result;
    }

    result.miracle_from_top = cursor == 0 && result.card.type == CardType::MIRACLE;
    state.selection.deck_search_buffer.clear();
    return result;
}

CardRef deck_search_play_selected_type(GameState& state)
{
    const PreparedDeckPlay prepared = deck_search_prepare_play(state);

    if(prepared.card.type == CardType::COUNT)
    {
        return prepared.card;
    }

    if(prepared.miracle_from_top)
    {
        play_miracle_bonus(state, 10);
    }
    else
    {
        PlayResolutionContext context;
        context.source = PlaySource::DECK_SEARCH;
        context.apply_destination = !state.swivel_waiting;
        resolve_played_card(state, prepared.card, context);
    }

    return prepared.card;
}

void deck_search_play_selected(GameState& state)
{
    deck_search_play_selected_type(state);
}

void move_toward(int &value, int target, int max_step)
{
    const int distance = target - value;

    if(distance == 0)
    {
        return;
    }

    if(distance > 0)
    {
        value += distance < max_step ? distance : max_step;
    }
    else
    {
        value -= -distance < max_step ? -distance : max_step;
    }
}

// Renders `source` windowed around `cursor` into `pool`. `visible_window` is the
// steady on-screen count used for centering/scroll math; pool may be larger for peeks.
CardRowResult render_card_row(bn::span<Card> pool, bn::span<const CardRef> source,
                              int cursor, int spacing, int y, int scroll_x, int target_scroll_x,
                              int x_offset, bn::span<const int> raise_offsets,
                              const InstancePool* instances, bn::sprite_text_generator* pip_generator,
                              int visible_window, const GameState* bounty_overlay_state,
                              bool bounty_graveyard_overlay)
{
    const int window = visible_window > 0 ? visible_window : pool.size();
    const int count = source.size();
    const int visible_count = count < window ? count : window;
    const int row_start_x = -(visible_count * spacing) / 2;
    const int logical_first = first_visible_index(cursor, count, window);
    const int first_visible = scroll_x / spacing;
    const int scroll_sub = scroll_x - first_visible * spacing;

    for(int slot = 0; slot < pool.size(); ++slot)
    {
        const int index = first_visible + slot;

        if(index < 0 || index >= count)
        {
            release_card_display_tiles(pool[slot]);
            continue;
        }

        pool[slot].set_type(source[index].type);

        const int card_x = row_start_x + slot * spacing - scroll_sub + x_offset;
        const int eased_raise = index < raise_offsets.size() ? raise_offsets[index] : 0;
        const int wave_raise = row_scroll_pair_raise(index, cursor, scroll_x, target_scroll_x, spacing, count);
        const int card_y = y - eased_raise - wave_raise;

        pool[slot].set_position(card_x, card_y);
        pool[slot].set_visible(true);
        pool[slot].set_blending_enabled(false);

        const CardInstance* instance = nullptr;

        if(instances && source[index].has_instance())
        {
            instance = instance_at(*instances, source[index].instance_id);
        }

        if(pip_generator && card_data(source[index].type).text_only)
        {
            const bool in_graveyard = bounty_graveyard_overlay;
            pool[slot].sync_face_labels(pip_generator, bounty_overlay_state, source[index], instance,
                                        in_graveyard);
        }
        else
        {
            pool[slot].clear_face_labels();
        }

        if(instances && pip_generator && source[index].has_instance())
        {
            pool[slot].set_upgrade_pips(pip_generator, instance);
        }
        else
        {
            pool[slot].clear_upgrade_pips();
        }

        if(bounty_overlay_state != nullptr && bounty_graveyard_overlay)
        {
            pool[slot].clear_amount_overlay();
        }
    }

    CardRowResult result;
    result.row_start_x = row_start_x;
    result.cursor_slot = count == 0 ? -1 : cursor - logical_first;
    result.visible_count = visible_count;
    result.scroll_sub = scroll_sub;
    result.has_left = logical_first > 0;
    result.has_right = logical_first + visible_count < count;
    return result;
}

bool graveyard_cursor_screen_position(int cursor, int graveyard_size, int spacing, int y, int selected_raise,
                                      int scroll_x, int main_x, int& out_x, int& out_y)
{
    if(graveyard_size <= 0 || cursor < 0 || cursor >= graveyard_size)
    {
        return false;
    }

    const int window = game_layout::VISIBLE_CARD_COUNT;
    const int visible_count = graveyard_size < window ? graveyard_size : window;
    const int row_start_x = -(visible_count * spacing) / 2;
    const int logical_first = first_visible_index(cursor, graveyard_size, window);
    const int first_visible = scroll_x / spacing;
    const int scroll_sub = scroll_x - first_visible * spacing;
    const int cursor_slot = cursor - logical_first;

    if(cursor_slot < 0 || cursor_slot >= visible_count)
    {
        return false;
    }

    out_x = row_start_x + cursor_slot * spacing - scroll_sub + main_x;
    out_y = y - selected_raise;
    return true;
}

void deal_next_hand(Deck &deck, GameState &state, int &selected_card)
{
    state.hand.clear();

    const int opening_n = state.round_start_seed.effective_opening_draw();
    int draw_count = deck.remaining() < opening_n ? deck.remaining() : opening_n;

    for (int card_index = 0; card_index < draw_count; ++card_index)
    {
        CardRef card;

        if (!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    for (int extra_draw = 0; extra_draw < state.round_start_extra_draws; ++extra_draw)
    {
        CardRef card;

        if (!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    state.round_start_extra_draws = 0;
    selected_card = 0;
}

// Build a fresh, shuffled battle deck from the player's persistent collection.
// Each Biggest Number game plays on this disposable copy; in-battle effects
// (draw, graveyard, RECLAIM's exile) mutate only the copy, so the collection is
// never touched and every owned card returns for the next game instance.
unsigned make_battle_random_seed(const bn::vector<CardType, 50>& collection)
{
    unsigned seed = static_cast<unsigned>(bn::core::current_cpu_ticks());
    seed ^= static_cast<unsigned>(bn::core::last_cpu_ticks()) << 16;
    seed ^= static_cast<unsigned>(collection.size()) * 2654435761u;

    for(int index = 0; index < collection.size(); ++index)
    {
        seed ^= static_cast<unsigned>(collection[index]) * (index + 1u);
        seed = (seed << 5) | (seed >> 27);
    }

    bn::seed_random mixer(seed);

    for(int index = 0; index < 4; ++index)
    {
        mixer.update();
        seed ^= mixer.get();
    }

    if(seed == 0)
    {
        seed = 1;
    }

    return seed;
}

unsigned make_battle_random_seed(const bn::vector<CardRef, 50>& collection)
{
    bn::vector<CardType, 50> types;

    for(int index = 0; index < collection.size(); ++index)
    {
        types.push_back(collection[index].type);
    }

    return make_battle_random_seed(types);
}

Deck build_battle_deck(const bn::vector<CardRef, 50>& collection, bn::seed_random& random_engine,
                       const InstancePool& pool)
{
    Deck deck(0);

    for(int index = 0; index < collection.size(); ++index)
    {
        deck.add_card(collection[index]);
    }

    deck.shuffle(random_engine);
    deck.apply_gravity(pool);
    return deck;
}

Deck build_battle_deck(const bn::vector<CardType, 50>& collection, bn::seed_random& random_engine)
{
    bn::vector<CardRef, 50> refs;

    for(CardType card_type : collection)
    {
        refs.push_back(CardRef{card_type, NO_INSTANCE});
    }

    InstancePool empty_pool;
    return build_battle_deck(refs, random_engine, empty_pool);
}
