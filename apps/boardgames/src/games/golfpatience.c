/*
 * Golf, the patience, in RetroStone Board, for one player (the rules: golfpatience_rules.c, golfpatience.h; the
 * table, the finger, the hint and the win: solitaire_ui.h). The seven columns of five; under them the stock and the
 * waste, the score beside the waste (the cards left in the columns; once they are cleared, minus the stock's, in gold
 * light). A tap sends a column's top card onto the waste when it follows (a drag too); a tap on the stock turns its
 * next card. The bar: the moves; the hint is the shell's tool.
 * Levels: 150 deals of our own in six chapters (golfpatience_levels.h, made and proved winnable by
 * tools/golfpatience_gen.c), the level screen's die for a deal at random; the stars count the hints and undos
 * (solitaire_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/golfpatience.h"
#include "games/golfpatience_levels.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &pg_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.36f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    float w = floorf(fminf(b.w / 7.46f, b.h / 4.86f)), h = SOLUI_ASPECT * w;
    float pitch = fminf(1.16f * w, (b.w - 1.1f * w) / 6), x0 = b.x + b.w / 2 - 3 * pitch;
    /* the columns' step: as set, more on a tall screen (each card shown more), never closer: five cards always fit */
    U.up_step = clampf((b.h / w - 3.5f) / 4, 0.36f, 0.6f);
    float col_h = h + 4 * U.up_step * w, gap = fminf(1.4f * w, fmaxf(0.5f * w, (b.h - col_h - h) / 3));
    float top = b.y + (b.h - col_h - gap - h) / 2 + h / 2;
    U.w = w;
    for (int i = 0; i < PG_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->fan = i < PG_STOCK ? SF_COLUMN : SF_PILE;
        if (i < PG_STOCK) {
            s->x = x0 + i * pitch;
            s->y = top;
            s->room = col_h;
        } else {
            s->x = b.x + b.w / 2 + (i == PG_STOCK ? -0.7f : 0.7f) * w;
            s->y = top + col_h - h / 2 + gap + h / 2;
        }
    }
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
}

static void start(int level)                    /* a designed level (golfpatience_levels.h) */
{
    uint8_t deck[52];
    int v = solui_level(pg_levels, PG_LEVELS, level, deck, 52);
    pg_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    pg_new(&G, v, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void) { return pg_won(&G.pos) ? GS_WIN0 : pg_stuck(&G.pos) ? GS_WIN1 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, pg_script(&G, s));
}

/* the deal: the columns' cards fly from the stock, then its first card turns onto the waste */
static void deal(void)
{
    const solspot *s = &U.at[PG_STOCK], *t = &U.at[PG_WASTE];
    const int gap = 26, delay = 35 * gap + 120;
    solui_deal(&U, s->x, s->y, gap);
    solui_slide(&U, sol_top(&G.pos, PG_WASTE), 0, s->x, s->y, 1, t->x, t->y, 260, delay, PG_WASTE, 0, (Uint32)delay);
    ui_sound_later(SND_MOVE, delay + 220);
}

/* the score beside the waste: the cards left in the columns; cleared, minus the stock's, in gold light */
static void draw_score(void)
{
    const solspot *t = &U.at[PG_WASTE];
    int v = pg_score(&G.pos);
    float x = t->x + 1.35f * U.w, y = t->y;
    if (v <= 0) ui_mark_gold(x, y, 1.5f * U.w);
    ui_number_signed(x, y, 0.5f * U.w, v, 1, 0);
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        deal();
    }
    if (!U.won || ui_ms() < U.win_t0) draw_score();
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
    solui_token(size, rgba, CARD(SUIT_CLUBS, 5), CARD(SUIT_HEARTS, 6));
}

/* four columns of face-up cards; under them the stock and the waste: an eight going on a seven (round the corner: a
 * king on the waste and an arrow going round) */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.2f;
    static const int cols[4][3] = {
        {CARD(SUIT_SPADES, 3), CARD(SUIT_HEARTS, 10), CARD(SUIT_CLUBS, 6)},
        {CARD(SUIT_DIAMONDS, 12), CARD(SUIT_SPADES, 9), CARD(SUIT_HEARTS, 8)},
        {CARD(SUIT_CLUBS, 11), CARD(SUIT_DIAMONDS, 2), CARD(SUIT_SPADES, RANK_ACE)},
        {CARD(SUIT_HEARTS, 4), CARD(SUIT_CLUBS, 13), CARD(SUIT_DIAMONDS, 5)}};
    for (int c = 0; c < 4; c++)
        solui_paint_column(rgba, size, DECK_FRENCH, 0.14f + 0.24f * c, 0.17f, w, 0, cols[c], 3, 0, 0.1f);
    cards_paint_at(rgba, size, DECK_FRENCH, CARD_NONE, 0.36f * size, 0.78f * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, v == PG_WRAP ? CARD(SUIT_CLUBS, RANK_KING) : CARD(SUIT_CLUBS, 7),
                   0.58f * size, 0.78f * size, (int)(w * size));
    if (v == PG_WRAP) solui_paint_again(rgba, size, 0.8f * size, 0.78f * size, (int)(0.2f * size));
}

static const game golfpatience_game = {
    .id = "golfpatience", .order = 344, .variants = PG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = PG_LEVELS, .chapters = PG_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(golfpatience_game)
