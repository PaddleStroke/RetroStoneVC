/*
 * Leady Squid: a run (the squids, the course of obstacles, the scroll, the
 * score). Pure game rules: no drawing or sound, events are reported instead.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 */
#include "ls.h"
#include <string.h>

int world_scroll_px(const world *w) { return (int)(w->scroll >> 16); }

static void add_obstacle(world *w)
{
    obstacle o = obstacle_make(&w->rng, w->next_index);
    o.x = FIRST_OBST_X + (w->next_index - w->first_index) * SPACING;
    w->ob[w->nob++] = o;
    w->next_index++;
}

/* keep the obstacles from just behind the screen to two spacings past its right edge */
static void course(world *w)
{
    int sx = world_scroll_px(w), k = 0;
    for (int i = 0; i < w->nob; i++)
        if (w->ob[i].x + OBST_W >= sx - 64) w->ob[k++] = w->ob[i];
    w->nob = k;
    while (w->nob < OB_MAX && (w->nob == 0 || w->ob[w->nob - 1].x < sx + RS_SCREEN_W + 2 * SPACING))
        add_obstacle(w);
}

/* The seeds of two runs often differ in their low bits only (the start frame is added), and the
 * xorshift's first outputs keep the seed's high bits: mix every bit into all of them first, or the
 * first gaps of a session would take a handful of heights only. */
static uint32_t seed_mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x ? x : 1;
}

static void new_course(world *w)
{
    w->nob = 0;
    w->next_index = w->first_index;
    rs_rng_seed(&w->rng, seed_mix(w->seed));
    course(w);
}

void world_init(world *w, int players, uint32_t seed, int first_index)
{
    memset(w, 0, sizeof *w);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->seed = seed;
    w->first_index = first_index;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        squid_reset(&w->sq[p], SQUID_X - p * P2_OFFSET_X);
        w->sq[p].next_ob = first_index;
        w->sq[p].score = first_index;
        if (p >= w->players) w->sq[p].state = SQ_OFF;
    }
    new_course(w);
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->sq[p].state == SQ_SWIM) return 1;
    return 0;
}

int world_all_resting(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->sq[p].state != SQ_REST) return 0;
    return 1;
}

const obstacle *world_next_obstacle(const world *w, int player)
{
    const squid *s = &w->sq[player];
    int wx = world_scroll_px(w) + s->x;
    for (int i = 0; i < w->nob; i++)
        if (w->ob[i].x + OBST_W > wx - 8) return &w->ob[i];
    return w->nob ? &w->ob[w->nob - 1] : 0;
}

int world_theme_at(const world *w)
{
    const obstacle *o = world_next_obstacle(w, 0);
    return o ? o->theme : 0;
}

static void swim(world *w, int p, int flap)
{
    squid *s = &w->sq[p];
    int *ev = &w->events[p];
    squid_swim_step(s, flap);
    if (flap) *ev |= EV_FLAP;
    box b = squid_box(s);
    int sx = world_scroll_px(w);
    for (int i = 0; i < w->nob; i++)
        if (box_hits_obstacle(b, &w->ob[i], sx)) {
            s->state = SQ_HIT;
            s->t = 0;
            *ev |= EV_HIT;
            return;
        }
    if (box_hits_seabed(b)) {
        s->state = SQ_REST;
        s->t = 0;
        s->y = (int32_t)squid_rest_y(s) * Q16_ONE;
        s->tilt = TILT_MIN;
        *ev |= EV_HIT | EV_LAND;
        return;
    }
    /* score: the squid's centre passes the obstacle's centre */
    int wx = sx + s->x;
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        if (o->index >= s->next_ob && wx >= o->x + OBST_W / 2) {
            s->score++;
            s->next_ob = o->index + 1;
            *ev |= EV_SCORE;
        }
    }
}

void world_step(world *w, const int flap[MAX_PLAYERS])
{
    w->t++;
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    if (!w->started) {
        int go = 0;
        for (int p = 0; p < w->players; p++) go |= flap[p];
        if (go) {
            w->started = 1;
            if (!w->fixed_seed) w->seed = w->seed * 2654435761u + (uint32_t)w->t;   /* the course: from the start frame */
            new_course(w);
            for (int p = 0; p < w->players; p++) {
                w->sq[p].state = SQ_SWIM;
                w->sq[p].t = 0;
                w->events[p] |= EV_START;
            }
        }
    }
    int moving = world_running(w);
    int old = world_scroll_px(w);
    if (moving) w->scroll += SCROLL;
    int dx = world_scroll_px(w) - old;
    course(w);
    for (int p = 0; p < w->players; p++) {
        squid *s = &w->sq[p];
        switch (s->state) {
        case SQ_READY:
            s->y = (int32_t)(SQUID_START_Y + bob_offset(s->t)) * Q16_ONE;
            s->flap_t++;
            break;
        case SQ_SWIM:
            swim(w, p, flap[p]);
            break;
        case SQ_HIT:
            if (s->t >= DEATH_HIT_FREEZE) {
                s->state = SQ_SINK;
                s->t = 0;
                if (s->vy < 0) s->vy = 0;
                w->events[p] |= EV_SINK;
            }
            break;
        case SQ_SINK:
            squid_sink_step(s);
            if (s->y >= (int32_t)squid_rest_y(s) * Q16_ONE) {
                s->y = (int32_t)squid_rest_y(s) * Q16_ONE;
                s->state = SQ_REST;
                s->t = 0;
                w->events[p] |= EV_LAND;
            }
            break;
        default:
            break;
        }
        /* a squid that is not swimming drifts with the world (2 players: the other one goes on) */
        if (s->state == SQ_HIT || s->state == SQ_SINK || s->state == SQ_REST) s->x -= dx;
        s->t++;
    }
}
