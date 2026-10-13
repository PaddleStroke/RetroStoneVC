/*
 * Forty Thieves in RetroStone Board, for one player (the rules: fortythieves_rules.c, fortythieves.h; the table, the
 * finger, the hint and the win: solitaire_ui.h). The stock, the waste and the eight foundations in a row at the top,
 * the ten columns under them, the cards narrow enough for a phone held upright. A tap sends a top card to its best
 * place (a foundation first, then the next card of its suit, then an empty column), a quick second tap only to a
 * foundation; a drag carries it where it is dropped; a tap on the stock turns a card. The bar: the moves; the hint is
 * the shell's tool.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/fortythieves.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &ft_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.12f,
                  .up_step = 0.42f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 10, 2.5f, 16);
    float w = t.w, gap = fminf(0.25f * w, (b.w - 1.1f * w - 9 * t.pitch) / 2);   /* the stock and the waste a little
                                                                                 * apart */
    for (int i = 0; i < FT_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        int col = i < FT_FOUND ? i : i < FT_STOCK ? i - FT_FOUND + 2 : i - FT_STOCK;
        s->x = t.x0 + col * t.pitch + (i >= FT_FOUND ? (col < 2 ? -gap : gap) : 0);
        s->y = i < FT_FOUND ? t.cy : t.ty;
        s->fan = i < FT_FOUND ? SF_COLUMN : i == FT_WASTE ? SF_WASTE : SF_PILE;
        s->room = t.room;
    }
}

static void start(int v)
{
    (void)v;
    ft_new(&G, (uint32_t)ui_random(1 << 30));
    solui_new(&U);
    dealing = 1;
}

static int status(void) { return ft_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, ft_script(&G, s));
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[FT_STOCK].x, U.at[FT_STOCK].y, 20);
    }
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
    solui_token(size, rgba, CARD(SUIT_DIAMONDS, 10), CARD(SUIT_DIAMONDS, 9));
}

/* two aces of a suit side by side (two packs), the stock; four columns of four cards under them, built in suit */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.2f, top = 0.17f;
    static const int cols[4][4] = {
        {CARD(SUIT_CLUBS, 4), CARD(SUIT_HEARTS, 11), CARD(SUIT_SPADES, 9), CARD(SUIT_SPADES, 8)},
        {CARD(SUIT_DIAMONDS, 12), CARD(SUIT_CLUBS, 2), CARD(SUIT_HEARTS, 7), CARD(SUIT_HEARTS, 6)},
        {CARD(SUIT_SPADES, 13), CARD(SUIT_DIAMONDS, 5), CARD(SUIT_CLUBS, 10), CARD(SUIT_CLUBS, 9)},
        {CARD(SUIT_HEARTS, 3), CARD(SUIT_SPADES, 12), CARD(SUIT_DIAMONDS, 8), CARD(SUIT_DIAMONDS, 7)}};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD_NONE, 0.13f * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), 0.62f * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), 0.86f * size, top * size, (int)(w * size));
    for (int c = 0; c < 4; c++)
        solui_paint_column(rgba, size, DECK_FRENCH, 0.13f + 0.245f * c, 0.47f, w, 0, cols[c], 4, 0, 0.11f);
}

static const game fortythieves_game = {
    .id = "fortythieves", .order = 343, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
};
REGISTER_GAME(fortythieves_game)
