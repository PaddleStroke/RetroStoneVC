/*
 * Clear the Arrows in RetroStone Board, for one player: winding arrows of lacquer packed on a plank of the house wood
 * (the rules: arrowescape_rules.c). Each arrow is one rounded ribbon running square to square through the squares'
 * middles (its bends quarter circles), a little lighter towards its head, a broad arrowhead on its head; neighbours
 * never share a colour. The shape of the board (a heart, a diamond...) is inlaid in the plank, so it shows more and
 * more as the arrows leave.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap an arrow. Its way clear: it draws back a hair and slithers off, faster and faster, its head straight out and its
 * body following the head's path square by square, bending round its corners like a snake; it fades over the table,
 * a spark where its head leaves the board. Its way blocked: its head slides up to the arrow in the way, knocks it and
 * springs back, the whole body following; both wobble, flushed red (a mistake). The shell's tool: the hint (an arrow
 * that can go now, the one freeing the most, glowing gold and nudging forward). Undo brings back the last arrow, all
 * the way to the start (mistakes stay). The bar: the arrows left, the mistakes. Cleared: a gold light runs out from
 * the middle over the board's shape. Everything moves on the shell's clock (ui_ms): the state changes at the tap, the
 * pictures follow.
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
#define WIDTH 0.42f                             /* the ribbon, of a square */
#define TAIL 0.3f                               /* the ribbon's end behind its last square's middle */
#define LIFT_MS 70                              /* an arrow drawing back before it goes */
#define BUMP_BACK_MS 440                        /* a bumped arrow springing back */
#define WIN_MS 1500
#define HINT_TOOL 0
#define PALETTE 8
#define TRACK (ARX_MAXLEN + ARX_MAXW + ARX_MAXH + 8)

static const uint8_t PAL[PALETTE][3] = {
    {198, 66, 52}, {44, 142, 104}, {224, 160, 50}, {58, 100, 182},
    {142, 72, 132}, {34, 140, 150}, {230, 114, 86}, {122, 140, 48},
};
static const uint8_t IVORY[3] = {250, 242, 224};

static arx_game G;
static uint8_t colour[ARX_MAXP];
static int serial, hint_piece = -1, tool_down = -1;
static float press_x, press_y;
static struct {                                 /* each arrow's motion (ms on the shell's clock; on: running) */
    uint8_t fly, bump, wob;
    Uint32 fly_t0, bump_t0, wob_t0;
    float fly_d, fly_edge;                      /* how far it slithers (squares), where its head leaves the board */
    Uint32 fly_ms, bump_hit;                    /* its flight; when the bump lands */
    float bump_gap;
    int wob_dir;                                /* the way it was knocked */
} mo[ARX_MAXP];
static struct { int on; Uint32 t0; } win;

static box last_box;
static float bx0, by0, q;                       /* the board's first square's top left, a square */
static int made_q = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_head, *t_miss, *t_arrow[ARX_MAXP];
static float arrow_x[ARX_MAXP], arrow_y[ARX_MAXP];   /* an arrow's picture: its top left, squares from the board's */

/* ---- an arrow's path: its squares from its tail to its head, then straight on past the board's edge -------------
 * A point at t (0: its tail square's middle, len - 1 its head's, on past it): straight through a square whose way in
 * and way out agree, a quarter circle round the corner between them where they do not. */
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

/* the point at t, squares from the board's top left (the middle of square (x, y): x + 0.5, y + 0.5) */
static void track_at(const track *t, float s, float *px, float *py)
{
    int k = (int)floorf(s + 0.5f);
    if (k < 0) k = 0;
    if (k > t->n - 1) k = t->n - 1;
    float u = s - (k - 0.5f);
    int ox = k + 1 < t->n ? t->x[k + 1] - t->x[k] : t->x[k] - t->x[k - 1];
    int oy = k + 1 < t->n ? t->y[k + 1] - t->y[k] : t->y[k] - t->y[k - 1];
    int ix = k > 0 ? t->x[k] - t->x[k - 1] : ox, iy = k > 0 ? t->y[k] - t->y[k - 1] : oy;
    float cx = t->x[k] + 0.5f, cy = t->y[k] + 0.5f;
    if ((ix == ox && iy == oy) || u < 0 || u > 1) {   /* straight (and past either end) */
        float d = s - k;
        *px = cx + d * (d < 0 ? ix : ox);
        *py = cy + d * (d < 0 ? iy : oy);
        return;
    }
    float kx = cx - 0.5f * ix + 0.5f * ox, ky = cy - 0.5f * iy + 0.5f * oy;   /* the corner it turns round */
    float a = u * PUZ_PI / 2, c = cosf(a), sn = sinf(a);
    *px = kx + 0.5f * (-ox * c + ix * sn);
    *py = ky + 0.5f * (-oy * c + iy * sn);
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the arrowhead pointing right in the unit square, centred on its head square's middle: its tip a little inside the
 * square's edge, its base across the square behind the middle, as wide as 1.7 ribbons; the ribbon ends under its
 * base (NECK before the head square's middle) */
static const float HEAD_XY[6] = {0.92f, 0.5f, 0.36f, 0.14f, 0.36f, 0.86f};
#define NECK 0.1f

static void head_poly(float cx, float cy, float s, int dir, float *xy)
{
    static const float ang[4] = {-PUZ_PI / 2, 0, PUZ_PI / 2, PUZ_PI};
    float c = cosf(ang[dir]), sn = sinf(ang[dir]);
    for (int i = 0; i < 3; i++) {
        float x = (HEAD_XY[2 * i] - 0.5f) * s, y = (HEAD_XY[2 * i + 1] - 0.5f) * s;
        xy[2 * i] = cx + x * c - y * sn;
        xy[2 * i + 1] = cy + x * sn + y * c;
    }
}

/* how lit a point of the head is, (dx, dy) pixels from its middle, s a square: lighter towards the top left */
static float head_lit(float dx, float dy, float s) { return clampf(-(dx * 0.6f + dy * 0.8f) / (0.35f * s), -1, 1); }

static void shade(const uint8_t rgb[3], float k, uint8_t out[3])
{
    for (int i = 0; i < 3; i++) out[i] = (uint8_t)clampf(k < 1 ? rgb[i] * k : rgb[i] + (255 - rgb[i]) * (k - 1), 0, 255);
}

/* arrow i of m painted into o (W x H), the board's first square at (ox, oy), s pixels a square: its soft shadow, its
 * ribbon (a rounded rod lit from the top left, lighter towards the head), its flat arrowhead in the same lacquer. The
 * ribbon's samples first leave, at each pixel near it, the distance to the nearest of them (so overlapping pieces
 * never darken); the head's shadow joins the ribbon's the same way (the most of the two) */
static void paint_arrow(uint8_t *o, int W, int H, const arx_map *m, int i, float ox, float oy, float s,
                        const uint8_t rgb[3])
{
    static const uint8_t dark[3] = {20, 12, 8};
    track t;
    track_of(m, i, &t);
    int len = m->p[i].len, n = 0;
    float r = WIDTH / 2 * s, t0 = -TAIL, t1 = len - 1, te = t1 - NECK, sx = 0.05f * s, sy = 0.07f * s, soft = 0.07f * s;
    float px[ARX_MAXLEN * 12 + 8], py[ARX_MAXLEN * 12 + 8], pf[ARX_MAXLEN * 12 + 8];
    for (float u = t0;; u += 0.1f) {
        float uu = fminf(u, te), x, y;
        track_at(&t, uu, &x, &y);
        px[n] = ox + x * s;
        py[n] = oy + y * s;
        pf[n++] = (uu - t0) / (t1 - t0 + 1e-3f);
        if (u >= te) break;
    }
    float hx, hy, head[6], hsh[6];
    track_at(&t, t1, &hx, &hy);
    hx = ox + hx * s;
    hy = oy + hy * s;
    head_poly(hx, hy, s, m->p[i].dir, head);
    head_poly(hx + sx, hy + sy, s, m->p[i].dir, hsh);
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, reach = r + soft + sx + sy + 2;
    for (int k = 0; k < n; k++) {
        x0 = fminf(x0, px[k]); x1 = fmaxf(x1, px[k]);
        y0 = fminf(y0, py[k]); y1 = fmaxf(y1, py[k]);
    }
    for (int k = 0; k < 3; k++) {
        x0 = fminf(x0, head[2 * k]); x1 = fmaxf(x1, hsh[2 * k]);
        y0 = fminf(y0, head[2 * k + 1]); y1 = fmaxf(y1, hsh[2 * k + 1]);
    }
    int bx = (int)fmaxf(0, floorf(x0 - reach)), by = (int)fmaxf(0, floorf(y0 - reach));
    int bw = (int)fminf((float)W, ceilf(x1 + reach)) - bx, bh = (int)fminf((float)H, ceilf(y1 + reach)) - by;
    if (bw <= 0 || bh <= 0) return;
    float *d = malloc(sizeof(float) * 5 * (size_t)bw * bh);
    if (!d) return;
    float *ds = d + bw * bh, *f = ds + bw * bh, *nx = f + bw * bh, *ny = nx + bw * bh;
    for (int k = 0; k < bw * bh; k++) d[k] = ds[k] = 1e9f;
    for (int k = 1; k < n; k++) {               /* each sample's segment: the distances near it */
        float ax = px[k - 1], ay = py[k - 1], ex = px[k], ey = py[k], vx = ex - ax, vy = ey - ay, l2 = vx * vx + vy * vy;
        int qx0 = (int)fmaxf(bx, floorf(fminf(ax, ex) - reach)), qx1 = (int)fminf(bx + bw - 1, ceilf(fmaxf(ax, ex) + reach));
        int qy0 = (int)fmaxf(by, floorf(fminf(ay, ey) - reach)), qy1 = (int)fminf(by + bh - 1, ceilf(fmaxf(ay, ey) + reach));
        for (int y = qy0; y <= qy1; y++)
            for (int x = qx0; x <= qx1; x++) {
                int j = (x - bx) + (y - by) * bw;
                float cx = x + 0.5f, cy = y + 0.5f, tt = l2 > 0 ? clampf(((cx - ax) * vx + (cy - ay) * vy) / l2, 0, 1) : 0;
                float qx = ax + tt * vx, qy = ay + tt * vy, dd = hypotf(cx - qx, cy - qy);
                if (dd < d[j]) {
                    d[j] = dd;
                    f[j] = pf[k - 1] + tt * (pf[k] - pf[k - 1]);
                    nx[j] = dd > 1e-3f ? (cx - qx) / dd : 0;
                    ny[j] = dd > 1e-3f ? (cy - qy) / dd : 0;
                }
                float dsh = pzk_seg_dist(cx - sx, cy - sy, ax, ay, ex, ey);
                if (dsh < ds[j]) ds[j] = dsh;
            }
    }
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            int j = x + y * bw, X = x + bx, Y = y + by;
            uint8_t *p = o + 4 * ((size_t)Y * W + X);
            float sh = clampf((r + soft - ds[j]) / (2 * soft), 0, 1);
            if (fabsf(X - hx - sx) < 0.6f * s && fabsf(Y - hy - sy) < 0.6f * s) sh = fmaxf(sh, pzt_cover(hsh, 3, (float)X, (float)Y));
            if (sh > 0) pzk_over(p, dark, 0.32f * sh);
            float cov = clampf(r - d[j] + 0.5f, 0, 1);
            if (cov > 0) {
                float e = clampf(d[j] / r, 0, 1), lit = -(nx[j] * 0.6f + ny[j] * 0.8f) * e;   /* the side towards the light */
                float k = 0.8f + 0.26f * f[j] + 0.16f * lit + 0.1f * (1 - e * e);
                uint8_t c[3];
                shade(rgb, k, c);
                pzk_over(p, c, cov);
            }
            if (fabsf(X - hx) < 0.6f * s && fabsf(Y - hy) < 0.6f * s) {   /* the head, flat, one soft light on it */
                float hcov = pzt_cover(head, 3, (float)X, (float)Y);
                if (hcov > 0) {
                    uint8_t c[3];
                    shade(rgb, 1.06f + 0.08f * head_lit(X + 0.5f - hx, Y + 0.5f - hy, s), c);
                    pzk_over(p, c, hcov);
                }
            }
        }
    free(d);
}


/* the plank w x h squares of s pixels in its frame f, the board's shape inlaid in it (a shallow, darker bed) */
static void paint_board(const arx_map *m, int s, int f, uint8_t *o)
{
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    static const float tone[3] = {0.9f, 0.87f, 0.84f};
    art_plank(m->w * s, m->h * s, f, o);
    for (int c = 0; c < m->w * m->h; c++)
        if (m->shape[c])
            pzk_inlay(o, W, H, f + c % m->w * s + 0.035f * s, f + c / m->w * s + 0.035f * s, 0.93f * s, 0.93f * s,
                      0.2f * s, tone, fmaxf(1, 0.04f * s));
}

/* each arrow a colour unlike its neighbours' (the arrows touching it) */
static void colours_of(const arx_map *m, uint8_t *col)
{
    uint8_t at[ARX_CELLS];
    memset(at, ARX_NONE, sizeof at);
    for (int i = 0; i < m->n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(m, i, cells);
        for (int k = 0; k < n; k++) at[cells[k]] = (uint8_t)i;
    }
    for (int i = 0; i < m->n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(m, i, cells), used = 0;
        for (int k = 0; k < n; k++)
            for (int e = 0; e < 4; e++) {
                int x = cells[k] % m->w + ARX_DX[e], y = cells[k] / m->w + ARX_DY[e];
                if (x < 0 || y < 0 || x >= m->w || y >= m->h) continue;
                int j = at[x + y * m->w];
                if (j != ARX_NONE && j < i) used |= 1 << col[j];
            }
        int c = (i * 5 + 3) % PALETTE;
        for (int t = 0; t < PALETTE && (used >> c & 1); t++) c = (c + 1) % PALETTE;
        col[i] = (uint8_t)c;
    }
}

/* a board in a picture size x size: its plank and its arrows */
static void paint_mini(const arx_map *m, int size, uint8_t *rgba)
{
    float span = fmaxf((float)m->w, (float)m->h) + 2 * FRAME;
    int s = (int)(size / span), f = (int)(FRAME * s);
    if (s < 2) return;
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4), col[ARX_MAXP];
    if (!o) return;
    paint_board(m, s, f, o);
    colours_of(m, col);
    for (int i = 0; i < m->n; i++) paint_arrow(o, W, H, m, i, (float)f, (float)f, (float)s, PAL[col[i]]);
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

/* the bar's mistake: a red disc, an ivory cross */
static void paint_miss(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t red[3] = {200, 56, 40};
    pzk_ball(o, size, size, size / 2.0f, size / 2.0f, 0.4f * size, red, 1, 0.6f);
    float a = 0.33f * size, b = 0.67f * size, w = 0.07f * size;
    pzk_stroke(o, size, size, a, a, b, b, w, IVORY, 1);
    pzk_stroke(o, size, size, a, b, b, a, w, IVORY, 1);
}

/* a little arrow bent once, its head to the right (the token, the first chapter's tab) */
static void paint_bent(int size, uint8_t *rgba, const char *board, int col)
{
    arx_map m;
    if (!arx_parse(&m, board)) return;
    float s = size / (fmaxf((float)m.w, (float)m.h) + 0.3f);
    float ox = (size - m.w * s) / 2, oy = (size - m.h * s) / 2;
    for (int i = 0; i < m.n; i++) paint_arrow(rgba, size, size, &m, i, ox, oy, s, PAL[(col + i) % PALETTE]);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_bent(size, rgba, "..|rR|u.", 0);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
/* each arrow's picture at rest, s pixels a square: the box of its squares and half a square round them (its head,
 * its shadow) */
static void make_arrows(int s)
{
    for (int i = 0; i < ARX_MAXP; i++) ui_free(&t_arrow[i]);
    for (int i = 0; i < G.m.n; i++) {
        int cells[ARX_MAXLEN], n = arx_cells(&G.m, i, cells), x0 = 99, y0 = 99, x1 = -1, y1 = -1;
        for (int k = 0; k < n; k++) {
            int x = cells[k] % G.m.w, y = cells[k] / G.m.w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        int W = (x1 - x0 + 2) * s, H = (y1 - y0 + 2) * s;
        uint8_t *o = calloc((size_t)W * H, 4);
        if (!o) continue;
        arrow_x[i] = x0 - 0.5f;
        arrow_y[i] = y0 - 0.5f;
        paint_arrow(o, W, H, &G.m, i, -arrow_x[i] * s, -arrow_y[i] * s, (float)s, PAL[colour[i]]);
        t_arrow[i] = ui_tex(W, H, o);
        free(o);
    }
}

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
        int W = w * s + 2 * f, H = h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
        if (made_q != s) {                      /* the arrowhead pointing right, near white (tinted when drawn), its soft
                                                 * light baked in */
            uint8_t *p = ui_buf(s, s);
            memset(p, 0, (size_t)s * s * 4);
            float xy[6];
            head_poly(s / 2.0f, s / 2.0f, (float)s, ARX_RIGHT, xy);
            for (int y = 0; y < s; y++)
                for (int x = 0; x < s; x++) {
                    float cov = pzt_cover(xy, 3, (float)x, (float)y);
                    if (cov <= 0) continue;
                    uint8_t c[3], v = (uint8_t)(255 * (0.93f + 0.07f * head_lit(x + 0.5f - s / 2.0f, y + 0.5f - s / 2.0f, s)));
                    c[0] = c[1] = c[2] = v;
                    pzk_over(p + 4 * (y * s + x), c, cov);
                }
            ui_free(&t_head);
            t_head = ui_tex(s, s, p);
        }
        made_q = s;
        make_arrows(s);
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
    colours_of(&G.m, colour);
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

/* how many squares lie ahead of arrow i's head to the edge */
static int ahead(int i)
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
        mo[i].fly_ms = (Uint32)(LIFT_MS + 150 + 24 * mo[i].fly_d);
        ui_sound(SND_MOVE);
        if (arx_solved(&G)) {
            win.on = 1;
            win.t0 = now + mo[i].fly_ms - 120;
            ui_sound_later(SND_END, (int)mo[i].fly_ms - 120);
        }
    } else {                                    /* a bump: its head runs into b, both wobble */
        mo[i].bump = 1;
        mo[i].bump_t0 = now;
        mo[i].bump_gap = (float)gap;
        mo[i].bump_hit = (Uint32)(90 + 45 * gap);
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
typedef struct { float s, ks, red, alpha, lift; int kd, moving; } pose;

static pose pose_of(int i, Uint32 now)
{
    pose p = {0, 0, 0, 1, 0, 0, 0};
    if (mo[i].fly) {
        float t = (float)(now - mo[i].fly_t0);
        if (t >= mo[i].fly_ms) { p.moving = 3; p.alpha = 0; return p; }
        p.moving = 2;
        p.lift = clampf(t / LIFT_MS, 0, 1);
        if (t < LIFT_MS) p.s = -0.1f * puz_ease(p.lift);
        else {
            float u = (t - LIFT_MS) / (mo[i].fly_ms - LIFT_MS), D = mo[i].fly_d;
            p.s = -0.1f + (D + 0.1f) * (0.3f * u + 0.7f * u * u);
        }
        return p;
    }
    if (mo[i].bump) {
        float t = (float)(now - mo[i].bump_t0), hit = (float)mo[i].bump_hit, A = mo[i].bump_gap + 0.16f;
        if (t < hit) {
            float u = t / hit;
            p.s = A * u * u;
            p.moving = 1;
        } else if (t < hit + BUMP_BACK_MS) {
            float tau = t - hit;
            p.s = A * expf(-tau / 85) * cosf(tau * 2 * PUZ_PI / 260);
            p.red = expf(-tau / 160);
            p.moving = 1;
        }
    }
    if (mo[i].wob) {
        float tau = (float)(Sint32)(now - mo[i].wob_t0);
        if (tau >= 0 && tau < BUMP_BACK_MS) {
            p.ks = 0.1f * expf(-tau / 90) * sinf(tau * 2 * PUZ_PI / 150);
            p.kd = mo[i].wob_dir;
            p.red = fmaxf(p.red, expf(-tau / 170));
            p.moving = 1;
        }
    }
    return p;
}

/* how much a point (squares from the board's top left) still shows: fading past the board's edge */
static float shows(float x, float y)
{
    float out = fmaxf(fmaxf(-x, x - G.m.w), fmaxf(-y, y - G.m.h));
    return clampf(1 - (out - 0.15f) / 1.5f, 0, 1);
}

/* arrow i drawn as it is posed: pass 0 its shadow, 1 its ribbon, 2 its sheen, 3 its head; glow: gold light added */
static void draw_arrow(int i, const pose *p, int pass, float glow)
{
    track t;
    track_of(&G.m, i, &t);
    int len = G.m.p[i].len, dir = G.m.p[i].dir;
    const uint8_t *rgb = PAL[colour[i]];
    float t0 = p->s - TAIL, t1 = p->s + len - 1, W = WIDTH * q, kx = p->ks * ARX_DX[p->kd], ky = p->ks * ARX_DY[p->kd];
    float red = clampf(p->red, 0, 1), gb = 1 - 0.18f * red;
    if (pass == 3) {
        float hx, hy;
        track_at(&t, t1, &hx, &hy);
        hx += kx;
        hy += ky;
        float a = shows(hx, hy) * p->alpha, sx = bx0 + hx * q, sy = by0 + hy * q, deg = 90.0f * (dir - 1);
        uint8_t c[3];
        shade(rgb, 1.13f, c);
        SDL_SetTextureAlphaMod(t_head, (Uint8)(80 * a));
        SDL_SetTextureColorMod(t_head, 0, 0, 0);
        ui_put_rot(t_head, sx + (0.05f + 0.06f * p->lift) * q, sy + (0.07f + 0.06f * p->lift) * q, q, q, deg);
        SDL_SetTextureAlphaMod(t_head, (Uint8)(255 * a));
        SDL_SetTextureColorMod(t_head, c[0], (Uint8)(c[1] * gb), (Uint8)(c[2] * gb));
        ui_put_rot(t_head, sx, sy, q, q, deg);
        if (glow > 0) {
            SDL_SetTextureBlendMode(t_head, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(t_head, 255, 190, 80);
            SDL_SetTextureAlphaMod(t_head, (Uint8)(255 * clampf(glow, 0, 1)));
            ui_put_rot(t_head, sx, sy, q, q, deg);
            SDL_SetTextureBlendMode(t_head, SDL_BLENDMODE_BLEND);
        }
        SDL_SetTextureColorMod(t_head, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_head, 255);
        return;
    }
    float px = 0, py = 0, pa = 0;
    for (float u = t0;; u += 0.2f) {
        float uu = fminf(u, t1 - NECK), x, y;
        track_at(&t, uu, &x, &y);
        x += kx;
        y += ky;
        float a = shows(x, y) * p->alpha;
        if (u > t0 && (a > 0 || pa > 0)) {
            float f = (uu - t0) / (t1 - t0 + 1e-3f), al = fminf(a, pa) * 0.5f + fmaxf(a, pa) * 0.5f;
            float ax = bx0 + px * q, ay = by0 + py * q, bx = bx0 + x * q, by = by0 + y * q;
            if (pass == 0) {
                float o = (0.05f + 0.06f * p->lift) * q, o2 = (0.07f + 0.06f * p->lift) * q;
                pzk_dot(bx + o, by + o2, W, 0.08f * q, (pzk_rgba){0, 0, 0, (Uint8)(38 * al)}, 0);   /* (dots: even) */
            } else if (pass == 1) {
                uint8_t c[3];
                shade(rgb, 0.8f + 0.28f * f, c);
                pzk_rgba k = {c[0], (Uint8)(c[1] * gb), (Uint8)(c[2] * gb), (Uint8)(255 * al)};
                pzk_line(ax, ay, bx, by, W, 0, k, 0);
                pzk_dot(bx, by, W, 0, k, 0);
                if (glow > 0) {
                    pzk_rgba g = {255, 190, 80, (Uint8)(255 * clampf(glow, 0, 1) * al)};
                    pzk_line(ax, ay, bx, by, W, 0, g, 1);
                }
            }
        }
        if (u > t0 || pass == 1) {
            if (u <= t0 && pass == 1 && a > 0) {   /* the tail's round end */
                uint8_t c[3];
                shade(rgb, 0.8f, c);
                pzk_dot(bx0 + x * q, by0 + y * q, W, 0, (pzk_rgba){c[0], (Uint8)(c[1] * gb), (Uint8)(c[2] * gb), (Uint8)(255 * a)}, 0);
            }
        }
        px = x;
        py = y;
        pa = a;
        if (u >= t1 - NECK) break;
    }
}

/* an arrow at rest drawn from its picture: moved by its knock (or the hint's nudge), flushed red, gold light added */
static void put_arrow(int i, const pose *p, float glow)
{
    SDL_Texture *t = t_arrow[i];
    if (!t) return;
    int tw, th;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    float x = bx0 + (arrow_x[i] + p->ks * ARX_DX[p->kd]) * q, y = by0 + (arrow_y[i] + p->ks * ARX_DY[p->kd]) * q;
    Uint8 gb = (Uint8)(255 - 45 * clampf(p->red, 0, 1));
    SDL_SetTextureColorMod(t, 255, gb, gb);
    ui_put(t, x, y, (float)tw, (float)th);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    if (glow > 0) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(t, 255, 190, 80);
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(glow, 0, 1)));
        ui_put(t, x, y, (float)tw, (float)th);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, 255);
    }
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
        int cells[ARX_MAXLEN], n = arx_cells(&G.m, hp, cells);
        for (int k = 0; k < n; k++)
            ui_mark_gold_a(bx0 + (cells[k] % G.m.w + 0.5f) * q, by0 + (cells[k] / G.m.w + 0.5f) * q, 1.7f * q,
                           0.5f + 0.4f * pulse);
        P[hp].ks = 0.08f * pulse * pulse;
        P[hp].kd = G.m.p[hp].dir;
        any = 1;
    }
    /* the arrows: red halos round a knock, those at rest from their pictures, those bumping drawn bending along their
     * path, those slithering off on top */
    for (int i = 0; i < G.m.n; i++) {          /* a red halo round the arrows of a knock */
        if (G.gone[i] || P[i].red < 0.02f || P[i].moving > 1) continue;
        int cells[ARX_MAXLEN], n = arx_cells(&G.m, i, cells);
        float kx = P[i].ks * ARX_DX[P[i].kd], ky = P[i].ks * ARX_DY[P[i].kd];
        for (int k = 0; k < n; k++)
            pzk_dot(bx0 + (cells[k] % G.m.w + 0.5f + kx) * q, by0 + (cells[k] / G.m.w + 0.5f + ky) * q, 0.8f * q,
                    0.3f * q, (pzk_rgba){236, 64, 40, (Uint8)(170 * P[i].red)}, 0);
    }
    for (int i = 0; i < G.m.n; i++)            /* at rest (or knocked, nudged): their pictures */
        if (!G.gone[i] && P[i].moving < 2 && P[i].s == 0) put_arrow(i, &P[i], i == hp ? 0.12f + 0.2f * pulse : 0);
    for (int pass = 0; pass < 4; pass++)       /* bumping: drawn as they bend along their path */
        for (int i = 0; i < G.m.n; i++)
            if (!G.gone[i] && P[i].moving < 2 && P[i].s != 0) draw_arrow(i, &P[i], pass, 0);
    for (int i = 0; i < G.m.n; i++) {
        if (P[i].moving != 2) continue;
        for (int pass = 0; pass < 4; pass++) draw_arrow(i, &P[i], pass, 0);
        float since = P[i].s - mo[i].fly_edge;  /* the spark where its head leaves the board */
        if (since > -0.2f) {
            const arx_piece *p = &G.m.p[i];
            float hx = p->x + 0.5f + mo[i].fly_edge * ARX_DX[(int)p->dir], hy = p->y + 0.5f + mo[i].fly_edge * ARX_DY[(int)p->dir];
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
/* the arrow under (x, y), a little slack round the squares: -1 none */
static int piece_at(float x, float y)
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    int cx = (int)floorf(fx), cy = (int)floorf(fy);
    if (cx >= 0 && cy >= 0 && cx < G.m.w && cy < G.m.h && G.at[cx + cy * G.m.w] != ARX_NONE)
        return G.at[cx + cy * G.m.w];
    int best = -1;
    float bd = 0.62f;                           /* a near miss: the nearest arrow's square */
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
    if (i < 0) i = piece_at(x, y);
    if (i >= 0) tap_piece(i);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_ex(b, NULL, G.left, t_miss, G.mistakes, 0, 0);   /* (the token already shows an arrow) */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the menu's card: a heart of winding arrows; a picker's: a board of that size */
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

/* a chapter's tab: a short bent arrow; a long one; a spiral; the showpieces of the bigger boards, the shapes, all */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    switch (chapter) {
    case 0: paint_bent(size, rgba, "..|rR|u.", 0); break;
    case 1: paint_bent(size, rgba, "rrd.|u.d.|u.rR", 2); break;
    case 2: paint_bent(size, rgba, "U...|ulll|.dlu|.rru", 4); break;
    default: {
        static arx_game t;
        if (arx_level(&t, (chapter + 1) * (ARX_LEVELS / ARX_CHAPTERS) - 1)) paint_mini(&t.m, size, rgba);
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
