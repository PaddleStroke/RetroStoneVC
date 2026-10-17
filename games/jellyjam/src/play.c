/*
 * Jelly Jam: the rules (the fields, the falling pairs, melting and chains, the rain, duels and rounds), the computer
 * chefs' brains and the fields' layout. No drawing, no sound, no screen width: deterministic, testable alone
 * (tests/test_rules.c links this file with the SDK's RNG only).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
 *
 * Heights are in 1/256 of a cell (CQ) from the field's floor; row 0 is the bottom, rows 0..11 are on screen, row 12 is
 * the hidden row. A field's columns are always packed from the bottom (gravity): a falling jelly is already in its
 * final cell and only DRAWN higher by its offset (off[][]), which shrinks as it falls.
 */
#include "game.h"
#include <string.h>

static const int8_t CDX[4] = {0, 1, 0, -1}, CDY[4] = {1, 0, -1, 0};    /* the child: up, right, down, left */

/* ---- the bare grid ------------------------------------------------------------------------------------------------ */
static int gheight(const uint8_t g[ROWS][COLS], int c)
{
    int h = 0;
    while (h < ROWS && g[h][c]) h++;
    return h;
}

int field_height(const field *f, int col) { return gheight(f->g, col); }

int field_count(const field *f)
{
    int n = 0;
    for (int c = 0; c < COLS; c++) n += gheight(f->g, c);
    return n;
}

static void gput(uint8_t g[ROWS][COLS], int c, int v)
{
    int h = gheight(g, c);
    if (h < ROWS) g[h][c] = (uint8_t)v;             /* above the hidden row: lost */
}

int grid_place(uint8_t g[ROWS][COLS], int col, int rot, int c0, int c1)
{
    int cc = col + CDX[rot & 3];
    if (col < 0 || col >= COLS || cc < 0 || cc >= COLS) return 0;
    if ((rot & 3) == 2) {                           /* the child below: it lands first */
        gput(g, col, c1);
        gput(g, col, c0);
    } else {
        gput(g, col, c0);
        gput(g, cc, c1);
    }
    return 1;
}

static void compact(uint8_t g[ROWS][COLS])
{
    for (int c = 0; c < COLS; c++) {
        int w = 0;
        for (int r = 0; r < ROWS; r++)
            if (g[r][c]) g[w++][c] = g[r][c];
        while (w < ROWS) g[w++][c] = 0;
    }
}

/* The visible rows as 6-bit masks, one set per cell kind: m[k][r] has bit c when g[r][c] == k (k 0 = empty). The groups
 * are found by growing regions on these masks: a few passes of shifts and ands, no stacks (the brains do it a lot). */
static void bitrows(const uint8_t g[ROWS][COLS], uint8_t m[RAIN + 1][VROWS])
{
    memset(m, 0, sizeof(uint8_t) * (RAIN + 1) * VROWS);
    for (int r = 0; r < VROWS; r++)
        for (int c = 0; c < COLS; c++) m[g[r][c] <= RAIN ? g[r][c] : 0][r] |= (uint8_t)(1 << c);
}

/* grows the region reg (bit rows) through the cells of `in` (orthogonal neighbours) until it stops; returns its size */
static int grow(uint8_t reg[VROWS], const uint8_t in[VROWS])
{
    int changed, n = 0;
    do {
        changed = 0;
        for (int r = 0; r < VROWS; r++) {
            uint8_t x = reg[r], nb = (uint8_t)(x | (x << 1) | (x >> 1));
            if (r > 0) nb |= reg[r - 1];
            if (r + 1 < VROWS) nb |= reg[r + 1];
            nb = (uint8_t)((nb & in[r]) | x);
            if (nb != x) {
                reg[r] = nb;
                changed = 1;
            }
        }
    } while (changed);
    for (int r = 0; r < VROWS; r++) n += __builtin_popcount(reg[r]);
    return n;
}

/* The groups of MELT_MIN or more jellies of one colour touching (on screen only): marks them (1) and the rain jellies
 * next to them (2); returns the colour jellies, the colours, the groups' sizes. */
static int groups(const uint8_t g[ROWS][COLS], uint8_t mark[ROWS][COLS], int *ncol, int sizes[], int *ngroups)
{
    uint8_t m[RAIN + 1][VROWS], hit[VROWS];
    int total = 0, nc = 0, ng = 0;
    bitrows(g, m);
    memset(hit, 0, sizeof hit);
    memset(mark, 0, sizeof(uint8_t) * ROWS * COLS);
    for (int k = 1; k <= COLOURS; k++) {
        uint8_t rem[VROWS];
        int any = 0;
        memcpy(rem, m[k], sizeof rem);
        for (int r = 0; r < VROWS; r++)
            while (rem[r]) {
                uint8_t reg[VROWS];
                memset(reg, 0, sizeof reg);
                reg[r] = (uint8_t)(rem[r] & -rem[r]);
                int n = grow(reg, m[k]);
                for (int q = 0; q < VROWS; q++) rem[q] &= (uint8_t)~reg[q];
                if (n < MELT_MIN) continue;
                for (int q = 0; q < VROWS; q++) hit[q] |= reg[q];
                total += n;
                any = 1;
                if (ng < 16) sizes[ng++] = n;
            }
        nc += any;
    }
    if (total)
        for (int r = 0; r < VROWS; r++) {
            uint8_t x = hit[r], nb = (uint8_t)((x << 1) | (x >> 1));
            if (r > 0) nb |= hit[r - 1];
            if (r + 1 < VROWS) nb |= hit[r + 1];
            nb &= m[RAIN][r];
            for (int c = 0; c < COLS; c++)
                mark[r][c] = (uint8_t)((x >> c) & 1 ? 1 : (nb >> c) & 1 ? 2 : 0);
        }
    *ncol = nc;
    *ngroups = ng;
    return total;
}

int melt_points(int chain, int jellies, int colours, const int *grp, int ngroups)
{
    static const int cp[CHAIN_POWER_N] = CHAIN_POWER, cb[COLOURS] = COLOUR_BONUS, gb[GROUP_BONUS_N] = GROUP_BONUS;
    int b = cp[chain < 1 ? 0 : chain > CHAIN_POWER_N ? CHAIN_POWER_N - 1 : chain - 1];
    if (colours >= 1) b += cb[colours > COLOURS ? COLOURS - 1 : colours - 1];
    for (int i = 0; i < ngroups; i++) {
        int k = grp[i] - MELT_MIN;
        b += gb[k < 0 ? 0 : k >= GROUP_BONUS_N ? GROUP_BONUS_N - 1 : k];
    }
    if (b < 1) b = 1;
    if (b > 999) b = 999;
    return 10 * jellies * b;
}

int grid_resolve(uint8_t g[ROWS][COLS], int *points, int *melted)
{
    uint8_t mark[ROWS][COLS];
    int chain = 0, pts = 0, mel = 0, ncol, sizes[16], ng;
    for (;;) {
        compact(g);
        int n = groups(g, mark, &ncol, sizes, &ng);
        if (!n) break;
        chain++;
        pts += melt_points(chain, n, ncol, sizes, ng);
        mel += n;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (mark[r][c]) g[r][c] = 0;
    }
    if (points) *points = pts;
    if (melted) *melted = mel;
    return chain;
}

/* ---- the shared sequence of pairs: every field gets the same pairs in the same order ---------------------------- */
static uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

void seq_pair(uint32_t seed, int i, uint8_t *c0, uint8_t *c1)
{
    uint32_t h = mix(seed * 0x9e3779b9u + (uint32_t)i * 0x85ebca6bu + 0x51ed27u);
    *c0 = (uint8_t)(1 + h % COLOURS);
    *c1 = (uint8_t)(1 + (h >> 11) % COLOURS);
}

/* ---- the falling pair --------------------------------------------------------------------------------------------- */
int pair_floor(const field *f, const pair_t *p)
{
    int y = gheight(f->g, p->col) * CQ;
    if (p->rot == 2) y += CQ;                       /* the child is below the pivot */
    else if (p->rot & 1) {
        int cc = p->col + CDX[p->rot];
        int y2 = cc >= 0 && cc < COLS ? gheight(f->g, cc) * CQ : 0;    /* a child off the field (the quick flip at a
                                                                          wall asks): pair_fits refuses it */
        if (y2 > y) y = y2;
    }
    return y;
}

static int pair_fits(const field *f, const pair_t *p)
{
    int cc = p->col + CDX[p->rot];
    if (p->col < 0 || p->col >= COLS || cc < 0 || cc >= COLS) return 0;
    if (p->y < pair_floor(f, p)) return 0;
    return p->y + (p->rot == 0 ? CQ : 0) <= ROWS * CQ;   /* the upper jelly: one row above the hidden row at most */
}

/* a move or a turn on the ground restarts the landing wait (LOCK_RESETS times at most) */
static void ground_reset(field *f)
{
    if (f->pr.lock_t > 0 && f->pr.resets < LOCK_RESETS) {
        f->pr.lock_t = 0;
        f->pr.resets++;
    }
}

int pair_move(field *f, int dir)
{
    pair_t p = f->pr;
    p.col = (int8_t)(p.col + dir);
    if (!pair_fits(f, &p)) return 0;
    p.slide = (int8_t)dir;
    p.slide_t = 0;
    f->pr = p;
    ground_reset(f);
    return 1;
}

int pair_turn(field *f, int dir)
{
    pair_t p = f->pr;
    int from = p.rot, ok = 1;
    /* a climb (the floor kick, the flip) lifts the pair off the ground and so restarts its landing wait: it counts as
     * one of the LOCK_RESETS restarts, and none is left once they are used up (turning round and round on the floor
     * would hold the pair up for ever) */
    int climb = f->pr.resets < LOCK_RESETS;
    p.rot = (int8_t)((p.rot + dir) & 3);
    if (!pair_fits(f, &p)) {
        ok = 0;
        if (p.rot & 1) {                            /* the child's side is blocked: the wall kick pushes the pair away */
            pair_t q = p;
            q.col = (int8_t)(q.col - CDX[p.rot]);
            if (pair_fits(f, &q)) { p = q; ok = 1; }
        } else if (p.rot == 2) {                    /* the child turns into the floor or the stack: the pair climbs */
            pair_t q = p;
            q.y = pair_floor(f, &q);
            if (climb && q.y - f->pr.y <= CQ && pair_fits(f, &q)) { p = q; ok = 1; }
        }
        if (!ok && f->pr.qturn) {                   /* blocked on both sides, pressed again quickly: the pair flips */
            pair_t q = f->pr;
            q.rot = (int8_t)((q.rot + 2) & 3);
            int fl = pair_floor(f, &q);
            if (q.y < fl) q.y = fl;
            if ((climb || q.y == f->pr.y) && pair_fits(f, &q)) { p = q; ok = 1; }
        }
        if (!ok) {
            f->pr.qturn = QUICK_TURN;
            return 0;
        }
    }
    p.rot_from = (int8_t)from;
    p.rot_t = 0;
    p.qturn = 0;
    if (p.y > f->pr.y) {                            /* it climbed */
        p.lock_t = 0;
        p.resets++;
        f->pr = p;
        return 1;
    }
    f->pr = p;
    ground_reset(f);
    return 1;
}

/* ---- a field's turn ---------------------------------------------------------------------------------------------- */
void field_reset(field *f)
{
    uint8_t kind = f->kind, level = (uint8_t)f->level;
    int16_t wins = f->wins;
    memset(f, 0, sizeof *f);
    f->kind = kind;
    f->level = (int8_t)level;
    f->wins = wins;
    f->alive = 1;
    f->state = FS_IDLE;
    f->target = -1;
    f->chain_t = f->pop_age = 999;
}

static void put_cell(field *f, int col, int yb, int colour)
{
    int h = gheight(f->g, col);
    if (h >= ROWS) return;                          /* above the hidden row: lost */
    int d = yb - h * CQ;
    f->g[h][col] = (uint8_t)colour;
    f->off[h][col] = (int16_t)(d > 0 ? d : 0);
    f->sq[h][col] = 0;
}

static void lock_pair(world *w, int i)
{
    field *f = &w->f[i];
    pair_t *p = &f->pr;
    int cc = p->col + CDX[p->rot], cy = p->y + CDY[p->rot] * CQ;
    if (p->rot == 2) {                              /* the lower jelly first */
        put_cell(f, cc, cy, p->c1);
        put_cell(f, p->col, p->y, p->c0);
    } else {
        put_cell(f, p->col, p->y, p->c0);
        put_cell(f, cc, cy, p->c1);
    }
    f->state = FS_SETTLE;
    f->t = 0;
    f->fall_v = SETTLE_V0;
    w->events[i] |= EV_LOCK;
}

/* returns 1 while something falls */
static int settle(field *f, int *ev)
{
    int moving = 0;
    f->fall_v = (int16_t)(f->fall_v + SETTLE_ACCEL > SETTLE_VMAX ? SETTLE_VMAX : f->fall_v + SETTLE_ACCEL);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (f->off[r][c] <= 0) continue;
            f->off[r][c] = (int16_t)(f->off[r][c] - f->fall_v);
            if (f->off[r][c] <= 0) {
                f->off[r][c] = 0;
                f->sq[r][c] = SQUASH_FRAMES;
                *ev |= EV_LAND;
            } else {
                moving = 1;
            }
        }
    return moving;
}

int pick_target(const world *w, int from)
{
    int best = -1, bv = 0;
    for (int k = 1; k < w->nf; k++) {
        int i = (from + k) % w->nf;
        const field *f = &w->f[i];
        if (!f->alive) continue;
        int v = field_count(f) + f->rain_q;         /* the leader: the field with the least on it (and coming) */
        if (best < 0 || v < bv) { best = i; bv = v; }
    }
    return best;
}

void rain_send(world *w, int from, int n)
{
    field *f = &w->f[from];
    if (n <= 0) return;
    if (f->rain_q > 0) {                            /* the offset: your melts cancel the rain coming to you first */
        int k = n < f->rain_q ? n : f->rain_q;
        f->rain_q -= k;
        n -= k;
        w->events[from] |= EV_OFFSET;
    }
    if (n <= 0) return;
    int t = pick_target(w, from);
    if (t < 0) return;
    w->f[t].rain_q += n;
    f->sent += n;
    f->target = (int16_t)t;
    w->events[from] |= EV_SENT;
}

static int drop_rain(world *w, field *f)
{
    int n = f->rain_q < RAIN_MAX_DROP ? f->rain_q : RAIN_MAX_DROP;
    if (n <= 0) return 0;
    f->rain_q -= n;
    int cnt[COLS], order[COLS];
    for (int c = 0; c < COLS; c++) {
        cnt[c] = n / COLS;
        order[c] = c;
    }
    for (int c = COLS - 1; c > 0; c--) {            /* the odd ones on random columns */
        int j = rs_rng_range(&w->rng, c + 1), t = order[c];
        order[c] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < n % COLS; k++) cnt[order[k]]++;
    for (int c = 0; c < COLS; c++) {
        int h = gheight(f->g, c);
        for (int k = 0; k < cnt[c] && h + k < ROWS; k++) {
            f->g[h + k][c] = RAIN;
            f->off[h + k][c] = (int16_t)((ROWS - h) * CQ);   /* the block falls from above the field */
            f->sq[h + k][c] = 0;
        }
    }
    f->fall_v = SETTLE_V0;
    return n;
}

static void remove_melted(field *f)
{
    f->npops = 0;
    f->pop_age = 0;
    for (int c = 0; c < COLS; c++) {
        int w = 0;
        for (int r = 0; r < ROWS; r++) {
            if (!f->g[r][c]) continue;
            if (f->melt[r][c]) {
                if (f->npops < POPS_MAX) {
                    pop_t *p = &f->pops[f->npops++];
                    p->col = (uint8_t)c;
                    p->row = (uint8_t)r;
                    p->colour = f->g[r][c];
                }
                continue;
            }
            f->g[w][c] = f->g[r][c];
            f->off[w][c] = (int16_t)((r - w) * CQ);
            f->sq[w][c] = r != w ? 0 : f->sq[r][c];
            w++;
        }
        for (int r = w; r < ROWS; r++) {
            f->g[r][c] = 0;
            f->off[r][c] = 0;
            f->sq[r][c] = 0;
        }
    }
    memset(f->melt, 0, sizeof f->melt);
    f->fall_v = SETTLE_V0;
}

static void lose(world *w, int i)
{
    field *f = &w->f[i];
    int alive = 0;                                  /* the fields in at the start of this frame: the ones out at the same
                                                       moment share the place (lost_t counts from the next frame) */
    for (int k = 0; k < w->nf; k++)
        alive += w->f[k].alive || (w->f[k].state == FS_LOST && w->f[k].lost_t == 0);
    f->alive = 0;
    f->state = FS_LOST;
    f->lost_t = 0;
    f->place = (int16_t)alive;                      /* the 1st of 4 out: 4th */
    w->places++;
    w->events[i] |= EV_LOSE;
}

static void check(world *w, int i)
{
    field *f = &w->f[i];
    uint8_t mark[ROWS][COLS];
    int ncol, sizes[16], ng;
    int n = groups(f->g, mark, &ncol, sizes, &ng);
    if (n > 0) {
        f->chain++;
        if (f->chain > f->max_chain) f->max_chain = f->chain;
        int pts = melt_points(f->chain, n, ncol, sizes, ng);
        f->score += pts;
        f->carry += pts;
        f->melted += n;
        int send = f->carry / RAIN_POINTS;
        f->carry %= RAIN_POINTS;
        rain_send(w, i, send);
        memcpy(f->melt, mark, sizeof mark);
        int sc = 0, top = 0, k = 0;                 /* where the chain shows: over the melt */
        for (int r = 0; r < VROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (mark[r][c] == 1) { sc += c; k++; if (r > top) top = r; }
        f->chain_col = (int16_t)(k ? sc / k : 2);
        f->chain_row = (int16_t)top;
        f->chain_shown = f->chain;
        f->chain_t = 0;
        f->state = FS_MELT;
        f->t = 0;
        w->events[i] |= EV_MELT;
        return;
    }
    f->chain = 0;
    if (!f->rained && f->rain_q > 0) {              /* the turn is over: the rain waiting for this field falls */
        f->rained = 1;
        if (drop_rain(w, f)) {
            w->events[i] |= EV_RAIN;
            f->state = FS_SETTLE;
            f->t = 0;
            return;
        }
    }
    f->rained = 0;
    f->state = FS_SPAWN;
    f->t = 0;
}

static void pair_step(world *w, int i, const pin *in)
{
    field *f = &w->f[i];
    pair_t *p = &f->pr;
    int *ev = &w->events[i];
    if (p->rot_t < 255) p->rot_t++;
    if (p->slide_t < 255) p->slide_t++;
    if (p->qturn) p->qturn--;
    if (in->cw && pair_turn(f, 1)) *ev |= EV_TURN;
    if (in->ccw && pair_turn(f, -1)) *ev |= EV_TURN;
    if (in->step) {
        if (pair_move(f, in->step)) *ev |= EV_MOVE;
        f->das_dir = 0;
    } else if (in->dir) {
        if (in->dir_pressed || in->dir != f->das_dir) {
            f->das_dir = in->dir;
            f->das_t = 0;
            if (pair_move(f, in->dir)) *ev |= EV_MOVE;
        } else {
            if (f->das_t < 100) f->das_t++;
            if (f->das_t >= DAS_DELAY && (f->das_t - DAS_DELAY) % DAS_REPEAT == 0 && pair_move(f, in->dir)) *ev |= EV_MOVE;
        }
    } else {
        f->das_dir = 0;
    }
    if (in->fast) p->fast = 1;
    int fl = pair_floor(f, p), soft = in->down || p->fast;
    if (p->y > fl) {
        int sp = p->fast ? FAST_SPEED : in->down ? SOFT_SPEED : w->fall;
        int ny = p->y - sp < fl ? fl : p->y - sp;
        if (soft) {
            f->drop_acc += p->y - ny;
            while (f->drop_acc >= CQ) {
                f->drop_acc -= CQ;
                f->score += SOFT_POINTS;
                f->carry += SOFT_POINTS;
            }
        }
        p->y = ny;
    }
    if (p->y <= fl) {
        p->lock_t++;
        if (soft || p->lock_t >= LOCK_FRAMES) lock_pair(w, i);
    } else {
        p->lock_t = 0;
    }
}

/* the frame counters a field keeps in every phase (the squash, the chain's and the pops' ages) */
static void field_tick(field *f)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (f->sq[r][c]) f->sq[r][c]--;
    if (f->chain_t < 999) f->chain_t++;
    if (f->pop_age < 999) f->pop_age++;
    if (f->state == FS_LOST && f->lost_t < 999) f->lost_t++;
}

static void field_step(world *w, int i, const pin *in)
{
    field *f = &w->f[i];
    f->t++;
    switch (f->state) {
    case FS_SPAWN:
        if (f->t < SPAWN_DELAY) break;
        if (f->g[SPAWN_ROW][SPAWN_COL]) {           /* filled up to the spawn point: this field loses */
            lose(w, i);
            break;
        }
        memset(&f->pr, 0, sizeof f->pr);
        seq_pair(w->seq_seed, f->seq, &f->pr.c0, &f->pr.c1);
        f->seq++;
        f->pr.col = SPAWN_COL;
        f->pr.y = SPAWN_ROW * CQ;
        f->pr.rot_t = ROT_ANIM;
        f->pr.slide_t = SLIDE_ANIM;
        f->state = FS_PAIR;
        f->t = 0;
        w->events[i] |= EV_SPAWN;
        break;
    case FS_PAIR:
        pair_step(w, i, in);
        break;
    case FS_SETTLE:
        if (!settle(f, &w->events[i])) {
            f->state = FS_CHECK;
            f->t = 0;
        }
        break;
    case FS_CHECK:
        check(w, i);
        break;
    case FS_MELT:
        if (f->t >= MELT_FRAMES) {
            remove_melted(f);
            w->events[i] |= EV_POP;
            f->state = FS_SETTLE;
            f->t = 0;
        }
        break;
    default:
        break;
    }
}

/* ---- duels and rounds --------------------------------------------------------------------------------------------- */
static void setup_fields(world *w)
{
    for (int i = 0; i < MAX_FIELDS; i++) {
        field *f = &w->f[i];
        int32_t score = f->score;
        field_reset(f);
        if (w->mode == MODE_DUEL && i == 0) f->score = score;   /* the player's points carry on from duel to duel */
        if (w->mode == MODE_VS) f->score = score;
        if (i >= w->nf) f->alive = 0;
    }
    if (w->mode == MODE_DUEL) {
        w->f[0].kind = FK_HUMAN;
        w->f[1].kind = FK_CPU;
        w->f[1].level = (int8_t)(w->level > 99 ? 99 : w->level);
    }
    w->seq_seed = mix(w->seed + 0x632be5abu * (uint32_t)(w->mode == MODE_DUEL ? w->level : w->round + 100));
    w->phase = PH_INTRO;
    w->phase_t = 0;
    w->round_t = 0;
    w->places = 0;
    w->winner = -1;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed ? seed : 1;
    rs_rng_seed(&w->rng, w->seed);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->winner = -1;
    w->level = 1;
    w->round = 1;
    w->fall = FALL_START;
}

void world_start(world *w, int level)
{
    w->started = 1;
    w->mode = w->players == 1 ? MODE_DUEL : MODE_VS;
    w->nf = w->players == 1 ? 2 : w->players;
    w->level = level < 1 ? 1 : level;
    w->round = 1;
    w->lost = 0;
    w->duels_won = 0;
    for (int i = 0; i < MAX_FIELDS; i++) {
        memset(&w->f[i], 0, sizeof w->f[i]);
        w->f[i].kind = FK_HUMAN;
    }
    setup_fields(w);
    w->events[0] |= EV_START;
}

int world_over(const world *w) { return w->started && w->phase == PH_OVER; }

static void decide(world *w)
{
    int alive = 0, last = -1;
    for (int i = 0; i < w->nf; i++)
        if (w->f[i].alive) { alive++; last = i; }
    if (w->mode == MODE_DUEL) {
        if (!w->f[0].alive) {                       /* the player's field lost: game over */
            w->lost = 1;
            w->winner = 1;
            w->phase = PH_OVER;
            w->phase_t = 0;
            w->events[1] |= EV_WIN;
        } else if (!w->f[1].alive) {                /* the chef's: the next chef */
            w->winner = 0;
            w->duels_won++;
            w->f[0].score += WIN_BONUS * w->level;
            w->f[0].place = 1;
            w->phase = PH_END;
            w->phase_t = 0;
            w->events[0] |= EV_WIN;
        }
        return;
    }
    if (alive > 1) return;
    w->winner = alive == 1 ? last : -1;
    if (last >= 0) {
        w->f[last].wins++;
        w->f[last].place = 1;
        w->events[last] |= EV_WIN;
    }
    w->phase = PH_END;
    w->phase_t = 0;
}

void world_step(world *w, const pin in[MAX_FIELDS])
{
    memset(w->events, 0, sizeof w->events);
    w->t++;
    if (!w->started) return;
    w->phase_t++;
    for (int i = 0; i < w->nf; i++) field_tick(&w->f[i]);
    switch (w->phase) {
    case PH_INTRO:
        if (w->phase_t >= INTRO_FRAMES) {
            w->phase = PH_PLAY;
            w->phase_t = 0;
            for (int i = 0; i < w->nf; i++) {
                w->f[i].state = FS_SPAWN;
                w->f[i].t = SPAWN_DELAY;
            }
        }
        break;
    case PH_PLAY: {
        w->round_t++;
        int base = FALL_START + (w->mode == MODE_DUEL ? FALL_LEVEL * (w->level - 1) : 0);
        int sp = base + SPEEDUP_STEP * (w->round_t / SPEEDUP_FRAMES);
        w->fall = sp > FALL_MAX ? FALL_MAX : sp;
        for (int i = 0; i < w->nf; i++) {
            field *f = &w->f[i];
            if (!f->alive) continue;
            pin p = in[i];
            if (f->kind == FK_CPU) ai_play(w, i, cpu_params(f->level), &p);
            field_step(w, i, &p);
        }
        decide(w);
        break;
    }
    case PH_END:
        if (w->phase_t < END_FRAMES) break;
        if (w->mode == MODE_DUEL) {
            w->level++;
            setup_fields(w);
        } else {
            int done = 0;
            for (int i = 0; i < w->nf; i++) done |= w->f[i].wins >= WINS_NEEDED;
            if (done) {
                w->phase = PH_OVER;
                w->phase_t = 0;
            } else {
                w->round++;
                setup_fields(w);
            }
        }
        break;
    default:
        break;
    }
}

/* ---- the brains: the computer chefs and the bot ------------------------------------------------------------------- */
/* A brain plans each pair in stages, one a frame while it "thinks" (so that no frame costs much): every spot the pair
 * can reach, judged cheaply (cheap_eval: the colours touching, the heights, the spots where one more jelly would melt a
 * group, and the chain the spot fires); then the longest chain one or two more jellies could fire after its best spots
 * (potential); then, for the brains that plan ahead, the next pair after the best ones; then its pick, with its noise
 * and its mistakes. Then it turns, steps and (some) soft-drop the pair there. */
const ai_params *cpu_params(int level)
{
    static const ai_params T[CHEFS] = CPU_TABLE;
    int k = level < 1 ? 0 : level > CHEFS ? CHEFS - 1 : level - 1;
    return &T[k];
}

void ai_skill_params(int skill, ai_params *out)
{
    static const ai_params best = CPU_PLAYER;
    *out = best;
    if (skill <= 0) return;
    if (skill > 10) skill = 10;
    out->think = (int16_t)(10 + 5 * skill);         /* human-like: slower, ... */
    out->step = (int16_t)(3 + skill);
    out->soft = skill < 5;
    out->ahead = skill < 3;                         /* ... plans less ... */
    out->ambition = (uint8_t)(skill < 3 ? 4 : skill < 6 ? 3 : skill < 9 ? 2 : 1);
    out->mistake = (uint8_t)(4 * skill);            /* ... and misplaces more */
    out->noise = (uint8_t)(12 * skill);
    out->aware = skill < 7;
}

/* how many cells of colour k a jelly of colour k put at (r0, c0) would touch, itself included (on screen) */
static int flood_count(const uint8_t m[RAIN + 1][VROWS], int r0, int c0, int k)
{
    uint8_t reg[VROWS];
    memset(reg, 0, sizeof reg);
    reg[r0] = (uint8_t)(1 << c0);
    return grow(reg, m[k]);
}

/* the longest chain one or two more jellies of a colour could fire, in half chains (2 per chain with one jelly, 2n-1
 * with two) */
static int potential(const uint8_t g[ROWS][COLS])
{
    uint8_t t[ROWS][COLS], m[RAIN + 1][VROWS];
    int best = 0;
    bitrows(g, m);
    for (int c = 0; c < COLS; c++) {
        int h = gheight(g, c);
        if (h >= VROWS) continue;
        for (int k = 1; k <= COLOURS; k++) {
            int n = flood_count(m, h, c, k);
            if (n >= MELT_MIN) {
                memcpy(t, g, sizeof t);
                t[h][c] = (uint8_t)k;
                int ch = grid_resolve(t, NULL, NULL) * 2;
                if (ch > best) best = ch;
            } else if (n == MELT_MIN - 1 && h + 1 < VROWS) {
                memcpy(t, g, sizeof t);
                t[h][c] = t[h + 1][c] = (uint8_t)k;
                int ch = grid_resolve(t, NULL, NULL) * 2 - 1;
                if (ch > best) best = ch;
            }
        }
    }
    return best;
}

static int32_t cheap_eval(const uint8_t g[ROWS][COLS])
{
    if (g[SPAWN_ROW][SPAWN_COL]) return -1000000;
    int32_t v = 0;
    int h[COLS], rain = 0;
    uint8_t m[RAIN + 1][VROWS];
    bitrows(g, m);
    for (int c = 0; c < COLS; c++) h[c] = gheight(g, c);
    for (int r = 0; r < VROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int k = g[r][c];
            if (k == RAIN) rain++;
            if (k < 1 || k > COLOURS) continue;
            if (c + 1 < COLS && g[r][c + 1] == k) v += AI_W_CONN;
            if (r + 1 < VROWS && g[r + 1][c] == k) v += AI_W_CONN;
        }
    for (int c = 0; c < COLS; c++) {
        if (h[c] < VROWS)                           /* one more jelly here would melt a group */
            for (int k = 1; k <= COLOURS; k++)
                if (flood_count(m, h[c], c, k) >= MELT_MIN) v += AI_W_TRIG;
        v -= h[c] * AI_W_HEIGHT;
        if (h[c] > AI_HIGH) v -= (h[c] - AI_HIGH) * (h[c] - AI_HIGH) * AI_W_HIGH;
        if (c >= SPAWN_COL - 1 && c <= SPAWN_COL + 1 && h[c] > AI_HIGH) v -= (h[c] - AI_HIGH) * AI_W_SPAWN;
        if (c + 1 < COLS) v -= (h[c] > h[c + 1] ? h[c] - h[c + 1] : h[c + 1] - h[c]) * AI_W_BUMP;
    }
    return v - rain * AI_W_RAINCELL;
}

static int maxh(const uint8_t g[ROWS][COLS])
{
    int m = 0;
    for (int c = 0; c < COLS; c++) {
        int h = gheight(g, c);
        if (h > m) m = h;
    }
    return m;
}

/* the worth of firing a chain of `chains` steps for `pts` points */
static int32_t fire_value(const world *w, int fi, const ai_params *P, int chains, int pts, int melted, int high)
{
    if (!chains) return 0;
    const field *f = &w->f[fi];
    int rain = pts / RAIN_POINTS;
    int t = pick_target(w, fi);
    int kill = 0;
    if (t >= 0) {                                   /* would it bury the opponent? */
        const field *o = &w->f[t];
        int free_rows = VROWS - field_height(o, SPAWN_COL);
        kill = (rain - f->rain_q) / COLS >= free_rows - 1 && rain - f->rain_q >= 6;
    }
    /* crowded, any brain fires what it has; an aware one also when rain is coming */
    int urgent = high >= AI_CROWDED || field_count(f) >= AI_CROWDED_N || (P->aware && f->rain_q >= AI_THREAT_RAIN);
    if (chains >= P->ambition || urgent || kill)
        return rain * AI_W_FIRE + chains * AI_W_CHAIN + (kill ? AI_W_KILL : 0) + (urgent ? f->rain_q * AI_W_FIRE : 0);
    return -melted * AI_W_WASTE;
}

/* the grid after candidate k (the pair placed, its chain fired) */
static void cand_grid(const field *f, const ai_cand *k, uint8_t g[ROWS][COLS], int *ch, int *pts, int *mel)
{
    memcpy(g, f->g, sizeof f->g);
    grid_place(g, k->col, k->rot, f->pr.c0, f->pr.c1);
    *ch = grid_resolve(g, pts, mel);
}

static void sort_cands(ai_t *a)
{
    for (int i = 1; i < a->n; i++) {
        ai_cand k = a->c[i];
        int j = i - 1;
        while (j >= 0 && a->c[j].v < k.v) { a->c[j + 1] = a->c[j]; j--; }
        a->c[j + 1] = k;
    }
}

/* stage 0: every spot the pair can reach from the spawn column, judged cheaply */
static void plan_start(world *w, int fi, const ai_params *P)
{
    field *f = &w->f[fi];
    ai_t *a = &f->ai;
    int high = maxh(f->g);
    a->n = 0;
    for (int col = 0; col < COLS; col++)
        for (int rot = 0; rot < 4; rot++) {
            int cc = col + CDX[rot];
            if (cc < 0 || cc >= COLS) continue;
            int lo = col < cc ? col : cc, hi = col > cc ? col : cc, ok = 1;
            if (lo > SPAWN_COL) lo = SPAWN_COL;
            if (hi < SPAWN_COL) hi = SPAWN_COL;
            for (int c = lo; c <= hi; c++)
                if (gheight(f->g, c) > SPAWN_ROW) ok = 0;
            if (!ok) continue;
            ai_cand *k = &a->c[a->n++];
            uint8_t g[ROWS][COLS];
            int ch, pts, mel;
            k->col = (int8_t)col;
            k->rot = (int8_t)rot;
            cand_grid(f, k, g, &ch, &pts, &mel);
            k->fire = fire_value(w, fi, P, ch, pts, mel, high);
            k->v = k->fire + cheap_eval(g);
        }
    sort_cands(a);
    a->stage = 1;
    a->idx = 0;
}

/* one more step of the plan; returns 1 when it is done */
static int plan_step(world *w, int fi, const ai_params *P)
{
    field *f = &w->f[fi];
    ai_t *a = &f->ai;
    uint8_t g[ROWS][COLS];
    int ch, pts, mel;
    if (a->stage == 1) {                            /* the potential of the best spots */
        if (a->idx < a->n && a->idx < AI_POT_TOP) {
            ai_cand *k = &a->c[a->idx++];
            if (k->v > -500000) {
                cand_grid(f, k, g, &ch, &pts, &mel);
                int p = potential(g);
                k->v += p * p * AI_W_POT;
            }
            return 0;
        }
        sort_cands(a);
        a->stage = P->ahead ? 2 : 3;
        a->idx = 0;
        return 0;
    }
    if (a->stage == 2) {                            /* the next pair after the best spots */
        if (a->idx < a->n && a->idx < AI_AHEAD_TOP) {
            ai_cand *k = &a->c[a->idx++];
            if (k->v <= -500000) return 0;
            uint8_t n0, n1;
            seq_pair(w->seq_seed, f->seq, &n0, &n1);
            cand_grid(f, k, g, &ch, &pts, &mel);
            int high = maxh(g);
            int32_t best2 = -2000000;
            for (int col = 0; col < COLS; col++)
                for (int rot = 0; rot < 4; rot++) {
                    uint8_t g2[ROWS][COLS];
                    memcpy(g2, g, sizeof g2);
                    if (!grid_place(g2, col, rot, n0, n1)) continue;
                    int pts2, mel2, ch2 = grid_resolve(g2, &pts2, &mel2);
                    int32_t v2 = fire_value(w, fi, P, ch2, pts2, mel2, high) + cheap_eval(g2);
                    if (v2 > best2) best2 = v2;
                }
            int32_t now = k->v - k->fire;
            if (best2 * 7 / 8 > now) k->v = k->fire + best2 * 7 / 8;
            return 0;
        }
        a->stage = 3;
        return 0;
    }
    /* stage 3: the pick, with the brain's noise and mistakes */
    for (int i = 0; i < a->n; i++)
        if (P->noise) a->c[i].v += rs_rng_range(&w->rng, 2 * P->noise * AI_NOISE_K + 1) - P->noise * AI_NOISE_K;
    sort_cands(a);
    int pick = 0;
    if (P->mistake && a->n > 1 && rs_rng_range(&w->rng, 100) < P->mistake) pick = rs_rng_range(&w->rng, a->n < 5 ? a->n : 5);
    if (a->n) {
        a->tcol = a->c[pick].col;
        a->trot = a->c[pick].rot;
        a->value = a->c[pick].v;
    } else {                                        /* nowhere to go: let it fall */
        a->tcol = f->pr.col;
        a->trot = f->pr.rot;
        a->value = -1000000;
    }
    a->stage = 4;
    return 1;
}

void ai_play(world *w, int fi, const ai_params *P, pin *out)
{
    field *f = &w->f[fi];
    ai_t *a = &f->ai;
    memset(out, 0, sizeof *out);
    if (f->state != FS_PAIR) return;
    if (!a->stage || a->seq != f->seq) {            /* a new pair: start planning */
        plan_start(w, fi, P);
        a->seq = f->seq;
        a->planned = 0;
        a->wait = P->think;
        a->gave_up = 0;
        a->gave_up_n = 0;
        return;
    }
    if (!a->planned) a->planned = (uint8_t)plan_step(w, fi, P);
    if (a->wait > 0) a->wait--;
    if (!a->planned || a->wait > 0) return;
    pair_t *p = &f->pr;
    if (!a->gave_up && p->rot != a->trot) {
        int d = (a->trot - p->rot) & 3;
        if (d == 3) out->ccw = 1;
        else out->cw = 1;
        a->wait = P->step;
        if (++a->gave_up_n > AI_TRIES) a->gave_up = 1;
        return;
    }
    if (!a->gave_up && p->col != a->tcol) {
        int dir = a->tcol > p->col ? 1 : -1;
        pair_t q = *p;
        q.col = (int8_t)(q.col + dir);
        if (!pair_fits(f, &q)) a->gave_up = 1;       /* blocked: drop it here */
        else out->step = (int8_t)dir;
        a->wait = P->step;
        if (++a->gave_up_n > AI_TRIES) a->gave_up = 1;
        return;
    }
    if (P->soft) out->down = 1;
}

/* ---- the layout: the fields on the 320-px stage ---------------------------------------------------------------- */
int field_layout(int nf, flayout out[MAX_FIELDS])
{
    memset(out, 0, sizeof(flayout) * MAX_FIELDS);
    if (nf < 1) return 0;
    if (nf > MAX_FIELDS) nf = MAX_FIELDS;
    for (int i = 0; i < nf; i++) {
        flayout *l = &out[i];
        if (nf <= 2) {                              /* two fields of 16-px cells, the middle for the previews */
            l->cell = LAYOUT_CELL_BIG;
            l->x = (int16_t)(i ? LAYOUT2_X1 : LAYOUT2_X0);
            l->y = LAYOUT2_Y;
        } else {                                    /* three or four of 12-px cells side by side */
            l->cell = LAYOUT_CELL_SMALL;
            l->x = (int16_t)(nf == 3 ? LAYOUT3_X0 + i * LAYOUT3_DX : LAYOUT4_X0 + i * LAYOUT4_DX);
            l->y = LAYOUT34_Y;
        }
        l->w = (int16_t)(l->cell * COLS);
        l->h = (int16_t)(l->cell * VROWS);
    }
    return nf;
}

int touch_column(int col0, int dx_px, int cell)
{
    if (cell < 1) cell = 1;
    int k = dx_px >= 0 ? (dx_px + cell / 2) / cell : -((-dx_px + cell / 2) / cell);
    int c = col0 + k;
    return c < 0 ? 0 : c >= COLS ? COLS - 1 : c;
}
