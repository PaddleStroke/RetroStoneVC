/*
 * Seal Shove: the rules (the floe, the seals' slippery physics, the bumps and slides, the cracks, the fish, rounds and
 * matches) and the bots' brains. No drawing, no sound, no screen width: pure fixed point with the world's own RNG, so
 * every run is deterministic and tests/test_rules.c checks it alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
 */
#include "game.h"
#include <string.h>

const char *const bot_names[SEALS] = {"FLUFF", "MOCHI", "PUDDLE", "BISCUIT"};   /* never translated */

/* ---- integer maths ------------------------------------------------------------------------------------------------- */
static const int16_t SINQ[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931,
    2019, 2106, 2191, 2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290, 3349,
    3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052,
    4065, 4076, 4085, 4091, 4095, 4096};

int isin(int a)
{
    a &= 255;
    if (a < 64) return SINQ[a];
    if (a < 128) return SINQ[128 - a];
    if (a < 192) return -SINQ[a - 128];
    return -SINQ[256 - a];
}

int icos(int a) { return isin(a + 64); }

int isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    uint64_t x = (uint64_t)v, r = 0, b = (uint64_t)1 << 62;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return (int)r;
}

/* atan of z (Q12, 0..1) in 1/256 of a turn: z * 32 + 11.1 z (1 - z) (within 0.3 degrees) */
static int atan_oct(int64_t z) { return (int)((z * 32 + z * (FX - z) * 111 / 10 / FX + FX / 2) / FX); }

int iatan2(int dy, int dx)
{
    if (!dx && !dy) return 0;
    int64_t ax = dx < 0 ? -(int64_t)dx : dx, ay = dy < 0 ? -(int64_t)dy : dy;
    int t = ax >= ay ? atan_oct(ay * FX / ax) : 64 - atan_oct(ax * FX / ay);
    if (dx < 0) t = 128 - t;
    if (dy < 0) t = 256 - t;
    return t & 255;
}

int dir8(int dx, int dy) { return ((iatan2(dy, dx) + 16) >> 5) & 7; }

static int rnd(world *w, int n) { return rs_rng_range(&w->rng, n); }
static int iabs(int v) { return v < 0 ? -v : v; }

/* ---- the floe -------------------------------------------------------------------------------------------------------- */
static int cell_ice(const world *w, int i, int j, int warn_water)
{
    if ((unsigned)i >= GRID || (unsigned)j >= GRID) return 0;
    int c = w->cell[j * GRID + i];
    if (!c) return 0;
    int st = w->ch[c - 1].state;
    return st == CH_ICE || (st == CH_WARN && !warn_water);
}

/* the ice field: the cells' ice (0 or 1 at their centres) blended between the centres (a smooth, rounded edge) */
int floe_field(const world *w, int xq, int yq, int warn_water)
{
    int u = xq - (GRID_X0 * 16 + CELL * 8), v = yq - (GRID_Y0 * 16 + CELL * 8);
    int i = u >> 6, j = v >> 6;                     /* CELL * 16 = 64 */
    int fu = (u & 63) * 4, fv = (v & 63) * 4;
    int a = cell_ice(w, i, j, warn_water), b = cell_ice(w, i + 1, j, warn_water);
    int c = cell_ice(w, i, j + 1, warn_water), d = cell_ice(w, i + 1, j + 1, warn_water);
    if (a == b && b == c && c == d) return a ? 256 : 0;
    int top = a * (256 - fu) + b * fu, bot = c * (256 - fu) + d * fu;
    return (top * (256 - fv) + bot * fv) >> 8;
}

int on_ice(const world *w, int32_t x, int32_t y) { return floe_field(w, x >> 8, y >> 8, 0) >= 128; }

int chunk_at(const world *w, int px, int py)
{
    int i = (px - GRID_X0) / CELL, j = (py - GRID_Y0) / CELL;
    if (px < GRID_X0 || py < GRID_Y0 || i >= GRID || j >= GRID) return -1;
    return w->cell[j * GRID + i] - 1;
}

static void safe_centre(world *w)
{
    int64_t sx = 0, sy = 0, n = 0;
    for (int j = 0; j < GRID; j++)
        for (int i = 0; i < GRID; i++) {
            int c = w->cell[j * GRID + i];
            if (c && w->ch[c - 1].state == CH_ICE) {
                sx += GRID_X0 + i * CELL + CELL / 2;
                sy += GRID_Y0 + j * CELL + CELL / 2;
                n++;
            }
        }
    w->safe_x = (int16_t)(n ? sx / n : FLOE_CX);
    w->safe_y = (int16_t)(n ? sy / n : FLOE_CY);
}

/* a new floe: a wobbly round outline, three rings of sectors (each ring turned and twisted its own way) round a core */
static void floe_new(world *w)
{
    static const int nsec[3] = RING_SECTORS;
    int p1 = rnd(w, 256), p2 = rnd(w, 256), p3 = rnd(w, 256), p4 = rnd(w, 256);
    int ofs[3], twist[3];
    for (int r = 0; r < 3; r++) {
        ofs[r] = rnd(w, 256);
        twist[r] = rnd(w, 5) - 2;
    }
    int base[3] = {0, nsec[0], nsec[0] + nsec[1]}, core = nsec[0] + nsec[1] + nsec[2];
    memset(w->cell, 0, sizeof w->cell);
    memset(w->ch, 0, sizeof w->ch);
    for (int j = 0; j < GRID; j++)
        for (int i = 0; i < GRID; i++) {
            int dx = GRID_X0 + i * CELL + CELL / 2 - FLOE_CX, dy = GRID_Y0 + j * CELL + CELL / 2 - FLOE_CY;
            int d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy), a = iatan2(dy, dx);
            int rim = FLOE_R + (isin(2 * a + p1) * 3 + isin(3 * a + p2) * 2) * FLOE_JITTER / (5 * FX);
            if (d >= rim) continue;
            int r1 = RING1_R + isin(3 * a + p3) * RING_JITTER / FX, r2 = RING2_R + isin(2 * a + p4) * RING_JITTER / FX;
            int ring = d >= r1 ? 0 : d >= r2 ? 1 : d >= CORE_R ? 2 : 3, id;
            if (ring == 3) {
                id = core;
            } else {
                int aa = (a - ofs[ring] + twist[ring] * d / 8) & 255;
                id = base[ring] + aa * nsec[ring] / 256;
            }
            w->cell[j * GRID + i] = (uint8_t)(id + 1);
            chunk *c = &w->ch[id];
            c->ring = (uint8_t)(ring > 2 ? 3 : ring);
            int x0 = GRID_X0 + i * CELL, y0 = GRID_Y0 + j * CELL;
            if (!c->ncells || x0 < c->x0) c->x0 = (int16_t)x0;
            if (!c->ncells || y0 < c->y0) c->y0 = (int16_t)y0;
            if (!c->ncells || x0 + CELL > c->x1) c->x1 = (int16_t)(x0 + CELL);
            if (!c->ncells || y0 + CELL > c->y1) c->y1 = (int16_t)(y0 + CELL);
            c->ncells++;
        }
    w->nchunks = core + 1;
    w->core = core;
    int64_t sx[MAX_CHUNKS] = {0}, sy[MAX_CHUNKS] = {0};
    for (int j = 0; j < GRID; j++)
        for (int i = 0; i < GRID; i++) {
            int c = w->cell[j * GRID + i] - 1;
            if (c < 0) continue;
            sx[c] += GRID_X0 + i * CELL + CELL / 2;
            sy[c] += GRID_Y0 + j * CELL + CELL / 2;
            static const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int k = 0; k < 4; k++) {
                int ii = i + nb[k][0], jj = j + nb[k][1];
                int o = ((unsigned)ii < GRID && (unsigned)jj < GRID) ? w->cell[jj * GRID + ii] - 1 : -1;
                if (o < 0) w->ch[c].edge = 1;
                else if (o != c) w->ch[c].adj |= 1u << o;
            }
        }
    for (int c = 0; c < w->nchunks; c++) {
        chunk *k = &w->ch[c];
        if (!k->ncells) {                           /* (never with these sizes) an empty chunk is gone already */
            k->state = CH_GONE;
            w->gone |= 1u << c;
            continue;
        }
        k->cx = (int16_t)(sx[c] / k->ncells);
        k->cy = (int16_t)(sy[c] / k->ncells);
    }
    w->gone = 0;
    for (int c = 0; c < w->nchunks; c++)
        if (w->ch[c].state == CH_GONE) w->gone |= 1u << c;
    w->floe_gen++;
    safe_centre(w);
}

static int pick_chunk(world *w)
{
    static const int wt[3] = RING_WEIGHTS;
    int cand[MAX_CHUNKS], cw[MAX_CHUNKS], n = 0, total = 0, others = 0;
    for (int c = 0; c < w->core; c++) {
        const chunk *k = &w->ch[c];
        if (k->state != CH_GONE) others++;
        if (k->state != CH_ICE || !(k->edge || (k->adj & w->gone))) continue;
        cand[n] = c;
        cw[n] = wt[k->ring < 3 ? k->ring : 2];
        total += cw[n++];
    }
    if (!n) {
        if (!others && w->ch[w->core].state == CH_ICE && w->t - w->last_break_t >= CORE_DELAY) return w->core;
        return -1;
    }
    int r = rnd(w, total);
    for (int i = 0; i < n; i++) {
        if (r < cw[i]) return cand[i];
        r -= cw[i];
    }
    return cand[n - 1];
}

static void floe_step(world *w)
{
    for (int c = 0; c < w->nchunks; c++) {
        chunk *k = &w->ch[c];
        if (k->t < 30000) k->t++;
        if (k->state == CH_WARN && k->t >= k->warn) {
            k->state = CH_GONE;
            k->t = 0;
            k->rev++;
            w->gone |= 1u << c;
            w->gev |= GEV_BREAK;
            w->breaks++;
            w->last_break_t = w->t;
            safe_centre(w);
        }
    }
    if (--w->crack_t > 0) return;
    int c = pick_chunk(w);
    if (c < 0) {
        w->crack_t = 10;
        return;
    }
    chunk *k = &w->ch[c];
    k->state = CH_WARN;
    k->t = 0;
    k->warn = (int16_t)(w->hurry ? HURRY_WARN : CRACK_WARN);
    k->rev++;
    w->gev |= GEV_CRACK;
    safe_centre(w);
    w->crack_t = w->hurry ? HURRY_GAP : w->crack_gap;
    if (w->crack_gap > CRACK_GAP_MIN) w->crack_gap -= CRACK_GAP_STEP;
}

/* ---- the seals ----------------------------------------------------------------------------------------------------------- */
int seal_standing(const seal *s) { return s->state == SS_PLAY || s->state == SS_DASH; }
int seal_radius(const seal *s) { return s->power == PW_HEAVY ? HEAVY_R : SEAL_R; }

int seal_mass(const seal *s)
{
    int m = s->power == PW_HEAVY ? SEAL_MASS * HEAVY_MASS_PCT / 100 : SEAL_MASS;
    if (s->state == SS_DASH) m = m * (s->mega ? MEGA_MASS_PCT : DASH_MASS_PCT) / 100;
    return m;
}

int seals_standing(const world *w)
{
    int n = 0;
    for (int i = 0; i < SEALS; i++) n += seal_standing(&w->s[i]);
    return n;
}

static void place_seals(world *w)
{
    static const int8_t at[SEALS][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};   /* P1 left, P2 right, top, bottom */
    for (int i = 0; i < SEALS; i++) {
        seal *s = &w->s[i];
        s->x = (FLOE_CX + at[i][0] * START_R) * FX;
        s->y = (FLOE_CY + at[i][1] * (START_R - 4)) * FX;
        s->vx = s->vy = 0;
        s->state = SS_PLAY;
        s->face = (uint8_t)dir8(-at[i][0], -at[i][1]);
        s->power = PW_NONE;
        s->t = s->dash_cd = s->stun = s->knock = s->brake = s->power_t = s->mega = s->walk = 0;
        s->hit_by = -1;
        s->hit_t = 999;
        s->round_kos = 0;
        s->bump_t = s->ready_t = 99;
        s->think_t = (int16_t)(i * 3);
        s->target = -1;
        s->hx = s->hy = 0;
        s->want_dash = 0;
        s->mode = BM_IDLE;
    }
}

void world_new_round(world *w)
{
    w->round++;
    w->phase = PH_INTRO;
    w->phase_t = 0;
    w->round_winner = -1;
    floe_new(w);
    place_seals(w);
    memset(&w->fish, 0, sizeof w->fish);
    w->fish.next = FISH_FIRST;
    w->crack_t = FIRST_CRACK;
    w->crack_gap = CRACK_GAP0;
    w->last_break_t = 0;
    w->hurry = 0;
    w->gev |= GEV_INTRO;
}

void world_init(world *w, int humans, uint32_t seed, int level)
{
    int gen = w->floe_gen;                          /* keeps counting: draw.c repaints a new floe */
    memset(w, 0, sizeof *w);
    w->floe_gen = gen;
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->humans = humans < 1 ? 1 : humans > MAX_PLAYERS ? MAX_PLAYERS : humans;
    for (int i = 0; i < SEALS; i++) w->s[i].human = (uint8_t)(i < w->humans);
    w->level = level;
    w->match_no = 1;
    w->match_winner = -1;
    world_new_round(w);
}

void world_next_match(world *w)
{
    w->match_no++;
    w->level++;
    w->match_over = 0;
    w->match_winner = -1;
    w->round = 0;
    for (int i = 0; i < SEALS; i++) {
        w->s[i].wins = 0;
        w->s[i].kos = 0;
    }
    world_new_round(w);
}

static void seal_control(world *w, int i, const pin *in)
{
    seal *s = &w->s[i];
    if (s->state != SS_PLAY || s->stun > 0) return;
    int dx = in->dx, dy = in->dy;
    if (in->dash && !s->dash_cd) {
        int a;
        if (dx || dy) a = iatan2(dy, dx);
        else a = s->face * 32;
        int v = DASH_V;
        s->mega = s->power == PW_MEGA;
        if (s->mega) {
            v = v * MEGA_SPEED_PCT / 100;
            s->power = PW_NONE;
            s->power_t = 0;
        }
        s->vx = (int32_t)((int64_t)icos(a) * v / FX);
        s->vy = (int32_t)((int64_t)isin(a) * v / FX);
        s->state = SS_DASH;
        s->t = 0;
        s->face = (uint8_t)(((a + 16) >> 5) & 7);
        s->brake = 0;
        s->knock = 0;
        w->events[i] |= EV_DASH;
        w->dashes++;
        return;
    }
    if (dx || dy) {
        s->vx += (int32_t)((int64_t)WADDLE_ACC * dx / 256);
        s->vy += (int32_t)((int64_t)WADDLE_ACC * dy / 256);
        s->face = (uint8_t)dir8(dx, dy);
    }
}

static int32_t damp(int32_t v, int f)
{
    int32_t d = (int32_t)(((int64_t)v * f) / 256);
    if (!d && v) d = v > 0 ? 1 : -1;
    return v - d;
}

static void seal_move(world *w, int i, int held)
{
    seal *s = &w->s[i];
    if (s->t < 30000) s->t++;
    if (s->hit_t < 999) s->hit_t++;
    if (s->bump_t < 99) s->bump_t++;
    if (s->ready_t < 99) s->ready_t++;
    if (s->stun > 0) s->stun--;
    if (s->knock > 0) s->knock--;
    if (s->brake > 0) s->brake--;
    if (s->dash_cd > 0 && s->state != SS_DASH && !--s->dash_cd) {
        s->ready_t = 0;
        w->events[i] |= EV_READY;
    }
    if (s->power != PW_NONE && --s->power_t <= 0) {
        s->power = PW_NONE;
        s->power_t = 0;
    }
    int f;
    switch (s->state) {
    case SS_DASH:
        f = DASH_FRICTION;
        if (s->t >= DASH_FRAMES) {
            s->state = SS_PLAY;
            s->t = 0;
            s->brake = BRAKE_FRAMES;
            s->dash_cd = DASH_COOLDOWN;
            s->mega = 0;
        }
        break;
    case SS_FALL:
        f = FALL_FRICTION;
        if (s->t == FALL_SPLASH) w->events[i] |= EV_SPLASH;
        if (s->t >= FALL_FRAMES) {
            s->state = SS_OUT;
            s->t = 0;
        }
        break;
    case SS_OUT:
        s->vx = s->vy = 0;
        return;
    default:
        f = s->brake > 0 ? BRAKE_FRICTION : s->knock > 0 ? KNOCK_FRICTION : (held && !s->stun) ? MOVE_FRICTION : IDLE_FRICTION;
        break;
    }
    s->vx = damp(s->vx, f);
    s->vy = damp(s->vy, f);
    if (iabs(s->vx) < 24 && iabs(s->vy) < 24 && !held) s->vx = s->vy = 0;
    s->x += s->vx;
    s->y += s->vy;
    int sp = iabs(s->vx) + iabs(s->vy);
    s->walk = (int16_t)((s->walk + sp / 64) & 0x7fff);
}

static void end_dash(seal *s)
{
    if (s->state != SS_DASH) return;
    s->state = SS_PLAY;
    s->t = 0;
    s->brake = BRAKE_FRAMES / 2;
    s->dash_cd = DASH_COOLDOWN;
    s->mega = 0;
}

static void collide(world *w, int a, int b)
{
    seal *p = &w->s[a], *q = &w->s[b];
    int64_t dx = q->x - p->x, dy = q->y - p->y, rr = (int64_t)(seal_radius(p) + seal_radius(q)) * FX;
    int64_t d2 = dx * dx + dy * dy;
    if (d2 >= rr * rr) return;
    int64_t d = isqrt64(d2), nx, ny;
    if (d < 64) {
        nx = (a < b) ? FX : -FX;
        ny = 0;
    } else {
        nx = dx * FX / d;
        ny = dy * FX / d;
    }
    int64_t mp = seal_mass(p), mq = seal_mass(q), over = rr - d;
    p->x -= (int32_t)(over * mq / (mp + mq) * nx / FX);
    p->y -= (int32_t)(over * mq / (mp + mq) * ny / FX);
    q->x += (int32_t)(over * mp / (mp + mq) * nx / FX);
    q->y += (int32_t)(over * mp / (mp + mq) * ny / FX);
    int64_t vp = ((int64_t)p->vx * nx + (int64_t)p->vy * ny) / FX;      /* p's speed towards q */
    int64_t vq = -((int64_t)q->vx * nx + (int64_t)q->vy * ny) / FX;     /* q's towards p */
    int64_t vn = vp + vq;
    if (vn <= 0) return;
    if (p->state == SS_DASH && q->state == SS_DASH) {
        /* a clash: both slides bounce back, both dizzy */
        int64_t bp = (int64_t)CLASH_V * 2 * mq / (mp + mq), bq = (int64_t)CLASH_V * 2 * mp / (mp + mq);
        p->vx = (int32_t)(-bp * nx / FX);
        p->vy = (int32_t)(-bp * ny / FX);
        q->vx = (int32_t)(bq * nx / FX);
        q->vy = (int32_t)(bq * ny / FX);
        end_dash(p);
        end_dash(q);
        p->brake = q->brake = 0;
        p->stun = q->stun = STUN_FRAMES;
        p->knock = q->knock = KNOCK_FRAMES;
        p->hit_by = (int16_t)b;
        q->hit_by = (int16_t)a;
        p->hit_t = q->hit_t = 0;
        p->bump_t = q->bump_t = 0;
        w->events[a] |= EV_CLASH;
        w->events[b] |= EV_CLASH;
        w->clashes++;
        return;
    }
    int64_t sep = vn * BUMP_E / 100;
    if (sep < BUMP_MIN) sep = BUMP_MIN;
    int64_t dv = vn + sep, dp = dv * mq / (mp + mq), dq = dv * mp / (mp + mq);
    p->vx -= (int32_t)(dp * nx / FX);
    p->vy -= (int32_t)(dp * ny / FX);
    q->vx += (int32_t)(dq * nx / FX);
    q->vy += (int32_t)(dq * ny / FX);
    int pd = p->state == SS_DASH, qd = q->state == SS_DASH;
    if (pd) w->events[a] |= EV_SMACK;
    if (qd) w->events[b] |= EV_SMACK;
    end_dash(p);
    end_dash(q);
    /* who pushed whom: the one that came faster (or slid) is the pusher */
    if (vp >= vq || pd) {
        q->hit_by = (int16_t)a;
        q->hit_t = 0;
        q->knock = KNOCK_FRAMES;
        q->brake = 0;
    }
    if (vq > vp || qd) {
        p->hit_by = (int16_t)b;
        p->hit_t = 0;
        p->knock = KNOCK_FRAMES;
        p->brake = 0;
    }
    p->bump_t = q->bump_t = 0;
    w->events[a] |= EV_BUMP;
    w->events[b] |= EV_BUMP;
    w->bumps++;
}

static void message(world *w, int msg, int s)
{
    w->msg = msg;
    w->msg_seal = s;
    w->msg_t = MSG_FRAMES;
}

static void falls(world *w)
{
    for (int i = 0; i < SEALS; i++) {
        seal *s = &w->s[i];
        if (!seal_standing(s) || on_ice(w, s->x, s->y)) continue;
        s->state = SS_FALL;
        s->t = 0;
        s->power = PW_NONE;
        s->power_t = 0;
        s->mega = 0;
        w->events[i] |= EV_FALL;
        w->falls++;
        if (w->phase != PH_FIGHT) continue;
        int k = s->hit_by;
        if (k >= 0 && k != i && s->hit_t <= KO_CREDIT) {
            seal *h = &w->s[k];
            h->kos++;
            h->round_kos++;
            h->score += PTS_KO;
            w->events[k] |= EV_KO;
            if (h->last_ko_t > 0 && w->t - h->last_ko_t <= DOUBLE_WINDOW) {
                h->score += PTS_DOUBLE;
                w->events[k] |= EV_DOUBLE;
                message(w, MSG_DOUBLE, k);
            }
            h->last_ko_t = w->t;
        }
    }
}

/* the fish: it lands on solid ice away from the seals, flops, and gives its power to the seal that grabs it */
static void fish_step(world *w)
{
    fishy *f = &w->fish;
    if (!f->on) {
        if (--f->next > 0) return;
        f->next = (int16_t)(FISH_GAP + rnd(w, FISH_GAP_RND));
        for (int k = 0; k < 24; k++) {
            int x = FLOE_CX - 70 + rnd(w, 141), y = FLOE_CY - 70 + rnd(w, 141), ok = 1;
            for (int a = 0; a < 256 && ok; a += 32)
                ok = floe_field(w, (x + icos(a) * FISH_SAFE / FX) * 16 + 8, (y + isin(a) * FISH_SAFE / FX) * 16 + 8, 1) >= 256;
            for (int i = 0; i < SEALS && ok; i++) {
                const seal *s = &w->s[i];
                if (!seal_standing(s)) continue;
                int ddx = s->x / FX - x, ddy = s->y / FX - y;
                ok = ddx * ddx + ddy * ddy >= FISH_FAR * FISH_FAR;
            }
            if (!ok) continue;
            f->on = 1;
            f->x = (int16_t)x;
            f->y = (int16_t)y;
            f->t = 0;
            f->kind = (uint8_t)(rnd(w, 2) ? PW_MEGA : PW_HEAVY);
            w->gev |= GEV_FISH;
            w->fishes++;
            return;
        }
        return;
    }
    f->t++;
    if (f->t > FISH_LIFE || floe_field(w, f->x * 16 + 8, f->y * 16 + 8, 0) < 128) {
        f->on = 0;
        w->gev |= GEV_FISH_GONE;
        return;
    }
    for (int i = 0; i < SEALS; i++) {
        seal *s = &w->s[i];
        if (!seal_standing(s)) continue;
        int64_t dx = s->x - (int64_t)f->x * FX, dy = s->y - (int64_t)f->y * FX, r = (int64_t)(seal_radius(s) + FISH_R) * FX;
        if (dx * dx + dy * dy >= r * r) continue;
        s->power = f->kind;
        s->power_t = (int16_t)(f->kind == PW_HEAVY ? HEAVY_FRAMES : MEGA_FRAMES);
        f->on = 0;
        w->events[i] |= EV_POWER;
        message(w, f->kind == PW_HEAVY ? MSG_HEAVY : MSG_MEGA, i);
        return;
    }
}

static void round_check(world *w)
{
    int standing = 0, last = -1, humans = 0, humans_in = 0;
    for (int i = 0; i < SEALS; i++) {
        const seal *s = &w->s[i];
        if (seal_standing(s)) { standing++; last = i; }
        humans += s->human;
        if (s->human && s->state != SS_OUT && s->state != SS_FALL) humans_in++;
        if (s->state == SS_FALL && s->t < FALL_SPLASH) return;    /* wait for the splash */
    }
    if (humans && !humans_in && standing >= 2) w->hurry = 1;      /* every player out: the bots finish it quickly */
    if (standing > 1) return;
    w->phase = PH_END;
    w->phase_t = 0;
    w->round_winner = standing == 1 ? last : -1;
    w->gev |= GEV_ROUND_END;
    if (last < 0 || standing != 1) return;
    seal *s = &w->s[last];
    s->wins++;
    s->score += PTS_ROUND;
    w->events[last] |= EV_WIN_ROUND;
    if (s->round_kos >= SEALS - 1) {
        s->score += PTS_SWEEP;
        w->gev |= GEV_SWEEP;
        message(w, MSG_SWEEP, last);
    }
    if (s->wins >= WIN_ROUNDS) {
        w->match_winner = last;
        s->matches++;
        s->score += PTS_MATCH;
        w->events[last] |= EV_WIN_MATCH;
    }
}

void world_step(world *w, const pin in[SEALS])
{
    memset(w->events, 0, sizeof w->events);
    w->gev = 0;
    w->t++;
    if (w->msg_t > 0 && !--w->msg_t) w->msg = MSG_NONE;
    if (w->match_over) return;
    w->phase_t++;
    pin act[SEALS];
    memset(act, 0, sizeof act);
    if (w->phase == PH_FIGHT) {
        bot_par bp;
        bot_par_level(w->level, &bp);
        for (int i = 0; i < SEALS; i++) {
            if (w->s[i].human) act[i] = in[i];
            else bot_think(w, i, &bp, &act[i]);
        }
    }
    for (int i = 0; i < SEALS; i++) {
        if (w->phase == PH_FIGHT) seal_control(w, i, &act[i]);
        seal_move(w, i, (act[i].dx || act[i].dy) && w->phase == PH_FIGHT);
    }
    for (int a = 0; a < SEALS; a++)
        for (int b = a + 1; b < SEALS; b++)
            if (seal_standing(&w->s[a]) && seal_standing(&w->s[b])) collide(w, a, b);
    if (w->phase != PH_INTRO) falls(w);
    switch (w->phase) {
    case PH_INTRO:
        if (w->phase_t >= INTRO_FRAMES) {
            w->phase = PH_FIGHT;
            w->phase_t = 0;
            w->gev |= GEV_GO;
        }
        break;
    case PH_FIGHT:
        floe_step(w);
        fish_step(w);
        round_check(w);
        break;
    case PH_END:
        if (w->phase_t >= END_FRAMES) {
            if (w->match_winner >= 0) {
                w->phase = PH_MATCH;
                w->phase_t = 0;
                w->gev |= GEV_MATCH_END;
            } else {
                world_new_round(w);
            }
        }
        break;
    case PH_MATCH:
        if (w->phase_t >= MATCH_FRAMES) w->match_over = 1;
        break;
    }
}

/* ---- the bots ----------------------------------------------------------------------------------------------------------- */
void bot_par_level(int level, bot_par *out)
{
    static const int t[BOT_LEVELS][10] = BOT_LEVEL_TABLE;
    const int *v = t[level < 0 ? 0 : level >= BOT_LEVELS ? BOT_LEVELS - 1 : level];
    bot_par p = {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
    *out = p;
}

/* the player bot (bot=1); skill 1..10 plays like a human: slower, wobblier, forgets the edge and the cracks */
void bot_par_player(int skill, bot_par *out)
{
    static const int v[10] = BOT_PLAYER;
    int e = skill < 0 ? 0 : skill > 10 ? 10 : skill;
    bot_par p = {v[0] + 2 * e, v[1] + e, v[2] + 2 * e, v[3] - 5 * e, v[4] - 4 * e, v[5] - e / 2, v[6] - 4 * e,
                 v[7] + 2 * e, v[8] - 3 * e, v[9] - 6 * e};
    if (p.dodge < 0) p.dodge = 0;
    if (p.dodge_dash < 0) p.dodge_dash = 0;
    if (p.warn < 0) p.warn = 0;
    *out = p;
}

int edge_scan(const world *w, int px, int py, int warn_water, int *angle)
{
    int best = EDGE_SCAN, ba = 0;
    if (floe_field(w, px * 16 + 8, py * 16 + 8, warn_water) < 128) best = 0;
    for (int k = 0; k < 16 && best > 0; k++) {
        int a = k * 16, c = icos(a), s = isin(a);
        for (int r = 6; r < best; r += 6)
            if (floe_field(w, (px * FX + c * r) / 256 + 8, (py * FX + s * r) / 256 + 8, warn_water) < 128) {
                best = r;
                ba = a;
                break;
            }
    }
    if (angle) *angle = ba;
    return best;
}

/* (x, y) scaled to length 256 */
static void unit256(int *x, int *y)
{
    int d = isqrt64((int64_t)*x * *x + (int64_t)*y * *y);
    if (!d) return;
    *x = *x * 256 / d;
    *y = *y * 256 / d;
}

static void rotate(int *x, int *y, int a)
{
    int c = icos(a), s = isin(a), nx = (*x * c - *y * s) / FX, ny = (*x * s + *y * c) / FX;
    *x = nx;
    *y = ny;
}

static void bot_out(seal *me, pin *out)
{
    out->dx = me->hx;
    out->dy = me->hy;
    out->any = 1;
    if (me->want_dash && !me->dash_cd && me->state == SS_PLAY) {
        out->dash = 1;
        me->want_dash = 0;
    }
}

void bot_think(world *w, int si, const bot_par *bp, pin *out)
{
    seal *me = &w->s[si];
    memset(out, 0, sizeof *out);
    if (!seal_standing(me) || w->phase != PH_FIGHT) {
        me->think_t = 0;
        return;
    }
    if (me->knock == KNOCK_FRAMES && me->think_t > bp->react / 3) me->think_t = (int16_t)(bp->react / 3);   /* hit! */
    if (me->think_t > 0) {
        me->think_t--;
        bot_out(me, out);
        return;
    }
    me->think_t = (int16_t)(bp->react + rnd(w, bp->react_rnd + 1));
    me->want_dash = 0;
    int px = me->x / FX, py = me->y / FX;
    int ww = rnd(w, 100) < bp->warn;                    /* it sees the cracks this time */
    int careful = rnd(w, 100) >= bp->mistake;           /* ... and minds the edge */
    int qx = px + (int)((int64_t)me->vx * BOT_LOOK / FX), qy = py + (int)((int64_t)me->vy * BOT_LOOK / FX);
    int hx = 0, hy = 0;
    /* 0. knocked and sliding: dig in against the slide */
    if (careful && me->knock > 0 && iabs(me->vx) + iabs(me->vy) > FX) {
        hx = -me->vx / 16;
        hy = -me->vy / 16;
        me->mode = BM_SAFE;
        goto steer;
    }
    /* 1. where it slides to: too near the water, back to the middle (on a tiny floe the margin shrinks: no hiding) */
    int margin = bp->margin, room = edge_scan(w, w->safe_x, w->safe_y, ww, NULL) - 8;
    if (margin > room) margin = room;
    if (careful && edge_scan(w, qx, qy, ww, NULL) < margin) {
        hx = w->safe_x - px;
        hy = w->safe_y - py;
        me->mode = BM_SAFE;
        me->target = -1;
        goto steer;
    }
    /* 2. a slide coming at it: dodge sideways (sometimes) */
    for (int o = 0; o < SEALS; o++) {
        const seal *s = &w->s[o];
        if (o == si || s->state != SS_DASH) continue;
        int64_t rx = me->x - s->x, ry = me->y - s->y, vv = (int64_t)s->vx * s->vx + (int64_t)s->vy * s->vy;
        int64_t dot = rx * s->vx + ry * s->vy;
        if (dot <= 0 || !vv) continue;
        int64_t tt = dot / vv;
        if (tt > DODGE_T) continue;
        int64_t mx = (rx - s->vx * tt) / FX, my = (ry - s->vy * tt) / FX, lim = 2 * SEAL_R + 4;
        if (mx * mx + my * my > lim * lim) continue;
        if (rnd(w, 100) >= bp->dodge) break;
        int ax = -s->vy / 16, ay = s->vx / 16;
        unit256(&ax, &ay);
        int e1 = edge_scan(w, px + ax * 24 / 256, py + ay * 24 / 256, ww, NULL);
        int e2 = edge_scan(w, px - ax * 24 / 256, py - ay * 24 / 256, ww, NULL);
        if (e2 > e1) { ax = -ax; ay = -ay; e1 = e2; }
        hx = ax;
        hy = ay;
        if (!me->dash_cd && e1 >= 36 && rnd(w, 100) < bp->dodge_dash) me->want_dash = 1;
        me->mode = BM_DODGE;
        goto steer;
    }
    /* 3. a fish nearby */
    if (w->fish.on && rnd(w, 100) < bp->fish) {
        int fx = w->fish.x - px, fy = w->fish.y - py;
        if (fx * fx + fy * fy < FISH_SEEK * FISH_SEEK && edge_scan(w, w->fish.x, w->fish.y, ww, NULL) >= bp->margin / 2) {
            hx = fx;
            hy = fy;
            me->mode = BM_FISH;
            goto steer;
        }
    }
    {
        /* 4. a target: the nearest, the most exposed (near the water); not one that two others already go for */
        int best = -1, bc = 0, bwa = 0;
        for (int o = 0; o < SEALS; o++) {
            const seal *s = &w->s[o];
            if (o == si || !seal_standing(s)) continue;
            int ox = s->x / FX, oy = s->y / FX, wa, ed = edge_scan(w, ox, oy, 0, &wa);
            int chasers = 0;
            for (int k = 0; k < SEALS; k++)
                if (k != si && k != o && w->s[k].target == o && seal_standing(&w->s[k])) chasers++;
            int cost = isqrt64((int64_t)(ox - px) * (ox - px) + (int64_t)(oy - py) * (oy - py)) + EXPOSE_W * ed +
                       (chasers >= CHASERS_MAX ? 120 : 0) + (o == me->target ? -12 : 0);
            if (best < 0 || cost < bc) { best = o; bc = cost; bwa = wa; }
        }
        me->target = (int16_t)best;
        if (best < 0) {
            hx = w->safe_x - px;
            hy = w->safe_y - py;
            me->mode = BM_IDLE;
            goto steer;
        }
        const seal *t = &w->s[best];
        int ox = t->x / FX, oy = t->y / FX;
        int ux = ox - w->safe_x, uy = oy - w->safe_y;  /* push it outwards, away from the safe middle */
        if (ux * ux + uy * uy < 64) { ux = icos(bwa); uy = isin(bwa); }
        unit256(&ux, &uy);
        int tx = ox - px, ty = oy - py, d = isqrt64((int64_t)tx * tx + (int64_t)ty * ty);
        int align = d ? (tx * ux + ty * uy) / d : 256;
        if (d < BOT_DASH_RANGE && !me->dash_cd && me->state == SS_PLAY && align > BOT_ALIGN && rnd(w, 100) < bp->dash) {
            int lead = d * FX / DASH_V;                  /* frames to get there: aim where it will be */
            hx = tx + (int)((int64_t)t->vx * lead / FX);
            hy = ty + (int)((int64_t)t->vy * lead / FX);
            int sx = hx, sy = hy;
            unit256(&sx, &sy);
            /* a careful bot slides only if it stops on the ice when it misses */
            if (!careful || edge_scan(w, px + sx * (d + 40) / 256, py + sy * (d + 40) / 256, ww, NULL) > 0)
                me->want_dash = 1;
            me->mode = BM_DASH;
        } else if (align > 160 && d < 46) {
            hx = tx;                                   /* lined up and close: shove it by walking */
            hy = ty;
            me->mode = BM_PUSH;
        } else {
            /* get behind it (on the middle's side): the attack point, around it when it is in the way */
            hx = ox - ux * (2 * SEAL_R + 10) / 256 - px;
            hy = oy - uy * (2 * SEAL_R + 10) / 256 - py;
            if (d < 44 && align < 64) {
                int cx = -uy, cy = ux;
                if (tx * cy - ty * cx < 0) { cx = -cx; cy = -cy; }
                hx = hx * 2 + cx / 4;
                hy = hy * 2 + cy / 4;
            }
            me->mode = BM_STALK;
        }
    }
steer:
    if (!hx && !hy) hx = 1;
    if (bp->aim) rotate(&hx, &hy, rnd(w, 2 * bp->aim + 1) - bp->aim);
    unit256(&hx, &hy);
    /* never walk straight off: a careful bot turns towards the middle when the ice ends in front of it */
    if (careful && me->mode != BM_SAFE && me->mode != BM_DASH &&
        floe_field(w, (px * 256 + hx * 20) * 16 / 256 + 8, (py * 256 + hy * 20) * 16 / 256 + 8, ww) < 128) {
        int cx = w->safe_x - px, cy = w->safe_y - py;
        unit256(&cx, &cy);
        hx = (hx + cx * 2) / 3;
        hy = (hy + cy * 2) / 3;
        unit256(&hx, &hy);
        me->want_dash = 0;
    }
    me->hx = (int16_t)hx;
    me->hy = (int16_t)hy;
    bot_out(me, out);
}

/* the tests' quitter: it walks off the ice, straight away from the middle */
void bot_give_up(world *w, int si, pin *out)
{
    seal *me = &w->s[si];
    memset(out, 0, sizeof *out);
    if (!seal_standing(me) || w->phase != PH_FIGHT) return;
    int hx = me->x / FX - FLOE_CX, hy = me->y / FX - FLOE_CY;
    if (!hx && !hy) hx = si & 1 ? 1 : -1;
    unit256(&hx, &hy);
    out->dx = (int16_t)hx;
    out->dy = (int16_t)hy;
    out->any = 1;
}

/* ---- touch geometry (player 1) --------------------------------------------------------------------------------------------- */
void touch_button(int right, tbutton *b)
{
    b->x = right - BTN_DX;
    b->y = RS_SCREEN_H - BTN_DY;
    b->r = BTN_R;
}

int touch_on_button(const tbutton *b, int x, int y)
{
    int dx = x - b->x, dy = y - b->y, r = b->r + BTN_SLOP;
    return dx * dx + dy * dy <= r * r;
}

int touch_stick_zone(int left, int width, int x, int y)
{
    (void)y;
    return x >= left && x < left + width * STICK_ZONE_PCT / 100;
}

void touch_stick(int x0, int y0, int x, int y, int16_t *dx, int16_t *dy)
{
    int ex = x - x0, ey = y - y0, d = isqrt64((int64_t)ex * ex + (int64_t)ey * ey);
    if (d < STICK_DEAD) {
        *dx = *dy = 0;
        return;
    }
    int k = d >= STICK_R ? 256 : d * 256 / STICK_R;
    *dx = (int16_t)(ex * k / d);
    *dy = (int16_t)(ey * k / d);
}
