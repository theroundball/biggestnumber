#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

// Runtime instrumentation for the two failure modes that look identical to the
// player -- a frozen frame -- but have nothing else in common:
//
//   1. Butano resource exhaustion (OBJ VRAM, palette colors, sprite items).
//      Fatal and self-announcing via BN_ERROR, but only once it is too late to
//      see what the game was doing. Tracked here so pressure shows up in the
//      emulator log while there is still headroom.
//
//   2. Logical deadlock. The main loop keeps running at 60fps, input is ignored
//      and no state advances. Silent, and indistinguishable from a crash unless
//      something is watching for it.

class GameContext;

namespace diag
{
    // Live occupancy of the Butano pools that every sprite in the game shares.
    struct ResourceSample
    {
        int sprite_items = 0;
        int sprite_tiles = 0;
        int sprite_colors = 0;
    };

    [[nodiscard]] ResourceSample sample_resources();

    // Pool sizes, queried rather than hardcoded: sprite_items follows
    // BN_CFG_SPRITES_MAX_ITEMS, the other two are fixed by the hardware.
    [[nodiscard]] ResourceSample resource_capacity();

    [[nodiscard]] const ResourceSample& resource_peak();

    void reset_resource_peak();

    // Samples the pools and logs any new high-water mark that is close enough to
    // a limit to matter. Cheap enough to call every frame.
    //
    // `context` labels the log line with the scene or phase being measured.
    void track_resource_peak(const char* context);

    // Number of hardware OBJ slots. Butano asserts past this, and the bounds
    // check that produces that assert is itself compiled out when asserts are
    // disabled, so exceeding it must be treated as fatal either way.
    constexpr int HARDWARE_SPRITE_LIMIT = 128;

    // Frames of zero progress, while something owes progress, before the game is
    // declared deadlocked. Generous enough that no legitimate animation or score
    // count reaches it.
    constexpr int STALL_REPORT_FRAMES = 300;

    // Detects a main loop that keeps running while the game makes no progress.
    class ProgressWatchdog
    {
    public:
        // `fingerprint` must change whenever the game advances.
        //
        // `stall_reason` names the subsystem that owes progress, or nullptr when
        // the game is legitimately idle waiting for the player. An idle game is
        // never reported, so this is safe to run in a shipped build.
        void tick(unsigned fingerprint, const char* stall_reason);

        void reset();

        [[nodiscard]] int stalled_frames() const { return _stalled_frames; }

    private:
        unsigned _fingerprint = 0;
        int _stalled_frames = 0;
        bool _primed = false;
    };

    // Order-sensitive hash used to tell progress from repetition.
    class FingerprintBuilder
    {
    public:
        void add(int value)
        {
            _hash = (_hash ^ unsigned(value)) * 16777619u;
        }

        void add(bool value)
        {
            add(value ? 1 : 0);
        }

        [[nodiscard]] unsigned value() const { return _hash; }

    private:
        unsigned _hash = 2166136261u;
    };

    // Everything the battle state machine needs to advance on its own, hashed.
    //
    // Deliberately excludes per-frame animation counters: a flight's frame
    // counter ticks every frame even when the flight is wedged, so including it
    // would make every stall look like progress.
    [[nodiscard]] unsigned battle_fingerprint(const GameContext& ctx);

    // Names the subsystem that owes progress without player input, or nullptr
    // when the game is legitimately waiting for the player.
    [[nodiscard]] const char* battle_stall_reason(const GameContext& ctx);
}

#endif
