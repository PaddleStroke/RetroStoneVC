/*
 * Hill Railways: the rules' test (the maps for every count and variant, the contracts, building, blind building,
 * surveying, delivering with rent, drawing and keeping, the turns and the end, the scores, whole random games from
 * seeds, the notation, determinism). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/railhills.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static rh_state S;
static uint32_t rs = 12345;
static int rnd(int n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (int)(rs % (uint32_t)n); }

static void fresh(int np, int variant, uint32_t seed)
{
    rh_cfg c;
    rh_config(&c, np, variant);
    rh_new(&S, &c, np, seed);
}

/* the maps: towns linked, never face down nor next to a face-down hex, values the cheapest way */
static void test_maps(void)
{
    int ok = 1, hidden = 0, hamlets = 0;
    for (int k = 0; k < 60; k++) {
        int np = 2 + k % 3, variant = k / 3 % 2;
        fresh(np, variant, 100 + k);
        ok &= S.ntown == S.cfg.towns && S.ncon >= 12 && S.phase == RH_KEEP && S.noffer == 3 && S.turn == 0;
        for (int i = 0; i < S.ntown; i++) {
            int c = S.town[i];
            ok &= S.terr[c] == RH_TOWN && S.town_at[c] == i && !S.hidden[c];
            for (int d = 0; d < 6; d++)
                if (S.nb[c][d] >= 0) ok &= !S.hidden[S.nb[c][d]];
            for (int j = 0; j < i; j++) ok &= rh_hex_dist(&S, c, S.town[j]) >= S.cfg.town_dist;
        }
        for (int c = 0; c < S.nc; c++) {
            hidden += S.hidden[c];
            hamlets += S.hamlet[c];
            if (S.hamlet[c]) ok &= S.hidden[c] && rh_cost_of[S.terr[c]] == 1;
            if (variant == 1) ok &= !S.hidden[c];
            ok &= S.owner[c] == -1;
        }
        for (int i = 0; i < S.ncon; i++) {
            const rh_con *n = &S.con[i];
            ok &= n->a != n->b && rh_hex_dist(&S, S.town[n->a], S.town[n->b]) >= S.cfg.min_dist;
            ok &= n->value >= 1;
        }
        for (int p = 0; p < np; p++) ok &= S.pl[p].coins == p && S.pl[p].nhand == 0;
    }
    CHECK(ok);
    CHECK(hidden > 0 && hamlets > 0);
}

/* the first deal, a turn of building, the end of the turn by itself */
static void test_turn(void)
{
    fresh(2, 0, 7);
    CHECK(!rh_legal(&S, RH_M_KEEP + 1));        /* keep at least two of the first three */
    CHECK(rh_legal(&S, RH_M_KEEP + 3) && rh_legal(&S, RH_M_KEEP + 7));
    CHECK(rh_play(&S, RH_M_KEEP + 3, NULL));
    CHECK(S.turn == 1 && S.phase == RH_KEEP && S.pl[0].nhand == 2);
    CHECK(rh_play(&S, RH_M_KEEP + 7, NULL));
    CHECK(S.turn == 0 && S.phase == RH_PLAY && S.round == 1 && S.pts == 2 && S.pl[1].nhand == 3);
    /* a build next to a town costs its terrain; one track a hex */
    int c = -1;
    for (int k = 0; k < S.nc && c < 0; k++)
        if (rh_legal(&S, RH_M_BUILD + k) && !S.hidden[k] && rh_cost(&S, k) == 1) c = k;
    CHECK(c >= 0);
    rh_result res;
    CHECK(rh_play(&S, RH_M_BUILD + c, &res));
    CHECK(S.owner[c] == 0 && S.pts == 1 && S.acted && S.turn == 0);
    CHECK(!rh_legal(&S, RH_M_BUILD + c));
    CHECK(!rh_legal(&S, RH_M_DRAW));            /* drawing is a whole turn */
    /* a hill or a river with one point left: no */
    for (int k = 0; k < S.nc; k++)
        if (!S.hidden[k] && rh_cost(&S, k) == 2) CHECK(!rh_legal(&S, RH_M_BUILD + k));
    int c2 = -1;
    for (int k = 0; k < S.nc && c2 < 0; k++)
        if (rh_legal(&S, RH_M_BUILD + k) && !S.hidden[k] && rh_cost(&S, k) == 1) c2 = k;
    if (c2 >= 0) {
        CHECK(rh_play(&S, RH_M_BUILD + c2, NULL));
        CHECK(S.turn == 1 && S.pts == 2 && !S.acted);   /* no point left: the turn ended by itself */
    }
    /* mountains and lakes never take track */
    for (int k = 0; k < S.nc; k++)
        if ((S.terr[k] == RH_MOUNTAIN || S.terr[k] == RH_LAKE) && !S.hidden[k]) CHECK(!rh_legal(&S, RH_M_BUILD + k));
    /* drawing: three offered, keep at least one, the others under the deck, the turn over */
    int before = S.ndeck, hand = S.pl[1].nhand;
    CHECK(rh_play(&S, RH_M_DRAW, NULL));
    CHECK(S.phase == RH_KEEP && S.noffer == 3 && S.keep_min == 1 && S.ndeck == before - 3);
    CHECK(!rh_legal(&S, RH_M_KEEP + 0));
    CHECK(rh_play(&S, RH_M_KEEP + 2, NULL));
    CHECK(S.pl[1].nhand == hand + 1 && S.ndeck == before - 1 && S.turn == 0 && S.round == 2);
}

/* blind building and surveying */
static void test_blind(void)
{
    int seen_fail = 0, seen_ok = 0, seen_survey = 0;
    for (int g = 0; g < 200 && (!seen_fail || !seen_ok || !seen_survey); g++) {
        fresh(2, 0, 500 + g);
        rh_play(&S, RH_M_KEEP + 3, NULL);
        rh_play(&S, RH_M_KEEP + 3, NULL);
        for (int t = 0; t < 30 && S.phase == RH_PLAY; t++) {
            int blind = -1, safe = -1;
            for (int c = 0; c < S.nc; c++) {
                if (!rh_legal(&S, RH_M_BUILD + c)) continue;
                if (S.hidden[c] && blind < 0) blind = c;
                if (!S.hidden[c] && safe < 0) safe = c;
            }
            if (blind >= 0) {
                rh_state b = S;
                rh_result res;
                int k = rh_cost_of[S.terr[blind]], pts = S.pts, turn = S.turn;
                CHECK(rh_play(&S, RH_M_BUILD + blind, &res));
                CHECK(!S.hidden[blind]);
                if (!k || k > pts) {
                    CHECK(res.failed && S.owner[blind] == -1 && (S.turn != turn || S.pts == pts - 1));
                    seen_fail++;
                } else {
                    CHECK(!res.failed && S.owner[blind] == turn);
                    seen_ok++;
                }
                (void)b;
            } else if (safe >= 0) rh_play(&S, RH_M_BUILD + safe, NULL);
            else rh_play(&S, RH_M_END, NULL);
            /* a survey turns up the hex and its neighbours */
            for (int c = 0; c < S.nc && S.phase == RH_PLAY && !seen_survey; c++)
                if (rh_legal(&S, RH_M_SURVEY + c)) {
                    CHECK(rh_play(&S, RH_M_SURVEY + c, NULL));
                    CHECK(!S.hidden[c]);
                    for (int d = 0; d < 6; d++)
                        if (S.nb[c][d] >= 0) CHECK(!S.hidden[S.nb[c][d]]);
                    seen_survey++;
                }
        }
    }
    CHECK(seen_fail > 0 && seen_ok > 0 && seen_survey > 0);
}

/* a delivery: the value, the rent to each owner on the route, a point */
static void test_deliver(void)
{
    int delivered = 0, rented = 0;
    for (int g = 0; g < 400 && (delivered < 30 || rented < 5); g++) {
        fresh(2 + g % 3, 1, 900 + g);
        while (S.phase == RH_KEEP) rh_play(&S, RH_M_KEEP + 3, NULL);
        for (int t = 0; t < 400 && S.phase != RH_OVER; t++) {
            int mv[2 * RH_MAXC + 64], n = rh_moves(&S, mv), m = -1;
            for (int i = 0; i < n && m < 0; i++)
                if (mv[i] >= RH_M_DELIVER && mv[i] < RH_M_DRAW) m = mv[i];
            if (m >= 0) {
                int p = S.turn, id = m < RH_M_PUBLIC ? S.pl[p].hand[m - RH_M_DELIVER] : S.pub[m - RH_M_PUBLIC];
                int pub = m >= RH_M_PUBLIC, coins[RH_NP], path[RH_MAXC], np = 0, pts = S.pts;
                for (int q = 0; q < S.np; q++) coins[q] = S.pl[q].coins;
                int rent = rh_route(&S, p, S.con[id].a, S.con[id].b, path, &np);
                int owed[RH_NP] = {0};
                for (int i = 0; i < np; i++)
                    if (S.owner[path[i]] >= 0 && S.owner[path[i]] != p) owed[S.owner[path[i]]]++;
                int nh = S.pl[p].nhand;
                rh_result res;
                CHECK(rh_play(&S, m, &res));
                CHECK(res.rent == rent && res.value == S.con[id].value + (pub ? S.cfg.pub_bonus : 0));
                CHECK(S.pl[p].coins == coins[p] + res.value - rent);
                for (int q = 0; q < S.np; q++)
                    if (q != p) CHECK(S.pl[q].coins == coins[q] + owed[q] * S.cfg.rent);
                CHECK(path[0] == S.town[S.con[id].a] && path[np - 1] == S.town[S.con[id].b]);
                if (!pub) CHECK(S.pl[p].nhand == nh - 1);
                else CHECK(S.pub[m - RH_M_PUBLIC] == -1);
                CHECK(S.phase != RH_PLAY || S.turn != p || S.pts == pts - 1);
                delivered++;
                rented += rent > 0;
                continue;
            }
            m = mv[rnd(n)];
            if (m >= RH_M_SURVEY && m < RH_M_DELIVER && rnd(4)) m = mv[0];
            rh_play(&S, m, NULL);
        }
    }
    CHECK(delivered >= 30 && rented >= 5);
}

/* whole random games: legal moves only, the end after the rounds, the scores add up, the notation */
static void test_games(void)
{
    int ok = 1, ended = 0;
    for (int g = 0; g < 150; g++) {
        int np = 2 + g % 3;
        fresh(np, g % 2, 3000 + g);
        int guard = 0, coins0 = 0;
        for (int p = 0; p < np; p++) coins0 += S.pl[p].coins;
        int gained = 0;
        while (S.phase != RH_OVER && guard++ < 5000) {
            int mv[2 * RH_MAXC + 64], n = rh_moves(&S, mv);
            ok &= n > 0;
            if (!n) break;
            int m = mv[rnd(n)];
            if (rnd(3) && n > 1 && m == RH_M_END) m = mv[rnd(n - 1)];
            char t[16];
            rh_text(m, t);
            int back;
            ok &= rh_parse(t, &back) == (int)strlen(t) && back == m;
            rh_result res;
            ok &= rh_play(&S, m, &res);
            if (res.con >= 0) gained += res.value;
            if (res.hamlet) gained += S.cfg.hamlet_bonus;
            ok &= S.round <= S.cfg.max_rounds + 1;
            for (int c = 0; c < S.nc; c++) ok &= S.owner[c] < np && (S.owner[c] < 0 || !S.hidden[c]);
        }
        ok &= S.phase == RH_OVER;
        ended += S.phase == RH_OVER;
        int sum = 0;
        for (int p = 0; p < np; p++) sum += S.pl[p].coins;
        ok &= sum == coins0 + gained;           /* rent only moves coins between players */
        unsigned w = rh_winners(&S);
        ok &= w != 0;
        for (int p = 0; p < np; p++)
            for (int q = 0; q < np; q++)
                if (w >> p & 1) ok &= rh_final(&S, p) >= rh_final(&S, q);
    }
    CHECK(ok);
    CHECK(ended == 150);
}

/* the same seed and moves: the same game */
static void test_determinism(void)
{
    rh_state a, b;
    int moves[4000], n = 0;
    fresh(3, 0, 77);
    a = S;
    rs = 999;
    while (S.phase != RH_OVER && n < 4000) {
        int mv[2 * RH_MAXC + 64], k = rh_moves(&S, mv), m = mv[rnd(k)];
        rh_play(&S, m, NULL);
        moves[n++] = m;
    }
    b = S;
    S = a;
    fresh(3, 0, 77);
    CHECK(memcmp(&S, &a, sizeof S) == 0);
    for (int i = 0; i < n; i++) rh_play(&S, moves[i], NULL);
    CHECK(memcmp(&S, &b, sizeof S) == 0);
    /* the notation: bad text refused */
    int m;
    CHECK(rh_parse("b12", &m) == 3 && m == RH_M_BUILD + 12);
    CHECK(rh_parse("x", &m) == 1 && m == RH_M_DRAW);
    CHECK(!rh_parse("q4", &m) && !rh_parse("b", &m) && !rh_parse("e3", &m));
}

int main(void)
{
    test_maps();
    test_turn();
    test_blind();
    test_deliver();
    test_games();
    test_determinism();
    printf("railhills: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
