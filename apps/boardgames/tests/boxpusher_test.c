/*
 * Box Pusher's rules: every level read (as many places as crates, the keeper inside, nothing solved at the start),
 * solved by the push-optimal search within the time a phone allows and played to the end by its hints (exactly the
 * fewest pushes), graded (the chapters' fewest pushes growing on the whole), all different; steps, pushes, walls, two
 * crates never pushed at once, walks round the crates, undo by whole actions; the hint from stuck positions (it goes
 * back to where a way remains); whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/boxpusher.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

int main(void)
{
    int pushes[BP_LEVELS], chapter_sum[BP_CHAPTERS] = {0};
    double worst = 0;
    for (int lv = 0; lv < BP_LEVELS; lv++) {
        bp_game g;
        bp_new(&g, lv);
        int places = 0;
        for (int c = 0; c < g.w * g.h; c++) places += g.goal[c];
        CHECK(g.player >= 0 && g.map[g.player] == BP_FLOOR && g.nbox >= 2 && g.nbox == places && !bp_solved(&g));
        CHECK(g.w <= 10 && g.h <= 10);
        clock_t t0 = clock();
        pushes[lv] = bp_solve(&g, NULL, NULL);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > worst) worst = ms;
        CHECK(pushes[lv] > 0 && pushes[lv] == bp_par[lv]);
        chapter_sum[lv / BP_PER_CHAPTER] += pushes[lv];
        CHECK(bp_script(&g, "=") && bp_solved(&g) && g.pushes == pushes[lv]);
        for (int k = 0; k < lv; k++) CHECK(strcmp(bp_levels[k], bp_levels[lv]) != 0);
    }
    printf("boxpusher: fewest pushes by chapter %d, %d, %d; the slowest search %.1f ms\n", chapter_sum[0],
           chapter_sum[1], chapter_sum[2], worst);
    CHECK(chapter_sum[0] < chapter_sum[1] && chapter_sum[1] < chapter_sum[2]);
    CHECK(worst < 400);

    /* a little room: "#####|#@$.#|#####" and more */
    bp_game g;
    g.level = 0;
    CHECK(bp_load(&g, "######|#@$ .#|######"));
    CHECK(g.w == 6 && g.h == 3 && g.nbox == 1 && g.player == 7);
    CHECK(!bp_can(&g, BP_UP) && !bp_can(&g, BP_LEFT) && bp_can(&g, BP_RIGHT));
    CHECK(bp_step(&g, BP_RIGHT) == 2 && g.pushes == 1 && g.moves == 1 && g.box[9] && g.player == 8);
    bp_begin(&g);                              /* a new action */
    CHECK(bp_step(&g, BP_RIGHT) == 2 && bp_solved(&g));
    CHECK(!bp_step(&g, BP_RIGHT));             /* the crate against the wall */
    CHECK(bp_undo(&g) && g.box[9] && g.pushes == 1 && bp_undo(&g) && g.player == 7 && g.moves == 0);
    CHECK(!bp_undo(&g));
    CHECK(bp_load(&g, "#######|#@$$ .#|#   .##|#######"));   /* two crates in a row: never pushed together */
    CHECK(!bp_can(&g, BP_RIGHT) && bp_can(&g, BP_DOWN));
    /* a walk round a crate is one action; undo takes it back whole */
    CHECK(bp_walk(&g, 17) && g.player == 17 && g.moves == 3 && g.pushes == 0);
    bp_begin(&g);
    CHECK(bp_step(&g, BP_RIGHT) == 1 && g.moves == 4);
    CHECK(bp_undo(&g) && g.player == 17 && bp_undo(&g) && g.player == 8 && g.moves == 0);
    uint8_t dirs[BP_CELLS];
    CHECK(bp_path(&g, 9, dirs, BP_CELLS) < 0);   /* onto a crate: no */
    CHECK(!bp_load(&g, "#####|#@$#|#####"));   /* a crate and no place */
    CHECK(!bp_load(&g, "#####|#$.#|#####"));   /* no keeper */
    CHECK(!bp_load(&g, "#?#"));

    /* stuck: a crate against a wall; the hint goes back, then pushes on the way */
    CHECK(bp_load(&g, "######|#    #|# $@ #|#   .#|######"));
    CHECK(bp_step(&g, BP_LEFT) == 2);          /* the crate against the left wall: it never comes back right */
    CHECK(bp_solve(&g, NULL, NULL) == -1);
    CHECK(bp_hint(&g) >= 0 && g.pushes == 1);  /* back, then the first push of a way */
    CHECK(bp_script(&g, "=") && bp_solved(&g));

    /* whole games: random steps from a seed, then the hints (going back when stuck) to the end */
    int whole = 1;
    for (uint32_t seed = 1; seed <= 120; seed++) {
        uint32_t r = pr_seed(seed);
        bp_new(&g, (int)(seed % BP_LEVELS));
        int n = pr_below(&r, 60);
        for (int i = 0; i < n; i++) {
            bp_begin(&g);
            bp_step(&g, pr_below(&r, 4));
        }
        bp_script(&g, "=");
        if (!bp_solved(&g)) whole = 0;
        while (bp_undo(&g)) {}
        bp_game fresh;
        fresh.level = g.level;
        bp_new(&fresh, g.level);
        if (memcmp(fresh.box, g.box, sizeof g.box) || fresh.player != g.player || g.moves || g.pushes) whole = 0;
    }
    CHECK(whole);

    if (fails) { printf("boxpusher: %d failures\n", fails); return 1; }
    printf("boxpusher: all passed\n");
    return 0;
}
