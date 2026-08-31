#ifndef GAME_STATE_H
#define GAME_STATE_H

// Rules / run state. May include combo_types.h (data) but must not include
// combo_system.h (resolution APIs) — keeps the card ↔ state graph acyclic.

#include "bn_array.h"
#include "bn_vector.h"
#include "bn_seed_random.h"
#include <cstdint>

#include "combo_types.h"
#include "campaign_types.h"
#include "scoring.h"
#include "card_type.h"
#include "card_instance.h"
#include "deck.h"
#include "play_resolution.h"

constexpr int DEAD_RISING_GY_RETURNS_PER_ROUND = 2;

void bounty_on_round_start(GameState& state);

enum class TrinketScoreField : uint8_t
{
    ROUND,
    TOTAL,
};

enum class TrinketType : uint8_t
{
    NONE,
    MOREL,
    LUCKY_SEVENS,
    ECHO,
    GET_WITH_THE_TIMES,
    PRIME_TIME,
    FIBONACCI,
    STAIRCASE,
    LONGSLEEVES,
    COUNT,
};

// Options for launching a campaign battle.
struct BattleLaunch
{
    int deck_index = -1;
    // When >= 0, total score tints green once it exceeds this value.
    // When < 0, uses the saved deck high score (classic Play Game).
    int score_to_beat = -1;
    bn::array<TrinketType, 3> trinkets = {
        TrinketType::MOREL,
        TrinketType::LUCKY_SEVENS,
        TrinketType::PRIME_TIME,
    };
    InstancePool instance_pool{};
    bn::array<CardRef, 2> longsleeve_cards = {};
    CampaignUiContext campaign_ui{};
    CampaignMode campaign_mode = CampaignMode::NONE;
    int same_number_target = 0;
    int number_now_scoring_round = 1;
    int number_now_round_peak = 0;
};

struct PendingScoreCheck
{
    TrinketScoreField field = TrinketScoreField::ROUND;
    int before = 0;
    int after = 0;
    bool from_lucky_sevens_roll = false;
    int lucky_sevens_added = 0;
};

struct ScorePopRequest
{
    int amount = 0;
    bool is_morel = false; // legacy; prefer from_trinket
    bool is_multiply = false;
    TrinketScoreField field = TrinketScoreField::ROUND;
    TrinketType from_trinket = TrinketType::NONE;
    // When set, score_count waits until the pop finishes flying to the score.
    bool defer_score_count = false;
    int count_before = 0;
    int count_after = 0;
    // Lucky Sevens: apply the roll to state only when the flight arrives.
    bool apply_on_arrive = false;
};

struct ScoreCountRequest
{
    TrinketScoreField field = TrinketScoreField::ROUND;
    int before = 0;
    int after = 0;
};

// Interactive follow-ups a card effect can queue for the main loop to resolve
// across multiple frames (each drives its own selection mode / cursor).
enum class PendingActionType
{
    NONE,
    EXILE_FROM_GRAVEYARD,
    EXILE_FROM_GRAVEYARD_THEN_MULTIPLY,
    EXILE_GRAVEYARD_MULTIPLY_BY_COUNT,
    DISCARD_FROM_HAND_THEN_MULTIPLY,
    OVERCLOCK_DISCARD_PROMPT,
    DISCARD_FROM_HAND,
    EXILE_FROM_HAND,
    PUT_HAND_ON_DECK_TOP,
    RETRIEVE_FROM_GRAVEYARD,
    RETRIEVE_FROM_GRAVEYARD_TO_TOP,
    GRAVEYARD_PICK_TO_BOTTOM,
    GRAVEYARD_PICK_TO_TOP,
    GRAVEYARD_PAIR_SWAP,
    SCRY,
    DECK_SEARCH,
    PLAY_DECK_TOP,
    PLAY_RANDOM_GRAVEYARD,
    ROLL_OVER_SUBSTITUTE,
    BUILD_A_NUMBER_PLACE_DIGIT,
    POKER_HAND_PLACE_DIGIT,
    PAPER_SWAP_HAND,
    PEANUT_BUTTER_SCRY,
    MIRACLE_AUTO_PLAY,
    EFFECT_DECK_DRAW,
    COMBO_CINEMATIC,
    SWAP_TOTAL_SCORE_DIGITS,
    MOVE_FOUR_TOTAL_DIGIT,
    REPLACE_TOTAL_DIGIT_WITH_FIVE,
    MAJOR_LIFT_TOTAL_DIGIT,
    MINOR_FALL_TOTAL_DIGIT,
    // Birds of a Feather: animate a qualifying GY run onto the deck.
    BIRDS_RETURN,
    // Lifeline: shuffle GY→deck after the played card has entered the GY.
    RECLAIM_GRAVEYARD,
    NECROMANCY_SHUFFLE,
    MILL_REVEAL,
    // Evaluate ghost: one queued step per UI row/component.
    EVALUATE_GHOST_STEP,
};

struct PendingAction
{
    PendingActionType type;
    int count = 1;
    int hand_index = -1;
    CardType graveyard_exclude = CardType::COUNT;
};

struct PendingHandDraw
{
    CardRef card;
    bool miracle_auto_play = false;
};

struct BattleStats
{
    int cards_exiled = 0;
    int cards_drawn_this_round = 0;
    int keyword_discards = 0;
    int cycles = 0;
    int ghost_plays = 0;
};

struct SelectionSession
{
    PendingActionType type = PendingActionType::NONE;
    int cursor = 0;

    bn::vector<CardRef, 7> scry_buffer;

    bn::vector<CardRef, 7> picked_ordered;
    int remaining_picks = 0;

    bn::vector<CardRef, 50> deck_search_buffer;
    CardType graveyard_exclude = CardType::COUNT;
    int multiply_factor = 0;
    int exiled_count = 0;
    int graveyard_swap_first = -1;
};

struct GameState
{
    int total_score = 0;
    RoundScore round;
    int current_round = 1;
    int cards_played_this_round = 0;
    bool echo_ready = false;
    RoundModifier round_start_seed = {};
    int round_start_extra_draws = 0;

    RoundModifier future_mods[3] = {};
    uint8_t keep_going_returns[3] = {};
    int keep_going_relocation_triggers = 0;
    int next_mod_index = 0;

    bn::vector<CardRef, 60> hand;
    bn::vector<CardRef, 50> graveyard;
    bn::vector<CardRef, 50> exile;

    bn::vector<PendingAction, 8> pending_actions;
    SelectionSession selection;
    PendingCombo pending_combo;
    ComboCinematicState combo_cinematic;
    // Combo progress bars: set at battle start from starting-deck composition (kCombos index).
    bool combo_progress_enabled[3] = {};

    int roundup_play_count = 0;
    int turtle_rounds_remaining = 0;
    bool swivel_waiting = false;
    // B skips optional ghost/combo offers and ends the round (commit → round start → draw).
    bool waive_optional_ghost_plays = false;
    // Armed on first play-effect while Echo is ready; drained by idle gate
    // (try_drain_echo_replay) — not via second-pass removal or card-specific flags.
    bool echo_pending_replay = false;
    CardRef echo_replay_card{};
    PlaySource echo_replay_scoring_source = PlaySource::HAND;
    int birds_return_threshold = 2;
    int birds_return_start = -1;
    int birds_return_count = 0;
    bool pending_double_adds = false;
    bool applying_double_adds = false;
    bool first_deck_draw_this_round = true;
    int prime_time_total_procs = 0;
    int prime_time_round_procs = 0;
    bn::vector<CardRef, 2> longsleeve_cards;
    BattleStats battle_stats;
    int effect_draw_remaining = 0;
    bool effect_draw_miracle_first = false;
    bool effect_draw_miracle_chaining = false;
    bool roll_over_substitution_active = false;
    bool roll_over_pick_active = false;
    bool roll_over_awaiting_follow_up = false;
    bn::vector<CardRef, 60> roll_over_stashed_hand;
    bn::array<CardRef, 2> roll_over_choices = {};
    int roll_over_choice_count = 0;
    CardRef roll_over_follow_up_card{};
    // Per physical Bounty copy (CardRef.bounty_id), not save instance_id.
    bn::array<uint8_t, InstancePool::CAPACITY> bounty_instance_plays{};
    bn::array<int, InstancePool::CAPACITY> bounty_instance_return_anchor{};
    bn::array<int, InstancePool::CAPACITY> bounty_instance_return_threshold{};
    uint8_t bounty_next_id = 0;
    CardRef play_effect_card{};
    bool finale_active = false;
    int paper_swap_hand_index = -1;
    int staircase_last_plus = 0;
    int staircase_length = 0;
    int staircase_sum = 0;
    bool build_a_number_active = false;
    bn::array<int, 3> build_digits = {-1, -1, -1};
    int build_pre_running = 0;
    int build_pre_end_multiplier = 1;
    bool applying_build_a_number_payout = false;
    bool poker_hand_active = false;
    bn::array<int, 5> poker_digits = {-1, -1, -1, -1, -1};
    bool sharing_is_caring_active = false;
    int sharing_round_mult = 5;

    bn::array<TrinketType, 3> trinkets = {
        TrinketType::MOREL,
        TrinketType::LUCKY_SEVENS,
        TrinketType::PRIME_TIME,
    };

    InstancePool instance_pool;

    bool has_longsleeves() const
    {
        return has_trinket(TrinketType::LONGSLEEVES);
    }

    // Morel +2 stacks deferred until Lucky 7 / Prime reactions from the card add settle.
    int deferred_morel_count = 0;

    bn::vector<TrinketType, 8> pending_trinket_procs;
    bn::vector<PendingScoreCheck, 12> pending_score_checks;
    bn::vector<ScorePopRequest, 12> pending_score_pops;
    bn::vector<ScoreCountRequest, 8> pending_score_counts;
    bn::vector<PendingHandDraw, 12> pending_hand_draws;

    Deck& deck;
    bn::seed_random& rng;
    int starting_deck_size;

    GameState(Deck& d, bn::seed_random& r) : deck(d), rng(r), starting_deck_size(d.size())
    {
        echo_ready = has_trinket(TrinketType::ECHO);
    }

    bool has_trinket(TrinketType type) const
    {
        for(TrinketType trinket : trinkets)
        {
            if(trinket == type)
            {
                return true;
            }
        }

        return false;
    }

    bool echo_first_play_active() const
    {
        return has_trinket(TrinketType::ECHO) && echo_ready;
    }

    void consume_echo()
    {
        echo_ready = false;
    }

    RoundModifier& mod_next() { return future_mods[next_mod_index]; }
    RoundModifier& mod_after_next() { return future_mods[(next_mod_index + 1) % 3]; }
    RoundModifier& mod_third() { return future_mods[(next_mod_index + 2) % 3]; }
    void schedule_keep_going();
    void resolve_keep_going_round_start();
    void finish_keep_going_round_start();

    void flush_staircase_climb();

    void commit_round()
    {
        flush_staircase_climb();
        total_score += round.committed();
        round.reset();
    }

    // Any positive round add that originates from a card (immediate play, discard,
    // future round seed, combo bonus, etc.). Trinket-sourced adds must not use this.
    int add_from_card(int amount);
    void mul_from_card(int factor);
    void apply_seed_multiply(int factor);
    void apply_round_start_trinkets();
    void apply_round_start_adds();
    void apply_round_start_multiply();
    bool apply_round_start_turtle_step();
    // clear_after: false = hand play (next slot only); true = ghost (all 3 slots, then clear).
    void evaluate_apply_future_modifiers(bool clear_after);
    void evaluate_apply_ui_slot(int ui_slot_offset, bool clear_after);
    void queue_evaluate_ghost_steps();
    void build_a_number_activate();
    void build_a_number_reset();
    bool build_a_number_all_digits_filled() const;
    int build_a_number_assembled_value() const;
    void build_a_number_complete_payout();
    int build_a_number_commit_prebuild();

    void start_new_round(bool preserve_round_score = false)
    {
        flush_staircase_climb();
        ++current_round;

        round_start_seed = future_mods[next_mod_index];
        future_mods[next_mod_index] = RoundModifier{};
        next_mod_index = (next_mod_index + 1) % 3;

        if(!preserve_round_score)
        {
            round.reset();
        }

        round_start_extra_draws = round_start_seed.draw_at_start;
        cards_played_this_round = 0;
        battle_stats.cards_drawn_this_round = 0;
        prime_time_round_procs = 0;
        echo_ready = has_trinket(TrinketType::ECHO);
        echo_pending_replay = false;
        echo_replay_card = CardRef{};
        first_deck_draw_this_round = true;
        roll_over_substitution_active = false;
        roll_over_pick_active = false;
        roll_over_awaiting_follow_up = false;
        roll_over_stashed_hand.clear();
        roll_over_choices = {};
        roll_over_choice_count = 0;
        roll_over_follow_up_card = CardRef{};
        // waive_optional_ghost_plays is cleared after the round-finish pipeline runs
        // end_run_if_needed (see try_finish_round_after_empty_hand / finish_deferred_round_start).
        bounty_on_round_start(*this);
    }
};

#endif
