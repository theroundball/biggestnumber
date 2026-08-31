#ifndef GAME_CONTEXT_H
#define GAME_CONTEXT_H

#include "bn_array.h"
#include "bn_seed_random.h"
#include "bn_span.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "card.h"
#include "game_state.h"
#include "game_types.h"
#include "game_ui.h"
#include "hud.h"
#include "play_resolution.h"

#include "score_pop_system.h"
#include "score_count_system.h"
#include "score_swap_system.h"
#include "swivel_system.h"
#include "trinket_system.h"
#include "ui_common.h"

struct PlayFlight
{
    bool active = false;
    bool is_discard = false;
    PlayRemovalPhase phase = PlayRemovalPhase::APPROACH;
    bool center_beat = false;
    PlayPresentOrigin origin = PlayPresentOrigin::HAND;
    CardRef played_ref{};
    PlayResolutionContext play_context{};
    bool play_resolved = false;
    bool is_miracle_bonus = false;
    bool swivel_follow = false;
    bool cycle_draw = false;
    bool cycle_exile = false;
    bool mill_without_play = false;
    bool mill_reveal_flex_continue = false;
    bool mill_reveal_waterfall_continue = false;
    bool mill_reveal_draw_on_hit = false;
    int mill_reveal_round_before = 0;
    int mill_reveal_total_before = 0;
    bool hand_committed = false;
    PlaySource scoring_source = PlaySource::HAND;
    RemovalStyle style = RemovalStyle::TO_GRAVEYARD;
    int frame = 0;
    int hand_index = -1;
    int start_x = 0;
    int start_y = 0;
    Card fx_card;
};

struct TransitFlight
{
    bool active = false;
    int frame = 0;
    int delay_frames = 0;
    TransitKind kind = TransitKind::NONE;
    GraveyardExilePickKind gy_kind = GraveyardExilePickKind::NONE;
    CardRef card{};
    int start_x = 0;
    int start_y = 0;
    int dest_x = 0;
    int dest_y = 0;
    int dest_hand_index = 0;
    int graveyard_index = -1;
    RemovalStyle style = RemovalStyle::TO_GRAVEYARD;
    bool state_applied = false;
    bool miracle_auto_play = false;
    Card fx_card;
};

class GameContext
{
public:
    GameContext(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch);

    bool run_finished = false;
    bool run_end_presentation_pending = false;
    bool confirm_input_armed = false;

    bn::seed_random random_engine;
    Deck deck;
    GameState state;
    int deck_high_score = 0;
    // Total score turns green after exceeding this (run peak or deck high score).
    int score_to_beat = 0;
    CampaignUiContext campaign_ui{};
    GameSceneResult scene_result;

    GameMode mode = GameMode::NORMAL;
    int selected_card = 0;
    int browse_cursor = 0;
    bool removing_card = false;
    bool pending_opening_hand_deal = false;
    int opening_draw_attempts_remaining = 0;
    bool echo_play_badge_active = false;
    bool swivel_follow_pending = false;
    int removal_start_x = 0;
    int removal_start_y = 0;
    int latest_flight_index = -1;
    int pending_cycle_draws = 0;
    bn::array<PlayFlight, game_layout::MAX_PLAY_FLIGHTS> play_flights;
    bool swapping_card = false;
    int swap_frame = 0;
    int swap_direction = 0;
    int scroll_x = 0;
    int target_scroll_x = 0;
    int row_scroll_x = 0;
    int target_row_scroll_x = 0;
    int target_row_scroll_index = 0;
    int hand_x = game_layout::HAND_X;
    int target_hand_x = game_layout::HAND_X;
    DirectionRepeatState direction_repeat;
    int edge_shift = 0;
    int target_edge_shift = 0;
    bn::array<int, 60> card_raise_offset{};
    bool game_over = false;
    bool y2k_bust_modal_active = false;
    bn::vector<bn::sprite_ptr, 48> y2k_bust_sprites;
    bool round_end_pending = false;
    bool skip_pending_combine = false;

    SidePanel side_panel = SidePanel::NONE;
    PanelTransition panel_transition = PanelTransition::NONE;
    // When opening the right-side browse panel, which zone to show.
    SidePanel browse_open_target = SidePanel::GRAVEYARD;
    int panel_slide = 0;
    int last_main_sprite_offset = 0;
    int last_round_sprite_offset = 0;
    int last_inspect_sprite_offset = 0;
    int last_details_sprite_offset = 0;
    int details_last_library = -1;
    int details_last_round = -1;
    int details_last_goal = -1;
    int details_last_mode = int(CampaignMode::NONE);

    bn::array<GameFadeBand, 4> fade_bands;
    GameMarker x_marker;
    GameMarker swap_lock_marker;
    bn::sprite_ptr library_marker;
    CardEffectBadge echo_badge;
    CardEffectBadge swivel_badge;
    GraveyardPickPlaceholder graveyard_pick_placeholder;
    ScoreProgressBar score_progress_bar;
    ComboProgressBars combo_progress_bars;
    bn::array<GameMarker, game_layout::VISIBLE_CARD_COUNT> grave_exclude_markers;
    bn::array<Card, game_layout::HAND_DISPLAY_POOL> hand_display;
    Card echo_ghost_card;
    bool echo_ghost_active = false;
    int echo_ghost_x = 0;
    int echo_ghost_y = 0;
    bool graveyard_card_fx_active = false;
    bool rags_exile_deferred_finish = false;

    struct PendingGraveyardExileFx
    {
        CardRef card;
        int start_x = 0;
        int start_y = 0;
        GraveyardExilePickKind kind = GraveyardExilePickKind::NONE;
    };

    bn::vector<PendingGraveyardExileFx, 12> pending_graveyard_exile_fx;
    bn::array<TransitFlight, game_layout::MAX_TRANSIT_FLIGHTS> transit_flights;
    bool keep_going_transfer_active = false;
    int keep_going_transfers_remaining = 0;
    bool deferred_round_start_pending = false;
    bool deferred_round_start_turtle_preserve = false;
    bn::array<Card, game_layout::VISIBLE_CARD_COUNT> grave_row_display;
    bn::array<Card, game_layout::SCRY_VISIBLE> scry_display;
    bn::array<Card, 4> combo_display;
    ComboFocusPhase combo_focus = ComboFocusPhase::NONE;
    // True only when the cinematic itself opened the graveyard panel, so the same
    // code owns closing it again.
    bool combo_focus_panel_opened = false;
    int combo_focus_frame = 0;
    int combo_focus_anchor_x = 0;
    GameMode combo_interrupted_mode = GameMode::NORMAL;
    PendingActionType combo_resume_type = PendingActionType::NONE;
    bn::array<Card, 2> swivel_display;
    Card inspect_card;
    SwivelFxState swivel_fx;

    bn::sprite_text_generator text_generator;
    bn::vector<bn::sprite_ptr, 32> text_sprites;
    bn::sprite_text_generator round_text_generator;
    bn::vector<bn::sprite_ptr, 32> round_text_sprites;
    bn::sprite_text_generator inspect_text_generator;
    bn::vector<bn::sprite_ptr, 64> inspect_sprites;
    bn::sprite_text_generator hud_count_generator;
    bn::sprite_text_generator hud_mod_generator;
    bn::sprite_text_generator details_text_generator;
    bn::vector<bn::sprite_ptr, 48> details_sprites;
    bn::vector<bn::sprite_ptr, 24> action_prompt_sprites;
    PersistentHud hud;

    struct DeckSearchResolveFx
    {
        bool active = false;
        DeckSearchResolvePhase phase = DeckSearchResolvePhase::PICK_FLIGHT;
        int frame = 0;
        CardType picked_type = CardType::COUNT;
        uint8_t picked_instance_id = NO_INSTANCE;
        int start_x = 0;
        int start_y = 0;
        RemovalStyle removal_style = RemovalStyle::TO_GRAVEYARD;
        bool swivel_follow = false;
    };

    DeckSearchResolveFx deck_search_resolve_fx;
    bool hand_draw_fx_active = false;
    LuckySevensFxState lucky_sevens_fx;
    bn::vector<TrinketScoreField, 8> pending_lucky_sevens;
    bn::vector<bn::sprite_ptr, 16> trinket_fx_sprites;
    bn::vector<ScorePop, MAX_ACTIVE_SCORE_POPS> score_pops;

    bn::array<ScoreCountFxState, 2> score_count_fx{};
    ScoreSwapFxState score_swap_fx;
    bn::vector<bn::sprite_ptr, 4> score_swap_marker_sprites;

    bool inspecting = false;
    int inspect_shown_index = -1;

    int round_score_wiggle_frames = 0;
    int total_score_wiggle_frames = 0;
    int _round_wiggle_x = 0;
    int _round_wiggle_y = 0;
    int _total_wiggle_x = 0;
    int _total_wiggle_y = 0;
    bool _round_score_initialized = false;
    bool _total_score_initialized = false;
    bn::string<48> _cached_round_score_text;
    int _cached_total_score = 0;
    int _cached_total_score_view_offset = 0;
    int total_score_view_offset = 0;
    int round_score_view_offset = 0;

    void shutdown_for_exit();
    void reset_card_animation_state();
    void capture_removal_start();
    void begin_play_presentation(CardRef card, int start_x, int start_y, PlayPresentOrigin origin,
                                 PlayResolutionContext context, RemovalStyle style,
                                 bool miracle_bonus = false);
    void begin_discard_presentation(int hand_index);
    void begin_direct_removal(int start_x, int start_y, RemovalStyle style, bool is_discard,
                              bool cycle_exile = false);
    PlayFlight* alloc_play_flight();
    PlayFlight* latest_play_flight();
    const PlayFlight* latest_play_flight() const;
    int play_flight_count() const;
    bool play_can_overlap() const;
    int play_flight_fan_offset_x(const PlayFlight& flight) const;
    int play_hold_frames(const PlayFlight& flight) const;
    bool graveyard_slot_hidden_by_flight(int graveyard_index) const;
    bool play_hides_hand_index(int hand_index) const;
    bool play_hides_graveyard_visual(int visual_index) const;
    bool play_hides_visual_slot(int visual_index) const;
    void snap_selected_card_raise();
    void retarget_selection_off_hidden_slot();
    Card& exclusive_fx_card();
    void sync_removing_card();
    void clear_play_flight(PlayFlight& flight);
    void resolve_play_flight_effects(PlayFlight& flight);
    void commit_play_flight_destination(PlayFlight& flight);
    void complete_play_flight(PlayFlight& flight);
    void tick_one_play_flight(PlayFlight& flight);
    bool tick_removal_fx();
    CardFlightSample sample_play_flight(const PlayFlight& flight, int main_x, int dest_x, int dest_y) const;
    void render_one_play_flight(PlayFlight& flight, int main_x, bool skip_face_labels);
    int hand_removal_shift() const;
    bool hand_removal_gap_layout() const;
    int visual_hand_slot_count() const;
    int hand_index_for_visual_slot(int visual_index) const;
    int visual_slot_for_hand_index(int hand_index) const;
    int active_removal_slot_index() const;
    int layout_hand_count() const;
    int hand_layout_center_count() const;
    void deal_opening_hand();
    void continue_opening_hand_deal();
    void draw_total_score();
    void draw_round_score();
    void show_total_score_value(int value);
    void show_round_score_running(int running, int end_multiplier);
    void sync_score_digit_view(SwapScoreField field, int digit_index);
    [[nodiscard]] int score_progress_goal() const;
    [[nodiscard]] bool score_progress_visible() const;
    void sync_score_progress_bar();
    void sync_combo_progress_bars();
    [[nodiscard]] bool should_end_run() const;
    void end_run_if_needed();
    [[nodiscard]] bool score_presentation_blocking() const;
    void request_run_end();
    void tick_run_end_presentation();
    void finish_finale_run();
    void finalize_total_score_display();
    void finalize_round_score_display();
    void tick_score_wiggles();
    int current_highlight_type() const;
    int current_highlight_index() const;
    void draw_inspect(CardType type);
    void clear_inspect();
    void snap_active_scroll();
    void sync_hand_selection();
    void prepare_hand_selection_mode();
    void clamp_hand_cursor();
    void begin_graveyard_card_fx(GraveyardExilePickKind pick_kind, int graveyard_index = -1);
    void prepare_transit_flight_visual(TransitFlight& flight);
    [[nodiscard]] bool graveyard_exile_spam_select() const;
    void confirm_graveyard_multiply_exile_pick();
    void confirm_graveyard_clover_exile_pick();
    void try_start_graveyard_exile_fx();
    void clear_graveyard_exile_fx();
    void begin_birds_return_fx();
    void begin_keep_going_round_transfers(bool turtle_preserve);
    void try_begin_keep_going_transfer();
    void finish_deferred_round_start();
    void run_round_start_pipeline();
    void apply_round_start_turtle_step();
    void hand_slot_screen_position(int hand_index, int main_x, int& out_x, int& out_y,
                                   int layout_hand_count = -1) const;
    int graveyard_card_fx_frame_count(const TransitFlight& flight) const;
    void complete_deck_to_hand_transit(TransitFlight& flight);
    void complete_graveyard_transit(TransitFlight& flight);
    bool try_begin_birds_return_fx();
    void complete_birds_return();
    void resolve_birds_return_instantly();
    void begin_deck_search_resolve(CardRef played, int start_x, int start_y, bool swivel_follow = false);
    void tick_deck_search_resolve();
    void finish_deck_search_resolve();
    bool deck_search_resolve_active() const;
    TransitFlight* alloc_transit_flight();
    int active_transit_count() const;
    int in_flight_deck_draw_count() const;
    void sync_transit_flags();
    void try_start_pending_transits();
    void tick_transit_flights();
    void complete_transit_flight(TransitFlight& flight);
    void clear_transit_flight(TransitFlight& flight);
    void render_transit_flights(int main_x);
    bool graveyard_slot_hidden_by_transit(int graveyard_index) const;
    bool zone_transit_active() const;
    bool hand_draw_fx_blocking() const;
    bool card_resolution_blocking_round_end() const;
    int scheduled_hand_count() const;
    void try_start_hand_draw_fx();
    void tick_hand_draw_fx();
    bool presentation_fx_blocking() const;
    bool try_drain_echo_replay();
    void tick_echo_pending();
    void tick_roll_over_pending();
    void shift_card_raise_after_remove(int removed_index);
    bool confirm_pressed() const;

    void begin_panel_transition(PanelTransition transition);
    void tick_panel();
    void complete_panel_transition();
    bool panel_transition_active() const;
    bool show_details_layer() const;
    bool show_graveyard_layer() const; // right-side card browse: GY or exile
    bool graveyard_pick_active() const;
    bool graveyard_library_pick_active() const;
    void sync_graveyard_library_marker(int main_x);
    bool card_selection_ui_active() const;
    bool selection_blocks_pending_finish() const;
    bool selection_mode_allows_input_during_presentation() const;
    // L = +1 (hand→info→exile→GY→hand), R = -1 (reverse).
    void cycle_side_panel(int direction);
    void switch_side_panel(SidePanel target);
    bn::span<const CardRef> active_browse_cards() const;
    void prepare_browse_for_panel(SidePanel panel);
    int main_panel_offset_x() const;
    int details_panel_offset_x() const;
    int graveyard_panel_offset_x() const;
    void sync_details_panel(bool force);
    void position_details_sprites();
    void apply_sprite_offset_delta(bn::span<bn::sprite_ptr> sprites, int target_offset, int& last_offset);
    void position_main_score_sprites();
    void hide_hand_display();
    void release_idle_card_pools();
    void position_inspect_sprites();
    CardRowResult render_graveyard_view(int panel_x, int cursor);
    void render_graveyard_browse(int panel_x);
    void render_graveyard_exclude_marks(const CardRowResult& row, int panel_x, int card_y,
                                        CardType exclude_type, int cursor);

    void update_target_scroll();
    void sync_target_row_scroll(int cursor, int count);
    void snap_row_scroll(int spacing);
    void sync_row_scroll_for_mode(int cursor, int count, int spacing);

    void process_instant_pending();
    void begin_next_pending_or_finish(bool close_selection = false);
    void tick_evaluate_ghost_steps();
    enum class RoundFinishResult
    {
        Blocked,
        CommittedNewRound,
        EndedRun,
    };
    RoundFinishResult try_finish_round_after_empty_hand();
    void finish_empty_hand_round();
    void roll_over_commit_pick(int choice_index);
    bool try_advance_roll_over_sequence();
    void tick_round_end_pending();
    void arm_echo_replay(CardRef played, PlaySource scoring_source, int ghost_x, int ghost_y);
    void advance_effect_draw();
    void continue_effect_draw_batch();
    // Re-scan hand/GY and start a pending combo if any. True = do not end the round yet.
    bool block_round_end_for_combo();

    void tick_combo();
    void tick_y2k_bust();
    void render_y2k_bust_overlay();
    void finish_combo_cinematic();
    void resume_after_combo();
    bool try_start_pending_combo();
    void enter_combo_mode();
    void begin_combo_focus();
    void begin_combo_focus_return();
    bool combo_focus_active() const;
    bool combo_focus_highlights_graveyard_card(int graveyard_index) const;
    int combo_focus_graveyard_index(int card_index) const;
    void combo_focus_slot_position(int card_index, int panel_x, int& out_x, int& out_y) const;
    void hide_combo_focus_row_cards();
    void render_combo_focus_frame(int score_target_x, int score_target_y);
    bool poll_direction(int& current_direction, bool scrolling, bool& direction_triggered,
                        int& direction_steps);
    void handle_input();
    void handle_input_inspect_toggle();
    void handle_input_side_panel(int current_direction, bool direction_triggered, int direction_steps);
    bool handle_input_presentation();
    void handle_input_normal(int current_direction, bool direction_triggered, int direction_steps,
                             bool scrolling);
    void handle_input_discard_target(int current_direction, bool direction_triggered, int direction_steps,
                                     bool scrolling);
    void handle_input_graveyard_target(int current_direction, bool direction_triggered, int direction_steps,
                                       bool row_scrolling);
    void handle_input_deck_search(int current_direction, bool direction_triggered, int direction_steps,
                                  bool row_scrolling);
    void handle_input_graveyard_pick(int current_direction, bool direction_triggered, int direction_steps,
                                     bool row_scrolling);
    void handle_input_scry(int current_direction, bool direction_triggered, int direction_steps,
                           bool row_scrolling);
    void handle_input_build_number_digit(int current_direction, bool direction_triggered,
                                         int direction_steps, bool scrolling);
    void handle_input_poker_hand_digit(int current_direction, bool direction_triggered,
                                       int direction_steps, bool scrolling);
    void sync_inspect_panel();
    void tick_scroll();
    void tick_card_raise();
    void render_play_presentation_overlay(int main_x);
    void sync_score_sprite_depth();
    void render_frame();
    void render_combo_frame(int main_x);
    void sync_pair_swap_prompt();
    void render_graveyard_selection_frame(int main_x);
    void render_deck_search_frame(int main_x);
    void render_scry_frame(int main_x);
    void render_hand_frame(int main_x, int swap_shift, int removal_shift);
    void commit_round_with_checks();
};

#endif
