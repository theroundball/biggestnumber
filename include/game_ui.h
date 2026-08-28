#ifndef GAME_UI_H
#define GAME_UI_H

#include "bn_array.h"
#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_vector.h"

#include "bn_array.h"
#include "game_types.h"

// One OBJ palette slot shared by HUD icons, card markers, fade bands, and effect badges.
// GBA allows 16 sprite palettes total; battle UI must not allocate one palette per widget.
namespace ui_palette
{
    constexpr int TRANSPARENT = 0;
    constexpr int MARKER_X = 1;
    constexpr int MARKER_LOCK = 2;
    constexpr int PLACEHOLDER = 3;
    constexpr int SWIVEL_BADGE = 4;
    constexpr int ECHO_BADGE = 5;
    constexpr int DECK = 6;
    constexpr int GRAVEYARD = 7;
    constexpr int TRINKET_EMPTY = 8;
    constexpr int TRINKET_EQUIPPED = 9;
    constexpr int TRINKET_LUCKY_SEVENS = 10;
    constexpr int TRINKET_GET_WITH_THE_TIMES = 11;
    constexpr int TURTLE = 12;
    constexpr int TRINKET_PRIME_TIME = 13;
    constexpr int SCORE_BAR_GOLD = 15;

    [[nodiscard]] const bn::sprite_palette_ptr& shared();
}

class GameFadeBand
{
public:
    GameFadeBand(int x, int color_index, int card_y);

    void set_visible(bool visible);
    void set_position_y(int card_y);
    void set_position_x(int x);

private:
    bn::sprite_tiles_ptr _tiles;
    bn::sprite_palette_ptr _palette;
    bn::sprite_ptr _top_sprite;
    bn::sprite_ptr _bottom_sprite;
};

class GameMarker
{
public:
    enum class Style
    {
        X,
        LOCK,
    };

    explicit GameMarker(Style style = Style::X);

    void set_position(int x, int y);
    void set_visible(bool visible);

private:
    Style _style;
    bn::sprite_tiles_ptr _tiles;
    bn::sprite_palette_ptr _palette;
    bn::sprite_ptr _sprite;

    void paint();
};

class GraveyardPickPlaceholder
{
public:
    GraveyardPickPlaceholder();

    void set_position(int x, int y);
    void set_visible(bool visible);

private:
    bn::sprite_tiles_ptr _tiles;
    bn::sprite_palette_ptr _palette;
    bn::sprite_ptr _sprite;
};

// Horizontal track above the total score: gold = committed total, dark gray = current round.
class ScoreProgressBar
{
public:
    ScoreProgressBar();

    void sync(int total, int round_committed, int goal);
    void set_visible(bool visible);
    void set_x_offset(int panel_offset);

private:
    bn::sprite_tiles_ptr _track_tile;
    bn::sprite_tiles_ptr _gold_tile;
    bn::sprite_tiles_ptr _round_tile;
    bn::sprite_palette_ptr _palette;
    bn::vector<bn::sprite_ptr, game_layout::SCORE_BAR_SEGMENT_COUNT> _track_sprites;
    bn::vector<bn::sprite_ptr, game_layout::SCORE_BAR_SEGMENT_COUNT> _gold_sprites;
    bn::vector<bn::sprite_ptr, game_layout::SCORE_BAR_SEGMENT_COUNT> _round_sprites;
    int _last_total_px = -1;
    int _last_combined_px = -1;
    int _last_goal = -1;
    int _x_offset = 0;
    bool _visible = false;

    [[nodiscard]] int segment_center_x(int segment_index) const;
    void reposition_segments();
    void rebuild_fill_sprites(bn::vector<bn::sprite_ptr, game_layout::SCORE_BAR_SEGMENT_COUNT>& sprites,
                              int segment_count, int tile_index, int segment_offset);
};

// Chunked gray/gold bars for combo graveyard progress (right rail, below trinkets).
class ComboProgressBars
{
public:
    ComboProgressBars();

    void sync(const bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT>& lengths,
              const bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT>& filled);
    void set_visible(bool visible);

private:
    struct Row
    {
        bn::vector<bn::sprite_ptr, game_layout::COMBO_BAR_MAX_SEGMENTS> track_sprites;
        bn::vector<bn::sprite_ptr, game_layout::COMBO_BAR_MAX_SEGMENTS> fill_sprites;
        int last_length = -1;
        int last_filled = -1;
    };

    bn::sprite_tiles_ptr _track_tile;
    bn::sprite_tiles_ptr _fill_tile;
    bn::sprite_palette_ptr _palette;
    bn::array<Row, game_layout::COMBO_BAR_ROW_COUNT> _rows;
    bool _visible = false;

    [[nodiscard]] int segment_center_x(int segment_index, int segment_count) const;
    [[nodiscard]] int row_center_y(int row_index) const;
    void sync_row(int row_index, int segment_count, int filled_count);
};

// Small 16x16 badge rendered above a hand card (Echo trinket art, Swivel placeholder
// until hud_effect_swivel.bmp exists — swap CardEffectBadge::sprite_item_for(SWIVEL)).
class CardEffectBadge
{
public:
    enum class Kind
    {
        NONE,
        ECHO,
        SWIVEL,
    };

    CardEffectBadge();

    void set_kind(Kind kind);
    void set_position_above_card(int card_x, int card_y);
    void set_visible(bool visible);

private:
    Kind _kind = Kind::NONE;
    bn::optional<bn::sprite_ptr> _sprite;
    bn::optional<bn::sprite_tiles_ptr> _owned_tiles;

    void rebuild(Kind kind);
    void clear();
    void paint(Kind kind);
};

void apply_row_fade_bands(bn::array<GameFadeBand, 4>& bands, int card_y, bool has_left, bool has_right);
void hide_fade_bands(bn::array<GameFadeBand, 4>& bands);

struct CardFlightSample
{
    int x = 0;
    int y = 0;
    bn::fixed scale = 1;
    bn::fixed rotation = 0;
    bn::fixed alpha = 1;
};

int card_target_x_for_hud_icon(int hud_icon_x, int main_x);
int card_target_y_for_hud_icon(int hud_icon_y);
int card_target_x_for_score_center(int main_x);
int card_target_y_for_score_center();
int card_target_y_for_play_presentation();

CardFlightSample sample_card_flight(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames,
                                  bn::fixed from_rotation = 0, bn::fixed to_rotation = 0,
                                  bn::fixed from_scale = 1, bn::fixed to_scale = bn::fixed(1) / 8);

CardFlightSample sample_card_exile_dissipate(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames);

CardFlightSample sample_card_to_deck(int from_x, int from_y, int to_x, int to_y, int frame, int total_frames);

CardFlightSample sample_deck_to_hand_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                              int total_frames);

CardFlightSample sample_graveyard_to_hand_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                                 int total_frames);

CardFlightSample sample_graveyard_to_deck_flight(int from_x, int from_y, int to_x, int to_y, int frame,
                                                 int total_frames);
CardFlightSample sample_hud_via_center_flight(int from_x, int from_y, int center_x, int center_y,
                                              int to_x, int to_y, int frame, int total_frames);

void ease_raise_toward(int& current, int target);
int swap_vertical_arc(int frame, int total_frames, int peak);
int hand_swap_wave_raise(int card_index, int selected_card, int swap_direction, bool swapping_card, int swap_frame);
int row_scroll_pair_raise(int card_index, int cursor, int scroll_x, int target_scroll_x, int spacing, int count);

#endif
