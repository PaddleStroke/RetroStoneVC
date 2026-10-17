/*
 * Werewolves, the folk game of a village and its hidden wolves for 5 to 12 people round one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The roles, dealt at random: werewolves (1 for 5 or 6 people, 2 for 7 to 9, 3 for 10 or more), one seer, one witch
 * from 7 people on, the others villagers. The device passes round once for everyone to see their role alone; then the
 * nights and the days, the device the narrator:
 * - the night: the werewolves pick a victim among the others; the seer (alive) looks at one player's role; the witch
 *   (alive) sees the victim and may save them with her potion of life and kill anyone with her potion of death, each
 *   potion once a game;
 * - the dawn: the night's dead are out, their roles shown;
 * - the day: the village talks, then votes one player out, whose role is shown; then the next night.
 * The village wins when no werewolf is left; the werewolves when they are as many as the others.
 * Notation (script; the screenshots and the tests), words separated by spaces, players from 0: l the person looking
 * passes the device on, k<p> the werewolves' victim, e<p> the seer looks at p, w the witch's step (w alone: nothing;
 * wh: she saves the victim; wp<p>: she kills p; whp<p>: both), n on (the dawn to the vote, the vote's result to the
 * night), v<p> the village votes p out.
 */
#ifndef WEREWOLVES_H
#define WEREWOLVES_H

#include <stdint.h>

#define WW_MINP 5
#define WW_MAXP 12
#define WW_VARIANTS (WW_MAXP - WW_MINP + 1)

enum { WW_VILLAGER, WW_WOLF, WW_SEER, WW_WITCH, WW_ROLES };
enum { WP_DEAL, WP_WOLVES, WP_SEER, WP_WITCH, WP_DAWN, WP_VOTE, WP_OUT, WP_OVER };
enum { WW_VILLAGE_WINS, WW_WOLVES_WIN };

typedef struct {
    int n, phase, looker, night;
    uint8_t role[WW_MAXP], alive[WW_MAXP];
    int victim, seen, saved, poisoned;          /* tonight: the wolves' victim, the seer's look, the witch's acts */
    int heal, poison;                           /* the witch's potions left */
    int dead[2], ndead;                         /* the dawn's dead */
    int out;                                    /* the day's vote */
    int winner;                                 /* WP_OVER: WW_VILLAGE_WINS or WW_WOLVES_WIN */
    uint32_t rng;
} ww_state;

int  ww_wolves_for(int n);
int  ww_witch_for(int n);                       /* 1: a witch with n people */
void ww_new(ww_state *s, int n, uint32_t seed);
int  ww_looked(ww_state *s);                    /* l */
int  ww_can_kill(const ww_state *s, int p);
int  ww_kill(ww_state *s, int p);               /* k */
int  ww_can_look(const ww_state *s, int p);
int  ww_look(ww_state *s, int p);               /* e */
int  ww_can_poison(const ww_state *s, int p);
int  ww_witch(ww_state *s, int save, int poison);   /* w: poison -1 none */
int  ww_next(ww_state *s);                      /* n */
int  ww_vote(ww_state *s, int p);               /* v */
int  ww_alive(const ww_state *s, int role);     /* how many of that role are alive (-1: everyone) */
int  ww_holder_role(const ww_state *s);         /* the role acting now (a night's step), else -1 */
int  ww_script(ww_state *s, const char *moves);

#endif
