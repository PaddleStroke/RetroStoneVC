/*
 * Canfield in RetroStone Board, for one player (the rules: canfield_rules.c, canfield.h; the table, the finger, the
 * hint and the win: solitaire_ui.h). The stock, the waste (its last three cards fanned) and the four foundations in a
 * row at the top; under the stock the reserve (its backs fanned down, its top card face up), under the foundations the
 * four tableau piles. A tap sends a card (or a whole pile, tapped at its first card) to its best place, a quick second
 * tap only to a foundation; a drag carries it where it is dropped; a tap on the stock turns three cards (an empty
 * stock, marked by an arrow going round, takes the waste back). The bar: the moves; the hint
 * is the shell's tool.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/canfield.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &cn_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 3, .down_step = 0.11f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 7, 2.4f, 15);
    for (int i = 0; i < CN_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        int col = i < CN_RESERVE ? i + 3 : i == CN_RESERVE ? 0 : i < CN_STOCK ? i - CN_FOUND + 3 : i - CN_STOCK;
        s->x = t.x0 + col * t.pitch;
        s->y = i <= CN_RESERVE ? t.cy : t.ty;
        s->fan = i <= CN_RESERVE ? SF_COLUMN : i == CN_WASTE ? SF_WASTE : SF_PILE;
        s->room = t.room;
    }
}

static void start(int v)
{
    (void)v;
    cn_new(&G, (uint32_t)ui_random(1 << 30));
    solui_new(&U);
    dealing = 1;
}

static int status(void) { return cn_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, cn_script(&G, s));
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal_kinds(&U, U.at[CN_STOCK].x, U.at[CN_STOCK].y, 26, 1u << SP_TAB | 1u << SP_FOUND);
    }
    if (!G.pos.n[CN_STOCK] && G.pos.n[CN_WASTE]) solui_again(&U, U.at[CN_STOCK].x, U.at[CN_STOCK].y);
    solui_draw(&U);
}

static void press(float x, float y) { solui_press(&U, x, y); }
static void motion(float x, float y) { solui_motion(&U, x, y); }
static void release(float x, float y) { solui_release(&U, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    solui_bar(&U, b, 0);
}

static int level_stars(void) { return solui_stars(&U); }
static int score(void) { return G.moves; }

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD(SUIT_SPADES, 7), CARD(SUIT_DIAMONDS, 7));
}

/* the reserve (backs fanned down, a card face up), two foundations started on the same rank (not an ace), a pile
 * built round the corner (a king on an ace) */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.25f, top = 0.18f;
    static const int res[] = {CARD(SUIT_HEARTS, 9)}, pile[] = {CARD(SUIT_CLUBS, 2), CARD(SUIT_HEARTS, RANK_ACE),
                                                                CARD(SUIT_SPADES, RANK_KING)};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_DIAMONDS, 7), 0.5f * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_SPADES, 7), 0.83f * size, top * size, (int)(w * size));
    solui_paint_column(rgba, size, DECK_FRENCH, 0.17f, 0.18f, w, 6, res, 1, 0.045f, 0);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.67f, 0.5f, w, 0, pile, 3, 0, 0.14f);
}

static const game canfield_game = {
    .id = "canfield", .order = 347, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
};
REGISTER_GAME(canfield_game)
