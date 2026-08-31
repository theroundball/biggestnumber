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
#include "bn_sprite_items_hud_deck.h"

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
#include "score_pop_system.h"
#include "score_swap_system.h"
#include "swivel_system.h"
#include "trinket_system.h"
#include "ui_inspect.h"

namespace
{
    template<int MaxOut>
    bn::string<MaxOut> format_score_window(const bn::string_view& full, int max_chars, int view_offset,
                                           bool allow_left_ellipsis)
    {
        bn::string<MaxOut> display;

        if(full.size() <= max_chars)
        {
            display = full;
            return display;
        }

        if(!allow_left_ellipsis)
        {
            const int window = max_chars - 3;

            for(int index = 0; index < window && index < full.size(); ++index)
            {
                display.push_back(full[index]);
            }

            display.append("...");
            return display;
        }

        int window = max_chars;
        int offset = view_offset;

        if(offset > 0)
        {
            window -= 3;
        }

        if(offset + window > full.size())
        {
            offset = int(full.size()) - window;

            if(offset < 0)
            {
                offset = 0;
            }
        }

        if(offset > 0)
        {
            display.append("...");
        }

        for(int index = offset; index < offset + window && index < full.size(); ++index)
        {
            display.push_back(full[index]);
        }

        if(offset + window < full.size())
        {
            display.append("...");
        }

        return display;
    }
}

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

    constexpr bn::color SCORE_VICTORY_GREEN(6, 28, 10);

    void apply_victory_green_tint(bn::span<bn::sprite_ptr> sprites)
    {
        if(sprites.empty())
        {
            return;
        }

        bn::span<const bn::color> source = sprites[0].palette().colors();
        bn::array<bn::color, 16> green_colors;

        for(int index = 0; index < 16; ++index)
        {
            green_colors[index] = index < source.size() ? source[index] : bn::color();
        }

        for(int index = 1; index < 16; ++index)
        {
            const bn::color& color = green_colors[index];

            if(color.red() + color.green() + color.blue() > 24)
            {
                green_colors[index] = SCORE_VICTORY_GREEN;
            }
        }

        const bn::sprite_palette_item item(
            bn::span<const bn::color>(green_colors.data(), green_colors.size()), bn::bpp_mode::BPP_4,
            bn::compression_type::NONE);
        const bn::sprite_palette_ptr green_palette = bn::sprite_palette_ptr::create(item);

        for(bn::sprite_ptr& sprite : sprites)
        {
            sprite.set_palette(green_palette);
        }
    }

    bool round_would_beat_goal(const GameContext& ctx, int running, int end_multiplier)
    {
        if(ctx.campaign_ui.mode == CampaignMode::NUMBER_NOW &&
           ctx.state.current_round != ctx.campaign_ui.number_now_scoring_round)
        {
            return false;
        }

        RoundScore display_score;
        display_score.running = running;
        display_score.end_multiplier = end_multiplier;
        return ctx.state.total_score + display_score.committed() > ctx.score_to_beat;
    }

    void apply_bones_gy_entry(GameState& state)
    {
        int bones_count = 0;

        for(const CardRef& card : state.graveyard)
        {
            if(card.type == CardType::BONES)
            {
                ++bones_count;
            }
        }

        const int graveyard_count = state.graveyard.size();

        if(graveyard_count > 0)
        {
            state.add_from_card(graveyard_count);
        }

        const int factor = bones_count + 1;

        if(factor > 1)
        {
            state.mul_from_card(factor);
        }
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
            if(context.hand_index >= 0 && context.hand_index < state.hand.size() && context.selected_card)
            {
                finish_played_card_from_hand(context.hand_index, *context.selected_card, state);
            }
        }
        else
        {
            const PostPlayDestination dest = route_played_card(state, card.type, context.source, swivel_follow);
            apply_post_play_destination(state, card, context.source, dest, context.hand_index,
                                        context.selected_card);

            if(dest == PostPlayDestination::GRAVEYARD)
            {
                if(card.type == CardType::BONES)
                {
                    apply_bones_gy_entry(state);
                }
                else if(card.type == CardType::TOMBSTONES)
                {
                    apply_tombstones_gy_entry(state);
                }
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
    library_marker(bn::sprite_items::hud_deck.create_sprite(0, 0)),
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
    library_marker.set_visible(false);
    library_marker.set_z_order(game_layout::CARD_FACE_TEXT_Z);
    library_marker.set_bg_priority(0);
    state.trinkets = launch.trinkets;
    state.longsleeve_cards.clear();

    for(const CardRef& longsleeve_card : launch.longsleeve_cards)
    {
        if(longsleeve_card.type != CardType::COUNT)
        {
            state.longsleeve_cards.push_back(longsleeve_card);
        }
    }

    state.echo_ready = state.has_trinket(TrinketType::ECHO);
    battle_stats_reset(state);
    state.instance_pool = launch.instance_pool;
    deck.ensure_unique_bounty_instances(state.instance_pool, state.bounty_next_id);
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

    combo_init_progress_availability(state);

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

    deal_opening_hand();
    state.apply_round_start_adds();
    state.apply_round_start_multiply();
    reset_card_animation_state();
    try_start_hand_draw_fx();
    draw_total_score();
    draw_round_score();
    hud.update(state, deck_hud_display_count(state, in_flight_deck_draw_count()));
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
    const bool digit_edit = score_swap_is_active(*this);
    const bn::string<12> full_text = bn::to_string<12>(value);
    const bn::string<16> display_text = format_score_window<16>(
        full_text, game_layout::TOTAL_SCORE_VISIBLE_CHARS, total_score_view_offset, digit_edit);

    if(_total_score_initialized && value == _cached_total_score && !text_sprites.empty() &&
       (!digit_edit || total_score_view_offset == _cached_total_score_view_offset))
    {
        return;
    }

    _cached_total_score = value;
    _cached_total_score_view_offset = total_score_view_offset;
    _total_score_initialized = true;
    text_sprites.clear();
    last_main_sprite_offset = 0;
    text_generator.set_center_alignment();
    text_generator.generate(0, -48, display_text, text_sprites);
    text_generator.set_left_alignment();

    // Green when this run/fight has beaten the score-to-beat baseline.
    if(value > score_to_beat && !text_sprites.empty())
    {
        apply_victory_green_tint(bn::span<bn::sprite_ptr>(text_sprites.data(), text_sprites.size()));
    }

    sync_score_progress_bar();
}

void GameContext::sync_score_digit_view(SwapScoreField field, int digit_index)
{
    if(!score_swap_is_active(*this))
    {
        return;
    }

    int& view_offset = field == SwapScoreField::TOTAL ? total_score_view_offset : round_score_view_offset;
    const int max_chars = field == SwapScoreField::TOTAL ? game_layout::TOTAL_SCORE_VISIBLE_CHARS
                                                           : game_layout::ROUND_SCORE_VISIBLE_CHARS;
    const int window = max_chars - 3;

    if(digit_index < view_offset)
    {
        view_offset = digit_index;
    }
    else if(digit_index >= view_offset + window)
    {
        view_offset = digit_index - window + 1;

        if(view_offset < 0)
        {
            view_offset = 0;
        }
    }

    if(field == SwapScoreField::TOTAL)
    {
        _total_score_initialized = false;
        show_total_score_value(state.total_score);
    }
    else
    {
        _round_score_initialized = false;
        show_round_score_running(state.round.running, state.round.end_multiplier);
    }
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

void GameContext::sync_combo_progress_bars()
{
    bn::array<ComboBarState, game_layout::COMBO_BAR_ROW_COUNT> states{};
    combo_collect_bar_states(state, states);

    bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT> lengths{};
    bn::array<uint8_t, game_layout::COMBO_BAR_ROW_COUNT> filled{};

    for(int row_index = 0; row_index < game_layout::COMBO_BAR_ROW_COUNT; ++row_index)
    {
        lengths[row_index] = states[row_index].length;
        filled[row_index] = states[row_index].filled;
    }

    combo_progress_bars.sync(lengths, filled);
}

bool GameContext::should_end_run() const
{
    if(round_end_pending)
    {
        return false;
    }

    return run_should_end(state, in_flight_deck_draw_count());
}

void GameContext::end_run_if_needed()
{
    if(!should_end_run())
    {
        return;
    }

    request_run_end();
}

bool GameContext::score_presentation_blocking() const
{
    if(score_count_is_active(*this, TrinketScoreField::ROUND) ||
       score_count_is_active(*this, TrinketScoreField::TOTAL))
    {
        return true;
    }

    if(!state.pending_score_counts.empty())
    {
        return true;
    }

    if(!score_pops.empty() || !state.pending_score_pops.empty())
    {
        return true;
    }

    if(round_score_wiggle_frames > 0 || total_score_wiggle_frames > 0)
    {
        return true;
    }

    if(score_pop_blocks_score_finalize(*this, TrinketScoreField::ROUND) ||
       score_pop_blocks_score_finalize(*this, TrinketScoreField::TOTAL))
    {
        return true;
    }

    if(!trinket_score_reaction_idle(*this))
    {
        return true;
    }

    if(score_swap_is_active(*this))
    {
        return true;
    }

    if(state.deferred_morel_count > 0)
    {
        return true;
    }

    return false;
}

void GameContext::request_run_end()
{
    game_over = true;
    scene_result.final_score = state.total_score;
    score_count_process_pending(*this);
    run_end_presentation_pending = true;
    tick_run_end_presentation();
}

void GameContext::tick_run_end_presentation()
{
    if(!run_end_presentation_pending)
    {
        return;
    }

    if(score_presentation_blocking())
    {
        return;
    }

    run_end_presentation_pending = false;
    run_finished = true;
}

void GameContext::finish_finale_run()
{
    state.finale_active = false;
    state.round.reset();
    request_run_end();
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

    if(state.build_a_number_active)
    {
        show_round_score_running(state.round.running, state.round.end_multiplier);
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
    if(state.build_a_number_active)
    {
        bn::string<16> builder_text;

        for(int index = 0; index < 3; ++index)
        {
            if(index > 0)
            {
                builder_text.append(' ');
            }

            if(state.build_digits[index] >= 0)
            {
                builder_text.append(bn::to_string<1>(state.build_digits[index]));
            }
            else if(mode == GameMode::BUILD_NUMBER_DIGIT && state.selection.cursor == index &&
                    state.selection.multiply_factor >= 1 && state.selection.multiply_factor <= 9)
            {
                builder_text.append('[');
                builder_text.append(bn::to_string<1>(state.selection.multiply_factor));
                builder_text.append(']');
            }
            else
            {
                builder_text.append('_');
            }
        }

        if(_round_score_initialized && builder_text == _cached_round_score_text && !round_text_sprites.empty())
        {
            sync_score_progress_bar();
            return;
        }

        _cached_round_score_text = builder_text;
        _round_score_initialized = true;
        round_text_sprites.clear();
        last_round_sprite_offset = 0;
        round_text_generator.set_center_alignment();
        round_text_generator.generate(0, 0, builder_text, round_text_sprites);
        round_text_generator.set_left_alignment();
        sync_score_progress_bar();
        return;
    }

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
    const bool digit_edit = score_swap_is_active(*this);
    const bn::string<48> display_text = format_score_window<48>(
        new_text, game_layout::ROUND_SCORE_VISIBLE_CHARS, round_score_view_offset, digit_edit);

    if(_round_score_initialized && display_text == _cached_round_score_text && !round_text_sprites.empty())
    {
        return;
    }

    _cached_round_score_text = display_text;
    _round_score_initialized = true;
    round_text_sprites.clear();
    last_round_sprite_offset = 0;
    round_text_generator.set_center_alignment();
    round_text_generator.generate(0, 0, display_text, round_text_sprites);
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

    if(round_would_beat_goal(*this, running, end_multiplier))
    {
        apply_victory_green_tint(
            bn::span<bn::sprite_ptr>(round_text_sprites.data(), round_text_sprites.size()));
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
    const int round_number = state.current_round;

    if(state.build_a_number_active)
    {
        const int round_score = state.build_a_number_commit_prebuild();

        if(campaign_ui.mode == CampaignMode::NUMBER_NOW && round_number != campaign_ui.number_now_scoring_round)
        {
            scene_result.last_round_score = 0;
            scene_result.last_round_number = round_number;
        }
        else
        {
            scene_result.last_round_score = round_score;
            scene_result.last_round_number = round_number;
        }
    }
    else if(campaign_ui.mode == CampaignMode::NUMBER_NOW &&
            round_number != campaign_ui.number_now_scoring_round)
    {
        scene_result.last_round_score = 0;
        scene_result.last_round_number = round_number;
        state.flush_staircase_climb();
        state.round.reset();
    }
    else
    {
        const int round_score = state.round.committed();
        scene_result.last_round_score = round_score;
        scene_result.last_round_number = round_number;
        state.commit_round();
    }

    score_count_cancel(*this, TrinketScoreField::ROUND);
    _cached_round_score_text = "";
    _round_score_initialized = false;
    round_score_wiggle_frames = 0;
    show_round_score_running(state.round.running, state.round.end_multiplier);
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

void GameContext::sync_hand_selection()
{
    if(mode != GameMode::NORMAL && mode != GameMode::DISCARD_TARGET)
    {
        return;
    }

    if(state.hand.empty() && mode == GameMode::DISCARD_TARGET)
    {
        return;
    }

    if(mode == GameMode::NORMAL)
    {
        clamp_hand_cursor();
        retarget_selection_off_hidden_slot();
    }
    else
    {
        clamp_hand_cursor();
    }

    const int slot_count = visual_hand_slot_count();

    for(int index = 0; index < slot_count && index < card_raise_offset.size(); ++index)
    {
        card_raise_offset[index] = 0;
    }

    snap_selected_card_raise();
    update_target_scroll();
    snap_active_scroll();
}

void GameContext::prepare_hand_selection_mode()
{
    clamp_hand_cursor();
    row_scroll_x = 0;
    target_row_scroll_x = 0;
    target_row_scroll_index = 0;
    sync_hand_selection();
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
    rags_exile_deferred_finish = false;
    for(TransitFlight& flight : transit_flights)
    {
        clear_transit_flight(flight);
    }
    pending_graveyard_exile_fx.clear();
    deck_search_resolve_fx.active = false;
    deck_search_resolve_fx.frame = 0;
    for(int& offset : card_raise_offset)
    {
        offset = 0;
    }
    echo_play_badge_active = false;
    swivel_follow_pending = false;

    if(!state.echo_pending_replay)
    {
        echo_ghost_active = false;
        echo_ghost_card.set_visible(false);
        state.echo_replay_card = CardRef{};
    }

    sync_hand_selection();
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
    if(score_swap_is_active(*this) || selection_blocks_pending_finish())
    {
        return false;
    }

    if(mode != GameMode::NORMAL)
    {
        return false;
    }

    if(play_flight_count() >= game_layout::MAX_PLAY_FLIGHTS)
    {
        return false;
    }

    if(graveyard_card_fx_active || deck_search_resolve_active())
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
    }

    return true;
}

int GameContext::play_flight_fan_offset_x(const PlayFlight& flight) const
{
    if(!flight.center_beat)
    {
        return 0;
    }

    int active_index = -1;
    int active_count = 0;

    for(const PlayFlight& candidate : play_flights)
    {
        if(!candidate.active || !candidate.center_beat)
        {
            continue;
        }

        if(&candidate == &flight)
        {
            active_index = active_count;
        }

        ++active_count;
    }

    if(active_count <= 1 || active_index < 0)
    {
        return 0;
    }

    return (active_index - (active_count - 1) / 2) * game_layout::PLAY_FAN_SPACING;
}

int GameContext::play_hold_frames(const PlayFlight& flight) const
{
    if(!flight.center_beat)
    {
        return game_layout::PLAY_HOLD_FRAMES;
    }

    if(play_flight_count() > 1)
    {
        return game_layout::PLAY_CHAIN_HOLD_FRAMES;
    }

    return game_layout::PLAY_HOLD_FRAMES;
}

bool GameContext::graveyard_slot_hidden_by_flight(int graveyard_index) const
{
    if(graveyard_index < 0 || graveyard_index >= state.graveyard.size())
    {
        return false;
    }

    const CardRef graveyard_card = state.graveyard[graveyard_index];

    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || !flight.hand_committed || !flight.play_resolved)
        {
            continue;
        }

        if(flight.style == RemovalStyle::TO_DECK_TOP || flight.style == RemovalStyle::EXILE_DISSIPATE)
        {
            continue;
        }

        if(flight.phase != PlayRemovalPhase::APPROACH &&
           flight.phase != PlayRemovalPhase::HOLD &&
           flight.phase != PlayRemovalPhase::DEPART)
        {
            continue;
        }

        const CardRef& played = flight.played_ref;

        if(played.type == graveyard_card.type && played.instance_id == graveyard_card.instance_id &&
           played.bounty_id == graveyard_card.bounty_id)
        {
            return true;
        }
    }

    return false;
}

bool GameContext::play_hides_hand_index(int hand_index) const
{
    for(const PlayFlight& flight : play_flights)
    {
        if(!flight.active || flight.hand_committed || flight.play_resolved || !flight.center_beat)
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
    if(!playable_slot_is_ghost(state, visual_index))
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

    if(is_discard && selected_card >= 0 && selected_card < state.hand.size())
    {
        flight->played_ref = state.hand[selected_card];
        flight->origin = PlayPresentOrigin::HAND;
        flight->play_context = PlayResolutionContext{};
        flight->play_context.source = PlaySource::HAND;
        flight->play_context.hand_index = selected_card;
        flight->play_context.selected_card = &selected_card;
        flight->scoring_source = PlaySource::HAND;

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

        const int removed_index = selected_card;
        discard_card(state, selected_card);
        shift_card_raise_after_remove(removed_index);
        flight->hand_committed = true;
    }

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
    bind_bounty_copy(state, card);

    if(origin == PlayPresentOrigin::HAND && context.hand_index >= 0 &&
       context.hand_index < state.hand.size())
    {
        state.hand[context.hand_index].bounty_id = card.bounty_id;
    }
    else if(origin == PlayPresentOrigin::GRAVEYARD && context.hand_index >= 0 &&
            context.hand_index < state.graveyard.size())
    {
        state.graveyard[context.hand_index].bounty_id = card.bounty_id;
    }

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

        if(state.echo_first_play_active() && card_has_play_effect(state, card) &&
           !state.build_a_number_active)
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
    else if(origin == PlayPresentOrigin::HAND && flight->center_beat &&
            card.type != CardType::SWIVEL &&
            state.selection.type != PendingActionType::PUT_HAND_ON_DECK_TOP)
    {
        resolve_play_flight_effects(*flight);
        commit_play_flight_destination(*flight);
        begin_next_pending_or_finish();
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
    const int fan_offset = play_flight_fan_offset_x(flight);
    const int center_x = card_target_x_for_score_center(main_x) + fan_offset;
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

    int count = layout_hand_count();

    if(hand_draw_fx_blocking())
    {
        const int scheduled = hand_scheduled_count(state, in_flight_deck_draw_count());

        if(scheduled > count)
        {
            count = scheduled;
        }
    }

    return count;
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
        bool discarded = flight.hand_committed;

        if(!discarded && discard_index >= 0 && discard_index < state.hand.size())
        {
            flight.played_ref = state.hand[discard_index];
            selected_card = discard_index;
            discard_card(state, selected_card);
            shift_card_raise_after_remove(discard_index);
            discarded = true;
            flight.hand_committed = true;
        }

        draw_round_score();

        if(state.selection.type == PendingActionType::DISCARD_FROM_HAND_THEN_MULTIPLY)
        {
            if(discarded && state.selection.multiply_factor > 0)
            {
                state.mul_from_card(state.selection.multiply_factor);
                state.selection.multiply_factor = 0;
                draw_round_score();
            }
        }
        else if(state.selection.type == PendingActionType::OVERCLOCK_DISCARD_PROMPT)
        {
            if(discarded)
            {
                state.mul_from_card(state.selection.multiply_factor);
                draw_round_score();

                if(!state.hand.empty())
                {
                    PendingAction next;
                    next.type = PendingActionType::OVERCLOCK_DISCARD_PROMPT;
                    next.count = state.selection.multiply_factor + 1;
                    state.pending_actions.push_back(next);
                }
            }
        }

        flight.play_resolved = true;
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
        apply_card_play(state, flight.played_ref, flight.scoring_source);
        build_a_number_try_queue_digit_placement(state, flight.played_ref, flight.scoring_source);
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

        if(flight.play_context.source != PlaySource::ECHO && !state.build_a_number_active)
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
       flight.play_context.source != PlaySource::GHOST)
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

    if(discard_resolved && !flight.hand_committed)
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
        continue_opening_hand_deal();
    }

    const int cycle_draws = pending_cycle_draws + (cycle_draw ? 1 : 0);
    pending_cycle_draws = 0;

    clear_play_flight(flight);
    sync_removing_card();

    if(play_flight_count() > 0)
    {
        pending_cycle_draws = cycle_draws;
        draw_total_score();
        sync_hand_selection();
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

    if(hand_draw_fx_blocking())
    {
        return;
    }

    if(swivel_follow)
    {
        swivel_complete_follow(*this);
    }
    else if(discard_resolved || !selection_blocks_pending_finish())
    {
        // Do not drain queued follow-ups (e.g. Jacks retrieve) while an earlier
        // interactive step (discard target) is still open.
        begin_next_pending_or_finish(discard_resolved);
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

        if(flight.hand_committed && !flight.is_discard)
        {
            complete_play_flight(flight);
            return;
        }

        if(flight.is_discard)
        {
            if(!flight.play_resolved)
            {
                resolve_play_flight_effects(flight);
            }

            complete_play_flight(flight);
            return;
        }

        if(flight.hand_committed)
        {
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
                flight.hand_index >= 0 && flight.hand_index < state.hand.size())
        {
            const int removed_index = flight.hand_index;

            if(state.selection.type == PendingActionType::PUT_HAND_ON_DECK_TOP)
            {
                apply_card_relocated(state, state.hand[removed_index].type);
            }

            hand_remove_at_to_deck_top(state, removed_index, selected_card);
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
        if(flight.frame >= (flight.origin == PlayPresentOrigin::DECK
                               ? game_layout::PLAY_DECK_APPROACH_FRAMES
                               : game_layout::PLAY_APPROACH_FRAMES))
        {
            flight.frame = 0;
            flight.phase = PlayRemovalPhase::HOLD;
        }

        break;

    case PlayRemovalPhase::HOLD:
        if(flight.frame >= play_hold_frames(flight))
        {
            if(!flight.play_resolved)
            {
                resolve_play_flight_effects(flight);
                commit_play_flight_destination(flight);
            }

            flight.frame = 0;
            flight.phase = PlayRemovalPhase::DEPART;
        }

        break;

    case PlayRemovalPhase::DEPART:
        if(flight.frame >= game_layout::PLAY_DEPART_FRAMES)
        {
            complete_play_flight(flight);
        }

        break;

    case PlayRemovalPhase::WAIT_PRESENTATION:
        complete_play_flight(flight);
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
    opening_draw_attempts_remaining = state.round_start_seed.effective_opening_draw();
    continue_opening_hand_deal();
}

void GameContext::continue_opening_hand_deal()
{
    while(opening_draw_attempts_remaining > 0 && deck.remaining() > 0)
    {
        --opening_draw_attempts_remaining;
        const bool first_attempt = state.first_deck_draw_this_round;
        CardRef card;

        if(!deck.draw(card))
        {
            break;
        }

        if(first_attempt && card.type == CardType::MIRACLE)
        {
            state.first_deck_draw_this_round = false;
            const int main_x = main_panel_offset_x();
            PlayResolutionContext context;
            context.source = PlaySource::DECK_TOP;
            context.apply_destination = false;
            pending_opening_hand_deal = true;
            begin_play_presentation(
                card,
                card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x),
                card_target_y_for_hud_icon(game_layout::HUD_DECK_Y),
                PlayPresentOrigin::DECK,
                context,
                RemovalStyle::TO_GRAVEYARD,
                true);
            selected_card = 0;
            return;
        }

        hand_add_card(state, card, true);
    }

    opening_draw_attempts_remaining = 0;

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

void GameContext::hand_slot_screen_position(int hand_index, int main_x, int& out_x, int& out_y,
                                            int layout_hand_count) const
{
    int layout_x = hand_x;

    if(layout_hand_count >= 0)
    {
        const int centered = layout_hand_count < game_layout::VISIBLE_CARD_COUNT ? layout_hand_count
                                                                                 : game_layout::VISIBLE_CARD_COUNT;
        layout_x = -(centered * game_layout::HAND_SPACING) / 2;
    }

    const int raise = hand_index >= 0 && hand_index < card_raise_offset.size() ? card_raise_offset[hand_index] : 0;
    out_x = layout_x + hand_index * game_layout::HAND_SPACING - scroll_x + edge_shift + main_x;
    out_y = game_layout::HAND_Y - raise;
}

void GameContext::begin_graveyard_card_fx(GraveyardExilePickKind pick_kind, int graveyard_index)
{
    if(active_transit_count() >= game_layout::MAX_TRANSIT_FLIGHTS)
    {
        return;
    }

    const int index = graveyard_index >= 0 ? graveyard_index : state.selection.cursor;

    if(index < 0 || index >= state.graveyard.size())
    {
        return;
    }

    TransitFlight* flight = alloc_transit_flight();

    if(!flight)
    {
        return;
    }

    const int main_x = main_panel_offset_x();
    int start_x = 0;
    int start_y = 0;
    const int cursor_raise = index < card_raise_offset.size() ? card_raise_offset[index] : 0;

    if(!graveyard_cursor_screen_position(index, state.graveyard.size(), game_layout::GRAVE_SPACING,
                                         game_layout::GRAVEYARD_BROWSE_Y, cursor_raise, row_scroll_x, main_x,
                                         start_x, start_y))
    {
        return;
    }

    const CardRef picked = state.graveyard[index];
    int gy_stagger_slot = 0;

    for(const TransitFlight& active_flight : transit_flights)
    {
        if(active_flight.active &&
           (active_flight.kind == TransitKind::GY_TO_HAND ||
            active_flight.kind == TransitKind::GY_TO_DECK ||
            active_flight.kind == TransitKind::GY_BIRDS_TO_DECK ||
            active_flight.kind == TransitKind::GY_EXILE))
        {
            ++gy_stagger_slot;
        }
    }

    flight->active = true;
    flight->frame = 0;
    flight->delay_frames = gy_stagger_slot * game_layout::TRANSIT_STAGGER_FRAMES;
    flight->card = picked;
    flight->start_x = start_x;
    flight->start_y = start_y;
    flight->graveyard_index = index;
    flight->gy_kind = pick_kind;
    flight->state_applied = false;

    if(pick_kind == GraveyardExilePickKind::TO_HAND)
    {
        flight->kind = TransitKind::GY_TO_HAND;
        flight->style = RemovalStyle::TO_GRAVEYARD;
        hand_slot_screen_position(state.hand.size(), main_x, flight->dest_x, flight->dest_y);
    }
    else if(pick_kind == GraveyardExilePickKind::TO_DECK_TOP ||
            pick_kind == GraveyardExilePickKind::SHUFFLE_TO_DECK ||
            pick_kind == GraveyardExilePickKind::BIRDS_TO_DECK)
    {
        flight->kind = pick_kind == GraveyardExilePickKind::BIRDS_TO_DECK ? TransitKind::GY_BIRDS_TO_DECK
                                                                          : TransitKind::GY_TO_DECK;
        flight->style = RemovalStyle::TO_DECK_TOP;
        flight->dest_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
        flight->dest_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    }
    else
    {
        flight->kind = TransitKind::GY_EXILE;
        flight->style = RemovalStyle::EXILE_DISSIPATE;
        flight->dest_x = card_target_x_for_score_center(main_x);
        flight->dest_y = card_target_y_for_score_center();
    }

    prepare_transit_flight_visual(*flight);
    sync_transit_flags();
}

bool GameContext::graveyard_exile_spam_select() const
{
    if(mode != GameMode::GRAVEYARD_TARGET)
    {
        return false;
    }

    if(state.selection.type == PendingActionType::EXILE_GRAVEYARD_MULTIPLY_BY_COUNT)
    {
        return true;
    }

    return state.selection.type == PendingActionType::EXILE_FROM_GRAVEYARD_THEN_MULTIPLY &&
           state.selection.remaining_picks > 0;
}

void GameContext::clear_graveyard_exile_fx()
{
    pending_graveyard_exile_fx.clear();
    rags_exile_deferred_finish = false;

    for(TransitFlight& flight : transit_flights)
    {
        if(!flight.active)
        {
            continue;
        }

        if(flight.kind == TransitKind::GY_TO_HAND || flight.kind == TransitKind::GY_TO_DECK ||
           flight.kind == TransitKind::GY_BIRDS_TO_DECK || flight.kind == TransitKind::GY_EXILE)
        {
            clear_transit_flight(flight);
        }
    }

    sync_transit_flags();
}

void GameContext::try_start_graveyard_exile_fx()
{
    try_start_pending_transits();
}

void GameContext::confirm_graveyard_multiply_exile_pick()
{
    if(state.graveyard.empty())
    {
        return;
    }

    state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
    const int picked_index = state.selection.cursor;
    const int main_x = main_panel_offset_x();
    int start_x = 0;
    int start_y = 0;
    const int cursor_raise = picked_index < card_raise_offset.size() ? card_raise_offset[picked_index] : 0;

    if(!graveyard_cursor_screen_position(picked_index, state.graveyard.size(), game_layout::GRAVE_SPACING,
                                         game_layout::GRAVEYARD_BROWSE_Y, cursor_raise, row_scroll_x, main_x,
                                         start_x, start_y))
    {
        return;
    }

    const CardRef card = state.graveyard[picked_index];
    graveyard_remove_at(state, picked_index);
    exile_push(state, card, true);
    ++state.selection.exiled_count;

    if(state.graveyard.empty())
    {
        state.selection.cursor = 0;
    }
    else
    {
        state.selection.cursor = clamp_graveyard_cursor(picked_index, state.graveyard.size());
    }

    sync_target_row_scroll(state.selection.cursor, state.graveyard.size());

    if(!pending_graveyard_exile_fx.full())
    {
        pending_graveyard_exile_fx.push_back(
            PendingGraveyardExileFx{card, start_x, start_y, GraveyardExilePickKind::MULTIPLY_BY_COUNT});
    }

    if(combo_try_start_pending(state))
    {
        enter_combo_mode();
        return;
    }

    if(state.graveyard.empty())
    {
        state.mul_from_card(state.selection.exiled_count);
        state.selection.exiled_count = 0;
        draw_round_score();
        rags_exile_deferred_finish = true;
    }

    try_start_graveyard_exile_fx();
}

void GameContext::confirm_graveyard_clover_exile_pick()
{
    if(state.graveyard.empty() || state.selection.remaining_picks <= 0)
    {
        return;
    }

    state.selection.cursor = clamp_graveyard_cursor(state.selection.cursor, state.graveyard.size());
    const int picked_index = state.selection.cursor;
    const int main_x = main_panel_offset_x();
    int start_x = 0;
    int start_y = 0;
    const int cursor_raise = picked_index < card_raise_offset.size() ? card_raise_offset[picked_index] : 0;

    if(!graveyard_cursor_screen_position(picked_index, state.graveyard.size(), game_layout::GRAVE_SPACING,
                                         game_layout::GRAVEYARD_BROWSE_Y, cursor_raise, row_scroll_x, main_x,
                                         start_x, start_y))
    {
        return;
    }

    const CardRef card = state.graveyard[picked_index];
    graveyard_remove_at(state, picked_index);
    exile_push(state, card, true);
    --state.selection.remaining_picks;

    if(state.graveyard.empty())
    {
        state.selection.cursor = 0;
    }
    else
    {
        state.selection.cursor = clamp_graveyard_cursor(picked_index, state.graveyard.size());
    }

    sync_target_row_scroll(state.selection.cursor, state.graveyard.size());

    if(!pending_graveyard_exile_fx.full())
    {
        pending_graveyard_exile_fx.push_back(
            PendingGraveyardExileFx{card, start_x, start_y, GraveyardExilePickKind::FROM_GRAVEYARD_THEN_MULTIPLY});
    }

    if(combo_try_start_pending(state))
    {
        if(state.selection.remaining_picks <= 0)
        {
            state.selection.type = PendingActionType::NONE;
        }

        enter_combo_mode();
        return;
    }

    if(state.selection.remaining_picks == 0)
    {
        state.mul_from_card(state.selection.multiply_factor);
        state.selection.multiply_factor = 0;
        draw_round_score();
        rags_exile_deferred_finish = true;
    }

    try_start_graveyard_exile_fx();
}

void GameContext::begin_keep_going_round_transfers(bool turtle_preserve)
{
    deferred_round_start_pending = true;
    deferred_round_start_turtle_preserve = turtle_preserve;

    keep_going_transfers_remaining = state.keep_going_returns[state.next_mod_index];
    state.keep_going_returns[state.next_mod_index] = 0;

    // GY fill is only for 7 Feet Deep's opening-draw override. A default 5-card
    // deal with a short deck must deal short and let the run end — not recycle GY.
    const RoundModifier& upcoming = state.future_mods[state.next_mod_index];

    if(upcoming.has_opening_draw_override())
    {
        const int opening_n = upcoming.effective_opening_draw();
        const int gy_size = state.graveyard.size();
        const int keep_actual = keep_going_transfers_remaining < gy_size ? keep_going_transfers_remaining
                                                                        : gy_size;
        const int deck_after_keep = state.deck.remaining() + keep_actual;
        const int fill_needed = opening_n - deck_after_keep;

        if(fill_needed > 0)
        {
            const int gy_left = gy_size - keep_actual;
            const int fill_actual = fill_needed < gy_left ? fill_needed : gy_left;

            if(fill_actual > 0)
            {
                keep_going_transfers_remaining += fill_actual;
            }
        }
    }

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

    keep_going_transfer_active = true;
    const int index = state.rng.get_int(state.graveyard.size());
    begin_graveyard_card_fx(GraveyardExilePickKind::TO_DECK_TOP, index);
}

void GameContext::apply_round_start_turtle_step()
{
    if(!state.apply_round_start_turtle_step())
    {
        return;
    }

    commit_round_with_checks();
    draw_total_score();
}

void GameContext::run_round_start_pipeline()
{
    // Step 1 (Dead Rising / opening-draw GY fill) runs via begin_keep_going_round_transfers before this.
    deal_opening_hand();
    apply_round_start_turtle_step();
    state.apply_round_start_adds();
    state.apply_round_start_multiply();
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

    state.start_new_round(deferred_round_start_turtle_preserve);
    reset_card_animation_state();
    run_round_start_pipeline();
    try_start_hand_draw_fx();
    process_instant_pending();
    draw_round_score();
    draw_total_score();

    if(!hand_draw_fx_blocking())
    {
        end_run_if_needed();

        if(!run_finished)
        {
            state.waive_optional_ghost_plays = false;
        }
    }
}

void GameContext::begin_birds_return_fx()
{
    if(state.birds_return_count <= 0)
    {
        return;
    }

    const int index = state.birds_return_start;

    if(index < 0 || index >= state.graveyard.size() ||
       state.graveyard[index].type != CardType::BIRDS_OF_A_FEATHER)
    {
        return;
    }

    if(active_transit_count() >= game_layout::MAX_TRANSIT_FLIGHTS)
    {
        return;
    }

    TransitFlight* flight = alloc_transit_flight();

    if(!flight)
    {
        return;
    }

    const int main_x = main_panel_offset_x();
    const CardRef picked = state.graveyard[index];
    flight->active = true;
    flight->frame = 0;
    flight->delay_frames = 0;
    flight->card = picked;
    flight->graveyard_index = index;
    flight->gy_kind = GraveyardExilePickKind::BIRDS_TO_DECK;
    flight->kind = TransitKind::GY_BIRDS_TO_DECK;
    flight->style = RemovalStyle::TO_DECK_TOP;
    flight->state_applied = false;
    flight->start_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
    flight->start_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);
    flight->dest_x = card_target_x_for_hud_icon(game_layout::HUD_DECK_X, main_x);
    flight->dest_y = card_target_y_for_hud_icon(game_layout::HUD_DECK_Y);
    prepare_transit_flight_visual(*flight);
    sync_transit_flags();
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

    begin_birds_return_fx();
    return zone_transit_active();
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
    return hand_scheduled_count(state, in_flight_deck_draw_count());
}

void GameContext::try_start_hand_draw_fx()
{
    try_start_pending_transits();
}

void GameContext::tick_hand_draw_fx()
{
    tick_transit_flights();
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

    return false;
}

bool GameContext::card_resolution_blocking_round_end() const
{
    return selection_blocks_pending_finish() ||
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
    if(hand_draw_fx_blocking())
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
    if(state.effect_draw_remaining > 0 && !hand_draw_fx_blocking())
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
       deck_search_resolve_active())
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

void GameContext::tick_roll_over_pending()
{
    if(mode != GameMode::NORMAL || !state.roll_over_substitution_active)
    {
        return;
    }

    try_advance_roll_over_sequence();
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
        try_advance_roll_over_sequence();
        swivel_clear_wait_if_hand_empty(*this);

        if(empty_hand_triggers_round_end(state))
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

    try_advance_roll_over_sequence();

    if(empty_hand_triggers_round_end(state))
    {
        finish_empty_hand_round();
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
    sync_hand_selection();
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
    draw_round_score();
    draw_total_score();
}

void GameContext::draw_inspect(CardType type)
{
    // Inspect text needs many sprite slots; release hidden score labels and pops first.
    round_text_sprites.clear();
    text_sprites.clear();
    _round_score_initialized = false;
    _total_score_initialized = false;
    score_count_cancel(*this, TrinketScoreField::ROUND);
    score_count_cancel(*this, TrinketScoreField::TOTAL);
    score_pops.clear();

    release_card_display_tiles(inspect_card);
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

bool GameContext::graveyard_library_pick_active() const
{
    return graveyard_pick_active() && selection_sends_to_library(state.selection.type);
}

bool GameContext::card_selection_ui_active() const
{
    return mode == GameMode::SCRY || mode == GameMode::DECK_SEARCH ||
           mode == GameMode::GRAVEYARD_TARGET || graveyard_pick_active();
}

bool GameContext::selection_blocks_pending_finish() const
{
    // Interactive follow-ups pop their pending action when the mode opens. Block
    // automatic finish_pending_queue calls (e.g. play-flight completion) until the
    // player or effect handler explicitly closes the selection (close_selection=true).
    switch(mode)
    {
    case GameMode::SCRY:
        return !state.selection.scry_buffer.empty();
    case GameMode::DECK_SEARCH:
        return !state.selection.deck_search_buffer.empty();
    case GameMode::GRAVEYARD_TARGET:
    case GameMode::GRAVEYARD_PICK:
        return true;
    case GameMode::DISCARD_TARGET:
        return !state.hand.empty();
    case GameMode::BUILD_NUMBER_DIGIT:
        return state.build_a_number_active;
    case GameMode::SCORE_SWAP:
        return score_swap_is_active(*this);
    case GameMode::COMBO:
        return combo_focus_active() || state.combo_cinematic.active;
    default:
        break;
    }

    // Desync guard: score-swap FX can outlive mode if finish ran too early.
    if(score_swap_is_active(*this))
    {
        return true;
    }

    if(deck_search_resolve_active() || graveyard_card_fx_active)
    {
        return true;
    }

    return false;
}

bool GameContext::selection_mode_allows_input_during_presentation() const
{
    switch(mode)
    {
    case GameMode::SCRY:
    case GameMode::DECK_SEARCH:
    case GameMode::GRAVEYARD_TARGET:
    case GameMode::GRAVEYARD_PICK:
    case GameMode::DISCARD_TARGET:
    case GameMode::BUILD_NUMBER_DIGIT:
    case GameMode::SCORE_SWAP:
        return true;
    default:
        return false;
    }
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
        &state.instance_pool, &hud_count_generator, 0, &state, true);

    if(graveyard_pick_active())
    {
        const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;

        for(int slot = 0; slot < row.visible_count; ++slot)
        {
            const int graveyard_index = first_visible + slot;

            if(graveyard_slot_hidden_by_transit(graveyard_index))
            {
                grave_row_display[slot].set_visible(false);
            }
        }
    }

    apply_row_fade_bands(fade_bands, game_layout::GRAVEYARD_BROWSE_Y, row.has_left, row.has_right);

    constexpr bn::array<int, 4> fade_band_x = {-116, -108, 108, 116};

    for(int band_index = 0; band_index < fade_bands.size(); ++band_index)
    {
        fade_bands[band_index].set_position_x(fade_band_x[band_index] + panel_x);
    }

    if(row.cursor_slot >= 0 && !graveyard_library_pick_active())
    {
        graveyard_pick_placeholder.set_position(panel_x, game_layout::GRAVEYARD_BROWSE_PLACEHOLDER_Y);
        graveyard_pick_placeholder.set_visible(true);
    }

    return row;
}

void GameContext::sync_graveyard_library_marker(int main_x)
{
    if(!graveyard_library_pick_active())
    {
        return;
    }

    const int grave_cursor = state.selection.cursor;
    const int grave_count = state.graveyard.size();

    if(grave_count <= 0 || grave_cursor < 0 || grave_cursor >= grave_count)
    {
        return;
    }

    constexpr int window = game_layout::VISIBLE_CARD_COUNT;
    const int visible_count = grave_count < window ? grave_count : window;
    const int row_start_x = -(visible_count * game_layout::GRAVE_SPACING) / 2;
    const int logical_first = first_visible_index(grave_cursor, grave_count, window);
    const int first_visible = row_scroll_x / game_layout::GRAVE_SPACING;
    const int scroll_sub = row_scroll_x - first_visible * game_layout::GRAVE_SPACING;
    const int cursor_slot = grave_cursor - logical_first;

    if(cursor_slot < 0 || cursor_slot >= visible_count)
    {
        return;
    }

    const int grave_cursor_raise =
        grave_cursor < card_raise_offset.size() ? card_raise_offset[grave_cursor] : 0;
    const int grave_cursor_wave = row_scroll_pair_raise(
        grave_cursor, grave_cursor, row_scroll_x, target_row_scroll_x, game_layout::GRAVE_SPACING,
        grave_count);
    const int card_x = row_start_x + cursor_slot * game_layout::GRAVE_SPACING - scroll_sub + main_x;
    const int selected_card_top = game_layout::GRAVEYARD_BROWSE_Y - grave_cursor_raise - grave_cursor_wave;

    graveyard_pick_placeholder.set_visible(false);
    library_marker.set_position(card_x + 16, selected_card_top + 32);
    library_marker.set_visible(true);
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
        case PendingActionType::OVERCLOCK_DISCARD_PROMPT:
        case PendingActionType::PAPER_SWAP_HAND:
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

    if(!state.hand.empty())
    {
        return false;
    }

    if(skip_pending_combine)
    {
        skip_pending_combine = false;
        return false;
    }

    if(state.pending_combo.length > 0)
    {
        round_end_pending = true;
        return true;
    }

    return false;
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

    begin_next_pending_or_finish(true);
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
    combo_progress_bars.set_visible(false);
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

GameContext::RoundFinishResult GameContext::try_finish_round_after_empty_hand()
{
    if(!skip_pending_combine && !empty_hand_triggers_round_end(state))
    {
        round_end_pending = false;
        clamp_hand_cursor();
        return RoundFinishResult::Blocked;
    }

    // Empty hand/deck is only meaningful after the played card has completely
    // resolved. A deferred round-end can otherwise race an action started on the
    // final removal frame (Necromancy's GY shuffle is the clearest example).
    if(card_resolution_blocking_round_end())
    {
        round_end_pending = true;
        return RoundFinishResult::Blocked;
    }

    // Combos resolve after the card's full effect but before round/run end.
    if(block_round_end_for_combo())
    {
        return RoundFinishResult::Blocked;
    }

    round_end_pending = false;
    swivel_clear_wait_if_hand_empty(*this);

    if(state.finale_active)
    {
        state.waive_optional_ghost_plays = false;
        skip_pending_combine = false;
        finish_finale_run();
        return RoundFinishResult::EndedRun;
    }

    skip_pending_combine = false;

    release_idle_card_pools();
    const bool turtle_preserve = state.turtle_rounds_remaining > 0;

    if(!turtle_preserve)
    {
        commit_round_with_checks();
        draw_total_score();

        if(campaign_ui.mode == CampaignMode::NUMBER_NOW &&
           state.current_round == campaign_ui.number_now_scoring_round)
        {
            state.waive_optional_ghost_plays = false;
            request_run_end();
            return RoundFinishResult::EndedRun;
        }
    }

    begin_keep_going_round_transfers(turtle_preserve);

    if(deferred_round_start_pending)
    {
        // Keep waive true until finish_deferred_round_start runs end_run_if_needed.
        return RoundFinishResult::CommittedNewRound;
    }

    if(!hand_draw_fx_blocking())
    {
        end_run_if_needed();
    }

    if(!run_finished)
    {
        state.waive_optional_ghost_plays = false;
    }

    return run_finished ? RoundFinishResult::EndedRun : RoundFinishResult::CommittedNewRound;
}

void GameContext::roll_over_commit_pick(int choice_index)
{
    if(!roll_over_pick_active(state) || choice_index < 0 || choice_index >= state.roll_over_choice_count)
    {
        return;
    }

    if(removing_card && !play_can_overlap())
    {
        return;
    }

    const CardRef picked = state.roll_over_choices[choice_index];
    const int other_index = choice_index == 0 ? 1 : 0;
    const CardRef follow_up = state.roll_over_choices[other_index];

    state.roll_over_choices = {};
    state.roll_over_choice_count = 0;
    state.roll_over_pick_active = false;
    state.roll_over_follow_up_card = follow_up;

    roll_over_restore_stashed_hand(state);

    capture_removal_start();

    PlayResolutionContext context;
    context.source = PlaySource::HAND;
    context.apply_destination = false;

    echo_play_badge_active = state.echo_first_play_active() && card_has_play_effect(state, picked);
    begin_play_presentation(picked, removal_start_x, removal_start_y, PlayPresentOrigin::GRAVEYARD, context,
                            RemovalStyle::EXILE_DISSIPATE);
    sync_hand_selection();
    update_target_scroll();
}

bool GameContext::try_advance_roll_over_sequence()
{
    if(!state.roll_over_substitution_active || state.roll_over_pick_active)
    {
        return false;
    }

    if(presentation_fx_blocking() || !state.pending_actions.empty() || play_flight_count() > 0 ||
       removing_card || hand_draw_fx_blocking() || card_resolution_blocking_round_end())
    {
        return false;
    }

    if(state.roll_over_follow_up_card.type != CardType::COUNT)
    {
        const CardRef follow_up = state.roll_over_follow_up_card;
        state.roll_over_follow_up_card = CardRef{};
        state.roll_over_awaiting_follow_up = true;

        const int main_x = main_panel_offset_x();
        const int start_x = card_target_x_for_hud_icon(game_layout::HUD_GRAVEYARD_X, main_x);
        const int start_y = card_target_y_for_hud_icon(game_layout::HUD_GRAVEYARD_Y);

        PlayResolutionContext context;
        context.source = PlaySource::HAND;
        context.apply_destination = false;

        echo_play_badge_active = state.echo_first_play_active() && card_has_play_effect(state, follow_up);
        begin_play_presentation(follow_up, start_x, start_y, PlayPresentOrigin::GRAVEYARD, context,
                                RemovalStyle::EXILE_DISSIPATE);
        sync_hand_selection();
        update_target_scroll();
        return true;
    }

    if(state.roll_over_awaiting_follow_up)
    {
        roll_over_finish_sequence(state, &selected_card);
        sync_hand_selection();
        update_target_scroll();
        return true;
    }

    try_finish_roll_over_substitution(state, &selected_card);
    return false;
}

void GameContext::finish_empty_hand_round()
{
    try_finish_round_after_empty_hand();
}

void GameContext::tick_round_end_pending()
{
    // Selection modes pop their pending action on entry; recover if auto-finish was skipped.
    if(mode == GameMode::DISCARD_TARGET && state.hand.empty() && state.pending_actions.empty() &&
       play_flight_count() == 0 && !hand_draw_fx_blocking())
    {
        begin_next_pending_or_finish();
    }
    else if(score_swap_is_active(*this) && mode != GameMode::SCORE_SWAP &&
            state.pending_actions.empty() && play_flight_count() == 0 && !hand_draw_fx_blocking())
    {
        mode = GameMode::SCORE_SWAP;
    }

    if(mode == GameMode::NORMAL && state.pending_actions.empty() && play_flight_count() == 0 &&
       !removing_card && state.hand.empty() && hand_draw_fx_blocking())
    {
        try_start_hand_draw_fx();
    }

    if(!round_end_pending)
    {
        if(mode != GameMode::NORMAL || !state.pending_actions.empty() || removing_card ||
           play_flight_count() > 0 || !state.hand.empty() || !empty_hand_triggers_round_end(state))
        {
            return;
        }

        if(card_resolution_blocking_round_end())
        {
            round_end_pending = true;
            return;
        }
    }

    finish_empty_hand_round();
    update_target_scroll();
}
