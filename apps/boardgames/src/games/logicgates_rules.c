/*
 * Logic Gates: the rules (logicgates.h): the levels read from their text, the drawing's tracks, the values, the
 * solver (every setting tried), the moves and undo, the hint, the notation, and the generator that helped make the
 * levels (logicgates_levels.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"
#include "games/logicgates_levels.h"

static const char *const KIND_NAME[LG_KINDS] = {"s", "NOT", "AND", "OR", "XOR", "NAND", "NOR", "y", "L", "A"};

int lg_gate(int kind, const uint8_t *in, int nin)
{
    int ones = 0;
    for (int i = 0; i < nin; i++) ones += in[i] != 0;
    switch (kind) {
    case LG_NOT: return ones == 0;
    case LG_AND: return ones == nin;
    case LG_OR: return ones > 0;
    case LG_XOR: return ones & 1;
    case LG_NAND: return ones != nin;
    case LG_NOR: return ones == 0;
    default: return 0;
    }
}

static int fail(char *err, int size, const char *fmt, ...)
{
    if (err && size > 0) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, (size_t)size, fmt, ap);
        va_end(ap);
    }
    return 0;
}

/* ---- the level's text -------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, locked, init, col, row, nref;
    int rcol[3], rrow[3], rport[3], rcut[3];
} lg_tok;

static int read_cell(const char **s, int *col, int *row)
{
    const char *p = *s;
    if (*p < 'a' || *p >= 'a' + LG_COLS || p[1] < '1' || p[1] >= '1' + LG_ROWS) return 0;
    *col = *p - 'a';
    *row = p[1] - '1';
    *s = p + 2;
    return 1;
}

static int read_kind(const char *s, int n, lg_tok *t)
{
    static const struct { const char *name; int kind, locked, init; } K[] = {
        {"s", LG_SWITCH, 0, 0}, {"S", LG_SWITCH, 0, 1}, {"k", LG_SWITCH, 1, 0}, {"K", LG_SWITCH, 1, 1},
        {"NOT", LG_NOT, 0, 0}, {"AND", LG_AND, 0, 0}, {"OR", LG_OR, 0, 0}, {"XOR", LG_XOR, 0, 0},
        {"NAND", LG_NAND, 0, 0}, {"NOR", LG_NOR, 0, 0}, {"y", LG_ROUTE, 0, 0}, {"Y", LG_ROUTE, 0, 1},
        {"L", LG_LAMP, 0, 0}, {"A", LG_ALARM, 0, 0}};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++)
        if ((int)strlen(K[i].name) == n && !strncmp(K[i].name, s, (size_t)n)) {
            t->kind = K[i].kind;
            t->locked = K[i].locked;
            t->init = K[i].init;
            return 1;
        }
    return 0;
}

static int ins_of(int kind, int n)
{
    switch (kind) {
    case LG_SWITCH: return n == 0;
    case LG_NOT: case LG_ROUTE: case LG_LAMP: case LG_ALARM: return n == 1;
    default: return n == 2 || n == 3;
    }
}

static int nports(int kind) { return kind == LG_ROUTE ? 2 : kind == LG_LAMP || kind == LG_ALARM ? 0 : 1; }

int lg_parse(const char *src, lg_level *L, char *err, int errsize)
{
    static lg_tok T[LG_NODES];
    int nt = 0, limit = -1;
    memset(L, 0, sizeof *L);
    for (const char *s = src; *s;) {
        if (*s == ' ') { s++; continue; }
        if (*s == '%') {
            limit = (int)strtol(s + 1, (char **)&s, 10);
            continue;
        }
        if (nt >= LG_NODES) return fail(err, errsize, "too many parts");
        lg_tok *t = &T[nt];
        memset(t, 0, sizeof *t);
        const char *at = strchr(s, '@');
        if (!at || !read_kind(s, (int)(at - s), t)) return fail(err, errsize, "a bad part at '%.12s'", s);
        s = at + 1;
        if (!read_cell(&s, &t->col, &t->row)) return fail(err, errsize, "a bad cell at '%.12s'", at);
        if (*s == '(') {
            s++;
            for (;;) {
                if (t->nref >= 3) return fail(err, errsize, "too many inputs at '%.12s'", at);
                int k = t->nref++;
                if (!read_cell(&s, &t->rcol[k], &t->rrow[k])) return fail(err, errsize, "a bad input at '%.12s'", s);
                if (*s == '<' || *s == '>') t->rport[k] = *s++ == '<' ? 1 : 2;
                if (*s == 'x') { t->rcut[k] = 1; s++; }
                if (*s == ',') { s++; continue; }
                if (*s == ')') { s++; break; }
                return fail(err, errsize, "a bad input list at '%.12s'", s);
            }
        }
        if (!ins_of(t->kind, t->nref)) return fail(err, errsize, "%s@%c%d: wrong inputs", KIND_NAME[t->kind],
                                                    'a' + t->col, t->row + 1);
        for (int i = 0; i < nt; i++)
            if (T[i].col == t->col && T[i].row == t->row) return fail(err, errsize, "two parts on one cell");
        nt++;
    }
    /* in reading order from the bottom left */
    for (int i = 1; i < nt; i++) {
        lg_tok t = T[i];
        int j = i;
        while (j > 0 && (T[j - 1].row > t.row || (T[j - 1].row == t.row && T[j - 1].col > t.col))) {
            T[j] = T[j - 1];
            j--;
        }
        T[j] = t;
    }
    int at[LG_COLS][LG_ROWS];
    memset(at, -1, sizeof at);
    for (int i = 0; i < nt; i++) {
        lg_node *n = &L->n[i];
        n->kind = (uint8_t)T[i].kind;
        n->col = (uint8_t)T[i].col;
        n->row = (uint8_t)T[i].row;
        n->nin = (uint8_t)T[i].nref;
        n->locked = (uint8_t)T[i].locked;
        n->init = (uint8_t)T[i].init;
        at[n->col][n->row] = i;
        if (n->col + 1 > L->cols) L->cols = n->col + 1;
        if (n->row + 1 > L->rows) L->rows = n->row + 1;
    }
    L->nn = nt;
    int used[LG_NODES][3] = {{0}}, lamps = 0;
    for (int i = 0; i < nt; i++) {
        lg_node *n = &L->n[i];
        if (n->kind == LG_SWITCH && n->row != 0) return fail(err, errsize, "a switch off the bottom row");
        lamps += n->kind == LG_LAMP;
        for (int k = 0; k < n->nin; k++) {
            int f = at[T[i].rcol[k]][T[i].rrow[k]], port = T[i].rport[k];
            if (f < 0) return fail(err, errsize, "an input from an empty cell %c%d", 'a' + T[i].rcol[k],
                                   T[i].rrow[k] + 1);
            int fk = L->n[f].kind;
            if (fk == LG_LAMP || fk == LG_ALARM) return fail(err, errsize, "a lamp's light used");
            if ((fk == LG_ROUTE) != (port != 0)) return fail(err, errsize, "a route switch's way (< >) is wrong");
            if (L->n[f].row >= n->row) return fail(err, errsize, "a wire not going up");
            if (L->nw >= LG_WIRES) return fail(err, errsize, "too many wires");
            lg_wire *w = &L->w[L->nw];
            w->from = (uint8_t)f;
            w->port = (uint8_t)port;
            w->to = (uint8_t)i;
            w->slot = (uint8_t)k;
            w->cuttable = (uint8_t)T[i].rcut[k];
            w->net = f * 3 + port;
            n->in[k] = (int8_t)L->nw++;
            used[f][port ? port - 1 : 0]++;
        }
    }
    if (!lamps) return fail(err, errsize, "no lamp");
    for (int i = 0; i < nt; i++)
        for (int p = 0; p < nports(L->n[i].kind); p++)
            if (!used[i][p]) return fail(err, errsize, "an output going nowhere at %c%d", 'a' + L->n[i].col,
                                         L->n[i].row + 1);
    /* the notation's numbers, the variables */
    for (int i = 0; i < nt; i++) {
        if (L->n[i].kind == LG_SWITCH) L->sw[L->nsw++] = i;
        if (L->n[i].kind == LG_ROUTE) L->rt[L->nrt++] = i;
    }
    for (int w = 0; w < L->nw; w++)
        if (L->w[w].cuttable) L->cut[L->ncut++] = w;
    L->cut_limit = limit < 0 || limit > L->ncut ? L->ncut : limit;
    for (int i = 0; i < L->nsw; i++)
        if (!L->n[L->sw[i]].locked) {
            if (L->nvar >= LG_VARS) return fail(err, errsize, "too many parts to change");
            L->var_kind[L->nvar] = LG_MSWITCH;
            L->var_idx[L->nvar++] = (uint8_t)i;
        }
    for (int i = 0; i < L->nrt + L->ncut; i++) {
        if (L->nvar >= LG_VARS) return fail(err, errsize, "too many parts to change");
        L->var_kind[L->nvar] = i < L->nrt ? LG_MROUTE : LG_MCUT;
        L->var_idx[L->nvar++] = (uint8_t)(i < L->nrt ? i : i - L->nrt);
    }
    for (int i = 0; i < nt; i++) {
        int d = 0;
        for (int k = 0; k < L->n[i].nin; k++) {
            int f = L->w[L->n[i].in[k]].from;
            if (L->depth[f] + 1 > d) d = L->depth[f] + 1;
        }
        L->depth[i] = d;
        if (d > L->maxdepth) L->maxdepth = d;
    }
    if (!lg_route(L)) {
        lg_check(L, err, errsize);
        return 0;
    }
    return 1;
}

/* ---- the drawing ------------------------------------------------------------------------------------------------- */
/* Laid out from the circuit alone (the cells in the text only name the parts).
 * - The rows: the switches, the gates by depth, the lamps on top. A wire rising past a row is an item of that row too,
 *   a trunk (one for the wires of a signal; a cuttable wire has its own).
 * - Each row's order: the fewest crossings, by an exact search of the orders (rows of up to EXACT items; else swaps of
 *   neighbours), swept up and down the rows until nothing improves. The switches' order means nothing to the puzzle;
 *   a route switch's left way stays on its left.
 * - The places: the switches a column apart; then row by row upwards, each part centred over its inputs, a trunk over
 *   its source, the row pushed apart only as far as it must be. A gate's input lies straight over its source when it
 *   can.
 * - Each gap between rows: one track for each signal going sideways there, the tracks ordered for the fewest
 *   crossings, those not side by side sharing a height, evenly spaced; the gap grows with its tracks (and when a fuse
 *   needs room). A wire rises from its source, runs along its track, and rises into its pin.
 * - A crossing left gets a hop (on the wire running sideways), a branching a dot. */
static float in_off(int nin, int slot)          /* an input's place under a part, columns from its middle (at first) */
{
    return nin == 1 ? 0 : nin == 2 ? (slot ? 0.18f : -0.18f) : (slot - 1) * 0.24f;
}
static float out_off(int port) { return port == 1 ? -0.18f : port == 2 ? 0.18f : 0; }

#define EPS 1e-4f
#define IDS (LG_NODES + LG_WIRES)               /* an item: a part (its index), or LG_NODES + a trunk */
#define EXACT 10                                /* rows ordered by an exact search: up to this many items */
#define SEP_PART 1.0f                           /* side by side, their middles at least (columns): two parts */
#define SEP_SIDE 0.55f                          /* a part and a trunk */
#define SEP_PASS 0.4f                           /* two trunks */
#define PIN_MAX 0.22f                           /* a gate's inputs: at most this far from its middle */
#define PIN_GAP 0.16f                           /* and this far apart */
#define SLACK 0.08f                             /* the room given up to keep a wire straight */
#define NOT_MAX 0.12f                           /* a NOT's input: this far */
#define GAP_MIN 0.5f                            /* a gap between rows, at least (rows) */
#define TRACK 0.14f                             /* its tracks at least this far apart */
#define BESIDE 0.15f                            /* two runs closer than this sideways need two heights */
#define TOUCH 0.06f                             /* a rise this near a run's end would seem to join it */
#define FUSE_ROOM 0.32f
#define BIG 1000

typedef struct { int u, v, port, key; } lg_edge;   /* in a gap: from item u (the row below) to item v */
typedef struct { int key, nd, lev; float src, lo, hi, dst[LG_WIRES]; } lg_run;   /* a signal in a gap */

static struct {
    int nl, ntr;
    int cnt[LG_ROWS], ord[LG_ROWS][IDS];        /* each row's items from the left */
    float pos[LG_ROWS][IDS], x[LG_ROWS][IDS];   /* an item's rank in its row, its place */
    int ne[LG_ROWS];
    lg_edge e[LG_ROWS][LG_WIRES];               /* the gap above each row */
    int tr_key[LG_WIRES], tr_bot[LG_WIRES], tr_top[LG_WIRES];   /* a trunk: its signal, the rows it passes */
    int tr_from[LG_WIRES], tr_port[LG_WIRES];
    int wtr[LG_WIRES];                          /* a wire's trunk (-1: it rises one row) */
    float pin[LG_WIRES];                        /* where it enters its part */
    int nr[LG_ROWS];
    lg_run run[LG_ROWS][LG_WIRES];              /* the signals in each gap */
    int wrun[LG_ROWS][LG_WIRES];                /* a wire's run in each gap */
    int nt[LG_ROWS];                            /* the heights used in each gap */
    float gap[LG_ROWS], grow[LG_ROWS];
} lay;

static int is_part(int id) { return id < LG_NODES; }
static int wkey(const lg_level *L, int w) { return L->w[w].cuttable ? 1000 + w : L->w[w].net; }
static float rank_off(int port) { return port == 1 ? -0.25f : port == 2 ? 0.25f : 0; }

/* the rows, the trunks, the edges in each gap */
static void build(lg_level *L)
{
    memset(&lay, 0, sizeof lay);
    int top = 0;
    for (int i = 0; i < L->nn; i++) {
        int k = L->n[i].kind;
        if (k != LG_LAMP && k != LG_ALARM && L->depth[i] > top) top = L->depth[i];
    }
    top++;                                      /* the lamps' row */
    for (int i = 0; i < L->nn; i++) {
        int k = L->n[i].kind;
        L->n[i].layer = (uint8_t)(k == LG_LAMP || k == LG_ALARM ? top : L->depth[i]);
    }
    lay.nl = top + 1;
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        int a = L->n[W->from].layer, b = L->n[W->to].layer, key = wkey(L, w), t = -1;
        lay.wtr[w] = -1;
        if (b <= a + 1) continue;
        for (int s = 0; s < lay.ntr; s++)
            if (lay.tr_key[s] == key) t = s;
        if (t < 0) {
            t = lay.ntr++;
            lay.tr_key[t] = key;
            lay.tr_bot[t] = a + 1;
            lay.tr_from[t] = W->from;
            lay.tr_port[t] = W->port;
        }
        if (b - 1 > lay.tr_top[t]) lay.tr_top[t] = b - 1;
        lay.wtr[w] = t;
    }
    /* the items, at first in the order of the text's columns */
    float key0[LG_ROWS][IDS];
    for (int i = 0; i < L->nn; i++) {
        int k = L->n[i].layer;
        key0[k][lay.cnt[k]] = L->n[i].col + 0.5f;
        lay.ord[k][lay.cnt[k]++] = i;
    }
    for (int t = 0; t < lay.ntr; t++)
        for (int k = lay.tr_bot[t]; k <= lay.tr_top[t]; k++) {
            key0[k][lay.cnt[k]] = L->n[lay.tr_from[t]].col + 0.5f + out_off(lay.tr_port[t]) + 0.01f * (t + 1);
            lay.ord[k][lay.cnt[k]++] = LG_NODES + t;
        }
    for (int k = 0; k < lay.nl; k++) {
        for (int i = 1; i < lay.cnt[k]; i++)
            for (int j = i; j > 0 && key0[k][j - 1] > key0[k][j]; j--) {
                float tk = key0[k][j];
                key0[k][j] = key0[k][j - 1];
                key0[k][j - 1] = tk;
                int ti = lay.ord[k][j];
                lay.ord[k][j] = lay.ord[k][j - 1];
                lay.ord[k][j - 1] = ti;
            }
        for (int i = 0; i < lay.cnt[k]; i++) lay.pos[k][lay.ord[k][i]] = (float)i;
    }
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        int a = L->n[W->from].layer, b = L->n[W->to].layer, t = lay.wtr[w];
        for (int k = a; k < b; k++) {
            lg_edge e = {k == a ? W->from : LG_NODES + t, k + 1 == b ? W->to : LG_NODES + t, k == a ? W->port : 0,
                         wkey(L, w)};
            int dup = 0;
            for (int i = 0; i < lay.ne[k]; i++) {
                const lg_edge *f = &lay.e[k][i];
                dup |= f->u == e.u && f->v == e.v && f->port == e.port && f->key == e.key;
            }
            if (!dup) lay.e[k][lay.ne[k]++] = e;
        }
    }
}

/* ---- the rows' order --------------------------------------------------------------------------------------------- */
static float src_rank(int k, const lg_edge *e) { return lay.pos[k][e->u] + (is_part(e->u) ? rank_off(e->port) : 0); }

static int gap_cross(int k)                     /* the crossings in the gap above row k, for the rows' orders */
{
    int n = 0;
    for (int i = 0; i < lay.ne[k]; i++)
        for (int j = i + 1; j < lay.ne[k]; j++) {
            const lg_edge *a = &lay.e[k][i], *b = &lay.e[k][j];
            if (a->key == b->key) continue;
            float s = src_rank(k, a) - src_rank(k, b), d = lay.pos[k + 1][a->v] - lay.pos[k + 1][b->v];
            n += s * d < 0;
        }
    return n;
}

static int all_cross(void)
{
    int n = 0;
    for (int k = 0; k + 1 < lay.nl; k++) n += gap_cross(k);
    return n;
}

/* row k's best order against its neighbours: c[i][j], the crossings when item i (its index now) is left of item j */
static void reorder(int k)
{
    int n = lay.cnt[k];
    if (n < 2) return;
    static int c[IDS][IDS];
    float bary[IDS], nb[IDS];
    for (int i = 0; i < n; i++) {
        memset(c[i], 0, sizeof(int) * (size_t)n);
        bary[i] = 0;
        nb[i] = 0;
    }
    if (k > 0)                                  /* the gap below: by where their edges come from */
        for (int i = 0; i < lay.ne[k - 1]; i++) {
            const lg_edge *a = &lay.e[k - 1][i];
            int ia = (int)lay.pos[k][a->v];
            bary[ia] += src_rank(k - 1, a);
            nb[ia]++;
            for (int j = i + 1; j < lay.ne[k - 1]; j++) {
                const lg_edge *b = &lay.e[k - 1][j];
                int ib = (int)lay.pos[k][b->v];
                if (a->key == b->key || ia == ib) continue;
                float sa = src_rank(k - 1, a), sb = src_rank(k - 1, b);
                if (sa > sb) c[ia][ib]++;
                else if (sa < sb) c[ib][ia]++;
            }
        }
    if (k + 1 < lay.nl)                         /* the gap above: by where their edges go */
        for (int i = 0; i < lay.ne[k]; i++) {
            const lg_edge *a = &lay.e[k][i];
            int ia = (int)lay.pos[k][a->u];
            bary[ia] += lay.pos[k + 1][a->v];
            nb[ia]++;
            for (int j = i + 1; j < lay.ne[k]; j++) {
                const lg_edge *b = &lay.e[k][j];
                int ib = (int)lay.pos[k][b->u];
                if (a->key == b->key || ia == ib) continue;
                float da = lay.pos[k + 1][a->v], db = lay.pos[k + 1][b->v];
                if (da > db) c[ia][ib]++;
                else if (da < db) c[ib][ia]++;
            }
        }
    for (int i = 0; i < n; i++) bary[i] = nb[i] > 0 ? bary[i] / nb[i] : (float)i;
    /* the cost: the crossings first; then the barycentres' order, then the order now */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j)
                c[i][j] = c[i][j] * 256 + (bary[i] > bary[j] + EPS ? 2 : fabsf(bary[i] - bary[j]) <= EPS && i > j);
    int perm[IDS];
    if (n <= EXACT) {                           /* every order: the best of each set of leftmost items */
        static int f[1 << EXACT];
        static signed char last[1 << EXACT];
        f[0] = 0;
        for (int S = 1; S < 1 << n; S++) {
            f[S] = 1 << 30;
            for (int j = 0; j < n; j++) {
                if (!(S >> j & 1)) continue;
                int R = S ^ (1 << j), cost = f[R];
                for (int i = 0; i < n; i++)
                    if (R >> i & 1) cost += c[i][j];
                if (cost < f[S]) { f[S] = cost; last[S] = (signed char)j; }
            }
        }
        for (int S = (1 << n) - 1, p = n - 1; S; p--) {
            perm[p] = last[S];
            S ^= 1 << last[S];
        }
    } else {                                    /* neighbours swapped while it helps */
        for (int i = 0; i < n; i++) perm[i] = i;
        for (int again = 1; again;) {
            again = 0;
            for (int p = 0; p + 1 < n; p++)
                if (c[perm[p + 1]][perm[p]] < c[perm[p]][perm[p + 1]]) {
                    int t = perm[p];
                    perm[p] = perm[p + 1];
                    perm[p + 1] = t;
                    again = 1;
                }
        }
    }
    int was[IDS];
    memcpy(was, lay.ord[k], sizeof(int) * (size_t)n);
    for (int p = 0; p < n; p++) {
        lay.ord[k][p] = was[perm[p]];
        lay.pos[k][lay.ord[k][p]] = (float)p;
    }
}

static void order_rows(void)
{
    static int best_ord[LG_ROWS][IDS], prev[LG_ROWS][IDS];
    int best = all_cross();
    memcpy(best_ord, lay.ord, sizeof best_ord);
    for (int round = 0; round < 8; round++) {
        memcpy(prev, lay.ord, sizeof prev);
        for (int k = 0; k < lay.nl; k++) reorder(k);
        for (int k = lay.nl - 1; k >= 0; k--) reorder(k);
        int n = all_cross();
        if (n < best) {
            best = n;
            memcpy(best_ord, lay.ord, sizeof best_ord);
        }
        if (!memcmp(prev, lay.ord, sizeof prev)) break;
    }
    memcpy(lay.ord, best_ord, sizeof best_ord);
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) lay.pos[k][lay.ord[k][i]] = (float)i;
}

/* ---- the places -------------------------------------------------------------------------------------------------- */
/* x[i] as near d[i] as can be (weights w) with x[i + 1] >= x[i] + s[i]: the blocks pooled at their mean */
static void spread(int n, const float *d, const float *w, const float *s, float *x)
{
    float bv[IDS], bw[IDS], off = 0;
    int bn[IDS], nb = 0;
    for (int i = 0; i < n; i++) {
        if (i) off += s[i - 1];
        bv[nb] = d[i] - off;
        bw[nb] = w[i];
        bn[nb++] = 1;
        while (nb > 1 && bv[nb - 2] > bv[nb - 1]) {
            float W = bw[nb - 2] + bw[nb - 1];
            bv[nb - 2] = (bv[nb - 2] * bw[nb - 2] + bv[nb - 1] * bw[nb - 1]) / W;
            bw[nb - 2] = W;
            bn[nb - 2] += bn[nb - 1];
            nb--;
        }
    }
    off = 0;
    for (int b = 0, i = 0; b < nb; b++)
        for (int k = 0; k < bn[b]; k++, i++) {
            if (i) off += s[i - 1];
            x[i] = bv[b] + off;
        }
}

static float sep(int a, int b)
{
    return is_part(a) && is_part(b) ? SEP_PART : is_part(a) || is_part(b) ? SEP_SIDE : SEP_PASS;
}

/* where a wire leaves row k (its source's output, or its trunk) and where it reaches row k + 1 */
static float wire_src(const lg_level *L, int w, int k)
{
    const lg_wire *W = &L->w[w];
    return k == L->n[W->from].layer ? lay.x[k][W->from] + out_off(W->port) : lay.x[k][LG_NODES + lay.wtr[w]];
}
static float wire_dst(const lg_level *L, int w, int k)
{
    return k + 1 == L->n[L->w[w].to].layer ? lay.pin[w] : lay.x[k + 1][LG_NODES + lay.wtr[w]];
}

/* x is where another signal rises from row k: a pin there would seem to join it */
static int on_rise(const lg_level *L, int k, int w, float x)
{
    for (int v = 0; v < L->nw; v++) {
        const lg_wire *V = &L->w[v];
        if (V->net == L->w[w].net || L->n[V->from].layer > k || L->n[V->to].layer <= k) continue;
        if (fabsf(wire_src(L, v, k) - x) < 0.1f) return 1;
    }
    return 0;
}

/* pin s of a part at x (its inputs p, from its middle) may move to c: under the part, apart from its neighbours */
static int pin_ok(const lg_level *L, int k, int w, float x, const float *p, int nin, int s, float c, float lim)
{
    return fabsf(c) <= lim + EPS && (s == 0 || c - p[s - 1] >= PIN_GAP - EPS) &&
           (s == nin - 1 || p[s + 1] - c >= PIN_GAP - EPS) && !on_rise(L, k, w, x + c);
}

static void place(lg_level *L)
{
    for (int i = 0; i < lay.cnt[0]; i++) lay.x[0][lay.ord[0][i]] = i * SEP_PART;
    for (int k = 1; k < lay.nl; k++) {
        int n = lay.cnt[k];
        float d[IDS], w[IDS], s[IDS], r[IDS];
        for (int i = 0; i < n; i++) {
            int id = lay.ord[k][i], m = 0;
            float sum = 0;
            for (int j = 0; j < lay.ne[k - 1]; j++) {
                const lg_edge *e = &lay.e[k - 1][j];
                if (e->v != id) continue;
                sum += lay.x[k - 1][e->u] + (is_part(e->u) ? out_off(e->port) : 0);
                m++;
            }
            d[i] = m ? sum / m : (i ? d[i - 1] + 1 : 0);
            w[i] = is_part(id) ? 1.0f : 3.0f;
            if (i) s[i - 1] = sep(lay.ord[k][i - 1], id);
        }
        spread(n, d, w, s, r);
        /* pushed a hair off straight: straight, if its neighbours stay nearly as far */
        for (int i = 0; i < n; i++)
            if (fabsf(r[i] - d[i]) < 0.12f && (i == 0 || d[i] - r[i - 1] >= s[i - 1] - SLACK) &&
                (i == n - 1 || r[i + 1] - d[i] >= s[i] - SLACK))
                r[i] = d[i];
        for (int i = 0; i < n; i++) lay.x[k][lay.ord[k][i]] = r[i];
    }
    /* from half a column in */
    float lo = 1e9f, hi = -1e9f;
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) {
            int id = lay.ord[k][i];
            float m = is_part(id) ? 0.5f : 0.3f;
            lo = fminf(lo, lay.x[k][id] - m);
            hi = fmaxf(hi, lay.x[k][id] + m);
        }
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) lay.x[k][lay.ord[k][i]] -= lo;
    L->width = hi - lo;
    for (int i = 0; i < L->nn; i++) L->n[i].x = lay.x[L->n[i].layer][i];
    /* the pins: spread under the part, or straight over their sources when they can be */
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        int k = N->layer - 1, w[3];
        float xs[3] = {0, 0, 0}, p[3];
        for (int s = 0; s < N->nin; s++) {
            w[s] = N->in[s];
            xs[s] = wire_src(L, w[s], k);
        }
        for (int a = 1; a < N->nin; a++)
            for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
                float t = xs[b];
                xs[b] = xs[b - 1];
                xs[b - 1] = t;
                int tw = w[b];
                w[b] = w[b - 1];
                w[b - 1] = tw;
            }
        for (int s = 0; s < N->nin; s++) p[s] = in_off(N->nin, s);
        if (N->nin == 1 && N->kind == LG_NOT && fabsf(xs[0] - N->x) <= NOT_MAX) p[0] = xs[0] - N->x;
        if (N->nin > 1)
            for (int pass = 0; pass < 3; pass++)
                for (int s = 0; s < N->nin; s++) {
                    /* straight over its source; else its place, or near it, off the other signals' rises */
                    static const float nudge[] = {0, 0.04f, -0.04f, 0.08f, -0.08f, 0.12f, -0.12f};
                    float c = xs[s] - N->x;
                    if (pass && pin_ok(L, k, w[s], N->x, p, N->nin, s, c, PIN_MAX)) { p[s] = c; continue; }
                    if (pass) continue;
                    for (size_t q = 0; q < sizeof nudge / sizeof nudge[0]; q++)
                        if (pin_ok(L, k, w[s], N->x, p, N->nin, s, p[s] + nudge[q], PIN_MAX)) {
                            p[s] += nudge[q];
                            break;
                        }
                }
        for (int s = 0; s < N->nin; s++) lay.pin[w[s]] = N->x + p[s];
    }
}

/* ---- the tracks -------------------------------------------------------------------------------------------------- */
static int inside(float x, float lo, float hi)  /* a rise at x against a run lo..hi: 1 crossing, BIG touching it */
{
    if (x > lo + TOUCH && x < hi - TOUCH) return 1;
    return x > lo - TOUCH && x < hi + TOUCH ? BIG : 0;
}

static int below_cost(const lg_run *A, const lg_run *B)   /* A's track under B's */
{
    int c = inside(B->src, A->lo, A->hi);       /* B rises from below past A's track */
    for (int d = 0; d < A->nd; d++) {
        c += inside(A->dst[d], B->lo, B->hi);   /* A rises above past B's track */
        if (fabsf(A->dst[d] - B->src) < 0.1f) c += BIG;   /* one line: B's rise and A's */
    }
    return c;
}

static void tracks(const lg_level *L, int k)
{
    lay.nr[k] = 0;
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        int a = L->n[W->from].layer, b = L->n[W->to].layer, key = wkey(L, w), r = -1;
        lay.wrun[k][w] = -1;
        if (k < a || k >= b) continue;
        for (int i = 0; i < lay.nr[k]; i++)
            if (lay.run[k][i].key == key) r = i;
        if (r < 0) {
            r = lay.nr[k]++;
            lg_run *R = &lay.run[k][r];
            R->key = key;
            R->nd = 0;
            R->src = R->lo = R->hi = wire_src(L, w, k);
        }
        lg_run *R = &lay.run[k][r];
        float xd = wire_dst(L, w, k);
        int dup = 0;
        for (int d = 0; d < R->nd; d++) dup |= fabsf(R->dst[d] - xd) < EPS;
        if (!dup) R->dst[R->nd++] = xd;
        R->lo = fminf(R->lo, xd);
        R->hi = fmaxf(R->hi, xd);
        lay.wrun[k][w] = r;
    }
    /* the runs going sideways: their order from the bottom */
    int m = 0, id[LG_WIRES];
    for (int r = 0; r < lay.nr[k]; r++) {
        lay.run[k][r].lev = 0;
        if (lay.run[k][r].hi - lay.run[k][r].lo > EPS) id[m++] = r;
    }
    int perm[LG_WIRES];
    static int c[LG_WIRES][LG_WIRES];
    for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
            c[i][j] = i == j ? 0 : below_cost(&lay.run[k][id[i]], &lay.run[k][id[j]]) * 4 + (i > j);
    if (m <= 12) {
        static int f[1 << 12];
        static signed char last[1 << 12];
        f[0] = 0;
        for (int S = 1; S < 1 << m; S++) {
            f[S] = 1 << 30;
            for (int j = 0; j < m; j++) {
                if (!(S >> j & 1)) continue;
                int R = S ^ (1 << j), cost = f[R];
                for (int i = 0; i < m; i++)
                    if (R >> i & 1) cost += c[i][j];
                if (cost < f[S]) { f[S] = cost; last[S] = (signed char)j; }
            }
        }
        for (int S = (1 << m) - 1, p = m - 1; S; p--) {
            perm[p] = last[S];
            S ^= 1 << last[S];
        }
    } else {
        for (int i = 0; i < m; i++) perm[i] = i;
        for (int again = 1; again;) {
            again = 0;
            for (int p = 0; p + 1 < m; p++)
                if (c[perm[p + 1]][perm[p]] < c[perm[p]][perm[p + 1]]) {
                    int t = perm[p];
                    perm[p] = perm[p + 1];
                    perm[p + 1] = t;
                    again = 1;
                }
        }
    }
    /* their heights: each above the ones under it that it runs beside */
    lay.nt[k] = 0;
    for (int p = 0; p < m; p++) {
        lg_run *R = &lay.run[k][id[perm[p]]];
        int lev = 1;
        for (int q = 0; q < p; q++) {
            const lg_run *Q = &lay.run[k][id[perm[q]]];
            if (Q->lo < R->hi + BESIDE && R->lo < Q->hi + BESIDE && Q->lev + 1 > lev) lev = Q->lev + 1;
        }
        R->lev = lev;
        if (lev > lay.nt[k]) lay.nt[k] = lev;
    }
}

/* the rows' heights and the wires' points */
static void paths(lg_level *L)
{
    L->ly[0] = 0;
    for (int k = 0; k + 1 < lay.nl; k++) {
        lay.gap[k] = fmaxf(GAP_MIN, TRACK * (lay.nt[k] + 1)) + lay.grow[k];
        L->ly[k + 1] = L->ly[k] + LG_NH + lay.gap[k];
    }
    L->layers = lay.nl;
    L->height = L->ly[lay.nl - 1] + LG_NH;
    for (int i = 0; i < L->nn; i++) L->n[i].y = L->ly[L->n[i].layer];
    for (int w = 0; w < L->nw; w++) {
        lg_wire *W = &L->w[w];
        int a = L->n[W->from].layer, b = L->n[W->to].layer, n = 0;
        float cur = wire_src(L, w, a);
        W->px[n] = cur;
        W->py[n++] = L->ly[a] + LG_NH;
        for (int k = a; k < b; k++) {
            float xd = wire_dst(L, w, k);
            if (fabsf(xd - cur) > EPS) {
                const lg_run *R = &lay.run[k][lay.wrun[k][w]];
                float ty = L->ly[k] + LG_NH + lay.gap[k] * R->lev / (lay.nt[k] + 1);
                W->px[n] = cur;
                W->py[n++] = ty;
                W->px[n] = xd;
                W->py[n++] = ty;
                cur = xd;
            }
        }
        W->px[n] = cur;
        W->py[n++] = L->ly[b];
        W->npt = n;
        W->len = 0;
        for (int i = 0; i + 1 < n; i++) W->len += fabsf(W->px[i + 1] - W->px[i]) + fabsf(W->py[i + 1] - W->py[i]);
    }
}

/* ---- the fuses, the hops, the dots, the checks ------------------------------------------------------------------- */
typedef struct { float a, b, c; int vert, wire, net; } lg_seg;   /* vertical: x = c, y a..b; horizontal: y = c */

static int segments(const lg_level *L, lg_seg *S)
{
    int n = 0;
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        for (int i = 0; i + 1 < W->npt; i++) {
            float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
            if (fabsf(x0 - x1) < EPS && fabsf(y0 - y1) < EPS) continue;
            lg_seg *s = &S[n++];
            s->vert = fabsf(x0 - x1) < EPS;
            s->a = s->vert ? fminf(y0, y1) : fminf(x0, x1);
            s->b = s->vert ? fmaxf(y0, y1) : fmaxf(x0, x1);
            s->c = s->vert ? x0 : y0;
            s->wire = w;
            s->net = W->net;
        }
    }
    return n;
}

/* a cuttable wire's fuse: the middle of the longest stretch of it that is its own (no other wire along it) and clear
 * of crossings; its length */
static float fuse_spot(const lg_level *L, int w, float *fs, float *fx, float *fy, int *fvert)
{
    static lg_seg S[LG_WIRES * LG_PTS];
    int n = segments(L, S);
    const lg_wire *W = &L->w[w];
    float best = 0, along = 0;
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        int vert = fabsf(x0 - x1) < EPS;
        float a = vert ? fminf(y0, y1) : fminf(x0, x1), b = vert ? fmaxf(y0, y1) : fmaxf(x0, x1),
              c = vert ? x0 : y0, len = b - a;
        if (len < EPS) continue;
        /* the blocked spans along it */
        static float lo[LG_WIRES * LG_PTS + 2], hi[LG_WIRES * LG_PTS + 2];
        int nb = 0;
        lo[nb] = a - 1;
        hi[nb++] = a + 0.07f;
        lo[nb] = b - 0.07f;
        hi[nb++] = b + 1;
        for (int k = 0; k < n; k++) {
            const lg_seg *t = &S[k];
            if (t->wire == w) continue;
            if (t->vert == vert) {
                if (fabsf(t->c - c) < 0.05f && t->a < b && t->b > a) { lo[nb] = t->a - 0.05f; hi[nb++] = t->b + 0.05f; }
            } else if (t->c > a - 0.1f && t->c < b + 0.1f && c > t->a - 0.05f && c < t->b + 0.05f) {
                lo[nb] = t->c - 0.1f;
                hi[nb++] = t->c + 0.1f;
            }
        }
        for (int p = 0; p < nb; p++)            /* the free spans: sweep the blocked ones in order */
            for (int q = p + 1; q < nb; q++)
                if (lo[q] < lo[p]) {
                    float tl = lo[p], th = hi[p];
                    lo[p] = lo[q];
                    hi[p] = hi[q];
                    lo[q] = tl;
                    hi[q] = th;
                }
        float reach = a - 1;
        for (int p = 0; p <= nb; p++) {
            float start = reach, end = p < nb ? lo[p] : b + 1;
            if (end > start && end - start > best) {
                float m = (start + end) / 2;
                best = end - start;
                *fvert = vert;
                *fx = vert ? c : m;
                *fy = vert ? m : c;
                *fs = along + (vert ? fabsf(m - y0) : fabsf(m - x0));
            }
            if (p < nb && hi[p] > reach) reach = hi[p];
        }
        along += len;
    }
    return best;
}

static float along(const lg_wire *W, float x, float y)   /* how far along the wire a point of it lies */
{
    float s = 0;
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        if (x >= fminf(x0, x1) - EPS && x <= fmaxf(x0, x1) + EPS && y >= fminf(y0, y1) - EPS &&
            y <= fmaxf(y0, y1) + EPS)
            return s + fabsf(x - x0) + fabsf(y - y0);
        s += fabsf(x1 - x0) + fabsf(y1 - y0);
    }
    return s;
}

/* the crossings (a hop on the wire running sideways) and the branchings (a dot where a signal goes three ways) */
static void marks(lg_level *L)
{
    static lg_seg S[LG_WIRES * LG_PTS];
    int n = segments(L, S);
    L->nhop = L->njoint = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            const lg_seg *v = &S[i], *h = &S[j];
            if (!v->vert || h->vert || wkey(L, v->wire) == wkey(L, h->wire)) continue;
            if (v->c <= h->a + 1e-3f || v->c >= h->b - 1e-3f || h->c <= v->a + 1e-3f || h->c >= v->b - 1e-3f) continue;
            int dup = 0;
            for (int k = 0; k < L->nhop; k++) dup |= fabsf(L->hop[k].x - v->c) < 1e-3f && fabsf(L->hop[k].y - h->c) < 1e-3f;
            if (dup || L->nhop >= LG_MARKS) continue;
            lg_mark *m = &L->hop[L->nhop++];
            m->x = v->c;
            m->y = h->c;
            m->wire = h->wire;
            m->s = along(&L->w[h->wire], m->x, m->y);
        }
    L->crossings = L->nhop;
    for (int w = 0; w < L->nw; w++) {
        const lg_wire *W = &L->w[w];
        for (int p = 1; p + 1 < W->npt; p++) {
            float x = W->px[p], y = W->py[p];
            int ways = 0;                       /* 1 down, 2 up, 4 left, 8 right */
            for (int i = 0; i < n; i++) {
                const lg_seg *s = &S[i];
                if (s->net != W->net) continue;
                if (s->vert && fabsf(s->c - x) < EPS && y > s->a - EPS && y < s->b + EPS)
                    ways |= (y > s->a + EPS) | (y < s->b - EPS) << 1;
                if (!s->vert && fabsf(s->c - y) < EPS && x > s->a - EPS && x < s->b + EPS)
                    ways |= (x > s->a + EPS) << 2 | (x < s->b - EPS) << 3;
            }
            if ((ways & 1) + (ways >> 1 & 1) + (ways >> 2 & 1) + (ways >> 3 & 1) < 3) continue;
            int dup = 0;
            for (int k = 0; k < L->njoint; k++)
                dup |= fabsf(L->joint[k].x - x) < 1e-3f && fabsf(L->joint[k].y - y) < 1e-3f;
            if (dup || L->njoint >= LG_MARKS) continue;
            L->joint[L->njoint++] = (lg_mark){x, y, along(W, x, y), w};
        }
    }
}

int lg_check(const lg_level *L, char *err, int size)
{
    static lg_seg S[LG_WIRES * LG_PTS];
    int n = segments(L, S);
    for (int i = 0; i < L->nn; i++)             /* the parts apart */
        for (int j = i + 1; j < L->nn; j++) {
            const lg_node *A = &L->n[i], *B = &L->n[j];
            if (fabsf(A->x - B->x) < 2 * LG_HALF + 0.1f && A->y < B->y + LG_NH + 0.05f && B->y < A->y + LG_NH + 0.05f)
                return fail(err, size, "parts %d and %d overlap", i, j);
        }
    for (int i = 0; i < n; i++) {
        const lg_seg *s = &S[i];
        for (int k = 0; k < L->nn; k++) {       /* no wire through a part */
            const lg_node *N = &L->n[k];
            if (k == L->w[s->wire].from || k == L->w[s->wire].to) continue;
            float x0 = N->x - LG_HALF - 0.03f, x1 = N->x + LG_HALF + 0.03f, y0 = N->y - 0.02f, y1 = N->y + LG_NH + 0.02f;
            int hit = s->vert ? s->c > x0 && s->c < x1 && s->a < y1 && s->b > y0
                              : s->c > y0 && s->c < y1 && s->a < x1 && s->b > x0;
            if (hit) return fail(err, size, "wire %d through part %d", s->wire, k);
        }
        for (int j = i + 1; j < n; j++) {
            const lg_seg *t = &S[j];
            if (s->net == t->net) continue;
            if (s->vert == t->vert) {           /* two signals along one line */
                float gap = s->vert ? 0.07f : 0.05f;
                if (fabsf(s->c - t->c) < gap && s->a < t->b + 0.04f && t->a < s->b + 0.04f)
                    return fail(err, size, "wires %d and %d along one line", s->wire, t->wire);
                continue;
            }
            const lg_seg *h = s->vert ? t : s, *v = s->vert ? s : t;   /* a crossing: square, not at an end */
            if (v->c < h->a - 0.04f || v->c > h->b + 0.04f || h->c < v->a - 0.04f || h->c > v->b + 0.04f) continue;
            if (v->c < h->a + TOUCH || v->c > h->b - TOUCH || h->c < v->a + TOUCH || h->c > v->b - TOUCH)
                return fail(err, size, "wires %d and %d touch", v->wire, h->wire);
        }
    }
    for (int w = 0; w < L->nw; w++) {           /* each fuse has room */
        float fs, fx, fy;
        int fv;
        if (L->w[w].cuttable && fuse_spot(L, w, &fs, &fx, &fy, &fv) < FUSE_ROOM)
            return fail(err, size, "no room for the fuse on wire %d", w);
    }
    return 1;
}

/* from the rows' order: the places, the tracks, the paths (the gaps grown for fuses without room), the hops and dots */
static void finish(lg_level *L)
{
    memset(lay.grow, 0, sizeof lay.grow);
    place(L);
    for (int k = 0; k + 1 < lay.nl; k++) tracks(L, k);
    for (int tries = 0;; tries++) {
        paths(L);
        int short_of = 0;
        float grow[LG_ROWS] = {0};
        for (int w = 0; w < L->nw; w++) {
            lg_wire *W = &L->w[w];
            if (!W->cuttable || fuse_spot(L, w, &W->fs, &W->fx, &W->fy, &W->fvert) >= FUSE_ROOM) continue;
            short_of = 1;
            for (int k = L->n[W->from].layer; k < L->n[W->to].layer; k++) grow[k] = 0.1f;
        }
        if (!short_of || tries == 12) break;
        for (int k = 0; k < LG_ROWS; k++) lay.grow[k] += grow[k];
    }
    marks(L);
}

/* several first orders (the text's, reversed, shuffled), each swept to its best; kept: the clear drawing with the
 * fewest crossings, then the shortest wires */
#define STARTS 40
int lg_route(lg_level *L)
{
    static int first[LG_ROWS][IDS], best[LG_ROWS][IDS];
    build(L);
    memcpy(first, lay.ord, sizeof first);
    float best_cost = 1e30f;
    uint32_t r = 12345;
    for (int c = 0; c < STARTS; c++) {
        memcpy(lay.ord, first, sizeof first);
        for (int k = 0; k < lay.nl; k++) {
            int n = lay.cnt[k], *o = lay.ord[k];
            for (int i = 0; c == 1 && i < n / 2; i++) {
                int t = o[i];
                o[i] = o[n - 1 - i];
                o[n - 1 - i] = t;
            }
            for (int i = n - 1; c > 1 && i > 0; i--) {
                r = r * 1103515245u + 12345u;
                int j = (int)((r >> 16) % (uint32_t)(i + 1)), t = o[i];
                o[i] = o[j];
                o[j] = t;
            }
            for (int i = 0; i < n; i++) lay.pos[k][o[i]] = (float)i;
        }
        order_rows();
        finish(L);
        float cost = L->crossings * 1000.0f;
        for (int w = 0; w < L->nw; w++) cost += L->w[w].len;
        if (!lg_check(L, NULL, 0)) cost += 1e6f;
        if (cost < best_cost) {
            best_cost = cost;
            memcpy(best, lay.ord, sizeof best);
        }
    }
    memcpy(lay.ord, best, sizeof best);
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) lay.pos[k][lay.ord[k][i]] = (float)i;
    finish(L);
    return lg_check(L, NULL, 0);
}

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* ---- the values -------------------------------------------------------------------------------------------------- */
int lg_eval(const lg_level *L, const uint8_t *val, const uint8_t *cut, uint8_t out[][2], uint8_t *wsrc,
            uint8_t *wend)
{
    int ok = 1;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        uint8_t in[3] = {0, 0, 0};
        for (int s = 0; s < n->nin; s++) {
            int w = n->in[s];
            const lg_wire *W = &L->w[w];
            uint8_t v = out[W->from][W->port == 2];
            if (wsrc) wsrc[w] = v;
            if (cut[w]) v = 0;
            if (wend) wend[w] = v;
            in[s] = v;
        }
        out[i][1] = 0;
        switch (n->kind) {
        case LG_SWITCH: out[i][0] = val[i] != 0; break;
        case LG_ROUTE:
            out[i][0] = !val[i] && in[0];
            out[i][1] = val[i] && in[0];
            break;
        case LG_LAMP: out[i][0] = in[0]; ok &= in[0]; break;
        case LG_ALARM: out[i][0] = in[0]; ok &= !in[0]; break;
        default: out[i][0] = (uint8_t)lg_gate(n->kind, in, n->nin);
        }
    }
    return ok;
}

uint32_t lg_mask(const lg_level *L, const uint8_t *val, const uint8_t *cut)
{
    uint32_t m = 0;
    for (int v = 0; v < L->nvar; v++) {
        int i = L->var_idx[v], on = L->var_kind[v] == LG_MSWITCH ? val[L->sw[i]]
                                    : L->var_kind[v] == LG_MROUTE ? val[L->rt[i]] : cut[L->cut[i]];
        if (on) m |= 1u << v;
    }
    return m;
}

static void apply(const lg_level *L, uint32_t m, uint8_t *val, uint8_t *cut)
{
    for (int v = 0; v < L->nvar; v++) {
        int i = L->var_idx[v], on = m >> v & 1;
        if (L->var_kind[v] == LG_MSWITCH) val[L->sw[i]] = (uint8_t)on;
        else if (L->var_kind[v] == LG_MROUTE) val[L->rt[i]] = (uint8_t)on;
        else cut[L->cut[i]] = (uint8_t)on;
    }
}

static int bits(uint32_t m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

void lg_solve(const lg_level *L, const uint8_t *val, const uint8_t *cut, lg_answer *a)
{
    uint8_t v[LG_NODES], c[LG_WIRES], out[LG_NODES][2];
    memcpy(v, val, sizeof v);
    memcpy(c, cut, sizeof c);
    uint32_t base = lg_mask(L, val, cut), cuts = 0;
    for (int i = 0; i < L->nvar; i++)
        if (L->var_kind[i] == LG_MCUT) cuts |= 1u << i;
    uint32_t must = base & cuts;
    a->par = -1;
    a->nsol = a->nconf = 0;
    a->best = 0;
    for (uint32_t m = 0; m < 1u << L->nvar; m++) {
        if ((m & must) != must || bits(m & cuts) > L->cut_limit) continue;
        apply(L, m, v, c);
        a->nconf++;
        if (!lg_eval(L, v, c, out, NULL, NULL)) continue;
        a->nsol++;
        int d = bits(m ^ base);
        if (a->par < 0 || d < a->par) { a->par = d; a->best = m; }
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void settle(lg_game *g) { lg_eval(&g->L, g->val, g->cut, g->out, g->wsrc, g->wend); }

void lg_restart(lg_game *g)
{
    for (int i = 0; i < g->L.nn; i++) g->val[i] = g->L.n[i].init;
    memset(g->cut, 0, sizeof g->cut);
    g->nh = 0;
    g->hinted = 0;
    settle(g);
}

void lg_load(lg_game *g, const lg_level *L, int level, int par)
{
    if (&g->L != L) g->L = *L;
    g->level = level;
    g->par = par;
    lg_restart(g);
}

int lg_open(lg_game *g, int level)
{
    if (level < 0 || level >= LG_LEVELS) return 0;
    static lg_level L;
    if (!lg_parse(lg_levels[level], &L, NULL, 0)) return 0;
    lg_load(g, &L, level, lg_pars[level]);
    return 1;
}

int lg_solved(const lg_game *g)
{
    for (int i = 0; i < g->L.nn; i++) {
        int k = g->L.n[i].kind;
        if ((k == LG_LAMP && !g->out[i][0]) || (k == LG_ALARM && g->out[i][0])) return 0;
    }
    return 1;
}

int lg_cuts(const lg_game *g)
{
    int n = 0;
    for (int i = 0; i < g->L.ncut; i++) n += g->cut[g->L.cut[i]];
    return n;
}

static int flip(lg_game *g, lg_move m, int undoing)
{
    const lg_level *L = &g->L;
    switch (m.kind) {
    case LG_MSWITCH:
        if (m.idx >= L->nsw || L->n[L->sw[m.idx]].locked) return 0;
        g->val[L->sw[m.idx]] ^= 1;
        return 1;
    case LG_MROUTE:
        if (m.idx >= L->nrt) return 0;
        g->val[L->rt[m.idx]] ^= 1;
        return 1;
    case LG_MCUT:
        if (m.idx >= L->ncut) return 0;
        if (undoing) { g->cut[L->cut[m.idx]] = 0; return 1; }
        if (g->cut[L->cut[m.idx]] || lg_cuts(g) >= L->cut_limit) return 0;
        g->cut[L->cut[m.idx]] = 1;
        return 1;
    }
    return 0;
}

int lg_play(lg_game *g, lg_move m)
{
    if (lg_solved(g) || g->nh >= LG_HIST || !flip(g, m, 0)) return 0;
    g->hist[g->nh++] = m;
    settle(g);
    return 1;
}

int lg_undo(lg_game *g)
{
    if (!g->nh) return 0;
    flip(g, g->hist[--g->nh], 1);
    settle(g);
    return 1;
}

int lg_stars(const lg_game *g)
{
    int s = g->nh <= g->par ? 3 : g->nh <= g->par + 2 ? 2 : 1;
    return g->hinted && s > 2 ? 2 : s;
}

int lg_hint(const lg_game *g, lg_move *m)
{
    lg_answer a;
    lg_solve(&g->L, g->val, g->cut, &a);
    if (a.par <= 0) return 0;
    uint32_t diff = a.best ^ lg_mask(&g->L, g->val, g->cut);
    for (int v = 0; v < g->L.nvar; v++)
        if (diff >> v & 1) {
            m->kind = g->L.var_kind[v];
            m->idx = g->L.var_idx[v];
            return 1;
        }
    return 0;
}

int lg_script(lg_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        char c = *s++;
        if (c == 'u') { if (!lg_undo(g)) return 0; continue; }
        if (c == 'x') { lg_restart(g); continue; }
        if (c == 'h' || c == '=') {
            lg_move m;
            int once = c == 'h';
            if (once) g->hinted = 1;
            do {
                if (!lg_hint(g, &m) || !lg_play(g, m)) return 0;
            } while (!once && !lg_solved(g));
            continue;
        }
        char *end;
        long n = strtol(s, &end, 10);
        if (end == s || n < 1) return 0;
        s = end;
        lg_move m = {c == 's' ? LG_MSWITCH : c == 'r' ? LG_MROUTE : LG_MCUT, (uint8_t)(n - 1)};
        if ((c != 's' && c != 'r' && c != 'c') || !lg_play(g, m)) return 0;
    }
    return 1;
}

/* ---- the generator ----------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, col, row, nin, alive, init, locked;
    int src[3], port[3], cut[3];
    int used[2];
} gnode;

static uint32_t next(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}
static int rn(uint32_t *s, int n) { return n > 0 ? (int)(next(s) % (uint32_t)n) : 0; }

/* the trunk of (node, port) can rise to the gap under row r: nothing on its column in between */
static int clear_up(gnode *N, int occ[LG_COLS][LG_ROWS], int f, int r)
{
    for (int rr = N[f].row + 1; rr < r; rr++)
        if (occ[N[f].col][rr] >= 0) return 0;
    return 1;
}

static float gx(const gnode *N, int f, int port) { return N[f].col + 0.5f + out_off(port); }

int lg_generate(uint32_t seed, const lg_params *p, char *out, int size)
{
    uint32_t s = mix(seed * 2654435761u + 0x9e3779b9u) | 1u;
    for (int i = 0; i < 4; i++) next(&s);
    gnode N[LG_NODES];
    int nn = 0, occ[LG_COLS][LG_ROWS], cols = p->cols, rows = p->rows;
    if (cols > LG_COLS || rows > LG_ROWS || rows < 3 || p->sw < 1) return 0;
    memset(occ, -1, sizeof occ);
    memset(N, 0, sizeof N);
    /* the switches, spread over the bottom row */
    int colsw[LG_COLS], nsw = p->sw < cols ? p->sw : cols;
    for (int c = 0; c < cols; c++) colsw[c] = c < nsw;
    for (int c = cols - 1; c > 0; c--) {
        int k = rn(&s, c + 1), t = colsw[c];
        colsw[c] = colsw[k];
        colsw[k] = t;
    }
    for (int c = 0; c < cols; c++)
        if (colsw[c]) {
            N[nn] = (gnode){.kind = LG_SWITCH, .col = c, .row = 0, .alive = 1, .init = rn(&s, 2)};
            occ[c][0] = nn++;
        }
    int routes = p->routes, need = p->lamps + p->alarm, kinds[LG_KINDS], nk = 0;
    for (int k = LG_NOT; k <= LG_NOR; k++)
        if (p->kinds >> k & 1) kinds[nk++] = k;
    if (!nk) return 0;
    for (int r = 1; r < rows - 1; r++) {
        int left = rows - 1 - r, placed = 0;
        for (int tries = 0; tries < 40 && placed < p->gates && nn < LG_NODES - 3; tries++) {
            int unused = 0;
            for (int i = 0; i < nn; i++)
                if (N[i].alive && N[i].kind != LG_LAMP)
                    for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++) unused += !N[i].used[q];
            int target = need + (unused - need) * (left - 1) / left;
            if (unused <= target && placed) break;
            int kind, nin;
            if (routes > 0 && rn(&s, 100) < 45) kind = LG_ROUTE, nin = 1;
            else {
                kind = kinds[rn(&s, nk)];
                if (kind == LG_NOT && rn(&s, 2)) kind = kinds[rn(&s, nk)];   /* fewer NOTs */
                nin = kind == LG_NOT ? 1 : rn(&s, 100) < p->three ? 3 : 2;
            }
            /* the first input: an unused output, the lowest first */
            int cf[LG_NODES * 2], cp[LG_NODES * 2], nc = 0, low = 99;
            for (int i = 0; i < nn; i++)
                for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++)
                    if (N[i].alive && N[i].row < r && !N[i].used[q] && clear_up(N, occ, i, r)) {
                        if (N[i].row < low) { low = N[i].row; nc = 0; }
                        if (N[i].row == low) { cf[nc] = i; cp[nc++] = q; }
                    }
            if (!nc) break;
            int pick = rn(&s, nc), src[3] = {cf[pick]}, port[3] = {N[cf[pick]].kind == LG_ROUTE ? cp[pick] + 1 : 0};
            if (kind == LG_NOT && N[src[0]].kind == LG_NOT) continue;   /* NOT NOT: nothing */
            float x0 = gx(N, src[0], port[0]);
            int ok = 1;
            for (int k = 1; k < nin && ok; k++) {   /* the others: near it; unused, or (fan) any */
                int fan = rn(&s, 100) < p->fan;
                nc = 0;
                for (int i = 0; i < nn; i++)
                    for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++) {
                        int pq = N[i].kind == LG_ROUTE ? q + 1 : 0, dup = 0;
                        for (int j = 0; j < k; j++) dup |= src[j] == i;   /* two inputs from one part: dull */
                        if (!N[i].alive || N[i].kind == LG_LAMP || N[i].row >= r || dup) continue;
                        if (fabsf(gx(N, i, pq) - x0) > 2.3f || !clear_up(N, occ, i, r)) continue;
                        if (N[i].used[q] && !fan) continue;
                        cf[nc] = i;
                        cp[nc++] = pq;
                    }
                if (!nc) { ok = 0; break; }
                pick = rn(&s, nc);
                src[k] = cf[pick];
                port[k] = cp[pick];
            }
            if (!ok) continue;
            /* sort the inputs from the left, so the wires need not cross */
            for (int a = 0; a < nin; a++)
                for (int b = a + 1; b < nin; b++)
                    if (gx(N, src[b], port[b]) < gx(N, src[a], port[a])) {
                        int t = src[a]; src[a] = src[b]; src[b] = t;
                        t = port[a]; port[a] = port[b]; port[b] = t;
                    }
            float mx = 0;
            for (int k = 0; k < nin; k++) mx += gx(N, src[k], port[k]);
            int c0 = (int)floorf(mx / nin), col = -1;
            for (int d = 0; d < 3 && col < 0; d++) {
                int a = c0 - d, b = c0 + d, first = rn(&s, 2) ? a : b, second = first == a ? b : a;
                if (first >= 0 && first < cols && occ[first][r] < 0) col = first;
                else if (second >= 0 && second < cols && occ[second][r] < 0) col = second;
            }
            if (col < 0) continue;
            gnode *G = &N[nn];
            *G = (gnode){.kind = kind, .col = col, .row = r, .nin = nin, .alive = 1, .init = rn(&s, 2)};
            for (int k = 0; k < nin; k++) {
                G->src[k] = src[k];
                G->port[k] = port[k];
                N[src[k]].used[port[k] ? port[k] - 1 : 0]++;
            }
            occ[col][r] = nn++;
            placed++;
            if (kind == LG_ROUTE) routes--;
        }
    }
    /* the lamps: on the top row, each fed by an unused output (the highest first) */
    int fed[LG_NODES * 2] = {0};                /* a signal lights one lamp at most */
    for (int l = 0; l < need; l++) {
        int bf = -1, bq = 0, br = -1;
        for (int pass = 0; pass < 2 && bf < 0; pass++)
            for (int i = 0; i < nn; i++)
                for (int q = 0; q < (N[i].kind == LG_ROUTE ? 2 : 1); q++)
                    if (N[i].alive && N[i].kind != LG_LAMP && N[i].kind != LG_ALARM && N[i].kind != LG_SWITCH &&
                        !fed[2 * i + q] && (pass || !N[i].used[q]) && clear_up(N, occ, i, rows - 1) &&
                        (N[i].row > br || (N[i].row == br && rn(&s, 2)))) {
                        bf = i;
                        bq = q;
                        br = N[i].row;
                    }
        if (bf < 0) return 0;
        fed[2 * bf + bq] = 1;
        int c0 = N[bf].col, col = -1;
        for (int d = 0; d < 3 && col < 0; d++)
            for (int sg = -1; sg <= 1 && col < 0; sg += 2)
                if (c0 + sg * d >= 0 && c0 + sg * d < cols && occ[c0 + sg * d][rows - 1] < 0) col = c0 + sg * d;
        if (col < 0) return 0;
        int port = N[bf].kind == LG_ROUTE ? bq + 1 : 0;
        N[nn] = (gnode){.kind = l < p->lamps ? LG_LAMP : LG_ALARM, .col = col, .row = rows - 1, .nin = 1,
                        .alive = 1, .src = {bf}, .port = {port}};
        N[bf].used[bq]++;
        occ[col][rows - 1] = nn++;
    }
    /* what feeds nothing goes */
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < nn; i++) {
            gnode *G = &N[i];
            if (!G->alive || G->kind == LG_LAMP || G->kind == LG_ALARM) continue;
            int np = G->kind == LG_ROUTE ? 2 : 1, idle = 0;
            for (int q = 0; q < np; q++) idle += !G->used[q];
            if (!idle) continue;
            if (G->kind == LG_ROUTE && idle == 1) return 0;
            G->alive = 0;
            occ[G->col][G->row] = -1;
            for (int k = 0; k < G->nin; k++) N[G->src[k]].used[G->port[k] ? G->port[k] - 1 : 0]--;
            changed = 1;
        }
    }
    int alive_sw = 0, gates = 0;
    for (int i = 0; i < nn; i++) {
        alive_sw += N[i].alive && N[i].kind == LG_SWITCH;
        gates += N[i].alive && N[i].kind >= LG_NOT && N[i].kind <= LG_ROUTE;
    }
    if (alive_sw < 2 || !gates) return 0;
    /* empty columns and rows closed up */
    for (int c = cols - 1; c >= 0; c--) {
        int any = 0;
        for (int i = 0; i < nn; i++) any |= N[i].alive && N[i].col == c;
        if (!any)
            for (int i = 0; i < nn; i++)
                if (N[i].col > c) N[i].col--;
    }
    for (int r = rows - 2; r >= 1; r--) {
        int any = 0;
        for (int i = 0; i < nn; i++) any |= N[i].alive && N[i].row == r;
        if (!any)
            for (int i = 0; i < nn; i++)
                if (N[i].row > r) N[i].row--;
    }
    /* locked switches, fuses */
    int lock = p->locked < alive_sw - 2 ? p->locked : alive_sw - 2;
    for (int t = 0; t < 20 && lock > 0; t++) {
        int i = rn(&s, nn);
        if (N[i].alive && N[i].kind == LG_SWITCH && !N[i].locked) { N[i].locked = 1; lock--; }
    }
    int fuses = p->fuses;
    for (int t = 0; t < 60 && fuses > 0; t++) {
        int i = rn(&s, nn);
        if (!N[i].alive || !N[i].nin || N[i].kind == LG_LAMP) continue;   /* a lamp's own wire: no use cutting */
        int k = rn(&s, N[i].nin);
        if (!N[i].cut[k]) { N[i].cut[k] = 1; fuses--; }
    }
    /* the text */
    int len = 0;
    if (p->fuses >= 2 && rn(&s, 3) == 0)          /* fewer cuts than fuses, now and then */
        len += snprintf(out + len, (size_t)(size - len), "%%%d", 1 + rn(&s, p->fuses - 1));
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            for (int i = 0; i < nn; i++) {
                gnode *G = &N[i];
                if (!G->alive || G->col != c || G->row != r) continue;
                const char *name = G->kind == LG_SWITCH ? (G->locked ? (G->init ? "K" : "k") : G->init ? "S" : "s")
                                 : G->kind == LG_ROUTE ? (G->init ? "Y" : "y") : KIND_NAME[G->kind];
                len += snprintf(out + len, (size_t)(size > len ? size - len : 0), "%s%s@%c%d", len ? " " : "", name,
                                'a' + c, r + 1);
                for (int k = 0; k < G->nin; k++) {
                    gnode *F = &N[G->src[k]];
                    len += snprintf(out + len, (size_t)(size > len ? size - len : 0), "%c%c%d%s%s", k ? ',' : '(',
                                    'a' + F->col, F->row + 1, G->port[k] == 1 ? "<" : G->port[k] == 2 ? ">" : "",
                                    G->cut[k] ? "x" : "");
                }
                if (G->nin) len += snprintf(out + len, (size_t)(size > len ? size - len : 0), ")");
            }
    return len < size;
}
