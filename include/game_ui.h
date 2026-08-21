#ifndef GAME_UI_H
#define GAME_UI_H

#include "bn_array.h"
#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_tiles_ptr.h"

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
    bn::optional<bn::sprite_palette_ptr> _owned_palette;

    static const bn::sprite_item* sprite_item_for(Kind kind);
    void rebuild(Kind kind);
    void clear();
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

void ease_raise_toward(int& current, int target);
int swap_vertical_arc(int frame, int total_frames, int peak);
int hand_swap_wave_raise(int card_index, int selected_card, int swap_direction, bool swapping_card, int swap_frame);
int row_scroll_pair_raise(int card_index, int cursor, int scroll_x, int target_scroll_x, int spacing, int count);

#endif
