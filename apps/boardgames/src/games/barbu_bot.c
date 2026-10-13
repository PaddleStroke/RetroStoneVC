/*
 * Barbu's bot (bots.h). The trick contracts are searched by the trick bots' core (trickbot.h: information-set Monte
 * Carlo over deals of the unseen cards that keep to what each play showed: a suit not followed; in trumps, a trump not
 * played, or not over the best one, when it had to be; in the barbu contract, a king of hearts kept when it had to go;
 * a heart led while hearts may not be led: nothing else in hand); dominoes by a search of its own on the same plan
 * (deals of the unseen cards; a player who passed holds none of the cards that could be laid then). The reward is the
 * deal's score with the doubles settled; each player for himself. Runs on the worker thread: it copies the game at
 * entry and reads only what its seat may know (its hand, the contract, the doubles, the cards played or laid, the
 * passes). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The contract (declaring): each contract still free (each trump suit, each starting rank for dominoes) is played out
 * on simulated deals of the unseen cards (the core's heuristic, or the dominoes' one) for the declarer's expected
 * score; it takes the one that beats that contract's usual score by most (every contract must be played once, so a
 * hand is spent where it is best compared with an ordinary hand, as these simulations score one: no tricks -6.8, no
 * hearts -7.9, no queens -6.1, the barbu -5.1, the last two -7.6, trumps +22.5, dominoes +28.4).
 * Doubling, from the same simulations of the contract named: it doubles each player it may whose expected score is
 * at least 6 under its own (in the positive contracts: 8), and redoubles those who doubled it on the same terms.
 * The deal's summary waits for a human tap (actor() -1); think() answers "n" there all the same (the tests).
 * Its moves are the game's notation ("c5h", "c6J", "d13", "r2", "d", "Qs", "p", "n"), played by barbu_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/barbu.h"
#include "games/trickbot.h"

#define BARBU_HARD_ITERS 14000                  /* about 0.3 s a card on a PC */
#define DOMINO_HARD_ITERS 12000
#define BB_PASS_MOVE 1000                       /* dominoes: the pass, as a search's move */

static const int8_t DOMINO_PTS[BARBU_P] = {45, 20, 5, -5};
static const float RANGE[BARBU_CONTRACTS] = {26, 30, 24, 20, 30, 65, 50};       /* a deal's scores' spread */
static const float USUAL[BARBU_CONTRACTS] = {-6.8f, -7.9f, -6.1f, -5.1f, -7.6f, 22.5f, 28.4f};   /* measured */

/* the doubles settled: each pair doubled moves the difference of their scores (twice when redoubled) */
static void settle(const uint8_t dbl[BARBU_P][BARBU_P], const int *score, int *final)
{
    for (int q = 0; q < BARBU_P; q++) final[q] = score[q];
    for (int a = 0; a < BARBU_P; a++)
        for (int b = 0; b < BARBU_P; b++)
            if (dbl[a][b]) {
                int d = (score[a] - score[b]) * dbl[a][b];
                final[a] += d;
                final[b] -= d;
            }
}

/* ==== the trick contracts on the core ============================================================================ */
typedef struct {
    uint8_t contract, taken, dbl[BARBU_P][BARBU_P];
    int16_t last2[BARBU_P];                     /* the last two tricks' penalties taken (10, 20) */
} bbx;

static int x_contract(const tb_state *st) { return TB_CXS(st, bbx)->contract; }

static int b_points(const tb_state *st, int c) { return -barbu_card_points(x_contract(st), c); }   /* as a penalty */

static int b_order(const tb_state *st, int c) { (void)st; return trick_rank(c); }   /* the caps: rank in the suit */

static int b_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int ct = x_contract(st), k = 0;
    if (!st->ntrick) {
        int only = trick_only_suit(h, n, SUIT_HEARTS);
        for (int i = 0; i < n; i++)
            if (!((ct == BB_NOHEARTS || ct == BB_KING) && CARD_SUIT(h[i]) == SUIT_HEARTS && !only)) out[k++] = h[i];
        return k;
    }
    int led = CARD_SUIT(st->trick[0]), trump = ct == BB_TRUMPS ? st->trump : -1, bt = 0;
    for (int i = 0; i < st->ntrick; i++)
        if (CARD_SUIT(st->trick[i]) == trump && trick_rank(st->trick[i]) > bt) bt = trick_rank(st->trick[i]);
    int has_led = trick_has_suit(h, n, led), suit = has_led ? led : -1, over = 0;
    if (!has_led && ct == BB_KING && trick_find(h, n, BARBU_KING) >= 0) { out[0] = BARBU_KING; return 1; }
    if (!has_led && trump >= 0 && trick_has_suit(h, n, trump)) suit = trump;
    if (suit < 0) { memcpy(out, h, (size_t)n); return n; }
    int beat = suit == trump;                   /* trumps led, or trumping: over the best trump if possible */
    for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == suit && trick_rank(h[i]) > bt;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == suit && (!beat || !over || trick_rank(h[i]) > bt)) out[k++] = h[i];
    return k;
}

static void b_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    int ct = x_contract(st), s = CARD_SUIT(card);
    uint8_t *cap = st->cap[seat];
#define B_CAP(u, r) do { if (cap[(u)] == TB_NOCAP || cap[(u)] > (r)) cap[(u)] = (uint8_t)(r); } while (0)
    if (!n) {                                   /* a heart led where it may not be: nothing but hearts */
        if ((ct == BB_NOHEARTS || ct == BB_KING) && s == SUIT_HEARTS)
            for (int u = 0; u < 4; u++)
                if (u != SUIT_HEARTS) B_CAP(u, 0);
        return;
    }
    int led = CARD_SUIT(trick[0]);
    if (ct == BB_KING && s != led && card != BARBU_KING) tb_exclude(st, seat, BARBU_KING);
    if (ct != BB_TRUMPS) return;
    int trump = st->trump, bt = 0;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(trick[i]) == trump && trick_rank(trick[i]) > bt) bt = trick_rank(trick[i]);
    if (s == led && led == trump && trick_rank(card) < bt) B_CAP(trump, bt);   /* trumps led, not beaten */
    if (s != led) {
        if (s != trump) B_CAP(trump, 0);        /* void and no trump: none */
        else if (trick_rank(card) < bt) B_CAP(trump, bt);   /* trumped under: none over */
    }
#undef B_CAP
}

static void b_trick_end(tb_state *st, int w)
{
    bbx *x = TB_XS(st, bbx);
    for (int i = 0; i < st->ntrick; i++)
        x->taken = (uint8_t)(x->taken + (barbu_card_points(x->contract, st->trick[i]) != 0));
    if (x->contract == BB_LASTTWO && st->tricks >= 11)   /* (the trick being gathered: tricks + 1) */
        x->last2[w] = (int16_t)(x->last2[w] + (st->tricks == 11 ? 10 : 20));
}

static int b_over(const tb_state *st)
{
    const bbx *x = TB_CXS(st, bbx);
    if (st->ntrick) return 0;
    switch (x->contract) {
    case BB_NOHEARTS: return x->taken >= 13;
    case BB_NOQUEENS: return x->taken >= 4;
    case BB_KING: return x->taken >= 1;
    default: return 0;
    }
}

static int contract_score(const tb_state *st, int s)
{
    const bbx *x = TB_CXS(st, bbx);
    switch (x->contract) {
    case BB_NOTRICKS: return -2 * st->won[s];
    case BB_LASTTWO: return -x->last2[s];
    case BB_TRUMPS: return 5 * st->won[s];
    default: return -st->pts[s];
    }
}

static void b_reward(const tb_state *st, float *rw)
{
    const bbx *x = TB_CXS(st, bbx);
    int score[BARBU_P], final[BARBU_P];
    for (int s = 0; s < BARBU_P; s++) score[s] = contract_score(st, s);
    settle(x->dbl, score, final);
    for (int s = 0; s < BARBU_P; s++) rw[s] = 0.5f + final[s] / (2.5f * RANGE[x->contract]);
}

static int b_want(const tb_state *st, int seat)
{
    (void)seat;
    int ct = x_contract(st);
    if (ct == BB_TRUMPS) return 1;
    if (ct == BB_LASTTWO) return st->tricks < 9 ? 1 : -1;   /* the early tricks shed the high cards */
    return -1;
}

static const tb_rules RULES = {
    .order = b_order, .legal = b_legal, .points = b_points, .trick_end = b_trick_end, .over = b_over,
    .reward = b_reward, .want = b_want, .infer = b_infer,
};

static int deal_start(const barbu_game *g)
{
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (BBA_KIND(g->act[i]) == BBA_NEXT) start = i + 1;
    return start;
}

/* the view of seat me in a trick contract (choosing or doubling: contract c, trump t, at the deal's start) */
static void view_as(const barbu_game *g, int me, tb_state *v, barbu_game *t, int c, int trump)
{
    const barbu_pos *p = &g->pos;
    tb_init(v, &RULES, BARBU_P, me);
    v->trump = c == BB_TRUMPS ? (uint8_t)trump : TB_NONE;
    int playing = p->phase == BBP_PLAY;
    v->turn = playing ? p->turn : p->declarer;
    v->leader = playing ? p->leader : p->declarer;
    v->tricks = p->tricks;
    bbx *x = TB_XS(v, bbx);
    x->contract = (uint8_t)c;
    x->taken = p->taken;
    memcpy(x->dbl, p->dbl, sizeof x->dbl);
    for (int s = 0; s < BARBU_P; s++) {
        v->won[s] = p->won[s];
        if (c == BB_NOHEARTS || c == BB_NOQUEENS || c == BB_KING) v->pts[s] = (int16_t)-p->pts[s];
        if (c == BB_LASTTWO) x->last2[s] = (int16_t)-p->pts[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    int start = deal_start(g), plays = p->tricks * BARBU_P;
    barbu_new(t, p->variant, p->seed);
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && BBA_KIND(a) == BBA_PLAY) {
            const barbu_pos *q = &t->pos;
            uint8_t who[BARBU_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % BARBU_P);
            tb_note(v, q->turn, BBA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, BBA_ARG(a));
        }
        barbu_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % BARBU_P, p->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_52, pack));
}

void bab_view(const barbu_game *g, int me, tb_state *v, barbu_game *t)
{
    view_as(g, me, v, t, g->pos.contract, g->pos.trump);
}

/* ==== dominoes: a search of its own ============================================================================== */
typedef struct {
    uint8_t hand[BARBU_P][BARBU_HAND], n[BARBU_P], nknown[BARBU_P];
    uint8_t lo[4], hi[4], rank, turn, me, nout, out[BARBU_P], full, eps;
    uint8_t dbl[BARBU_P][BARBU_P];
    uint64_t ex[BARBU_P][2];                    /* the cards each seat cannot hold (by the card's byte & 127) */
    uint8_t unseen[52], nunseen;
} dstate;

static int d_lays(const dstate *d, int c)
{
    int s = CARD_SUIT(c), r = trick_rank(c);
    if (!d->lo[s]) return r == d->rank;
    return r == d->lo[s] - 1 || r == d->hi[s] + 1;
}

static int d_excluded(const dstate *d, int s, int c) { return (int)(d->ex[s][(c & 127) >> 6] >> (c & 63) & 1); }

static int d_over(const dstate *d) { return d->nout >= BARBU_P; }

static int d_moves(const void *st, int *out)
{
    const dstate *d = st;
    if (d_over(d)) return 0;
    int k = 0, t = d->turn;
    for (int i = 0; i < d->n[t]; i++)
        if (d_lays(d, d->hand[t][i])) out[k++] = d->hand[t][i];
    if (!k) out[k++] = BB_PASS_MOVE;
    return k;
}

static int d_next_in(const dstate *d, int pl)
{
    for (int k = 1; k <= BARBU_P; k++) {
        int q = (pl + k) % BARBU_P;
        if (d->n[q]) return q;
    }
    return pl;
}

static void d_make(void *st, int m)
{
    dstate *d = st;
    int pl = d->turn;
    if (m != BB_PASS_MOVE) {
        uint8_t *h = d->hand[pl];
        for (int i = 0; i < d->n[pl]; i++)
            if (h[i] == m) {
                memmove(h + i, h + i + 1, (size_t)(d->n[pl] - i - 1));
                d->n[pl]--;
                break;
            }
        int s = CARD_SUIT(m), r = trick_rank(m);
        if (!d->lo[s]) d->lo[s] = d->hi[s] = (uint8_t)r;
        else if (r < d->lo[s]) d->lo[s] = (uint8_t)r;
        else d->hi[s] = (uint8_t)r;
        if (!d->n[pl]) d->out[d->nout++] = (uint8_t)pl;
        if (d->nout == BARBU_P - 1) {           /* the last one left */
            d->out[d->nout++] = (uint8_t)d_next_in(d, pl);
            return;
        }
    }
    d->turn = (uint8_t)d_next_in(d, pl);
}

static int d_side(const void *st) { return ((const dstate *)st)->turn; }

static void d_scores(const dstate *d, int *final)
{
    int score[BARBU_P] = {0};
    for (int i = 0; i < d->nout; i++) score[d->out[i]] = DOMINO_PTS[i];
    settle(d->dbl, score, final);
}

static int d_result(const void *st, float *rw)
{
    const dstate *d = st;
    if (!d_over(d)) return 0;
    int final[BARBU_P];
    d_scores(d, final);
    for (int s = 0; s < BARBU_P; s++) {
        float r = 0.5f + final[s] / (2.5f * RANGE[BB_DOMINO]);
        rw[s] = r < 0 ? 0 : r > 1 ? 1 : r;
    }
    return 1;
}

/* the playouts: lay the card that opens the most of one's own cards beyond it and the fewest of the others' */
static int d_playout(const void *st, ai_rng *r)
{
    const dstate *d = st;
    int mv[BARBU_HAND + 1], n = d_moves(st, mv);
    if (n <= 1 || ai_rand_n(r, 100) < d->eps) return n ? mv[ai_rand_n(r, n)] : AI_NONE;
    int t = d->turn, best = 0, bs = -1000;
    for (int i = 0; i < n; i++) {
        int c = mv[i], s = CARD_SUIT(c), rk = trick_rank(c), mine = 0, theirs = 0;
        int up = !d->lo[s] || rk > d->hi[s];    /* the row grows this way (a starter: both) */
        for (int q = 0; q < BARBU_P; q++)
            for (int j = 0; j < d->n[q]; j++) {
                int e = d->hand[q][j], er = trick_rank(e);
                if (e == c || CARD_SUIT(e) != s) continue;
                int beyond = !d->lo[s] ? 1 : up ? er > rk : er < rk;
                if (!beyond) continue;
                if (q == t) mine++;
                else theirs++;
            }
        int sc = 3 * mine - theirs + ai_rand_n(r, 2);
        if (sc > bs) bs = sc, best = i;
    }
    return mv[best];
}

static void d_determinise(void *st, int seat, ai_rng *r)
{
    (void)seat;
    dstate *d = st;
    if (d->full) return;
    uint8_t pool[52];
    int nu = d->nunseen;
    memcpy(pool, d->unseen, (size_t)nu);
    for (int attempt = 0; attempt < 16; attempt++) {
        for (int i = nu - 1; i > 0; i--) {
            int j = ai_rand_n(r, i + 1);
            uint8_t x = pool[i]; pool[i] = pool[j]; pool[j] = x;
        }
        int need[BARBU_P], ok = 1, strict = attempt < 14;
        uint8_t fill[BARBU_P][BARBU_HAND];
        for (int s = 0; s < BARBU_P; s++) need[s] = s == d->me ? 0 : d->n[s] - d->nknown[s];
        for (int i = 0; i < nu && ok; i++) {    /* each card to a seat with room that may hold it */
            int c = pool[i], room = 0;
            for (int s = 0; s < BARBU_P; s++)
                room += need[s] > 0 && (!strict || !d_excluded(d, s, c)) ? need[s] : 0;
            if (!room) { ok = 0; break; }
            int x = ai_rand_n(r, room), s = 0;
            for (; s < BARBU_P; s++) {
                if (need[s] <= 0 || (strict && d_excluded(d, s, c))) continue;
                if (x < need[s]) break;
                x -= need[s];
            }
            fill[s][d->n[s] - d->nknown[s] - need[s]] = (uint8_t)c;
            need[s]--;
        }
        if (!ok) continue;
        for (int s = 0; s < BARBU_P; s++)
            for (int k = d->nknown[s]; k < d->n[s]; k++) d->hand[s][k] = fill[s][k - d->nknown[s]];
        break;
    }
    for (int s = 0; s < BARBU_P; s++) d->nknown[s] = d->n[s];
    d->full = 1;
}

/* the view of seat me in dominoes (rank: the starting rank, at the deal's start when choosing or doubling) */
static void d_view(const barbu_game *g, int me, dstate *d, barbu_game *t, int rank)
{
    const barbu_pos *p = &g->pos;
    memset(d, 0, sizeof *d);
    d->me = (uint8_t)me;
    d->rank = (uint8_t)rank;
    d->eps = 8;
    int playing = p->phase == BBP_PLAY && p->contract == BB_DOMINO;
    d->turn = playing ? p->turn : p->declarer;
    memcpy(d->dbl, p->dbl, sizeof d->dbl);
    if (playing) {
        memcpy(d->lo, p->lo, 4);
        memcpy(d->hi, p->hi, 4);
        d->nout = p->nout;
        memcpy(d->out, p->out, sizeof d->out);
    }
    for (int s = 0; s < BARBU_P; s++) d->n[s] = p->n[s];
    memcpy(d->hand[me], p->hand[me], p->n[me]);
    d->nknown[me] = p->n[me];
    if (playing) {                              /* the passes replayed: what each could not lay then */
        int start = deal_start(g);
        barbu_new(t, p->variant, p->seed);
        for (int i = 0; i < g->n; i++) {
            int a = g->act[i];
            if (i >= start && BBA_KIND(a) == BBA_PASS) {
                const barbu_pos *q = &t->pos;
                dstate tmp = *d;
                memcpy(tmp.lo, q->lo, 4);
                memcpy(tmp.hi, q->hi, 4);
                for (int s = 0; s < 4; s++)
                    for (int rk = 1; rk <= 13; rk++) {
                        int c = CARD(s, rk);
                        if (d_lays(&tmp, c)) d->ex[q->turn][(c & 127) >> 6] |= 1ull << (c & 63);
                    }
            }
            barbu_play(t, a);
        }
    }
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack);
    for (int i = 0; i < n; i++) {
        int c = pack[i], s = CARD_SUIT(c), r = trick_rank(c), mine = 0;
        if (d->lo[s] && r >= d->lo[s] && r <= d->hi[s]) continue;   /* laid */
        for (int k = 0; k < p->n[me]; k++) mine |= p->hand[me][k] == c;
        if (!mine) d->unseen[d->nunseen++] = (uint8_t)c;
    }
}

static const ai_game DOMINO = {
    .size = sizeof(dstate), .moves = d_moves, .make = d_make, .side = d_side, .players = BARBU_P,
    .result = d_result, .playout = d_playout, .determinise = d_determinise,
};

/* the expected score of every seat over `samples` dealt worlds played out by the playouts' heuristic */
static void d_simulate(const dstate *v, int samples, ai_rng *r, float *score)
{
    float sum[BARBU_P] = {0};
    int done = 0;
    for (; done < samples && !ai_cancelled(); done++) {
        dstate w = *v;
        d_determinise(&w, w.me, r);
        for (int guard = 0; guard < 400 && !d_over(&w); guard++) d_make(&w, d_playout(&w, r));
        int final[BARBU_P];
        d_scores(&w, final);
        for (int s = 0; s < BARBU_P; s++) sum[s] += final[s];
    }
    for (int s = 0; s < BARBU_P; s++) score[s] = done ? sum[s] / done : 0;
}

/* ==== the calls ================================================================================================= */
/* each seat's expected score in contract c (x: the trump suit or the dominoes' rank), from me's view */
static void expect(const barbu_game *g, int me, int c, int x, int samples, ai_rng *r, barbu_game *t, float *score)
{
    if (c == BB_DOMINO) {
        dstate *d = malloc(sizeof *d);
        if (!d) { memset(score, 0, sizeof(float) * BARBU_P); return; }
        d_view(g, me, d, t, x);
        memset(d->dbl, 0, sizeof d->dbl);
        d_simulate(d, samples, r, score);
        free(d);
        return;
    }
    tb_state *v = malloc(sizeof *v);
    if (!v) { memset(score, 0, sizeof(float) * BARBU_P); return; }
    view_as(g, me, v, t, c, x);
    memset(TB_XS(v, bbx)->dbl, 0, sizeof TB_XS(v, bbx)->dbl);
    float rw[TB_MAXP];
    tb_simulate(v, samples, r, NULL, NULL, rw);
    for (int s = 0; s < BARBU_P; s++) score[s] = (rw[s] - 0.5f) * 2.5f * RANGE[c];
    free(v);
}

static int samples_of(int level) { return level <= AI_EASY ? 12 : level == AI_MEDIUM ? 32 : 96; }

static int choose_contract(const barbu_game *g, int me, int level, ai_rng *r, barbu_game *t)
{
    const barbu_pos *p = &g->pos;
    int best = 0;
    float bv = -1e9f;
    for (int c = 0; c < BARBU_CONTRACTS; c++) {
        if (!barbu_can_choose(p, c)) continue;
        int lo = 0, hi = 0;
        if (c == BB_TRUMPS) hi = 3;
        if (c == BB_DOMINO) lo = 2, hi = 14;
        for (int x = lo; x <= hi; x++) {
            float sc[BARBU_P];
            expect(g, me, c, x, samples_of(level) / (c == BB_DOMINO ? 2 : 1), r, t, sc);
            float v = sc[me] - USUAL[c];
            if (v > bv) bv = v, best = BBA_CONTRACT(c, x);
        }
    }
    return best ? best : barbu_auto(p);
}

static int doubling(const barbu_game *g, int me, int level, ai_rng *r, barbu_game *t)
{
    const barbu_pos *p = &g->pos;
    unsigned mask = barbu_double_mask(p);
    if (!mask) return p->phase == BBP_DOUBLE ? BBA_DOUBLE : BBA_REDOUBLE;
    float sc[BARBU_P];
    expect(g, me, p->contract, p->contract == BB_TRUMPS ? p->trump : p->rank, samples_of(level), r, t, sc);
    float margin = p->contract >= BB_TRUMPS ? 8 : 6;
    int who = 0;
    for (int q = 0; q < BARBU_P; q++)
        if (mask >> q & 1 && sc[me] - sc[q] >= margin) who |= 1 << q;
    return (p->phase == BBP_DOUBLE ? BBA_DOUBLE : BBA_REDOUBLE) | who;
}

/* the action for seat at a level (0: nothing to do) */
int bab_choose(const barbu_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const barbu_pos *p = &g->pos;
    if (p->phase == BBP_ROUND) return BBA_NEXT;
    if (p->phase == BBP_OVER || p->turn != seat) return 0;
    barbu_game *t = malloc(sizeof *t);
    int a = 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 16 + (uint64_t)p->phase * 4 + seat));
    if (t) {
        if (p->phase == BBP_CHOOSE) a = choose_contract(g, seat, level, &r, t);
        else if (p->phase == BBP_DOUBLE || p->phase == BBP_REDOUBLE) a = doubling(g, seat, level, &r, t);
        else if (p->contract == BB_DOMINO) {
            dstate *d = malloc(sizeof *d);
            if (d) {
                d_view(g, seat, d, t, p->rank);
                tb_level_t lv = tb_level(level, DOMINO_HARD_ITERS);
                d->eps = (uint8_t)lv.eps;
                ai_opts o = {.budget = ai_level_budget(level), .rng = &r, .uct_c = 0.7f};
                o.budget.nodes = lv.iters;
                int m = ai_mcts(&DOMINO, d, &o, stats);
                a = m == BB_PASS_MOVE ? BBA_PASS : m >= 0 ? BBA_PLAY | m : 0;
                free(d);
            }
        } else {
            tb_state *v = malloc(sizeof *v);
            if (v) {
                bab_view(g, seat, v, t);
                int c = tb_choose(v, level, seed, BARBU_HARD_ITERS, stats);
                if (c != TB_NONE) a = BBA_PLAY | c;
                free(v);
            }
        }
    }
    free(t);
    return a && barbu_legal(p, a) ? a : barbu_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    barbu_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *barbu_live();                         /* the copy, at entry: nothing else is read */
    int a = bab_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 12) return 0;
    static const char ranks[] = "23456789TJQKA", suits[] = "shdc";
    int x = BBA_ARG(a), k = 0;
    char c[3];
    switch (BBA_KIND(a)) {
    case BBA_CHOOSE: {
        int ct = x & 15, y = x >> 4;
        k = snprintf(move, (size_t)cap, "c%d", ct);
        if (ct == BB_TRUMPS) move[k++] = suits[y & 3], move[k] = 0;
        if (ct == BB_DOMINO) move[k++] = ranks[(y - 2) % 13], move[k] = 0;
        return 1;
    }
    case BBA_DOUBLE:
    case BBA_REDOUBLE:
        move[k++] = BBA_KIND(a) == BBA_DOUBLE ? 'd' : 'r';
        for (int q = 0; q < BARBU_P; q++)
            if (x >> q & 1) move[k++] = (char)('0' + q);
        move[k] = 0;
        return 1;
    case BBA_PLAY: trick_name(x, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case BBA_PASS: snprintf(move, (size_t)cap, "p"); return 1;
    case BBA_NEXT: snprintf(move, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const barbu_pos *p = &barbu_live()->pos;
    return p->phase < BBP_ROUND ? p->turn : -1;
}

static const bot barbu_bot = {.think = think, .actor = actor, .play = barbu_show};
REGISTER_BOT(barbu, &barbu_bot)
