/*
 * Ring Puzzle in RetroStone Board, for one player: brass ring tracks set in a walnut-framed plank, polished beads of
 * ivory, ebony and coloured lacquer riding in them, each place's colour inlaid in lacquer along the outside of its
 * track (the rules: rings_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag round a ring: its beads follow the finger along the track (the shared ones with them, out of the other ring),
 * and on letting go they settle to the nearest step with a little ease past it and back; every step is a move. Or tap
 * one of a ring's two curved arrows, outside it: one step that way. The group of rings stands across the screen or
 * upright, whichever gives them more room. The lamp: the hint (the next move of a shortest way: its ring in gold
 * light, its arrow lit; tap it). Undo takes back one step, all the way to the start. The bar: the moves, and at its
 * right end the par by its flag. Solved: a gold light runs round the rings and the beads lift one after another.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/rings.h"
#include "shell.h"

#define TURN_MS 300                             /* a step, eased past and back */
#define SNAP_MS 220
#define WIN_MS 1500
#define HINT_TOOL 0
#define MARGIN 1.72f                            /* room round a ring (of its radius): beads, inlays, arrows */

static rg_game G;
static box last_box;
static int turned, made_key = -1, made_bw = -1, made_bh = -1, made_small = -1, serial, made_serial = -1;
static float S, ox, oy, bead;                   /* the scale (a ring's radius, pixels), the unit's origin; a bead */
static float bx, by;                            /* the board picture's top left */
static int bw, bh;
static SDL_Texture *t_board, *t_bead[RG_COLOURS], *t_par;

static struct {                                 /* a ring turning: from angle a0 to 0 (radians, the step's way) */
    int on, ring;
    float a0;
    rg_pos from;                                /* the position drawn (the beads as they were) */
    Uint32 t0, ms;
} anim;
static struct { int on, ring, moving; float x0, y0, angle, last; int cand[RG_RINGS], ncand; } drag;
static struct { int on, move; Uint32 t0; } hint;
static struct { int on; Uint32 t0; } win;
static int tool_down = -1, arrow_down = -1;

/* ---- the geometry ------------------------------------------------------------------------------------------- */
static void to_screen(const rg_map *m, float ux, float uy, float *x, float *y)
{
    (void)m;
    if (turned) { float t = ux; ux = -uy; uy = t; }
    *x = ox + ux * S;
    *y = oy + uy * S;
}

static void to_unit(float x, float y, float *ux, float *uy)
{
    float a = (x - ox) / S, b = (y - oy) / S;
    if (turned) { *ux = b; *uy = -a; }
    else { *ux = a; *uy = b; }
}

static float step_angle(void) { return 2 * PUZ_PI / G.m.n; }

/* a ring's way out (unit space): away from the rings' middle (the middle of a row: up) */
static void outward(const rg_map *m, int k, float *dx, float *dy)
{
    float gx = 0, gy = 0;
    for (int i = 0; i < m->rings; i++) gx += m->cx[i] / m->rings, gy += m->cy[i] / m->rings;
    float x = m->cx[k] - gx, y = m->cy[k] - gy, l = sqrtf(x * x + y * y);
    if (l < 1e-3f) x = 0, y = -1, l = 1;
    *dx = x / l;
    *dy = y / l;
}

static float bead_unit(const rg_map *m) { return fminf(0.84f * 2 * PUZ_PI / m->n, 0.5f); }

/* ring k's arrow (0: counter-clockwise, 1: clockwise): its centre on the screen */
static void arrow_at(int k, int which, float *x, float *y)
{
    float dx, dy;
    outward(&G.m, k, &dx, &dy);
    float base = atan2f(-dy, dx), a = base + (which ? -0.42f : 0.42f), r = 1 + 0.5f * bead_unit(&G.m) + 0.42f;
    to_screen(&G.m, G.m.cx[k] + r * cosf(a), G.m.cy[k] - r * sinf(a), x, y);
}

static float arrow_d(void) { return fmaxf(0.8f * bead, 0.85f * ui.unit); }

/* ---- the pictures ------------------------------------------------------------------------------------------- */
static void colour_rgb(int c, uint8_t rgb[3])
{
    for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)art_player[c & 3].fill[i];
}

/* the board: the plank in its frame, each place's colour inlaid outside its ring, the brass tracks; into o (w x h),
 * the unit's origin at (x0, y0), s pixels a ring's radius (turned: a quarter turn) */
static void paint_board(const rg_map *m, int w, int h, float x0, float y0, float s, int turn, int frame, uint8_t *o)
{
    int fr = frame ? (int)fmaxf(2, 0.09f * s) : 0;
    art_plank(w - 2 * fr, h - 2 * fr, fr, o);
    float bu = bead_unit(m), tw = 0.30f * bu * s, st0 = 1 * s + 0.5f * bu * s + 0.05f * s, st1 = st0 + 0.09f * s;
    float half = PUZ_PI / m->n;
    for (int pass = 0; pass < 2; pass++)        /* the inlays, then the tracks over them */
        for (int k = 0; k < m->rings; k++) {
            float cx = m->cx[k], cy = m->cy[k], sx, sy;
            if (turn) { float t = cx; cx = -cy; cy = t; }
            sx = x0 + cx * s;
            sy = y0 + cy * s;
            float reach = st1 + 2;
            for (int y = (int)fmaxf(0, sy - reach); y < h && y <= sy + reach; y++)
                for (int x = (int)fmaxf(0, sx - reach); x < w && x <= sx + reach; x++) {
                    float dx = x + 0.5f - sx, dy = y + 0.5f - sy, r = sqrtf(dx * dx + dy * dy);
                    uint8_t *p = o + 4 * ((size_t)y * w + x);
                    if (pass == 0) {
                        float cov = clampf(fminf(r - st0, st1 - r) + 0.5f, 0, 1);
                        if (cov <= 0) continue;
                        for (int j = 0; j < m->rings; j++) {   /* none where it would run under another ring */
                            float jx = m->cx[j], jy = m->cy[j];
                            if (j == k) continue;
                            if (turn) { float t = jx; jx = -jy; jy = t; }
                            float e = hypotf(x + 0.5f - (x0 + jx * s), y + 0.5f - (y0 + jy * s)) - (st1 + 0.06f * s);
                            cov *= clampf(e + 0.5f, 0, 1);
                        }
                        if (cov <= 0) continue;
                        /* the slot: the angle in the ring's own (unturned) frame */
                        float ux = dx, uy = dy;
                        if (turn) { ux = dy; uy = -dx; }
                        float a = atan2f(-uy, ux);
                        if (a < 0) a += 2 * PUZ_PI;
                        int i = (int)floorf(a / (2 * half) + 0.5f) % m->n;
                        float off = fabsf(a - i * 2 * half);
                        if (off > PUZ_PI) off = 2 * PUZ_PI - off;
                        {                       /* a place whose inlay runs into another ring: none at all */
                            float mr = (st0 + st1) / 2 / s, mx = m->cx[k] + mr * cosf(i * 2 * half);
                            float my = m->cy[k] - mr * sinf(i * 2 * half);
                            int under = 0;
                            for (int j = 0; j < m->rings; j++)
                                if (j != k && hypotf(mx - m->cx[j], my - m->cy[j]) < st1 / s + 0.1f) under = 1;
                            if (under) continue;
                        }
                        float edge = (half - off) * r - 0.06f * s;   /* a fine gap between two places' inlays */
                        cov *= clampf(edge + 0.5f, 0, 1);
                        if (cov <= 0) continue;
                        uint8_t c[3];
                        colour_rgb(rg_colour(m->target, m->slot[k][i]), c);
                        float t = (r - st0) / (st1 - st0), lit = 0.86f + 0.3f * (1 - fabsf(t - 0.35f) * 2);
                        for (int j = 0; j < 3; j++) c[j] = (uint8_t)clampf(c[j] * lit, 0, 255);
                        pzk_over(p, c, cov);
                    } else {
                        float d = fabsf(r - s), cov = clampf(tw - d + 0.5f, 0, 1);
                        if (cov <= 0) continue;
                        float t = (r - s) / tw, nx = dx / (r + 1e-3f), ny = dy / (r + 1e-3f);
                        float up = sqrtf(fmaxf(0, 1 - t * t)), lit = 0.5f + 0.42f * up + 0.34f * t * -(nx + ny) * 0.7071f;
                        if (fabsf(t) < 0.3f) lit *= 0.8f + 0.2f * fabsf(t) / 0.3f;   /* the groove the beads ride in */
                        static const uint8_t brass[3] = {204, 160, 78};
                        uint8_t c[3];
                        for (int j = 0; j < 3; j++) c[j] = (uint8_t)clampf(brass[j] * lit + 40 * powf(up, 12) * (t < 0), 0, 255);
                        pzk_over(p, c, cov);
                    }
                }
        }
    /* each place's colour round its seat: a ring of lacquer the bead sits in, a fine dark line on each side */
    float hr0 = 0.47f * bu * s, hr1 = 0.6f * bu * s;
    for (int i = 0; i < m->places; i++) {
        float ux = m->px[i], uy = m->py[i];
        if (turn) { float t = ux; ux = -uy; uy = t; }
        float sx = x0 + ux * s, sy = y0 + uy * s;
        uint8_t c[3];
        colour_rgb(rg_colour(m->target, i), c);
        for (int y = (int)fmaxf(0, sy - hr1 - 2); y < h && y <= sy + hr1 + 2; y++)
            for (int x = (int)fmaxf(0, sx - hr1 - 2); x < w && x <= sx + hr1 + 2; x++) {
                float dx = x + 0.5f - sx, dy = y + 0.5f - sy, r = sqrtf(dx * dx + dy * dy);
                float cov = clampf(fminf(r - hr0, hr1 - r) + 0.5f, 0, 1), line = clampf(1.2f - fabsf(r - hr1), 0, 1);
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                if (cov > 0) {
                    uint8_t l[3];
                    float k = 0.9f + 0.25f * -(dx + dy) / (r + 1e-3f) * 0.7f;
                    for (int j = 0; j < 3; j++) l[j] = (uint8_t)clampf(c[j] * k, 0, 255);
                    pzk_over(p, l, cov);
                }
                if (line > 0) pzk_over(p, PUZ_INK, 0.45f * line);
            }
    }
}

/* a bead's picture and its shadow blended into a thumbnail */
static void put_bead(uint8_t *o, int w, int h, int colour, float cx, float cy, int d)
{
    uint8_t *b = malloc((size_t)d * d * 4), rgb[3];
    if (!b) return;
    colour_rgb(colour, rgb);
    art_stone(rgb, d, b);
    art_flat_put(o, w, h, b, d, cx, cy);
    free(b);
}

/* the rings of a map with the beads of p, square, on their board (thumbnails) */
static void paint_map(const rg_map *m, rg_pos p, int size, uint8_t *rgba)
{
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int k = 0; k < m->rings; k++) {
        x0 = fminf(x0, m->cx[k]), x1 = fmaxf(x1, m->cx[k]);
        y0 = fminf(y0, m->cy[k]), y1 = fmaxf(y1, m->cy[k]);
    }
    float mg = 1.35f, s = size / fmaxf(x1 - x0 + 2 * mg, y1 - y0 + 2 * mg);
    float ux = size / 2.0f - (x0 + x1) / 2 * s, uy = size / 2.0f - (y0 + y1) / 2 * s;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(m, size, size, ux, uy, s, 0, 1, b);
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
    int d = (int)(bead_unit(m) * s);
    for (int i = 0; i < m->places && d >= 3; i++)
        put_bead(rgba, size, size, rg_colour(p, i), ux + m->px[i] * s, uy + m->py[i] * s, d);
}

/* a curved arrow round the button's middle (kind 0: counter-clockwise, 1: clockwise), white */
static void paint_arrow(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t white[3] = {255, 255, 255};
    art_fig *f = art_fig_new();
    float r = 0.24f, end = kind ? -80.0f : 260.0f;          /* (degrees, counter-clockwise, y up) */
    if (kind) art_arc(f, ART_PART, 0.5f, 0.5f, r - 0.05f, r + 0.05f, -60, 170);
    else art_arc(f, ART_PART, 0.5f, 0.5f, r - 0.05f, r + 0.05f, 10, 240);
    float e = end * PUZ_PI / 180, hx = 0.5f + r * cosf(e), hy = 0.5f - r * sinf(e);
    art_head(f, ART_PART, hx, hy, kind ? -(end - 90) : -(end + 90), 0.26f);
    art_style st = art_flat(white, white, 0, 0);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static void token(int player, int size, uint8_t *rgba)   /* two beads, ivory and ebony, overlapping */
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    int d = (int)(0.56f * size);
    put_bead(rgba, size, size, 1, 0.62f * size, 0.6f * size, d);
    put_bead(rgba, size, size, 0, 0.38f * size, 0.42f * size, d);
}

/* ---- the layout --------------------------------------------------------------------------------------------- */
static float shrink = 1;                        /* the scale cut so the arrows fit the room too */

static void layout(box b)
{
    last_box = b;
    const rg_map *m = &G.m;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int k = 0; k < m->rings; k++) {
        x0 = fminf(x0, m->cx[k]), x1 = fmaxf(x1, m->cx[k]);
        y0 = fminf(y0, m->cy[k]), y1 = fmaxf(y1, m->cy[k]);
    }
    float uw = x1 - x0 + 2 * MARGIN, uh = y1 - y0 + 2 * MARGIN;
    float s0 = fminf(b.w / uw, b.h / uh), s1 = fminf(b.w / uh, b.h / uw);
    turned = s1 > 1.04f * s0;
    S = floorf(shrink * fminf(turned ? s1 : s0, 0.3f * fminf((float)ui.w, (float)ui.h)));
    float mx = (x0 + x1) / 2, my = (y0 + y1) / 2, smx, smy;
    ox = oy = 0;
    float save = S;
    S = 1;
    to_screen(m, mx, my, &smx, &smy);
    S = save;
    ox = floorf(b.x + b.w / 2 - smx * S);
    oy = floorf(b.y + b.h / 2 - smy * S);
    bead = bead_unit(m) * S;
    /* the board: a plank round the rings and their arrows */
    float bwu = (turned ? uh : uw) - 0.25f, bhu = (turned ? uw : uh) - 0.25f;
    bw = (int)fminf(bwu * S, b.w);
    bh = (int)fminf(bhu * S, b.h);
    bx = floorf(b.x + (b.w - bw) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    /* the plank grown to hold the arrows as well (else they straddle its edge), within the room */
    float px0 = bx, py0 = by, px1 = bx + bw, py1 = by + bh, ar = arrow_d() / 2 + 0.12f * bead;
    for (int k = 0; k < m->rings; k++)
        for (int w = 0; w < 2; w++) {
            float ax, ay;
            arrow_at(k, w, &ax, &ay);
            px0 = fminf(px0, ax - ar), px1 = fmaxf(px1, ax + ar);
            py0 = fminf(py0, ay - ar), py1 = fmaxf(py1, ay + ar);
        }
    float over = fmaxf((px1 - px0) / b.w, (py1 - py0) / b.h);
    if (over > 1.001f && shrink == 1) {         /* the arrows out of the room: all a little smaller */
        shrink = 1 / over;
        layout(b);
        shrink = 1;
        return;
    }
    bx = floorf(fmaxf(px0, b.x));
    by = floorf(fmaxf(py0, b.y));
    bw = (int)(fminf(px1, b.x + b.w) - bx);
    bh = (int)(fminf(py1, b.y + b.h) - by);
    int key = (int)S * 16 + turned * 4 + m->kind;
    if (made_key != key || made_serial != serial || made_bw != bw || made_bh != bh) {
        made_key = key;
        made_bw = bw;
        made_bh = bh;
        made_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            float ux0, uy0;                     /* the unit's origin in the picture */
            to_screen(m, 0, 0, &ux0, &uy0);
            paint_board(m, bw, bh, ux0 - bx, uy0 - by, S, turned, 1, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        int d = (int)bead;
        for (int c = 0; c < RG_COLOURS; c++) {
            ui_free(&t_bead[c]);
            uint8_t rgb[3];
            colour_rgb(c, rgb);
            art_stone(rgb, d, ui_buf(d, d));
            t_bead[c] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 3) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------- */
static void calm(void)
{
    anim.on = drag.on = hint.on = win.on = 0;
    tool_down = arrow_down = -1;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!rg_level(&G, level)) rg_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    rg_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return rg_solved(&G); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.nh; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.nh, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && (int)(now - anim.t0) < (int)anim.ms) || (win.on && (int)(now - win.t0) < WIN_MS);
}

static void check_win(void)
{
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms() + (anim.on ? anim.ms : 0);
        ui_sound_later(SND_END, anim.on ? (int)anim.ms : 0);
    }
}

/* the ring k shown turned by `a0` (the step's way: + counter-clockwise) going to rest, over ms */
static void settle(int k, float a0, rg_pos from, Uint32 ms)
{
    anim.on = 1;
    anim.ring = k;
    anim.a0 = a0;
    anim.from = from;
    anim.t0 = ui_ms();
    anim.ms = ms;
    ui_animate();
}

/* one step of ring k (dir 0 +, 1 -), shown turning */
static void step(int k, int dir)
{
    if (solved()) return;
    rg_move(&G, k * 2 + dir);
    settle(k, dir ? step_angle() : -step_angle(), G.p, TURN_MS);
    ui_sound(SND_MOVE);
    hint.on = 0;
    check_win();
}

static int undo(void)
{
    calm();
    return rg_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int nh = G.nh, ok = rg_script(&G, moves);
    if (ok && G.nh > nh) {                      /* the last move shown turning (the pictures' --at) */
        int mv = G.hist[G.nh - 1];
        settle(mv >> 1, mv & 1 ? step_angle() : -step_angle(), G.p, TURN_MS);
    }
    check_win();
    return ok;
}

/* ---- drawing ------------------------------------------------------------------------------------------------ */
/* the ring's eased way to rest: past it a little, then back (f 0..1 -> 1..0) */
static float rest(float f)
{
    f = clampf(f, 0, 1);
    float c1 = 1.4f, c3 = c1 + 1, e = 1 + c3 * powf(f - 1, 3) + c1 * powf(f - 1, 2);   /* ease out, a little back */
    return 1 - e;
}

static void bead_at(int colour, float x, float y, float d, Uint8 alpha)
{
    SDL_Texture *t = t_bead[colour & 3];
    ui_shadow_flat(x, y, d, d, 0.55f * alpha / 255.0f);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    if (made_key < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)bw, (float)bh);
    const rg_map *m = &G.m;
    /* which ring turns, by how much: the finger's, or a step settling */
    int ring = -1;
    float off = 0;
    rg_pos shown = G.p;
    if (drag.on && drag.moving) ring = drag.ring, off = drag.angle;
    else if (anim.on) {
        float f = (now - anim.t0) / (float)anim.ms;
        if (f >= 1) anim.on = 0;
        else {
            ring = anim.ring;
            off = anim.a0 * rest(f);
            shown = anim.from;
            ui_animate();
        }
    }
    float sa = step_angle();
    /* the hint: its ring in gold light, pulsing */
    if (hint.on) {
        float pulse = 0.5f + 0.5f * sinf((now - hint.t0) / 160.0f);
        int k = hint.move >> 1;
        for (int i = 0; i < m->n; i++) {
            float x, y;
            to_screen(m, m->cx[k] + cosf(sa * (i + 0.5f)), m->cy[k] - sinf(sa * (i + 0.5f)), &x, &y);
            ui_mark_gold_a(x, y, 1.3f * bead, 0.25f + 0.3f * pulse);
        }
        ui_animate();
    }
    /* the beads: those of the turning ring at their slot's angle plus the turn, the others in their places */
    float wt = win.on && (int)(now - win.t0) >= 0 ? (float)(now - win.t0) : -1;
    if (win.on && wt < WIN_MS) ui_animate();
    for (int p = 0; p < m->places; p++) {
        int on = -1;
        if (ring >= 0)
            for (int i = 0; i < m->n; i++)
                if (m->slot[ring][i] == p) on = i;
        float x, y, d = bead;
        if (on >= 0) {
            float a = sa * on + off;
            to_screen(m, m->cx[ring] + cosf(a), m->cy[ring] - sinf(a), &x, &y);
        } else to_screen(m, m->px[p], m->py[p], &x, &y);
        if (wt >= 0) {                          /* the win: a wave of light and a lift, place by place */
            float k = puz_pulse(wt, 28.0f * p, 420);
            if (k > 0) ui_mark_gold_a(x, y, 2.6f * bead, k), ui_mark_gold_a(x, y, 1.6f * bead, k);
            y -= 0.18f * bead * k;
            d *= 1 + 0.08f * k;
        }
        bead_at(rg_colour(shown, p), x, y, d, 255);
    }
    /* the arrows */
    for (int k = 0; k < m->rings; k++)
        for (int w = 0; w < 2; w++) {
            float x, y;
            arrow_at(k, w, &x, &y);
            int how = UI_TINT | (arrow_down == k * 2 + w ? UI_ON : 0);
            if (hint.on && hint.move == k * 2 + w) how |= (now - hint.t0) / 300 % 2 ? UI_LIT : UI_ON;
            if (solved()) how |= UI_DIM;
            ui_button_paint(x, y, arrow_d(), paint_arrow, w, 0.9f, how);
        }
    ui_tool(HINT_TOOL, ICON_HINT, (solved() ? UI_DIM : 0) | (tool_down == HINT_TOOL ? UI_ON : 0));
}

/* ---- the pointer -------------------------------------------------------------------------------------------- */
static int arrow_hit(float x, float y)
{
    for (int k = 0; k < G.m.rings; k++)
        for (int w = 0; w < 2; w++) {
            float ax, ay;
            arrow_at(k, w, &ax, &ay);
            if (ui_button_hit(ax, ay, arrow_d(), x, y)) return k * 2 + w;
        }
    return -1;
}

static float finger_angle(int k, float x, float y)   /* the finger's angle round ring k (unit space, + ccw) */
{
    float ux, uy;
    to_unit(x, y, &ux, &uy);
    return atan2f(-(uy - G.m.cy[k]), ux - G.m.cx[k]);
}

static float wrap(float a)
{
    while (a > PUZ_PI) a -= 2 * PUZ_PI;
    while (a < -PUZ_PI) a += 2 * PUZ_PI;
    return a;
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return; }
    if (anim.on && (int)(ui_ms() - anim.t0) < (int)anim.ms) anim.on = 0;   /* the last step lands at once */
    drag.on = 0;
    arrow_down = -1;
    if (solved()) return;
    arrow_down = arrow_hit(x, y);
    if (arrow_down >= 0) { ui_animate(); return; }
    float ux, uy;
    to_unit(x, y, &ux, &uy);
    drag.ncand = 0;
    float slack = 0.75f * bead_unit(&G.m) + 0.12f;
    for (int k = 0; k < G.m.rings; k++) {
        float r = hypotf(ux - G.m.cx[k], uy - G.m.cy[k]);
        if (fabsf(r - 1) < slack) drag.cand[drag.ncand++] = k;
    }
    if (!drag.ncand) return;
    drag.on = 1;
    drag.moving = 0;
    drag.ring = drag.cand[0];
    drag.x0 = x;
    drag.y0 = y;
    drag.angle = 0;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moving) {
        if (hypotf(x - drag.x0, y - drag.y0) < 0.3f * bead) return;
        float best = -1;                        /* the ring the finger moves along most */
        for (int i = 0; i < drag.ncand; i++) {
            int k = drag.cand[i];
            float d = fabsf(wrap(finger_angle(k, x, y) - finger_angle(k, drag.x0, drag.y0)));
            if (d > best) best = d, drag.ring = k;
        }
        drag.moving = 1;
        drag.last = finger_angle(drag.ring, drag.x0, drag.y0);
        hint.on = 0;
    }
    float a = finger_angle(drag.ring, x, y);
    drag.angle += wrap(a - drag.last);
    drag.last = a;
    ui_animate();
}

static void use_hint(void)
{
    int mv = rg_hint(&G);
    if (mv < 0) { ui_sound(SND_DENY); return; }
    hint.on = 1;
    hint.move = mv;
    hint.t0 = ui_ms();
    ui_sound(SND_TAP);
    ui_animate();
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == k && k == HINT_TOOL && !solved()) use_hint();
        return;
    }
    if (arrow_down >= 0) {
        int a = arrow_down;
        arrow_down = -1;
        if (arrow_hit(x, y) == a) step(a >> 1, a & 1);
        ui_animate();
        return;
    }
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moving) {                         /* a tap on a ring: nothing to do there */
        ui_animate();
        return;
    }
    float sa = step_angle();
    int k = (int)lroundf(drag.angle / sa), ring = drag.ring;
    for (int i = 0; i < abs(k); i++) rg_move(&G, ring * 2 + (k < 0));
    /* the beads shown from where the finger left them: the new position, turned back by the rest */
    settle(ring, drag.angle - k * sa, G.p, SNAP_MS);
    if (k) {
        ui_sound(SND_MOVE);
        hint.on = 0;
    }
    check_win();
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.nh, t_par, par(), solved() && G.nh <= par());
}

/* ---- the shell's pictures ----------------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const uint8_t KIND[RG_VARIANTS] = {RG_TWO, RG_TWO, RG_TRIANGLE}, N[RG_VARIANTS] = {8, 12, 12},
                         PAT[RG_VARIANTS] = {0, 1, 1};
    rg_map m;
    int v = variant < 0 ? 1 : variant % RG_VARIANTS;
    rg_build(&m, KIND[v], N[v]);
    rg_pattern(&m, PAT[v], 0);
    rg_pos p = m.target;
    if (variant < 0) {                          /* the menu's card: a little scrambled */
        static const uint8_t way[] = {0, 2, 2, 1};
        for (int i = 0; i < 4; i++) p = rg_apply(&m, p, way[i]);
    }
    paint_map(&m, p, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    rg_game *t = malloc(sizeof *t);
    if (t && rg_level(t, level)) paint_map(&t->m, t->p, size, rgba);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const uint8_t KIND[RG_CHAPTERS] = {RG_TWO, RG_TWO, RG_TWO, RG_CHAIN, RG_TRIANGLE, RG_TRIANGLE},
                         N[RG_CHAPTERS] = {8, 8, 12, 8, 12, 12}, PAT[RG_CHAPTERS] = {0, 1, 2, 0, 0, 1};
    int c = chapter < 0 ? 0 : chapter % RG_CHAPTERS;
    rg_map m;
    rg_build(&m, KIND[c], N[c]);
    rg_pattern(&m, PAT[c], 0);
    paint_map(&m, m.target, size, rgba);
}

static const game rings_game = {
    .id = "rings", .order = 462, .variants = RG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = RG_LEVELS, .chapters = RG_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = 1,
};
REGISTER_GAME(rings_game)
