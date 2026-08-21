#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "game_types.h"

void move_toward_int(int& value, int target, int max_step);
void move_toward_raise(int& value, int target);

// First index visible in a sliding window centered on `cursor`. Shared by
// horizontal card rows and vertical menu/deck-editor lists (same math).
int first_visible_index(int cursor, int count, int window);

// Block until no buttons are held, so a scene transition doesn't inherit the
// confirming press from the previous screen (e.g. A on "Build Deck" opening
// "+ New Deck" immediately).
void wait_for_keypad_clear();

enum class DirectionAxis
{
    HORIZONTAL,
    VERTICAL,
};

struct DirectionRepeatState
{
    int held_direction = 0;
    int held_direction_frames = 0; // frames since last fire / direction change
    int held_total_frames = 0;     // frames since this direction was engaged
    bool direction_initial_delay = true;
};

// D-pad repeat with hold acceleration (shared by hand, GY, menus, deck editor).
// Sets out_direction to -1/0/1, out_steps to how many slots to move when triggered.
bool poll_direction_repeat(DirectionAxis axis, DirectionRepeatState& state, bool scrolling,
                           int& out_direction, bool& out_triggered, int& out_steps);

// Lightweight text pool for menu / deck-editor scenes (not battle HUD).
class SceneText
{
public:
    explicit SceneText(bn::sprite_text_generator& generator);

    void clear();
    void set_visible(bool visible);
    void set_z_order(int z);
    void set_bg_priority(int priority);
    void draw_centered_line(int y, const bn::string_view& text);
    void draw_centered_line(int x, int y, const bn::string_view& text);
    void draw_left_line(int x, int y, const bn::string_view& text);

private:
    bn::sprite_text_generator& _generator;
    bn::vector<bn::sprite_ptr, 128> _sprites;
};

class SelectorGlyph
{
public:
    explicit SelectorGlyph(bn::sprite_text_generator& generator, int anchor_x = -100);

    void set_position(int y);
    void set_position(int x, int y);
    void set_visible(bool visible);

private:
    int _anchor_x;
    bn::vector<bn::sprite_ptr, 1> _sprites;
};

#endif
