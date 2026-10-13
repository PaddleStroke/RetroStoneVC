/*
 * Yacht in RetroStone Board: a wooden scorecard and a well of green felt on the table, side by side; the dice in the
 * player's colour tumble on the felt (tap the felt to throw), a tap on a die keeps it (it slides to the kept lane at
 * the felt's edge), a tap on a category shows what it would score, a second tap scores it (the rules and the
 * notation: yacht_rules.c, yacht.h). One to six players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The scorecard has no words: each row's category is a picture of dice (a face for ones to sixes, overlapping dice
 * for a pair or three of a kind, a run for a straight, three and two for a full house, five alike for five of a kind,
 * dice scattered every way for chance), a gold star for the upper bonus, an equals sign for the total; a column for
 * each player (their die on top), the player to move's column lit. Face to face, the top players' columns read their
 * way. Coordinates: fractions of the board's side, from its top left.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/yacht.h"
#include "shell.h"

#define IN0 (CUP_FR + 0.012f)                   /* the plank inside the frame */
#define IN1 (1 - CUP_FR - 0.012f)
#define FELT_W 0.29f
#define ICON_W 0.135f                           /* the scorecard's column of pictures */
#define COL_MAX 0.15f
#define LANE 0.105f                             /* the kept dice's lane, along the felt's left edge */
#define DIE 0.082f
#define SLOT0 0.115f                            /* the five dice's places down the felt */
#define SLOT_STEP 0.145f
#define PIPS_Y 0.86f                            /* the throws left */

/* the rows of the scorecard after the categories */
enum { ROW_CAT, ROW_BONUS, ROW_EXTRA, ROW_TOTAL };

typedef struct {
    float sx0, sx1, sy0, sy1;                   /* the scorecard */
    float hdr, row, cw;                         /* the header's height, a row's, a player's column's width */
    float fx0, fx1;                             /* the felt (top and bottom: the plank's) */
    int nrows, kind[YA_MAX_CATS + 4], cat[YA_MAX_CATS + 4];
} geom;

static ygame G;
static const ypos *P = &G.pos;
static int S, made = -1, made_np = -1, made_var = -1;
static float bx, by;
static SDL_Texture *t_board;
static int pick = -1;                           /* the category tapped once (its points shown); -1: none */
static struct { int on, key; unsigned mask; Uint32 t0; } tumble;
static Uint32 slide_t0[YA_DICE];                /* a die kept or let go: when it started sliding */
static unsigned slide_mask;
static struct { int on, cat; Uint32 t0; } pend;   /* a bot's move, shown first: the dice kept slide, or its box
                                                 * is picked; then the throw (cat -1) or the score */
#define PEND_MS 450

static geom geom_for(int variant, int players)
{
    geom g;
    int cats[YA_MAX_CATS], n = ya_cats(variant, cats);
    g.nrows = 0;
    for (int i = 0; i < n; i++) {
        g.kind[g.nrows] = ROW_CAT;
        g.cat[g.nrows++] = cats[i];
        if (cats[i] == YC_SIXES && ya_bonus_at(variant)) g.kind[g.nrows] = ROW_BONUS, g.cat[g.nrows++] = -1;
    }
    if (variant == YA_CLASSIC) g.kind[g.nrows] = ROW_EXTRA, g.cat[g.nrows++] = -1;
    g.kind[g.nrows] = ROW_TOTAL;
    g.cat[g.nrows++] = -1;
    g.cw = fminf(COL_MAX, (IN1 - IN0 - FELT_W - 0.02f - ICON_W) / players);
    float sw = ICON_W + players * g.cw, free = IN1 - IN0 - FELT_W - 0.02f - sw;
    g.sx0 = IN0 + free / 3;
    g.sx1 = g.sx0 + sw;
    g.fx1 = IN1 - free / 3;
    g.fx0 = g.fx1 - FELT_W;
    g.sy0 = IN0;
    g.sy1 = IN1;
    g.row = (g.sy1 - g.sy0 - 0.012f) / (g.nrows + 0.85f);
    g.hdr = 0.85f * g.row;
    return g;
}

static geom now_geom(void) { return geom_for(P->variant, P->players); }

static float row_y(geom g, int r) { return g.sy0 + 0.006f + g.hdr + r * g.row; }   /* a row's top */
static float col_x(geom g, int p) { return g.sx0 + ICON_W + p * g.cw; }             /* a player's column's left */

/* where die i rests: kept (in the lane) or thrown (in the felt, a little askew after throw `key`) */
static void die_spot(geom g, int i, int kept, int key, float *x, float *y, float *deg)
{
    *y = SLOT0 + i * SLOT_STEP;
    if (kept) {
        *x = g.fx0 + LANE / 2;
        *deg = 0;
        return;
    }
    float mid = g.fx0 + LANE + (FELT_W - LANE) / 2;
    *x = mid + 0.035f * cup_jitter(key, i, 0);
    *y += 0.018f * cup_jitter(key, i, 1);
    *deg = key < 0 ? 0 : cup_tilt(key, i);
}

static int throw_key(int turns, int throws) { return turns * YA_THROWS + throws; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    pick = -1;
    tumble.on = 0;
    slide_mask = 0;
    pend.on = 0;
}

static void start(int variant)
{
    ya_new(&G, variant, ui.players);
    cup_mem_reset();
    reset_ui();
}

static int status(void)
{
    if (!P->over) return GS_PLAY;
    if (P->players == 1) {
        int goal = P->variant == YA_GENERALA ? YA_SOLO_GENERALA : YA_SOLO_GOAL;
        return P->served || ya_total(P, 0) >= goal ? GS_WIN0 : GS_WIN1;
    }
    return P->winner >= 0 ? gs_win(P->winner) : GS_DRAW;
}

static unsigned winners(void)                   /* a tie for the highest total: they share the win */
{
    unsigned m = 0;
    if (!P->over || P->players < 2) return 0;
    if (P->winner >= 0) return 1u << P->winner;
    int best = 0;
    for (int q = 0; q < P->players; q++) best = ya_total(P, q) > best ? ya_total(P, q) : best;
    for (int q = 0; q < P->players; q++)
        if (ya_total(P, q) == best) m |= 1u << q;
    return m;
}

static int turn(void) { return P->turn; }

static int sliding(void)
{
    for (int i = 0; i < YA_DICE; i++)
        if (slide_mask >> i & 1 && ui_ms() - slide_t0[i] < CUP_SLIDE_MS) return 1;
    return 0;
}

static int busy(void) { return (tumble.on && ui_ms() - tumble.t0 < UI_DICE_MS) || sliding() || pend.on; }
static void home(int player, float *fx, float *fy) { cup_home(player, P->players, fx, fy); }

static void throw_dice(void)
{
    int v[YA_DICE], key = throw_key(P->turns, P->throws);
    unsigned kept = P->throws ? P->kept : 0;
    cup_roll(key, YA_DICE, v);
    if (!ya_throw(&G, kept, v)) { ui_sound(SND_DENY); return; }
    tumble.on = 1;
    tumble.key = key;
    tumble.mask = ~kept & ((1u << YA_DICE) - 1);
    tumble.t0 = ui_ms();
    slide_mask = 0;
    pick = -1;
    ui_sound(SND_DICE);
}

static void toggle_keep(int i)
{
    if (!P->throws || P->throws >= YA_THROWS || P->over) { ui_sound(SND_DENY); return; }
    unsigned k = P->kept ^ (1u << i);
    if (!ya_keep(&G, k)) { ui_sound(SND_DENY); return; }
    slide_mask |= 1u << i;
    slide_t0[i] = ui_ms();
    pick = -1;
    if (k >> i & 1) ui_sound(SND_PLACE);
    else ui_sound_soft(SND_TAP, 0.5f);
}

static void tap_category(int cat)
{
    if (ya_points(P, cat) < 0) { ui_sound(SND_DENY); pick = -1; return; }
    if (pick != cat) {                          /* the first tap: its points shown */
        pick = cat;
        ui_sound_soft(SND_TAP, 0.5f);
        return;
    }
    if (!ya_score(&G, cat)) { ui_sound(SND_DENY); return; }
    pick = -1;
    tumble.on = 0;
    slide_mask = 0;
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    if (pend.on) { pend.on = 0; pick = -1; return 1; }   /* a bot's move still on show: not made */
    if (pick >= 0) { pick = -1; return 1; }
    if (!ya_undo(&G)) return 0;
    reset_ui();
    return 1;
}

/* "d33215 d33-6- 3": throws and scores (yacht.h) */
static int script(const char *s)
{
    int ok = ya_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] & YA_THROW_EV) {
            const ypos *b = &G.past[i];
            int v[YA_DICE];
            unsigned kept = b->throws ? b->kept : 0;
            for (int k = 0; k < YA_DICE; k++) v[k] = kept >> k & 1 ? 0 : (G.ev[i] >> (3 * k) & 7);
            cup_remember(throw_key(b->turns, b->throws), YA_DICE, v);
        }
    reset_ui();
    if (P->throws) {                            /* the last throw at rest on the felt */
        tumble.on = 1;
        tumble.key = throw_key(P->turns, P->throws - 1);
        tumble.mask = 0;
        tumble.t0 = ui_ms() - UI_DICE_MS;
    }
    return ok;
}

/* ---- the bot (yacht_bot.c) --------------------------------------------------------------------------------------- */
const ygame *yacht_live(void) { return &G; }

/* a bot's move, as taps make it: "d--?-?" the dice marked "-" kept (they slide to the lane), the others thrown a
 * moment later (the game's faces); a category's name: its box picked (its points shown), then scored */
int yacht_show(const char *move)
{
    if (busy() || P->over) return 0;
    if (move[0] == 'd') {
        unsigned kept = 0;
        if (strlen(move) != 1 + YA_DICE || P->throws >= YA_THROWS) return 0;
        for (int i = 0; i < YA_DICE; i++) {
            if (move[1 + i] == '-') kept |= 1u << i;
            else if (move[1 + i] != '?') return 0;
        }
        if ((!P->throws && kept) || kept == (1u << YA_DICE) - 1) return 0;
        for (int i = 0; i < YA_DICE; i++)
            if (P->throws && ((kept ^ P->kept) >> i & 1)) toggle_keep(i);
        pend.on = 1;
        pend.cat = -1;
        pend.t0 = ui_ms();
        return 1;
    }
    int cat = ya_cat_by_name(move, (int)strlen(move));
    if (ya_points(P, cat) < 0) return 0;
    pick = cat;
    ui_sound_soft(SND_TAP, 0.5f);
    pend.on = 1;
    pend.cat = cat;
    pend.t0 = ui_ms();
    return 1;
}

static void pending(void)                       /* the bot's move, after its moment on show */
{
    if (!pend.on || ui_ms() - pend.t0 < (pend.cat < 0 && slide_mask ? CUP_SLIDE_MS + PEND_MS : PEND_MS)) return;
    pend.on = 0;
    if (pend.cat < 0) {
        throw_dice();
        return;
    }
    pick = pend.cat;
    tap_category(pend.cat);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* n dice of these faces in a row centred on (cx, cy): `step` apart (of a die: under 1, they overlap), a die `d`
 * pixels, each raised by rise (of a die) more than the one before */
static void paint_row(uint8_t *o, int n, float cx, float cy, int d, const int *faces, int count, float step,
                      float rise)
{
    float w = d * (1 + step * (count - 1));
    for (int i = 0; i < count; i++) {
        float x = cx - w / 2 + d / 2.0f + i * step * d, y = cy + (i - (count - 1) / 2.0f) * rise * d;
        art_die_blend(o, n, n, x, y, d, 0, faces[i], 0);
    }
}

static void paint_star(uint8_t *o, int n, float cx, float cy, float r)
{
    static const uint8_t gold[3] = {226, 178, 64}, edge[3] = {130, 90, 30};
    float pts[20];
    for (int k = 0; k < 10; k++) {
        float a = -ART_PI / 2 + k * ART_PI / 5, rr = k & 1 ? 0.21f : 0.5f;
        pts[2 * k] = 0.5f + rr * cosf(a);
        pts[2 * k + 1] = 0.53f + rr * sinf(a);
    }
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, pts, 10);
    art_style st = art_flat(gold, edge, 0.04f, 0.3f);
    art_fig_blend(o, n, n, f, &st, (int)(cx - r), (int)(cy - r), (int)(2 * r), 1);
    art_fig_free(f);
}

/* a row's picture: the category, the bonus, the total; w x h pixels centred on (cx, cy) */
static void paint_icon(uint8_t *o, int n, int kind, int cat, int variant, float cx, float cy, float w, float h)
{
    int d = (int)fminf(0.66f * h, 0.3f * w);
    if (kind == ROW_BONUS) { paint_star(o, n, cx, cy, 0.42f * h); return; }
    if (kind == ROW_EXTRA) {
        static const int five[5] = {6, 6, 6, 6, 6};
        paint_row(o, n, cx - 0.12f * w, cy, (int)(0.8f * d), five, 5, 0.32f, 0);
        paint_star(o, n, cx + 0.33f * w, cy - 0.08f * h, 0.3f * h);
        return;
    }
    if (kind == ROW_TOTAL) {                    /* an equals sign: the sum */
        art_tint_rect(o, n, n, cx - 0.16f * w, cy - 0.17f * h, cx + 0.16f * w, cy - 0.06f * h, ART_INK, 0.85f);
        art_tint_rect(o, n, n, cx - 0.16f * w, cy + 0.06f * h, cx + 0.16f * w, cy + 0.17f * h, ART_INK, 0.85f);
        return;
    }
    if (ya_is_upper(cat)) {
        art_die_blend(o, n, n, cx, cy, d, 0, cat + 1, 0);
        return;
    }
    int small = (int)fminf(0.6f * h, w * 0.9f / 5.25f);
    switch (cat) {
    case YC_PAIR: paint_row(o, n, cx, cy, d, (const int[]){4, 4}, 2, 0.55f, -0.08f); break;
    case YC_TWO_PAIRS:
        paint_row(o, n, cx - 0.21f * w, cy, (int)(0.9f * d), (const int[]){3, 3}, 2, 0.5f, -0.08f);
        paint_row(o, n, cx + 0.21f * w, cy, (int)(0.9f * d), (const int[]){5, 5}, 2, 0.5f, -0.08f);
        break;
    case YC_THREE_KIND: paint_row(o, n, cx, cy, d, (const int[]){5, 5, 5}, 3, 0.5f, -0.06f); break;
    case YC_FOUR_KIND: paint_row(o, n, cx, cy, d, (const int[]){2, 2, 2, 2}, 4, 0.42f, -0.05f); break;
    case YC_FULL:
        paint_row(o, n, cx - 0.17f * w, cy, (int)(0.85f * d), (const int[]){6, 6, 6}, 3, 0.42f, -0.05f);
        paint_row(o, n, cx + 0.27f * w, cy, (int)(0.85f * d), (const int[]){2, 2}, 2, 0.42f, -0.05f);
        break;
    case YC_SMALL:
        if (variant == YA_NORDIC) paint_row(o, n, cx, cy, small, (const int[]){1, 2, 3, 4, 5}, 5, 1.06f, 0);
        else paint_row(o, n, cx, cy, small, (const int[]){1, 2, 3, 4}, 4, 1.06f, 0);
        break;
    case YC_LARGE:
        if (variant == YA_NORDIC) paint_row(o, n, cx, cy, small, (const int[]){2, 3, 4, 5, 6}, 5, 1.06f, 0);
        else paint_row(o, n, cx, cy, small, (const int[]){1, 2, 3, 4, 5}, 5, 1.06f, 0);
        break;
    case YC_STRAIGHT: paint_row(o, n, cx, cy, small, (const int[]){2, 3, 4, 5, 6}, 5, 1.06f, 0); break;
    case YC_YACHT:
        paint_row(o, n, cx, cy, d, (const int[]){6, 6, 6, 6, 6}, 5, 0.34f, 0);
        break;
    case YC_DOUBLE:
        paint_row(o, n, cx - 0.1f * w, cy - 0.14f * h, (int)(0.8f * d), (const int[]){6, 6, 6, 6, 6}, 5, 0.3f, 0);
        paint_row(o, n, cx + 0.1f * w, cy + 0.14f * h, (int)(0.8f * d), (const int[]){6, 6, 6, 6, 6}, 5, 0.3f, 0);
        break;
    case YC_CHANCE: {                           /* dice thrown every way */
        static const float at[4][3] = {{-0.3f, 0.12f, -28}, {-0.07f, -0.14f, 21}, {0.15f, 0.13f, -12},
                                       {0.33f, -0.1f, 38}};
        static const int faces[4] = {3, 6, 1, 4};
        for (int i = 0; i < 4; i++)
            art_die_blend(o, n, n, cx + at[i][0] * w, cy + at[i][1] * h, (int)(0.82f * d), 0, faces[i], at[i][2]);
        break;
    }
    }
}

/* the table: the plank in its frame, the scorecard inlaid (its rules, its pictures), the felt with its kept lane */
static void paint_board(uint8_t *o, int n, geom g, int variant, int players)
{
    cup_board(o, n);
    static const float maple[3] = {1.1f, 1.12f, 1.16f};
    art_inlay(o, n, n, g.sx0 * n, g.sy0 * n, (g.sx1 - g.sx0) * n, (g.sy1 - g.sy0) * n, 0.012f * n, maple,
              0.004f * n);
    float x0 = g.sx0 * n, x1 = g.sx1 * n, lw = fmaxf(1, 0.0016f * n);
    for (int r = 0; r < g.nrows; r++) {         /* the rules: strong round the bonus, double above the total */
        float y = row_y(g, r) * n;
        int strong = r == 0 || g.kind[r] != ROW_CAT || (r > 0 && g.kind[r - 1] == ROW_BONUS);
        art_tint_rect(o, n, n, x0 + 0.004f * n, y - lw / 2, x1 - 0.004f * n, y + lw / 2, ART_INK,
                      strong ? 0.7f : 0.35f);
        if (g.kind[r] == ROW_TOTAL)
            art_tint_rect(o, n, n, x0 + 0.004f * n, y - 2.5f * lw, x1 - 0.004f * n, y - 1.5f * lw, ART_INK, 0.7f);
    }
    float yt = row_y(g, 0) * n - 0.6f * g.hdr * n, yb = row_y(g, g.nrows) * n;
    for (int p = 0; p < players; p++) {         /* the columns */
        float x = col_x(g, p) * n;
        art_tint_rect(o, n, n, x - lw / 2, yt, x + lw / 2, yb, ART_INK, p ? 0.3f : 0.6f);
    }
    for (int r = 0; r < g.nrows; r++) {
        float cy = (row_y(g, r) + g.row / 2) * n;
        paint_icon(o, n, g.kind[r], g.cat[r], variant, (g.sx0 + ICON_W / 2) * n, cy, ICON_W * n, g.row * n);
    }
    cup_well(o, n, g.fx0, IN0, g.fx1, IN1);
    static const uint8_t dark[3] = {18, 60, 36};
    art_tint_rect(o, n, n, g.fx0 * n, IN0 * n, (g.fx0 + LANE) * n, IN1 * n, dark, 0.35f);   /* the kept lane */
    art_tint_rect(o, n, n, (g.fx0 + LANE) * n - lw, IN0 * n, (g.fx0 + LANE) * n, IN1 * n, ART_INK, 0.3f);
}

static box laid;                                /* the board's square, as layout() last had it */

static void layout(box b)
{
    laid = b;
    S = (int)floorf(b.w);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made == S && made_np == P->players && made_var == P->variant) return;
    made = S;
    made_np = P->players;
    made_var = P->variant;
    uint8_t *o = malloc((size_t)S * S * 4);
    if (!o) return;
    paint_board(o, S, now_geom(), P->variant, P->players);
    ui_free(&t_board);
    t_board = ui_tex(S, S, o);
    free(o);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static float X(float f) { return bx + f * S; }
static float Y(float f) { return by + f * S; }

static void number_at(float cx, float cy, float h, int v, int light, int upside)
{
    if (upside) ui_number_rot(X(cx), Y(cy), h * S, v, light, 180);
    else ui_number(X(cx), Y(cy), h * S, v, light);
}

static void draw_sheet(geom g)
{
    int cur = P->over ? -1 : P->turn;
    float top = row_y(g, 0), bottom = row_y(g, g.nrows);
    if (cur >= 0) {                             /* the player to move's column, lit */
        float x = col_x(g, cur);
        ui_fill(X(x), Y(top - g.hdr), g.cw * S, (bottom - top + g.hdr) * S, 250, 214, 90, 60);
    }
    int lastp = -1, lastc = -1;                 /* the last score: a warm tint */
    if (G.n && !(G.ev[G.n - 1] & YA_THROW_EV) && !(P->throws && !P->over)) {
        lastp = G.past[G.n - 1].turn;
        lastc = G.ev[G.n - 1];
    }
    float dh = fminf(0.56f * g.row, 0.3f * g.cw);   /* the digits' height (three fit a column) */
    for (int p = 0; p < P->players; p++) {
        float cx = col_x(g, p) + g.cw / 2;
        int up = ui_upside(p);
        if (p == cur) ui_mark_gold(X(cx), Y(top - g.hdr / 2), 1.2f * g.hdr * S);
        ui_die(X(cx), Y(top - g.hdr / 2), 0.62f * g.hdr * S, p, 5, up ? 180 : 0, 0, 1, 1);
        for (int r = 0; r < g.nrows; r++) {
            float cy = row_y(g, r) + g.row / 2;
            int v = -1;
            float h = dh;
            if (g.kind[r] == ROW_CAT) {
                v = P->box[p][g.cat[r]];
                if (p == lastp && g.cat[r] == lastc) ui_mark_last(X(col_x(g, p)), Y(row_y(g, r)), g.cw * S, g.row * S);
            } else if (g.kind[r] == ROW_BONUS) {
                int full = 1;
                for (int c = YC_ONES; c <= YC_SIXES; c++) full &= P->box[p][c] >= 0;
                v = ya_bonus(P, p);
                if (!v && !full) {              /* not yet: the upper part so far, small */
                    v = ya_upper(P, p);
                    h = 0.62f * dh;
                    if (!v) v = -1;
                }
            } else if (g.kind[r] == ROW_EXTRA) {
                v = P->extra[p] ? 100 * P->extra[p] : -1;
            } else {
                v = ya_total(P, p);
                h = fminf(1.12f * dh, 0.27f * g.cw * 1.15f);
            }
            if (v >= 0) number_at(cx, cy, h, v, 0, up);
        }
    }
    if (pick >= 0 && cur >= 0) {                /* the points the category would score: cream on dark, lit */
        for (int r = 0; r < g.nrows; r++) {
            if (g.kind[r] != ROW_CAT || g.cat[r] != pick) continue;
            float x = col_x(g, cur), y = row_y(g, r), cx = x + g.cw / 2, cy = y + g.row / 2;
            ui_mark_select(X(g.sx0), Y(y), (x - g.sx0) * S, g.row * S);
            ui_mark_gold(X(cx), Y(cy), 1.5f * g.row * S);
            ui_fill(X(x + 0.06f * g.cw), Y(y + 0.08f * g.row), 0.88f * g.cw * S, 0.84f * g.row * S, 84, 56, 36, 235);
            number_at(cx, cy, dh, ya_points(P, pick), 1, ui_upside(cur));
        }
    }
}

static void draw_felt(geom g)
{
    Uint32 now = ui_ms();
    int me = P->over ? (P->winner >= 0 ? P->winner : P->turn) : P->turn;
    float ds = DIE * S;
    int can_throw = !P->over && P->throws < YA_THROWS && (!P->throws || P->kept != (1u << YA_DICE) - 1);
    float px0 = g.fx0 + FELT_W / 2;             /* the throws left: small dice, lit while they can be thrown */
    if (!P->over)
        for (int k = 0; k < YA_THROWS; k++) {
            float x = px0 + (k - 1) * 0.07f;
            int left = k >= P->throws;
            if (left && can_throw) ui_mark_gold(X(x), Y(PIPS_Y), 0.085f * S);
            ui_die(X(x), Y(PIPS_Y), 0.045f * S, me, 6 - k, 0, 0, left ? 1 : 0.28f, 1);
        }
    if (!P->throws) {                           /* waiting to be thrown: the player's five dice */
        if (P->over) return;
        static const int idle[YA_DICE] = {5, 3, 6, 2, 4};
        for (int i = 0; i < YA_DICE; i++) {
            float x, y, deg;
            die_spot(g, i, 0, -1, &x, &y, &deg);
            ui_die(X(x), Y(y), ds, me, idle[i], 0, 0, 0.9f, 1);
        }
        return;
    }
    int t = tumble.on ? (int)(now - tumble.t0) : UI_DICE_MS;
    int key = tumble.on ? tumble.key : throw_key(P->turns, P->throws - 1);
    for (int i = 0; i < YA_DICE; i++) {
        int kept = P->kept >> i & 1;
        float x, y, deg;
        die_spot(g, i, kept, key, &x, &y, &deg);
        if (slide_mask >> i & 1) {              /* sliding to or from the lane */
            float f = (float)(now - slide_t0[i]) / CUP_SLIDE_MS, x0, y0, d0;
            die_spot(g, i, !kept, key, &x0, &y0, &d0);
            if (f < 1) {
                x = cup_mix(x0, x, f);
                y = cup_mix(y0, y, f);
                deg = cup_mix(d0, deg, f);
                ui_animate();
            }
        }
        if (tumble.on && (tumble.mask >> i & 1) && t < UI_DICE_MS)
            ui_die_thrown(X(x), Y(y), ds, me, P->dice[i], deg, i, t, 1);
        else ui_die(X(x), Y(y), ds, me, P->dice[i], deg, 0, 1, 1);
    }
}

static void draw(void)
{
    pending();
    if (made_np != P->players || made_var != P->variant) layout(laid);   /* a new game of another kind */
    geom g = now_geom();
    ui_put(t_board, bx, by, (float)S, (float)S);
    draw_sheet(g);
    draw_felt(g);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->over || (tumble.on && ui_ms() - tumble.t0 < UI_DICE_MS)) return;   /* not while they tumble */
    geom g = now_geom();
    float u = (x - bx) / S, v = (y - by) / S;
    if (u >= g.sx0 && u <= g.sx1 && v >= row_y(g, 0) && v < row_y(g, g.nrows)) {   /* a row of the scorecard */
        int r = (int)floorf((v - row_y(g, 0)) / g.row);
        if (r >= 0 && r < g.nrows && g.kind[r] == ROW_CAT) tap_category(g.cat[r]);
        else { pick = -1; ui_sound(SND_DENY); }
        return;
    }
    if (u >= g.fx0 && u <= g.fx1 && v >= IN0 && v <= IN1) {   /* the felt: a die, or a throw */
        if (P->throws) {
            int key = throw_key(P->turns, P->throws - 1);
            for (int i = 0; i < YA_DICE; i++) {
                float dx, dy, deg;
                die_spot(g, i, P->kept >> i & 1, key, &dx, &dy, &deg);
                if (fabsf(u - dx) < 0.6f * DIE && fabsf(v - dy) < 0.6f * DIE) { toggle_keep(i); return; }
            }
        }
        if (P->throws < YA_THROWS) throw_dice();
        else ui_sound(SND_DENY);
        return;
    }
    pick = -1;
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's total */
{
    float h = 0.5f * ui.unit;
    int s = ya_total(P, player), n = s >= 100 ? 3 : s >= 10 ? 2 : 1;
    ui_number(b.x + (0.74f * (n - 1) + 0.62f) * h / 2, b.y + b.h / 2, h, s, 1);
}

static void token(int player, int size, uint8_t *rgba) { cup_token(player, size, rgba); }

/* the menu's card and the picker's: the scorecard's pictures of the variant, a throw on the felt */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? YA_CLASSIC : variant;
    geom g = geom_for(v, 2);
    g.nrows = 0;                                /* the rows only the variant has, bigger */
    static const int rows[YA_VARIANTS][5] = {{YC_THREE_KIND, YC_FULL, YC_SMALL, YC_YACHT, -1},
                                             {YC_PAIR, YC_TWO_PAIRS, YC_FULL, YC_YACHT, -1},
                                             {YC_STRAIGHT, YC_FULL, YC_FOUR_KIND, YC_DOUBLE, -1}};
    for (int i = 0; rows[v][i] >= 0; i++) g.kind[g.nrows] = ROW_CAT, g.cat[g.nrows++] = rows[v][i];
    g.sx0 = IN0;
    g.sx1 = 0.6f;
    g.fx0 = 0.64f;
    g.fx1 = IN1;
    g.row = (g.sy1 - g.sy0 - 0.012f) / (g.nrows + 0.3f);
    g.hdr = 0.15f * g.row;
    cup_board(rgba, size);
    static const float maple[3] = {1.1f, 1.12f, 1.16f};
    art_inlay(rgba, size, size, g.sx0 * size, g.sy0 * size, (g.sx1 - g.sx0) * size, (g.sy1 - g.sy0) * size,
              0.02f * size, maple, 0.008f * size);
    for (int r = 0; r < g.nrows; r++) {
        float y = row_y(g, r) * size;
        if (r)
            art_tint_rect(rgba, size, size, g.sx0 * size + 2, y - 0.5f, g.sx1 * size - 2, y + 0.5f, ART_INK, 0.4f);
        paint_icon(rgba, size, ROW_CAT, g.cat[r], v, (g.sx0 + g.sx1) / 2 * size, y + g.row * size / 2,
                   (g.sx1 - g.sx0) * size, g.row * size);
    }
    cup_well(rgba, size, g.fx0, IN0, g.fx1, IN1);
    static const int faces[YA_VARIANTS][3] = {{6, 6, 6}, {2, 3, 4}, {5, 5, 5}};
    for (int i = 0; i < 3; i++)
        art_die_blend(rgba, size, size, (g.fx0 + g.fx1) / 2 * size + (i == 1 ? 0.03f : -0.02f) * size,
                      (0.25f + 0.25f * i) * size, (int)(0.15f * size), 0, faces[v][i], (float)(i * 23 - 20));
}

static const game yacht_game = {
    .id = "yacht", .order = 153, .variants = YA_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1u << 1 | 1u << 2 | 1u << 3 | 1u << 4 | 1u << 5 | 1u << 6, .home = home,
};
REGISTER_GAME(yacht_game)
