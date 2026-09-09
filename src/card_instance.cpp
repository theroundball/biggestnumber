#include "card_instance.h"

#include "bn_string.h"
#include "card_data.h"

int effective_immediate_plus(const CardInstance& instance)
{
    return card_data(instance.base).immediate_plus + instance.plus_digit * 10;
}

int effective_immediate_multiply(const CardInstance& instance)
{
    const int base = card_data(instance.base).immediate_multiply;

    if(base <= 1)
    {
        return 0;
    }

    return base + instance.increment_mult;
}

bool card_is_multiplier(CardType type)
{
    if(card_base_play_multiplier(type) > 1)
    {
        return true;
    }

    return false;
}

int card_base_play_multiplier(CardType type)
{
    const int immediate = card_data(type).immediate_multiply;

    if(immediate > 1)
    {
        return immediate;
    }

    switch(type)
    {
    case CardType::CLOVER:
        return 3;

    case CardType::BIG_KUROSAWA_BURGER:
        return 4;

    case CardType::OVERCLOCK:
        return 2;

    default:
        return 0;
    }
}

int instance_play_multiplier(const CardInstance& instance)
{
    const int base = card_base_play_multiplier(instance.base);

    if(base <= 1)
    {
        return 0;
    }

    return base + instance.increment_mult;
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

    if(instance.increment_mult != 0)
    {
        out.append(" x");
        out.append(bn::to_string<8>(instance_play_multiplier(instance)));
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

    if(instance.increment_mult != 0)
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
    return instance.plus_digit != 0 || instance.increment_mult != 0 ||
           instance.gravity != Gravity::NONE;
}

bool instance_can_plus_digit(const CardInstance& instance)
{
    return card_data(instance.base).immediate_plus > 0;
}

bool instance_can_increment_mult(const CardInstance& instance)
{
    return card_is_multiplier(instance.base);
}

bool instance_can_gravity(const CardInstance& instance)
{
    return !instance.has_gravity_upgrade;
}

bool instance_apply_plus_digit(CardInstance& instance)
{
    if(!instance_can_plus_digit(instance))
    {
        return false;
    }

    ++instance.plus_digit;
    instance.has_plus_upgrade = true;
    return true;
}

bool instance_apply_increment_mult(CardInstance& instance)
{
    if(!instance_can_increment_mult(instance))
    {
        return false;
    }

    ++instance.increment_mult;
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
