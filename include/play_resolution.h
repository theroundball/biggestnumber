#ifndef PLAY_RESOLUTION_H
#define PLAY_RESOLUTION_H

#include <cstdint>

#include "card_instance.h"
#include "card_type.h"

struct GameState;

enum class PlaySource : uint8_t
{
    HAND,
    SCRY,
    DECK_SEARCH,
    DECK_TOP,
    ECHO,
    GHOST,
    LONGSLEEVE,
};

enum class PostPlayDestination : uint8_t
{
    NONE,
    GRAVEYARD,
    EXILE,
    DECK_TOP,
};

struct PlayResolutionResult
{
    PostPlayDestination dest = PostPlayDestination::GRAVEYARD;
    bool increment_cards_played = true;
};

struct PlayResolutionContext
{
    PlaySource source = PlaySource::HAND;
    int hand_index = -1;
    int* selected_card = nullptr;
    bool apply_destination = true;
};

PostPlayDestination route_played_card(const GameState& state, CardType type, PlaySource source,
                                      bool swivel_follow = false);

void apply_post_play_destination(GameState& state, CardRef card, PlaySource source,
                                 PostPlayDestination dest, int hand_index, int* selected_card,
                                 bool trigger_discard_on_graveyard = false);

PlayResolutionResult resolve_played_card(GameState& state, CardRef card,
                                         const PlayResolutionContext& context);

#endif
