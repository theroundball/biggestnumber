#ifndef CAMPAIGN_TYPES_H
#define CAMPAIGN_TYPES_H

#include <cstdint>

#include "card_type.h"

constexpr int POKER_HAND_RANK_COUNT = 9;

enum class CampaignMode : uint8_t
{
    NONE,
    BIGGEST_NUMBER,
    SAME_NUMBER,
    NUMBER_NOW,
    AINT_GOT_TIME,
    SHARING_IS_CARING,
    POKER_HAND,
};

enum class PokerHandRank : uint8_t
{
    HIGH_CARD,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    FIVE_OF_A_KIND,
    COUNT
};

// Battle details-panel + HUD labels (replaces overloading score_to_beat for display).
struct CampaignUiContext
{
    CampaignMode mode = CampaignMode::NONE;
    int biggest_number_record = 0;
    int same_number_target = 0;
    int number_now_scoring_round = 1;
    int number_now_round_peak = 0;
    int number_now_round_count = 1;
    int aint_got_time_record = 0;
    int sharing_is_caring_record = 0;
    int poker_hand_records[POKER_HAND_RANK_COUNT] = {};
};

enum class PrizeOfferKind : uint8_t
{
    CARD,
    UPGRADE_PLUS_DIGIT,
    UPGRADE_INCREMENT_MULT,
    UPGRADE_LEAD,
    UPGRADE_YEAST,
    TRINKET,
};

struct PrizeOffer
{
    PrizeOfferKind kind = PrizeOfferKind::CARD;
    CardType card = CardType::COUNT;
    uint8_t trinket = 0;
};

constexpr int CAMPAIGN_NUMBER_NOW_MAX_ROUNDS = 10;
constexpr int CAMPAIGN_PRIZE_SLOT_COUNT = 3;
constexpr int CAMPAIGN_FLEX_SLOT_INDEX = 1;

#endif
