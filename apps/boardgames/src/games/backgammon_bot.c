/*
 * Backgammon's bot (bots.h), all four games of the board: backgammon, plakoto, fevga, long nardy. The throw is a
 * move: "roll", played by backgammon_show() with the game's own dice, as a tap throws them (the opening roll too).
 * After a throw the bot plays the whole turn in its head: every way to play the dice (the rules' moves of one die,
 * the most dice used, the larger die when only one can be), the positions they lead to, each weighed; then it plays
 * the best way's first checker, one die at a time, as a player taps them (and thinks again for the next one).
 * The weighing, from the side that just moved, the other to throw:
 *  - the pips (the race), and in a pure race the checkers still to bear off;
 *  - backgammon: the points made (the 5, 4 and bar points first, the anchors in the other's home board), the primes
 *    with the other's checkers behind them, the other's checkers on the bar against our home board, piles of
 *    checkers; the blots: for each of the other's 36 throws, the worst blot it can hit (direct, combined, doubles,
 *    from the bar), weighed by the pips lost and the other's home board;
 *  - plakoto: the pins (the other's checkers held under ours, a pinned mother most of all), our own pinned, the blots
 *    the other can pin by the same count of throws (the mother's above all);
 *  - fevga, long nardy (no hitting): the points held in front of the other's checkers, runs of them most.
 * Levels: easy weighs the pips and the blots only, blurred (it plays a human's loose moves); medium all of it, a
 * little blurred; hard all of it and one throw further: for the best ways, each of the other's 21 throws and its
 * best answer, the way that leaves the other the least.
 * There is no doubling cube in these rules: nothing to offer or take.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "roll", or one checker by one die, "13/8", "bar/22", "6/off", a hit "13/8*".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/backgammon.h"

#define WIN_SCORE 1000000
#define LEAVES 4096                             /* the ways to play a throw, at most (different positions) */
#define SEEN_BITS 14

typedef struct {
    bpos end;                                   /* the position after it (the turn not passed) */
    bmove seq[4];
    signed char n;                              /* moves */
    unsigned char dice;                         /* the dice used (bit i: die i) */
    int score;
} way;

typedef struct {
    way *w;
    int n, most;
    uint64_t key[1 << SEEN_BITS];               /* positions seen (a way, or a step on the way), this generation */
    unsigned gen[1 << SEEN_BITS], now;
    long work;                                  /* positions made: the budget */
} ways;

/* ---- every way to play the dice ---------------------------------------------------------------------------------- */
static uint64_t pos_key(const bpos *p, int k)
{
    uint64_t h = (uint64_t)k * 0x9e3779b97f4a7c15ull, w;
    const unsigned char *b = (const unsigned char *)p->n;
    for (size_t i = 0; i + 8 <= sizeof p->n; i += 8) {
        memcpy(&w, b + i, 8);
        h = ai_mix(h, w);
    }
    for (size_t i = sizeof p->n / 8 * 8; i < sizeof p->n; i++) h = ai_mix(h, b[i]);
    if (p->variant == BG_PLAKOTO) {
        b = (const unsigned char *)p->pin;
        for (size_t i = 0; i + 8 <= sizeof p->pin; i += 8) {
            memcpy(&w, b + i, 8);
            h = ai_mix(h, w);
        }
        for (size_t i = sizeof p->pin / 8 * 8; i < sizeof p->pin; i++) h = ai_mix(h, b[i]);
    }
    return ai_mix(h, (uint64_t)p->used | (uint64_t)p->head << 8);
}

static int seen(ways *t, uint64_t key)          /* 1: seen this generation; else noted */
{
    unsigned mask = (1u << SEEN_BITS) - 1, i = (unsigned)key & mask;
    for (int probe = 0; probe < 64; probe++, i = (i + 1) & mask) {
        if (t->gen[i] != t->now) {
            t->gen[i] = t->now;
            t->key[i] = key;
            return 0;
        }
        if (t->key[i] == key) return 1;
    }
    return 0;                                   /* full round here: count it as new */
}

/* the same checker off with a smaller die still to play: the notation's move ("6/off") takes the smaller one, so
 * the bot plays it so too */
static int smaller_off(const bpos *p, int di, bmove m)
{
    if (m.to != BG_OFF) return 0;
    for (int j = 0; j < p->nd; j++) {
        if ((p->used >> j & 1) || p->die[j] >= p->die[di]) continue;
        bmove l[BG_MAXMOVES];
        int c = bg_singles(p, j, l);
        for (int k = 0; k < c; k++)
            if (l[k].from == m.from && l[k].to == BG_OFF) return 1;
    }
    return 0;
}

static void walk(ways *t, const bpos *p, bmove *seq, int k)
{
    int any = 0;
    unsigned tried = 0;
    t->work++;
    for (int i = 0; i < p->nd; i++) {
        if ((p->used >> i & 1) || (tried >> p->die[i] & 1)) continue;
        tried |= 1u << p->die[i];
        bmove m[BG_MAXMOVES];
        int c = bg_singles(p, i, m);
        for (int j = 0; j < c; j++) {
            if (smaller_off(p, i, m[j])) continue;
            any = 1;
            bpos q = *p;
            bg_apply(&q, m[j]);
            seq[k] = m[j];
            if (k + 1 < p->nd && seen(t, pos_key(&q, k + 1))) continue;   /* reached another way already */
            walk(t, &q, seq, k + 1);
        }
    }
    if (any) return;
    if (k > t->most) t->most = k;
    if (k < t->most || t->n >= LEAVES || (k == p->nd && seen(t, pos_key(p, 99)))) return;
    way *w = &t->w[t->n++];
    w->end = *p;
    memcpy(w->seq, seq, sizeof w->seq);
    w->n = (signed char)k;
    w->dice = (unsigned char)p->used;
    w->score = 0;
}

/* the ways to play p's dice (p: the moves phase), the rules' choice: the most dice, the larger die alone */
static int all_ways(ways *t, const bpos *p)
{
    bmove seq[4] = {{0}};
    t->n = t->most = 0;
    t->now++;
    walk(t, p, seq, 0);
    int k = 0;
    for (int i = 0; i < t->n; i++)
        if (t->w[i].n == t->most) t->w[k++] = t->w[i];
    t->n = k;
    if (t->most == 1 && p->nd == 2 && !p->used && p->die[0] != p->die[1]) {
        int big = p->die[0] > p->die[1] ? 0 : 1, any = 0;
        for (int i = 0; i < t->n; i++) any |= t->w[i].dice >> big & 1;
        if (any) {
            k = 0;
            for (int i = 0; i < t->n; i++)
                if (t->w[i].dice >> big & 1) t->w[k++] = t->w[i];
            t->n = k;
        }
    }
    return t->n;
}

/* the way played from the start, the end checked after each move: 1 the mover won, 2 a draw, 0 the game goes on */
static int finish(const bpos *root, const way *w, bpos *out)
{
    *out = *root;
    for (int i = 0; i < w->n; i++) {
        bg_apply(out, w->seq[i]);
        if (bg_settle(out)) return out->tie ? 2 : out->side == root->side ? 1 : 3;
    }
    return 0;
}

/* ---- the weighing ------------------------------------------------------------------------------------------------ */
static int opposite(const bpos *p) { return p->variant == BG_BACKGAMMON || p->variant == BG_PLAKOTO; }

/* the other's checkers on each of s's points (s's numbering): opposite ways, the other's bar is s's 0 */
static void theirs(const bpos *p, int s, int *on)
{
    memset(on, 0, 26 * sizeof *on);
    for (int j = 1; j <= 24; j++) on[bg_opp(p->variant, j)] += p->n[!s][j];
    if (opposite(p)) on[0] = p->n[!s][BG_BAR];
}

static int made_home(const bpos *p, int s)      /* s's home board points made */
{
    int k = 0;
    for (int i = 1; i <= 6; i++) k += p->n[s][i] >= 2;
    return k;
}

/* the other may land on s's point x on the way (backgammon: fewer than two of s's; plakoto: not a point where s
 * pins one of the other's) */
static int open_for(const bpos *p, int s, int x)
{
    if (x < 1 || x > 24) return 0;
    int c = p->n[s][x];
    if (c >= 2) return 0;
    return !(p->variant == BG_PLAKOTO && c == 1 && p->pin[!s][bg_opp(p->variant, x)]);
}

/* the other hits (or pins) s's blot on b with the throw d1, d2 */
static int hits(const bpos *p, int s, const int *on, int b, int d1, int d2)
{
    int bar = on[0];
    for (int k = 0; k < b; k++) {
        if (!on[k]) continue;
        if (p->variant == BG_PLAKOTO && k > 0 && p->pin[!s][bg_opp(p->variant, k)] && on[k] == 1) continue;
        if (bar && k > 0) break;                /* the bar first */
        int d = b - k;
        if (d1 != d2) {
            if (d == d1 || d == d2) return 1;
            if (d == d1 + d2 && bar <= 1 && (open_for(p, s, k + d1) || open_for(p, s, k + d2))) return 1;
        } else {
            int steps = bar >= 2 ? 1 : 4;
            for (int m = 1, x = k; m <= steps; m++) {
                x += d1;
                if (x == b) return 1;
                if (x > b || !open_for(p, s, x)) break;
            }
        }
    }
    return 0;
}

/* s's blots the other can reach, its 36 throws: the worst one each throw can hit, in eval units, times 36 */
static int exposure(const bpos *p, int s, int light)
{
    int on[26], blot[24], loss[24], nb = 0, entry = 0;
    theirs(p, s, on);
    for (int i = 19; i <= 24; i++) entry += p->n[!s][bg_opp(p->variant, i)] >= 2;   /* their home board, for us */
    for (int b = 1; b <= 24; b++) {
        if (p->n[s][b] != 1) continue;
        if (p->variant == BG_PLAKOTO && (p->pin[s][b] || p->pin[!s][bg_opp(p->variant, b)])) continue;
        int any = 0;
        for (int k = 0; k < b && !any; k++) any = on[k] > 0;
        if (!any) continue;
        int l;
        if (p->variant == BG_PLAKOTO)           /* pinned; the mother: the game lost */
            l = b == 24 && !p->n[!s][24] ? 3000 : light ? 40 : 60 + 3 * b;
        else
            l = 6 * (25 - b) + 40 + (light ? 0 : 14 * entry);
        blot[nb] = b;
        loss[nb++] = l;
    }
    if (!nb) return 0;
    int sum = 0;
    for (int d1 = 1; d1 <= 6; d1++)
        for (int d2 = d1; d2 <= 6; d2++) {
            int worst = 0;
            for (int i = 0; i < nb; i++)
                if (loss[i] > worst && hits(p, s, on, blot[i], d1, d2)) worst = loss[i];
            sum += (d1 == d2 ? 1 : 2) * worst;
        }
    return sum;
}

static const int POINT_W[25] = {0, 4, 10, 16, 24, 30, 26, 20, 10, 4, 4, 4, 2, 0, 0, 0, 0, 0, 10, 16, 22, 26, 20, 14,
                                12};

/* backgammon: s's points, primes, the other on the bar, piles */
static int shape_bg(const bpos *p, int s)
{
    int v = 0, on[26];
    theirs(p, s, on);
    for (int i = 1; i <= 24; i++) {
        if (p->n[s][i] >= 2) v += POINT_W[i];
        if (p->n[s][i] > 4) v -= 5 * (p->n[s][i] - 4);
    }
    for (int i = 1, run = 0; i <= 25; i++) {    /* primes: the other's checkers behind them (below, in s's numbers) */
        if (i <= 24 && p->n[s][i] >= 2) { run++; continue; }
        if (run >= 3) {
            int behind = 0;
            for (int k = 0; k < i - run; k++) behind += on[k];
            if (behind) v += (run - 2) * (run - 2) * 12 + (run == 6 ? 60 : 0);
        }
        run = 0;
    }
    v += p->n[!s][BG_BAR] * (12 + 9 * made_home(p, s));
    return v;
}

/* plakoto: the pins s holds and s's points */
static int shape_pk(const bpos *p, int s)
{
    int v = 0;
    for (int j = 1; j <= 24; j++) {
        if (p->pin[!s][j]) v += j == 24 && p->n[!s][24] == 1 ? 900 : 50 + 3 * j;   /* their mother: nearly won */
        if (p->n[s][j] >= 2) v += 6;
    }
    return v;
}

/* fevga, long nardy: the points s holds in front of the other's checkers (along its way: its 24 down to 1) */
static int shape_fv(const bpos *p, int s)
{
    int v = 0, run = 0, behind = 0;
    for (int j = 24; j >= 0; j--) {             /* the other's numbering */
        int held = j >= 1 && p->n[s][bg_opp(p->variant, j)] > 0;
        if (held) { run++; continue; }
        if (run >= 2 && behind) v += (run > 6 ? 6 : run) * (run > 6 ? 6 : run) * 4;
        else if (run == 1 && behind) v += 3;
        run = 0;
        if (j >= 1) behind += p->n[!s][j];
    }
    for (int i = 1; i <= 23; i++)               /* spread out: more points held, room to land; no piles (the head
                                                 * aside: it empties by itself) */
        if (p->n[s][i]) v += 4 - (p->n[s][i] > 3 ? 3 * (p->n[s][i] - 3) : 0);
    return v;
}

/* the position for player me (who just moved; the other throws next), in eval units (about 8 a pip) */
static int weigh(const bpos *p, int me, int light)
{
    int op = !me, pm = bg_pips(p, me), po = bg_pips(p, op), v;
    if (p->variant == BG_FEVGA || p->variant == BG_NARDY) {
        v = 8 * (po - pm);
        v += (shape_fv(p, me) - shape_fv(p, op)) / (light ? 2 : 1);   /* all the same pips: the shape decides */
        return v;
    }
    int on[26], back = 0, front = 25;           /* contact: one of ours still behind one of theirs */
    theirs(p, me, on);
    for (int i = 25; i >= 1 && !back; i--) if (p->n[me][i]) back = i;
    for (int i = 0; i <= 24 && front == 25; i++) if (on[i]) front = i;
    if (back <= front) {                        /* a race: the pips, the checkers left */
        int lm = 15 - p->n[me][BG_OFF], lo = 15 - p->n[op][BG_OFF];
        return 10 * (po - pm) + 6 * (lo - lm) + (p->variant == BG_PLAKOTO ? shape_pk(p, me) - shape_pk(p, op) : 0);
    }
    v = 6 * (po - pm) - exposure(p, me, light) / 36 + exposure(p, op, light) / 144;
    if (light) return v + (p->variant == BG_PLAKOTO ? shape_pk(p, me) - shape_pk(p, op) : 0);
    if (p->variant == BG_PLAKOTO) return v + shape_pk(p, me) - shape_pk(p, op);
    return v + shape_bg(p, me) - shape_bg(p, op);
}

/* a way's score for the mover: its end, or the game won */
static int score_way(const bpos *root, const way *w, int light)
{
    bpos q;
    int end = finish(root, w, &q);
    if (end == 1) return WIN_SCORE;
    if (end == 2) return 0;
    if (end == 3) return -WIN_SCORE;
    return weigh(&w->end, root->side, light);
}

/* ---- one throw further (hard) ------------------------------------------------------------------------------------ */
/* what the other can make of the position after our way: its 21 throws, its best answer to each (its own view),
 * averaged; *ok 0: the budget ran out first */
static int reply(ways *t, const bpos *after, long cap, double until, int *ok)
{
    long sum = 0;
    for (int d1 = 1; d1 <= 6; d1++)
        for (int d2 = d1; d2 <= 6; d2++) {
            if (ai_cancelled() || (cap && t->work > cap) || ai_now_ms() > until) { *ok = 0; return 0; }
            bpos q = *after;
            q.side = !after->side;
            q.phase = BG_MOVE;
            q.used = 0;
            q.head = 0;
            q.die[0] = (signed char)d1;
            q.die[1] = (signed char)d2;
            q.nd = 2;
            if (d1 == d2) {
                q.die[2] = q.die[3] = (signed char)d1;
                q.nd = 4;
            }
            if (q.throws[(int)q.side] < 2) q.throws[(int)q.side]++;
            int best;
            if (!all_ways(t, &q)) best = weigh(&q, q.side, 0);   /* no move: the turn comes back to us */
            else {
                best = -WIN_SCORE * 2;
                for (int i = 0; i < t->n; i++) {
                    int s = score_way(&q, &t->w[i], 0);
                    if (s > best) best = s;
                    if (best == WIN_SCORE) break;
                }
            }
            sum += (d1 == d2 ? 1 : 2) * (long)best;
        }
    *ok = 1;
    return (int)(-sum / 36);
}

/* the positions the ways lead to (the tests: the same as playing the rules' bg_legal() moves to the end of the
 * turn, every way); -1: no memory */
int bgb_ends(const bpos *p, bpos *out, int cap)
{
    ways *t = calloc(1, sizeof *t);
    way *w = malloc(sizeof(way) * LEAVES);
    if (!t || !w) { free(t); free(w); return -1; }
    t->w = w;
    int n = all_ways(t, p), k = 0;
    for (int i = 0; i < n && k < cap; i++) out[k++] = w[i].end;
    free(t);
    free(w);
    return k;
}

/* ---- the choice -------------------------------------------------------------------------------------------------- */
static int by_score(const void *a, const void *b)
{
    int x = ((const way *)a)->score, y = ((const way *)b)->score;
    return (y > x) - (y < x);
}

/* the first move of the best way to play p's dice (p: the moves phase, its mover), at a level; 0: none */
int bgb_choose(const bpos *p, int level, uint64_t seed, bmove *first, ai_stats *stats)
{
    double t0 = ai_now_ms();
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase != BG_MOVE) return 0;
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ways *t = calloc(1, sizeof *t);
    way *mine = malloc(sizeof(way) * LEAVES), *theirs_w = malloc(sizeof(way) * LEAVES);
    bmove l[BG_MAXMOVES];
    int nl = bg_legal(p, l), found = 0;
    if (!t || !mine || !theirs_w || !nl) {
        free(t); free(mine); free(theirs_w);
        if (nl) *first = l[0];
        return nl > 0;
    }
    ai_rng r;
    ai_seed(&r, ai_mix(seed, pos_key(p, p->nd)));
    t->w = mine;
    int n = all_ways(t, p);
    for (int i = 0; i < n; i++) {
        mine[i].score = score_way(p, &mine[i], level == AI_EASY);
        if (level == AI_EASY) mine[i].score += ai_rand_n(&r, opposite(p) ? 90 : 40);
        else if (level == AI_MEDIUM) mine[i].score += ai_rand_n(&r, 12);
    }
    qsort(mine, (size_t)n, sizeof *mine, by_score);
    int pick = 0;
    if (level == AI_HARD && n > 1 && mine[0].score < WIN_SCORE) {
        t->w = theirs_w;                        /* the other's ways from here on */
        long cap = 300000;                      /* positions; and 0.5 s at most (a slow phone) */
        int best = 0, bs = 0, done = 0;
        for (int i = 0; i < n && i < 8; i++) {
            if (i && mine[i].score < mine[0].score - 400) break;   /* far behind the best: not worth a look */
            int ok, s = reply(t, &mine[i].end, cap, t0 + 500, &ok);
            if (!ok) break;
            if (!done || s > bs) best = i, bs = s;
            done++;
        }
        pick = best;
    }
    t->w = mine;
    if (n) {
        *first = mine[pick].seq[0];
        found = 1;
    }
    bmove m;                                    /* the notation's move: the same checker, the rules' die */
    if (!found || !bg_find(p, first->from, first->to, &m)) *first = l[0];
    if (stats) {
        stats->nodes = t->work;
        stats->score = n ? mine[pick].score : 0;
        stats->ms = ai_now_ms() - t0;
    }
    free(t); free(mine); free(theirs_w);
    return 1;
}

static void place(char *s, int x)
{
    if (x == BG_BAR) strcpy(s, "bar");
    else if (x == BG_OFF) strcpy(s, "off");
    else sprintf(s, "%d", x);
}

/* "13/8", "bar/22", "6/off", a hit "13/8*" */
void bgb_notation(bmove m, char *out)
{
    char a[8], b[8];
    place(a, m.from);
    place(b, m.to);
    sprintf(out, "%s/%s%s", a, b, m.hit ? "*" : "");
}

static int think(int seat, int level, char *move, int cap)
{
    bpos p = *bg_pos(backgammon_live());        /* the copy, at entry: nothing else is read */
    if (p.phase == BG_OVER || p.side != seat || cap < 12) return 0;
    if (p.phase != BG_MOVE) {                   /* the dice are the game's: the bot only throws them */
        strcpy(move, "roll");
        return 1;
    }
    bmove m;
    if (!bgb_choose(&p, level, ai_session_seed(), &m, NULL) || ai_cancelled()) return 0;
    bgb_notation(m, move);
    return 1;
}

static const bot backgammon_bot = {.think = think, .play = backgammon_show};
REGISTER_BOT(backgammon, &backgammon_bot)
