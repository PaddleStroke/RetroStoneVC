/*
 * Schafkopf's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo
 * over deals of the unseen cards that keep to what each play showed: a suit not followed, the called ace not played
 * when the called suit was sought), the declaring side and the others each playing for their side (a Ramsch: each for
 * himself), the reward the deal's score as the rules count it (the tariff, schneider, schwarz, the Laufende; the
 * Ramsch's payments). Runs on the worker thread: it copies the game at entry and reads only what its seat may know:
 * its hand, the bidding's words (pass or play), the game declared, the cards played. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The hidden partner (a Rufspiel): the called ace's holder. Each dealt world makes him so (dealt()); the declarer
 * never holds it; a player who follows the called suit the first time it is led without the ace does not hold it,
 * nor does one who leads that suit first without the ace (unless he could hold four of it: running away). Once the
 * ace is played the side is known. The Laufende are counted from the cards' holders: the cards played by whom they
 * were, the rest as each world deals them.
 *
 * The bidding (rule-based from the hand, each game checked by deals played out by the core's heuristic):
 *   T = the hand's trumps in the game looked at, O its Obers, U its Unters, A its aces of plain suits.
 * - Solo in the suit of most trumps: T >= 7, or T >= 6 with O >= 2, or T >= 5 with O >= 3 and A >= 2.
 * - Wenz: U >= 2 and U + A + the tens beside an ace >= 5 (at least 3 Unters, or the club Unter).
 * - Rufspiel: T >= 4 with O + U >= 2, or T >= 3 with O >= 2 and A >= 1, holding a suit to call: the callable suit of
 *   fewest cards (a blank one best), then the one holding fewest points in it.
 * - The highest game that passes its check: played-out deals won at least 50 in 100 (a Solo, a Wenz) or 45 in 100
 *   (a Rufspiel; the heuristic's play-outs are pessimistic: the bots' declarers win about 7 games in 10). A
 *   Rufspiel is not said after another player said he would play (it could not outbid him). Otherwise pass (a Ramsch
 *   follows if everyone passes, in that variant).
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("p", "rc", "w", "sh", "Qc", "N"), played by schafkopf_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/schafkopf.h"
#include "games/trickbot.h"

#define SF_HARD_ITERS 36000                     /* about 0.2 s a card on a quiet PC (0.3 s at the first tricks) */
#define SF_RUN 14                               /* the trump run, Obers to the trump suit's 7 */

typedef struct {
    uint8_t kind, suit;                         /* the game (SFG_RUF, SFG_WENZ, SFG_SOLO, SFG_RAMSCH) and its suit */
    uint8_t declarer;                           /* SFK_NONE: a Ramsch */
    uint8_t partner;                            /* the called ace's holder, when known (SFK_NONE) */
    uint8_t searched;                           /* the trick the called suit was first led in + 1 (0: not yet) */
    uint8_t nrun, holder[SF_RUN];               /* the trump run: who held each card (SFK_NONE: as dealt()) */
} sfx;

#define SX(st) TB_XS(st, sfx)
#define SCX(st) TB_CXS(st, sfx)

/* sfk_power() inline (the search calls it most): trumps 10 and over */
static inline int sf_pow(int c, int kind, int suit)
{
    static const int8_t PLAIN[14] = {0, 8, 0, 0, 0, 0, 0, 1, 2, 3, 7, 4, 5, 6};   /* A 10 K (O) (U) 9 8 7 */
    static const int8_t HIGH[4] = {2, 1, 0, 3};
    int r = CARD_RANK(c) & 15, s = CARD_SUIT(c) & 3;
    r = r > 13 ? 0 : r;
    if (r == RANK_JACK) return 20 + HIGH[s];
    if (kind == SFG_WENZ) return PLAIN[r];
    if (r == RANK_QUEEN) return 30 + HIGH[s];
    return (s == (kind == SFG_SOLO ? suit : SUIT_HEARTS) ? 10 : 0) + PLAIN[r];
}

/* the tests: the same as the rules */
int sfb_power(int c, int kind, int suit) { return sf_pow(c, kind, suit); }

static int s_suit(const tb_state *st, int c)
{
    return sf_pow(c, SCX(st)->kind, SCX(st)->suit) >= 10 ? SUIT_TRUMP : CARD_SUIT(c);
}

static int s_power(const tb_state *st, int c, int led)
{
    int p = sf_pow(c, SCX(st)->kind, SCX(st)->suit);
    return p >= 10 || CARD_SUIT(c) == led ? p : 0;
}

static int s_points(const tb_state *st, int c) { (void)st; return sfk_points(c); }

static int called_plain(const sfx *x, int c)   /* a card of the called suit (not a trump) */
{
    return x->kind == SFG_RUF && CARD_SUIT(c) == x->suit && sf_pow(c, SFG_RUF, 0) < 10;
}

/* the called ace's rules (sfk_can_play) */
static int s_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    const sfx *x = SCX(st);
    int k = tb_legal_follow(st, h, n, out);
    if (x->kind != SFG_RUF) return k;
    int ace = CARD(x->suit, RANK_ACE), holds = 0, plain = 0;
    for (int i = 0; i < n; i++) holds |= h[i] == ace, plain += called_plain(x, h[i]);
    if (!holds) return k;
    int m = 0;
    if (!st->ntrick) {                          /* leading the called suit: the ace, unless running away */
        for (int i = 0; i < k; i++)
            if (!(called_plain(x, out[i]) && out[i] != ace && !x->searched && plain < 4)) out[m++] = out[i];
        return m;
    }
    int sought = called_plain(x, st->trick[0]) && (!x->searched || x->searched == st->tricks + 1);
    if (sought) {                               /* the called suit sought: the ace */
        out[0] = (uint8_t)ace;
        return 1;
    }
    if (tb_led(st) != x->suit || !called_plain(x, st->trick[0]))
        if (!x->searched && n > 1 && tb_suit(st, out[0]) != tb_led(st))   /* void: the ace never thrown away */
            for (int i = 0; i < k; i++)
                if (out[i] != ace) out[m++] = out[i];
    return m ? m : k;
}

static void s_trick_end(tb_state *st, int w)
{
    (void)w;
    sfx *x = SX(st);
    if (x->kind == SFG_RUF && !x->searched && called_plain(x, st->trick[0])) x->searched = (uint8_t)(st->tricks + 1);
}

/* the Ramsch: avoid tricks, unless someone has taken them all so far (his march, or stopping it) */
static int s_want(const tb_state *st, int seat)
{
    (void)seat;
    if (SCX(st)->kind != SFG_RAMSCH) return 1;
    if (st->tricks >= 4)
        for (int s = 0; s < TK4_P; s++)
            if (st->won[s] == st->tricks) return 1;
    return -1;
}

/* the side's Laufende (as sfk_runners: the run held by one side from the top; its length) */
static int runners(const tb_state *st)
{
    const sfx *x = SCX(st);
    int with = -1, k = 0;
    for (; k < x->nrun; k++) {
        int h = x->holder[k], ours = h < TK4_P && st->team[h] == 0;
        if (with < 0) with = ours;
        if (ours != with) break;
    }
    return k;
}

/* each seat's score as the rules count it */
static void s_scores(const tb_state *st, int *score)
{
    const sfx *x = SCX(st);
    if (x->kind == SFG_RAMSCH) {
        int most = -1, tricks = -1, losers = 0, nl = 0;
        for (int q = 0; q < TK4_P; q++) {
            if (st->won[q] == SFK_HAND) {       /* Durchmarsch */
                for (int s = 0; s < TK4_P; s++) score[s] = s == q ? 150 : -50;
                return;
            }
            most = st->pts[q] > most ? st->pts[q] : most;
        }
        for (int q = 0; q < TK4_P; q++)
            if (st->pts[q] == most && st->won[q] > tricks) tricks = st->won[q];
        for (int q = 0; q < TK4_P; q++)
            if (st->pts[q] == most && st->won[q] == tricks) losers |= 1 << q, nl++;
        for (int q = 0; q < TK4_P; q++) score[q] = losers >> q & 1 ? -10 * (TK4_P - nl) : 10 * nl;
        return;
    }
    int side = 0, tricks = 0;
    for (int q = 0; q < TK4_P; q++)
        if (st->team[q] == 0) side += st->pts[q], tricks += st->won[q];
    int won = side >= 61, lose_eyes = won ? 120 - side : side, lose_tricks = won ? SFK_HAND - tricks : tricks;
    int run = runners(st), counted = run >= (x->kind == SFG_WENZ ? 2 : 3) ? run : 0;
    int value = (x->kind == SFG_RUF ? 10 : 50) + 10 * (lose_eyes <= 30) + 10 * (lose_tricks == 0) + 10 * counted;
    int v = won ? value : -value, solo = x->kind != SFG_RUF;
    for (int q = 0; q < TK4_P; q++) score[q] = st->team[q] == 0 ? (solo ? 3 * v : v) : -v;
}

/* the reward: the score over its range (the tariff with the Laufende, schneider and schwarz), a little of the card
 * points; the Ramsch: the payments (the march at the top), a little for each point not taken */
/* the tests: each seat's score as the core counts a deal played out (a dealt or revealed state) */
void sfb_scores(const tb_state *st, int *score) { s_scores(st, score); }

static void s_reward(const tb_state *st, float *rw)
{
    const sfx *x = SCX(st);
    int score[TK4_P];
    s_scores(st, score);
    if (x->kind == SFG_RAMSCH) {
        for (int s = 0; s < TK4_P; s++) rw[s] = 0.55f + (float)score[s] / 80.0f - 0.05f * (float)st->pts[s] / 120.0f;
        return;
    }
    int side = 0, run = runners(st), counted = run >= (x->kind == SFG_WENZ ? 2 : 3) ? run : 0;
    for (int q = 0; q < TK4_P; q++) side += st->team[q] == 0 ? st->pts[q] : 0;
    int unit = x->kind == SFG_RUF ? 10 : 50, v = 0;
    for (int q = 0; q < TK4_P; q++)
        if (st->team[q]) v = -score[q];         /* the side's unit (a solo's: the opponents' payment) */
    float r = 0.5f + 0.5f * (float)v / (float)(unit + 10 * counted + 20) + 0.03f * (float)(side - 60) / 60.0f;
    for (int s = 0; s < TK4_P; s++) rw[s] = st->team[s] ? 1 - r : r;
}

/* each dealt world: the called ace's holder is the partner; the trump run's holders */
static void s_dealt(tb_state *st)
{
    sfx *x = SX(st);
    uint8_t run[SF_RUN];
    int n = 0;
    static const int order[4] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS};
    static const int ranks[6] = {RANK_ACE, 10, RANK_KING, 9, 8, 7};
    if (x->kind != SFG_WENZ)
        for (int i = 0; i < 4; i++) run[n++] = CARD(order[i], RANK_QUEEN);
    for (int i = 0; i < 4; i++) run[n++] = CARD(order[i], RANK_JACK);
    if (x->kind != SFG_WENZ)
        for (int i = 0; i < 6; i++) run[n++] = CARD(x->kind == SFG_SOLO ? x->suit : SUIT_HEARTS, ranks[i]);
    for (int k = 0; k < n; k++)
        for (int s = 0; s < TK4_P && x->holder[k] == SFK_NONE; s++)
            if (trick_find(st->hand[s], st->n[s], run[k]) >= 0) x->holder[k] = (uint8_t)s;
    if (x->kind != SFG_RUF) return;
    int ace = CARD(x->suit, RANK_ACE);
    for (int s = 0; s < TK4_P && x->partner == SFK_NONE; s++)
        if (trick_find(st->hand[s], st->n[s], ace) >= 0) x->partner = (uint8_t)s;
    for (int s = 0; s < TK4_P; s++) st->team[s] = s != x->declarer && s != x->partner;
}

static const tb_rules RULES = {
    .suit = s_suit, .power = s_power, .legal = s_legal, .points = s_points, .trick_end = s_trick_end,
    .reward = s_reward, .want = s_want, .dealt = s_dealt,
};

/* the trump run of a game (as the rules' Laufende) */
static int trump_run(int kind, int suit, uint8_t *out)
{
    static const int order[4] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS};
    static const int ranks[6] = {RANK_ACE, 10, RANK_KING, 9, 8, 7};
    int n = 0;
    if (kind != SFG_WENZ)
        for (int i = 0; i < 4; i++) out[n++] = CARD(order[i], RANK_QUEEN);
    for (int i = 0; i < 4; i++) out[n++] = CARD(order[i], RANK_JACK);
    if (kind != SFG_WENZ)
        for (int i = 0; i < 6; i++) out[n++] = CARD(kind == SFG_SOLO ? suit : SUIT_HEARTS, ranks[i]);
    return n;
}

/* the view of seat me, the deal's game `kind`/`suit` by `declarer` (the bidding looks at games not yet played): its
 * hand, the deal's plays replayed on the scratch game t (who played what: the voids, the called ace's signs) */
static void view(const sfgame *g, int me, int kind, int suit, int declarer, tb_state *v, sfgame *t)
{
    const sfpos *p = &g->pos;
    tb_init(v, &RULES, TK4_P, me);
    sfx *x = SX(v);
    x->kind = (uint8_t)kind;
    x->suit = (uint8_t)suit;
    x->declarer = (uint8_t)declarer;
    x->partner = SFK_NONE;
    v->trump = SUIT_TRUMP;
    v->turn = (uint8_t)tk4_turn(&p->t);
    v->leader = p->t.leader;
    v->tricks = p->t.tricks;
    for (int s = 0; s < TK4_P; s++) {
        v->pts[s] = p->t.eyes[s];
        v->won[s] = p->t.won[s];
        if (s != me) tb_hidden(v, s, p->t.n[s]);
    }
    tb_mine(v, p->t.hand[me], p->t.n[me]);
    uint8_t run[SF_RUN];
    x->nrun = (uint8_t)trump_run(kind, suit, run);
    memset(x->holder, SFK_NONE, sizeof x->holder);
    for (int k = 0; k < x->nrun; k++)
        if (trick_find(p->t.hand[me], p->t.n[me], run[k]) >= 0) x->holder[k] = (uint8_t)me;
    int ace = CARD(suit, RANK_ACE), ruf = kind == SFG_RUF;
    if (ruf) {
        if (trick_find(p->t.hand[me], p->t.n[me], ace) >= 0) x->partner = (uint8_t)me;
        if (declarer != me) tb_exclude(v, declarer, ace);
    }
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (SFA_KIND(g->act[i]) == SFA_NEXT) start = i + 1;
    sfk_new(t, g->variant, g->seed);
    int plays = p->t.tricks * TK4_P, mine_called = 0;
    if (ruf)
        for (int i = 0; i < p->t.n[me]; i++) mine_called += called_plain(x, p->t.hand[me][i]);
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && SFA_KIND(a) == SFA_PLAY && p->phase == SFP_PLAY) {
            const sfpos *q = &t->pos;
            int c = SFA_ARG(a), seat = q->turn, nt = q->t.ntrick;
            uint8_t who[TK4_P];
            for (int j = 0; j < nt; j++) who[j] = (uint8_t)((q->t.leader + j) % TK4_P);
            tb_note(v, seat, c, q->t.trick, who, nt);
            if (k++ < plays) tb_seen(v, c);
            for (int r = 0; r < x->nrun; r++)
                if (run[r] == c) x->holder[r] = (uint8_t)seat;
            if (ruf && c == ace) x->partner = (uint8_t)seat;
            if (ruf && called_plain(x, c) && c != ace && !q->searched && !nt) {
                /* the called suit led first, not with the ace: no ace, unless he could hold four of it */
                int others = mine_called;
                for (int j = 0; j < v->ngone; j++) others += called_plain(x, v->gone[j]) && v->gone[j] != c;
                if (6 - others < 4) tb_exclude(v, seat, ace);
            }
            if (ruf && nt && q->searched == q->t.tricks + 1 && called_plain(x, c) && c != ace)
                tb_exclude(v, seat, ace);       /* followed the sought suit without the ace */
        }
        sfk_play(t, a);
    }
    x->searched = p->phase == SFP_PLAY ? p->searched : 0;
    for (int j = 0; j < p->t.ntrick; j++) tb_trick_add(v, (p->t.leader + j) % TK4_P, p->t.trick[j]);
    if (kind == SFG_RAMSCH)
        for (int s = 0; s < TK4_P; s++) v->team[s] = (uint8_t)s;
    else
        for (int s = 0; s < TK4_P; s++) v->team[s] = s != declarer && s != x->partner;
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

void sfb_view(const sfgame *g, int me, tb_state *v, sfgame *t)
{
    const sfpos *p = &g->pos;
    view(g, me, p->kind, p->suit, p->declarer, v, t);
}

/* ---- the bidding ------------------------------------------------------------------------------------------------ */
typedef struct { int T, O, U, A, tens_ace; } sfhand;

static sfhand strength(const uint8_t *h, int n, int kind, int suit)
{
    sfhand s = {0};
    for (int i = 0; i < n; i++) {
        int c = h[i], r = CARD_RANK(c), tr = sf_pow(c, kind, suit) >= 10;
        s.T += tr;
        s.O += r == RANK_QUEEN && kind != SFG_WENZ;
        s.U += r == RANK_JACK;
        s.A += !tr && r == RANK_ACE;
        if (!tr && r == 10)
            for (int j = 0; j < n; j++) s.tens_ace += h[j] == CARD(CARD_SUIT(c), RANK_ACE);
    }
    return s;
}

/* deals of the unseen cards played out by the heuristic: how often seat me's side scored */
static float wins(const tb_state *v, int samples, ai_rng *r)
{
    tb_state *w = malloc(sizeof *w);
    int done = 0, won = 0;
    if (w)
        for (; done < samples && !ai_cancelled(); done++) {
            *w = *v;
            tb_determinise(w, r);
            uint8_t L[TB_HAND];
            for (int guard = 0; guard < 100 && !tb_over(w); guard++) {
                int n = tb_legal(w, L);
                if (!n) break;
                tb_play(w, tb_heuristic(w, L, n, r));
            }
            int score[TK4_P];
            s_scores(w, score);
            won += score[w->me] > 0;
        }
    free(w);
    return done ? (float)won / (float)done : 0;
}

static int bid(const sfgame *g, int me, int level, uint64_t seed, sfgame *t)
{
    const sfpos *p = &g->pos;
    const uint8_t *h = p->t.hand[me];
    int n = p->t.n[me], cand[3], nc = 0, other = 0;
    for (int q = 0; q < TK4_P; q++) other |= p->word[q] == SFW_PLAY;
    int bs = 0, bt = -1;                        /* a Solo: the suit of most trumps */
    for (int s = 0; s < 4; s++) {
        int T = strength(h, n, SFG_SOLO, s).T;
        if (T > bt) bt = T, bs = s;
    }
    sfhand k = strength(h, n, SFG_SOLO, bs);
    if (k.T >= 7 || (k.T >= 6 && k.O >= 2) || (k.T >= 5 && k.O >= 3 && k.A >= 2)) cand[nc++] = SFG(SFG_SOLO, bs);
    k = strength(h, n, SFG_WENZ, 0);
    int club_u = trick_find(h, n, CARD(SUIT_CLUBS, RANK_JACK)) >= 0;
    if (k.U >= 2 && (k.U >= 3 || club_u) && k.U + k.A + k.tens_ace >= 5) cand[nc++] = SFG(SFG_WENZ, 0);
    k = strength(h, n, SFG_RUF, 0);
    if (!other && ((k.T >= 4 && k.O + k.U >= 2) || (k.T >= 3 && k.O >= 2 && k.A >= 1))) {
        int best = -1, bl = 99, bp = 99;        /* the suit to call: the fewest cards, then the fewest points */
        for (int s = 0; s < 4; s++) {
            if (!sfk_game_ok(p, SFG(SFG_RUF, s))) continue;
            int len = 0, pts = 0;
            for (int i = 0; i < n; i++)
                if (CARD_SUIT(h[i]) == s && sf_pow(h[i], SFG_RUF, 0) < 10) len++, pts += sfk_points(h[i]);
            if (len < bl || (len == bl && pts < bp)) best = s, bl = len, bp = pts;
        }
        if (best >= 0) cand[nc++] = SFG(SFG_RUF, best);
    }
    if (!nc) return SFA_PASS;
    tb_state *v = malloc(sizeof *v);
    int a = SFA_PASS;
    if (v) {
        ai_rng r;
        ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 16 + me));
        int samples = level <= AI_EASY ? 32 : level == AI_MEDIUM ? 64 : 128;
        for (int i = 0; i < nc && a == SFA_PASS; i++) {   /* the highest first */
            int kind = SFG_KIND(cand[i]);
            view(g, me, kind, SFG_SUIT(cand[i]), me, v, t);
            if (wins(v, samples, &r) >= (kind == SFG_RUF ? 0.45f : 0.50f)) a = SFA_GAME | cand[i];
        }
    }
    free(v);
    return a;
}

/* the action for seat at a level (0: nothing to do) */
int sfb_choose(const sfgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const sfpos *p = &g->pos;
    if (p->phase == SFP_DEAL) return SFA_NEXT;
    if (p->phase == SFP_OVER || p->turn != seat) return 0;
    sfgame *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        if (p->phase == SFP_BID) a = bid(g, seat, level, seed, t);
        else {
            sfb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, SF_HARD_ITERS, stats);
            if (c != TB_NONE && sfk_can_play(p, c)) a = SFA_PLAY | c;
        }
    }
    free(t);
    free(v);
    return a && sfk_legal(p, a) ? a : sfk_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    sfgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *schafkopf_live();                     /* the copy, at entry: nothing else is read */
    int a = sfb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    static const char SUITS[4] = {'s', 'h', 'd', 'c'};
    int x = SFA_ARG(a), kind = SFG_KIND(x);
    char c[3];
    switch (SFA_KIND(a)) {
    case SFA_PASS: snprintf(move, (size_t)cap, "p"); return 1;
    case SFA_GAME:
        if (kind == SFG_WENZ) snprintf(move, (size_t)cap, "w");
        else snprintf(move, (size_t)cap, "%c%c", kind == SFG_RUF ? 'r' : 's', SUITS[SFG_SUIT(x)]);
        return 1;
    case SFA_PLAY: trick_name(x, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case SFA_NEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const sfpos *p = &schafkopf_live()->pos;
    return p->phase == SFP_BID || p->phase == SFP_PLAY ? p->turn : -1;
}

static const bot schafkopf_bot = {.think = think, .actor = actor, .play = schafkopf_show};
REGISTER_BOT(schafkopf, &schafkopf_bot)
