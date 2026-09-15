#include "overworld_field.h"

#include "bn_backdrop.h"
#include "bn_bg_palette_item.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bg_tiles.h"
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
    constexpr int SCREEN_HALF_W = 120;
    constexpr int SCREEN_HALF_H = 80;

    alignas(8) bn::tile g_green_tile;
    alignas(4) bn::regular_bg_map_cell g_map_cells[MAP_W * MAP_H];
    bn::array<bn::color, 16> g_palette_colors;

    bn::optional<bn::regular_bg_ptr> g_bg;

    void init_palette_colors()
    {
        g_palette_colors[0] = bn::color(0, 0, 0);
        g_palette_colors[1] = bn::color(8, 20, 8);
    }

    void fill_green_tile()
    {
        for(int py = 0; py < 8; ++py)
        {
            uint32_t row = 0;

            for(int px = 0; px < 8; ++px)
            {
                row |= uint32_t(1) << (px * 4);
            }

            g_green_tile.data[py] = row;
        }
    }

    void build_map_cells()
    {
        for(int my = 0; my < MAP_H; ++my)
        {
            for(int mx = 0; mx < MAP_W; ++mx)
            {
                bn::regular_bg_map_cell_info info(0);
                info.set_tile_index(0);
                info.set_palette_id(0);
                g_map_cells[my * MAP_W + mx] = info.cell();
            }
        }
    }
}

void overworld_field_init()
{
    if(g_bg)
    {
        return;
    }

    bn::bg_tiles::set_allow_offset(false);
    init_palette_colors();
    fill_green_tile();
    build_map_cells();

    const bn::regular_bg_tiles_item tiles_item(
        bn::span<const bn::tile>(&g_green_tile, 1),
        bn::bpp_mode::BPP_4);
    const bn::regular_bg_map_item map_item(g_map_cells[0], bn::size(MAP_W, MAP_H));
    const bn::bg_palette_item palette_item(
        bn::span<const bn::color>(g_palette_colors.data(), g_palette_colors.size()),
        bn::bpp_mode::BPP_4);
    const bn::regular_bg_item bg_item(tiles_item, palette_item, map_item);

    g_bg = bg_item.create_bg(0, 0);
    g_bg->set_priority(3);
    g_bg->set_z_order(-32766);
    g_bg->set_visible(false);
}

void overworld_field_set_visible(bool visible)
{
    if(!g_bg)
    {
        overworld_field_init();
    }

    g_bg->set_visible(visible);
}

void overworld_field_set_camera(bn::fixed camera_x, bn::fixed camera_y)
{
    if(!g_bg)
    {
        overworld_field_init();
    }

    g_bg->set_x(-camera_x - SCREEN_HALF_W);
    g_bg->set_y(-camera_y - SCREEN_HALF_H);
}
