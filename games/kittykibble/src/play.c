/*
 * Kitty Kibble: the rules (no drawing, no sound): deterministic, testable alone (tests/test_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
 *
 * The maze chase (DESIGN.md "Rules"): the kittens eat every kibble of a room while four robot vacuums chase them;
 * a ball of yarn turns the tables for a few seconds; one vacuum reaching a kitten ends its run. Movement: every
 * actor runs along the corridors' centre lines and decides at cell centres; a kitten keeps the direction pressed
 * (and let go) for TURN_BUFFER frames and takes it at the next junction, even CORNER_PX before or after its centre;
 * it turns round at once. The vacuums never turn round on their own: at each cell they take the open way (not back)
 * that brings them closest to their target (their personality), or a random one when scared.
 */
#include "game.h"
#include <string.h>

static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
static const int RELEASE[VACUUMS] = DOCK_RELEASE;
static const int SLOTS[VACUUMS] = DOCK_SLOTS;
static const int CORNERS[VACUUMS][2] = HOME_CORNERS;
static const int MODES_E[] = MODES_EASY, MODES_H[] = MODES_HARD;
static const int FISH_PTS[] = FISH_POINTS;
static const int FISH_PCT[2] = FISH_AT;

#define Q(px) ((int32_t)(px) * Q16_ONE)
#define CENTER(c) ((c) * CELL + CELL / 2)
#define MAZE_PXW Q(MAZE_W * CELL)
#define OPP(d) (((d) + 2) & 3)

static int iabs(int a) { return a < 0 ? -a : a; }
static int imin(int a, int b) { return a < b ? a : b; }

/* ---- the maze ------------------------------------------------------------------------------------------------- */
int cell_at(const world *w, int cx, int cy)
{
    if (cy < 0 || cy >= MAZE_H) return C_WALL;
    cx %= MAZE_W;
    if (cx < 0) cx += MAZE_W;
    return w->cell[cy][cx];
}

static int walkable(int c) { return c != C_WALL && c != C_GATE && c != C_DOCK; }

int kitten_open(const world *w, int cx, int cy, int dir)
{
    return dir >= 0 && walkable(cell_at(w, cx + DX[dir], cy + DY[dir]));
}

static int is_tunnel_row(const world *w, int cy) { return cell_at(w, 0, cy) == C_TUNNEL; }
static int cell_x(int32_t x) { return (int)(x / Q(CELL)); }

/* the Q16 distance from p to the next cell centre going sgn (+1, -1); 0 on a centre */
static int32_t to_center(int32_t p, int sgn)
{
    int32_t m = Q(CELL), off = ((p - Q(CELL / 2)) % m + m) % m;
    if (sgn > 0) return off ? m - off : 0;
    return off;
}

static int32_t wrap_x(int32_t x)
{
    while (x < 0) x += MAZE_PXW;
    while (x >= MAZE_PXW) x -= MAZE_PXW;
    return x;
}

/* ---- speeds and timings by room ------------------------------------------------------------------------------- */
static int tier(int room) { return imin(room - 1, 6); }

int kitten_speed(const world *w, const kitten *k)
{
    (void)k;
    int pct = 100 + tier(w->room) * KIT_SPEED_UP + (world_yarn_time(w) ? KIT_YARN_UP : 0);
    return (int)((int64_t)KIT_SPEED * pct / 100);
}

int vacuum_speed(const world *w, const vacuum *v)
{
    if (v->state == VS_HOME) return VAC_HOME;
    if (v->state == VS_LEAVE || v->state == VS_ENTER) return VAC_LEAVE;
    int64_t s = (int64_t)VAC_SPEED * (100 + tier(w->room) * VAC_SPEED_UP) / 100;
    if (v->scared_t) s = s * VAC_SCARED / 100;
    int cx = cell_x(v->x), cy = (int)(v->y / Q(CELL));
    if (is_tunnel_row(w, cy) && (cx <= 1 || cx >= MAZE_W - 2)) s = s * VAC_TUNNEL / 100;
    return (int)s;
}

int yarn_frames(int room)
{
    int f = YARN_FRAMES - (room - 1) * YARN_LESS;
    return f < YARN_MIN ? YARN_MIN : f;
}

int dock_release(int room, int v)
{
    return RELEASE[v] * (100 - imin(room - 1, 5) * DOCK_FASTER) / 100;
}

int fish_points(int room)
{
    int n = (int)(sizeof FISH_PTS / sizeof FISH_PTS[0]);
    return FISH_PTS[imin(room - 1, n - 1)];
}

static const int *modes(const world *w) { return w->room <= 2 ? MODES_E : MODES_H; }

int world_yarn_time(const world *w)
{
    int t = 0;
    for (int i = 0; i < VACUUMS; i++)
        if (w->v[i].scared_t > t && w->v[i].state != VS_HOME && w->v[i].state != VS_ENTER) t = w->v[i].scared_t;
    return t;
}

/* ---- set-up ------------------------------------------------------------------------------------------------------ */
static void fx_add(world *w, int kind, int x, int y, int value)
{
    effect *e = &w->fxs[w->fxs_next];
    w->fxs_next = (w->fxs_next + 1) % FX_MAX;
    e->kind = (uint8_t)kind;
    e->x = (int16_t)x;
    e->y = (int16_t)y;
    e->value = (int16_t)value;
    e->t = 0;
    e->on = 1;
}

static void place_kitten(world *w, int p)
{
    kitten *k = &w->k[p];
    k->x = Q(CENTER(w->start_x[p]));
    k->y = Q(CENTER(w->start_y[p]));
    k->dir = w->start_x[p] > MAZE_W / 2 ? DIR_RIGHT : DIR_LEFT;
    k->face = k->dir;
    k->moving = 1;
    k->want = DIR_NONE;
    k->want_t = 0;
    k->pause = 0;
    k->chain = 0;
    k->munch_t = 99;
}

static void place_vacuum(world *w, int i)
{
    vacuum *v = &w->v[i];
    memset(v, 0, sizeof *v);
    if (i == V_DASH) {                      /* Dash starts outside, on the exit cell */
        v->state = VS_ROAM;
        v->x = Q(CENTER(DOCK_X));
        v->y = Q(CENTER(DOCK_Y));
        v->dir = DIR_LEFT;
    } else {
        v->state = VS_DOCK;
        v->x = Q(CENTER(SLOTS[i]));
        v->y = Q(CENTER(DOCK_IN_Y));
        v->dir = DIR_UP;
        v->wait = dock_release(w->room, i);
    }
    v->tx = v->ty = 0;
}

void world_start_room(world *w, int room)
{
    w->room = room;
    w->maze = (room - 1) % ROOMS;
    w->kibble_left = 0;
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++) {
            char ch = maze_rows[w->maze][y][x];
            int c = C_EMPTY;
            switch (ch) {
            case '#': c = C_WALL; break;
            case '.': c = C_KIBBLE; break;
            case 'o': c = C_YARN; break;
            case 'T': c = C_TUNNEL; break;
            case '-': c = C_GATE; break;
            case 'D': c = C_DOCK; break;
            case 'f': w->fish_x = x; w->fish_y = y; break;
            case '1': case '2': case '3': case '4':
                w->start_x[ch - '1'] = x;
                w->start_y[ch - '1'] = y;
                break;
            default: break;
            }
            w->cell[y][x] = (uint8_t)c;
            if (c == C_KIBBLE || c == C_YARN) w->kibble_left++;
        }
    w->kibble_total = w->kibble_left;
    w->phase = PH_INTRO;
    w->phase_t = 0;
    w->freeze = 0;
    w->mode = MODE_SCATTER;
    w->mode_i = 0;
    w->mode_t = modes(w)[0];
    w->fish_on = w->fish_t = w->fish_n = 0;
    for (int p = 0; p < w->players; p++) {
        if (w->k[p].state == KS_CAUGHT) { w->k[p].state = KS_OUT; w->k[p].t = PANEL_DELAY; }
        place_kitten(w, p);
    }
    for (int i = 0; i < VACUUMS; i++) place_vacuum(w, i);
    memset(w->fxs, 0, sizeof w->fxs);
    w->msg = MSG_NONE;
    w->wevents |= WEV_INTRO | (room > 1 ? WEV_ROOM : 0);
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->seed = seed;
    rs_rng_seed(&w->rng, seed);
    rs_rng_seed(&w->bot_rng, seed ^ 0x6b697474u);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        w->k[p].state = p < w->players ? KS_PLAY : KS_OUT;
        w->bot_dir[p] = DIR_NONE;
    }
    world_start_room(w, 1);
    w->phase = PH_READY;                    /* the run waits for its first press (the title's press) */
    w->wevents = 0;
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->k[p].state == KS_PLAY) return 1;
    return 0;
}

int world_all_out(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->k[p].state != KS_OUT) return 0;
    return 1;
}

/* ---- the kittens ------------------------------------------------------------------------------------------------- */
/* the d-pad: a direction pressed is kept TURN_BUFFER frames after it is let go; held, it stays; of several held,
 * the one across the way it goes (a diagonal turns at the next junction) */
static void kitten_input(kitten *k, pin in, int count_down)
{
    int pick = DIR_NONE;
    uint8_t bits = in.pressed ? in.pressed : in.held;
    for (int d = 0; d < 4; d++)
        if (bits & DIR_BIT(d)) {
            if (pick == DIR_NONE) pick = d;
            if (k->dir >= 0 && (d & 1) != (k->dir & 1)) pick = d;   /* across the way: the turn wins */
        }
    if (in.pressed) {
        k->want = pick;
        k->want_t = TURN_BUFFER;
    } else if (k->want >= 0 && (in.held & DIR_BIT(k->want))) {
        k->want_t = TURN_BUFFER;
    } else if (pick != DIR_NONE) {
        k->want = pick;
        k->want_t = TURN_BUFFER;
    } else if (k->want >= 0 && count_down && --k->want_t <= 0) {
        k->want = DIR_NONE;
        k->want_t = 0;
    }
}

static void kitten_move(world *w, int p)
{
    kitten *k = &w->k[p];
    int32_t s = kitten_speed(w, k);
    /* turn round at once */
    if (k->want >= 0 && k->moving && k->want == OPP(k->dir)) {
        k->dir = k->want;
        w->events[p] |= EV_TURN;
    }
    /* a turn across the way, up to CORNER_PX before or after a junction's centre: snap to it and turn */
    if (k->want >= 0 && k->moving && (k->want & 1) != (k->dir & 1)) {
        int32_t *pp = (k->dir & 1) ? &k->y : &k->x;
        int32_t m = Q(CELL), off = ((*pp - Q(CELL / 2)) % m + m) % m;
        if (off > m / 2) off -= m;                    /* signed: from the nearest centre */
        if (off && iabs(off) <= Q(CORNER_PX)) {
            int32_t c = *pp - off;
            int cx = cell_x(k->dir & 1 ? k->x : wrap_x(c)), cy = (int)((k->dir & 1 ? c : k->y) / Q(CELL));
            if (kitten_open(w, cx, cy, k->want)) {
                *pp = (k->dir & 1) ? c : wrap_x(c);
                k->dir = k->want;
                w->events[p] |= EV_TURN;
            }
        }
    }
    while (s > 0) {
        int horiz = !(k->dir & 1);
        int sgn = DX[k->dir] + DY[k->dir];
        int32_t *pp = horiz ? &k->x : &k->y;
        int32_t d = to_center(*pp, sgn);
        if (d == 0) {                                  /* on a centre: the buffered turn, or stop at a wall */
            int cx = cell_x(k->x), cy = (int)(k->y / Q(CELL));
            if (k->want >= 0 && k->want != k->dir && kitten_open(w, cx, cy, k->want)) {
                k->dir = k->want;
                k->moving = 1;
                w->events[p] |= EV_TURN;
            }
            if (!kitten_open(w, cx, cy, k->dir)) {
                k->moving = 0;
                return;
            }
            k->moving = 1;
            horiz = !(k->dir & 1);
            sgn = DX[k->dir] + DY[k->dir];
            pp = horiz ? &k->x : &k->y;
            d = Q(CELL);
        }
        int32_t step = s < d ? s : d;
        *pp += sgn * step;
        if (horiz) k->x = wrap_x(k->x);
        s -= step;
    }
    k->face = k->dir;
}

static void eat(world *w, int p)
{
    kitten *k = &w->k[p];
    int cx = cell_x(k->x), cy = (int)(k->y / Q(CELL));
    uint8_t *c = &w->cell[cy][cx];
    if (*c == C_KIBBLE) {
        *c = C_EMPTY;
        k->score += KIBBLE_POINTS;
        k->eaten++;
        k->pause = KIBBLE_PAUSE;
        k->munch_t = 0;
        w->kibble_left--;
        w->events[p] |= EV_MUNCH;
    } else if (*c == C_YARN) {
        *c = C_EMPTY;
        k->score += YARN_POINTS;
        k->pause = YARN_PAUSE;
        k->munch_t = 0;
        w->kibble_left--;
        w->events[p] |= EV_YARN;
        w->wevents |= WEV_SCARED;
        int f = yarn_frames(w->room);
        for (int i = 0; i < VACUUMS; i++) {
            vacuum *v = &w->v[i];
            if (v->state == VS_HOME || v->state == VS_ENTER) continue;
            if (v->state == VS_ROAM && !v->scared_t) v->reverse = 1;
            v->scared_t = f;
        }
        for (int q = 0; q < w->players; q++) w->k[q].chain = 0;
        fx_add(w, FX_SPARK, CENTER(cx), CENTER(cy), 0);
    }
    /* the fish treat: 30% and 70% of the room eaten */
    int eaten = w->kibble_total - w->kibble_left;
    if (w->fish_n < 2 && eaten * 100 >= w->kibble_total * FISH_PCT[w->fish_n]) {
        w->fish_n++;
        w->fish_on = 1;
        w->fish_t = FISH_FRAMES;
        w->wevents |= WEV_FISH;
    }
    if (w->fish_on && cx == w->fish_x && cy == w->fish_y) {
        int pts = fish_points(w->room);
        k->score += pts;
        w->fish_on = 0;
        w->events[p] |= EV_FISH;
        fx_add(w, FX_POINTS, CENTER(cx), CENTER(cy), pts);
    }
}

/* ---- the vacuums --------------------------------------------------------------------------------------------- */
int nearest_kitten(const world *w, int cx, int cy)
{
    int best = -1, bd = 1 << 30;
    for (int p = 0; p < w->players; p++) {
        const kitten *k = &w->k[p];
        if (k->state != KS_PLAY) continue;
        int dx = iabs(cell_x(k->x) - cx), dy = iabs((int)(k->y / Q(CELL)) - cy);
        if (dx > MAZE_W / 2) dx = MAZE_W - dx;      /* the tunnels */
        if (dx + dy < bd) { bd = dx + dy; best = p; }
    }
    return best;
}

void vacuum_target(const world *w, int i, int *tx, int *ty)
{
    const vacuum *v = &w->v[i];
    int cx = cell_x(v->x), cy = (int)(v->y / Q(CELL));
    *tx = CORNERS[i][0];
    *ty = CORNERS[i][1];
    if (v->state == VS_HOME) { *tx = DOCK_X; *ty = DOCK_Y; return; }
    int p = nearest_kitten(w, cx, cy);
    if (p < 0 || w->mode == MODE_SCATTER) return;
    const kitten *k = &w->k[p];
    int kx = cell_x(k->x), ky = (int)(k->y / Q(CELL));
    switch (i) {
    case V_DASH:                                /* straight for the kitten */
        *tx = kx;
        *ty = ky;
        break;
    case V_SNEAK:                               /* cuts it off: the cell SNEAK_AHEAD ahead of where it faces */
        *tx = kx + DX[k->face] * SNEAK_AHEAD;
        *ty = ky + DY[k->face] * SNEAK_AHEAD;
        break;
    case V_PATROL: {                            /* guards its post: chases a kitten that comes near it */
        int dx = kx - PATROL_POST_X, dy = ky - PATROL_POST_Y;
        if (dx * dx + dy * dy <= PATROL_RANGE * PATROL_RANGE) { *tx = kx; *ty = ky; }
        else { *tx = PATROL_POST_X; *ty = PATROL_POST_Y; }
        break;
    }
    case V_WOBBLE: {                            /* wanders (vacuum_decide), backs off to its corner when close */
        int dx = kx - cx, dy = ky - cy;
        if (dx * dx + dy * dy > WOBBLE_SHY * WOBBLE_SHY) { *tx = -1000; *ty = -1000; }   /* -1000: at random */
        break;
    }
    }
}

static int vacuum_open(const world *w, const vacuum *v, int cx, int cy, int dir)
{
    int c = cell_at(w, cx + DX[dir], cy + DY[dir]);
    (void)v;
    return walkable(c);
}

static void vacuum_decide(world *w, int i)
{
    vacuum *v = &w->v[i];
    int cx = cell_x(v->x), cy = (int)(v->y / Q(CELL));
    if (v->reverse) {
        v->reverse = 0;
        if (vacuum_open(w, v, cx, cy, OPP(v->dir))) { v->dir = OPP(v->dir); return; }
    }
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++)
        if (d != OPP(v->dir) && vacuum_open(w, v, cx, cy, d)) opts[n++] = d;
    if (!n) { v->dir = OPP(v->dir); return; }
    int tx, ty;
    vacuum_target(w, i, &tx, &ty);
    v->tx = tx;
    v->ty = ty;
    if ((v->scared_t && v->state == VS_ROAM) || tx == -1000) {       /* scared (or Wobble far away): at random */
        v->dir = opts[rs_rng_range(&w->rng, n)];
        return;
    }
    int best = opts[0], bd = 1 << 30;
    for (int k = 0; k < n; k++) {
        int nx = cx + DX[opts[k]] - tx, ny = cy + DY[opts[k]] - ty, dd = nx * nx + ny * ny;
        if (dd < bd) { bd = dd; best = opts[k]; }
    }
    v->dir = best;
}

/* move toward the stage point (x, y) at speed s along one axis then the other (x first if xfirst); 1 = arrived */
static int slide_to(vacuum *v, int32_t x, int32_t y, int32_t s, int xfirst)
{
    for (int pass = 0; pass < 2 && s > 0; pass++) {
        int horiz = (pass == 0) == (xfirst != 0);
        int32_t *pp = horiz ? &v->x : &v->y, goal = horiz ? x : y, d = goal - *pp;
        if (!d) continue;
        int32_t step = iabs(d) < s ? iabs(d) : s;
        *pp += d > 0 ? step : -step;
        v->dir = horiz ? (d > 0 ? DIR_RIGHT : DIR_LEFT) : (d > 0 ? DIR_DOWN : DIR_UP);
        s -= step;
        if (*pp != goal) return 0;
    }
    return v->x == x && v->y == y;
}

static void vacuum_step(world *w, int i)
{
    vacuum *v = &w->v[i];
    static const int lane[VACUUMS] = DOCK_SLOTS;
    v->t++;
    if (v->scared_t && v->state != VS_HOME && v->state != VS_ENTER) {
        if (--v->scared_t == 0 && world_yarn_time(w) == 0) w->wevents |= WEV_CALM;
    }
    switch (v->state) {
    case VS_DOCK:
        if (v->wait > 0) v->wait--;
        else { v->state = VS_LEAVE; v->t = 0; }
        return;
    case VS_LEAVE:
        if (slide_to(v, Q(CENTER(DOCK_X)), Q(CENTER(DOCK_Y)), vacuum_speed(w, v), 1)) {
            v->state = VS_ROAM;
            v->t = 0;
            v->dir = (i & 1) ? DIR_RIGHT : DIR_LEFT;
            vacuum_decide(w, i);
        }
        return;
    case VS_ENTER:
        if (slide_to(v, Q(CENTER(lane[i])), Q(CENTER(DOCK_IN_Y)), vacuum_speed(w, v), 0)) {
            v->state = VS_DOCK;
            v->t = 0;
            v->wait = DOCK_REGEN;
            v->scared_t = 0;
            v->dir = DIR_UP;
        }
        return;
    default:
        break;
    }
    int32_t s = vacuum_speed(w, v);
    while (s > 0) {
        int horiz = !(v->dir & 1), sgn = DX[v->dir] + DY[v->dir];
        int32_t *pp = horiz ? &v->x : &v->y;
        int32_t d = to_center(*pp, sgn);
        if (d == 0) {
            int cx = cell_x(v->x), cy = (int)(v->y / Q(CELL));
            if (v->state == VS_HOME && cx == DOCK_X && cy == DOCK_Y) {      /* back at the gate: in it goes */
                v->state = VS_ENTER;
                v->t = 0;
                return;
            }
            vacuum_decide(w, i);
            horiz = !(v->dir & 1);
            sgn = DX[v->dir] + DY[v->dir];
            pp = horiz ? &v->x : &v->y;
            d = Q(CELL);
        }
        int32_t step = s < d ? s : d;
        *pp += sgn * step;
        if (horiz) v->x = wrap_x(v->x);
        s -= step;
    }
}

/* ---- meeting: a kitten catches a scared vacuum, or a vacuum catches a kitten ---------------------------------- */
static void meet(world *w)
{
    for (int p = 0; p < w->players; p++) {
        kitten *k = &w->k[p];
        if (k->state != KS_PLAY) continue;
        for (int i = 0; i < VACUUMS; i++) {
            vacuum *v = &w->v[i];
            if (v->state != VS_ROAM && v->state != VS_LEAVE) continue;
            int dx = iabs((int)((k->x - v->x) >> 16)), dy = iabs((int)((k->y - v->y) >> 16));
            if (dx > MAZE_W * CELL / 2) dx = MAZE_W * CELL - dx;
            if (dx >= CATCH_PX || dy >= CATCH_PX) continue;
            if (v->scared_t) {                       /* caught: it zips back to its dock */
                int pts = CATCH_POINTS << imin(k->chain, 3);
                k->chain++;
                k->catches++;
                k->score += pts;
                v->state = VS_HOME;
                v->scared_t = 0;
                v->reverse = 0;
                v->t = 0;
                fx_add(w, FX_POINTS, (int)(v->x >> 16), (int)(v->y >> 16), pts);
                fx_add(w, FX_DUST, (int)(v->x >> 16), (int)(v->y >> 16), 0);
                w->events[p] |= EV_CATCH;
                w->wevents |= WEV_HOME;
                w->freeze = CATCH_FREEZE;
            } else {                                 /* one hit ends the run (the house rule) */
                k->state = KS_CAUGHT;
                k->t = 0;
                w->events[p] |= EV_CAUGHT;
                w->freeze = world_running(w) ? HIT_FREEZE_MP : HIT_FREEZE;
                if (w->players > 1) { w->msg = MSG_CAUGHT; w->msg_t = MSG_FRAMES; w->msg_player = p; }
                break;
            }
        }
    }
}

/* ---- the step ------------------------------------------------------------------------------------------------- */
void world_step(world *w, const pin in[MAX_PLAYERS])
{
    memset(w->events, 0, sizeof w->events);
    w->wevents = 0;
    if (!w->started) {
        int go = 0;
        for (int p = 0; p < w->players; p++) go |= in[p].held | in[p].pressed | in[p].start;
        if (!go) return;
        w->started = 1;
        w->phase = PH_INTRO;
        w->phase_t = 0;
        w->wevents |= WEV_INTRO;
    }
    w->t++;
    for (int i = 0; i < FX_MAX; i++)
        if (w->fxs[i].on && ++w->fxs[i].t >= (w->fxs[i].kind == FX_POINTS ? 50 : 24)) w->fxs[i].on = 0;
    if (w->msg_t && --w->msg_t == 0) w->msg = MSG_NONE;
    for (int p = 0; p < w->players; p++) {
        kitten *k = &w->k[p];
        if (k->state == KS_PLAY) kitten_input(k, in[p], w->phase == PH_PLAY && !w->freeze);
    }
    if (w->phase == PH_INTRO) {
        if (++w->phase_t >= INTRO_FRAMES) { w->phase = PH_PLAY; w->phase_t = 0; w->wevents |= WEV_PLAY; }
        return;
    }
    if (w->phase == PH_CLEAR) {
        if (++w->phase_t >= CLEAR_FRAMES) world_start_room(w, w->room + 1);
        return;
    }
    if (w->phase != PH_PLAY) return;
    w->phase_t++;
    if (w->freeze) { w->freeze--; return; }
    for (int p = 0; p < w->players; p++) {
        kitten *k = &w->k[p];
        k->t++;
        if (k->state == KS_CAUGHT) {
            if (k->t >= CAUGHT_FRAMES) { k->state = KS_OUT; k->t = 0; w->events[p] |= EV_OUT; }
            continue;
        }
        if (k->state != KS_PLAY) continue;
        if (k->munch_t < 99) k->munch_t++;
        if (k->pause) { k->pause--; continue; }
        kitten_move(w, p);
        eat(w, p);
    }
    if (!w->kibble_left) {                           /* the room is clean: the next one */
        w->phase = PH_CLEAR;
        w->phase_t = 0;
        w->wevents |= WEV_CLEAR;
        for (int i = 0; i < VACUUMS; i++) w->v[i].scared_t = 0;
        return;
    }
    if (!world_running(w)) return;                   /* the last kitten spins: the vacuums stop */
    /* scatter / chase: the clock stops during yarn time */
    if (!world_yarn_time(w) && w->mode_t > 0 && --w->mode_t == 0) {
        w->mode_i++;
        w->mode = w->mode_i & 1 ? MODE_CHASE : MODE_SCATTER;
        w->mode_t = modes(w)[w->mode_i];
        w->wevents |= WEV_MODE;
        for (int i = 0; i < VACUUMS; i++)
            if (w->v[i].state == VS_ROAM && !w->v[i].scared_t) w->v[i].reverse = 1;
    }
    for (int i = 0; i < VACUUMS; i++) vacuum_step(w, i);
    if (w->fish_on && --w->fish_t <= 0) { w->fish_on = 0; w->wevents |= WEV_FISH_GONE; }
    meet(w);
}

/* ---- touch: a swipe step turns the kitten -------------------------------------------------------------------- */
int touch_swipe_dir(world *w, int down, int pressed, int x, int y)
{
    if (!down) { w->touch_on = 0; return DIR_NONE; }
    if (pressed || !w->touch_on) {
        w->touch_on = 1;
        w->touch_x = x;
        w->touch_y = y;
        return DIR_NONE;
    }
    int dx = x - w->touch_x, dy = y - w->touch_y, ax = iabs(dx), ay = iabs(dy);
    if (ax < TOUCH_DRAG && ay < TOUCH_DRAG) return DIR_NONE;
    w->touch_x = x;                                  /* measured again from here: a finger can swipe on and on */
    w->touch_y = y;
    if (ax >= 2 * ay) return dx > 0 ? DIR_RIGHT : DIR_LEFT;
    if (ay >= 2 * ax) return dy > 0 ? DIR_DOWN : DIR_UP;
    return DIR_NONE;
}

/* ---- the bot -------------------------------------------------------------------------------------------------- */
#define NCELL (MAZE_W * MAZE_H)
#define BOT_ESCAPE 5            /* a target must leave this many safe cells beyond it (no corner to be trapped in) */
#define BOT_MARGIN 20           /* frames: a cell is safe when the kitten gets there this much before any vacuum */
static int16_t bot_dk[NCELL], bot_tv[NCELL], bot_first[NCELL], bot_q[NCELL], bot_up[NCELL];   /* scratch */
static int16_t bot_tmp[NCELL], bot_safe[NCELL], bot_reach[NCELL];                               /* (state_audit) */

/* breadth-first distances (cells) from (sx, sy) over the corridors, through `ok` cells only when ok != NULL; the
 * first step of each path, the parent of each cell; returns the cells reached (bot_q holds them in order) */
static int bfs(const world *w, int sx, int sy, int16_t *dist, int16_t *first, const int16_t *ok)
{
    for (int i = 0; i < NCELL; i++) { dist[i] = -1; if (first) first[i] = -1; }
    int head = 0, tail = 0, s = sy * MAZE_W + sx;
    dist[s] = 0;
    bot_up[s] = -1;
    bot_q[tail++] = (int16_t)s;
    while (head < tail) {
        int c = bot_q[head++], cx = c % MAZE_W, cy = c / MAZE_W;
        for (int d = 0; d < 4; d++) {
            int nx = (cx + DX[d] + MAZE_W) % MAZE_W, ny = cy + DY[d];
            if (ny < 0 || ny >= MAZE_H || !walkable(w->cell[ny][nx])) continue;
            int n = ny * MAZE_W + nx;
            if (dist[n] >= 0 || (ok && !ok[n])) continue;
            dist[n] = (int16_t)(dist[c] + 1);
            if (first) first[n] = (int16_t)(c == s ? d : first[c]);
            bot_up[n] = (int16_t)c;
            bot_q[tail++] = (int16_t)n;
        }
    }
    return tail;
}

/* when each corridor cell can be reached by the first dangerous vacuum (frames, bot_tv): a scared one only once its
 * fright is over, one in the dock once it is out; vacuums further than `vision` cells from (kx, ky) are not seen */
static void bot_danger(const world *w, int kx, int ky, int vision)
{
    for (int i = 0; i < NCELL; i++) bot_tv[i] = 30000;
    bfs(w, kx, ky, bot_reach, NULL, NULL);                          /* the kitten's plain distances */
    int vs = (int)((int64_t)VAC_SPEED * (100 + tier(w->room) * VAC_SPEED_UP) / 100);
    for (int i = 0; i < VACUUMS; i++) {
        const vacuum *v = &w->v[i];
        if (v->state == VS_HOME || v->state == VS_ENTER) continue;
        int vx = cell_x(v->x), vy = (int)(v->y / Q(CELL)), delay = v->scared_t ? v->scared_t + 6 : 0;
        if (v->state == VS_DOCK || v->state == VS_LEAVE) { vx = DOCK_X; vy = DOCK_Y; delay += v->wait + 30; }
        if (bot_reach[vy * MAZE_W + vx] > vision) continue;           /* it does not see that far */
        bfs(w, vx, vy, bot_tmp, NULL, NULL);
        for (int c = 0; c < NCELL; c++)
            if (bot_tmp[c] >= 0) {
                int t = (int)((int64_t)bot_tmp[c] * Q(CELL) / vs) + delay;
                if (t < bot_tv[c]) bot_tv[c] = (int16_t)(t > 30000 ? 30000 : t);
            }
    }
}

/* the bot's look-ahead: the world played on BOT_LOOK frames (a copy) with the kitten taking `dir`, then fleeing (at
 * each junction the way furthest from the vacuums): how long it lasts. A plan that gets it cornered is changed. */
#define BOT_LOOK 150
#define BOT_LOOK_SKILLS 7       /* the human-like bot looks ahead less and less, not at all from skill 7 */
static world bot_sim;                                                           /* scratch (state_audit.txt) */

static int flee_dir(const world *s, const kitten *k)
{
    int kx = cell_x(k->x), ky = (int)(k->y / Q(CELL)), best = k->dir, bv = -1;
    for (int d = 0; d < 4; d++) {
        if (!kitten_open(s, kx, ky, d)) continue;
        int nx = (kx + DX[d] + MAZE_W) % MAZE_W, ny = ky + DY[d], m = 1 << 20;
        for (int i = 0; i < VACUUMS; i++) {
            const vacuum *v = &s->v[i];
            if (v->state != VS_ROAM || v->scared_t > 60) continue;
            int dx = iabs(cell_x(v->x) - nx), dy = iabs((int)(v->y / Q(CELL)) - ny);
            if (dx > MAZE_W / 2) dx = MAZE_W - dx;
            if (dx + dy < m) m = dx + dy;
        }
        m = m * 4 - (d == OPP(k->dir) ? 3 : 0);
        if (m > bv) { bv = m; best = d; }
    }
    return best;
}

static int survive(const world *w, int p, int dir, int look)
{
    pin in[MAX_PLAYERS];
    memset(in, 0, sizeof in);
    bot_sim = *w;
    for (int f = 0; f < look; f++) {
        const kitten *k = &bot_sim.k[p];
        if (k->state != KS_PLAY) return f;
        if (bot_sim.room != w->room || bot_sim.phase == PH_CLEAR) return look;
        in[p].held = (uint8_t)DIR_BIT(f < 12 ? dir : flee_dir(&bot_sim, k));
        world_step(&bot_sim, in);
    }
    return look;
}

static int bot_verify(world *w, int p, int dir, int kx, int ky, int look)
{
    int s0 = survive(w, p, dir, look);
    if (s0 >= look) return dir;
    int best = dir, bs = s0;
    for (int d = 0; d < 4; d++) {
        if (d == dir || (!kitten_open(w, kx, ky, d) && d != OPP(w->k[p].dir))) continue;
        int s = survive(w, p, d, look);
        if (s > bs) { bs = s; best = d; }
    }
    return best;
}

int bot_decide(world *w, int p, int skill)
{
    kitten *k = &w->k[p];
    if (k->state != KS_PLAY) return 0;
    if (w->phase != PH_PLAY && w->phase != PH_READY) return 0;
    /* the human-like bot reacts late: it keeps its last decision a few frames */
    if (skill > 0 && w->bot_t[p] > 0 && w->bot_dir[p] != DIR_NONE) {
        w->bot_t[p]--;
        return DIR_BIT(w->bot_dir[p]);
    }
    int ks = kitten_speed(w, k);
    int vision = skill > 0 ? 30 - 2 * skill : 99;
    int margin = skill > 0 ? BOT_MARGIN - 3 * skill : BOT_MARGIN;
    /* the decision cell: the next centre ahead (or here when stopped); behind instead when ahead is not safe */
    int kx = cell_x(k->x), ky = (int)(k->y / Q(CELL)), extra = 0;
    int bx = kx, by = ky, bextra = 0;
    if (k->moving) {
        int sg = DX[k->dir] + DY[k->dir];
        int32_t pp = (k->dir & 1) ? k->y : k->x;
        int32_t ahead = to_center(pp, sg), behind = to_center(pp, -sg);
        int32_t ca = pp + sg * ahead, cb = pp - sg * behind;
        if (k->dir & 1) { ky = (int)(ca / Q(CELL)); by = (int)(cb / Q(CELL)); }
        else { kx = cell_x(wrap_x(ca)); bx = cell_x(wrap_x(cb)); }
        extra = (int)(ahead / ks);
        bextra = (int)(behind / ks);
    }
    bot_danger(w, kx, ky, vision);
    if (skill < 0) {                                     /* botstop: it runs into the nearest vacuum (the tests) */
        bfs(w, kx, ky, bot_dk, bot_first, NULL);
        int target = -1, bd = 1 << 30;
        for (int i = 0; i < VACUUMS; i++) {
            const vacuum *v = &w->v[i];
            if (v->state != VS_ROAM || v->scared_t) continue;
            int c = (int)(v->y / Q(CELL)) * MAZE_W + cell_x(v->x);
            if (bot_dk[c] >= 0 && bot_dk[c] < bd) { bd = bot_dk[c]; target = c; }
        }
        int d = target >= 0 && bot_dk[target] > 0 ? bot_first[target] : k->dir;
        return d < 0 ? 0 : DIR_BIT(d);
    }
    if (k->moving && bot_tv[ky * MAZE_W + kx] <= extra + margin / 2 && bot_tv[by * MAZE_W + bx] > bextra + margin) {
        kx = bx;                                         /* a vacuum comes the other way: turn round */
        ky = by;
        extra = bextra;
    }
    int here = ky * MAZE_W + kx;
    bfs(w, kx, ky, bot_dk, NULL, NULL);
    for (int c = 0; c < NCELL; c++) {
        int tk = bot_dk[c] < 0 ? 30000 : extra + (int)((int64_t)bot_dk[c] * Q(CELL) * 13 / 10 / ks);
        bot_safe[c] = (int16_t)(bot_dk[c] >= 0 && tk + margin < bot_tv[c]);
    }
    bot_safe[here] = 1;
    int n = bfs(w, kx, ky, bot_dk, bot_first, bot_safe);
    /* how deep the safe corridors go beyond each cell (no dead end between two vacuums) */
    for (int i = 0; i < n; i++) bot_reach[bot_q[i]] = bot_dk[bot_q[i]];
    for (int i = n - 1; i > 0; i--) {
        int c = bot_q[i], u = bot_up[c];
        if (bot_reach[c] > bot_reach[u]) bot_reach[u] = bot_reach[c];
    }
    int deepest = bot_reach[here];
    int danger_near = bot_tv[here] < 150;
    int best = -1, bcost = 1 << 30;
    for (int i = 1; i < n; i++) {
        int c = bot_q[i], cx = c % MAZE_W, cy = c / MAZE_W, bonus = -1;
        if (bot_reach[c] < bot_dk[c] + BOT_ESCAPE && bot_reach[c] < deepest) continue;
        int cell = w->cell[cy][cx];
        if (cell == C_KIBBLE) bonus = 0;
        if (cell == C_YARN) bonus = (danger_near && skill <= 5) ? 14 : (w->kibble_left <= 4 ? 0 : -1);
        if (w->fish_on && cx == w->fish_x && cy == w->fish_y && w->fish_t > bot_dk[c] * 10) bonus = 16;
        for (int v = 0; v < VACUUMS; v++) {
            const vacuum *vc = &w->v[v];
            if (vc->state != VS_ROAM || !vc->scared_t || cell_x(vc->x) != cx || (int)(vc->y / Q(CELL)) != cy) continue;
            if (vc->scared_t > bot_dk[c] * 14 + 30 && (skill <= 5 || bot_dk[c] < 6)) bonus = 30;
        }
        if (bonus < 0) continue;
        int cost = bot_dk[c] * 2 - bonus * 2 + (bot_first[c] == k->dir ? 0 : 1) - (c == w->bot_goal[p] ? 6 : 0);
        if (cost < bcost) { bcost = cost; best = c; }
    }
    if (best < 0) {                                      /* nothing is safe: the nearest kibble all the same */
        n = bfs(w, kx, ky, bot_dk, bot_first, NULL);
        for (int i = 1; i < n && best < 0; i++) {
            int c = bot_q[i], cl = w->cell[c / MAZE_W][c % MAZE_W];
            if (cl == C_KIBBLE || cl == C_YARN) best = c;
        }
        danger_near = 1;                                 /* (the look-ahead checks the way) */
    }
    w->bot_goal[p] = best;
    int dir = best >= 0 ? bot_first[best] : DIR_NONE;
    if (dir == DIR_NONE) {                               /* nowhere safe: the way that keeps the vacuums furthest */
        int bv = -(1 << 30);
        for (int d = 0; d < 4; d++) {
            if (!kitten_open(w, kx, ky, d)) continue;
            int nx = (kx + DX[d] + MAZE_W) % MAZE_W, ny = ky + DY[d], c = ny * MAZE_W + nx;
            int v = bot_tv[c] * 4 + (d == k->dir ? 1 : 0);
            if (v > bv) { bv = v; dir = d; }
        }
    }
    if (skill == 0 && w->phase == PH_PLAY && dir != DIR_NONE && danger_near) {
        int key = (here * 4 + dir) * 4 + k->dir + 1;    /* looked ahead once per cell and plan */
        if (w->bot_t[p] != key) { w->bot_t[p] = key; w->bot_dir[p] = bot_verify(w, p, dir, kx, ky, BOT_LOOK); }
        dir = w->bot_dir[p];
    }
    if (skill > 0 && skill < BOT_LOOK_SKILLS && w->phase == PH_PLAY && dir != DIR_NONE && danger_near)
        dir = bot_verify(w, p, dir, kx, ky, BOT_LOOK - skill * BOT_LOOK / BOT_LOOK_SKILLS);   /* a shorter look */
    if (skill > 0) {                                     /* a slip now and then: a random way */
        if (rs_rng_range(&w->bot_rng, 100) < skill * 3) {
            int opts[4], no = 0;
            for (int d = 0; d < 4; d++)
                if (kitten_open(w, kx, ky, d) && d != OPP(k->dir)) opts[no++] = d;
            if (no) dir = opts[rs_rng_range(&w->bot_rng, no)];
        }
        w->bot_dir[p] = dir;
        w->bot_t[p] = 2 + skill;
    }
    return dir == DIR_NONE ? 0 : DIR_BIT(dir);
}
