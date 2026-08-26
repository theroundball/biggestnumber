#ifndef TRINKET_SYSTEM_H
#define TRINKET_SYSTEM_H

#include "game_state.h"

class GameContext;
class PersistentHud;

constexpr int TRINKET_WIGGLE_FRAMES = 72;
constexpr int LUCKY_SEVENS_ROULETTE_FRAMES = 120;
constexpr int LUCKY_SEVENS_ROULETTE_SETTLE_FRAMES = 20;
constexpr int LUCKY_SEVENS_ROLL_MIN = 7;
constexpr int LUCKY_SEVENS_ROLL_MAX = 13;

bool score_contains_seven(int value);
bool score_is_prime(int value);
int trinket_slot_index(const GameState& state, TrinketType type);

void trinket_queue_proc(GameState& state, TrinketType type);
void trinket_queue_score_check(GameState& state, TrinketScoreField field, int before, int after,
                               bool from_lucky_sevens_roll = false, int lucky_sevens_added = 0);

// True when no Lucky 7 FX / queued checks / apply-on-arrive flights are in flight.
bool trinket_score_reaction_idle(const GameContext& ctx);

// Apply deferred Morel stacks once reactions from prior adds have finished.
void trinket_flush_deferred_modifiers(GameContext& ctx);

void trinket_process_queues(GameContext& ctx);
void trinket_tick_fx(GameContext& ctx);
void trinket_render_fx(GameContext& ctx);

void staircase_reset(GameState& state);
void staircase_on_card_plus(GameState& state, int plus);
void staircase_flush_climb(GameState& state);

struct LuckySevensFxState
{
    bool active = false;
    int frame = 0;
    int slot = 0;
    TrinketScoreField field = TrinketScoreField::ROUND;
    int final_roll = 0;
    int displayed_roll = 0;
    int rendered_roll = -1;
};

#endif
