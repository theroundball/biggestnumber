#ifndef SCORING_H
#define SCORING_H

#include "bn_string.h"

// A future-round effect (also used as a card's immediate effect payload).
// `multiply == 0` means "no multiplier"; stacked future seeds add (x5 + x5 = x10).
struct RoundModifier
{
    int positive = 0;
    int multiply = 0;        // 0 = none; becomes the seeded round's end-multiplier
    int draw_at_start = 0;   // extra cards drawn when the seeded round begins

    void accumulate(const RoundModifier& o)
    {
        positive += o.positive;
        multiply += o.multiply;
        draw_at_start += o.draw_at_start;
    }
};

// The running score for the current round.
// Numeric value = end_multiplier * running. Multipliers stack additively into
// end_multiplier (x3 then x5 -> x8), including across turtle-mode sub-rounds.
struct RoundScore
{
    int running = 0;
    int end_multiplier = 1;

    void reset() { running = 0; end_multiplier = 1; }
    void add(int n) { running += n; }

    // First multiplier replaces the default 1; later ones add (x5 + x5 = x10).
    void add_multiplier(int m)
    {
        if(m <= 1)
        {
            return;
        }

        if(end_multiplier == 1)
        {
            end_multiplier = m;
        }
        else
        {
            end_multiplier += m;
        }
    }

    void schedule_end_multiply(int m) { add_multiplier(m); }
    int  committed() const { return running * end_multiplier; }
};


// struct RoundScore
// {
//     int positives = 0;
//     int negatives = 0;      // stored as a positive magnitude
//     int multiplier = 1;     // running product of every multiplier played
//     bool has_positive = false;
//     bool has_negative = false;
//     bool has_multiplier = false;

//     int value() const { return multiplier * (positives - negatives); }

//     void reset() { *this = RoundScore{}; }

//     // Fold a modifier (card effect or round-start seed) into the accumulator.
//     void apply(const RoundModifier& m)
//     {
//         if(m.positive) { positives += m.positive; has_positive = true; }
//         if(m.negative) { negatives += m.negative; has_negative = true; }
//         if(m.multiply) { multiplier *= m.multiply; has_multiplier = true; }
//     }
// };

// Render the round score per the display rules:
//   plus only        -> "P"
//   plus and minus   -> "P - N"
//   minus only       -> "-N"
//   multiplier wraps  -> "M(<inner>)"   (e.g. "3(0)", "3(12 - 5)")
//
// Sized for scores up to ~100,000,000 (9 digits): worst case
// "100000000(100000000 - 100000000)" is 32 chars, so 48 leaves headroom.
bn::string<48> format_round_score(const RoundScore& r);

// Short HUD label for a seeded future round (e.g. "+5", "x3", "draw 1").
bn::string<16> format_round_modifier(const RoundModifier& modifier);

#endif
