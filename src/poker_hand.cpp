#include "poker_hand.h"

namespace
{
    int poker_hand_rank_base_points(PokerHandRank rank)
    {
        switch(rank)
        {
        case PokerHandRank::HIGH_CARD:
            return 50;
        case PokerHandRank::PAIR:
            return 100;
        case PokerHandRank::TWO_PAIR:
            return 200;
        case PokerHandRank::THREE_OF_A_KIND:
            return 400;
        case PokerHandRank::STRAIGHT:
            return 600;
        case PokerHandRank::FULL_HOUSE:
            return 750;
        case PokerHandRank::FOUR_OF_A_KIND:
            return 900;
        case PokerHandRank::FIVE_OF_A_KIND:
            return 1000;
        case PokerHandRank::FLUSH:
        default:
            return 0;
        }
    }

    int count_digit(const bn::array<int, 5>& digits, int digit, bool used[5])
    {
        int count = 0;

        for(int index = 0; index < 5; ++index)
        {
            if(!used[index] && digits[index] == digit)
            {
                ++count;
            }
        }

        return count;
    }

    int sum_digits(const bn::array<int, 5>& digits, bool used[5])
    {
        int total = 0;

        for(int index = 0; index < 5; ++index)
        {
            if(used[index] && digits[index] > 0)
            {
                total += digits[index];
            }
        }

        return total;
    }

    void mark_digit(bn::array<int, 5>& digits, bool used[5], int digit, int times)
    {
        for(int index = 0; index < 5 && times > 0; ++index)
        {
            if(!used[index] && digits[index] == digit)
            {
                used[index] = true;
                --times;
            }
        }
    }

    bool is_straight_five(const bn::array<int, 5>& digits)
    {
        int values[5] = {};
        int count = 0;

        for(int digit : digits)
        {
            if(digit > 0)
            {
                values[count++] = digit;
            }
        }

        if(count != 5)
        {
            return false;
        }

        for(int first = 0; first < count - 1; ++first)
        {
            for(int second = first + 1; second < count; ++second)
            {
                if(values[first] > values[second])
                {
                    const int swap = values[first];
                    values[first] = values[second];
                    values[second] = swap;
                }
            }
        }

        for(int index = 1; index < 5; ++index)
        {
            if(values[index] != values[index - 1] + 1)
            {
                return false;
            }
        }

        return true;
    }

    int score_for_rank(PokerHandRank rank, const bn::array<int, 5>& digits, bool used[5])
    {
        return poker_hand_rank_base_points(rank) + sum_digits(digits, used);
    }

    PokerHandRank best_rank_from_digits(const bn::array<int, 5>& digits, bool used[5], int& out_score)
    {
        int counts[10] = {};

        for(int index = 0; index < 5; ++index)
        {
            if(digits[index] > 0)
            {
                ++counts[digits[index]];
            }
        }

        int pair_digits[5] = {};
        int pair_count = 0;
        int three_digit = -1;
        int four_digit = -1;
        int five_digit = -1;

        for(int digit = 1; digit <= 9; ++digit)
        {
            if(counts[digit] >= 5)
            {
                five_digit = digit;
            }
            else if(counts[digit] == 4)
            {
                four_digit = digit;
            }
            else if(counts[digit] == 3)
            {
                three_digit = digit;
            }
            else if(counts[digit] == 2)
            {
                pair_digits[pair_count++] = digit;
            }
        }

        bool used_local[5] = {};

        if(five_digit > 0)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, five_digit, 5);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::FIVE_OF_A_KIND, digits, used);
            return PokerHandRank::FIVE_OF_A_KIND;
        }

        if(four_digit > 0)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, four_digit, 4);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::FOUR_OF_A_KIND, digits, used);
            return PokerHandRank::FOUR_OF_A_KIND;
        }

        if(three_digit > 0 && pair_count > 0)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, three_digit, 3);
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, pair_digits[0], 2);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::FULL_HOUSE, digits, used);
            return PokerHandRank::FULL_HOUSE;
        }

        if(is_straight_five(digits))
        {
            for(int index = 0; index < 5; ++index)
            {
                if(digits[index] > 0)
                {
                    used[index] = true;
                }
            }

            out_score = score_for_rank(PokerHandRank::STRAIGHT, digits, used);
            return PokerHandRank::STRAIGHT;
        }

        if(pair_count >= 2)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, pair_digits[1], 2);
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, pair_digits[0], 2);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::TWO_PAIR, digits, used);
            return PokerHandRank::TWO_PAIR;
        }

        if(three_digit > 0)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, three_digit, 3);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::THREE_OF_A_KIND, digits, used);
            return PokerHandRank::THREE_OF_A_KIND;
        }

        if(pair_count == 1)
        {
            mark_digit(const_cast<bn::array<int, 5>&>(digits), used_local, pair_digits[0], 2);
            for(int index = 0; index < 5; ++index)
            {
                used[index] = used_local[index];
            }

            out_score = score_for_rank(PokerHandRank::PAIR, digits, used);
            return PokerHandRank::PAIR;
        }

        for(int index = 0; index < 5; ++index)
        {
            if(digits[index] > 0)
            {
                used[index] = true;
            }
        }

        out_score = score_for_rank(PokerHandRank::HIGH_CARD, digits, used);
        return PokerHandRank::HIGH_CARD;
    }
}

int poker_hand_rank_points(PokerHandRank rank)
{
    return poker_hand_rank_base_points(rank);
}

PokerHandEvaluation poker_hand_evaluate(const bn::array<int, 5>& digits)
{
    PokerHandEvaluation result;
    int placed = 0;

    for(int digit : digits)
    {
        if(digit > 0)
        {
            ++placed;
        }
    }

    if(placed == 0)
    {
        return result;
    }

    bool used[5] = {};
    int score = 0;
    result.rank = best_rank_from_digits(digits, used, score);
    result.score = score;
    result.valid = true;
    return result;
}

const char* poker_hand_rank_name(PokerHandRank rank)
{
    switch(rank)
    {
    case PokerHandRank::HIGH_CARD:
        return "High Card";
    case PokerHandRank::PAIR:
        return "Pair";
    case PokerHandRank::TWO_PAIR:
        return "Two Pair";
    case PokerHandRank::THREE_OF_A_KIND:
        return "Three of a Kind";
    case PokerHandRank::STRAIGHT:
        return "Straight";
    case PokerHandRank::FLUSH:
        return "Flush";
    case PokerHandRank::FULL_HOUSE:
        return "Full House";
    case PokerHandRank::FOUR_OF_A_KIND:
        return "Four of a Kind";
    case PokerHandRank::FIVE_OF_A_KIND:
        return "Five of a Kind";
    default:
        return "?";
    }
}
