/*
 * Train Switches' rules: a switch sending a train one way or the other, a train through a switch from a branch, a
 * toggle flipping behind each train, a crossing, a tunnel, trains together crashing on a crossing and head on, a
 * wrong station, a dead end; maps as text both ways; the chapters' pictures; every level read back, its par the
 * search's, unsolved as given, and solved with par flips; the puzzles made at random (their par in the variant's
 * range most of the time, the time they take); whole games from a seed (random flips and runs, hints to the end,
 * undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/trains.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int run(const char *map, unsigned set)   /* a network's run from a setting: TRS_*, -1 not a map */
{
    trs_map m;
    if (!trs_parse(&m, map)) return -1;
    return trs_simulate(&m, (uint16_t)set, NULL);
}

int main(void)
{
    trs_game g;
    trs_run r;
    char text[TRS_TEXT];
    static const char *Y = ".0.e4..0./.0.s70tc./.0.p11p10|S|10|0";

    /* a switch: stem up, its branches right (0) and down (1) */
    CHECK(trs_parse(&g.m, Y) && g.m.ns == 1 && g.m.ne == 1 && g.m.nt == 1 && g.m.w == 3 && g.m.h == 3);
    CHECK(trs_branch(&g.m, 4, 0) == TRS_RIGHT && trs_branch(&g.m, 4, 1) == TRS_DOWN);
    CHECK(trs_simulate(&g.m, 0, &r) == TRS_WRONG && r.bad == 8);
    CHECK(trs_simulate(&g.m, 1, &r) == TRS_OK && r.len[0] == 3 && r.cell[0][2] == 7 && r.ticks == 2);
    trs_begin(&g);
    CHECK(trs_hint(&g) == 0);
    CHECK(trs_go(&g, NULL) == TRS_WRONG && g.fails == 1 && !g.solved);
    CHECK(trs_script(&g, "b2") && g.moves == 1 && g.set == 1 && trs_hint(&g) < 0);
    CHECK(trs_undo(&g) && g.moves == 0 && g.set == 0);
    CHECK(trs_script(&g, "b2 g") && g.solved && g.runs == 2);
    CHECK(!trs_flip(&g, 0));                    /* solved: nothing more */
    CHECK(trs_undo(&g) && !g.solved && g.moves == 1);
    CHECK(!trs_script(&g, "a1"));               /* no switch there */
    CHECK(trs_solve(&g.m, 0, NULL, NULL) == 1);
    /* through a switch from a branch: out by the stem, whatever its setting (into the portal: a dead end) */
    CHECK(run(".0.e4..0./.0.s70tc./.0.p11e1.|S|11|0", 0) == TRS_DEAD);
    CHECK(run(".0.e4..0./.0.s70tc./.0.p11e1.|S|11|0", 1) == TRS_DEAD);
    CHECK(run(".0.e4..0./.0.s72tc./.0.p11e1.|S|11|0", 0) == TRS_OK);
    CHECK(run(".0.e4..0./.0.s72tc./.0.p11e1.|S|11|0", 1) == TRS_OK);
    /* a toggle: flipped after each train */
    CHECK(run(".0.e4..0./.0.S70tc./.0.p11p10|S|00,10|0", 0) == TRS_OK);
    CHECK(run(".0.e4..0./.0.S70tc./.0.p11p10|S|00,10|0", 1) == TRS_WRONG);
    CHECK(run(".0.e4..0./.0.s70tc./.0.p11p10|S|00,10|0", 0) == TRS_WRONG);   /* a plain switch: both alike */
    CHECK(trs_parse(&g.m, ".0.e4..0./.0.S70tc./.0.p11p10|S|00,10|0"));
    CHECK(trs_simulate(&g.m, 0, &r) == TRS_OK && r.nflip == 2 && r.start[1] == 4 && r.flip_t[0] == 1);
    /* a crossing: one by one, then together (a crash on it) */
    CHECK(run(".0.e4..0./e2.xf.p81/.0.p10.0.|S|00,11|", 0) == TRS_OK);
    CHECK(trs_parse(&g.m, ".0.e4..0./e2.xf.p81/.0.p10.0.|T|00,11|") && g.m.together);
    CHECK(trs_simulate(&g.m, 0, &r) == TRS_CRASH && r.bad == 4 && r.ticks == 1);
    CHECK(run(".0.e4..0./e2.xf.p81/.0.p10.0.|T|00,10|", 0) == -1);   /* together: a train a portal */
    CHECK(run("e4..0.e4./t5..0.t5./p11.0.p10|T|10,01|", 0) == TRS_OK);
    /* head on: two trains passing each other between two squares */
    CHECK(run("e2.t5.ta.t8.e8.|T|00,01|", 0) == -1);   /* not a map: sides do not meet */
    CHECK(trs_parse(&g.m, "e2.ta.ta.e8.|T|00,01|"));
    CHECK(trs_simulate(&g.m, 0, &r) == TRS_CRASH && r.ticks == 2);
    /* a tunnel: in at one mouth, out at the other */
    CHECK(trs_parse(&g.m, "e2.u80u20p80|S|00|"));
    CHECK(trs_simulate(&g.m, 0, &r) == TRS_OK && r.len[0] == 4 && r.cell[0][2] == 2 && r.out[0][1] == 5);
    CHECK(run("e2.u80u21p80|S|00|", 0) == -1);  /* a mouth without its twin */
    CHECK(trs_parse(&g.m, "e2.u80u20p80|S|00|"));
    trs_text(&g.m, text);
    CHECK(!strcmp(text, "e2.u80u20p80|S|00|"));
    CHECK(trs_parse(&g.m, Y));
    trs_text(&g.m, text);
    CHECK(!strcmp(text, Y));
    CHECK(!trs_parse(&g.m, ".0.e4..0./.0.s70tc./.0.p11p10|S|10|"));   /* a setting for each switch */
    CHECK(!trs_parse(&g.m, ".0.e4..0./.0.s70tc./.0.p11|S|10|0"));     /* rows alike */
    for (int i = 0; i < TRS_CHAPTERS; i++) CHECK(trs_parse(&g.m, trs_icons[i]));

    /* every level */
    int per = TRS_LEVELS / TRS_CHAPTERS, seen[TRS_CHAPTERS][4] = {{0}};
    for (int lv = 0; lv < TRS_LEVELS; lv++) {
        CHECK(trs_level(&g, lv));
        trs_text(&g.m, text);
        CHECK(!strcmp(text, trs_levels[lv]));
        int count = 0;
        CHECK(g.par > 0 && g.par == trs_level_par[lv] && trs_solve(&g.m, g.set, NULL, &count) == g.par && count > 0);
        CHECK(trs_simulate(&g.m, g.set, NULL) != TRS_OK);
        char moves[8];
        snprintf(moves, sizeof moves, "=%d", g.par);
        CHECK(trs_script(&g, moves) && g.moves == g.par && trs_hint(&g) < 0);
        CHECK(trs_go(&g, NULL) == TRS_OK && g.solved);
        int ch = lv / per, tog = 0, cross = 0, tun = 0;
        for (int c = 0; c < g.m.w * g.m.h; c++) {
            tog |= g.m.kind[c] == TRS_SWITCH && g.m.toggle[c];
            cross |= g.m.kind[c] == TRS_CROSS;
            tun |= g.m.kind[c] == TRS_TUNNEL;
        }
        seen[ch][0] |= tog;
        seen[ch][1] |= cross;
        seen[ch][2] |= g.m.together;
        seen[ch][3] |= tun;
        if (lv < 2) CHECK(g.m.ns <= 2 && g.m.nt == 1 && g.par == 1);   /* a gentle start */
    }
    CHECK(seen[2][0] && seen[3][1] && seen[4][2] && seen[5][3]);   /* each chapter's new piece is there */

    /* puzzles at random */
    for (int v = 0; v < TRS_VARIANTS; v++) {
        trs_spec sp;
        trs_variant(v, &sp);
        int in = 0, N = 40;
        clock_t t0 = clock();
        double worst = 0;
        for (int s = 1; s <= N; s++) {
            clock_t a = clock();
            trs_random(&g, v, (uint32_t)s * 7919u);
            double dt = (double)(clock() - a) / CLOCKS_PER_SEC;
            if (dt > worst) worst = dt;
            CHECK(g.par > 0);
            in += g.par >= sp.lo && g.par <= sp.hi;
            CHECK(trs_simulate(&g.m, g.set, NULL) != TRS_OK);
            CHECK(trs_script(&g, "=") && g.solved && g.moves == g.par);
        }
        printf("variant %d: par in range %d/%d, %.1f ms each, worst %.0f ms\n", v, in, N,
               1000.0 * (clock() - t0) / CLOCKS_PER_SEC / N, worst * 1000);
        CHECK(in * 10 >= N * 8);
        CHECK(worst < 0.5);
    }

    /* whole games from a seed: random flips and runs, then the hints to the end, then undo back to the start */
    for (int seed = 1; seed <= 60; seed++) {
        uint32_t rs = pr_seed((uint32_t)seed);
        CHECK(trs_level(&g, pr_below(&rs, TRS_LEVELS)));
        uint16_t first = g.set;
        for (int k = 0; k < 12; k++) {
            if (pr_below(&rs, 4) == 0) { if (trs_go(&g, NULL) == TRS_OK) trs_undo(&g); }
            else CHECK(trs_flip(&g, pr_below(&rs, g.m.ns)));
        }
        for (int k = 0; k < 20 && trs_hint(&g) >= 0; k++) CHECK(trs_flip(&g, trs_hint(&g)));
        CHECK(trs_hint(&g) < 0 && trs_go(&g, NULL) == TRS_OK);
        while (trs_undo(&g)) {}
        CHECK(g.set == first && g.moves == 0 && !g.solved);
    }

    if (fails) { printf("trains: %d FAILED\n", fails); return 1; }
    printf("trains: ok\n");
    return 0;
}
