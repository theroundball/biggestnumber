#include "game_context.h"

#include "bn_blending.h"
#include "bn_color.h"
#include "bn_compression_type.h"
#include "bn_keypad.h"
#include "bn_math.h"
#include "bn_string.h"
#include "bn_affine_mat_attributes.h"
#include "bn_optional.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_palette_ptr.h"

#include "card_data.h"
#include "campaign_types.h"
#include "combo_system.h"
#include "fixed_32x64_sprite_font.h"
#include "common_variable_8x8_sprite_font.h"
#include "common_variable_8x16_sprite_font.h"
#include "game_events.h"
#include "game_helpers.h"
#include "play_resolution.h"
#include "scoring.h"
#include "save_data.h"
#include "score_count_system.h"
#include "score_swap_system.h"
#include "swivel_system.h"
#include "trinket_system.h"
#include "ui_inspect.h"

namespace
{
    constexpr bn::fixed ROUND_MULTIPLY_PREFIX_SCALE = bn::fixed(3) / 2;

    const bn::sprite_affine_mat_ptr& round_multiply_prefix_affine_mat()
    {
        static bn::optional<bn::sprite_affine_mat_ptr> mat;

        if(!mat.has_value())
        {
            bn::affine_mat_attributes attrs;
            attrs.set_scale(ROUND_MULTIPLY_PREFIX_SCALE);
            mat = bn::sprite_affine_mat_ptr::create(attrs);
        }

        return *mat;
    }

    int round_score_multiplier_prefix_length(int end_multiplier)
    {
        if(end_multiplier == 1)
        {
            return 0;
        }

        return bn::to_string<12>(end_multiplier).size();
    }

    void apply_tombstones_gy_entry(GameState& state)
    {
        const int factor = count_unique_graveyard_types(state);

        if(factor > 1)
        {
            state.mul_from_card(factor);
        }
    }

    void increment_play_counters(GameState& state, CardType type, PlaySource source)
    {
        (void)type;
        (void)source;

        ++state.cards_played_this_round;
    }

    void finish_hand_play_destination(GameState& state, CardRef card, PlayResolutionContext& context,
                                      RemovalStyle style, bool swivel_follow)
    {
        if(context.source == PlaySource::ECHO)
        {
            return;
        }

        if(style == RemovalStyle::TO_DECK_TOP)
        {
            if(context.selected_card)
            {
                finish_played_card_from_hand(*context.selected_card, state);
            }
        }
        else
        {
            const PostPlayDestination dest = route_played_card(state, card.type, context.source, swivel_follow);
            apply_post_play_destination(state, card, context.source, dest, context.hand_index,
                                        context.selected_card);

            if(dest == PostPlayDestination::GRAVEYARD && card.type == CardType::TOMBSTONES)
            {
                apply_tombstones_gy_entry(state);
            }
        }

        increment_play_counters(state, card.type, context.source);
        maybe_draw_if_solo(state, card.type);
    }
}

GameContext::GameContext(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch) :
    random_engine(make_battle_random_seed(collection)),
    deck(build_battle_deck(collection, random_engine, launch.instance_pool)),
    state(deck, random_engine),
    deck_high_score(launch.deck_index >= 0 && launch.deck_index < save_data_get().deck_count
        ? saved_deck_high_score(save_data_get().decks[launch.deck_index]) : 0),
    score_to_beat(launch.score_to_beat >= 0 ? launch.score_to_beat : deck_high_score),
    fade_bands{
        GameFadeBand(-116, 3, game_layout::HAND_Y),
        GameFadeBand(-108, 2, game_layout::HAND_Y),
        GameFadeBand(108, 2, game_layout::HAND_Y),
        GameFadeBand(116, 3, game_layout::HAND_Y),
    },
    x_marker(),
    swap_lock_marker(GameMarker::Style::LOCK),
    grave_exclude_markers{
        GameMarker(),
        GameMarker(),
        GameMarker(),
        GameMarker(),
        GameMarker(),
        GameMarker(),
        GameMarker(),
    },
    text_generator(fixed_32x64_sprite_font),
    round_text_generator(common::variable_8x16_sprite_font),
    inspect_text_generator(common::variable_8x8_sprite_font),
    hud_count_generator(common::variable_8x8_sprite_font),
    hud_mod_generator(common::variable_8x8_sprite_font),
    details_text_generator(common::variable_8x16_sprite_font),
    hud(hud_count_generator, hud_mod_generator)
{
    state.trinkets = launch.trinkets;
    state.echo_ready = state.has_trinket(TrinketType::ECHO);
    battle_stats_reset(state);
    state.instance_pool = launch.instance_pool;
    campaign_ui = launch.campaign_ui;

    switch(campaign_ui.mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        score_to_beat = campaign_ui.biggest_number_record;
        break;

    case CampaignMode::SAME_NUMBER:
        if(campaign_ui.same_number_target > 0)
        {
            score_to_beat = campaign_ui.same_number_target - 1;
        }
        break;

    case CampaignMode::NUMBER_NOW:
        score_to_beat = campaign_ui.number_now_round_peak;
        break;

    default:
        score_to_beat = launch.score_to_beat >= 0 ? launch.score_to_beat : deck_high_score;
        break;
    }

    if(launch.score_to_beat >= 0 && campaign_ui.mode == CampaignMode::NONE)
    {
        score_to_beat = launch.score_to_beat;
    }

    for(Card& card : grave_row_display)
    {
        card.set_visible(false);
    }

    for(Card& card : hand_display)
    {
        card.set_visible(false);
    }

    for(Card& card : combo_display)
    {
        card.set_visible(false);
    }

    for(Card& card : swivel_display)
    {
        card.set_visible(false);
    }

    hide_inspect_card(inspect_card);

    for(PlayFlight& flight : play_flights)
    {
        flight.fx_card.set_visible(false);
        release_card_display_tiles(flight.fx_card);
    }

    state.apply_round_start_trinkets();
    deal_opening_hand();
    reset_card_animation_state();
    try_start_hand_draw_fx();
    draw_total_score();
    draw_round_score();
    hud.update(state);
    bn::blending::set_transparency_alpha(game_layout::EDGE_FADE_ALPHA);
    update_target_scroll();
    process_instant_pending();
}

// Redraws the persistent game-score readout (only changes at round end).
void GameContext::draw_total_score()
{
    if(score_swap_is_active(*this))
    {
        return;
    }

    if(score_count_is_active(*this, TrinketScoreField::TOTAL))
    {
        return;
    }

    score_count_process_pending(*this);

    if(score_count_is_active(*this, TrinketScoreField::TOTAL))
    {
        return;
    }

    const int new_total = state.total_score;
    const bool changed = _total_score_initialized && new_total != _cached_total_score;

    if(_total_score_initialized && !changed)
    {
        return;
    }

    _total_score_initialized = true;
    _cached_total_score = new_total;

    if(changed)
    {
        _total_wiggle_x = 0;
        _total_wiggle_y = 0;
        total_score_wiggle_frames = game_layout::SCORE_WIGGLE_FRAMES;
    }

    show_total_score_value(new_total);
};

void GameContext::show_total_score_value(int value)
{
    if(_total_score_initialized && value == _cached_total_score && !text_sprites.empty())
    {
        return;
    }

    _cached_total_score = value;
    _total_score_initialized = true;
    text_sprites.clear();
    last_main_sprite_offset = 0;
    text_generator.set_center_alignment();
    text_generator.generate(0, -48, bn::to_string<12>(value), text_sprites);
    text_generator.set_left_alignment();

    // Green when this run/fight has beaten the score-to-beat baseline.
    if(value > score_to_beat && !text_sprites.empty())
    {
        static bn::array<bn::color, 16> green_colors;
        bn::span<const bn::color> source = text_sprites[0].palette().colors();

        for(int index = 0; index < 16; ++index)
        {
            green_colors[index] = index < source.size() ? source[index] : bn::color();
        }

        constexpr bn::color SCORE_GREEN(6, 28, 10);

        for(int index = 1; index < 16; ++index)
        {
            const bn::color& color = green_colors[index];

            if(color.red() + color.green() + color.blue() > 24)
            {
                green_colors[index] = SCORE_GREEN;
            }
        }

        const bn::sprite_palette_item item(
            bn::span<const bn::color>(green_colors.data(), green_colors.size()), bn::bpp_mode::BPP_4,
            bn::compression_type::NONE);
        const bn::sprite_palette_ptr green_palette = bn::sprite_palette_ptr::create(item);

        for(bn::sprite_ptr& sprite : text_sprites)
        {
            sprite.set_palette(green_palette);
        }
    }

    sync_score_progress_bar();
}

void GameContext::finalize_total_score_display()
{
    _total_score_initialized = true;
    _total_wiggle_x = 0;
    _total_wiggle_y = 0;
    total_score_wiggle_frames = game_layout::SCORE_WIGGLE_FRAMES;
}

int GameContext::score_progress_goal() const
{
    switch(campaign_ui.mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        return campaign_ui.biggest_number_record;

    case CampaignMode::SAME_NUMBER:
        return campaign_ui.same_number_target;

    case CampaignMode::NUMBER_NOW:
        return campaign_ui.number_now_round_peak;

    default:
        return score_to_beat;
    }
}

bool GameContext::score_progress_visible() const
{
    return score_progress_goal() > 0;
}

void GameContext::sync_score_progress_bar()
{
    const int goal = score_progress_goal();
    int display_total = state.total_score;
    int display_round = state.round.committed();

    if(score_count_fx[1].active)
    {
        display_total = score_count_fx[1].displayed;
    }

    if(score_count_fx[0].active)
    {
        display_round = score_count_fx[0].displayed * score_count_fx[0].end_multiplier;
    }

    score_progress_bar.sync(display_total, display_round, goal);
}

// Redraws the current round's running score formula.
void GameContext::draw_round_score()
{
    if(score_swap_is_active(*this))
    {
        return;
    }

    if(score_count_is_active(*this, TrinketScoreField::ROUND))
    {
        return;
    }

    score_count_process_pending(*this);

    if(score_count_is_active(*this, TrinketScoreField::ROUND))
    {
        return;
    }

    const bn::string<48> new_text = format_round_score(state.round);
    const bool changed = _round_score_initialized && new_text != _cached_round_score_text;

    if(_round_score_initialized && !changed)
    {
        return;
    }

    _round_score_initialized = true;
    _cached_round_score_text = new_text;

    if(changed)
    {
        _round_wiggle_x = 0;
        _round_wiggle_y = 0;
        round_score_wiggle_frames = game_layout::SCORE_WIGGLE_FRAMES;
    }

    show_round_score_running(state.round.running, state.round.end_multiplier);
};

void GameContext::show_round_score_running(int running, int end_multiplier)
{
    // Number Now: non-scoring rounds always display as 0(running) so the ×0 is obvious.
    if(campaign_ui.mode == CampaignMode::NUMBER_NOW &&
       state.current_round != campaign_ui.number_now_scoring_round)
    {
        end_multiplier = 0;
    }

    RoundScore display_score;
    display_score.running = running;
    display_score.end_multiplier = end_multiplier;
    const bn::string<48> new_text = format_round_score(display_score);

    if(_round_score_initialized && new_text == _cached_round_score_text && !round_text_sprites.empty())
    {
        return;
    }

    _cached_round_score_text = new_text;
    _round_score_initialized = true;
    round_text_sprites.clear();
    last_round_sprite_offset = 0;
    round_text_generator.set_center_alignment();
    round_text_generator.generate(0, 0, format_round_score(display_score), round_text_sprites);
    round_text_generator.set_left_alignment();

    if(end_multiplier != 1)
    {
        const int prefix_length = round_score_multiplier_prefix_length(end_multiplier);
        const bn::sprite_affine_mat_ptr& multiply_mat = round_multiply_prefix_affine_mat();

        for(int index = 0; index < prefix_length && index < round_text_sprites.size(); ++index)
        {
            round_text_sprites[index].set_affine_mat(multiply_mat);
        }
    }

    sync_score_progress_bar();
}

void GameContext::finalize_round_score_display()
{
    // Wiggle only — do not snap to state.round.running while a trinket flight still
    // owns a deferred count (Morel / Lucky Sevens arrival).
    _round_score_initialized = true;
    _round_wiggle_x = 0;
    _round_wiggle_y = 0;
    round_score_wiggle_frames = game_layout::SCORE_WIGGLE_FRAMES;
}

void GameContext::tick_score_wiggles()
{
    auto tick_group = [](int& frames, int& wiggle_x, int& wiggle_y, bn::span<bn::sprite_ptr> sprites)
    {
        if(frames <= 0)
        {
            return;
        }

        const int frame = game_layout::SCORE_WIGGLE_FRAMES - frames;
        const int new_wiggle_x = (bn::sin(bn::fixed(frame) / 6) * 2).integer();
        const int new_wiggle_y = (bn::cos(bn::fixed(frame) / 8) * 1).integer();
        const int delta_x = new_wiggle_x - wiggle_x;
        const int delta_y = new_wiggle_y - wiggle_y;

        for(bn::sprite_ptr& sprite : sprites)
        {
            sprite.set_x(sprite.x() + delta_x);
            sprite.set_y(sprite.y() + delta_y);
        }

        wiggle_x = new_wiggle_x;
        wiggle_y = new_wiggle_y;
        --frames;

        if(frames == 0)
        {
            for(bn::sprite_ptr& sprite : sprites)
            {
                sprite.set_x(sprite.x() - wiggle_x);
                sprite.set_y(sprite.y() - wiggle_y);
            }

            wiggle_x = 0;
            wiggle_y = 0;
        }
    };

    if(score_swap_is_active(*this))
    {
        return;
    }

    tick_group(round_score_wiggle_frames, _round_wiggle_x, _round_wiggle_y,
               bn::span<bn::sprite_ptr>(round_text_sprites.data(), round_text_sprites.size()));
    tick_group(total_score_wiggle_frames, _total_wiggle_x, _total_wiggle_y,
               bn::span<bn::sprite_ptr>(text_sprites.data(), text_sprites.size()));
}

void GameContext::commit_round_with_checks()
{
    const int before = state.total_score;
    const int round_score = state.round.committed();
    const int round_number = state.current_round;

    if(campaign_ui.mode == CampaignMode::NUMBER_NOW && round_number != campaign_ui.number_now_scoring_round)
    {
        scene_result.last_round_score = 0;
        scene_result.last_round_number = round_number;
        state.round.reset();
    }
    else
    {
        scene_result.last_round_score = round_score;
        scene_result.last_round_number = round_number;
        state.commit_round();
    }

    score_count_cancel(*this, TrinketScoreField::ROUND);
    _cached_round_score_text = "";
    _round_score_initialized = false;
    round_score_wiggle_frames = 0;
    show_round_score_running(0, 1);
    score_count_queue(state, TrinketScoreField::TOTAL, before, state.total_score);
    trinket_queue_score_check(state, TrinketScoreField::TOTAL, before, state.total_score);
}

// The card the inspect view would describe, given the current mode, as an
// int CardType (or -1 if there's nothing highlighted right now).
int GameContext::current_highlight_type() const
{
    if (mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK)
    {
        if(state.graveyard.empty())
        {
            return -1;
        }

        const int cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
        return int(state.graveyard[cursor].type);
    }

    if (side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
    {
        const bn::span<const CardRef> cards = active_browse_cards();

        if(cards.empty())
        {
            return -1;
        }

        const int cursor = clamp_graveyard_cursor(browse_cursor, cards.size());
        return int(cards[cursor].type);
    }

    if (mode == GameMode::SCRY)
    {
        return state.selection.scry_buffer.empty() ? -1 : int(state.selection.scry_buffer[state.selection.cursor].type);
    }

    if (mode == GameMode::DECK_SEARCH)
    {
        return state.selection.deck_search_buffer.empty() ? -1
            : int(state.selection.deck_search_buffer[state.selection.cursor].type);
    }

    if (mode == GameMode::COMBO)
    {
        return -1;
    }

    return selected_card < 0 || selected_card >= playable_slot_count(state) ? -1
           : int(playable_slot_card(state, selected_card).type);
};

int GameContext::current_highlight_index() const
{
    switch(mode)
    {
    case GameMode::GRAVEYARD_TARGET:
    case GameMode::GRAVEYARD_PICK:
    case GameMode::SCRY:
    case GameMode::DECK_SEARCH:
        return state.selection.cursor;

    case GameMode::COMBO:
        return -1;

    default:
        if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
        {
            return browse_cursor;
        }

        return selected_card;
    }
}

void GameContext::snap_active_scroll()
{
    scroll_x = target_scroll_x;
    edge_shift = target_edge_shift;
    hand_x = target_hand_x;

    if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE ||
       mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK ||
       mode == GameMode::DECK_SEARCH || mode == GameMode::SCRY)
    {
        row_scroll_x = target_row_scroll_x;
    }
}

void GameContext::clamp_hand_cursor()
{
    const int count = (mode == GameMode::DISCARD_TARGET) ? state.hand.size() : playable_slot_count(state);

    if(count <= 0)
    {
        selected_card = 0;
        return;
    }

    if(selected_card < 0 || selected_card >= count)
    {
        selected_card = count - 1;
    }
}

void GameContext::prepare_hand_selection_mode()
{
    clamp_hand_cursor();
    row_scroll_x = 0;
    target_row_scroll_x = 0;
    target_row_scroll_index = 0;
    update_target_scroll();
    snap_active_scroll();
}

void GameContext::reset_card_animation_state()
{
    for(PlayFlight& flight : play_flights)
    {
        clear_play_flight(flight);
    }

    removing_card = false;
    pending_opening_hand_deal = false;
    pending_cycle_draws = 0;
    graveyard_card_fx_active = false;
    graveyard_card_fx_frame = 0;
    graveyard_card_fx_kind = GraveyardExilePickKind::NONE;
    graveyard_card_fx_instance_id = NO_INSTANCE;
    deck_search_resolve_fx.active = false;
    deck_search_resolve_fx.frame = 0;
    for(int& offset : card_raise_offset)
    {
        offset = 0;
    }
    snap_selected_card_raise();
    echo_play_badge_active = false;
    swivel_follow_pending = false;

    if(!state.echo_pending_replay)
    {
        echo_ghost_active = false;
        echo_ghost_card.set_visible(false);
        state.echo_replay_card = CardRef{};
    }

    clamp_hand_cursor();
}

void GameContext::clear_play_flight(PlayFlight& flight)
{
    flight.active = false;
    flight.is_discard = false;
    flight.phase = PlayRemovalPhase::APPROACH;
    flight.center_beat = false;
    flight.origin = PlayPresentOrigin::HAND;
    flight.played_ref = CardRef{};
    flight.play_context = PlayResolutionContext{};
    flight.play_resolved = false;
    flight.is_miracle_bonus = false;
    flight.swivel_follow = false;
    flight.cycle_draw = false;
    flight.cycle_exile = false;
    flight.mill_without_play = false;
    flight.mill_reveal_flex_continue = false;
    flight.mill_reveal_waterfall_continue = false;
    flight.mill_reveal_draw_on_hit = false;
    flight.mill_reveal_round_before = 0;
    flight.mill_reveal_total_before = 0;
    flight.hand_committed = false;
    flight.scoring_source = PlaySource::HAND;
    flight.style = RemovalStyle::TO_GRAVEYARD;
    flight.frame = 0;
    flight.hand_index = -1;
    flight.start_x = 0;
    flight.start_y = 0;
    flight.fx_card.set_visible(false);
    release_card_display_tiles(flight.fx_card);
}

void GameContext::sync_removing_card()
{
    removing_card = play_flight_count() > 0;
}

int GameContext::play_flight_count() const
{
    int count = 0;

    for(const PlayFlight& flight : play_flights)
    {
        if(flight.active)
        {
            ++count;
        }
    }

    return count;
}

PlayFlight* GameContext::alloc_play_flight()
{
    for(int index = 0; index < game_layout::MAX_PLAY_FLIGHTS; ++index)
    {
        if(!play_flights[index].active)
        {
            clear_play_flight(play_flights[index]);
            play_flights[index].active = true;
            latest_flight_index = index;
            sync_removing_card();
            return &play_flights[index];
        }
    }

    return nullptr;
}

PlayFlight* GameContext::latest_play_flight()
{
    if(latest_flight_index < 0 || latest_flight_index >= game_layout::MAX_PLAY_FLIGHTS)
    {
        return nullptr;
    }

    PlayFlight& flight = play_flights[latest_flight_index];
    return flight.active ? &flight : nullptr;
}

const PlayFlight* GameContext::latest_play_flight() const
{
    if(latest_flight_index < 0 || latest_flight_index >= game_layout::MAX_PLAY_FLIGHTS)
    {
        return nullptr;
    }

    const PlayFlight& flight = play_flights[latest_flight_index];
    return flight.active ? &flight : nullptr;
}

Card& GameContext::exclusive_fx_card()
{
    return play_flights[0].fx_card;
}

bool GameContext::play_can_overlap() const
{
    if(mode != GameMode::NORMAL)
    {
        return false;
    }

    if(play_flight_count() >= game_layout::MAX_PLAY_FLIGHTS)
    {
        return false;
    }

    if(play_flight_count() <= 0)
    {
        return true;
    }

    if(!state.pending_actions.empty())
    {
        return false;
    }

    if(graveyard_card_fx_active || hand_draw_fx_blocking() || deck_search_resolve_active())
    {
        return false;
    }

    if(lucky_sevens_fx.active || !pending_lucky_sevens.empty())
    {
        return false;
    }

    if(state.echo_pending_replay || state.swivel_waiting)
    {
        return false;
    }

    if(combo_focus_active() || state.combo_cinematic.active)
    {
        return false;
    }

    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active)
        {
            continue;
        }

        if(flight.origin != PlayPresentOrigin::HAND &&
           flight.origin != PlayPresentOrigin::GRAVEYARD)
        {
            return false;
        }

        if(flight.swivel_follow || flight.is_discard ||
           flight.mill_without_play || flight.is_miracle_bonus)
        {
            return false;
        }

        if(flight.played_ref.type == CardType::SWIVEL)
        {
            return false;
        }

        if(flight.center_beat &&
           (flight.phase == PlayRemovalPhase::APPROACH || flight.phase == PlayRemovalPhase::HOLD))
        {
            return false;
        }
    }

    return true;
}

bool GameContext::play_hides_hand_index(int hand_index) const
{
    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed || !flight.center_beat)
        {
            continue;
        }

        if(flight.origin == PlayPresentOrigin::HAND && flight.hand_index >= 0 &&
           flight.hand_index == hand_index)
        {
            return true;
        }
    }

    return false;
}

bool GameContext::play_hides_graveyard_visual(int visual_index) const
{
    if(!playable_slot_is_flashback(state, visual_index))
    {
        return false;
    }

    const int gy_index = playable_slot_graveyard_index(state, visual_index);

    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed)
        {
            continue;
        }

        if(flight.origin == PlayPresentOrigin::GRAVEYARD && flight.hand_index >= 0 &&
           flight.hand_index == gy_index)
        {
            return true;
        }
    }

    return false;
}

bool GameContext::play_hides_visual_slot(int visual_index) const
{
    if(play_hides_graveyard_visual(visual_index))
    {
        return true;
    }

    const int hand_index = playable_slot_hand_index(state, visual_index);
    return hand_index >= 0 && play_hides_hand_index(hand_index);
}

void GameContext::snap_selected_card_raise()
{
    if(mode != GameMode::NORMAL && mode != GameMode::DISCARD_TARGET)
    {
        return;
    }

    if(selected_card < 0 || selected_card >= card_raise_offset.size())
    {
        return;
    }

    if(play_hides_visual_slot(selected_card))
    {
        return;
    }

    card_raise_offset[selected_card] = game_layout::SELECTED_RAISE;
}

void GameContext::retarget_selection_off_hidden_slot()
{
    if(mode != GameMode::NORMAL)
    {
        return;
    }

    const int count = playable_slot_count(state);

    if(count <= 0 || selected_card < 0)
    {
        return;
    }

    if(!play_hides_visual_slot(selected_card))
    {
        return;
    }

    for(int visual_index = selected_card + 1; visual_index < count; ++visual_index)
    {
        if(!play_hides_visual_slot(visual_index))
        {
            selected_card = visual_index;
            return;
        }
    }

    for(int visual_index = selected_card - 1; visual_index >= 0; --visual_index)
    {
        if(!play_hides_visual_slot(visual_index))
        {
            selected_card = visual_index;
            return;
        }
    }
}

void GameContext::begin_direct_removal(int start_x, int start_y, RemovalStyle style, bool is_discard,
                                      bool cycle_exile)
{
    PlayFlight* flight = alloc_play_flight();

    if(!flight)
    {
        return;
    }

    removal_start_x = start_x;
    removal_start_y = start_y;
    flight->start_x = start_x;
    flight->start_y = start_y;
    flight->style = style;
    flight->is_discard = is_discard;
    flight->center_beat = false;
    flight->phase = PlayRemovalPhase::DEPART;
    flight->frame = 0;
    flight->hand_index = selected_card;
    flight->cycle_exile = cycle_exile;
    flight->cycle_draw = cycle_exile;

    if(!is_discard && selected_card >= 0 && selected_card < state.hand.size())
    {
        flight->played_ref = state.hand[selected_card];
        flight->play_context = PlayResolutionContext{};
        flight->play_context.source = PlaySource::HAND;
        flight->play_context.hand_index = selected_card;
        flight->play_context.selected_card = &selected_card;
        flight->origin = PlayPresentOrigin::HAND;
        flight->scoring_source = PlaySource::HAND;

        const bool early_commit = flight->played_ref.type != CardType::SWIVEL &&
                                  state.selection.type != PendingActionType::PUT_HAND_ON_DECK_TOP;

        if(cycle_exile)
        {
            flight->fx_card.set_type(flight->played_ref.type);

            if(flight->played_ref.has_instance())
            {
                flight->fx_card.set_upgrade_pips(
                    &hud_count_generator, instance_at(state.instance_pool, flight->played_ref.instance_id));
            }
            else
            {
                flight->fx_card.clear_upgrade_pips();
            }

            const int removed_index = flight->hand_index;
            battle_stat_record_cycle(state);
            battle_stat_record_keyword_discard(state);
            hand_remove_at_exiled(state, removed_index, selected_card);
            shift_card_raise_after_remove(removed_index);
            flight->hand_committed = true;
            flight->play_resolved = true;
        }
        else if(early_commit)
        {
            flight->fx_card.set_type(flight->played_ref.type);

            if(flight->played_ref.has_instance())
            {
                flight->fx_card.set_upgrade_pips(
                    &hud_count_generator, instance_at(state.instance_pool, flight->played_ref.instance_id));
            }
            else
            {
                flight->fx_card.clear_upgrade_pips();
            }

            resolve_play_flight_effects(*flight);
            commit_play_flight_destination(*flight);
        }
    }
}

void GameContext::begin_play_presentation(CardRef card, int start_x, int start_y, PlayPresentOrigin origin,
                                          PlayResolutionContext context, RemovalStyle style, bool miracle_bonus)
{
    PlayFlight* flight = alloc_play_flight();

    if(!flight)
    {
        return;
    }

    removal_start_x = start_x;
    removal_start_y = start_y;
    flight->played_ref = card;
    flight->play_context = context;
    flight->origin = origin;
    flight->style = style;
    flight->is_discard = false;
    flight->is_miracle_bonus = miracle_bonus;
    flight->play_resolved = false;
    flight->mill_without_play = false;
    flight->mill_reveal_flex_continue = false;
    flight->mill_reveal_draw_on_hit = false;
    flight->cycle_draw = false;
    flight->start_x = start_x;
    flight->start_y = start_y;
    flight->frame = 0;
    flight->phase = PlayRemovalPhase::APPROACH;
    flight->center_beat = miracle_bonus || card_has_play_effect(state, card);
    flight->scoring_source = context.source == PlaySource::ECHO ? state.echo_replay_scoring_source
                                                               : context.source;

    if(origin == PlayPresentOrigin::HAND)
    {
        selected_card = context.hand_index;
        flight->hand_index = context.hand_index;

        if(state.echo_first_play_active() && card_has_play_effect(state, card))
        {
            echo_ghost_active = true;
            echo_ghost_x = start_x;
            echo_ghost_y = start_y;
            echo_ghost_card.set_type(card.type);
            echo_ghost_card.set_position(start_x, start_y);
            echo_ghost_card.clear_visual();
            echo_ghost_card.set_draw_on_top(false);
            echo_ghost_card.set_blending_enabled(true);
            echo_ghost_card.set_visible(true);

            if(card.has_instance())
            {
                echo_ghost_card.set_upgrade_pips(
                    &hud_count_generator, instance_at(state.instance_pool, card.instance_id));
            }
            else
            {
                echo_ghost_card.clear_upgrade_pips();
            }
        }
    }
    else if(origin == PlayPresentOrigin::GRAVEYARD)
    {
        flight->hand_index = context.hand_index;
    }
    else
    {
        selected_card = -1;
        flight->hand_index = -1;
    }

    flight->fx_card.set_type(card.type);

    if(card.has_instance())
    {
        flight->fx_card.set_upgrade_pips(&hud_count_generator,
                                         instance_at(state.instance_pool, card.instance_id));
    }
    else
    {
        flight->fx_card.clear_upgrade_pips();
    }

    if(origin == PlayPresentOrigin::GRAVEYARD && !flight->center_beat)
    {
        resolve_play_flight_effects(*flight);
        commit_play_flight_destination(*flight);
    }

    retarget_selection_off_hidden_slot();
    snap_selected_card_raise();

    if(origin == PlayPresentOrigin::HAND || origin == PlayPresentOrigin::GRAVEYARD)
    {
        update_target_scroll();
    }
}

void GameContext::begin_discard_presentation(int hand_index)
{
    capture_removal_start();
    PlayFlight* flight = alloc_play_flight();

    if(!flight)
    {
        return;
    }

    flight->played_ref = state.hand[hand_index];
    flight->style = RemovalStyle::TO_GRAVEYARD;
    flight->is_discard = true;
    flight->is_miracle_bonus = false;
    flight->play_resolved = false;
    flight->origin = PlayPresentOrigin::HAND;
    flight->play_context = PlayResolutionContext{};
    flight->play_context.source = PlaySource::HAND;
    flight->play_context.hand_index = hand_index;
    flight->play_context.selected_card = &selected_card;
    flight->start_x = removal_start_x;
    flight->start_y = removal_start_y;
    flight->frame = 0;
    flight->phase = PlayRemovalPhase::APPROACH;
    flight->center_beat = card_has_discard_effect(flight->played_ref.type);
    selected_card = hand_index;
    flight->hand_index = hand_index;

    if(flight->center_beat)
    {
        flight->fx_card.set_type(flight->played_ref.type);

        if(flight->played_ref.has_instance())
        {
            flight->fx_card.set_upgrade_pips(
                &hud_count_generator, instance_at(state.instance_pool, flight->played_ref.instance_id));
        }
        else
        {
            flight->fx_card.clear_upgrade_pips();
        }
    }

    update_target_scroll();
}

CardFlightSample GameContext::sample_play_flight(const PlayFlight& flight, int main_x, int dest_x, int dest_y) const
{
    const int center_x = card_target_x_for_score_center(main_x);
    const int center_y = flight.center_beat ? card_target_y_for_play_presentation()
                                           : card_target_y_for_score_center();
    const bn::fixed min_scale = bn::fixed(game_layout::REMOVAL_MIN_SCALE) /
                                bn::fixed(game_layout::REMOVAL_MIN_SCALE_DIVISOR);
    const bool deck_zoom_origin = flight.origin == PlayPresentOrigin::DECK;

    if(!flight.center_beat)
    {
        if(flight.style == RemovalStyle::EXILE_DISSIPATE)
        {
            return sample_card_exile_dissipate(
                flight.start_x, flight.start_y, center_x, center_y,
                flight.frame, game_layout::REMOVAL_FRAMES);
        }

        if(flight.style == RemovalStyle::TO_DECK_TOP)
        {
            return sample_card_to_deck(
                flight.start_x, flight.start_y, dest_x, dest_y,
                flight.frame, game_layout::REMOVAL_FRAMES);
        }

        return sample_card_flight(
            flight.start_x, flight.start_y, dest_x, dest_y,
            flight.frame, game_layout::REMOVAL_FRAMES, 0, 0, 1, min_scale);
    }

    if(flight.phase == PlayRemovalPhase::APPROACH)
    {
        if(deck_zoom_origin)
        {
            return sample_card_flight(
                flight.start_x, flight.start_y, center_x, center_y,
                flight.frame, game_layout::PLAY_DECK_APPROACH_FRAMES,
                0, 0, min_scale, 1);
        }

        return sample_card_flight(
            flight.start_x, flight.start_y, center_x, center_y,
            flight.frame, game_layout::PLAY_APPROACH_FRAMES, 0, 0, 1, 1);
    }

    if(flight.phase == PlayRemovalPhase::HOLD)
    {
        CardFlightSample sample;
        sample.x = center_x;
        sample.y = center_y;
        sample.scale = 1;
        sample.alpha = 1;
        return sample;
    }

    if(flight.phase == PlayRemovalPhase::WAIT_PRESENTATION)
    {
        CardFlightSample sample;
        sample.x = dest_x;
        sample.y = dest_y;
        sample.scale = 1;
        sample.alpha = 0;
        return sample;
    }

    if(flight.style == RemovalStyle::EXILE_DISSIPATE)
    {
        return sample_card_exile_dissipate(
            center_x, center_y, center_x, center_y,
            flight.frame, game_layout::PLAY_DEPART_FRAMES);
    }

    if(flight.style == RemovalStyle::TO_DECK_TOP)
    {
        return sample_card_to_deck(
            center_x, center_y, dest_x, dest_y,
            flight.frame, game_layout::PLAY_DEPART_FRAMES);
    }

    return sample_card_flight(
        center_x, center_y, dest_x, dest_y,
        flight.frame, game_layout::PLAY_DEPART_FRAMES, 0, 0, 1, min_scale);
}

int GameContext::hand_removal_shift() const
{
    const PlayFlight* compressing = nullptr;

    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed || !flight.center_beat)
        {
            continue;
        }

        if(flight.origin == PlayPresentOrigin::HAND && flight.hand_index >= 0)
        {
            compressing = &flight;
            break;
        }
    }

    if(!compressing)
    {
        return 0;
    }

    if(game_layout::PLAY_APPROACH_FRAMES <= 1)
    {
        return game_layout::HAND_SPACING;
    }

    switch(compressing->phase)
    {
    case PlayRemovalPhase::APPROACH:
        return game_layout::HAND_SPACING * compressing->frame / (game_layout::PLAY_APPROACH_FRAMES - 1);

    case PlayRemovalPhase::HOLD:
        return game_layout::HAND_SPACING;

    default:
        return 0;
    }
}

bool GameContext::hand_removal_gap_layout() const
{
    return false;
}

int GameContext::visual_hand_slot_count() const
{
    if(hand_removal_gap_layout())
    {
        return playable_slot_count(state) + 1;
    }

    if(mode == GameMode::DISCARD_TARGET)
    {
        return state.hand.size();
    }

    return playable_slot_count(state);
}

int GameContext::hand_index_for_visual_slot(int visual_index) const
{
    return visual_index;
}

int GameContext::visual_slot_for_hand_index(int hand_index) const
{
    return hand_index;
}

int GameContext::active_removal_slot_index() const
{
    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed || !flight.center_beat)
        {
            continue;
        }

        if(flight.origin == PlayPresentOrigin::HAND && flight.hand_index >= 0)
        {
            return flight.hand_index;
        }
    }

    return selected_card;
}

int GameContext::layout_hand_count() const
{
    int count = visual_hand_slot_count();

    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed || !flight.center_beat || count <= 0)
        {
            continue;
        }

        if((flight.origin == PlayPresentOrigin::HAND || flight.origin == PlayPresentOrigin::GRAVEYARD) &&
           flight.hand_index >= 0)
        {
            --count;
        }
    }

    return count;
}

int GameContext::hand_layout_center_count() const
{
    if(hand_removal_gap_layout())
    {
        return visual_hand_slot_count();
    }

    return layout_hand_count();
}

void GameContext::resolve_play_flight_effects(PlayFlight& flight)
{
    if(flight.play_resolved)
    {
        return;
    }

    if(flight.is_discard)
    {
        const int discard_index = flight.hand_index >= 0 ? flight.hand_index : selected_card;
        selected_card = discard_index;
        discard_card(state, selected_card);
        draw_round_score();

        if(state.selection.type == PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY)
        {
            state.mul_from_card(state.selection.multiply_factor);
            draw_round_score();
        }
    }
    else if(flight.is_miracle_bonus)
    {
        flight.mill_reveal_round_before = state.round.running;
        flight.mill_reveal_total_before = state.total_score;
        play_miracle_bonus(state, 10);
        graveyard_push(state, flight.played_ref);
        draw_round_score();
    }
    else if(flight.mill_without_play)
    {
        apply_card_relocated(state, flight.played_ref.type);
        draw_round_score();
    }
    else
    {
        flight.mill_reveal_round_before = state.round.running;
        flight.mill_reveal_total_before = state.total_score;
        apply_card_relocated_from_play(state, flight.played_ref.type, flight.play_context.source);
        state.swivel_waiting = flight.swivel_follow;
        apply_card_play(state, flight.played_ref, flight.scoring_source);
        state.swivel_waiting = false;
        draw_round_score();
    }

    flight.play_resolved = true;
}

void GameContext::commit_play_flight_destination(PlayFlight& flight)
{
    if(flight.hand_committed || flight.is_discard)
    {
        return;
    }

    if(flight.origin == PlayPresentOrigin::HAND)
    {
        const int removed_index = flight.hand_index >= 0 ? flight.hand_index : selected_card;

        finish_hand_play_destination(state, flight.played_ref, flight.play_context, flight.style,
                                     flight.swivel_follow);
        shift_card_raise_after_remove(removed_index);
        flight.hand_committed = true;

        if(flight.play_context.source != PlaySource::ECHO)
        {
            arm_echo_replay(flight.played_ref, flight.scoring_source, flight.start_x, flight.start_y);
        }

        if(flight.played_ref.type == CardType::SWIVEL)
        {
            swivel_on_swivel_played(*this);
        }

        swivel_clear_wait_if_hand_empty(*this);
        return;
    }

    if(flight.origin != PlayPresentOrigin::GRAVEYARD &&
       flight.play_context.source != PlaySource::FLASHBACK)
    {
        return;
    }

    const PostPlayDestination dest = route_played_card(
        state, flight.played_ref.type, flight.play_context.source, flight.swivel_follow);
    int removed_visual = -1;

    if(flight.origin == PlayPresentOrigin::GRAVEYARD && flight.hand_index >= 0)
    {
        const int slot_count = playable_slot_count(state);

        for(int visual = state.hand.size(); visual < slot_count; ++visual)
        {
            if(playable_slot_graveyard_index(state, visual) == flight.hand_index)
            {
                removed_visual = visual;
                break;
            }
        }
    }

    apply_post_play_destination(state, flight.played_ref, flight.play_context.source, dest,
                                flight.play_context.hand_index, flight.play_context.selected_card);
    increment_play_counters(state, flight.played_ref.type, flight.play_context.source);
    maybe_draw_if_solo(state, flight.played_ref.type);
    flight.hand_committed = true;

    if(flight.play_context.source != PlaySource::ECHO)
    {
        arm_echo_replay(flight.played_ref, flight.scoring_source, flight.start_x, flight.start_y);
    }

    if(removed_visual >= 0 && selected_card > removed_visual)
    {
        --selected_card;
    }

    clamp_hand_cursor();
    snap_selected_card_raise();
}

void GameContext::complete_play_flight(PlayFlight& flight)
{
    const bool swivel_follow = flight.swivel_follow;
    const bool cycle_draw = flight.cycle_draw;
    const bool mill_draw = flight.mill_reveal_draw_on_hit && !flight.mill_without_play;
    const bool mill_flex = flight.mill_reveal_flex_continue && flight.mill_without_play &&
                           state.deck.remaining() > 0;
    const bool mill_waterfall = flight.mill_reveal_waterfall_continue && flight.play_resolved &&
                                !flight.mill_without_play &&
                                (state.round.running > flight.mill_reveal_round_before ||
                                 state.total_score > flight.mill_reveal_total_before) &&
                                state.deck.remaining() > 0;
    const bool miracle_bonus = flight.is_miracle_bonus && flight.play_resolved;
    const bool discard_resolved = flight.is_discard && flight.play_resolved &&
                                  flight.origin == PlayPresentOrigin::HAND;
    const int discard_index = flight.hand_index;

    if(flight.play_resolved && !flight.is_discard && !flight.is_miracle_bonus)
    {
        if(flight.mill_without_play)
        {
            graveyard_push(state, flight.played_ref);
        }
        else if(flight.origin == PlayPresentOrigin::HAND ||
                flight.origin == PlayPresentOrigin::GRAVEYARD)
        {
            if(!flight.hand_committed)
            {
                commit_play_flight_destination(flight);
            }
        }
        else if(flight.origin == PlayPresentOrigin::ECHO)
        {
            increment_play_counters(state, flight.played_ref.type, PlaySource::ECHO);
            maybe_draw_if_solo(state, flight.played_ref.type);
            trinket_queue_proc(state, TrinketType::ECHO);
            state.consume_echo();

            const int echo_slot = trinket_slot_index(state, TrinketType::ECHO);

            if(echo_slot >= 0)
            {
                hud.start_trinket_wiggle(echo_slot);
            }

            if(flight.played_ref.type == CardType::SWIVEL)
            {
                swivel_on_swivel_played(*this);
                swivel_clear_wait_if_hand_empty(*this);
            }

            echo_play_badge_active = false;
        }
        else if(flight.style == RemovalStyle::TO_DECK_TOP)
        {
            state.deck.insert_top(flight.played_ref);
        }
        else
        {
            const PostPlayDestination dest = route_played_card(
                state, flight.played_ref.type, flight.play_context.source, flight.swivel_follow);
            apply_post_play_destination(state, flight.played_ref, flight.play_context.source, dest,
                                        flight.play_context.hand_index,
                                        flight.play_context.selected_card);
            increment_play_counters(state, flight.played_ref.type, flight.play_context.source);
            maybe_draw_if_solo(state, flight.played_ref.type);

            if(flight.play_context.source != PlaySource::ECHO)
            {
                arm_echo_replay(flight.played_ref, flight.scoring_source, flight.start_x, flight.start_y);
            }
        }
    }

    if(discard_resolved)
    {
        shift_card_raise_after_remove(discard_index >= 0 ? discard_index : selected_card);
    }

    if(mill_draw)
    {
        try_draw_one_to_hand(state);
    }

    if(mill_flex)
    {
        PendingAction action;
        action.type = PendingActionType::MILL_REVEAL;
        action.count = 1;
        state.pending_actions.push_back(action);
    }

    if(mill_waterfall)
    {
        PendingAction action;
        action.type = PendingActionType::MILL_REVEAL;
        action.count = 2;
        state.pending_actions.push_back(action);
    }

    if(pending_opening_hand_deal)
    {
        pending_opening_hand_deal = false;

        while(state.hand.size() < 5 && state.deck.remaining() > 0)
        {
            CardRef card;

            if(!deck.draw(card))
            {
                break;
            }

            hand_add_card(state, card, true);
        }

        for(int extra_draw = 0; extra_draw < state.round_start_extra_draws; ++extra_draw)
        {
            CardRef card;

            if(!deck.draw(card))
            {
                break;
            }

            hand_add_card(state, card, true);
        }

        state.round_start_extra_draws = 0;
        selected_card = 0;
    }

    const int cycle_draws = pending_cycle_draws + (cycle_draw ? 1 : 0);
    pending_cycle_draws = 0;

    clear_play_flight(flight);
    sync_removing_card();
    update_target_scroll();

    if(play_flight_count() > 0)
    {
        pending_cycle_draws = cycle_draws;
        draw_total_score();
        return;
    }

    reset_card_animation_state();
    draw_total_score();

    if(cycle_draws > 0)
    {
        state.effect_draw_remaining += cycle_draws;
        state.effect_draw_miracle_first = true;
        state.effect_draw_miracle_chaining = false;
        advance_effect_draw();
    }

    if(miracle_bonus)
    {
        state.effect_draw_miracle_chaining = false;

        if(state.effect_draw_remaining > 0)
        {
            advance_effect_draw();
        }
    }

    if(hand_draw_fx_blocking())
    {
        return;
    }

    continue_effect_draw_batch();

    if(hand_draw_fx_blocking() || removing_card)
    {
        return;
    }

    if(swivel_follow)
    {
        swivel_complete_follow(*this);
    }
    else
    {
        begin_next_pending_or_finish();
    }
}

void GameContext::tick_one_play_flight(PlayFlight& flight)
{
    ++flight.frame;

    if(!flight.center_beat)
    {
        if(flight.frame < game_layout::REMOVAL_FRAMES)
        {
            return;
        }

        if(flight.origin != PlayPresentOrigin::HAND)
        {
            resolve_play_flight_effects(flight);
            complete_play_flight(flight);
            return;
        }

        if(flight.hand_committed)
        {
            complete_play_flight(flight);
            return;
        }

        if(flight.is_discard)
        {
            resolve_play_flight_effects(flight);
            complete_play_flight(flight);
            return;
        }

        if(flight.cycle_exile && selected_card >= 0 && selected_card < state.hand.size())
        {
            const int removed_index = flight.hand_index >= 0 ? flight.hand_index : selected_card;
            battle_stat_record_cycle(state);
            battle_stat_record_keyword_discard(state);
            hand_remove_at_exiled(state, selected_card, selected_card);
            shift_card_raise_after_remove(removed_index);
            draw_round_score();
            flight.hand_committed = true;
            flight.play_resolved = true;
        }
        else if(flight.style == RemovalStyle::TO_DECK_TOP && !flight.swivel_follow &&
                selected_card >= 0 && selected_card < state.hand.size())
        {
            const int removed_index = flight.hand_index >= 0 ? flight.hand_index : selected_card;

            if(state.selection.type == PendingActionType::PUT_HAND_ON_DECK_TOP)
            {
                apply_card_relocated(state, state.hand[selected_card].type);
            }

            hand_remove_at_to_deck_top(state, selected_card, selected_card);
            shift_card_raise_after_remove(removed_index);
            flight.hand_committed = true;
            flight.play_resolved = true;
        }
        else if(selected_card >= 0 && selected_card < state.hand.size())
        {
            resolve_play_flight_effects(flight);
            commit_play_flight_destination(flight);
        }

        complete_play_flight(flight);
        return;
    }

    switch(flight.phase)
    {
    case PlayRemovalPhase::APPROACH:
        {
            const int approach_frames = flight.origin == PlayPresentOrigin::DECK
                                            ? game_layout::PLAY_DECK_APPROACH_FRAMES
                                            : game_layout::PLAY_APPROACH_FRAMES;

            if(flight.frame >= approach_frames)
            {
                bool other_at_center = false;

                for(const PlayFlight& other : play_flights)
                {
                    if(&other == &flight || !other.active || !other.center_beat)
                    {
                        continue;
                    }

                    if(other.phase == PlayRemovalPhase::APPROACH || other.phase == PlayRemovalPhase::HOLD)
                    {
                        other_at_center = true;
                        break;
                    }
                }

                if(!other_at_center)
                {
                    flight.frame = 0;
                    flight.phase = PlayRemovalPhase::HOLD;
                }
                else
                {
                    flight.frame = approach_frames;
                }
            }
        }

        break;

    case PlayRemovalPhase::HOLD:
        if(flight.frame >= game_layout::PLAY_HOLD_FRAMES)
        {
            resolve_play_flight_effects(flight);
            flight.frame = 0;
            flight.phase = PlayRemovalPhase::DEPART;
            commit_play_flight_destination(flight);
        }

        break;

    case PlayRemovalPhase::DEPART:
        if(flight.frame >= game_layout::PLAY_DEPART_FRAMES)
        {
            flight.frame = 0;

            if(presentation_fx_blocking())
            {
                flight.phase = PlayRemovalPhase::WAIT_PRESENTATION;
            }
            else
            {
                complete_play_flight(flight);
            }
        }

        break;

    case PlayRemovalPhase::WAIT_PRESENTATION:
        if(!presentation_fx_blocking())
        {
            complete_play_flight(flight);
        }

        break;

    default:
        break;
    }
}

bool GameContext::tick_removal_fx()
{
    if(play_flight_count() <= 0)
    {
        removing_card = false;
        return false;
    }

    for(int index = 0; index < game_layout::MAX_PLAY_FLIGHTS; ++index)
    {
        if(play_flights[index].active)
        {
            tick_one_play_flight(play_flights[index]);
        }
    }

    sync_removing_card();
    return removing_card;
}

void GameContext::deal_opening_hand()
{
    state.hand.clear();
    state.first_deck_draw_this_round = true;

    CardRef first;

    if(deck.draw(first))
    {
        if(first.type == CardType::MIRACLE)
        {
            const int main_x = main_panel_offset_x();
            PlayResolutionContext context;
            context.source = PlaySource::DECK_TOP;
            context.apply_destination = false;
            pending_opening_hand_deal = true;
            begin_play_presentation(
                first,
                card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x),
                card_target_y_for_hud_icon(game_layout::HUD_DECK_Y),
                PlayPresentOrigin::DECK,
                context,
                RemovalStyle::TO_GRAVEYARD,
                true);
            selected_card = 0;
            return;
        }

        hand_add_card(state, first, true);
    }

    while(scheduled_hand_count() < 5 && deck.remaining() > 0)
    {
        CardRef card;

        if(!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    for(int extra_draw = 0; extra_draw < state.round_start_extra_draws; ++extra_draw)
    {
        CardRef card;

        if(!deck.draw(card))
        {
            break;
        }

        hand_add_card(state, card, true);
    }

    state.round_start_extra_draws = 0;
    selected_card = 0;
}

void GameContext::capture_removal_start()
{
    clamp_hand_cursor();
    const int main_x = main_panel_offset_x();
    const int raise = selected_card >= 0 && selected_card < card_raise_offset.size()
                          ? card_raise_offset[selected_card]
                          : 0;
    removal_start_x = hand_x + selected_card * game_layout::HAND_SPACING - scroll_x + edge_shift + main_x;
    removal_start_y = game_layout::HAND_Y - raise;
}

bool GameContext::confirm_pressed() const
{
    return confirm_input_armed &&
           (bn::keypad::up_pressed() || bn::keypad::a_pressed());
}

void GameContext::hand_slot_screen_position(int hand_index, int main_x, int& out_x, int& out_y) const
{
    const int raise = hand_index >= 0 && hand_index < card_raise_offset.size() ? card_raise_offset[hand_index] : 0;
    out_x = hand_x + hand_index * game_layout::HAND_SPACING - scroll_x + edge_shift + main_x;
    out_y = game_layout::HAND_Y - raise;
}

int GameContext::graveyard_card_fx_frame_count() const
{
    switch(graveyard_card_fx_kind)
    {
    case GraveyardExilePickKind::TO_HAND:
    case GraveyardExilePickKind::TO_DECK_TOP:
    case GraveyardExilePickKind::SHUFFLE_TO_DECK:
        return game_layout::ZONE_TRANSFER_FRAMES;

    case GraveyardExilePickKind::MULTIPLY_BY_COUNT:
    case GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY:
    case GraveyardExilePickKind::NONE:
    default:
        return game_layout::REMOVAL_FRAMES;
    }
}

void GameContext::begin_graveyard_card_fx(GraveyardExilePickKind pick_kind)
{
    const int main_x = main_panel_offset_x();
    int start_x = 0;
    int start_y = 0;

    const int cursor = state.selection.cursor;
    const int cursor_raise = cursor < card_raise_offset.size() ? card_raise_offset[cursor] : 0;

    if(!graveyard_cursor_screen_position(cursor, state.graveyard.size(), game_layout::GRAVE_SPACING,
                                         game_layout::GRAVEYARD_BROWSE_Y, cursor_raise, row_scroll_x, main_x,
                                         start_x, start_y))
    {
        return;
    }

    graveyard_card_fx_active = true;
    graveyard_card_fx_frame = 0;
    graveyard_card_fx_start_x = start_x;
    graveyard_card_fx_start_y = start_y;
    const CardRef picked = state.graveyard[state.selection.cursor];
    graveyard_card_fx_type = picked.type;
    graveyard_card_fx_instance_id = picked.instance_id;
    graveyard_card_fx_index = state.selection.cursor;
    graveyard_card_fx_kind = pick_kind;

    if(pick_kind == GraveyardExilePickKind::TO_HAND)
    {
        graveyard_card_fx_style = RemovalStyle::TO_GRAVEYARD;
        hand_slot_screen_position(state.hand.size(), main_x, graveyard_card_fx_dest_x, graveyard_card_fx_dest_y);
    }
    else if(pick_kind == GraveyardExilePickKind::TO_DECK_TOP ||
            pick_kind == GraveyardExilePickKind::SHUFFLE_TO_DECK)
    {
        graveyard_card_fx_style = RemovalStyle::TO_DECK_TOP;
        graveyard_card_fx_dest_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
        graveyard_card_fx_dest_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    }
    else
    {
        graveyard_card_fx_style = RemovalStyle::EXILE_DISSIPATE;
        graveyard_card_fx_dest_x = card_target_x_for_score_center(main_x);
        graveyard_card_fx_dest_y = card_target_y_for_score_center();
    }
}

void GameContext::begin_keep_going_round_transfers(bool turtle_preserve)
{
    deferred_round_start_pending = true;
    deferred_round_start_turtle_preserve = turtle_preserve;

    keep_going_transfers_remaining = state.keep_going_returns[state.next_mod_index];
    state.keep_going_returns[state.next_mod_index] = 0;

    if(keep_going_transfers_remaining <= 0 || state.graveyard.empty())
    {
        finish_deferred_round_start();
        return;
    }

    state.deck.compact();
    try_begin_keep_going_transfer();
}

void GameContext::try_begin_keep_going_transfer()
{
    if(keep_going_transfers_remaining <= 0 || state.graveyard.empty())
    {
        keep_going_transfer_active = false;
        keep_going_transfers_remaining = 0;
        state.deck.apply_gravity(state.instance_pool);
        finish_deferred_round_start();
        return;
    }

    const int index = state.rng.get_int(state.graveyard.size());
    state.selection.cursor = index;
    sync_row_scroll_for_mode(index, state.graveyard.size(), game_layout::GRAVE_SPACING);
    keep_going_transfer_active = true;
    begin_graveyard_card_fx(GraveyardExilePickKind::TO_DECK_TOP);
}

void GameContext::finish_deferred_round_start()
{
    if(!deferred_round_start_pending)
    {
        return;
    }

    deferred_round_start_pending = false;
    keep_going_transfer_active = false;
    keep_going_transfers_remaining = 0;

    if(deck.empty())
    {
        if(state.turtle_rounds_remaining > 0)
        {
            state.round.reset();
        }

        game_over = true;
        run_finished = true;
        scene_result.final_score = state.total_score;
        return;
    }

    state.start_new_round(deferred_round_start_turtle_preserve);
    reset_card_animation_state();
    deal_opening_hand();
    try_start_hand_draw_fx();
    process_instant_pending();
    draw_round_score();
    draw_total_score();
}

void GameContext::complete_graveyard_card_fx()
{
    graveyard_card_fx_active = false;
    graveyard_card_fx_frame = 0;
    release_card_display_tiles(exclusive_fx_card());

    const GraveyardExilePickKind kind = graveyard_card_fx_kind;
    graveyard_card_fx_kind = GraveyardExilePickKind::NONE;
    graveyard_card_fx_instance_id = NO_INSTANCE;

    if(kind == GraveyardExilePickKind::SHUFFLE_TO_DECK)
    {
        necromancy_shuffle_graveyard_to_deck(state);
        begin_next_pending_or_finish();
        return;
    }

    if(graveyard_card_fx_index < 0 || graveyard_card_fx_index >= state.graveyard.size())
    {
        begin_next_pending_or_finish();
        return;
    }

    const CardRef card = state.graveyard[graveyard_card_fx_index];
    graveyard_remove_at(state, graveyard_card_fx_index);

    if(kind == GraveyardExilePickKind::TO_HAND)
    {
        apply_card_relocated(state, card.type);

        if(!state.hand.full())
        {
            hand_add_card(state, card);
        }

        draw_round_score();
        begin_next_pending_or_finish();
        return;
    }

    if(kind == GraveyardExilePickKind::TO_DECK_TOP)
    {
        apply_card_relocated(state, card.type);
        if(state.selection.type == PendingActionType::BIRDS_RETURN)
        {
            state.deck.add_card(card);
        }
        else
        {
            state.deck.insert_top(card);
        }
        draw_round_score();

        if(keep_going_transfer_active)
        {
            keep_going_transfer_active = false;
            --keep_going_transfers_remaining;

            if(keep_going_transfers_remaining > 0 && !state.graveyard.empty())
            {
                try_begin_keep_going_transfer();
                return;
            }

            state.deck.apply_gravity(state.instance_pool);
            finish_deferred_round_start();
            return;
        }

        if(state.selection.type == PendingActionType::BIRDS_RETURN)
        {
            --state.birds_return_count;

            if(try_begin_birds_return_fx())
            {
                return;
            }

            if(state.birds_return_count > 0)
            {
                resolve_birds_return_instantly();
            }
            else
            {
                complete_birds_return();
            }

            begin_next_pending_or_finish();
            return;
        }

        begin_next_pending_or_finish();
        return;
    }

    exile_push(state, card, true);
    ++state.selection.exiled_count;

    if(state.graveyard.empty())
    {
        state.selection.cursor = 0;
    }
    else
    {
        state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
    }

    sync_target_row_scroll(state.selection.cursor, state.graveyard.size());

    if(kind == GraveyardExilePickKind::MULTIPLY_BY_COUNT ||
       kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY)
    {
        if(combo_try_start_pending(state))
        {
            enter_combo_mode();
            return;
        }
    }

    if(kind == GraveyardExilePickKind::EXILE_ONE)
    {
        draw_round_score();
        begin_next_pending_or_finish();
    }
    else if(kind == GraveyardExilePickKind::MULTIPLY_BY_COUNT)
    {
        if(state.graveyard.empty())
        {
            state.mul_from_card(state.selection.exiled_count);
            draw_round_score();
            begin_next_pending_or_finish();
        }
    }
    else if(kind == GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY)
    {
        --state.selection.remaining_picks;

        if(state.selection.remaining_picks <= 0)
        {
            state.mul_from_card(state.selection.multiply_factor);
            draw_round_score();
            begin_next_pending_or_finish();
        }
        else if(state.graveyard.empty())
        {
            begin_next_pending_or_finish();
        }
    }
}

bool GameContext::try_begin_birds_return_fx()
{
    if(state.birds_return_count <= 0)
    {
        return false;
    }

    const int index = state.birds_return_start;

    if(index < 0 || index >= state.graveyard.size() ||
       state.graveyard[index].type != CardType::BIRDS_OF_A_FEATHER)
    {
        return false;
    }

    state.selection.cursor = index;
    sync_row_scroll_for_mode(index, state.graveyard.size(), game_layout::GRAVE_SPACING);
    begin_graveyard_card_fx(GraveyardExilePickKind::TO_DECK_TOP);
    return graveyard_card_fx_active;
}

void GameContext::complete_birds_return()
{
    const bool finished_run = state.birds_return_start >= 0 && state.birds_return_count == 0;

    state.birds_return_start = -1;
    state.birds_return_count = 0;

    if(finished_run)
    {
        state.deck.apply_gravity(state.instance_pool);
        state.birds_return_threshold =
            state.birds_return_threshold == 5 ? 6 : state.birds_return_threshold + 1;
        combo_check_zone(state, ComboZone::GRAVEYARD);
    }
}

void GameContext::resolve_birds_return_instantly()
{
    while(state.birds_return_count > 0)
    {
        const int index = state.birds_return_start;

        if(index < 0 || index >= state.graveyard.size() ||
           state.graveyard[index].type != CardType::BIRDS_OF_A_FEATHER)
        {
            break;
        }

        const CardRef card = state.graveyard[index];
        graveyard_remove_at(state, index);
        apply_card_relocated(state, card.type);
        state.deck.add_card(card);
        --state.birds_return_count;
    }

    complete_birds_return();
    draw_round_score();
}

bool GameContext::deck_search_resolve_active() const
{
    return deck_search_resolve_fx.active;
}

bool GameContext::hand_draw_fx_blocking() const
{
    return hand_draw_fx_active || !state.pending_hand_draws.empty();
}

int GameContext::scheduled_hand_count() const
{
    return hand_scheduled_count(state, hand_draw_fx_active);
}

void GameContext::try_start_hand_draw_fx()
{
    if(hand_draw_fx_active || state.pending_hand_draws.empty())
    {
        return;
    }

    if(graveyard_card_fx_active || deck_search_resolve_active() || lucky_sevens_fx.active ||
       removing_card)
    {
        return;
    }

    const PendingHandDraw pending = state.pending_hand_draws.front();
    state.pending_hand_draws.erase(state.pending_hand_draws.begin());

    const int main_x = main_panel_offset_x();
    hand_draw_fx_active = true;
    hand_draw_fx_frame = 0;
    hand_draw_fx_card = pending.card;
    hand_draw_fx_miracle_auto = pending.miracle_auto_play;
    hand_draw_fx_dest_index = state.hand.size();
    hand_draw_fx_start_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
    hand_draw_fx_start_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    hand_slot_screen_position(hand_draw_fx_dest_index, main_x, hand_draw_fx_dest_x, hand_draw_fx_dest_y);
}

void GameContext::tick_hand_draw_fx()
{
    if(!hand_draw_fx_active)
    {
        try_start_hand_draw_fx();
        return;
    }

    ++hand_draw_fx_frame;

    if(hand_draw_fx_frame >= game_layout::HAND_DRAW_FRAMES)
    {
        complete_hand_draw_fx();
    }
}

void GameContext::complete_hand_draw_fx()
{
    hand_draw_fx_active = false;
    hand_draw_fx_frame = 0;
    release_card_display_tiles(exclusive_fx_card());

    if(state.hand.full())
    {
        hand_draw_fx_miracle_auto = false;
        return;
    }

    const bool miracle_auto_play =
        hand_draw_fx_miracle_auto && hand_draw_fx_card.type == CardType::MIRACLE;
    hand_draw_fx_miracle_auto = false;

    const bool was_empty = state.hand.empty();
    state.hand.push_back(hand_draw_fx_card);
    battle_stat_record_draw_to_hand(state);
    game_events_dispatch(state, GameEvent::HAND_CHANGED);

    if(was_empty)
    {
        selected_card = 0;
    }

    update_target_scroll();

    if(miracle_auto_play)
    {
        const int miracle_index = state.hand.size() - 1;
        play_miracle_bonus(state, 10);
        hand_remove_at_to_graveyard(state, miracle_index, selected_card);
        draw_round_score();
    }

    try_start_hand_draw_fx();

    if(!hand_draw_fx_blocking())
    {
        continue_effect_draw_batch();

        if(!hand_draw_fx_blocking() && !removing_card)
        {
            begin_next_pending_or_finish();
        }
    }
}

bool GameContext::presentation_fx_blocking() const
{
    if(lucky_sevens_fx.active || !pending_lucky_sevens.empty())
    {
        return true;
    }

    if(state.deferred_morel_count > 0)
    {
        return true;
    }

    if(score_count_is_active(*this, TrinketScoreField::ROUND) ||
       score_count_is_active(*this, TrinketScoreField::TOTAL))
    {
        return true;
    }

    if(!score_pops.empty())
    {
        return true;
    }

    if(!state.pending_score_checks.empty() || !state.pending_score_pops.empty() ||
       !state.pending_score_counts.empty())
    {
        return true;
    }

    return false;
}

bool GameContext::card_resolution_blocking_round_end() const
{
    return mode != GameMode::NORMAL ||
           removing_card ||
           graveyard_card_fx_active ||
           deferred_round_start_pending ||
           deck_search_resolve_active() ||
           hand_draw_fx_blocking() ||
           presentation_fx_blocking() ||
           !state.pending_actions.empty() ||
           state.echo_pending_replay ||
           state.swivel_waiting;
}

void GameContext::advance_effect_draw()
{
    if(removing_card || hand_draw_fx_blocking())
    {
        return;
    }

    if(state.effect_draw_remaining <= 0)
    {
        state.effect_draw_miracle_first = false;
        state.effect_draw_miracle_chaining = false;
        return;
    }

    CardRef drawn;

    if(!deck.draw(drawn))
    {
        state.effect_draw_remaining = 0;
        state.effect_draw_miracle_first = false;
        state.effect_draw_miracle_chaining = false;
        return;
    }

    const bool allow_miracle = state.effect_draw_miracle_chaining ||
                               (state.effect_draw_miracle_first && state.effect_draw_remaining > 0);

    if(drawn.type == CardType::MIRACLE && allow_miracle)
    {
        if(state.effect_draw_miracle_first)
        {
            state.effect_draw_miracle_first = false;
        }

        state.effect_draw_miracle_chaining = true;
        state.first_deck_draw_this_round = false;
        --state.effect_draw_remaining;

        const int main_x = main_panel_offset_x();
        PlayResolutionContext context;
        context.source = PlaySource::DECK_TOP;
        context.apply_destination = false;
        begin_play_presentation(
            drawn,
            card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x),
            card_target_y_for_hud_icon(game_layout::HUD_DECK_Y),
            PlayPresentOrigin::DECK,
            context,
            RemovalStyle::TO_GRAVEYARD,
            true);
        return;
    }

    const bool miracle_auto_play = drawn.type == CardType::MIRACLE &&
                                   (state.effect_draw_miracle_first || state.effect_draw_miracle_chaining);
    state.effect_draw_miracle_chaining = false;
    state.effect_draw_miracle_first = false;
    hand_add_card(state, drawn, true, miracle_auto_play);
    --state.effect_draw_remaining;
}

void GameContext::continue_effect_draw_batch()
{
    if(state.effect_draw_remaining > 0 && !hand_draw_fx_blocking() && !removing_card)
    {
        advance_effect_draw();
    }

    if(state.effect_draw_remaining <= 0)
    {
        state.effect_draw_miracle_chaining = false;
        state.effect_draw_miracle_first = false;
    }
}

void GameContext::arm_echo_replay(CardRef played, PlaySource scoring_source, int ghost_x, int ghost_y)
{
    if(!state.echo_first_play_active() || !card_has_play_effect(state, played))
    {
        echo_play_badge_active = false;
        return;
    }

    if(!echo_ghost_active)
    {
        echo_ghost_x = ghost_x;
        echo_ghost_y = ghost_y;
    }

    state.echo_pending_replay = true;
    state.echo_replay_card = played;
    state.echo_replay_scoring_source = scoring_source;
    echo_play_badge_active = true;
}

bool GameContext::try_drain_echo_replay()
{
    if(!state.echo_pending_replay)
    {
        return false;
    }

    if(!state.pending_actions.empty() || state.swivel_waiting || presentation_fx_blocking() ||
       deck_search_resolve_active() || removing_card)
    {
        return false;
    }

    const CardRef replay = state.echo_replay_card;
    state.echo_pending_replay = false;
    echo_play_badge_active = true;
    echo_ghost_active = false;
    echo_ghost_card.set_visible(false);

    PlayResolutionContext context;
    context.source = PlaySource::ECHO;
    context.apply_destination = false;

    begin_play_presentation(
        replay,
        echo_ghost_x,
        echo_ghost_y,
        PlayPresentOrigin::ECHO,
        context,
        removal_style_for_hand_play(replay.type));

    return true;
}

void GameContext::tick_echo_pending()
{
    if(!state.echo_pending_replay || mode != GameMode::NORMAL)
    {
        return;
    }

    if(swapping_card || graveyard_card_fx_active || deck_search_resolve_active() ||
       hand_draw_fx_blocking())
    {
        return;
    }

    if(!try_drain_echo_replay())
    {
        swivel_clear_wait_if_hand_empty(*this);

        if(empty_hand_triggers_round_end(state) && !removing_card && !presentation_fx_blocking() &&
           !hand_draw_fx_blocking() && !block_round_end_for_combo())
        {
            finish_empty_hand_round();
        }

        return;
    }

    if(!state.pending_actions.empty())
    {
        begin_next_pending_or_finish();
        return;
    }

    if(empty_hand_triggers_round_end(state))
    {
        if(block_round_end_for_combo())
        {
            return;
        }

        if(presentation_fx_blocking())
        {
            round_end_pending = true;
        }
        else
        {
            finish_empty_hand_round();
        }
    }

    update_target_scroll();
}

void GameContext::begin_deck_search_resolve(CardRef played, int start_x, int start_y, bool swivel_follow)
{
    const CardData& data = card_data(played.type);
    RemovalStyle style = RemovalStyle::TO_GRAVEYARD;

    if(data.exiles_self_on_play)
    {
        style = RemovalStyle::EXILE_DISSIPATE;
    }
    else if(swivel_follow || state.swivel_waiting)
    {
        style = RemovalStyle::TO_DECK_TOP;
    }

    if(swivel_follow)
    {
        state.swivel_waiting = false;
    }

    deck_search_resolve_fx.active = true;
    deck_search_resolve_fx.phase = DeckSearchResolvePhase::PICK_FLIGHT;
    deck_search_resolve_fx.frame = 0;
    deck_search_resolve_fx.picked_type = played.type;
    deck_search_resolve_fx.picked_instance_id = played.instance_id;
    deck_search_resolve_fx.start_x = start_x;
    deck_search_resolve_fx.start_y = start_y;
    deck_search_resolve_fx.removal_style = style;
    deck_search_resolve_fx.swivel_follow = swivel_follow;

    state.selection = SelectionSession{};
    mode = GameMode::NORMAL;
    row_scroll_x = 0;
    target_row_scroll_x = 0;
    target_row_scroll_index = 0;
}

void GameContext::finish_deck_search_resolve()
{
    const bool swivel_follow = deck_search_resolve_fx.swivel_follow;
    const CardRef picked{
        deck_search_resolve_fx.picked_type,
        deck_search_resolve_fx.picked_instance_id,
    };

    deck_search_resolve_fx.active = false;
    deck_search_resolve_fx.frame = 0;
    deck_search_resolve_fx.phase = DeckSearchResolvePhase::PICK_FLIGHT;
    deck_search_resolve_fx.swivel_follow = false;
    deck_search_resolve_fx.picked_instance_id = NO_INSTANCE;
    release_card_display_tiles(exclusive_fx_card());
    echo_badge.set_visible(false);

    if(swivel_follow)
    {
        state.deck.insert_top(picked);
        swivel_complete_follow(*this);
        return;
    }

    begin_next_pending_or_finish();
}

void GameContext::tick_deck_search_resolve()
{
    if(!deck_search_resolve_fx.active)
    {
        return;
    }

    switch(deck_search_resolve_fx.phase)
    {
    case DeckSearchResolvePhase::PICK_FLIGHT:
        ++deck_search_resolve_fx.frame;

        if(deck_search_resolve_fx.frame >= game_layout::REMOVAL_FRAMES)
        {
            deck_search_resolve_fx.frame = 0;
            release_card_display_tiles(exclusive_fx_card());
            deck_search_resolve_fx.phase = DeckSearchResolvePhase::WAIT_PRESENTATION;
        }

        break;

    case DeckSearchResolvePhase::WAIT_PRESENTATION:
        if(!presentation_fx_blocking())
        {
            finish_deck_search_resolve();
        }

        break;

    default:
        deck_search_resolve_fx.active = false;
        break;
    }
}

void GameContext::shift_card_raise_after_remove(int removed_index)
{
    if(removed_index < 0)
    {
        return;
    }

    const int new_size = state.hand.size();

    for(int index = removed_index; index < new_size; ++index)
    {
        card_raise_offset[index] = card_raise_offset[index + 1];
    }

    if(new_size < card_raise_offset.size())
    {
        card_raise_offset[new_size] = 0;
    }

    snap_selected_card_raise();
}

// Draw the name + wrapped description of a card into the inspect panel.
void GameContext::clear_inspect()
{
    inspecting = false;
    inspect_sprites.clear();
    inspect_shown_index = -1;
    last_inspect_sprite_offset = 0;
    hide_inspect_card(inspect_card);
}

void GameContext::draw_inspect(CardType type)
{
    const CardInstance* instance = nullptr;
    CardRef ref{};
    bool have_ref = false;

    if(mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK)
    {
        if(!state.graveyard.empty())
        {
            const int cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
            ref = state.graveyard[cursor];
            have_ref = true;
        }
    }
    else if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
    {
        const bn::span<const CardRef> cards = active_browse_cards();

        if(!cards.empty())
        {
            const int cursor = clamp_graveyard_cursor(browse_cursor, cards.size());
            ref = cards[cursor];
            have_ref = true;
        }
    }
    else if(mode == GameMode::SCRY)
    {
        if(!state.selection.scry_buffer.empty())
        {
            ref = state.selection.scry_buffer[state.selection.cursor];
            have_ref = true;
        }
    }
    else if(mode == GameMode::DECK_SEARCH)
    {
        if(!state.selection.deck_search_buffer.empty())
        {
            ref = state.selection.deck_search_buffer[state.selection.cursor];
            have_ref = true;
        }
    }
    else if(mode != GameMode::COMBO && selected_card >= 0 &&
            selected_card < playable_slot_count(state))
    {
        ref = playable_slot_card(state, selected_card);
        have_ref = true;
    }

    if(have_ref && ref.has_instance())
    {
        instance = instance_at(state.instance_pool, ref.instance_id);
    }

    last_inspect_sprite_offset = 0;
    show_inspect_card(inspect_card, type, instance, &hud_count_generator);
    draw_card_inspect(type, round_text_generator, inspect_text_generator, inspect_sprites,
                      inspect_layout::TITLE_Y, instance);

    if(state.pending_double_adds)
    {
        inspect_text_generator.set_left_alignment();
        inspect_text_generator.generate(inspect_layout::TEXT_X, inspect_layout::TITLE_Y + 72,
                                        "Adds are doubled.", inspect_sprites);
        elevate_inspect_sprites(inspect_sprites);
    }
}

bool GameContext::panel_transition_active() const
{
    return panel_transition != PanelTransition::NONE;
}

bool GameContext::show_details_layer() const
{
    return side_panel == SidePanel::DETAILS ||
           panel_transition == PanelTransition::OPEN_DETAILS ||
           panel_transition == PanelTransition::CLOSE_DETAILS;
}

bool GameContext::show_graveyard_layer() const
{
    return side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE ||
           panel_transition == PanelTransition::OPEN_GRAVEYARD ||
           panel_transition == PanelTransition::CLOSE_GRAVEYARD;
}

bool GameContext::graveyard_pick_active() const
{
    return mode == GameMode::GRAVEYARD_TARGET || mode == GameMode::GRAVEYARD_PICK;
}

bool GameContext::card_selection_ui_active() const
{
    return mode == GameMode::SCRY || mode == GameMode::DECK_SEARCH ||
           mode == GameMode::GRAVEYARD_TARGET || graveyard_pick_active();
}

bn::span<const CardRef> GameContext::active_browse_cards() const
{
    if(side_panel == SidePanel::EXILE ||
       (panel_transition == PanelTransition::OPEN_GRAVEYARD && browse_open_target == SidePanel::EXILE))
    {
        return bn::span<const CardRef>(state.exile.data(), state.exile.size());
    }

    return bn::span<const CardRef>(state.graveyard.data(), state.graveyard.size());
}

void GameContext::prepare_browse_for_panel(SidePanel panel)
{
    const bn::span<const CardRef> cards =
        panel == SidePanel::EXILE
            ? bn::span<const CardRef>(state.exile.data(), state.exile.size())
            : bn::span<const CardRef>(state.graveyard.data(), state.graveyard.size());

    browse_cursor = cards.empty() ? 0 : cards.size() - 1;
    sync_row_scroll_for_mode(browse_cursor, cards.size(), game_layout::GRAVE_SPACING);
}

void GameContext::switch_side_panel(SidePanel target)
{
    if(target == side_panel && panel_transition == PanelTransition::NONE)
    {
        return;
    }

    const int main_before = main_panel_offset_x();
    const int details_before = details_panel_offset_x();
    const int grave_before = graveyard_panel_offset_x();

    clear_inspect();

    if(side_panel == SidePanel::DETAILS || target != SidePanel::DETAILS)
    {
        details_sprites.clear();
        details_last_library = -1;
        details_last_round = -1;
        details_last_goal = -1;
        details_last_mode = int(CampaignMode::NONE);
        last_details_sprite_offset = details_before;
    }

    side_panel = target;
    panel_transition = PanelTransition::NONE;
    panel_slide = 0;

    if(target == SidePanel::DETAILS)
    {
        sync_details_panel(true);
    }
    else if(target == SidePanel::EXILE || target == SidePanel::GRAVEYARD)
    {
        browse_open_target = target;
        prepare_browse_for_panel(target);
    }
    else
    {
        row_scroll_x = 0;
        target_row_scroll_x = 0;
        target_row_scroll_index = 0;
    }

    last_main_sprite_offset = main_before;
    position_main_score_sprites();
    last_details_sprite_offset = details_before;
    position_details_sprites();
    // Grave browse repositions every frame from panel_x; force last so next delta is correct.
    last_inspect_sprite_offset = grave_before;
}

void GameContext::cycle_side_panel(int direction)
{
    if(panel_transition_active() || direction == 0)
    {
        return;
    }

    constexpr SidePanel ORDER[] = {
        SidePanel::NONE,
        SidePanel::DETAILS,
        SidePanel::EXILE,
        SidePanel::GRAVEYARD,
    };
    constexpr int ORDER_COUNT = 4;

    int index = 0;

    for(int i = 0; i < ORDER_COUNT; ++i)
    {
        if(ORDER[i] == side_panel)
        {
            index = i;
            break;
        }
    }

    int next = index + direction;

    while(next < 0)
    {
        next += ORDER_COUNT;
    }

    next %= ORDER_COUNT;
    const SidePanel target = ORDER[next];

    if(target == side_panel)
    {
        return;
    }

    if(side_panel == SidePanel::NONE)
    {
        if(target == SidePanel::DETAILS)
        {
            begin_panel_transition(PanelTransition::OPEN_DETAILS);
            sync_details_panel(true);
        }
        else
        {
            browse_open_target = target;
            prepare_browse_for_panel(target);
            begin_panel_transition(PanelTransition::OPEN_GRAVEYARD);
        }

        return;
    }

    if(target == SidePanel::NONE)
    {
        if(side_panel == SidePanel::DETAILS)
        {
            begin_panel_transition(PanelTransition::CLOSE_DETAILS);
        }
        else
        {
            begin_panel_transition(PanelTransition::CLOSE_GRAVEYARD);
        }

        return;
    }

    switch_side_panel(target);
}

int GameContext::main_panel_offset_x() const
{
    switch(panel_transition)
    {
    case PanelTransition::OPEN_DETAILS:
        return panel_slide;

    case PanelTransition::CLOSE_DETAILS:
        return game_layout::PANEL_WIDTH - panel_slide;

    case PanelTransition::OPEN_GRAVEYARD:
        return -panel_slide;

    case PanelTransition::CLOSE_GRAVEYARD:
        return -(game_layout::PANEL_WIDTH - panel_slide);

    default:
        if(side_panel == SidePanel::DETAILS)
        {
            return game_layout::PANEL_WIDTH;
        }

        if(side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
        {
            return -game_layout::PANEL_WIDTH;
        }

        return 0;
    }
}

int GameContext::details_panel_offset_x() const
{
    switch(panel_transition)
    {
    case PanelTransition::OPEN_DETAILS:
        return -game_layout::PANEL_WIDTH + panel_slide;

    case PanelTransition::CLOSE_DETAILS:
        return -panel_slide;

    default:
        return side_panel == SidePanel::DETAILS ? 0 : -game_layout::PANEL_WIDTH;
    }
}

int GameContext::graveyard_panel_offset_x() const
{
    switch(panel_transition)
    {
    case PanelTransition::OPEN_GRAVEYARD:
        return game_layout::PANEL_WIDTH - panel_slide;

    case PanelTransition::CLOSE_GRAVEYARD:
        return panel_slide;

    default:
        return side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE
            ? 0
            : game_layout::PANEL_WIDTH;
    }
}

void GameContext::begin_panel_transition(PanelTransition transition)
{
    if(panel_transition != PanelTransition::NONE)
    {
        return;
    }

    if(transition == PanelTransition::OPEN_DETAILS && side_panel != SidePanel::NONE)
    {
        return;
    }

    if(transition == PanelTransition::OPEN_GRAVEYARD && side_panel != SidePanel::NONE)
    {
        return;
    }

    if(transition == PanelTransition::CLOSE_DETAILS && side_panel != SidePanel::DETAILS)
    {
        return;
    }

    if(transition == PanelTransition::CLOSE_GRAVEYARD &&
       side_panel != SidePanel::GRAVEYARD && side_panel != SidePanel::EXILE)
    {
        return;
    }

    if(transition == PanelTransition::OPEN_DETAILS || transition == PanelTransition::OPEN_GRAVEYARD)
    {
        clear_inspect();
    }

    if(transition == PanelTransition::OPEN_GRAVEYARD)
    {
        if(browse_open_target != SidePanel::EXILE && browse_open_target != SidePanel::GRAVEYARD)
        {
            browse_open_target = SidePanel::GRAVEYARD;
        }

        prepare_browse_for_panel(browse_open_target);
    }

    panel_transition = transition;
    panel_slide = 0;
}

void GameContext::complete_panel_transition()
{
    switch(panel_transition)
    {
    case PanelTransition::OPEN_DETAILS:
        side_panel = SidePanel::DETAILS;
        break;

    case PanelTransition::CLOSE_DETAILS:
        side_panel = SidePanel::NONE;
        panel_slide = 0;
        details_sprites.clear();
        details_last_library = -1;
        details_last_round = -1;
        details_last_goal = -1;
        details_last_mode = int(CampaignMode::NONE);
        last_details_sprite_offset = 0;
        break;

    case PanelTransition::OPEN_GRAVEYARD:
        side_panel = browse_open_target == SidePanel::EXILE ? SidePanel::EXILE : SidePanel::GRAVEYARD;
        break;

    case PanelTransition::CLOSE_GRAVEYARD:
        side_panel = SidePanel::NONE;
        panel_slide = 0;
        row_scroll_x = 0;
        target_row_scroll_x = 0;
        target_row_scroll_index = 0;
        break;

    default:
        break;
    }

    panel_transition = PanelTransition::NONE;
}

void GameContext::tick_panel()
{
    if(panel_transition == PanelTransition::NONE)
    {
        return;
    }

    panel_slide += game_layout::PANEL_SLIDE_SPEED;

    if(panel_slide >= game_layout::PANEL_WIDTH)
    {
        panel_slide = game_layout::PANEL_WIDTH;
        complete_panel_transition();
    }
}

void GameContext::apply_sprite_offset_delta(bn::span<bn::sprite_ptr> sprites, int target_offset, int& last_offset)
{
    const int delta = target_offset - last_offset;

    if(delta == 0)
    {
        return;
    }

    for(bn::sprite_ptr& sprite : sprites)
    {
        sprite.set_x(sprite.x() + delta);
    }

    last_offset = target_offset;
}

void GameContext::sync_details_panel(bool force)
{
    if(! show_details_layer())
    {
        return;
    }

    const int library = state.deck.remaining();
    const int round = state.current_round;
    const int mode_value = int(campaign_ui.mode);
    int goal_value = score_to_beat;

    switch(campaign_ui.mode)
    {
    case CampaignMode::BIGGEST_NUMBER:
        goal_value = campaign_ui.biggest_number_record;
        break;

    case CampaignMode::SAME_NUMBER:
        goal_value = campaign_ui.same_number_target;
        break;

    case CampaignMode::NUMBER_NOW:
        goal_value = campaign_ui.number_now_round_peak;
        break;

    default:
        break;
    }

    if(! force && library == details_last_library && round == details_last_round &&
       goal_value == details_last_goal && mode_value == details_last_mode)
    {
        return;
    }

    details_sprites.clear();
    last_details_sprite_offset = 0;

    details_text_generator.set_center_alignment();

    if(campaign_ui.mode == CampaignMode::NUMBER_NOW)
    {
        bn::string<32> score_line = "Score all of your";
        details_text_generator.generate(0, -56, score_line, details_sprites);

        bn::string<32> on_round_line = "points on round ";
        on_round_line.append(bn::to_string<4>(campaign_ui.number_now_scoring_round));
        details_text_generator.generate(0, -40, on_round_line, details_sprites);

        bn::string<24> current_line = "Current round: ";
        current_line.append(bn::to_string<4>(round));
        details_text_generator.generate(0, -24, current_line, details_sprites);

        bn::string<28> record_line = "Round ";
        record_line.append(bn::to_string<4>(campaign_ui.number_now_scoring_round));
        record_line.append(" best: ");
        record_line.append(bn::to_string<12>(campaign_ui.number_now_round_peak));
        details_text_generator.generate(0, -8, record_line, details_sprites);

        details_text_generator.generate(0, 16, "Next 3 rounds", details_sprites);
    }
    else
    {
        bn::string<24> library_line = "Library ";
        library_line.append(bn::to_string<8>(library));
        details_text_generator.generate(0, -56, library_line, details_sprites);

        bn::string<16> round_line = "Round ";
        round_line.append(bn::to_string<4>(round));
        details_text_generator.generate(0, -40, round_line, details_sprites);

        bn::string<32> goal_line;

        switch(campaign_ui.mode)
        {
        case CampaignMode::BIGGEST_NUMBER:
            goal_line = "Record: ";
            goal_line.append(bn::to_string<12>(campaign_ui.biggest_number_record));
            break;

        case CampaignMode::SAME_NUMBER:
            goal_line = "Target: ";
            goal_line.append(bn::to_string<8>(campaign_ui.same_number_target));
            break;

        default:
            goal_line = "Biggest Number: ";
            goal_line.append(bn::to_string<12>(score_to_beat));
            break;
        }

        details_text_generator.generate(0, -24, goal_line, details_sprites);
        details_text_generator.generate(0, -8, "Future Rounds", details_sprites);
    }

    details_text_generator.set_left_alignment();

    details_last_library = library;
    details_last_round = round;
    details_last_goal = goal_value;
    details_last_mode = mode_value;
}

void GameContext::hide_hand_display()
{
    for(Card& card : hand_display)
    {
        release_card_display_tiles(card);
    }
}

void GameContext::position_details_sprites()
{
    apply_sprite_offset_delta(bn::span<bn::sprite_ptr>(details_sprites.data(), details_sprites.size()),
                              details_panel_offset_x(), last_details_sprite_offset);
}

void GameContext::position_main_score_sprites()
{
    apply_sprite_offset_delta(bn::span<bn::sprite_ptr>(text_sprites.data(), text_sprites.size()),
                              main_panel_offset_x(), last_main_sprite_offset);
    apply_sprite_offset_delta(bn::span<bn::sprite_ptr>(round_text_sprites.data(), round_text_sprites.size()),
                              main_panel_offset_x(), last_round_sprite_offset);
}

void GameContext::position_inspect_sprites()
{
    const int target_offset = (side_panel == SidePanel::GRAVEYARD || side_panel == SidePanel::EXILE)
        ? graveyard_panel_offset_x()
        : main_panel_offset_x();
    apply_sprite_offset_delta(bn::span<bn::sprite_ptr>(inspect_sprites.data(), inspect_sprites.size()),
                              target_offset, last_inspect_sprite_offset);

    if(inspecting)
    {
        inspect_card.set_position(inspect_layout::CARD_X + target_offset, inspect_layout::CARD_Y);
    }
}

void GameContext::render_graveyard_exclude_marks(const CardRowResult& row, int panel_x, int card_y,
                                                CardType exclude_type, int cursor)
{
    for(GameMarker& marker : grave_exclude_markers)
    {
        marker.set_visible(false);
    }

    if(exclude_type == CardType::COUNT || row.visible_count == 0)
    {
        return;
    }

    const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;

    for(int slot = 0; slot < row.visible_count; ++slot)
    {
        const int index = first_visible + slot;

        if(index >= state.graveyard.size() || index == cursor)
        {
            continue;
        }

        if(state.graveyard[index].type != exclude_type)
        {
            continue;
        }

        const int card_x = row.row_start_x + slot * game_layout::GRAVE_SPACING - row.scroll_sub + panel_x + 16;
        const int card_raise = index == cursor && cursor >= 0 && cursor < card_raise_offset.size()
                                   ? card_raise_offset[cursor]
                                   : 0;
        grave_exclude_markers[slot].set_position(card_x, card_y - card_raise + 32);
        grave_exclude_markers[slot].set_visible(true);
    }
}

CardRowResult GameContext::render_graveyard_view(int panel_x, int cursor)
{
    const bn::span<const CardRef> cards = active_browse_cards();
    const int grave_count = cards.size();
    const bn::span<const int> grave_raises(
        card_raise_offset.data(),
        grave_count < card_raise_offset.size() ? grave_count : card_raise_offset.size());

    const CardRowResult row = render_card_row(
        grave_row_display,
        cards,
        cursor, game_layout::GRAVE_SPACING, game_layout::GRAVEYARD_BROWSE_Y,
        row_scroll_x, target_row_scroll_x, panel_x, grave_raises,
        &state.instance_pool, &hud_count_generator);

    apply_row_fade_bands(fade_bands, game_layout::GRAVEYARD_BROWSE_Y, row.has_left, row.has_right);

    constexpr bn::array<int, 4> fade_band_x = {-116, -108, 108, 116};

    for(int band_index = 0; band_index < fade_bands.size(); ++band_index)
    {
        fade_bands[band_index].set_position_x(fade_band_x[band_index] + panel_x);
    }

    if(row.cursor_slot >= 0)
    {
        graveyard_pick_placeholder.set_position(panel_x, game_layout::GRAVEYARD_BROWSE_PLACEHOLDER_Y);
        graveyard_pick_placeholder.set_visible(true);
    }

    return row;
}

void GameContext::render_graveyard_browse(int panel_x)
{
    hide_hand_display();
    render_graveyard_view(panel_x, browse_cursor);
    hide_combo_focus_row_cards();
}

void GameContext::update_target_scroll()
{
    const int hand_count = hand_layout_center_count();
    const int scroll_count = visual_hand_slot_count();
    const int max_start = scroll_count > game_layout::VISIBLE_CARD_COUNT
                              ? scroll_count - game_layout::VISIBLE_CARD_COUNT
                              : 0;
    const int centered_card_count = hand_count < game_layout::VISIBLE_CARD_COUNT ? hand_count
                                                                                 : game_layout::VISIBLE_CARD_COUNT;
    target_hand_x = -(centered_card_count * game_layout::HAND_SPACING) / 2;
    const int first_visible_card = selected_card < 3 ? 0 : selected_card - 3 < max_start ? selected_card - 3
                                                                                         : max_start;
    target_scroll_x = first_visible_card * game_layout::HAND_SPACING;

    const int max_scroll_x = max_start * game_layout::HAND_SPACING;
    target_edge_shift = max_scroll_x == 0 ? 0 : target_scroll_x == 0          ? game_layout::HAND_SPACING / 2 + 4
                                            : target_scroll_x == max_scroll_x ? -game_layout::HAND_SPACING / 2 - 4
                                                                              : 0;
};

void GameContext::sync_target_row_scroll(int cursor, int count)
{
    target_row_scroll_index = first_visible_index(cursor, count, game_layout::VISIBLE_CARD_COUNT);
};

void GameContext::snap_row_scroll(int spacing)
{
    row_scroll_x = target_row_scroll_index * spacing;
    target_row_scroll_x = row_scroll_x;
};

void GameContext::sync_row_scroll_for_mode(int cursor, int count, int spacing)
{
    sync_target_row_scroll(cursor, count);
    snap_row_scroll(spacing);
};

void GameContext::process_instant_pending()
{
    while(!state.pending_actions.empty())
    {
        if(state.pending_actions.front().type == PendingActionType::MIRACLE_AUTO_PLAY)
        {
            const PendingAction action = state.pending_actions.front();
            state.pending_actions.erase(state.pending_actions.begin());

            if(action.hand_index >= 0 && action.hand_index < state.hand.size())
            {
                play_miracle_bonus(state, 10);
                hand_remove_at_to_graveyard(state, action.hand_index, selected_card);
                draw_round_score();
            }

            continue;
        }

        if(try_start_pending_combo())
        {
            return;
        }

        break;
    }
};

// Advance to the next queued interactive action, or, if none remain, finish
// the turn: back to NORMAL and, if the hand is now empty, close out the round.
// Implementation: pending_actions.cpp (one starter per PendingActionType).

namespace
{
    GameMode mode_for_selection(PendingActionType type)
    {
        switch(type)
        {
        case PendingActionType::GRAVEYARD_PICK_TO_BOTTOM:
        case PendingActionType::GRAVEYARD_PICK_TO_TOP:
            return GameMode::GRAVEYARD_PICK;
        case PendingActionType::DISCARD_FROM_HAND:
        case PendingActionType::EXILE_FROM_HAND:
        case PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY:
        case PendingActionType::PUT_HAND_ON_DECK_TOP:
            return GameMode::DISCARD_TARGET;
        case PendingActionType::NONE:
            return GameMode::NORMAL;
        default:
            return GameMode::GRAVEYARD_TARGET;
        }
    }

    bool combo_selection_resumes_after_interrupt(PendingActionType type)
    {
        switch(type)
        {
        case PendingActionType::NONE:
            return false;
        case PendingActionType::EXILE_FROM_GRAVEYARD:
        case PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT:
        case PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY:
        case PendingActionType::RETRIEVE_FROM_GRAVEYARD:
        case PendingActionType::RETRIEVE_FROM_GRAVEYARD_TO_TOP:
        case PendingActionType::GRAVEYARD_PICK_TO_BOTTOM:
        case PendingActionType::GRAVEYARD_PICK_TO_TOP:
        case PendingActionType::GRAVEYARD_PAIR_SWAP:
            return true;
        default:
            return false;
        }
    }

    void combo_fizzle_revealed_selection(GameState& state)
    {
        if(state.selection.type == PendingActionType::SCRY)
        {
            for(int index = state.selection.scry_buffer.size() - 1; index >= 0; --index)
            {
                state.deck.insert_top(state.selection.scry_buffer[index]);
            }
        }

        state.selection.scry_buffer.clear();
        state.selection.deck_search_buffer.clear();
        state.selection = SelectionSession{};
    }
}

void GameContext::enter_combo_mode()
{
    // Chained combos re-enter from COMBO itself; keep the mode the first one interrupted.
    if(mode != GameMode::COMBO)
    {
        combo_interrupted_mode = mode;
    }

    mode = GameMode::COMBO;
    begin_combo_focus();
}

void GameContext::begin_combo_focus()
{
    combo_focus = ComboFocusPhase::NONE;
    combo_focus_panel_opened = false;
    combo_focus_frame = 0;
    combo_focus_anchor_x = 0;

    if(state.pending_combo.zone != ComboZone::GRAVEYARD || state.pending_combo.length <= 0)
    {
        return;
    }

    // Modes that already show a card row in the main view telegraph the match on
    // their own, and a live selection owns row_scroll_x, which the pan would clobber.
    if(combo_interrupted_mode != GameMode::NORMAL || inspecting ||
       state.selection.type != PendingActionType::NONE)
    {
        return;
    }

    if(side_panel != SidePanel::NONE || panel_transition_active())
    {
        return;
    }

    browse_open_target = SidePanel::GRAVEYARD;
    begin_panel_transition(PanelTransition::OPEN_GRAVEYARD);

    if(panel_transition != PanelTransition::OPEN_GRAVEYARD)
    {
        return;
    }

    // begin_panel_transition parks the cursor on the newest card; scroll to the match instead.
    const int match_center = state.pending_combo.start_index + state.pending_combo.length / 2;
    browse_cursor = clamp_graveyard_cursor(match_center, state.graveyard.size());
    sync_row_scroll_for_mode(browse_cursor, state.graveyard.size(), game_layout::GRAVE_SPACING);

    combo_focus = ComboFocusPhase::PAN_IN;
    combo_focus_panel_opened = true;
}

void GameContext::begin_combo_focus_return()
{
    if(!combo_focus_panel_opened || panel_transition_active())
    {
        return;
    }

    if(side_panel != SidePanel::GRAVEYARD && side_panel != SidePanel::EXILE)
    {
        return;
    }

    begin_panel_transition(PanelTransition::CLOSE_GRAVEYARD);
}

bool GameContext::combo_focus_active() const
{
    return combo_focus != ComboFocusPhase::NONE;
}

int GameContext::combo_focus_graveyard_index(int card_index) const
{
    return state.pending_combo.use_match_indices ? state.pending_combo.match_indices[card_index]
                                                 : state.pending_combo.start_index + card_index;
}

bool GameContext::combo_focus_highlights_graveyard_card(int graveyard_index) const
{
    if(mode != GameMode::COMBO || !combo_focus_active())
    {
        return false;
    }

    if(state.pending_combo.zone != ComboZone::GRAVEYARD)
    {
        return false;
    }

    for(int index = 0; index < state.pending_combo.length; ++index)
    {
        if(combo_focus_graveyard_index(index) == graveyard_index)
        {
            return true;
        }
    }

    return false;
}

void GameContext::combo_focus_slot_position(int card_index, int panel_x, int& out_x, int& out_y) const
{
    // Mirrors render_card_row so the cinematic cards start exactly on their row slots.
    const int count = state.graveyard.size();
    const int window = game_layout::VISIBLE_CARD_COUNT;
    const int visible_count = count < window ? count : window;
    const int row_start_x = -(visible_count * game_layout::GRAVE_SPACING) / 2;
    const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;
    const int scroll_sub = row_scroll_x - first_visible * game_layout::GRAVE_SPACING;
    const int slot = combo_focus_graveyard_index(card_index) - first_visible;

    out_x = row_start_x + slot * game_layout::GRAVE_SPACING - scroll_sub + panel_x;
    out_y = game_layout::GRAVEYARD_BROWSE_Y - game_layout::GRAVEYARD_BROWSE_SELECTED_RAISE;
}

void GameContext::hide_combo_focus_row_cards()
{
    if(mode != GameMode::COMBO || combo_focus != ComboFocusPhase::PLAYING)
    {
        return;
    }

    if(state.pending_combo.zone != ComboZone::GRAVEYARD)
    {
        return;
    }

    const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;

    for(int index = 0; index < state.pending_combo.length; ++index)
    {
        const int slot = combo_focus_graveyard_index(index) - first_visible;

        if(slot >= 0 && slot < grave_row_display.size())
        {
            release_card_display_tiles(grave_row_display[slot]);
        }
    }
}

bool GameContext::try_start_pending_combo()
{
    if(play_flight_count() > 0)
    {
        return false;
    }

    if(state.combo_cinematic.active)
    {
        mode = GameMode::COMBO;
        return true;
    }

    if(!combo_try_start_pending(state))
    {
        return false;
    }

    enter_combo_mode();
    return true;
}

bool GameContext::block_round_end_for_combo()
{
    if(state.combo_cinematic.active)
    {
        mode = GameMode::COMBO;
        round_end_pending = false;
        update_target_scroll();
        return true;
    }

    // Last chance before round/run end: re-scan so a just-completed GY combo
    // (e.g. PB onto Jelly) cannot be skipped by deferred empty-hand finish.
    if(state.pending_combo.length <= 0)
    {
        combo_check_zone(state, ComboZone::HAND);
        combo_check_zone(state, ComboZone::GRAVEYARD);
        combo_check_zone(state, ComboZone::DECK);
    }

    if(try_start_pending_combo())
    {
        round_end_pending = false;
        update_target_scroll();
        return true;
    }

    return state.pending_combo.length > 0;
}

void GameContext::finish_combo_cinematic()
{
    combo_resume_type = state.selection.type;

    // Clear active before removing cards so GRAVEYARD_CHANGED can queue the next match.
    // (combo_check_zone no-ops while a cinematic is marked active.)
    state.combo_cinematic.active = false;
    combo_remove_resolved_cards(state, selected_card);
    browse_cursor = clamp_graveyard_cursor(browse_cursor, state.graveyard.size());
    state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());

    // Hand removes do not dispatch HAND_CHANGED; cover that and any missed GY scan.
    if(state.pending_combo.length <= 0)
    {
        combo_check_zone(state, ComboZone::HAND);
        combo_check_zone(state, ComboZone::GRAVEYARD);
        combo_check_zone(state, ComboZone::DECK);

        if(combo_resume_type == PendingActionType::SCRY ||
           combo_resume_type == PendingActionType::DECK_SEARCH)
        {
            combo_check_zone(state, ComboZone::REVEALED);
        }
    }

    // Hold the resume until the camera is back on the main view, otherwise the player
    // could act on a hand they cannot see yet.
    if(combo_focus_panel_opened)
    {
        begin_combo_focus_return();

        if(panel_transition_active() || side_panel != SidePanel::NONE)
        {
            combo_focus = ComboFocusPhase::PAN_OUT;
            draw_round_score();
            return;
        }
    }

    combo_focus = ComboFocusPhase::NONE;
    combo_focus_panel_opened = false;
    resume_after_combo();
}

void GameContext::resume_after_combo()
{
    const PendingActionType resume_type = combo_resume_type;
    combo_resume_type = PendingActionType::NONE;

    if(combo_try_start_pending(state))
    {
        enter_combo_mode();
        draw_round_score();
        return;
    }

    if(combo_selection_resumes_after_interrupt(resume_type))
    {
        sync_target_row_scroll(state.selection.cursor, state.graveyard.size());
        mode = mode_for_selection(resume_type);
        draw_round_score();
        return;
    }

    if(resume_type == PendingActionType::SCRY || resume_type == PendingActionType::DECK_SEARCH)
    {
        combo_fizzle_revealed_selection(state);
    }
    else
    {
        state.selection = SelectionSession{};
    }

    begin_next_pending_or_finish();
}

void GameContext::shutdown_for_exit()
{
    score_swap_fx = ScoreSwapFxState{};
    score_swap_marker_sprites.clear();
    reset_card_animation_state();
    hide_hand_display();

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

    release_card_display_tiles(exclusive_fx_card());
    release_card_display_tiles(inspect_card);
    release_card_display_tiles(echo_ghost_card);
    clear_inspect();
    text_sprites.clear();
    round_text_sprites.clear();
    details_sprites.clear();
    hud.set_visible(false);
}

void GameContext::release_idle_card_pools()
{
    if(mode != GameMode::SCRY)
    {
        for(Card& card : scry_display)
        {
            release_card_display_tiles(card);
        }
    }

    if(mode != GameMode::COMBO)
    {
        for(Card& card : combo_display)
        {
            release_card_display_tiles(card);
        }
    }

    for(Card& card : swivel_display)
    {
        release_card_display_tiles(card);
    }

    if(side_panel != SidePanel::GRAVEYARD && side_panel != SidePanel::EXILE &&
       mode != GameMode::GRAVEYARD_TARGET &&
       mode != GameMode::GRAVEYARD_PICK && mode != GameMode::DECK_SEARCH)
    {
        for(Card& card : grave_row_display)
        {
            release_card_display_tiles(card);
        }
    }

    release_card_display_tiles(exclusive_fx_card());
}

void GameContext::finish_empty_hand_round()
{
    if(!empty_hand_triggers_round_end(state))
    {
        round_end_pending = false;
        clamp_hand_cursor();
        return;
    }

    // Empty hand/deck is only meaningful after the played card has completely
    // resolved. A deferred round-end can otherwise race an action started on the
    // final removal frame (Necromancy's GY shuffle is the clearest example).
    if(card_resolution_blocking_round_end())
    {
        round_end_pending = true;
        return;
    }

    // Combos resolve after the card's full effect but before round/run end.
    if(block_round_end_for_combo())
    {
        return;
    }

    round_end_pending = false;
    swivel_clear_wait_if_hand_empty(*this);

    release_idle_card_pools();
    if(state.turtle_rounds_remaining > 0)
    {
        --state.turtle_rounds_remaining;

        if(state.turtle_rounds_remaining == 0)
        {
            commit_round_with_checks();
            draw_total_score();
        }

        begin_keep_going_round_transfers(state.turtle_rounds_remaining > 0);

        if(deferred_round_start_pending)
        {
            return;
        }

        if(deck.empty())
        {
            if(state.turtle_rounds_remaining > 0)
            {
                state.round.reset();
            }

            game_over = true;
            run_finished = true;
            scene_result.final_score = state.total_score;
        }
    }
    else
    {
        commit_round_with_checks();
        draw_total_score();

        if(campaign_ui.mode == CampaignMode::NUMBER_NOW &&
           state.current_round == campaign_ui.number_now_scoring_round)
        {
            game_over = true;
            run_finished = true;
            scene_result.final_score = state.total_score;
            return;
        }

        begin_keep_going_round_transfers(false);

        if(deferred_round_start_pending)
        {
            return;
        }

        if(deck.empty())
        {
            game_over = true;
            run_finished = true;
            scene_result.final_score = state.total_score;
        }
    }
}

void GameContext::tick_round_end_pending()
{
    if(!round_end_pending)
    {
        return;
    }

    if(!empty_hand_triggers_round_end(state))
    {
        round_end_pending = false;
        return;
    }

    if(card_resolution_blocking_round_end())
    {
        return;
    }

    round_end_pending = false;
    finish_empty_hand_round();
    update_target_scroll();
}
