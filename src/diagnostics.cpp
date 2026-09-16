#include "diagnostics.h"

#include "bn_assert.h"
#include "bn_log.h"
#include "bn_sprite_palettes.h"
#include "bn_sprite_tiles.h"
#include "bn_sprites.h"

#include "game_context.h"

namespace
{
    diag::ResourceSample g_peak;

    // Fraction of a pool that counts as "worth logging". Below this the numbers
    // are noise; above it, the next unlucky frame is the one that fails.
    constexpr int WARN_NUMERATOR = 3;
    constexpr int WARN_DENOMINATOR = 4;

    bool worth_reporting(int value, int capacity)
    {
        return value * WARN_DENOMINATOR >= capacity * WARN_NUMERATOR;
    }
}

diag::ResourceSample diag::sample_resources()
{
    ResourceSample sample;
    sample.sprite_items = bn::sprites::used_items_count();
    sample.sprite_tiles = bn::sprite_tiles::used_tiles_count();
    sample.sprite_colors = bn::sprite_palettes::used_colors_count();
    return sample;
}

diag::ResourceSample diag::resource_capacity()
{
    ResourceSample capacity;
    capacity.sprite_items = bn::sprites::used_items_count() + bn::sprites::available_items_count();
    capacity.sprite_tiles = bn::sprite_tiles::used_tiles_count() + bn::sprite_tiles::available_tiles_count();
    capacity.sprite_colors = bn::sprite_palettes::used_colors_count() +
                             bn::sprite_palettes::available_colors_count();
    return capacity;
}

const diag::ResourceSample& diag::resource_peak()
{
    return g_peak;
}

void diag::reset_resource_peak()
{
    g_peak = ResourceSample();
}

void diag::track_resource_peak(const char* context)
{
    const ResourceSample sample = sample_resources();
    const ResourceSample capacity = resource_capacity();

    // Sprite items are measured against the hardware OBJ count rather than the
    // much larger software pool. Created-but-hidden sprites still count here,
    // so this is an upper bound on what will be committed to OAM -- staying
    // under it guarantees the "Too many on screen sprites" assert cannot fire.
    if(sample.sprite_items > g_peak.sprite_items)
    {
        const bool over_hardware = sample.sprite_items > HARDWARE_SPRITE_LIMIT;

        if(over_hardware || worth_reporting(sample.sprite_items, HARDWARE_SPRITE_LIMIT))
        {
            BN_LOG("[res] ", context, " sprite items ", sample.sprite_items, "/", capacity.sprite_items,
                   over_hardware ? " OVER 128 HARDWARE OBJ" : " (128 hardware OBJ)");
        }

        g_peak.sprite_items = sample.sprite_items;
    }

    if(sample.sprite_tiles > g_peak.sprite_tiles)
    {
        if(worth_reporting(sample.sprite_tiles, capacity.sprite_tiles))
        {
            BN_LOG("[res] ", context, " sprite tiles ", sample.sprite_tiles, "/", capacity.sprite_tiles);
        }

        g_peak.sprite_tiles = sample.sprite_tiles;
    }

    if(sample.sprite_colors > g_peak.sprite_colors)
    {
        if(worth_reporting(sample.sprite_colors, capacity.sprite_colors))
        {
            BN_LOG("[res] ", context, " palette colors ", sample.sprite_colors, "/", capacity.sprite_colors);
        }

        g_peak.sprite_colors = sample.sprite_colors;
    }
}

void diag::ProgressWatchdog::reset()
{
    _fingerprint = 0;
    _stalled_frames = 0;
    _primed = false;
}

void diag::ProgressWatchdog::tick(unsigned fingerprint, const char* stall_reason)
{
    if(!_primed || fingerprint != _fingerprint)
    {
        _fingerprint = fingerprint;
        _stalled_frames = 0;
        _primed = true;
        return;
    }

    if(!stall_reason)
    {
        // Idle by choice: the game is waiting for the player, which is allowed
        // to last forever.
        _stalled_frames = 0;
        return;
    }

    ++_stalled_frames;

    if(_stalled_frames < STALL_REPORT_FRAMES)
    {
        return;
    }

    const ResourceSample& peak = resource_peak();

    // Deliberately fatal. A stall is already an unrecoverable state for the
    // player; failing loudly names the subsystem instead of leaving a frozen
    // frame that is indistinguishable from a crash.
    BN_ERROR("Progress stalled ", _stalled_frames, " frames\nblocked by: ", stall_reason,
             "\npeak items/tiles/colors: ", peak.sprite_items, "/", peak.sprite_tiles, "/",
             peak.sprite_colors);
}

unsigned diag::battle_fingerprint(const GameContext& ctx)
{
    const GameState& state = ctx.state;
    FingerprintBuilder builder;

    builder.add(int(ctx.mode));
    builder.add(ctx.selected_card);
    builder.add(ctx.browse_cursor);
    builder.add(state.hand.size());
    builder.add(state.graveyard.size());
    builder.add(state.pending_actions.size());
    builder.add(state.total_score);
    builder.add(state.round.running);
    builder.add(state.current_round);
    builder.add(state.effect_draw_remaining);
    builder.add(int(state.selection.type));
    builder.add(state.selection.remaining_picks);
    builder.add(state.pending_hand_draws.size());
    builder.add(state.pending_score_pops.size());
    builder.add(state.pending_score_counts.size());
    builder.add(state.pending_score_checks.size());
    builder.add(state.deferred_morel_count);
    builder.add(ctx.score_pops.size());
    builder.add(ctx.pending_lucky_sevens.size());
    builder.add(ctx.pending_cycle_draws);
    builder.add(ctx.opening_draw_attempts_remaining);
    builder.add(ctx.deck.remaining());
    builder.add(state.echo_pending_replay);
    builder.add(state.swivel_waiting);
    builder.add(ctx.round_end_pending);
    builder.add(ctx.run_finished);
    builder.add(int(ctx.combo_focus));
    builder.add(int(ctx.panel_transition));

    // Phase, not frame. A wedged flight keeps counting frames forever, so the
    // frame counter would disguise the exact stall this is meant to catch.
    for(const PlayFlight& flight : ctx.play_flights)
    {
        builder.add(flight.active);
        builder.add(int(flight.phase));
        builder.add(flight.play_resolved);
        builder.add(flight.hand_committed);
    }

    for(const TransitFlight& flight : ctx.transit_flights)
    {
        builder.add(flight.active);
        builder.add(int(flight.kind));
        builder.add(flight.state_applied);
    }

    return builder.value();
}

const char* diag::battle_stall_reason(const GameContext& ctx)
{
    // Only states that suppress player input are listed. While input is still
    // accepted the player can always break the tie, so no amount of waiting is
    // evidence of a bug -- reporting those would fire on anyone who sets the
    // controller down. Everything below must clear on its own within about a
    // second, making STALL_REPORT_FRAMES a wide margin.
    if(ctx.lucky_sevens_fx.active)
    {
        return "lucky_sevens_fx";
    }

    if(!ctx.pending_lucky_sevens.empty())
    {
        return "pending_lucky_sevens";
    }

    if(ctx.deck_search_resolve_active())
    {
        return "deck_search_resolve_fx";
    }

    if(ctx.y2k_bust_modal_active)
    {
        return "y2k_bust_modal";
    }

    if(ctx.state.deferred_morel_count > 0)
    {
        return "deferred_morel";
    }

    if(ctx.play_flight_count() > 0)
    {
        return "play_flight";
    }

    if(ctx.removing_card)
    {
        return "removing_card";
    }

    if(ctx.hand_draw_fx_blocking())
    {
        return "hand_draw_fx";
    }

    if(ctx.zone_transit_active())
    {
        return "zone_transit";
    }

    if(ctx.graveyard_card_fx_active)
    {
        return "graveyard_card_fx";
    }

    if(ctx.swapping_card)
    {
        return "swapping_card";
    }

    if(ctx.state.effect_draw_remaining > 0)
    {
        return "effect_draw";
    }

    return nullptr;
}
