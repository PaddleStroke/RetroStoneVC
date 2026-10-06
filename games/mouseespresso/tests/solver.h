/*
 * Mouse Espresso: the input-search solver (tests only). It flies the game's own physics (hero_fly) through the
 * game's own course (course.c: the real director, course_hits) and searches "A held / A released" frame by frame.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 */
#ifndef ME_SOLVER_H
#define ME_SOLVER_H
#include "game.h"

#define SV_MIN_DWELL 4          /* frames: a player holds or releases A at least this long (a 67-ms tap) */
#define SV_COMFORT 6            /* a window this wide is comfortable: the careful player takes the latest such */
#define SV_WINDOW_CAP 14        /* windows are measured up to this (14 frames and more: no timing pressure) */
#define SV_MAX_FRAMES 16384

typedef struct sv_toggle { int32_t frame,x; int window,pattern; } sv_toggle;

typedef struct sv_run {
 int ok;                        /* the careful path reached the end alive */
 int32_t frames,fail_frame,fail_x;
 int ntog;
 sv_toggle *tog;                /* the careful path's input changes (caller's array, SV_MAX_FRAMES) */
 int8_t *held;                  /* the input on every frame (caller's array, SV_MAX_FRAMES + 1) */
 int16_t *ys;                   /* the mouse's y on every frame (caller's array) */
 int min_window;
 uint64_t nodes;
 int peak_items;                /* the ring's peak occupancy */
} sv_run;

void sv_init(void);
/* The careful player over seed's course, from the start until world x end_x (or SV_MAX_FRAMES), its mouse at slot
 * x (72: one player; 48 + 28 p: four players). */
void sv_careful(uint32_t seed,int slot,int32_t end_x,sv_run *r);
/* the travel (Q16) after t frames of a run */
int64_t sv_travel(int t);
/* the course the last sv_careful flew (its ring), and every pattern it generated */
const course *sv_course(void);
#define SV_MAX_PATTERNS 2048
typedef struct sv_pattern { pattern_rec p; int items,biscuits,dynamic; } sv_pattern;
const sv_pattern *sv_patterns(int *n);
extern long sv_exhausted;      /* questions the search gave up on (counted as "no": windows only narrower) */

#endif
