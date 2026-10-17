/*
 * Ladybug Laps: the rules. No drawing, no sound, no screen width: deterministic (integer maths, the world's own RNG)
 * and testable alone (tests/test_rules.c links this file with tracks.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
 *
 * The track: tracks.c's grid of pieces, traced from the start into a centre line of waypoints (16 px apart), with a
 * racing line (toward the inside of the turns) for the bots; the surface of any pixel comes from the pieces (on the
 * band or not), the zones (jam, puddles) and the objects (bumps); solid objects and walls are collisions.
 * The ladybugs: top-down arcade cars. The d-pad's Left / Right rotate the ladybug (ROT_RATE, ramping up); the touch
 * stick (and the bots' brains) give a direction it turns toward (TURN_RATE); A the gas, B the brake (and reverse from
 * a standstill). The velocity is split into forward and sideways parts: the gas and the brake act on the first, the
 * surface's grip eats the second (a slide when it is low: the drift, the puddles). A bump at speed throws it into the
 * air (a hop); two ladybugs bump off each other.
 * The race (DESIGN.md "The race"): 3 laps; the checkpoints are the waypoints: a racer only moves on to one of the
 * next few, so a shortcut over the table gains nothing. 1 player: the camera follows the player, a fall puts it back
 * where it fell; the race ends when the player finishes (its place: the order it finished in) or when the three bots
 * have. 2-4 players (Micro Machines' way): the camera follows the leader (the racer furthest along); a racer whose
 * centre leaves the fixed 320 x 240 race zone round it is LEFT BEHIND, and one that falls off the table, OUT of the
 * race; the race ends when the leader completes the last lap, or when one racer is left. The racers out rank below
 * the others, the first out last. The places score 3, 2, 1, 0 points. A race never lasts more than RACE_LIMIT.
 */
#include "game.h"
#include <string.h>

const char *const bot_names[RACERS] = {"RUBY", "SUNNY", "BLUEBELL", "BASIL"};

/* sin(i x 90 / 256 degrees) x 16384, i = 0..256 */
const int16_t sin_q14[ANG_FULL / 4 + 1] = {
    0, 101, 201, 302, 402, 503, 603, 704, 804, 904, 1005, 1105, 1205, 1306, 1406, 1506,
    1606, 1706, 1806, 1906, 2006, 2105, 2205, 2305, 2404, 2503, 2603, 2702, 2801, 2900, 2999, 3098,
    3196, 3295, 3393, 3492, 3590, 3688, 3786, 3883, 3981, 4078, 4176, 4273, 4370, 4467, 4563, 4660,
    4756, 4852, 4948, 5044, 5139, 5235, 5330, 5425, 5520, 5614, 5708, 5803, 5897, 5990, 6084, 6177,
    6270, 6363, 6455, 6547, 6639, 6731, 6823, 6914, 7005, 7096, 7186, 7276, 7366, 7456, 7545, 7635,
    7723, 7812, 7900, 7988, 8076, 8163, 8250, 8337, 8423, 8509, 8595, 8680, 8765, 8850, 8935, 9019,
    9102, 9186, 9269, 9352, 9434, 9516, 9598, 9679, 9760, 9841, 9921, 10001, 10080, 10159, 10238, 10316,
    10394, 10471, 10549, 10625, 10702, 10778, 10853, 10928, 11003, 11077, 11151, 11224, 11297, 11370, 11442, 11514,
    11585, 11656, 11727, 11797, 11866, 11935, 12004, 12072, 12140, 12207, 12274, 12340, 12406, 12472, 12537, 12601,
    12665, 12729, 12792, 12854, 12916, 12978, 13039, 13100, 13160, 13219, 13279, 13337, 13395, 13453, 13510, 13567,
    13623, 13678, 13733, 13788, 13842, 13896, 13949, 14001, 14053, 14104, 14155, 14206, 14256, 14305, 14354, 14402,
    14449, 14497, 14543, 14589, 14635, 14680, 14724, 14768, 14811, 14854, 14896, 14937, 14978, 15019, 15059, 15098,
    15137, 15175, 15213, 15250, 15286, 15322, 15357, 15392, 15426, 15460, 15493, 15525, 15557, 15588, 15619, 15649,
    15679, 15707, 15736, 15763, 15791, 15817, 15843, 15868, 15893, 15917, 15941, 15964, 15986, 16008, 16029, 16049,
    16069, 16088, 16107, 16125, 16143, 16160, 16176, 16192, 16207, 16221, 16235, 16248, 16261, 16273, 16284, 16295,
    16305, 16315, 16324, 16332, 16340, 16347, 16353, 16359, 16364, 16369, 16373, 16376, 16379, 16381, 16383, 16384,
    16384};

static const int16_t surf_table[SF_COUNT][4] = SURF_TABLE;
static const int16_t bot_table[RACERS][6] = BOT_TABLE;
static const int finish_points[RACERS] = FINISH_POINTS;

int race_elimination(const world *w) { return w->humans >= 2; }
int race_focus(const world *w) { return w->humans == 1 ? 0 : w->leader; }

/* ---- integer maths -------------------------------------------------------------------------------------------------- */
int isin(int a)
{
    a &= ANG_FULL - 1;
    if (a < 256) return sin_q14[a];
    if (a < 512) return sin_q14[512 - a];
    if (a < 768) return -sin_q14[a - 512];
    return -sin_q14[1024 - a];
}

int icos(int a) { return isin(a + 256); }

int ang_diff(int a, int b)
{
    int d = (b - a) & (ANG_FULL - 1);
    return d >= 512 ? d - 1024 : d;
}

/* the angle of (dx, dy) in 1/1024 of a turn (y down: 256 is south) */
int iatan2(int32_t dy, int32_t dx)
{
    if (!dx && !dy) return 0;
    int64_t ax = dx < 0 ? -(int64_t)dx : dx, ay = dy < 0 ? -(int64_t)dy : dy;
    int swap = ay > ax;
    int64_t mx = swap ? ay : ax, mn = swap ? ax : ay;
    int lo = 0, hi = 128;                       /* the smallest a with tan(a) >= mn / mx */
    while (lo < hi) {
        int m = (lo + hi) / 2;
        if ((int64_t)sin_q14[m] * mx >= (int64_t)sin_q14[256 - m] * mn) hi = m;
        else lo = m + 1;
    }
    int a = swap ? 256 - lo : lo;               /* 0..256 in the first quadrant */
    if (dx < 0) a = 512 - a;
    if (dy < 0) a = -a;
    return a & (ANG_FULL - 1);
}

int isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    int64_t r = 0, b = (int64_t)1 << 62;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return (int)r;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int iabs(int v) { return v < 0 ? -v : v; }

/* ---- the track ----------------------------------------------------------------------------------------------------- */
static const int DX4[4] = {1, 0, -1, 0}, DY4[4] = {0, 1, 0, -1};

/* the two edges (0 E, 1 S, 2 W, 3 N) a piece joins; -1: not a piece */
static int piece_edges(int ch, int *e1, int *e2)
{
    switch (ch) {
    case '-': *e1 = 0; *e2 = 2; return 1;
    case '|': *e1 = 1; *e2 = 3; return 1;
    case 'r': case 'A': *e1 = 0; *e2 = 1; return 1;
    case '7': case 'B': *e1 = 2; *e2 = 1; return 1;
    case 'L': case 'C': *e1 = 3; *e2 = 0; return 1;
    case 'J': case 'D': *e1 = 3; *e2 = 2; return 1;
    }
    return 0;
}

/* a big corner's cell for an edge: A: E (1,0), S (0,1); B: W (0,0), S (1,1); C: N (0,0), E (1,1); D: N (1,0), W (0,1) */
static void big_cell(int t, int e, int *cx, int *cy)
{
    static const int8_t tab[4][4][2] = {
        /* A */ {{1, 0}, {0, 1}, {0, 0}, {0, 0}},
        /* B */ {{0, 0}, {1, 1}, {0, 0}, {0, 0}},
        /* C */ {{1, 1}, {0, 0}, {0, 0}, {0, 0}},
        /* D */ {{0, 0}, {0, 0}, {0, 1}, {1, 0}}};
    *cx = tab[t - 'A'][e][0];
    *cy = tab[t - 'A'][e][1];
}

static int gridc(const world *w, int x, int y)
{
    if (x < 0 || y < 0 || x >= tracks[w->track].gw || y >= tracks[w->track].gh) return '.';
    return w->grid[y][x];
}

/* the top-left cell of the big corner covering cell (x, y), its letter; 0: none */
static int block_of(const world *w, int x, int y, int *ox, int *oy)
{
    static const int8_t off[4][2] = {{0, 0}, {-1, 0}, {0, -1}, {-1, -1}};
    for (int k = 0; k < 4; k++) {
        int bx = x + off[k][0], by = y + off[k][1], t = gridc(w, bx, by);
        if (t < 'A' || t > 'D') continue;
        if (gridc(w, bx + 1, by) == '#' && gridc(w, bx, by + 1) == '#' && gridc(w, bx + 1, by + 1) == '#') {
            *ox = bx;
            *oy = by;
            return t;
        }
    }
    return 0;
}

/* the middle of a cell's edge, px */
static void edge_point(int cx, int cy, int e, int *x, int *y)
{
    *x = cx * CELL + (e == 0 ? CELL : e == 2 ? 0 : CELL / 2);
    *y = cy * CELL + (e == 1 ? CELL : e == 3 ? 0 : CELL / 2);
}

static int add_wp(waypoints *p, int x, int y)
{
    if (p->n >= MAX_WP) return 0;
    p->x[p->n] = (int16_t)x;
    p->y[p->n] = (int16_t)y;
    p->n++;
    return 1;
}

/* the arc from (x0, y0) round (cx, cy), a quarter turn, in n steps (the first point included, not the last) */
static int add_arc(waypoints *p, int cx, int cy, int x0, int y0, int x1, int y1, int n)
{
    int r = iabs(x0 - cx) + iabs(y0 - cy);      /* axis-aligned from the centre */
    int a0 = iatan2(y0 - cy, x0 - cx), a1 = iatan2(y1 - cy, x1 - cx), d = ang_diff(a0, a1);
    for (int k = 0; k < n; k++) {
        int a = a0 + d * k / n;
        if (!add_wp(p, cx + ((r * icos(a) + 8192) >> 14), cy + ((r * isin(a) + 8192) >> 14))) return 0;
    }
    return 1;
}

int track_build(world *w, int track, int reverse)
{
    const track_def *t = &tracks[track];
    waypoints *p = &w->wp;
    memset(p, 0, sizeof *p);
    memset(w->grid, '.', sizeof w->grid);
    w->track = track;
    w->reverse = reverse;
    if (t->gw > MAX_GW || t->gh > MAX_GH) return 0;
    for (int y = 0; y < t->gh; y++)
        for (int x = 0; x < t->gw; x++) w->grid[y][x] = (uint8_t)t->grid[y][x];
    w->wpx = t->gw * CELL;
    w->wpy = t->gh * CELL;
    int x = t->sx, y = t->sy, d = t->sdir, steps = 0;
    do {
        int ch = gridc(w, x, y), ein = (d + 2) & 3, e1, e2, px, py, qx, qy;
        if (ch == '+') ch = (d & 1) ? '|' : '-';
        if (ch == '#') {
            int ox, oy;
            ch = block_of(w, x, y, &ox, &oy);
            if (!ch) return 0;
        }
        if (!piece_edges(ch, &e1, &e2)) return 0;
        if (ein != e1 && ein != e2) return 0;
        int eout = ein == e1 ? e2 : e1;
        if (ch >= 'A' && ch <= 'D') {
            int ox, oy, ix, iy, xx, xy;
            if (block_of(w, x, y, &ox, &oy) != ch) return 0;
            big_cell(ch, ein, &ix, &iy);
            big_cell(ch, eout, &xx, &xy);
            if (ox + ix != x || oy + iy != y) return 0;
            edge_point(x, y, ein, &px, &py);
            edge_point(ox + xx, oy + xy, eout, &qx, &qy);
            int ccx = (ox + (ch == 'A' || ch == 'C' ? 2 : 0)) * CELL, ccy = (oy + (ch == 'A' || ch == 'B' ? 2 : 0)) * CELL;
            if (!add_arc(p, ccx, ccy, px, py, qx, qy, 9)) return 0;
            x = ox + xx + DX4[eout];
            y = oy + xy + DY4[eout];
        } else if (ch == '-' || ch == '|') {
            edge_point(x, y, ein, &px, &py);
            for (int k = 0; k < CELL / WP_STEP; k++)
                if (!add_wp(p, px + DX4[d] * WP_STEP * k, py + DY4[d] * WP_STEP * k)) return 0;
            x += DX4[d];
            y += DY4[d];
        } else {
            edge_point(x, y, ein, &px, &py);
            edge_point(x, y, eout, &qx, &qy);
            int ccx = x * CELL + (ch == 'r' || ch == 'L' ? CELL : 0), ccy = y * CELL + (ch == 'r' || ch == '7' ? CELL : 0);
            if (!add_arc(p, ccx, ccy, px, py, qx, qy, 3)) return 0;
            x += DX4[eout];
            y += DY4[eout];
        }
        d = eout;
        if (++steps > MAX_GW * MAX_GH * 2) return 0;
    } while (x != t->sx || y != t->sy || d != t->sdir);
    int n = p->n;
    if (reverse) {                              /* the same line, the other way: waypoint 0 stays on the start line */
        for (int i = 1, j = n - 1; i < j; i++, j--) {
            int16_t tx = p->x[i], ty = p->y[i];
            p->x[i] = p->x[j];
            p->y[i] = p->y[j];
            p->x[j] = tx;
            p->y[j] = ty;
        }
    }
    int32_t cum = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n, dx = p->x[j] - p->x[i], dy = p->y[j] - p->y[i];
        p->dir[i] = (int16_t)iatan2(dy, dx);
        p->cum[i] = cum;
        cum += isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
    }
    p->len = cum;
    /* the racing line: toward the inside of the turn ahead (+: right), smoothed */
    int16_t raw[MAX_WP];
    for (int i = 0; i < n; i++) {
        int turn = ang_diff(p->dir[(i + n - 2) % n], p->dir[(i + 3) % n]);
        raw[i] = (int16_t)clampi(turn * RL_K / 16, -RL_MAX, RL_MAX);
    }
    for (int i = 0; i < n; i++)
        p->rl[i] = (int16_t)((raw[(i + n - 1) % n] + 2 * raw[i] + raw[(i + 1) % n]) / 4);
    return 1;
}

/* on the band of the pieces? */
static int on_band(const world *w, int x, int y)
{
    if (x < 0 || y < 0) return 0;
    int cx = x / CELL, cy = y / CELL, lx = x - cx * CELL, ly = y - cy * CELL, ch = gridc(w, cx, cy);
    int64_t lo, hi, dx, dy;
    switch (ch) {
    case '-': return iabs(ly - CELL / 2) <= HALF_W;
    case '|': return iabs(lx - CELL / 2) <= HALF_W;
    case '+': return iabs(ly - CELL / 2) <= HALF_W || iabs(lx - CELL / 2) <= HALF_W;
    case 'r': case '7': case 'L': case 'J':
        dx = lx - (ch == 'r' || ch == 'L' ? CELL : 0);
        dy = ly - (ch == 'r' || ch == '7' ? CELL : 0);
        lo = CELL / 2 - HALF_W;
        hi = CELL / 2 + HALF_W;
        break;
    case 'A': case 'B': case 'C': case 'D': case '#': {
        int ox, oy, t = block_of(w, cx, cy, &ox, &oy);
        if (!t) return 0;
        dx = x - (ox + (t == 'A' || t == 'C' ? 2 : 0)) * CELL;
        dy = y - (oy + (t == 'A' || t == 'B' ? 2 : 0)) * CELL;
        lo = CELL * 3 / 2 - HALF_W;
        hi = CELL * 3 / 2 + HALF_W;
        break;
    }
    default: return 0;
    }
    int64_t d2 = dx * dx + dy * dy;
    return d2 >= lo * lo && d2 <= hi * hi;
}

static int in_zone(const tzone *z, int x, int y)
{
    int64_t dx = x - z->x, dy = y - z->y, rx = z->rx, ry = z->ry;
    if (z->shape == ZS_RECT) return iabs((int)dx) <= rx && iabs((int)dy) <= ry;
    return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
}

int surface_at(const world *w, int x, int y)
{
    const track_def *t = &tracks[w->track];
    if (t->table) {
        if (x < t->tx0 || y < t->ty0 || x >= t->tx1 || y >= t->ty1) return SF_FALL;
    } else if (x < WALL_IN || y < WALL_IN || x >= w->wpx - WALL_IN || y >= w->wpy - WALL_IN) {
        return SF_WALL;
    }
    for (int i = t->nzone - 1; i >= 0; i--)
        if (in_zone(&t->zone[i], x, y)) return t->zone[i].surf;
    for (int i = 0; i < t->nobj; i++) {
        const tobj *o = &t->obj[i];
        const obj_kind *k = &obj_kinds[o->kind];
        if (k->effect == OE_BUMP && iabs(x - o->x) * 2 <= k->w && iabs(y - o->y) * 2 <= k->h) return SF_BUMP;
    }
    return on_band(w, x, y) ? SF_TRACK : SF_ROUGH;
}

/* ---- the cars ------------------------------------------------------------------------------------------------------ */
int car_speed(const car *c)
{
    return (int)(((int64_t)c->vx * icos(c->ang) + (int64_t)c->vy * isin(c->ang)) >> 14);
}

static void set_velocity(car *c, int32_t f, int32_t l)
{
    int cs = icos(c->ang), sn = isin(c->ang);
    c->vx = (int32_t)(((int64_t)f * cs - (int64_t)l * sn) >> 14);
    c->vy = (int32_t)(((int64_t)f * sn + (int64_t)l * cs) >> 14);
}

/* the waypoint nearest to (x, y) px among wp + from .. wp + to; its squared distance */
static int nearest_wp(const waypoints *p, int x, int y, int wp, int from, int to, int64_t *d2out)
{
    int best = wp;
    int64_t bd = -1;
    for (int k = from; k <= to; k++) {
        int i = ((wp + k) % p->n + p->n) % p->n;
        int64_t dx = x - p->x[i], dy = y - p->y[i], d2 = dx * dx + dy * dy;
        if (bd < 0 || d2 < bd) { bd = d2; best = i; }
    }
    if (d2out) *d2out = bd;
    return best;
}

static int32_t progress_of(const world *w, const car *c)
{
    const waypoints *p = &w->wp;
    int i = c->wp;
    int32_t px = c->x / FX - p->x[i], py = c->y / FX - p->y[i];
    int32_t proj = (px * icos(p->dir[i]) + py * isin(p->dir[i])) >> 14;
    proj = clampi(proj, -WP_STEP, 2 * WP_STEP);
    return c->lap * p->len + p->cum[i] + proj;
}

/* the checkpoints: a racer only moves on to one of the next few waypoints (or back a little) */
static void update_wp(world *w, car *c)
{
    const waypoints *p = &w->wp;
    int64_t d2;
    int old = c->wp, k = nearest_wp(p, c->x / FX, c->y / FX, old, -2, 5, &d2);
    int snap = HALF_W + 32;
    if (d2 <= (int64_t)snap * snap) {
        if (old >= p->n - 8 && k < 8) c->lap++;
        else if (old < 8 && k >= p->n - 8) c->lap--;
        c->wp = (int16_t)k;
    }
    c->prog = progress_of(w, c);
}

/* waypoint i (in range), lane px to the right of the centre line: the spot (Q8) */
static void lane_spot(const waypoints *p, int i, int lane, int32_t *x, int32_t *y)
{
    int a = p->dir[i];
    *x = (p->x[i] * FX) + (int32_t)(((int64_t)-isin(a) * lane * FX) >> 14);
    *y = (p->y[i] * FX) + (int32_t)(((int64_t)icos(a) * lane * FX) >> 14);
}

/* a ladybug there (Q8) would be inside a solid object (a sugar cube or a snail on the band) */
static int in_solid(const world *w, int32_t x, int32_t y)
{
    const track_def *t = &tracks[w->track];
    for (int i = 0; i < t->nobj; i++) {
        const tobj *o = &t->obj[i];
        const obj_kind *k = &obj_kinds[o->kind];
        if (k->effect != OE_SOLID) continue;
        int32_t dx = x - o->x * FX, dy = y - o->y * FX;
        if (k->shape == SH_CIRCLE) {
            int64_t r = (int64_t)(k->w + CAR_R + 1) * FX;
            if ((int64_t)dx * dx + (int64_t)dy * dy < r * r) return 1;
        } else if (iabs(dx) < (k->w / 2 + CAR_R + 1) * FX && iabs(dy) < (k->h / 2 + CAR_R + 1) * FX) {
            return 1;
        }
    }
    return 0;
}

/* put car c on the centre line at waypoint i, lane px to the right, heading along it, at speed f; a lane blocked by
 * an object: the nearest free one of the four racers' lanes */
static void place_at(world *w, car *c, int i, int lane, int32_t f)
{
    static const int lanes[RACERS] = {-13, -4, 5, 14};     /* colour x 9 - 13 */
    const waypoints *p = &w->wp;
    i = (i % p->n + p->n) % p->n;
    int a = p->dir[i];
    lane_spot(p, i, lane, &c->x, &c->y);
    if (in_solid(w, c->x, c->y)) {
        int best = -1;
        int32_t x, y;
        for (int k = 0; k < RACERS; k++) {
            lane_spot(p, i, lanes[k], &x, &y);
            if (!in_solid(w, x, y) && (best < 0 || iabs(lanes[k] - lane) < iabs(lanes[best] - lane))) best = k;
        }
        if (best >= 0) lane_spot(p, i, lanes[best], &c->x, &c->y);
    }
    c->ang = (int16_t)a;
    c->z = c->vz = 0;
    c->wp = (int16_t)i;
    set_velocity(c, f, 0);
}

static void bot_params(world *w, car *c, int i)
{
    const int16_t *b = bot_table[c->colour];
    int lv = w->level;
    if (c->human) {                             /* a player: full speed; its brain (bot=1) the player bot's */
        c->speed_pct = 100;
        c->corner_pct = PBOT_CORNER;
        c->line_err = 3;
        c->react = 6;
        c->wide = 0;
        c->lane = 0;
        return;
    }
    c->speed_pct = (int16_t)(b[0] - DIFF_SPEED0 + lv * DIFF_SPEED_STEP / 2);
    if (c->speed_pct > b[0] + DIFF_SPEED_MAX) c->speed_pct = (int16_t)(b[0] + DIFF_SPEED_MAX);
    c->corner_pct = b[1];
    c->line_err = (int16_t)(b[2] * (DIFF_ERR0 - (lv < DIFF_ERR0 ? lv : DIFF_ERR0)) / DIFF_ERR0 + b[2] / 2);
    c->wide = b[3];
    c->react = b[4];
    c->lane = b[5];
    (void)i;
}

void world_init(world *w, int track, int reverse, int level, int humans, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed);
    w->level = level;
    w->humans = humans;
    if (!track_build(w, track, reverse)) {      /* never with the shipped tracks (the test builds them all) */
        w->wp.n = 1;
        w->wp.len = 1;
    }
    const waypoints *p = &w->wp;
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[i];
        c->colour = (uint8_t)i;
        c->human = (uint8_t)(i < humans ? i + 1 : 0);
        bot_params(w, c, i);
        int slot = (int)((i + seed) % RACERS), row = slot / 2;
        int back = GRID_BACK + row * GRID_ROW, lane = (slot & 1) ? GRID_LANE : -GRID_LANE;
        int a = p->dir[0];
        c->ang = (int16_t)a;
        c->x = (p->x[0] - ((back * icos(a)) >> 14) - ((lane * isin(a)) >> 14)) * FX;
        c->y = (p->y[0] - ((back * isin(a)) >> 14) + ((lane * icos(a)) >> 14)) * FX;
        int64_t d2;
        c->wp = (int16_t)nearest_wp(p, c->x / FX, c->y / FX, 0, -p->n / 2, p->n / 2, &d2);
        c->lap = (int16_t)(c->wp > p->n / 2 ? -1 : 0);
        c->prog = progress_of(w, c);
        c->surf = SF_TRACK;
        c->place = (int16_t)(i + 1);
    }
    w->leader = 0;
    for (int i = 1; i < RACERS; i++)
        if (w->c[i].prog > w->c[w->leader].prog) w->leader = i;
    w->camx = w->c[w->leader].x;
    w->camy = w->c[w->leader].y;
    w->phase = PH_COUNT;
    for (int i = 0; i < RACERS; i++) w->order[i] = i;
}

/* ---- collisions ------------------------------------------------------------------------------------------------------- */
/* push car c out of a circle (px, Q8 centre) of radius r px; returns 1 on a hit (bounces) */
static int hit_circle(car *c, int32_t ox, int32_t oy, int r)
{
    int32_t dx = c->x - ox, dy = c->y - oy, rr = (r + CAR_R) * FX;
    int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
    if (d2 >= (int64_t)rr * rr) return 0;
    int32_t d = isqrt64(d2);
    if (d < 1) { dx = FX; dy = 0; d = FX; }
    int32_t nx = (int32_t)((int64_t)dx * 16384 / d), ny = (int32_t)((int64_t)dy * 16384 / d);
    c->x = ox + (int32_t)(((int64_t)nx * rr) >> 14);
    c->y = oy + (int32_t)(((int64_t)ny * rr) >> 14);
    int32_t vn = (int32_t)(((int64_t)c->vx * nx + (int64_t)c->vy * ny) >> 14);
    if (vn < 0) {
        int32_t j = vn + (vn * WALL_E >> 8);
        c->vx -= (int32_t)(((int64_t)j * nx) >> 14);
        c->vy -= (int32_t)(((int64_t)j * ny) >> 14);
    }
    return vn < -BUMP_SFX_MIN ? 2 : 1;
}

/* a box: half sizes hw, hh px round (ox, oy) Q8: the car (a point) out of the box grown by its radius, by the side
 * it went in the least; the bounce on that side */
static int hit_box(car *c, int32_t ox, int32_t oy, int hw, int hh)
{
    int32_t ex = (hw + CAR_R) * FX, ey = (hh + CAR_R) * FX, dx = c->x - ox, dy = c->y - oy, vn;
    if (dx <= -ex || dx >= ex || dy <= -ey || dy >= ey) return 0;
    if (ex - iabs(dx) < ey - iabs(dy)) {
        int s = dx < 0 ? -1 : 1;
        c->x = ox + s * ex;
        vn = c->vx * s;
        if (vn < 0) c->vx -= s * (vn + (vn * WALL_E >> 8));
    } else {
        int s = dy < 0 ? -1 : 1;
        c->y = oy + s * ey;
        vn = c->vy * s;
        if (vn < 0) c->vy -= s * (vn + (vn * WALL_E >> 8));
    }
    return vn < -BUMP_SFX_MIN ? 2 : 1;
}

static int collide_world(world *w, car *c)
{
    const track_def *t = &tracks[w->track];
    int hit = 0;
    for (int i = 0; i < t->nobj; i++) {
        const tobj *o = &t->obj[i];
        const obj_kind *k = &obj_kinds[o->kind];
        if (k->effect != OE_SOLID) continue;
        int h = k->shape == SH_CIRCLE ? hit_circle(c, o->x * FX, o->y * FX, k->w)
                                      : hit_box(c, o->x * FX, o->y * FX, k->w / 2, k->h / 2);
        if (h > hit) hit = h;
    }
    if (!t->table) {                            /* the walls of a garden track */
        int32_t lo = (WALL_IN + CAR_R) * FX, hx = (w->wpx - WALL_IN - CAR_R) * FX, hy = (w->wpy - WALL_IN - CAR_R) * FX;
        int32_t v0x = c->vx, v0y = c->vy;
        if (c->x < lo) { c->x = lo; if (c->vx < 0) c->vx = -c->vx * WALL_E >> 8; }
        if (c->x > hx) { c->x = hx; if (c->vx > 0) c->vx = -c->vx * WALL_E >> 8; }
        if (c->y < lo) { c->y = lo; if (c->vy < 0) c->vy = -c->vy * WALL_E >> 8; }
        if (c->y > hy) { c->y = hy; if (c->vy > 0) c->vy = -c->vy * WALL_E >> 8; }
        if (v0x != c->vx || v0y != c->vy) {
            int h = (iabs(v0x - c->vx) > 2 * BUMP_SFX_MIN || iabs(v0y - c->vy) > 2 * BUMP_SFX_MIN) ? 2 : 1;
            if (h > hit) hit = h;
        }
    }
    return hit;
}

static int solid(const car *c) { return c->state == CS_RACE && !c->ghost; }

static void collide_cars(world *w)
{
    for (int i = 0; i < RACERS; i++)
        for (int j = i + 1; j < RACERS; j++) {
            car *a = &w->c[i], *b = &w->c[j];
            if (!solid(a) || !solid(b) || (a->z > 0) != (b->z > 0)) continue;
            int32_t dx = b->x - a->x, dy = b->y - a->y, rr = 2 * CAR_R * FX;
            int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
            if (d2 >= (int64_t)rr * rr) continue;
            int32_t d = isqrt64(d2);
            if (d < 1) { dx = FX; dy = (i + j) & 1 ? FX : -FX; d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy); }
            int32_t nx = (int32_t)((int64_t)dx * 16384 / d), ny = (int32_t)((int64_t)dy * 16384 / d), push = (rr - d) / 2 + 1;
            a->x -= (int32_t)(((int64_t)nx * push) >> 14);
            a->y -= (int32_t)(((int64_t)ny * push) >> 14);
            b->x += (int32_t)(((int64_t)nx * push) >> 14);
            b->y += (int32_t)(((int64_t)ny * push) >> 14);
            int32_t vn = (int32_t)(((int64_t)(a->vx - b->vx) * nx + (int64_t)(a->vy - b->vy) * ny) >> 14);
            if (vn > 0) {
                int32_t jn = (vn + (vn * BUMP_E >> 8)) / 2;
                a->vx -= (int32_t)(((int64_t)jn * nx) >> 14);
                a->vy -= (int32_t)(((int64_t)jn * ny) >> 14);
                b->vx += (int32_t)(((int64_t)jn * nx) >> 14);
                b->vy += (int32_t)(((int64_t)jn * ny) >> 14);
                if (vn > BUMP_SFX_MIN) {
                    w->events[i] |= EV_BUMP;
                    w->events[j] |= EV_BUMP;
                }
            }
        }
}

/* ---- one car's step ------------------------------------------------------------------------------------------------- */
static void steer(car *c, const pin *in)
{
    int f = iabs(car_speed(c)), fa = f < TURN_FULL_AT ? f : TURN_FULL_AT;
    if (in->turn) {                             /* the pad: rotate (the same way when reversing) */
        int dir = in->turn > 0 ? 1 : -1, full = ROT_STILL + (ROT_RATE - ROT_STILL) * fa / TURN_FULL_AT;
        int held = c->steer * dir;              /* the last frame's turn, this way: the press ramps up */
        int rate = held > 0 ? held + ROT_RAMP : ROT_START;
        if (rate > full) rate = full;
        c->ang = (int16_t)((c->ang + dir * rate) & (ANG_FULL - 1));
        c->steer = (int16_t)(dir * rate);
        return;
    }
    c->steer = 0;
    if (!in->dx && !in->dy) return;
    int target = iatan2(in->dy, in->dx), d = ang_diff(c->ang, target);
    int rate = TURN_STILL + (TURN_RATE - TURN_STILL) * fa / TURN_FULL_AT;
    int s = d > rate ? rate : d < -rate ? -rate : d;
    c->ang = (int16_t)((c->ang + s) & (ANG_FULL - 1));
    c->steer = (int16_t)s;
}

static void drive(world *w, int i, const pin *in)
{
    car *c = &w->c[i];
    int ev = 0;
    if (c->z > 0 || c->vz > 0) {                /* in the air: flies straight */
        c->x += c->vx;
        c->y += c->vy;
        c->z += c->vz;
        c->vz -= HOP_GRAVITY;
        if (c->z <= 0) {
            c->z = c->vz = 0;
            ev |= EV_LAND;
        }
        c->gas = c->brake = 0;
    } else {
        const int16_t *s = surf_table[c->surf];
        int32_t top = s[0] * c->speed_pct / 100;
        steer(c, in);
        int32_t f = car_speed(c);
        int32_t l = (int32_t)(((int64_t)-c->vx * isin(c->ang) + (int64_t)c->vy * icos(c->ang)) >> 14);
        c->gas = in->gas;
        c->brake = in->brake;
        if (in->brake) {
            if (f > 0) f = f > BRAKE ? f - BRAKE : 0;
            else if (f > -REV_MAX) f -= REV_ACCEL;
        } else if (in->gas) {
            if (f < top) {
                f += ACCEL * s[1] / 100;
                if (f > top) f = top;
            }
        } else {
            f = f > COAST ? f - COAST : f < -COAST ? f + COAST : 0;
        }
        if (f > top) f -= (f - top) / OVERSPEED_DIV + 1;
        f -= f * s[3] / 256;
        l -= l * s[2] / 256;
        set_velocity(c, f, l);
        c->x += c->vx;
        c->y += c->vy;
    }
    int h = collide_world(w, c);
    if (h == 2) ev |= EV_WALL;
    int sf = surface_at(w, c->x / FX, c->y / FX);
    if (c->z == 0) {
        if (sf == SF_FALL) {
            c->state = CS_FALL;
            c->timer = FALL_FRAMES;
            c->falls++;
            ev |= EV_FALL;
        } else if (sf == SF_BUMP && car_speed(c) > HOP_MIN_SPEED && !(ev & EV_LAND)) {
            c->vz = HOP_VZ + car_speed(c) / HOP_VZ_DIV;
            c->z = 1;
            ev |= EV_HOP;
        } else {
            if (sf == SF_WATER && c->surf != SF_WATER) ev |= EV_SPLASH;
            if (sf == SF_JAM && c->surf != SF_JAM) ev |= EV_JAM;
        }
        c->surf = (uint8_t)(sf == SF_BUMP || sf == SF_FALL ? SF_TRACK : sf);
    }
    w->events[i] |= ev;
}

/* back on the track after a fall: at its own waypoint (the racing line), still */
static void respawn_here(world *w, car *c)
{
    int lane = (c->colour * 9) - 13;
    place_at(w, c, c->wp, lane, 0);
    c->state = CS_RACE;
    c->ghost = GHOST_FRAMES;
    c->surf = SF_TRACK;
}

/* 2-4 players: out of the race (left behind, or fallen off: it ends its fall first) */
static void eliminate(world *w, int i)
{
    car *c = &w->c[i];
    c->out = (int16_t)++w->eliminated;
    if (c->state == CS_RACE) {
        c->state = CS_OUT;
        c->drops++;
        c->z = c->vz = 0;
        w->events[i] |= EV_DROP;
    }
    c->warn = 0;
    w->events[i] |= EV_OUT;
}

int racer_out_of_zone(const world *w, int32_t x, int32_t y)
{
    int32_t dx = (x - w->camx) / FX, dy = (y - w->camy) / FX;
    return iabs(dx) > ZONE_HW + ZONE_OUT || iabs(dy) > ZONE_HH + ZONE_OUT;
}

static void camera_step(world *w, int snap)
{
    const car *L = &w->c[race_focus(w)];
    int32_t lx = L->vx * LOOK_FRAMES, ly = L->vy * LOOK_FRAMES;
    int32_t lm = LOOK_MAX * FX;
    int64_t ll = (int64_t)lx * lx + (int64_t)ly * ly;
    if (ll > (int64_t)lm * lm) {
        int32_t d = isqrt64(ll);
        lx = (int32_t)((int64_t)lx * lm / d);
        ly = (int32_t)((int64_t)ly * lm / d);
    }
    int32_t tx = L->x + lx, ty = L->y + ly;
    if (snap) {
        w->camx = tx;
        w->camy = ty;
    } else {
        int32_t mx = (tx - w->camx) / CAM_EASE, my = (ty - w->camy) / CAM_EASE, mm = CAM_MAX_STEP * FX;
        w->camx += clampi(mx, -mm, mm);
        w->camy += clampi(my, -mm, mm);
    }
    w->camx = clampi(w->camx, ZONE_HW * FX, (w->wpx - ZONE_HW) * FX);
    w->camy = clampi(w->camy, ZONE_HH * FX, (w->wpy - ZONE_HH) * FX);
}

/* the places: the finishers in the order they finished, then the racers still in by how far along they are, then the
 * racers out, the last one out first */
static int64_t rank_key(const car *c)
{
    if (c->fin) return ((int64_t)1 << 40) - c->fin;
    if (c->out) return -((int64_t)1 << 40) + c->out;
    return c->prog;
}

static void rank_places(world *w)
{
    int ord[RACERS];
    for (int i = 0; i < RACERS; i++) ord[i] = i;
    for (int i = 1; i < RACERS; i++)
        for (int j = i; j > 0 && rank_key(&w->c[ord[j]]) > rank_key(&w->c[ord[j - 1]]); j--) {
            int t = ord[j];
            ord[j] = ord[j - 1];
            ord[j - 1] = t;
        }
    for (int i = 0; i < RACERS; i++) {
        w->order[i] = ord[i];
        w->c[ord[i]].place = (int16_t)(i + 1);
    }
}

/* the race is over: the places as they stand score 3, 2, 1, 0 */
static void finish_race(world *w)
{
    rank_places(w);
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[w->order[i]];
        c->points = (int16_t)(c->points + finish_points[i]);
        w->final_place[w->order[i]] = i + 1;
    }
    for (int i = 0; i < RACERS; i++) w->events[i] |= EV_FINISH;
    w->phase = PH_FINISH;
    w->phase_t = 0;
}

/* is the race over? (the player, or the leader, finished; one racer left; the three bots finished; the time) */
static int race_over(const world *w)
{
    if (w->phase_t >= RACE_LIMIT) return 1;
    if (race_elimination(w)) return w->finished > 0 || RACERS - w->eliminated <= 1;
    if (w->humans == 1) return w->c[0].fin || w->finished >= RACERS - 1;
    return w->finished > 0;                     /* the title's race of bots */
}

void world_step(world *w, const pin in[RACERS])
{
    memset(w->events, 0, sizeof w->events);
    w->t++;
    w->phase_t++;
    pin cmd[RACERS];
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[i];
        if (c->human && in) cmd[i] = in[i];
        else bot_think(w, i, &cmd[i]);
        if (c->ghost > 0) c->ghost--;
    }
    if (w->phase == PH_COUNT) {
        if (w->phase_t % (COUNTDOWN / 3) == 1 && w->phase_t < COUNTDOWN) w->events[0] |= EV_BEEP;
        camera_step(w, w->t <= 1);
        if (w->phase_t >= COUNTDOWN) {
            w->phase = PH_RACE;
            w->phase_t = 0;
            w->events[0] |= EV_GO;
        }
        return;
    }
    int elim = race_elimination(w) && w->phase == PH_RACE;
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[i];
        if (c->state == CS_RACE) {
            drive(w, i, &cmd[i]);
            if (elim && c->state == CS_FALL) eliminate(w, i);     /* 2-4 players: off the table is out */
        } else if (c->state == CS_FALL && --c->timer <= 0) {
            if (c->out) {
                c->state = CS_OUT;              /* the end of its fall: gone */
            } else {
                respawn_here(w, c);
                w->events[i] |= EV_BACK;
            }
        }
    }
    collide_cars(w);
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[i];
        if (c->state == CS_OUT) continue;
        int lap0 = c->lap;
        update_wp(w, c);
        if (c->lap > lap0 && c->lap >= 1 && w->phase == PH_RACE) w->events[i] |= EV_LAP;
        if (w->phase == PH_RACE && !c->fin && !c->out && c->lap >= LAPS) c->fin = (int16_t)++w->finished;
    }
    /* the leader: furthest along (of the racers still in) */
    int lead = -1;
    for (int i = 0; i < RACERS; i++) {
        const car *c = &w->c[i];
        if (c->state == CS_OUT || c->out) continue;
        if (lead < 0 || c->prog > w->c[lead].prog) lead = i;
    }
    if (lead >= 0) w->leader = lead;
    camera_step(w, 0);
    if (w->phase <= PH_RACE) rank_places(w);    /* after the race: the final places stay */
    for (int i = 0; i < RACERS; i++) {
        car *c = &w->c[i];
        int32_t dx = iabs((c->x - w->camx) / FX), dy = iabs((c->y - w->camy) / FX);
        c->warn = (uint8_t)(elim && c->state == CS_RACE && i != w->leader &&
                            (dx > ZONE_HW - WARN_PX || dy > ZONE_HH - WARN_PX));
    }
    if (w->phase == PH_RACE) {
        if (elim && w->phase_t > DROP_GRACE)    /* 2-4 players: left behind, out of the race */
            for (int i = 0; i < RACERS; i++) {
                car *c = &w->c[i];
                if (i == w->leader || c->state != CS_RACE || !racer_out_of_zone(w, c->x, c->y)) continue;
                eliminate(w, i);
            }
        if (race_over(w)) finish_race(w);
    } else if (w->phase == PH_FINISH && w->phase_t >= FINISH_FRAMES) {
        w->phase = PH_DONE;
    }
}

/* ---- the bots -------------------------------------------------------------------------------------------------------- */
void bot_think(world *w, int i, pin *out)
{
    car *c = &w->c[i];
    const waypoints *p = &w->wp;
    memset(out, 0, sizeof *out);
    if (c->state != CS_RACE || p->n < 4) return;
    int n = p->n;
    int f = car_speed(c);
    /* its settings; the player bot's get worse with botskill (a human-like driver, to tune the medals) */
    int sk = c->human ? w->skill : 0;
    int top = 704 * (c->human ? PBOT_SPEED - sk : c->speed_pct) / 100;
    int corner = c->corner_pct - 3 * sk, line_err = c->line_err + 2 * sk, react = c->react + 2 * sk;
    /* the error of its line: a new one every `react` frames */
    if (--c->err_t <= 0) {
        c->err_t = (int16_t)(react + rs_rng_range(&w->rng, react + 1));
        c->err = (int16_t)(line_err ? rs_rng_range(&w->rng, 2 * line_err + 1) - line_err : 0);
    }
    int side = iabs(c->err) >= 2 * HALF_W;      /* getting round an obstacle (below): line up on the other side first */
    int look = side ? 1 : BOT_LOOK_WP + (f > 0 ? f / BOT_LOOK_SPEED : 0);
    int k = (c->wp + look) % n;
    int rl = p->rl[k];
    int off = rl + c->lane + c->err;
    if (rl > 4) off -= c->wide;                 /* the wider line of some */
    else if (rl < -4) off += c->wide;
    off = clampi(off, -(HALF_W - CAR_R - 2), HALF_W - CAR_R - 2);
    int a = p->dir[k];
    int tx = p->x[k] - ((off * isin(a)) >> 14), ty = p->y[k] + ((off * icos(a)) >> 14);
    /* a solid object on its way (the target, or halfway to it): aim at the side of the band that is clear */
    if (in_solid(w, tx * FX, ty * FX) || in_solid(w, (c->x + tx * FX) / 2, (c->y + ty * FX) / 2)) {
        int edge = HALF_W - CAR_R - 2;
        for (int s = 0; s < 2; s++) {
            int o2 = (off > 0) == (s == 0) ? -edge : edge;      /* the far side first */
            int x2 = p->x[k] - ((o2 * isin(a)) >> 14), y2 = p->y[k] + ((o2 * icos(a)) >> 14);
            if (!in_solid(w, x2 * FX, y2 * FX) && !in_solid(w, (c->x + x2 * FX) / 2, (c->y + y2 * FX) / 2)) {
                tx = x2;
                ty = y2;
                off = o2;
                break;
            }
        }
    }
    int ta = iatan2(ty * FX - c->y, tx * FX - c->x);
    /* stuck against something (a sugar cube on its line, a ladybug): back out, then take the other side of the band
     * for a while (nobody is put back on the track any more: it must get round by itself) */
    int sdx = c->x / FX - c->stuck_x, sdy = c->y / FX - c->stuck_y;
    if (w->phase == PH_COUNT || c->ghost || c->rev_t > 0 || sdx * sdx + sdy * sdy > BOT_STUCK_PX * BOT_STUCK_PX) {
        c->stuck_x = (int16_t)(c->x / FX);
        c->stuck_y = (int16_t)(c->y / FX);
        c->stuck_t = 0;
    } else {
        if (++c->stuck_t > BOT_STUCK) {
            c->rev_t = BOT_REVERSE;
            c->stuck_t = 0;
            c->err = (int16_t)(off > 0 ? -2 * HALF_W : 2 * HALF_W);    /* (clamped to the band's side) */
            c->err_t = BOT_REVERSE + BOT_SIDE;
            for (int j = 0; j < RACERS; j++) {  /* wedged against a ladybug (one that does not drive, by an object): */
                const car *o = &w->c[j];        /* it goes through the others a moment, blinking (as one put back) */
                int64_t dx = o->x - c->x, dy = o->y - c->y, r = (2 * CAR_R + 4) * FX;
                if (j != i && o->state == CS_RACE && dx * dx + dy * dy < r * r) c->ghost = GHOST_FRAMES;
            }
        }
    }
    if (c->rev_t > 0) {
        c->rev_t--;
        out->dx = (int16_t)(icos(ta) >> 6);
        out->dy = (int16_t)(isin(ta) >> 6);
        out->brake = 1;
        out->any = 1;
        return;
    }
    out->dx = (int16_t)(icos(ta) >> 6);
    out->dy = (int16_t)(isin(ta) >> 6);
    /* the speed for the turn ahead: the sharpest heading change over 3 waypoints in the next BOT_BRAKE_WP */
    int turn = 0;
    for (int j = 1; j <= BOT_BRAKE_WP; j++) {
        int t = iabs(ang_diff(p->dir[(c->wp + j) % n], p->dir[(c->wp + j + 3) % n]));
        if (t > turn) turn = t;
    }
    if (w->humans == 1 && !c->human) {          /* the mild rubber band (1 player) */
        int32_t d = c->prog - w->c[0].prog;
        if (d > RUBBER_PX) top -= 704 * RUBBER_PCT / 100;
        else if (d < -RUBBER_PX) top += 704 * RUBBER_PCT / 100;
    }
    int safe = top;
    if (turn > BOT_TURN_FREE) safe = top - (turn - BOT_TURN_FREE) * BOT_TURN_K * 100 / corner;
    if (safe < BOT_SAFE_MIN) safe = BOT_SAFE_MIN;
    int aim = iabs(ang_diff(c->ang, ta));
    if (f > safe + 24 || (aim > 200 && f > 300)) out->brake = 1;
    else if (f < safe && aim < 300) out->gas = 1;
    out->any = 1;
}

/* ---- the championship -------------------------------------------------------------------------------------------- */
int champ_track(int race) { return race % TRACKS; }
int champ_reverse(int race) { return (race / TRACKS) & 1; }
int champ_qualifies(int place) { return place <= QUALIFY_PLACE; }

/* ---- touch (player 1) ------------------------------------------------------------------------------------------------- */
void touch_brake_button(int left, int width, tbutton *b)
{
    b->x = left + width - BTN_BRAKE_DX;
    b->y = RS_SCREEN_H - BTN_BRAKE_DY;
    b->r = BTN_BRAKE_R;
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
    int ddx = x - x0, ddy = y - y0;
    if (ddx * ddx + ddy * ddy < STICK_DEAD * STICK_DEAD) {
        *dx = *dy = 0;
        return;
    }
    int a = iatan2(ddy, ddx);
    *dx = (int16_t)(icos(a) >> 6);
    *dy = (int16_t)(isin(a) >> 6);
}
