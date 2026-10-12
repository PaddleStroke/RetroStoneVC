/*
 * La Belle Lucie in RetroStone Board, for one player (the rules: bellelucie_rules.c, bellelucie.h; the table, the
 * finger, the hint and the win: solitaire_ui.h). At the top the hand (a tap there redeals: an arrow going round, the
 * redeals left in digits low in its place), the merci's place (shown on the last deal only, in gold light while the
 * merci is open) and the four foundations; under them the eighteen fans, three across (six with the screen held
 * sideways),
 * each fanned to the right and closing up when it grows long. A tap sends a top card to its best place (a foundation
 * first, then the fan it follows), a quick second tap only to a foundation; a drag carries it where it is dropped; on
 * the last deal, a tap on a buried card draws it out (the merci). A redeal and the merci fly each card to its new
 * place. The bar: the moves, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/bellelucie.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static int animate(solui *u, const solpos *before, int first);

static solrec G;
static solui U = {.R = &lb_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .fan_step = 0.32f, .pile = -1, .made_bar = -1, .animate = animate};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    int wide = b.w > b.h, cols = wide ? 6 : 3, rows = LB_FANS / cols;
    float cw = b.w / cols, w = floorf(fminf(cw / 2.05f, b.h / ((rows + 1) * 1.4f * 1.13f + 0.45f)));
    float h = SOLUI_ASPECT * w, rh = fminf(1.3f * h, (b.h - 0.3f * w - h) / rows), pitch = 1.16f * w;
    float ty = b.y + 0.12f * w + h / 2, fy = ty + h / 2 + 0.25f * w + (b.h - 0.12f * w - h - 0.25f * w - rows * rh) / 2;
    U.w = w;
    for (int i = 0; i < LB_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        if (i < LB_FOUND) {                     /* the fans: row by row */
            int r = i / cols, c = i % cols;
            s->fan = SF_FAN;
            s->x = b.x + c * cw + 0.12f * w + w / 2;
            s->y = fy + r * rh + rh / 2;
            s->room = cw - 1.24f * w;
            s->bare = 1;
        } else {                                /* the top row: the hand, the merci, a gap, the foundations */
            int slot = i < LB_MERCI ? i - LB_FOUND + 3 : i == LB_MERCI ? 1 : 0;
            float x0 = b.x + b.w / 2 - 3 * pitch - 0.1f * w;
            s->fan = i == LB_MERCI ? SF_SLOT : SF_PILE;
            s->x = x0 + slot * pitch + (slot >= 3 ? 0.2f * w : 0);
            s->y = ty;
        }
    }
}

/* a redeal and the merci: each card flies to its new place; the other moves as usual */
static int animate(solui *u, const solpos *before, int first)
{
    for (int m = first; m < u->rec->n; m++)
        if (SM_KIND(u->rec->mv[m].kind) == SM_REDEAL) return solui_rearrange(u, before, before->redeals < 2 ? 16 : 70);
    return 0;
}

static void start(int v)
{
    (void)v;
    lb_new(&G, (uint32_t)ui_random(1 << 30));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return lb_won(&G.pos) ? GS_WIN0 : lb_stuck(&G.pos) ? GS_WIN1 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, lb_script(&G, s));
}

static void draw(void)
{
    const solpos *p = &G.pos;
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[LB_HAND].x, U.at[LB_HAND].y, 16);
    }
    if (lb_redeals_left(p) && !U.won) {         /* the hand: an arrow going round, the redeals left in it */
        solui_again(&U, U.at[LB_HAND].x, U.at[LB_HAND].y - 0.14f * U.w);
        ui_number(U.at[LB_HAND].x, U.at[LB_HAND].y + 0.4f * U.w, 0.3f * U.w, lb_redeals_left(p), 1);
    }
    const solspot *m = &U.at[LB_MERCI];
    if (lb_merci_open(p) && !p->n[LB_MERCI]) {  /* the merci's place, open: in gold light */
        ui_mark_gold(m->x, m->y, 1.25f * U.w);
        cards_place(U.deck, m->x, m->y, U.w);
    } else if (p->redeals > LB_REDEALS && !solui_hidden(&U, LB_MERCI, 0)) {
        cards_place(U.deck, m->x, m->y, U.w);   /* used: its place stays, empty or holding the card */
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
    solui_token(size, rgba, CARD(SUIT_HEARTS, 8), CARD(SUIT_HEARTS, 7));
}

/* four fans of three, fanned to the right, a foundation over them */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.22f;
    static const int fans[4][3] = {
        {CARD(SUIT_SPADES, 9), CARD(SUIT_HEARTS, 3), CARD(SUIT_CLUBS, 12)},
        {CARD(SUIT_DIAMONDS, 6), CARD(SUIT_CLUBS, 2), CARD(SUIT_HEARTS, 10)},
        {CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_SPADES, 5), CARD(SUIT_DIAMONDS, 11)},
        {CARD(SUIT_CLUBS, 8), CARD(SUIT_DIAMONDS, RANK_ACE), CARD(SUIT_SPADES, 4)}};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), 0.5f * size, 0.18f * size, (int)(w * size));
    for (int f = 0; f < 4; f++)
        for (int k = 0; k < 3; k++)
            cards_paint_at(rgba, size, DECK_FRENCH, fans[f][k], (0.14f + (f % 2) * 0.47f + k * 0.08f) * size,
                           (0.5f + (f / 2) * 0.3f) * size, (int)(w * size));
}

static const game bellelucie_game = {
    .id = "bellelucie", .order = 341, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(bellelucie_game)
