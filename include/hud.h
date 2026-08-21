#ifndef HUD_H
#define HUD_H

#include "bn_fixed.h"
#include "bn_sprite_item.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"
#include "bn_array.h"
#include "bn_string.h"

#include "campaign_types.h"
#include "game_state.h"

// Persistent top-bar HUD: deck stack, graveyard tombstone, and trinket slots.
class PersistentHud
{
public:
    PersistentHud(bn::sprite_text_generator& count_text_generator,
                  bn::sprite_text_generator& mod_text_generator);

    void update(const GameState& state);
    void set_visible(bool visible);
    void sync_details_modifiers(const GameState& state, int panel_x, bool visible,
                                CampaignMode campaign_mode = CampaignMode::NONE,
                                int number_now_scoring_round = 0);
    void start_trinket_wiggle(int slot);
    void tick_trinket_wiggles();
    bool trinket_slot_position(int slot, int& out_x, int& out_y) const;

private:
    enum class IconKind
    {
        DECK_STACK,
        TOMBSTONE,
        TRINKET_EMPTY,
        TRINKET_MOREL,
        TRINKET_LUCKY_SEVENS,
        TRINKET_ECHO,
        TRINKET_GET_WITH_THE_TIMES,
        TRINKET_PRIME_TIME,
        TURTLE_TIMER,
    };

    class HudIcon
    {
    public:
        explicit HudIcon(IconKind kind, bool compact = false);

        void set_kind(IconKind kind);
        void set_position(int x, int y);
        void set_visible(bool visible);

    private:
        IconKind _kind;
        bool _compact;
        bn::sprite_tiles_ptr _tiles;
        bn::sprite_palette_ptr _palette;
        bn::sprite_ptr _sprite;

        static const bn::sprite_item* sprite_item_for(IconKind kind);
        void apply_kind(IconKind kind);
        void paint(IconKind kind);
        static int solid_color_index(IconKind kind);
    };

    class HudCount
    {
    public:
        explicit HudCount(bn::sprite_text_generator& text_generator);

        void set_value(int value);
        void set_position(int x, int y);
        void set_visible(bool visible);

    private:
        void redraw();

        bn::sprite_text_generator& _text_generator;
        bn::vector<bn::sprite_ptr, 8> _sprites;
        int _last_value = -1;
        int _x = 0;
        int _y = 0;
        bool _visible = true;
    };

    class HudModLine
    {
    public:
        explicit HudModLine(bn::sprite_text_generator& text_generator);

        void set_modifier(const RoundModifier& modifier);
        void set_round_prefix(const bn::string_view& prefix);
        void clear_round_prefix();
        void set_position(int x, int y);
        void set_visible(bool visible);

    private:
        void redraw();
        void append_segment_at(int x, const bn::string_view& text, bn::fixed scale);

        bn::sprite_text_generator& _text_generator;
        bn::vector<bn::sprite_ptr, 16> _sprites;
        RoundModifier _last_modifier{};
        bn::string<8> _round_prefix;
        bool _has_modifier = false;
        int _x = 0;
        int _y = 0;
        bool _visible = true;
    };

    void update_modifier_rows(const GameState& state,
                              bn::array<HudModLine, 3>& lines,
                              bn::array<HudIcon, 3>& turtles,
                              int origin_x,
                              int turtle_x,
                              int text_x,
                              int text_x_with_turtle,
                              int row_y0,
                              int row_spacing,
                              bool visible,
                              CampaignMode campaign_mode = CampaignMode::NONE,
                              int number_now_scoring_round = 0);

    HudIcon _deck_icon;
    HudIcon _tomb_icon;
    bn::array<HudIcon, 3> _trinket_icons;
    HudCount _deck_count;
    HudCount _grave_count;
    bn::array<HudModLine, 3> _future_mod_lines;
    bn::array<HudIcon, 3> _turtle_row_icons;
    bn::array<HudModLine, 3> _details_mod_lines;
    bn::array<HudIcon, 3> _details_turtle_icons;
    bn::array<int, 3> _trinket_base_x{};
    bn::array<int, 3> _trinket_base_y{};
    bn::array<int, 3> _trinket_wiggle_frames{};
    bool _visible = true;
};

#endif
