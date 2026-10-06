/*
 * Snail Switch: the planner (plan.c), shared by the test bot and the tests' solver (tests/solver.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#ifndef SNAIL_PLAN_H
#define SNAIL_PLAN_H
#include "game.h"
/* A snail for planning: state s = side + 2 * curl frame (0..21) + PLAN_K * k, k = 1 when it holds the fruit whose
 * catch window it is in (its berry is low[f] + k). */
#define PLAN_K      (2*FLIP_FRAMES)
#define PLAN_STATES (2*PLAN_K)
/* the course over frames 0..nf as one snail (at x hx) sees it: per frame the scroll, the time, the last fruit it must
 * hold (low) and its first item near it (first: an index into it) */
typedef struct plan {const item*it;int n,hx,nf;const int32_t*scroll;const int*t;const int32_t*low;const int*first;} plan;
int plan_state(int side,int flip,int k);
int plan_flip(int s);
int plan_side(int s);
int32_t plan_low(const item*it,int n,int32_t wx,int32_t next_id);
/* state s at frame f-1, pressing (a settled snail: the other surface) or not: its state at frame f, -1 when out */
int plan_next(const plan*p,int f,int s,int press);
#endif
