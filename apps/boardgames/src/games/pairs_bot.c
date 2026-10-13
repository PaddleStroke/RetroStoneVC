/*
 * Pairs' bot (bots.h): a memory. It knows a card's picture only if the card has been turned up (by anyone) and it
 * still remembers it: its copy wipes the pictures of the cards never turned. How long a picture stays in mind is
 * the level; the card turned up this turn is always in view.
 *   easy:   remembers a card it saw a few actions ago, rarely one from several turns back (a child's memory);
 *   medium: forgets slowly (a grown-up's);
 *   hard:   remembers every card it has seen.
 * Then the same play for all: a pair it knows, it takes; else a card never turned (it may make a pair, and it shows
 * a new picture); the second card the match of the first if it knows it, else a card never turned; with none left,
 * a card it does not remember.
 * Runs on the worker thread: it copies the game at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "t5" a card turned up; played by pairs_show(). The two cards of a miss turn
 * back by themselves (actor() is -1 then); asked anyway, the bot says "h".
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/pairs.h"

#define UNKNOWN 255

/* the card the bot turns now in g (the seat to play's view: unseen pictures wiped), or PA_BACK; -1: nothing */
int prb_choose(const prgame *g, int level, uint64_t seed)
{
    const prpos *p = &g->pos;
    if (p->phase == PP_SHOW) return PA_BACK;
    if (p->phase != PP_PLAY) return -1;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 977 + p->turn));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    /* when each card was last turned up (actions ago), from the list of actions everyone saw */
    int age[PR_MAX];
    for (int c = 0; c < PR_MAX; c++) age[c] = -1;
    for (int i = 0; i < g->n; i++)
        if (PA_KIND(g->act[i]) == PA_TURN) age[PA_CARD(g->act[i])] = g->n - i;
    /* what it has in mind: a picture, or UNKNOWN (never seen, or forgotten) */
    double span = level == AI_EASY ? 7 : level == AI_MEDIUM ? 40 : 0;   /* actions: a memory's half life, about */
    uint8_t mind[PR_MAX];
    int first = p->up[0];
    for (int c = 0; c < p->n; c++) {
        mind[c] = UNKNOWN;
        if (p->found[c] != PR_NONE || !p->seen[c] || p->pic[c] == UNKNOWN) continue;
        int keep = c == first || span == 0 || ai_rand_f(&r) < pow(0.5, age[c] / span);
        if (keep) mind[c] = p->pic[c];
    }
    int unseen[PR_MAX], nu = 0, blank[PR_MAX], nb = 0;
    for (int c = 0; c < p->n; c++) {
        if (p->found[c] != PR_NONE || c == first) continue;
        if (!p->seen[c]) unseen[nu++] = c;
        else if (mind[c] == UNKNOWN) blank[nb++] = c;
    }
    if (first < 0) {                            /* the first card: a pair it knows */
        int pairs[PR_MAX], np = 0;
        for (int a = 0; a < p->n; a++)
            for (int b = a + 1; b < p->n; b++)
                if (mind[a] != UNKNOWN && mind[a] == mind[b]) pairs[np++] = a;
        if (np) return PA_TURN | pairs[ai_rand_n(&r, np)];
    } else {                                    /* the second card: the first one's match, if it knows where */
        int match[PR_MAX], nm = 0;
        for (int c = 0; c < p->n; c++)
            if (c != first && p->found[c] == PR_NONE && mind[c] != UNKNOWN && mind[c] == p->pic[first]) match[nm++] = c;
        if (nm) return PA_TURN | match[ai_rand_n(&r, nm)];
    }
    if (nu) return PA_TURN | unseen[ai_rand_n(&r, nu)];
    if (nb) return PA_TURN | blank[ai_rand_n(&r, nb)];
    for (int c = 0; c < p->n; c++)              /* (every card in mind, none matching: cannot be; any card left) */
        if (p->found[c] == PR_NONE && c != first) return PA_TURN | c;
    return -1;
}

/* the seat's view: the pictures of the cards never turned up wiped */
void prb_view(prgame *g)
{
    for (int c = 0; c < g->pos.n; c++)
        if (!g->pos.seen[c]) g->pos.pic[c] = UNKNOWN;
    g->seed = g->pos.seed = 0;                  /* (the deal's seed would tell them all) */
}

static int think(int seat, int level, char *move, int cap)
{
    prgame g = *pairs_live();                   /* the copy, at entry: nothing else is read */
    prb_view(&g);                               /* then only what the seat may see */
    if (g.pos.turn != seat || cap < 4) return 0;
    int a = prb_choose(&g, level, ai_session_seed());
    if (a < 0 || ai_cancelled()) return 0;
    if (a == PA_BACK) snprintf(move, (size_t)cap, "h");
    else snprintf(move, (size_t)cap, "t%d", PA_CARD(a));
    return 1;
}

/* the cards of a miss turn back by themselves (pairs.c), nobody acts then */
static int actor(void)
{
    const prpos *p = &pairs_live()->pos;
    return p->phase == PP_PLAY ? p->turn : -1;
}

static const bot pairs_bot = {.think = think, .actor = actor, .play = pairs_show};
REGISTER_BOT(pairs, &pairs_bot)
