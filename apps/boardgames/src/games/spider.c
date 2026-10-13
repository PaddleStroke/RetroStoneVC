/*
 * Spider in RetroStone Board, for one player (the rules: spider_rules.c, spider.h; the table, the finger, the hint
 * and the win: solitaire_ui.h). The eight foundations in a row at the top (each complete run shows its king), the
 * stock at the right end (a back for each row still to deal), the ten columns under them, the cards narrow enough
 * for a phone held upright and overlapping. A tap sends a run to its best place (onto its own suit first), a drag
 * carries it where it is dropped; a tap on the stock deals a row. The bar: the moves; the hint is the shell's tool.
 * Levels: 150 deals of our own in six chapters (spider_levels.h, made and proved winnable by tools/spider_gen.c), the
 * level screen's die for a deal at random; the stars count the hints and undos (solitaire_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/solitaire_ui.h"
#include "games/spider.h"
#include "games/spider_levels.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &sp_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.12f,
                  .up_step = 0.44f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 10, 2.8f, 20);
    for (int i = 0; i < SPI_PILES; i++) {
        solspot *s = &U.at[i];
        int col = i < SPI_FOUND ? i : i < SPI_STOCK ? i - SPI_FOUND : 9;
        s->x = t.x0 + col * t.pitch;
        s->y = i < SPI_FOUND ? t.cy : t.ty;
        s->fan = i < SPI_FOUND ? SF_COLUMN : i < SPI_STOCK ? SF_PILE : SF_ROW;
        s->first = i >= SPI_FOUND && i < SPI_STOCK;
        s->room = t.room;
    }
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
}

static void start(int level)                    /* a designed level (spider_levels.h) */
{
    uint8_t deck[104];
    int v = solui_level(sp_levels, SPI_LEVELS, level, deck, 104);
    sp_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    sp_new(&G, v, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void) { return sp_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }

static int undo(void) { dealing = 0; return solui_undo(&U); }

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

static int level_stars(void) { return solui_stars(&U); }
static int score(void) { return G.moves; }

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
        cards_paint_at(rgba, size, DECK_FRENCH, CARD_NONE, (0.62f + 0.07f * k) * size, 0.2f * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_SPADES, RANK_KING), 0.17f * size, 0.2f * size, (int)(w * size));
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
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = SPI_LEVELS, .chapters = SPI_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(spider_game)
