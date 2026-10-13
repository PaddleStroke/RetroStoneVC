/*
 * What the dice games' bot tests share (tests/ai_shutthebox_test.c, ai_yacht_test.c, ai_farkle_test.c...): the
 * bots' throws ("d??", "r?", "t??": each "?" a face the game throws) given faces from the session's seed, so every
 * game repeats; a game's winners as the harness's two sides; the checks.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_DICE_H
#define AI_DICE_H

#include "ai_selfplay.h"

/* m with each "?" a face 1..6 from the session's seed and a count of the game's throws (out: cap bytes) */
static inline void dice_fill(const char *m, char *out, int cap, uint64_t count)
{
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), count * 0x51ed27 + 0x9e37));
    int i = 0;
    for (; m[i] && i + 1 < cap; i++) out[i] = m[i] == '?' ? (char)('1' + ai_rand_n(&r, 6)) : m[i];
    out[i] = 0;
}

/* the winners (bit p) as the harness's sides: the even players (0), the odd ones (1), both (2: a draw) */
static inline int dice_side(unsigned winners)
{
    unsigned even = winners & 0x55u, odd = winners & 0xaau;
    return even && odd ? 2 : even ? 0 : odd ? 1 : 2;
}

static int dice_fails;
static inline void dice_check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    dice_fails += !ok;
}

/* a full run's games for each pairing: n, or DICE_GAMES from the environment (a finer measure) */
static inline int dice_games(int n)
{
    const char *e = getenv("DICE_GAMES");
    return e && atoi(e) > 0 ? atoi(e) : n;
}

static inline int dice_full(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--full")) return 1;
    return 0;
}

#endif
