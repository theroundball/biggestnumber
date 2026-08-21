#ifndef SCORE_COUNT_SYSTEM_H
#define SCORE_COUNT_SYSTEM_H

#include "game_state.h"

class GameContext;

constexpr int SCORE_COUNT_MAX_FRAMES = 120;
constexpr int SCORE_COUNT_STEPS_PER_TICK = 5;

struct ScoreCountFxState
{
    bool active = false;
    int to = 0;
    int displayed = 0;
    int step = 1;
    int end_multiplier = 1;
};

void score_count_queue(GameState& state, TrinketScoreField field, int before, int after);
void score_count_process_pending(GameContext& ctx);
void score_count_tick(GameContext& ctx);
void score_count_cancel(GameContext& ctx, TrinketScoreField field);
bool score_count_is_active(const GameContext& ctx, TrinketScoreField field);

#endif
