/*
 * Y in RetroStone Board: the triangle of hexagonal cells (9, 13, 19 a side) on the plank, the stones, the swap (the
 * rules: ygame_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The plank takes the whole room the shell gives (game.fill); the triangle stands on its bottom side, or lies on it
 * turned a quarter (its apex to the right) where that makes the cells clearly bigger (a phone held upright), half
 * round more when the board is flipped. A dark band runs round the three sides (both players join all three), cut
 * across at the corners. A tap plays on an empty cell; where the cells are small for a finger, the first tap shows a
 * ghost stone and a second tap on it plays it, like go, and the shell's loupe beside the finger shows the cells
 * magnified while it is down. The swap: on the second move ebony taps ivory's stone, a ghost shows it turned ebony, a
 * second tap swaps. The script's moves: "a1 c5 swap" (ygame.h).
 */
#include <math.h>
#include <string.h>

#include "games/ygame.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define STRIP 0.55f                             /* how far the band round the sides reaches beyond the cells (rows) */
#define CUT 1.1f                                /* the corners cut across, this far beyond the corner cell's centre */
#define FILL 0.97f                              /* how much of the plank the triangle and its band take */
#define LIE 1.08f                               /* the triangle lies on its side when that makes the cells this much
                                                 * bigger */
#define PLACE_MS 150
#define SWAP_MS 340
#define GHOST_A 150
#define LOUPE_CELLS 4.2f                        /* the cells the loupe shows across */
#define H 0.8660254f                            /* from one row of cells to the next, in cells */

/* the board on the screen: a cell's centre at (c - r / 2, (r - 2 (n - 1) / 3) H) cells from the triangle's centre,
 * turned by the view's angle */
typedef struct { int n; float s, x0, y0, cs, sn, lw; } view;

enum { WOOD, INK, BAND };                       /* what covers a point of the plank */

static ygame G;
static view V;
static int fr, bw, bh, made_w = -1, made_h = -1, made_n = -1, made_z = -1, made_a = -1;
static float bx, by;                            /* the board's picture, its top left corner */
static SDL_Texture *t_board, *t_stone[2], *t_last, *t_zstone[2];   /* t_zstone: the loupe's stones */
static SDL_Texture **stones = t_stone;          /* the stones drawn now (the board's, the loupe's) */
static box room;                                /* the shell's room: the plank in it, the loupe in it */
static struct { float x, y, dx, dy; } fing;     /* the finger on the board, its offset to the ghost */
static int last_size;
static struct { int on, cell, down, armed, moved; } ghost;
static struct { int on, cell, swap; Uint32 t0; } anim;

static const int DC[6] = {-1, 1, -1, 0, 0, 1}, DR[6] = {0, 0, -1, -1, 1, 1};
static const float COLOUR[3][3] = {{0, 0, 0}, {34, 24, 16}, {84, 52, 30}};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int size_of(int variant) { return variant == 0 ? 9 : variant == 2 ? 19 : 13; }

static void calm(void)
{
    ghost.on = ghost.down = 0;
    anim.on = 0;
}

static void start(int variant)
{
    yg_new(&G, size_of(variant));
    calm();
}

static int status(void) { return !G.n || G.winner < 0 ? GS_PLAY : G.winner == 0 ? GS_WIN0 : GS_WIN1; }

static int turn(void) { return yg_turn(&G); }

/* is the cell ivory's first stone, which ebony may take over now? */
static int swappable(int cell) { return cell >= 0 && yg_can_swap(&G) && cell == G.move[0]; }

static void play(int cell)
{
    int swap = swappable(cell);
    if (swap ? !yg_swap(&G) : !yg_play(&G, cell)) { ui_sound(SND_DENY); return; }
    ghost.on = 0;
    anim.on = 1;
    anim.swap = swap;
    anim.cell = cell;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
}

/* the bot's side (bots.h): the game, read-only; a move in the notation ("c4", "swap"), played as a tap does */
const ygame *ygame_live(void) { return &G; }

int ygame_show(const char *move)
{
    int len, m = yg_cell(G.n, move, &len);
    if (status() != GS_PLAY || m == YG_NONE || move[len]) return 0;
    if (m == YG_SWAP ? !yg_can_swap(&G) : !yg_legal(&G, m)) return 0;
    calm();
    play(m == YG_SWAP ? G.move[0] : m);
    return 1;
}

static int undo(void)
{
    calm();
    return yg_undo(&G);
}

static int script(const char *s)
{
    calm();
    return yg_script(&G, s);
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
/* the outline of the triangle and its band (cells, from its centre, y down): the corners cut across */
static int outline(int n, float pt[6][2])
{
    float in = (n - 1) * H / 3 + (0.5f + STRIP) * H, cut = (2 * (n - 1) / 3.0f + CUT) * H, w = (2 * in - cut) / 1.7320508f;
    static const float U[3][2] = {{0, -1}, {0.8660254f, 0.5f}, {-0.8660254f, 0.5f}};
    for (int i = 0; i < 3; i++) {
        float ux = U[i][0], uy = U[i][1];
        pt[2 * i][0] = cut * ux - w * uy, pt[2 * i][1] = cut * uy + w * ux;
        pt[2 * i + 1][0] = cut * ux + w * uy, pt[2 * i + 1][1] = cut * uy - w * ux;
    }
    return 6;
}

/* how far the outline reaches from the centre (cells) turned by the angle a (radians): lo and hi, across and down */
static void reach(int n, float a, float lo[2], float hi[2])
{
    float pt[6][2], cs = cosf(a), sn = sinf(a);
    outline(n, pt);
    lo[0] = lo[1] = 1e9f;
    hi[0] = hi[1] = -1e9f;
    for (int i = 0; i < 6; i++) {
        float x = pt[i][0] * cs - pt[i][1] * sn, y = pt[i][0] * sn + pt[i][1] * cs;
        lo[0] = fminf(lo[0], x), hi[0] = fmaxf(hi[0], x);
        lo[1] = fminf(lo[1], y), hi[1] = fmaxf(hi[1], y);
    }
}

/* the angle (quarter turns: 0 standing, 1 lying with its apex to the right; 2 more when flipped) */
static float angle_of(int quarter) { return quarter * ART_PI / 2; }

/* the view of a board of side n turned a quarter turns, fitted in w x h, its outline's box centred on (cx, cy) */
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

/* standing (0) or lying (1) in a plank w x h: lying only where the cells get clearly bigger */
static int best_quarter(int n, float w, float h)
{
    float lo[2], hi[2], s[2];
    for (int q = 0; q < 2; q++) {
        reach(n, angle_of(q), lo, hi);
        s[q] = fminf(w / (hi[0] - lo[0]), h / (hi[1] - lo[1]));
    }
    return s[1] > LIE * s[0];
}

/* (c, r), whole or not, to pixels and back */
static void at(const view *v, float c, float r, float *x, float *y)
{
    float lx = (c - r / 2) * v->s, ly = (r - 2 * (v->n - 1) / 3.0f) * H * v->s;
    *x = v->x0 + lx * v->cs - ly * v->sn;
    *y = v->y0 + lx * v->sn + ly * v->cs;
}

static void lattice(const view *v, float x, float y, float *c, float *r)
{
    float dx = x - v->x0, dy = y - v->y0;
    float lx = dx * v->cs + dy * v->sn, ly = -dx * v->sn + dy * v->cs;
    *r = ly / (H * v->s) + 2 * (v->n - 1) / 3.0f;
    *c = lx / v->s + *r / 2;
}

/* the cell whose hexagon holds (c, r) (through the axial q = c - r, whose centre is at q + r / 2) */
static void nearest(float c, float r, int *hc, int *hr)
{
    float q = c - r, y = -q - r, rx = roundf(q), ry = roundf(y), rz = roundf(r);
    float dx = fabsf(rx - q), dy = fabsf(ry - y), dz = fabsf(rz - r);
    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dz >= dy) rz = -rx - ry;
    *hc = (int)(rx + rz);
    *hr = (int)rz;
}

static int on_board(int n, int c, int r) { return r >= 0 && r < n && c >= 0 && c <= r; }

/* how far (lx, ly) pixels from a cell's centre (the board not turned) is inside its hexagon of width s; < 0: outside */
static float inside(float lx, float ly, float s)
{
    float a = fabsf(lx), b = fabsf(lx / 2 + ly * H), c = fabsf(lx / 2 - ly * H);
    return s / 2 - fmaxf(a, fmaxf(b, c));
}

static float inside_cell(const view *v, float c, float r, int hc, int hr)
{
    float dc = c - hc, dr = r - hr;
    return inside((dc - dr / 2) * v->s, dr * H * v->s, v->s);
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* what covers a point of the plank: the lines round the cells, the band beyond them round the three sides */
static int what(const view *v, float x, float y)
{
    int n = v->n, hc, hr;
    float c, r, half = v->lw / 2, k = H * v->s;
    lattice(v, x, y, &c, &r);
    nearest(c, r, &hc, &hr);
    if (on_board(n, hc, hr)) return inside_cell(v, c, r, hc, hr) < half ? INK : WOOD;
    for (int i = 0; i < 6; i++)                 /* the outline of the cells along the sides */
        if (on_board(n, hc + DC[i], hr + DR[i]) && -inside_cell(v, c, r, hc + DC[i], hr + DR[i]) < half) return INK;
    float a = c, b = r - c, d = n - 1 - r;      /* the rows in from the left, the right and the bottom side */
    float out = fmaxf(-a, fmaxf(-b, -d)) - 0.5f, tip = fmaxf(-(a + b), fmaxf(-(a + d), -(b + d))) - CUT;
    if (out > STRIP || tip > 0) return WOOD;
    if ((STRIP - out) * k < v->lw || -tip * k < v->lw) return INK;
    return BAND;
}

/* the cells and the band painted on the plank o (w pixels wide) between (x0, y0) and (x1, y1): 3 x 3 samples a pixel,
 * but one where the pixel is well inside a cell */
static void paint(uint8_t *o, int w, int x0, int y0, int x1, int y1, const view *v)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float c, r;
            int hc, hr;
            lattice(v, x + 0.5f, y + 0.5f, &c, &r);
            nearest(c, r, &hc, &hr);
            if (on_board(v->n, hc, hr) && inside_cell(v, c, r, hc, hr) > v->lw / 2 + 1.5f) continue;
            float sum[3] = {0, 0, 0}, wood = 0;
            for (int j = 0; j < 9; j++) {
                int k = what(v, x + (j % 3 + 0.5f) / 3, y + (j / 3 + 0.5f) / 3);
                float a = k == INK ? 0.85f : k == WOOD ? 0 : 0.92f;
                for (int i = 0; i < 3; i++) sum[i] += a * COLOUR[k][i];
                wood += 1 - a;
            }
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)((p[i] * wood + sum[i]) / 9 + 0.5f);
        }
}

/* the plank (w x h with its frame of f; squares: 9 x 9 squares of the plain wood, the cards' finer grain), the board
 * painted on it */
static void paint_board(uint8_t *o, int w, int h, int f, int squares, const view *v)
{
    if (squares) art_board_n(9, 9, (w - 2 * f) / 9, f, 0, o);
    else art_plank(w - 2 * f, h - 2 * f, f, o);
    paint(o, w, f, f, w - f, h - f, v);
}

/* the last stone's cell: a hexagon of the house's warm tint (white here, tinted when drawn) */
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
/* the plank in the whole room the shell gives (game.fill), cut to the triangle standing or lying, whichever makes
 * its cells the biggest */
static void layout(box b)
{
    int n = G.n;
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
    if (made_z != zs) {                         /* the loupe's stones, crisp */
        made_z = zs;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_zstone[p]);
            if (zs) art_counter(COUNTER_STONE, p, zs, ui_buf(zs, zs)), t_zstone[p] = ui_tex(zs, zs, ui_buf(zs, zs));
        }
    }
    if (made_w == bw && made_h == bh && made_n == n && made_a == quarter) return;
    made_w = bw;
    made_h = bh;
    made_n = n;
    made_a = quarter;
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

/* the cell under (x, y), or -1 */
static int xy_cell(float x, float y)
{
    float c, r;
    int hc, hr;
    lattice(&V, x, y, &c, &r);
    nearest(c, r, &hc, &hr);
    return on_board(G.n, hc, hr) ? yg_index(hc, hr) : -1;
}

static void stone_at(int player, float x, float y, float size, float alpha)
{
    SDL_Texture *t = stones[player];
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a cell's centre in the view v */
static void cell_in(const view *v, int cell, float *x, float *y)
{
    at(v, (float)yg_col(cell), (float)yg_row(cell), x, y);
}

/* the board and all on it in the view v (the board's own, or the loupe's: the plank's top left at (px, py), k times
 * as big); t: the move's animation, 0..1 eased */
static void draw_board(const view *v, float px, float py, float k, float t)
{
    int count = yg_count(G.n), last = yg_last(&G), swap_ghost = ghost.on && swappable(ghost.cell);
    float s = v->s, x, y;
    ui_put(t_board, px, py, bw * k, bh * k);
    if (last >= 0 && !anim.on) {
        cell_in(v, last, &x, &y);
        ui_put_c(t_last, x, y, last_size * k, last_size * k);
    }
    for (int i = 0; i < count; i++) {           /* the shadows first: a stone's shadow falls on the wood only */
        if (G.cell[i] == YG_EMPTY || (anim.on && i == anim.cell)) continue;
        cell_in(v, i, &x, &y);
        ui_shadow_flat(x, y, s, s, 1);
    }
    for (int i = 0; i < count; i++) {
        if (G.cell[i] == YG_EMPTY || (anim.on && i == anim.cell)) continue;
        cell_in(v, i, &x, &y);
        stone_at(G.cell[i] - 1, x, y, s, swap_ghost && i == ghost.cell ? 0.45f : 1);
    }
    if (G.winner >= 0 && !anim.on) {            /* the winning Y lit in gold, once its last stone is down */
        int cells[YG_CELLS], kc = yg_chain(&G, cells);
        for (int i = 0; i < kc; i++) {
            cell_in(v, cells[i], &x, &y);
            ui_mark_gold(x, y, 1.15f * s);
        }
    }
    if (anim.on) {
        cell_in(v, anim.cell, &x, &y);
        if (anim.swap) {                        /* the swap: ivory's stone lifts and turns ebony where it stands */
            float g = 1 + 0.2f * sinf(ART_PI * t);
            ui_shadow_flat(x, y, s, s, 1);
            stone_at(0, x, y - 0.12f * s * sinf(ART_PI * t), s * g, 1 - t);
            stone_at(1, x, y - 0.12f * s * sinf(ART_PI * t), s * g, t);
        } else {                                /* a stone set down: from a little above, a little bigger */
            float g = 1 + 0.25f * (1 - t);
            ui_shadow_flat(x, y, s, s, t);
            stone_at(G.cell[anim.cell] - 1, x, y - 0.15f * s * (1 - t), s * g, fminf(1, 2 * t));
        }
    }
    if (ghost.on && G.winner < 0) {
        cell_in(v, ghost.cell, &x, &y);
        stone_at(yg_turn(&G), x, y, s, GHOST_A / 255.0f);
    }
}

/* the loupe while a finger is down on small cells: the board round it magnified, the ghost in it */
static void draw_loupe(float t)
{
    if (!ghost.down || !t_zstone[0] || status() != GS_PLAY) return;
    float fx = fing.x + fing.dx, fy = fing.y + fing.dy;
    ui_loupe l = ui_loupe_at(fx, fy, V.s, LOUPE_CELLS, ui_upside(turn()), room);
    float k = made_z / V.s, px, py;
    view z = V;                                 /* the same board, k times as big round the finger */
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

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int two_taps(void) { return ui_loupe_small(V.s); }

/* the ghost to the cell if a stone may go there, or ivory's first stone may be swapped (else none: loud, the
 * refusal) */
static void aim(int cell, int loud)
{
    ghost.on = cell >= 0 && (yg_legal(&G, cell) || swappable(cell));
    ghost.cell = cell;
    if (!ghost.on && loud) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    fing.x = x, fing.y = y, fing.dx = fing.dy = 0;
    if (ghost.on && ui_loupe_small(V.s)) {         /* small cells: a tap near the ghost (as shown) is on it */
        float gx, gy;
        cell_in(&V, ghost.cell, &gx, &gy);
        ui_loupe_snap(x, y, gx, gy, 0.8f * V.s, &fing.dx, &fing.dy);
    }
    int cell = xy_cell(x + fing.dx, y + fing.dy);
    if (cell < 0) { ghost.on = 0; return; }
    ghost.down = 1;
    ghost.moved = 0;
    ghost.armed = ghost.on && ghost.cell == cell;  /* the second tap on the ghost plays it */
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

/* two taps: the second plays; one tap (big cells): lifting the finger plays, the swap still asks twice */
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

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, player, size, rgba); }

/* the menu's card: a small board, ivory's Y joining the three sides among ebony's stones; the picker's: each board
 * with a few stones */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int menu = variant < 0, n = menu ? 7 : size_of(variant);
    int f = (int)(size * 0.04f), w = 9 * ((size - 2 * f) / 9) + 2 * f, o = (size - w) / 2;
    view v;
    fit(&v, n, 0, (float)(w - 2 * f), (float)(w - 2 * f), w / 2.0f, w / 2.0f);
    uint8_t *board = ui_buf(w, w);
    paint_board(board, w, w, f, 1, &v);
    art_blend(rgba, size, size, board, w, w, o, o);
    /* (c, r, player): the menu's on 7 (exact cells), the picker's as parts of the side */
    static const int menu_st[][3] = {{0, 4, 0}, {1, 4, 0}, {2, 4, 0}, {2, 3, 0}, {2, 2, 0}, {3, 5, 0}, {3, 6, 0},
                                     {1, 2, 1}, {3, 4, 1}, {4, 6, 1}, {1, 6, 1}};
    static const float pick_st[][3] = {{0.33f, 0.67f, 0}, {0.5f, 0.75f, 1}, {0.33f, 0.5f, 1}, {0.17f, 0.67f, 0},
                                       {0.5f, 1, 0}, {0.67f, 0.83f, 1}, {0.25f, 0.33f, 0}};
    int k = menu ? (int)(sizeof menu_st / sizeof menu_st[0]) : (int)(sizeof pick_st / sizeof pick_st[0]);
    int s = (int)lroundf(v.s);
    for (int i = 0; i < k; i++) {
        float c = menu ? (float)menu_st[i][0] : lroundf(pick_st[i][0] * (n - 1));
        float r = menu ? (float)menu_st[i][1] : lroundf(pick_st[i][1] * (n - 1));
        int who = menu ? menu_st[i][2] : (int)pick_st[i][2];
        float px, py;
        at(&v, c, r, &px, &py);
        uint8_t *one = ui_buf(s, s);
        art_counter(COUNTER_STONE, who, s, one);
        art_blend(rgba, size, size, one, s, s, o + (int)lroundf(px - s / 2.0f), o + (int)lroundf(py - s / 2.0f));
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the winning stone to land */

static const game ygame_game = {
    .id = "ygame", .order = 101, .variants = 3, .variant_first = 1,   /* (13 first) */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(ygame_game)
