/*
 * Tangram in RetroStone Board, for one player: a plank of the house wood in its walnut frame, the silhouette inlaid
 * dark in it, the seven tans cut from the plank and lacquered (red, lapis, jade, amber, ivory, walnut), bevelled and
 * lit from the top left; the tray beside the silhouette, the two filling the room (game.fill); the hint is the shell's
 * tool (the rules: tangram_rules.c). The shell's level screen chooses the figure: six chapters, the stars for the hints not taken.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a tan: let go, it settles in a fitting place (its corners onto the silhouette's, another tan's, or the grid).
 * Tap a tan: it turns an eighth of a turn; tap the parallelogram twice quickly: it turns over instead. The tan touched
 * last lies on top. The hint slides one tan into its place, in gold light. Undo takes back one action (a move, a
 * turn, a turning over, a hint), all the way to the start. The bar: the actions. Solved: gold light runs over the
 * tans one after another.
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
#define TURN_MS 130
#define DOUBLE_MS 330
#define HINT_MS 1200
#define WAVE_STEP 110
#define WAVE_LEN 600

enum { B_HINT, BTNS };

/* the lacquers: red, lapis, jade, amber (both small triangles), ivory, walnut */
static const uint8_t LACQUER[TGM_PIECES][3] = {{186, 62, 46}, {60, 96, 168}, {66, 140, 106}, {230, 172, 70},
                                              {230, 172, 70}, {244, 232, 206}, {150, 100, 62}};

static tgm_game G;
static int zorder[TGM_PIECES];
static struct { int piece; float x0, y0; Uint32 t0, ms; } slide[TGM_PIECES];   /* a tan gliding to its place */
static struct { int piece, rot0, flip0; Uint32 t0; } turning = {-1, 0, 0, 0};    /* a tan turning an eighth */
static struct { int on; Uint32 t0; } win;
static struct { int piece; Uint32 t0; } hint = {-1, 0};
static struct { int on, piece, moved; float x0, y0, gx, gy, x, y; } ptr;   /* gx, gy: the grip, in units */
static struct { int piece; Uint32 t; } last_tap = {-1, 0};

static box last_box;
static float k, tx, ty;                         /* pixels a unit; the table's top left (inside the frame) */
static int made_level = -1, made_k = -1, made_right = -1, made_small = -1, frame;
static SDL_Texture *t_board, *t_tan[TGM_PIECES][16], *t_small;
static float tan_off[TGM_PIECES][16][2];        /* each texture's top left from the tan's middle (pixels) */
static int tan_w[TGM_PIECES][16], tan_h[TGM_PIECES][16];

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tan painted into a picture (w x h): its corners in pixels, the plank lacquered, a bevel lit from the top left, a
 * shadow under it */
static void paint_tan(uint8_t *o, int w, int h, int piece, const float *xy, int n, float unit, int shadow)
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
    float sh = shadow ? 0.07f * unit : 0, bev = fmaxf(1.2f, 0.08f * unit), sxy[8];
    for (int i = 0; i < 2 * n; i++) sxy[i] = xy[i] + sh * (i & 1 ? 1.2f : 0.8f);
    int ww = (int)ceilf(x1 - x0) + 1, hh = (int)ceilf(y1 - y0) + 1;
    uint8_t *plank = malloc((size_t)ww * hh * 4);
    if (!plank) return;
    pzt_plank(plank, ww, hh, piece * 5 + 2);
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1 + sh * 1.3f); y++)
        for (int x = (int)floorf(x0); x <= (int)ceilf(x1 + sh); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            uint8_t *p = o + 4 * (y * w + x);
            float s = shadow ? pzt_cover(sxy, n, (float)x, (float)y) * 0.4f : 0;   /* the shadow, under */
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
            pzt_tint(wood, LACQUER[piece], 1);
            /* the bevel: the nearest edge's facing (outward), lit when it faces the top left; a sheen across */
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
            float kf = best < bev ? 1 + 0.4f * lit * (1 - best / bev) : 1;
            kf *= 1.06f - 0.12f * ((x - x0) + (y - y0)) / (x1 - x0 + y1 - y0 + 1);   /* the lacquer's sheen */
            float a = cov + p[3] / 255.0f * (1 - cov);
            for (int c = 0; c < 3; c++) {
                float v = clampf(wood[c] * kf, 0, 255);
                p[c] = (uint8_t)((v * cov + p[c] * (p[3] / 255.0f) * (1 - cov)) / (a > 0 ? a : 1));
            }
            p[3] = (uint8_t)(255 * a);
        }
    free(plank);
}

/* a level's tans fitted in a square picture: laid in their lacquers (tans 1), or the silhouette in ink */
static void paint_figure(int level, int size, uint8_t *rgba, int tans, const uint8_t ink[3])
{
    tgm_place s[TGM_PIECES];
    double W, H;
    tgm_solution(level, s, &W, &H);
    float unit = size * 0.88f / (float)(W > H ? W : H);
    float ox = (size - (float)W * unit) / 2, oy = (size - (float)H * unit) / 2;
    float poly[TGM_PIECES][8];
    int n[TGM_PIECES];
    for (int i = 0; i < TGM_PIECES; i++) {
        double xy[8];
        n[i] = tgm_corners(i, &s[i], xy);
        for (int j = 0; j < n[i]; j++) {
            poly[i][2 * j] = ox + (float)xy[2 * j] * unit;
            poly[i][2 * j + 1] = oy + (float)xy[2 * j + 1] * unit;
        }
        if (tans) paint_tan(rgba, size, size, i, poly[i], n[i], unit, 0);
    }
    if (tans) return;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float c = 0;
            for (int i = 0; i < TGM_PIECES && c < 1; i++) c += pzt_cover(poly[i], n[i], (float)x, (float)y);
            if (c <= 0) continue;
            uint8_t *p = rgba + 4 * (y * size + x);
            p[0] = ink[0], p[1] = ink[1], p[2] = ink[2], p[3] = (uint8_t)(255 * fminf(c, 1));
        }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_figure(0, size, rgba, 1, NULL);   /* the square of the seven tans */
}

/* the board: the plank in its frame, the silhouette inlaid dark (a sunken edge: shaded at the top left, lit at the
 * bottom right) */
static void paint_board(void)
{
    int W = G.table_w * (int)k + 2 * frame, H = G.table_h * (int)k + 2 * frame;
    uint8_t *o = ui_buf(W, H);
    art_board_n(G.table_w, G.table_h, (int)k, frame, 0, o);
    static const uint8_t INK[3] = {40, 28, 22};
    int X0 = frame + (int)floor(G.fx * k) - 2, Y0 = frame + (int)floor(G.fy * k) - 2;
    int X1 = frame + (int)ceil((G.fx + G.fw) * k) + 2, Y1 = frame + (int)ceil((G.fy + G.fh) * k) + 2;
    float e = fmaxf(1.5f, 0.05f * k);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            int in = 0, up = 0, down = 0;
            for (int s = 0; s < 9; s++) {
                double u = (x - frame + (s % 3 + 0.5) / 3) / k, v = (y - frame + (s / 3 + 0.5) / 3) / k;
                if (!tgm_inside(&G, u, v)) continue;
                in++;
                up += !tgm_inside(&G, u - e / k, v - e / k);
                down += !tgm_inside(&G, u + e / k, v + e / k);
            }
            if (!in) continue;
            uint8_t *p = o + 4 * (y * W + x);
            float a = in / 9.0f * 0.88f, lift = (down - up) / 9.0f * 0.22f;
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(p[c] + (INK[c] - p[c]) * a + 255 * lift * a, 0, 255);
        }
    ui_free(&t_board);
    t_board = ui_tex(W, H, o);
}

/* a tan's texture for its turning (made at the table's scale when first drawn) */
static SDL_Texture *tan_tex(int piece, int rot, int flip)
{
    int v = rot + 8 * flip;
    if (t_tan[piece][v]) return t_tan[piece][v];
    tgm_place pl = {0, 0, rot, flip};
    double c[8];
    float xy[8], x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int n = tgm_corners(piece, &pl, c);
    for (int i = 0; i < n; i++) {
        xy[2 * i] = (float)c[2 * i] * k;
        xy[2 * i + 1] = (float)c[2 * i + 1] * k;
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
    paint_tan(o, w, h, piece, xy, n, k, 1);
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
    box r = b;                                  /* the whole room (game.fill) */
    /* the tray under the silhouette or at its right: whichever lets the table be the larger */
    tgm_game a = G, c = G;
    tgm_arrange(&a, 0);
    tgm_arrange(&c, 1);
    float ka = floorf(fminf(r.w / (a.table_w + 0.5f), r.h / (a.table_h + 0.5f)));
    float kc = floorf(fminf(r.w / (c.table_w + 0.5f), r.h / (c.table_h + 0.5f)));
    int right = kc > ka;
    if (right != G.tray_right) {
        tgm_arrange(&G, right);
        for (int i = 0; i < TGM_PIECES; i++) slide[i].ms = 0;
    }
    k = right ? kc : ka;
    if (k < 4) k = 4;
    frame = (int)(0.25f * k);
    tx = floorf(r.x + (r.w - G.table_w * k) / 2);
    ty = floorf(r.y + (r.h - G.table_h * k) / 2);
    if (made_k != (int)k)
        for (int i = 0; i < TGM_PIECES; i++)
            for (int v = 0; v < 16; v++) ui_free(&t_tan[i][v]);
    if (made_level != G.level || made_k != (int)k || made_right != G.tray_right) paint_board();
    made_level = G.level;
    made_k = (int)k;
    made_right = G.tray_right;
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
    hint.piece = last_tap.piece = turning.piece = -1;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    int right = G.tray_right;
    tgm_new(&G, level, (uint32_t)ui_random(0x7fffffff) + 1u);
    tgm_arrange(&G, right);
    for (int i = 0; i < TGM_PIECES; i++) zorder[i] = i;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return tgm_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return tgm_stars(&G); }
static int chapter_of(int level) { return tgm_chapter(level); }

static Uint32 win_end(void) { return win.t0 + (Uint32)(TGM_PIECES * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < TGM_PIECES; i++)
        if (slide[i].ms && now - slide[i].t0 < slide[i].ms) return 1;
    if (turning.piece >= 0 && now - turning.t0 < TURN_MS) return 1;
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
    if (made_level != G.level && last_box.w > 0) layout(last_box);
    if (tgm_solved(&G)) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* ---- the table on the screen ------------------------------------------------------------------------------------- */
static void place_of(int i, Uint32 now, float *x, float *y)   /* its middle (units), gliding or carried */
{
    *x = (float)G.p[i].x;
    *y = (float)G.p[i].y;
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

static void put_tan(int i, int rot, int flip, float px, float py, float lift, float deg)
{
    SDL_Texture *t = tan_tex(i, rot, flip);
    int v = rot + 8 * flip;
    float w = tan_w[i][v] * lift, h = tan_h[i][v] * lift;
    float ox = tan_off[i][v][0] * lift + w / 2, oy = tan_off[i][v][1] * lift + h / 2;   /* its centre from its middle */
    if (deg == 0) { ui_put(t, px + ox - w / 2, py + oy - h / 2, w, h); return; }
    float a = deg * PUZ_PI / 180, c = cosf(a), s = sinf(a);
    ui_put_rot(t, px + ox * c - oy * s, py + ox * s + oy * c, w, h, deg);
}

static void draw(void)
{
    if (made_k < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, tx - frame, ty - frame, G.table_w * k + 2 * frame, G.table_h * k + 2 * frame);
    float wt = win.on && now >= win.t0 ? (float)(now - win.t0) : -1;
    if (win.on && now < win_end()) ui_animate();
    if (wt >= 0) {                              /* the whole silhouette glows as it fills */
        float p = puz_pulse(wt, 0, TGM_PIECES * WAVE_STEP + WAVE_LEN);
        ui_mark_gold(tx + (float)(G.fx + G.fw / 2) * k, ty + (float)(G.fy + G.fh / 2) * k,
                     1.3f * (float)fmax(G.fw, G.fh) * k * p);
    }
    for (int z = 0; z < TGM_PIECES; z++) {
        int i = zorder[z];
        float x, y;
        place_of(i, now, &x, &y);
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
        if (turning.piece == i && now - turning.t0 < TURN_MS) {   /* the old turning, swung round */
            float t = puz_ease((now - turning.t0) / (float)TURN_MS);
            put_tan(i, turning.rot0, turning.flip0, px, py, lift, 45 * t);
            ui_animate();
            continue;
        }
        put_tan(i, G.p[i].rot, G.p[i].flip, px, py, lift, 0);
    }
    ui_tool(B_HINT, ICON_HINT, tgm_solved(&G) ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int tan_at(float x, float y)              /* the top tan under a point (a little margin round each) */
{
    float u = (x - tx) / k, v = (y - ty) / k;
    for (int pass = 0; pass < 2; pass++)
        for (int z = TGM_PIECES - 1; z >= 0; z--) {
            int i = zorder[z];
            double c[8];
            float xy[8];
            int n = tgm_corners(i, &G.p[i], c), in = 0;
            for (int j = 0; j < 2 * n; j++) xy[j] = (float)c[j];
            for (int a = 0, b = n - 1; a < n; b = a++) {
                float xa = xy[2 * a], ya = xy[2 * a + 1], xb = xy[2 * b], yb = xy[2 * b + 1];
                if ((ya > v) != (yb > v) && u < (xb - xa) * (v - ya) / (yb - ya) + xa) in = !in;
            }
            if (in) return i;
            if (pass && pzt_edge_dist(xy, n, u, v) < 0.25f) return i;   /* near its edge: still it */
        }
    return -1;
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (tgm_solved(&G)) return;
    if (ui_tool_hit(x, y) == B_HINT) {
        float x0[TGM_PIECES], y0[TGM_PIECES];
        for (int i = 0; i < TGM_PIECES; i++) x0[i] = (float)G.p[i].x, y0[i] = (float)G.p[i].y;
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
    ptr.gx = (x - tx) / k - (float)G.p[i].x;
    ptr.gy = (y - ty) / k - (float)G.p[i].y;
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
    float x0 = (float)G.p[i].x, y0 = (float)G.p[i].y;
    if (i == 6 && last_tap.piece == 6 && now - last_tap.t < DOUBLE_MS) {   /* twice: turned over instead */
        tgm_undo(&G);
        x0 = (float)G.p[i].x;
        y0 = (float)G.p[i].y;
        tgm_flip(&G, 6);
        turning.piece = last_tap.piece = -1;
    } else {
        turning.piece = i;
        turning.rot0 = G.p[i].rot;
        turning.flip0 = G.p[i].flip;
        turning.t0 = now;
        tgm_turn(&G, i);
        last_tap.piece = i;
        last_tap.t = now;
    }
    glide(i, x0, y0, SNAP_MS);
    ui_sound(SND_TAP);
    after();
}

/* ---- the bar: the actions ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    puz_bar_moves(b, t_small, G.moves);
}

/* ---- the pictures for the shell: the level screen's tiles (the silhouette), its tabs (the chapter's first
 * figure), the menu's card (the square of the seven tans) -------------------------------------------------------- */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    static const uint8_t ink[3] = {44, 30, 22};
    paint_figure(level, size, rgba, 0, ink);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int level = 0;
    while (level < TGM_LEVELS - 1 && tgm_chapter(level) < chapter) level++;
    paint_figure(level, size, rgba, 0, PZT_CREAM);
}

/* the menu's card: the square of the seven tans on the table's plank in its walnut frame, as the other puzzles' */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int f = (int)(size * 0.035f), in = (int)(size * 0.94f) - 2 * f, w = in + 2 * f, q = (int)(0.96f * in);
    uint8_t *b = malloc((size_t)w * w * 4), *t = calloc((size_t)q * q, 4);
    if (b && t) {
        art_plank(in, in, f, b);
        paint_figure(0, q, t, 1, NULL);
        art_blend(b, w, w, t, q, q, f + (in - q) / 2, f + (in - q) / 2);
        art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    }
    free(b);
    free(t);
}

static int score(void) { return G.moves; }   /* the end card's number */

static const game tangram_game = {
    .id = "tangram", .order = 444, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = TGM_LEVELS, .chapters = TGM_CHAPTERS, .chapter_of = chapter_of, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .levels_open = TGM_LEVELS,
    .fill = 1, .tools = BTNS,
};
REGISTER_GAME(tangram_game)
