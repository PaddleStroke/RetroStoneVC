/*
 * Pool in RetroStone Board: the table in its walnut rails (along the screen's long side), the balls rolling as the
 * rules' physics runs them (cuetable.h), the aiming line to the first contact. Aim: pull back from the cue ball and
 * let go (the farther, the harder), or tap where to aim and tap the power gauge. With the cue ball in hand: drag it,
 * or tap it then tap its new place. The rules: pool_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The moves' notation (script, the bots): "angle:power" or "angle:power@x,y" (pool.h), shots separated by spaces.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "games/cuetable_ui.h"
#include "games/pool.h"
#include "shell.h"

#define CUSH 1.9f                               /* the cushions' band, units */
#define RAIL 3.9f                               /* the wooden rail outside it */
#define FR (CUSH + RAIL)
#define OW (POOL_W + 2 * FR)
#define OH (POOL_H + 2 * FR)
#define PULL_FULL 26.0f                         /* a pull this long (units): full power */
#define INTRO_MS 650                            /* a bot's shot: the cue ball set, the cue drawn back, then struck */

static pool_game G;
static cue_table T;
static cue_view V;
static cue_gauge Gg;
static cue_replay R;
static cue_world before;                        /* the table as the shot started (drawn under the replay) */
static SDL_Texture *t_table, *t_ball[16], *t_cue;
static int made_s = -1, made_rot = -1, made_d = -1;

enum { F_NONE, F_AIM, F_PULL, F_GAUGE, F_PLACE };
static struct {
    int mode;
    float ang;                                  /* the aim, radians (table) */
    float power, pull;                          /* the gauge's power; the pull shown while pulling */
    float px, py;                               /* the cue ball's place in hand */
    int lifted, moved;
    float x0, y0;                               /* the finger's start (pixels) */
    int aimed;                                  /* the aim set for this turn */
} A;
static struct { int on; Uint32 t0; int ang, pow, placed; float x, y, fx, fy; } intro;

static const pool_pos *P(void) { return pool_now(&G); }

/* ---- the colours of the balls: the house's lacquers ------------------------------------------------------------ */
static const uint8_t COLOUR[8][3] = {
    {246, 238, 220}, {222, 168, 40}, {38, 72, 160}, {184, 42, 36}, {96, 52, 128}, {222, 112, 34}, {34, 118, 72},
    {118, 38, 32},
};
static const uint8_t EBONY[3] = {28, 24, 22}, IVORY[3] = {246, 238, 220};

/* ball n (0 the cue ball) seen from above, d x d: lit from the upper left, its number's ivory disc */
static void paint_ball(int n, int d, uint8_t *o)
{
    const uint8_t *base = n == 0 ? IVORY : n == 8 ? EBONY : COLOUR[n > 8 ? n - 8 : n];
    int stripe = n > 8;
    float r = d / 2.0f;
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float nx = (x + 0.5f - r) / r, ny = (y + 0.5f - r) / r, q = nx * nx + ny * ny;
            uint8_t *p = o + 4 * (y * d + x);
            float a = clampf((1 - sqrtf(q)) * r + 0.5f, 0, 1);
            if (a <= 0) { memset(p, 0, 4); continue; }
            float nz = sqrtf(fmaxf(0, 1 - q));
            const uint8_t *c = base;
            if (stripe && fabsf(ny) > 0.5f) c = IVORY;
            if (n > 0 && nx * nx + ny * ny < 0.2f) c = IVORY;   /* the number's disc */
            float lam = fmaxf(0, -0.45f * nx - 0.55f * ny + 0.70f * nz), spec = powf(fmaxf(0, lam), 28);
            float k = 0.38f + 0.72f * lam;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(c[i] * k + 255 * 0.55f * spec, 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
}

/* the cue: a long taper of maple, an ivory ferrule and a dark tip, a walnut butt; len x th pixels, the tip at the
 * right */
static void paint_cue(int len, int th, uint8_t *o)
{
    memset(o, 0, (size_t)len * th * 4);
    for (int x = 0; x < len; x++) {
        float f = (float)x / len, half = th * (0.5f - 0.22f * f);
        const float *c;
        static const float tip[3] = {60, 70, 90}, fer[3] = {240, 232, 214}, maple[3] = {220, 186, 132},
                           butt[3] = {92, 54, 30}, ring[3] = {208, 168, 72};
        c = f > 0.985f ? tip : f > 0.955f ? fer : f > 0.40f ? maple : f > 0.385f ? ring : butt;
        for (int y = 0; y < th; y++) {
            float dy = y + 0.5f - th / 2.0f, a = clampf(half - fabsf(dy) + 0.5f, 0, 1);
            if (a <= 0) continue;
            float lit = 0.75f + 0.35f * (1 - fabsf(dy / fmaxf(half, 1))) - 0.2f * (dy > 0);
            uint8_t *p = o + 4 * (y * len + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(c[i] * lit, 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
    }
}

/* a pocket: a dark hole with a soft rim, at (cx, cy) pixels, radius r */
static void hole(uint8_t *o, int w, int h, float cx, float cy, float r)
{
    for (int y = (int)(cy - r - 2); y <= (int)(cy + r + 2); y++)
        for (int x = (int)(cx - r - 2); x <= (int)(cx + r + 2); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy), a = clampf(r - d + 0.5f, 0, 1);
            if (a <= 0) continue;
            float k = 0.05f + 0.10f * clampf((dx + dy) / (2 * r) + 0.5f, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * (1 - a) + 40 * k * a);
        }
}

/* the table w x h pixels at s pixels a unit, in its own way (along x) */
static void paint_table(const cue_table *tb, float s, int w, int h, uint8_t *o)
{
    int fr = (int)fmaxf(2, 0.9f * s);
    art_plank(w - 2 * fr, h - 2 * fr, fr, o);
    float ix = FR * s, iy = FR * s;             /* the felt's playing area */
    int c0 = (int)(RAIL * s);
    art_felt(o, w, h, c0, c0, w - c0, h - c0);
    for (int y = c0; y < h - c0; y++)           /* the cushions, a darker band but at the mouths */
        for (int x = c0; x < w - c0; x++) {
            float tx = (x + 0.5f - ix) / s, ty = (y + 0.5f - iy) / s;
            int in = tx >= 0 && tx <= POOL_W && ty >= 0 && ty <= POOL_H;
            if (in) continue;
            int edge = ty < 0 ? CUE_TOP : ty > POOL_H ? CUE_BOTTOM : tx < 0 ? CUE_LEFT : CUE_RIGHT;
            float along = edge <= CUE_BOTTOM ? tx : ty, depth = edge == CUE_TOP ? -ty : edge == CUE_BOTTOM ?
                          ty - POOL_H : edge == CUE_LEFT ? -tx : tx - POOL_W;
            if (cue_in_mouth(tb, edge, along) || ((tx < 0 || tx > POOL_W) && (ty < 0 || ty > POOL_H))) continue;
            /* the cushion's ends cut at a slant towards the pocket */
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float k = depth < 0.25f ? 1.18f : 0.74f;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
        }
    static const float PK[6][3] = {{-0.5f, -0.5f, 2.7f}, {50, -1.2f, 2.4f}, {100.5f, -0.5f, 2.7f},
                                   {-0.5f, 50.5f, 2.7f}, {50, 51.2f, 2.4f}, {100.5f, 50.5f, 2.7f}};
    for (int k = 0; k < 6; k++) hole(o, w, h, ix + PK[k][0] * s, iy + PK[k][1] * s, PK[k][2] * s);
    static const uint8_t gold[3] = {226, 196, 120};
    float dr = fmaxf(1.2f, 0.42f * s), mid = (CUSH + RAIL * 0.5f) * s;
    for (int k = 1; k < 8; k++) {               /* the diamonds on the rails */
        if (k == 4) continue;
        float x = ix + k * 12.5f * s;
        art_roundrect_put(o, w, h, (int)(x - dr), (int)(iy - mid - dr), (int)(2 * dr), (int)(2 * dr), dr, gold, 0.9f);
        art_roundrect_put(o, w, h, (int)(x - dr), (int)(iy + POOL_H * s + mid - dr), (int)(2 * dr), (int)(2 * dr), dr,
                          gold, 0.9f);
    }
    for (int k = 1; k < 4; k++) {
        float y = iy + k * 12.5f * s;
        art_roundrect_put(o, w, h, (int)(ix - mid - dr), (int)(y - dr), (int)(2 * dr), (int)(2 * dr), dr, gold, 0.9f);
        art_roundrect_put(o, w, h, (int)(ix + POOL_W * s + mid - dr), (int)(y - dr), (int)(2 * dr), (int)(2 * dr), dr,
                          gold, 0.9f);
    }
    static const uint8_t chalk[3] = {226, 236, 222};   /* the head string and the foot spot, faint */
    art_tint_rect(o, w, h, ix + POOL_HEAD * s - 0.5f, iy, ix + POOL_HEAD * s + 0.5f, iy + POOL_H * s, chalk, 0.16f);
    float fs = fmaxf(1, 0.35f * s);
    art_roundrect_put(o, w, h, (int)(ix + POOL_FOOT_X * s - fs), (int)(iy + POOL_H / 2 * s - fs), (int)(2 * fs),
                      (int)(2 * fs), fs, chalk, 0.4f);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static int variant;

static void aim_default(void)
{
    const pool_pos *p = P();
    const cue_ball *c = &p->w.b[0];
    uint32_t m = pool_targets(p);
    float best = 1e30f;
    A.ang = 0;
    for (int i = 1; i < p->w.n; i++) {          /* the nearest ball it may strike */
        if (!(m >> i & 1)) continue;
        float d = hypotf(p->w.b[i].x - c->x, p->w.b[i].y - c->y);
        if (d < best) { best = d; A.ang = atan2f(p->w.b[i].y - c->y, p->w.b[i].x - c->x); }
    }
    A.px = c->x;
    A.py = c->y;
    A.lifted = 0;
    A.aimed = 0;
}

static void start(int v)
{
    variant = v;
    pool_table(&T);
    pool_game_new(&G, v, (uint64_t)ui_random(1 << 30) * 2654435761u + 1);
    R.on = intro.on = 0;
    A.mode = F_NONE;
    A.power = 0.6f;
    aim_default();
}

static int turn(void) { return P()->turn; }

static int status(void)
{
    int s = P()->status;
    return s == POOL_WIN0 ? GS_WIN0 : s == POOL_WIN1 ? GS_WIN1 : GS_PLAY;
}

static int busy(void) { return R.on || intro.on; }

/* plays a shot: the rules at once, the screen runs the same steps */
static int shoot(int ang, int pow, int placed, float x, float y, int animate)
{
    const pool_pos *p = P();
    cue_world w;
    if (!pool_ready(p, ang, pow, placed, x, y, &w)) return 0;
    before = p->w;
    if (placed) { before.b[0].x = x; before.b[0].y = y; before.b[0].down = -1; }
    if (!pool_play(&G, ang, pow, placed, x, y, NULL)) return 0;
    if (animate) {
        cue_replay_start(&R, &T, &w);
        ui_sound(SND_MOVE);
    }
    A.mode = F_NONE;
    aim_default();
    return 1;
}

static int undo(void)
{
    if (!pool_undo(&G)) return 0;
    R.on = intro.on = 0;
    A.mode = F_NONE;
    aim_default();
    return 1;
}

static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int ang, pow, placed, n;
        float x, y;
        if (!pool_parse(s, &ang, &pow, &placed, &x, &y, &n) || !shoot(ang, pow, placed, x, y, 0)) return 0;
        s += n;
    }
    return 1;
}

const pool_game *pool_live(void) { return &G; }

/* a bot's shot: the cue ball glides to its place, the cue draws back, then the shot (as a person's) */
int pool_show(const char *move)
{
    int ang, pow, placed, n;
    float x, y;
    if (busy() || !pool_parse(move, &ang, &pow, &placed, &x, &y, &n)) return 0;
    cue_world w;
    if (!pool_ready(P(), ang, pow, placed, x, y, &w)) return 0;
    intro.on = 1;
    intro.t0 = ui_ms();
    intro.ang = ang;
    intro.pow = pow;
    intro.placed = placed;
    intro.x = x;
    intro.y = y;
    intro.fx = P()->w.b[0].x;
    intro.fy = P()->w.b[0].y;
    A.mode = F_NONE;
    return 1;
}

static void intro_tick(void)
{
    if (!intro.on) return;
    ui_animate();
    if (ui_ms() - intro.t0 < INTRO_MS) return;
    intro.on = 0;
    if (shoot(intro.ang, intro.pow, intro.placed, intro.x, intro.y, 1)) R.t0 = intro.t0 + INTRO_MS;   /* on time */
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    cue_view v = {.tw = POOL_W, .th = POOL_H};
    cue_fit(b, OW, OH, 1, ui.flipped ? 2 : 0, &v, &Gg);
    V = v;
    int d = (int)ceilf(2 * POOL_R * V.s);
    if (made_s != (int)(V.s * 8) || made_rot != V.rot) {
        made_s = (int)(V.s * 8);
        made_rot = V.rot;
        int w = (int)lroundf(OW * V.s), h = (int)lroundf(OH * V.s);
        uint8_t *src = malloc((size_t)w * h * 4);
        if (src) {
            paint_table(&T, V.s, w, h, src);
            int ow = V.rot & 1 ? h : w, oh = V.rot & 1 ? w : h;
            uint8_t *out = ui_buf(ow, oh);
            cue_turn_pic(src, w, h, V.rot, out);
            ui_free(&t_table);
            t_table = ui_tex(ow, oh, out);
            free(src);
        }
    }
    if (made_d != d) {
        made_d = d;
        for (int n = 0; n < 16; n++) {
            ui_free(&t_ball[n]);
            paint_ball(n, d, ui_buf(d, d));
            t_ball[n] = ui_tex(d, d, ui_buf(d, d));
        }
        int len = (int)(42 * V.s), th = (int)fmaxf(4, 0.95f * V.s);
        ui_free(&t_cue);
        paint_cue(len, th, ui_buf(len, th));
        t_cue = ui_tex(len, th, ui_buf(len, th));
    }
}

static void ball_at(int n, float x, float y, float k, float alpha)
{
    float sx, sy, d = 2 * POOL_R * V.s * k;
    cue_to_screen(&V, x, y, &sx, &sy);
    ui_shadow_flat(sx, sy, d * 1.05f, d * 1.05f, 0.8f * alpha);
    SDL_SetTextureAlphaMod(t_ball[n], (Uint8)(255 * alpha));
    ui_put_c(t_ball[n], sx, sy, d, d);
    if (n > 0 && d >= 13) {
        static const uint8_t ink[3] = {40, 30, 24};
        ui_number_ex(sx, sy, 0.36f * d, n, ink, (Uint8)(255 * alpha), UI_NUM_TIGHT);
    }
}

static int good_first(int j) { return (int)(pool_targets(P()) >> j & 1); }

static void draw_cue(float x, float y, float ang, float pull, float alpha)
{
    float len = 42 * V.s, off = (POOL_R + 0.6f + pull * 9) * V.s, sx, sy;
    cue_to_screen(&V, x, y, &sx, &sy);
    double deg = cue_screen_deg(&V, ang * 180 / CUE_PI);
    float a = (float)(deg * CUE_PI / 180), cx = sx - cosf(a) * (off + len / 2), cy = sy - sinf(a) * (off + len / 2);
    SDL_SetTextureAlphaMod(t_cue, (Uint8)(255 * alpha));
    ui_shadow_flat(cx + 0.4f * V.s, cy + 0.6f * V.s, len * 0.9f, fmaxf(4, 1.4f * V.s), 0.35f * alpha);
    ui_put_rot(t_cue, cx, cy, len, fmaxf(4, 0.95f * V.s), deg);
}

static int human_turn(void) { return !(ui_bot_seats() >> P()->turn & 1); }

static void draw(void)
{
    intro_tick();
    cue_replay_tick(&R, 120);
    const pool_pos *p = P();
    float tw = (V.rot & 1 ? OH : OW) * V.s, th = (V.rot & 1 ? OW : OH) * V.s;
    ui_put_c(t_table, V.cx, V.cy, tw, th);
    if (R.on) {
        for (int i = 0; i < R.w.n; i++) {
            float x, y, k;
            const cue_ball *b = &R.w.b[i];
            if (b->down < 0) ball_at(i, b->x, b->y, 1, 1);
            else if (cue_replay_falling(&R, i, T.px[b->down], T.py[b->down], &x, &y, &k)) ball_at(i, x, y, k, k);
        }
        return;
    }
    int playing = p->status == POOL_PLAY;
    float cx = p->w.b[0].x, cy = p->w.b[0].y;
    if (intro.on) {                             /* a bot's cue ball gliding to its place */
        float f = ui_ease(clampf((ui_ms() - intro.t0) / (INTRO_MS * 0.45f), 0, 1));
        if (intro.placed) { cx = intro.fx + (intro.x - intro.fx) * f; cy = intro.fy + (intro.y - intro.fy) * f; }
    } else if (p->in_hand) { cx = A.px; cy = A.py; }
    for (int i = 1; i < p->w.n; i++)
        if (p->w.b[i].down < 0) ball_at(i, p->w.b[i].x, p->w.b[i].y, 1, 1);
    if (!playing) {
        if (p->w.b[0].down < 0) ball_at(0, cx, cy, 1, 1);
        return;
    }
    cue_world w = p->w;                         /* the aim from where the cue ball is now */
    w.b[0].x = cx;
    w.b[0].y = cy;
    w.b[0].down = -1;
    float sx, sy;
    cue_to_screen(&V, cx, cy, &sx, &sy);
    if (p->in_hand && !intro.on && human_turn()) {   /* in hand: a gold light round it (lifted: brighter) */
        int ok = pool_place_ok(p, cx, cy);
        if (ok) ui_mark_gold_a(sx, sy, 3.4f * POOL_R * V.s, A.lifted ? 1 : 0.5f + 0.2f * sinf(ui_ms() / 260.0f));
        else ui_mark_glow(sx, sy, 3.4f * POOL_R * V.s);
        ui_animate();
    }
    float ang = A.ang, pull = A.mode == F_PULL ? A.pull : 0;
    int show = !(A.mode == F_PLACE || A.lifted);
    if (intro.on) {
        float dx, dy, f = clampf((ui_ms() - intro.t0) / (float)INTRO_MS, 0, 1);
        cue_dir(intro.ang, &dx, &dy);
        ang = atan2f(dy, dx);
        pull = intro.pow / 1000.0f * sinf(clampf((f - 0.4f) / 0.6f, 0, 1) * CUE_PI * 0.5f);
        show = f > 0.4f;
    }
    if (show && (human_turn() || intro.on)) {
        cue_aim_draw(&V, &T, &w, 0, cosf(ang), sinf(ang), good_first);
        draw_cue(cx, cy, ang, pull, 1);
    }
    ball_at(0, cx, cy, A.mode == F_PLACE || A.lifted ? 1.12f : 1, 1);
    if (human_turn() && !intro.on)
        cue_gauge_draw(&Gg, A.mode == F_PULL ? A.pull : A.power, A.mode == F_NONE || A.mode == F_GAUGE);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
static void finger_table(float x, float y, float *tx, float *ty) { cue_from_screen(&V, x, y, tx, ty); }

static void fire(float power)
{
    const pool_pos *p = P();
    int pow = (int)lroundf(clampf(power, 0.01f, 1) * 1000), ang = cue_angle(cosf(A.ang), sinf(A.ang));
    float x = roundf(A.px * 100) / 100, y = roundf(A.py * 100) / 100;
    if (p->in_hand && !pool_place_ok(p, x, y)) { ui_sound(SND_DENY); return; }
    if (!shoot(ang, pow, p->in_hand, x, y, 1)) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const pool_pos *p = P();
    if (busy() || p->status != POOL_PLAY) return;
    float tx, ty, slack = 0.4f * ui.unit / V.s;
    finger_table(x, y, &tx, &ty);
    float cx = p->in_hand ? A.px : p->w.b[0].x, cy = p->in_hand ? A.py : p->w.b[0].y, d = hypotf(tx - cx, ty - cy);
    A.x0 = x;
    A.y0 = y;
    A.moved = 0;
    if (cue_gauge_hit(&Gg, x, y)) {
        A.mode = F_GAUGE;
        A.power = cue_gauge_at(&Gg, x, y);
        return;
    }
    if (p->in_hand && A.lifted) {              /* the cue ball lifted: this tap sets it down */
        if (pool_place_ok(p, tx, ty)) {
            A.px = tx;
            A.py = ty;
            A.lifted = 0;
            ui_sound(SND_PLACE);
        } else ui_sound(SND_DENY);
        A.mode = F_NONE;
        return;
    }
    if (p->in_hand && d < POOL_R + slack) { A.mode = F_PLACE; return; }
    if (d < 4 * POOL_R + slack) { A.mode = F_PULL; A.pull = 0; return; }
    if (tx < -FR || ty < -FR || tx > POOL_W + FR || ty > POOL_H + FR) { A.mode = F_NONE; return; }
    A.mode = F_AIM;
    A.ang = atan2f(ty - cy, tx - cx);
    A.aimed = 1;
}

static void motion(float x, float y)
{
    const pool_pos *p = P();
    if (A.mode == F_NONE) return;
    if (fabsf(x - A.x0) + fabsf(y - A.y0) > 0.18f * ui.unit) A.moved = 1;
    float tx, ty;
    finger_table(x, y, &tx, &ty);
    float cx = p->in_hand ? A.px : p->w.b[0].x, cy = p->in_hand ? A.py : p->w.b[0].y;
    switch (A.mode) {
    case F_GAUGE: A.power = cue_gauge_at(&Gg, x, y); break;
    case F_AIM: A.ang = atan2f(ty - cy, tx - cx); break;
    case F_PULL: {
        float dx = cx - tx, dy = cy - ty, l = hypotf(dx, dy);
        if (l > 1.2f * POOL_R) {
            A.ang = atan2f(dy, dx);
            A.pull = clampf((l - 1.2f * POOL_R) / PULL_FULL, 0, 1);
        } else A.pull = 0;
        break;
    }
    case F_PLACE:
        if (!A.moved) break;
        A.px = clampf(tx, POOL_R, p->brk ? POOL_HEAD : POOL_W - POOL_R);
        A.py = clampf(ty, POOL_R, POOL_H - POOL_R);
        break;
    }
    ui_animate();
}

static void release(float x, float y)
{
    const pool_pos *p = P();
    motion(x, y);
    int m = A.mode;
    A.mode = F_NONE;
    switch (m) {
    case F_GAUGE:
        if (A.power < 0.02f) break;
        fire(A.power);
        break;
    case F_PULL:
        if (A.pull >= 0.03f) { A.power = A.pull; fire(A.pull); }
        break;
    case F_PLACE:
        if (!A.moved) { A.lifted = 1; ui_sound(SND_TAP); break; }
        if (!pool_place_ok(p, A.px, A.py)) {   /* not a place for it: back where it was */
            ui_sound(SND_DENY);
            A.px = p->w.b[0].x;
            A.py = p->w.b[0].y;
            if (!pool_place_ok(p, A.px, A.py))
                cue_free_spot(&p->w, POOL_R, POOL_HEAD / 2, POOL_H / 2, POOL_R, POOL_R,
                              p->brk ? POOL_HEAD : POOL_W - POOL_R, POOL_H - POOL_R, 0, &A.px, &A.py);
        } else ui_sound(SND_PLACE);
        break;
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static SDL_Texture *t_small[16];
static int made_small = -1;

/* the player's bar: their group's balls still on the table (8-ball), then the 8; 9-ball: the ball to strike first
 * (the player to move) */
static void bar(int player, box b)
{
    const pool_pos *p = P();
    int d = (int)(0.5f * ui.unit);
    if (d != made_small) {
        made_small = d;
        for (int n = 0; n < 16; n++) {
            ui_free(&t_small[n]);
            paint_ball(n, d, ui_buf(d, d));
            t_small[n] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    float x = b.x + 0.1f * ui.unit, cy = b.y + b.h / 2, step = fminf(0.56f * ui.unit, (b.w - d) / 8);
    int list[16], n = 0;
    if (p->variant == POOL_NINE) {
        uint32_t m = pool_targets(p);
        if (p->turn == player)
            for (int i = 1; i < 10; i++)
                if (m >> i & 1) list[n++] = i;
    } else if (p->group[player] != POOL_OPEN) {
        for (int i = 1; i < 16; i++)
            if (p->w.b[i].down < 0 && pool_group_of(i) == p->group[player]) list[n++] = i;
        if (!n && p->w.b[8].down < 0) list[n++] = 8;
    }
    for (int k = 0; k < n; k++) {
        ui_shadow_flat(x + k * step + d / 2.0f, cy, d, d, 0.7f);
        ui_put_c(t_small[list[k]], x + k * step + d / 2.0f, cy, d, d);
    }
    if (p->turn == player && p->in_hand && p->status == POOL_PLAY && !R.on) {   /* the cue ball in hand */
        float hx = b.x + b.w - 0.5f * ui.unit;
        ui_mark_gold_a(hx, cy, 1.5f * d, 0.8f);
        ui_put_c(t_small[0], hx, cy, d, d);
    }
}

static void token(int player, int size, uint8_t *rgba) { paint_ball(player ? 8 : 0, size, rgba); }

/* the menu's card: a corner of the table, the rack (8-ball's triangle, 9-ball's diamond), the cue ball and the cue */
static void thumb(int v, int size, uint8_t *rgba)
{
    float s = size / 40.0f, x0 = 65.7f, y0 = 15;   /* the foot end: the rack, the corner pocket, the cue */
    int w = (int)ceilf(OW * s), h = (int)ceilf(OH * s);
    uint8_t *t = malloc((size_t)w * h * 4);
    if (!t) return;
    cue_table tb;
    pool_table(&tb);
    paint_table(&tb, s, w, h, t);
    int ox = (int)((FR + x0) * s), oy = (int)((FR + y0) * s);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            int sx = x + ox, sy = y + oy;
            uint8_t *q = rgba + 4 * (y * size + x);
            if (sx < w && sy < h) memcpy(q, t + 4 * ((size_t)sy * w + sx), 4);
        }
    free(t);
    pool_pos p;
    pool_new(&p, v == 1 ? POOL_NINE : POOL_EIGHT, 7);
    int d = (int)(2 * POOL_R * s);
    uint8_t *ball = malloc((size_t)d * d * 4);
    if (!ball) return;
    float bx0 = FR * s - ox, by0 = FR * s - oy;
    for (int i = 1; i < p.w.n; i++) {
        paint_ball(i, d, ball);
        art_flat_put(rgba, size, size, ball, d, bx0 + p.w.b[i].x * s, by0 + p.w.b[i].y * s);
    }
    float qx = bx0 + 70 * s, qy = by0 + 38 * s, dx = POOL_FOOT_X - 70, dy = POOL_H / 2 - 38, l = hypotf(dx, dy);
    dx /= l;
    dy /= l;
    paint_ball(0, d, ball);
    art_flat_put(rgba, size, size, ball, d, qx, qy);
    free(ball);
    int len = (int)(30 * s), th = (int)fmaxf(3, 0.95f * s);
    uint8_t *cue = malloc((size_t)len * th * 4);
    if (!cue) return;
    paint_cue(len, th, cue);                    /* behind the cue ball, its tip towards it */
    float gap = len / 2.0f + 1.6f * POOL_R * s;
    art_blend_rot(rgba, size, size, cue, len, th, qx - dx * gap, qy - dy * gap, atan2f(dy, dx) * 180 / CUE_PI);
    free(cue);
}

static const game pool_game_def = {
    .id = "pool", .order = 482, .variants = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(pool_game_def)
