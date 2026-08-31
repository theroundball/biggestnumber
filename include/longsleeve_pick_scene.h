#ifndef LONGSLEEVE_PICK_SCENE_H
#define LONGSLEEVE_PICK_SCENE_H

#include "bn_array.h"

#include "card_instance.h"
#include "save_data.h"

bool run_longsleeve_deck_pick_scene(SaveData& save, int deck_index, bn::array<uint8_t, 2>& out_instance_ids);

#endif
