/*
 * Baker's Dozen in RetroStone Board, for one player (the rules: bakersdozen_rules.c, bakersdozen.h; the table, the
 * finger, the hint and the win: solitaire_ui.h). The four foundations at the top right; under them the thirteen
 * columns, in one row on a wide screen, or in two (seven, then six) on a phone held upright, whichever gives the
 * bigger cards. A tap sends a top card to its best place (a foundation first, then the column that buries the fewest
 * low cards), a quick second tap only to a foundation; a drag carries it where it is dropped. The bar: the moves; the
 * hint is the shell's tool.
 * Levels: 150 deals of our own in six chapters (bakersdozen_levels.h, made and proved winnable by
 * tools/bakersdozen_gen.c), the level screen's die for a deal at random; the stars count the hints and undos
 * (solitaire_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/bakersdozen.h"
#include "games/bakersdozen_levels.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &bkd_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.3f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
/* the two rows' card: seven across, the foundations' row, then two bands of columns each with room for 3 widths of
 * cards under its first one */
static float two_rows_w(box b) { return floorf(fminf(b.w / (6 * 1.06f + 1.1f), b.h / 10.6f)); }

static void layout(box b)
{
    U.b = b;
    U.up_step = U.base_up > 0 ? U.base_up : U.up_step;
    U.down_step = U.base_down > 0 ? U.base_down : U.down_step;
    float w2 = two_rows_w(b), w1 = floorf(fminf(b.w / (12 * 1.06f + 1.1f), b.h / (3.06f + 2.6f)));
    if (w1 >= w2) {                             /* one row of thirteen */
        soltable t = solui_table(&U, b, 13, 2.6f, 12);
        for (int i = 0; i < BKD_PILES; i++) {
            solspot *s = &U.at[i];
            memset(s, 0, sizeof *s);
            s->x = t.x0 + (i < BKD_FOUND ? i : i - BKD_FOUND + 9) * t.pitch;
            s->y = i < BKD_FOUND ? t.cy : t.ty;
            s->fan = i < BKD_FOUND ? SF_COLUMN : SF_PILE;
            s->room = t.room;
        }
        return;
    }
    if (U.base_up <= 0) U.base_up = U.up_step, U.base_down = U.down_step;
    float w = U.w = w2, pitch = fminf(1.16f * w, (b.w - 1.1f * w) / 6), x0 = b.x + b.w / 2 - 3 * pitch;
    float ty = b.y + 0.76f * w, top = ty + 0.9f * w, band = (b.y + b.h - top) / 2;
    float room = band - 1.55f * w;
    U.up_step = clampf(room / w / 7, U.base_up, 0.55f);
    for (int i = 0; i < BKD_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        if (i < BKD_FOUND) {
            int row = i < 7 ? 0 : 1, col = i < 7 ? i : i - 7;
            s->x = x0 + (col + 0.5f * row) * pitch;
            s->y = top + row * band + 0.78f * w;
            s->fan = SF_COLUMN;
            s->room = room;
        } else {
            s->x = x0 + (i - BKD_FOUND + 3) * pitch;
            s->y = ty;
            s->fan = SF_PILE;
        }
    }
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
}

static void start(int level)                    /* a designed level (bakersdozen_levels.h) */
{
    uint8_t deck[52];
    int v = solui_level(bkd_levels, BKD_LEVELS, level, deck, 52);
    bkd_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    (void)v;
    bkd_new(&G, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void) { return bkd_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, bkd_script(&G, s));
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[BKD_FOUND].x, U.at[BKD_FOUND].y, 16);
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
    solui_token(size, rgba, CARD(SUIT_CLUBS, 8), CARD(SUIT_HEARTS, 7));
}

/* two foundations started; four short columns, a king at the bottom of two of them, ranks down whatever the suit */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.2f, top = 0.16f;
    static const int cols[4][4] = {
        {CARD(SUIT_SPADES, RANK_KING), CARD(SUIT_DIAMONDS, 5), CARD(SUIT_CLUBS, 4), CARD(SUIT_HEARTS, 3)},
        {CARD(SUIT_HEARTS, 9), CARD(SUIT_SPADES, 2), CARD(SUIT_CLUBS, 11), CARD(SUIT_DIAMONDS, 10)},
        {CARD(SUIT_DIAMONDS, RANK_KING), CARD(SUIT_HEARTS, 12), CARD(SUIT_SPADES, 6), CARD(SUIT_CLUBS, 5)},
        {CARD(SUIT_CLUBS, 9), CARD(SUIT_HEARTS, 4), CARD(SUIT_DIAMONDS, 8), CARD(SUIT_SPADES, 7)}};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, 2), 0.62f * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_CLUBS, RANK_ACE), 0.86f * size, top * size, (int)(w * size));
    for (int c = 0; c < 4; c++)
        solui_paint_column(rgba, size, DECK_FRENCH, 0.13f + 0.245f * c, 0.44f, w, 0, cols[c], 4, 0, 0.12f);
}

static const game bakersdozen_game = {
    .id = "bakersdozen", .order = 343, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = BKD_LEVELS, .chapters = BKD_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(bakersdozen_game)
