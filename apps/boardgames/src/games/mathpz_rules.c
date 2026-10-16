/*
 * Number Puzzles' rules (mathpz.h): the tiles, the merges and undo, the solver (every way, the nice ones, Target's
 * fewest operations), the puzzles made at random for each variant, the levels and the notation. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mathpz.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define BIG 1000000000LL                        /* results beyond this are refused (never needed) */

int npz_tiles(int kind) { return kind == NPZ_TARGET ? 6 : kind == NPZ_FIVE ? 5 : 4; }

static int64_t gcd64(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    while (b) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1;
}

static int q_make(int64_t n, int64_t d, npz_q *r)
{
    if (d == 0) return 0;
    if (d < 0) n = -n, d = -d;
    int64_t g = gcd64(n, d);
    n /= g, d /= g;
    if (n > BIG || n < -BIG || d > BIG) return 0;
    r->n = (int32_t)n;
    r->d = (int32_t)d;
    return 1;
}

static npz_q q_int(int v) { return (npz_q){v, 1}; }
static int q_is(npz_q a, int v) { return a.d == 1 && a.n == v; }

int npz_apply(int kind, npz_q a, int op, npz_q b, npz_q *r)
{
    int64_t an = a.n, ad = a.d, bn = b.n, bd = b.d;
    int ok;
    switch (op) {
    case NPZ_ADD: ok = q_make(an * bd + bn * ad, ad * bd, r); break;
    case NPZ_SUB: ok = q_make(an * bd - bn * ad, ad * bd, r); break;
    case NPZ_MUL: ok = q_make(an * bn, ad * bd, r); break;
    case NPZ_DIV: ok = bn != 0 && q_make(an * bd, ad * bn, r); break;
    default: return 0;
    }
    if (ok && kind == NPZ_TARGET && (r->d != 1 || r->n <= 0)) return 0;   /* whole numbers above zero */
    return ok;
}

void npz_load(npz_game *g, int kind, const int *nums, int target)
{
    memset(g, 0, sizeof *g);
    g->kind = kind < 0 || kind >= NPZ_GAMES ? NPZ_FOUR : kind;
    g->n = npz_tiles(g->kind);
    g->target = target;
    for (int i = 0; i < g->n; i++) {
        g->v[i] = q_int(nums[i]);
        g->live[i] = 1;
        g->start[i] = (uint8_t)nums[i];
    }
}

int npz_left(const npz_game *g)
{
    int k = 0;
    for (int i = 0; i < g->n; i++) k += g->live[i];
    return k;
}

int npz_nearest(const npz_game *g)
{
    int best = -1;
    int64_t bd = 0;
    for (int i = 0; i < g->n; i++) {
        if (!g->live[i] || g->v[i].d != 1) continue;
        int64_t d = (int64_t)g->v[i].n - g->target;
        if (d < 0) d = -d;
        if (best < 0 || d < bd) best = i, bd = d;
    }
    return best;
}

int npz_off(const npz_game *g)
{
    int i = npz_nearest(g);
    if (i < 0) return 1 << 30;
    int64_t d = (int64_t)g->v[i].n - g->target;
    d = d < 0 ? -d : d;
    return d > (1 << 30) ? 1 << 30 : (int)d;
}

int npz_solved(const npz_game *g)
{
    if (g->kind == NPZ_TARGET) return g->done || npz_off(g) == 0;
    if (npz_left(g) != 1) return 0;
    for (int i = 0; i < g->n; i++)
        if (g->live[i]) return q_is(g->v[i], g->target);
    return 0;
}

int npz_stuck(const npz_game *g) { return g->kind != NPZ_TARGET && npz_left(g) == 1 && !npz_solved(g); }

int npz_can_finish(const npz_game *g)
{
    return g->kind == NPZ_TARGET && !npz_solved(g) && npz_off(g) <= NPZ_NEAR;
}

int npz_finish(npz_game *g)
{
    if (!npz_can_finish(g)) return 0;
    g->done = 1;
    return 1;
}

int npz_merge(npz_game *g, int a, int op, int b)
{
    if (a < 0 || b < 0 || a >= g->n || b >= g->n || a == b || !g->live[a] || !g->live[b] || npz_solved(g)) return 0;
    npz_q r;
    if (!npz_apply(g->kind, g->v[a], op, g->v[b], &r)) return 0;
    g->hist[g->nh].s = (npz_step){(int8_t)a, (int8_t)op, (int8_t)b};
    g->hist[g->nh].va = g->v[a];
    g->hist[g->nh].vb = g->v[b];
    g->hist[g->nh].mb = g->made[b];
    g->nh++;
    g->live[a] = 0;
    g->v[b] = r;
    g->made[b] = 1;
    if (npz_stuck(g)) g->mistakes++;
    return 1;
}

int npz_undo(npz_game *g)
{
    if (g->done) {
        g->done = 0;
        return 1;
    }
    if (!g->nh) return 0;
    g->nh--;
    int a = g->hist[g->nh].s.a, b = g->hist[g->nh].s.b;
    g->live[a] = 1;
    g->v[a] = g->hist[g->nh].va;
    g->v[b] = g->hist[g->nh].vb;
    g->made[b] = g->hist[g->nh].mb;
    return 1;
}

static int logic_stars(int hints, int mistakes)   /* the shell's ui_logic_stars, the same rule */
{
    int k = hints + mistakes;
    return k == 0 ? 3 : k <= 2 ? 2 : 1;
}

int npz_stars(const npz_game *g)
{
    if (!npz_solved(g)) return 0;
    int cap = logic_stars(g->hints, g->mistakes), base = 3;
    if (g->kind == NPZ_TARGET) {
        int off = npz_off(g);
        base = off == 0 ? 3 : off <= 5 ? 2 : 1;
    }
    return base < cap ? base : cap;
}

/* ---- the solver ------------------------------------------------------------------------------------------------- */
enum { M_ANY, M_NICE, M_PLAIN };

typedef struct {
    int kind, target, mode, first_only, found;
    long count;
    npz_step first;
} srch;

/* the operations on a pair (x, y): each as left op right, `swap` 1 when the left one is y */
static const struct { int op, swap; } PAIR_OPS[6] = {
    {NPZ_ADD, 0}, {NPZ_MUL, 0}, {NPZ_SUB, 0}, {NPZ_SUB, 1}, {NPZ_DIV, 0}, {NPZ_DIV, 1},
};

/* Four numbers, All five: every tile used, the last one the target */
static long rec_all(srch *s, const npz_q *v, const int8_t *slot, int m, int depth)
{
    if (m == 1) return q_is(v[0], s->target);
    long n = 0;
    for (int i = 0; i < m; i++)
        for (int j = i + 1; j < m; j++)
            for (int k = 0; k < 6; k++) {
                int op = PAIR_OPS[k].op, sw = PAIR_OPS[k].swap;
                if (sw && v[i].n == v[j].n && v[i].d == v[j].d) continue;   /* the same both ways */
                if (s->mode == M_PLAIN && op == NPZ_DIV) continue;
                npz_q l = sw ? v[j] : v[i], rgt = sw ? v[i] : v[j], r;
                if (!npz_apply(NPZ_FOUR, l, op, rgt, &r)) continue;
                if (s->mode != M_ANY && (r.d != 1 || r.n < 0)) continue;
                npz_q w[NPZ_MAX];
                int8_t ws[NPZ_MAX];
                int c = 0;
                for (int t = 0; t < m; t++)
                    if (t != i && t != j) w[c] = v[t], ws[c++] = slot[t];
                w[c] = r;
                ws[c++] = sw ? slot[i] : slot[j];
                long got = rec_all(s, w, ws, c, depth + 1);
                if (got && depth == 0 && !s->found) {
                    s->found = 1;
                    s->first = (npz_step){sw ? slot[j] : slot[i], (int8_t)op, sw ? slot[i] : slot[j]};
                }
                n += got;
                if (s->first_only && got) return n;
            }
    return n;
}

/* Target: whole numbers; a hit when a result is the target, within `left` more operations. Results equal to an
 * operand are left out (x 1, / 1, 2b - b...: the same as leaving a tile unused). */
static long rec_t(srch *s, const int64_t *v, const int8_t *slot, int m, int left, int depth)
{
    long n = 0;
    for (int i = 0; i < m; i++)
        for (int j = i + 1; j < m; j++) {
            int64_t a = v[i], b = v[j];
            for (int k = 0; k < 6; k++) {
                int op = PAIR_OPS[k].op, sw = PAIR_OPS[k].swap;
                int64_t l = sw ? b : a, rg = sw ? a : b, r;
                if (op == NPZ_ADD) r = l + rg;
                else if (op == NPZ_MUL) r = l * rg;
                else if (op == NPZ_SUB) r = l - rg;
                else r = rg && l % rg == 0 ? l / rg : 0;
                if (r <= 0 || r == a || r == b || r > BIG) continue;
                if (sw && (op == NPZ_ADD || op == NPZ_MUL)) continue;
                long got = 0;
                if (r == s->target) got = 1;
                else if (left > 1 && m > 2) {
                    int64_t w[NPZ_MAX];
                    int8_t ws[NPZ_MAX];
                    int c = 0;
                    for (int t = 0; t < m; t++)
                        if (t != i && t != j) w[c] = v[t], ws[c++] = slot[t];
                    w[c] = r;
                    ws[c++] = sw ? slot[i] : slot[j];
                    got = rec_t(s, w, ws, c, left - 1, depth + 1);
                }
                if (got && depth == 0 && !s->found) {
                    s->found = 1;
                    s->first = (npz_step){sw ? slot[j] : slot[i], (int8_t)op, sw ? slot[i] : slot[j]};
                }
                n += got;
                if (s->first_only && got) return n;
            }
        }
    return n;
}

static int gather(const npz_game *g, npz_q *v, int64_t *iv, int8_t *slot)
{
    int m = 0;
    for (int i = 0; i < g->n; i++)
        if (g->live[i]) {
            v[m] = g->v[i];
            iv[m] = g->v[i].n;
            slot[m++] = (int8_t)i;
        }
    return m;
}

int npz_next(const npz_game *g, npz_step *st)
{
    if (npz_solved(g)) return 0;
    npz_q v[NPZ_MAX];
    int64_t iv[NPZ_MAX];
    int8_t slot[NPZ_MAX];
    int m = gather(g, v, iv, slot);
    srch s = {.kind = g->kind, .target = g->target, .first_only = 1};
    if (g->kind == NPZ_TARGET) {
        for (int left = 1; left < m && !s.found; left++) rec_t(&s, iv, slot, m, left, 0);
    } else {
        if (m < 2) return 0;
        s.mode = M_NICE;
        rec_all(&s, v, slot, m, 0);
        if (!s.found) {
            s.mode = M_ANY;
            rec_all(&s, v, slot, m, 0);
        }
    }
    if (s.found && st) *st = s.first;
    return s.found;
}

int npz_hint(npz_game *g, npz_step *st)
{
    if (npz_solved(g)) return 0;
    npz_step s;
    if (npz_next(g, &s) && npz_merge(g, s.a, s.op, s.b)) {
        g->hints++;
        if (st) *st = s;
        return 1;
    }
    if (g->nh && npz_undo(g)) {
        g->hints++;
        return 2;
    }
    return 0;
}

/* Four numbers, All five: the ways in one mode (M_*) */
static long ways_in(int kind, const int *nums, int target, int mode)
{
    int m = npz_tiles(kind);
    npz_q v[NPZ_MAX];
    int8_t slot[NPZ_MAX];
    for (int i = 0; i < m; i++) v[i] = q_int(nums[i]), slot[i] = (int8_t)i;
    srch s = {.kind = kind, .target = target, .mode = mode};
    return rec_all(&s, v, slot, m, 0);
}

void npz_rate(int kind, const int *nums, int target, npz_rating *r)
{
    memset(r, 0, sizeof *r);
    int m = npz_tiles(kind);
    int64_t iv[NPZ_MAX];
    int8_t slot[NPZ_MAX];
    for (int i = 0; i < m; i++) iv[i] = nums[i], slot[i] = (int8_t)i;
    srch s = {.kind = kind, .target = target};
    if (kind == NPZ_TARGET) {
        for (int i = 0; i < m; i++)
            if (nums[i] == target) return;      /* there already: not a puzzle */
        for (int left = 1; left < m; left++) {
            s.count = 0;
            long n = rec_t(&s, iv, slot, m, left, 0);
            if (n) {
                r->fewest = left;
                r->ways_fewest = n;
                break;
            }
        }
        return;
    }
    r->ways = ways_in(kind, nums, target, M_ANY);
    r->nice = ways_in(kind, nums, target, M_NICE);
    r->plain = ways_in(kind, nums, target, M_PLAIN);
}

/* ---- puzzles at random ------------------------------------------------------------------------------------------- */
/* All five's numbers, as a deck of 1..25 would deal them (the small ones three times, 11..17 twice, the rest once) */
static int five_card(uint32_t *rs, int top)
{
    for (;;) {
        int v = 1 + pr_below(rs, 25), w = v <= 10 ? 3 : v <= 17 ? 2 : 1;
        if (v <= top && pr_below(rs, 3) < w) return v;
    }
}

static const int OTHER_TARGETS[] = {10, 12, 15, 18, 20, 30, 36, 40, 48, 60};

/* does a puzzle fit a tier? Its ways counted only as far as needed (the nice ones first: the fewest to try). Easy:
 * many ways without a division; medium: some nice ways; hard: few of them, or fractions needed. */
static int fits(int kind, int tier, const int *nums, int target)
{
    static const long PLAIN_EASY[2] = {12, 300}, NICE_MID[2][2] = {{1, 40}, {10, 300}}, NICE_HARD[2] = {8, 24};
    int k = kind == NPZ_FIVE;
    long nice = ways_in(kind, nums, target, M_NICE);
    if (tier == 0) return nice >= PLAIN_EASY[k] && ways_in(kind, nums, target, M_PLAIN) >= PLAIN_EASY[k];
    if (tier == 1) return nice >= NICE_MID[k][0] && nice <= NICE_MID[k][1];
    if (nice > NICE_HARD[k]) return 0;
    return nice > 0 || ways_in(kind, nums, target, M_ANY) > 0;
}

/* Target: an operation chain made at random (k operations on k + 1 of the tiles), its result the target */
static int target_build(uint32_t *rs, const int *nums, int k)
{
    int64_t v[NPZ_MAX];
    int m = 6;
    for (int i = 0; i < m; i++) v[i] = nums[i];
    for (int step = 0; step < k; step++) {
        int ok = 0;
        for (int tries = 0; tries < 40 && !ok; tries++) {
            int i = pr_below(rs, m), j = pr_below(rs, m - 1);
            if (j >= i) j++;
            int64_t a = v[i], b = v[j], r;
            int op = pr_below(rs, 4);
            if (op == NPZ_ADD) r = a + b;
            else if (op == NPZ_MUL) r = a * b;
            else if (op == NPZ_SUB) r = a - b;
            else r = b && a % b == 0 ? a / b : 0;
            if (r <= 0 || r == a || r == b || r > 5000) continue;
            v[j] = r;
            v[i] = v[--m];
            if (j == m) j = i;
            ok = 1;
            if (step == k - 1) return (int)r;
        }
        if (!ok) return 0;
    }
    return 0;
}

static void target_nums(uint32_t *rs, int bigs, int *nums)
{
    static const int BIGS[4] = {25, 50, 75, 100};
    int smalls[20], used[4] = {0};
    for (int i = 0; i < 20; i++) smalls[i] = 1 + i / 2;
    pr_shuffle(rs, smalls, 20);
    int c = 0;
    while (c < bigs) {
        int b = pr_below(rs, 4);
        if (used[b]) continue;
        used[b] = 1;
        nums[c++] = BIGS[b];
    }
    for (int i = 0; c < 6; i++) nums[c++] = smalls[i];
}

void npz_make(int variant, uint32_t seed, int *nums, int *target)
{
    int kind = variant / NPZ_TIERS, tier = variant % NPZ_TIERS;
    if (kind < 0 || kind >= NPZ_GAMES) kind = NPZ_FOUR, tier = 0;
    uint32_t rs = pr_seed(seed);
    npz_rating r;
    if (kind == NPZ_TARGET) {
        for (int tries = 0;; tries++) {
            int bigs = tier == 2 ? pr_below(&rs, 3) : 1 + pr_below(&rs, 2);
            int k = tier == 0 ? 2 : tier == 1 ? 3 : 4 + pr_below(&rs, 2);
            target_nums(&rs, bigs, nums);
            int t = target_build(&rs, nums, k);
            if (t < 101 || t > 999) continue;
            npz_rate(kind, nums, t, &r);
            if (r.fewest == k || (tries > 400 && r.fewest)) {
                *target = t;
                break;
            }
        }
    } else {
        for (int tries = 0;; tries++) {
            int t = 24, n = npz_tiles(kind);
            if (kind == NPZ_FOUR) {
                int top = tier == 0 ? 10 : 13;
                for (int i = 0; i < n; i++) nums[i] = 1 + pr_below(&rs, top);
                if (tier == 2 && pr_below(&rs, 2)) t = OTHER_TARGETS[pr_below(&rs, 10)];
            } else {
                int top = tier == 0 ? 12 : tier == 1 ? 17 : 25;
                for (int i = 0; i < n; i++) nums[i] = five_card(&rs, top);
                t = five_card(&rs, 25);
            }
            if (fits(kind, tier, nums, t) || (tries > 300 && ways_in(kind, nums, t, M_ANY) > 0)) {
                *target = t;
                break;
            }
        }
    }
    for (int i = npz_tiles(kind); i < NPZ_MAX; i++) nums[i] = 0;
}

void npz_new(npz_game *g, int variant, uint32_t seed)
{
    int nums[NPZ_MAX], t;
    npz_make(variant, seed, nums, &t);
    npz_load(g, variant / NPZ_TIERS, nums, t);
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int op_of(char c)
{
    return c == '+' ? NPZ_ADD : c == '-' ? NPZ_SUB : c == 'x' || c == '*' ? NPZ_MUL : c == '/' || c == ':' ? NPZ_DIV
                                                                                                             : -1;
}

int npz_script(npz_game *g, const char *moves)
{
    const char *p = moves;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *e = p;
        while (*e && !isspace((unsigned char)*e)) e++;
        int len = (int)(e - p);
        if (len == 1 && *p == 'h') {
            if (!npz_hint(g, NULL)) return 0;
        } else if (len == 1 && *p == 'u') {
            if (!npz_undo(g)) return 0;
        } else if (len == 1 && *p == '!') {
            if (!npz_finish(g)) return 0;
        } else if (len == 1 && *p == '=') {
            npz_step s;
            while (!npz_solved(g) && npz_next(g, &s))
                if (!npz_merge(g, s.a, s.op, s.b)) return 0;
            if (!npz_solved(g)) return 0;
        } else if (len == 3 && p[0] >= 'a' && p[2] >= 'a' && op_of(p[1]) >= 0) {
            if (!npz_merge(g, p[0] - 'a', op_of(p[1]), p[2] - 'a')) return 0;
        } else {
            return 0;
        }
        p = e;
    }
    return 1;
}

/* ---- the levels -------------------------------------------------------------------------------------------------- */
int npz_level_kind(int level)
{
    int c = level / NPZ_PER_CHAPTER / 3;
    return c < 0 ? NPZ_FOUR : c >= NPZ_GAMES ? NPZ_FIVE : c;
}

#ifndef NPZ_GEN                                 /* not in the levels' tool (tools/mathpz_gen.c): it makes them */
#include "games/mathpz_levels.h"

int npz_level_nums(int level, int *nums, int *target)
{
    if (level < 0 || level >= NPZ_LEVELS) return 0;
    int n = npz_tiles(npz_level_kind(level));
    for (int i = 0; i < NPZ_MAX; i++) nums[i] = i < n ? npz_lv_nums[level][i] : 0;
    *target = npz_lv_target[level];
    return n;
}

int npz_level(npz_game *g, int level)
{
    int nums[NPZ_MAX], t;
    if (!npz_level_nums(level, nums, &t)) return 0;
    npz_load(g, npz_level_kind(level), nums, t);
    return 1;
}
#endif
