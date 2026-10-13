/*
 * Secret Code's bot (bots.h): the code-breaker. The game is played against the app, which makes the code at random
 * (sc_make_code: the code-maker's part, already a fair random code), so the bot's seat is the breaker's: it watches
 * the guesses and their key pegs, never the code.
 *   easy:   a guess that agrees with the last answer and with only some of the earlier ones (a beginner who forgets
 *           what a row told them): it gets there, a few rows later than it could.
 *   medium: a guess at random among the codes that agree with every answer so far (the classic careful player).
 *   hard:   the guess (any code, the ones that could be right preferred) that leaves the fewest codes on average
 *           after its answer (the expected size of the parts), from a fixed good first guess; a few hundred thousand
 *           key-peg counts a guess at most, so the big variant samples the guesses it tries.
 * Runs on the worker thread: it copies the game at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its move is one guess in the game's notation ("RRGB"), played by secretcode_show() (the pegs, then the check).
 */
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/secretcode.h"

#define MAXCODES 32768                          /* 8 colours ^ 5 pegs */
#define HARD_WORK 6000000L                      /* key-peg counts a hard guess may spend (about 30 ms on a PC) */
#define HARD_SAMPLE 2000                        /* the candidates the parts are counted over, at most */

typedef struct {
    int pegs, colours, total;                   /* total: colours ^ pegs */
    int8_t (*code)[SC_MAXPEGS];                 /* every code, by index */
    uint8_t (*count)[SC_MAXCOLOURS];            /* each code's pegs of each colour */
} codes;

static int make_codes(codes *c, int pegs, int colours)
{
    c->pegs = pegs;
    c->colours = colours;
    c->total = 1;
    for (int i = 0; i < pegs; i++) c->total *= colours;
    c->code = malloc(sizeof c->code[0] * (size_t)c->total);
    c->count = calloc((size_t)c->total, sizeof c->count[0]);
    if (!c->code || !c->count) { free(c->code); free(c->count); return 0; }
    for (int k = 0; k < c->total; k++)
        for (int i = 0, x = k; i < pegs; i++, x /= colours) {
            c->code[k][i] = (int8_t)(x % colours);
            c->count[k][x % colours]++;
        }
    return 1;
}

/* the answer to a guess (code index g) for a code (index s), as one number: black * 8 + white */
static inline int answer(const codes *c, int s, int g)
{
    int b = 0, both = 0;
    for (int i = 0; i < c->pegs; i++) b += c->code[s][i] == c->code[g][i];
    for (int k = 0; k < c->colours; k++) {
        int x = c->count[s][k], y = c->count[g][k];
        both += x < y ? x : y;
    }
    return b * 8 + both - b;
}

static int index_of(const codes *c, const int8_t *pegs)
{
    int k = 0;
    for (int i = c->pegs - 1; i >= 0; i--) k = k * c->colours + pegs[i];
    return k;
}

static int no_repeat(const codes *c, int k)
{
    for (int x = 0; x < c->colours; x++)
        if (c->count[k][x] > 1) return 0;
    return 1;
}

/* a good first guess for each size (two colours twice, then new ones: the usual openings) */
static void opening(const sc_variant *v, int8_t *g)
{
    static const int8_t O4[SC_MAXPEGS] = {0, 0, 1, 1}, O4U[SC_MAXPEGS] = {0, 1, 2, 3}, O5[SC_MAXPEGS] = {0, 0, 1, 2, 3};
    memcpy(g, v->pegs == 5 ? O5 : v->repeats ? O4 : O4U, SC_MAXPEGS);
}

/* the guess for the breaker of g at a level, in out (pegs); 0: the game is not being played */
int scb_guess(const sc_game *g, int level, uint64_t seed, int8_t *out)
{
    const sc_variant *v = &g->v;
    if (g->n >= v->rows || (g->n && g->black[g->n - 1] == v->pegs)) return 0;   /* solved or failed (no code read) */
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 7919u + (uint64_t)v->pegs));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (level == AI_HARD && g->n == 0) { opening(v, out); return 1; }
    codes c;
    if (!make_codes(&c, v->pegs, v->colours)) {     /* no memory: any guess */
        for (int i = 0; i < v->pegs; i++) out[i] = (int8_t)ai_rand_n(&r, v->colours);
        return 1;
    }
    /* the rows the breaker keeps in mind: all of them, or (easy) the last one and about half the others */
    int keep[SC_MAXROWS], nk = 0;
    for (int row = 0; row < g->n; row++)
        if (level != AI_EASY || row == g->n - 1 || ai_rand_n(&r, 100) < 45) keep[nk++] = row;
    int rowg[SC_MAXROWS], rowa[SC_MAXROWS];
    for (int i = 0; i < nk; i++) {
        rowg[i] = index_of(&c, g->guess[keep[i]]);
        rowa[i] = g->black[keep[i]] * 8 + g->white[keep[i]];
    }
    int *S = malloc(sizeof(int) * (size_t)c.total), ns = 0;
    if (!S) { free(c.code); free(c.count); opening(v, out); return 1; }
    for (int k = 0; k < c.total; k++) {
        if (!v->repeats && !no_repeat(&c, k)) continue;   /* the code has no colour twice: everyone knows */
        int ok = 1;
        for (int i = 0; i < nk && ok; i++) ok = answer(&c, k, rowg[i]) == rowa[i];
        if (ok) S[ns++] = k;
    }
    int pick = -1;
    if (!ns) {                                  /* (never: the app answers truly) any code */
        pick = ai_rand_n(&r, c.total);
    } else if (level != AI_HARD || ns <= 2) {
        pick = S[ai_rand_n(&r, ns)];
    } else {
        /* the parts are counted over the candidates (a sample of them when they are many); the guesses tried: every
         * code when there is room, else the candidates first, then codes drawn at random */
        int m = ns < HARD_SAMPLE ? ns : HARD_SAMPLE;
        long room = HARD_WORK / m;
        int np = (long)c.total <= room ? c.total : (int)room;
        int *pool = malloc(sizeof(int) * (size_t)np), *T = malloc(sizeof(int) * (size_t)m);
        uint8_t *in = calloc((size_t)c.total, 1);
        if (!pool || !in || !T) {
            pick = S[0];
        } else {
            for (int i = 0; i < ns; i++) in[S[i]] = 1;
            for (int i = 0; i < m; i++) T[i] = m == ns ? S[i] : S[ai_rand_n(&r, ns)];
            if (np == c.total) {
                for (int k = 0; k < np; k++) pool[k] = k;
            } else {
                for (int i = 0; i < np; i++)
                    pool[i] = i < ns && i < np / 2 ? S[ns <= np / 2 ? i : ai_rand_n(&r, ns)] : ai_rand_n(&r, c.total);
            }
            long best = -1;
            int parts[64];
            for (int i = 0; i < np && !ai_cancelled(); i++) {
                int q = pool[i];
                memset(parts, 0, sizeof parts);
                for (int j = 0; j < m; j++) parts[answer(&c, T[j], q)]++;
                long cost = 0;
                for (int a = 0; a < 64; a++) cost += (long)parts[a] * parts[a];
                cost = 4 * cost - 4 * in[q];    /* a candidate may be the code itself: that part is solved */
                if (best < 0 || cost < best || (cost == best && in[q] && !in[pick])) best = cost, pick = q;
            }
            if (pick < 0) pick = S[0];
        }
        free(pool);
        free(T);
        free(in);
    }
    memcpy(out, c.code[pick], (size_t)v->pegs);
    free(S);
    free(c.code);
    free(c.count);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    sc_game g = *secretcode_live();             /* the copy, at entry: nothing else is read */
    memset(g.code, -1, sizeof g.code);          /* the breaker never sees the code: the copy forgets it */
    if (seat != 0 || cap < g.v.pegs + 1) return 0;
    int8_t guess[SC_MAXPEGS];
    if (!scb_guess(&g, level, ai_session_seed(), guess) || ai_cancelled()) return 0;
    for (int i = 0; i < g.v.pegs; i++) move[i] = sc_letters[guess[i]];
    move[g.v.pegs] = 0;
    return 1;
}

static const bot secretcode_bot = {.think = think, .play = secretcode_show};
REGISTER_BOT(secretcode, &secretcode_bot)
