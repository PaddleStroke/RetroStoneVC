/*
 * Play by Ear's rules, without SDL (melody_rules.c; the tests link them): a tune is played, the player plays its notes
 * back in order on a keyboard. Only the order of the pitches counts, not the rhythm. A wrong key is a mistake and the
 * player goes on from the same place; the hint gives the next key; "listen again" plays the tune once more.
 * The tunes are all in the public domain (tools/melody_tunes.txt); the levels: games/melody_levels.h
 * (tools/melody_gen.py). The piano's voice, also without SDL: melody_synth.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef MELODY_H
#define MELODY_H

#include <stdint.h>

#define MEL_MAX 56                              /* notes in a level, at most */
#define MEL_LOW 60                              /* the keyboard's lowest key: C4 (MIDI) */
#define MEL_HIGH 84                             /* the highest any level reaches: C6 */
#define MEL_LISTENS_FREE 1                      /* "listen again" this many times still gives 3 stars */
#define MEL_LEVEL_COUNT 150                     /* melody_levels.h's MEL_LEVELS (checked there), in 6 chapters */
#define MEL_CHAPTER_COUNT 6

typedef struct { const char *id; uint8_t bpm; uint16_t died; } mel_tune;
/* a level: its tune (mel_tunes[], help/<lang>/melody_titles.txt), its notes, the keyboard's white keys from C4 (8:
 * up to C5, 12: G5, 15: C6), its grade x 10; the pitches (MIDI) and each one's gap to the next (sixteenths) */
typedef struct { uint8_t tune, n, whites; uint16_t grade; uint8_t p[MEL_MAX], d[MEL_MAX]; } mel_level;

/* random play: a phrase of the library of that length (never notes at random), sometimes in another key */
enum { MEL_SHORT, MEL_MEDIUM, MEL_LONG, MEL_VARIANTS };

typedef struct {
    int level;                                  /* the level, or the library's phrase played at random */
    int tune, n, whites, bpm;
    uint8_t p[MEL_MAX], d[MEL_MAX];             /* its pitches (transposed at random play) and gaps */
    int pos;                                    /* the notes played back right so far */
    int mistakes, hints, listens;               /* wrong keys; hints; "listen again" (the first playing is free) */
    int hinted;                                 /* the place the last hint was for (-1): a hint asked again there
                                                 * counts once */
} mel_game;

enum { MEL_WRONG, MEL_RIGHT, MEL_DONE, MEL_OVER };   /* mel_press: the key was wrong, right, the last; already over */

int  mel_levels_count(void);
const mel_level *mel_level_at(int level);
const mel_tune *mel_tune_at(int tune);
int  mel_top(int whites);                       /* the keyboard's highest key (MIDI) */
int  mel_whites_for(int highest);               /* the smallest keyboard holding that key */
int  mel_black(int pitch);                      /* 1: a black key */
int  mel_variant_of(int notes);                 /* the random play's variant of a phrase that long */

void mel_load(mel_game *g, int level);
/* a phrase of the library for the variant (MEL_SHORT up to 8 notes, MEDIUM 9..16, LONG 17 and more), moved to
 * another key two times out of three, within the keyboard */
void mel_random(mel_game *g, int variant, uint32_t seed);

int  mel_press(mel_game *g, int pitch);         /* MEL_*: right moves on; wrong counts a mistake and stays */
int  mel_hint(mel_game *g);                     /* the next pitch (-1: solved); counted once a place */
void mel_listen(mel_game *g);                   /* "listen again": counted */
int  mel_undo(mel_game *g);                     /* the last right note taken back (the mistakes stay); 0: none */
int  mel_solved(const mel_game *g);
int  mel_next(const mel_game *g);               /* the pitch to play now (-1: solved) */
/* the stars: the logic puzzles' rule on (hints + listens past the first free one, mistakes): 3 with none of them,
 * 2 with two at most, else 1 */
int  mel_help(const mel_game *g);
int  mel_stars(const mel_game *g);
int  mel_ms(const mel_game *g, int i);          /* when note i starts, ms after the first (with its rhythm) */
int  mel_len_ms(const mel_game *g, int i);      /* how long note i lasts, ms */

/* a note's name ("C4", "F#5", "Bb4") into a pitch; its length in *used; -1: not a note */
int  mel_parse_note(const char *s, int *used);
/* moves in the picture mode and the tests: notes by name ("E4 D4 C4"), "h" a hint, "l" listen again; 0: bad */
int  mel_script(mel_game *g, const char *moves);

/* ---- the piano's voice (melody_synth.c): a note as sound, 16-bit at MEL_RATE. Additive partials, slightly
 * inharmonic, in pairs a little out of tune with each other (two strings), a soft attack and the hammer's knock, a
 * fast then slow decay (higher partials faster); dull: a key gone wrong, damped at once (a felt-muffled knock). */
#define MEL_RATE 44100
int  mel_synth_len(int pitch, int dull);       /* its length in samples */
void mel_synth(int pitch, int dull, int16_t *out, int n);

#endif
