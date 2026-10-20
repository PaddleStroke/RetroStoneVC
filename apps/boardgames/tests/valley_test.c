/*
 * Valley Harvest's rules test (src/games/valley_rules.c, no SDL): the valley (its size, the homes and their rings, the
 * river, the face-down middle and its ruins), a harvest's reach and yield (the workshops, the river, the rent), the
 * buildings' places and costs (roads over the river, a manor on a house, the workers, the pieces in hand), the
 * market's prices, the round's end (the workers home, the valley's use, the fewest points first), the notation, undo
 * and its floor, then random games for two, three and four players driven by a seed (every move legal, the game
 * ends, the same seed plays the same game, the moves replayed give the same state).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/valley.h"

static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t rng = 12345;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (unsigned)n);
}

static int count_in(const vl_state *s)
{
    int n = 0;
    for (int c = 0; c < VL_CELLS; c++) n += vl_in(s, c);
    return n;
}

/* a state with nothing face down: every land known (for the yields' checks) */
static void open_all(vl_state *s)
{
    for (int c = 0; c < VL_CELLS; c++) s->hidden[c] = 0;
}

/* a random game: its moves from vl_moves (a turn ended a third of the time once the worker is out) */
static int random_game(vl_game *g, int players, int variant, uint32_t seed, int *turns)
{
    vl_begin(g, players, variant, seed);
    int mv[VL_MAXMOVES], plies = 0, legal = 1;
    while (!g->s.over && plies < VL_HIST) {
        int n = vl_moves(&g->s, mv);
        if (!n) { legal = 0; break; }
        int m = g->s.phase == VL_ACT && rnd(3) == 0 ? VL_MV_END : mv[rnd(n)];
        for (int i = 0; i < n; i++) legal &= vl_legal(&g->s, mv[i]);
        if (!vl_play(g, m, NULL)) { legal = 0; break; }
        plies++;
    }
    *turns = g->s.turns;
    return legal && g->s.over;
}

int main(void)
{
    vl_state s;
    /* ---- the valley ----------------------------------------------------------------------------------------- */
    for (int players = 2; players <= 4; players++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            vl_new(&s, players, 0, seed);
            CHECK(count_in(&s) == (players == 2 ? 37 : 61));
            int homes = 0, river = 0, hidden = 0, ruins = 0, land[3] = {0};
            for (int c = 0; c < VL_CELLS; c++) {
                if (!vl_in(&s, c)) { CHECK(s.terr[c] == VL_OFF); continue; }
                if (s.bld[c] == VL_HOUSE) {
                    homes++;
                    CHECK(vl_dist(c) == s.radius);              /* a corner */
                    int seen[3] = {0};
                    for (int d = 0; d < 6; d++) {
                        int n = vl_nb[c][d];
                        if (n >= 0 && vl_in(&s, n)) {
                            CHECK(!s.hidden[n] && s.terr[n] < VL_RIVER);
                            seen[s.terr[n]]++;
                        }
                    }
                    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);   /* a forest, a field, a hill */
                }
                river += s.terr[c] == VL_RIVER;
                hidden += s.hidden[c];
                ruins += s.gift[c] == 1;
                if (s.terr[c] < VL_RIVER) land[s.terr[c]]++;
                if (s.gift[c]) CHECK(s.hidden[c]);
                if (s.terr[c] == VL_RIVER) CHECK(!s.hidden[c]);     /* the river is known from the start */
            }
            CHECK(homes == players);
            CHECK(river >= 3 && river <= s.radius + 2);
            CHECK(ruins == VL.gifts);
            CHECK(hidden >= count_in(&s) - 4 * players - river - 1);
            for (int t = 0; t < 3; t++) CHECK(land[t] >= count_in(&s) / 4);
            for (int p = 0; p < players; p++)
                CHECK(s.p[p].goods[0] == 1 && s.p[p].goods[1] == 1 && s.p[p].goods[2] == 1 && s.p[p].coins == 3 &&
                      s.p[p].workers == 2);
            vl_new(&s, players, 2, seed);                       /* the open valley: nothing face down */
            int any = 0;
            for (int c = 0; c < VL_CELLS; c++) any |= s.hidden[c];
            CHECK(!any);
        }
    vl_new(&s, 2, 1, 5);
    CHECK(s.rounds == 6);

    /* ---- a harvest ---------------------------------------------------------------------------------------- */
    vl_new(&s, 2, 0, 7);
    open_all(&s);
    int home = -1;
    for (int c = 0; c < VL_CELLS; c++)
        if (s.bld[c] == VL_HOUSE && s.own[c] == 0) home = c;
    CHECK(home >= 0);
    int forest = -1, far = -1;
    for (int d = 0; d < 6; d++) {
        int n = vl_nb[home][d];
        if (n >= 0 && vl_in(&s, n) && s.terr[n] == VL_FOREST) forest = n;
    }
    for (int c = 0; c < VL_CELLS; c++)
        if (vl_in(&s, c) && s.terr[c] < VL_RIVER && !vl_reach(&s, 0, c) && !vl_reach(&s, 1, c)) far = c;
    CHECK(forest >= 0 && far >= 0);
    CHECK(vl_harvestable(&s, forest, 0) && !vl_harvestable(&s, far, 0) && !vl_legal(&s, far));
    CHECK(!vl_legal(&s, VL_REST));                              /* land in reach: no rest */
    CHECK(vl_yield(&s, forest) == 1);
    {   /* a sawmill of player 1 beside it: one more, and a coin for its owner */
        vl_state t = s;
        int spot = -1;
        for (int d = 0; d < 6 && spot < 0; d++) {
            int n = vl_nb[forest][d];
            if (n >= 0 && vl_in(&t, n) && n != home && t.bld[n] == VL_NONE && t.terr[n] != VL_RIVER) spot = n;
        }
        CHECK(spot >= 0);
        t.bld[spot] = VL_SAW, t.own[spot] = 1;
        CHECK(vl_yield(&t, forest) == 2);
        t.terr[spot] = VL_FOREST;
        int wood = t.p[0].goods[0], coins1 = t.p[1].coins;
        vl_effect e;
        vl_apply(&t, forest, &e);
        CHECK(t.p[0].goods[0] == wood + 2 && t.p[1].coins == coins1 + VL.rent && e.nrented == 1 && e.amount == 2);
        CHECK(t.wk[forest] == 0 && t.phase == VL_ACT && t.p[0].placed == 1);
        CHECK(!vl_harvestable(&t, forest, 1));                  /* taken for the round */
    }
    {   /* a field by the river: one more */
        vl_state t = s;
        int field = -1, water = -1;
        for (int c = 0; c < VL_CELLS && field < 0; c++)
            if (vl_in(&t, c) && t.terr[c] == VL_FIELD && t.bld[c] == VL_NONE)
                for (int d = 0; d < 6; d++) {
                    int n = vl_nb[c][d];
                    if (n >= 0 && vl_in(&t, n) && t.terr[n] == VL_RIVER) { field = c; water = n; }
                }
        if (field >= 0) {
            CHECK(vl_yield(&t, field) == 1 + VL.river_bonus);
            t.terr[water] = VL_HILL;
            int wet = 0;
            for (int d = 0; d < 6; d++)
                wet |= vl_nb[field][d] >= 0 && t.terr[vl_nb[field][d]] == VL_RIVER;
            CHECK(vl_yield(&t, field) == 1 + (wet ? VL.river_bonus : 0));
        }
    }

    /* ---- building ----------------------------------------------------------------------------------------- */
    {
        vl_state t = s;
        vl_make(&t, forest);                                    /* harvest, then build */
        t.p[0].goods[0] = t.p[0].goods[1] = t.p[0].goods[2] = 20;
        int ring = -1, out = -1, water = -1;
        for (int d = 0; d < 6; d++) {
            int n = vl_nb[home][d];
            if (n >= 0 && vl_in(&t, n) && n != forest) ring = n;
        }
        CHECK(ring >= 0);
        CHECK(vl_legal(&t, VL_MV_BUILD(VL_HOUSE, ring)));
        CHECK(!vl_legal(&t, VL_MV_BUILD(VL_HOUSE, forest)));     /* a worker stands there */
        CHECK(!vl_legal(&t, VL_MV_BUILD(VL_HOUSE, far)));        /* not next to the village */
        CHECK(!vl_legal(&t, VL_MV_BUILD(VL_MANOR, ring)));       /* a manor: on a house */
        CHECK(vl_legal(&t, VL_MV_BUILD(VL_MANOR, home)));
        for (int c = 0; c < VL_CELLS; c++)
            if (vl_in(&t, c) && t.terr[c] == VL_RIVER) water = c;
        if (water >= 0) {                                       /* roads cross the river, buildings don't */
            vl_state r = t;
            r.bld[water] = VL_NONE;
            int nb = vl_nb[water][0] >= 0 ? vl_nb[water][0] : vl_nb[water][3];
            r.bld[nb] = VL_ROAD, r.own[nb] = 0, r.wk[nb] = -1;
            CHECK(vl_legal(&r, VL_MV_BUILD(VL_ROAD, water)));
            CHECK(!vl_legal(&r, VL_MV_BUILD(VL_HOUSE, water)));
        }
        int pp = t.p[0].pp;
        vl_make(&t, VL_MV_BUILD(VL_ROAD, ring));
        CHECK(t.bld[ring] == VL_ROAD && t.own[ring] == 0 && t.p[0].pp == pp && t.p[0].goods[0] == 19 && t.p[0].goods[2] == 19);
        CHECK(vl_harvestable(&t, ring, 1) == vl_reach(&t, 1, ring));   /* a road's land stays free */
        for (int d = 0; d < 6; d++) {                           /* the village reaches past the road */
            int n = vl_nb[ring][d];
            if (n >= 0 && vl_in(&t, n)) CHECK(vl_reach(&t, 0, n));
        }
        CHECK(vl_legal(&t, VL_MV_BUILD(VL_HOUSE, ring)));        /* a house on one's own road */
        vl_make(&t, VL_MV_BUILD(VL_HOUSE, ring));
        CHECK(t.p[0].workers == 3 && t.p[0].pp == pp + VL.pts[VL_HOUSE] && t.p[0].goods[1] == 18);
        vl_make(&t, VL_MV_BUILD(VL_MANOR, ring));
        CHECK(t.bld[ring] == VL_MANOR && t.p[0].pp == pp + VL.pts[VL_HOUSE] + VL.pts[VL_MANOR]);
        /* the houses in hand */
        int built = 1;
        for (int more = 1; more;) {
            more = 0;
            for (int c = 0; c < VL_CELLS && vl_left(&t, 0, VL_HOUSE) > 0; c++)
                if (vl_legal(&t, VL_MV_BUILD(VL_HOUSE, c))) { vl_make(&t, VL_MV_BUILD(VL_HOUSE, c)); built++; more = 1; }
        }
        CHECK(vl_left(&t, 0, VL_HOUSE) == 0 && built == VL.supply[VL_HOUSE] && t.p[0].workers == VL.workers_max);
        for (int c = 0; c < VL_CELLS; c++) CHECK(!vl_legal(&t, VL_MV_BUILD(VL_HOUSE, c)));
        (void)out;
    }

    /* ---- the market --------------------------------------------------------------------------------------- */
    {
        vl_state t = s;
        vl_make(&t, forest);
        CHECK(t.stock[VL_WOOD] == VL.stock0 && vl_buy_price(&t, VL_WOOD) == VL.price[VL.stock0]);
        int c0 = t.p[0].coins, price = vl_buy_price(&t, VL_WOOD);
        vl_make(&t, VL_MV_BUY(VL_WOOD));
        CHECK(t.p[0].coins == c0 - price && t.stock[VL_WOOD] == VL.stock0 - 1 && vl_buy_price(&t, VL_WOOD) >= price);
        CHECK(!vl_legal(&t, VL_MV_SELL(VL_WOOD)));              /* not back the same turn */
        int g0 = t.p[0].goods[VL_STONE], sp = vl_sell_price(&t, VL_STONE);
        CHECK(sp == VL.price[VL.stock0 + 1] - VL.spread || (sp == 1 && VL.price[VL.stock0 + 1] - VL.spread < 1));
        vl_make(&t, VL_MV_SELL(VL_STONE));
        CHECK(t.p[0].goods[VL_STONE] == g0 - 1 && t.stock[VL_STONE] == VL.stock0 + 1);
        CHECK(!vl_legal(&t, VL_MV_BUY(VL_STONE)));
        t.p[0].coins = 0;
        CHECK(!vl_legal(&t, VL_MV_BUY(VL_GRAIN)));
        t.stock[VL_GRAIN] = 0;
        t.p[0].coins = 50;
        CHECK(!vl_legal(&t, VL_MV_BUY(VL_GRAIN)) && vl_buy_price(&t, VL_GRAIN) == 0);
        t.stock[VL_GRAIN] = VL_STOCK_MAX;
        CHECK(!vl_legal(&t, VL_MV_SELL(VL_GRAIN)) && vl_sell_price(&t, VL_GRAIN) == 0);
    }

    /* ---- a round ------------------------------------------------------------------------------------------ */
    {
        vl_game g;
        vl_begin(&g, 3, 0, 99);
        int seen[3] = {0}, order[8], n = 0;
        while (g.s.round == 0) {
            int mv[VL_MAXMOVES];
            vl_moves(&g.s, mv);
            if (g.s.phase == VL_HARVEST) { order[n < 8 ? n : 7] = g.s.cur; n++; seen[g.s.cur]++; CHECK(vl_play(&g, mv[0], NULL)); }
            else CHECK(vl_play(&g, VL_MV_END, NULL));
        }
        CHECK(n == 6 && seen[0] == 2 && seen[1] == 2 && seen[2] == 2);
        CHECK(order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 0);
        for (int c = 0; c < VL_CELLS; c++) CHECK(g.s.wk[c] < 0);
        for (int gd = 0; gd < 3; gd++) CHECK(g.s.stock[gd] == VL.stock0 - VL.use);
        CHECK(g.s.phase == VL_HARVEST && g.s.cur == g.s.start);
        int low = 0;                                            /* the fewest points starts */
        for (int p = 1; p < 3; p++)
            if (vl_score(&g.s, p) < vl_score(&g.s, low)) low = p;
        CHECK(vl_score(&g.s, g.s.start) == vl_score(&g.s, low));
    }

    /* ---- the notation ----------------------------------------------------------------------------------------- */
    {
        int all[] = {0, 36, 60, VL_REST, VL_MV_END, VL_MV_BUY(0), VL_MV_BUY(2), VL_MV_SELL(1), VL_MV_BUILD(VL_ROAD, 5),
                     VL_MV_BUILD(VL_MANOR, 60), VL_MV_BUILD(VL_QUARRY, 0), VL_MV_BUILD(VL_MILL, 33)};
        for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) {
            char b[8];
            int len;
            vl_name(all[i], b);
            CHECK(vl_parse(b, &len) == all[i] && b[len] == 0);
        }
        int len;
        CHECK(vl_parse("h61", &len) < 0 && vl_parse("x3", &len) < 0 && vl_parse("+q", &len) < 0);
    }

    /* ---- undo and its floor ----------------------------------------------------------------------------------- */
    {
        vl_game g;
        vl_begin(&g, 2, 0, 3);
        vl_state first = g.s;
        int mv[VL_MAXMOVES];
        vl_moves(&g.s, mv);
        CHECK(vl_play(&g, mv[0], NULL));
        CHECK(vl_undo(&g) && !memcmp(&g.s, &first, sizeof first) && !vl_undo(&g));
        /* a road that turns land up can't be taken back */
        CHECK(vl_play(&g, mv[0], NULL));
        g.s.p[0].goods[0] = g.s.p[0].goods[2] = 5;
        g.s0.p[0].goods[0] = g.s0.p[0].goods[2] = 5;    /* (the undo replays from the start) */
        int road = -1;
        int n = vl_moves(&g.s, mv);
        for (int i = 0; i < n && road < 0; i++)
            if (mv[i] / 64 == VL_ROAD && mv[i] < VL_MV_BUY(0)) {
                vl_state t = g.s;
                vl_effect e;
                vl_apply(&t, mv[i], &e);
                if (e.nrev) road = mv[i];
            }
        CHECK(road >= 0);
        if (road >= 0) {
            CHECK(vl_play(&g, road, NULL));
            CHECK(!vl_undo(&g) && g.n == 2);
            CHECK(vl_play(&g, VL_MV_END, NULL) && vl_undo(&g) && g.n == 2);
        }
    }

    /* ---- random games ------------------------------------------------------------------------------------------ */
    static vl_game g, h;
    for (int players = 2; players <= 4; players++) {
        int ended = 0, games = 30, turns_min = 1 << 30, turns_max = 0;
        for (int k = 0; k < games; k++) {
            int turns;
            uint32_t seed = 1000u * (uint32_t)players + (uint32_t)k;
            uint32_t r0 = rng;
            ended += random_game(&g, players, k % 3, seed, &turns);
            if (turns < turns_min) turns_min = turns;
            if (turns > turns_max) turns_max = turns;
            CHECK(g.s.round == g.s.rounds);
            unsigned w = vl_winners(&g.s);
            CHECK(w != 0 && w < 1u << players);
            /* the same seed and the same choices: the same game; the moves replayed by script: the same state */
            uint32_t r1 = rng;
            rng = r0;
            random_game(&h, players, k % 3, seed, &turns);
            CHECK(!memcmp(&g.s, &h.s, sizeof g.s));
            rng = r1;
            static char text[VL_HIST * 6];
            size_t used = 0;
            for (int i = 0; i < g.n; i++) {
                char b[8];
                vl_name(g.mv[i], b);
                used += (size_t)snprintf(text + used, sizeof text - used, "%s ", b);
            }
            vl_begin(&h, players, k % 3, seed);
            CHECK(vl_script(&h, text) && !memcmp(&g.s, &h.s, sizeof g.s));
            /* every point counted: the buildings' points as built */
            for (int p = 0; p < players; p++) {
                int pts = 0;
                for (int c = 0; c < VL_CELLS; c++)
                    if (g.s.own[c] == p && g.s.bld[c] >= VL_HOUSE && !(g.s0.own[c] == p && g.s0.bld[c] == VL_HOUSE))
                        pts += VL.pts[g.s.bld[c]] + (g.s.bld[c] == VL_MANOR ? VL.pts[VL_HOUSE] : 0);
                    else if (g.s.own[c] == p && g.s.bld[c] == VL_MANOR)
                        pts += VL.pts[VL_MANOR];
                CHECK(pts == g.s.p[p].pp);
            }
        }
        CHECK(ended == games);
        printf("%d players: %d random games ended, %d..%d turns\n", players, ended, turns_min, turns_max);
    }

    printf("valley: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
