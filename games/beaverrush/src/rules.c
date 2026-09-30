/*
 * Beaver Rush: the trunk, its generator and the timer curve (pure functions: tests/test_rules.c).
 * (c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved: games/beaverrush/LICENSE.
 *
 * The generator lays the trunk out in small chunks (a single segment, a same-side stack, a zig-zag run),
 * then emits it one segment at a time under the fairness rule: a branch never sits right above a branch
 * of the other side (the beaver could neither stay nor move: DESIGN.md "Fairness"). When the next wanted
 * branch would break it, an empty segment is emitted first.
 */
#include "br.h"
#include <string.h>

static int imin(int a, int b) { return a < b ? a : b; }

int gen_stage(const gen *g) { return g->made / STAGE_LOGS; }

void gen_init(gen *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    rs_rng_seed(&g->rng, seed ? seed : 0x8bc0ffeeu);
    g->since_gold = 0;
}

static int roll(gen *g, int n) { return rs_rng_range(&g->rng, n); }

/* the next chunk: wanted branch sides, 0 = an empty segment */
static void plan(gen *g)
{
    int stage = gen_stage(g);
    int pz = stage >= ZIG_FROM_STAGE ? imin(P_ZIG0 + P_ZIG_STEP * (stage - ZIG_FROM_STAGE), P_ZIG_MAX) : 0;
    int ps = stage >= 1 ? P_STACK : 0;
    int pb = imin(P_BRANCH0 + P_BRANCH_STEP * stage, P_BRANCH_MAX);
    int r = roll(g, 100);
    int side = 1 + roll(g, 2);
    g->qn = g->qi = 0;
    if (r < pz) {
        int kmax = imin(ZIG_MAX, ZIG_MIN + (stage - ZIG_FROM_STAGE) / 2);
        int k = ZIG_MIN + roll(g, kmax - ZIG_MIN + 1);
        for (int i = 0; i < k; i++) {
            if (i) g->queue[g->qn++] = 0;
            g->queue[g->qn++] = (uint8_t)side;
            side = other_side(side);
        }
    } else if (r < pz + ps) {
        int k = 2 + roll(g, 2);
        for (int i = 0; i < k; i++) g->queue[g->qn++] = (uint8_t)side;
    } else {
        g->queue[g->qn++] = (uint8_t)(roll(g, 100) < pb ? side : 0);
    }
}

seg gen_next(gen *g)
{
    seg s = {0, 0, 0, 0};
    s.look = (uint8_t)roll(g, 4);
    int want = 0;
    if (g->made >= START_CLEAR) {
        if (g->qi >= g->qn) plan(g);
        want = g->queue[g->qi];
        if (want && g->last && want != g->last) {
            want = 0;                                   /* the fairness gap; the branch comes next */
        } else {
            g->qi++;
            if (want && want == g->last && g->same_run >= MAX_SAME_RUN) want = 0;
        }
        if (!want && !g->last && g->empty_run >= MAX_EMPTY_RUN) want = 1 + roll(g, 2);
        if (!want && g->since_gold >= GOLD_SPACING && roll(g, GOLD_ODDS) == 0) s.gold = 1;
    }
    s.branch = (uint8_t)want;
    g->same_run = want ? (want == g->last ? g->same_run + 1 : 1) : 0;
    g->empty_run = want ? 0 : g->empty_run + 1;
    g->since_gold = s.gold ? 0 : g->since_gold + 1;
    g->last = want;
    g->made++;
    return s;
}

void tree_init(tree *t, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    gen_init(&t->g, seed);
    for (int i = 0; i < TRUNK_N; i++) {
        if (i == SEG_SHOWN) t->g_at8 = t->g;
        t->s[i] = gen_next(&t->g);
    }
}

void tree_reroll(tree *t, uint32_t seed)
{
    t->g = t->g_at8;
    rs_rng_seed(&t->g.rng, seed ? seed : 0x8bc0ffeeu);
    for (int i = SEG_SHOWN; i < TRUNK_N; i++) t->s[i] = gen_next(&t->g);
}

void tree_pop(tree *t)
{
    memmove(&t->s[0], &t->s[1], sizeof t->s[0] * (TRUNK_N - 1));
    t->s[TRUNK_N - 1] = gen_next(&t->g);
}

int tree_insert_branch(tree *t, int from, int pref)
{
    for (int i = from < 1 ? 1 : from; i < TRUNK_N - 1; i++) {
        seg *s = &t->s[i];
        if (s->branch || s->gold) continue;
        int below = t->s[i - 1].branch, above = t->s[i + 1].branch;
        for (int k = 0; k < 2; k++) {
            int x = k ? other_side(pref) : pref;
            if ((!below || below == x) && (!above || above == x)) {
                s->branch = (uint8_t)x;
                s->stolen = 1;
                return i;
            }
        }
    }
    return -1;
}

int trunk_fair(const seg *s, int n)
{
    for (int i = 0; i + 1 < n; i++)
        if (s[i].branch && s[i + 1].branch && s[i].branch != s[i + 1].branch) return 0;
    return 1;
}

/* ---- the timer ------------------------------------------------------------------------------------ */
int level_of(int logs) { return imin(logs / LEVEL_LOGS, LEVEL_MAX); }

int rate_q16(int level)
{
    level = clampi(level, 0, LEVEL_MAX);
    int64_t r = RATE_Q16(LEVEL_LOGS * level + LEVEL_MID);
    if (level > LEVEL_ENDGAME) r += (level - LEVEL_ENDGAME) * ENDGAME_RATE_Q16;
    return (int)r;
}

/* the drain that the refill of rate_q16(level) gnaws per second exactly compensates */
int32_t drain_per_frame(int level)
{
    return (int32_t)((int64_t)REFILL * rate_q16(level) / (60 * 65536));
}
