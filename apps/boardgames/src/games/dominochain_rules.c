/*
 * Domino Chain: the rules (dominochain.h): boards as text, pieces put, turned and taken back, undo, the push step by
 * step, the search for the fewest pieces (the par, the hint), the boards' maker, the levels. No SDL: tested by
 * tests/dominochain_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/dominochain.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static const char KIND_CH[] = "dtbs";

int dmc_kind_of(int t)
{
    switch (t) {
    case DMC_DOMINO: return DMC_K_DOMINO;
    case DMC_TURN: return DMC_K_TURN;
    case DMC_LONG: return DMC_K_LONG;
    case DMC_SPLIT: return DMC_K_SPLIT;
    default: return -1;
    }
}

static int type_of_kind(int k)
{
    static const int T[DMC_KINDS] = {DMC_DOMINO, DMC_TURN, DMC_LONG, DMC_SPLIT};
    return T[k & 3];
}

/* ---- boards as text ---------------------------------------------------------------------------------------------- */
static int cell_of(char ch, int *t, int *o)
{
    const char *p;
    *o = 0;
    if (ch == '#') *t = DMC_FELT;
    else if (ch == '.') *t = DMC_WOOD;
    else if (ch == 'h' || ch == 'v') { *t = DMC_DOMINO; *o = ch == 'h'; }
    else if (ch == 'H' || ch == 'V') { *t = DMC_LONG; *o = ch == 'H'; }
    else if (ch >= '1' && ch <= '4') { *t = DMC_TURN; *o = ch - '1'; }
    else if (ch == '+') *t = DMC_SPLIT;
    else if ((p = strchr("URDL", ch)) != NULL) { *t = DMC_START; *o = (int)(p - "URDL"); }
    else if (ch == 'B') *t = DMC_BELL;
    else return 0;
    return 1;
}

static char char_of(int t, int o)
{
    switch (t) {
    case DMC_WOOD: return '.';
    case DMC_DOMINO: return o ? 'h' : 'v';
    case DMC_LONG: return o ? 'H' : 'V';
    case DMC_TURN: return (char)('1' + (o & 3));
    case DMC_SPLIT: return '+';
    case DMC_START: return "URDL"[o & 3];
    case DMC_BELL: return 'B';
    default: return '#';
    }
}

int dmc_parse(dmc_map *m, const char *s, const char *stock)
{
    memset(m, 0, sizeof *m);
    m->start = m->bell = -1;
    int x = 0, y = 0;
    uint8_t t[DMC_CELLS] = {0}, o[DMC_CELLS] = {0};
    for (;; s++) {
        if (*s == '|' || !*s) {
            if (x) {
                if (!m->w) m->w = x;
                if (x != m->w) return 0;
                y++;
            }
            x = 0;
            if (!*s) break;
            continue;
        }
        int tt, oo;
        if (x >= DMC_MAXW || y >= DMC_MAXH || !cell_of(*s, &tt, &oo)) return 0;
        t[x + y * DMC_MAXW] = (uint8_t)tt;
        o[x + y * DMC_MAXW] = (uint8_t)oo;
        x++;
    }
    m->h = y;
    if (!m->w || !m->h) return 0;
    for (int yy = 0; yy < m->h; yy++)
        for (int xx = 0; xx < m->w; xx++) {
            int c = xx + yy * m->w;
            m->t[c] = t[xx + yy * DMC_MAXW];
            m->o[c] = o[xx + yy * DMC_MAXW];
            if (m->t[c] == DMC_START) { if (m->start >= 0) return 0; m->start = c; }
            if (m->t[c] == DMC_BELL) { if (m->bell >= 0) return 0; m->bell = c; }
        }
    if (m->start < 0 || m->bell < 0 || !stock || strlen(stock) != DMC_KINDS) return 0;
    for (int k = 0; k < DMC_KINDS; k++) {
        if (stock[k] < '0' || stock[k] > '9') return 0;
        m->stock[k] = (uint8_t)(stock[k] - '0');
    }
    return 1;
}

void dmc_text(const dmc_map *m, const dmc_state *s, char *out)
{
    int n = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) out[n++] = '|';
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w, o, t = dmc_piece(m, s, c, &o);
            out[n++] = char_of(t, o);
        }
    }
    out[n] = 0;
}

/* ---- pieces put -------------------------------------------------------------------------------------------------- */
int dmc_piece(const dmc_map *m, const dmc_state *s, int c, int *o)
{
    if (s && s->t[c] != DMC_WOOD) { *o = s->o[c]; return s->t[c]; }
    *o = m->o[c];
    return m->t[c];
}

int dmc_free(const dmc_map *m, const dmc_state *s, int c)
{
    return c >= 0 && c < m->w * m->h && m->t[c] == DMC_WOOD && s->t[c] == DMC_WOOD;
}

static void empty_state(const dmc_map *m, dmc_state *s)
{
    memset(s, 0, sizeof *s);
    memset(s->t, DMC_WOOD, sizeof s->t);
    memcpy(s->left, m->stock, sizeof s->left);
}

void dmc_begin(dmc_game *g)
{
    empty_state(&g->m, &g->s);
    g->nh = g->edits = g->hints = g->done = 0;
}

static int keep(dmc_game *g)
{
    if (g->done) return 0;
    if (g->nh >= DMC_HIST) {
        memmove(g->hist, g->hist + DMC_HIST / 2, sizeof g->hist / 2);
        g->nh -= DMC_HIST / 2;
    }
    g->hist[g->nh++] = g->s;
    g->edits++;
    return 1;
}

static int orient_ok(int t, int o)
{
    return t == DMC_TURN ? o >= 0 && o < 4 : t == DMC_SPLIT ? o == 0 : o == 0 || o == 1;
}

int dmc_put(dmc_game *g, int c, int kind, int o)
{
    if (g->done || kind < 0 || kind >= DMC_KINDS || !dmc_free(&g->m, &g->s, c) || !g->s.left[kind]) return 0;
    int t = type_of_kind(kind);
    if (!orient_ok(t, o)) return 0;
    keep(g);
    g->s.t[c] = (uint8_t)t;
    g->s.o[c] = (uint8_t)o;
    g->s.left[kind]--;
    g->s.placed++;
    return 1;
}

int dmc_take(dmc_game *g, int c)
{
    if (g->done || c < 0 || c >= g->m.w * g->m.h || g->s.t[c] == DMC_WOOD) return 0;
    keep(g);
    g->s.left[dmc_kind_of(g->s.t[c])]++;
    g->s.t[c] = DMC_WOOD;
    g->s.o[c] = 0;
    g->s.placed--;
    return 1;
}

int dmc_turn_piece(dmc_game *g, int c, int o)
{
    if (g->done || c < 0 || c >= g->m.w * g->m.h || g->s.t[c] == DMC_WOOD || !orient_ok(g->s.t[c], o) ||
        g->s.o[c] == o)
        return 0;
    keep(g);
    g->s.o[c] = (uint8_t)o;
    return 1;
}

int dmc_undo(dmc_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->edits--;
    g->done = 0;
    return 1;
}

/* ---- the push: one core for the game and the search ------------------------------------------------------------- */
typedef struct {
    uint8_t t[DMC_CELLS], o[DMC_CELLS], fallen[DMC_CELLS], und[DMC_CELLS];
    uint8_t left[DMC_KINDS], order[DMC_CELLS];
    int placed, w, h;
} board;

typedef struct { int16_t c, c2; uint8_t d, over; } hit;
#define MAXN 64

static int nb(const board *b, int c, int d)
{
    if (c < 0) return -1;
    int x = c % b->w + DX[d], y = c / b->w + DY[d];
    return x < 0 || y < 0 || x >= b->w || y >= b->h ? -1 : x + y * b->w;
}

static void add(hit *list, int *n, int c, int d, int over, int c2)
{
    if (c < 0 || *n >= MAXN) return;
    list[*n] = (hit){(int16_t)c, (int16_t)c2, (uint8_t)d, (uint8_t)over};
    (*n)++;
}

/* where a piece falling the way d from c sends its hit */
static void send(const board *b, int c, int d, int t, hit *next, int *nn)
{
    int a = nb(b, c, d);
    if (t == DMC_LONG) add(next, nn, a, d, 1, nb(b, a, d));
    else add(next, nn, a, d, 0, -1);
}

static int standing(const board *b, int c) { return b->t[c] >= DMC_DOMINO && !b->fallen[c]; }

/* one hit at step `step`: the piece hit falls (its hits for the next step added) or stays; -1 done, or the square of
 * undecided wood the hit reaches (the search decides it, then plays the same hit again) */
static int process(board *b, hit h, hit *next, int *nn, int step, dmc_trace *tr)
{
    for (;;) {
        int c = h.c, d = h.d;
        if (c < 0) return -1;
        if (standing(b, c)) {
            int t = b->t[c], o = b->o[c], out = -1;
            switch (t) {
            case DMC_DOMINO:
            case DMC_LONG: if ((d & 1) == o) out = d; break;
            case DMC_TURN: {
                int e = (d + 2) & 3;
                if (e == o) out = (o + 1) & 3;
                else if (e == ((o + 1) & 3)) out = o;
                break;
            }
            case DMC_SPLIT: out = d; break;
            case DMC_BELL: out = d; break;
            default: break;                     /* the start: down at the push */
            }
            if (out < 0) {
                if (tr && tr->nwob < DMC_FALLS)
                    tr->wob[tr->nwob++] = (dmc_event){(uint8_t)c, (uint8_t)step, (uint8_t)d};
                return -1;
            }
            b->fallen[c] = 1;
            if (tr && tr->n < DMC_FALLS) tr->fall[tr->n++] = (dmc_event){(uint8_t)c, (uint8_t)step, (uint8_t)out};
            if (t == DMC_BELL) { if (tr && tr->bell < 0) tr->bell = step; }
            else if (t == DMC_SPLIT) {
                send(b, c, (d + 1) & 3, t, next, nn);
                send(b, c, (d + 3) & 3, t, next, nn);
            } else send(b, c, out, t, next, nn);
            return -1;
        }
        if (b->und[c]) return c;
        if (h.over && h.c2 >= 0) { h.c = h.c2; h.over = 0; h.c2 = -1; continue; }
        return -1;
    }
}

static int all_down(const board *b)
{
    for (int c = 0; c < b->w * b->h; c++)
        if (standing(b, c)) return 0;
    return 1;
}

static void board_of(const dmc_map *m, const dmc_state *s, board *b)
{
    memset(b, 0, sizeof *b);
    b->w = m->w;
    b->h = m->h;
    for (int c = 0; c < m->w * m->h; c++) {
        int o, t = dmc_piece(m, s, c, &o);
        b->t[c] = (uint8_t)t;
        b->o[c] = (uint8_t)o;
    }
    if (s) { memcpy(b->left, s->left, sizeof b->left); b->placed = s->placed; }
}

/* the push's first hit: the start down at step 0 */
static void push_start(const dmc_map *m, board *b, hit *next, int *nn, dmc_trace *tr)
{
    b->fallen[m->start] = 1;
    if (tr && tr->n < DMC_FALLS) tr->fall[tr->n++] = (dmc_event){(uint8_t)m->start, 0, (uint8_t)m->o[m->start]};
    send(b, m->start, m->o[m->start], DMC_DOMINO, next, nn);
}

int dmc_run(const dmc_map *m, const dmc_state *s, dmc_trace *tr)
{
    board b;
    dmc_trace own;
    if (!tr) tr = &own;
    memset(tr, 0, sizeof *tr);
    tr->bell = -1;
    board_of(m, s, &b);
    hit cur[MAXN], next[MAXN];
    int nc = 0, nn = 0, step = 0;
    push_start(m, &b, cur, &nc, tr);
    while (nc) {
        step++;
        nn = 0;
        for (int i = 0; i < nc; i++) process(&b, cur[i], next, &nn, step, tr);
        memcpy(cur, next, sizeof(hit) * (size_t)nn);
        nc = nn;
    }
    tr->steps = step;
    memcpy(tr->fallen, b.fallen, sizeof tr->fallen);
    tr->ok = all_down(&b) && b.fallen[m->bell];
    return tr->ok;
}

int dmc_push(dmc_game *g, dmc_trace *tr)
{
    if (g->done) return 1;
    g->done = dmc_run(&g->m, &g->s, tr);
    return g->done;
}

int dmc_open_hits(const dmc_map *m, const dmc_state *s, uint8_t *way)
{
    board b;
    board_of(m, s, &b);
    for (int c = 0; c < m->w * m->h; c++) {
        way[c] = 255;
        b.und[c] = m->t[c] == DMC_WOOD && s->t[c] == DMC_WOOD;   /* each free square: seen, never filled */
    }
    hit cur[MAXN], next[MAXN];
    int nc = 0, nn = 0, n = 0;
    push_start(m, &b, cur, &nc, NULL);
    for (int step = 1; nc; step++) {
        nn = 0;
        for (int i = 0; i < nc; i++) {
            int c = process(&b, cur[i], next, &nn, step, NULL);
            if (c >= 0) {                       /* empty wood reached: noted, then left empty */
                if (way[c] == 255) { way[c] = cur[i].d; n++; }
                b.und[c] = 0;
                i--;
            }
        }
        memcpy(cur, next, sizeof(hit) * (size_t)nn);
        nc = nn;
    }
    return n;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct {
    int budget, found;
    long nodes, cap;
    board best;
} ctx_t;

static int go(ctx_t *x, board *b, const hit *cur, int nc, int i, hit *next, int nn, int step)
{
    int c = -1;
    if (++x->nodes > x->cap) return 0;
    for (;;) {
        while (i < nc) {
            c = process(b, cur[i], next, &nn, step, NULL);
            if (c >= 0) goto decide;
            i++;
        }
        if (!nn) {
            if (all_down(b)) { x->best = *b; x->found = 1; return 1; }
            return 0;
        }
        hit fresh[MAXN];
        return go(x, b, next, nn, 0, fresh, 0, step + 1);
    }
decide: {
        int d = cur[i].d;
        /* the options: left empty; a domino or a long one along the push; a splitter; a turn either way */
        int opt_t[6] = {DMC_WOOD, DMC_DOMINO, DMC_LONG, DMC_SPLIT, DMC_TURN, DMC_TURN};
        int opt_o[6] = {0, d & 1, d & 1, 0, (d + 2) & 3, (d + 1) & 3};
        for (int k = 0; k < 6; k++) {
            int t = opt_t[k], kind = dmc_kind_of(t);
            if (t != DMC_WOOD && (b->placed >= x->budget || !b->left[kind])) continue;
            board nbd = *b;
            hit nx[MAXN];
            memcpy(nx, next, sizeof(hit) * (size_t)nn);
            nbd.und[c] = 0;
            if (t != DMC_WOOD) {
                nbd.t[c] = (uint8_t)t;
                nbd.o[c] = (uint8_t)opt_o[k];
                nbd.left[kind]--;
                nbd.order[nbd.placed++] = (uint8_t)c;
            }
            if (go(x, &nbd, cur, nc, i, nx, nn, step)) return 1;
            if (x->nodes > x->cap) return 0;
        }
        return 0;
    }
}

int dmc_solve(const dmc_map *m, const dmc_state *s, int most, long cap, dmc_state *sol, int *order, long *nodes)
{
    ctx_t x;
    memset(&x, 0, sizeof x);
    x.cap = cap;
    board b;
    board_of(m, s, &b);
    int res = -1;
    for (int c = 0; c < m->w * m->h; c++) b.und[c] = m->t[c] == DMC_WOOD && s->t[c] == DMC_WOOD;
    hit cur[MAXN], next[MAXN];
    int nc = 0;
    push_start(m, &b, cur, &nc, NULL);
    for (int extra = 0; extra <= most && res < 0 && x.nodes <= cap; extra++) {
        board bb = b;
        bb.placed = 0;
        x.budget = extra;
        if (go(&x, &bb, cur, nc, 0, next, 0, 1)) res = extra;
    }
    if (nodes) *nodes = x.nodes;
    if (res >= 0) {
        if (sol) {
            *sol = *s;
            for (int c = 0; c < m->w * m->h; c++)
                if (m->t[c] == DMC_WOOD && s->t[c] == DMC_WOOD && x.best.t[c] != DMC_WOOD) {
                    sol->t[c] = x.best.t[c];
                    sol->o[c] = x.best.o[c];
                    sol->left[dmc_kind_of(x.best.t[c])]--;
                    sol->placed++;
                }
        }
        if (order)
            for (int k = 0; k < res; k++) order[k] = x.best.order[k];
    }
    return res;
}

int dmc_hint(const dmc_map *m, const dmc_state *s, int par, int *c, int *kind, int *o)
{
    dmc_state sol;
    int order[DMC_CELLS], most = 0;
    for (int k = 0; k < DMC_KINDS; k++) most += s->left[k];
    int r = dmc_solve(m, s, most, 2000000, &sol, order, NULL);
    if (r > 0 && (par <= 0 || s->placed + r <= par)) {
        *c = order[0];
        *kind = dmc_kind_of(sol.t[*c]);
        *o = sol.o[*c];
        return 1;
    }
    dmc_state e;
    empty_state(m, &e);
    most = 0;
    for (int k = 0; k < DMC_KINDS; k++) most += m->stock[k];
    r = dmc_solve(m, &e, most, 4000000, &sol, order, NULL);
    if (r < 0) return 0;
    for (int q = 0; q < m->w * m->h; q++)      /* a piece put that the way does not use: back */
        if (s->t[q] != DMC_WOOD && (sol.t[q] != s->t[q] || sol.o[q] != s->o[q])) { *c = q; return 2; }
    for (int k = 0; k < r; k++)
        if (s->t[order[k]] == DMC_WOOD) {
            *c = order[k];
            *kind = dmc_kind_of(sol.t[*c]);
            *o = sol.o[*c];
            return 1;
        }
    return 0;
}

int dmc_script(dmc_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'p') { dmc_push(g, NULL); continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            for (int k = 0; k < 2 * DMC_CELLS && !g->done; k++) {
                int c, kind, o, h = dmc_hint(&g->m, &g->s, g->par, &c, &kind, &o);
                if (h == 1) dmc_put(g, c, kind, o);
                else if (h == 2) dmc_take(g, c);
                else break;
                g->hints++;
                if (!all) break;
                if (dmc_run(&g->m, &g->s, NULL)) break;
            }
            if (all) dmc_push(g, NULL);
            continue;
        }
        if (*s == 'x') {
            if (!s[1] || !s[2]) return 0;
            int x = s[1] - 'a', y = s[2] - 'a';
            s += 2;
            if (x < 0 || y < 0 || x >= g->m.w || y >= g->m.h || !dmc_take(g, x + y * g->m.w)) return 0;
            continue;
        }
        const char *k = strchr(KIND_CH, *s);
        if (!k || !s[1] || !s[2] || !s[3]) return 0;
        int x = s[1] - 'a', y = s[2] - 'a', o = s[3] - '0';
        s += 3;
        if (x < 0 || y < 0 || x >= g->m.w || y >= g->m.h) return 0;
        if (!dmc_put(g, x + y * g->m.w, (int)(k - KIND_CH), o)) return 0;
    }
    return 1;
}

/* ---- the maker: a chain laid at random on the board, then pieces taken out for the stock ------------------------- */
typedef struct { int w, h; uint8_t t[DMC_CELLS], o[DMC_CELLS], used[DMC_CELLS]; int n, cells[DMC_CELLS]; } chain;

static int at(const chain *k, int x, int y) { return x < 0 || y < 0 || x >= k->w || y >= k->h ? -1 : x + y * k->w; }

static int cnb(const chain *k, int c, int d) { return c < 0 ? -1 : at(k, c % k->w + DX[d], c / k->w + DY[d]); }

/* a square the chain may take: on the board, free, its neighbours free but `from` (no two parts of the chain touch) */
static int room(const chain *k, int c, int from)
{
    if (c < 0 || k->used[c]) return 0;
    for (int d = 0; d < 4; d++) {
        int e = cnb(k, c, d);
        if (e >= 0 && e != from && k->used[e]) return 0;
    }
    return 1;
}

static void lay(chain *k, int c, int t, int o)
{
    k->t[c] = (uint8_t)t;
    k->o[c] = (uint8_t)o;
    k->used[c] = 1;
    k->cells[k->n++] = c;
}

/* the chain from the start: straight runs, a turn or a long domino over a felt square or a splitter between them,
 * the bell at its end; 0 when it does not fit */
static int lay_chain(chain *k, const dmc_spec *sp, uint32_t *r)
{
    memset(k->t, DMC_FELT, sizeof k->t);
    memset(k->used, 0, sizeof k->used);
    k->n = 0;
    int x = 1 + pr_below(r, k->w - 2), y = 1 + pr_below(r, k->h - 2), d = pr_below(r, 4);
    int c = at(k, x, y), prev = -1, turns = sp->turns, longs = sp->longs, splits = sp->splits;
    lay(k, c, DMC_START, d);
    for (int run = 0; run < sp->runs; run++) {
        int len = 1 + pr_below(r, 3);
        for (int i = 0; i < len; i++) {
            int e = cnb(k, c, d);
            if (!room(k, e, c)) return 0;
            lay(k, e, DMC_DOMINO, d & 1);
            prev = c;
            c = e;
        }
        if (run == sp->runs - 1) break;
        int pick = pr_below(r, 10);
        if (longs + splits >= sp->runs - 1 - run) pick = longs ? 0 : 4;   /* the last junctions: the specials due */
        if (longs && pick < 3) {                /* a long domino over a felt square */
            int e = cnb(k, c, d), f = cnb(k, e, d), g = cnb(k, f, d);
            if (!room(k, e, c) || f < 0 || k->used[f] || !room(k, g, -1)) return 0;
            lay(k, e, DMC_LONG, d & 1);
            k->used[f] = 2;                     /* the gap it reaches over: felt, kept free */
            lay(k, g, DMC_DOMINO, d & 1);
            prev = f;
            c = g;
            longs--;
        } else if (splits && pick < 6) {        /* a splitter: one way goes on, the other a short dead end */
            int e = cnb(k, c, d);
            if (!room(k, e, c)) return 0;
            lay(k, e, DMC_SPLIT, 0);
            int side = pr_below(r, 2), dead = (d + (side ? 1 : 3)) & 3, n = 1 + pr_below(r, 2), q = e;
            for (int i = 0; i < n; i++) {
                int f = cnb(k, q, dead);
                if (!room(k, f, q)) return 0;
                lay(k, f, DMC_DOMINO, dead & 1);
                q = f;
            }
            int beyond = cnb(k, q, dead);       /* where the dead end falls: kept empty */
            if (beyond >= 0) {
                if (k->used[beyond]) return 0;
                k->used[beyond] = 2;
            }
            prev = e;
            c = e;
            d = (d + (side ? 3 : 1)) & 3;
            splits--;
        } else if (turns) {                     /* a turn */
            int e = cnb(k, c, d), nd = (d + (pr_below(r, 2) ? 1 : 3)) & 3;
            if (!room(k, e, c)) return 0;
            int in = (d + 2) & 3, corner = ((in + 1) & 3) == nd ? in : nd;
            lay(k, e, DMC_TURN, corner);
            prev = c;
            c = e;
            d = nd;
            turns--;
        }
    }
    (void)prev;
    int e = cnb(k, c, d);
    if (!room(k, e, c)) return 0;
    lay(k, e, DMC_BELL, 0);
    return turns <= sp->turns - (sp->turns > 0) && splits == 0 && longs == 0;
}

static void crop(dmc_map *m, dmc_state *s)      /* the felt round the wood taken away */
{
    int x0 = m->w, x1 = -1, y0 = m->h, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != DMC_FELT) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    dmc_map n = *m;
    dmc_state ns = *s;
    n.w = x1 - x0 + 1;
    n.h = y1 - y0 + 1;
    memset(n.t, DMC_FELT, sizeof n.t);
    memset(n.o, 0, sizeof n.o);
    memset(ns.t, DMC_WOOD, sizeof ns.t);
    memset(ns.o, 0, sizeof ns.o);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int a = x + y * m->w, b = x - x0 + (y - y0) * n.w;
            n.t[b] = m->t[a];
            n.o[b] = m->o[a];
            ns.t[b] = s->t[a];
            ns.o[b] = s->o[a];
        }
    n.start = m->start % m->w - x0 + (m->start / m->w - y0) * n.w;
    n.bell = m->bell % m->w - x0 + (m->bell / m->w - y0) * n.w;
    *m = n;
    *s = ns;
}

void dmc_make(dmc_map *m, dmc_state *sol, const dmc_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    static chain k;
    for (int tries = 0; tries < 3000; tries++) {
        k.w = sp->w;
        k.h = sp->h;
        if (!lay_chain(&k, sp, &r)) continue;
        memset(m, 0, sizeof *m);
        m->w = k.w;
        m->h = k.h;
        for (int c = 0; c < k.w * k.h; c++) {
            m->t[c] = k.used[c] == 1 ? k.t[c] : DMC_FELT;
            m->o[c] = k.used[c] == 1 ? k.o[c] : 0;
            if (m->t[c] == DMC_START) m->start = c;
            if (m->t[c] == DMC_BELL) m->bell = c;
        }
        dmc_state all;
        empty_state(m, &all);
        if (!dmc_run(m, &all, NULL)) continue;  /* the whole chain must fall */
        /* the stock: the special pieces first (up to the spec's), then dominoes */
        int take[DMC_KINDS] = {0, sp->stock_turns, sp->stock_longs, sp->stock_splits}, gaps = sp->gaps, got = 0;
        empty_state(m, sol);
        int idx[DMC_CELLS];
        for (int i = 0; i < k.n; i++) idx[i] = k.cells[i];
        pr_shuffle(&r, idx, k.n);
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < k.n && got < gaps; i++) {
                int c = idx[i], t = m->t[c], kind = dmc_kind_of(t);
                if (kind < 0 || (pass == 1) != (kind == DMC_K_DOMINO) || (pass == 0 && !take[kind])) continue;
                if (pass == 0) take[kind]--;
                sol->t[c] = (uint8_t)t;
                sol->o[c] = m->o[c];
                sol->placed++;
                m->t[c] = DMC_WOOD;
                m->o[c] = 0;
                m->stock[kind]++;
                got++;
            }
        if (got < gaps) continue;
        m->stock[DMC_K_DOMINO] = (uint8_t)(m->stock[DMC_K_DOMINO] + sp->extra);
        /* wood round it: from three decoys on, the chain's mirror image (a ghost of its way); then mirrored pairs of
         * squares next to the wood */
        if (sp->decoys >= 3)
            for (int c = 0; c < m->w * m->h; c++) {
                int mirror = m->w - 1 - c % m->w + c / m->w * m->w;
                if (k.used[c] == 1 && m->t[mirror] == DMC_FELT && !k.used[mirror]) m->t[mirror] = DMC_WOOD;
            }
        for (int j = 0, laid = 0; j < 30 * sp->decoys + 30 && laid < sp->decoys; j++) {
            int c = pr_below(&r, m->w * m->h), x = c % m->w, y = c / m->w, mirror = m->w - 1 - x + y * m->w, near = 0;
            if (m->t[c] != DMC_FELT || k.used[c] == 2) continue;
            for (int d = 0; d < 4; d++) {
                int e = at(&k, x + DX[d], y + DY[d]);
                near |= e >= 0 && m->t[e] != DMC_FELT;
            }
            if (!near) continue;
            m->t[c] = DMC_WOOD;
            if (m->t[mirror] == DMC_FELT && k.used[mirror] != 2) m->t[mirror] = DMC_WOOD;
            laid++;
        }
        sol->left[0] = sol->left[1] = sol->left[2] = sol->left[3] = 0;
        for (int q = 0; q < DMC_KINDS; q++) sol->left[q] = (uint8_t)(m->stock[q]);
        for (int c = 0; c < m->w * m->h; c++)
            if (sol->t[c] != DMC_WOOD) sol->left[dmc_kind_of(sol->t[c])]--;
        if (!dmc_run(m, sol, NULL)) continue;
        crop(m, sol);
        return;
    }
    dmc_parse(m, "Rh.hB", "1000");             /* (never seen) */
    empty_state(m, sol);
}

/* ---- the variants and the levels --------------------------------------------------------------------------------- */
void dmc_variant(int v, dmc_spec *sp)
{
    static const dmc_spec V[DMC_VARIANTS] = {
        {6, 7, 4, 3, 2, 0, 0, 1, 0, 0, 2, 1},
        {7, 8, 6, 4, 3, 1, 0, 2, 1, 0, 4, 1},
        {8, 10, 7, 5, 3, 1, 1, 2, 1, 1, 6, 1},
    };
    *sp = V[v < 0 || v >= DMC_VARIANTS ? 0 : v];
}

#ifndef DMC_GEN                                 /* not in the levels' tool (tools/dominochain_gen.c): it makes them */
#include "games/dominochain_levels.h"

int dmc_level(dmc_game *g, int level)
{
    if (level < 0 || level >= DMC_LEVELS) return 0;
    if (!dmc_parse(&g->m, dmc_levels[level].board, dmc_levels[level].stock)) return 0;
    dmc_begin(g);
    g->par = dmc_levels[level].par;
    return 1;
}

/* a puzzle at random: of a few made to the variant, the one needing the most pieces */
void dmc_random(dmc_game *g, int variant, uint32_t seed)
{
    dmc_spec sp;
    dmc_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    int best = -1;
    for (int k = 0; k < 4; k++) {
        dmc_map m;
        dmc_state sol, e;
        dmc_make(&m, &sol, &sp, pr_next(&r));
        empty_state(&m, &e);
        int p = dmc_solve(&m, &e, sp.gaps + sp.extra, 300000, NULL, NULL, NULL);
        if (p > best) { best = p; g->m = m; }
        if (best >= sp.gaps) break;
    }
    if (best < 1) { dmc_level(g, 0); return; }
    dmc_begin(g);
    g->par = best;
}
#endif
