/*
 * Gears for one player: the rules (gears.h). No SDL: the tests and the levels' tool (tools/gears_gen.c) use them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/gears.h"
#include "games/puzrand.h"

/* ---- the board ---------------------------------------------------------------------------------------------------- */
int gea_parse(gea_map *m, const char *t)
{
    memset(m, 0, sizeof *m);
    if (!t || strlen(t) < 3) return 0;
    m->w = t[0] - 'a';
    m->h = t[1] - 'a';
    m->sizes = t[2] - '0';
    if (m->w < 2 || m->h < 2 || m->w > GEA_MAXW || m->h > GEA_MAXH || m->sizes < 2 || m->sizes > 14) return 0;
    int motors = 0;
    for (const char *s = t + 3; *s; s += 4) {
        if (strlen(s) < 4 || m->n >= GEA_PEGS) return 0;
        gea_peg *p = &m->p[m->n++];
        p->x = (uint8_t)(s[1] - 'a');
        p->y = (uint8_t)(s[2] - 'a');
        p->size = (uint8_t)(s[3] - '0');
        p->dir = 0;
        switch (s[0]) {
        case 'M': p->kind = GEA_MOTOR; p->dir = 1; motors++; break;
        case 'T': case 't': p->kind = GEA_TARGET; p->dir = s[0] == 'T' ? 1 : -1; break;
        case 'S': case 's': p->kind = GEA_SHAFT; p->dir = s[0] == 'S' ? 1 : -1; break;
        case 'P': p->kind = GEA_PEG; p->size = 0; break;
        default: return 0;
        }
        if (p->x > m->w || p->y > m->h) return 0;
        if (p->kind != GEA_PEG && (p->size < 1 || p->size > GEA_SIZES)) return 0;
    }
    return motors == 1;
}

void gea_text(const gea_map *m, char *out)
{
    int n = 0;
    out[n++] = (char)('a' + m->w);
    out[n++] = (char)('a' + m->h);
    out[n++] = (char)('0' + m->sizes);
    for (int i = 0; i < m->n; i++) {
        const gea_peg *p = &m->p[i];
        char k = p->kind == GEA_MOTOR ? 'M' : p->kind == GEA_TARGET ? (p->dir > 0 ? 'T' : 't')
               : p->kind == GEA_SHAFT ? (p->dir > 0 ? 'S' : 's') : 'P';
        out[n++] = k;
        out[n++] = (char)('a' + p->x);
        out[n++] = (char)('a' + p->y);
        out[n++] = (char)('0' + p->size);
    }
    out[n] = 0;
}

int gea_rel(const gea_map *m, int i, int a, int j, int b)
{
    int dx = m->p[i].x - m->p[j].x, dy = m->p[i].y - m->p[j].y, d2 = dx * dx + dy * dy, s = a + b;
    if (d2 == s * s) return GEA_MESH;
    return 25 * d2 < (5 * s + 2) * (5 * s + 2) ? GEA_CLASH : GEA_FREE;   /* closer than s + 0.4: the teeth hit */
}

static int inside(const gea_map *m, int i, int s)
{
    const gea_peg *p = &m->p[i];
    return p->x >= s && p->x + s <= m->w && p->y >= s && p->y + s <= m->h;
}

int gea_fixed(const gea_map *m, int peg) { return m->p[peg].kind == GEA_MOTOR || m->p[peg].kind == GEA_TARGET; }

void gea_fresh(const gea_map *m, gea_state *s)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < m->n; i++)
        if (gea_fixed(m, i)) s->g[i] = m->p[i].size;
}

void gea_begin(gea_game *g)
{
    gea_fresh(&g->m, &g->s);
    g->nh = 0;
}

int gea_fits(const gea_map *m, const gea_state *s, int peg, int size)
{
    if (peg < 0 || peg >= m->n || size < 1 || size > GEA_SIZES) return GEA_OUTSIDE;
    if (gea_fixed(m, peg) || s->g[peg]) return GEA_TAKEN;
    if (!(m->sizes >> size & 1) || !inside(m, peg, size)) return GEA_OUTSIDE;
    for (int j = 0; j < m->n; j++)
        if (j != peg && s->g[j] && gea_rel(m, peg, size, j, s->g[j]) == GEA_CLASH) return GEA_COLLIDES;
    return GEA_OK;
}

static int motor_of(const gea_map *m)
{
    for (int i = 0; i < m->n; i++)
        if (m->p[i].kind == GEA_MOTOR) return i;
    return 0;
}

void gea_eval(const gea_map *m, const gea_state *s, gea_eval_t *e)
{
    memset(e, 0, sizeof *e);
    int col[GEA_PEGS], q[GEA_PEGS], nq = 0, mo = motor_of(m);
    for (int i = 0; i < m->n; i++) e->from[i] = -1;
    e->train[mo] = 1;
    col[mo] = 0;
    q[nq++] = mo;
    for (int k = 0; k < nq; k++) {
        int u = q[k];
        for (int v = 0; v < m->n; v++) {
            if (v == u || !s->g[v] || gea_rel(m, u, s->g[u], v, s->g[v]) != GEA_MESH) continue;
            if (e->train[v]) {
                if (col[v] == col[u]) e->jam = 1;
                continue;
            }
            e->train[v] = 1;
            col[v] = !col[u];
            e->from[v] = (int8_t)u;
            q[nq++] = v;
        }
    }
    int targets = 0, right = 0;
    for (int i = 0; i < m->n; i++) {
        const gea_peg *p = &m->p[i];
        if (e->train[i] && !e->jam) e->dir[i] = (int8_t)(col[i] ? -1 : 1);
        if (!gea_fixed(m, i) && s->g[i]) {
            e->put++;
            if (!e->train[i]) e->idle++;
        }
        if (p->kind == GEA_TARGET || p->kind == GEA_SHAFT) {
            targets++;
            e->ok[i] = e->dir[i] == p->dir && (p->kind == GEA_TARGET || s->g[i] == p->size);
            right += e->ok[i];
        }
    }
    e->solved = targets && right == targets && !e->jam && !e->idle;
}

int gea_solved(const gea_map *m, const gea_state *s)
{
    gea_eval_t e;
    gea_eval(m, s, &e);
    return e.solved;
}

int gea_count(const gea_map *m, const gea_state *s)
{
    int n = 0;
    for (int i = 0; i < m->n; i++) n += !gea_fixed(m, i) && s->g[i];
    return n;
}

static void push(gea_game *g)
{
    if (g->nh == GEA_HIST) {                    /* a long game forgets its first steps */
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (GEA_HIST - 1));
        g->nh--;
    }
    g->hist[g->nh++] = g->s;
}

int gea_put(gea_game *g, int peg, int size)
{
    if (peg < 0 || peg >= g->m.n || gea_fixed(&g->m, peg)) return 0;
    if (!size) {
        if (!g->s.g[peg]) return 0;
        push(g);
        g->s.g[peg] = 0;
        return 1;
    }
    if (gea_fits(&g->m, &g->s, peg, size) != GEA_OK) return 0;
    push(g);
    g->s.g[peg] = (uint8_t)size;
    return 1;
}

int gea_shift(gea_game *g, int from, int to)
{
    if (from < 0 || from >= g->m.n || gea_fixed(&g->m, from) || !g->s.g[from] || from == to) return 0;
    gea_state t = g->s;
    int size = t.g[from];
    t.g[from] = 0;
    if (gea_fits(&g->m, &t, to, size) != GEA_OK) return 0;
    push(g);
    g->s = t;
    g->s.g[to] = (uint8_t)size;
    return 1;
}

int gea_undo(gea_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    return 1;
}

/* ---- the search ----------------------------------------------------------------------------------------------------
 * Every train built from the motor: a gear (a peg and a size) is chosen only where it meshes with the train built so
 * far and hits nothing; the first such choice in the pegs' order is either put or ruled out for good, so each set of
 * placements whose gears are all driven is met exactly once. A jammed train is cut at once. */
enum { S_FEWEST, S_NEAREST, S_WAYS };
typedef struct {
    const gea_map *m;
    int n, mode, count, cost, motor, stop, wayn, ways, waycap;
    long nodes, limit, jams;
    uint8_t rel[GEA_PEGS][GEA_PEGS][GEA_SIZES + 1][GEA_SIZES + 1];
    uint8_t allow[GEA_PEGS], cur[GEA_PEGS], dec[GEA_PEGS], excl[GEA_PEGS], want[GEA_PEGS];
    int bestkey;
    uint8_t best[GEA_PEGS];
} slv;

static void slv_init(slv *S, const gea_map *m, int mode)
{
    memset(S, 0, sizeof *S);
    S->m = m;
    S->n = m->n;
    S->mode = mode;
    S->motor = motor_of(m);
    S->bestkey = 1 << 30;
    for (int i = 0; i < m->n; i++)
        for (int j = 0; j < m->n; j++)
            for (int a = 1; a <= GEA_SIZES; a++)
                for (int b = 1; b <= GEA_SIZES; b++)
                    S->rel[i][j][a][b] = i == j ? GEA_CLASH : (uint8_t)gea_rel(m, i, a, j, b);
    for (int i = 0; i < m->n; i++) {
        const gea_peg *p = &m->p[i];
        if (gea_fixed(m, i)) { S->cur[i] = p->size; S->dec[i] = 1; continue; }
        for (int s = 1; s <= GEA_SIZES; s++)
            if (m->sizes >> s & 1 && inside(m, i, s) && (p->kind != GEA_SHAFT || s == p->size))
                S->allow[i] |= (uint8_t)(1 << s);
        if (!S->allow[i]) S->dec[i] = 1;
    }
}

/* the train from the motor: in it, its colours (the way each turns); 0 jammed */
static int slv_train(const slv *S, uint8_t *in, uint8_t *col)
{
    int q[GEA_PEGS], nq = 0;
    memset(in, 0, GEA_PEGS);
    in[S->motor] = 1;
    col[S->motor] = 0;
    q[nq++] = S->motor;
    for (int k = 0; k < nq; k++) {
        int u = q[k];
        for (int v = 0; v < S->n; v++) {
            if (!S->cur[v] || v == u || S->rel[u][v][S->cur[u]][S->cur[v]] != GEA_MESH) continue;
            if (in[v]) {
                if (col[v] == col[u]) return 0;
                continue;
            }
            in[v] = 1;
            col[v] = (uint8_t)!col[u];
            q[nq++] = v;
        }
    }
    return 1;
}

static int slv_valid(const slv *S, const uint8_t *in, const uint8_t *col)
{
    for (int i = 0; i < S->n; i++) {
        const gea_peg *p = &S->m->p[i];
        if (p->kind != GEA_TARGET && p->kind != GEA_SHAFT) continue;
        if (!in[i] || (col[i] ? -1 : 1) != p->dir) return 0;
        if (p->kind == GEA_SHAFT && S->cur[i] != p->size) return 0;
    }
    return 1;
}

static void slv_dfs(slv *S)
{
    if (S->stop) return;
    if (S->limit && ++S->nodes > S->limit) { S->stop = 2; return; }
    if (!S->limit) S->nodes++;
    if (S->mode == S_FEWEST) {
        int lb = S->count;                      /* each shaft still empty needs a gear */
        for (int i = 0; i < S->n; i++) lb += !S->cur[i] && S->m->p[i].kind == GEA_SHAFT && !S->dec[i];
        if (lb >= S->bestkey) return;
    } else if (S->mode == S_NEAREST) {
        if (S->cost * 32 + S->count >= S->bestkey) return;
    } else if (S->count > S->wayn) return;
    uint8_t in[GEA_PEGS], col[GEA_PEGS];
    if (!slv_train(S, in, col)) { S->jams++; return; }
    int P = -1, Z = 0;
    for (int p = 0; p < S->n && P < 0; p++) {
        if (S->dec[p]) continue;
        for (int s = 1; s <= GEA_SIZES; s++) {
            if (!(S->allow[p] >> s & 1) || S->excl[p] >> s & 1) continue;
            int ok = 1, mesh = 0;
            for (int q = 0; q < S->n && ok; q++) {
                if (!S->cur[q]) continue;
                int r = S->rel[p][q][s][S->cur[q]];
                if (r == GEA_CLASH) ok = 0;
                else if (r == GEA_MESH && in[q]) mesh = 1;
            }
            if (ok && mesh) { P = p; Z = s; break; }
        }
    }
    if (P < 0) {                                /* a set of placements: is it a solution? */
        if (!slv_valid(S, in, col)) return;
        if (S->mode == S_WAYS) {
            if (S->count == S->wayn && ++S->ways >= S->waycap) S->stop = 1;
            return;
        }
        int key = S->count;
        if (S->mode == S_NEAREST) {
            int cost = S->cost;
            for (int i = 0; i < S->n; i++) cost += S->want[i] && !S->cur[i];
            key = cost * 32 + S->count;
        }
        if (key < S->bestkey) {
            S->bestkey = key;
            memcpy(S->best, S->cur, sizeof S->best);
        }
        return;
    }
    /* put it */
    int add = S->want[P] == Z ? 0 : S->want[P] ? 2 : 1;
    S->cur[P] = (uint8_t)Z;
    S->dec[P] = 1;
    S->count++;
    S->cost += add;
    slv_dfs(S);
    S->cost -= add;
    S->count--;
    S->dec[P] = 0;
    S->cur[P] = 0;
    /* or rule it out */
    uint8_t ex = S->excl[P];
    S->excl[P] |= (uint8_t)(1 << Z);
    if (!(S->allow[P] & ~S->excl[P])) S->dec[P] = 1;
    slv_dfs(S);
    S->dec[P] = 0;
    S->excl[P] = ex;
}

int gea_solve(const gea_map *m, gea_state *sol, long limit, long *nodes, long *jams)
{
    slv *S = malloc(sizeof *S);
    if (!S) return -1;
    slv_init(S, m, S_FEWEST);
    S->limit = limit;
    slv_dfs(S);
    int r = S->stop == 2 || S->bestkey >= (1 << 30) ? -1 : S->bestkey;
    if (sol && r >= 0) memcpy(sol->g, S->best, sizeof sol->g);
    if (nodes) *nodes = S->nodes;
    if (jams) *jams = S->jams;
    free(S);
    return r;
}

int gea_ways(const gea_map *m, int par, int cap)
{
    slv *S = malloc(sizeof *S);
    if (!S) return 0;
    slv_init(S, m, S_WAYS);
    S->wayn = par;
    S->waycap = cap;
    S->limit = 4000000;
    slv_dfs(S);
    int w = S->ways;
    free(S);
    return w;
}

int gea_hint(const gea_map *m, const gea_state *s, int par, int *peg, int *size)
{
    gea_eval_t e;
    gea_eval(m, s, &e);
    if (e.solved) return 0;
    slv *S = malloc(sizeof *S);
    if (!S) return 0;
    slv_init(S, m, S_NEAREST);
    int put = 0;
    for (int i = 0; i < m->n; i++)
        if (!gea_fixed(m, i)) { S->want[i] = s->g[i]; put += s->g[i] > 0; }
    S->limit = 600000;
    S->bestkey = (put + (par > 0 ? par : 12) + 1) * 32;   /* no worse than taking everything off and starting again */
    slv_dfs(S);
    int found = S->bestkey < (1 << 30) && S->best[motor_of(m)], r = 0;
    if (found) {
        for (int i = 0; i < m->n && !r; i++)    /* first a gear that is not in it */
            if (!gea_fixed(m, i) && s->g[i] && S->best[i] != s->g[i]) { *peg = i; *size = 0; r = 1; }
        for (int pass = 0; pass < 2 && !r; pass++)   /* then one of it, meshing with the train (else any) */
            for (int i = 0; i < m->n && !r; i++) {
                if (gea_fixed(m, i) || s->g[i] || !S->best[i]) continue;
                int mesh = pass;
                for (int j = 0; j < m->n && !mesh; j++)
                    mesh = e.train[j] && s->g[j] && gea_rel(m, i, S->best[i], j, s->g[j]) == GEA_MESH;
                if (mesh) { *peg = i; *size = S->best[i]; r = 1; }
            }
    }
    free(S);
    if (!r)                                     /* nothing found in time: take off a gear that does not turn, or the last */
        for (int pass = 0; pass < 2 && !r; pass++)
            for (int i = m->n - 1; i >= 0 && !r; i--)
                if (!gea_fixed(m, i) && s->g[i] && (pass || !e.train[i] || e.jam)) { *peg = i; *size = 0; r = 1; }
    return r;
}

/* ---- making boards -------------------------------------------------------------------------------------------------
 * Chains of gears grown from the motor at random (each gear meshing with the one before it and with nothing else,
 * hitting nothing), each ending in a target or a shaft; then empty pegs beside them (each where some gear on it would
 * mesh with a gear on another peg), maybe mirrored left to right. The gears of the chains are taken away, their pegs
 * left empty; the targets' ways chosen among those that can be solved, the one with the most gears to put. */
static int pick_size(uint32_t *r, int mask)
{
    int c[GEA_SIZES], n = 0;
    for (int s = 1; s <= GEA_SIZES; s++)
        if (mask >> s & 1) c[n++] = s;
    return n ? c[pr_below(r, n)] : 1;
}

static int far_enough(const gea_map *m, int x, int y)   /* no peg closer than 2 */
{
    for (int i = 0; i < m->n; i++) {
        int dx = m->p[i].x - x, dy = m->p[i].y - y;
        if (dx * dx + dy * dy < 4) return 0;
    }
    return 1;
}

static int add_peg(gea_map *m, int x, int y, int kind, int size)
{
    if (m->n >= GEA_PEGS) return -1;
    gea_peg *p = &m->p[m->n];
    p->x = (uint8_t)x;
    p->y = (uint8_t)y;
    p->kind = (uint8_t)kind;
    p->size = (uint8_t)size;
    p->dir = (int8_t)(kind == GEA_MOTOR ? 1 : 0);
    return m->n++;
}

/* a point d from (x, y), at random, inside the board for size b and far enough from every peg */
static int hop(const gea_map *m, uint32_t *r, int x, int y, int d, int b, int *nx, int *ny)
{
    int vx[64], vy[64], nv = 0;
    for (int dx = -d; dx <= d; dx++)
        for (int dy = -d; dy <= d; dy++)
            if (dx * dx + dy * dy == d * d && nv < 64) { vx[nv] = dx; vy[nv] = dy; nv++; }
    for (int k = 0; k < 3 * nv; k++) {
        int i = pr_below(r, nv), X = x + vx[i], Y = y + vy[i];
        if (X < b || Y < b || X + b > m->w || Y + b > m->h || !far_enough(m, X, Y)) continue;
        *nx = X;
        *ny = Y;
        return 1;
    }
    return 0;
}

/* a gear of size b at a new peg (index n, already added) against the chain's gears (skel): meshing with `with` only,
 * hitting none */
static int fits_tree(const gea_map *m, const uint8_t *skel, int n, int b, int with)
{
    for (int j = 0; j < n; j++) {
        if (!skel[j]) continue;
        int r = gea_rel(m, n, b, j, skel[j]);
        if (r == GEA_CLASH || (r == GEA_MESH && j != with)) return 0;
    }
    return 1;
}

static int build(gea_map *t, const gea_spec *sp, uint32_t *r)
{
    uint8_t skel[GEA_PEGS] = {0};
    memset(t, 0, sizeof *t);
    t->w = sp->w;
    t->h = sp->h;
    t->sizes = sp->sizes;
    int ms = pick_size(r, sp->motor);
    if (sp->w < 2 * ms || sp->h < 2 * ms) return 0;
    int mx = sp->sym ? sp->w / 2 : ms + pr_below(r, sp->w - 2 * ms + 1), my = ms + pr_below(r, sp->h - 2 * ms + 1);
    skel[add_peg(t, mx, my, GEA_MOTOR, ms)] = (uint8_t)ms;
    int ends = sp->targets + sp->shafts;
    for (int k = 0; k < ends; k++) {
        int from = 0;
        if (k && pr_below(r, 2)) {              /* a branch from an earlier chain's gear */
            int c[GEA_PEGS], nc = 0;
            for (int i = 1; i < t->n; i++)
                if (t->p[i].kind == GEA_PEG) c[nc++] = i;
            if (nc) from = c[pr_below(r, nc)];
        }
        int len = sp->chain + pr_below(r, 2), cur = from, a = skel[from];
        for (int step = 0; step <= len; step++) {
            int end = step == len, b = end && k < sp->targets ? pick_size(r, sp->tsizes) : pick_size(r, sp->sizes);
            int x, y, ok = 0;
            for (int tries = 0; tries < 12 && !ok; tries++) {
                if (!hop(t, r, t->p[cur].x, t->p[cur].y, a + b, b, &x, &y)) continue;
                int i = add_peg(t, x, y, end ? (k < sp->targets ? GEA_TARGET : GEA_SHAFT) : GEA_PEG, end ? b : 0);
                if (i < 0) return 0;
                if (fits_tree(t, skel, i, b, cur)) ok = 1;
                else t->n--;
            }
            if (!ok) return 0;
            skel[t->n - 1] = (uint8_t)b;
            cur = t->n - 1;
            a = b;
        }
    }
    for (int d = 0; d < sp->decoys; d++) {      /* empty pegs beside the chains, most often where they close a loop */
        int cx[GEA_MAXW * GEA_MAXH], cy[GEA_MAXW * GEA_MAXH], cw[GEA_MAXW * GEA_MAXH];
        int nc = 0, total = 0;
        for (int y = 1; y < t->h; y++)
            for (int x = 1; x < t->w; x++) {
                if (!far_enough(t, x, y)) continue;
                int score = 0;
                for (int i = 0; i < t->n; i++) {
                    int dx = t->p[i].x - x, dy = t->p[i].y - y, d2 = dx * dx + dy * dy, hit = 0;
                    for (int b = 1; b <= GEA_SIZES && !hit; b++) {
                        if (!(sp->sizes >> b & 1) || x < b || y < b || x + b > t->w || y + b > t->h) continue;
                        for (int a = 1; a <= GEA_SIZES && !hit; a++)
                            if ((gea_fixed(t, i) ? a == t->p[i].size : sp->sizes >> a & 1) && d2 == (a + b) * (a + b))
                                hit = 1;
                    }
                    score += hit;
                }
                if (!score) continue;
                cx[nc] = x;
                cy[nc] = y;
                cw[nc] = score * score;
                total += cw[nc++];
            }
        if (!nc) break;
        int k = pr_below(r, total), i = 0;
        while (k >= cw[i]) k -= cw[i++];
        if (add_peg(t, cx[i], cy[i], GEA_PEG, 0) < 0) return 0;
    }
    if (sp->sym) {                              /* each peg's mirror image too (the motor sits on the axis) */
        int n0 = t->n;
        for (int i = 0; i < n0; i++) {
            int x = sp->w - t->p[i].x, y = t->p[i].y;
            if (x == t->p[i].x) continue;
            if (!far_enough(t, x, y)) continue;
            int k = add_peg(t, x, y, t->p[i].kind, t->p[i].size);
            if (k < 0) return 0;
        }
    }
    for (int i = 0; i < t->n; i++)              /* the fixed gears all clear of each other */
        for (int j = i + 1; j < t->n; j++)
            if (gea_fixed(t, i) && gea_fixed(t, j) && gea_rel(t, i, t->p[i].size, j, t->p[j].size) != GEA_FREE)
                return 0;
    return 1;
}

/* the board cut down round its pegs: each peg keeps room for the biggest gear it could take before (so nothing that
 * could be put can no longer be) */
static void crop(gea_map *m)
{
    int x0 = m->w, x1 = 0, y0 = m->h, y1 = 0;
    for (int i = 0; i < m->n; i++) {
        int s = gea_fixed(m, i) || m->p[i].kind == GEA_SHAFT ? m->p[i].size : 0;
        for (int k = GEA_SIZES; k >= 1 && !s; k--)
            if (m->sizes >> k & 1 && inside(m, i, k)) s = k;
        if (!s) s = 1;
        x0 = m->p[i].x - s < x0 ? m->p[i].x - s : x0;
        x1 = m->p[i].x + s > x1 ? m->p[i].x + s : x1;
        y0 = m->p[i].y - s < y0 ? m->p[i].y - s : y0;
        y1 = m->p[i].y + s > y1 ? m->p[i].y + s : y1;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > m->w) x1 = m->w;
    if (y1 > m->h) y1 = m->h;
    for (int i = 0; i < m->n; i++) {
        m->p[i].x = (uint8_t)(m->p[i].x - x0);
        m->p[i].y = (uint8_t)(m->p[i].y - y0);
    }
    m->w = x1 - x0;
    m->h = y1 - y0;
}

static long make_limit = 300000;

void gea_make(gea_map *m, const gea_spec *sp, uint32_t seed, int *par)
{
    uint32_t r = pr_seed(seed);
    int best = -1;
    gea_map t;
    *par = -1;
    for (int attempt = 0; attempt < 40 && best < 0; attempt++) {
        if (!build(&t, sp, &r)) continue;
        int ends[GEA_PEGS], ne = 0;
        for (int i = 0; i < t.n; i++)
            if (t.p[i].kind == GEA_TARGET || t.p[i].kind == GEA_SHAFT) ends[ne++] = i;
        if (!ne || ne > 4) continue;
        int top = -1, nc = 1 << ne, c0 = pr_below(&r, nc);
        for (int k = 0; k < nc; k++) {          /* the ways asked: the hardest that can be solved */
            int c = (c0 + k) % nc;
            for (int e = 0; e < ne; e++) t.p[ends[e]].dir = (int8_t)(c >> e & 1 ? -1 : 1);
            int p = gea_solve(&t, NULL, make_limit, NULL, NULL);
            if (p >= 1 && p <= sp->hi && p > top) { top = p; *m = t; }
        }
        if (top >= sp->lo) best = top;
    }
    if (best >= 1) crop(m);
    *par = best;
}

/* ---- the notation ----------------------------------------------------------------------------------------------- */
int gea_script(gea_game *g, const char *moves)
{
    for (const char *s = moves; s && *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            s++;
            for (int k = 0; k < (all ? 60 : 1); k++) {
                int peg, size;
                if (!gea_hint(&g->m, &g->s, g->par, &peg, &size)) break;
                gea_put(g, peg, size);
                if (gea_solved(&g->m, &g->s)) break;
            }
            continue;
        }
        int peg = *s - 'a';
        if (peg < 0 || peg >= g->m.n) return 0;
        s++;
        if (*s == '>') {
            int to = s[1] - 'a';
            if (!gea_shift(g, peg, to)) return 0;
            s += 2;
            continue;
        }
        if (*s < '0' || *s > '3' || !gea_put(g, peg, *s - '0')) return 0;
        s++;
    }
    return 1;
}

/* ---- levels and random boards ------------------------------------------------------------------------------------ */
void gea_variant(int v, gea_spec *sp)
{
    static const gea_spec V[GEA_VARIANTS] = {
        /* w, h, sizes, motor, tsizes, targets, shafts, chain, decoys, sym, lo, hi */
        {8, 10, 6, 6, 6, 1, 0, 2, 3, 0, 3, 4},
        {11, 13, 14, 14, 14, 2, 0, 2, 4, 0, 4, 6},
        {14, 17, 14, 14, 14, 2, 1, 2, 6, 0, 6, 8},
    };
    *sp = V[v < 0 || v >= GEA_VARIANTS ? 0 : v];
}

/* a naive player builds at random: any gear of the tray on any peg where it meshes with the train and jams nothing
 * (a placement that solves it taken at once), GEA_NAIVE times; the share that solve it (*near: within par + 1 gears) */
double gea_naive(const gea_map *m, int par, uint32_t seed, double *near)
{
    uint32_t r = pr_seed(seed);
    int w = 0, w2 = 0;
    for (int k = 0; k < GEA_NAIVE; k++) {
        gea_state s;
        gea_fresh(m, &s);
        for (int step = 0; step < 40 && !gea_solved(m, &s); step++) {
            int cp[GEA_PEGS * GEA_SIZES], cs[GEA_PEGS * GEA_SIZES], nc = 0, win = -1;
            for (int i = 0; i < m->n; i++)
                for (int z = 1; z <= GEA_SIZES; z++) {
                    if (!(m->sizes >> z & 1) || s.g[i] || gea_fixed(m, i) || gea_fits(m, &s, i, z) != GEA_OK) continue;
                    gea_state t = s;
                    t.g[i] = (uint8_t)z;
                    gea_eval_t e;
                    gea_eval(m, &t, &e);
                    if (!e.train[i] || e.jam) continue;
                    if (e.solved && win < 0) win = nc;
                    cp[nc] = i, cs[nc] = z, nc++;
                }
            if (!nc) break;
            int pick = win >= 0 ? win : pr_below(&r, nc);
            s.g[cp[pick]] = (uint8_t)cs[pick];
        }
        if (gea_solved(m, &s)) w++, w2 += gea_count(m, &s) <= par + 1;
    }
    if (near) *near = w2 / (double)GEA_NAIVE;
    return w / (double)GEA_NAIVE;
}

/* the chapters: the first, its idea alone, only a short start (one size of gear: any train that reaches the target
 * turns it the one way it can) */
static const int GEA_FIRST[GEA_CHAPTERS + 1] = {0, 3, 32, 61, 90, 120, GEA_LEVELS};

int gea_chapter_of(int level)
{
    int c = 0;
    while (c < GEA_CHAPTERS - 1 && level >= GEA_FIRST[c + 1]) c++;
    return c;
}

int gea_chapter_start(int chapter)
{
    return GEA_FIRST[chapter < 0 ? 0 : chapter > GEA_CHAPTERS ? GEA_CHAPTERS : chapter];
}

#ifndef GEA_GEN                                 /* not in the levels' tool (tools/gears_gen.c): it makes them */
#include "games/gears_levels.h"

int gea_level(gea_game *g, int level)
{
    if (level < 0 || level >= GEA_LEVELS || !gea_parse(&g->m, gea_levels[level].text)) return 0;
    gea_begin(g);
    g->par = gea_levels[level].par;
    return 1;
}

/* a board at random: made to the variant (its par in the variant's range; else a nearer one) */
void gea_random(gea_game *g, int variant, uint32_t seed)
{
    gea_spec sp;
    gea_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    gea_map best;
    int bpar = -1, got = 0;
    double bn = 2;
    /* of a few boards in range, the one naive players solve least (enough: a quarter of them) */
    for (int k = 0; k < 40 && got < 6 && bn > 0.25; k++) {
        int par;
        if (k && k % 10 == 0 && sp.lo > 1 && bpar < 0) sp.lo--;   /* none in range so far: a little easier */
        gea_make(&g->m, &sp, pr_next(&r), &par);
        if (par < 1) continue;
        got++;
        double nv = gea_naive(&g->m, par, pr_next(&r), NULL);
        if (nv < bn) bn = nv, best = g->m, bpar = par;
    }
    if (bpar < 1) {
        gea_level(g, 0);
        return;
    }
    g->m = best;
    gea_begin(g);
    g->par = bpar;
}
#endif
