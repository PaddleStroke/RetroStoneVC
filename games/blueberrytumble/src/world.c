/*
 * Blueberry Tumble: the run. The frame clock, the berries (player 2 rolls P2_OFFSET px behind), the course written
 * ahead by the director, the smashed and collected things. No drawing, no sound.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include <string.h>

#define PX64(v) ((int64_t)(v) * Q16_ONE)

/* Widescreen (DESIGN.md "Widescreen"): the run-up. The course is the same at every width: flat ground from x 0, the
 * first pattern at START_CELLS (384 px). At the start the screen shows the course up to its right edge (PIVOT_X +
 * 384 = 480 is the first pattern's screen x): on a screen wider than that the start would show the first pattern
 * (and change it on the title, when the lobby's placeholder course gives way to the run's). So the berries start
 * whole bars before x 0 (the music's bars stay on the course's bar lines; whole frames at the first tempo, rounded
 * up: half a frame at most) and the first pattern waits off the real right edge. From x 0 on, the run is exactly the
 * run without a run-up (the same course, the same frames of the clock: course_x), `world_runup_frames()` later. */
int world_runup_frames(void)
{
    int over = rs_screen_width() - PIVOT_X - START_CELLS * CELL;   /* px of the first pattern on screen at x 0 */
    if (over <= 0) return 0;
    int bars = (over + BAR_CELLS * CELL - 1) / (BAR_CELLS * CELL);
    return (bars * (BAR_CELLS / BEAT_CELLS) * tier_fpb_num(0) + tier_fpb_den(0) - 1) / tier_fpb_den(0);
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    course_reset(&w->course, -1);
    director_start(&w->course, seed);
    director_fill(&w->course, 48);
    w->players = players;
    w->seed = seed;
    w->f = -world_runup_frames();          /* the course clock: x 0 at frame 0 (widescreen: the run-up before) */
    int64_t x0 = course_x(&w->course, w->f);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        berry_reset(&w->b[p], x0 - PX64(p * P2_OFFSET));
        w->b[p].need_release = 1;           /* the press that starts the run is not a jump */
        w->b[p].held = 1;
        if (p >= players) w->b[p].dead = 1;
    }
}

int world_x(const world *w, int p) { return (int)(w->b[p].x >> 16); }

int world_score(const world *w, int p) { return w->metres[p] + w->coins[p] * GOLDEN_BONUS; }

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (!w->b[p].dead) return 1;
    return 0;
}

static void mark_gone(bt_course *c, int32_t id, int *newly)
{
    if (id < 0) {
        bt_cone *k = &c->cone[(-1 - id) % CONE_RING];
        if (!k->gone) { k->gone = 1; (*newly)++; }
        return;
    }
    bt_col *k = course_col_w(c, id / 16);
    int bit = 1 << (id & 15);
    if (!(k->gone & bit)) { k->gone |= (uint16_t)bit; (*newly)++; }
}

void world_step(world *w, const int held[MAX_PLAYERS])
{
    for (int p = 0; p < MAX_PLAYERS; p++) {
        w->events[p] = 0;
        w->info[p].nsmash = 0;
        w->info[p].coin_id = -1;
    }
    if (!w->started) return;
    w->t++;
    if (!world_running(w)) return;          /* every berry is down: the course stops */
    w->f++;
    int64_t xref = course_x(&w->course, w->f);
    director_fill(&w->course, (int32_t)((xref >> 16) / CELL) + 40);
    for (int p = 0; p < w->players; p++) {
        berry *b = &w->b[p];
        if (b->dead) continue;
        int64_t x = xref - PX64(p * P2_OFFSET);
        step_info in;
        memset(&in, 0, sizeof in);         /* it is kept in the world (saved): no stale bytes */
        berry before = *b;
        int ev = berry_step(b, &w->course, x, xref, held[p], &in);
        if (w->god && (ev & EV_DIE)) {
            /* the screenshot aid: bounce off whatever it was (never in a normal game) */
            *b = before;
            b->x = x;
            b->vy = JUMP_V;
            b->grounded = 0;
            if (b->h < 0) b->h = 0;
            ev = EV_JUMP;
        }
        int newly = 0;
        for (int i = 0; i < in.nsmash; i++) mark_gone(&w->course, in.smash_id[i], &newly);
        if (!newly) ev &= ~EV_SMASH;
        if (ev & EV_COIN) {
            int got = 0;
            mark_gone(&w->course, in.coin_id, &got);
            if (got) w->coins[p]++;
            else ev &= ~EV_COIN;
        }
        if (ev & EV_DIE) w->dead_f[p] = w->t;
        int32_t m = (int32_t)((b->x >> 16) / CELL);
        if (m > w->metres[p]) w->metres[p] = m;
        w->events[p] = ev;
        w->info[p] = in;
    }
}
