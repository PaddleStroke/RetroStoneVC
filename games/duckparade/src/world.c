/*
 * Duck Parade: a run. The ducks and their hops, the parades (Snake: each duckling goes where its parent was
 * `lag` hops ago), the lost ducklings, the nest ponds, the camera and the fox.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * Timing (the judge in lanes.c follows the same rules, frame for frame): a hop that starts at frame T is in
 * the lane it leaves for frames T..T+HOP_MID-1 and in the lane it lands in for T+HOP_MID..T+HOP_FRAMES-1; at
 * T+HOP_FRAMES the duck stands where it landed (and may hop again at once).
 */
#include "dp.h"
#include <string.h>

const lane *world_lane(const world *w, int32_t col) { return lanes_get(&w->L, col); }

int world_cam_px(const world *w) { return w->cam >> 8; }

int medal_of(int score)
{
    return score >= MEDAL_PEARL ? 4 : score >= MEDAL_GOLD ? 3 : score >= MEDAL_SILVER ? 2 : score >= MEDAL_BRONZE ? 1 : 0;
}

/* ---- places -------------------------------------------------------------------------------------------- */
static int place_y_at(const world *w, const place *p, uint32_t t)
{
    if (p->plat < 0) return p->y;
    const lane *l = world_lane(w, p->col);
    if (!l || p->plat >= l->n) return -1000;
    return field_y_of_loop(mover_pos(l, p->plat, t) + p->y);
}

int place_field_y(const world *w, const place *p) { return place_y_at(w, p, w->t); }

static place ground(int32_t col, int y) { place p = {col, (int16_t)y, -1, 0}; return p; }

/* a river landing: the platform under the centre at frame t, the place relative to it (plat -1: water) */
static place on_river(const world *w, int32_t col, int y, uint32_t t)
{
    const lane *l = world_lane(w, col);
    place p = ground(col, y);
    int i = l ? lane_platform(l, t, y + CELL / 2) : -1;
    if (i >= 0) {
        p.plat = (int8_t)i;
        p.y = (int16_t)(y - field_y_of_loop(mover_pos(l, i, t)));
        /* keep the offset small: the platform's top may be a loop turn away */
        while (p.y > LOOP_PX / 2) p.y = (int16_t)(p.y - LOOP_PX);
        while (p.y < -LOOP_PX / 2) p.y = (int16_t)(p.y + LOOP_PX);
    }
    return p;
}

static int same_place(const place *a, const place *b)
{
    return a->col == b->col && a->y == b->y && a->plat == b->plat;
}

/* where a hop is (the lane and the top y) for the collision checks at frame t */
static void hop_spot(const world *w, const hop *h, uint32_t t, int32_t *col, int *y)
{
    const place *p = h->t < h->dur / 2 ? &h->from : &h->to;
    *col = p->col;
    *y = place_y_at(w, p, t);
}

/* ---- set-up ---------------------------------------------------------------------------------------------- */
static void take_spawns(world *w)
{
    lanes *L = &w->L;
    for (int i = 0; i < L->nsp; i++) {
        for (int k = 0; k < LOOSE_MAX; k++)
            if (w->ls[k].state == LOOSE_FREE) {
                loose *q = &w->ls[k];
                memset(q, 0, sizeof *q);
                q->state = LOOSE_WAIT;
                q->col = L->sp[i].col;
                q->row = L->sp[i].row;
                q->x = (q->col * CELL) << 8;
                q->y = (q->row * CELL) << 8;
                q->id = (int16_t)w->next_id++;
                q->t = (int16_t)(q->id * 37 & 63);
                break;
            }
    }
    L->nsp = 0;
}

static void generate(world *w, int urgent)
{
    int32_t cam_col = world_cam_px(w) / CELL;
    lanes_generate(&w->L, cam_col + SCREEN_COLS + 14, 1);
    if (urgent || w->L.g.next_col < cam_col + SCREEN_COLS + 3) lanes_generate(&w->L, cam_col + SCREEN_COLS + 3, -1);
    take_spawns(w);
}

static void duck_place(world *w, duck *d, int32_t col, int row)
{
    memset(d, 0, sizeof *d);
    d->state = DK_READY;
    d->at = ground(col, row * CELL);
    d->hist[0] = d->at;
    d->nhist = 1;
    d->max_col = col;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->players = players;
    lanes_init(&w->L, seed);
    lanes_generate(&w->L, SCREEN_COLS + 14, -1);
    take_spawns(w);
    /* the parents line up in the start column: Mother in the middle, Father 2 rows below, P3 2 above, P4 4 below
     * (the cells between them are cleared: the meadow stays connected) */
    static const int8_t row_of[MAX_PLAYERS] = {START_ROW, START_ROW + 2, START_ROW - 2, START_ROW + 4};
    lane *l = &w->L.ring[START_COL % RING];
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (p < players) {
            if (p) {
                int a = row_of[p] < START_ROW ? row_of[p] : START_ROW + 1, b = row_of[p] < START_ROW ? START_ROW - 1 : row_of[p];
                for (int r = a; r <= b; r++) l->block &= (uint16_t)~(1 << r);
            }
            duck_place(w, &w->d[p], START_COL, row_of[p]);
        } else {
            w->d[p].state = DK_OUT;
        }
        w->bank_t[p] = -1;
    }
}

/* ---- queries ------------------------------------------------------------------------------------------------ */
static int alive(const duck *d) { return d->state == DK_READY || d->state == DK_ALIVE; }

int world_running(const world *w)
{
    if (!w->started) return 0;
    for (int p = 0; p < w->players; p++) if (alive(&w->d[p])) return 1;
    return 0;
}

int world_all_out(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        const duck *d = &w->d[p];
        if (alive(d)) return 0;
        if (d->state != DK_OUT && d->t < DEATH_ANIM) return 0;
    }
    return 1;
}

int world_score(const world *w, int p)
{
    const duck *d = &w->d[p];
    return (d->max_col - START_COL) + d->banked;
}

int world_total(const world *w)
{
    int32_t far = START_COL;
    int banked = 0;
    for (int p = 0; p < w->players; p++) {
        if (w->d[p].max_col > far) far = w->d[p].max_col;
        banked += w->d[p].banked;
    }
    return (far - START_COL) + banked;
}

int world_line_total(const world *w)
{
    int n = 0;
    for (int p = 0; p < w->players; p++) n += w->d[p].nline;
    return n;
}

int world_fox_state(const world *w, int *target)
{
    int st = 0;
    int32_t behind = 0;
    for (int p = 0; p < w->players; p++) {
        const duck *d = &w->d[p];
        if (d->state == DK_CAUGHT && d->t < DEATH_ANIM) { if (target) *target = p; return 2; }
        if (alive(d) && d->fox_warn && (!st || d->at.col < behind)) { st = 1; behind = d->at.col; if (target) *target = p; }
    }
    return st;
}

/* the hop's progress along its move (the clone: 75% at mid-hop), in 1/256 */
static int hop_frac(int t, int dur)
{
    int mid = dur / 2;
    if (t <= mid) return 192 * t / (mid ? mid : 1);
    return 192 + 64 * (t - mid) / (dur - mid);
}

static int hop_lift(int t, int dur, int height) { return height * 4 * t * (dur - t) / (dur * dur); }

static void hop_visual(const world *w, const place *at, const hop *h, int wait, int *x, int *y, int *lift)
{
    int fy = place_field_y(w, at);
    *x = at->col * CELL;
    *y = fy;
    *lift = 0;
    if (h->dir && wait <= 0) {
        int y0 = place_field_y(w, &h->from), y1 = place_field_y(w, &h->to);
        int f = hop_frac(h->t, h->dur);
        *x = h->from.col * CELL + (h->to.col - h->from.col) * CELL * f / 256;
        *y = y0 + (y1 - y0) * f / 256;
        *lift = hop_lift(h->t, h->dur, h->dur < HOP_FRAMES ? HOP_HEIGHT - 2 : HOP_HEIGHT);
    } else if (h->dir) {
        *x = h->from.col * CELL;
        *y = place_field_y(w, &h->from);
    }
    if (h->bump) {
        int k = h->bump > BUMP_FRAMES / 2 ? BUMP_FRAMES - h->bump : h->bump;   /* 0..4..0 */
        int dx = 0, dy = 0;
        switch (h->bdir) {
        case HOP_FWD: dx = 1; break;
        case HOP_BACK: dx = -1; break;
        case HOP_UP: dy = -1; break;
        case HOP_DOWN: dy = 1; break;
        }
        *x += dx * k;
        *y += dy * k;
    }
}

void duck_visual(const world *w, const duck *d, int *x, int *y, int *lift, int *squash16)
{
    hop_visual(w, &d->at, &d->h, 0, x, y, lift);
    int s = 16;
    if (d->h.dir) {
        int t = d->h.t;
        s = t < HOP_MID ? 16 + (STRETCH_16 - 16) * t / HOP_MID
                        : STRETCH_16 + (SQUASH_16 - STRETCH_16) * (t - HOP_MID) / (HOP_FRAMES - HOP_MID);
    } else if (d->land_t < 6) {
        s = SQUASH_16 + (16 - SQUASH_16) * d->land_t / 6;    /* the bounce back after landing */
    } else if (d->idle > 90) {
        int k = (d->idle - 90) % IDLE_PERIOD;                /* idle breathing */
        k = k < IDLE_PERIOD / 2 ? k : IDLE_PERIOD - k;
        s = 16 - k * 2 / (IDLE_PERIOD / 2);
    }
    *squash16 = s;
}

void duckling_visual(const world *w, const duck *parent, const duckling *k, int *x, int *y, int *lift)
{
    (void)parent;
    hop_visual(w, &k->at, &k->h, k->wait, x, y, lift);
}

/* ---- lost ducklings ------------------------------------------------------------------------------------------- */
static int loose_at(const world *w, int32_t col, int row, int also_flying)
{
    for (int i = 0; i < LOOSE_MAX; i++) {
        const loose *q = &w->ls[i];
        if ((q->state == LOOSE_WAIT || (also_flying && (q->state == LOOSE_FLY || q->state == LOOSE_TUMBLE))) &&
            q->col == col && q->row == row) return i;
    }
    return -1;
}

/* the nearest free grass cell on screen from world px (x, y) (field y); 0 if none */
static int nearest_grass(const world *w, int x, int y, int32_t *col, int *row)
{
    int cam_col = world_cam_px(w) / CELL, best = 1 << 30;
    for (int32_t c = cam_col + 1; c < cam_col + SCREEN_COLS; c++) {
        const lane *l = world_lane(w, c);
        if (!l || l->kind != LK_GRASS) continue;
        uint16_t fr = lane_free_rows(l);
        for (int r = 0; r < ROWS; r++) {
            if (!(fr >> r & 1) || loose_at(w, c, r, 1) >= 0) continue;
            int dx = c * CELL - x, dy = r * CELL - y;
            int dist = dx * dx + dy * dy + (dx < 0 ? 64 : 0);   /* ahead is a little better */
            if (dist < best) { best = dist; *col = c; *row = r; }
        }
    }
    return best < (1 << 30);
}

static void knock_off(world *w, int p, int k, int splash)
{
    duck *d = &w->d[p];
    int x, y, lift;
    duckling_visual(w, d, &d->line[k], &x, &y, &lift);
    for (int i = 0; i < LOOSE_MAX; i++)
        if (w->ls[i].state == LOOSE_FREE) {
            loose *q = &w->ls[i];
            memset(q, 0, sizeof *q);
            q->state = LOOSE_TUMBLE;
            q->x = x << 8;
            q->y = y << 8;
            q->col = -1;
            q->id = d->line[k].id;
            q->owner_was = (uint8_t)(p + 1 + (splash ? 4 : 0));
            break;
        }
    memmove(&d->line[k], &d->line[k + 1], sizeof(duckling) * (size_t)(d->nline - k - 1));
    d->nline--;
    d->catchup_t = 0;
    w->events[p] |= splash ? EV_SPLASHLING : EV_KNOCK;
    w->ev_knock_x[p] = x + CELL / 2 - world_cam_px(w);
}

static void loose_step(world *w)
{
    int cam = world_cam_px(w);
    for (int i = 0; i < LOOSE_MAX; i++) {
        loose *q = &w->ls[i];
        if (q->state == LOOSE_FREE) continue;
        q->t++;
        if (q->lock) q->lock--;
        switch (q->state) {
        case LOOSE_WAIT:
            if (q->col * CELL + CELL < cam) q->state = LOOSE_FREE;       /* left behind: it runs home */
            break;
        case LOOSE_TUMBLE:
            if (q->t >= KNOCK_FRAMES) {
                int32_t c;
                int r;
                if (nearest_grass(w, q->x >> 8, q->y >> 8, &c, &r)) {
                    q->state = LOOSE_FLY;
                    q->col = c;
                    q->row = (int8_t)r;
                    q->t = 0;
                } else {
                    q->state = LOOSE_FREE;
                }
            }
            break;
        case LOOSE_FLY: {
            int tx = (q->col * CELL) << 8, ty = (q->row * CELL) << 8;
            int dx = tx - q->x, dy = ty - q->y;
            int dist = absi(dx) > absi(dy) ? absi(dx) + absi(dy) / 2 : absi(dy) + absi(dx) / 2;
            if (dist <= FLUTTER_SPEED) {
                q->x = tx;
                q->y = ty;
                q->state = LOOSE_WAIT;
                q->lock = REJOIN_LOCK;
                q->owner_was = 0;
            } else {
                q->x += (int32_t)((int64_t)dx * FLUTTER_SPEED / dist);
                q->y += (int32_t)((int64_t)dy * FLUTTER_SPEED / dist);
            }
            const lane *l = world_lane(w, q->col);
            if (!l || l->kind != LK_GRASS || (q->x >> 8) + CELL < cam) q->state = LOOSE_FREE;
            break;
        }
        }
    }
}

/* ---- the parade ---------------------------------------------------------------------------------------------------- */
static void join(world *w, int p, int li)
{
    duck *d = &w->d[p];
    loose *q = &w->ls[li];
    if (d->nline >= line_cap(w->players)) return;         /* 24 a line; 16 with 3 parents, 12 with 4 (tuning.h) */
    /* the new duckling goes right behind the parent; the others keep their places (Snake grows at the head) */
    memmove(&d->line[1], &d->line[0], sizeof(duckling) * (size_t)d->nline);
    for (int k = 1; k <= d->nline; k++) d->line[k].lag++;
    duckling *n = &d->line[0];
    memset(n, 0, sizeof *n);
    n->lag = 1;
    n->at = d->at;
    n->id = q->id;
    d->nline++;
    q->state = LOOSE_FREE;
    w->events[p] |= EV_PICK;
    w->ev_pick_n[p] = d->nline;
}

static void check_pick(world *w, int p)
{
    duck *d = &w->d[p];
    if (d->h.dir || d->at.plat >= 0) return;
    int li = loose_at(w, d->at.col, d->at.y / CELL, 0);
    if (li >= 0 && !w->ls[li].lock) join(w, p, li);
}

static void bank(world *w, int p)
{
    duck *d = &w->d[p];
    int n = d->nline;
    w->bank_t[p] = 0;
    w->bank_col[p] = d->at.col;
    w->bank_row[p] = d->at.y / CELL;
    w->ev_bank_n[p] = n;
    if (!n) return;
    d->banked += bank_points(n);
    if (n > d->bank_best) d->bank_best = n;
    w->events[p] |= EV_BANK;
    /* the ducklings hop into the pond one after the other and swim there */
    int free_rows[ROWS], nf = 0;
    for (int r = 0; r < ROWS; r++) if (loose_at(w, d->at.col, r, 1) < 0 && r != d->at.y / CELL) free_rows[nf++] = r;
    for (int k = 0; k < n; k++) {
        int x, y, lift;
        duckling_visual(w, d, &d->line[k], &x, &y, &lift);
        for (int i = 0; i < LOOSE_MAX; i++)
            if (w->ls[i].state == LOOSE_FREE) {
                loose *q = &w->ls[i];
                memset(q, 0, sizeof *q);
                q->state = LOOSE_HOME;
                q->x = x << 8;
                q->y = y << 8;
                q->col = d->at.col;
                q->row = (int8_t)(nf ? free_rows[(k * 5 + 3) % nf] : d->at.y / CELL);
                q->t = (int16_t)(-k * 4);        /* one after the other */
                q->id = d->line[k].id;
                break;
            }
    }
    d->nline = 0;
    w->nest_ducklings += n;
}

/* the home-going ducklings (banked): fly into the nest pond and swim there */
static void home_step(world *w)
{
    int cam = world_cam_px(w);
    for (int i = 0; i < LOOSE_MAX; i++) {
        loose *q = &w->ls[i];
        if (q->state != LOOSE_HOME) continue;
        if (q->t < 0) continue;
        int tx = (q->col * CELL) << 8, ty = (q->row * CELL) << 8;
        int dx = tx - q->x, dy = ty - q->y;
        int dist = absi(dx) + absi(dy);
        if (dist > 2 * FLUTTER_SPEED) {
            q->x += (int32_t)((int64_t)dx * 2 * FLUTTER_SPEED / dist);
            q->y += (int32_t)((int64_t)dy * 2 * FLUTTER_SPEED / dist);
        } else {
            q->x = tx;
            q->y = ty;
        }
        if ((q->x >> 8) + CELL < cam) q->state = LOOSE_FREE;
    }
}

/* safe for a duckling to stand at place `pl` from frame t0 for `frames` frames? */
static int place_clear(const world *w, const place *pl, uint32_t t0, int frames)
{
    const lane *l = world_lane(w, pl->col);
    if (!l) return 0;
    for (int k = 0; k < frames; k++) {
        int y = place_y_at(w, pl, t0 + (uint32_t)k);
        if (y < 0 || y > FIELD_H - CELL) return 0;
        if (lane_hits(l, t0 + (uint32_t)k, y + (CELL - DUCKLING_HIT_H) / 2, y + (CELL + DUCKLING_HIT_H) / 2)) return 0;
    }
    return 1;
}

static void duckling_hop(duckling *k, const place *to, int wait)
{
    k->h.dir = HOP_FWD;
    k->h.t = 0;
    k->h.from = k->at;
    k->h.to = *to;
    k->wait = (int8_t)wait;
    k->h.dur = (int8_t)(HOP_FRAMES - wait);
}

static void line_step(world *w, int p)
{
    duck *d = &w->d[p];
    /* each duckling: its hop, then the checks */
    for (int k = 0; k < d->nline; k++) {
        duckling *u = &d->line[k];
        u->age++;
        if (u->h.dir) {
            if (u->wait > 0) u->wait--;
            else if (++u->h.t >= u->h.dur) {
                u->at = u->h.to;
                u->h.dir = 0;
                u->h.t = 0;
            }
        }
        int32_t col;
        int y;
        if (u->h.dir && u->wait == 0) hop_spot(w, &u->h, w->t, &col, &y);
        else { col = u->at.col; y = place_field_y(w, &u->at); }
        const lane *l = world_lane(w, col);
        int river_stand = l && l->kind == LK_RIVER && !(u->h.dir && u->wait == 0);
        if (!l || y < 0 || y > FIELD_H - CELL) { knock_off(w, p, k, 1); k--; continue; }
        if (lane_hits(l, w->t, y + (CELL - DUCKLING_HIT_H) / 2, y + (CELL + DUCKLING_HIT_H) / 2)) {
            knock_off(w, p, k, 0);
            k--;
            continue;
        }
        (void)river_stand;
        if ((col + 1) * CELL <= world_cam_px(w)) { knock_off(w, p, k, 0); k--; continue; }
    }
    /* close the gaps: the first duckling after a gap hurries one cell along the path when it is clear */
    int complete = 1;
    for (int k = 0; k < d->nline; k++) {
        duckling *u = &d->line[k];
        int want = k == 0 ? 1 : d->line[k - 1].lag + 1;
        if (u->lag <= want) continue;
        complete = 0;
        if (u->h.dir || d->catchup_t < CATCHUP_DELAY || u->lag - 1 >= d->nhist) continue;
        const place *to = &d->hist[u->lag - 1];
        if (place_clear(w, to, w->t + HOP_MID, CATCHUP_SAFE)) {
            u->lag--;
            duckling_hop(u, to, 0);
            d->catchup_t = CATCHUP_DELAY - 4;       /* the next one a little later */
        }
        break;                                      /* one at a time */
    }
    if (complete) d->catchup_t = 0;
    else d->catchup_t++;
}

/* the parent hopped: every duckling heads for its place on the path */
static void line_follow(world *w, int p)
{
    duck *d = &w->d[p];
    for (int k = 0; k < d->nline; k++) {
        duckling *u = &d->line[k];
        if (u->lag >= d->nhist || u->lag >= HIST) { knock_off(w, p, k, 0); k--; continue; }
        const place *to = &d->hist[u->lag];
        if (u->h.dir) { u->at = u->h.to; u->h.dir = 0; }      /* a hop in progress: finish it now */
        if (same_place(&u->at, to)) continue;
        duckling_hop(u, to, FOLLOW_STAGGER * (1 + k % 3));      /* a marching ripple: 2, 4, 6 frames */
    }
}

/* ---- the ducks ---------------------------------------------------------------------------------------------------- */
static void die(world *w, int p, int state)
{
    duck *d = &w->d[p];
    int x, y, lift, sq;
    duck_visual(w, d, &x, &y, &lift, &sq);
    d->death_col = x;
    d->death_y = y;
    d->state = state;
    d->t = 0;
    w->events[p] |= state == DK_HIT ? EV_HIT : state == DK_SWEPT ? EV_SWEPT : EV_CAUGHT;
    while (d->nline) knock_off(w, p, d->nline - 1, 0);
    w->events[p] &= ~(EV_KNOCK | EV_SPLASHLING);
}

static void push_hist(duck *d, const place *pl)
{
    memmove(&d->hist[1], &d->hist[0], sizeof(place) * (HIST - 1));
    d->hist[0] = *pl;
    if (d->nhist < HIST) d->nhist++;
}

static void start_hop(world *w, int p, int dir)
{
    duck *d = &w->d[p];
    const lane *here = world_lane(w, d->at.col);
    int y = place_field_y(w, &d->at);
    place to;
    int blocked = 0;
    if (dir == HOP_FWD || dir == HOP_BACK) {
        int32_t c2 = d->at.col + (dir == HOP_FWD ? 1 : -1);
        const lane *l2 = world_lane(w, c2);
        int y2 = y;
        if (!l2) blocked = 1;
        else if (lane_is_ground(l2)) {
            y2 = clampi((y + CELL / 2) / CELL, 0, ROWS - 1) * CELL;          /* snap to the nearest row */
            if ((l2->kind == LK_GRASS || l2->kind == LK_HEDGE) && (l2->block >> (y2 / CELL) & 1)) blocked = 1;
        }
        if (c2 * CELL < world_cam_px(w) - CELL / 2) blocked = 1;
        to = ground(c2, y2);
        if (!blocked) {
            d->h.from = ground(d->at.col, y);                             /* in the air: the y is kept */
            d->h.to = to;
        }
    } else {
        int dy = dir == HOP_UP ? -CELL : CELL;
        if (y + dy < 0 || y + dy > FIELD_H - CELL) blocked = 1;
        if (!blocked && here && lane_is_ground(here) && (here->kind == LK_GRASS || here->kind == LK_HEDGE) &&
            (here->block >> ((y + dy) / CELL) & 1)) blocked = 1;
        if (!blocked) {
            d->h.from = d->at;
            d->h.to = d->at;
            d->h.to.y = (int16_t)(d->at.y + dy);      /* on a platform: along it (it carries the duck) */
        }
    }
    if (blocked) {
        d->h.bump = BUMP_FRAMES;
        d->h.bdir = (int8_t)dir;
        w->events[p] |= EV_BUMP;
        return;
    }
    d->h.dir = (int8_t)dir;
    d->h.dur = HOP_FRAMES;
    d->h.t = 0;
    d->h.bump = 0;
    d->hops++;
    d->idle = 0;
    w->events[p] |= EV_HOP;
    if (!w->started) { w->started = 1; w->events[p] |= EV_START; }
    for (int q = 0; q < w->players; q++) if (w->d[q].state == DK_READY) w->d[q].state = DK_ALIVE;
    /* the path: where the duck will stand (on a river, relative to the platform it lands on) */
    place dest = d->h.to;
    const lane *l2 = world_lane(w, dest.col);
    if (l2 && l2->kind == LK_RIVER) {
        int yl = place_y_at(w, &dest, w->t + HOP_FRAMES);
        dest = on_river(w, dest.col, yl, w->t + HOP_FRAMES);
    }
    push_hist(d, &dest);
    line_follow(w, p);
}

static void land(world *w, int p)
{
    duck *d = &w->d[p];
    const lane *l = world_lane(w, d->h.to.col);
    int y = place_field_y(w, &d->h.to);
    d->h.dir = 0;
    d->h.t = 0;
    w->events[p] |= EV_LAND;
    d->land_t = 0;
    if (l && l->kind == LK_RIVER) {
        d->at = on_river(w, d->h.to.col, y, w->t);
        if (d->at.plat < 0 || y < 0 || y > FIELD_H - CELL) {
            die(w, p, DK_SWEPT);
            return;
        }
        w->events[p] |= EV_RIDE;
        d->hist[0] = d->at;
    } else {
        d->at = ground(d->h.to.col, y);
    }
    if (d->at.col > d->max_col) d->max_col = d->at.col;
    if (l && l->kind == LK_NEST) bank(w, p);
}

static void duck_step(world *w, int p, int press)
{
    duck *d = &w->d[p];
    d->t++;
    if (!alive(d)) {
        if (d->state != DK_OUT && d->t >= DEATH_ANIM) d->state = DK_OUT;
        return;
    }
    d->idle++;
    d->land_t++;
    if (d->h.bump) d->h.bump--;
    if (d->h.dir) {
        if (++d->h.t < HOP_FRAMES) {
            if (press && d->h.t >= HOP_FRAMES - HOP_BUFFER) d->queued = press;
        } else {
            land(w, p);
            if (!alive(d)) return;
        }
    }
    if (!d->h.dir) {
        int go = press ? press : d->queued;
        d->queued = 0;
        if (go && !d->h.bump) start_hop(w, p, go);
    }
    /* collisions at this frame */
    int32_t col;
    int y;
    if (d->h.dir) hop_spot(w, &d->h, w->t, &col, &y);
    else { col = d->at.col; y = place_field_y(w, &d->at); }
    const lane *l = world_lane(w, col);
    if (!l) return;
    if (lane_hits(l, w->t, y + (CELL - DUCK_HIT_H) / 2, y + (CELL + DUCK_HIT_H) / 2)) { die(w, p, DK_HIT); return; }
    if (!d->h.dir) {
        if (l->kind == LK_RIVER && (y < 0 || y > FIELD_H - CELL)) { die(w, p, DK_SWEPT); return; }
        if (l->kind == LK_POND && !(l->block >> (y / CELL) & 1)) { die(w, p, DK_SWEPT); return; }
        check_pick(w, p);
    }
}

/* ---- the camera and the fox ----------------------------------------------------------------------------------------- */
static void camera_step(world *w)
{
    if (!world_running(w)) return;
    /* the group: the parents still in the run (a hopping parent counts where it lands) */
    int32_t lead = -1;
    int64_t sum = 0;
    int n = 0;
    for (int p = 0; p < w->players; p++) {
        const duck *d = &w->d[p];
        if (!alive(d)) continue;
        int32_t c = d->h.dir ? d->h.to.col : d->at.col;
        if (c > lead) lead = c;
        sum += c;
        n++;
    }
    if (lead < 0) return;
    /* ease towards the group's mean column at the anchor (solo: the duck), never backwards, always creeping */
    int32_t target = (int32_t)(sum * CELL * 256 / n) - CAM_ANCHOR_COL * CELL * 256;
    int d256 = lanes_diff256(lead);
    int32_t creep = CREEP_START + (CREEP_MAX - CREEP_START) * d256 / 256;
    int32_t ease = (target - w->cam) * CAM_EASE_256 / 256;
    int32_t move = ease > creep ? ease : creep;
    if (n > 1) {
        /* co-op: the leader is not pushed off the right edge (a straggler does not hold the camera back) */
        int32_t lead_cam = (lead - CAM_LEAD_COL) * CELL * 256;
        int32_t pull = (lead_cam - w->cam) * CAM_LEAD_EASE_256 / 256;
        if (pull > move) move = pull;
    }
    w->cam += move;
    if (n > 1 && w->cam < (lead - CAM_LEAD_MAX_COL) * CELL * 256) w->cam = (lead - CAM_LEAD_MAX_COL) * CELL * 256;
}

static void fox_step(world *w)
{
    int cam = world_cam_px(w);
    for (int p = 0; p < w->players; p++) {
        duck *d = &w->d[p];
        if (!alive(d) || !w->started) { d->fox_warn = 0; continue; }
        int32_t col = d->h.dir && d->h.t >= HOP_MID ? d->h.to.col : d->h.dir ? d->h.from.col : d->at.col;
        int cx = col * CELL + CELL / 2;
        if (cx < cam + FOX_EDGE_PX) { die(w, p, DK_CAUGHT); continue; }
        int warn = cx < cam + FOX_WARN_COLS * CELL + CELL / 2;
        if (warn && !d->fox_warn) w->events[p] |= EV_FOXWARN;
        d->fox_warn = warn;
    }
}

void world_step(world *w, const int press[MAX_PLAYERS])
{
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    generate(w, 0);
    for (int p = 0; p < w->players; p++) duck_step(w, p, press[p]);
    for (int p = 0; p < w->players; p++) {
        if (alive(&w->d[p])) line_step(w, p);
        if (w->bank_t[p] >= 0) w->bank_t[p]++;
    }
    camera_step(w);
    fox_step(w);
    loose_step(w);
    home_step(w);
    w->t++;
}
