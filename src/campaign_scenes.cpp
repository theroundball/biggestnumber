#include "campaign_scenes.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_span.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "battle_backdrop.h"
#include "campaign.h"
#include "menu_scenes.h"
#include "poker_hand.h"
#include "prize_row_scene.h"
#include "prize_system.h"
#include "save_data.h"
#include "common_variable_8x16_sprite_font.h"
#include "library_grid_pick_scene.h"
#include "ui_common.h"

namespace
{
    constexpr int LIST_LINE_HEIGHT = 16;
    constexpr int LIST_START_Y = -20;
    constexpr int STATUS_TITLE_Y = -68;
    constexpr int STATUS_START_Y = -48;
}

MenuSceneResult run_campaign_starter_pick_scene(CardType& out_utility)
{
    PrizeOffer offers[3] = {
        {PrizeOfferKind::CARD, CardType::FISHING_POLE, 0},
        {PrizeOfferKind::CARD, CardType::JACKS, 0},
        {PrizeOfferKind::CARD, CardType::SHELLS, 0},
    };

    const PrizeRowResult pick =
        run_prize_row_scene("Pick a utility card", offers, 3, "A pick  Select info");

    if(!pick.picked)
    {
        return MenuSceneResult::MAIN_MENU;
    }

    out_utility = pick.offer.card;
    return MenuSceneResult::STAY;
}

CampaignPlayMenuResult run_campaign_play_menu_scene(bn::seed_random& rng)
{
    wait_for_keypad_clear();

    CampaignPlayMenuResult result;
    SaveData& save = save_data_mut();
    campaign_prepare_same_number_target(save, rng);

    constexpr int ITEM_COUNT = 8;
    constexpr int VISIBLE_ROWS = 5;
    bn::string<32> labels[ITEM_COUNT];
    labels[0] = "Change / build deck";
    labels[1] = "Biggest Number";
    labels[3] = "Number Now";
    labels[4] = "Ain't Got Time";
    labels[5] = "Sharing is Caring";
    labels[6] = "Poker Hand";

    auto refresh_same_number_label = [&]()
    {
        bn::string<32> same_line = "Same Number (";
        same_line.append(bn::to_string<8>(save_data_get().same_number_target));
        same_line.append(")");
        labels[2] = same_line;
    };

    refresh_same_number_label();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(text_generator, -100);

    int cursor = 0;
    int scroll_offset = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        MenuSceneResult status_result;

        if(campaign_poll_lr_status(status_result, scene_text, selector))
        {
            if(status_result == MenuSceneResult::SELL_COLLECTION)
            {
                bn::seed_random sell_rng(bn::core::current_cpu_ticks() | 1u);
                run_campaign_sell_collection_flow(sell_rng);
                campaign_prepare_same_number_target(save_data_mut(), rng);
            }

            refresh_same_number_label();
            continue;
        }

        if(cursor < scroll_offset)
        {
            scroll_offset = cursor;
        }
        else if(cursor >= scroll_offset + VISIBLE_ROWS)
        {
            scroll_offset = cursor - VISIBLE_ROWS + 1;
        }

        scene_text.clear();
        scene_text.draw_centered_line(-56, "Play Game");

        for(int row = 0; row < VISIBLE_ROWS; ++row)
        {
            const int index = scroll_offset + row;

            if(index >= ITEM_COUNT)
            {
                break;
            }

            scene_text.draw_centered_line(LIST_START_Y + row * LIST_LINE_HEIGHT, labels[index]);
        }

        selector.set_position(LIST_START_Y + (cursor - scroll_offset) * LIST_LINE_HEIGHT);
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= ITEM_COUNT)
            {
                cursor = ITEM_COUNT - 1;
            }
        }

        if(bn::keypad::a_pressed())
        {
            if(cursor == 0)
            {
                result.next = MenuSceneResult::DECK_LIST_BUILD;
                return result;
            }

            switch(cursor)
            {
            case 1:
                result.mode = CampaignMode::BIGGEST_NUMBER;
                break;
            case 2:
                result.mode = CampaignMode::SAME_NUMBER;
                break;
            case 3:
                result.mode = CampaignMode::NUMBER_NOW;
                break;
            case 4:
                result.mode = CampaignMode::AINT_GOT_TIME;
                break;
            case 5:
                result.mode = CampaignMode::SHARING_IS_CARING;
                break;
            default:
                result.mode = CampaignMode::POKER_HAND;
                break;
            }

            result.next = MenuSceneResult::RUN_GAME;
            return result;
        }

        if(bn::keypad::b_pressed())
        {
            result.next = MenuSceneResult::MAIN_MENU;
            return result;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_campaign_battle_results_scene(CampaignMode mode, const GameSceneResult& result,
                                                  bool won, int same_number_target, bool& out_to_prize,
                                                  bool granted_sticker_paper)
{
    wait_for_keypad_clear();

    out_to_prize = false;

    bn::sprite_text_generator label_generator(common::variable_8x16_sprite_font);
    bn::vector<bn::sprite_ptr, 24> sprites;

    while(true)
    {
        sprites.clear();

        label_generator.set_center_alignment();
        label_generator.generate(0, -56, won ? "Victory" : "Defeat", sprites);

        if(granted_sticker_paper)
        {
            label_generator.generate(0, -44, "+1 sticker paper", sprites);
        }

        bn::string<32> score_line = "Score ";
        score_line.append(bn::to_string<12>(result.final_score));
        label_generator.generate(0, -28, score_line, sprites);

        if(mode == CampaignMode::SAME_NUMBER)
        {
            bn::string<32> target_line = "Target ";
            target_line.append(bn::to_string<8>(same_number_target));
            label_generator.generate(0, -12, target_line, sprites);
        }
        else if(mode == CampaignMode::POKER_HAND && result.poker_hand_score > 0)
        {
            const PokerHandRank rank = PokerHandRank(result.poker_hand_rank);
            bn::string<32> poker_line = poker_hand_rank_name(rank);
            poker_line.append("! Score ");
            poker_line.append(bn::to_string<8>(result.poker_hand_score));
            label_generator.generate(0, -12, poker_line, sprites);
        }

        const char* action = won ? "Pick a prize" : "Menu";
        label_generator.generate(0, 24, action, sprites);
        label_generator.set_left_alignment();

        if(bn::keypad::a_pressed() || bn::keypad::b_pressed() || bn::keypad::start_pressed() ||
           bn::keypad::select_pressed() || bn::keypad::up_pressed() || bn::keypad::down_pressed() ||
           bn::keypad::left_pressed() || bn::keypad::right_pressed())
        {
            out_to_prize = won;
            return MenuSceneResult::STAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_campaign_prize_scene(CampaignMode mode, int peak_before, int band_score,
                                         bn::seed_random& rng)
{
    SaveData& save = save_data_mut();

    PrizeOffer offers[CAMPAIGN_PRIZE_SLOT_COUNT];
    prize_build_offers(save, mode, peak_before, band_score, rng, offers);

    const PrizeRowResult pick =
        run_prize_row_scene("Choose a prize", offers, CAMPAIGN_PRIZE_SLOT_COUNT, "A pick  Select info");

    if(!pick.picked)
    {
        return MenuSceneResult::MAIN_MENU;
    }

    if(pick.offer.kind == PrizeOfferKind::CARD)
    {
        campaign_apply_prize_card(save, pick.offer.card);
        return MenuSceneResult::STAY;
    }

    uint8_t instance_id = NO_INSTANCE;

    if(run_upgrade_target_scene(save, pick.offer.kind, rng, instance_id))
    {
        campaign_apply_prize_upgrade(save, pick.offer.kind, instance_id, rng);
    }

    return MenuSceneResult::STAY;
}

MenuSceneResult run_campaign_trinket_prize_scene(bn::seed_random& rng)
{
    SaveData& save = save_data_mut();
    const TrinketType granted = prize_roll_trinket(save, rng);

    if(granted == TrinketType::NONE)
    {
        return MenuSceneResult::STAY;
    }

    PrizeOffer offer;
    offer.kind = PrizeOfferKind::TRINKET;
    offer.trinket = uint8_t(granted);

    const PrizeRowResult pick =
        run_prize_row_scene("Trinket unlocked!", &offer, 1, "A collect  Select info");

    if(pick.picked)
    {
        campaign_apply_prize_trinket(save, granted);
    }

    return MenuSceneResult::STAY;
}

namespace
{
    MenuSceneResult run_campaign_nostalgia_pick_scene(CardType& out_card)
    {
        const SaveData& save = save_data_get();
        bn::vector<CardType, int(CardType::COUNT)> owned;

        for(int type_index = 0; type_index < int(CardType::COUNT); ++type_index)
        {
            const CardType type = CardType(type_index);

            if(library_total_owned(save, type) > 0)
            {
                owned.push_back(type);
            }
        }

        if(owned.empty())
        {
            return MenuSceneResult::MAIN_MENU;
        }

        const LibraryGridPickResult pick = run_library_grid_pick_scene(
            "Keep one card", "A keep  Select info",
            bn::span<const CardType>(owned.data(), owned.size()));

        if(!pick.picked)
        {
            return MenuSceneResult::MAIN_MENU;
        }

        out_card = pick.card;
        return MenuSceneResult::STAY;
    }

    bool run_campaign_sell_confirm_scene()
    {
        wait_for_keypad_clear();

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        SceneText scene_text(text_generator);

        while(true)
        {
            scene_text.clear();
            scene_text.draw_centered_line(-48, "Sell collection?");
            scene_text.draw_centered_line(-28, "Lose all progress");
            scene_text.draw_centered_line(-12, "Keep 1 card + wheels");
            scene_text.draw_centered_line(16, "A sell  B cancel");

            if(bn::keypad::a_pressed())
            {
                return true;
            }

            if(bn::keypad::b_pressed())
            {
                return false;
            }

            battle_backdrop_tick();
            bn::core::update();
        }
    }
}

MenuSceneResult run_campaign_status_scene()
{
    wait_for_keypad_clear();

    const SaveData& save = save_data_get();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(text_generator, -100);

    int cursor = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        scene_text.clear();
        scene_text.draw_centered_line(STATUS_TITLE_Y, "Campaign Status");

        bn::string<32> wins_line = "Wins ";
        wins_line.append(bn::to_string<8>(save.total_wins));
        scene_text.draw_centered_line(STATUS_START_Y, wins_line);

        bn::string<32> paper_line = "Sticker paper ";
        paper_line.append(bn::to_string<8>(save.sticker_paper));
        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT, paper_line);

        bn::string<32> trinket_line = "Trinket in ";
        trinket_line.append(bn::to_string<4>(campaign_wins_until_trinket(save)));
        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT * 2, trinket_line);

        bn::string<32> record_line = "BN record ";
        record_line.append(bn::to_string<12>(save.biggest_number_record));
        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT * 3, record_line);

        bn::string<32> same_line = "SN ";
        same_line.append(bn::to_string<8>(save.same_number_wins));
        same_line.append(" wins  N=");
        same_line.append(bn::to_string<8>(save.same_number_target));
        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT * 4, same_line);

        bn::string<32> library_line = "Library ";
        library_line.append(bn::to_string<8>(campaign_library_total_cards(save)));
        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT * 5, library_line);

        scene_text.draw_centered_line(STATUS_START_Y + LIST_LINE_HEIGHT * 6, "Sell Collection");

        selector.set_position(STATUS_START_Y + cursor * LIST_LINE_HEIGHT);
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= 7)
            {
                cursor = 6;
            }
        }

        if(bn::keypad::a_pressed() && cursor == 6)
        {
            return MenuSceneResult::SELL_COLLECTION;
        }

        // R returns to the menu that opened status (L opened this screen).
        if(bn::keypad::r_pressed() || bn::keypad::b_pressed())
        {
            return MenuSceneResult::STAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_campaign_sell_collection_flow(bn::seed_random& rng)
{
    (void)rng;

    if(!run_campaign_sell_confirm_scene())
    {
        return MenuSceneResult::STAY;
    }

    CardType nostalgia = CardType::LONGBOARD;
    CardType utility = CardType::JACKS;

    if(run_campaign_nostalgia_pick_scene(nostalgia) == MenuSceneResult::MAIN_MENU)
    {
        return MenuSceneResult::STAY;
    }

    if(run_campaign_starter_pick_scene(utility) == MenuSceneResult::MAIN_MENU)
    {
        return MenuSceneResult::STAY;
    }

    if(!campaign_apply_sell_collection(save_data_mut(), nostalgia, utility))
    {
        return MenuSceneResult::STAY;
    }

    return MenuSceneResult::STAY;
}

bool campaign_poll_lr_status(MenuSceneResult& out_result, SceneText& host_text, SelectorGlyph& host_selector)
{
    if(!campaign_is_ready(save_data_get()))
    {
        return false;
    }

    // L opens campaign status; R on that screen returns here.
    if(bn::keypad::l_pressed())
    {
        host_text.clear();
        host_selector.set_visible(false);
        out_result = run_campaign_status_scene();
        return true;
    }

    return false;
}

namespace
{
    const char* mode_intro_title(CampaignMode mode)
    {
        switch(mode)
        {
        case CampaignMode::BIGGEST_NUMBER:
            return "Biggest Number";
        case CampaignMode::SAME_NUMBER:
            return "Same Number";
        case CampaignMode::NUMBER_NOW:
            return "Number Now";
        case CampaignMode::AINT_GOT_TIME:
            return "Ain't Got Time";
        case CampaignMode::SHARING_IS_CARING:
            return "Sharing is Caring";
        case CampaignMode::POKER_HAND:
            return "Poker Hand";
        default:
            return "Campaign";
        }
    }

    void draw_mode_intro_lines(SceneText& scene_text, CampaignMode mode, const CampaignUiContext& ctx)
    {
        scene_text.draw_centered_line(-56, mode_intro_title(mode));

        switch(mode)
        {
        case CampaignMode::BIGGEST_NUMBER:
            scene_text.draw_centered_line(-32, "Beat your record");
            break;
        case CampaignMode::SAME_NUMBER:
            scene_text.draw_centered_line(-32, "Match the target");
            break;
        case CampaignMode::NUMBER_NOW:
            scene_text.draw_centered_line(-32, "Score on one round");
            break;
        case CampaignMode::AINT_GOT_TIME:
            scene_text.draw_centered_line(-32, "3 rounds only");
            break;
        case CampaignMode::SHARING_IS_CARING:
            scene_text.draw_centered_line(-32, "x5 mult fades per play");
            break;
        case CampaignMode::POKER_HAND:
            scene_text.draw_centered_line(-32, "Build best poker hand");
            break;
        default:
            break;
        }

        bn::string<32> detail_line;

        switch(mode)
        {
        case CampaignMode::BIGGEST_NUMBER:
            detail_line = "Record ";
            detail_line.append(bn::to_string<12>(ctx.biggest_number_record));
            break;
        case CampaignMode::SAME_NUMBER:
            detail_line = "Target ";
            detail_line.append(bn::to_string<8>(ctx.same_number_target));
            break;
        case CampaignMode::NUMBER_NOW:
            detail_line = "Round ";
            detail_line.append(bn::to_string<4>(ctx.number_now_scoring_round));
            detail_line.append(" best ");
            detail_line.append(bn::to_string<12>(ctx.number_now_round_peak));
            break;
        case CampaignMode::AINT_GOT_TIME:
            detail_line = "Record ";
            detail_line.append(bn::to_string<12>(ctx.aint_got_time_record));
            break;
        case CampaignMode::SHARING_IS_CARING:
            detail_line = "Record ";
            detail_line.append(bn::to_string<12>(ctx.sharing_is_caring_record));
            break;
        case CampaignMode::POKER_HAND:
            detail_line = "Beat a hand record";
            break;
        default:
            detail_line = "";
            break;
        }

        if(!detail_line.empty())
        {
            scene_text.draw_centered_line(-12, detail_line);
        }

        scene_text.draw_centered_line(24, "A start  B menu");
    }
}

MenuSceneResult run_mode_intro_scene(CampaignMode mode, const CampaignUiContext& ctx)
{
    wait_for_keypad_clear();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);

    while(true)
    {
        scene_text.clear();
        draw_mode_intro_lines(scene_text, mode, ctx);

        if(bn::keypad::a_pressed())
        {
            return MenuSceneResult::STAY;
        }

        if(bn::keypad::b_pressed())
        {
            return MenuSceneResult::MAIN_MENU;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}

MenuSceneResult run_same_number_deck_scene()
{
    const SaveData& save = save_data_get();

    if(save.active_deck_index < 0 || save.active_deck_index >= save.deck_count)
    {
        return MenuSceneResult::MAIN_MENU;
    }

    const DeckEditorResult edit = run_deck_editor_scene(save.active_deck_index, false, true);

    if(edit.next == MenuSceneResult::MAIN_MENU || !edit.ephemeral_confirmed)
    {
        return MenuSceneResult::MAIN_MENU;
    }

    return MenuSceneResult::STAY;
}

MenuSceneResult run_campaign_shop_scene(bn::seed_random& rng)
{
    wait_for_keypad_clear();

    SaveData& save = save_data_mut();

    constexpr int ITEM_COUNT = 5;
    constexpr PrizeOfferKind UPGRADE_KINDS[4] = {
        PrizeOfferKind::UPGRADE_PLUS_DIGIT,
        PrizeOfferKind::UPGRADE_INCREMENT_MULT,
        PrizeOfferKind::UPGRADE_LEAD,
        PrizeOfferKind::UPGRADE_YEAST,
    };
    const char* UPGRADE_LABELS[4] = {
        "+Digit upgrade",
        "Increment mult",
        "Lead upgrade",
        "Yeast upgrade",
    };

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    SceneText scene_text(text_generator);
    SelectorGlyph selector(text_generator, -100);

    int cursor = 0;
    DirectionRepeatState direction_repeat;

    while(true)
    {
        MenuSceneResult status_result;

        if(campaign_poll_lr_status(status_result, scene_text, selector))
        {
            if(status_result == MenuSceneResult::SELL_COLLECTION)
            {
                bn::seed_random sell_rng(bn::core::current_cpu_ticks() | 1u);
                run_campaign_sell_collection_flow(sell_rng);
            }

            continue;
        }

        scene_text.clear();

        bn::string<32> title = "Shop (";
        title.append(bn::to_string<8>(save.sticker_paper));
        title.append(" paper)");
        scene_text.draw_centered_line(-56, title);

        for(int index = 0; index < 4; ++index)
        {
            bn::string<32> line = UPGRADE_LABELS[index];
            line.append(" - ");
            line.append(bn::to_string<4>(CAMPAIGN_STICKER_PAPER_UPGRADE_COST));
            scene_text.draw_centered_line(LIST_START_Y + index * LIST_LINE_HEIGHT, line);
        }

        scene_text.draw_centered_line(LIST_START_Y + 4 * LIST_LINE_HEIGHT, "Back");

        selector.set_position(LIST_START_Y + cursor * LIST_LINE_HEIGHT);
        selector.set_visible(true);

        int direction = 0;
        bool direction_triggered = false;
        int direction_steps = 1;
        poll_direction_repeat(DirectionAxis::VERTICAL, direction_repeat, false, direction, direction_triggered,
                              direction_steps);

        if(direction_triggered)
        {
            cursor += direction * direction_steps;

            if(cursor < 0)
            {
                cursor = 0;
            }
            else if(cursor >= ITEM_COUNT)
            {
                cursor = ITEM_COUNT - 1;
            }
        }

        if(bn::keypad::a_pressed())
        {
            if(cursor == 4)
            {
                return MenuSceneResult::STAY;
            }

            if(int(save.sticker_paper) < CAMPAIGN_STICKER_PAPER_UPGRADE_COST)
            {
                continue;
            }

            uint8_t instance_id = NO_INSTANCE;

            if(run_upgrade_target_scene(save, UPGRADE_KINDS[cursor], rng, instance_id) &&
               campaign_spend_sticker_paper(save, CAMPAIGN_STICKER_PAPER_UPGRADE_COST))
            {
                campaign_apply_prize_upgrade(save, UPGRADE_KINDS[cursor], instance_id, rng);
            }
        }

        if(bn::keypad::b_pressed())
        {
            return MenuSceneResult::STAY;
        }

        battle_backdrop_tick();
        bn::core::update();
    }
}
