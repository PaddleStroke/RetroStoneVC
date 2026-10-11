/*
 * Peg Solitaire in RetroStone Board, for one player: the round dish turned into the plank, its holes, the glossy
 * marbles, and the gutter round the dish where the marbles taken off go (the rules: pegsolitaire_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The holes lie in the middle of the board's square, a hole spacing apart: the crosses on a square grid, the triangle
 * on its three lines. Round them, the gutter: a groove where the marbles taken off sit in a row, from the top round
 * to the right. The input: tap a marble (its jumps show as dots), then a dot; or drag the marble there; or tap an
 * empty hole only one marble can jump into. The marble hops over its neighbour, which lifts off and rolls into the
 * gutter. Undo takes back one jump, all the way to the start.
 * The bar: the marbles left; the best game of this board since the app started (fewest marbles left; in gold light
 * when that one ended in the goal hole). It is kept in memory only, never in a file.
 */
#include <math.h>
#include <string.h>

#include "games/pegsolitaire.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define H 0.8660254f                            /* from one row of the triangle to the next, in spacings */
#define HOLE 0.22f                              /* a hole's radius (spacings) */
#define MARBLE 0.86f                            /* a marble's width (spacings) */
#define TRAY 0.60f                              /* a marble's width in the gutter */
#define PITCH 0.672f                            /* from one marble to the next in the gutter */
#define GUTTER 0.34f                            /* the gutter's half width */
#define COLOUR 5                                /* the marbles: lapis glass (the house's sixth colour) */
#define JUMP_MS 300
#define FLY_MS 460
#define LIFT_AT 120                             /* the marble jumped over lifts off this long after the hop starts */
#define PI_F 3.14159265f

static psgame G;
static int q, fr, made_w = -1, made_v = -1, made_small = -1;
static float bx, by, cx, cy, s;                 /* the board's corner, its middle, the spacing (pixels) */
static SDL_Texture *t_board, *t_marble, *t_tray, *t_small;
static int sel = -1;                            /* the marble picked */
static psmove opts[8];                          /* its jumps */
static int nopts;
static struct { int on, from, moved, fresh, target; float x, y, x0, y0; } ptr;   /* fresh: picked by this press */
static struct { int on, dropped; psmove m; Uint32 t0; } anim;
static struct { int pegs, gold; } best[PS_VARIANTS];   /* this app's best games (memory only) */

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static float ring_r(int v) { return v == PS_TRIANGLE ? 3.0f : 3.85f; }
static int slots(int v) { return (int)(2 * PI_F * ring_r(v) / PITCH); }
/* the spacing that fits the dish and its gutter across a plank of inner side `inner` */
static float spacing(int v, float inner) { return inner / (2 * (ring_r(v) + GUTTER + 0.3f)); }

/* a hole's centre in spacings from the board's middle, y down */
static void plane(int v, int c, float *x, float *y)
{
    int gx = c % PS_SIDE, gy = c / PS_SIDE;
    if (v == PS_TRIANGLE) {
        *x = gx - gy / 2.0f;
        *y = (gy - 8 / 3.0f) * H;               /* the triangle's middle on the board's */
    } else {
        *x = gx - 3.0f;
        *y = gy - 3.0f;
    }
}

/* the gutter's place k: from the top, round to the right */
static void slot_plane(int v, int k, float *x, float *y)
{
    float a = -PI_F / 2 + 2 * PI_F * (k + 0.5f) / slots(v);
    *x = ring_r(v) * cosf(a);
    *y = ring_r(v) * sinf(a);
}

static void hole_xy(int c, float *x, float *y)
{
    float px, py;
    plane(G.variant, c, &px, &py);
    *x = cx + s * px;
    *y = cy + s * py;
}

static void slot_xy(int k, float *x, float *y)
{
    float px, py;
    slot_plane(G.variant, k, &px, &py);
    *x = cx + s * px;
    *y = cy + s * py;
}

static int xy_hole(float x, float y)            /* the hole under (x, y), -1: none (a little room round each) */
{
    int best_c = -1;
    float bd = 0.62f * s;
    for (int c = 0; c < PS_CELLS; c++) {
        if (!G.hole[c]) continue;
        float hx, hy;
        hole_xy(c, &hx, &hy);
        float d = hypotf(x - hx, y - hy);
        if (d < bd) bd = d, best_c = c;
    }
    return best_c;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void pick(int c)
{
    sel = c;
    nopts = c >= 0 ? ps_moves_from(&G, c, opts) : 0;
}

static void calm(void)
{
    pick(-1);
    memset(&ptr, 0, sizeof ptr);
    anim.on = 0;
}

static void start(int variant)
{
    ps_new(&G, variant);
    calm();
}

static int status(void)
{
    int st = ps_status(&G);
    return st == PS_ONE ? GS_WIN0 : st == PS_STUCK ? GS_WIN1 : GS_PLAY;
}

static int turn(void) { return 0; }

static void remember(void)                      /* the game over: the best of this board, if it beats it */
{
    if (ps_status(&G) == PS_PLAY) return;
    int v = G.variant, gold = ps_gold(&G);
    if (!best[v].pegs || G.pegs < best[v].pegs || (G.pegs == best[v].pegs && gold)) {
        best[v].pegs = G.pegs;
        best[v].gold |= gold;
    }
}

static int removed(void) { return G.holes - 1 - G.pegs; }   /* the marbles in the gutter */

static void play(int from, int to, int dropped)
{
    if (!ps_play(&G, from, to)) { ui_sound(SND_DENY); return; }
    anim.m = G.hist[G.n - 1];
    anim.on = 1;
    anim.dropped = dropped;
    anim.t0 = ui_ms();
    pick(-1);
    ui_sound(dropped ? SND_PLACE : SND_MOVE);
    ui_sound_later(SND_CAPTURE, LIFT_AT + FLY_MS);
    remember();
    ui_animate();
}

static int undo(void)
{
    calm();
    return ps_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = ps_script(&G, moves);
    remember();
    return ok;
}

static int busy(void) { return anim.on; }

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
/* a round hole bored in the wood (r its radius, b its bevelled rim, pixels; dx, dy from its centre): how much to
 * darken or lighten the wood there, the far wall in the light from the upper left; 1 untouched */
static float bore(float dx, float dy, float r, float b)
{
    float d = hypotf(dx, dy);
    if (d >= r + b) return 1;
    float lit = (dx + dy) / (1.4142f * d + 1e-3f);
    float wall = 0.42f + 0.24f * lit * fminf(d / r, 1), rim = 1 + 0.2f * lit * clampf(1 - (d - r) / b, 0, 1);
    float in = clampf(r - d + 0.5f, 0, 1);
    return wall * in + rim * (1 - in);
}

/* the dish on the plank o (w pixels wide, its frame f): its floor a shade darker, the gutter round it (a groove, its
 * far side lit), the holes */
static void paint_dish(uint8_t *o, int w, int f, int v)
{
    psgame T;
    ps_new(&T, v);
    float sp = spacing(v, (float)(w - 2 * f)), mid = w / 2.0f, R = ring_r(v);
    for (int py = f; py < w - f; py++)
        for (int px = f; px < w - f; px++) {
            float x = (px + 0.5f - mid) / sp, y = (py + 0.5f - mid) / sp, r = hypotf(x, y), k = 1;
            k *= 1 - 0.07f * clampf((R - GUTTER - 0.12f - r) * sp + 0.5f, 0, 1);   /* the dish's floor */
            float g = (r - R) / GUTTER;         /* across the gutter: -1 its inner edge .. 1 its outer */
            if (fabsf(g) < 1 + 2.0f / (GUTTER * sp)) {
                float ux = x / (r + 1e-4f), uy = y / (r + 1e-4f);
                float facing = -(ux + uy) * 0.7071f;   /* 1: this side of the ring faces the light (upper left) */
                float depth = sqrtf(fmaxf(0, 1 - g * g)), slope = clampf(g, -1, 1);
                float in = clampf((1 - fabsf(g)) * GUTTER * sp + 0.5f, 0, 1);
                float lit = 0.62f + 0.12f * depth - 0.30f * slope * facing;
                float rim = 1 + 0.16f * (fabsf(g) > 1 ? (g > 0 ? facing : -facing) : 0);
                k *= in * lit + (1 - in) * rim;
            }
            int gy = v == PS_TRIANGLE ? (int)lroundf(y / H + 8 / 3.0f) : (int)lroundf(y + 3);   /* the nearest hole */
            int gx = v == PS_TRIANGLE ? (int)lroundf(x + gy / 2.0f) : (int)lroundf(x + 3);
            if (gx >= 0 && gy >= 0 && gx < PS_SIDE && gy < PS_SIDE && T.hole[gy * PS_SIDE + gx]) {
                float hx, hy;
                plane(v, gy * PS_SIDE + gx, &hx, &hy);
                k *= bore((x - hx) * sp, (y - hy) * sp, HOLE * sp, 0.07f * sp);
            }
            uint8_t *p = o + 4 * (py * w + px);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)clampf(p[j] * k + 0.5f, 0, 255);
        }
}

static void paint_board(uint8_t *o, int w, int f, int v)
{
    int qq = (w - 2 * f) / 9;
    art_board_n(9, 9, qq, f, 0, o);
    paint_dish(o, w, f, v);
}

static box last_box;

static void layout(box b)
{
    last_box = b;
    q = (int)(b.w * (1 - 2 * FRAME) / 9);
    fr = (int)((b.w - 9 * q) / 2);
    int w = 9 * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    cx = bx + w / 2.0f;
    cy = by + w / 2.0f;
    s = spacing(G.variant, 9.0f * q);
    if (made_w != w || made_v != G.variant) {
        made_w = w;
        made_v = G.variant;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, G.variant);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int d = (int)lroundf(MARBLE * s), t = (int)lroundf(TRAY * s);
        ui_free(&t_marble);
        art_counter(COUNTER_STONE, COLOUR, d, ui_buf(d, d));
        t_marble = ui_tex(d, d, ui_buf(d, d));
        ui_free(&t_tray);
        art_counter(COUNTER_STONE, COLOUR, t, ui_buf(t, t));
        t_tray = ui_tex(t, t, ui_buf(t, t));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        art_counter(COUNTER_STONE, COLOUR, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static float ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static void marble(float x, float y, float d, float lift)
{
    SDL_Texture *t = d < 0.75f * MARBLE * s ? t_tray : t_marble;
    ui_shadow_flat(x + 0.25f * d * lift, y + 0.35f * d * lift, d, d, 1 - 0.5f * lift);
    ui_put_c(t, x, y - 0.3f * d * lift, d * (1 + 0.18f * lift), d * (1 + 0.18f * lift));
}

static void draw(void)
{
    float w = 9.0f * q + 2 * fr, d = MARBLE * s, td = TRAY * s, x, y;
    if (made_v != G.variant) {                  /* a new game of another board: its picture */
        layout(last_box);
        w = 9.0f * q + 2 * fr;
        d = MARBLE * s;
        td = TRAY * s;
    }
    ui_put(t_board, bx, by, w, w);
    float t = anim.on ? (float)(ui_ms() - anim.t0) : 0;
    if (anim.on && t >= LIFT_AT + FLY_MS && t >= JUMP_MS) anim.on = 0;
    if (anim.on) ui_animate();
    int st = status();
    if (G.n && !anim.on) {                      /* the last jump: where it left, and where it landed */
        psmove m = G.hist[G.n - 1];
        hole_xy(m.from, &x, &y);
        ui_mark_last_round(x, y, 0.96f * s);
        hole_xy(m.to, &x, &y);
        ui_mark_last_round(x, y, 0.96f * s);
    }
    if (sel >= 0) {
        hole_xy(sel, &x, &y);
        ui_mark_select_round(x, y, 1.0f * s);
    }
    int last = ps_last(&G);
    if (st == GS_WIN0 && !anim.on && ps_gold(&G)) {   /* the last marble in the goal: gold */
        hole_xy(last, &x, &y);
        ui_mark_gold(x, y, 2.2f * s);
    }
    int in_gutter = removed() - (anim.on && t < LIFT_AT + FLY_MS);
    for (int k = 0; k < in_gutter; k++) {       /* the marbles taken off, in their gutter */
        slot_xy(k, &x, &y);
        marble(x, y, td, 0);
    }
    for (int c = 0; c < PS_CELLS; c++) {        /* the marbles at rest (the one moving, and the one held, apart) */
        if (!G.peg[c] || (anim.on && c == anim.m.to) || (ptr.on && ptr.moved && c == ptr.from)) continue;
        hole_xy(c, &x, &y);
        marble(x, y, d, 0);
    }
    if (anim.on) {
        float x0, y0, x1, y1;
        hole_xy(anim.m.from, &x0, &y0);
        hole_xy(anim.m.to, &x1, &y1);
        if (t < LIFT_AT) {                      /* the one jumped over, still in its hole */
            hole_xy(anim.m.over, &x, &y);
            marble(x, y, d, 0);
        } else if (t < LIFT_AT + FLY_MS) {      /* lifting off, rolling round into the gutter */
            float g = (t - LIFT_AT) / FLY_MS, e = ease(g), ox, oy, gx, gy;
            hole_xy(anim.m.over, &ox, &oy);
            slot_xy(removed() - 1, &gx, &gy);
            marble(ox + (gx - ox) * e, oy + (gy - oy) * e, d + (td - d) * e, sinf(PI_F * g));
        }
        float f = anim.dropped ? 1 : ease(t / JUMP_MS);   /* the jumper: a hop over its neighbour */
        marble(x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, d, anim.dropped ? 0 : sinf(PI_F * clampf(t / JUMP_MS, 0, 1)));
    }
    if (st != GS_PLAY || anim.on) return;
    for (int i = 0; i < nopts; i++) {           /* the picked marble's jumps */
        hole_xy(opts[i].to, &x, &y);
        ui_mark_dot(x, y, 0.5f * s);
    }
    if (ptr.on && ptr.moved && ptr.from >= 0) { /* the marble carried, a little above the finger */
        if (ptr.target >= 0) {
            hole_xy(ptr.target, &x, &y);
            ui_mark_select_round(x, y, 1.0f * s);
        }
        marble(ptr.x, ptr.y - 0.45f * d, d, 0.6f);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int target_of(int h)                     /* the picked marble's jump landing in h; -1 */
{
    for (int i = 0; i < nopts; i++)
        if (opts[i].to == h) return h;
    return -1;
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (status() != GS_PLAY) return;
    anim.on = 0;
    int h = xy_hole(x, y);
    ptr.x = ptr.x0 = x;
    ptr.y = ptr.y0 = y;
    ptr.target = -1;
    ptr.from = -1;
    if (h >= 0 && G.peg[h]) {                   /* a marble: pick it (or keep it, to let it go on release) */
        if (!ps_moves_from(&G, h, NULL)) { ui_sound(SND_DENY); pick(-1); return; }
        ptr.on = 1;
        ptr.from = h;
        if (h != sel) { pick(h); ptr.fresh = 1; }
        ui_animate();
        return;
    }
    if (h >= 0 && target_of(h) >= 0) {          /* a dot: the jump, on release */
        ptr.on = 1;
        ptr.target = h;
        return;
    }
    psmove all[PS_MAX_MOVES], *only = NULL;     /* an empty hole only one marble can jump into */
    int n = h >= 0 ? ps_moves(&G, all) : 0, k = 0;
    for (int i = 0; i < n; i++)
        if (all[i].to == h) { only = &all[i]; k++; }
    if (k == 1) { play(only->from, only->to, 0); return; }
    if (h >= 0 || sel >= 0) ui_sound(SND_DENY);
    pick(-1);
}

static void motion(float x, float y)
{
    if (!ptr.on || ptr.from < 0) return;
    ptr.x = x;
    ptr.y = y;
    if (hypotf(x - ptr.x0, y - ptr.y0) > 0.3f * s) ptr.moved = 1;
    if (ptr.moved) ptr.target = target_of(xy_hole(x, y - 0.45f * MARBLE * s));
    ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.from < 0) {                         /* pressed on a dot */
        if (xy_hole(x, y) == ptr.target) play(sel, ptr.target, 0);
        return;
    }
    if (ptr.moved) {                            /* dropped: on a dot, the jump; elsewhere, back to its hole */
        int h = target_of(xy_hole(x, y - 0.45f * MARBLE * s));
        if (h < 0) h = target_of(xy_hole(x, y));
        if (h >= 0) play(ptr.from, h, 1);
        return;
    }
    if (!ptr.fresh && xy_hole(x, y) == sel) pick(-1);   /* a tap on the marble picked: let it go */
}

/* ---- the bar: the marbles left, and the best game of this board ------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_bar_count(b, t_small, G.pegs);
    const int v = G.variant;
    if (!best[v].pegs) return;
    float u = ui.unit, sz = 0.6f * u, dh = 0.46f * u, cyb = b.y + b.h / 2;
    int digits = best[v].pegs >= 10 ? 2 : 1;
    float nw = (0.74f * (digits - 1) + 0.62f) * dh, x = b.x + b.w - nw - sz - 0.25f * u;
    if (x < b.x + 1.45f * u) return;            /* no room beside the count */
    if (best[v].gold) ui_mark_gold(x + sz / 2, cyb, 2.6f * sz);
    else ui_mark_last_round(x + sz / 2, cyb, 1.25f * sz);   /* the best so far: a warm light */
    ui_put(t_small, x, cyb - sz / 2, sz, sz);
    ui_number(x + sz + 0.12f * u + nw / 2, cyb, dh, best[v].pegs, 1);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_counter(COUNTER_STONE, COLOUR, size, rgba);
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
static void thumb_marble(uint8_t *rgba, int size, float x, float y, int d, int shadow)
{
    uint8_t *one = ui_buf(d, d);
    if (shadow) art_flat_shadow(d, one);
    else art_counter(COUNTER_STONE, COLOUR, d, one);
    int ox = (int)lroundf(x - d / 2.0f), oy = (int)lroundf(y - d / 2.0f);
    if (shadow) { ox += d / 20; oy += d / 12; }
    art_blend(rgba, size, size, one, d, d, ox, oy);
}

/* the menu's card: the English board in the middle of a game, the gutter half full; the picker's: each start */
static void thumb(int variant, int size, uint8_t *rgba)
{
    psgame T;
    int v = variant < 0 ? PS_ENGLISH : variant;
    ps_new(&T, v);
    if (variant < 0)
        ps_script(&T, "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 e7e5 c3c5");
    int f = (int)(size * 0.04f), qq = (size - 2 * f) / 9, w = 9 * qq + 2 * f, o = (size - w) / 2;
    uint8_t *board = calloc((size_t)w * w, 4);
    if (!board) return;
    paint_board(board, w, f, v);
    art_blend(rgba, size, size, board, w, w, o, o);
    free(board);
    float sp = spacing(v, 9.0f * qq), mid = o + w / 2.0f;
    int d = (int)lroundf(MARBLE * sp), td = (int)lroundf(TRAY * sp), out = T.holes - 1 - T.pegs;
    for (int pass = 1; pass >= 0; pass--) {     /* the shadows, then the marbles */
        for (int c = 0; c < PS_CELLS; c++) {
            if (!T.peg[c]) continue;
            float x, y;
            plane(v, c, &x, &y);
            thumb_marble(rgba, size, mid + x * sp, mid + y * sp, d, pass);
        }
        for (int k = 0; k < out; k++) {
            float x, y;
            slot_plane(v, k, &x, &y);
            thumb_marble(rgba, size, mid + x * sp, mid + y * sp, td, pass);
        }
    }
}

static const game pegsolitaire_game = {
    .id = "pegsolitaire", .order = 300, .variants = PS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
};
REGISTER_GAME(pegsolitaire_game)
