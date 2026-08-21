#include "battle_backdrop.h"

#include "bn_backdrop.h"
#include "bn_bg_palette_item.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bg_tiles.h"
#include "bn_math.h"
#include "bn_optional.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_cell_info.h"
#include "bn_regular_bg_map_item.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_tiles_item.h"
#include "bn_tile.h"

namespace
{
    constexpr int MAP_W = 32;
    constexpr int MAP_H = 32;
    constexpr int TILE_VARIANTS_W = 8;
    constexpr int TILE_VARIANTS_H = 4;
    constexpr int TILE_COUNT = TILE_VARIANTS_W * TILE_VARIANTS_H;

    constexpr bn::fixed SCROLL_DX = 0.25;
    constexpr bn::fixed SCROLL_DY = 0.15;

    alignas(8) bn::tile g_tile_storage[TILE_COUNT];
    alignas(4) bn::regular_bg_map_cell g_map_cells[MAP_W * MAP_H];
    bn::array<bn::color, 16> g_palette_colors;

    bn::optional<bn::regular_bg_ptr> g_bg;
    bn::fixed g_scroll_x = 0;
    bn::fixed g_scroll_y = 0;

    int marble_color_index(int world_x, int world_y)
    {
        const bn::fixed x = world_x;
        const bn::fixed y = world_y;
        const bn::fixed wave = bn::sin(x / 14) + bn::cos(y / 11) + bn::sin((x + y) / 18);

        int level = (wave / 2).integer() + 2;

        if(level < 0)
        {
            level = 0;
        }
        else if(level > 5)
        {
            level = 5;
        }

        return 1 + level;
    }

    void fill_tile(bn::tile& tile, int world_tile_x, int world_tile_y)
    {
        const int origin_x = world_tile_x * 8;
        const int origin_y = world_tile_y * 8;

        for(int py = 0; py < 8; ++py)
        {
            uint32_t row = 0;

            for(int px = 0; px < 8; ++px)
            {
                const int color_index = marble_color_index(origin_x + px, origin_y + py);
                row |= uint32_t(color_index) << (px * 4);
            }

            tile.data[py] = row;
        }
    }

    void build_tiles()
    {
        for(int ty = 0; ty < TILE_VARIANTS_H; ++ty)
        {
            for(int tx = 0; tx < TILE_VARIANTS_W; ++tx)
            {
                const int tile_index = ty * TILE_VARIANTS_W + tx;
                fill_tile(g_tile_storage[tile_index], tx, ty);
            }
        }
    }

    void build_map_cells()
    {
        for(int my = 0; my < MAP_H; ++my)
        {
            for(int mx = 0; mx < MAP_W; ++mx)
            {
                const int tile_index = (mx % TILE_VARIANTS_W) + (my % TILE_VARIANTS_H) * TILE_VARIANTS_W;
                bn::regular_bg_map_cell_info info(0);
                info.set_tile_index(tile_index);
                info.set_palette_id(0);
                g_map_cells[my * MAP_W + mx] = info.cell();
            }
        }
    }

    void init_palette_colors()
    {
        // Index 0 is transparent on BG layers — match the deepest blue so gaps never read as black.
        g_palette_colors[0] = bn::color(4, 10, 22);
        g_palette_colors[1] = bn::color(4, 10, 22);
        g_palette_colors[2] = bn::color(6, 14, 26);
        g_palette_colors[3] = bn::color(8, 18, 28);
        g_palette_colors[4] = bn::color(10, 20, 30);
        g_palette_colors[5] = bn::color(14, 24, 31);
        g_palette_colors[6] = bn::color(18, 28, 31);
        g_palette_colors[7] = bn::color(8, 14, 24);
    }
}

void battle_backdrop_init()
{
    if(g_bg)
    {
        return;
    }

    bn::backdrop::set_color(bn::color(4, 10, 22));
    bn::bg_tiles::set_allow_offset(false);

    init_palette_colors();
    build_tiles();
    build_map_cells();

    const bn::regular_bg_tiles_item tiles_item(
        bn::span<const bn::tile>(g_tile_storage, TILE_COUNT),
        bn::bpp_mode::BPP_4);
    const bn::regular_bg_map_item map_item(g_map_cells[0], bn::size(MAP_W, MAP_H));
    const bn::bg_palette_item palette_item(
        bn::span<const bn::color>(g_palette_colors.data(), g_palette_colors.size()),
        bn::bpp_mode::BPP_4);
    const bn::regular_bg_item bg_item(tiles_item, palette_item, map_item);

    g_bg = bg_item.create_bg(0, 0);
    g_bg->set_priority(3);
    g_bg->set_z_order(-32767);
    g_bg->set_visible(true);
}

void battle_backdrop_tick()
{
    if(! g_bg)
    {
        battle_backdrop_init();
    }

    g_scroll_x += SCROLL_DX;
    g_scroll_y += SCROLL_DY;
    g_bg->set_x(g_scroll_x);
    g_bg->set_y(g_scroll_y);
}

void battle_backdrop_set_visible(bool visible)
{
    if(g_bg)
    {
        g_bg->set_visible(visible);
    }
}
