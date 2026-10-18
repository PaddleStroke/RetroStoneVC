/*
 * Ring Puzzle's rules (rings.h): the rings and their shared places, a turn, the targets, the search from both ends
 * (the par, the hint), undo, the levels and the random puzzles, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/rings.h"

#define RG_PI 3.14159265358979f

int rg_build(rg_map *m, int kind, int n)
{
    memset(m, 0, sizeof *m);
    if ((n != 8 && n != 12) || (kind == RG_TRIANGLE && n != 12) || (kind == RG_CHAIN && n != 8) || kind < RG_TWO ||
        kind > RG_TRIANGLE)
        return 0;
    float a = 2 * RG_PI / n, d = 2 * cosf(n == 12 ? 2 * a : a);   /* (crossing two beads off the line: room in the
                                                                    * overlap for the beads of a big ring) */
    m->kind = kind;
    m->n = n;
    m->rings = kind == RG_TWO ? 2 : 3;
    m->cx[1] = d;
    if (kind == RG_CHAIN) m->cx[2] = 2 * d;
    if (kind == RG_TRIANGLE) m->cx[2] = d / 2, m->cy[2] = -d * 0.8660254f;
    for (int k = 0; k < m->rings; k++)
        for (int i = 0; i < n; i++) {
            float x = m->cx[k] + cosf(a * i), y = m->cy[k] - sinf(a * i);
            int p = -1;
            for (int j = 0; j < m->places; j++)
                if (fabsf(m->px[j] - x) < 1e-3f && fabsf(m->py[j] - y) < 1e-3f) p = j;
            if (p < 0) {
                if (m->places >= RG_PLACES) return 0;
                p = m->places++;
                m->px[p] = x;
                m->py[p] = y;
            }
            m->slot[k][i] = p;
        }
    return 1;
}

int rg_moves(const rg_map *m) { return 2 * m->rings; }

rg_pos rg_apply(const rg_map *m, rg_pos p, int move)
{
    int k = move >> 1, n = m->n, step = move & 1 ? n - 1 : 1;
    if (k < 0 || k >= m->rings) return p;
    rg_pos out = p;
    for (int i = 0; i < n; i++) {               /* slot i's bead goes to slot i + step */
        int from = m->slot[k][i], to = m->slot[k][(i + step) % n];
        out &= ~((rg_pos)3 << (2 * to));
        out |= (rg_pos)rg_colour(p, from) << (2 * to);
    }
    return out;
}

/* ---- the targets ------------------------------------------------------------------------------------------- */
static int members(const rg_map *m, int place, int *ring)
{
    int c = 0;
    for (int k = 0; k < m->rings; k++)
        for (int i = 0; i < m->n; i++)
            if (m->slot[k][i] == place) { if (ring) ring[c] = k; c++; break; }
    return c;
}

void rg_pattern(rg_map *m, int pattern, int colours)
{
    (void)colours;
    float gx = 0, gy = 0;                       /* the rings' middle */
    for (int k = 0; k < m->rings; k++) gx += m->cx[k] / m->rings, gy += m->cy[k] / m->rings;
    int shared = 0;
    m->target = 0;
    m->colours = pattern == 2 ? 4 : m->rings + (pattern == 1);
    for (int p = 0; p < m->places; p++) {
        int ring[RG_RINGS], c = members(m, p, ring), col;
        if (c > 1) col = ring[shared++ % c];
        else {
            int k = ring[0];
            col = k;
            float dx = m->px[p] - m->cx[k], dy = m->py[p] - m->cy[k], ox = m->cx[k] - gx, oy = m->cy[k] - gy;
            float ol = sqrtf(ox * ox + oy * oy);
            if (ol < 1e-3f) ox = 0, oy = -1, ol = 1;   /* (the middle ring of a row: its top) */
            if (pattern == 1 && (dx * ox + dy * oy) / ol > cosf(1.2f * 2 * RG_PI / m->n))
                col = m->rings;                 /* the far arc: the extra colour */
            if (pattern == 2 && dy > 1e-3f) col = k + 2;
        }
        m->target |= (rg_pos)col << (2 * p);
    }
}

int rg_text(const rg_map *m, rg_pos p, char *out)
{
    for (int i = 0; i < m->places; i++) out[i] = (char)('0' + rg_colour(p, i));
    out[m->places] = 0;
    return m->places;
}

int rg_parse(const rg_map *m, const char *s, rg_pos *p)
{
    *p = 0;
    if ((int)strlen(s) != m->places) return 0;
    for (int i = 0; i < m->places; i++) {
        if (s[i] < '0' || s[i] > '3') return 0;
        *p |= (rg_pos)(s[i] - '0') << (2 * i);
    }
    return 1;
}

/* ---- the search: from both ends, a layer at a time, the smaller side first ---------------------------------- */
#define CAP (1 << 18)
#define HASH (1 << 19)
typedef struct { rg_pos s; int32_t parent; int8_t move, depth; } fnode;
typedef struct { rg_pos s; int8_t depth; } bnode;

static uint32_t mix(rg_pos s)
{
    s ^= s >> 33;
    s *= 0xff51afd7ed558ccdULL;
    s ^= s >> 33;
    return (uint32_t)s & (HASH - 1);
}

static int find(const int32_t *h, const void *nodes, size_t size, rg_pos s)
{
    for (uint32_t i = mix(s);; i = (i + 1) & (HASH - 1)) {
        if (h[i] < 0) return -1;
        if (*(const rg_pos *)((const char *)nodes + size * (size_t)h[i]) == s) return h[i];
    }
}

static void put(int32_t *h, rg_pos s, int idx)
{
    uint32_t i = mix(s);
    while (h[i] >= 0) i = (i + 1) & (HASH - 1);
    h[i] = idx;
}

int rg_solve(const rg_map *m, rg_pos p, int *first)
{
    if (first) *first = -1;
    if (p == m->target) return 0;
    static fnode *F;                            /* (kept from one search to the next: the main thread's) */
    static bnode *B;
    static int32_t *hf, *hb;
    if (!F) F = malloc(sizeof *F * CAP), B = malloc(sizeof *B * CAP), hf = malloc(sizeof *hf * HASH), hb = malloc(sizeof *hb * HASH);
    int best = -1, nf = 0, nb = 0, moves = rg_moves(m);
    if (!F || !B || !hf || !hb) goto out;
    memset(hf, 0xff, sizeof *hf * HASH);
    memset(hb, 0xff, sizeof *hb * HASH);
    F[0] = (fnode){p, -1, -1, 0};
    put(hf, p, nf++);
    B[0] = (bnode){m->target, 0};
    put(hb, m->target, nb++);
    int f0 = 0, f1 = 1, b0 = 0, b1 = 1, df = 0, db = 0, meet = -1, meet_move = -1;
    while (best < 0 && df + db < RG_DEEP && f1 > f0 && b1 > b0) {
        if (f1 - f0 <= b1 - b0) {               /* a forward layer */
            for (int i = f0; i < f1; i++)
                for (int mv = 0; mv < moves; mv++) {
                    if (F[i].move >= 0 && mv == rg_inverse(F[i].move)) continue;
                    rg_pos s = rg_apply(m, F[i].s, mv);
                    if (find(hf, F, sizeof *F, s) >= 0) continue;
                    if (nf >= CAP) goto out;
                    F[nf] = (fnode){s, i, (int8_t)mv, (int8_t)(df + 1)};
                    put(hf, s, nf);
                    int j = find(hb, B, sizeof *B, s);
                    if (j >= 0 && (best < 0 || df + 1 + B[j].depth < best)) best = df + 1 + B[j].depth, meet = nf, meet_move = -1;
                    nf++;
                }
            f0 = f1;
            f1 = nf;
            df++;
        } else {                                /* a backward layer (every move has its inverse: the same moves) */
            for (int i = b0; i < b1; i++)
                for (int mv = 0; mv < moves; mv++) {
                    rg_pos s = rg_apply(m, B[i].s, mv);
                    if (find(hb, B, sizeof *B, s) >= 0) continue;
                    if (nb >= CAP) goto out;
                    B[nb] = (bnode){s, (int8_t)(db + 1)};
                    put(hb, s, nb);
                    int j = find(hf, F, sizeof *F, s);
                    if (j >= 0 && (best < 0 || F[j].depth + db + 1 < best))
                        best = F[j].depth + db + 1, meet = j, meet_move = j == 0 ? rg_inverse(mv) : -1;
                    nb++;
                }
            b0 = b1;
            b1 = nb;
            db++;
        }
    }
    if (best > 0 && first) {
        if (meet == 0) *first = meet_move;
        else {
            int i = meet;
            while (F[i].parent > 0) i = F[i].parent;
            *first = F[i].move;
        }
    }
out:
    return best;
}

/* ---- a game -------------------------------------------------------------------------------------------------- */
void rg_begin(rg_game *g, rg_pos start)
{
    g->p = g->start = start;
    g->nh = 0;
}

int rg_move(rg_game *g, int move)
{
    if (move < 0 || move >= rg_moves(&g->m)) return 0;
    g->p = rg_apply(&g->m, g->p, move);
    if (g->nh < RG_HIST) g->hist[g->nh++] = (uint8_t)move;
    else {                                      /* (undo's memory full: the oldest half forgotten) */
        memmove(g->hist, g->hist + RG_HIST / 2, RG_HIST / 2);
        g->nh = RG_HIST / 2;
        g->hist[g->nh++] = (uint8_t)move;
    }
    return 1;
}

int rg_undo(rg_game *g)
{
    if (!g->nh) return 0;
    g->p = rg_apply(&g->m, g->p, rg_inverse(g->hist[--g->nh]));
    return 1;
}

int rg_solved(const rg_game *g) { return g->p == g->m.target; }

int rg_hint(const rg_game *g)
{
    int first;
    if (rg_solved(g) || rg_solve(&g->m, g->p, &first) <= 0) return -1;
    return first;
}

int rg_script(rg_game *g, const char *moves)
{
    for (const char *s = moves; *s;) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            s++;
            do {
                int mv = rg_hint(g);
                if (mv < 0) break;
                rg_move(g, mv);
            } while (all && !rg_solved(g));
            continue;
        }
        if (*s < 'a' || *s >= 'a' + g->m.rings || (s[1] != '+' && s[1] != '-')) return 0;
        rg_move(g, (*s - 'a') * 2 + (s[1] == '-'));
        s += 2;
    }
    return 1;
}

#ifndef RG_GEN                                  /* not in the levels' tool (tools/rings_gen.c): it makes them */
#include "games/rings_levels.h"

int rg_level(rg_game *g, int level)
{
    if (level < 0 || level >= RG_LEVELS) return 0;
    const rg_level_def *d = &rg_levels[level];
    rg_pos t, s;
    if (!rg_build(&g->m, d->kind, d->n) || !rg_parse(&g->m, d->target, &t) || !rg_parse(&g->m, d->start, &s)) return 0;
    g->m.target = t;
    g->m.colours = d->colours;
    rg_begin(g, s);
    g->par = d->par;
    return 1;
}
#endif

void rg_random(rg_game *g, int variant, uint32_t seed)
{
    static const uint8_t KIND[RG_VARIANTS] = {RG_TWO, RG_TWO, RG_TRIANGLE}, N[RG_VARIANTS] = {8, 12, 12},
                         PAT[RG_VARIANTS] = {0, 1, 1}, LO[RG_VARIANTS] = {3, 5, 6}, HI[RG_VARIANTS] = {5, 7, 8};
    int v = variant < 0 || variant >= RG_VARIANTS ? 0 : variant;
    uint32_t r = pr_seed(seed);
    rg_build(&g->m, KIND[v], N[v]);
    rg_pattern(&g->m, PAT[v], 0);
    int aim = LO[v] + pr_below(&r, HI[v] - LO[v] + 1), best = -1;
    rg_pos keep = g->m.target;
    for (int t = 0; t < 40 && best != aim; t++) {
        rg_pos p = g->m.target;
        int last = -1, k = aim + pr_below(&r, 3);
        for (int i = 0; i < k; i++) {
            int mv;
            do mv = pr_below(&r, rg_moves(&g->m));
            while (last >= 0 && mv == rg_inverse(last));
            p = rg_apply(&g->m, p, mv);
            last = mv;
        }
        int par = rg_solve(&g->m, p, NULL);
        if (par > 0 && (best < 0 || abs(par - aim) < abs(best - aim))) best = par, keep = p;
    }
    rg_begin(g, keep);
    g->par = best > 0 ? best : 0;
}
