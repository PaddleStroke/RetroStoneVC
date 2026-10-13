/*
 * Euchre's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the voids shown; the left bower follows trumps; a lone maker's partner sits
 * out), partners playing for their side, the reward the hand's points for the side. Runs on the worker thread: it
 * copies the game at entry and reads only what its seat may know (its hand, the turned card and where it went, its
 * own card laid away, the cards played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Making trumps (rule-based, from the hand's worth in a trump suit: the right bower 3, the left 2.5, the ace of trumps
 * 2, king 1.5, queen 1.2, ten and nine 1; a side ace 1, a side king with company 0.4; each side suit void 0.5 with
 * two trumps or more):
 * - round one: order it up at 7 (the dealer counts the card in and his worst card out; the dealer's partner adds 0.7,
 *   1.5 for a bower; an opponent of the dealer takes as much off); alone at 11;
 * - round two: the best other suit at 7 (the dealer, stuck, names his best suit whatever it is worth); alone at 11;
 * - the dealer lays away his weakest side card, a singleton first (a void to trump with).
 * The others count the turned card the dealer took in his hand until it is played (he may have laid it away: the
 * view cannot know, and errs that way).
 * The hand's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("o", "oa", "p", "nh", "dTs", "Jh", "N"), played by euchre_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/euchre.h"
#include "games/trickbot.h"

#define EUCHRE_HARD_ITERS 20000                 /* about 0.3 s a card on a PC */

typedef struct { uint8_t maker, alone; } eux;

static int e_suit(const tb_state *st, int c) { return euchre_suit(c, st->trump); }

static int e_power(const tb_state *st, int c, int led)
{
    int s = euchre_suit(c, st->trump);
    return s == st->trump || s == led ? euchre_power(c, st->trump) : 0;
}

static void e_reward(const tb_state *st, float *rw)
{
    const eux *x = TB_CXS(st, eux);
    int m = x->maker & 1, t = 0, pts[2] = {0, 0};
    for (int s = 0; s < EU_P; s++)
        if ((s & 1) == m) t += st->won[s];
    if (t >= 3) pts[m] = t == 5 ? (x->alone ? 4 : 2) : 1;
    else pts[!m] = 2;
    float r0 = 0.5f + (pts[0] - pts[1]) / 8.0f;
    for (int s = 0; s < EU_P; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static const tb_rules RULES = {.suit = e_suit, .power = e_power, .reward = e_reward};

static void euchre_pack(uint8_t *deck, int *n)
{
    uint8_t pack[CARDS_MAX];
    int k = cards_pack(PACK_32, pack), m = 0;
    for (int j = 0; j < k; j++)
        if (CARD_RANK(pack[j]) == RANK_ACE || CARD_RANK(pack[j]) >= 9) deck[m++] = pack[j];
    *n = m;
}

static int played_this_hand(const tb_state *v, int c)
{
    for (int j = 0; j < v->ngone; j++)
        if (v->gone[j] == c) return 1;
    for (int j = 0; j < v->ntrick; j++)
        if (v->trick[j] == c) return 1;
    return 0;
}

/* the view of seat me (the play) */
void eub_view(const egame *g, int me, tb_state *v, egame *t)
{
    const epos *p = &g->pos;
    tb_init(v, &RULES, EU_P, me);
    v->trump = p->trump;
    v->sitting = p->out;
    for (int s = 0; s < EU_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    eux *x = TB_XS(v, eux);
    x->maker = p->maker;
    x->alone = p->alone;
    for (int s = 0; s < EU_P; s++) {
        v->won[s] = p->took[s];
        if (s != me) tb_hidden(v, s, s == p->out ? 0 : p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (EA_KIND(g->act[i]) == EA_NEXT) start = i + 1;
    euchre_new(t, g->seed);
    int plays = p->tricks * (p->out == EU_NONE ? EU_P : EU_P - 1);
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && EA_KIND(a) == EA_PLAY) {
            const epos *q = &t->pos;
            tb_note(v, q->turn, EA_ARG(a), q->trick, q->who, q->ntrick);
            if (k++ < plays) tb_seen(v, EA_ARG(a));
        }
        euchre_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, p->who[j], p->trick[j]);
    /* the turned card: in the dealer's hand if he took it (until played), else out of play, face up */
    int took = !p->turned_down && p->out != p->dealer && p->maker != EU_NONE;
    if (took && p->dealer != me && !played_this_hand(v, p->up)) tb_known(v, p->dealer, p->up);
    if (!took) tb_seen(v, p->up);
    if (p->dealer == me && p->laid != EU_NONE) tb_seen(v, p->laid);   /* my own card laid away */
    uint8_t deck[24];
    int n;
    euchre_pack(deck, &n);
    tb_finish(v, deck, n);
}

/* ---- making trumps ----------------------------------------------------------------------------------------------- */
static float worth(const uint8_t *h, int n, int trump)
{
    float w = 0;
    int len[4] = {0}, trumps = 0;
    for (int i = 0; i < n; i++) {
        int s = euchre_suit(h[i], trump);
        len[s]++;
        trumps += s == trump;
    }
    for (int i = 0; i < n; i++) {
        int c = h[i], s = euchre_suit(c, trump), pw = euchre_power(c, trump), r = trick_rank(c);
        if (s == trump) w += pw == 40 ? 3 : pw == 39 ? 2.5f : r == 14 ? 2 : r == 13 ? 1.5f : r == 12 ? 1.2f : 1;
        else if (r == 14) w += 1;
        else if (r == 13 && len[s] >= 2) w += 0.4f;
    }
    if (trumps >= 2)
        for (int s = 0; s < 4; s++)
            if (s != trump && !len[s]) w += 0.5f;
    return w;
}

/* the hand with the card c in and its worst card out (the dealer taking the turned card) */
static int with_card(const uint8_t *h, int n, int c, int trump, uint8_t *out)
{
    memcpy(out, h, (size_t)n);
    out[n] = (uint8_t)c;
    int worst = 0;
    for (int i = 1; i <= n; i++) {
        int a = euchre_suit(out[i], trump) == trump ? 100 + euchre_power(out[i], trump) : euchre_power(out[i], trump);
        int b = euchre_suit(out[worst], trump) == trump ? 100 + euchre_power(out[worst], trump)
                                                        : euchre_power(out[worst], trump);
        if (a < b) worst = i;
    }
    out[worst] = out[n];
    return n;
}

static int discard(const epos *p)
{
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn], best = -1, bs = 1 << 20, len[4] = {0};
    for (int i = 0; i < n; i++) len[euchre_suit(h[i], p->trump)]++;
    for (int i = 0; i < n; i++) {
        int c = h[i], s = euchre_suit(c, p->trump);
        int score = s == p->trump ? 1000 + euchre_power(c, p->trump) : euchre_power(c, p->trump) * 4
                    - (len[s] == 1 && trick_rank(c) != 14 ? 30 : 0);
        if (score < bs) bs = score, best = i;
    }
    return EA_DISCARD | h[best];
}

static int call(const epos *p)
{
    int me = p->turn, up = CARD_SUIT(p->up);
    const uint8_t *h = p->hand[me];
    int n = p->n[me];
    if (p->phase == EP_ROUND1) {
        float w;
        if (me == p->dealer) {
            uint8_t hh[EU_HAND];
            with_card(h, n, p->up, up, hh);
            w = worth(hh, n, up);
        } else {
            float bonus = euchre_power(p->up, up) >= 39 ? 1.5f : 0.7f;
            w = worth(h, n, up) + ((p->dealer & 1) == (me & 1) ? bonus : -bonus);
        }
        if (w >= 7) return EA_ORDER | (w >= 11 ? EA_ALONE : 0);
        return EA_PASS;
    }
    int best = -1;
    float bw = -1;
    for (int t = 0; t < 4; t++) {
        if (t == up) continue;
        float w = worth(h, n, t);
        if (w > bw) bw = w, best = t;
    }
    if (bw >= 7 || me == p->dealer) return EA_NAME | best | (bw >= 11 ? EA_ALONE : 0);
    return EA_PASS;
}

/* the action for seat at a level (0: nothing to do) */
int eub_choose(const egame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const epos *p = &g->pos;
    if (p->phase == EP_HAND) return EA_NEXT;
    if (p->phase == EP_OVER || p->turn != seat) return 0;
    if (p->phase == EP_ROUND1 || p->phase == EP_ROUND2) return call(p);
    if (p->phase == EP_DISCARD) return discard(p);
    egame *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        eub_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, EUCHRE_HARD_ITERS, stats);
        if (c != TB_NONE && euchre_can_play(p, c)) a = EA_PLAY | c;
    }
    free(t);
    free(v);
    return a ? a : euchre_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    egame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *euchre_live();                        /* the copy, at entry: nothing else is read */
    int a = eub_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    static const char suits[] = "shdc";
    char c[3];
    int x = EA_ARG(a);
    switch (EA_KIND(a)) {
    case EA_ORDER: snprintf(move, (size_t)cap, "o%s", x & EA_ALONE ? "a" : ""); return 1;
    case EA_PASS: snprintf(move, (size_t)cap, "p"); return 1;
    case EA_NAME: snprintf(move, (size_t)cap, "n%c%s", suits[x & 3], x & EA_ALONE ? "a" : ""); return 1;
    case EA_DISCARD: trick_name(x, c); snprintf(move, (size_t)cap, "d%s", c); return 1;
    case EA_PLAY: trick_name(x, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case EA_NEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const epos *p = &euchre_live()->pos;
    return p->phase < EP_HAND ? p->turn : -1;
}

static const bot euchre_bot = {.think = think, .actor = actor, .play = euchre_show};
REGISTER_BOT(euchre, &euchre_bot)
