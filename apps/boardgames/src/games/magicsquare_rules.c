/*
 * Magic Squares: the rules (magicsquare.h): the figures' lines, the magic fillings made by chance, the count of
 * solutions, the maker, the player's tiles and undo. No SDL: tested by tests/magicsquare_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/magicsquare.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static int side_of(int kind) { return kind == MSQ_TRI3 ? 3 : kind == MSQ_TRI4 ? 4 : 5; }
static int is_square(int kind) { return kind == MSQ_SQ3 || kind == MSQ_SQ4; }

void msq_shape(msq_puzzle *p, int kind)
{
    memset(p, 0, sizeof *p);
    p->kind = kind;
    if (is_square(kind)) {
        int n = kind == MSQ_SQ3 ? 3 : 4;
        p->np = n * n;
        for (int i = 0; i < n; i++) {
            p->len[i] = p->len[n + i] = (uint8_t)n;
            for (int j = 0; j < n; j++) p->line[i][j] = (uint8_t)(i * n + j), p->line[n + i][j] = (uint8_t)(j * n + i);
        }
        p->len[2 * n] = p->len[2 * n + 1] = (uint8_t)n;
        for (int j = 0; j < n; j++) p->line[2 * n][j] = (uint8_t)(j * n + j), p->line[2 * n + 1][j] = (uint8_t)(j * n + n - 1 - j);
        p->nl = 2 * n + 2;
        return;
    }
    int k = side_of(kind), in = k - 2;
    p->np = 3 * (k - 1);
    p->nl = 3;
    for (int s = 0; s < 3; s++) {
        p->len[s] = (uint8_t)k;
        p->line[s][0] = (uint8_t)s;
        for (int j = 0; j < in; j++) p->line[s][1 + j] = (uint8_t)(3 + s * in + j);
        p->line[s][k - 1] = (uint8_t)((s + 1) % 3);
    }
}

static int line_sum(const msq_puzzle *p, int l, const int16_t *v)
{
    int s = 0;
    for (int j = 0; j < p->len[l]; j++) s += v[p->line[l][j]];
    return s;
}

int msq_magic(const msq_puzzle *p)
{
    for (int i = 0; i < p->np; i++) {
        if (p->val[i] < 1) return 0;
        for (int j = 0; j < i; j++)
            if (p->val[i] == p->val[j]) return 0;
    }
    for (int l = 0; l < p->nl; l++)
        if (line_sum(p, l, p->val) != p->target) return 0;
    return p->nl > 0;
}

/* ---- the count: the gaps filled from the tray, the lines checked as they fill --------------------------------- */
typedef struct {
    const msq_puzzle *p;
    int16_t v[MSQ_PLACES];
    int nv;
    int16_t tv[MSQ_PLACES];
    int8_t left[MSQ_PLACES];
    int order[MSQ_PLACES], no;
    long nodes;
    uint16_t lines_of[MSQ_PLACES];
} counter;

static int lines_ok(counter *c, int place)
{
    const msq_puzzle *p = c->p;
    int lo = 1 << 30, hi = 0;
    for (int t = 0; t < c->nv; t++)
        if (c->left[t] > 0) lo = c->tv[t] < lo ? c->tv[t] : lo, hi = c->tv[t] > hi ? c->tv[t] : hi;
    for (int l = 0; l < p->nl; l++) {
        if (!(c->lines_of[place] >> l & 1)) continue;
        int s = 0, u = 0;
        for (int j = 0; j < p->len[l]; j++) {
            int v = c->v[p->line[l][j]];
            if (v) s += v;
            else u++;
        }
        if (!u && s != p->target) return 0;
        if (u && (s + u * lo > p->target || s + u * hi < p->target)) return 0;
        if (u == 1) {
            int need = p->target - s, ok = 0;
            for (int t = 0; t < c->nv && !ok; t++) ok = c->left[t] > 0 && c->tv[t] == need;
            if (!ok) return 0;
        }
    }
    return 1;
}

static int count_in(counter *c, int k, int limit)
{
    c->nodes++;
    if (k == c->no) return 1;
    int place = c->order[k], n = 0;
    for (int t = 0; t < c->nv && n < limit; t++) {
        if (c->left[t] <= 0) continue;
        c->left[t]--;
        c->v[place] = c->tv[t];
        if (lines_ok(c, place)) n += count_in(c, k + 1, limit - n);
        c->v[place] = 0;
        c->left[t]++;
        if (c->nodes > 5000000) break;
    }
    return n;
}

int msq_count(const msq_puzzle *p, int limit, long *nodes)
{
    static counter c;
    memset(&c, 0, sizeof c);
    c.p = p;
    for (int l = 0; l < p->nl; l++)
        for (int j = 0; j < p->len[l]; j++) c.lines_of[p->line[l][j]] |= (uint16_t)(1 << l);
    for (int i = 0; i < p->np; i++) {
        if (p->given[i]) { c.v[i] = p->val[i]; continue; }
        int t = 0;
        while (t < c.nv && c.tv[t] != p->val[i]) t++;
        if (t == c.nv) c.tv[c.nv++] = p->val[i];
        c.left[t]++;
    }
    uint8_t done[MSQ_PLACES];                   /* the gaps in an order that closes lines early */
    for (int i = 0; i < p->np; i++) done[i] = p->given[i];
    for (;;) {
        int best = -1, bs = -1;
        for (int i = 0; i < p->np; i++) {
            if (done[i]) continue;
            int s = 0;
            for (int l = 0; l < p->nl; l++) {
                if (!(c.lines_of[i] >> l & 1)) continue;
                int f = 0;
                for (int j = 0; j < p->len[l]; j++) f += done[p->line[l][j]];
                s += f * f + 1;
            }
            if (s > bs) bs = s, best = i;
        }
        if (best < 0) break;
        done[best] = 1;
        c.order[c.no++] = best;
    }
    int n = count_in(&c, 0, limit);
    if (nodes) *nodes = c.nodes;
    return n;
}

/* ---- the magic fillings ------------------------------------------------------------------------------------------ */
/* a side's inner places filled from the numbers left (used: bit v) to make need; 1 found */
static int inner(int *out, int n, int need, uint32_t *used, int top, uint32_t *s)
{
    if (n == 0) return need == 0;
    int vs[32], nv = 0;
    for (int v = 1; v <= top; v++)
        if (!(*used >> v & 1) && v <= need) vs[nv++] = v;
    pr_shuffle(s, vs, nv);
    for (int i = 0; i < nv; i++) {
        *used |= 1u << vs[i];
        out[0] = vs[i];
        if (inner(out + 1, n - 1, need - vs[i], used, top, s)) return 1;
        *used &= ~(1u << vs[i]);
    }
    return 0;
}

static int fill_triangle(msq_puzzle *p, uint32_t *s)
{
    int k = side_of(p->kind), in = k - 2, N = p->np, total = N * (N + 1) / 2;
    for (int tries = 0; tries < 2000; tries++) {
        int c[3], vs[16];
        for (int v = 0; v < N; v++) vs[v] = v + 1;
        pr_shuffle(s, vs, N);
        c[0] = vs[0], c[1] = vs[1], c[2] = vs[2];
        if ((total + c[0] + c[1] + c[2]) % 3) continue;
        int S = (total + c[0] + c[1] + c[2]) / 3, ok = 1;
        uint32_t used = 1u << c[0] | 1u << c[1] | 1u << c[2];
        for (int i = 0; i < 3; i++) p->val[i] = (int16_t)c[i];
        for (int side = 0; side < 3 && ok; side++) {
            int out[4];
            ok = inner(out, in, S - c[side] - c[(side + 1) % 3], &used, N, s);
            for (int j = 0; ok && j < in; j++) p->val[3 + side * in + j] = (int16_t)out[j];
        }
        if (!ok) continue;
        p->target = S;
        return 1;
    }
    return 0;
}

/* the 4 x 4: the numbers 1..16 in a random order, row by row, every line 34 */
static int sq4_in(msq_puzzle *p, int cell, uint32_t *used, uint32_t *s)
{
    if (cell == 16) {
        int16_t *v = p->val;
        return v[0] + v[5] + v[10] + v[15] == 34 && v[3] + v[6] + v[9] + v[12] == 34;
    }
    int r = cell / 4, c = cell % 4, vs[16], nv = 0;
    for (int v = 1; v <= 16; v++)
        if (!(*used >> v & 1)) vs[nv++] = v;
    pr_shuffle(s, vs, nv);
    for (int i = 0; i < nv; i++) {
        int v = vs[i], row = v, col = v;
        for (int j = 0; j < c; j++) row += p->val[r * 4 + j];
        for (int j = 0; j < r; j++) col += p->val[j * 4 + c];
        if (row > 34 || col > 34 || (c == 3 && row != 34) || (r == 3 && col != 34)) continue;
        if (c < 3 && row + (3 - c) > 34) continue;
        if (r == 3 && c == 3 && p->val[0] + p->val[5] + p->val[10] + v != 34) continue;
        if (r == 3 && c == 0 && p->val[3] + p->val[6] + p->val[9] + v != 34) continue;
        p->val[cell] = (int16_t)v;
        *used |= 1u << v;
        if (sq4_in(p, cell + 1, used, s)) return 1;
        *used &= ~(1u << v);
    }
    p->val[cell] = 0;
    return 0;
}

static int fill(msq_puzzle *p, int spread, uint32_t *s)
{
    if (p->kind == MSQ_SQ3) {
        int c = 5, x, y;
        if (spread < 2) {
            static const int X[8][2] = {{1, 3}, {3, 1}, {-1, 3}, {3, -1}, {1, -3}, {-3, 1}, {-1, -3}, {-3, -1}};
            const int *q = X[pr_below(s, 8)];
            x = q[0], y = q[1];
        } else {
            c = 8 + pr_below(s, 18);
            do {
                x = pr_below(s, c) - c / 2, y = pr_below(s, c) - c / 2;
            } while (!x || !y || x == y || x == -y || 2 * x == y || x == 2 * y || 2 * x == -y || x == -2 * y);
        }
        const int v[9] = {c - x, c + x + y, c - y, c + x - y, c, c - x + y, c + y, c - x - y, c + x};
        for (int i = 0; i < 9; i++) p->val[i] = (int16_t)v[i];
        p->target = 3 * c;
    } else if (p->kind == MSQ_SQ4) {
        uint32_t used = 0;
        memset(p->val, 0, sizeof p->val);
        if (!sq4_in(p, 0, &used, s)) return 0;
        p->target = 34;
    } else if (!fill_triangle(p, s)) {
        return 0;
    }
    if (spread == 1) {                          /* times a factor plus a step: still magic */
        int a = 1 + pr_below(s, 3), b = pr_below(s, 8), n = 0;
        n = p->len[0];
        for (int i = 0; i < p->np; i++) p->val[i] = (int16_t)(a * p->val[i] + b);
        p->target = a * p->target + b * n;
    }
    return msq_magic(p);
}

void msq_make_puzzle(const msq_make *m, uint32_t seed, msq_puzzle *p)
{
    uint32_t s = pr_seed(seed);
    int kind = m->kind < 0 || m->kind >= MSQ_KINDS ? MSQ_SQ3 : m->kind;
    for (int attempt = 0; attempt < 50; attempt++) {
        msq_shape(p, kind);
        if (!fill(p, m->spread, &s)) continue;
        for (int i = 0; i < p->np; i++) p->given[i] = 1;
        int order[MSQ_PLACES], blanks = 0;
        for (int i = 0; i < p->np; i++) order[i] = i;
        pr_shuffle(&s, order, p->np);
        for (int i = 0; i < p->np && blanks < m->blanks; i++) {
            p->given[order[i]] = 0;
            if (msq_count(p, 2, NULL) == 1) blanks++;
            else p->given[order[i]] = 1;
        }
        if (blanks >= 2) return;
    }
    msq_shape(p, MSQ_SQ3);                      /* never: the plain 3 x 3, two gaps */
    const int16_t v[9] = {2, 7, 6, 9, 5, 1, 4, 3, 8};
    memcpy(p->val, v, sizeof v);
    p->target = 15;
    for (int i = 0; i < 9; i++) p->given[i] = i > 1;
}

msq_make msq_variant_make(int variant)
{
    static const msq_make V[MSQ_VARIANTS] = {{MSQ_TRI3, 4, 0}, {MSQ_SQ3, 5, 1}, {MSQ_TRI4, 6, 1}, {MSQ_SQ4, 9, 0}};
    return V[variant < 0 || variant >= MSQ_VARIANTS ? 0 : variant];
}

/* ---- as text ----------------------------------------------------------------------------------------------------- */
static const char KIND[] = "abcsq";

int msq_encode(const msq_puzzle *p, char *out, int size)
{
    int len = snprintf(out, (size_t)size, "%c:%d:", KIND[p->kind], p->target);
    for (int i = 0; i < p->np; i++) {
        int k = snprintf(out + len, (size_t)(size - len), "%s%s%d", i ? "," : "", p->given[i] ? "" : "?", p->val[i]);
        if (k < 0 || len + k >= size) return 0;
        len += k;
    }
    return len;
}

int msq_decode(const char *s, msq_puzzle *p)
{
    const char *k = s[0] ? strchr(KIND, s[0]) : NULL;
    if (!k || s[1] != ':') return 0;
    msq_shape(p, (int)(k - KIND));
    char *e;
    long t = strtol(s + 2, &e, 10);
    if (*e != ':' || t < 1) return 0;
    p->target = (int)t;
    s = e + 1;
    for (int i = 0; i < p->np; i++) {
        if (i && *s++ != ',') return 0;
        p->given[i] = *s != '?';
        if (*s == '?') s++;
        long v = strtol(s, &e, 10);
        if (e == s || v < 1 || v > 999) return 0;
        p->val[i] = (int16_t)v;
        s = e;
    }
    return !*s && msq_magic(p);
}

/* ---- the player's tiles ------------------------------------------------------------------------------------------ */
void msq_load(msq_game *g, const msq_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    for (int i = 0; i < p->np; i++)
        if (!p->given[i]) g->gaps[g->ngap++] = i;
    g->ntray = g->ngap;
    for (int i = 0; i < g->ngap; i++) g->tray[i] = p->val[g->gaps[i]];
    for (int i = 1; i < g->ntray; i++)
        for (int j = i; j > 0 && g->tray[j - 1] > g->tray[j]; j--) {
            int16_t t = g->tray[j];
            g->tray[j] = g->tray[j - 1];
            g->tray[j - 1] = t;
        }
    memset(g->at, -1, sizeof g->at);
}

void msq_new(msq_game *g, int variant, uint32_t seed)
{
    msq_puzzle p;
    msq_make m = msq_variant_make(variant);
    msq_make_puzzle(&m, seed, &p);
    msq_load(g, &p);
}

int msq_gap_of(const msq_game *g, int place)
{
    for (int i = 0; i < g->ngap; i++)
        if (g->gaps[i] == place) return i;
    return -1;
}

int msq_holder(const msq_game *g, int t)
{
    for (int i = 0; i < g->ngap; i++)
        if (g->at[i] == t) return i;
    return -1;
}

int msq_tile_free(const msq_game *g, int t) { return t >= 0 && t < g->ntray && msq_holder(g, t) < 0; }

int msq_value(const msq_game *g, int place)
{
    if (g->p.given[place]) return g->p.val[place];
    int i = msq_gap_of(g, place);
    return i >= 0 && g->at[i] >= 0 ? g->tray[(int)g->at[i]] : 0;
}

int msq_line_state(const msq_game *g, int l)
{
    int s = 0;
    for (int j = 0; j < g->p.len[l]; j++) {
        int v = msq_value(g, g->p.line[l][j]);
        if (!v) return 0;
        s += v;
    }
    return s == g->p.target ? 1 : -1;
}

static int wrong_lines(const msq_game *g)
{
    int n = 0;
    for (int l = 0; l < g->p.nl; l++) n += msq_line_state(g, l) < 0;
    return n;
}

static void push(msq_game *g)
{
    if (g->nh == MSQ_HIST) {
        memmove(g->hist[0], g->hist[1], sizeof g->hist[0] * (MSQ_HIST - 1));
        g->nh--;
    }
    memcpy(g->hist[g->nh++], g->at, sizeof g->at);
    g->moves++;
}

static void settle(msq_game *g)                 /* a full line gone wrong: a mistake */
{
    int w = wrong_lines(g);
    if (w > g->wrong_lines) g->mistakes += w - g->wrong_lines;
    g->wrong_lines = w;
}

int msq_put(msq_game *g, int i, int t)
{
    if (i < 0 || i >= g->ngap || t < 0 || t >= g->ntray || g->at[i] == t) return 0;
    push(g);
    int j = msq_holder(g, t);
    if (j >= 0) g->at[j] = -1;
    g->at[i] = (int8_t)t;
    settle(g);
    return 1;
}

int msq_take(msq_game *g, int i)
{
    if (i < 0 || i >= g->ngap || g->at[i] < 0) return 0;
    push(g);
    g->at[i] = -1;
    settle(g);
    return 1;
}

int msq_move(msq_game *g, int i, int j)
{
    if (i < 0 || j < 0 || i >= g->ngap || j >= g->ngap || i == j || g->at[i] < 0) return 0;
    push(g);
    int8_t t = g->at[i];
    g->at[i] = g->at[j];
    g->at[j] = t;
    settle(g);
    return 1;
}

int msq_undo(msq_game *g)
{
    if (!g->nh) return 0;
    memcpy(g->at, g->hist[--g->nh], sizeof g->at);
    g->moves--;
    g->wrong_lines = wrong_lines(g);
    return 1;
}

static int is_right(const msq_game *g, int i) { return g->at[i] >= 0 && g->tray[(int)g->at[i]] == g->p.val[g->gaps[i]]; }

static int right(msq_game *g, int i)
{
    int want = g->p.val[g->gaps[i]], pick = -1;
    for (int t = 0; t < g->ntray && pick < 0; t++)
        if (g->tray[t] == want && msq_tile_free(g, t)) pick = t;
    for (int t = 0; t < g->ntray && pick < 0; t++) {
        int h = msq_holder(g, t);
        if (g->tray[t] == want && h >= 0 && !is_right(g, h)) pick = t;
    }
    return pick >= 0 && msq_put(g, i, pick) ? i : -1;
}

/* the hint: a wrong tile put right; else the gap on the line nearest done (the fewest gaps left on one of its lines) */
int msq_hint(msq_game *g)
{
    if (msq_solved(g)) return -1;
    int i = -1;
    for (int k = 0; k < g->ngap && i < 0; k++)
        if (g->at[k] >= 0 && !is_right(g, k)) i = k;
    if (i < 0) {
        int best = 99;
        for (int k = 0; k < g->ngap; k++) {
            if (is_right(g, k)) continue;
            for (int l = 0; l < g->p.nl; l++) {
                int on = 0, open = 0;
                for (int j = 0; j < g->p.len[l]; j++) {
                    int pl = g->p.line[l][j], gi = msq_gap_of(g, pl);
                    on |= pl == g->gaps[k];
                    open += gi >= 0 && !is_right(g, gi);
                }
                if (on && open < best) best = open, i = k;
            }
        }
    }
    if (i < 0 || right(g, i) < 0) return -1;
    g->hints++;
    return i;
}

int msq_solved(const msq_game *g)
{
    if (!g->ngap) return 0;
    for (int i = 0; i < g->ngap; i++)
        if (g->at[i] < 0) return 0;
    for (int l = 0; l < g->p.nl; l++)
        if (msq_line_state(g, l) != 1) return 0;
    return 1;
}

int msq_script(msq_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '!') {                        /* a hint ('h' is a place) */
            if (msq_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int i = 0; i < g->ngap && k > 0; i++)
                if (!is_right(g, i)) right(g, i), k--;
            continue;
        }
        if (*s < 'a' || *s >= 'a' + g->p.np) return 0;
        int i = msq_gap_of(g, *s++ - 'a');
        if (i < 0) return 0;
        if (*s == '=') {
            int v = 0, t = -1;
            for (s++; *s >= '0' && *s <= '9'; s++) v = v * 10 + *s - '0';
            for (int k = 0; k < g->ntray && t < 0; k++)
                if (g->tray[k] == v && msq_tile_free(g, k)) t = k;
            for (int k = 0; k < g->ntray && t < 0; k++)
                if (g->tray[k] == v) t = k;
            if (t < 0 || !msq_put(g, i, t)) return 0;
        } else if (*s == '>') {
            s++;
            if (*s < 'a' || *s >= 'a' + g->p.np) return 0;
            if (!msq_move(g, i, msq_gap_of(g, *s++ - 'a'))) return 0;
        } else if (!msq_take(g, i)) {
            return 0;
        }
    }
    return 1;
}
