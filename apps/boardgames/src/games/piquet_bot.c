/*
 * Piquet's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards), the reward what the rules count in the play; the exchange chosen by a hand count checked
 * by simulated deals. Runs on the worker thread: it copies the game at entry and reads only what its seat may know.
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the seat knows (the view): its own twelve cards and the cards it threw; the cards played (the deal's actions
 * replayed on a scratch game from the seed: who played what, a suit not followed: void); the other's declarations as
 * the board shows them to both (the point's cards and value, the best sequence's length and top card, the best set's
 * size and a card of its rank, every card of the combinations that scored for them). The cards the other threw and
 * the talon's cards left are unseen: in the search they are dealt at random between the other's hand and the cards out
 * of play (rest[]); each such deal is then mended (dealt(): cards swapped between the other's hand and the rest) until
 * the other's twelve cards give the declarations they made, as far as a few swaps allow.
 *
 * The exchange (the elder throws 1 to 5, the younger 0 to what is left): every way to throw is scored by a hand
 * count over random draws from the unseen cards (the point by its length and its chance of being good, sequences and
 * sets by theirs, the play by sure tricks from the ace down, guarded kings and queens, long suits; the elder loses
 * 1.5 a card he leaves to the younger); the best few are then played out in simulated deals: the draws and the other's
 * hand dealt at random from the unseen cards (the younger of the simulation takes what the elder left, throwing his
 * weakest cards), the declarations compared by the rules themselves, the twelve tricks played by the core's
 * heuristic; the throw with the best average net score is chosen. Easy counts a few draws and takes one of its three
 * best counts at random; medium simulates its best four, hard its best eight with more deals.
 * The declarations: the rules declare every hand's best (nothing is kept back or sunk in this game's rules): the bot
 * answers "k". The round's summary waits for a human tap (actor() -1); think() answers "n" there all the same.
 * The reward (the search): the play's points as the rules count them (a lead of a 10 or higher, the other's lead won
 * with one, the last trick, the cards for seven tricks or more, capot 40 instead, the pique when the elder reaches 30
 * first), the deal's net score mapped to 0..1; in the partie's last deal, the partie itself: who wins it (most), and by
 * how much as the Rubicon counts it (the loser under 100: both totals and 100).
 * The game has one variant (Rubicon piquet, six deals); the bot plays it.
 * Its moves are the game's notation ("e7h8hTc", "e", "k", "As", "n"), played by piquet_show().
 */
#include <math.h>
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/piquet.h"
#include "games/trickbot.h"

#ifndef PIQUET_HARD_ITERS
#define PIQUET_HARD_ITERS 16000                 /* about 30 ms a card on a PC (processor time), 0.2 s at worst */
#endif
#define NOBODY 255

typedef struct {
    int16_t dp[2], bonus[2], total[2];          /* the deal's points (declarations and play), its pique / repique */
    int16_t rootdiff;                           /* seat 0's lead in the deal at the search's root */
    uint8_t elder, done, last, nk;              /* done: no pique possible any more; last: the partie's last deal */
    uint8_t has, pn, pv, sn, stop, ssuit, setn, setrank;   /* the other's declarations (has: known) */
    uint32_t played;                            /* the other's cards played this deal (pqb_bit) */
} pqx;

static uint32_t pqb_bit(int c) { return 1u << ((CARD_SUIT(c) & 3) * 8 + piquet_index(c)); }

static int pqb_card(int bit)                    /* the card of a bit of pqb_bit() */
{
    int s = bit / 8, k = bit % 8;
    return CARD(s, k == 7 ? RANK_ACE : k + 7);
}

/* ---- the rules as the core plays them ---------------------------------------------------------------------------- */
static void pq_add(tb_state *st, int pl, int v, int pique)
{
    pqx *x = TB_XS(st, pqx);
    x->dp[pl] = (int16_t)(x->dp[pl] + v);
    if (!pique || x->done) return;
    int e = x->elder, y = 1 - e;
    if (x->dp[y] == 0 && x->dp[e] >= 30) {
        x->done = 1;
        x->bonus[e] = (int16_t)(x->bonus[e] + 30);
    }
}

static void pq_trick_end(tb_state *st, int w)
{
    int leader = st->who[0];
    if (trick_rank(st->trick[0]) >= 10) pq_add(st, leader, 1, 1);   /* a lead of 10 or higher */
    if (w != leader && trick_rank(st->trick[1]) >= 10) pq_add(st, w, 1, 1);   /* the other's lead won with one */
    if (st->n[0] || st->n[1]) return;
    pq_add(st, w, 1, 1);                        /* the last trick */
    for (int q = 0; q < 2; q++) {
        if (st->won[q] == PIQUET_HAND) pq_add(st, q, 40, 0);   /* capot */
        else if (st->won[q] > PIQUET_HAND / 2) pq_add(st, q, 10, 0);   /* the cards */
    }
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static void pq_reward(const tb_state *st, float *rw)
{
    const pqx *x = TB_CXS(st, pqx);
    int s0 = x->dp[0] + x->bonus[0], s1 = x->dp[1] + x->bonus[1];
    if (!x->last) rw[0] = clamp01(0.5f + ((s0 - s1) - x->rootdiff) / 60.0f);
    else {                                      /* the partie: its winner, and the Rubicon's score */
        int t0 = x->total[0] + s0, t1 = x->total[1] + s1;
        float w = t0 > t1 ? 1 : t0 < t1 ? 0 : 0.5f, sc = 0;
        if (t0 != t1) {
            int tw = t0 > t1 ? t0 : t1, tl = t0 > t1 ? t1 : t0;
            sc = (float)(tl < 100 ? tw + tl + 100 : tw - tl + 100);
            if (t1 > t0) sc = -sc;
        }
        rw[0] = 0.7f * w + 0.3f * clamp01(0.5f + sc / 600);
    }
    rw[1] = 1 - rw[0];
}

/* the other's declarations against the twelve cards dealt to them: 0 when they match */
static int pqb_mismatch(const uint8_t *h, int n, const pqx *x)
{
    piquet_decl d;
    piquet_declare(h, n, &d);
    int m = 8 * abs(d.pn - x->pn) + 8 * abs(d.sn - x->sn) + 8 * abs(d.setn - x->setn);
    if (d.pn == x->pn) m += abs(d.pv - x->pv) < 8 ? abs(d.pv - x->pv) : 8;
    if (x->sn && d.sn == x->sn) m += 4 * (d.stop != x->stop) + 4 * (d.ssuit != x->ssuit);
    if (x->setn && d.setn == x->setn) m += 4 * (d.setrank != x->setrank);
    return m;
}

/* after each deal of the unseen cards: cards swapped between the other's unknown ones and the rest, keeping each swap
 * that brings their declarations no further from what they declared */
static void pq_dealt(tb_state *st)
{
    const pqx *x = TB_CXS(st, pqx);
    int o = 1 - st->me, n = st->n[o], k = x->nk, h = 0;
    if (!x->has || n <= k || !st->nrest) return;
    uint8_t H[PIQUET_HAND + 1];
    memcpy(H, st->hand[o], (size_t)n);
    for (int b = 0; b < 32 && h + n < PIQUET_HAND; b++)
        if (x->played >> b & 1) H[n + h++] = (uint8_t)pqb_card(b);
    int m = pqb_mismatch(H, n + h, x);
    if (!m) return;
    uint64_t key = 0;
    for (int i = 0; i < n; i++) key = key * 131 + st->hand[o][i];
    for (int i = 0; i < st->nrest; i++) key = key * 31 + st->rest[i];
    ai_rng r;
    ai_seed(&r, key);
    for (int t = 0; t < 40 && m; t++) {
        int i = k + ai_rand_n(&r, n - k), j = ai_rand_n(&r, st->nrest), c = st->rest[j];
        if (!tb_fits(st, o, c)) continue;
        uint8_t was = H[i];
        H[i] = (uint8_t)c;
        int m2 = pqb_mismatch(H, n + h, x);
        if (m2 <= m) {
            m = m2;
            st->rest[j] = was;
            st->hand[o][i] = (uint8_t)c;
        } else H[i] = was;
    }
}

static const tb_rules RULES = {.trick_end = pq_trick_end, .reward = pq_reward, .dealt = pq_dealt};

/* the core's x from a position (the deal's points so far, the totals) */
static void pqb_x(tb_state *v, const piquet_pos *p)
{
    pqx *x = TB_XS(v, pqx);
    for (int s = 0; s < 2; s++) {
        x->dp[s] = (int16_t)(p->dpts[s] + p->ppts[s]);
        x->bonus[s] = p->bonus[s];
        x->total[s] = p->total[s];
        v->won[s] = p->won[s];
    }
    x->elder = p->elder;
    x->done = p->pique != NOBODY || p->repique != NOBODY;
    x->last = p->deal == PIQUET_DEALS - 1;
    x->rootdiff = (int16_t)(x->dp[0] + x->bonus[0] - x->dp[1] - x->bonus[1]);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
}

/* the view of seat me (PQ_PLAY): its hand, the cards it threw, the plays, the other's declarations and shown cards */
void pqb_view(const piquet_game *g, int me, tb_state *v, piquet_game *t)
{
    const piquet_pos *p = &g->pos;
    int o = 1 - me;
    tb_init(v, &RULES, PIQUET_P, me);
    pqb_x(v, p);
    pqx *x = TB_XS(v, pqx);
    tb_hidden(v, o, p->n[o]);
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (PQA_KIND(g->act[i]) == PQA_NEXT) start = i + 1;
    piquet_new(t, g->seed);
    uint32_t shown = 0;
    for (int i = 0, k = 0, plays = p->tricks * PIQUET_P; i < g->n; i++) {
        int a = g->act[i];
        const piquet_pos *q = &t->pos;
        if (i >= start && PQA_KIND(a) == PQA_EXCHANGE && q->turn == me)
            for (int j = 0; j < q->n[me]; j++)  /* the cards it threw */
                if (PQA_ARG(a) >> j & 1) tb_seen(v, q->hand[me][j]);
        if (i >= start && PQA_KIND(a) == PQA_PLAY) {
            int c = PQA_ARG(a);
            uint8_t who[PIQUET_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % PIQUET_P);
            tb_note(v, q->turn, c, q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, c);
            if (q->turn == o) x->played |= pqb_bit(c);
        }
        int phase = q->phase;
        piquet_play(t, a);
        if (i >= start && phase == PQ_DECLARE && q->phase == PQ_PLAY) {   /* the board, as both see it */
            uint8_t c[PIQUET_HAND];
            for (int kind = 0; kind < PQ_KINDS; kind++)
                if (piquet_kind_winner(q, kind) == o)
                    for (int j = 0, nc = piquet_shown(q, o, kind, c); j < nc; j++) shown |= pqb_bit(c[j]);
            const piquet_decl *d = &q->decl[o];
            if (d->sn) shown |= pqb_bit(pqb_card(d->ssuit * 8 + d->stop));   /* the sequence's top card */
            for (int j = 0; j < q->n[o] && d->setn; j++)   /* the set's card (the first of its rank) */
                if (piquet_index(q->hand[o][j]) == d->setrank) { shown |= pqb_bit(q->hand[o][j]); break; }
        }
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % PIQUET_P, p->trick[j]);
    for (int b = 0; b < 32; b++)
        if ((shown & ~x->played) >> b & 1) tb_known(v, o, pqb_card(b));
    x->nk = v->nknown[o];
    if (p->phase == PQ_PLAY) {
        const piquet_decl *d = &p->decl[o];
        x->has = 1;
        x->pn = d->pn, x->pv = d->pv, x->sn = d->sn, x->stop = d->stop, x->ssuit = d->ssuit;
        x->setn = d->setn, x->setrank = d->setrank;
    }
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* ---- the exchange ----------------------------------------------------------------------------------------------- */
/* a twelve-card hand's worth: its declarations by their chance of counting, and its tricks */
static float pqb_worth(const uint8_t *h, int n)
{
    static const float pp[9] = {0, 0, 0, 0.02f, 0.15f, 0.5f, 0.85f, 0.97f, 1};   /* the point good, by its cards */
    static const float ps[9] = {0, 0, 0, 0.3f, 0.55f, 0.85f, 0.95f, 1, 1};       /* the best sequence good */
    piquet_decl d;
    piquet_declare(h, n, &d);
    float v = d.pn * clamp01(pp[d.pn > 8 ? 8 : d.pn] + (d.pn == 5 ? (d.pv - 42) * 0.01f : 0));
    if (d.sn) v += d.seqs * clamp01(ps[d.sn > 8 ? 8 : d.sn] + (d.sn <= 4 ? (d.stop - 5) * 0.04f : 0));
    if (d.setn == 4) v += 14 * (0.75f + 0.04f * (d.setrank - 3)) + (d.sets - 14) * 0.3f;
    else if (d.setn == 3) v += d.sets * (0.2f + 0.06f * (d.setrank - 3));
    float tricks = 0;
    for (int s = 0; s < 4; s++) {
        int has[8] = {0}, len = 0;
        for (int i = 0; i < n; i++)
            if ((CARD_SUIT(h[i]) & 3) == s) has[piquet_index(h[i])] = 1, len++;
        int sure = 0;
        while (sure < 8 && has[7 - sure]) sure++;
        float t = (float)sure;
        if (!sure) {
            if (has[6]) t += len >= 2 ? 0.6f : 0.25f;   /* a king, guarded */
            if (has[5] && len >= 3) t += 0.3f;          /* a queen, twice guarded */
        } else if (sure < len && sure < 7 && !has[7 - sure] && sure + 1 < 8 && has[6 - sure])
            t += 0.5f;                          /* a tenace under the top */
        if (len >= 5) t += (len - 4) * (sure ? 0.8f : 0.35f);
        tricks += t > len ? (float)len : t;
    }
    return v + 2.0f * tricks + 10.0f / (1 + expf(-(tricks - 5.5f) * 1.2f));
}

/* the younger of a simulation: takes the cards the elder left, throwing their weakest (low cards of short suits) */
static void pqb_young_takes(uint8_t *h, const uint8_t *draw, int k)
{
    for (int t = 0; t < k; t++) {
        int worst = 0;
        float wv = 1e9f;
        for (int i = 0; i < PIQUET_HAND; i++) {
            int len = 0;
            for (int j = 0; j < PIQUET_HAND; j++) len += CARD_SUIT(h[j]) == CARD_SUIT(h[i]);
            float val = (float)piquet_index(h[i]) * 1.2f + (float)len * 1.5f;
            if (val < wv) wv = val, worst = i;
        }
        h[worst] = draw[t];
    }
}

/* a simulated deal from position p: my hand and the other's set, declared by the rules, played by the heuristic;
 * my net score */
static float pqb_simulate(const piquet_pos *p, int me, const uint8_t *mine, const uint8_t *other, ai_rng *r)
{
    piquet_game *s = malloc(sizeof *s);
    tb_state *w = malloc(sizeof *w);
    float net = 0;
    if (s && w) {
        memset(s, 0, sizeof *s);
        s->pos = *p;
        s->seed = p->seed;
        piquet_pos *q = &s->pos;
        memcpy(q->hand[me], mine, PIQUET_HAND);
        memcpy(q->hand[1 - me], other, PIQUET_HAND);
        trick_sort(q->hand[0], PIQUET_HAND);
        trick_sort(q->hand[1], PIQUET_HAND);
        q->n[0] = q->n[1] = PIQUET_HAND;
        q->blanche[1 - me] = 0;                 /* unknown to me: not counted */
        q->phase = PQ_DECLARE;
        q->turn = q->elder;
        piquet_play(s, PQA_DECLARE);
        piquet_play(s, PQA_DECLARE);
        tb_init(w, &RULES, PIQUET_P, me);
        pqb_x(w, q);
        for (int pl = 0; pl < 2; pl++) memcpy(w->hand[pl], q->hand[pl], PIQUET_HAND), w->n[pl] = w->nknown[pl] = 12;
        w->full = 1;
        w->eps = 8;
        uint8_t L[TB_HAND];
        for (int guard = 0; guard < 40 && !tb_over(w); guard++) {
            int n = tb_legal(w, L);
            if (!n) break;
            tb_play(w, tb_heuristic(w, L, n, r));
        }
        const pqx *x = TB_CXS(w, pqx);
        net = (float)(x->dp[me] + x->bonus[me] - x->dp[1 - me] - x->bonus[1 - me]);
    }
    free(s);
    free(w);
    return net;
}

#define PQB_MAXOPT 1600
#define PQB_SAMPLES 96

/* the cards to throw (a mask of the sorted hand) */
static int pqb_exchange(const piquet_pos *p, int me, int level, uint64_t seed)
{
    const uint8_t *h = p->hand[me];
    int lo = piquet_throw_min(p), hi = piquet_throw_max(p), elder = me == p->elder, left = PIQUET_TALON - p->top;
    int s1 = level <= AI_EASY ? 4 : level == AI_MEDIUM ? 8 : 16;
    int top = level <= AI_EASY ? 0 : level == AI_MEDIUM ? 4 : 8, s2 = level == AI_MEDIUM ? 32 : PQB_SAMPLES;
    uint8_t unseen[32], pack[CARDS_MAX];
    int nu = 0, np = cards_pack(PACK_32, pack);
    for (int i = 0; i < np; i++)
        if (trick_find(h, PIQUET_HAND, pack[i]) < 0) unseen[nu++] = pack[i];
    ai_rng r;
    ai_seed(&r, ai_mix(seed, 0x9e1 + (uint64_t)p->deal * 977 + (uint64_t)p->top));
    uint8_t (*perm)[32] = malloc(sizeof(uint8_t[32]) * PQB_SAMPLES);
    int *opt = malloc(sizeof(int) * PQB_MAXOPT);
    float *val = malloc(sizeof(float) * PQB_MAXOPT);
    int best = lo ? 1 : 0, nopt = 0;
    if (!perm || !opt || !val) goto out;
    for (int j = 0; j < PQB_SAMPLES; j++) {     /* common draws for every option */
        memcpy(perm[j], unseen, (size_t)nu);
        for (int i = nu - 1; i > 0; i--) {
            int k = ai_rand_n(&r, i + 1);
            uint8_t t = perm[j][i]; perm[j][i] = perm[j][k]; perm[j][k] = t;
        }
    }
    for (int m = 0; m < 1 << PIQUET_HAND && nopt < PQB_MAXOPT; m++) {
        int k = __builtin_popcount((unsigned)m);
        if (k < lo || k > hi) continue;
        float sum = 0;
        for (int j = 0; j < s1; j++) {
            uint8_t nh[PIQUET_HAND];
            int c = 0;
            for (int i = 0; i < PIQUET_HAND; i++)
                if (!(m >> i & 1)) nh[c++] = h[i];
            for (int i = 0; i < k; i++) nh[c++] = perm[j][i];
            sum += pqb_worth(nh, PIQUET_HAND);
        }
        opt[nopt] = m;
        val[nopt++] = sum / s1 - (elder ? 1.5f * (hi - k) : 0);
    }
    if (ai_cancelled()) goto out;
    for (int i = 0; i < nopt && i < (top ? top : 3); i++)   /* the best few first */
        for (int j = i + 1; j < nopt; j++)
            if (val[j] > val[i]) {
                float tv = val[i]; val[i] = val[j]; val[j] = tv;
                int to = opt[i]; opt[i] = opt[j]; opt[j] = to;
            }
    if (!nopt) goto out;
    if (!top) { best = opt[ai_rand_n(&r, nopt < 3 ? nopt : 3)]; goto out; }
    float bv = -1e9f;
    for (int i = 0; i < top && i < nopt && !ai_cancelled(); i++) {
        int m = opt[i], k = __builtin_popcount((unsigned)m);
        float sum = 0;
        for (int j = 0; j < s2; j++) {
            uint8_t nh[PIQUET_HAND], oh[PIQUET_HAND];
            int c = 0;
            for (int q = 0; q < PIQUET_HAND; q++)
                if (!(m >> q & 1)) nh[c++] = h[q];
            for (int q = 0; q < k; q++) nh[c++] = perm[j][q];
            memcpy(oh, perm[j] + left, PIQUET_HAND);   /* the other's hand: past the talon's cards */
            if (elder) pqb_young_takes(oh, perm[j] + k, left - k);
            ai_rng rr;
            ai_seed(&rr, ai_mix(seed, (uint64_t)j));
            sum += pqb_simulate(p, me, nh, oh, &rr);
        }
        float v = sum / s2 - (elder ? 0.5f * (hi - k) : 0);
        if (v > bv) bv = v, best = m;
    }
out:
    free(perm);
    free(opt);
    free(val);
    return best;
}

/* ---- the choice --------------------------------------------------------------------------------------------------- */
/* the action for seat at a level (0: nothing to do) */
int pqb_choose(const piquet_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const piquet_pos *p = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase == PQ_ROUND) return PQA_NEXT;
    if (p->phase == PQ_OVER || p->turn != seat) return 0;
    if (p->phase == PQ_DECLARE) return PQA_DECLARE;
    int a = 0;
    if (p->phase == PQ_EXCHANGE) {
        a = PQA_EXCHANGE | pqb_exchange(p, seat, level, seed);
        return piquet_legal(p, a) ? a : piquet_auto(p);
    }
    tb_state *v = malloc(sizeof *v);
    piquet_game *t = malloc(sizeof *t);
    if (v && t) {
        pqb_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, PIQUET_HARD_ITERS, stats);
        if (c != TB_NONE && piquet_can_play(p, c)) a = PQA_PLAY | c;
    }
    free(v);
    free(t);
    return a ? a : piquet_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    piquet_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *piquet_live();                        /* the copy, at entry: nothing else is read */
    int a = pqb_choose(g, seat, level, ai_session_seed(), NULL);
    int ok = a && !ai_cancelled() && cap >= 16;
    if (ok) {
        char c[3];
        switch (PQA_KIND(a)) {
        case PQA_EXCHANGE: {
            int k = snprintf(move, (size_t)cap, "e");
            for (int i = 0; i < g->pos.n[seat]; i++)
                if (PQA_ARG(a) >> i & 1) {
                    trick_name(g->pos.hand[seat][i], c);
                    k += snprintf(move + k, (size_t)(cap - k), "%s", c);
                }
            break;
        }
        case PQA_DECLARE: snprintf(move, (size_t)cap, "k"); break;
        case PQA_PLAY: trick_name(PQA_ARG(a), c), snprintf(move, (size_t)cap, "%s", c); break;
        case PQA_NEXT: snprintf(move, (size_t)cap, "n"); break;
        default: ok = 0;
        }
    }
    free(g);
    return ok;
}

static int actor(void)
{
    const piquet_pos *p = &piquet_live()->pos;
    return p->phase <= PQ_PLAY ? p->turn : -1;
}

static const bot piquet_bot = {.think = think, .actor = actor, .play = piquet_show};
REGISTER_BOT(piquet, &piquet_bot)
