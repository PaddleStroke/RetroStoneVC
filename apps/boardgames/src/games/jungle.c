/*
 * Jungle (Dou Shou Qi) in RetroStone Board: the board with its two rivers, the traps and the dens; the animals on
 * ivory and ebony discs, each with its rank; tap or drag, the lion's and the tiger's jumps across the water (the
 * rules: jungle_rules.c, jungle.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation (the screenshots, the tests): a move is its two squares, files a..g from player 0's left,
 * ranks 1..9 from player 0's side: "g3g4 a7a6 g1g2" (a jump: "b3b7"; jungle.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/jungle.h"
#include "games/trays.h"
#include "shell.h"
#include "text.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define DISC 0.88f                              /* a piece's size, in squares */
#define STEP_MS 150
#define JUMP_MS 300
#define PI_F 3.14159265f

static jggame G;
static jgmove legal[JG_MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the squares' top left corner (inside the frame) */
static SDL_Texture *t_piece[2][9], *t_small[2][9], *t_board;
static int sel = -1;
static trays T;                                 /* the animals each player took, by their side */
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; jgmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = jg_legal(jg_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    jg_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    switch (jg_status(&G)) {
    case JS_PLAY: return GS_PLAY;
    case JS_DEN: case JS_ALL: case JS_STUCK: return jg_pos(&G)->side ? GS_WIN0 : GS_WIN1;   /* the mover lost */
    default: return GS_DRAW;
    }
}

static int turn(void) { return jg_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int jump(jgmove m) { return abs(m.to - m.from) != 1 && abs(m.to - m.from) != JG_W; }

static void play(jgmove m, int animate)
{
    if (!jg_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.taken ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!jg_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "g3g4 a7a6 b3b7": the screenshots' and the tests' moves (jungle.h) */
static int script(const char *s)
{
    int ok = jg_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the animals ------------------------------------------------------------------------------------------------- */
/* Each animal is a silhouette seen from the side, facing right, drawn in a 100 x 80 box (y down, its feet on y 78),
 * with cut-outs (an eye, the tiger's stripes, the leopard's spots) through which the disc shows. */
#define AS 0.006f                               /* the box's unit, in the piece's unit square */
#define AX 0.20f
#define AY 0.165f

static void a_poly(art_fig *f, const float *xy, int n)
{
    float p[96];
    for (int i = 0; i < n && i < 48; i++) {
        p[2 * i] = AX + xy[2 * i] * AS;
        p[2 * i + 1] = AY + xy[2 * i + 1] * AS;
    }
    art_poly(f, ART_PART, p, n);
}

static void a_ell(art_fig *f, float cx, float cy, float rx, float ry)
{
    art_ellipse(f, ART_PART, AX + cx * AS, AY + cy * AS, rx * AS, ry * AS);
}

static void a_arc(art_fig *f, float cx, float cy, float r0, float r1, float deg0, float deg1)
{
    art_arc(f, ART_PART, AX + cx * AS, AY + cy * AS, r0 * AS, r1 * AS, deg0, deg1);
}

/* a hind leg (x: its back edge at the top, w: its width): a thigh narrowing to the hock, a foot forwards */
static void a_hind(art_fig *f, float x, float top, float w)
{
    const float p[] = {x - 2, top, x + w + 2, top, x + w * 0.85f, 60, x + w * 0.62f, 66, x + w * 0.62f, 75,
                       x + w * 1.0f, 78, x - w * 0.02f, 78, x + w * 0.02f, 68, x - w * 0.2f, 60};
    a_poly(f, p, 9);
}

/* a front leg: straight, slimmer at the wrist, a paw forwards */
static void a_front(art_fig *f, float x, float top, float w)
{
    const float p[] = {x, top, x + w, top, x + w * 0.86f, 74, x + w * 1.25f, 78, x + w * 0.04f, 78,
                       x + w * 0.1f, 72};
    a_poly(f, p, 6);
}

static void elephant(art_fig *b, art_fig *c)
{
    a_ell(b, 40, 38, 31, 22);
    a_ell(b, 73, 28, 15, 16);
    const float trunk[] = {82, 32, 90, 40, 94, 52, 95, 64, 99, 74, 97, 78, 89, 78, 87, 66, 85, 54, 80, 46, 72, 42};
    a_poly(b, trunk, 11);
    const float legs[][2] = {{12, 50}, {29, 54}, {50, 54}, {65, 42}};
    for (int i = 0; i < 4; i++) {
        const float p[] = {legs[i][0], legs[i][1], legs[i][0] + 13, legs[i][1], legs[i][0] + 12.5f, 77,
                           legs[i][0] + 12, 78, legs[i][0] + 0.5f, 78, legs[i][0], 77};
        a_poly(b, p, 6);
    }
    const float tail[] = {11, 32, 4, 46, 7, 47, 14, 36};
    a_poly(b, tail, 4);
    a_arc(c, 68, 28, 11, 13.5f, 95, 255);       /* the ear's edge */
    a_ell(c, 78, 23, 2.6f, 2.6f);
    const float tusk[] = {78, 42, 90, 47, 92, 44, 82, 38};
    a_poly(c, tusk, 4);
}

static void lion(art_fig *b, art_fig *c)
{
    a_ell(b, 38, 44, 27, 13);
    a_hind(b, 12, 46, 11);
    a_hind(b, 24, 48, 10);
    a_front(b, 50, 50, 9);
    a_front(b, 60, 48, 9);
    const float tail[] = {12, 40, 5, 31, 3, 20, 6, 19, 9, 29, 15, 36};
    a_poly(b, tail, 6);
    a_ell(b, 5, 15, 5, 6.5f);
    float mane[2 * 22];
    for (int i = 0; i < 22; i++) {              /* the mane: a shaggy star round the head */
        float a = i * 2 * PI_F / 22, r = i & 1 ? 21 : 27;
        mane[2 * i] = 68 + r * cosf(a);
        mane[2 * i + 1] = 30 + r * sinf(a);
    }
    a_poly(b, mane, 22);
    a_ell(b, 86, 34, 9, 9);
    a_ell(b, 93, 38, 5, 5);
    a_arc(c, 82, 33, 12, 14.5f, 100, 260);      /* the face's edge in the mane */
    a_ell(c, 88, 29, 2.4f, 2.4f);
}

static void tiger(art_fig *b, art_fig *c)
{
    a_ell(b, 40, 44, 30, 13);
    a_hind(b, 12, 46, 11);
    a_hind(b, 24, 48, 10);
    a_front(b, 52, 50, 9);
    a_front(b, 62, 48, 9);
    const float tail[] = {11, 40, 4, 48, 2, 60, 6, 68, 10, 67, 7, 59, 9, 50, 15, 44};
    a_poly(b, tail, 8);
    a_ell(b, 78, 34, 13, 12);
    const float ruff[] = {68, 30, 76, 42, 70, 50, 82, 46, 90, 46, 96, 40, 92, 32, 78, 24};
    a_poly(b, ruff, 8);
    a_ell(b, 72, 22, 5, 5.5f);
    for (int i = 0; i < 5; i++) {               /* the stripes */
        float x = 20 + i * 9.5f;
        const float s[] = {x, 31, x + 4.5f, 31, x + 2.5f, 44, x - 0.5f, 48};
        a_poly(c, s, 4);
    }
    const float s1[] = {76, 21, 79, 21, 78, 29}, s2[] = {68, 29, 71, 28, 72, 36};
    a_poly(c, s1, 3);
    a_poly(c, s2, 3);
    const float s3[] = {2, 58, 7, 56, 8, 59, 3, 61};
    a_poly(c, s3, 4);
    a_ell(c, 85, 31, 2.4f, 2.4f);
}

static void leopard(art_fig *b, art_fig *c)    /* slim, low, its long tail curled up at the end */
{
    a_ell(b, 46, 46, 30, 10.5f);
    a_hind(b, 18, 48, 9);
    a_hind(b, 29, 50, 8.5f);
    a_front(b, 57, 50, 7.5f);
    a_front(b, 66, 48, 7.5f);
    const float tail[] = {18, 42, 10, 50, 5, 62, 4, 72, 9, 78, 15, 76, 12, 72, 9, 71, 9, 63, 13, 53, 20, 46};
    a_poly(b, tail, 11);
    const float neck[] = {64, 40, 76, 32, 84, 38, 76, 50};
    a_poly(b, neck, 4);
    a_ell(b, 82, 36, 10, 9);
    a_ell(b, 91, 40, 5, 4.5f);
    a_ell(b, 78, 27, 3.5f, 4);
    static const float spot[][2] = {{25, 42}, {34, 48}, {37, 40}, {46, 46}, {50, 39}, {58, 46}, {64, 40}, {28, 51},
                                    {42, 53}, {70, 46}};
    for (int i = 0; i < 10; i++) a_ell(c, spot[i][0], spot[i][1], 2.6f, 2.3f);
    a_ell(c, 86, 33, 2.2f, 2.2f);
}

static void wolf(art_fig *b, art_fig *c)        /* sitting, howling at the sky */
{
    a_ell(b, 36, 62, 20, 16);
    const float chest[] = {34, 48, 48, 26, 56, 14, 70, 16, 72, 32, 68, 48, 68, 78, 56, 78, 54, 62, 40, 70};
    a_poly(b, chest, 10);
    a_ell(b, 63, 17, 10, 9);
    const float snout[] = {60, 12, 80, 0, 85, 4, 74, 20, 66, 24};
    a_poly(b, snout, 5);
    const float ear[] = {53, 15, 48, 0, 62, 10};
    a_poly(b, ear, 3);
    const float ruff[] = {50, 30, 46, 42, 54, 36, 52, 48, 60, 40};
    a_poly(b, ruff, 5);
    const float tail[] = {22, 64, 6, 66, 0, 72, 2, 78, 30, 78};
    a_poly(b, tail, 5);
    const float paw[] = {16, 74, 60, 74, 72, 78, 16, 78};
    a_poly(b, paw, 4);
    a_ell(c, 65, 14, 2.4f, 2.4f);
    const float leg[] = {55, 60, 57, 60, 58, 75, 56, 75};
    a_poly(c, leg, 4);
}

static void dog(art_fig *b, art_fig *c)
{
    a_ell(b, 44, 44, 25, 12);
    a_hind(b, 20, 46, 10);
    a_hind(b, 31, 48, 9);
    a_front(b, 55, 50, 8.5f);
    a_front(b, 64, 46, 8.5f);
    const float head[] = {58, 40, 62, 22, 70, 13, 80, 14, 86, 19, 97, 23, 98, 31, 84, 34, 74, 42};
    a_poly(b, head, 9);
    const float tail[] = {21, 38, 13, 28, 10, 13, 14, 12, 18, 25, 27, 34};
    a_poly(b, tail, 6);
    const float ear[] = {68, 17, 74, 16, 73, 33, 67, 34};
    a_poly(c, ear, 4);
    a_ell(c, 81, 20, 2.4f, 2.4f);
}

static void cat(art_fig *b, art_fig *c)         /* sitting, its tail curled up behind */
{
    const float body[] = {30, 78, 27, 64, 32, 50, 42, 40, 56, 40, 63, 50, 66, 64, 67, 78};
    a_poly(b, body, 8);
    a_ell(b, 56, 29, 13, 12);
    const float ear_l[] = {44, 25, 45, 6, 56, 18}, ear_r[] = {57, 18, 68, 6, 69, 25};
    a_poly(b, ear_l, 3);
    a_poly(b, ear_r, 3);
    a_arc(b, 23, 60, 13, 18, 150, 285);
    a_ell(b, 8, 52, 4.2f, 4.2f);
    a_ell(c, 51, 28, 2.4f, 2.6f);
    a_ell(c, 61, 28, 2.4f, 2.6f);
    const float leg[] = {54, 60, 56, 60, 56, 78, 54, 78};
    a_poly(c, leg, 4);
}

static void rat(art_fig *b, art_fig *c)
{
    a_ell(b, 48, 60, 25, 14);
    const float head[] = {62, 50, 78, 52, 97, 64, 95, 68, 76, 72, 64, 72};
    a_poly(b, head, 6);
    a_ell(b, 70, 46, 7.5f, 8.5f);
    a_front(b, 31, 66, 7);
    a_front(b, 58, 66, 7);
    a_arc(b, 18, 47, 14, 18.5f, 175, 305);      /* the long tail, curling up */
    a_ell(c, 70, 47, 3.8f, 4.8f);
    a_ell(c, 83, 58, 2.2f, 2.2f);
}

static void (*const ANIMAL[9])(art_fig *, art_fig *) = {0, rat, cat, dog, wolf, leopard, tiger, lion, elephant};

/* a piece: the player's disc, the animal on it in the other player's colour, its rank below it; size x size */
static void paint_piece(int pc, int size, uint8_t *rgba)
{
    int player = JG_OWNER(pc), rank = JG_RANK(pc);
    static const float INK[2][3] = {{66, 48, 38}, {238, 228, 208}};
    art_counter(COUNTER_DISC, player, size, rgba);
    size_t n = (size_t)size * size * 4;
    uint8_t *fig = malloc(n), *cut = malloc(n);
    if (!fig || !cut) { free(fig); free(cut); return; }
    art_style st = {.light = 0.12f};
    memcpy(st.fill, INK[player], sizeof st.fill);
    art_fig *b = art_fig_new(), *c = art_fig_new();
    ANIMAL[rank](b, c);
    art_render(b, &st, size, size, (float)size, 0, 0, fig);
    art_render(c, &st, size, size, (float)size, 0, 0, cut);
    for (size_t i = 3; i < n; i += 4) fig[i] = (uint8_t)(fig[i] * (255 - cut[i]) / 255);
    art_blend(rgba, size, size, fig, size, size, 0, 0);
    int h = (int)(0.15f * size + 0.5f), w = (int)(0.62f * h + 3);   /* the rank: the font's bold digit */
    if (h >= 4) {
        const uint8_t ink[3] = {(uint8_t)INK[player][0], (uint8_t)INK[player][1], (uint8_t)INK[player][2]};
        memset(fig, 0, (size_t)w * h * 4);
        text_glyph('0' + rank, h * 1.2f, 1, ink, fig, w, h);
        art_blend(rgba, size, size, fig, w, h, (int)(0.5f * size - w / 2.0f + 0.5f), (int)(0.665f * size));
    }
    art_fig_free(b);
    art_fig_free(c);
    free(fig);
    free(cut);
}

/* ---- the board --------------------------------------------------------------------------------------------------- */
/* paints a figure (in the board's pixels, inside the square of side s round (cx, cy)) over the board w x h, moved by
 * (dx, dy), its alpha scaled */
static void put_fig(art_fig *f, const art_style *st, float alpha, float cx, float cy, float s, float dx, float dy,
                    uint8_t *o, int w, int h)
{
    int X = (int)floorf(cx - s / 2) - 1, Y = (int)floorf(cy - s / 2) - 1, n = (int)ceilf(s) + 3;
    uint8_t *tile = malloc((size_t)n * n * 4);
    if (!tile) return;
    art_render(f, st, n, n, 1, dx - X, dy - Y, tile);
    for (size_t i = 3; i < (size_t)n * n * 4; i += 4) tile[i] = (uint8_t)(tile[i] * alpha);
    art_blend(o, w, h, tile, n, n, X, Y);
    free(tile);
}

static void ink(art_fig *f, const float rgb[3], float alpha, float cx, float cy, float s, float off, uint8_t *o,
                int w, int h)
{
    art_style st = {.light = 0};
    memcpy(st.mark, rgb, sizeof st.mark);
    put_fig(f, &st, alpha, cx, cy, s, off, off, o, w, h);
}

/* a river: x0, y0 .. x1, y1 (pixels), calm blue-green water, darker by its banks, soft ripples and a shine */
static void paint_water(uint8_t *o, int w, int clip0x, int clip0y, int clip1x, int clip1y,
                        float x0, float y0, float x1, float y1, float s)
{
    float rad = 0.2f * s, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2, hy = (y1 - y0) / 2;
    for (int y = clip0y; y < clip1y; y++)
        for (int x = clip0x; x < clip1x; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float qx = fabsf(px - cx) - hx + rad, qy = fabsf(py - cy) - hy + rad;
            float d = hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;   /* < 0 inside */
            float a = clampf(0.5f - d, 0, 1);
            if (a <= 0) continue;
            float v = (py - y0) / (y1 - y0), u = (px - x0) / (x1 - x0);
            float c[3] = {78 - 26 * v, 150 - 36 * v, 150 - 30 * v};
            float rip = sinf((py + 0.12f * s * sinf(px / s * 4.1f + py / s * 0.7f)) / s * 11.0f);
            float lift = 0.06f * fmaxf(rip - 0.55f, 0) / 0.45f;
            float shine = 0.22f * expf(-((u - 0.3f) * (u - 0.3f) * 10 + (v - 0.16f) * (v - 0.16f) * 28));
            float bank = clampf(-d / (0.14f * s), 0, 1);
            bank = 0.62f + 0.38f * sqrtf(bank);
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) {
                float col = c[i] * bank * (1 + lift) + (255 - c[i]) * shine;
                p[i] = (uint8_t)clampf(p[i] + (col - p[i]) * a, 0, 255);
            }
        }
}

/* a straight groove from (ax, ay) to (bx, by), width wd, round ends */
static void groove(art_fig *f, float ax, float ay, float bx2, float by2, float wd)
{
    float len = hypotf(bx2 - ax, by2 - ay), nx = -(by2 - ay) / len * wd / 2, ny = (bx2 - ax) / len * wd / 2;
    const float band[] = {ax + nx, ay + ny, bx2 + nx, by2 + ny, bx2 - nx, by2 - ny, ax - nx, ay - ny};
    art_poly(f, ART_MARK, band, 4);
    art_ellipse(f, ART_MARK, ax, ay, wd / 2, wd / 2);
    art_ellipse(f, ART_MARK, bx2, by2, wd / 2, wd / 2);
}

/* a trap: a carved diamond with a smaller one inside (dark in the cut, lit on its lower edge) */
static void paint_trap(float cx, float cy, float s, uint8_t *o, int w, int h)
{
    static const float dark[3] = {96, 62, 38}, lit[3] = {255, 238, 206};
    art_fig *f = art_fig_new();
    float wd = fmaxf(1.2f, 0.045f * s);
    for (int k = 0; k < 2; k++) {
        float r = k ? 0.13f * s : 0.30f * s;
        groove(f, cx, cy - r, cx + r, cy, wd);
        groove(f, cx + r, cy, cx, cy + r, wd);
        groove(f, cx, cy + r, cx - r, cy, wd);
        groove(f, cx - r, cy, cx, cy - r, wd);
    }
    float off = fmaxf(0.8f, 0.018f * s);
    ink(f, lit, 0.35f, cx, cy, s, off, o, w, h);
    ink(f, dark, 0.42f, cx, cy, s, 0, o, w, h);
    art_fig_free(f);
}

/* a den: an inlaid disc of walnut with a maple star of eight points */
static void paint_den(float cx, float cy, float s, uint8_t *o, int w, int h)
{
    art_style wal = {{120, 78, 50}, {70, 44, 28}, {70, 44, 28}, {0, 0, 0}, 0.18f, fmaxf(1, 0.03f * s)};
    art_style map = {{240, 214, 168}, {92, 60, 38}, {92, 60, 38}, {0, 0, 0}, 0.10f, fmaxf(0.8f, 0.018f * s)};
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, cx, cy, 0.40f * s, 0.40f * s);
    put_fig(f, &wal, 1, cx, cy, s, 0, 0, o, w, h);
    art_fig_free(f);
    f = art_fig_new();
    float star[2 * 16];
    for (int i = 0; i < 16; i++) {
        float a = i * PI_F / 8 - PI_F / 2, r = (i & 1 ? 0.12f : i & 2 ? 0.22f : 0.32f) * s;
        star[2 * i] = cx + r * cosf(a);
        star[2 * i + 1] = cy + r * sinf(a);
    }
    art_poly(f, ART_PART, star, 16);
    put_fig(f, &map, 1, cx, cy, s, 0, 0, o, w, h);
    art_fig_free(f);
}

/* the board, or a part of it (the thumbnail): files f0.. (nx of them), screen rows r0.. from the top (ny), squares of
 * s pixels in a frame of fr0; (nx s + 2 fr0) x (ny s + 2 fr0) */
static void paint_board(int f0, int r0, int nx, int ny, int s, int fr0, uint8_t *o)
{
    int w = nx * s + 2 * fr0, h = ny * s + 2 * fr0;
    art_board_n(nx, ny, s, fr0, 1, o);
    for (int k = 0; k < 2; k++) {               /* the rivers: files b..c and e..f, ranks 4..6 (rows 3..5) */
        float x0 = fr0 + (1 + 3 * k - f0) * s + 0.04f * s, y0 = fr0 + (3 - r0) * s + 0.04f * s;
        paint_water(o, w, fr0, fr0, fr0 + nx * s, fr0 + ny * s, x0, y0, x0 + 2 * s - 0.08f * s,
                    y0 + 3 * s - 0.08f * s, (float)s);
    }
    for (int sq0 = 0; sq0 < JG_SQUARES; sq0++) {
        int c = JG_FILE(sq0) - f0, r = 8 - JG_ROW(sq0) - r0;
        if (c < 0 || c >= nx || r < 0 || r >= ny) continue;
        float cx = fr0 + (c + 0.5f) * s, cy = fr0 + (r + 0.5f) * s;
        if (jg_trap(sq0) >= 0) paint_trap(cx, cy, (float)s, o, w, h);
        if (jg_den(sq0) >= 0) paint_den(cx, cy, (float)s, o, w, h);
    }
}

/* the board as big as the room lets it be (the room's width in portrait: taller than the square); the animals each
 * player took in a tray by their side, in the room it leaves (above and below it, or beside it) */
static void layout(box b)
{
    sq = (int)floorf(fminf(b.h / (JG_H + 2 * FRAME), b.w / (JG_W + 2 * FRAME)));
    fr = (int)(sq * FRAME);
    int w = JG_W * sq + 2 * fr, h = JG_H * sq + 2 * fr;
    trays_place(&T, b, (float)w, (float)h, 8, (float)sq, 0.7f * ui.unit, &bx, &by);
    bx += fr;
    by += fr;
    if (made != sq) {
        made = sq;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)w * h * 4);
        if (o) {
            paint_board(0, 0, JG_W, JG_H, sq, fr, o);
            t_board = ui_tex(w, h, o);
            free(o);
        }
        int d = (int)(DISC * sq);
        for (int c = 0; c < 2; c++)
            for (int r = JG_RAT; r <= JG_ELEPHANT; r++) {
                ui_free(&t_piece[c][r]);
                paint_piece(JG_PC(c, r), d, ui_buf(d, d));
                t_piece[c][r] = ui_tex(d, d, ui_buf(d, d));
            }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++)
            for (int r = JG_RAT; r <= JG_ELEPHANT; r++) {
                ui_free(&t_small[c][r]);
                paint_piece(JG_PC(c, r), small, ui_buf(small, small));
                t_small[c][r] = ui_tex(small, small, ui_buf(small, small));
            }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void sq_xy(int s, float *x, float *y)    /* a square's centre */
{
    int c = JG_FILE(s), r = 8 - JG_ROW(s);
    if (ui.flipped) { c = 6 - c; r = 8 - r; }
    *x = bx + (c + 0.5f) * sq;
    *y = by + (r + 0.5f) * sq;
}

static int xy_sq(float x, float y)
{
    int c = (int)floorf((x - bx) / sq), r = (int)floorf((y - by) / sq);
    if (c < 0 || c >= JG_W || r < 0 || r >= JG_H) return -1;
    if (ui.flipped) { c = 6 - c; r = 8 - r; }
    return JG_SQ(c, 8 - r);
}

static void piece_at(int pc, float cx, float cy, float d, float alpha)
{
    ui_flat_at(t_piece[JG_OWNER(pc)][JG_RANK(pc)], cx, cy, d, alpha, ui_upside(JG_OWNER(pc)));
}

static void mark_square(int s, void (*mark)(float, float, float, float))
{
    float x, y;
    sq_xy(s, &x, &y);
    mark(x - sq / 2.0f, y - sq / 2.0f, (float)sq, (float)sq);
}

/* the animals the player took (gone[rank] 1: the other player's animal is off the board); their number */
static int taken(int player, int gone[9])
{
    const jgpos *p = jg_pos(&G);
    int c = !player, n = 0;
    for (int r = 0; r <= JG_ELEPHANT; r++) gone[r] = r >= JG_RAT;
    for (int s = 0; s < JG_SQUARES; s++)
        if (p->sq[s] && JG_OWNER(p->sq[s]) == c) gone[JG_RANK(p->sq[s])] = 0;
    for (int r = JG_RAT; r <= JG_ELEPHANT; r++) n += gone[r];
    return n;
}

/* the animals each player took, in the tray by their side, the strongest first, turned to face them; the one just
 * taken joins them once it has faded from the board */
static void draw_trays(void)
{
    if (!trays_on(&T)) return;
    for (int c = 0; c < 2; c++) {
        int gone[9], i = 0;
        taken(c, gone);
        if (anim.on && anim.m.taken && JG_OWNER(anim.m.taken) != c) gone[JG_RANK(anim.m.taken)] = 0;
        for (int r = JG_ELEPHANT; r >= JG_RAT; r--) {
            if (!gone[r]) continue;
            float x, y;
            trays_slot(&T, c, i++, &x, &y);
            ui_flat_at(t_piece[!c][r], x, y, DISC * T.pitch, 1, ui_upside(c));
        }
    }
}

static void draw(void)
{
    const jgpos *p = jg_pos(&G);
    float d = DISC * sq, x, y;
    ui_put(t_board, bx - fr, by - fr, JG_W * sq + 2.0f * fr, JG_H * sq + 2.0f * fr);
    draw_trays();
    if (G.ply) {
        mark_square(G.move[G.ply - 1].from, ui_mark_last);
        mark_square(G.move[G.ply - 1].to, ui_mark_last);
    }
    if (sel >= 0) mark_square(sel, ui_mark_select);
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(jump(anim.m) ? JUMP_MS : STEP_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    for (int s = 0; s < JG_SQUARES; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, d, 1);
    }
    if (sel >= 0)                               /* where the selected animal can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (legal[i].taken) ui_mark_ring(x, y, (float)sq);
            else ui_mark_dot(x, y, 0.32f * sq);
        }
    if (anim.on) {
        float x0, y0;
        sq_xy(anim.m.to, &x, &y);
        if (anim.m.taken && e < 1) piece_at(anim.m.taken, x, y, d, 1 - e);   /* the animal taken fades */
        sq_xy(anim.m.from, &x0, &y0);
        float hop = jump(anim.m) ? 1 + 0.18f * sinf(PI_F * t) : 1;           /* a jump: up and over the water */
        piece_at(p->sq[anim.m.to], x0 + (x - x0) * e, y0 + (y - y0) * e, d * hop, 1);
    }
    if (drag.on && drag.moved) {                /* the dragged animal, a little bigger, beside the finger */
        float s = 1.18f * d, off = ui_upside(p->side) ? -0.55f : 0.55f;
        piece_at(p->sq[drag.from], drag.x, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const jgpos *p = jg_pos(&G);
    int s = xy_sq(x, y), i;
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && (i = target(sel, s)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->sq[s] && JG_OWNER(p->sq[s]) == p->side && can_move(s)) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (s != sel) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the animal stays selected */
    int s = xy_sq(x, y), i = s >= 0 ? target(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the animals a player took, the strongest first (in the bar when there are no trays) */
static void bar(int player, box b)
{
    if (trays_on(&T)) return;
    int c = !player, gone[9], n = taken(player, gone);
    float sz = 0.6f * ui.unit, x = b.x, cy = b.y + b.h / 2;
    float step = n > 1 ? fminf(0.66f * ui.unit, (b.w - sz) / (n - 1)) : 0;
    for (int r = JG_ELEPHANT; r >= JG_RAT; r--) {
        if (!gone[r]) continue;
        ui_shadow_flat(x + sz / 2, cy, sz, sz, 0.8f);
        ui_put(t_small[c][r], x, cy - sz / 2, sz, sz);
        x += step;
    }
}

static void token(int player, int size, uint8_t *rgba) { paint_piece(JG_PC(player, JG_ELEPHANT), size, rgba); }

/* the menu's card: a corner of the board by a river, a few animals */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    int d = (int)(DISC * s);
    uint8_t *board = malloc((size_t)n * n * 4), *one = malloc((size_t)d * d * 4);
    if (!board || !one) { free(board); free(one); return; }
    paint_board(0, 2, 4, 4, s, f, board);       /* files a..d, ranks 7..4 */
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{0, 0, JG_PC(1, JG_TIGER)}, {3, 1, JG_PC(1, JG_LION)}, {2, 2, JG_PC(0, JG_RAT)},
                                   {0, 3, JG_PC(0, JG_ELEPHANT)}, {3, 3, JG_PC(0, JG_LEOPARD)}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        float cx = o + f + (place[i][0] + 0.5f) * s, cy = o + f + (place[i][1] + 0.5f) * s;
        paint_piece(place[i][2], d, one);
        art_flat_put(rgba, size, size, one, d, cx, cy);
    }
    free(board);
    free(one);
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to arrive */

static const game jungle_game = {
    .id = "jungle", .order = 190, .variants = 1, .fill = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(jungle_game)
