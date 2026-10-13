/*
 * Mystery Face's bot (bots.h). The device answers every question truly about the face a player picked, so the bot's
 * part is to pick its face, ask, put down the faces the answer rules out, and guess when it should. It never sees
 * the other player's face: its copy forgets it, and the faces it keeps in mind are those still standing on its own
 * board (what a player sees), less those this turn's answer rules out.
 *   pick:   a face at random.
 *   easy:   asks any question that splits its faces (not the best one), guesses half the time when two are left, and
 *           now and then forgets to put down a face the answer ruled out (so it may guess wrong later).
 *   medium: asks a good question (one that cuts its faces near half, not always the nearest), and guesses when one
 *           is left, or when the other player has one left (they would win next turn: a guess is the last chance).
 *   hard:   weighs the race: its chance to win after each question (its faces as each answer would leave them, the
 *           real questions splitting them on later turns too, the other player halving theirs) against a guess now,
 *           and plays the best: an even split when ahead, a bold one when behind, a guess when waiting loses more.
 * Runs on the worker thread: it copies the game at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation, a word or a few: "p7 e" (picked), "q3" (asked), "f2 f9 f14 e" (the faces put
 * down, then the turn ended; more than fit in a move go in the next one), "g11" (a guess); played by
 * mysteryface_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/mysteryface.h"

#include <stdlib.h>

static int popcount(uint32_t x) { return __builtin_popcount(x); }

/* ---- hard: the race --------------------------------------------------------------------------------------------
 * The bot's faces as they are (the real questions, and how each splits them), the other player's as a count, playing
 * as most people do: questions that halve their faces, a guess only with one face left, or when the bot has one left.
 * win(S, b): the bot to move with the faces S, the other player b faces: its chance to win, with its best question
 * (an even split when ahead, a bold one when behind) or a guess. Memo: a table of (S, b). */
#define RACE_BITS 16
typedef struct { uint64_t key; double v; } race_slot;
typedef struct { race_slot *t; uint32_t yes[MF_QUESTIONS]; long work; } racing;

static double theirs(racing *R, int b, uint32_t S);

static double win(racing *R, uint32_t S, int b)
{
    int a = popcount(S);
    if (a <= 1) return 1;
    if (b < 1) b = 1;
    uint64_t key = (uint64_t)S << 5 | (uint64_t)b, h = ai_mix(key, 0x7ace) & ((1u << RACE_BITS) - 1);
    if (R->t[h].key == key + 1) return R->t[h].v;
    double best = 1.0 / a;                      /* a guess */
    for (int q = 0; q < MF_QUESTIONS && R->work++ < 4000000; q++) {
        uint32_t y = S & R->yes[q], n = S & ~R->yes[q];
        if (!y || !n) continue;
        double v = (double)popcount(y) / a * (1 - theirs(R, b, y)) + (double)popcount(n) / a * (1 - theirs(R, b, n));
        if (v > best) best = v;
    }
    R->t[h].key = key + 1;
    R->t[h].v = best;
    return best;
}

/* the other player to move with b faces, the bot with S: the other player's chance to win */
static double theirs(racing *R, int b, uint32_t S)
{
    if (b <= 1) return 1;
    if (popcount(S) <= 1) return 1.0 / b;       /* the bot wins next turn: they guess */
    int k = b / 2;
    return (double)k / b * (1 - win(R, S, k)) + (double)(b - k) / b * (1 - win(R, S, b - k));
}

/* the move for the seat to play in p (its view: the other face unknown), in the notation; 0: nothing to do */
int mfb_move(const mfpos *p, int level, uint64_t seed, char *move, int cap)
{
    int me = p->turn, other = !me;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->down[me] * 64 + (uint64_t)p->questions[me] * 2 + (p->asked >= 0)));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (cap < 8) return 0;
    if (p->phase == MF_PICK) {
        if (p->secret[me] >= 0) { snprintf(move, (size_t)cap, "e"); return 1; }
        snprintf(move, (size_t)cap, "p%d e", ai_rand_n(&r, MF_FACES));
        return 1;
    }
    if (p->phase != MF_PLAY) return 0;
    uint32_t up = 0;                            /* the faces it keeps in mind: standing on its board */
    for (int f = 0; f < MF_FACES; f++)
        if (!(p->down[me] >> f & 1)) up |= 1u << f;

    if (p->asked >= 0) {                        /* answered: the faces ruled out go down, then the turn ends */
        int n = 0;
        move[0] = 0;
        for (int f = 0; f < MF_FACES; f++) {
            if (!(up >> f & 1) || mf_has(f, p->asked) == p->answer) continue;
            /* easy forgets one in twelve (the same ones each time it looks again this turn) */
            if (level == AI_EASY && (ai_mix((uint64_t)p->questions[me] * 97 + (uint64_t)p->secret[me], (uint64_t)f)
                                     % 12) == 0)
                continue;
            if ((int)strlen(move) + 5 + 2 >= cap) return 1;   /* the rest in the next move */
            snprintf(move + strlen(move), (size_t)cap - strlen(move), "%sf%d", n++ ? " " : "", f);
        }
        snprintf(move + strlen(move), (size_t)cap - strlen(move), "%se", n ? " " : "");
        return 1;
    }

    int n = popcount(up), m = mf_up(p, other);
    int faces[MF_FACES], nf = 0;
    for (int f = 0; f < MF_FACES; f++)
        if (up >> f & 1) faces[nf++] = f;
    /* the questions that split the faces kept in mind: how many say yes */
    int yes[MF_QUESTIONS], useful[MF_QUESTIONS], nu = 0;
    for (int q = 0; q < MF_QUESTIONS; q++) {
        yes[q] = 0;
        for (int i = 0; i < nf; i++) yes[q] += mf_has(faces[i], q);
        if (yes[q] > 0 && yes[q] < n) useful[nu++] = q;
    }
    int guess = n == 1 || (nu == 0 && n >= 1);  /* one left, or nothing left to ask */
    int ask = -1;
    if (!guess && level == AI_EASY) {
        guess = m <= 1 || (n == 2 && ai_rand_n(&r, 2));
        ask = useful[ai_rand_n(&r, nu)];
    } else if (!guess && level == AI_MEDIUM) {
        guess = m <= 1;
        int best = MF_FACES + 1, ties = 0, d[MF_QUESTIONS];
        for (int i = 0; i < nu; i++) {          /* a good question: its split near the best one's (by a sixth) */
            int q = useful[i];
            d[i] = 2 * yes[q] - n < 0 ? n - 2 * yes[q] : 2 * yes[q] - n;
            if (d[i] < best) best = d[i];
        }
        for (int i = 0; i < nu; i++)
            if (6 * (d[i] - best) <= n && ai_rand_n(&r, ++ties) == 0) ask = useful[i];
    } else if (!guess) {
        racing R = {calloc((size_t)1 << RACE_BITS, sizeof(race_slot)), {0}, 0};
        for (int q = 0; q < MF_QUESTIONS; q++)
            for (int f = 0; f < MF_FACES; f++) R.yes[q] |= (uint32_t)mf_has(f, q) << f;
        double best = 1.0 / n;                  /* a guess now */
        int ties = 0;
        for (int i = 0; i < nu; i++) {
            int q = useful[i];
            uint32_t y = up & R.yes[q], no = up & ~R.yes[q];
            double v = R.t ? (double)popcount(y) / n * (1 - theirs(&R, m, y)) +
                             (double)popcount(no) / n * (1 - theirs(&R, m, no))
                           : 1 - (double)abs(2 * yes[q] - n) / n;
            if (v > best + 1e-9) best = v, ask = q, ties = 1;
            else if (v > best - 1e-9 && ask >= 0 && ai_rand_n(&r, ++ties) == 0) ask = q;
        }
        guess = ask < 0;
        free(R.t);
    }
    if (guess && n >= 1) {
        snprintf(move, (size_t)cap, "g%d", faces[ai_rand_n(&r, nf)]);
        return 1;
    }
    if (ask < 0) ask = nu ? useful[0] : ai_rand_n(&r, MF_QUESTIONS);   /* (every face down: a question still) */
    snprintf(move, (size_t)cap, "q%d", ask);
    return 1;
}

/* the seat's view: the other player's face forgotten */
void mfb_view(const mfpos *live, int seat, mfpos *view)
{
    *view = *live;
    view->secret[!seat] = -1;
}

static int think(int seat, int level, char *move, int cap)
{
    mfpos all = mysteryface_live()->pos, p;     /* the copy, at entry: nothing else is read */
    mfb_view(&all, seat, &p);                   /* then only what the seat may see */
    if (p.phase == MF_OVER || p.turn != seat) return 0;
    return mfb_move(&p, level, ai_session_seed(), move, cap) && !ai_cancelled();
}

static const bot mysteryface_bot = {.think = think, .play = mysteryface_show};
REGISTER_BOT(mysteryface, &mysteryface_bot)
