/*
 * Bagh-Chal's bot (bots.h), for either side: alpha-beta (ai.h) over the rules' position, with quiescence over the
 * tigers' jumps. One evaluation, from the goats' side (the side to move's view is its sign), with what each side
 * plays for:
 *   the tigers: goats taken (five win), goats they can jump now (two at once cannot both be saved), their room to
 *   move (a tiger with room is never trapped);
 *   the goats: tigers trapped (no step, no jump: all four win), the tigers' room taken away, goats kept on the edges
 *   and in the corners while they come in (where no tiger can jump them), goats guarding each other (the point
 *   behind a goat filled: no jump over it).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c3" a goat set down, "a1-a2" a step, "a1xc3" a jump), played by baghchal_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/baghchal.h"
#include "games/capture_bots.h"

static int encode(bcmove m) { return (m.from + 1) | m.to << 5 | (m.over >= 0) << 10; }

static bcmove decode(int m)
{
    int from = (m & 31) - 1, to = m >> 5 & 31;
    return (bcmove){(int8_t)from, (int8_t)to, (int8_t)(m >> 10 ? (from + to) / 2 : -1)};
}

typedef struct {
    bcpos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture or goat set down */
} bcs;

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    bcmove l[BC_MAX_MOVES];
    int n = bc_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static uint64_t a_hash(const void *st)
{
    const bcpos *p = st;
    return cpb_hash(p->at, sizeof p->at, p->side | p->placed << 1 | p->taken << 6);
}

static void a_make(void *st, int m)
{
    bcs *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    bc_make(&s->p, decode(m));
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}

static int a_side(const void *st) { return ((const bcpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const bcpos *p = st;
    int goats = p->side == BC_GOAT;
    if (p->taken >= BC_TIGERS_WIN) { *score = goats ? -ai_win(0) : ai_win(0); return 1; }
    if (p->quiet >= BC_QUIET || cpb_rep_seen(&((const bcs *)st)->rep, a_hash(st))) { *score = 0; return 1; }
    bcmove l[BC_MAX_MOVES];
    if (bc_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: trapped tigers, or goats that cannot move */
    return 1;
}

/* the goats' view; 100 about a goat */
static int goats_view(const bcpos *p)
{
    int v = -260 * p->taken, steps = 0, trapped = 0, guarded = 0, edge = 0;
    uint32_t prey = 0;                          /* the goats a tiger can jump now */
    for (int i = 0; i < BC_POINTS; i++) {
        if (p->at[i] == BC_GOAT) {
            int x = bc_x(i), y = bc_y(i);
            edge += (x == 0 || x == 4) + (y == 0 || y == 4);
            continue;
        }
        if (p->at[i] != BC_TIGER) continue;
        int moves = 0;
        for (int d = 0; d < 8; d++) {
            int to = bc_next(i, d);
            if (to < 0) continue;
            if (p->at[to] < 0) moves++;
            else if (p->at[to] == BC_GOAT) {
                int land = bc_next(to, d);
                if (land >= 0 && p->at[land] < 0) moves++, prey |= 1u << to;
                else if (land >= 0 && p->at[land] == BC_GOAT) guarded++;
            }
        }
        steps += moves;
        trapped += !moves;
    }
    int threats = __builtin_popcount(prey);
    if (threats) {
        if (p->side == BC_TIGER) v -= 40 + 20 * (threats - 1);    /* quiescence takes it: a little more */
        else v -= threats > 1 ? 150 + 50 * (threats - 2) : 35;    /* the goats save one */
    }
    v += 95 * trapped - 7 * steps + 4 * guarded;
    if (p->placed < BC_GOATS) v += 3 * edge;     /* coming in: the edges first */
    return v;
}

static int a_eval(const void *st)
{
    const bcpos *p = st;
    int v = goats_view(p);
    return p->side == BC_GOAT ? v : -v;
}

static int a_order(const void *st, int m)       /* jumps first; goats: next to a tiger to hem it in */
{
    const bcpos *p = st;
    bcmove x = decode(m);
    if (x.over >= 0) return 100;
    if (p->side == BC_TIGER) return 0;
    int near = 0;
    for (int d = 0; d < 8; d++) {
        int q = bc_next(x.to, d);
        near += q >= 0 && p->at[q] == BC_TIGER;
    }
    return near;
}

static int a_noisy(const void *st, int *out)
{
    const bcpos *p = st;
    if (p->side != BC_TIGER) return 0;
    bcmove l[BC_MAX_MOVES];
    int n = bc_legal(p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].over >= 0) out[k++] = encode(l[i]);
    return k;
}

static const ai_game ADAPTER = {
    .size = sizeof(bcs), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 120,
    .medium_depth = 4, .medium_nodes = 40000, .medium_noise = 25,
    .hard_nodes = 400000, .tt_bits = 18,
};

/* the move in the game g (its side to move), at a level (encoded: from + 1 | to << 5 | jump << 10); -1: none. g is
 * read at once (its position and the ones before it that may come again). */
int bcb_choose(const bcgame *g, int level, uint64_t seed, ai_stats *stats)
{
    bcs s = {.p = *bc_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int m, score;
    if (a_over(&s, &score)) return -1;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, &m, 1, stats) ? m : -1;
}

/* a move in the notation: "c3", "a1-a2", "a1xc3" */
void bcb_write(int m, char *out, int cap)
{
    bcmove x = decode(m);
    if (x.from < 0) snprintf(out, (size_t)cap, "%c%c", 'a' + bc_x(x.to), '1' + bc_y(x.to));
    else snprintf(out, (size_t)cap, "%c%c%c%c%c", 'a' + bc_x(x.from), '1' + bc_y(x.from), x.over >= 0 ? 'x' : '-',
                  'a' + bc_x(x.to), '1' + bc_y(x.to));
}

static int think(int seat, int level, char *move, int cap)
{
    const bcgame *g = baghchal_live();          /* read at entry by bcb_choose(): nothing else is */
    if (bc_pos(g)->side != seat || cap < 6) return 0;
    int m = bcb_choose(g, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    bcb_write(m, move, cap);
    return 1;
}

static const bot baghchal_bot = {.think = think, .play = baghchal_show};
REGISTER_BOT(baghchal, &baghchal_bot)
