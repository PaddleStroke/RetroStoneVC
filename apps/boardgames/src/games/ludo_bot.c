/*
 * Ludo's bot (bots.h), both rules (Ludo, the petits chevaux) and two to four players. The throw is a move: "roll",
 * played by ludo_show() with the game's own die, as a tap throws it; after a throw the bot chooses the token.
 * The player's sense, a score for each token that can move: what it catches (and what that costs its owner), coming
 * out of the yard, getting home or under shelter (Ludo's safe squares, the home column, the stairs), the risk on the
 * square it goes to (the chance that a player throwing before our next turn reaches it: 1..6 behind it, 7..12 after a
 * 6, a token coming out of its yard; petits chevaux: no horse passes another), the risk it leaves, and the way: the
 * petits chevaux reward the token ahead (the stairs take an exact number a step: the sooner there, the better; tuned
 * by self-play, as the other weights).
 *   easy    that sense, blurred: now and then a choice that is merely good, sometimes one at random;
 *   medium  that sense, sharp;
 *   hard    each choice played out to the end of the game many times by the medium player (every choice on the same
 *           throws, so the luck is the same for all of them): the choice that wins most often, the bot's own win the
 *           only one that counts.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "roll", or the token's letter "a".."d".
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/ludo.h"

/* ---- the position: ludo_throw and ludo_move on a lpos alone (no history) --------------------------------------- */
static void next_player(lpos *p)
{
    for (int s = ludo_seat(p->turn) + 1;; s++)  /* the next seat round the table that has a player */
        if (ludo_seat(s & 3) < p->players) {    /* (the seats and the players: the same exchange 1 <-> 2) */
            p->turn = (uint8_t)ludo_seat(s & 3);
            break;
        }
    p->sixes = 0;
    p->phase = LUDO_THROW;
}

void ldb_throw(lpos *p, int value)
{
    p->die = (uint8_t)value;
    p->throws++;
    if (p->variant == LUDO_CLASSIC && value == 6 && ++p->sixes == 3) {
        next_player(p);
        return;
    }
    p->phase = LUDO_MOVE;
    if (ludo_movable(p)) return;
    if (value == 6) p->phase = LUDO_THROW;
    else next_player(p);
}

void ldb_move(lpos *p, int token)
{
    int to = ludo_dest(p, token), hits = ludo_hits(p, token), me = p->turn;
    if (to < 0) return;
    p->at[me][token] = (int8_t)to;
    for (int i = 0; i < LUDO_PLAYERS * LUDO_TOKENS; i++)
        if (hits >> i & 1) p->at[i / LUDO_TOKENS][i % LUDO_TOKENS] = -1;
    if (ludo_home(p, me) == LUDO_TOKENS) {
        p->winner = (int8_t)me;
        p->phase = LUDO_OVER;
    } else if (p->die == 6) {
        p->phase = LUDO_THROW;
    } else {
        next_player(p);
    }
}

/* ---- what a token is worth, and its risk ------------------------------------------------------------------------- */
static int worth(const lpos *p, int at)         /* in squares of the way: coming out is worth 16, home 12 more */
{
    if (at < 0) return 0;
    return 16 + at + (at >= ludo_goal(p->variant) ? 12 : 0);
}

/* where nobody may be caught: the yard, the home column (or the stairs), Ludo's safe squares */
static int sheltered(const lpos *p, int pl, int at)
{
    if (at < 0 || at >= LUDO_COLUMN) return 1;
    return p->variant == LUDO_CLASSIC && ludo_safe(ludo_square(pl, at));
}

/* petits chevaux: a horse on the track strictly between square a and d squares further */
static int blocked(const lpos *p, int a, int d)
{
    for (int o = 0; o < p->players; o++)
        for (int t = 0; t < LUDO_TOKENS; t++) {
            int s = ludo_square(o, p->at[o][t]);
            if (s < 0) continue;
            int k = (s - a + LUDO_TRACK) % LUDO_TRACK;
            if (k > 0 && k < d) return 1;
        }
    return 0;
}

/* the chance (in 36ths) that player o, throwing once (and again after a 6), reaches the track square sq */
static int reach(const lpos *p, int o, int sq)
{
    int ways = 0, yard = 0, dist1 = 0, dist2 = 0;
    for (int t = 0; t < LUDO_TOKENS; t++) {
        int at = p->at[o][t];
        if (at < 0) { yard = 1; continue; }
        int s = ludo_square(o, at);
        if (s < 0) continue;
        int d = (sq - s + LUDO_TRACK) % LUDO_TRACK;
        if (d < 1 || d > 12 || at + d >= LUDO_COLUMN) continue;   /* past its own way round: into its column */
        if (p->variant == LUDO_CHEVAUX && (at + d > LUDO_FOOT || blocked(p, s, d))) continue;
        if (d <= 6) dist1 |= 1 << d;
        else dist2 |= 1 << (d - 6);
    }
    if (yard) {
        int d = (sq - ludo_square(o, 0) + LUDO_TRACK) % LUDO_TRACK;
        if (d == 0 && p->variant == LUDO_CHEVAUX) dist1 |= 1 << 6;   /* coming out takes the horse on its start */
        else if (d >= 1 && d <= 6) dist2 |= 1 << d;   /* a 6 to come out, then the number */
    }
    for (int d = 1; d <= 6; d++) ways += dist1 >> d & 1 ? 6 : dist2 >> d & 1 ? 1 : 0;
    return ways > 36 ? 36 : ways;
}

static int risk(const lpos *p, int me, int at)  /* the others' chances on the square, added (36ths) */
{
    int sq = ludo_square(me, at), r = 0;
    for (int o = 0; o < p->players; o++)
        if (o != me) r += reach(p, o, sq);
    return r;
}

/* ---- the player's sense ------------------------------------------------------------------------------------------ */
/* the weights, by the rules (Ludo, the petits chevaux): a capture, coming out, home, under shelter, out of it, the
 * risk taken (worth * risk / this), the risk left (the same; 0: not counted), the way (the square reached * way / 8) */
static const struct { int take, out, home, in, leave, risk, flee, way; } W[LUDO_VARIANTS] = {
    {30, 24, 20, 10, 6, 36, 48, 1},
    {30, 40, 20, 10, 6, 36, 0, 8},
};

static int score(const lpos *p, int t)
{
    int me = p->turn, at = p->at[me][t], to = ludo_dest(p, t), s = 0, v = p->variant, hits = ludo_hits(p, t);
    for (int i = 0; i < LUDO_PLAYERS * LUDO_TOKENS; i++)
        if (hits >> i & 1) s += W[v].take + worth(p, p->at[i / LUDO_TOKENS][i % LUDO_TOKENS]);
    if (at < 0) s += W[v].out;
    if (to >= ludo_goal(v)) s += W[v].home;
    int was = sheltered(p, me, at), will = sheltered(p, me, to);
    if (!was && will) s += W[v].in;
    if (was && !will && at >= 0) s -= W[v].leave;
    if (!will) s -= risk(p, me, to) * (worth(p, to) + 4) / W[v].risk;
    if (!was && at >= 0 && W[v].flee) s += risk(p, me, at) * (worth(p, at) + 4) / W[v].flee;
    return 4 * s + (to > 0 ? to : 0) * W[v].way / 8;
}

/* the best token by that sense, the scores blurred by up to `noise` (32: 8 squares) */
static int sense(const lpos *p, ai_rng *r, int noise)
{
    int best = -1, bs = 0, m = ludo_movable(p);
    for (int t = 0; t < LUDO_TOKENS; t++) {
        if (!(m >> t & 1)) continue;
        int s = score(p, t) + (noise ? ai_rand_n(r, noise) : 0);
        if (best < 0 || s > bs) best = t, bs = s;
    }
    return best;
}

/* ---- hard: the play-outs ----------------------------------------------------------------------------------------- */
/* a game played out by the medium player for everyone: 1 if me wins (a very long one: the furthest way ahead) */
static int playout(lpos *p, ai_rng *r, int me)
{
    for (int k = 0; k < 3000; k++) {
        if (p->winner >= 0) return p->winner == me;
        if (p->phase == LUDO_THROW) ldb_throw(p, 1 + ai_rand_n(r, 6));
        else ldb_move(p, sense(p, r, 0));
    }
    int best = 0, bv = -1;
    for (int o = 0; o < p->players; o++) {
        int v = 0;
        for (int t = 0; t < LUDO_TOKENS; t++) v += worth(p, p->at[o][t]);
        if (v > bv) bv = v, best = o;
    }
    return best == me;
}

/* Each choice played out `runs` times (fewer if the time runs out), every choice on the same throws. The medium
 * choice stands unless another wins clearly more often: paired games, the ones they split counted (McNemar's test,
 * z > 2), so the luck of a few hundred games never overturns the player's sense by chance. */
static int hard_move(const lpos *p, uint64_t seed, int runs, ai_stats *stats)
{
    int cand[LUDO_TOKENS], n = 0, m = ludo_movable(p), me = p->turn;
    for (int t = 0; t < LUDO_TOKENS; t++)
        if (m >> t & 1) cand[n++] = t;
    ai_rng pick;
    ai_seed(&pick, seed);
    int first = sense(p, &pick, 0), f = 0;
    if (n == 1) return cand[0];
    for (int i = 0; i < n; i++)
        if (cand[i] == first) f = i;
    double t0 = ai_now_ms();
    int better[LUDO_TOKENS] = {0}, worse[LUDO_TOKENS] = {0}, done = 0;
    for (; done < runs; done++) {
        if ((done & 7) == 0 && (ai_cancelled() || ai_now_ms() - t0 > 600)) break;
        uint64_t s = ai_rand(&pick) | (uint64_t)ai_rand(&pick) << 32;
        int won[LUDO_TOKENS];
        for (int i = 0; i < n; i++) {
            lpos q = *p;
            ldb_move(&q, cand[i]);
            ai_rng r;
            ai_seed(&r, s);
            won[i] = playout(&q, &r, me);
        }
        for (int i = 0; i < n; i++) {
            better[i] += won[i] && !won[f];
            worse[i] += !won[i] && won[f];
        }
    }
    int best = f;
    double bz = 2.0;
    for (int i = 0; i < n; i++) {
        int split = better[i] + worse[i];
        double z = split ? (better[i] - worse[i]) / __builtin_sqrt((double)split) : 0;
        if (i != f && z > bz) best = i, bz = z;
    }
    if (stats) {
        stats->nodes = (long)done * n;
        stats->score = (int)(bz * 100);
        stats->ms = ai_now_ms() - t0;
    }
    return cand[best];
}

/* the token to move for the position p (a throw made), at a level; -1: none */
int ldb_choose(const lpos *p, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase != LUDO_MOVE || !ludo_movable(p)) return -1;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, ai_mix((uint64_t)p->throws, (uint64_t)p->turn)));
    double t0 = ai_now_ms();
    int t;
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   t = sense(p, &r, ai_rand_n(&r, 5) ? 80 : 4000); break;   /* a fifth of the time: at random */
    case AI_MEDIUM: t = sense(p, &r, 0); break;
    default:        return hard_move(p, ai_rand(&r), p->variant == LUDO_CHEVAUX ? 150 : 300, stats);
    }
    if (stats) stats->ms = ai_now_ms() - t0;
    return t;
}

static int think(int seat, int level, char *move, int cap)
{
    lpos p = ludo_live()->pos;                  /* the copy, at entry: nothing else is read */
    if (p.phase == LUDO_OVER || p.turn != seat || cap < 5) return 0;
    if (p.phase == LUDO_THROW) {                /* the die is the game's: the bot only throws it */
        strcpy(move, "roll");
        return 1;
    }
    int t = ldb_choose(&p, level, ai_session_seed(), NULL);
    if (t < 0 || ai_cancelled()) return 0;
    move[0] = (char)('a' + t);
    move[1] = 0;
    return 1;
}

static const bot ludo_bot = {.think = think, .play = ludo_show};
REGISTER_BOT(ludo, &ludo_bot)
