#ifndef LIBRARY_GRID_PICK_SCENE_H
#define LIBRARY_GRID_PICK_SCENE_H

#include "bn_span.h"

#include "card_type.h"
#include "menu_scenes.h"

struct LibraryGridPickResult
{
    bool picked = false;
    CardType card = CardType::COUNT;
};

// Deck-editor-style catalog grid: browse owned types, Select inspect, A confirm, B cancel.
LibraryGridPickResult run_library_grid_pick_scene(const char* title, const char* confirm_hint,
                                                  bn::span<const CardType> types);

#endif
