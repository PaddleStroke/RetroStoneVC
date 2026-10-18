/*
 * Twin Walkers in RetroStone Board, for one player: a floor of the house plank ruled in tiles, the walls raised blocks
 * of dark walnut, the two walkers little round creatures of red lacquer and lapis (the first with a sprout on its head,
 * the mirror walker with a feeler), each exit a mat of its walker's colour with a star; crumbling tiles pale and
 * crazed, plates of brass with a jewel and the doors of their colour (a gate of bars, gone faint while open), ice
 * pale blue (the rules: twinwalk_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the floor, tap one of the four arrows (the shell's tools), tap the floor (the way from the first walker to
 * the finger; nearer the mirror walker, its way seen from it), or use the arrow keys: both walkers hop at once, a
 * walker stopped nudges against what stops it. The lamp: the hint (the next move of a shortest way, its arrow in gold
 * light and the tiles the walkers would reach ringed; from a dead end, the hint first takes back the moves that led
 * there). Undo takes back one move, all the way to the start. The bar: the moves, and at its right end the par (the
 * fewest possible) by a flag. Solved: both walkers jump on their exits in gold light; at par the light runs out over
 * the floor.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "games/twinwalk.h"
#include "shell.h"

#define FRAME 0.035f
#define STEP_MS 150                             /* one tile, both walkers together */
#define BUMP_MS 220
#define FALL_MS 420
#define DOOR_MS 180
#define JUMP_MS 900
#define WAVE_STEP 50
#define WAVE_LEN 520

static const int COL[2] = {2, 5};               /* the walkers' colours: red lacquer, lapis */
static const uint8_t JEWEL[TWK_GROUPS][3] = {{62, 160, 112}, {232, 170, 52}};   /* jade, amber */
static const uint8_t BRASS[3] = {214, 170, 84}, ICE[3] = {214, 232, 246}, SAND[3] = {226, 210, 176};
static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

static twk_game G;
static struct { int on, ticks; twk_state from; twk_trace tr; Uint32 t0; } mv;
static struct { int on; Uint32 t0; } win;
static Uint32 fall_t0[TWK_MAXCR];
static uint8_t door_open[TWK_CELLS];
static Uint32 door_t0[TWK_CELLS];
static int facing[2] = {TWK_DOWN, TWK_DOWN}, hint_way = -1, down = -1, serial;
static swp_swipe sw;

static box last_box;
static float ox, oy;                            /* the floor's top left on the screen (inside the frame) */
static int fr, cell, bw, bh, made_cell = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_walker[2][4], *t_door[TWK_GROUPS], *t_gap, *t_crumble, *t_par;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static int is_wall(const twk_map *m, int x, int y)   /* off the map counts as a wall: the frame */
{
    return x < 0 || y < 0 || x >= m->w || y >= m->h || m->t[x + y * m->w] == TWK_WALL;
}

static void mul(uint8_t *p, float k)
{
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
}

/* a wall's block: the wood darkened to walnut, bevelled along its open sides (lit at the top and the left), its
 * outer corners rounded; walls side by side join into one mass */
static void paint_wall(const twk_map *m, int cx, int cy, int sq, int f, uint8_t *o, int W)
{
    float b = fmaxf(2, 0.16f * sq), rad = 0.2f * sq;
    int up = !is_wall(m, cx, cy - 1), dn = !is_wall(m, cx, cy + 1), lf = !is_wall(m, cx - 1, cy),
        rt = !is_wall(m, cx + 1, cy);
    for (int y = 0; y < sq; y++)
        for (int x = 0; x < sq; x++) {
            uint8_t *p = o + 4 * ((size_t)(f + cy * sq + y) * W + f + cx * sq + x);
            float px = x + 0.5f, py = y + 0.5f;
            float ex = lf && px < rad ? rad - px : rt && px > sq - rad ? px - (sq - rad) : 0;
            float ey = up && py < rad ? rad - py : dn && py > sq - rad ? py - (sq - rad) : 0;
            if (ex > 0 && ey > 0 && hypotf(ex, ey) > rad) continue;   /* outside a rounded corner: the floor */
            float k = 0.42f;
            float dt = up ? py : 1e9f, dl = lf ? px : 1e9f, db = dn ? sq - py : 1e9f, dr = rt ? sq - px : 1e9f;
            if (ex > 0 && ey > 0) {             /* in a corner: the distance to its arc */
                float d = rad - hypotf(ex, ey);
                if (px < sq / 2.0f) dl = d; else dr = d;
                if (py < sq / 2.0f) dt = d; else db = d;
            }
            float lit = fminf(dt, dl), dark = fminf(db, dr);
            if (lit < b && lit <= dark) k *= 1 + 0.85f * (1 - lit / b) * (1 - lit / b);
            else if (dark < b) k *= 1 - 0.45f * (1 - dark / b);
            p[0] = (uint8_t)clampf(p[0] * k * 1.05f, 0, 255);
            p[1] = (uint8_t)clampf(p[1] * k * 0.95f, 0, 255);
            p[2] = (uint8_t)clampf(p[2] * k * 0.9f, 0, 255);
        }
}

/* a crumbling tile: pale sandstone, crazed */
static void paint_crumble(uint8_t *o, int W, int H, float x0, float y0, int sq, int seed)
{
    float in = 0.06f * sq;
    for (int y = (int)(y0 + in); y < (int)(y0 + sq - in); y++)
        for (int x = (int)(x0 + in); x < (int)(x0 + sq - in); x++) {
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float n = art_noise(x, y);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(SAND[i] * (1 + 0.08f * n), 0, 255);
        }
    static const uint8_t crack[3] = {120, 98, 70};
    float cx = x0 + sq * (0.42f + 0.16f * art_hash(seed, 1)), cy = y0 + sq * (0.42f + 0.16f * art_hash(seed, 2));
    for (int k = 0; k < 5; k++) {                /* cracks from a point near the middle out to the sides */
        float a = (k * 72 + 40 * art_hash(seed, k + 3)) * ART_PI / 180, l = sq * (0.3f + 0.14f * art_hash(seed, k + 9));
        float mx = cx + cosf(a + 0.3f) * l * 0.5f, my = cy + sinf(a + 0.3f) * l * 0.5f;
        pzk_stroke(o, W, H, cx, cy, mx, my, fmaxf(0.6f, 0.018f * sq), crack, 0.8f);
        pzk_stroke(o, W, H, mx, my, cx + cosf(a) * l, cy + sinf(a) * l, fmaxf(0.5f, 0.014f * sq), crack, 0.7f);
    }
    float tone[3] = {1, 1, 1};
    pzk_inlay(o, W, H, x0 + in, y0 + in, sq - 2 * in, sq - 2 * in, 0.12f * sq, tone, fmaxf(1, 0.05f * sq));
}

/* ice: pale blue, glossy streaks */
static void paint_ice(uint8_t *o, int W, int H, float x0, float y0, int sq)
{
    float in = 0.03f * sq;
    for (int y = (int)(y0 + in); y < (int)(y0 + sq - in); y++)
        for (int x = (int)(x0 + in); x < (int)(x0 + sq - in); x++) {
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float lx = x - x0, ly = y - y0, s = sinf((lx + ly) * 6.2832f / (0.9f * sq));
            float k = 1 + 0.03f * art_noise(x, y) - 0.06f * (ly / sq) + (s > 0.92f ? 0.07f : 0);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(ICE[i] * k, 0, 255);
        }
    float tone[3] = {1, 1, 1};
    pzk_inlay(o, W, H, x0 + in, y0 + in, sq - 2 * in, sq - 2 * in, 0.1f * sq, tone, fmaxf(1, 0.04f * sq));
}

/* an exit: a round mat of its walker's colour, a star on it */
static void paint_exit(uint8_t *o, int W, int H, float cx, float cy, int sq, int who)
{
    const float *fc = art_player[COL[who]].fill;
    uint8_t col[3] = {(uint8_t)fc[0], (uint8_t)fc[1], (uint8_t)fc[2]}, pale[3];
    for (int i = 0; i < 3; i++) pale[i] = (uint8_t)(col[i] + (255 - col[i]) * 0.55f);
    pzk_ball(o, W, H, cx, cy, 0.42f * sq, pale, 0.85f, 0.2f);
    pzk_arc(o, W, H, cx, cy, 0.33f * sq, 0.42f * sq, 0, 360, col, 1);
    art_fig *f = art_fig_new();
    art_star(f, ART_PART, 0.5f, 0.52f, 0.36f);
    int d = (int)(0.5f * sq);
    art_style st = art_flat(col, col, 0, 0.3f);
    art_fig_blend(o, W, H, f, &st, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f), d, 0.9f);
    art_fig_free(f);
}

/* a plate: a brass disc with its jewel; a door's slot: a groove of its colour across the tile */
static void paint_plate(uint8_t *o, int W, int H, float cx, float cy, int sq, int g)
{
    static const uint8_t dark[3] = {60, 40, 24};
    pzk_ball(o, W, H, cx + 0.03f * sq, cy + 0.04f * sq, 0.37f * sq, dark, 0.35f, 0);
    pzk_ball(o, W, H, cx, cy, 0.35f * sq, BRASS, 1, 0.9f);
    pzk_arc(o, W, H, cx, cy, 0.26f * sq, 0.29f * sq, 0, 360, dark, 0.35f);
    pzk_ball(o, W, H, cx, cy, 0.15f * sq, JEWEL[g], 1, 1);
}

static void paint_slot(uint8_t *o, int W, int H, float x0, float y0, int sq, int g)
{
    float tone[3];
    for (int i = 0; i < 3; i++) tone[i] = 0.6f + 0.4f * JEWEL[g][i] / 255.0f;
    pzk_inlay(o, W, H, x0 + 0.08f * sq, y0 + 0.08f * sq, 0.84f * sq, 0.84f * sq, 0.1f * sq, tone, fmaxf(1, 0.05f * sq));
}

/* the floor: the plank ruled in tiles, the walls raised, the special tiles and the exits; (w sq + 2 f) x (h sq + 2 f) */
static void paint_board(const twk_map *m, int sq, int f, uint8_t *o)
{
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f;
    art_board_n(m->w, m->h, sq, f, 0, o);
    for (int y = 0; y < m->h; y++)              /* the floor's seams, the walls' shadows towards the lower right */
        for (int x = 0; x < m->w; x++) {
            if (is_wall(m, x, y)) continue;
            int wl = is_wall(m, x - 1, y) && x > 0, wt = is_wall(m, x, y - 1) && y > 0;
            for (int py = 0; py < sq; py++)
                for (int px = 0; px < sq; px++) {
                    uint8_t *p = o + 4 * ((size_t)(f + y * sq + py) * W + f + x * sq + px);
                    float k = (px == 0 && x > 0) || (py == 0 && y > 0) ? 0.86f : 1;
                    float s = 0.16f * sq;
                    if (wl && px < s) k *= 0.78f + 0.22f * px / s;
                    if (wt && py < s) k *= 0.78f + 0.22f * py / s;
                    mul(p, k);
                }
        }
    for (int c = 0; c < m->w * m->h; c++) {
        int x = c % m->w, y = c / m->w;
        float x0 = f + x * sq, y0 = f + y * sq, cx = x0 + sq / 2.0f, cy = y0 + sq / 2.0f;
        switch (m->t[c]) {
        case TWK_WALL: paint_wall(m, x, y, sq, f, o, W); break;
        case TWK_CRUMBLE: paint_crumble(o, W, H, x0, y0, sq, c); break;
        case TWK_ICE: paint_ice(o, W, H, x0, y0, sq); break;
        case TWK_PLATE: paint_plate(o, W, H, cx, cy, sq, m->grp[c]); break;
        case TWK_DOOR: paint_slot(o, W, H, x0, y0, sq, m->grp[c]); break;
        default: break;
        }
        for (int i = 0; i < 2; i++)
            if (c == m->exit[i]) paint_exit(o, W, H, cx, cy, sq, i);
    }
}

/* a door's gate: a brass frame, bars of its colour (size x size) */
static void paint_gate(int g, int size, uint8_t *o)
{
    static const uint8_t dark[3] = {40, 28, 18};
    float s = (float)size, r = 0.045f * s;
    for (int k = 0; k < 4; k++) {               /* the bars, their shadows first */
        float x = s * (0.2f + 0.2f * k);
        pzk_stroke(o, size, size, x + 0.03f * s, 0.14f * s, x + 0.03f * s, 0.9f * s, r, dark, 0.4f);
        pzk_stroke(o, size, size, x, 0.12f * s, x, 0.88f * s, r, JEWEL[g], 1);
        pzk_stroke(o, size, size, x - 0.3f * r, 0.14f * s, x - 0.3f * r, 0.86f * s, 0.35f * r, PUZ_WHITE, 0.35f);
    }
    for (int k = 0; k < 2; k++) {               /* the rails */
        float y = s * (k ? 0.82f : 0.18f);
        pzk_stroke(o, size, size, 0.1f * s, y, 0.9f * s, y, 1.2f * r, BRASS, 1);
        pzk_stroke(o, size, size, 0.1f * s, y - 0.5f * r, 0.9f * s, y - 0.5f * r, 0.4f * r, PUZ_WHITE, 0.3f);
    }
}

/* a gap where a crumbling tile fell: the dark under the floor */
static void paint_gap(int size, uint8_t *o)
{
    static const uint8_t deep[3] = {14, 11, 10};
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float px = x + 0.5f, py = y + 0.5f, e = fminf(fminf(px, size - px), fminf(py, size - py));
            float a = clampf(e + 0.5f - 0.04f * size, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            float k = 1 + 2.2f * clampf(1 - fminf(px, py) / (0.25f * size), 0, 1);   /* its far walls lit a little */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(deep[i] * k, 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
}

/* a walker, seen from above at a slant: a round body of its colour on two feet, big eyes looking its way, rosy
 * cheeks; the first a little sprout on its head, the mirror walker a feeler */
static void paint_walker(int who, int way, int size, uint8_t *o)
{
    const art_style *pl = &art_player[COL[who]];
    uint8_t ink[3] = {(uint8_t)pl->outer[0], (uint8_t)pl->outer[1], (uint8_t)pl->outer[2]};
    static const uint8_t white[3] = {252, 250, 244}, pupil[3] = {26, 20, 18}, rose[3] = {250, 150, 150};
    static const uint8_t leaf[3] = {96, 170, 72}, gold[3] = {240, 196, 80};
    float lx = DX[way] * 0.05f, ly = DY[way] * 0.035f;
    art_fig *f = art_fig_new();                 /* the feet */
    art_ellipse(f, ART_PART, 0.34f, 0.88f, 0.12f, 0.07f);
    art_ellipse(f, ART_PART, 0.66f, 0.88f, 0.12f, 0.07f);
    art_fig_paint(o, size, f, ink, ink, 0, 0.3f);
    if (who == 0) {                             /* the sprout */
        f = art_fig_new();
        art_band(f, ART_PART, 0.5f, 0.24f, 0.5f, 0.12f, 0.035f);
        art_ellipse(f, ART_PART, 0.42f, 0.1f, 0.09f, 0.05f);
        art_ellipse(f, ART_PART, 0.59f, 0.09f, 0.09f, 0.05f);
        art_fig_paint(o, size, f, leaf, ink, 0.012f, 0.4f);
    } else {                                    /* the feeler */
        f = art_fig_new();
        art_band(f, ART_PART, 0.5f, 0.25f, 0.56f, 0.1f, 0.03f);
        art_fig_paint(o, size, f, ink, ink, 0, 0);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.57f, 0.09f, 0.065f, 0.065f);
        art_fig_paint(o, size, f, gold, ink, 0.012f, 0.6f);
    }
    f = art_fig_new();                          /* the body */
    art_ellipse(f, ART_PART, 0.5f, 0.55f, 0.38f, 0.35f);
    art_style st = *pl;
    st.line = 0.018f;
    st.light = 0.45f;
    art_fig_blend(o, size, size, f, &st, 0, 0, size, 1);
    art_fig_free(f);
    f = art_fig_new();                          /* a sheen at its top left */
    art_ellipse(f, ART_PART, 0.36f, 0.36f, 0.1f, 0.06f);
    art_style sh = art_flat(white, white, 0, 0);
    art_fig_blend(o, size, size, f, &sh, 0, 0, size, 0.35f);
    art_fig_free(f);
    if (way != TWK_UP) {                        /* the cheeks (turned away: not seen) */
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.27f + lx, 0.64f, 0.07f, 0.045f);
        art_ellipse(f, ART_PART, 0.73f + lx, 0.64f, 0.07f, 0.045f);
        art_style rs = art_flat(rose, rose, 0, 0);
        art_fig_blend(o, size, size, f, &rs, 0, 0, size, 0.55f);
        art_fig_free(f);
    }
    float ey = way == TWK_UP ? 0.45f : 0.52f;   /* the eyes */
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.38f + lx, ey + ly, 0.09f, 0.11f);
    art_ellipse(f, ART_PART, 0.62f + lx, ey + ly, 0.09f, 0.11f);
    art_fig_paint(o, size, f, white, ink, 0.012f, 0.1f);
    f = art_fig_new();
    float px = 1.4f * lx, py = 1.6f * ly;
    art_ellipse(f, ART_PART, 0.38f + lx + px, ey + ly + py + 0.01f, 0.05f, 0.06f);
    art_ellipse(f, ART_PART, 0.62f + lx + px, ey + ly + py + 0.01f, 0.05f, 0.06f);
    art_fig_paint(o, size, f, pupil, pupil, 0, 0);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.365f + lx + px, ey + ly + py - 0.015f, 0.018f, 0.018f);
    art_ellipse(f, ART_PART, 0.605f + lx + px, ey + ly + py - 0.015f, 0.018f, 0.018f);
    art_fig_paint(o, size, f, white, white, 0, 0);
}

static void token(int player, int size, uint8_t *rgba)   /* the two walkers side by side */
{
    (void)player;
    int d = (int)(0.62f * size);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    paint_walker(0, TWK_DOWN, d, b);
    art_blend(rgba, size, size, b, d, d, 0, (size - d) / 2);
    memset(b, 0, (size_t)d * d * 4);
    paint_walker(1, TWK_DOWN, d, b);
    art_blend(rgba, size, size, b, d, d, size - d, (size - d) / 2);
    free(b);
}

static void make_tex(SDL_Texture **t, int size, void (*paint)(int, int, uint8_t *), int k)
{
    ui_free(t);
    memset(ui_buf(size, size), 0, (size_t)size * size * 4);
    paint(k, size, ui_buf(size, size));
    *t = ui_tex(size, size, ui_buf(size, size));
}

static void gap_painter(int k, int size, uint8_t *o) { (void)k; paint_gap(size, o); }
static void crumble_painter(int k, int size, uint8_t *o)
{
    (void)k;
    paint_crumble(o, size, size, 0, 0, size, 7);
}

static void layout(box b)
{
    last_box = b;
    fr = (int)(fminf(b.w, b.h) * FRAME);
    cell = (int)fminf((b.w - 2 * fr) / G.m.w, (b.h - 2 * fr) / G.m.h);   /* the floor in all the room (game.fill) */
    bw = G.m.w * cell + 2 * fr;
    bh = G.m.h * cell + 2 * fr;
    ox = floorf(b.x + (b.w - bw) / 2) + fr;
    oy = floorf(b.y + (b.h - bh) / 2) + fr;
    if (made_cell != cell || made_serial != serial) {
        int fresh = made_cell != cell;
        made_cell = cell;
        made_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            paint_board(&G.m, cell, fr, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        if (fresh || !t_gap) {
            int d = (int)(0.86f * cell);
            for (int i = 0; i < 2; i++)
                for (int w = 0; w < 4; w++) {
                    ui_free(&t_walker[i][w]);
                    memset(ui_buf(d, d), 0, (size_t)d * d * 4);
                    paint_walker(i, w, d, ui_buf(d, d));
                    t_walker[i][w] = ui_tex(d, d, ui_buf(d, d));
                }
            for (int g = 0; g < TWK_GROUPS; g++) make_tex(&t_door[g], cell, paint_gate, g);
            make_tex(&t_gap, cell, gap_painter, 0);
            make_tex(&t_crumble, cell, crumble_painter, 0);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int solved(void) { return twk_solved(&G.m, &G.s); }
static int perfect(void) { return solved() && G.moves == G.par; }
static Uint32 move_ms(void) { return (Uint32)(mv.ticks ? mv.ticks * STEP_MS : BUMP_MS); }
static Uint32 win_len(void) { return (Uint32)(JUMP_MS + (perfect() ? (G.m.w + G.m.h) * WAVE_STEP + WAVE_LEN : 0)); }

static void calm(void)
{
    mv.on = win.on = 0;
    hint_way = down = -1;
    sw.on = 0;
    memset(fall_t0, 0, sizeof fall_t0);
    memset(door_t0, 0, sizeof door_t0);
    for (int c = 0; c < G.m.w * G.m.h; c++) door_open[c] = (uint8_t)twk_door_open(&G.m, &G.s, c);
}

static void begin(void)
{
    serial++;
    facing[0] = facing[1] = TWK_DOWN;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!twk_level(&G, level)) twk_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    twk_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (mv.on && now - mv.t0 < move_ms()) || (win.on && now - win.t0 < JUMP_MS);
}

static void push(int way)
{
    if (solved() || way < 0) return;
    twk_state from = G.s;
    twk_trace tr;
    twk_state t;
    if (!twk_try(&G.m, &G.s, way, &t, &tr)) {   /* neither can go: both nudge */
        mv.on = 1;
        mv.from = G.s;
        mv.tr = tr;
        mv.ticks = 0;
        mv.t0 = ui_ms();
        facing[0] = way;
        facing[1] = twk_mirror_way(way);
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    twk_move(&G, way);
    hint_way = -1;
    facing[0] = way;
    facing[1] = twk_mirror_way(way);
    mv.on = 1;
    mv.from = from;
    mv.tr = tr;
    mv.ticks = tr.n[0] > tr.n[1] ? tr.n[0] : tr.n[1];
    mv.t0 = ui_ms();
    for (int i = 0; i < 2; i++)                 /* the tiles that fall, as their walker steps off */
        for (int k = 0; k < tr.n[i]; k++) {
            int src = k ? tr.at[i][k - 1] : from.p[i];
            if (G.m.t[src] == TWK_CRUMBLE) {
                fall_t0[G.m.grp[src]] = mv.t0 + (Uint32)(k * STEP_MS + STEP_MS / 3);
                ui_sound_later(SND_CAPTURE, k * STEP_MS + STEP_MS / 3);
            }
        }
    ui_sound(SND_MOVE);
    for (int k = 1; k <= mv.ticks; k++) ui_sound_later(SND_PLACE, k * STEP_MS - 15);
    if (solved()) {
        win.on = 1;
        win.t0 = mv.t0 + move_ms();
        ui_sound_later(SND_END, (int)move_ms() + 80);
    }
    ui_animate();
}

static void key(SDL_Keycode k)
{
    int w = swp_key_way(k);
    if (w >= 0) push(w);
}

static void hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    int d = twk_hint(&G);
    while (d < 0 && twk_undo(&G)) d = twk_hint(&G);
    mv.on = 0;
    if (d < 0) { ui_sound(SND_DENY); return; }
    for (int c = 0; c < G.m.w * G.m.h; c++) door_open[c] = (uint8_t)twk_door_open(&G.m, &G.s, c);
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    int r = twk_undo(&G);
    calm();
    return r;
}

static int script(const char *moves)
{
    int ok = twk_script(&G, moves);
    calm();
    if (G.nh) facing[0] = facing[1] = TWK_DOWN;
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)
{
    *x = ox + (c % G.m.w + 0.5f) * cell;
    *y = oy + (c / G.m.w + 0.5f) * cell;
}

/* walker i now: its centre, its lift (pixels), its squash (1: none); *at the tile it stands on (or nearest) */
static void walker_at(int i, Uint32 now, float *x, float *y, float *lift, float *sq, int *at)
{
    *lift = 0;
    *sq = 1;
    *at = G.s.p[i];
    cell_xy(G.s.p[i], x, y);
    if (!mv.on) return;
    float t = (float)(now - mv.t0);
    if (t >= move_ms()) return;
    const twk_trace *tr = &mv.tr;
    if (tr->bump[i]) {                          /* stopped at once: a nudge its way */
        int way = i ? twk_mirror_way(facing[0]) : facing[0];
        float q = clampf(t / BUMP_MS, 0, 1), k = 0.16f * sinf(PUZ_PI * q);
        cell_xy(mv.from.p[i], x, y);
        *x += DX[way] * k * cell;
        *y += DY[way] * k * cell;
        *at = mv.from.p[i];
        return;
    }
    int k = (int)(t / STEP_MS);
    if (k >= tr->n[i]) return;                  /* it stopped before the other */
    float f = (t - k * STEP_MS) / STEP_MS;
    int from = k ? tr->at[i][k - 1] : mv.from.p[i], to = tr->at[i][k];
    float fx, fy, tx, ty;
    cell_xy(from, &fx, &fy);
    cell_xy(to, &tx, &ty);
    int slide = G.m.t[from] == TWK_ICE;
    float e = slide ? f : ui_ease(f);
    *x = fx + (tx - fx) * e;
    *y = fy + (ty - fy) * e;
    *at = f < 0.5f ? from : to;
    if (!slide) {
        *lift = ui_hop(f) * 0.8f * cell;
        if (f > 0.82f) *sq = 1 + 0.5f * (f - 0.82f);   /* landing: squashed a little */
    }
}

static void draw_walker(int i, float x, float y, float lift, float squash, float extra_lift)
{
    float d = 0.86f * cell, l = lift + extra_lift;
    ui_shadow_flat(x + 0.04f * cell, y + 0.3f * cell, 0.62f * cell * (1 - 0.25f * l / cell), 0.22f * cell, 0.5f);
    float w = d * squash, h = d / squash;
    ui_put(t_walker[i][facing[i] & 3], x - w / 2, y - h / 2 - l + (d - h) / 2, w, h);
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, ox - fr, oy - fr, (float)bw, (float)bh);
    float x[2], y[2], lift[2], sq[2];
    int at[2];
    for (int i = 0; i < 2; i++) walker_at(i, now, &x[i], &y[i], &lift[i], &sq[i], &at[i]);
    for (int c = 0; c < G.m.w * G.m.h; c++) {   /* the gaps, the tiles falling into them */
        if (G.m.t[c] != TWK_CRUMBLE || !(G.s.gone >> G.m.grp[c] & 1)) continue;
        Uint32 t0 = fall_t0[G.m.grp[c]];
        if (t0 && now < t0) continue;           /* not fallen yet: its walker still on it */
        float cx, cy;
        cell_xy(c, &cx, &cy);
        ui_put_c(t_gap, cx, cy, (float)cell, (float)cell);
        if (t0 && now < t0 + FALL_MS) {
            float q = (now - t0) / (float)FALL_MS, k = 1 - 0.55f * q * q;
            SDL_SetTextureAlphaMod(t_crumble, (Uint8)(255 * (1 - q)));
            ui_put_c(t_crumble, cx, cy + 0.1f * q * cell, cell * k, cell * k);
            SDL_SetTextureAlphaMod(t_crumble, 255);
            ui_animate();
        }
    }
    twk_state shown = G.s;                      /* the doors: by where the walkers are on the screen */
    shown.p[0] = (uint8_t)at[0];
    shown.p[1] = (uint8_t)at[1];
    for (int c = 0; c < G.m.w * G.m.h; c++) {
        if (G.m.t[c] != TWK_DOOR) continue;
        int open = twk_door_open(&G.m, &shown, c);
        if (open != door_open[c]) { door_open[c] = (uint8_t)open; door_t0[c] = now; }
        float q = door_t0[c] ? clampf((now - door_t0[c]) / (float)DOOR_MS, 0, 1) : 1;
        if (q < 1) ui_animate();
        float a = open ? 1 - q : q;             /* how shut */
        float cx, cy;
        cell_xy(c, &cx, &cy);
        SDL_Texture *t = t_door[G.m.grp[c]];
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * (0.14f + 0.86f * a)));
        ui_put_c(t, cx, cy - (1 - a) * 0.0f, (float)cell, cell * (0.55f + 0.45f * a));
        SDL_SetTextureAlphaMod(t, 255);
    }
    if (hint_way >= 0 && !mv.on) {              /* the hint: where the walkers would go */
        twk_state t;
        if (twk_try(&G.m, &G.s, hint_way, &t, NULL))
            for (int i = 0; i < 2; i++)
                if (t.p[i] != G.s.p[i]) {
                    float hx, hy;
                    cell_xy(t.p[i], &hx, &hy);
                    ui_mark_gold(hx, hy, 1.1f * cell);
                }
    }
    float wt = win.on ? (float)(int)(now - win.t0) : -1e9f;
    for (int i = 0; i < 2; i++)                 /* a walker on its exit: a soft light */
        if (at[i] == G.m.exit[i] && (!mv.on || now - mv.t0 >= move_ms()))
            ui_mark_gold_a(x[i], y[i], 1.3f * cell, win.on ? 1 : 0.45f);
    if (win.on && wt >= 0) {
        for (int i = 0; i < 2; i++) {
            float q = puz_pulse(wt, 100, JUMP_MS - 100);
            if (q > 0) ui_mark_gold(x[i], y[i], (1.3f + 1.4f * q) * cell * q);
        }
        if (perfect())                          /* at par: the light runs out over the floor */
            for (int c = 0; c < G.m.w * G.m.h; c++) {
                if (G.m.t[c] == TWK_WALL) continue;
                int ex = G.m.exit[0] % G.m.w, ey = G.m.exit[0] / G.m.w, fx = G.m.exit[1] % G.m.w,
                    fy = G.m.exit[1] / G.m.w;
                float dist = fminf(hypotf((float)(c % G.m.w - ex), (float)(c / G.m.w - ey)),
                                   hypotf((float)(c % G.m.w - fx), (float)(c / G.m.w - fy)));
                float k = puz_pulse(wt, JUMP_MS * 0.4f + dist * WAVE_STEP, WAVE_LEN), cx, cy;
                if (k <= 0) continue;
                cell_xy(c, &cx, &cy);
                ui_mark_gold(cx, cy, 1.2f * cell * k);
            }
        if (wt < (float)win_len()) ui_animate();
    }
    int order = y[0] <= y[1] ? 0 : 1;           /* the farther one first */
    for (int k = 0; k < 2; k++) {
        int i = k ? 1 - order : order;
        float jump = 0;
        if (win.on && wt >= 0 && wt < JUMP_MS) {   /* two happy jumps together */
            float q = fmodf(wt / (JUMP_MS / 2.0f), 1);
            jump = 0.35f * cell * sinf(PUZ_PI * q);
        }
        draw_walker(i, x[i], y[i], lift[i], sq[i], jump);
    }
    if (mv.on) {
        if (now - mv.t0 < move_ms()) ui_animate();
        else mv.on = 0;
    }
    unsigned can = 0;
    for (int w = 0; w < 4; w++)
        if (!solved() && twk_try(&G.m, &G.s, w, NULL, NULL)) can |= 1u << w;
    swp_tools_draw(can, mv.on ? -1 : hint_way, !solved(), down);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* a tap on the floor: the way from the walker nearer the finger (the mirror walker's seen from it, mirrored back) */
static void tap_at(float x, float y)
{
    float d[2], vx[2], vy[2];
    for (int i = 0; i < 2; i++) {
        float cx, cy;
        cell_xy(G.s.p[i], &cx, &cy);
        vx[i] = x - cx;
        vy[i] = y - cy;
        d[i] = hypotf(vx[i], vy[i]);
    }
    int i = d[1] < d[0] ? 1 : 0;
    if (d[i] < 0.4f * cell) { ui_sound(SND_DENY); return; }
    int way = swp_way(vx[i], vy[i]);
    push(i ? twk_mirror_way(way) : way);
}

static void press(float x, float y)
{
    sw.on = 0;
    down = swp_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved() && x >= ox - fr && y >= oy - fr && x < ox - fr + bw && y < oy - fr + bh) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) push(w);
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (swp_tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) push(w);
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: a map, the walkers at their starts ------------------------------------------------- */
static void paint_map(const twk_map *m, int size, uint8_t *rgba)
{
    int big = m->w > m->h ? m->w : m->h, f = (int)(size * FRAME), sq = (size - 2 * f) / big;
    if (sq < 3) return;
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f, d = (int)(0.86f * sq);
    uint8_t *o = malloc((size_t)W * H * 4), *wk = calloc((size_t)d * d, 4), *g = calloc((size_t)sq * sq, 4);
    if (o && wk && g) {
        paint_board(m, sq, f, o);
        for (int c = 0; c < m->w * m->h; c++)
            if (m->t[c] == TWK_DOOR) {
                memset(g, 0, (size_t)sq * sq * 4);
                paint_gate(m->grp[c], sq, g);
                art_blend(o, W, H, g, sq, sq, f + c % m->w * sq, f + c / m->w * sq);
            }
        for (int i = 0; i < 2; i++) {
            memset(wk, 0, (size_t)d * d * 4);
            paint_walker(i, TWK_DOWN, d, wk);
            float cx = f + (m->start[i] % m->w + 0.5f) * sq, cy = f + (m->start[i] / m->w + 0.5f) * sq;
            art_blend(o, W, H, wk, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
        }
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
    free(wk);
    free(g);
}

/* the menu's card and the random tab's: a level of that kind (the levels are read, never made here: the cards are
 * painted on a thread of their own) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LV[TWK_VARIANTS] = {22, 70, 146};
    twk_game T;
    if (twk_level(&T, variant < 0 ? 44 : LV[variant % TWK_VARIANTS])) paint_map(&T.m, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    twk_game T;
    if (twk_level(&T, level)) paint_map(&T.m, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[TWK_CHAPTERS] = {
        "A#B|...|a#b", "A.#.B|.#.#.|a...b", "A%B|%.%|a%b", "ApB|.P.|a.b", "A~B|~~~|a~b", "A%B|p~P|a.b",
    };
    twk_map m;
    if (chapter >= 0 && chapter < TWK_CHAPTERS && twk_parse(&m, ICON[chapter])) paint_map(&m, size, rgba);
}

static const game twinwalk_game = {
    .id = "twinwalk", .order = 454, .variants = TWK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = TWK_LEVELS, .chapters = TWK_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .tools = SWP_KNOBS,
};
REGISTER_GAME(twinwalk_game)
