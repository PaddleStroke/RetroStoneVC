/*
 * Torch Maze in RetroStone Board, for one player: a maze of stone seen from above, in the dark, in the house's frame.
 * A little mouse carries a torch: its warm, flickering light shows the flagstones and the walls round it and fades
 * softly into the dark, the walls casting their shadows. What the light has shown stays on the map as a chalk sketch
 * (in the last chapter it fades after a while). The exit, stairs going down, glows gold through the dark. Keys of red
 * lacquer, jade and lapis open the doors of their colour; plates of stone turn the iron gates; one-way doors swing one
 * way, a brass arrow on the floor; braziers, once lit, light their part of the maze for good; oil flasks fill the
 * torch, whose light shrinks as it burns (the rules: torchmaze_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the maze, tap an arrow (the shell's tools) or use the arrow keys: the mouse walks along the corridor, round
 * its bends, to the next junction, room, door or thing. A tap beside it: one step that way. A step it cannot take: it
 * bumps and steps back. The lamp: the hint (a trail of gold light along the next steps of a shortest way, to the exit
 * or to what must be fetched first; from a dead end it first takes back the moves that led there). Undo takes back a
 * move (never the map). The bar: the steps, the oil left (levels with fuel), the par by its flag. Solved: the mouse
 * goes down the stairs in a swell of gold light.
 * A maze too big for readable cells is drawn at that size, and the view follows the mouse.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "games/torchmaze.h"
#include "shell.h"

#define WT 0.13f                                /* half a wall's thickness, of a cell */
#define TOL (WT + 0.005f)                       /* the light lights the top of a wall it meets */
#define STEP_MS 105
#define WALK0_MS 90
#define BUMP_MS 220
#define POP_MS 650
#define GATE_MS 320
#define WIN_MS 1200
#define HINT_MS 3200
#define BRAZ_R 3.2f
#define SUB 9                                   /* a cell's light patch: SUB x SUB vertices */

static const uint8_t DARK[3] = {13, 11, 10}, CHALK[3] = {236, 230, 212}, GOLD[3] = {246, 202, 92},
                     BRASS[3] = {214, 170, 84}, IRON[3] = {58, 56, 60}, WOOD[3] = {120, 78, 44},
                     WOOD_DARK[3] = {70, 44, 26};
static const int KEYCOL[TMZ_COLOURS] = {2, 3, 5};   /* red lacquer, jade, lapis */
static const float VU[SUB] = {0, WT - 0.01f, WT + 0.01f, 0.36f, 0.5f, 0.64f, 1 - WT - 0.01f, 1 - WT + 0.01f, 1};

static tmz_game G;
static int serial, theme, down = -1;
static swp_swipe sw;
static tmz_state before;                        /* the state as the walk began: what the cells show until it ends */
static struct { int on, n, from; int16_t cells[TMZ_PATH]; Uint32 t0, len; } walk;
static struct { int on, dir; Uint32 t0; } bump;
static struct { int on, cell, what, k; Uint32 t0; } pop;
static struct { int on; Uint32 t0; } win, gate_turn;
static struct { int on, n; int16_t cells[16]; Uint32 t0; } hint;

static box last_box;
static float ox, oy, cell, fr;                  /* the maze's picture's top left on the screen, a cell, the frame */
static int scroll, made_cell = -1, made_serial = -1, made_small = -1;
static box fbox;                                /* the frame's box on the screen */
static SDL_Texture *t_lit, *t_chalk, *t_frame, *t_mouse, *t_stick, *t_key[TMZ_COLOURS], *t_ckey, *t_door[TMZ_COLOURS],
                   *t_oneway, *t_gate, *t_plate, *t_oil, *t_coil, *t_braz, *t_par, *t_drop;

static SDL_Vertex *vbuf;
static int *ibuf;

/* ---- colours --------------------------------------------------------------------------------------------------- */
static void player_rgb(int p, uint8_t rgb[3])
{
    for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)art_player[p].fill[i];
}

static void floor_rgb(int th, float *rgb)
{
    static const float F[6][3] = {{196, 170, 128}, {170, 166, 158}, {146, 160, 126},
                                  {190, 146, 124}, {138, 134, 140}, {138, 150, 168}};
    for (int i = 0; i < 3; i++) rgb[i] = F[th % 6][i];
}

/* ---- painting helpers ------------------------------------------------------------------------------------------ */
static float sstep(float a, float b, float x)
{
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

/* a smooth noise, -0.5..0.5, its blobs s pixels across (the stone's mottling) */
static float vnoise(int x, int y, float s)
{
    float fx = x / fmaxf(s, 1), fy = y / fmaxf(s, 1);
    int ix = (int)floorf(fx), iy = (int)floorf(fy);
    float u = fx - ix, v = fy - iy;
    u = u * u * (3 - 2 * u);
    v = v * v * (3 - 2 * v);
    float a = art_hash(ix, iy), b = art_hash(ix + 1, iy), c = art_hash(ix, iy + 1), d = art_hash(ix + 1, iy + 1);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v - 0.5f;
}

/* a box's coverage at a pixel: the box x0..x1, y0..y1 (pixels), its edge soft over s */
static float box_cov(float px, float py, float x0, float y0, float x1, float y1, float s)
{
    float dx = fmaxf(x0 - px, px - x1), dy = fmaxf(y0 - py, py - y1);
    float d = hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0);
    return clampf(0.5f - d / s, 0, 1);
}

static void mask_box(float *mk, int W, int H, float x0, float y0, float x1, float y1, float s)
{
    for (int y = (int)floorf(y0 - s); y <= (int)ceilf(y1 + s); y++)
        for (int x = (int)floorf(x0 - s); x <= (int)ceilf(x1 + s); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float c = box_cov(x + 0.5f, y + 0.5f, x0, y0, x1, y1, s);
            if (c > mk[y * W + x]) mk[y * W + x] = c;
        }
}

/* the walls of a maze as a mask (cells c pixels, from (x0, y0) of the picture): the edges that are walls, their
 * corners joined, the rock filled; grow: pixels added round them (a shadow), soft: their edges' softness */
static void wall_mask(const tmz_map *m, float c, float X0, float Y0, float grow, float soft, float *mk, int W, int H,
                      float *rock)
{
    float t = WT * c + grow;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int k = x + y * m->w;
            float px = X0 + x * c, py = Y0 + y * c;
            if (!m->in[k]) {
                mask_box(rock ? rock : mk, W, H, px - grow, py - grow, px + c + grow, py + c + grow, soft);
                continue;
            }
            if (x == 0 || !m->in[k - 1]) mask_box(mk, W, H, px - t, py - t, px + t, py + c + t, soft);
            if (y == 0 || !m->in[k - m->w]) mask_box(mk, W, H, px - t, py - t, px + c + t, py + t, soft);
            if (m->e[0][k] != TMZ_OPEN || x == m->w - 1 || !m->in[k + 1]) {
                if (m->e[0][k] == TMZ_WALL || x == m->w - 1 || !m->in[k + 1])
                    mask_box(mk, W, H, px + c - t, py - t, px + c + t, py + c + t, soft);
                else {                          /* a door's or a gate's frame: the posts at its ends */
                    mask_box(mk, W, H, px + c - t, py - t, px + c + t, py + t, soft);
                    mask_box(mk, W, H, px + c - t, py + c - t, px + c + t, py + c + t, soft);
                }
            }
            if (m->e[1][k] != TMZ_OPEN || y == m->h - 1 || !m->in[k + m->w]) {
                if (m->e[1][k] == TMZ_WALL || y == m->h - 1 || !m->in[k + m->w])
                    mask_box(mk, W, H, px - t, py + c - t, px + c + t, py + c + t, soft);
                else {
                    mask_box(mk, W, H, px - t, py + c - t, px + t, py + c + t, soft);
                    mask_box(mk, W, H, px + c - t, py + c - t, px + c + t, py + c + t, soft);
                }
            }
        }
}

/* the exit: stairs going down into the dark, a stone rim round them (into the picture at the cell's top left) */
static void paint_stairs(uint8_t *o, int W, int H, float x0, float y0, float c, const float *fl)
{
    float in = WT * c + 0.02f * c, s = c - 2 * in;
    for (int y = (int)(y0 + in); y < (int)(y0 + in + s) && y < H; y++)
        for (int x = (int)(x0 + in); x < (int)(x0 + in + s) && x < W; x++) {
            if (x < 0 || y < 0) continue;
            float v = (y + 0.5f - y0 - in) / s, u = (x + 0.5f - x0 - in) / s;
            int step = (int)(v * 5);
            float f = step >= 4 ? 0.12f : 0.95f - step * 0.2f, edge = fmodf(v * 5, 1);
            if (edge < 0.18f) f *= 1.15f;       /* each step's lit nose */
            f *= 1 - 0.25f * fabsf(u - 0.5f) * 2;
            uint8_t rgb[3];
            for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)clampf(fl[i] * f * (1 + 0.06f * art_noise(x, y)), 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), rgb, 1);
        }
    pzk_stroke(o, W, H, x0 + in, y0 + in, x0 + in + s, y0 + in, 0.03f * c, GOLD, 0.6f);
}

/* the maze lit: flagstones, the walls' shadows, the walls' stone tops, the rock, the stairs */
static void paint_lit(const tmz_map *m, float c, int W, int H, uint8_t *o, int th)
{
    float fl[3], wl[3], rk[3];
    floor_rgb(th, fl);
    for (int i = 0; i < 3; i++) {
        static const float STONE[3] = {232, 220, 196};
        wl[i] = fl[i] * 0.3f + STONE[i] * 0.7f;
        rk[i] = fl[i] * 0.36f;
        fl[i] *= 0.74f;
    }
    float *wall = calloc((size_t)W * H, sizeof *wall), *shade = calloc((size_t)W * H, sizeof *shade),
          *rock = calloc((size_t)W * H, sizeof *rock);
    if (!wall || !shade || !rock) { free(wall); free(shade); free(rock); return; }
    wall_mask(m, c, 0, 0, 0, 1.2f, wall, W, H, rock);
    wall_mask(m, c, 0.05f * c, 0.08f * c, 0.03f * c, 0.10f * c, shade, W, H, NULL);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int cx = x / (int)c, cy = y / (int)c;
            if (cx >= m->w) cx = m->w - 1;
            if (cy >= m->h) cy = m->h - 1;
            float u = (x + 0.5f) / c - cx, v = (y + 0.5f) / c - cy, n = art_noise(x, y),
                  big = vnoise(x, y, 0.09f * c), rgb[3];
            float tint = 0.92f + 0.16f * art_hash(cx * 7 + 3, cy * 13 + th);
            /* a flagstone: its seam, its bevel */
            float e = fminf(fminf(u, 1 - u), fminf(v, 1 - v)), f = tint * (1 + 0.03f * n + 0.12f * big);
            if (e < 0.035f) f *= 0.62f;
            else if (e < 0.09f) f *= (u < v ? (u < 1 - v) : (v < 1 - u)) ? 1.08f : 0.93f;
            for (int i = 0; i < 3; i++) rgb[i] = fl[i] * f;
            float sh = shade[y * W + (x)] * 0.55f;
            for (int i = 0; i < 3; i++) rgb[i] *= 1 - sh;
            float wv = wall[y * W + x], rv = rock[y * W + x];
            if (rv > 0) {
                float rf = 0.85f + 0.3f * big + 0.05f * n;
                for (int i = 0; i < 3; i++) rgb[i] += (rk[i] * rf - rgb[i]) * rv;
            }
            if (wv > 0) {                       /* the wall's top, embossed: lit at its upper left */
                int e2 = (int)fmaxf(2, 0.035f * c);
                int a = (y - e2 >= 0 && x - e2 >= 0) ? (y - e2) * W + x - e2 : y * W + x,
                    b = (y + e2 < H && x + e2 < W) ? (y + e2) * W + x + e2 : y * W + x;
                float em = (wall[y * W + x] - wall[a]) * 0.3f - (wall[y * W + x] - wall[b]) * 0.4f;
                float rim = 1;                  /* its edge inked dark: the least of the wall a little way round */
                int e1 = (int)fmaxf(1, 0.022f * c);
                if (x >= e1 && y >= e1 && x + e1 < W && y + e1 < H)
                    rim = fminf(fminf(wall[y * W + x - e1], wall[y * W + x + e1]),
                                fminf(wall[(y - e1) * W + x], wall[(y + e1) * W + x]));
                float wf = (0.92f + 0.2f * big + 0.03f * n) * (1 + em) * (0.45f + 0.55f * rim);
                for (int i = 0; i < 3; i++) rgb[i] += (wl[i] * wf - rgb[i]) * wv;
            }
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(rgb[i], 0, 255);
            p[3] = 255;
        }
    paint_stairs(o, W, H, (m->exit % m->w) * c, (m->exit / m->w) * c, c, fl);
    free(wall);
    free(shade);
    free(rock);
}

/* the chalk map: the walls' lines, the rock hatched, the doors, gates and one-way doors marked, the plates, the stairs */
static void paint_chalk(const tmz_map *m, float c, int W, int H, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    float *mk = calloc((size_t)W * H, sizeof *mk);
    if (!mk) return;
    for (int y = 0; y < m->h; y++)              /* the lines: the edges of the walls' mask, thin */
        for (int x = 0; x < m->w; x++) {
            int k = x + y * m->w;
            float px = x * c, py = y * c, t = 0.035f * c;
            if (!m->in[k]) continue;
            if (x == 0 || !m->in[k - 1]) mask_box(mk, W, H, px - t, py - t, px + t, py + c + t, 1.2f);
            if (y == 0 || !m->in[k - m->w]) mask_box(mk, W, H, px - t, py - t, px + c + t, py + t, 1.2f);
            if (m->e[0][k] == TMZ_WALL) mask_box(mk, W, H, px + c - t, py - t, px + c + t, py + c + t, 1.2f);
            if (m->e[1][k] == TMZ_WALL) mask_box(mk, W, H, px - t, py + c - t, px + c + t, py + c + t, 1.2f);
        }
    for (int i = 0; i < W * H; i++) {
        int x = i % W, y = i / W, cx = x / (int)c, cy = y / (int)c;
        float a = mk[i] * (0.62f + 0.38f * (art_noise(x * 3, y * 3) + 0.5f));
        if (cx < m->w && cy < m->h && !m->in[cx + cy * m->w] && ((x + y) % (int)fmaxf(4, 0.16f * c)) < 1.3f)
            a = fmaxf(a, 0.28f);                /* rock: hatched */
        pzk_over(o + 4 * (size_t)i, CHALK, a);
    }
    free(mk);
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < m->w * m->h; k++) {
            int t = m->e[s][k];
            if (t == TMZ_WALL || t == TMZ_OPEN) continue;
            float x = (k % m->w + (s == 0)) * c, y = (k / m->w + (s == 1)) * c;
            float ax = x, ay = y, bx = s == 0 ? x : x + c, by = s == 0 ? y + c : y;
            if (s == 0) { ay += WT * c; by -= WT * c; } else { ax += WT * c; bx -= WT * c; }
            if (t == TMZ_DOOR) {
                uint8_t col[3];
                player_rgb(KEYCOL[m->k[s][k] % 3], col);
                for (int i = 0; i < 3; i++) col[i] = (uint8_t)(col[i] * 0.5f + CHALK[i] * 0.5f);
                pzk_stroke(o, W, H, ax, ay, bx, by, 0.06f * c, col, 0.85f);
            } else if (t == TMZ_GATE) {
                for (int d = 0; d < 4; d++) {
                    float f = (d + 0.5f) / 4;
                    pzk_ball(o, W, H, ax + (bx - ax) * f, ay + (by - ay) * f, 0.035f * c, CHALK, 0.8f, 0);
                }
            } else {                            /* a one-way door: a chevron its way */
                int way = m->k[s][k];
                float mx = (ax + bx) / 2, my = (ay + by) / 2, dx = (float)(way == TMZ_RIGHT) - (way == TMZ_LEFT),
                      dy = (float)(way == TMZ_DOWN) - (way == TMZ_UP), q = 0.16f * c;
                pzk_stroke(o, W, H, ax, ay, bx, by, 0.025f * c, CHALK, 0.6f);
                pzk_stroke(o, W, H, mx + dx * q - dy * q, my + dy * q + dx * q, mx + 2 * dx * q, my + 2 * dy * q,
                           0.03f * c, CHALK, 0.85f);
                pzk_stroke(o, W, H, mx + dx * q + dy * q, my + dy * q - dx * q, mx + 2 * dx * q, my + 2 * dy * q,
                           0.03f * c, CHALK, 0.85f);
            }
        }
    for (int k = 0; k < m->w * m->h; k++) {
        float x = (k % m->w) * c, y = (k / m->w) * c;
        if (m->item[k] == TMZ_PLATE) {
            float a = 0.3f * c, b = 0.7f * c;
            pzk_stroke(o, W, H, x + a, y + a, x + b, y + a, 0.025f * c, CHALK, 0.8f);
            pzk_stroke(o, W, H, x + b, y + a, x + b, y + b, 0.025f * c, CHALK, 0.8f);
            pzk_stroke(o, W, H, x + b, y + b, x + a, y + b, 0.025f * c, CHALK, 0.8f);
            pzk_stroke(o, W, H, x + a, y + b, x + a, y + a, 0.025f * c, CHALK, 0.8f);
        } else if (m->item[k] == TMZ_BRAZIER) {
            pzk_arc(o, W, H, x + 0.5f * c, y + 0.5f * c, 0.17f * c, 0.21f * c, 0, 360, CHALK, 0.8f);
        } else if (m->item[k] == TMZ_EXIT) {
            for (int i = 0; i < 3; i++) {
                float yy = y + (0.32f + 0.17f * i) * c, w2 = (0.22f - 0.04f * i) * c;
                pzk_stroke(o, W, H, x + 0.5f * c - w2, yy, x + 0.5f * c + w2, yy, 0.028f * c, GOLD, 0.9f);
            }
        }
    }
}

/* ---- the sprites --------------------------------------------------------------------------------------------- */
static art_fig *fig(void) { return art_fig_new(); }

/* the mouse, facing the viewer, its right paw out (the torch's), in a unit square */
static void paint_mouse(uint8_t *o, int S)
{
    static const uint8_t FUR[3] = {158, 136, 120}, FUR_D[3] = {72, 58, 50}, BELLY[3] = {222, 204, 182},
                         PINK[3] = {232, 150, 150}, EYE[3] = {24, 18, 16}, SCARF[3] = {190, 52, 40};
    art_fig *f = fig();                         /* the tail, curling behind */
    art_arc(f, ART_PART, 0.22f, 0.70f, 0.13f, 0.165f, 100, 250);
    pzt_paint(o, S, f, PINK, FUR_D, 0.012f, 0.3f);
    f = fig();
    art_ellipse(f, ART_PART, 0.40f, 0.88f, 0.09f, 0.05f);   /* the feet */
    art_ellipse(f, ART_PART, 0.60f, 0.88f, 0.09f, 0.05f);
    pzt_paint(o, S, f, PINK, FUR_D, 0.015f, 0.3f);
    f = fig();
    art_ellipse(f, ART_PART, 0.50f, 0.68f, 0.22f, 0.21f);   /* the body */
    pzt_paint(o, S, f, FUR, FUR_D, 0.022f, 0.6f);
    f = fig();
    art_ellipse(f, ART_PART, 0.50f, 0.72f, 0.13f, 0.14f);   /* the belly */
    pzt_paint(o, S, f, BELLY, BELLY, 0, 0.3f);
    f = fig();
    art_ellipse(f, ART_PART, 0.27f, 0.17f, 0.13f, 0.13f);   /* the ears */
    art_ellipse(f, ART_PART, 0.73f, 0.17f, 0.13f, 0.13f);
    pzt_paint(o, S, f, FUR, FUR_D, 0.022f, 0.5f);
    f = fig();
    art_ellipse(f, ART_PART, 0.27f, 0.18f, 0.075f, 0.075f);
    art_ellipse(f, ART_PART, 0.73f, 0.18f, 0.075f, 0.075f);
    pzt_paint(o, S, f, PINK, PINK, 0, 0.3f);
    f = fig();
    art_ellipse(f, ART_PART, 0.50f, 0.37f, 0.21f, 0.19f);   /* the head */
    pzt_paint(o, S, f, FUR, FUR_D, 0.022f, 0.6f);
    f = fig();
    art_arc(f, ART_PART, 0.50f, 0.36f, 0.17f, 0.215f, 205, 335);   /* the scarf */
    pzt_paint(o, S, f, SCARF, FUR_D, 0.014f, 0.5f);
    f = fig();
    art_ellipse(f, ART_PART, 0.42f, 0.35f, 0.035f, 0.045f);   /* the eyes */
    art_ellipse(f, ART_PART, 0.58f, 0.35f, 0.035f, 0.045f);
    pzt_paint(o, S, f, EYE, EYE, 0, 0.2f);
    pzk_ball(o, S, S, 0.43f * S, 0.335f * S, 0.013f * S, PUZ_WHITE, 0.95f, 0);
    pzk_ball(o, S, S, 0.59f * S, 0.335f * S, 0.013f * S, PUZ_WHITE, 0.95f, 0);
    pzk_ball(o, S, S, 0.36f * S, 0.43f * S, 0.04f * S, PINK, 0.4f, 0);   /* the cheeks */
    pzk_ball(o, S, S, 0.64f * S, 0.43f * S, 0.04f * S, PINK, 0.4f, 0);
    pzk_ball(o, S, S, 0.50f * S, 0.42f * S, 0.03f * S, PINK, 1, 0.5f);   /* the nose */
    for (int s = -1; s <= 1; s += 2)            /* the whiskers */
        for (int i = 0; i < 2; i++)
            pzk_stroke(o, S, S, (0.5f + s * 0.06f) * S, (0.43f + 0.02f * i) * S, (0.5f + s * 0.21f) * S,
                       (0.41f + 0.05f * i) * S, 0.006f * S, FUR_D, 0.7f);
    f = fig();                                  /* the paws: the left by its side, the right held up */
    art_ellipse(f, ART_PART, 0.30f, 0.68f, 0.055f, 0.07f);
    art_ellipse(f, ART_PART, 0.71f, 0.60f, 0.06f, 0.06f);
    pzt_paint(o, S, f, FUR, FUR_D, 0.018f, 0.5f);
}

/* the torch's stick (the flame is drawn live): a unit square, the stick upright in its middle */
static void paint_stick(uint8_t *o, int S)
{
    art_fig *f = fig();
    const float q[] = {0.46f, 0.30f, 0.54f, 0.30f, 0.52f, 0.95f, 0.48f, 0.95f};
    art_poly(f, ART_PART, q, 4);
    pzt_paint(o, S, f, WOOD, WOOD_DARK, 0.02f, 0.4f);
    f = fig();
    art_rect(f, ART_PART, 0.43f, 0.24f, 0.57f, 0.36f);   /* the rag wound round its head */
    static const uint8_t RAG[3] = {96, 70, 48};
    pzt_paint(o, S, f, RAG, WOOD_DARK, 0.02f, 0.4f);
}

/* a key: a gem of its colour in a gold bow, a gold shaft and its bit (lying from upper left to lower right) */
static void paint_key(uint8_t *o, int S, int colour, int chalk)
{
    uint8_t gem[3], gold[3] = {226, 182, 82}, ink[3] = {96, 64, 22};
    player_rgb(KEYCOL[colour % 3], gem);
    if (chalk) for (int i = 0; i < 3; i++) gold[i] = ink[i] = CHALK[i], gem[i] = (uint8_t)(gem[i] / 2 + CHALK[i] / 2);
    art_fig *f = fig();
    art_band(f, ART_PART, 0.40f, 0.40f, 0.80f, 0.80f, 0.09f);
    art_band(f, ART_PART, 0.66f, 0.66f, 0.76f, 0.56f, 0.08f);
    art_band(f, ART_PART, 0.74f, 0.74f, 0.84f, 0.64f, 0.08f);
    art_arc(f, ART_PART, 0.32f, 0.32f, 0.10f, 0.19f, 0, 360);
    pzt_paint(o, S, f, gold, ink, chalk ? 0 : 0.02f, chalk ? 0 : 0.5f);
    f = fig();
    art_ellipse(f, ART_PART, 0.32f, 0.32f, 0.095f, 0.095f);
    pzt_paint(o, S, f, gem, ink, chalk ? 0 : 0.015f, chalk ? 0 : 0.7f);
}

/* a door across its doorway (horizontal: the doorway's gap runs left to right): planks, iron bands, its colour's
 * panel, a brass keyhole plate */
static void paint_door(uint8_t *o, int S, int colour)
{
    uint8_t col[3];
    player_rgb(KEYCOL[colour % 3], col);
    art_roundrect_put(o, S, S, (int)(0.02f * S), (int)(0.38f * S), (int)(0.96f * S), (int)(0.24f * S), 0.04f * S,
                      WOOD_DARK, 1);
    art_roundrect_put(o, S, S, (int)(0.04f * S), (int)(0.40f * S), (int)(0.92f * S), (int)(0.20f * S), 0.03f * S, col,
                      1);
    for (int i = 1; i < 4; i++)
        pzk_stroke(o, S, S, (0.04f + 0.23f * i) * S, 0.41f * S, (0.04f + 0.23f * i) * S, 0.59f * S, 0.008f * S,
                   WOOD_DARK, 0.5f);
    pzk_stroke(o, S, S, 0.12f * S, 0.43f * S, 0.88f * S, 0.43f * S, 0.012f * S, PUZ_WHITE, 0.25f);
    pzk_ball(o, S, S, 0.5f * S, 0.5f * S, 0.07f * S, BRASS, 1, 0.6f);
    pzk_ball(o, S, S, 0.5f * S, 0.495f * S, 0.022f * S, DARK, 1, 0);
}

/* a one-way door: a plain wooden flap across the doorway (horizontal) */
static void paint_oneway(uint8_t *o, int S)
{
    art_roundrect_put(o, S, S, (int)(0.04f * S), (int)(0.44f * S), (int)(0.92f * S), (int)(0.12f * S), 0.03f * S,
                      WOOD_DARK, 1);
    art_roundrect_put(o, S, S, (int)(0.06f * S), (int)(0.455f * S), (int)(0.88f * S), (int)(0.09f * S), 0.03f * S,
                      WOOD, 1);
    pzk_ball(o, S, S, 0.10f * S, 0.5f * S, 0.035f * S, IRON, 1, 0.5f);
}

/* a gate: iron bars across the doorway (horizontal), their tips pointed */
static void paint_gate(uint8_t *o, int S)
{
    pzk_stroke(o, S, S, 0.04f * S, 0.5f * S, 0.96f * S, 0.5f * S, 0.035f * S, IRON, 1);
    for (int i = 0; i < 6; i++) {
        float x = (0.1f + 0.16f * i) * S;
        pzk_stroke(o, S, S, x, 0.40f * S, x, 0.60f * S, 0.03f * S, IRON, 1);
        pzk_stroke(o, S, S, x - 0.008f * S, 0.41f * S, x - 0.008f * S, 0.58f * S, 0.008f * S, PUZ_WHITE, 0.25f);
    }
}

/* a pressure plate: a square stone raised a little, a brass rim */
static void paint_plate(uint8_t *o, int S)
{
    static const uint8_t STONE[3] = {150, 140, 124};
    art_roundrect_put(o, S, S, (int)(0.24f * S), (int)(0.26f * S), (int)(0.54f * S), (int)(0.54f * S), 0.06f * S,
                      DARK, 0.45f);
    art_roundrect_put(o, S, S, (int)(0.2f * S), (int)(0.2f * S), (int)(0.6f * S), (int)(0.6f * S), 0.06f * S, BRASS,
                      1);
    art_roundrect_put(o, S, S, (int)(0.25f * S), (int)(0.25f * S), (int)(0.5f * S), (int)(0.5f * S), 0.04f * S, STONE,
                      1);
    pzk_stroke(o, S, S, 0.27f * S, 0.27f * S, 0.72f * S, 0.27f * S, 0.012f * S, PUZ_WHITE, 0.35f);
}

/* an oil flask: a round glass belly of amber oil, a neck, a cork */
static void paint_oil(uint8_t *o, int S, int chalk)
{
    uint8_t oil[3] = {214, 138, 36}, glass[3] = {170, 190, 186}, cork[3] = {150, 104, 62}, ink[3] = {60, 40, 20};
    if (chalk) for (int i = 0; i < 3; i++) oil[i] = glass[i] = cork[i] = ink[i] = CHALK[i];
    art_fig *f = fig();
    art_rect(f, ART_PART, 0.43f, 0.20f, 0.57f, 0.42f);
    pzt_paint(o, S, f, glass, ink, chalk ? 0 : 0.015f, 0.3f);
    f = fig();
    art_ellipse(f, ART_PART, 0.5f, 0.62f, 0.25f, 0.25f);
    pzt_paint(o, S, f, glass, ink, chalk ? 0 : 0.018f, 0.5f);
    if (!chalk) {
        f = fig();
        art_ellipse(f, ART_PART, 0.5f, 0.66f, 0.21f, 0.19f);
        pzt_paint(o, S, f, oil, oil, 0, 0.7f);
        pzk_ball(o, S, S, 0.41f * S, 0.55f * S, 0.05f * S, PUZ_WHITE, 0.55f, 0);
    }
    f = fig();
    art_rect(f, ART_PART, 0.42f, 0.13f, 0.58f, 0.23f);
    pzt_paint(o, S, f, cork, ink, chalk ? 0 : 0.015f, 0.4f);
}

/* a brazier seen from above: an iron bowl on its rim, coals in it (lit: glowing; the flame is live) */
static void paint_braz(uint8_t *o, int S)
{
    pzk_ball(o, S, S, 0.53f * S, 0.55f * S, 0.32f * S, DARK, 0.5f, 0);
    pzk_ball(o, S, S, 0.5f * S, 0.5f * S, 0.32f * S, IRON, 1, 0.6f);
    pzk_ball(o, S, S, 0.5f * S, 0.5f * S, 0.24f * S, (const uint8_t[]){30, 24, 22}, 1, 0);
    for (int i = 0; i < 7; i++) {
        float a = i * 0.9f, r = 0.12f * (i % 3) * S;
        pzk_ball(o, S, S, 0.5f * S + cosf(a) * r, 0.5f * S + sinf(a) * r, 0.065f * S,
                 (const uint8_t[]){64, 54, 50}, 1, 0.6f);
    }
}

static SDL_Texture *sprite(int S, void (*paint)(uint8_t *, int))
{
    uint8_t *o = ui_buf(S, S);
    memset(o, 0, (size_t)S * S * 4);
    paint(o, S);
    return ui_tex(S, S, o);
}

/* ---- layout ------------------------------------------------------------------------------------------------- */
static void make_maze_pictures(void)
{
    int W = (int)(G.m.w * cell), H = (int)(G.m.h * cell);
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_lit(&G.m, cell, W, H, o, theme);
    ui_free(&t_lit);
    t_lit = ui_tex(W, H, o);
    paint_chalk(&G.m, cell, W, H, o);
    ui_free(&t_chalk);
    t_chalk = ui_tex(W, H, o);
    free(o);
}

/* the frame (walnut, or the light theme's maple) round the dark */
static void make_frame(void)
{
    int W = (int)fbox.w, H = (int)fbox.h, f = (int)fr;
    if (W < 2 * f + 2 || H < 2 * f + 2) return;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    art_plank(W - 2 * f, H - 2 * f, f, o);
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float d = fminf(fminf((float)(x - f), (float)(W - f - x)), fminf((float)(y - f), (float)(H - f - y)));
            float k = 1 - 0.5f * clampf(1 - d / (0.5f * cell), 0, 1);   /* the frame's shadow inside */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(DARK[i] * k * (1 + 0.15f * art_noise(x, y)), 0, 255);
        }
    ui_free(&t_frame);
    t_frame = ui_tex(W, H, o);
    free(o);
}

static void make_sprites(void)
{
    int S = (int)fmaxf(16, cell);
    ui_free(&t_mouse); t_mouse = sprite(S, paint_mouse);
    ui_free(&t_stick); t_stick = sprite(S, paint_stick);
    for (int k = 0; k < TMZ_COLOURS; k++) {
        uint8_t *o = ui_buf(S, S);
        memset(o, 0, (size_t)S * S * 4);
        paint_key(o, S, k, 0);
        ui_free(&t_key[k]);
        t_key[k] = ui_tex(S, S, o);
        memset(o, 0, (size_t)S * S * 4);
        paint_door(o, S, k);
        ui_free(&t_door[k]);
        t_door[k] = ui_tex(S, S, o);
    }
    {
        uint8_t *o = ui_buf(S, S);
        memset(o, 0, (size_t)S * S * 4);
        paint_key(o, S, 0, 1);
        ui_free(&t_ckey);
        t_ckey = ui_tex(S, S, o);
        memset(o, 0, (size_t)S * S * 4);
        paint_oil(o, S, 0);
        ui_free(&t_oil);
        t_oil = ui_tex(S, S, o);
        memset(o, 0, (size_t)S * S * 4);
        paint_oil(o, S, 1);
        ui_free(&t_coil);
        t_coil = ui_tex(S, S, o);
    }
    ui_free(&t_oneway); t_oneway = sprite(S, paint_oneway);
    ui_free(&t_gate); t_gate = sprite(S, paint_gate);
    ui_free(&t_plate); t_plate = sprite(S, paint_plate);
    ui_free(&t_braz); t_braz = sprite(S, paint_braz);
}

static void make_small(void)
{
    int small = (int)(0.6f * ui.unit);
    if (made_small == small) return;
    made_small = small;
    uint8_t *o = ui_buf(small, small);
    memset(o, 0, (size_t)small * small * 4);
    art_icon(ICON_PAR, small, o);
    ui_free(&t_par);
    t_par = ui_tex(small, small, o);
    memset(o, 0, (size_t)small * small * 4);
    paint_oil(o, small, 0);
    ui_free(&t_drop);
    t_drop = ui_tex(small, small, o);
}

static void follow(float fx, float fy)          /* the view: the maze centred, or the mouse kept in the middle */
{
    float iw = fbox.w - 2 * fr, ih = fbox.h - 2 * fr, mw = G.m.w * cell, mh = G.m.h * cell;
    float bx = fbox.x + fr, by = fbox.y + fr;
    if (mw <= iw) ox = floorf(bx + (iw - mw) / 2);
    else ox = roundf(clampf(bx + iw / 2 - fx * cell, bx + iw - mw, bx));
    if (mh <= ih) oy = floorf(by + (ih - mh) / 2);
    else oy = roundf(clampf(by + ih / 2 - fy * cell, by + ih - mh, by));
}

static void layout(box b)
{
    last_box = b;
    float cmin = 0.065f * fminf((float)ui.w, (float)ui.h);
    float c = floorf(fminf(b.w / (G.m.w + 0.5f), b.h / (G.m.h + 0.5f)));
    scroll = c < 0.9f * cmin;
    if (scroll) c = floorf(cmin);
    fr = fmaxf(3, floorf(0.25f * c));
    if (scroll) {                               /* the room, or the maze where it is smaller that way */
        float w = fminf(b.w, G.m.w * c + 2 * fr), h = fminf(b.h, G.m.h * c + 2 * fr);
        fbox = (box){floorf(b.x + (b.w - w) / 2), floorf(b.y + (b.h - h) / 2), w, h};
    }
    else {
        float w = G.m.w * c + 2 * fr, h = G.m.h * c + 2 * fr;
        fbox = (box){floorf(b.x + (b.w - w) / 2), floorf(b.y + (b.h - h) / 2), w, h};
    }
    ui_board_used(fbox.x, fbox.y, fbox.w, fbox.h);
    int changed = made_cell != (int)c || made_serial != serial;
    cell = c;
    if (changed) {
        made_cell = (int)c;
        made_serial = serial;
        make_maze_pictures();
        make_sprites();
    }
    make_frame();
    make_small();
    follow(G.s.at % G.m.w + 0.5f, G.s.at / G.m.w + 0.5f);
    if (!vbuf) vbuf = malloc(sizeof *vbuf * (size_t)TMZ_CELLS * SUB * SUB);
    if (!ibuf) ibuf = malloc(sizeof *ibuf * (size_t)TMZ_CELLS * (SUB - 1) * (SUB - 1) * 6);
}

/* ---- the position ------------------------------------------------------------------------------------------ */
static void calm(void)
{
    walk.on = bump.on = pop.on = win.on = gate_turn.on = hint.on = 0;
    down = -1;
    sw.on = 0;
    before = G.s;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!tmz_level(&G, level)) tmz_level(&G, 0);
    theme = level / (TMZ_LEVELS / TMZ_CHAPTERS);
    begin();
}

static void start_random(int variant)
{
    tmz_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    theme = 1 + variant * 2;
    begin();
}

static int solved(void) { return tmz_solved(&G); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.s.steps; }
static int par(void) { return G.par; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.s.steps, G.par), G.hints, 0); }


static int busy(void)
{
    Uint32 now = ui_ms();
    return (walk.on && now - walk.t0 < walk.len) || (win.on && now - win.t0 < WIN_MS);
}

static void set_pop(int c, int what, int k, Uint32 t0)
{
    pop.on = 1;
    pop.cell = c;
    pop.what = what;
    pop.k = k;
    pop.t0 = t0;
}

static int dir_of_cells(int a, int b)
{
    for (int d = 0; d < 4; d++)
        if (tmz_next(&G.m, a, d) == b) return d;
    return -1;
}

/* a move made: the walk along its cells, its footsteps, what happens at its end */
static void moved(const tmz_state *was, const int16_t *cells, int n)
{
    Uint32 now = ui_ms();
    before = *was;
    walk.on = 1;
    walk.from = was->at;
    walk.n = n < TMZ_PATH ? n : TMZ_PATH;
    memcpy(walk.cells, cells, sizeof cells[0] * (size_t)walk.n);
    walk.t0 = now;
    walk.len = WALK0_MS + (Uint32)walk.n * STEP_MS;
    for (int i = 0; i < walk.n; i++)
        snd_play_at(SND_TAP, i % 2 ? 0.16f : 0.22f, (int)(WALK0_MS / 2 + i * STEP_MS));
    int end = G.s.at, t = (int)walk.len;
    pop.on = 0;
    if (G.m.item[end] == TMZ_KEY && !(was->keys >> G.m.ik[end] & 1)) {
        set_pop(end, TMZ_KEY, G.m.ik[end], now + walk.len);
        ui_sound_later(SND_CHECK, t);
    } else if (G.m.item[end] == TMZ_OIL && !(was->flasks >> G.m.ik[end] & 1)) {
        set_pop(end, TMZ_OIL, G.m.ik[end], now + walk.len);
        ui_sound_later(SND_DROP, t);
    } else if (G.m.item[end] == TMZ_PLATE) {
        set_pop(end, TMZ_PLATE, 0, now + walk.len);
        gate_turn.on = 1;
        gate_turn.t0 = now + walk.len;
        ui_sound_later(SND_PLACE, t);
        ui_sound_later(SND_CAPTURE, t + 120);
    } else if (G.m.item[end] == TMZ_BRAZIER) {
        set_pop(end, TMZ_BRAZIER, G.m.ik[end], now + walk.len);
        ui_sound_later(SND_DROP, t);
    }
    if (solved()) {
        win.on = 1;
        win.t0 = now + walk.len;
    }
    hint.on = 0;
    ui_animate();
}

static void go(int dir, int run)
{
    if (solved() || dir < 0) return;
    if (walk.on) walk.on = 0;                   /* a walk still going: it ends at once */
    tmz_state was = G.s;
    int16_t cells[TMZ_PATH];
    int n = 0;
    if (run) tmz_run(&G, dir, cells, &n);
    else if (tmz_step(&G, dir)) { cells[0] = G.s.at; n = 1; }
    if (n <= 0) {
        bump.on = 1;
        bump.dir = dir;
        bump.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    moved(&was, cells, n);
}

static void key(SDL_Keycode k)
{
    int w = swp_key_way(k);
    if (w >= 0) go(w, 1);
}

/* the hint: a trail of gold light along the next steps of a shortest way; from a dead end, back first */
static void show_hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    tmz_way w;
    tmz_solve(&G.m, &G.s, &w);
    while (w.steps < 0 && tmz_undo(&G)) tmz_solve(&G.m, &G.s, &w);
    walk.on = 0;
    before = G.s;
    if (w.steps <= 0) { ui_sound(SND_DENY); return; }
    G.hints++;
    hint.on = 1;
    hint.n = w.n < 12 ? w.n : 12;
    for (int i = 0; i < hint.n; i++) hint.cells[i] = w.path[i];
    hint.t0 = ui_ms();
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    int ok = tmz_undo(&G);
    before = G.s;
    return ok;
}

static int script(const char *moves)
{
    calm();
    char seg[512];
    int n = 0, ok = 1;
    for (const char *p = moves;; p++) {
        if (*p == 'h' || !*p) {
            seg[n] = 0;
            if (n && !tmz_script(&G, seg)) ok = 0;
            n = 0;
            if (!*p) break;
            show_hint();
        } else if (n < (int)sizeof seg - 1) seg[n++] = *p;
    }
    before = G.s;
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the light ----------------------------------------------------------------------------------------------- */
static float tx, ty, trad, flick;               /* the torch (cells), its light's radius now, its flicker */

static float fall(float d, float r) { return r <= 0 ? 0 : 1 - sstep(0.45f * r, r + 1.0f, d); }

static int nlit, lit_cell[TMZ_BRAZIERS];       /* the braziers burning now, their light's radius */
static float lit_r[TMZ_BRAZIERS];

static void braziers_now(Uint32 now)
{
    nlit = 0;
    for (int c = 0; c < G.m.w * G.m.h; c++) {
        if (G.m.item[c] != TMZ_BRAZIER || !(G.lit >> G.m.ik[c] & 1) || nlit >= TMZ_BRAZIERS) continue;
        float r = BRAZ_R * (1 + 0.025f * sinf(now / 90.0f + c));
        if (pop.on && pop.what == TMZ_BRAZIER && pop.cell == c) {
            if (now < pop.t0) continue;         /* not reached yet */
            r *= clampf((now - pop.t0) / 700.0f, 0.05f, 1);
        }
        lit_cell[nlit] = c;
        lit_r[nlit++] = r;
    }
}

static float light_at(const tmz_state *s, float px, float py)
{
    float l = 0, d = hypotf(px - tx, py - ty);
    if (trad > 0 && d < trad + 0.9f && tmz_ray(&G.m, s, tx, ty, px, py, TOL)) l = fall(d, trad);
    for (int i = 0; i < nlit && l < 1; i++) {
        int c = lit_cell[i];
        float bx = c % G.m.w + 0.5f, by = c / G.m.w + 0.5f, db = hypotf(px - bx, py - by);
        if (db < lit_r[i] + 0.9f && tmz_ray(&G.m, s, bx, by, px, py, TOL)) l = fmaxf(l, 0.9f * fall(db, lit_r[i]));
    }
    return fminf(l, 1);
}

/* the cells lit now: a light patch each (SUB x SUB vertices, the light at each), the lit maze drawn through them */
static void draw_lit(const tmz_state *s)
{
    if (!vbuf || !ibuf || !t_lit) return;
    int nv = 0, ni = 0, W = G.m.w, H = G.m.h;
    static uint8_t near[TMZ_CELLS];
    memset(near, 0, sizeof near);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float d = hypotf(x + 0.5f - tx, y + 0.5f - ty);
            int on = trad > 0 && d < trad + 1.6f;
            for (int i = 0; i < nlit && !on; i++)
                if (hypotf(x - lit_cell[i] % W, y - lit_cell[i] / W) < lit_r[i] + 1.6f) on = 1;
            near[x + y * W] = (uint8_t)on;
        }
    for (int k = 0; k < W * H; k++) {
        if (!near[k]) continue;
        int x = k % W, y = k / W, base = nv;
        float any = 0;
        for (int j = 0; j < SUB; j++)
            for (int i = 0; i < SUB; i++) {
                float px = x + VU[i], py = y + VU[j], l = light_at(s, px, py);
                any += l;
                Uint8 a = (Uint8)clampf(255 * l, 0, 255);
                float warm = 0.62f + 0.38f * l;
                vbuf[nv++] = (SDL_Vertex){{ox + px * cell, oy + py * cell},
                                          {(Uint8)(255 * warm), (Uint8)(232 * warm), (Uint8)(192 * warm), a},
                                          {px / W, py / H}};
            }
        if (any <= 0.001f) { nv = base; continue; }
        for (int j = 0; j < SUB - 1; j++)
            for (int i = 0; i < SUB - 1; i++) {
                int a = base + j * SUB + i;
                ibuf[ni++] = a; ibuf[ni++] = a + 1; ibuf[ni++] = a + SUB;
                ibuf[ni++] = a + 1; ibuf[ni++] = a + SUB + 1; ibuf[ni++] = a + SUB;
            }
    }
    if (ni) SDL_RenderGeometry(ui.r, t_lit, vbuf, nv, ibuf, ni);
}

/* the chalk map: each remembered cell's square of it (rock beside a remembered cell too) */
static void draw_chalk(Uint32 now)
{
    if (!vbuf || !ibuf || !t_chalk) return;
    int nv = 0, ni = 0, W = G.m.w, H = G.m.h;
    /* while walking, the cells the light has not yet reached on the screen wait */
    int shown_tick = G.tick;
    if (walk.on && now - walk.t0 < walk.len) {
        int done = (int)((now - walk.t0) * walk.n / walk.len);
        shown_tick = G.tick - (walk.n - done);
    }
    for (int k = 0; k < W * H; k++) {
        float a = 0;
        if (G.m.in[k]) {
            if (G.first[k] && G.first[k] <= shown_tick) tmz_remembered(&G, k, &a);
        } else
            for (int d = 0; d < 4; d++) {
                int x = k % W + (d == 1) - (d == 3), y = k / W + (d == 2) - (d == 0);
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                int n = x + y * W;
                float b = 0;
                if (G.m.in[n] && G.first[n] && G.first[n] <= shown_tick) tmz_remembered(&G, n, &b);
                a = fmaxf(a, b);
            }
        if (a <= 0) continue;
        int x = k % W, y = k / W;
        Uint8 al = (Uint8)(175 * a);
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
                vbuf[nv + j * 2 + i] = (SDL_Vertex){{ox + (x + i) * cell, oy + (y + j) * cell}, {255, 255, 255, al},
                                                    {(float)(x + i) / W, (float)(y + j) / H}};
        ibuf[ni++] = nv; ibuf[ni++] = nv + 1; ibuf[ni++] = nv + 2;
        ibuf[ni++] = nv + 1; ibuf[ni++] = nv + 3; ibuf[ni++] = nv + 2;
        nv += 4;
    }
    if (ni) SDL_RenderGeometry(ui.r, t_chalk, vbuf, nv, ibuf, ni);
}

/* a flame: a flickering teardrop of light (cx, cy its foot, h tall) */
static void draw_flame(float cx, float cy, float h, float lean, float t, int i)
{
    enum { N = 14 };
    SDL_Vertex v[N + 2];
    int idx[N * 3];
    float fl = 1 + 0.12f * sinf(t / 55.0f + i) + 0.08f * sinf(t / 23.0f + 2 * i);
    float tipx = cx + lean * h + 0.08f * h * sinf(t / 70.0f + i), tipy = cy - h * fl;
    for (int layer = 0; layer < 2; layer++) {
        float k = layer ? 0.55f : 1;
        Uint8 r = 255, g = layer ? 240 : 150, b = layer ? 170 : 40;
        v[0] = (SDL_Vertex){{cx + (tipx - cx) * 0.25f, cy - h * 0.3f * k}, {r, g, b, 255}, {0, 0}};
        for (int j = 0; j <= N; j++) {
            float a = (float)j / N * 2 * PUZ_PI, sx = sinf(a), sy = cosf(a);
            float wdt = 0.28f * h * k, up = sy < 0 ? sy : sy * 0.35f;   /* a drop: round below, drawn up to a tip */
            float px = cx + sx * wdt * (sy < 0 ? (1 + sy) * 0.9f + 0.1f : 1), py = cy - h * 0.25f * k - up * 0.25f * h;
            if (sy < 0) {                       /* the upper half pulled to the tip */
                float f = -sy;
                px += (tipx - cx) * f * k;
                py += (tipy - cy) * f * k * 0.85f;
            }
            v[1 + j] = (SDL_Vertex){{px, py}, {r, (Uint8)(g * 0.7f), b, 0}, {0, 0}};
        }
        for (int j = 0; j < N; j++) { idx[3 * j] = 0; idx[3 * j + 1] = 1 + j; idx[3 * j + 2] = 2 + j; }
        SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_ADD);
        SDL_RenderGeometry(ui.r, NULL, v, N + 2, idx, N * 3);
    }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
}

/* the mouse's place on the screen now (cells), and how far it is in its walk (bob) */
static void mouse_at(Uint32 now, float *mx, float *my, float *phase, int *face)
{
    int W = G.m.w, at = G.s.at;
    *phase = 0;
    *face = G.s.face;
    if (walk.on && now - walk.t0 < walk.len) {
        float f = puz_ease((now - walk.t0) / (float)walk.len) * walk.n;
        int i = (int)f;
        if (i >= walk.n) i = walk.n - 1;
        int a = i == 0 ? walk.from : walk.cells[i - 1], b = walk.cells[i];
        float t = f - i;
        *mx = (a % W) + ((b % W) - (a % W)) * t + 0.5f;
        *my = (a / W) + ((b / W) - (a / W)) * t + 0.5f;
        *phase = f;
        int dx = b % W - a % W, dy = b / W - a / W;
        *face = dx > 0 ? TMZ_RIGHT : dx < 0 ? TMZ_LEFT : dy > 0 ? TMZ_DOWN : TMZ_UP;
        return;
    }
    *mx = at % W + 0.5f;
    *my = at / W + 0.5f;
    if (bump.on) {
        float t = (now - bump.t0) / (float)BUMP_MS;
        if (t >= 1) bump.on = 0;
        else {
            float k = 0.14f * sinf(PUZ_PI * t);
            *mx += k * ((bump.dir == TMZ_RIGHT) - (bump.dir == TMZ_LEFT));
            *my += k * ((bump.dir == TMZ_DOWN) - (bump.dir == TMZ_UP));
            ui_animate();
        }
    }
}

static void put_flip(SDL_Texture *t, float cx, float cy, float w, float h, double deg, int flip, Uint8 shade,
                     Uint8 alpha)
{
    if (!t) return;
    SDL_FRect d = {cx - w / 2, cy - h / 2, w, h};
    SDL_SetTextureColorMod(t, shade, shade, shade);
    SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderCopyExF(ui.r, t, NULL, &d, deg, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* an edge's thing drawn on it: a door, a one-way door, a gate */
static void draw_edges(const tmz_state *s, Uint32 now)
{
    int W = G.m.w;
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < W * G.m.h; k++) {
            int t = G.m.e[side][k], kk = G.m.k[side][k];
            if (t != TMZ_DOOR && t != TMZ_ONEWAY && t != TMZ_GATE) continue;
            float ex = k % W + (side == 0 ? 1.0f : 0.5f), ey = k / W + (side == 1 ? 1.0f : 0.5f);
            float l = fmaxf(light_at(s, ex - (side == 0) * 0.3f, ey - (side == 1) * 0.3f),
                            light_at(s, ex + (side == 0) * 0.3f, ey + (side == 1) * 0.3f));
            if (l <= 0.01f) continue;
            Uint8 sh = (Uint8)(255 * (0.55f + 0.45f * l)), al = (Uint8)(255 * clampf(l * 1.4f, 0, 1));
            float sx = ox + ex * cell, sy = oy + ey * cell, len = cell * (1 - 2 * WT) / 0.92f;
            double deg = side == 0 ? 90 : 0;
            if (t == TMZ_DOOR) {
                if (s->keys >> kk & 1) {         /* unlocked: swung open against the post */
                    float hx = side == 0 ? sx : sx - cell * (0.5f - WT), hy = side == 0 ? sy - cell * (0.5f - WT) : sy;
                    float cx = hx + (side == 0 ? 0.33f * cell : 0), cy = hy + (side == 0 ? 0 : 0.33f * cell);
                    put_flip(t_door[kk % 3], side == 0 ? cx + 0.0f : cx, side == 0 ? cy + 0.0f : cy, len * 0.75f,
                             len * 0.75f, side == 0 ? 0 : 90, 0, sh, al);
                } else put_flip(t_door[kk % 3], sx, sy, len, len, deg, 0, sh, al);
            } else if (t == TMZ_ONEWAY) {
                put_flip(t_oneway, sx, sy, len, len, deg, 0, sh, al);
                float dx = (float)(kk == TMZ_RIGHT) - (kk == TMZ_LEFT), dy = (float)(kk == TMZ_DOWN) - (kk == TMZ_UP);
                for (int i = 0; i < 2; i++) {   /* brass chevrons on the floor, its way */
                    float px = sx + dx * (0.22f + 0.17f * i) * cell, py = sy + dy * (0.22f + 0.17f * i) * cell;
                    float q = 0.11f * cell, w2 = 0.045f * cell;
                    pzk_rgba c = {BRASS[0], BRASS[1], BRASS[2], (Uint8)(al * 0.9f)};
                    pzk_line(px - dx * q - dy * q, py - dy * q + dx * q, px, py, w2, 1, c, 0);
                    pzk_line(px - dx * q + dy * q, py - dy * q - dx * q, px, py, w2, 1, c, 0);
                    pzk_dot(px, py, w2, 1, c, 0);
                }
            } else {
                int open = (kk ^ s->gates) & 1;
                float f = open ? 1 : 0;
                if (gate_turn.on && now >= gate_turn.t0) {
                    float g = (now - gate_turn.t0) / (float)GATE_MS;
                    if (g < 1) { f = open ? puz_ease(g) : 1 - puz_ease(g); ui_animate(); }
                }
                float k2 = 1 - 0.85f * f;       /* raised: its bars drawn shorter into the wall's slot */
                put_flip(t_gate, sx, sy, len, len * (0.25f + 0.75f * k2), deg, 0, sh, al);
            }
        }
}

/* the things in the cells: keys, flasks, plates, braziers (lit: their flames) */
static void draw_items(const tmz_state *s, Uint32 now)
{
    int W = G.m.w;
    for (int k = 0; k < W * G.m.h; k++) {
        int it = G.m.item[k], ik = G.m.ik[k];
        if (it == TMZ_NONE || it == TMZ_EXIT) continue;
        float cx = ox + (k % W + 0.5f) * cell, cy = oy + (k / W + 0.5f) * cell;
        float l = light_at(s, k % W + 0.5f, k / W + 0.5f), mem = 0;
        tmz_remembered(&G, k, &mem);
        Uint8 sh = (Uint8)(255 * (0.55f + 0.45f * l)), al = (Uint8)(255 * clampf(l * 1.5f, 0, 1));
        if (it == TMZ_KEY && !(s->keys >> ik & 1)) {
            float bob = 0.04f * cell * sinf(now / 300.0f + k);
            if (l > 0.01f) {
                ui_shadow_flat(cx + 0.04f * cell, cy + 0.08f * cell, 0.42f * cell, 0.2f * cell, 0.35f * l);
                put_flip(t_key[ik % 3], cx, cy - bob, 0.62f * cell, 0.62f * cell, 0, 0, sh, al);
                ui_mark_gold_a(cx, cy, 0.7f * cell, 0.18f * l * (0.6f + 0.4f * sinf(now / 260.0f)));
                ui_animate();
            } else if (mem > 0) put_flip(t_ckey, cx, cy, 0.5f * cell, 0.5f * cell, 0, 0, 255, (Uint8)(110 * mem));
        } else if (it == TMZ_OIL && !(s->flasks >> ik & 1)) {
            if (l > 0.01f) {
                ui_shadow_flat(cx + 0.04f * cell, cy + 0.12f * cell, 0.4f * cell, 0.18f * cell, 0.35f * l);
                put_flip(t_oil, cx, cy, 0.6f * cell, 0.6f * cell, 0, 0, sh, al);
            } else if (mem > 0) put_flip(t_coil, cx, cy, 0.5f * cell, 0.5f * cell, 0, 0, 255, (Uint8)(110 * mem));
        } else if (it == TMZ_PLATE) {
            if (l <= 0.01f) continue;
            float press = 0;
            if (pop.on && pop.cell == k && now >= pop.t0 && now - pop.t0 < POP_MS) {
                press = puz_pulse((float)(now - pop.t0), 0, POP_MS);
                ui_animate();
            }
            put_flip(t_plate, cx, cy, cell * (1 - 0.06f * press), cell * (1 - 0.06f * press), 0, 0,
                     (Uint8)(sh * (1 - 0.15f * press)), al);
        } else if (it == TMZ_BRAZIER) {
            int lit = G.lit >> ik & 1;
            if (lit && pop.on && pop.what == TMZ_BRAZIER && pop.cell == k && now < pop.t0) lit = 0;
            if (l <= 0.01f && !lit) continue;
            put_flip(t_braz, cx, cy, 0.8f * cell, 0.8f * cell, 0, 0, lit ? 255 : sh, lit ? 255 : al);
            if (lit) {
                float grow = 1;
                if (pop.on && pop.cell == k) grow = clampf((now - pop.t0) / 500.0f, 0.1f, 1);
                pzk_dot(cx, cy, 2.2f * cell * grow, 0.9f * cell, (pzk_rgba){255, 150, 60, 60}, 1);
                for (int i = 0; i < 3; i++)
                    draw_flame(cx + (i - 1) * 0.1f * cell, cy + 0.05f * cell, (0.42f - 0.08f * (i != 1)) * cell * grow,
                               0, (float)now, k * 3 + i);
                ui_animate();
            }
        }
    }
}

static void draw_mouse(Uint32 now)
{
    float mx, my, phase;
    int face;
    mouse_at(now, &mx, &my, &phase, &face);
    float cx = ox + mx * cell, cy = oy + my * cell, s = 0.92f * cell, lift = 0, alpha = 1, scale = 1;
    int flip = face == TMZ_LEFT;
    if (!walk.on || now - walk.t0 >= walk.len) {
        static int last_face_left;
        if (face == TMZ_LEFT) last_face_left = 1;
        else if (face == TMZ_RIGHT) last_face_left = 0;
        flip = last_face_left;
    }
    int walking = walk.on && now - walk.t0 < walk.len;
    if (walking) lift = 0.05f * cell * fabsf(sinf(PUZ_PI * phase));
    if (win.on && now >= win.t0) {              /* down the stairs */
        float t = clampf((now - win.t0) / (float)(WIN_MS * 0.7f), 0, 1);
        scale = 1 - 0.6f * puz_ease(t);
        alpha = 1 - puz_ease(t);
        cy += 0.15f * cell * puz_ease(t);
    }
    float tilt = walking ? 5 * sinf(PUZ_PI * phase) : 2 * sinf(now / 600.0f);
    int torch_out = G.m.fuel && G.s.fuel <= 0 && !walking;
    Uint8 shade = torch_out ? 120 : 255, al = (Uint8)(255 * alpha);
    ui_shadow_flat(cx + 0.04f * cell, cy + 0.36f * cell * scale, 0.5f * cell * scale, 0.18f * cell * scale, 0.4f * alpha);
    /* the torch, held up in the paw at its right (its left when it walks left), swaying */
    float side = flip ? -1 : 1, sway = (walking ? 9 * sinf(PUZ_PI * phase + 0.6f) : 4 * sinf(now / 420.0f)) * side;
    float hx = cx + side * 0.22f * s * scale, hy = cy - lift + 0.06f * s * scale;
    put_flip(t_stick, hx, hy - 0.12f * s * scale, 0.7f * s * scale, 0.7f * s * scale, sway + side * 12, 0, shade, al);
    put_flip(t_mouse, cx, cy - lift, s * scale, s * scale, tilt, flip, shade, al);
    float a = (sway + side * 12) * PUZ_PI / 180, top = 0.7f * s * scale * 0.32f;
    float fx = hx - 0.12f * s * scale * 0 + sinf(a) * (top + 0.14f * s * scale), fy = hy - 0.12f * s * scale - cosf(a) * (top + 0.04f * s * scale);
    if (!torch_out && alpha > 0.05f) {
        float lean = walking ? -0.25f * ((face == TMZ_RIGHT) - (face == TMZ_LEFT)) : 0;
        draw_flame(fx, fy + 0.04f * cell, 0.6f * cell * scale * (0.7f + 0.3f * trad / fmaxf(1, G.m.radius / 10.0f)),
                   lean, (float)now, 0);
        pzk_dot(fx, fy - 0.1f * cell, 1.3f * cell, 0.6f * cell, (pzk_rgba){255, 170, 70, (Uint8)(70 * alpha)}, 1);
    } else if (torch_out) {
        pzk_dot(fx, fy, 0.25f * cell, 0.15f * cell, (pzk_rgba){200, 70, 30, (Uint8)(120 + 60 * sinf(now / 200.0f))}, 1);
        ui_animate();
    }
    ui_animate();
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    int walking = walk.on && now - walk.t0 < walk.len;
    if (walk.on && !walking) walk.on = 0;
    const tmz_state *s = walking ? &before : &G.s;
    float mx, my, ph;
    int face;
    mouse_at(now, &mx, &my, &ph, &face);
    if (scroll) follow(mx, my);
    flick = 1 + 0.03f * sinf(now / 71.0f) + 0.02f * sinf(now / 37.0f + 1.3f) + 0.012f * sinf(now / 13.0f);
    trad = tmz_light(&G.m, s) * flick;
    braziers_now(now);
    if (pop.on && pop.what == TMZ_OIL && now >= pop.t0) {   /* the torch flares, filled */
        float p = puz_pulse((float)(now - pop.t0), 0, POP_MS);
        trad = tmz_light(&G.m, &G.s) * flick * (1 + 0.25f * p);
    }
    tx = mx + (face == TMZ_LEFT ? -0.12f : 0.12f);
    ty = my - 0.12f;
    if (t_frame) ui_put(t_frame, fbox.x, fbox.y, fbox.w, fbox.h);
    SDL_Rect clip = {(int)(fbox.x + fr), (int)(fbox.y + fr), (int)(fbox.w - 2 * fr), (int)(fbox.h - 2 * fr)};
    SDL_RenderSetClipRect(ui.r, &clip);
    draw_chalk(now);
    draw_lit(s);
    draw_edges(s, now);
    draw_items(s, now);
    if (hint.on) {                              /* gold light running along the next steps */
        float t = (now - hint.t0) / (float)HINT_MS;
        if (t >= 1) hint.on = 0;
        else {
            for (int i = 0; i < hint.n; i++) {
                int c = hint.cells[i];
                float wave = puz_pulse((float)(now - hint.t0) - i * 70.0f, 0, 900) * (1 - t) +
                             0.35f * (1 - t) * (1 - (float)i / hint.n);
                ui_mark_gold_a(ox + (c % G.m.w + 0.5f) * cell, oy + (c / G.m.w + 0.5f) * cell, 0.75f * cell, wave);
            }
            ui_animate();
        }
    }
    if (pop.on && now >= pop.t0) {               /* a key picked up: it rises in a glint and fades */
        float t = (now - pop.t0) / (float)POP_MS;
        if (t >= 1 && pop.what != TMZ_BRAZIER) pop.on = 0;
        else if (pop.what == TMZ_KEY || pop.what == TMZ_OIL) {
            float cx = ox + (pop.cell % G.m.w + 0.5f) * cell, cy = oy + (pop.cell / G.m.w + 0.5f) * cell;
            put_flip(pop.what == TMZ_KEY ? t_key[pop.k % 3] : t_oil, cx, cy - 0.7f * cell * puz_ease(t),
                     0.62f * cell * (1 + 0.3f * t), 0.62f * cell * (1 + 0.3f * t), 0, 0, 255,
                     (Uint8)(255 * (1 - t * t)));
            ui_mark_gold(cx, cy - 0.6f * cell * puz_ease(t), (0.6f + 1.2f * t) * cell * (1 - t));
            ui_animate();
        }
        if (pop.what == TMZ_BRAZIER && t >= 1.2f) pop.on = 0;
    }
    /* the exit's glow, seen through the dark */
    {
        float ex = ox + (G.m.exit % G.m.w + 0.5f) * cell, ey = oy + (G.m.exit / G.m.w + 0.5f) * cell;
        float p = 0.5f + 0.5f * sinf(now / 500.0f);
        pzk_dot(ex, ey, 1.4f * cell, 0.8f * cell, (pzk_rgba){255, 196, 90, (Uint8)(60 + 40 * p)}, 1);
        pzk_dot(ex, ey, 0.15f * cell, 0.5f * cell, (pzk_rgba){255, 220, 140, (Uint8)(40 + 25 * p)}, 1);
        if (win.on && now >= win.t0) {
            float q = puz_pulse((float)(now - win.t0), 0, WIN_MS);
            ui_mark_gold(ex, ey, (1.2f + 3.0f * q) * cell * (0.4f + 0.6f * q));
        }
    }
    draw_mouse(now);
    SDL_RenderSetClipRect(ui.r, NULL);
    unsigned can = 0;
    for (int w = 0; w < 4; w++)
        if (!solved() && tmz_can(&G.m, &G.s, w)) can |= 1u << w;
    int lit = -1;
    if (hint.on && hint.n > 0) lit = dir_of_cells(G.s.at, hint.cells[0]);
    swp_tools_draw(can, lit, !solved(), down);
    ui_animate();
}

/* ---- the pointer --------------------------------------------------------------------------------------------- */
static void tap_at(float x, float y)            /* beside the mouse: one step that way; on it: refused */
{
    float mx, my, ph;
    int face;
    mouse_at(ui_ms(), &mx, &my, &ph, &face);
    float dx = x - (ox + mx * cell), dy = y - (oy + my * cell);
    if (fabsf(dx) < 0.4f * cell && fabsf(dy) < 0.4f * cell) { ui_sound(SND_DENY); return; }
    go(swp_way(dx, dy), 0);
}

static void press(float x, float y)
{
    sw.on = 0;
    down = swp_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved()) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) go(w, 1);
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (swp_tool_hit(x, y) != k) return;
        if (k == SWP_HINT) show_hint();
        else if (solved()) ui_sound(SND_DENY);
        else go(k, 1);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) go(w, 1);
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    if (G.m.fuel) {
        ui_count c[3] = {{NULL, G.s.steps}, {t_drop, G.s.fuel}, {t_par, G.par}};
        pzt_cream_ink(t_par);
        ui_bar_fit(b, c, 3, 0);
        pzt_cream_end(t_par);
    } else pzt_bar_two_cream(b, NULL, G.s.steps, t_par, par(), solved() && G.s.steps <= G.par);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_mouse(rgba, size);
}

/* ---- the shell's pictures (painted in pixels: no SDL here) ---------------------------------------------------- */
/* a map in chalk on a dark slate (a level's tile, a random size's card): the start a warm dot, the exit gold */
static void paint_map(const tmz_map *m, int size, uint8_t *rgba, float fade)
{
    int n = m->w > m->h ? m->w : m->h;
    float c = floorf(size * 0.84f / n);
    if (c < 2) return;
    int W = (int)(m->w * c), H = (int)(m->h * c), x0 = (size - W) / 2, y0 = (size - H) / 2;
    int pad = (int)fmaxf(2, 0.05f * size);
    art_roundrect_put(rgba, size, size, x0 - pad, y0 - pad, W + 2 * pad, H + 2 * pad, 0.08f * size, DARK, 1);
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_chalk(m, c, W, H, o);
    for (int y = 0; y < H; y++)                 /* fading: the map sketched only near the start */
        for (int x = 0; x < W; x++) {
            float d = hypotf(x / c - (m->start % m->w + 0.5f), y / c - (m->start / m->w + 0.5f));
            float a = fade > 0 ? clampf(1.2f - d / fade, 0, 1) : 1;
            o[4 * ((size_t)y * W + x) + 3] = (uint8_t)(o[4 * ((size_t)y * W + x) + 3] * a);
        }
    art_blend(rgba, size, size, o, W, H, x0, y0);
    free(o);
    for (int k = 0; k < m->w * m->h; k++)
        if (m->item[k] == TMZ_KEY) {
            uint8_t col[3];
            player_rgb(KEYCOL[m->ik[k] % 3], col);
            pzk_ball(rgba, size, size, x0 + (k % m->w + 0.5f) * c, y0 + (k / m->w + 0.5f) * c, fmaxf(1.2f, 0.22f * c),
                     col, 1, 0.4f);
        }
    float ex = x0 + (m->exit % m->w + 0.5f) * c, ey = y0 + (m->exit / m->w + 0.5f) * c;
    pzk_ball(rgba, size, size, ex, ey, fmaxf(2, 0.7f * c), GOLD, 0.25f, 0);
    pzk_ball(rgba, size, size, ex, ey, fmaxf(1.3f, 0.3f * c), GOLD, 1, 0.3f);
    static const uint8_t FIRE[3] = {255, 170, 70}, FIRE2[3] = {255, 190, 100};
    float sx = x0 + (m->start % m->w + 0.5f) * c, sy = y0 + (m->start / m->w + 0.5f) * c;
    pzk_ball(rgba, size, size, sx, sy, fmaxf(2, 0.8f * c), FIRE, 0.25f, 0);
    pzk_ball(rgba, size, size, sx, sy, fmaxf(1.3f, 0.3f * c), FIRE2, 1, 0.3f);
}

/* a flame painted (a picture's): a drop of orange, its heart yellow, its foot at (cx, cy), h tall */
static void paint_flame_px(uint8_t *o, int size, float cx, float cy, float h)
{
    static const uint8_t ORANGE[3] = {250, 140, 40}, YELLOW[3] = {255, 226, 120};
    for (int layer = 0; layer < 2; layer++) {
        float k = layer ? 0.55f : 1, w = 0.3f * h * k, base = cy - 0.28f * h * k, tip = cy - h * k;
        for (int y = (int)tip - 1; y <= (int)cy + 1; y++)
            for (int x = (int)(cx - w) - 1; x <= (int)(cx + w) + 1; x++) {
                if (x < 0 || y < 0 || x >= size || y >= size) continue;
                float px = x + 0.5f - cx, py = y + 0.5f, in;   /* a disc below, narrowing to the tip above */
                if (py >= base) in = w - hypotf(px, py - base);
                else in = w * (py - tip) / (base - tip) - fabsf(px);
                pzk_over(o + 4 * ((size_t)y * size + x), layer ? YELLOW : ORANGE, clampf(in + 0.5f, 0, 1));
            }
    }
}

/* the menu's card: a corner of a maze in the torch's light, the rest dark with the chalk map, the mouse */
static void paint_scene(int size, uint8_t *rgba)
{
    static const uint8_t FIRE[3] = {255, 170, 60};
    tmz_game *t = malloc(sizeof *t);
    if (!t) return;
    if (!tmz_level(t, 9)) { free(t); return; }
    tmz_script(t, "=5");
    const tmz_map *m = &t->m;
    float c = floorf(size / 6.2f);
    int W = (int)(m->w * c), H = (int)(m->h * c);
    float mx = t->s.at % m->w + 0.5f, my = t->s.at / m->w + 0.5f;
    int x0 = (int)(size / 2 - mx * c), y0 = (int)(size * 0.55f - my * c);
    uint8_t *lit = malloc((size_t)W * H * 4), *ch = malloc((size_t)W * H * 4);
    if (lit && ch) {
        paint_lit(m, c, W, H, lit, 0);
        paint_chalk(m, c, W, H, ch);
        float R = 2.3f;
        static const float warm[3] = {1.0f, 0.87f, 0.69f};
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                uint8_t *p = rgba + 4 * ((size_t)y * size + x);
                float acc[3];
                for (int i = 0; i < 3; i++) acc[i] = DARK[i];
                int lx = x - x0, ly = y - y0;
                if (lx >= 0 && ly >= 0 && lx < W && ly < H) {
                    float px = (lx + 0.5f) / c, py = (ly + 0.5f) / c, l = 0, d = hypotf(px - mx, py - my + 0.1f);
                    int k = (int)px + (int)py * m->w;
                    if (d < R + 0.9f && tmz_ray(m, &t->s, mx, my - 0.1f, px, py, TOL)) l = fall(d, R);
                    const uint8_t *q = ch + 4 * ((size_t)ly * W + lx), *s2 = lit + 4 * ((size_t)ly * W + lx);
                    if (t->first[k])
                        for (int i = 0; i < 3; i++) acc[i] += (q[i] - acc[i]) * q[3] / 255.0f * 0.55f;
                    for (int i = 0; i < 3; i++) acc[i] += (s2[i] * warm[i] * (0.62f + 0.38f * l) - acc[i]) * l;
                    float g = clampf(1 - d / (R + 1.6f), 0, 1);
                    for (int i = 0; i < 3; i++) acc[i] += 46 * g * g * warm[i];
                }
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(acc[i], 0, 255);
                p[3] = 255;
            }
        float ex = x0 + (m->exit % m->w + 0.5f) * c, ey = y0 + (m->exit / m->w + 0.5f) * c;
        pzk_ball(rgba, size, size, ex, ey, 0.9f * c, GOLD, 0.3f, 0);
        int S = (int)(0.95f * c);
        uint8_t *mo = calloc((size_t)S * S, 4);
        if (mo) {
            paint_mouse(mo, S);
            art_blend(rgba, size, size, mo, S, S, (int)(x0 + mx * c - S / 2), (int)(y0 + my * c - S / 2));
            free(mo);
        }
        float fx = x0 + mx * c + 0.3f * c, fy = y0 + my * c - 0.2f * c;
        pzk_stroke(rgba, size, size, fx - 0.05f * c, fy + 0.1f * c, fx - 0.12f * c, fy + 0.45f * c, 0.05f * c, WOOD,
                   1);
        pzk_ball(rgba, size, size, fx, fy, 0.45f * c, FIRE, 0.25f, 0);
        paint_flame_px(rgba, size, fx, fy + 0.1f * c, 0.42f * c);
    }
    free(lit);
    free(ch);
    free(t);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) { paint_scene(size, rgba); return; }
    tmz_spec sp;
    tmz_variant(variant, &sp);
    sp.tries = 1;
    tmz_map *m = malloc(sizeof *m);
    if (!m) return;
    for (uint32_t s = 3; s < 40; s++)
        if (tmz_make(m, &sp, s * 7 + (uint32_t)variant)) { paint_map(m, size, rgba, 0); break; }
    free(m);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    tmz_map *m = malloc(sizeof *m);
    if (m && level >= 0 && level < TMZ_LEVELS && tmz_parse(m, tmz_levels[level]))
        paint_map(m, size, rgba, m->fade ? 3.5f : 0);
    free(m);
}

/* a chapter's tab: what it brings (a maze, a key, a gate and its plate, a one-way door, a flask, a fading map) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int S = size;
    uint8_t *o = calloc((size_t)S * S, 4);
    if (!o) return;
    switch (chapter) {
    case 1: paint_key(o, S, 0, 0); break;
    case 2:                                     /* a portcullis: iron bars, pointed, two crossbars */
        for (int i = 0; i < 5; i++) {
            float x = (0.22f + 0.14f * i) * S;
            pzk_stroke(o, S, S, x, 0.18f * S, x, 0.78f * S, 0.035f * S, IRON, 1);
            pzk_stroke(o, S, S, x - 0.01f * S, 0.2f * S, x - 0.01f * S, 0.74f * S, 0.01f * S, PUZ_WHITE, 0.35f);
            pzk_ball(o, S, S, x, 0.84f * S, 0.04f * S, IRON, 1, 0.4f);
        }
        pzk_stroke(o, S, S, 0.15f * S, 0.32f * S, 0.85f * S, 0.32f * S, 0.04f * S, IRON, 1);
        pzk_stroke(o, S, S, 0.15f * S, 0.6f * S, 0.85f * S, 0.6f * S, 0.04f * S, IRON, 1);
        pzk_stroke(o, S, S, 0.15f * S, 0.31f * S, 0.85f * S, 0.31f * S, 0.01f * S, PUZ_WHITE, 0.3f);
        break;
    case 3:
        paint_oneway(o, S);
        for (int i = 0; i < 2; i++) {
            float y = (0.70f + 0.14f * i) * S;
            pzk_stroke(o, S, S, 0.36f * S, y - 0.1f * S, 0.5f * S, y, 0.05f * S, BRASS, 1);
            pzk_stroke(o, S, S, 0.64f * S, y - 0.1f * S, 0.5f * S, y, 0.05f * S, BRASS, 1);
        }
        break;
    case 4: paint_oil(o, S, 0); break;
    default: {
        tmz_map *m = malloc(sizeof *m);
        if (m && tmz_parse(m, tmz_levels[chapter == 5 ? 125 : 2])) paint_map(m, S, o, chapter == 5 ? 9.0f : 0);
        free(m);
    }
    }
    art_blend(rgba, size, size, o, S, S, 0, 0);
    free(o);
}

static const game torchmaze_game = {
    .id = "torchmaze", .order = 460, .variants = TMZ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = TMZ_LEVELS, .chapters = TMZ_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = SWP_KNOBS,
};
REGISTER_GAME(torchmaze_game)
