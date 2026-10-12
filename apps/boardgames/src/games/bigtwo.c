/*
 * Big Two in RetroStone Board: the Cantonese climbing game for 4 (or 3) passing the device round (the rules:
 * bigtwo_rules.c, bigtwo.h, games/shed.h; the table: games/shed_ui.h, shared with Tiến lên). The hand from the 3 of
 * diamonds up to the 2 of spades; the 3 of diamonds glows for the player who must lead it. At the end, the cards each
 * one has left and the penalty.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/bigtwo.h"
#include "games/shed_ui.h"

static void start(int v)
{
    (void)v;
    shui_start(&bigt_rules, SHUI_PENALTY, bigt_penalty);
}

/* the menu's card: the 3 of diamonds, and the 2s of hearts and spades on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[3] = {CARD(SUIT_DIAMONDS, 3), CARD(SUIT_HEARTS, 2), CARD(SUIT_SPADES, 2)};
    ct_thumb(cards, 3, size, rgba);
}

static const game bigtwo_game = {
    .id = "bigtwo", .order = 315, .variants = 1,
    .thumb = thumb, .token = shui_token, .start = start, .layout = shui_layout, .draw = shui_draw,
    .press = shui_press, .motion = shui_motion, .release = shui_release, .undo = shui_undo, .turn = shui_turn,
    .status = shui_status, .bar = shui_bar, .can_pass = shui_can_pass, .pass = shui_pass, .script = shui_script,
    .busy = shui_busy, .winners = shui_winners,
    .players = 1 << 3 | 1 << 4, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(bigtwo_game)
