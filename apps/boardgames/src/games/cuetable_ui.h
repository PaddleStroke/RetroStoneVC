/*
 * What the table games' screens share (carrom.c, pool.c): the table seen on the screen (turned a quarter when that
 * makes it bigger), a shot run step by step as it is drawn (the same steps as the rules': cuetable.h), the balls that
 * fall, the power gauge beside the table, the dotted aiming line to the first contact and the first bounce after it.
 * Every function static inline (each game its own copy). Prefix cue_. Uses the shell (SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CUETABLE_UI_H
#define CUETABLE_UI_H

#include <stdlib.h>

#include "games/cuetable.h"
#include "shell.h"

/* ---- the table on the screen: its middle at (cx, cy), s pixels a unit, turned rot quarters clockwise ------------- */
typedef struct {
    float cx, cy, s;
    int rot;
    float tw, th;                               /* the table's size (units) */
} cue_view;

static inline void cue_to_screen(const cue_view *v, float x, float y, float *sx, float *sy)
{
    float dx = (x - v->tw / 2) * v->s, dy = (y - v->th / 2) * v->s;
    switch (v->rot & 3) {
    case 0: *sx = v->cx + dx; *sy = v->cy + dy; break;
    case 1: *sx = v->cx - dy; *sy = v->cy + dx; break;
    case 2: *sx = v->cx - dx; *sy = v->cy - dy; break;
    default: *sx = v->cx + dy; *sy = v->cy - dx; break;
    }
}

static inline void cue_from_screen(const cue_view *v, float sx, float sy, float *x, float *y)
{
    float dx = (sx - v->cx) / v->s, dy = (sy - v->cy) / v->s, tx, ty;
    switch (v->rot & 3) {
    case 0: tx = dx; ty = dy; break;
    case 1: tx = dy; ty = -dx; break;
    case 2: tx = -dx; ty = -dy; break;
    default: tx = -dy; ty = dx; break;
    }
    *x = tx + v->tw / 2;
    *y = ty + v->th / 2;
}

static inline double cue_screen_deg(const cue_view *v, float table_deg) { return table_deg + 90.0 * (v->rot & 3); }

/* a picture painted in the table's own way (w x h, rows along x) turned rot quarters clockwise (w x h, or h x w) */
static inline void cue_turn_pic(const uint8_t *src, int w, int h, int rot, uint8_t *out)
{
    rot &= 3;
    int ow = rot & 1 ? h : w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int ox, oy;
            switch (rot) {
            case 0: ox = x; oy = y; break;
            case 1: ox = h - 1 - y; oy = x; break;
            case 2: ox = w - 1 - x; oy = h - 1 - y; break;
            default: ox = y; oy = w - 1 - x; break;
            }
            memcpy(out + 4 * ((size_t)oy * ow + ox), src + 4 * ((size_t)y * w + x), 4);
        }
}

/* ---- the room: the table (ow x oh units with its frame) and the gauge beside it, as big as they go ------------- */
typedef struct {
    box gauge;                                  /* the power gauge (w 0: none) */
    int vertical;                               /* the gauge stands up (beside the table), else lies under it */
} cue_gauge;

static inline void cue_fit(box room, float ow, float oh, int may_turn, int rot_base, cue_view *v, cue_gauge *g)
{
    float th = 0.62f * ui.unit, gap = 0.3f * ui.unit, best = -1;
    for (int turn = 0; turn <= (may_turn ? 1 : 0); turn++)
        for (int vert = 0; vert < 2; vert++) {
            float w = turn ? oh : ow, h = turn ? ow : oh;
            float rw = room.w - (vert ? th + 2 * gap : 0), rh = room.h - (vert ? 0 : th + 2 * gap);
            float s = fminf(rw / w, rh / h);
            if (s <= best + 0.01f) continue;
            best = s;
            v->s = floorf(s * 8) / 8;
            v->rot = (rot_base + 3 * turn) & 3; /* turned: its +x up the screen */
            g->vertical = vert;
            float tw = w * v->s, tht = h * v->s;
            float x0 = room.x + (rw - tw) / 2, y0 = room.y + (rh - tht) / 2;
            if (vert) x0 = room.x + (room.w - tw - th - gap) / 2;
            else y0 = room.y + (room.h - tht - th - gap) / 2;
            v->cx = floorf(x0 + tw / 2);
            v->cy = floorf(y0 + tht / 2);
            if (vert) g->gauge = (box){x0 + tw + gap, y0 + 0.12f * tht, th, 0.76f * tht};
            else g->gauge = (box){x0 + 0.12f * tw, y0 + tht + gap, 0.76f * tw, th};
        }
}

/* the power at a point of the gauge (0..1; low at the bottom, or at the left) */
static inline float cue_gauge_at(const cue_gauge *g, float x, float y)
{
    float f = g->vertical ? (g->gauge.y + g->gauge.h - y) / g->gauge.h : (x - g->gauge.x) / g->gauge.w;
    return clampf(f, 0, 1);
}

static inline int cue_gauge_hit(const cue_gauge *g, float x, float y)
{
    float m = 0.45f * ui.unit;
    return x >= g->gauge.x - m && x <= g->gauge.x + g->gauge.w + m && y >= g->gauge.y - m &&
           y <= g->gauge.y + g->gauge.h + m;
}

/* the gauge: a walnut groove, the power in it from ivory through gold to red lacquer; lit: it may be used now */
static inline void cue_gauge_draw(const cue_gauge *g, float power, int lit)
{
    box b = g->gauge;
    float e = fmaxf(2, 0.07f * ui.unit);
    if (lit) ui_fill(b.x - 2 * e, b.y - 2 * e, b.w + 4 * e, b.h + 4 * e, 214, 172, 74, 150);
    ui_fill(b.x - e, b.y - e, b.w + 2 * e, b.h + 2 * e, 58, 36, 22, 255);
    ui_fill(b.x, b.y, b.w, b.h, 30, 20, 14, 255);
    int n = 40;
    for (int k = 0; k < n; k++) {                /* the ticks of the scale, faint; the power filled bright */
        float f = (k + 0.5f) / n, on = f <= power;
        Uint8 r = (Uint8)(f < 0.5f ? 236 - 30 * f : 226 - 40 * (f - 0.5f)), gg = (Uint8)(f < 0.5f ? 222 - 60 * f : 192 - 300 * (f - 0.5f)),
              bb = (Uint8)(f < 0.5f ? 190 - 200 * f : 90 - 100 * (f - 0.5f));
        Uint8 a = on ? 255 : lit ? 46 : 26;
        if (g->vertical) {
            float h = b.h / n;
            ui_fill(b.x + 0.18f * b.w, b.y + b.h - (k + 1) * h + 0.12f * h, 0.64f * b.w, 0.76f * h, r, gg, bb, a);
        } else {
            float w = b.w / n;
            ui_fill(b.x + k * w + 0.12f * w, b.y + 0.18f * b.h, 0.76f * w, 0.64f * b.h, r, gg, bb, a);
        }
    }
}

/* ---- dots and rings for the aiming line ------------------------------------------------------------------------ */
static inline SDL_Texture *cue_dot_tex(void)
{
    static SDL_Texture *t;
    if (!t) {
        static const uint8_t cream[3] = {250, 240, 214};
        art_disc(32, cream, 1, 0, ui_buf(32, 32));
        t = ui_tex(32, 32, ui_buf(32, 32));
    }
    return t;
}

static inline SDL_Texture *cue_ring_tex(void)
{
    static SDL_Texture *t;
    if (!t) {
        static const uint8_t white[3] = {255, 255, 255};
        art_ring(96, 0.86f, white, 1, ui_buf(96, 96));
        t = ui_tex(96, 96, ui_buf(96, 96));
    }
    return t;
}

/* dots from (x0, y0) to (x1, y1) (table units), every `step` units, d pixels across, faded by a */
static inline void cue_dots(const cue_view *v, float x0, float y0, float x1, float y1, float step, float d, float a)
{
    SDL_Texture *t = cue_dot_tex();
    float L = hypotf(x1 - x0, y1 - y0);
    int n = (int)(L / step);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * a));
    for (int k = 1; k <= n; k++) {
        float f = k * step / L, sx, sy;
        cue_to_screen(v, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, &sx, &sy);
        ui_put_c(t, sx, sy, d, d);
    }
}

/* the aiming line of ball i along (dx, dy): dots to where it first touches a ball or a cushion, a ring for it there,
 * then the first bounce: the ball struck's way and its own (or its way off the cushion). good(j): the colour of the
 * ring (1 gold: a ball that may be struck first; 0 red: a foul). Returns the ball it would touch, or -1. */
static inline int cue_aim_draw(const cue_view *v, const cue_table *t, const cue_world *w, int i, float dx, float dy,
                               int (*good)(int j))
{
    const cue_ball *c = &w->b[i];
    float dist;
    int edge, j = cue_ray(t, w, c->x, c->y, c->r, dx, dy, i, &dist, &edge);
    float gx = c->x + dx * dist, gy = c->y + dy * dist, dd = fmaxf(3, 0.16f * c->r * 2 * v->s), step = 1.15f * c->r;
    cue_dots(v, c->x, c->y, gx, gy, step, dd, 0.9f);
    float sx, sy, rd = 2 * c->r * v->s;
    cue_to_screen(v, gx, gy, &sx, &sy);
    SDL_Texture *ring = cue_ring_tex();
    int ok = j < 0 || !good || good(j);
    SDL_SetTextureColorMod(ring, ok ? 255 : 230, ok ? 236 : 70, ok ? 190 : 60);
    SDL_SetTextureAlphaMod(ring, 230);
    ui_put_c(ring, sx, sy, rd, rd);
    float len = 7 * c->r;
    if (j >= 0) {
        const cue_ball *o = &w->b[j];
        float nx = o->x - gx, ny = o->y - gy, nl = hypotf(nx, ny);
        if (nl > 1e-4f) {
            nx /= nl;
            ny /= nl;
            float k = dx * nx + dy * ny;         /* how full the contact: the ball struck goes along n */
            cue_dots(v, o->x, o->y, o->x + nx * len * (0.35f + 0.65f * k), o->y + ny * len * (0.35f + 0.65f * k), step,
                     dd, 0.75f);
            float tx = dx - k * nx, ty = dy - k * ny;
            cue_dots(v, gx, gy, gx + tx * len, gy + ty * len, step, dd * 0.8f, 0.45f);
        }
    } else {
        float rx = dx, ry = dy;
        if (edge == CUE_LEFT || edge == CUE_RIGHT) rx = -rx;
        else ry = -ry;
        cue_dots(v, gx, gy, gx + rx * len, gy + ry * len, step, dd * 0.8f, 0.5f);
    }
    SDL_SetTextureColorMod(ring, 255, 255, 255);
    return j;
}

/* ---- a shot as it is drawn: the rules' steps, run with the clock ---------------------------------------------- */
#define CUE_FALL_MS 170
typedef struct {
    int on;
    cue_table t;
    cue_world w;
    cue_events e;
    Uint32 t0;
    int done;                                   /* steps run */
    Uint32 fell[CUE_MAX];                       /* when each ball fell (0: not), its last place */
    float fx[CUE_MAX], fy[CUE_MAX];
    Uint32 click;                               /* the last click's time */
} cue_replay;

static inline void cue_replay_start(cue_replay *r, const cue_table *t, const cue_world *w)
{
    r->on = 1;
    r->t = *t;
    r->w = *w;
    cue_events_clear(&r->e);
    r->t0 = ui_ms();
    r->done = 0;
    memset(r->fell, 0, sizeof r->fell);
    r->click = 0;
}

/* runs the steps due by now (the clicks, the drops' sounds); 1 while it runs */
static inline int cue_replay_tick(cue_replay *r, float loud)
{
    if (!r->on) return 0;
    Uint32 now = ui_ms();
    long due = (long)(now - r->t0) * CUE_HZ / 1000;
    float knock = 0;
    int fell = 0;
    while (r->done < due && r->done < CUE_MAX_STEPS && cue_moving(&r->w)) {
        cue_world before = r->w;
        float k = cue_step(&r->t, &r->w, &r->e);
        if (k > knock) knock = k;
        for (int i = 0; i < r->w.n; i++)
            if (before.b[i].down < 0 && r->w.b[i].down >= 0) {
                r->fell[i] = r->t0 + (Uint32)(r->done * 1000L / CUE_HZ);
                if (!r->fell[i]) r->fell[i] = 1;
                r->fx[i] = before.b[i].x;
                r->fy[i] = before.b[i].y;
                fell = 1;
            }
        r->done++;
    }
    if (knock > 8 && now - r->click > 45) {
        ui_sound_soft(SND_PLACE, fminf(1, 0.15f + knock / loud));
        r->click = now;
    }
    if (fell) ui_sound(SND_CAPTURE);
    int falling = 0;
    for (int i = 0; i < r->w.n; i++) falling |= r->fell[i] && now - r->fell[i] < CUE_FALL_MS;
    if (!cue_moving(&r->w) && !falling) r->on = 0;
    if (r->on) ui_animate();
    return r->on;
}

/* a ball falling: how far into its pocket (0..1), its place then; 0: not falling now */
static inline int cue_replay_falling(const cue_replay *r, int i, float px, float py, float *x, float *y, float *k)
{
    if (!r->on || !r->fell[i]) return 0;
    float f = clampf((ui_ms() - r->fell[i]) / (float)CUE_FALL_MS, 0, 1);
    if (f >= 1) return 0;
    *x = r->fx[i] + (px - r->fx[i]) * f * 0.7f;
    *y = r->fy[i] + (py - r->fy[i]) * f * 0.7f;
    *k = 1 - 0.55f * f;
    return 1;
}

#endif
