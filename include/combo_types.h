#ifndef COMBO_TYPES_H
#define COMBO_TYPES_H

#include <cstdint>

#include "bn_array.h"
#include "bn_vector.h"
#include "card_type.h"

// Combo match / cinematic *data* only — kept separate from combo_system.h so
// game_state.h does not pull in combo resolution APIs (same idea as card_type.h).

enum class ComboZone
{
    HAND,
    GRAVEYARD,
    REVEALED,
};

struct PendingCombo
{
    uint8_t combo_id = 0;
    ComboZone zone = ComboZone::HAND;
    int start_index = 0;
    int length = 0;
    bn::array<int, 4> match_indices = {};
    bool use_match_indices = false;
};

struct ComboCinematicState
{
    bool active = false;
    int frame = 0;
    bn::vector<CardType, 4> cards;
    int card_count = 0;
};

constexpr int COMBO_GATHER_FRAMES = 24;
constexpr int COMBO_EXIT_FRAMES = 20;
constexpr int COMBO_TOTAL_FRAMES = COMBO_GATHER_FRAMES + COMBO_EXIT_FRAMES;

#endif
