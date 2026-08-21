#include "scoring.h"


bn::string<48> format_round_score(const RoundScore& r)
{
    bn::string<48> inner = bn::to_string<12>(r.running);

    if(r.end_multiplier == 1)
    {
        return inner;
    }

    bn::string<48> out;
    out.append(bn::to_string<12>(r.end_multiplier));
    out.append("(");
    out.append(inner);
    out.append(")");
    return out;
}

bn::string<16> format_round_modifier(const RoundModifier& modifier)
{
    bn::string<16> out;

    if(modifier.positive)
    {
        out.append("+");
        out.append(bn::to_string<8>(modifier.positive));
    }

    if(modifier.multiply)
    {
        if(! out.empty())
        {
            out.append(" ");
        }

        out.append("x");
        out.append(bn::to_string<8>(modifier.multiply));
    }

    if(modifier.draw_at_start)
    {
        if(! out.empty())
        {
            out.append(" ");
        }

        out.append("draw ");
        out.append(bn::to_string<8>(modifier.draw_at_start));
    }

    return out;
}
