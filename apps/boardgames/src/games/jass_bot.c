/*
 * Jass's bot (bots.h), the Schieber, both variants (to 1000, and to 2500 with the multipliers): the card play searched
 * by the trick bots' core (trickbot.h: information-set Monte Carlo over deals of the unseen cards), partners playing
 * for their side, the reward the deal's points for the side (the cards, the last trick's five, the match's hundred,
 * multiplied). Runs on the worker thread: it copies the game at entry and reads only what its seat may know: its
 * hand, the game chosen, the cards played. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the play reads: a suit not followed shows a void, except when a trump was played instead (allowed while
 * holding the suit led); trumps led and a side card played shows no trump but perhaps the Puur (the other trumps are
 * ruled out); a trump played under the trick's trump on a side suit shows a hand of trumps only.
 * The game's choice (rule-based, checked by simulation): each of the six games (four trump suits, obenabe, undeufe)
 * is simulated from the seat's nine cards (deals of the other 27 played out by the core's heuristic): the team's
 * expected points, the match counted. The best game is the one with the best expected margin (points over the other
 * side, times the game's multiplier in the 2500 variant). The first to choose pushes (schieben) when even the best
 * game leaves the side under 96 points of 157; the partner then takes the best game. Easy reads the estimates with
 * noise (+-15 points). The weis: always declared with the first card (declaring costs nothing; only the best team's
 * counts). Stöck counts by itself.
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("th", "o", "u", "p", "w", "Jh", "N"), played by jass_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/jass.h"
#include "games/trickbot.h"

#define JASS_HARD_ITERS 32000                   /* about 0.15 s a card on a quiet PC (0.3 s at the first tricks) */

typedef struct { uint8_t mode, variant, voids[TK4_P]; } jassx;

static int jx_mode(const tb_state *st) { return TB_CXS(st, jassx)->mode; }

static int j_power(const tb_state *st, int c, int led)
{
    int m = jx_mode(st), s = CARD_SUIT(c);
    if (m < 4 && s == m) return 100 + jas_power(c, m);
    return s == led ? jas_power(c, m) : 0;
}

static int j_order(const tb_state *st, int c) { return jas_power(c, jx_mode(st)); }

static int j_points(const tb_state *st, int c) { return jas_points(c, jx_mode(st)); }

/* as the rules' jas_can_play */
static int j_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    if (!st->ntrick) { memcpy(out, h, (size_t)n); return n; }
    int m = jx_mode(st), led = CARD_SUIT(st->trick[0]), has_led = trick_has_suit(h, n, led), k = 0;
    if (m >= 4) return tb_legal_follow(st, h, n, out);
    if (led == m) {
        int others = 0;
        for (int i = 0; i < n; i++) others += CARD_SUIT(h[i]) == m && CARD_RANK(h[i]) != RANK_JACK;
        if (!others) { memcpy(out, h, (size_t)n); return n; }
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == m) out[k++] = h[i];
        return k;
    }
    int best = 0, all = 1;
    for (int i = 0; i < st->ntrick; i++)
        if (CARD_SUIT(st->trick[i]) == m && jas_power(st->trick[i], m) > best) best = jas_power(st->trick[i], m);
    for (int i = 0; i < n; i++) all &= CARD_SUIT(h[i]) == m;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]);
        if (s == m ? !best || jas_power(h[i], m) > best || all : !has_led || s == led) out[k++] = h[i];
    }
    return k;
}

static void j_trick_end(tb_state *st, int w)
{
    if (st->tricks == JAS_HAND - 1) st->pts[w] = (int16_t)(st->pts[w] + 5);   /* the last trick */
}

/* a card is out of the unknown hands: the observer's, seen gone, or on the table */
static int j_out(const tb_state *st, int c)
{
    if (trick_find(st->hand[st->me], st->nknown[st->me], c) >= 0) return 1;
    for (int i = 0; i < st->ngone; i++)
        if (st->gone[i] == c) return 1;
    for (int i = 0; i < st->ntrick; i++)
        if (st->trick[i] == c) return 1;
    return 0;
}

static void j_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    if (!n) return;
    jassx *x = TB_XS(st, jassx);
    int m = x->mode, led = CARD_SUIT(trick[0]), s = CARD_SUIT(card);
    if (s == led) return;
    if (m < 4 && s == m) {                      /* trumped: the suit led may still be held */
        st->cap[seat][led] = x->voids[seat] >> led & 1 ? 0 : TB_NOCAP;
        int best = 0;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(trick[i]) == m && jas_power(trick[i], m) > best) best = jas_power(trick[i], m);
        if (best && jas_power(card, m) < best)  /* under the trick's trump: trumps only */
            for (int u = 0; u < 4; u++)
                if (u != m) st->cap[seat][u] = 0, x->voids[seat] |= (uint8_t)(1 << u);
        return;
    }
    if (m < 4 && led == m && !(x->voids[seat] >> m & 1) && !j_out(st, CARD(m, RANK_JACK))) {
        st->cap[seat][m] = TB_NOCAP;            /* no trump but perhaps the Puur */
        for (int r = 1; r <= 13; r++)
            if (r != RANK_JACK) tb_exclude(st, seat, CARD(m, r));
        return;
    }
    x->voids[seat] |= (uint8_t)(1 << led);      /* a true void */
}

static void j_reward(const tb_state *st, float *rw)
{
    int e[2] = {0, 0}, won[2] = {0, 0};
    for (int s = 0; s < st->players; s++) e[s & 1] += st->pts[s], won[s & 1] += st->won[s];
    for (int k = 0; k < 2; k++) e[k] += won[k] == JAS_HAND ? 100 : 0;
    float r0 = 0.5f + (e[0] - e[1]) / 514.0f;
    for (int s = 0; s < st->players; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static const tb_rules RULES = {
    .power = j_power, .order = j_order, .legal = j_legal, .points = j_points, .trick_end = j_trick_end,
    .reward = j_reward, .infer = j_infer,
};

static void j_base(tb_state *v, int me, int mode, int variant)
{
    tb_init(v, &RULES, TK4_P, me);
    for (int s = 0; s < TK4_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->trump = mode < 4 ? (uint8_t)mode : TB_NONE;
    jassx *x = TB_XS(v, jassx);
    x->mode = (uint8_t)mode;
    x->variant = (uint8_t)variant;
}

/* the 36 cards: 6 to ace */
static int j_pack(uint8_t *out)
{
    uint8_t all[CARDS_MAX];
    int n = cards_pack(PACK_52, all), k = 0;
    for (int i = 0; i < n; i++)
        if (CARD_RANK(all[i]) == RANK_ACE || CARD_RANK(all[i]) >= 6) out[k++] = all[i];
    return k;
}

/* the view of seat me in the play (the deal's cards replayed on t: who played what) */
void jasb_view(const jsgame *g, int me, tb_state *v, jsgame *t)
{
    const jspos *p = &g->pos;
    const tk4_table *tb = &p->t;
    j_base(v, me, p->mode, p->variant);
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
        if (JA_KIND(g->act[i]) == JA_NEXT) start = i + 1;
    jas_new(t, g->variant, g->seed);
    for (int i = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && JA_KIND(a) == JA_PLAY) {
            const tk4_table *q = &t->pos.t;
            uint8_t who[TK4_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % TK4_P);
            tb_note(v, tk4_turn(q), JA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, JA_ARG(a));
        }
        jas_play(t, a);
    }
    for (int j = 0; j < tb->ntrick; j++) tb_trick_add(v, (tb->leader + j) % TK4_P, tb->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, j_pack(pack));
}

/* ---- the choice ---------------------------------------------------------------------------------------------- */
/* the team's expected points (the match's hundred in) with this game, from the seat's hand */
static float estimate(const jspos *p, int me, int mode, ai_rng *r)
{
    tb_state *v = malloc(sizeof *v);
    if (!v) return 0;
    j_base(v, me, mode, p->variant);
    v->turn = v->leader = p->t.leader;
    for (int s = 0; s < TK4_P; s++)
        if (s != me) tb_hidden(v, s, JAS_HAND);
    tb_mine(v, p->t.hand[me], p->t.n[me]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, j_pack(pack));
    float rw[TB_MAXP];
    tb_simulate(v, 36, r, NULL, NULL, rw);      /* the reward: 0.5 + margin / 514, the match in */
    free(v);
    float margin = (rw[me] - 0.5f) * 514.0f;
    return (JAS_TOTAL + margin) / 2;
}

static int choose_game(const jspos *p, int me, int level, uint64_t seed)
{
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 16 + (uint64_t)me + 777));
    int best = 0;
    float bv = -1e9f, be = 0;
    for (int m = 0; m < JM_MODES; m++) {
        float e = estimate(p, me, m, &r);
        if (level <= AI_EASY) e += (float)(ai_rand_f(&r) * 30 - 15);
        float val = (2 * e - JAS_TOTAL) * (float)jas_mult(p->variant, m);
        if (val > bv) bv = val, best = m, be = e;
    }
    if (!p->pushed && be < 96) return JA_PUSH;
    return JA_MODE | best;
}

/* the action for seat at a level (0: nothing to do) */
int jasb_choose(const jsgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const jspos *p = &g->pos;
    if (p->phase == JP_DEAL) return JA_NEXT;
    if (p->phase == JP_OVER || p->turn != seat) return 0;
    int a = 0;
    if (p->phase == JP_CHOOSE) a = choose_game(p, seat, level, seed);
    else if (jas_can_weis(p)) a = JA_WEIS;
    else {
        jsgame *t = malloc(sizeof *t);
        tb_state *v = malloc(sizeof *v);
        if (t && v) {
            jasb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, JASS_HARD_ITERS, stats);
            if (c != TB_NONE) a = JA_PLAY | c;
        }
        free(t);
        free(v);
    }
    return a && jas_legal(p, a) ? a : jas_auto(p);
}

/* an action in the notation */
void jasb_name(int a, char *move, int cap)
{
    char c[3];
    int arg = JA_ARG(a);
    switch (JA_KIND(a)) {
    case JA_MODE:
        if (arg == JM_OBEN) snprintf(move, (size_t)cap, "o");
        else if (arg == JM_UNDE) snprintf(move, (size_t)cap, "u");
        else snprintf(move, (size_t)cap, "t%c", "shdc"[arg & 3]);
        break;
    case JA_PUSH: snprintf(move, (size_t)cap, "p"); break;
    case JA_WEIS: snprintf(move, (size_t)cap, "w"); break;
    case JA_PLAY: trick_name(arg, c); snprintf(move, (size_t)cap, "%s", c); break;
    case JA_NEXT: snprintf(move, (size_t)cap, "N"); break;
    default: if (cap) move[0] = 0; break;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    jsgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *jass_live();                          /* the copy, at entry: nothing else is read */
    int a = jasb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    jasb_name(a, move, cap);
    return move[0] != 0;
}

static int actor(void)
{
    const jspos *p = &jass_live()->pos;
    return p->phase <= JP_PLAY ? p->turn : -1;
}

static const bot jass_bot = {.think = think, .actor = actor, .play = jass_show};
REGISTER_BOT(jass, &jass_bot)
