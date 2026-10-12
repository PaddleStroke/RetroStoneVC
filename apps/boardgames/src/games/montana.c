/*
 * Montana in RetroStone Board, for one player (the rules: montana_rules.c, montana.h; the table, the finger, the hint
 * and the win: solitaire_ui.h). The four rows of thirteen places; under them (beside them, the screen held sideways)
 * the hand: an arrow going round while a redeal is left, the redeals left in digits under it (a tap there redeals,
 * each card flying to its new place). A tap on a card sends it to the gap it fills; a tap on a gap brings the card
 * that fills it; a card can also be dragged to its gap. The rows' runs in order lie in a soft gold light. The bar: the
 * moves, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/montana.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static int animate(solui *u, const solpos *before, int first);

static solrec G;
static solui U = {.R = &mt_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1, .animate = animate};
static int dealing;
static float pitch, row_h;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    int wide = b.w > b.h;
    float w = floorf(wide ? fminf(b.w / 15.4f, b.h / 6.5f) : fminf(b.w / 13.4f, b.h / 8.6f)), h = SOLUI_ASPECT * w;
    pitch = fminf(1.03f * w, (b.w - w - (wide ? 2.2f * w : 0)) / 12);
    row_h = 1.12f * h;
    float grid_w = 12 * pitch + w, grid_h = 3 * row_h + h, tall = grid_h + (wide ? 0 : 0.5f * w + h + 0.5f * w);
    float x0 = b.x + (b.w - grid_w - (wide ? 2.2f * w : 0)) / 2 + w / 2, y0 = b.y + (b.h - tall) / 2 + h / 2;
    U.w = w;
    for (int i = 0; i < MT_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->fan = SF_PILE;
        if (i < MT_HAND) {
            s->x = x0 + (i % 13) * pitch;
            s->y = y0 + (i / 13) * row_h;
        } else if (wide) {
            s->x = x0 + grid_w + 1.2f * w;
            s->y = y0 + 1.5f * row_h;
        } else {
            s->x = b.x + b.w / 2;
            s->y = y0 + grid_h - h / 2 + 0.5f * w + h / 2;
        }
    }
}

/* a redeal: each card flies to its new place */
static int animate(solui *u, const solpos *before, int first)
{
    for (int m = first; m < u->rec->n; m++)
        if (SM_KIND(u->rec->mv[m].kind) == SM_REDEAL) return solui_rearrange(u, before, 14);
    return 0;
}

static void start(int v)
{
    (void)v;
    mt_new(&G, (uint32_t)ui_random(1 << 30));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return mt_won(&G.pos) ? GS_WIN0 : mt_stuck(&G.pos) ? GS_WIN1 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, mt_script(&G, s));
}

static void draw(void)
{
    const solpos *p = &G.pos;
    const solspot *hs = &U.at[MT_HAND];
    if (dealing) {
        dealing = 0;
        solui_deal_kinds(&U, hs->x, hs->y, 14, 1u << SP_FOUND);
    }
    if (!U.won) {
        for (int r = 0; r < 4; r++) {           /* the runs in order: a soft gold light under them */
            int k = mt_run(p, r);
            for (int c = 0; c < k; c++)
                if (!solui_hidden(&U, 13 * r + c, 0)) ui_mark_last(U.at[13 * r + c].x - 0.56f * U.w,
                                                                  U.at[13 * r + c].y - 0.56f * SOLUI_ASPECT * U.w,
                                                                  1.12f * U.w, 1.12f * SOLUI_ASPECT * U.w);
        }
        if (mt_redeals_left(p)) {
            solui_again(&U, hs->x, hs->y);
            ui_number(hs->x, hs->y + 0.5f * SOLUI_ASPECT * U.w + 0.3f * U.w, 0.32f * U.w, mt_redeals_left(p), 1);
        }
    }
    solui_draw(&U);
}

static void press(float x, float y) { solui_press(&U, x, y); }
static void motion(float x, float y) { solui_motion(&U, x, y); }

/* a tap on a gap: the card that fills it comes; the rest as usual */
static void release(float x, float y)
{
    int gap = U.pile;
    if (!U.drag && gap >= 0 && gap < MT_HAND && U.idx < 0 && !U.won) {
        U.pile = -1;
        solmove m = mt_fill(&G.pos, gap);
        if (m.kind == SM_NONE || !solui_play(&U, m, NULL)) solui_deny();
        return;
    }
    solui_release(&U, x, y);
}

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
    solui_token(size, rgba, CARD(SUIT_CLUBS, 2), CARD(SUIT_CLUBS, 3));
}

/* three rows: a run 2, 3, 4 of a suit then a gap; cards out of order with a gap among them */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.2f;
    static const int cards[3][4] = {
        {CARD(SUIT_HEARTS, 2), CARD(SUIT_HEARTS, 3), CARD(SUIT_HEARTS, 4), CARD_NONE},
        {CARD(SUIT_SPADES, 9), CARD_NONE, CARD(SUIT_DIAMONDS, 6), CARD(SUIT_CLUBS, 12)},
        {CARD(SUIT_CLUBS, 5), CARD(SUIT_DIAMONDS, 11), CARD(SUIT_SPADES, 7), CARD(SUIT_HEARTS, 10)}};
    int pw = (int)(w * size), ph = (int)(pw * SOLUI_ASPECT);
    uint8_t *place = malloc((size_t)pw * ph * 4);
    const uint8_t edge[4] = {246, 238, 220, 70};
    if (place) art_roundrect(pw, ph, 0.075f * pw, edge, place);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) {
            float x = (0.14f + 0.24f * c) * size, y = (0.2f + 0.3f * r) * size;
            if (cards[r][c] != CARD_NONE) cards_paint_at(rgba, size, DECK_FRENCH, cards[r][c], x, y, pw);
            else if (place) art_blend(rgba, size, size, place, pw, ph, (int)(x - pw / 2.0f), (int)(y - ph / 2.0f));
        }
    free(place);
}

static const game montana_game = {
    .id = "montana", .order = 348, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(montana_game)
