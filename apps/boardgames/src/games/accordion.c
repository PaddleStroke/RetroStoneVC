/*
 * Accordion in RetroStone Board, for one player (the rules: accordion_rules.c, accordion.h; the slides, the finger,
 * the hint: solitaire_ui.h). The row of piles wraps across the table like a line of text, as many across as give the
 * biggest cards. A tap moves a pile onto the third pile to its left if it can, else onto its neighbour; a drag carries
 * it onto either; the piles after it slide back to close the row up. The win (one pile): its cards leap off and bounce
 * across the table. The bar: the moves; the hint is the shell's tool (its move comes from a short search).
 * Levels: 150 deals of our own in six chapters (accordion_levels.h, made and won down to one pile by
 * tools/accordion_gen.c), the level screen's die for a deal at random; the stars count the piles left (1: three, 2
 * or 3: two, 4 to 6: one), and more than six left is a game lost.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/accordion.h"
#include "games/accordion_levels.h"
#include "games/solitaire_ui.h"
#include "shell.h"

#define ACD_GAP 45                              /* ms between two cards leaping off at the win */

static int animate(solui *u, const solpos *before, int first);

static solrec G;
static solui U = {.R = &acd_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .pile = -1, .made_bar = -1,
                  .animate = animate};
static int dealing, dropped;
static float drop[2];                           /* where a dragged pile was let go */

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    int best = 7;
    float bw = 0;
    for (int cols = 4; cols <= 13; cols++) {    /* the most room for each card */
        int rows = (ACD_PILES + cols - 1) / cols;
        float w = fminf(b.w / (cols * 1.08f + 0.1f), b.h / (rows * SOLUI_ASPECT * 1.07f + 0.15f));
        if (w > bw) bw = w, best = cols;
    }
    int cols = best, rows = (ACD_PILES + cols - 1) / cols;
    float w = U.w = floorf(bw), px = fminf(b.w / cols, 1.16f * w), py = fminf(b.h / rows, 1.2f * SOLUI_ASPECT * w);
    float x0 = b.x + b.w / 2 - (cols - 1) * px / 2, y0 = b.y + b.h / 2 - (rows - 1) * py / 2;
    for (int i = 0; i < ACD_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->x = x0 + (i % cols) * px;
        s->y = y0 + (i / cols) * py;
        s->fan = SF_PILE;
    }
}

/* a move's slides: the pile flies onto the other (from the finger, after a drag), then the piles after it slide back
 * a place each, all together */
static int animate(solui *u, const solpos *before, int first)
{
    for (int a = 0; a < SOL_PILES; a++)
        for (int i = 0; i < SOL_CARDS; i++) u->land[a][i] = 0, u->slot[a][i] = -1;
    solpos cur = *before, nxt;
    for (int m = first; m < u->rec->n; m++) {
        solmove mv = u->rec->mv[m];
        int a = mv.from, b = mv.to, n = mv.n, mine = !(mv.kind & SM_AUTO);
        nxt = cur;
        sol_apply(&nxt, mv);
        for (int k = 0; k < n; k++)
            solui_carry(u, &cur, &nxt, a, cur.n[a] - n + k, b, nxt.n[b] - n + k, 1, 1, mine ? 0 : 110,
                        mine && dropped ? drop : NULL, mine && k == n - 1 ? SND_MOVE : -1);
        cur = nxt;
    }
    dropped = 0;
    return 1;
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
    dropped = 0;
}

static void start(int level)                    /* a designed level (accordion_levels.h) */
{
    uint8_t deck[52];
    int v = solui_level(acd_levels, ACD_LEVELS, level, deck, 52);
    acd_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    (void)v;
    acd_new(&G, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void)
{
    if (acd_won(&G.pos)) return GS_WIN0;
    if (!acd_stuck(&G.pos)) return GS_PLAY;
    return acd_stars(acd_piles(&G.pos)) ? GS_WIN0 : GS_WIN1;
}

static int turn(void) { return 0; }

static Uint32 leap_end(void) { return U.win_t0 + (Uint32)((ACD_PILES - 1) * ACD_GAP + SOLUI_LIFE_MS); }

static int busy(void) { return dealing || ct_busy(&U.mv) || (U.won && ui_ms() < leap_end()); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, acd_script(&G, s));
}

/* the win: the last pile's cards leap off one by one from its top, each with a trail */
static void draw_leaps(void)
{
    const solpos *p = &G.pos;
    Uint32 now = ui_ms();
    int n = p->n[0], gone = now < U.win_t0 ? 0 : (int)((now - U.win_t0) / ACD_GAP) + 1;
    if (gone > n) gone = n;
    float x0 = U.at[0].x, y0 = U.at[0].y;
    if (n - gone > 0) cards_pile(U.deck, x0, y0, U.w, n - gone, p->c[0][n - gone - 1], 0);
    SDL_Rect clip = {(int)U.b.x, (int)U.b.y, (int)U.b.w, (int)U.b.h};
    SDL_RenderSetClipRect(ui.r, &clip);
    for (int j = 0; j < gone; j++) {
        float t = (float)(now - U.win_t0 - (Uint32)(j * ACD_GAP)), xs[CT_TRAIL + 1], ys[CT_TRAIL + 1];
        if (t > SOLUI_LIFE_MS) continue;
        ct_leap(U.b, U.w, j, x0, y0, t, xs, ys);
        for (int k = CT_TRAIL; k >= 0; k--) {
            if (isnan(xs[k])) continue;
            if (k) cards_place(U.deck, xs[k], ys[k], U.w);
            else cards_draw(U.deck, p->c[0][n - 1 - j], xs[k], ys[k], U.w, 0, 0);
        }
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    ui_animate();
}

static void draw(void)
{
    const solpos *p = &G.pos;
    Uint32 now = ui_ms();
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[0].x, U.at[0].y, 14);
    }
    if (U.hint && now - U.hint_t0 > SOLUI_HINT_MS) U.hint = 0;
    if (U.hint) ui_animate();
    int leaping = U.won && now >= U.win_t0 && now < leap_end();
    for (int i = 0; i < ACD_PILES && !leaping; i++) {   /* each pile: its top card on its thickness */
        int k = p->n[i];
        if (U.drag && i == U.pile) continue;
        while (k > 0 && solui_hidden(&U, i, k - 1)) k--;
        if (!k) continue;
        cards_pile(U.deck, U.at[i].x, U.at[i].y, U.w, k, p->c[i][k - 1], solui_glows(&U, i, p->n[i] - 1) ? CD_GLOW : 0);
    }
    ct_draw(&U.mv);
    if (leaping) draw_leaps();
    else if (U.won && now < U.win_t0) ui_animate();     /* the leaps are coming */
    if (U.drag && U.pile >= 0 && p->n[U.pile])  /* the whole pile under the finger */
        cards_pile(U.deck, U.fx - U.gx, U.fy - U.gy, U.w, p->n[U.pile], sol_top(p, U.pile), CD_RAISED);
    solui_tool(&U);
}

static void press(float x, float y) { solui_press(&U, x, y); }
static void motion(float x, float y) { solui_motion(&U, x, y); }

static void release(float x, float y)
{
    if (U.drag) drop[0] = x - U.gx, drop[1] = y - U.gy, dropped = 1;
    solui_release(&U, x, y);
    dropped = 0;                                /* the move's slides took it, if it was played */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    solui_bar(&U, b, 0);
}

static int level_stars(void) { return acd_stars(acd_piles(&G.pos)); }
static int score(void) { return acd_piles(&G.pos); }

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD(SUIT_CLUBS, 7), CARD(SUIT_HEARTS, 7));
}

/* a row wrapping onto a second one; its last card lifted on its way onto the third card to its left (the same rank) */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.2f;
    static const int row[7] = {CARD(SUIT_SPADES, 4), CARD(SUIT_HEARTS, 12), CARD(SUIT_DIAMONDS, 9),
                               CARD(SUIT_CLUBS, 7), CARD(SUIT_CLUBS, 2), CARD(SUIT_SPADES, 11),
                               CARD(SUIT_DIAMONDS, 6)};
    for (int i = 0; i < 7; i++)
        cards_paint_at(rgba, size, DECK_FRENCH, row[i], (0.15f + 0.233f * (i % 4)) * size,
                       (i < 4 ? 0.28f : 0.62f) * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, 7), 0.8f * size, 0.47f * size, (int)(w * size));
}

static const game accordion_game = {
    .id = "accordion", .order = 349, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = ACD_LEVELS, .chapters = ACD_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(accordion_game)
