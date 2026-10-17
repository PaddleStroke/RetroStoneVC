/*
 * Pool's rules (pool.h): the table and its pockets, the racks, the shot run by the table's physics (cuetable.h), then
 * the break, the fouls, the groups, the 8 and the 9, ball in hand. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pool.h"

void pool_table(cue_table *t)
{
    memset(t, 0, sizeof *t);
    t->w = POOL_W;
    t->h = POOL_H;
    t->mu = 22;
    t->drag = 0.26f;
    t->e_ball = 0.95f;
    t->e_wall = 0.74f;
    t->slip = 0.96f;
    t->drop = 0.8f * POOL_R;
    static const struct { int edge; float a, b; } M[] = {
        {CUE_TOP, -2, POOL_CORNER}, {CUE_TOP, POOL_W / 2 - POOL_SIDE, POOL_W / 2 + POOL_SIDE},
        {CUE_TOP, POOL_W - POOL_CORNER, POOL_W + 2},
        {CUE_BOTTOM, -2, POOL_CORNER}, {CUE_BOTTOM, POOL_W / 2 - POOL_SIDE, POOL_W / 2 + POOL_SIDE},
        {CUE_BOTTOM, POOL_W - POOL_CORNER, POOL_W + 2},
        {CUE_LEFT, -2, POOL_CORNER}, {CUE_LEFT, POOL_H - POOL_CORNER, POOL_H + 2},
        {CUE_RIGHT, -2, POOL_CORNER}, {CUE_RIGHT, POOL_H - POOL_CORNER, POOL_H + 2},
    };
    t->nmouths = (int)(sizeof M / sizeof M[0]);
    for (int k = 0; k < t->nmouths; k++) {
        t->mouth[k].edge = M[k].edge;
        t->mouth[k].a = M[k].a;
        t->mouth[k].b = M[k].b;
    }
    static const float P[6][2] = {{-0.9f, -0.9f}, {50, -1.6f}, {100.9f, -0.9f},
                                  {-0.9f, 50.9f}, {50, 51.6f}, {100.9f, 50.9f}};
    t->npockets = 6;
    for (int k = 0; k < 6; k++) { t->px[k] = P[k][0]; t->py[k] = P[k][1]; }
}

int pool_balls(const pool_pos *p) { return p->variant == POOL_NINE ? 9 : 15; }

static void put(cue_world *w, int i, float x, float y)
{
    cue_ball *b = &w->b[i];
    memset(b, 0, sizeof *b);
    b->x = x;
    b->y = y;
    b->r = POOL_R;
    b->m = 1;
    b->down = -1;
}

/* the rack, its apex on the foot spot: 8-ball a triangle of five rows (the 8 in the middle of the third, a solid and
 * a stripe in the back corners), 9-ball a diamond of 1, 2, 3, 2, 1 (the 1 at the apex, the 9 in the middle) */
void pool_new(pool_pos *p, int variant, uint64_t seed)
{
    memset(p, 0, sizeof *p);
    p->variant = variant == POOL_NINE ? POOL_NINE : POOL_EIGHT;
    p->in_hand = p->brk = 1;
    cue_world *w = &p->w;
    int n = pool_balls(p);
    w->n = n + 1;
    put(w, 0, POOL_HEAD / 2, POOL_H / 2);
    float d = 2 * POOL_R + 0.02f, dx = d * 0.8660254f;
    int order[15], k = 0;
    uint64_t s = seed;
    if (p->variant == POOL_EIGHT) {
        int rest[12], nr = 0;                   /* the balls but the 8 and the two corners */
        int solid = 1 + (int)(cue_rand(&s) % 7), stripe = 9 + (int)(cue_rand(&s) % 7);
        for (int b = 1; b <= 15; b++)
            if (b != 8 && b != solid && b != stripe) rest[nr++] = b;
        for (int i = nr - 1; i > 0; i--) {
            int j = (int)(cue_rand(&s) % (unsigned)(i + 1)), t = rest[i];
            rest[i] = rest[j];
            rest[j] = t;
        }
        int left_solid = cue_rand(&s) & 1, ri = 0;
        for (int row = 0; row < 5; row++)
            for (int j = 0; j <= row; j++) {
                int b;
                if (row == 2 && j == 1) b = 8;
                else if (row == 4 && j == 0) b = left_solid ? solid : stripe;
                else if (row == 4 && j == 4) b = left_solid ? stripe : solid;
                else b = rest[ri++];
                put(w, b, POOL_FOOT_X + row * dx, POOL_H / 2 + (j - row / 2.0f) * d);
                order[k++] = b;
            }
    } else {
        int rest[7], nr = 0;
        for (int b = 2; b <= 8; b++) rest[nr++] = b;
        for (int i = nr - 1; i > 0; i--) {
            int j = (int)(cue_rand(&s) % (unsigned)(i + 1)), t = rest[i];
            rest[i] = rest[j];
            rest[j] = t;
        }
        static const int rows[5] = {1, 2, 3, 2, 1};
        int ri = 0;
        for (int row = 0; row < 5; row++)
            for (int j = 0; j < rows[row]; j++) {
                int b = row == 0 ? 1 : row == 2 && j == 1 ? 9 : rest[ri++];
                put(w, b, POOL_FOOT_X + row * dx, POOL_H / 2 + (j - (rows[row] - 1) / 2.0f) * d);
                order[k++] = b;
            }
    }
    (void)order;
}

int pool_left(const pool_pos *p, int group)
{
    int n = 0;
    for (int i = 1; i < p->w.n; i++)
        if (p->w.b[i].down < 0 && pool_group_of(i) == group) n++;
    return n;
}

static int lowest(const pool_pos *p)
{
    for (int i = 1; i < p->w.n; i++)
        if (p->w.b[i].down < 0) return i;
    return -1;
}

uint32_t pool_targets(const pool_pos *p)
{
    uint32_t m = 0;
    if (p->variant == POOL_NINE) {
        int l = lowest(p);
        return l > 0 ? 1u << l : 0;
    }
    int g = p->group[p->turn];
    for (int i = 1; i < p->w.n; i++) {
        if (p->w.b[i].down >= 0) continue;
        if (g == POOL_OPEN ? i != 8 || p->brk : pool_group_of(i) == g) m |= 1u << i;
    }
    if (!m && p->w.b[8].down < 0) m = 1u << 8;  /* the group cleared: the 8 */
    return m;
}

int pool_place_ok(const pool_pos *p, float x, float y)
{
    if (!p->in_hand) return 0;
    if (x < POOL_R || x > POOL_W - POOL_R || y < POOL_R || y > POOL_H - POOL_R) return 0;
    if (p->brk && x > POOL_HEAD) return 0;
    return !cue_overlaps(&p->w, x, y, POOL_R, 0);
}

int pool_ready(const pool_pos *p, int ang, int pow, int placed, float x, float y, cue_world *w)
{
    if (p->status != POOL_PLAY || pow < 1 || pow > 1000 || ang < 0 || ang >= 36000) return 0;
    if (placed && !pool_place_ok(p, x, y)) return 0;
    if (p->w.b[0].down >= 0 && !placed) return 0;
    *w = p->w;
    for (int i = 0; i < w->n; i++) w->b[i].vx = w->b[i].vy = 0;
    if (placed) { w->b[0].x = x; w->b[0].y = y; w->b[0].down = -1; }
    cue_strike(w, 0, ang, POOL_VMAX * (float)pow / 1000);
    return 1;
}

/* a ball back on the foot spot (or the free place nearest it, towards the foot cushion) */
static void spot(pool_pos *p, int i)
{
    cue_ball *b = &p->w.b[i];
    b->down = -1;
    b->vx = b->vy = 0;
    cue_free_spot(&p->w, POOL_R, POOL_FOOT_X, POOL_H / 2, POOL_R, POOL_R, POOL_W - POOL_R, POOL_H - POOL_R, i, &b->x,
                  &b->y);
}

void pool_apply(pool_pos *p, const cue_events *ev)
{
    int me = p->turn, op = !me, brk = p->brk;
    uint32_t pot = 0, objects = 0;
    for (int i = 1; i < p->w.n; i++) objects |= 1u << i;
    for (int k = 0; k < ev->npot; k++) pot |= 1u << ev->pot[k];
    /* what the table was before the shot: the shot's own pockets taken off */
    int cleared = 0, low = 0;
    if (p->variant == POOL_EIGHT && p->group[me] != POOL_OPEN) {
        int left = pool_left(p, p->group[me]);
        for (int k = 0; k < ev->npot; k++) left += pool_group_of(ev->pot[k]) == p->group[me];
        cleared = left == 0;
    }
    for (int i = 1; i < p->w.n && !low; i++)
        if (p->w.b[i].down < 0 || (pot >> i & 1)) low = i;
    int foul = POOL_FOUL_NONE, objs = __builtin_popcount(pot & objects);
    if (pot & 1) foul = POOL_FOUL_SCRATCH;
    else if (ev->first < 0) foul = POOL_FOUL_MISS;
    else if (p->variant == POOL_NINE) {
        if (ev->first != low) foul = POOL_FOUL_WRONG;
    } else if (!brk) {
        int g = p->group[me];
        if (g == POOL_OPEN ? ev->first == 8 : cleared ? ev->first != 8 : pool_group_of(ev->first) != g)
            foul = POOL_FOUL_WRONG;
    }
    if (!foul && brk && !objs && __builtin_popcount(ev->rail & objects) < 4) foul = POOL_FOUL_BREAK;
    if (!foul && !brk && !objs && !ev->rail_after) foul = POOL_FOUL_RAIL;
    p->foul = foul;
    int keep = !foul && objs > 0;
    if (p->variant == POOL_EIGHT) {
        if (pot >> 8 & 1) {
            if (brk) { spot(p, 8); objs--; keep = !foul && objs > 0; }   /* the 8 on the break: spotted */
            else { p->status = !foul && cleared ? (me ? POOL_WIN1 : POOL_WIN0) : (me ? POOL_WIN0 : POOL_WIN1); }
        }
        if (p->status == POOL_PLAY && !brk && !foul && p->group[me] == POOL_OPEN)
            for (int k = 0; k < ev->npot; k++) {   /* the first ball pocketed names the groups */
                int g = pool_group_of(ev->pot[k]);
                if (g == POOL_OPEN) continue;
                p->group[me] = g;
                p->group[op] = g == POOL_SOLIDS ? POOL_STRIPES : POOL_SOLIDS;
                break;
            }
        if (keep && p->group[me] != POOL_OPEN) {   /* only a ball of your group keeps the turn */
            keep = 0;
            for (int k = 0; k < ev->npot; k++) keep |= pool_group_of(ev->pot[k]) == p->group[me];
        }
    } else if (pot >> 9 & 1) {
        if (!foul) p->status = me ? POOL_WIN1 : POOL_WIN0;
        else spot(p, 9);
    }
    p->brk = 0;
    p->in_hand = 0;
    if (p->status != POOL_PLAY) { p->shots++; return; }
    if (foul) {
        p->in_hand = 1;
        if (!pool_place_ok(p, p->w.b[0].x, p->w.b[0].y)) p->w.b[0].down = 0;   /* on a pocket's lip: lifted too */
        if (p->w.b[0].down >= 0) {              /* the cue ball back on the table, where the next player starts */
            cue_ball *c = &p->w.b[0];
            c->down = -1;
            cue_free_spot(&p->w, POOL_R, POOL_HEAD / 2, POOL_H / 2, POOL_R, POOL_R, POOL_W - POOL_R, POOL_H - POOL_R,
                          0, &c->x, &c->y);
        }
    }
    if (!keep) p->turn = op;
    p->shots++;
}

int pool_shot(pool_pos *p, int ang, int pow, int placed, float x, float y, cue_events *ev)
{
    cue_table t;
    cue_events e;
    cue_world w;
    if (!pool_ready(p, ang, pow, placed, x, y, &w)) return 0;
    pool_table(&t);
    cue_events_clear(&e);
    cue_run(&t, &w, &e);
    p->w = w;
    pool_apply(p, &e);
    if (ev) *ev = e;
    return 1;
}

int pool_parse(const char *s, int *ang, int *pow, int *placed, float *x, float *y, int *len)
{
    int n = 0, m = 0, ix, iy;
    if (sscanf(s, "%d:%d%n", ang, pow, &n) != 2) return 0;
    *placed = 0;
    if (s[n] == '@') {
        if (sscanf(s + n, "@%d,%d%n", &ix, &iy, &m) != 2) return 0;
        *placed = 1;
        *x = ix / 100.0f;
        *y = iy / 100.0f;
        n += m;
    }
    if (len) *len = n;
    return 1;
}

void pool_game_new(pool_game *g, int variant, uint64_t seed)
{
    g->ply = 0;
    pool_new(&g->pos[0], variant, seed);
}

int pool_play(pool_game *g, int ang, int pow, int placed, float x, float y, cue_events *ev)
{
    if (g->ply >= POOL_HIST) {
        memmove(&g->pos[0], &g->pos[1], sizeof g->pos[0] * POOL_HIST);
        g->ply--;
    }
    g->pos[g->ply + 1] = g->pos[g->ply];
    if (!pool_shot(&g->pos[g->ply + 1], ang, pow, placed, x, y, ev)) return 0;
    g->ply++;
    return 1;
}

int pool_undo(pool_game *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}
