/*
 * Clear the Arrows in RetroStone Board, for one player: lacquered tiles packed on a plank of the house wood, each with
 * an ivory arrow inlaid (the rules: arrowescape_rules.c). The shape of the board (a heart, a diamond...) is inlaid in
 * the plank, so it shows more and more as the tiles leave.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile. Its way clear: it lifts, draws back a hair and shoots off the board the way it points, faster and
 * faster, a ghost or two behind it, a spark where it leaves the board, fading over the table. Its way blocked: it
 * slides up to the first tile in its way, knocks it and springs back; both wobble, flushed red (a mistake). A
 * turnstile (a brass disc) that cannot go swings a quarter clockwise, overshooting a little. The colours tell the
 * ways at a glance: up lapis, right red, down jade, left amber. The shell's tool: the hint (a tile that can go now,
 * the one freeing the most, glowing gold). Undo brings back the last tile, all the way to the start (mistakes stay).
 * The bar: the tiles left, the mistakes. Cleared: a gold light runs out from the middle over the board's shape.
 * Everything moves on the shell's clock (ui_ms): the state changes at the tap, the pictures follow.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/arrowescape.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.34f                             /* the plank's walnut frame, of a square */
#define LIFT_MS 70                              /* a tile drawing back before it shoots */
#define BUMP_BACK_MS 440                        /* a bumped tile springing back */
#define TURN_MS 260
#define WIN_MS 1500
#define HINT_TOOL 0

static const uint8_t COL[4][3] = {{58, 98, 176}, {194, 66, 50}, {44, 138, 100}, {216, 150, 46}};
static const uint8_t IVORY[3] = {250, 242, 224}, EBONY[3] = {38, 28, 24}, BRASS[3] = {222, 178, 92};

static arx_game G;
static int serial, hint_piece = -1, tool_down = -1;
static float press_x, press_y;
static struct {                                 /* each tile's motion (ms on the shell's clock; on: running) */
    uint8_t fly, bump, wob, turn;
    Uint32 fly_t0, bump_t0, wob_t0, turn_t0;
    uint8_t fly_dir;
    float fly_d, fly_edge;                      /* how far it goes (squares), where its head leaves the board */
    Uint32 fly_ms, bump_hit;                    /* its flight; when the bump lands */
    float bump_gap;
    float wob_dir;                              /* the way it was knocked */
} mo[ARX_MAXP];
static struct { int on; Uint32 t0; } win;

static box last_box;
static float bx0, by0, q;                       /* the board's first square's top left, a square */
static int made_q = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_tile[ARX_MAXLEN][4], *t_shadow[ARX_MAXLEN][2], *t_pivot, *t_tarrow, *t_miss;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the signed distance to a rounded rectangle's edge (inside > 0), the box x0, y0, w x h, corners r */
static float rr_dist(float px, float py, float x0, float y0, float w, float h, float r)
{
    float cx = x0 + w / 2, cy = y0 + h / 2, dx = fabsf(px - cx) - (w / 2 - r), dy = fabsf(py - cy) - (h / 2 - r);
    float ox = fmaxf(dx, 0), oy = fmaxf(dy, 0);
    return r - (hypotf(ox, oy) + fminf(fmaxf(dx, dy), 0));
}

/* a point along a tile (a: squares from its tail end to its head end, c: 0..1 across, from its left as it points)
 * on the picture */
static void along(int dir, int len, float x0, float y0, float s, float a, float c, float *x, float *y)
{
    switch (dir) {
    case ARX_RIGHT: *x = x0 + a * s; *y = y0 + c * s; break;
    case ARX_LEFT: *x = x0 + (len - a) * s; *y = y0 + (1 - c) * s; break;
    case ARX_DOWN: *x = x0 + (1 - c) * s; *y = y0 + a * s; break;
    default: *x = x0 + c * s; *y = y0 + (len - a) * s; break;
    }
}

/* a polygon (n points) painted in rgb at alpha a over the picture, its edges smoothed */
static void poly(uint8_t *o, int W, int H, const float *xy, int n, const uint8_t rgb[3], float a)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        x0 = fminf(x0, xy[2 * i]);
        x1 = fmaxf(x1, xy[2 * i]);
        y0 = fminf(y0, xy[2 * i + 1]);
        y1 = fmaxf(y1, xy[2 * i + 1]);
    }
    for (int y = (int)fmaxf(0, floorf(y0)); y < H && y <= (int)y1; y++)
        for (int x = (int)fmaxf(0, floorf(x0)); x < W && x <= (int)x1; x++) {
            float c = pzt_cover(xy, n, (float)x, (float)y);
            if (c > 0) pzk_over(o + 4 * ((size_t)y * W + x), rgb, a * c);
        }
}

/* an arrow along a tile of len squares (s pixels a square), its shaft from a0, its head's tip at tip, as wide as hw */
static void arrow_poly(int dir, int len, float x0, float y0, float s, float a0, float tip, float hw, float sw,
                       float dx, float dy, float *xy)
{
    float base = tip - 1.25f * hw;
    const float A[7] = {a0, base, base, tip, base, base, a0},
                C[7] = {0.5f - sw, 0.5f - sw, 0.5f - hw, 0.5f, 0.5f + hw, 0.5f + sw, 0.5f + sw};
    for (int i = 0; i < 7; i++) {
        along(dir, len, x0, y0, s, A[i], C[i], &xy[2 * i], &xy[2 * i + 1]);
        xy[2 * i] += dx;
        xy[2 * i + 1] += dy;
    }
}

/* a turnstile's arrow (ebony, pointing dir) in the square at x0, y0 */
static void paint_tarrow(uint8_t *o, int W, int H, float x0, float y0, float s, int dir)
{
    float xy[14];
    static const uint8_t lit[3] = {255, 236, 180};
    arrow_poly(dir, 1, x0, y0, s, 0.27f, 0.8f, 0.2f, 0.07f, 0.02f * s, 0.03f * s, xy);
    poly(o, W, H, xy, 7, lit, 0.45f);
    arrow_poly(dir, 1, x0, y0, s, 0.27f, 0.8f, 0.2f, 0.07f, 0, 0, xy);
    poly(o, W, H, xy, 7, EBONY, 1);
}

/* a turnstile's brass disc on its pivot in the square at x0, y0 */
static void paint_pivot(uint8_t *o, int W, int H, float x0, float y0, float s)
{
    static const uint8_t dark[3] = {96, 64, 26};
    float cx = x0 + s / 2, cy = y0 + s / 2;
    pzk_ball(o, W, H, cx, cy, 0.43f * s, dark, 1, 0.3f);
    pzk_ball(o, W, H, cx - 0.01f * s, cy - 0.015f * s, 0.39f * s, BRASS, 1, 0.75f);
    pzk_arc(o, W, H, cx, cy, 0.30f * s, 0.315f * s, 0, 360, dark, 0.35f);
}

/* a tile of len squares lying its way dir (a turnstile: its disc and arrow), its squares s pixels, painted over the
 * picture o (W x H), the box of its squares from x0, y0 */
static void paint_piece(uint8_t *o, int W, int H, float x0, float y0, float s, int len, int dir, int turn, int arrow)
{
    if (turn) {
        paint_pivot(o, W, H, x0, y0, s);
        if (arrow) paint_tarrow(o, W, H, x0, y0, s, dir);
        return;
    }
    int hor = dir == ARX_LEFT || dir == ARX_RIGHT;
    float bw = hor ? len * s : s, bh = hor ? s : len * s, m = 0.055f * s, r = 0.2f * s, bev = fmaxf(1.2f, 0.09f * s);
    for (int y = (int)fmaxf(0, floorf(y0)); y < H && y < y0 + bh + 1; y++)
        for (int x = (int)fmaxf(0, floorf(x0)); x < W && x < x0 + bw + 1; x++) {
            float px = x + 0.5f, py = y + 0.5f, d = rr_dist(px, py, x0 + m, y0 + m, bw - 2 * m, bh - 2 * m, r);
            float cov = clampf(d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 1.07f - 0.14f * (py - y0) / bh + 0.035f * art_noise(x * 2, y * 7);
            if (d < bev) {                      /* the bevel: lit at the top and the left */
                float e = 1 - d / bev, gx = rr_dist(px + 1, py, x0 + m, y0 + m, bw - 2 * m, bh - 2 * m, r) - d,
                      gy = rr_dist(px, py + 1, x0 + m, y0 + m, bw - 2 * m, bh - 2 * m, r) - d;
                k += 0.3f * e * (gx + gy) * 0.7071f;   /* the inward normal facing the bottom right: lit */
            }
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(COL[dir][i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), c, cov);
        }
    if (!arrow) return;
    float xy[14], a0 = len == 1 ? 0.22f : 0.34f, tip = len - 0.2f, hw = 0.25f, sw = 0.08f;
    uint8_t deep[3];
    for (int i = 0; i < 3; i++) deep[i] = (uint8_t)(COL[dir][i] * 0.5f);
    arrow_poly(dir, len, x0, y0, s, a0, tip, hw, sw, -0.02f * s, -0.025f * s, xy);   /* the groove's lit lip */
    poly(o, W, H, xy, 7, deep, 0.55f);
    arrow_poly(dir, len, x0, y0, s, a0, tip, hw, sw, 0.012f * s, 0.018f * s, xy);
    poly(o, W, H, xy, 7, IVORY, 0.35f);
    arrow_poly(dir, len, x0, y0, s, a0, tip, hw, sw, 0, 0, xy);
    poly(o, W, H, xy, 7, IVORY, 1);
}

/* a soft shadow under a tile (w x h pixels of squares; round: a turnstile's), its picture (w + 2 blur) x ... */
static SDL_Texture *shadow_tex(int w, int h, int round, float s)
{
    int blur = (int)fmaxf(2, 0.12f * s), W = w + 2 * blur, H = h + 2 * blur;
    uint8_t *o = ui_buf(W, H);
    float m = 0.06f * s, r = round ? 0.42f * s : 0.2f * s;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float d = round ? 0.42f * s - hypotf(x + 0.5f - W / 2.0f, y + 0.5f - H / 2.0f)
                            : rr_dist(x + 0.5f, y + 0.5f, blur + m, blur + m, w - 2 * m, h - 2 * m, r);
            float a = clampf((d + blur) / (2.0f * blur), 0, 1);
            uint8_t *p = o + 4 * (y * W + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(150 * a * a);
        }
    return ui_tex(W, H, o);
}

/* the plank w x h squares of s pixels in its frame f, the board's shape inlaid in it (a shallow, darker bed) */
static void paint_board(const arx_map *m, int s, int f, uint8_t *o)
{
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    static const float tone[3] = {0.80f, 0.76f, 0.72f};
    art_plank(m->w * s, m->h * s, f, o);
    for (int c = 0; c < m->w * m->h; c++)
        if (m->shape[c])
            pzk_inlay(o, W, H, f + c % m->w * s + 0.035f * s, f + c / m->w * s + 0.035f * s, 0.93f * s, 0.93f * s,
                      0.16f * s, tone, fmaxf(1, 0.05f * s));
}

/* a board in a picture size x size: its plank, its tiles (fly: one tile drawn shooting off, -1 none) */
static void paint_mini(const arx_map *m, int size, uint8_t *rgba, int fly)
{
    float span = fmaxf((float)m->w, (float)m->h) + 2 * FRAME + (fly >= 0 ? 0.6f : 0);
    int s = (int)(size / span), f = (int)(FRAME * s);
    if (s < 2) return;
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, s, f, o);
    for (int i = 0; i < m->n; i++) {
        const arx_piece *p = &m->p[i];
        int tx = p->x - (p->len - 1) * ARX_DX[(int)p->dir], ty = p->y - (p->len - 1) * ARX_DY[(int)p->dir];
        float x0 = f + fminf((float)p->x, (float)tx) * s, y0 = f + fminf((float)p->y, (float)ty) * s;
        if (i == fly) continue;
        paint_piece(o, W, H, x0, y0, (float)s, p->len, p->dir, p->turn, s >= 6);
    }
    int ox = (size - W) / 2, oy = (size - H) / 2;
    art_blend(rgba, size, size, o, W, H, ox, oy);
    free(o);
    if (fly >= 0) {                             /* the tile shooting out past the frame, two ghosts behind it */
        const arx_piece *p = &m->p[fly];
        int tx = p->x - (p->len - 1) * ARX_DX[(int)p->dir], ty = p->y - (p->len - 1) * ARX_DY[(int)p->dir];
        float x0 = ox + f + fminf((float)p->x, (float)tx) * s, y0 = oy + f + fminf((float)p->y, (float)ty) * s;
        float go = (p->dir == ARX_RIGHT ? m->w - p->x : p->dir == ARX_LEFT ? p->x + 1 : p->dir == ARX_DOWN ? m->h - p->y
                                                                                                   : p->y + 1) - 0.3f;
        uint8_t *g = calloc((size_t)size * size, 4);
        if (!g) return;
        for (int k = 2; k >= 0; k--) {
            memset(g, 0, (size_t)size * size * 4);
            float d = (go - 0.45f * k) * s;
            paint_piece(g, size, size, x0 + d * ARX_DX[(int)p->dir], y0 + d * ARX_DY[(int)p->dir], (float)s, p->len,
                        p->dir, p->turn, 1);
            if (k)
                for (int j = 0; j < size * size; j++) g[4 * j + 3] = (uint8_t)(g[4 * j + 3] * (k == 1 ? 0.35f : 0.15f));
            art_blend(rgba, size, size, g, size, size, 0, 0);
        }
        free(g);
    }
}

/* the bar's pictures: a red tile pointing right; a mistake: two tiles knocking, a red flush between them */
static void paint_small_tile(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    paint_piece(o, size, size, 0.08f * size, 0.08f * size, 0.84f * size, 1, ARX_RIGHT, 0, 1);
}

static void paint_miss(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t red[3] = {200, 56, 40};
    pzk_ball(o, size, size, size / 2.0f, size / 2.0f, 0.4f * size, red, 1, 0.6f);
    float a = 0.33f * size, b = 0.67f * size, w = 0.07f * size;
    pzk_stroke(o, size, size, a, a, b, b, w, IVORY, 1);
    pzk_stroke(o, size, size, a, b, b, a, w, IVORY, 1);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_small_tile(size, rgba);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    int w = G.m.w, h = G.m.h;
    q = floorf(fminf(b.w / (w + 2 * FRAME), b.h / (h + 2 * FRAME)));
    if (q > 1.6f * ui.unit) q = floorf(1.6f * ui.unit);
    if (q < 4) q = 4;
    bx0 = floorf(b.x + (b.w - w * q) / 2);
    by0 = floorf(b.y + (b.h - h * q) / 2);
    int s = (int)q, f = (int)(FRAME * q);
    if (made_q != s || made_serial != serial) {
        made_q = s;
        int W = w * s + 2 * f, H = h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
    }
    static int tiles_q = -1;
    if (tiles_q != s) {
        tiles_q = s;
        for (int len = 1; len <= ARX_MAXLEN; len++) {
            for (int d = 0; d < 4; d++) {
                int hor = d == ARX_LEFT || d == ARX_RIGHT, W = hor ? len * s : s, H = hor ? s : len * s;
                uint8_t *o = ui_buf(W, H);
                memset(o, 0, (size_t)W * H * 4);
                paint_piece(o, W, H, 0, 0, (float)s, len, d, 0, 1);
                ui_free(&t_tile[len - 1][d]);
                t_tile[len - 1][d] = ui_tex(W, H, o);
            }
            for (int hor = 0; hor < 2; hor++) {
                ui_free(&t_shadow[len - 1][hor]);
                t_shadow[len - 1][hor] = shadow_tex(hor ? len * s : s, hor ? s : len * s, 0, (float)s);
            }
        }
        uint8_t *o = ui_buf(s, s);
        memset(o, 0, (size_t)s * s * 4);
        paint_pivot(o, s, s, 0, 0, (float)s);
        ui_free(&t_pivot);
        t_pivot = ui_tex(s, s, o);
        memset(o, 0, (size_t)s * s * 4);
        paint_tarrow(o, s, s, 0, 0, (float)s, ARX_UP);
        ui_free(&t_tarrow);
        t_tarrow = ui_tex(s, s, o);
        ui_free(&t_shadow[0][0]);                /* (a turnstile: round; a single square's is the same either way) */
        t_shadow[0][0] = shadow_tex(s, s, 1, (float)s);
    }
    made_serial = serial;
    ui_board_used(bx0 - f, by0 - f, w * q + 2 * f, h * q + 2 * f);
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
    serial++;
    calm();
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

static Uint32 bump_ms(int i) { return mo[i].bump_hit + BUMP_BACK_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < G.m.n; i++) {
        if (mo[i].fly && now - mo[i].fly_t0 < mo[i].fly_ms) return 1;
        if (mo[i].bump && now - mo[i].bump_t0 < bump_ms(i)) return 1;
        if (mo[i].turn && now - mo[i].turn_t0 < TURN_MS) return 1;
    }
    return win.on && (Sint32)(now - win.t0) < WIN_MS;
}

/* how many squares lie ahead of tile i's head (its way now) to the edge */
static int ahead(int i)
{
    const arx_piece *p = &G.m.p[i];
    switch (G.dir[i]) {
    case ARX_UP: return p->y;
    case ARX_RIGHT: return G.m.w - 1 - p->x;
    case ARX_DOWN: return G.m.h - 1 - p->y;
    default: return p->x;
    }
}

static void tap_piece(int i)
{
    Uint32 now = ui_ms();
    int dir = G.dir[i], e = ahead(i), b = -1, gap = 0;
    arx_blocker(&G, i, &gap);
    int res = arx_tap(&G, i, &b);
    if (res == ARX_REFUSED) return;
    if (hint_piece >= 0 && (res == ARX_GONE || G.gone[hint_piece])) hint_piece = -1;
    if (res == ARX_GONE) {
        mo[i].fly = 1;
        mo[i].fly_t0 = now;
        mo[i].fly_dir = (uint8_t)dir;
        mo[i].fly_d = e + G.m.p[i].len + FRAME + 0.4f;
        mo[i].fly_edge = e + 0.5f;
        mo[i].fly_ms = (Uint32)(LIFT_MS + 150 + 26 * mo[i].fly_d);
        ui_sound(SND_MOVE);
        if (arx_solved(&G)) {
            win.on = 1;
            win.t0 = now + mo[i].fly_ms - 120;
            ui_sound_later(SND_END, (int)mo[i].fly_ms - 120);
        }
    } else if (res == ARX_TURNED) {
        mo[i].turn = 1;
        mo[i].turn_t0 = now;
        ui_sound(SND_TAP);
    } else {                                    /* a bump: it runs into b, both wobble */
        mo[i].bump = 1;
        mo[i].bump_t0 = now;
        mo[i].bump_gap = (float)gap;
        mo[i].bump_hit = (Uint32)(90 + 45 * gap);
        if (b >= 0) {
            mo[b].wob = 1;
            mo[b].wob_t0 = now + mo[i].bump_hit;
            mo[b].wob_dir = (float)dir;
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
static float ease_back(float u)                 /* past the end a little, then back */
{
    u = clampf(u, 0, 1) - 1;
    return 1 + 2.4f * u * u * u + 1.4f * u * u;
}

/* tile i's middle (squares from the board's top left) lying its way dir */
static void middle(int i, int dir, float *cx, float *cy)
{
    const arx_piece *p = &G.m.p[i];
    float k = (p->len - 1) / 2.0f;
    *cx = p->x + 0.5f - k * ARX_DX[dir];
    *cy = p->y + 0.5f - k * ARX_DY[dir];
}

/* a tile drawn moved by (dx, dy) squares, grown by grow, turned by deg, faded (alpha 0..1), flushed red (0..1),
 * its shadow lifted by lift (0..1); how: 1 its shadow alone, 2 a gold light added over it */
static void draw_tile(int i, int dir, float dx, float dy, float grow, float deg, float alpha, float red, float lift,
                      int how)
{
    const arx_piece *p = &G.m.p[i];
    float cx, cy;
    middle(i, dir, &cx, &cy);
    cx = bx0 + (cx + dx) * q;
    cy = by0 + (cy + dy) * q;
    int hor = dir == ARX_LEFT || dir == ARX_RIGHT;
    float w = (p->turn || !hor ? 1 : p->len) * q, h = (p->turn || hor ? 1 : p->len) * q;
    Uint8 a = (Uint8)clampf(255 * alpha, 0, 255);
    if (how & 1) {
        SDL_Texture *t = p->turn ? t_shadow[0][0] : t_shadow[p->len - 1][hor];
        int tw, th;
        if (!t) return;
        SDL_QueryTexture(t, NULL, NULL, &tw, &th);
        float sx = tw * grow, sy = th * grow, off = (0.05f + 0.1f * lift) * q;
        SDL_SetTextureAlphaMod(t, (Uint8)(a * (1 - 0.35f * lift)));
        ui_put_rot(t, cx + off, cy + 1.3f * off, sx, sy, deg);
        SDL_SetTextureAlphaMod(t, 255);
        return;
    }
    Uint8 gb = (Uint8)(255 - 50 * clampf(red, 0, 1));
    if (how & 2) {                              /* the hint: gold light added over it */
        SDL_Texture *t = p->turn ? t_pivot : t_tile[p->len - 1][dir];
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(t, 255, 190, 80);
        SDL_SetTextureAlphaMod(t, a);
        ui_put_rot(t, cx, cy, w * grow, h * grow, deg);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, 255);
        return;
    }
    if (p->turn) {
        SDL_SetTextureAlphaMod(t_pivot, a);
        SDL_SetTextureColorMod(t_pivot, 255, gb, gb);
        ui_put_rot(t_pivot, cx, cy, q * grow, q * grow, deg);
        SDL_SetTextureColorMod(t_pivot, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_pivot, 255);
        float tdeg = 90.0f * dir;
        if (mo[i].turn) {
            float u = (ui_ms() - mo[i].turn_t0) / (float)TURN_MS;
            if (u < 1) tdeg = 90.0f * (dir - 1) + 90.0f * ease_back(u);
        }
        SDL_SetTextureAlphaMod(t_tarrow, a);
        ui_put_rot(t_tarrow, cx, cy, q * grow, q * grow, tdeg + deg);
        SDL_SetTextureAlphaMod(t_tarrow, 255);
        return;
    }
    SDL_Texture *t = t_tile[p->len - 1][dir];
    SDL_SetTextureAlphaMod(t, a);
    SDL_SetTextureColorMod(t, 255, gb, gb);
    ui_put_rot(t, cx, cy, w * grow, h * grow, deg);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* where tile i is in its motion now: moved along its way by *s squares, knocked by *ks the way *kd, turned *deg, grown,
 * flushed red; 0 when it rests */
typedef struct { float s, ks, deg, grow, red, lift, alpha; int kd, moving; } pose;

static pose pose_of(int i, Uint32 now)
{
    pose p = {0, 0, 0, 1, 0, 0, 1, 0, 0};
    if (mo[i].fly) {
        float t = (float)(now - mo[i].fly_t0);
        if (t < mo[i].fly_ms) {
            p.moving = 2;
            float D = mo[i].fly_d, l = clampf(t / LIFT_MS, 0, 1);
            p.lift = l;
            p.grow = 1 + 0.07f * l;
            if (t < LIFT_MS) p.s = -0.1f * puz_ease(l);
            else {
                float u = (t - LIFT_MS) / (mo[i].fly_ms - LIFT_MS);
                p.s = -0.1f + (D + 0.1f) * (0.3f * u + 0.7f * u * u);
            }
            float out = mo[i].fly_edge + 0.2f;   /* past the edge: fading over the table */
            p.alpha = 1 - clampf((p.s - out) / (D - out), 0, 1);
        } else {
            p.alpha = 0;
            p.moving = 3;                       /* gone */
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
            p.deg = 5 * expf(-tau / 110) * sinf(tau * 2 * PUZ_PI / 150);
            p.red = expf(-tau / 160);
            p.moving = 1;
        }
    }
    if (mo[i].wob) {
        float tau = (float)(Sint32)(now - mo[i].wob_t0);
        if (tau >= 0 && tau < BUMP_BACK_MS) {
            p.ks = 0.08f * expf(-tau / 70) * sinf(fminf(tau, 200) * PUZ_PI / 120);
            p.kd = (int)mo[i].wob_dir;
            p.deg += 8 * expf(-tau / 120) * sinf(tau * 2 * PUZ_PI / 140);
            p.red = fmaxf(p.red, expf(-tau / 170));
            p.moving = 1;
        }
    }
    if (mo[i].turn && now - mo[i].turn_t0 < TURN_MS) p.moving = 1;
    return p;
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    int f = (int)(FRAME * q), any = 0;
    ui_put(t_board, bx0 - f, by0 - f, G.m.w * q + 2 * f, G.m.h * q + 2 * f);
    static pose P[ARX_MAXP];
    for (int i = 0; i < G.m.n; i++) {
        P[i] = pose_of(i, now);
        any |= P[i].moving == 1 || P[i].moving == 2;
    }
    int hp = hint_piece >= 0 && !G.gone[hint_piece] && !P[hint_piece].moving ? hint_piece : -1;
    float pulse = 0.5f + 0.5f * sinf(now / 190.0f);
    if (hp >= 0) {                              /* the hint: a gold glow under it, a nudge its way, gold light on it */
        int cells[ARX_MAXLEN], n = arx_cells(&G.m, hp, G.dir[hp], cells);
        for (int k = 0; k < n; k++)
            ui_mark_gold_a(bx0 + (cells[k] % G.m.w + 0.5f) * q, by0 + (cells[k] / G.m.w + 0.5f) * q, 2.0f * q,
                           0.6f + 0.4f * pulse);
        P[hp].s = 0.07f * pulse * pulse;
        any = 1;
    }
    for (int pass = 0; pass < 3; pass++)        /* the shadows, red halos round a knock, the tiles at rest or knocked */
        for (int i = 0; i < G.m.n; i++) {
            if (G.gone[i] || P[i].moving >= 2) continue;
            const pose *p = &P[i];
            int d = G.dir[i];
            float dx = p->s * ARX_DX[d] + p->ks * ARX_DX[p->kd], dy = p->s * ARX_DY[d] + p->ks * ARX_DY[p->kd];
            if (pass == 1) {
                if (p->red < 0.02f) continue;
                int cells[ARX_MAXLEN], n = arx_cells(&G.m, i, d, cells);
                for (int k = 0; k < n; k++)
                    pzk_dot(bx0 + (cells[k] % G.m.w + 0.5f + dx) * q, by0 + (cells[k] / G.m.w + 0.5f + dy) * q,
                            0.95f * q, 0.32f * q, (pzk_rgba){236, 64, 40, (Uint8)(190 * p->red)}, 0);
                continue;
            }
            draw_tile(i, d, dx, dy, p->grow, p->deg, 1, p->red, 0, pass == 0);
            if (pass == 2 && i == hp) draw_tile(i, d, dx, dy, 1, 0, 0.12f + 0.2f * pulse, 0, 0, 2);
        }
    for (int i = 0; i < G.m.n; i++) {           /* the tiles shooting off: their ghosts, their shadows, themselves */
        if (P[i].moving != 2) continue;
        const pose *p = &P[i];
        int d = mo[i].fly_dir;
        float t = (float)(now - mo[i].fly_t0);
        if (t > LIFT_MS)
            for (int k = 2; k >= 1; k--) {
                float s = p->s - 0.32f * k * clampf((t - LIFT_MS) / 120, 0, 1);
                draw_tile(i, d, s * ARX_DX[d], s * ARX_DY[d], p->grow, 0, p->alpha * (k == 1 ? 0.3f : 0.14f), 0, 1, 0);
            }
        draw_tile(i, d, p->s * ARX_DX[d], p->s * ARX_DY[d], p->grow, 0, p->alpha, 0, p->lift, 1);
        draw_tile(i, d, p->s * ARX_DX[d], p->s * ARX_DY[d], p->grow, 0, p->alpha, 0, p->lift, 0);
        /* the spark where its head leaves the board */
        float hx = G.m.p[i].x + 0.5f + (mo[i].fly_edge) * ARX_DX[d], hy = G.m.p[i].y + 0.5f + mo[i].fly_edge * ARX_DY[d];
        float since = p->s - mo[i].fly_edge;
        if (since > -0.2f) {
            float a = clampf(1 - since / 3.0f, 0, 1) * clampf((since + 0.2f) / 0.4f, 0, 1);
            ui_mark_gold_a(bx0 + hx * q, by0 + hy * q, (0.8f + 0.25f * since) * q, 0.8f * a);
        }
    }
    if (win.on) {                               /* the gold light running out from the middle over the shape */
        float wt = (float)(Sint32)(now - win.t0);
        if (wt < WIN_MS) {
            float mx = G.m.w / 2.0f, my = G.m.h / 2.0f, far = hypotf(mx, my), glow = puz_pulse(wt, 0, WIN_MS);
            if (wt > 0) {
                ui_mark_gold_a(bx0 + mx * q, by0 + my * q, 1.25f * fmaxf((float)G.m.w, (float)G.m.h) * q, 0.55f * glow);
                for (int c = 0; c < G.m.w * G.m.h; c++) {
                    if (!G.m.shape[c]) continue;
                    float x = c % G.m.w + 0.5f, y = c / G.m.w + 0.5f, d = hypotf(x - mx, y - my);
                    float a = puz_pulse(wt, 520 * d / (far + 0.5f), 620);
                    if (a > 0) ui_mark_gold_a(bx0 + x * q, by0 + y * q, 2.1f * q, a);
                }
            }
            any = 1;
        }
    }
    if (any) ui_animate();
    ui_tool(HINT_TOOL, ICON_HINT, (arx_solved(&G) ? UI_DIM : 0) | (tool_down == HINT_TOOL ? UI_ON : 0));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the tile under (x, y), a little slack round the squares: -1 none */
static int piece_at(float x, float y)
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    int cx = (int)floorf(fx), cy = (int)floorf(fy);
    if (cx >= 0 && cy >= 0 && cx < G.m.w && cy < G.m.h && G.at[cx + cy * G.m.w] != ARX_NONE)
        return G.at[cx + cy * G.m.w];
    int best = -1;
    float bd = 0.62f;                           /* a near miss: the nearest tile's square */
    for (int c = 0; c < G.m.w * G.m.h; c++) {
        if (G.at[c] == ARX_NONE) continue;
        float d = fmaxf(fabsf(fx - (c % G.m.w + 0.5f)), fabsf(fy - (c / G.m.w + 0.5f)));
        if (d < bd) { bd = d; best = G.at[c]; }
    }
    return best;
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    press_x = x;
    press_y = y;
    if (tool_down >= 0) ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == k && k == HINT_TOOL) hint();
        return;
    }
    if (arx_solved(&G)) return;
    int i = piece_at(press_x, press_y);
    if (i < 0 || piece_at(x, y) != i) {
        if (i < 0 && piece_at(x, y) < 0) return;
        if (i < 0) i = piece_at(x, y);
        if (i < 0) return;
    }
    tap_piece(i);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_ex(b, NULL, G.left, t_miss, G.mistakes, 0, 0);   /* (the token already shows a tile) */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the menu's card: a heart of tiles, one shooting off to the right */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static arx_game t;
    if (variant >= 0) {
        arx_random(&t, variant, 11u + (uint32_t)variant);
        paint_mini(&t.m, size, rgba, -1);
        return;
    }
    if (!arx_parse(&t.m, ".UU.UU.|LUUUUUR|LLUDURR|.LLDRR.|..LDR..|...D...")) return;
    int fly = -1;                               /* the R at the right end of the second row, its way clear */
    for (int i = 0; i < t.m.n; i++)
        if (t.m.p[i].x == 6 && t.m.p[i].y == 1) fly = i;
    paint_mini(&t.m, size, rgba, fly);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    static arx_game t;
    if (arx_level(&t, level)) paint_mini(&t.m, size, rgba, -1);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    float s = 0.42f * size;
    switch (chapter) {
    case 0:                                     /* a single square */
        paint_piece(rgba, size, size, (size - 1.5f * s) / 2, (size - 1.5f * s) / 2, 1.5f * s, 1, ARX_RIGHT, 0, 1);
        break;
    case 1:                                     /* a bar */
        s = 0.3f * size;
        paint_piece(rgba, size, size, (size - 3 * s) / 2, (size - s) / 2, s, 3, ARX_RIGHT, 0, 1);
        break;
    case 2:                                     /* a turnstile */
        paint_piece(rgba, size, size, (size - 1.6f * s) / 2, (size - 1.6f * s) / 2, 1.6f * s, 1, ARX_RIGHT, 1, 1);
        break;
    default: {                                  /* a big board, a shape, all of it: its showpiece */
        static arx_game t;
        if (arx_level(&t, (chapter + 1) * (ARX_LEVELS / ARX_CHAPTERS) - 1)) paint_mini(&t.m, size, rgba, -1);
        break;
    }
    }
}

static const game arrowescape_game = {
    .id = "arrowescape", .order = 446, .variants = ARX_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = ARX_LEVELS, .chapters = ARX_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .fill = 1, .tools = 1,
};
REGISTER_GAME(arrowescape_game)
