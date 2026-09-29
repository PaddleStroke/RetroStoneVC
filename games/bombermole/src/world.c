/*
 * Bomber Mole: the world simulation (all three depths run at once).
 * All rights reserved, 8BCraft.
 */
#include "bm.h"
#include <stdlib.h>
#include <string.h>

const int DX[4] = {0, 1, 0, -1};
const int DY[4] = {-1, 0, 1, 0};

world W;
int pending_depth = -1, pending_from = -1;

#define BLAST_FRAMES 30
#define FUSE 150
#define DIG_TIME 40
#define DIG_TIME_DRY 20
#define LEAF_TIME 12
#define ROOT_REGROW 480
#define DIRT_REGROW 720
#define INVUL 120
#define SPLAT_TIME 240

static rs_rng rng;

/* ---- terrain properties ------------------------------------------------------------ */
int terrain_walkable(int t, int enemy)
{
    switch (t) {
    case TR_FLOOR: case TR_PUDDLE: case TR_THIN: case TR_EXIT: case TR_BRIDGE: case TR_ICE:
    case TR_THIN_ICE: case TR_MUD: case TR_COVER: case TR_BURNT: case TR_PLATE: case TR_VENT:
    case TR_HOLE_UP: case TR_LADDER:
        return 1;
    case TR_HOLE_DOWN: case TR_PIPE:
        return !enemy;
    default:
        return 0;
    }
}

static int blast_stops(const cell *c)
{
    switch (c->t) {
    case TR_STONE: case TR_DIRT: case TR_ROCK: case TR_ROOTS: case TR_FROZEN: case TR_LEAVES:
    case TR_PUDDLE: case TR_CRATE: case TR_SPRINKLER: case TR_WINDMILL: case TR_LEVER:
        return 1;
    case TR_GATE:
        return !c->state;
    default:
        return 0;
    }
}

static void mark(int d, int x, int y)
{
    if (in_grid(x, y)) W.cell_dirty[d][y][x] = 1;
    W.dirty[d] = 1;
}

static int log_at(int d, int x, int y)
{
    for (int i = 0; i < MAX_LOGS; i++) {
        logobj *l = &W.logs[i];
        if (l->alive && l->depth == d && ((l->cx == x && l->cy == y) || (l->moving && l->tx == x && l->ty == y)))
            return i;
    }
    return -1;
}

static bomb *bomb_at(int d, int x, int y)
{
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (b->active && b->depth == d && ((b->cx == x && b->cy == y) || (b->moving && b->tx == x && b->ty == y)))
            return b;
    }
    return NULL;
}

static actor *actor_at(int d, int x, int y, const actor *except, int enemies_only)
{
    for (int i = 0; i < W.na; i++) {
        actor *a = &W.a[i];
        if (!a->alive || a == except || a->depth != d) continue;
        if (enemies_only && (a->kind == AK_MOLE || a->kind == AK_DOG)) continue;
        if ((a->cx == x && a->cy == y) || (a->moving && a->tx == x && a->ty == y)) return a;
    }
    return NULL;
}

static int cell_passable(const actor *a, int d, int x, int y)
{
    if (!in_grid(x, y)) return 0;
    const cell *c = &W.g[d][y][x];
    int enemy = a && a->kind != AK_MOLE;
    if (c->t == TR_WATER) return log_at(d, x, y) >= 0;
    if (c->t == TR_GATE) return c->state;
    if (a && a->kind == AK_BOSS && W.def->boss == BOSS_OWL && c->t != TR_STONE && c->t != TR_WINDMILL)
        return 1;                                   /* the owl flies over obstacles */
    if (a && a->kind == AK_BOSS && W.def->boss == BOSS_BADGER && c->t == TR_DIRT)
        return 1;                                   /* the badger digs */
    if (!terrain_walkable(c->t, enemy)) return 0;
    if (bomb_at(d, x, y)) return 0;
    if (enemy && actor_at(d, x, y, a, 1)) return 0;
    return 1;
}

/* ---- effects ------------------------------------------------------------------------ */
static void fx_add(int kind, int d, int x, int y, int life, int vx, int vy)
{
    for (int i = 0; i < MAX_FX; i++)
        if (!W.fx[i].life) {
            fxp *f = &W.fx[i];
            memset(f, 0, sizeof *f);
            f->kind = (uint8_t)kind;
            f->depth = (uint8_t)d;
            f->x = (int16_t)(x * 16);
            f->y = (int16_t)(y * 16);
            f->vx = (int16_t)vx;
            f->vy = (int16_t)vy;
            f->life = (int16_t)life;
            return;
        }
}

static void dust_at(int d, int cx, int cy) { fx_add(FXP_DUST, d, cx * CELL, cy * CELL, 24, 0, 0); }

/* ---- actors ------------------------------------------------------------------------- */
static actor *actor_new(int kind, int d, int x, int y)
{
    if (W.na >= MAX_ACTORS) return NULL;
    actor *a = &W.a[W.na++];
    memset(a, 0, sizeof *a);
    a->kind = (uint8_t)kind;
    a->depth = (uint8_t)d;
    a->cx = a->tx = (int8_t)x;
    a->cy = a->ty = (int8_t)y;
    a->alive = 1;
    a->dir = DIR_DOWN;
    return a;
}

actor *world_player(int p)
{
    for (int i = 0; i < W.na; i++)
        if (W.a[i].kind == AK_MOLE && W.a[i].player == p) return &W.a[i];
    return NULL;
}

int world_player_depth(int p)
{
    actor *a = world_player(p);
    return a ? a->depth : 0;
}

static void actor_px(const actor *a, int *x, int *y)
{
    *x = a->cx * CELL + (a->tx - a->cx) * a->prog / (SUB / CELL);
    *y = a->cy * CELL + (a->ty - a->cy) * a->prog / (SUB / CELL);
}

int world_gust_on(void)
{
    const level_def *L = W.def;
    return L->gust_period > 0 && (int)(W.t % (uint32_t)L->gust_period) < L->gust_active;
}

int world_vent_on(void)
{
    const level_def *L = W.def;
    return L->vent_period > 0 && (int)(W.t % (uint32_t)L->vent_period) < L->vent_active;
}

static int base_speed(const actor *a)
{
    const int season = W.season;
    switch (a->kind) {
    case AK_MOLE: return 20 + W.ps[a->player].speed * 5;
    case AK_FERRET: case AK_CAT: {
        static const int pct[3] = {85, 100, 115};        /* difficulty */
        (void)season;
        return ENEMY_TYPES[a->etype].speed * pct[clampi(W.diff, 0, 2)] / 100;
    }
    case AK_DOG: return 22;
    case AK_BOSS:
        switch (W.def->boss) {
        case BOSS_FOX: return 20;
        case BOSS_FARMER: return a->dig ? 14 : 0;       /* dig = phase 2 */
        default: return 15;
        }
    }
    return 16;
}

static int effective_speed(const actor *a)
{
    int s = a->speed ? a->speed : base_speed(a);
    const cell *c = &W.g[a->depth][a->cy][a->cx];
    if (a->sliding) return 36;
    if (a->forced) return 32;
    if (c->t == TR_MUD || c->t == TR_PUDDLE) s /= 2;
    else if (c->timer && c->regrow == 0 && c->t == TR_FLOOR && c->state == 7) s = s * 2 / 3; /* tomato splat */
    else if (W.season == SEASON_WINTER && a->depth == 0 && a->kind == AK_MOLE && c->t == TR_FLOOR) s = s * 3 / 4;
    if (c->pushdir && c->pushkind == PUSH_WIND && world_gust_on() && a->moving) {
        int pd = c->pushdir - 1;
        if (pd == a->dir) s = s * 3 / 2;
        else if (pd == (a->dir + 2) % 4) s /= 2;
    }
    return s < 4 ? 4 : s;
}

static void start_move(actor *a, int dir)
{
    if (dir == (a->dir + 2) % 4) a->since_reverse = 0;
    else if (a->since_reverse < 255) a->since_reverse++;
    a->dir = (uint8_t)dir;
    a->tx = (int8_t)(a->cx + DX[dir]);
    a->ty = (int8_t)(a->cy + DY[dir]);
    a->prog = 0;
    a->moving = 1;
    if (dir == DIR_LEFT) a->flip = 1;
    if (dir == DIR_RIGHT) a->flip = 0;
}

static void hurt_player(actor *a);
static void enemy_down(actor *a);
static void collect(actor *a);
static void explode(bomb *b);

/* leaving a cell */
static void leave_cell(actor *a, int d, int x, int y)
{
    cell *c = &W.g[d][y][x];
    if (c->t == TR_THIN_ICE) {
        if (++c->state >= 2) {
            c->t = TR_WATER;
            c->state = 0;
            fx_add(FXP_SPLASH, d, x * CELL, y * CELL, 20, 0, 0);
            sfx_at(SFX_SPLASH, x * CELL);
        }
        mark(d, x, y);
    }
    (void)a;
}

static int teleport_pipe(actor *a)
{
    const cell *c = &W.g[a->depth][a->cy][a->cx];
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                const cell *o = &W.g[d][y][x];
                if (o->t == TR_PIPE && o->chan == c->chan && !(d == a->depth && x == a->cx && y == a->cy)) {
                    if (d != a->depth && a->kind == AK_MOLE) {
                        pending_from = a->depth;
                        pending_depth = d;
                    }
                    a->depth = (uint8_t)d;
                    a->cx = a->tx = (int8_t)x;
                    a->cy = a->ty = (int8_t)y;
                    sfx(SFX_DEPTH);
                    return 1;
                }
            }
    return 0;
}

static void arrive(actor *a)
{
    int ox = a->cx, oy = a->cy;
    a->tiles_moved++;
    a->cx = a->tx;
    a->cy = a->ty;
    a->prog = 0;
    a->moving = 0;
    a->forced = 0;
    leave_cell(a, a->depth, ox, oy);
    cell *c = &W.g[a->depth][a->cy][a->cx];
    /* ice: keep sliding while the next cell is free */
    if ((c->t == TR_ICE || c->t == TR_THIN_ICE) && a->kind != AK_BOSS) {
        int nx = a->cx + DX[a->dir], ny = a->cy + DY[a->dir];
        if (cell_passable(a, a->depth, nx, ny)) {
            a->sliding = 1;
            start_move(a, a->dir);
            if (a->kind == AK_MOLE) collect(a);
            return;
        }
    }
    a->sliding = 0;
    if (a->kind != AK_MOLE) return;
    collect(a);
    switch (c->t) {
    case TR_HOLE_DOWN:
        pending_from = a->depth;
        pending_depth = a->depth + 1;
        break;
    case TR_HOLE_UP: case TR_LADDER:
        pending_from = a->depth;
        pending_depth = a->depth - 1;
        break;
    case TR_PIPE:
        teleport_pipe(a);
        break;
    case TR_EXIT:
        if (W.exit_open) W.events |= EV_EXIT;
        break;
    default:
        break;
    }
}

/* ---- items -------------------------------------------------------------------------------- */
static void collect(actor *a)
{
    cell *c = &W.g[a->depth][a->cy][a->cx];
    if (!c->item || !terrain_walkable(c->t, 0)) return;
    pstats *ps = &W.ps[a->player];
    switch (c->item) {
    case IT_GRUB:
        W.grubs_left--;
        W.events |= EV_GRUB;
        sfx(SFX_GRUB);
        if (W.grubs_left <= 0 && !W.exit_open) {
            W.exit_open = 1;
            W.events |= EV_EXIT_OPEN;
            sfx(SFX_EXIT_OPEN);
            for (int y = 0; y < GH; y++)
                for (int x = 0; x < GW; x++)
                    if (W.g[0][y][x].t == TR_EXIT) mark(0, x, y);
        }
        break;
    case IT_BOMB: ps->bombs = clampi(ps->bombs + 1, 1, 8); sfx(SFX_POWERUP); break;
    case IT_FIRE: ps->range = clampi(ps->range + 1, 1, 8); sfx(SFX_POWERUP); break;
    case IT_SPEED: ps->speed = clampi(ps->speed + 1, 0, 3); sfx(SFX_POWERUP); break;
    case IT_REMOTE: ps->remote = 1; sfx(SFX_POWERUP); break;
    case IT_HEART: ps->hearts = clampi(ps->hearts + 1, 1, 3); sfx(SFX_POWERUP); break;
    }
    if (c->item != IT_GRUB && a->kind == AK_MOLE) { W.events |= EV_PICKUP; W.pickup = c->item; }
    c->item = IT_NONE;
}

/* ---- switches: a lever flips its channel when the mole walks into it or a blast hits it ---------- */
static void lever_toggle(int d, int x, int y)
{
    int ch = W.g[d][y][x].chan;
    W.lever[ch] ^= 1;
    for (int dd = 0; dd < NDEPTH; dd++)                 /* every lever of the channel shows the state */
        for (int yy = 0; yy < GH; yy++)
            for (int xx = 0; xx < GW; xx++) {
                cell *c = &W.g[dd][yy][xx];
                if (c->t == TR_LEVER && c->chan == ch) { c->state = W.lever[ch]; mark(dd, xx, yy); }
            }
    W.chan_flash[ch] = 60;                              /* the linked gates flash for 1 s */
    sfx_at(SFX_LEVER, x * CELL);
}

/* ---- bombs -------------------------------------------------------------------------------- */
static int bomb_can_enter(int d, int x, int y)
{
    if (!in_grid(x, y)) return 0;
    const cell *c = &W.g[d][y][x];
    if (c->t == TR_WATER) return 1;                 /* it sinks */
    if (c->t == TR_GATE && !c->state) return 0;
    if (!terrain_walkable(c->t, 0)) return 0;
    if (bomb_at(d, x, y) || actor_at(d, x, y, NULL, 0)) return 0;
    return 1;
}

static void bomb_start_slide(bomb *b, int dir)
{
    int nx = b->cx + DX[dir], ny = b->cy + DY[dir];
    if (!bomb_can_enter(b->depth, nx, ny)) return;
    b->moving = 1;
    b->dir = (uint8_t)dir;
    b->tx = (int8_t)nx;
    b->ty = (int8_t)ny;
    b->prog = 0;
}

static void bomb_remove(bomb *b)
{
    if (b->owner < MAX_PLAYERS) W.ps[b->owner].placed--;
    b->active = 0;
}

static void bomb_fizzle(bomb *b)
{
    fx_add(FXP_STEAM, b->depth, b->cx * CELL, b->cy * CELL, 30, 0, -8);
    sfx_at(SFX_FIZZLE, b->cx * CELL);
    bomb_remove(b);
}

static void bomb_arrive(bomb *b)
{
    b->cx = b->tx;
    b->cy = b->ty;
    b->moving = 0;
    b->prog = 0;
    cell *c = &W.g[b->depth][b->cy][b->cx];
    if (c->t == TR_WATER && log_at(b->depth, b->cx, b->cy) < 0) {
        fx_add(FXP_SPLASH, b->depth, b->cx * CELL, b->cy * CELL, 20, 0, 0);
        bomb_fizzle(b);
        return;
    }
    if (c->t == TR_HOLE_DOWN && b->depth < NDEPTH - 1) {   /* falls to the depth below */
        b->depth++;
        sfx_at(SFX_DEPTH, b->cx * CELL);
        return;
    }
    if (c->t == TR_ICE || c->t == TR_THIN_ICE) bomb_start_slide(b, b->dir);
}

static void place_bomb(actor *a)
{
    pstats *ps = &W.ps[a->player];
    if (ps->placed >= ps->bombs || a->stun) return;
    int x = a->cx, y = a->cy;
    if (a->moving && a->prog > SUB / 2) { x = a->tx; y = a->ty; }
    int d = a->depth;
    /* facing an open hole: toss the bomb down */
    int fx = x + DX[a->dir], fy = y + DY[a->dir];
    int toss = !a->moving && in_grid(fx, fy) && W.g[d][fy][fx].t == TR_HOLE_DOWN && d < NDEPTH - 1;
    const cell *c = &W.g[d][y][x];
    if (!toss && (c->t == TR_HOLE_DOWN || c->t == TR_HOLE_UP || c->t == TR_LADDER || c->t == TR_PIPE ||
                  c->t == TR_EXIT || bomb_at(d, x, y)))
        return;
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (b->active) continue;
        memset(b, 0, sizeof *b);
        b->active = 1;
        b->owner = a->player;
        b->range = (uint8_t)ps->range;
        b->fuse = FUSE;
        b->order = ++W.bomb_order;
        b->dir = a->dir;
        if (toss) {
            b->depth = (uint8_t)(d + 1);
            b->cx = b->tx = (int8_t)fx;
            b->cy = b->ty = (int8_t)fy;
        } else {
            b->depth = (uint8_t)d;
            b->cx = b->tx = (int8_t)x;
            b->cy = b->ty = (int8_t)y;
        }
        ps->placed++;
        a->timer = 12;                              /* "place bomb" pose */
        sfx_at(toss ? SFX_DEPTH : SFX_BOMB_DROP, x * CELL);
        if (!toss && (c->t == TR_ICE || c->t == TR_THIN_ICE)) bomb_start_slide(b, a->dir);  /* kicked */
        return;
    }
}

static void detonate_oldest(int p)
{
    bomb *best = NULL;
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (b->active && b->owner == p && (!best || b->order < best->order)) best = b;
    }
    if (best) best->fuse = 1;
}

/* explosions wake sleepers around, and on the depth above or below */
static void noise(int d, int x, int y)
{
    for (int i = 0; i < W.na; i++) {
        actor *a = &W.a[i];
        if (!a->alive || !a->asleep) continue;
        int dist = abs(a->cx - x) + abs(a->cy - y);
        int dz = abs(a->depth - d);
        if ((dz == 0 && dist <= 6) || (dz == 1 && dist <= 4)) {
            a->asleep = 0;
            a->stun = 30;
            if (a->kind == AK_DOG) sfx_at(SFX_WOOF, a->cx * CELL);
        }
    }
}

static void set_regrow(int d, int x, int y, int to, int frames)
{
    cell *c = &W.g[d][y][x];
    c->regrow = (uint8_t)to;
    c->timer = (uint16_t)frames;
}

static int near_roots(int d, int x, int y)
{
    for (int k = 0; k < 4; k++) {
        int nx = x + DX[k], ny = y + DY[k];
        if (in_grid(nx, ny) && W.g[d][ny][nx].t == TR_ROOTS) return 1;
    }
    return 0;
}

static void break_cell(int d, int x, int y)
{
    cell *c = &W.g[d][y][x];
    switch (c->t) {
    case TR_DIRT: case TR_LEAVES: case TR_ROCK: case TR_FROZEN: case TR_CRATE:
        if (c->t == TR_DIRT && near_roots(d, x, y)) set_regrow(d, x, y, TR_DIRT, DIRT_REGROW);
        if (c->t == TR_CRATE) sfx_at(SFX_SPLAT, x * CELL);
        c->t = TR_FLOOR;
        c->state = 0;
        dust_at(d, x, y);
        sfx_at(SFX_BREAK, x * CELL);
        break;
    case TR_ROOTS:
        c->t = TR_FLOOR;
        set_regrow(d, x, y, TR_ROOTS, ROOT_REGROW);
        dust_at(d, x, y);
        break;
    case TR_COVER:
        c->t = TR_BURNT;
        break;
    case TR_BRIDGE:
        c->t = TR_WATER;
        fx_add(FXP_SPLASH, d, x * CELL, y * CELL, 20, 0, 0);
        break;
    case TR_THIN_ICE:
        c->t = TR_WATER;
        break;
    case TR_THIN:
        /* the floor gives way: a hole down, a hole up below, rubble stuns what is below */
        c->t = TR_HOLE_DOWN;
        if (d < NDEPTH - 1) {
            cell *b = &W.g[d + 1][y][x];
            if (b->t != TR_HOLE_UP && b->t != TR_LADDER) {
                b->t = TR_HOLE_UP;
                b->regrow = 0;
                mark(d + 1, x, y);
            }
            dust_at(d + 1, x, y);
            for (int i = 0; i < W.na; i++) {
                actor *a = &W.a[i];
                if (!a->alive || a->kind == AK_MOLE) continue;
                if (a->depth == d + 1 && a->cx == x && a->cy == y) a->stun = 180;
                if (a->depth == d && a->cx == x && a->cy == y && !a->moving) {   /* falls, stunned */
                    a->depth++;
                    a->stun = 180;
                    a->state = 0;
                }
            }
        }
        sfx_at(SFX_BREAK, x * CELL);
        break;
    default:
        return;
    }
    mark(d, x, y);
}

static void blast_cell(int d, int x, int y, int shape)
{
    W.blast[d][y][x] = BLAST_FRAMES;
    W.shape[d][y][x] = (uint8_t)shape;
    mark(d, x, y);
    bomb *o = bomb_at(d, x, y);
    if (o && o->fuse > 4) o->fuse = 4;              /* chain reaction */
}

static void explode(bomb *b)
{
    int d = b->depth, x = b->cx, y = b->cy;
    bomb_remove(b);
    blast_cell(d, x, y, FX_CENTER);
    static const int ends[4] = {FX_END_UP, FX_END_RIGHT, FX_END_DOWN, FX_END_LEFT};
    for (int k = 0; k < 4; k++) {
        for (int r = 1; r <= b->range; r++) {
            int nx = x + DX[k] * r, ny = y + DY[k] * r;
            if (!in_grid(nx, ny)) break;
            cell *c = &W.g[d][ny][nx];
            if (blast_stops(c)) {
                if (c->t == TR_LEVER) lever_toggle(d, nx, ny);
                if (c->t != TR_STONE && c->t != TR_PUDDLE && c->t != TR_SPRINKLER && c->t != TR_WINDMILL &&
                    c->t != TR_LEVER && c->t != TR_GATE) {
                    break_cell(d, nx, ny);
                    blast_cell(d, nx, ny, ends[k]);
                }
                break;
            }
            break_cell(d, nx, ny);
            blast_cell(d, nx, ny, r == b->range ? ends[k] : (k & 1) ? FX_H : FX_V);
        }
    }
    break_cell(d, x, y);
    noise(d, x, y);
    W.shake = 6;
    sfx_at(SFX_BLAST, x * CELL);
}

static void update_bombs(void)
{
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (!b->active) continue;
        if (b->moving) {
            b->prog += 48;
            if (b->prog >= SUB) bomb_arrive(b);
            if (!b->active) continue;
            if (!b->moving && W.g[b->depth][b->cy][b->cx].t != TR_ICE) {
                /* stopped */
            }
        }
        int remote = b->owner < MAX_PLAYERS && W.ps[b->owner].remote;
        if (b->fuse <= 4 || !remote) b->fuse--;
        if (b->fuse == 60 || b->fuse == 30) sfx_at(SFX_FUSE, b->cx * CELL);
        if (b->fuse <= 0) explode(b);
    }
}

/* ---- blasts, hits ------------------------------------------------------------------------------ */
static void hurt_player(actor *a)
{
    if (a->invul || !a->alive || a->state == 99 || dev_god || rs_option_int("god", 0)) return;
    pstats *ps = &W.ps[a->player];
    ps->hearts--;
    if (ps->hearts <= 0) {
        a->state = 99;                              /* knocked out */
        a->timer = 90;
        a->moving = 0;
        sfx(SFX_KO);
    } else {
        a->invul = INVUL;
        sfx(SFX_HURT);
    }
}

static void enemy_down(actor *a)
{
    if (a->kind == AK_BOSS) {
        if (a->invul) return;
        if (W.def->boss == BOSS_FARMER && !a->dig) return;   /* shielded by his crates */
        a->hp--;
        a->invul = 90;
        sfx(SFX_BOSS_HIT);
        if (a->hp > 0) return;
        /* the boss drops the last grub */
        cell *c = &W.g[a->depth][a->cy][a->cx];
        c->item = IT_GRUB;
        W.boss_alive = 0;
        W.events |= EV_BOSS_DOWN;
    }
    if (a->kind == AK_DOG) { a->stun = 120; return; }
    if ((a->kind == AK_FERRET || a->kind == AK_CAT) && a->hp > 1) {   /* tier 4: two hits */
        if (a->invul) return;
        a->hp--;
        a->invul = 60;
        a->stun = 30;
        sfx_at(SFX_BOSS_HIT, a->cx * CELL);
        return;
    }
    a->alive = 0;
    dust_at(a->depth, a->cx, a->cy);
    fx_add(FXP_STAR, a->depth, a->cx * CELL, a->cy * CELL - 8, 40, 0, -6);
    sfx_at(SFX_ENEMY_DOWN, a->cx * CELL);
}

static void update_blasts(void)
{
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                if (!W.blast[d][y][x]) continue;
                if (--W.blast[d][y][x] == 0) mark(d, x, y);
                else if (W.blast[d][y][x] % 10 == 0) mark(d, x, y);
            }
    for (int i = 0; i < W.na; i++) {
        actor *a = &W.a[i];
        if (!a->alive) continue;
        int px, py;
        actor_px(a, &px, &py);
        int cx = (px + 8) / CELL, cy = (py + 8) / CELL;
        int hit = 0;
        if (a->kind == AK_BOSS) {                   /* 2x2 footprint */
            for (int dy = -1; dy <= 0; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (in_grid(cx + dx, cy + dy) && W.blast[a->depth][cy + dy][cx + dx] > BLAST_FRAMES - 20) hit = 1;
        } else if (in_grid(cx, cy) && W.blast[a->depth][cy][cx] > BLAST_FRAMES - 20) {
            hit = 1;
        }
        if (!hit) continue;
        if (a->kind == AK_MOLE) hurt_player(a);
        else enemy_down(a);
    }
}

/* ---- switches, gates, timers ------------------------------------------------------------------ */
static void update_switches(void)
{
    uint8_t pressed[NCHAN] = {0};
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                cell *c = &W.g[d][y][x];
                if (c->t == TR_PLATE) {
                    int on = actor_at(d, x, y, NULL, 0) != NULL || bomb_at(d, x, y) != NULL || log_at(d, x, y) >= 0;
                    if (on != c->state) { c->state = (uint8_t)on; mark(d, x, y); if (on) sfx(SFX_SWITCH); }
                    if (on) pressed[c->chan] = 1;
                }
                /* regrowth and splats */
                if (c->timer) {
                    if (c->regrow && --c->timer == 0) {
                        if (!actor_at(d, x, y, NULL, 0) && !bomb_at(d, x, y) && c->t == TR_FLOOR) {
                            c->t = c->regrow;
                            dust_at(d, x, y);
                            mark(d, x, y);
                        }
                        c->regrow = 0;
                    } else if (!c->regrow && c->t == TR_FLOOR && c->state == 7 && --c->timer == 0) {
                        c->state = 0;
                        mark(d, x, y);
                    }
                }
            }
    for (int ch = 0; ch < NCHAN; ch++) {
        int on = pressed[ch] || W.lever[ch];
        if (on && !W.chan_on[ch]) W.chan_timer[ch] = 0xffff;   /* rising edge: start timed gates */
        W.chan_on[ch] = (uint8_t)on;
    }
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                cell *c = &W.g[d][y][x];
                if (c->t != TR_GATE) continue;
                int open;
                if (c->timed) {
                    if (W.chan_timer[c->chan] == 0xffff) c->timer = (uint16_t)(c->timed * 6);
                    if (c->timer) c->timer--;
                    open = c->timer > 0;
                } else {
                    open = W.chan_on[c->chan];
                }
                if (!open && c->state && (actor_at(d, x, y, NULL, 0) || bomb_at(d, x, y))) open = 1;
                if (open != c->state) {
                    c->state = (uint8_t)open;
                    mark(d, x, y);
                    sfx(SFX_SWITCH);
                }
            }
    for (int ch = 0; ch < NCHAN; ch++)
        if (W.chan_timer[ch] == 0xffff) W.chan_timer[ch] = 0;
}

/* ---- windmill lanes --------------------------------------------------------------------------------
 * A windmill blows AWAY from itself, the way its sails face (cell.blow; front-view windmills blow down):
 * its lane is the straight line of cells in front of it up to the first solid cell (a wall, a block, a
 * closed gate...). Cells behind a block are sheltered. Recomputed every frame (a dug or blasted block
 * lengthens the lane); cells with a push field of their own (the level's gale lanes) keep it. */
static int wind_stops(const cell *c)
{
    switch (c->t) {
    case TR_STONE: case TR_DIRT: case TR_ROCK: case TR_ROOTS: case TR_FROZEN: case TR_CRATE:
    case TR_SPRINKLER: case TR_WINDMILL: case TR_LEVER:
        return 1;
    case TR_GATE:
        return !c->state;
    default:
        return 0;
    }
}

int world_wind_lanes(int counts[4])
{
    int n = 0;
    if (counts) memset(counts, 0, 4 * sizeof counts[0]);
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                cell *c = &W.g[d][y][x];
                if (c->lane) { c->lane = 0; c->pushdir = 0; c->pushkind = PUSH_NONE; }
            }
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                const cell *w = &W.g[d][y][x];
                if (w->t != TR_WINDMILL) continue;
                int dir = w->blow ? w->blow - 1 : DIR_DOWN;
                for (int nx = x + DX[dir], ny = y + DY[dir]; in_grid(nx, ny); nx += DX[dir], ny += DY[dir]) {
                    cell *c = &W.g[d][ny][nx];
                    if (wind_stops(c)) break;
                    if (c->pushdir && !c->lane) continue;          /* a push field of its own */
                    if (!c->lane) {
                        c->pushdir = (uint8_t)(dir + 1);
                        c->pushkind = PUSH_WIND;
                        c->lane = 1;
                        if (counts) counts[dir]++;
                        n++;
                    }
                }
            }
    return n;
}

/* ---- push fields (wind, currents) -------------------------------------------------------------- */
static void update_push(void)
{
    int gust = world_gust_on();
    int wind_tick = gust && W.def->gust_step > 0 && W.t % (uint32_t)W.def->gust_step == 0;
    int flow_tick = W.t % 30 == 0;
    if (!wind_tick && !flow_tick) goto particles;
    for (int i = 0; i < W.na; i++) {
        actor *a = &W.a[i];
        if (!a->alive || a->moving || a->kind == AK_BOSS) continue;
        cell *c = &W.g[a->depth][a->cy][a->cx];
        if (!c->pushdir) continue;
        if ((c->pushkind == PUSH_WIND && !wind_tick) || (c->pushkind == PUSH_FLOW && !flow_tick)) continue;
        int dir = c->pushdir - 1;
        if (!cell_passable(a, a->depth, a->cx + DX[dir], a->cy + DY[dir])) continue;
        int face = a->dir;
        start_move(a, dir);
        a->dir = (uint8_t)face;                     /* pushed, not walking */
        a->tx = (int8_t)(a->cx + DX[dir]);
        a->ty = (int8_t)(a->cy + DY[dir]);
        a->forced = 1;
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (!b->active || b->moving) continue;
        cell *c = &W.g[b->depth][b->cy][b->cx];
        if (!c->pushdir) continue;
        if ((c->pushkind == PUSH_WIND && !wind_tick) || (c->pushkind == PUSH_FLOW && !flow_tick)) continue;
        bomb_start_slide(b, c->pushdir - 1);
    }
    for (int i = 0; i < MAX_LOGS; i++) {
        logobj *l = &W.logs[i];
        if (!l->alive || l->moving) continue;
        cell *c = &W.g[l->depth][l->cy][l->cx];
        if (!c->pushdir || c->pushkind != PUSH_FLOW || !flow_tick) continue;
        int dir = c->pushdir - 1, nx = l->cx + DX[dir], ny = l->cy + DY[dir];
        if (in_grid(nx, ny) && W.g[l->depth][ny][nx].t == TR_WATER && log_at(l->depth, nx, ny) < 0 &&
            !actor_at(l->depth, l->cx, l->cy, NULL, 0)) {
            l->moving = 1; l->dir = (uint8_t)dir; l->tx = (int8_t)nx; l->ty = (int8_t)ny; l->prog = 0;
        }
    }
    /* wind blows leaf piles along (autumn) */
    if (wind_tick && W.season == SEASON_AUTUMN)
        for (int d = 0; d < NDEPTH; d++)
            for (int y = 0; y < GH; y++)
                for (int x = 0; x < GW; x++) {
                    cell *c = &W.g[d][y][x];
                    if (c->t != TR_LEAVES || !c->pushdir || c->pushkind != PUSH_WIND || (W.t / 16 + x + y) % 3) continue;
                    int dir = c->pushdir - 1, nx = x + DX[dir], ny = y + DY[dir];
                    if (!in_grid(nx, ny)) continue;
                    cell *n = &W.g[d][ny][nx];
                    if (n->t != TR_FLOOR || actor_at(d, nx, ny, NULL, 0) || bomb_at(d, nx, ny)) continue;
                    n->t = TR_LEAVES;
                    if (!n->item) { n->item = c->item; c->item = IT_NONE; }
                    c->t = TR_FLOOR;
                    mark(d, x, y);
                    mark(d, nx, ny);
                }
particles:
    /* streaks and spray on a few random push cells (their own random numbers: the game's stay as they are) */
    static rs_rng fxrng;
    if (W.t == 0) rs_rng_seed(&fxrng, 0xf00dcafeu);
    if ((gust && W.t % 6 == 0) || W.t % 12 == 0) {   /* the game's random sequence stays as it was */
        (void)rs_rng_range(&rng, GW);
        (void)rs_rng_range(&rng, GH);
    }
    if ((gust && W.t % 3 == 0) || W.t % 12 == 0)
    for (int k = 0; k < 10; k++) {
        int x = rs_rng_range(&fxrng, GW), y = rs_rng_range(&fxrng, GH);
        for (int d = 0; d < NDEPTH; d++) {
            cell *c = &W.g[d][y][x];
            if (!c->pushdir) continue;
            if (c->pushkind == PUSH_WIND && !gust) continue;
            int dir = c->pushdir - 1;
            if (c->pushkind == PUSH_WIND) W.wind_fx[dir]++;
            fx_add(c->pushkind == PUSH_WIND ? FXP_WIND : FXP_SPRAY, d, x * CELL, y * CELL, 32,
                   DX[dir] * (c->pushkind == PUSH_WIND ? 24 : 10), DY[dir] * (c->pushkind == PUSH_WIND ? 24 : 10));
            k = 10;                                /* one particle per tick */
        }
    }
}

static void update_logs(void)
{
    for (int i = 0; i < MAX_LOGS; i++) {
        logobj *l = &W.logs[i];
        if (!l->alive || !l->moving) continue;
        l->prog += 16;
        if (l->prog >= SUB) {
            l->cx = l->tx; l->cy = l->ty; l->moving = 0; l->prog = 0;
        }
    }
}

/* ---- sprinklers and vents ---------------------------------------------------------------------- */
static int spray_covers(int d, int x, int y)
{
    for (int k = 1; k <= 3; k++)
        for (int dir = 0; dir < 4; dir++) {
            int sx = x - DX[dir] * k, sy = y - DY[dir] * k;
            if (!in_grid(sx, sy)) continue;
            const cell *s = &W.g[d][sy][sx];
            if (s->t != TR_SPRINKLER || s->state != dir) continue;
            int ok = 1;                              /* nothing solid in between */
            for (int j = 1; j < k; j++) {
                const cell *m = &W.g[d][sy + DY[dir] * j][sx + DX[dir] * j];
                if (!terrain_walkable(m->t, 0) && m->t != TR_WATER) ok = 0;
            }
            if (ok) return 1;
        }
    return 0;
}

static void update_sprinklers(void)
{
    int turn = W.t % 120 == 0;
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                cell *c = &W.g[d][y][x];
                if (c->t != TR_SPRINKLER) continue;
                if (turn) c->state = (uint8_t)((c->state + 1) & 3);
                if (W.t % 8 == 0) {
                    int dir = c->state;
                    for (int k = 1; k <= 3; k++) {
                        int nx = x + DX[dir] * k, ny = y + DY[dir] * k;
                        if (!in_grid(nx, ny)) break;
                        const cell *m = &W.g[d][ny][nx];
                        if (!terrain_walkable(m->t, 0) && m->t != TR_WATER) break;
                        if (rs_rng_range(&rng, 2)) fx_add(FXP_SPRAY, d, nx * CELL, ny * CELL, 10, DX[dir] * 16, DY[dir] * 16);
                    }
                }
            }
    for (int i = 0; i < MAX_BOMBS; i++) {
        bomb *b = &W.b[i];
        if (b->active && !b->moving && b->fuse > 4 && spray_covers(b->depth, b->cx, b->cy)) bomb_fizzle(b);
    }
}

static void update_vents(void)
{
    const level_def *L = W.def;
    if (L->vent_period <= 0) return;
    int phase = (int)(W.t % (uint32_t)L->vent_period);
    int warn = phase >= L->vent_period - 40;
    int on = phase < L->vent_active;
    for (int d = 1; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                cell *c = &W.g[d][y][x];
                if (c->t != TR_VENT) continue;
                if ((c->state != 0) != on) { c->state = (uint8_t)on; mark(d, x, y); }
                if ((warn && W.t % 10 == 0) || (on && W.t % 3 == 0))
                    fx_add(FXP_STEAM, d, x * CELL + rs_rng_range(&rng, 6) - 3, y * CELL, on ? 24 : 12, 0, on ? -40 : -10);
                if (phase == 0) sfx_at(SFX_STEAM, x * CELL);
                if (!on) continue;
                /* the geyser lifts whatever stands on it to the depth above */
                if (!cell_passable(NULL, d - 1, x, y) && W.g[d - 1][y][x].t != TR_HOLE_DOWN) continue;
                for (int i = 0; i < W.na; i++) {
                    actor *a = &W.a[i];
                    if (!a->alive || a->moving || a->depth != d || a->cx != x || a->cy != y) continue;
                    if (a->kind == AK_MOLE) {
                        if (pending_depth < 0) { pending_from = d; pending_depth = d - 1; }
                    } else {
                        a->depth--;
                        a->stun = 60;
                    }
                }
                bomb *b = bomb_at(d, x, y);
                if (b && !b->moving) b->depth--;
            }
}

/* ---- AI ---------------------------------------------------------------------------------------- */
static int bfs_len;     /* length of the last path found by bfs_step */

/* first step of a shortest path from a to (x, y) on its depth, or -1 */
static int bfs_step(const actor *a, int gx, int gy, int maxn)
{
    static int8_t from[GH][GW];
    static uint8_t qx[GW * GH], qy[GW * GH];
    memset(from, -1, sizeof from);
    int h = 0, t = 0;
    qx[t] = (uint8_t)a->cx; qy[t] = (uint8_t)a->cy; t++;
    from[a->cy][a->cx] = 4;
    while (h < t && t < maxn) {
        int x = qx[h], y = qy[h];
        h++;
        if (x == gx && y == gy) {
            bfs_len = 1;
            while (1) {
                int k = from[y][x];
                int px = x - DX[k], py = y - DY[k];
                if (px == a->cx && py == a->cy) return k;
                x = px; y = py;
                bfs_len++;
            }
        }
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!in_grid(nx, ny) || from[ny][nx] >= 0) continue;
            if (!(nx == gx && ny == gy) && !cell_passable(a, a->depth, nx, ny)) continue;
            from[ny][nx] = (int8_t)k;
            qx[t] = (uint8_t)nx; qy[t] = (uint8_t)ny; t++;
        }
    }
    return -1;
}

/* Chase step with a stable choice. A distance field is built from the target (BFS on the grid);
 * among the neighbours one step closer the actor prefers, in order: going straight on, the axis
 * on which the target is farther, then the other. It turns back only when nothing else gets
 * closer or keeps the distance, or after 3 tiles without turning back. Returns -1 when the
 * target is farther than maxd. Recomputed only at tile centres. */
static int chase_dir(actor *a, int gx, int gy, int maxd, int *dist_out)
{
    static int16_t dist[GH][GW];
    static uint8_t qx[GW * GH], qy[GW * GH];
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) dist[y][x] = -1;
    if (!in_grid(gx, gy)) return -1;
    int h = 0, t = 0;
    dist[gy][gx] = 0;
    qx[t] = (uint8_t)gx; qy[t] = (uint8_t)gy; t++;
    while (h < t) {
        int x = qx[h], y = qy[h];
        h++;
        if (x == a->cx && y == a->cy) break;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!in_grid(nx, ny) || dist[ny][nx] >= 0) continue;
            if (!(nx == a->cx && ny == a->cy) && !cell_passable(a, a->depth, nx, ny)) continue;
            dist[ny][nx] = (int16_t)(dist[y][x] + 1);
            qx[t] = (uint8_t)nx; qy[t] = (uint8_t)ny; t++;
        }
    }
    int d0 = dist[a->cy][a->cx];
    if (d0 <= 0 || d0 > maxd) return -1;
    if (dist_out) *dist_out = d0;
    int dx = gx - a->cx, dy = gy - a->cy;
    int hx = dx > 0 ? DIR_RIGHT : DIR_LEFT, vy = dy > 0 ? DIR_DOWN : DIR_UP;
    int order[4], n = 0;
    order[n++] = a->dir;
    if (abs(dx) >= abs(dy)) { order[n++] = hx; order[n++] = vy; }
    else { order[n++] = vy; order[n++] = hx; }
    order[n++] = (a->dir + 2) % 4;
    int back = (a->dir + 2) % 4, pick = -1;
    for (int pass = 0; pass < 2 && pick < 0; pass++)          /* pass 0: closer, pass 1: not farther */
        for (int i = 0; i < 4 && pick < 0; i++) {
            int k = order[i], nx = a->cx + DX[k], ny = a->cy + DY[k];
            if (!in_grid(nx, ny) || dist[ny][nx] < 0) continue;
            if (!(nx == gx && ny == gy) && !cell_passable(a, a->depth, nx, ny)) continue;
            if (dist[ny][nx] > d0 - 1 + pass) continue;
            if (k == back && a->since_reverse < 3) {
                /* hysteresis: turn back only if nothing else keeps the distance */
                int other = 0;
                for (int j = 0; j < 4; j++) {
                    int mx = a->cx + DX[j], my = a->cy + DY[j];
                    if (j != back && in_grid(mx, my) && dist[my][mx] >= 0 && dist[my][mx] <= d0 &&
                        cell_passable(a, a->depth, mx, my)) other = 1;
                }
                if (other) continue;
            }
            pick = k;
        }
    return pick;
}

static int wander(actor *a)
{
    if (rs_rng_range(&rng, 10) < 7 && cell_passable(a, a->depth, a->cx + DX[a->dir], a->cy + DY[a->dir]))
        return a->dir;
    int opts[4], n = 0;
    for (int k = 0; k < 4; k++)
        if (cell_passable(a, a->depth, a->cx + DX[k], a->cy + DY[k]) && k != (a->dir + 2) % 4) opts[n++] = k;
    if (!n && cell_passable(a, a->depth, a->cx + DX[(a->dir + 2) % 4], a->cy + DY[(a->dir + 2) % 4]))
        return (a->dir + 2) % 4;
    return n ? opts[rs_rng_range(&rng, n)] : -1;
}

static actor *nearest_player(const actor *a, int *dist)
{
    actor *best = NULL;
    int bd = 9999;
    for (int p = 0; p < W.nplayers; p++) {
        actor *m = world_player(p);
        if (!m || !m->alive || m->state == 99 || m->depth != a->depth) continue;
        int dd = abs(m->cx - a->cx) + abs(m->cy - a->cy);
        if (dd < bd) { bd = dd; best = m; }
    }
    if (dist) *dist = bd;
    return best;
}

/* cats see along rows and columns; cover (tall grass, corn) hides the mole */
static int line_of_sight(const actor *a, const actor *m, int range, int *dir)
{
    if (m->depth != a->depth || W.g[m->depth][m->cy][m->cx].t == TR_COVER) return 0;
    int dx = m->cx - a->cx, dy = m->cy - a->cy;
    if ((dx && dy) || abs(dx) + abs(dy) > range || (!dx && !dy)) return 0;
    int k = dx > 0 ? DIR_RIGHT : dx < 0 ? DIR_LEFT : dy > 0 ? DIR_DOWN : DIR_UP;
    for (int i = 1; i < abs(dx) + abs(dy); i++) {
        const cell *c = &W.g[a->depth][a->cy + DY[k] * i][a->cx + DX[k] * i];
        if (!terrain_walkable(c->t, 1) || c->t == TR_COVER || bomb_at(a->depth, a->cx + DX[k] * i, a->cy + DY[k] * i))
            return 0;
    }
    *dir = k;
    return 1;
}

enum { CAT_PATROL, CAT_CROUCH, CAT_POUNCE, CAT_REST };

static int safe_to_enter(const actor *a, int k);

static void ai_cat(actor *a, int range, int pounce_cells, int rest)
{
    int dist, dir;
    actor *m = nearest_player(a, &dist);
    switch (a->state) {
    case CAT_CROUCH:
        if (--a->timer <= 0) { a->state = CAT_POUNCE; a->aux = (int16_t)pounce_cells; sfx_at(SFX_POUNCE, a->cx * CELL); }
        return;
    case CAT_REST:
        if (--a->timer <= 0) a->state = CAT_PATROL;
        return;
    case CAT_POUNCE:
        if (a->aux-- > 0 && cell_passable(a, a->depth, a->cx + DX[a->dir], a->cy + DY[a->dir])) {
            a->speed = 48;
            start_move(a, a->dir);
        } else {
            a->speed = 0;
            a->state = CAT_REST;
            a->timer = (int16_t)rest;
        }
        return;
    }
    if (a->kind == AK_CAT && range <= 0) m = NULL;
    if (m && line_of_sight(a, m, range, &dir)) {
        a->dir = (uint8_t)dir;
        a->flip = dir == DIR_LEFT;
        a->state = CAT_CROUCH;
        a->timer = 30;
        return;
    }
    a->speed = 0;
    if (a->kind == AK_BOSS && m && rs_rng_range(&rng, 4)) {
        int k = chase_dir(a, m->cx, m->cy, 40, NULL);
        if (k >= 0) { start_move(a, k); return; }
    }
    int k;
    if (cell_passable(a, a->depth, a->cx + DX[a->dir], a->cy + DY[a->dir]) && rs_rng_range(&rng, 8)) k = a->dir;
    else k = wander(a);
    if (k >= 0 && a->kind == AK_CAT && !safe_to_enter(a, k)) k = -1;
    if (k >= 0) start_move(a, k);
}

/* ---- typed enemies (tiers) ------------------------------------------------------------------- */
const enemy_type ENEMY_TYPES[ET_COUNT] = {
    /*  name            kind       tier variant            speed move         vis los aware       fuse react pounce rest hits */
    {"sleepy_ferret",  AK_FERRET, 1, VAR_SLEEPY_FERRET,  9, MOVE_WANDER, 0,  0, AWARE_NONE,   0,  0,  0, 0,   1},
    {"brown_ferret",   AK_FERRET, 2, VAR_BROWN_FERRET,  13, MOVE_CHASE,  6,  0, AWARE_LATE,  60, 30,  0, 0,   1},
    {"polecat",        AK_FERRET, 3, VAR_POLECAT,       16, MOVE_CHASE,  8,  0, AWARE_ALWAYS, 0, 12,  0, 0,   1},
    {"stoat",          AK_FERRET, 4, VAR_STOAT,         19, MOVE_CHASE, 12,  0, AWARE_ALWAYS, 0,  6,  0, 0,   2},
    {"ginger_cat",     AK_CAT,    1, VAR_GINGER_CAT,     9, MOVE_PATROL, 4,  1, AWARE_NONE,   0,  0,  2, 120, 1},
    {"grey_cat",       AK_CAT,    2, VAR_GREY_CAT,      12, MOVE_PATROL, 7,  1, AWARE_LATE,  60, 30,  4, 70,  1},
    {"black_cat",      AK_CAT,    3, VAR_BLACK_CAT,     14, MOVE_PATROL, 8,  1, AWARE_ALWAYS, 0, 12,  6, 60,  1},
    {"siamese_cat",    AK_CAT,    4, VAR_SIAMESE_CAT,   16, MOVE_PATROL, 9,  1, AWARE_ALWAYS, 0,  6,  6, 45,  2},
};

int enemy_type_for(int kind, int tier)
{
    tier = clampi(tier, 1, 4);
    for (int i = 0; i < ET_COUNT; i++)
        if (ENEMY_TYPES[i].kind == kind && ENEMY_TYPES[i].tier == tier) return i;
    return kind == AK_CAT ? ET_GINGER_CAT : ET_SLEEPY_FERRET;
}

static const enemy_type *etype_of(const actor *a)
{
    return (a->kind == AK_FERRET || a->kind == AK_CAT) ? &ENEMY_TYPES[a->etype] : NULL;
}

/* is (x, y) in the blast of a bomb this enemy type knows about? */
static int in_danger(int d, int x, int y, const enemy_type *T)
{
    if (!T || T->aware == AWARE_NONE) return 0;
    if (W.blast[d][y][x]) return 1;
    for (int i = 0; i < MAX_BOMBS; i++) {
        const bomb *b = &W.b[i];
        if (!b->active || b->depth != d) continue;
        if (T->aware == AWARE_LATE && b->fuse >= T->aware_fuse) continue;
        if (b->cx != x && b->cy != y) continue;
        int dist = abs(b->cx - x) + abs(b->cy - y);
        if (dist > b->range) continue;
        int k = b->cx < x ? DIR_RIGHT : b->cx > x ? DIR_LEFT : b->cy < y ? DIR_DOWN : DIR_UP, clear = 1;
        for (int s = 1; s < dist && clear; s++)
            if (blast_stops(&W.g[d][b->cy + DY[k] * s][b->cx + DX[k] * s])) clear = 0;
        if (clear) return 1;
    }
    return 0;
}

static int safe_to_enter(const actor *a, int k)
{
    const enemy_type *T = etype_of(a);
    return !in_danger(a->depth, a->cx + DX[k], a->cy + DY[k], T);
}

/* first step toward the nearest cell outside every known blast */
static int flee_step(const actor *a, const enemy_type *T)
{
    static int8_t from[GH][GW];
    static uint8_t qx[GW * GH], qy[GW * GH];
    memset(from, -1, sizeof from);
    int h = 0, t = 0;
    qx[t] = (uint8_t)a->cx; qy[t] = (uint8_t)a->cy; t++;
    from[a->cy][a->cx] = 4;
    while (h < t) {
        int x = qx[h], y = qy[h];
        h++;
        if (!(x == a->cx && y == a->cy) && !in_danger(a->depth, x, y, T)) {
            while (1) {
                int k = from[y][x];
                int px = x - DX[k], py = y - DY[k];
                if (px == a->cx && py == a->cy) return k;
                x = px; y = py;
            }
        }
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!in_grid(nx, ny) || from[ny][nx] >= 0 || !cell_passable(a, a->depth, nx, ny)) continue;
            from[ny][nx] = (int8_t)k;
            qx[t] = (uint8_t)nx; qy[t] = (uint8_t)ny; t++;
        }
    }
    return -1;
}

static int scaled_react(const enemy_type *T)
{
    static const int pct[3] = {150, 100, 70};
    return T->react * pct[clampi(W.diff, 0, 2)] / 100;
}

/* random wandering: at each cell keep going half of the time; turning back only at a dead end
 * (or rarely), so the sprite does not flip left-right-left */
static int wander_random(actor *a)
{
    if (rs_rng_range(&rng, 2) && cell_passable(a, a->depth, a->cx + DX[a->dir], a->cy + DY[a->dir]))
        return a->dir;
    int opts[4], n = 0, back = (a->dir + 2) % 4, back_ok = 0;
    for (int k = 0; k < 4; k++) {
        if (!cell_passable(a, a->depth, a->cx + DX[k], a->cy + DY[k])) continue;
        if (k == back) back_ok = 1;
        else opts[n++] = k;
    }
    if (back_ok && (!n || (a->since_reverse >= 4 && rs_rng_range(&rng, 8) == 0))) return back;
    return n ? opts[rs_rng_range(&rng, n)] : -1;
}

static void ai_typed(actor *a)
{
    const enemy_type *T = etype_of(a);
    /* bombs: notice them (after the reaction delay), then run out of the blast */
    if (T->aware != AWARE_NONE) {
        if (in_danger(a->depth, a->cx, a->cy, T)) {
            if (!a->react) a->react = (int16_t)(scaled_react(T) + 1);
            if (a->react > 1) a->react--;
            if (a->react <= 1) {
                int k = flee_step(a, T);
                if (k >= 0) { a->speed = 0; start_move(a, k); return; }
            }
        } else {
            a->react = 0;
        }
    }
    if (a->kind == AK_CAT) {
        ai_cat(a, T->vision, T->pounce, T->cooldown);
        return;
    }
    int k = -1;
    if (T->move == MOVE_CHASE && T->vision) {
        /* hysteresis: start chasing within the vision range, stop 3 cells beyond it */
        actor *m = nearest_player(a, NULL);
        int d = 0;
        if (m) k = chase_dir(a, m->cx, m->cy, T->vision + (a->chasing ? 3 : 0), &d);
        a->chasing = k >= 0;
    }
    if (k < 0) k = T->move == MOVE_WANDER ? wander_random(a) : wander(a);
    if (k >= 0 && !safe_to_enter(a, k)) {
        int opts[4], n = 0, back = (a->dir + 2) % 4;
        for (int j = 0; j < 4; j++)
            if (cell_passable(a, a->depth, a->cx + DX[j], a->cy + DY[j]) && safe_to_enter(a, j)) opts[n++] = j;
        k = -1;
        for (int j = 0; j < n && k < 0; j++)
            if (opts[j] != back) k = opts[j];
        if (k < 0 && n) k = opts[0];
    }
    if (k >= 0) start_move(a, k);
}

static void ai_dog(actor *a)
{
    /* chase the nearest cat on this depth; otherwise stay close to the mole */
    actor *target = NULL;
    int bd = 9999;
    for (int i = 0; i < W.na; i++) {
        actor *c = &W.a[i];
        if (!c->alive || c->depth != a->depth || (c->kind != AK_CAT && !(c->kind == AK_BOSS && W.def->boss == BOSS_CAT)))
            continue;
        int dd = abs(c->cx - a->cx) + abs(c->cy - a->cy);
        if (dd < bd) { bd = dd; target = c; }
    }
    int k = -1;
    if (target) k = chase_dir(a, target->cx, target->cy, 60, NULL);
    else {
        int dist;
        actor *m = nearest_player(a, &dist);
        if (m && dist > 3) k = bfs_step(a, m->cx, m->cy, 300);
        else if (rs_rng_range(&rng, 3) == 0) k = wander(a);
    }
    if (k >= 0 && cell_passable(a, a->depth, a->cx + DX[k], a->cy + DY[k])) start_move(a, k);
}

/* the summer farmer: lobs tomatoes at the mole; his crates shield him */
static void ai_farmer(actor *a)
{
    int crates = 0;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) crates += W.g[a->depth][y][x].t == TR_CRATE;
    if (!a->dig && crates == 0) {                    /* phase 2: angry and vulnerable */
        a->dig = 1;
        a->hp = 3;
        a->invul = 30;
        sfx(SFX_BOSS_HIT);
    }
    int rate = a->dig ? 70 : 110;
    if (W.t % (uint32_t)rate == 0) {
        actor *m = world_player(0);
        if (m && m->alive && m->depth == a->depth) {
            int tx = m->moving ? m->tx : m->cx, ty = m->moving ? m->ty : m->cy;
            for (int i = 0; i < MAX_FX; i++)
                if (!W.fx[i].life) {
                    fxp *f = &W.fx[i];
                    memset(f, 0, sizeof *f);
                    f->kind = FXP_TOMATO;
                    f->depth = a->depth;
                    f->x = (int16_t)(a->cx * CELL * 16);
                    f->y = (int16_t)((a->cy * CELL - 16) * 16);
                    f->vx = (int16_t)tx;             /* target cell */
                    f->vy = (int16_t)ty;
                    f->life = f->t = 60;             /* ~1 s of flight: the shadow warns */
                    break;
                }
            a->timer = 20;                          /* throw pose */
        }
    }
    if (a->dig && !a->moving) ai_cat(a, 5, 3, 70);
}

static void tomato_land(fxp *f)
{
    int d = f->depth, x = f->vx, y = f->vy;
    if (!in_grid(x, y)) return;
    if (W.g[d][y][x].t == TR_HOLE_DOWN && d < NDEPTH - 1) d++;   /* falls down the hole */
    actor *m = world_player(0);
    if (m && m->alive && m->depth == d) {
        int px, py;
        actor_px(m, &px, &py);
        if (abs(px - x * CELL) < 11 && abs(py - y * CELL) < 11) {
            m->stun = 60;
            sfx(SFX_HURT);
        }
    }
    cell *c = &W.g[d][y][x];
    if (c->t == TR_FLOOR && !c->regrow) {
        c->state = 7;                               /* splat: slows for a few seconds */
        c->timer = SPLAT_TIME;
        mark(d, x, y);
    }
    fx_add(FXP_SPLASH, d, x * CELL, y * CELL, 16, 0, 0);
    sfx_at(SFX_SPLAT, x * CELL);
}

/* ---- players ------------------------------------------------------------------------------------ */
/* ---- test bot (--opt bot=1): a naive player for the "is it beatable" tests ---------------------
 * Walks to a cell 2 cells in line with the nearest enemy, drops a bomb there
 * (in the enemy's path), walks out of every blast and away from enemies,
 * waits for the bomb, and starts again. Never uses holes, ladders or pipes. */
static int bot_mode;
static const enemy_type BOT_EYES = {"bot", 0, 0, 0, 0, MOVE_WANDER, 0, 0, AWARE_ALWAYS, 0, 0, 0, 0, 1};
static const uint16_t DIR_BITS[4] = {RS_BTN_UP, RS_BTN_RIGHT, RS_BTN_DOWN, RS_BTN_LEFT};

static int bot_enemy_near(int d, int x, int y)
{
    for (int i = 0; i < W.na; i++) {
        const actor *a = &W.a[i];
        if (!a->alive || a->depth != d || (a->kind != AK_FERRET && a->kind != AK_CAT)) continue;
        if (abs(a->cx - x) + abs(a->cy - y) <= 1 || (a->moving && abs(a->tx - x) + abs(a->ty - y) <= 1)) return 1;
    }
    return 0;
}

static int bot_unsafe(const actor *m, int x, int y)
{
    return in_danger(m->depth, x, y, &BOT_EYES) || bot_enemy_near(m->depth, x, y);
}

static int bot_goal(const actor *m, int x, int y, const actor *e)
{
    if (!e) return !bot_unsafe(m, x, y);                 /* flee: any safe cell */
    int ex = e->moving ? e->tx : e->cx, ey = e->moving ? e->ty : e->cy;
    int dx = ex - x, dy = ey - y;
    if ((dx && dy) || abs(dx) + abs(dy) != 2) return 0;
    int mx = x + dx / 2, my = y + dy / 2;                /* the cell between must be open */
    return terrain_walkable(W.g[m->depth][my][mx].t, 0) && !bot_unsafe(m, x, y);
}

/* first step toward the nearest goal cell, through safe cells only */
static int bot_search(const actor *m, const actor *e)
{
    static int8_t from[GH][GW];
    static uint8_t qx[GW * GH], qy[GW * GH];
    memset(from, -1, sizeof from);
    int h = 0, t = 0;
    qx[t] = (uint8_t)m->cx; qy[t] = (uint8_t)m->cy; t++;
    from[m->cy][m->cx] = 4;
    while (h < t) {
        int x = qx[h], y = qy[h];
        h++;
        if (bot_goal(m, x, y, e)) {
            if (x == m->cx && y == m->cy) return 4;      /* already there */
            while (1) {
                int k = from[y][x];
                int px = x - DX[k], py = y - DY[k];
                if (px == m->cx && py == m->cy) return k;
                x = px; y = py;
            }
        }
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!in_grid(nx, ny) || from[ny][nx] >= 0 || !cell_passable(m, m->depth, nx, ny)) continue;
            int tt = W.g[m->depth][ny][nx].t;
            if (tt == TR_HOLE_DOWN || tt == TR_HOLE_UP || tt == TR_LADDER || tt == TR_PIPE || tt == TR_VENT) continue;
            if (e && bot_unsafe(m, nx, ny)) continue;
            if (!e && (W.blast[m->depth][ny][nx] || bot_enemy_near(m->depth, nx, ny))) continue;   /* flee paths too */
            from[ny][nx] = (int8_t)k;
            qx[t] = (uint8_t)nx; qy[t] = (uint8_t)ny; t++;
        }
    }
    return -1;
}

static uint16_t world_bot(actor *m, uint16_t *pressed)
{
    *pressed = 0;
    if (m->moving) return DIR_BITS[m->dir];
    if (bot_unsafe(m, m->cx, m->cy)) {                   /* run out of blasts and away from enemies */
        int k = bot_search(m, NULL);
        if (k >= 0 && k < 4) { *pressed = DIR_BITS[k]; return DIR_BITS[k]; }
        return 0;
    }
    for (int i = 0; i < MAX_BOMBS; i++)
        if (W.b[i].active && W.b[i].owner == m->player) return 0;      /* wait for it */
    actor *e = NULL;
    int bd = 999;
    for (int i = 0; i < W.na; i++) {
        actor *a = &W.a[i];
        if (!a->alive || a->depth != m->depth || (a->kind != AK_FERRET && a->kind != AK_CAT)) continue;
        int dd = abs(a->cx - m->cx) + abs(a->cy - m->cy);
        if (dd < bd) { bd = dd; e = a; }
    }
    if (!e) return 0;
    int k = bot_search(m, e);
    if (k == 4) { *pressed = RS_BTN_B; return 0; }      /* in its path: bomb */
    if (k >= 0) { *pressed = DIR_BITS[k]; return DIR_BITS[k]; }
    return 0;
}

/* ---- objective bot (--opt bot=2): plays a whole level with what the game shows ---------------
 * The targets are the remaining golden grubs (shown on the pause map and counted per depth on the
 * HUD), then the exit molehill (shown by the objective arrow). A Dijkstra plan over the three depths
 * walks, digs soft dirt, bombs rocks and roots, and takes holes and ladders; it waits out its own
 * bombs and runs from blasts and enemies like the naive bot. */
enum { ACT_WALK, ACT_DIG, ACT_BOMB };

static int plan_step(const actor *m, int *act)
{
    static int16_t cost[NDEPTH][GH][GW];
    static int8_t from[NDEPTH][GH][GW];           /* direction used to enter the node */
    static uint8_t done[NDEPTH][GH][GW];
    memset(cost, 0x7f, sizeof cost);
    memset(from, -1, sizeof from);
    memset(done, 0, sizeof done);
    cost[m->depth][m->cy][m->cx] = 0;
    int gd = -1, gx = -1, gy = -1;
    for (;;) {
        int bd = -1, bx = 0, by = 0, bc = 0x7fff;
        for (int d = 0; d < NDEPTH; d++)
            for (int y = 0; y < GH; y++)
                for (int x = 0; x < GW; x++)
                    if (!done[d][y][x] && cost[d][y][x] < bc) { bc = cost[d][y][x]; bd = d; bx = x; by = y; }
        if (bd < 0) break;
        done[bd][by][bx] = 1;
        const cell *c = &W.g[bd][by][bx];
        int goal = W.grubs_left > 0 ? c->item == IT_GRUB : (c->t == TR_EXIT && W.exit_open);
        if (goal) { gd = bd; gx = bx; gy = by; break; }
        for (int k = 0; k < 4; k++) {
            int nx = bx + DX[k], ny = by + DY[k];
            if (!in_grid(nx, ny)) continue;
            const cell *n = &W.g[bd][ny][nx];
            int step;
            switch (n->t) {
            case TR_STONE: case TR_WINDMILL: case TR_SPRINKLER: case TR_LEVER: case TR_VENT: case TR_PIPE:
                continue;
            case TR_WATER:
                continue;
            case TR_GATE:
                if (!n->state) continue;
                step = 1;
                break;
            case TR_DIRT: case TR_LEAVES: step = 5; break;
            case TR_ROCK: case TR_ROOTS: case TR_FROZEN: case TR_CRATE: step = 16; break;
            default: step = 1; break;
            }
            if (bot_enemy_near(bd, nx, ny)) step += 6;
            int td = bd;
            if (n->t == TR_HOLE_DOWN && bd < NDEPTH - 1) td = bd + 1;       /* walking in: the depth below */
            if ((n->t == TR_HOLE_UP || n->t == TR_LADDER) && bd > 0) td = bd - 1;
            if (n->t == TR_HOLE_DOWN || n->t == TR_HOLE_UP || n->t == TR_LADDER) {
                /* the node reached is on the other depth; remember how we got there */
                if (cost[td][ny][nx] > bc + step) {
                    cost[td][ny][nx] = (int16_t)(bc + step);
                    from[td][ny][nx] = (int8_t)(k | (bd << 3) | 0x40);
                }
                continue;
            }
            if (cost[bd][ny][nx] > bc + step) {
                cost[bd][ny][nx] = (int16_t)(bc + step);
                from[bd][ny][nx] = (int8_t)(k | (bd << 3));
            }
        }
    }
    if (gd < 0) return -1;
    /* walk back to the first step from the mole */
    int d = gd, x = gx, y = gy, k = -1;
    while (!(d == m->depth && x == m->cx && y == m->cy)) {
        int f = from[d][y][x];
        if (f < 0) return -1;
        k = f & 3;
        int pd = (f >> 3) & 3;
        x -= DX[k];
        y -= DY[k];
        d = pd;
    }
    if (k < 0) return -1;
    int t = W.g[m->depth][m->cy + DY[k]][m->cx + DX[k]].t;
    *act = (t == TR_DIRT || t == TR_LEAVES) ? ACT_DIG :
           (t == TR_ROCK || t == TR_ROOTS || t == TR_FROZEN || t == TR_CRATE) ? ACT_BOMB : ACT_WALK;
    return k;
}

static uint16_t world_bot_objective(actor *m, uint16_t *pressed)
{
    *pressed = 0;
    if (m->moving) return DIR_BITS[m->dir];
    if (bot_unsafe(m, m->cx, m->cy)) {
        int k = bot_search(m, NULL);
        if (k >= 0 && k < 4) { *pressed = DIR_BITS[k]; return DIR_BITS[k]; }
        return 0;
    }
    for (int i = 0; i < MAX_BOMBS; i++)
        if (W.b[i].active && W.b[i].owner == m->player) return 0;      /* wait for it */
    for (int y = 0; y < GH; y++)                                        /* and for its blast to end */
        for (int x = 0; x < GW; x++)
            if (W.blast[m->depth][y][x]) return 0;
    /* an enemy in line within 2 cells: bomb it first (then the flee rule walks away) */
    for (int i = 0; i < W.na; i++) {
        const actor *e = &W.a[i];
        if (!e->alive || e->depth != m->depth || (e->kind != AK_FERRET && e->kind != AK_CAT)) continue;
        if (bot_goal(m, m->cx, m->cy, e) || abs(e->cx - m->cx) + abs(e->cy - m->cy) == 1) {
            *pressed = RS_BTN_B;
            return 0;
        }
    }
    int act = ACT_WALK;
    int k = plan_step(m, &act);
    if (k < 0) return 0;
    int nx = m->cx + DX[k], ny = m->cy + DY[k];
    if (act == ACT_BOMB) { *pressed = RS_BTN_B; return 0; }
    if (act == ACT_WALK && bot_unsafe(m, nx, ny)) return 0;             /* let it pass */
    *pressed = DIR_BITS[k];
    return DIR_BITS[k];
}

static int pad_dir(uint16_t held, uint16_t pressed)
{
    static const uint16_t bits[4] = {RS_BTN_UP, RS_BTN_RIGHT, RS_BTN_DOWN, RS_BTN_LEFT};
    for (int k = 0; k < 4; k++)
        if (pressed & bits[k]) return k;
    for (int k = 0; k < 4; k++)
        if (held & bits[k]) return k;
    return -1;
}

static void update_player(actor *a)
{
    uint16_t held = rs_pad(a->player), pr = rs_pad_pressed(a->player);
    if (bot_mode) held = bot_mode == 2 ? world_bot_objective(a, &pr) : world_bot(a, &pr);
    if (a->state == 99) {                           /* knocked out */
        if (--a->timer == 0) W.events |= EV_DEAD;
        return;
    }
    if (a->invul) a->invul--;
    if (a->timer) a->timer--;
    if (a->stun) { a->stun--; return; }
    pstats *ps = &W.ps[a->player];
    if (pr & RS_BTN_B) place_bomb(a);
    if ((pr & RS_BTN_A) && ps->remote) detonate_oldest(a->player);
    int dir = pad_dir(held, pr);
    if (a->moving) {
        if (!a->forced && !a->sliding && dir >= 0 && dir == (a->dir + 2) % 4) {   /* turn back */
            int8_t x = a->cx, y = a->cy;
            a->cx = a->tx; a->cy = a->ty; a->tx = x; a->ty = y;
            a->prog = (int16_t)(SUB - a->prog);
            a->dir = (uint8_t)dir;
            a->flip = dir == DIR_LEFT ? 1 : dir == DIR_RIGHT ? 0 : a->flip;
        }
        return;
    }
    if (dir < 0) { a->dig = 0; return; }
    int nx = a->cx + DX[dir], ny = a->cy + DY[dir];
    a->dir = (uint8_t)dir;
    if (dir == DIR_LEFT) a->flip = 1;
    if (dir == DIR_RIGHT) a->flip = 0;
    if (!in_grid(nx, ny)) return;
    cell *c = &W.g[a->depth][ny][nx];
    if (c->t == TR_WATER) {                         /* push a floating log along the water */
        int li = log_at(a->depth, nx, ny);
        int bx = nx + DX[dir], by = ny + DY[dir];
        if (li >= 0 && in_grid(bx, by) && W.g[a->depth][by][bx].t == TR_WATER && log_at(a->depth, bx, by) < 0) {
            if (pr & (RS_BTN_UP | RS_BTN_DOWN | RS_BTN_LEFT | RS_BTN_RIGHT) || a->dig++ > 10) {
                logobj *l = &W.logs[li];
                l->moving = 1; l->dir = (uint8_t)dir; l->tx = (int8_t)bx; l->ty = (int8_t)by; l->prog = 0;
                a->dig = 0;
                sfx_at(SFX_SPLASH, nx * CELL);
            }
            return;
        }
    }
    if (cell_passable(a, a->depth, nx, ny)) {
        a->dig = 0;
        start_move(a, dir);
        return;
    }
    if (c->t == TR_LEVER) {
        if (pr & (RS_BTN_UP | RS_BTN_DOWN | RS_BTN_LEFT | RS_BTN_RIGHT)) lever_toggle(a->depth, nx, ny);
        return;
    }
    if (c->t == TR_DIRT || c->t == TR_LEAVES) {     /* moles dig */
        int need = c->t == TR_LEAVES ? LEAF_TIME : W.season == SEASON_SUMMER ? DIG_TIME_DRY : DIG_TIME;
        a->dig++;
        if (a->dig % 8 == 1) {
            sfx_at(SFX_DIG, nx * CELL);
            fx_add(FXP_DUST, a->depth, nx * CELL + rs_rng_range(&rng, 8) - 4, ny * CELL + 4, 12, 0, -4);
        }
        int crack = a->dig * 2 >= need;
        if (c->state != crack && c->t == TR_DIRT) { c->state = (uint8_t)crack; mark(a->depth, nx, ny); }
        if (a->dig >= need) {
            if (c->t == TR_DIRT && near_roots(a->depth, nx, ny)) set_regrow(a->depth, nx, ny, TR_DIRT, DIRT_REGROW);
            c->t = TR_FLOOR;
            c->state = 0;
            a->dig = 0;
            dust_at(a->depth, nx, ny);
            mark(a->depth, nx, ny);
        }
        return;
    }
    a->dig = 0;
}

/* facing statistics, for the chase tests: changes of the sprite direction, and left-right
 * jitter (a change undone less than a tile later) */
static void track_facing(actor *a)
{
    if (a->kind == AK_MOLE) return;
    if (a->dir != a->face) {
        if (a->face_changes && a->dir == a->prev_face && W.t - a->face_t < 16) a->jitter++;
        a->prev_face = a->face;
        a->face = a->dir;
        a->face_changes++;
        a->face_t = W.t;
    }
}

static void update_actor(actor *a)
{
    if (!a->alive) return;
    a->anim++;
    if (a->moving) {
        a->prog = (int16_t)(a->prog + effective_speed(a));
        if (a->prog >= SUB) arrive(a);
    }
    if (a->kind == AK_MOLE) { update_player(a); return; }
    track_facing(a);
    if (a->invul) a->invul--;
    if (a->timer && a->kind != AK_CAT && a->kind != AK_BOSS) a->timer--;
    if (a->asleep) {
        if (W.t % 40 == 0) fx_add(FXP_ZZZ, a->depth, a->cx * CELL + 4, a->cy * CELL - 8, 40, 4, -8);
        return;
    }
    if (a->stun) { a->stun--; return; }
    if (a->moving) return;
    switch (a->kind) {
    case AK_FERRET: ai_typed(a); break;
    case AK_CAT: ai_typed(a); break;
    case AK_DOG: ai_dog(a); break;
    case AK_BOSS:
        if (W.def->boss == BOSS_FARMER) {
            if (a->timer) a->timer--;
            ai_farmer(a);
        } else {
            ai_cat(a, 6, 6, 70);
        }
        break;
    }
    /* badger digs through the dirt it walks into */
    if (a->kind == AK_BOSS && W.def->boss == BOSS_BADGER && a->moving) {
        cell *c = &W.g[a->depth][a->ty][a->tx];
        if (c->t == TR_DIRT) { c->t = TR_FLOOR; dust_at(a->depth, a->tx, a->ty); mark(a->depth, a->tx, a->ty); }
    }
}

static void contacts(void)
{
    for (int p = 0; p < W.nplayers; p++) {
        actor *m = world_player(p);
        if (!m || !m->alive || m->state == 99) continue;
        int mx, my;
        actor_px(m, &mx, &my);
        for (int i = 0; i < W.na; i++) {
            actor *a = &W.a[i];
            if (!a->alive || a->depth != m->depth || a->asleep) continue;
            int ax, ay;
            actor_px(a, &ax, &ay);
            if (a->kind == AK_DOG) {
                /* the dog chases cats away on contact */
                for (int j = 0; j < W.na; j++) {
                    actor *c = &W.a[j];
                    if (!c->alive || c->kind != AK_CAT || c->depth != a->depth) continue;
                    int cx, cy;
                    actor_px(c, &cx, &cy);
                    if (abs(cx - ax) < 12 && abs(cy - ay) < 12 && !a->stun) { enemy_down(c); sfx(SFX_WOOF); }
                }
                continue;
            }
            if (a->kind == AK_MOLE || a->stun) continue;
            int r = a->kind == AK_BOSS ? 16 : 11;
            if (abs(ax - mx) < r && abs(ay - my) < r) hurt_player(m);
        }
    }
}

static void update_fx(void)
{
    for (int i = 0; i < MAX_FX; i++) {
        fxp *f = &W.fx[i];
        if (!f->life) continue;
        if (f->kind == FXP_TOMATO) {
            if (--f->life == 0) tomato_land(f);
            continue;
        }
        f->x = (int16_t)(f->x + f->vx);
        f->y = (int16_t)(f->y + f->vy);
        f->t++;
        f->life--;
    }
}

/* ---- public ------------------------------------------------------------------------------------- */
void world_start(const level_def *L, const pstats *carry)
{
    memset(&W, 0, sizeof W);
    W.def = L;
    W.season = L->season;
    W.mode = MODE_SOLO;
    W.nplayers = 1;
    rs_rng_seed(&rng, 0x5eed1234u ^ (uint32_t)(L->arc * 97 + L->num * 13));
    memcpy(W.g, L->g, sizeof W.g);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        W.ps[p].bombs = L->bombs;
        W.ps[p].range = L->range;
        W.ps[p].speed = L->speed;
        W.ps[p].hearts = 1;
        W.ps[p].lives = carry ? carry[p].lives : 3;
    }
    W.diff = opt_diff;
    bot_mode = rs_option_int("bot", 0);
    int typed = 0, has_dog = 0;
    for (int i = 0; i < L->nsp; i++) {
        const spawn *s = &L->sp[i];
        if (s->kind == AK_MOLE && s->player >= W.nplayers) continue;   /* 2-4 player starts: future */
        int is_enemy = s->kind == AK_CAT || s->kind == AK_FERRET;
        if (is_enemy && W.diff == DIFF_EASY && typed++ % 3 == 2) continue;   /* easy: a third fewer */
        actor *a = actor_new(s->kind, s->depth, s->x, s->y);
        if (!a) break;
        a->asleep = s->asleep;
        a->player = s->player;
        has_dog |= s->kind == AK_DOG;
        if (s->kind == AK_BOSS) {
            a->hp = L->boss == BOSS_BADGER ? 3 : 5;
            W.boss_alive = 1;
        }
        if (is_enemy) {
            a->etype = (uint8_t)(s->etype >= 0 ? s->etype : enemy_type_for(s->kind, L->tier));
            a->hp = ENEMY_TYPES[a->etype].hits;
            a->dir = (uint8_t)rs_rng_range(&rng, 4);
            a->face = a->prev_face = a->dir;
            const char *force = rs_option("enemytype");       /* debug: every enemy of one type */
            for (int t = 0; force && t < ET_COUNT; t++)
                if (!strcmp(force, ENEMY_TYPES[t].name)) { a->kind = ENEMY_TYPES[t].kind; a->etype = (uint8_t)t; a->hp = ENEMY_TYPES[t].hits; }
        }
    }
    if (W.diff == DIFF_HARD) {
        /* hard: one more enemy on each depth that has some, far from the mole */
        int n0 = W.na;
        for (int d = 0; d < NDEPTH; d++) {
            const actor *first = NULL;
            for (int i = 0; i < n0; i++)
                if (W.a[i].depth == d && (W.a[i].kind == AK_CAT || W.a[i].kind == AK_FERRET)) { first = &W.a[i]; break; }
            if (!first) continue;
            const actor *m = world_player(0);
            for (int tries = 0; tries < 200; tries++) {
                int x = rs_rng_range(&rng, GW), y = rs_rng_range(&rng, GH);
                if (W.g[d][y][x].t != TR_FLOOR || W.g[d][y][x].item || actor_at(d, x, y, NULL, 0)) continue;
                if (m && m->depth == d && abs(m->cx - x) + abs(m->cy - y) < 7) continue;
                actor *a = actor_new(first->kind, d, x, y);
                if (a) { a->etype = first->etype; a->hp = first->hp; a->dir = (uint8_t)rs_rng_range(&rng, 4); }
                break;
            }
        }
    }
    /* sprite palettes of the enemy variants: slots 1 and 2, plus 3 without a boss and 7 without a dog */
    memset(W.var_slot, -1, sizeof W.var_slot);
    {
        int pool[4], np = 0, used = 0;
        pool[np++] = OBJ_PAL_FERRET;
        pool[np++] = OBJ_PAL_CAT;
        if (!W.boss_alive) pool[np++] = OBJ_PAL_BOSS;
        if (!has_dog) pool[np++] = OBJ_PAL_CRITTER;
        for (int i = 0; i < W.na; i++) {
            actor *a = &W.a[i];
            if (a->kind != AK_CAT && a->kind != AK_FERRET) continue;
            const enemy_type *T = &ENEMY_TYPES[a->etype];
            if (W.var_slot[T->variant] < 0) {
                if (used < np) {
                    W.var_slot[T->variant] = (int8_t)pool[used++];
                } else {                       /* out of palettes: share the closest variant of the kind */
                    int best = -1, bd = 99;
                    for (int t = 0; t < ET_COUNT; t++)
                        if (ENEMY_TYPES[t].kind == T->kind && W.var_slot[ENEMY_TYPES[t].variant] >= 0 &&
                            abs(ENEMY_TYPES[t].tier - T->tier) < bd) { bd = abs(ENEMY_TYPES[t].tier - T->tier); best = t; }
                    W.var_slot[T->variant] = best >= 0 ? W.var_slot[ENEMY_TYPES[best].variant] : (int8_t)pool[0];
                    rs_log("level %s: no free sprite palette for %s", L->file, T->name);
                }
            }
            a->pal = (uint8_t)W.var_slot[T->variant];
        }
    }
    for (int i = 0; i < L->nlogs; i++) {
        W.logs[i].alive = 1;
        W.logs[i].depth = L->logs[i].depth;
        W.logs[i].cx = W.logs[i].tx = (int8_t)L->logs[i].x;
        W.logs[i].cy = W.logs[i].ty = (int8_t)L->logs[i].y;
    }
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                W.grubs_total += W.g[d][y][x].item == IT_GRUB;
                if (W.g[d][y][x].t == TR_SPRINKLER) W.g[d][y][x].state = (uint8_t)((x + y) & 3);
            }
    W.grubs_total += W.boss_alive;
    W.grubs_left = W.grubs_total;
    world_wind_lanes(NULL);
    for (int d = 0; d < NDEPTH; d++) W.dirty[d] = 1;
    pending_depth = pending_from = -1;
}

void world_update(void)
{
    W.events = 0;
    if (W.shake) W.shake--;
    for (int i = 0; i < W.na; i++) update_actor(&W.a[i]);
    update_bombs();
    update_logs();
    world_wind_lanes(NULL);
    for (int ch = 0; ch < NCHAN; ch++)
        if (W.chan_flash[ch]) W.chan_flash[ch]--;
    update_push();
    update_sprinklers();
    update_vents();
    update_switches();
    update_blasts();
    contacts();
    update_fx();
    W.t++;
}

int world_enemies(int depth)
{
    int n = 0;
    for (int i = 0; i < W.na; i++) {
        const actor *a = &W.a[i];
        n += a->alive && a->depth == depth && (a->kind == AK_FERRET || a->kind == AK_CAT || a->kind == AK_BOSS);
    }
    return n;
}

/* something dangerous on `depth` for a mole on `from` */
int world_danger(int depth, int from)
{
    for (int i = 0; i < MAX_BOMBS; i++)
        if (W.b[i].active && W.b[i].depth == depth) return 1;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.blast[depth][y][x]) return 1;
    if (abs(depth - from) != 1) return 0;
    for (int i = 0; i < W.na; i++) {
        const actor *a = &W.a[i];
        if (!a->alive || a->depth != depth || a->kind == AK_MOLE || a->kind == AK_DOG || a->asleep) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int x = a->cx + dx, y = a->cy + dy;
                if (!in_grid(x, y)) continue;
                int t = W.g[depth][y][x].t;
                if ((depth < from && t == TR_HOLE_DOWN) || (depth > from && (t == TR_HOLE_UP || t == TR_LADDER)))
                    return 1;
            }
    }
    return 0;
}
