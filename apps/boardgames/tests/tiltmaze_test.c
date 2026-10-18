/*
 * Tilt Maze's rules: a marble rolling to a wall, into its hole, over a filled hole, the front marble first, a marble in
 * another colour's hole (stuck: no more tilts), a gate, a one-way tile, a lid lifting under a marble at rest, a tilt
 * that moves nothing; trays as text both ways; every level read back, its par the search's and its shortest way
 * played into the holes in par tilts, each chapter's special tiles there; the puzzles made at random (their par in the
 * variant's range most of the time, the time they take); whole games from a seed (random tilts, hints to the end with
 * undo out of dead ends, then undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/tiltmaze.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)


static int one(const char *map, const char *moves)   /* a tray played: 1 solved */
{
    tlm_game g;
    memset(&g, 0, sizeof g);
    if (!tlm_parse(&g.m, map)) return -1;
    tlm_begin(&g);
    if (!tlm_script(&g, moves)) return -2;
    return tlm_solved(&g.m, &g.s);
}

int main(void)
{
    tlm_game g;
    tlm_state t;
    tlm_trace tr;
    char text[3 * TLM_CELLS];

    CHECK(tlm_parse(&g.m, "R0.0r0"));
    CHECK(g.m.w == 3 && g.m.h == 1 && g.m.nm == 1 && g.m.start[0] == 0 && g.m.t[2] == TLM_HOLE);
    tlm_begin(&g);
    CHECK(!tlm_try(&g.m, &g.s, TLM_LEFT, &t, NULL));   /* against the edge: nothing moves */
    CHECK(tlm_try(&g.m, &g.s, TLM_RIGHT, &t, &tr) && tr.len[0] == 2 && tr.drop[0] == 1 && tlm_solved(&g.m, &t));
    CHECK(one("R1.0r0", "r") == -2);            /* a wall on its right */
    CHECK(one("R0R0r0.0r0", "r") == 1);         /* the front one fills the first hole, the other rolls over it */
    CHECK(tlm_parse(&g.m, "R0B0.0.0r0b0") && g.m.nm == 2);
    tlm_begin(&g);
    CHECK(tlm_try(&g.m, &g.s, TLM_RIGHT, &t, &tr));   /* the front marble first: the blue into the red hole */
    CHECK(tlm_dead(&t) && (t.stuck & 2) && t.at[1] == 4 && t.at[0] == 3);
    CHECK(!tlm_try(&g.m, &t, TLM_LEFT, NULL, NULL));  /* stuck: no more tilts */
    CHECK(one("B0k0b0", "r") == -2);            /* the red gate stops the blue marble */
    CHECK(one("R0k0r0", "r") == 1);             /* and lets the red through */
    CHECK(one("R0>0r0", "r") == 1);
    CHECK(one("r0>0R0", "l") == -2);            /* the one-way tile the wrong way */
    CHECK(one("r0<0R0", "l") == 1);
    CHECK(tlm_parse(&g.m, "R0.010/R0r0.0"));    /* a lid: it lifts once the open red hole is filled */
    tlm_begin(&g);
    CHECK(g.m.t[2] == TLM_LID && !tlm_lid_open(&g.m, &g.s, 2));
    CHECK(tlm_try(&g.m, &g.s, TLM_RIGHT, &t, &tr) && tlm_solved(&g.m, &t) && tr.drop[0] == 2 && tr.drop[1] == 1);
    CHECK(one("R0.0.0/.0.010/R0.0r0", "r") == 0);   /* the red hole filled, the lid lifted: nothing on it yet */
    CHECK(one("R0.0.0/.0.010/R0.0r0", "rd") == 1);
    CHECK(!tlm_parse(&g.m, "R0.0b0"));          /* a hole for each marble */
    CHECK(tlm_parse(&g.m, "R2.0#1/l0v0r0"));
    tlm_text(&g.m, text);
    CHECK(!strcmp(text, "R2.0#1/l0v0r0"));

    /* every level */
    int per = TLM_LEVELS / TLM_CHAPTERS;
    for (int lv = 0; lv < TLM_LEVELS; lv++) {
        CHECK(tlm_level(&g, lv));
        tlm_text(&g.m, text);
        CHECK(!strcmp(text, tlm_levels[lv]));
        CHECK(!tlm_solved(&g.m, &g.s) && g.par > 0 && g.par == tlm_level_par[lv]);
        CHECK(tlm_solve(&g.m, &g.s, NULL, NULL) == g.par);
        CHECK(tlm_script(&g, "=") && tlm_solved(&g.m, &g.s) && g.moves == g.par);
        CHECK(g.m.w <= 8 && g.m.h <= 10);
        int ch = lv / per, cols = 0, kinds[6] = {0};
        for (int c = 0; c < g.m.w * g.m.h; c++) kinds[g.m.t[c]]++;
        for (int i = 0; i < g.m.nm; i++)
            if (!i || g.m.col[i] != g.m.col[i - 1]) cols++;
        int special = kinds[TLM_LID] + kinds[TLM_GATE] + kinds[TLM_ONEWAY];
        if (ch == 0) CHECK(cols == 1 && !special);
        if (ch == 1) CHECK(cols >= 2 && !special);
        if (ch == 2) CHECK(kinds[TLM_GATE] > 0);
        if (ch == 3) CHECK(kinds[TLM_ONEWAY] > 0);
        if (ch == 4) CHECK(kinds[TLM_LID] > 0);
    }
    for (int i = 0; i < TLM_LEVELS; i++)
        for (int j = i + 1; j < TLM_LEVELS; j++) CHECK(strcmp(tlm_levels[i], tlm_levels[j]));
    CHECK(tlm_level_par[0] <= 3);

    /* the puzzles made at random */
    for (int v = 0; v < TLM_VARIANTS; v++) {
        tlm_spec sp;
        tlm_variant(v, &sp);
        int in = 0, runs = 40;
        double worst = 0;
        for (int k = 0; k < runs; k++) {
            clock_t c0 = clock();
            tlm_random(&g, v, 3000u + (uint32_t)k * 31u);
            double sec = (double)(clock() - c0) / CLOCKS_PER_SEC;
            if (sec > worst) worst = sec;
            CHECK(g.par > 0 && g.par <= sp.hi);
            if (g.par >= sp.lo) in++;
            CHECK(tlm_script(&g, "=") && g.moves == g.par);
        }
        printf("tiltmaze variant %d: %d/%d in range, slowest %.0f ms\n", v, in, runs, worst * 1000);
        CHECK(in >= runs * 3 / 4);
        CHECK(worst < 0.25);
    }

    /* whole games from a seed */
    for (int k = 0; k < 30; k++) {
        uint32_t rs = pr_seed(700u + (uint32_t)k);
        tlm_level(&g, (int)(pr_next(&rs) % TLM_LEVELS));
        tlm_state s0 = g.s;
        for (int i = 0; i < 20; i++) tlm_move(&g, pr_below(&rs, 4));
        int guard = 0;
        while (!tlm_solved(&g.m, &g.s) && guard++ < 500) {
            int d = tlm_hint(&g);
            if (d < 0) { CHECK(tlm_undo(&g)); continue; }
            CHECK(tlm_move(&g, d));
        }
        CHECK(tlm_solved(&g.m, &g.s));
        CHECK(!tlm_move(&g, TLM_UP));
        while (tlm_undo(&g)) {}
        CHECK(g.moves == 0 && !memcmp(&g.s, &s0, sizeof s0));
    }

    if (fails) printf("tiltmaze: %d failures\n", fails);
    else printf("tiltmaze: all good\n");
    return fails != 0;
}
