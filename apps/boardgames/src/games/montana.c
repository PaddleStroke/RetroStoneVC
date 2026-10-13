/*
 * Montana in RetroStone Board, for one player (the rules: montana_rules.c, montana.h; the table, the finger, the hint
 * and the win: solitaire_ui.h). The four rows of thirteen places; under them (beside them, the screen held sideways)
 * the hand: an arrow going round while a redeal is left, the redeals left in digits under it (a tap there redeals,
 * each card flying to its new place). A tap on a card sends it to the gap it fills; a tap on a gap brings the card
 * that fills it; a card can also be dragged to its gap. The rows' runs in order lie in a soft gold light. The bar: the
 * moves; the hint is the shell's tool.
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
static float pitch, row_h;                    /* turned: the rows stand as columns (an upright screen) */
static int turned;

/* ---- the table --------------------------------------------------------------------------------------------------- */
/* a wide screen: the four rows across, the cards overlapping a little when the width is short (each card's index
 * still shows), the hand on the right. An upright one: the rows turned to stand as four columns down the screen, each
 * card lying over the one before it like a patience's column, the hand on their right (or under them). */
static void layout(box b)
{
    U.b = b;
    float w, h, x0, y0, hx, hy;
    turned = b.h > b.w;
    if (!turned) {
        w = floorf(fminf(b.w / (12 * 0.8f + 1.1f + 1.9f), b.h / (3 * 1.1f * SOLUI_ASPECT + SOLUI_ASPECT + 0.2f)));
        h = SOLUI_ASPECT * w;
        pitch = clampf((b.w - 3.0f * w) / 12, 0.8f * w, 1.04f * w);
        row_h = fminf(1.12f * h, (b.h - 0.2f * w - h) / 3);
        float grid_w = 12 * pitch + w;
        x0 = b.x + (b.w - grid_w - 1.9f * w) / 2 + w / 2;
        y0 = b.y + (b.h - 3 * row_h - h) / 2 + h / 2;
        hx = x0 + grid_w + 1.4f * w - w / 2;
        hy = y0 + 1.5f * row_h;
    } else {                                    /* the hand on the right, or under the columns: the bigger cards */
        float wr = fminf(b.w / (3 * 1.12f + 1.1f + 1.4f), b.h / (12 * 0.5f * SOLUI_ASPECT + SOLUI_ASPECT + 0.2f));
        float wb = fminf(b.w / (3 * 1.12f + 1.1f), b.h / (12 * 0.5f * SOLUI_ASPECT + 2 * SOLUI_ASPECT + 1.2f));
        int under = wb > wr;
        w = floorf(under ? wb : wr);
        h = SOLUI_ASPECT * w;
        pitch = fminf(1.16f * w, (b.w - 1.1f * w - (under ? 0 : 1.4f * w)) / 3);
        float room = b.h - 0.2f * w - h - (under ? h + 1.0f * w : 0);
        row_h = clampf(room / 12, 0.5f * h, 0.75f * h);
        float grid_w = 3 * pitch + w, grid_h = 12 * row_h + h, tall = grid_h + (under ? 0.6f * w + h : 0);
        x0 = b.x + (b.w - grid_w - (under ? 0 : 1.4f * w)) / 2 + w / 2;
        y0 = b.y + (b.h - tall) / 2 + h / 2;
        hx = under ? b.x + b.w / 2 : x0 + 3 * pitch + 1.4f * w;
        hy = under ? y0 + grid_h + 0.6f * w : y0 + 6 * row_h;
    }
    U.w = w;
    for (int i = 0; i < MT_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->fan = SF_PILE;
        if (i < MT_HAND) {
            int r = i / 13, c = i % 13;
            s->x = x0 + (turned ? r : c) * pitch;
            s->y = y0 + (turned ? c : r) * row_h;
        } else {
            s->x = hx;
            s->y = hy;
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
    solui_new(&U);
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

static int level_stars(void) { return solui_stars(&U); }
static int score(void) { return G.moves; }

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
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
};
REGISTER_GAME(montana_game)
