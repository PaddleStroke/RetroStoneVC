/*
 * Clock in RetroStone Board, for one player (the rules: clock_rules.c, clock.h; the cards' slides, the hint and the
 * win: solitaire_ui.h). Twelve piles round a clock face, each with its hour in digits beside it, the kings' pile in
 * the middle; the cards turned peek out from under each pile, towards the middle. A golden clock hand points at the
 * pile the next card comes from (that pile in gold light); it turns round to the next pile after each card. The game
 * plays itself: a tap anywhere turns the next card, which flies to its hour, turning over on its way. The bar: the
 * cards turned, the hint (the next card). A tiny game for children.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/clock.h"
#include "games/solitaire_ui.h"
#include "shell.h"

#define HAND_MS 420

static solrec G;
static solui U = {.R = &ck_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1};
static int dealing, pressed;
static float cx, cy, rx, ry;                    /* the clock's middle and the piles' ellipse */
static float hand_from, hand_to;                /* the hand's angle (degrees, clockwise from twelve) */
static Uint32 hand_t0;
static int hand_at = -1;                        /* the rank it points at */

static float hour_deg(int r) { return 30.0f * (r % 12); }

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    float w = floorf(fminf(b.w / 8.4f, b.h / 8.6f)), h = SOLUI_ASPECT * w;
    cx = b.x + b.w / 2;
    cy = b.y + b.h / 2;
    rx = b.w / 2 - 1.15f * w;
    ry = b.h / 2 - 0.5f * h - 0.65f * w;
    if (rx > 1.6f * ry) rx = 1.6f * ry;         /* sideways: not too flat */
    U.w = w;
    for (int i = 0; i < CK_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        int r = i % 13 + 1;
        float a = hour_deg(r) * 3.14159265f / 180, x = cx, y = cy, ux = 0, uy = 1;
        if (r != 13) {
            ux = sinf(a), uy = -cosf(a);
            x += rx * ux, y += ry * uy;
            ux = -ux, uy = -uy;                 /* towards the middle */
        }
        s->fan = SF_PILE;
        s->x = x + (i < CK_DOWN ? 0.3f * h * ux : 0);
        s->y = y + (i < CK_DOWN ? 0.3f * h * uy : 0);
    }
}

static void start(int v)
{
    (void)v;
    ck_new(&G, (uint32_t)ui_random(1 << 30));
    solui_new(&U);
    dealing = 1;
    hand_at = -1;
}

static int status(void) { return ck_won(&G.pos) ? GS_WIN0 : ck_over(&G.pos) ? GS_WIN1 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    hand_at = -1;
    return solui_scripted(&U, ck_script(&G, s));
}

/* ---- the clock: the hours, the hand ------------------------------------------------------------------------------ */
static void paint_hand(int kind, int size, uint8_t *rgba)   /* pointing up from the middle, gold */
{
    (void)kind;
    static const uint8_t gold[3] = {226, 186, 92}, ink[3] = {120, 84, 30};
    memset(rgba, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    float t = 7.0f / size > 0.012f ? 7.0f / size : 0.012f;
    art_band(f, ART_PART, 0.5f, 0.5f, 0.5f, 0.1f, t);
    art_head(f, ART_PART, 0.5f, 0.04f, -90, 0.075f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 1.8f * t, 1.8f * t);
    art_fig_paint(rgba, size, f, gold, ink, 0.003f, 0.5f);
}

static void draw_hand(void)
{
    int r = ck_current(&G.pos);
    Uint32 now = ui_ms();
    if (r != 13 && r != hand_at) {              /* it turns on, clockwise, to the next pile */
        float shown = hand_to;
        if (hand_at >= 0 && now - hand_t0 < HAND_MS) {
            float t = (float)(now - hand_t0) / HAND_MS;
            shown = hand_from + (hand_to - hand_from) * t * t * (3 - 2 * t);
        }
        hand_from = hand_at < 0 ? hour_deg(r) : shown;
        hand_to = hour_deg(r);
        while (hand_to < hand_from) hand_to += 360;
        while (hand_to - hand_from >= 360) hand_to -= 360;
        hand_t0 = hand_at < 0 ? now - HAND_MS : now;
        hand_at = r;
    }
    if (hand_at < 0) return;
    float deg = hand_to;
    if (now - hand_t0 < HAND_MS) {
        float t = (float)(now - hand_t0) / HAND_MS;
        deg = hand_from + (hand_to - hand_from) * t * t * (3 - 2 * t);
        ui_animate();
    }
    float rad = deg * 3.14159265f / 180, len = hypotf(rx * sinf(rad), ry * cosf(rad)) - 0.45f * SOLUI_ASPECT * U.w;
    int size = (int)(2 * len);
    if (size < 8) return;
    ui_put_rot(ct_picture(paint_hand, 0, size), cx, cy, (float)size, (float)size, deg);
}

static void draw(void)
{
    const solpos *p = &G.pos;
    if (dealing) {
        dealing = 0;
        solui_deal_kinds(&U, cx, cy, 16, 1u << SP_STOCK);
    }
    int r = ck_current(p), over = ck_over(p);
    float w = U.w, h = SOLUI_ASPECT * w;
    for (int k = 1; k <= 12; k++) {             /* the hours, outside the piles, clear of the cards */
        const solspot *s = &U.at[CK_DOWN + k - 1];
        float a = hour_deg(k) * 3.14159265f / 180, ux = sinf(a), uy = -cosf(a);
        float t = fminf(fabsf(ux) > 0.01f ? 0.5f * w / fabsf(ux) : 1e9f,
                        fabsf(uy) > 0.01f ? 0.5f * h / fabsf(uy) : 1e9f);
        t += 0.32f * w + (k >= 10 ? 0.12f * w * fabsf(ux) : 0);
        ui_number(s->x + t * ux, s->y + t * uy, 0.4f * w, k, 1);
    }
    if (!U.won) draw_hand();
    if (!over && !U.won && !solui_busy(&U)) ui_mark_gold(U.at[CK_DOWN + r - 1].x, U.at[CK_DOWN + r - 1].y, 1.5f * w);
    solui_draw(&U);
}

/* any tap on the table: the next card */
static void press(float x, float y)
{
    (void)x, (void)y;
    pressed = 1;
}

static void motion(float x, float y) { (void)x, (void)y; }

static void release(float x, float y)
{
    (void)x, (void)y;
    if (!pressed || U.won) return;
    pressed = 0;
    solmove m = ck_next(&G.pos);
    if (m.kind == SM_NONE || !solui_play(&U, m, NULL)) solui_deny();
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    solui_bar(&U, b, 0);
}

/* the end card as the other patiences': the stars, the cards turned, replay and the next deal. The clock deals its own
 * game: no choice, no hint nor undo, changes how it ends, so a deal that comes out is three stars (solui_stars would
 * take them away for a hint that only shows the card a tap turns anyway); one that does not, the usual card. */
static int level_stars(void) { return ck_won(&G.pos) ? 3 : 0; }
static int score(void) { return G.moves; }

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD_NONE, CARD(SUIT_DIAMONDS, RANK_KING));
}

/* small backs round a ring, a king face up in the middle, the hand pointing at three o'clock */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.13f;
    int hs = size;
    uint8_t *hand = malloc((size_t)hs * hs * 4);
    if (hand) {                                 /* the hand, turned to three o'clock: painted pointing up, rotated */
        paint_hand(0, hs, hand);
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                const uint8_t *s = hand + 4 * ((size_t)(size - 1 - x) * hs + y);
                uint8_t *d = rgba + 4 * ((size_t)y * size + x);
                if (s[3]) memcpy(d, s, 4);
            }
        free(hand);
    }
    for (int k = 1; k <= 12; k++) {
        float a = hour_deg(k) * 3.14159265f / 180;
        int card = k == 3 ? CARD(SUIT_SPADES, 3) : CARD_NONE;
        cards_paint_at(rgba, size, DECK_FRENCH, card, (0.5f + 0.38f * sinf(a)) * size, (0.5f - 0.36f * cosf(a)) * size,
                       (int)(w * size));
    }
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_KING), 0.5f * size, 0.5f * size,
                   (int)(0.17f * size));
}

static const game clock_game = {
    .id = "clock", .order = 349, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
};
REGISTER_GAME(clock_game)
