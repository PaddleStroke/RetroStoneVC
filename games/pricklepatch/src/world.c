/*
 * Prickle Patch: the rules. The gardener walks; the tools work by themselves; the pests come from every side, more and
 * tougher with the minutes, three bosses at 5:00, 10:00 and 15:00 (the Crow King: beat it and the garden is saved).
 * Pests drop gems (XP); each level offers three choices: a new tool or passive, or a level of one owned; a tool at its
 * top level with its passive evolves. No drawing, no sound, no SDK state but the RNG functions on the world's own RNG:
 * deterministic, and the same at every screen size.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include <string.h>

/* ---- tables ------------------------------------------------------------------------------------------------------- */
/*                                     hp  speed dmg r  xp knock  art */
const enemy_info EN_INFO[ET_COUNT] = {
    [ET_SLUG]     = {   8,  7,  5,  6, 1, 8, SF_SLUG},
    [ET_APHID]    = {   5, 11,  3,  4, 1, 8, SF_APHID},
    [ET_BEETLE]   = {  26,  7,  7,  7, 2, 3, SF_BEETLE},
    [ET_ANT]      = {  12, 15,  6,  6, 1, 6, SF_ANT},
    [ET_WASP]     = {  18, 13,  7,  6, 2, 6, SF_WASP},
    [ET_THISTLE]  = {  60,  5, 10,  8, 4, 2, SF_THISTLE},
    [ET_CROW]     = {  40, 18,  9,  9, 3, 4, SF_CROW},
    [ET_LOCUST]   = {  28, 14,  8,  7, 3, 5, SF_LOCUST},
    [ET_QUEEN]    = {2500,  8, 14, 18, 0, 0, SF_QUEEN},
    [ET_STAG]     = {9000,  7, 18, 18, 0, 0, SF_STAG},
    [ET_CROWKING] = {36000, 12, 22, 18, 0, 0, SF_CROWKING},
};
const uint8_t EVOLVE_WITH[W_COUNT] = {P_POUCH, P_HAT, P_HONEY, P_SUNDIAL, P_MAGNET, P_COMPOST, P_HONEY, P_BOOTS, P_CLOVER};

/* the spawner, minute by minute: how many pests at once, and the mix (weights of slug .. locust) */
typedef struct wave { int16_t target; uint8_t mix[8]; } wave;
static const wave WAVES[16] = {
    { 12, {10, 0, 0, 0, 0, 0, 0, 0}},
    { 26, { 7, 5, 0, 0, 0, 0, 0, 0}},
    { 40, { 5, 5, 3, 0, 0, 0, 0, 0}},
    { 55, { 4, 4, 4, 3, 0, 0, 0, 0}},
    { 70, { 3, 4, 4, 4, 2, 0, 0, 0}},
    { 85, { 2, 4, 4, 3, 3, 1, 0, 0}},
    {100, { 2, 3, 4, 3, 4, 2, 0, 0}},
    {120, { 1, 4, 3, 3, 4, 2, 1, 0}},
    {140, { 1, 3, 3, 3, 4, 3, 2, 1}},
    {160, { 0, 3, 3, 3, 4, 3, 2, 2}},
    {180, { 0, 3, 3, 2, 3, 3, 3, 3}},
    {200, { 0, 2, 3, 2, 3, 3, 4, 3}},
    {220, { 0, 2, 2, 2, 3, 4, 4, 4}},
    {240, { 0, 2, 2, 2, 3, 4, 4, 5}},
    {260, { 0, 2, 2, 1, 3, 4, 5, 5}},
    {280, { 0, 2, 2, 1, 3, 4, 5, 6}},
};
/* the spawner's events: at this second, what (a ring of pests round the gardener, a column marching across, an elite
 * carrying a chest, a boss) */
enum { EVK_RING, EVK_MARCH, EVK_ELITE, EVK_BOSS, EVK_SWARM };
typedef struct sevent { int16_t sec; uint8_t kind, type, n; } sevent;
static const sevent EVENTS[] = {
    { 90, EVK_RING, ET_APHID, 20},
    {180, EVK_ELITE, ET_BEETLE, 1},
    {240, EVK_MARCH, ET_ANT, 18},
    {300, EVK_BOSS, ET_QUEEN, 1},
    {390, EVK_RING, ET_APHID, 32},
    {450, EVK_ELITE, ET_THISTLE, 1},
    {510, EVK_SWARM, ET_WASP, 20},
    {600, EVK_BOSS, ET_STAG, 1},
    {690, EVK_RING, ET_LOCUST, 36},
    {750, EVK_ELITE, ET_CROW, 1},
    {810, EVK_MARCH, ET_ANT, 30},
    {840, EVK_SWARM, ET_CROW, 14},
    {900, EVK_BOSS, ET_CROWKING, 1},
};
#define N_EVENTS ((int)(sizeof EVENTS / sizeof EVENTS[0]))

/* the gardeners: health, speed (Q4), armour, pickup (%), the first tool */
static const struct { int16_t hp; int8_t speed, armor; int16_t pickup; uint8_t tool; } CHARS[NCHAR] = {
    {100, 16, 0, 100, W_SEED}, {140, 13, 2, 100, W_CAN}, {80, 19, 0, 130, W_BEES}};

/* 256 x sin(2 pi i / 64), a quarter */
static const int16_t SINQ[17] = {0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255, 256};
static int isin(int i)
{
    i &= 63;
    int q = i & 15;
    switch (i >> 4) {
    case 0: return SINQ[q];
    case 1: return SINQ[16 - q];
    case 2: return -SINQ[q];
    default: return -SINQ[16 - q];
    }
}
static int icos(int i) { return isin(i + 16); }

static int iabs(int v) { return v < 0 ? -v : v; }
/* an approximate length (within 4%) */
static int alen(int dx, int dy)
{
    int a = iabs(dx), b = iabs(dy);
    if (a < b) { int t = a; a = b; b = t; }
    return a + (b * 3 >> 3) - (b > a / 2 ? (b >> 4) : 0);
}
static int rnd(world *w, int n) { return n > 0 ? rs_rng_range(&w->rng, n) : 0; }

int hp_scale(int32_t t) { return 256 + (int)(t * 40 / (60 * SEC)); }
int xp_for(int level) { int k = level - 1; return 5 + k * 3 + k * k / 4; }
/* the pests' touch grows with the time too: x1 at 0:00, x1.25 at 7:30, x1.5 at 15:00 */
static int dmg_scale(int32_t t) { return 256 + (int)(t * 8 / (60 * SEC)); }

/* ---- derived stats ------------------------------------------------------------------------------------------------- */
static void stats_update(world *w)
{
    const uint8_t *b = w->boosts;
    int c = w->chr;
    w->dmg_mul = 256 + w->pas[P_COMPOST] * 26 + b[B_MIGHT] * 10;
    w->area_mul = 256 + w->pas[P_HAT] * 26;
    w->speed = CHARS[c].speed * (256 + w->pas[P_BOOTS] * 20 + b[B_SWIFT] * 8) / 256;
    int hpmax = (CHARS[c].hp + w->pas[P_HONEY] * 15 + b[B_VIGOR] * 8) * FX;
    if (hpmax > w->h.hpmax) w->h.hp += hpmax - w->h.hpmax;     /* more health: the new part comes full */
    w->h.hpmax = hpmax;
    if (w->h.hp > hpmax) w->h.hp = hpmax;
    w->regen = w->pas[P_HONEY] * FX;
    w->pickup = PICKUP_R * CHARS[c].pickup / 100 * (256 + w->pas[P_MAGNET] * 64 + b[B_REACH] * 26) / 256;
    w->cd_mul = 256 - w->pas[P_SUNDIAL] * 15 - b[B_HASTE] * 8;
    w->amount = (w->pas[P_POUCH] + 1) / 2;
    w->xp_mul = 256 + w->pas[P_CLOVER] * 20 + b[B_GROWTH] * 10;
    w->coin_mul = 256 + w->pas[P_CLOVER] * 38;
    w->armor = CHARS[c].armor;
}

int weapon_level_of(const world *w, int id)
{
    for (int i = 0; i < w->n_wp; i++)
        if (w->wp[i].id == id) return w->wp[i].level;
    return 0;
}

static void add_weapon(world *w, int id)
{
    if (w->n_wp >= MAX_WEAPONS || weapon_level_of(w, id)) return;
    weapon *p = &w->wp[w->n_wp++];
    memset(p, 0, sizeof *p);
    p->id = (uint8_t)id;
    p->level = 1;
    p->timer = 20;
}

/* ---- set-up ---------------------------------------------------------------------------------------------------------- */
void world_init(world *w, int chr, int map, const uint8_t boosts[B_COUNT], uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->chr = (uint8_t)(chr >= 0 && chr < NCHAR ? chr : 0);
    w->map = (uint8_t)map;
    if (boosts) memcpy(w->boosts, boosts, B_COUNT);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 0x5eed);
    w->h.face = 0;
    w->h.dx = 64;
    w->level = 1;
    w->xp_need = xp_for(1);
    w->boss = -1;
    stats_update(w);
    w->h.hp = w->h.hpmax;
    add_weapon(w, CHARS[w->chr].tool);
}

/* ---- pests ------------------------------------------------------------------------------------------------------------- */
static enemy *spawn_at(world *w, int type, int32_t x, int32_t y, int elite)
{
    if (w->n_en >= MAX_EN) return NULL;
    enemy *e = &w->en[w->n_en++];
    memset(e, 0, sizeof *e);
    e->type = (uint8_t)type;
    e->x = x;
    e->y = y;
    int hp = EN_INFO[type].hp;
    if (type < ET_BOSS0) hp = hp * hp_scale(w->t) / 256;
    if (w->map == 1) hp = hp * 5 / 4;                 /* the moonlit garden: tougher pests */
    if (elite) hp *= 12;
    e->hp = e->hpmax = hp;
    e->elite = (uint8_t)elite;
    e->anim = (uint8_t)rnd(w, 64);
    if (type >= ET_BOSS0) {
        w->boss = w->n_en - 1;
        w->boss_type = type;
        w->boss_hpmax = e->hpmax;
    }
    return e;
}

/* a point of the spawn ring, angle a (0..63) */
static void ring_point(const world *w, int a, int rx, int ry, int32_t *x, int32_t *y)
{
    *x = w->h.x + icos(a) * rx * FX / 256;
    *y = w->h.y + isin(a) * ry * FX / 256;
}

static int pick_type(world *w)
{
    int m = w->t / (60 * SEC);
    const wave *wv = &WAVES[m < 16 ? m : 15];
    int sum = 0;
    for (int i = 0; i < 8; i++) sum += wv->mix[i];
    int r = rnd(w, sum);
    for (int i = 0; i < 8; i++) {
        if (r < wv->mix[i]) return i;
        r -= wv->mix[i];
    }
    return ET_SLUG;
}

static int target_count(const world *w)
{
    int m = w->t / (60 * SEC), f = w->t % (60 * SEC);
    if (m >= 15) return WAVES[15].target + (m - 15) * 20;
    int a = WAVES[m].target, b = WAVES[m + 1 < 16 ? m + 1 : 15].target;
    int n = a + (b - a) * f / (60 * SEC);
    if (w->boss >= 0) n = n * 3 / 4;                  /* a boss out: fewer of the others */
    if (w->map == 1) n = n * 9 / 8;
    return n > MAX_EN - 20 ? MAX_EN - 20 : n;
}

static void play_event(world *w, const sevent *ev)
{
    int a0 = rnd(w, 64);
    switch (ev->kind) {
    case EVK_RING:                                    /* a ring round the gardener, closing in */
        for (int i = 0; i < ev->n; i++) {
            int32_t x, y;
            ring_point(w, a0 + i * 64 / ev->n, 230, 190, &x, &y);
            spawn_at(w, ev->type, x, y, 0);
        }
        break;
    case EVK_MARCH: {                                 /* a column marching straight across, past the gardener */
        int32_t x, y;
        ring_point(w, a0, SPAWN_RX, SPAWN_RY, &x, &y);
        int dx = (int)((w->h.x - x) / FX), dy = (int)((w->h.y - y) / FX), l = alen(dx, dy);
        int sp = EN_INFO[ev->type].speed;
        for (int i = 0; i < ev->n; i++) {
            int side = (i % 3) - 1;
            enemy *e = spawn_at(w, ev->type, x - (int32_t)dx * FX * (i / 3) * 14 / l + (int32_t)dy * FX * side * 14 / l,
                                y - (int32_t)dy * FX * (i / 3) * 14 / l - (int32_t)dx * FX * side * 14 / l, 0);
            if (e) {
                e->st = 1;
                e->ax = (int16_t)(dx * sp / (l ? l : 1));
                e->ay = (int16_t)(dy * sp / (l ? l : 1));
            }
        }
        break;
    }
    case EVK_SWARM:                                   /* a swarm from one side */
        for (int i = 0; i < ev->n; i++) {
            int32_t x, y;
            ring_point(w, a0 + rnd(w, 9) - 4, SPAWN_RX + rnd(w, 60), SPAWN_RY + rnd(w, 40), &x, &y);
            spawn_at(w, ev->type, x, y, 0);
        }
        break;
    case EVK_ELITE: {
        int32_t x, y;
        ring_point(w, a0, SPAWN_RX, SPAWN_RY, &x, &y);
        spawn_at(w, ev->type, x, y, 1);
        w->events |= EV_ELITE;
        break;
    }
    case EVK_BOSS: {
        int32_t x, y;
        ring_point(w, a0, SPAWN_RX - 40, SPAWN_RY - 40, &x, &y);
        if (spawn_at(w, ev->type, x, y, 0)) {
            w->events |= EV_BOSS;
            w->warn = ev->type + 1;
        }
        break;
    }
    }
}

static void spawner(world *w)
{
    int sec = w->t / SEC;
    for (int i = 0; i < N_EVENTS; i++)
        if (!(w->events_done & (1u << i)) && sec >= EVENTS[i].sec) {
            w->events_done |= 1u << i;
            if (EVENTS[i].kind == EVK_BOSS && (w->bosses_done & (1 << (EVENTS[i].type - ET_BOSS0)))) continue;
            if (EVENTS[i].kind == EVK_BOSS && w->boss >= 0) continue;
            play_event(w, &EVENTS[i]);
        }
    int target = target_count(w), n = w->n_en - (w->boss >= 0);
    if (n >= target) return;
    /* the pests come at SPAWN_RATE(minute) a second at most (the field fills up to its target): 2 a second at 0:00,
     * 7 at 5:00, 12 at 10:00, 17 at 15:00 */
    w->spawn_acc += SPAWN_RATE(w->t / (60 * SEC)) * 64 / SEC;
    while (w->spawn_acc >= 64 && n < target) {
        w->spawn_acc -= 64;
        int32_t x, y;
        ring_point(w, rnd(w, 64), SPAWN_RX + rnd(w, 40), SPAWN_RY + rnd(w, 30), &x, &y);
        if (!spawn_at(w, pick_type(w), x, y, 0)) break;
        n++;
    }
    if (w->spawn_acc > 192) w->spawn_acc = 192;
}

/* ---- the collision grid (rebuilt each step around the gardener) ----------------------------------------------------- */
static int16_t g_head[GRID_N * GRID_N], g_next[MAX_EN];   /* scratch, rebuilt each step (state_audit.txt) */
static int g_ox, g_oy;                                    /* the grid's origin, px (scratch) */

static void grid_build(world *w)
{
    for (int i = 0; i < GRID_N * GRID_N; i++) g_head[i] = -1;
    g_ox = (int)(w->h.x / FX) - GRID_N * GRID_CELL / 2;
    g_oy = (int)(w->h.y / FX) - GRID_N * GRID_CELL / 2;
    for (int i = 0; i < w->n_en; i++) {
        int gx = ((int)(w->en[i].x / FX) - g_ox) / GRID_CELL, gy = ((int)(w->en[i].y / FX) - g_oy) / GRID_CELL;
        g_next[i] = -1;
        if (gx < 0 || gy < 0 || gx >= GRID_N || gy >= GRID_N) continue;
        g_next[i] = g_head[gy * GRID_N + gx];
        g_head[gy * GRID_N + gx] = (int16_t)i;
    }
}

/* calls fn for each pest whose body touches the circle (x, y, r) (px); fn returns 1 to stop */
typedef int (*hit_fn)(world *w, int i, void *u);
static void query(world *w, int x, int y, int r, hit_fn fn, void *u)
{
    int gx0 = (x - r - 18 - g_ox) / GRID_CELL, gx1 = (x + r + 18 - g_ox) / GRID_CELL;
    int gy0 = (y - r - 18 - g_oy) / GRID_CELL, gy1 = (y + r + 18 - g_oy) / GRID_CELL;
    if (x - r - 18 - g_ox < 0) gx0 = 0;
    if (y - r - 18 - g_oy < 0) gy0 = 0;
    if (gx1 >= GRID_N) gx1 = GRID_N - 1;
    if (gy1 >= GRID_N) gy1 = GRID_N - 1;
    for (int gy = gy0; gy <= gy1; gy++)
        for (int gx = gx0; gx <= gx1; gx++)
            for (int i = g_head[gy * GRID_N + gx]; i >= 0; i = g_next[i]) {
                const enemy *e = &w->en[i];
                if (e->hp <= 0) continue;
                int dx = (int)(e->x / FX) - x, dy = (int)(e->y / FX) - y, rr = r + EN_INFO[e->type].r;
                if (dx * dx + dy * dy <= rr * rr && fn(w, i, u)) return;
            }
}

/* ---- damage, kills, drops ------------------------------------------------------------------------------------------- */
static void drop_gem(world *w, int32_t x, int32_t y, int val)
{
    if (val <= 0) return;
    x = (x / (8 * FX)) * (8 * FX) + 4 * FX;           /* the 8-px cell's centre */
    y = (y / (8 * FX)) * (8 * FX) + 4 * FX;
    for (int i = 0; i < w->n_gm; i++)                  /* a gem already in that cell: they merge */
        if (!w->gm[i].fly && w->gm[i].x == x && w->gm[i].y == y) {
            w->gm[i].val = (int16_t)(w->gm[i].val + val > 30000 ? 30000 : w->gm[i].val + val);
            return;
        }
    if (w->n_gm >= MAX_GEM) {                          /* full: the gem nearest joins it */
        int best = 0, bd = 0x7fffffff;
        for (int i = 0; i < w->n_gm; i++) {
            if (w->gm[i].fly) continue;
            int d = alen((int)((w->gm[i].x - x) / FX), (int)((w->gm[i].y - y) / FX));
            if (d < bd) { bd = d; best = i; }
        }
        w->gm[best].val = (int16_t)(w->gm[best].val + val > 30000 ? 30000 : w->gm[best].val + val);
        return;
    }
    gem *g = &w->gm[w->n_gm++];
    memset(g, 0, sizeof *g);
    g->x = x;
    g->y = y;
    g->val = (int16_t)val;
}

static void drop_item(world *w, int kind, int32_t x, int32_t y, int big)
{
    if (w->n_it >= MAX_ITEM) {                         /* full: a coin or an apple makes room (never a chest) */
        int k = -1;
        for (int i = 0; i < w->n_it; i++)
            if (w->it[i].kind != IT_CHEST) { k = i; break; }
        if (k < 0 || kind != IT_CHEST) return;
        w->it[k] = w->it[--w->n_it];
    }
    item *it = &w->it[w->n_it++];
    it->x = x;
    it->y = y;
    it->kind = (uint8_t)kind;
    it->big = (uint8_t)big;
    it->t = 0;
}

/* the pests whose health is gone (killed, or gone past the field) leave the array, in order (deterministic) */
static void compact(world *w)
{
    int n = 0;
    w->boss = -1;
    for (int i = 0; i < w->n_en; i++) {
        if (w->en[i].hp <= 0) continue;
        if (n != i) w->en[n] = w->en[i];
        if (w->en[n].type >= ET_BOSS0) w->boss = n;
        n++;
    }
    w->n_en = n;
}

static void kill(world *w, int i)
{
    enemy *e = &w->en[i];
    int type = e->type, px = (int)(e->x / FX), py = (int)(e->y / FX);
    w->kills++;
    w->events |= EV_KILL;
    if (w->n_kfx < MAX_KILLFX) w->kfx[w->n_kfx++] = (killfx){(int16_t)px, (int16_t)py, (uint8_t)type, e->elite};
    if (type >= ET_BOSS0) {
        w->bosses_done |= 1 << (type - ET_BOSS0);
        w->events |= EV_BOSSKILL;
        w->hitstop = 30;
        drop_item(w, IT_CHEST, e->x, e->y, 1);
        drop_gem(w, e->x + 16 * FX, e->y, 60 + 40 * (type - ET_BOSS0));
        for (int k = 0; k < 6; k++) drop_item(w, IT_COIN, e->x + (isin(k * 11) * 28 / 256) * FX, e->y + (icos(k * 11) * 28 / 256) * FX, 1);
        if (type == ET_CROWKING && !w->won) {
            w->won = 1;
            w->events |= EV_WIN;
        }
    } else {
        int xp = EN_INFO[type].xp;
        if (e->elite) {
            drop_item(w, IT_CHEST, e->x, e->y, 0);
            xp = 20;
        }
        drop_gem(w, e->x, e->y, xp);
        int luck = w->pas[P_CLOVER];
        int r = rnd(w, 1000);
        if (r < 14 + luck * 3) drop_item(w, IT_COIN, e->x, e->y, 0);
        else if (r < 17 + luck * 3) drop_item(w, IT_APPLE, e->x, e->y, 0);
        else if (r < 19 + luck && w->t > 60 * SEC) drop_item(w, IT_MAGNET, e->x, e->y, 0);
    }
    e->hp = 0;                                        /* (compact() takes it out at the step's end) */
}

/* a hit: the damage (the passives' multiplier applied by the caller), a push away from (fx, fy) px, a pop */
static void hurt(world *w, int i, int dmg, int fx, int fy, int knock)
{
    enemy *e = &w->en[i];
    if (e->hp <= 0) return;
    if (dmg < 1) dmg = 1;
    e->hp = e->hp - dmg < 0 ? 0 : e->hp - dmg;
    e->flash = 4;
    w->events |= EV_HIT;
    if (w->n_pops < MAX_POPS) {
        int big = dmg >= 40;
        w->pops[w->n_pops++] = (pop){(int16_t)(e->x / FX), (int16_t)(e->y / FX - EN_INFO[e->type].r - 4),
                                     (int16_t)dmg, (uint8_t)big, 0};
    }
    if (knock && EN_INFO[e->type].knock) {
        int dx = (int)(e->x / FX) - fx, dy = (int)(e->y / FX) - fy, l = alen(dx, dy);
        if (l < 1) { dx = 1; l = 1; }
        int k = knock * EN_INFO[e->type].knock / 8;
        e->kx = (int16_t)(dx * k / l);
        e->ky = (int16_t)(dy * k / l);
    }
    if (e->hp <= 0) {
        e->hp = 1;
        kill(w, i);
    }
}

static int dmg_of(const world *w, int base) { return base * w->dmg_mul / 256; }
static int area_of(const world *w, int r) { return r * w->area_mul / 256; }
static int cd_of(const world *w, int cd) { int c = cd * w->cd_mul / 256; return c < 6 ? 6 : c; }

/* ---- shots ------------------------------------------------------------------------------------------------------------- */
static shot *new_shot(world *w, int kind, int32_t x, int32_t y, int vx, int vy, int dmg, int life, int r, int pierce)
{
    if (w->n_sh >= MAX_SHOT) return NULL;
    shot *s = &w->sh[w->n_sh++];
    memset(s, 0, sizeof *s);
    s->kind = (uint8_t)kind;
    s->x = x;
    s->y = y;
    s->vx = (int16_t)vx;
    s->vy = (int16_t)vy;
    s->dmg = (int16_t)dmg;
    s->life = (int16_t)life;
    s->r = (uint8_t)r;
    s->pierce = (uint8_t)pierce;
    s->id = ++w->serial ? w->serial : ++w->serial;
    return s;
}

/* the nearest pest within r px of (x, y) px: its index, -1 none */
static int nearest(world *w, int x, int y, int r)
{
    int best = -1, bd = r;
    for (int i = 0; i < w->n_en; i++) {
        int d = alen((int)(w->en[i].x / FX) - x, (int)(w->en[i].y / FX) - y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* a velocity of length sp (Q4) toward (tx, ty) from (x, y), px; (0, 0): along the gardener's last move */
static void aim(const world *w, int x, int y, int tx, int ty, int sp, int *vx, int *vy)
{
    int dx = tx - x, dy = ty - y, l = alen(dx, dy);
    if (l < 1) { dx = w->h.dx; dy = w->h.dy; l = alen(dx, dy); if (l < 1) { dx = 64; l = 64; } }
    *vx = dx * sp / l;
    *vy = dy * sp / l;
}

/* the hit of shot s on pest i (query's callback) */
static int shot_hit(world *w, int i, void *u)
{
    shot *s = (shot *)u;
    enemy *e = &w->en[i];
    if (e->lh == s->id) return 0;
    e->lh = s->id;
    int knock = s->kind == SK_SEED || s->kind == SK_SUNSEED ? 20 : s->kind == SK_MOWER ? 40 : s->kind == SK_RAKE ? 28 : 16;
    if (s->kind == SK_SPLASH || s->kind == SK_BLAST) knock = 48;
    hurt(w, i, s->dmg, (int)(s->x / FX), (int)(s->y / FX), knock);
    if (s->pierce != 255) {
        if (s->pierce == 0) { s->life = 0; return 1; }
        s->pierce--;
    }
    return 0;
}

/* a zone's tick: each pest in it once per cd frames (flames) */
static int zone_hit(world *w, int i, void *u)
{
    shot *s = (shot *)u;
    enemy *e = &w->en[i];
    if (e->cd_zone) return 0;
    e->cd_zone = 20;
    hurt(w, i, s->dmg, (int)(s->x / FX), (int)(s->y / FX), 0);
    return 0;
}

/* the splash: a box ahead of the gardener (aux: the direction, -1 / 1), hits each pest once */
static int splash_hit(world *w, int i, void *u)
{
    shot *s = (shot *)u;
    enemy *e = &w->en[i];
    if (s->aux2) return shot_hit(w, i, u);            /* the rainbow: a ring, any pest in its circle */
    int dy = (int)((e->y - s->y) / FX), hh = s->r / 2 + EN_INFO[e->type].r;
    if (dy < -hh || dy > hh) return 0;
    return shot_hit(w, i, u);
}

static int gem_pull(world *w, int x, int y, int r)
{
    int n = 0;
    for (int i = 0; i < w->n_gm; i++) {
        gem *g = &w->gm[i];
        if (g->fly) continue;
        if (iabs((int)(g->x / FX) - x) < r && iabs((int)(g->y / FX) - y) < r) { g->fly = 1; g->v = 32; n++; }
    }
    return n;
}

static void shots_step(world *w)
{
    for (int k = 0; k < w->n_sh; k++) {
        shot *s = &w->sh[k];
        s->t++;
        int px = (int)(s->x / FX), py = (int)(s->y / FX);
        switch (s->kind) {
        case SK_RAKE: {                               /* out, slowing, then back to the gardener */
            if (!s->aux) {
                s->vx = (int16_t)(s->vx * 15 / 16);
                s->vy = (int16_t)(s->vy * 15 / 16);
                if (iabs(s->vx) + iabs(s->vy) < 10) { s->aux = 1; s->id = ++w->serial; }
            } else {
                int vx, vy;
                aim(w, px, py, (int)(w->h.x / FX), (int)(w->h.y / FX), 64 + s->t / 2, &vx, &vy);
                s->vx = (int16_t)vx;
                s->vy = (int16_t)vy;
                if (alen((int)(w->h.x / FX) - px, (int)(w->h.y / FX) - py) < 10) s->life = 0;
            }
            if (s->aux2) gem_pull(w, px, py, 12);     /* the golden rake gathers gems */
            break;
        }
        case SK_MOWER:
            if (s->aux) {                             /* the turbo mower: bounces round the gardener, hits again */
                int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX);
                if ((px < hx - 150 && s->vx < 0) || (px > hx + 150 && s->vx > 0)) s->vx = (int16_t)-s->vx;
                if ((py < hy - 105 && s->vy < 0) || (py > hy + 105 && s->vy > 0)) s->vy = (int16_t)-s->vy;
                if (s->t % 20 == 0) s->id = ++w->serial;
            }
            break;
        case SK_PUFF: {                               /* a drifting dandelion puff, wobbling; hits again now and then */
            int wob = isin(s->t + s->aux * 7) * 6 / 256;
            s->x += (int32_t)(-s->vy * wob / 32);
            s->y += (int32_t)(s->vx * wob / 32);
            if (s->aux2) {                            /* the wishing wind: homing */
                int j = nearest(w, px, py, 160);
                if (j >= 0) {
                    int vx, vy;
                    aim(w, px, py, (int)(w->en[j].x / FX), (int)(w->en[j].y / FX), 24, &vx, &vy);
                    s->vx = (int16_t)((s->vx * 7 + vx) / 8);
                    s->vy = (int16_t)((s->vy * 7 + vy) / 8);
                }
            }
            if (s->t % 40 == 0) s->id = ++w->serial;
            break;
        }
        case SK_PUMPKIN:                              /* in flight: lands on its target, then bursts */
            if (s->life == 1) {
                shot *b = new_shot(w, SK_BLAST, s->x, s->y, 0, 0, s->dmg, 6, s->aux, 255);
                if (b) b->aux2 = s->aux2;
                w->events |= EV_BOOM;
                if (s->aux2) {                        /* the giant pumpkin: seeds fly out */
                    for (int a = 0; a < 64; a += 8)
                        new_shot(w, SK_SEED, s->x, s->y, icos(a) * 48 / 256, isin(a) * 48 / 256, s->dmg / 3, 40, 3, 1);
                }
                s = &w->sh[k];                        /* (new_shot may not move the array, but stay safe) */
            }
            break;
        case SK_THORNRING:
            s->x = w->h.x;
            s->y = w->h.y;
            break;
        default:
            break;
        }
        s->x += s->vx;
        s->y += s->vy;
        px = (int)(s->x / FX);
        py = (int)(s->y / FX);
        if (s->kind == SK_PUMPKIN || s->kind == SK_THORNRING) { /* (no hit in flight; the ring is drawn only) */
        } else if (s->kind == SK_FLAME) {
            query(w, px, py, s->r, zone_hit, s);
        } else if (s->kind == SK_SPLASH) {
            if (s->t <= 2) query(w, px, py, s->r, splash_hit, s);
        } else if (s->kind == SK_BLAST) {
            if (s->t == 1) query(w, px, py, s->r, shot_hit, s);
        } else {
            query(w, px, py, s->r, shot_hit, s);
        }
        if (--s->life <= 0) {
            w->sh[k] = w->sh[--w->n_sh];
            k--;
        }
    }
}

/* ---- the tools ------------------------------------------------------------------------------------------------------------ */
static int orbit_hit(world *w, int i, void *u)
{
    int *d = (int *)u;
    enemy *e = &w->en[i];
    if (e->cd_orbit) return 0;
    e->cd_orbit = (uint8_t)d[1];
    hurt(w, i, d[0], d[2], d[3], 24);
    return 0;
}

static int aura_hit(world *w, int i, void *u)
{
    int *d = (int *)u;
    int hp = w->en[i].hp;
    hurt(w, i, d[0], (int)(w->h.x / FX), (int)(w->h.y / FX), d[1]);
    if (d[2] && hp <= d[0] && w->h.hp < w->h.hpmax) w->h.hp += FX / 2;      /* the bramble crown heals on its kills */
    return 0;
}

/* the bees' positions (also drawn by draw.c): bee k of n at the frame t */
void bees_pos(const world *w, const weapon *p, int k, int n, int *x, int *y);
static int bees_radius(const world *w, const weapon *p) { return area_of(w, p->evo ? 46 : p->level >= 5 ? 38 : 30); }
void bees_pos(const world *w, const weapon *p, int k, int n, int *x, int *y)
{
    int r = bees_radius(w, p), period = p->evo ? 64 : 96;
    int a = (int)((w->t * 64 / period) % 64) + k * 64 / n;
    *x = (int)(w->h.x / FX) + icos(a) * r / 256;
    *y = (int)(w->h.y / FX) + isin(a) * r / 256;
}

static void fire(world *w, weapon *p)
{
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX), L = p->level, n = w->amount;
    switch (p->id) {
    case W_SEED: {
        int cnt = (p->evo ? 4 : L >= 4 ? 3 : L >= 2 ? 2 : 1) + n;
        int dmg = dmg_of(w, p->evo ? 24 : L >= 5 ? 18 : L >= 3 ? 14 : 10), pierce = p->evo ? 3 : L >= 5 ? 1 : 0;
        int j = nearest(w, hx, hy, 220);
        if (w->boss >= 0 && (++p->aux2 & 1)) {           /* every other volley: at the boss, when it is in range */
            const enemy *b = &w->en[w->boss];
            if (alen((int)(b->x / FX) - hx, (int)(b->y / FX) - hy) < 220) j = w->boss;
        }
        int tx = j >= 0 ? (int)(w->en[j].x / FX) : hx + w->h.dx, ty = j >= 0 ? (int)(w->en[j].y / FX) : hy + w->h.dy;
        for (int i = 0; i < cnt; i++) {
            int vx, vy, sp = 56;
            aim(w, hx, hy, tx, ty, sp, &vx, &vy);
            int spread = (i - (cnt - 1) / 2) * 5;     /* a little fan */
            int fx = vx - vy * spread / 64, fy = vy + vx * spread / 64;
            new_shot(w, p->evo ? SK_SUNSEED : SK_SEED, w->h.x, w->h.y, fx, fy, dmg, 70, 4, pierce);
        }
        if (p->evo && (++p->aux & 1))                 /* every other volley: a ring of sunflower seeds */
            for (int a = 0; a < 64; a += 8)
                new_shot(w, SK_SUNSEED, w->h.x, w->h.y, icos(a) * 48 / 256, isin(a) * 48 / 256, dmg, 60, 4, pierce);
        w->events |= EV_SHOT;
        p->timer = (int16_t)cd_of(w, p->evo ? 36 : 52);
        break;
    }
    case W_CAN: {
        int dmg = dmg_of(w, p->evo ? 36 : L >= 5 ? 28 : L >= 3 ? 22 : 16);
        if (p->evo) {
            shot *s = new_shot(w, SK_SPLASH, w->h.x, w->h.y, 0, 0, dmg, 14, area_of(w, 64), 255);
            if (s) s->aux2 = 1;
        } else {
            int len = area_of(w, L >= 4 ? 64 : 52), dir = w->h.face ? -1 : 1;
            int j = nearest(w, hx, hy, 120);                 /* toward the nearest pest's side */
            if (j >= 0) dir = w->en[j].x < w->h.x ? -1 : 1;
            for (int side = 0; side < (L >= 2 ? 2 : 1); side++) {
                int d = side ? -dir : dir;
                shot *s = new_shot(w, SK_SPLASH, w->h.x + d * (len / 2 + 6) * FX, w->h.y, 0, 0, dmg, 12, len / 2, 255);
                if (s) s->aux = (int16_t)d;
            }
        }
        w->events |= EV_SPLASH;
        p->timer = (int16_t)cd_of(w, p->evo ? 50 : L >= 5 ? 56 : 66);
        break;
    }
    case W_MOWER: {
        int cnt = (L >= 4 ? 2 : 1) + n, dmg = dmg_of(w, p->evo ? 50 : L >= 5 ? 40 : L >= 2 ? 30 : 22);
        for (int i = 0; i < cnt; i++) {
            int dx = w->h.dx, dy = w->h.dy;
            if (i % 2) { dx = -dx; dy = -dy; }
            int rot = (i / 2) * 10;
            int vx = (dx * 64 - dy * rot) / 64, vy = (dy * 64 + dx * rot) / 64, l = alen(vx, vy);
            if (l < 1) { vx = 64; l = 64; }
            shot *s = new_shot(w, SK_MOWER, w->h.x, w->h.y, vx * 64 / l, vy * 64 / l, dmg, p->evo ? 240 : 55,
                               area_of(w, 10), 255);
            if (s && p->evo) {
                s->aux = 1;
                s->vy = (int16_t)(s->vy ? s->vy : 40);
            }
        }
        w->events |= EV_MOWER;
        p->timer = (int16_t)cd_of(w, p->evo ? 260 : L >= 3 ? 115 : 140);
        break;
    }
    case W_RAKE: {
        int cnt = (p->evo ? 4 : L >= 5 ? 3 : L >= 3 ? 2 : 1) + n;
        int dmg = dmg_of(w, p->evo ? 40 : L >= 4 ? 30 : L >= 2 ? 22 : 16), sp = L >= 4 || p->evo ? 80 : 68;
        int j = nearest(w, hx, hy, 200);
        for (int i = 0; i < cnt; i++) {
            int vx, vy;
            if (p->evo) {
                vx = icos(i * 64 / cnt + (int)(w->t / 4)) * sp / 256;
                vy = isin(i * 64 / cnt + (int)(w->t / 4)) * sp / 256;
            } else {
                aim(w, hx, hy, j >= 0 ? (int)(w->en[j].x / FX) : hx + w->h.dx, j >= 0 ? (int)(w->en[j].y / FX) : hy + w->h.dy,
                    sp, &vx, &vy);
                int rot = (i - (cnt - 1) / 2) * 20;
                int fx = vx - vy * rot / 64, fy = vy + vx * rot / 64;
                vx = fx;
                vy = fy;
            }
            shot *s = new_shot(w, SK_RAKE, w->h.x, w->h.y, vx, vy, dmg, 200, area_of(w, 9), 255);
            if (s) s->aux2 = p->evo;
        }
        w->events |= EV_RAKE;
        p->timer = (int16_t)cd_of(w, p->evo ? 80 : 100);
        break;
    }
    case W_PUMPKIN: {
        int cnt = (L >= 5 ? 3 : L >= 2 ? 2 : 1) + n, dmg = dmg_of(w, p->evo ? 80 : L >= 4 ? 45 : 30);
        int r = area_of(w, p->evo ? 52 : L >= 3 ? 34 : 26);
        for (int i = 0; i < cnt; i++) {
            int tx, ty;
            if (w->n_en) {
                int j = rnd(w, w->n_en), d = alen((int)(w->en[j].x / FX) - hx, (int)(w->en[j].y / FX) - hy);
                if (d > 190) j = nearest(w, hx, hy, 190);
                if (j >= 0) { tx = (int)(w->en[j].x / FX); ty = (int)(w->en[j].y / FX); }
                else { int a = rnd(w, 64); tx = hx + icos(a) * 80 / 256; ty = hy + isin(a) * 80 / 256; }
            } else {
                int a = rnd(w, 64);
                tx = hx + icos(a) * 80 / 256;
                ty = hy + isin(a) * 80 / 256;
            }
            int fl = 36;
            shot *s = new_shot(w, SK_PUMPKIN, w->h.x, w->h.y, (tx - hx) * FX / fl, (ty - hy) * FX / fl, dmg, fl, 0, 255);
            if (s) { s->aux = (int16_t)r; s->aux2 = p->evo; }
        }
        p->timer = (int16_t)cd_of(w, p->evo ? 100 : 110);
        break;
    }
    case W_THORNS: {
        int r = area_of(w, p->evo ? 56 : L >= 4 ? 40 : L >= 2 ? 34 : 28);
        int d[3] = {dmg_of(w, p->evo ? 18 : L >= 5 ? 11 : L >= 3 ? 8 : 5), p->evo || L >= 5 ? 28 : 10, p->evo};
        query(w, hx, hy, r, aura_hit, d);
        shot *s = new_shot(w, SK_THORNRING, w->h.x, w->h.y, 0, 0, 0, 10, r, 255);
        (void)s;
        p->timer = (int16_t)cd_of(w, p->evo ? 16 : L >= 5 ? 20 : 24);
        break;
    }
    case W_CHILI: {
        if (!w->h.moving && !p->evo) { p->timer = 2; break; }
        int r = area_of(w, p->evo ? 15 : L >= 4 ? 13 : 10), dmg = dmg_of(w, p->evo ? 20 : L >= 5 ? 13 : L >= 2 ? 9 : 6);
        new_shot(w, SK_FLAME, w->h.x - w->h.dx * 6 * FX / 64, w->h.y - w->h.dy * 6 * FX / 64 + 4 * FX, 0, 0, dmg,
                 p->evo ? 150 : L >= 3 ? 180 : 120, r, 255);
        p->timer = (int16_t)cd_of(w, p->evo ? (w->h.moving ? 8 : 30) : L >= 5 ? 14 : 20);
        break;
    }
    case W_PUFF: {
        int cnt = (p->evo ? 6 : L >= 4 ? 4 : L >= 2 ? 3 : 2) + n, dmg = dmg_of(w, p->evo ? 20 : L >= 3 ? 13 : 9);
        for (int i = 0; i < cnt; i++) {
            int a = rnd(w, 64);
            shot *s = new_shot(w, SK_PUFF, w->h.x, w->h.y, icos(a) * 12 / 256, isin(a) * 12 / 256, dmg,
                               p->evo ? 300 : L >= 5 ? 360 : 240, area_of(w, 6), 255);
            if (s) { s->aux = (int16_t)rnd(w, 64); s->aux2 = p->evo; }
        }
        w->events |= EV_PUFF;
        p->timer = (int16_t)cd_of(w, p->evo ? 60 : 70);
        break;
    }
    default:
        break;
    }
}

static void tools_step(world *w)
{
    for (int i = 0; i < w->n_wp; i++) {
        weapon *p = &w->wp[i];
        if (p->id == W_BEES) {                        /* the bees: always out, each pest stung once per cooldown */
            int n = (p->evo ? 6 : p->level >= 5 ? 4 : p->level >= 4 ? 3 : p->level >= 2 ? 2 : 1) + w->amount;
            int d[4] = {dmg_of(w, p->evo ? 16 : p->level >= 3 ? 11 : 7), p->evo ? 20 : 30, 0, 0};
            for (int k = 0; k < n; k++) {
                int x, y;
                bees_pos(w, p, k, n, &x, &y);
                d[2] = (int)(w->h.x / FX);
                d[3] = (int)(w->h.y / FX);
                query(w, x, y, p->evo && k == 0 ? 9 : 5, orbit_hit, d);
            }
            continue;
        }
        if (--p->timer <= 0) fire(w, p);
    }
}

/* ---- the pests' moves ----------------------------------------------------------------------------------------------------- */
static void boss_ai(world *w, int i)
{
    enemy *e = &w->en[i];
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX), ex = (int)(e->x / FX), ey = (int)(e->y / FX);
    e->st_t++;
    switch (e->type) {
    case ET_QUEEN:                                    /* slow; now and then her children come out */
        if (e->st_t % 240 == 120)
            for (int k = 0; k < 6; k++)
                spawn_at(w, ET_SLUG, e->x + icos(k * 11) * 30 * FX / 256, e->y + isin(k * 11) * 30 * FX / 256, 0);
        break;
    case ET_STAG:                                     /* walks, stops and shakes (the warning), charges */
        if (e->st == 0 && e->st_t >= 240) { e->st = 1; e->st_t = 0; }
        else if (e->st == 1 && e->st_t >= 45) {
            int vx, vy;
            aim(w, ex, ey, hx, hy, 56, &vx, &vy);
            e->ax = (int16_t)vx;
            e->ay = (int16_t)vy;
            e->st = 2;
            e->st_t = 0;
        } else if (e->st == 2 && e->st_t >= 50) { e->st = 0; e->st_t = 0; }
        break;
    case ET_CROWKING:                                 /* circles, dives through, calls crows */
        if (e->st == 0 && e->st_t >= 200) {
            int vx, vy;
            aim(w, ex, ey, hx, hy, 60, &vx, &vy);
            e->ax = (int16_t)vx;
            e->ay = (int16_t)vy;
            e->st = 2;
            e->st_t = 0;
        } else if (e->st == 2 && e->st_t >= 45) { e->st = 0; e->st_t = 0; }
        if (e->st_t == 100 && e->st == 0)
            for (int k = 0; k < 4; k++) {
                int32_t x, y;
                ring_point(w, k * 16 + rnd(w, 8), SPAWN_RX, SPAWN_RY, &x, &y);
                spawn_at(w, ET_CROW, x, y, 0);
            }
        break;
    }
}

static void enemies_step(world *w)
{
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX);
    for (int i = 0; i < w->n_en; i++) {
        enemy *e = &w->en[i];
        if (e->hp <= 0) continue;
        const enemy_info *in = &EN_INFO[e->type];
        if (e->flash) e->flash--;
        if (e->cd_orbit) e->cd_orbit--;
        if (e->cd_zone) e->cd_zone--;
        e->anim++;
        if (e->type >= ET_BOSS0) {
            boss_ai(w, i);
            e = &w->en[i];
        }
        int ex = (int)(e->x / FX), ey = (int)(e->y / FX);
        int dx = hx - ex, dy = hy - ey, l = alen(dx, dy);
        /* far away: back to the ring (ahead of the gardener when it walks), the marchers gone */
        if (l > FAR_R && e->type < ET_BOSS0) {
            if (e->st == 1) { e->hp = 0; continue; }
            int a = rnd(w, 64);
            if (w->h.moving && rnd(w, 2)) {
                int best = 0, bd = -0x40000000;
                for (int k = 0; k < 64; k += 4) {
                    int d = icos(k) * w->h.dx + isin(k) * w->h.dy;
                    if (d > bd) { bd = d; best = k; }
                }
                a = best + rnd(w, 13) - 6;
            }
            ring_point(w, a, SPAWN_RX, SPAWN_RY, &e->x, &e->y);
            continue;
        }
        int sp = in->speed, vx = 0, vy = 0;
        if (w->map == 1) sp = sp * 9 / 8;
        if (l < 1) l = 1;
        switch (e->type) {
        case ET_ANT:
            if (e->st == 1) { vx = e->ax; vy = e->ay; break; }
            vx = dx * sp / l;
            vy = dy * sp / l;
            break;
        case ET_WASP: {                               /* a zigzag */
            int wob = isin(e->anim * 2) * sp / 256;
            vx = dx * sp / l - dy * wob / l;
            vy = dy * sp / l + dx * wob / l;
            break;
        }
        case ET_CROW:                                 /* swoops: when close it keeps its line a moment */
            if (e->st == 2) {
                vx = e->ax;
                vy = e->ay;
                if (++e->st_t > 40) e->st = 0;
            } else {
                vx = dx * sp / l;
                vy = dy * sp / l;
                if (l < 60) { e->st = 2; e->st_t = 0; e->ax = (int16_t)vx; e->ay = (int16_t)vy; }
            }
            break;
        case ET_LOCUST: {                             /* hops: a burst at 1.5 x its speed, a pause */
            int k = e->anim % 50 < 26 ? 3 : 0;
            vx = dx * sp * k / (2 * l);
            vy = dy * sp * k / (2 * l);
            break;
        }
        case ET_STAG:
            if (e->st == 1) break;                    /* the warning: still */
            if (e->st == 2) { vx = e->ax; vy = e->ay; break; }
            vx = dx * sp / l;
            vy = dy * sp / l;
            break;
        case ET_CROWKING:
            if (e->st == 2) { vx = e->ax; vy = e->ay; break; }
            {                                         /* circles at 110 px, drifting in */
                int want = 110, tang = sp;
                int rad = (l - want) * sp / 40;
                if (rad > sp) rad = sp;
                if (rad < -sp) rad = -sp;
                vx = (dx * rad - dy * tang) / l;
                vy = (dy * rad + dx * tang) / l;
            }
            break;
        default:
            vx = dx * sp / l;
            vy = dy * sp / l;
            break;
        }
        if (e->elite) { vx = vx * 9 / 8; vy = vy * 9 / 8; }
        e->x += vx + e->kx;
        e->y += vy + e->ky;
        e->kx = (int16_t)(e->kx * 3 / 4);
        e->ky = (int16_t)(e->ky * 3 / 4);
        if (vx < -2) e->face = 1;
        else if (vx > 2) e->face = 0;
        /* touching the gardener */
        int rr = in->r + HERO_R - 2;
        if (iabs(dx) < rr && iabs(dy) < rr && dx * dx + dy * dy < rr * rr && !w->h.hurt_t && !w->dead) {
            int dmg = in->dmg * (e->elite ? 2 : 1) * dmg_scale(w->t) / 256 - w->armor;
            if (dmg < 1) dmg = 1;
            w->h.hp -= dmg * FX;
            w->h.hurt_t = HURT_IFRAMES;
            w->events |= EV_HURT;
            if (w->h.hp <= 0) {
                w->h.hp = 0;
                w->dead = 1;
                w->events |= EV_DEAD;
            }
        }
    }
}

/* the pests push each other apart (the swarm keeps its shape): pairs in the same bucket */
static void separate(world *w)
{
    for (int b = 0; b < GRID_N * GRID_N; b++) {
        int n = 0;
        for (int i = g_head[b]; i >= 0 && n < 10; i = g_next[i], n++) {
            enemy *a = &w->en[i];
            int m = 0;
            for (int j = g_next[i]; j >= 0 && m < 8; j = g_next[j], m++) {
                enemy *c = &w->en[j];
                int dx = (int)((a->x - c->x) / 4), dy = (int)((a->y - c->y) / 4);     /* quarter px */
                int boss = a->type >= ET_BOSS0 || c->type >= ET_BOSS0;
                int rr = (EN_INFO[a->type].r + EN_INFO[c->type].r) * 3 + 4;          /* 3/4 of the radii, + 1 px */
                if (!boss && rr > SEPARATE_R * 4) rr = SEPARATE_R * 4;
                if (iabs(dx) >= rr || iabs(dy) >= rr) continue;
                int l = alen(dx, dy);
                if (l >= rr) continue;
                if (l == 0) { dx = (i & 1) ? 1 : -1; dy = (j & 1) ? 1 : -1; l = 1; }
                int push = (rr - l) * 2;
                int px = dx * push / l, py = dy * push / l;
                int wa = a->type >= ET_BOSS0 ? 0 : c->type >= ET_BOSS0 ? 2 : 1, wc = 2 - wa;
                a->x += px * wa / 2;
                a->y += py * wa / 2;
                c->x -= px * wc / 2;
                c->y -= py * wc / 2;
            }
        }
    }
}

/* ---- gems and pickups ---------------------------------------------------------------------------------------------------- */
static void level_check(world *w)
{
    while (w->xp >= w->xp_need) {
        w->xp -= w->xp_need;
        w->level++;
        w->xp_need = xp_for(w->level);
        w->pending++;
        w->events |= EV_LEVEL;
    }
}

static void gems_step(world *w)
{
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX), r = w->pickup;
    for (int i = 0; i < w->n_gm; i++) {
        gem *g = &w->gm[i];
        int dx = hx - (int)(g->x / FX), dy = hy - (int)(g->y / FX);
        if (!g->fly) {
            if (w->magnet_t || (iabs(dx) < r && iabs(dy) < r && dx * dx + dy * dy < r * r)) { g->fly = 1; g->v = 0; }
            else continue;
        }
        g->v = (int16_t)(g->v + GEM_ACCEL > GEM_MAXV ? GEM_MAXV : g->v + GEM_ACCEL);
        int l = alen(dx, dy);
        if (l < 8) {
            w->xp += g->val * w->xp_mul / 256 > 0 ? g->val * w->xp_mul / 256 : 1;
            w->events |= EV_GEM;
            w->gems_got++;
            w->gm[i] = w->gm[--w->n_gm];
            i--;
            continue;
        }
        g->x += dx * g->v / l;
        g->y += dy * g->v / l;
    }
    if (w->magnet_t) w->magnet_t--;
    level_check(w);
}

static void items_step(world *w)
{
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX);
    for (int i = 0; i < w->n_it; i++) {
        item *it = &w->it[i];
        it->t++;
        int dx = hx - (int)(it->x / FX), dy = hy - (int)(it->y / FX);
        int r = it->kind == IT_COIN ? w->pickup : TOUCH_R;
        if (iabs(dx) >= r || iabs(dy) >= r || dx * dx + dy * dy >= r * r) {
            if (it->t > 60 * SEC && it->kind != IT_CHEST) { w->it[i] = w->it[--w->n_it]; i--; }   /* old: gone */
            continue;
        }
        switch (it->kind) {
        case IT_APPLE:
            w->h.hp += 30 * FX;
            if (w->h.hp > w->h.hpmax) w->h.hp = w->h.hpmax;
            w->events |= EV_HEAL;
            break;
        case IT_MAGNET:
            w->magnet_t = 90;
            w->events |= EV_MAGNET;
            break;
        case IT_COIN:
            w->coins += (it->big ? 5 : 1) * w->coin_mul / 256 > 0 ? (it->big ? 5 : 1) * w->coin_mul / 256 : 1;
            w->events |= EV_COIN;
            break;
        case IT_CHEST:
            w->chest_open = it->big ? 3 : 1;
            w->events |= EV_CHEST;
            break;
        }
        w->it[i] = w->it[--w->n_it];
        i--;
    }
}

/* ---- one step ----------------------------------------------------------------------------------------------------------- */
void world_step(world *w, int ix, int iy)
{
    w->events = 0;
    w->n_pops = w->n_kfx = 0;
    w->warn = 0;
    w->gems_got = 0;
    if (w->dead) return;
    if (w->won) {                                     /* the garden saved: the pests left pop, a few a frame */
        if (w->won < 255) w->won++;
        for (int k = 0; k < 4 && w->n_en > 0; k++) {
            enemy *e = &w->en[w->n_en - 1];
            if (w->n_kfx < MAX_KILLFX)
                w->kfx[w->n_kfx++] = (killfx){(int16_t)(e->x / FX), (int16_t)(e->y / FX), e->type, 1};
            w->n_en--;
        }
        w->n_sh = 0;
        w->boss = -1;
        if (w->n_en) w->events |= EV_KILL;
        return;
    }
    w->t++;
    /* the gardener */
    hero *h = &w->h;
    int l = alen(ix, iy);
    if (l > 64) { ix = ix * 64 / l; iy = iy * 64 / l; l = 64; }
    h->moving = l > 8;
    if (h->moving) {
        h->vx = (int16_t)(ix * w->speed / 64);
        h->vy = (int16_t)(iy * w->speed / 64);
        h->x += h->vx;
        h->y += h->vy;
        h->dx = (int16_t)(ix * 64 / l);
        h->dy = (int16_t)(iy * 64 / l);
        if (ix < -8) h->face = 1;
        else if (ix > 8) h->face = 0;
        h->walk_t++;
    } else {
        h->vx = h->vy = 0;
    }
    if (h->hurt_t) h->hurt_t--;
    if (w->regen && ++w->regen_t >= REGEN_TICK) {
        w->regen_t = 0;
        if (h->hp < h->hpmax) h->hp = h->hp + w->regen > h->hpmax ? h->hpmax : h->hp + w->regen;
    }
    if (!w->won) spawner(w);
    grid_build(w);
    tools_step(w);
    shots_step(w);
    enemies_step(w);
    compact(w);
    grid_build(w);
    separate(w);
    gems_step(w);
    items_step(w);
}

/* ---- level-ups -------------------------------------------------------------------------------------------------------------- */
static int owns_passive(const world *w, int id) { return w->pas[id] > 0; }

void world_offer(world *w)
{
    choice cand[40];
    int wt[40], n = 0, sum = 0;
#define ADD(k, i, lv, weight) do { cand[n] = (choice){(uint8_t)(k), (uint8_t)(i), (uint8_t)(lv), 0}; wt[n] = (weight); \
                                   sum += wt[n]; n++; } while (0)
    for (int i = 0; i < w->n_wp; i++) {
        const weapon *p = &w->wp[i];
        if (p->evo) continue;
        if (p->level < MAX_LEVEL) ADD(CH_WUP, p->id, p->level + 1, 8);
        else if (owns_passive(w, EVOLVE_WITH[p->id])) ADD(CH_EVOLVE, p->id, MAX_LEVEL, 20);
    }
    if (w->n_wp < MAX_WEAPONS)
        for (int id = 0; id < W_COUNT; id++)
            if (!weapon_level_of(w, id)) ADD(CH_WNEW, id, 1, 4);
    for (int i = 0; i < w->n_pas; i++) {
        int id = w->pas_order[i];
        if (w->pas[id] < MAX_LEVEL) ADD(CH_PUP, id, w->pas[id] + 1, 5);
    }
    if (w->n_pas < MAX_PASSIVES)
        for (int id = 0; id < P_COUNT; id++)
            if (!w->pas[id]) ADD(CH_PNEW, id, 1, 3);
#undef ADD
    w->n_offer = 0;
    while (w->n_offer < 3 && n > 0) {
        int r = rnd(w, sum), k = 0;
        while (k < n - 1 && r >= wt[k]) { r -= wt[k]; k++; }
        w->offer[w->n_offer++] = cand[k];
        sum -= wt[k];
        cand[k] = cand[n - 1];
        wt[k] = wt[n - 1];
        n--;
    }
    if (w->n_offer == 0) {                            /* everything at its top: a snack or a pouch of coins */
        w->offer[w->n_offer++] = (choice){CH_HEAL, 0, 0, 0};
        w->offer[w->n_offer++] = (choice){CH_COINS, 0, 0, 0};
    }
}

void world_apply(world *w, const choice *c)
{
    switch (c->kind) {
    case CH_WNEW: add_weapon(w, c->id); break;
    case CH_WUP:
    case CH_EVOLVE:
        for (int i = 0; i < w->n_wp; i++)
            if (w->wp[i].id == c->id) {
                if (c->kind == CH_EVOLVE) w->wp[i].evo = 1;
                else if (w->wp[i].level < MAX_LEVEL) w->wp[i].level++;
            }
        break;
    case CH_PNEW:
        if (w->n_pas < MAX_PASSIVES && !w->pas[c->id]) {
            w->pas_order[w->n_pas++] = c->id;
            w->pas[c->id] = 1;
        }
        break;
    case CH_PUP:
        if (w->pas[c->id] && w->pas[c->id] < MAX_LEVEL) w->pas[c->id]++;
        break;
    case CH_HEAL:
        w->h.hp = w->h.hp + 30 * FX > w->h.hpmax ? w->h.hpmax : w->h.hp + 30 * FX;
        break;
    case CH_COINS:
        w->coins += 25;
        break;
    }
    stats_update(w);
}

void world_choose(world *w, int k)
{
    if (k < 0 || k >= w->n_offer) k = 0;
    world_apply(w, &w->offer[k]);
    if (w->pending > 0) w->pending--;
    w->n_offer = 0;
}

/* a chest: n upgrades, an evolution first when one is ready, else levels of what is owned (else coins) */
void world_open_chest(world *w, int n);
void world_open_chest(world *w, int n)
{
    w->n_chest = 0;
    for (int k = 0; k < n && k < 3; k++) {
        choice c = {CH_COINS, 0, 0, 0};
        int found = 0;
        for (int i = 0; i < w->n_wp && !found; i++) {
            const weapon *p = &w->wp[i];
            if (!p->evo && p->level >= MAX_LEVEL && owns_passive(w, EVOLVE_WITH[p->id])) {
                c = (choice){CH_EVOLVE, p->id, MAX_LEVEL, 0};
                found = 1;
            }
        }
        if (!found) {
            choice cand[16];
            int m = 0;
            for (int i = 0; i < w->n_wp; i++)
                if (!w->wp[i].evo && w->wp[i].level < MAX_LEVEL)
                    cand[m++] = (choice){CH_WUP, w->wp[i].id, (uint8_t)(w->wp[i].level + 1), 0};
            for (int i = 0; i < w->n_pas; i++) {
                int id = w->pas_order[i];
                if (w->pas[id] < MAX_LEVEL) cand[m++] = (choice){CH_PUP, (uint8_t)id, (uint8_t)(w->pas[id] + 1), 0};
            }
            if (m) c = cand[rnd(w, m)];
        }
        w->chest[w->n_chest++] = c;
        world_apply(w, &c);
    }
    w->chest_open = 0;
}

/* ---- CONTINUE ----------------------------------------------------------------------------------------------------------- */
void world_snapshot(const world *w, snapshot *s)
{
    memset(s, 0, sizeof *s);
    s->chr = (int8_t)w->chr;
    s->map = (int8_t)w->map;
    s->level = (uint8_t)(w->level > 255 ? 255 : w->level);
    s->kills = (uint16_t)(w->kills > 65535 ? 65535 : w->kills);
    s->coins = (uint16_t)(w->coins > 65535 ? 65535 : w->coins);
    s->t = w->t;
    s->xp = w->xp;
    s->hp = w->h.hp;
    s->seed = w->rng.s;
    s->events_done = w->events_done;
    s->bosses_done = (uint8_t)w->bosses_done;
    s->n_wp = (uint8_t)w->n_wp;
    for (int i = 0; i < w->n_wp; i++) {
        s->wp[i][0] = w->wp[i].id;
        s->wp[i][1] = w->wp[i].level;
        s->wp[i][2] = w->wp[i].evo;
    }
    s->n_pas = (uint8_t)w->n_pas;
    for (int i = 0; i < w->n_pas; i++) {
        s->pas[i][0] = w->pas_order[i];
        s->pas[i][1] = w->pas[w->pas_order[i]];
    }
}

void world_resume(world *w, const snapshot *s, const uint8_t boosts[B_COUNT])
{
    world_init(w, s->chr, s->map, boosts, s->seed);
    w->rng.s = s->seed ? s->seed : 0x5eed;
    w->t = s->t;
    w->level = s->level ? s->level : 1;
    w->xp_need = xp_for(w->level);
    w->xp = s->xp;
    w->kills = s->kills;
    w->coins = s->coins;
    w->events_done = s->events_done;
    w->bosses_done = s->bosses_done;
    w->n_wp = 0;
    for (int i = 0; i < s->n_wp && i < MAX_WEAPONS; i++) {
        if (s->wp[i][0] >= W_COUNT) continue;
        weapon *p = &w->wp[w->n_wp++];
        memset(p, 0, sizeof *p);
        p->id = s->wp[i][0];
        p->level = (uint8_t)(s->wp[i][1] < 1 ? 1 : s->wp[i][1] > MAX_LEVEL ? MAX_LEVEL : s->wp[i][1]);
        p->evo = s->wp[i][2] ? 1 : 0;
        p->timer = 30;
    }
    for (int i = 0; i < s->n_pas && i < MAX_PASSIVES; i++) {
        if (s->pas[i][0] >= P_COUNT) continue;
        w->pas_order[w->n_pas++] = s->pas[i][0];
        w->pas[s->pas[i][0]] = (uint8_t)(s->pas[i][1] > MAX_LEVEL ? MAX_LEVEL : s->pas[i][1]);
    }
    /* a boss that was out when the run was left comes back */
    for (int i = 0; i < N_EVENTS; i++)
        if (EVENTS[i].kind == EVK_BOSS && (w->events_done & (1u << i)) &&
            !(w->bosses_done & (1 << (EVENTS[i].type - ET_BOSS0))))
            w->events_done &= ~(1u << i);
    stats_update(w);
    w->h.hp = s->hp > 0 && s->hp <= w->h.hpmax ? s->hp : w->h.hpmax;
}

uint32_t world_hash(const world *w)
{
    uint32_t v = 2166136261u;
    const uint32_t a[] = {(uint32_t)w->h.x, (uint32_t)w->h.y, (uint32_t)w->h.hp, (uint32_t)w->n_en, (uint32_t)w->n_sh,
                          (uint32_t)w->n_gm, (uint32_t)w->kills, (uint32_t)w->level, (uint32_t)w->xp, w->rng.s};
    for (unsigned i = 0; i < sizeof a / sizeof a[0]; i++) v = (v ^ a[i]) * 16777619u;
    for (int i = 0; i < w->n_en; i++) v = (v ^ (uint32_t)(w->en[i].x * 31 + w->en[i].y + w->en[i].hp)) * 16777619u;
    return v;
}

/* (tests, screenshots: --opt time=S) the spawner's events before second s are done, its bosses beaten */
void world_skip_to(world *w, int s)
{
    for (int i = 0; i < N_EVENTS; i++)
        if (EVENTS[i].sec < s) {
            w->events_done |= 1u << i;
            if (EVENTS[i].kind == EVK_BOSS) w->bosses_done |= 1 << (EVENTS[i].type - ET_BOSS0);
        }
}
