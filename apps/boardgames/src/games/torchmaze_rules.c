/*
 * Torch Maze for one player: the rules (torchmaze.h). No SDL: the tests and the levels' tool use them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/torchmaze.h"

#define BRAZIER_R 3.2f                          /* a lit brazier's light, cells */
#define FADE_SOFT 6                             /* the steps a fading cell takes to go */
#define MAX_STATES 3000000
#define ONEWAY_COST 12                          /* the sensible explorer goes through a one-way door last */

const char tmz_alpha[65] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";
const uint8_t tmz_allow[TMZ_CHAPTERS] = {20, 20, 20, 20, 18, 20};

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

int tmz_par_of(int best, int allow) { return best <= 0 ? 0 : (best * allow + 50) / 100; }

/* ---- the maze's edges ------------------------------------------------------------------------------------------- */
/* the edge from c that way: its slot (side 0 right, 1 below) and cell; 0: off the grid */
static int edge_of(const tmz_map *m, int c, int dir, int *side, int *cell)
{
    int x = c % m->w, y = c / m->w;
    switch (dir) {
    case TMZ_RIGHT: if (x + 1 >= m->w) return 0; *side = 0; *cell = c; return 1;
    case TMZ_LEFT: if (x <= 0) return 0; *side = 0; *cell = c - 1; return 1;
    case TMZ_DOWN: if (y + 1 >= m->h) return 0; *side = 1; *cell = c; return 1;
    default: if (y <= 0) return 0; *side = 1; *cell = c - m->w; return 1;
    }
}

int tmz_next(const tmz_map *m, int c, int dir)
{
    int x = c % m->w + DX[dir & 3], y = c / m->w + DY[dir & 3];
    if (x < 0 || y < 0 || x >= m->w || y >= m->h || !m->in[x + y * m->w]) return -1;
    return x + y * m->w;
}

int tmz_opening(const tmz_map *m, int c, int dir)
{
    int s, e;
    if (tmz_next(m, c, dir) < 0 || !edge_of(m, c, dir, &s, &e)) return 0;
    return m->e[s][e] != TMZ_WALL;
}

static int edge_type(const tmz_map *m, int c, int dir, int *k)
{
    int s, e;
    if (tmz_next(m, c, dir) < 0 || !edge_of(m, c, dir, &s, &e)) return TMZ_WALL;
    if (k) *k = m->k[s][e];
    return m->e[s][e];
}

static void set_edge(tmz_map *m, int c, int dir, int type, int k)
{
    int s, e;
    if (!edge_of(m, c, dir, &s, &e)) return;
    m->e[s][e] = (uint8_t)type;
    m->k[s][e] = (uint8_t)k;
}

/* may one walk from c that way, the keys and gates as in s (the fuel not counted) */
static int passable(const tmz_map *m, const tmz_state *s, int c, int dir)
{
    int k = 0, t = edge_type(m, c, dir, &k);
    switch (t) {
    case TMZ_OPEN: return 1;
    case TMZ_DOOR: return s->keys >> k & 1;
    case TMZ_ONEWAY: return dir == k;
    case TMZ_GATE: return (k ^ s->gates) & 1;
    default: return 0;
    }
}

int tmz_can(const tmz_map *m, const tmz_state *s, int dir)
{
    if (m->fuel && s->fuel <= 0) return 0;
    return passable(m, s, s->at, dir & 3);
}

float tmz_light(const tmz_map *m, const tmz_state *s)
{
    float r = m->radius / 10.0f;
    if (!m->fuel) return r;
    if (s->fuel <= 0) return 0;
    return 1.0f + (r - 1.0f) * (float)s->fuel / (float)m->fuel;
}

/* ---- light ------------------------------------------------------------------------------------------------------- */
/* the light crossing the edge on side `side` (0 right, 1 below) of cell (ex, ey): 1 it passes */
static int clear(const tmz_map *m, const tmz_state *s, int ex, int ey, int side)
{
    int nx = ex + (side == 0), ny = ey + (side == 1);
    if (ex < 0 || ey < 0 || nx >= m->w || ny >= m->h) return 0;
    int a = ex + ey * m->w, b = nx + ny * m->w;
    if (!m->in[a] || !m->in[b]) return 0;
    switch (m->e[side][a]) {
    case TMZ_OPEN: case TMZ_GATE: return 1;
    case TMZ_DOOR: return s->keys >> m->k[side][a] & 1;
    default: return 0;
    }
}

int tmz_ray(const tmz_map *m, const tmz_state *s, float x0, float y0, float x1, float y1, float tol)
{
    float dx = x1 - x0, dy = y1 - y0;
    int cx = (int)floorf(x0), cy = (int)floorf(y0), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    float tx = 2, ty = 2, ddx = 2, ddy = 2;
    if (fabsf(dx) > 1e-6f) { tx = (sx > 0 ? cx + 1 - x0 : x0 - cx) / fabsf(dx); ddx = 1 / fabsf(dx); }
    if (fabsf(dy) > 1e-6f) { ty = (sy > 0 ? cy + 1 - y0 : y0 - cy) / fabsf(dy); ddy = 1 / fabsf(dy); }
    for (int guard = 0; guard < 64; guard++) {
        float t = fminf(tx, ty);
        if (t >= 1) return 1;
        if (tx < ty) {
            if (fabsf(x1 - (sx > 0 ? cx + 1 : cx)) > tol && !clear(m, s, sx > 0 ? cx : cx - 1, cy, 0)) return 0;
            cx += sx;
            tx += ddx;
        } else {
            if (fabsf(y1 - (sy > 0 ? cy + 1 : cy)) > tol && !clear(m, s, cx, sy > 0 ? cy : cy - 1, 1)) return 0;
            cy += sy;
            ty += ddy;
        }
    }
    return 1;
}

/* the cells a light at (px, py) of radius r shows: mark[c] = 1 */
static void look(const tmz_map *m, const tmz_state *s, float px, float py, float r, uint8_t *mark)
{
    int x0 = (int)floorf(px - r - 1), x1 = (int)ceilf(px + r + 1), y0 = (int)floorf(py - r - 1),
        y1 = (int)ceilf(py + r + 1);
    static const float P[5][2] = {{0.5f, 0.5f}, {0.2f, 0.2f}, {0.8f, 0.2f}, {0.2f, 0.8f}, {0.8f, 0.8f}};
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < m->h; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < m->w; x++) {
            int c = x + y * m->w;
            if (!m->in[c] || mark[c]) continue;
            if (hypotf(x + 0.5f - px, y + 0.5f - py) > r + 0.5f) continue;
            for (int i = 0; i < 5; i++) {
                float qx = x + P[i][0], qy = y + P[i][1];
                if (hypotf(qx - px, qy - py) <= r + 0.15f && tmz_ray(m, s, px, py, qx, qy, 0)) {
                    mark[c] = 1;
                    break;
                }
            }
        }
    if (r > 0) {
        int c = (int)floorf(px) + (int)floorf(py) * m->w;
        if (c >= 0 && c < m->w * m->h && m->in[c]) mark[c] = 1;
    }
}

static int brazier_cell(const tmz_map *m, int b)
{
    for (int c = 0; c < m->w * m->h; c++)
        if (m->item[c] == TMZ_BRAZIER && m->ik[c] == b) return c;
    return -1;
}

static void remember(tmz_game *g)
{
    static uint8_t mark[TMZ_CELLS];
    const tmz_map *m = &g->m;
    memset(mark, 0, sizeof mark);
    int at = g->s.at;
    look(m, &g->s, at % m->w + 0.5f, at / m->w + 0.5f, tmz_light(m, &g->s), mark);
    for (int b = 0; b < m->nbraz; b++) {
        int c = brazier_cell(m, b);
        if (g->lit >> b & 1 && c >= 0) look(m, &g->s, c % m->w + 0.5f, c / m->w + 0.5f, BRAZIER_R, mark);
    }
    for (int c = 0; c < m->w * m->h; c++)
        if (mark[c]) {
            if (!g->first[c]) g->first[c] = g->tick;
            g->last[c] = g->tick;
        }
}

int tmz_remembered(const tmz_game *g, int c, float *alpha)
{
    float a = 0;
    if (c >= 0 && c < g->m.w * g->m.h && g->first[c]) {
        int age = (int)(uint16_t)(g->tick - g->last[c]);
        a = !g->m.fade || age <= g->m.fade ? 1 : 1 - (float)(age - g->m.fade) / FADE_SOFT;
        if (a < 0) a = 0;
    }
    if (alpha) *alpha = a;
    return a > 0;
}

/* ---- moves ------------------------------------------------------------------------------------------------------- */
/* the explorer steps into the cell that way (allowed): the step, the fuel, what lies there */
static void enter(const tmz_map *m, tmz_state *s, int dir)
{
    int n = tmz_next(m, s->at, dir);
    s->at = (int16_t)n;
    s->steps++;
    s->face = (uint8_t)dir;
    if (m->fuel) s->fuel--;
    switch (m->item[n]) {
    case TMZ_KEY: s->keys |= (uint8_t)(1u << m->ik[n]); break;
    case TMZ_OIL:
        if (!(s->flasks >> m->ik[n] & 1)) {
            s->flasks |= (uint8_t)(1u << m->ik[n]);
            s->fuel = (int16_t)m->fuel;
        }
        break;
    case TMZ_PLATE: s->gates ^= 1; break;
    default: break;
    }
}

void tmz_begin(tmz_game *g)
{
    memset(&g->s, 0, sizeof g->s);
    g->s.at = (int16_t)g->m.start;
    g->s.fuel = (int16_t)g->m.fuel;
    g->s.face = TMZ_DOWN;
    g->nh = 0;
    g->tick = 1;
    memset(g->first, 0, sizeof g->first);
    memset(g->last, 0, sizeof g->last);
    g->lit = 0;
    g->hints = 0;
    remember(g);
}

int tmz_solved(const tmz_game *g) { return g->s.at == g->m.exit; }

static void stepped(tmz_game *g)
{
    int c = g->s.at;
    if (g->m.item[c] == TMZ_BRAZIER) g->lit |= (uint8_t)(1u << g->m.ik[c]);
    g->tick++;
    remember(g);
}

static void push(tmz_game *g)
{
    if (g->nh >= TMZ_HIST) {
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (TMZ_HIST - 1));
        g->nh--;
    }
    g->hist[g->nh++] = g->s;
}

int tmz_step(tmz_game *g, int dir)
{
    if (tmz_solved(g) || !tmz_can(&g->m, &g->s, dir)) return 0;
    push(g);
    enter(&g->m, &g->s, dir & 3);
    stepped(g);
    return 1;
}

/* c in a room: one of the four 2 x 2 squares round it open throughout */
static int in_room(const tmz_map *m, int c)
{
    for (int q = 0; q < 4; q++) {
        int x = c % m->w - (q & 1), y = c / m->w - (q >> 1);
        if (x < 0 || y < 0 || x + 1 >= m->w || y + 1 >= m->h) continue;
        int a = x + y * m->w;
        if (tmz_opening(m, a, TMZ_RIGHT) && tmz_opening(m, a, TMZ_DOWN) && tmz_opening(m, a + 1, TMZ_DOWN) &&
            tmz_opening(m, a + m->w, TMZ_RIGHT))
            return 1;
    }
    return 0;
}

/* the sides open at c across the way d (bit 0 its left, bit 1 its right) */
static int sides(const tmz_map *m, int c, int d)
{
    return tmz_opening(m, c, (d + 3) & 3) | tmz_opening(m, c, (d + 1) & 3) << 1;
}

int tmz_run(tmz_game *g, int dir, int16_t *cells, int *n)
{
    const tmz_map *m = &g->m;
    int k = 0;
    dir &= 3;
    if (tmz_solved(g) || !tmz_can(m, &g->s, dir)) { if (n) *n = 0; return 0; }
    push(g);
    static uint8_t seen[TMZ_CELLS];
    memset(seen, 0, sizeof seen);
    seen[g->s.at] = 1;
    int prev = g->s.at;
    for (;;) {
        enter(m, &g->s, dir);
        stepped(g);
        int c = g->s.at;
        if (cells && k < TMZ_PATH) cells[k] = (int16_t)c;
        k++;
        seen[c] = 1;
        if (c == m->exit || m->item[c] != TMZ_NONE) break;
        int open[4], no = 0;
        for (int o = 0; o < 4; o++)
            if (o != ((dir + 2) & 3) && tmz_opening(m, c, o)) open[no++] = o;
        int next = -1;
        if (no == 1) next = open[0];
        else if (no >= 2 && tmz_opening(m, c, dir) && in_room(m, c) && in_room(m, prev) &&
                 sides(m, c, dir) == sides(m, prev, dir))
            next = dir;                         /* across a room, along its wall */
        if (next < 0 || edge_type(m, c, next, NULL) != TMZ_OPEN || !tmz_can(m, &g->s, next)) break;
        if (seen[tmz_next(m, c, next)] || k >= TMZ_PATH) break;
        prev = c;
        dir = next;
    }
    if (n) *n = k;
    return k;
}

int tmz_undo(tmz_game *g)
{
    if (g->nh <= 0) return 0;
    g->s = g->hist[--g->nh];
    g->tick++;
    remember(g);
    return 1;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct { int K, G, F, U; long total; } dims;

static dims dims_of(const tmz_map *m)
{
    dims d;
    d.K = 1 << m->ncol;
    d.G = 1;
    for (int s = 0; s < 2; s++)
        for (int c = 0; c < m->w * m->h; c++)
            if (m->e[s][c] == TMZ_GATE) d.G = 2;
    d.F = 1 << m->nflask;
    d.U = m->fuel ? m->fuel + 1 : 1;
    d.total = (long)m->w * m->h * d.K * d.G * d.F * d.U;
    return d;
}

static long code(const dims *d, const tmz_state *s)
{
    return (((((long)s->at * d->K + s->keys) * d->G + s->gates) * d->F + s->flasks) * d->U + (d->U > 1 ? s->fuel : 0));
}

static void decode(const dims *d, long v, tmz_state *s)
{
    s->fuel = (int16_t)(v % d->U); v /= d->U;
    s->flasks = (uint8_t)(v % d->F); v /= d->F;
    s->gates = (uint8_t)(v % d->G); v /= d->G;
    s->keys = (uint8_t)(v % d->K); v /= d->K;
    s->at = (int16_t)v;
}

void tmz_solve(const tmz_map *m, const tmz_state *s0, tmz_way *w)
{
    w->steps = -1;
    w->n = 0;
    w->states = 0;
    dims d = dims_of(m);
    if (d.total > MAX_STATES) return;
    int32_t *par = malloc(sizeof *par * (size_t)d.total), *q = malloc(sizeof *q * (size_t)d.total);
    if (!par || !q) { free(par); free(q); return; }
    memset(par, 0xff, sizeof *par * (size_t)d.total);
    tmz_state s = *s0;
    if (!m->fuel) s.fuel = 0;
    long root = code(&d, &s), goal = -1;
    par[root] = (int32_t)root;
    int head = 0, tail = 0;
    q[tail++] = (int32_t)root;
    while (head < tail) {
        long v = q[head++];
        tmz_state a;
        decode(&d, v, &a);
        if (a.at == m->exit) { goal = v; break; }
        if (m->fuel && a.fuel <= 0) continue;
        for (int dir = 0; dir < 4; dir++) {
            if (!passable(m, &a, a.at, dir)) continue;
            tmz_state b = a;
            enter(m, &b, dir);
            if (!m->fuel) b.fuel = 0;
            long u = code(&d, &b);
            if (par[u] >= 0) continue;
            par[u] = (int32_t)v;
            q[tail++] = (int32_t)u;
        }
    }
    w->states = tail;
    if (goal >= 0) {
        int n = 0;
        for (long v = goal; v != root; v = par[v]) n++;
        w->steps = n;
        int k = n < TMZ_PATH ? n : TMZ_PATH;
        long v = goal;
        for (int i = n - 1; i >= 0; i--, v = par[v]) {
            tmz_state a;
            decode(&d, v, &a);
            if (i < k) w->path[i] = a.at;
        }
        w->n = k;
    }
    free(par);
    free(q);
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int way_of_char(char ch)
{
    const char *u = "urdl", *U = "URDL";
    for (int i = 0; i < 4; i++) {
        if (ch == u[i]) return i;
        if (ch == U[i]) return 4 + i;
    }
    return -1;
}

static int dir_between(const tmz_map *m, int a, int b)
{
    for (int d = 0; d < 4; d++)
        if (tmz_next(m, a, d) == b) return d;
    return -1;
}

int tmz_script(tmz_game *g, const char *moves)
{
    for (const char *p = moves; *p; p++) {
        if (*p == ' ' || *p == ',') continue;
        if (*p == 'h') { g->hints++; continue; }
        if (*p == '=') {
            int k = 0;
            while (p[1] >= '0' && p[1] <= '9') k = k * 10 + *++p - '0';
            tmz_way w;
            tmz_solve(&g->m, &g->s, &w);
            if (w.steps < 0) return 0;
            int n = k > 0 && k < w.n ? k : w.n;
            for (int i = 0; i < n; i++) {
                int d = dir_between(&g->m, g->s.at, w.path[i]);
                if (d < 0 || !tmz_step(g, d)) return 0;
            }
            continue;
        }
        int w = way_of_char(*p);
        if (w < 0) return 0;
        if (!(w >= 4 ? tmz_run(g, w - 4, NULL, NULL) : tmz_step(g, w))) return 0;
    }
    return 1;
}

/* ---- maps as text ------------------------------------------------------------------------------------------------ */
static int bits_of(const tmz_map *m, int c)
{
    if (c >= m->w * m->h || !m->in[c]) return 0;
    return 1 | (m->e[0][c] != TMZ_WALL) << 1 | (m->e[1][c] != TMZ_WALL) << 2;
}

void tmz_text(const tmz_map *m, char *out)
{
    int n = sprintf(out, "%d,%d,%d,%d,%d;", m->w, m->h, m->radius, m->fuel, m->fade), N = m->w * m->h;
    for (int c = 0; c < N; c += 2) out[n++] = tmz_alpha[bits_of(m, c) | bits_of(m, c + 1) << 3];
    out[n++] = ';';
    n += sprintf(out + n, "S%d X%d", m->start, m->exit);
    for (int c = 0; c < N; c++) {
        static const char IT[] = " KBOP";
        int t = m->item[c];
        if (t == TMZ_KEY) n += sprintf(out + n, " K%d%d", m->ik[c], c);
        else if (t >= TMZ_BRAZIER && t <= TMZ_PLATE) n += sprintf(out + n, " %c%d", IT[t], c);
    }
    for (int s = 0; s < 2; s++)
        for (int c = 0; c < N; c++) {
            int t = m->e[s][c];
            if (t == TMZ_DOOR || t == TMZ_ONEWAY || t == TMZ_GATE)
                n += sprintf(out + n, " %c%d%c%d", t == TMZ_DOOR ? 'D' : t == TMZ_ONEWAY ? 'W' : 'G', m->k[s][c],
                             s ? 'd' : 'r', c);
        }
    out[n] = 0;
}

int tmz_parse(tmz_map *m, const char *text)
{
    memset(m, 0, sizeof *m);
    if (sscanf(text, "%d,%d,%d,%d,%d;", &m->w, &m->h, &m->radius, &m->fuel, &m->fade) != 5) return 0;
    if (m->w < 2 || m->h < 2 || m->w > TMZ_MAXW || m->h > TMZ_MAXH) return 0;
    const char *p = strchr(text, ';');
    if (!p) return 0;
    p++;
    int N = m->w * m->h;
    for (int c = 0; c < N; c += 2, p++) {
        const char *q = *p ? strchr(tmz_alpha, *p) : NULL;
        if (!q) return 0;
        int v = (int)(q - tmz_alpha);
        for (int j = 0; j < 2 && c + j < N; j++) {
            int b = v >> (3 * j) & 7;
            m->in[c + j] = b & 1;
            m->e[0][c + j] = b >> 1 & 1 ? TMZ_OPEN : TMZ_WALL;
            m->e[1][c + j] = b >> 2 & 1 ? TMZ_OPEN : TMZ_WALL;
        }
    }
    if (*p++ != ';') return 0;
    m->start = m->exit = -1;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char t = *p++;
        int a = 0, side = 0, c;
        if (t == 'K' || t == 'D' || t == 'W' || t == 'G') a = *p++ - '0';
        if (t == 'D' || t == 'W' || t == 'G') side = *p++ == 'd';
        c = (int)strtol(p, (char **)&p, 10);
        if (c < 0 || c >= N || a < 0 || a > 3) return 0;
        switch (t) {
        case 'S': m->start = c; break;
        case 'X': m->exit = c; m->item[c] = TMZ_EXIT; break;
        case 'K': m->item[c] = TMZ_KEY; m->ik[c] = (uint8_t)a; if (a + 1 > m->ncol) m->ncol = a + 1; break;
        case 'B': m->item[c] = TMZ_BRAZIER; m->ik[c] = (uint8_t)m->nbraz++; break;
        case 'O': m->item[c] = TMZ_OIL; m->ik[c] = (uint8_t)m->nflask++; break;
        case 'P': m->item[c] = TMZ_PLATE; break;
        case 'D': m->e[side][c] = TMZ_DOOR; m->k[side][c] = (uint8_t)a; if (a + 1 > m->ncol) m->ncol = a + 1; break;
        case 'W': m->e[side][c] = TMZ_ONEWAY; m->k[side][c] = (uint8_t)a; break;
        case 'G': m->e[side][c] = TMZ_GATE; m->k[side][c] = (uint8_t)(a & 1); break;
        default: return 0;
        }
    }
    if (m->start < 0 || m->exit < 0 || !m->in[m->start] || !m->in[m->exit] || m->nbraz > TMZ_BRAZIERS ||
        m->nflask > TMZ_FLASKS || m->ncol > TMZ_COLOURS)
        return 0;
    return 1;
}

/* ---- the sensible explorer ---------------------------------------------------------------------------------------- */
/* the known ways from s, by Dial's buckets (a step 1, through a one-way door ONEWAY_COST more: no coming back; a one-way
 * door found a trap never again): dist (-1 not reached), from, and the cells in their order (*n) */
static void known_ways(const tmz_map *m, const tmz_state *s, const uint8_t *known, const uint8_t *bad, int *dist,
                       int *from, int *order, int *n)
{
    static int bucket[16][TMZ_CELLS], nb[16];
    static uint8_t done[TMZ_CELLS];
    int N = m->w * m->h, tail = 0, pending = 1;
    for (int c = 0; c < N; c++) { dist[c] = -1; done[c] = 0; }
    memset(nb, 0, sizeof nb);
    dist[s->at] = 0;
    from[s->at] = -1;
    bucket[0][nb[0]++] = s->at;
    for (int cost = 0; pending > 0; cost++) {
        int b = cost & 15;
        for (int i = 0; i < nb[b]; i++) {
            int c = bucket[b][i];
            pending--;
            if (done[c] || dist[c] != cost) continue;
            done[c] = 1;
            order[tail++] = c;
            for (int d = 0; d < 4; d++) {
                int nx = tmz_next(m, c, d), side, e;
                if (nx < 0 || !known[nx] || done[nx] || !passable(m, s, c, d)) continue;
                int ow = edge_type(m, c, d, NULL) == TMZ_ONEWAY;
                if (ow && edge_of(m, c, d, &side, &e) && bad[e * 2 + side]) continue;
                int nd = cost + 1 + (ow ? ONEWAY_COST : 0);
                if (dist[nx] >= 0 && dist[nx] <= nd) continue;
                dist[nx] = nd;
                from[nx] = c;
                bucket[nd & 15][nb[nd & 15]++] = nx;
                pending++;
            }
        }
        nb[b] = 0;
    }
    for (int c = 0; c < N; c++)
        if (!done[c]) dist[c] = -1;
    *n = tail;
}

/* the explorer walks what it knows to the exit from the start, if the fuel lets it (the whole of what it knows: a
 * search over the known cells): the steps, -1 none */
static int known_route(const tmz_map *m, const uint8_t *known)
{
    static tmz_map k;
    k = *m;
    for (int c = 0; c < m->w * m->h; c++)
        if (!known[c]) k.in[c] = 0;
    if (!k.in[m->exit]) return -1;
    tmz_state s = {0};
    s.at = (int16_t)m->start;
    s.fuel = (int16_t)m->fuel;
    tmz_way w;
    tmz_solve(&k, &s, &w);
    return w.steps;
}

int tmz_explore(const tmz_map *m, double lambda, int cap)
{
    static uint8_t known[TMZ_CELLS], bad[2 * TMZ_CELLS];
    static int dist[TMZ_CELLS], from[TMZ_CELLS], q[TMZ_CELLS], ow_step[TMZ_PATH * 4];
    static tmz_state hist[TMZ_PATH * 4];
    int N = m->w * m->h, ex = m->exit % m->w, ey = m->exit / m->w, presses = 0, rewinds = 0, nh = 0, effort = 0;
    tmz_state s = {0};
    s.at = (int16_t)m->start;
    s.fuel = (int16_t)m->fuel;
    memset(known, 0, (size_t)N);
    memset(bad, 0, (size_t)N * 2);
    look(m, &s, s.at % m->w + 0.5f, s.at / m->w + 0.5f, tmz_light(m, &s), known);
    if (cap > TMZ_PATH * 4 - 1) cap = TMZ_PATH * 4 - 1;
    while (effort <= cap) {
        if (s.at == m->exit) return effort;
        int tail = 0, target = -1, fdir = -1;
        if (!m->fuel || s.fuel > 0) {
            known_ways(m, &s, known, bad, dist, from, q, &tail);
            double best = 1e18;
            if (dist[m->exit] >= 0) target = m->exit;
            else
                for (int i = 0; i < tail; i++) {
                    int c = q[i];
                    if (c != s.at && ((m->item[c] == TMZ_KEY && !(s.keys >> m->ik[c] & 1)) ||
                                      (m->item[c] == TMZ_OIL && !(s.flasks >> m->ik[c] & 1) && s.fuel < m->fuel))) {
                        double v = dist[c] + lambda * (abs(c % m->w - ex) + abs(c / m->w - ey)) - 3;
                        if (v < best) { best = v; target = c; fdir = -1; }
                    }
                    for (int d = 0; d < 4; d++) {       /* an unknown cell beyond a known one */
                        int n = tmz_next(m, c, d), side, e;
                        if (n < 0 || known[n] || !passable(m, &s, c, d)) continue;
                        int ow = edge_type(m, c, d, NULL) == TMZ_ONEWAY;
                        if (ow && edge_of(m, c, d, &side, &e) && bad[e * 2 + side]) continue;
                        double v = dist[c] + 1 + (ow ? ONEWAY_COST : 0) +
                                   lambda * (abs(n % m->w - ex) + abs(n / m->w - ey));
                        if (v < best) { best = v; target = c; fdir = d; }
                    }
                }
            if (target < 0 && presses < 16) {   /* nothing left to see: a plate */
                for (int i = 1; i < tail && target < 0; i++)
                    if (m->item[q[i]] == TMZ_PLATE) target = q[i];
                if (target < 0 && m->item[s.at] == TMZ_PLATE && tail > 1) target = q[1];
            }
        }
        if (target < 0) {                       /* stuck (trapped, the torch out): back, as a player undoes */
            if (++rewinds > 8) return -1;
            int k = -1;
            for (int i = nh - 1; i >= 0 && k < 0; i--)
                if (ow_step[i] >= 0) k = i;
            if (k >= 0 && !(m->fuel && s.fuel <= 0)) {   /* to before the last one-way door, never through it again */
                bad[ow_step[k]] = 1;
                s = hist[k];
                nh = k;
            } else {                            /* to the start: the way it knows, if it knows one */
                s = hist[0];
                if (!nh) s = (tmz_state){.at = (int16_t)m->start, .fuel = (int16_t)m->fuel};
                nh = 0;
                presses = 0;
                int r = known_route(m, known);
                if (r > 0) return effort + r;
            }
            continue;
        }
        int c = target, d, side, e;
        if (c == s.at) d = fdir;
        else {
            while (from[c] != s.at) c = from[c];
            d = dir_between(m, s.at, c);
        }
        if (d < 0 || !tmz_can(m, &s, d)) return -1;
        hist[nh] = s;
        ow_step[nh] = edge_type(m, s.at, d, NULL) == TMZ_ONEWAY && edge_of(m, s.at, d, &side, &e) ? e * 2 + side : -1;
        nh++;
        effort++;
        enter(m, &s, d);
        look(m, &s, s.at % m->w + 0.5f, s.at / m->w + 0.5f, tmz_light(m, &s), known);
        presses += m->item[s.at] == TMZ_PLATE;
        if (m->item[s.at] == TMZ_BRAZIER)       /* a brazier passed lights its part */
            look(m, &s, s.at % m->w + 0.5f, s.at / m->w + 0.5f, BRAZIER_R, known);
    }
    return -1;
}

void tmz_measure(const tmz_map *m, tmz_metric *mt)
{
    memset(mt, 0, sizeof *mt);
    tmz_state s = {0};
    s.at = (int16_t)m->start;
    s.fuel = (int16_t)m->fuel;
    tmz_way w;
    tmz_solve(m, &s, &w);
    mt->best = w.steps;
    mt->states = w.states;
    for (int c = 0; c < m->w * m->h; c++) {
        int o = 0;
        for (int d = 0; d < 4; d++) o += tmz_opening(m, c, d);
        mt->deadends += m->in[c] && o == 1;
    }
    if (w.steps <= 0) return;
    double sum = 0;
    static const double L[2] = {1.0, 0.35};
    for (int i = 0; i < 2; i++) {
        mt->naive[i] = tmz_explore(m, L[i], 12 * w.steps + 300);
        sum += mt->naive[i] < 0 ? fmax(3.0 * w.steps, w.steps + 60.0) : mt->naive[i];
    }
    mt->ratio = sum / 2 / w.steps;
    mt->score = sum / 2;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int in_shape(int shape, int x, int y, int w, int h)
{
    float u = (x + 0.5f) / w * 2 - 1, v = (y + 0.5f) / h * 2 - 1, r2 = u * u + v * v;
    switch (shape) {
    case 1: return r2 <= 1.08f;
    case 2: return fabsf(u) + fabsf(v) <= 1.12f;
    case 3: return fabsf(u) < 0.42f || fabsf(v) < 0.42f;
    case 4: return r2 <= 1.08f && r2 >= 0.14f;
    case 5: return fabsf(u) >= 0.28f || fabsf(v) <= 0.3f;
    case 6: return fabsf(u) + fabsf(v) <= 1.55f;
    default: return 1;
    }
}

/* the cells reachable from `from` (every edge but walls passable, or as a state lets: s not NULL), their distance */
static int reach(const tmz_map *m, const tmz_state *s, int from, int *dist)
{
    static int q[TMZ_CELLS];
    int N = m->w * m->h, head = 0, tail = 0;
    for (int c = 0; c < N; c++) dist[c] = -1;
    dist[from] = 0;
    q[tail++] = from;
    while (head < tail) {
        int c = q[head++];
        for (int d = 0; d < 4; d++) {
            int n = tmz_next(m, c, d);
            if (n < 0 || dist[n] >= 0 || !(s ? passable(m, s, c, d) : tmz_opening(m, c, d))) continue;
            dist[n] = dist[c] + 1;
            q[tail++] = n;
        }
    }
    return tail;
}

static int openings(const tmz_map *m, int c)
{
    int o = 0;
    for (int d = 0; d < 4; d++) o += tmz_opening(m, c, d);
    return o;
}

static tmz_state start_state(const tmz_map *m)
{
    tmz_state s = {0};
    s.at = (int16_t)m->start;
    s.fuel = (int16_t)m->fuel;
    return s;
}

static int best_of(const tmz_map *m)
{
    tmz_state s = start_state(m);
    tmz_way w;
    tmz_solve(m, &s, &w);
    return w.steps;
}

/* a shortest way from the start to the exit, every door and gate open: its cells (path[0] the start) */
static int plain_path(const tmz_map *m, int *path)
{
    static int dist[TMZ_CELLS];
    reach(m, NULL, m->exit, dist);
    if (dist[m->start] < 0) return 0;
    int n = 0, c = m->start;
    path[n++] = c;
    while (c != m->exit) {
        int nx = -1;
        for (int d = 0; d < 4 && nx < 0; d++) {
            int k = tmz_next(m, c, d);
            if (k >= 0 && tmz_opening(m, c, d) && dist[k] == dist[c] - 1) nx = k;
        }
        if (nx < 0) return 0;
        path[n++] = c = nx;
    }
    return n;
}

/* the edge between a and b is a bridge: shut, the start no longer reaches the exit */
static int bridge(tmz_map *m, int a, int dir)
{
    static int dist[TMZ_CELLS];
    int s, e;
    if (!edge_of(m, a, dir, &s, &e)) return 0;
    uint8_t t = m->e[s][e];
    m->e[s][e] = TMZ_WALL;
    reach(m, NULL, m->start, dist);
    m->e[s][e] = t;
    return dist[m->exit] < 0;
}

/* a cell free for a thing: in the maze, nothing there, not the start */
static int free_cell(const tmz_map *m, int c) { return m->in[c] && m->item[c] == TMZ_NONE && c != m->start; }

/* the cells the explorer reaches from the start with these keys and gates (doors of the other colours shut) */
static int region(const tmz_map *m, int keys, int gates, int *dist)
{
    tmz_state s = start_state(m);
    s.keys = (uint8_t)keys;
    s.gates = (uint8_t)gates;
    return reach(m, &s, m->start, dist);
}

/* a cell for a thing: of those reached (dist >= 0), far from `from` (a cell, its own distances), a dead end
 * preferred, picked among the farthest few */
static int far_cell(const tmz_map *m, const int *dist, int from, uint32_t *r)
{
    static int df[TMZ_CELLS];
    static int cand[TMZ_CELLS];
    static float val[TMZ_CELLS];
    reach(m, NULL, from, df);
    int n = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (dist[c] < 0 || df[c] < 0 || !free_cell(m, c) || c == from) continue;
        cand[n] = c;
        val[n] = (float)df[c] + (openings(m, c) == 1 ? 3 : 0) + pr_below(r, 4);
        n++;
    }
    if (!n) return -1;
    for (int i = 0; i < n; i++)                 /* the top quarter, by a small sort */
        for (int j = i + 1; j < n; j++)
            if (val[j] > val[i]) {
                float tv = val[i]; val[i] = val[j]; val[j] = tv;
                int tc = cand[i]; cand[i] = cand[j]; cand[j] = tc;
            }
    int top = n / 4 + 1;
    return cand[pr_below(r, top)];
}

/* a bridge on the plain way between fractions lo..hi of it, not yet special: its cell and way (0: none) */
static int path_bridge(tmz_map *m, float lo, float hi, uint32_t *r, int *cell, int *dir)
{
    static int path[TMZ_CELLS];
    int n = plain_path(m, path), cand[TMZ_CELLS], nc = 0;
    if (n < 3) return 0;
    for (int i = (int)(lo * (n - 1)); i < (int)(hi * (n - 1)) && i + 1 < n; i++) {
        int d = dir_between(m, path[i], path[i + 1]);
        if (d >= 0 && edge_type(m, path[i], d, NULL) == TMZ_OPEN && path[i] != m->start && bridge(m, path[i], d))
            cand[nc++] = i;
    }
    if (!nc) return 0;
    int i = cand[pr_below(r, nc)];
    *cell = path[i];
    *dir = dir_between(m, path[i], path[i + 1]);
    return 1;
}

int tmz_make(tmz_map *m, const tmz_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    memset(m, 0, sizeof *m);
    m->w = sp->w;
    m->h = sp->h;
    m->radius = sp->radius;
    m->fade = sp->fade;
    int w = m->w, h = m->h, N = w * h;
    static int room[TMZ_CELLS], dist[TMZ_CELLS], q[TMZ_CELLS], act[TMZ_CELLS];
    static uint8_t vis[TMZ_CELLS];
    /* the shape, kept whole: its biggest part */
    for (int c = 0; c < N; c++) m->in[c] = (uint8_t)in_shape(sp->shape, c % w, c / w, w, h);
    {
        static uint8_t done[TMZ_CELLS];
        memset(done, 0, sizeof done);
        int bestn = 0, bestc = -1;
        for (int c = 0; c < N; c++) {
            if (!m->in[c] || done[c]) continue;
            int head = 0, tail = 0;
            q[tail++] = c;
            done[c] = 1;
            while (head < tail) {
                int a = q[head++];
                for (int d = 0; d < 4; d++) {
                    int b = tmz_next(m, a, d);
                    if (b >= 0 && !done[b]) { done[b] = 1; q[tail++] = b; }
                }
            }
            if (tail > bestn) { bestn = tail; bestc = c; }
        }
        if (bestc < 0) return 0;
        memset(done, 0, sizeof done);
        int head = 0, tail = 0;
        q[tail++] = bestc;
        done[bestc] = 1;
        while (head < tail) {
            int a = q[head++];
            for (int d = 0; d < 4; d++) {
                int b = tmz_next(m, a, d);
                if (b >= 0 && !done[b]) { done[b] = 1; q[tail++] = b; }
            }
        }
        for (int c = 0; c < N; c++) m->in[c] = done[c];
    }
    /* rooms */
    memset(room, 0, sizeof room);
    for (int k = 0, made = 0; k < 60 && made < sp->rooms; k++) {
        int rw = 2 + pr_below(&r, 2), rh = 2 + pr_below(&r, 2), x0 = pr_below(&r, w - rw + 1),
            y0 = pr_below(&r, h - rh + 1), ok = 1;
        for (int y = y0 - 1; y <= y0 + rh && ok; y++)
            for (int x = x0 - 1; x <= x0 + rw && ok; x++) {
                int inside = x >= x0 && x < x0 + rw && y >= y0 && y < y0 + rh;
                if (x < 0 || y < 0 || x >= w || y >= h) { ok = !inside; continue; }
                if (inside && !m->in[x + y * w]) ok = 0;
                if (room[x + y * w]) ok = 0;
            }
        if (!ok) continue;
        made++;
        for (int y = y0; y < y0 + rh; y++)
            for (int x = x0; x < x0 + rw; x++) {
                room[x + y * w] = made;
                if (x + 1 < x0 + rw) set_edge(m, x + y * w, TMZ_RIGHT, TMZ_OPEN, 0);
                if (y + 1 < y0 + rh) set_edge(m, x + y * w, TMZ_DOWN, TMZ_OPEN, 0);
            }
    }
    /* the maze grown: the newest cell or one at random, a room taken whole */
    memset(vis, 0, sizeof vis);
    int na = 0, c0;
    do c0 = pr_below(&r, N); while (!m->in[c0]);
    {
        int add[TMZ_CELLS], nadd = 0;
        add[nadd++] = c0;
        if (room[c0])
            for (int c = 0; c < N; c++)
                if (room[c] == room[c0] && c != c0) add[nadd++] = c;
        for (int i = 0; i < nadd; i++) { vis[add[i]] = 1; act[na++] = add[i]; }
    }
    while (na > 0) {
        int i = pr_below(&r, 100) < sp->newest ? na - 1 : pr_below(&r, na), c = act[i], ds[4], nd = 0;
        for (int d = 0; d < 4; d++) {
            int n = tmz_next(m, c, d);
            if (n >= 0 && !vis[n]) ds[nd++] = d;
        }
        if (!nd) { act[i] = act[--na]; continue; }
        int d = ds[pr_below(&r, nd)], n = tmz_next(m, c, d);
        set_edge(m, c, d, TMZ_OPEN, 0);
        vis[n] = 1;
        act[na++] = n;
        if (room[n])
            for (int k = 0; k < N; k++)
                if (room[k] == room[n] && !vis[k]) { vis[k] = 1; act[na++] = k; }
    }
    /* loops: dead ends opened */
    {
        int de[TMZ_CELLS], nde = 0;
        for (int c = 0; c < N; c++)
            if (m->in[c] && openings(m, c) == 1) de[nde++] = c;
        pr_shuffle(&r, de, nde);
        for (int i = 0; i < nde; i++) {
            int c = de[i];
            if (openings(m, c) != 1 || pr_below(&r, 100) >= sp->braid) continue;
            int ds[4], nd = 0, pref = -1;
            for (int d = 0; d < 4; d++) {
                int n = tmz_next(m, c, d);
                if (n < 0 || tmz_opening(m, c, d)) continue;
                ds[nd++] = d;
                if (openings(m, n) == 1) pref = d;
            }
            if (nd) set_edge(m, c, pref >= 0 ? pref : ds[pr_below(&r, nd)], TMZ_OPEN, 0);
        }
    }
    /* the start and the exit: of a few tried, where the sensible explorer walks the most for the way */
    {
        int cells[TMZ_CELLS], nc = 0;
        for (int c = 0; c < N; c++)
            if (m->in[c]) cells[nc++] = c;
        double bestv = -1;
        int bs = -1, bx = -1;
        for (int t = 0; t < (sp->tries > 0 ? sp->tries : 1); t++) {
            int s = cells[pr_below(&r, nc)];
            if (room[s] && pr_below(&r, 2)) s = cells[pr_below(&r, nc)];
            reach(m, NULL, s, dist);
            int far = 0;
            for (int c = 0; c < N; c++)
                if (dist[c] > far) far = dist[c];
            int cand[TMZ_CELLS], nx = 0;
            for (int c = 0; c < N; c++)
                if (dist[c] >= (far * 7) / 10 && !room[c]) cand[nx++] = c;
            if (!nx) continue;
            int x = cand[pr_below(&r, nx)];
            m->start = s;
            m->exit = x;
            int e = sp->tries > 1 ? tmz_explore(m, 1.0, 8 * far + 200) : dist[x];
            double ee = e < 0 ? 3.0 * dist[x] : e, v = ee * sqrt(ee / (dist[x] > 0 ? dist[x] : 1));
            if (v > bestv) { bestv = v; bs = s; bx = x; }
        }
        if (bs < 0 || bs == bx) return 0;
        m->start = bs;
        m->exit = bx;
        m->item[bx] = TMZ_EXIT;
    }
    /* doors and their keys: each door on the way, its key far from it, before it */
    for (int k = 0; k < sp->colours && k < TMZ_COLOURS; k++) {
        int c, d;
        if (!path_bridge(m, 0.3f, 0.9f, &r, &c, &d)) return 0;
        set_edge(m, c, d, TMZ_DOOR, k);
        m->ncol = k + 1;
        region(m, (1 << k) - 1, 0, dist);
        int kc = far_cell(m, dist, c, &r);
        if (kc < 0) return 0;
        m->item[kc] = TMZ_KEY;
        m->ik[kc] = (uint8_t)k;
    }
    /* gates: shut ones on the way, open ones anywhere; plates far from them */
    for (int g = 0; g < sp->gates; g++) {
        int c, d;
        if (!path_bridge(m, 0.25f, 0.95f, &r, &c, &d)) return 0;
        set_edge(m, c, d, TMZ_GATE, 0);
    }
    for (int g = 0; g < sp->opens; g++) {
        for (int t = 0; t < 40; t++) {
            int c = pr_below(&r, N), d = pr_below(&r, 4);
            if (!m->in[c] || edge_type(m, c, d, NULL) != TMZ_OPEN || c == m->start) continue;
            set_edge(m, c, d, TMZ_GATE, 1);
            break;
        }
    }
    for (int p = 0; p < sp->plates; p++) {
        region(m, (1 << m->ncol) - 1, 0, dist);
        int pc = far_cell(m, dist, m->start, &r);
        if (pc < 0) return 0;
        m->item[pc] = TMZ_PLATE;
    }
    /* one-way doors */
    for (int o = 0; o < sp->oneways; o++) {
        for (int t = 0; t < 60; t++) {
            int c = pr_below(&r, N), d = pr_below(&r, 4), n = tmz_next(m, c, d);
            if (n < 0 || !m->in[c] || edge_type(m, c, d, NULL) != TMZ_OPEN || c == m->start || n == m->start) continue;
            if (room[c] && room[c] == room[n]) continue;
            set_edge(m, c, d, TMZ_ONEWAY, pr_below(&r, 2) ? d : (d + 2) & 3);
            break;
        }
    }
    /* braziers: on junctions and in rooms, apart */
    for (int b = 0; b < sp->braziers && m->nbraz < TMZ_BRAZIERS; b++) {
        int bc = -1;
        float bv = -1;
        for (int t = 0; t < 40; t++) {
            int c = pr_below(&r, N);
            if (!free_cell(m, c)) continue;
            float v = (float)openings(m, c) + (room[c] ? 1.5f : 0) + pr_below(&r, 3) * 0.3f;
            for (int k = 0; k < N; k++)
                if (m->item[k] == TMZ_BRAZIER && abs(k % w - c % w) + abs(k / w - c / w) < 5) v -= 10;
            if (v > bv) { bv = v; bc = c; }
        }
        if (bc >= 0 && bv > 0) {
            m->item[bc] = TMZ_BRAZIER;
            m->ik[bc] = (uint8_t)m->nbraz++;
        }
    }
    /* flasks, then the fuel: the least that does, and the slack */
    if (sp->slack >= 0) {
        for (int f = 0; f < sp->flasks && f < TMZ_FLASKS; f++) {
            region(m, (1 << m->ncol) - 1, 0, dist);
            int fc = far_cell(m, dist, m->start, &r);
            if (fc < 0) break;
            m->item[fc] = TMZ_OIL;
            m->ik[fc] = (uint8_t)m->nflask++;
        }
        int plain = best_of(m);
        if (plain <= 0) return 0;
        int lo = 1, hi = plain;
        m->fuel = hi;
        if (best_of(m) < 0) return 0;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            m->fuel = mid;
            if (best_of(m) >= 0) hi = mid;
            else lo = mid + 1;
        }
        m->fuel = lo + sp->slack;
        if (m->fuel < 2) m->fuel = 2;
    }
    return best_of(m) > 0;
}

void tmz_variant(int v, tmz_spec *sp)
{
    static const tmz_spec V[TMZ_VARIANTS] = {
        {9, 9, 0, 55, 15, 1, 1, 0, 0, 0, 0, 0, -1, 1, 28, 0, 5},
        {13, 13, 0, 60, 20, 2, 2, 1, 1, 0, 0, 0, -1, 2, 27, 0, 5},
        {17, 17, 0, 65, 22, 3, 2, 1, 1, 1, 1, 0, -1, 3, 26, 0, 4},
    };
    *sp = V[v < 0 || v >= TMZ_VARIANTS ? 0 : v];
}

void tmz_random(tmz_game *g, int variant, uint32_t seed)
{
    tmz_spec sp;
    tmz_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    tmz_map best, m;
    double bv = -1;
    int bbest = 0;
    for (int k = 0; k < 8 && (k < 4 || bv < 0); k++) {
        static const int SHAPES[] = {0, 0, 1, 2, 3, 4, 5, 6};
        sp.shape = SHAPES[pr_below(&r, 8)];
        if (!tmz_make(&m, &sp, pr_next(&r))) continue;
        tmz_metric mt;
        tmz_measure(&m, &mt);
        if (mt.best <= 0) continue;
        double v = mt.ratio + 0.01 * mt.best;
        if (v > bv) { bv = v; best = m; bbest = mt.best; }
    }
    for (int k = 0; bv < 0 && k < 200; k++) {  /* a plain maze, should all else fail */
        tmz_spec plain = {9, 9, 0, 50, 10, 0, 0, 0, 0, 0, 0, 0, -1, 0, 28, 0, 1};
        if (tmz_make(&m, &plain, pr_next(&r))) { best = m; bbest = best_of(&m); bv = 0; }
    }
    g->m = best;
    tmz_begin(g);
    g->best = bbest;
    g->allow = 200;
    g->par = tmz_par_of(bbest, g->allow);
}

#ifndef TMZ_GEN                                 /* not in the levels' tool (tools/torchmaze_gen.c): it makes them */
#include "games/torchmaze_levels.h"

int tmz_level(tmz_game *g, int level)
{
    if (level < 0 || level >= TMZ_LEVELS || !tmz_parse(&g->m, tmz_levels[level])) return 0;
    tmz_begin(g);
    g->best = tmz_level_best[level];
    g->allow = tmz_allow[level / (TMZ_LEVELS / TMZ_CHAPTERS)] * 10;
    g->par = tmz_par_of(g->best, g->allow);
    return 1;
}
#endif
