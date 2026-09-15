#ifndef OVERWORLD_FIELD_H
#define OVERWORLD_FIELD_H

#include "bn_fixed.h"

// Solid green walkable map on a regular BG layer (black void is the backdrop color).
void overworld_field_init();
void overworld_field_set_visible(bool visible);
void overworld_field_set_camera(bn::fixed camera_x, bn::fixed camera_y);

#endif
