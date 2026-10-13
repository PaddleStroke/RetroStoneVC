/*
 * Briscola's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo
 * over deals of the unseen cards to the other hands and the stock, the turned briscola kept at the bottom, then in
 * the hand of whoever drew it), partners playing for their side with four, the reward the game's result and its
 * points (a game is one deal). Runs on the worker thread: it copies the game at entry and reads only what its seat may
 * know (its hand, the briscola, the cards played, how many are left to draw). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * No calls: every move is a card. The playouts: lead a low card that is worth nothing (a liscio), give points to a
 * partner holding the trick, take a trick worth points with the cheapest card that holds it, keep the trumps for
 * tricks with points in them.
 * Its moves are the game's notation ("1b", "Rc", italian.h), played by briscola_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/briscola.h"
#include "games/trickbot.h"

#define BRISCOLA_HARD_ITERS 20000               /* about 0.3 s a card on a PC */

static int b_power(const tb_state *st, int c, int led)
{
    int s = CARD_SUIT(c);
    if (s == st->trump) return 100 + briscola_strength(c);
    return s == led ? briscola_strength(c) : 0;
}

static int b_points(const tb_state *st, int c) { (void)st; return briscola_points(c); }

static void b_reward(const tb_state *st, float *rw)
{
    int side[2] = {0, 0};
    for (int s = 0; s < st->players; s++) side[st->team[s] & 1] += st->pts[s];
    for (int s = 0; s < st->players; s++) {
        int me = side[st->team[s] & 1], them = side[!(st->team[s] & 1)];
        float win = me > them ? 1 : me == them ? 0.5f : 0;
        rw[s] = 0.8f * win + 0.2f * me / (float)BRISCOLA_TOTAL;
    }
}

static const tb_rules RULES = {
    .power = b_power, .points = b_points, .reward = b_reward, .draw = 1, .nofollow = 1,
};

/* the view of seat me */
void brib_view(const brgame *g, int me, tb_state *v, brgame *t)
{
    const brpos *p = &g->pos;
    int np = p->players;
    tb_init(v, &RULES, np, me);
    v->trump = p->trump;
    for (int s = 0; s < np; s++) v->team[s] = (uint8_t)briscola_side(p, s);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    for (int s = 0; s < np; s++)
        if (s != me) tb_hidden(v, s, p->n[s]);
    tb_mine(v, p->hand[me], p->n[me]);
    /* the points taken so far, on each side's first seat (the reward adds a side's seats) */
    v->pts[0] = p->points[0];
    v->pts[1] = p->points[1];
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < p->ntook[s]; i++) tb_seen(v, p->took[s][i]);
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % np, p->trick[j]);
    /* the briscola: at the bottom of the stock; once drawn, in the hand of the last to draw until played */
    int owner = -1;
    if (!p->nstock) {
        briscola_new(t, g->players, g->seed);
        for (int i = 0; i < g->n; i++) {
            briscola_play(t, g->act[i]);
            for (int k = 0; k < t->pos.ndrew; k++)
                if (t->pos.drew[k] == p->briscola && t->pos.ntrick == 0) owner = (t->pos.last_winner + k) % np;
        }
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < p->ntook[s]; i++) owner = p->took[s][i] == p->briscola ? -1 : owner;
        for (int j = 0; j < p->ntrick; j++) owner = p->trick[j] == p->briscola ? -1 : owner;
        if (owner >= 0 && owner != me) tb_known(v, owner, p->briscola);
    }
    tb_stock_set(v, p->nstock, &p->briscola, p->nstock ? 1 : 0);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_40, pack));
}

/* the card for seat at a level (-1: nothing to do) */
int brib_choose(const brgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const brpos *p = &g->pos;
    if (p->phase != BR_PLAY || p->turn != seat) return -1;
    brgame *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int c = TB_NONE;
    if (t && v) {
        brib_view(g, seat, v, t);
        c = tb_choose(v, level, seed, BRISCOLA_HARD_ITERS, stats);
    }
    free(t);
    free(v);
    return c != TB_NONE && briscola_legal(p, c) ? c : briscola_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    brgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *briscola_live();                      /* the copy, at entry: nothing else is read */
    int c = brib_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (c < 0 || ai_cancelled() || cap < 3) return 0;
    it_name(c, move);
    return 1;
}

static const bot briscola_bot = {.think = think, .play = briscola_show};
REGISTER_BOT(briscola, &briscola_bot)
