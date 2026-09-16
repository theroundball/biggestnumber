#ifndef SCENE_GRAPHICS_H
#define SCENE_GRAPHICS_H

#include "bn_span.h"

#include "card.h"

// Shared card graphics lifecycle (menu / deck editor / overworld / battle).
//
// The global caches (text-card tiles, border palettes, placeholder palette,
// text-box palette) are owned by card.cpp and are referenced by live Card
// sprites. Butano refcounts the handles, so dropping a cache entry while a
// sprite still uses it frees nothing -- it only makes the cache forget the entry
// and allocate a duplicate on the next lookup. Enough duplicates and OBJ VRAM or
// the 16 palette banks run out, which surfaces as a Butano BN_ERROR nowhere near
// the code responsible.
//
// Getting that ordering right by hand was the source of a long tail of crashes,
// so it is now enforced instead of documented:
//
// 1. reclaim_scene_graphics_state() is self-gating. It does nothing while
//    card_art_is_live(), and logs when it declines. Calling it at the wrong
//    moment is a no-op rather than a corruption, so the sequence below is a
//    recommendation for promptness, not a correctness requirement.
//
// 2. Preferred scene exit sequence for card-heavy scenes:
//      scene_graphics_release_card_pool(pool)  // each Card pool in the scene
//      destroy Card objects / leave scene
//      scene_graphics_reclaim_all()
//
// 3. Battle (GameContext) does not reclaim during the fight loop. Battle entry
//    reclaims after the previous context shut down; battle exit releases all
//    card pools then reclaims in shutdown_for_exit().
//
// 4. During battle, card pools are hidden when idle rather than released to
//    placeholder. Releasing mid-animation (especially play_flights[0].fx_card)
//    drops the art out from under a visible card.
//
// 5. The shared 32-tile text-card body block is allocated once and kept for the
//    lifetime of the program. Every text-only card in every scene draws from it,
//    so reclaiming it saves 32 of 1024 tiles in exchange for a duplicate-
//    allocation hazard.

// Reset shared palette/tile caches. See rule 2.
void scene_graphics_reclaim_all();

// Release every card in a pool back to placeholder tiles (call before ~Card).
void scene_graphics_release_card_pool(bn::span<Card> cards);

// Alias for rule 3 — release pools first, then call this.
void scene_graphics_leave_card_scene();

// Battle entry: reclaim stale caches after the previous battle/menu released cards.
void scene_graphics_prepare_battle();

#ifndef BN_DATA_EWRAM_BSS
    #define BN_DATA_EWRAM_BSS __attribute__((section(".sbss")))
#endif

// EWRAM card pool with placement-new lifecycle (avoids ~16KB GBA stack overflow).
template<int MaxCards>
class CardDisplayPool
{
public:
    CardDisplayPool() = default;

    CardDisplayPool(const CardDisplayPool&) = delete;
    CardDisplayPool& operator=(const CardDisplayPool&) = delete;

    void construct_cards()
    {
        destroy_cards();

        for(int index = 0; index < MaxCards; ++index)
        {
            new(card_slot(index)) Card();
            card_slot(index)->set_visible(false);
        }

        _constructed = true;
    }

    void release_all()
    {
        if(!_constructed)
        {
            return;
        }

        scene_graphics_release_card_pool(span());
    }

    void destroy_cards()
    {
        if(!_constructed)
        {
            return;
        }

        scene_graphics_release_card_pool(span());

        for(int index = 0; index < MaxCards; ++index)
        {
            card_slot(index)->~Card();
        }

        _constructed = false;
    }

    ~CardDisplayPool()
    {
        destroy_cards();
    }

    Card& operator[](int index)
    {
        return *card_slot(index);
    }

    bn::span<Card> span()
    {
        return bn::span<Card>(card_slot(0), MaxCards);
    }

    int size() const
    {
        return MaxCards;
    }

private:
    alignas(Card) char _storage[sizeof(Card) * MaxCards];
    bool _constructed = false;

    Card* card_slot(int index)
    {
        return reinterpret_cast<Card*>(_storage + (sizeof(Card) * index));
    }
};

#endif
