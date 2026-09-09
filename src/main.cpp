#include "bn_core.h"
#include "bn_seed_random.h"

#include "battle_backdrop.h"
#include "campaign_flow.h"
#include "overworld_scene.h"
#include "save_data.h"

namespace
{
    unsigned make_random_seed()
    {
        unsigned seed = static_cast<unsigned>(bn::core::current_cpu_ticks());
        seed ^= static_cast<unsigned>(bn::core::last_cpu_ticks()) << 16;

        if(seed == 0)
        {
            seed = 1;
        }

        return seed;
    }
}

int main()
{
    bn::core::init();
    save_data_init();
    battle_backdrop_init();

    bn::seed_random rng(make_random_seed());
    campaign_run_boot_setup(rng);

    while(true)
    {
        switch(run_overworld_scene())
        {
        case OverworldSceneResult::OPEN_PLAY_MENU:
            campaign_run_overworld_play_flow(rng);
            break;

        default:
            break;
        }
    }
}
