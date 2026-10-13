/*
 * Klondike in RetroStone Board, for one player (the rules: klondike_rules.c, klondike.h; the table, the finger, the
 * hint and the win: solitaire_ui.h). The stock, the waste (draw three: its last three cards fanned) and the four
 * foundations in a row at the top, the seven columns under them. A tap sends a card or a run to its best place (a
 * foundation first), a quick second tap only to a foundation; a drag carries a run where it is dropped; a tap on
 * the stock draws (an empty stock, marked by an arrow going round, takes the waste back). The bar: the moves; the hint
 * is the shell's tool.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/klondike.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &kl_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 7, 2.4f, 16);
    for (int i = 0; i < KL_PILES; i++) {
        solspot *s = &U.at[i];
        int col = i < KL_FOUND ? i : i < KL_STOCK ? i - KL_FOUND + 3 : i == KL_STOCK ? 0 : 1;
        s->x = t.x0 + col * t.pitch;
        s->y = i < KL_FOUND ? t.cy : t.ty;
        s->fan = i < KL_FOUND ? SF_COLUMN : i == KL_WASTE ? SF_WASTE : SF_PILE;
        s->room = t.room;
        s->first = 0;
    }
}

static void start(int v)
{
    kl_new(&G, v, (uint32_t)ui_random(1 << 30));
    solui_new(&U);
    U.waste_fan = G.pos.draw;
    dealing = 1;
}

static int status(void) { return kl_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }

static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    int ok = kl_script(&G, s);
    solui_calm(&U);
    dealing = 0;
    solui_check_win(&U, ui_ms());
    return ok;
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[KL_STOCK].x, U.at[KL_STOCK].y, 40);
    }
    /* the empty stock: an arrow going round (tap: the waste comes back, turned over) */
    if (!G.pos.n[KL_STOCK] && G.pos.n[KL_WASTE]) solui_again(&U, U.at[KL_STOCK].x, U.at[KL_STOCK].y);
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
    solui_token(size, rgba, CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_SPADES, RANK_ACE));
}

/* the stock, the waste (one card, or three fanned), an ace up; three columns under them */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.25f, top = 0.2f;
    static const int c0[] = {CARD(SUIT_SPADES, 13), CARD(SUIT_HEARTS, 12)},
                     c1[] = {CARD(SUIT_DIAMONDS, 9), CARD(SUIT_SPADES, 8)},
                     c2[] = {CARD(SUIT_HEARTS, 5), CARD(SUIT_SPADES, 4)};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD_NONE, 0.17f * size, top * size, (int)(w * size));
    if (v == KL_DRAW3) {
        static const int waste[] = {CARD(SUIT_CLUBS, 2), CARD(SUIT_SPADES, 10), CARD(SUIT_DIAMONDS, 7)};
        for (int i = 0; i < 3; i++) cards_paint_at(rgba, size, DECK_FRENCH, waste[i], (0.4f + 0.07f * i) * size,
                                                top * size, (int)(w * size));
    } else {
        cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_DIAMONDS, 7), 0.45f * size, top * size, (int)(w * size));
    }
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), 0.83f * size, top * size, (int)(w * size));
    solui_paint_column(rgba, size, DECK_FRENCH, 0.17f, 0.6f, w, 0, c0, 2, 0, 0.17f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.5f, 0.6f, w, 2, c1, 2, 0.03f, 0.17f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.83f, 0.6f, w, 1, c2, 2, 0.03f, 0.17f);
}

static const game klondike_game = {
    .id = "klondike", .order = 320, .variants = KL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
};
REGISTER_GAME(klondike_game)
