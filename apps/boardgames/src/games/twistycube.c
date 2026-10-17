/*
 * Twisty Cube in RetroStone Board, for one player: the cube in three dimensions over the green table (seen from above
 * and to the side, a mild perspective), an ebony body under rounded stickers in six colours, shaded by a light from
 * the top left, each a little raised at its rim; a soft shadow on the felt. Optional: a small dark symbol on each
 * colour (the eye tool), for colour-blind players. The rules: twistycube_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe across a sticker: its layer turns that way (a quarter turn, animated). Swipe on the table: the whole cube
 * turns (only the view: no move); the arrow keys too. Taps alone: tap a sticker (it glows), then another one where
 * it should go. The lamp (short scrambles only): the hint plays the next move back towards the solved cube. Undo takes
 * back one move, all the way to the start. The bar: the moves, the time since the first one, and the par (a flag).
 * Solved: the cube spins once in gold light.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "games/twistycube.h"
#include "shell.h"

#define TURN_MS 190
#define VIEW_MS 230
#define WIN_MS 1300
#define CAM_D 8.0f                              /* the eye's distance (cube units: the cube is -1..1) */
#define YAW (-38.0f)
#define PITCH 27.0f

enum { B_EYE, B_HINT, BTNS };

/* the faces' colours: R L U D F B (red lacquer, orange, ivory, amber yellow, jade, lapis) */
static const uint8_t COL[6][3] = {
    {178, 40, 36}, {228, 116, 32}, {238, 230, 210}, {236, 194, 46}, {40, 146, 90}, {38, 84, 178},
};
static const uint8_t BODY[3] = {30, 26, 24};

/* ---- the camera: cube coordinates -> the screen ---------------------------------------------------------------- */
typedef struct {
    float m[9];                                 /* the cube's turn as seen (view then the camera's tilt) */
    float cx, cy, s;                            /* the cube's middle on the screen, pixels a cube unit */
} cam;

static void mat_mul(const float *a, const float *b, float *o)   /* o = a b */
{
    float t[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) t[3 * i + j] = a[3 * i] * b[j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
    memcpy(o, t, sizeof t);
}

static void mat_rot(int axis, float deg, float *o)   /* counter-clockwise seen from +axis */
{
    float a = deg * PUZ_PI / 180, c = cosf(a), s = sinf(a);
    int u = (axis + 1) % 3, v = (axis + 2) % 3;
    memset(o, 0, 9 * sizeof *o);
    o[4 * axis] = 1;
    o[3 * u + u] = c;
    o[3 * u + v] = -s;
    o[3 * v + u] = s;
    o[3 * v + v] = c;
}

static void mat_apply(const float *m, const float *p, float *o)
{
    float t[3];
    for (int i = 0; i < 3; i++) t[i] = m[3 * i] * p[0] + m[3 * i + 1] * p[1] + m[3 * i + 2] * p[2];
    memcpy(o, t, sizeof t);
}

static void tilt(float *o)                      /* the camera's tilt: turned left a little, then looking down */
{
    float a[9], b[9];
    mat_rot(1, YAW, a);
    mat_rot(0, PITCH, b);
    mat_mul(b, a, o);
}

static void cam_make(cam *k, const float *view, float cx, float cy, float s)
{
    float t[9];
    tilt(t);
    mat_mul(t, view, k->m);
    k->cx = cx;
    k->cy = cy;
    k->s = s;
}

static void cam_proj(const cam *k, const float *p, float *sx, float *sy, float *z)   /* p in the camera's frame */
{
    float f = CAM_D / (CAM_D - p[2]);
    *sx = k->cx + k->s * p[0] * f;
    *sy = k->cy - k->s * p[1] * f;
    if (z) *z = p[2];
}

/* ---- the cube as quads, back to front -------------------------------------------------------------------------- */
enum { Q_BODY, Q_STICKER };
typedef struct {
    float x[4], y[4];
    uint8_t kind, col;
    float shade;
    int sticker;
} quad;
#define MAXQ 200

static float shade_of(const float *ncam)        /* the light from the top left, in front */
{
    static const float L[3] = {-0.38f, 0.62f, 0.69f};
    float d = ncam[0] * L[0] + ncam[1] * L[1] + ncam[2] * L[2];
    return 0.6f + 0.4f * fmaxf(0, d);
}

/* the cube's quads seen by camera k: its layers turned by `deg` round `axis` for layer `layer` (deg 0: at rest);
 * stickers `in` of a cubie's face (1: the whole face, the hit test's) */
static int cube_quads(const cam *k, const tcb_cube *c, int axis, int layer, float deg, float in, quad *q)
{
    int n = c->n, nq = 0;
    int lo[3], hi[3], nch = 0;                  /* the chunks: layers lo..hi along the axis */
    if (deg == 0 || layer < 0) { lo[0] = 0; hi[0] = n - 1; nch = 1; axis = axis < 0 ? 0 : axis; }
    else {
        if (layer > 0) { lo[nch] = 0; hi[nch++] = layer - 1; }
        lo[nch] = layer; hi[nch++] = layer;
        if (layer < n - 1) { lo[nch] = layer + 1; hi[nch++] = n - 1; }
    }
    float R[9], M[9];
    mat_rot(axis, deg, R);
    float zc[3];
    int order[3] = {0, 1, 2};
    for (int i = 0; i < nch; i++) {             /* each chunk's middle: the farther first */
        float p[3] = {0, 0, 0};
        p[axis] = -1 + (lo[i] + hi[i] + 1.0f) / n;
        mat_apply(k->m, p, p);
        zc[i] = p[2];
    }
    for (int i = 0; i < nch; i++)
        for (int j = i + 1; j < nch; j++)
            if (zc[order[j]] < zc[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int oi = 0; oi < nch; oi++) {
        int ch = order[oi], turning = deg != 0 && lo[ch] == layer && hi[ch] == layer;
        if (turning) mat_mul(k->m, R, M);
        else memcpy(M, k->m, sizeof M);
        /* the body: a box, its faces seen */
        float b0[3] = {-1, -1, -1}, b1[3] = {1, 1, 1};
        b0[axis] = -1 + 2.0f * lo[ch] / n;
        b1[axis] = -1 + 2.0f * (hi[ch] + 1) / n;
        for (int f = 0; f < 6; f++) {
            int a = f / 2, sg = f % 2 ? -1 : 1, u = (a + 1) % 3, v = (a + 2) % 3;
            float corner[4][3];
            static const int U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1};
            for (int i = 0; i < 4; i++) {
                int ii = sg > 0 ? i : 3 - i;
                corner[i][a] = sg > 0 ? b1[a] : b0[a];
                corner[i][u] = U[ii] ? b1[u] : b0[u];
                corner[i][v] = V[ii] ? b1[v] : b0[v];
            }
            quad *o = &q[nq];
            float area = 0;
            for (int i = 0; i < 4; i++) {
                float w[3];
                mat_apply(M, corner[i], w);
                cam_proj(k, w, &o->x[i], &o->y[i], NULL);
            }
            for (int i = 0; i < 4; i++) area += o->x[i] * o->y[(i + 1) % 4] - o->x[(i + 1) % 4] * o->y[i];
            if (area > -0.5f) continue;
            float nr[3] = {0, 0, 0};
            nr[a] = (float)sg;
            mat_apply(M, nr, nr);
            o->kind = Q_BODY;
            o->shade = shade_of(nr);
            o->sticker = -1;
            if (nq < MAXQ) nq++;
        }
        /* the stickers on it */
        for (int s = 0; s < 6 * n * n; s++) {
            int p[3], nr[3];
            tcb_place(n, s, p, nr);
            int l = (p[axis] + n - 1) / 2;
            if (l < lo[ch] || l > hi[ch]) continue;
            int a = nr[0] ? 0 : nr[1] ? 1 : 2, sg = nr[a], u = (a + 1) % 3, v = (a + 2) % 3;
            float ctr[3] = {(float)p[0] / n, (float)p[1] / n, (float)p[2] / n}, h = in / n;
            ctr[a] = (float)sg * 1.002f;
            quad *o = &q[nq];
            static const int U[4] = {-1, 1, 1, -1}, V[4] = {-1, -1, 1, 1};
            float area = 0;
            for (int i = 0; i < 4; i++) {
                int ii = sg > 0 ? i : 3 - i;
                float w[3] = {ctr[0], ctr[1], ctr[2]};
                w[u] += U[ii] * h;
                w[v] += V[ii] * h;
                mat_apply(M, w, w);
                cam_proj(k, w, &o->x[i], &o->y[i], NULL);
            }
            for (int i = 0; i < 4; i++) area += o->x[i] * o->y[(i + 1) % 4] - o->x[(i + 1) % 4] * o->y[i];
            if (area > -0.5f) continue;
            float nn[3] = {(float)nr[0], (float)nr[1], (float)nr[2]};
            mat_apply(M, nn, nn);
            o->kind = Q_STICKER;
            o->col = c->c[s];
            o->shade = shade_of(nn);
            o->sticker = s;
            if (nq < MAXQ) nq++;
        }
    }
    return nq;
}

/* ---- painting: the sticker, the symbols, the cube into a picture ---------------------------------------------- */
static float rrect(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    float dx = fabsf(px - cx) - hx, dy = fabsf(py - cy) - hy;
    return hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0) - r;
}

/* a sticker in white (tinted by the vertex colour): a rounded square, a little lighter at its raised rim, a fine dark
 * line at its very edge */
static void paint_sticker(int S, uint8_t *o)
{
    float m = 0.07f * S, r = 0.17f * S, b = 0.13f * S;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            uint8_t *p = o + 4 * ((size_t)y * S + x);
            float d = rrect(x + 0.5f, y + 0.5f, m, m, S - m, S - m, r), e = -d;
            float lum = 0.86f + 0.04f * art_noise(x, y);
            if (e < b) lum += 0.14f * (1 - e / b) * (e / b) * 4 * 0.5f;
            if (e < 0.035f * S) lum *= 0.72f;
            uint8_t g = (uint8_t)clampf(255 * lum, 0, 255);
            p[0] = p[1] = p[2] = g;
            p[3] = (uint8_t)(255 * clampf(0.5f - d, 0, 1));
        }
}

/* the symbol of colour k (white on clear): a disc, a ring, a plus, a triangle, a diamond, a cross */
static float symbol_dist(int k, float u, float v)   /* u, v: -1..1 over the symbol */
{
    switch (k) {
    case 0: return hypotf(u, v) - 0.62f;
    case 1: return fabsf(hypotf(u, v) - 0.6f) - 0.2f;
    case 2: return fminf(fmaxf(fabsf(u) - 0.8f, fabsf(v) - 0.22f), fmaxf(fabsf(v) - 0.8f, fabsf(u) - 0.22f));
    case 3: return fmaxf(-0.48f - v, 0.887f * fabsf(u) + 0.463f * (v - 0.72f));   /* a triangle pointing up */
    case 4: return (fabsf(u) + fabsf(v)) * 0.7071f - 0.58f;
    default: {
        float s = 0.7071f, a = (u + v) * s, b = (u - v) * s;
        return fminf(fmaxf(fabsf(a) - 0.85f, fabsf(b) - 0.2f), fmaxf(fabsf(b) - 0.85f, fabsf(a) - 0.2f));
    }
    }
}

static void paint_symbol(int k, int S, uint8_t *o)
{
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float u = (x + 0.5f) / S * 2 - 1, v = 1 - (y + 0.5f) / S * 2;
            float d = symbol_dist(k, u * 1.25f, v * 1.25f) * S / 2.5f;
            uint8_t *p = o + 4 * ((size_t)y * S + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * clampf(0.5f - d, 0, 1));
        }
}

static void px_over(uint8_t *p, const uint8_t rgb[3], float a)
{
    if (a <= 0) return;
    if (a > 1) a = 1;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)((rgb[i] * a + p[i] * da * (1 - a)) / oa + 0.5f);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

static void fill_quad(uint8_t *o, int W, int H, const quad *q, const uint8_t rgb[3], float shrink)
{
    float xy[8], mx = 0, my = 0;
    for (int i = 0; i < 4; i++) { mx += q->x[i] / 4; my += q->y[i] / 4; }
    for (int i = 0; i < 4; i++) {
        xy[2 * i] = mx + (q->x[i] - mx) * shrink;
        xy[2 * i + 1] = my + (q->y[i] - my) * shrink;
    }
    float x0 = W, y0 = H, x1 = 0, y1 = 0;
    for (int i = 0; i < 4; i++) {
        x0 = fminf(x0, xy[2 * i]); x1 = fmaxf(x1, xy[2 * i]);
        y0 = fminf(y0, xy[2 * i + 1]); y1 = fmaxf(y1, xy[2 * i + 1]);
    }
    uint8_t c[3];
    for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(rgb[i] * q->shade, 0, 255);
    for (int y = (int)fmaxf(0, floorf(y0)); y < (int)fminf(H, ceilf(y1)); y++)
        for (int x = (int)fmaxf(0, floorf(x0)); x < (int)fminf(W, ceilf(x1)); x++) {
            float a = pzt_cover(xy, 4, (float)x, (float)y);
            if (a > 0) px_over(o + 4 * ((size_t)y * W + x), c, a);
        }
}

/* a cube into a clear picture size x size (thumbnails: no SDL), seen from the rest view, a layer turned by deg */
static void paint_cube(const tcb_cube *c, int size, uint8_t *rgba, int axis, int layer, float deg)
{
    static const float I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    cam k;
    cam_make(&k, I, size / 2.0f, size * 0.47f, size * 0.29f);
    quad q[MAXQ];
    int nq = cube_quads(&k, c, axis, layer, deg, 1, q);
    for (int i = 0; i < nq; i++)
        fill_quad(rgba, size, size, &q[i], q[i].kind == Q_BODY ? BODY : COL[q[i].col], q[i].kind == Q_BODY ? 1.0f : 0.84f);
}

/* ---- the game's state on the screen ------------------------------------------------------------------------------ */
static tcb_game G;
static float view[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};   /* the cube's turn as seen, at rest */
static struct { int on; tcb_move m; tcb_cube from; Uint32 t0; } turn;
static struct { int on, axis; float deg; float v0[9]; Uint32 t0; } spin;   /* the whole cube turning (the view) */
static struct { int on; Uint32 t0; } win;
static int symbols = -1, sel = -1, down_tool = -1;
static struct { int on, sticker, done; float x0, y0; } fing;
static Uint32 t_first, t_spent;                 /* the clock: the first move's time, the time spent once solved */
static box last_box;
static float ccx, ccy, cs;                      /* the cube's middle, its scale */
static int made_s = -1, made_small = -1;
static SDL_Texture *t_sticker, *t_sym[6], *t_small, *t_flag;

static int solved(void) { return tcb_solved(&G.cube); }

static void load_symbols(void)
{
    if (symbols >= 0) return;
    uint8_t b = 0;
    symbols = ui_save_get("twistycube/symbols", &b, 1) == 1 && b;
}

static void small_pic(int size, uint8_t *rgba)  /* a small solved cube (the token, the bar) */
{
    tcb_cube c;
    tcb_init(&c, 3);
    paint_cube(&c, size, rgba, -1, -1, 0);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    small_pic(size, rgba);
}

static void layout(box b)
{
    last_box = b;
    load_symbols();
    float side = fminf(b.w, b.h);
    cs = floorf(side * 0.95f / 3.45f);
    ccx = b.x + b.w / 2;
    ccy = b.y + b.h / 2 + 0.02f * side;
    int S = (int)fmaxf(16, 2 * cs / (G.n > 0 ? G.n : 3) * 1.15f);
    if (made_s != S) {
        made_s = S;
        ui_free(&t_sticker);
        uint8_t *o = ui_buf(S, S);
        paint_sticker(S, o);
        t_sticker = ui_tex(S, S, o);
        for (int k = 0; k < 6; k++) {
            ui_free(&t_sym[k]);
            int s2 = (int)fmaxf(8, S * 0.5f);
            o = ui_buf(s2, s2);
            paint_symbol(k, s2, o);
            t_sym[k] = ui_tex(s2, s2, o);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 4) {
        made_small = small;
        ui_free(&t_small);
        ui_free(&t_flag);
        uint8_t *o = ui_buf(small, small);
        memset(o, 0, (size_t)small * small * 4);
        small_pic(small, o);
        t_small = ui_tex(small, small, o);
        o = ui_buf(small, small);
        memset(o, 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, o);
        static const uint8_t cream[3] = {236, 222, 190};
        for (int i = 0; i < small * small; i++)
            for (int j = 0; j < 3; j++) o[4 * i + j] = (uint8_t)(o[4 * i + j] * cream[j] / 255);
        t_flag = ui_tex(small, small, o);
    }
    ui_tools_shown(G.full ? 1 : BTNS);
}

static void calm(void)
{
    turn.on = win.on = 0;
    if (spin.on) { spin.on = 0; }
    sel = -1;
    fing.on = 0;
    down_tool = -1;
}

static void begin(void)
{
    calm();
    static const float I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    memcpy(view, I, sizeof view);
    t_first = t_spent = 0;
    made_s = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!tcb_level(&G, level)) tcb_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    tcb_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn_of(void) { return 0; }
static int score(void) { return tcb_moves(&G); }
static int par(void) { return G.full ? 0 : G.par; }
static int level_stars(void)
{
    int s = ui_par_stars(tcb_moves(&G), par());
    return G.hints && s > 2 ? 2 : s;
}

static int busy(void)
{
    Uint32 now = ui_ms();
    return (turn.on && now - turn.t0 < TURN_MS) || (spin.on && now - spin.t0 < VIEW_MS) ||
           (win.on && (int)(now - win.t0) < WIN_MS);
}

/* the view turned at rest: its numbers rounded (whole quarter turns) */
static void view_round(float *m)
{
    for (int i = 0; i < 9; i++) m[i] = roundf(m[i]);
}

static void finish_anims(void)
{
    if (spin.on) {
        float R[9];
        mat_rot(spin.axis, spin.deg, R);
        mat_mul(R, spin.v0, view);
        view_round(view);
        spin.on = 0;
    }
    turn.on = 0;
}

static void play(tcb_move m, int hint)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    finish_anims();
    turn.from = G.cube;
    if (!tcb_play(&G, m)) { ui_sound(SND_DENY); return; }
    if (hint) G.hints++;
    turn.on = 1;
    turn.m = m;
    turn.t0 = ui_ms();
    sel = -1;
    Uint32 now = ui_ms();
    if (!t_first) t_first = now ? now : 1;
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, TURN_MS - 30);
    if (solved()) {
        t_spent = now - t_first;
        win.on = 1;
        win.t0 = now + TURN_MS;
        ui_sound_later(SND_END, TURN_MS + 150);
    }
    ui_animate();
}

static void spin_view(int axis, int sg)
{
    finish_anims();
    spin.on = 1;
    spin.axis = axis;
    spin.deg = 90.0f * (float)sg;
    memcpy(spin.v0, view, sizeof view);
    spin.t0 = ui_ms();
    sel = -1;
    ui_sound(SND_TAP);
    ui_animate();
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_LEFT) spin_view(1, -1);
    else if (k == SDLK_RIGHT) spin_view(1, 1);
    else if (k == SDLK_UP) spin_view(0, -1);
    else if (k == SDLK_DOWN) spin_view(0, 1);
}

static void hint(void)
{
    tcb_move m;
    if (G.full || !tcb_hint(&G, &m)) { ui_sound(SND_DENY); return; }
    play(m, 1);
}

static int undo(void)
{
    finish_anims();
    win.on = 0;
    if (G.nh <= 0) return 0;
    tcb_move m = tcb_inverse(G.hist[G.nh - 1]);
    turn.from = G.cube;
    tcb_undo(&G);
    turn.on = 1;
    turn.m = m;
    turn.t0 = ui_ms();
    sel = -1;
    t_spent = 0;
    ui_animate();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = tcb_script(&G, moves);
    if (tcb_moves(&G) && !t_first) t_first = ui_ms() ? ui_ms() : 1;
    if (solved()) {
        t_spent = ui_ms() - t_first;
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ------------------------------------------------------------------------------------------------------- */
static void current_view(float *v, Uint32 now)
{
    memcpy(v, view, 9 * sizeof *v);
    if (spin.on) {
        float t = (now - spin.t0) / (float)VIEW_MS;
        if (t >= 1) finish_anims(), memcpy(v, view, 9 * sizeof *v);
        else {
            float R[9];
            mat_rot(spin.axis, spin.deg * puz_ease(t), R);
            mat_mul(R, spin.v0, v);
            ui_animate();
        }
    }
    if (win.on) {                               /* solved: one turn round the upright axis */
        float t = ((int)(now - win.t0)) / (float)WIN_MS;
        if (t > 0 && t < 1) {
            float R[9];
            mat_rot(1, 360 * puz_ease(t), R);
            mat_mul(R, v, v);
        }
        if (t < 1) ui_animate();
    }
}

static void put_quad(SDL_Texture *t, const quad *q, const uint8_t rgb[3], float shade, Uint8 alpha, float shrink)
{
    float mx = 0, my = 0;
    for (int i = 0; i < 4; i++) { mx += q->x[i] / 4; my += q->y[i] / 4; }
    SDL_Color c = {(Uint8)clampf(rgb[0] * shade, 0, 255), (Uint8)clampf(rgb[1] * shade, 0, 255),
                   (Uint8)clampf(rgb[2] * shade, 0, 255), alpha};
    static const float U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1};
    SDL_Vertex v[4];
    for (int i = 0; i < 4; i++)
        v[i] = (SDL_Vertex){{mx + (q->x[i] - mx) * shrink, my + (q->y[i] - my) * shrink}, c, {U[i], V[i]}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(ui.r, t, v, 4, idx, 6);
}

static void draw(void)
{
    if (made_s < 0) layout(last_box);
    Uint32 now = ui_ms();
    float v[9];
    current_view(v, now);
    cam k;
    cam_make(&k, v, ccx, ccy, cs);
    /* the shadow on the felt */
    ui_shadow_flat(ccx + 0.12f * cs, ccy + 1.55f * cs, 3.0f * cs, 0.9f * cs, 0.55f);
    const tcb_cube *c = &G.cube;
    int axis = -1, layer = -1;
    float deg = 0;
    if (turn.on) {
        float t = (now - turn.t0) / (float)TURN_MS;
        if (t >= 1) turn.on = 0;
        else {
            c = &turn.from;
            axis = tcb_axis(turn.m);
            layer = tcb_layer(turn.m);
            deg = (tcb_back(turn.m) ? -90.0f : 90.0f) * puz_ease(t);
            if (deg == 0) deg = 0.001f;
            ui_animate();
        }
    }
    if (win.on) {                               /* the gold light behind it */
        float t = ((int)(now - win.t0)) / (float)WIN_MS;
        float a = t < 0 ? 0 : puz_pulse(t * WIN_MS, 0, WIN_MS * 0.9f);
        if (t >= 1) a = 0.55f;
        if (a > 0) ui_mark_gold_a(ccx, ccy, 4.6f * cs, a);
    }
    quad q[MAXQ];
    int nq = cube_quads(&k, c, axis, layer, deg, 0.94f, q);
    for (int i = 0; i < nq; i++) {
        if (q[i].kind == Q_BODY) {
            put_quad(NULL, &q[i], BODY, q[i].shade, 255, 1.0f);
            continue;
        }
        put_quad(t_sticker, &q[i], COL[q[i].col], q[i].shade, 255, 1.0f);
        if (symbols) {
            const uint8_t *cc = COL[q[i].col];
            uint8_t dark[3] = {(uint8_t)(cc[0] * 0.42f), (uint8_t)(cc[1] * 0.42f), (uint8_t)(cc[2] * 0.42f)};
            put_quad(t_sym[q[i].col], &q[i], dark, q[i].shade, 215, 0.5f);
        }
        if (q[i].sticker == sel && !turn.on) {
            float mx = (q[i].x[0] + q[i].x[2]) / 2, my = (q[i].y[0] + q[i].y[2]) / 2;
            ui_mark_gold_a(mx, my, 2.2f * cs / G.n, 0.75f + 0.2f * sinf(now / 240.0f));
            ui_animate();
        }
    }
    ui_tool(B_EYE, ICON_EYE, symbols ? UI_ON : 0);
    if (!G.full) ui_tool(B_HINT, ICON_HINT, solved() ? UI_DIM : 0);
}

/* ---- the pointer --------------------------------------------------------------------------------------------------- */
static void rest_cam(cam *k) { cam_make(k, view, ccx, ccy, cs); }

static int inside(const quad *q, float x, float y)
{
    int pos = 0, neg = 0;
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        float cr = (q->x[j] - q->x[i]) * (y - q->y[i]) - (q->y[j] - q->y[i]) * (x - q->x[i]);
        if (cr > 0) pos++;
        else if (cr < 0) neg++;
    }
    return !pos || !neg;
}

static int pick(float x, float y)               /* the sticker under the finger, at rest; -1 none */
{
    cam k;
    rest_cam(&k);
    quad q[MAXQ];
    int nq = cube_quads(&k, &G.cube, -1, -1, 0, 1, q);
    for (int i = nq - 1; i >= 0; i--)
        if (q[i].kind == Q_STICKER && inside(&q[i], x, y)) return q[i].sticker;
    return -1;
}

static void cube_to_screen(const cam *k, const float *p, float *sx, float *sy)
{
    float w[3];
    mat_apply(k->m, p, w);
    cam_proj(k, w, sx, sy, NULL);
}

static void sticker_mid(int s, float *ctr, int *na)
{
    int p[3], nr[3], n = G.n;
    tcb_place(n, s, p, nr);
    *na = nr[0] ? 0 : nr[1] ? 1 : 2;
    for (int i = 0; i < 3; i++) ctr[i] = (float)p[i] / n;
    ctr[*na] = (float)nr[*na];
}

/* a swipe (dx, dy) from sticker s: the layer move whose motion there on the screen goes most that way */
static void swipe_sticker(int s, float dx, float dy)
{
    cam k;
    rest_cam(&k);
    float ctr[3], len = hypotf(dx, dy), best = -2, x0, y0;
    int na, ba = -1, bs = 1;
    sticker_mid(s, ctr, &na);
    cube_to_screen(&k, ctr, &x0, &y0);
    for (int a = 0; a < 3; a++) {
        if (a == na) continue;
        for (int sg = -1; sg <= 1; sg += 2) {
            float e[3] = {0, 0, 0}, vel[3], p2[3], x1, y1;
            e[a] = (float)sg;
            vel[0] = e[1] * ctr[2] - e[2] * ctr[1];
            vel[1] = e[2] * ctr[0] - e[0] * ctr[2];
            vel[2] = e[0] * ctr[1] - e[1] * ctr[0];
            for (int i = 0; i < 3; i++) p2[i] = ctr[i] + 0.05f * vel[i];
            cube_to_screen(&k, p2, &x1, &y1);
            float vx = x1 - x0, vy = y1 - y0, vl = hypotf(vx, vy);
            if (vl < 1e-4f) continue;
            float d = (vx * dx + vy * dy) / (vl * len);
            if (d > best) { best = d; ba = a; bs = sg; }
        }
    }
    if (ba < 0 || best < 0.35f) { ui_sound(SND_DENY); return; }
    play(tcb_mv(ba, tcb_layer_of(G.n, s, ba), bs < 0), 0);
}

/* a swipe on the table: the whole cube turned (the view). Across: round the upright axis; up or down: round the level
 * axis that moves the cube's front on the finger's side most that way */
static void swipe_table(float x, float dx, float dy)
{
    if (fabsf(dx) > fabsf(dy)) { spin_view(1, dx > 0 ? 1 : -1); return; }
    float T[9], Tt[9], qc[3] = {clampf((x - ccx) / cs, -1, 1), 0, 1.4f}, q[3];
    tilt(T);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) Tt[3 * i + j] = T[3 * j + i];
    mat_apply(Tt, qc, q);
    cam k;
    static const float I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    cam_make(&k, I, ccx, ccy, cs);              /* world coordinates: the view itself left out */
    float x0, y0, best = -2, len = hypotf(dx, dy);
    int ba = -1, bs = 1;
    cube_to_screen(&k, q, &x0, &y0);
    for (int a = 0; a < 3; a += 2)
        for (int sg = -1; sg <= 1; sg += 2) {
            float e[3] = {0, 0, 0}, vel[3], p2[3], x1, y1;
            e[a] = (float)sg;
            vel[0] = e[1] * q[2] - e[2] * q[1];
            vel[1] = e[2] * q[0] - e[0] * q[2];
            vel[2] = e[0] * q[1] - e[1] * q[0];
            for (int i = 0; i < 3; i++) p2[i] = q[i] + 0.05f * vel[i];
            cube_to_screen(&k, p2, &x1, &y1);
            float vx = x1 - x0, vy = y1 - y0, vl = hypotf(vx, vy);
            if (vl < 1e-4f) continue;
            float d = (vx * dx + vy * dy) / (vl * len);
            if (d > best) { best = d; ba = a; bs = sg; }
        }
    if (ba >= 0) spin_view(ba, bs);
}

static void press(float x, float y)
{
    fing.on = 0;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { down_tool = t; ui_animate(); return; }
    if (solved() || win.on) return;
    fing.on = 1;
    fing.done = 0;
    fing.x0 = x;
    fing.y0 = y;
    fing.sticker = pick(x, y);
}

static void motion(float x, float y)
{
    if (!fing.on || fing.done) return;
    float dx = x - fing.x0, dy = y - fing.y0, th = fmaxf(0.35f * ui.unit, 0.3f * cs / G.n);
    if (hypotf(dx, dy) < th) return;
    fing.done = 1;
    if (fing.sticker >= 0) swipe_sticker(fing.sticker, dx, dy);
    else swipe_table(fing.x0, dx, dy);
}

static void release(float x, float y)
{
    if (down_tool >= 0) {
        int t = down_tool;
        down_tool = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t) return;
        if (t == B_EYE) {
            symbols = !symbols;
            uint8_t b = (uint8_t)symbols;
            ui_save_set("twistycube/symbols", &b, 1);
            ui_sound(SND_TAP);
        } else if (t == B_HINT) hint();
        return;
    }
    if (!fing.on) return;
    motion(x, y);
    fing.on = 0;
    if (fing.done) return;
    /* a tap: choose a sticker, then tap where it goes */
    int s = fing.sticker;
    if (s < 0) { sel = -1; ui_animate(); return; }
    if (sel < 0 || s == sel) {
        sel = s == sel ? -1 : s;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    cam k;
    rest_cam(&k);
    float a[3], sx0, sy0;
    int na;
    sticker_mid(sel, a, &na);
    cube_to_screen(&k, a, &sx0, &sy0);
    int from = sel;
    sel = -1;
    swipe_sticker(from, x - sx0, y - sy0);
}

/* the bar: the moves, the clock (minutes:seconds), the par by its flag */
static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, dh = 0.46f * u, cy = b.y + b.h / 2;
    float after = ui_bar_number(b, NULL, tcb_moves(&G));
    float right = b.x + b.w;
    if (par() > 0) {
        float w = ui_number_w(par(), dh), sz = 0.6f * u;
        int glow = solved() && tcb_moves(&G) <= par();
        if (glow) ui_mark_gold(right - w - 0.12f * u - sz / 2, cy, 2.2f * sz);
        if (t_flag) ui_put(t_flag, right - w - 0.12f * u - sz, cy - sz / 2, sz, sz);
        ui_number(right - w / 2, cy, dh, par(), 1);
        right -= w + 0.12f * u + sz + 0.3f * u;
    }
    Uint32 ms = solved() ? t_spent : t_first ? ui_ms() - t_first : 0;
    int sec = (int)(ms / 1000), mn = sec / 60;
    sec %= 60;
    if (mn > 99) mn = 99;
    static const uint8_t cream[3] = {236, 222, 190};
    float wm = ui_number_w(mn, dh), wd = ui_number_w(0, dh), gap = 0.16f * u, total = wm + gap + 2 * wd;
    float mid = (after + right) / 2, x = mid - total / 2;
    if (x < after + 0.25f * u || x + total > right) return;   /* no room: the clock left out */
    ui_number_ex(x + wm / 2, cy, dh, mn, cream, 230, 0);
    float dot = fmaxf(2, 0.07f * u), xc = x + wm + gap / 2;
    ui_fill(xc - dot / 2, cy - 0.14f * u - dot / 2, dot, dot, cream[0], cream[1], cream[2], 230);
    ui_fill(xc - dot / 2, cy + 0.12f * u - dot / 2, dot, dot, cream[0], cream[1], cream[2], 230);
    ui_number_ex(x + wm + gap + wd / 2, cy, dh, sec / 10, cream, 230, 0);
    ui_number_ex(x + wm + gap + 1.5f * wd, cy, dh, sec % 10, cream, 230, 0);
}

/* ---- the shell's pictures ----------------------------------------------------------------------------------------- */
static void scrambled(tcb_cube *c, int n, int depth, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    tcb_move m[TCB_MAXSCR];
    int k = tcb_scramble(&r, n, depth, m);
    tcb_init(c, n);
    for (int i = 0; i < k; i++) tcb_turn(c, m[i]);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    tcb_cube c;
    if (variant < 0) {                          /* the menu: a 3 x 3 mixed, its top layer turning */
        scrambled(&c, 3, 5, 11);
        paint_cube(&c, size, rgba, 1, 2, 24);
        return;
    }
    int n = variant + 2;
    scrambled(&c, n, n + 3, 40u + (uint32_t)variant);
    paint_cube(&c, size, rgba, 1, n - 1, 18);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    tcb_game g;
    if (tcb_level(&g, level)) paint_cube(&g.cube, size, rgba, -1, -1, 0);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)   /* a 3 x 3 more and more mixed */
{
    static const int DEPTH[TCB_CHAPTERS] = {1, 2, 4, 6, 9, 26};
    tcb_cube c;
    if (chapter < 0 || chapter >= TCB_CHAPTERS) return;
    scrambled(&c, 3, DEPTH[chapter], 300u + (uint32_t)chapter);
    paint_cube(&c, size, rgba, -1, -1, 0);
}

static const game twistycube_game = {
    .id = "twistycube", .order = 470, .variants = TCB_VARIANTS, .variant_first = 1,   /* (random play: 3 x 3) */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn_of, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = TCB_LEVELS, .chapters = TCB_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(twistycube_game)
