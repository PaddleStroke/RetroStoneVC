/*
 * Mental Math for one player: the rules. A drill of sums against the clock: "7 + 5 = ?", "36 / 4 = ?", sometimes
 * "? + 6 = 13". Type the missing number; as soon as it has as many digits as the answer it is checked: right, the next
 * sum comes; wrong, it is a mistake and the answer is typed again. The drill is over when every sum is answered.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A drill is made from a spec: the operations, the ranges (sums up to a number, from one; differences of numbers up
 * to one; tens alone; a two-digit number with a one-digit one or with another two-digit one; the times tables chosen
 * (bit n: the table of n), their other factor up to a number), how often a missing number is not the result, how
 * many sums and the time a sum is worth. No sum twice in a drill, no answer twice in a row. The levels: 150 drills of
 * our own in six chapters (plus, minus, to a hundred, the times tables, division, everything), made by
 * tools/mentalmath_gen.c and stored as their sums (mentalmath_levels.h). The variants (random play): plus, minus, plus
 * and minus to a hundred, times, divided, mixed.
 * The time and the stars: par is the sums' time added up (seconds); 3 stars within it, 2 within half as much again, 1
 * beyond; and no more than the hints and mistakes allow (none 3, up to two 2, more 1).
 * The notation (script, the pictures, the tests): digits typed ("7", "12"), "b" a digit taken back, "h" a hint (the
 * answer typed), "=" the right answer typed, "+n" n sums answered right.
 */
#ifndef MENTALMATH_H
#define MENTALMATH_H

#include <stdint.h>

#define MMH_MAXQ 20                             /* sums in a drill at most */
#define MMH_LEVELS 150
#define MMH_CHAPTERS 6
#define MMH_VARIANTS 6

enum { MMH_ADD = 1, MMH_SUB, MMH_MUL, MMH_DIV };
enum { MMH_MORE, MMH_RIGHT, MMH_WRONG };        /* what a digit typed did */

typedef struct {
    int16_t n[3];                               /* n[0] op n[1] = n[2] */
    uint8_t op, miss;                           /* the missing number: 0, 1 or 2 (the result) */
} mmh_q;

typedef struct {
    unsigned ops;                               /* bit MMH_ADD .. MMH_DIV */
    int add_lo, add_hi;                         /* sums: the result between them */
    int sub_lo, sub_hi;                         /* differences: the first number between them */
    int form;                                   /* plus and minus: 0 any, 1 tens alone, 2 two digits and one, 3 two and two */
    unsigned tables;                            /* times and divided: bit n the table of n */
    int times_hi;                               /* the other factor, 1 .. this */
    int missing;                                /* the sums with an operand missing, in hundredths */
    int count, tenths;                          /* the sums; the time a sum is worth (tenths of a second) */
} mmh_spec;

typedef struct {
    int nq, par;                                /* the sums; par in seconds */
    mmh_q q[MMH_MAXQ];
    int cur, typed, ndig;                       /* the sum asked; the digits typed so far */
    uint8_t slips[MMH_MAXQ];                    /* each sum's mistakes and hint */
    int mistakes, hints;
} mmh_game;

int  mmh_answer(const mmh_q *q);
int  mmh_digits(int v);
int  mmh_check(const mmh_q *q);                 /* 1: a true sum (whole numbers, nothing below zero) */
void mmh_drill(const mmh_spec *s, uint32_t seed, mmh_game *g);   /* a new drill */
mmh_spec mmh_variant_spec(int variant);
/* the levels (mentalmath_levels.h): "<par>:" then the sums, comma separated, as "7+5=?12" ('?' before the missing
 * number; + - * /) */
int  mmh_encode(const mmh_game *g, char *out, int size);
int  mmh_decode(const char *s, mmh_game *g);    /* 1: a drill (ready to play) */

int  mmh_type(mmh_game *g, int digit);          /* MMH_MORE, MMH_RIGHT, MMH_WRONG */
int  mmh_back(mmh_game *g);                     /* 1: a digit taken back */
int  mmh_hint(mmh_game *g);                     /* the answer typed (counted): MMH_RIGHT; 0 over */
int  mmh_done(const mmh_game *g);
int  mmh_stars(const mmh_game *g, int tenths);  /* the drill over in that time: 1..3 */
int  mmh_script(mmh_game *g, const char *moves);

/* the drill's clock, paused while the game is away (as Falling Notes' song): each frame drawn says the time (now: the
 * game's clock, ms) and the real time (real: ms; a gap of more than MMH_AWAY_MS between two frames drawn is the game
 * not on the screen: another screen, the rules, the app in the background), and that gap is not counted */
#define MMH_AWAY_MS 250
#define MMH_LEAP_MS 300000                      /* the game's clock leaping more between two frames: not counted */
typedef struct { uint32_t t0, last, real, end; int on, over; } mmh_clock;
void     mmh_clock_start(mmh_clock *c, uint32_t now, uint32_t real);
void     mmh_clock_frame(mmh_clock *c, uint32_t now, uint32_t real);   /* a frame drawn (or the player acting) */
uint32_t mmh_clock_ms(const mmh_clock *c, uint32_t now, uint32_t real); /* the drill's time so far */
void     mmh_clock_stop(mmh_clock *c, uint32_t now, uint32_t real);

#endif
