/*
 * Tiến lên in RetroStone Board: the Vietnamese climbing game for 2 to 4 passing the device round (the rules:
 * tienlen_rules.c, tienlen.h, games/shed.h; the table: games/shed_ui.h, shared with Big Two). The hand from the 3 of
 * spades up to the 2 of hearts; the lowest card dealt glows for the player who must lead it. A player who passes
 * keeps the pass sign (out of the trick) until the next lead. Once out, each player's place shows by their chip; at
 * the end, the places.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/shed_ui.h"
#include "games/tienlen.h"

static void start(int v)
{
    (void)v;
    shui_start(&tln_rules, SHUI_PLACES, NULL);
}

/* the menu's card: a 2 of hearts chopped by four 9s */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[5] = {CARD(SUIT_HEARTS, 2), CARD(SUIT_SPADES, 9), CARD(SUIT_CLUBS, 9),
                                 CARD(SUIT_DIAMONDS, 9), CARD(SUIT_HEARTS, 9)};
    ct_thumb(cards, 5, size, rgba);
}

static const game tienlen_game = {
    .id = "tienlen", .order = 316, .variants = 1,
    .thumb = thumb, .token = shui_token, .start = start, .layout = shui_layout, .draw = shui_draw,
    .press = shui_press, .motion = shui_motion, .release = shui_release, .undo = shui_undo, .turn = shui_turn,
    .status = shui_status, .bar = shui_bar, .can_pass = shui_can_pass, .pass = shui_pass, .script = shui_script,
    .busy = shui_busy, .winners = shui_winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(tienlen_game)
