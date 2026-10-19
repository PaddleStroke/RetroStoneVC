/*
 * Number Puzzles in RetroStone Board, for one player: maple tiles on the house plank, their numbers burned in, the
 * target on a lacquer plaque above them, the four operations as round buttons under them (beside them in a wide
 * room); the hint (and Target's "done") on the shell's tool rail (the rules: mathpz_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile, an operation, another tile: the first slides onto the second and they become one tile holding the
 * result (in honey wood: a tile made), chosen for the next step. A drag from a tile to another does the same with the
 * operation chosen. A refused step (a division by zero; in Target a result below one or not whole): SND_DENY. Undo
 * splits the last tile made back into its two. Four numbers and All five: the last tile glows red when it is not the
 * target (undo). Target: the tick is lit while the nearest tile is within NPZ_NEAR of the target. The lamp: one step of
 * a way there, played (or the last step undone when no way is left). Solved: the gold light swells on the tile and on
 * the plaque. The bar: the steps made, and the hints used.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mathpz.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define MERGE_MS 230
#define POP_MS 220
#define WIN_MS 950
#define HINT_MS 1100
/* the layout, in tiles: the gap between tiles, the plaque's height, the operations' buttons, the plank's margin */
#define GAP 0.16f
#define PLQ 0.72f
#define OPD 0.62f
#define OPG 0.2f
#define PAD 0.26f

enum { B_HINT, B_DONE, BTNS };

static const uint8_t GOLD[3] = {240, 202, 112};

static npz_game G;
static int sel = -1, op = -1, down = -1;
static struct { int on, a, b, split; npz_q va, vb; uint8_t ma, mb; Uint32 t0; } anim;
static struct { int slot; Uint32 t0; } pop = {-1, 0}, hint = {-1, 0};
static int hint_op = -1;
static struct { int on, slot; Uint32 t0; } win;

static box last_box;
static float T, px, py, pw, ph, ox, oy;         /* a tile's side; the plank; the plaque's centre */
static float tx[NPZ_MAX], ty[NPZ_MAX], opx[NPZ_OPS], opy[NPZ_OPS];   /* the slots' and the buttons' centres */
static int made_key = -1, made_small = -1;
static SDL_Texture *t_plank, *t_tile[2], *t_plaque, *t_small, *t_lamp;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tile, s x s: a cut of the house plank, pale maple (given) or honey (made), bevelled */
static void paint_tile(int s, int made, uint8_t *o)
{
    int P = 2 * s;
    uint8_t *src = malloc((size_t)P * P * 4);
    if (!src) return;
    art_board_n(1, 1, P, 0, 0, src);
    int sx = made ? s / 3 : s / 7, sy = made ? s / 2 : s / 5;
    float m[3] = {1.18f, 1.14f, 1.08f};
    if (made) m[0] = 1.05f, m[1] = 0.9f, m[2] = 0.66f;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            const uint8_t *p = src + 4 * ((size_t)(sy + y) * P + sx + x);
            uint8_t *d = o + 4 * ((size_t)y * s + x);
            for (int i = 0; i < 3; i++) d[i] = (uint8_t)clampf(p[i] * m[i], 0, 255);
            d[3] = 255;
        }
    free(src);
    puz_bevel(o, s, s, 0.1f * s, fmaxf(2, 0.07f * s), 0.36f);
}

/* the plaque, w x h: the plank darkened to deep lacquer, bevelled */
static void paint_plaque(int w, int h, uint8_t *o)
{
    int S = w > h ? w : h;
    uint8_t *src = malloc((size_t)S * S * 4);
    if (!src) return;
    art_board_n(1, 1, S, 0, 0, src);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *p = src + 4 * ((size_t)y * S + x);
            uint8_t *d = o + 4 * ((size_t)y * w + x);
            d[0] = (uint8_t)(p[0] * 0.42f), d[1] = (uint8_t)(p[1] * 0.24f), d[2] = (uint8_t)(p[2] * 0.19f), d[3] = 255;
        }
    free(src);
    puz_bevel(o, w, h, 0.2f * h, fmaxf(2, 0.09f * h), 0.45f);
}

/* an operation's sign, white on clear (size x size; the button tints it): + - x / */
static void paint_op(int k, int size, uint8_t *o)
{
    float s = (float)size, c = s / 2, r = 0.07f * s, l = 0.3f * s;
    static const uint8_t W[3] = {255, 255, 255};
    if (k == NPZ_ADD || k == NPZ_SUB) pzk_stroke(o, size, size, c - l, c, c + l, c, r, W, 1);
    if (k == NPZ_ADD) pzk_stroke(o, size, size, c, c - l, c, c + l, r, W, 1);
    if (k == NPZ_MUL) {
        float d = 0.75f * l;
        pzk_stroke(o, size, size, c - d, c - d, c + d, c + d, r, W, 1);
        pzk_stroke(o, size, size, c - d, c + d, c + d, c - d, r, W, 1);
    }
    if (k == NPZ_DIV) {
        pzk_stroke(o, size, size, c - l, c, c + l, c, r, W, 1);
        pzk_stroke(o, size, size, c, c - 0.6f * l, c, c - 0.6f * l, 1.25f * r, W, 1);
        pzk_stroke(o, size, size, c, c + 0.6f * l, c, c + 0.6f * l, 1.25f * r, W, 1);
    }
}

/* a tile with its number burned in, into a picture (w x h) at (x, y), s across */
static void tile_into(uint8_t *o, int w, int h, int x, int y, int s, int value, int made)
{
    uint8_t *t = calloc((size_t)s * s, 4);
    if (!t) return;
    paint_tile(s, made, t);
    if (value > 0) {
        int n = value >= 100 ? 3 : value >= 10 ? 2 : 1;
        puz_burn(t, s, s, value, s / 2.0f, s * 0.53f, s * (n == 3 ? 0.36f : n == 2 ? 0.46f : 0.54f));
    }
    art_blend(o, w, h, t, s, s, x, y);
    free(t);
}

/* the kind's tiles laid out in a picture (size x size): its plaque with the target and its tiles; digits 0: none
 * (a small tab) */
static void paint_scene(int kind, const int *nums, int target, int size, uint8_t *rgba, int digits)
{
    int n = npz_tiles(kind), cols = n == 4 ? 2 : 3, rows = (n + cols - 1) / cols;
    int f = (int)fmaxf(1, size * 0.04f), inner = size - 2 * f;
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    art_plank(inner, inner, f, b);
    float u = inner / (cols + (cols - 1) * GAP + 2 * 0.3f);
    float hu = inner / (PLQ + GAP + rows + (rows - 1) * GAP + 2 * 0.3f);
    if (hu < u) u = hu;
    int s = (int)u, plh = (int)(PLQ * u), plw = (int)fminf(inner * 0.8f, 2.4f * u);
    float top = (size - (PLQ + GAP + rows + (rows - 1) * GAP) * u) / 2;
    uint8_t *pl = calloc((size_t)plw * plh, 4);
    if (pl) {
        paint_plaque(plw, plh, pl);
        if (digits) {
            int dg = target >= 100 ? 3 : 2;
            float dh = fminf(0.62f * plh, plw / (0.66f * dg + 0.3f));
            puz_digits(pl, plw, plh, target, plw / 2.0f, plh * 0.53f, dh, GOLD, 1, 1);
        }
        art_blend(b, size, size, pl, plw, plh, (size - plw) / 2, (int)top);
        free(pl);
    }
    for (int i = 0; i < n; i++) {
        int r = i / cols, inrow = r == rows - 1 ? n - r * cols : cols, c = i % cols;
        float rw = inrow * u + (inrow - 1) * GAP * u;
        int x = (int)((size - rw) / 2 + c * (1 + GAP) * u), y = (int)(top + (PLQ + GAP + r * (1 + GAP)) * u);
        tile_into(b, size, size, x, y, s, digits ? nums[i] : 0, 0);
    }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

static const int SHOW_NUMS[NPZ_GAMES][NPZ_MAX] = {{3, 8, 3, 8}, {25, 50, 3, 6, 7, 9}, {3, 7, 11, 14, 20}};
static const int SHOW_TARGET[NPZ_GAMES] = {24, 732, 12};

static void kind_pic(int kind, int size, uint8_t *rgba)
{
    int k = kind < 0 || kind >= NPZ_GAMES ? 0 : kind;
    paint_scene(k, SHOW_NUMS[k], SHOW_TARGET[k], size, rgba, 1);
}

static void thumb(int variant, int size, uint8_t *rgba) { kind_pic(variant < 0 ? 0 : variant / NPZ_TIERS, size, rgba); }

static void token(int player, int size, uint8_t *rgba)   /* a tile of 24 */
{
    (void)player;
    int s = (int)(0.86f * size);
    tile_into(rgba, size, size, (size - s) / 2, (size - s) / 2, s, 24, 0);
}

/* a chapter's tab: its game's tiles (no digits), its tier as one to three gold studs under them */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int kind = chapter / NPZ_TIERS, tier = chapter % NPZ_TIERS;
    paint_scene(kind, SHOW_NUMS[kind < NPZ_GAMES ? kind : 0], 0, size, rgba, 0);
    float d = size * 0.12f, y = size * 0.9f;
    for (int i = 0; i <= tier; i++) {
        float x = size / 2.0f + (i - tier / 2.0f) * 1.4f * d;
        pzk_stroke(rgba, size, size, x, y, x, y, d / 2, PZK_DARK, 0.9f);
        pzk_stroke(rgba, size, size, x, y, x, y, d / 2 - 1.5f, PZK_GOLD, 1);
    }
}

/* the bar's pictures: a small honey tile (the steps), a lamp (the hints) */
static void small_pics(int s)
{
    ui_free(&t_small);
    ui_free(&t_lamp);
    uint8_t *o = ui_buf(s, s);
    memset(o, 0, (size_t)s * s * 4);
    int k = (int)(0.86f * s);
    uint8_t *t = calloc((size_t)k * k, 4);
    if (t) {
        paint_tile(k, 1, t);
        art_blend(o, s, s, t, k, k, (s - k) / 2, (s - k) / 2);
        free(t);
    }
    t_small = ui_tex(s, s, o);
    memset(o, 0, (size_t)s * s * 4);
    art_icon(ICON_HINT, s, o);
    for (int i = 0; i < s * s; i++)             /* the shell's icons are white: cream, as its own */
        o[4 * i] = 246, o[4 * i + 1] = 230, o[4 * i + 2] = 190;
    t_lamp = ui_tex(s, s, o);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
/* tiles in `cols` columns, the buttons in a row under them (side 0) or a column at their right (side 1): the plank's
 * size in tiles */
static void units(int cols, int side, float *uw, float *uh)
{
    int rows = (G.n + cols - 1) / cols;
    float tw = cols + (cols - 1) * GAP, th = PLQ + GAP + rows + (rows - 1) * GAP;
    float rw = NPZ_OPS * OPD + (NPZ_OPS - 1) * OPG;
    if (side) {
        *uw = tw + 2 * GAP + OPD + 2 * PAD;
        *uh = fmaxf(th, rw) + 2 * PAD;
    } else {
        *uw = fmaxf(tw, rw) + 2 * PAD;
        *uh = th + 2 * GAP + OPD + 2 * PAD;
    }
}

static void layout(box b)
{
    last_box = b;
    ui_tools_shown(G.kind == NPZ_TARGET ? 2 : 1);
    int split = G.n == 4 ? 2 : 3, best_cols = split, best_side = 0;
    float best = 0;
    for (int k = 0; k < 4; k++) {
        int cols = k & 1 ? G.n : split, side = k >> 1;
        float uw, uh;
        units(cols, side, &uw, &uh);
        float t = fminf(b.w * 0.98f / uw, b.h * 0.98f / uh);
        if (t > best * 1.03f) best = t, best_cols = cols, best_side = side;
    }
    float cap = 0.3f * fminf((float)ui.w, (float)ui.h);
    T = floorf(fminf(best, cap));
    float uw, uh;
    units(best_cols, best_side, &uw, &uh);
    pw = floorf(uw * T), ph = floorf(uh * T);
    px = floorf(b.x + (b.w - pw) / 2), py = floorf(b.y + (b.h - ph) / 2);
    ui_board_used(px, py, pw, ph);
    int rows = (G.n + best_cols - 1) / best_cols;
    float tw = best_cols + (best_cols - 1) * GAP, th = PLQ + GAP + rows + (rows - 1) * GAP;
    float rw = NPZ_OPS * OPD + (NPZ_OPS - 1) * OPG;
    float bx = px + PAD * T, by = py + PAD * T;   /* the tiles' block: its top left */
    if (best_side) by += (fmaxf(th, rw) - th) / 2 * T;
    else bx += (fmaxf(tw, rw) - tw) / 2 * T;
    ox = bx + tw * T / 2;
    oy = by + PLQ * T / 2;
    for (int i = 0; i < G.n; i++) {
        int r = i / best_cols, c = i % best_cols, inrow = r == rows - 1 ? G.n - r * best_cols : best_cols;
        float rx = bx + (tw - (inrow + (inrow - 1) * GAP)) / 2 * T;
        tx[i] = rx + (c * (1 + GAP) + 0.5f) * T;
        ty[i] = by + (PLQ + GAP + r * (1 + GAP) + 0.5f) * T;
    }
    for (int k = 0; k < NPZ_OPS; k++) {
        if (best_side) {
            opx[k] = bx + (tw + 2 * GAP + OPD / 2) * T;
            opy[k] = py + ph / 2 + ((k - 1.5f) * (OPD + OPG)) * T;
        } else {
            opx[k] = px + pw / 2 + ((k - 1.5f) * (OPD + OPG)) * T;
            opy[k] = by + (th + 2 * GAP + OPD / 2) * T;
        }
    }
    int key = ((int)T * 64 + (int)pw) * 8 + G.n + 7 * best_side;
    if (made_key != key && T >= 4) {
        made_key = key;
        int f = (int)fmaxf(2, 0.09f * T), w = (int)pw, h = (int)ph;
        uint8_t *o = calloc((size_t)w * h, 4);
        if (o) {
            art_plank(w - 2 * f, h - 2 * f, f, o);
            for (int y = f; y < h - f; y++)     /* the floor darkened: the tiles stand out */
                for (int x = f; x < w - f; x++)
                    for (int c = 0; c < 3; c++) o[4 * (y * w + x) + c] = (uint8_t)(o[4 * (y * w + x) + c] * 0.66f);
            static const float tone[3] = {0.8f, 0.77f, 0.74f};
            for (int i = 0; i < G.n; i++)       /* each slot sunk a little into the plank */
                art_inlay(o, w, h, tx[i] - px - 0.53f * T, ty[i] - py - 0.53f * T, 1.06f * T, 1.06f * T, 0.12f * T,
                          tone, fmaxf(1, 0.025f * T));
            ui_free(&t_plank);
            t_plank = ui_tex(w, h, o);
            free(o);
        }
        for (int m = 0; m < 2; m++) {
            int s = (int)T;
            ui_free(&t_tile[m]);
            uint8_t *t = ui_buf(s, s);
            paint_tile(s, m, t);
            t_tile[m] = ui_tex(s, s, t);
        }
        int qw = (int)(2.4f * T), qh = (int)(PLQ * T);
        ui_free(&t_plaque);
        uint8_t *q = calloc((size_t)qw * qh, 4);
        if (q) {
            paint_plaque(qw, qh, q);
            t_plaque = ui_tex(qw, qh, q);
            free(q);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 2) {
        made_small = small;
        small_pics(small);
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = op = down = hint_op = -1;
    memset(&anim, 0, sizeof anim);
    memset(&win, 0, sizeof win);
    pop.slot = hint.slot = -1;
}

static void begin(void)
{
    calm();
    made_key = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!npz_level(&G, level)) npz_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    npz_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return npz_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return npz_stars(&G); }

static int score(void)                          /* the end card's number: the target made, or the nearest */
{
    int i = npz_nearest(&G);
    return G.kind == NPZ_TARGET && i >= 0 ? G.v[i].n : G.target;
}

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < MERGE_MS) || (win.on && now - win.t0 < WIN_MS + MERGE_MS);
}

static void won(int slot)
{
    win.on = 1;
    win.slot = slot;
    win.t0 = ui_ms();
    sel = op = -1;
}

/* the merge just made (a onto b), seen: the slide, then the pop */
static void slide(const npz_game *g, int split)
{
    const npz_past *h = &g->hist[split ? g->nh : g->nh - 1];
    anim.on = 1;
    anim.split = split;
    anim.a = h->s.a;
    anim.b = h->s.b;
    anim.va = h->va;
    anim.vb = h->vb;
    anim.mb = h->mb;
    anim.ma = g->made[h->s.a];
    anim.t0 = ui_ms();
    ui_animate();
}

static void merged(void)
{
    slide(&G, 0);
    pop.slot = anim.b;
    pop.t0 = anim.t0 + MERGE_MS;
    ui_sound_later(SND_PLACE, MERGE_MS);
    sel = npz_left(&G) > 1 ? anim.b : -1;
    op = -1;
    if (npz_solved(&G)) {
        won(G.kind == NPZ_TARGET ? npz_nearest(&G) : anim.b);
        win.t0 = anim.t0;
    }
}

static int try_merge(int a, int o, int b)
{
    if (!npz_merge(&G, a, o, b)) {
        ui_sound(SND_DENY);
        return 0;
    }
    merged();
    return 1;
}

static int undo(void)
{
    if (G.done) {
        npz_undo(&G);
        win.on = 0;
        return 1;
    }
    if (!G.nh) return 0;
    npz_undo(&G);
    slide(&G, 1);
    sel = op = hint_op = -1;
    win.on = 0;
    pop.slot = -1;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = npz_script(&G, moves);
    if (npz_solved(&G)) {
        int s = G.kind == NPZ_TARGET ? npz_nearest(&G) : -1;
        for (int i = 0; i < G.n && s < 0; i++)
            if (G.live[i]) s = i;
        won(s);
    }
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a whole number, below zero too, centred, h high, no wider than maxw (made smaller) */
static void draw_int(float cx, float cy, float h, float maxw, int64_t v, const uint8_t rgb[3], Uint8 a)
{
    int neg = v < 0, av = (int)(neg ? -v : v);
    float mw = neg ? 0.55f * h : 0, w = ui_number_w(av, h) + mw;
    if (w > maxw) {
        h *= maxw / w;
        mw *= maxw / w;
        w = maxw;
    }
    float x = cx - w / 2;
    if (neg) {
        float t = fmaxf(1, floorf(0.12f * h + 0.5f));
        ui_fill(floorf(x + 0.05f * h), floorf(cy - t / 2), floorf(0.4f * h), t, rgb[0], rgb[1], rgb[2], a);
    }
    ui_number_ex(x + mw + (w - mw) / 2, cy, h, av, rgb, a, UI_NUM_TIGHT);
}

/* a tile's value, burned in: a whole number, or a fraction (the numerator over the denominator, a bar between) */
static void draw_value(float cx, float cy, float s, npz_q v, const uint8_t rgb[3], Uint8 a)
{
    const uint8_t *shine = PUZ_CREAM;
    float off = fmaxf(1, s / 60);
    if (v.d == 1) {
        float h = 0.5f * s;
        draw_int(cx + 0.6f * off, cy + off, h, 0.84f * s, v.n, shine, (Uint8)(a / 2));
        draw_int(cx, cy, h, 0.84f * s, v.n, rgb, a);
        return;
    }
    float h = 0.3f * s, bw = 0.62f * s, t = fmaxf(2, floorf(0.045f * s));
    int neg = v.n < 0;
    float sx = neg ? 0.12f * s : 0;
    if (neg) ui_fill(floorf(cx - bw / 2 - 0.2f * s + 0.5f * sx), floorf(cy - t / 2), floorf(0.18f * s), t, rgb[0],
                     rgb[1], rgb[2], a);
    draw_int(cx + sx, cy - 0.22f * s, h, 0.62f * s, neg ? -(int64_t)v.n : v.n, rgb, a);
    ui_fill(floorf(cx + sx - bw / 2 + 0.5f), floorf(cy - t / 2), floorf(bw), t, rgb[0], rgb[1], rgb[2], a);
    draw_int(cx + sx, cy + 0.24f * s, h, 0.62f * s, v.d, rgb, a);
}

static void draw_tile(float cx, float cy, float s, npz_q v, int made, float lift, int chosen)
{
    ui_shadow_flat(cx + 0.04f * s, cy + 0.06f * s + lift * 0.5f, 1.08f * s, 1.08f * s, 0.45f + 0.3f * (lift > 0));
    if (chosen) ui_mark_gold(cx, cy - lift, 1.7f * s);
    ui_put_c(t_tile[made ? 1 : 0], cx, cy - lift, s, s);
    draw_value(cx, cy - lift, s, v, PUZ_INK, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_plank, px, py, pw, ph);
    /* the plaque: the target in gold */
    float qw = 2.4f * T, qh = PLQ * T;
    ui_shadow_flat(ox + 0.03f * T, oy + 0.05f * T, qw * 1.04f, qh * 1.1f, 0.45f);
    ui_put_c(t_plaque, ox, oy, qw, qh);
    float wt = win.on && now >= win.t0 + MERGE_MS ? (float)(now - win.t0 - MERGE_MS) : -1;
    if (wt >= 0) {
        float p = puz_pulse(wt, 0, WIN_MS);
        if (p > 0) ui_mark_gold_a(ox, oy, 2.6f * T, p);
        if (wt < WIN_MS) ui_animate();
    } else if (win.on) {
        ui_animate();
    }
    draw_int(ox, oy, 0.5f * qh * 1.1f, 0.85f * qw, G.target, GOLD, 255);

    /* the tiles */
    float at = anim.on ? (now - anim.t0) / (float)MERGE_MS : 1;
    if (anim.on && at >= 1) anim.on = 0;
    if (anim.on) ui_animate();
    for (int i = 0; i < G.n; i++) {
        if (!G.live[i] || (anim.on && i == anim.a)) continue;   /* gone, or on its way (drawn below) */
        npz_q v = G.v[i];
        int made = G.made[i];
        if (anim.on && !anim.split && i == anim.b) v = anim.vb, made = anim.mb;   /* not landed yet */
        float s = T;
        if (pop.slot == i && now >= pop.t0) {
            float p = puz_pulse((float)(now - pop.t0), 0, POP_MS);
            if (p > 0) {
                s *= 1 + 0.1f * p;
                ui_mark_gold_a(tx[i], ty[i], 1.9f * T, p);
                ui_animate();
            } else {
                pop.slot = -1;
            }
        } else if (pop.slot == i) {
            ui_animate();
        }
        if (win.on && i == win.slot && wt >= 0) {
            float p = puz_pulse(wt, 0, WIN_MS);
            if (p > 0) ui_mark_gold_a(tx[i], ty[i], 2.6f * T, p);
            ui_mark_gold_a(tx[i], ty[i], 1.6f * T, 0.6f);
        }
        if (npz_stuck(&G) && !anim.on) ui_mark_glow(tx[i], ty[i], 1.7f * T);
        if (hint.slot == i) {
            float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
            if (p > 0) {
                ui_mark_gold_a(tx[i], ty[i], 2.2f * T, p);
                ui_animate();
            } else {
                hint.slot = -1;
            }
        }
        draw_tile(tx[i], ty[i], s, v, made, i == sel ? 0.07f * T : 0, i == sel);
    }
    if (anim.on) {                              /* the tile on its way: onto b, or back from b to its slot */
        float e = puz_ease(at);
        int from = anim.split ? anim.b : anim.a, to = anim.split ? anim.a : anim.b;
        float x = tx[from] + (tx[to] - tx[from]) * e, y = ty[from] + (ty[to] - ty[from]) * e;
        float lift = 0.1f * T * sinf(PUZ_PI * e);
        draw_tile(x, y, T * (anim.split ? 1 : 1 - 0.12f * e), anim.va, anim.ma, lift, 0);
    }

    /* the operations */
    int playing = !npz_solved(&G);
    for (int k = 0; k < NPZ_OPS; k++) {
        int how = UI_TINT;
        if (k == op) how |= UI_ON;
        else if (sel >= 0 && op < 0 && playing) how |= UI_LIT;
        else if (!playing) how |= UI_DIM;
        if (k == hint_op) {
            float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
            if (p > 0) {
                ui_mark_gold_a(opx[k], opy[k], 1.8f * OPD * T, p);
                ui_animate();
            } else {
                hint_op = -1;
            }
        }
        ui_button_paint(opx[k], opy[k], OPD * T, paint_op, k, 0.7f, how);
    }
    ui_tool(B_HINT, ICON_HINT, playing ? 0 : UI_DIM);
    if (G.kind == NPZ_TARGET) ui_tool(B_DONE, ICON_TICK, npz_can_finish(&G) ? UI_LIT : UI_DIM);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int tile_at(float x, float y)
{
    for (int i = 0; i < G.n; i++)
        if (G.live[i] && fabsf(x - tx[i]) <= 0.56f * T && fabsf(y - ty[i]) <= 0.56f * T) return i;
    return -1;
}

static int op_at(float x, float y)
{
    for (int k = 0; k < NPZ_OPS; k++)
        if (ui_button_hit(opx[k], opy[k], OPD * T, x, y)) return k;
    return -1;
}

static void tap_tile(int k)
{
    if (sel < 0 || (sel != k && op < 0)) {
        sel = k;
        ui_sound(SND_TAP);
    } else if (sel == k) {
        sel = op = -1;
    } else {
        try_merge(sel, op, k);
    }
    ui_animate();
}

static void press(float x, float y)
{
    down = -1;
    if (npz_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t == B_HINT) {
        npz_step s;
        int how = npz_hint(&G, &s);
        if (!how) { ui_sound(SND_DENY); return; }
        if (how == 1) {
            merged();
            hint.slot = s.b;
            hint_op = s.op;
            hint.t0 = ui_ms();
        } else {
            slide(&G, 1);
            sel = op = -1;
            hint.slot = anim.a;
            hint_op = -1;
            hint.t0 = ui_ms();
        }
        return;
    }
    if (t == B_DONE) {
        if (npz_finish(&G)) won(npz_nearest(&G));
        else ui_sound(SND_DENY);
        return;
    }
    if (anim.on) return;
    int o = op_at(x, y);
    if (o >= 0) {
        if (sel < 0) { ui_sound(SND_DENY); return; }
        op = op == o ? -1 : o;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    down = tile_at(x, y);
    if (down < 0 && (sel >= 0 || op >= 0)) {    /* a tap beside: nothing chosen */
        sel = op = -1;
        ui_animate();
    }
}

static void motion(float x, float y) { (void)x, (void)y; }

static void release(float x, float y)
{
    int k = down, j = tile_at(x, y);
    down = -1;
    if (k < 0 || npz_solved(&G) || anim.on) return;
    if (j == k) tap_tile(k);
    else if (j >= 0) {                          /* a drag from k onto j: k's step with the operation chosen */
        if (op < 0) { ui_sound(SND_DENY); sel = k; ui_animate(); return; }
        try_merge(k, op, j);
    }
}

/* ---- the bar: the steps made; the hints used at the right ------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    if (t_lamp && ui_ink_dark()) SDL_SetTextureColorMod(t_lamp, 92, 70, 52);   /* the light theme: the lamp in ink */
    else if (t_lamp) SDL_SetTextureColorMod(t_lamp, 255, 255, 255);
    pzt_bar_two(b, t_small, G.nh, t_lamp, G.hints, 0);
}

static const game mathpz_game = {
    .id = "mathpz", .order = 447, .variants = NPZ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = NPZ_LEVELS, .chapters = NPZ_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_stars = level_stars, .levels_open = NPZ_LEVELS, .axes = {NPZ_GAMES, NPZ_TIERS},
    .axis_pic = {kind_pic, NULL}, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(mathpz_game)
