/*
 * @NAME@: the rules (the play stub of games/_template: hop over the crates). No drawing, no sound.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
 *
 * REPLACE this with the game. Keep it pure (fixed point, the world's own RNG) so the determinism tests hold.
 */
#include "game.h"
#include <string.h>

static void hero_reset(hero *h, int x)
{
    memset(h, 0, sizeof *h);
    h->x = x;
    h->y = (int32_t)GROUND_Y << 16;
    h->state = HS_READY;
    h->land_t = 99;
    h->air_t = 99;
}

int hero_on_ground(const hero *h) { return h->y >= ((int32_t)GROUND_Y << 16) && h->vy >= 0; }

static void spawn(world *w)
{
    while (w->nb < BLOCK_MAX && w->next_x < (int)w->scroll + RS_SCREEN_W + 64) {
        w->bx[w->nb] = w->next_x;
        w->bidx[w->nb] = w->next_index++;
        w->nb++;
        w->next_x += BLOCK_GAP_MIN + rs_rng_range(&w->rng, BLOCK_GAP_RND);
        w->next_x &= ~7;
    }
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->players = players;
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->next_x = RS_SCREEN_W + 32;
    hero_reset(&w->h[0], HERO_X);
    hero_reset(&w->h[1], HERO_X + P2_OFFSET_X);
    if (players < 2) w->h[1].state = HS_OFF;
    spawn(w);
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state == HS_RUN) return 1;
    return 0;
}

int world_all_down(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state != HS_DOWN) return 0;
    return 1;
}

int world_block_ahead(const world *w, int player)
{
    const hero *h = &w->h[player];
    int best = -1;
    for (int i = 0; i < w->nb; i++) {
        int d = w->bx[i] - (int)w->scroll - (h->x + 6);
        if (d + BLOCK_W + 12 >= 0 && (best < 0 || d < best)) best = d < 0 ? 0 : d;
    }
    return best;
}

static int hits(const world *w, const hero *h)
{
    int hx0 = h->x - 6, hx1 = h->x + 6, hy1 = (int)(h->y >> 16);    /* the feet */
    for (int i = 0; i < w->nb; i++) {
        int x0 = w->bx[i] - (int)w->scroll, x1 = x0 + BLOCK_W, y0 = GROUND_Y - BLOCK_H + 2;
        if (hx1 > x0 + 2 && hx0 < x1 - 2 && hy1 > y0) return 1;
    }
    return 0;
}

void world_step(world *w, const int press[MAX_PLAYERS])
{
    int running = world_running(w);
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        w->events[p] = 0;
        h->t++;
        h->air_t++;
        h->land_t++;
        if ((h->state == HS_READY || h->state == HS_RUN) && press[p] && hero_on_ground(h)) {
            if (h->state == HS_READY) {
                h->state = HS_RUN;
                h->t = 0;
                w->events[p] |= EV_START;
                if (!w->started) {
                    w->started = 1;
                    running = 1;
                }
            }
            h->vy = JUMP_VY;
            h->air_t = 0;
            w->events[p] |= EV_JUMP;
        }
        if (h->state == HS_RUN || h->state == HS_HIT) {
            if (h->state == HS_HIT && h->t < HIT_FREEZE) continue;
            int was_air = !hero_on_ground(h);
            h->vy += GRAVITY;
            h->y += h->vy;
            if (h->y >= ((int32_t)GROUND_Y << 16)) {
                h->y = (int32_t)GROUND_Y << 16;
                h->vy = 0;
                if (was_air) { h->land_t = 0; w->events[p] |= EV_LAND; }
                if (h->state == HS_HIT) { h->state = HS_DOWN; h->t = 0; }
            }
        }
        if (h->state == HS_RUN) {
            if (hits(w, h)) {
                h->state = HS_HIT;
                h->t = 0;
                h->vy = JUMP_VY / 2;
                w->events[p] |= EV_HIT;
                continue;
            }
            for (int i = 0; i < w->nb; i++)
                if (w->bidx[i] == h->next_block && w->bx[i] + BLOCK_W - (int)w->scroll < h->x - 6) {
                    h->score++;
                    h->next_block++;
                    w->events[p] |= EV_SCORE;
                }
        }
    }
    if (running && world_running(w)) {
        w->scroll += SCROLL_SPEED;
        /* drop the blocks that left the screen */
        while (w->nb > 0 && w->bx[0] + BLOCK_W < (int)w->scroll - 64) {
            memmove(w->bx, w->bx + 1, sizeof(int) * (size_t)(w->nb - 1));
            memmove(w->bidx, w->bidx + 1, sizeof(int) * (size_t)(w->nb - 1));
            w->nb--;
        }
        spawn(w);
    }
    w->t++;
}
