/*
 * Nutty Branch: the endless course. Chunk after chunk: a geometry (straight, curve, S-bend, dip, fork) and an
 * obstacle group, then a quiet stretch; content-light like Leady Squid, parametrised and random. Every spacing is in
 * frames at the speed of that stretch (F2U), so the timing stays human whatever the speed. The fairness of every
 * combination is checked by tests/test_physics.c over thousands of seeds (a planner with human limits).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
#include <string.h>

#define CHUNK_OBS 96

typedef struct chunk_ctx {
    world *w;
    int32_t s0;                 /* units: the chunk's start */
    int32_t v;                  /* Q8 units/frame: the speed there */
    int m, diff;                /* metres; difficulty 0..256 */
    obj ob[CHUNK_OBS];
    int n;
} chunk_ctx;

static int rnd(world *w, int n) { return n <= 1 ? 0 : rs_rng_range(&w->rng, n); }
static int rnd_in(world *w, int lo, int hi) { return lo + rnd(w, hi - lo + 1); }
static int32_t F2U(const chunk_ctx *c, int frames) { return (int32_t)((int64_t)frames * c->v >> 8); }

static void add(chunk_ctx *c, int kind, int32_t s, int32_t ang, int32_t len, int side)
{
    if (c->n >= CHUNK_OBS) return;
    obj *o = &c->ob[c->n++];
    memset(o, 0, sizeof *o);
    o->kind = (uint8_t)kind;
    o->s = s;
    o->ang = ang;
    o->len = len;
    o->side = (int8_t)side;
    o->t0 = -1;
    o->t1 = -1;
}

/* scenery: kind (decor_kind), side (-1 / 1), lateral (Q8 units from the centre line; side branches and fungi: their
 * angle), height (Q8, over the bark: kept in t0, never woken) */
static void decor(chunk_ctx *c, int kind, int32_t s, int side, int32_t lat, int32_t h)
{
    add(c, OB_DECOR, s, lat, kind, side);
    if (c->n) c->ob[c->n - 1].t0 = h;
}

static int drnd(world *w, int n) { return n <= 1 ? 0 : rs_rng_range(&w->g.drng, n); }

/* ---- the segments ------------------------------------------------------------------------------------------- */
static void put_seg(world *w, int32_t curve, int32_t dh)
{
    gen_state *g = &w->g;
    int i = g->seg & (SEG_RING - 1), j = (g->seg + 1) & (SEG_RING - 1);
    w->curve[i] = (int16_t)curve;
    w->head[i] = g->head;
    w->hgt[i] = g->h;
    g->head += curve;
    g->h += dh;
    w->head[j] = g->head;
    w->hgt[j] = g->h;
    g->seg++;
}

/* n segments: the curve eases in over the first `ease` and out over the last `ease`; the height follows a dip
 * (1 - cos) / 2 of depth `dip` (Q8 units, < 0 = a rise) over the whole run */
static void put_run(world *w, int n, int32_t curve, int ease, int32_t dip)
{
    int32_t h0 = w->g.h;
    for (int k = 0; k < n; k++) {
        int32_t c = curve;
        if (ease > 0 && k < ease) c = curve * (k + 1) / (ease + 1);
        if (ease > 0 && k >= n - ease) c = curve * (n - k) / (ease + 1);
        int32_t target = h0 - (int32_t)((int64_t)dip * (16384 - cos_q14((int32_t)((int64_t)ANG(360) * (k + 1) / n))) / 32768);
        put_seg(w, c, target - w->g.h);
    }
}

/* ---- the nuts ------------------------------------------------------------------------------------------------------ */
static void acorn_line(chunk_ctx *c, int32_t s, int32_t s1, int32_t ang0, int32_t ang1)
{
    int32_t step = F2U(c, 9);
    if (step < 24) step = 24;
    int n = (s1 - s) / step;
    for (int k = 0; k <= n && n > 0; k++)
        add(c, OB_ACORN, s + k * step, ang0 + (ang1 - ang0) * k / n, 0, 0);
}

static void edge_nut(chunk_ctx *c, int32_t s)
{
    int side = rnd(c->w, 2) ? 1 : -1;
    int32_t a = ANG(40) + rnd(c->w, ANG(12));
    add(c, rnd(c->w, 10) == 0 && c->m > 100 ? OB_WALNUT : OB_HAZEL, s, side * a, 0, 0);
}

/* ---- the groups ------------------------------------------------------------------------------------------------------ */
enum { G_NUTS, G_TWIGS, G_KNOTS, G_GAP, G_SAP, G_WEB, G_PECKER, G_CATER, G_WIND, G_CONES, G_OWL, G_FORK,
       G_SNAKE, G_SWING, G_BEES, G_SNOW, G_CROW, G_SHOWER, G_RAIN, G_HOLES, G_KINDS };
static const int UNLOCK_M[G_KINDS] = {0, 0, UNLOCK_KNOT, UNLOCK_GAP, UNLOCK_SAP, UNLOCK_WEB, UNLOCK_PECKER,
                                      UNLOCK_CATER, UNLOCK_WIND, UNLOCK_CONE, UNLOCK_OWL, UNLOCK_FORK, UNLOCK_SNAKE,
                                      UNLOCK_SWING, UNLOCK_BEES, 0, UNLOCK_CROW, UNLOCK_SHOWER, UNLOCK_RAIN,
                                      UNLOCK_KNOTHOLE};
static const int WEIGHT[G_KINDS] = {3, 6, 5, 5, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2, 5, 3, 2, 2, 4};

int gen_band(int32_t metres) { return (int)((metres < 0 ? 0 : metres) / BAND_M % 6); }
static int night(int m) { return gen_band(m) == 4; }

/* returns the units used by the group from s */
static int32_t group(chunk_ctx *c, int kind, int32_t s)
{
    world *w = c->w;
    int32_t s_start = s;
    switch (kind) {
    case G_NUTS: {
        int32_t len = F2U(c, 120);
        int32_t a = rnd_in(w, -ANG(25), ANG(25));
        acorn_line(c, s, s + len / 2, 0, a);
        acorn_line(c, s + len / 2 + F2U(c, 9), s + len, a, -a / 2);
        edge_nut(c, s + len / 3);
        return len;
    }
    case G_TWIGS: {
        int n = 1 + rnd(w, 1 + c->diff / 80);
        for (int k = 0; k < n; k++) {
            /* a branch grows out of one flank and crosses the path: a low one (jump it; acorns lead over it) or,
             * further on, a high arch (run under it: acorns lead through the middle) */
            int side = rnd(w, 2) ? 1 : -1, arch = c->m >= UNLOCK_ARCH && rnd(w, 5) < 2;
            add(c, OB_TWIG, s, arch ? 0 : side * ANG(21), arch, side);
            acorn_line(c, s - F2U(c, 34), s - F2U(c, 8), 0, arch ? 0 : -side * ANG(20));
            s += F2U(c, 48 + rnd(w, 30) - c->diff / 32);
        }
        if (rnd(w, 3) == 0) edge_nut(c, s);
        return s - s_start;
    }
    case G_KNOTS: {
        if (rnd(w, 2)) {                     /* a gate: two knots, a lane between them (or jump one) */
            int32_t a = ANG(31) + rnd(w, ANG(4));
            add(c, OB_KNOT, s, -a, 0, 0);
            add(c, OB_KNOT, s, a, 0, 0);
            acorn_line(c, s - F2U(c, 30), s + F2U(c, 12), 0, 0);
        } else {
            int32_t a = rnd_in(w, -ANG(26), ANG(26));
            add(c, OB_KNOT, s, a, 0, 0);
            int32_t ga = a > 0 ? a - ANG(34) : a + ANG(34);
            acorn_line(c, s - F2U(c, 30), s, ga / 2, ga);
        }
        return F2U(c, 50);
    }
    case G_GAP: {
        int frames = GAP_FRAMES_MIN + rnd(w, 1 + (GAP_FRAMES_MAX - GAP_FRAMES_MIN) * (64 + c->diff) / 320);
        int32_t len = F2U(c, frames);
        add(c, OB_GAP, s, 0, len, 0);
        acorn_line(c, s - F2U(c, 24), s - F2U(c, 6), 0, 0);
        acorn_line(c, s + len + F2U(c, 6), s + len + F2U(c, 24), 0, 0);
        return len + F2U(c, 30);
    }
    case G_SAP: {
        int32_t a = rnd(w, 3) == 0 ? 0 : (rnd(w, 2) ? 1 : -1) * ANG(12 + rnd(w, 10));
        int32_t len = SAP_LEN + rnd(w, 32);
        add(c, OB_SAP, s, a, len, 0);
        /* the lure: a nut on the far side of the patch, near the edge */
        add(c, rnd(w, 4) == 0 ? OB_WALNUT : OB_HAZEL, s + len / 2, a > 0 ? -ANG(36) : ANG(36), 0, 0);
        return len + F2U(c, 20);
    }
    case G_WEB: {
        int32_t a = rnd_in(w, -ANG(18), ANG(18));
        add(c, OB_WEB, s, a, 0, 0);
        int32_t ga = a > 0 ? a - ANG(46) : a + ANG(46);
        acorn_line(c, s - F2U(c, 36), s, ga / 2, ga);
        return F2U(c, 40);
    }
    case G_PECKER: {
        int side = rnd(w, 2) ? 1 : -1;
        int32_t a = side * (ANG(10) + rnd(w, ANG(16)));
        add(c, OB_PECKER, s, a, HOLE_LEN, side);
        acorn_line(c, s - F2U(c, 30), s + F2U(c, 6), -side * ANG(14), -side * ANG(22));
        return F2U(c, 40);
    }
    case G_CATER: {
        int side = rnd(w, 2) ? 1 : -1;      /* it crawls toward `side` */
        int32_t a = -side * (ANG(10) + rnd(w, ANG(25)));
        add(c, OB_CATER, s, a, 0, side);
        return F2U(c, 40);
    }
    case G_WIND: {
        int side = rnd(w, 2) ? 1 : -1;
        int32_t len = F2U(c, WIND_FRAMES);
        add(c, OB_WIND, s, 0, len, side);
        /* the nuts upwind: lean into it */
        acorn_line(c, s + F2U(c, 10), s + len - F2U(c, 10), -side * ANG(8), -side * ANG(16));
        if (c->diff > 120) {
            int ts = rnd(w, 2) ? 1 : -1;     /* a side twig in the gust (from the windward side: lean into it) */
            add(c, OB_TWIG, s + len / 2, ts * ANG(21), 0, ts);
        }
        return len;
    }
    case G_CONES: {
        int n = 1 + rnd(w, 2 + c->diff / 128);
        for (int k = 0; k < n; k++) {
            add(c, OB_CONE, s, rnd_in(w, -ANG(30), ANG(30)), 0, 0);
            s += F2U(c, 44 + rnd(w, 20));
        }
        return s - s_start;
    }
    case G_HOLES: {
        int n = 1 + rnd(w, 2);
        for (int k = 0; k < n; k++) {
            int32_t a = rnd_in(w, -ANG(28), ANG(28));
            add(c, OB_KNOTHOLE, s, a, KNOTHOLE_LEN, 0);
            acorn_line(c, s - F2U(c, 30), s - F2U(c, 6), a / 2, a > 0 ? a - ANG(30) : a + ANG(30));
            s += F2U(c, 50 + rnd(w, 24));
        }
        return s - s_start;
    }
    case G_SNAKE: {
        int side = rnd(w, 2) ? 1 : -1;      /* it slithers toward `side` */
        add(c, OB_SNAKE, s, -side * (ANG(15) + rnd(w, ANG(20))), 0, side);
        return F2U(c, 40);
    }
    case G_SWING: {
        add(c, OB_SWING, s, 0, rnd(w, SWING_PERIOD), 0);
        decor(c, DC_OVER0, s, 1, 0, SWING_PIVOT + Q8(8));       /* the limb it hangs from */
        acorn_line(c, s - F2U(c, 40), s - F2U(c, 12), 0, 0);
        return F2U(c, 40);
    }
    case G_BEES: {
        int side = rnd(w, 2) ? 1 : -1;      /* the swarm drifts toward `side` */
        add(c, OB_BEES, s, -side * (ANG(30) + rnd(w, ANG(15))), 0, side);
        decor(c, DC_HIVE, s - 40, -side, -side * Q8(96), Q8(72));   /* the hive it came out of */
        return F2U(c, 50);
    }
    case G_SNOW: {
        int n = 1 + rnd(w, 2 + c->diff / 128);
        for (int k = 0; k < n; k++) {
            add(c, OB_SNOW, s, rnd_in(w, -ANG(30), ANG(30)), 0, 0);
            s += F2U(c, 46 + rnd(w, 20));
        }
        return s - s_start;
    }
    case G_CROW: {
        int32_t len = F2U(c, 90), a = rnd_in(w, -ANG(20), ANG(20));
        acorn_line(c, s, s + len, a, a);
        add(c, OB_CROW, s, a, len, 0);      /* it lands at the far end and eats its way toward you */
        return len + F2U(c, 20);
    }
    case G_SHOWER: {
        int32_t len = F2U(c, 240);
        add(c, OB_SHOWER, s, 0, len, 0);
        return len;
    }
    case G_RAIN: {
        int32_t len = F2U(c, 300);
        add(c, OB_RAIN, s, 0, len, 0);
        acorn_line(c, s + F2U(c, 20), s + len - F2U(c, 20), 0, 0);
        return len;
    }
    case G_OWL: {
        int side = rnd(w, 2) ? 1 : -1;
        add(c, OB_OWL, s, 0, 0, side);
        acorn_line(c, s - F2U(c, 30), s - F2U(c, 8), 0, 0);
        return F2U(c, 40);
    }
    default:
        return 0;
    }
}

int gen_force = -1;

int gen_kind_of(const char *name)
{
    static const char *names[G_KINDS] = {"nuts", "twig", "knot", "gap", "sap", "web", "pecker", "cater", "wind", "cone",
                                         "owl", "fork", "snake", "swing", "bees", "snow", "crow", "shower", "rain",
                                         "hole"};
    for (int k = 0; name && k < G_KINDS; k++)
        if (!strcmp(name, names[k])) return k;
    return -1;
}

static int pick_group(chunk_ctx *c)
{
    if (gen_force >= 0) return gen_force;           /* tests and screenshots: one kind of group only */
    int tot = 0, wts[G_KINDS];
    for (int k = 0; k < G_KINDS; k++) {
        wts[k] = c->m >= UNLOCK_M[k] ? WEIGHT[k] : 0;
        if (k == G_OWL && night(c->m)) wts[k] = 5;                  /* the owls hunt more at night */
        if (k == G_SNOW && gen_band(c->m) != 5) wts[k] = 0;         /* the snow lumps: winter only */
        if (k == G_RAIN && c->w->g.last_rain_s > c->s0 - 2500) wts[k] = 0;
        if (k == G_BEES && c->w->g.last_bees_s > c->s0 - 1500) wts[k] = 0;
        if (k == G_OWL && c->w->g.last_owl_s > c->s0 - 1600) wts[k] = 0;
        if (k == G_FORK && c->w->g.last_fork_s > c->s0 - 3000) wts[k] = 0;
        if (k == G_WIND && c->w->g.last_wind_s > c->s0 - 1200) wts[k] = 0;
        tot += wts[k];
    }
    int r = rnd(c->w, tot);
    for (int k = 0; k < G_KINDS; k++) {
        if (r < wts[k]) return k;
        r -= wts[k];
    }
    return G_NUTS;
}

static void flush(chunk_ctx *c)
{
    world *w = c->w;
    /* insertion sort by s (a chunk's objects are nearly in order) */
    for (int i = 1; i < c->n; i++) {
        obj o = c->ob[i];
        int j = i - 1;
        while (j >= 0 && c->ob[j].s > o.s) { c->ob[j + 1] = c->ob[j]; j--; }
        c->ob[j + 1] = o;
    }
    for (int i = 0; i < c->n && w->ob_count < OB_MAX; i++)
        w->ob[(w->ob_first + w->ob_count++) % OB_MAX] = c->ob[i];
}

/* the scenery along a stretch (its own RNG: the course is the same with or without it): side branches growing out
 * of both sides, leaves and vines passing overhead, fungi on the flanks */
static void scenery(chunk_ctx *c, int32_t s0, int32_t s1)
{
    world *w = c->w;
    for (int32_t s = s0 + drnd(w, 160); s < s1; s += 160 + drnd(w, 240)) {
        int side = drnd(w, 2) ? 1 : -1, clear = 1;
        for (int i = 0; i < c->n; i++) {
            const obj *o = &c->ob[i];
            if ((o->kind == OB_GAP || o->kind == OB_KNOTHOLE) && s > o->s - 80 && s < o->s + o->len + 80) clear = 0;
            if (o->kind == OB_FORK && s > o->s - 80) clear = 0;
            if (o->kind == OB_TWIG && s > o->s - 60 && s < o->s + 60) clear = 0;
        }
        if (clear) decor(c, DC_SIDE0 + drnd(w, 3), s, side, side * ANG(84), 0);
    }
    for (int32_t s = s0 + drnd(w, 300); s < s1; s += 240 + drnd(w, 300)) {
        int side = drnd(w, 2) ? 1 : -1;
        decor(c, DC_OVER0 + drnd(w, 2), s, side, side * Q8(70 + drnd(w, 100)), Q8(120 + drnd(w, 50)));
    }
    if (drnd(w, 3) == 0) {
        int side = drnd(w, 2) ? 1 : -1;
        decor(c, DC_MUSH, s0 + drnd(w, s1 - s0 > 1 ? s1 - s0 : 1), side, side * ANG(74), 0);
    }
}

static void chunk(world *w)
{
    chunk_ctx c;
    memset(&c, 0, sizeof c);
    c.w = w;
    c.s0 = w->g.seg * SEG_LEN;
    c.m = c.s0 / UNITS_PER_M;
    if (c.m < 0) c.m = 0;
    c.v = course_speed(c.m);
    c.diff = c.m >= 1500 ? 256 : c.m * 256 / 1500;
    w->g.chunks++;
    int kind = pick_group(&c);
    int32_t lead = F2U(&c, 36);
    int32_t s = c.s0 + lead;
    int32_t used;
    if (kind == G_FORK) {
        /* the fork: a straight run, the split in it, the dead branch ends in a stump; no traps round it */
        int side = rnd(w, 2) ? 1 : -1;      /* the dead branch's side */
        int32_t dead = FORK_DEAD_MIN + rnd(w, FORK_DEAD_RND);
        int32_t sf = s + F2U(&c, 20);
        add(&c, OB_FORK, sf, 0, dead, side);
        acorn_line(&c, sf - F2U(&c, 60), sf + F2U(&c, 30), 0, -side * ANG(22));
        used = (sf - c.s0) + dead + 160;
        w->g.last_fork_s = sf;
        int nseg = (used + SEG_LEN - 1) / SEG_LEN;
        put_run(w, nseg, 0, 0, 0);
        flush(&c);
        return;
    }
    used = group(&c, kind, s);
    if (kind == G_OWL) w->g.last_owl_s = s;
    if (kind == G_WIND) w->g.last_wind_s = s;
    if (kind == G_RAIN) w->g.last_rain_s = s;
    if (kind == G_BEES) w->g.last_bees_s = s;
    /* a second group further on, as the difficulty rises (never two jump groups too close) */
    if (c.diff > 64 && rnd(&c.w[0], 256) < c.diff / 2) {
        int k2 = pick_group(&c);
        if (k2 != G_FORK && k2 != G_OWL && k2 != G_RAIN && k2 != G_BEES && !(k2 == G_SAP && kind == G_WIND) &&
            !(k2 == G_WIND && kind == G_SAP)) {
            int32_t s2 = s + used + F2U(&c, 30 + LAND_FRAMES);
            used = s2 - s + group(&c, k2, s2);
            if (k2 == G_WIND) w->g.last_wind_s = s2;
        }
    }
    int32_t len = lead + used + F2U(&c, REST_FRAMES - c.diff / 16);
    int nseg = (len + SEG_LEN - 1) / SEG_LEN;
    if (nseg < 12) nseg = 12;
    /* the geometry under it */
    int geo = rnd(w, 8);
    int32_t curves[3] = {CURVE_EASY, CURVE_MED, CURVE_HARD};
    int strength = rnd(w, 1 + (c.diff >= 64) + (c.diff >= 160));
    int32_t cv = curves[strength] * (rnd(w, 2) ? 1 : -1);
    int32_t dip = 0;
    if (geo == 6 || geo == 7) dip = (rnd(w, 4) == 0 ? -1 : 1) * (Q8(30) + rnd(w, DIP_MAX - Q8(30)) * (64 + c.diff) / 320);
    if (geo <= 1) put_run(w, nseg, 0, 0, dip);                                /* straight */
    else if (geo <= 4) put_run(w, nseg, cv, nseg / 4, dip);                   /* a curve */
    else if (geo == 5) {                                                     /* an S-bend */
        put_run(w, nseg / 2, cv, nseg / 8, 0);
        put_run(w, nseg - nseg / 2, -cv, nseg / 8, 0);
    } else put_run(w, nseg, geo == 7 ? cv / 2 : 0, nseg / 4, dip);           /* a dip (or a rise) */
    scenery(&c, c.s0, c.s0 + nseg * SEG_LEN);
    flush(&c);
}

void gen_init(world *w)
{
    memset(&w->g, 0, sizeof w->g);
    int32_t s0 = w->start_m * UNITS_PER_M;
    w->g.seg = (s0 - 512) / SEG_LEN;
    w->g.last_fork_s = w->g.last_owl_s = w->g.last_wind_s = w->g.last_rain_s = w->g.last_bees_s = -100000;
    rs_rng_seed(&w->g.drng, (w->seed ^ 0x5ce7e5u) | 1);
    /* the run-up: a straight with a line of acorns */
    chunk_ctx c;
    memset(&c, 0, sizeof c);
    c.w = w;
    c.v = course_speed(w->start_m);
    c.m = w->start_m;
    acorn_line(&c, s0 + 300, s0 + 700, 0, ANG(20));
    scenery(&c, s0 - 400, s0 + 800);
    flush(&c);
    put_run(w, (512 + 800) / SEG_LEN, 0, 0, 0);
}

void gen_fill(world *w, int32_t until_s)
{
    while (w->g.seg * SEG_LEN < until_s && w->ob_count <= OB_MAX - CHUNK_OBS) chunk(w);
}

void gen_trim(world *w, int32_t behind_s)
{
    while (w->ob_count > 0) {
        const obj *o = &w->ob[w->ob_first];
        int32_t end = o->s + (o->kind == OB_FORK ? o->len + FORK_TAIL : o->len) + 32;
        if (end >= behind_s) break;
        w->ob_first = (w->ob_first + 1) % OB_MAX;
        w->ob_count--;
    }
}
