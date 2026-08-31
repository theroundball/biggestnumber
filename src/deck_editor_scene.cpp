#include "menu_scenes.h"

#include "bn_array.h"
#include "bn_color.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_span.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_shape_size.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_string.h"
#include "bn_tile.h"
#include "bn_vector.h"

#include "battle_backdrop.h"

#include "campaign.h"
#include "card.h"
#include "card_data.h"
#include "card_instance.h"
#include "common_variable_8x8_sprite_font.h"
#include "common_variable_8x16_sprite_font.h"
#include "game_helpers.h"
#include "game_types.h"
#include "longsleeve_pick_scene.h"
#include "save_data.h"
#include "ui_common.h"
#include "ui_inspect.h"

namespace
{
    constexpr int HEADER_Y = -72;
    constexpr int ACTIONS_ITEM_Y = -20;
    constexpr int ACTIONS_SELECTOR_X = -100;
    constexpr int ACTIONS_LINE_HEIGHT = 16;
    constexpr int ACTIONS_ROW_COUNT = 3;
    constexpr int STATUS_ITEM_Y = -52;
    constexpr int STATUS_LINE_HEIGHT = 14;

    // Prefer an upgraded owned copy so pip badges show when any copy has upgrades.
    const CardInstance* catalog_display_instance(const SaveData& save, CardType type)
    {
        const CardInstance* fallback = nullptr;

        for(uint8_t id = 0; id < save.instance_pool.count; ++id)
        {
            const CardInstance& entry = save.instance_pool.entries[id];

            if(entry.base != type)
            {
                continue;
            }

            if(instance_has_upgrades(entry))
            {
                return &entry;
            }

            if(!fallback)
            {
                fallback = &entry;
            }
        }

        return fallback;
    }

    const char* trinket_display_name(TrinketType type)
    {
        switch(type)
        {
        case TrinketType::MOREL:
            return "Morel";
        case TrinketType::LUCKY_SEVENS:
            return "Lucky 7";
        case TrinketType::ECHO:
            return "Echo";
        case TrinketType::GET_WITH_THE_TIMES:
            return "Get w/ Times";
        case TrinketType::PRIME_TIME:
            return "Prime Time";
        case TrinketType::FIBONACCI:
            return "Fibonacci";
        case TrinketType::STAIRCASE:
            return "Staircase";
        case TrinketType::LONGSLEEVES:
            return "Longsleeves";
        default:
            return "none";
        }
    }

    void build_trinket_options(const SaveData& save, const SavedDeck& deck, bn::vector<TrinketType, 8>& out)
    {
        out.clear();
        out.push_back(TrinketType::NONE);

        if(saved_deck_unrestricted_build(deck))
        {
            for(int index = int(TrinketType::NONE) + 1; index < int(TrinketType::COUNT); ++index)
            {
                out.push_back(static_cast<TrinketType>(index));
            }

            return;
        }

        for(int index = int(TrinketType::NONE) + 1; index < int(TrinketType::COUNT); ++index)
        {
            if(save.trinket_owned[index] != 0)
            {
                out.push_back(static_cast<TrinketType>(index));
            }
        }
    }

    void cycle_trinket_slot(SavedDeck& deck, int slot, const SaveData& save, int deck_index)
    {
        bn::vector<TrinketType, 8> options;
        build_trinket_options(save, deck, options);

        if(options.empty())
        {
            deck.trinkets[slot] = uint8_t(TrinketType::NONE);
            saved_deck_clear_longsleeves(deck);
            return;
        }

        const TrinketType previous = static_cast<TrinketType>(deck.trinkets[slot]);
        int current_index = 0;

        for(int index = 0; index < options.size(); ++index)
        {
            if(options[index] == previous)
            {
                current_index = index;
                break;
            }
        }

        const int next_index = (current_index + 1) % options.size();
        const TrinketType next = options[next_index];
        deck.trinkets[slot] = uint8_t(next);

        if(previous == TrinketType::LONGSLEEVES && next != TrinketType::LONGSLEEVES)
        {
            saved_deck_clear_longsleeves(deck);
        }

        if(next == TrinketType::LONGSLEEVES)
        {
            if(deck_index < 0)
            {
                deck.trinkets[slot] = uint8_t(previous);
                return;
            }

            SaveData& save_mut = save_data_mut();
            bn::array<uint8_t, 2> picks = {
                deck.longsleeve_instance_ids[0],
                deck.longsleeve_instance_ids[1],
            };

            if(!run_longsleeve_deck_pick_scene(save_mut, deck_index, &deck, picks))
            {
                deck.trinkets[slot] = uint8_t(previous);
                saved_deck_clear_longsleeves(deck);
                return;
            }

            deck.longsleeve_instance_ids[0] = picks[0];
            deck.longsleeve_instance_ids[1] = picks[1];
            save_data_write();
        }
    }

    bn::string<24> trinket_slot_label(int slot_index, const SavedDeck& deck)
    {
        bn::string<24> line = slot_index == 0 ? "Trinket 1: " : "Trinket 2: ";
        line.append(trinket_display_name(static_cast<TrinketType>(deck.trinkets[slot_index])));
        return line;
    }

    // Vertical catalog grid: two full rows. Pool matches visible rows only —
    // extra pooled peek cards blow GBA OBJ tile VRAM (each card = body + 2 accents).
    constexpr int GRID_COLS = 5;
    constexpr int GRID_FULL_ROWS = 2;
    constexpr int GRID_ROW_PITCH = 64; // card height; tight so two rows clear the screen bottom
    constexpr int GRID_COL_PITCH = 42; // 40px display width + 2px gap
    constexpr int GRID_TOP = -54;      // top of the two fully-visible rows
    constexpr int GRID_BOTTOM = GRID_TOP + GRID_FULL_ROWS * GRID_ROW_PITCH; // 74; ~6px above screen edge
    constexpr int GRID_POOL_SIZE = GRID_COLS * GRID_FULL_ROWS;
    constexpr int SCROLL_RAIL_X = 112;
    constexpr int COUNT_OVERLAY_Y_OFFSET = 28;

    constexpr bn::array<bn::color, 16> SCROLL_RAIL_COLORS = {
        bn::color(0, 0, 0), bn::color(8, 8, 10), bn::color(24, 24, 28), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
    };

    constexpr bn::sprite_palette_item SCROLL_RAIL_PALETTE(
        bn::span<const bn::color>(SCROLL_RAIL_COLORS.data(), SCROLL_RAIL_COLORS.size()),
        bn::bpp_mode::BPP_4);

    uint32_t solid_tile_row(int color_index)
    {
        uint32_t row = 0;

        for(int px = 0; px < 8; ++px)
        {
            row |= uint32_t(color_index) << (px * 4);
        }

        return row;
    }

    void paint_solid_tiles(bn::sprite_tiles_ptr& tiles, int color_index)
    {
        auto vram = tiles.vram();
        auto* tile_span = vram.get();

        if(! tile_span)
        {
            return;
        }

        const uint32_t fill = solid_tile_row(color_index);

        for(int tile_index = 0; tile_index < tile_span->size(); ++tile_index)
        {
            bn::tile& tile = (*tile_span)[tile_index];

            for(int row_index = 0; row_index < 8; ++row_index)
            {
                tile.data[row_index] = fill;
            }
        }
    }

    // Thumb-only scroll position indicator (no track background).
    class CatalogScrollRail
    {
    public:
        CatalogScrollRail() :
            _thumb_tiles(bn::sprite_tiles_ptr::allocate(2, bn::bpp_mode::BPP_4)),
            _palette(bn::sprite_palette_ptr::create(SCROLL_RAIL_PALETTE)),
            _thumb(bn::sprite_ptr::create(SCROLL_RAIL_X, 0, bn::sprite_shape_size(8, 16),
                                          _thumb_tiles, _palette))
        {
            paint_solid_tiles(_thumb_tiles, 2);
            _thumb.set_z_order(-1);
            set_visible(false);
        }

        void set_visible(bool visible)
        {
            _thumb.set_visible(visible);
        }

        void update(int scroll_y, int max_scroll, int track_top, int track_height)
        {
            if(max_scroll <= 0 || track_height < 16)
            {
                set_visible(false);
                return;
            }

            set_visible(true);

            constexpr int thumb_h = 16;
            const int travel = track_height - thumb_h;
            const int thumb_top = track_top + (scroll_y * travel) / max_scroll;
            _thumb.set_position(SCROLL_RAIL_X, thumb_top + thumb_h / 2);
        }

    private:
        bn::sprite_tiles_ptr _thumb_tiles;
        bn::sprite_palette_ptr _palette;
        bn::sprite_ptr _thumb;
    };

    int grid_left_x()
    {
        const int span = (GRID_COLS - 1) * GRID_COL_PITCH + game_layout::CARD_DISPLAY_WIDTH;
        return -span / 2;
    }

    int slot_row(int slot_index)
    {
        return slot_index / GRID_COLS;
    }

    int slot_col(int slot_index)
    {
        return slot_index % GRID_COLS;
    }

    int row_count_for(int slot_count)
    {
        return slot_count <= 0 ? 0 : (slot_count + GRID_COLS - 1) / GRID_COLS;
    }

    int max_scroll_for(int slot_count)
    {
        const int rows = row_count_for(slot_count);
        const int extra = rows - GRID_FULL_ROWS;
        return extra > 0 ? extra * GRID_ROW_PITCH : 0;
    }

    void update_grid_scroll_target(int cursor, int slot_count, int& target_scroll_y)
    {
        const int max_scroll = max_scroll_for(slot_count);
        const int row = slot_row(cursor);
        const int row_top = row * GRID_ROW_PITCH;
        const int window = GRID_FULL_ROWS * GRID_ROW_PITCH;

        if(row_top < target_scroll_y)
        {
            target_scroll_y = row_top;
        }
        else if(row_top + GRID_ROW_PITCH > target_scroll_y + window)
        {
            target_scroll_y = row_top + GRID_ROW_PITCH - window;
        }

        if(target_scroll_y < 0)
        {
            target_scroll_y = 0;
        }
        else if(target_scroll_y > max_scroll)
        {
            target_scroll_y = max_scroll;
        }
    }

    void move_grid_cursor(int dx, int dy, int steps, int slot_count, int& cursor, int& target_scroll_y)
    {
        if(slot_count <= 0 || steps <= 0)
        {
            return;
        }

        for(int step = 0; step < steps; ++step)
        {
            int row = slot_row(cursor);
            int col = slot_col(cursor);
            const int rows = row_count_for(slot_count);

            col += dx;
            row += dy;

            if(col < 0)
            {
                col = 0;
            }
            else if(col >= GRID_COLS)
            {
                col = GRID_COLS - 1;
            }

            if(row < 0)
            {
                row = 0;
            }
            else if(row >= rows)
            {
                row = rows - 1;
            }

            int next = row * GRID_COLS + col;

            if(next >= slot_count)
            {
                next = slot_count - 1;
            }

            if(next == cursor)
            {
                break;
            }

            cursor = next;
        }

        update_grid_scroll_target(cursor, slot_count, target_scroll_y);
    }

    bool side_panel_active(int panel_slide)
    {
        return panel_slide > 0;
    }

    void draw_status_panel(SceneText& scene_text, int offset_x, const SaveData& save)
    {
        bn::string<32> wins_line = "Wins ";
        wins_line.append(bn::to_string<8>(save.total_wins));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y, wins_line);

        bn::string<32> paper_line = "Sticker paper ";
        paper_line.append(bn::to_string<8>(save.sticker_paper));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y + STATUS_LINE_HEIGHT, paper_line);

        bn::string<32> trinket_line = "Trinket in ";
        trinket_line.append(bn::to_string<4>(campaign_wins_until_trinket(save)));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y + STATUS_LINE_HEIGHT * 2, trinket_line);

        bn::string<32> record_line = "BN record ";
        record_line.append(bn::to_string<12>(save.biggest_number_record));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y + STATUS_LINE_HEIGHT * 3, record_line);

        bn::string<32> same_line = "SN ";
        same_line.append(bn::to_string<8>(save.same_number_wins));
        same_line.append("  N=");
        same_line.append(bn::to_string<8>(save.same_number_target));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y + STATUS_LINE_HEIGHT * 4, same_line);

        bn::string<32> library_line = "Library ";
        library_line.append(bn::to_string<8>(campaign_library_total_cards(save)));
        scene_text.draw_centered_line(offset_x, STATUS_ITEM_Y + STATUS_LINE_HEIGHT * 5, library_line);
    }

    int build_catalog_slot_count(const SaveData& save, const SavedDeck& working,
                                 bn::array<int, int(CardType::COUNT)>& slot_types)
    {
        int slot_count = 0;

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = CardType(type_index);

            if(card_meta(type).max_copies <= 0)
            {
                continue;
            }

            if(saved_deck_unrestricted_build(working) ||
               library_total_owned(save, type) > 0 || working.counts[type_index] > 0)
            {
                slot_types[slot_count] = type_index;
                ++slot_count;
            }
        }

        return slot_count;
    }

    bool try_save_working_deck(SavedDeck& working, SaveData& save, int deck_index)
    {
        saved_deck_trim_to_max(working, DECK_MAX_CARDS);

        const int total = saved_deck_total_cards(working);

        if(total < DECK_MIN_CARDS)
        {
            return false;
        }

        int write_index = deck_index;

        if(write_index < 0 && saved_deck_unrestricted_build(working))
        {
            const int existing = save_data_unrestricted_deck_index(save);

            if(existing >= 0)
            {
                write_index = existing;
            }
        }

        if(deck_index < 0)
        {
            if(!saved_deck_unrestricted_build(working))
            {
                saved_deck_assign_unique_name(working, save, -1);
            }
        }
        else
        {
            saved_deck_sanitize_name(working);
        }

        if(write_index >= 0)
        {
            save.decks[write_index] = working;
            campaign_set_active_deck(save, write_index);
        }
        else if(save.deck_count < MAX_SAVED_DECKS)
        {
            write_index = save.deck_count;
            save.decks[write_index] = working;
            // Count first: campaign_set_active_deck rejects indices past deck_count.
            ++save.deck_count;
            campaign_set_active_deck(save, write_index);
        }
        else
        {
            return false;
        }

        save_data_validate(save);
        save_data_write();
        return true;
    }
}

DeckEditorResult run_deck_editor_scene(int deck_index, bool create_debug_deck, bool ephemeral_session)
{
    wait_for_keypad_clear();

    DeckEditorResult result;

    SaveData& save = save_data_mut();
    SavedDeck working;

    if(deck_index >= 0)
    {
        working = save.decks[deck_index];
    }
    else if(create_debug_deck)
    {
        working = saved_deck_make_debug();
    }
    else
    {
        working = saved_deck_make_new();
    }

    bn::sprite_text_generator title_generator(common::variable_8x16_sprite_font);
    bn::sprite_text_generator body_generator(common::variable_8x8_sprite_font);
    bn::sprite_text_generator count_generator(common::variable_8x16_sprite_font);

    SceneText scene_text(title_generator);
    SceneText header_text(title_generator);
    SceneText quantity_text(count_generator);
    SelectorGlyph actions_selector(title_generator, ACTIONS_SELECTOR_X);
    CatalogScrollRail scroll_rail;

    bn::vector<bn::sprite_ptr, 64> inspect_sprites;
    bn::array<Card, GRID_POOL_SIZE> catalog_cards;

    for(Card& card : catalog_cards)
    {
        card.set_visible(false);
    }

    bool inspecting = false;
    int inspect_shown_for = -1;

    bn::array<int, int(CardType::COUNT)> catalog_slot_types;
    int catalog_slot_count = build_catalog_slot_count(save, working, catalog_slot_types);
    int catalog_cursor = 0;

    int scroll_y = 0;
    int target_scroll_y = 0;
    update_grid_scroll_target(catalog_cursor, catalog_slot_count, target_scroll_y);
    scroll_y = target_scroll_y;

    DirectionRepeatState horizontal_repeat;
    DirectionRepeatState vertical_repeat;
    DirectionRepeatState actions_repeat;

    int panel_slide = 0;
    int panel_slide_target = 0;
    int panel_side = 1;
    int actions_cursor = 0;
    int save_notice_frames = 0;
    bn::string<24> save_notice;

    const int grid_left = grid_left_x();
    const int track_top = GRID_TOP;
    const int track_height = GRID_FULL_ROWS * GRID_ROW_PITCH;

    while(true)
    {
        catalog_slot_count = build_catalog_slot_count(save, working, catalog_slot_types);

        if(catalog_slot_count <= 0)
        {
            catalog_cursor = 0;
        }
        else if(catalog_cursor >= catalog_slot_count)
        {
            catalog_cursor = catalog_slot_count - 1;
        }

        if(! inspecting)
        {
            // L/R carousel: left = Save/trinkets, right = campaign stats, center = catalog.
            if(bn::keypad::l_pressed())
            {
                if(panel_slide_target > 0 && panel_side < 0)
                {
                    panel_slide_target = 0;
                }
                else
                {
                    panel_side = -1;
                    panel_slide_target = game_layout::PANEL_WIDTH;

                    for(Card& card : catalog_cards)
                    {
                        release_card_display_tiles(card);
                    }
                }
            }
            else if(bn::keypad::r_pressed())
            {
                if(panel_slide_target > 0 && panel_side > 0)
                {
                    panel_slide_target = 0;
                }
                else
                {
                    panel_side = 1;
                    panel_slide_target = game_layout::PANEL_WIDTH;

                    for(Card& card : catalog_cards)
                    {
                        release_card_display_tiles(card);
                    }
                }
            }
        }

        move_toward_int(panel_slide, panel_slide_target, game_layout::PANEL_SLIDE_SPEED);

        const bool panel_open_or_opening = panel_slide > 0 || panel_slide_target > 0;
        const bool on_actions = side_panel_active(panel_slide) && panel_side < 0;
        const bool on_status = side_panel_active(panel_slide) && panel_side > 0;

        const int catalog_offset_x = -panel_side * panel_slide;
        const int actions_offset_x = panel_side * (game_layout::PANEL_WIDTH - panel_slide);

        move_toward(scroll_y, target_scroll_y, game_layout::SCROLL_SPEED);

        battle_backdrop_set_visible(true);

        if(inspecting)
        {
            // Tear down catalog chrome/cards, then show only the inspect card + text.
            scene_text.clear();
            quantity_text.clear();
            actions_selector.set_visible(false);
            scroll_rail.set_visible(false);
            header_text.clear();

            for(Card& card : catalog_cards)
            {
                release_card_display_tiles(card);
            }

            if(catalog_slot_count > 0)
            {
                const CardType type = CardType(catalog_slot_types[catalog_cursor]);
                const CardInstance* instance = catalog_display_instance(save, type);
                show_inspect_card(catalog_cards[0], type, instance, &body_generator);

                if(inspect_shown_for != catalog_cursor)
                {
                    draw_card_inspect(type, title_generator, body_generator, inspect_sprites,
                                      inspect_layout::TITLE_Y, instance);
                    inspect_shown_for = catalog_cursor;
                }
            }
        }

        if(! inspecting)
        {
            scene_text.clear();
            quantity_text.clear();

            for(Card& card : catalog_cards)
            {
                card.set_visible(false);
            }

            actions_selector.set_visible(false);

            header_text.clear();

            bn::string<24> header_label;

            if(deck_index < 0 && working.name[0] == '\0')
            {
                header_label = "New Deck  ";
            }
            else if(saved_deck_unrestricted_build(working))
            {
                header_label = "Debug  ";
            }
            else
            {
                header_label = saved_deck_display_name(working);
                header_label.append("  ");
            }

            header_label.append(bn::to_string<4>(saved_deck_total_cards(working)));
            header_label.append("/");
            header_label.append(bn::to_string<4>(DECK_MAX_CARDS));

            if(catalog_slot_count > 0 && !saved_deck_unrestricted_build(working))
            {
                const CardType selected = CardType(catalog_slot_types[catalog_cursor]);
                header_label.append("  Lib ");
                header_label.append(bn::to_string<4>(library_total_owned(save, selected)));
            }

            header_text.draw_centered_line(0, HEADER_Y, header_label);

            // Drop catalog tiles as soon as L/R starts opening — keep VRAM free for the
            // actions panel text. Only rebuild the grid when the panel is fully closed.
            const bool show_catalog_grid = panel_slide == 0 && panel_slide_target == 0;

            if(show_catalog_grid)
            {
                int pool_slot = 0;
                const int max_scroll = max_scroll_for(catalog_slot_count);

                // Only fully visible rows — peek bands stay empty to save tile VRAM.
                const int clip_top = GRID_TOP;
                const int clip_bottom = GRID_BOTTOM;

                for(int slot_index = 0; slot_index < catalog_slot_count; ++slot_index)
                {
                    const int type_index = catalog_slot_types[slot_index];
                    const int row = slot_row(slot_index);
                    const int col = slot_col(slot_index);
                    const int card_x = grid_left + col * GRID_COL_PITCH + catalog_offset_x;
                    const int card_y = GRID_TOP + row * GRID_ROW_PITCH - scroll_y;
                    const int card_bottom = card_y + 64;

                    if(card_bottom <= clip_top || card_y >= clip_bottom)
                    {
                        continue;
                    }

                    if(card_x >= 120 || card_x + game_layout::CARD_DISPLAY_WIDTH <= -120)
                    {
                        continue;
                    }

                    if(pool_slot >= catalog_cards.size())
                    {
                        break;
                    }

                    const bool selected = slot_index == catalog_cursor;
                    const int raise = selected ? game_layout::SELECTED_RAISE / 2 : 0;
                    const int count = working.counts[type_index];

                    Card& card = catalog_cards[pool_slot];
                    const CardType type = CardType(type_index);
                    card.set_type(type);
                    card.set_position(card_x, card_y - raise);
                    card.set_visible(true);

                    if(card_data(type).text_only)
                    {
                        card.sync_face_labels(&body_generator, nullptr, CardRef{type, NO_INSTANCE},
                                              catalog_display_instance(save, type));
                    }
                    else
                    {
                        card.clear_face_labels();
                    }

                    // Pips only on the selected card — each pip string costs font tiles.
                    if(selected)
                    {
                        card.set_upgrade_pips(&body_generator, catalog_display_instance(save, type));
                    }
                    else
                    {
                        card.clear_upgrade_pips();
                    }

                    ++pool_slot;

                    if(count > 0)
                    {
                        bn::string<8> count_label = "x";
                        count_label.append(bn::to_string<4>(count));
                        quantity_text.draw_centered_line(
                            card_x + game_layout::CARD_BODY_WIDTH / 2,
                            card_y - raise + COUNT_OVERLAY_Y_OFFSET, count_label);
                    }
                }

                quantity_text.set_z_order(game_layout::MARKER_Z_ORDER);
                quantity_text.set_bg_priority(0);

                for(int unused = pool_slot; unused < catalog_cards.size(); ++unused)
                {
                    release_card_display_tiles(catalog_cards[unused]);
                }

                scroll_rail.update(scroll_y, max_scroll, track_top, track_height);
            }
            else
            {
                for(Card& card : catalog_cards)
                {
                    release_card_display_tiles(card);
                }

                scroll_rail.set_visible(false);
            }

            if(panel_slide > 0)
            {
                if(panel_side < 0)
                {
                    scene_text.draw_centered_line(actions_offset_x, ACTIONS_ITEM_Y,
                                                  ephemeral_session ? "Start" : "Save");
                    scene_text.draw_centered_line(actions_offset_x, ACTIONS_ITEM_Y + ACTIONS_LINE_HEIGHT,
                                                  trinket_slot_label(0, working));
                    scene_text.draw_centered_line(actions_offset_x,
                                                  ACTIONS_ITEM_Y + ACTIONS_LINE_HEIGHT * 2,
                                                  trinket_slot_label(1, working));

                    if(save_notice_frames > 0)
                    {
                        scene_text.draw_centered_line(actions_offset_x, ACTIONS_ITEM_Y + ACTIONS_LINE_HEIGHT * 3,
                                                      save_notice);
                    }

                    actions_selector.set_position(actions_offset_x + ACTIONS_SELECTOR_X,
                                                  ACTIONS_ITEM_Y + actions_cursor * ACTIONS_LINE_HEIGHT);
                    actions_selector.set_visible(true);
                }
                else
                {
                    draw_status_panel(scene_text, actions_offset_x, save);
                }
            }
        }

        // D-pad still browses the catalog while inspect is open.
        if((! inspecting && ! on_actions && ! on_status && ! panel_open_or_opening) ||
           (inspecting && ! on_actions && ! on_status && catalog_slot_count > 0))
        {
            // Poll even while the grid is sliding so hold-acceleration keeps
            // ramping (especially vertical, which always kicks off a scroll).
            int vertical_direction = 0;
            bool vertical_triggered = false;
            int vertical_steps = 1;
            int horizontal_direction = 0;
            bool horizontal_triggered = false;
            int horizontal_steps = 1;

            poll_direction_repeat(DirectionAxis::VERTICAL, vertical_repeat, false,
                                  vertical_direction, vertical_triggered, vertical_steps);
            poll_direction_repeat(DirectionAxis::HORIZONTAL, horizontal_repeat, false,
                                  horizontal_direction, horizontal_triggered, horizontal_steps);

            // Prefer vertical when both fire so diagonals don't skip oddly.
            if(vertical_triggered)
            {
                move_grid_cursor(0, vertical_direction, vertical_steps, catalog_slot_count, catalog_cursor,
                                 target_scroll_y);
            }
            else if(horizontal_triggered)
            {
                move_grid_cursor(horizontal_direction, 0, horizontal_steps, catalog_slot_count, catalog_cursor,
                                 target_scroll_y);
            }

            if(!inspecting && catalog_slot_count > 0)
            {
                const CardType selected = CardType(catalog_slot_types[catalog_cursor]);

                if(bn::keypad::a_pressed())
                {
                    saved_deck_try_add_card(save, deck_index, working, selected);
                }
                else if(bn::keypad::b_pressed())
                {
                    saved_deck_try_remove_card(working, selected);
                }
            }
        }

        if(! inspecting && on_actions)
        {
            int direction = 0;
            bool direction_triggered = false;
            int direction_steps = 1;
            poll_direction_repeat(DirectionAxis::VERTICAL, actions_repeat, false, direction, direction_triggered,
                                  direction_steps);

            if(direction_triggered)
            {
                actions_cursor += direction * direction_steps;

                if(actions_cursor < 0)
                {
                    actions_cursor = 0;
                }
                else if(actions_cursor >= ACTIONS_ROW_COUNT)
                {
                    actions_cursor = ACTIONS_ROW_COUNT - 1;
                }
            }

            if(bn::keypad::a_pressed())
            {
                if(actions_cursor == 0)
                {
                    if(ephemeral_session)
                    {
                        if(saved_deck_total_cards(working) >= DECK_MIN_CARDS)
                        {
                            campaign_set_ephemeral_battle_deck(working);
                            result.ephemeral_confirmed = true;
                            result.next = MenuSceneResult::STAY;
                            return result;
                        }

                        save_notice = "Need 1+ cards";
                    }
                    else if(try_save_working_deck(working, save, deck_index))
                    {
                        result.next = MenuSceneResult::MAIN_MENU;
                        return result;
                    }

                    if(saved_deck_total_cards(working) < DECK_MIN_CARDS)
                    {
                        save_notice = "Need 1+ cards";
                    }
                    else if(save.deck_count >= MAX_SAVED_DECKS && deck_index < 0 &&
                            save_data_unrestricted_deck_index(save) < 0)
                    {
                        save_notice = "Max decks";
                    }
                    else
                    {
                        save_notice = "Save failed";
                    }

                    save_notice_frames = 120;
                }
                else
                {
                    cycle_trinket_slot(working, actions_cursor - 1, save, deck_index);
                }
            }
        }

        // Select only — Down is grid navigation in the deck builder.
        const bool inspect_toggle =
            bn::keypad::select_pressed() && !on_actions && !on_status && !panel_open_or_opening &&
            catalog_slot_count > 0;

        if(inspect_toggle)
        {
            inspecting = !inspecting;

            if(!inspecting)
            {
                inspect_sprites.clear();
                inspect_shown_for = -1;
                hide_inspect_card(catalog_cards[0]);
            }
        }

        if(bn::keypad::b_pressed() && (inspecting || panel_open_or_opening))
        {
            if(inspecting)
            {
                inspecting = false;
                inspect_sprites.clear();
                inspect_shown_for = -1;
                hide_inspect_card(catalog_cards[0]);
            }
            else
            {
                panel_slide_target = 0;
            }
        }

        // Start leaves without saving (B is used for removing copies).
        if(bn::keypad::start_pressed() && ! inspecting && ! panel_open_or_opening)
        {
            result.next = MenuSceneResult::MAIN_MENU;
            return result;
        }

        battle_backdrop_tick();

        if(save_notice_frames > 0)
        {
            --save_notice_frames;
        }

        bn::core::update();
    }
}
