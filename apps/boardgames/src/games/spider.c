/*
 * Spider in RetroStone Board, for one player (the rules: spider_rules.c, spider.h; the table, the finger, the hint
 * and the win: solitaire_ui.h). The eight foundations in a row at the top (each complete run shows its king), the
 * stock at the right end (a back for each row still to deal), the ten columns under them, the cards narrow enough
 * for a phone held upright and overlapping. A tap sends a run to its best place (onto its own suit first), a drag
 * carries it where it is dropped; a tap on the stock deals a row. The bar: the moves, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/solitaire_ui.h"
#include "games/spider.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &sp_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.12f,
                  .up_step = 0.44f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    float w = floorf(fminf(b.w / 10.9f, b.h / 7.0f)), pitch = fminf(1.12f * w, (b.w - w) / 9);
    float x0 = b.x + b.w / 2 - 4.5f * pitch, ty = b.y + 0.06f * w + 0.7f * w, cy = ty + 1.58f * w;
    U.w = w;
    for (int i = 0; i < SPI_PILES; i++) {
        solspot *s = &U.at[i];
        int col = i < SPI_FOUND ? i : i < SPI_STOCK ? i - SPI_FOUND : 9;
        s->x = x0 + col * pitch;
        s->y = i < SPI_FOUND ? cy : ty;
        s->fan = i < SPI_FOUND ? SF_COLUMN : i < SPI_STOCK ? SF_PILE : SF_ROW;
        s->first = i >= SPI_FOUND && i < SPI_STOCK;
        s->room = b.y + b.h - 0.72f * w - cy;
    }
}

static void start(int v)
{
    sp_new(&G, v, (uint32_t)ui_random(1 << 30));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return sp_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }

static int undo(void)
{
    if (!sol_undo(&G)) return 0;
    solui_calm(&U);
    U.won = 0;
    dealing = 0;
    ui_handoff(0);
    return 1;
}

static int script(const char *s)
{
    int ok = sp_script(&G, s);
    solui_calm(&U);
    dealing = 0;
    solui_check_win(&U, ui_ms());
    return ok;
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[SPI_STOCK].x, U.at[SPI_STOCK].y, 22);
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

static int bar_press(int player, float x, float y)
{
    (void)player;
    return solui_bar_press(&U, x, y);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD(SUIT_SPADES, RANK_KING), CARD(SUIT_SPADES, RANK_QUEEN));
}

/* three columns of runs in the variant's suits (the menu's: two suits), the stock's backs above */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int suits = v == SP_1SUIT ? 1 : v == SP_4SUITS ? 4 : 2;
    static const int order[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};
    const float w = 0.25f;
    for (int k = 0; k < 3; k++)
        solui_paint(rgba, size, DECK_FRENCH, CARD_NONE, (0.62f + 0.07f * k) * size, 0.2f * size, (int)(w * size));
    solui_paint(rgba, size, DECK_FRENCH, CARD(SUIT_SPADES, RANK_KING), 0.17f * size, 0.2f * size, (int)(w * size));
    for (int c = 0; c < 3; c++) {
        int cards[4], n = 2;
        for (int i = 0; i < n; i++) cards[i] = CARD(order[c % suits], 11 - c - i);
        solui_paint_column(rgba, size, DECK_FRENCH, 0.17f + 0.33f * c, 0.6f, w, c, cards, n, 0.03f, 0.17f);
    }
}

static const game spider_game = {
    .id = "spider", .order = 340, .variants = SP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(spider_game)
