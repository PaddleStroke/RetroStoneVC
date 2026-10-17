/*
 * Snowball Siege: the rules. A side view of a snowy battlefield wider than the screen: hills, valleys, frozen ponds,
 * pine trees and a snow fort per side, all a 1-bit terrain (bit x & 7 of byte x / 8) that every splat carves. Turn by
 * turn, the active critter shuffles, aims, charges and throws; the wind (new every turn) pushes the throw; a splat
 * chills the critters near it (half-hearts) and knocks them away, and a critter knocked into the pond is out too.
 * The last side standing wins. No drawing, no sound, integer maths only: deterministic and testable alone
 * (tests/test_rules.c). Every number is in game.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
 */
#include "game.h"
#include <string.h>
#include <limits.h>

#define X_MIN 6
#define X_MAX (WORLD_W - 7)

static const int8_t PK[PK_COUNT][6] = PK_TABLE;
static const int32_t SIN_Q16[91] = {
    0, 1144, 2287, 3430, 4572, 5712, 6850, 7987, 9121, 10252, 11380, 12505, 13626, 14742, 15855, 16962, 18064, 19161,
    20252, 21336, 22415, 23486, 24550, 25607, 26656, 27697, 28729, 29753, 30767, 31772, 32768, 33754, 34729, 35693,
    36647, 37590, 38521, 39441, 40348, 41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930, 48703, 49461,
    50203, 50931, 51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393, 58903, 59396,
    59870, 60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856, 64104, 64332, 64540, 64729,
    64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526, 65536};

const int8_t *pk_info(int kind) { return PK[kind < 0 || kind >= PK_COUNT ? 0 : kind]; }

/* ---- maths ------------------------------------------------------------------------------------------------------ */
int32_t isin(int d)
{
    d %= 360;
    if (d < 0) d += 360;
    if (d <= 90) return SIN_Q16[d];
    if (d <= 180) return SIN_Q16[180 - d];
    if (d <= 270) return -SIN_Q16[d - 180];
    return -SIN_Q16[360 - d];
}
int32_t icos(int d) { return isin(d + 90); }

int32_t isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    int64_t r = 0, bit = (int64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int32_t)r;
}

static int iabs(int v) { return v < 0 ? -v : v; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int rnd(rs_rng *r, int n) { return n <= 1 ? 0 : rs_rng_range(r, n); }
static int rnd_pm(rs_rng *r, int e) { return e <= 0 ? 0 : rs_rng_range(r, 2 * e + 1) - e; }

/* the angle (degrees, -90..90) of the vector (ax >= 0 forward, up) */
static int atan_deg(int up, int ax)
{
    int best = 0;
    int64_t bd = -1;
    if (ax == 0) return up >= 0 ? 90 : -90;
    for (int a = -90; a <= 90; a++) {
        int64_t d = (int64_t)up * icos(a) - (int64_t)ax * isin(a);
        if (d < 0) d = -d;
        if (bd < 0 || d < bd) { bd = d; best = a; }
    }
    return best;
}

/* ---- terrain ------------------------------------------------------------------------------------------------------ */
int terrain_solid(const world *w, int x, int y)
{
    if (x < 0 || x >= WORLD_W || y < 0) return 0;
    if (y >= WORLD_H) return 1;
    return (w->ter[y][x >> 3] >> (x & 7)) & 1;
}

void terrain_set(world *w, int x, int y, int on)
{
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;
    if (on) w->ter[y][x >> 3] |= (uint8_t)(1 << (x & 7));
    else w->ter[y][x >> 3] &= (uint8_t)~(1 << (x & 7));
}

static void mark_dirty(world *w, int x0, int y0, int x1, int y1)
{
    rect16 *r = &w->dirty[w->dirty_seq % DIRTY_N];
    r->x0 = (int16_t)clampi(x0, 0, WORLD_W - 1);
    r->y0 = (int16_t)clampi(y0, 0, WORLD_H - 1);
    r->x1 = (int16_t)clampi(x1, 0, WORLD_W - 1);
    r->y1 = (int16_t)clampi(y1, 0, WORLD_H - 1);
    w->dirty_seq++;
}

void terrain_carve(world *w, int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= BEDROCK_Y) continue;
        int half = isqrt64((int64_t)r * r - (int64_t)dy * dy);
        for (int x = imax(0, cx - half); x <= imin(WORLD_W - 1, cx + half); x++) terrain_set(w, x, y, 0);
    }
    mark_dirty(w, cx - r, cy - r, cx + r, cy + r);
}

int in_pond(const world *w, int x)
{
    for (int i = 0; i < 2; i++)
        if (w->pond_x[i] && x >= w->pond_x[i] - POND_HALF && x <= w->pond_x[i] + POND_HALF) return 1;
    return 0;
}

int surface_y(const world *w, int x)
{
    for (int y = 0; y < WORLD_H; y++)
        if (terrain_solid(w, x, y)) return y;
    return WORLD_H;
}

int fort_at(const world *w, int x, int y)
{
    for (int i = 0; i < w->nfort; i++) {
        const rect16 *f = &w->fort[i];
        if (x >= f->x0 && x <= f->x1 && y >= f->y0 && y <= f->y1) return 1;
    }
    return 0;
}

static int supported(const world *w, int x, int y)
{
    for (int dx = -2; dx <= 2; dx++)
        if (terrain_solid(w, x + dx, y)) return 1;
    return 0;
}

/* the critter's leading edge (2 px ahead of its centre x) is free from its feet to its head */
static int front_free(const world *w, int x, int y, int dir)
{
    static const int8_t rows[4] = {1, 4, 8, 11};
    for (int i = 0; i < 4; i++)
        if (terrain_solid(w, x + 2 * dir, y - rows[i])) return 0;
    return 1;
}

/* ---- the battlefield ------------------------------------------------------------------------------------------------ */
int species_of_player(int p) { return p & 3; }
static int cpu_species(int level) { return 1 + (level - 1) % 3; }       /* penguins, foxes, hares, penguins... */

static const int16_t SIDE_POS[3][4] = SIDE_X;

static void lerp_heights(int *h, int x0, int x1, int target, int blend)
{
    for (int x = x0 - blend; x <= x1 + blend; x++) {
        if (x < 0 || x >= WORLD_W) continue;
        if (x >= x0 && x <= x1) { h[x] = target; continue; }
        int d = x < x0 ? x0 - x : x - x1;               /* 1..blend */
        h[x] = (target * (blend - d) + h[x] * d) / blend;
    }
}

static void wall(world *w, int x0, int x1, int top, int base)
{
    for (int x = x0; x <= x1; x++)
        for (int y = top; y < base + 6 && y < WORLD_H; y++) {
            if (y < top + CREN_H && ((x - x0) / 3) % 2) continue;     /* the crenels */
            terrain_set(w, x, y, 1);
        }
    rect16 *f = &w->fort[w->nfort++];
    f->x0 = (int16_t)x0; f->x1 = (int16_t)x1; f->y0 = (int16_t)top; f->y1 = (int16_t)(base - 1);
}

/* a new battlefield for the sides on (their critters full of hearts at home), from the world's RNG */
static void gen_field(world *w)
{
    int h[WORLD_W];
    static const int amp[3] = HILL_AMP, per[3] = HILL_PERIOD;
    rs_rng *r = &w->rng;
    int a[3], ph[3];
    for (int i = 0; i < 3; i++) { a[i] = amp[i] + rnd(r, amp[i] + 1); ph[i] = rnd(r, 360); }
    for (int x = 0; x < WORLD_W; x++) {
        int64_t v = 0;
        for (int i = 0; i < 3; i++) v += (int64_t)a[i] * isin(x * 360 / per[i] + ph[i]);
        h[x] = clampi(SURF_MID - (int)(v >> 16), SURF_MIN, SURF_MAX);
    }
    memset(w->ter, 0, sizeof w->ter);
    w->nfort = 0;
    w->pond_x[0] = w->pond_x[1] = 0;
    const int16_t *sx = SIDE_POS[w->nsides - 2];
    /* the ponds: between the middle sides (2 or 4 sides: the middle of the field; 3 sides: between each pair) */
    if (w->nsides == 3) {
        w->pond_x[0] = (int16_t)((sx[0] + sx[1]) / 2);
        w->pond_x[1] = (int16_t)((sx[1] + sx[2]) / 2);
    } else {
        w->pond_x[0] = WORLD_W / 2;
    }
    for (int i = 0; i < 2; i++) {
        int pc = w->pond_x[i];
        if (!pc) continue;
        lerp_heights(h, pc - POND_HALF - 2, pc + POND_HALF + 2, POND_Y - 1, POND_BANK);
        for (int x = pc - POND_HALF; x <= pc + POND_HALF; x++) h[x] = POND_Y + POND_DEPTH;
    }
    /* each side's flat ground, at the hills' mean height there */
    int base[MAX_SIDES];
    for (int s = 0; s < w->nsides; s++) {
        int sum = 0;
        for (int x = sx[s] - PLAT_HALF; x <= sx[s] + PLAT_HALF; x++) sum += h[x];
        base[s] = clampi(sum / (2 * PLAT_HALF + 1) + rnd_pm(r, 4), SURF_MIN + 22, SURF_MAX - 14);
        lerp_heights(h, sx[s] - PLAT_HALF, sx[s] + PLAT_HALF, base[s], PLAT_BLEND);
    }
    for (int x = 0; x < WORLD_W; x++)
        for (int y = h[x]; y < WORLD_H; y++) terrain_set(w, x, y, 1);
    /* the forts: a high wall towards the middle of the field, a low one behind (both high for a side in the middle) */
    for (int s = 0; s < w->nsides; s++) {
        int c = sx[s], dir = c < WORLD_W / 2 ? 1 : c > WORLD_W / 2 ? -1 : 0;
        for (int k = -1; k <= 1; k += 2) {
            int hgt = dir == 0 ? FORT_H * 3 / 4 : k == dir ? FORT_H : FORT_BACK_H;
            int x0 = k > 0 ? c + FORT_DIST : c - FORT_DIST - FORT_THICK + 1;
            wall(w, x0, x0 + FORT_THICK - 1, base[s] - hgt, base[s]);
        }
    }
    /* pine trees, away from the forts and the ponds */
    w->ntrees = 0;
    int want = TREES_MIN + rnd(r, TREES_RND + 1);
    for (int tries = 0; tries < 200 && w->ntrees < want; tries++) {
        int x = 12 + rnd(r, WORLD_W - 24), ok = 1;
        for (int s = 0; s < w->nsides; s++)
            if (iabs(x - sx[s]) < PLAT_HALF + 14) ok = 0;
        for (int i = 0; i < 2; i++)
            if (w->pond_x[i] && iabs(x - w->pond_x[i]) < POND_HALF + POND_BANK) ok = 0;
        for (int i = 0; i < w->ntrees; i++)
            if (iabs(x - w->trees[i].x) < 28) ok = 0;
        if (!ok) continue;
        tree *t = &w->trees[w->ntrees++];
        t->x = (int16_t)x;
        t->y = (int16_t)h[x];
        t->on = 1;
        t->kind = (int8_t)rnd(r, 2);
    }
    /* the critters, at home */
    static const int gap[TEAM] = CRITTER_GAP;
    w->ncrit = 0;
    for (int s = 0; s < w->nsides; s++) {
        int c = sx[s], dir = c <= WORLD_W / 2 ? 1 : -1;
        side *sd = &w->s[s];
        sd->next = 0;
        sd->out_turn = 0;
        sd->out_turn_set = 0;
        for (int k = 0; k < PK_COUNT; k++) sd->stock[k] = pk_info(k)[5];
        for (int i = 0; i < TEAM; i++) {
            critter *cr = &w->c[w->ncrit++];
            memset(cr, 0, sizeof *cr);
            cr->x = (int32_t)(c + gap[i] * dir) << 16;
            cr->y = (int32_t)base[s] << 16;
            cr->side = (int8_t)s;
            cr->hp = HP_MAX;
            cr->facing = (int8_t)dir;
            cr->state = CS_STAND;
            cr->apex = (int16_t)base[s];
            cr->hurt_t = 999;
            cr->anim_t = (int16_t)(i * 23 + s * 11);
            cr->last_hit = -1;
        }
    }
    memset(w->fx, 0, sizeof w->fx);
    memset(&w->p, 0, sizeof w->p);
    w->gen++;
    w->dirty_seq = 0;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    rs_rng_seed(&w->fxrng, (seed ^ 0x9e3779b9u) | 1);
    w->players = clampi(players, 1, MAX_PLAYERS);
    w->nsides = w->players >= 2 ? w->players : 2;
    w->level = 1;
    for (int s = 0; s < w->nsides; s++) {
        side *sd = &w->s[s];
        sd->on = 1;
        if (w->players == 1 && s == 1) {
            sd->player = -1;
            sd->cpu = 1;
            sd->species = (int8_t)cpu_species(1);
        } else {
            sd->player = (int8_t)s;
            sd->species = (int8_t)species_of_player(s);
        }
    }
    gen_field(w);
    w->phase = PH_DEMO;
    w->winner = -1;
    w->cur_side = 0;
    w->cur = 0;
    w->angle = AIM_START;
}

/* ---- queries ------------------------------------------------------------------------------------------------------ */
static int crit_out(const critter *c) { return c->state == CS_SNOWED || c->state == CS_SPLASHED; }

int side_alive(const world *w, int s)
{
    int n = 0;
    for (int i = 0; i < w->ncrit; i++)
        if (w->c[i].side == s && !crit_out(&w->c[i]) && w->c[i].hp > 0) n++;
    return n;
}

int side_hearts(const world *w, int s)
{
    int n = 0;
    for (int i = 0; i < w->ncrit; i++)
        if (w->c[i].side == s && !crit_out(&w->c[i])) n += w->c[i].hp;
    return n;
}

int side_rank_key(const world *w, int s)
{
    int alive = side_alive(w, s) > 0;
    int top = alive ? 1000 + (w->phase == PH_END && w->winner == s) : w->s[s].out_turn;   /* the 80th turn's winner */
    return top * 100000 + imin(w->s[s].score, 99999);
}

critter *world_active(world *w) { return &w->c[w->cur]; }
const critter *world_active_c(const world *w) { return &w->c[w->cur]; }
int world_over(const world *w) { return w->over; }

int world_human_turn(const world *w)
{
    if (w->phase == PH_DEMO || w->phase == PH_END || w->s[w->cur_side].cpu) return -1;
    return w->s[w->cur_side].player;
}

/* ---- fx (presentation, deterministic: their own RNG) ------------------------------------------------------------ */
static void fx_add(world *w, int kind, int x, int y, int vx, int vy, int life)
{
    for (int i = 0; i < MAX_FX; i++) {
        fx *f = &w->fx[i];
        if (f->life) continue;
        f->kind = (uint8_t)kind;
        f->x = x << 8;
        f->y = y << 8;
        f->vx = vx;
        f->vy = vy;
        f->t = 0;
        f->life = (uint8_t)life;
        return;
    }
}

static void fx_burst(world *w, int x, int y, int n, int speed)
{
    for (int i = 0; i < n; i++) {
        int a = rnd(&w->fxrng, 360);
        int v = speed / 2 + rnd(&w->fxrng, speed / 2 + 1);
        fx_add(w, FX_CHUNK, x, y, (int)(((int64_t)icos(a) * v) >> 16), -iabs((int)(((int64_t)isin(a) * v) >> 16)) - 80,
               26 + rnd(&w->fxrng, 14));
    }
}

static void fx_step(world *w)
{
    for (int i = 0; i < MAX_FX; i++) {
        fx *f = &w->fx[i];
        if (!f->life) continue;
        f->x += f->vx;
        f->y += f->vy;
        if (f->kind == FX_CHUNK) f->vy += 22;
        if (f->kind == FX_SPARK) f->vy -= 2;
        if (++f->t >= f->life) f->life = 0;
    }
}

/* ---- throws --------------------------------------------------------------------------------------------------------- */
int32_t wind_accel(int wind, int kind) { return (int32_t)((int64_t)wind * WIND_ACC * pk_info(kind)[3] / 100); }

void proj_launch(const world *w, const critter *c, int kind, int facing, int angle, int power, proj *p)
{
    (void)w;
    memset(p, 0, sizeof *p);
    int32_t v = (int32_t)((int64_t)V_MAX * clampi(power, 0, POWER_MAX) / POWER_MAX);
    p->on = 1;
    p->kind = (int8_t)kind;
    p->x = c->x + facing * (4 << 16);
    p->y = c->y - (11 << 16);
    p->x0 = c->x;
    p->vx = (int32_t)(((int64_t)v * icos(angle) >> 16) * facing);
    p->vy = (int32_t)(-((int64_t)v * isin(angle) >> 16));
    p->side = c->side;
    p->crit = (int8_t)(c - w->c);
}

static void bounce(const world *w, proj *p, int px, int py)
{
    int sx = 0, sy = 0;
    for (int dy = -3; dy <= 3; dy++)
        for (int dx = -3; dx <= 3; dx++)
            if (terrain_solid(w, px + dx, py + dy)) { sx += dx; sy += dy; }
    int64_t nx = -sx, ny = -sy;
    if (!nx && !ny) ny = -1;
    int32_t len = isqrt64(nx * nx + ny * ny);
    nx = nx * 65536 / len;
    ny = ny * 65536 / len;
    int64_t dot = ((int64_t)p->vx * nx + (int64_t)p->vy * ny) >> 16;
    if (dot < 0) {
        p->vx -= (int32_t)((2 * dot * nx) >> 16);
        p->vy -= (int32_t)((2 * dot * ny) >> 16);
    }
    p->vx = (int32_t)((int64_t)p->vx * BOUNCE_KEEP / 256);
    p->vy = (int32_t)((int64_t)p->vy * BOUNCE_KEEP / 256);
}

/* One frame of a throw's flight: PE_NONE (still flying), or how it ended (p->x, p->y: where) */
int proj_frame(const world *w, proj *p, int32_t wind_acc, int *crit, int *bounced)
{
    const int8_t *K = pk_info(p->kind);
    *crit = -1;
    *bounced = 0;
    p->t++;
    p->vx += wind_acc;
    p->vy += G_PROJ;
    int n = (imax(iabs(p->vx), iabs(p->vy)) >> 16) + 1;
    int32_t sx = p->vx / n, sy = p->vy / n;
    int rr = (CR_R + K[0]) * (CR_R + K[0]);
    for (int i = 0; i < n; i++) {
        int32_t ox = p->x, oy = p->y;
        p->x += sx;
        p->y += sy;
        int px = p->x >> 16, py = p->y >> 16;
        if (px < -32 || px >= WORLD_W + 32 || py >= WORLD_H) return PE_GONE;
        for (int k = 0; k < w->ncrit; k++) {
            const critter *c = &w->c[k];
            if (crit_out(c) || (k == p->crit && p->t <= PROJ_SAFE)) continue;
            int dx = px - (c->x >> 16), dy = py - ((c->y >> 16) - CR_H / 2);
            if (dx * dx + dy * dy <= rr) { *crit = k; return PE_CRITTER; }
        }
        if (terrain_solid(w, px, py)) {
            if (p->bounces < K[4]) {
                p->x = ox;
                p->y = oy;
                bounce(w, p, px, py);
                p->bounces++;
                *bounced = 1;
                if (iabs(p->vx) + iabs(p->vy) < BOUNCE_VMIN) return PE_TERRAIN;
                return PE_NONE;
            }
            return PE_TERRAIN;
        }
        if (py >= POND_Y && in_pond(w, px)) return PE_WATER;
    }
    if (p->t > PROJ_MAX_T) return PE_GONE;
    return PE_NONE;
}

void proj_sim(const world *w, const critter *c, int kind, int facing, int angle, int power, int32_t wind_acc,
              impact *out)
{
    proj p;
    proj_launch(w, c, kind, facing, angle, power, &p);
    int e = PE_NONE, crit = -1, b;
    while (e == PE_NONE) e = proj_frame(w, &p, wind_acc, &crit, &b);
    out->end = e;
    out->x = p.x >> 16;
    out->y = p.y >> 16;
    out->crit = crit;
    out->t = p.t;
}

int splat_damage(int kind, int dist)
{
    const int8_t *K = pk_info(kind);
    int r = K[1], d = imax(0, dist - CR_R);
    if (d >= r) return 0;
    return (K[2] * (r - d) + r - 1) / r;
}

static void add_score(world *w, int s, int pts)
{
    if (s >= 0 && s < w->nsides) w->s[s].score += pts;
}

static void show(world *w, int msg)
{
    w->msg = msg;
    w->msg_t = 90;
}

void splat(world *w, int kind, int x, int y)
{
    const int8_t *K = pk_info(kind);
    int thrower = w->p.side, enemies = 0, longshot = 0;
    terrain_carve(w, x, y, K[1]);
    for (int i = 0; i < w->ntrees; i++) {
        tree *t = &w->trees[i];
        if (t->on && iabs(t->x - x) <= K[1] + 3 && y + K[1] >= t->y - TREE_H && y - K[1] <= t->y) {
            t->on = 0;
            fx_add(w, FX_TREE, t->x, t->y - 12, 0, 0, 30);
            fx_burst(w, t->x, t->y - 16, 4, 320);
            w->events |= EV_TREE;
        }
    }
    for (int k = 0; k < w->ncrit; k++) {
        critter *c = &w->c[k];
        if (crit_out(c)) continue;
        int cx = c->x >> 16, cy = (c->y >> 16) - CR_H / 2, dx = cx - x, dy = cy - y;
        int d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
        int dmg = splat_damage(kind, d);
        if (!dmg) continue;
        c->hp = (int8_t)imax(0, c->hp - dmg);
        c->hurt_t = 0;
        c->last_hit = (int8_t)thrower;
        int32_t sp = KNOCK_BASE + KNOCK_PER * dmg;
        if (d > 0) {
            c->vx = (int32_t)((int64_t)sp * dx / d);
            c->vy = (int32_t)((int64_t)sp * dy / d) - KNOCK_UP;
        } else {
            c->vx = 0;
            c->vy = -sp - KNOCK_UP;
        }
        c->state = CS_AIR;
        c->apex = (int16_t)(c->y >> 16);
        w->events |= EV_HIT;
        w->hits++;
        if (c->side != thrower) {
            add_score(w, thrower, PTS_DAMAGE * dmg);
            w->s[thrower].damage += (int16_t)dmg;
            enemies++;
            if (iabs((w->p.x0 >> 16) - cx) >= LONG_DIST) longshot = 1;
        }
    }
    w->enemy_hits += enemies;
    if (enemies >= 2) { add_score(w, thrower, PTS_DOUBLE); show(w, MSG_DOUBLE); }
    else if (enemies && longshot) { add_score(w, thrower, PTS_LONG); show(w, MSG_LONG); }
    fx_add(w, FX_PUFF, x, y, 0, 0, 18);
    fx_burst(w, x, y, kind == PK_BIG ? 9 : 6, kind == PK_BIG ? 520 : 400);
    w->events |= EV_SPLAT;
    w->last_x = x;
    w->ev_x = x;
}

/* ---- critters ------------------------------------------------------------------------------------------------------ */
static void critter_out_splash(world *w, critter *c)
{
    c->state = CS_SPLASHED;
    c->out_t = 0;
    c->hp = 0;
    fx_add(w, FX_SPLASH, c->x >> 16, POND_Y, 0, 0, 30);
    w->events |= EV_SPLASH;
    w->ev_x = c->x >> 16;
    show(w, MSG_SPLASH);
    if (c->last_hit >= 0 && c->last_hit != c->side) add_score(w, c->last_hit, PTS_SPLASH);
}

static void land(world *w, critter *c)
{
    c->state = CS_STAND;
    c->vx = c->vy = 0;
    int fall = (c->y >> 16) - c->apex;
    if (fall > FALL_SAFE && c->hp > 0) {
        int dmg = 1 + (fall - FALL_SAFE) / FALL_PER;
        c->hp = (int8_t)imax(0, c->hp - dmg);
        c->hurt_t = 0;
        w->events |= EV_HIT;
        if (c->last_hit >= 0 && c->last_hit != c->side) {
            add_score(w, c->last_hit, PTS_DAMAGE * dmg);
            w->s[c->last_hit].damage += (int16_t)dmg;
        }
    }
    c->apex = (int16_t)(c->y >> 16);
    w->events |= EV_LAND;
}

static void critter_phys(world *w, critter *c)
{
    if (crit_out(c)) {
        if (c->out_t < 30000) c->out_t++;
        if (c->state == CS_SNOWED) {            /* a snow mound settles where the ground went (or into the pond) */
            int mx = c->x >> 16, my = c->y >> 16;
            if (!supported(w, mx, my) && my < WORLD_H - 1) c->y += Q16_ONE;
            if ((c->y >> 16) >= POND_Y && in_pond(w, mx) && !supported(w, mx, c->y >> 16)) c->state = CS_SPLASHED;
        }
        return;
    }
    if (c->hurt_t < 30000) c->hurt_t++;
    if (c->anim_t < 30000) c->anim_t++;
    int ix = c->x >> 16, iy = c->y >> 16;
    if (c->state == CS_STAND) {
        if (supported(w, ix, iy)) return;
        c->state = CS_AIR;
        c->vx = c->vy = 0;
        c->apex = (int16_t)iy;
    }
    c->vy += G_CRIT;
    if (c->vy > FALL_VMAX) c->vy = FALL_VMAX;
    int n = (imax(iabs(c->vx), iabs(c->vy)) >> 16) + 1;
    for (int i = 0; i < n; i++) {
        if (c->vx) {
            int32_t nx = c->x + c->vx / n;
            int jx = nx >> 16, dir = c->vx > 0 ? 1 : -1;
            iy = c->y >> 16;
            if (jx < X_MIN || jx > X_MAX) {
                c->x = (int32_t)clampi(jx, X_MIN, X_MAX) << 16;
                c->vx = 0;
            } else if (jx != (c->x >> 16) && !front_free(w, jx, iy, dir)) {
                int k = 1;
                while (k <= CLIMB && !front_free(w, jx, iy - k, dir)) k++;
                if (k <= CLIMB) {
                    c->x = nx;
                    c->y -= k << 16;
                } else {
                    c->vx = -c->vx / 4;
                }
            } else {
                c->x = nx;
            }
        }
        ix = c->x >> 16;
        int32_t ny = c->y + c->vy / n;
        if (c->vy > 0) {
            int from = c->y >> 16, to = ny >> 16, landed = supported(w, ix, from);
            for (int yy = from + 1; yy <= to && !landed; yy++)
                if (supported(w, ix, yy)) {
                    c->y = (int32_t)yy << 16;
                    landed = 1;
                    break;
                }
            if (!landed) c->y = ny;
            if ((c->y >> 16) >= POND_Y && in_pond(w, ix) && !supported(w, ix, c->y >> 16)) {
                critter_out_splash(w, c);
                return;
            }
            if (landed) { land(w, c); return; }
            if ((c->y >> 16) >= WORLD_H) { critter_out_splash(w, c); return; }
        } else {
            if (terrain_solid(w, ix, (ny >> 16) - CR_H)) c->vy = 0;
            else c->y = ny;
            if ((c->y >> 16) < c->apex) c->apex = (int16_t)(c->y >> 16);
        }
    }
}

/* one pixel of walking: up or down a step of CLIMB, else blocked (0) */
static int critter_walk1(world *w, critter *c, int dir)
{
    int ix = c->x >> 16, iy = c->y >> 16, nx = ix + dir, ny = iy;
    if (nx < X_MIN || nx > X_MAX) return 0;
    if (!front_free(w, nx, ny, dir)) {
        int k = 1;
        while (k <= CLIMB && !(front_free(w, nx, ny - k, dir) && supported(w, nx, ny - k))) k++;
        if (k > CLIMB) return 0;
        ny -= k;
    } else if (!supported(w, nx, ny)) {
        for (int k = 1; k <= CLIMB; k++)
            if (supported(w, nx, ny + k)) { ny += k; break; }
    }
    c->x = (int32_t)nx << 16;
    c->y = (int32_t)ny << 16;
    return 1;
}

static void all_phys(world *w)
{
    for (int i = 0; i < w->ncrit; i++) critter_phys(w, &w->c[i]);
}

static int all_settled(const world *w)
{
    for (int i = 0; i < w->ncrit; i++)
        if (!crit_out(&w->c[i]) && w->c[i].state == CS_AIR) return 0;
    return 1;
}

/* ---- turns ------------------------------------------------------------------------------------------------------- */
static void end_match(world *w, int winner)
{
    w->phase = PH_END;
    w->t = 0;
    w->winner = winner;
    w->p.on = 0;
    if (w->players == 1) {
        if (winner == 0) {
            w->wins++;
            add_score(w, 0, PTS_WIN + PTS_WIN_LEVEL * (w->level - 1) + PTS_HP_LEFT * side_hearts(w, 0));
            w->events |= EV_WON;
            show(w, MSG_WON);
        } else {
            w->events |= EV_LOST;
            if (winner < 0) show(w, MSG_DRAW);
        }
    } else {
        w->events |= EV_WON;
        show(w, winner < 0 ? MSG_DRAW : MSG_WON);
    }
}

static void next_match(world *w)
{
    w->level++;
    w->s[1].species = (int8_t)cpu_species(w->level);
    w->s[1].cpu = (int8_t)imin(w->level, 127);  /* an int8_t: no wrap to a human side at match 128 */
    gen_field(w);
    w->winner = -1;
    w->turns = 0;
    w->frost = 0;
    w->events |= EV_MATCH;
}

static void start_turn(world *w, int s)
{
    side *sd = &w->s[s];
    int pick = -1;
    for (int k = 0; k < TEAM && pick < 0; k++) {
        int local = (sd->next + k) % TEAM, n = 0;
        for (int i = 0; i < w->ncrit; i++) {
            if (w->c[i].side != s) continue;
            if (n++ == local && !crit_out(&w->c[i]) && w->c[i].hp > 0) { pick = i; sd->next = (int8_t)((local + 1) % TEAM); }
        }
    }
    if (pick < 0) pick = w->cur;
    w->cur_side = s;
    w->cur = pick;
    w->phase = PH_AIM;
    w->t = 0;
    w->timer = TURN_FRAMES;
    w->angle = AIM_START;
    w->power = 0;
    w->charging = 0;
    w->weapon = PK_SNOW;
    w->thrown = 0;
    w->moved = 0;
    w->a_prev = 1;                              /* A must be let go first (hot seat: the last player may hold it) */
    w->aim_t = w->walk_t = w->walk_dir = 0;
    w->walk_acc = 0;
    w->shuffle = 0;
    w->sling_on = 0;
    memset(&w->cpu, 0, sizeof w->cpu);
    w->wind = w->turns == 0 ? rnd_pm(&w->rng, 5) : clampi(w->wind + rnd_pm(&w->rng, WIND_STEP), -WIND_MAX, WIND_MAX);
    w->turns++;
    w->banner_t = BANNER_T;
    w->events |= EV_TURN;
    w->ev_x = w->c[pick].x >> 16;
}

void world_start(world *w)
{
    w->started = 1;
    w->turns = 0;
    w->frost = 0;
    w->winner = -1;
    w->over = 0;
    w->events |= EV_MATCH;
    start_turn(w, w->players == 1 ? (w->level - 1) % 2 : 0);
}

/* the end of a turn (everything landed): the outs, the end of the match, or the next side */
static void resolve(world *w)
{
    for (int i = 0; i < w->ncrit; i++) {
        critter *c = &w->c[i];
        if (crit_out(c) || c->hp > 0) continue;
        c->state = CS_SNOWED;
        c->out_t = 0;
        w->events |= EV_SNOWED;
        w->ev_x = c->x >> 16;
        if (w->msg != MSG_SPLASH || w->msg_t < 40) show(w, MSG_SNOWED);
        if (c->last_hit >= 0 && c->last_hit != c->side) add_score(w, c->last_hit, PTS_SNOWED);
    }
    int alive = 0, last = -1;
    for (int s = 0; s < w->nsides; s++) {
        if (side_alive(w, s)) { alive++; last = s; }
        else if (!w->s[s].out_turn_set) { w->s[s].out_turn_set = 1; w->s[s].out_turn = (int16_t)w->turns; }
    }
    if (alive <= 1) { end_match(w, alive ? last : -1); return; }
    if (w->turns >= MAX_TURNS) {
        int best = -1, bh = -1, tie = 0;
        for (int s = 0; s < w->nsides; s++) {
            int hh = side_alive(w, s) ? side_hearts(w, s) : -1;
            if (hh > bh) { bh = hh; best = s; tie = 0; }
            else if (hh == bh) tie = 1;
        }
        end_match(w, tie ? -1 : best);
        return;
    }
    if (w->turns >= FROST_TURN) {               /* the cold bites: half a heart each, never the last one */
        if (!w->frost) show(w, MSG_FROST);
        w->frost = 1;
        for (int i = 0; i < w->ncrit; i++)
            if (!crit_out(&w->c[i]) && w->c[i].hp > 1) w->c[i].hp--;
    }
    int s = w->cur_side;
    for (int k = 1; k <= w->nsides; k++) {
        int n = (w->cur_side + k) % w->nsides;
        if (side_alive(w, n)) { s = n; break; }
    }
    start_turn(w, s);
}

static void throw_now(world *w, critter *c)
{
    int kind = w->weapon;
    side *sd = &w->s[w->cur_side];
    if (sd->stock[kind] == 0) kind = PK_SNOW;
    if (sd->stock[kind] > 0) sd->stock[kind]--;
    proj_launch(w, c, kind, c->facing, w->angle, w->power, &w->p);
    w->thrown = 1;
    w->charging = 0;
    w->sling_on = 0;
    w->phase = PH_FLIGHT;
    w->t = 0;
    w->hits = w->enemy_hits = 0;
    c->anim_t = 0;
    w->events |= EV_THROW;
    w->ev_x = c->x >> 16;
}

static void next_weapon(world *w)
{
    const side *sd = &w->s[w->cur_side];
    for (int k = 1; k <= PK_COUNT; k++) {
        int n = (w->weapon + k) % PK_COUNT;
        if (sd->stock[n] != 0) { w->weapon = n; return; }
    }
}

static void walk_px(world *w, critter *c, int dir)
{
    if (w->moved >= MOVE_BUDGET || c->state != CS_STAND) return;
    w->walk_acc += WALK_SPEED;
    while (w->walk_acc >= Q16_ONE && w->moved < MOVE_BUDGET) {
        w->walk_acc -= Q16_ONE;
        if (!critter_walk1(w, c, dir)) { w->walk_acc = 0; break; }
        w->moved++;
        c->anim_t = 0;
        if (w->moved % 6 == 0) w->events |= EV_STEP;
    }
}

static void aim_input(world *w, critter *c, const tin *in)
{
    if (w->thrown || c->state != CS_STAND) return;
    if (in->fire) {                             /* touch: the slingshot let go */
        c->facing = in->sl_facing ? in->sl_facing : c->facing;
        w->angle = clampi(in->sl_angle, AIM_MIN, AIM_MAX);
        w->power = clampi(in->sl_power, POWER_START, POWER_MAX);
        throw_now(w, c);
        return;
    }
    if (in->sling) {                            /* touch: being pulled: the aim follows */
        if (in->sl_facing) c->facing = in->sl_facing;
        w->angle = clampi(in->sl_angle, AIM_MIN, AIM_MAX);
        w->power = clampi(in->sl_power, POWER_START, POWER_MAX);
        w->sling_on = 1;
        w->charging = 0;
        return;
    }
    if (w->sling_on) { w->sling_on = 0; w->power = 0; }
    if (w->charging) {
        if (in->a_down) {
            w->power += CHARGE_STEP;
            if (w->power >= POWER_MAX) { w->power = POWER_MAX; throw_now(w, c); }
        } else {
            throw_now(w, c);
        }
        w->a_prev = in->a_down;
        return;
    }
    if (in->a_down && !w->a_prev) {
        w->charging = 1;
        w->power = POWER_START;
        w->a_prev = 1;
        return;
    }
    w->a_prev = in->a_down;
    if (in->b_press) {
        int was = w->weapon;
        next_weapon(w);
        if (w->weapon != was) w->events |= EV_SWITCH;
    }
    if (in->aim) {
        if (w->aim_t % AIM_RATE == 0) {
            int a = clampi(w->angle + in->aim, AIM_MIN, AIM_MAX);
            if (a != w->angle) { w->angle = a; w->events |= EV_AIM; }
        }
        w->aim_t++;
    } else {
        w->aim_t = 0;
    }
    if (in->shuffle && !w->shuffle) {
        c->facing = in->shuffle;
        w->shuffle = in->shuffle * SHUFFLE_PX;
        w->events |= EV_AIM;
    }
    if (in->walk) {
        if (in->walk != w->walk_dir) {
            w->walk_dir = in->walk;
            w->walk_t = c->facing == in->walk ? 0 : -WALK_TURN;
            c->facing = in->walk;
            w->events |= EV_AIM;
        }
        if (++w->walk_t > 0) walk_px(w, c, in->walk);
    } else {
        w->walk_dir = 0;
        w->walk_t = 0;
        if (w->shuffle) {
            int d = w->shuffle > 0 ? 1 : -1, before = w->moved;
            walk_px(w, c, d);
            if (w->moved == before && w->walk_acc == 0) w->shuffle = 0;
            else w->shuffle -= d * (w->moved - before);
            if (w->moved >= MOVE_BUDGET) w->shuffle = 0;
        } else {
            w->walk_acc = 0;
        }
    }
}

void world_step(world *w, const tin *in)
{
    static const tin none;
    w->events = 0;
    w->t_all++;
    fx_step(w);
    if (w->msg_t > 0 && --w->msg_t == 0) w->msg = MSG_NONE;
    if (w->banner_t > 0) w->banner_t--;
    if (!in) in = &none;
    switch (w->phase) {
    case PH_DEMO:
        all_phys(w);
        break;
    case PH_AIM: {
        critter *c = world_active(w);
        if (crit_out(c) || c->hp <= 0) { w->phase = PH_SETTLE; w->t = 0; break; }   /* out, or snowed by a fall */
        tin cin;
        if (w->s[w->cur_side].cpu) {
            cpu_think(w, w->s[w->cur_side].cpu, &cin);
            in = &cin;
        }
        aim_input(w, c, in);
        all_phys(w);
        if (w->phase != PH_AIM) break;
        w->timer--;
        if (w->timer > 0 && w->timer <= TICK_FROM && w->timer % 60 == 0) w->events |= EV_TICK;
        if (w->timer <= 0) {
            if (w->charging || w->sling_on) {
                if (w->power < POWER_START) w->power = POWER_START;
                throw_now(w, c);
            } else {
                w->phase = PH_TIMEUP;
                w->t = 0;
                w->events |= EV_TIMEUP;
                show(w, MSG_TIMEUP);
            }
        }
        break;
    }
    case PH_FLIGHT: {
        int crit, bounced;
        int e = proj_frame(w, &w->p, wind_accel(w->wind, w->p.kind), &crit, &bounced);
        if (bounced) { w->events |= EV_BOUNCE; w->ev_x = w->p.x >> 16; }
        if (e != PE_NONE) {
            int x = w->p.x >> 16, y = w->p.y >> 16;
            if (e == PE_CRITTER) {             /* a critter hit: the splat at its feet (it carves a crater under it) */
                const critter *hc = &w->c[crit];
                x = hc->x >> 16;
                y = (hc->y >> 16) - 2;
            }
            if (e == PE_TERRAIN || e == PE_CRITTER) splat(w, w->p.kind, x, y);
            else if (e == PE_WATER) {
                fx_add(w, FX_SPLASH, x, POND_Y, 0, 0, 24);
                w->events |= EV_PLOP;
                w->ev_x = x;
                w->last_x = x;
            } else {
                w->last_x = clampi(x, 0, WORLD_W - 1);
            }
            w->p.on = 0;
            w->phase = PH_SETTLE;
            w->t = 0;
        }
        all_phys(w);
        break;
    }
    case PH_TIMEUP:
        all_phys(w);
        if (w->t >= TIMEUP_WAIT) { w->phase = PH_SETTLE; w->t = 0; }
        break;
    case PH_SETTLE:
        all_phys(w);
        if ((w->t >= SETTLE_MIN && all_settled(w)) || w->t >= SETTLE_MAX) resolve(w);
        break;
    case PH_END:
        all_phys(w);
        if (w->t >= END_WAIT && !w->over) {
            if (w->players == 1 && w->winner == 0) {
                next_match(w);
                start_turn(w, (w->level - 1) % 2);
                return;
            }
            w->over = 1;
        }
        break;
    }
    w->t++;
}

/* ---- the computer --------------------------------------------------------------------------------------------- */
#define ANG_LO (-10)
#define ANG_HI 86
#define NA ((ANG_HI - ANG_LO) / CPU_ANG_STEP + 1)
#define NJ ((POWER_MAX - POWER_START) / CHARGE_STEP / CPU_POW_STEP + 1)

int cpu_shot_value(const world *w, int sd, int kind, const impact *im)
{
    if (im->end == PE_GONE) return -1000000;
    int val = 0, mind = 100000;
    for (int k = 0; k < w->ncrit; k++) {
        const critter *c = &w->c[k];
        if (crit_out(c) || c->hp <= 0) continue;
        int dx = (c->x >> 16) - im->x, dy = (c->y >> 16) - CR_H / 2 - im->y;
        int d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
        int dmg = im->end == PE_WATER ? 0 : splat_damage(kind, d);
        if (c->side != sd) {
            mind = imin(mind, d);
            val += dmg * 1000 + (dmg && dmg >= c->hp ? 2500 : 0);
        } else {
            val -= dmg * 1600 + (dmg && dmg >= c->hp ? 4000 : 0);
        }
    }
    val -= mind * 4;
    if (im->end == PE_WATER) val -= 200;
    return val;
}

static int power_of_j(int j) { return imin(POWER_MAX, POWER_START + j * CHARGE_STEP); }

static void cpu_try(world *w, const critter *c, cpu_t *k, int face, int ang, int j)
{
    if (ang < AIM_MIN || ang > AIM_MAX || j < 0) return;
    impact im;
    proj_sim(w, c, k->weapon, face, ang, power_of_j(j), k->wind_seen, &im);
    int v = cpu_shot_value(w, c->side, k->weapon, &im);
    if (v > k->best_val) {
        k->best_val = v;
        k->best_ang = (int16_t)ang;
        k->best_j = (int16_t)j;
        k->best_face = (int16_t)face;
    }
}

void cpu_think(world *w, int level, tin *out)
{
    static const int8_t ang_err[CPU_LEVELS] = CPU_ANG_ERR, wind_noise[CPU_LEVELS] = CPU_WIND_NOISE;
    static const int16_t pow_err[CPU_LEVELS] = CPU_POW_ERR, think[CPU_LEVELS] = CPU_THINK;
    static const int8_t wind_read[CPU_LEVELS] = CPU_WIND_READ;
    memset(out, 0, sizeof *out);
    if (w->phase != PH_AIM || w->thrown) return;
    cpu_t *k = &w->cpu;
    const critter *c = world_active_c(w);
    const side *sd = &w->s[w->cur_side];
    int li = level <= 0 ? -1 : imin(level, CPU_LEVELS) - 1;
    switch (k->stage) {
    case 0: {
        int left = 0, right = 0, strong = 0, huddle = 0;
        for (int i = 0; i < w->ncrit; i++) {
            const critter *e = &w->c[i];
            if (e->side == c->side || crit_out(e) || e->hp <= 0) continue;
            if (e->x < c->x) left = 1;
            else right = 1;
            if (e->hp >= 4) strong = 1;
            for (int j = 0; j < w->ncrit; j++)
                if (j != i && w->c[j].side == e->side && !crit_out(&w->c[j]) && iabs((w->c[j].x - e->x) >> 16) < 24)
                    huddle = 1;
        }
        k->nfacing = 0;
        if (right) k->fdir[k->nfacing++] = 1;
        if (left) k->fdir[k->nfacing++] = -1;
        if (!k->nfacing) k->fdir[k->nfacing++] = c->facing;
        int r = rnd(&w->rng, 100);
        k->weapon = PK_SNOW;
        if (sd->stock[PK_BIG] != 0 && (strong || huddle) && r < CPU_BIG_PCT) k->weapon = PK_BIG;
        else if (sd->stock[PK_ICE] != 0 && r >= 100 - CPU_ICE_PCT) k->weapon = PK_ICE;
        int read = li < 0 ? 100 : wind_read[li], noise = li < 0 ? 0 : rnd_pm(&w->rng, wind_noise[li]);
        int64_t acc = (int64_t)w->wind * WIND_ACC * read / 100 + (int64_t)noise * WIND_ACC;
        k->wind_seen = (int32_t)(acc * pk_info(k->weapon)[3] / 100);
        k->ai = 0;
        k->best_val = INT32_MIN;
        k->best_ang = AIM_START;
        k->best_j = (int16_t)(NJ / 2 * CPU_POW_STEP);
        k->best_face = c->facing;
        k->wait = (int16_t)(li < 0 ? 8 : think[li]);
        k->stage = 1;
        return;
    }
    case 1: {
        int total = k->nfacing * NA * NJ;
        for (int n = 0; n < CPU_SIMS && k->ai < total; n++, k->ai++) {
            int f = k->ai / (NA * NJ), a = (k->ai / NJ) % NA, j = k->ai % NJ;
            cpu_try(w, c, k, k->fdir[f], ANG_LO + a * CPU_ANG_STEP, j * CPU_POW_STEP);
        }
        if (k->ai >= total) k->stage = 2;
        if (k->wait > 0) k->wait--;
        return;
    }
    case 2: {
        int ba = k->best_ang, bj = k->best_j;
        for (int da = -1; da <= 1; da++)
            for (int dj = -1; dj <= 1; dj++)
                if (da || dj) cpu_try(w, c, k, k->best_face, ba + da, bj + dj);
        int ea = li < 0 ? 0 : ang_err[li], ep = li < 0 ? 0 : pow_err[li];
        k->tgt_ang = (int16_t)clampi(k->best_ang + rnd_pm(&w->rng, ea), AIM_MIN, AIM_MAX);
        k->tgt_power = (int16_t)clampi(power_of_j(k->best_j) + rnd_pm(&w->rng, ep), POWER_START, POWER_MAX);
        k->facing = (int8_t)k->best_face;
        k->stage = 3;
        k->act_t = 0;
        return;
    }
    case 3:
        k->act_t++;
        if (w->weapon != k->weapon) {
            if (k->act_t > 40) k->weapon = (int8_t)w->weapon;      /* none left: what it holds */
            else if (k->act_t % 8 == 0) out->b_press = 1;
            return;
        }
        if (c->facing != k->facing) { out->walk = k->facing; return; }
        if (k->wait > 0) { k->wait--; return; }
        if (w->angle != k->tgt_ang) { out->aim = w->angle < k->tgt_ang ? 1 : -1; return; }
        k->stage = 4;
        out->a_down = 1;
        return;
    case 4:
        out->a_down = w->power < k->tgt_power;
        return;
    }
}

/* ---- touch: the slingshot -------------------------------------------------------------------------------------- */
void sling_aim(int dx, int dy, int *facing, int *angle, int *power)
{
    *facing = dx >= 0 ? 1 : -1;
    *angle = clampi(atan_deg(-dy, iabs(dx)), AIM_MIN, AIM_MAX);
    int len = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
    *power = clampi(POWER_START + (len - SLING_MIN) * (POWER_MAX - POWER_START) / (SLING_MAX - SLING_MIN), POWER_START,
                    POWER_MAX);
}

/* ---- the camera ------------------------------------------------------------------------------------------------ */
static int cam_clamp(int x, int sw)
{
    int lo = sw / 2, hi = WORLD_W - (sw - sw / 2);
    if (hi < lo) return WORLD_W / 2;
    return clampi(x, lo, hi);
}

static int cam_target(const world *w, int sw, int t)
{
    if (w->phase == PH_DEMO) return WORLD_W / 2 + (int)(((int64_t)isin(t / 3) * (WORLD_W / 2 - sw / 2 + 8)) >> 16);
    if (w->p.on) {
        /* a throw: between the ball and where it is heading (its flight, up to CAM_AHEAD frames on), the ball kept
         * about CAM_EDGE px inside the screen */
        proj q = w->p;
        int crit, b, ball = w->p.x >> 16, lim = sw / 2 - CAM_EDGE;
        int lead = (w->p.x + w->p.vx * CAM_LERP) >> 16;    /* the camera trails its target by about CAM_LERP frames */
        int32_t acc = wind_accel(w->wind, q.kind);
        for (int i = 0; i < CAM_AHEAD && proj_frame(w, &q, acc, &crit, &b) == PE_NONE; i++) {}
        int mid = (ball + clampi(q.x >> 16, 0, WORLD_W)) / 2;
        return clampi(mid, lead - lim, lead + lim);
    }
    if (w->phase == PH_SETTLE || w->phase == PH_END) {
        for (int i = 0; i < w->ncrit; i++)
            if (!crit_out(&w->c[i]) && w->c[i].state == CS_AIR) return w->c[i].x >> 16;
        return w->last_x;
    }
    return w->c[w->cur].x >> 16;
}

void camera_init(camera *c, const world *w, int screen_w)
{
    memset(c, 0, sizeof *c);
    c->x = cam_clamp(cam_target(w, screen_w, 0), screen_w);
}

void camera_step(camera *c, const world *w, int sw, int pan_dx, int t)
{
    if (w->phase != PH_AIM || (w->events & (EV_AIM | EV_STEP | EV_THROW | EV_TURN))) c->manual = 0;
    if (pan_dx && w->phase == PH_AIM) c->manual = 1;
    if (c->manual) {
        c->x = cam_clamp(c->x + pan_dx, sw);
        return;
    }
    int dx = cam_target(w, sw, t) - c->x, st = dx / CAM_LERP;
    if (!st && dx) st = dx > 0 ? 1 : -1;
    c->x = cam_clamp(c->x + clampi(st, -CAM_VMAX, CAM_VMAX), sw);
}

int camera_left(const camera *c, int screen_w) { return c->x - screen_w / 2; }
