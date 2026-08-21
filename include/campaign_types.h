#ifndef CAMPAIGN_TYPES_H
#define CAMPAIGN_TYPES_H

#include <cstdint>

#include "card_type.h"

enum class CampaignMode : uint8_t
{
    NONE,
    BIGGEST_NUMBER,
    SAME_NUMBER,
    NUMBER_NOW,
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
