#include "world_data.h"

namespace
{
    constexpr CardType WHEELIE_CARDS[] = {
        CardType::LONGBOARD,
        CardType::HEELYS,
        CardType::SCOOTER,
        CardType::SKATEBOARD,
        CardType::ROLLER_BLADES,
        CardType::WAGON,
        CardType::BIKE,
        CardType::TOPPINGS,
        CardType::CYCLE,
        CardType::CYCLE_SEVEN,
    };

    constexpr CardType COMBO_KID_CARDS[] = {
        CardType::ROCK,
        CardType::PAPER,
        CardType::SCISSORS,
        CardType::SHOOT,
        CardType::PEANUT_BUTTER,
        CardType::JELLY,
        CardType::STRAW,
        CardType::STICKS,
        CardType::BRICKS,
    };

    constexpr CardType CLOCK_SHOP_CARDS[] = {
        CardType::SIPS,
        CardType::SNAIL_MAIL,
        CardType::TIME_IS_TOO_EXPENSIVE,
        CardType::TIME_IS_MONEY,
        CardType::SEVEN_FEET_DEEP,
        CardType::OVERCLOCK,
        CardType::FINALE,
        CardType::EVALUATE,
        CardType::SEMAPHORE,
    };

    constexpr CardType GRAVEYARD_GATE_CARDS[] = {
        CardType::BONES,
        CardType::BUSTED,
        CardType::THRESHOLD,
        CardType::TOMBSTONES,
        CardType::JACKS,
        CardType::FISHING_POLE,
        CardType::SHELLS,
        CardType::ROLL_OVER,
        CardType::JOURNAL,
    };

    constexpr CardType UNDERTAKER_CARDS[] = {
        CardType::LIFELINE,
        CardType::NECROMANCY,
        CardType::RAGS_TO_RICHES,
        CardType::BIRDS_OF_A_FEATHER,
        CardType::DEAD_RISING,
        CardType::COMEBACK,
        CardType::ENCORE,
        CardType::GET_ME_OUTA_HERE,
        CardType::CLOVER,
    };

    constexpr CardType LIBRARIAN_CARDS[] = {
        CardType::HACKER,
        CardType::LIBRARIAN,
        CardType::PILOT,
        CardType::MIRACLE,
        CardType::SPECULATIVE,
        CardType::FLEX,
        CardType::SWIVEL,
        CardType::WISHES,
        CardType::CATNIP,
    };

    constexpr CardType DIGIT_HERMIT_CARDS[] = {
        CardType::SWAP,
        CardType::THE_FOURTH,
        CardType::THE_FIFTH,
        CardType::PALINDROME,
        CardType::BUILD_A_NUMBER,
        CardType::MINOR_FALL,
        CardType::MAJOR_LIFT,
        CardType::ROUNDUP,
        CardType::DILLA,
    };

    constexpr CardType ODD_JOBS_CARDS[] = {
        CardType::STOLLER,
        CardType::RIP_STICK,
        CardType::BIG_KUROSAWA_BURGER,
        CardType::TURTLE_MODE,
        CardType::TRIPTYCH,
        CardType::SOLO,
        CardType::BOUNTY,
    };

    constexpr NpcDef NPC_DEFS[WORLD_NPC_COUNT] = {
        {"Wheelie", CampaignMode::BIGGEST_NUMBER, 48, 56, WHEELIE_CARDS,
         int(sizeof(WHEELIE_CARDS) / sizeof(WHEELIE_CARDS[0]))},
        {"Combo Kid", CampaignMode::SAME_NUMBER, 208, 56, COMBO_KID_CARDS,
         int(sizeof(COMBO_KID_CARDS) / sizeof(COMBO_KID_CARDS[0]))},
        {"Clock Shop", CampaignMode::NUMBER_NOW, 48, 128, CLOCK_SHOP_CARDS,
         int(sizeof(CLOCK_SHOP_CARDS) / sizeof(CLOCK_SHOP_CARDS[0]))},
        {"Graveyard", CampaignMode::AINT_GOT_TIME, 208, 128, GRAVEYARD_GATE_CARDS,
         int(sizeof(GRAVEYARD_GATE_CARDS) / sizeof(GRAVEYARD_GATE_CARDS[0]))},
        {"Undertaker", CampaignMode::SHARING_IS_CARING, 48, 200, UNDERTAKER_CARDS,
         int(sizeof(UNDERTAKER_CARDS) / sizeof(UNDERTAKER_CARDS[0]))},
        {"Librarian", CampaignMode::POKER_HAND, 208, 200, LIBRARIAN_CARDS,
         int(sizeof(LIBRARIAN_CARDS) / sizeof(LIBRARIAN_CARDS[0]))},
        {"Digit Hermit", CampaignMode::Y2K, 128, 88, DIGIT_HERMIT_CARDS,
         int(sizeof(DIGIT_HERMIT_CARDS) / sizeof(DIGIT_HERMIT_CARDS[0]))},
        {"Odd Jobs", CampaignMode::BIGGEST_NUMBER, 128, 216, ODD_JOBS_CARDS,
         int(sizeof(ODD_JOBS_CARDS) / sizeof(ODD_JOBS_CARDS[0]))},
    };
}

const NpcDef& world_npc_def(int npc_index)
{
    if(npc_index < 0 || npc_index >= WORLD_NPC_COUNT)
    {
        return NPC_DEFS[0];
    }

    return NPC_DEFS[npc_index];
}

int world_npc_default_collection_count(int npc_index, CardType type)
{
    if(npc_index < 0 || npc_index >= WORLD_NPC_COUNT || type == CardType::COUNT)
    {
        return 0;
    }

    const NpcDef& npc = NPC_DEFS[npc_index];
    int count = 0;

    for(int index = 0; index < npc.collection_count; ++index)
    {
        if(npc.collection[index] == type)
        {
            ++count;
        }
    }

    return count;
}
