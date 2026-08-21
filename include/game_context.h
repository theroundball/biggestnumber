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

class GameContext
{
public:
    GameContext(const bn::vector<CardRef, 50>& collection, const BattleLaunch& launch);

    bool run_finished = false;
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
    bool removal_is_discard = false;
    PlayRemovalPhase removal_phase = PlayRemovalPhase::APPROACH;
    bool removal_center_beat = false;
    PlayPresentOrigin removal_origin = PlayPresentOrigin::HAND;
    CardRef removal_played_ref{};
    PlayResolutionContext removal_play_context{};
    bool removal_play_resolved = false;
    bool removal_is_miracle_bonus = false;
    bool removal_swivel_follow = false;
    bool pending_opening_hand_deal = false;
    RemovalStyle removal_style = RemovalStyle::TO_GRAVEYARD;
    bool echo_play_badge_active = false;
    bool swivel_follow_pending = false;
    int removal_frame = 0;
    int removal_hand_index = -1;
    int removal_start_x = 0;
    int removal_start_y = 0;
    bool swapping_card = false;
    bool swap_first_step = true;
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
    bool round_end_pending = false;

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
    CardEffectBadge echo_badge;
    CardEffectBadge swivel_badge;
    GraveyardPickPlaceholder graveyard_pick_placeholder;
    bn::array<GameMarker, game_layout::VISIBLE_CARD_COUNT> grave_exclude_markers;
    bn::array<Card, game_layout::HAND_DISPLAY_POOL> hand_display;
    Card removal_fx_card;
    bool graveyard_card_fx_active = false;
    int graveyard_card_fx_frame = 0;
    int graveyard_card_fx_start_x = 0;
    int graveyard_card_fx_start_y = 0;
    CardType graveyard_card_fx_type = CardType::COUNT;
    uint8_t graveyard_card_fx_instance_id = NO_INSTANCE;
    int graveyard_card_fx_index = 0;
    int graveyard_card_fx_dest_x = 0;
    int graveyard_card_fx_dest_y = 0;
    GraveyardExilePickKind graveyard_card_fx_kind = GraveyardExilePickKind::NONE;
    RemovalStyle graveyard_card_fx_style = RemovalStyle::EXILE_DISSIPATE;
    bn::array<Card, game_layout::VISIBLE_CARD_COUNT> grave_row_display;
    bn::array<Card, game_layout::SCRY_VISIBLE> scry_display;
    bn::array<Card, 4> combo_display;
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
    int hand_draw_fx_frame = 0;
    int hand_draw_fx_start_x = 0;
    int hand_draw_fx_start_y = 0;
    int hand_draw_fx_dest_x = 0;
    int hand_draw_fx_dest_y = 0;
    int hand_draw_fx_dest_index = 0;
    CardRef hand_draw_fx_card{};
    LuckySevensFxState lucky_sevens_fx;
    bn::vector<TrinketScoreField, 8> pending_lucky_sevens;
    bn::vector<bn::sprite_ptr, 16> trinket_fx_sprites;
    bn::vector<ScorePop, MAX_ACTIVE_SCORE_POPS> score_pops;

    bn::array<ScoreCountFxState, 2> score_count_fx{};
    ScoreSwapFxState score_swap_fx;

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

    void reset_card_animation_state();
    void capture_removal_start();
    void begin_play_presentation(CardRef card, int start_x, int start_y, PlayPresentOrigin origin,
                                 PlayResolutionContext context, RemovalStyle style,
                                 bool miracle_bonus = false);
    void begin_discard_presentation(int hand_index);
    void begin_direct_removal(int start_x, int start_y, RemovalStyle style, bool is_discard);
    void resolve_removal_play_effects();
    void complete_removal_fx();
    bool tick_removal_fx();
    CardFlightSample sample_removal_flight(int main_x, int dest_x, int dest_y) const;
    int hand_removal_shift() const;
    bool hand_removal_gap_layout() const;
    int visual_hand_slot_count() const;
    int hand_index_for_visual_slot(int visual_index) const;
    int visual_slot_for_hand_index(int hand_index) const;
    int active_removal_slot_index() const;
    int layout_hand_count() const;
    int hand_layout_center_count() const;
    void deal_opening_hand();
    void draw_total_score();
    void draw_round_score();
    void show_total_score_value(int value);
    void show_round_score_running(int running, int end_multiplier);
    void finalize_total_score_display();
    void finalize_round_score_display();
    void tick_score_wiggles();
    int current_highlight_type() const;
    int current_highlight_index() const;
    void draw_inspect(CardType type);
    void clear_inspect();
    void snap_active_scroll();
    void prepare_hand_selection_mode();
    void clamp_hand_cursor();
    void begin_graveyard_card_fx(GraveyardExilePickKind pick_kind);
    void hand_slot_screen_position(int hand_index, int main_x, int& out_x, int& out_y) const;
    int graveyard_card_fx_frame_count() const;
    void complete_graveyard_card_fx();
    void begin_deck_search_resolve(CardRef played, int start_x, int start_y, bool swivel_follow = false);
    void tick_deck_search_resolve();
    void finish_deck_search_resolve();
    bool deck_search_resolve_active() const;
    bool hand_draw_fx_blocking() const;
    int scheduled_hand_count() const;
    void try_start_hand_draw_fx();
    void tick_hand_draw_fx();
    void complete_hand_draw_fx();
    bool presentation_fx_blocking() const;
    bool try_drain_echo_replay();
    void tick_echo_pending();
    void shift_card_raise_after_remove(int removed_index);
    bool confirm_pressed() const;

    void begin_panel_transition(PanelTransition transition);
    void tick_panel();
    void complete_panel_transition();
    bool panel_transition_active() const;
    bool show_details_layer() const;
    bool show_graveyard_layer() const; // right-side card browse: GY or exile
    bool graveyard_pick_active() const;
    bool card_selection_ui_active() const;
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
    void begin_next_pending_or_finish();
    void finish_empty_hand_round();
    void tick_round_end_pending();
    void arm_echo_replay(CardRef played);
    // Re-scan hand/GY and start a pending combo if any. True = do not end the round yet.
    bool block_round_end_for_combo();

    void tick_combo();
    void finish_combo_cinematic();
    bool try_start_pending_combo();
    bool poll_direction(int& current_direction, bool scrolling, bool& direction_triggered,
                        int& direction_steps);
    void handle_input();
    void handle_input_inspect_toggle();
    void handle_input_side_panel(int current_direction, bool direction_triggered, int direction_steps,
                                 bool row_scrolling);
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
