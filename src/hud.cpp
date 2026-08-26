#include "hud.h"

#include "bn_affine_mat_attributes.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_span.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_string.h"
#include "bn_tile.h"
#include "game_ui.h"
#include "scoring.h"

#include "campaign_types.h"
#include "trinket_system.h"

namespace
{
    constexpr int HUD_MARGIN = 4;
    constexpr int HUD_ICON_HALF = 8;
    constexpr int HUD_SMALL_ICON_HALF = 4;
    constexpr int HUD_ROW_GAP = 4;
    constexpr int HUD_ICON_Z_ORDER = -2;
    constexpr int HUD_TEXT_Z_ORDER = -3;

    // Butano screen center is (0, 0); top-left corner is (-120, -80) on a 240x160 display.
    constexpr int DECK_X = -120 + HUD_MARGIN + HUD_ICON_HALF;
    constexpr int DECK_Y = -80 + HUD_MARGIN + HUD_ICON_HALF;
    constexpr int TOMB_X = 120 - HUD_MARGIN - HUD_ICON_HALF;
    constexpr int TOMB_Y = DECK_Y;
    constexpr int DECK_COUNT_Y = DECK_Y;
    constexpr int TOMB_COUNT_Y = TOMB_Y;
    constexpr int MOD_X = -120 + 2;
    constexpr int MOD_TURTLE_ICON_X = MOD_X + HUD_SMALL_ICON_HALF;
    constexpr int MOD_TEXT_X = MOD_X;
    constexpr int MOD_TEXT_X_WITH_TURTLE = MOD_X + HUD_SMALL_ICON_HALF * 2 + HUD_ROW_GAP;
    constexpr int MOD_POSITIVE_COLUMN_WIDTH = 22;
    constexpr int MOD_MULTIPLY_COLUMN_WIDTH = 20;
    constexpr int MOD_DRAW_COLUMN_WIDTH = 44;
    constexpr int MOD_SEGMENT_GAP = 6;
    constexpr int TRINKET_X = TOMB_X;
    constexpr int RAIL_Y0 = DECK_Y + HUD_ICON_HALF + HUD_ROW_GAP + HUD_ICON_HALF;
    constexpr int RAIL_ROW_SPACING = 6 + HUD_ROW_GAP;
    constexpr int TRINKET_ROW_SPACING = HUD_ICON_HALF * 2 + HUD_ROW_GAP;

    constexpr bn::fixed MOD_TEXT_SCALE = bn::fixed(3) / 4;
    constexpr bn::fixed MOD_EMPHASIZED_TEXT_SCALE = bn::fixed(1);

    const bn::sprite_affine_mat_ptr& mod_text_affine_mat(bn::fixed scale)
    {
        static bn::optional<bn::sprite_affine_mat_ptr> normal_mat;
        static bn::optional<bn::sprite_affine_mat_ptr> emphasized_mat;

        bn::optional<bn::sprite_affine_mat_ptr>& slot =
            scale == MOD_TEXT_SCALE ? normal_mat : emphasized_mat;

        if(!slot.has_value())
        {
            bn::affine_mat_attributes attrs;
            attrs.set_scale(scale);
            slot = bn::sprite_affine_mat_ptr::create(attrs);
        }

        return *slot;
    }

    bool modifier_is_empty(const RoundModifier& modifier)
    {
        return modifier.positive == 0 && modifier.multiply == 0 && modifier.draw_at_start == 0;
    }

    bool modifier_equals(const RoundModifier& a, const RoundModifier& b)
    {
        return a.positive == b.positive && a.multiply == b.multiply && a.draw_at_start == b.draw_at_start;
    }

    // GBA BPP_4 palettes hold exactly 16 colors — indices match ui_palette::shared().
    constexpr int COLOR_DECK = ui_palette::DECK;
    constexpr int COLOR_GRAVEYARD = ui_palette::GRAVEYARD;
    constexpr int COLOR_TRINKET_EMPTY = ui_palette::TRINKET_EMPTY;
    constexpr int COLOR_TRINKET_EQUIPPED = ui_palette::TRINKET_EQUIPPED;
    constexpr int COLOR_TRINKET_ECHO = ui_palette::ECHO_BADGE;
    constexpr int COLOR_TRINKET_LUCKY_SEVENS = ui_palette::TRINKET_LUCKY_SEVENS;
    constexpr int COLOR_TRINKET_GET_WITH_THE_TIMES = ui_palette::TRINKET_GET_WITH_THE_TIMES;
    constexpr int COLOR_TRINKET_PRIME_TIME = ui_palette::TRINKET_PRIME_TIME;
    constexpr int COLOR_TURTLE = ui_palette::TURTLE;

    uint32_t solid_tile_row(int color_index)
    {
        uint32_t row = 0;

        for(int px = 0; px < 8; ++px)
        {
            row |= uint32_t(color_index) << (px * 4);
        }

        return row;
    }

    const RoundModifier& future_mod_at(const GameState& state, int slot)
    {
        return state.future_mods[(state.next_mod_index + slot) % 3];
    }

    int keep_going_at(const GameState& state, int slot)
    {
        return state.keep_going_returns[(state.next_mod_index + slot) % 3];
    }

    bool mod_line_is_empty(const RoundModifier& modifier, int keep_going)
    {
        return modifier_is_empty(modifier) && keep_going <= 0;
    }

    constexpr int DETAILS_MOD_X = -72;
    constexpr int DETAILS_MOD_TURTLE_X = DETAILS_MOD_X + HUD_SMALL_ICON_HALF;
    constexpr int DETAILS_MOD_TEXT_X = DETAILS_MOD_X;
    constexpr int DETAILS_MOD_TEXT_X_WITH_TURTLE = DETAILS_MOD_X + HUD_SMALL_ICON_HALF * 2 + HUD_ROW_GAP;
    // Below the "Future / Next 3 rounds" header (was 10 — overlapped NN copy).
    constexpr int DETAILS_MOD_ROW_Y0 = 24;
    constexpr int DETAILS_MOD_ROW_Y0_NUMBER_NOW = 40;
    constexpr int DETAILS_MOD_ROW_SPACING = RAIL_ROW_SPACING + 2;
}

const bn::sprite_item* PersistentHud::HudIcon::sprite_item_for(IconKind kind)
{
    (void)kind;
    return nullptr;
}

int PersistentHud::HudIcon::solid_color_index(IconKind kind)
{
    switch(kind)
    {
    case IconKind::DECK_STACK:
        return COLOR_DECK;

    case IconKind::TOMBSTONE:
        return COLOR_GRAVEYARD;

    case IconKind::TRINKET_EMPTY:
        return COLOR_TRINKET_EMPTY;

    case IconKind::TRINKET_MOREL:
        return COLOR_TRINKET_EQUIPPED;

    case IconKind::TRINKET_ECHO:
        return COLOR_TRINKET_ECHO;

    case IconKind::TRINKET_LUCKY_SEVENS:
        return COLOR_TRINKET_LUCKY_SEVENS;

    case IconKind::TRINKET_GET_WITH_THE_TIMES:
        return COLOR_TRINKET_GET_WITH_THE_TIMES;

    case IconKind::TRINKET_PRIME_TIME:
        return COLOR_TRINKET_PRIME_TIME;

    case IconKind::TRINKET_FIBONACCI:
        return COLOR_TRINKET_EQUIPPED;

    case IconKind::TURTLE_TIMER:
        return COLOR_TURTLE;

    default:
        return COLOR_TRINKET_EMPTY;
    }
}

PersistentHud::HudIcon::HudIcon(IconKind kind, bool compact) :
    _kind(kind),
    _compact(compact),
    _tiles(bn::sprite_tiles_ptr::allocate(compact ? 1 : 4, bn::bpp_mode::BPP_4)),
    _palette(ui_palette::shared()),
    _sprite(bn::sprite_ptr::create(
        0, 0,
        compact ? bn::sprite_shape_size(8, 8) : bn::sprite_shape_size(16, 16),
        _tiles, _palette))
{
    _sprite.set_z_order(HUD_ICON_Z_ORDER);
    apply_kind(kind);
}

void PersistentHud::HudIcon::apply_kind(IconKind kind)
{
    _kind = kind;
    _sprite.set_tiles(_tiles);
    _sprite.set_palette(_palette);
    paint(kind);
}

void PersistentHud::HudIcon::set_kind(IconKind kind)
{
    if(kind == _kind)
    {
        return;
    }

    const bool visible = _sprite.visible();
    apply_kind(kind);
    _sprite.set_visible(visible);
}

void PersistentHud::HudIcon::set_position(int x, int y)
{
    _sprite.set_position(x, y);
}

void PersistentHud::HudIcon::set_visible(bool visible)
{
    _sprite.set_visible(visible);
}

void PersistentHud::HudIcon::paint(IconKind kind)
{
    auto vram = _tiles.vram();
    auto* tile_span = vram.get();

    if(!tile_span)
    {
        return;
    }

    const uint32_t row = solid_tile_row(solid_color_index(kind));

    for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
    {
        bn::tile& tile = (*tile_span)[tile_index];

        for(int py = 0; py < 8; ++py)
        {
            tile.data[py] = row;
        }
    }
}

PersistentHud::HudCount::HudCount(bn::sprite_text_generator& text_generator) :
    _text_generator(text_generator)
{
}

void PersistentHud::HudCount::redraw()
{
    _sprites.clear();

    if(_last_value < 0)
    {
        return;
    }

    _text_generator.set_center_alignment();
    _text_generator.generate(_x, _y, bn::to_string<8>(_last_value), _sprites);
    _text_generator.set_left_alignment();

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_z_order(HUD_TEXT_Z_ORDER);
        sprite.set_visible(_visible);
    }
}

void PersistentHud::HudCount::set_value(int value)
{
    if(value == _last_value)
    {
        return;
    }

    _last_value = value;
    redraw();
}

void PersistentHud::HudCount::set_position(int x, int y)
{
    if(_x == x && _y == y)
    {
        return;
    }

    _x = x;
    _y = y;

    if(_last_value >= 0)
    {
        redraw();
    }
}

void PersistentHud::HudCount::set_visible(bool visible)
{
    _visible = visible;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_visible(visible);
    }
}

PersistentHud::HudModLine::HudModLine(bn::sprite_text_generator& text_generator) :
    _text_generator(text_generator)
{
}

void PersistentHud::HudModLine::append_segment_at(int x, const bn::string_view& text, bn::fixed scale)
{
    if(text.empty())
    {
        return;
    }

    bn::vector<bn::sprite_ptr, 8> segment;
    _text_generator.set_left_alignment();
    _text_generator.generate(x, _y, text, segment);

    const bn::sprite_affine_mat_ptr& scale_mat = mod_text_affine_mat(scale);

    for(bn::sprite_ptr& sprite : segment)
    {
        sprite.set_z_order(HUD_TEXT_Z_ORDER);
        sprite.set_visible(_visible);
        sprite.set_affine_mat(scale_mat);
        _sprites.push_back(bn::move(sprite));
    }
}

void PersistentHud::HudModLine::redraw()
{
    _sprites.clear();

    if(_round_prefix.empty() && mod_line_is_empty(_last_modifier, _keep_going))
    {
        return;
    }

    int draw_x = _x;

    if(!_round_prefix.empty())
    {
        append_segment_at(_x, _round_prefix, MOD_TEXT_SCALE);
        draw_x = _x + MOD_POSITIVE_COLUMN_WIDTH + MOD_SEGMENT_GAP;
    }

    if(mod_line_is_empty(_last_modifier, _keep_going))
    {
        return;
    }

    bn::string<16> segment;
    int mul_x = draw_x;

    if(_last_modifier.positive)
    {
        segment = "+";
        segment.append(bn::to_string<8>(_last_modifier.positive));
        append_segment_at(draw_x, segment, MOD_TEXT_SCALE);
        mul_x = draw_x + MOD_POSITIVE_COLUMN_WIDTH + MOD_SEGMENT_GAP;
        draw_x = mul_x;
    }

    if(_last_modifier.multiply)
    {
        segment = "x";
        segment.append(bn::to_string<8>(_last_modifier.multiply));
        append_segment_at(mul_x, segment, MOD_EMPHASIZED_TEXT_SCALE);
        draw_x = mul_x + MOD_MULTIPLY_COLUMN_WIDTH + MOD_SEGMENT_GAP;
    }

    if(_last_modifier.draw_at_start)
    {
        segment = "draw ";
        segment.append(bn::to_string<8>(_last_modifier.draw_at_start));
        append_segment_at(draw_x, segment, MOD_EMPHASIZED_TEXT_SCALE);
        draw_x += MOD_DRAW_COLUMN_WIDTH + MOD_SEGMENT_GAP;
    }

    if(_keep_going > 0)
    {
        segment = "keep";

        if(_keep_going > 1)
        {
            segment.append(" ");
            segment.append(bn::to_string<4>(_keep_going));
        }

        append_segment_at(draw_x, segment, MOD_EMPHASIZED_TEXT_SCALE);
    }
}

void PersistentHud::HudModLine::set_round_prefix(const bn::string_view& prefix)
{
    if(prefix == _round_prefix)
    {
        return;
    }

    _round_prefix = prefix;
    redraw();
}

void PersistentHud::HudModLine::clear_round_prefix()
{
    if(_round_prefix.empty())
    {
        return;
    }

    _round_prefix.clear();
    redraw();
}

void PersistentHud::HudModLine::set_modifier(const RoundModifier& modifier)
{
    if(_has_modifier && modifier_equals(_last_modifier, modifier))
    {
        return;
    }

    _last_modifier = modifier;
    _has_modifier = !modifier_is_empty(modifier);
    redraw();
}

void PersistentHud::HudModLine::set_keep_going(int returns)
{
    if(returns == _keep_going)
    {
        return;
    }

    _keep_going = returns;
    redraw();
}

void PersistentHud::HudModLine::set_position(int x, int y)
{
    if(_x == x && _y == y)
    {
        return;
    }

    _x = x;
    _y = y;

    if(_has_modifier || _keep_going > 0 || !_round_prefix.empty())
    {
        redraw();
    }
}

void PersistentHud::HudModLine::set_visible(bool visible)
{
    _visible = visible;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_visible(visible);
    }
}

PersistentHud::PersistentHud(bn::sprite_text_generator& count_text_generator,
                             bn::sprite_text_generator& mod_text_generator) :
    _deck_icon(IconKind::DECK_STACK),
    _tomb_icon(IconKind::TOMBSTONE),
    _trinket_icons{
        HudIcon(IconKind::TRINKET_MOREL),
        HudIcon(IconKind::TRINKET_LUCKY_SEVENS),
        HudIcon(IconKind::TRINKET_PRIME_TIME),
    },
    _deck_count(count_text_generator),
    _grave_count(count_text_generator),
    _future_mod_lines{
        HudModLine(mod_text_generator),
        HudModLine(mod_text_generator),
        HudModLine(mod_text_generator),
    },
    _turtle_row_icons{
        HudIcon(IconKind::TURTLE_TIMER, true),
        HudIcon(IconKind::TURTLE_TIMER, true),
        HudIcon(IconKind::TURTLE_TIMER, true),
    },
    _details_mod_lines{
        HudModLine(mod_text_generator),
        HudModLine(mod_text_generator),
        HudModLine(mod_text_generator),
    },
    _details_turtle_icons{
        HudIcon(IconKind::TURTLE_TIMER, true),
        HudIcon(IconKind::TURTLE_TIMER, true),
        HudIcon(IconKind::TURTLE_TIMER, true),
    }
{
    _deck_icon.set_position(DECK_X, DECK_Y);
    _tomb_icon.set_position(TOMB_X, TOMB_Y);
    _deck_count.set_position(DECK_X, DECK_COUNT_Y);
    _grave_count.set_position(TOMB_X, TOMB_COUNT_Y);

    for(int slot = 0; slot < 3; ++slot)
    {
        const int mod_row_y = RAIL_Y0 + slot * RAIL_ROW_SPACING;
        const int trinket_row_y = RAIL_Y0 + slot * TRINKET_ROW_SPACING;
        _future_mod_lines[slot].set_position(MOD_TEXT_X, mod_row_y);
        _turtle_row_icons[slot].set_position(MOD_TURTLE_ICON_X, mod_row_y);
        _turtle_row_icons[slot].set_visible(false);
        _trinket_icons[slot].set_position(TRINKET_X, trinket_row_y);
        _trinket_base_x[slot] = TRINKET_X;
        _trinket_base_y[slot] = trinket_row_y;
        _details_mod_lines[slot].set_visible(false);
        _details_turtle_icons[slot].set_visible(false);
    }
}

void PersistentHud::update_modifier_rows(const GameState& state,
                                         bn::array<HudModLine, 3>& lines,
                                         bn::array<HudIcon, 3>& turtles,
                                         int origin_x,
                                         int turtle_x,
                                         int text_x,
                                         int text_x_with_turtle,
                                         int row_y0,
                                         int row_spacing,
                                         bool visible,
                                         CampaignMode campaign_mode,
                                         int number_now_scoring_round)
{
    for(int slot = 0; slot < 3; ++slot)
    {
        const int row_y = row_y0 + slot * row_spacing;
        const bool show_turtle = state.turtle_rounds_remaining > slot;

        turtles[slot].set_position(origin_x + turtle_x, row_y);
        turtles[slot].set_visible(visible && show_turtle);
        lines[slot].set_position(origin_x + (show_turtle ? text_x_with_turtle : text_x), row_y);
        lines[slot].set_modifier(future_mod_at(state, slot));
        lines[slot].set_keep_going(keep_going_at(state, slot));

        if(campaign_mode == CampaignMode::NUMBER_NOW && number_now_scoring_round > 0)
        {
            const int future_round = state.current_round + slot + 1;
            bn::string<8> prefix = "R";
            prefix.append(bn::to_string<4>(future_round));

            if(future_round == number_now_scoring_round)
            {
                prefix.append("* ");
            }
            else
            {
                prefix.append("x0 ");
            }

            lines[slot].set_round_prefix(prefix);
        }
        else
        {
            lines[slot].clear_round_prefix();
        }

        lines[slot].set_visible(visible);
    }
}

void PersistentHud::update(const GameState& state)
{
    _deck_count.set_value(state.deck.remaining());
    _grave_count.set_value(state.graveyard.size());

    for(int slot = 0; slot < 3; ++slot)
    {
        IconKind kind = IconKind::TRINKET_EMPTY;

        switch(state.trinkets[slot])
        {
        case TrinketType::MOREL:
            kind = IconKind::TRINKET_MOREL;
            break;

        case TrinketType::ECHO:
            kind = IconKind::TRINKET_ECHO;
            break;

        case TrinketType::LUCKY_SEVENS:
            kind = IconKind::TRINKET_LUCKY_SEVENS;
            break;

        case TrinketType::GET_WITH_THE_TIMES:
            kind = IconKind::TRINKET_GET_WITH_THE_TIMES;
            break;

        case TrinketType::PRIME_TIME:
            kind = IconKind::TRINKET_PRIME_TIME;
            break;

        case TrinketType::FIBONACCI:
            kind = IconKind::TRINKET_FIBONACCI;
            break;

        default:
            break;
        }

        _trinket_icons[slot].set_kind(kind);
        _trinket_base_y[slot] = RAIL_Y0 + slot * TRINKET_ROW_SPACING;
    }

    update_modifier_rows(state,
                         _future_mod_lines,
                         _turtle_row_icons,
                         0,
                         MOD_TURTLE_ICON_X,
                         MOD_TEXT_X,
                         MOD_TEXT_X_WITH_TURTLE,
                         RAIL_Y0,
                         RAIL_ROW_SPACING,
                         _visible);
}

void PersistentHud::sync_details_modifiers(const GameState& state, int panel_x, bool visible,
                                           CampaignMode campaign_mode, int number_now_scoring_round)
{
    const int row_y0 = campaign_mode == CampaignMode::NUMBER_NOW ? DETAILS_MOD_ROW_Y0_NUMBER_NOW
                                                                : DETAILS_MOD_ROW_Y0;

    update_modifier_rows(state,
                         _details_mod_lines,
                         _details_turtle_icons,
                         panel_x,
                         DETAILS_MOD_TURTLE_X,
                         DETAILS_MOD_TEXT_X,
                         DETAILS_MOD_TEXT_X_WITH_TURTLE,
                         row_y0,
                         DETAILS_MOD_ROW_SPACING,
                         visible,
                         campaign_mode,
                         number_now_scoring_round);
}

void PersistentHud::set_visible(bool visible)
{
    _visible = visible;
    _deck_icon.set_visible(visible);
    _tomb_icon.set_visible(visible);
    _deck_count.set_visible(visible);
    _grave_count.set_visible(visible);

    for(HudIcon& icon : _trinket_icons)
    {
        icon.set_visible(visible);
    }

    for(HudModLine& line : _future_mod_lines)
    {
        line.set_visible(visible);
    }
}

void PersistentHud::start_trinket_wiggle(int slot)
{
    if(slot < 0 || slot >= _trinket_wiggle_frames.size())
    {
        return;
    }

    _trinket_wiggle_frames[slot] = TRINKET_WIGGLE_FRAMES;
}

bool PersistentHud::trinket_slot_position(int slot, int& out_x, int& out_y) const
{
    if(slot < 0 || slot >= _trinket_base_x.size())
    {
        return false;
    }

    out_x = _trinket_base_x[slot];
    out_y = _trinket_base_y[slot];
    return true;
}

void PersistentHud::tick_trinket_wiggles()
{
    for(int slot = 0; slot < _trinket_wiggle_frames.size(); ++slot)
    {
        if(_trinket_wiggle_frames[slot] <= 0)
        {
            _trinket_icons[slot].set_position(_trinket_base_x[slot], _trinket_base_y[slot]);
            continue;
        }

        const int frame = TRINKET_WIGGLE_FRAMES - _trinket_wiggle_frames[slot];
        const int wiggle_x = (bn::sin(bn::fixed(frame) / 8) * 2).integer();
        const int wiggle_y = (bn::cos(bn::fixed(frame) / 10) * 1).integer();
        _trinket_icons[slot].set_position(_trinket_base_x[slot] + wiggle_x, _trinket_base_y[slot] + wiggle_y);
        --_trinket_wiggle_frames[slot];
    }
}
