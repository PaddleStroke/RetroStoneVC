/*
 * Pyramid in RetroStone Board, for one player (the rules: pyramid_rules.c, pyramid.h; the table, the finger, the
 * hint and the win: solitaire_ui.h, its pairs). The pyramid of 28 cards; under it (beside it, the screen held
 * sideways) the stock, the turned card, the waste, and the pile of the cards taken off. A tap on a card lifts it, a
 * tap on another one pairs them (a king goes off on one tap); a card can also be dragged onto its pair. A tap on the
 * stock turns a card (an empty stock, marked by an arrow going round, turns the waste over while passes are left;
 * three passes: the passes left in digits under the stock). The bar: the moves, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/pyramid.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &py_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1, .pairs = 1};
static int dealing;
static float passes_x, passes_y;                /* where the passes left are written */

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    int wide = b.w > 1.25f * b.h;
    float w = floorf(wide ? fminf(b.w / 11.2f, b.h / 6.4f) : fminf(b.w / 7.9f, b.h / 8.7f));
    float h = SOLUI_ASPECT * w, pitch = fminf(1.1f * w, (b.w - w) / 6), vs = 0.55f * h, cx = b.x + b.w / 2;
    float tall = 6 * vs + h + (wide ? 0 : 0.5f * w + h + 0.6f * w);
    float top = b.y + (b.h - tall) / 2 + h / 2;
    U.w = w;
    for (int i = 0; i < PY_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->fan = i < PY_STOCK ? SF_SLOT : SF_PILE;
        if (i < PY_STOCK) {
            s->x = cx + (py_slots[i].col - py_slots[i].row / 2.0f) * pitch;
            s->y = top + py_slots[i].row * vs;
        } else if (!wide) {                     /* a row under the pyramid: stock, turned card, waste; off right */
            int col = i == PY_STOCK ? 0 : i == PY_TURN ? 1 : i == PY_WASTE ? 2 : 6;
            s->x = cx + (col - 3) * pitch;
            s->y = top + 6 * vs + h + 0.5f * w;
        } else {                                /* a column on the left: stock, turned card, waste; off right */
            float side = 3 * pitch + 1.55f * w;
            int row = i == PY_STOCK ? 0 : i == PY_TURN ? 1 : i == PY_WASTE ? 2 : 0;
            s->x = i == PY_OUT ? cx + side : cx - side;
            s->y = top + row * (6 * vs) / 2;
        }
    }
    passes_x = U.at[PY_STOCK].x;
    passes_y = wide ? U.at[PY_STOCK].y + 0.5f * h + 0.42f * w : U.at[PY_STOCK].y + 0.5f * h + 0.4f * w;
}

static void start(int v)
{
    py_new(&G, v, (uint32_t)ui_random(1 << 30));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return py_won(&G.pos) ? GS_WIN0 : py_stuck(&G.pos) ? GS_WIN1 : GS_PLAY; }
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
    int ok = py_script(&G, s);
    solui_calm(&U);
    dealing = 0;
    solui_check_win(&U, ui_ms());
    return ok;
}

static void draw(void)
{
    const solpos *p = &G.pos;
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[PY_STOCK].x, U.at[PY_STOCK].y, 30);
    }
    if (!p->n[PY_STOCK] && py_rules.stock(p).kind != SM_NONE) solui_again(&U, U.at[PY_STOCK].x, U.at[PY_STOCK].y);
    if (p->variant == PY_THRICE && !U.won) ui_number(passes_x, passes_y, 0.36f * U.w, py_passes(p), 1);
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
    solui_token(size, rgba, CARD(SUIT_HEARTS, RANK_QUEEN), CARD(SUIT_SPADES, RANK_ACE));
}

/* a small pyramid of three rows (a queen and an ace lifted, a pair), the stock under it: one back, or three
 * fanned (three passes) */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.22f, vs = 0.17f, top = 0.17f;
    static const int cards[6] = {CARD(SUIT_CLUBS, 9), CARD(SUIT_DIAMONDS, 5), CARD(SUIT_SPADES, 8),
                                 CARD(SUIT_HEARTS, RANK_QUEEN), CARD(SUIT_CLUBS, 4), CARD(SUIT_SPADES, RANK_ACE)};
    for (int r = 0, k = 0; r < 3; r++)
        for (int c = 0; c <= r; c++, k++) {
            int lift = k == 3 || k == 5 ? (int)(0.03f * size) : 0;
            solui_paint(rgba, size, DECK_FRENCH, cards[k], (0.5f + (c - r / 2.0f) * 0.24f) * size,
                        (top + r * vs) * size - lift, (int)(w * size));
        }
    int backs = v == PY_THRICE ? 3 : 1;
    for (int k = 0; k < backs; k++)
        solui_paint(rgba, size, DECK_FRENCH, CARD_NONE, (0.17f + 0.1f * k) * size, 0.83f * size, (int)(w * size));
    solui_paint(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_KING), 0.8f * size, 0.83f * size, (int)(w * size));
}

static const game pyramid_game = {
    .id = "pyramid", .order = 345, .variants = PY_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(pyramid_game)
