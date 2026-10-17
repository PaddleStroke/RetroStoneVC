/*
 * Block Puzzle in RetroStone Board, for one player: a walnut tray of 10 x 10 sunk cells, wooden blocks in lacquer
 * colours, three pieces waiting beside it (below it on a phone held upright, on its right on a wide screen) (the
 * rules: blocks_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a piece onto the grid: it lifts above the finger (so the finger never hides it), its shadow shows where it
 * would land (snapped to the nearest place it fits), and the rows and columns it would make full glow. Let go there to
 * lay it; elsewhere it goes back. With taps alone: tap a piece (it rises in gold light), tap a cell (its shadow goes
 * there, centred on the cell), tap the shadow to lay it. Full rows and columns clear in a gold sparkle running out
 * from the piece, the points rising over it. A piece that fits nowhere is dimmed in its place.
 * The lamp (the shell's tool) is a hint: a piece chosen and its place blinking in gold (tap the place to lay it).
 * Undo takes back one placement at a time, all the way to the start (classic's pieces come back the same).
 * Classic (the die tab): the bar shows the score and the best (kept on the device); the end, when nothing fits: the
 * blocks dim row by row. A level: the bar shows what is left of the goal (lines, points, gold blocks, pattern cells)
 * and the pieces left; the goal reached: a gold wave over the tray and the stars card; failed: the blocks dim.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/blocks.h"
#include "games/hues.h"
#include "games/puzzles.h"
#include "shell.h"

#define PLACE_MS 140
#define STEP_MS 28                              /* the clear running along a line, a cell at a time */
#define POP_MS 300
#define RISE_MS 1000
#define DEAL_MS 260
#define BACK_MS 160
#define LIFT_MS 110
#define HINT_MS 2600
#define END_MS 1000
#define FRAME 0.42f                             /* the walnut frame, a part of a cell */
#define PAD 0.12f
#define TRAY_K 0.6f                             /* a waiting piece's cells, a part of the grid's */

static bk_game G;
static int best_score, best_read;

static box last_box;
static int tall;                                /* the tray below the grid (else on its right) */
static float q, bx, by, bs, cx0, cy0;           /* a cell; the board's top left and side; the cells' top left */
static box slot_box[3];
static float tq;                                /* a waiting piece's cell */
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_block[BK_GOLD + 1], *t_pat, *t_spark;
static SDL_Texture *t_tok, *t_star, *t_left, *t_goal[5];

static int sel = -1;                            /* the piece chosen by a tap (-1 none) */
static struct { int on, x, y; } ghost;          /* its shadow placed by a tap */
static struct {
    int on, moving, slot;
    float x, y, x0, y0;                         /* the finger; where it went down */
    Uint32 t0;
} drag;
static struct { int on, x, y, slot; Uint32 t0; } hint;
static struct {
    int on;
    Uint32 t0;
    uint8_t placed[BK_CELLS];                   /* the cells just laid */
    uint8_t gone[BK_CELLS];                     /* the cells cleared: their colour */
    uint8_t delay[BK_CELLS];                    /* steps from the piece */
    int gold, gain, lines, streak, maxd;
    float px, py;                               /* the piece's middle (cells) */
} fx;
static struct { int on; Uint32 t0; } deal;
static struct { int on, slot; float x, y; Uint32 t0; } back;
static struct { int on, won; Uint32 t0; } endfx;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void gold_leaf(uint8_t *o, int w, int h)  /* gold over the wood's grain, a sheen across it */
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float grain = p[0] / 222.0f, t = (x + y) / (float)(w + h), k = 1.18f - 0.3f * t;
            k += 0.22f * expf(-powf((t - 0.36f) * 7, 2));
            static const float gold[3] = {222, 172, 64};
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(gold[i] * (0.8f + 0.2f * grain) * k, 0, 255);
        }
}

/* a block of colour hue (hues.h 1..12, or BK_GOLD), d x d: lacquered wood, bevelled */
static void paint_block(int hue, int d, uint8_t *o)
{
    int P = 3 * d;
    uint8_t *plank = malloc((size_t)P * P * 4);
    if (!plank) return;
    art_board_n(1, 1, P, 0, 0, plank);
    int ox = (hue % 3) * d, oy = (hue / 3 % 3) * d;   /* each colour cut from its own part of the plank */
    for (int y = 0; y < d; y++) memcpy(o + 4 * (size_t)y * d, plank + 4 * ((size_t)(oy + y) * P + ox), (size_t)d * 4);
    free(plank);
    if (hue == BK_GOLD) gold_leaf(o, d, d);
    else hue_lacquer(o, d, d, hue);
    puz_bevel(o, d, d, 0.14f * d, fmaxf(1.5f, 0.12f * d), 0.42f);
    if (hue == BK_GOLD && d >= 8) {             /* a star struck into the gold */
        static const uint8_t pale[3] = {255, 240, 190}, ink[3] = {150, 104, 30};
        art_fig *f = art_fig_new();
        art_star(f, ART_PART, 0.5f, 0.53f, 0.27f);
        art_style st = art_flat(pale, ink, 0.05f, 0.4f);
        art_fig_blend(o, d, d, f, &st, 0, 0, d, 0.9f);
        art_fig_free(f);
    }
}

/* the tray: the walnut frame, its floor in shade, each cell sunk (w x w; the cells cq wide from (o, o)) */
static void paint_board(int w, int fr, float o, float cq, uint8_t *rgba)
{
    art_plank(w - 2 * fr, w - 2 * fr, fr, rgba);
    for (int y = fr; y < w - fr; y++)
        for (int x = fr; x < w - fr; x++) {
            uint8_t *p = rgba + 4 * (y * w + x);
            p[0] = (uint8_t)(p[0] * 0.8f), p[1] = (uint8_t)(p[1] * 0.8f), p[2] = (uint8_t)(p[2] * 0.82f);
        }
    static const float tone[3] = {0.6f, 0.58f, 0.56f};
    float in = fmaxf(1, 0.04f * cq);
    for (int r = 0; r < BK_N; r++)
        for (int c = 0; c < BK_N; c++)
            art_inlay(rgba, w, w, o + c * cq + in, o + r * cq + in, cq - 2 * in, cq - 2 * in, 0.12f * cq, tone,
                      fmaxf(1, 0.05f * cq));
}

/* a pattern cell: a gold diamond engraved in the cell's floor, white (tinted when drawn), d x d */
static void paint_pat(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    static const uint8_t white[3] = {255, 255, 255};
    art_fig *f = art_fig_new();
    const float dia[] = {0.5f, 0.2f, 0.8f, 0.5f, 0.5f, 0.8f, 0.2f, 0.5f};
    art_poly(f, ART_PART, dia, 4);
    art_style st = art_flat(white, white, 0, 0);
    art_render(f, &st, d, d, (float)d, 0, 0, o);
    art_fig_free(f);
    for (int i = 0; i < d * d; i++) {           /* a hollow: the diamond's middle fainter */
        int x = i % d, y = i / d;
        float m = fabsf(x + 0.5f - d / 2.0f) + fabsf(y + 0.5f - d / 2.0f);
        if (m < 0.17f * d) o[4 * i + 3] = (uint8_t)(o[4 * i + 3] * 0.35f);
    }
}

/* a sparkle: a four-pointed star of light, white, d x d */
static void paint_spark(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float u = fabsf((x + 0.5f) / d - 0.5f) * 2, v = fabsf((y + 0.5f) / d - 0.5f) * 2;
            float a = fmaxf(0, 1 - (u * 7 + v) ) + fmaxf(0, 1 - (v * 7 + u)) + 0.8f * fmaxf(0, 1 - hypotf(u, v) * 2.2f);
            uint8_t *p = o + 4 * (y * d + x);
            p[0] = 255, p[1] = 246, p[2] = 210, p[3] = (uint8_t)clampf(255 * a, 0, 255);
        }
}

/* a piece's picture: its blocks, cell c pixels, into a picture size x size centred on (cx, cy) */
static void put_piece(uint8_t *rgba, int size, int shape, float c, float cx, float cy)
{
    const bk_shape *s = &bk_shapes[shape];
    int d = (int)c - (c >= 10 ? 1 : 0);
    if (d < 2) return;
    uint8_t *t = malloc((size_t)d * d * 4);
    if (!t) return;
    paint_block(bk_colour(shape), d, t);
    for (int r = 0; r < s->h; r++)
        for (int k = 0; k < s->w; k++)
            if (s->row[r] >> k & 1)
                art_blend(rgba, size, size, t, d, d, (int)(cx + (k - s->w / 2.0f) * c), (int)(cy + (r - s->h / 2.0f) * c));
    free(t);
}

static void token(int player, int size, uint8_t *rgba)   /* three blocks in a corner */
{
    (void)player;
    put_piece(rgba, size, 9, 0.4f * size, size / 2.0f, size / 2.0f);
}

static void paint_star(int size, uint8_t *rgba)  /* the best score's mark: a gold star */
{
    memset(rgba, 0, (size_t)size * size * 4);
    static const uint8_t gold[3] = {240, 196, 80}, ink[3] = {120, 80, 20};
    art_fig *f = art_fig_new();
    art_star(f, ART_PART, 0.5f, 0.54f, 0.46f);
    art_fig_paint(rgba, size, f, gold, ink, 0.05f, 0.5f);
}

/* the goals' marks for the bar: lines (a row of blocks), points (the token), gold (a gold block), pattern */
static void paint_goal(int goal, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int d = (int)(0.3f * size);
    uint8_t *t = malloc((size_t)size * size * 4);
    if (!t) return;
    switch (goal) {
    case BK_LINES:
        for (int i = 0; i < 3; i++) {
            paint_block(4, d, t);
            art_blend(rgba, size, size, t, d, d, (int)(size / 2.0f + (i - 1.5f) * d), (size - d) / 2);
        }
        for (int y = 0; y < size; y++)          /* a gold light along it */
            for (int x = 0; x < size; x++) {
                uint8_t *p = rgba + 4 * (y * size + x);
                float a = expf(-powf((y + 0.5f - size / 2.0f) / (0.34f * size), 2)) * 0.5f;
                if (p[3] < 250) {
                    p[0] = 255, p[1] = 214, p[2] = 110;
                    p[3] = (uint8_t)fmaxf(p[3], 255 * a * (1 - fabsf(x + 0.5f - size / 2.0f) / (size / 2.0f)));
                }
            }
        break;
    case BK_SCORE: token(0, size, rgba); break;
    case BK_GOLDS: paint_block(BK_GOLD, (int)(0.8f * size), t); art_blend(rgba, size, size, t, (int)(0.8f * size),
                                                                            (int)(0.8f * size), (int)(0.1f * size),
                                                                            (int)(0.1f * size)); break;
    case BK_PATTERN:
        paint_pat(size, t);
        for (int i = 0; i < size * size; i++) t[4 * i] = 240, t[4 * i + 1] = 196, t[4 * i + 2] = 80;
        art_blend(rgba, size, size, t, size, size, 0, 0);
        break;
    default: {                                  /* the pieces left: two blocks */
        paint_block(2, (int)(0.42f * size), t);
        int e = (int)(0.42f * size);
        art_blend(rgba, size, size, t, e, e, (int)(size / 2.0f - e), (size - e) / 2);
        art_blend(rgba, size, size, t, e, e, size / 2, (size - e) / 2);
        break;
    }
    }
    free(t);
}

static SDL_Texture *tex_of(int size, void (*paint)(int, uint8_t *))
{
    uint8_t *b = ui_buf(size, size);
    paint(size, b);
    return ui_tex(size, size, b);
}

static void layout(box b)
{
    last_box = b;
    tall = b.h >= 1.15f * b.w;
    float span = BK_N + 2 * (FRAME + PAD);      /* the board, in cells */
    if (tall) q = floorf(fminf(0.98f * b.w / span, b.h / (span + 3.7f)));
    else q = floorf(fminf(0.98f * b.h / span, b.w / (span + 4.1f)));
    if (q < 4) q = 4;
    bs = floorf(span * q);
    float fr = floorf(FRAME * q), o = fr + floorf(PAD * q);
    if (tall) {
        float th = fminf(b.h - bs, 4.2f * q);   /* the tray of pieces under the board */
        bx = floorf(b.x + (b.w - bs) / 2);
        by = floorf(b.y + (b.h - bs - th) / 2);
        for (int i = 0; i < 3; i++) slot_box[i] = (box){bx + i * bs / 3, by + bs, bs / 3, th};
        tq = floorf(fminf(TRAY_K * q, fminf(bs / 3 / 5.6f, th / 5.4f)));
    } else {
        float tw = fminf(b.w - bs, 4.6f * q);   /* the tray on the board's right */
        bx = floorf(b.x + (b.w - bs - tw) / 2);
        by = floorf(b.y + (b.h - bs) / 2);
        for (int i = 0; i < 3; i++) slot_box[i] = (box){bx + bs, by + i * bs / 3, tw, bs / 3};
        tq = floorf(fminf(TRAY_K * q, fminf(tw / 5.6f, bs / 3 / 5.4f)));
    }
    cx0 = bx + o;
    cy0 = by + o;
    int key = (int)q * 8 + tall;
    if (made_key != key) {
        made_key = key;
        int w = (int)bs;
        ui_free(&t_board);
        paint_board(w, (int)fr, o, q, ui_buf(w, w));
        t_board = ui_tex(w, w, ui_buf(w, w));
        int d = (int)q - (int)fmaxf(1, roundf(0.06f * q));
        for (int c = 1; c <= BK_GOLD; c++) {
            ui_free(&t_block[c]);
            paint_block(c, d, ui_buf(d, d));
            t_block[c] = ui_tex(d, d, ui_buf(d, d));
        }
        ui_free(&t_pat);
        t_pat = tex_of(d, paint_pat);
        ui_free(&t_spark);
        t_spark = tex_of((int)(0.8f * q), paint_spark);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 2) {
        made_small = small;
        ui_free(&t_tok);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_tok = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_star);
        t_star = tex_of(small, paint_star);
        for (int g = 0; g < 5; g++) {
            ui_free(&t_goal[g]);
            paint_goal(g, small, ui_buf(small, small));
            t_goal[g] = ui_tex(small, small, ui_buf(small, small));
        }
        t_left = t_goal[0];
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    ghost.on = drag.on = hint.on = fx.on = back.on = endfx.on = 0;
}

static void read_best(void)
{
    if (best_read) return;
    best_read = 1;
    uint8_t b[4];
    if (ui_save_get("blocks/best", b, 4) == 4) best_score = b[0] | b[1] << 8 | b[2] << 16 | (b[3] & 0x7f) << 24;
}

static void keep_best(void)
{
    if (G.goal != BK_CLASSIC || G.p.k.score <= best_score) return;
    best_score = G.p.k.score;
    uint8_t b[4] = {(uint8_t)best_score, (uint8_t)(best_score >> 8), (uint8_t)(best_score >> 16),
                    (uint8_t)(best_score >> 24)};
    ui_save_set("blocks/best", b, 4);
}

static void begin(void)
{
    calm();
    deal.on = 1;
    deal.t0 = ui_ms();
    if (last_box.w > 0) layout(last_box);
    ui_animate();
}

static void start(int level)                    /* a designed level */
{
    if (!bk_level(&G, level)) bk_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* classic: endless, the pieces by chance */
{
    (void)variant;
    read_best();
    bk_new_classic(&G, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void)
{
    if (G.goal == BK_CLASSIC) return bk_over(&G) ? GS_WIN0 : GS_PLAY;   /* classic: over, never lost */
    return bk_solved(&G) ? GS_WIN0 : bk_over(&G) ? GS_WIN1 : GS_PLAY;
}
static int turn(void) { return 0; }
static int score(void) { return G.goal == BK_CLASSIC ? G.p.k.score : G.p.k.placed; }
static int par(void) { return G.goal == BK_CLASSIC ? 0 : G.par; }
static int level_stars(void) { return G.goal == BK_CLASSIC ? bk_stars(&G) : ui_par_stars(G.p.k.placed, G.par); }

static Uint32 fx_len(void) { return fx.lines ? (Uint32)(PLACE_MS / 2 + fx.maxd * STEP_MS + POP_MS) : PLACE_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (fx.on && now - fx.t0 < fx_len()) || (endfx.on && now - endfx.t0 < END_MS) || drag.on;
}

static void ended(Uint32 at)
{
    if (!bk_over(&G)) return;
    endfx.on = 1;
    endfx.won = bk_solved(&G);
    endfx.t0 = at;
}

static int undo(void)
{
    calm();
    return bk_undo(&G);
}

static void lay(int slot, int x, int y);

/* the moves at once, the last one laid as a finger would (its clear animating: the pictures' --at) */
static int script(const char *moves)
{
    calm();
    int nh = G.nh, ok = bk_script(&G, moves);
    if (ok && G.nh > nh) {
        uint8_t mv[2] = {G.hist[G.nh - 1].mv[0], G.hist[G.nh - 1].mv[1]};
        bk_undo(&G);
        lay(mv[0], mv[1] % BK_N, mv[1] / BK_N);
        return 1;
    }
    keep_best();
    ended(ui_ms());
    return ok;
}

static void lay(int slot, int x, int y)
{
    bk_core before = G.p.k;
    uint8_t col[BK_CELLS];
    memcpy(col, G.p.col, sizeof col);
    int sh = G.p.k.hand[slot];
    if (!bk_place(&G, slot, x, y)) { ui_sound(SND_DENY); return; }
    const bk_shape *s = &bk_shapes[sh];
    memset(&fx, 0, sizeof fx);
    for (int r = 0; r < s->h; r++)
        for (int c = 0; c < s->w; c++)
            if (s->row[r] >> c & 1) {
                fx.placed[(y + r) * BK_N + x + c] = 1;
                col[(y + r) * BK_N + x + c] = (uint8_t)bk_colour(sh);
            }
    const bk_result *res = &G.last;
    fx.px = x + s->w / 2.0f;
    fx.py = y + s->h / 2.0f;
    fx.lines = res->lines;
    fx.gain = res->gain;
    fx.gold = res->golds;
    fx.streak = res->streak;
    for (int i = 0; i < BK_CELLS && res->lines; i++) {
        int cx = i % BK_N, cy = i / BK_N;
        if (!(res->rows >> cy & 1) && !(res->cols >> cx & 1)) continue;
        fx.gone[i] = col[i];
        float dx = fabsf(cx + 0.5f - fx.px), dy = fabsf(cy + 0.5f - fx.py);
        int dd = (int)((res->rows >> cy & 1) && (res->cols >> cx & 1) ? fminf(dx, dy) : res->rows >> cy & 1 ? dx : dy);
        fx.delay[i] = (uint8_t)dd;
        if (dd > fx.maxd) fx.maxd = dd;
    }
    (void)before;
    fx.on = 1;
    fx.t0 = ui_ms();
    sel = -1;
    ghost.on = hint.on = 0;
    if (bk_hand_empty(&before) || (G.p.k.hand[0] != BK_NONE && G.p.k.hand[1] != BK_NONE && G.p.k.hand[2] != BK_NONE &&
                                   !bk_over(&G))) {
        deal.on = 1;                            /* three new pieces */
        deal.t0 = fx.t0 + 60;
    }
    ui_sound(SND_PLACE);
    if (res->lines) {
        ui_sound_later(res->lines >= 2 || res->golds ? SND_END : SND_CAPTURE, PLACE_MS / 2);
    }
    keep_best();
    ended(fx.t0 + fx_len());
    ui_animate();
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
static void cell_xy(int x, int y, float *cx, float *cy)   /* a cell's middle */
{
    *cx = cx0 + (x + 0.5f) * q;
    *cy = cy0 + (y + 0.5f) * q;
}

static void slot_mid(int i, float *cx, float *cy)
{
    *cx = slot_box[i].x + slot_box[i].w / 2;
    *cy = slot_box[i].y + slot_box[i].h / 2;
}

/* a piece's top left when it is lifted under the finger at (x, y): above the finger, its middle over it */
static void lifted_at(int shape, float x, float y, float *lx, float *ly)
{
    const bk_shape *s = &bk_shapes[shape];
    *lx = x - s->w * q / 2;
    *ly = y - s->h * q - 1.7f * q;
    if (*ly < 0) *ly = 0;
}

/* the place a piece drawn with its top left at (lx, ly) lands: the nearest place it fits, near enough */
static int snap(int shape, float lx, float ly, int *ox, int *oy)
{
    float fx_ = (lx - cx0) / q, fy_ = (ly - cy0) / q;
    const bk_shape *s = &bk_shapes[shape];
    if (fx_ < -0.8f || fy_ < -0.8f || fx_ + s->w > BK_N + 0.8f || fy_ + s->h > BK_N + 0.8f) return 0;
    int bxx = 0, byy = 0, got = 0;
    float bd = 0.62f;                           /* half a cell and a little: the slack round a place */
    for (int y = (int)floorf(fy_) - 1; y <= (int)floorf(fy_) + 2; y++)
        for (int x = (int)floorf(fx_) - 1; x <= (int)floorf(fx_) + 2; x++) {
            if (!bk_fits(G.p.k.fill, shape, x, y)) continue;
            float d = hypotf(x - fx_, y - fy_);
            if (d < bd) bd = d, bxx = x, byy = y, got = 1;
        }
    if (got) *ox = bxx, *oy = byy;
    return got;
}

/* the shadow to show now: the dragged piece's, or the one placed by a tap or a hint */
static int shadow(int *shape, int *x, int *y)
{
    if (drag.on && drag.moving) {
        int sh = G.p.k.hand[drag.slot];
        float lx, ly;
        lifted_at(sh, drag.x, drag.y, &lx, &ly);
        if (!snap(sh, lx, ly, x, y)) return 0;
        *shape = sh;
        return 1;
    }
    if (ghost.on && sel >= 0 && G.p.k.hand[sel] != BK_NONE) {
        *shape = G.p.k.hand[sel];
        *x = ghost.x;
        *y = ghost.y;
        return 1;
    }
    return 0;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void block_at(int hue, float cx, float cy, float d, Uint8 light, Uint8 alpha)
{
    SDL_Texture *t = hue >= 1 && hue <= BK_GOLD ? t_block[hue] : NULL;
    if (!t) return;
    SDL_SetTextureColorMod(t, light, light, light);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a whole piece, its cells c wide, its top left at (x, y); its shadow below it, soft-cornered (the blocks' own
 * shape in black): lifted high (1), or resting on the felt (0) */
static void piece_at(int shape, float x, float y, float c, Uint8 light, Uint8 alpha, int lifted)
{
    const bk_shape *s = &bk_shapes[shape];
    float d = c * (1 - 0.06f), sx = lifted ? 0.16f * c : 0.05f * c, sy = lifted ? 0.24f * c : 0.08f * c;
    SDL_Texture *t = t_block[bk_colour(shape)];
    if (t) {
        SDL_SetTextureColorMod(t, 0, 0, 0);
        SDL_SetTextureAlphaMod(t, (Uint8)(alpha * (lifted ? 0.3f : 0.35f)));
        for (int r = 0; r < s->h; r++)
            for (int k = 0; k < s->w; k++)
                if (s->row[r] >> k & 1) ui_put_c(t, x + (k + 0.5f) * c + sx, y + (r + 0.5f) * c + sy, d, d);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, 255);
    }
    for (int r = 0; r < s->h; r++)
        for (int k = 0; k < s->w; k++)
            if (s->row[r] >> k & 1) block_at(bk_colour(shape), x + (k + 0.5f) * c, y + (r + 0.5f) * c, d, light, alpha);
}

static float block_d(void) { return q - fmaxf(1, roundf(0.06f * q)); }

static void draw_tray(Uint32 now)
{
    for (int i = 0; i < 3; i++) {
        int sh = G.p.k.hand[i];
        if (sh == BK_NONE) continue;
        if (drag.on && drag.slot == i) continue;
        const bk_shape *s = &bk_shapes[sh];
        float cx, cy;
        slot_mid(i, &cx, &cy);
        float c = tq, ox = 0, oy = 0;
        if (back.on && back.slot == i) {        /* flying back from where it was let go */
            float f = clampf((now - back.t0) / (float)BACK_MS, 0, 1), e = puz_ease(f);
            if (f >= 1) back.on = 0;
            else ui_animate();
            c = q + (tq - q) * e;
            ox = (back.x + s->w * q / 2 - cx) * (1 - e);
            oy = (back.y + s->h * q / 2 - cy) * (1 - e);
        } else if (deal.on) {                   /* coming in from the side */
            float f = clampf(((int)(now - deal.t0) - i * 60) / (float)DEAL_MS, 0, 1), e = puz_ease(f);
            if (f < 1) ui_animate();
            if (tall) ox = (1 - e) * (last_box.x + last_box.w + bs / 3 - cx + i * bs / 6);
            else oy = (1 - e) * (last_box.y + last_box.h + bs / 3 - cy + i * bs / 6);
            if (i == 2 && f >= 1) deal.on = 0;
        }
        int fits = bk_fits_anywhere(G.p.k.fill, sh), chosen = sel == i;
        if (chosen) {
            for (int r = 0; r < s->h; r++)
                for (int k = 0; k < s->w; k++)
                    if (s->row[r] >> k & 1)
                        ui_mark_gold_a(cx + ox + (k + 0.5f - s->w / 2.0f) * c * 1.12f,
                                       cy + oy + (r + 0.5f - s->h / 2.0f) * c * 1.12f, 1.9f * c, 0.6f);
            c *= 1.12f;
        }
        float x = cx + ox - s->w * c / 2, y = cy + oy - s->h * c / 2;
        piece_at(sh, x, y, c, fits ? 255 : 105, fits ? 255 : 170, chosen);
    }
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, bs, bs);
    ui_tool(0, ICON_HINT, bk_over(&G) ? UI_DIM : hint.on && (now - hint.t0) / 320 % 2 == 0 ? UI_ON : 0);
    float d = block_d();
    float et = endfx.on && now >= endfx.t0 ? (float)(now - endfx.t0) : -1;
    if (endfx.on && (now < endfx.t0 || et < END_MS)) ui_animate();
    float ft = fx.on ? (float)(now - fx.t0) : 0;
    if (fx.on && ft >= fx_len() + RISE_MS) fx.on = 0;
    if (fx.on) ui_animate();
    if (hint.on && now - hint.t0 > HINT_MS) hint.on = 0, ghost.on = 0, sel = -1;
    if (hint.on) ui_animate();

    /* the pattern's cells: engraved, in gold light once covered */
    for (int i = 0; i < BK_CELLS && G.goal == BK_PATTERN; i++) {
        int x = i % BK_N, y = i / BK_N;
        if (!(G.pat[y] >> x & 1)) continue;
        float cx, cy;
        cell_xy(x, y, &cx, &cy);
        int open = G.p.k.want[y] >> x & 1;
        if (open) {
            SDL_SetTextureColorMod(t_pat, 236, 190, 86);
            SDL_SetTextureAlphaMod(t_pat, 200);
        } else {
            ui_mark_gold_a(cx, cy, 1.2f * q, 0.55f);
            SDL_SetTextureColorMod(t_pat, 255, 236, 150);
            SDL_SetTextureAlphaMod(t_pat, 255);
        }
        ui_put_c(t_pat, cx, cy, d, d);
    }
    SDL_SetTextureColorMod(t_pat, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_pat, 255);

    /* the shadow and the lines it would clear */
    int ssh, sx, sy, sh_on = shadow(&ssh, &sx, &sy);
    uint16_t lit_rows = 0, lit_cols = 0;
    if (sh_on) {
        bk_core t = G.p.k;
        int slot = drag.on ? drag.slot : sel;
        bk_result r;
        if (bk_lay(&t, slot, sx, sy, &r)) lit_rows = r.rows, lit_cols = r.cols;
    }
    float pulse = 0.5f + 0.5f * sinf(now / 140.0f);
    if (lit_rows || lit_cols) ui_animate();

    /* the blocks */
    for (int i = 0; i < BK_CELLS; i++) {
        int x = i % BK_N, y = i / BK_N, c = G.p.col[i];
        if (!c) continue;
        float cx, cy;
        cell_xy(x, y, &cx, &cy);
        float s = 1;
        if (fx.on && fx.placed[i] && ft < PLACE_MS) s = 1.12f - 0.12f * puz_ease(ft / PLACE_MS);
        Uint8 light = 255;
        if (et >= 0 && !endfx.won) light = (Uint8)(255 - 120 * clampf((et - y * 50) / 300.0f, 0, 1));
        int lit = (lit_rows >> y & 1) || (lit_cols >> x & 1);
        if (lit) ui_mark_gold_a(cx, cy, 1.5f * q, 0.35f + 0.3f * pulse);
        block_at(c, cx, cy, d * s, light, 255);
        if (G.goal == BK_PATTERN && (G.pat[y] >> x & 1)) ui_mark_gold_a(cx, cy, 0.5f * q, 0.7f);
    }

    /* the cells clearing: each pops in a gold sparkle, running out from the piece */
    if (fx.on && fx.lines) {
        for (int i = 0; i < BK_CELLS; i++) {
            if (!fx.gone[i]) continue;
            float t = ft - PLACE_MS / 2 - fx.delay[i] * STEP_MS;
            if (t > POP_MS) continue;
            float cx, cy;
            cell_xy(i % BK_N, i / BK_N, &cx, &cy);
            if (t < 0) { block_at(fx.gone[i], cx, cy, d, 255, 255); continue; }
            float f = t / POP_MS, grow = f < 0.3f ? 1 + 0.5f * f : 1.15f * (1 - (f - 0.3f) / 0.7f);
            int gold = fx.gone[i] == BK_GOLD;
            ui_mark_gold_a(cx, cy, (gold ? 3.2f : 2.1f) * q * (0.4f + f), 1 - f);
            block_at(fx.gone[i], cx, cy, d * grow, (Uint8)(255 - 40 * f), (Uint8)(255 * (1 - f * f)));
            for (int k = 0; k < (gold ? 3 : 1); k++) {   /* sparkles flying off */
                float a = art_hash(i, k) * 2 * ART_PI, r = (0.2f + 0.9f * f) * q * (gold ? 1.4f : 1);
                float sz = q * (gold ? 0.8f : 0.55f) * sinf(ART_PI * f);
                SDL_SetTextureAlphaMod(t_spark, (Uint8)(255 * (1 - f)));
                ui_put_rot(t_spark, cx + cosf(a) * r, cy + sinf(a) * r - 0.3f * q * f, sz, sz, 90 * f + 45 * k);
            }
        }
        SDL_SetTextureAlphaMod(t_spark, 255);
    }

    /* the shadow where the piece would land */
    if (sh_on) {
        const bk_shape *s = &bk_shapes[ssh];
        int blink = !hint.on || (now - hint.t0) / 300 % 2 == 0;
        for (int r = 0; r < s->h; r++)
            for (int k = 0; k < s->w; k++)
                if (s->row[r] >> k & 1) {
                    float cx, cy;
                    cell_xy(sx + k, sy + r, &cx, &cy);
                    if (hint.on && blink) ui_mark_gold_a(cx, cy, 1.4f * q, 0.7f);
                    block_at(bk_colour(ssh), cx, cy, d, 210, drag.on ? 95 : 140);
                }
    }

    /* the end: a gold wave over the tray (a level solved), or the blocks dimmed (above) */
    if (et >= 0 && endfx.won)
        for (int i = 0; i < BK_CELLS; i++) {
            float p = puz_pulse(et, (i % BK_N + i / BK_N) * 30.0f, 420);
            if (p <= 0) continue;
            float cx, cy;
            cell_xy(i % BK_N, i / BK_N, &cx, &cy);
            ui_mark_gold_a(cx, cy, 2.2f * q, p);
        }

    draw_tray(now);

    /* the points rising over the piece */
    if (fx.on && fx.gain > 0 && ft < fx_len() + RISE_MS) {
        float t = clampf(ft / (float)(fx_len() + RISE_MS), 0, 1), h = (fx.lines ? 0.95f : 0.62f) * q;
        if (fx.streak >= 2) h *= 1.2f;
        float cx = cx0 + fx.px * q, cy = cy0 + fx.py * q - 0.5f * q - 1.4f * q * puz_ease(t);
        cx = clampf(cx, bx + 1.5f * h, bx + bs - 1.5f * h);
        Uint8 a = (Uint8)(255 * clampf(1.6f - 1.6f * t, 0, 1));
        static const uint8_t gold[3] = {255, 216, 110}, cream[3] = {250, 244, 230};
        const uint8_t *ink = fx.lines ? gold : cream;
        float w = ui_number_w(fx.gain, h), px = cx - (w + 0.55f * h) / 2;
        if (fx.lines) ui_mark_gold_a(cx, cy, 2.2f * h * (1 + fx.streak * 0.2f), 0.45f * a / 255.0f);
        ui_fill(px, cy - 0.05f * h, 0.42f * h, 0.1f * h, ink[0], ink[1], ink[2], a);   /* a plus */
        ui_fill(px + 0.16f * h, cy - 0.21f * h, 0.1f * h, 0.42f * h, ink[0], ink[1], ink[2], a);
        ui_number_ex(px + 0.55f * h + w / 2 + 0.03f * h, cy + 0.04f * h, h, fx.gain, PUZ_INK, (Uint8)(a * 0.6f), 0);
        ui_number_ex(px + 0.55f * h + w / 2, cy, h, fx.gain, ink, a, 0);
    }

    /* the piece under the finger, lifted */
    if (drag.on) {
        int sh = G.p.k.hand[drag.slot];
        const bk_shape *s = &bk_shapes[sh];
        float lx, ly, cx, cy;
        lifted_at(sh, drag.x, drag.y, &lx, &ly);
        slot_mid(drag.slot, &cx, &cy);
        float f = drag.moving ? clampf((now - drag.t0) / (float)LIFT_MS, 0, 1) : 0, e = puz_ease(f);
        if (f < 1) ui_animate();
        float c = tq * 1.12f + (q - tq * 1.12f) * e;
        float x0 = cx - s->w * tq * 1.12f / 2, y0 = cy - s->h * tq * 1.12f / 2;
        piece_at(sh, x0 + (lx - x0) * e, y0 + (ly - y0) * e, c, 255, 255, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int slot_at(float x, float y)
{
    for (int i = 0; i < 3; i++) {
        box b = slot_box[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h && G.p.k.hand[i] != BK_NONE) return i;
    }
    return -1;
}

static int board_cell(float x, float y, int *cx, int *cy)
{
    float fx_ = (x - cx0) / q, fy_ = (y - cy0) / q;
    if (fx_ < 0 || fy_ < 0 || fx_ >= BK_N || fy_ >= BK_N) return 0;
    *cx = (int)fx_;
    *cy = (int)fy_;
    return 1;
}

static void use_hint(void);

static void press(float x, float y)
{
    if (ui_tool_hit(x, y) == 0) {
        if (!bk_over(&G)) use_hint();
        return;
    }
    if (fx.on && ui_ms() - fx.t0 < fx_len()) fx.on = 0;   /* the last placement lands at once */
    back.on = 0;
    if (bk_over(&G)) return;
    int s = slot_at(x, y);
    if (s >= 0) {
        drag.on = 1;
        drag.moving = 0;
        drag.slot = s;
        drag.x = drag.x0 = x;
        drag.y = drag.y0 = y;
        drag.t0 = ui_ms();
        ui_animate();
        return;
    }
    drag.on = 0;
    drag.slot = -1;
    drag.x0 = x;
    drag.y0 = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (!drag.moving && hypotf(x - drag.x0, y - drag.y0) > 0.3f * q) {
        drag.moving = 1;
        drag.t0 = ui_ms();
        sel = -1;
        ghost.on = hint.on = 0;
        ui_sound(SND_TAP);
    }
    ui_animate();
}

static void tap_board(float x, float y)
{
    int cx, cy;
    if (!board_cell(x, y, &cx, &cy)) {
        if (sel >= 0) sel = -1, ghost.on = 0, ui_animate();
        return;
    }
    if (sel < 0) { ui_sound(SND_DENY); return; }
    int sh = G.p.k.hand[sel];
    const bk_shape *s = &bk_shapes[sh];
    if (ghost.on && cx >= ghost.x && cy >= ghost.y && cx < ghost.x + s->w && cy < ghost.y + s->h &&
        (s->row[cy - ghost.y] >> (cx - ghost.x) & 1)) {
        lay(sel, ghost.x, ghost.y);             /* the shadow tapped: laid there */
        return;
    }
    int gx, gy;                                 /* the shadow centred on the cell tapped */
    if (snap(sh, cx0 + (cx + 0.5f - s->w / 2.0f) * q, cy0 + (cy + 0.5f - s->h / 2.0f) * q, &gx, &gy)) {
        ghost.on = 1;
        ghost.x = gx;
        ghost.y = gy;
        hint.on = 0;
        ui_sound(SND_TAP);
    } else ui_sound(SND_DENY);
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) {
        if (hypotf(x - drag.x0, y - drag.y0) < 0.5f * q && !bk_over(&G)) tap_board(x, y);
        return;
    }
    drag.x = x;
    drag.y = y;
    int s = drag.slot;
    if (!drag.moving) {                         /* a tap on a piece: chosen, or no longer */
        drag.on = 0;
        hint.on = 0;
        if (sel == s) sel = -1, ghost.on = 0;
        else {
            sel = s;
            ghost.on = 0;
            if (!bk_fits_anywhere(G.p.k.fill, G.p.k.hand[s])) ui_sound(SND_DENY);
            else ui_sound(SND_TAP);
        }
        ui_animate();
        return;
    }
    int sh = G.p.k.hand[s], gx, gy;
    float lx, ly;
    lifted_at(sh, x, y, &lx, &ly);
    drag.on = 0;
    if (snap(sh, lx, ly, &gx, &gy)) {
        lay(s, gx, gy);
        return;
    }
    back.on = 1;                                /* not on the grid: back to its place */
    back.slot = s;
    back.x = lx;
    back.y = ly;
    back.t0 = ui_ms();
    int cx, cy;
    if (board_cell(lx + q / 2, ly + q / 2, &cx, &cy)) ui_sound(SND_DENY);
    ui_animate();
}

static void use_hint(void)
{
    int s, x, y;
    fx.on = 0;
    if (!bk_hint(&G, &s, &x, &y)) { ui_sound(SND_DENY); return; }
    sel = s;
    ghost.on = 1;
    ghost.x = x;
    ghost.y = y;
    hint.on = 1;
    hint.slot = s;
    hint.t0 = ui_ms();
    ui_sound(SND_TAP);
    ui_animate();
}

/* ---- the bar: classic's score and best; a level's goal left and pieces left ----------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    if (G.goal == BK_CLASSIC) {
        int top = best_score > G.p.k.score ? best_score : G.p.k.score;
        ui_bar_fit(b, (ui_count[]){{t_tok, G.p.k.score}, {t_star, top}}, 2, 0);
        return;
    }
    ui_bar_fit(b, (ui_count[]){{t_goal[G.goal], bk_goal_left(&G.p.k, G.goal, G.target)}, {t_left, bk_left(&G)}}, 2, 0);
}

/* ---- the pictures of the shell's screens --------------------------------------------------------------------- */
/* a grid (as the levels' data) on its board, size x size */
static void paint_grid(const char *grid, int size, uint8_t *rgba)
{
    float span = BK_N + 2 * (FRAME + PAD), c = size / span;
    int fr = (int)(FRAME * c), o = fr + (int)(PAD * c);
    float cq = (size - 2.0f * o) / BK_N;
    paint_board(size, fr, (float)o, cq, rgba);
    int d = (int)cq - (cq >= 10 ? 1 : 0);
    if (d < 2) return;
    uint8_t *t = malloc((size_t)d * d * 4);
    if (!t) return;
    int painted = -1;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < BK_CELLS; i++) {
            char ch = grid[i];
            int hue = ch == 'G' ? BK_GOLD : ch >= '1' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'c' ? ch - 'a' + 10 : 0;
            int px = (int)(o + (i % BK_N) * cq + (cq - d) / 2), py = (int)(o + (i / BK_N) * cq + (cq - d) / 2);
            if (pass == 0 && ch == 'o') {
                paint_pat(d, t);
                for (int k = 0; k < d * d; k++) t[4 * k] = 236, t[4 * k + 1] = 190, t[4 * k + 2] = 86;
                art_blend(rgba, size, size, t, d, d, px, py);
                painted = -1;
            }
            if (pass == 1 && hue) {
                if (painted != hue) paint_block(hue, d, t), painted = hue;
                art_blend(rgba, size, size, t, d, d, px, py);
            }
        }
    free(t);
}

/* the menu's card: a game going on, a row about to clear */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static const char g2[BK_CELLS + 1] = "....44...."
                                         ".........."
                                         "..1......."
                                         "..1....G.."
                                         ".881..7G7."
                                         "3333.33333"
                                         "..2...99.."
                                         ".22...9..."
                                         "...a.ccc.."
                                         "..aa.c....";
    paint_grid(g2, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    if (level >= 0 && level < BK_LEVELS) paint_grid(bk_levels[level].grid, size, rgba);
}

/* a chapter's tab: a piece, bigger chapter by chapter */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SH[BK_CHAPTERS] = {1, 9, 27, 15, 36, 14};
    int sh = SH[chapter < 0 ? 0 : chapter % BK_CHAPTERS];
    const bk_shape *s = &bk_shapes[sh];
    int n = s->w > s->h ? s->w : s->h;
    put_piece(rgba, size, sh, 0.86f * size / (n < 2 ? 2 : n), size / 2.0f, size / 2.0f);
}

static const game blocks_game = {
    .id = "blocks", .order = 425, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .score = score, .par = par, .level_stars = level_stars,
    .levels = BK_LEVELS, .chapters = BK_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb,
};
REGISTER_GAME(blocks_game)
