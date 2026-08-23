#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include "bn_fixed.h"

enum class GameMode
{
    NORMAL,
    DISCARD_TARGET,
    GRAVEYARD_TARGET,
    GRAVEYARD_PICK,
    SCRY,
    DECK_SEARCH,
    COMBO,
    SWIVEL,
    SCORE_SWAP,
};

enum class SidePanel
{
    NONE,
    DETAILS,
    EXILE,
    GRAVEYARD,
};

enum class PanelTransition
{
    NONE,
    OPEN_DETAILS,
    CLOSE_DETAILS,
    OPEN_GRAVEYARD,  // right-side card browse (graveyard or exile)
    CLOSE_GRAVEYARD,
};

enum class RemovalStyle : uint8_t
{
    TO_GRAVEYARD,
    TO_DECK_TOP,
    EXILE_DISSIPATE,
};

enum class DeckSearchResolvePhase : uint8_t
{
    PICK_FLIGHT,
    WAIT_PRESENTATION,
};

enum class PlayRemovalPhase : uint8_t
{
    APPROACH,
    HOLD,
    DEPART,
    WAIT_PRESENTATION,
};

enum class PlayPresentOrigin : uint8_t
{
    HAND,
    DECK,
    SCRY,
    DECK_SEARCH,
    ECHO,
    GRAVEYARD,
};

enum class GraveyardExilePickKind : uint8_t
{
    NONE,
    MULTIPLY_BY_COUNT,
    FROM_GRAVEYARD_THEN_MULTIPLY,
    TO_DECK_TOP,
    TO_HAND,
    SHUFFLE_TO_DECK,
};

namespace game_layout
{
    constexpr int PANEL_WIDTH = 240;
    constexpr int PANEL_SLIDE_SPEED = 15;
    constexpr int VISIBLE_CARD_COUNT = 7;
    constexpr int HAND_DISPLAY_POOL = VISIBLE_CARD_COUNT + 2;
    // Max cards fully on-screen at HAND/GRAVE spacing (240 / 40). Larger decks must scroll.
    constexpr int CARD_CAROUSEL_VISIBLE = 5;
    constexpr int CARD_CAROUSEL_POOL = CARD_CAROUSEL_VISIBLE + 2;
    constexpr int HAND_X = -140;
    constexpr int HAND_SPACING = 40;
    constexpr int HAND_Y = 14;
    constexpr int SELECTED_RAISE = 8;
    constexpr bn::fixed EDGE_FADE_ALPHA = bn::fixed(0.7);
    constexpr int SCROLL_SPEED = 6;
    constexpr int DIRECTION_INITIAL_DELAY = 15;
    constexpr int DIRECTION_REPEAT_INTERVAL = 8;
    // Hold-acceleration floors (intervals shorter than this feel stuttery vs scroll anim).
    constexpr int DIRECTION_REPEAT_INTERVAL_MIN = 2;
    constexpr int DIRECTION_ACCEL_STEPS_MAX = 4;
    constexpr int HAND_RAISE_EASE_DIVISOR = 3;
    constexpr int SWAP_FRAMES = 12;
    constexpr int SWAP_EASE_SCALE = 256;
    constexpr int SWAP_ARC_PEAK = 5;
    constexpr int REMOVAL_FRAMES = 24;
    constexpr int PLAY_APPROACH_FRAMES = 16;
    constexpr int PLAY_DECK_APPROACH_FRAMES = 28;
    constexpr int PLAY_HOLD_FRAMES = 15;
    constexpr int PLAY_DEPART_FRAMES = 24;
    constexpr int HAND_DRAW_FRAMES = 8;
    constexpr int ZONE_TRANSFER_FRAMES = 28;
    constexpr int REMOVAL_MIN_SCALE = 1;
    constexpr int REMOVAL_MIN_SCALE_DIVISOR = 8;
    constexpr int EXILE_MIN_SCALE = 1;
    constexpr int EXILE_MIN_SCALE_DIVISOR = 8;
    constexpr int HUD_DECK_X = -108;
    constexpr int HUD_DECK_Y = -68;
    constexpr int HUD_GRAVEYARD_X = 108;
    constexpr int HUD_GRAVEYARD_Y = -68;
    constexpr int HAND_CARD_Z = -5;
    constexpr int PLAY_PRESENTATION_CARD_Z = -8;
    constexpr int PLAY_PRESENTATION_SCORE_Z = -30;
    constexpr int MARKER_Z_ORDER = -10;
    constexpr int SCORE_CENTER_Y = -41;
    constexpr int CARD_BODY_HEIGHT = 64;
    // Play beat: sit above raised hand row with a small gap (card position is top-left).
    constexpr int PLAY_PRESENTATION_CENTER_Y =
        HAND_Y - SELECTED_RAISE - CARD_BODY_HEIGHT - 8;
    // Deck builder: debug mode shows every card type regardless of library ownership.
    constexpr bool DECK_EDITOR_TEST_ALL_CARDS = false;
    constexpr int SCORE_WIGGLE_FRAMES = 5;
    constexpr int SCRY_Y = -2;
    constexpr int SCRY_SPACING = 28;
    constexpr int SCRY_VISIBLE = 7;
    constexpr int SCRY_SELECT_RAISE = 8;
    constexpr int GRAVE_SPACING = 40;
    constexpr int GRAVE_Y = -24;
    constexpr int GRAVE_SELECTED_RAISE = 8;
    constexpr int GRAVEYARD_BROWSE_Y = 2;
    constexpr int GRAVEYARD_BROWSE_SELECTED_RAISE = 8;
    constexpr int SCREEN_TOP = -80;
    constexpr int GRAVE_SELECTION_PLACEHOLDER_SIZE = 32;
    constexpr int CARD_BODY_WIDTH = 32;
    constexpr int CARD_ACCENT_WIDTH = 8;
    constexpr int CARD_DISPLAY_WIDTH = CARD_BODY_WIDTH + CARD_ACCENT_WIDTH;
    constexpr int CARD_EFFECT_BADGE_Y = -12;
    constexpr int GRAVEYARD_BROWSE_CARD_TOP = GRAVEYARD_BROWSE_Y - GRAVEYARD_BROWSE_SELECTED_RAISE;
    constexpr int GRAVEYARD_BROWSE_PLACEHOLDER_Y =
        (SCREEN_TOP + GRAVEYARD_BROWSE_CARD_TOP) / 2;
    constexpr int GRAVE_SELECTION_CARD_TOP = GRAVE_Y - GRAVE_SELECTED_RAISE;
    constexpr int GRAVE_SELECTION_PLACEHOLDER_Y =
        (SCREEN_TOP + GRAVE_SELECTION_CARD_TOP) / 2;
}

struct GameSceneResult
{
    int final_score = 0;
    int last_round_score = 0;
    int last_round_number = 0;
    bool exited_early = false;
};

struct CardRowResult
{
    int row_start_x;
    int cursor_slot;
    int visible_count = 0;
    int scroll_sub = 0;
    bool has_left;
    bool has_right;
};

#endif
