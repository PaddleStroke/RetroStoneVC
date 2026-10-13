/*
 * Bataille corse's bot (bots.h). Turning cards has no choice in it; the game is in the slaps, so the bot plays its
 * turn (a card turned, or the pile gathered when a tribute went unpaid) and, when the centre shows a double (or a
 * sandwich in that variant) as its turn comes, slaps it after a reaction time of its level, as a hand would:
 *   easy:   sees four doubles in five, and slaps after 0.9 to 1.5 s;
 *   medium: sees eleven in twelve, after 0.6 to 0.95 s;
 *   hard:   sees them all, after 0.38 to 0.56 s.
 * The wait is part of think() (on the worker thread, polling the cancel), so a human who slaps first takes the pile:
 * the bot's slap then comes too late and slap_show() lets it go (a late slap costs nothing). It never slaps wrong.
 * It acts as the seat whose turn it is (the shell's actor: game.turn()); a double showing on a human's turn is the
 * humans' to slap (the shell knows no bot seat outside the turn).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "p" a card turned, "c" the pile gathered, "s1" player 1 slaps; played by
 * slap_show().
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L                 /* nanosleep */
#endif
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "ai.h"
#include "bots.h"
#include "games/slap.h"

static void nap(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
#endif
}

/* the reaction to a double at a level: 0 not seen, else the time to the slap (ms) */
static int reaction(int level, ai_rng *r)
{
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    static const int seen[AI_LEVELS] = {80, 92, 100}, lo[AI_LEVELS] = {900, 600, 380}, hi[AI_LEVELS] = {1500, 950, 560};
    if (ai_rand_n(r, 100) >= seen[level]) return 0;
    return lo[level] + ai_rand_n(r, hi[level] - lo[level] + 1);
}

static int think(int seat, int level, char *move, int cap)
{
    double t0 = ai_now_ms();
    slpos p = slap_live()->pos;                 /* the copy, at entry: nothing else is read */
    if (p.phase == SLP_OVER || p.turn != seat || cap < 4) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), (uint64_t)p.nc * 64 + (uint64_t)seat));
    int wait = slap_right(&p) ? reaction(level, &r) : 0;
    if (wait) {                                 /* the hand's way to the pile */
        while (slap_realtime() && ai_now_ms() - t0 < wait) {
            if (ai_cancelled()) return 0;
            nap(5);
        }
        snprintf(move, (size_t)cap, "s%d", seat);
    } else {
        snprintf(move, (size_t)cap, p.phase == SLP_CLAIM ? "c" : "p");
    }
    return !ai_cancelled();
}

static const bot slap_bot = {.think = think, .play = slap_show};
REGISTER_BOT(slap, &slap_bot)
