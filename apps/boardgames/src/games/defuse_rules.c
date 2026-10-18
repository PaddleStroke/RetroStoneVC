/*
 * Defuse the Bomb: the rules (defuse.h). The circuit is read and drawn by Logic Gates' rules (logicgates_rules.c);
 * here: the roles, the values of a state, the moves (a press: held, then let go), the solver (a search over the
 * states: the switches, the cuts, the latches; a move that lights DETONATE is never taken), the hint, undo, the
 * notation, and the generator of the levels' tool and the die tab.
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

/* per level: each part's switch or latch number, each wire's cable number (-1: none) */
static int8_t SWX[LG_NODES], LTX[LG_NODES], CTX[LG_WIRES];
static const df_level *indexed;

static void index_of(const df_level *D)
{
    if (indexed == D) return;
    memset(SWX, -1, sizeof SWX);
    memset(LTX, -1, sizeof LTX);
    memset(CTX, -1, sizeof CTX);
    for (int i = 0; i < D->nsw; i++) SWX[D->sw[i]] = (int8_t)i;
    for (int i = 0; i < D->nlatch; i++) LTX[D->latch[i]] = (int8_t)i;
    for (int i = 0; i < D->L.ncut; i++) CTX[D->L.cut[i]] = (int8_t)i;
    indexed = D;
}

uint32_t df_bit_switch(const df_level *D, int i) { (void)D; return 1u << i; }
uint32_t df_bit_cut(const df_level *D, int i) { return 1u << (D->nsw + i); }
static uint32_t bit_latch(const df_level *D, int i) { return 1u << (D->nsw + D->ncut + i); }

int df_parse(const char *src, df_level *D, char *err, int errsize)
{
    memset(D, 0, sizeof *D);
    indexed = NULL;
    const char *sep = strchr(src, '~'), *lg = sep ? sep + 1 : src;
    int pc = -1, pr = -1, bc[LG_NODES], br[LG_NODES], nb = 0, mc[LG_NODES], mr[LG_NODES], nm = 0;
    D->secs = DF_SECS;
    for (const char *s = src; sep && s < sep;) {
        if (*s == ' ') { s++; continue; }
        if (*s == 'T') {
            D->secs = (int)strtol(s + 1, (char **)&s, 10);
            if (D->secs < 10 || D->secs > 600) return fail(err, errsize, "a bad timer");
            continue;
        }
        int c, r;
        if ((*s != 'P' && *s != 'B' && *s != 'M') || !cell_of(s + 1, &c, &r)) return fail(err, errsize, "a bad role");
        if (*s == 'P') { pc = c; pr = r; }
        else if (*s == 'B') { bc[nb] = c; br[nb++] = r; }
        else { mc[nm] = c; mr[nm++] = r; }
        if (nb >= LG_NODES || nm >= LG_NODES) return fail(err, errsize, "too many roles");
        s += 3;
    }
    if (!lg_parse(lg, &D->L, err, errsize)) return 0;
    lg_level *L = &D->L;
    if (L->nrt) return fail(err, errsize, "a route switch");
    D->det = D->plug = D->battery = -1;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        int role = DF_GATE;
        if (n->kind == LG_SWITCH) role = n->locked ? (n->init ? DF_BATTERY : -1) : DF_SW;
        else if (n->kind == LG_ALARM) role = DF_DET;
        else if (n->kind == LG_LAMP) role = n->col == pc && n->row == pr ? DF_PLUG : DF_LED;
        if (role < 0) return fail(err, errsize, "a switch locked at 0");
        if (role == DF_DET) {
            if (D->det >= 0) return fail(err, errsize, "two DETONATE LEDs");
            D->det = i;
        }
        if (role == DF_PLUG) D->plug = i;
        if (role == DF_BATTERY) {
            if (D->battery >= 0) return fail(err, errsize, "two batteries");
            D->battery = i;
        }
        D->role[i] = (uint8_t)role;
    }
    for (int k = 0; k < nb; k++) {
        int i = node_at(L, bc[k], br[k]);
        if (i < 0 || D->role[i] != DF_SW || L->n[i].init) return fail(err, errsize, "a button not a switch at 0");
        D->role[i] = DF_BUTTON;
    }
    for (int k = 0; k < nm; k++) {
        int i = node_at(L, mc[k], mr[k]);
        if (i < 0 || L->n[i].kind != LG_NOT) return fail(err, errsize, "a latch not a NOT");
        D->role[i] = DF_LATCH;
        D->latch[D->nlatch++] = i;
    }
    if (pc >= 0 && D->plug < 0) return fail(err, errsize, "no power socket there");
    if (D->det < 0) return fail(err, errsize, "no DETONATE");
    for (int i = 0; i < L->nsw; i++)
        if (D->role[L->sw[i]] != DF_BATTERY) D->sw[D->nsw++] = L->sw[i];
    D->ncut = L->ncut;
    D->nbits = D->nsw + D->ncut + D->nlatch;
    if (D->nbits > DF_BITS) return fail(err, errsize, "too many switches, cables and latches");
    /* the disarm LEDs lettered from the left as drawn */
    int led[LG_NODES];
    for (int i = 0; i < L->nn; i++)
        if (D->role[i] == DF_LED) {
            int j = D->nled++;
            while (j > 0 && L->n[led[j - 1]].x > L->n[i].x) { led[j] = led[j - 1]; j--; }
            led[j] = i;
        }
    if (D->nled < 1 || D->nled > 4) return fail(err, errsize, "1 to 4 disarm LEDs");
    for (int k = 0; k < D->nled; k++) D->letter[led[k]] = (uint8_t)k;
    return 1;
}

static uint32_t start_of(const df_level *D)
{
    uint32_t s = 0;
    for (int i = 0; i < D->nsw; i++)
        if (D->L.n[D->sw[i]].init) s |= df_bit_switch(D, i);
    return s;
}

/* ---- the values ---------------------------------------------------------------------------------------------------- */
void df_eval(const df_level *D, uint32_t st, int press, df_values *v)
{
    index_of(D);
    const lg_level *L = &D->L;
    v->next = st;
    for (int i = 0; i < L->nn; i++) {
        const lg_node *n = &L->n[i];
        uint8_t in[3] = {0, 0, 0};
        for (int s = 0; s < n->nin; s++) {
            int w = n->in[s];
            uint8_t x = v->out[L->w[w].from];
            v->wsrc[w] = x;
            if (CTX[w] >= 0 && (st & df_bit_cut(D, CTX[w]))) x = 0;
            v->wend[w] = x;
            in[s] = x;
        }
        switch (D->role[i]) {
        case DF_SW: v->out[i] = (st >> SWX[i]) & 1; break;
        case DF_BUTTON: v->out[i] = press == SWX[i]; break;
        case DF_BATTERY: v->out[i] = 1; break;
        case DF_LATCH: {
            uint32_t b = bit_latch(D, LTX[i]);
            v->out[i] = in[0] || (st & b);
            if (in[0]) v->next |= b;
            break;
        }
        case DF_GATE: v->out[i] = (uint8_t)lg_gate(n->kind, in, n->nin); break;
        default: v->out[i] = in[0];
        }
    }
    v->powered = D->plug < 0 || v->out[D->plug];
    v->det = v->powered && v->out[D->det];
    v->lit = 0;
    for (int i = 0; i < L->nn; i++) v->lit += D->role[i] == DF_LED && v->out[i] && v->powered;
    v->win = !v->det && (!v->powered || v->lit == D->nled);
}

int df_step(const df_level *D, uint32_t st, df_move m, uint32_t *next, df_values *held)
{
    df_values v;
    uint32_t ns = st;
    if (m.kind == DF_MFLIP) {
        if (m.idx >= D->nsw || D->role[D->sw[m.idx]] != DF_SW) return -1;
        ns ^= df_bit_switch(D, m.idx);
    } else if (m.kind == DF_MCUT) {
        if (m.idx >= D->ncut || (st & df_bit_cut(D, m.idx))) return -1;
        ns |= df_bit_cut(D, m.idx);
    } else if (m.kind == DF_MPRESS) {
        if (m.idx >= D->nsw || D->role[D->sw[m.idx]] != DF_BUTTON) return -1;
        df_values h;
        df_eval(D, st, m.idx, &h);
        if (held) *held = h;
        ns = h.next;
        if (h.det || h.win) {
            *next = ns;
            return h.det ? DF_LOST : DF_WON;
        }
    } else {
        return -1;
    }
    df_eval(D, ns, -1, &v);
    *next = v.next;
    return v.det ? DF_LOST : v.win ? DF_WON : DF_SAFE;
}

int df_moves(const df_level *D, uint32_t st, df_move *m)
{
    int n = 0;
    for (int i = 0; i < D->nsw; i++)
        m[n++] = (df_move){(uint8_t)(D->role[D->sw[i]] == DF_BUTTON ? DF_MPRESS : DF_MFLIP), (uint8_t)i};
    for (int i = 0; i < D->ncut; i++)
        if (!(st & df_bit_cut(D, i))) m[n++] = (df_move){DF_MCUT, (uint8_t)i};
    return n;
}

/* a breadth-first search from st over the states the safe moves reach */
int df_solve(const df_level *D, uint32_t st, df_move *first)
{
    df_values v;
    df_eval(D, st, -1, &v);
    if (v.win) return 0;
    uint32_t N = 1u << D->nbits;
    uint8_t *seen = calloc(N / 8 + 1, 1);
    uint32_t *q = malloc(sizeof(uint32_t) * N);
    uint16_t *fm = malloc(sizeof(uint16_t) * N);
    int res = -1;
    if (!seen || !q || !fm) goto done;
    int head = 0, tail = 0, depth = 0;
    q[tail++] = st;
    seen[st >> 3] |= (uint8_t)(1 << (st & 7));
    while (head < tail && res < 0) {
        int end = tail;
        for (; head < end && res < 0; head++) {
            uint32_t s = q[head];
            df_move ms[LG_NODES + LG_WIRES];
            int nm = df_moves(D, s, ms);
            for (int k = 0; k < nm; k++) {
                uint32_t ns;
                int r = df_step(D, s, ms[k], &ns, NULL);
                if (r < 0 || r == DF_LOST) continue;
                uint16_t f = depth ? fm[s] : (uint16_t)(ms[k].kind << 8 | ms[k].idx);
                if (r == DF_WON) {
                    res = depth + 1;
                    if (first) *first = (df_move){(uint8_t)(f >> 8), (uint8_t)(f & 255)};
                    break;
                }
                if (seen[ns >> 3] & (1 << (ns & 7))) continue;
                seen[ns >> 3] |= (uint8_t)(1 << (ns & 7));
                fm[ns] = f;
                q[tail++] = ns;
            }
        }
        depth++;
    }
done:
    free(seen);
    free(q);
    free(fm);
    return res;
}

/* ---- the game ------------------------------------------------------------------------------------------------------ */
void df_load(df_game *g, const df_level *D, uint32_t start, int level, int par)
{
    if (&g->D != D) g->D = *D;
    indexed = NULL;
    g->level = level;
    g->par = par;
    g->start = start;
    df_restart(g);
}

void df_restart(df_game *g)
{
    g->state = g->start;
    g->nh = 0;
    g->hinted = 0;
    g->over = 0;
    df_eval(&g->D, g->state, -1, &g->v);
}

int df_open(df_game *g, int level)
{
    if (level < 0 || level >= DF_LEVELS || !df_levels[level]) return 0;
    static df_level D;
    if (!df_parse(df_levels[level], &D, NULL, 0)) return 0;
    df_load(g, &D, start_of(&D), level, df_pars[level]);
    return 1;
}

int df_play(df_game *g, df_move m)
{
    if (g->over) return -1;
    uint32_t ns;
    int r = df_step(&g->D, g->state, m, &ns, NULL);
    if (r < 0) return -1;
    g->state = ns;
    if (g->nh < DF_HIST) g->hist[g->nh] = m;
    g->nh++;
    df_eval(&g->D, g->state, -1, &g->v);
    if (r != DF_SAFE) g->over = r;
    return r;
}

/* undo: the moves played again from the start but the last (the latches set as they were) */
int df_undo(df_game *g)
{
    if (!g->nh || g->over == DF_LOST || g->nh > DF_HIST) return 0;
    int n = g->nh - 1, hinted = g->hinted;
    df_move h[DF_HIST];
    memcpy(h, g->hist, sizeof h);
    df_restart(g);
    for (int i = 0; i < n; i++) df_play(g, h[i]);
    g->hinted = hinted;
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
    return df_solve(&g->D, g->state, m) > 0;
}

int df_script(df_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        char c = *s++;
        if (c == 'u') { if (!df_undo(g)) return 0; continue; }
        if (c == 'x') { df_restart(g); continue; }
        if (c == 'h') {
            df_move m;
            if (!df_hint(g, &m)) return 0;
            g->hinted = 1;
            continue;
        }
        if (c == '=') {
            df_restart(g);
            df_move m;
            for (int k = 0; k < 64 && !g->over && df_solve(&g->D, g->state, &m) > 0; k++) df_play(g, m);
            if (g->over != DF_WON) return 0;
            continue;
        }
        if (c != 's' && c != 'b' && c != 'c') return 0;
        int n = 0;
        if (*s < '1' || *s > '9') return 0;
        while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
        df_move m = {(uint8_t)(c == 's' ? DF_MFLIP : c == 'b' ? DF_MPRESS : DF_MCUT), (uint8_t)(n - 1)};
        if (df_play(g, m) < 0) return 0;
    }
    return 1;
}

/* ---- the rating ---------------------------------------------------------------------------------------------------- */
static uint64_t mix(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001b3ull; }

static const df_spec *rate_spec;           /* df_make's: a start not for it, given up early */
static int spec_ok(const df_level *D, const df_spec *sp, const df_rating *r);

int df_rate(const df_level *D, uint32_t start, df_rating *r)
{
    memset(r, 0, sizeof *r);
    df_values v;
    df_eval(D, start, -1, &v);
    if (v.det || v.win || D->nbits > 15) return 0;
    df_move first;
    r->par = df_solve(D, start, &first);
    if (r->par <= 0) return 0;
    const lg_level *L = &D->L;
    r->vars = D->nbits;
    for (int i = 0; i < L->nn; i++) r->gates += D->role[i] == DF_GATE || D->role[i] == DF_LATCH;
    r->depth = L->maxdepth;
    r->crossings = L->crossings;
    /* the first moves: blowing, leaving no way, tempting */
    df_move ms[LG_NODES + LG_WIRES];
    int nm = df_moves(D, start, ms), best_lit = -1, greedy = -1;
    r->moves_first = nm;
    for (int k = 0; k < nm; k++) {
        uint32_t ns;
        df_values h;
        int res = df_step(D, start, ms[k], &ns, &h);
        df_values after;
        df_eval(D, ns, -1, &after);
        int lit = ms[k].kind == DF_MPRESS ? h.lit : after.lit;
        if (res == DF_LOST) r->lose_first++;
        else if (res == DF_SAFE && df_solve(D, ns, NULL) < 0) r->dead_first++;
        if (lit > best_lit) { best_lit = lit; greedy = k; }
    }
    if (greedy >= 0 && best_lit > v.lit) {
        uint32_t ns;
        int res = df_step(D, start, ms[greedy], &ns, NULL);
        r->greedy_trap = res == DF_LOST || (res == DF_SAFE && df_solve(D, ns, NULL) < 0);
    }
    /* the best way's moves, one of them made first */
    uint32_t s = start;
    df_move way[64];
    int nw = 0;
    while (nw < 64) {
        df_move m;
        if (df_solve(D, s, &m) <= 0) break;
        way[nw++] = m;
        uint32_t ns;
        if (df_step(D, s, m, &ns, NULL) != DF_SAFE) break;
        s = ns;
    }
    for (int k = 1; k < nw; k++) {
        uint32_t ns;
        if (df_step(D, start, way[k], &ns, NULL) == DF_LOST) r->order_trap = 1;
    }
    /* the ways in: how many first moves start a best way */
    for (int k = 0; k < nm; k++) {
        uint32_t ns;
        int res = df_step(D, start, ms[k], &ns, NULL);
        if (res == DF_WON || (res == DF_SAFE && df_solve(D, ns, NULL) == r->par - 1)) r->nsol++;
    }
    r->score = 1.6f * r->par + 0.5f * r->vars + 0.35f * r->gates + 0.4f * r->depth +
               2.5f * r->lose_first / (float)nm + 1.0f * r->dead_first + 1.5f * r->order_trap +
               1.0f * r->greedy_trap - 0.6f * (r->nsol - 1);
    if (rate_spec && !spec_ok(D, rate_spec, r)) return 0;
    /* every variable matters, no gate stuck; the key: what every state shows */
    uint32_t N = 1u << D->nbits, nv = (uint32_t)D->nbits;
    uint8_t seen0[LG_NODES] = {0}, seen1[LG_NODES] = {0};
    uint32_t matters = 0;
    int dead_power = D->plug < 0;              /* a power socket that can go dark (else it is for show) */
    uint64_t key = 0xcbf29ce484222325ull;
    for (uint32_t s = 0; s < N; s++) {
        df_eval(D, s, -1, &v);
        int sig = v.det | v.win << 1 | v.powered << 2 | v.lit << 3;
        dead_power |= !v.powered;
        key = mix(key, (uint64_t)sig);
        for (int i = 0; i < L->nn; i++) {
            if (v.out[i]) seen1[i] = 1;
            else seen0[i] = 1;
        }
        for (uint32_t b = 0; b < nv; b++) {
            if (matters >> b & 1) continue;
            df_values u;
            df_eval(D, s ^ (1u << b), -1, &u);
            if ((u.det | u.win << 1 | u.powered << 2 | u.lit << 3) != sig) matters |= 1u << b;
        }
        for (int k = 0; k < D->nsw; k++) {      /* a button: pressed in this state */
            if (D->role[D->sw[k]] != DF_BUTTON) continue;
            df_values u;
            df_eval(D, s, k, &u);
            for (int i = 0; i < L->nn; i++) {
                if (u.out[i]) seen1[i] = 1;
                else seen0[i] = 1;
            }
            if ((u.det | u.win << 1 | u.powered << 2 | u.lit << 3) != sig || u.next != s) matters |= 1u << k;
            key = mix(key, (uint64_t)(u.det | u.win << 1) + 7);
        }
    }
    if (!dead_power) return -1;
    for (int b = 0; b < D->nbits; b++)
        if (!(matters >> b & 1)) return -1;    /* a switch, a cable or a latch that never matters */
    for (int i = 0; i < L->nn; i++)
        if ((D->role[i] == DF_GATE || D->role[i] == DF_LATCH) && !(seen0[i] && seen1[i])) return -1;
    r->key = mix(key, start);
    return 1;
}

/* ---- the generator ------------------------------------------------------------------------------------------------- */
typedef struct {
    int kind, role, nin, depth, init, row, col;
    int src[3], cut[3];
    float x;
    uint32_t supp;                              /* the sources it depends on (bit: a source's node) */
} gnode;
typedef struct { int node, used, ttl; } gsig;

static int range(uint32_t *s, int lo, int hi) { return hi > lo ? lo + pr_below(s, hi - lo + 1) : lo; }
static const char *const KIND[LG_KINDS] = {"s", "NOT", "AND", "OR", "XOR", "NAND", "NOR", "y", "L", "A"};

static int pick_kind(uint32_t *s, unsigned kinds)
{
    int k[8], n = 0;
    for (int i = LG_AND; i <= LG_NOR; i++)
        if (kinds >> i & 1) k[n++] = i;
    return n ? k[pr_below(s, n)] : -1;
}

static int put(char *out, int size, int len, const char *txt)
{
    int n = (int)strlen(txt);
    if (len + n + 1 > size) return size;
    memcpy(out + len, txt, (size_t)n + 1);
    return len + n;
}

static int grow(uint32_t *s, const df_spec *sp, char *out, int size)
{
    gnode N[LG_NODES];
    gsig q[2 * LG_NODES];
    int nn = 0, nq = 0;
    memset(N, 0, sizeof N);
    int nsw = range(s, sp->sw_lo, sp->sw_hi), nsrc = nsw + sp->btn + (sp->battery || sp->plug ? 1 : 0);
    if (nsrc > LG_COLS) return 0;
    int order[LG_COLS];
    for (int i = 0; i < nsrc; i++) order[i] = i;
    pr_shuffle(s, order, nsrc);
    for (int i = 0; i < nsrc; i++) {
        int k = order[i];
        int role = k < nsw ? DF_SW : k < nsw + sp->btn ? DF_BUTTON : DF_BATTERY;
        N[nn] = (gnode){.kind = LG_SWITCH, .role = role, .x = (float)i, .init = role == DF_SW ? pr_below(s, 2) : role == DF_BATTERY};
        N[nn].supp = 1u << nn;
        q[nq++] = (gsig){nn++, 0, 0};
    }
    /* the latches, each over a button (its signal still free for a gate beside: the press's danger) */
    int nl = 0;
    for (int i = 0; i < nq && nl < sp->latches; i++) {
        if (N[q[i].node].role != DF_BUTTON) continue;
        gnode *g = &N[nn];
        *g = (gnode){.kind = LG_NOT, .role = DF_LATCH, .nin = 1, .depth = 1, .x = N[q[i].node].x + 0.1f};
        g->src[0] = q[i].node;
        g->supp = N[q[i].node].supp;
        q[i].used = 1;
        q[i].ttl = 3;
        memmove(q + i + 2, q + i + 1, sizeof(gsig) * (size_t)(nq - i - 1));
        q[i + 1] = (gsig){nn++, 0, 0};
        nq++;
        nl++;
        i++;
    }
    int leds = range(s, sp->leds_lo, sp->leds_hi), k = leds + 1 + (sp->plug ? 1 : 0);
    int G = range(s, sp->gates_lo, sp->gates_hi), gates = 0;
    unsigned two = sp->kinds & ~(1u << LG_NOT);
    for (int step = 0;; step++) {
        int un = 0;
        for (int i = 0; i < nq; i++) un += !q[i].used;
        if (un == k && gates >= G) break;
        if (step > 150 || un < k || nn >= LG_NODES - 6) return 0;
        int need = un - k, spare = G - gates - need, act;
        int can_not = sp->kinds >> LG_NOT & 1;
        if (need > 0 && (spare <= 0 || pr_below(s, 100) < 55)) act = 1;   /* a join */
        else if (spare > 0 && can_not && pr_below(s, 100) < 45) act = 2;   /* a NOT */
        else if (spare > 0 && two) act = 3;                                /* a branching */
        else if (need < 0) return 0;
        else continue;
        int a = pr_below(s, nq);
        for (int i = 0; i < nq; i++)            /* the battery's signal taken first, half the time */
            if (!q[i].used && N[q[i].node].role == DF_BATTERY && act != 2 && pr_below(s, 2)) a = i;
        if (q[a].used) continue;
        int in[3] = {a, 0, 0}, nin = 1, kind;
        if (act == 2) {
            int fk = N[q[a].node].kind;
            if (fk == LG_NOT || N[q[a].node].role == DF_LATCH || N[q[a].node].role == DF_BATTERY) continue;
            kind = LG_NOT;
        } else if (act == 3) {
            int b = pr_below(s, 2) ? a - 1 : a + 1;
            if (b < 0 || b >= nq || q[b].node == q[a].node) continue;
            if (!q[b].used && need <= 0) continue;   /* (a neighbour still free: a join, not a branching) */
            in[nin++] = b;
            kind = pick_kind(s, two);
        } else {
            if (!two) continue;
            int dir = pr_below(s, 2) ? -1 : 1;
            for (int pass = 0; pass < 2 && nin < 2; pass++, dir = -dir)
                for (int b = a + dir; b >= 0 && b < nq && nin < 2; b += dir)
                    if (!q[b].used) in[nin++] = b;
            if (nin < 2) continue;
            kind = pick_kind(s, two);
        }
        if (kind < 0) return 0;
        int depth = 0;
        float x = 0;
        uint32_t supp = 0;
        for (int j = 0; j < nin; j++) {
            const gnode *f = &N[q[in[j]].node];
            if (f->depth > depth) depth = f->depth;
            x += f->x;
            supp |= f->supp;
        }
        if (nin == 2 && in[1] < in[0]) { int t = in[0]; in[0] = in[1]; in[1] = t; }
        if (++depth > sp->depth_hi) continue;
        gnode *g = &N[nn];
        *g = (gnode){.kind = kind, .role = DF_GATE, .nin = nin, .depth = depth, .x = x / nin, .supp = supp};
        for (int j = 0; j < nin; j++) g->src[j] = q[in[j]].node;
        gsig q2[2 * LG_NODES];
        int m = 0, placed = 0;
        for (int i = 0; i < nq; i++) {
            int taken = 0;
            for (int j = 0; j < nin; j++) taken |= in[j] == i;
            gsig t = q[i];
            if (taken && (act != 3 || i == a)) {
                if (!placed) { q2[m++] = (gsig){nn, 0, 0}; placed = 1; }
                continue;
            }
            if (taken) { t.used = 1; t.ttl = 3; }
            if (t.used && --t.ttl <= 0) continue;
            q2[m++] = t;
        }
        memcpy(q, q2, sizeof(gsig) * (size_t)m);
        nq = m;
        nn++;
        gates++;
    }
    /* the outputs on the signals left: DETONATE, the power socket (on a signal from the battery), the LEDs */
    int top = 0, idx[LG_NODES], ni = 0;
    for (int i = 0; i < nn; i++)
        if (N[i].depth > top) top = N[i].depth;
    for (int i = 0; i < nq; i++)
        if (!q[i].used) idx[ni++] = i;
    int bat = -1;
    for (int i = 0; i < nn; i++)
        if (N[i].role == DF_BATTERY) bat = i;
    int plug_at = -1;
    if (sp->plug) {
        int c[LG_NODES], nc = 0;
        for (int j = 0; j < ni; j++)
            if (N[q[idx[j]].node].supp >> bat & 1) c[nc++] = j;
        if (!nc) return 0;
        plug_at = c[pr_below(s, nc)];
    } else if (bat >= 0) {                      /* the battery used somewhere */
        int used = 0;
        for (int i = 0; i < nn; i++)
            for (int j = 0; j < N[i].nin; j++) used |= N[i].src[j] == bat;
        for (int j = 0; j < ni; j++) used |= q[idx[j]].node == bat;
        if (!used) return 0;
    }
    int det_at;
    do det_at = pr_below(s, ni); while (det_at == plug_at);
    for (int j = 0; j < ni; j++) {
        int src = q[idx[j]].node;
        if (N[src].role == DF_BATTERY && j != plug_at) return 0;   /* a LED always lit, DETONATE always lit */
        if (N[src].role == DF_BUTTON) return 0;                     /* a LED lit only while pressed */
        N[nn] = (gnode){.kind = j == det_at ? LG_ALARM : LG_LAMP, .role = j == det_at ? DF_DET : j == plug_at ? DF_PLUG : DF_LED,
                        .nin = 1, .depth = top + 1, .x = N[src].x, .src = {src, 0, 0}};
        nn++;
    }
    /* the cables: on the gates', the LEDs' and the socket's inputs (not DETONATE's own: no puzzle left) */
    int fuses = range(s, sp->fuses_lo, sp->fuses_hi), nf = 0;
    if (sp->plug && bat >= 0 && fuses > 0)  /* the power: a cable on the battery's way (never right at the socket: too easy) */
        for (int i = 0; i < nn && !nf; i++)
            for (int j = 0; j < N[i].nin && !nf; j++)
                if (N[i].src[j] == bat && N[i].role != DF_PLUG) { N[i].cut[j] = 1; nf++; }
    for (int t = 0; t < 120 && nf < fuses; t++) {
        int i = pr_below(s, nn);
        if (N[i].kind == LG_SWITCH || N[i].role == DF_DET || N[i].role == DF_LATCH || N[i].role == DF_PLUG) continue;
        int j = pr_below(s, N[i].nin);
        if (!N[i].cut[j]) { N[i].cut[j] = 1; nf++; }
    }
    if (nf < sp->fuses_lo) return 0;
    /* the cells: the row by depth, the columns in the order of their places */
    for (int r = 0; r <= top + 1; r++) {
        int id[LG_NODES], n = 0;
        for (int i = 0; i < nn; i++)
            if (N[i].depth == r) id[n++] = i;
        if (n > LG_COLS || r >= LG_ROWS) return 0;
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
        if (g->role == DF_PLUG) { snprintf(tok, sizeof tok, "P%c%c ", c, r); len = put(out, size, len, tok); }
        if (g->role == DF_BUTTON) { snprintf(tok, sizeof tok, "B%c%c ", c, r); len = put(out, size, len, tok); }
        if (g->role == DF_LATCH) { snprintf(tok, sizeof tok, "M%c%c ", c, r); len = put(out, size, len, tok); }
    }
    len = put(out, size, len, "~");
    for (int i = 0; i < nn; i++) {
        const gnode *g = &N[i];
        const char *name = g->kind == LG_SWITCH ? (g->role == DF_BATTERY ? "K" : g->init ? "S" : "s") : KIND[g->kind];
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

static int spec_ok(const df_level *D, const df_spec *sp, const df_rating *r)
{
    unsigned have = 0;
    for (int i = 0; i < D->L.nn; i++)
        if (D->role[i] == DF_GATE) have |= 1u << D->L.n[i].kind;
    if ((have & sp->need) != sp->need) return 0;
    if (r->par < sp->par_lo || r->par > sp->par_hi) return 0;
    if (sp->trap && !r->order_trap && !r->greedy_trap) return 0;
    if (sp->dead && !r->dead_first) return 0;
    if (r->lose_first == 0) return 0;           /* something always blows */
    if (r->lose_first == r->moves_first) return 0;
    if (sp->cross >= 0 && D->L.crossings > sp->cross) return 0;
    return 1;
}

/* the text's switches set to a start (bit i: switch i on) */
static void set_start(char *out, const df_level *D, uint32_t st)
{
    for (int i = 0; i < D->nsw; i++) {
        const lg_node *n = &D->L.n[D->sw[i]];
        if (D->role[D->sw[i]] != DF_SW) continue;
        char pat[8];
        snprintf(pat, sizeof pat, "@%c%d", 'a' + n->col, n->row + 1);
        for (char *p = strstr(out, pat); p; p = strstr(p + 1, pat))
            if (p > out && (p[-1] == 's' || p[-1] == 'S') && (p - 1 == out || p[-2] == ' ') &&
                (p[3] == ' ' || !p[3])) {
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
        if (D.nbits > 15 || D.nsw > 8) continue;
        if (sp->cross >= 0 && D.L.crossings > sp->cross) continue;
        /* the start: a setting of the switches safe and not won, its par in range (one of them at random) */
        uint32_t ok[256], nok = 0, mask = 0;
        for (int i = 0; i < D.nsw; i++)
            if (D.role[D.sw[i]] == DF_SW) mask |= 1u << i;
        for (uint32_t st = 0; st < (1u << D.nsw); st++) {
            if (st & ~mask) continue;
            df_values v;
            df_eval(&D, st, -1, &v);
            if (v.det || v.win) continue;
            int p = df_solve(&D, st, NULL);
            if (p >= sp->par_lo && p <= sp->par_hi) ok[nok++] = st;
        }
        if (!nok) continue;
        int first = pr_below(&s, (int)nok);
        for (uint32_t k = 0; k < nok; k++) {    /* from a random one on, the first that makes a good puzzle */
            uint32_t st = ok[(first + k) % nok];
            rate_spec = sp;
            int ok = df_rate(&D, st, r);
            rate_spec = NULL;
            if (ok < 0) break;                  /* (the circuit's fault: every start alike) */
            if (!ok) continue;
            set_start(out, &D, st);
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

/* ---- the die tab ------------------------------------------------------------------------------------------------------ */
void df_random_spec(int size, df_spec *sp)
{
    memset(sp, 0, sizeof *sp);
    sp->kinds = 1u << LG_AND | 1u << LG_OR | 1u << LG_NOT;
    sp->depth_hi = 4;
    sp->cross = 2;
    sp->trap = 0;                               /* (the levels teach the traps; random play: something blows) */
    switch (size) {
    case 0:
        sp->sw_lo = 3; sp->sw_hi = 4; sp->gates_lo = 2; sp->gates_hi = 4; sp->leds_lo = 1; sp->leds_hi = 2;
        sp->fuses_hi = 1; sp->par_lo = 2; sp->par_hi = 4;
        break;
    case 1:
        sp->kinds |= 1u << LG_XOR | 1u << LG_NAND;
        sp->sw_lo = 3; sp->sw_hi = 4; sp->gates_lo = 3; sp->gates_hi = 5; sp->leds_lo = 2; sp->leds_hi = 2;
        sp->fuses_lo = 1; sp->fuses_hi = 2; sp->battery = 1; sp->plug = 1; sp->par_lo = 2; sp->par_hi = 6;
        break;
    default:
        sp->kinds |= 1u << LG_XOR | 1u << LG_NAND | 1u << LG_NOR;
        sp->sw_lo = 4; sp->sw_hi = 5; sp->gates_lo = 4; sp->gates_hi = 7; sp->leds_lo = 3; sp->leds_hi = 3;
        sp->fuses_lo = 1; sp->fuses_hi = 2; sp->battery = 1; sp->btn = 1; sp->latches = 1; sp->par_lo = 3; sp->par_hi = 8;
        sp->depth_hi = 5;
        sp->cross = 3;
    }
}

int df_random(df_game *g, int size, uint32_t seed)
{
    df_spec sp;
    df_rating r;
    char txt[DF_TEXT];
    df_random_spec(size < 0 ? 0 : size >= DF_RANDOM ? DF_RANDOM - 1 : size, &sp);
    for (int k = 0; k < 40; k++)
        if (df_make(seed + 7919u * (uint32_t)k, &sp, 150, txt, sizeof txt, &r)) {
            static df_level D;
            if (!df_parse(txt, &D, NULL, 0)) continue;
            df_load(g, &D, start_of(&D), -1, r.par);
            return 1;
        }
    return 0;
}
