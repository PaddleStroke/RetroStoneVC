/*
 * Secret Code in RetroStone Board, for one player: the wooden board with its rows of holes, the hidden code under a
 * wooden lid at the top, a tray of coloured pegs beside it or under it (the rules: secretcode_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The row being filled has a cursor (a hole in the select mark): a tap on a peg in the tray puts that colour there and
 * moves the cursor on to the next empty hole, so four taps fill a row; a tap on a hole moves the cursor there, a
 * second tap on it turns its peg to the next colour; a peg dragged from the tray goes in the hole it is dropped on.
 * The row full, a check button (the shell's tool, and in the row's key holes) checks it: the key pegs come one by one.
 * Undo takes back the last peg placed in the row being filled; a guess checked stays. At the end the lid slides away.
 *
 * The board takes all the room the shell gives (game.fill): upright, the tray beside it or under it (a row of cups),
 * whichever gives the bigger holes; on a wide screen it lies turned a quarter (its rows across: the guesses from the
 * left, the code at the right end under its lid), when that makes the holes bigger.
 *
 * The board is measured in half rows (q pixels): a row is 2q high, a code hole 2q wide; its key holes 3q wide. The
 * geometry is worked out upright, (a, b) from the board's top left (a across a row, b down the rows), and turned on
 * the screen when the board lies (b then runs from the right to the left, a down).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/secretcode.h"
#include "shell.h"

#define FRAME 0.6f                              /* the board's frame, in half rows */
#define HOLE_R 0.66f                            /* a code hole's radius, half rows */
#define PEG 1.56f                               /* a code peg, half rows */
#define KEY_R 0.27f                             /* a key hole's radius, half rows */
#define KEY 0.62f                               /* a key peg, half rows */
#define KEY_STEP 140                            /* ms between two key pegs appearing */
#define LID_MS 800                              /* the lid sliding away */

/* ---- the pegs' colours: the house's lacquers, and a mark on each for the colour blind ---------------------------- */
static const art_style COLOURS[SC_MAXCOLOURS] = {
    {{168, 52, 40}, {48, 14, 10}, {226, 140, 120}, {240, 196, 178}, 0.36f, 0},     /* red lacquer: a dot */
    {{222, 168, 66}, {66, 42, 10}, {120, 84, 30}, {108, 70, 18}, 0.30f, 0},        /* amber: a ring */
    {{62, 128, 98}, {14, 40, 28}, {150, 206, 176}, {200, 232, 212}, 0.36f, 0},     /* jade: a cross */
    {{52, 84, 150}, {10, 20, 46}, {150, 176, 226}, {196, 210, 242}, 0.36f, 0},     /* lapis: a triangle */
    {{242, 234, 216}, {36, 30, 28}, {64, 54, 48}, {150, 138, 120}, 0.24f, 0},      /* ivory: a diamond */
    {{70, 64, 62}, {10, 9, 9}, {150, 140, 132}, {172, 162, 152}, 0.40f, 0},        /* ebony: a bar */
    {{134, 64, 126}, {40, 12, 38}, {214, 150, 206}, {232, 196, 228}, 0.36f, 0},    /* plum: two dots */
    {{128, 178, 212}, {26, 52, 72}, {60, 96, 124}, {44, 82, 112}, 0.30f, 0},       /* sky: a cup */
};

static void mark(art_fig *f, int colour)
{
    switch (colour) {
    case 0: art_ellipse(f, ART_MARK, 0.5f, 0.5f, 0.11f, 0.11f); break;
    case 1: art_arc(f, ART_MARK, 0.5f, 0.5f, 0.075f, 0.145f, 0, 360); break;
    case 2:
        art_rect(f, ART_MARK, 0.36f, 0.462f, 0.64f, 0.538f);
        art_rect(f, ART_MARK, 0.462f, 0.36f, 0.538f, 0.64f);
        break;
    case 3: {
        const float t[] = {0.5f, 0.35f, 0.64f, 0.61f, 0.36f, 0.61f};
        art_poly(f, ART_MARK, t, 3);
        break;
    }
    case 4: {
        const float d[] = {0.5f, 0.35f, 0.65f, 0.5f, 0.5f, 0.65f, 0.35f, 0.5f};
        art_poly(f, ART_MARK, d, 4);
        break;
    }
    case 5: art_rect(f, ART_MARK, 0.34f, 0.46f, 0.66f, 0.54f); break;
    case 6:
        art_ellipse(f, ART_MARK, 0.415f, 0.5f, 0.062f, 0.062f);
        art_ellipse(f, ART_MARK, 0.585f, 0.5f, 0.062f, 0.062f);
        break;
    default: art_arc(f, ART_MARK, 0.5f, 0.43f, 0.085f, 0.155f, 15, 165); break;
    }
}

/* a code peg seen from above: a round lacquered head, its mark on top */
static void peg_pic(int colour, int size, uint8_t *rgba)
{
    art_style st = COLOURS[colour];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.45f, 0.45f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.31f, 0.31f);
    mark(f, colour);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* the player's token: a peg seen from the side (its dome, its lip, its stem), red lacquer */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_style st = COLOURS[0];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    const float stem[] = {0.43f, 0.50f, 0.57f, 0.50f, 0.555f, 0.83f, 0.5f, 0.90f, 0.445f, 0.83f};
    art_poly(f, ART_PART, stem, 5);
    float dome[2 * 26];
    for (int i = 0; i <= 24; i++) {
        float a = (180 + 7.5f * i) * 3.14159265f / 180;
        dome[2 * i] = 0.5f + 0.34f * cosf(a);
        dome[2 * i + 1] = 0.53f + 0.38f * sinf(a);
    }
    art_poly(f, ART_PART, dome, 25);
    art_ellipse(f, ART_PART, 0.5f, 0.535f, 0.35f, 0.055f);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* a key peg: the house's glossy stone, ebony (right hole) or ivory (right colour); drawn four times larger and
 * shrunk, as the house outline is too wide for so small a piece */
static void key_pic(int ebony, int size, uint8_t *rgba)
{
    enum { K4 = 4 };
    int big = K4 * size;
    uint8_t *b = malloc((size_t)big * big * 4);
    if (!b) return;
    art_counter(COUNTER_STONE, ebony ? 1 : 0, big, b);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < K4; j++)
                for (int i = 0; i < K4; i++) {
                    const uint8_t *p = b + 4 * ((y * K4 + j) * big + x * K4 + i);
                    for (int c = 0; c < 3; c++) acc[c] += p[c] * p[3];
                    acc[3] += p[3];
                }
            uint8_t *o = rgba + 4 * (y * size + x);
            for (int c = 0; c < 3; c++) o[c] = (uint8_t)(acc[3] > 0 ? acc[c] / acc[3] : 0);
            o[3] = (uint8_t)(acc[3] / (K4 * K4));
        }
    free(b);
}

/* ---- the board: the house plank, its holes bored ----------------------------------------------------------------- */
/* a round hole bored in the wood at (cx, cy): dark inside, its wall in shadow at the top left, its far rim lit */
static void bore(uint8_t *o, int w, int h, float cx, float cy, float R)
{
    float bevel = fmaxf(1.0f, 0.2f * R);
    int x0 = (int)floorf(cx - R - bevel - 1), x1 = (int)ceilf(cx + R + bevel + 1);
    int y0 = (int)floorf(cy - R - bevel - 1), y1 = (int)ceilf(cy + R + bevel + 1);
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < h; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < w; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
            if (d > R + bevel) continue;
            uint8_t *p = o + 4 * (y * w + x);
            float slant = (dx + dy) / (d * 1.4142f + 1e-3f);
            float rim = 1 + 0.26f * slant * clampf(1 - (d - R) / bevel, 0, 1);
            float in = 0.40f + 0.16f * (dx + dy) / (1.4142f * R) - 0.10f * clampf(d / R, 0, 1);
            float a = clampf(R - d + 0.5f, 0, 1), k = a * in + (1 - a) * rim;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
        }
}

/* a groove across the wood: a dark line, a light one under it */
static void groove(uint8_t *o, int w, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int first = x1 - x0 > y1 - y0 ? y == y0 : x == x0;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * (first ? 0.72f : 1.10f), 0, 255);
        }
}

/* the geometry of a board of P pegs and N rows in half rows of q pixels, frame f: from its top left corner */
static void code_xy(int P, int N, int q, int f, int row, int i, float *x, float *y)   /* row -1: the code */
{
    (void)P;
    *x = f + (2 * i + 1) * q;
    *y = row < 0 ? f + q : f + 3 * q + (N - 1 - row) * 2 * q + q;
}

static void keys_xy(int P, int N, int q, int f, int row, float *x, float *y)   /* the key holes' centre */
{
    float cx, cy;
    code_xy(P, N, q, f, row, 0, &cx, &cy);
    *x = f + 2 * P * q + 1.5f * q;
    *y = cy;
}

/* key peg k of a row (the ebony ones first: from the top left as seen, the board upright or turned: t) */
static void key_xy(int P, int N, int q, int f, int t, int row, int k, float *x, float *y)
{
    float cx, cy, s = t ? -1.0f : 1.0f;         /* turned: b runs right to left on the screen */
    keys_xy(P, N, q, f, row, &cx, &cy);
    if (P == 4) {                               /* two by two */
        *x = cx + (k & 1 ? 0.42f : -0.42f) * q;
        *y = cy + s * (k & 2 ? 0.42f : -0.42f) * q;
    } else {                                    /* three, then two */
        *x = cx + (k < 3 ? (k - 1) * 0.8f : (k - 3.5f) * 0.8f) * q;
        *y = cy + s * (k < 3 ? -0.42f : 0.42f) * q;
    }
}

static int board_w(int P, int q, int f) { return (2 * P + 3) * q + 2 * f; }
static int board_h(int N, int q, int f) { return (2 * N + 3) * q + 2 * f; }

/* a point (a, b) of the board upright, in its picture turned or not (t): the picture is board_h wide when turned */
static void pic_xy(int N, int q, int f, int t, float a, float b, float *x, float *y)
{
    *x = t ? board_h(N, q, f) - b : a;
    *y = t ? a : b;
}

static void make_board(int P, int N, int q, int f, int t, uint8_t *o)
{
    int w = board_w(P, q, f), h = board_h(N, q, f), W = t ? h : w, H = t ? w : h;
    art_board_n(t ? 2 * N + 3 : 2 * P + 3, t ? 2 * P + 3 : 2 * N + 3, q, f, 0, o);
    int gy = f + (int)(2.5f * q), gx = f + 2 * P * q;
    if (!t) {
        groove(o, W, f, gy, w - f - 1, gy + 1);         /* under the code */
        groove(o, W, gx, gy + 2, gx + 1, h - f - 1);    /* the key holes apart */
    } else {                                            /* the same, the board turned */
        groove(o, W, h - gy - 2, f, h - gy - 1, w - f - 1);
        groove(o, W, f, gx, h - gy - 3, gx + 1);
    }
    float a, b, x, y;
    for (int i = 0; i < P; i++) {
        code_xy(P, N, q, f, -1, i, &a, &b);
        pic_xy(N, q, f, t, a, b, &x, &y);
        bore(o, W, H, x, y, HOLE_R * q);
    }
    for (int r = 0; r < N; r++) {
        for (int i = 0; i < P; i++) {
            code_xy(P, N, q, f, r, i, &a, &b);
            pic_xy(N, q, f, t, a, b, &x, &y);
            bore(o, W, H, x, y, HOLE_R * q);
        }
        for (int k = 0; k < P; k++) {
            key_xy(P, N, q, f, t, r, k, &a, &b);
            pic_xy(N, q, f, t, a, b, &x, &y);
            bore(o, W, H, x, y, KEY_R * q);
        }
    }
}

/* the tray: a narrow plank, a cup for each colour, top to bottom (row: left to right) */
static void make_tray(int K, int p, int f, int row, uint8_t *o)
{
    int w = (row ? K : 1) * p + 2 * f, h = (row ? 1 : K) * p + 2 * f;
    art_board_n(row ? K : 1, row ? 1 : K, p, f, 0, o);
    for (int c = 0; c < K; c++)
        bore(o, w, h, f + (row ? c + 0.5f : 0.5f) * p, f + (row ? 0.5f : c + 0.5f) * p, 0.40f * p);
}

/* the lid over the code: a small plank in its walnut frame, as wide as the board's inside */
static int lid_f(int q) { return (int)(0.42f * q + 0.5f); }

/* a soft shadow: a rounded rectangle, its edge blurred over b pixels */
static void soft_rect(int w, int h, float b, float alpha, uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = fminf(x + 0.5f, w - x - 0.5f), dy = fminf(y + 0.5f, h - y - 0.5f);
            float k = clampf(dx / b, 0, 1) * clampf(dy / b, 0, 1);
            uint8_t *p = o + 4 * (y * w + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(255 * alpha * k * k * (3 - 2 * k));
        }
}

/* ---- the state --------------------------------------------------------------------------------------------------- */
static sc_game G;
static int var = 1, P = 4, N = 10, K = 6;
static int cursor;                              /* the hole the next colour goes in */
static struct { int on; int row; Uint32 t0; } shown;   /* the last guess checked: its key pegs come one by one */
static struct { int on, colour, hole, moved; float x, y, x0, y0; } hold;  /* a finger on a tray peg or a hole */

static int q, fr, pp, pf, made = -1, made_bar = -1;
static int turned, trow;                        /* the board lying (its rows across); the tray a row under it */
static float bx, by, tx, ty;                    /* the board's and the tray's top left corners */
static box last_box;
static SDL_Texture *t_board, *t_tray, *t_lid, *t_lid_shadow, *t_peg[SC_MAXCOLOURS], *t_tpeg[SC_MAXCOLOURS],
    *t_key[2], *t_row;

static void layout(box b);

static int status(void)
{
    int s = sc_status(&G);
    return s == SC_SOLVED ? GS_WIN0 : s == SC_FAILED ? GS_WIN1 : GS_PLAY;
}

static int turn(void) { return 0; }

static void start(int variant)
{
    var = variant < 0 || variant >= SC_VARIANTS ? 1 : variant;
    int8_t code[SC_MAXPEGS];
    sc_make_code(var, ui_random, code);
    sc_new(&G, var, code);
    P = G.v.pegs;
    N = G.v.rows;
    K = G.v.colours;
    cursor = 0;
    shown.on = 0;
    hold.on = 0;
    if (made >= 0) layout(last_box);            /* another variant: another board */
}

/* the next empty hole after h (round the row), or h itself when the row is full */
static int next_empty(int h)
{
    for (int k = 1; k <= P; k++)
        if (G.guess[G.n][(h + k) % P] < 0) return (h + k) % P;
    return h;
}

static void place(int hole, int colour)
{
    if (!sc_put(&G, hole, colour)) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
    cursor = next_empty(hole);
    ui_animate();
}

static int lid_end(void)                        /* when the lid has slid away (the key pegs shown first) */
{
    int n = G.n ? G.black[G.n - 1] + G.white[G.n - 1] : 0;
    return 150 + n * KEY_STEP + 250 + LID_MS;
}

static void check(void)
{
    if (!sc_check(&G)) { ui_sound(SND_DENY); return; }
    shown.on = 1;
    shown.row = G.n - 1;
    shown.t0 = ui_ms();
    int n = G.black[G.n - 1] + G.white[G.n - 1];
    for (int k = 0; k < n; k++) ui_sound_later(SND_PLACE, 150 + k * KEY_STEP);
    if (!n) ui_sound(SND_TAP);
    cursor = 0;
    ui_animate();
}

static int undo(void)
{
    int h = sc_undo(&G);
    if (h < 0) return 0;
    cursor = h;
    hold.on = 0;
    return 1;
}

/* "RRGB YWKB": the guesses, each checked once full; a shorter last word stays in the row being filled */
static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        int i = 0;
        for (; *s && *s != ' '; s++, i++)
            if (i >= P || !sc_put(&G, i, sc_colour(*s))) return 0;
        if (i == P && !sc_check(&G)) return 0;
        if (i) ui_sound(SND_PLACE);
    }
    shown.on = 0;
    G.nh = 0;                                   /* the pegs of the moves: not undone one by one */
    cursor = next_empty(P - 1);
    return 1;
}

static int busy(void) { return shown.on && sc_status(&G) != SC_PLAY && (int)(ui_ms() - shown.t0) < lid_end(); }

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the board's and the tray's size on the screen */
static float board_sw(void) { return (float)(turned ? board_h(N, q, fr) : board_w(P, q, fr)); }
static float board_sh(void) { return (float)(turned ? board_w(P, q, fr) : board_h(N, q, fr)); }
static float tray_sw(void) { return (float)((trow ? K : 1) * pp + 2 * pf); }
static float tray_sh(void) { return (float)((trow ? 1 : K) * pp + 2 * pf); }

/* the board in all the room: upright or lying (turned), the tray beside it or a row under it, whichever gives the
 * bigger holes (lying only when they come out clearly bigger) */
static void layout(box b)
{
    last_box = b;
    float u = ui.unit, gap = 0.45f * u, pp0 = 1.12f * u, T0 = 1.5f * pp0, best = 0;
    float aw = 2 * P + 3 + 2 * FRAME, ah = 2 * N + 3 + 2 * FRAME;   /* the board upright, in half rows */
    for (int t = 0; t < 2; t++)
        for (int r = 0; r < 2; r++) {
            float sw = t ? ah : aw, sh = t ? aw : ah;
            float k = fminf((b.w - (r ? 0 : T0 + gap)) / sw, (b.h - (r ? T0 + gap : 0)) / sh) * (t ? 0.96f : 1);
            if (k > best) best = k, turned = t, trow = r;
        }
    float sw = turned ? ah : aw, sh = turned ? aw : ah;
    q = (int)floorf(0.97f * fminf((b.w - (trow ? 0 : T0 + gap)) / sw, (b.h - (trow ? T0 + gap : 0)) / sh));
    fr = (int)(FRAME * q + 0.5f);
    int w = board_w(P, q, fr), h = board_h(N, q, fr);
    float bw = board_sw(), bh = board_sh();
    float thick = trow ? b.h - bh - gap : b.w - bw - gap, len = trow ? b.w : b.h;   /* the tray's room */
    pp = (int)floorf(fminf(fminf(thick / 1.5f, 0.97f * len / (K + 0.5f)), fmaxf(pp0, fminf(1.5f * u, PEG * q / 0.82f))));
    pf = (int)(0.25f * pp + 0.5f);
    if (trow) {                                 /* the board, the tray under it: centred together */
        float all = bh + gap + tray_sh();
        bx = floorf(b.x + (b.w - bw) / 2);
        by = floorf(b.y + (b.h - all) / 2);
        tx = floorf(b.x + (b.w - tray_sw()) / 2);
        ty = by + bh + gap;
    } else {                                    /* the tray at the board's right */
        float all = bw + gap + tray_sw();
        bx = floorf(b.x + (b.w - all) / 2);
        by = floorf(b.y + (b.h - bh) / 2);
        tx = bx + bw + gap;
        ty = floorf(b.y + (b.h - tray_sh()) / 2);
    }
    int key = ((q * 4096 + pp) * 4 + var) * 4 + turned * 2 + trow;   /* the pictures follow the sizes, the variant */
    if (made == key) return;
    made = key;
    int W = turned ? h : w, H = turned ? w : h;
    ui_free(&t_board);
    make_board(P, N, q, fr, turned, ui_buf(W, H));
    t_board = ui_tex(W, H, ui_buf(W, H));
    int tw = (int)tray_sw(), th = (int)tray_sh();
    ui_free(&t_tray);
    make_tray(K, pp, pf, trow, ui_buf(tw, th));
    t_tray = ui_tex(tw, th, ui_buf(tw, th));
    int lf = lid_f(q), lw = (2 * P + 3) * q + 2 * lf, lh = 2 * q + 2 * lf;
    if (turned) { int s = lw; lw = lh; lh = s; }
    ui_free(&t_lid);
    art_board_n(turned ? 2 : 2 * P + 3, turned ? 2 * P + 3 : 2, q, lf, 0, ui_buf(lw, lh));
    t_lid = ui_tex(lw, lh, ui_buf(lw, lh));
    int sw2 = lw + 2 * q, sh2 = lh + 2 * q;
    ui_free(&t_lid_shadow);
    soft_rect(sw2, sh2, 1.1f * q, 0.5f, ui_buf(sw2, sh2));
    t_lid_shadow = ui_tex(sw2, sh2, ui_buf(sw2, sh2));
    int d = (int)(PEG * q), dt = (int)(0.82f * pp), dk = (int)(KEY * q + 0.5f);
    for (int c = 0; c < SC_MAXCOLOURS; c++) {
        ui_free(&t_peg[c]);
        ui_free(&t_tpeg[c]);
        if (c >= K) continue;
        peg_pic(c, d, ui_buf(d, d));
        t_peg[c] = ui_tex(d, d, ui_buf(d, d));
        peg_pic(c, dt, ui_buf(dt, dt));
        t_tpeg[c] = ui_tex(dt, dt, ui_buf(dt, dt));
    }
    for (int p = 0; p < 2; p++) {               /* the key pegs: ebony (right hole), ivory (right colour) */
        ui_free(&t_key[p]);
        key_pic(!p, dk, ui_buf(dk, dk));
        t_key[p] = ui_tex(dk, dk, ui_buf(dk, dk));
    }
}

/* the bar's picture: a row of the board (the guesses left) */
static void bar_textures(void)
{
    int key = (int)ui.unit;
    if (made_bar == key) return;
    made_bar = key;
    int s = (int)(0.6f * ui.unit), rq = s / 10 > 1 ? s / 10 : 1, rf = rq;   /* a row: four holes in a strip */
    int rw = 9 * rq + 2 * rf, rh = 2 * rq + 2 * rf;
    uint8_t *b = ui_buf(s, s);
    memset(b, 0, (size_t)s * s * 4);
    uint8_t *strip = calloc((size_t)rw * rh, 4);
    if (strip) {
        art_board_n(9, 2, rq, rf, 0, strip);
        for (int i = 0; i < 4; i++) bore(strip, rw, rh, rf + (2 * i + 1.5f) * rq, rf + rq, 0.75f * rq);
        art_blend(b, s, s, strip, rw, rh, (s - rw) / 2, (s - rh) / 2);
        free(strip);
    }
    ui_free(&t_row);
    t_row = ui_tex(s, s, b);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a point (a, b) of the board upright: where it is on the screen */
static void at(float a, float b, float *x, float *y)
{
    pic_xy(N, q, fr, turned, a, b, x, y);
    *x += bx;
    *y += by;
}

/* a rectangle of the board upright (its top left (a, b), w across a row, h down the rows): on the screen */
static void rect_at(float a, float b, float w, float h, float *x, float *y, float *sw, float *sh)
{
    *x = bx + (turned ? board_h(N, q, fr) - (b + h) : a);
    *y = by + (turned ? a : b);
    *sw = turned ? h : w;
    *sh = turned ? w : h;
}

static void hole_at(int row, int i, float *x, float *y)
{
    float a, b;
    code_xy(P, N, q, fr, row, i, &a, &b);
    at(a, b, x, y);
}

/* the screen's point (x, y) on the board upright */
static void board_ab(float x, float y, float *a, float *b)
{
    *a = turned ? y - by : x - bx;
    *b = turned ? board_h(N, q, fr) - (x - bx) : y - by;
}

static float ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* the check button in the row's key holes: there when the row is full */
static void key_button(float *x, float *y, float *r)
{
    float a, b;
    keys_xy(P, N, q, fr, G.n, &a, &b);
    at(a, b, x, y);
    *r = 0.9f * q;
}

static void tray_xy(int c, float *x, float *y)  /* a cup's centre */
{
    *x = tx + pf + (trow ? c + 0.5f : 0.5f) * pp;
    *y = ty + pf + (trow ? 0.5f : c + 0.5f) * pp;
}

static void draw(void)
{
    float x, y, w, h, a, b, d = PEG * q, dk = KEY * q;
    int st = sc_status(&G), elapsed = shown.on ? (int)(ui_ms() - shown.t0) : 1 << 30;
    if (shown.on && elapsed < lid_end()) ui_animate();
    ui_put(t_board, bx, by, board_sw(), board_sh());
    float lid = 0;                              /* the lid: 0 shut, 1 gone */
    if (st != SC_PLAY) lid = shown.on ? ease((elapsed - (lid_end() - LID_MS)) / (float)LID_MS) : 1;
    if (st == SC_SOLVED && lid >= 1)            /* the code found: the winning row and the code in gold light */
        for (int i = 0; i < 2 * P; i++) {
            hole_at(i < P ? G.n - 1 : -1, i % P, &x, &y);
            ui_mark_gold(x, y, 3.6f * q);
        }
    if (st == SC_PLAY) {                        /* the row being filled: a warm light */
        code_xy(P, N, q, fr, G.n, 0, &a, &b);
        rect_at((float)fr, b - q, (float)(2 * P + 3) * q, 2.0f * q, &x, &y, &w, &h);
        ui_mark_last(x, y, w, h);
        hole_at(G.n, cursor, &x, &y);           /* where the next colour goes: its light around the peg */
        ui_mark_select_round(x, y, 2.1f * q);
    }
    for (int r = 0; r <= G.n && r < N; r++)     /* the guesses, and the row being filled */
        for (int i = 0; i < P; i++) {
            int c = G.guess[r][i];
            if (c < 0) continue;
            hole_at(r, i, &x, &y);
            ui_flat_at(t_peg[c], x, y, d, 1, 0);
        }
    for (int r = 0; r < G.n; r++) {             /* their key pegs: the ebony ones first */
        int nb = G.black[r], n = nb + G.white[r];
        for (int k = 0; k < n; k++) {
            float al = 1, s = 1;
            if (shown.on && r == shown.row) {
                float t = (elapsed - 150 - k * KEY_STEP) / 110.0f;
                if (t <= 0) continue;
                al = clampf(t, 0, 1);
                s = 1.35f - 0.35f * ease(t);    /* set down from a little above */
            }
            key_xy(P, N, q, fr, turned, r, k, &a, &b);
            at(a, b, &x, &y);
            ui_flat_at(t_key[k >= nb], x, y, dk * s, al, 0);
        }
    }
    if (lid > 0)
        for (int i = 0; i < P; i++) {           /* the code, under the lid */
            hole_at(-1, i, &x, &y);
            ui_flat_at(t_peg[G.code[i]], x, y, d, 1, 0);
        }
    if (lid < 1) {                              /* it slides off along its length (to the left; lying: up), fading */
        int lf = lid_f(q);
        float lw = (float)(2 * P + 3) * q + 2 * lf, lh = 2.0f * q + 2 * lf;
        rect_at(fr - lf - lid * (lw + fr), (float)(fr - lf), lw, lh, &x, &y, &w, &h);
        Uint8 al = (Uint8)(255 * clampf(1 - (lid - 0.5f) * 2, 0, 1));
        SDL_SetTextureAlphaMod(t_lid_shadow, al);
        ui_put(t_lid_shadow, x - q + 0.25f * q, y - q + 0.4f * q, w + 2 * q, h + 2 * q);
        SDL_SetTextureAlphaMod(t_lid, al);
        ui_put(t_lid, x, y, w, h);
        SDL_SetTextureAlphaMod(t_lid, 255);
    }
    if (st == SC_PLAY && sc_full(&G)) {         /* the row full: the check in its key holes */
        float r;
        key_button(&x, &y, &r);
        ui_button(x, y, 2 * r, ICON_TICK, UI_ON);
    }
    ui_tool(0, ICON_TICK, st == SC_PLAY && sc_full(&G) ? UI_ON : UI_DIM);   /* the shell's tool: the same check */
    ui_put(t_tray, tx, ty, tray_sw(), tray_sh());
    for (int c = 0; c < K; c++) {               /* the tray's pegs (the one held lifted out) */
        float al = hold.on && hold.colour == c && hold.moved ? 0.35f : 1;
        tray_xy(c, &x, &y);
        ui_flat_at(t_tpeg[c], x, y, 0.82f * pp, al, 0);
    }
    if (hold.on && hold.colour >= 0 && hold.moved) {   /* the peg carried, above the finger */
        float s = 0.9f * pp;
        ui_shadow_flat(hold.x + 0.12f * s, hold.y - 0.35f * s, s, s, 0.8f);
        ui_put_c(t_tpeg[hold.colour], hold.x, hold.y - 0.5f * s, s, s);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int tray_at(float x, float y)            /* the tray's colour under the pointer; -1: none */
{
    float along = trow ? x - tx : y - ty, across = trow ? y - ty : x - tx;
    if (across < 0 || across >= pp + 2 * pf || along < pf * 0.5f || along >= K * pp + 1.5f * pf) return -1;
    int c = (int)floorf((along - pf) / pp);
    return c < 0 ? 0 : c >= K ? K - 1 : c;
}

/* the row being filled's hole under the pointer (reach: how far from the row's middle, in half rows); -1: none */
static int hole_under(float x, float y, float reach)
{
    float a, b;
    board_ab(x, y, &a, &b);
    for (int i = 0; i < P; i++) {
        float ha, hb;
        code_xy(P, N, q, fr, G.n, i, &ha, &hb);
        if (fabsf(a - ha) < q && fabsf(b - hb) < reach * q) return i;
    }
    return -1;
}

static void press(float x, float y)
{
    hold.on = 0;
    if (sc_status(&G) != SC_PLAY) return;
    if (ui_tool_hit(x, y) == 0) {               /* the shell's tool: the check */
        if (sc_full(&G)) check();
        else ui_sound(SND_DENY);
        return;
    }
    int c = tray_at(x, y), h = hole_under(x, y, 1.0f);
    float kx, ky, kr;
    key_button(&kx, &ky, &kr);
    if (c < 0 && h < 0 && sc_full(&G) && hypotf(x - kx, y - ky) < 1.4f * kr) {   /* the row's own check */
        check();
        return;
    }
    if (c < 0 && h < 0) {                       /* elsewhere on the board: not a place to play */
        if (x >= bx && y >= by && x < bx + board_sw() && y < by + board_sh()) ui_sound(SND_DENY);
        return;
    }
    hold.on = 1;
    hold.colour = c;
    hold.hole = h;
    hold.moved = 0;
    hold.x = hold.x0 = x;
    hold.y = hold.y0 = y;
}

static void motion(float x, float y)
{
    if (!hold.on) return;
    hold.x = x;
    hold.y = y;
    if (hold.colour >= 0 && hypotf(x - hold.x0, y - hold.y0) > 0.3f * pp) hold.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!hold.on) return;
    hold.on = 0;
    ui_animate();
    if (hold.colour >= 0) {
        if (!hold.moved) { place(cursor, hold.colour); return; }   /* a tap in the tray: at the cursor */
        int h = hole_under(x, y - 0.5f * 0.9f * pp, 1.3f);         /* dropped: where the peg is, above the finger */
        if (h < 0) h = hole_under(x, y, 1.0f);
        if (h >= 0) place(h, hold.colour);
        return;
    }
    if (hole_under(x, y, 1.0f) != hold.hole) return;
    if (hold.hole != cursor) { cursor = hold.hole; ui_sound(SND_TAP); return; }   /* a tap on a hole: the cursor */
    int c = G.guess[G.n][cursor];               /* a tap on the cursor: its peg's next colour */
    if (!sc_put(&G, cursor, (c + 1) % K)) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
}

/* ---- the bar: the guesses left (the check is the shell's tool) --------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    bar_textures();
    if (sc_status(&G) != SC_PLAY) return;
    ui_bar_count(b, t_row, sc_left(&G));
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
/* a corner of the board: a few guesses and their key pegs, the lid over the code, the tray beside it */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const char *const rows[SC_VARIANTS][4] = {      /* the guesses from the bottom, the last being filled */
        {"RYGB", "YRWK", "WKYG", "KB.."}, {"RRGB", "RBYY", "YBBR", "BB.."}, {"RRYGB", "PSRRY", "SPKYS", "SP..."}};
    static const int keyb[SC_VARIANTS][3][2] = {{{1, 2}, {0, 3}, {2, 1}}, {{1, 1}, {1, 2}, {2, 2}},
                                                 {{1, 2}, {2, 1}, {3, 1}}};
    int v = variant < 0 ? 1 : variant, p = sc_variants[v].pegs, k = sc_variants[v].colours, n = 5;
    float fw = 2.0f * p + 3 + 2 * FRAME, fh = 2.0f * n + 3 + 2 * FRAME, ft = fh / (k + 0.5f);   /* in half rows */
    int q2 = (int)(0.98f * size / fmaxf(fw + 1 + 1.5f * ft, fh));
    int f = (int)(FRAME * q2 + 0.5f), w = board_w(p, q2, f), h = board_h(n, q2, f);
    int tp = (int)(ft * q2), tf = (int)(0.25f * tp + 0.5f);
    int all = w + q2 + tp + 2 * tf, ox = (size - all) / 2, oy = (size - h) / 2;
    uint8_t *one = calloc((size_t)size * size, 4);
    if (!one) return;
    make_board(p, n, q2, f, 0, one);
    art_blend(rgba, size, size, one, w, h, ox, oy);
    int d = (int)(PEG * q2), dk = (int)(KEY * q2 + 0.5f);
    float x, y;
    for (int r = 0; r < 4; r++)
        for (int i = 0; i < p; i++) {
            int c = sc_colour(rows[v][r][i]);
            if (c < 0) continue;
            code_xy(p, n, q2, f, r, i, &x, &y);
            peg_pic(c, d, one);
            art_blend(rgba, size, size, one, d, d, ox + (int)(x - d / 2.0f), oy + (int)(y - d / 2.0f));
        }
    for (int r = 0; r < 3; r++)
        for (int j = 0; j < keyb[v][r][0] + keyb[v][r][1]; j++) {
            key_xy(p, n, q2, f, 0, r, j, &x, &y);
            key_pic(j < keyb[v][r][0], dk, one);
            art_blend(rgba, size, size, one, dk, dk, ox + (int)(x - dk / 2.0f), oy + (int)(y - dk / 2.0f));
        }
    int lf = lid_f(q2), lw = (2 * p + 3) * q2 + 2 * lf, lh = 2 * q2 + 2 * lf;
    art_board_n(2 * p + 3, 2, q2, lf, 0, one);
    art_blend(rgba, size, size, one, lw, lh, ox + f - lf, oy + f - lf);
    int tw = tp + 2 * tf, th = k * tp + 2 * tf, tx0 = ox + w + q2, ty0 = (size - th) / 2;
    make_tray(k, tp, tf, 0, one);
    art_blend(rgba, size, size, one, tw, th, tx0, ty0);
    int dt = (int)(0.82f * tp);
    for (int c = 0; c < k; c++) {
        peg_pic(c, dt, one);
        art_blend(rgba, size, size, one, dt, dt, tx0 + tf + (tp - dt) / 2, ty0 + tf + c * tp + (tp - dt) / 2);
    }
    free(one);
}

static const game secretcode_game = {
    .id = "secretcode", .order = 255, .variants = SC_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
};
REGISTER_GAME(secretcode_game)
