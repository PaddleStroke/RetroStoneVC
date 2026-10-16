/*
 * Gecko Golf: the rules. No drawing, no sound: integer maths only (Q16), the world's own seed, so a run is the same
 * on every machine and the tests can drive it alone (tests/test_rules.c links this file with the SDK's RNG only).
 *
 *   the course   an endless side view, one hole per 320-px stage, generated hole by hole from the seed (gen_hole):
 *                rolling ground, hills, valleys, cliffs, ledges, with ponds, bunkers, ice and bouncy mushrooms that
 *                come with the biome and the difficulty (the hole number); the cup on a flat green
 *   the ball     a point on a height map (one height per px): flight under gravity in sub-steps of at most 1 px,
 *                bounces off the slope's normal (each surface its bounce and grip), rolling along the slope, walls
 *                where the ground steps up, the cup a notch in the ground (a slow ball drops in, a fast one skips)
 *   the turns    the player farthest from the cup plays (ties: the honour, the fewest strokes on the last hole);
 *                each shot costs a stroke of the player's budget; water and out of bounds one more; a hole in one
 *                gives 3 back, in two 1; out of strokes, out
 *   the bot      plays a shot by simulating the candidates (sim_shot) and keeping the one that ends nearest the cup
 *
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
 */
#include "game.h"
#include <stdlib.h>
#include <string.h>

/* ---- maths ---------------------------------------------------------------------------------------------------------- */
static const int32_t SIN_HD[181] = {     /* sin of 0..90 degrees in half degrees, Q16 */
    0, 572, 1144, 1716, 2287, 2859, 3430, 4001, 4572, 5142, 5712, 6281,
    6850, 7419, 7987, 8554, 9121, 9687, 10252, 10817, 11380, 11943, 12505, 13066,
    13626, 14185, 14742, 15299, 15855, 16409, 16962, 17514, 18064, 18613, 19161, 19707,
    20252, 20795, 21336, 21876, 22415, 22951, 23486, 24019, 24550, 25080, 25607, 26132,
    26656, 27177, 27697, 28214, 28729, 29242, 29753, 30261, 30767, 31271, 31772, 32271,
    32768, 33262, 33754, 34242, 34729, 35212, 35693, 36172, 36647, 37120, 37590, 38057,
    38521, 38982, 39441, 39896, 40348, 40797, 41243, 41686, 42126, 42562, 42995, 43425,
    43852, 44275, 44695, 45112, 45525, 45935, 46341, 46744, 47143, 47538, 47930, 48318,
    48703, 49084, 49461, 49834, 50203, 50569, 50931, 51289, 51643, 51993, 52339, 52682,
    53020, 53354, 53684, 54010, 54332, 54650, 54963, 55273, 55578, 55879, 56175, 56468,
    56756, 57040, 57319, 57594, 57865, 58131, 58393, 58650, 58903, 59152, 59396, 59635,
    59870, 60100, 60326, 60547, 60764, 60976, 61183, 61386, 61584, 61777, 61966, 62149,
    62328, 62503, 62672, 62837, 62997, 63152, 63303, 63449, 63589, 63725, 63856, 63983,
    64104, 64220, 64332, 64439, 64540, 64637, 64729, 64816, 64898, 64975, 65048, 65115,
    65177, 65234, 65287, 65334, 65376, 65414, 65446, 65474, 65496, 65514, 65526, 65534,
    65536};

int isin_hd(int a)
{
    a %= 720;
    if (a < 0) a += 720;
    if (a <= 180) return SIN_HD[a];
    if (a <= 360) return SIN_HD[360 - a];
    if (a <= 540) return -SIN_HD[a - 360];
    return -SIN_HD[720 - a];
}

int icos_hd(int a) { return isin_hd(a + 180); }

static uint64_t isqrt64(uint64_t v)
{
    uint64_t r = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int32_t floor16(int32_t x) { return x >= 0 ? x >> 16 : -((-x + 0xffff) >> 16); }

static uint32_t hash32(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

int pin_none(pin *in)
{
    memset(in, 0, sizeof *in);
    in->set_aim = in->set_gauge = -1;
    return 0;
}

/* ---- the course ----------------------------------------------------------------------------------------------------- */
int terrain_y(const terrain *t, int32_t wx) { return t->h[wx & TMASK]; }
int terrain_surf(const terrain *t, int32_t wx) { return t->s[wx & TMASK]; }
int terrain_seq(const terrain *t, int32_t wx) { return t->q[wx & TMASK]; }
int biome_of_seq(int seq) { return seq % BI_COUNT; }
const hole_rec *world_hole(const world *w, int k) { return &w->tr.holes[k % HOLE_RING]; }

static int rnd(rs_rng *r, int n) { return n > 0 ? rs_rng_range(r, n) : 0; }

static void put(terrain *t, int32_t x, int y, int s, int q)
{
    t->h[x & TMASK] = (int16_t)clampi(y, Y_TOP, Y_BOT);
    t->s[x & TMASK] = (uint8_t)s;
    t->q[x & TMASK] = (uint8_t)q;
}

/* smoothstep, f and the result in 0..FX */
static int32_t smooth16(int32_t f)
{
    int64_t ff = f;
    return (int32_t)(ff * ff / FX * (3 * FX - 2 * ff) / FX);
}

typedef struct gpt { int32_t x; int y, step; } gpt;   /* a control point; step: a cliff to the next one */

static int pick_shape(rs_rng *r, int d, int prev)
{
    int wgt[SH_COUNT] = {3, 1, 1, 0, 0, 0};
    if (d >= 2) { wgt[SH_ROLL] = 2; wgt[SH_HILL] = 2; wgt[SH_VALLEY] = 2; wgt[SH_CLIFF_UP] = 1; wgt[SH_CLIFF_DOWN] = 1; }
    if (d >= 7) { wgt[SH_ROLL] = 1; wgt[SH_CLIFF_UP] = 2; wgt[SH_CLIFF_DOWN] = 2; wgt[SH_LEDGE] = 1; }
    if (d >= 14) wgt[SH_LEDGE] = 2;
    if (prev == SH_LEDGE) wgt[SH_LEDGE] = 0;          /* never two ledges in a row */
    int sum = 0;
    for (int i = 0; i < SH_COUNT; i++) sum += wgt[i];
    int v = rnd(r, sum);
    for (int i = 0; i < SH_COUNT; i++) {
        if (v < wgt[i]) return i;
        v -= wgt[i];
    }
    return SH_ROLL;
}

/* the chance (percent) of each hazard by biome: pond, bunker, ice, mushrooms */
static const uint8_t HAZ_PCT[BI_COUNT][4] = {
    {55, 40, 0, 25},        /* meadow: ponds, a few bunkers and mushrooms */
    {20, 75, 0, 0},         /* dunes: bunkers everywhere, an oasis now and then */
    {25, 15, 70, 0},        /* snowy hills: ice */
    {50, 20, 10, 55},       /* night: ponds and glowing mushrooms */
    {40, 25, 15, 75}};      /* candy land: gumdrops (mushrooms) */

static void add_prop(hole_rec *h, rs_rng *r, terrain *t, int32_t a, int32_t b, int biome)
{
    static const uint8_t KINDS[BI_COUNT][2] = {{PR_FLOWER, PR_TUFT}, {PR_CACTUS, PR_ROCK}, {PR_PINE, PR_SNOWMAN},
                                               {PR_GLOWSHROOM, PR_FIREFLY}, {PR_LOLLIPOP, PR_CANDYCANE}};
    int n = 0;
    for (int i = 0; i < PROPS_PER_HOLE; i++) h->props[i].kind = PR_NONE;
    for (int tries = 0; tries < 10 && n < PROPS_PER_HOLE - 1 + (rnd(r, 2)); tries++) {
        if (b - a < 24) break;
        int32_t x = a + rnd(r, (int)(b - a));
        if (t->s[x & TMASK] != SF_GRASS || t->s[(x - 4) & TMASK] != SF_GRASS || t->s[(x + 4) & TMASK] != SF_GRASS)
            continue;
        if (iabs(t->h[(x - 4) & TMASK] - t->h[(x + 4) & TMASK]) > 5) continue;
        int clash = 0;
        for (int j = 0; j < n; j++)
            if (iabs((int)(h->props[j].x - x)) < 18) clash = 1;
        if (clash) continue;
        prop *p = &h->props[n++];
        p->x = x;
        p->y = t->h[x & TMASK];
        p->kind = KINDS[biome][rnd(r, 3) == 0 ? 1 : 0];
        p->var = (uint8_t)rnd(r, 4);
    }
}

/* one hole: the course from the last cup (gen_x, gen_y) to this hole's cup, and its hazards and decor */
static void gen_hole(terrain *t, int k)
{
    rs_rng r;
    rs_rng_seed(&r, hash32(t->seed, (uint32_t)k) | 1u);
    int d = k < DIFF_MAX ? k : DIFF_MAX;
    int seq = k / HOLES_PER_BIOME, biome = biome_of_seq(seq);
    int32_t T = t->gen_x;
    int y0 = t->gen_y;
    int dist = DIST_MIN + rnd(&r, DIST_RND0 + DIST_RND_D * d);
    if (dist > DIST_MAX) dist = DIST_MAX;
    int range = CUP_DY0 + CUP_DY_D * d;
    if (range > CUP_DY_MAX) range = CUP_DY_MAX;
    int yc = clampi(y0 + rnd(&r, 2 * range + 1) - range, CUP_Y_TOP, CUP_Y_BOT);
    int32_t C = T + dist;
    int shape = pick_shape(&r, d, t->prev_shape);
    int32_t a = T + TEE_FLAT, b = C - GREEN_HALF;
    gpt pt[10];
    int n = 0;
    pt[n++] = (gpt){T, y0, 0};
    if (t->prev_shape == SH_LEDGE) {                    /* off the last ledge: a drop after the tee */
        int drop = 20 + rnd(&r, 20);
        pt[n++] = (gpt){a, y0, 1};
        a += 3;
        y0 = clampi(y0 + drop, Y_TOP, Y_BOT);
    }
    pt[n++] = (gpt){a, y0, 0};
    int32_t L = b - a;
    int mid_lo = y0 < yc ? y0 : yc, mid_hi = y0 > yc ? y0 : yc;
    switch (shape) {
    case SH_ROLL: {
        int bumps = 1 + rnd(&r, 2);
        for (int i = 1; i <= bumps; i++) {
            int32_t x = a + L * i / (bumps + 1) + rnd(&r, 17) - 8;
            int y = y0 + (yc - y0) * i / (bumps + 1) + rnd(&r, 2 * (6 + d / 2) + 1) - (6 + d / 2);
            pt[n++] = (gpt){x, clampi(y, Y_TOP, Y_BOT), 0};
        }
        break;
    }
    case SH_HILL: {
        int32_t x = a + L * (35 + rnd(&r, 30)) / 100;
        pt[n++] = (gpt){x, clampi(mid_lo - 16 - rnd(&r, 16 + d * 2), Y_TOP, Y_BOT), 0};
        break;
    }
    case SH_VALLEY: {
        int32_t x = a + L * (35 + rnd(&r, 30)) / 100;
        pt[n++] = (gpt){x, clampi(mid_hi + 22 + rnd(&r, 18 + d * 2), Y_TOP, Y_BOT), 0};
        break;
    }
    case SH_CLIFF_UP:
    case SH_CLIFF_DOWN: {
        int32_t x = a + L * (30 + rnd(&r, 35)) / 100;
        int hgt = 20 + rnd(&r, 14 + d);
        int y = shape == SH_CLIFF_UP ? yc + hgt : yc - hgt;
        y = clampi(y, Y_TOP, Y_BOT);
        pt[n++] = (gpt){x, y, 1};
        pt[n++] = (gpt){x + 2, clampi(yc + rnd(&r, 9) - 4, Y_TOP, Y_BOT), 0};
        break;
    }
    case SH_LEDGE: {
        int low = clampi(yc + 22 + rnd(&r, 18 + d), Y_TOP, Y_BOT);
        int32_t x = a + L * (40 + rnd(&r, 20)) / 100;
        pt[n++] = (gpt){x, clampi((y0 + low) / 2 + rnd(&r, 13) - 6, Y_TOP, Y_BOT), 0};
        pt[n++] = (gpt){b - 6, low, 1};
        pt[n++] = (gpt){b - 3, yc, 0};
        break;
    }
    }
    pt[n++] = (gpt){b, yc, 0};
    pt[n++] = (gpt){C, yc, 0};
    /* the ground between the points: smooth, or a cliff */
    for (int i = 0; i + 1 < n; i++) {
        int32_t x0 = pt[i].x, x1 = pt[i + 1].x;
        for (int32_t x = x0; x < x1; x++) {
            int y;
            if (pt[i].step) y = pt[i].y;
            else y = pt[i].y + (int)((int64_t)(pt[i + 1].y - pt[i].y) * smooth16((int32_t)((x - x0) * FX / (x1 - x0))) / FX);
            put(t, x, y, SF_GRASS, seq);
        }
    }
    /* hazards: by biome and difficulty, never on the tee or the green */
    int haz = 0;
    int32_t ha = pt[1].x + 4, hb = b - 6;              /* where hazards may go */
    if (d >= 4 && rnd(&r, 100) < HAZ_PCT[biome][0]) {
        if (shape == SH_VALLEY) {
            int bottom = 0;
            for (int32_t x = ha; x < hb; x++) if (t->h[x & TMASK] > bottom) bottom = t->h[x & TMASK];
            int wl = bottom - 6 - rnd(&r, 8);
            int wet = 0;
            for (int32_t x = ha; x < hb; x++)
                if (t->h[x & TMASK] > wl) { t->h[x & TMASK] = (int16_t)wl; t->s[x & TMASK] = SF_WATER; wet++; }
            if (wet) haz |= HZ_POND;
        } else if (d >= 5 && hb - ha > 70) {
            /* a pond dug in a flat-ish stretch */
            int w = 18 + rnd(&r, 16 + d / 2);
            for (int tries = 0; tries < 6 && !(haz & HZ_POND); tries++) {
                int32_t p = ha + 10 + rnd(&r, (int)(hb - ha - w - 20));
                int hl = t->h[p & TMASK], hr = t->h[(p + w) & TMASK];
                if (iabs(hl - hr) > 6) continue;
                int wl = (hl > hr ? hl : hr) + 2;
                for (int32_t x = p + 1; x < p + w; x++) {
                    t->h[x & TMASK] = (int16_t)clampi(wl, Y_TOP, Y_BOT);
                    t->s[x & TMASK] = SF_WATER;
                }
                haz |= HZ_POND;
            }
        }
    }
    if (d >= 1 && rnd(&r, 100) < HAZ_PCT[biome][1]) {
        int w = 14 + rnd(&r, 14 + d / 3);
        int32_t c0 = rnd(&r, 100) < 60 ? b - w - 2 - rnd(&r, 16) : ha + rnd(&r, (int)(hb - ha > w ? hb - ha - w : 1));
        int ok = c0 >= ha && c0 + w <= b - 2;
        for (int32_t x = c0; ok && x < c0 + w; x++) if (t->s[x & TMASK] != SF_GRASS) ok = 0;
        if (ok) {
            for (int32_t x = c0; x < c0 + w; x++) {
                int u = (int)(x - c0) * 2 - w, dep = 3 - 3 * u * u / (w * w);
                t->h[x & TMASK] = (int16_t)clampi(t->h[x & TMASK] + dep, Y_TOP, Y_BOT);
                t->s[x & TMASK] = SF_SAND;
            }
            haz |= HZ_BUNKER;
        }
    }
    if (d >= 3 && rnd(&r, 100) < HAZ_PCT[biome][2]) {
        int w = 30 + rnd(&r, 30 + d);
        int32_t c0 = rnd(&r, 2) ? b - w : ha + rnd(&r, (int)(L / 2 > 1 ? L / 2 : 1));
        if (c0 < ha) c0 = ha;
        for (int32_t x = c0; x < c0 + w && x < b; x++)
            if (t->s[x & TMASK] == SF_GRASS) t->s[x & TMASK] = SF_ICE;
        haz |= HZ_ICE;
    }
    if (d >= 3 && rnd(&r, 100) < HAZ_PCT[biome][3]) {
        int count = 1 + (d >= 10 ? rnd(&r, 2) : 0);
        for (int m = 0; m < count; m++) {
            int w = 12 + rnd(&r, 8), hgt = 6 + rnd(&r, 5);
            if (hb - ha - w - 30 <= 0) break;
            int32_t c0 = ha + 12 + rnd(&r, (int)(hb - ha - w - 24));
            int ok = 1;
            for (int32_t x = c0 - 2; x < c0 + w + 2; x++) if (t->s[x & TMASK] != SF_GRASS) ok = 0;
            if (!ok) continue;
            for (int32_t x = c0; x < c0 + w; x++) {
                int u = (int)(x - c0) * 2 + 1 - w, dome = hgt - hgt * u * u / (w * w);
                t->h[x & TMASK] = (int16_t)clampi(t->h[x & TMASK] - dome, Y_TOP, Y_BOT);
                t->s[x & TMASK] = SF_MUSH;
            }
            haz |= HZ_MUSH;
        }
    }
    hole_rec *h = &t->holes[k % HOLE_RING];
    memset(h, 0, sizeof *h);
    h->tee_x = T;
    h->cup_x = C;
    h->tee_y = (int16_t)t->h[T & TMASK];
    h->cup_y = (int16_t)yc;
    h->biome = (uint8_t)biome;
    h->seq = (uint8_t)seq;
    h->shape = (uint8_t)shape;
    h->hazards = (uint8_t)haz;
    add_prop(h, &r, t, pt[1].x + 8, b - 14, biome);
    t->gen_x = C;
    t->gen_y = yc;
    t->gen_hole = k + 1;
    t->prev_shape = shape;
    t->rev++;
}

void world_gen_ahead(world *w)
{
    while (w->tr.gen_x < w->hole_cam + GEN_AHEAD) gen_hole(&w->tr, w->tr.gen_hole);
}

static void cup_dig(world *w, int k, int dig)
{
    const hole_rec *h = world_hole(w, k);
    for (int32_t x = h->cup_x - CUP_W / 2; x < h->cup_x + CUP_W / 2; x++) {
        w->tr.h[x & TMASK] = (int16_t)(h->cup_y + (dig ? CUP_DEPTH : 0));
        w->tr.s[x & TMASK] = (uint8_t)(dig ? SF_CUP : SF_GRASS);
    }
    w->tr.rev++;
}

int world_cup_sx(const world *w) { return (int)(world_hole(w, w->hole)->cup_x - w->hole_cam); }

/* ---- the ball ------------------------------------------------------------------------------------------------------- */
typedef struct pctx { const terrain *t; int32_t cam; int cup_l, cup_r, lip; } pctx;

static void pctx_of(const world *w, pctx *c)
{
    const hole_rec *h = world_hole(w, w->hole);
    c->t = &w->tr;
    c->cam = w->hole_cam;
    int cx = (int)(h->cup_x - w->hole_cam);
    c->cup_l = cx - CUP_W / 2;
    c->cup_r = cx + CUP_W / 2;
    c->lip = h->cup_y;
}

static int H(const pctx *c, int32_t sx) { return c->t->h[(c->cam + sx) & TMASK]; }
static int S(const pctx *c, int32_t sx) { return c->t->s[(c->cam + sx) & TMASK]; }

/* the ground's y (Q16) under stage x (Q16): linear between two columns, flat up to a step */
static int32_t ground(const pctx *c, int32_t x)
{
    int32_t col = floor16(x);
    int h0 = H(c, col), h1 = H(c, col + 1);
    if (iabs(h1 - h0) > WALL_STEP) return h0 << 16;
    return (h0 << 16) + (h1 - h0) * (x - (col << 16));
}

/* the slope dy/dx (Q16) at a column: over up to 2 px each side, not across a step */
static int32_t slope(const pctx *c, int32_t col)
{
    int32_t l = col, r = col + 1;
    if (iabs(H(c, r) - H(c, l)) > WALL_STEP) r = col;
    for (int i = 0; i < 2 && iabs(H(c, l - 1) - H(c, l)) <= WALL_STEP; i++) l--;
    for (int i = 0; i < 2 && iabs(H(c, r + 1) - H(c, r)) <= WALL_STEP; i++) r++;
    if (r == l) return 0;
    return (H(c, r) - H(c, l)) * FX / (r - l);
}

static void surface_of(int s, int *e, int *keep, int32_t *mu, int32_t *hold)
{
    switch (s) {
    case SF_SAND: *e = SAND_E; *keep = SAND_KEEP; *mu = SAND_MU; *hold = SAND_HOLD; break;
    case SF_ICE: *e = ICE_E; *keep = ICE_KEEP; *mu = ICE_MU; *hold = ICE_HOLD; break;
    case SF_MUSH: *e = MUSH_E; *keep = MUSH_KEEP; *mu = GRASS_MU; *hold = GRASS_HOLD; break;
    default: *e = GRASS_E; *keep = GRASS_KEEP; *mu = GRASS_MU; *hold = GRASS_HOLD; break;
    }
}

static int in_cup(const pctx *c, const ball *b)
{
    int32_t col = floor16(b->x);
    return col >= c->cup_l && col < c->cup_r && b->y > ((c->lip + CUP_IN) << 16);
}

static int holed(const pctx *c, ball *b)
{
    b->state = BS_CUP;
    b->x = (((c->cup_l + c->cup_r) << 16) / 2);
    b->y = (c->lip + CUP_DEPTH) << 16;
    b->vx = b->vy = b->u = 0;
    return EV_CUP;
}

static int out_of_bounds(const ball *b) { return b->x < (OB_LEFT << 16) || b->x > (OB_RIGHT << 16); }

static void at_rest(const pctx *c, ball *b)
{
    b->state = BS_REST;
    b->vx = b->vy = b->u = 0;
    b->y = ground(c, b->x);
}

/* the ball meets the ground at column col: bounce off the slope, or roll */
static int contact(const pctx *c, ball *b, int32_t col, int s)
{
    int ev = 0;
    int32_t k = slope(c, col);
    int64_t len = (int64_t)isqrt64((uint64_t)((int64_t)FX * FX + (int64_t)k * k));
    int32_t nxq = (int32_t)((int64_t)k * FX / len), nyq = (int32_t)(-(int64_t)FX * FX / len);
    int32_t txq = -nyq, tyq = nxq;
    int64_t vn = ((int64_t)b->vx * nxq + (int64_t)b->vy * nyq) >> 16;     /* < 0: into the ground */
    int64_t vt = ((int64_t)b->vx * txq + (int64_t)b->vy * tyq) >> 16;
    if (vn > 0) vn = 0;
    int e, keep;
    int32_t mu, hold;
    surface_of(s, &e, &keep, &mu, &hold);
    int64_t vn2 = -vn * e / 256, vt2 = vt * keep / 256;
    if (s == SF_MUSH && b->mush < 12) {
        b->mush++;
        if (vn2 < MUSH_MIN) vn2 = MUSH_MIN;
        if (vt2 < MUSH_NUDGE && vt2 > -MUSH_NUDGE) vt2 = (k > 0 || (k == 0 && vt2 >= 0)) ? MUSH_NUDGE : -MUSH_NUDGE;
        ev |= EV_MUSH;
    } else {
        if (s != SF_MUSH) b->mush = 0;
        if (vn2 < ROLL_VN) {
            b->state = BS_ROLL;
            b->u = (int32_t)vt2;
            b->vx = b->vy = 0;
            b->surf = (uint8_t)s;
            if (-vn > FX / 3) ev |= EV_BOUNCE;
            return ev;
        }
    }
    b->vx = (int32_t)((vn2 * nxq + vt2 * txq) >> 16);
    b->vy = (int32_t)((vn2 * nyq + vt2 * tyq) >> 16);
    b->surf = (uint8_t)s;
    return ev | EV_BOUNCE;
}

static int fly(const pctx *c, ball *b)
{
    int ev = 0;
    b->vy += GRAVITY;
    int32_t m = iabs(b->vx) > iabs(b->vy) ? iabs(b->vx) : iabs(b->vy);
    int n = (m >> 16) + 1;
    if (n > 24) n = 24;
    for (int i = 0; i < n && b->state == BS_FLY; i++) {
        int32_t nx = b->x + b->vx / n, ny = b->y + b->vy / n;
        int32_t oc = floor16(b->x), nc = floor16(nx);
        if (nc != oc && ny > ((H(c, nc) + WALL_TOL) << 16)) {   /* the side of a higher column: a wall */
            b->vx = -(int32_t)((int64_t)b->vx * WALL_E / 256);
            nx = b->x;
            nc = oc;
            ev |= EV_BOUNCE;
            b->surf = (uint8_t)S(c, oc);
        }
        int32_t gy = ground(c, nx);
        b->x = nx;
        if (ny >= gy) {
            int s = S(c, nc);
            b->y = gy;
            if (s == SF_WATER) {
                b->state = BS_SPLASH;
                return ev | EV_SPLASH;
            }
            ev |= contact(c, b, nc, s);
        } else {
            b->y = ny;
        }
        if (out_of_bounds(b)) {
            b->state = BS_OB;
            return ev | EV_OB;
        }
        if (in_cup(c, b)) return ev | holed(c, b);
    }
    return ev;
}

static int roll(const pctx *c, ball *b)
{
    int ev = 0;
    int32_t col = floor16(b->x);
    int s = S(c, col);
    if (s == SF_WATER) {
        b->state = BS_SPLASH;
        return EV_SPLASH;
    }
    int32_t k = slope(c, col);
    int64_t len = (int64_t)isqrt64((uint64_t)((int64_t)FX * FX + (int64_t)k * k));
    int32_t sinq = (int32_t)((int64_t)k * FX / len), cosq = (int32_t)((int64_t)FX * FX / len);
    int32_t acc = (int32_t)((int64_t)GRAVITY * sinq >> 16);
    int e, keep;
    int32_t mu, hold;
    surface_of(s, &e, &keep, &mu, &hold);
    b->u += acc;
    if (b->u > 0) b->u = b->u > mu ? b->u - mu : 0;
    else if (b->u < 0) b->u = b->u < -mu ? b->u + mu : 0;
    if (iabs(b->u) < STOP_U && iabs(acc) <= hold) {
        at_rest(c, b);
        return EV_REST;
    }
    int32_t dx = (int32_t)((int64_t)b->u * cosq >> 16);
    int n = (iabs(dx) >> 16) + 1;
    for (int i = 0; i < n; i++) {
        int32_t step = dx / n, nx = b->x + step;
        int32_t oc = floor16(b->x), nc = floor16(nx);
        if (nc != oc && (H(c, nc) << 16) < b->y - (WALL_TOL << 16)) {   /* rolling into a wall */
            b->u = -(int32_t)((int64_t)b->u * WALL_E / 256);
            return ev | EV_BOUNCE;
        }
        int32_t yline = b->y + (int32_t)((int64_t)step * k >> 16);
        int32_t gy = ground(c, nx);
        if (gy > yline + LEAVE_TOL) {                   /* the ground falls away: a crest, a cliff, the cup */
            b->x = nx;
            if (nc >= c->cup_l && nc < c->cup_r && iabs(b->u) <= CUP_CAPTURE) return ev | holed(c, b);
            b->state = BS_FLY;
            b->vx = (int32_t)((int64_t)b->u * cosq >> 16);
            b->vy = (int32_t)((int64_t)b->u * sinq >> 16);
            b->y = yline;
            return ev;
        }
        b->x = nx;
        b->y = gy;
        s = S(c, nc);
        b->surf = (uint8_t)s;
        if (s == SF_WATER) {
            b->state = BS_SPLASH;
            return ev | EV_SPLASH;
        }
        if (out_of_bounds(b)) {
            b->state = BS_OB;
            return ev | EV_OB;
        }
        if (in_cup(c, b)) return ev | holed(c, b);
    }
    return ev;
}

static int phys(const pctx *c, ball *b)
{
    if (b->state != BS_FLY && b->state != BS_ROLL) return 0;
    b->t++;
    int ev = b->state == BS_FLY ? fly(c, b) : roll(c, b);
    if ((b->state == BS_FLY || b->state == BS_ROLL) && b->t >= BALL_MAX_T) {
        at_rest(c, b);
        ev |= EV_REST;
        if (in_cup(c, b)) ev |= holed(c, b);
    }
    return ev;
}

int ball_step(const world *w, ball *b)
{
    pctx c;
    pctx_of(w, &c);
    return phys(&c, b);
}

void shot_velocity(int aim, int gauge, int32_t *vx, int32_t *vy)
{
    int64_t v = V_MIN + (int64_t)(V_MAX - V_MIN) * clampi(gauge, 0, GAUGE_MAX) / GAUGE_MAX;
    *vx = (int32_t)(v * icos_hd(aim) >> 16);
    *vy = (int32_t)(-v * isin_hd(aim) >> 16);
}

void ball_shoot(ball *b, int aim, int gauge)
{
    b->lx = b->x;
    b->ly = b->y;
    shot_velocity(aim, gauge, &b->vx, &b->vy);
    b->u = 0;
    b->t = 0;
    b->mush = 0;
    b->state = BS_FLY;
}

#define SIM_FRAMES 700
int sim_shot(const world *w, int p, int aim, int gauge, ball *end)
{
    pctx c;
    pctx_of(w, &c);
    ball b = w->g[p].b;
    ball_shoot(&b, aim, gauge);
    for (int f = 0; f < SIM_FRAMES && (b.state == BS_FLY || b.state == BS_ROLL); f++) phys(&c, &b);
    if (b.state == BS_FLY || b.state == BS_ROLL) at_rest(&c, &b);
    if (end) *end = b;
    return b.state;
}

/* ---- the touch slingshot ---------------------------------------------------------------------------------------------- */
int touch_shot(int dx, int dy, int *aim, int *gauge)
{
    int len = (int)isqrt64((uint64_t)((int64_t)dx * dx + (int64_t)dy * dy));
    if (len < TOUCH_DEAD) return 0;
    int ux = dx, uy = -dy;                              /* the shot's direction, up positive */
    if (uy < 0) uy = 0;                                 /* a pull upwards: along the ground */
    int best = ux >= 0 ? AIM_MIN : AIM_MAX;
    int64_t bd = -((int64_t)1 << 62);
    for (int a = AIM_MIN; a <= AIM_MAX; a++) {
        int64_t dot = (int64_t)icos_hd(a) * ux + (int64_t)isin_hd(a) * uy;
        if (dot > bd) { bd = dot; best = a; }
    }
    *aim = best;
    *gauge = clampi((len - TOUCH_DEAD) * GAUGE_MAX / (TOUCH_FULL - TOUCH_DEAD), 0, GAUGE_MAX);
    return 1;
}

/* ---- the world ------------------------------------------------------------------------------------------------------ */
void world_init(world *w, int players, uint32_t seed, int first_hole)
{
    memset(w, 0, sizeof *w);
    terrain *t = &w->tr;
    t->seed = seed ? seed : 1;
    if (first_hole < 0) first_hole = 0;
    /* the prelude: gentle ground rising behind the first tee */
    int y0 = 150, seq = first_hole / HOLES_PER_BIOME;
    for (int32_t x = -PRELUDE; x < 0; x++) {
        int32_t f = (int32_t)((-x) * FX / PRELUDE);
        put(t, x, y0 - (int)((int64_t)36 * smooth16(f) / FX), SF_GRASS, seq);
    }
    t->gen_x = 0;
    t->gen_y = y0;
    t->gen_hole = first_hole;
    t->prev_shape = SH_ROLL;
    w->hole = first_hole;
    w->hole_cam = -TEE_X;
    w->view_cam = w->hole_cam;
    world_gen_ahead(w);
    cup_dig(w, w->hole, 1);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        golfer *g = &w->g[p];
        g->b.x = g->b.lx = TEE_X << 16;
        g->b.y = g->b.ly = (int32_t)world_hole(w, w->hole)->tee_y << 16;
        g->b.state = BS_REST;
        g->strokes = START_STROKES;
        g->aim = AIM_START;
        g->holes = first_hole;
        if (p >= w->players) {
            g->out = 1;
            g->b.state = BS_OFF;
        }
    }
    w->turn = 0;
    w->phase = PH_AIM;
    w->gauge_dir = 1;
}

int world_running(const world *w)
{
    if (w->phase == PH_OVER) return 0;
    for (int p = 0; p < w->players; p++)
        if (!w->g[p].out) return 1;
    return 0;
}

int world_score(const world *w, int p) { return w->g[p].holes; }

static void message(world *w, int msg, int p, int n)
{
    w->msg = msg;
    w->msg_p = p;
    w->msg_n = n;
    w->msg_t = 0;
}

static void phase(world *w, int ph)
{
    w->phase = ph;
    w->phase_t = 0;
}

static void start_pan(world *w)
{
    const hole_rec *h = world_hole(w, w->hole);
    int old_seq = h->seq;
    cup_dig(w, w->hole, 0);
    w->pan_from = w->view_cam;
    int32_t cam = h->cup_x - TEE_X;
    for (int p = 0; p < w->players; p++) {
        golfer *g = &w->g[p];
        if (g->out) continue;
        g->last_hole = g->hole_strokes;
        g->hole_strokes = 0;
        g->done = 0;
        memset(&g->b, 0, sizeof g->b);
        g->b.x = g->b.lx = TEE_X << 16;
        g->b.y = g->b.ly = (int32_t)h->cup_y << 16;
        g->b.state = BS_REST;
    }
    w->hole++;
    w->hole_cam = cam;
    world_gen_ahead(w);
    cup_dig(w, w->hole, 1);
    w->wevents |= EV_PAN;
    if (world_hole(w, w->hole)->seq != old_seq) {
        w->wevents |= EV_BIOME;
        message(w, MSG_BIOME, 0, biome_of_seq(world_hole(w, w->hole)->seq));
    }
    phase(w, PH_PAN);
}

/* the next shot: the player farthest from the cup (ties: the honour, then the player number) */
static void next_turn(world *w)
{
    int cup = world_cup_sx(w), best = -1;
    int32_t bd = -1;
    for (int p = 0; p < w->players; p++) {
        const golfer *g = &w->g[p];
        if (g->out || g->done) continue;
        int32_t d = iabs(g->b.x - (cup << 16));
        if (best < 0 || d > bd || (d == bd && g->last_hole < w->g[best].last_hole)) {
            best = p;
            bd = d;
        }
    }
    if (best >= 0) {
        w->turn = best;
        w->gauge = 0;
        w->gauge_dir = 1;
        w->events[best] |= EV_TURN;
        phase(w, PH_AIM);
        return;
    }
    for (int p = 0; p < w->players; p++)
        if (!w->g[p].out) {
            start_pan(w);
            return;
        }
    w->wevents |= EV_OVER;
    phase(w, PH_OVER);
}

static void go_out(world *w, int p)
{
    golfer *g = &w->g[p];
    g->out = 1;
    g->b.state = BS_OFF;
    w->events[p] |= EV_OUT;
    message(w, MSG_OUT, p, 0);
}

static void shoot(world *w)
{
    golfer *g = &w->g[w->turn];
    if (w->gauge < GAUGE_MIN_SHOT) {                    /* a tap: no shot */
        w->gauge = 0;
        phase(w, PH_AIM);
        return;
    }
    g->strokes--;
    g->hole_strokes++;
    g->total++;
    w->shots++;
    ball_shoot(&g->b, g->aim, w->gauge);
    w->events[w->turn] |= EV_SHOT;
    phase(w, PH_FLIGHT);
}

static void aim_input(world *w, const pin *in)
{
    golfer *g = &w->g[w->turn];
    int before = g->aim;
    if (in->set_aim >= 0) g->aim = clampi(in->set_aim, AIM_MIN, AIM_MAX);
    if (in->turn) g->aim = clampi(g->aim + in->turn, AIM_MIN, AIM_MAX);
    if (g->aim / 10 != before / 10) w->events[w->turn] |= EV_AIM;
    if (w->phase == PH_AIM) {
        if (in->set_gauge >= 0) {                       /* the slingshot pulled past its dead zone */
            phase(w, PH_CHARGE);
            w->charge_touch = 1;
            w->events[w->turn] |= EV_CHARGE;
        } else {
            if (in->hold) {
                phase(w, PH_CHARGE);
                w->charge_touch = 0;
                w->gauge = 0;
                w->gauge_dir = 1;
                w->events[w->turn] |= EV_CHARGE;
            }
            return;
        }
    }
    if (in->cancel) {
        w->gauge = 0;
        phase(w, PH_AIM);
        return;
    }
    if (w->charge_touch) {
        if (in->set_gauge >= 0) w->gauge = clampi(in->set_gauge, 0, GAUGE_MAX);
        if (in->shoot) shoot(w);
        return;
    }
    if (in->hold) {
        w->gauge += w->gauge_dir * GAUGE_RATE;
        if (w->gauge >= GAUGE_MAX) { w->gauge = GAUGE_MAX; w->gauge_dir = -1; }
        if (w->gauge <= 0) { w->gauge = 0; w->gauge_dir = 1; }
    } else {
        shoot(w);
    }
}

static void hole_done(world *w, int p)
{
    golfer *g = &w->g[p];
    g->done = 1;
    g->holes++;
    int back = g->hole_strokes == 1 ? REFUND_ACE : g->hole_strokes == 2 ? REFUND_TWO : 0;
    g->strokes = clampi(g->strokes + back, 0, STROKES_MAX);
    if (g->hole_strokes == 1) {
        g->aces++;
        w->events[p] |= EV_ACE;
    }
    if (back) w->events[p] |= EV_REFUND;
    w->events[p] |= EV_CUP;
    message(w, g->hole_strokes == 1 ? MSG_ACE : g->hole_strokes == 2 ? MSG_TWO : MSG_HOLED, p, back);
    phase(w, PH_HOLED);
}

void world_step(world *w, const pin in[MAX_PLAYERS])
{
    memset(w->events, 0, sizeof w->events);
    w->wevents = 0;
    w->t++;
    w->phase_t++;
    if (w->msg) w->msg_t++;
    golfer *g = &w->g[w->turn];
    switch (w->phase) {
    case PH_AIM:
    case PH_CHARGE:
        aim_input(w, &in[w->turn]);
        break;
    case PH_FLIGHT: {
        pctx c;
        pctx_of(w, &c);
        int ev = phys(&c, &g->b);
        w->events[w->turn] |= ev & (EV_BOUNCE | EV_MUSH | EV_SPLASH | EV_OB | EV_REST);
        if (ev & EV_BOUNCE) w->bounce_surf = g->b.surf;
        if (g->b.state == BS_CUP) {
            hole_done(w, w->turn);
        } else if (g->b.state == BS_SPLASH || g->b.state == BS_OB) {
            message(w, g->b.state == BS_SPLASH ? MSG_WATER : MSG_OB, w->turn, PENALTY);
            phase(w, PH_SPLASH);
        } else if (g->b.state == BS_REST) {
            phase(w, PH_SETTLE);
        }
        break;
    }
    case PH_SETTLE:
        if (w->phase_t >= SETTLE_FRAMES) {
            if (g->strokes <= 0) go_out(w, w->turn);
            next_turn(w);
        }
        break;
    case PH_HOLED:
        if (w->phase_t >= HOLED_FRAMES) next_turn(w);
        break;
    case PH_SPLASH:
        if (w->phase_t >= SPLASH_FRAMES) {
            g->b.x = g->b.lx;
            g->b.y = g->b.ly;
            g->b.vx = g->b.vy = g->b.u = 0;
            g->b.state = BS_REST;
            g->strokes = clampi(g->strokes - PENALTY, 0, STROKES_MAX);
            g->hole_strokes += PENALTY;
            g->total += PENALTY;
            if (g->strokes <= 0) go_out(w, w->turn);
            next_turn(w);
        }
        break;
    case PH_PAN: {
        int32_t span = w->hole_cam - w->pan_from;
        w->view_cam = w->pan_from + (int32_t)((int64_t)span * smooth16(w->phase_t * FX / PAN_FRAMES) / FX);
        if (w->phase_t >= PAN_FRAMES) {
            w->view_cam = w->hole_cam;
            next_turn(w);
        }
        break;
    }
    default:
        break;
    }
}

/* ---- the bot ------------------------------------------------------------------------------------------------------- */
static int32_t shot_cost(const world *w, int result, const ball *b)
{
    int cup = world_cup_sx(w);
    int d = iabs(floor16(b->x) - cup);
    if (result == BS_CUP) return 0;
    if (result == BS_SPLASH || result == BS_OB) return (1 << 20) + d;
    int32_t cost = 64 + d * 16;
    if (b->surf == SF_SAND) cost += 8;
    return cost;
}

typedef struct cand { int aim, gauge; int32_t cost; } cand;

static void keep_best(cand *top, int n, int aim, int gauge, int32_t cost)
{
    for (int i = 0; i < n; i++) {
        if (top[i].cost < 0 || cost < top[i].cost) {
            for (int j = n - 1; j > i; j--) top[j] = top[j - 1];
            top[i] = (cand){aim, gauge, cost};
            return;
        }
    }
}

void bot_plan_shot(const world *w, int p, int skill, bot_plan *out)
{
    const golfer *g = &w->g[p];
    int cup = world_cup_sx(w), bx = floor16(g->b.x);
    int dir = cup >= bx ? 1 : -1;
    cand top[4];
    for (int i = 0; i < 4; i++) top[i] = (cand){AIM_START, 60, -1};
    ball end;
    for (int a = 6; a <= 179; a += a < 174 ? 8 : a < 178 ? 2 : 1) {   /* up to straight lobs (out of a ledge's foot) */
        int aim = dir > 0 ? a : 360 - a;
        aim = clampi(aim, AIM_MIN, AIM_MAX);
        for (int gauge = GAUGE_MIN_SHOT; gauge <= GAUGE_MAX; gauge += 6) {
            int r = sim_shot(w, p, aim, gauge, &end);
            keep_best(top, 4, aim, gauge, shot_cost(w, r, &end));
        }
    }
    cand best = top[0];
    if (skill < 5) {                                    /* refine around the best few */
        for (int i = 0; i < 4 && top[i].cost > 0; i++)
            for (int da = -4; da <= 4; da += 2)
                for (int dg = -4; dg <= 4; dg += 2) {
                    int aim = clampi(top[i].aim + da, AIM_MIN, AIM_MAX), gauge = top[i].gauge + dg;
                    if (gauge < GAUGE_MIN_SHOT || gauge > GAUGE_MAX) continue;
                    int r = sim_shot(w, p, aim, gauge, &end);
                    int32_t cost = shot_cost(w, r, &end);
                    if (cost < best.cost) best = (cand){aim, gauge, cost};
                }
    }
    if (skill > 0) {                                    /* a human-like hand: off by up to skill steps */
        uint32_t h = hash32(w->tr.seed ^ (uint32_t)(p * 7919), (uint32_t)(w->hole * 64 + g->hole_strokes));
        /* the aim off by up to skill half degrees, the power by up to skill * 2.5% of the shot (a putt: a little) */
        int na = skill, ng = best.gauge * skill / 40 + 1;
        best.aim = clampi(best.aim + (int)(h % (uint32_t)(2 * na + 1)) - na, AIM_MIN, AIM_MAX);
        best.gauge = clampi(best.gauge + ((int)((h >> 12) % (uint32_t)(2 * ng + 1)) - ng) / 2 * 2, GAUGE_MIN_SHOT,
                            GAUGE_MAX);
    }
    out->aim = (int16_t)best.aim;
    out->gauge = (int16_t)(best.gauge & ~1);
    out->hole = w->hole;
    out->stroke = g->hole_strokes + w->shots * 16;
    out->valid = 1;
}

void bot_give_up(const world *w, int p, bot_plan *out)
{
    int cup = world_cup_sx(w), bx = floor16(w->g[p].b.x);
    out->aim = (int16_t)(cup >= bx ? AIM_MAX - 30 : AIM_MIN + 30);     /* away from the cup, hard: out of bounds */
    out->gauge = GAUGE_MAX;
    out->hole = w->hole;
    out->stroke = w->g[p].hole_strokes + w->shots * 16;
    out->valid = 1;
}
