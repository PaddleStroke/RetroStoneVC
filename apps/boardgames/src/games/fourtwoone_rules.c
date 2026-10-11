/*
 * 421: the rules (fourtwoone.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/fourtwoone.h"

/* the faces, highest first */
static void sorted(const uint8_t d[FTO_DICE], int s[FTO_DICE])
{
    for (int i = 0; i < FTO_DICE; i++) s[i] = d[i];
    for (int i = 1; i < FTO_DICE; i++)
        for (int j = i; j > 0 && s[j] > s[j - 1]; j--) {
            int t = s[j];
            s[j] = s[j - 1];
            s[j - 1] = t;
        }
}

int fto_rank(const uint8_t d[FTO_DICE])
{
    int s[FTO_DICE];
    sorted(d, s);
    int a = s[0], b = s[1], c = s[2];
    if (a == 4 && b == 2 && c == 1) return 1000;
    if (a == 1) return 900;                     /* 1-1-1 */
    if (b == 1 && c == 1) return 800 + 2 * a + 1;   /* a fiche: 1-1-a */
    if (a == b && b == c) return 800 + 2 * a;   /* three a */
    if (b == a - 1 && c == a - 2) return 700 + a;   /* a run */
    if (a == 2 && b == 2 && c == 1) return 0;   /* the nénette */
    return 100 + 49 * a + 7 * b + c;
}

int fto_value(const uint8_t d[FTO_DICE])
{
    int r = fto_rank(d);
    if (r == 1000) return 10;
    if (r == 900) return 7;
    if (r > 800) return (r - 800) / 2;          /* fiches and threes: the face */
    if (r > 700) return 2;                      /* runs */
    return 1;
}

void fto_new(tgame *g, int players)
{
    memset(g, 0, sizeof *g);
    tpos *p = &g->pos;
    p->players = (uint8_t)(players < 2 ? 2 : players > FTO_PLAYERS ? FTO_PLAYERS : players);
    p->pot = FTO_CHIPS;
    p->phase = FTO_CHARGE;
    p->in = (uint8_t)((1u << p->players) - 1);
    p->limit = 1;
    p->loser = -1;
    p->first_out = -1;
    p->last_win = p->last_lose = -1;
}

static int order_of(const tpos *p, int q)
{
    for (int i = 0; i < p->norder; i++)
        if (p->order[i] == q) return i;
    return FTO_PLAYERS;
}

int fto_better(const tpos *p, int a, int b)
{
    int ra = fto_rank(p->res[a]), rb = fto_rank(p->res[b]);
    if (ra != rb) return ra > rb;
    if (p->res_throws[a] != p->res_throws[b]) return p->res_throws[a] < p->res_throws[b];
    return order_of(p, a) < order_of(p, b);
}

int fto_best(const tpos *p)
{
    int w = -1;
    for (int i = 0; i < p->norder; i++)
        if (w < 0 || fto_better(p, p->order[i], w)) w = p->order[i];
    return w;
}

int fto_worst(const tpos *p)
{
    int w = -1;
    for (int i = 0; i < p->norder; i++)
        if (w < 0 || fto_better(p, w, p->order[i])) w = p->order[i];
    return w;
}

static int count(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

static void goes_out(tpos *p, int q)
{
    p->in &= (uint8_t)~(1u << q);
    if (p->first_out < 0) p->first_out = (int8_t)q;
}

/* the round over: the chips go, the next round (or the end) */
static void settle(tpos *p)
{
    int best = fto_best(p), worst = fto_worst(p), amount = fto_value(p->res[best]);
    p->last_win = (int8_t)best;
    p->last_lose = (int8_t)worst;
    p->last_in = 0;
    memcpy(p->last_res, p->res, sizeof p->res);
    memcpy(p->last_throws, p->res_throws, sizeof p->res_throws);
    memcpy(p->last_order, p->order, sizeof p->order);
    for (int i = 0; i < p->norder; i++) p->last_in |= (uint8_t)(1u << p->order[i]);
    if (p->phase == FTO_CHARGE) {
        if (amount > p->pot) amount = p->pot;
        p->chips[worst] = (uint8_t)(p->chips[worst] + amount);
        p->pot = (uint8_t)(p->pot - amount);
        p->last_pot = 1;
        if (!p->pot) {                          /* the charge over: who has nothing is out */
            p->phase = FTO_DISCHARGE;
            for (int k = 1; k <= p->players; k++) {   /* round the table from the loser */
                int q = (worst + k) % p->players;
                if (!p->chips[q]) goes_out(p, q);
            }
        }
    } else {
        if (amount > p->chips[best]) amount = p->chips[best];
        p->chips[best] = (uint8_t)(p->chips[best] - amount);
        p->chips[worst] = (uint8_t)(p->chips[worst] + amount);
        p->last_pot = 0;
        if (!p->chips[best]) goes_out(p, best);
    }
    p->last_amount = (uint8_t)amount;
    p->rounds++;
    p->done = 0;
    p->norder = 0;
    p->throws = 0;
    p->kept = 0;
    memset(p->dice, 0, sizeof p->dice);
    memset(p->res, 0, sizeof p->res);
    memset(p->res_throws, 0, sizeof p->res_throws);
    if (p->phase == FTO_DISCHARGE && count(p->in) <= 1) {  /* one holds all the chips: they have lost */
        p->phase = FTO_OVER;
        p->loser = (int8_t)worst;
        return;
    }
    p->leader = p->turn = (uint8_t)worst;       /* the loser leads the next round */
    p->limit = p->phase == FTO_CHARGE ? 1 : FTO_THROWS;
}

/* the player to throw's turn over: their result, the next player round the table, or the round settled */
static void finish(tpos *p)
{
    int me = p->turn;
    memcpy(p->res[me], p->dice, FTO_DICE);
    p->res_throws[me] = p->throws;
    p->done |= (uint8_t)(1u << me);
    p->order[p->norder++] = (uint8_t)me;
    if (me == p->leader && p->phase == FTO_DISCHARGE) p->limit = p->throws;  /* the others: no more throws */
    p->throws = 0;
    p->kept = 0;
    for (int k = 1; k < p->players; k++) {
        int q = (me + k) % p->players;
        if ((p->in >> q & 1) && !(p->done >> q & 1)) {
            p->turn = (uint8_t)q;
            memset(p->dice, 0, sizeof p->dice);
            return;
        }
    }
    settle(p);
}

static void push(tgame *g, uint16_t ev)
{
    g->past[g->n] = g->pos;
    g->ev[g->n++] = ev;
}

int fto_throw(tgame *g, unsigned kept, const int v[FTO_DICE])
{
    tpos *p = &g->pos;
    if (p->phase == FTO_OVER || p->throws >= p->limit || g->n >= FTO_MAX_EVENTS) return 0;
    if (!p->throws) kept = 0;
    kept &= (1u << FTO_DICE) - 1;
    if (kept == (1u << FTO_DICE) - 1) return 0;
    uint16_t ev = FTO_THROW_EV;
    uint8_t d[FTO_DICE];
    for (int i = 0; i < FTO_DICE; i++) {
        if (!(kept >> i & 1) && (v[i] < 1 || v[i] > 6)) return 0;
        d[i] = (uint8_t)(kept >> i & 1 ? p->dice[i] : v[i]);
        ev |= (uint16_t)(d[i] << (3 * i));
    }
    push(g, ev);
    memcpy(p->dice, d, sizeof d);
    p->kept = (uint8_t)kept;
    p->throws++;
    if (p->throws >= p->limit) finish(p);
    return 1;
}

int fto_keep(tgame *g, unsigned kept)
{
    tpos *p = &g->pos;
    if (p->phase == FTO_OVER || !p->throws || p->throws >= p->limit) return 0;
    p->kept = (uint8_t)(kept & ((1u << FTO_DICE) - 1));
    return 1;
}

int fto_can_stop(const tpos *p) { return p->phase != FTO_OVER && p->throws >= 1 && p->throws < p->limit; }

int fto_stop(tgame *g)
{
    tpos *p = &g->pos;
    if (!fto_can_stop(p) || g->n >= FTO_MAX_EVENTS) return 0;
    push(g, 0);
    finish(p);
    return 1;
}

int fto_undo(tgame *g)
{
    int k = g->n - 1;
    while (k >= 0 && (g->ev[k] & FTO_THROW_EV)) k--;
    if (k < 0) return 0;
    g->pos = g->past[k];
    g->n = k;
    return 1;
}

unsigned fto_winners(const tpos *p)
{
    if (p->phase != FTO_OVER || p->loser < 0) return 0;
    return ((1u << p->players) - 1) & ~(1u << p->loser);
}

int fto_script(tgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len = 0;
        while (s[len] && s[len] != ' ') len++;
        if (*s == 'd') {
            int v[FTO_DICE] = {0};
            unsigned kept = 0;
            if (len != 1 + FTO_DICE) return 0;
            for (int i = 0; i < FTO_DICE; i++) {
                char c = s[1 + i];
                if (c == '-') kept |= 1u << i;
                else if (c >= '1' && c <= '6') v[i] = c - '0';
                else return 0;
            }
            if (!g->pos.throws && kept) return 0;
            if (!fto_throw(g, kept, v)) return 0;
        } else if (*s == 's' && len == 1) {
            if (!fto_stop(g)) return 0;
        } else return 0;
        s += len;
    }
    return 1;
}
