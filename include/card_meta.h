#ifndef CARD_META_H
#define CARD_META_H

#include <cstdint>

#include "bn_seed_random.h"
#include "bn_vector.h"

#include "card_type.h"

enum class CardRarity : uint8_t
{
    COMMON,
    UNCOMMON,
    RARE,
};

struct CardMeta
{
    CardRarity rarity = CardRarity::COMMON;
    // 0 = never offered on the drop table.
    uint8_t max_copies = 5;
};

const CardMeta& card_meta(CardType type);

// Combo pieces (RPS, PB&J, Straw/Sticks/Bricks) — never offered again once owned.
bool card_is_combo_piece(CardType type);

bool palindrome_prize_eligible(const SaveData& save);

// Merged baseline+delta rarities for the three prize slots (campaign prize_system).
void drop_merged_slot_rarities(int run_peak_before, int battle_score, CardRarity out_slots[3]);

// Roguelike drop UI: roll three card offers for the given slot rarities.
void roll_drop_offers(const bn::vector<CardType, 50>& run_deck, const CardRarity slots[3],
                      bn::seed_random& rng, CardType out_offers[3]);

#endif
