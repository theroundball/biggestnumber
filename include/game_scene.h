#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "bn_vector.h"

#include "card_instance.h"
#include "card_type.h"
#include "game_state.h"
#include "game_types.h"

GameSceneResult run_game_scene(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch);
GameSceneResult run_game_scene(const bn::vector<CardType, 50>& collection, const BattleLaunch& launch);
GameSceneResult run_game_scene(const bn::vector<CardType, 50>& collection, int deck_index);

#endif
