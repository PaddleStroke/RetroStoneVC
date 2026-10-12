/*
 * Unicorn Hop: the rules. No drawing, no sound: fixed point and the world's own RNG, so a run is the same every time
 * (the determinism tests) and tests/test_rules.c can play it alone. The bot is here too: it plays from the world.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
 *
 * The unicorn stands on a cloud and waits. A press: it jumps straight up (a fixed arc, no steering). Above it each
 * level has one cloud sliding left and right at its own speed; coming down, the hooves cross the top of the cloud
 * above while over it -> it lands, climbs a level and rides that cloud. Otherwise it falls: the run is over.
 */
#include "game.h"
#include <string.h>

static const int width_steps[][3] = {   /* up to level (excluded): one of two widths, in middle pieces */
    {EASY_LEVELS, 5, 4}, {20, 4, 3}, {35, 3, 2}, {RAMP_LEVELS, 2, 2}, {1 << 30, 2, 1}};

static int iabs(int v) { return v < 0 ? -v : v; }

int hero_alt(const hero *h) { return h->level * LEVEL_H + (h->dy >> 16); }
int cam_px(const world *w) { return w->cam >> 8; }
int cloud_there(const cloud *c) { return c->gone_t < 0; }

const cloud *world_cloud(const world *w, int level)
{
    if (level < 0 || level >= w->top || level < w->top - RING) return NULL;
    const cloud *c = &w->c[level & (RING - 1)];
    return c->index == level ? c : NULL;
}

static cloud *cloud_at(world *w, int level) { return (cloud *)world_cloud(w, level); }

int jump_frames(int big)
{
    int32_t dy = 0, vy = big ? BIG_JUMP_VY : JUMP_VY, tgt = (int32_t)(LEVEL_H * (1 + big)) << 16;
    for (int n = 1; n < 200; n++) {
        int32_t prev = dy;
        vy -= GRAVITY;
        dy += vy;
        if (vy < 0 && prev >= tgt && dy < tgt) return n;
    }
    return 0;
}

/* ---- the levels: made as the climb goes up, kept in a ring ------------------------------------------------------- */
static int cloud_speed(world *w, int i)
{
    int s;
    if (i < EASY_LEVELS) s = SPEED_MIN + (SPEED_EASY - SPEED_MIN) * (i - 1) / (EASY_LEVELS - 1);
    else {
        int k = i < RAMP_LEVELS ? i : RAMP_LEVELS;
        s = SPEED_EASY + (int)((int64_t)(SPEED_MAX - SPEED_EASY) * (k - EASY_LEVELS) / (RAMP_LEVELS - EASY_LEVELS));
    }
    s = s * ((i & 1) ? 100 - SPEED_ALT : 100 + SPEED_ALT) / 100;
    return (int)((int64_t)s * (100 - SPEED_JITTER + rs_rng_range(&w->rng, 2 * SPEED_JITTER + 1)) / 100);
}

static void gen_level(world *w, int i)
{
    cloud *c = &w->c[i & (RING - 1)];
    memset(c, 0, sizeof *c);
    c->index = i;
    c->rain_t = c->gone_t = -1;
    c->star_by = -1;
    c->look = (uint8_t)rs_rng_range(&w->rng, 256);
    if (i == 0) {
        c->type = CL_START;
        c->w = START_W;
        c->x = CENTER_X << 16;
    } else {
        int k = 0;
        while (i >= width_steps[k][0]) k++;
        int mids = rs_rng_range(&w->rng, 2) ? width_steps[k][1] : width_steps[k][2];
        c->w = (int16_t)(i == 1 ? FIRST_W : 32 + 16 * mids);
        int v = cloud_speed(w, i);
        c->vx = (i & 1) ? -v : v;           /* the directions alternate; level 1 goes left, towards player 1 */
        int room = BAND_R - BAND_L - c->w;
        c->x = (i == 1 ? CENTER_X : BAND_L + c->w / 2 + rs_rng_range(&w->rng, room + 1)) << 16;
        c->type = CL_NORMAL;
        if (i - w->last_special >= 2) {
            if (i >= PINK_FROM && rs_rng_range(&w->rng, PINK_CHANCE) == 0) c->type = CL_PINK;
            else if (i >= RAIN_FROM && rs_rng_range(&w->rng, RAIN_CHANCE) == 0) c->type = CL_RAIN;
            if (c->type != CL_NORMAL) w->last_special = i;
        }
        const cloud *b = world_cloud(w, i - 1);
        if (b && b->type == CL_RAIN) {
            /* above a rain cloud: its twin, the same width, moving with it right above it; both stop when the rain
             * starts. The rain is a hurry (jump before it melts), never a trap: the escape is right above */
            c->w = b->w;
            c->x = b->x;
            c->vx = b->vx;
        }
    }
    /* the gap above: a thunder cloud (never above a rain cloud, never across a pink bounce, never twice in a
     * row), else maybe a star */
    const cloud *below = i > 0 ? world_cloud(w, i - 1) : NULL;
    int p = 0;
    if (i >= THUNDER_FROM) {
        int k = i < RAMP_LEVELS ? i : RAMP_LEVELS;
        p = THUNDER_P0 + (THUNDER_P1 - THUNDER_P0) * (k - THUNDER_FROM) / (RAMP_LEVELS - THUNDER_FROM);
        if (c->type == CL_RAIN || c->type == CL_PINK || (below && (below->thunder || below->type == CL_PINK))) p = 0;
    }
    if (p && rs_rng_range(&w->rng, 100) < p) {
        c->thunder = 1;
        c->tx = (BAND_L + 16 + rs_rng_range(&w->rng, BAND_R - BAND_L - 32)) << 16;
        int v = THUNDER_SPEED * (80 + rs_rng_range(&w->rng, 41)) / 100;
        c->tvx = rs_rng_range(&w->rng, 2) ? v : -v;
    } else if (i >= STAR_FROM && rs_rng_range(&w->rng, 100) < STAR_CHANCE) {
        c->star = 1;
        c->sx = (int16_t)(BAND_L + 32 + rs_rng_range(&w->rng, BAND_R - BAND_L - 64));
    }
}

static void gen_upto(world *w, int top)
{
    while (w->top < top) gen_level(w, w->top++);
}

/* one frame of a cloud: it slides and turns back at the band's ends; its thunder cloud drifts; a rain cloud drips,
 * melts and comes back. Returns WEV_* */
static int cloud_tick(cloud *c)
{
    int ev = 0;
    int32_t half = (int32_t)c->w << 15, lo = (BAND_L << 16) + half, hi = (BAND_R << 16) - half;
    c->x += c->vx;
    if (c->x < lo) { c->x = lo; c->vx = -c->vx; }
    if (c->x > hi) { c->x = hi; c->vx = -c->vx; }
    if (c->thunder) {
        c->tx += c->tvx;
        if (c->tx < (BAND_L + 14) << 16) { c->tx = (BAND_L + 14) << 16; c->tvx = -c->tvx; }
        if (c->tx > (BAND_R - 14) << 16) { c->tx = (BAND_R - 14) << 16; c->tvx = -c->tvx; }
    }
    if (c->gone_t >= 0) {
        if (++c->gone_t >= RAIN_REFORM) { c->gone_t = -1; c->rain_t = -1; ev |= WEV_REFORM; }
    } else if (c->rain_t >= 0) {
        if (++c->rain_t >= RAIN_MELT) { c->gone_t = 0; ev |= WEV_MELT; }
    }
    return ev;
}

/* ---- the unicorns -------------------------------------------------------------------------------------------------- */
static void hero_reset(hero *h, int x)
{
    memset(h, 0, sizeof *h);
    h->state = HS_READY;
    h->x = x << 16;
    h->ox = (x - CENTER_X) << 16;
    h->face = 1;
    h->target = 1;
    h->air_t = h->land_t = 99;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->last_special = -9;
    w->rainbow_t = -1;
    gen_upto(w, 8);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        hero_reset(&w->h[p], CENTER_X + (2 * p - (w->players - 1)) * 15);
        if (p >= w->players) w->h[p].state = HS_OFF;
    }
}

static int alive(const hero *h) { return h->state == HS_READY || h->state == HS_STAND || h->state == HS_AIR; }

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (alive(&w->h[p]) || w->h[p].state == HS_ZAP) return 1;
    return 0;
}

int world_all_down(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state != HS_DOWN) return 0;
    return 1;
}

static void score(hero *h) { h->score = h->height + h->stars * STAR_POINTS + h->rainbows * RAINBOW_BONUS; }

static void fall(hero *h, int why, int32_t vy)
{
    h->state = HS_FALL;
    h->t = 0;
    h->why = why;
    h->vy = vy;
}

static void take_off(world *w, hero *h, int p)
{
    const cloud *c = world_cloud(w, h->level);
    h->big = c && c->type == CL_PINK;
    h->vy = h->big ? BIG_JUMP_VY : JUMP_VY;
    h->target = h->level + 1 + h->big;
    h->state = HS_AIR;
    h->t = 0;
    h->air_t = 0;
    w->events[p] |= EV_JUMP | (h->big ? EV_BIG : 0);
}

/* in the air: the gaps it flies through (stars, thunder), then the landing or the miss */
static void fly(world *w, hero *h, int p)
{
    int32_t prev = h->dy;
    h->vy -= GRAVITY;
    h->dy += h->vy;
    h->air_t++;
    int alt = hero_alt(h), x = h->x >> 16;
    for (int g = h->level; g < h->target; g++) {
        cloud *gc = cloud_at(w, g);
        if (!gc) continue;
        int gy = g * LEVEL_H + GAP_Y;
        if (gc->thunder && iabs((gc->tx >> 16) - x) < 11 + 7 && alt + 24 > gy - 6 && alt + 2 < gy + 6) {
            h->state = HS_ZAP;
            h->t = 0;
            h->why = FALL_ZAP;
            w->events[p] |= EV_ZAP;
            return;
        }
        if (gc->star && !gc->star_taken && iabs(gc->sx - x) < 7 + 7 && alt + 24 > gy - 7 && alt + 2 < gy + 7) {
            gc->star_taken = 1;
            gc->star_by = (int16_t)p;
            gc->star_t = w->t;
            h->stars++;
            score(h);
            w->events[p] |= EV_STAR;
        }
    }
    int32_t tgt = (int32_t)((h->target - h->level) * LEVEL_H) << 16;
    if (h->vy < 0 && prev >= tgt && h->dy < tgt) {
        cloud *tc = cloud_at(w, h->target);
        if (tc && cloud_there(tc) && iabs((h->x - tc->x) >> 16) <= tc->w / 2 + LAND_MARGIN) {
            int old = h->level;
            h->level = h->target;
            h->dy = h->vy = 0;
            h->ox = h->x - tc->x;
            h->state = HS_STAND;
            h->t = 0;
            h->land_t = 0;
            h->height = h->level;
            w->events[p] |= EV_LAND;
            if (tc->type == CL_RAIN && tc->rain_t < 0) {
                /* the rain starts: the cloud stops, heavy, and so does its twin right above: hop up, quick! */
                cloud *up = cloud_at(w, h->level + 1);
                tc->rain_t = 0;
                tc->vx = 0;
                if (up && up->x == tc->x) up->vx = 0;
                w->events[p] |= EV_DRIP;
            }
            if (h->level / RAINBOW_EVERY > old / RAINBOW_EVERY) {
                h->rainbows++;
                w->events[p] |= EV_RAINBOW;
                int m = h->level / RAINBOW_EVERY * RAINBOW_EVERY;
                if (m > w->rainbow_level) { w->rainbow_level = m; w->rainbow_t = 0; }
            }
            score(h);
        } else {
            fall(h, FALL_MISS, h->vy);
            w->events[p] |= EV_MISS;
        }
    }
}

/* standing: it rides its cloud and steps inside it after a landing on its very end */
static void ride(world *w, hero *h, int p)
{
    const cloud *c = world_cloud(w, h->level);
    if (!c) return;
    if (!cloud_there(c)) {
        fall(h, FALL_MELT, 0);
        w->events[p] |= EV_MELT;
        return;
    }
    int32_t lim = ((int32_t)c->w / 2 - SETTLE_IN) << 16;
    if (h->ox > lim) h->ox = h->ox - Q16_ONE < lim ? lim : h->ox - Q16_ONE;
    if (h->ox < -lim) h->ox = h->ox + Q16_ONE > -lim ? -lim : h->ox + Q16_ONE;
    h->x = c->x + h->ox;
    if (c->vx) h->face = c->vx > 0 ? 1 : -1;
}

static void camera(world *w)
{
    int lo = 1 << 30, hi = -1;
    for (int p = 0; p < w->players; p++) {
        const hero *h = &w->h[p];
        if (!alive(h)) continue;
        if (h->level < lo) lo = h->level;
        if (h->level > hi) hi = h->level;
    }
    if (hi >= 0) {
        int cl = hi - CAM_LAG > lo ? hi - CAM_LAG : lo;
        if (cl > w->cam_level) w->cam_level = cl;     /* the camera never goes back down */
    }
    int32_t target = (int32_t)(w->cam_level * LEVEL_H) << 8, d = target - w->cam;
    if (d) {
        int32_t s = d * CAM_EASE / 256;
        w->cam += s ? s : (d > 0 ? 1 : -1);
    }
    gen_upto(w, (hi > w->cam_level ? hi : w->cam_level) + 8);
}

void world_step(world *w, const int press[MAX_PLAYERS])
{
    w->wevents = 0;
    for (int i = 0; i < RING; i++)
        if (world_cloud(w, w->c[i].index) == &w->c[i]) w->wevents |= cloud_tick(&w->c[i]);
    if (w->rainbow_t >= 0 && ++w->rainbow_t >= RAINBOW_SHOW) w->rainbow_t = -1;
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        w->events[p] = 0;
        h->t++;
        if (h->land_t < 99) h->land_t++;
        if (h->state != HS_AIR && h->air_t < 99) h->air_t++;
        if ((h->state == HS_READY || h->state == HS_STAND) && press[p]) {
            if (!w->started) {
                /* the first press starts the run: everyone stands ready from now on (only the presser jumps) */
                w->started = 1;
                for (int q = 0; q < w->players; q++)
                    if (w->h[q].state == HS_READY) {
                        w->h[q].state = HS_STAND;
                        w->h[q].t = 0;
                        w->events[q] |= EV_START;
                    }
            }
            take_off(w, h, p);
        }
        switch (h->state) {
        case HS_READY:
        case HS_STAND:
            ride(w, h, p);
            break;
        case HS_AIR:
            fly(w, h, p);
            break;
        case HS_ZAP:
            if (h->t >= HIT_FREEZE) fall(h, FALL_ZAP, JUMP_VY / 4);
            break;
        case HS_FALL:
            h->vy -= GRAVITY;
            h->dy += h->vy;
            if (h->t >= FALL_FRAMES || hero_alt(h) < cam_px(w) - (RS_SCREEN_H - STAND_Y) - 48) {
                h->state = HS_DOWN;
                h->t = 0;
            }
            break;
        default:
            break;
        }
    }
    camera(w);
    /* left below the screen (2-4 players: the camera follows the leader): out */
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        if (alive(h) && hero_alt(h) < cam_px(w) - OUT_BELOW) {
            h->state = HS_DOWN;
            h->t = 0;
            h->why = FALL_OUT;
            w->events[p] |= EV_OUT;
        }
    }
    w->t++;
}

/* ---- the bot: it times its jumps by playing the jump ahead on copies of the clouds -------------------------------- */
/* returns the landing's distance from the target cloud's centre (px), or -1: a zap or no cloud there */
static int foresee(const world *w, const hero *h, int *half)
{
    const cloud *c = world_cloud(w, h->level);
    if (!c) return -1;
    int big = c->type == CL_PINK, target = h->level + 1 + big;
    const cloud *tc0 = world_cloud(w, target);
    if (!tc0) return -1;
    cloud tc = *tc0, gaps[2];
    int ng = 0;
    for (int g = h->level; g < target; g++) {
        const cloud *gc = world_cloud(w, g);
        if (gc) gaps[ng++] = *gc;
    }
    int32_t dy = 0, vy = big ? BIG_JUMP_VY : JUMP_VY, tgt = (int32_t)((target - h->level) * LEVEL_H) << 16;
    int x = h->x >> 16;
    for (int n = 0; n < 200; n++) {
        cloud_tick(&tc);
        for (int i = 0; i < ng; i++) cloud_tick(&gaps[i]);
        int32_t prev = dy;
        vy -= GRAVITY;
        dy += vy;
        int alt = h->level * LEVEL_H + (dy >> 16);
        for (int i = 0; i < ng; i++) {
            int gy = gaps[i].index * LEVEL_H + GAP_Y;
            if (gaps[i].thunder && iabs((gaps[i].tx >> 16) - x) < 11 + 7 + 4 && alt + 28 > gy - 6 && alt - 2 < gy + 6)
                return -1;
        }
        if (vy < 0 && prev >= tgt && dy < tgt) {
            if (!cloud_there(&tc)) return -1;
            *half = tc.w / 2;
            return iabs((h->x - tc.x) >> 16);
        }
    }
    return -1;
}

int bot_decide(const world *w, int player, int stop, int err)
{
    const hero *h = &w->h[player];
    if (h->state != HS_READY && h->state != HS_STAND) return 0;
    if (h->state == HS_STAND && h->t < 6 + 11 * player) return 0;    /* a breath after landing (P2-P4 longer) */
    int half = 0, d = foresee(w, h, &half);
    if (stop) return d >= 0 && d > half + LAND_MARGIN + 8;   /* a sure miss (not a zap) */
    /* 2-4 players: a bot about to be left below hurries; a bot far ahead waits for the others (polite bots keep
     * the four unicorns on the screen together: the tests and the screenshots) */
    int lead = h->level, low = h->level;
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state <= HS_AIR) {
            if (w->h[p].level > lead) lead = w->h[p].level;
            if (w->h[p].level < low) low = w->h[p].level;
        }
    const cloud *c = world_cloud(w, h->level);
    if (h->level - low >= CAM_LAG && !(c && c->rain_t >= 0)) return 0;
    int safe = lead - h->level >= CAM_LAG ? -LAND_MARGIN + 2 : BOT_SAFE;
    return d >= 0 && d + err <= half - safe;
}
