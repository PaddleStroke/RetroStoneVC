/*
 * Futoshiki in RetroStone Board, for one player: the house plank with n x n cells inlaid in it (a given digit's cell
 * a darker wood), the signs burned into the wood between them, the given digits in dark ink and the player's in blue
 * ink, a row of wooden keys 1..n under the board (the rules: futoshiki_rules.c; what the logic grids share: bench.h,
 * bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row and column are lit faintly, and every cell holding its digit warmly (a key tapped with no empty cell picked
 * lights its digit's cells instead). The shell's tools: the pencil (notes: the keys then put small digits in the
 * cell, or take them out), the eraser (the cell's digit, or its notes), the lamp (a hint: a wrong digit put right,
 * else the easiest cell filled, in gold light) and the eye (clashes shown: a digit twice in a line turns red, and so
 * does a sign that does not hold; on at first). A key whose n digits are all on the board fades. Undo takes back one
 * action, all the way to the start. The bar: the moves. Solved: a wave of gold light from the last cell; the stars
 * count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/futoshiki_gen.c: futoshiki_levels.h),
 * or its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/futoshiki.h"
#include "games/futoshiki_levels.h"
#include "games/race.h"
#include "shell.h"

#define GAP 0.42f                               /* the room between two cells (the signs), a part of a cell */
#define MARGIN 0.32f                            /* round the cells, inside the frame */

static const float CELL_TONE[3] = {1.07f, 1.06f, 1.04f}, GIVEN_TONE[3] = {0.88f, 0.86f, 0.84f};

static fsk_game G;
static bench_dg dg;
static int serial;

static box last_box;
static float bx, by, bw, q, pitch, ox, oy;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_small, *t_sign;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a sign: a chevron pointing at the lesser side; dir 0 left, 1 up, 2 right, 3 down (a unit square) */
static void sign_fig(art_fig *f, int dir)
{
    float p[3][2] = {{0.66f, 0.2f}, {0.3f, 0.5f}, {0.66f, 0.8f}};
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < dir; k++) {         /* a quarter turn clockwise round the middle */
            float x = p[i][0] - 0.5f, y = p[i][1] - 0.5f;
            p[i][0] = 0.5f - y;
            p[i][1] = 0.5f + x;
        }
    art_band(f, ART_PART, p[0][0], p[0][1], p[1][0], p[1][1], 0.2f);
    art_band(f, ART_PART, p[1][0], p[1][1], p[2][0], p[2][1], 0.2f);
    art_ellipse(f, ART_PART, p[1][0], p[1][1], 0.1f, 0.1f);
}

/* the cells' places: cell (x, y)'s top left corner in a board whose grid starts at (o, o), cells c wide */
static float place_of(int i, float o, float c) { return o + i * c * (1 + GAP); }

/* the board: the plank, each cell a lighter inlay (a given digit's darker) */
static void paint_board(uint8_t *o, int w, int f, const fsk_puz *p, float c, float org)
{
    int n = p->n;
    bench_plank(o, w, w, f);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            race_inlay(o, w, w, place_of(x, org, c), place_of(y, org, c), c, c, 0.08f * c,
                       p->g[y * n + x] ? GIVEN_TONE : CELL_TONE, fmaxf(1.5f, 0.07f * c));
}

/* the signs and the given digits drawn into a picture (the thumbnails) */
static void paint_clues(uint8_t *o, int w, const fsk_puz *p, float c, float org, const uint8_t *shown)
{
    int n = p->n;
    art_style st = art_flat(PUZ_INK, PUZ_INK, 0, 0);
    for (int cell = 0; cell < n * n; cell++) {
        int x = cell % n, y = cell / n;
        float cx = place_of(x, org, c) + c / 2, cy = place_of(y, org, c) + c / 2, s = GAP * c * 1.3f;
        for (int way = 0; way < 2; way++) {
            int r = p->rel[way][cell];
            if (!r) continue;
            art_fig *f = art_fig_new();
            sign_fig(f, way ? (r == FSK_LT ? 1 : 3) : (r == FSK_LT ? 0 : 2));
            float sx = way ? cx : cx + c * (1 + GAP) / 2, sy = way ? cy + c * (1 + GAP) / 2 : cy;
            art_fig_blend(o, w, w, f, &st, (int)(sx - s / 2), (int)(sy - s / 2), (int)s, 1);
            art_fig_free(f);
        }
        int d = shown ? shown[cell] : p->g[cell];
        if (d)
            puz_digits(o, w, w, d, cx, cy + 0.03f * c, 0.66f * c, p->g[cell] ? BENCH_GIVEN : BENCH_BLUE, 1,
                       p->g[cell] != 0);
    }
}

/* the token: a little tile with 2 < 5 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.92f), h = (int)(size * 0.6f);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    bench_tile(b, w, h, 0.16f * h, CELL_TONE);
    puz_digits(b, w, h, 2, w * 0.22f, h * 0.53f, 0.66f * h, BENCH_GIVEN, 1, 1);
    puz_digits(b, w, h, 5, w * 0.78f, h * 0.53f, 0.66f * h, BENCH_BLUE, 1, 0);
    art_fig *f = art_fig_new();
    sign_fig(f, 0);
    art_style st = art_flat(PUZ_INK, PUZ_INK, 0, 0);
    art_fig_blend(b, w, h, f, &st, (int)(w / 2.0f - 0.25f * h), (int)(h / 2.0f - 0.25f * h), (int)(0.5f * h), 1);
    art_fig_free(f);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.n ? G.n : 5;
    bench_pad_layout(&dg.pad, b, n);
    box sq = dg.pad.board;
    bw = sq.w;
    fr = (int)(bw * BENCH_FRAME);
    q = floorf((bw - 2 * fr) / (n + GAP * (n - 1) + 2 * MARGIN));
    pitch = q * (1 + GAP);
    float grid = n * q + (n - 1) * GAP * q, org = floorf((bw - grid) / 2);
    bx = sq.x;
    by = sq.y;
    ox = bx + org;
    oy = by + org;
    dg.cs = q;
    int key = ((int)bw * 16 + n) * 64 + serial % 64;
    if (made_key != key) {
        made_key = key;
        int w = (int)bw;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, &G.p, q, org);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int s = (int)(GAP * q * 1.25f);
        ui_free(&t_sign);
        memset(ui_buf(s, s), 0, (size_t)s * s * 4);
        art_fig *f = art_fig_new();
        sign_fig(f, 0);
        art_style st = art_flat(PUZ_WHITE, PUZ_WHITE, 0, 0);
        art_render(f, &st, s, s, (float)s, 0, 0, ui_buf(s, s));
        art_fig_free(f);
        t_sign = ui_tex(s, s, ui_buf(s, s));
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* its top left corner */
{
    *x = ox + (c % G.n) * pitch;
    *y = oy + (c / G.n) * pitch;
}

static int cell_at(float x, float y)
{
    int n = G.n;
    float ix = (x - ox + GAP * q / 2) / pitch, iy = (y - oy + GAP * q / 2) / pitch;   /* the gaps shared out */
    if (ix < 0 || iy < 0 || ix >= n || iy >= n) return -1;
    return (int)iy * n + (int)ix;
}

static int editable(int c) { return c >= 0 && c < G.n * G.n && !G.p.g[c]; }
static int given(int c) { return G.p.g[c] != 0; }
static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return fsk_put(&G, c, d); }
static int note(int c, int d) { return fsk_note(&G, c, d); }
static int erase(int c) { return fsk_erase(&G, c); }
static int hint(void) { return fsk_hint(&G); }
static int solved(void) { return fsk_solved(&G); }
static int clash(int c) { return fsk_clash(&G, c); }
static int full(int d) { return fsk_count_digit(&G, d) >= G.n; }
static int peer(int s, int c) { return c / G.n == s / G.n || c % G.n == s % G.n ? 38 : 0; }

static const bench_dg_game digits = {cell_at, cell_xy, NULL, editable, given, val, notes, put, note, erase, hint,
                                     solved, clash, full, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)
{
    serial++;
    bench_dg_start(&dg, &digits, G.n, G.n);
    dg.look = BENCH_DG_LOOK;
    dg.look.nh = 0.28f;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    fsk_puz p;
    if (!fsk_decode(fsk_levels[level < 0 || level >= FSK_NLEVELS ? 0 : level], &p) || !fsk_load_level(&G, &p))
        fsk_new(&G, 3, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    fsk_new(&G, variant < 0 || variant >= FSK_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return fsk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = fsk_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = fsk_script(&G, moves);
    if (fsk_solved(&G)) bench_dg_after(&dg, G.n * G.n / 2);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a sign broken by the digits on its two sides */
static int sign_broken(int way, int c)
{
    int r = G.p.rel[way][c], o = c + (way ? G.n : 1), a = G.val[c], b = G.val[o];
    return r && a && b && ((r == FSK_LT && a >= b) || (r == FSK_GT && a <= b));
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.n, won = fsk_solved(&G);
    ui_put(t_board, bx, by, bw, bw);
    bench_dg_marks(&dg);
    float ss = GAP * q * 1.25f, x, y;           /* the signs */
    for (int c = 0; c < n * n; c++)
        for (int way = 0; way < 2; way++) {
            int r = G.p.rel[way][c];
            if (!r) continue;
            cell_xy(c, &x, &y);
            float sx = x + q / 2 + (way ? 0 : pitch / 2), sy = y + q / 2 + (way ? pitch / 2 : 0);
            int dir = way ? (r == FSK_LT ? 1 : 3) : (r == FSK_LT ? 0 : 2);
            const uint8_t *ink = dg.clash && !won && sign_broken(way, c) ? BENCH_RED : PUZ_INK;
            SDL_SetTextureColorMod(t_sign, ink[0], ink[1], ink[2]);
            ui_put_rot(t_sign, sx, sy, ss, ss, 90.0 * dir);
        }
    SDL_SetTextureColorMod(t_sign, 255, 255, 255);
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

/* ---- the pictures of puzzles: the menu's and the picker's cards (a puzzle of the size and level, its signs and
 * digits, and as many ebony stones as its level; the menu's a 5 x 5 puzzle half done), the chapters' tabs ------- */
/* a puzzle (its digits given and those shown) on a side x side plank, at the top of a size x size picture (in its
 * middle when mid) */
static void paint_puzzle(const fsk_puz *p, const uint8_t *shown, int size, int side, int mid, uint8_t *rgba)
{
    int n = p->n, f = (int)(side * BENCH_FRAME);
    float c = floorf((side - 2 * f) / (n + GAP * (n - 1) + 2 * MARGIN));
    float org = floorf((side - (n * c + (n - 1) * GAP * c)) / 2);
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    paint_board(b, side, f, p, c, org);
    paint_clues(b, side, p, c, org, shown);
    int o = (size - side) / 2;
    art_blend(rgba, size, size, b, side, side, o, mid ? o : 0);
    free(b);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, n = fsk_size_of(v);
    fsk_puz p;
    uint8_t sol[FSK_CELLS], shown[FSK_CELLS];
    fsk_make(n, fsk_level_of(v), 2026u + (uint32_t)v, &p, sol);
    for (int c = 0; c < n * n; c++) shown[c] = p.g[c] ? p.g[c] : variant < 0 && c % 3 == 0 ? sol[c] : 0;
    int side = variant < 0 || bare ? size : (int)(0.84f * size);
    paint_puzzle(&p, shown, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, fsk_level_of(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= FSK_CHAPTERS ? FSK_CHAPTERS - 1 : chapter;
    static fsk_game T;
    fsk_puz p;
    if (!fsk_decode(fsk_levels[(ch + 1) * FSK_NLEVELS / FSK_CHAPTERS - 1], &p) || !fsk_load_level(&T, &p)) return;
    paint_puzzle(&p, T.sol, size, (int)(0.92f * size), 1, rgba);
}

static const game futoshiki_game = {
    .id = "futoshiki", .order = 414, .variants = FSK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {FSK_MAX - FSK_MIN + 1, 3}, .axis_pic = {size_pic, NULL},
    .levels = FSK_NLEVELS, .chapters = FSK_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS,
};
REGISTER_GAME(futoshiki_game)
