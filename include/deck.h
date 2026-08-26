#ifndef DECK_H
#define DECK_H

#include "bn_seed_random.h"
#include "bn_span.h"
#include "bn_vector.h"
#include "card_instance.h"

class Deck
{
public:
    explicit Deck(int card_count = 0, int dealt_cards = 0);

    [[nodiscard]] int remaining() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] int size() const;
    bool draw(CardRef& card);
    void add_card(CardRef card);
    void add_card(CardType card_type); // classic: no instance
    void insert_top(CardRef card);
    void insert_top(CardType card_type);
    void shuffle(bn::seed_random& random_engine);

    [[nodiscard]] CardRef peek_undrawn_ref(int index) const;
    [[nodiscard]] CardType peek_undrawn(int index) const;
    [[nodiscard]] bn::span<const CardRef> undrawn_span() const;
    void move_undrawn_to_top(int index);

    void compact();
    void remove_undrawn(int count);
    void remove_undrawn_from_top(int count);
    void remove_undrawn_at(int index);
    // Remove random undrawn cards into `exile_out` (run exile zone).
    void exile_random_undrawn(int count, bn::seed_random& random_engine, bn::vector<CardRef, 50>& exile_out);
    // Remove undrawn from the deck end into `exile_out` (Lifeline-style).
    void exile_undrawn_end(int count, bn::vector<CardRef, 50>& exile_out);

    // After shuffle / insert — Yeast toward top (low undrawn index), Lead toward bottom.
    void apply_gravity(const InstancePool& pool);

private:
    static constexpr int MAX_CARD_COUNT = 50;
    CardRef _cards[MAX_CARD_COUNT];
    int _size;
    int _next_card;
};

#endif
