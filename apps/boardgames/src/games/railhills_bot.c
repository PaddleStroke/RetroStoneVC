/*
 * Valley Rails' bot (bots.h). Two pieces, both pure (no SDL, no statics written):
 *   the planner (greedy): it values each contract it holds and each public line by its value against what is left to
 *   build (Dijkstra over the map: its own track and towns free, empty hexes their terrain, face-down ones a guess,
 *   other players' track its rent), builds along the best one's route, delivers when the rent is small, surveys with
 *   points it cannot spend, draws when it has nothing worth building and keeps what its network reaches cheaply;
 *   the search (hard): a flat Monte Carlo over the turn's plans (the planner's, one per target, drawing, surveying,
 *   delivering now or not): each plan is played on copies whose hidden things (the others' hands, the decks, the
 *   face-down hexes) are dealt again at random, then every player plays the planner for a few turns, and the
 *   position is scored (coins, and the contracts in hand by what is left to build). The best plan's first move.
 * Easy is the planner with noise (a target picked loosely, no surveys), medium the planner, hard the search.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/railhills.h"

#define HIDDEN_W 16                             /* a face-down hex's guessed cost, tenths */
#define RENT_W 13                               /* a hex of another player's track, tenths of a build point */

typedef struct { uint64_t s; } prng;
static uint32_t pr_next(prng *r)
{
    uint64_t z = (r->s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (uint32_t)((z ^ (z >> 31)) >> 16);
}
static int pr_n(prng *r, int n) { return (int)(pr_next(r) % (uint32_t)n); }
static double pr_f(prng *r) { return (pr_next(r) & 0xffffff) / 16777216.0; }

/* ---- the planner's view of a target ------------------------------------------------------------------------- */
typedef struct {
    int id, pub, slot;                          /* the contract; 1 a public line (its slot), else the hand's slot */
    int value;
    int est;                                    /* tenths of build points (and rent) left */
    int need_pts, need_pieces, rent, hidden;    /* along the planned route */
    int path[RH_MAXC], npath;
} target;

static void plan_weights(const rh_state *s, int p, int *w, int rent_w)
{
    for (int c = 0; c < s->nc; c++) {
        int t = s->terr[c];
        if (t == RH_SEA) w[c] = -1;
        else if (s->hidden[c]) w[c] = HIDDEN_W;    /* face down: a guess, never its land */
        else if (t == RH_LAKE) w[c] = -1;
        else if (t == RH_TOWN || s->owner[c] == p) w[c] = 0;
        else if (s->owner[c] >= 0) w[c] = rent_w;
        else w[c] = rh_cost_of[t] > s->cfg.pts ? -1 : 10 * rh_cost_of[t];
    }
}

static void plan_target(const rh_state *s, int p, const int *w, target *t)
{
    int dist[RH_MAXC], prev[RH_MAXC];
    const rh_con *k = &s->con[t->id];
    int a = s->town[k->a], b = s->town[k->b];
    rh_paths(s, w, a, dist, prev);
    t->npath = 0;
    t->need_pts = t->need_pieces = t->rent = t->hidden = 0;
    if (dist[b] >= RH_FAR) { t->est = RH_FAR; return; }
    t->est = dist[b];
    for (int c = b; c >= 0; c = prev[c]) t->path[t->npath++] = c;
    for (int i = 0; i < t->npath; i++) {
        int c = t->path[i];
        if (s->terr[c] == RH_TOWN || s->owner[c] == p) continue;
        if (s->owner[c] >= 0) t->rent++;
        else if (s->hidden[c]) t->hidden++, t->need_pts += 2, t->need_pieces++;
        else t->need_pts += rh_cost_of[s->terr[c]], t->need_pieces++;
    }
}

/* the targets p can work for: its hand, then the public lines */
static int targets(const rh_state *s, int p, target *T)
{
    int w[RH_MAXC], n = 0;
    plan_weights(s, p, w, RENT_W * s->cfg.rent);
    for (int i = 0; i < s->pl[p].nhand; i++) {
        target *t = &T[n++];
        t->id = s->pl[p].hand[i], t->pub = 0, t->slot = i, t->value = s->con[t->id].value;
        plan_target(s, p, w, t);
    }
    for (int j = 0; j < RH_PUB; j++)
        if (s->pub[j] >= 0) {
            target *t = &T[n++];
            t->id = s->pub[j], t->pub = 1, t->slot = j, t->value = s->con[t->id].value + s->cfg.pub_bonus;
            plan_target(s, p, w, t);
        }
    return n;
}

/* about how many turns each player has left */
static int turns_left(const rh_state *s)
{
    int least = 255;
    for (int q = 0; q < s->np; q++)
        if (s->pl[q].track < least) least = s->pl[q].track;
    int t = (least * 10 + 9) / (s->cfg.pts * 7);   /* about 1.4 points a piece */
    int r = s->cfg.max_rounds - s->round + 1;
    if (r < t) t = r;
    if (s->ending) t = 0;
    return t < 0 ? 0 : t;
}

/* a target's worth to the planner: its value for what is left, 0 when it cannot be done in time */
static double worth(const rh_state *s, int p, const target *t)
{
    if (t->est >= RH_FAR) return -1;
    int tl = turns_left(s);
    if (t->need_pieces > s->pl[p].track) return -1;
    if (t->need_pts > s->cfg.pts * (tl + 1)) return -1;
    double v = t->value;
    if (t->pub) v *= s->np > 2 ? 0.6 : 0.75;    /* the others may take it first */
    return v / (t->est / 10.0 + 1.0);
}

/* the empty hexes of a route p could build now (cheapest first, then nearest an end) */
static int route_step(const rh_state *s, int p, const target *t)
{
    int best = -1, bk = 99;
    for (int i = 0; i < t->npath; i++) {
        int c = t->path[i];
        if (!rh_can_build(s, p, c)) continue;
        if (s->hidden[c] && s->pts < 2) continue;   /* blind only with the points for a hill */
        int k = (s->hidden[c] ? 7 : rh_cost_of[s->terr[c]] * 4) + (i < t->npath - 1 - i ? i : t->npath - 1 - i) / 3;
        if (k < bk) bk = k, best = c;
    }
    return best;
}

static int keep_choice(const rh_state *s, int p, double noise, prng *r)
{
    /* each offered contract's cost from p's network now */
    int w[RH_MAXC];
    plan_weights(s, p, w, RENT_W * s->cfg.rent);
    double sc[RH_OFFER];
    int need[RH_OFFER];
    for (int i = 0; i < s->noffer; i++) {
        target t;
        t.id = s->offer[i], t.pub = 0, t.value = s->con[t.id].value;
        plan_target(s, p, w, &t);
        need[i] = t.need_pts;
        sc[i] = t.est >= RH_FAR ? -1 : t.value / (t.est / 10.0 + 1.0) + noise * pr_f(r);
    }
    /* the points already promised to the hand */
    int promised = 0;
    target T[RH_HAND + RH_PUB];
    int nt = targets(s, p, T);
    for (int i = 0; i < nt; i++)
        if (!T[i].pub && T[i].est < RH_FAR) promised += T[i].need_pts;
    int budget = s->cfg.pts * (turns_left(s) + 1) * 3 / 4 - promised;
    int order[RH_OFFER], n = s->noffer;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && sc[order[j]] > sc[order[j - 1]]; j--) {
            int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
        }
    unsigned mask = 0;
    int kept = 0;
    for (int i = 0; i < n; i++) {
        int o = order[i];
        int must = kept < s->keep_min;
        if (!must && (sc[o] < 0.45 || need[o] > budget)) continue;
        if (s->pl[p].nhand + kept >= RH_HAND) break;
        mask |= 1u << o, kept++, budget -= need[o];
    }
    if (!mask) mask = 1u << order[0];
    return RH_M_KEEP + (int)mask;
}

/* the planner: p's next move. noise > 0: looser choices (the easy level); focus >= 0: work for that target only (its
 * index in targets()), -1 the best */
static int greedy(const rh_state *s, double noise, int focus, prng *r)
{
    int p = s->turn;
    if (s->phase == RH_KEEP) return keep_choice(s, p, noise, r);
    if (s->phase != RH_PLAY) return AI_NONE;
    target T[RH_HAND + RH_PUB];
    int nt = targets(s, p, T);
    int tl = turns_left(s);
    /* deliveries: free, or the rent small against what building would cost; at the end, anything that pays */
    for (int i = 0; i < nt; i++) {
        int m = T[i].pub ? RH_M_PUBLIC + T[i].slot : RH_M_DELIVER + T[i].slot;
        if (!rh_legal(s, m)) continue;
        int rent = rh_route(s, p, s->con[T[i].id].a, s->con[T[i].id].b, NULL, NULL);
        int ownw[RH_MAXC];
        plan_weights(s, p, ownw, -1);           /* without renting */
        target own = T[i];
        plan_target(s, p, ownw, &own);
        int own_pts = own.est >= RH_FAR ? 999 : own.need_pts;
        double rel = T[i].value - rent - (double)rent / (s->np - 1);   /* against the others: the owners gain */
        if (rent == 0 || (rel > 0 && (T[i].pub || tl <= 1 || own_pts > tl * s->cfg.pts || rent * 2 <= own_pts)))
            return m;
    }
    /* drawing: at a turn's start, with nothing worth building */
    double best = -1;
    int bi = -1;
    for (int i = 0; i < nt; i++) {
        if (focus >= 0 && i != focus) continue;
        double v = worth(s, p, &T[i]);
        if (v < 0) continue;
        if (noise > 0) v *= 0.4 + noise * pr_f(r);
        if (v > best) best = v, bi = i;
    }
    int live = 0;
    for (int i = 0; i < nt; i++)
        if (!T[i].pub && worth(s, p, &T[i]) >= 0) live++;
    if (!s->acted && rh_legal(s, RH_M_DRAW) && tl >= 3 && (live == 0 || (bi < 0 && focus < 0))) return RH_M_DRAW;
    /* building: the best target's route, then the others' */
    int order[RH_HAND + RH_PUB], no = 0;
    if (bi >= 0) order[no++] = bi;
    if (focus < 0) {
        double wv[RH_HAND + RH_PUB];
        for (int i = 0; i < nt; i++) wv[i] = worth(s, p, &T[i]);
        for (int k = 0; k < nt; k++) {
            int b2 = -1;
            for (int i = 0; i < nt; i++) {
                int used = 0;
                for (int j = 0; j < no; j++) used |= order[j] == i;
                if (!used && wv[i] >= 0 && (b2 < 0 || wv[i] > wv[b2])) b2 = i;
            }
            if (b2 < 0) break;
            order[no++] = b2;
        }
    }
    if (noise > 0 && s->pts >= 1 && pr_f(r) < 0.3) {   /* easy: now and then a hex next to its track, anywhere */
        int cand[RH_MAXC], n = 0;
        for (int c = 0; c < s->nc; c++)
            if (rh_can_build(s, p, c) && !s->hidden[c]) cand[n++] = c;
        if (n) return RH_M_BUILD + cand[pr_n(r, n)];
    }
    for (int k = 0; k < no; k++) {
        int c = route_step(s, p, &T[order[k]]);
        if (c >= 0) return RH_M_BUILD + c;
    }
    /* a point left over: survey the first face-down hex on the best route */
    if (noise <= 0 && s->pts >= 1)
        for (int k = 0; k < no; k++)
            for (int pass = 0; pass < 2; pass++)   /* the route's face-down hexes next to the track first */
                for (int i = 0; i < T[order[k]].npath; i++) {
                    int c = T[order[k]].path[i];
                    if (s->hidden[c] && (pass || rh_touches(s, p, c))) return RH_M_SURVEY + c;
                }
    if (noise > 0 && s->pts >= 1 && pr_n(r, 4) == 0) {   /* easy: now and then a hex anywhere next to its track */
        int cand[RH_MAXC], n = 0;
        for (int c = 0; c < s->nc; c++)
            if (rh_can_build(s, p, c)) cand[n++] = c;
        if (n) return RH_M_BUILD + cand[pr_n(r, n)];
    }
    if (!s->acted && rh_legal(s, RH_M_DRAW) && tl >= 2) return RH_M_DRAW;
    return RH_M_END;
}

int rh_greedy_move(const rh_state *s, double noise, uint64_t seed)
{
    prng r = {seed};
    int m = greedy(s, noise, -1, &r);
    if (m == AI_NONE || !rh_legal(s, m)) {      /* never stall */
        int mv[2 * RH_MAXC + 64];
        int n = rh_moves(s, mv);
        return n ? mv[n - 1] : AI_NONE;
    }
    return m;
}

/* ---- hidden things dealt again --------------------------------------------------------------------------------- */
void rh_determinise(rh_state *s, int seat, uint64_t seed)
{
    prng r = {seed ^ 0x51ed2701u};
    uint8_t pool[RH_MAXK];
    int n = 0;
    for (int q = 0; q < s->np; q++)
        if (q != seat)
            for (int i = 0; i < s->pl[q].nhand; i++) pool[n++] = s->pl[q].hand[i];
    for (int i = 0; i < s->ndeck; i++) pool[n++] = s->deck[i];
    int offers_hidden = s->phase == RH_KEEP && s->turn != seat;
    if (offers_hidden)
        for (int i = 0; i < s->noffer; i++) pool[n++] = s->offer[i];
    for (int i = n - 1; i > 0; i--) {
        int j = pr_n(&r, i + 1);
        uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    int k = 0;
    for (int q = 0; q < s->np; q++)
        if (q != seat)
            for (int i = 0; i < s->pl[q].nhand; i++) s->pl[q].hand[i] = pool[k++];
    for (int i = 0; i < s->ndeck; i++) s->deck[i] = pool[k++];
    if (offers_hidden)
        for (int i = 0; i < s->noffer; i++) s->offer[i] = pool[k++];
    for (int i = s->npdeck - 1; i > 0; i--) {
        int j = pr_n(&r, i + 1);
        uint8_t t = s->pdeck[i]; s->pdeck[i] = s->pdeck[j]; s->pdeck[j] = t;
    }
    /* face-down hexes: their lands (and hamlets) shuffled among them */
    int cells[RH_MAXC], nh = 0;
    for (int c = 0; c < s->nc; c++)
        if (s->hidden[c]) cells[nh++] = c;
    for (int i = nh - 1; i > 0; i--) {
        int j = pr_n(&r, i + 1), a = cells[i], b = cells[j];
        uint8_t t = s->terr[a]; s->terr[a] = s->terr[b]; s->terr[b] = t;
        t = s->hamlet[a]; s->hamlet[a] = s->hamlet[b]; s->hamlet[b] = t;
    }
    for (int c = 0; c < s->nc; c++)              /* a hamlet only on easy land */
        if (s->hamlet[c] && rh_cost_of[s->terr[c]] != 1) s->hamlet[c] = 0;
}

/* ---- the search ------------------------------------------------------------------------------------------------ */
/* a player's prospects: coins, and each contract in hand by what is left to build (one that cannot be done counts
 * its penalty) */
static double prospect(const rh_state *s, int q)
{
    if (s->phase == RH_OVER) return rh_final(s, q);
    double v = s->pl[q].coins;
    int w[RH_MAXC];
    plan_weights(s, q, w, RENT_W * s->cfg.rent);
    int tl = turns_left(s), promised = 0;
    for (int i = 0; i < s->pl[q].nhand; i++) {
        target t;
        t.id = s->pl[q].hand[i], t.pub = 0, t.value = s->con[t.id].value;
        plan_target(s, q, w, &t);
        int pen = t.value * s->cfg.pen_pct / 100;
        promised += t.need_pts;
        if (t.est >= RH_FAR || t.need_pieces > s->pl[q].track || promised > s->cfg.pts * (tl + 1)) v -= pen;
        else {
            double f = 1.0 - (double)t.need_pts / (s->cfg.pts * (tl + 1) + 1);
            v += (t.value - t.rent * s->cfg.rent) * f - pen * (1 - f);
        }
    }
    return v;
}

static double score_for(const rh_state *s, int p)
{
    double mine = prospect(s, p), best = -1e9, sum = 0;
    for (int q = 0; q < s->np; q++) {
        if (q == p) continue;
        double o = prospect(s, q);
        sum += o;
        if (o > best) best = o;
    }
    /* against the best other, and a little against the field */
    return mine - 0.7 * best - 0.3 * sum / (s->np - 1);
}

static void rollout(rh_state *s, int turns, prng *r)
{
    int guard = 0, start_turn = s->turn, passed = 0;
    if (turns <= 0) turns = 1 << 20;
    while (s->phase != RH_OVER && guard++ < 2000) {
        int before = s->turn;
        int m = greedy(s, 0, -1, r);
        if (m == AI_NONE || !rh_play(s, m, NULL)) {
            if (!rh_play(s, RH_M_END, NULL)) break;
        }
        if (s->turn != before && ++passed >= turns) break;
    }
    (void)start_turn;
}

/* a plan: the moves of the rest of this turn, made by the planner (focus: a target, -1 the best) or forced first */
typedef struct { int first, focus, kind; } plan;
enum { PL_GREEDY, PL_FOCUS, PL_FIRST };

static void play_plan(rh_state *s, const plan *pl, prng *r)
{
    int p = s->turn, guard = 0;
    if (pl->kind == PL_FIRST) {
        if (!rh_play(s, pl->first, NULL)) return;
    }
    while (s->phase == RH_PLAY && s->turn == p && guard++ < 40) {
        int m = greedy(s, 0, pl->kind == PL_FOCUS ? pl->focus : -1, r);
        if (pl->kind == PL_FOCUS && m == RH_M_DRAW) m = RH_M_END;
        if (m == AI_NONE || !rh_play(s, m, NULL)) { rh_play(s, RH_M_END, NULL); break; }
    }
    if (s->phase == RH_KEEP && s->turn == p) rh_play(s, greedy(s, 0, -1, r), NULL);
}

int rh_search_move(const rh_state *s0, int rollouts, int horizon, double ms_cap, uint64_t seed)
{
    int p = s0->turn;
    if (s0->phase == RH_OVER) return AI_NONE;
    plan P[48];
    int np = 0;
    if (s0->phase == RH_KEEP) {
        for (int k = 1; k < 1 << s0->noffer && np < 48; k++)
            if (rh_legal(s0, RH_M_KEEP + k)) P[np++] = (plan){RH_M_KEEP + k, -1, PL_FIRST};
    } else {
        P[np++] = (plan){0, -1, PL_GREEDY};
        target T[RH_HAND + RH_PUB];
        int nt = targets(s0, p, T);
        for (int i = 0; i < nt && np < 40; i++) {
            if (route_step(s0, p, &T[i]) >= 0) P[np++] = (plan){0, i, PL_FOCUS};
            int m = T[i].pub ? RH_M_PUBLIC + T[i].slot : RH_M_DELIVER + T[i].slot;
            if (rh_legal(s0, m)) P[np++] = (plan){m, -1, PL_FIRST};
            for (int j = 0; j < T[i].npath && np < 40; j++)    /* a survey of the route's first face-down hex */
                if (s0->hidden[T[i].path[j]] && s0->pts >= 1) {
                    P[np++] = (plan){RH_M_SURVEY + T[i].path[j], -1, PL_FIRST};
                    break;
                }
        }
        if (rh_legal(s0, RH_M_DRAW)) P[np++] = (plan){RH_M_DRAW, -1, PL_FIRST};
        if (s0->acted) P[np++] = (plan){RH_M_END, -1, PL_FIRST};
    }
    /* the first move of each plan (to drop doubles and to answer) */
    int first[48];
    for (int i = 0; i < np; i++) {
        if (P[i].kind == PL_FIRST) first[i] = P[i].first;
        else {
            prng r2 = {seed + 77};
            first[i] = greedy(s0, 0, P[i].kind == PL_FOCUS ? P[i].focus : -1, &r2);
            if (P[i].kind == PL_FOCUS && first[i] == RH_M_DRAW) first[i] = RH_M_END;
        }
    }
    if (np == 1) return first[0];
    double sum[48] = {0};
    rh_state *a = malloc(sizeof *a);
    if (!a) return first[0];
    double t0 = ai_now_ms();
    for (int k = 0; k < rollouts; k++) {
        if (k >= 2 && ms_cap > 0 && ai_now_ms() - t0 > ms_cap) break;   /* a slow device: the safety net */
        uint64_t ds = seed * 31 + (uint64_t)k * 0x9e3779b97f4a7c15ull;
        for (int i = 0; i < np; i++) {
            if (ai_cancelled()) { free(a); return first[0]; }
            *a = *s0;
            rh_determinise(a, p, ds);
            prng rr = {ds + 1};
            play_plan(a, &P[i], &rr);
            rollout(a, horizon, &rr);
            sum[i] += score_for(a, p);
        }
    }
    free(a);
    int bi = 0;
    for (int i = 1; i < np; i++)
        if (sum[i] > sum[bi]) bi = i;
    int m = first[bi];
    if (!rh_legal(s0, m)) return rh_greedy_move(s0, 0, seed);
    return m;
}

int rh_bot_move(const rh_state *s, int level, uint64_t seed)
{
    if (level <= 0) return rh_greedy_move(s, level < 0 ? 0 : 2.0, seed);
    if (level == 1) return rh_greedy_move(s, 0, seed);
    return rh_search_move(s, 48 / s->np, 0, 0.45 * ai_level_budget(AI_HARD).ms, seed);   /* rollouts to the end */
}

/* ---- the registered bot ------------------------------------------------------------------------------------------ */
#ifndef RH_NO_REGISTER
const rh_state *railhills_live(void);
int railhills_show(const char *move);

static int think(int seat, int level, char *move, int cap)
{
    rh_state *s = malloc(sizeof *s);
    if (!s) return 0;
    *s = *railhills_live();
    if (rh_actor(s) != seat) { free(s); return 0; }
    rh_determinise(s, seat, ai_session_seed() ^ 0xabcdefu);   /* never the true hidden things */
    int m = rh_bot_move(s, level, ai_mix(ai_session_seed(), (uint64_t)seat * 977 + s->round));
    free(s);
    if (m == AI_NONE || ai_cancelled()) return 0;
    char t[16];
    rh_text(m, t);
    snprintf(move, cap, "%s", t);
    return 1;
}

static const bot railhills_bot = {think, NULL, railhills_show, 0, NULL};
REGISTER_BOT(railhills, &railhills_bot);
#endif
