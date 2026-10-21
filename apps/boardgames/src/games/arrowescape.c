/*
 * Clear the Arrows in RetroStone Board, for one player: arrows drawn as thin lines of ink straight on the table (the
 * rules: arrowescape_rules.c), no board and no squares behind them: dark ink on the light theme's ivory, cream on the
 * dark theme's green. Each arrow runs square to square through the squares' middles, its bends tight rounded corners
 * (a quarter of a square), a small filled triangle at its head. The big boards fill the room and can be zoomed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap an arrow. Its way clear: it draws back a hair and slithers off, faster and faster, its head straight out and its
 * body following the head's path square by square, round its corners; it fades as it leaves the board's room, a spark
 * where its head crosses the edge. Its way blocked: its head slides up to the arrow in the way, knocks it and springs
 * back; both flash red (a mistake). The shell's tools: the hint (an arrow that can go now, the one freeing the most:
 * gold, nudging forward), and while zoomed, the whole board back. Two fingers pinch to zoom (up to cells a tenth of
 * the screen's short side) and slide it; one finger slides a zoomed board; the keys + and - zoom, 0 fits it. Undo
 * brings back the last arrow, all the way to the start (mistakes stay). The bar: the arrows left, the mistakes.
 * Cleared: a gold light runs out from the middle over where the arrows were. Everything moves on the shell's clock
 * (ui_ms): the state changes at the tap, the pictures follow.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/arrowescape.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define MARGIN 0.4f                             /* room round the board, of a square (heads leaving, a little air) */
#define LINE 0.17f                              /* the line's width, of a square */
#define TURN 0.25f                              /* a bend's radius, of a square */
#define TAIL 0.15f                              /* the line's end behind its last square's middle */
#define TIP 0.4f                                /* the head's tip ahead of its square's middle */
#define BASE 0.04f                              /* its base behind it, where the line ends */
#define HALF 0.21f                              /* half the head's width */
#define LIFT_MS 70                              /* an arrow drawing back before it goes */
#define BUMP_BACK_MS 440                        /* a bumped arrow springing back */
#define WIN_MS 1500
#define HINT_TOOL 0
#define FIT_TOOL 1
#define TRACK (ARX_MAXLEN + ARX_MAXW + ARX_MAXH + 8)

static const uint8_t GOLD[3] = {226, 170, 52}, RED[3] = {214, 58, 40}, PAPER[3] = {248, 242, 228},
                     PAPER_INK[3] = {54, 44, 36};

static arx_game G;
static int hint_piece = -1, tool_down = -1, arr_game = -1, game_serial;   /* (game_serial: a new board each start) */
static struct {                                 /* each arrow's motion (ms on the shell's clock; on: running) */
    uint8_t fly, bump, wob;
    Uint32 fly_t0, bump_t0, wob_t0;
    float fly_d, fly_edge;                      /* how far it slithers (squares), where its head leaves the board */
    Uint32 fly_ms, bump_hit;                    /* its flight; when the bump lands */
    float bump_gap;
    int wob_dir;                                /* the way it was knocked */
} mo[ARX_MAXP];
static struct { int on; Uint32 t0; } win;

/* the view: the board fitted to the room (q0 a square), zoomed by z round the room's middle and slid by (px, py) */
static box last_box;
static float q0, z = 1, pan_x, pan_y, q, bx0, by0, hs;   /* hs: the lines' and heads' square (no bigger than 0.6 unit) */
static SDL_Texture *t_head, *t_miss;
static int head_px = -1, made_small = -1;

/* the pointers: the mouse (id -1) and up to two fingers; a tap unless they moved or pinched */
typedef struct { int on, id; float x, y, x0, y0; } pointer;
static pointer pt[2];
static struct { int on; float d0, z0, px0, py0; } pinch;
static int moved, pinched;
static float pan_px0, pan_py0;

/* ---- an arrow's path: its squares from its tail to its head, then straight on past the board's edge -------------
 * A point at t (0: its tail square's middle, len - 1 its head's, on past it): straight through a square whose way in
 * and way out agree; where they do not, straight to TURN from the middle, a quarter circle of radius TURN, straight
 * on out. */
typedef struct { int n, x[TRACK], y[TRACK]; } track;

static void track_of(const arx_map *m, int i, track *t)
{
    int cells[ARX_MAXLEN], n = arx_cells(m, i, cells);
    const arx_piece *p = &m->p[i];
    t->n = 0;
    for (int k = n - 1; k >= 0; k--) {
        t->x[t->n] = cells[k] % m->w;
        t->y[t->n++] = cells[k] / m->w;
    }
    for (int j = 1; t->n < TRACK; j++) {
        t->x[t->n] = p->x + j * ARX_DX[(int)p->dir];
        t->y[t->n++] = p->y + j * ARX_DY[(int)p->dir];
    }
}

/* square k's way in and way out (a step: -1, 0, 1 each way); 1 when it bends */
static int ways(const track *t, int k, int *ix, int *iy, int *ox, int *oy)
{
    *ox = k + 1 < t->n ? t->x[k + 1] - t->x[k] : t->x[k] - t->x[k - 1];
    *oy = k + 1 < t->n ? t->y[k + 1] - t->y[k] : t->y[k] - t->y[k - 1];
    *ix = k > 0 ? t->x[k] - t->x[k - 1] : *ox;
    *iy = k > 0 ? t->y[k] - t->y[k - 1] : *oy;
    return *ix != *ox || *iy != *oy;
}

/* the point at t, squares from the board's top left (the middle of square (x, y): x + 0.5, y + 0.5) */
static void track_at(const track *t, float s, float *px, float *py)
{
    int k = (int)floorf(s + 0.5f);
    if (k < 0) k = 0;
    if (k > t->n - 1) k = t->n - 1;
    int ix, iy, ox, oy, bent = ways(t, k, &ix, &iy, &ox, &oy);
    float cx = t->x[k] + 0.5f, cy = t->y[k] + 0.5f, u = s - (k - 0.5f);
    if (!bent || u < 0 || u > 1) {              /* straight (and past either end) */
        float d = s - k;
        *px = cx + d * (d < 0 ? ix : ox);
        *py = cy + d * (d < 0 ? iy : oy);
        return;
    }
    float l1 = 0.5f - TURN, la = PUZ_PI / 2 * TURN, L = 2 * l1 + la, a = u * L;
    if (a < l1) {
        *px = cx - (0.5f - a) * ix;
        *py = cy - (0.5f - a) * iy;
    } else if (a < l1 + la) {                   /* round the corner, its middle TURN in from both ways */
        float f = (a - l1) / TURN, kx = cx - TURN * ix + TURN * ox, ky = cy - TURN * iy + TURN * oy;
        *px = kx + TURN * (-ox * cosf(f) + ix * sinf(f));
        *py = ky + TURN * (-oy * cosf(f) + iy * sinf(f));
    } else {
        *px = cx + (TURN + a - l1 - la) * ox;
        *py = cy + (TURN + a - l1 - la) * oy;
    }
}

/* the points along the line from t0 to t1: a square's ends where it runs straight, 14 points round a bend; n */
static int track_points(const track *t, float t0, float t1, float *xs, float *ys, int cap)
{
    int n = 0;
    float s = t0;
    while (n < cap) {
        track_at(t, s, &xs[n], &ys[n]);
        n++;
        if (s >= t1) break;
        int k = (int)floorf(s + 0.5f), ix, iy, ox, oy;
        if (k < 0) k = 0;
        if (k > t->n - 1) k = t->n - 1;
        float next = k + 0.5f;                  /* the square's far edge */
        if (ways(t, k, &ix, &iy, &ox, &oy)) next = fminf(next, s + 1.0f / 14);
        if (next <= s + 1e-4f) next = s + 1.0f / 14;
        s = fminf(next, t1);
    }
    return n;
}

static void head_xy(float cx, float cy, float s, int dir, float *xy)   /* the head's triangle, s pixels a square */
{
    static const float A[3] = {TIP, -BASE, -BASE}, C[3] = {0, -HALF, HALF};
    for (int i = 0; i < 3; i++) {
        xy[2 * i] = cx + (A[i] * ARX_DX[dir] - C[i] * ARX_DY[dir]) * s;
        xy[2 * i + 1] = cy + (A[i] * ARX_DY[dir] + C[i] * ARX_DX[dir]) * s;
    }
}

/* ---- pictures (the menu's card, the level tiles, the tabs): a sheet of paper, the arrows in ink on it ------------- */
/* arrow i of m in ink over the picture o (W x H), the board's first square at (ox, oy), s pixels a square, its line
 * thick times as wide as on the board */
static void paint_arrow(uint8_t *o, int W, int H, const arx_map *m, int i, float ox, float oy, float s,
                        const uint8_t ink[3], float thick)
{
    track t;
    track_of(m, i, &t);
    float xs[ARX_MAXLEN * 16 + 8], ys[ARX_MAXLEN * 16 + 8], r = fmaxf(0.45f, thick * LINE / 2 * s), xy[6];
    int n = track_points(&t, -TAIL, m->p[i].len - 1 - BASE, xs, ys, ARX_MAXLEN * 16 + 8);
    for (int k = 1; k < n; k++)
        pzk_stroke(o, W, H, ox + xs[k - 1] * s, oy + ys[k - 1] * s, ox + xs[k] * s, oy + ys[k] * s, r, ink, 1);
    float hx, hy;
    track_at(&t, m->p[i].len - 1, &hx, &hy);
    head_xy(ox + hx * s, oy + hy * s, s * (thick > 1 ? 1.2f : 1), m->p[i].dir, xy);
    for (int y = (int)fmaxf(0, oy + (hy - 0.6f) * s - 1); y < H && y < oy + (hy + 0.6f) * s + 1; y++)
        for (int x = (int)fmaxf(0, ox + (hx - 0.6f) * s - 1); x < W && x < ox + (hx + 0.6f) * s + 1; x++)
            pzk_over(o + 4 * ((size_t)y * W + x), ink, pzt_cover(xy, 3, (float)x, (float)y));
}

/* a board on its sheet of paper in a picture size x size */
static void paint_mini(const arx_map *m, int size, uint8_t *rgba)
{
    float span = fmaxf((float)m->w, (float)m->h) + 2 * MARGIN, s = size * 0.94f / span;
    float pw = (m->w + 2 * MARGIN) * s, ph = (m->h + 2 * MARGIN) * s, x0 = (size - pw) / 2, y0 = (size - ph) / 2;
    art_roundrect_put(rgba, size, size, (int)x0, (int)y0, (int)pw, (int)ph, 0.06f * fminf(pw, ph), PAPER, 1);
    for (int i = 0; i < m->n; i++)
        paint_arrow(rgba, size, size, m, i, x0 + MARGIN * s, y0 + MARGIN * s, s, PAPER_INK, s < 12 ? 1.4f : 1);
}

/* the bar's mistake: a red disc, an ivory cross */
static void paint_miss(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t ivory[3] = {250, 242, 224};
    pzk_ball(o, size, size, size / 2.0f, size / 2.0f, 0.4f * size, RED, 1, 0.6f);
    float a = 0.33f * size, b = 0.67f * size, w = 0.07f * size;
    pzk_stroke(o, size, size, a, a, b, b, w, ivory, 1);
    pzk_stroke(o, size, size, a, b, b, a, w, ivory, 1);
}

/* a few arrows drawn in a colour over a clear picture, filling it, their lines bolder (the token, the tabs) */
static void paint_drawing(int size, uint8_t *rgba, const char *board, const uint8_t ink[3])
{
    arx_map m;
    if (!arx_parse(&m, board)) return;
    float s = size / (fmaxf((float)m.w, (float)m.h) + 0.4f), ox = (size - m.w * s) / 2, oy = (size - m.h * s) / 2;
    for (int i = 0; i < m.n; i++) paint_arrow(rgba, size, size, &m, i, ox, oy, s, ink, 1.6f);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_drawing(size, rgba, "..|rR|u.", GOLD);
}

/* ---- the view ---------------------------------------------------------------------------------------------------- */
static float zoom_most(void)                    /* the most zoom: cells a tenth of the screen's short side */
{
    float want = 0.1f * fminf((float)ui.w, (float)ui.h);
    return q0 > 0 && want > q0 ? want / q0 : 1;
}

/* the board placed: zoomed, slid, kept covering the room when it is bigger than it (else centred) */
static void place(void)
{
    box b = last_box;
    if (z < 1.02f) { z = 1; pan_x = pan_y = 0; }
    q = q0 * z;
    float W = (G.m.w + 2 * MARGIN) * q, H = (G.m.h + 2 * MARGIN) * q;
    float lx = W > b.w ? (W - b.w) / 2 : 0, ly = H > b.h ? (H - b.h) / 2 : 0;
    pan_x = clampf(pan_x, -lx, lx);
    pan_y = clampf(pan_y, -ly, ly);
    bx0 = b.x + b.w / 2 - G.m.w * q / 2 + pan_x;
    by0 = b.y + b.h / 2 - G.m.h * q / 2 + pan_y;
    hs = fminf(q, 0.6f * ui.unit);              /* (the lines as thin on a small board as on a big one) */
    int hp = (int)ceilf(hs);
    if (hp != head_px && hp > 1) {              /* the head, white (tinted when drawn), a square of its own size */
        head_px = hp;
        uint8_t *p = ui_buf(hp, hp);
        memset(p, 0, (size_t)hp * hp * 4);
        static const uint8_t white[3] = {255, 255, 255};
        float xy[6];
        head_xy(hp / 2.0f, hp / 2.0f, (float)hp, ARX_RIGHT, xy);
        for (int y = 0; y < hp; y++)
            for (int x = 0; x < hp; x++) pzk_over(p + 4 * (y * hp + x), white, pzt_cover(xy, 3, (float)x, (float)y));
        ui_free(&t_head);
        t_head = ui_tex(hp, hp, p);
    }
    ui_tools_shown(z > 1 ? 2 : 1);
}

/* zoomed to z1 keeping the board's point under (x, y) where it is */
static void zoom_at(float z1, float x, float y)
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    z = clampf(z1, 1, zoom_most());
    place();
    pan_x += x - (bx0 + fx * q);
    pan_y += y - (by0 + fy * q);
    place();
    ui_animate();
}

static void layout(box b)
{
    last_box = b;
    q0 = fminf(b.w / (G.m.w + 2 * MARGIN), b.h / (G.m.h + 2 * MARGIN));
    if (q0 > 1.5f * ui.unit) q0 = 1.5f * ui.unit;
    if (q0 < 2) q0 = 2;
    if (z > zoom_most()) z = zoom_most();
    place();
    ui_board_used(b.x + b.w / 2 - (G.m.w / 2.0f + MARGIN) * q0, b.y + b.h / 2 - (G.m.h / 2.0f + MARGIN) * q0,
                  (G.m.w + 2 * MARGIN) * q0, (G.m.h + 2 * MARGIN) * q0);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_miss);
        paint_miss(small, ui_buf(small, small));
        t_miss = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(mo, 0, sizeof mo);
    win.on = 0;
    hint_piece = tool_down = -1;
}

static void begin(void)
{
    game_serial++;
    calm();
    z = 1;
    pan_x = pan_y = 0;
    memset(pt, 0, sizeof pt);
    pinch.on = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!arx_level(&G, level)) arx_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    arx_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return arx_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int level_stars(void) { return ui_logic_stars(G.hints, G.mistakes); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < G.m.n; i++) {
        if (mo[i].fly && now - mo[i].fly_t0 < mo[i].fly_ms) return 1;
        if (mo[i].bump && now - mo[i].bump_t0 < mo[i].bump_hit + BUMP_BACK_MS) return 1;
    }
    return win.on && (Sint32)(now - win.t0) < WIN_MS;
}

static int ahead(int i)                         /* the squares ahead of arrow i's head to the edge */
{
    const arx_piece *p = &G.m.p[i];
    switch (p->dir) {
    case ARX_UP: return p->y;
    case ARX_RIGHT: return G.m.w - 1 - p->x;
    case ARX_DOWN: return G.m.h - 1 - p->y;
    default: return p->x;
    }
}

static void tap_piece(int i)
{
    Uint32 now = ui_ms();
    int e = ahead(i), b = -1, gap = 0;
    arx_blocker(&G, i, &gap);
    int res = arx_tap(&G, i, &b);
    if (res == ARX_REFUSED) return;
    if (hint_piece >= 0 && G.gone[hint_piece]) hint_piece = -1;
    if (res == ARX_GONE) {
        mo[i].fly = 1;
        mo[i].fly_t0 = now;
        mo[i].fly_d = e + G.m.p[i].len + TAIL + 1.6f;
        mo[i].fly_edge = e + 0.5f;
        mo[i].fly_ms = (Uint32)(LIFT_MS + 150 + 22 * mo[i].fly_d);
        ui_sound(SND_MOVE);
        if (arx_solved(&G)) {
            win.on = 1;
            win.t0 = now + mo[i].fly_ms - 120;
            ui_sound_later(SND_END, (int)mo[i].fly_ms - 120);
        }
    } else {                                    /* a bump: its head runs into b, both flash red */
        mo[i].bump = 1;
        mo[i].bump_t0 = now;
        mo[i].bump_gap = (float)gap;
        mo[i].bump_hit = (Uint32)(90 + 40 * (gap < 8 ? gap : 8));
        if (b >= 0) {
            mo[b].wob = 1;
            mo[b].wob_t0 = now + mo[i].bump_hit;
            mo[b].wob_dir = G.m.p[i].dir;
        }
        ui_sound(SND_TAP);
        ui_sound_later(SND_DENY, (int)mo[i].bump_hit);
    }
    ui_animate();
}

static void hint(void)
{
    if (arx_solved(&G)) { ui_sound(SND_DENY); return; }
    int i = arx_hint(&G);
    if (i < 0) { ui_sound(SND_DENY); return; }
    if (hint_piece != i) G.hints++;
    hint_piece = i;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return arx_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = arx_script(&G, moves);
    if (arx_solved(&G)) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* where arrow i is in its motion now: slid along its path by s squares, knocked by ks the way kd, flushed red, faded
 * (alpha); moving: 1 knocked or bumping, 2 slithering off, 3 gone */
typedef struct { float s, ks, red, alpha; int kd, moving; } pose;

static pose pose_of(int i, Uint32 now)
{
    pose p = {0, 0, 0, 1, 0, 0};
    if (mo[i].fly) {
        float t = (float)(now - mo[i].fly_t0);
        if (t >= mo[i].fly_ms) { p.moving = 3; p.alpha = 0; return p; }
        p.moving = 2;
        if (t < LIFT_MS) p.s = -0.08f * puz_ease(t / LIFT_MS);
        else {
            float u = (t - LIFT_MS) / (mo[i].fly_ms - LIFT_MS), D = mo[i].fly_d;
            p.s = -0.08f + (D + 0.08f) * (0.3f * u + 0.7f * u * u);
        }
        return p;
    }
    if (mo[i].bump) {
        float t = (float)(now - mo[i].bump_t0), hit = (float)mo[i].bump_hit, A = mo[i].bump_gap + 0.14f;
        if (t < hit) {
            float u = t / hit;
            p.s = A * u * u;
            p.moving = 1;
        } else if (t < hit + BUMP_BACK_MS) {
            float tau = t - hit;
            p.s = A * expf(-tau / 85) * cosf(tau * 2 * PUZ_PI / 260);
            p.red = expf(-tau / 220);
            p.moving = 1;
        }
    }
    if (mo[i].wob) {
        float tau = (float)(Sint32)(now - mo[i].wob_t0);
        if (tau >= 0 && tau < BUMP_BACK_MS) {
            p.ks = 0.1f * expf(-tau / 90) * sinf(tau * 2 * PUZ_PI / 150);
            p.kd = mo[i].wob_dir;
            p.red = fmaxf(p.red, expf(-tau / 220));
            p.moving = 1;
        }
    }
    return p;
}

/* how much a point (squares from the board's top left) still shows: fading past the board's edge */
static float shows(float x, float y)
{
    float out = fmaxf(fmaxf(-x, x - G.m.w), fmaxf(-y, y - G.m.h));
    return clampf(1 - (out - 0.1f) / 1.3f, 0, 1);
}

/* arrow i drawn as it is posed, in rgb: its line (round joins, a round tail), its head */
static void draw_arrow(int i, const pose *p, const uint8_t rgb[3])
{
    track t;
    track_of(&G.m, i, &t);
    int len = G.m.p[i].len, dir = G.m.p[i].dir;
    float xs[TRACK * 8], ys[TRACK * 8], kx = p->ks * ARX_DX[p->kd], ky = p->ks * ARX_DY[p->kd], W = LINE * hs;
    int n = track_points(&t, p->s - TAIL * hs / q, p->s + len - 1 - BASE * hs / q, xs, ys, TRACK * 8);
    float pa = 0, px = 0, py = 0;
    for (int k = 0; k < n; k++) {
        float x = xs[k] + kx, y = ys[k] + ky, a = shows(x, y) * p->alpha, sx = bx0 + x * q, sy = by0 + y * q;
        /* a point on a straight run, all of it shown: no joint there (one line for the run, no beads) */
        int straight = k > 0 && k < n - 1 && a >= 1 && pa >= 1 &&
                       fabsf((xs[k] - xs[k - 1]) * (ys[k + 1] - ys[k]) - (ys[k] - ys[k - 1]) * (xs[k + 1] - xs[k])) < 1e-4f;
        if (straight && shows(xs[k + 1] + kx, ys[k + 1] + ky) * p->alpha >= 1) continue;
        if (k > 0 && (a > 0 || pa > 0)) {
            pzk_rgba c = {rgb[0], rgb[1], rgb[2], (Uint8)(255 * (a + pa) / 2)};
            pzk_line(bx0 + px * q, by0 + py * q, sx, sy, W, 0, c, 0);
        }
        if (k < n - 1 && a > 0) pzk_dot(sx, sy, W, 0, (pzk_rgba){rgb[0], rgb[1], rgb[2], (Uint8)(255 * a)}, 0);
        px = x;
        py = y;
        pa = a;
    }
    float hx, hy;
    track_at(&t, p->s + len - 1, &hx, &hy);
    hx += kx;
    hy += ky;
    float a = shows(hx, hy) * p->alpha;
    if (a <= 0 || !t_head) return;
    SDL_SetTextureColorMod(t_head, rgb[0], rgb[1], rgb[2]);
    SDL_SetTextureAlphaMod(t_head, (Uint8)(255 * a));
    ui_put_rot(t_head, bx0 + hx * q, by0 + hy * q, (float)head_px, (float)head_px, 90.0 * (dir - 1));
    SDL_SetTextureColorMod(t_head, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_head, 255);
}

/* ---- the arrows at rest: each one's picture, white (tinted when drawn), made at the board's scale; the line's pieces
 * leave the most of their coverage at each pixel (no beads where they join), the head over them ---------------- */
static SDL_Texture *t_arr[ARX_MAXP];
static float arr_x[ARX_MAXP], arr_y[ARX_MAXP], arr_q;   /* a picture's top left (squares); the scale it was made at */

static void make_arrows(void)
{
    static float mask[1024 * 1024];
    for (int i = 0; i < ARX_MAXP; i++) ui_free(&t_arr[i]);
    arr_q = q;
    arr_game = game_serial;
    for (int i = 0; i < G.m.n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(&G.m, i, cells), x0 = 99, y0 = 99, x1 = -1, y1 = -1;
        for (int k = 0; k < n; k++) {
            int x = cells[k] % G.m.w, y = cells[k] / G.m.w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        arr_x[i] = x0 - 0.5f;
        arr_y[i] = y0 - 0.5f;
        int W = (int)ceilf((x1 - x0 + 2) * q), H = (int)ceilf((y1 - y0 + 2) * q);
        if (W > 1024 || H > 1024) continue;     /* (far too big: drawn as it moves instead) */
        memset(mask, 0, sizeof(float) * (size_t)W * H);
        track t;
        track_of(&G.m, i, &t);
        float xs[TRACK * 8], ys[TRACK * 8], r = LINE * hs / 2, xy[6], hx, hy;
        int np = track_points(&t, -TAIL * hs / q, G.m.p[i].len - 1 - BASE * hs / q, xs, ys, TRACK * 8);
        for (int k = 0; k < np; k++) {
            xs[k] = (xs[k] - arr_x[i]) * q;
            ys[k] = (ys[k] - arr_y[i]) * q;
        }
        for (int k = 1; k < np; k++) pzk_stroke_mask(mask, W, H, xs[k - 1], ys[k - 1], xs[k], ys[k], r);
        track_at(&t, G.m.p[i].len - 1, &hx, &hy);
        head_xy((hx - arr_x[i]) * q, (hy - arr_y[i]) * q, hs, G.m.p[i].dir, xy);
        for (int y = (int)fmaxf(0, (hy - arr_y[i] - 0.6f) * q); y < H && y < (hy - arr_y[i] + 0.6f) * q; y++)
            for (int x = (int)fmaxf(0, (hx - arr_x[i] - 0.6f) * q); x < W && x < (hx - arr_x[i] + 0.6f) * q; x++) {
                float c = pzt_cover(xy, 3, (float)x, (float)y);
                if (c > mask[x + y * W]) mask[x + y * W] = c;
            }
        uint8_t *o = ui_buf(W, H);
        for (int k = 0; k < W * H; k++) {
            o[4 * k] = o[4 * k + 1] = o[4 * k + 2] = 255;
            o[4 * k + 3] = (uint8_t)(255 * clampf(mask[k], 0, 1) + 0.5f);
        }
        t_arr[i] = ui_tex(W, H, o);
    }
}

/* arrow i at rest from its picture (moved by its knock or the hint's nudge), in rgb; 0: it has none */
static int put_arrow(int i, const pose *p, const uint8_t rgb[3])
{
    SDL_Texture *t = t_arr[i];
    if (!t || p->s != 0) return 0;
    int tw, th;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    float k = q / arr_q;
    SDL_SetTextureColorMod(t, rgb[0], rgb[1], rgb[2]);
    ui_put(t, bx0 + (arr_x[i] + p->ks * ARX_DX[p->kd]) * q, by0 + (arr_y[i] + p->ks * ARX_DY[p->kd]) * q, tw * k, th * k);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    return 1;
}

static void mix(const uint8_t a[3], const uint8_t b[3], float f, uint8_t out[3])
{
    f = clampf(f, 0, 1);
    for (int i = 0; i < 3; i++) out[i] = (uint8_t)(a[i] + (b[i] - a[i]) * f);
}

static void draw(void)
{
    if (q0 <= 0) layout(last_box);
    place();
    Uint32 now = ui_ms();
    int any = 0;
    SDL_Rect clip = {(int)last_box.x, (int)last_box.y, (int)ceilf(last_box.w), (int)ceilf(last_box.h)};
    if (z > 1) SDL_RenderSetClipRect(ui.r, &clip);   /* (zoomed: the board kept to its room) */
    const uint8_t *ink = ui_table_ink();
    if (arr_game != game_serial || (pinch.on ? fabsf(q - arr_q) > 0.06f * arr_q : q != arr_q)) make_arrows();
    static pose P[ARX_MAXP];
    for (int i = 0; i < G.m.n; i++) {
        P[i] = pose_of(i, now);
        any |= P[i].moving == 1 || P[i].moving == 2;
    }
    int hp = hint_piece >= 0 && !G.gone[hint_piece] && !P[hint_piece].moving ? hint_piece : -1;
    if (hp >= 0) {                              /* the hint: gold, nudging its way */
        float pulse = 0.5f + 0.5f * sinf(now / 190.0f);
        P[hp].ks = 0.08f * pulse * pulse;
        P[hp].kd = G.m.p[hp].dir;
        any = 1;
    }
    for (int i = 0; i < G.m.n; i++) {
        if ((G.gone[i] && P[i].moving != 2) || P[i].moving == 3) continue;
        uint8_t c[3];
        if (i == hp) mix(ink, GOLD, 1, c);
        else mix(ink, RED, P[i].red, c);
        if (!put_arrow(i, &P[i], c)) draw_arrow(i, &P[i], c);
        if (P[i].moving == 2) {                 /* the spark where its head leaves the board */
            float since = P[i].s - mo[i].fly_edge;
            if (since > -0.2f) {
                const arx_piece *p = &G.m.p[i];
                float hx = p->x + 0.5f + mo[i].fly_edge * ARX_DX[(int)p->dir];
                float hy = p->y + 0.5f + mo[i].fly_edge * ARX_DY[(int)p->dir];
                float a = clampf(1 - since / 3.0f, 0, 1) * clampf((since + 0.2f) / 0.4f, 0, 1);
                ui_mark_gold_a(bx0 + hx * q, by0 + hy * q, (0.7f + 0.2f * since) * q, 0.6f * a);
            }
        }
    }
    if (win.on) {                               /* the gold light running out from the middle */
        float wt = (float)(Sint32)(now - win.t0);
        if (wt < WIN_MS) {
            float mx = G.m.w / 2.0f, my = G.m.h / 2.0f, far = hypotf(mx, my), glow = puz_pulse(wt, 0, WIN_MS);
            if (wt > 0) {
                ui_mark_gold_a(bx0 + mx * q, by0 + my * q, 1.1f * fmaxf((float)G.m.w, (float)G.m.h) * q, 0.5f * glow);
                int step = G.m.w * G.m.h > 200 ? 2 : 1;   /* (the big boards: every other square, the same light) */
                for (int y = 0; y < G.m.h; y += step)
                    for (int x = 0; x < G.m.w; x += step) {
                        if (!G.m.shape[x + y * G.m.w]) continue;
                        float d = hypotf(x + 0.5f - mx, y + 0.5f - my), a = puz_pulse(wt, 520 * d / (far + 0.5f), 620);
                        if (a > 0) ui_mark_gold_a(bx0 + (x + 0.5f) * q, by0 + (y + 0.5f) * q, 2.0f * step * q, 0.7f * a);
                    }
            }
            any = 1;
        }
    }
    if (z > 1) SDL_RenderSetClipRect(ui.r, NULL);
    if (any) ui_animate();
    ui_tool(HINT_TOOL, ICON_HINT, (arx_solved(&G) ? UI_DIM : 0) | (tool_down == HINT_TOOL ? UI_ON : 0));
    if (z > 1) ui_tool(FIT_TOOL, ICON_TILES, tool_down == FIT_TOOL ? UI_ON : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the arrow under (x, y), a little slack round the squares: -1 none */
static int piece_at(float x, float y)
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    int cx = (int)floorf(fx), cy = (int)floorf(fy);
    if (cx >= 0 && cy >= 0 && cx < G.m.w && cy < G.m.h && G.at[cx + cy * G.m.w] != ARX_NONE)
        return G.at[cx + cy * G.m.w];
    int best = -1;
    float bd = 0.62f;                           /* a near miss: the nearest arrow's square */
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int sx = cx + dx, sy = cy + dy;
            if (sx < 0 || sy < 0 || sx >= G.m.w || sy >= G.m.h || G.at[sx + sy * G.m.w] == ARX_NONE) continue;
            float d = fmaxf(fabsf(fx - (sx + 0.5f)), fabsf(fy - (sy + 0.5f)));
            if (d < bd) { bd = d; best = G.at[sx + sy * G.m.w]; }
        }
    return best;
}

static void pointer_down(int id, float x, float y)
{
    int k = !pt[0].on ? 0 : !pt[1].on ? 1 : -1;
    if (k < 0) return;
    if (!pt[0].on && !pt[1].on) {               /* the gesture's first pointer: a tool, or the board */
        moved = pinched = 0;
        tool_down = ui_tool_hit(x, y);
        if (tool_down >= 0) ui_animate();
    }
    pt[k] = (pointer){1, id, x, y, x, y};
    pan_px0 = pan_x;
    pan_py0 = pan_y;
    if (pt[0].on && pt[1].on && tool_down < 0) {   /* two: a pinch */
        pinch.on = pinched = 1;
        pinch.d0 = fmaxf(10, hypotf(pt[0].x - pt[1].x, pt[0].y - pt[1].y));
        pinch.z0 = z;
        pinch.px0 = ((pt[0].x + pt[1].x) / 2 - bx0) / q;   /* the board's point between the fingers */
        pinch.py0 = ((pt[0].y + pt[1].y) / 2 - by0) / q;
    }
}

static void pointer_move(int id, float x, float y)
{
    int k = pt[0].on && pt[0].id == id ? 0 : pt[1].on && pt[1].id == id ? 1 : -1;
    if (k < 0) return;
    pt[k].x = x;
    pt[k].y = y;
    if (hypotf(x - pt[k].x0, y - pt[k].y0) > 0.25f * ui.unit) moved = 1;
    if (tool_down >= 0) return;
    if (pinch.on && pt[0].on && pt[1].on) {
        float d = hypotf(pt[0].x - pt[1].x, pt[0].y - pt[1].y), mx = (pt[0].x + pt[1].x) / 2,
              my = (pt[0].y + pt[1].y) / 2;
        z = clampf(pinch.z0 * d / pinch.d0, 1, zoom_most());
        place();
        pan_x += mx - (bx0 + pinch.px0 * q);    /* the same point of the board between the fingers */
        pan_y += my - (by0 + pinch.py0 * q);
        place();
        ui_animate();
    } else if (!pinched && moved && z > 1) {    /* one finger slides a zoomed board */
        pan_x = pan_px0 + (x - pt[k].x0);
        pan_y = pan_py0 + (y - pt[k].y0);
        place();
        ui_animate();
    }
}

static void pointer_up(int id, float x, float y)
{
    int k = pt[0].on && pt[0].id == id ? 0 : pt[1].on && pt[1].id == id ? 1 : -1;
    if (k < 0) return;
    pt[k].on = 0;
    if (pinch.on) {                             /* one finger left of a pinch: it slides on from there */
        pinch.on = 0;
        int o = !k;
        if (pt[o].on) {
            pt[o].x0 = pt[o].x;
            pt[o].y0 = pt[o].y;
            pan_px0 = pan_x;
            pan_py0 = pan_y;
        }
        return;
    }
    if (pt[0].on || pt[1].on) return;
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t) return;
        if (t == HINT_TOOL) hint();
        else if (t == FIT_TOOL) { z = 1; place(); ui_sound(SND_TAP); }
        return;
    }
    if (moved || pinched || arx_solved(&G)) return;
    int i = piece_at(pt[k].x0, pt[k].y0);
    if (i < 0) i = piece_at(x, y);
    if (i >= 0) tap_piece(i);
}

static void press(float x, float y) { pointer_down(-1, x, y); }
static void motion(float x, float y) { pointer_move(-1, x, y); }
static void release(float x, float y) { pointer_up(-1, x, y); }

static void finger(int id, int phase, float x, float y)
{
    if (phase == FINGER_DOWN) pointer_down(id, x, y);
    else if (phase == FINGER_MOVE) pointer_move(id, x, y);
    else pointer_up(id, x, y);
}

static void key(SDL_Keycode k)                  /* + and - zoom round the room's middle, 0 the whole board */
{
    float cx = last_box.x + last_box.w / 2, cy = last_box.y + last_box.h / 2;
    if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS) zoom_at(z * 1.5f, cx, cy);
    else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) zoom_at(z / 1.5f, cx, cy);
    else if (k == SDLK_0 || k == SDLK_KP_0) zoom_at(1, cx, cy);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_ex(b, NULL, G.left, t_miss, G.mistakes, 0, 0);   /* (the token already shows an arrow) */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the menu's card: a board of winding arrows on its paper; a picker's: a board of that size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static arx_game t;
    if (variant >= 0) {
        arx_random(&t, variant, 11u + (uint32_t)variant);
        paint_mini(&t.m, size, rgba);
        return;
    }
    if (arx_level(&t, 2 * (ARX_LEVELS / ARX_CHAPTERS) - 1)) paint_mini(&t.m, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    static arx_game t;
    if (arx_level(&t, level)) paint_mini(&t.m, size, rgba);
}

/* a chapter's tab, on its paper: a short bent arrow; a long one; a spiral; the bigger boards' showpieces */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const SMALL[3] = {"..|rR|u.", "rrd.|u.d.|u.rR", "U...|ulll|.dlu|.rru"};
    static arx_game t;
    if (chapter >= 0 && chapter < 3) {
        if (arx_parse(&t.m, SMALL[chapter])) paint_mini(&t.m, size, rgba);
    } else if (arx_level(&t, (chapter + 1) * (ARX_LEVELS / ARX_CHAPTERS) - 1)) paint_mini(&t.m, size, rgba);
}

static const game arrowescape_game = {
    .id = "arrowescape", .order = 446, .variants = ARX_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .finger = finger, .key = key,
    .levels = ARX_LEVELS, .chapters = ARX_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .fill = 1, .tools = 2,
};
REGISTER_GAME(arrowescape_game)
