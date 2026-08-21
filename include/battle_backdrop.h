#ifndef BATTLE_BACKDROP_H
#define BATTLE_BACKDROP_H

// Procedural animated battle background (regular BG layer — zero sprites).
// Call init once after bn::core::init(), then tick() before each bn::core::update().

void battle_backdrop_init();
void battle_backdrop_tick();
void battle_backdrop_set_visible(bool visible);

#endif
