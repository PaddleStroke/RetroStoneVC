/*
 * Peg Solitaire in RetroStone Board, for one player: the round dish turned into the plank, its holes, the glossy
 * marbles, and the gutter round the dish where the marbles taken off go (the rules: pegsolitaire_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The level screen: 150 levels in six chapters (positions on the six boards, the classic full boards ending their
 * chapters), and a die tab for a random position on any of the boards. The goal hole has a brass ring. The holes lie in
 * the middle of the board's square, a hole spacing apart: the crosses and the diamond on a square grid, the triangles
 * on their three lines. Round them, the gutter: a groove where the marbles taken off sit in a row, from the top round
 * to the right. The input: tap a marble (its jumps show as dots), then a dot; or drag the marble there; or tap an empty
 * hole only one marble can jump into. The marble hops over its neighbour, which lifts off and rolls into the gutter.
 * Stuck with several marbles: they glow red (undo). Undo takes back one jump, all the way to the start. The bar: the
 * marbles left. The lamp (the shell's tool): the jump to make lights up (in gold when it leads to one marble in the
 * goal; the marbles glow red when no way to one marble is left). The stars: one marble in the goal with no hint: 3; a
 * star less for missing the goal, a star less for any hint.
 */
#include <math.h>
#include <stdlib.h>
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

static const uint8_t BRASS[3] = {226, 182, 86};

static psgame G;
static int q, fr, made_w = -1, made_b = -1, made_goal = -2, made_small = -1;
static float bx, by, cx, cy, s;                 /* the board's corner, its middle, the spacing (pixels) */
static SDL_Texture *t_board, *t_marble, *t_tray, *t_small;
static int sel = -1;                            /* the marble picked */
static psmove opts[8];                          /* its jumps */
static int nopts;
static struct { int on, from, moved, fresh, target; float x, y, x0, y0; } ptr;   /* fresh: picked by this press */
static struct { int on, dropped; psmove m; Uint32 t0; } anim;
static struct { int on, gold, none; psmove m; } hint;

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static const psgame *board_of(int b)            /* each board's holes, made once */
{
    static psgame boards[PS_BOARDS];
    static int made[PS_BOARDS];
    b = b < 0 || b >= PS_BOARDS ? PS_ENGLISH : b;
    if (!made[b]) {
        ps_board(&boards[b], b);
        made[b] = 1;
    }
    return &boards[b];
}

/* a cell's centre in spacings from the board's middle, y down */
static void plane(int b, int c, float *x, float *y)
{
    const psgame *B = board_of(b);
    int gx = c % PS_SIDE, gy = c / PS_SIDE;
    if (B->tri) {
        *x = gx - gy / 2.0f;
        *y = (gy - 2 * (B->rows - 1) / 3.0f) * H;   /* the triangle's middle on the board's */
    } else {
        *x = gx - (B->cols - 1) / 2.0f;
        *y = gy - (B->rows - 1) / 2.0f;
    }
}

/* the cell nearest a point of the plane (it may be no hole) */
static int plane_cell(int b, float x, float y)
{
    const psgame *B = board_of(b);
    int gx, gy;
    if (B->tri) {
        gy = (int)lroundf(y / H + 2 * (B->rows - 1) / 3.0f);
        gx = (int)lroundf(x + gy / 2.0f);
    } else {
        gx = (int)lroundf(x + (B->cols - 1) / 2.0f);
        gy = (int)lroundf(y + (B->rows - 1) / 2.0f);
    }
    return gx < 0 || gy < 0 || gx >= PS_SIDE || gy >= PS_SIDE ? -1 : gy * PS_SIDE + gx;
}

/* the gutter's radius: round the farthest hole */
static float ring_r(int b)
{
    float r = 0;
    for (int c = 0; c < PS_CELLS; c++) {
        if (!board_of(b)->hole[c]) continue;
        float x, y;
        plane(b, c, &x, &y);
        r = fmaxf(r, hypotf(x, y));
    }
    return r + 0.69f;
}

static int slots(int b) { return (int)(2 * PI_F * ring_r(b) / PITCH); }
/* the spacing that fits the dish and its gutter across a plank of inner side `inner` */
static float spacing(int b, float inner) { return inner / (2 * (ring_r(b) + GUTTER + 0.3f)); }

/* the gutter's place k: from the top, round to the right */
static void slot_plane(int b, int k, float *x, float *y)
{
    float a = -PI_F / 2 + 2 * PI_F * (k + 0.5f) / slots(b);
    *x = ring_r(b) * cosf(a);
    *y = ring_r(b) * sinf(a);
}

static void hole_xy(int c, float *x, float *y)
{
    float px, py;
    plane(G.board, c, &px, &py);
    *x = cx + s * px;
    *y = cy + s * py;
}

static void slot_xy(int k, float *x, float *y)
{
    float px, py;
    slot_plane(G.board, k, &px, &py);
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
    memset(&hint, 0, sizeof hint);
    anim.on = 0;
}

static void start(int level)                    /* a level of the level screen */
{
    if (!ps_level(&G, level)) ps_new(&G, PS_ENGLISH);   /* never: every level is tested */
    calm();
}

static void start_random(int variant)           /* the die tab: a random position of that board */
{
    ps_random(&G, variant < 0 || variant >= PS_BOARDS ? PS_ENGLISH : variant, (uint32_t)ui_random(0x7fffffff) + 1u,
              0);
    calm();
}

static int status(void) { return ps_status(&G) == PS_ONE ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return ps_stars(&G); }

static int removed(void)                        /* the marbles in the gutter */
{
    int start = 0;
    for (int c = 0; c < PS_CELLS; c++) start += G.start[c];
    return start - G.pegs;
}

static void play(int from, int to, int dropped)
{
    if (!ps_play(&G, from, to)) { ui_sound(SND_DENY); return; }
    anim.m = G.hist[G.n - 1];
    anim.on = 1;
    anim.dropped = dropped;
    anim.t0 = ui_ms();
    pick(-1);
    memset(&hint, 0, sizeof hint);
    ui_sound(dropped ? SND_PLACE : SND_MOVE);
    ui_sound_later(SND_CAPTURE, LIFT_AT + FLY_MS);
    if (ps_status(&G) == PS_STUCK) ui_sound_later(SND_DENY, LIFT_AT + FLY_MS + 120);
    ui_animate();
}

static int undo(void)
{
    calm();
    return ps_undo(&G);
}

/* the jumps ("d6d4 b5d5"; "?" the hint followed) */
static int script(const char *moves)
{
    calm();
    while (*moves) {
        if (*moves == ' ') { moves++; continue; }
        if (*moves == '?') {
            psmove m;
            if (ps_hint(&G, &m) < 1 || !ps_play(&G, m.from, m.to)) return 0;
            moves++;
            continue;
        }
        char one[5] = {0};
        memcpy(one, moves, strlen(moves) < 4 ? strlen(moves) : 4);
        if (!ps_script(&G, one)) return 0;
        moves += strlen(one);
    }
    return 1;
}

static int busy(void) { return anim.on; }

static void ask_hint(void)
{
    if (busy() || ps_status(&G) != PS_PLAY) { ui_sound(SND_DENY); return; }
    psmove m;
    int r = ps_hint(&G, &m);
    memset(&hint, 0, sizeof hint);
    if (r > 0) {
        hint.on = 1;
        hint.gold = r == 1;
        hint.m = m;
        pick(-1);
        ui_sound(SND_TAP);
    } else {
        hint.none = r == 0;
        ui_sound(SND_DENY);
    }
    ui_animate();
}

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
/* the dish on the plank o (w pixels wide, its frame f): its floor a shade darker, the gutter round it (a groove, its
 * far side lit), the holes, a brass ring round the goal (-1: none) */
static void paint_dish(uint8_t *o, int w, int f, int b, int goal)
{
    const psgame *T = board_of(b);
    float sp = spacing(b, (float)(w - 2 * f)), mid = w / 2.0f, R = ring_r(b);
    for (int py = f; py < w - f; py++)
        for (int px = f; px < w - f; px++) {
            float x = (px + 0.5f - mid) / sp, y = (py + 0.5f - mid) / sp, r = hypotf(x, y), k = 1, ring = 0;
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
            int c = plane_cell(b, x, y);        /* the nearest hole */
            if (c >= 0 && T->hole[c]) {
                float hx, hy;
                plane(b, c, &hx, &hy);
                float d = hypotf(x - hx, y - hy);
                k *= art_recess((x - hx) * sp, (y - hy) * sp, HOLE * sp, 0.07f * sp, 0.42f, 0.24f, 0.2f);
                if (c == goal)                  /* the brass ring, a little lit on its upper left */
                    ring = clampf(0.05f * sp - fabsf(d - (HOLE + 0.1f)) * sp + 0.5f, 0, 1) *
                           (0.8f + 0.2f * clampf(-((x - hx) + (y - hy)) / (d + 1e-4f), -1, 1));
            }
            uint8_t *p = o + 4 * (py * w + px);
            for (int j = 0; j < 3; j++) {
                float v = clampf(p[j] * k + 0.5f, 0, 255);
                p[j] = (uint8_t)(v + (BRASS[j] - v) * clampf(ring, 0, 1));
            }
        }
}

static void paint_board(uint8_t *o, int w, int f, int b, int goal)
{
    int qq = (w - 2 * f) / 9;
    art_board_n(9, 9, qq, f, 0, o);
    paint_dish(o, w, f, b, goal);
}

static box last_box;

static void layout(box b)
{
    last_box = b;
    float side = fminf(b.w, b.h);
    q = (int)(side * (1 - 2 * FRAME) / 9);
    fr = (int)((side - 9 * q) / 2);
    int w = 9 * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    cx = bx + w / 2.0f;
    cy = by + w / 2.0f;
    s = spacing(G.board, 9.0f * q);
    if (made_w != w || made_b != G.board || made_goal != G.goal) {
        int same_size = made_w == w && made_b == G.board;
        made_w = w;
        made_b = G.board;
        made_goal = G.goal;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, G.board, G.goal);
        t_board = ui_tex(w, w, ui_buf(w, w));
        if (!same_size || !t_marble) {
            int d = (int)lroundf(MARBLE * s), t = (int)lroundf(TRAY * s);
            ui_free(&t_marble);
            art_counter(COUNTER_STONE, COLOUR, d, ui_buf(d, d));
            t_marble = ui_tex(d, d, ui_buf(d, d));
            ui_free(&t_tray);
            art_counter(COUNTER_STONE, COLOUR, t, ui_buf(t, t));
            t_tray = ui_tex(t, t, ui_buf(t, t));
        }
    }
    ui_board_used(bx, by, (float)w, (float)w);
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

static void draw_board(void)
{
    if (made_b != G.board || made_goal != G.goal) layout(last_box);   /* a new game: its board's picture */
    float w = 9.0f * q + 2 * fr, d = MARBLE * s, td = TRAY * s, x, y;
    ui_put(t_board, bx, by, w, w);
    float t = anim.on ? (float)(ui_ms() - anim.t0) : 0;
    if (anim.on && t >= LIFT_AT + FLY_MS && t >= JUMP_MS) anim.on = 0;
    if (anim.on) ui_animate();
    int st = ps_status(&G);
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
    if (hint.on && !anim.on && st == PS_PLAY) { /* the hint: the marble and where it goes */
        for (int k = 0; k < 2; k++) {
            hole_xy(k ? hint.m.to : hint.m.from, &x, &y);
            if (hint.gold) {
                ui_mark_gold(x, y, 1.6f * s);
                ui_mark_gold(x, y, 1.15f * s);
            } else ui_mark_select_round(x, y, 1.1f * s);
        }
    }
    int last = ps_last(&G);
    if (st == PS_ONE && !anim.on && ps_gold(&G)) {   /* the last marble in the goal: gold */
        hole_xy(last, &x, &y);
        ui_mark_gold(x, y, 2.2f * s);
    }
    int in_gutter = removed() - (anim.on && t < LIFT_AT + FLY_MS);
    for (int k = 0; k < in_gutter; k++) {       /* the marbles taken off, in their gutter */
        slot_xy(k, &x, &y);
        marble(x, y, td, 0);
    }
    int red = !anim.on && (st == PS_STUCK || hint.none);
    for (int c = 0; c < PS_CELLS; c++) {        /* the marbles at rest (the one moving, and the one held, apart) */
        if (!G.peg[c] || (anim.on && c == anim.m.to) || (ptr.on && ptr.moved && c == ptr.from)) continue;
        hole_xy(c, &x, &y);
        if (red) ui_mark_glow(x, y, 1.2f * s);  /* stuck, or no way left: undo */
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
    if (st != PS_PLAY || anim.on) return;
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

static void draw(void)
{
    draw_board();
    ui_tool(0, ICON_HINT, (hint.on ? UI_ON : 0) | (ps_status(&G) != PS_PLAY ? UI_DIM : 0));   /* the lamp */
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
    if (ui_tool_hit(x, y) == 0) {               /* the lamp */
        ask_hint();
        return;
    }
    if (ps_status(&G) != PS_PLAY) return;
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

/* ---- the bar: the marbles left ----------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_bar_count(b, t_small, G.pegs);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_counter(COUNTER_STONE, COLOUR, size, rgba);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void thumb_marble(uint8_t *rgba, int size, float x, float y, int d, int shadow)
{
    uint8_t *one = ui_buf(d, d);
    if (shadow) art_flat_shadow(d, one);
    else art_counter(COUNTER_STONE, COLOUR, d, one);
    int ox = (int)lroundf(x - d / 2.0f), oy = (int)lroundf(y - d / 2.0f);
    if (shadow) { ox += d / 20; oy += d / 12; }
    art_blend(rgba, size, size, one, d, d, ox, oy);
}

/* a game in a card: the dish, its goal's ring (goal: 1), the marbles, the gutter's */
static void game_pic(const psgame *T, int goal, int size, uint8_t *rgba)
{
    int v = T->board, f = (int)(size * 0.04f), qq = (size - 2 * f) / 9, w = 9 * qq + 2 * f, o = (size - w) / 2;
    uint8_t *board = calloc((size_t)w * w, 4);
    if (!board) return;
    paint_board(board, w, f, v, goal ? T->goal : -1);
    art_blend(rgba, size, size, board, w, w, o, o);
    free(board);
    float sp = spacing(v, 9.0f * qq), mid = o + w / 2.0f;
    int d = (int)lroundf(MARBLE * sp), td = (int)lroundf(TRAY * sp), out = 0;
    for (int c = 0; c < PS_CELLS; c++) out += T->start[c];
    out -= T->pegs;
    for (int pass = 1; pass >= 0; pass--) {     /* the shadows, then the marbles */
        for (int c = 0; c < PS_CELLS; c++) {
            if (!T->peg[c]) continue;
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

/* the menu's card: the English board in the middle of a game, the gutter half full; the picker's (the random play):
 * each board full but its classic empty hole */
static void thumb(int variant, int size, uint8_t *rgba)
{
    psgame T;
    ps_new(&T, variant < 0 ? PS_ENGLISH : variant);
    if (variant < 0)
        ps_script(&T, "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 e7e5 c3c5");
    game_pic(&T, 0, size, rgba);
}

/* a level's tile: its board, its marbles, its goal's ring */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    psgame T;
    if (ps_level(&T, level)) game_pic(&T, 1, size, rgba);
}

/* a chapter's tab: its marbles, more from one chapter to the next (3, 5, 7, 9, 12, 16) in rows of up to four */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int N[PS_CHAPTERS] = {3, 5, 7, 9, 12, 16};
    int n = N[chapter < 0 ? 0 : chapter >= PS_CHAPTERS ? PS_CHAPTERS - 1 : chapter];
    int per = n <= 3 ? 3 : n <= 9 ? 3 : 4, rows = (n + per - 1) / per, d = (int)(size / (per + 0.2f));
    uint8_t *one = calloc((size_t)d * d, 4);
    if (!one) return;
    art_counter(COUNTER_STONE, COLOUR, d, one);
    for (int i = 0; i < n; i++) {
        int r = i / per, k = r == rows - 1 ? n - r * per : per, col = i % per;
        float x = size / 2.0f + (col - (k - 1) / 2.0f) * d, y = size / 2.0f + (r - (rows - 1) / 2.0f) * d;
        art_blend(rgba, size, size, one, d, d, (int)lroundf(x - d / 2.0f), (int)lroundf(y - d / 2.0f));
    }
    free(one);
}

static const game pegsolitaire_game = {
    .id = "pegsolitaire", .order = 300, .variants = PS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = PS_LEVELS, .chapters = PS_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars,
};
REGISTER_GAME(pegsolitaire_game)
