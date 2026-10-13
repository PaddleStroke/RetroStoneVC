/*
 * Klaverjas's bot (bots.h), the Rotterdam rules: the card play searched by the trick bots' core (trickbot.h:
 * information-set Monte Carlo over deals of the unseen cards), partners playing for their side, the reward the deal's
 * score as the rules count it (the cards, the roem lying in each trick, pit, and nat: the playing side under half
 * of it all scores nothing). Runs on the worker thread: it copies the game at entry and reads only what its seat may
 * know: its hand, the bidding, the cards played. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the play reads (beyond the suits not followed): void and no trump played while the trick held none: no
 * trump; void and a side card on the trick's trump: no trump above it; a trump under the trick's trump (trumps led,
 * or ruffing): none above it, and ruffing low a hand of trumps only.
 * The trump call (rule-based, checked by simulation): each suit is simulated as trumps from the seat's eight cards
 * (deals of the other 24 played out by the core's heuristic): the side's expected card points with the stuk it holds.
 * Name the best suit when that reaches 95 of 162 (the game needs more than half of all points, roem included);
 * else pass; all passed, voorhand names its best suit (verplicht). Easy reads the estimates with noise (+-12).
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("th", "p", "Jh", "N"), played by klaverjas_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/klaverjas.h"
#include "games/trickbot.h"

#define KLAVERJAS_HARD_ITERS 40000              /* about 0.15 s a card on a quiet PC (0.3 s at the first tricks) */

typedef struct { uint8_t taker; int16_t roem[2]; } kvjx;

static int k_power(const tb_state *st, int c, int led)
{
    int s = CARD_SUIT(c);
    if (s == st->trump) return 100 + kvj_power(c, st->trump);
    return s == led ? kvj_power(c, st->trump) : 0;
}

static int k_order(const tb_state *st, int c) { return kvj_power(c, st->trump); }

static int k_points(const tb_state *st, int c) { return kvj_points(c, st->trump); }

/* the trick's best trump (its power), 0: none */
static int k_high(const uint8_t *trick, int n, int T)
{
    int best = 0;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(trick[i]) == T && kvj_power(trick[i], T) > best) best = kvj_power(trick[i], T);
    return best;
}

/* as the rules' kvj_can_play */
static int k_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    if (!st->ntrick) { memcpy(out, h, (size_t)n); return n; }
    int T = st->trump, led = CARD_SUIT(st->trick[0]), best = k_high(st->trick, st->ntrick, T), over = 0, other = 0, k = 0;
    for (int i = 0; i < n; i++) {
        if (CARD_SUIT(h[i]) == T && kvj_power(h[i], T) > best) over = 1;
        if (CARD_SUIT(h[i]) != T) other = 1;
    }
    int has_trump = trick_has_suit(h, n, T), has_led = trick_has_suit(h, n, led);
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]), ok;
        if (led == T) ok = !has_trump || (s == T && (!over || kvj_power(h[i], T) > best));
        else if (has_led) ok = s == led;
        else if (!has_trump) ok = 1;
        else if (!best) ok = s == T;
        else if (over) ok = s == T && kvj_power(h[i], T) > best;
        else ok = other ? s != T : 1;
        if (ok) out[k++] = h[i];
    }
    return k;
}

static void k_trick_end(tb_state *st, int w)
{
    kvjx *x = TB_XS(st, kvjx);
    if (st->tricks == KVJ_HAND - 1) st->pts[w] = (int16_t)(st->pts[w] + 10);   /* the last trick */
    x->roem[w & 1] = (int16_t)(x->roem[w & 1] + kvj_roem(st->trick, st->ntrick, st->trump));
}

static void k_cap(tb_state *st, int seat, int suit, int cap)
{
    if (st->cap[seat][suit] == TB_NOCAP || st->cap[seat][suit] > cap) st->cap[seat][suit] = (uint8_t)cap;
}

static void k_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    if (!n) return;
    int T = st->trump, led = CARD_SUIT(trick[0]), s = CARD_SUIT(card), best = k_high(trick, n, T);
    if (led == T) {
        if (s == T && kvj_power(card, T) < best) k_cap(st, seat, T, best);
        return;
    }
    if (s == led) return;
    if (s != T) k_cap(st, seat, T, best);       /* none: no trump; else none over the trick's */
    else if (best && kvj_power(card, T) < best) {   /* under it: trumps only */
        k_cap(st, seat, T, best);
        for (int u = 0; u < 4; u++)
            if (u != T) st->cap[seat][u] = 0;
    }
}

/* the deal's score for each team, as the rules' deal_end() (the tests compare them) */
void kvjb_score(const tb_state *st, int *sc)
{
    const kvjx *x = TB_CXS(st, kvjx);
    int eyes[2] = {0, 0}, won[2] = {0, 0}, roem[2] = {x->roem[0], x->roem[1]};
    for (int s = 0; s < st->players; s++) eyes[s & 1] += st->pts[s], won[s & 1] += st->won[s];
    for (int k = 0; k < 2; k++) roem[k] += won[k] == KVJ_HAND ? 100 : 0;
    int P = x->taker & 1, O = 1 - P, all = KVJ_TOTAL + roem[0] + roem[1], mine = eyes[P] + roem[P];
    if (2 * mine > all) sc[P] = mine, sc[O] = eyes[O] + roem[O];
    else sc[P] = 0, sc[O] = all;
}

static void k_reward(const tb_state *st, float *rw)
{
    int sc[2];
    kvjb_score(st, sc);
    float r0 = 0.5f + (sc[0] - sc[1]) / 700.0f;
    for (int s = 0; s < st->players; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static const tb_rules RULES = {
    .power = k_power, .order = k_order, .legal = k_legal, .points = k_points, .trick_end = k_trick_end,
    .reward = k_reward, .infer = k_infer,
};

static void k_base(tb_state *v, int me, int trump, int taker)
{
    tb_init(v, &RULES, TK4_P, me);
    for (int s = 0; s < TK4_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->trump = (uint8_t)trump;
    TB_XS(v, kvjx)->taker = (uint8_t)taker;
}

/* the view of seat me in the play (the deal's cards replayed on t: who played what) */
void kvjb_view(const kvgame *g, int me, tb_state *v, kvgame *t)
{
    const kvpos *p = &g->pos;
    const tk4_table *tb = &p->t;
    k_base(v, me, p->trump, p->taker);
    kvjx *x = TB_XS(v, kvjx);
    x->roem[0] = p->roem[0], x->roem[1] = p->roem[1];
    v->turn = (uint8_t)tk4_turn(tb);
    v->leader = tb->leader;
    v->tricks = tb->tricks;
    for (int s = 0; s < TK4_P; s++) {
        v->won[s] = tb->won[s];
        v->pts[s] = tb->eyes[s];
        if (s != me) tb_hidden(v, s, tb->n[s]);
    }
    tb_mine(v, tb->hand[me], tb->n[me]);
    int start = 0, k = 0, plays = tb->tricks * TK4_P;
    for (int i = 0; i < g->n; i++)
        if (KA_KIND(g->act[i]) == KA_NEXT) start = i + 1;
    kvj_new(t, g->seed);
    for (int i = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && KA_KIND(a) == KA_PLAY) {
            const tk4_table *q = &t->pos.t;
            uint8_t who[TK4_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % TK4_P);
            tb_note(v, tk4_turn(q), KA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, KA_ARG(a));
        }
        kvj_play(t, a);
    }
    for (int j = 0; j < tb->ntrick; j++) tb_trick_add(v, (tb->leader + j) % TK4_P, tb->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* ---- the trump call ------------------------------------------------------------------------------------------- */
/* the side's expected card points with suit tr as trumps (and the stuk the seat holds), from its hand */
static float estimate(const kvpos *p, int me, int tr, ai_rng *r)
{
    tb_state *v = malloc(sizeof *v);
    if (!v) return 0;
    k_base(v, me, tr, me);
    v->turn = v->leader = p->t.leader;
    for (int s = 0; s < TK4_P; s++)
        if (s != me) tb_hidden(v, s, KVJ_HAND);
    tb_mine(v, p->t.hand[me], p->t.n[me]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
    float P[TB_MAXP];
    tb_simulate(v, 36, r, NULL, P, NULL);
    free(v);
    const uint8_t *h = p->t.hand[me];
    int kq = trick_find(h, p->t.n[me], CARD(tr, RANK_KING)) >= 0 && trick_find(h, p->t.n[me], CARD(tr, RANK_QUEEN)) >= 0;
    return P[me & 1] + P[(me & 1) + 2] + (kq ? 20 : 0);
}

static int call(const kvpos *p, int me, int level, uint64_t seed)
{
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 16 + (uint64_t)me + 555));
    int best = 0;
    float be = -1;
    for (int s = 0; s < 4; s++) {
        float e = estimate(p, me, s, &r) + (level <= AI_EASY ? (float)(ai_rand_f(&r) * 24 - 12) : 0);
        if (e > be) be = e, best = s;
    }
    return be >= 95 || kvj_must_name(p) ? KA_TRUMP | best : KA_PASS;
}

/* the action for seat at a level (0: nothing to do) */
int kvjb_choose(const kvgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const kvpos *p = &g->pos;
    if (p->phase == KP_DEAL) return KA_NEXT;
    if (p->phase == KP_OVER || p->turn != seat) return 0;
    int a = 0;
    if (p->phase == KP_BID) a = call(p, seat, level, seed);
    else {
        kvgame *t = malloc(sizeof *t);
        tb_state *v = malloc(sizeof *v);
        if (t && v) {
            kvjb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, KLAVERJAS_HARD_ITERS, stats);
            if (c != TB_NONE) a = KA_PLAY | c;
        }
        free(t);
        free(v);
    }
    return a && kvj_legal(p, a) ? a : kvj_auto(p);
}

/* an action in the notation */
void kvjb_name(int a, char *move, int cap)
{
    char c[3];
    switch (KA_KIND(a)) {
    case KA_PASS: snprintf(move, (size_t)cap, "p"); break;
    case KA_TRUMP: snprintf(move, (size_t)cap, "t%c", "shdc"[KA_ARG(a) & 3]); break;
    case KA_PLAY: trick_name(KA_ARG(a), c); snprintf(move, (size_t)cap, "%s", c); break;
    case KA_NEXT: snprintf(move, (size_t)cap, "N"); break;
    default: if (cap) move[0] = 0; break;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    kvgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *klaverjas_live();                     /* the copy, at entry: nothing else is read */
    int a = kvjb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    kvjb_name(a, move, cap);
    return move[0] != 0;
}

static int actor(void)
{
    const kvpos *p = &klaverjas_live()->pos;
    return p->phase <= KP_PLAY ? p->turn : -1;
}

static const bot klaverjas_bot = {.think = think, .actor = actor, .play = klaverjas_show};
REGISTER_BOT(klaverjas, &klaverjas_bot)
