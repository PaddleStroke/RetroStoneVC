/*
 * Mexico's rules (mexico.h): the values and their ranking, the throws, the announcements, the call, the lives.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/mexico.h"

/* the values as read, lowest first */
static const uint8_t NUMBERS[MX_VALUES] = {31, 32, 41, 42, 43, 51, 52, 53, 54, 61, 62, 63, 64, 65,
                                           11, 22, 33, 44, 55, 66, 21};

int mx_number(int rank) { return rank >= 0 && rank < MX_VALUES ? NUMBERS[rank] : 0; }

int mx_from_number(int number)
{
    for (int r = 0; r < MX_VALUES; r++)
        if (NUMBERS[r] == number) return r;
    return -1;
}

int mx_rank(int d1, int d2)
{
    int hi = d1 > d2 ? d1 : d2, lo = d1 > d2 ? d2 : d1;
    return mx_from_number(hi * 10 + lo);
}

void mx_dice_of(int rank, int *hi, int *lo)
{
    int v = mx_number(rank);
    *hi = v / 10;
    *lo = v % 10;
}

void mx_new(mxgame *g, int players, int lives)
{
    memset(g, 0, sizeof *g);
    mxpos *p = &g->pos;
    p->players = (uint8_t)(players < 2 ? 2 : players > MX_MAXP ? MX_MAXP : players);
    p->start_lives = (uint8_t)lives;
    for (int i = 0; i < p->players; i++) p->lives[i] = (uint8_t)lives;
    p->claim = -1;
    p->winner = -1;
    p->phase = MX_THROW;
}

int mx_next(const mxpos *p, int player)
{
    for (int i = 1; i <= p->players; i++) {
        int q = (player + i) % p->players;
        if (p->lives[q]) return q;
    }
    return player;
}

int mx_alive(const mxpos *p)
{
    int n = 0;
    for (int i = 0; i < p->players; i++) n += p->lives[i] > 0;
    return n;
}

int mx_legal(const mxpos *p, int act)
{
    int kind = MX_KIND(act), arg = MX_ARG(act);
    switch (kind) {
    case MXA_THROW: {
        int d1 = arg / 8, d2 = arg % 8;
        return p->phase == MX_THROW && d1 >= 1 && d1 <= 6 && d2 >= 1 && d2 <= 6;
    }
    case MXA_SAY: return p->phase == MX_SAY && arg < MX_VALUES && (int)arg >= p->claim;
    case MXA_CALL: return p->phase == MX_THROW && p->claim >= 0;
    case MXA_NEXT: return p->phase == MX_REVEAL;
    }
    return 0;
}

int mx_play(mxgame *g, int act)
{
    mxpos *p = &g->pos;
    if (!mx_legal(p, act)) return 0;
    int arg = MX_ARG(act);
    switch (MX_KIND(act)) {
    case MXA_THROW:
        p->dice[0] = (uint8_t)(arg / 8);
        p->dice[1] = (uint8_t)(arg % 8);
        p->phase = MX_SAY;
        break;
    case MXA_SAY:
        if (p->nclaims == MX_CLAIMS) {          /* a long round: the oldest forgotten */
            memmove(p->claims, p->claims + 1, sizeof p->claims[0] * (MX_CLAIMS - 1));
            p->nclaims--;
        }
        p->claims[p->nclaims].player = p->turn;
        p->claims[p->nclaims++].rank = (uint8_t)arg;
        p->claim = (int8_t)arg;
        p->claimer = p->turn;
        p->turn = (uint8_t)mx_next(p, p->turn);
        p->phase = MX_THROW;
        break;
    case MXA_CALL:
        p->caller = p->turn;
        p->held = mx_rank(p->dice[0], p->dice[1]) >= p->claim;
        p->loser = p->held ? p->caller : p->claimer;
        p->lives[p->loser]--;
        p->phase = MX_REVEAL;
        if (mx_alive(p) == 1) {
            p->phase = MX_OVER;
            p->winner = (int8_t)mx_next(p, p->loser);
        }
        break;
    case MXA_NEXT:
        p->round++;
        p->starter = (uint8_t)(p->lives[p->loser] ? p->loser : mx_next(p, p->loser));
        p->turn = p->starter;
        p->claim = -1;
        p->nclaims = 0;
        p->dice[0] = p->dice[1] = 0;
        p->phase = MX_THROW;
        break;
    }
    if (g->n < MX_MAX_ACTS) g->act[g->n] = (uint16_t)act;   /* the first ones kept */
    g->n++;
    return 1;
}

int mx_script(mxgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act = 0;
        if (s[0] == 't' && s[1] >= '1' && s[1] <= '6' && s[2] >= '1' && s[2] <= '6') {
            act = MX_ACT(MXA_THROW, MX_DICE(s[1] - '0', s[2] - '0'));
            s += 3;
        } else if (s[0] == 's' && s[1] >= '1' && s[1] <= '6' && s[2] >= '1' && s[2] <= '6') {
            int r = mx_from_number((s[1] - '0') * 10 + (s[2] - '0'));
            if (r < 0) return 0;
            act = MX_ACT(MXA_SAY, r);
            s += 3;
        } else if (s[0] == 'c') {
            act = MX_ACT(MXA_CALL, 0);
            s++;
        } else if (s[0] == 'n') {
            act = MX_ACT(MXA_NEXT, 0);
            s++;
        } else return 0;
        if (!mx_play(g, act)) return 0;
    }
    return 1;
}
