/*
 * Logic Gates: the rules (logicgates.h): the levels read from their text, the drawing's tracks, the values, the
 * solver (every setting tried), the moves and undo, the hint, the notation; every setting at once (a bit each) to
 * rate a level and tell two apart; the generator that makes the levels (tools/logicgates_gen.c) and the die tab's
 * puzzles.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/logicgates.h"
#include "games/logicgates_levels.h"
#include "games/puzrand.h"

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

/* the level from its text; its drawing too when `draw` */
static int stored_layout(lg_level *L, const char *s);
static int parse(const char *src, lg_level *L, char *err, int errsize, int draw)
{
    static lg_tok T[LG_NODES];
    int nt = 0, limit = -1;
    const char *stored = NULL;
    memset(L, 0, sizeof *L);
    for (const char *s = src; *s;) {
        if (*s == ' ') { s++; continue; }
        if (*s == '|') { stored = s + 1; break; }   /* the layout (lg_layout_text) */
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
    if (draw && !(stored && stored_layout(L, stored) ? lg_check(L, NULL, 0) : lg_route_fast(L))) {
        lg_check(L, err, errsize);
        return 0;
    }
    return 1;
}

int lg_parse(const char *src, lg_level *L, char *err, int errsize) { return parse(src, L, err, errsize, 1); }

/* ---- the drawing ------------------------------------------------------------------------------------------------- */
/* Laid out from the circuit alone (the cells in the text only name the parts).
 * - The rows: the lamps on top; every other part as high as it can go, just under the lowest part it feeds: a switch
 *   sits right under its gate, at any height, and a part feeding a high gate does not drag a long wire up from the
 *   bottom. The board is as wide as its widest row, not as its switches. (The levels' tool also tries other rows for
 *   each part, within what its wires allow, for a narrower or clearer drawing.) A wire rising past a row is an item of
 *   that row too, a trunk (one for the wires of a signal; a cuttable wire has its own).
 * - Each row's order: the fewest crossings, by an exact search of the orders (rows of up to `exact` items; else swaps
 *   of neighbours), swept up and down the rows until nothing improves, from several first orders. A route switch's
 *   left way stays on its left.
 * - The places: each row packed; then, a few times over, row by row upwards each part centred over its inputs and a
 *   trunk over its source, and row by row downwards each switch under the pins it feeds; a row pushed apart only as
 *   far as it must be. A gate's input lies straight over its source when it can.
 * - Each gap between rows: one track for each signal going sideways there, the tracks ordered for the fewest
 *   crossings, those not side by side sharing a height, evenly spaced; the gap grows with its tracks (and when a
 *   cable needs room). A wire rises from its source, runs along its track, and rises into its pin.
 * - A crossing left gets a hop (on the wire running sideways), a branching a dot.
 * - A cuttable wire: on its longest straight stretch of its own, two terminals, a jumper cable between them. */
static float in_off(int nin, int slot)          /* an input's place under a part, columns from its middle (at first) */
{
    return nin == 1 ? 0 : nin == 2 ? (slot ? 0.18f : -0.18f) : (slot - 1) * 0.24f;
}
static float out_off(int port) { return port == 1 ? -0.18f : port == 2 ? 0.18f : 0; }

#define EPS 1e-4f
static float clampf01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
#define IDS (LG_NODES + LG_WIRES)               /* an item: a part (its index), or LG_NODES + a trunk */
#define EXACT 10                                /* rows ordered by an exact search: up to this many items (at most) */
#define SEP_PART 1.0f                           /* side by side, their middles at least (columns): two parts */
#define SEP_SIDE 0.55f                          /* a part and a trunk */
#define SEP_PASS 0.4f                           /* two trunks */
#define PIN_MAX 0.22f                           /* a gate's inputs: at most this far from its middle */
#define PIN_GAP 0.16f                           /* and this far apart */
#define SLACK 0.08f                             /* the room given up to keep a wire straight */
#define NOT_MAX 0.12f                           /* a NOT's input: this far */
#define GAP_MIN 0.42f                           /* a gap between rows, at least (rows) */
#define TRACK 0.14f                             /* its tracks at least this far apart */
#define BESIDE 0.15f                            /* two runs closer than this sideways need two heights */
#define TOUCH 0.06f                             /* a rise this near a run's end would seem to join it */
#define CABLE_ROOM 0.75f                        /* a cuttable wire's straight stretch of its own, at least */
#define BIG 1000
#define PLACE_ROUNDS 4

static int exact_rows = EXACT, exact_tracks = 12;   /* the searches' sizes (smaller in the fast search) */

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

static int is_lamp(const lg_level *L, int i) { return L->n[i].kind == LG_LAMP || L->n[i].kind == LG_ALARM; }

/* each part's row: the lamps on top, every other part just under the lowest part it feeds (the inputs come first in
 * the parts' order, so from the last part down) */
static void rows_high(lg_level *L)
{
    int top = 0;
    for (int i = 0; i < L->nn; i++)
        if (!is_lamp(L, i) && L->depth[i] > top) top = L->depth[i];
    top++;
    for (int i = L->nn - 1; i >= 0; i--) {
        int m = top;
        if (!is_lamp(L, i))
            for (int w = 0; w < L->nw; w++)
                if (L->w[w].from == i && L->n[L->w[w].to].layer - 1 < m) m = L->n[L->w[w].to].layer - 1;
        L->n[i].layer = (uint8_t)m;
    }
}

/* the rows as set: every wire going up, the lamps alone on the top row, the bottom row used, at most LG_ROWS rows */
static int rows_ok(const lg_level *L)
{
    int top = 0, low = LG_ROWS;
    for (int i = 0; i < L->nn; i++) {
        if (L->n[i].layer > top) top = L->n[i].layer;
        if (L->n[i].layer < low) low = L->n[i].layer;
    }
    if (low != 0 || top >= LG_ROWS) return 0;
    for (int i = 0; i < L->nn; i++)
        if (is_lamp(L, i) != (L->n[i].layer == top)) return 0;
    for (int w = 0; w < L->nw; w++)
        if (L->n[L->w[w].from].layer >= L->n[L->w[w].to].layer) return 0;
    return 1;
}

/* the rows (the parts' layers, set), the trunks, the edges in each gap */
static void build(lg_level *L)
{
    memset(&lay, 0, sizeof lay);
    int top = 0;
    for (int i = 0; i < L->nn; i++)
        if (L->n[i].layer > top) top = L->n[i].layer;
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
    if (n <= exact_rows) {                      /* every order: the best of each set of leftmost items */
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

static const lg_level *sepL;                    /* the level being placed (its parts' widths) */
static float sep(int a, int b)
{
    float ha = is_part(a) ? lg_half(sepL->n[a].kind) : 0, hb = is_part(b) ? lg_half(sepL->n[b].kind) : 0;
    return is_part(a) && is_part(b) ? ha + hb + SEP_PART - 2 * LG_HALF
         : is_part(a) || is_part(b) ? ha + hb + SEP_SIDE - LG_HALF : SEP_PASS;
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

/* where an edge of the gap above row k leaves its item, and (roughly: the pins are not set yet) where it enters its
 * item above: a gate's inputs spread under it in the order of their sources */
static float src_x(int k, const lg_edge *e) { return lay.x[k][e->u] + (is_part(e->u) ? out_off(e->port) : 0); }
static float dst_x(int k, const lg_edge *e)
{
    float x = lay.x[k + 1][e->v];
    if (!is_part(e->v)) return x;
    int n = 0, r = 0;
    float me = src_x(k, e);
    for (int j = 0; j < lay.ne[k]; j++) {
        const lg_edge *f = &lay.e[k][j];
        if (f->v != e->v) continue;
        float o = src_x(k, f);
        n++;
        r += o < me - EPS || (fabsf(o - me) <= EPS && f < e);
    }
    return x + in_off(n > 3 ? 3 : n, r);
}

/* row k's items as near d (weights w) as their order and their room allow; straight: a part pushed a hair off its
 * place goes back to it if its neighbours stay nearly as far */
static void row_place(int k, const float *d, const float *w, int straight)
{
    int n = lay.cnt[k];
    float s[IDS], r[IDS];
    for (int i = 0; i + 1 < n; i++) s[i] = sep(lay.ord[k][i], lay.ord[k][i + 1]);
    spread(n, d, w, s, r);
    for (int i = 0; straight && i < n; i++)
        if (fabsf(r[i] - d[i]) < 0.12f && (i == 0 || d[i] - r[i - 1] >= s[i - 1] - SLACK) &&
            (i == n - 1 || r[i + 1] - d[i] >= s[i] - SLACK))
            r[i] = d[i];
    for (int i = 0; i < n; i++) lay.x[k][lay.ord[k][i]] = r[i];
}

/* up: each item over its inputs (one without inputs where it is, lightly); down: each item without inputs (a
 * switch) under the pins it feeds, the others where they are */
static void place_up(int k, int straight)
{
    float d[IDS], w[IDS];
    for (int i = 0; i < lay.cnt[k]; i++) {
        int id = lay.ord[k][i], m = 0;
        float sum = 0;
        for (int j = 0; j < lay.ne[k - 1]; j++)
            if (lay.e[k - 1][j].v == id) { sum += src_x(k - 1, &lay.e[k - 1][j]); m++; }
        d[i] = m ? sum / m : lay.x[k][id];
        w[i] = !m ? 0.25f : is_part(id) ? 1.0f : 3.0f;
    }
    row_place(k, d, w, straight);
}

static void place_down(int k)
{
    float d[IDS], w[IDS];
    int any = 0;
    for (int i = 0; i < lay.cnt[k]; i++) {
        int id = lay.ord[k][i], fed = 0, m = 0;
        float sum = 0;
        for (int j = 0; k > 0 && j < lay.ne[k - 1] && !fed; j++) fed = lay.e[k - 1][j].v == id;
        if (!fed)
            for (int j = 0; j < lay.ne[k]; j++)
                if (lay.e[k][j].u == id) { sum += dst_x(k, &lay.e[k][j]); m++; }
        d[i] = m ? sum / m : lay.x[k][id];
        w[i] = m ? 1.0f : is_part(id) ? 2.0f : 3.0f;
        any |= m > 0;
    }
    if (any) row_place(k, d, w, 0);
}

static void place(lg_level *L)
{
    sepL = L;
    for (int k = 0; k < lay.nl; k++)            /* each row packed from the left */
        for (int i = 0; i < lay.cnt[k]; i++)
            lay.x[k][lay.ord[k][i]] = i ? lay.x[k][lay.ord[k][i - 1]] + sep(lay.ord[k][i - 1], lay.ord[k][i]) : 0;
    for (int round = 0; round < PLACE_ROUNDS; round++) {
        for (int k = 1; k < lay.nl; k++) place_up(k, 0);
        for (int k = lay.nl - 2; k >= 0; k--) place_down(k);
    }
    for (int k = 1; k < lay.nl; k++) place_up(k, 1);
    /* from half a column in */
    float lo = 1e9f, hi = -1e9f;
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) {
            int id = lay.ord[k][i];
            float m = is_part(id) ? lg_half(L->n[id].kind) + 0.1f : 0.3f;
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
    if (m <= exact_tracks) {
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
            for (int k = 0; k < L->nhop; k++)
                dup |= fabsf(L->hop[k].x - v->c) < 1e-3f && fabsf(L->hop[k].y - h->c) < 1e-3f;
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
            if (fabsf(A->x - B->x) < lg_half(A->kind) + lg_half(B->kind) + 0.1f && A->y < B->y + LG_NH + 0.05f &&
                B->y < A->y + LG_NH + 0.05f)
                return fail(err, size, "parts %d and %d overlap", i, j);
        }
    for (int i = 0; i < n; i++) {
        const lg_seg *s = &S[i];
        for (int k = 0; k < L->nn; k++) {       /* no wire through a part */
            const lg_node *N = &L->n[k];
            if (k == L->w[s->wire].from || k == L->w[s->wire].to) continue;
            float hw = lg_half(N->kind), x0 = N->x - hw - 0.03f, x1 = N->x + hw + 0.03f, y0 = N->y - 0.02f,
                  y1 = N->y + LG_NH + 0.02f;
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
        if (L->w[w].cuttable && fuse_spot(L, w, &fs, &fx, &fy, &fv) < CABLE_ROOM - 0.02f)
            return fail(err, size, "no room for the cable on wire %d", w);
    }
    return 1;
}

static void point_at(const lg_wire *W, float s, float *x, float *y)   /* the point s along a wire */
{
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        float len = fabsf(x1 - x0) + fabsf(y1 - y0);
        if (s <= len || i + 2 == W->npt) {
            float f = len > EPS ? clampf01(s / len) : 0;
            *x = x0 + (x1 - x0) * f;
            *y = y0 + (y1 - y0) * f;
            return;
        }
        s -= len;
    }
    *x = W->px[0];
    *y = W->py[0];
}

/* the room beside a cable's stretch (a line at c across lo..hi; vert: x = c) on one side (+1: right, or above): to
 * the nearest other wire or part, 1 at most */
static float room_beside(const lg_level *L, int w, int vert, float c, float lo, float hi, int side)
{
    static lg_seg S[LG_WIRES * LG_PTS];
    int n = segments(L, S);
    float best = 1;
    for (int i = 0; i < n; i++) {
        const lg_seg *t = &S[i];
        if (t->wire == w) continue;
        float d = -1;
        if (t->vert == vert) {
            if (t->a < hi && t->b > lo) d = (t->c - c) * side;
        } else if (t->c > lo - 0.05f && t->c < hi + 0.05f) {
            d = side > 0 ? t->a - c : c - t->b;     /* a run reaching towards the line */
            if (d < 0 && (side > 0 ? t->b > c : t->a < c)) d = 0.02f;
        }
        if (d > 0.01f && d < best) best = d;
    }
    for (int k = 0; k < L->nn; k++) {
        const lg_node *N = &L->n[k];
        float hw = lg_half(N->kind), a = vert ? N->y : N->x - hw, b = vert ? N->y + LG_NH : N->x + hw;
        float p0 = vert ? N->x - hw : N->y, p1 = vert ? N->x + hw : N->y + LG_NH;
        if (a >= hi || b <= lo) continue;
        float d = side > 0 ? p0 - c : c - p1;
        if (d > 0.01f && d < best) best = d;
    }
    return best;
}

/* a cuttable wire's cable: its terminals on its free stretch (fuse_spot), bowing to the roomier side */
static void cable(lg_level *L, int w)
{
    lg_wire *W = &L->w[w];
    float room = fuse_spot(L, w, &W->fs, &W->fx, &W->fy, &W->fvert);
    float h = room / 2 - 0.05f;
    h = h < 0.12f ? 0.12f : h > 0.42f ? 0.42f : h;
    W->ta = W->fs - h;
    W->tb = W->fs + h;
    point_at(W, W->ta, &W->tax, &W->tay);
    point_at(W, W->tb, &W->tbx, &W->tby);
    int v = W->fvert;
    float c = v ? W->fx : W->fy, lo = (v ? W->fy : W->fx) - h, hi = (v ? W->fy : W->fx) + h;
    float r1 = room_beside(L, w, v, c, lo, hi, 1), r0 = room_beside(L, w, v, c, lo, hi, -1);
    float r = r1 >= r0 - 0.02f ? r1 : r0, b = fminf(0.17f, 0.45f * r);
    W->bow = r1 >= r0 - 0.02f ? b : -b;
}

/* from the rows' order: the places, the tracks, the paths (the gaps grown for cables without room), the hops and
 * dots, the cables */
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
            if (!W->cuttable || fuse_spot(L, w, &W->fs, &W->fx, &W->fy, &W->fvert) >= CABLE_ROOM) continue;
            short_of = 1;
            for (int k = L->n[W->from].layer; k < L->n[W->to].layer; k++) grow[k] = 0.1f;
        }
        if (!short_of || tries == 20) break;
        for (int k = 0; k < LG_ROWS; k++) lay.grow[k] += grow[k];
    }
    marks(L);
    for (int w = 0; w < L->nw; w++)
        if (L->w[w].cuttable) cable(L, w);
}

/* ---- the searches ------------------------------------------------------------------------------------------------ */
float lg_part_size(const lg_level *L, float rw, float rh, float *cw, float *ch)
{
    float span = L->height + LG_PAD_B + LG_PAD_T, cols = L->width > 1 ? L->width : 1;
    float a = fminf(fminf(rw / cols, fminf(rw, rh) / 3.2f), 1.3f * rh / span);   /* a column; a row: */
    float b = fminf(rh / span, 1.7f * a);
    if (cw) *cw = a;
    if (ch) *ch = b;
    return fminf(0.75f * a, fmaxf(0.6f * a, 0.55f * b));
}

/* a drawing's cost: unclear, crossings, small parts (on a phone held upright, and lying), long wires */
static float cost_of(const lg_level *L)
{
    float len = 0;
    for (int w = 0; w < L->nw; w++) len += L->w[w].len;
    float p = lg_part_size(L, 1000, 1600, NULL, NULL) / 1000,
          l = fmaxf(lg_part_size(L, 1700, 850, NULL, NULL), lg_part_size(L, 850, 1700, NULL, NULL)) / 1000;
    return L->crossings * 400.0f + len * 0.3f - 2000 * (p + 0.5f * l) + (lg_check(L, NULL, 0) ? 0 : 1e6f);
}

/* the layout as text (lg_layout_text): the parts' rows, the rows' items */
static void remember(lg_level *L)
{
    char *o = L->layout;
    int n = 0;
    o[n++] = ' ';
    o[n++] = '|';
    for (int i = 0; i < L->nn; i++) o[n++] = (char)('0' + L->n[i].layer);
    o[n++] = '/';
    for (int k = 0; k < lay.nl; k++) {
        if (k) o[n++] = '.';
        for (int i = 0; i < lay.cnt[k]; i++) {
            int id = lay.ord[k][i];
            if (n >= LG_LAYOUT - 2 || (!is_part(id) && id - LG_NODES >= 26)) { o[0] = 0; return; }
            o[n++] = (char)(is_part(id) ? '0' + id : 'a' + id - LG_NODES);
        }
    }
    o[n] = 0;
}

int lg_layout_text(const lg_level *L, char *out, int size)
{
    if (!L->layout[0] || (int)strlen(L->layout) >= size) return 0;
    strcpy(out, L->layout);
    return 1;
}

/* the layout stored after the circuit's text (s: past its '|'); 0 when it does not fit the circuit */
static int stored_layout(lg_level *L, const char *s)
{
    for (int i = 0; i < L->nn; i++) {
        if (*s < '0' || *s >= '0' + LG_ROWS) return 0;
        L->n[i].layer = (uint8_t)(*s++ - '0');
    }
    if (*s++ != '/' || !rows_ok(L)) return 0;
    build(L);
    for (int k = 0; k < lay.nl; k++) {
        int got[IDS], c = 0;
        static uint8_t seen[IDS];
        memset(seen, 0, sizeof seen);
        while (*s && *s != '.' && *s != ' ') {
            int ch = *s++, id = ch >= 'a' ? LG_NODES + ch - 'a' : ch - '0', in = 0;
            if (id < 0 || id >= IDS || seen[id] || c >= lay.cnt[k]) return 0;
            for (int i = 0; i < lay.cnt[k]; i++) in |= lay.ord[k][i] == id;
            if (!in) return 0;
            seen[id] = 1;
            got[c++] = id;
        }
        if (c != lay.cnt[k]) return 0;
        for (int i = 0; i < c; i++) {
            lay.ord[k][i] = got[i];
            lay.pos[k][got[i]] = (float)i;
        }
        if (*s == '.') s++;
    }
    finish(L);
    remember(L);
    L->stored = 1;
    return 1;
}

/* the rows' orders for the parts' rows as set: several first orders (the text's, reversed, shuffled; `starts` at
 * most, and none started past `until` when it is set), each swept to its best; kept: the cheapest drawing (cost_of).
 * Its cost. */
static float search_orders(lg_level *L, int starts, clock_t until)
{
    static int first[LG_ROWS][IDS], best[LG_ROWS][IDS];
    build(L);
    memcpy(first, lay.ord, sizeof first);
    memcpy(best, lay.ord, sizeof best);
    float best_cost = 1e30f;
    uint32_t r = 12345;
    for (int c = 0; c < starts; c++) {
        if (c > 1 && until && clock() > until) break;
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
        float cost = cost_of(L);
        if (cost < best_cost) {
            best_cost = cost;
            memcpy(best, lay.ord, sizeof best);
        }
    }
    memcpy(lay.ord, best, sizeof best);
    for (int k = 0; k < lay.nl; k++)
        for (int i = 0; i < lay.cnt[k]; i++) lay.pos[k][lay.ord[k][i]] = (float)i;
    finish(L);
    return best_cost;
}

/* the die tab's: the parts as high as they go, a few orders, small exact searches: under a millisecond (the same
 * drawing for the same circuit on any machine; a time limit far above that, should a machine be very slow) */
int lg_route_fast(lg_level *L)
{
    exact_rows = 7;
    exact_tracks = 8;
    rows_high(L);
    search_orders(L, 6, clock() + (clock_t)(CLOCKS_PER_SEC / 40));   /* a safety net: it takes well under 1 ms */
    exact_rows = EXACT;
    exact_tracks = 12;
    remember(L);
    L->stored = 0;
    return lg_check(L, NULL, 0);
}

int lg_route(lg_level *L) { return lg_route_fast(L); }

/* the parts' rows as set, made regular: the lowest row 0, the lamps alone on top */
static void rows_settle(lg_level *L)
{
    int low = LG_ROWS, top = 0;
    for (int i = 0; i < L->nn; i++)
        if (!is_lamp(L, i)) {
            if (L->n[i].layer < low) low = L->n[i].layer;
            if (L->n[i].layer > top) top = L->n[i].layer;
        }
    for (int i = 0; i < L->nn; i++) L->n[i].layer = (uint8_t)(is_lamp(L, i) ? top + 1 - low : L->n[i].layer - low);
}

/* the levels' tool's: from the parts as high as they go, one part at a time moved a row up or down while the drawing
 * gets cheaper (`effort` tries); then its rows' orders from many starts with the full exact searches */
int lg_route_best(lg_level *L, int effort)
{
    uint8_t cur[LG_NODES], best[LG_NODES];
    exact_rows = 8;
    exact_tracks = 10;
    rows_high(L);
    for (int i = 0; i < L->nn; i++) cur[i] = L->n[i].layer;
    float cur_cost = search_orders(L, 6, 0);
    memcpy(best, cur, sizeof best);
    uint32_t r = 0x2545f491u;
    for (int it = 0; it < effort; it++) {
        for (int i = 0; i < L->nn; i++) L->n[i].layer = cur[i];
        r = r * 1103515245u + 12345u;
        int i = (int)((r >> 8) % (uint32_t)L->nn);
        if (is_lamp(L, i)) continue;
        int to = L->n[i].layer + ((r >> 4) & 1 ? 1 : -1);
        if (to < -1 || to >= LG_ROWS - 1) continue;
        if (to < 0)                             /* a row added under all */
            for (int j = 0; j < L->nn; j++) L->n[j].layer++;
        else L->n[i].layer = (uint8_t)to;
        if (to < 0) L->n[i].layer = 0;
        rows_settle(L);
        if (!rows_ok(L)) continue;
        float c = search_orders(L, 6, 0);
        if (c < cur_cost - 0.01f) {
            cur_cost = c;
            for (int j = 0; j < L->nn; j++) cur[j] = L->n[j].layer;
            memcpy(best, cur, sizeof best);
        }
    }
    for (int i = 0; i < L->nn; i++) L->n[i].layer = best[i];
    exact_rows = EXACT;
    exact_tracks = 12;
    search_orders(L, 40, 0);
    remember(L);
    L->stored = 0;
    return lg_check(L, NULL, 0);
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


/* ---- a level as text --------------------------------------------------------------------------------------------- */
static int put(char *out, int size, int len, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + len, (size_t)(size > len ? size - len : 0), fmt, ap);
    va_end(ap);
    return len + (n > 0 ? n : 0);
}

int lg_text(const lg_level *L, char *out, int size)
{
    int len = 0;
    if (size < 1) return 0;
    out[0] = 0;
    if (L->cut_limit < L->ncut) len = put(out, size, len, "%%%d", L->cut_limit);
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        const char *name = N->kind == LG_SWITCH ? (N->locked ? (N->init ? "K" : "k") : N->init ? "S" : "s")
                         : N->kind == LG_ROUTE ? (N->init ? "Y" : "y") : KIND_NAME[N->kind];
        len = put(out, size, len, "%s%s@%c%d", len ? " " : "", name, 'a' + N->col, N->row + 1);
        for (int k = 0; k < N->nin; k++) {
            const lg_wire *W = &L->w[N->in[k]];
            const lg_node *F = &L->n[W->from];
            len = put(out, size, len, "%c%c%d%s%s", k ? ',' : '(', 'a' + F->col, F->row + 1,
                      W->port == 1 ? "<" : W->port == 2 ? ">" : "", W->cuttable ? "x" : "");
        }
        if (N->nin) len = put(out, size, len, ")");
    }
    return len < size;
}

/* ---- every setting at once: a signal's value in each setting of the variables, a bit each --------------------------
 * Setting m (0 .. 2^nvar - 1): variable v (lg_mask's order) is bit v of m. */
#define BV_VARS 14
#define BW (1 << (BV_VARS - 6))                 /* words: 2^14 settings */

static struct {
    int nv, nw;                                 /* the variables; the words used */
    uint64_t last;                              /* the settings in a word (fewer than 64 settings: not all) */
    int vnode[LG_NODES], vwire[LG_WIRES];       /* a switch's or route switch's variable, a fuse's (-1: none) */
    uint32_t cuts;                              /* the fuses' variables */
    uint64_t out[LG_NODES][2][BW];
    uint64_t ok[BW], lit[BW], valid[BW];        /* solved; every lamp lit (alarms or not); within the cuts allowed */
} bv;

static uint64_t var_word(int v, int wi)
{
    static const uint64_t P[6] = {0xAAAAAAAAAAAAAAAAull, 0xCCCCCCCCCCCCCCCCull, 0xF0F0F0F0F0F0F0F0ull,
                                  0xFF00FF00FF00FF00ull, 0xFFFF0000FFFF0000ull, 0xFFFFFFFF00000000ull};
    return v < 6 ? P[v] : (wi >> (v - 6) & 1) ? ~0ull : 0;
}

static int bit(const uint64_t *b, uint32_t m) { return (int)(b[m >> 6] >> (m & 63) & 1); }

static int ctz64(uint64_t x) { return __builtin_ctzll(x); }   /* x not 0 */
static int popc(uint64_t x) { return __builtin_popcountll(x); }

static int bv_setup(const lg_level *L)
{
    if (L->nvar > BV_VARS) return 0;
    bv.nv = L->nvar;
    bv.nw = bv.nv > 6 ? 1 << (bv.nv - 6) : 1;
    bv.last = bv.nv >= 6 ? ~0ull : (1ull << (1 << bv.nv)) - 1;
    memset(bv.vnode, -1, sizeof bv.vnode);
    memset(bv.vwire, -1, sizeof bv.vwire);
    bv.cuts = 0;
    for (int v = 0; v < L->nvar; v++) {
        int i = L->var_idx[v];
        if (L->var_kind[v] == LG_MSWITCH) bv.vnode[L->sw[i]] = v;
        else if (L->var_kind[v] == LG_MROUTE) bv.vnode[L->rt[i]] = v;
        else { bv.vwire[L->cut[i]] = v; bv.cuts |= 1u << v; }
    }
    if (bits(bv.cuts) <= L->cut_limit) {        /* every setting within the cuts allowed */
        for (int k = 0; k < bv.nw; k++) bv.valid[k] = ~0ull;
        return 1;
    }
    memset(bv.valid, 0, sizeof(uint64_t) * (size_t)bv.nw);
    for (uint32_t m = 0; m < 1u << bv.nv; m++)
        if (bits(m & bv.cuts) <= L->cut_limit) bv.valid[m >> 6] |= 1ull << (m & 63);
    return 1;
}

/* the values in every setting; `force`: a gate whose output is turned round (-1: none) */
static void bv_eval(const lg_level *L, int force)
{
    static uint64_t in[3][BW];
    int nw = bv.nw;
    for (int k = 0; k < nw; k++) bv.ok[k] = bv.lit[k] = ~0ull;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        for (int s = 0; s < N->nin; s++) {
            const lg_wire *W = &L->w[N->in[s]];
            const uint64_t *src = bv.out[W->from][W->port == 2];
            int cv = bv.vwire[N->in[s]];
            for (int k = 0; k < nw; k++) in[s][k] = cv < 0 ? src[k] : src[k] & ~var_word(cv, k);
        }
        uint64_t *o = bv.out[i][0], *o2 = bv.out[i][1];
        int v = bv.vnode[i];
        for (int k = 0; k < nw; k++) {
            uint64_t a = N->nin ? in[0][k] : 0, b = N->nin > 1 ? in[1][k] : 0, c = N->nin > 2 ? in[2][k] : 0, r;
            uint64_t all = N->nin > 2 ? a & b & c : a & b;
            switch (N->kind) {
            case LG_SWITCH: r = v >= 0 ? var_word(v, k) : N->init ? ~0ull : 0; break;
            case LG_NOT: r = ~a; break;
            case LG_AND: r = all; break;
            case LG_OR: r = a | b | c; break;
            case LG_XOR: r = a ^ b ^ c; break;
            case LG_NAND: r = ~all; break;
            case LG_NOR: r = ~(a | b | c); break;
            case LG_ROUTE: r = a & ~var_word(v, k); o2[k] = a & var_word(v, k); break;
            case LG_LAMP: r = a; bv.ok[k] &= a; bv.lit[k] &= a; break;
            default: r = a; bv.ok[k] &= ~a; break;   /* an alarm */
            }
            o[k] = i == force ? ~r : r;
        }
    }
    for (int k = 0; k < nw; k++) {
        bv.ok[k] &= bv.valid[k];
        bv.lit[k] &= bv.valid[k];
    }
    bv.ok[nw - 1] &= bv.last;
    bv.lit[nw - 1] &= bv.last;
}

/* ---- what a level is like ---------------------------------------------------------------------------------------- */
static uint64_t hmix(uint64_t a, uint64_t b)
{
    uint64_t x = a * 0x9e3779b97f4a7c15ull ^ (b + 0x632be59bd9b4e019ull + (a << 6) + (a >> 2));
    x ^= x >> 31;
    x *= 0xbf58476d1ce4e5b9ull;
    x ^= x >> 29;
    x *= 0x94d049bb133111ebull;
    return x ^ x >> 32;
}

static uint64_t hsorted(uint64_t h, uint64_t *v, int n)   /* a set's hash: its items' order means nothing */
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && v[j - 1] > v[j]; j--) {
            uint64_t t = v[j];
            v[j] = v[j - 1];
            v[j - 1] = t;
        }
    h = hmix(h, (uint64_t)n);
    for (int i = 0; i < n; i++) h = hmix(h, v[i]);
    return h;
}

/* a port's class: an output, or a route switch's way set at the start, or its other way (exact); any way (else) */
static uint64_t port_class(const lg_level *L, int node, int port, int exact)
{
    return !port ? 0 : !exact ? 1 : port - 1 == L->n[node].init ? 1 : 2;
}

/* the circuit's key: each part's hash from its kind and its inputs' (as a set), the lamps' as a set, and each signal
 * going several ways with the parts it feeds (mode 0 exact, 1 structure, 2 shape) */
static uint64_t circuit_key(const lg_level *L, int mode)
{
    uint64_t H[LG_NODES], e[2 * LG_NODES], top[LG_NODES];
    int nt = 0, nb = 0;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        uint64_t in[3];
        for (int s = 0; s < N->nin; s++) {
            const lg_wire *W = &L->w[N->in[s]];
            in[s] = hmix(hmix(H[W->from], port_class(L, W->from, W->port, mode == 0)), W->cuttable);
        }
        if (N->kind == LG_SWITCH)
            H[i] = hmix(1000, N->locked ? 2u + N->init : mode == 0 ? N->init : 0);
        else if (N->kind >= LG_NOT && N->kind <= LG_NOR)
            H[i] = hsorted(hmix(2000, mode == 2 ? (N->kind == LG_NOT) : (uint64_t)N->kind), in, N->nin);
        else
            H[i] = hsorted(hmix(3000, N->kind), in, N->nin);
        if (N->kind == LG_LAMP || N->kind == LG_ALARM) top[nt++] = H[i];
    }
    for (int i = 0; i < L->nn; i++)
        for (int p = 0; p < nports(L->n[i].kind); p++) {
            uint64_t to[LG_WIRES];
            int n = 0, port = L->n[i].kind == LG_ROUTE ? p + 1 : 0;
            for (int w = 0; w < L->nw; w++)
                if (L->w[w].from == i && L->w[w].port == port) to[n++] = hmix(H[L->w[w].to], L->w[w].cuttable);
            if (n > 1) e[nb++] = hsorted(hmix(H[i], port_class(L, i, port, mode == 0)), to, n);
        }
    uint64_t h = hsorted(hsorted(hmix(77, (uint64_t)mode), top, nt), e, nb);
    return hmix(h, mode == 2 ? 0 : (uint64_t)L->cut_limit);
}

int lg_rate(const lg_level *L, lg_rating *r)
{
    memset(r, 0, sizeof *r);
    if (!bv_setup(L)) return 0;
    bv_eval(L, -1);
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        int k = N->kind;
        r->sw += k == LG_SWITCH;
        r->routes += k == LG_ROUTE;
        r->lamps += k == LG_LAMP;
        r->alarms += k == LG_ALARM;
        if (k >= LG_NOT && k <= LG_NOR) {
            r->gates++;
            r->xors += k == LG_XOR;
            r->nots += k == LG_NOT;
            r->three += N->nin == 3;
            if (L->depth[i] > r->depth) r->depth = L->depth[i];
            uint64_t one = 0, zero = 0;         /* its output the same in every setting? */
            for (int w = 0; w < bv.nw; w++) {
                uint64_t m = bv.valid[w] & (w == bv.nw - 1 ? bv.last : ~0ull);
                one |= bv.out[i][0][w] & m;
                zero |= ~bv.out[i][0][w] & m;
            }
            r->consts += !one || !zero;
        }
        for (int p = 0; p < nports(k); p++) {
            int n = 0, port = k == LG_ROUTE ? p + 1 : 0;
            for (int w = 0; w < L->nw; w++) n += L->w[w].from == i && L->w[w].port == port;
            r->fans += n > 1;
        }
    }
    r->vars = L->nvar;
    r->fuses = L->ncut;
    r->crossings = L->crossings;
    /* the solutions; the start's distance to each */
    static uint64_t ok[BW];
    memcpy(ok, bv.ok, sizeof(uint64_t) * (size_t)bv.nw);
    uint8_t val[LG_NODES], cut[LG_WIRES];
    for (int i = 0; i < L->nn; i++) val[i] = L->n[i].init;
    memset(cut, 0, sizeof cut);
    uint32_t base = lg_mask(L, val, cut), sol[64];
    uint64_t dist[64];
    int ns = 0;
    r->par = -1;
    for (int w = 0; w < bv.nw; w++) {
        r->nsol += popc(ok[w]);
        for (uint64_t x = ok[w]; x; x &= x - 1) {
            uint32_t m = (uint32_t)(w * 64 + ctz64(x));
            int d = bits(m ^ base);
            if (r->par < 0 || d < r->par) r->par = d;
            if (ns < 64) { dist[ns] = (uint64_t)d; sol[ns++] = m; }
        }
    }
    /* each variable's sway (the settings where turning it changes the answer), the solutions where it is on */
    uint64_t vk[LG_VARS];
    for (int v = 0; v < bv.nv; v++) {
        int sway = 0, on = 0;
        for (uint32_t m = 0; m < 1u << bv.nv; m++) {
            if (m >> v & 1) continue;
            uint32_t m2 = m | 1u << v;
            if (bit(bv.valid, m) && bit(bv.valid, m2) && bit(ok, m) != bit(ok, m2)) sway++;
        }
        for (int s = 0; s < ns; s++) on += (int)(sol[s] >> v & 1);
        if (L->var_kind[v] == LG_MROUTE && on > r->nsol - on) on = r->nsol - on;   /* its sides mean nothing */
        if (!sway) {
            if (L->var_kind[v] == LG_MCUT) r->decoys++;
            else r->herrings++;
        }
        vk[v] = hmix(hmix(L->var_kind[v], (uint64_t)sway), (uint64_t)on);
    }
    /* the gates that matter not: turned round, nothing changes */
    for (int i = 0; i < L->nn; i++) {
        int k = L->n[i].kind;
        if (k < LG_NOT || k > LG_NOR) continue;
        bv_eval(L, i);
        r->idle += !memcmp(ok, bv.ok, sizeof(uint64_t) * (size_t)bv.nw);
    }
    bv_eval(L, -1);
    r->exact = circuit_key(L, 0);
    r->structure = circuit_key(L, 1);
    r->shape = circuit_key(L, 2);
    r->func = hsorted(hmix(hmix(5, (uint64_t)r->nsol), (uint64_t)bv.nv), vk, bv.nv);
    r->start = hsorted(hmix(9, (uint64_t)r->par), dist, ns);
    int matter = r->vars - r->herrings - r->decoys;
    r->score = 1.2f * r->par + 0.5f * matter + 0.4f * r->gates + 0.6f * r->depth + 0.4f * r->xors + 0.4f * r->fans +
               0.6f * r->routes + 0.6f * r->fuses + 0.6f * r->alarms + 0.5f * (r->lamps - 1) + 0.3f * r->three +
               0.4f * r->herrings + 0.3f * r->decoys - 0.8f * log2f((float)(r->nsol > 0 ? r->nsol : 1));
    return 1;
}

/* ---- the generator ----------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, nin, depth, locked, init, row, col;
    int src[3], port[3], cut[3];
    float x;                                    /* its place from the left (the text's columns follow it) */
} gnode;

typedef struct { int node, port, used, ttl; } gsig;   /* a signal in the row: used (a branching may take it again) */

static int range(uint32_t *s, int lo, int hi) { return hi > lo ? lo + pr_below(s, hi - lo + 1) : lo; }

static int pick_kind(uint32_t *s, const lg_spec *sp)   /* a gate of two or three inputs */
{
    static const int W[LG_KINDS] = {0, 0, 6, 4, 4, 3, 3};
    int sum = 0, w[LG_KINDS] = {0};
    for (int k = LG_AND; k <= LG_NOR; k++)
        if (sp->kinds >> k & 1) sum += w[k] = W[k] * (sp->need >> k & 1 ? 3 : 1);
    if (!sum) return -1;
    int r = pr_below(s, sum);
    for (int k = LG_AND; k <= LG_NOR; k++)
        if ((r -= w[k]) < 0) return k;
    return LG_AND;
}

static float sig_x(const gnode *N, const gsig *g)
{
    return N[g->node].x + (g->port ? (g->port == 1 ? -0.25f : 0.25f) : 0);
}

/* a gate and the gate it makes with a NOT over it (not_twin: such a pair is drawn as that gate when allowed) */
static const int TWIN[LG_KINDS] = {[LG_AND] = LG_NAND, [LG_OR] = LG_NOR, [LG_NAND] = LG_AND, [LG_NOR] = LG_OR};

/* a circuit at random for the spec: its text (the switches' and route switches' ways at random) */
static int grow(uint32_t *s, const lg_spec *sp, char *out, int size)
{
    gnode N[LG_NODES];
    gsig q[2 * LG_NODES];
    int nn = 0, nq = 0;
    memset(N, 0, sizeof N);
    int nsw = range(s, sp->sw_lo, sp->sw_hi);
    if (nsw < 2 || nsw > LG_COLS) return 0;
    for (int i = 0; i < nsw; i++) {
        N[nn] = (gnode){.kind = LG_SWITCH, .x = (float)i, .init = pr_below(s, 2)};
        q[nq++] = (gsig){nn++, 0, 0, 0};
    }
    int G = range(s, sp->gates_lo, sp->gates_hi), R = range(s, sp->routes_lo, sp->routes_hi);
    int lamps = range(s, sp->lamps_lo > 1 ? sp->lamps_lo : 1, sp->lamps < 1 ? 1 : sp->lamps);
    int alarms = sp->alarms > 0 ? (sp->alarm_need ? 1 : pr_below(s, 2)) : 0;
    int k = lamps + alarms, gates = 0, routes = 0;
    float bal = 0.2f + pr_below(s, 100) / 70.0f;   /* how much the shallow signals come first: rows, or chains */
    float eb[LG_ROWS + 2];
    for (int d = 0; d < LG_ROWS + 2; d++) eb[d] = expf(-bal * d);
    for (int step = 0;; step++) {
        int un = 0;
        for (int i = 0; i < nq; i++) un += !q[i].used;
        if (un == k && gates >= G && routes >= R) break;
        if (step > 120 || un < k || gates > sp->gates_hi || nn >= LG_NODES - 4 || nq >= 2 * LG_NODES - 2) return 0;
        int need = un - k + (R - routes), spare = G - gates - need, act, odds = sp->nots + sp->fan;
        if (routes < R && pr_below(s, 100) < 40) act = 0;                            /* a route switch */
        else if (need > 0 && (spare <= 0 || pr_below(s, 100) >= odds)) act = 1;      /* a join */
        else if (spare > 0 && pr_below(s, odds > 0 ? odds : 1) < sp->nots) act = 2; /* a NOT */
        else if (spare > 0) act = 3;                                                 /* a branching */
        else continue;
        /* the signal it starts from: the shallower first (bal) */
        float wsum = 0, w[2 * LG_NODES];
        for (int i = 0; i < nq; i++) {
            w[i] = q[i].used ? 0 : eb[N[q[i].node].depth];
            int fk = N[q[i].node].kind;
            int gate = fk >= LG_AND && fk <= LG_NOR && fk != LG_XOR;
            int twin = sp->not_twin ? TWIN[fk] && sp->kinds >> TWIN[fk] & 1 : gate && sp->kinds >> LG_NAND & 1;
            if (act == 2 && (fk == LG_NOT || twin))
                w[i] = 0;                       /* NOT NOT: nothing; NOT AND: a NAND (when there are NANDs) */
            else if (act == 2 && sp->not_twin && TWIN[fk])
                w[i] *= 6;                      /* NOT NAND: an AND, when ANDs are not allowed themselves */
            wsum += w[i];
        }
        if (wsum <= 0) continue;
        float r = wsum * (float)pr_below(s, 10000) / 10000.0f;
        int a = 0;
        while (a < nq - 1 && (r -= w[a]) > 0) a++;
        if (q[a].used) continue;
        int in[3] = {a, 0, 0}, nin = 1, kind;
        if (act == 0) kind = LG_ROUTE;
        else if (act == 2) kind = LG_NOT;
        else if (act == 3) {                    /* a neighbour, used or not, stays in the row */
            int b = pr_below(s, 2) ? a - 1 : a + 1;
            if (b < 0 || b >= nq || q[b].node == q[a].node) continue;
            in[nin++] = b;
            kind = pick_kind(s, sp);
        } else {                                /* the next unused ones, left or right */
            int want = need >= 2 && pr_below(s, 100) < sp->three ? 3 : 2, dir = pr_below(s, 2) ? -1 : 1;
            for (int pass = 0; pass < 2 && nin < want; pass++, dir = -dir)
                for (int b = a + dir; b >= 0 && b < nq && nin < want; b += dir)
                    if (!q[b].used) in[nin++] = b;
            if (nin < 2) continue;
            kind = pick_kind(s, sp);
        }
        if (kind < 0) return 0;
        int twice = 0;                          /* both ways of one route switch into one gate: it never matters */
        for (int i = 0; i < nin; i++)
            for (int j = i + 1; j < nin; j++) twice |= q[in[i]].node == q[in[j]].node;
        if (twice) continue;
        int depth = 0;
        float x = 0;
        for (int j = 0; j < nin; j++) {
            if (N[q[in[j]].node].depth > depth) depth = N[q[in[j]].node].depth;
            x += sig_x(N, &q[in[j]]);
        }
        if (++depth > sp->depth_hi) continue;
        for (int i = 0; i < nin; i++)           /* from the left */
            for (int j = i + 1; j < nin; j++)
                if (in[j] < in[i]) { int t = in[i]; in[i] = in[j]; in[j] = t; }
        gnode *g = &N[nn];
        *g = (gnode){.kind = kind, .nin = nin, .depth = depth, .x = x / nin, .init = pr_below(s, 2)};
        for (int j = 0; j < nin; j++) {
            g->src[j] = q[in[j]].node;
            g->port[j] = q[in[j]].port;
        }
        /* the row: the inputs taken out (a branching's neighbour stays, used), the output where the first was */
        gsig q2[2 * LG_NODES];
        int m = 0, placed = 0;
        for (int i = 0; i < nq; i++) {
            int taken = 0;
            for (int j = 0; j < nin; j++) taken |= in[j] == i;
            gsig t = q[i];
            if (taken && (act != 3 || i == a)) {
                if (!placed) {
                    if (kind == LG_ROUTE) {
                        q2[m++] = (gsig){nn, 1, 0, 0};
                        q2[m++] = (gsig){nn, 2, 0, 0};
                    } else {
                        q2[m++] = (gsig){nn, 0, 0, 0};
                    }
                    placed = 1;
                }
                continue;
            }
            if (taken) { t.used = 1; t.ttl = 3; }   /* a branching's neighbour */
            if (t.used && --t.ttl <= 0) continue;
            q2[m++] = t;
        }
        memcpy(q, q2, sizeof(gsig) * (size_t)m);
        nq = m;
        nn++;
        if (kind == LG_ROUTE) routes++;
        else gates++;
    }
    if (gates < sp->gates_lo) return 0;
    /* the lamps, on the signals left (one of them an alarm, maybe) */
    int top = 0, li = 0;
    unsigned have = 0;
    for (int i = 0; i < nn; i++) {
        if (N[i].depth > top) top = N[i].depth;
        if (N[i].kind != LG_SWITCH) have |= 1u << N[i].kind;
    }
    if ((have & sp->need) != sp->need || top < sp->depth_lo) return 0;
    int alarm_at = alarms ? pr_below(s, k) : -1;
    for (int i = 0; i < nq; i++) {
        if (q[i].used) continue;
        int least = sp->depth_lo > 2 ? 2 : sp->depth_lo < 1 ? 1 : sp->depth_lo;
        if (N[q[i].node].depth < least) return 0;   /* a lamp on a switch, or on one gate over them: dull */
        N[nn] = (gnode){.kind = li == alarm_at ? LG_ALARM : LG_LAMP, .nin = 1, .depth = top + 1,
                        .x = sig_x(N, &q[i]), .src = {q[i].node, 0, 0}, .port = {q[i].port, 0, 0}};
        nn++;
        li++;
    }
    /* fuses on the gates' inputs (not a lamp's: no use cutting it); locked switches */
    int fuses = range(s, sp->fuses_lo, sp->fuses_hi), nf = 0;
    for (int t = 0; t < 80 && nf < fuses; t++) {
        int i = pr_below(s, nn);
        if (N[i].kind == LG_SWITCH || N[i].kind == LG_LAMP || N[i].kind == LG_ALARM) continue;
        int j = pr_below(s, N[i].nin);
        if (!N[i].cut[j]) { N[i].cut[j] = 1; nf++; }
    }
    if (nf < sp->fuses_lo) return 0;
    int lk = sp->locked < nsw - 2 ? sp->locked : nsw - 2, nl = 0;
    if (sp->locked_lo > lk) return 0;
    for (int n = range(s, sp->locked_lo, lk), t = 0; n > 0 && t < 20; t++) {
        int i = pr_below(s, nsw);
        if (!N[i].locked) { N[i].locked = 1; n--; nl++; }
    }
    if (nl < sp->locked_lo) return 0;
    /* the cells: the row by depth, the columns in the order of their places */
    for (int r = 0; r <= top + 1; r++) {
        int idx[LG_NODES], n = 0;
        for (int i = 0; i < nn; i++)
            if (N[i].depth == r) idx[n++] = i;
        if (n > LG_COLS || r >= LG_ROWS) return 0;
        for (int a = 1; a < n; a++)
            for (int b = a; b > 0 && N[idx[b - 1]].x > N[idx[b]].x; b--) {
                int t = idx[b];
                idx[b] = idx[b - 1];
                idx[b - 1] = t;
            }
        for (int c = 0; c < n; c++) {
            N[idx[c]].row = r;
            N[idx[c]].col = c;
        }
    }
    int len = 0;
    if (nf >= 2 && pr_below(s, 3) == 0) len = put(out, size, len, "%%%d", 1 + pr_below(s, nf - 1));
    for (int i = 0; i < nn; i++) {
        const gnode *g = &N[i];
        const char *name = g->kind == LG_SWITCH ? (g->locked ? (g->init ? "K" : "k") : g->init ? "S" : "s")
                         : g->kind == LG_ROUTE ? (g->init ? "Y" : "y") : KIND_NAME[g->kind];
        len = put(out, size, len, "%s%s@%c%d", len ? " " : "", name, 'a' + g->col, g->row + 1);
        for (int j = 0; j < g->nin; j++) {
            const gnode *f = &N[g->src[j]];
            len = put(out, size, len, "%c%c%d%s%s", j ? ',' : '(', 'a' + f->col, f->row + 1,
                      g->port[j] == 1 ? "<" : g->port[j] == 2 ? ">" : "", g->cut[j] ? "x" : "");
        }
        if (g->nin) len = put(out, size, len, ")");
    }
    return len < size;
}

/* a start for the solutions: one of them, its cuts mended, some switches and route switches turned; its par in the
 * spec's range, not every lamp lit, a route switch to turn in every solution when the spec says so */
static int pick_start(uint32_t *s, lg_level *L, const lg_spec *sp, const uint64_t *ok, const uint64_t *lit)
{
    uint32_t sol[64], routes = 0;
    int ns = 0, fv[LG_VARS], nf = 0;
    for (int w = 0; w < bv.nw; w++)
        for (uint64_t x = ok[w]; x && ns < 64; x &= x - 1) sol[ns++] = (uint32_t)(w * 64 + ctz64(x));
    if (!ns) return 0;
    for (int v = 0; v < L->nvar; v++) {
        routes |= (uint32_t)(L->var_kind[v] == LG_MROUTE) << v;
        if (!(bv.cuts >> v & 1)) fv[nf++] = v;
    }
    for (int t = 0; t < 40; t++) {
        uint32_t pick = sol[pr_below(s, ns)], m = pick & ~bv.cuts;
        int flips = range(s, sp->par_lo, sp->par_hi) - bits(pick & bv.cuts);
        pr_shuffle(s, fv, nf);
        for (int i = 0; i < flips && i < nf; i++) m ^= 1u << fv[i];
        int par = 99, turn = 1;
        for (int i = 0; i < ns; i++) {
            int d = bits(sol[i] ^ m);
            if (d < par) par = d;
            if (!((sol[i] ^ m) & routes)) turn = 0;
        }
        if (par < sp->par_lo || par > sp->par_hi || par < 1 || bit(lit, m) || (sp->need_route && !turn)) continue;
        int single = 0;                         /* not every move solves it */
        for (int v = 0; v < L->nvar; v++) single += bit(ok, m ^ 1u << v);
        if (L->nvar > 1 && single >= L->nvar) continue;
        for (int v = 0; v < L->nvar; v++) {
            int i = L->var_idx[v];
            if (L->var_kind[v] == LG_MSWITCH) L->n[L->sw[i]].init = (uint8_t)(m >> v & 1);
            else if (L->var_kind[v] == LG_MROUTE) L->n[L->rt[i]].init = (uint8_t)(m >> v & 1);
        }
        return 1;
    }
    return 0;
}

int lg_make(uint32_t seed, const lg_spec *sp, int tries, char *out, int size, lg_rating *r)
{
    static lg_level L;
    static char buf[1024];
    static uint64_t ok[BW], lit[BW];
    uint32_t s = pr_seed(seed);
    for (int t = 0; t < tries; t++) {
        if (!grow(&s, sp, buf, sizeof buf) || !parse(buf, &L, NULL, 0, 0) || !bv_setup(&L)) continue;
        bv_eval(&L, -1);
        int nsol = 0, need_cut = 1;
        for (int w = 0; w < bv.nw; w++) nsol += popc(bv.ok[w]);
        for (int w = 0; sp->need_cut && nsol <= sp->sols && w < bv.nw; w++)
            for (uint64_t x = bv.ok[w]; x; x &= x - 1)
                if (!((uint32_t)(w * 64 + ctz64(x)) & bv.cuts)) need_cut = 0;
        if (!nsol || nsol > sp->sols || (sp->need_cut && !need_cut)) continue;
        memcpy(ok, bv.ok, sizeof(uint64_t) * (size_t)bv.nw);
        memcpy(lit, bv.lit, sizeof(uint64_t) * (size_t)bv.nw);
        if (!pick_start(&s, &L, sp, ok, lit) || !lg_text(&L, buf, sizeof buf)) continue;
        if (!parse(buf, &L, NULL, 0, 0) || !lg_rate(&L, r)) continue;   /* rated first: the drawing costs more */
        if (r->consts || r->idle || r->herrings > sp->herrings || r->par < sp->par_lo || r->par > sp->par_hi) continue;
        if (r->depth < sp->depth_lo || r->depth > sp->depth_hi) continue;
        if (sp->cut_matters && r->fuses && r->decoys >= r->fuses) continue;
        if (!lg_parse(buf, &L, NULL, 0) || L.crossings > sp->cross || (sp->wide > 0 && L.width > sp->wide)) continue;
        r->crossings = L.crossings;
        if (size < 1 || (int)strlen(buf) >= size) return 0;
        strcpy(out, buf);
        return 1;
    }
    return 0;
}

/* ---- the die tab ------------------------------------------------------------------------------------------------- */
#define CB(k) (1u << LG_C_##k)
static const int GATE_OF[LG_C_GATES] = {LG_AND, LG_OR, LG_NOT, LG_XOR, LG_NAND, LG_NOR};

/* a puzzle needs few settings that light it: an AND, or a NOT with an OR, a NAND or a NOR (which make one:
 * a NOR of two NOTs is an AND). OR alone lights at any switch, XOR half of all settings; NAND and NOR alone light
 * most settings of a big circuit. */
int lg_comp_ok(unsigned c)
{
    return (c & CB(AND)) || ((c & CB(NOT)) && (c & (CB(OR) | CB(NAND) | CB(NOR))));
}

unsigned lg_comp_fix(unsigned c)
{
    c &= LG_C_ALL;
    return lg_comp_ok(c) ? c : c | CB(AND);
}

unsigned lg_comp_of(const lg_level *L)
{
    unsigned c = 0;
    int lamps = 0;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *N = &L->n[i];
        for (int k = 0; k < LG_C_GATES; k++)
            if (N->kind == GATE_OF[k]) c |= 1u << k;
        if (N->kind == LG_ALARM) c |= CB(ALARM);
        if (N->kind == LG_ROUTE) c |= CB(ROUTE);
        if (N->kind == LG_SWITCH && N->locked) c |= CB(LOCK);
        lamps += N->kind == LG_LAMP;
    }
    if (lamps > 1) c |= CB(LAMPS);
    if (L->ncut) c |= CB(CABLE);
    return c;
}

void lg_random_spec(int size, unsigned comp, unsigned need, lg_spec *sp)
{
    /* the sizes; their parts come from the checklist */
    static const lg_spec S[LG_RANDOM] = {
        /* small: 3 to 4 switches */
        {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .fan = 15, .nots = 25, .depth_lo = 2, .depth_hi = 3,
         .par_lo = 2, .par_hi = 3, .sols = 2, .cross = 1},
        /* medium: 5 to 6 switches */
        {.sw_lo = 5, .sw_hi = 6, .gates_lo = 5, .gates_hi = 8, .fan = 20, .nots = 20, .depth_lo = 3, .depth_hi = 4,
         .par_lo = 3, .par_hi = 5, .sols = 3, .cross = 2, .wide = 5.5f},
        /* big: 7 to 8 switches, three inputs too */
        {.sw_lo = 7, .sw_hi = 8, .gates_lo = 8, .gates_hi = 12, .three = 15, .fan = 25, .nots = 15, .depth_lo = 3,
         .depth_hi = 5, .par_lo = 4, .par_hi = 7, .sols = 4, .herrings = 1, .cross = 3, .wide = 6.5f},
    };
    static const int ROUTES[LG_RANDOM] = {1, 1, 2}, FUSES[LG_RANDOM] = {1, 2, 2}, LOCKS[LG_RANDOM] = {1, 1, 2};
    int z = size < 0 ? 0 : size >= LG_RANDOM ? LG_RANDOM - 1 : size, gates = 0;
    *sp = S[z];
    comp = lg_comp_fix(comp);
    sp->not_twin = 1;
    need &= comp;
    for (int k = 0; k < LG_C_GATES; k++) {
        if (comp >> k & 1) sp->kinds |= 1u << GATE_OF[k];
        if (need >> k & 1) { sp->need |= 1u << GATE_OF[k]; gates++; }
    }
    if (!(comp & CB(NOT))) sp->nots = 0;
    else if (need & CB(NOT) && sp->nots < 25) sp->nots = 25;
    if (gates > sp->gates_lo) sp->gates_lo = gates < sp->gates_hi ? gates : sp->gates_hi;
    int on;
#define TAKE(bit) (on = comp & CB(bit) ? 1 + !!(need & CB(bit)) : 0)   /* 0 not allowed, 1 allowed, 2 a must */
    if (TAKE(ROUTE)) { sp->routes_hi = ROUTES[z]; sp->routes_lo = on > 1; }
    if (TAKE(CABLE)) { sp->fuses_hi = FUSES[z]; sp->fuses_lo = on > 1; sp->cut_matters = on > 1; }
    if (TAKE(LOCK)) { sp->locked = LOCKS[z]; sp->locked_lo = on > 1; }
    if (TAKE(ALARM)) { sp->alarms = 1; sp->alarm_need = on > 1; }
    sp->lamps = TAKE(LAMPS) ? 2 : 1;
    sp->lamps_lo = on > 1 ? 2 : 0;
#undef TAKE
}

/* The rules eased a step (ease 1..): as many solutions as the levels may have (4), a smaller par; then a shallower
 * circuit, a switch that never matters, a crossing more; then more solutions (8, 16, 32), any width. */
static void ease_spec(lg_spec *sp, int ease)
{
    if (ease <= 0) return;
    int sols = ease < 3 ? 4 : 4 << (ease - 2 < 3 ? ease - 2 : 3);
    if (sp->sols < sols) sp->sols = sols;
    sp->par_lo = sp->par_lo - ease > 1 ? sp->par_lo - ease : 1;
    if (ease < 2) return;
    sp->depth_lo = sp->depth_lo - 1 > 1 ? sp->depth_lo - 1 : 1;
    sp->herrings += 1;
    sp->cross += 1;
    if (ease > 2) sp->wide = 0;
}

/* Each try asks for some of the parts chosen (as many as the size has room for, picked anew each time) with another
 * seed: first as strict as the levels, then eased a step; then one part fewer at each try. When even none required
 * finds nothing (a narrow choice: NOT and NAND), the rules eased further at each try. */
#define RND_TRIES 24
#define RND_CIRCUITS 160                        /* the circuits each try grows */
int lg_random(lg_game *g, int size, unsigned comp, uint32_t seed)
{
    static const int ROOM[LG_RANDOM] = {3, 5, 8};   /* the parts a circuit must have, at most */
    lg_spec sp;
    lg_rating r;
    char text[1024];
    static lg_level L;
    int z = size < 0 ? 0 : size >= LG_RANDOM ? LG_RANDOM - 1 : size, item[LG_COMPONENTS], n = 0;
    comp = lg_comp_fix(comp);
    for (int k = 0; k < LG_COMPONENTS; k++)
        if (comp >> k & 1) item[n++] = k;
    uint32_t s = pr_seed(seed ^ 0x5bd1e995u);
    int want = n < ROOM[z] ? n : ROOM[z];
    for (int k = 0; k < RND_TRIES; k++) {
        int m = k < 2 ? want : want + 1 - k, ease = k ? 1 : 0;   /* all of them strict, eased; then one fewer */
        if (m < 0) {
            ease = 1 - m;
            m = 0;
        }
        unsigned need = 0;
        pr_shuffle(&s, item, n);
        for (int i = 0; i < m; i++) need |= 1u << item[i];
        lg_random_spec(z, comp, need, &sp);
        ease_spec(&sp, ease);
        if (lg_make(seed + (uint32_t)k * 7919u, &sp, RND_CIRCUITS, text, sizeof text, &r) &&
            lg_parse(text, &L, NULL, 0)) {
            lg_load(g, &L, -1, r.par);
            return 1;
        }
    }
    /* nothing yet (a seed in a few hundred): the gates alone, eased; at the very last a small circuit
     * of the gates chosen, written here (par 2) */
    lg_random_spec(z, comp & ((1u << LG_C_GATES) - 1), 0, &sp);
    ease_spec(&sp, 5);
    if (lg_make(seed ^ 0xa5a5a5a5u, &sp, 4 * RND_CIRCUITS, text, sizeof text, &r) && lg_parse(text, &L, NULL, 0)) {
        lg_load(g, &L, -1, r.par);
        return 1;
    }
    static const char *const LAST[4] = {
        "s@a1 S@b1 s@c1 AND@a2(a1,b1) AND@b3(a2,c1) L@b4(b3)",   /* AND */
        "S@a1 S@b1 NOR@a2(a1,b1) L@a3(a2)",                      /* NOR (with NOT: an AND of NOTs) */
        "s@a1 s@b1 NAND@a2(a1,b1) NOT@a3(a2) L@a4(a3)",          /* NOT NAND */
        "S@a1 S@b1 OR@a2(a1,b1) NOT@a3(a2) L@a4(a3)",            /* NOT OR */
    };
    int last = comp & CB(AND) ? 0 : comp & CB(NOR) ? 1 : comp & CB(NAND) ? 2 : 3;
    if (!lg_parse(LAST[last], &L, NULL, 0)) return 0;
    lg_answer a;
    uint8_t val[LG_NODES], cut[LG_WIRES] = {0};
    for (int i = 0; i < L.nn; i++) val[i] = L.n[i].init;
    lg_solve(&L, val, cut, &a);
    lg_load(g, &L, -1, a.par);
    return 1;
}
