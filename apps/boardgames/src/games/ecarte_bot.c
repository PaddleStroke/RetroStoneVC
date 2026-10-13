/*
 * Écarté's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards), the reward the game itself: who reaches 5 first, from the totals after the deal; the
 * proposals, answers and discards from the hand's chances in simulated deals. Runs on the worker thread: it copies the
 * game at entry and reads only what its seat may know. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the seat knows (the view): its five cards, the turned card, the cards it threw (the deal's actions replayed on
 * a scratch game from the seed), the cards played and what each play shows (a suit not followed: void; following
 * under the card led: nothing higher in that suit, since one must win the trick if one can; neither following nor
 * trumping: no trumps), and the king of trumps: its point is counted before the play for whoever holds it, so the
 * other holds it when they scored it, and does not when nobody did. The other's discards and the stock are unseen
 * (rest[]).
 *
 * The value of a deal's result: the chance of winning the game from the totals it leaves (a table worked out at each
 * call: from a score a to b, each later deal won by either side alike, for 1 point four times in five, 2 the fifth;
 * 5 points win). The search's reward is that chance after the deal as the rules score it (three or four tricks 1, the
 * vole 2; 2 for the elder's opponent when the elder played without proposing and lost, 2 for the elder when the
 * dealer refused the first proposal and lost).
 * P, the hand's chance: simulated deals from the seat's view (the other's five cards dealt at random from the unseen
 * cards; both sides played by the core's heuristic, the elder leading): the share of deals it takes three tricks
 * (and the vole). Easy deals 24, medium 64, hard 160 (and easy errs by up to a tenth).
 * - The elder, first: plays without proposing when P's value (a win, or 2 to the other if it loses) beats proposing's
 *   (the exchange taken to leave a chance of 1/4 + P/2, each side scoring 1); later proposals: proposes while P is under
 *   one half and the stock lasts.
 * - The dealer, first proposal: refuses when P (counted a twentieth better: the elder who proposes holds no strong
 *   hand) under the refusal's stakes (2 to the elder if it loses) beats accepting (1/4 + P/2, 1 point either way);
 *   later proposals: refuses when P is one half or more (no penalty then).
 * - The discard (the elder at least one card): every set of cards it may throw is tried in simulated deals: the
 *   draws from the unseen cards, the other's hand dealt from the rest, the deal played out; the set whose deals leave
 *   the best chance of the game wins (the same draws for every set). Easy tries 12 deals a set, medium 40, hard 96.
 * The round's summary waits for a human tap (actor() -1); think() answers "n" there all the same.
 * The game has one variant (to 5 points); the bot plays it.
 * Its moves are the game's notation ("q", "y", "x", "eQs7h", "e", "Kh", "n"), played by ecarte_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/ecarte.h"
#include "games/trickbot.h"

#ifndef ECB_Q0                                  /* the calls (see the header) */
#define ECB_Q0 0.25f                            /* the exchange leaves a chance of Q0 + Q1 P */
#define ECB_Q1 0.5f
#define ECB_DEALER 0.05f                        /* the dealer's P when the elder has proposed */
#define ECB_LATER 0.5f                          /* later proposals: below it, propose or accept */
#endif
#ifndef ECARTE_HARD_ITERS
#define ECARTE_HARD_ITERS 20000                 /* about 15 ms a card on a PC: five cards (4x more: no better) */
#endif

typedef struct {
    float r0[2][3];                             /* seat 0's chance of the game when seat w scores v (1, 2) */
    uint8_t elder, dealer, forced, refused;
} ecx;

/* ---- the game's value: seat 0's chance from totals a (seat 0) and b ------------------------------------------------ */
static float ecb_value(int a, int b)
{
    if (a >= ECARTE_GOAL) return 1;
    if (b >= ECARTE_GOAL) return 0;
    float t[ECARTE_GOAL + 2][ECARTE_GOAL + 2];
    for (int i = ECARTE_GOAL + 1; i >= 0; i--)
        for (int j = ECARTE_GOAL + 1; j >= 0; j--) {
            if (i >= ECARTE_GOAL) { t[i][j] = 1; continue; }
            if (j >= ECARTE_GOAL) { t[i][j] = 0; continue; }
            int i2 = i + 2 > ECARTE_GOAL + 1 ? ECARTE_GOAL + 1 : i + 2, j2 = j + 2 > ECARTE_GOAL + 1 ? ECARTE_GOAL + 1 : j + 2;
            t[i][j] = 0.5f * (0.8f * t[i + 1][j] + 0.2f * t[i2][j]) + 0.5f * (0.8f * t[i][j + 1] + 0.2f * t[i][j2]);
        }
    return t[a < 0 ? 0 : a][b < 0 ? 0 : b];
}

/* seat me's chance of the game when seat w scores v this deal, from totals tot[] */
static float ecb_after(const int *tot, int me, int w, int v)
{
    int a = tot[0] + (w == 0) * v, b = tot[1] + (w == 1) * v;
    float p0 = ecb_value(a, b);
    return me == 0 ? p0 : 1 - p0;
}

/* ---- the rules as the core plays them ---------------------------------------------------------------------------- */
static int ec_power(const tb_state *st, int c, int led)
{
    int s = CARD_SUIT(c);
    return s == st->trump ? 100 + ecarte_rank(c) : s == led ? ecarte_rank(c) : 0;
}

static int ec_order(const tb_state *st, int c) { (void)st; return ecarte_rank(c); }

/* follow and win if you can; without the suit led, trump if you can */
static int ec_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int k = 0;
    if (!st->ntrick) {
        for (int i = 0; i < n; i++) out[k++] = h[i];
        return k;
    }
    int led = CARD_SUIT(st->trick[0]), r = ecarte_rank(st->trick[0]);
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == led && ecarte_rank(h[i]) > r) out[k++] = h[i];
    if (k) return k;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == led) out[k++] = h[i];
    if (k) return k;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == st->trump) out[k++] = h[i];
    if (k) return k;
    for (int i = 0; i < n; i++) out[k++] = h[i];
    return k;
}

/* the deal is decided: three tricks to one side and one to the other (no vole left) */
static int ec_over(const tb_state *st)
{
    return (st->won[0] >= 3 && st->won[1] >= 1) || (st->won[1] >= 3 && st->won[0] >= 1);
}

/* the deal's winner and points as the rules score them */
static int ec_points(const tb_state *st, int *w)
{
    const ecx *x = TB_CXS(st, ecx);
    *w = st->won[0] >= 3 ? 0 : 1;
    int v = st->won[*w] == ECARTE_HAND ? 2 : 1;
    if ((*w == x->dealer && x->forced) || (*w == x->elder && x->refused)) v = 2;
    return v;
}

static void ec_reward(const tb_state *st, float *rw)
{
    int w, v = ec_points(st, &w);
    rw[0] = TB_CXS(st, ecx)->r0[w][v];
    rw[1] = 1 - rw[0];
}

/* following under the card led: nothing higher in that suit; neither following nor trumping: no trumps */
static void ec_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    if (!n) return;
    int led = CARD_SUIT(trick[0]), s = CARD_SUIT(card), r = ecarte_rank(trick[0]);
    if (s == led) {
        if (ecarte_rank(card) < r && st->cap[seat][led] > r) st->cap[seat][led] = (uint8_t)r;
    } else if (s != st->trump && st->trump < TB_SUITS) st->cap[seat][st->trump] = 0;
}

static const tb_rules RULES = {
    .power = ec_power, .order = ec_order, .legal = ec_legal, .over = ec_over, .reward = ec_reward, .infer = ec_infer,
};

/* the stakes: the totals once the play starts (seat me's king of trumps counted), the flags */
static void ecb_x(tb_state *v, const ecarte_pos *p, int me, int forced, int refused)
{
    ecx *x = TB_XS(v, ecx);
    int tot[2] = {p->total[0], p->total[1]};
    if (p->phase < EC_PLAY && CARD_RANK(p->turned) != RANK_KING &&
        trick_find(p->hand[me], p->n[me], CARD(p->trump, RANK_KING)) >= 0)
        tot[me]++;                              /* its point, counted when the play starts */
    for (int w = 0; w < 2; w++)
        for (int k = 1; k <= 2; k++) x->r0[w][k] = ecb_after(tot, 0, w, k);
    x->elder = p->elder;
    x->dealer = p->dealer;
    x->forced = (uint8_t)forced;
    x->refused = (uint8_t)refused;
    v->trump = p->trump;
}

/* the view of seat me: its hand, the turned card, the cards it threw, the plays, the king's point */
void ecb_view(const ecarte_game *g, int me, tb_state *v, ecarte_game *t)
{
    const ecarte_pos *p = &g->pos;
    int o = 1 - me, king = CARD(p->trump, RANK_KING), king_played = 0;
    tb_init(v, &RULES, ECARTE_P, me);
    ecb_x(v, p, me, p->forced || (p->phase == EC_PROPOSE && p->asked == 0), p->refused);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    for (int s = 0; s < 2; s++) v->won[s] = p->won[s];
    tb_hidden(v, o, p->n[o]);
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (ECA_KIND(g->act[i]) == ECA_NEXT) start = i + 1;
    ecarte_new(t, g->seed);
    for (int i = 0, k = 0, plays = p->tricks * ECARTE_P; i < g->n; i++) {
        int a = g->act[i];
        const ecarte_pos *q = &t->pos;
        if (i >= start && ECA_KIND(a) == ECA_DISCARD && q->turn == me)
            for (int j = 0; j < q->n[me]; j++)  /* the cards it threw */
                if (ECA_ARG(a) >> j & 1) tb_seen(v, q->hand[me][j]);
        if (i >= start && ECA_KIND(a) == ECA_PLAY) {
            int c = ECA_ARG(a);
            uint8_t who[ECARTE_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % ECARTE_P);
            tb_note(v, q->turn, c, q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, c);
            king_played |= c == king;
        }
        ecarte_play(t, a);
    }
    tb_seen(v, p->turned);
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % ECARTE_P, p->trick[j]);
    if (p->phase == EC_PLAY && CARD_RANK(p->turned) != RANK_KING) {   /* the king's point told who holds it */
        if (p->king[o] && !king_played) tb_known(v, o, king);
        else if (!p->king[o]) tb_exclude(v, o, king);
    }
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* ---- the calls ---------------------------------------------------------------------------------------------------- */
static int ecb_samples(int level) { return level <= AI_EASY ? 24 : level == AI_MEDIUM ? 64 : 160; }

/* a deal played out by the heuristic from a dealt state: seat me's chance of the game after it */
static float ecb_playout(tb_state *w, int me, ai_rng *r, int *won)
{
    uint8_t L[TB_HAND];
    for (int guard = 0; guard < 12 && !tb_over(w); guard++) {
        int n = tb_legal(w, L);
        if (!n) break;
        tb_play(w, tb_heuristic(w, L, n, r));
    }
    float rw[2];
    ec_reward(w, rw);
    if (won) *won = w->won[me];
    return rw[me];
}

/* P: the share of simulated deals seat me takes (three tricks, five), its hand as it is, the elder leading */
static void ecb_chance(const ecarte_game *g, int me, int level, uint64_t seed, float *p3, float *p5)
{
    tb_state *v = malloc(sizeof *v);
    ecarte_game *t = malloc(sizeof *t);
    *p3 = 0.5f, *p5 = 0;
    if (v && t) {
        ecb_view(g, me, v, t);
        v->turn = v->leader = g->pos.elder;
        v->tricks = v->ntrick = 0;
        ai_rng r;
        ai_seed(&r, ai_mix(seed, 0xec1 + (uint64_t)g->n));
        int n = ecb_samples(level), w3 = 0, w5 = 0, done = 0;
        for (; done < n && !ai_cancelled(); done++) {
            tb_state w = *v;
            tb_determinise(&w, &r);
            w.eps = 8;
            int won;
            ecb_playout(&w, me, &r, &won);
            w3 += won >= 3;
            w5 += won == ECARTE_HAND;
        }
        if (done) *p3 = (float)w3 / done, *p5 = (float)w5 / done;
        if (level <= AI_EASY) *p3 += (float)(ai_rand_n(&r, 21) - 10) / 100;
    }
    free(v);
    free(t);
}

/* the value for seat me of playing the hand at P (p3, p5): a win scores 1 (the vole 2), a loss `loss` to the other */
static float ecb_stake(const int *tot, int me, float p3, float p5, int loss)
{
    float pv = p5 < p3 ? p5 : p3;
    return (p3 - pv) * ecb_after(tot, me, me, 1) + pv * ecb_after(tot, me, me, 2) +
           (1 - p3) * ecb_after(tot, me, 1 - me, loss);
}

static float ecb_exchanged(const int *tot, int me, float p3)   /* the exchange: 1/4 + P/2, 1 point either way */
{
    float q = ECB_Q0 + ECB_Q1 * p3;
    return q * ecb_after(tot, me, me, 1) + (1 - q) * ecb_after(tot, me, 1 - me, 1);
}

/* the cards to throw: every legal set tried on the same simulated draws and deals */
static int ecb_discard(const ecarte_game *g, int me, int level, uint64_t seed)
{
    const ecarte_pos *p = &g->pos;
    int lo = ecarte_throw_min(p), hi = ecarte_throw_max(p), n = p->n[me], S = level <= AI_EASY ? 12 :
        level == AI_MEDIUM ? 40 : 96;
    tb_state *v = malloc(sizeof *v);
    ecarte_game *t = malloc(sizeof *t);
    int best = lo ? 1 : 0;
    if (!v || !t) goto out;
    ecb_view(g, me, v, t);                      /* the unseen cards: v->unseen */
    ecx *x = TB_XS(v, ecx);
    x->forced = x->refused = 0;
    int tot[2] = {p->total[0], p->total[1]}, nu = v->nunseen;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, 0xd15 + (uint64_t)g->n));
    float bv = -1;
    uint8_t perm[96][32];
    for (int j = 0; j < S; j++) {
        memcpy(perm[j], v->unseen, (size_t)nu);
        for (int i = nu - 1; i > 0; i--) {
            int k = ai_rand_n(&r, i + 1);
            uint8_t c = perm[j][i]; perm[j][i] = perm[j][k]; perm[j][k] = c;
        }
    }
    for (int m = 0; m < 1 << n && !ai_cancelled(); m++) {
        int k = __builtin_popcount((unsigned)m);
        if (k < lo || k > hi || k + ECARTE_HAND > nu) continue;
        float sum = 0;
        for (int j = 0; j < S; j++) {
            tb_state w = *v;
            int c = 0;
            for (int i = 0; i < n; i++)
                if (!(m >> i & 1)) w.hand[me][c++] = p->hand[me][i];
            for (int i = 0; i < k; i++) w.hand[me][c++] = perm[j][i];
            memcpy(w.hand[1 - me], perm[j] + k, ECARTE_HAND);
            w.n[0] = w.n[1] = w.nknown[0] = w.nknown[1] = ECARTE_HAND;
            w.full = 1;
            w.turn = w.leader = p->elder;
            w.eps = 8;
            int t2[2] = {tot[0], tot[1]}, king = CARD(p->trump, RANK_KING);
            for (int s = 0; s < 2 && CARD_RANK(p->turned) != RANK_KING; s++)   /* the king's point at the play */
                t2[s] += trick_find(w.hand[s], ECARTE_HAND, king) >= 0;
            if (t2[0] >= ECARTE_GOAL || t2[1] >= ECARTE_GOAL) {   /* the game over before a card */
                sum += t2[me] >= ECARTE_GOAL;
                continue;
            }
            ai_rng rr;
            ai_seed(&rr, ai_mix(seed, (uint64_t)j));
            ecb_playout(&w, me, &rr, NULL);
            int win, pts = ec_points(&w, &win);
            sum += ecb_after(t2, me, win, pts);
        }
        float val = sum / S + (float)ai_rand_n(&r, 100) * 1e-6f;   /* (ties: at random) */
        if (val > bv) bv = val, best = m;
    }
out:
    free(v);
    free(t);
    return best;
}

/* ---- the choice --------------------------------------------------------------------------------------------------- */
/* the action for seat at a level (0: nothing to do) */
int ecb_choose(const ecarte_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const ecarte_pos *p = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase == EC_ROUND) return ECA_NEXT;
    if (p->phase == EC_OVER || p->turn != seat) return 0;
    int tot[2] = {p->total[0], p->total[1]}, a = 0;
    if (p->phase == EC_DISCARD) {
        a = ECA_DISCARD | ecb_discard(g, seat, level, seed);
        return ecarte_legal(p, a) ? a : ecarte_auto(p);
    }
    if (p->phase == EC_ANSWER || (p->phase == EC_PROPOSE && p->nstock > 0)) {
        float p3, p5;
        ecb_chance(g, seat, level, seed, &p3, &p5);
        if (CARD_RANK(p->turned) != RANK_KING && trick_find(p->hand[seat], p->n[seat], CARD(p->trump, RANK_KING)) >= 0)
            tot[seat]++;                        /* the king's point, counted at the play */
        if (p->phase == EC_ANSWER) {
            if (p->asked == 1) {
                float pd = p3 + ECB_DEALER > 1 ? 1 : p3 + ECB_DEALER;
                a = ecb_stake(tot, seat, pd, p5, 2) > ecb_exchanged(tot, seat, pd) ? ECA_NO : ECA_YES;
            } else a = p3 >= ECB_LATER ? ECA_NO : ECA_YES;
            return a;
        }
        int ask = p->asked == 0 ? ecb_stake(tot, seat, p3, p5, 2) <= ecb_exchanged(tot, seat, p3) : p3 < ECB_LATER;
        if (ask) return ECA_ASK;
    }
    tb_state *v = malloc(sizeof *v);           /* the play (the elder may play at once) */
    ecarte_game *t = malloc(sizeof *t);
    if (v && t) {
        ecb_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, ECARTE_HARD_ITERS, stats);
        if (c != TB_NONE && ecarte_can_play(p, c)) a = ECA_PLAY | c;
    }
    free(v);
    free(t);
    if (a) return a;
    if (p->phase == EC_PROPOSE)                 /* never stall: a card */
        for (int i = 0; i < p->n[seat]; i++)
            if (ecarte_can_play(p, p->hand[seat][i])) return ECA_PLAY | p->hand[seat][i];
    return ecarte_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    ecarte_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *ecarte_live();                        /* the copy, at entry: nothing else is read */
    int a = ecb_choose(g, seat, level, ai_session_seed(), NULL);
    int ok = a && !ai_cancelled() && cap >= 16;
    if (ok) {
        char c[3];
        switch (ECA_KIND(a)) {
        case ECA_ASK: snprintf(move, (size_t)cap, "q"); break;
        case ECA_YES: snprintf(move, (size_t)cap, "y"); break;
        case ECA_NO: snprintf(move, (size_t)cap, "x"); break;
        case ECA_DISCARD: {
            int k = snprintf(move, (size_t)cap, "e");
            for (int i = 0; i < g->pos.n[seat]; i++)
                if (ECA_ARG(a) >> i & 1) {
                    trick_name(g->pos.hand[seat][i], c);
                    k += snprintf(move + k, (size_t)(cap - k), "%s", c);
                }
            break;
        }
        case ECA_PLAY: trick_name(ECA_ARG(a), c), snprintf(move, (size_t)cap, "%s", c); break;
        case ECA_NEXT: snprintf(move, (size_t)cap, "n"); break;
        default: ok = 0;
        }
    }
    free(g);
    return ok;
}

static int actor(void)
{
    const ecarte_pos *p = &ecarte_live()->pos;
    return p->phase <= EC_PLAY ? p->turn : -1;
}

static const bot ecarte_bot = {.think = think, .actor = actor, .play = ecarte_show};
REGISTER_BOT(ecarte, &ecarte_bot)
