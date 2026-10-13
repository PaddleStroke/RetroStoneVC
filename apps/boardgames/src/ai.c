/*
 * RetroStone Board's AI kit (ai.h): alpha-beta with chance nodes, UCT with determinisation, the levels, the cancel
 * flag, a small random generator. No SDL (the tests link it alone); no state shared between two searches but the
 * cancel flag and the session seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define _POSIX_C_SOURCE 200809L
#include "ai.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ---- the random generator: splitmix64 --------------------------------------------------------------------------- */
uint64_t ai_mix(uint64_t a, uint64_t b)
{
    uint64_t z = a + 0x9e3779b97f4a7c15ull * (b + 1);
    z = (z ^ z >> 30) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ z >> 27) * 0x94d049bb133111ebull;
    return z ^ z >> 31;
}

void ai_seed(ai_rng *r, uint64_t seed) { r->s = ai_mix(seed, 0x5eed); }

static uint64_t next64(ai_rng *r)
{
    uint64_t z = (r->s += 0x9e3779b97f4a7c15ull);
    z = (z ^ z >> 30) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ z >> 27) * 0x94d049bb133111ebull;
    return z ^ z >> 31;
}

uint32_t ai_rand(ai_rng *r) { return (uint32_t)(next64(r) >> 32); }
int ai_rand_n(ai_rng *r, int n) { return n <= 1 ? 0 : (int)(((next64(r) >> 32) * (uint64_t)n) >> 32); }
double ai_rand_f(ai_rng *r) { return (double)(next64(r) >> 11) * (1.0 / 9007199254740992.0); }

/* written by the main thread before it starts the worker (the thread's creation orders it), read by the worker */
static uint64_t session_seed = 1;
void ai_set_seed(uint64_t seed) { session_seed = seed; }
uint64_t ai_session_seed(void) { return session_seed; }

/* ---- cancel, clock ----------------------------------------------------------------------------------------------- */
static atomic_int cancel_flag;
void ai_cancel(void) { atomic_store_explicit(&cancel_flag, 1, memory_order_relaxed); }
void ai_cancel_clear(void) { atomic_store_explicit(&cancel_flag, 0, memory_order_relaxed); }
int ai_cancelled(void) { return atomic_load_explicit(&cancel_flag, memory_order_relaxed); }

double ai_now_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
#endif
}

/* ---- levels ------------------------------------------------------------------------------------------------------ */
ai_budget ai_level_budget(int level)
{
    static const ai_budget B[AI_LEVELS] = {
        [AI_EASY]   = {.ms = 300, .nodes = 4000, .depth = 2, .noise = 150},
        [AI_MEDIUM] = {.ms = 600, .nodes = 60000, .depth = 5, .noise = 30},
        [AI_HARD]   = {.ms = 1000, .nodes = 500000, .depth = 0, .noise = 0},
    };
    return B[level < 0 ? 0 : level >= AI_LEVELS ? AI_LEVELS - 1 : level];
}

int ai_random_move(const ai_game *g, const void *st, ai_rng *r)
{
    int mv[AI_MAX_MOVES], n = g->moves(st, mv);
    return n ? mv[ai_rand_n(r, n)] : AI_NONE;
}

/* ---- alpha-beta -------------------------------------------------------------------------------------------------- */
enum { TT_EXACT = 1, TT_LOWER, TT_UPPER };
typedef struct { uint64_t key; int32_t score, move; int8_t depth; uint8_t flag; } tt_entry;

#define HIST_BITS 12

typedef struct {
    const ai_game *g;
    size_t size;
    uint8_t *stack;                             /* copy-make: a state a ply */
    int *mv, *sc;                               /* a ply's moves and their order (or a chance node's weights) */
    int killer[AI_MAX_PLY][2];
    int hist[2][1 << HIST_BITS];
    tt_entry *tt;
    uint64_t tt_mask;
    long nodes, cap;
    double deadline;
    int stop;
} abx;

static int tick(abx *c)
{
    if (c->stop) return 1;
    if ((++c->nodes & 1023) == 0 && (ai_cancelled() || (c->deadline > 0 && ai_now_ms() >= c->deadline))) c->stop = 1;
    if (c->cap && c->nodes >= c->cap) c->stop = 1;
    return c->stop;
}

/* a game over's score at this ply: the quicker win, the slower loss */
static int at_ply(int s, int ply) { return s > AI_WIN / 2 ? s - ply : s < -AI_WIN / 2 ? s + ply : s; }
static int to_tt(int s, int ply) { return s > AI_WIN / 2 ? s + ply : s < -AI_WIN / 2 ? s - ply : s; }

static int hist_i(int m) { return (int)(((uint32_t)m * 0x9e3779b1u) >> (32 - HIST_BITS)); }

static void *child(abx *c, void *st, int ply, int m)
{
    if (c->g->unmake) { c->g->make(st, m); return st; }
    void *ch = c->stack + (size_t)(ply + 1) * c->size;
    memcpy(ch, st, c->size);
    c->g->make(ch, m);
    return ch;
}

static void back(abx *c, void *st, int m)
{
    if (c->g->unmake) c->g->unmake(st, m);
}

/* the moves' order: the table's move, then order() (captures...), then the killers, then history */
static void score_moves(abx *c, const void *st, int *mv, int *sc, int n, int ply, int ttmove)
{
    int side = c->g->side(st) & 1;
    for (int i = 0; i < n; i++) {
        int m = mv[i], s;
        if (m == ttmove) s = 0x7fffffff;      /* above any order() * 4096 */
        else {
            s = c->g->order ? c->g->order(st, m) * 4096 : 0;
            if (ply < AI_MAX_PLY && (m == c->killer[ply][0] || m == c->killer[ply][1]))
                s += m == c->killer[ply][0] ? 4000 : 3990;
            else {
                int h = c->hist[side][hist_i(m)];
                s += h > 3900 ? 3900 : h;
            }
        }
        sc[i] = s;
    }
}

static int pick(int *mv, int *sc, int i, int n)  /* brings the best of i..n-1 to i */
{
    int b = i;
    for (int j = i + 1; j < n; j++)
        if (sc[j] > sc[b]) b = j;
    int t = mv[i]; mv[i] = mv[b]; mv[b] = t;
    t = sc[i]; sc[i] = sc[b]; sc[b] = t;
    return mv[i];
}

static int search(abx *c, void *st, int depth, int ply, int alpha, int beta);

/* the child's score in the parent's view: the sign flips only when the side to move changes */
static int sub(abx *c, void *ch, int same, int depth, int ply, int alpha, int beta)
{
    return same ? search(c, ch, depth, ply, alpha, beta) : -search(c, ch, depth, ply, -beta, -alpha);
}

static int quiesce(abx *c, void *st, int ply, int alpha, int beta)
{
    const ai_game *g = c->g;
    int s;
    if (tick(c)) return 0;
    if (g->over(st, &s)) return at_ply(s, ply);
    int stand = g->eval(st);
    if (ply >= AI_MAX_PLY - 1 || stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    int *mv = c->mv + (size_t)ply * AI_MAX_MOVES, *sc = c->sc + (size_t)ply * AI_MAX_MOVES;
    int n = g->noisy(st, mv), me = g->side(st), best = stand;
    for (int i = 0; i < n; i++) sc[i] = g->order ? g->order(st, mv[i]) : 0;
    for (int i = 0; i < n; i++) {
        int m = pick(mv, sc, i, n);
        void *ch = child(c, st, ply, m);
        int same = g->side(ch) == me;
        s = same ? quiesce(c, ch, ply + 1, alpha, beta) : -quiesce(c, ch, ply + 1, -beta, -alpha);
        back(c, st, m);
        if (c->stop) return 0;
        if (s > best) best = s;
        if (s > alpha) alpha = s;
        if (alpha >= beta) break;
    }
    return best;
}

/* a chance node: the average of its outcomes, each searched with a full window (a throw costs no depth) */
static int chance_node(abx *c, void *st, int n, int depth, int ply)
{
    int *out = c->mv + (size_t)ply * AI_MAX_MOVES, *w = c->sc + (size_t)ply * AI_MAX_MOVES, me = c->g->side(st);
    double sum = 0, total = 0;
    for (int i = 0; i < n; i++) {
        int m = out[i];
        void *ch = child(c, st, ply, m);
        int s = sub(c, ch, c->g->side(ch) == me, depth, ply + 1, -AI_INF, AI_INF);
        back(c, st, m);
        if (c->stop) return 0;
        sum += (double)w[i] * s;
        total += w[i];
    }
    return total > 0 ? (int)lround(sum / total) : 0;
}

static int search(abx *c, void *st, int depth, int ply, int alpha, int beta)
{
    const ai_game *g = c->g;
    int s;
    if (tick(c)) return 0;
    if (g->over(st, &s)) return at_ply(s, ply);
    if (ply >= AI_MAX_PLY - 1) return g->eval(st);
    if (depth <= 0) return g->noisy ? quiesce(c, st, ply, alpha, beta) : g->eval(st);
    if (g->chance) {
        int n = g->chance(st, c->mv + (size_t)ply * AI_MAX_MOVES, c->sc + (size_t)ply * AI_MAX_MOVES);
        if (n) return chance_node(c, st, n, depth, ply);
    }
    uint64_t key = 0;
    tt_entry *e = NULL;
    int ttmove = AI_NONE;
    if (c->tt) {
        key = g->hash(st);
        e = &c->tt[key & c->tt_mask];
        if (e->key == key && e->flag) {
            ttmove = e->move;
            if (e->depth >= depth) {
                int t = at_ply(e->score, ply);
                if (e->flag == TT_EXACT || (e->flag == TT_LOWER && t >= beta) || (e->flag == TT_UPPER && t <= alpha))
                    return t;
            }
        }
    }
    int *mv = c->mv + (size_t)ply * AI_MAX_MOVES, *sc = c->sc + (size_t)ply * AI_MAX_MOVES;
    int n = g->moves(st, mv);
    if (!n) return g->eval(st);
    score_moves(c, st, mv, sc, n, ply, ttmove);
    int me = g->side(st), best = -AI_INF, bestm = mv[0], alpha0 = alpha;
    for (int i = 0; i < n; i++) {
        int m = pick(mv, sc, i, n);
        void *ch = child(c, st, ply, m);
        int same = g->side(ch) == me;
        if (i == 0) s = sub(c, ch, same, depth - 1, ply + 1, alpha, beta);
        else {                                  /* the others: a null window first (principal variation search) */
            s = sub(c, ch, same, depth - 1, ply + 1, alpha, alpha + 1);
            if (s > alpha && s < beta && !c->stop) s = sub(c, ch, same, depth - 1, ply + 1, alpha, beta);
        }
        back(c, st, m);
        if (c->stop) return 0;
        if (s > best) { best = s; bestm = m; }
        if (s > alpha) alpha = s;
        if (alpha >= beta) {
            if (ply < AI_MAX_PLY && m != c->killer[ply][0]) {
                c->killer[ply][1] = c->killer[ply][0];
                c->killer[ply][0] = m;
            }
            int *h = &c->hist[me & 1][hist_i(m)];
            *h += depth * depth;
            if (*h > 1 << 20)                   /* the history ages: all of it halved */
                for (int k = 0; k < 2 << HIST_BITS; k++) c->hist[k >> HIST_BITS][k & ((1 << HIST_BITS) - 1)] /= 2;
            break;
        }
    }
    if (e) {
        e->key = key;
        e->score = to_tt(best, ply);
        e->move = bestm;
        e->depth = (int8_t)(depth > 127 ? 127 : depth);
        e->flag = best <= alpha0 ? TT_UPPER : best >= beta ? TT_LOWER : TT_EXACT;
    }
    return best;
}

int ai_ab(const ai_game *g, const void *root, const ai_opts *o, ai_stats *stats)
{
    double t0 = ai_now_ms();
    ai_rng own, *rng = o && o->rng ? o->rng : (ai_seed(&own, 1), &own);
    ai_budget b = o ? o->budget : ai_level_budget(AI_HARD);
    if (stats) memset(stats, 0, sizeof *stats);
    int rm[AI_MAX_MOVES], rs[AI_MAX_MOVES], w[AI_MAX_MOVES];
    if (g->chance && g->chance(root, rm, w)) return AI_NONE;   /* a throw to make: not a decision */
    int n = g->moves(root, rm), s;
    if (!n || g->over(root, &s)) return n ? rm[0] : AI_NONE;
    abx *c = calloc(1, sizeof *c);
    if (!c) return rm[0];
    c->g = g;
    c->size = g->size;
    c->stack = malloc((AI_MAX_PLY + 2) * g->size);
    c->mv = malloc(sizeof(int) * AI_MAX_PLY * AI_MAX_MOVES);
    c->sc = malloc(sizeof(int) * AI_MAX_PLY * AI_MAX_MOVES);
    int bits = o && g->hash ? o->tt_bits : 0;
    if (bits > 0) {
        bits = bits > 26 ? 26 : bits;
        c->tt = calloc((size_t)1 << bits, sizeof(tt_entry));
        c->tt_mask = ((uint64_t)1 << bits) - 1;
    }
    if (!c->stack || !c->mv || !c->sc) {
        free(c->stack); free(c->mv); free(c->sc); free(c->tt); free(c);
        return rm[0];
    }
    for (int i = 0; i < AI_MAX_PLY; i++) c->killer[i][0] = c->killer[i][1] = AI_NONE;
    c->cap = b.nodes;
    c->deadline = b.ms > 0 ? t0 + b.ms : 0;
    void *st = c->stack;
    memcpy(st, root, g->size);

    /* the root's moves: shuffled (ties go differently from one seed to another), then by order() */
    for (int i = n - 1; i > 0; i--) {
        int j = ai_rand_n(rng, i + 1), t = rm[i];
        rm[i] = rm[j];
        rm[j] = t;
    }
    for (int i = 0; i < n; i++) rs[i] = g->order ? g->order(st, rm[i]) : 0;
    for (int i = 1; i < n; i++)                 /* a stable sort: insertion */
        for (int j = i; j > 0 && rs[j] > rs[j - 1]; j--) {
            int t = rm[j]; rm[j] = rm[j - 1]; rm[j - 1] = t;
            t = rs[j]; rs[j] = rs[j - 1]; rs[j - 1] = t;
        }

    int unit = g->unit > 0 ? g->unit : 100, amp = b.noise > 0 ? (int)((long)b.noise * unit / 100) : 0;
    int maxd = b.depth > 0 && b.depth < AI_MAX_PLY - 1 ? b.depth : AI_MAX_PLY - 2;
    int me = g->side(st), best = rm[0], last = 0, done = 0;
    int score[AI_MAX_MOVES];
    for (int depth = 1; depth <= maxd; depth++) {
        int alpha = -AI_INF, beta = AI_INF, asp = !amp && o && o->aspiration > 0 && done && !ai_is_win(last);
        if (asp) { alpha = last - o->aspiration; beta = last + o->aspiration; }
        for (;;) {
            int a = alpha, ibest = AI_NONE, iscore = -AI_INF, first_done = 0;
            for (int i = 0; i < n; i++) {
                int m = rm[i];
                void *ch = child(c, st, 0, m);
                int same = g->side(ch) == me, lo = amp ? (iscore > -AI_INF ? iscore - amp - 1 : -AI_INF) : a;
                if (amp && lo < alpha) lo = alpha;
                if (i == 0 || amp) s = sub(c, ch, same, depth - 1, 1, lo, beta);
                else {
                    s = sub(c, ch, same, depth - 1, 1, a, a + 1);
                    if (s > a && s < beta && !c->stop) s = sub(c, ch, same, depth - 1, 1, a, beta);
                }
                back(c, st, m);
                if (c->stop) break;
                score[i] = s;
                if (i == 0) first_done = 1;
                if (s > iscore) { iscore = s; ibest = m; }
                if (s > a) a = s;
                if (a >= beta && !amp) break;
            }
            if (c->stop) {                      /* a better move found before the cut: deeper than the last best */
                if (first_done && ibest != AI_NONE && ibest != rm[0] && iscore > score[0]) best = ibest;
                break;
            }
            if (asp && (iscore <= alpha || iscore >= beta)) {   /* outside the window: again, all of it */
                alpha = -AI_INF;
                beta = AI_INF;
                asp = 0;
                continue;
            }
            /* complete: the best first for the next depth (stable: the others keep their order) */
            for (int i = 0; i < n; i++)
                if (rm[i] == ibest) {
                    for (int j = i; j > 0; j--) {
                        int t = rm[j]; rm[j] = rm[j - 1]; rm[j - 1] = t;
                        t = score[j]; score[j] = score[j - 1]; score[j - 1] = t;
                    }
                    break;
                }
            best = ibest;
            last = iscore;
            done = depth;
            break;
        }
        if (c->stop || ai_is_win(last)) break;
    }
    if (amp && done && !ai_is_win(last)) {      /* the easy levels: noise among the moves of the last depth */
        int bi = -1;
        long bv = 0;
        for (int i = 0; i < n; i++) {
            if (score[i] < last - amp || ai_is_win(score[i])) continue;
            long v = score[i] + ai_rand_n(rng, amp + 1);
            if (bi < 0 || v > bv) { bi = i; bv = v; }
        }
        if (bi >= 0) best = rm[bi];
    }
    if (stats) {
        stats->nodes = c->nodes;
        stats->depth = done;
        stats->score = last;
        stats->stopped = c->stop;
        stats->ms = ai_now_ms() - t0;
    }
    free(c->stack); free(c->mv); free(c->sc); free(c->tt); free(c);
    return best;
}

/* ---- UCT (information-set MCTS when the game deals the unseen) ------------------------------------------------- */
typedef struct { int move, child, next, visits, avail; float reward; int8_t player; } mnode;

#define MSLOTS 1024                             /* a node's legal moves, looked up by move: a small hash table */

typedef struct {
    int key[MSLOTS], idx[MSLOTS];
    unsigned gen[MSLOTS], now;
} mset;

static int mslot(int m) { return (int)(((uint32_t)m * 0x9e3779b1u) >> 22); }

static void mset_fill(mset *h, const int *mv, int n)
{
    h->now++;
    for (int i = 0; i < n; i++) {
        int k = mslot(mv[i]);
        while (h->gen[k] == h->now) k = (k + 1) & (MSLOTS - 1);
        h->gen[k] = h->now;
        h->key[k] = mv[i];
        h->idx[k] = i;
    }
}

static int mset_find(const mset *h, int m)
{
    for (int k = mslot(m); h->gen[k] == h->now; k = (k + 1) & (MSLOTS - 1))
        if (h->key[k] == m) return h->idx[k];
    return -1;
}

static int draw_chance(const ai_game *g, const void *st, ai_rng *r)   /* an outcome by its weight; AI_NONE: none */
{
    int out[AI_MAX_MOVES], w[AI_MAX_MOVES];
    int n = g->chance ? g->chance(st, out, w) : 0;
    if (!n) return AI_NONE;
    long total = 0;
    for (int i = 0; i < n; i++) total += w[i] > 0 ? w[i] : 0;
    if (total <= 0) return out[ai_rand_n(r, n)];
    long x = (long)(ai_rand_f(r) * total);
    for (int i = 0; i < n; i++) {
        x -= w[i] > 0 ? w[i] : 0;
        if (x < 0) return out[i];
    }
    return out[n - 1];
}

static int finished(const ai_game *g, const void *st, int players, float *rw)
{
    if (g->result) return g->result(st, rw);
    int s;
    if (!g->over(st, &s)) return 0;
    int me = g->side(st);
    for (int p = 0; p < players; p++) rw[p] = s == 0 ? 1.0f / players : 0;
    if (s > 0) rw[me] = 1;
    else if (s < 0) for (int p = 0; p < players; p++) rw[p] = p == me ? 0 : 1.0f / (players - 1);
    return 1;
}

int ai_mcts(const ai_game *g, const void *root, const ai_opts *o, ai_stats *stats)
{
    double t0 = ai_now_ms();
    ai_rng own, *rng = o && o->rng ? o->rng : (ai_seed(&own, 1), &own);
    ai_budget b = o ? o->budget : ai_level_budget(AI_HARD);
    float cexp = o && o->uct_c > 0 ? o->uct_c : 0.7f;
    int players = g->players >= 2 ? (g->players > AI_MAX_SEATS ? AI_MAX_SEATS : g->players) : 2;
    if (stats) memset(stats, 0, sizeof *stats);
    int rm[AI_MAX_MOVES], n = g->moves(root, rm);
    float rw[AI_MAX_SEATS];
    if (!n || finished(g, root, players, rw) || (g->chance && draw_chance(g, root, rng) != AI_NONE))
        return n ? rm[0] : AI_NONE;
    if (n == 1) return rm[0];
    long cap = b.nodes > 0 ? b.nodes : 1 << 20;
    if (cap > 1 << 22) cap = 1 << 22;
    mnode *T = malloc(sizeof(mnode) * (size_t)(cap + 1));
    uint8_t *st = malloc(g->size);
    int *mv = malloc(sizeof(int) * AI_MAX_MOVES);
    int *path = malloc(sizeof(int) * 4096);
    mset *h = calloc(1, sizeof *h);
    if (!T || !st || !mv || !path || !h) {
        free(T); free(st); free(mv); free(path); free(h);
        return rm[ai_rand_n(rng, n)];
    }
    long used = 1, it = 0;
    T[0] = (mnode){.move = AI_NONE, .child = -1, .next = -1};
    int seat = g->side(root), stopped = 0;
    double deadline = b.ms > 0 ? t0 + b.ms : 0;
    for (; it < cap; it++) {
        if ((it & 63) == 0 && (ai_cancelled() || (deadline > 0 && ai_now_ms() >= deadline))) { stopped = 1; break; }
        memcpy(st, root, g->size);
        if (g->determinise) g->determinise(st, seat, rng);
        int node = 0, depth = 0, over = 0;
        path[depth++] = 0;
        /* selection and expansion */
        for (;;) {
            int m;
            while ((m = draw_chance(g, st, rng)) != AI_NONE) g->make(st, m);
            if (finished(g, st, players, rw)) { over = 1; break; }
            int k = g->moves(st, mv);
            if (!k) break;
            mset_fill(h, mv, k);
            /* the children legal here: each one available once more; the untried legal moves */
            int tried = 0, bestc = -1;
            double bestv = -1e30, lnn;
            unsigned char seen[AI_MAX_MOVES];
            memset(seen, 0, (size_t)k);
            for (int ch = T[node].child; ch >= 0; ch = T[ch].next) {
                int i = mset_find(h, T[ch].move);
                if (i < 0) continue;
                seen[i] = 1;
                tried++;
                T[ch].avail++;
            }
            if (tried < k && used < cap) {      /* expand: an untried move at random */
                int pickn = ai_rand_n(rng, k - tried), i = 0;
                for (; i < k; i++)
                    if (!seen[i] && pickn-- == 0) break;
                int nn = (int)used++;
                T[nn] = (mnode){.move = mv[i], .child = -1, .next = T[node].child, .player = (int8_t)g->side(st)};
                T[node].child = nn;
                g->make(st, mv[i]);
                path[depth++] = nn;
                break;
            }
            if (!tried) break;                  /* the tree is full: play out from here */
            for (int ch = T[node].child; ch >= 0; ch = T[ch].next) {
                if (mset_find(h, T[ch].move) < 0) continue;
                lnn = log((double)T[ch].avail);
                double v = T[ch].visits ? T[ch].reward / T[ch].visits + cexp * sqrt(lnn / T[ch].visits) : 1e9;
                if (v > bestv) { bestv = v; bestc = ch; }
            }
            g->make(st, T[bestc].move);
            node = bestc;
            if (depth < 4095) path[depth++] = node;
        }
        /* the playout */
        if (!over) {
            int steps = 0;
            for (;;) {
                int m;
                while ((m = draw_chance(g, st, rng)) != AI_NONE) g->make(st, m);
                if (finished(g, st, players, rw)) break;
                if (g->cutoff && steps >= g->cutoff) {
                    if (!g->estimate || !g->estimate(st, rw))
                        for (int p = 0; p < players; p++) rw[p] = 1.0f / players;
                    break;
                }
                m = g->playout ? g->playout(st, rng) : ai_random_move(g, st, rng);
                if (m == AI_NONE) {             /* no move and not over: the estimate, or a draw */
                    if (!g->estimate || !g->estimate(st, rw))
                        for (int p = 0; p < players; p++) rw[p] = 1.0f / players;
                    break;
                }
                g->make(st, m);
                steps++;
            }
        }
        for (int i = 0; i < depth; i++) {       /* back up: each node holds the reward of the player who moved */
            mnode *x = &T[path[i]];
            x->visits++;
            if (i) x->reward += rw[x->player >= 0 && x->player < players ? x->player : 0];
        }
    }
    /* the root's most visited move that is legal in the real position */
    mset_fill(h, rm, n);
    int best = AI_NONE, bv = -1;
    float br = 0;
    for (int ch = T[0].child; ch >= 0; ch = T[ch].next)
        if (mset_find(h, T[ch].move) >= 0 && T[ch].visits > bv) {
            bv = T[ch].visits;
            best = T[ch].move;
            br = T[ch].visits ? T[ch].reward / T[ch].visits : 0;
        }
    if (best == AI_NONE) best = rm[ai_rand_n(rng, n)];
    if (stats) {
        stats->nodes = it;
        stats->score = (int)lroundf(br * 100);
        stats->stopped = stopped;
        stats->ms = ai_now_ms() - t0;
    }
    free(T); free(st); free(mv); free(path); free(h);
    return best;
}
