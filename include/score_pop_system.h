#ifndef SCORE_POP_SYSTEM_H
#define SCORE_POP_SYSTEM_H

#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include <cstdint>

#include "game_state.h"

class GameContext;

constexpr int SCORE_POP_FRAMES = 17;
constexpr int SCORE_POP_RISE = 22;
constexpr int SCORE_POP_FADE_START = 11;
constexpr int SCORE_POP_TRINKET_HOLD_FRAMES = 4;
constexpr int SCORE_POP_TRINKET_FLY_FRAMES = 6;
constexpr int MAX_ACTIVE_SCORE_POPS = 6;

enum class ScorePopMotion : uint8_t
{
    FLOAT_RISE,      // Card pops near the score — rise and blink-fade
    TRINKET_FLIGHT,  // Spawn at trinket, hold, fly/zoom to score
};

struct ScorePop
{
    bn::vector<bn::sprite_ptr, 8> sprites;
    bn::vector<bn::fixed, 8> glyph_offset_x;
    bn::vector<bn::fixed, 8> glyph_offset_y;
    bn::optional<bn::sprite_affine_mat_ptr> affine_mat;
    int frame = 0;
    ScorePopMotion motion = ScorePopMotion::FLOAT_RISE;
    TrinketScoreField field = TrinketScoreField::ROUND;
    int from_x = 0;
    int from_y = 0;
    int to_x = 0;
    int to_y = 0;
    int amount = 0;
    bool defer_score_count = false;
    int count_before = 0;
    int count_after = 0;
    bool apply_on_arrive = false;
    bool arrived = false;
};

void score_pop_queue(GameState& state, int amount, bool is_morel = false,
                     TrinketScoreField field = TrinketScoreField::ROUND, bool is_multiply = false,
                     bool allow_zero = false);

// Bonus that appears beside a trinket, then flies to the score. Optionally defers
// score_count (or full apply) until the flight arrives.
void score_pop_queue_from_trinket(GameState& state, int amount, TrinketType trinket,
                                  TrinketScoreField field, int count_before, int count_after,
                                  bool defer_score_count = true, bool apply_on_arrive = false);

void score_pop_process_pending(GameContext& ctx);
void score_pop_tick(GameContext& ctx);
void score_pop_sync_positions(GameContext& ctx);
void score_pop_render(GameContext& ctx, bool visible);
bool score_pop_blocks_score_finalize(const GameContext& ctx, TrinketScoreField field);

#endif
