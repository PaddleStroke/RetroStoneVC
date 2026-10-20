/*
 * Hex Landscape's rules: tiles as text and turned, placements (next to the land, rivers and rails meeting their own,
 * within reach), the points (edges met, the perfect tile and its bonus tile), regions, quests (exact: done closed at
 * n, failed past it or closed short; at least: done at n), tiles that fit nowhere set aside, undo of the last tile
 * only, the notation; objectives (met for good, the level over once all are met, lost without the first); every
 * level: one to three objectives, a play of it meeting them all (the levels' tool's, tests/hexland_lines.h), the hint's
 * player measured;
 * the random puzzles (the same from the same seed, their size, the time they take); whole games from a seed (random
 * placements, the hint's always allowed, undo).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/hexland.h"
#include "games/puzrand.h"
#include "hexland_lines.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static hxl_game g, t;

static int at(int q, int r) { return hxl_cell(q, r); }

static void setup(const char *start, const char *stack, const char *reserve)
{
    hxl_level_t lv = {start, stack, reserve, "", 100};
    CHECK(hxl_load(&g, &lv));
}

static double now_ms(void) { return (double)clock() * 1000.0 / CLOCKS_PER_SEC; }

int main(void)
{
    /* tiles */
    uint32_t a = hxl_tile_parse("mfwvrt");
    char s[8];
    CHECK(a && hxl_edge(a, 0) == HXL_MEADOW && hxl_edge(a, 5) == HXL_RAIL);
    hxl_tile_text(a, s);
    CHECK(!strcmp(s, "mfwvrt"));
    hxl_tile_text(hxl_turn(a, 1), s);
    CHECK(!strcmp(s, "tmfwvr"));
    CHECK(hxl_turn(a, 6) == a && hxl_turn(hxl_turn(a, 2), 4) == a);
    CHECK(!hxl_tile_parse("mfwvrx") && !hxl_tile_parse("mfw"));
    hxl_item it[4];
    CHECK(hxl_items_parse("ffffff=f12 vvmmmm+v8 mmmmmm", it, 4) == 3);
    CHECK(it[0].qn == 12 && it[0].qkind == HXL_EXACT && it[0].qterr == HXL_FOREST);
    CHECK(it[1].qn == 8 && it[1].qkind == HXL_ATLEAST && it[1].qterr == HXL_VILLAGE && !it[2].qn);
    CHECK(hxl_items_parse("ffffff=q12", it, 4) < 0 && hxl_items_parse("ffffff=f", it, 4) < 0);
    for (int d = 0; d < 6; d++) CHECK(hxl_nb(hxl_nb(HXL_MID, d), hxl_opp(d)) == HXL_MID);

    /* placements: next to the land; a river meets a river; soft terrains may meet anything */
    setup("MMmmmmmm", "rmmrmm mmmmmm ffffff", "");
    CHECK(!hxl_can(&g.s, at(2, 0), hxl_tile_parse("mmmmmm")));   /* not next to it */
    CHECK(!hxl_can(&g.s, HXL_MID, hxl_tile_parse("mmmmmm")));    /* taken */
    CHECK(!hxl_can(&g.s, at(1, 0), hxl_tile_parse("mmmrmm")));   /* its river (edge 3) meets meadow */
    CHECK(hxl_can(&g.s, at(1, 0), hxl_tile_parse("rmmmmm")));    /* the river runs off east: allowed */
    CHECK(hxl_gain(&g.s, at(1, 0), hxl_tile_parse("fffmff"), NULL, NULL) == HXL_MATCH);
    CHECK(hxl_gain(&g.s, at(1, 0), hxl_tile_parse("fffwff"), NULL, NULL) == 0);
    hxl_result res;
    CHECK(!hxl_play(&g, at(1, 0), 3, &res));                     /* turned so its river meets the meadow */
    CHECK(hxl_play(&g, at(1, 0), 1, &res) && res.points == HXL_MATCH && res.matches == 1);
    CHECK(g.s.score == HXL_MATCH && hxl_left(&g.s) == 2);
    /* the next tile must not meet the river's open end (south-east) with meadow */
    CHECK(!hxl_can(&g.s, at(1, 1), hxl_tile_parse("mmmmmm")) && hxl_can(&g.s, at(2, 0), hxl_tile_parse("mmmmmm")));
    CHECK(hxl_undo(&g) && g.s.score == 0 && hxl_left(&g.s) == 3 && !g.s.cell[at(1, 0)]);
    CHECK(!hxl_undo(&g));                                        /* the last placement only */
    CHECK(hxl_script(&g, "1,0,1 -1,0,0") && hxl_left(&g.s) == 1 && g.s.score == 2 * HXL_MATCH);
    CHECK(!hxl_script(&g, "5,5,0"));

    /* the perfect tile: six neighbours met; a tile from the reserve goes on the stack */
    setup("MLffffff NLffffff LMffffff NMffffff LNffffff MNffffff", "ffffff mmmmmm", "vvvvvv wwwwww");
    CHECK(hxl_play(&g, HXL_MID, 0, &res));
    CHECK(res.perfect && res.matches == 6 && res.bonus == HXL_PERFECT_TILES);
    CHECK(g.s.score == 6 * HXL_MATCH + HXL_PERFECT && g.s.perfects == 1 && hxl_left(&g.s) == 1 + HXL_PERFECT_TILES);
    CHECK(g.s.stack[g.s.ns - 1].tile == hxl_tile_parse("vvvvvv"));

    /* regions: the forest through the tiles joined by forest edges */
    setup("MMffmmmm", "mfmmmm mmmmmm", "");
    int open, size = hxl_region(&g.s, HXL_MID, HXL_FOREST, &open);
    CHECK(size == 1 && open);
    CHECK(hxl_play(&g, at(1, 0), 0, &res));                      /* "mfmmmm": its edge 3 (west) is meadow */
    CHECK(hxl_region(&g.s, HXL_MID, HXL_FOREST, &open) == 1);
    hxl_undo(&g);
    CHECK(hxl_play(&g, at(1, 0), 2, &res));                      /* turned: forest on its west edge */
    CHECK(hxl_region(&g.s, HXL_MID, HXL_FOREST, &open) == 2 && open);

    /* quests: a forest of exactly 2, done when it closes at 2 */
    setup("MMfmmmmm", "mmmfmm=f2 ffffff", "vvvvvv vvvvvv vvvvvv");
    CHECK(hxl_play(&g, at(1, 0), 0, &res));                      /* its forest (west) meets ours: 2, closed */
    CHECK(res.ndone == 1 && g.s.done == 1 && g.s.q[0].state == HXL_DONE && res.bonus == HXL_QUEST_TILES);
    CHECK(g.s.score == HXL_MATCH + HXL_QUEST && hxl_left(&g.s) == 1 + HXL_QUEST_TILES);
    /* exactly 1 but it grows to 2: failed */
    setup("MMfmmmmm", "fmmfmm=f1 mmmmmm", "");
    CHECK(hxl_play(&g, at(1, 0), 0, &res) && res.nfail == 1 && g.s.q[0].state == HXL_FAILED && !g.s.done);
    /* exactly 3 closed at 2: failed */
    setup("MMfmmmmm", "mmmfmm=f3", "");
    CHECK(hxl_play(&g, at(1, 0), 0, &res) && g.s.q[0].state == HXL_FAILED);
    /* at least 2: done open */
    setup("MMfmmmmm", "fmmfmm+f2 mmmmmm", "rrrrrr");
    CHECK(hxl_play(&g, at(1, 0), 0, &res) && g.s.q[0].state == HXL_DONE && hxl_left(&g.s) == 2);

    /* a tile that fits nowhere is set aside */
    setup("MMmmmmmm", "mmmmmm rrrrrr ffffff", "");
    CHECK(hxl_play(&g, at(1, 0), 0, &res) && res.skipped == 1 && g.s.skipped == 1);
    CHECK(hxl_current(&g.s)->tile == hxl_tile_parse("ffffff"));
    CHECK(hxl_stars(100, 100) == 3 && hxl_stars(80, 100) == 2 && hxl_stars(79, 100) == 1);

    /* objectives: as text both ways; a forest of 3, met for good; the level ends once all are met */
    hxl_obj ob[HXL_OBJS];
    char txt[64];
    CHECK(hxl_objs_parse("f6 =v5 p3", ob, HXL_OBJS) == 3 && ob[1].kind == HXL_OB_EXACT && ob[1].terr == HXL_VILLAGE);
    hxl_objs_text(ob, 3, txt, (int)sizeof txt);
    CHECK(!strcmp(txt, "f6 =v5 p3"));
    CHECK(hxl_objs_parse("=p3", ob, HXL_OBJS) < 0 && hxl_objs_parse("f", ob, HXL_OBJS) < 0 &&
          hxl_objs_parse("f1 f2 f3 f4", ob, HXL_OBJS) < 0);
    {
        hxl_level_t lv = {"MMfmmmmm", "fmmfmm mmmfmm ffffff", "", "f3 s1000", 0};
        CHECK(hxl_load(&g, &lv) && g.s.nobj == 2 && !g.s.met);
        CHECK(hxl_obj_progress(&g.s, 0, NULL) == 1);
        CHECK(hxl_play(&g, at(1, 0), 0, &res) && !res.obj_met && hxl_obj_progress(&g.s, 0, NULL) == 2);
        CHECK(hxl_play(&g, at(2, 0), 0, &res) && res.obj_met == 1 && g.s.met == 1);
        CHECK(!hxl_over(&g.s) && hxl_level_stars(&g.s, 0) == 2 && !hxl_lost(&g.s));
        hxl_level_t lv2 = {"MMfmmmmm", "mmmfmm mmmmmm", "", "=f2", 0};   /* exactly 2: closed at 2, then over */
        CHECK(hxl_load(&g, &lv2));
        CHECK(hxl_play(&g, at(1, 0), 0, &res) && res.obj_met == 1 && hxl_over(&g.s) && hxl_level_stars(&g.s, 0) == 3);
        CHECK(!hxl_play(&g, at(-1, 0), 0, &res));   /* over: nothing more */
        hxl_level_t lv3 = {"MMfmmmmm", "mmmmmm", "", "f2 p1", 0};    /* the stack out without the main one: lost */
        CHECK(hxl_load(&g, &lv3) && hxl_play(&g, at(1, 0), 0, &res) && hxl_over(&g.s) && hxl_lost(&g.s));
        CHECK(hxl_level_stars(&g.s, 0) == 0);
    }

    /* every level: one to three objectives, the first three levels one each; a play of it meets them all */
    int naive3 = 0, n_lv = 0, nobj[4] = {0}, kinds[5] = {0};
    double slow = 0;
    for (int L = 0; L < HXL_LEVELS; L++) {
        CHECK(hxl_level(&g, L));
        CHECK(g.s.ns >= 5 && g.start_tiles >= 1 && g.s.nobj >= 1 && g.s.nobj <= HXL_OBJS && !g.s.met);
        if (L < 3) CHECK(g.s.nobj == 1);
        nobj[g.s.nobj]++;
        for (int i = 0; i < g.s.nobj; i++) kinds[g.s.obj[i].kind]++;
        t = g;
        CHECK(hxl_script(&t, HXL_LINES[L]));
        if (hxl_level_stars(&t.s, 0) != 3) printf("level %d: its play meets %x of its objectives\n", L + 1, t.s.met);
        CHECK(hxl_over(&t.s) && hxl_level_stars(&t.s, 0) == 3);
        double t0 = now_ms();
        t = g;
        hxl_greedy(&t.s);
        if (now_ms() - t0 > slow) slow = now_ms() - t0;
        if (L >= 3) {                           /* the hint's player, the greedy one */
            n_lv++;
            t = g;
            hxl_script(&t, "=");
            int naive = hxl_level_stars(&t.s, 0);
            if (naive >= 3) naive3++;
        }
        for (int M = 0; M < L; M++)             /* no two levels alike */
            if (!strcmp(hxl_levels[M].stack, hxl_levels[L].stack)) { printf("levels %d and %d\n", M + 1, L + 1); fails++; }
    }
    /* (the hint is a help: its player is not held to fall short; the tool measures a greedy player blind to the
     * objectives, its table in its output) */
    printf("hexland: %d levels (objectives: %d with one, %d with two, %d with three; regions %d, exact %d, perfect %d, "
           "quests %d, score %d); the hint's player meets all of %d of the %d levels from 4; the greedy player at "
           "most %.0f ms\n", HXL_LEVELS, nobj[1], nobj[2], nobj[3], kinds[0], kinds[1], kinds[2], kinds[3], kinds[4],
           naive3, n_lv, slow);
    CHECK(!hxl_level(&g, HXL_LEVELS) && !hxl_level(&g, -1));

    /* random puzzles: the same from the same seed, the stack's size, the time */
    for (int v = 0; v < HXL_VARIANTS; v++) {
        double worst = 0;
        int lo = 1 << 30, hi = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            double t0 = now_ms();
            hxl_random(&g, v, seed);
            double ms = now_ms() - t0;
            if (ms > worst) worst = ms;
            hxl_random(&t, v, seed);
            CHECK(!memcmp(&g.s, &t.s, sizeof g.s) && g.goal == t.goal);
            CHECK(g.goal >= 10 && g.s.ns >= 20 && g.start_tiles == 1);
            if (g.s.ns < lo) lo = g.s.ns;
            if (g.s.ns > hi) hi = g.s.ns;
        }
        CHECK(worst < 900);
        printf("hexland: random variant %d, stacks of %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* whole games: random placements to the end, the hint always allowed, undo once */
    int ok = 1;
    static int moves[HXL_MOVES];
    for (uint32_t seed = 1; seed <= 40; seed++) {
        uint32_t r = pr_seed(seed);
        if (seed % 2) hxl_level(&g, (int)(seed * 13 % HXL_LEVELS));
        else hxl_random(&g, (int)(seed % HXL_VARIANTS), seed);
        int steps = 0;
        while (!hxl_over(&g.s) && steps++ < 400) {
            int c, k;
            if (steps % 5 == 0 && hxl_hint(&g.s, &c, &k)) {
                if (!hxl_can(&g.s, c, hxl_turn(hxl_current(&g.s)->tile, k))) ok = 0;
            }
            int n = hxl_moves(&g.s, moves);
            if (!n) { ok = 0; break; }
            int m = moves[pr_below(&r, n)], before = g.s.score;
            if (!hxl_play(&g, m / 6, m % 6, &res)) { ok = 0; break; }
            if (g.s.score < before) ok = 0;
            if (steps == 3) {
                hxl_state keep = g.s;
                if (!hxl_undo(&g) || hxl_undo(&g)) ok = 0;
                if (!hxl_play(&g, m / 6, m % 6, NULL) || memcmp(&keep, &g.s, sizeof keep)) ok = 0;
            }
        }
        if (!hxl_over(&g.s)) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("hexland: %d failures\n", fails); return 1; }
    printf("hexland: all passed\n");
    return 0;
}
