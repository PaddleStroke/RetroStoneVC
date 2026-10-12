/*
 * TriPeaks in RetroStone Board, for one player (the rules: tripeaks_rules.c, tripeaks.h; the table, the finger, the
 * hint and the win: solitaire_ui.h). The three peaks of 28 cards; under them the stock and the waste, the streak in
 * digits beside the waste (in gold light from three cards in a row). A tap sends a free card onto the waste when it
 * follows (a drag too); a tap on the stock turns its next card. The bar: the moves, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/tripeaks.h"                     /* first: it sets the piles' count (solitaire.h) */
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &tp_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    float w = floorf(fminf(b.w / 10.7f, b.h / 6.4f)), h = SOLUI_ASPECT * w;
    float pitch = fminf(1.06f * w, (b.w - w) / 9), vs = 0.5f * h, cx = b.x + b.w / 2;
    float gap = fminf(1.2f * w, fmaxf(0.6f * w, (b.h - 3 * vs - 2 * h) / 3));
    float tall = 3 * vs + h + gap + h, top = b.y + (b.h - tall) / 2 + h / 2;
    U.w = w;
    for (int i = 0; i < TP_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->fan = i < TP_STOCK ? SF_SLOT : SF_PILE;
        if (i < TP_STOCK) {
            s->x = cx + (tp_slot_x[i] - 4.5f) * pitch;
            s->y = top + tp_slots[i].row * vs;
        } else {
            s->x = cx + (i == TP_STOCK ? -0.7f : 0.7f) * w;
            s->y = top + 3 * vs + h + gap;
        }
    }
}

static void start(int v)
{
    tp_new(&G, v, (uint32_t)ui_random(1 << 30));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return tp_won(&G.pos) ? GS_WIN0 : tp_stuck(&G.pos) ? GS_WIN1 : GS_PLAY; }
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
    int ok = tp_script(&G, s);
    solui_calm(&U);
    dealing = 0;
    solui_check_win(&U, ui_ms());
    return ok;
}

/* the deal: the peaks' cards fly from the stock, then its first card turns onto the waste */
static void deal(void)
{
    const solspot *s = &U.at[TP_STOCK], *t = &U.at[TP_WASTE];
    const int gap = 26, delay = TP_SLOTS * gap + 120;
    solui_deal(&U, s->x, s->y, gap);
    solui_slide(&U, sol_top(&G.pos, TP_WASTE), 0, s->x, s->y, 1, t->x, t->y, 260, delay, TP_WASTE, 0, (Uint32)delay);
    ui_sound_later(SND_MOVE, delay + 220);
}

/* the streak: digits beside the waste, in gold light from three on */
static void draw_streak(void)
{
    int k = tp_streak(&G);
    if (!k || U.won) return;
    const solspot *t = &U.at[TP_WASTE];
    float x = t->x + 1.3f * U.w, y = t->y, nh = 0.5f * U.w;
    if (k >= 3) ui_mark_gold(x, y, 1.5f * U.w);
    ui_number(x, y, nh, k, 1);
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        deal();
    }
    draw_streak();
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
    solui_token(size, rgba, CARD(SUIT_DIAMONDS, 7), CARD(SUIT_CLUBS, 8));
}

/* three small peaks (a back on top, two cards under it), the stock and the waste under them; round the corner (the
 * classic rule): a king on the waste and an arrow going round */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.135f;
    static const int low[6] = {CARD(SUIT_HEARTS, 9), CARD(SUIT_SPADES, 4), CARD(SUIT_CLUBS, RANK_QUEEN),
                               CARD(SUIT_DIAMONDS, 2), CARD(SUIT_SPADES, RANK_ACE), CARD(SUIT_HEARTS, 6)};
    for (int p = 0; p < 3; p++) {
        float px = 0.2f + 0.3f * p;
        solui_paint(rgba, size, DECK_FRENCH, CARD_NONE, px * size, 0.27f * size, (int)(w * size));
        for (int k = 0; k < 2; k++)
            solui_paint(rgba, size, DECK_FRENCH, low[2 * p + k], (px + (k ? 0.071f : -0.071f)) * size, 0.43f * size,
                        (int)(w * size));
    }
    solui_paint(rgba, size, DECK_FRENCH, CARD_NONE, 0.39f * size, 0.76f * size, (int)(w * size));
    solui_paint(rgba, size, DECK_FRENCH, v == TP_NOWRAP ? CARD(SUIT_CLUBS, 8) : CARD(SUIT_CLUBS, RANK_KING),
                0.58f * size, 0.76f * size, (int)(w * size));
    if (v == TP_NOWRAP) return;
    int d = (int)(0.2f * size);
    uint8_t *a = malloc((size_t)d * d * 4);
    if (!a) return;
    solui_again_pic(d, a);
    for (int i = 0; i < d * d; i++)             /* cream, a little faint */
        a[4 * i] = 246, a[4 * i + 1] = 238, a[4 * i + 2] = 220, a[4 * i + 3] = (uint8_t)(a[4 * i + 3] * 0.85f);
    art_blend(rgba, size, size, a, d, d, (int)(0.78f * size - d / 2.0f), (int)(0.76f * size - d / 2.0f));
    free(a);
}

static const game tripeaks_game = {
    .id = "tripeaks", .order = 346, .variants = TP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(tripeaks_game)
