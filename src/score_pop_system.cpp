#include "score_pop_system.h"

#include "bn_affine_mat_attributes.h"
#include "bn_array.h"
#include "bn_blending.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_math.h"
#include "bn_span.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_string.h"

#include "game_context.h"
#include "game_state.h"
#include "game_types.h"
#include "score_count_system.h"
#include "trinket_system.h"

namespace
{
    constexpr int ROUND_SCORE_X = 0;
    constexpr int ROUND_SCORE_Y = 0;
    constexpr int TOTAL_SCORE_Y = -48;
    constexpr int CARD_POP_OFFSET_X = 14;
    constexpr int CARD_POP_OFFSET_Y = 0;
    constexpr int CARD_POP_RIGHT_PAD = 12;
    constexpr int TRINKET_POP_OFFSET_X = -18;
    constexpr bn::fixed FLOAT_POP_SCALE = bn::fixed(3) / 2;
    constexpr bn::color POP_GOLD(31, 25, 5);
    constexpr int TRINKET_POP_OFFSET_Y = 0;
    constexpr int FLIGHT_TARGET_OFFSET_X = 18;
    constexpr int FLIGHT_TARGET_OFFSET_Y = -8;

    bn::string<8> format_score_pop(const ScorePopRequest& request)
    {
        bn::string<8> text;

        if(request.is_multiply)
        {
            text = "x";
        }
        else
        {
            text = "+";
        }

        text.append(bn::to_string<6>(request.amount));
        return text;
    }

    bn::fixed ease_in_out(bn::fixed t)
    {
        if(t < bn::fixed(1) / 2)
        {
            return bn::fixed(2) * t * t;
        }

        const bn::fixed u = bn::fixed(1) - t;
        return bn::fixed(1) - bn::fixed(2) * u * u;
    }

    int pop_total_frames(const ScorePop& pop)
    {
        if(pop.motion == ScorePopMotion::TRINKET_FLIGHT)
        {
            return SCORE_POP_TRINKET_HOLD_FRAMES + SCORE_POP_TRINKET_FLY_FRAMES;
        }

        return SCORE_POP_FRAMES;
    }

    bool float_pop_visible(const ScorePop& pop)
    {
        if(pop.frame >= SCORE_POP_FRAMES)
        {
            return false;
        }

        if(pop.frame >= SCORE_POP_FADE_START)
        {
            return (pop.frame % 2) == 0;
        }

        return true;
    }

    int float_pop_rise(int frame)
    {
        return frame * SCORE_POP_RISE / SCORE_POP_FRAMES;
    }

    void apply_pop_anchor(ScorePop& pop, int anchor_x, int anchor_y, bn::fixed scale, bn::fixed alpha,
                          bool visible)
    {
        if(pop.affine_mat.has_value())
        {
            bn::affine_mat_attributes attrs;
            attrs.set_scale(scale < bn::fixed(0.25) ? bn::fixed(0.25) : scale);
            pop.affine_mat->set_attributes(attrs);
        }

        if(alpha < bn::fixed(1))
        {
            bn::blending::set_transparency_alpha(alpha < bn::fixed(0.05) ? bn::fixed(0.05) : alpha);
        }

        for(int sprite_index = 0; sprite_index < pop.sprites.size(); ++sprite_index)
        {
            bn::sprite_ptr& sprite = pop.sprites[sprite_index];
            sprite.set_position(anchor_x + pop.glyph_offset_x[sprite_index],
                                anchor_y + pop.glyph_offset_y[sprite_index]);
            sprite.set_visible(visible);
            sprite.set_z_order(game_layout::SCORE_POP_Z);
            sprite.set_bg_priority(game_layout::SCORE_POP_BG_PRIORITY);
            sprite.put_above();

            if(pop.affine_mat.has_value())
            {
                sprite.set_affine_mat(*pop.affine_mat);
            }

            sprite.set_blending_enabled(alpha < bn::fixed(1));
        }
    }

    void update_float_pop(GameContext& ctx, ScorePop& pop)
    {
        const int rise = float_pop_rise(pop.frame);
        const bool visible = float_pop_visible(pop);
        const int panel_x = ctx.main_panel_offset_x();
        apply_pop_anchor(pop, pop.from_x + panel_x, pop.from_y - rise, FLOAT_POP_SCALE, 1, visible);
    }

    void update_trinket_flight_pop(GameContext& ctx, ScorePop& pop)
    {
        const int panel_x = ctx.main_panel_offset_x();
        const int hold = SCORE_POP_TRINKET_HOLD_FRAMES;
        const int fly = SCORE_POP_TRINKET_FLY_FRAMES;

        int anchor_x = pop.from_x + panel_x;
        int anchor_y = pop.from_y;
        bn::fixed scale = 1;
        bn::fixed alpha = 1;

        if(pop.frame <= hold)
        {
            // Brief settle beside the trinket — slight grow for emphasis.
            const bn::fixed t = bn::fixed(pop.frame) / bn::fixed(hold);
            scale = bn::fixed(1) + t * bn::fixed(2) / 10;
        }
        else
        {
            const int fly_frame = pop.frame - hold;
            const bn::fixed t = ease_in_out(bn::fixed(fly_frame) / bn::fixed(fly));
            anchor_x = pop.from_x + ((pop.to_x - pop.from_x) * t).integer() + panel_x;
            anchor_y = pop.from_y + ((pop.to_y - pop.from_y) * t).integer();
            scale = bn::fixed(12) / 10 - t * bn::fixed(4) / 10;
            alpha = bn::fixed(1) - t * bn::fixed(35) / 100;
        }

        apply_pop_anchor(pop, anchor_x, anchor_y, scale, alpha, true);
    }

    void update_pop_sprites(GameContext& ctx, ScorePop& pop)
    {
        if(pop.motion == ScorePopMotion::TRINKET_FLIGHT)
        {
            update_trinket_flight_pop(ctx, pop);
        }
        else
        {
            update_float_pop(ctx, pop);
        }
    }

    void handle_trinket_flight_arrival(GameContext& ctx, ScorePop& pop)
    {
        if(pop.arrived)
        {
            return;
        }

        pop.arrived = true;

        if(pop.apply_on_arrive)
        {
            if(pop.field == TrinketScoreField::ROUND)
            {
                const int before = ctx.state.round.running;
                ctx.state.round.add(pop.amount);
                score_count_queue(ctx.state, TrinketScoreField::ROUND, before, ctx.state.round.running);
                trinket_queue_score_check(ctx.state, TrinketScoreField::ROUND, before, ctx.state.round.running,
                                          true, pop.amount);
                ctx.draw_round_score();
            }
            else
            {
                const int before = ctx.state.total_score;
                ctx.state.total_score += pop.amount;
                score_count_queue(ctx.state, TrinketScoreField::TOTAL, before, ctx.state.total_score);
                trinket_queue_score_check(ctx.state, TrinketScoreField::TOTAL, before, ctx.state.total_score,
                                          true, pop.amount);
                ctx.draw_total_score();
            }
        }
        else if(pop.defer_score_count)
        {
            score_count_queue(ctx.state, pop.field, pop.count_before, pop.count_after);

            if(pop.field == TrinketScoreField::ROUND)
            {
                ctx.draw_round_score();
            }
            else
            {
                ctx.draw_total_score();
            }
        }
    }

    void capture_glyph_offsets(ScorePop& pop, int anchor_x, int anchor_y)
    {
        for(const bn::sprite_ptr& sprite : pop.sprites)
        {
            if(pop.glyph_offset_x.full() || pop.glyph_offset_y.full())
            {
                break;
            }

            pop.glyph_offset_x.push_back(sprite.x() - anchor_x);
            pop.glyph_offset_y.push_back(sprite.y() - anchor_y);
        }
    }

    const bn::sprite_palette_ptr& pop_gold_palette(const bn::sprite_ptr& sample)
    {
        static bn::optional<bn::sprite_palette_ptr> palette;

        if(!palette.has_value())
        {
            bn::array<bn::color, 16> colors;
            const bn::span<const bn::color> source = sample.palette().colors();

            for(int index = 0; index < 16; ++index)
            {
                colors[index] = index < source.size() ? source[index] : bn::color();
            }

            for(int index = 1; index < 16; ++index)
            {
                if(colors[index].red() + colors[index].green() + colors[index].blue() > 24)
                {
                    colors[index] = POP_GOLD;
                }
            }

            const bn::sprite_palette_item item(
                bn::span<const bn::color>(colors.data(), colors.size()), bn::bpp_mode::BPP_4);
            palette = bn::sprite_palette_ptr::create(item);
        }

        return *palette;
    }

    void tint_pop_sprites(ScorePop& pop)
    {
        if(pop.sprites.empty())
        {
            return;
        }

        const bn::sprite_palette_ptr& palette = pop_gold_palette(pop.sprites[0]);

        for(bn::sprite_ptr& sprite : pop.sprites)
        {
            sprite.set_palette(palette);
        }
    }

    int score_cluster_right_x(const GameContext& ctx)
    {
        int right = CARD_POP_OFFSET_X;

        for(const bn::sprite_ptr& sprite : ctx.round_text_sprites)
        {
            const int edge = sprite.x().integer() + 8;
            if(edge > right)
            {
                right = edge;
            }
        }

        for(const bn::sprite_ptr& sprite : ctx.text_sprites)
        {
            const int edge = sprite.x().integer() + 16;
            if(edge > right)
            {
                right = edge;
            }
        }

        return right + CARD_POP_RIGHT_PAD;
    }

    bool resolve_trinket_anchor(GameContext& ctx, TrinketType trinket, int& out_x, int& out_y)
    {
        const int slot = trinket_slot_index(ctx.state, trinket);

        if(slot < 0 || !ctx.hud.trinket_slot_position(slot, out_x, out_y))
        {
            out_x = ROUND_SCORE_X + CARD_POP_OFFSET_X;
            out_y = ROUND_SCORE_Y + CARD_POP_OFFSET_Y;
            return false;
        }

        out_x += TRINKET_POP_OFFSET_X;
        out_y += TRINKET_POP_OFFSET_Y;
        return true;
    }
}

void score_pop_queue(GameState& state, int amount, bool is_morel, TrinketScoreField field, bool is_multiply,
                     bool allow_zero)
{
    if(amount < 0)
    {
        return;
    }

    if(is_multiply && amount <= 1)
    {
        return;
    }

    if(!is_multiply && amount == 0 && !allow_zero)
    {
        return;
    }

    ScorePopRequest request;
    request.amount = amount;
    request.is_morel = is_morel;
    request.is_multiply = is_multiply;
    request.field = field;

    // Legacy morel flag → trinket flight (callers should prefer score_pop_queue_from_trinket).
    if(is_morel)
    {
        request.from_trinket = TrinketType::MOREL;
    }

    if(state.pending_score_pops.full())
    {
        state.pending_score_pops.erase(state.pending_score_pops.begin());
    }

    state.pending_score_pops.push_back(request);
}

void score_pop_queue_from_trinket(GameState& state, int amount, TrinketType trinket,
                                  TrinketScoreField field, int count_before, int count_after,
                                  bool defer_score_count, bool apply_on_arrive)
{
    if(amount <= 0 || trinket == TrinketType::NONE)
    {
        return;
    }

    ScorePopRequest request;
    request.amount = amount;
    request.field = field;
    request.from_trinket = trinket;
    request.defer_score_count = defer_score_count;
    request.count_before = count_before;
    request.count_after = count_after;
    request.apply_on_arrive = apply_on_arrive;
    request.is_morel = trinket == TrinketType::MOREL;

    if(state.pending_score_pops.full())
    {
        state.pending_score_pops.erase(state.pending_score_pops.begin());
    }

    state.pending_score_pops.push_back(request);
}

void score_pop_process_pending(GameContext& ctx)
{
    for(const ScorePopRequest& request : ctx.state.pending_score_pops)
    {
        while(ctx.score_pops.full())
        {
            ctx.score_pops.erase(ctx.score_pops.begin());
        }

        ScorePop pop;
        pop.amount = request.amount;
        pop.field = request.field;
        pop.defer_score_count = request.defer_score_count;
        pop.count_before = request.count_before;
        pop.count_after = request.count_after;
        pop.apply_on_arrive = request.apply_on_arrive;

        const int score_y = request.field == TrinketScoreField::ROUND ? ROUND_SCORE_Y : TOTAL_SCORE_Y;

        if(request.from_trinket != TrinketType::NONE)
        {
            pop.motion = ScorePopMotion::TRINKET_FLIGHT;
            resolve_trinket_anchor(ctx, request.from_trinket, pop.from_x, pop.from_y);
            pop.to_x = ROUND_SCORE_X + FLIGHT_TARGET_OFFSET_X;
            pop.to_y = score_y + FLIGHT_TARGET_OFFSET_Y;
            pop.affine_mat = bn::sprite_affine_mat_ptr::create();
        }
        else
        {
            int stagger_slot = 0;

            for(const ScorePop& existing : ctx.score_pops)
            {
                if(existing.field == request.field && existing.motion == ScorePopMotion::FLOAT_RISE)
                {
                    ++stagger_slot;
                }
            }

            const int stagger_offset = stagger_slot == 0
                ? 0
                : ((stagger_slot % 3) - 1) * SCORE_POP_STAGGER_SPACING;

            pop.motion = ScorePopMotion::FLOAT_RISE;
            pop.from_x = score_cluster_right_x(ctx) + stagger_offset;
            pop.from_y = score_y + CARD_POP_OFFSET_Y;
            pop.to_x = pop.from_x;
            pop.to_y = pop.from_y;
            pop.affine_mat = bn::sprite_affine_mat_ptr::create();
        }

        const int old_z = ctx.round_text_generator.z_order();
        const int old_bg = ctx.round_text_generator.bg_priority();
        const bool old_one_sprite = ctx.round_text_generator.one_sprite_per_character();

        ctx.round_text_generator.set_z_order(game_layout::SCORE_POP_Z);
        ctx.round_text_generator.set_bg_priority(game_layout::SCORE_POP_BG_PRIORITY);
        ctx.round_text_generator.set_one_sprite_per_character(true);
        ctx.round_text_generator.set_center_alignment();
        ctx.round_text_generator.generate_optional(pop.from_x, pop.from_y, format_score_pop(request),
                                                   pop.sprites);
        ctx.round_text_generator.set_left_alignment();
        ctx.round_text_generator.set_z_order(old_z);
        ctx.round_text_generator.set_bg_priority(old_bg);
        ctx.round_text_generator.set_one_sprite_per_character(old_one_sprite);

        if(pop.sprites.empty())
        {
            continue;
        }

        tint_pop_sprites(pop);
        capture_glyph_offsets(pop, pop.from_x, pop.from_y);
        update_pop_sprites(ctx, pop);
        ctx.score_pops.push_back(pop);
    }

    ctx.state.pending_score_pops.clear();
}

void score_pop_tick(GameContext& ctx)
{
    for(int index = 0; index < ctx.score_pops.size(); )
    {
        ScorePop& pop = ctx.score_pops[index];
        ++pop.frame;

        if(pop.motion == ScorePopMotion::TRINKET_FLIGHT &&
           pop.frame == SCORE_POP_TRINKET_HOLD_FRAMES + SCORE_POP_TRINKET_FLY_FRAMES)
        {
            handle_trinket_flight_arrival(ctx, pop);
        }

        if(pop.frame >= pop_total_frames(pop))
        {
            if(pop.motion == ScorePopMotion::TRINKET_FLIGHT)
            {
                handle_trinket_flight_arrival(ctx, pop);
            }

            ctx.score_pops.erase(ctx.score_pops.begin() + index);
            continue;
        }

        ++index;
    }
}

void score_pop_sync_positions(GameContext& ctx)
{
    for(ScorePop& pop : ctx.score_pops)
    {
        update_pop_sprites(ctx, pop);
    }
}

void score_pop_render(GameContext& ctx, bool visible)
{
    if(!visible)
    {
        for(ScorePop& pop : ctx.score_pops)
        {
            for(bn::sprite_ptr& sprite : pop.sprites)
            {
                sprite.set_visible(false);
            }
        }
    }
}

bool score_pop_blocks_score_finalize(const GameContext& ctx, TrinketScoreField field)
{
    for(const ScorePop& pop : ctx.score_pops)
    {
        if(pop.field == field &&
           (pop.defer_score_count || pop.apply_on_arrive) &&
           !pop.arrived)
        {
            return true;
        }
    }

    return false;
}
