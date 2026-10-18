/*
 * Falling Notes' rules, without SDL (pianofall_rules.c; the tests link them): a song's notes fall down the lanes above
 * a piano's keys, each a bar as long as the note, and the player presses each key as its note reaches the line above
 * the keys (and holds a long one to its end). Each press is judged by how far it is from the note's time: perfect,
 * good, or a miss (no press in time). The points make the accuracy, and the accuracy the stars.
 * The songs: games/pianofall_levels.h (tools/pianofall_gen.py, from Play by Ear's public-domain tunes); random play: a
 * melody made at random (three difficulties). All the times here are the song's own milliseconds (as written, at the
 * level's tempo): the game turns its clock into them (slower in practice), calibration included.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PIANOFALL_H
#define PIANOFALL_H

#include <stdint.h>

#define PFL_MAX 256                             /* notes in a song, at most */
#define PFL_LEVEL_COUNT 60                      /* pianofall_levels.h's PFL_LEVELS (checked there), in 6 chapters */
#define PFL_CHAPTER_COUNT 6
#define PFL_PERFECT_MS 70                       /* a press this close to its note's time: perfect, */
#define PFL_GOOD_MS 150                         /* this close: good; later than this: the note is missed */
#define PFL_HOLD_MS 560                         /* a note this long is held (a bar to hold down), */
#define PFL_HOLD_SLACK 160                      /* let go this long before its end at most */
#define PFL_GAP_MIN 90                          /* the tightest two notes on one key may follow each other */
#define PFL_LOWEST 55                           /* G3 .. C6: the keys a song may use */
#define PFL_HIGHEST 84
#define PFL_WHITES_MAX 15
#define PFL_OFFSET_MAX 120                      /* the calibration: presses taken as this much later at most (ms) */

/* a song as stored: its notes' start and length in sixteenths, their pitches (MIDI) */
typedef struct { uint16_t t; uint8_t d, p; } pfl_src;
/* tune: its title (help/<lang>/melody_titles.txt, line 2 * tune); bpm; the keyboard's lowest and highest white keys;
 * chords: two notes at once; n notes from pfl_notes[first] */
typedef struct { uint8_t tune, bpm, low, high, chords; uint16_t n, first; } pfl_song;

enum { PFL_EASY, PFL_MEDIUM, PFL_HARD, PFL_VARIANTS };   /* random play */
enum { PFL_PENDING, PFL_PERFECT_HIT, PFL_GOOD_HIT, PFL_MISS };   /* a note's judgement */
enum { PFL_TAP, PFL_HOLDING, PFL_HELD, PFL_BROKEN };    /* a long note's hold: none (a tap), down, to its end, let go */

typedef struct { int t, len, pitch; } pfl_note; /* ms from the song's start */

typedef struct {
    int level;                                  /* the song (-1: random play) */
    int tune;                                   /* its title (-1: a random melody) */
    int bpm, low, high, n;                      /* the keyboard: lowest and highest white keys (MIDI) */
    pfl_note note[PFL_MAX];
    uint8_t judge[PFL_MAX], hold[PFL_MAX];      /* PFL_PENDING..., PFL_TAP... */
    int16_t err[PFL_MAX];                       /* a press's time from its note's (ms, + late) */
    int scan;                                   /* the first note not judged yet */
    int now;                                    /* the song's time reached (pfl_advance) */
    int combo, best_combo, perfect, good, miss, stray;
    int err_sum, err_n;                         /* the presses' times from their notes' (the calibration) */
} pfl_game;

int  pfl_levels_count(void);
const pfl_song *pfl_song_at(int level);
void pfl_load(pfl_game *g, int level);
/* a melody made at random, in a key, with a rhythm (PFL_EASY: white keys, slow, quarters and halves; MEDIUM: a key
 * with black keys, eighths; HARD: faster, wider, a second voice on strong beats) */
void pfl_random(pfl_game *g, int variant, uint32_t seed);

void pfl_restart(pfl_game *g);                  /* the same notes, nothing played yet */

int  pfl_black(int pitch);
int  pfl_whites(const pfl_game *g);             /* white keys shown */
int  pfl_end_ms(const pfl_game *g);             /* the song's end: its last note's */
int  pfl_is_hold(const pfl_game *g, int i);

/* the song's time moves on to t: the notes no longer playable are missed, the holds kept to their end done */
void pfl_advance(pfl_game *g, int t);
/* a key pressed at t: the note judged (its index; its judgement in g->judge[i]), or -1 a press for no note (stray:
 * the combo is broken) */
int  pfl_press(pfl_game *g, int pitch, int t);
/* the key of note i (a long one) let go at t: kept to its end, or let go too soon (half its points) */
void pfl_release(pfl_game *g, int i, int t);
int  pfl_over(const pfl_game *g);               /* every note judged and every hold over */
int  pfl_points(const pfl_game *g);             /* perfect 100, good 60, a hold let go too soon half of it */
int  pfl_accuracy(const pfl_game *g);           /* 0..100: the points against all perfect */
int  pfl_stars(int accuracy);                   /* 3 from 95, 2 from 80, 1 from 50, else 0 */
/* the calibration after a song: the offset (ms) moved half way towards the presses' mean lateness, kept within
 * +-PFL_OFFSET_MAX; unchanged with fewer than 8 presses */
int  pfl_calibrate(const pfl_game *g, int offset);

/* the picture mode's and the tests' moves: "p<ms>" played perfectly up to that time, "g<ms>" every note pressed 100 ms
 * late (good), "m<ms>" nothing pressed (misses), "end" perfectly to the end; 0: bad */
int  pfl_script(pfl_game *g, const char *moves);
/* played perfectly from g->now to t: each note pressed at its time, each long one let go at its end */
void pfl_autoplay(pfl_game *g, int t, int late);

/* a chart's check (the tests): sorted, within the keys and the keyboard, no two notes at once on a key, notes on one
 * key far enough apart, at most two at once, lengths positive; 0: fine, else a code */
int  pfl_check(const pfl_game *g);

#endif
