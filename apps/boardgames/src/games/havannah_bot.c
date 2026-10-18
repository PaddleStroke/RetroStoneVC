/*
 * Ring and Fork's bot (bots.h), both boards, with or without the swap: Monte Carlo tree search with RAVE
 * (games/rave.h), as Y's and hex's. The position keeps its groups in a union-find, each root holding the corners and
 * the sides its group touches, so a stone's bridge or fork is two bit tests; a ring is found at the stone that closes
 * it: two separate runs of the player's stones round it already in one group (the loop has cells on both sides of
 * the stone, the side away from the rim inside it), or a stone of the player's next to it now wholly surrounded.
 * A playout fills the board at random until a shape (or a full board: a draw, nobody's win), answering an intrusion
 * into one of its bridges. The search's root first takes a win in one and stops the other's.
 * Easy: no search, a stone near the stones already down (its win in one always, the other's stopped one time in
 * two); medium a few thousand playouts; hard as many as its budget allows (about 0.3 s on a PC).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("f6", "swap"), played by havannah_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/havannah.h"
#include "games/rave.h"

typedef struct {
    int16_t nb[HV_SQ][6];                       /* the neighbours in turn round each cell, -1 off the board */
    uint16_t mask[HV_SQ];                       /* the corner (bits 0..5) or the side (bits 6..11) a cell is on */
    uint8_t centre[HV_SQ];                      /* the distance from the middle cell */
    int16_t list[HV_SQ];                        /* the board's cells */
    int count, n;
} hvt;

typedef struct {
    const hvt *t;
    uint8_t cell[HV_SQ];                        /* 0 empty, 1 ivory, 2 ebony */
    int16_t up[HV_SQ];                          /* the union-find: a stone's parent (a root: itself) */
    uint16_t mask[HV_SQ];                       /* a root's corners and sides */
    int side, winner, last, stones;
} hvs;

static int popc(unsigned v)
{
    int k = 0;
    for (; v; v &= v - 1) k++;
    return k;
}

static void tables(hvt *t, int n)
{
    memset(t, 0, sizeof *t);
    t->n = n;
    for (int i = 0; i < HV_SQ; i++) {
        if (!hv_valid(n, i)) continue;
        int nb[6], c = hv_corner(n, i), sd = hv_side(n, i), q = i % HV_W - (n - 1), r = i / HV_W - (n - 1);
        hv_around(n, i, nb);
        for (int k = 0; k < 6; k++) t->nb[i][k] = (int16_t)nb[k];
        t->mask[i] = (uint16_t)(c >= 0 ? 1 << c : sd >= 0 ? 1 << (6 + sd) : 0);
        int s = -q - r, aq = q < 0 ? -q : q, ar = r < 0 ? -r : r, as = s < 0 ? -s : s;
        t->centre[i] = (uint8_t)(aq > ar ? (aq > as ? aq : as) : (ar > as ? ar : as));
        t->list[t->count++] = (int16_t)i;
    }
}

static int root(hvs *s, int x)
{
    while (s->up[x] != x) x = s->up[x] = s->up[s->up[x]];
    return x;
}

static int root_c(const hvs *s, int x)            /* the same without shortening the ways (a const position) */
{
    while (s->up[x] != x) x = s->up[x];
    return x;
}

static int shape_mask(unsigned m) { return popc(m & 63) >= 2 || popc(m >> 6 & 63) >= 3; }

/* would player p's stone on the empty cell x make a shape? */
static int wins_at(const hvs *s, int x, int p)
{
    const int16_t *nb = s->t->nb[x];
    int own = p + 1, run_root[3], runs = 0, start = -1, f[6];
    unsigned m = s->t->mask[x];
    for (int k = 0; k < 6; k++) {
        f[k] = nb[k] >= 0 && s->cell[nb[k]] == own;
        if (f[k]) m |= s->mask[root_c(s, nb[k])];
        else if (start < 0) start = k;
    }
    if (shape_mask(m)) return 1;
    if (start < 0) return 0;                    /* wholly surrounded already: never (that ring came before) */
    for (int j = 1; j <= 6; j++) {              /* the runs round x, from a gap: a run's root against the others' */
        int k = (start + j) % 6;
        if (f[k] && !f[(k + 5) % 6]) {
            int r = root_c(s, nb[k]);
            for (int i = 0; i < runs; i++)
                if (run_root[i] == r) return 1;
            if (runs < 3) run_root[runs++] = r;
        }
    }
    for (int k = 0; k < 6; k++) {               /* a stone of p's next to x, now wholly p's */
        if (!f[k]) continue;
        const int16_t *nn = s->t->nb[nb[k]];
        int all = 1;
        for (int j = 0; j < 6 && all; j++) all = nn[j] >= 0 && (nn[j] == x || s->cell[nn[j]] == own);
        if (all) return 1;
    }
    return 0;
}

static void put(hvs *s, int x)
{
    int p = s->side, own = p + 1;
    if (wins_at(s, x, p)) s->winner = p;
    s->cell[x] = (uint8_t)own;
    s->up[x] = (int16_t)x;
    s->mask[x] = s->t->mask[x];
    for (int k = 0; k < 6; k++) {
        int y = s->t->nb[x][k];
        if (y < 0 || s->cell[y] != own) continue;
        int a = root(s, x), b = root(s, y);
        if (a != b) { s->up[a] = (int16_t)b; s->mask[b] |= s->mask[a]; }
    }
    s->last = x;
    s->stones++;
    s->side ^= 1;
}

static void load(hvs *s, const hvt *t, const hvgame *g)
{
    memset(s, 0, sizeof *s);
    s->t = t;
    s->winner = -1;
    s->last = -1;
    for (int i = 0; i < g->nmoves; i++) {       /* the stones in their order (a swap turns the first one) */
        int m = g->move[i];
        if (m == HV_SWAP) {
            s->cell[g->move[0]] = HV_EBONY;
            s->side = 0;
            continue;
        }
        s->side = i & 1;
        put(s, m);
    }
    s->side = hv_turn(g);
    s->winner = g->winner == HV_DRAW ? -1 : g->winner;
    s->last = hv_last(g);
}

/* ---- the search's game (rave.h) ---------------------------------------------------------------------------------- */
static int r_expand(const void *st, int *mv, float *prior, float *weight)
{
    const hvs *s = st;
    int k = 0;
    if (s->winner >= 0 || s->stones >= s->t->count) return 0;
    for (int i = 0; i < s->t->count; i++) {
        int c = s->t->list[i];
        if (s->cell[c]) continue;
        int near = 0;
        for (int j = 0; j < 6; j++) near |= s->t->nb[c][j] >= 0 && s->cell[s->t->nb[c][j]];
        mv[k] = c;
        prior[k] = near ? 0.55f : s->t->mask[c] ? 0.35f : 0.45f;   /* next to a stone; the rim a little less */
        weight[k] = near ? 4 : 3;
        k++;
    }
    return k;
}

static void r_make(void *st, int m) { put(st, m); }

static int r_side(const void *st) { return ((const hvs *)st)->side; }

static int r_over(const void *st, int *winner)
{
    const hvs *s = st;
    if (s->winner >= 0) { *winner = s->winner; return 1; }
    if (s->stones >= s->t->count) { *winner = -1; return 1; }
    return 0;
}

/* the bridge answer: the other just took x, one of the two common neighbours of two stones of `me`; the other one */
static int save_bridge(const hvs *s, int x, int me)
{
    const int16_t *nb = s->t->nb[x];
    for (int k = 0; k < 6; k++) {
        int a = nb[k], m = nb[(k + 1) % 6], b = nb[(k + 2) % 6];
        if (a >= 0 && b >= 0 && m >= 0 && s->cell[a] == me + 1 && s->cell[b] == me + 1 && !s->cell[m]) return m;
    }
    return -1;
}

static int r_playout(void *st, ai_rng *rg, int *mv, uint8_t *pl, int cap, int *len)
{
    hvs *s = st;
    int16_t empty[HV_SQ];
    int k = 0;
    for (int i = 0; i < s->t->count; i++)
        if (!s->cell[s->t->list[i]]) empty[k++] = s->t->list[i];
    for (int i = k - 1; i > 0; i--) {           /* the order the cells fill in */
        int j = ai_rand_n(rg, i + 1);
        int16_t t = empty[i];
        empty[i] = empty[j];
        empty[j] = t;
    }
    int next = 0;
    *len = 0;
    while (s->winner < 0 && s->stones < s->t->count) {
        int m = s->last >= 0 ? save_bridge(s, s->last, s->side) : -1;
        if (m < 0) {
            while (s->cell[empty[next]]) next++;
            m = empty[next];
        }
        if (*len < cap) { mv[*len] = m; pl[*len] = (uint8_t)s->side; (*len)++; }
        put(s, m);
    }
    return s->winner;
}

static const rave_game RG = {
    .size = sizeof(hvs), .moves_max = HV_SQ, .expand = r_expand, .make = r_make, .side = r_side,
    .over = r_over, .playout = r_playout, .rave_k = 500, .explore = 0, .expand_at = 2,
};

/* a cell near the stones: next to one (twice as likely by one's own), else two away; the middle when the board is
 * empty */
static int near_stones(const hvs *s, ai_rng *r)
{
    int pick[HV_SQ * 2], k = 0, own = s->side + 1;
    for (int i = 0; i < s->t->count; i++) {
        int c = s->t->list[i];
        if (s->cell[c]) continue;
        int mine = 0, any = 0;
        for (int j = 0; j < 6; j++) {
            int y = s->t->nb[c][j];
            if (y >= 0 && s->cell[y]) any = 1, mine |= s->cell[y] == own;
        }
        if (any) pick[k++] = c;
        if (mine) pick[k++] = c;
    }
    if (!k)
        for (int i = 0; i < s->t->count; i++)
            if (!s->cell[s->t->list[i]] && s->t->centre[s->t->list[i]] <= s->t->n / 2) pick[k++] = s->t->list[i];
    return k ? pick[ai_rand_n(r, k)] : -1;
}

int hvb_choose(const hvgame *g, int level, uint64_t seed, ai_stats *stats)
{
    if (g->winner >= 0) return HV_NONE;
    hvt t;
    tables(&t, g->n);
    hvs s;
    load(&s, &t, g);
    uint64_t key = (uint64_t)g->nmoves;
    for (int i = 0; i < t.count; i++) key = ai_mix(key, s.cell[t.list[i]]);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, key));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (stats) memset(stats, 0, sizeof *stats);
    int me = s.side, n = g->n;
    if (hv_can_swap(g)) {                       /* a first stone near the middle is worth taking */
        int far = t.centre[g->move[0]];
        if (far <= n / 2 && t.mask[g->move[0]] == 0 && (level != AI_EASY || ai_rand_n(&rng, 2))) return HV_SWAP;
    }
    if (g->nmoves == 0) {                       /* the opening: near the middle, or (the swap) further out */
        int want = g->swap && level != AI_EASY ? n / 2 + 1 : ai_rand_n(&rng, 2), pick[HV_SQ], k = 0;
        for (int i = 0; i < t.count; i++)
            if (t.centre[t.list[i]] == want) pick[k++] = t.list[i];
        if (k) return pick[ai_rand_n(&rng, k)];
    }
    int block = -1;
    for (int i = 0; i < t.count; i++) {         /* a win in one; the other's win in one */
        int c = t.list[i];
        if (s.cell[c]) continue;
        if (wins_at(&s, c, me)) return c;
        if (block < 0 && wins_at(&s, c, me ^ 1)) block = c;
    }
    if (block >= 0 && (level != AI_EASY || ai_rand_n(&rng, 2))) return block;
    if (level == AI_EASY) {
        int m = near_stones(&s, &rng);
        return m < 0 ? HV_NONE : m;
    }
    long iters = level == AI_MEDIUM ? 2500 : n == 6 ? 45000 : 25000;
    float rate;
    int m = rave_search(&RG, &s, iters, ai_level_budget(level).ms * 85 / 100, &rng, &rate, NULL, stats);
    return m < 0 ? HV_NONE : m;
}

static int think(int seat, int level, char *move, int cap)
{
    hvgame g = *havannah_live();                /* the copy, at entry: nothing else is read */
    if (g.winner >= 0 || hv_turn(&g) != seat || cap < 5) return 0;
    int m = hvb_choose(&g, level, ai_session_seed(), NULL);
    if (m == HV_NONE || ai_cancelled()) return 0;
    hv_name(m, move, cap);
    return 1;
}

static const bot havannah_bot = {.think = think, .play = havannah_show};
REGISTER_BOT(havannah, &havannah_bot)

/* the test's check of the quick shapes against the rules': would the player to move win with a stone on cell? */
int hvb_wins(const hvgame *g, int cell)
{
    hvt t;
    tables(&t, g->n);
    hvs s;
    load(&s, &t, g);
    return wins_at(&s, cell, s.side);
}
