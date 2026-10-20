/*
 * Valley Harvest's rules (valley.h): the map, a turn's actions, the market, the rounds, the notation, undo. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/valley.h"

#ifndef VL_TUNE
const
#endif
vl_rules VL = {
    .rounds = {8, 6, 8},
    .workers0 = 2, .workers_max = 4,
    .start_goods = 1, .start_coins = 3, .seat_coins = 0,
    .cost = {
        [VL_ROAD] = {1, 0, 1},
        [VL_HOUSE] = {1, 2, 0},
        [VL_SAW] = {1, 0, 2},
        [VL_MILL] = {1, 0, 2},
        [VL_QUARRY] = {1, 0, 2},
        [VL_MANOR] = {2, 2, 2},
    },
    .pts = {[VL_HOUSE] = 1, [VL_SAW] = 2, [VL_MILL] = 2, [VL_QUARRY] = 2, [VL_MANOR] = 4},
    .supply = {[VL_HOUSE] = 5, [VL_SAW] = 3},
    .coins_per_pt = 4,
    .stock0 = 3,
    .price = {0, 4, 3, 3, 2, 2, 1},
    .spread = 1, .use = 1,
    .river_bonus = 1, .shop_bonus = 1, .rent = 1,
    .hidden = 1, .reveal_build = 1, .explore = 0,
    .gifts = 3, .gift_coins = 3,
    .catchup = 1,
    .manor_upgrade = 1,
};

int vl_nb[VL_CELLS][6];
int8_t vl_q[VL_CELLS], vl_r[VL_CELLS];
static int8_t cell_at[2 * VL_RMAX + 1][2 * VL_RMAX + 1];
static const int DQ[6] = {1, 0, -1, -1, 0, 1}, DR[6] = {0, 1, 1, 0, -1, -1};

__attribute__((constructor)) static void vl_init(void)
{
    int n = 0;
    for (int r = -VL_RMAX; r <= VL_RMAX; r++)
        for (int q = -VL_RMAX; q <= VL_RMAX; q++) {
            cell_at[q + VL_RMAX][r + VL_RMAX] = -1;
            if (q + r < -VL_RMAX || q + r > VL_RMAX) continue;
            vl_q[n] = (int8_t)q;
            vl_r[n] = (int8_t)r;
            cell_at[q + VL_RMAX][r + VL_RMAX] = (int8_t)n++;
        }
    for (int c = 0; c < VL_CELLS; c++)
        for (int d = 0; d < 6; d++) vl_nb[c][d] = vl_cell(vl_q[c] + DQ[d], vl_r[c] + DR[d]);
}

int vl_cell(int q, int r)
{
    if (q < -VL_RMAX || q > VL_RMAX || r < -VL_RMAX || r > VL_RMAX) return -1;
    return cell_at[q + VL_RMAX][r + VL_RMAX];
}

static int iabs(int x) { return x < 0 ? -x : x; }
int vl_dist(int c)
{
    int a = iabs(vl_q[c]), b = iabs(vl_r[c]), d = iabs(vl_q[c] + vl_r[c]);
    return a > b ? (a > d ? a : d) : (b > d ? b : d);
}
int vl_in(const vl_state *s, int c) { return c >= 0 && c < VL_CELLS && vl_dist(c) <= s->radius; }

static uint32_t rnd(uint32_t *x)
{
    *x ^= *x << 13;
    *x ^= *x >> 17;
    *x ^= *x << 5;
    return *x;
}

/* the homes: corners of the valley (south-east, north-west for two; every other one for three; the four nearest the
 * players' sides for four) */
static const int HOMES[5][4] = {{0}, {0}, {1, 4}, {1, 3, 5}, {1, 2, 4, 5}};
static int corner(int radius, int d) { return vl_cell(DQ[d] * radius, DR[d] * radius); }

void vl_new(vl_state *s, int players, int variant, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    if (players < 2) players = 2;
    if (players > VL_PLAYERS) players = VL_PLAYERS;
    if (variant < 0 || variant >= VL_VARIANTS) variant = 0;
    uint32_t x = seed * 2654435761u + 12345;
    if (!x) x = 1;
    for (int i = 0; i < 4; i++) rnd(&x);
    s->players = (uint8_t)players;
    s->radius = players <= 2 ? 3 : 4;
    s->rounds = (uint8_t)VL.rounds[variant];
    for (int c = 0; c < VL_CELLS; c++) {
        s->terr[c] = vl_in(s, c) ? VL_FOREST : VL_OFF;
        s->bld[c] = VL_NONE;
        s->own[c] = s->wk[c] = -1;
    }
    uint8_t fixed[VL_CELLS] = {0};              /* 1 a home's ring, 2 a home, 3 the river */
    for (int p = 0; p < players; p++) {         /* each home: a forest, a field and a hill beside it */
        int h = corner(s->radius, HOMES[players][p]);
        s->bld[h] = VL_HOUSE;
        s->own[h] = (int8_t)p;
        s->terr[h] = VL_FIELD;
        fixed[h] = 2;
        int ring[6], k = 0;
        for (int d = 0; d < 6; d++)
            if (vl_in(s, vl_nb[h][d])) ring[k++] = vl_nb[h][d];
        int t[3] = {VL_FOREST, VL_FIELD, VL_HILL};
        for (int i = 2; i > 0; i--) {
            int j = (int)(rnd(&x) % (unsigned)(i + 1)), tmp = t[i];
            t[i] = t[j];
            t[j] = tmp;
        }
        for (int i = 0; i < k; i++) {
            s->terr[ring[i]] = (uint8_t)t[i % 3];
            fixed[ring[i]] = 1;
        }
    }
    /* a river wandering from near the middle, never beside itself */
    int len = s->radius + 2, at = -1;
    for (int tries = 0; tries < 200 && at < 0; tries++) {
        int c = (int)(rnd(&x) % VL_CELLS);
        if (vl_in(s, c) && !fixed[c] && vl_dist(c) <= 1) at = c;
    }
    for (int i = 0; i < len && at >= 0; i++) {
        s->terr[at] = VL_RIVER;
        fixed[at] = 3;
        int opts[6], k = 0;
        for (int d = 0; d < 6; d++) {
            int c = vl_nb[at][d];
            if (!vl_in(s, c) || fixed[c]) continue;
            int wet = 0;
            for (int e = 0; e < 6; e++)
                if (vl_nb[c][e] >= 0 && fixed[vl_nb[c][e]] == 3) wet++;
            if (wet <= 1) opts[k++] = c;
        }
        at = k ? opts[rnd(&x) % (unsigned)k] : -1;
    }
    /* the rest: a third each of forests, fields and hills, shuffled */
    int free_c[VL_CELLS], nf = 0;
    for (int c = 0; c < VL_CELLS; c++)
        if (vl_in(s, c) && !fixed[c]) free_c[nf++] = c;
    for (int i = nf - 1; i > 0; i--) {
        int j = (int)(rnd(&x) % (unsigned)(i + 1)), tmp = free_c[i];
        free_c[i] = free_c[j];
        free_c[j] = tmp;
    }
    int t0 = (int)(rnd(&x) % 3);
    for (int i = 0; i < nf; i++) s->terr[free_c[i]] = (uint8_t)((i + t0) % 3);
    if (VL.hidden && variant != 2) {
        for (int i = 0; i < nf; i++) {          /* the bag's order is random: its first ones hold the ruins */
            s->hidden[free_c[i]] = 1;
            if (i < VL.gifts) s->gift[free_c[i]] = 1;
        }
    }
    for (int p = 0; p < players; p++) {
        for (int g = 0; g < VL_GOODS; g++) s->p[p].goods[g] = (int8_t)VL.start_goods;
        s->p[p].coins = (int16_t)(VL.start_coins + p * VL.seat_coins);
        s->p[p].workers = (uint8_t)VL.workers0;
    }
    for (int g = 0; g < VL_GOODS; g++) s->stock[g] = (uint8_t)VL.stock0;
    s->phase = VL_HARVEST;
}

int vl_reach(const vl_state *s, int p, int c)
{
    if (s->own[c] == p) return 1;
    for (int d = 0; d < 6; d++) {
        int n = vl_nb[c][d];
        if (n >= 0 && s->own[n] == p) return 1;
    }
    return 0;
}

int vl_harvestable(const vl_state *s, int c, int p)
{
    if (!vl_in(s, c) || s->wk[c] >= 0) return 0;
    if (s->hidden[c]) return VL.explore && vl_reach(s, p, c);
    return s->terr[c] < VL_RIVER && (s->bld[c] == VL_NONE || s->bld[c] == VL_ROAD) && vl_reach(s, p, c);
}

int vl_shop_of(int terr) { return terr == VL_FOREST ? VL_SAW : terr == VL_FIELD ? VL_MILL : terr == VL_HILL ? VL_QUARRY : VL_NONE; }

int vl_yield(const vl_state *s, int c)
{
    int t = s->terr[c], y = 1, wet = 0;
    if (t >= VL_RIVER || s->hidden[c]) return 0;
    for (int d = 0; d < 6; d++) {
        int n = vl_nb[c][d];
        if (n < 0 || !vl_in(s, n) || s->hidden[n]) continue;
        if (s->bld[n] == vl_shop_of(t)) y += VL.shop_bonus;
        if (s->terr[n] == VL_RIVER) wet = 1;
    }
    if (t == VL_FIELD && wet) y += VL.river_bonus;
    return y;
}

int vl_can_pay(const vl_state *s, int p, int k)
{
    for (int g = 0; g < VL_GOODS; g++)
        if (s->p[p].goods[g] < VL.cost[k][g]) return 0;
    return 1;
}

int vl_left(const vl_state *s, int p, int k)
{
    const vl_player *P = &s->p[p];
    if (k >= VL_SAW && k <= VL_QUARRY)
        return VL.supply[VL_SAW] ? VL.supply[VL_SAW] - P->built[VL_SAW] - P->built[VL_MILL] - P->built[VL_QUARRY] : -1;
    return VL.supply[k] ? VL.supply[k] - P->built[k] : -1;
}

int vl_can_build(const vl_state *s, int p, int k, int c)
{
    if (k <= VL_NONE || k >= VL_BLDS || !vl_in(s, c) || !vl_left(s, p, k)) return 0;
    if (k == VL_MANOR && VL.manor_upgrade) return s->bld[c] == VL_HOUSE && s->own[c] == p && s->wk[c] < 0;
    if (s->hidden[c] || s->wk[c] >= 0 || !vl_reach(s, p, c)) return 0;
    if (k == VL_ROAD) return s->bld[c] == VL_NONE;
    if (s->terr[c] == VL_RIVER) return 0;
    return s->bld[c] == VL_NONE || (s->bld[c] == VL_ROAD && s->own[c] == p);
}

int vl_buy_price(const vl_state *s, int g) { return s->stock[g] >= 1 ? VL.price[s->stock[g]] : 0; }

int vl_sell_price(const vl_state *s, int g)
{
    if (s->stock[g] >= VL_STOCK_MAX) return 0;
    int v = VL.price[s->stock[g] + 1] - VL.spread;
    return v > 1 ? v : 1;
}

int vl_moves(const vl_state *s, int *out)
{
    int n = 0, p = s->cur;
    if (s->over) return 0;
    if (s->phase == VL_HARVEST) {
        for (int c = 0; c < VL_CELLS; c++)
            if (vl_harvestable(s, c, p)) out[n++] = c;
        if (!n) out[n++] = VL_REST;
        return n;
    }
    for (int k = VL_ROAD; k < VL_BLDS; k++) {
        if (!vl_can_pay(s, p, k)) continue;
        for (int c = 0; c < VL_CELLS && n < VL_MAXMOVES - 8; c++)
            if (vl_can_build(s, p, k, c)) out[n++] = VL_MV_BUILD(k, c);
    }
    for (int g = 0; g < VL_GOODS; g++) {
        int bp = vl_buy_price(s, g);
        if (bp && s->p[p].coins >= bp && !(s->sold >> g & 1)) out[n++] = VL_MV_BUY(g);
        if (s->p[p].goods[g] > 0 && vl_sell_price(s, g) && !(s->bought >> g & 1)) out[n++] = VL_MV_SELL(g);
    }
    out[n++] = VL_MV_END;
    return n;
}

int vl_legal(const vl_state *s, int m)
{
    if (s->over || m < 0) return 0;
    int p = s->cur;
    if (s->phase == VL_HARVEST) {
        if (m == VL_REST) {
            for (int c = 0; c < VL_CELLS; c++)
                if (vl_harvestable(s, c, p)) return 0;
            return 1;
        }
        return m < VL_CELLS && vl_harvestable(s, m, p);
    }
    if (m == VL_MV_END) return 1;
    if (m >= VL_MV_SELL(0) && m < VL_MV_SELL(VL_GOODS)) {
        int g = m - VL_MV_SELL(0);
        return s->p[p].goods[g] > 0 && vl_sell_price(s, g) && !(s->bought >> g & 1);
    }
    if (m >= VL_MV_BUY(0) && m < VL_MV_BUY(VL_GOODS)) {
        int g = m - VL_MV_BUY(0), bp = vl_buy_price(s, g);
        return bp && s->p[p].coins >= bp && !(s->sold >> g & 1);
    }
    int k = m / 64, c = m % 64;
    return k > VL_NONE && k < VL_BLDS && c < VL_CELLS && vl_can_pay(s, p, k) && vl_can_build(s, p, k, c);
}

int vl_score(const vl_state *s, int p) { return s->p[p].pp + s->p[p].coins / VL.coins_per_pt; }

unsigned vl_winners(const vl_state *s)
{
    int best = -1;
    unsigned w = 0;
    for (int p = 0; p < s->players; p++) {
        int sc = vl_score(s, p) * 1000 + s->p[p].coins;
        if (sc > best) { best = sc; w = 1u << p; }
        else if (sc == best) w |= 1u << p;
    }
    return w;
}

static void reveal(vl_state *s, int c, int p, vl_effect *e)
{
    s->hidden[c] = 0;
    if (e && e->nrev < 6) e->revealed[e->nrev++] = c;
    if (s->gift[c] == 1) {
        s->gift[c] = 2;
        s->p[p].coins += VL.gift_coins;
        if (e) e->gift += VL.gift_coins;
    }
}

static void next_turn(vl_state *s, vl_effect *e)
{
    s->turns++;
    s->bought = s->sold = 0;
    s->phase = VL_HARVEST;
    for (int i = 1; i <= s->players; i++) {
        int q = (s->cur + i) % s->players;
        if (s->p[q].placed < s->p[q].workers) { s->cur = (uint8_t)q; return; }
    }
    /* the round's end: the workers home, the valley uses its goods, the one behind starts */
    if (e) e->round_end = 1;
    for (int c = 0; c < VL_CELLS; c++) s->wk[c] = -1;
    for (int g = 0; g < VL_GOODS; g++) s->stock[g] = (uint8_t)(s->stock[g] > VL.use ? s->stock[g] - VL.use : 0);
    for (int q = 0; q < s->players; q++) s->p[q].placed = 0;
    s->round++;
    if (s->round >= s->rounds) {
        s->over = 1;
        if (e) e->game_end = 1;
        return;
    }
    int st = (s->start + 1) % s->players;
    if (VL.catchup) {                           /* the fewest points (ties: the next in turn from the last start) */
        int best = st;
        for (int i = 0; i < s->players; i++) {
            int q = (s->start + 1 + i) % s->players;
            if (vl_score(s, q) < vl_score(s, best)) best = q;
        }
        st = best;
    }
    s->start = s->cur = (uint8_t)st;
}

void vl_apply(vl_state *s, int m, vl_effect *e)
{
    int p = s->cur;
    vl_player *P = &s->p[p];
    if (e) {
        memset(e, 0, sizeof *e);
        e->player = p;
        e->cell = e->good = -1;
    }
    if (s->phase == VL_HARVEST) {
        if (m == VL_REST) {
            P->coins++;
            if (e) e->kind = 1, e->amount = 1;
        } else {
            if (s->hidden[m]) reveal(s, m, p, e);
            s->wk[m] = (int8_t)p;
            int t = s->terr[m];
            if (e) e->kind = 0, e->cell = m;
            if (t < VL_RIVER) {
                int y = vl_yield(s, m);
                P->goods[t] = (int8_t)(P->goods[t] + y > 99 ? 99 : P->goods[t] + y);
                if (e) e->good = t, e->amount = y;
                for (int d = 0; d < 6; d++) {   /* the workshops' owners take their rent */
                    int n = vl_nb[m][d];
                    if (n >= 0 && s->bld[n] == vl_shop_of(t) && s->own[n] != p && s->own[n] >= 0) {
                        s->p[s->own[n]].coins += VL.rent;
                        if (e) { e->rent[s->own[n]] += VL.rent; e->rented[e->nrented++] = n; }
                    }
                }
            } else P->coins++;                  /* explored water: a coin */
        }
        P->placed++;
        s->phase = VL_ACT;
        return;
    }
    if (m == VL_MV_END) {
        if (e) e->kind = 5;
        next_turn(s, e);
        return;
    }
    if (m >= VL_MV_SELL(0)) {
        int g = m - VL_MV_SELL(0), v = vl_sell_price(s, g);
        s->stock[g]++;
        P->coins += v;
        P->goods[g]--;
        s->sold |= 1 << g;
        if (e) e->kind = 4, e->good = g, e->amount = v;
        return;
    }
    if (m >= VL_MV_BUY(0)) {
        int g = m - VL_MV_BUY(0), v = vl_buy_price(s, g);
        P->coins -= v;
        s->stock[g]--;
        P->goods[g]++;
        s->bought |= 1 << g;
        if (e) e->kind = 3, e->good = g, e->amount = v;
        return;
    }
    int k = m / 64, c = m % 64;
    for (int g = 0; g < VL_GOODS; g++) P->goods[g] -= VL.cost[k][g];
    s->bld[c] = (int8_t)k;
    s->own[c] = (int8_t)p;
    P->built[k]++;
    P->pp += VL.pts[k];
    if (k == VL_HOUSE && P->workers < VL.workers_max) P->workers++;
    if (e) e->kind = 2, e->cell = c;
    if (VL.reveal_build)
        for (int d = 0; d < 6; d++) {
            int n = vl_nb[c][d];
            if (n >= 0 && vl_in(s, n) && s->hidden[n]) reveal(s, n, p, e);
        }
}

void vl_make(vl_state *s, int m) { vl_apply(s, m, NULL); }

/* ---- notation ---------------------------------------------------------------------------------------------------- */
static const char BLD_CH[VL_BLDS] = {0, 'R', 'H', 'S', 'M', 'Q', 'V'};
static const char GOOD_CH[VL_GOODS] = {'w', 'g', 's'};

int vl_parse(const char *s, int *len)
{
    const char *p = s;
    int m = -1;
    if (*p == 'r') { m = VL_REST; p++; }
    else if (*p == 'e') { m = VL_MV_END; p++; }
    else if (*p == '+' || *p == '-') {
        for (int g = 0; g < VL_GOODS; g++)
            if (p[1] == GOOD_CH[g]) m = *p == '+' ? VL_MV_BUY(g) : VL_MV_SELL(g);
        if (m >= 0) p += 2;
    } else {
        int k = -1;
        if (*p == 'h') k = 0;
        for (int b = 1; b < VL_BLDS; b++)
            if (*p == BLD_CH[b]) k = b;
        if (k >= 0 && p[1] >= '0' && p[1] <= '9') {
            int c = 0;
            p++;
            while (*p >= '0' && *p <= '9' && c < 100) c = c * 10 + (*p++ - '0');
            if (c < VL_CELLS) m = k ? VL_MV_BUILD(k, c) : c;
        }
    }
    if (len) *len = m >= 0 ? (int)(p - s) : 0;
    return m;
}

void vl_name(int m, char *out)
{
    if (m == VL_REST) strcpy(out, "r");
    else if (m == VL_MV_END) strcpy(out, "e");
    else if (m >= VL_MV_SELL(0) && m < VL_MV_SELL(VL_GOODS)) out[0] = '-', out[1] = GOOD_CH[m - VL_MV_SELL(0)], out[2] = 0;
    else if (m >= VL_MV_BUY(0) && m < VL_MV_BUY(VL_GOODS)) out[0] = '+', out[1] = GOOD_CH[m - VL_MV_BUY(0)], out[2] = 0;
    else if (m >= 0 && m < VL_CELLS) snprintf(out, 6, "h%d", m);
    else if (m / 64 > 0 && m / 64 < VL_BLDS) snprintf(out, 6, "%c%d", BLD_CH[m / 64], m % 64);
    else strcpy(out, "?");
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void vl_begin(vl_game *g, int players, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    vl_new(&g->s0, players, variant, seed);
    g->s = g->s0;
}

int vl_play(vl_game *g, int m, vl_effect *e)
{
    if (g->n >= VL_HIST || !vl_legal(&g->s, m)) return 0;
    vl_effect local;
    if (!e) e = &local;
    vl_apply(&g->s, m, e);
    g->mv[g->n++] = (int16_t)m;
    if (e->nrev) g->floor = g->n;               /* what was turned up stays up */
    return 1;
}

int vl_undo(vl_game *g)
{
    if (g->n <= g->floor) return 0;
    g->n--;
    g->s = g->s0;
    for (int i = 0; i < g->n; i++) vl_make(&g->s, g->mv[i]);
    return 1;
}

int vl_script(vl_game *g, const char *moves)
{
    const char *p = moves;
    for (;;) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t') p++;
        if (!*p) return 1;
        int len, m = vl_parse(p, &len);
        if (m < 0 || !vl_play(g, m, NULL)) return 0;
        p += len;
    }
}
