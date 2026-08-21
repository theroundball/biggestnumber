#ifndef SWIVEL_SYSTEM_H
#define SWIVEL_SYSTEM_H

#include "card_type.h"

class GameContext;

struct SwivelFxState
{
    CardType swivel_type = CardType::COUNT;
};

bool swivel_is_waiting(const GameContext& ctx);
void swivel_on_swivel_played(GameContext& ctx);
void swivel_abandon_wait(GameContext& ctx);
void swivel_complete_follow(GameContext& ctx);
void swivel_clear_wait_if_hand_empty(GameContext& ctx);
void swivel_tick_stall_recovery(GameContext& ctx);

#endif
