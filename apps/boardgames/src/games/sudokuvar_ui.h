/*
 * The Sudoku variants on the screen, shared by Killer Sudoku, Jigsaw Sudoku and Diagonal Sudoku (the rules:
 * sudokuvar.h; what the logic grids share: bench.h, bench_ui.h). A game's .c sets SVU_STYLE, includes this, and adds
 * its start(), start_random(), pictures and game record.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The house plank ruled in 9 x 9 cells, thick lines round the regions (the boxes, or a jigsaw's regions, each in its
 * own tone of wood), the diagonals' cells in honey wood, the cages' dashed lines and sums (their digits burned small in
 * each cage's first cell), the clues in dark ink on cells stained darker and the player's digits in blue ink; a row of
 * wooden keys 1..9 beside the board. Tap a cell, then a key (the same key again takes the digit out), or drag a key
 * onto a cell. The cell's peers are lit faintly (its cage a little more), and every cell holding its digit warmly.
 * The shell's tools: the pencil (notes), the eraser, the lamp (a hint: a wrong digit put right, else the cell the
 * easiest reasoning finds next, in gold light) and the eye (clashes: a digit twice in a unit or a cage, a full cage
 * missing its sum, in red; on at first). Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light runs over the board from the last cell filled; the stars count the hints and the
 * mistakes (bench.h).
 */
#ifndef SUDOKUVAR_UI_H
#define SUDOKUVAR_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/sudokuvar.h"
#include "shell.h"

enum { SVU_KILLER, SVU_JIGSAW, SVU_DIAGONAL };

#ifndef SVU_STYLE
#error "SVU_STYLE: SVU_KILLER, SVU_JIGSAW or SVU_DIAGONAL"
#endif

static sv_game G;
static bench_dg dg;

static box last_box;
static float gx, gy, gw;
static int cs, fr, made_w = -1, made_small = -1, serial, made_serial = -1;
static SDL_Texture *t_board, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const uint8_t SVU_HONEY = 1;             /* BENCH_TONE: the diagonals' wood */
static const uint8_t SVU_TONES[4] = {0, 1, 3, 4};   /* BENCH_TONE: the regions' and cages' woods, neighbours apart */

/* groups of cells (lab, n of them) given tones 0..3 so that no two side by side share one (greedily) */
static inline void svu_colour(const uint8_t *lab, int n, uint8_t *tone)
{
    for (int k = 0; k < n; k++) {
        unsigned used = 0;
        for (int c = 0; c < SV_CELLS; c++) {
            if (lab[c] != k) continue;
            int nb[4] = {c % 9 ? c - 1 : -1, c % 9 < 8 ? c + 1 : -1, c >= 9 ? c - 9 : -1, c < 72 ? c + 9 : -1};
            for (int i = 0; i < 4; i++)
                if (nb[i] >= 0 && lab[nb[i]] < k) used |= 1u << tone[lab[nb[i]]];
        }
        int t = 0;
        while (t < 3 && used >> t & 1) t++;
        tone[k] = (uint8_t)t;
    }
}

/* a cell of the picture (w wide) tinted by a wood's tone, softened by k (0: none, 1: the whole tone) */
static inline void svu_tint(uint8_t *o, int w, float x0, float y0, float x1, float y1, const float tone[3], float k)
{
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = (int)x0; x < (int)x1; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * (1 + k * (tone[i] - 1)), 0, 255);
        }
}

/* a dashed line of ink from (x0, y0) along x (way 0) or y (way 1) for len pixels, t thick */
static inline void svu_dashes(uint8_t *o, int w, float x0, float y0, int way, float len, float t, float dash,
                              float gap)
{
    for (float a = 0; a < len; a += dash + gap) {
        float b = fminf(len, a + dash);
        if (way) puz_ink_rect(o, w, w, x0 - t / 2, y0 + a, x0 + t / 2, y0 + b, PUZ_INK, 0.8f);
        else puz_ink_rect(o, w, w, x0 + a, y0 - t / 2, x0 + b, y0 + t / 2, PUZ_INK, 0.8f);
    }
}

/* the cages' dashed lines, a little inside their cells, and their sums (labels: their digits' height in cells; 0
 * none) in their first cells */
static inline void svu_cages(uint8_t *o, int w, int q, int f, const sv_shape *s, float labels)
{
    float in = 0.09f * q, t = fmaxf(1, 0.035f * q), dash = fmaxf(2, 0.12f * q), gap = fmaxf(1.5f, 0.08f * q);
    uint8_t *clean = labels > 0 ? malloc((size_t)w * w * 4) : NULL;
    if (clean) memcpy(clean, o, (size_t)w * w * 4);
#define SAME(cx, cy) ((cx) >= 0 && (cx) < 9 && (cy) >= 0 && (cy) < 9 && s->cage_of[(cy) * 9 + (cx)] == k)
    for (int c = 0; c < SV_CELLS; c++) {
        int k = s->cage_of[c], cx = c % 9, cy = c / 9;
        float x = f + cx * q, y = f + cy * q;
        for (int side = 0; side < 4; side++) {
            /* the side: 0 top, 1 bottom (along x), 2 left, 3 right (along y); its neighbour across, and the cells
             * before and after along it */
            int ox = side == 2 ? -1 : side == 3 ? 1 : 0, oy = side == 0 ? -1 : side == 1 ? 1 : 0;
            if (SAME(cx + ox, cy + oy)) continue;
            int way = side >= 2, px = way ? 0 : -1, py = way ? -1 : 0;
            float lo, hi;
            if (!SAME(cx + px, cy + py)) lo = in;   /* a corner of the cage */
            else lo = SAME(cx + px + ox, cy + py + oy) ? -in : 0;   /* turning in, or going straight on */
            if (!SAME(cx - px, cy - py)) hi = q - in;
            else hi = SAME(cx - px + ox, cy - py + oy) ? q + in : q;
            float at = side == 0 || side == 2 ? in : q - in;
            if (way) svu_dashes(o, w, x + at, y + lo, 1, hi - lo, t, dash, gap);
            else svu_dashes(o, w, x + lo, y + at, 0, hi - lo, t, dash, gap);
        }
    }
#undef SAME
    for (int k = 0; labels > 0 && k < s->ncages; k++) {
        int c = s->cage[k].cell[0], sum = s->cage[k].sum;
        float dh = labels * q, lw = (sum >= 10 ? 0.66f * dh : 0) + 0.62f * dh;
        float x = f + c % 9 * q + in * 0.6f, y = f + c / 9 * q + in * 0.6f;
        if (clean)                              /* the dashes under the label taken away */
            for (int yy = (int)(y - t); yy < (int)(y + dh + 0.12f * q) && yy < w; yy++)
                for (int xx = (int)(x - t); xx < (int)(x + lw + 0.14f * q) && xx < w; xx++)
                    if (xx >= 0 && yy >= 0) memcpy(o + 4 * ((size_t)yy * w + xx), clean + 4 * ((size_t)yy * w + xx), 4);
        puz_digits(o, w, w, sum, x + 0.06f * q + lw / 2, y + 0.05f * q + dh / 2, dh, PUZ_INK, 1, 1);
    }
    free(clean);
}

/* the plank ruled for the shape (w = 9q + 2f square): the regions' tones (a jigsaw), the diagonals' wood, the clues'
 * cells stained (given: NULL none), thin lines between the cells, thick ones round the regions, the cages (labels: as
 * svu_cages) */
static inline void svu_paint(uint8_t *o, int q, int f, const sv_shape *s, const uint8_t *given, float labels)
{
    int w = 9 * q + 2 * f;
    art_board_n(9, 9, q, f, 0, o);
    if (SVU_STYLE == SVU_JIGSAW) {
        uint8_t tone[9];
        svu_colour(s->region, 9, tone);
        for (int c = 0; c < SV_CELLS; c++) {
            float x = f + c % 9 * q, y = f + c / 9 * q;
            svu_tint(o, w, x, y, x + q, y + q, BENCH_TONE[SVU_TONES[tone[s->region[c]]]], 0.8f);
        }
    }
    if (s->diag)
        for (int i = 0; i < 9; i++)
            for (int k = 0; k < 2; k++) {
                float x = f + (k ? 8 - i : i) * q, y = f + i * q;
                svu_tint(o, w, x, y, x + q, y + q, BENCH_TONE[SVU_HONEY], 0.9f);
            }
    float in = fmaxf(1, 0.04f * q);
    for (int c = 0; given && c < SV_CELLS; c++)
        if (given[c]) {
            float x = f + c % 9 * q, y = f + c / 9 * q;
            bench_shade(o, w, w, x + in, y + in, x + q - in, y + q - in, BENCH_GIVEN_SHADE);
        }
    float thin = fmaxf(1.0f, q * 0.026f), thick = fmaxf(2.0f, q * 0.07f), lo = (float)f, hi = (float)(w - f);
    float ta = s->ncages ? 0.32f : 0.5f;
    for (int i = 1; i < 9; i++) {
        float p = f + i * q;
        puz_ink_rect(o, w, w, p - thin / 2, lo, p + thin / 2, hi, PUZ_INK, ta);
        puz_ink_rect(o, w, w, lo, p - thin / 2, hi, p + thin / 2, PUZ_INK, ta);
    }
    for (int c = 0; c < SV_CELLS; c++) {        /* thick where two regions meet */
        int x = c % 9, y = c / 9;
        if (x < 8 && s->region[c] != s->region[c + 1]) {
            float p = f + (x + 1) * q;
            puz_ink_rect(o, w, w, p - thick / 2, fmaxf(lo, f + y * q - thick / 2), p + thick / 2,
                         fminf(hi, f + (y + 1) * q + thick / 2), PUZ_INK, 0.88f);
        }
        if (y < 8 && s->region[c] != s->region[c + 9]) {
            float p = f + (y + 1) * q;
            puz_ink_rect(o, w, w, fmaxf(lo, f + x * q - thick / 2), p - thick / 2,
                         fminf(hi, f + (x + 1) * q + thick / 2), p + thick / 2, PUZ_INK, 0.88f);
        }
    }
    puz_ink_rect(o, w, w, lo, lo, hi, lo + thick, PUZ_INK, 0.88f);
    puz_ink_rect(o, w, w, lo, hi - thick, hi, hi, PUZ_INK, 0.88f);
    puz_ink_rect(o, w, w, lo, lo, lo + thick, hi, PUZ_INK, 0.88f);
    puz_ink_rect(o, w, w, hi - thick, lo, hi, hi, PUZ_INK, 0.88f);
    if (s->ncages) svu_cages(o, w, q, f, s, labels);
}

/* a picture of a puzzle (its shape, clues and, filled: NULL none, some of the solution's digits in blue) side px
 * wide, at (ox, oy) of a size x size picture */
static inline void svu_picture(uint8_t *rgba, int size, int side, int ox, int oy, const sv_shape *s,
                               const uint8_t *clues, const uint8_t *filled)
{
    int f = (int)(side * BENCH_FRAME), q = (side - 2 * f) / 9, w = 9 * q + 2 * f;
    if (q < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    svu_paint(b, q, f, s, clues, q >= 12 ? 0.26f : 0);
    for (int c = 0; c < SV_CELLS; c++) {
        float cx = f + (c % 9 + 0.5f) * q, cy = f + (c / 9 + (s->ncages ? 0.6f : 0.54f)) * q;
        float dh = (s->ncages ? 0.5f : 0.64f) * q;
        if (clues && clues[c]) puz_digits(b, w, w, clues[c], cx, cy, dh, BENCH_GIVEN, 1, 1);
        else if (filled && filled[c]) puz_digits(b, w, w, filled[c], cx, cy, dh, BENCH_BLUE, 1, 0);
    }
    art_blend(rgba, size, size, b, w, w, ox < 0 ? (size - w) / 2 : ox, oy < 0 ? (size - w) / 2 : oy);
    free(b);
}

/* a chapter's tab: the board of a level, small: its regions' or cages' woods (so their sizes show), the diagonals,
 * its clues' cells inked */
static inline void svu_icon(uint8_t *rgba, int size, const sv_shape *s, const uint8_t *clues)
{
    int side = (int)(0.8f * size), f = (int)fmaxf(1, side * 0.05f), q = (side - 2 * f) / 9, w = 9 * q + 2 * f;
    if (q < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(9, 9, q, f, 0, b);
    uint8_t tone[SV_CELLS];
    if (s->ncages) svu_colour(s->cage_of, s->ncages, tone);
    else svu_colour(s->region, 9, tone);
    for (int c = 0; c < SV_CELLS; c++) {
        float x = f + c % 9 * q, y = f + c / 9 * q;
        if (s->ncages || SVU_STYLE == SVU_JIGSAW)
            svu_tint(b, w, x, y, x + q, y + q, BENCH_TONE[SVU_TONES[tone[s->ncages ? s->cage_of[c] : s->region[c]]]],
                     1);
        if (s->diag && (c % 10 == 0 || (c % 8 == 0 && c && c < 80)))
            svu_tint(b, w, x, y, x + q, y + q, BENCH_TONE[SVU_HONEY], 1);
        if (clues && clues[c]) puz_ink_rect(b, w, w, x + 0.5f, y + 0.5f, x + q - 0.5f, y + q - 0.5f, BENCH_GIVEN, 0.7f);
    }
    float t = fmaxf(1, 0.3f * q), lo = (float)f, hi = (float)(w - f), tc = fmaxf(1, 0.16f * q);
    for (int c = 0; s->ncages && c < SV_CELLS; c++) {   /* the cages' outlines, fine */
        int x = c % 9, y = c / 9;
        if (x < 8 && s->cage_of[c] != s->cage_of[c + 1])
            puz_ink_rect(b, w, w, f + (x + 1) * q - tc / 2, f + y * q, f + (x + 1) * q + tc / 2, f + (y + 1) * q,
                         PUZ_INK, 0.6f);
        if (y < 8 && s->cage_of[c] != s->cage_of[c + 9])
            puz_ink_rect(b, w, w, f + x * q, f + (y + 1) * q - tc / 2, f + (x + 1) * q, f + (y + 1) * q + tc / 2,
                         PUZ_INK, 0.6f);
    }
    for (int c = 0; c < SV_CELLS; c++) {
        int x = c % 9, y = c / 9;
        if (x < 8 && s->region[c] != s->region[c + 1])
            puz_ink_rect(b, w, w, f + (x + 1) * q - t / 2, f + y * q - t / 2, f + (x + 1) * q + t / 2,
                         f + (y + 1) * q + t / 2, PUZ_INK, 0.85f);
        if (y < 8 && s->region[c] != s->region[c + 9])
            puz_ink_rect(b, w, w, f + x * q - t / 2, f + (y + 1) * q - t / 2, f + (x + 1) * q + t / 2,
                         f + (y + 1) * q + t / 2, PUZ_INK, 0.85f);
    }
    puz_ink_rect(b, w, w, lo, lo, hi, lo + t, PUZ_INK, 0.85f);
    puz_ink_rect(b, w, w, lo, hi - t, hi, hi, PUZ_INK, 0.85f);
    puz_ink_rect(b, w, w, lo, lo, lo + t, hi, PUZ_INK, 0.85f);
    puz_ink_rect(b, w, w, hi - t, lo, hi, hi, PUZ_INK, 0.85f);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

/* the token: a corner of a grid of 3 x 3 cells in the variant's look, a clue in dark ink and a digit in blue */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), q = (size - 2 * f) / 3, w = 3 * q + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(3, 3, q, f, 0, b);
    for (int c = 0; c < 9; c++) {
        float x = f + c % 3 * q, y = f + c / 3 * q;
        if (SVU_STYLE == SVU_DIAGONAL && (c % 4 == 0 || c == 2 || c == 6))
            svu_tint(b, w, x, y, x + q, y + q, BENCH_TONE[SVU_HONEY], 1);
        if (SVU_STYLE == SVU_JIGSAW)
            svu_tint(b, w, x, y, x + q, y + q, BENCH_TONE[c == 0 || c == 1 || c == 3 ? 1 : 3], 0.9f);
    }
    float th = fmaxf(1, q * 0.05f), tk = fmaxf(1.5f, q * 0.1f);
    for (int i = 1; i < 3; i++) {
        float p = f + i * q;
        puz_ink_rect(b, w, w, p - th / 2, (float)f, p + th / 2, (float)(w - f), PUZ_INK, 0.45f);
        puz_ink_rect(b, w, w, (float)f, p - th / 2, (float)(w - f), p + th / 2, PUZ_INK, 0.45f);
    }
    if (SVU_STYLE == SVU_JIGSAW) {              /* a region's corner: cells 0, 1 and 3 */
        puz_ink_rect(b, w, w, f + 2.0f * q - tk / 2, (float)f, f + 2.0f * q + tk / 2, f + 1.0f * q + tk / 2, PUZ_INK, 0.9f);
        puz_ink_rect(b, w, w, f + 1.0f * q - tk / 2, f + 1.0f * q - tk / 2, f + 2.0f * q + tk / 2, f + 1.0f * q + tk / 2,
                     PUZ_INK, 0.9f);
        puz_ink_rect(b, w, w, f + 1.0f * q - tk / 2, f + 1.0f * q - tk / 2, f + 1.0f * q + tk / 2, f + 2.0f * q + tk / 2,
                     PUZ_INK, 0.9f);
        puz_ink_rect(b, w, w, (float)f, f + 2.0f * q - tk / 2, f + 1.0f * q + tk / 2, f + 2.0f * q + tk / 2, PUZ_INK, 0.9f);
    }
    if (SVU_STYLE == SVU_KILLER) {              /* a cage of two cells, its sum 12 */
        sv_shape s;
        memset(&s, 0, sizeof s);
        for (int c = 0; c < SV_CELLS; c++) s.cage_of[c] = (uint8_t)(c + 1);
        s.cage_of[0] = s.cage_of[1] = 0;
        s.ncages = 1;
        s.cage[0] = (sv_cage){2, 12, {0, 1}};
        float in = 0.11f * q, t = fmaxf(1, 0.05f * q), d = fmaxf(2, 0.16f * q), g = fmaxf(1.5f, 0.1f * q);
        svu_dashes(b, w, f + in, f + in, 0, 2.0f * q - 2 * in, t, d, g);
        svu_dashes(b, w, f + in, f + q - in, 0, 2.0f * q - 2 * in, t, d, g);
        svu_dashes(b, w, f + in, f + in, 1, q - 2 * in, t, d, g);
        svu_dashes(b, w, f + 2.0f * q - in, f + in, 1, q - 2 * in, t, d, g);
        puz_digits(b, w, w, 12, f + 0.36f * q, f + 0.3f * q, 0.3f * q, PUZ_INK, 1, 1);
        puz_digits(b, w, w, 5, f + 0.5f * q, f + 1.55f * q, 0.68f * q, BENCH_BLUE, 1, 0);
        puz_digits(b, w, w, 7, f + 1.55f * q, f + 0.6f * q, 0.56f * q, BENCH_BLUE, 1, 0);
    } else {
        puz_digits(b, w, w, 4, f + 1.5f * q, f + 1.52f * q, 0.68f * q, BENCH_GIVEN, 1, 1);
        puz_digits(b, w, w, 9, f + 2.5f * q, f + 2.52f * q, 0.68f * q, BENCH_BLUE, 1, 0);
        puz_digits(b, w, w, 1, f + 0.5f * q, f + 0.52f * q, 0.68f * q, BENCH_BLUE, 1, 0);
    }
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    bench_pad_layout(&dg.pad, b, 9);
    box sq = dg.pad.board;
    fr = (int)(sq.w * BENCH_FRAME);
    cs = (int)((sq.w - 2 * fr) / 9);
    gw = (float)(9 * cs + 2 * fr);
    gx = floorf(sq.x + (sq.w - gw) / 2);
    gy = floorf(sq.y + (sq.w - gw) / 2);
    dg.cs = (float)cs;
    if (made_w != (int)gw || made_serial != serial) {
        made_w = (int)gw;
        made_serial = serial;
        int w = (int)gw;
        ui_free(&t_board);
        uint8_t *o = ui_buf(w, w);
        svu_paint(o, cs, fr, &G.s, G.given, 0.25f);
        t_board = ui_tex(w, w, o);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* its top left corner */
{
    *x = gx + fr + (c % 9) * cs;
    *y = gy + fr + (c / 9) * cs;
}

static int cell_at(float x, float y)
{
    float ix = x - gx - fr, iy = y - gy - fr;
    if (ix < 0 || iy < 0 || ix >= 9 * cs || iy >= 9 * cs) return -1;
    return (int)(iy / cs) * 9 + (int)(ix / cs);
}

static int peer(int a, int b)                   /* its cage, then its units */
{
    if (G.s.ncages && G.s.cage_of[a] == G.s.cage_of[b]) return 52;
    return sv_sees(&G.s, a, b) ? 34 : 0;
}

static int editable(int c) { return c >= 0 && c < SV_CELLS && !G.given[c]; }
static int given(int c) { return G.given[c]; }
static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return sv_put(&G, c, d); }
static int note(int c, int d) { return sv_note(&G, c, d); }
static int erase(int c) { return sv_erase(&G, c); }
static int hint(void) { return sv_hint(&G); }
static int solved(void) { return sv_solved(&G); }
static int clash(int c) { return sv_clash(&G, c); }
static int full(int d) { return sv_count_digit(&G, d) >= 9; }

static const bench_dg_game digits = {cell_at, cell_xy, NULL, editable, given, val, notes, put, note, erase, hint,
                                     solved, clash, full, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)                         /* a new puzzle in G */
{
    serial++;
    bench_dg_start(&dg, &digits, 9, 9);
    dg.look = G.s.ncages ? (bench_dg_look){0.1f, 0.54f, 0.27f, 0.47f, 0.23f, 0.2f, 0.2f, 0.22f}   /* under the sum */
                         : BENCH_DG_LOOK;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return sv_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = sv_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = sv_script(&G, moves);
    if (sv_solved(&G)) bench_dg_after(&dg, 40);
    return ok;
}

/* ---- the board on the screen, the finger ------------------------------------------------------------------------ */
static void draw(void)
{
    if (made_w < 0) layout(last_box);
    ui_put(t_board, gx, gy, gw, gw);
    bench_dg_marks(&dg);
    bench_dg_cells(&dg);
    bench_dg_pad(&dg);
}

static void press(float x, float y) { bench_dg_press(&dg, x, y); }
static void motion(float x, float y) { bench_dg_motion(&dg, x, y); }
static void release(float x, float y) { bench_dg_release(&dg, x, y); }

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* the fields every variant's game record shares */
#define SVU_GAME_FIELDS                                                                                            \
    .token = token, .layout = layout, .draw = draw, .press = press, .motion = motion, .release = release,           \
    .undo = undo, .turn = turn, .status = status, .bar = bar, .script = script, .busy = busy, .players = 1u << 1,   \
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS

#endif
