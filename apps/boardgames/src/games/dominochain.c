/*
 * Falling Dominoes in RetroStone Board (the id stays "dominochain"), for one player: a board of the house wood in its
 * walnut frame, its gaps sunk (felt deep down, walnut walls), seen from above at a slight tilt so the pieces stand up
 * in three dimensions (the rules: dominochain_rules.c). Plain dominoes ivory with ebony pips; the trigger red lacquer
 * with gold pips; a delay domino amber with an hourglass; a long domino jade, twice as tall; a turn three small
 * dominoes on a curved walnut base; a splitter an ebony block with a brass cap; a stopper a walnut post with a brass
 * cap. The push: each piece tips over its edge in turn along the chain, a delay domino a beat late; a piece hit from
 * the side rocks and stays, a stopper too. Missed: the pieces left standing (or the trigger fallen too soon) glow red,
 * then everything stands up again. Solved: gold light runs down the chain to the trigger.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a piece: it lifts (the free squares it may go to dotted). Tap a free square: it goes there (a move); tap it
 * again: it turns (a move). Swipe a domino along its length, or use the two arrows (the shell's tools), to push it.
 * The lamp: the next move of a shortest way (or the push to make, its arrow in gold light; from a dead end it takes
 * back a move first). Undo takes back one move. The bar: the moves, and at its right end the par by its flag.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominochain.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "shell.h"

#define STEP_MS 150                             /* a step of the chain */
#define FALL_MS 260                             /* a piece tipping over */
#define WOBBLE_MS 300
#define MOVE_MS 240                             /* a piece carried to its new square */
#define MISS_MS 900                             /* the standing pieces glowing red */
#define RISE_MS 380
#define WIN_MS 1500
#define FRAME 0.3f
#define KZ 0.34f                                /* a unit of height up the screen, of a square (one tilt for all) */
#define TOP 0.45f                               /* room above the board for the pieces standing, of a square */
#define MINI_T 0.08f                            /* a turn's small dominoes: thick, wide, high (squares) */
#define MINI_W 0.28f
#define MINI_H 0.58f
#define BASE_H 0.07f                            /* a turn's walnut base: its height */
#define LIFT 0.16f                              /* a piece picked up */

enum { T_HINT, T_PUSH_A, T_PUSH_B, TOOLS };     /* the lamp, the push up or left, down or right */
enum { F_IVORY, F_RED, F_AMBER, F_JADE, FACES };

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static const uint8_t IVORY[3] = {238, 230, 210}, EBONY[3] = {46, 40, 38}, RED[3] = {182, 46, 36},
                     BRASS[3] = {214, 170, 84}, WALNUT[3] = {108, 72, 47}, AMBER[3] = {226, 158, 52},
                     JADE[3] = {54, 140, 104}, GOLD[3] = {244, 206, 110};

static dmc_game G;
static int serial, sel = -1, tool_down = -1, glow_c = -1, hint_way = -1, down_c = -1;
static float down_x, down_y;
static Uint32 glow_t0;
static dmc_trace tr;
static struct { int on, ok; Uint32 t0, end; } run;   /* a push on its way (end: when its last piece lies) */
static struct { int on; Uint32 t0; } rise, win;
static struct { int on, from, to; Uint32 t0; } carry;   /* a piece carried to its new square */

static box last_box;
static float bx0, by0, q, lift;                 /* the board's first square's top left, a square; a box's base */
static int made_q = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_face[FACES], *t_par;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a domino's face, w x h: its colour, a rim, a line across the middle, pips (a and b on its halves); hourglass: an
 * hourglass instead of the pips */
static void paint_face(const uint8_t body[3], const uint8_t pip[3], int a, int b, int hourglass, int w, int h,
                       uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float r = 0.16f * w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f, dx = fmaxf(r - px, px - (w - r)), dy = fmaxf(r - py, py - (h - r)), d;
            if (dx > 0 && dy > 0) d = r - hypotf(dx, dy);
            else d = fminf(fminf(px, w - px), fminf(py, h - py));
            float k = 1 + 0.04f * art_noise(x, y * 2) - (d < 0.07f * w ? 0.12f : 0);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(body[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * w + x), c, clampf(d + 0.5f, 0, 1));
        }
    if (hourglass) {                            /* two triangles point to point, a rim top and bottom */
        const float top[] = {0.22f * w, 0.2f * h, 0.78f * w, 0.2f * h, 0.5f * w, 0.5f * h};
        const float bot[] = {0.5f * w, 0.5f * h, 0.78f * w, 0.8f * h, 0.22f * w, 0.8f * h};
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                pzk_over(p, pip, 0.85f * (pzt_cover(top, 3, (float)x, (float)y) + pzt_cover(bot, 3, (float)x, (float)y)));
            }
        puz_ink_rect(o, w, h, 0.16f * w, 0.15f * h, 0.84f * w, 0.19f * h, pip, 1);
        puz_ink_rect(o, w, h, 0.16f * w, 0.81f * h, 0.84f * w, 0.85f * h, pip, 1);
        return;
    }
    puz_ink_rect(o, w, h, 0.18f * w, 0.49f * h, 0.82f * w, 0.51f * h + 1, pip, 0.8f);
    static const float P[7][2] = {{0.5f, 0.5f}, {0.28f, 0.25f}, {0.72f, 0.75f}, {0.28f, 0.75f}, {0.72f, 0.25f},
                                  {0.28f, 0.5f}, {0.72f, 0.5f}};
    static const int USE[7][7] = {{0}, {0}, {1, 2}, {0, 1, 2}, {1, 2, 3, 4}, {0, 1, 2, 3, 4}, {1, 2, 3, 4, 5, 6}};
    for (int half = 0; half < 2; half++) {
        int n = half ? b : a;
        for (int i = 0; i < n && i < 6; i++) {
            const float *p = P[n == 1 ? 0 : USE[n][i]];
            float cx = p[0] * w, cy = (half * 0.5f + p[1] * 0.5f) * h;
            pzk_ball(o, w, h, cx, cy, 0.075f * w + 0.4f, pip, 1, 0);
        }
    }
}

/* a small domino standing, its face on (the bars' token, the end card) */
static void paint_token(int size, uint8_t *o)
{
    int w = (int)(0.52f * size), h = (int)(0.9f * size);
    memset(o, 0, (size_t)size * size * 4);
    if (w < 3 || h < 3) return;
    uint8_t *f = malloc((size_t)w * h * 4);
    if (!f) return;
    paint_face(RED, GOLD, 3, 5, 0, w, h, f);
    art_blend(o, size, size, f, w, h, (size - w) / 2, (size - h) / 2);
    free(f);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_token(size, rgba);
}

/* the board: the plank in its frame f, w x h squares of s pixels, the gaps sunk */
static void paint_board(const dmc_state *m, int s, int f, uint8_t *o)
{
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    art_plank(m->w * s, m->h * s, f, o);
    float t = fmaxf(1, 0.025f * s);
    for (int c = 0; c < m->w * m->h; c++) {
        float x = f + c % m->w * s, y = f + c / m->w * s;
        if (m->t[c] == DMC_GAP) {               /* a gap sunk in the floor: felt deep down, its walnut walls */
            int gx = c % m->w, gy = c / m->w;
            int up = gy > 0 && m->t[c - m->w] == DMC_GAP, left = gx > 0 && m->t[c - 1] == DMC_GAP;
            int right = gx < m->w - 1 && m->t[c + 1] == DMC_GAP, down = gy < m->h - 1 && m->t[c + m->w] == DMC_GAP;
            art_felt(o, W, H, (int)x, (int)y, (int)x + s, (int)y + s);
            for (int py = (int)y; py < (int)y + s; py++)
                for (int px = (int)x; px < (int)x + s; px++) {
                    float u = (px - x + 0.5f) / s, v = (py - y + 0.5f) / s;
                    uint8_t *p = o + 4 * ((size_t)py * W + px);
                    for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.62f);
                    float lit = -1;
                    if (!up && v < 0.26f) lit = 0.55f + 0.6f * (1 - v / 0.26f);
                    else if (!left && u < 0.1f) lit = 0.45f;
                    else if (!right && u > 0.93f) lit = 0.7f;
                    if (lit > 0) {
                        uint8_t w3[3];
                        float g = 1 + 0.08f * art_noise(px, py * 3);
                        for (int i = 0; i < 3; i++) w3[i] = (uint8_t)clampf(WALNUT[i] * lit * g, 0, 255);
                        pzk_over(p, w3, 1);
                    }
                    if (!down && v > 0.95f) pzk_over(p, PUZ_INK, 0.55f);   /* the near lip's edge */
                    if (!up && v < 0.035f) pzk_over(p, PUZ_INK, 0.6f);     /* the far lip's edge */
                }
            continue;
        }
        art_tint_rect(o, W, H, x, y, x + s, y + t, PUZ_INK, 0.24f);   /* the squares ruled faintly */
        art_tint_rect(o, W, H, x, y, x + t, y + s, PUZ_INK, 0.24f);
        if (m->t[c] == DMC_TRIGGER) {           /* the trigger's square: a brass ring inlaid, the goal */
            pzk_arc(o, W, H, x + s / 2.0f, y + s / 2.0f, 0.36f * s, 0.46f * s, 0, 360, BRASS, 1);
            pzk_arc(o, W, H, x + s / 2.0f, y + s / 2.0f, 0.46f * s, 0.49f * s, 0, 360, PUZ_INK, 0.5f);
        }
    }
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    q = floorf(fminf(b.w / (G.s.w + 2 * FRAME), b.h / (G.s.h + 2 * FRAME + TOP)));
    bx0 = floorf(b.x + (b.w - G.s.w * q) / 2);
    by0 = floorf(b.y + (b.h - (G.s.h - TOP) * q) / 2);
    int s = (int)q;
    if (made_q != s || made_serial != serial) {
        made_q = s;
        made_serial = serial;
        int f = (int)(FRAME * s), W = G.s.w * s + 2 * f, H = G.s.h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m.start, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
        int fw = (int)(0.62f * s), fh = s;
        static const uint8_t *const BODY[FACES] = {IVORY, RED, AMBER, JADE}, *const PIP[FACES] = {EBONY, GOLD, EBONY,
                                                                                                IVORY};
        for (int i = 0; i < FACES; i++) {
            ui_free(&t_face[i]);
            paint_face(BODY[i], PIP[i], 2 + i, 4 - i % 3, i == F_AMBER, fw, fh, ui_buf(fw, fh));
            t_face[i] = ui_tex(fw, fh, ui_buf(fw, fh));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);                        /* the par's flag, as the other puzzles show it */
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    run.on = rise.on = win.on = carry.on = 0;
    tool_down = glow_c = hint_way = down_c = -1;
}

static void begin(void)
{
    serial++;
    calm();
    sel = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!dmc_level(&G, level)) dmc_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    dmc_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return G.done && !run.on ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static Uint32 run_len(void) { return (Uint32)(tr.steps * STEP_MS + FALL_MS); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return run.on || rise.on || carry.on || (win.on && now - win.t0 < WIN_MS);
}

static int pushable_at(int c)
{
    int t = c >= 0 ? G.s.t[c] : 0;
    return t == DMC_DOMINO || t == DMC_DELAY || t == DMC_LONG || t == DMC_TRIGGER;
}

static void push(int p)
{
    if (G.done || run.on || !dmc_can_push(&G.s, p)) { ui_sound(SND_DENY); return; }
    run.ok = dmc_push(&G, p, &tr);
    run.on = 1;
    run.t0 = ui_ms();
    run.end = run_len() + (run.ok ? 0 : MISS_MS);
    glow_c = hint_way = -1;
    sel = -1;
    ui_sound(SND_TAP);
    for (int i = 1; i < tr.n; i++) ui_sound_later(SND_MOVE, (int)(tr.fall[i].step * STEP_MS + FALL_MS * 0.7f));
    ui_animate();
}

static void run_over(void)                      /* the push's end: solved, or everything stands up again */
{
    run.on = 0;
    if (run.ok) {
        win.on = 1;
        win.t0 = ui_ms();
        ui_sound(SND_END);
    } else {
        rise.on = 1;
        rise.t0 = ui_ms();
        ui_sound(SND_DENY);
    }
    ui_animate();
}

static void move(int m)                         /* a move, the piece carried there */
{
    if (!dmc_move(&G, m)) { ui_sound(SND_DENY); return; }
    int a = DMC_FROM(m), b = DMC_TO(m);
    if (a != b) {
        carry.on = 1;
        carry.from = a;
        carry.to = b;
        carry.t0 = ui_ms();
        ui_sound_later(SND_PLACE, MOVE_MS);
    } else ui_sound(SND_MOVE);
    sel = b;
    hint_way = -1;
    ui_animate();
}

static void hint(void)
{
    if (G.done || run.on) { ui_sound(SND_DENY); return; }
    int m, p, h = dmc_hint(&G, &m, &p);
    if (!h) { ui_sound(SND_DENY); return; }
    G.hints++;
    if (h == 1) {
        move(m);
        glow_c = DMC_TO(m);
        glow_t0 = ui_ms();
    } else if (h == 2) {                        /* the push to make: its domino lifted, its arrow lit */
        sel = p / 4;
        hint_way = p % 4;
        ui_sound(SND_TAP);
    } else {
        dmc_undo(&G);
        sel = -1;
        ui_sound(SND_TAP);
    }
    ui_animate();
}

static int undo(void)
{
    calm();
    sel = -1;
    return dmc_undo(&G);
}

static int script(const char *moves)
{
    calm();
    sel = -1;
    dmc_state before = G.s;
    int ok = dmc_script(&G, moves);
    if (G.done) {                               /* the push that solved it, shown done */
        int p = dmc_solving_push(&G.s);
        dmc_run(&G.s, p, &tr);
        win.on = 1;
        win.t0 = ui_ms();
    }
    (void)before;
    return ok;
}

/* ---- the pieces in three dimensions ------------------------------------------------------------------------------ */
static void proj(float x, float y, float z, float *sx, float *sy)
{
    *sx = bx0 + x * q;
    *sy = by0 + (y - KZ * (z + lift)) * q;
}

typedef struct { float x, y, z; } v3;

/* a box standing on (cx, cy) (squares), its thin side along (fx, fy), w across and h high, tipped by phi (radians)
 * over its front bottom edge; its faces lit from above, its broad faces with tex (NULL: plain) */
static void draw_box(float cx, float cy, float fx, float fy, float t, float w, float h, float phi, const uint8_t rgb[3],
                     SDL_Texture *tex)
{
    float sx = -fy, sy = fx, cp = cosf(phi), sp = sinf(phi);
    v3 P[8];
    for (int i = 0; i < 8; i++) {
        float a = (i & 1 ? 0.5f : -0.5f) * t, b = (i & 2 ? 0.5f : -0.5f) * w, c = i & 4 ? h : 0;
        float a0 = a - t / 2, A = a0 * cp + c * sp, C = -a0 * sp + c * cp;
        P[i] = (v3){cx + fx * (t / 2 + A) + sx * b, cy + fy * (t / 2 + A) + sy * b, C};
    }
    static const int F[5][4] = {{1, 3, 7, 5}, {2, 0, 4, 6}, {3, 2, 6, 7}, {0, 1, 5, 4}, {4, 5, 7, 6}};
    float nf[3] = {fx * cp, fy * cp, -sp}, nz[3] = {fx * sp, fy * sp, cp}, ns[3] = {sx, sy, 0};
    float L[3] = {-0.3f, -0.25f, 0.92f}, view[3] = {0, KZ, 1};
    for (int k = 0; k < 5; k++) {
        float n[3];
        const float *src = k < 2 ? nf : k < 4 ? ns : nz, sg = k == 1 || k == 3 ? -1.0f : 1.0f;
        for (int i = 0; i < 3; i++) n[i] = sg * src[i];
        if (n[0] * view[0] + n[1] * view[1] + n[2] * view[2] <= 0.001f) continue;
        float lit = 0.8f + 0.22f * fmaxf(0, n[0] * L[0] + n[1] * L[1] + n[2] * L[2]);
        SDL_Vertex v[4];
        int broad = k < 2 && tex;
        for (int j = 0; j < 4; j++) {
            const v3 *p = &P[F[k][j]];
            float x, y;
            proj(p->x, p->y, p->z, &x, &y);
            int ci = F[k][j];
            Uint8 col[3];
            for (int i = 0; i < 3; i++) col[i] = (Uint8)clampf((broad ? 255 : rgb[i]) * lit, 0, 255);
            v[j] = (SDL_Vertex){{x, y}, {col[0], col[1], col[2], 255}, {ci & 2 ? 1.0f : 0.0f, ci & 4 ? 0.0f : 1.0f}};
        }
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ui.r, broad ? tex : NULL, v, 4, idx, 6);
    }
}

/* its shadow on the board: the box's corners cast down and to the right */
static void draw_shadow(float cx, float cy, float fx, float fy, float t, float w, float h, float phi)
{
    float sx = -fy, sy = fx, cp = cosf(phi), sp = sinf(phi), pts[16], keep = lift;
    lift = 0;
    for (int i = 0; i < 8; i++) {
        float a = (i & 1 ? 0.5f : -0.5f) * t, b = (i & 2 ? 0.5f : -0.5f) * w, c = (i & 4 ? h : 0) + keep;
        float a0 = a - t / 2, A = a0 * cp + c * sp, C = -a0 * sp + c * cp;
        float x = cx + fx * (t / 2 + A) + sx * b + 0.32f * C, y = cy + fy * (t / 2 + A) + sy * b + 0.22f * C;
        proj(x, y, 0, &pts[2 * i], &pts[2 * i + 1]);
    }
    lift = keep;
    int hull[9], nh = 0, first = 0;             /* the hull of the eight (gift wrapping) */
    for (int i = 1; i < 8; i++)
        if (pts[2 * i] < pts[2 * first] || (pts[2 * i] == pts[2 * first] && pts[2 * i + 1] < pts[2 * first + 1]))
            first = i;
    int p = first;
    do {
        hull[nh++] = p;
        int nx = (p + 1) % 8;
        for (int i = 0; i < 8; i++) {
            float cr = (pts[2 * nx] - pts[2 * p]) * (pts[2 * i + 1] - pts[2 * p + 1]) -
                       (pts[2 * nx + 1] - pts[2 * p + 1]) * (pts[2 * i] - pts[2 * p]);
            if (cr < 0) nx = i;
        }
        p = nx;
    } while (p != first && nh < 8);
    SDL_Vertex v[8];
    int idx[18], ni = 0;
    for (int i = 0; i < nh; i++) v[i] = (SDL_Vertex){{pts[2 * hull[i]], pts[2 * hull[i] + 1]}, {0, 0, 0, 56}, {0, 0}};
    for (int i = 1; i + 1 < nh; i++) { idx[ni++] = 0; idx[ni++] = i; idx[ni++] = i + 1; }
    if (ni) SDL_RenderGeometry(ui.r, NULL, v, nh, idx, ni);
}

/* how far a piece has tipped at t ms into the push (radians, 0 standing), the way it falls (*way), its wobble */
static float tipped(int c, float t, int *way, float *wob, int part, int parts)
{
    *wob = 0;
    if (!run.on && !rise.on && !win.on) return 0;
    for (int i = 0; i < tr.n; i++)
        if (tr.fall[i].c == c) {
            *way = tr.fall[i].way;
            float t0 = tr.fall[i].step * STEP_MS + (float)part * STEP_MS / parts;
            float f = clampf((t - t0) / FALL_MS, 0, 1);
            float a = (PUZ_PI / 2 - 0.06f) * f * f;
            if (rise.on) a *= 1 - puz_ease((ui_ms() - rise.t0) / (float)RISE_MS);
            return a;
        }
    for (int i = 0; i < tr.nwob; i++)
        if (tr.wob[i].c == c) {
            float f = (t - tr.wob[i].step * STEP_MS) / WOBBLE_MS;
            if (f > 0 && f < 1) { *way = tr.wob[i].way; *wob = 0.16f * sinf(PUZ_PI * 2 * f) * (1 - f); }
        }
    return 0;
}

typedef struct { float key; int c, part; } item;

static int by_key(const void *a, const void *b)
{
    float d = ((const item *)a)->key - ((const item *)b)->key;
    return d < 0 ? -1 : d > 0;
}

/* a turn's curve: the corner's point in the square (ox, oy) and the angles from side `in` (a0) to the other (a1) */
static void turn_arc(int corner, int in, float *ox, float *oy, float *a0, float *a1)
{
    static const float CX[4] = {1, 1, 0, 0}, CY[4] = {0, 1, 1, 0};
    static const float MX[4] = {0.5f, 1, 0.5f, 0}, MY[4] = {0, 0.5f, 1, 0.5f};
    int out = in == corner ? (corner + 1) & 3 : corner;
    *ox = CX[corner];
    *oy = CY[corner];
    *a0 = atan2f(MY[in] - *oy, MX[in] - *ox);
    *a1 = atan2f(MY[out] - *oy, MX[out] - *ox);
    if (*a1 - *a0 > PUZ_PI) *a1 -= 2 * PUZ_PI;
    if (*a0 - *a1 > PUZ_PI) *a1 += 2 * PUZ_PI;
}

/* where a piece stands now (squares): its square's middle, or on its way to it (and how high it is carried) */
static void piece_at(int c, float *x, float *y, float *up)
{
    *x = c % G.s.w + 0.5f;
    *y = c / G.s.w + 0.5f;
    *up = c == sel && !run.on && !rise.on && !win.on ? LIFT : 0;
    if (carry.on && c == carry.to) {
        float f = clampf((ui_ms() - carry.t0) / (float)MOVE_MS, 0, 1), e = puz_ease(f);
        *x = carry.from % G.s.w + 0.5f + (c % G.s.w - carry.from % G.s.w) * e;
        *y = carry.from / G.s.w + 0.5f + (c / G.s.w - carry.from / G.s.w) * e;
        *up = LIFT + 0.35f * sinf(PUZ_PI * f);
    }
}

static void draw_turn_base(float px, float py, int corner)
{
    enum { N = 12 };
    float ox, oy, a0, a1;
    turn_arc(corner, corner, &ox, &oy, &a0, &a1);
    float gx = px - 0.5f + ox, gy = py - 0.5f + oy, r0 = 0.36f, r1 = 0.64f;
    SDL_Vertex v[2 * (N + 1)];
    int idx[6 * N], ni = 0;
    for (int i = 0; i < N; i++) {
        int k = 2 * i, q6[6] = {k, k + 1, k + 3, k, k + 3, k + 2};
        for (int j = 0; j < 6; j++) idx[ni++] = q6[j];
    }
    for (int pass = 0; pass < 4; pass++) {      /* the shadow, the inner wall, the outer wall, the top */
        float r = pass == 1 ? r0 : r1, lit = pass == 1 ? 0.62f : 0.78f;
        for (int i = 0; i <= N; i++) {
            float u = 0.04f + 0.92f * i / N, a = a0 + (a1 - a0) * u, ca = cosf(a), sa = sinf(a), x0, y0, x1, y1;
            SDL_Color col = {0, 0, 0, 60};
            if (pass == 0) {
                float keep = lift;
                lift = 0;
                proj(gx + (r0 - 0.02f) * ca + 0.04f, gy + (r0 - 0.02f) * sa + 0.04f, 0, &x0, &y0);
                proj(gx + (r1 + 0.02f) * ca + 0.04f, gy + (r1 + 0.02f) * sa + 0.04f, 0, &x1, &y1);
                lift = keep;
            } else {
                if (pass < 3) {
                    proj(gx + r * ca, gy + r * sa, 0, &x0, &y0);
                    proj(gx + r * ca, gy + r * sa, BASE_H, &x1, &y1);
                } else {
                    proj(gx + r0 * ca, gy + r0 * sa, BASE_H, &x0, &y0);
                    proj(gx + r1 * ca, gy + r1 * sa, BASE_H, &x1, &y1);
                    lit = 1.12f;
                }
                col = (SDL_Color){(Uint8)clampf(WALNUT[0] * lit, 0, 255), (Uint8)clampf(WALNUT[1] * lit, 0, 255),
                                  (Uint8)clampf(WALNUT[2] * lit, 0, 255), 255};
            }
            v[2 * i] = (SDL_Vertex){{x0, y0}, col, {0, 0}};
            v[2 * i + 1] = (SDL_Vertex){{x1, y1}, col, {0, 0}};
        }
        SDL_RenderGeometry(ui.r, NULL, v, 2 * (N + 1), idx, ni);
    }
}

/* a cap on a block or a post: a brass disc on its top */
static void cap(float x, float y, float z, float d)
{
    float sx, sy;
    proj(x, y, z, &sx, &sy);
    pzk_rgba br = {BRASS[0], BRASS[1], BRASS[2], 255}, dk = {90, 62, 24, 255};
    pzk_dot(sx, sy, d * q * 1.15f, 1, dk, 0);
    pzk_dot(sx, sy, d * q, 1, br, 0);
}

static void draw_piece(int c, int part, float t, int shadow)
{
    int p = G.s.t[c], o = G.s.o[c], way = -1;
    float x, y, up, wob;
    piece_at(c, &x, &y, &up);
    lift = up;
    if (p == DMC_DOMINO || p == DMC_LONG || p == DMC_DELAY || p == DMC_TRIGGER) {
        float phi = tipped(c, t, &way, &wob, 0, 1);
        if (way < 0) way = o ? 1 : 2;
        float fx = (float)DX[way], fy = (float)DY[way], h = p == DMC_LONG ? 1.9f : 0.92f;
        if (wob != 0) phi = wob;
        const uint8_t *body = p == DMC_TRIGGER ? RED : p == DMC_DELAY ? AMBER : p == DMC_LONG ? JADE : IVORY;
        SDL_Texture *face = t_face[p == DMC_TRIGGER ? F_RED : p == DMC_DELAY ? F_AMBER : p == DMC_LONG ? F_JADE : F_IVORY];
        if (shadow) draw_shadow(x - fx * 0.1f, y - fy * 0.1f, fx, fy, 0.2f, 0.62f, h, phi);
        else draw_box(x - fx * 0.1f, y - fy * 0.1f, fx, fy, 0.2f, 0.62f, h, phi, body, face);
    } else if (p == DMC_TURN) {
        int in = o;                             /* standing: as from its first side; fallen: from where it was hit */
        if (run.on || rise.on || win.on)
            for (int i = 0; i < tr.n; i++)
                if (tr.fall[i].c == c) in = tr.fall[i].way == o ? (o + 1) & 3 : o;
        float ox, oy, a0, a1;
        turn_arc(o, in, &ox, &oy, &a0, &a1);
        float u = 0.2f + 0.3f * part, a = a0 + (a1 - a0) * u, sg = a1 > a0 ? 1.0f : -1.0f;
        float mx = x - 0.5f + ox + 0.5f * cosf(a), my = y - 0.5f + oy + 0.5f * sinf(a), fx = -sinf(a) * sg,
              fy = cosf(a) * sg;
        float phi = tipped(c, t, &way, &wob, part, 3);
        if (wob != 0) phi = wob;
        if (!shadow && part == 0) draw_turn_base(x, y, o);
        if (shadow) draw_shadow(mx - fx * MINI_T / 2, my - fy * MINI_T / 2, fx, fy, MINI_T, MINI_W, MINI_H, phi);
        else {
            lift = up + BASE_H;
            draw_box(mx - fx * MINI_T / 2, my - fy * MINI_T / 2, fx, fy, MINI_T, MINI_W, MINI_H, phi, IVORY, NULL);
        }
    } else if (p == DMC_SPLIT) {
        float phi = tipped(c, t, &way, &wob, 0, 1);
        if (way < 0) way = 2;
        float fx = (float)DX[way], fy = (float)DY[way];
        if (wob != 0) phi = wob;
        if (shadow) draw_shadow(x - fx * 0.21f, y - fy * 0.21f, fx, fy, 0.42f, 0.42f, 0.42f, phi);
        else {
            draw_box(x - fx * 0.21f, y - fy * 0.21f, fx, fy, 0.42f, 0.42f, 0.42f, phi, EBONY, NULL);
            if (phi < 0.3f) {                   /* its brass cap and the two ways it sends the chain */
                float sx, sy;
                cap(x, y, 0.42f, 0.16f);
                proj(x, y, 0.42f, &sx, &sy);
                pzk_rgba br = {BRASS[0], BRASS[1], BRASS[2], 255};
                pzk_line(sx - 0.17f * q, sy, sx + 0.17f * q, sy, 0.05f * q, 1, br, 0);
                pzk_line(sx, sy - 0.17f * q * KZ * 2, sx, sy + 0.17f * q * KZ * 2, 0.05f * q, 1, br, 0);
            }
        }
    } else if (p == DMC_STOPPER) {              /* a walnut post, a brass cap: it never falls, it rocks */
        tipped(c, t, &way, &wob, 0, 1);
        if (way < 0) way = 2;
        float fx = (float)DX[way], fy = (float)DY[way];
        if (shadow) draw_shadow(x - fx * 0.23f, y - fy * 0.23f, fx, fy, 0.46f, 0.46f, 0.5f, wob * 0.4f);
        else {
            draw_box(x - fx * 0.23f, y - fy * 0.23f, fx, fy, 0.46f, 0.46f, 0.5f, wob * 0.4f, WALNUT, NULL);
            cap(x + fx * 0.25f * wob, y + fy * 0.25f * wob, 0.5f, 0.24f);
        }
    }
    lift = 0;
}

static void glow_cell(int c, float d, float a)
{
    float sx, sy;
    proj(c % G.s.w + 0.5f, c / G.s.w + 0.5f, 0.3f, &sx, &sy);
    ui_mark_gold_a(sx, sy, d * q, a);
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    float t = run.on ? (float)(int)(now - run.t0) : 1e9f;
    if (run.on && t >= run.end) { run_over(); t = 1e9f; }
    if (rise.on && now - rise.t0 >= RISE_MS) rise.on = 0;
    if (carry.on && now - carry.t0 >= MOVE_MS) carry.on = 0;
    int f = (int)(FRAME * q), wh = G.s.w * G.s.h, still = !run.on && !rise.on && !win.on && !G.done;
    ui_put(t_board, bx0 - f, by0 - f, G.s.w * q + 2 * f, G.s.h * q + 2 * f);
    if (still && sel >= 0 && dmc_movable(&G.s, sel))   /* where the piece lifted may go */
        for (int c = 0; c < wh; c++)
            if (G.s.t[c] == DMC_WOOD) ui_mark_dot(bx0 + (c % G.s.w + 0.5f) * q, by0 + (c / G.s.w + 0.5f) * q, 0.26f * q);
    if (still && sel >= 0) glow_cell(sel, 1.25f, 0.7f);
    if (glow_c >= 0) {                          /* a hint's move: its square in gold light */
        float g = (now - glow_t0) / 1600.0f;
        if (g < 1) { glow_cell(glow_c, 1.5f, 1 - g); ui_animate(); }
        else glow_c = -1;
    }
    item it[3 * DMC_CELLS];
    int n = 0;
    for (int c = 0; c < wh; c++) {
        int p = G.s.t[c];
        if (p < DMC_DOMINO) continue;
        int parts = p == DMC_TURN ? 3 : 1;
        float px, py, up, wob;
        piece_at(c, &px, &py, &up);
        for (int k = 0; k < parts; k++) {
            int w = -1;
            float phi = tipped(c, t, &w, &wob, k, parts), y = py;
            if (phi > 0.2f && w >= 0) y += 0.5f * DY[w] * sinf(phi);   /* a piece lying down: where it lies */
            it[n++] = (item){y + 0.001f * px + (phi > 1.2f ? -0.4f : 0) + (c == carry.to && carry.on ? 5 : 0), c, k};
        }
    }
    qsort(it, (size_t)n, sizeof *it, by_key);
    for (int i = 0; i < n; i++) draw_piece(it[i].c, it[i].part, t, 1);
    for (int i = 0; i < n; i++) draw_piece(it[i].c, it[i].part, t, 0);
    if (run.on || rise.on || win.on || carry.on) ui_animate();
    /* missed: the pieces left standing glow red (the trigger too, when it fell before the end) */
    int late = (run.on && !run.ok && t > run.end - MISS_MS) || (rise.on && !run.ok);
    if (late) {
        float g = rise.on ? (now - rise.t0) / (float)RISE_MS : 0;
        for (int c = 0; c < wh; c++) {
            int p = G.s.t[c];
            int wrong = (p >= DMC_DOMINO && p != DMC_STOPPER && !tr.fallen[c]) || (p == DMC_TRIGGER && tr.fallen[c]);
            if (!wrong) continue;
            float sx, sy;
            proj(c % G.s.w + 0.5f, c / G.s.w + 0.5f, 0.4f, &sx, &sy);
            ui_mark_glow(sx, sy, 1.1f * q * (1 - 0.5f * g));
        }
    }
    if (win.on) {                               /* gold light down the fallen chain, the trigger brightest */
        float wt = (float)(int)(now - win.t0);
        if (wt < WIN_MS) {
            for (int i = 0; i < tr.n; i++) {
                int c = tr.fall[i].c;
                float a = puz_pulse(wt, 50.0f * tr.fall[i].step, 520);
                if (a > 0) glow_cell(c, G.s.t[c] == DMC_TRIGGER ? 2.2f : 1.3f, a);
            }
            ui_animate();
        }
    }
    /* the tools: the lamp, then the two pushes along the piece lifted */
    ui_tool(T_HINT, ICON_HINT, (G.done ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
    int can = still && pushable_at(sel), o = can ? G.s.o[sel] : 0;
    for (int k = 0; k < 2; k++) {
        int way = o ? (k ? 1 : 3) : (k ? 2 : 0), how = UI_TINT | (can ? 0 : UI_DIM);
        if (can && way == hint_way) how |= UI_LIT;
        if (tool_down == T_PUSH_A + k) how |= UI_ON;
        ui_tool_paint(T_PUSH_A + k, swp_arrow, way, 0.62f, how);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)            /* the square under the finger (a piece standing: its top too) */
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    int cx = (int)floorf(fx), cy = (int)floorf(fy);
    if (cx < 0 || cx >= G.s.w) return -1;
    if (cy >= 0 && cy < G.s.h && G.s.t[cx + cy * G.s.w] >= DMC_DOMINO) return cx + cy * G.s.w;
    int up = (int)floorf(fy + KZ * 0.9f);       /* a tap on a standing piece's upper part */
    if (up >= 0 && up < G.s.h && G.s.t[cx + up * G.s.w] >= DMC_DOMINO) return cx + up * G.s.w;
    return cy >= 0 && cy < G.s.h ? cx + cy * G.s.w : -1;
}

static void tap_cell(int c)
{
    if (G.done) return;
    if (c < 0) { sel = -1; return; }
    if (sel >= 0 && c == sel) {                 /* tapped again: turned (a move), or put down */
        if (dmc_can_move(&G.s, DMC_MOVE(c, c))) move(DMC_MOVE(c, c));
        else sel = -1;
        return;
    }
    if (G.s.t[c] >= DMC_DOMINO) {               /* another piece: lifted */
        sel = c;
        hint_way = -1;
        ui_sound(SND_TAP);
        return;
    }
    if (sel >= 0 && G.s.t[c] == DMC_WOOD) {
        if (dmc_movable(&G.s, sel)) move(DMC_MOVE(sel, c));
        else { ui_sound(SND_DENY); sel = -1; }
        return;
    }
    sel = -1;
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    down_c = tool_down < 0 ? cell_at(x, y) : -1;
    down_x = x;
    down_y = y;
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (busy()) { tool_down = down_c = -1; return; }
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != k) return;
        if (k == T_HINT) { hint(); return; }
        if (!pushable_at(sel) || G.done) { ui_sound(SND_DENY); return; }
        int o = G.s.o[sel], way = o ? (k == T_PUSH_B ? 1 : 3) : (k == T_PUSH_B ? 2 : 0);
        push(DMC_PUSH(sel, way));
        return;
    }
    float dx = x - down_x, dy = y - down_y;
    if (down_c >= 0 && hypotf(dx, dy) > 0.45f * q && pushable_at(down_c) && !G.done) {   /* a swipe: the push */
        int way = swp_way(dx, dy);
        if ((way & 1) == G.s.o[down_c]) push(DMC_PUSH(down_c, way));
        else ui_sound(SND_DENY);
        down_c = -1;
        return;
    }
    down_c = -1;
    tap_cell(cell_at(x, y));
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), G.done && !run.on && G.moves <= par());
}

/* ---- the shell's pictures: the board and its pieces seen from above ---------------------------------------------- */
static void paint_map(const dmc_state *m, int size, uint8_t *rgba)
{
    int s = (int)(size * 0.94f / (fmaxf((float)m->w, (float)m->h) + 2 * FRAME)), f = (int)(FRAME * s);
    if (s < 3) return;
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, s, f, o);
    for (int c = 0; c < m->w * m->h; c++) {     /* each piece from above: a bar across its way, a curve, a block */
        int t = m->t[c], d = m->o[c];
        float x = f + c % m->w * s, y = f + c / m->w * s, cx = x + s / 2.0f, cy = y + s / 2.0f;
        if (t == DMC_DOMINO || t == DMC_LONG || t == DMC_DELAY || t == DMC_TRIGGER) {
            const uint8_t *col = t == DMC_TRIGGER ? RED : t == DMC_DELAY ? AMBER : t == DMC_LONG ? JADE : IVORY;
            float hw = d ? 0.11f * s : 0.34f * s, hh = d ? 0.34f * s : 0.11f * s;
            float ax = hw > hh ? hw - hh : 0, ay = hh > hw ? hh - hw : 0;
            pzk_stroke(o, W, H, cx - ax, cy - ay, cx + ax, cy + ay, fminf(hw, hh) + 0.5f, col, 1);
        } else if (t == DMC_TURN) {
            static const float CX[4] = {1, 1, 0, 0}, CY[4] = {0, 1, 1, 0};
            static const float A0[4] = {180, 90, 0, 270};   /* the quarter inside the square, from its corner */
            float a0 = A0[d & 3];
            pzk_arc(o, W, H, x + CX[d & 3] * s, y + CY[d & 3] * s, 0.42f * s, 0.58f * s, a0, a0 + 90, IVORY, 1);
        } else if (t == DMC_SPLIT) {
            pzk_ball(o, W, H, cx, cy, 0.26f * s, EBONY, 1, 0.5f);
            pzk_ball(o, W, H, cx, cy, 0.1f * s, BRASS, 1, 0);
        } else if (t == DMC_STOPPER) {
            pzk_ball(o, W, H, cx, cy, 0.28f * s, WALNUT, 1, 0.6f);
            pzk_ball(o, W, H, cx, cy, 0.15f * s, BRASS, 1, 0.6f);
        }
    }
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    dmc_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant < 0) dmc_level(t, 70);
    else dmc_random(t, variant, 7u + (uint32_t)variant);
    paint_map(&t->m.start, size, rgba);
    free(t);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    dmc_game *t = malloc(sizeof *t);
    if (t && dmc_level(t, level)) paint_map(&t->m.start, size, rgba);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[DMC_CHAPTERS] = {
        "hS2|..v|..Y", "#Y#|#v#|.+.|#v#", "hdhX", "hl#hX", "S+h|#v.|.Y.",
    };
    dmc_state m;
    if (chapter >= 0 && chapter < DMC_CHAPTERS && dmc_parse(&m, ICON[chapter])) paint_map(&m, size, rgba);
}

static const game dominochain_game = {
    .id = "dominochain", .order = 459, .variants = DMC_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = DMC_LEVELS, .chapters = DMC_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(dominochain_game)
