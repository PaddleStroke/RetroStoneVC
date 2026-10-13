/*
 * Mexico's bot (bots.h), both lives (three, six) and two to six players. It sees what its seat sees: the values
 * announced in the round, the lives, and the dice under the cup only once it has thrown them itself; never the dice
 * the player before it announced (they stay under the cup until a call).
 * The throw is a move, "t??" (the game's own dice, as a tap on the cup throws them); then it announces a value
 * ("s62", "s21" Mexico), or, believing nobody, calls ("c"). After a call, "n" starts the next round.
 *   The odds: a fresh throw's value (a double 1 in 36, 21 and the others 2 in 36). How players announce: the truth
 *   when their throw reaches the value to beat (now and then a little more), else a lie just above it. So a value
 *   announced is believed by how often the truth would say it against how often a lie would.
 *   Calling loses when the value holds; believing loses when our own throw falls short and the lie we must then
 *   tell is called. It calls when calling risks less.
 *   Announcing: the truth when the throw reaches the value to beat; else the lie the next player would find the
 *   likeliest (just above the value, or the same).
 *   easy    believes or calls on a whim around the plain odds of a throw reaching the value; lies anywhere a little
 *           above it;
 *   medium  calls when a fresh throw would reach the value less than one time in three; lies at the value itself;
 *   hard    the reckoning above: the value's believability from the one announced before it.
 * Runs on the worker thread: it copies its own view of the position at entry and touches nothing else (bots.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/mexico.h"

static double chance(int rank)                  /* a fresh throw's value */
{
    if (rank < 0 || rank >= MX_VALUES) return 0;
    return rank >= 14 && rank < MX_MEXICO ? 1.0 / 36 : 2.0 / 36;
}

static double at_least(int rank)                /* a fresh throw reaches the value */
{
    double s = 0;
    for (int r = rank < 0 ? 0 : rank; r < MX_VALUES; r++) s += chance(r);
    return s;
}

/* how a player announces `said` after throwing x, the value to beat being `before` (-1: none): the truth 4 times in
 * 5 (and a little more the fifth), or a lie just above the value to beat */
static double says(int said, int x, int before)
{
    if (x >= before) {
        if (x == MX_MEXICO) return said == x;
        int top = x + 3 < MX_MEXICO ? x + 3 : MX_MEXICO;
        return said == x ? 0.8 : said > x && said <= top ? 0.2 / (top - x) : 0;
    }
    int top = before + 2 < MX_MEXICO ? before + 2 : MX_MEXICO;
    return said >= before && said <= top ? 1.0 / (top - before + 1) : 0;
}

/* the chance the dice under the cup reach `said`, announced over `before` */
static double believable(int said, int before)
{
    double yes = 0, all = 0;
    for (int x = 0; x < MX_VALUES; x++) {
        double w = chance(x) * says(said, x, before);
        all += w;
        if (x >= said) yes += w;
    }
    return all > 0 ? yes / all : 0;
}

/* the lie to tell over `before`: the one the next player finds likeliest */
static int best_lie(int before)
{
    int best = before < 0 ? 0 : before;
    double bw = -1;
    for (int r = before < 0 ? 0 : before; r < MX_VALUES && r <= before + 2; r++) {
        double w = believable(r, before);
        if (w > bw + 1e-9) bw = w, best = r;
    }
    return best;
}

static int think(int seat, int level, char *move, int cap)
{
    const mxpos *live = &mexico_live()->pos;    /* at entry: the public part, and the dice only if its own throw */
    int phase = live->phase, turn = live->turn, claim = live->claim, n = live->nclaims;
    int before = n >= 2 ? live->claims[n - 2].rank : -1, round = live->round, mine = -1;
    if (phase == MX_SAY && turn == seat) mine = mx_rank(live->dice[0], live->dice[1]);
    if (cap < 8 || turn != seat || phase == MX_OVER) return 0;
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix((uint64_t)round, (uint64_t)n * 8 + seat)));
    if (phase == MX_REVEAL) {
        strcpy(move, "n");
        return 1;
    }
    if (phase == MX_THROW) {
        int call = 0;
        if (claim >= 0) {
            double reach = at_least(claim);
            if (level == AI_EASY) call = reach + 0.3 * (ai_rand_f(&r) - 0.5) < 0.3;
            else if (level == AI_MEDIUM) call = reach < 1.0 / 3;
            else {
                double lose_call = believable(claim, before);
                double lose_believe = (1 - reach) * (1 - believable(best_lie(claim), claim));
                call = lose_call < lose_believe;
            }
        }
        strcpy(move, call ? "c" : "t??");
        return !ai_cancelled();
    }
    if (phase != MX_SAY || mine < 0) return 0;
    int say = mine;
    if (mine < claim) {                         /* a lie: it must */
        if (level == AI_EASY) say = claim + ai_rand_n(&r, 3);
        else if (level == AI_MEDIUM) say = claim;
        else say = best_lie(claim);
        if (say >= MX_VALUES) say = MX_VALUES - 1;
    }
    snprintf(move, (size_t)cap, "s%d", mx_number(say));
    return !ai_cancelled();
}

static const bot mexico_bot = {.think = think, .play = mexico_show};
REGISTER_BOT(mexico, &mexico_bot)
