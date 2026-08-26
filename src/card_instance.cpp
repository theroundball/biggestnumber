#include "card_instance.h"

#include "bn_string.h"
#include "card_data.h"

int effective_immediate_plus(const CardInstance& instance)
{
    const int base = card_data(instance.base).immediate_plus;

    if(instance.plus_digit == 0)
    {
        return base;
    }

    // Concatenate digit to the right: +3 + digit 7 → +37.
    return base * 10 + instance.plus_digit;
}

int effective_immediate_multiply(const CardInstance& instance)
{
    const int base = card_data(instance.base).immediate_multiply;

    if(base != 0)
    {
        return base;
    }

    return instance.increment_mult ? 2 : 0;
}

void instance_pool_clamp(InstancePool& pool)
{
    if(pool.count > InstancePool::CAPACITY)
    {
        pool.count = InstancePool::CAPACITY;
    }
}

const CardInstance* instance_at(const InstancePool& pool, uint8_t id)
{
    if(id >= InstancePool::CAPACITY || id >= pool.count)
    {
        return nullptr;
    }

    return &pool.entries[id];
}

CardInstance* instance_at_mut(InstancePool& pool, uint8_t id)
{
    if(id >= InstancePool::CAPACITY || id >= pool.count)
    {
        return nullptr;
    }

    return &pool.entries[id];
}

uint8_t instance_pool_add(InstancePool& pool, CardType type)
{
    if(pool.count >= InstancePool::CAPACITY)
    {
        return NO_INSTANCE;
    }

    const uint8_t id = pool.count;
    pool.entries[id] = CardInstance{};
    pool.entries[id].base = type;
    ++pool.count;
    return id;
}

void instance_pool_clear(InstancePool& pool)
{
    pool.count = 0;
}

void format_instance_upgrade_suffix(const CardInstance& instance, bn::string<32>& out)
{
    out.clear();

    if(instance.plus_digit != 0)
    {
        out.append(" +");
        out.append(bn::to_string<8>(effective_immediate_plus(instance)));
    }

    if(instance.increment_mult)
    {
        out.append(" x2");
    }

    if(instance.gravity == Gravity::LEAD)
    {
        out.append(" [Lead]");
    }
    else if(instance.gravity == Gravity::YEAST)
    {
        out.append(" [Yeast]");
    }
}

void format_instance_upgrade_pips(const CardInstance& instance, bn::string<8>& out)
{
    out.clear();

    if(instance.plus_digit != 0)
    {
        out.append("+");
    }

    if(instance.increment_mult)
    {
        out.append("x");
    }

    if(instance.gravity == Gravity::LEAD)
    {
        out.append("L");
    }
    else if(instance.gravity == Gravity::YEAST)
    {
        out.append("Y");
    }
}

bool instance_has_upgrades(const CardInstance& instance)
{
    return instance.plus_digit != 0 || instance.increment_mult ||
           instance.gravity != Gravity::NONE;
}

bool instance_can_plus_digit(const CardInstance& instance)
{
    return !instance.has_plus_upgrade;
}

bool instance_can_increment_mult(const CardInstance& instance)
{
    if(instance.has_mult_upgrade)
    {
        return false;
    }

    // Only if the base card has no multiply of its own.
    return card_data(instance.base).immediate_multiply == 0;
}

bool instance_can_gravity(const CardInstance& instance)
{
    return !instance.has_gravity_upgrade;
}

bool instance_apply_plus_digit(CardInstance& instance, uint8_t digit_1_to_9)
{
    if(!instance_can_plus_digit(instance) || digit_1_to_9 < 1 || digit_1_to_9 > 9)
    {
        return false;
    }

    instance.plus_digit = digit_1_to_9;
    instance.has_plus_upgrade = true;
    return true;
}

bool instance_apply_increment_mult(CardInstance& instance)
{
    if(!instance_can_increment_mult(instance))
    {
        return false;
    }

    instance.increment_mult = true;
    instance.has_mult_upgrade = true;
    return true;
}

bool instance_apply_gravity(CardInstance& instance, Gravity gravity)
{
    if(!instance_can_gravity(instance) || gravity == Gravity::NONE)
    {
        return false;
    }

    instance.gravity = gravity;
    instance.has_gravity_upgrade = true;
    return true;
}
