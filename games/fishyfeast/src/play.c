/*
 * Fishy Feast: the rules (DESIGN.md "Rules"): the pond, the fish and their pieces, plankton, pellets, urchins, eating,
 * splitting, ejecting, merging, bursting, the decay, the bots' brains, and the geometry of the views and of the touch
 * controls. No drawing, no sound, no screen width: deterministic (integer maths, the world's own RNG), testable alone
 * (tests/test_rules.c links this file with the SDK's RNG only).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
 */
#include "game.h"
#include <string.h>

/* the bots' names: never translated (docs/i18n.md: names stay as they are) */
const char *const fish_names[BOTS] = {"BUBBLES", "FINLEY", "COOKIE", "MOCHI", "PEBBLE", "SPLASH", "WIGGLES",
                                      "PUDDING", "NOODLE", "BISCUIT", "JELLY", "GUPPY", "MARBLE", "PICKLE", "WAFFLE"};
const int16_t zoom_steps[ZOOM_STEPS] = ZOOM_STEP_TABLE;

typedef struct bot_kind {
    int react_min, react_rnd, flee, chase_max, split, forget, misjudge, prey_w, food_w, pieces, lead;
} bot_kind;
static const bot_kind KINDS[BK_COUNT] = BOT_KIND_TABLE;
static const uint8_t KIND_OF[BOTS] = BOT_KINDS_OF;
static const int16_t BOT_MASS0[BOTS] = BOT_START_MASSES;

/* a quarter of a sine, 1/256 turn steps, Q12 */
static const int16_t SINQ[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285, 1380, 1474, 1567, 1660, 1751, 1842,
    1931, 2019, 2106, 2191, 2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229,
    3290, 3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996,
    4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096};

static int isin(int a)
{
    a &= 255;
    if (a < 64) return SINQ[a];
    if (a < 128) return SINQ[128 - a];
    if (a < 192) return -SINQ[a - 128];
    return -SINQ[256 - a];
}
static int icos(int a) { return isin(a + 64); }

int isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    uint64_t x = (uint64_t)v, res = 0, bit = (uint64_t)1 << 62;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (int)res;
}

static int rnd(world *w, int n) { return rs_rng_range(&w->rng, n); }
static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

int32_t fish_radius(int32_t mass) { return isqrt64((int64_t)mass * RADIUS_K); }
int32_t fish_speed(int32_t r)
{
    int32_t v = (int32_t)((int64_t)SPEED_NUM * FX * FX / (r + SPEED_OFS * FX));
    return v < SPEED_MIN ? SPEED_MIN : v;
}
int can_eat(int32_t eater, int32_t prey) { return (int64_t)eater * EAT_DEN >= (int64_t)prey * EAT_NUM; }
static int merge_time(int32_t mass)
{
    int extra = mass / MQ * MERGE_PER_MASS;
    return MERGE_BASE + (extra > MERGE_EXTRA_MAX ? MERGE_EXTRA_MAX : extra);
}
/* how far a half of radius r shoots when it splits (its boost adds up to 256 / (256 - BOOST_KEEP) times its start) */
static int32_t split_reach(int32_t r) { return (SPLIT_V0 * FX + r / SPLIT_V_DIV) * 256 / (256 - BOOST_KEEP); }

int zoom_step_for(int32_t r, int target_px, int max_zoom)
{
    for (int i = 0; i < ZOOM_STEPS; i++) {
        if (zoom_steps[i] > max_zoom) continue;
        if (((int64_t)r * zoom_steps[i]) >> 16 <= target_px) return zoom_steps[i];
    }
    return zoom_steps[ZOOM_STEPS - 1];
}

/* ---- the pond: a rounded rectangle ------------------------------------------------------------------------------------ */
/* keep a point in the pond, `in` (Q8) from its edge */
static void pond_clamp(int32_t *x, int32_t *y, int32_t in)
{
    const int32_t W = POND_W * FX, H = POND_H * FX, C = POND_CORNER * FX;
    if (in > C - FX) in = C - FX;
    *x = clamp32(*x, in, W - in);
    *y = clamp32(*y, in, H - in);
    int32_t ccx = *x < C ? C : *x > W - C ? W - C : *x, ccy = *y < C ? C : *y > H - C ? H - C : *y;
    if (ccx != *x && ccy != *y) {                       /* in a corner: inside its circle */
        int64_t dx = *x - ccx, dy = *y - ccy;
        int32_t d = isqrt64(dx * dx + dy * dy), lim = C - in;
        if (d > lim) {
            *x = ccx + (int32_t)(dx * lim / d);
            *y = ccy + (int32_t)(dy * lim / d);
        }
    }
}

int pond_span(int y, int *x0, int *x1)
{
    if (y < 0 || y >= POND_H) return 0;
    int dy = y < POND_CORNER ? POND_CORNER - y : y >= POND_H - POND_CORNER ? y - (POND_H - POND_CORNER) + 1 : 0;
    int h = dy ? isqrt64((int64_t)POND_CORNER * POND_CORNER - (int64_t)dy * dy) : POND_CORNER;
    *x0 = POND_CORNER - h;
    *x1 = POND_W - POND_CORNER + h;
    return 1;
}

static void random_spot(world *w, int margin, int32_t *x, int32_t *y)
{
    for (int k = 0; k < 20; k++) {
        *x = (margin + rnd(w, POND_W - 2 * margin)) * FX;
        *y = (margin + rnd(w, POND_H - 2 * margin)) * FX;
        int32_t cx = *x, cy = *y;
        pond_clamp(&cx, &cy, margin * FX);
        if (cx == *x && cy == *y) return;
    }
    *x = POND_W / 2 * FX;
    *y = POND_H / 2 * FX;
}

/* ---- fish ------------------------------------------------------------------------------------------------------------ */
static int new_cell(world *w)
{
    for (int i = 0; i < MAX_CELLS; i++)
        if (!w->c[i].on) return i;
    return -1;
}

static void cell_mass(cell *c, int32_t m)
{
    if (m > MASS_CAP * MQ) m = MASS_CAP * MQ;
    c->mass = m;
    c->r = fish_radius(m);
}

static void brain_reset(owner *ow)
{
    ow->think_t = 0;
    ow->mode = BM_WANDER;
    ow->target = -1;
    ow->chase_t = ow->split_cd = 0;
    ow->ignore = -1;
    ow->ignore_t = 0;
}

/* a new fish of `mass` units for owner o, at the safest of SPAWN_TRIES spots (away from the fish that could eat it) */
static void spawn(world *w, int o, int mass)
{
    int32_t bx = POND_W / 2 * FX, by = POND_H / 2 * FX, best = -1;
    for (int k = 0; k < SPAWN_TRIES; k++) {
        int32_t x, y;
        random_spot(w, 80, &x, &y);
        int32_t safe = SPAWN_SAFE * 4 * FX;
        for (int i = 0; i < MAX_CELLS; i++) {
            const cell *c = &w->c[i];
            if (!c->on || c->owner == o || !can_eat(c->mass, mass * MQ)) continue;
            int64_t dx = (c->x - x) / FX, dy = (c->y - y) / FX;
            int32_t d = isqrt64(dx * dx + dy * dy) * FX - c->r;
            if (d < safe) safe = d;
        }
        for (int i = 0; i < URCHINS_N; i++) {          /* not right on an urchin either */
            int64_t dx = w->u[i].x - x / FX, dy = w->u[i].y - y / FX;
            if (dx * dx + dy * dy < 40 * 40) safe = 0;
        }
        if (safe > best) { best = safe; bx = x; by = y; }
        if (best >= SPAWN_SAFE * FX) break;
    }
    int i = new_cell(w);
    if (i < 0) return;
    cell *c = &w->c[i];
    memset(c, 0, sizeof *c);
    c->on = 1;
    c->x = bx;
    c->y = by;
    c->owner = (int16_t)o;
    c->face = (uint8_t)rnd(w, 2);
    cell_mass(c, mass * MQ);
    owner *ow = &w->own[o];
    ow->alive = 1;
    ow->ncells = 1;
    ow->mass = c->mass;
    ow->cx = bx;
    ow->cy = by;
    ow->dirx = ow->diry = 0;
    ow->alive_t = 0;
    ow->face = c->face;
    if (mass > ow->best) ow->best = mass;
    brain_reset(ow);
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ^ 0x9e3779b9u);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->leader = -1;
    /* urchins: spread out, away from the edges */
    for (int i = 0; i < URCHINS_N; i++) {
        int32_t bx = 0, by = 0, bd = -1;
        for (int k = 0; k < 24; k++) {
            int32_t x, y;
            random_spot(w, 140, &x, &y);
            x /= FX;
            y /= FX;
            int32_t d = 1 << 30;
            for (int j = 0; j < i; j++) {
                int32_t dx = x - w->u[j].x, dy = y - w->u[j].y, e = dx * dx + dy * dy;
                if (e < d) d = e;
            }
            if (d > bd) { bd = d; bx = x; by = y; }
        }
        w->u[i].x = (int16_t)bx;
        w->u[i].y = (int16_t)by;
    }
    for (int i = 0; i < PLANKTON_N; i++) {
        int32_t x, y;
        random_spot(w, 10, &x, &y);
        w->sp[i].x = (int16_t)(x / FX);
        w->sp[i].y = (int16_t)(y / FX);
        w->sp[i].kind = (uint8_t)rnd(w, 3);
        w->sp[i].on = 1;
    }
    for (int o = 0; o < OWNERS; o++) {
        owner *ow = &w->own[o];
        ow->eaten_by = -1;
        brain_reset(ow);
        if (o < MAX_PLAYERS) {
            ow->kind = OW_PLAYER;
            ow->pers = BK_PLAYER;
            ow->colour = (uint8_t)(8 + o);
            ow->out = o >= w->players;              /* not playing */
        } else {
            int b = o - MAX_PLAYERS;
            ow->kind = OW_BOT;
            ow->pers = KIND_OF[b];
            ow->name = (uint8_t)b;
            ow->colour = (uint8_t)(b % 8);
            spawn(w, o, BOT_MASS0[b]);
        }
    }
}

void world_start(world *w)
{
    for (int p = 0; p < w->players; p++) {
        w->own[p].out = 0;
        w->own[p].best = 0;
        spawn(w, p, START_MASS);
    }
    w->started = 0;                         /* the clock waits for the first input (ready=1 and the tests) */
    w->t = 0;
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (!w->own[p].out) return 1;
    return 0;
}

int owner_mass(const world *w, int o) { return w->own[o].alive ? w->own[o].mass / MQ : 0; }

int owner_rank(const world *w, int o)
{
    if (!w->own[o].alive) return 0;
    int r = 1;
    for (int i = 0; i < OWNERS; i++)
        if (i != o && w->own[i].alive &&
            (w->own[i].mass > w->own[o].mass || (w->own[i].mass == w->own[o].mass && i < o))) r++;
    return r;
}

int leaderboard(const world *w, int out[], int n)
{
    int k = 0;
    uint32_t used = 0;
    while (k < n) {
        int best = -1;
        for (int i = 0; i < OWNERS; i++)
            if (w->own[i].alive && !(used >> i & 1) && (best < 0 || w->own[i].mass > w->own[best].mass)) best = i;
        if (best < 0) break;
        used |= 1u << best;
        out[k++] = best;
    }
    return k;
}

/* a unit vector (length 256) from a steering input; none: where the fish looks */
static void unit_dir(int dx, int dy, int face, int32_t *ux, int32_t *uy)
{
    int64_t d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
    if (d < 32) {
        *ux = face ? -256 : 256;
        *uy = 0;
        return;
    }
    *ux = (int32_t)(dx * 256 / d);
    *uy = (int32_t)(dy * 256 / d);
}

int owner_split(world *w, int o, int dx, int dy)
{
    owner *ow = &w->own[o];
    if (!ow->alive) return 0;
    int32_t ux, uy;
    unit_dir(dx, dy, ow->face, &ux, &uy);
    int list[MAX_CELLS], n = 0, made = 0, count = 0;
    for (int i = 0; i < MAX_CELLS; i++)
        if (w->c[i].on && w->c[i].owner == o) list[n++] = i;
    count = n;
    for (int k = 0; k < n && count < SPLIT_MAX; k++) {
        cell *c = &w->c[list[k]];
        if (c->mass < SPLIT_MIN * MQ) continue;
        int j = new_cell(w);
        if (j < 0) break;
        cell *h = &w->c[j];
        int32_t half = c->mass / 2;
        cell_mass(c, c->mass - half);
        *h = *c;
        cell_mass(h, half);
        int32_t off = c->r + h->r / 2;
        h->x = c->x + (int32_t)((int64_t)ux * off / 256);
        h->y = c->y + (int32_t)((int64_t)uy * off / 256);
        pond_clamp(&h->x, &h->y, h->r / 2);
        int32_t v0 = SPLIT_V0 * FX + h->r / SPLIT_V_DIV;
        h->bx = ux * v0 / 256;
        h->by = uy * v0 / 256;
        h->age = 0;
        h->immune = 0;
        c->merge_t = h->merge_t = (int16_t)merge_time(half);
        count++;
        made++;
    }
    if (made) {
        ow->ncells = (int16_t)count;
        w->splits++;
    }
    return made;
}

int owner_eject(world *w, int o, int dx, int dy)
{
    owner *ow = &w->own[o];
    if (!ow->alive) return 0;
    int32_t ux, uy;
    unit_dir(dx, dy, ow->face, &ux, &uy);
    int made = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (!c->on || c->owner != o || c->mass < EJECT_MIN * MQ) continue;
        int j = -1, oldest = 0;
        for (int k = 0; k < MAX_PELLETS; k++) {
            if (!w->pl[k].on) { j = k; break; }
            if (w->pl[k].t > w->pl[oldest].t) oldest = k;
        }
        if (j < 0) j = oldest;                  /* the pond is full of pellets: the oldest dissolves */
        pellet *p = &w->pl[j];
        cell_mass(c, c->mass - EJECT_COST * MQ);
        int32_t off = c->r + 6 * FX;
        p->on = 1;
        p->x = c->x + (int32_t)((int64_t)ux * off / 256);
        p->y = c->y + (int32_t)((int64_t)uy * off / 256);
        pond_clamp(&p->x, &p->y, 4 * FX);
        p->bx = ux * EJECT_V0;
        p->by = uy * EJECT_V0;
        p->owner = (int16_t)o;
        p->t = 0;
        p->colour = ow->colour;
        p->life = PELLET_LIFE;
        made++;
    }
    return made;
}

static void ev(world *w, int o, int e)
{
    if (o >= 0 && o < MAX_PLAYERS) w->events[o] |= e;
}

static void fx_add(world *w, int32_t x, int32_t y, int32_t r, int kind)
{
    if (w->nfx >= (int)(sizeof w->fx / sizeof w->fx[0])) return;
    w->fx[w->nfx].x = (int16_t)(x / FX);
    w->fx[w->nfx].y = (int16_t)(y / FX);
    w->fx[w->nfx].r = (int16_t)(r / FX);
    w->fx[w->nfx].kind = (uint8_t)kind;
    w->nfx++;
}

/* the cell i bursts on urchin u: into up to BURST_PIECES pieces flying outwards */
static void burst(world *w, int i, int u)
{
    cell *c = &w->c[i];
    owner *ow = &w->own[c->owner];
    int free = SPLIT_MAX - ow->ncells, n = free + 1 < BURST_PIECES ? free + 1 : BURST_PIECES, pool = 0;
    for (int j = 0; j < MAX_CELLS; j++) pool += !w->c[j].on;
    if (n > pool + 1) n = pool + 1;             /* the cell pool is (almost) full: fewer pieces, no mass lost */
    int32_t ux = c->x - w->u[u].x * FX, uy = c->y - w->u[u].y * FX;
    w->u[u].shake = 30;
    fx_add(w, c->x, c->y, c->r, FX_BURST);
    if (n < 2) {                                /* in 16 pieces already: bounced off */
        int64_t d = isqrt64((int64_t)ux * ux + (int64_t)uy * uy);
        if (d) {
            c->bx = (int32_t)(ux * 3 * FX / d);
            c->by = (int32_t)(uy * 3 * FX / d);
        }
        c->immune = 40;
        return;
    }
    int32_t piece = c->mass / n;
    int a0 = rnd(w, 256);
    cell_mass(c, c->mass - piece * (n - 1));
    for (int k = 0; k < n; k++) {
        cell *p = c;
        if (k) {
            int j = new_cell(w);
            if (j < 0) break;
            p = &w->c[j];
            *p = *c;
            cell_mass(p, piece);
            ow->ncells++;
        }
        int a = a0 + 256 * k / n;
        p->bx = icos(a) * BURST_V0 * FX / 4096;
        p->by = isin(a) * BURST_V0 * FX / 4096;
        p->x += icos(a) * (p->r / FX) / 64;
        p->y += isin(a) * (p->r / FX) / 64;
        pond_clamp(&p->x, &p->y, p->r / 2);
        p->merge_t = (int16_t)merge_time(p->mass);
        p->immune = 40;
        p->age = 0;
    }
    w->bursts++;
    ev(w, c->owner, EV_BURST);
}

/* cell e eats cell f (another fish's piece) */
static void eat(world *w, int e, int f)
{
    cell *a = &w->c[e], *b = &w->c[f];
    fx_add(w, b->x, b->y, b->r, FX_EAT);
    cell_mass(a, a->mass + b->mass);
    b->on = 0;
    owner *bo = &w->own[b->owner];
    bo->ncells--;
    bo->eaten_by = a->owner;
    w->eats++;
    ev(w, a->owner, EV_EAT);
    ev(w, b->owner, EV_LOST);
    if (bo->ncells <= 0) w->own[a->owner].kills++;
}

static void move_cells(world *w)
{
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (!c->on) continue;
        const owner *ow = &w->own[c->owner];
        int32_t sp = fish_speed(c->r);
        int32_t tx = ow->dirx * sp / 256, ty = ow->diry * sp / 256;
        if (ow->ncells > 1) {                   /* the pieces stay loosely together */
            tx += clamp32((ow->cx - c->x) / COHESION_DIV, -COHESION_MAX, COHESION_MAX);
            ty += clamp32((ow->cy - c->y) / COHESION_DIV, -COHESION_MAX, COHESION_MAX);
        }
        c->vx += (tx - c->vx) / ACCEL_DIV;
        c->vy += (ty - c->vy) / ACCEL_DIV;
        c->x += c->vx + c->bx;
        c->y += c->vy + c->by;
        c->bx = c->bx * BOOST_KEEP / 256;
        c->by = c->by * BOOST_KEEP / 256;
        if (c->bx > -8 && c->bx < 8) c->bx = 0;
        if (c->by > -8 && c->by < 8) c->by = 0;
        int32_t hx = c->vx + c->bx;
        if (hx < -40) c->face = 1;
        else if (hx > 40) c->face = 0;
        pond_clamp(&c->x, &c->y, c->r / 2);
        if (c->merge_t > 0) c->merge_t--;
        if (c->immune) c->immune--;
        if (c->age < 60000) c->age++;
    }
}

static void move_pellets(world *w)
{
    for (int i = 0; i < MAX_PELLETS; i++) {
        pellet *p = &w->pl[i];
        if (!p->on) continue;
        p->x += p->bx;
        p->y += p->by;
        p->bx = p->bx * BOOST_KEEP / 256;
        p->by = p->by * BOOST_KEEP / 256;
        pond_clamp(&p->x, &p->y, 4 * FX);
        if (p->t < 30000) p->t++;
        if (p->life && !--p->life) p->on = 0;
    }
}

static void eat_food(world *w)
{
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (!c->on) continue;
        int32_t cx = c->x / FX, cy = c->y / FX, r = c->r / FX + 1, r2 = r * r;
        for (int k = 0; k < PLANKTON_N; k++) {
            speck *s = &w->sp[k];
            if (!s->on) continue;
            int32_t dx = s->x - cx, dy = s->y - cy;
            if (dx > r || dx < -r || dy > r || dy < -r || dx * dx + dy * dy > r2) continue;
            s->on = 0;
            s->regrow = (uint16_t)(PLANKTON_REGROW + rnd(w, PLANKTON_REGROW_RND));
            cell_mass(c, c->mass + MQ);
            w->own[c->owner].eats++;
            ev(w, c->owner, EV_PLANKTON);
        }
        for (int k = 0; k < MAX_PELLETS; k++) {
            pellet *p = &w->pl[k];
            if (!p->on || (p->owner == c->owner && p->t < EJECT_OWN_DELAY)) continue;
            int32_t dx = (p->x - c->x) / FX, dy = (p->y - c->y) / FX;
            if (dx > r || dx < -r || dy > r || dy < -r || dx * dx + dy * dy > r2) continue;
            p->on = 0;
            cell_mass(c, c->mass + EJECT_MASS * MQ);
            ev(w, c->owner, EV_PELLET);
        }
    }
}

/* pieces of one fish push apart until they may merge, then merge; pieces of different fish eat each other */
static void touch_cells(world *w)
{
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *a = &w->c[i];
        if (!a->on) continue;
        for (int j = i + 1; j < MAX_CELLS && a->on; j++) {
            cell *b = &w->c[j];
            if (!b->on) continue;
            int32_t dx = b->x - a->x, dy = b->y - a->y, rs = a->r + b->r;
            if (dx >= rs || dx <= -rs || dy >= rs || dy <= -rs) continue;
            int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
            if (d2 >= (int64_t)rs * rs) continue;
            int32_t d = isqrt64(d2);
            if (a->owner == b->owner) {
                if (!a->merge_t && !b->merge_t) {
                    cell *big = a->mass >= b->mass ? a : b, *sm = big == a ? b : a;
                    if (d <= big->r) {
                        int64_t m = (int64_t)big->mass + sm->mass;
                        big->x = (int32_t)(((int64_t)big->x * big->mass + (int64_t)sm->x * sm->mass) / m);
                        big->y = (int32_t)(((int64_t)big->y * big->mass + (int64_t)sm->y * sm->mass) / m);
                        cell_mass(big, (int32_t)m);
                        fx_add(w, sm->x, sm->y, sm->r, FX_MERGE);
                        sm->on = 0;
                        w->own[a->owner].ncells--;
                        w->merges++;
                        ev(w, a->owner, EV_MERGE);
                    }
                    continue;
                }
                /* still apart: a soft push, the lighter piece moves more */
                int32_t ov = rs - d, m = a->mass + b->mass;
                int32_t ux = d ? (int32_t)((int64_t)dx * 256 / d) : 256, uy = d ? (int32_t)((int64_t)dy * 256 / d) : 0;
                int32_t pa = (int32_t)((int64_t)ov * b->mass / m) / 2, pb = (int32_t)((int64_t)ov * a->mass / m) / 2;
                a->x -= (int32_t)((int64_t)ux * pa / 256);
                a->y -= (int32_t)((int64_t)uy * pa / 256);
                b->x += (int32_t)((int64_t)ux * pb / 256);
                b->y += (int32_t)((int64_t)uy * pb / 256);
                pond_clamp(&a->x, &a->y, a->r / 2);     /* not pushed out of the pond */
                pond_clamp(&b->x, &b->y, b->r / 2);
                continue;
            }
            if (can_eat(a->mass, b->mass) && d <= a->r - b->r * EAT_COVER / 100) eat(w, i, j);
            else if (can_eat(b->mass, a->mass) && d <= b->r - a->r * EAT_COVER / 100) eat(w, j, i);
        }
    }
}

static void urchins(world *w)
{
    const int32_t ur = fish_radius(URCHIN_MASS * MQ);
    for (int k = 0; k < URCHINS_N; k++)
        if (w->u[k].shake) w->u[k].shake--;
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (!c->on || c->mass <= URCHIN_MASS * MQ || c->immune) continue;
        for (int k = 0; k < URCHINS_N; k++) {
            int64_t dx = c->x - w->u[k].x * FX, dy = c->y - w->u[k].y * FX;
            int64_t t = c->r + (int64_t)ur * URCHIN_TOUCH / 100;
            if (dx * dx + dy * dy < t * t) {
                burst(w, i, k);
                break;
            }
        }
    }
}

static void decay(world *w)
{
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (!c->on) continue;
        const owner *ow = &w->own[c->owner];
        int ppm = 0;
        if (ow->mass > DECAY_FROM * MQ) ppm += DECAY_PPM;
        if (ow->kind == OW_BOT && ow->mass > BOT_CAP * MQ) ppm += BOT_CAP_PPM;
        if (ppm) cell_mass(c, c->mass - (int32_t)((int64_t)c->mass * ppm / 1000000));
    }
}

static void totals(world *w)
{
    int64_t sx[OWNERS] = {0}, sy[OWNERS] = {0}, sm[OWNERS] = {0};
    int n[OWNERS] = {0}, big[OWNERS];
    for (int o = 0; o < OWNERS; o++) big[o] = -1;
    for (int i = 0; i < MAX_CELLS; i++) {
        const cell *c = &w->c[i];
        if (!c->on) continue;
        int o = c->owner;
        n[o]++;
        sm[o] += c->mass;
        sx[o] += (int64_t)c->x * (c->mass / 64);
        sy[o] += (int64_t)c->y * (c->mass / 64);
        if (big[o] < 0 || c->mass > w->c[big[o]].mass) big[o] = i;
    }
    int leader = -1;
    for (int o = 0; o < OWNERS; o++) {
        owner *ow = &w->own[o];
        ow->ncells = (int16_t)n[o];
        if (n[o]) {
            ow->mass = (int32_t)sm[o];
            int64_t q = sm[o] / 64;
            if (q > 0) {
                ow->cx = (int32_t)(sx[o] / q);
                ow->cy = (int32_t)(sy[o] / q);
            }
            ow->face = w->c[big[o]].face;
            if (ow->mass / MQ > ow->best) ow->best = ow->mass / MQ;
            ow->alive_t++;
            if (leader < 0 || ow->mass > w->own[leader].mass) leader = o;
            continue;
        }
        if (ow->alive) {                        /* its last piece was eaten */
            ow->alive = 0;
            ow->mass = 0;
            if (ow->kind == OW_PLAYER) {
                ow->out = 1;
                ev(w, o, EV_OUT);
            } else {
                ow->respawn_t = (int16_t)(BOT_RESPAWN + rnd(w, BOT_RESPAWN_RND));
            }
        } else if (ow->kind == OW_BOT && ow->respawn_t > 0 && !--ow->respawn_t) {
            spawn(w, o, START_MASS + rnd(w, BOT_RESPAWN_MASS));
            if (!ow->alive) ow->respawn_t = 1;  /* the cell pool was full: again next frame */
        }
    }
    if (leader != w->leader && leader >= 0 && leader < MAX_PLAYERS && w->own[leader].mass >= TOP_MIN_MASS * MQ) {
        ev(w, leader, EV_TOP);
        w->msg_top[leader] = TOP_MSG_FRAMES;
    }
    w->leader = leader;
}

static void regrow(world *w)
{
    for (int k = 0; k < PLANKTON_N; k++) {
        speck *s = &w->sp[k];
        if (s->on || --s->regrow) continue;
        int32_t x, y;
        random_spot(w, 10, &x, &y);
        s->x = (int16_t)(x / FX);
        s->y = (int16_t)(y / FX);
        s->on = 1;
    }
}

void world_step(world *w, const pin in[MAX_PLAYERS])
{
    memset(w->events, 0, sizeof w->events);
    w->nfx = 0;
    if (!w->started) {
        int any = 0;
        for (int p = 0; p < w->players; p++)
            if (w->own[p].alive) any |= in[p].any;
        int waiting = 0;
        for (int p = 0; p < w->players; p++) waiting |= w->own[p].alive;
        if (waiting && !any) return;            /* the players' fish wait for the first press */
        w->started = 1;
        if (waiting)
            for (int p = 0; p < w->players; p++) ev(w, p, EV_START);
    }
    w->t++;
    for (int p = 0; p < MAX_PLAYERS; p++)
        if (w->msg_top[p] > 0) w->msg_top[p]--;
    for (int o = 0; o < OWNERS; o++) {
        owner *ow = &w->own[o];
        if (!ow->alive) continue;
        pin b;
        const pin *p = &b;
        if (o < MAX_PLAYERS) {
            p = &in[o];
        } else {
            bot_think(w, o, 0, &b);
        }
        int dx = p->dx, dy = p->dy;
        int64_t d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
        if (d > 256) {
            dx = (int)(dx * 256 / d);
            dy = (int)(dy * 256 / d);
        }
        ow->dirx = (int16_t)dx;
        ow->diry = (int16_t)dy;
        if (p->split && owner_split(w, o, dx, dy)) ev(w, o, EV_SPLIT);
        if (p->eject && owner_eject(w, o, dx, dy)) ev(w, o, EV_EJECT);
    }
    move_cells(w);
    move_pellets(w);
    eat_food(w);
    touch_cells(w);
    urchins(w);
    decay(w);
    totals(w);
    regrow(w);
}

/* ---- the bots' brains --------------------------------------------------------------------------------------------------
 * Every few frames (their reaction time) a bot looks around (what a player of its size would see), with its sizes a
 * little off (misjudge), and picks: FLEE the fish that could eat one of its pieces (sooner for the timid ones; a small
 * one runs for the shelter of an urchin), CHASE a fish it can eat (the hunters; the opportunists like split pieces;
 * not for ever: chase_max, then it gives up on that one), else GRAZE plankton and pellets, else wander. Pieces that
 * would burst keep away from the urchins, unless it forgets this time. The hunters (and the opportunists) sometimes
 * split at a prey in reach, aiming a little off. Between decisions it keeps its heading: it reacts late. Players are
 * left alone in their first seconds, a very small player only when it swims right by, and only one or two bots hunt
 * the same player at once. */
static int32_t perceived(int32_t m, uint32_t seed, int i, int mis)
{
    if (!mis) return m;
    uint32_t h = seed ^ ((uint32_t)i * 2654435761u);
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    int e = (int)(h % (uint32_t)(2 * mis + 1)) - mis;
    return (int32_t)((int64_t)m * (100 + e) / 100);
}

static void normalise(int64_t *x, int64_t *y, int len)
{
    int64_t d = isqrt64(*x * *x + *y * *y);
    if (!d) return;
    *x = *x * len / d;
    *y = *y * len / d;
}

static int chasers_of(const world *w, int p, int except)
{
    int n = 0;
    for (int o = MAX_PLAYERS; o < OWNERS; o++)
        if (o != except && w->own[o].alive && w->own[o].mode == BM_CHASE && w->own[o].target == p) n++;
    return n;
}

void bot_think(world *w, int o, int skill, pin *out)
{
    owner *ow = &w->own[o];
    memset(out, 0, sizeof *out);
    if (!ow->alive) return;
    if (ow->split_cd > 0) ow->split_cd--;
    if (ow->ignore_t > 0) ow->ignore_t--;
    if (ow->think_t > 0) {
        ow->think_t--;
        out->dx = ow->dirx;
        out->dy = ow->diry;
        return;
    }
    const bot_kind *k = &KINDS[ow->pers];
    ow->think_t = (int16_t)(k->react_min + rnd(w, k->react_rnd + 1) + skill * 2);
    int mis = k->misjudge + skill * 3, forget = k->forget + skill * 3;
    uint32_t seed = rs_rng_next(&w->rng);
    int mine[SPLIT_MAX], nm = 0;
    int32_t mbig = 0;
    for (int i = 0; i < MAX_CELLS && nm < SPLIT_MAX; i++)
        if (w->c[i].on && w->c[i].owner == o) {
            mine[nm++] = i;
            if (w->c[i].mass > mbig) mbig = w->c[i].mass;
        }
    if (!nm) return;
    const int32_t cx = ow->cx, cy = ow->cy, rt = fish_radius(ow->mass);
    const int z = zoom_step_for(rt, BOT_VIEW_PX, 256);
    const int32_t vw = BOT_VIEW_HW * FX * 256 / z, vh = BOT_VIEW_HH * FX * 256 / z;
    const int32_t ur = fish_radius(URCHIN_MASS * MQ);

    /* threats and prey */
    int64_t fx = 0, fy = 0;
    int fear = 0, prey = -1, prey_c = -1, prey_owner = -1;
    int32_t prey_d = 0, prey_m = 0;
    int64_t prey_v = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        const cell *x = &w->c[i];
        if (!x->on || x->owner == o) continue;
        int32_t dx = x->x - cx, dy = x->y - cy;
        if (dx > vw + x->r || dx < -vw - x->r || dy > vh + x->r || dy < -vh - x->r) continue;
        const owner *xo = &w->own[x->owner];
        int32_t pm = perceived(x->mass, seed, i, mis);
        int splitter = xo->kind == OW_PLAYER || xo->pers == BK_HUNTER || xo->pers == BK_OPPORTUNIST;
        int near_c = -1;
        int32_t near_d = 0;
        for (int m = 0; m < nm; m++) {
            const cell *c = &w->c[mine[m]];
            int64_t ddx = c->x - x->x, ddy = c->y - x->y;
            int32_t d = isqrt64(ddx * ddx + ddy * ddy);
            if (can_eat(pm, c->mass)) {         /* it could eat this piece of mine */
                int32_t edge = d - x->r - c->r;
                int32_t range = x->r * k->flee / 100 + FLEE_PAD * FX;
                if (splitter && can_eat(pm / 2, c->mass)) {
                    int32_t reach = split_reach(fish_radius(pm / 2));
                    if (reach > range) range = reach;
                }
                if (edge < range) {
                    int f = (int)((int64_t)(range - edge) * 256 / range);
                    if (f > 256) f = 256;
                    if (f > fear) fear = f;
                    if (d > 0) {
                        fx += ddx * 256 / d * f;
                        fy += ddy * 256 / d * f;
                    }
                }
            } else if (can_eat(c->mass, pm) && (near_c < 0 || d < near_d)) {
                near_c = mine[m];
                near_d = d;
            }
        }
        if (near_c < 0) continue;
        if (x->owner == ow->ignore && ow->ignore_t > 0) continue;
        if (xo->kind == OW_PLAYER && o >= MAX_PLAYERS) {    /* fair play with the players */
            if (w->t < PLAYER_GRACE) continue;
            if (xo->mass < NEWBIE_MASS * MQ && near_d > (NEWBIE_NEAR + 0) * FX + w->c[near_c].r) continue;
            int cap = xo->mass < CHASERS_MASS * MQ ? CHASERS_SMALL : CHASERS_BIG;
            if (!(ow->mode == BM_CHASE && ow->target == x->owner) && chasers_of(w, x->owner, o) >= cap) continue;
        }
        int64_t v = (int64_t)(x->mass / 16) * k->prey_w * 16 / (near_d / FX + 40);
        if (xo->ncells > 1) v = v * k->pieces / 100;
        if (v > prey_v) {
            prey_v = v;
            prey = i;
            prey_c = near_c;
            prey_d = near_d;
            prey_m = pm;
            prey_owner = x->owner;
        }
    }

    /* food: plankton and pellets in view */
    int64_t food_v = 0;
    int32_t food_x = 0, food_y = 0;
    for (int i = 0; i < PLANKTON_N; i++) {
        const speck *s = &w->sp[i];
        if (!s->on) continue;
        int32_t dx = s->x * FX - cx, dy = s->y * FX - cy;
        if (dx > vw || dx < -vw || dy > vh || dy < -vh) continue;
        int32_t d = isqrt64((int64_t)(dx / FX) * (dx / FX) + (int64_t)(dy / FX) * (dy / FX));
        int64_t v = (int64_t)MQ * k->food_w / (d + 30);
        if (v > food_v) { food_v = v; food_x = s->x * FX; food_y = s->y * FX; }
    }
    for (int i = 0; i < MAX_PELLETS; i++) {
        const pellet *p = &w->pl[i];
        if (!p->on || (p->owner == o && p->t < EJECT_OWN_DELAY)) continue;
        int32_t dx = p->x - cx, dy = p->y - cy;
        if (dx > vw || dx < -vw || dy > vh || dy < -vh) continue;
        int32_t d = isqrt64((int64_t)(dx / FX) * (dx / FX) + (int64_t)(dy / FX) * (dy / FX));
        int64_t v = (int64_t)EJECT_MASS * MQ * k->food_w / (d + 30);
        if (v > food_v) { food_v = v; food_x = p->x; food_y = p->y; }
    }

    int64_t ax = 0, ay = 0;
    int mode = BM_WANDER, target = -1, split = 0;
    if (fear >= FLEE_MIN) {
        mode = BM_FLEE;
        ax = fx;
        ay = fy;
        normalise(&ax, &ay, 256);
        if (mbig < URCHIN_MASS * MQ) {          /* small enough to hide among the spikes: run for an urchin */
            int best = -1;
            int64_t bd = (int64_t)220 * 220;
            for (int u = 0; u < URCHINS_N; u++) {
                int64_t dx = w->u[u].x - cx / FX, dy = w->u[u].y - cy / FX, d2 = dx * dx + dy * dy;
                if (d2 < bd && dx * ax + dy * ay > 0) { bd = d2; best = u; }
            }
            if (best >= 0) {
                int64_t hx = w->u[best].x * FX - cx, hy = w->u[best].y * FX - cy;
                normalise(&hx, &hy, 180);
                ax += hx;
                ay += hy;
            }
        }
    } else if (prey >= 0 && prey_v * 4 >= food_v * 3) {
        const cell *x = &w->c[prey];
        int same = ow->mode == BM_CHASE && ow->target == prey_owner;
        ow->chase_t = (int16_t)(same ? ow->chase_t + ow->think_t + 1 : 0);
        if (ow->chase_t > k->chase_max) {       /* it got away: give up on that one for a while */
            ow->ignore = (int16_t)prey_owner;
            ow->ignore_t = IGNORE_FRAMES;
            ow->chase_t = 0;
        } else {
            mode = BM_CHASE;
            target = prey_owner;
            ax = x->x + (x->vx + x->bx) * k->lead - cx;
            ay = x->y + (x->vy + x->by) * k->lead - cy;
            const cell *c = &w->c[prey_c];
            int cap = ow->kind == OW_PLAYER ? 4 : BOT_SPLIT_PIECES;
            if (k->split && !ow->split_cd && nm <= cap && fear < 16 && c->mass >= SPLIT_MIN * MQ &&
                can_eat(c->mass / 2, prey_m)) {
                int32_t rh = fish_radius(c->mass / 2);
                if (prey_d - rh < split_reach(rh) * 85 / 100 && rnd(w, 100) < k->split) {
                    split = 1;
                    ow->split_cd = BOT_SPLIT_CD;
                    ax = x->x - c->x;           /* straight at it (the wobble below still applies) */
                    ay = x->y - c->y;
                }
            }
            normalise(&ax, &ay, 256);
        }
    }
    if (mode == BM_WANDER) {
        if (food_v > 0) {
            mode = BM_GRAZE;
            ax = food_x - cx;
            ay = food_y - cy;
            normalise(&ax, &ay, 256);
        } else {                                /* nothing in view: drift on, turning a little */
            int a = rnd(w, 49) - 24;
            ax = ow->dirx;
            ay = ow->diry;
            if (!ax && !ay) {
                int b = rnd(w, 256);
                ax = icos(b) / 16;
                ay = isin(b) / 16;
            }
            int64_t rx = (ax * icos(a) - ay * isin(a)) / 4096, ry = (ax * isin(a) + ay * icos(a)) / 4096;
            ax = rx;
            ay = ry;
            normalise(&ax, &ay, 256);
        }
    }
    /* the urchins: the pieces that would burst keep away (unless it forgets, this time) */
    if (rnd(w, 100) >= forget) {
        for (int m = 0; m < nm; m++) {
            const cell *c = &w->c[mine[m]];
            if (c->mass <= URCHIN_MASS * MQ) continue;
            for (int u = 0; u < URCHINS_N; u++) {
                int64_t dx = c->x - w->u[u].x * FX, dy = c->y - w->u[u].y * FX;
                int32_t d = isqrt64(dx * dx + dy * dy), edge = d - c->r - ur / 2, keep = 56 * FX;
                if (edge < keep && d > 0) {
                    int64_t f = (int64_t)(keep - edge) * 512 / keep;
                    ax += dx * f / d;
                    ay += dy * f / d;
                }
            }
        }
    }
    /* the pond's edge */
    {
        int32_t px = cx / FX, py = cy / FX, mg = 40 + rt / FX;
        if (px < mg) ax += (mg - px) * 384 / mg;
        if (px > POND_W - mg) ax -= (px - (POND_W - mg)) * 384 / mg;
        if (py < mg) ay += (mg - py) * 384 / mg;
        if (py > POND_H - mg) ay -= (py - (POND_H - mg)) * 384 / mg;
    }
    normalise(&ax, &ay, 256);
    int noise = AIM_NOISE + 2 * skill, a = rnd(w, 2 * noise + 1) - noise;
    int64_t rx = (ax * icos(a) - ay * isin(a)) / 4096, ry = (ax * isin(a) + ay * icos(a)) / 4096;
    ow->dirx = (int16_t)clamp32((int32_t)rx, -256, 256);
    ow->diry = (int16_t)clamp32((int32_t)ry, -256, 256);
    if (mode != BM_CHASE) ow->chase_t = 0;
    ow->mode = (int16_t)mode;
    ow->target = (int16_t)target;
    out->dx = ow->dirx;
    out->dy = ow->diry;
    out->split = (uint8_t)split;
}

/* ---- the views (split screen) --------------------------------------------------------------------------------------- */
int view_layout(int players, int left, int width, vrect out[4])
{
    const int D = 2, H = 240, hw = (width - D) / 2, hh = (H - D) / 2, x2 = left + width - hw, y2 = H - hh;
    if (players <= 1) {
        out[0] = (vrect){left, 0, width, H};
        return 1;
    }
    if (players == 2) {
        out[0] = (vrect){left, 0, hw, H};
        out[1] = (vrect){x2, 0, hw, H};
        return 2;
    }
    out[0] = (vrect){left, 0, hw, hh};
    out[1] = (vrect){x2, 0, hw, hh};
    out[2] = (vrect){left, y2, hw, hh};
    out[3] = (vrect){x2, y2, hw, hh};
    return 4;                                   /* 3 players: the 4th quarter shows the whole pond */
}

/* a tall screen (stage lines top .. bottom - 1): the views at the stage's top and bottom edges reach the screen's */
void views_tall(vrect *v, int n, int top, int bottom)
{
    for (int i = 0; i < n; i++) {
        int y0 = v[i].y == 0 ? top : v[i].y, y1 = v[i].y + v[i].h == 240 ? bottom : v[i].y + v[i].h;
        v[i].y = y0;
        v[i].h = y1 - y0;
    }
}

int view_kind(int players) { return players <= 1 ? VK_FULL : players == 2 ? VK_HALF : VK_QUAD; }
int view_target_px(int kind) { return kind == VK_FULL ? ZOOM_TARGET_FULL : kind == VK_HALF ? ZOOM_TARGET_HALF : ZOOM_TARGET_QUAD; }

/* ---- touch, player 1 ------------------------------------------------------------------------------------------------ */
void touch_buttons(const vrect *v, tbutton *split, tbutton *eject)
{
    *split = (tbutton){v->x + v->w - BTN_SPLIT_DX, v->y + v->h - BTN_SPLIT_DY, BTN_SPLIT_R};
    *eject = (tbutton){v->x + v->w - BTN_EJECT_DX, v->y + v->h - BTN_EJECT_DY, BTN_EJECT_R};
}

int touch_on_button(const tbutton *b, int x, int y)
{
    int dx = x - b->x, dy = y - b->y, r = b->r + BTN_SLOP;
    return dx * dx + dy * dy <= r * r;
}

int touch_stick_zone(const vrect *v, int x, int y)
{
    return x >= v->x && y >= v->y && y < v->y + v->h && x < v->x + v->w * STICK_ZONE_PCT / 100;
}

void touch_stick(int x0, int y0, int x, int y, int16_t *dx, int16_t *dy)
{
    int ddx = x - x0, ddy = y - y0, d = isqrt64((int64_t)ddx * ddx + (int64_t)ddy * ddy);
    if (d < STICK_DEAD) {
        *dx = *dy = 0;
        return;
    }
    int mag = d >= STICK_R ? 256 : (d - STICK_DEAD) * 256 / (STICK_R - STICK_DEAD);
    if (mag < 48) mag = 48;
    *dx = (int16_t)(ddx * mag / d);
    *dy = (int16_t)(ddy * mag / d);
}
