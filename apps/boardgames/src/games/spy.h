/*
 * The Spy, a talk game for 3 to 10 people round one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Each round the device is passed round: everyone but the spy sees the same place (one of SPY_PLACES, never twice in
 * a game), the spy sees only that they are the spy. Then the people talk, asking each other questions, while the clock
 * runs; then they agree on whom to accuse. The spy may stop the talk at any time to name the place.
 * Scoring, a round:
 * - the spy names the place right (stopping the talk, or as a last chance once accused): the spy 3;
 * - the spy accused and wrong about the place, or stopping the talk and wrong: everyone else 1;
 * - someone else accused: the spy 2.
 * SPY_ROUNDS rounds; the most points win (a tie shares the win).
 * Notation (script; the screenshots and the tests), words separated by spaces: l the person looking passes the device
 * on, c the talk ends (the accusation), s the spy stops the talk to name the place, v<k> person k accused (from 0),
 * g<k> the spy names place k (from 0), n the next round.
 */
#ifndef SPY_H
#define SPY_H

#include <stdint.h>

#define SPY_PLACES 60
#define SPY_MINP 3
#define SPY_MAXP 10
#define SPY_ROUNDS 5
#define SPY_VARIANTS (SPY_MAXP - SPY_MINP + 1)

enum { SPY_LOOK, SPY_TALK, SPY_VOTE, SPY_GUESS, SPY_SCORE, SPY_OVER };
enum { SPY_CAUGHT = 1, SPY_STOPPED = 2 };       /* how the round's guess came */

typedef struct {
    int n, round, phase;
    int looker;                                 /* SPY_LOOK: the person looking now */
    int place, spy, first;                      /* the round's place, its spy, the first to ask */
    int accused, guess, how;                    /* -1: none yet; how: SPY_CAUGHT, SPY_STOPPED, 0 */
    int score[SPY_MAXP], gain[SPY_MAXP];        /* the totals; the last round's points */
    uint8_t deck[SPY_PLACES];                   /* the places in the game's order */
    uint32_t rng;
} spy_state;

void spy_new(spy_state *s, int n, uint32_t seed);
int  spy_looked(spy_state *s);                  /* l: the next person looks, or the talk starts */
int  spy_call(spy_state *s);                    /* c: the talk ends, the accusation */
int  spy_stop(spy_state *s);                    /* s: the spy stops the talk to name the place */
int  spy_accuse(spy_state *s, int person);      /* v: the spy then names the place (caught), else the score */
int  spy_name(spy_state *s, int place);         /* g */
int  spy_next(spy_state *s);                    /* n: the next round, or the end */
int  spy_script(spy_state *s, const char *moves);   /* 0: a bad move */
unsigned spy_winners(const spy_state *s);       /* the best total (bit n: person n) */
/* the minutes the talk lasts, for n people */
int  spy_minutes(int n);

#endif
