#ifndef UI_INSPECT_H
#define UI_INSPECT_H

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "card.h"
#include "card_instance.h"
#include "card_type.h"
#include "campaign_types.h"
#include "game_state.h"

namespace inspect_layout
{
    // Card on the left, description column on the right.
    constexpr int CARD_X = -100;
    constexpr int CARD_Y = -28;
    constexpr int TEXT_X = -34;
    constexpr int TITLE_Y = -48;
    constexpr int BODY_LINE_HEIGHT = 11;
    constexpr int BODY_MAX_CHARS = 19;
}

const char* upgrade_inspect_title(PrizeOfferKind kind);
const char* trinket_inspect_title(TrinketType type);

void generate_wrapped_text(bn::sprite_text_generator& generator, int x, int y, int line_height,
                           int max_chars, const char* text, bn::vector<bn::sprite_ptr, 64>& out);

void elevate_inspect_sprites(bn::vector<bn::sprite_ptr, 64>& sprites);

void show_inspect_card(Card& card, CardType type, const CardInstance* instance = nullptr,
                       bn::sprite_text_generator* pip_generator = nullptr);
void hide_inspect_card(Card& card);

void draw_text_inspect(const char* title, const char* body,
                       bn::sprite_text_generator& title_generator,
                       bn::sprite_text_generator& body_generator,
                       bn::vector<bn::sprite_ptr, 64>& out_sprites,
                       int title_y = inspect_layout::TITLE_Y);

void draw_card_inspect(CardType type,
                       bn::sprite_text_generator& title_generator,
                       bn::sprite_text_generator& body_generator,
                       bn::vector<bn::sprite_ptr, 64>& out_sprites,
                       int title_y = inspect_layout::TITLE_Y,
                       const CardInstance* instance = nullptr);

void draw_upgrade_inspect(PrizeOfferKind kind,
                          bn::sprite_text_generator& title_generator,
                          bn::sprite_text_generator& body_generator,
                          bn::vector<bn::sprite_ptr, 64>& out_sprites,
                          int title_y = inspect_layout::TITLE_Y);

void draw_trinket_inspect(TrinketType type,
                          bn::sprite_text_generator& title_generator,
                          bn::sprite_text_generator& body_generator,
                          bn::vector<bn::sprite_ptr, 64>& out_sprites,
                          int title_y = inspect_layout::TITLE_Y);

void draw_prize_offer_inspect(const PrizeOffer& offer,
                              bn::sprite_text_generator& title_generator,
                              bn::sprite_text_generator& body_generator,
                              bn::vector<bn::sprite_ptr, 64>& out_sprites,
                              int title_y = inspect_layout::TITLE_Y,
                              const CardInstance* instance = nullptr);

#endif
