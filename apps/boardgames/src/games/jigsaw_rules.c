/*
 * Jigsaw's rules: the grid, the knobs, the outlines, the tray and the table, snapping and groups, the hint, undo, the
 * stars, the levels and the script. No SDL (tests/jigsaw_test.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/jigsaw.h"
#include "games/jigsaw_pictures.h"
#include "games/jigsaw_levels.h"
#include "games/puzrand.h"

const int jig_counts[JIG_COUNTS] = {12, 24, 48, 96, 150};

/* ---- the pictures and the levels --------------------------------------------------------------------------------- */
#define NPICS ((int)(sizeof JIG_PICS / sizeof JIG_PICS[0]))
#define NLEVELS ((int)(sizeof JIG_LEVELS / sizeof JIG_LEVELS[0]))
#define NCHAPTERS ((int)(sizeof JIG_CHAPTER_START / sizeof JIG_CHAPTER_START[0]))

int jig_pictures(void) { return NPICS; }
const jig_picture *jig_pic(int i) { return &JIG_PICS[i < 0 || i >= NPICS ? 0 : i]; }
int jig_levels(void) { return NLEVELS; }
int jig_chapters(void) { return NCHAPTERS; }

int jig_chapter_of(int level)
{
    int c = 0;
    while (c + 1 < NCHAPTERS && level >= JIG_CHAPTER_START[c + 1]) c++;
    return c;
}

int jig_chapter_first(int chapter)
{
    return JIG_CHAPTER_START[chapter < 0 ? 0 : chapter >= NCHAPTERS ? NCHAPTERS - 1 : chapter];
}

static int lv(int level) { return level < 0 ? 0 : level >= NLEVELS ? NLEVELS - 1 : level; }
int jig_level_pic(int level) { return JIG_LEVELS[lv(level)][0]; }
int jig_level_count(int level) { return JIG_LEVELS[lv(level)][1]; }
int jig_level_rotate(int level) { return JIG_LEVELS[lv(level)][2]; }

/* ---- the cut ----------------------------------------------------------------------------------------------------- */
void jig_grid(int w, int h, int pieces, int *cols, int *rows)
{
    float best = 1e9f;
    *cols = *rows = 2;
    for (int c = 2; c <= 20; c++)
        for (int r = 2; r <= 20; r++) {
            if (c * r > JIG_MAX) continue;
            float count = fabsf(logf((float)(c * r) / pieces)), shape = fabsf(logf(((float)w / c) / ((float)h / r)));
            float s = 3 * count + shape;
            if (s < best) best = s, *cols = c, *rows = r;
        }
}

static float frand(uint32_t *s, float lo, float hi) { return lo + (hi - lo) * (pr_next(s) & 0xffffff) / 16777215.0f; }

static void knob_make(jig_knob *k, uint32_t *s)
{
    k->sign = pr_below(s, 2) ? 1 : -1;
    k->c = frand(s, 0.46f, 0.54f);
    k->n = frand(s, 0.06f, 0.078f);
    k->r = frand(s, 0.1f, 0.122f);
    k->h = frand(s, 0.12f, 0.145f);
    k->t = frand(s, -0.025f, 0.025f);
    k->w1 = frand(s, -0.03f, 0.03f);
    k->w2 = frand(s, -0.03f, 0.03f);
}

/* a cubic Bézier's points after p0 (m of them, its end the last) */
static float *cubic(float *o, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int m)
{
    for (int i = 1; i <= m; i++) {
        float t = (float)i / m, a = (1 - t) * (1 - t) * (1 - t), b = 3 * (1 - t) * (1 - t) * t, c = 3 * (1 - t) * t * t,
              d = t * t * t;
        *o++ = a * x0 + b * x1 + c * x2 + d * x3;
        *o++ = a * y0 + b * y1 + c * y2 + d * y3;
    }
    return o;
}

void jig_edge(const jig_knob *k, float *uv)
{
    /* the shoulder, the neck going in, the bulb (a near circle, tilted), back down the neck, the other shoulder */
    float c = k->c, n = k->n, r = k->r, h = k->h, t = k->t;
    float bx = c - n, cx = c - r + 0.5f * t, dx = c + t, ex = c + r + 0.5f * t, fx = c + n;
    float *o = uv;
    *o++ = 0;
    *o++ = 0;                                   /* the start; then 6 segments of 8, the last point (1, 0) dropped */
    o = cubic(o, 0, 0, 0.28f, k->w1, bx - 0.03f, -0.035f, bx, 0, 8);
    o = cubic(o, bx, 0, bx + 0.03f, 0.035f, cx, h - 0.6f * r, cx, h, 8);
    o = cubic(o, cx, h, cx, h + 0.552f * r, dx - 0.552f * r, h + r, dx, h + r, 8);
    o = cubic(o, dx, h + r, dx + 0.552f * r, h + r, ex, h + 0.552f * r, ex, h, 8);
    o = cubic(o, ex, h, ex, h - 0.6f * r, fx - 0.03f, 0.035f, fx, 0, 8);
    o = cubic(o, fx, 0, fx + 0.03f, -0.035f, 0.72f, k->w2, 1, 0, 7);
    (void)o;
}

static const jig_knob *hknob(const jig_game *g, int i, int j) { return &g->hk[(j - 1) * g->cols + i]; }
static const jig_knob *vknob(const jig_game *g, int i, int j) { return &g->vk[j * (g->cols - 1) + i - 1]; }

/* one side of a piece into xy (from its start, its end left out): a straight border, or the knobbed edge, forward or
 * backward. horizontal: the line y = j over column i; vertical: the line x = i beside row j. */
static int side(const jig_game *g, float *xy, int horiz, int i, int j, int back)
{
    int border = horiz ? j == 0 || j == g->rows : i == 0 || i == g->cols;
    float uv[2 * JIG_EDGE_PTS + 2];
    int m;
    if (border) {
        uv[0] = uv[1] = 0;
        m = 1;
    } else {
        jig_edge(horiz ? hknob(g, i, j) : vknob(g, i, j), uv);
        m = JIG_EDGE_PTS;
    }
    uv[2 * m] = 1;
    uv[2 * m + 1] = 0;
    float sg = border ? 0 : (horiz ? hknob(g, i, j) : vknob(g, i, j))->sign;
    for (int q = 0; q < m; q++) {               /* backward: from the end, the start left out */
        int s = back ? m - q : q;
        float u = uv[2 * s], v = sg * uv[2 * s + 1];
        xy[2 * q] = horiz ? i + u : i + v;
        xy[2 * q + 1] = horiz ? j + v : j + u;
    }
    return m;
}

int jig_outline(const jig_game *g, int k, float *xy)
{
    int i = k % g->cols, j = k / g->cols, n = 0;
    n += side(g, xy + 2 * n, 1, i, j, 0);       /* the top, left to right */
    n += side(g, xy + 2 * n, 0, i + 1, j, 0);   /* the right, down */
    n += side(g, xy + 2 * n, 1, i, j + 1, 1);   /* the bottom, right to left */
    n += side(g, xy + 2 * n, 0, i, j, 1);       /* the left, up */
    return n;
}

int jig_is_edge(const jig_game *g, int k)
{
    int i = k % g->cols, j = k / g->cols;
    return i == 0 || j == 0 || i == g->cols - 1 || j == g->rows - 1;
}

void jig_home(const jig_game *g, int k, float *x, float *y)
{
    *x = k % g->cols + 0.5f;
    *y = k / g->cols + 0.5f;
}

int jig_solved(const jig_game *g) { return g->n > 0 && g->s.placed == g->n; }

void jig_new(jig_game *g, int pic, int count, int rotate, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    const jig_picture *P = jig_pic(pic);
    g->pic = pic < 0 || pic >= NPICS ? 0 : pic;
    g->count = count < 0 ? 0 : count >= JIG_COUNTS ? JIG_COUNTS - 1 : count;
    g->rotate = rotate != 0;
    g->seed = seed;
    jig_grid(P->w, P->h, jig_counts[g->count], &g->cols, &g->rows);
    g->n = g->cols * g->rows;
    uint32_t s = pr_seed(seed);
    for (int j = 1; j < g->rows; j++)
        for (int i = 0; i < g->cols; i++) knob_make(&g->hk[(j - 1) * g->cols + i], &s);
    for (int j = 0; j < g->rows; j++)
        for (int i = 1; i < g->cols; i++) knob_make(&g->vk[j * (g->cols - 1) + i - 1], &s);
    int order[JIG_MAX];
    for (int k = 0; k < g->n; k++) order[k] = k;
    pr_shuffle(&s, order, g->n);
    for (int k = 0; k < g->n; k++) {
        jig_piece *p = &g->s.p[k];
        jig_home(g, k, &p->x, &p->y);
        p->where = JIG_TRAY;
        p->group = (int16_t)k;
        p->rot = (int8_t)(g->rotate ? 1 + pr_below(&s, 3) : 0);   /* turned: never upright at first */
        g->s.tray[k] = (int16_t)order[k];
    }
    g->s.ntray = (int16_t)g->n;
}

void jig_load(jig_game *g, int level)
{
    level = lv(level);
    jig_new(g, jig_level_pic(level), jig_level_count(level), jig_level_rotate(level), 7919u * (uint32_t)level + 17u);
}

void jig_random(jig_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= JIG_VARIANTS) variant = 0;
    uint32_t s = pr_seed(seed ^ 0x51ed27u);
    jig_new(g, pr_below(&s, NPICS), variant / 2, variant % 2, seed);
}

/* ---- moves ------------------------------------------------------------------------------------------------------- */
static void push(jig_game *g)
{
    if (g->nh == JIG_HIST) {
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (JIG_HIST - 1));
        g->nh--;
    }
    g->hist[g->nh++] = g->s;
}

int jig_undo(jig_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    return 1;
}

static void z_remove(jig_pos *s, int k)
{
    for (int i = 0; i < s->nz; i++)
        if (s->z[i] == k) {
            memmove(s->z + i, s->z + i + 1, sizeof s->z[0] * (size_t)(s->nz - i - 1));
            s->nz--;
            return;
        }
}

static void tray_remove(jig_pos *s, int k)
{
    for (int i = 0; i < s->ntray; i++)
        if (s->tray[i] == k) {
            memmove(s->tray + i, s->tray + i + 1, sizeof s->tray[0] * (size_t)(s->ntray - i - 1));
            s->ntray--;
            return;
        }
}

/* the group's pieces to the top of the table's pile (their order kept) */
static void z_raise(jig_game *g, int grp)
{
    jig_pos *s = &g->s;
    int16_t keep[JIG_MAX], up[JIG_MAX];
    int a = 0, b = 0;
    for (int i = 0; i < s->nz; i++) {
        if (s->p[s->z[i]].group == grp) up[b++] = s->z[i];
        else keep[a++] = s->z[i];
    }
    memcpy(s->z, keep, sizeof keep[0] * (size_t)a);
    memcpy(s->z + a, up, sizeof up[0] * (size_t)b);
}

static void rot_vec(int rot, float dx, float dy, float *x, float *y)   /* a quarter turn clockwise, rot times */
{
    for (int r = 0; r < (rot & 3); r++) {
        float t = dx;
        dx = -dy;
        dy = t;
    }
    *x = dx;
    *y = dy;
}

static void group_shift(jig_game *g, int grp, float dx, float dy)
{
    for (int k = 0; k < g->n; k++)
        if (g->s.p[k].group == grp && g->s.p[k].where == JIG_TABLE) g->s.p[k].x += dx, g->s.p[k].y += dy;
}

static void group_place(jig_game *g, int grp)
{
    for (int k = 0; k < g->n; k++) {
        jig_piece *p = &g->s.p[k];
        if (p->group != grp || p->where == JIG_PLACED) continue;
        if (p->where == JIG_TRAY) tray_remove(&g->s, k);
        else z_remove(&g->s, k);
        p->where = JIG_PLACED;
        p->rot = 0;
        p->group = -1;
        jig_home(g, k, &p->x, &p->y);
        g->s.placed++;
    }
}

static void group_join(jig_game *g, int a, int b)   /* b's pieces into a's group (the lower index names it) */
{
    int to = a < b ? a : b, from = a < b ? b : a;
    for (int k = 0; k < g->n; k++)
        if (g->s.p[k].group == from || g->s.p[k].group == a || g->s.p[k].group == b) g->s.p[k].group = (int16_t)to;
    (void)from;
}

/* group grp (on the table) snapped: to its place, then to its neighbours, again and again while it grows */
static int snap(jig_game *g, int grp)
{
    int done = 0;
    for (;;) {
        int changed = 0;
        for (int k = 0; k < g->n && !changed; k++) {
            jig_piece *p = &g->s.p[k];
            if (p->group != grp || p->where != JIG_TABLE) continue;
            float hx, hy;
            jig_home(g, k, &hx, &hy);
            if (p->rot == 0 && fabsf(p->x - hx) < JIG_TOL && fabsf(p->y - hy) < JIG_TOL) {
                group_place(g, grp);
                return done | JIG_PLACE | JIG_SNAP;
            }
            int i = k % g->cols, j = k / g->cols;
            static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int d = 0; d < 4; d++) {
                int ni = i + D[d][0], nj = j + D[d][1];
                if (ni < 0 || nj < 0 || ni >= g->cols || nj >= g->rows) continue;
                int b = nj * g->cols + ni;
                jig_piece *q = &g->s.p[b];
                if (q->where == JIG_TRAY || q->group == grp) continue;
                float ox, oy;
                if (q->where == JIG_PLACED) {   /* a placed neighbour: only upright, at its place (above) */
                    continue;
                }
                if (q->rot != p->rot) continue;
                rot_vec(p->rot, (float)D[d][0], (float)D[d][1], &ox, &oy);
                float ex = p->x + ox, ey = p->y + oy;
                if (fabsf(q->x - ex) < JIG_TOL && fabsf(q->y - ey) < JIG_TOL) {
                    group_shift(g, grp, q->x - ex, q->y - ey);   /* the group moves to the one lying there */
                    int other = q->group;
                    group_join(g, grp, other);
                    grp = grp < other ? grp : other;
                    z_raise(g, grp);
                    done |= JIG_SNAP;
                    changed = 1;
                    break;
                }
            }
        }
        if (!changed) return done;
    }
}

int jig_drop(jig_game *g, int k, float x, float y)
{
    if (k < 0 || k >= g->n) return 0;
    jig_piece *p = &g->s.p[k];
    if (p->where == JIG_PLACED) return 0;
    push(g);
    if (p->where == JIG_TRAY) {
        tray_remove(&g->s, k);
        p->where = JIG_TABLE;
        p->x = x;
        p->y = y;
        g->s.z[g->s.nz++] = (int16_t)k;
    } else {
        group_shift(g, p->group, x - p->x, y - p->y);
    }
    z_raise(g, p->group);
    g->s.moves++;
    int r = snap(g, p->group);
    if (jig_solved(g)) r |= JIG_DONE;
    return r;
}

int jig_to_tray(jig_game *g, int k, int at)
{
    if (k < 0 || k >= g->n) return 0;
    jig_piece *p = &g->s.p[k];
    if (p->where != JIG_TABLE) return 0;
    for (int q = 0; q < g->n; q++)
        if (q != k && g->s.p[q].group == p->group) return 0;   /* a group stays on the table */
    push(g);
    z_remove(&g->s, k);
    p->where = JIG_TRAY;
    if (at < 0 || at > g->s.ntray) at = g->s.ntray;
    memmove(g->s.tray + at + 1, g->s.tray + at, sizeof g->s.tray[0] * (size_t)(g->s.ntray - at));
    g->s.tray[at] = (int16_t)k;
    g->s.ntray++;
    jig_home(g, k, &p->x, &p->y);
    return 1;
}

int jig_rotate(jig_game *g, int k)
{
    if (!g->rotate || k < 0 || k >= g->n) return 0;
    jig_piece *p = &g->s.p[k];
    if (p->where == JIG_PLACED) return 0;
    push(g);
    if (p->where == JIG_TRAY) {
        p->rot = (int8_t)((p->rot + 1) & 3);
        return 1;
    }
    float cx = p->x, cy = p->y;
    for (int q = 0; q < g->n; q++) {
        jig_piece *o = &g->s.p[q];
        if (o->group != p->group || o->where != JIG_TABLE) continue;
        float dx, dy;
        rot_vec(1, o->x - cx, o->y - cy, &dx, &dy);
        o->x = cx + dx;
        o->y = cy + dy;
        o->rot = (int8_t)((o->rot + 1) & 3);
    }
    z_raise(g, p->group);
    g->s.moves++;
    snap(g, p->group);
    return 1;
}

int jig_hint_pick(const jig_game *g)
{
    if (jig_solved(g)) return -1;
    int frame = 1;                              /* the border first, while it is not whole */
    for (int k = 0; k < g->n; k++)
        if (jig_is_edge(g, k) && g->s.p[k].where != JIG_PLACED) frame = 0;
    int best = -1, bs = -1;
    for (int k = 0; k < g->n; k++) {
        const jig_piece *p = &g->s.p[k];
        if (p->where == JIG_PLACED) continue;
        int i = k % g->cols, j = k / g->cols, by = 0;
        if (i > 0 && g->s.p[k - 1].where == JIG_PLACED) by++;
        if (i + 1 < g->cols && g->s.p[k + 1].where == JIG_PLACED) by++;
        if (j > 0 && g->s.p[k - g->cols].where == JIG_PLACED) by++;
        if (j + 1 < g->rows && g->s.p[k + g->cols].where == JIG_PLACED) by++;
        int corner = (i == 0 || i == g->cols - 1) && (j == 0 || j == g->rows - 1);
        int s = 4 * by + (frame || jig_is_edge(g, k) ? 20 : 0) + (corner ? 2 : 0);
        if (s > bs) bs = s, best = k;
    }
    return best;
}

int jig_hint(jig_game *g)
{
    int k = jig_hint_pick(g);
    if (k < 0) return -1;
    push(g);
    jig_piece *p = &g->s.p[k];
    if (p->where == JIG_TRAY) {                 /* alone out of the tray */
        tray_remove(&g->s, k);
        p->where = JIG_TABLE;
        g->s.z[g->s.nz++] = (int16_t)k;
    }
    group_place(g, p->group);
    g->hints++;
    g->s.moves++;
    return k;
}

/* ---- the stars --------------------------------------------------------------------------------------------------- */
int jig_target_s(const jig_game *g) { return g->n * (g->rotate ? 12 : 8); }

int jig_slow(const jig_game *g, int seconds)
{
    int t = jig_target_s(g);
    return seconds <= t ? 0 : seconds <= 2 * t ? 1 : 3;
}

int jig_stars(const jig_game *g, int seconds)
{
    int m = g->hints + jig_slow(g, seconds);
    return m == 0 ? 3 : m <= 2 ? 2 : 1;
}

/* ---- the script -------------------------------------------------------------------------------------------------- */
static int place_one(jig_game *g, int k)        /* p<k>: turned upright first, then dropped at its place */
{
    if (k < 0 || k >= g->n) return 0;
    if (g->s.p[k].where == JIG_PLACED) return 1;
    for (int t = 0; t < 4 && g->s.p[k].rot && g->s.p[k].where != JIG_PLACED; t++) jig_rotate(g, k);
    if (g->s.p[k].where == JIG_PLACED) return 1;   /* turned, it snapped already */
    float x, y;
    jig_home(g, k, &x, &y);
    jig_drop(g, k, x, y);
    return 1;
}

int jig_script(jig_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        char c = *s++;
        int k = 0, n = 0;
        float x = 0, y = 0;
        switch (c) {
        case 'p':
            if (sscanf(s, "%d%n", &k, &n) != 1 || !place_one(g, k)) return 0;
            break;
        case 'm':
            if (sscanf(s, "%d@%f,%f%n", &k, &x, &y, &n) != 3 || k < 0 || k >= g->n) return 0;
            jig_drop(g, k, x, y);
            break;
        case 't': {
            int at = 0;
            if (sscanf(s, "%d@%d%n", &k, &at, &n) != 2 || !jig_to_tray(g, k, at)) return 0;
            break;
        }
        case 'r':
            if (sscanf(s, "%d%n", &k, &n) != 1 || !jig_rotate(g, k)) return 0;
            break;
        case 'h':
            jig_hint(g);
            break;
        case 'u':
            if (!jig_undo(g)) return 0;
            break;
        case 'E':
            for (int q = 0; q < g->n; q++)
                if (jig_is_edge(g, q)) place_one(g, q);
            break;
        case 'N':
            if (sscanf(s, "%d%n", &k, &n) != 1) return 0;
            for (int q = 0; q < k && q < g->n; q++) place_one(g, q);
            break;
        default:
            return 0;
        }
        s += n;
        if (*s && *s != ' ' && *s != ',') return 0;
    }
    return 1;
}
