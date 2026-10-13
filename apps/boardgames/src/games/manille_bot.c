/*
 * Manille's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to what each play showed: a suit not followed, a trump not played when it had
 * to be, an opponent's card not beaten, so nothing over it), partners playing for their side, the reward the deal's
 * points and tricks for the side against the other's. Runs on the worker thread: it copies the game at entry and reads
 * only what its seat may know (its hand, the dealer's turned card, the doubles, the cards played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Doubling (rule-based on simulated deals): from its own hand and the turned card, the points and tricks its side can
 * expect (deals of the unseen cards played out by the core's heuristic): double at 42 or more (of 68; half is 34),
 * redouble at 46 or more, else pass.
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("p", "d", "r", "Ts", "N"), played by manille_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/manille.h"
#include "games/trickbot.h"

#define MANILLE_HARD_ITERS 28000                /* about 0.3 s a card on a PC */

static int m_power(const tb_state *st, int c, int led)
{
    int s = CARD_SUIT(c);
    if (s == st->trump) return 100 + man_rank(c);
    return s == led ? man_rank(c) : 0;
}

static int m_order(const tb_state *st, int c) { (void)st; return man_rank(c); }

static int m_points(const tb_state *st, int c) { (void)st; return man_points(c); }

/* the best card of a trick so far (index) */
static int best_of(const tb_state *st, const uint8_t *trick, int n)
{
    int b = 0, led = CARD_SUIT(trick[0]);
    for (int i = 1; i < n; i++)
        if (m_power(st, trick[i], led) > m_power(st, trick[b], led)) b = i;
    return b;
}

/* follow; an opponent holding the trick: beat his card of the suit led if you can; void, trump, and over-trump his
 * trump if you can; the partner holding it: anything (of the suit led if you have it) */
static int m_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int k = 0;
    if (!st->ntrick) {
        memcpy(out, h, (size_t)n);
        return n;
    }
    int led = CARD_SUIT(st->trick[0]), b = best_of(st, st->trick, st->ntrick), bc = st->trick[b];
    int partner = st->team[st->who[b]] == st->team[st->turn], has_led = 0, has_trump = 0, over = 0;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]);
        has_led |= s == led;
        has_trump |= s == st->trump;
    }
    if (has_led) {
        int need = !partner && CARD_SUIT(bc) == led;
        for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == led && man_rank(h[i]) > man_rank(bc);
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == led && (!need || !over || man_rank(h[i]) > man_rank(bc))) out[k++] = h[i];
        return k;
    }
    if (partner || !has_trump) {
        memcpy(out, h, (size_t)n);
        return n;
    }
    if (CARD_SUIT(bc) != st->trump) {
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == st->trump) out[k++] = h[i];
        return k;
    }
    for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == st->trump && man_rank(h[i]) > man_rank(bc);
    if (!over) {
        memcpy(out, h, (size_t)n);
        return n;
    }
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == st->trump && man_rank(h[i]) > man_rank(bc)) out[k++] = h[i];
    return k;
}

/* what a play shows (the void itself is the core's) */
static void m_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    if (!n) return;
    int led = CARD_SUIT(trick[0]), b = best_of(st, trick, n), bc = trick[b], s = CARD_SUIT(card);
    if (st->team[who[b]] == st->team[seat]) return;
    uint8_t *cap = st->cap[seat];
#define M_CAP(u, r) do { if (cap[(u)] == TB_NOCAP || cap[(u)] > (r)) cap[(u)] = (uint8_t)(r); } while (0)
    if (s == led) {                             /* it did not beat the opponent's card: it holds none over it */
        if (CARD_SUIT(bc) == led && man_rank(card) < man_rank(bc)) M_CAP(led, man_rank(bc));
        return;
    }
    if (CARD_SUIT(bc) != st->trump) {           /* void and no trump played: no trumps */
        if (s != st->trump) M_CAP(st->trump, 0);
    } else if (s != st->trump || man_rank(card) < man_rank(bc))
        M_CAP(st->trump, man_rank(bc));         /* no over-trump: none over the opponent's trump */
#undef M_CAP
}

static void m_reward(const tb_state *st, float *rw)
{
    int made[2] = {0, 0};
    for (int s = 0; s < MAN_P; s++) made[s & 1] += st->pts[s] + st->won[s];
    float r0 = 0.5f + (made[0] - made[1]) / 136.0f;
    for (int s = 0; s < MAN_P; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static const tb_rules RULES = {
    .power = m_power, .order = m_order, .legal = m_legal, .points = m_points, .infer = m_infer, .reward = m_reward,
};

/* the view of seat me (speaking: the deal's start, the first player to lead) */
void manb_view(const mgame *g, int me, tb_state *v, mgame *t)
{
    const mpos *p = &g->pos;
    tb_init(v, &RULES, MAN_P, me);
    v->trump = p->trump;
    for (int s = 0; s < MAN_P; s++) v->team[s] = (uint8_t)man_team(s);
    v->turn = p->phase == MP_PLAY ? p->turn : p->leader;
    v->leader = p->leader;
    v->tricks = p->tricks;
    for (int s = 0; s < MAN_P; s++)
        if (s != me) tb_hidden(v, s, p->n[s]);
    tb_mine(v, p->hand[me], p->n[me]);
    v->pts[0] = p->pts[0], v->pts[1] = p->pts[1];   /* each side's so far, on its first seat */
    v->won[0] = p->won[0], v->won[1] = p->won[1];
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (MA_KIND(g->act[i]) == MA_NEXT) start = i + 1;
    man_new(t, g->seed);
    int plays = p->tricks * MAN_P, turned_out = 0;
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && MA_KIND(a) == MA_PLAY) {
            const mpos *q = &t->pos;
            uint8_t who[MAN_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % MAN_P);
            tb_note(v, q->turn, MA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, MA_ARG(a));
            turned_out |= MA_ARG(a) == p->turned;
        }
        man_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % MAN_P, p->trick[j]);
    if (p->dealer != me && !turned_out) tb_known(v, p->dealer, p->turned);   /* the turned card: the dealer's */
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* doubling: the points and tricks the side can expect */
static int speak(const mgame *g, int me, int level, uint64_t seed, mgame *t)
{
    const mpos *p = &g->pos;
    int can_double = man_legal(p, MA_DOUBLE), can_re = man_legal(p, MA_REDOUBLE);
    if (!can_double && !can_re) return MA_PASS;
    tb_state *v = malloc(sizeof *v);
    if (!v) return MA_PASS;
    manb_view(g, me, v, t);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 8 + me));
    float pts[TB_MAXP], won[TB_MAXP];
    tb_simulate(v, level <= AI_EASY ? 16 : level == AI_MEDIUM ? 32 : 64, &r, won, pts, NULL);
    free(v);
    float mine = 0;
    for (int s = 0; s < MAN_P; s++)
        if (man_team(s) == man_team(me)) mine += pts[s] + won[s];
    if (can_re && mine >= 46) return MA_REDOUBLE;
    if (can_double && mine >= 42) return MA_DOUBLE;
    return MA_PASS;
}

/* the action for seat at a level (0: nothing to do) */
int manb_choose(const mgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const mpos *p = &g->pos;
    if (p->phase == MP_DEAL) return MA_NEXT;
    if (p->phase == MP_OVER || p->turn != seat) return 0;
    mgame *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        if (p->phase == MP_SPEAK) a = speak(g, seat, level, seed, t);
        else {
            manb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, MANILLE_HARD_ITERS, stats);
            if (c != TB_NONE && man_can_play(p, c)) a = MA_PLAY | c;
        }
    }
    free(t);
    free(v);
    return a && man_legal(p, a) ? a : man_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    mgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *manille_live();                       /* the copy, at entry: nothing else is read */
    int a = manb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[3];
    switch (MA_KIND(a)) {
    case MA_PASS: snprintf(move, (size_t)cap, "p"); return 1;
    case MA_DOUBLE: snprintf(move, (size_t)cap, "d"); return 1;
    case MA_REDOUBLE: snprintf(move, (size_t)cap, "r"); return 1;
    case MA_PLAY: trick_name(MA_ARG(a), c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case MA_NEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const mpos *p = &manille_live()->pos;
    return p->phase == MP_SPEAK || p->phase == MP_PLAY ? p->turn : -1;
}

static const bot manille_bot = {.think = think, .actor = actor, .play = manille_show};
REGISTER_BOT(manille, &manille_bot)
