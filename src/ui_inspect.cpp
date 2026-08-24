#include "ui_inspect.h"

#include "bn_string.h"
#include "card_data.h"
#include "game_state.h"

namespace
{
    const char* upgrade_title(PrizeOfferKind kind)
    {
        switch(kind)
        {
        case PrizeOfferKind::UPGRADE_PLUS_DIGIT:
            return "+Digit upgrade";
        case PrizeOfferKind::UPGRADE_INCREMENT_MULT:
            return "x2 upgrade";
        case PrizeOfferKind::UPGRADE_LEAD:
            return "Lead upgrade";
        case PrizeOfferKind::UPGRADE_YEAST:
            return "Yeast upgrade";
        default:
            return "Upgrade";
        }
    }

    const char* upgrade_body(PrizeOfferKind kind)
    {
        switch(kind)
        {
        case PrizeOfferKind::UPGRADE_PLUS_DIGIT:
            return "Pick a card. A random digit 1-9 is concatenated onto its +N.";
        case PrizeOfferKind::UPGRADE_INCREMENT_MULT:
            return "Pick a card with no built-in multiply. It gains x2 on play.";
        case PrizeOfferKind::UPGRADE_LEAD:
            return "Pick a card. It gains Lead gravity in your deck.";
        case PrizeOfferKind::UPGRADE_YEAST:
            return "Pick a card. It gains Yeast gravity in your deck.";
        default:
            return "Campaign upgrade prize.";
        }
    }

    const char* trinket_title(TrinketType type)
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
            return "Get With The Times";
        case TrinketType::PRIME_TIME:
            return "Prime Time";
        case TrinketType::FIBONACCI:
            return "Fibonacci";
        default:
            return "Trinket";
        }
    }

    const char* trinket_body(TrinketType type)
    {
        switch(type)
        {
        case TrinketType::MOREL:
            return "When a card adds to round score, add +2 more after reactions settle.";
        case TrinketType::LUCKY_SEVENS:
            return "When score contains 7, roll 7-13 and add that amount.";
        case TrinketType::ECHO:
            return "Echoes the first card you play with a play effect each round.";
        case TrinketType::GET_WITH_THE_TIMES:
            return "Card adds do nothing; gain +6 at the start of each round instead.";
        case TrinketType::PRIME_TIME:
            return "When round or total score are prime numbers, add n where n is the number of times your round or total score have been a prime number.";
        case TrinketType::FIBONACCI:
            return "At each round start, add the next value in 1, 1, 2, 3, 5, 8...";
        default:
            return "Global modifier for your active deck.";
        }
    }
}

const char* upgrade_inspect_title(PrizeOfferKind kind)
{
    return upgrade_title(kind);
}

const char* trinket_inspect_title(TrinketType type)
{
    return trinket_title(type);
}

void elevate_inspect_sprites(bn::vector<bn::sprite_ptr, 64>& sprites)
{
    for(bn::sprite_ptr& sprite : sprites)
    {
        sprite.set_z_order(-10);
    }
}

void show_inspect_card(Card& card, CardType type, const CardInstance* instance,
                       bn::sprite_text_generator* pip_generator)
{
    card.clear_visual();
    card.set_type(type);
    card.set_position(inspect_layout::CARD_X, inspect_layout::CARD_Y);
    card.set_blending_enabled(false);
    card.set_upgrade_pips(pip_generator, instance);
    card.set_inspect_visual(2);
    card.set_visible(true);
}

void hide_inspect_card(Card& card)
{
    release_card_display_tiles(card);
}

void draw_text_inspect(const char* title, const char* body,
                       bn::sprite_text_generator& title_generator,
                       bn::sprite_text_generator& body_generator,
                       bn::vector<bn::sprite_ptr, 64>& out_sprites, int title_y)
{
    out_sprites.clear();

    title_generator.set_left_alignment();
    title_generator.generate(inspect_layout::TEXT_X, title_y, title, out_sprites);

    body_generator.set_left_alignment();
    generate_wrapped_text(body_generator, inspect_layout::TEXT_X, title_y + 20,
                          inspect_layout::BODY_LINE_HEIGHT, inspect_layout::BODY_MAX_CHARS, body,
                          out_sprites);

    elevate_inspect_sprites(out_sprites);
}

void generate_wrapped_text(bn::sprite_text_generator& generator, int x, int y, int line_height,
                           int max_chars, const char* text, bn::vector<bn::sprite_ptr, 64>& out)
{
    int length = 0;

    while(text[length] != '\0')
    {
        ++length;
    }

    int cur_y = y;
    int line_start = 0;

    while(line_start < length)
    {
        while(line_start < length && text[line_start] == ' ')
        {
            ++line_start;
        }

        if(line_start >= length)
        {
            break;
        }

        int line_end = line_start;

        for(int i = line_start; i <= length; ++i)
        {
            if(i == length || text[i] == ' ')
            {
                if(i - line_start <= max_chars)
                {
                    line_end = i;
                }
                else
                {
                    break;
                }
            }
        }

        if(line_end == line_start)
        {
            line_end = line_start + max_chars < length ? line_start + max_chars : length;
        }

        generator.generate(x, cur_y, bn::string_view(text + line_start, line_end - line_start), out);
        cur_y += line_height;
        line_start = line_end;
    }
}

void draw_card_inspect(CardType type,
                       bn::sprite_text_generator& title_generator,
                       bn::sprite_text_generator& body_generator,
                       bn::vector<bn::sprite_ptr, 64>& out_sprites,
                       int title_y,
                       const CardInstance* instance)
{
    out_sprites.clear();
    const CardData& data = card_data(type);

    bn::string<40> title = data.name;

    if(instance)
    {
        bn::string<32> suffix;
        format_instance_upgrade_suffix(*instance, suffix);
        title.append(suffix);
    }

    title_generator.set_left_alignment();
    title_generator.generate(inspect_layout::TEXT_X, title_y, title, out_sprites);

    body_generator.set_left_alignment();
    generate_wrapped_text(body_generator, inspect_layout::TEXT_X, title_y + 20,
                          inspect_layout::BODY_LINE_HEIGHT, inspect_layout::BODY_MAX_CHARS,
                          data.description, out_sprites);

    if(instance && (instance->plus_digit || instance->increment_mult ||
                    instance->gravity != Gravity::NONE))
    {
        bn::string<40> upgrade_line = "Upgrades:";
        bn::string<32> suffix;
        format_instance_upgrade_suffix(*instance, suffix);
        upgrade_line.append(suffix);
        body_generator.generate(inspect_layout::TEXT_X, title_y + 64, upgrade_line, out_sprites);
    }

    elevate_inspect_sprites(out_sprites);
}

void draw_upgrade_inspect(PrizeOfferKind kind,
                          bn::sprite_text_generator& title_generator,
                          bn::sprite_text_generator& body_generator,
                          bn::vector<bn::sprite_ptr, 64>& out_sprites, int title_y)
{
    draw_text_inspect(upgrade_title(kind), upgrade_body(kind), title_generator, body_generator, out_sprites,
                      title_y);
}

void draw_trinket_inspect(TrinketType type,
                          bn::sprite_text_generator& title_generator,
                          bn::sprite_text_generator& body_generator,
                          bn::vector<bn::sprite_ptr, 64>& out_sprites, int title_y)
{
    draw_text_inspect(trinket_title(type), trinket_body(type), title_generator, body_generator, out_sprites,
                      title_y);
}

void draw_prize_offer_inspect(const PrizeOffer& offer,
                              bn::sprite_text_generator& title_generator,
                              bn::sprite_text_generator& body_generator,
                              bn::vector<bn::sprite_ptr, 64>& out_sprites, int title_y,
                              const CardInstance* instance)
{
    switch(offer.kind)
    {
    case PrizeOfferKind::CARD:
        draw_card_inspect(offer.card, title_generator, body_generator, out_sprites, title_y, instance);
        break;

    case PrizeOfferKind::TRINKET:
        draw_trinket_inspect(static_cast<TrinketType>(offer.trinket), title_generator, body_generator,
                             out_sprites, title_y);
        break;

    default:
        draw_upgrade_inspect(offer.kind, title_generator, body_generator, out_sprites, title_y);
        break;
    }
}
