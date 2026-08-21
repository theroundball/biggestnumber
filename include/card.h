#ifndef CARD_H
#define CARD_H

#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"
#include "card_instance.h"
#include "card_type.h"

// Forward declare GameState so a card can act on it without an include cycle.
struct GameState;

// Resolve card effects without allocating display sprites (safe to call every play).
void apply_card_play(GameState& state, CardType type);
void apply_card_play(GameState& state, CardRef card);
void apply_card_discard(GameState& state, CardType type);

// A rendered card instance living in the hand: its identity (CardType) plus the
// sprites used to draw it. All gameplay data (name, effect) lives in CardData;
// see card_data.h.
class Card
{
public:
    // Safe default constructor so the fixed-capacity hand vector allocates safely.
    Card();

    // Active card constructor.
    Card(CardType type, bn::fixed x, bn::fixed y);

    // Resolve this card's effect against the game state.
    void play(GameState& state) const;

    // Resolve this card's discard effect (if any) against the game state.
    void discard(GameState& state) const;

    CardType get_type() const { return _type; }

    // Repaint this instance to show a different card (reuses the same sprites).
    void set_type(CardType type);

    void set_position(bn::fixed x, bn::fixed y);
    void set_visible(bool visible);
    void set_blending_enabled(bool blending_enabled);
    void set_visual(bn::fixed scale, bn::fixed rotation_degrees);
    void clear_visual();
    void set_draw_on_top(bool on_top);

    // Corner pips for run upgrades (+ / x / L / Y). Pass nullptr to clear.
    void set_upgrade_pips(bn::sprite_text_generator* generator, const CardInstance* instance);
    void clear_upgrade_pips();

private:
    void apply_visual_transform();
    void sync_upgrade_pip_visibility();
    void reposition_upgrade_pips();

    CardType _type;
    bn::fixed _x;
    bn::fixed _y;
    bn::fixed _visual_scale = 1;
    bn::fixed _visual_rotation = 0;
    bool _visual_active = false;
    bool _visible = false;
    bn::optional<bn::sprite_affine_mat_ptr> _affine_mat;
    bn::sprite_ptr _body;
    bn::sprite_ptr _accent_top;
    bn::sprite_ptr _accent_bottom;
    bn::vector<bn::sprite_ptr, 4> _upgrade_pips;
    bn::string<8> _upgrade_pip_text;
    bn::sprite_text_generator* _upgrade_pip_generator = nullptr;
    bn::fixed _pip_anchor_x = 0;
    bn::fixed _pip_anchor_y = 0;

    void sync_part_visibility();
    void reposition_parts();
};

// Shared placeholder graphics so idle display slots drop unique card tile refs.
constexpr CardType CARD_DISPLAY_PLACEHOLDER = CardType::SIPS;
void release_card_display_tiles(Card& card);

#endif
