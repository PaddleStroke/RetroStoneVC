/*
 * Defuse the Bomb: the rules (defuse.h). The circuit is read and drawn by Logic Gates' rules (logicgates_rules.c);
 * here: the roles, the values of a state, the moves and the ticks (the capacitors' charges and pulses, the flip-flops
 * turning over, the board timer), the solver (the fewest moves, the ticks passing between them, never a change that
 * lights DETONATE), the hint, undo, the notation, the rating, and the generator of the levels' tool and the die tab.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/defuse.h"
#ifdef DF_NO_LEVELS                            /* (the levels' tool: it makes them) */
const char *const df_levels[DF_LEVELS];
const uint8_t df_pars[DF_LEVELS];
const char *const df_pool[DF_RANDOM][DF_POOL];
const uint8_t df_pool_pars[DF_RANDOM][DF_POOL];
#else
#include "games/defuse_levels.h"
#endif
#include "games/puzrand.h"

static int fail(char *err, int size, const char *msg)
{
    if (err && size > 0) snprintf(err, (size_t)size, "%s", msg);
    return 0;
}

/* ---- the level's text ---------------------------------------------------------------------------------------------- */
static int cell_of(const char *s, int *col, int *row)
{
    if (s[0] < 'a' || s[0] >= 'a' + LG_COLS || s[1] < '1' || s[1] >= '1' + LG_ROWS) return 0;
    *col = s[0] - 'a';
    *row = s[1] - '1';
    return 1;
}

static int node_at(const lg_level *L, int col, int row)
{
    for (int i = 0; i < L->nn; i++)
        if (L->n[i].col == col && L->n[i].row == row) return i;
    return -1;
}

static int8_t CTX[LG_WIRES];                    /* each wire's cable number (-1: none), for the level last indexed */
static const lg_level *indexed;

int df_parse(const char *src, df_level *D, char *err, int errsize)
{
    memset(D, 0, sizeof *D);
    indexed = NULL;                             /* (a new circuit at this address: its cables looked up again) */
    const char *sep = strchr(src, '~'), *lg = sep ? sep + 1 : src;
    struct { char k; int col, row, n; } R[LG_NODES];
    int nr = 0;
    D->secs = DF_SECS;
    for (const char *s = src; sep && s < sep;) {
        if (*s == ' ') { s++; continue; }
        if (*s == 'T') {
            D->secs = (int)strtol(s + 1, (char **)&s, 10);
            if (D->secs < 10 || D->secs > 600) return fail(err, errsize, "a bad timer");
            continue;
        }
        if (nr >= LG_NODES || !strchr("PZFC", *s) || !cell_of(s + 1, &R[nr].col, &R[nr].row))
            return fail(err, errsize, "a bad role");
        R[nr].k = *s;
        s += 3;
        R[nr].n = 0;
        if (*s == ':') R[nr].n = (int)strtol(s + 1, (char **)&s, 10);
        nr++;
    }
    if (!lg_parse(lg, &D->L, err, errsize)) return 0;
    lg_level *L = &D->L;
    if (L->nrt) return fail(err, errsize, "a route switch");
    D->det = D->disarm = D->power = D->battery = D->timer = -1;
    memset(D->idx, -1, sizeof D->idx);
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        int role = DF_GATE;
        if (n->kind == LG_SWITCH) role = n->locked ? (n->init ? DF_BATTERY : DF_TIMER) : DF_SW;
        else if (n->kind == LG_ALARM) role = DF_DET;
        else if (n->kind == LG_LAMP) role = DF_DISARM;
        D->role[i] = (uint8_t)role;
    }
    for (int k = 0; k < nr; k++) {
        int i = node_at(L, R[k].col, R[k].row);
        if (i < 0) return fail(err, errsize, "a role on an empty cell");
        int kind = L->n[i].kind;
        if (R[k].k == 'P' && kind == LG_LAMP) D->role[i] = DF_POWER;
        else if (R[k].k == 'Z' && D->role[i] == DF_TIMER) D->timer_ticks = R[k].n;
        else if (R[k].k == 'F' && kind == LG_NOT && D->nff < DF_MAXFF) {
            D->role[i] = DF_FLIP;
            D->ff0[D->nff] = R[k].n & 1;
            D->idx[i] = (int8_t)D->nff;
            D->ff[D->nff++] = i;
        } else if (R[k].k == 'C' && kind == LG_NOT && D->ncap < DF_MAXCAP && R[k].n >= 2 && R[k].n <= 15) {
            D->role[i] = DF_CAP;
            D->size[i] = (uint8_t)R[k].n;
            D->idx[i] = (int8_t)D->ncap;
            D->cap[D->ncap++] = i;
        } else {
            return fail(err, errsize, "a role that does not fit its part");
        }
    }
    for (int i = 0; i < L->nn; i++) {
        int r = D->role[i];
        int *slot = r == DF_DET ? &D->det : r == DF_DISARM ? &D->disarm : r == DF_POWER ? &D->power :
                    r == DF_BATTERY ? &D->battery : r == DF_TIMER ? &D->timer : NULL;
        if (!slot) continue;
        if (*slot >= 0) return fail(err, errsize, "two of a one-only part");
        *slot = i;
    }
    if (D->det < 0 || D->disarm < 0 || D->power < 0) return fail(err, errsize, "POWER, DISARM and DETONATE needed");
    if (D->timer >= 0 && (D->timer_ticks < 1 || D->timer_ticks > 60)) return fail(err, errsize, "a board timer's ticks");
    for (int i = 0; i < L->nsw; i++)
        if (D->role[L->sw[i]] == DF_SW) {
            if (D->nsw >= DF_MAXSW) return fail(err, errsize, "too many switches");
            D->idx[L->sw[i]] = (int8_t)D->nsw;
            D->sw[D->nsw++] = L->sw[i];
        }
    D->ncut = L->ncut;
    if (D->ncut > DF_MAXCUT) return fail(err, errsize, "too many cables");
    return 1;
}

df_state df_start(const df_level *D)
{
    df_state s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < D->nsw; i++)
        if (D->L.n[D->sw[i]].init) s.sw |= (uint8_t)(1u << i);
    for (int k = 0; k < D->nff; k++)
        if (D->ff0[k]) s.ff |= (uint8_t)(1u << k);
    s.tmr = D->timer >= 0 ? (uint8_t)D->timer_ticks : 0;
    return s;
}

/* ---- the values ---------------------------------------------------------------------------------------------------- */

void df_eval(const df_level *D, df_state *st, const uint8_t *before, unsigned fire, df_values *v)
{
    const lg_level *L = &D->L;
    if (indexed != L) {
        memset(CTX, -1, sizeof CTX);
        for (int i = 0; i < L->ncut; i++) CTX[L->cut[i]] = (int8_t)i;
        indexed = L;
    }
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        uint8_t in[3] = {0, 0, 0};
        for (int s = 0; s < n->nin; s++) {
            int w = n->in[s];
            uint8_t x = v->out[L->w[w].from];
            v->wsrc[w] = x;
            if (CTX[w] >= 0 && (st->cut >> CTX[w] & 1)) x = 0;
            v->wend[w] = x;
            in[s] = x;
        }
        int k = D->idx[i];
        switch (D->role[i]) {
        case DF_SW: v->out[i] = st->sw >> k & 1; break;
        case DF_BATTERY: v->out[i] = 1; break;
        case DF_TIMER: v->out[i] = st->tmr == 0; break;
        case DF_FLIP:
            v->ffin[k] = in[0];
            if (before && in[0] && !before[k]) st->ff ^= (uint8_t)(1u << k);
            v->out[i] = st->ff >> k & 1;
            break;
        case DF_CAP:
            v->capin[k] = in[0];
            v->out[i] = fire >> k & 1;
            break;
        case DF_GATE: v->out[i] = (uint8_t)lg_gate(n->kind, in, n->nin); break;
        default: v->out[i] = in[0];
        }
    }
    v->powered = v->out[D->power];
    v->det = v->powered && v->out[D->det];
    v->disarm = v->powered && v->out[D->disarm];
    v->win = !v->det && (!v->powered || v->disarm);
}

static int outcome(const df_values *v) { return v->det ? DF_LOST : v->win ? DF_WON : DF_SAFE; }

static void step(df_steps *st, const df_values *v, int origin)
{
    if (!st || st->n >= DF_STEPS) return;
    st->v[st->n] = *v;
    st->origin[st->n++] = origin;
}

int df_tick(const df_level *D, df_state *s, df_steps *st)
{
    df_values b, v;
    if (st) st->n = 0;
    df_eval(D, s, NULL, 0, &b);
    int origin = -1;
    if (s->tmr > 0 && --s->tmr == 0) origin = D->timer;
    unsigned fire = 0;
    for (int k = 0; k < D->ncap; k++) {
        if (!b.capin[k]) { s->ch[k] = 0; continue; }
        if (++s->ch[k] >= D->size[D->cap[k]]) {
            fire |= 1u << k;
            s->ch[k] = 0;
            origin = D->cap[k];
        }
    }
    df_eval(D, s, b.ffin, fire, &v);
    step(st, &v, origin);
    int r = outcome(&v);
    if (r != DF_SAFE || !fire) return r;
    df_values e;                                /* the pulse ends */
    df_eval(D, s, v.ffin, 0, &e);
    step(st, &e, origin);
    return outcome(&e);
}

int df_move_do(const df_level *D, df_state *s, df_move m, df_steps *st)
{
    if (m.kind == DF_MWAIT) return df_tick(D, s, st);
    df_values b, v;
    if (st) st->n = 0;
    df_eval(D, s, NULL, 0, &b);
    int origin;
    if (m.kind == DF_MFLIP) {
        if (m.idx >= D->nsw) return -1;
        s->sw ^= (uint8_t)(1u << m.idx);
        origin = D->sw[m.idx];
    } else if (m.kind == DF_MCUT) {
        if (m.idx >= D->ncut || (s->cut >> m.idx & 1)) return -1;
        s->cut |= (uint8_t)(1u << m.idx);
        origin = -2 - D->L.cut[m.idx];          /* (a wire: -2 - its number) */
    } else {
        return -1;
    }
    df_eval(D, s, b.ffin, 0, &v);
    step(st, &v, origin);
    return outcome(&v);
}

int df_moves(const df_level *D, const df_state *s, df_move *m)
{
    int n = 0;
    for (int i = 0; i < D->nsw; i++) m[n++] = (df_move){DF_MFLIP, (uint8_t)i};
    for (int i = 0; i < D->ncut; i++)
        if (!(s->cut >> i & 1)) m[n++] = (df_move){DF_MCUT, (uint8_t)i};
    return n;
}

/* ---- the solver: the fewest moves, then the fewest ticks (a cheapest-first search; a state is its switches, cuts,
 * flip-flops, charges, the board timer and the moves made since the last tick) ----------------------------------- */
static uint64_t pack(const df_state *s, int quick)
{
    uint64_t k = s->sw | (uint64_t)s->cut << 8 | (uint64_t)s->ff << 16 | (uint64_t)s->tmr << 20 | (uint64_t)quick << 27;
    for (int i = 0; i < DF_MAXCAP; i++) k |= (uint64_t)(s->ch[i] & 15) << (30 + 4 * i);
    return k + 1;                               /* (0: an empty slot) */
}

static df_state unpack(uint64_t k, int *quick)
{
    df_state s;
    k--;
    s.sw = (uint8_t)k;
    s.cut = (uint8_t)(k >> 8);
    s.ff = (uint8_t)(k >> 16 & 15);
    s.tmr = (uint8_t)(k >> 20 & 127);
    *quick = (int)(k >> 27 & 7);
    for (int i = 0; i < DF_MAXCAP; i++) s.ch[i] = (uint8_t)(k >> (30 + 4 * i) & 15);
    return s;
}

#define SLOTS (1 << 17)
#define COST(m, t) ((uint32_t)(m) << 12 | (uint32_t)(t))
typedef struct { uint64_t key; uint32_t cost; uint16_t first; uint8_t done; } slot;
typedef struct { uint32_t cost, at; } heapit;

static int frozen;                              /* the rating's: no time passes */

static uint32_t find(slot *H, uint64_t key)
{
    uint32_t h = (uint32_t)((key * 0x9e3779b97f4a7c15ull) >> 47) & (SLOTS - 1);
    while (H[h].key && H[h].key != key) h = (h + 1) & (SLOTS - 1);
    return h;
}

static void push(heapit *Q, int *n, heapit x)
{
    int i = (*n)++;
    while (i > 0 && Q[(i - 1) / 2].cost > x.cost) { Q[i] = Q[(i - 1) / 2]; i = (i - 1) / 2; }
    Q[i] = x;
}

static heapit pop(heapit *Q, int *n)
{
    heapit top = Q[0], x = Q[--*n];
    int i = 0;
    for (;;) {
        int c = 2 * i + 1;
        if (c >= *n) break;
        if (c + 1 < *n && Q[c + 1].cost < Q[c].cost) c++;
        if (Q[c].cost >= x.cost) break;
        Q[i] = Q[c];
        i = c;
    }
    if (*n) Q[i] = x;
    return top;
}

static int solve_q(const df_level *D, const df_state *s0, int quick0, df_move *first, int *ticks)
{
    df_state t = *s0;
    df_values v;
    df_eval(D, &t, NULL, 0, &v);
    if (v.win) { if (ticks) *ticks = 0; return 0; }
    /* (the tables kept between searches, made once; only the slots used are cleared after) */
    static slot *H;
    static heapit *Q;
    static uint32_t *used;
    if (!H) H = calloc(SLOTS, sizeof(slot));
    if (!Q) Q = malloc(sizeof(heapit) * SLOTS * 2);
    if (!used) used = malloc(sizeof(uint32_t) * SLOTS);
    int res = -1, nq = 0, stored = 0, limit = D->secs > 8 ? D->secs - 4 : D->secs;
    uint32_t best = 0xffffffffu;
    uint16_t best_first = 0;
    if (!H || !Q || !used) return -1;
    uint32_t h = find(H, pack(s0, quick0));
    used[stored++] = h;
    H[h] = (slot){pack(s0, quick0), 0, 0xffff, 0};
    push(Q, &nq, (heapit){0, h});
    while (nq) {
        heapit it = pop(Q, &nq);
        slot *sl = &H[it.at];
        if (sl->done || it.cost != sl->cost) continue;
        if (it.cost >= best) break;
        sl->done = 1;
        int q;
        df_state s = unpack(sl->key, &q);
        df_move ms[DF_MAXSW + DF_MAXCUT + 1];
        int nm = q < DF_QUICK ? df_moves(D, &s, ms) : 0;
        if (!frozen && (int)(it.cost & 4095) < limit) ms[nm++] = (df_move){DF_MWAIT, 0};
        for (int k = 0; k < nm; k++) {
            df_state ns = s;
            int r = df_move_do(D, &ns, ms[k], NULL);
            if (r < 0 || r == DF_LOST) continue;
            int wait = ms[k].kind == DF_MWAIT;
            uint32_t c = it.cost + (wait ? 1 : 1u << 12);
            uint16_t f = sl->first == 0xffff ? (uint16_t)(ms[k].kind << 8 | ms[k].idx) : sl->first;
            if (r == DF_WON) {
                if (c < best) { best = c; best_first = f; }
                continue;
            }
            uint64_t key = pack(&ns, wait ? 0 : q + 1);
            uint32_t at = find(H, key);
            if (H[at].key && (H[at].done || H[at].cost <= c)) continue;
            if (!H[at].key) {
                if (stored >= SLOTS * 3 / 4 || nq >= SLOTS * 2 - 1) goto done;   /* too big: given up */
                H[at].key = key;
                used[stored++] = at;
            }
            H[at].cost = c;
            H[at].first = f;
            H[at].done = 0;
            if (nq >= SLOTS * 2 - 1) goto done;
            push(Q, &nq, (heapit){c, at});
        }
    }
    if (best != 0xffffffffu) {
        res = (int)(best >> 12);
        if (ticks) *ticks = (int)(best & 4095);
        if (first) *first = (df_move){(uint8_t)(best_first >> 8), (uint8_t)(best_first & 255)};
    }
done:
    for (int i = 0; i < stored; i++) H[used[i]] = (slot){0, 0, 0, 0};
    return res;
}

int df_solve(const df_level *D, const df_state *s, df_move *first, int *ticks)
{
    return solve_q(D, s, 0, first, ticks);
}

/* ---- the game ------------------------------------------------------------------------------------------------------ */
void df_load(df_game *g, const df_level *D, int level, int par)
{
    if (&g->D != D) g->D = *D;
    indexed = NULL;
    g->level = level;
    g->par = par;
    g->start = df_start(&g->D);
    df_restart(g);
}

void df_restart(df_game *g)
{
    g->s = g->start;
    g->nh = 0;
    g->ticks = 0;
    g->hinted = 0;
    g->over = 0;
    df_eval(&g->D, &g->s, NULL, 0, &g->v);
}

int df_open(df_game *g, int level)
{
    if (level < 0 || level >= DF_LEVELS || !df_levels[level]) return 0;
    static df_level D;
    if (!df_parse(df_levels[level], &D, NULL, 0)) return 0;
    df_load(g, &D, level, df_pars[level]);
    return 1;
}

static int after(df_game *g, int r)
{
    df_eval(&g->D, &g->s, NULL, 0, &g->v);
    if (r == DF_LOST) g->v.det = 1;             /* (it blew at a pulse: shown so) */
    if (r != DF_SAFE) g->over = r;
    return r;
}

int df_play(df_game *g, df_move m)
{
    if (g->over || m.kind == DF_MWAIT) return m.kind == DF_MWAIT && !g->over ? df_pass(g) : -1;
    df_state before = g->s;
    int r = df_move_do(&g->D, &g->s, m, NULL);
    if (r < 0) return -1;
    if (g->nh < DF_HIST) g->hist[g->nh] = before;
    g->nh++;
    return after(g, r);
}

int df_pass(df_game *g)
{
    if (g->over) return -1;
    g->ticks++;
    return after(g, df_tick(&g->D, &g->s, NULL));
}

int df_undo(df_game *g)
{
    if (!g->nh || g->over == DF_LOST || g->nh > DF_HIST) return 0;
    g->s = g->hist[--g->nh];
    g->over = 0;
    df_eval(&g->D, &g->s, NULL, 0, &g->v);
    return 1;
}

void df_blow(df_game *g)
{
    if (!g->over) g->over = DF_LOST;
}

int df_stars(const df_game *g)
{
    if (g->over != DF_WON) return 0;
    int moves = g->nh, par = g->par > 0 ? g->par : moves;
    int s = moves <= par ? 3 : 2 * moves <= 3 * par ? 2 : 1;
    if (g->hinted && s > 2) s = 2;
    return s;
}

int df_hint(const df_game *g, df_move *m)
{
    if (g->over) return 0;
    return df_solve(&g->D, &g->s, m, NULL) >= 0 && !g->v.win;   /* (0 moves: time alone wins, a wait) */
}

int df_script(df_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        char c = *s++;
        int n = 0, digits = 0;
        while (*s >= '0' && *s <= '9') { n = n * 10 + *s++ - '0'; digits++; }
        if (c == 'u') { if (!df_undo(g)) return 0; continue; }
        if (c == 'x') { df_restart(g); continue; }
        if (c == 'w') {
            for (int k = 0; k < (digits ? n : 1) && !g->over; k++) df_pass(g);
            continue;
        }
        if (c == 'h') {
            df_move m;
            if (!df_hint(g, &m)) return 0;
            g->hinted = 1;
            continue;
        }
        if (c == '=') {
            df_restart(g);
            df_move m;
            for (int k = 0; k < 200 && !g->over; k++) {
                if (df_solve(&g->D, &g->s, &m, NULL) < 0) return 0;
                if (m.kind == DF_MWAIT) df_pass(g);
                else df_play(g, m);
            }
            if (g->over != DF_WON) return 0;
            continue;
        }
        if ((c != 's' && c != 'c') || n < 1) return 0;
        df_move m = {(uint8_t)(c == 's' ? DF_MFLIP : DF_MCUT), (uint8_t)(n - 1)};
        if (df_play(g, m) < 0) return 0;
    }
    return 1;
}

/* ---- the rating ---------------------------------------------------------------------------------------------------- */
static uint64_t mix(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001b3ull; }

/* how far towards DISARM a state is: its conditions lit (an AND's inputs), POWER out the most */
static int progress(const df_level *D, const df_values *v)
{
    if (!v->powered) return 10;
    const lg_node *n = &D->L.n[D->disarm];
    int w = n->in[0], src = D->L.w[w].from, p = 0;
    const lg_node *a = &D->L.n[src];
    if (D->role[src] == DF_GATE && a->kind == LG_AND) {
        for (int k = 0; k < a->nin; k++) p += v->wend[a->in[k]];
        return p + (v->wend[w] ? 1 : 0);
    }
    return v->wend[w] ? 2 : 0;
}

/* the best way's actions from s (moves and waits), each the solver's next */
static int best_way(const df_level *D, const df_state *s0, df_move *way, int cap)
{
    df_state s = *s0;
    int n = 0, q = 0;
    while (n < cap) {
        df_move m;
        if (solve_q(D, &s, q, &m, NULL) <= 0) break;
        way[n++] = m;
        int r = df_move_do(D, &s, m, NULL);
        q = m.kind == DF_MWAIT ? 0 : q + 1;
        if (r != DF_SAFE) break;
    }
    return n;
}

/* the best way's moves in every order (its waits kept in their places): the orders that still win */
static void orders(const df_level *D, const df_state *s0, const df_move *way, int n, int *ways, int *all)
{
    int ns = 0;
    df_move mv[16];
    for (int i = 0; i < n && ns < 16; i++)
        if (way[i].kind != DF_MWAIT) mv[ns++] = way[i];
    *ways = *all = 0;
    if (ns > 7) { *ways = 1; *all = 2; return; }
    int perm[7], used[7];
    /* every arrangement (repeats of one move counted once: an arrangement kept when no earlier one matches) */
    int total = 1;
    for (int i = 2; i <= ns; i++) total *= i;
    static df_move seen[5040][7];
    int nseen = 0;
    for (int p = 0; p < total; p++) {
        int x = p;
        memset(used, 0, sizeof used);
        for (int i = 0; i < ns; i++) {
            int k = x % (ns - i);
            x /= ns - i;
            int j = -1;
            while (k >= 0) { j++; if (!used[j]) k--; }
            used[j] = 1;
            perm[i] = j;
        }
        df_move seq[7];
        for (int i = 0; i < ns; i++) seq[i] = mv[perm[i]];
        int dup = 0;
        for (int a = 0; a < nseen && !dup; a++) dup = !memcmp(seen[a], seq, sizeof(df_move) * (size_t)ns);
        if (dup) continue;
        memcpy(seen[nseen++], seq, sizeof(df_move) * (size_t)ns);
        df_state s = *s0;
        int r = DF_SAFE, k = 0;
        for (int i = 0; i < n && r == DF_SAFE; i++)
            r = df_move_do(D, &s, way[i].kind == DF_MWAIT ? way[i] : seq[k++], NULL);
        (*all)++;
        if (r == DF_WON) (*ways)++;
    }
}

int df_rate(const df_level *D, const df_state *start, df_rating *r)
{
    memset(r, 0, sizeof *r);
    df_state s = *start;
    df_values v;
    df_eval(D, &s, NULL, 0, &v);
    if (v.det || v.win) return 0;
    r->par = df_solve(D, start, NULL, &r->ticks);
    if (r->par <= 0) return 0;
    const lg_level *L = &D->L;
    r->vars = D->nsw + D->ncut;
    for (int i = 0; i < L->nn; i++) r->gates += D->role[i] == DF_GATE || D->role[i] == DF_FLIP || D->role[i] == DF_CAP;
    r->depth = L->maxdepth;
    r->crossings = L->crossings;
    /* the first moves */
    df_move ms[DF_MAXSW + DF_MAXCUT + 1];
    int nm = df_moves(D, start, ms), gp = progress(D, &v), best_p = gp, greedy = -1;
    r->moves_first = nm;
    for (int k = 0; k < nm; k++) {
        df_state ns = *start;
        int res = df_move_do(D, &ns, ms[k], NULL);
        if (res == DF_LOST) { r->lose_first++; continue; }
        int p = solve_q(D, &ns, 1, NULL, NULL);
        if (res == DF_SAFE && p < 0) r->dead_first++;
        if (res == DF_WON || (res == DF_SAFE && p == r->par - 1)) r->nsol++;
        df_values a;
        df_eval(D, &ns, NULL, 0, &a);
        int pr = res == DF_WON ? 20 : progress(D, &a);
        if (pr > best_p) { best_p = pr; greedy = k; }
    }
    {
        df_state ns = *start;                   /* waiting first */
        int res = df_tick(D, &ns, NULL);
        if (res == DF_WON || (res == DF_SAFE && df_solve(D, &ns, NULL, NULL) == r->par)) r->nsol++;
    }
    if (greedy >= 0) {
        df_state ns = *start;
        int res = df_move_do(D, &ns, ms[greedy], NULL);
        r->greedy_ok = res == DF_WON || (res == DF_SAFE && solve_q(D, &ns, 1, NULL, NULL) == r->par - 1);
    }
    /* the orders of the best way's moves */
    df_move way[64];
    int n = best_way(D, start, way, 64);
    orders(D, start, way, n, &r->ways, &r->orders);
    /* time: the bomb left alone (its deadline); the way without time (frozen) */
    df_state w = *start;
    for (int t = 1; t <= 40; t++) {
        int res = df_tick(D, &w, NULL);
        if (res == DF_LOST) { r->deadline = t; break; }
        if (res == DF_WON) break;
    }
    frozen = 1;
    int fp = df_solve(D, start, NULL, NULL);
    frozen = 0;
    r->timing = fp != r->par || r->deadline > 0;
    /* the controls reaching two lamps or more (through the wires) */
    int reach[LG_NODES];
    for (int i = L->nn - 1; i >= 0; i--) {
        reach[i] = D->role[i] == DF_DET ? 1 : D->role[i] == DF_DISARM ? 2 : D->role[i] == DF_POWER ? 4 : 0;
        for (int w2 = 0; w2 < L->nw; w2++)
            if (L->w[w2].from == i) reach[i] |= reach[L->w[w2].to];
    }
    int many = 0, ctl = 0;
    for (int k = 0; k < D->nsw; k++) {
        int m = reach[D->sw[k]];
        ctl++;
        many += (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) >= 2;
    }
    for (int k = 0; k < D->ncut; k++) {
        int m = reach[L->w[L->cut[k]].to];
        ctl++;
        many += (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) >= 2;
    }
    r->shared = ctl ? 100 * many / ctl : 0;
    /* no gate stuck (states sampled), and the key: what the samples show */
    uint32_t seed = 12345;
    uint8_t seen0[LG_NODES] = {0}, seen1[LG_NODES] = {0};
    uint64_t key = 0xcbf29ce484222325ull;
    for (int t = 0; t < 256; t++) {
        df_state x;
        memset(&x, 0, sizeof x);
        x.sw = (uint8_t)pr_next(&seed);
        x.cut = (uint8_t)(pr_next(&seed) & ((1u << D->ncut) - 1));
        x.ff = (uint8_t)(pr_next(&seed) & 15);
        x.tmr = (uint8_t)(pr_next(&seed) & 1);
        unsigned fire = pr_next(&seed) & 7;
        df_values a;
        df_eval(D, &x, NULL, fire & ((1u << D->ncap) - 1), &a);
        key = mix(key, (uint64_t)(a.det | a.disarm << 1 | a.powered << 2));
        for (int i = 0; i < L->nn; i++) {
            if (a.out[i]) seen1[i] = 1;
            else seen0[i] = 1;
        }
    }
    for (int i = 0; i < L->nn; i++)
        if (D->role[i] == DF_GATE && !(seen0[i] && seen1[i])) return -1;
    if (!seen1[D->det] || !seen1[D->disarm] || !seen0[D->power]) return -1;
    r->key = mix(mix(key, pack(start, 0)), (uint64_t)D->timer_ticks);
    float lose = (r->lose_first + r->dead_first) / (float)(nm ? nm : 1);
    r->score = 1.4f * r->par + 3.0f * lose + 2.5f / (float)(r->nsol ? r->nsol : 1) +
               3.0f * (r->orders > 0 ? 1 - r->ways / (float)r->orders : 0) + 1.5f * r->timing + 0.25f * r->gates +
               0.2f * r->vars + 1.0f * !r->greedy_ok;
    return 1;
}

int df_spec_ok(const df_spec *sp, const df_rating *r)
{
    if (r->par < sp->par_lo || r->par > sp->par_hi) return 0;
    if (r->ticks > (sp->secs ? sp->secs : DF_SECS) - 15) return 0;
    if (r->lose_first + r->dead_first >= r->moves_first) return 0;   /* (some way in) */
    if (sp->timing && !r->timing) return 0;
    if (sp->easy) return r->lose_first + r->dead_first > 0;
    if (r->lose_first + r->dead_first < 1) return 0;   /* a trap */
    if (r->orders < 2 || r->ways >= r->orders) return 0;   /* an order that matters */
    if (r->greedy_ok) return 0;                 /* the obvious move does not work */
    if (r->shared < 60) return 0;               /* one network: most controls reach two lamps */
    return 1;
}

/* ---- the generator ------------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, role, nin, depth, init, row, col, size;
    int src[3], cut[3], uses;
    float x;
    uint32_t supp;
} gnode;

static int range(uint32_t *s, int lo, int hi) { return hi > lo ? lo + pr_below(s, hi - lo + 1) : lo; }
static const char *const KIND[LG_KINDS] = {"s", "NOT", "AND", "OR", "XOR", "NAND", "NOR", "y", "L", "A"};

static int pick_two(uint32_t *s, unsigned kinds)
{
    int k[8], n = 0;
    for (int i = LG_AND; i <= LG_NOR; i++)
        if (kinds >> i & 1) k[n++] = i;
    return n ? k[pr_below(s, n)] : LG_AND;
}

static int put(char *out, int size, int len, const char *txt)
{
    int n = (int)strlen(txt);
    if (len + n + 1 > size) return size;
    memcpy(out + len, txt, (size_t)n + 1);
    return len + n;
}

/* a signal to take: one not used yet mostly (fan: % of a used one), near x when x >= 0 */
static float deep_w = 0.2f;                    /* how much deeper signals are preferred */
static int take_ex(uint32_t *s, gnode *N, int nn, int fan, float x, uint64_t ex)
{
    int best = -1;
    float bd = 1e9f;
    int want_used = pr_below(s, 100) < fan;
    for (int t = 0; t < 2; t++) {
        for (int i = 0; i < nn; i++) {
            if ((ex >> i & 1) || N[i].role == DF_DET || N[i].role == DF_DISARM || N[i].role == DF_POWER) continue;
            if (!t && (N[i].uses > 0) != want_used) continue;
            float d = (x >= 0 ? fabsf(N[i].x - x) : 0) + pr_below(s, 100) / 60.0f - deep_w * N[i].depth;
            if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) break;
    }
    return best;
}

static int take(uint32_t *s, gnode *N, int nn, int fan, float x, int not)
{
    return take_ex(s, N, nn, fan, x, not >= 0 ? 1ull << not : 0);
}

static gnode *add(gnode *N, int *nn, int kind, int role, int a, int b)
{
    gnode *g = &N[*nn];
    memset(g, 0, sizeof *g);
    g->kind = kind;
    g->role = role;
    g->src[0] = a;
    g->nin = 1;
    g->depth = N[a].depth + 1;
    g->x = N[a].x;
    g->supp = N[a].supp;
    N[a].uses++;
    if (b >= 0) {
        g->src[1] = b;
        g->nin = 2;
        if (N[b].depth + 1 > g->depth) g->depth = N[b].depth + 1;
        g->x = (N[a].x + N[b].x) / 2;
        g->supp |= N[b].supp;
        N[b].uses++;
    }
    (*nn)++;
    return g;
}

static int grow(uint32_t *s, const df_spec *sp, char *out, int size)
{
    deep_w = 0.2f;
    gnode N[LG_NODES];
    int nn = 0;
    int nsw = range(s, sp->sw_lo, sp->sw_hi), nsrc = nsw + 1 + (sp->timer ? 1 : 0);
    int order[LG_COLS];
    for (int i = 0; i < nsrc; i++) order[i] = i;
    pr_shuffle(s, order, nsrc);
    for (int i = 0; i < nsrc; i++) {
        int k = order[i];
        gnode *g = &N[nn];
        memset(g, 0, sizeof *g);
        g->kind = LG_SWITCH;
        g->role = k < nsw ? DF_SW : k == nsw ? DF_BATTERY : DF_TIMER;
        g->init = g->role == DF_SW ? pr_below(s, 2) : g->role == DF_BATTERY;
        g->x = (float)i;
        g->supp = 1u << nn;
        g->size = g->role == DF_TIMER ? range(s, 6, 15) : 0;
        nn++;
    }
    /* the inner parts, in a random order */
    int nff = range(s, sp->ff_lo, sp->ff_hi), ncap = range(s, sp->cap_lo, sp->cap_hi);
    int G = range(s, sp->gates_lo, sp->gates_hi), todo[LG_NODES], nt = 0;
    for (int i = 0; i < G; i++) {
        int one = (sp->kinds >> LG_NOT & 1) && pr_below(s, 100) < 22;
        todo[nt++] = one ? LG_NOT : pick_two(s, sp->kinds);
    }
    for (int i = 0; i < nff; i++) todo[nt++] = 100;
    for (int i = 0; i < ncap; i++) todo[nt++] = 101;
    pr_shuffle(s, todo, nt);
    for (int t = 0; t < nt; t++) {
        if (nn >= LG_NODES - 10) return 0;
        int k = todo[t];
        int a = take(s, N, nn, sp->fan, -1, -1);
        if (a < 0) return 0;
        if (k >= 100 || k == LG_NOT) {
            if (k == LG_NOT && N[a].kind == LG_NOT && N[a].role == DF_GATE) continue;   /* (NOT NOT) */
            if (k == 101 && N[a].role == DF_BATTERY) continue;
            gnode *g = add(N, &nn, LG_NOT, k == 100 ? DF_FLIP : k == 101 ? DF_CAP : DF_GATE, a, -1);
            if (k == 100) g->init = pr_below(s, 2);
            if (k == 101) g->size = range(s, 3, 6);
        } else {
            int b = take(s, N, nn, sp->fan, N[a].x, a);
            if (b < 0) return 0;
            if ((N[a].role == DF_BATTERY || N[b].role == DF_BATTERY) && k != LG_XOR) k = LG_AND;   /* (no gate stuck) */
            add(N, &nn, k, DF_GATE, a, b);
        }
        if (N[nn - 1].depth > 4) return 0;
    }
    /* the lamps, each from a gate of two signals or more (shared ones first): POWER an OR of two branches from the
     * battery (one move seldom puts it out), DISARM an AND of its conditions, DETONATE an OR or an AND */
    int bat = -1;
    for (int i = 0; i < nn; i++)
        if (N[i].role == DF_BATTERY) bat = i;
    int pb[2] = {-1, -1};
    for (int h = 0; h < 2; h++) {
        float pd = -1e9f;
        for (int i = 0; i < nn; i++)
            if ((N[i].supp >> bat & 1) && i != pb[0] && N[i].role != DF_CAP && N[i].kind != LG_SWITCH &&
                (!h || (N[i].supp & ~N[pb[0]].supp & ~(1u << bat)))) {   /* (the second branch fed by another switch) */
                float d = N[i].depth + pr_below(s, 100) / 40.0f - 1.5f * N[i].uses;
                if (d > pd) { pd = d; pb[h] = i; }
            }
        if (pb[h] < 0) {                        /* a branch made: the battery through an AND with a signal */
            int b = take(s, N, nn, 30, N[bat].x, bat);
            if (b < 0 || N[b].role == DF_CAP) return 0;
            add(N, &nn, LG_AND, DF_GATE, bat, b);
            pb[h] = nn - 1;
        }
    }
    add(N, &nn, LG_OR, DF_GATE, pb[0], pb[1]);
    int pw = nn - 1;
    int conds = range(s, sp->conds > 1 ? 2 : 1, sp->conds > 1 ? sp->conds : 1);
    int dc[3] = {-1, -1, -1};
    deep_w = 1.2f;                              /* (the conditions and DETONATE from deep in the network) */
    for (int c = 0; c < conds; c++) {
        uint64_t ex = 1ull << pw;
        for (int j = 0; j < c; j++) ex |= 1ull << dc[j];
        for (int i = 0; i < nn; i++)
            if (N[i].role == DF_CAP) ex |= 1ull << i;
        dc[c] = take_ex(s, N, nn, c ? 50 : 30, c ? N[dc[0]].x : -1, ex);
        if (dc[c] < 0) return 0;
    }
    int da = dc[0];
    if (conds > 1) {
        gnode *g = add(N, &nn, LG_AND, DF_GATE, dc[0], dc[1]);
        if (conds > 2 && dc[2] >= 0 && dc[2] != dc[1]) {
            g->src[2] = dc[2];
            g->nin = 3;
            N[dc[2]].uses++;
            if (N[dc[2]].depth + 1 > g->depth) g->depth = N[dc[2]].depth + 1;
            g->supp |= N[dc[2]].supp;
        }
        da = nn - 1;
    }
    uint64_t ex = 1ull << pw | 1ull << da | 1ull << bat;
    int e1 = take_ex(s, N, nn, 70, -1, ex), e2 = e1 >= 0 ? take_ex(s, N, nn, 70, N[e1].x, ex | 1ull << e1) : -1;
    if (e1 < 0 || e2 < 0) return 0;
    add(N, &nn, pr_below(s, 3) ? LG_AND : LG_OR, DF_GATE, e1, e2);
    int de = nn - 1;
    deep_w = 0.2f;
    int lamp_src[3] = {pw, da, de};
    /* every signal used: one left over joins DETONATE's or DISARM's gate through a gate */
    for (int guard = 0; guard < 20; guard++) {
        int u = -1;
        for (int i = 0; i < nn && u < 0; i++)
            if (!N[i].uses && i != pw && i != da && i != de) u = i;
        if (u < 0) break;
        if (nn >= LG_NODES - 4) return 0;
        int l = 1 + pr_below(s, 2), kind = l == 1 ? LG_AND : pick_two(s, sp->kinds & ~(1u << LG_NOT));
        if (N[u].role == DF_CAP || N[u].role == DF_BATTERY) l = 2, kind = N[u].role == DF_BATTERY ? LG_AND : LG_OR;
        gnode *t = &N[lamp_src[l]];
        if (t->role == DF_GATE && t->nin == 2 && t->kind != LG_XOR && (l == 2 || t->kind == LG_AND) &&
            N[u].depth < t->depth) {   /* a third input to the lamp's gate (no row more) */
            t->src[2] = u;
            t->nin = 3;
            t->supp |= N[u].supp;
            N[u].uses++;
            continue;
        }
        add(N, &nn, kind, DF_GATE, u, lamp_src[l]);
        lamp_src[l] = nn - 1;
    }
    for (int l = 0; l < 3; l++)
        if (N[lamp_src[l]].role == DF_CAP && l != 2) return 0;   /* a lamp lit an instant only */
    for (int l = 0; l < 3; l++)
        if (N[lamp_src[l]].role == DF_BATTERY || N[lamp_src[l]].role == DF_TIMER) return 0;
    int top = 0;
    for (int i = 0; i < nn; i++)
        if (N[i].depth > top) top = N[i].depth;
    const int ROLE[3] = {DF_POWER, DF_DISARM, DF_DET};
    for (int l = 0; l < 3; l++) {
        gnode *g = add(N, &nn, l == 2 ? LG_ALARM : LG_LAMP, ROLE[l], lamp_src[l], -1);
        g->depth = top + 1;
    }
    /* the cables: on the gates', flip-flops' and capacitors' inputs (none on a lamp's: too easy) */
    int fuses = range(s, sp->fuses_lo, sp->fuses_hi), nf = 0;
    for (int t = 0; t < 120 && nf < fuses; t++) {
        int i = pr_below(s, nn);
        if (N[i].kind == LG_SWITCH || N[i].role == DF_DET || N[i].role == DF_DISARM || N[i].role == DF_POWER) continue;
        int j = pr_below(s, N[i].nin);
        if (!N[i].cut[j]) { N[i].cut[j] = 1; nf++; }
    }
    if (nf < sp->fuses_lo) return 0;
    /* the cells: the row by depth, the columns in the order of their places */
    if (top + 1 >= LG_ROWS) return 0;
    for (int r = 0; r <= top + 1; r++) {
        int id[LG_NODES], n = 0;
        for (int i = 0; i < nn; i++)
            if (N[i].depth == r) id[n++] = i;
        if (n > LG_COLS) return 0;
        for (int a = 1; a < n; a++)
            for (int b = a; b > 0 && N[id[b - 1]].x > N[id[b]].x; b--) { int t = id[b]; id[b] = id[b - 1]; id[b - 1] = t; }
        for (int c = 0; c < n; c++) { N[id[c]].row = r; N[id[c]].col = c; }
    }
    char tok[64];
    int len = 0;
    out[0] = 0;
    if (sp->secs && sp->secs != DF_SECS) { snprintf(tok, sizeof tok, "T%d ", sp->secs); len = put(out, size, len, tok); }
    for (int i = 0; i < nn; i++) {
        const gnode *g = &N[i];
        char c = (char)('a' + g->col), r = (char)('1' + g->row);
        tok[0] = 0;
        if (g->role == DF_POWER) snprintf(tok, sizeof tok, "P%c%c ", c, r);
        if (g->role == DF_TIMER) snprintf(tok, sizeof tok, "Z%c%c:%d ", c, r, g->size);
        if (g->role == DF_FLIP) snprintf(tok, sizeof tok, "F%c%c:%d ", c, r, g->init);
        if (g->role == DF_CAP) snprintf(tok, sizeof tok, "C%c%c:%d ", c, r, g->size);
        len = put(out, size, len, tok);
    }
    len = put(out, size, len, "~");
    for (int i = 0; i < nn; i++) {
        const gnode *g = &N[i];
        const char *name = g->kind == LG_SWITCH ? (g->role == DF_BATTERY ? "K" : g->role == DF_TIMER ? "k" : g->init ? "S" : "s")
                                                : KIND[g->kind];
        snprintf(tok, sizeof tok, " %s@%c%d", name, 'a' + g->col, g->row + 1);
        len = put(out, size, len, tok);
        for (int j = 0; j < g->nin; j++) {
            const gnode *f = &N[g->src[j]];
            snprintf(tok, sizeof tok, "%c%c%d%s", j ? ',' : '(', 'a' + f->col, f->row + 1, g->cut[j] ? "x" : "");
            len = put(out, size, len, tok);
        }
        if (g->nin) len = put(out, size, len, ")");
    }
    return len < size - 1;
}

/* the text's switches set to a start (bit i: switch i on) */
static void set_start(char *out, const df_level *D, unsigned st)
{
    for (int i = 0; i < D->nsw; i++) {
        const lg_node *n = &D->L.n[D->sw[i]];
        char pat[8];
        snprintf(pat, sizeof pat, "@%c%d", 'a' + n->col, n->row + 1);
        for (char *p = strstr(out, pat); p; p = strstr(p + 1, pat))
            if (p > out && (p[-1] == 's' || p[-1] == 'S') && p[-2] == ' ' && (p[3] == ' ' || !p[3])) {
                p[-1] = st >> i & 1 ? 'S' : 's';
                break;
            }
    }
}

int df_make(uint32_t seed, const df_spec *sp, int tries, char *out, int size, df_rating *r)
{
    uint32_t s = pr_seed(seed);
    static df_level D;
    for (int t = 0; t < tries; t++) {
        if (!grow(&s, sp, out, size)) continue;
        if (!df_parse(out, &D, NULL, 0)) continue;
        if (sp->cross >= 0 && D.L.crossings > sp->cross) continue;
        /* the starts: every setting of the switches safe and not won whose par is in range (the solver's), a few of
         * them rated from a random one on */
        unsigned n = 1u << D.nsw, first = (unsigned)pr_below(&s, (int)n), good[64];
        int ng = 0;
        for (unsigned k = 0; k < n && ng < 64; k++) {
            unsigned st = (first + k) % n;
            df_state s0 = df_start(&D), x;
            s0.sw = (uint8_t)st;
            x = s0;
            df_values v;
            df_eval(&D, &x, NULL, 0, &v);
            if (v.det || v.win) continue;
            int p = df_solve(&D, &s0, NULL, NULL);
            if (p >= sp->par_lo && p <= sp->par_hi) good[ng++] = st;
        }
        for (int k = 0; k < ng && k < 6; k++) {
            df_state s0 = df_start(&D);
            s0.sw = (uint8_t)good[k];
            int res = df_rate(&D, &s0, r);
            if (res < 0) break;
            if (!res) continue;
            if (!df_spec_ok(sp, r)) continue;
            set_start(out, &D, good[k]);
            return 1;
        }
    }
    return 0;
}

int df_text_with_layout(const char *src, const df_level *D, char *out, int size)
{
    char lay[LG_LAYOUT + 2];
    if (!lg_layout_text(&D->L, lay, sizeof lay)) return 0;
    const char *bar = strchr(src, '|');
    int n = bar ? (int)(bar - src) : (int)strlen(src);
    while (n > 0 && src[n - 1] == ' ') n--;
    if (n + (int)strlen(lay) + 1 > size) return 0;
    memcpy(out, src, (size_t)n);
    strcpy(out + n, lay);
    return 1;
}

/* ---- the die tab --------------------------------------------------------------------------------------------------- */
void df_random_spec(int size, df_spec *sp)
{
    memset(sp, 0, sizeof *sp);
    sp->kinds = 1u << LG_AND | 1u << LG_OR | 1u << LG_NOT;
    sp->fan = 35;
    sp->cross = 4;
    switch (size) {
    case 0:
        sp->sw_lo = 3; sp->sw_hi = 4; sp->gates_lo = 3; sp->gates_hi = 4; sp->conds = 2; sp->fuses_hi = 1;
        sp->ff_hi = 1; sp->par_lo = 2; sp->par_hi = 4;
        break;
    case 1:
        sp->kinds |= 1u << LG_XOR;
        sp->sw_lo = 3; sp->sw_hi = 4; sp->gates_lo = 4; sp->gates_hi = 5; sp->conds = 2; sp->fuses_lo = 1;
        sp->fuses_hi = 2; sp->ff_hi = 1; sp->cap_hi = 1; sp->par_lo = 3; sp->par_hi = 5;
        break;
    default:
        sp->kinds |= 1u << LG_XOR | 1u << LG_NAND | 1u << LG_NOR;
        sp->sw_lo = 4; sp->sw_hi = 5; sp->gates_lo = 5; sp->gates_hi = 6; sp->conds = 3; sp->fuses_lo = 1;
        sp->fuses_hi = 2; sp->ff_lo = 1; sp->ff_hi = 1; sp->cap_lo = 1; sp->cap_hi = 1; sp->timer = 1;
        sp->par_lo = 4; sp->par_hi = 7; sp->secs = 90;
    }
}

int df_random(df_game *g, int size, uint32_t seed)
{
    int z = size < 0 ? 0 : size >= DF_RANDOM ? DF_RANDOM - 1 : size, k = (int)(pr_seed(seed) % DF_POOL);
    static df_level D;
    if (!df_pool[z][k] || !df_parse(df_pool[z][k], &D, NULL, 0)) return 0;
    df_load(g, &D, -1, df_pool_pars[z][k]);
    return 1;
}
