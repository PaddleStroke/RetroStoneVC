/*
 * Falling Dominoes: the rules (dominochain.h): boards as text, moves, undo, the push step by step, the search for the
 * fewest moves (the par, the hint, what makes a level hard), the boards' maker, the levels. No SDL: tested by
 * tests/dominochain_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/dominochain.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

/* ---- boards as text ---------------------------------------------------------------------------------------------- */
static int cell_of(char ch, int *t, int *o)
{
    *o = 0;
    switch (ch) {
    case '#': *t = DMC_GAP; return 1;
    case '.': *t = DMC_WOOD; return 1;
    case 'h': case 'v': *t = DMC_DOMINO; *o = ch == 'h'; return 1;
    case 'X': case 'Y': *t = DMC_TRIGGER; *o = ch == 'X'; return 1;
    case 'd': case 'e': *t = DMC_DELAY; *o = ch == 'd'; return 1;
    case 'l': case 'i': *t = DMC_LONG; *o = ch == 'l'; return 1;
    case '+': *t = DMC_SPLIT; return 1;
    case 'S': *t = DMC_STOPPER; return 1;
    default: break;
    }
    if (ch >= '1' && ch <= '4') { *t = DMC_TURN; *o = ch - '1'; return 1; }
    return 0;
}

static char char_of(int t, int o)
{
    switch (t) {
    case DMC_WOOD: return '.';
    case DMC_DOMINO: return o ? 'h' : 'v';
    case DMC_TRIGGER: return o ? 'X' : 'Y';
    case DMC_DELAY: return o ? 'd' : 'e';
    case DMC_LONG: return o ? 'l' : 'i';
    case DMC_TURN: return (char)('1' + (o & 3));
    case DMC_SPLIT: return '+';
    case DMC_STOPPER: return 'S';
    default: return '#';
    }
}

int dmc_parse(dmc_state *s, const char *b)
{
    memset(s, 0, sizeof *s);
    int x = 0, y = 0, triggers = 0, pieces = 0;
    uint8_t t[DMC_CELLS] = {0}, o[DMC_CELLS] = {0};
    for (;; b++) {
        if (*b == '|' || !*b) {
            if (x) {
                if (!s->w) s->w = x;
                if (x != s->w) return 0;
                y++;
            }
            x = 0;
            if (!*b) break;
            continue;
        }
        int tt, oo;
        if (x >= DMC_MAXW || y >= DMC_MAXH || !cell_of(*b, &tt, &oo)) return 0;
        t[x + y * DMC_MAXW] = (uint8_t)tt;
        o[x + y * DMC_MAXW] = (uint8_t)oo;
        triggers += tt == DMC_TRIGGER;
        pieces += tt >= DMC_DOMINO;
        x++;
    }
    s->h = y;
    if (!s->w || !s->h || triggers != 1 || pieces > DMC_PIECES) return 0;
    for (int yy = 0; yy < s->h; yy++)
        for (int xx = 0; xx < s->w; xx++) {
            s->t[xx + yy * s->w] = t[xx + yy * DMC_MAXW];
            s->o[xx + yy * s->w] = o[xx + yy * DMC_MAXW];
        }
    return 1;
}

void dmc_text(const dmc_state *s, char *out)
{
    int n = 0;
    for (int y = 0; y < s->h; y++) {
        if (y) out[n++] = '|';
        for (int x = 0; x < s->w; x++) out[n++] = char_of(s->t[x + y * s->w], s->o[x + y * s->w]);
    }
    out[n] = 0;
}

/* ---- moves -------------------------------------------------------------------------------------------------------- */
static int turnable(int t) { return t == DMC_DOMINO || t == DMC_DELAY || t == DMC_LONG || t == DMC_TURN; }
static int pushable(int t) { return t == DMC_DOMINO || t == DMC_DELAY || t == DMC_LONG || t == DMC_TRIGGER; }

int dmc_frozen;

int dmc_movable(const dmc_state *s, int c)
{
    return c >= 0 && c < s->w * s->h && s->t[c] >= DMC_DOMINO && s->t[c] != DMC_TRIGGER && !(dmc_frozen >> s->t[c] & 1);
}

int dmc_can_move(const dmc_state *s, int m)
{
    int a = DMC_FROM(m), b = DMC_TO(m);
    if (!dmc_movable(s, a) || b < 0 || b >= s->w * s->h) return 0;
    return a == b ? turnable(s->t[a]) : s->t[b] == DMC_WOOD;
}

void dmc_apply(dmc_state *s, int m)
{
    int a = DMC_FROM(m), b = DMC_TO(m);
    if (a == b) {
        s->o[a] = (uint8_t)(s->t[a] == DMC_TURN ? (s->o[a] + 1) & 3 : !s->o[a]);
        return;
    }
    s->t[b] = s->t[a];
    s->o[b] = s->o[a];
    s->t[a] = DMC_WOOD;
    s->o[a] = 0;
}

int dmc_can_push(const dmc_state *s, int p)
{
    int c = p / 4, d = p % 4;
    return p >= 0 && c < s->w * s->h && pushable(s->t[c]) && (d & 1) == s->o[c];
}

void dmc_notation(const dmc_state *s, int m, char *out)
{
    int a = DMC_FROM(m), b = DMC_TO(m), n = 0;
    out[n++] = a == b ? 'r' : 'm';
    out[n++] = (char)('a' + a % s->w);
    out[n++] = (char)('a' + a / s->w);
    if (a != b) { out[n++] = (char)('a' + b % s->w); out[n++] = (char)('a' + b / s->w); }
    out[n] = 0;
}

/* ---- the push ----------------------------------------------------------------------------------------------------- */
typedef struct { int16_t c, c2; uint8_t d, over; } hit;
#define MAXN 96

static int nb(const dmc_state *s, int c, int d)
{
    if (c < 0) return -1;
    int x = c % s->w + DX[d], y = c / s->w + DY[d];
    return x < 0 || y < 0 || x >= s->w || y >= s->h ? -1 : x + y * s->w;
}

typedef struct {
    const dmc_state *s;
    int16_t step[DMC_CELLS];                    /* when each piece falls (-1 standing) */
    hit next[MAXN];
    int nn;
    struct { int16_t c; uint8_t d; int16_t at; } later[DMC_PIECES];   /* delay dominoes due */
    int nl;
    dmc_trace *tr;
} sim;

static void send(sim *S, int c, int d, int t)
{
    if (S->nn >= MAXN) return;
    int a = nb(S->s, c, d);
    if (a < 0) return;
    S->next[S->nn++] = (hit){(int16_t)a, (int16_t)(t == DMC_LONG ? nb(S->s, a, d) : -1), (uint8_t)d, t == DMC_LONG};
}

static void fall(sim *S, int c, int d, int step)   /* c falls the way d at step: its hits for the next step */
{
    int t = S->s->t[c];
    S->step[c] = (int16_t)step;
    if (S->tr && S->tr->n < DMC_FALLS) {
        S->tr->fall[S->tr->n].c = (uint8_t)c;
        S->tr->fall[S->tr->n].step = (uint8_t)step;
        S->tr->fall[S->tr->n++].way = (uint8_t)d;
    }
    if (t == DMC_SPLIT) { send(S, c, (d + 1) & 3, t); send(S, c, (d + 3) & 3, t); }
    else send(S, c, d, t);
}

static void knock(sim *S, hit h, int step)
{
    for (;;) {
        int c = h.c, d = h.d;
        if (c < 0) return;
        int t = S->s->t[c], o = S->s->o[c];
        if (t >= DMC_DOMINO && S->step[c] < 0) {
            switch (t) {
            case DMC_DOMINO: case DMC_TRIGGER: case DMC_LONG:
                if ((d & 1) == o) { fall(S, c, d, step); return; }
                break;
            case DMC_DELAY:
                if ((d & 1) == o) {
                    S->step[c] = (int16_t)(step + 2);   /* due: it falls then */
                    S->later[S->nl].c = (int16_t)c;
                    S->later[S->nl].d = (uint8_t)d;
                    S->later[S->nl++].at = (int16_t)(step + 2);
                    return;
                }
                break;
            case DMC_TURN: {
                int e = (d + 2) & 3, out = e == o ? (o + 1) & 3 : e == ((o + 1) & 3) ? o : -1;
                if (out >= 0) { fall(S, c, out, step); return; }
                break;
            }
            case DMC_SPLIT: fall(S, c, d, step); return;
            default: break;                     /* a stopper */
            }
            if (S->tr && S->tr->nwob < DMC_FALLS) {
                S->tr->wob[S->tr->nwob].c = (uint8_t)c;
                S->tr->wob[S->tr->nwob].step = (uint8_t)step;
                S->tr->wob[S->tr->nwob++].way = (uint8_t)d;
            }
            return;
        }
        if (h.over && h.c2 >= 0) { h.c = h.c2; h.over = 0; h.c2 = -1; continue; }
        return;
    }
}

int dmc_run(const dmc_state *s, int p, dmc_trace *tr)
{
    sim S;
    S.s = s;
    S.nn = S.nl = 0;
    S.tr = tr;
    for (int c = 0; c < DMC_CELLS; c++) S.step[c] = -1;
    if (tr) { memset(tr, 0, sizeof *tr); tr->trigger = -1; }
    if (!dmc_can_push(s, p)) return 0;
    fall(&S, p / 4, p % 4, 0);
    hit cur[MAXN];
    int step = 0;
    while (S.nn || S.nl) {
        step++;
        int nc = S.nn;
        memcpy(cur, S.next, sizeof(hit) * (size_t)nc);
        S.nn = 0;
        int due[DMC_PIECES], dd[DMC_PIECES], nd = 0, kept = 0;
        for (int i = 0; i < S.nl; i++)          /* the delay dominoes due now, in the order they were hit */
            if (S.later[i].at == step) { due[nd] = S.later[i].c; dd[nd++] = S.later[i].d; }
            else S.later[kept++] = S.later[i];
        S.nl = kept;
        for (int i = 0; i < nd; i++) {
            S.step[due[i]] = -1;
            fall(&S, due[i], dd[i], step);
        }
        for (int i = 0; i < nc; i++) knock(&S, cur[i], step);
        if (step > 4 * DMC_CELLS) break;
    }
    int last = -1, trig = -1, ok = 1;
    for (int c = 0; c < s->w * s->h; c++) {
        int t = s->t[c];
        if (t < DMC_DOMINO || t == DMC_STOPPER) continue;
        if (S.step[c] < 0) { ok = 0; continue; }
        if (t == DMC_TRIGGER) trig = S.step[c];
        else if (S.step[c] > last) last = S.step[c];
    }
    ok = ok && trig > last;
    if (tr) {
        tr->steps = step;
        tr->trigger = trig;
        tr->ok = ok;
        for (int c = 0; c < DMC_CELLS; c++) tr->fallen[c] = S.step[c] >= 0;
    }
    return ok;
}

int dmc_solving_push(const dmc_state *s)
{
    for (int c = 0; c < s->w * s->h; c++)
        if (pushable(s->t[c]))
            for (int k = 0; k < 2; k++) {
                int p = DMC_PUSH(c, (s->o[c] & 1) + 2 * k);
                if (dmc_run(s, p, NULL)) return p;
            }
    return -1;
}

/* ---- the search: positions as their pieces sorted (kind, way, square), alike pieces alike -------------------------- */
typedef struct { uint16_t p[DMC_PIECES]; } pos;
#define FIRSTS 8                                /* the first moves' bits: 512 */

static int pieces_of(const dmc_state *s, pos *out)
{
    int n = 0;
    memset(out, 0, sizeof *out);
    for (int c = 0; c < s->w * s->h; c++)
        if (s->t[c] >= DMC_DOMINO && n < DMC_PIECES) out->p[n++] = (uint16_t)(s->t[c] << 10 | s->o[c] << 7 | c);
    for (int i = 1; i < n; i++)                 /* (already in square order: sorted by kind and way next) */
        for (int j = i; j > 0 && out->p[j] < out->p[j - 1]; j--) {
            uint16_t k = out->p[j];
            out->p[j] = out->p[j - 1];
            out->p[j - 1] = k;
        }
    return n;
}

static void state_of(const dmc_state *floor, const pos *p, int n, dmc_state *out)
{
    *out = *floor;
    for (int i = 0; i < n; i++) {
        int c = p->p[i] & 127;
        out->t[c] = (uint8_t)(p->p[i] >> 10);
        out->o[c] = (uint8_t)(p->p[i] >> 7 & 7);
    }
}

static uint32_t hash_pos(const pos *p, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ p->p[i]) * 16777619u;
    return h ^ (h >> 16);
}

int dmc_search(const dmc_state *s, int depth, long cap, dmc_info *info)
{
    dmc_info own;
    if (!info) info = &own;
    memset(info, 0, sizeof *info);
    info->par = -1;
    info->push = -1;
    dmc_state floor = *s, a, b;
    for (int c = 0; c < s->w * s->h; c++)
        if (floor.t[c] != DMC_GAP) { floor.t[c] = DMC_WOOD; floor.o[c] = 0; }
    pos p0;
    int n = pieces_of(s, &p0);
    int pz = dmc_solving_push(s);
    if (pz >= 0) {
        info->par = 0;
        info->goals = 1;
        info->push = pz;
        return 1;
    }
    long size = 1;
    while (size < 2 * cap) size <<= 1;
    pos *q = malloc((size_t)cap * sizeof *q);
    int32_t *parent = malloc((size_t)cap * sizeof *parent), *tab = malloc((size_t)size * sizeof *tab);
    int16_t *mv = malloc((size_t)cap * sizeof *mv);
    uint64_t *first = calloc((size_t)cap * FIRSTS, sizeof *first);
    int found = 0;
    if (!q || !parent || !tab || !mv || !first) goto done;
    memset(tab, -1, (size_t)size * sizeof *tab);
    q[0] = p0;
    parent[0] = -1;
    tab[hash_pos(&p0, n) & (size - 1)] = 0;
    long nq = 1, lo = 0, hi = 1;
    int roots = 0;
    for (int d = 0; d < depth && !found; d++) {
        for (long i = lo; i < hi; i++) {
            state_of(&floor, &q[i], n, &a);
            for (int k = 0; k < n; k++) {
                int c = q[i].p[k] & 127;
                if (!dmc_movable(&a, c)) continue;
                for (int e = 0; e < a.w * a.h; e++) {
                    int m = DMC_MOVE(c, e);
                    if (!dmc_can_move(&a, m)) continue;
                    b = a;
                    dmc_apply(&b, m);
                    pos np;
                    pieces_of(&b, &np);
                    uint32_t h = hash_pos(&np, n) & (uint32_t)(size - 1);
                    while (tab[h] >= 0 && memcmp(&q[tab[h]], &np, sizeof np)) h = (h + 1) & (uint32_t)(size - 1);
                    long j = tab[h];
                    if (j < 0) {
                        if (nq >= cap) { info->states = nq; goto done; }
                        j = nq++;
                        tab[h] = (int32_t)j;
                        q[j] = np;
                        parent[j] = (int32_t)i;
                        mv[j] = (int16_t)m;
                        if (d == 0) {
                            if (roots < 64 * FIRSTS) first[j * FIRSTS + roots / 64] |= 1ull << (roots % 64);
                            roots++;
                        }
                    }
                    if (d > 0 && j >= hi)       /* reached in this layer: every first move that leads here */
                        for (int w = 0; w < FIRSTS; w++) first[j * FIRSTS + w] |= first[i * FIRSTS + w];
                }
            }
        }
        lo = hi;
        hi = nq;
        uint64_t good[FIRSTS] = {0};
        for (long i = lo; i < hi; i++) {        /* the new layer: the positions a push solves */
            state_of(&floor, &q[i], n, &a);
            int pp = dmc_solving_push(&a);
            if (pp < 0) continue;
            if (!found) {
                found = 1;
                info->par = d + 1;
                info->push = pp;
                int k = d + 1;
                for (long j = i; parent[j] >= 0 && k > 0; j = parent[j]) if (k <= DMC_SOL) info->sol[--k] = mv[j];
            }
            info->goals++;
            for (int w = 0; w < FIRSTS; w++) good[w] |= first[i * FIRSTS + w];
        }
        if (found) {
            info->firsts = roots;
            for (int w = 0; w < FIRSTS; w++) info->good += __builtin_popcountll(good[w]);
        }
    }
    info->states = nq;
done:
    free(q);
    free(parent);
    free(tab);
    free(mv);
    free(first);
    return found;
}

/* the obvious play: after any push, turn a piece the chain rocked, or carry a piece left standing to the square where
 * the chain stopped; a board this play solves within its par needs no idea */
int dmc_obvious(const dmc_state *s, int depth)
{
    if (dmc_solving_push(s) >= 0) return 1;
    if (!depth) return 0;
    for (int c = 0; c < s->w * s->h; c++) {
        int t = s->t[c];
        if (t != DMC_DOMINO && t != DMC_DELAY && t != DMC_LONG && t != DMC_TRIGGER) continue;
        for (int k = 0; k < 2; k++) {
            dmc_trace tr;
            int p = DMC_PUSH(c, (s->o[c] & 1) + 2 * k);
            dmc_run(s, p, &tr);
            for (int i = 0; i < tr.nwob; i++) {
                dmc_state n = *s;
                int m = DMC_MOVE(tr.wob[i].c, tr.wob[i].c);
                if (!dmc_can_move(&n, m)) continue;
                dmc_apply(&n, m);
                if (dmc_obvious(&n, depth - 1)) return 1;
            }
            for (int i = 0; i < tr.n; i++) {    /* where the chain stopped: empty wood next on its way */
                int a = tr.fall[i].c, w = tr.fall[i].way, ways[2] = {w, -1}, nw = 1;
                if (s->t[a] == DMC_SPLIT) { ways[0] = (w + 1) & 3; ways[1] = (w + 3) & 3; nw = 2; }
                for (int j = 0; j < nw; j++) {
                    int e = nb(s, a, ways[j]);
                    if (e >= 0 && s->t[a] == DMC_LONG && s->t[e] != DMC_WOOD) e = nb(s, e, ways[j]);
                    if (e < 0 || s->t[e] != DMC_WOOD) continue;
                    for (int q = 0; q < s->w * s->h; q++) {
                        if (!dmc_movable(s, q) || tr.fallen[q]) continue;
                        dmc_state n = *s;
                        dmc_apply(&n, DMC_MOVE(q, e));
                        if (dmc_obvious(&n, depth - 1)) return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* ---- the game ----------------------------------------------------------------------------------------------------- */
void dmc_begin(dmc_game *g)
{
    g->s = g->m.start;
    g->nh = g->moves = g->hints = g->done = 0;
}

int dmc_move(dmc_game *g, int m)
{
    if (g->done || !dmc_can_move(&g->s, m) || g->nh >= DMC_HIST) return 0;
    g->hist[g->nh++] = g->s;
    dmc_apply(&g->s, m);
    g->moves++;
    return 1;
}

int dmc_undo(dmc_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    g->done = 0;
    return 1;
}

int dmc_push(dmc_game *g, int p, dmc_trace *tr)
{
    if (g->done) return 1;
    g->done = dmc_run(&g->s, p, tr);
    return g->done;
}

static int same(const dmc_state *a, const dmc_state *b)
{
    pos x, y;
    int n = pieces_of(a, &x);
    return n == pieces_of(b, &y) && !memcmp(&x, &y, sizeof x);
}

int dmc_hint(dmc_game *g, int *m, int *p)
{
    if (g->done) return 0;
    int pz = dmc_solving_push(&g->s);
    if (pz >= 0) { *p = pz; return 2; }
    dmc_state t = g->m.start;                   /* on the stored way: its next move */
    for (int k = 0; k < g->m.nsol; k++) {
        if (same(&t, &g->s) && dmc_can_move(&g->s, g->m.sol[k])) { *m = g->m.sol[k]; return 1; }
        if (!dmc_can_move(&t, g->m.sol[k])) break;
        dmc_apply(&t, g->m.sol[k]);
    }
    dmc_info in;
    if (dmc_search(&g->s, 2, 40000, &in) && in.par > 0) { *m = in.sol[0]; return 1; }
    return g->nh ? 3 : 0;
}

int dmc_script(dmc_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            for (int k = 0; k < 64 && !g->done; k++) {
                int m, p, h = dmc_hint(g, &m, &p);
                if (!h) break;
                g->hints++;
                if (h == 1) dmc_move(g, m);
                else if (h == 3) dmc_undo(g);
                else dmc_push(g, p, NULL);
                if (!all) break;
            }
            continue;
        }
        int x0, y0, x1, y1;
        if (*s == 'm' || *s == 'r') {
            int turn = *s == 'r';
            if (!s[1] || !s[2] || (!turn && (!s[3] || !s[4]))) return 0;
            x0 = s[1] - 'a';
            y0 = s[2] - 'a';
            x1 = turn ? x0 : s[3] - 'a';
            y1 = turn ? y0 : s[4] - 'a';
            s += turn ? 2 : 4;
            if (x0 < 0 || y0 < 0 || x1 < 0 || y1 < 0 || x0 >= g->s.w || x1 >= g->s.w || y0 >= g->s.h || y1 >= g->s.h)
                return 0;
            if (!dmc_move(g, DMC_MOVE(x0 + y0 * g->s.w, x1 + y1 * g->s.w))) return 0;
            continue;
        }
        if (*s == 'p') {
            const char *w;
            if (!s[1] || !s[2] || !s[3] || !(w = strchr("urdl", s[3]))) return 0;
            x0 = s[1] - 'a';
            y0 = s[2] - 'a';
            s += 3;
            if (x0 < 0 || y0 < 0 || x0 >= g->s.w || y0 >= g->s.h) return 0;
            int p = DMC_PUSH(x0 + y0 * g->s.w, (int)(w - "urdl"));
            if (!dmc_can_push(&g->s, p)) return 0;
            dmc_push(g, p, NULL);
            continue;
        }
        return 0;
    }
    return 1;
}

/* ---- the maker: a chain laid at random, solved, then moves made at random ------------------------------------------ */
typedef struct { int w, h; uint8_t t[DMC_CELLS], o[DMC_CELLS], used[DMC_CELLS]; int n, cells[DMC_CELLS]; } chain;

static int at(const chain *k, int x, int y) { return x < 0 || y < 0 || x >= k->w || y >= k->h ? -1 : x + y * k->w; }
static int cnb(const chain *k, int c, int d) { return c < 0 ? -1 : at(k, c % k->w + DX[d], c / k->w + DY[d]); }

static int room(const chain *k, int c, int from)   /* free, its neighbours free but `from` */
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

/* the chain from the domino pushed: straight runs, a turn, a long domino over a gap or a splitter (its other way a
 * short run) between them, the trigger at its end; 0 when it does not fit */
static int lay_chain(chain *k, const dmc_spec *sp, uint32_t *r)
{
    memset(k->t, DMC_GAP, sizeof k->t);
    memset(k->o, 0, sizeof k->o);
    memset(k->used, 0, sizeof k->used);
    k->n = 0;
    int x = 1 + pr_below(r, k->w - 2), y = 1 + pr_below(r, k->h - 2), d = pr_below(r, 4);
    int c = at(k, x, y), turns = sp->turns, longs = sp->longs, splits = sp->splits;
    lay(k, c, DMC_DOMINO, d & 1);
    for (int run = 0; run < sp->runs; run++) {
        int len = 1 + pr_below(r, 3);
        for (int i = 0; i < len; i++) {
            int e = cnb(k, c, d);
            if (!room(k, e, c)) return 0;
            lay(k, e, DMC_DOMINO, d & 1);
            c = e;
        }
        if (run == sp->runs - 1) break;
        int pick = pr_below(r, 10);
        if (longs + splits >= sp->runs - 1 - run) pick = longs ? 0 : 4;
        if (longs && pick < 3) {
            int e = cnb(k, c, d), f = cnb(k, e, d), g = cnb(k, f, d);
            if (!room(k, e, c) || f < 0 || k->used[f] || !room(k, g, -1)) return 0;
            lay(k, e, DMC_LONG, d & 1);
            k->used[f] = 2;                     /* the gap it reaches over */
            lay(k, g, DMC_DOMINO, d & 1);
            c = g;
            longs--;
        } else if (splits && pick < 6) {
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
            int beyond = cnb(k, q, dead);
            if (beyond >= 0) {
                if (k->used[beyond]) return 0;
                k->used[beyond] = 3;            /* kept wood, empty */
            }
            c = e;
            d = (d + (side ? 3 : 1)) & 3;
            splits--;
        } else if (turns) {
            int e = cnb(k, c, d), nd = (d + (pr_below(r, 2) ? 1 : 3)) & 3;
            if (!room(k, e, c)) return 0;
            int in = (d + 2) & 3;
            lay(k, e, DMC_TURN, ((in + 1) & 3) == nd ? in : nd);
            c = e;
            d = nd;
            turns--;
        }
    }
    int e = cnb(k, c, d);
    if (!room(k, e, c)) return 0;
    lay(k, e, DMC_TRIGGER, d & 1);
    return splits == 0 && longs == 0;
}

/* how far a board is from solved under its best push: the pieces left standing, one more when the trigger is not
 * last (0: solved) */
static int badness(const dmc_state *s)
{
    int best = 1 << 30;
    for (int c = 0; c < s->w * s->h; c++)
        if (pushable(s->t[c]))
            for (int k = 0; k < 2; k++) {
                dmc_trace tr;
                if (dmc_run(s, DMC_PUSH(c, (s->o[c] & 1) + 2 * k), &tr)) return 0;
                int b = 0, last = -1;
                for (int i = 0; i < tr.n; i++)
                    if (s->t[tr.fall[i].c] != DMC_TRIGGER && tr.fall[i].step > last) last = tr.fall[i].step;
                for (int q = 0; q < s->w * s->h; q++)
                    b += s->t[q] >= DMC_DOMINO && s->t[q] != DMC_STOPPER && !tr.fallen[q];
                b += tr.trigger <= last;
                if (b < best) best = b;
            }
    return best;
}

static void crop(dmc_state *s)
{
    int x0 = s->w, x1 = -1, y0 = s->h, y1 = -1;
    for (int c = 0; c < s->w * s->h; c++)
        if (s->t[c] != DMC_GAP) {
            int x = c % s->w, y = c / s->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    dmc_state n;
    memset(&n, 0, sizeof n);
    n.w = x1 - x0 + 1;
    n.h = y1 - y0 + 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            n.t[x - x0 + (y - y0) * n.w] = s->t[x + y * s->w];
            n.o[x - x0 + (y - y0) * n.w] = s->o[x + y * s->w];
        }
    *s = n;
}

int dmc_make(dmc_state *s, const dmc_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    static chain k;
    for (int tries = 0; tries < 3000; tries++) {
        k.w = sp->w;
        k.h = sp->h;
        if (!lay_chain(&k, sp, &r)) continue;
        memset(s, 0, sizeof *s);
        s->w = k.w;
        s->h = k.h;
        for (int c = 0; c < k.w * k.h; c++) {
            s->t[c] = k.used[c] == 1 ? k.t[c] : k.used[c] == 3 ? DMC_WOOD : DMC_GAP;
            s->o[c] = k.used[c] == 1 ? k.o[c] : 0;
        }
        /* delay dominoes in place of some on the runs, where the trigger still falls last */
        for (int j = 0, laid = 0; j < 20 && laid < sp->delays; j++) {
            int c = k.cells[1 + pr_below(&r, k.n - 1)];
            if (s->t[c] != DMC_DOMINO) continue;
            s->t[c] = DMC_DELAY;
            if (dmc_solving_push(s) < 0) s->t[c] = DMC_DOMINO;
            else laid++;
        }
        if (dmc_solving_push(s) < 0) continue;
        /* the wood: the chain's mirror image (a ghost of its way), then squares next to the wood */
        for (int c = 0; c < s->w * s->h; c++) {
            int mirror = s->w - 1 - c % s->w + c / s->w * s->w;
            if (s->t[c] != DMC_GAP && s->t[mirror] == DMC_GAP && k.used[mirror] != 2) s->t[mirror] = DMC_WOOD;
        }
        for (int j = 0, laid = 0; j < 60 && laid < 2; j++) {
            int c = pr_below(&r, s->w * s->h), x = c % s->w, y = c / s->w, near = 0;
            if (s->t[c] != DMC_GAP || k.used[c] == 2) continue;
            for (int d = 0; d < 4; d++) {
                int e = at(&k, x + DX[d], y + DY[d]);
                near |= e >= 0 && s->t[e] != DMC_GAP;
            }
            if (near) { s->t[c] = DMC_WOOD; laid++; }
        }
        /* stoppers on free wood, where the solved chain still works */
        for (int j = 0, laid = 0; j < 40 && laid < sp->stoppers; j++) {
            int c = pr_below(&r, s->w * s->h);
            if (s->t[c] != DMC_WOOD) continue;
            s->t[c] = DMC_STOPPER;
            if (dmc_solving_push(s) < 0) s->t[c] = DMC_WOOD;
            else laid++;
        }
        /* then moves that keep it looking nearly right: of all moves, one leaving the fewest pieces standing (or the
         * trigger too soon) under its best push, without solving it; at random among the best */
        int made = 0;
        for (int j = 0; j < sp->scramble; j++) {
            int best = 1 << 30, pick[512], np = 0;
            for (int c = 0; c < s->w * s->h; c++) {
                if (!dmc_movable(s, c)) continue;
                for (int e = 0; e < s->w * s->h; e++) {
                    int m = DMC_MOVE(c, e);
                    if (!dmc_can_move(s, m)) continue;
                    dmc_state n = *s;
                    dmc_apply(&n, m);
                    int b = badness(&n);
                    if (b == 0) continue;           /* (solved again) */
                    if (b < best) { best = b; np = 0; }
                    if (b == best && np < 512) pick[np++] = m;
                }
            }
            if (!np) break;
            dmc_apply(s, pick[pr_below(&r, np)]);
            made++;
        }
        int pieces = 0;
        for (int c = 0; c < s->w * s->h; c++) pieces += s->t[c] >= DMC_DOMINO;
        if (made < sp->scramble || pieces > DMC_PIECES || dmc_solving_push(s) >= 0) continue;
        crop(s);
        return 1;
    }
    return 0;
}

/* ---- the levels and the puzzles at random ------------------------------------------------------------------------- */
int dmc_set(dmc_game *g, const char *board, int par, const char *sol)
{
    memset(&g->m, 0, sizeof g->m);
    if (!dmc_parse(&g->m.start, board)) return 0;
    g->m.par = par;
    g->m.push = -1;
    dmc_begin(g);
    if (sol) {                                  /* its stored way, in the notation */
        dmc_game t = *g;
        const char *p = sol;
        while (*p && g->m.nsol < DMC_SOL) {
            int len = *p == 'm' ? 5 : *p == 'r' ? 3 : *p == 'p' ? 4 : 1;
            char one[6];
            if ((int)strlen(p) < len) break;
            memcpy(one, p, (size_t)len);
            one[len] = 0;
            if (*p == 'm' || *p == 'r') {
                int n = t.nh;
                if (!dmc_script(&t, one) || t.nh != n + 1) break;
                int x0 = p[1] - 'a', y0 = p[2] - 'a', x1 = *p == 'm' ? p[3] - 'a' : x0, y1 = *p == 'm' ? p[4] - 'a' : y0;
                g->m.sol[g->m.nsol++] = DMC_MOVE(x0 + y0 * g->s.w, x1 + y1 * g->s.w);
            } else if (*p == 'p') {
                g->m.push = DMC_PUSH(p[1] - 'a' + (p[2] - 'a') * g->s.w, (int)(strchr("urdl", p[3]) - "urdl"));
            }
            p += len;
        }
    }
    g->par = par;
    return 1;
}

void dmc_variant(int v, dmc_spec *sp)
{
    static const dmc_spec V[DMC_VARIANTS] = {
        {5, 6, 3, 1, 0, 0, 0, 2, 2, 2},
        {6, 7, 3, 1, 0, 0, 0, 3, 2, 2},
        {7, 8, 4, 2, 1, 0, 0, 3, 2, 2},
    };
    *sp = V[v < 0 || v >= DMC_VARIANTS ? 0 : v];
}

#ifndef DMC_GEN                                 /* not in the levels' tool (tools/dominochain_gen.c): it makes them */
#include "games/dominochain_levels.h"

int dmc_level(dmc_game *g, int level)
{
    if (level < 0 || level >= DMC_LEVELS) return 0;
    return dmc_set(g, dmc_levels[level].board, dmc_levels[level].par, dmc_levels[level].sol);
}

/* a puzzle at random: made to the variant, its par found by a short search (two moves at most), one the obvious play
 * does not solve (after many tries, any) */
void dmc_random(dmc_game *g, int variant, uint32_t seed)
{
    dmc_spec sp;
    dmc_variant(variant, &sp);
    uint32_t r = pr_seed(seed);
    for (int k = 0; k < 90; k++) {
        dmc_state s;
        dmc_info in;
        if (!dmc_make(&s, &sp, pr_next(&r))) continue;
        if (!dmc_search(&s, sp.depth, 60000, &in) || in.par < 1) continue;
        if (k < 80 && dmc_obvious(&s, in.par)) continue;
        memset(&g->m, 0, sizeof g->m);
        g->m.start = s;
        g->m.par = in.par;
        g->m.nsol = in.par;
        memcpy(g->m.sol, in.sol, sizeof in.sol);
        g->m.push = in.push;
        dmc_begin(g);
        g->par = in.par;
        return;
    }
    dmc_level(g, 3);
}
#endif
