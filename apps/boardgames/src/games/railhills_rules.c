/*
 * Valley Rails: the rules (no SDL; the tests and the bot link them). See railhills.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "railhills.h"

const int rh_cost_of[RH_TERRAINS] = {0, 1, 1, 1, 2, 2, 3, 0, 0};

/* ---- a small random generator (the map's maker, the deal) ------------------------------------------------------ */
static uint32_t rnd(uint32_t *x)
{
    *x ^= *x << 13;
    *x ^= *x >> 17;
    *x ^= *x << 5;
    return *x;
}
static int rnd_n(uint32_t *x, int n) { return (int)(rnd(x) % (uint32_t)n); }
static float rnd_f(uint32_t *x) { return (rnd(x) & 0xffffff) / 16777216.0f; }

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9e3779b1u ^ b * 0x85ebca6bu ^ c * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

/* value noise: smooth, 0..1, on a lattice of `cell` units */
static float vnoise(uint32_t seed, float x, float y, float cell)
{
    x /= cell;
    y /= cell;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float v00 = (hash3(seed, ix + 100, iy + 100) & 0xffff) / 65535.0f;
    float v10 = (hash3(seed, ix + 101, iy + 100) & 0xffff) / 65535.0f;
    float v01 = (hash3(seed, ix + 100, iy + 101) & 0xffff) / 65535.0f;
    float v11 = (hash3(seed, ix + 101, iy + 101) & 0xffff) / 65535.0f;
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
}

/* ---- the grid ------------------------------------------------------------------------------------------------- */
static int nb_of(int w, int h, int c, int d)
{
    int col = c % w, row = c / w, odd = row & 1;
    static const int DC[2][6] = {{1, 0, -1, -1, -1, 0}, {1, 1, 0, -1, 0, 1}};
    static const int DR[6] = {0, 1, 1, 0, -1, -1};
    int nc = col + DC[odd][d], nr = row + DR[d];
    if (nc < 0 || nr < 0 || nc >= w || nr >= h) return -1;
    return nc + nr * w;
}

int rh_hex_dist(const rh_state *s, int a, int b)
{
    int ar = a / s->w, br = b / s->w;
    int ax = a % s->w - (ar - (ar & 1)) / 2, bx = b % s->w - (br - (br & 1)) / 2;
    int dx = ax - bx, dz = ar - br, dy = -dx - dz;
    dx = abs(dx), dy = abs(dy), dz = abs(dz);
    return dx > dy ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
}

int rh_cost(const rh_state *s, int c)
{
    if (c < 0 || c >= s->nc) return 0;
    return rh_cost_of[s->terr[c]];
}

/* ---- shortest paths (a binary heap; small maps) ----------------------------------------------------------------- */
typedef struct { int d, c; } hitem;
static void hpush(hitem *h, int *n, int d, int c)
{
    int i = (*n)++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h[p].d <= d) break;
        h[i] = h[p];
        i = p;
    }
    h[i].d = d, h[i].c = c;
}
static hitem hpop(hitem *h, int *n)
{
    hitem top = h[0], last = h[--*n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1;
        if (l >= *n) break;
        if (l + 1 < *n && h[l + 1].d < h[l].d) l++;
        if (h[l].d >= last.d) break;
        h[i] = h[l];
        i = l;
    }
    h[i] = last;
    return top;
}

/* from src: dist[] (INT32_MAX/2 unreached) with w[c] the cost of entering c (< 0: blocked); prev[] may be NULL */

static void paths(const rh_state *s, const int *w, int src, int *dist, int *prev)
{
    hitem heap[RH_MAXC * 6 + 8];
    int n = 0;
    for (int c = 0; c < s->nc; c++) dist[c] = RH_FAR;
    if (prev) for (int c = 0; c < s->nc; c++) prev[c] = -1;
    dist[src] = 0;
    hpush(heap, &n, 0, src);
    while (n) {
        hitem it = hpop(heap, &n);
        if (it.d > dist[it.c]) continue;
        for (int d = 0; d < 6; d++) {
            int m = s->nb[it.c][d];
            if (m < 0 || w[m] < 0) continue;
            int nd = it.d + w[m];
            if (nd < dist[m]) {
                dist[m] = nd;
                if (prev) prev[m] = it.c;
                hpush(heap, &n, nd, m);
            }
        }
    }
}

/* ---- the map's maker ---------------------------------------------------------------------------------------------- */
static int terrain_cost_w(const rh_state *s, int c)   /* contracts' values: the land as it is, no track */
{
    int t = s->terr[c];
    if (t == RH_SEA || t == RH_LAKE) return -1;
    return rh_cost_of[t];
}

static int make_map(rh_state *s, uint32_t seed)
{
    uint32_t x = seed * 2654435761u + 12345;
    if (!x) x = 1;
    int w = s->w, h = s->h;
    float cx = (w - 0.5f) / 2, cy = (h - 1) * 0.866f / 2;
    float rx = (w - 0.5f) / 2 + 0.3f, ry = (h - 1) * 0.866f / 2 + 0.45f;
    uint32_t ns = rnd(&x);
    float el[RH_MAXC], mo[RH_MAXC];
    for (int c = 0; c < s->nc; c++) {
        int col = c % w, row = c / w;
        float wx = col + 0.5f * (row & 1), wy = row * 0.866f;
        float dx = (wx - cx) / rx, dy = (wy - cy) / ry;
        float d = sqrtf(dx * dx + dy * dy);
        float edge = 1.02f + 0.36f * (vnoise(ns + 7, wx, wy, 2.2f) - 0.5f);
        s->terr[c] = d < edge ? RH_MEADOW : RH_SEA;
        el[c] = 0.65f * vnoise(ns, wx, wy, 3.2f) + 0.35f * vnoise(ns + 1, wx, wy, 1.6f);
        mo[c] = 0.7f * vnoise(ns + 2, wx, wy, 2.6f) + 0.3f * vnoise(ns + 3, wx, wy, 1.3f);
    }
    /* the largest piece of land only */
    {
        int comp[RH_MAXC], best = -1, bestn = 0, ncomp = 0;
        for (int c = 0; c < s->nc; c++) comp[c] = -1;
        for (int c = 0; c < s->nc; c++) {
            if (s->terr[c] == RH_SEA || comp[c] >= 0) continue;
            int st[RH_MAXC], sp = 0, n = 0;
            st[sp++] = c;
            comp[c] = ncomp;
            while (sp) {
                int a = st[--sp];
                n++;
                for (int d = 0; d < 6; d++) {
                    int b = nb_of(w, h, a, d);
                    if (b >= 0 && s->terr[b] != RH_SEA && comp[b] < 0) comp[b] = ncomp, st[sp++] = b;
                }
            }
            if (n > bestn) bestn = n, best = ncomp;
            ncomp++;
        }
        for (int c = 0; c < s->nc; c++)
            if (comp[c] != best) s->terr[c] = RH_SEA;
    }
    for (int c = 0; c < s->nc; c++)
        for (int d = 0; d < 6; d++) {
            int b = nb_of(w, h, c, d);
            s->nb[c][d] = (int16_t)(b >= 0 && s->terr[b] != RH_SEA && s->terr[c] != RH_SEA ? b : -1);
        }
    /* the land's kinds: the elevation's highest share hills and mountains, the moisture forests and fields */
    int land[RH_MAXC], nl = 0;
    for (int c = 0; c < s->nc; c++)
        if (s->terr[c] != RH_SEA) land[nl++] = c;
    if (nl < s->cfg.towns * 5) return 0;
    /* rank by elevation */
    int ord[RH_MAXC];
    memcpy(ord, land, nl * sizeof(int));
    for (int i = 1; i < nl; i++)
        for (int j = i; j > 0 && el[ord[j]] > el[ord[j - 1]]; j--) {
            int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t;
        }
    int nmount = nl * 6 / 100, nhill = nl * 20 / 100;
    for (int i = 0; i < nl; i++) {
        int c = ord[i];
        if (i < nmount) s->terr[c] = RH_MOUNTAIN;
        else if (i < nmount + nhill) s->terr[c] = RH_HILL;
        else s->terr[c] = mo[c] > 0.6f ? RH_FOREST : mo[c] < 0.38f ? RH_FIELD : RH_MEADOW;
    }
    /* lakes: two small ones at the lowest */
    for (int k = 0, i = nl - 1; k < 2 && i > nl / 2; i--) {
        int c = ord[i], ok = 1;
        for (int d = 0; d < 6; d++)
            if (s->nb[c][d] < 0 || s->terr[s->nb[c][d]] == RH_LAKE) ok = 0;   /* not on the shore, not touching */
        if (!ok || rnd_n(&x, 3) == 0) continue;
        s->terr[c] = RH_LAKE;
        k++;
        if (rnd_n(&x, 2)) {                     /* sometimes a second cell */
            int b = s->nb[c][rnd_n(&x, 6)];
            if (b >= 0 && s->terr[b] != RH_MOUNTAIN) {
                int ok2 = 1;
                for (int d = 0; d < 6; d++) if (s->nb[b][d] < 0) ok2 = 0;
                if (ok2) s->terr[b] = RH_LAKE;
            }
        }
    }
    /* a river from one shore to the other, across the island's long way (top to bottom), wandering */
    {
        int src = -1, dst = -1;
        int col0 = w / 4 + rnd_n(&x, w / 2), col1 = w / 4 + rnd_n(&x, w / 2);
        for (int r = 0; r < h && src < 0; r++)
            for (int dc = 0; dc < w && src < 0; dc++) {
                int c = (col0 + (dc & 1 ? -(dc + 1) / 2 : dc / 2) + w) % w + r * w;
                if (s->terr[c] != RH_SEA) src = c;
            }
        for (int r = h - 1; r >= 0 && dst < 0; r--)
            for (int dc = 0; dc < w && dst < 0; dc++) {
                int c = (col1 + (dc & 1 ? -(dc + 1) / 2 : dc / 2) + w) % w + r * w;
                if (s->terr[c] != RH_SEA) dst = c;
            }
        if (src < 0 || dst < 0) return 0;
        int c = src, guard = 0;
        while (guard++ < 200) {
            if (s->terr[c] != RH_LAKE) s->terr[c] = RH_RIVER;
            if (c == dst) break;
            int dnow = rh_hex_dist(s, c, dst), cand[6], nc = 0;
            for (int d = 0; d < 6; d++) {
                int b = s->nb[c][d];
                if (b < 0 || s->terr[b] == RH_RIVER) continue;
                int db = rh_hex_dist(s, b, dst);
                if (db < dnow) cand[nc++] = b, cand[nc++] = b;    /* closer: twice as likely */
                else if (db == dnow && rnd_n(&x, 3) == 0) cand[nc++] = b;
            }
            if (!nc) break;
            c = cand[rnd_n(&x, nc)];
        }
    }
    /* towns: spread out, on flat land */
    s->ntown = 0;
    for (int c = 0; c < s->nc; c++) s->town_at[c] = -1;
    for (int k = 0; k < s->cfg.towns; k++) {
        int best = -1;
        float bs = -1;
        for (int i = 0; i < nl; i++) {
            int c = land[i], t = s->terr[c];
            if (t != RH_MEADOW && t != RH_FIELD && t != RH_FOREST && t != RH_HILL) continue;
            int md = 99;
            for (int j = 0; j < s->ntown; j++) {
                int dd = rh_hex_dist(s, c, s->town[j]);
                if (dd < md) md = dd;
            }
            if (md < s->cfg.town_dist) continue;
            int shore = 0;
            for (int d = 0; d < 6; d++) if (s->nb[c][d] < 0) shore++;
            float sc = (md > 6 ? 6 : md) + 2.2f * rnd_f(&x) - 0.25f * shore + (t == RH_HILL ? -1.0f : 0);
            if (sc > bs) bs = sc, best = c;
        }
        if (best < 0) return 0;
        s->town_at[best] = (int8_t)s->ntown;
        s->town[s->ntown++] = (uint8_t)best;
        s->terr[best] = RH_TOWN;
    }
    /* every town reachable from the first */
    int wgt[RH_MAXC], dist[RH_MAXC];
    for (int c = 0; c < s->nc; c++) wgt[c] = terrain_cost_w(s, c);
    paths(s, wgt, s->town[0], dist, NULL);
    for (int i = 1; i < s->ntown; i++)
        if (dist[s->town[i]] >= RH_FAR) return 0;
    /* face down: the hexes not next to a town, fog_pct of them; hamlets under some of the easy ones */
    memset(s->hidden, 0, sizeof s->hidden);
    memset(s->hamlet, 0, sizeof s->hamlet);
    int hid[RH_MAXC], nh = 0;
    for (int i = 0; i < nl; i++) {
        int c = land[i];
        if (s->terr[c] == RH_TOWN) continue;
        int near = 0;
        for (int d = 0; d < 6; d++) {
            int b = s->nb[c][d];
            if (b >= 0 && s->terr[b] == RH_TOWN) near = 1;
        }
        if (near || rnd_n(&x, 100) >= s->cfg.fog_pct) continue;
        s->hidden[c] = 1;
        if (rh_cost_of[s->terr[c]] == 1) hid[nh++] = c;
    }
    for (int k = 0; k < s->cfg.hamlets && nh > 0; k++) {
        int i = rnd_n(&x, nh);
        s->hamlet[hid[i]] = 1;
        hid[i] = hid[--nh];
    }
    /* the contracts: town pairs far enough apart, valued by the land's cheapest way between them */
    s->ncon = 0;
    for (int i = 0; i < s->ntown; i++) {
        paths(s, wgt, s->town[i], dist, NULL);
        for (int j = i + 1; j < s->ntown; j++) {
            if (rh_hex_dist(s, s->town[i], s->town[j]) < s->cfg.min_dist || s->ncon >= RH_MAXK) continue;
            rh_con *k = &s->con[s->ncon++];
            k->a = (uint8_t)i, k->b = (uint8_t)j;
            int v = (dist[s->town[j]] * s->cfg.value_pct + 50) / 100 + s->cfg.value_add;
            k->value = (uint8_t)(v > 255 ? 255 : v);
        }
    }
    if (s->ncon < 12) return 0;
    /* the longest third make the public lines' pile (when there are public lines) */
    int ids[RH_MAXK];
    for (int i = 0; i < s->ncon; i++) ids[i] = i;
    for (int i = 1; i < s->ncon; i++)
        for (int j = i; j > 0 && s->con[ids[j]].value > s->con[ids[j - 1]].value; j--) {
            int t = ids[j]; ids[j] = ids[j - 1]; ids[j - 1] = t;
        }
    int npl = s->cfg.npub > 0 ? s->ncon / 4 : 0;
    s->npdeck = 0, s->ndeck = 0;
    for (int i = 0; i < s->ncon; i++)
        if (i < npl) s->pdeck[s->npdeck++] = (uint8_t)ids[i];
        else s->deck[s->ndeck++] = (uint8_t)ids[i];
    for (int i = s->npdeck - 1; i > 0; i--) {
        int j = rnd_n(&x, i + 1), t = s->pdeck[i];
        s->pdeck[i] = s->pdeck[j], s->pdeck[j] = (uint8_t)t;
    }
    for (int i = s->ndeck - 1; i > 0; i--) {
        int j = rnd_n(&x, i + 1), t = s->deck[i];
        s->deck[i] = s->deck[j], s->deck[j] = (uint8_t)t;
    }
    return 1;
}

void rh_config(rh_cfg *c, int players, int variant)
{
    memset(c, 0, sizeof *c);
    c->w = players <= 2 ? 11 : players == 3 ? 12 : 13;
    c->h = players <= 3 ? 13 : 14;
    c->towns = players <= 3 ? 12 : 14;
    c->pts = 2;
    c->track = 99;                              /* no limit: the rounds end the game */
    c->rent = 1;
    c->npub = 2;
    c->pub_bonus = 3;
    c->pub_refill = 0;
    c->deal = 3, c->deal_keep = 2;
    c->draw = 3, c->draw_keep = 1;
    c->pen_pct = 100;
    c->fog_pct = variant == 1 ? 0 : 50;         /* variant 1: the clear valley, nothing face down */
    c->hamlets = variant == 1 ? 0 : 4, c->hamlet_bonus = 2;
    c->blind = 1, c->reveal_adj = 0;
    c->max_rounds = players <= 2 ? 18 : players == 3 ? 16 : 14;
    c->deliver_cost = 1;
    c->seat_coins = 1;
    c->value_add = 0;
    c->value_pct = 100;
    c->min_dist = 4;
    c->town_dist = 3;
    c->start_coins = 0;
}

static void offer(rh_state *s, int n, int keep)
{
    s->noffer = 0;
    while (s->noffer < n && s->noffer < RH_OFFER && s->ndeck > 0) s->offer[s->noffer++] = s->deck[--s->ndeck];
    s->keep_min = keep < s->noffer ? keep : s->noffer;
    s->phase = RH_KEEP;
}

void rh_new(rh_state *s, const rh_cfg *c, int players, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->cfg = *c;
    s->np = players < 2 ? 2 : players > RH_NP ? RH_NP : players;
    s->w = c->w > RH_MAXW ? RH_MAXW : c->w;
    s->h = c->h > RH_MAXH ? RH_MAXH : c->h;
    s->nc = s->w * s->h;
    s->seed = seed;
    for (uint32_t k = 0;; k++)                  /* a map that works (towns linked, enough contracts) */
        if (make_map(s, seed + k * 7919u)) break;
    for (int c2 = 0; c2 < s->nc; c2++) s->owner[c2] = -1;
    for (int p = 0; p < s->np; p++) {
        s->pl[p].coins = (int16_t)(c->start_coins + p * c->seat_coins);
        s->pl[p].track = (uint8_t)c->track;
    }
    for (int j = 0; j < RH_PUB; j++) s->pub[j] = -1;
    for (int j = 0; j < c->npub && j < RH_PUB; j++)
        if (s->npdeck > 0) s->pub[j] = s->pdeck[--s->npdeck];
    s->last = -1;
    s->turn = 0;
    s->round = 0;                               /* round 0: the first deal, each in turn */
    offer(s, c->deal, c->deal_keep);
}

int rh_touches(const rh_state *s, int p, int c)
{
    for (int d = 0; d < 6; d++) {
        int b = s->nb[c][d];
        if (b >= 0 && (s->owner[b] == p || s->terr[b] == RH_TOWN)) return 1;
    }
    return 0;
}

int rh_can_build(const rh_state *s, int p, int c)
{
    if (s->phase != RH_PLAY || p != s->turn || c < 0 || c >= s->nc) return 0;
    if (s->hidden[c])                           /* face down: built blind (it turns up first) */
        return s->cfg.blind && s->owner[c] < 0 && s->pts >= 1 && s->pl[p].track > 0 && rh_touches(s, p, c);
    int k = rh_cost_of[s->terr[c]];
    return k > 0 && s->owner[c] < 0 && k <= s->pts && s->pl[p].track > 0 && rh_touches(s, p, c);
}

int rh_route(const rh_state *s, int p, int a, int b, int *path, int *npath)
{
    int w[RH_MAXC], dist[RH_MAXC], prev[RH_MAXC];
    for (int c = 0; c < s->nc; c++)
        w[c] = s->terr[c] == RH_TOWN || s->owner[c] == p ? 1 : s->owner[c] >= 0 ? 1001 : -1;
    int ca = s->town[a], cb = s->town[b];
    paths(s, w, ca, dist, prev);
    if (dist[cb] >= RH_FAR) return -1;
    if (path) {
        int n = 0, tmp[RH_MAXC];
        for (int c = cb; c >= 0; c = prev[c]) tmp[n++] = c;
        for (int i = 0; i < n; i++) path[i] = tmp[n - 1 - i];
        *npath = n;
    }
    return dist[cb] / 1000 * s->cfg.rent;
}

int rh_actor(const rh_state *s) { return s->phase == RH_OVER ? -1 : s->turn; }
int rh_over(const rh_state *s) { return s->phase == RH_OVER; }

static int popc(unsigned m) { return __builtin_popcount(m); }

int rh_legal(const rh_state *s, int m)
{
    int p = s->turn;
    const rh_player *P = &s->pl[p];
    if (s->phase == RH_KEEP) {
        if (m < RH_M_KEEP || m >= RH_M_KEEP + (1 << s->noffer)) return 0;
        unsigned k = (unsigned)(m - RH_M_KEEP);
        return popc(k) >= s->keep_min && P->nhand + popc(k) <= RH_HAND;
    }
    if (s->phase != RH_PLAY) return 0;
    if (m >= RH_M_BUILD && m < RH_M_BUILD + s->nc) return rh_can_build(s, p, m - RH_M_BUILD);
    if (m >= RH_M_SURVEY && m < RH_M_SURVEY + s->nc) return s->pts >= 1 && s->hidden[m - RH_M_SURVEY];
    if (m >= RH_M_DELIVER && m < RH_M_PUBLIC + RH_PUB && s->pts < s->cfg.deliver_cost) return 0;
    if (m >= RH_M_DELIVER && m < RH_M_DELIVER + P->nhand) {
        const rh_con *k = &s->con[P->hand[m - RH_M_DELIVER]];
        return rh_route(s, p, k->a, k->b, NULL, NULL) >= 0;
    }
    if (m >= RH_M_PUBLIC && m < RH_M_PUBLIC + RH_PUB) {
        int id = s->pub[m - RH_M_PUBLIC];
        return id >= 0 && rh_route(s, p, s->con[id].a, s->con[id].b, NULL, NULL) >= 0;
    }
    if (m == RH_M_DRAW) return !s->acted && s->ndeck > 0 && P->nhand + (s->cfg.draw_keep < s->ndeck ? s->cfg.draw_keep : s->ndeck) <= RH_HAND;
    if (m == RH_M_END) return 1;
    return 0;
}

int rh_moves(const rh_state *s, int *out)
{
    int n = 0, p = s->turn;
    if (s->phase == RH_KEEP) {
        for (int k = 1; k < 1 << s->noffer; k++)
            if (rh_legal(s, RH_M_KEEP + k)) out[n++] = RH_M_KEEP + k;
        return n;
    }
    if (s->phase != RH_PLAY) return 0;
    for (int i = 0; i < s->pl[p].nhand; i++)
        if (rh_legal(s, RH_M_DELIVER + i)) out[n++] = RH_M_DELIVER + i;
    for (int j = 0; j < RH_PUB; j++)
        if (rh_legal(s, RH_M_PUBLIC + j)) out[n++] = RH_M_PUBLIC + j;
    if (rh_legal(s, RH_M_DRAW)) out[n++] = RH_M_DRAW;
    for (int c = 0; c < s->nc; c++)
        if (rh_can_build(s, p, c)) out[n++] = RH_M_BUILD + c;
    if (s->pts >= 1)
        for (int c = 0; c < s->nc; c++)
            if (s->hidden[c]) out[n++] = RH_M_SURVEY + c;
    out[n++] = RH_M_END;
    return n;
}

static int anything_but_end(const rh_state *s)
{
    int p = s->turn;
    if (s->phase != RH_PLAY) return 1;
    if (s->pts >= 1)
        for (int c = 0; c < s->nc; c++)
            if (s->hidden[c] || rh_can_build(s, p, c)) return 1;
    if (rh_legal(s, RH_M_DRAW)) return 1;
    for (int i = 0; i < s->pl[p].nhand; i++)
        if (rh_legal(s, RH_M_DELIVER + i)) return 1;
    for (int j = 0; j < RH_PUB; j++)
        if (rh_legal(s, RH_M_PUBLIC + j)) return 1;
    return 0;
}

static void reveal(rh_state *s, int c, rh_result *res)
{
    if (c < 0 || !s->hidden[c]) return;
    s->hidden[c] = 0;
    if (res && res->nrev < 16) res->revealed[res->nrev++] = c;
}

static void end_turn(rh_state *s, rh_result *res)
{
    if (!s->acted) s->passes++;
    s->turn = (s->turn + 1) % s->np;
    int all_passed = 0;
    if (s->turn == 0) s->round++, all_passed = s->passes >= s->np, s->passes = 0;
    if (s->turn == 0 && (s->ending || s->round > s->cfg.max_rounds || all_passed || (s->cfg.deck_end && s->ndeck == 0))) {
        s->phase = RH_OVER;
        if (res) res->ended = 1;
        return;
    }
    s->pts = s->cfg.pts;
    s->acted = 0;
}

/* a turn with nothing left to do but end ends by itself (and the next too) */
static void settle(rh_state *s, rh_result *res)
{
    while (s->phase == RH_PLAY && !anything_but_end(s)) end_turn(s, res);
}

static void deliver(rh_state *s, int id, int bonus, rh_result *res)
{
    int p = s->turn, path[RH_MAXC], np = 0;
    const rh_con *k = &s->con[id];
    int rent = rh_route(s, p, k->a, k->b, path, &np);
    int paid[RH_NP] = {0};
    for (int i = 0; i < np; i++) {
        int o = s->owner[path[i]];
        if (o >= 0 && o != p) paid[o] += s->cfg.rent;
    }
    for (int q = 0; q < s->np; q++) s->pl[q].coins = (int16_t)(s->pl[q].coins + paid[q]);
    s->pl[p].coins = (int16_t)(s->pl[p].coins + k->value + bonus - rent);
    s->pl[p].done++;
    s->acted = 1;                               /* a delivery is no pass */
    s->pts -= s->cfg.deliver_cost;
    if (res) {
        res->con = id, res->value = k->value + bonus, res->rent = rent;
        memcpy(res->paid, paid, sizeof paid);
        memcpy(res->path, path, np * sizeof(int));
        res->npath = np;
    }
}

int rh_play(rh_state *s, int m, rh_result *res)
{
    rh_result dummy;
    if (!res) res = &dummy;
    memset(res, 0, sizeof *res);
    res->con = -1;
    if (!rh_legal(s, m)) return 0;
    int p = s->turn;
    rh_player *P = &s->pl[p];
    if (s->phase == RH_KEEP) {
        unsigned k = (unsigned)(m - RH_M_KEEP);
        uint8_t back[RH_OFFER];
        int nb = 0;
        for (int i = 0; i < s->noffer; i++)
            if (k >> i & 1) P->hand[P->nhand++] = s->offer[i];
            else back[nb++] = s->offer[i];
        if (nb && !s->cfg.discard) {            /* the others under the deck (or out of the game) */
            memmove(s->deck + nb, s->deck, s->ndeck);
            memcpy(s->deck, back, nb);
            s->ndeck += nb;
        }
        s->noffer = 0;
        s->phase = RH_PLAY;
        if (s->round == 0) {                    /* the first deal: the next seat chooses, then the first plays */
            if (s->turn + 1 < s->np) {
                s->turn++;
                offer(s, s->cfg.deal, s->cfg.deal_keep);
                return 1;
            }
            s->turn = 0, s->round = 1, s->pts = s->cfg.pts, s->acted = 0;
            settle(s, res);
            return 1;
        }
        s->acted = 1;
        end_turn(s, res);
        settle(s, res);
        return 1;
    }
    if (m >= RH_M_BUILD && m < RH_M_BUILD + s->nc) {
        int c = m - RH_M_BUILD;
        if (s->hidden[c]) {                     /* built blind: it turns up; too dear or unbuildable: a point lost */
            reveal(s, c, res);
            int k = rh_cost_of[s->terr[c]];
            if (!k || k > s->pts) {
                s->pts -= 1;
                s->acted = 1;
                res->failed = 1;
                settle(s, res);
                return 1;
            }
        }
        s->owner[c] = (int8_t)p;
        s->pts -= rh_cost_of[s->terr[c]];
        P->track--;
        s->acted = 1;
        s->last = c;
        if (s->hamlet[c]) {
            P->coins = (int16_t)(P->coins + s->cfg.hamlet_bonus);
            s->hamlet[c] = 0;
            res->hamlet = 1;
        }
        if (s->cfg.reveal_adj)
            for (int d = 0; d < 6; d++) reveal(s, s->nb[c][d], res);
        if (P->track == 0) s->ending = 1;
    } else if (m >= RH_M_SURVEY && m < RH_M_SURVEY + s->nc) {
        int c = m - RH_M_SURVEY;
        s->pts -= 1;
        s->acted = 1;
        reveal(s, c, res);
        for (int d = 0; d < 6; d++) reveal(s, s->nb[c][d], res);
    } else if (m >= RH_M_DELIVER && m < RH_M_DELIVER + RH_HAND) {
        int i = m - RH_M_DELIVER, id = P->hand[i];
        deliver(s, id, 0, res);
        memmove(P->hand + i, P->hand + i + 1, P->nhand - i - 1);
        P->nhand--;
    } else if (m >= RH_M_PUBLIC && m < RH_M_PUBLIC + RH_PUB) {
        int j = m - RH_M_PUBLIC, id = s->pub[j];
        deliver(s, id, s->cfg.pub_bonus, res);
        P->pubs++;
        s->pub[j] = (int16_t)(s->cfg.pub_refill && s->npdeck > 0 ? s->pdeck[--s->npdeck] : -1);
    } else if (m == RH_M_DRAW) {
        s->acted = 1;
        offer(s, s->cfg.draw, s->cfg.draw_keep);
        return 1;
    } else if (m == RH_M_END) {
        end_turn(s, res);
    }
    settle(s, res);
    return 1;
}

int rh_final(const rh_state *s, int p)
{
    int v = s->pl[p].coins;
    for (int i = 0; i < s->pl[p].nhand; i++) v -= s->con[s->pl[p].hand[i]].value * s->cfg.pen_pct / 100;
    return v;
}

unsigned rh_winners(const rh_state *s)
{
    int best = -100000, bd = -1;
    unsigned m = 0;
    for (int p = 0; p < s->np; p++) {
        int f = rh_final(s, p), d = s->pl[p].done;
        if (f > best || (f == best && d > bd)) best = f, bd = d, m = 1u << p;
        else if (f == best && d == bd) m |= 1u << p;
    }
    return m;
}

int rh_con_of(const rh_state *s, int a, int b)
{
    for (int i = 0; i < s->ncon; i++)
        if ((s->con[i].a == a && s->con[i].b == b) || (s->con[i].a == b && s->con[i].b == a)) return i;
    return -1;
}

int rh_parse(const char *t, int *m)
{
    const char *p = t;
    while (*p == ' ') p++;
    char k = *p++;
    int n = 0, digits = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0'), digits++;
    switch (k) {
    case 'b': if (!digits) return 0; *m = RH_M_BUILD + n; break;
    case 's': if (!digits) return 0; *m = RH_M_SURVEY + n; break;
    case 'd': if (!digits) return 0; *m = RH_M_DELIVER + n; break;
    case 'p': if (!digits) return 0; *m = RH_M_PUBLIC + n; break;
    case 'k': if (!digits) return 0; *m = RH_M_KEEP + n; break;
    case 'x': if (digits) return 0; *m = RH_M_DRAW; break;
    case 'e': if (digits) return 0; *m = RH_M_END; break;
    default: return 0;
    }
    return (int)(p - t);
}

void rh_text(int m, char *out)
{
    if (m >= RH_M_END) strcpy(out, "e");
    else if (m >= RH_M_KEEP) sprintf(out, "k%d", m - RH_M_KEEP);
    else if (m >= RH_M_DRAW) strcpy(out, "x");
    else if (m >= RH_M_PUBLIC) sprintf(out, "p%d", m - RH_M_PUBLIC);
    else if (m >= RH_M_DELIVER) sprintf(out, "d%d", m - RH_M_DELIVER);
    else if (m >= RH_M_SURVEY) sprintf(out, "s%d", m - RH_M_SURVEY);
    else sprintf(out, "b%d", m - RH_M_BUILD);
}

/* the shortest paths for the bot (same heap) */

void rh_paths(const rh_state *s, const int *w, int src, int *dist, int *prev) { paths(s, w, src, dist, prev); }
