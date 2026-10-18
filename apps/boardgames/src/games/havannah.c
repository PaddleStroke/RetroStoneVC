/*
 * Ring and Fork in RetroStone Board: the hexagon of hexagonal cells (6 or 8 a side) on the plank, the stones, the
 * swap (the rules: havannah_rules.c, havannah.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The plank takes the whole room the shell gives (game.fill); the hexagon lies with two sides across the screen, or
 * turned a quarter where that makes the cells clearly bigger, half round more when the board is flipped. A dark band
 * runs along each of the six sides, cut short before the corners, and the six corner cells are stained darker: the
 * sides and the corners the shapes are made of. A tap plays on an empty cell; where the cells are small for a finger
 * the first tap shows a ghost stone and a second tap plays it, the shell's loupe showing the cells under the finger.
 * The swap: ebony taps ivory's first stone, a ghost shows it turned, a second tap swaps. Keys: the arrows move the
 * ghost, Return or Space plays it. At the end the winner's shape lights up in gold, stone by stone from the last one,
 * before the end card (busy() holds it). Script: "f6 e7 swap" (havannah.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/havannah.h"
#include "shell.h"

#define FRAME 0.035f                            /* the plank's frame, a part of its short side */
#define STRIP 0.5f                              /* how far a side's band reaches beyond the cells (rows) */
#define CUT 0.6f                                /* the bands stop this far (cells) from a corner cell's middle */
#define FILL 0.97f
#define TURN 1.08f                              /* turned a quarter when that makes the cells this much bigger */
#define PLACE_MS 150
#define SWAP_MS 340
#define SHINE_MS 1600                           /* the shape lit before the end card (then the shell's pause) */
#define GHOST_A 150
#define LOUPE_CELLS 4.2f
#define H 0.8660254f

typedef struct { int n; float s, x0, y0, cs, sn, lw; } view;   /* a cell's width, the middle, the turn, the lines */
enum { WOOD, INK, BAND, CORNER };

static hvgame G;
static view V;
static int fr, bw, bh, made_w = -1, made_h = -1, made_n = -1, made_q = -1, made_z = -1, last_size;
static float bx, by;
static SDL_Texture *t_board, *t_stone[2], *t_zstone[2], *t_last;
static SDL_Texture **stones = t_stone;
static box room;
static struct { float x, y, dx, dy; } fing;
static struct { int on, cell, down, armed, moved; } ghost;
static struct { int on, cell, swap; Uint32 t0; } anim;
static struct { int on; Uint32 t0; } shine;     /* the winning shape lighting up, stone by stone from the last */

static const float COLOUR[4][3] = {{0, 0, 0}, {34, 24, 16}, {84, 52, 30}, {96, 60, 34}};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int size_of(int variant) { return variant % 2 ? 8 : 6; }

static void calm(void)
{
    ghost.on = ghost.down = 0;
    ghost.cell = -1;
    anim.on = 0;
    shine.on = 0;
}

static void start(int variant)
{
    hv_new(&G, size_of(variant), variant >= 2);
    calm();
}

static int status(void)
{
    if (!G.n || G.winner < 0) return GS_PLAY;
    return G.winner == HV_DRAW ? GS_DRAW : gs_win(G.winner);
}

static int turn(void) { return hv_turn(&G); }

static int swappable(int cell) { return cell >= 0 && hv_can_swap(&G) && cell == G.move[0]; }

static void play(int cell)
{
    int swap = swappable(cell);
    if (swap ? !hv_swap(&G) : !hv_play(&G, cell)) { ui_sound(SND_DENY); return; }
    ghost.on = 0;
    anim.on = 1;
    anim.swap = swap;
    anim.cell = cell;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
    if (G.winner == 0 || G.winner == 1) shine.on = 1, shine.t0 = anim.t0 + (swap ? SWAP_MS : PLACE_MS);
}

const hvgame *havannah_live(void) { return &G; }

int havannah_show(const char *move)
{
    int len, m = hv_cell(G.n, move, &len);
    if (status() != GS_PLAY || m == HV_NONE || move[len]) return 0;
    if (m == HV_SWAP ? !hv_can_swap(&G) : !hv_legal(&G, m)) return 0;
    calm();
    play(m == HV_SWAP ? G.move[0] : m);
    return 1;
}

static int undo(void)
{
    calm();
    return hv_undo(&G);
}

static int script(const char *s)
{
    calm();
    return hv_script(&G, s);
}

/* the end waits for the winning stone to land and its shape to light up */
static int busy(void) { return anim.on || (shine.on && (Sint32)(ui_ms() - shine.t0) < SHINE_MS); }
/* a stone still landing, by the clock (not by the last frame drawn): a press or a key at a given time is taken or
 * refused alike, live and when a kept game is played again (whose frames fall at other times) */
static int landing(void) { return anim.on && ui_ms() - anim.t0 < (Uint32)(anim.swap ? SWAP_MS : PLACE_MS); }

/* ---- the geometry: cells (q, r) from the middle, the hexagon of "radius" c = n - 1 ------------------------------ */
static void at(const view *v, float q, float r, float *x, float *y)
{
    float lx = (q + r / 2) * v->s, ly = r * H * v->s;
    *x = v->x0 + lx * v->cs - ly * v->sn;
    *y = v->y0 + lx * v->sn + ly * v->cs;
}

static void lattice(const view *v, float x, float y, float *q, float *r)
{
    float dx = x - v->x0, dy = y - v->y0, lx = dx * v->cs + dy * v->sn, ly = -dx * v->sn + dy * v->cs;
    *r = ly / (H * v->s);
    *q = lx / v->s - *r / 2;
}

static void nearest(float q, float r, int *hq, int *hr)
{
    float s = -q - r, rq = roundf(q), rr = roundf(r), rs = roundf(s);
    float dq = fabsf(rq - q), dr = fabsf(rr - r), ds = fabsf(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds) rr = -rq - rs;
    *hq = (int)rq;
    *hr = (int)rr;
}

static int on_hex(int n, int q, int r) { return hv_on(n, q + n - 1, r + n - 1); }

static int cell_of(int n, int q, int r) { return (r + n - 1) * HV_W + q + n - 1; }

/* how far (lx, ly) pixels from a cell's centre (not turned) is inside its hexagon of width s; < 0: outside */
static float inside(float lx, float ly, float s)
{
    float a = fabsf(lx), b = fabsf(lx / 2 + ly * H), c = fabsf(lx / 2 - ly * H);
    return s / 2 - fmaxf(a, fmaxf(b, c));
}

static float inside_cell(const view *v, float q, float r, int hq, int hr)
{
    float dq = q - hq, dr = r - hr;
    return inside((dq + dr / 2) * v->s, dr * H * v->s, v->s);
}

/* the hexagon's outline reach turned by a: lo and hi across and down (cells) */
static void reach(int n, float a, float lo[2], float hi[2])
{
    float c = n - 1 + 0.5f + STRIP, cs = cosf(a), sn = sinf(a);
    lo[0] = lo[1] = 1e9f;
    hi[0] = hi[1] = -1e9f;
    static const float CQ[6] = {1, 1, 0, -1, -1, 0}, CR[6] = {-1, 0, 1, 1, 0, -1};   /* the corners' directions */
    for (int i = 0; i < 6; i++) {
        float q = CQ[i] * c, r = CR[i] * c, lx = q + r / 2, ly = r * H;
        float x = lx * cs - ly * sn, y = lx * sn + ly * cs;
        lo[0] = fminf(lo[0], x), hi[0] = fmaxf(hi[0], x);
        lo[1] = fminf(lo[1], y), hi[1] = fmaxf(hi[1], y);
    }
}

static float angle_of(int quarter) { return quarter * ART_PI / 2; }

static void fit(view *v, int n, int quarter, float w, float h, float cx, float cy)
{
    float a = angle_of(quarter), lo[2], hi[2];
    reach(n, a, lo, hi);
    v->n = n;
    v->cs = cosf(a);
    v->sn = sinf(a);
    v->s = FILL * fminf(w / (hi[0] - lo[0]), h / (hi[1] - lo[1]));
    v->lw = 0.9f + 0.025f * v->s;
    v->x0 = cx - (lo[0] + hi[0]) / 2 * v->s;
    v->y0 = cy - (lo[1] + hi[1]) / 2 * v->s;
}

static int best_quarter(int n, float w, float h)
{
    float lo[2], hi[2], s[2];
    for (int q = 0; q < 2; q++) {
        reach(n, angle_of(q), lo, hi);
        s[q] = fminf(w / (hi[0] - lo[0]), h / (hi[1] - lo[1]));
    }
    return s[1] > TURN * s[0];
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* what covers a point of the plank: the cells' lines, a corner cell's stain, the sides' bands beyond the cells */
static int what(const view *v, float x, float y)
{
    int n = v->n, hq, hr, c = n - 1;
    float q, r, half = v->lw / 2, k = H * v->s;
    lattice(v, x, y, &q, &r);
    nearest(q, r, &hq, &hr);
    if (on_hex(n, hq, hr)) {
        if (inside_cell(v, q, r, hq, hr) < half) return INK;
        return hv_corner(n, cell_of(n, hq, hr)) >= 0 ? CORNER : WOOD;
    }
    for (int i = 0; i < 6; i++)                 /* the outline of the rim's cells */
        if (on_hex(n, hq + HV_DX[i], hr + HV_DY[i]) && -inside_cell(v, q, r, hq + HV_DX[i], hr + HV_DY[i]) < half)
            return INK;
    float s = -q - r, f[6] = {-r, q, -s, r, -q, s};    /* the six sides' reach, in turn round the board */
    int side = 0;
    for (int i = 1; i < 6; i++)
        if (f[i] > f[side]) side = i;
    float out = f[side] - c - 0.5f;             /* rows beyond the rim */
    float along = fabsf(f[(side + 1) % 6] - f[(side + 5) % 6]) / 2, end = c / 2.0f - CUT;   /* cells from its middle */
    if (out > STRIP || along > end) return WOOD;   /* near a corner: the band cut square */
    if ((STRIP - out) * k < v->lw || (end - along) * v->s < v->lw) return INK;
    return BAND;
}

static void paint(uint8_t *o, int w, int x0, int y0, int x1, int y1, const view *v)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float q, r;
            int hq, hr;
            lattice(v, x + 0.5f, y + 0.5f, &q, &r);
            nearest(q, r, &hq, &hr);
            if (on_hex(v->n, hq, hr) && inside_cell(v, q, r, hq, hr) > v->lw / 2 + 1.5f &&
                hv_corner(v->n, cell_of(v->n, hq, hr)) < 0)
                continue;
            float sum[3] = {0, 0, 0}, wood = 0;
            for (int j = 0; j < 9; j++) {
                int kind = what(v, x + (j % 3 + 0.5f) / 3, y + (j / 3 + 0.5f) / 3);
                float a = kind == INK ? 0.85f : kind == WOOD ? 0 : kind == BAND ? 0.92f : 0.5f;
                for (int i = 0; i < 3; i++) sum[i] += a * COLOUR[kind][i];
                wood += 1 - a;
            }
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)((p[i] * wood + sum[i]) / 9 + 0.5f);
        }
}

static void paint_board(uint8_t *o, int w, int h, int f, int squares, const view *v)
{
    if (squares) art_board_n(9, 9, (w - 2 * f) / 9, f, 0, o);
    else art_plank(w - 2 * f, h - 2 * f, f, o);
    paint(o, w, f, f, w - f, h - f, v);
}

static void paint_last(uint8_t *o, int size, const view *v)
{
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - size / 2.0f, dy = y + 0.5f - size / 2.0f;
            float d = inside(dx * v->cs + dy * v->sn, -dx * v->sn + dy * v->cs, v->s) - v->lw / 2;
            uint8_t *p = o + 4 * (y * size + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * fminf(fmaxf(d + 0.5f, 0), 1));
        }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    int n = G.n ? G.n : 8;
    room = b;
    fr = (int)(FRAME * fminf(b.w, b.h));
    float iw = b.w - 2 * fr, ih = b.h - 2 * fr, lo[2], hi[2];
    int quarter = best_quarter(n, iw, ih) + (ui.flipped ? 2 : 0);
    reach(n, angle_of(quarter), lo, hi);
    float s = FILL * fminf(iw / (hi[0] - lo[0]), ih / (hi[1] - lo[1]));
    bw = (int)fminf(iw, ceilf((hi[0] - lo[0]) * s / FILL)) + 2 * fr;
    bh = (int)fminf(ih, ceilf((hi[1] - lo[1]) * s / FILL)) + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    fit(&V, n, quarter, iw, ih, bx + bw / 2.0f, by + bh / 2.0f);
    int zs = ui_loupe_small(V.s) ? (int)lroundf(V.s * UI_LOUPE_Z) : 0;
    if (made_z != zs) {
        made_z = zs;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_zstone[p]);
            if (zs) art_counter(COUNTER_STONE, p, zs, ui_buf(zs, zs)), t_zstone[p] = ui_tex(zs, zs, ui_buf(zs, zs));
        }
    }
    if (made_w == bw && made_h == bh && made_n == n && made_q == quarter) return;
    made_w = bw, made_h = bh, made_n = n, made_q = quarter;
    view local;
    fit(&local, n, quarter, iw, ih, bw / 2.0f, bh / 2.0f);
    ui_free(&t_board);
    paint_board(ui_buf(bw, bh), bw, bh, fr, 0, &local);
    t_board = ui_tex(bw, bh, ui_buf(bw, bh));
    int si = (int)lroundf(V.s);
    for (int p = 0; p < 2; p++) {
        ui_free(&t_stone[p]);
        art_counter(COUNTER_STONE, p, si, ui_buf(si, si));
        t_stone[p] = ui_tex(si, si, ui_buf(si, si));
    }
    last_size = (int)(1.25f * V.s) + 2;
    ui_free(&t_last);
    paint_last(ui_buf(last_size, last_size), last_size, &local);
    t_last = ui_tex(last_size, last_size, ui_buf(last_size, last_size));
    SDL_SetTextureColorMod(t_last, 250, 214, 90);
    SDL_SetTextureAlphaMod(t_last, 100);
}

static int xy_cell(float x, float y)
{
    float q, r;
    int hq, hr;
    lattice(&V, x, y, &q, &r);
    nearest(q, r, &hq, &hr);
    return on_hex(G.n, hq, hr) ? cell_of(G.n, hq, hr) : -1;
}

static void cell_in(const view *v, int cell, float *x, float *y)
{
    at(v, (float)(cell % HV_W - (G.n - 1)), (float)(cell / HV_W - (G.n - 1)), x, y);
}

static void stone_at(int player, float x, float y, float size, float alpha)
{
    SDL_Texture *t = stones[player];
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw_board(const view *v, float px, float py, float k, float t)
{
    int last = hv_last(&G), swap_ghost = ghost.on && swappable(ghost.cell);
    float s = v->s, x, y;
    ui_put(t_board, px, py, bw * k, bh * k);
    if (last >= 0 && !anim.on) {
        cell_in(v, last, &x, &y);
        ui_put_c(t_last, x, y, last_size * k, last_size * k);
    }
    for (int i = 0; i < HV_SQ; i++) {
        if (G.cell[i] == HV_EMPTY || (anim.on && i == anim.cell)) continue;
        cell_in(v, i, &x, &y);
        ui_shadow_flat(x, y, s, s, 1);
    }
    for (int i = 0; i < HV_SQ; i++) {
        if (G.cell[i] == HV_EMPTY || (anim.on && i == anim.cell)) continue;
        cell_in(v, i, &x, &y);
        stone_at(G.cell[i] - 1, x, y, s, swap_ghost && i == ghost.cell ? 0.45f : 1);
    }
    if (G.winner >= 0 && G.winner != HV_DRAW && !anim.on) {   /* the winning shape in gold, once its stone is down */
        int cells[HV_SQ], kc = hv_shape_cells(&G, cells), last = hv_last(&G), far[HV_SQ], top = 1;
        for (int i = 0; i < kc; i++) {          /* each stone's distance from the last one: the light runs from it */
            int dq = cells[i] % HV_W - last % HV_W, dr = cells[i] / HV_W - last / HV_W, ds = -dq - dr;
            far[i] = (abs(dq) + abs(dr) + abs(ds)) / 2;
            top = far[i] > top ? far[i] : top;
        }
        float t = 1;
        if (shine.on) {
            t = (Sint32)(ui_ms() - shine.t0) / (float)SHINE_MS;
            if (t < 1) ui_animate();
            t = clampf(t, 0, 1);
        }
        for (int i = 0; i < kc; i++) {
            float a = clampf((0.65f * t - 0.5f * far[i] / top) / 0.15f, 0, 1);   /* each stone lit in turn */
            if (a <= 0) continue;
            cell_in(v, cells[i], &x, &y);
            float pulse = t < 1 ? 0.25f * sinf(ART_PI * clampf(t * 1.5f - far[i] / (float)top, 0, 1)) : 0;
            ui_mark_gold_a(x, y, (1.15f + pulse) * s, a);
        }
    }
    if (anim.on) {
        cell_in(v, anim.cell, &x, &y);
        if (anim.swap) {
            float g = 1 + 0.2f * sinf(ART_PI * t);
            ui_shadow_flat(x, y, s, s, 1);
            stone_at(0, x, y - 0.12f * s * sinf(ART_PI * t), s * g, 1 - t);
            stone_at(1, x, y - 0.12f * s * sinf(ART_PI * t), s * g, t);
        } else {
            float g = 1 + 0.25f * (1 - t);
            ui_shadow_flat(x, y, s, s, t);
            stone_at(G.cell[anim.cell] - 1, x, y - 0.15f * s * (1 - t), s * g, fminf(1, 2 * t));
        }
    }
    if (ghost.on && G.winner < 0) {
        cell_in(v, ghost.cell, &x, &y);
        if (swappable(ghost.cell)) stone_at(1, x, y, s, GHOST_A / 255.0f);
        else stone_at(hv_turn(&G), x, y, s, GHOST_A / 255.0f);
    }
}

static void draw_loupe(float t)
{
    if (!ghost.down || !t_zstone[0] || status() != GS_PLAY) return;
    float fx = fing.x + fing.dx, fy = fing.y + fing.dy;
    ui_loupe l = ui_loupe_at(fx, fy, V.s, LOUPE_CELLS, ui_upside(turn()), room);
    float k = made_z / V.s, px, py;
    view z = V;
    z.s = V.s * k;
    l.z = k;
    ui_loupe_map(&l, V.x0, V.y0, &z.x0, &z.y0);
    ui_loupe_map(&l, bx, by, &px, &py);
    ui_loupe_begin(&l);
    stones = t_zstone;
    draw_board(&z, px, py, k, t);
    stones = t_stone;
    ui_loupe_end();
}

static void draw(void)
{
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.swap ? SWAP_MS : PLACE_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    draw_board(&V, bx, by, 1, t);
    draw_loupe(t);
}

/* ---- the pointer and the keys ------------------------------------------------------------------------------------ */
static int two_taps(void) { return ui_loupe_small(V.s); }

static void aim(int cell, int loud)
{
    ghost.on = cell >= 0 && (hv_legal(&G, cell) || swappable(cell));
    ghost.cell = cell;
    if (!ghost.on && loud) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    if (status() != GS_PLAY || landing()) return;
    fing.x = x, fing.y = y, fing.dx = fing.dy = 0;
    if (ghost.on && ui_loupe_small(V.s)) {
        float gx, gy;
        cell_in(&V, ghost.cell, &gx, &gy);
        ui_loupe_snap(x, y, gx, gy, 0.8f * V.s, &fing.dx, &fing.dy);
    }
    int cell = xy_cell(x + fing.dx, y + fing.dy);
    if (cell < 0) { ghost.on = 0; return; }
    ghost.down = 1;
    ghost.moved = 0;
    ghost.armed = ghost.on && ghost.cell == cell;
    if (!ghost.armed) aim(cell, 1);
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ghost.down) return;
    fing.x = x, fing.y = y;
    ui_animate();
    int cell = xy_cell(x + fing.dx, y + fing.dy);
    if (cell < 0 || cell == ghost.cell) return;
    ghost.moved = 1;
    aim(cell, 0);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ghost.down) return;
    ghost.down = 0;
    if (!ghost.on) {
        if (ghost.moved) ui_sound(SND_DENY);
        return;
    }
    if ((ghost.armed && !ghost.moved) || (!two_taps() && !swappable(ghost.cell))) play(ghost.cell);
}

/* the arrows move the ghost a cell (as seen on the screen), Return or Space plays it */
static void key(SDL_Keycode k)
{
    if (status() != GS_PLAY || landing() || (ui_bot_seats() >> turn() & 1)) return;
    if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_KP_ENTER) {
        if (ghost.on) play(ghost.cell);
        else ui_sound(SND_DENY);
        return;
    }
    float dx = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0, dy = k == SDLK_UP ? -1 : k == SDLK_DOWN ? 1 : 0;
    if (!dx && !dy) return;
    int from = ghost.cell;
    if (!hv_valid(G.n, from)) from = cell_of(G.n, 0, 0);
    else {
        float x, y, best = 1e9f;
        int to = -1;
        cell_in(&V, from, &x, &y);
        for (int i = 0; i < 6; i++) {           /* the neighbour most nearly that way on the screen */
            int a = from % HV_W + HV_DX[i], b = from / HV_W + HV_DY[i];
            if (!hv_on(G.n, a, b)) continue;
            float nx, ny;
            cell_in(&V, b * HV_W + a, &nx, &ny);
            float ux = (nx - x) / V.s, uy = (ny - y) / V.s, along = ux * dx + uy * dy;
            if (along < 0.3f) continue;
            float score = -along + 0.6f * fabsf(ux * dy - uy * dx);
            if (score < best) best = score, to = b * HV_W + a;
        }
        if (to < 0) { ui_sound(SND_DENY); return; }
        from = to;
    }
    ghost.on = hv_legal(&G, from) || swappable(from);
    ghost.cell = from;
    ui_animate();
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, player, size, rgba); }

/* the menu's card: a small board, ivory's ring round an ebony stone; the picker's: the boards with a few stones (the
 * swap's: ivory's first stone turned half ebony) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int menu = variant < 0, n = menu ? 4 : size_of(variant);
    int f = (int)(size * 0.04f), w = 9 * ((size - 2 * f) / 9) + 2 * f, o = (size - w) / 2;
    view v;
    fit(&v, n, 0, (float)(w - 2 * f), (float)(w - 2 * f), w / 2.0f, w / 2.0f);
    uint8_t *board = ui_buf(w, w);
    paint_board(board, w, w, f, 1, &v);
    art_blend(rgba, size, size, board, w, w, o, o);
    /* (q, r, player): the menu's ring of six ivory round ebony's stone, more ebony beside; the picker's in parts */
    static const int menu_st[][3] = {{1, -1, 0}, {2, -1, 0}, {2, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 0},
                                     {1, 0, 1}, {-1, 0, 1}, {-1, 2, 1}, {-2, 1, 1}, {0, -2, 1}};
    static const float pick_st[][3] = {{0, 0, 0}, {0.34f, -0.34f, 1}, {-0.34f, 0.67f, 0}, {0.67f, 0, 1},
                                       {-0.67f, 0.34f, 1}, {0.34f, 0.34f, 0}};
    int k = menu ? (int)(sizeof menu_st / sizeof menu_st[0]) : (int)(sizeof pick_st / sizeof pick_st[0]);
    int s = (int)lroundf(v.s);
    for (int i = 0; i < k; i++) {
        float q = menu ? (float)menu_st[i][0] : roundf(pick_st[i][0] * (n - 1));
        float r = menu ? (float)menu_st[i][1] : roundf(pick_st[i][1] * (n - 1));
        int who = menu ? menu_st[i][2] : (int)pick_st[i][2];
        float px, py;
        at(&v, q, r, &px, &py);
        uint8_t *one = ui_buf(s, s);
        art_counter(COUNTER_STONE, who, s, one);
        art_blend(rgba, size, size, one, s, s, o + (int)lroundf(px - s / 2.0f), o + (int)lroundf(py - s / 2.0f));
        if (!menu && variant >= 2 && i == 0) {  /* the swap: the first stone's right half turned ebony */
            art_counter(COUNTER_STONE, 1, s, one);
            for (int y = 0; y < s; y++)
                for (int x = 0; x < s / 2; x++) one[4 * (y * s + x) + 3] = 0;
            art_blend(rgba, size, size, one, s, s, o + (int)lroundf(px - s / 2.0f), o + (int)lroundf(py - s / 2.0f));
        }
    }
}

static const game havannah_game = {
    .id = "havannah", .order = 102, .variants = 4, .variant_first = 1,   /* 6, 8, then the same with the swap */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy, .fill = 1, .key = key,
};
REGISTER_GAME(havannah_game)
