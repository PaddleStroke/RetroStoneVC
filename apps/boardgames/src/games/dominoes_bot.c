/*
 * Dominoes' bot (bots.h), every variant (the block game, the draw game, All Fives): information-set Monte Carlo tree
 * search (ai.h's ai_mcts with determinise()) over the round, from what its seat may know: its own bones, the line,
 * how many bones each player and the boneyard hold, and the numbers each player was seen to lack (a pass, or a draw:
 * nothing in hand matched the open ends then). Each iteration deals the unseen bones at random, keeping to those.
 * Runs on the worker thread: it copies the game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The search's own small model of the round: the hands as bit sets, the line as its open ends (All Fives: the
 *   spinner, a double at an end counted twice), the boneyard in the dealt order; the points as the rules count them
 *   (the round's winner, the blocked round's lowest count, All Fives' multiples of five as they are made).
 *   The reward: a player's points against the others' (the round's winner scores the bones left), or 1 to whoever
 *   ends the game with the most once someone passes the target.
 *   The playouts: the bone that scores most (All Fives), else the heaviest, keeping a double for later now and then.
 * Levels: easy plays the playouts' own choice with a slip now and then (any bone that fits); medium a short search;
 * hard a longer one (about 0.3 s a move on a PC).
 * Its moves are the game's notation ("p36e", "p66" for the first bone, "d", "x", "n"), played by dominoes_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/dominoes.h"

#define DB_HARD_ITERS 20000

typedef struct {
    uint8_t np, variant, turn, passes, over, winner, nyard, lead;
    uint8_t laid, spinner, cw, ce, cdbl;        /* the middle bone: its sides, a double */
    uint8_t armn[ENDS], endv[ENDS], enddbl[ENDS];
    uint8_t nhand[DOM_MAXP];
    uint32_t hand[DOM_MAXP];                    /* bit b: bone b */
    uint8_t yard[DOM_BONES];                    /* dealt by determinise(): the top last */
    uint8_t lacks[DOM_MAXP];                    /* bit v: the player holds no bone with a v (the view) */
    int16_t total[DOM_MAXP], gain[DOM_MAXP];
    uint32_t unseen;                            /* the view: the bones nowhere in sight */
    uint8_t me;
} dsim;

static int lo_of(int b) { return dom_lo(b); }
static int hi_of(int b) { return dom_hi(b); }

static int s_open(const dsim *s, int e)
{
    if (!s->laid) return e == END_C;
    if (e == END_W || e == END_E) return 1;
    if (e == END_N || e == END_S) return s->spinner && s->armn[END_W] && s->armn[END_E];
    return 0;
}

static int s_end(const dsim *s, int e)
{
    if (s->armn[e]) return s->endv[e];
    return e == END_E ? s->ce : s->cw;
}

static int s_fits(const dsim *s, int b, int e)
{
    if (!s->laid) return e == END_C && (s->lead == DOM_NONE || b == s->lead);
    if (e >= ENDS || !s_open(s, e)) return 0;
    int v = s_end(s, e);
    return lo_of(b) == v || hi_of(b) == v;
}

static int s_count(const dsim *s)              /* the open ends added up (as the rules' dom_count) */
{
    if (!s->laid) return 0;
    if (!s->armn[END_W] && !s->armn[END_E]) return s->cw + s->ce;
    int sum = 0;
    for (int e = 0; e < ENDS; e++) {
        if (s->armn[e]) sum += s->enddbl[e] ? 2 * s->endv[e] : s->endv[e];
        else if (e == END_W || e == END_E) sum += s->cdbl ? 2 * s->cw : e == END_W ? s->cw : s->ce;
    }
    return sum;
}

static int pips(uint32_t h)
{
    int sum = 0;
    for (; h; h &= h - 1) {
        int b = __builtin_ctz(h);
        sum += lo_of(b) + hi_of(b);
    }
    return sum;
}

static void round_end(dsim *s, int winner)
{
    int low = 1 << 20, nlow = 0, p[DOM_MAXP];
    for (int q = 0; q < s->np; q++) {
        p[q] = pips(s->hand[q]);
        if (p[q] < low) low = p[q], nlow = 0;
        if (p[q] == low) nlow++;
    }
    if (winner == DOM_NONE && nlow == 1)
        for (int q = 0; q < s->np; q++)
            if (p[q] == low) winner = q;
    if (winner != DOM_NONE) {
        int pts = 0;
        for (int q = 0; q < s->np; q++)
            if (q != winner) pts += p[q];
        if (s->variant == DOM_FIVES) pts = (pts + 2) / 5 * 5;
        s->gain[winner] = (int16_t)(s->gain[winner] + pts);
    }
    s->winner = (uint8_t)winner;
    s->over = 1;
}

/* a move: the rules' action (DA_PLAY | bone | end, DA_DRAW, DA_PASS) */
static void s_make(void *st, int a)
{
    dsim *s = st;
    int q = s->turn;
    switch (DA_KIND(a)) {
    case DA_PLAY: {
        int b = DA_BONE(a), e = DA_END(a), dbl = lo_of(b) == hi_of(b);
        s->hand[q] &= ~(1u << b);
        s->nhand[q]--;
        s->lead = DOM_NONE;
        s->passes = 0;
        if (!s->laid) {
            s->laid = 1;
            s->cw = (uint8_t)lo_of(b);
            s->ce = (uint8_t)hi_of(b);
            s->cdbl = (uint8_t)dbl;
            s->spinner = s->variant == DOM_FIVES && dbl;
        } else {
            int v = s_end(s, e), o = lo_of(b) == v ? hi_of(b) : lo_of(b);
            if (s->variant == DOM_FIVES && !s->spinner && dbl) {   /* the first double: the spinner */
                int back = e == END_E ? END_W : END_E;
                int far = s_end(s, back), fard = s->armn[back] ? s->enddbl[back] : s->cdbl;
                int all = s->armn[END_W] + s->armn[END_E] + 1;
                memset(s->armn, 0, sizeof s->armn);
                s->armn[back] = (uint8_t)all;
                s->endv[back] = (uint8_t)far;
                s->enddbl[back] = (uint8_t)fard;
                s->cw = s->ce = (uint8_t)lo_of(b);
                s->cdbl = 1;
                s->spinner = 1;
            } else {
                s->armn[e]++;
                s->endv[e] = (uint8_t)o;
                s->enddbl[e] = (uint8_t)dbl;
            }
        }
        if (s->variant == DOM_FIVES) {
            int c = s_count(s);
            if (c > 0 && c % 5 == 0) s->gain[q] = (int16_t)(s->gain[q] + c);
            if (s->total[q] + s->gain[q] >= DOM_TARGET) { s->winner = (uint8_t)q; s->over = 1; break; }
        }
        if (!s->nhand[q]) round_end(s, q);
        else s->turn = (uint8_t)((q + 1) % s->np);
        break;
    }
    case DA_DRAW: {
        int b = s->yard[--s->nyard];
        s->hand[q] |= 1u << b;
        s->nhand[q]++;
        break;
    }
    case DA_PASS:
        if (++s->passes >= s->np) round_end(s, DOM_NONE);
        else s->turn = (uint8_t)((q + 1) % s->np);
        break;
    }
}

static int s_moves(const void *st, int *out)
{
    const dsim *s = st;
    if (s->over) return 0;
    int k = 0;
    uint32_t h = s->hand[s->turn];
    for (; h; h &= h - 1) {
        int b = __builtin_ctz(h);
        if (!s->laid) {
            if (s_fits(s, b, END_C)) out[k++] = DA(DA_PLAY, b, END_C);
            continue;
        }
        int seen = -1;
        for (int e = 0; e < ENDS; e++)
            if (s_fits(s, b, e)) {
                /* two ends alike (the same number, no arm yet on either or both plain): one of them */
                int key = s_end(s, e) * 4 + (s->armn[e] ? 1 : 0) + (s->armn[e] && s->enddbl[e] ? 2 : 0);
                if (e < END_N && key == seen) continue;
                if (e < END_N) seen = key;
                out[k++] = DA(DA_PLAY, b, e);
            }
    }
    if (k) return k;
    if (s->variant != DOM_BLOCK && s->nyard) out[k++] = DA_DRAW;
    else out[k++] = DA_PASS;
    return k;
}

static int s_side(const void *st) { return ((const dsim *)st)->turn; }

static int s_result(const void *st, float *rw)
{
    const dsim *s = st;
    if (!s->over) return 0;
    int top = -1, ntop = 0, reached = 0;
    for (int q = 0; q < s->np; q++) {
        int t = s->total[q] + s->gain[q];
        reached |= t >= DOM_TARGET;
        if (t > top) top = t, ntop = 0;
        if (t == top) ntop++;
    }
    for (int q = 0; q < s->np; q++) {
        if (reached) { rw[q] = s->total[q] + s->gain[q] == top ? 1.0f / ntop : 0; continue; }
        int others = 0;
        for (int o = 0; o < s->np; o++)
            if (o != q) others += s->gain[o];
        float v = 0.5f + (s->gain[q] - (float)others / (s->np - 1)) / 80.0f;
        rw[q] = v < 0 ? 0 : v > 1 ? 1 : v;
    }
    return 1;
}

/* the playouts' choice: All Fives the most points, else the heaviest bone (a double first among equals) */
static int s_heuristic(const dsim *s, ai_rng *r, int eps)
{
    int mv[AI_MAX_MOVES], n = s_moves(s, mv);
    if (n <= 1) return n ? mv[0] : AI_NONE;
    if (eps && ai_rand_n(r, 100) < eps) return mv[ai_rand_n(r, n)];
    int best = -(1 << 20), bm = mv[0];
    for (int i = 0; i < n; i++) {
        int a = mv[i], b = DA_BONE(a), w = (lo_of(b) + hi_of(b)) * 4 + (lo_of(b) == hi_of(b) ? 6 : 0);
        if (s->variant == DOM_FIVES) {
            dsim t = *s;
            int g = t.gain[t.turn];
            s_make(&t, a);
            w += (t.gain[s->turn] - g) * 10;
        }
        w += ai_rand_n(r, 3);
        if (w > best) best = w, bm = a;
    }
    return bm;
}

static int s_playout(const void *st, ai_rng *r) { return s_heuristic(st, r, 15); }

/* deals the unseen bones: the others' hands (keeping to what they were seen to lack), then the boneyard */
static void s_determinise(void *st, int seat, ai_rng *r)
{
    dsim *s = st;
    (void)seat;
    uint8_t pool[DOM_BONES];
    int n = 0;
    for (uint32_t u = s->unseen; u; u &= u - 1) pool[n++] = (uint8_t)__builtin_ctz(u);
    for (int i = n - 1; i > 0; i--) {
        int j = ai_rand_n(r, i + 1);
        uint8_t t = pool[i];
        pool[i] = pool[j];
        pool[j] = t;
    }
    /* the most constrained players first */
    int order[DOM_MAXP], no = 0;
    for (int q = 0; q < s->np; q++)
        if (q != s->me) order[no++] = q;
    for (int i = 1; i < no; i++)
        for (int j = i; j > 0 && __builtin_popcount(s->lacks[order[j]]) > __builtin_popcount(s->lacks[order[j - 1]]);
             j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    uint8_t used[DOM_BONES] = {0};
    for (int k = 0; k < no; k++) {
        int q = order[k], need = s->nhand[q];
        s->hand[q] = 0;
        for (int pass = 0; pass < 2 && need; pass++)    /* pass 1: anything left (the view was too strict) */
            for (int i = 0; i < n && need; i++) {
                int b = pool[i];
                if (used[i]) continue;
                if (!pass && (s->lacks[q] >> lo_of(b) & 1 || s->lacks[q] >> hi_of(b) & 1)) continue;
                used[i] = 1;
                s->hand[q] |= 1u << b;
                need--;
            }
    }
    int y = 0;
    for (int i = 0; i < n; i++)
        if (!used[i] && y < s->nyard) s->yard[y++] = pool[i];
}

static const ai_game GAME = {
    .size = sizeof(dsim), .moves = s_moves, .make = s_make, .side = s_side, .result = s_result,
    .playout = s_playout, .determinise = s_determinise,
};

/* the view of seat me: its hand, the line, the counts, the numbers each was seen to lack (the round replayed) */
static void view_of(const dgame *g, int me, dsim *s)
{
    const dpos *p = &g->pos;
    memset(s, 0, sizeof *s);
    s->np = p->players;
    s->variant = p->variant;
    s->turn = p->turn;
    s->passes = p->passes;
    s->nyard = p->nyard;
    s->lead = p->lead;
    s->me = (uint8_t)me;
    s->laid = p->center != DOM_NONE;
    if (s->laid) {
        s->cw = p->cw;
        s->ce = p->ce;
        s->cdbl = (uint8_t)dom_double(p->center);
        s->spinner = p->spinner;
        for (int e = 0; e < ENDS; e++) {
            s->armn[e] = p->narm[e];
            if (p->narm[e]) {
                s->endv[e] = p->out[e][p->narm[e] - 1];
                s->enddbl[e] = (uint8_t)dom_double(p->arm[e][p->narm[e] - 1]);
            }
        }
    }
    uint32_t line = 0;
    if (s->laid) line |= 1u << p->center;
    for (int e = 0; e < ENDS; e++)
        for (int i = 0; i < p->narm[e]; i++) line |= 1u << p->arm[e][i];
    for (int q = 0; q < p->players; q++) {
        s->nhand[q] = p->nhand[q];
        s->total[q] = p->total[q];
    }
    for (int i = 0; i < p->nhand[me]; i++) s->hand[me] |= 1u << p->hand[me][i];
    s->unseen = ((1u << DOM_BONES) - 1) & ~line & ~s->hand[me];
    dgame *t = malloc(sizeof *t);
    if (t) {                                    /* the round's passes and draws: what each lacked then */
        dom_new(t, g->variant, g->players, g->seed);
        int drew_last[DOM_MAXP] = {0};
        for (int i = 0; i < g->n; i++) {
            const dpos *q = &t->pos;
            int a = g->act[i], k = DA_KIND(a), who = q->turn;
            if (k == DA_NEXT) memset(s->lacks, 0, sizeof s->lacks), memset(drew_last, 0, sizeof drew_last);
            if ((k == DA_DRAW || k == DA_PASS) && who != me && q->center != DOM_NONE) {
                int m = 0;
                for (int e = 0; e < ENDS; e++)
                    if (dom_open(q, e)) m |= 1 << dom_end(q, e);
                if (k == DA_DRAW && !drew_last[who]) s->lacks[who] = 0;   /* new bones since: only this turn's */
                s->lacks[who] |= (uint8_t)m;
            }
            for (int w = 0; w < DOM_MAXP; w++) drew_last[w] = w == who && k == DA_DRAW;
            dom_play(t, a);
        }
        free(t);
    }
}

/* the tests: the model's step against the rules': the view before, the action made, compared with the view after
 * (the open ends and their numbers, the count, the points made); 1: the same */
int domb_step_ok(const dgame *before, int act, const dgame *after)
{
    dsim *a = malloc(sizeof *a);
    int ok = a != NULL;
    if (ok) {
        int me = before->pos.turn;
        view_of(before, me, a);
        int16_t g0 = a->gain[me];
        s_make(a, act);
        if (DA_KIND(act) == DA_PLAY) {
            for (int e = 0; e < ENDS; e++) {
                ok &= s_open(a, e) == dom_open(&after->pos, e);
                if (s_open(a, e)) ok &= s_end(a, e) == dom_end(&after->pos, e);
            }
            ok &= s_count(a) == dom_count(&after->pos);
            if (after->pos.variant == DOM_FIVES && after->pos.phase == DP_PLAY)
                ok &= a->gain[me] - g0 == after->pos.scored;
        }
        ok &= a->over == (after->pos.phase != DP_PLAY);
        if (!a->over) ok &= a->turn == after->pos.turn;
    }
    free(a);
    return ok;
}

/* the action for seat at a level (0: nothing to do) */
int domb_choose(const dgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const dpos *p = &g->pos;
    if (p->phase == DP_ROUND) return DA_NEXT;
    if (p->phase != DP_PLAY || p->turn != seat) return 0;
    dsim *s = malloc(sizeof *s);
    if (!s) return dom_auto(p);
    view_of(g, seat, s);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 31 + (uint64_t)seat));
    int mv[AI_MAX_MOVES], n = s_moves(s, mv), a;
    if (n <= 1) a = n ? mv[0] : 0;
    else if (level == AI_EASY && ai_rand_n(&r, 100) < 20) a = s_heuristic(s, &r, 30);   /* a slip */
    else {
        ai_opts o = {.budget = ai_level_budget(level), .rng = &r, .uct_c = 0.6f};
        long hard = p->variant == DOM_FIVES ? DB_HARD_ITERS / 2 : DB_HARD_ITERS;
        o.budget.nodes = level == AI_EASY ? 200 : level == AI_MEDIUM ? hard / 16 : hard;
        o.budget.ms = o.budget.ms * 7 / 10;      /* (the safety cap, with room for the rest) */
        a = ai_mcts(&GAME, s, &o, stats);
    }
    free(s);
    return a && dom_legal(p, a) ? a : dom_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    dgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *dominoes_live();                      /* the copy, at entry: nothing else is read */
    int a = domb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 6) return 0;
    switch (DA_KIND(a)) {
    case DA_PLAY: {
        int b = DA_BONE(a), e = DA_END(a);
        static const char *ends[] = {"w", "e", "n", "s"};
        snprintf(move, (size_t)cap, "p%d%d%s", dom_lo(b), dom_hi(b), e == END_C ? "" : ends[e]);
        break;
    }
    case DA_DRAW: snprintf(move, (size_t)cap, "d"); break;
    case DA_PASS: snprintf(move, (size_t)cap, "x"); break;
    case DA_NEXT: snprintf(move, (size_t)cap, "n"); break;
    default: return 0;
    }
    return 1;
}

static int actor(void)
{
    const dpos *p = &dominoes_live()->pos;
    return p->phase == DP_PLAY ? p->turn : -1;
}

static const bot dominoes_bot = {.think = think, .actor = actor, .play = dominoes_show};
REGISTER_BOT(dominoes, &dominoes_bot)
