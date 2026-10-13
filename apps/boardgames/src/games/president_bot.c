/*
 * Président's bot (bots.h), 3 to 6 players: information-set Monte Carlo tree search (ai_mcts) over the deal, each
 * iteration dealing the cards the seat has not seen (the pack minus its hand, the cards played this deal and the
 * cards it knows elsewhere) at random into the others' hands; heuristic playouts to the deal's end. Runs on the worker
 * thread: it copies the game at entry and reads only what its seat may know (its hand, the plays, the exchange it took
 * part in). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Suits do not matter: a play is a rank and a count (the lowest suits held, as the notation plays them). What the
 * seat knows of the others: the cards it gave in the exchange (the Trou its best cards, the Président the ones he gave
 * back), in that player's hand until played. The cards given back (the Président two, the Vice-président one) are
 * rule-based: the lowest cards, single ranks first. The playouts: lead the lowest rank (all its cards), follow with
 * the lowest rank that beats without breaking a set, keep the 2s and aces while the hand is long. The reward: the
 * place, as the points. The next deal waits for a human tap (actor() -1); think() answers "N" there all the same.
 * Its moves are the game's notation ("77", "p", "gKs", "N"), played by president_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/president.h"

#define PRES_HARD_ITERS 16000                   /* about 0.3 s a move on a PC */
#define PM_PLAY(rank, count) (PA_PLAY | (rank) << 4 | (count))   /* the search's play: a rank and a count */

typedef struct { ppos p; uint8_t me, ngone; uint8_t gone[52]; } pr_st;   /* gone: the cards played this deal */

static int popc(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

static int held(const ppos *p, int pl, int rank)   /* the suits of rank the player holds (a mask) */
{
    int m = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (CARD_RANK(p->hand[pl][i]) == rank) m |= 1 << CARD_SUIT(p->hand[pl][i]);
    return m;
}

/* the count of each rank the player holds (by rank 1..13) */
static void counts(const ppos *p, int pl, int *cnt)
{
    memset(cnt, 0, 14 * sizeof *cnt);
    for (int i = 0; i < p->n[pl]; i++) cnt[CARD_RANK(p->hand[pl][i])]++;
}

static int lowest_suits(int m, int count)
{
    int out = 0;
    for (int s = 0; s < 4 && count > 0; s++)
        if (m >> s & 1) out |= 1 << s, count--;
    return out;
}

static int rank_of_strength(int st) { return st == 12 ? 1 : st == 13 ? 2 : st + 2; }

/* the search's move as the rules' action */
static int real(const ppos *p, int m)
{
    if (PA_KIND(m) != PA_PLAY) return m;
    int r = PA_RANK(m);
    return PA_PLAYOF(r, lowest_suits(held(p, p->turn, r), PA_MASK(m)));
}

static void p_apply(ppos *p, int a)
{
    pgame g;
    g.pos = *p;
    g.n = 0;
    if (pres_play(&g, a)) *p = g.pos;
}

static int p_moves(const void *s, int *out)
{
    const ppos *p = &((const pr_st *)s)->p;
    if (p->phase != PP_PLAY) return 0;
    int k = 0, pl = p->turn, cnt[14];
    counts(p, pl, cnt);
    for (int st = 1; st <= 13; st++) {
        int r = rank_of_strength(st), c = cnt[r];
        if (!c) continue;
        if (!p->top_count)
            for (int j = 1; j <= c; j++) out[k++] = PM_PLAY(r, j);
        else if (c >= p->top_count && st > pres_strength(p->top_rank)) out[k++] = PM_PLAY(r, p->top_count);
    }
    if (p->top_count) out[k++] = PA_PASS;
    return k;
}

static void p_make(void *s, int m)
{
    ppos *p = &((pr_st *)s)->p;
    p_apply(p, real(p, m));
}

static int p_side(const void *s) { return ((const pr_st *)s)->p.turn; }

static int p_result(const void *s, float *rw)
{
    const ppos *p = &((const pr_st *)s)->p;
    if (p->phase == PP_PLAY || p->phase == PP_GIVE) return 0;
    for (int q = 0; q < p->players; q++) {
        int place = pres_place(p, q);
        rw[q] = place < 0 ? 0 : (float)pres_points(p->players, place) / (float)(p->players - 1);
    }
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static int p_policy(const ppos *p, ai_rng *r, int eps)
{
    int pl = p->turn, n = p->n[pl], best = PA_PASS;
    long bs = -(1L << 30);
    if (p->top_count) bs = n > 6 ? 0 : -50;    /* passing: a fair choice while the hand is long */
    int ranks = 0, cnt[14];
    counts(p, pl, cnt);
    for (int rk = 1; rk <= 13; rk++) ranks += cnt[rk] != 0;
    for (int st = 1; st <= 13; st++) {
        int rk = rank_of_strength(st), c = cnt[rk];
        if (!c) continue;
        long sc;
        if (!p->top_count) {
            sc = 1000 - st * 60 + c * 25;
            if (st >= 12 && ranks > 2) sc -= 600;   /* keep the aces and 2s to win the lead back */
            if (ranks <= 2 && st >= 12) sc += 2000;  /* the last ranks: the sure one first */
        } else {
            if (c < p->top_count || st <= pres_strength(p->top_rank)) continue;
            sc = 600 - st * 40 - (c - p->top_count) * 120;   /* the lowest that beats, without breaking a set */
            if (st >= 12 && n > 5) sc -= 500;
            if (ranks <= 2) sc += 800;
        }
        if (eps && ai_rand_n(r, 100) < eps) sc += ai_rand_n(r, 1200);
        if (sc > bs) bs = sc, best = PM_PLAY(rk, p->top_count ? p->top_count : c);
    }
    return best;
}

static int p_playout(const void *s, ai_rng *r) { return p_policy(&((const pr_st *)s)->p, r, 8); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void p_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    pr_st *st = s;
    ppos *p = &st->p;
    uint8_t pack[CARDS_MAX], seen[CARDS_MAX * 2], pool[CARDS_MAX];
    int npack = cards_pack(PACK_52, pack), ns = 0;
    for (int q = 0; q < p->players; q++)        /* every card in a hand that is not hidden: known */
        for (int i = 0; i < p->n[q]; i++) seen[ns++] = p->hand[q][i];
    for (int i = 0; i < st->ngone; i++) seen[ns++] = st->gone[i];   /* the current trick among them */
    int np = cdb_unseen(pack, npack, seen, ns, pool);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++) cdb_fill(p->hand[q], p->n[q], pool, &np);
    p->seed = ai_rand(r);
}

/* the cards played this deal (all the plays since the last deal began) */
static int played(const pgame *g, uint8_t *out)
{
    int start = 0, k = 0;
    for (int i = 0; i < g->n; i++)
        if (PA_KIND(g->act[i]) == PA_NEXT) start = i + 1;
    for (int i = start; i < g->n; i++)
        if (PA_KIND(g->act[i]) == PA_PLAY)
            for (int s = 0; s < 4; s++)
                if (PA_MASK(g->act[i]) >> s & 1) out[k++] = (uint8_t)CARD(s, PA_RANK(g->act[i]));
    return k;
}

static int role_of(const ppos *p, int role)
{
    for (int q = 0; q < p->players; q++)
        if (p->role[q] == role) return q;
    return -1;
}

/* seat me's view: its hand; the others' hands hidden but the cards it gave them (still unplayed); the cards played
 * this deal (out of the deal of the unseen) */
static void p_view(const pgame *g, int me, pr_st *v)
{
    const ppos *live = &g->pos;
    v->p = *live;
    v->me = (uint8_t)me;
    ppos *p = &v->p;
    uint8_t *gone = v->gone;
    int ngone = v->ngone = (uint8_t)played(g, gone);
    /* the cards me gave: to whom */
    uint8_t known[PRES_MAXP][4], nk[PRES_MAXP] = {0};
    int pres = role_of(live, PR_PRES), vice = role_of(live, PR_VICE), trou = role_of(live, PR_TROU),
        vt = role_of(live, PR_VICETROU);
    int to = -1, from = -1;
    if (me == trou) to = pres, from = pres;     /* the trou's best cards went to the président: got[pres] */
    if (me == vt) to = vice, from = vice;
    if (me == pres) to = trou, from = trou;     /* the président's cards given back: got[trou] */
    if (me == vice) to = vt, from = vt;
    if (to >= 0 && to != me)
        for (int i = 0; i < live->ngot[from]; i++) {
            int c = live->got[from][i], out = 0;
            for (int j = 0; j < ngone; j++) out |= gone[j] == c;
            if (!out && nk[to] < 4) known[to][nk[to]++] = (uint8_t)c;
        }
    for (int q = 0; q < live->players; q++) {
        if (q == me) continue;
        memset(p->hand[q], CDB_HIDDEN, live->n[q]);
        for (int i = 0; i < nk[q] && i < live->n[q]; i++) p->hand[q][i] = known[q][i];
    }
    p->seed = 0;
}

/* the cards given back: the lowest, single ranks first */
static int give_choice(const ppos *p, int pl)
{
    int best = -1, bs = 1 << 30;
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i], st = pres_strength(CARD_RANK(c)), cnt = popc(held(p, pl, CARD_RANK(c)));
        int sc = st * 10 + (cnt > 1 ? 40 : 0);
        if (sc < bs) bs = sc, best = c;
    }
    return PA_GIVE | best;
}

/* the action for seat at a level (0: nothing to do) */
int prb_choose_act(const pgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const ppos *live = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == PP_DEAL) return PA_NEXT;
    if ((live->phase != PP_PLAY && live->phase != PP_GIVE) || live->turn != seat) return 0;
    if (live->phase == PP_GIVE) return give_choice(live, seat);
    pr_st *v = malloc(sizeof *v);
    if (!v) return pres_auto(live);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 131 + live->n[seat]));
    p_view(g, seat, v);
    int mv[AI_MAX_MOVES], n = p_moves(v, mv), m = n ? mv[0] : AI_NONE;
    if (n > 1) {
        if (cdb_slip(level, &r)) m = p_policy(&v->p, &r, CDB_SLIP_EPS);
        else {
            ai_game G = {
                .size = sizeof(pr_st), .moves = p_moves, .make = p_make, .side = p_side, .players = live->players,
                .result = p_result, .playout = p_playout, .determinise = p_determinise,
            };
            int x = cdb_search(&G, v, level, PRES_HARD_ITERS, &r, stats);
            if (x != AI_NONE) m = x;
        }
    }
    free(v);
    int a = m == AI_NONE ? 0 : real(live, m);
    return a && pres_legal(live, a) ? a : pres_auto(live);
}

static char rank_char(int r) { return "?A23456789TJQK"[r]; }

static int think(int seat, int level, char *move, int cap)
{
    pgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *president_live();                     /* the copy, at entry: nothing else is read */
    int a = prb_choose_act(g, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL), ok = 1;
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    switch (PA_KIND(a)) {
    case PA_PLAY: {
        int k = 0;
        for (int i = 0; i < popc(PA_MASK(a)); i++) move[k++] = rank_char(PA_RANK(a));
        move[k] = 0;
        break;
    }
    case PA_PASS: snprintf(move, (size_t)cap, "p"); break;
    case PA_GIVE:
        snprintf(move, (size_t)cap, "g%c%c", rank_char(CARD_RANK(a & 0xff)), "shdc"[CARD_SUIT(a & 0xff)]);
        break;
    case PA_NEXT: snprintf(move, (size_t)cap, "N"); break;
    default: ok = 0;
    }
    return ok;
}

static int actor(void)
{
    const ppos *p = &president_live()->pos;
    return p->phase == PP_PLAY || p->phase == PP_GIVE ? p->turn : -1;
}

static const bot president_bot = {.think = think, .actor = actor, .play = president_show};
REGISTER_BOT(president, &president_bot)
