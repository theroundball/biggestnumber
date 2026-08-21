#ifndef CARD_INSTANCE_H
#define CARD_INSTANCE_H

#include <cstdint>

#include "bn_array.h"
#include "bn_string.h"

#include "card_type.h"

enum class Gravity : uint8_t
{
    NONE,
    LEAD,
    YEAST,
};

// Per-copy upgrades for a run (and the battle copy of that run).
struct CardInstance
{
    CardType base = CardType::COUNT;
    Gravity gravity = Gravity::NONE;
    uint8_t plus_digit = 0;      // 0 = none; 1–9 concatenated onto base +N
    bool increment_mult = false; // ×2 on play when base has no immediate_multiply
    bool has_plus_upgrade = false;
    bool has_mult_upgrade = false;
    bool has_gravity_upgrade = false;
};

struct InstancePool
{
    static constexpr int CAPACITY = 50;
    bn::array<CardInstance, CAPACITY> entries{};
    uint8_t count = 0;
};

// Zone / deck entry: CardType for rules + optional instance id (255 = none / classic).
struct CardRef
{
    CardType type = CardType::COUNT;
    uint8_t instance_id = 255;

    bool has_instance() const
    {
        return instance_id != 255;
    }
};

constexpr uint8_t NO_INSTANCE = 255;

int effective_immediate_plus(const CardInstance& instance);
int effective_immediate_multiply(const CardInstance& instance);

const CardInstance* instance_at(const InstancePool& pool, uint8_t id);
CardInstance* instance_at_mut(InstancePool& pool, uint8_t id);

// Append a new instance of `type`. Returns id, or NO_INSTANCE if full.
uint8_t instance_pool_add(InstancePool& pool, CardType type);

void instance_pool_clear(InstancePool& pool);

// Format upgrade suffix for UI, e.g. " +37 ×2 [Lead]". Empty if none.
void format_instance_upgrade_suffix(const CardInstance& instance, bn::string<32>& out);

// Compact corner pips for card art overlays: "+", "x", "L", "Y" (combined).
void format_instance_upgrade_pips(const CardInstance& instance, bn::string<8>& out);

bool instance_has_upgrades(const CardInstance& instance);

bool instance_can_plus_digit(const CardInstance& instance);
bool instance_can_increment_mult(const CardInstance& instance);
bool instance_can_gravity(const CardInstance& instance);

bool instance_apply_plus_digit(CardInstance& instance, uint8_t digit_1_to_9);
bool instance_apply_increment_mult(CardInstance& instance);
bool instance_apply_gravity(CardInstance& instance, Gravity gravity);

#endif
