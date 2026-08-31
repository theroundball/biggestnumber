#ifndef POKER_HAND_H
#define POKER_HAND_H

#include "bn_array.h"
#include "campaign_types.h"

struct PokerHandEvaluation
{
    PokerHandRank rank = PokerHandRank::HIGH_CARD;
    int score = 0;
    bool valid = false;
};

PokerHandEvaluation poker_hand_evaluate(const bn::array<int, 5>& digits);
const char* poker_hand_rank_name(PokerHandRank rank);

#endif
