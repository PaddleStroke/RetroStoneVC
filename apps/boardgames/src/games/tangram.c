/*
 * Tangram in RetroStone Board, for one player: the silhouette laid dark on the green felt, the seven tans in the
 * house's woods (maple, oak, cherry, walnut, amber...) bevelled and lit from the top left, the shell's hint button
 * beside the table (the rules: tangram_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a tan: let go, it settles on the grid of the silhouette (when it stands at right angles). Tap a tan: it turns
 * an eighth of a turn; tap the parallelogram twice quickly: it turns over instead. The tan touched last lies on top.
 * The hint slides one tan into its place, in gold light. Undo takes back one action (a move, a turn, a turning over,
 * a hint), all the way to the start. The bar: the actions. Solved: gold light runs over the tans one after another.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pztools.h"
#include "games/tangram.h"
#include "shell.h"

#define SLIDE_MS 260
#define SNAP_MS 90
#define DOUBLE_MS 330
#define HINT_MS 1200
#define WAVE_STEP 110
#define WAVE_LEN 600

enum { B_HINT, BTNS };

static const uint8_t WOOD[TGM_PIECES][3] = {{198, 120, 72}, {214, 164, 96}, {150, 98, 60}, {236, 206, 150},
                                           {222, 184, 128}, {176, 82, 52}, {242, 222, 176}};

static tgm_game G;
static int set_no, zorder[TGM_PIECES];
static struct { int piece; float x0, y0; Uint32 t0, ms; } slide[TGM_PIECES];   /* a tan gliding to its place */
static struct { int on; Uint32 t0; } win;
static struct { int piece; Uint32 t0; } hint = {-1, 0};
static struct { int on, piece, moved; float x0, y0, gx, gy, x, y; } ptr;   /* gx, gy: the grip, in units */
static struct { int piece; Uint32 t; } last_tap = {-1, 0};

static box last_box;
static pzt_column col;
static float k, tx, ty;                         /* pixels a unit; the table's top left */
static int made_fig = -1, made_k = -1, made_small = -1;
static SDL_Texture *t_shadow, *t_tan[TGM_PIECES][16], *t_small;
static float tan_off[TGM_PIECES][16][2];        /* each texture's top left from the tan's middle (pixels) */
static int tan_w[TGM_PIECES][16], tan_h[TGM_PIECES][16];

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tan painted into a picture (w x h): its corners in pixels, its wood, a bevel lit from the top left, a shadow */
static void paint_tan(uint8_t *o, int w, int h, int piece, const float *xy, int n, float unit)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, area = 0;
    for (int i = 0; i < n; i++) {
        x0 = fminf(x0, xy[2 * i]);
        y0 = fminf(y0, xy[2 * i + 1]);
        x1 = fmaxf(x1, xy[2 * i]);
        y1 = fmaxf(y1, xy[2 * i + 1]);
        int j = (i + 1) % n;
        area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
    }
    float sh = 0.07f * unit, bev = fmaxf(1.5f, 0.09f * unit), sxy[8];
    for (int i = 0; i < 2 * n; i++) sxy[i] = xy[i] + sh * (i & 1 ? 1.2f : 0.8f);
    int ww = (int)ceilf(x1 - x0) + 1, hh = (int)ceilf(y1 - y0) + 1;
    uint8_t *plank = malloc((size_t)ww * hh * 4);
    if (!plank) return;
    pzt_plank(plank, ww, hh, piece * 5 + 2);
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1 + sh * 1.3f); y++)
        for (int x = (int)floorf(x0); x <= (int)ceilf(x1 + sh); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            uint8_t *p = o + 4 * (y * w + x);
            float s = pzt_cover(sxy, n, (float)x, (float)y) * 0.4f;   /* the shadow, under */
            if (s > 0 && p[3] < 255) {
                float a = s + p[3] / 255.0f * (1 - s);
                for (int c = 0; c < 3; c++) p[c] = (uint8_t)((p[c] * (p[3] / 255.0f) * (1 - s)) / (a > 0 ? a : 1));
                p[3] = (uint8_t)(255 * a);
            }
            float cov = pzt_cover(xy, n, (float)x, (float)y);
            if (cov <= 0) continue;
            int px = x - (int)floorf(x0), py = y - (int)floorf(y0);
            px = px < 0 ? 0 : px >= ww ? ww - 1 : px;
            py = py < 0 ? 0 : py >= hh ? hh - 1 : py;
            uint8_t wood[4];
            memcpy(wood, plank + 4 * (py * ww + px), 4);
            pzt_tint(wood, WOOD[piece], 1);
            /* the bevel: the nearest edge's facing (outward), lit when it faces the top left */
            float best = 1e9f, lit = 0;
            for (int i = 0, j = n - 1; i < n; j = i++) {
                float ax = xy[2 * j], ay = xy[2 * j + 1], dx = xy[2 * i] - ax, dy = xy[2 * i + 1] - ay;
                float l2 = dx * dx + dy * dy;
                float t = l2 > 0 ? clampf(((x + 0.5f - ax) * dx + (y + 0.5f - ay) * dy) / l2, 0, 1) : 0;
                float d = hypotf(x + 0.5f - ax - t * dx, y + 0.5f - ay - t * dy);
                if (d < best) {
                    best = d;
                    float l = sqrtf(l2) + 1e-6f, nx = dy / l, ny = -dx / l;   /* outward for a clockwise outline */
                    if (area < 0) { nx = -nx; ny = -ny; }
                    lit = -(nx + ny) * 0.7071f;
                }
            }
            float kf = best < bev ? 1 + 0.38f * lit * (1 - best / bev) : 1;
            float a = cov + p[3] / 255.0f * (1 - cov);
            for (int c = 0; c < 3; c++) {
                float v = clampf(wood[c] * kf, 0, 255);
                p[c] = (uint8_t)((v * cov + p[c] * (p[3] / 255.0f) * (1 - cov)) / (a > 0 ? a : 1));
            }
            p[3] = (uint8_t)(255 * a);
        }
    free(plank);
}

/* is (u, v) (units, from the silhouette's corner) inside it */
static int in_figure(const tgm_game *g, float u, float v)
{
    if (u < 0 || v < 0 || u >= g->tw || v >= g->th) return 0;
    int cx = (int)u, cy = (int)v;
    float a = u - cx, b = v - cy;
    int q = b < a && b < 1 - a ? 0 : a > b && a > 1 - b ? 1 : b > a && b > 1 - a ? 2 : 3;
    return g->target[cy][cx] >> q & 1;
}

/* the silhouette: dark on the felt, its edges smooth */
static void paint_shadow(const tgm_game *g, float unit, uint8_t *o, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int in = 0;
            for (int s = 0; s < 9; s++)
                in += in_figure(g, (x + (s % 3 + 0.5f) / 3) / unit, (y + (s / 3 + 0.5f) / 3) / unit);
            uint8_t *p = o + 4 * (y * w + x);
            p[0] = 16, p[1] = 30, p[2] = 25, p[3] = (uint8_t)(235 * in / 9);
        }
}

static void paint_figure(int fig, int size, uint8_t *rgba)   /* a silhouette's tans, laid (the cards, the token) */
{
    tgm_game T;
    tgm_new(&T, fig / TGM_PER_SET, fig % TGM_PER_SET, 1);
    float unit = size * 0.9f / (T.tw > T.th ? T.tw : T.th);
    float ox = (size - T.tw * unit) / 2, oy = (size - T.th * unit) / 2;
    for (int i = 0; i < TGM_PIECES; i++) {
        float xy[8];
        int n = tgm_corners(i, &T.slot[i], xy);
        for (int j = 0; j < n; j++) {
            xy[2 * j] = ox + (xy[2 * j] - T.fx) * unit;
            xy[2 * j + 1] = oy + xy[2 * j + 1] * unit;
        }
        paint_tan(rgba, size, size, i, xy, n, unit);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_figure(2 * TGM_PER_SET, size, rgba);  /* a house of tans */
}

/* a tan's texture for its turning (made at the table's scale when first drawn) */
static SDL_Texture *tan_tex(int piece, int rot, int flip)
{
    int v = rot + 8 * flip;
    if (t_tan[piece][v]) return t_tan[piece][v];
    tgm_place pl = {0, 0, rot, flip};
    float xy[8], x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int n = tgm_corners(piece, &pl, xy);
    for (int i = 0; i < n; i++) {
        xy[2 * i] *= k;
        xy[2 * i + 1] *= k;
        x0 = fminf(x0, xy[2 * i]);
        y0 = fminf(y0, xy[2 * i + 1]);
        x1 = fmaxf(x1, xy[2 * i]);
        y1 = fmaxf(y1, xy[2 * i + 1]);
    }
    float m = 0.12f * k + 2;
    int w = (int)ceilf(x1 - x0 + 2 * m), h = (int)ceilf(y1 - y0 + 2 * m);
    for (int i = 0; i < n; i++) {
        xy[2 * i] += m - x0;
        xy[2 * i + 1] += m - y0;
    }
    uint8_t *o = ui_buf(w, h);
    memset(o, 0, (size_t)w * h * 4);
    paint_tan(o, w, h, piece, xy, n, k);
    t_tan[piece][v] = ui_tex(w, h, o);
    tan_off[piece][v][0] = x0 - m;
    tan_off[piece][v][1] = y0 - m;
    tan_w[piece][v] = w;
    tan_h[piece][v] = h;
    return t_tan[piece][v];
}

static void layout(box b)
{
    last_box = b;
    pzt_column_fit(&col, b, BTNS);
    box r = col.room;
    k = floorf(fminf(r.w / G.table_w, r.h / G.table_h));
    tx = floorf(r.x + (r.w - G.table_w * k) / 2);
    ty = floorf(r.y + (r.h - G.table_h * k) / 2);
    if (made_k != (int)k)
        for (int i = 0; i < TGM_PIECES; i++)
            for (int v = 0; v < 16; v++) ui_free(&t_tan[i][v]);
    if (made_fig != G.figure || made_k != (int)k) {
        made_fig = G.figure;
        int w = (int)ceilf(G.tw * k), h = (int)ceilf(G.th * k);
        ui_free(&t_shadow);
        paint_shadow(&G, k, ui_buf(w, h), w, h);
        t_shadow = ui_tex(w, h, ui_buf(w, h));
    }
    made_k = (int)k;
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    for (int i = 0; i < TGM_PIECES; i++) slide[i].ms = 0;
    win.on = 0;
    hint.piece = last_tap.piece = -1;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    set_no = variant < 0 || variant >= TGM_SETS ? 0 : variant;
    tgm_new(&G, set_no, ui_random(TGM_PER_SET), (uint32_t)ui_random(0x7fffffff) + 1u);
    for (int i = 0; i < TGM_PIECES; i++) zorder[i] = i;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return tgm_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }

static Uint32 win_end(void) { return win.t0 + (Uint32)(TGM_PIECES * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < TGM_PIECES; i++)
        if (slide[i].ms && now - slide[i].t0 < slide[i].ms) return 1;
    return win.on && now < win_end();
}

static void on_top(int piece)
{
    int at = 0;
    for (int i = 0; i < TGM_PIECES; i++)
        if (zorder[i] == piece) at = i;
    for (int i = at; i < TGM_PIECES - 1; i++) zorder[i] = zorder[i + 1];
    zorder[TGM_PIECES - 1] = piece;
}

static void glide(int piece, float x0, float y0, Uint32 ms)   /* from (x0, y0) to its place now */
{
    slide[piece].x0 = x0;
    slide[piece].y0 = y0;
    slide[piece].t0 = ui_ms();
    slide[piece].ms = ms;
    ui_animate();
}

static void after(void)
{
    if (tgm_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms() + SLIDE_MS;
        ui_sound_later(SND_END, SLIDE_MS);
    }
    ui_animate();
}

static int undo(void)
{
    int i = tgm_undo(&G);
    if (i < 0) return 0;
    calm();
    on_top(i);
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = tgm_script(&G, moves);
    if (made_fig != G.figure && last_box.w > 0) layout(last_box);
    if (tgm_solved(&G)) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* ---- the table on the screen ------------------------------------------------------------------------------------- */
static void place_of(int i, Uint32 now, float *x, float *y)   /* its middle (units), gliding or carried */
{
    *x = G.p[i].x;
    *y = G.p[i].y;
    if (ptr.on && ptr.moved && ptr.piece == i) {
        *x = (ptr.x - tx) / k - ptr.gx;
        *y = (ptr.y - ty) / k - ptr.gy;
        return;
    }
    if (slide[i].ms && now - slide[i].t0 < slide[i].ms) {
        float t = puz_ease((now - slide[i].t0) / (float)slide[i].ms);
        *x = slide[i].x0 + (*x - slide[i].x0) * t;
        *y = slide[i].y0 + (*y - slide[i].y0) * t;
        ui_animate();
    }
}

static void draw(void)
{
    if (made_k < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_shadow, tx + G.fx * k, ty, G.tw * k, G.th * k);
    float wt = win.on && now >= win.t0 ? (float)(now - win.t0) : -1;
    if (win.on && now < win_end()) ui_animate();
    if (wt >= 0) {                              /* the whole silhouette glows as it fills */
        float p = puz_pulse(wt, 0, TGM_PIECES * WAVE_STEP + WAVE_LEN);
        ui_mark_gold(tx + (G.fx + G.tw / 2.0f) * k, ty + G.th / 2.0f * k, 1.3f * fmaxf(G.tw, G.th) * k * p);
    }
    for (int z = 0; z < TGM_PIECES; z++) {
        int i = zorder[z];
        float x, y;
        place_of(i, now, &x, &y);
        SDL_Texture *t = tan_tex(i, G.p[i].rot, G.p[i].flip);
        int v = G.p[i].rot + 8 * G.p[i].flip;
        float px = tx + x * k, py = ty + y * k;
        if (wt >= 0) {
            float p = puz_pulse(wt, z * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(px, py, 2.0f * k * p);
        }
        if (hint.piece == i) {
            float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
            if (p > 0) { ui_mark_gold(px, py, 2.6f * k * p); ui_animate(); }
            else hint.piece = -1;
        }
        float lift = ptr.on && ptr.moved && ptr.piece == i ? 1.04f : 1;
        float w = tan_w[i][v] * lift, h = tan_h[i][v] * lift;
        ui_put(t, px + tan_off[i][v][0] * lift, py + tan_off[i][v][1] * lift, w, h);
    }
    ui_button(col.x, col.y[B_HINT], col.d, ICON_HINT, tgm_solved(&G) ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int tan_at(float x, float y)              /* the top tan under a point (a little margin round each) */
{
    float u = (x - tx) / k, v = (y - ty) / k;
    for (int pass = 0; pass < 2; pass++)
        for (int z = TGM_PIECES - 1; z >= 0; z--) {
            int i = zorder[z];
            float xy[8];
            int n = tgm_corners(i, &G.p[i], xy);
            int c = 0;
            for (int a = 0, b = n - 1; a < n; b = a++) {
                float xa = xy[2 * a], ya = xy[2 * a + 1], xb = xy[2 * b], yb = xy[2 * b + 1];
                if ((ya > v) != (yb > v) && u < (xb - xa) * (v - ya) / (yb - ya) + xa) c = !c;
            }
            if (c) return i;
            if (pass && pzt_edge_dist(xy, n, u, v) < 0.25f) return i;   /* near its edge: still it */
        }
    return -1;
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (tgm_solved(&G)) return;
    if (pzt_column_hit(&col, x, y) == B_HINT) {
        float x0[TGM_PIECES], y0[TGM_PIECES];
        for (int i = 0; i < TGM_PIECES; i++) x0[i] = G.p[i].x, y0[i] = G.p[i].y;
        int i = tgm_hint(&G);
        if (i < 0) { ui_sound(SND_DENY); return; }
        on_top(i);
        glide(i, x0[i], y0[i], SLIDE_MS);
        hint.piece = i;
        hint.t0 = ui_ms();
        ui_sound(SND_MOVE);
        ui_sound_later(SND_PLACE, SLIDE_MS - 30);
        after();
        return;
    }
    int i = tan_at(x, y);
    if (i < 0) return;
    on_top(i);
    ptr.on = 1;
    ptr.piece = i;
    ptr.x = ptr.x0 = x;
    ptr.y = ptr.y0 = y;
    ptr.gx = (x - tx) / k - G.p[i].x;
    ptr.gy = (y - ty) / k - G.p[i].y;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.x = x;
    ptr.y = y;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.18f * k) { ptr.moved = 1; ui_sound(SND_TAP); }
    ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    int i = ptr.piece;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved) {
        float ux = (x - tx) / k - ptr.gx, uy = (y - ty) / k - ptr.gy;
        tgm_move(&G, i, ux, uy);
        glide(i, ux, uy, SNAP_MS);
        ui_sound(SND_PLACE);
        last_tap.piece = -1;
        after();
        return;
    }
    Uint32 now = ui_ms();
    if (i == 6 && last_tap.piece == 6 && now - last_tap.t < DOUBLE_MS) {   /* twice: turned over instead */
        tgm_undo(&G);
        tgm_flip(&G, 6);
        last_tap.piece = -1;
    } else {
        tgm_turn(&G, i);
        last_tap.piece = i;
        last_tap.t = now;
    }
    ui_sound(SND_TAP);
    after();
}

/* ---- the bar: the actions ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    puz_bar_moves(b, t_small, G.moves);
}

/* ---- the menu's and the picker's cards: a silhouette of the set laid in tans ---------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    paint_figure(variant < 0 ? 0 : variant * TGM_PER_SET, size, rgba);
}

static int score(void) { return G.moves; }   /* the end card's number */

static const game tangram_game = {
    .id = "tangram", .order = 444, .variants = TGM_SETS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
};
REGISTER_GAME(tangram_game)
