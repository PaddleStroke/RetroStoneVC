/*
 * Blueberry Tumble: the run. The frame clock, the berries (player 2 rolls P2_OFFSET px behind), the course written
 * ahead by the director, the smashed and collected things. No drawing, no sound.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include <string.h>

#define PX64(v) ((int64_t)(v) * Q16_ONE)

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    course_reset(&w->course, -1);
    director_start(&w->course, seed);
    director_fill(&w->course, 48);
    w->players = players;
    w->seed = seed;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        berry_reset(&w->b[p], -PX64(p * P2_OFFSET));
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
