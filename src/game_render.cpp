#include "game_context.h"

#include "bn_blending.h"

#include "combo_system.h"
#include "game_helpers.h"
#include "game_ui.h"
#include "score_pop_system.h"
#include "swivel_system.h"
#include "trinket_system.h"

namespace
{
    constexpr bn::array<int, 4> FADE_BAND_X = {-116, -108, 108, 116};
    constexpr int VIEW_LEFT = -120;
    constexpr int VIEW_RIGHT = 120;
    constexpr int FADE_EDGE_INSET = 12;

    int swap_eased_shift(int spacing, int frame)
    {
        if(game_layout::SWAP_FRAMES <= 1)
        {
            return spacing;
        }

        const int progress = frame * game_layout::SWAP_EASE_SCALE / (game_layout::SWAP_FRAMES - 1);
        const int eased = progress < game_layout::SWAP_EASE_SCALE / 2
            ? (2 * progress * progress / game_layout::SWAP_EASE_SCALE)
            : (game_layout::SWAP_EASE_SCALE -
               2 * (game_layout::SWAP_EASE_SCALE - progress) * (game_layout::SWAP_EASE_SCALE - progress) /
                   game_layout::SWAP_EASE_SCALE);

        return spacing * eased / game_layout::SWAP_EASE_SCALE;
    }

    void position_fade_bands(bn::array<GameFadeBand, 4>& bands, int panel_x)
    {
        for(int band_index = 0; band_index < bands.size(); ++band_index)
        {
            bands[band_index].set_position_x(FADE_BAND_X[band_index] + panel_x);
        }
    }

    void hand_row_fade_visibility(
        const GameContext& ctx,
        int hand_x,
        int scroll_x,
        int edge_shift,
        int main_x,
        int removal_shift,
        bool swapping_card,
        int swap_direction,
        int swap_shift,
        bool& has_left_cards,
        bool& has_right_cards)
    {
        has_left_cards = false;
        has_right_cards = false;

        const int removal_slot = ctx.active_removal_slot_index();
        const int visual_count = ctx.visual_hand_slot_count();

        for(int visual_index = 0; visual_index < visual_count; ++visual_index)
        {
            const int hand_index = ctx.hand_index_for_visual_slot(visual_index);

            if(hand_index < 0 || hand_index >= ctx.state.hand.size())
            {
                continue;
            }

            if(ctx.removing_card && ctx.removal_center_beat && ctx.removal_origin == PlayPresentOrigin::HAND &&
               ctx.removal_hand_index >= 0 && hand_index == ctx.removal_hand_index)
            {
                continue;
            }

            int card_x = hand_x + visual_index * game_layout::HAND_SPACING - scroll_x + edge_shift + main_x;

            if(ctx.removing_card && visual_index > removal_slot)
            {
                card_x -= removal_shift;
            }

            const int cursor_visual = ctx.visual_slot_for_hand_index(ctx.selected_card);

            if(swapping_card && visual_index == cursor_visual)
            {
                card_x += swap_direction * swap_shift;
            }
            else if(swapping_card && visual_index == cursor_visual + swap_direction)
            {
                card_x -= swap_direction * swap_shift;
            }

            const int card_right = card_x + game_layout::CARD_DISPLAY_WIDTH;

            if(card_x < VIEW_LEFT + FADE_EDGE_INSET)
            {
                has_left_cards = true;
            }

            if(card_right > VIEW_RIGHT - FADE_EDGE_INSET)
            {
                has_right_cards = true;
            }
        }
    }
}

void GameContext::tick_scroll()
{
        move_toward(scroll_x, target_scroll_x, game_layout::SCROLL_SPEED);
        move_toward(edge_shift, target_edge_shift, game_layout::SCROLL_SPEED);
        move_toward(hand_x, target_hand_x, game_layout::SCROLL_SPEED);

        if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE ||
           mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK ||
           mode == GameMode::DECK_SEARCH || mode == GameMode::SCRY)
        {
            const int row_spacing = mode == GameMode::SCRY ? game_layout::SCRY_SPACING : game_layout::GRAVE_SPACING;
            const int row_cursor = (side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
                ? browse_cursor
                : state.selection.cursor;

            target_row_scroll_x = target_row_scroll_index * row_spacing;
            move_toward(row_scroll_x, target_row_scroll_x, game_layout::SCROLL_SPEED);

            const int first_visible = row_scroll_x / row_spacing;

            if(row_cursor < first_visible || row_cursor >= first_visible + game_layout::VISIBLE_CARD_COUNT)
            {
                snap_row_scroll(row_spacing);
            }
        }


}

void GameContext::tick_card_raise()
{
    int count = 0;
    int cursor = 0;
    int selected_raise = 0;

    if(show_graveyard_layer() && mode != GameMode::GRAVEYARD_TARGET && mode != GameMode::GRAVEYARD_PICK)
    {
        count = active_browse_cards().size();
        cursor = browse_cursor;
        selected_raise = game_layout::GRAVEYARD_BROWSE_SELECTED_RAISE;
    }
    else if(mode == GameMode::NORMAL || mode == GameMode::DISCARD_TARGET)
    {
        count = state.hand.size();
        cursor = selected_card;
        selected_raise = game_layout::SELECTED_RAISE;
    }
    else if(mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK)
    {
        count = state.graveyard.size();
        cursor = state.selection.cursor;
        selected_raise = game_layout::GRAVEYARD_BROWSE_SELECTED_RAISE;
    }
    else if(mode == GameMode::DECK_SEARCH)
    {
        count = state.selection.deck_search_buffer.size();
        cursor = state.selection.cursor;
        selected_raise = game_layout::GRAVE_SELECTED_RAISE;
    }
    else if(mode == GameMode::SCRY)
    {
        count = state.selection.scry_buffer.size();
        cursor = state.selection.cursor;
        selected_raise = game_layout::SCRY_SELECT_RAISE;
    }
    else
    {
        return;
    }

    for(int card_index = 0; card_index < count; ++card_index)
    {
        int target = card_index == cursor ? selected_raise : 0;

        // Roll Over: keep the locked first pick raised so the pair is obvious.
        if(mode == GameMode::GRAVEYARD_TARGET &&
           state.selection.type == PendingActionType::GRAVEYARD_PAIR_SWAP &&
           state.selection.graveyard_swap_first >= 0 &&
           card_index == state.selection.graveyard_swap_first)
        {
            target = selected_raise;
        }

        ease_raise_toward(card_raise_offset[card_index], target);
    }
}

void GameContext::render_combo_frame(int main_x)
{
            for (int fade_index = 0; fade_index < 4; ++fade_index)
            {
                fade_bands[fade_index].set_visible(false);
            }

            const int frame = state.combo_cinematic.frame;
            const int count = state.combo_cinematic.card_count;
            const int spacing = 36;
            const int row_start = -(count * spacing) / 2;
            const int score_target_x = card_target_x_for_score_center(main_x);
            const int score_target_y = card_target_y_for_score_center();

            for (int card_index = 0; card_index < count; ++card_index)
            {
                const int gather_x = row_start + card_index * spacing + main_x;
                int card_x = gather_x;
                int card_y = game_layout::HAND_Y;

                if (frame < COMBO_GATHER_FRAMES)
                {
                    card_x = gather_x * (COMBO_GATHER_FRAMES - frame) / COMBO_GATHER_FRAMES;
                    combo_display[card_index].set_position(card_x, card_y);
                    combo_display[card_index].clear_visual();
                    combo_display[card_index].set_blending_enabled(false);
                }
                else
                {
                    const int exit_frame = frame - COMBO_GATHER_FRAMES;
                    const CardFlightSample flight = sample_card_exile_dissipate(
                        gather_x, game_layout::HAND_Y, score_target_x, score_target_y,
                        exit_frame, COMBO_EXIT_FRAMES);
                    card_x = flight.x;
                    card_y = flight.y;
                    combo_display[card_index].set_position(card_x, card_y);
                    combo_display[card_index].set_visual(flight.scale, 0);

                    if(flight.alpha < 1)
                    {
                        bn::blending::set_transparency_alpha(
                            flight.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : flight.alpha);
                        combo_display[card_index].set_blending_enabled(true);
                    }
                    else
                    {
                        combo_display[card_index].set_blending_enabled(false);
                    }
                }

                combo_display[card_index].set_type(state.combo_cinematic.cards[card_index]);
                combo_display[card_index].set_visible(true);
            }
}

void GameContext::sync_pair_swap_prompt()
{
    action_prompt_sprites.clear();

    if(mode != GameMode::GRAVEYARD_TARGET ||
       state.selection.type != PendingActionType::GRAVEYARD_PAIR_SWAP)
    {
        return;
    }

    const int swap_number = 4 - state.selection.remaining_picks;
    bn::string<32> line;

    if(state.selection.graveyard_swap_first < 0)
    {
        line = "Swap ";
        line += bn::to_string<4>(swap_number);
        line += "/3 - pick first";
    }
    else
    {
        line = "Swap ";
        line += bn::to_string<4>(swap_number);
        line += "/3 - pick second";
    }

    hud_mod_generator.set_center_alignment();
    hud_mod_generator.generate(0, -72, line, action_prompt_sprites);
}

void GameContext::render_graveyard_selection_frame(int main_x)
{
            const int grave_cursor = state.selection.cursor;
            const CardRowResult r = render_graveyard_view(main_x, grave_cursor);

            if(graveyard_card_fx_active)
            {
                const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;

                for(int slot = 0; slot < grave_row_display.size(); ++slot)
                {
                    if(first_visible + slot == graveyard_card_fx_index)
                    {
                        grave_row_display[slot].set_visible(false);
                    }
                }
            }

            if(r.cursor_slot >= 0)
            {
                const int grave_cursor_raise = grave_cursor < card_raise_offset.size()
                    ? card_raise_offset[grave_cursor] : 0;
                const int grave_cursor_wave = row_scroll_pair_raise(
                    grave_cursor, grave_cursor, row_scroll_x, target_row_scroll_x,
                    game_layout::GRAVE_SPACING, state.graveyard.size());
                const int card_x = r.row_start_x + r.cursor_slot * game_layout::GRAVE_SPACING - r.scroll_sub + main_x;
                const int selected_card_top = game_layout::GRAVEYARD_BROWSE_Y - grave_cursor_raise - grave_cursor_wave;
                const int marker_x = card_x + 16;
                const int marker_y = selected_card_top + 32;

                if(mode == GameMode::GRAVEYARD_TARGET &&
                   (state.selection.type == PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY ||
                    state.selection.type == PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT))
                {
                    x_marker.set_position(marker_x, marker_y);
                    x_marker.set_visible(true);
                }
            }

            // Roll Over: green check on the locked first card (may be off-cursor).
            if(mode == GameMode::GRAVEYARD_TARGET &&
               state.selection.type == PendingActionType::GRAVEYARD_PAIR_SWAP &&
               state.selection.graveyard_swap_first >= 0)
            {
                const int locked = state.selection.graveyard_swap_first;
                const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;
                const int locked_slot = locked - first_visible;

                if(locked_slot >= 0 && locked_slot < r.visible_count)
                {
                    const int locked_raise = locked < card_raise_offset.size() ? card_raise_offset[locked] : 0;
                    const int locked_x =
                        r.row_start_x + locked_slot * game_layout::GRAVE_SPACING - r.scroll_sub + main_x;
                    swap_lock_marker.set_position(locked_x + 16,
                                                  game_layout::GRAVEYARD_BROWSE_Y - locked_raise + 32);
                    swap_lock_marker.set_visible(true);
                }
            }

            if(mode == GameMode::GRAVEYARD_TARGET &&
               state.selection.type != PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY &&
               state.selection.type != PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT)
            {
                render_graveyard_exclude_marks(r, main_x, game_layout::GRAVEYARD_BROWSE_Y,
                                               state.selection.graveyard_exclude, grave_cursor);
            }

            sync_pair_swap_prompt();
}

void GameContext::render_deck_search_frame(int main_x)
{
            const int deck_search_count = state.selection.deck_search_buffer.size();
            const bn::span<const int> deck_search_raises(
                card_raise_offset.data(),
                deck_search_count < card_raise_offset.size()
                    ? deck_search_count
                    : card_raise_offset.size());
            const int deck_search_cursor = state.selection.cursor;

            const CardRowResult r = render_card_row(grave_row_display,
                bn::span<const CardRef>(state.selection.deck_search_buffer.data(), deck_search_count),
                deck_search_cursor, game_layout::GRAVE_SPACING, game_layout::GRAVE_Y,
                row_scroll_x, target_row_scroll_x, main_x, deck_search_raises,
                &state.instance_pool, &hud_count_generator);

            apply_row_fade_bands(fade_bands, game_layout::GRAVE_Y, r.has_left, r.has_right);
            position_fade_bands(fade_bands, main_x);
}

void GameContext::render_scry_frame(int main_x)
{
            const int swap_shift = swapping_card ? swap_eased_shift(game_layout::SCRY_SPACING, swap_frame) : 0;

            const int scry_count = state.selection.scry_buffer.size();
            const bn::span<const int> scry_raises(
                card_raise_offset.data(),
                scry_count < card_raise_offset.size() ? scry_count : card_raise_offset.size());
            const int scry_cursor = state.selection.cursor;

            const CardRowResult r = render_card_row(scry_display,
                                                    bn::span<const CardRef>(state.selection.scry_buffer.data(), scry_count),
                                                    scry_cursor, game_layout::SCRY_SPACING, game_layout::SCRY_Y,
                                                    row_scroll_x, target_row_scroll_x, main_x, scry_raises,
                                                    &state.instance_pool, &hud_count_generator);

            if(swapping_card && r.cursor_slot >= 0)
            {
                const int partner_index = scry_cursor + swap_direction;
                const int neighbor_slot = r.cursor_slot + swap_direction;

                if(partner_index >= 0 && partner_index < scry_count &&
                   neighbor_slot >= 0 && neighbor_slot < r.visible_count)
                {
                    const int cursor_x = r.row_start_x + r.cursor_slot * game_layout::SCRY_SPACING - r.scroll_sub + main_x;
                    const int neighbor_x = r.row_start_x + neighbor_slot * game_layout::SCRY_SPACING - r.scroll_sub + main_x;
                    const int scroll_wave_cursor = row_scroll_pair_raise(
                        scry_cursor, scry_cursor, row_scroll_x, target_row_scroll_x,
                        game_layout::SCRY_SPACING, scry_count);
                    const int scroll_wave_partner = row_scroll_pair_raise(
                        partner_index, scry_cursor, row_scroll_x, target_row_scroll_x,
                        game_layout::SCRY_SPACING, scry_count);
                    const int arc_cursor = hand_swap_wave_raise(
                        scry_cursor, scry_cursor, swap_direction, swapping_card, swap_frame);
                    const int arc_partner = hand_swap_wave_raise(
                        partner_index, scry_cursor, swap_direction, swapping_card, swap_frame);
                    const int cursor_y = game_layout::SCRY_Y - card_raise_offset[scry_cursor] -
                                         scroll_wave_cursor - arc_cursor;
                    const int neighbor_y = game_layout::SCRY_Y - card_raise_offset[partner_index] -
                                           scroll_wave_partner - arc_partner;

                    scry_display[r.cursor_slot].set_position(cursor_x + swap_direction * swap_shift, cursor_y);
                    scry_display[neighbor_slot].set_position(neighbor_x - swap_direction * swap_shift, neighbor_y);
                }
            }

            const int scry_cursor_raise = scry_cursor < card_raise_offset.size()
                ? card_raise_offset[scry_cursor] : 0;
            const int scry_cursor_wave = row_scroll_pair_raise(
                scry_cursor, scry_cursor, row_scroll_x, target_row_scroll_x,
                game_layout::SCRY_SPACING, scry_count);
            const int card_y = game_layout::SCRY_Y - scry_cursor_raise - scry_cursor_wave;
            apply_row_fade_bands(fade_bands, card_y, r.has_left, r.has_right);
            position_fade_bands(fade_bands, main_x);
}

void GameContext::render_hand_frame(int main_x, int swap_shift, int removal_shift)
{
        if(side_panel != SidePanel::GRAVEYARD && side_panel != SidePanel::EXILE)
        {
            int pool_slot = 0;
            const int visual_count = visual_hand_slot_count();
            const int cursor_visual = visual_slot_for_hand_index(selected_card);

            for(int visual_index = 0; visual_index < visual_count; ++visual_index)
            {
                const int hand_index = hand_index_for_visual_slot(visual_index);

                if(hand_index < 0 || hand_index >= state.hand.size())
                {
                    continue;
                }

                if(removing_card && removal_center_beat && removal_origin == PlayPresentOrigin::HAND &&
                   removal_hand_index >= 0 && hand_index == removal_hand_index)
                {
                    continue;
                }

                int card_x = hand_x + visual_index * game_layout::HAND_SPACING - scroll_x + edge_shift + main_x;
                card_x -= visual_index > active_removal_slot_index() ? removal_shift : 0;

                if(swapping_card && visual_index == cursor_visual)
                {
                    card_x += swap_direction * swap_shift;
                }
                else if(swapping_card && visual_index == cursor_visual + swap_direction)
                {
                    card_x -= swap_direction * swap_shift;
                }

                const bool card_visible = card_x < 120 && card_x + 40 > -120;

                if(!card_visible)
                {
                    continue;
                }

                if(pool_slot >= hand_display.size())
                {
                    break;
                }

                Card& card = hand_display[pool_slot];
                card.set_type(state.hand[hand_index].type);

                const int selection_raise = visual_index < int(card_raise_offset.size())
                                                ? card_raise_offset[visual_index]
                                                : 0;
                const int wave_raise = hand_swap_wave_raise(
                    visual_index, cursor_visual, swap_direction, swapping_card, swap_frame) +
                    row_scroll_pair_raise(
                        visual_index, cursor_visual, scroll_x, target_scroll_x,
                        game_layout::HAND_SPACING, visual_count);
                int card_y = game_layout::HAND_Y - selection_raise - wave_raise;

                if(removing_card && !removal_center_beat && !removal_play_resolved &&
                   hand_index == selected_card)
                {
                    const int deck_target_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
                    const int deck_target_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
                    const int graveyard_target_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
                    const int graveyard_target_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);
                    int dest_x = graveyard_target_x;
                    int dest_y = graveyard_target_y;

                    if(removal_style == RemovalStyle::TO_DECK_TOP)
                    {
                        dest_x = deck_target_x;
                        dest_y = deck_target_y;
                    }

                    const CardFlightSample flight = sample_removal_flight(main_x, dest_x, dest_y);
                    card_x = flight.x;
                    card_y = flight.y;
                    card.set_position(card_x, card_y);
                    card.set_visual(flight.scale, 0);

                    if(flight.alpha < 1)
                    {
                        bn::blending::set_transparency_alpha(
                            flight.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : flight.alpha);
                        card.set_blending_enabled(true);
                    }
                    else
                    {
                        card.set_blending_enabled(false);
                    }
                }
                else
                {
                    card.set_position(card_x, card_y);
                    card.clear_visual();
                    card.set_blending_enabled(false);
                }

                card.set_draw_on_top(false);
                card.set_visible(true);

                if(state.hand[hand_index].has_instance())
                {
                    card.set_upgrade_pips(&hud_count_generator,
                                          instance_at(state.instance_pool, state.hand[hand_index].instance_id));
                }
                else
                {
                    card.clear_upgrade_pips();
                }

                ++pool_slot;

                if(hand_index == selected_card && !(removing_card && removal_center_beat))
                {
                    if(echo_play_badge_active && removing_card)
                    {
                        echo_badge.set_kind(CardEffectBadge::Kind::ECHO);
                        echo_badge.set_position_above_card(card_x, card_y);
                        echo_badge.set_visible(true);
                    }

                    if(swivel_is_waiting(*this))
                    {
                        swivel_badge.set_kind(CardEffectBadge::Kind::SWIVEL);
                        swivel_badge.set_position_above_card(card_x, card_y);
                        swivel_badge.set_visible(true);
                    }
                }
            }

            bool has_left_fade = false;
            bool has_right_fade = false;

            hand_row_fade_visibility(
                *this, hand_x, scroll_x, edge_shift, main_x, removal_shift,
                swapping_card, swap_direction, swap_shift, has_left_fade, has_right_fade);

            bn::blending::set_transparency_alpha(game_layout::EDGE_FADE_ALPHA);
            apply_row_fade_bands(fade_bands, game_layout::HAND_Y, has_left_fade, has_right_fade);
            position_fade_bands(fade_bands, main_x);

            // Red X over the hand card chosen for a discard cost.
            if(mode == GameMode::DISCARD_TARGET && selected_card >= 0 && selected_card < state.hand.size())
            {
                int discard_x = 0;
                int discard_y = 0;
                const int discard_visual = visual_slot_for_hand_index(selected_card);
                hand_slot_screen_position(discard_visual, main_x, discard_x, discard_y);
                const int wave_raise = hand_swap_wave_raise(
                    discard_visual, cursor_visual, swap_direction, swapping_card, swap_frame) +
                    row_scroll_pair_raise(
                        discard_visual, cursor_visual, scroll_x, target_scroll_x,
                        game_layout::HAND_SPACING, visual_count);
                discard_y -= wave_raise;
                x_marker.set_position(discard_x + 16, discard_y + 32);
                x_marker.set_visible(true);
            }

            for(int slot = pool_slot; slot < hand_display.size(); ++slot)
            {
                release_card_display_tiles(hand_display[slot]);
            }
        }
}

void GameContext::sync_score_sprite_depth()
{
    const bool presentation_active = removing_card && removal_center_beat;
    const int score_z = presentation_active ? game_layout::PLAY_PRESENTATION_SCORE_Z : 0;
    const int score_bg_priority = presentation_active ? 3 : 2;

    for(bn::sprite_ptr& sprite : text_sprites)
    {
        sprite.set_z_order(score_z);
        sprite.set_bg_priority(score_bg_priority);
    }

    for(bn::sprite_ptr& sprite : round_text_sprites)
    {
        sprite.set_z_order(score_z);
        sprite.set_bg_priority(score_bg_priority);
    }

    for(ScorePop& pop : score_pops)
    {
        for(bn::sprite_ptr& sprite : pop.sprites)
        {
            sprite.set_z_order(score_z);
            sprite.set_bg_priority(score_bg_priority);
        }
    }
}

void GameContext::render_play_presentation_overlay(int main_x)
{
    if(graveyard_card_fx_active || hand_draw_fx_active ||
       (deck_search_resolve_fx.active &&
        deck_search_resolve_fx.phase == DeckSearchResolvePhase::PICK_FLIGHT))
    {
        return;
    }

    if(!removing_card || !removal_center_beat || removal_phase == PlayRemovalPhase::WAIT_PRESENTATION)
    {
        release_card_display_tiles(removal_fx_card);
        return;
    }

    const int deck_target_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
    const int deck_target_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    const int graveyard_target_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
    const int graveyard_target_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);
    int dest_x = graveyard_target_x;
    int dest_y = graveyard_target_y;

    if(removal_style == RemovalStyle::TO_DECK_TOP)
    {
        dest_x = deck_target_x;
        dest_y = deck_target_y;
    }

    const CardFlightSample flight = sample_removal_flight(main_x, dest_x, dest_y);

    removal_fx_card.set_type(removal_played_ref.type);

    if(removal_played_ref.has_instance())
    {
        removal_fx_card.set_upgrade_pips(&hud_count_generator,
                                         instance_at(state.instance_pool, removal_played_ref.instance_id));
    }
    else
    {
        removal_fx_card.clear_upgrade_pips();
    }

    removal_fx_card.set_position(flight.x, flight.y);
    removal_fx_card.set_visual(flight.scale, 0);
    removal_fx_card.set_draw_on_top(true);

    if(flight.alpha <= 0)
    {
        removal_fx_card.set_visible(false);
        return;
    }

    removal_fx_card.set_visible(true);

    if(flight.alpha < 1)
    {
        bn::blending::set_transparency_alpha(
            flight.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : flight.alpha);
        removal_fx_card.set_blending_enabled(true);
    }
    else
    {
        removal_fx_card.set_blending_enabled(false);
    }

    if(echo_play_badge_active)
    {
        echo_badge.set_kind(CardEffectBadge::Kind::ECHO);
        echo_badge.set_position_above_card(flight.x, flight.y);
        echo_badge.set_visible(true);
    }
}

void GameContext::render_frame()
{
        const int main_x = main_panel_offset_x();
        const int removal_shift = hand_removal_shift();
        const int swap_spacing = mode == GameMode::SCRY ? game_layout::SCRY_SPACING : game_layout::HAND_SPACING;
        const int swap_shift = swapping_card ? swap_eased_shift(swap_spacing, swap_frame) : 0;

        // Cursors are hidden unless a targeting mode turns them on below.
        x_marker.set_visible(false);
        swap_lock_marker.set_visible(false);
        echo_badge.set_visible(false);
        swivel_badge.set_visible(false);
        graveyard_pick_placeholder.set_visible(false);
        for(GameMarker& marker : grave_exclude_markers)
        {
            marker.set_visible(false);
        }
        for (Card &card : grave_row_display)
        {
            release_card_display_tiles(card);
        }
        for (Card &card : scry_display)
        {
            release_card_display_tiles(card);
        }
        for (Card &card : combo_display)
        {
            release_card_display_tiles(card);
        }
        for (Card &card : swivel_display)
        {
            release_card_display_tiles(card);
        }

        hide_hand_display();

        if (mode == GameMode::COMBO)
        {
            action_prompt_sprites.clear();
            render_combo_frame(main_x);
        }
        else if (inspecting || show_details_layer())
        {
            action_prompt_sprites.clear();
            if(inspecting)
            {
                // Un-render the previous page: every card pool except the inspect card.
                for(Card& card : grave_row_display)
                {
                    release_card_display_tiles(card);
                }

                for(Card& card : scry_display)
                {
                    release_card_display_tiles(card);
                }

                for(Card& card : combo_display)
                {
                    release_card_display_tiles(card);
                }

                for(Card& card : swivel_display)
                {
                    release_card_display_tiles(card);
                }

                x_marker.set_visible(false);
                swap_lock_marker.set_visible(false);
                echo_badge.set_visible(false);
                swivel_badge.set_visible(false);
                graveyard_pick_placeholder.set_visible(false);

                for(GameMarker& marker : grave_exclude_markers)
                {
                    marker.set_visible(false);
                }
            }

            hide_fade_bands(fade_bands);
        }
        else if (mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK)
        {
            render_graveyard_selection_frame(main_x);
        }
        else if (mode == GameMode::DECK_SEARCH)
        {
            action_prompt_sprites.clear();
            render_deck_search_frame(main_x);
        }
        else if (mode == GameMode::SCRY)
        {
            action_prompt_sprites.clear();
            render_scry_frame(main_x);
        }
        else
        {
            action_prompt_sprites.clear();
            render_hand_frame(main_x, swap_shift, removal_shift);
        }

        if(graveyard_card_fx_active)
        {
            const int deck_target_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
            const int deck_target_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
            const int score_target_x = card_target_x_for_score_center(main_x);
            const int score_target_y = card_target_y_for_score_center();
            const int frame_count = graveyard_card_fx_frame_count();
            CardFlightSample flight;

            if(graveyard_card_fx_kind == GraveyardExilePickKind::TO_HAND)
            {
                flight = sample_graveyard_to_hand_flight(
                    graveyard_card_fx_start_x, graveyard_card_fx_start_y,
                    graveyard_card_fx_dest_x, graveyard_card_fx_dest_y,
                    graveyard_card_fx_frame, frame_count);
            }
            else if(graveyard_card_fx_kind == GraveyardExilePickKind::TO_DECK_TOP ||
                    graveyard_card_fx_kind == GraveyardExilePickKind::SHUFFLE_TO_DECK)
            {
                flight = sample_graveyard_to_deck_flight(
                    graveyard_card_fx_start_x, graveyard_card_fx_start_y,
                    graveyard_card_fx_dest_x, graveyard_card_fx_dest_y,
                    graveyard_card_fx_frame, frame_count);
            }
            else if(graveyard_card_fx_style == RemovalStyle::TO_DECK_TOP)
            {
                flight = sample_card_to_deck(
                    graveyard_card_fx_start_x, graveyard_card_fx_start_y,
                    deck_target_x, deck_target_y,
                    graveyard_card_fx_frame, frame_count);
            }
            else
            {
                flight = sample_card_exile_dissipate(
                    graveyard_card_fx_start_x, graveyard_card_fx_start_y,
                    score_target_x, score_target_y,
                    graveyard_card_fx_frame, frame_count);
            }

            removal_fx_card.set_type(graveyard_card_fx_type);

            if(graveyard_card_fx_instance_id != NO_INSTANCE)
            {
                removal_fx_card.set_upgrade_pips(&hud_count_generator,
                                                 instance_at(state.instance_pool, graveyard_card_fx_instance_id));
            }
            else
            {
                removal_fx_card.clear_upgrade_pips();
            }

            removal_fx_card.set_position(flight.x, flight.y);
            removal_fx_card.set_visual(flight.scale, 0);
            removal_fx_card.set_draw_on_top(true);
            removal_fx_card.set_visible(true);

            if(flight.alpha < 1)
            {
                bn::blending::set_transparency_alpha(
                    flight.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : flight.alpha);
                removal_fx_card.set_blending_enabled(true);
            }
            else
            {
                removal_fx_card.set_blending_enabled(false);
            }
        }
        else if(hand_draw_fx_active)
        {
            int dest_x = 0;
            int dest_y = 0;
            hand_slot_screen_position(hand_draw_fx_dest_index, main_x, dest_x, dest_y);
            const CardFlightSample flight = sample_deck_to_hand_flight(
                hand_draw_fx_start_x, hand_draw_fx_start_y,
                dest_x, dest_y,
                hand_draw_fx_frame, game_layout::HAND_DRAW_FRAMES);

            removal_fx_card.set_type(hand_draw_fx_card.type);

            if(hand_draw_fx_card.has_instance())
            {
                removal_fx_card.set_upgrade_pips(&hud_count_generator,
                                                 instance_at(state.instance_pool, hand_draw_fx_card.instance_id));
            }
            else
            {
                removal_fx_card.clear_upgrade_pips();
            }

            removal_fx_card.set_position(flight.x, flight.y);
            removal_fx_card.set_visual(flight.scale, 0);
            removal_fx_card.set_draw_on_top(true);
            removal_fx_card.set_visible(true);
            removal_fx_card.set_blending_enabled(false);
        }
        else if(deck_search_resolve_fx.active &&
                deck_search_resolve_fx.phase == DeckSearchResolvePhase::PICK_FLIGHT)
        {
            const int deck_target_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
            const int deck_target_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
            const int score_target_x = card_target_x_for_score_center(main_x);
            const int score_target_y = card_target_y_for_score_center();
            const int graveyard_target_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
            const int graveyard_target_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);
            const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                        bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
            CardFlightSample flight;

            if(deck_search_resolve_fx.removal_style == RemovalStyle::EXILE_DISSIPATE)
            {
                flight = sample_card_exile_dissipate(
                    deck_search_resolve_fx.start_x, deck_search_resolve_fx.start_y,
                    score_target_x, score_target_y,
                    deck_search_resolve_fx.frame, game_layout::REMOVAL_FRAMES);
            }
            else if(deck_search_resolve_fx.removal_style == RemovalStyle::TO_DECK_TOP)
            {
                flight = sample_card_to_deck(
                    deck_search_resolve_fx.start_x, deck_search_resolve_fx.start_y,
                    deck_target_x, deck_target_y,
                    deck_search_resolve_fx.frame, game_layout::REMOVAL_FRAMES);
            }
            else
            {
                flight = sample_card_flight(
                    deck_search_resolve_fx.start_x, deck_search_resolve_fx.start_y,
                    graveyard_target_x, graveyard_target_y,
                    deck_search_resolve_fx.frame, game_layout::REMOVAL_FRAMES,
                    0, 0, 1, min_scale);
            }

            removal_fx_card.set_type(deck_search_resolve_fx.picked_type);
            removal_fx_card.set_position(flight.x, flight.y);
            removal_fx_card.set_visual(flight.scale, 0);
            removal_fx_card.set_visible(true);

            if(flight.alpha < 1)
            {
                bn::blending::set_transparency_alpha(
                    flight.alpha < bn::fixed(0.05) ? bn::fixed(0.05) : flight.alpha);
                removal_fx_card.set_blending_enabled(true);
            }
            else
            {
                removal_fx_card.set_blending_enabled(false);
            }
        }
        else if(!removing_card)
        {
            release_card_display_tiles(removal_fx_card);
        }

        position_main_score_sprites();
        score_pop_sync_positions(*this);

        const bool hide_main_scores = card_selection_ui_active() || inspecting;
        const bool show_round_score = !hide_main_scores &&
            (mode != GameMode::COMBO || state.combo_cinematic.frame >= COMBO_GATHER_FRAMES);

        for(bn::sprite_ptr& sprite : text_sprites)
        {
            sprite.set_visible(!hide_main_scores);
        }

        for(bn::sprite_ptr& sprite : round_text_sprites)
        {
            sprite.set_visible(show_round_score);
        }

        score_pop_render(*this, show_round_score && !inspecting);
        sync_score_sprite_depth();
        render_play_presentation_overlay(main_x);

        if(show_details_layer() && !inspecting)
        {
            sync_details_panel(false);
            position_details_sprites();
            hud.sync_details_modifiers(state, details_panel_offset_x(), true, campaign_ui.mode,
                                       campaign_ui.number_now_scoring_round);
        }
        else
        {
            if(inspecting)
            {
                details_sprites.clear();
                last_details_sprite_offset = 0;
            }

            hud.sync_details_modifiers(state, 0, false);
        }

        if(show_graveyard_layer() && !inspecting)
        {
            render_graveyard_browse(graveyard_panel_offset_x());
        }

        position_inspect_sprites();
        trinket_render_fx(*this);
}
