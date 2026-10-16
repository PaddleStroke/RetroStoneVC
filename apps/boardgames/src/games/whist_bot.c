/*
 * Whist's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the voids shown), partners playing for their side, the reward the side's
 * tricks that count (four players: all thirteen; German whist: those taken once the stock is gone). Runs on the worker
 * thread: it copies the game at entry and reads only what its seat may know (its hand, the turned card while the
 * dealer still holds it, the cards played; German whist: the face-up cards the other took). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * German whist's stock: the core draws from the stock's end and knows only cards at its bottom, so the view puts the
 * face-up card at the bottom and dealt() moves it to the top once the rest is dealt (each search's world).
 * The deal's summary waits for a human tap (actor() -1); think() answers "n" there all the same (the tests).
 * Its moves are the game's notation ("Qs", "n"), played by whist_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/trickbot.h"
#include "games/whist.h"

#define WHIST_HARD_ITERS 8000                   /* about 0.25 s a card on a PC */

typedef struct { uint8_t late[2], faceup; } whx;

static void w_trick_end(tb_state *st, int w)
{
    if (st->players == 2 && st->nstock == 0) TB_XS(st, whx)->late[w]++;
}

static void w_reward(const tb_state *st, float *rw)
{
    const whx *x = TB_CXS(st, whx);
    float r0;
    if (st->players == 2) r0 = 0.5f + (x->late[0] - x->late[1]) / 26.0f;
    else {
        int a = st->won[0] + st->won[2], b = st->won[1] + st->won[3];
        r0 = 0.5f + (a - b) / 26.0f + (a > 6 ? 0.04f : 0) - (b > 6 ? 0.04f : 0);
    }
    for (int s = 0; s < st->players; s++) rw[s] = (st->team[s] == 0) ? r0 : 1 - r0;
}

static void w_dealt(tb_state *st)               /* the face-up card back on top of the stock */
{
    if (!TB_CXS(st, whx)->faceup || st->nstock < 2) return;
    uint8_t t = st->stock[0];
    st->stock[0] = st->stock[st->nstock - 1];
    st->stock[st->nstock - 1] = t;
}

static const tb_rules RULES4 = {.reward = w_reward};
static const tb_rules RULES2 = {.reward = w_reward, .trick_end = w_trick_end, .dealt = w_dealt, .draw = 1};

void whb_view(const whist_game *g, int me, tb_state *v, whist_game *t)
{
    const whist_pos *p = &g->pos;
    int P = p->players;
    tb_init(v, P == 2 ? &RULES2 : &RULES4, P, me);
    v->trump = p->trump;
    for (int s = 0; s < P; s++) v->team[s] = (uint8_t)whist_side(p, s);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    whx *x = TB_XS(v, whx);
    x->late[0] = p->late[0], x->late[1] = p->late[1];
    for (int s = 0; s < P; s++) {
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (WA_KIND(g->act[i]) == WA_NEXT) start = i + 1;
    whist_new(t, g->players, g->variant, g->seed);
    uint8_t pub[WHIST_STOCK], npub = 0, played[52], nplayed = 0;   /* the face-up cards the other took; played */
    for (int i = 0; i < g->n; i++) {
        int a = g->act[i];
        const whist_pos *q = &t->pos;
        if (i >= start && WA_KIND(a) == WA_PLAY) {
            uint8_t who[WHIST_MAXP];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % P);
            tb_note(v, q->turn, WA_ARG(a), q->trick, who, q->ntrick);
            played[nplayed++] = (uint8_t)WA_ARG(a);
        }
        int tricks = q->tricks;
        whist_play(t, a);
        if (i >= start && P == 2 && q->tricks != tricks && q->drew[0] != CARD_NONE && q->last_winner != me)
            pub[npub++] = q->drew[0];
    }
    for (int k = 0; k < nplayed - p->ntrick; k++) tb_seen(v, played[k]);
    int other = (me + 1) % P;
    if (P == 4 && p->dealer != me) {            /* the turned card, while the dealer has not played it */
        int gone = 0;
        for (int k = 0; k < nplayed; k++) gone |= played[k] == p->up;
        if (!gone) tb_known(v, p->dealer, p->up);
    }
    if (P == 2)
        for (int k = 0; k < npub; k++) {
            int gone = 0;
            for (int j = 0; j < nplayed; j++) gone |= played[j] == pub[k];
            if (!gone) tb_known(v, other, pub[k]);
        }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % P, p->trick[j]);
    if (P == 2 && p->nstock) {
        uint8_t top = p->stock[p->nstock - 1];
        tb_stock_set(v, p->nstock, &top, 1);
        x->faceup = 1;
    }
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_52, pack));
}

int whb_choose(const whist_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const whist_pos *p = &g->pos;
    if (p->phase == WP_ROUND) return WA_NEXT;
    if (p->phase != WP_PLAY || p->turn != seat) return 0;
    whist_game *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        whb_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, p->players == 2 ? WHIST_HARD_ITERS / 2 : WHIST_HARD_ITERS, stats);
        if (c != TB_NONE && whist_can_play(p, c)) a = WA_PLAY | c;
    }
    free(t);
    free(v);
    return a ? a : whist_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    whist_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *whist_live();                         /* the copy, at entry: nothing else is read */
    int a = whb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[3];
    if (WA_KIND(a) == WA_NEXT) { snprintf(move, (size_t)cap, "n"); return 1; }
    trick_name(WA_ARG(a), c);
    snprintf(move, (size_t)cap, "%s", c);
    return 1;
}

static int actor(void)
{
    const whist_pos *p = &whist_live()->pos;
    return p->phase == WP_PLAY ? p->turn : -1;
}

static const bot whist_bot = {.think = think, .actor = actor, .play = whist_show};
REGISTER_BOT(whist, &whist_bot)
