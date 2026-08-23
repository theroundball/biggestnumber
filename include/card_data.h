#ifndef CARD_DATA_H
#define CARD_DATA_H

#include "bn_sprite_item.h"
#include "card_type.h"
#include "scoring.h"

// Forward declared so card definitions can carry effect callbacks without
// pulling in the whole game state here.
struct GameState;

// The shared, read-only definition of a card. One record per CardType.
//
// Common cases are pure data: immediate score deltas and up to three future-round
// modifiers. Exotic cards (deck/hand/graveyard manipulation) supply an on_play
// and/or on_discard callback; leave them null otherwise.
struct CardData
{
    const char* name = "";
    const char* description = "";   // human-readable effect text (shown in the inspect view)
    const bn::sprite_item* body_item = nullptr;
    const bn::sprite_item* accent_top_item = nullptr;
    const bn::sprite_item* accent_bottom_item = nullptr;

    int immediate_plus = 0;
    int immediate_multiply = 0;

    RoundModifier future[3] = {};   // [0..2] = next three rounds

    void (*on_play)(GameState&) = nullptr;
    void (*on_discard)(GameState&) = nullptr;
    void (*on_exile)(GameState&) = nullptr;

    bool defer_graveyard_until_pending = false;   // e.g. Clover: exile picks before this hits GY
    bool exiles_self_on_play = false;             // e.g. Necromancy: leaves the game on play
    bool has_cycle = false;
    bool has_flashback = false;
    int flashback_plus = 0;
};

// Look up the definition for a card type (indexed into the static table).
const CardData& card_data(CardType type);

int count_unique_graveyard_types(const GameState& state);

// Lifeline reclaim: merge entire graveyard into deck (including Lifeline if already
// routed there), shuffle, exile 5 undrawn cards from deck bottom.
constexpr int LIFELINE_EXILE_COUNT = 5;

void reclaim_graveyard_into_deck(GameState& state);

#endif
