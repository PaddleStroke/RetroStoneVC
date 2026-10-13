/*
 * Snakes and ladders in RetroStone Board, for two to four: the numbered board, its wooden ladders and its snakes, the
 * pawns, the die in the bar of the player to throw; tap to throw, the pawn hops on its own (the rules: snakes_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The ladders and the snakes are painted once (at the board's size, for each way round) on a picture laid over the
 * squares: figures in squares, from the art kit, each with a soft shadow.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"
#include "games/snakes.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define HOP_MS 170                              /* a hop to the next square */
#define PAUSE_MS 140                            /* on the foot of a ladder, or a snake's head, before it acts */
#define LINK_MS 150                             /* a square's length up a ladder or down a snake */
#define K 0.1f                                  /* the figures' units, a square */
#define PI_F 3.14159265f

static sn_game G;
static int sq, fr, made = -1, made_over[2] = {-1, -1};
static float bx, by;                            /* the board's top left square corner */
static SDL_Texture *t_board, *t_over[2], *t_pawn[SN_PLAYERS];
static struct { int on, hops, linked, at; sn_move m; Uint32 t0; } anim;   /* t0 0: the die still tumbles */

static const art_style WOOD = {{226, 172, 104}, {58, 36, 20}, {112, 72, 40}, {0, 0, 0}, 0.30f, 0};
static const art_style SKIN[2] = {
    {{148, 150, 72}, {38, 40, 18}, {70, 72, 30}, {92, 96, 42}, 0.40f, 0},       /* olive, darker blotches */
    {{206, 158, 70}, {62, 40, 16}, {112, 76, 30}, {146, 98, 42}, 0.40f, 0},     /* ochre */
};
static const art_style EYE = {{240, 232, 200}, {30, 26, 16}, {30, 26, 16}, {22, 20, 14}, 0.10f, 0};
static const art_style TONGUE = {{184, 52, 46}, {90, 24, 20}, {90, 24, 20}, {0, 0, 0}, 0.0f, 0};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    (void)variant;
    sn_new_players(&G, ui.players);
    anim.on = 0;
}

static int status(void)
{
    int st = sn_status(&G);
    return st == SN_PLAY ? GS_PLAY : gs_win(st - SN_WIN0);
}

static int turn(void) { return sn_turn(&G); }

static int busy(void) { return anim.on || ui_dice_busy(); }   /* the end waits for the pawn to reach 100 */

/* the die thrown: a throw undone comes back the same, else a new one; the pawn moves once the die lies still */
static void throw_die(void)
{
    int v = sn_next(&G), p = sn_turn(&G);
    if (!v) v = 1 + ui_random(6);
    ui_dice_throw(1, &v, p);
    sn_play(&G, v);
    anim.m = G.last;
    anim.on = ui_dice_busy();                   /* the picture mode: no tumble, no hops, the pawn is there */
    anim.t0 = 0;
    anim.hops = anim.linked = 0;
    anim.at = anim.m.from;
}

static int undo(void)
{
    if (!sn_undo(&G)) return 0;
    anim.on = 0;
    return 1;
}

/* "4 6 2": the throws, in order (the screenshots and the tests choose them) */
static int script(const char *s)
{
    for (char *end; *s; s = end) {
        long v = strtol(s, &end, 10);
        if (end == s) break;
        if (!sn_play(&G, (int)v)) return 0;
    }
    anim.on = 0;
    return 1;
}

/* ---- the bot (snakes_bot.c) -------------------------------------------------------------------------------------- */
const sn_game *snakes_live(void) { return &G; }

int snakes_show(const char *move)
{
    if (strcmp(move, "roll") || status() != GS_PLAY || busy()) return 0;
    throw_die();
    return 1;
}

/* the player who threw each throw: a 6 throws again */
static int last_throw(int player)
{
    int v = 0;
    for (int i = 0, p = 0; i < G.n; i++) {
        if (p == player) v = G.roll[i];
        if (G.roll[i] != 6) p = (p + 1) % G.players;
    }
    return v;
}

/* ---- the figures: drawn in squares, painted with a soft shadow --------------------------------------------------- */
enum { MAXP = 32, MAXPT = 48 };                 /* the art kit's limits: shapes in a figure, points in a shape */
typedef struct {
    art_fig *f;
    float x0, y0, x1, y1;                       /* its extent, in squares */
    int n, np[MAXP];
    float pt[MAXP][2 * MAXPT];                  /* its shapes again (squares): which parts of the picture to paint */
} sketch;

static void sk_new(sketch *s)
{
    s->f = art_fig_new();
    s->x0 = s->y0 = 1e9f;
    s->x1 = s->y1 = -1e9f;
    s->n = 0;
}

static void sk_poly(sketch *s, int role, const float *xy, int n)
{
    float p[2 * MAXPT];
    n = n > MAXPT ? MAXPT : n;
    for (int i = 0; i < 2 * n; i += 2) {
        p[i] = xy[i] * K;
        p[i + 1] = xy[i + 1] * K;
        s->x0 = fminf(s->x0, xy[i]);
        s->x1 = fmaxf(s->x1, xy[i]);
        s->y0 = fminf(s->y0, xy[i + 1]);
        s->y1 = fmaxf(s->y1, xy[i + 1]);
    }
    art_poly(s->f, role, p, n);
    if (s->n < MAXP) {
        memcpy(s->pt[s->n], xy, sizeof(float) * 2 * n);
        s->np[s->n++] = n;
    }
}

/* 1: a shape of the sketch comes within r of (x, y) (squares) */
static int sk_near(const sketch *s, float x, float y, float r)
{
    for (int k = 0; k < s->n; k++) {
        const float *p = s->pt[k];
        int in = 0, n = s->np[k];
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float ax = p[2 * j], ay = p[2 * j + 1], bx_ = p[2 * i], by_ = p[2 * i + 1];
            if ((by_ > y) != (ay > y) && x < (ax - bx_) * (y - by_) / (ay - by_) + bx_) in = !in;
            float ex = ax - bx_, ey = ay - by_, t = ((x - bx_) * ex + (y - by_) * ey) / (ex * ex + ey * ey + 1e-12f);
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            if (hypotf(x - bx_ - ex * t, y - by_ - ey * t) < r) return 1;
        }
        if (in) return 1;
    }
    return 0;
}

/* a bar w wide from (ax, ay) to (bx, by) */
static void sk_bar(sketch *s, float ax, float ay, float bx_, float by_, float w)
{
    float dx = bx_ - ax, dy = by_ - ay, l = hypotf(dx, dy), nx = -dy / l * w / 2, ny = dx / l * w / 2;
    const float p[8] = {ax + nx, ay + ny, bx_ + nx, by_ + ny, bx_ - nx, by_ - ny, ax - nx, ay - ny};
    sk_poly(s, ART_PART, p, 4);
}

/* an oval at (cx, cy) along the axis (ux, uy): a ahead (front) and b behind, h across */
static void sk_oval(sketch *s, int role, float cx, float cy, float ux, float uy, float a, float b, float h)
{
    float p[36];
    for (int i = 0; i < 18; i++) {
        float t = i * 2 * PI_F / 18, c = cosf(t), along = c * (c > 0 ? a : b), across = sinf(t) * h;
        p[2 * i] = cx + ux * along - uy * across;
        p[2 * i + 1] = cy + uy * along + ux * across;
    }
    sk_poly(s, role, p, 18);
}

static void soften(uint8_t *a, int w, int h, int r)  /* a box blur of the alpha, across then down */
{
    uint8_t *line = malloc((size_t)(w > h ? w : h));
    for (int pass = 0; pass < 2; pass++) {
        int n = pass ? h : w, m = pass ? w : h, step = pass ? 4 * w : 4;
        for (int j = 0; j < m; j++) {
            uint8_t *row = a + 4 * (pass ? j : j * w) + 3;
            for (int i = 0; i < n; i++) {
                int sum = 0, cnt = 0;
                for (int k = i - r; k <= i + r; k++)
                    if (k >= 0 && k < n) sum += row[k * step], cnt++;
                line[i] = (uint8_t)(sum / cnt);
            }
            for (int i = 0; i < n; i++) row[i * step] = line[i];
        }
    }
    free(line);
}

/* paints a sketch (and frees it) into dst (dw x dh), on its soft shadow (shadow: 1): px pixels a square, the
 * squares' (0, 0) at pixel (ox, oy). Tile by tile, only where a shape comes: a long snake's slant is mostly empty. */
static void paint(sketch *s, art_style st, int shadow, uint8_t *dst, int dw, int dh, float px, float ox, float oy)
{
    enum { T = 16 };
    float pad = 2 + 0.12f * px, scale = px / K;
    int x0 = (int)floorf(ox + s->x0 * px - pad), y0 = (int)floorf(oy + s->y0 * px - pad);
    int w = (int)ceilf(ox + s->x1 * px + pad) - x0, h = (int)ceilf(oy + s->y1 * px + pad) - y0;
    uint8_t *buf = calloc((size_t)w * h, 4), tile[T * T * 4];
    st.line = fmaxf(1.3f / scale, 0.024f * K);
    float reach = (0.71f * T + 2) / px + st.line / K;
    for (int ty = 0; ty < h; ty += T)
        for (int tx = 0; tx < w; tx += T) {
            int tw = w - tx < T ? w - tx : T, th = h - ty < T ? h - ty : T;
            if (!sk_near(s, (x0 + tx + tw / 2.0f - ox) / px, (y0 + ty + th / 2.0f - oy) / px, reach)) continue;
            art_render(s->f, &st, tw, th, scale, ox - x0 - tx, oy - y0 - ty, tile);
            for (int j = 0; j < th; j++) memcpy(buf + 4 * ((ty + j) * w + tx), tile + 4 * j * tw, 4 * (size_t)tw);
        }
    if (shadow) {                               /* its shape in black, soft, a little to the lower right */
        uint8_t *sh = malloc((size_t)w * h * 4);
        for (int i = 0; i < w * h * 4; i += 4) sh[i] = sh[i + 1] = sh[i + 2] = 0, sh[i + 3] = buf[i + 3];
        soften(sh, w, h, (int)(0.045f * px) + 1);
        for (int i = 3; i < w * h * 4; i += 4) sh[i] = (uint8_t)(sh[i] * 0.34f);
        art_blend(dst, dw, dh, sh, w, h, x0 + (int)(0.06f * px), y0 + (int)(0.09f * px));
        free(sh);
    }
    art_blend(dst, dw, dh, buf, w, h, x0, y0);
    art_fig_free(s->f);
    free(buf);
}

/* a ladder from its foot to its top (squares' centres): two rails, rungs a third of a square apart */
static void ladder(float fx, float fy, float tx, float ty, uint8_t *dst, int dw, int dh, float px, float ox, float oy)
{
    float dx = tx - fx, dy = ty - fy, len = hypotf(dx, dy), ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    float a = -0.24f, b = len + 0.24f, half = 0.25f;
    int n = (int)((b - a) / 0.34f + 0.5f);
    n = n < 2 ? 2 : n > 28 ? 28 : n;
    sketch s;
    sk_new(&s);
    for (int i = 1; i < n; i++) {               /* the rungs first: the rails' edges cross over them */
        float t = a + (b - a) * i / n, cx = fx + ux * t, cy = fy + uy * t;
        sk_bar(&s, cx - nx * half, cy - ny * half, cx + nx * half, cy + ny * half, 0.075f);
    }
    for (int side = -1; side <= 1; side += 2)
        sk_bar(&s, fx + ux * a + nx * half * side, fy + uy * a + ny * half * side, fx + ux * b + nx * half * side,
               fy + uy * b + ny * half * side, 0.095f);
    paint(&s, WOOD, 1, dst, dw, dh, px, ox, oy);
}

/* a point of a snake's spine, t from its head (0) to its tail (1): waves across the line between them */
static void spine(const float *e, int kind, float t, float *x, float *y)
{
    float dx = e[2] - e[0], dy = e[3] - e[1], len = hypotf(dx, dy), nx = -dy / len, ny = dx / len;
    int k = (int)(len / 1.3f + 0.5f);
    float a = 0.36f * (kind & 1 ? 1 : -1) * sinf(PI_F * (k < 2 ? 2 : k) * t) * (1 - 0.35f * t);
    *x = e[0] + dx * t + nx * a;
    *y = e[1] + dy * t + ny * a;
}

/* a snake from its head to its tail (e: their centres): a body tapering to a point, blotches, a head with eyes */
static void snake(const float *e, int kind, uint8_t *dst, int dw, int dh, float px, float ox, float oy)
{
    enum { N = 24 };
    float side[2][2 * N], len = hypotf(e[2] - e[0], e[3] - e[1]);
    for (int i = 0; i < N; i++) {
        float t = i / (N - 1.0f), x, y, x1, y1;
        spine(e, kind, t, &x, &y);
        spine(e, kind, t + 0.01f, &x1, &y1);
        float l = hypotf(x1 - x, y1 - y), nx = -(y1 - y) / l, ny = (x1 - x) / l;
        float w = 0.165f * powf(1 - t, 0.75f) * (t < 0.08f ? 0.82f + 2.25f * t : 1);   /* a neck behind the head */
        side[0][2 * i] = x + nx * w;
        side[0][2 * i + 1] = y + ny * w;
        side[1][2 * i] = x - nx * w;
        side[1][2 * i + 1] = y - ny * w;
    }
    float body[4 * N];
    int n = 0;
    for (int i = 0; i < N; i++, n += 2) body[n] = side[0][2 * i], body[n + 1] = side[0][2 * i + 1];
    for (int i = N - 2; i >= 0; i--, n += 2) body[n] = side[1][2 * i], body[n + 1] = side[1][2 * i + 1];
    sketch s, eyes, tongue;
    sk_new(&s);
    sk_poly(&s, ART_PART, body, n / 2);
    int m = (int)(len * 2.4f);                  /* the blotches down the back, smaller to the tail */
    m = m < 4 ? 4 : m > 24 ? 24 : m;
    for (int i = 0; i < m; i++) {
        float t = 0.11f + 0.78f * i / (m - 1), x, y, x1, y1;
        spine(e, kind, t, &x, &y);
        spine(e, kind, t + 0.01f, &x1, &y1);
        float l = hypotf(x1 - x, y1 - y), ux = (x1 - x) / l, uy = (y1 - y) / l, w = 0.10f * powf(1 - t, 0.75f);
        float along = 0.9f * w + 0.03f, across = 0.85f * w;
        const float d[8] = {x - ux * along, y - uy * along, x - uy * across, y + ux * across,
                            x + ux * along, y + uy * along, x + uy * across, y - ux * across};
        sk_poly(&s, ART_MARK, d, 4);
    }
    float x, y;                                 /* the head looks away from the body */
    spine(e, kind, 0.03f, &x, &y);
    float l = hypotf(e[0] - x, e[1] - y), ux = (e[0] - x) / l, uy = (e[1] - y) / l;
    float cx = e[0] + ux * 0.02f, cy = e[1] + uy * 0.02f;
    sk_oval(&s, ART_PART, cx, cy, ux, uy, 0.25f, 0.17f, 0.165f);
    sk_new(&tongue);
    const float fork[][2] = {{0.24f, -0.02f}, {0.36f, -0.016f}, {0.43f, -0.07f}, {0.385f, 0}, {0.43f, 0.07f},
                             {0.36f, 0.016f}, {0.24f, 0.02f}};
    float tp[14];
    for (int i = 0; i < 7; i++) {
        tp[2 * i] = cx + ux * fork[i][0] - uy * fork[i][1];
        tp[2 * i + 1] = cy + uy * fork[i][0] + ux * fork[i][1];
    }
    sk_poly(&tongue, ART_PART, tp, 7);
    sk_new(&eyes);
    for (int k = -1; k <= 1; k += 2) {
        float ex = cx + ux * 0.07f - uy * 0.085f * k, ey = cy + uy * 0.07f + ux * 0.085f * k;
        sk_oval(&eyes, ART_PART, ex, ey, ux, uy, 0.048f, 0.048f, 0.042f);
        sk_oval(&eyes, ART_MARK, ex + ux * 0.012f, ey + uy * 0.012f, ux, uy, 0.024f, 0.024f, 0.024f);
    }
    paint(&s, SKIN[kind & 1], 1, dst, dw, dh, px, ox, oy);
    paint(&tongue, TONGUE, 0, dst, dw, dh, px, ox, oy);
    paint(&eyes, EYE, 0, dst, dw, dh, px, ox, oy);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a square's centre, in squares from the board's top left corner (0: off the board, on the frame left of 1) */
static void centre(int square, int flip, float *x, float *y)
{
    int c = 0, r = 0;
    if (square > 0) sn_cell(square, &c, &r);
    float col = square > 0 ? c + 0.5f : -0.3f;
    *x = flip ? SN_SIDE - col : col;
    *y = (flip ? r : SN_SIDE - 1 - r) + 0.5f;
}

static void make_over(int flip)
{
    int n = SN_SIDE * sq;
    uint8_t *o = calloc((size_t)n * n, 4);
    for (int i = 0; i < SN_LINKS; i++) {        /* the ladders, then the snakes over them */
        float e[4];
        centre(sn_links[i].from, flip, &e[0], &e[1]);
        centre(sn_links[i].to, flip, &e[2], &e[3]);
        if (sn_links[i].to > sn_links[i].from) ladder(e[0], e[1], e[2], e[3], o, n, n, (float)sq, 0, 0);
        else snake(e, i, o, n, n, (float)sq, 0, 0);
    }
    ui_free(&t_over[flip]);
    t_over[flip] = ui_tex(n, n, o);
    made_over[flip] = sq;
    free(o);
}

static void layout(box b)
{
    sq = (int)floorf(b.w / (SN_SIDE + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = (float)SN_SIDE * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != sq) {
        made = sq;
        int n = SN_SIDE * sq + 2 * fr;
        ui_free(&t_board);
        art_board_n(SN_SIDE, SN_SIDE, sq, fr, 1, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        for (int p = 0; p < SN_PLAYERS; p++) {
            ui_free(&t_pawn[p]);
            art_piece(PAWN, p, sq, ui_buf(sq, sq));
            t_pawn[p] = ui_tex(sq, sq, ui_buf(sq, sq));
        }
    }
    if (made_over[ui.flipped] != sq) make_over(ui.flipped);
}

/* where the moving pawn is (squares) and how high it hops; 0: the move is over */
static int moving(float *x, float *y, float *lift)
{
    const sn_move *m = &anim.m;
    *lift = 0;
    if (!anim.on) return 0;
    if (!anim.t0) {
        if (ui_dice_busy()) { centre(m->from, ui.flipped, x, y); return 1; }
        anim.t0 = ui_ms();
    }
    int t = (int)(ui_ms() - anim.t0);
    if (t < m->roll * HOP_MS) {                 /* square by square, a sound as it lands on each */
        int k = t / HOP_MS;
        float f = (t - k * HOP_MS) / (float)HOP_MS, x1, y1;
        for (; anim.hops < k; anim.hops++) ui_sound(SND_MOVE);
        centre(sn_hop(m->from, k), ui.flipped, x, y);
        centre(sn_hop(m->from, k + 1), ui.flipped, &x1, &y1);
        f = f * f * (3 - 2 * f);
        *x += (x1 - *x) * f;
        *y += (y1 - *y) * f;
        *lift = 0.3f * sinf(PI_F * f);
        anim.at = sn_hop(m->from, k + (f > 0.5f));
        return 1;
    }
    for (; anim.hops < m->roll; anim.hops++) ui_sound(SND_MOVE);
    anim.at = m->land;
    t -= m->roll * HOP_MS + PAUSE_MS;
    if (m->to == m->land) return anim.on = 0;
    centre(m->land, ui.flipped, x, y);
    if (t < 0) return 1;
    int l = sn_link_at(m->land), up = m->to > m->land;
    float e[4];
    centre(m->land, ui.flipped, &e[0], &e[1]);
    centre(m->to, ui.flipped, &e[2], &e[3]);
    float dur = LINK_MS * (1.5f + hypotf(e[2] - e[0], e[3] - e[1]) * (up ? 1 : 0.8f)), f = t / dur;
    if (!anim.linked) {
        anim.linked = 1;
        ui_sound(up ? SND_CHECK : SND_CAPTURE);
    }
    if (f >= 1) {
        anim.at = m->to;
        return anim.on = 0;
    }
    f = f * f * (3 - 2 * f);
    if (up) {
        *x = e[0] + (e[2] - e[0]) * f;
        *y = e[1] + (e[3] - e[1]) * f;
    } else {
        spine(e, l, f, x, y);
    }
    return 1;
}

/* a pawn standing on a square (its centre: x, y, pixels), size wide */
static void pawn(int p, float x, float y, float size)
{
    int up = ui_upside(p);                      /* turned round: its foot towards the top */
    ui_stand_at(t_pawn[p], x - size / 2, y - (up ? 0.45f : 0.55f) * size, size, up);
}

/* where the i-th of m pawns on one square stands (squares from its centre), and how big: one alone in the middle, two
 * side by side, three or four in two rows (dy < 0: the back row) */
static void place(int i, int m, float *dx, float *dy, float *size)
{
    static const float AT[4][4][2] = {
        {{0, 0}},
        {{-0.21f, 0}, {0.21f, 0}},
        {{-0.21f, -0.17f}, {0.21f, -0.17f}, {0, 0.17f}},
        {{-0.21f, -0.17f}, {0.21f, -0.17f}, {-0.21f, 0.17f}, {0.21f, 0.17f}},
    };
    static const float SIZE[4] = {0.96f, 0.8f, 0.62f, 0.62f};
    *dx = AT[m - 1][i][0];
    *dy = AT[m - 1][i][1];
    *size = SIZE[m - 1];
}

static void draw(void)
{
    float n = (float)SN_SIDE * sq, x, y;
    ui_put(t_board, bx - fr, by - fr, n + 2 * fr, n + 2 * fr);
    for (int s = 1; s <= SN_LAST; s++) {        /* the numbers, small, in each square's top left corner */
        centre(s, ui.flipped, &x, &y);
        int c = (int)x, r = (int)y, digits = s < 10 ? 1 : s < 100 ? 2 : 3;
        float h = 0.2f * sq, w = 0.74f * h * (digits - 1) + 0.62f * h;
        ui_number(bx + c * sq + 0.08f * sq + w / 2, by + r * sq + 0.08f * sq + h / 2, h, s, (c + r) & 1);
    }
    float lift, mx, my;
    int mov = moving(&mx, &my, &lift);
    if (mov) ui_animate();
    if (!mov && G.n) {                          /* the last move: where it left, where it ended */
        if (G.last.from) {
            centre(G.last.from, ui.flipped, &x, &y);
            ui_mark_last(bx + (x - 0.5f) * sq, by + (y - 0.5f) * sq, (float)sq, (float)sq);
        }
        centre(G.last.to, ui.flipped, &x, &y);
        ui_mark_last(bx + (x - 0.5f) * sq, by + (y - 0.5f) * sq, (float)sq, (float)sq);
    }
    ui_put(t_over[ui.flipped], bx, by, n, n);
    /* the order: the ones standing still first, the one to move (or moving) last, on top; the pawns sharing a square
     * together, its back row first */
    int np = G.players, top = mov ? anim.m.player : sn_turn(&G), order[SN_PLAYERS], done[SN_PLAYERS] = {0};
    for (int k = 0; k < np; k++) order[k] = (top + 1 + k) % np;
    for (int k = 0; k < np; k++) {
        int p = order[k];
        if (mov && p == anim.m.player) {
            if (!anim.t0 && !anim.m.from) continue;     /* not on the board yet: it comes on once the die lies still */
            float up = (ui_upside(p) ? -1 : 1) * lift;   /* a hop goes up for the player it faces */
            pawn(p, bx + mx * sq, by + (my - up) * sq, 0.96f * sq);
            continue;
        }
        if (!G.pos[p] || done[p]) continue;     /* off the board: in the bar */
        int with[SN_PLAYERS], m = 0;            /* the pawns on its square, in the players' order */
        for (int o = 0; o < np; o++)
            if (G.pos[o] == G.pos[p] && !(mov && o == anim.m.player)) with[m++] = o;
        centre(G.pos[p], ui.flipped, &x, &y);
        for (int row = 0; row < 2; row++)
            for (int j = k; j < np; j++) {
                int o = order[j], i = 0;
                while (i < m && with[i] != o) i++;
                float dx, dy, size;
                if (i == m || done[o]) continue;
                place(i, m, &dx, &dy, &size);
                if ((dy < 0) != !row) continue;
                pawn(o, bx + (x + (ui.flipped ? -dx : dx)) * sq, by + (y + dy) * sq, size * sq);
                done[o] = 1;
            }
    }
}

/* ---- the pointer: a tap anywhere throws the die (the board has nothing else to tap) ------------------------------ */
static void press(float x, float y)
{
    (void)x, (void)y;
    if (status() != GS_PLAY || busy()) return;
    throw_die();
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the square the player is on, and the die: thrown, waiting to be thrown (in gold light), or the last throw (dimmed) */
static void bar(int player, box b)
{
    float u = ui.unit, cy = b.y + b.h / 2, d = 0.6f * u, cx = b.x + b.w - 0.6f * u;
    int at = anim.on && anim.m.player == player ? anim.at : G.pos[player];
    if (at) ui_number(b.x + 0.45f * u, cy, 0.46f * u, at, 1);
    if (anim.on && anim.m.player == player) {
        ui_dice_draw(cx, cy, d, 0);
        return;
    }
    int v = last_throw(player), waiting = status() == GS_PLAY && sn_turn(&G) == player && !anim.on;
    if (!v && !waiting) return;
    if (waiting) ui_mark_gold(cx, cy, 1.9f * d);
    ui_die_at(cx, cy, d, player, v ? v : 6, waiting ? 1 : 110 / 255.0f);
}

static void token(int player, int size, uint8_t *rgba) { art_piece(PAWN, player, size, rgba); }

/* where each one sits: 0 and 2 at the bottom (left, right), 1 and 3 at the top (left, right); the die goes from one
 * side to the other at each turn */
static void home(int player, float *fx, float *fy)
{
    *fx = player < 2 ? 0.25f : 0.75f;
    *fy = player & 1 ? 0.0f : 1.0f;
}

/* the menu's card: a corner of the board, a ladder, a snake, a pawn */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    float px = (float)s, ox = (float)(o + f);
    ladder(0.5f, 3.5f, 1.5f, 0.5f, rgba, size, size, px, ox, ox);
    const float e[4] = {3.5f, 0.5f, 2.5f, 3.5f};
    snake(e, 1, rgba, size, size, px, ox, ox);
    uint8_t *one = ui_buf(s, s);
    art_piece(PAWN, 0, s, one);
    art_blend(rgba, size, size, one, s, s, o + f + 1 * s, o + f + 2 * s + s / 4);
}

static const game snakes_game = {
    .id = "snakes", .order = 150, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1u << 2 | 1u << 3 | 1u << 4, .home = home,
};
REGISTER_GAME(snakes_game)
