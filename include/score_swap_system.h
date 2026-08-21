#ifndef SCORE_SWAP_SYSTEM_H
#define SCORE_SWAP_SYSTEM_H

#include "bn_array.h"

class GameContext;

enum class SwapScoreField : uint8_t
{
    ROUND,
    TOTAL,
};

struct ScoreSwapDigitSlot
{
    SwapScoreField field = SwapScoreField::TOTAL;
    int digit_index = 0;
    int sprite_index = 0;
};

struct ScoreSwapFxState
{
    bool active = false;
    bool swapping = false;
    int cursor_slot = 0;
    int selected_count = 0;
    int selected_slots[2] = {-1, -1};
    int slot_count = 0;
    bn::array<ScoreSwapDigitSlot, 24> slots{};
    int swap_frame = 0;
    int swap_slot_a = -1;
    int swap_slot_b = -1;
    bn::array<int, 24> digit_raise{};
    bn::array<int, 24> base_sprite_x{};
    bn::array<int, 24> base_sprite_y{};
};

bool score_swap_try_begin(GameContext& ctx);
void score_swap_handle_input(GameContext& ctx);
void score_swap_tick(GameContext& ctx);
bool score_swap_is_active(const GameContext& ctx);

#endif
