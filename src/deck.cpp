#include "deck.h"

#include "bn_span.h"
#include "bn_utility.h"

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
