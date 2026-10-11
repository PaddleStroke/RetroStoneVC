/*
 * Hex in RetroStone Board: the rhombus of hexagonal cells (7 x 7, 9 x 9, 11 x 11) on the plank, the stones, the swap
 * (the rules: hex_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The rhombus is wide (its long diagonal 1.7 times the short one): turned a little, its long diagonal runs from the top
 * left corner of the board's square to the bottom right, which makes its cells the biggest. Each side is painted the
 * colour of the player who joins it: ivory top and bottom, ebony left and right. A tap plays on an empty cell; on
 * 11 x 11 (small cells on a phone) the first tap shows a ghost stone and a second tap on it plays it, like go. The
 * swap: on the second move ebony taps ivory's stone, a ghost shows where it goes (its mirror cell, ebony's), a second
 * tap swaps. The script's moves: "a1 b3 swap" (hex.h).
 */
#include <math.h>
#include <string.h>

#include "games/hex.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define TILT 15.0f                              /* degrees: the long diagonal from 30 degrees to 45 (corner to
                                                 * corner) */
#define STRIP 0.55f                             /* how far the sides' colours reach beyond the cells (rows) */
#define CUT 1.2f                                /* the acute corners cut across, this far beyond the corner cell's
                                                 * centre (c + r, from the cell) */
#define FILL 0.97f                             /* how much of the plank the rhombus and its strips take */
#define TWO_TAPS 11                             /* from this size on, the first tap shows a ghost stone */
#define PLACE_MS 150
#define SWAP_MS 340
#define GHOST_A 150
#define H 0.8660254f                            /* from one row of cells to the next, in cells */

/* the board on the screen: a cell's centre at (c + r / 2, r * H) cells from the middle cell, turned by the tilt (half
 * round more when flipped) */
typedef struct { int n; float s, x0, y0, cs, sn, lw; } view;

enum { WOOD, INK, SIDE0, SIDE1 };               /* what covers a point of the plank */

static hexgame G;
static view V;
static int q, fr, made_w = -1, made_n = -1;
static float bx, by;                            /* the board's picture, its top left corner */
static SDL_Texture *t_board, *t_stone[2], *t_last;
static int last_size;
static struct { int on, cell, down, armed, moved; } ghost;
static struct { int on, from, to, swap; Uint32 t0; } anim;

static const int DC[6] = {-1, 1, 0, 1, -1, 0}, DR[6] = {0, 0, -1, -1, 1, 1};
static const float COLOUR[4][3] = {{0, 0, 0}, {34, 24, 16}, {238, 228, 206}, {56, 50, 48}};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    ghost.on = ghost.down = 0;
    anim.on = 0;
}

static void start(int variant)
{
    hex_new(&G, variant == 0 ? 7 : variant == 1 ? 9 : 11);
    calm();
}

static int status(void) { return !G.n || G.winner < 0 ? GS_PLAY : G.winner == 0 ? GS_WIN0 : GS_WIN1; }

static int turn(void) { return hex_turn(&G); }

/* is the cell ivory's first stone, which ebony may take over now? */
static int swappable(int cell) { return cell >= 0 && hex_can_swap(&G) && cell == G.move[0]; }

static void play(int cell)
{
    int swap = swappable(cell);
    if (swap ? !hex_swap(&G) : !hex_play(&G, cell)) { ui_sound(SND_DENY); return; }
    ghost.on = 0;
    anim.on = 1;
    anim.swap = swap;
    anim.from = cell;
    anim.to = hex_last(&G);
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    calm();
    return hex_undo(&G);
}

static int script(const char *s)
{
    calm();
    return hex_script(&G, s);
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
/* the view of an n x n board fitted in a plank of inner side `inner` centred on (cx, cy) */
static void fit(view *v, int n, float inner, float cx, float cy, int flip)
{
    float a = TILT * 3.14159265f / 180, h = n / 2.0f + STRIP, t = h - (n - 1) - CUT, ext = 0;
    v->n = n;
    v->cs = cosf(a);
    v->sn = sinf(a);
    /* the strips' outline (from the middle cell, a cell wide): the obtuse corners, the cut acute ones */
    const float corner[4][2] = {{h, -h}, {t, -h}, {-h, t}, {-h, h}};
    for (int i = 0; i < 4; i++) {               /* the other half is the same turned half round */
        float c = corner[i][0], r = corner[i][1], lx = c + r / 2, ly = r * H;
        ext = fmaxf(ext, fmaxf(fabsf(lx * v->cs - ly * v->sn), fabsf(lx * v->sn + ly * v->cs)));
    }
    v->s = FILL * inner / (2 * ext);
    v->lw = 0.9f + 0.025f * v->s;
    if (flip) { v->cs = -v->cs; v->sn = -v->sn; }
    v->x0 = cx;
    v->y0 = cy;
}

/* (c, r), whole or not, to pixels and back */
static void at(const view *v, float c, float r, float *x, float *y)
{
    float m = (v->n - 1) / 2.0f, lx = (c - m + (r - m) / 2) * v->s, ly = (r - m) * H * v->s;
    *x = v->x0 + lx * v->cs - ly * v->sn;
    *y = v->y0 + lx * v->sn + ly * v->cs;
}

static void lattice(const view *v, float x, float y, float *c, float *r)
{
    float m = (v->n - 1) / 2.0f, dx = x - v->x0, dy = y - v->y0;
    float lx = dx * v->cs + dy * v->sn, ly = -dx * v->sn + dy * v->cs;
    *r = ly / (H * v->s) + m;
    *c = lx / v->s - (*r - m) / 2 + m;
}

/* the cell whose hexagon holds (c, r) */
static void nearest(float c, float r, int *hc, int *hr)
{
    float y = -c - r, rx = roundf(c), ry = roundf(y), rz = roundf(r);
    float dx = fabsf(rx - c), dy = fabsf(ry - y), dz = fabsf(rz - r);
    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dz >= dy) rz = -rx - ry;
    *hc = (int)rx;
    *hr = (int)rz;
}

static int on_board(int n, int c, int r) { return c >= 0 && r >= 0 && c < n && r < n; }

/* how far (lx, ly) pixels from a cell's centre (the board not turned) is inside its hexagon of width s; < 0: outside */
static float inside(float lx, float ly, float s)
{
    float a = fabsf(lx), b = fabsf(lx / 2 + ly * H), c = fabsf(lx / 2 - ly * H);
    return s / 2 - fmaxf(a, fmaxf(b, c));
}

static float inside_cell(const view *v, float c, float r, int hc, int hr)
{
    float dc = c - hc, dr = r - hr;
    return inside((dc + dr / 2) * v->s, dr * H * v->s, v->s);
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* what covers a point of the plank: the lines round the cells, the sides' strips beyond them (split at the corners) */
static int what(const view *v, float x, float y)
{
    int n = v->n, hc, hr, a = 0, b = -1;
    float c, r, half = v->lw / 2, k = H * v->s;
    lattice(v, x, y, &c, &r);
    nearest(c, r, &hc, &hr);
    if (on_board(n, hc, hr)) return inside_cell(v, c, r, hc, hr) < half ? INK : WOOD;
    for (int i = 0; i < 6; i++)                 /* the outline of the cells along the sides */
        if (on_board(n, hc + DC[i], hr + DR[i]) && -inside_cell(v, c, r, hc + DC[i], hr + DR[i]) < half) return INK;
    float out[4] = {-0.5f - r, r - (n - 0.5f), -0.5f - c, c - (n - 0.5f)};   /* beyond the top, bottom, left, right */
    for (int i = 1; i < 4; i++) if (out[i] > out[a]) a = i;
    for (int i = 0; i < 4; i++) if (i != a && (b < 0 || out[i] > out[b])) b = i;
    float tip = fmaxf(-CUT - (c + r), (c + r) - 2 * (n - 1) - CUT);   /* beyond the cuts of the acute corners */
    if (out[a] > STRIP || tip > 0) return WOOD;
    if ((STRIP - out[a]) * k < v->lw || -tip * k < v->lw || ((a < 2) != (b < 2) && (out[a] - out[b]) * k < half))
        return INK;
    return a < 2 ? SIDE0 : SIDE1;
}

/* the cells and the strips painted on the plank o (w pixels wide) between (x0, y0) and (x1, y1): 3 x 3 samples a pixel,
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
                float a = k == INK ? 0.85f : k == WOOD ? 0 : 1;
                for (int i = 0; i < 3; i++) sum[i] += a * COLOUR[k][i];
                wood += 1 - a;
            }
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)((p[i] * wood + sum[i]) / 9 + 0.5f);
        }
}

/* the plank (9 x 9 squares of q pixels in a frame of f: w = 9 q + 2 f), the board painted on it */
static void paint_board(uint8_t *o, int w, int f, const view *v)
{
    art_board_n(9, 9, (w - 2 * f) / 9, f, 0, o);
    paint(o, w, f, f, w - f, w - f, v);
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
static void layout(box b)
{
    int n = G.n;
    q = (int)(b.w * (1 - 2 * FRAME) / 9);
    fr = (int)((b.w - 9 * q) / 2);
    int w = 9 * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    fit(&V, n, 9.0f * q, bx + w / 2.0f, by + w / 2.0f, ui.flipped);
    if (made_w == w && made_n == n) return;
    made_w = w;
    made_n = n;
    view local = V;                             /* the picture is the same turned half round */
    fit(&local, n, 9.0f * q, w / 2.0f, w / 2.0f, 0);
    ui_free(&t_board);
    paint_board(ui_buf(w, w), w, fr, &local);
    t_board = ui_tex(w, w, ui_buf(w, w));
    int s = (int)lroundf(V.s);
    for (int p = 0; p < 2; p++) {
        ui_free(&t_stone[p]);
        art_counter(COUNTER_STONE, p, s, ui_buf(s, s));
        t_stone[p] = ui_tex(s, s, ui_buf(s, s));
    }
    last_size = (int)(1.25f * V.s) + 2;
    ui_free(&t_last);
    paint_last(ui_buf(last_size, last_size), last_size, &local);
    t_last = ui_tex(last_size, last_size, ui_buf(last_size, last_size));
    SDL_SetTextureColorMod(t_last, 250, 214, 90);
    SDL_SetTextureAlphaMod(t_last, 100);
}

static void cell_xy(int cell, float *x, float *y) { at(&V, (float)(cell % G.n), (float)(cell / G.n), x, y); }

/* the cell under (x, y), or -1 */
static int xy_cell(float x, float y)
{
    float c, r;
    int hc, hr;
    lattice(&V, x, y, &c, &r);
    nearest(c, r, &hc, &hr);
    return on_board(G.n, hc, hr) ? hc + hr * G.n : -1;
}

static void stone_at(int player, float x, float y, float size, float alpha)
{
    SDL_Texture *t = t_stone[player];
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    int n = G.n, last = hex_last(&G), swap_ghost = ghost.on && swappable(ghost.cell);
    float bw = 9.0f * q + 2 * fr, s = V.s, x, y, t = 1;
    ui_put(t_board, bx, by, bw, bw);
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.swap ? SWAP_MS : PLACE_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    if (last >= 0 && !anim.on) {
        cell_xy(last, &x, &y);
        ui_put_c(t_last, x, y, (float)last_size, (float)last_size);
    }
    for (int i = 0; i < n * n; i++) {           /* the shadows first: a stone's shadow falls on the wood only */
        if (G.cell[i] == HEX_EMPTY || (anim.on && i == anim.to)) continue;
        cell_xy(i, &x, &y);
        ui_shadow_flat(x, y, s, s, swap_ghost && i == ghost.cell ? 0.45f : 1);
    }
    for (int i = 0; i < n * n; i++) {
        if (G.cell[i] == HEX_EMPTY || (anim.on && i == anim.to)) continue;
        cell_xy(i, &x, &y);
        stone_at(G.cell[i] - 1, x, y, s, swap_ghost && i == ghost.cell ? 0.45f : 1);
    }
    if (G.winner >= 0 && !anim.on) {            /* the winning chain lit in gold, once its last stone is down: the
                                                 * stones touch, so the light goes over them */
        int cells[HEX_CELLS], k = hex_chain(&G, cells);
        for (int i = 0; i < k; i++) {
            cell_xy(cells[i], &x, &y);
            ui_mark_gold(x, y, 1.15f * s);
        }
    }
    if (anim.on && anim.swap) {                 /* the swap: ivory's stone slides to its mirror cell, turning ebony */
        float x0, y0, x1, y1;
        cell_xy(anim.from, &x0, &y0);
        cell_xy(anim.to, &x1, &y1);
        x = x0 + (x1 - x0) * t;
        y = y0 + (y1 - y0) * t;
        ui_shadow_flat(x, y, s, s, 1);
        stone_at(0, x, y, s, 1 - t);
        stone_at(1, x, y, s, t);
    } else if (anim.on) {                       /* a stone set down: from a little above, a little bigger */
        float k = 1 + 0.25f * (1 - t);
        cell_xy(anim.to, &x, &y);
        ui_shadow_flat(x, y, s, s, t);
        stone_at(G.cell[anim.to] - 1, x, y - 0.15f * s * (1 - t), s * k, fminf(1, 2 * t));
    }
    if (ghost.on && G.winner < 0) {
        cell_xy(swap_ghost ? hex_mirror(n, ghost.cell) : ghost.cell, &x, &y);
        stone_at(hex_turn(&G), x, y, s, GHOST_A / 255.0f);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int two_taps(void) { return G.n >= TWO_TAPS; }

/* the ghost to the cell if a stone may go there, or ivory's first stone may be swapped (else none: loud, the
 * refusal) */
static void aim(int cell, int loud)
{
    ghost.on = cell >= 0 && (hex_legal(&G, cell) || swappable(cell));
    ghost.cell = cell;
    if (!ghost.on && loud) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int cell = xy_cell(x, y);
    if (cell < 0) { ghost.on = 0; return; }
    ghost.down = 1;
    ghost.moved = 0;
    /* the second tap on the ghost plays it (the swap's: on the stone, or on its ghost) */
    ghost.armed = ghost.on && (ghost.cell == cell || (swappable(ghost.cell) && cell == hex_mirror(G.n, ghost.cell)));
    if (!ghost.armed) aim(cell, 1);
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ghost.down) return;
    int cell = xy_cell(x, y);
    if (cell < 0 || cell == ghost.cell) return;
    ghost.moved = 1;
    aim(cell, 0);
    ui_animate();
}

/* two taps: the second plays; one tap (small boards): lifting the finger plays, the swap still asks twice */
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

/* the menu's card: the board's top right corner (where ivory's side meets ebony's), a few stones; the picker's: the
 * whole board of each size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int corner = variant < 0, n = variant == 0 ? 7 : variant == 1 ? 9 : 11;
    int f = (int)(size * (corner ? 0.06f : 0.04f)), w = 9 * ((size - 2 * f) / 9) + 2 * f, o = (size - w) / 2;
    float px, py;
    view v;
    fit(&v, n, (float)(w - 2 * f), 0, 0, 0);
    if (corner) {                               /* bigger cells, the corner cell up and right of the middle */
        v.s = (w - 2 * f) / 5.0f;
        v.lw = 0.9f + 0.025f * v.s;
        at(&v, n - 2.1f, 1.1f, &px, &py);
    } else {
        px = py = 0;
    }
    v.x0 = w / 2.0f - px;
    v.y0 = w / 2.0f - py;
    uint8_t *board = ui_buf(w, w);
    paint_board(board, w, f, &v);
    art_blend(rgba, size, size, board, w, w, o, o);
    static const int corner_st[][3] = {{0, 1, 0}, {1, 1, 1}, {2, 0, 1}, {1, 2, 0}, {3, 1, 0}, {2, 3, 1}, {3, 2, 0}};
    static const float board_st[][3] = {{0.5f, 0.5f, 0}, {0.5f, 0.17f, 1}, {0.67f, 0.33f, 0}, {0.33f, 0.67f, 1},
                                        {0.33f, 0.83f, 0}, {0.83f, 0.5f, 1}, {0.17f, 0.5f, 0}};
    int k = corner ? (int)(sizeof corner_st / sizeof corner_st[0]) : (int)(sizeof board_st / sizeof board_st[0]);
    int s = (int)lroundf(v.s);
    for (int i = 0; i < k; i++) {
        float c = corner ? n - 1 - corner_st[i][0] : lroundf(board_st[i][0] * (n - 1));
        float r = corner ? corner_st[i][1] : lroundf(board_st[i][1] * (n - 1));
        int who = corner ? corner_st[i][2] : (int)board_st[i][2];
        at(&v, c, r, &px, &py);
        uint8_t *one = ui_buf(s, s);
        art_counter(COUNTER_STONE, who, s, one);
        art_blend(rgba, size, size, one, s, s, o + (int)lroundf(px - s / 2.0f), o + (int)lroundf(py - s / 2.0f));
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the winning stone to land */

static const game hex_game = {
    .id = "hex", .order = 100, .variants = 3,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy,
};
REGISTER_GAME(hex_game)
