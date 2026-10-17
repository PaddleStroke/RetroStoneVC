/*
 * Caterpillar Crawl: the rules. No drawing, no sound: deterministic (integer maths, the world's own RNG), testable
 * alone (tests/test_rules.c). DESIGN.md "Rules" explains them; every number is in game.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
 *
 * 1 player: the caterpillar crawls the garden plot, one cell per step; a leaf makes it longer, a wall, an obstacle or
 * its own body ends the run. 12 leaves and it spins a cocoon and flies off as a butterfly: the next garden (the next
 * obstacle layout), a little caterpillar again, a little faster.
 * 2-4 players: the arena (the light cycles' rules): everyone crawls on the same clock and grows as it crawls; touching
 * any body, a wall or an obstacle is out; the last one crawling wins the round; the first to ARENA_WINS wins the match.
 */
#include "game.h"
#include <string.h>

static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};

/* ---- the layouts: '.' free; obstacles R rock, B bush, P flower pot, M mushroom ----------------------------------- */
/* 1 player: one per garden, in rotation. The start row (y 6, the head at x 5 crawling right) is always clear. */
static const char *const GARDENS[LAYOUTS][GRID_H] = {
    {"..................", "..................", "..................", "..................",
     "..................", "..................", "..................", "..................",
     "..................", "..................", "..................", ".................."},
    {"..................", "..................", "...RR........RR...", "...RR........RR...",
     "..................", "..................", "..................", "..................",
     "...RR........RR...", "...RR........RR...", "..................", ".................."},
    {"..................", "..................", "..................", "....BBBBBBBBBB....",
     "..................", "..................", "..................", "..................",
     "....BBBBBBBBBB....", "..................", "..................", ".................."},
    {"..................", "..................", "..P...P....P...P..", "..................",
     "........P.........", "..................", "..................", ".........P........",
     "..................", "..P...P....P...P..", "..................", ".................."},
    {"..................", ".MMM..........MMM.", ".M..............M.", "..................",
     "..................", "........RR........", "..................", "..................",
     "..................", ".M..............M.", ".MMM..........MMM.", ".................."},
    {"......B....B......", "......B....B......", "......B....B......", "......B....B......",
     "..................", "..................", "..................", "..................",
     "......B....B......", "......B....B......", "......B....B......", "......B....B......"},
};
/* the arena: one per round, in rotation; the four starts (a pinwheel along the edges) stay clear */
static const char *const ARENAS[ARENA_LAYOUTS][GRID_H] = {
    {"..................", "..................", "..................", "..................",
     "..................", "..................", "..................", "..................",
     "..................", "..................", "..................", ".................."},
    {"..................", "..................", "..................", "..................",
     "..................", "........RR........", "........RR........", "..................",
     "..................", "..................", "..................", ".................."},
    {"..................", "..................", "..................", "..................",
     ".....P......P.....", "..................", "..................", ".....P......P.....",
     "..................", "..................", "..................", ".................."},
    {"..................", "..................", "..................", "..................",
     "......B....B......", "......B....B......", "......B....B......", "......B....B......",
     "..................", "..................", "..................", ".................."},
};
/* the arena's starts: head x, y, heading (the body trails behind it) */
static const int ARENA_START[MAX_PLAYERS][3] = {
    {4, 1, DIR_RIGHT}, {13, 10, DIR_LEFT}, {16, 3, DIR_DOWN}, {1, 8, DIR_UP}};

static const char *layout_row(const world *w, int y)
{
    return w->mode == MODE_ARENA ? ARENAS[w->layout % ARENA_LAYOUTS][y] : GARDENS[w->layout % LAYOUTS][y];
}

int world_obstacle(const world *w, int cell)
{
    if (cell < 0 || cell >= CELLS) return OB_NONE;
    switch (layout_row(w, cell / GRID_W)[cell % GRID_W]) {
    case 'R': return OB_ROCK;
    case 'B': return OB_BUSH;
    case 'P': return OB_POT;
    case 'M': return OB_SHROOM;
    default: return OB_NONE;
    }
}

int cat_cell(const cat *c, int i) { return c->body[(c->head + i) % CELLS]; }

/* ---- effects (drawn by draw.c; part of the world so they replay the same) ---------------------------------------- */
static void fx_add(world *w, int kind, int x, int y, int value)
{
    int k = 0;
    for (int i = 0; i < FX_MAX; i++) {
        if (!w->fx[i].kind) { k = i; break; }
        if (w->fx[i].t > w->fx[k].t) k = i;           /* full: the oldest gives way */
    }
    effect *e = &w->fx[k];
    e->kind = (uint8_t)kind;
    e->t = 0;
    e->x = (int16_t)x;
    e->y = (int16_t)y;
    e->value = (int16_t)value;
}

static void fx_cell(world *w, int kind, int cell, int value)
{
    fx_add(w, kind, GRID_X + (cell % GRID_W) * CELL + CELL / 2, GRID_Y + (cell / GRID_W) * CELL + CELL / 2, value);
}

static void fx_step(world *w)
{
    static const uint8_t life[5] = {0, 18, 24, 48, 10};
    for (int i = 0; i < FX_MAX; i++) {
        effect *e = &w->fx[i];
        if (e->kind && ++e->t >= life[e->kind]) e->kind = 0;
    }
}

/* ---- caterpillars --------------------------------------------------------------------------------------------- */
int cat_turn(cat *c, int dir)
{
    if (dir < 0 || dir > 3) return 0;
    int last = c->nq ? c->queue[c->nq - 1] : c->dir;
    /* the same way, or straight back into itself: never (a U-turn takes two turns, two steps) */
    if (dir == last || dir == DIR_REV(last) || c->nq >= TURN_BUFFER) return 0;
    c->queue[c->nq++] = dir;
    return 1;
}

/* a caterpillar of len segments, its head at hx, hy heading dir, the body straight behind it */
static void cat_place(world *w, int p, int hx, int hy, int dir, int len, int state)
{
    cat *c = &w->c[p];
    int keep_wins = c->wins, score = c->score, leaves = c->leaves, fl = c->flowers, be = c->berries;
    int bf = c->butterflies;
    memset(c, 0, sizeof *c);
    c->wins = keep_wins;
    c->score = score, c->leaves = leaves, c->flowers = fl, c->berries = be, c->butterflies = bf;
    c->state = state;
    c->dir = c->mdir = dir;
    c->len = len;
    c->head = 0;
    c->eat_t = 99;
    for (int i = 0; i < len; i++) {
        int x = hx - DX[dir] * i, y = hy - DY[dir] * i, cell = y * GRID_W + x;
        c->body[i] = (uint8_t)cell;
        w->occ[cell] = (uint8_t)(p + 1);
    }
}

static void clear_plot(world *w)
{
    memset(w->occ, 0, sizeof w->occ);
    memset(w->fat, 0, sizeof w->fat);
    memset(w->fx, 0, sizeof w->fx);
    for (int i = 0; i < CELLS; i++)
        if (world_obstacle(w, i)) w->occ[i] = CELL_ROCK;
}

static int manhattan(int a, int b)
{
    int dx = a % GRID_W - b % GRID_W, dy = a / GRID_W - b / GRID_W;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

/* a free cell at least `far` steps from the head (uniform among them; closer ones if none), not `avoid` */
static int free_cell(world *w, int head, int avoid)
{
    for (int far = LEAF_MIN_DIST; far >= 0; far--) {
        int n = 0;
        for (int i = 0; i < CELLS; i++)
            if (!w->occ[i] && i != avoid && i != w->leaf && i != w->treat && manhattan(i, head) >= far) n++;
        if (!n) continue;
        int k = rs_rng_range(&w->rng, n);
        for (int i = 0; i < CELLS; i++)
            if (!w->occ[i] && i != avoid && i != w->leaf && i != w->treat && manhattan(i, head) >= far && k-- == 0)
                return i;
    }
    return -1;
}

void world_start_garden(world *w, int garden)
{
    w->mode = MODE_SOLO;
    w->garden = garden;
    w->layout = garden % LAYOUTS;
    clear_plot(w);
    w->leaf = w->treat = -1;
    w->treat_kind = TREAT_NONE;
    w->treat_t = w->slow_t = w->garden_leaves = 0;
    cat_place(w, 0, START_X, START_Y, DIR_RIGHT, START_LEN, garden == 0 && !w->started ? CS_READY : CS_WAIT);
    w->leaf = free_cell(w, cat_cell(&w->c[0], 0), -1);
    w->phase = garden == 0 && !w->started ? PH_READY : PH_GARDEN;
    w->phase_t = 0;
    w->acc = Q16_ONE - 1;
}

static void start_round(world *w, int round)
{
    w->round = round;
    w->round_winner = -1;
    w->layout = (round - 1) % ARENA_LAYOUTS;
    clear_plot(w);
    w->leaf = w->treat = -1;
    for (int p = 0; p < w->players; p++)
        cat_place(w, p, ARENA_START[p][0], ARENA_START[p][1], ARENA_START[p][2], ARENA_LEN,
                  w->started ? CS_WAIT : CS_READY);
    w->phase = w->started ? PH_ROUND : PH_READY;
    w->phase_t = 0;
    w->acc = Q16_ONE - 1;
    if (w->started) w->wevents |= WEV_ROUND;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->mode = w->players >= 2 ? MODE_ARENA : MODE_SOLO;
    if (w->mode == MODE_SOLO) world_start_garden(w, 0);
    else start_round(w, 1);
}

int world_speed(const world *w)
{
    if (w->mode == MODE_ARENA) return ARENA_SPEED;
    int s = SPEED_START + w->garden * SPEED_GARDEN + (w->c[0].len - START_LEN) * SPEED_LEN;
    if (s > SPEED_MAX) s = SPEED_MAX;
    if (w->slow_t > 0) s = s * SLOW_PCT / 100;
    return s;
}

static int32_t speed_q16(const world *w) { return (int32_t)((int64_t)world_speed(w) * Q16_ONE / 6000); }

int world_will_step(const world *w)
{
    if (!w->started) return 0;
    switch (w->phase) {
    case PH_PLAY: return w->acc + speed_q16(w) >= Q16_ONE;
    case PH_GARDEN: return w->phase_t + 1 >= GARDEN_PAUSE;
    case PH_ROUND: return w->phase_t + 1 >= ROUND_PAUSE;
    default: return 0;
    }
}

int world_running(const world *w) { return w->phase != PH_OVER; }
int world_over_t(const world *w) { return w->phase == PH_OVER ? w->phase_t : -1; }
int world_rank_key(const world *w, int p) { return w->c[p].wins; }

/* ---- the step: every crawling caterpillar moves one cell, at once ---------------------------------------------- */
static void eat(world *w, int p, int cell)
{
    cat *c = &w->c[p];
    if (cell == w->leaf) {
        c->score++;
        c->leaves++;
        c->grow += GROW_PER_LEAF;
        c->eat_t = 0;
        w->fat[cell] = 1;
        w->garden_leaves++;
        w->events[p] |= EV_LEAF;
        fx_cell(w, FX_LEAF, cell, 0);
        w->leaf = -1;
        if (w->garden_leaves >= LEAVES_PER_GARDEN) {          /* the cocoon: the butterfly comes */
            w->phase = PH_META;
            w->phase_t = 0;
            w->meta_len = c->len;
            w->meta_cell = cell;
            c->state = CS_COCOON;
            c->t = 0;
            c->grow = 0;
            c->nq = 0;
            if (w->treat >= 0) fx_cell(w, FX_PUFF, w->treat, 0);
            w->treat = -1;
            w->treat_kind = TREAT_NONE;
            w->slow_t = 0;
            w->wevents |= WEV_COCOON;
            return;
        }
        w->leaf = free_cell(w, cell, -1);
        if (w->treat < 0 && (c->leaves % FLOWER_EVERY == 0 || c->leaves % BERRY_EVERY == 0)) {
            w->treat_kind = c->leaves % FLOWER_EVERY == 0 ? TREAT_FLOWER : TREAT_BERRY;
            w->treat = free_cell(w, cell, -1);
            w->treat_t = 0;
            if (w->treat >= 0) w->wevents |= WEV_TREAT;
        }
    } else if (cell == w->treat) {
        if (w->treat_kind == TREAT_FLOWER) {
            c->score += FLOWER_POINTS;
            c->flowers++;
            w->events[p] |= EV_FLOWER;
            fx_cell(w, FX_POINTS, cell, FLOWER_POINTS);
        } else {
            c->berries++;
            w->slow_t = SLOW_FRAMES;
            w->events[p] |= EV_BERRY;
        }
        fx_cell(w, FX_SPARK, cell, 0);
        w->treat = -1;
        w->treat_kind = TREAT_NONE;
    }
}

static void crash(world *w, int p, int why)
{
    cat *c = &w->c[p];
    c->state = CS_HIT;
    c->t = 0;
    c->why = why;
    c->nq = 0;
    w->events[p] |= EV_HIT;
}

static void round_over(world *w)
{
    int alive = 0, last = -1;
    for (int p = 0; p < w->players; p++)
        if (w->c[p].state == CS_CRAWL) alive++, last = p;
    if (alive > 1) return;
    w->round_winner = last;
    if (last >= 0) {
        w->c[last].wins++;
        w->c[last].state = CS_WAIT;
        w->c[last].t = 0;
        w->events[last] |= EV_WIN;
    }
    int done = w->round >= ARENA_MAX_ROUNDS;
    for (int p = 0; p < w->players; p++)
        if (w->c[p].wins >= ARENA_WINS) done = 1;
    w->phase = done ? PH_OVER : PH_ROUND_END;
    w->phase_t = 0;
    w->wevents |= done ? WEV_OVER : WEV_ROUND_END;
}

static void step_all(world *w)
{
    int target[MAX_PLAYERS], why[MAX_PLAYERS], moving[MAX_PLAYERS], freed[MAX_PLAYERS];
    w->stepped = 1;
    w->wevents |= WEV_STEP;
    for (int p = 0; p < w->players; p++) {
        cat *c = &w->c[p];
        target[p] = -1;
        why[p] = 0;
        freed[p] = -1;
        moving[p] = c->state == CS_CRAWL;
        if (!moving[p]) continue;
        if (c->nq) {                                    /* the next buffered turn */
            c->dir = c->queue[0];
            c->nq--;
            memmove(c->queue, c->queue + 1, sizeof(int) * TURN_BUFFER - sizeof(int));
            w->events[p] |= EV_TURN;
        }
        int h = cat_cell(c, 0), x = h % GRID_W + DX[c->dir], y = h / GRID_W + DY[c->dir];
        if (x < 0 || y < 0 || x >= GRID_W || y >= GRID_H) why[p] = HIT_WALL;
        else target[p] = y * GRID_W + x;
    }
    /* the tails leave first (crawling into the cell a tail leaves this very step is fine) */
    for (int p = 0; p < w->players; p++) {
        cat *c = &w->c[p];
        if (!moving[p] || c->grow > 0) continue;
        int t = cat_cell(c, c->len - 1);
        if (w->occ[t] == p + 1) w->occ[t] = CELL_FREE;
        freed[p] = t;
    }
    for (int p = 0; p < w->players; p++) {
        if (!moving[p] || why[p]) continue;
        int v = w->occ[target[p]];
        if (v == CELL_ROCK) why[p] = HIT_ROCK;
        else if (v == p + 1) why[p] = HIT_SELF;
        else if (v) why[p] = HIT_OTHER;
    }
    for (int p = 0; p < w->players; p++)            /* two heads into the same cell: both out */
        for (int q = p + 1; q < w->players; q++)
            if (moving[p] && moving[q] && target[p] >= 0 && target[p] == target[q]) {
                if (!why[p]) why[p] = HIT_OTHER;
                if (!why[q]) why[q] = HIT_OTHER;
            }
    for (int p = 0; p < w->players; p++) {
        cat *c = &w->c[p];
        if (!moving[p] || why[p]) continue;
        int cell = target[p];
        w->occ[cell] = (uint8_t)(p + 1);
        c->head = (c->head + CELLS - 1) % CELLS;
        c->body[c->head] = (uint8_t)cell;
        if (c->grow > 0) {
            c->grow--;
            c->len++;
            w->events[p] |= EV_GROW;
        } else if (freed[p] >= 0 && (w->occ[freed[p]] == CELL_FREE || freed[p] == cell)) {
            w->fat[freed[p]] = 0;                       /* the tail passed (its own head may come in at once) */
        }
        c->mdir = c->dir;
        c->crawled++;
        if (w->mode == MODE_ARENA && c->crawled % ARENA_GROW_EVERY == 0) c->grow++;
        if (w->mode == MODE_SOLO) eat(w, p, cell);
    }
    for (int p = 0; p < w->players; p++) {
        if (!moving[p] || !why[p]) continue;
        cat *c = &w->c[p];
        if (freed[p] >= 0) {                        /* it did not move: its tail stays (or drops, if taken) */
            if (w->occ[freed[p]] == CELL_FREE) w->occ[freed[p]] = (uint8_t)(p + 1);
            else c->len--;
        }
        crash(w, p, why[p]);
    }
    if (w->mode == MODE_SOLO) {
        if (w->c[0].state == CS_HIT) {
            w->phase = PH_OVER;
            w->phase_t = 0;
            w->wevents |= WEV_OVER;
        }
    } else {
        round_over(w);
    }
}

static void advance(world *w)
{
    w->acc += speed_q16(w);
    if (w->acc < Q16_ONE) return;
    w->acc -= Q16_ONE;
    step_all(w);
}

static void go_crawl(world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->c[p].state == CS_WAIT || w->c[p].state == CS_READY) {
            w->c[p].state = CS_CRAWL;
            w->c[p].t = 0;
        }
    w->phase = PH_PLAY;
    w->phase_t = 0;
    w->acc = Q16_ONE - 1;                           /* the first step at once: the head never stands still */
    w->wevents |= WEV_GO;
}

/* the butterfly: the body curls into a cocoon at the head, which hatches and flutters off; then the next garden */
static void meta_step(world *w)
{
    cat *c = &w->c[0];
    int t = w->phase_t;
    if (t <= META_SHRINK) {
        int want = 1 + (w->meta_len - 1) * (META_SHRINK - t) / META_SHRINK;
        while (c->len > want) {
            int cell = cat_cell(c, c->len - 1);
            if (w->occ[cell] == 1) w->occ[cell] = CELL_FREE;
            w->fat[cell] = 0;
            if (c->len % 2) fx_cell(w, FX_PUFF, cell, 0);
            c->len--;
        }
    }
    if (t == META_COCOON) {
        c->butterflies++;
        c->score += BUTTERFLY_POINTS;
        fx_cell(w, FX_POINTS, w->meta_cell, BUTTERFLY_POINTS);
        fx_cell(w, FX_SPARK, w->meta_cell, 0);
        w->wevents |= WEV_BUTTERFLY;
    }
    if (t >= META_FRAMES) {
        world_start_garden(w, w->garden + 1);
        w->wevents |= WEV_GARDEN;
    }
}

static void start(world *w)
{
    w->started = 1;
    for (int p = 0; p < w->players; p++) w->events[p] |= EV_START;
    if (w->mode == MODE_SOLO) {
        if (w->phase == PH_READY) go_crawl(w);
        else w->phase_t = 0;                        /* a later garden (the option garden=N): GARDEN N first */
    } else {
        for (int p = 0; p < w->players; p++) w->c[p].state = CS_WAIT;
        w->phase = PH_ROUND;
        w->phase_t = 0;
        w->wevents |= WEV_ROUND;
    }
}

static void cats_tick(world *w)
{
    for (int p = 0; p < w->players; p++) {
        cat *c = &w->c[p];
        c->t++;
        if (c->eat_t < 999) c->eat_t++;
        if (c->state == CS_HIT && c->t >= HIT_FRAMES) {
            c->state = w->mode == MODE_ARENA ? CS_MELT : CS_DOWN;
            c->t = 0;
        } else if (c->state == CS_MELT && c->t % MELT_EVERY == 0) {
            if (c->len > 0) {
                int cell = cat_cell(c, c->len - 1);
                if (w->occ[cell] == p + 1) w->occ[cell] = CELL_FREE;
                w->fat[cell] = 0;
                if (c->len % 2 || c->len == 1) fx_cell(w, FX_PUFF, cell, 0);
                c->len--;
            }
            if (c->len <= 0) {
                c->state = CS_DOWN;
                c->t = 0;
            }
        }
    }
}

void world_step(world *w, const int press[MAX_PLAYERS])
{
    w->wevents = 0;
    w->stepped = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    fx_step(w);
    int any = 0;
    for (int p = 0; p < w->players; p++) {
        cat *c = &w->c[p];
        if (press[p]) any = 1;
        if (PRESS_DIR(press[p]) >= 0 && (c->state == CS_READY || c->state == CS_CRAWL || c->state == CS_WAIT))
            cat_turn(c, PRESS_DIR(press[p]));
    }
    if (!w->started) {
        if (!any) {
            w->t++;
            return;
        }
        start(w);
    }
    cats_tick(w);
    w->phase_t++;
    switch (w->phase) {
    case PH_PLAY:
        if (w->slow_t > 0) w->slow_t--;
        if (w->treat >= 0 && ++w->treat_t >= TREAT_LIFE) {
            fx_cell(w, FX_PUFF, w->treat, 0);
            w->treat = -1;
            w->treat_kind = TREAT_NONE;
            w->wevents |= WEV_TREAT_GONE;
        }
        advance(w);
        break;
    case PH_META:
        meta_step(w);
        break;
    case PH_GARDEN:
        if (w->phase_t >= GARDEN_PAUSE) {
            go_crawl(w);
            advance(w);
        }
        break;
    case PH_ROUND:
        if (w->phase_t >= ROUND_PAUSE) {
            go_crawl(w);
            advance(w);
        }
        break;
    case PH_ROUND_END:
        if (w->phase_t >= ROUND_END) start_round(w, w->round + 1);
        break;
    default:
        break;
    }
    w->t++;
}

/* ---- the bot (tests, screenshots, the card frames): it plays from the world, never the pad ------------------------ */
/* the cells a flood from `from` reaches (free cells; `skip` counts as free: a tail leaving), at most `cap` */
static int flood(const uint8_t *occ, int from, int cap, int *dist, int target)
{
    static const int NB[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    uint8_t seen[CELLS];
    int qu[CELLS], qh = 0, qt = 0, n = 0;
    memset(seen, 0, sizeof seen);
    seen[from] = 1;
    qu[qt++] = from;
    int d[CELLS];
    d[from] = 0;
    if (dist) *dist = from == target ? 0 : 9999;
    while (qh < qt && n < cap) {
        int c = qu[qh++];
        n++;
        for (int k = 0; k < 4; k++) {
            int x = c % GRID_W + NB[k][0], y = c / GRID_W + NB[k][1];
            if (x < 0 || y < 0 || x >= GRID_W || y >= GRID_H) continue;
            int m = y * GRID_W + x;
            if (seen[m] || occ[m]) continue;
            seen[m] = 1;
            d[m] = d[c] + 1;
            if (dist && m == target && *dist == 9999) *dist = d[m];
            qu[qt++] = m;
        }
    }
    return n;
}

static uint32_t bot_hash(const world *w, int p, int salt)
{
    uint32_t h = w->seed * 2654435761u ^ ((uint32_t)w->t * 40503u + (uint32_t)p * 7919u + (uint32_t)salt * 104729u);
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

int bot_decide(const world *w, int p, int skill, int stop)
{
    const cat *c = &w->c[p];
    if (!w->started) return (w->t % 40) == 20 ? PRESS_GO : 0;
    if (c->state != CS_CRAWL && c->state != CS_WAIT) return 0;
    if (!world_will_step(w)) return 0;
    int head = cat_cell(c, 0), best = -1, best_score = -2000000000;
    uint8_t occ[CELLS], safe[CELLS];
    memcpy(occ, w->occ, sizeof occ);
    if (c->grow == 0 && c->len > 1) occ[cat_cell(c, c->len - 1)] = CELL_FREE;   /* its tail moves on */
    memcpy(safe, occ, sizeof safe);
    if (w->mode == MODE_ARENA)                      /* the cells the others may crawl into next are dangerous */
        for (int q = 0; q < w->players; q++) {
            const cat *o = &w->c[q];
            if (q == p || o->state != CS_CRAWL) continue;
            int oh = cat_cell(o, 0);
            for (int k = 0; k < 4; k++) {
                int x = oh % GRID_W + DX[k], y = oh / GRID_W + DY[k];
                if (k != DIR_REV(o->dir) && x >= 0 && y >= 0 && x < GRID_W && y < GRID_H && !safe[y * GRID_W + x])
                    safe[y * GRID_W + x] = CELL_ROCK;
            }
        }
    /* a human-like bot (skill 1..10) sometimes reacts a step late: it goes on straight */
    if (skill > 0 && (int)(bot_hash(w, p, 1) % 100) < skill * 2) return 0;
    int target = w->leaf;
    if (w->treat >= 0 && (target < 0 || manhattan(head, w->treat) < manhattan(head, target)) &&
        TREAT_LIFE - w->treat_t > manhattan(head, w->treat) * 12)
        target = w->treat;
    for (int d = 0; d < 4; d++) {
        if (d == DIR_REV(c->dir)) continue;
        int x = head % GRID_W + DX[d], y = head / GRID_W + DY[d];
        int blocked = x < 0 || y < 0 || x >= GRID_W || y >= GRID_H || occ[y * GRID_W + x];
        int score;
        if (stop) {
            score = blocked ? 1000 : d == c->dir ? 10 : 0;       /* botstop: into the first wall or body */
        } else if (blocked) {
            score = -1000000 + (d == c->dir);
        } else {
            int n = y * GRID_W + x, dist = 9999;
            if (w->mode == MODE_ARENA) {
                /* the most room; then hugging walls and bodies (it fills its room well), each bot its own way */
                int area = flood(safe, n, CELLS, NULL, -1), hug = 0;
                for (int k = 0; k < 4; k++) {
                    int nx = x + DX[k], ny = y + DY[k];
                    hug += nx < 0 || ny < 0 || nx >= GRID_W || ny >= GRID_H || occ[ny * GRID_W + nx];
                }
                score = area * 64 + hug * 8 + (d == c->dir ? 4 : 0) + (int)(bot_hash(w, p, d) % 16) -
                        (safe[n] ? 1000000 : 0);
            } else {
                int area = flood(occ, n, CELLS, &dist, target);
                int need = c->len + c->grow + 2;
                int greedy = skill >= 3;            /* a beginner only looks one step ahead */
                if (greedy) score = (area >= 3 ? 5000 : area * 100) - (target >= 0 ? manhattan(n, target) * 10 : 0);
                else score = (area >= need ? 50000 : area * 100) - (dist == 9999 ? 500 : dist * 10);
                if (d == c->dir) score += 3;
                if (skill > 0) score += (int)(bot_hash(w, p, d + 9) % (unsigned)(skill * 4 + 1));
            }
        }
        if (score > best_score) best_score = score, best = d;
    }
    if (best < 0 || best == c->dir) return 0;
    return 1 + best;
}
