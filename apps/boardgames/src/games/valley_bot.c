/*
 * Valley Harvest's bot (bots.h), two to four players. A plan (a house while it lacks workers, then workshops, then
 * manors) steers a greedy player: the best harvest for it (its yield, what the plan lacks, the market's price), then
 * the plan's building in the best place (a workshop where its land lies thick, a road towards land it cannot reach
 * yet or the unknown), the market for the one good it lacks, surplus sold dear. Easy: that player with a lot of
 * chance. Medium and hard: Monte Carlo tree search (ai_mcts) over every action, the plan as its playouts, played to
 * the end of the game. The face-down land is dealt anew in each playout (determinise): the bot never reads what lies
 * under it.
 * Runs on the worker thread: it copies the state at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("h12", "S20", "+w", "e"), played by valley_show().
 */
#include <math.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/valley.h"

static int shops_of(const vl_state *s, int p)
{
    return s->p[p].built[VL_SAW] + s->p[p].built[VL_MILL] + s->p[p].built[VL_QUARRY];
}

static int target(const vl_state *s, int p)
{
    if (s->p[p].workers < VL.workers_max && s->round < s->rounds - 2) return VL_HOUSE;
    if (shops_of(s, p) < 2 && s->round < s->rounds - 2 && vl_left(s, p, VL_SAW)) return VL_SAW;
    return VL_MANOR;
}

/* a workshop's worth at c: the free land of its kind beside it (more when it is in the player's reach) */
static int shop_value(const vl_state *s, int k, int c, int p)
{
    int t = k == VL_SAW ? VL_FOREST : k == VL_MILL ? VL_FIELD : VL_HILL, v = 0;
    for (int d = 0; d < 6; d++) {
        int n = vl_nb[c][d];
        if (n < 0 || !vl_in(s, n) || s->hidden[n]) continue;
        if (s->terr[n] == t && (s->bld[n] == VL_NONE || s->bld[n] == VL_ROAD)) v += 2 + vl_reach(s, p, n);
    }
    return v;
}

static int reach_count(const vl_state *s, int p)
{
    int n = 0;
    for (int c = 0; c < VL_CELLS; c++)
        if (vl_in(s, c) && !s->hidden[c] && s->terr[c] < VL_RIVER && (s->bld[c] == VL_NONE || s->bld[c] == VL_ROAD) &&
            vl_reach(s, p, c))
            n++;
    return n;
}

/* the planned player's action; eps: the chance of a random one (a random action, or the turn ended) */
int vlb_greedy(const vl_state *s, ai_rng *r, float eps)
{
    int mv[VL_MAXMOVES], n = vl_moves(s, mv), p = s->cur;
    if (n <= 1) return n ? mv[0] : AI_NONE;
    if (eps > 0 && ai_rand_f(r) < eps) {
        if (s->phase == VL_ACT && ai_rand_f(r) < 0.5f) return VL_MV_END;
        return mv[ai_rand_n(r, n)];
    }
    const vl_player *P = &s->p[p];
    int tg = target(s, p);
    if (s->phase == VL_HARVEST) {
        int best = mv[0];
        float bv = -1e9f;
        for (int i = 0; i < n; i++) {
            int c = mv[i];
            float v;
            if (s->hidden[c]) v = 1.4f;
            else {
                int t = s->terr[c], y = vl_yield(s, c), lack = VL.cost[tg][t] - P->goods[t];
                v = (float)y + 0.7f * (float)(lack > 0 ? (lack < y ? lack : y) : 0) + 0.15f * (float)vl_sell_price(s, t);
            }
            v += 0.01f * (float)ai_rand_n(r, 50);
            if (v > bv) { bv = v; best = c; }
        }
        return best;
    }
    int order[4] = {VL_MANOR, VL_HOUSE, VL_SAW, VL_ROAD};
    if (tg == VL_HOUSE) order[0] = VL_HOUSE, order[1] = VL_SAW, order[2] = VL_MANOR;
    if (tg == VL_SAW) order[0] = VL_SAW, order[1] = VL_MANOR, order[2] = VL_HOUSE;
    int rc = -1;
    for (int oi = 0; oi < 4; oi++) {
        int k = order[oi], bestm = -1;
        float bv = -1e9f;
        for (int i = 0; i < n; i++) {
            if (mv[i] >= VL_MV_BUY(0) || mv[i] < 64) continue;
            int kk = mv[i] / 64, c = mv[i] % 64;
            if (k == VL_SAW ? (kk < VL_SAW || kk > VL_QUARRY) : kk != k) continue;
            float v;
            if (kk >= VL_SAW && kk <= VL_QUARRY) {
                v = (float)shop_value(s, kk, c, p) - (float)vl_yield(s, c);
                if (v < 3) continue;
            } else if (kk == VL_ROAD) {
                if (rc < 0) rc = reach_count(s, p);
                if (rc >= P->workers + 2) continue;
                v = 0;
                for (int d = 0; d < 6; d++) {
                    int nb = vl_nb[c][d];
                    if (nb < 0 || !vl_in(s, nb)) continue;
                    if (s->hidden[nb]) v += 1;
                    else if (s->terr[nb] < VL_RIVER && s->bld[nb] == VL_NONE && !vl_reach(s, p, nb)) v += 2;
                }
                if (v <= 0) continue;
            } else
                v = -2.0f * (float)vl_yield(s, c) + (s->bld[c] == VL_ROAD ? 1 : 0);
            v += 0.01f * (float)ai_rand_n(r, 50);
            if (v > bv) { bv = v; bestm = mv[i]; }
        }
        if (bestm >= 0) return bestm;
    }
    int need = 0, gneed = -1;                   /* the market: the one good the plan lacks, surplus sold dear */
    for (int g = 0; g < VL_GOODS; g++)
        if (VL.cost[tg][g] > P->goods[g]) { need += VL.cost[tg][g] - P->goods[g]; gneed = g; }
    if (need == 1)
        for (int i = 0; i < n; i++)
            if (mv[i] == VL_MV_BUY(gneed)) return mv[i];
    for (int i = 0; i < n; i++)
        if (mv[i] >= VL_MV_SELL(0) && mv[i] < VL_MV_SELL(VL_GOODS)) {
            int g = mv[i] - VL_MV_SELL(0);
            if (P->goods[g] > VL.cost[tg][g] + 1 && vl_sell_price(s, g) >= 2) return mv[i];
        }
    return VL_MV_END;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out) { return vl_moves(st, out); }
static void a_make(void *st, int m) { vl_make(st, m); }
static int a_side(const void *st) { return ((const vl_state *)st)->cur; }

static float estimate_of(const vl_state *s, int p)
{
    const vl_player *P = &s->p[p];
    int left = s->rounds - s->round;
    float g = (float)(P->goods[0] + P->goods[1] + P->goods[2]);
    return (float)P->pp + (float)P->coins / (float)VL.coins_per_pt + 0.45f * g +
           (float)(P->workers - VL.workers0) * 0.9f * (float)left + 0.12f * (float)reach_count(s, p) * (left > 0);
}

static void rewards(const vl_state *s, float *rw)
{
    float e[VL_PLAYERS], m = -1e9f, sum = 0;
    if (s->over) {                              /* a win (shared by a tie), a little for the margin */
        int nw = 0;
        for (int p = 0; p < s->players; p++) {
            e[p] = (float)vl_score(s, p) + 0.01f * (float)s->p[p].coins;
            if (e[p] > m) m = e[p];
        }
        for (int p = 0; p < s->players; p++) nw += e[p] == m;
        for (int p = 0; p < s->players; p++) rw[p] = (e[p] == m ? 0.8f / (float)nw : 0) + 0.2f * (e[p] / (m + 1));
        return;
    }
    for (int p = 0; p < s->players; p++) {
        e[p] = estimate_of(s, p);
        if (e[p] > m) m = e[p];
    }
    for (int p = 0; p < s->players; p++) sum += rw[p] = expf((e[p] - m) / 2.0f);
    for (int p = 0; p < s->players; p++) rw[p] /= sum;
}

static int a_result(const void *st, float *rw)
{
    if (!((const vl_state *)st)->over) return 0;
    rewards(st, rw);
    return 1;
}
static int a_estimate(const void *st, float *rw) { rewards(st, rw); return 1; }
static int a_playout(const void *st, ai_rng *r) { return vlb_greedy(st, r, 0.25f); }

/* the face-down land dealt anew: what lies under it, the ruins too, shuffled among the face-down cells */
static void a_determinise(void *st, int seat, ai_rng *r)
{
    (void)seat;
    vl_state *s = st;
    int h[VL_CELLS], n = 0;
    for (int c = 0; c < VL_CELLS; c++)
        if (vl_in(s, c) && s->hidden[c]) h[n++] = c;
    for (int i = n - 1; i > 0; i--) {
        int j = ai_rand_n(r, i + 1);
        uint8_t t = s->terr[h[i]], g = s->gift[h[i]];
        s->terr[h[i]] = s->terr[h[j]], s->gift[h[i]] = s->gift[h[j]];
        s->terr[h[j]] = t, s->gift[h[j]] = g;
    }
}

static const long ITERS[AI_LEVELS] = {0, 300, 2500};

int vlb_choose(const vl_state *s, int level, uint64_t seed)
{
    if (s->over) return AI_NONE;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)s->turns * 131 + s->phase * 7 + s->p[s->cur].coins));
    if (level <= AI_EASY) return vlb_greedy(s, &r, 0.3f);
    int mv[VL_MAXMOVES];
    if (vl_moves(s, mv) == 1) return mv[0];
    ai_game g = {
        .size = sizeof(vl_state), .moves = a_moves, .make = a_make, .side = a_side, .players = s->players,
        .result = a_result, .playout = a_playout, .estimate = a_estimate, .cutoff = 0,
        .determinise = a_determinise,
    };
    ai_budget b = ai_level_budget(level);
    static const int SCALE[VL_PLAYERS + 1] = {3, 3, 6, 4, 3};   /* fewer, longer playouts with more players */
    long iters = ITERS[level > AI_HARD ? AI_HARD : level] * SCALE[s->players] / 3;
    /* the time cap a little under the level's: the search looks at the clock every 64 playouts, a long one each */
    ai_opts o = {.budget = {.ms = b.ms * 3 / 4, .nodes = iters}, .rng = &r, .uct_c = 0.3f};
    return ai_mcts(&g, s, &o, NULL);
}

static int think(int seat, int level, char *move, int cap)
{
    vl_state s = valley_live()->s;              /* the copy, at entry: nothing else is read */
    if (s.over || s.cur != seat || cap < 6) return 0;
    int m = vlb_choose(&s, level, ai_session_seed());
    if (m == AI_NONE || ai_cancelled() || !vl_legal(&s, m)) return 0;
    vl_name(m, move);
    return 1;
}

static const bot valley_bot = {.think = think, .play = valley_show};
REGISTER_BOT(valley, &valley_bot)
