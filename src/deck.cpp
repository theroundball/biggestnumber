#include "deck.h"

#include "bn_span.h"
#include "bn_utility.h"
#include "card_type.h"

Deck::Deck(int card_count, int dealt_cards) :
    _cards{},
    _size(card_count),
    _next_card(dealt_cards)
{
}

int Deck::remaining() const
{
    return _size - _next_card;
}

bool Deck::empty() const
{
    return _next_card >= _size;
}

int Deck::size() const
{
    return _size;
}

bool Deck::draw(CardRef& card)
{
    if(empty())
    {
        return false;
    }

    card = _cards[_next_card];
    ++_next_card;
    return true;
}

void Deck::add_card(CardRef card)
{
    if(_size < MAX_CARD_COUNT)
    {
        _cards[_size] = card;
        ++_size;
    }
}

void Deck::add_card(CardType card_type)
{
    add_card(CardRef{card_type, NO_INSTANCE});
}

void Deck::insert_top(CardRef card)
{
    if(_size < MAX_CARD_COUNT)
    {
        for(int i = _size; i > _next_card; --i)
        {
            _cards[i] = _cards[i - 1];
        }

        _cards[_next_card] = card;
        ++_size;
    }
}

void Deck::insert_top(CardType card_type)
{
    insert_top(CardRef{card_type, NO_INSTANCE});
}

void Deck::compact()
{
    const int remaining_count = remaining();

    for(int i = 0; i < remaining_count; ++i)
    {
        _cards[i] = _cards[_next_card + i];
    }

    _next_card = 0;
    _size = remaining_count;
}

void Deck::remove_undrawn(int count)
{
    const int available = remaining();

    if(count > available)
    {
        count = available;
    }

    _size -= count;
}

void Deck::remove_undrawn_from_top(int count)
{
    const int available = remaining();

    if(count > available)
    {
        count = available;
    }

    _next_card += count;
}

void Deck::remove_undrawn_at(int index)
{
    if(index < 0 || index >= remaining())
    {
        return;
    }

    const int absolute = _next_card + index;

    for(int slot = absolute; slot < _size - 1; ++slot)
    {
        _cards[slot] = _cards[slot + 1];
    }

    --_size;
}

void Deck::exile_random_undrawn(int count, bn::seed_random& random_engine, bn::vector<CardRef, 50>& exile_out)
{
    while(count > 0 && !empty())
    {
        const int pick = random_engine.get_int(remaining());
        const CardRef card = peek_undrawn_ref(pick);
        remove_undrawn_at(pick);

        if(!exile_out.full())
        {
            exile_out.push_back(card);
        }

        --count;
    }
}

void Deck::exile_undrawn_end(int count, bn::vector<CardRef, 50>& exile_out)
{
    int remaining_to_exile = count;

    if(remaining_to_exile > remaining())
    {
        remaining_to_exile = remaining();
    }

    while(remaining_to_exile > 0 && _size > _next_card)
    {
        --_size;
        const CardRef card = _cards[_size];

        if(!exile_out.full())
        {
            exile_out.push_back(card);
        }

        --remaining_to_exile;
    }
}

void Deck::exile_undrawn_from_top(int count, bn::vector<CardRef, 50>& exile_out)
{
    int remaining_to_exile = count;

    if(remaining_to_exile > remaining())
    {
        remaining_to_exile = remaining();
    }

    while(remaining_to_exile > 0 && !empty())
    {
        const CardRef card = _cards[_next_card];
        remove_undrawn_at(0);

        if(!exile_out.full())
        {
            exile_out.push_back(card);
        }

        --remaining_to_exile;
    }
}

CardRef Deck::peek_undrawn_ref(int index) const
{
    return _cards[_next_card + index];
}

CardType Deck::peek_undrawn(int index) const
{
    return _cards[_next_card + index].type;
}

bn::span<const CardRef> Deck::undrawn_span() const
{
    return bn::span<const CardRef>(&_cards[_next_card], remaining());
}

void Deck::move_undrawn_to_top(int index)
{
    if(index <= 0 || index >= remaining())
    {
        return;
    }

    const CardRef card = _cards[_next_card + index];

    for(int i = _next_card + index; i > _next_card; --i)
    {
        _cards[i] = _cards[i - 1];
    }

    _cards[_next_card] = card;
}

void Deck::shuffle(bn::seed_random& random_engine)
{
    const int current_remaining = remaining();

    for(int i = current_remaining - 1; i > 0; --i)
    {
        const int random_offset = random_engine.get_int(i + 1);
        const int first_target_index = _next_card + i;
        const int second_target_index = _next_card + random_offset;
        bn::swap(_cards[first_target_index], _cards[second_target_index]);
    }
}

void Deck::apply_gravity(const InstancePool& pool)
{
    const int count = remaining();

    if(count <= 1 || pool.count == 0)
    {
        return;
    }

    CardRef yeast[MAX_CARD_COUNT];
    CardRef plain[MAX_CARD_COUNT];
    CardRef lead[MAX_CARD_COUNT];
    int yeast_n = 0;
    int plain_n = 0;
    int lead_n = 0;

    for(int index = 0; index < count; ++index)
    {
        const CardRef card = _cards[_next_card + index];
        const CardInstance* instance = instance_at(pool, card.instance_id);

        if(instance && instance->gravity == Gravity::YEAST)
        {
            yeast[yeast_n++] = card;
        }
        else if(instance && instance->gravity == Gravity::LEAD)
        {
            lead[lead_n++] = card;
        }
        else
        {
            plain[plain_n++] = card;
        }
    }

    int write = _next_card;

    for(int index = 0; index < yeast_n; ++index)
    {
        _cards[write++] = yeast[index];
    }

    for(int index = 0; index < plain_n; ++index)
    {
        _cards[write++] = plain[index];
    }

    for(int index = 0; index < lead_n; ++index)
    {
        _cards[write++] = lead[index];
    }
}

void Deck::ensure_unique_bounty_instances(InstancePool& pool, uint8_t& next_bounty_id)
{
    bool instance_seen[InstancePool::CAPACITY] = {};
    bool bounty_id_seen[InstancePool::CAPACITY] = {};

    for(int index = 0; index < _size; ++index)
    {
        CardRef& card = _cards[index];

        if(card.type != CardType::BOUNTY)
        {
            continue;
        }

        const bool duplicate_instance = card.has_instance() && card.instance_id < InstancePool::CAPACITY &&
                                        instance_seen[card.instance_id];

        if(card.has_instance() && !duplicate_instance)
        {
            instance_seen[card.instance_id] = true;
        }
        else
        {
            CardInstance template_instance{};

            if(card.has_instance())
            {
                if(const CardInstance* existing = instance_at(pool, card.instance_id))
                {
                    template_instance = *existing;
                }
            }

            const uint8_t new_id = instance_pool_add(pool, CardType::BOUNTY);

            if(new_id != NO_INSTANCE)
            {
                if(card.has_instance())
                {
                    pool.entries[new_id] = template_instance;
                    pool.entries[new_id].base = CardType::BOUNTY;
                }

                card.instance_id = new_id;
                instance_seen[new_id] = true;
            }
        }

        const bool duplicate_bounty = card.has_bounty_copy() && card.bounty_id < InstancePool::CAPACITY &&
                                      bounty_id_seen[card.bounty_id];

        if(card.has_bounty_copy() && !duplicate_bounty)
        {
            bounty_id_seen[card.bounty_id] = true;

            if(card.bounty_id >= next_bounty_id)
            {
                next_bounty_id = static_cast<uint8_t>(card.bounty_id + 1);
            }

            continue;
        }

        if(next_bounty_id >= InstancePool::CAPACITY)
        {
            continue;
        }

        card.bounty_id = next_bounty_id;
        bounty_id_seen[card.bounty_id] = true;
        ++next_bounty_id;
    }
}
