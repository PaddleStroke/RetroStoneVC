/*
 * Blueberry Tumble: the input-search solver (tests only). It runs the game's own physics (src/physics.c on
 * src/course.c) and searches "A held / A released" on every frame.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#ifndef BT_SOLVER_H
#define BT_SOLVER_H

#include "bt.h"

#define SV_MAX_TOGGLES 512

typedef struct sv_toggle {
    int32_t frame;              /* the input changes on this frame (held from it, or released from it) */
    int press;                  /* 1 = press, 0 = release */
    int win_lo, win_hi;         /* the frames on which the change still works (contiguous) */
} sv_toggle;

typedef struct sv_result {
    int ok;                     /* the careful path reached the end alive (with the coin if asked) */
    int32_t frames;             /* frames simulated */
    int ntog;
    sv_toggle tog[SV_MAX_TOGGLES];
    int presses;
    int min_window;             /* the tightest window (frames) */
    int32_t min_window_frame;
    int skills;                 /* SK_* seen on the path */
    int react;                  /* the least reaction time (frames) */
    int32_t fail_frame;         /* when !ok: where the path got stuck */
    int32_t land_after;         /* frames after `boundary_x` until the berry is on the ground (the tail) */
    uint64_t nodes;             /* physics steps */
} sv_result;

typedef struct sv_query {
    bt_course *c;
    berry start;                /* the berry at start_f */
    int32_t start_f;
    int32_t end_f;              /* success: alive on this frame */
    int horizon;                /* 0: the goal is end_f; else each decision looks this many frames ahead */
    int need_coin;
    int32_t boundary_x;         /* px: measure the tail after this x (-1: no) */
    int late_last_press;        /* 1: the last press at the late edge of its window (the tail's worst case) */
    int32_t fill_to;            /* 0, or: the director fills the course ahead as the path advances (streams) */
} sv_query;

void sv_init(void);             /* allocates the memo (once) */
void sv_reset(void);            /* forgets every state (a new course) */
/* can `b` at frame f reach frame goal alive? */
int  sv_alive(const sv_query *q, const berry *b, int32_t f, int32_t goal);
/* the careful player's path: keep the input while possible, change it in the middle of each window */
void sv_careful(const sv_query *q, sv_result *r);

#endif
