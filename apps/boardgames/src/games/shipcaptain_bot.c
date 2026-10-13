/*
 * Ship, Captain and Crew's bot (bots.h), two to six players. The roll is a move: "r" and a "?" for each die it
 * throws ("r?????", then "r???" with the ship in...), played by shipcaptain_show() with the game's own dice as a tap
 * rolls them; "s" stops (with ship, captain and crew in and rolls left). Until the crew is in there is nothing to
 * choose: it rolls.
 *   easy    stops on a cargo of its whim (6 to 9), now and then forgetting the cargo to beat;
 *   medium  stops on 8 or more once it is at least the best cargo of the round so far (the last to play: once it
 *           is), rolls the cargo again otherwise;
 *   hard    weighs each cargo by its chance of the round's chip: none under the best cargo already made (level with
 *           it: a chip shared), and for each player still to play, the chance they beat it, playing to beat it
 *           (their whole turn worked out: the ship, captain and crew coming in their three rolls, then the cargo
 *           rolled again while it is under ours); then stops or rolls again by the expected worth.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/shipcaptain.h"

/* a roll at stage s (0 nothing in, 1 the ship, 2 ship and captain): the chance of each stage after it, and with the
 * crew in, of each cargo (2..12): go[s][s2][cargo] */
typedef struct { double go[3][4][13]; } rolls_t;

static void rolls_make(rolls_t *R)
{
    memset(R, 0, sizeof *R);
    for (int s = 0; s < 3; s++) {
        int n = SCC_DICE - s, all = 1;
        for (int i = 0; i < n; i++) all *= 6;
        for (int code = 0; code < all; code++) {
            int d[SCC_DICE], used[SCC_DICE] = {0}, st = s;
            for (int i = 0, x = code; i < n; i++, x /= 6) d[i] = 1 + x % 6;
            for (int want = 6 - st; st < 3; want = 6 - st) {   /* the ship, the captain, the crew, in order */
                int found = -1;
                for (int i = 0; i < n && found < 0; i++)
                    if (!used[i] && d[i] == want) found = i;
                if (found < 0) break;
                used[found] = 1;
                st++;
            }
            int cargo = 0;
            if (st == 3)
                for (int i = 0; i < n; i++) cargo += used[i] ? 0 : d[i];
            R->go[s][st][cargo] += 1.0 / all;
        }
    }
}

static const double TWO[13] = {0, 0, 1 / 36.0, 2 / 36.0, 3 / 36.0, 4 / 36.0, 5 / 36.0, 6 / 36.0, 5 / 36.0, 4 / 36.0,
                               3 / 36.0, 2 / 36.0, 1 / 36.0};

/* a player out to beat `target`, with the crew in and cargo x after `used` rolls: the chance of ending above it
 * (*eq: level with it); the cargo rolled again while under it */
static double beat_from(int x, int used, int target, double *eq)
{
    if (x > target) { *eq = 0; return 1; }
    if (used >= SCC_ROLLS || x == target) {
        *eq = x == target;
        return 0;
    }
    double gt = 0, e = 0;
    for (int y = 2; y <= 12; y++) {
        double ey;
        gt += TWO[y] * beat_from(y, used + 1, target, &ey);
        e += TWO[y] * ey;
    }
    *eq = e;
    return gt;
}

/* the same from stage s after `used` rolls (the crew not yet in) */
static double beat_stage(const rolls_t *R, int s, int used, int target, double *eq)
{
    *eq = 0;
    if (used >= SCC_ROLLS) return 0;
    double gt = 0;
    for (int s2 = 0; s2 <= 3; s2++)
        for (int c = 0; c <= 12; c++) {
            double p = R->go[s][s2][c], e;
            if (p <= 0) continue;
            gt += p * (s2 < 3 ? beat_stage(R, s2, used + 1, target, &e) : beat_from(c, used + 1, target, &e));
            *eq += p * e;
        }
    return gt;
}

/* hard: what ending on cargo x is worth: the round's chip (shared: less) */
typedef struct { double u[13]; } stakes;

static void stakes_make(const rolls_t *R, const sccpos *p, stakes *S)
{
    int me = p->turn, best = 0, later = 0;
    for (int q = 0; q < p->players; q++) {
        if (q == me) continue;
        if (p->done >> q & 1) best = p->res[q] > best ? p->res[q] : best;
        else later++;
    }
    for (int x = 0; x <= 12; x++) {
        double w = x < 2 || x < best ? 0 : x == best ? 0.6 : 1;
        if (w > 0 && later) {
            double eq, gt = beat_stage(R, 0, 0, x, &eq);
            for (int k = 0; k < later; k++) w *= 1 - gt - 0.4 * eq;
        }
        S->u[x] = w;
    }
}

static double worth(const stakes *S, int x, int used)   /* the crew in, cargo x after `used` rolls, played well */
{
    if (used >= SCC_ROLLS) return S->u[x];
    double again = 0;
    for (int y = 2; y <= 12; y++) again += TWO[y] * worth(S, y, used + 1);
    return S->u[x] > again ? S->u[x] : again;
}

static int stops(const sccpos *p, int level, ai_rng *r)
{
    int x = scc_cargo(p), me = p->turn, best = 0, later = 0;
    for (int q = 0; q < p->players; q++) {
        if (q == me) continue;
        if (p->done >> q & 1) best = p->res[q] > best ? p->res[q] : best;
        else later++;
    }
    if (level == AI_EASY) {
        int whim = 6 + ai_rand_n(r, 4);
        return x >= whim && (ai_rand_n(r, 2) || x >= best);
    }
    if (level == AI_MEDIUM) return later ? x >= 8 && x >= best : x >= best;
    rolls_t R;
    stakes S;
    rolls_make(&R);
    stakes_make(&R, p, &S);
    double again = 0;
    for (int y = 2; y <= 12; y++) again += TWO[y] * worth(&S, y, p->rolls + 1);
    return S.u[x] >= again;
}

static int think(int seat, int level, char *move, int cap)
{
    sccpos p = shipcaptain_live()->pos;         /* the copy, at entry: nothing else is read */
    if (p.phase != SCC_PLAY || p.turn != seat || cap < 8) return 0;
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.rounds, (uint64_t)p.rolls * 8 + p.turn)));
    if (scc_can_stop(&p) && stops(&p, level, &r)) strcpy(move, "s");
    else {
        int n = __builtin_popcount(scc_free(&p)), len = 0;
        move[len++] = 'r';
        while (n--) move[len++] = '?';
        move[len] = 0;
    }
    return !ai_cancelled();
}

static const bot shipcaptain_bot = {.think = think, .play = shipcaptain_show};
REGISTER_BOT(shipcaptain, &shipcaptain_bot)
