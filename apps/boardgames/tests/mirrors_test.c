/*
 * Mirrors' rules: the beam (mirrors, glass, blocks, bombs, the board's edge), the search on small hand-made puzzles,
 * the puzzles made from many seeds (one way to solve them, the level's work, never solved from the start, the same
 * from the same seed, made fast), taps, moves, hints to the end and undo all the way back; every level (one way to
 * solve it, solved in exactly par actions by the fewest taps on the solution's cells).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/mirrors.h"
#include "games/mirrors_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a board from rows: '.' empty, '#' block, '>' '^' '<' 'v' the lamp, 'o' a gem, '*' a bomb, '/' '\' pivots,
 * 's' 'b' loose mirrors standing '/' '\', 'S' 'B' glass '/' '\' */
static void load(mr_game *g, int n, const char *const *rows, int loose)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->loose = loose;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int c = x + y * n;
            switch (rows[y][x]) {
            case '#': g->kind[c] = MR_WALL; break;
            case '>': case '^': case '<': case 'v':
                g->kind[c] = MR_LAMP;
                g->way[c] = (uint8_t)(rows[y][x] == '>' ? 0 : rows[y][x] == '^' ? 1 : rows[y][x] == '<' ? 2 : 3);
                g->lamp = c;
                break;
            case 'o': g->kind[c] = MR_GEM; break;
            case '*': g->kind[c] = MR_BOMB; break;
            case '/': g->kind[c] = MR_PIVOT; g->way[c] = MR_SLASH; break;
            case '\\': g->kind[c] = MR_PIVOT; g->way[c] = MR_BACK; break;
            case 's': g->kind[c] = MR_LOOSE; g->way[c] = MR_SLASH; break;
            case 'b': g->kind[c] = MR_LOOSE; g->way[c] = MR_BACK; break;
            case 'S': g->kind[c] = MR_GLASS; g->way[c] = MR_SLASH; break;
            case 'B': g->kind[c] = MR_GLASS; g->way[c] = MR_BACK; break;
            }
        }
}

static int same_puzzle(const mr_game *a, const mr_game *b)
{
    return a->n == b->n && a->lamp == b->lamp && a->loose == b->loose && !memcmp(a->kind, b->kind, sizeof a->kind) &&
           !memcmp(a->way, b->way, sizeof a->way);
}

int main(void)
{
    mr_game g;
    mr_beam b;

    /* the beam: reflections both ways, a gem passed through, out at the edge */
    static const char *const r1[] = {">..\\.", ".....", "....o", "...\\/", "....."};
    load(&g, 5, r1, 0);
    mr_trace(&g, &b);
    CHECK(b.side[3] == (1 << 2 | 1 << 3));      /* d1 '\': in from the west, out to the south */
    CHECK(b.side[13] == (1 << 1 | 1 << 3));     /* d3 crossed north to south */
    CHECK(b.side[18] == (1 << 1 | 1 << 0));     /* d4 '\': in from the north, out to the east */
    CHECK(b.side[19] == (1 << 2 | 1 << 1));     /* e4 '/': in from the west, out to the north */
    CHECK(b.side[4] == (1 << 3 | 1 << 1));      /* e1: out at the top edge */
    CHECK(b.side[12] == 0 && b.side[23] == 0);
    CHECK(b.gems == 1 && b.lit == 1 && b.bomb < 0);
    CHECK(b.dist[3] == 3 && b.dist[18] == 6 && b.dist[4] == 10 && b.length == 10);
    CHECK(mr_solved(&g));
    /* a block stops it, a bomb stops it and is touched */
    static const char *const r2[] = {">.#..", ".....", ".....", ".....", "....."};
    load(&g, 5, r2, 0);
    mr_trace(&g, &b);
    CHECK(b.side[1] == (1 << 2 | 1 << 0) && b.side[2] == 0 && b.length == 1);
    static const char *const r3[] = {">o*o.", ".....", ".....", ".....", "....."};
    load(&g, 5, r3, 0);
    mr_trace(&g, &b);
    CHECK(b.bomb == 2 && b.lit == 1 && b.side[3] == 0 && !mr_solved(&g));
    /* glass: through and turned; a loop through two glasses ends */
    static const char *const r4[] = {">.B.o", ".....", "..o..", ".....", "....."};
    load(&g, 5, r4, 0);
    mr_trace(&g, &b);
    CHECK(b.lit == 2 && b.side[2] == (1 << 2 | 1 << 0 | 1 << 3) && mr_solved(&g));
    static const char *const r5[] = {"v....", "S...B", ".....", ".....", "B...S"};
    load(&g, 5, r5, 0);
    mr_trace(&g, &b);
    CHECK(b.side[24] && b.side[20] && b.side[9]);

    /* the search: a pivot to turn (one way), a loose mirror to stand (one place, one way) */
    static const char *const s1[] = {">..\\.", ".....", "...o.", ".....", "....."};
    load(&g, 5, s1, 0);
    long work;
    CHECK(mr_solve(&g, 9, &work) == 1 && g.sol[3] == MR_BACK && work == 1);
    g.way[3] = MR_SLASH;
    CHECK(!mr_solved(&g) && mr_hint(&g) == 3 && g.way[3] == MR_BACK && mr_solved(&g) && mr_hint(&g) < 0);
    static const char *const s2[] = {">....", ".....", ".....", "..o..", "....."};
    load(&g, 5, s2, 1);
    CHECK(mr_solve(&g, 9, &work) == 1 && g.sol[2] == MR_BACK && g.sol[1] == 2);
    CHECK(mr_loose_left(&g) == 1 && mr_hint(&g) == 2 && g.kind[2] == MR_LOOSE && mr_solved(&g));
    CHECK(mr_loose_left(&g) == 0);
    /* two ways: down at c1, or down at b1 and back east at b3 */
    static const char *const s3[] = {">....", ".....", "..o..", ".....", "....."};
    load(&g, 5, s3, 2);
    CHECK(mr_solve(&g, 99, &work) > 1);
    /* a bomb in the way: the only way round it */
    static const char *const s4[] = {">.*.o", ".....", ".....", ".....", "....."};
    load(&g, 5, s4, 0);
    CHECK(mr_solve(&g, 9, &work) == 0);

    /* taps: a pivot turns; an empty cell: '/', '\', off; none left: no; blocks, gems: nothing */
    static const char *const t1[] = {">./..", ".....", ".#o*.", ".....", "....."};
    load(&g, 5, t1, 1);
    CHECK(mr_tap(&g, 2) && g.way[2] == MR_BACK && mr_tap(&g, 2) && g.way[2] == MR_SLASH);
    CHECK(mr_tap(&g, 5) && g.kind[5] == MR_LOOSE && g.way[5] == MR_SLASH);
    CHECK(!mr_tap(&g, 6));                      /* the only loose mirror is out */
    CHECK(mr_tap(&g, 5) && g.way[5] == MR_BACK && mr_tap(&g, 5) && g.kind[5] == MR_EMPTY);
    CHECK(!mr_tap(&g, 11) && !mr_tap(&g, 12) && !mr_tap(&g, 13) && !mr_tap(&g, 0));
    CHECK(mr_set(&g, 6, MR_BACK) && mr_move(&g, 6, 20) && g.kind[6] == MR_EMPTY && g.kind[20] == MR_LOOSE &&
          g.way[20] == MR_BACK);
    CHECK(!mr_move(&g, 20, 11) && !mr_move(&g, 2, 7));
    CHECK(g.moves == 7);
    while (mr_undo(&g) >= 0) {}
    CHECK(g.moves == 0 && g.kind[20] == MR_EMPTY && g.kind[5] == MR_EMPTY && g.way[2] == MR_SLASH);
    CHECK(mr_script(&g, "c1 a2 a2 a2 b2b b2>e5 h") == 0);   /* the hint: nothing to light here, no solution */
    load(&g, 5, s2, 1);
    mr_solve(&g, 2, NULL);
    CHECK(mr_script(&g, "c1s c1>c2 c2e =") && mr_solved(&g) && g.moves == 4);
    CHECK(!mr_script(&g, "z9") && !mr_script(&g, "a1") && !mr_script(&g, "c3"));

    /* the puzzles from many seeds */
    for (int level = 0; level < MR_VARIANTS; level++) {
        long wsum = 0, wmax = 0;
        double worst = 0;
        int seeds = level == 3 ? 150 : 300;
        for (int seed = 1; seed <= seeds; seed++) {
            clock_t t0 = clock();
            mr_new(&g, level, (uint32_t)seed * 7919u + (uint32_t)level);
            double ms = 1000.0 * (clock() - t0) / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            CHECK(g.n == 5 + level && g.kind[g.lamp] == MR_LAMP);
            CHECK(!mr_solved(&g));
            mr_game h = g;
            CHECK(mr_solve(&h, 2, &work) == 1);
            wsum += work;
            if (work > wmax) wmax = work;
            int gems = 0, bombs = 0, pivots = 0, glass = 0;
            for (int c = 0; c < g.n * g.n; c++) {
                gems += g.kind[c] == MR_GEM;
                bombs += g.kind[c] == MR_BOMB;
                pivots += g.kind[c] == MR_PIVOT;
                glass += g.kind[c] == MR_GLASS;
            }
            CHECK(gems >= 2 && pivots >= 3 && g.loose == (level > 2 ? 3 : level));
            CHECK(level < 2 || glass == 1);
            CHECK(level < 1 || bombs >= 1);
            mr_game same;
            mr_new(&same, level, (uint32_t)seed * 7919u + (uint32_t)level);
            CHECK(same_puzzle(&g, &same));
            /* played: some taps, the hints to the end, undo all the way back */
            mr_game start = g;
            for (int i = 0; i < 6; i++) mr_tap(&g, (seed * 13 + i * 7) % (g.n * g.n));
            int hints = 0;
            while (!mr_solved(&g) && hints < 40) {
                if (mr_hint(&g) < 0) break;
                hints++;
            }
            CHECK(mr_solved(&g));
            mr_beam fb;
            mr_trace(&g, &fb);
            CHECK(fb.bomb < 0 && fb.lit == fb.gems);
            while (mr_undo(&g) >= 0) {}
            CHECK(g.moves == 0 && !memcmp(g.kind, start.kind, sizeof g.kind) && !memcmp(g.way, start.way, sizeof g.way));
        }
        printf("mirrors level %d: work mean %.1f max %ld, slowest %.1f ms\n", level, (double)wsum / seeds, wmax, worst);
        CHECK(worst < 500);
    }

    /* every level: it reads back the same, has one way to solve it (mr_decode checks it), is not solved at the start,
     * and the fewest taps on the solution's cells solve it in exactly par actions; hints count; no two alike */
    {
        static mr_game g;
        int last_n = 0, grows = 1;
        for (int i = 0; i < MR_LEVELS; i++) {
            CHECK(mr_decode(&g, mr_levels[i]));
            char code[MR_CELLS + 8];
            CHECK(mr_encode(&g, code, sizeof code) && !strcmp(code, mr_levels[i]));
            CHECK(!mr_solved(&g) && g.par >= 1 && g.moves == 0 && g.hints == 0);
            if (i < 25) CHECK(g.n <= 5 && g.loose == 0);
            grows &= g.n >= last_n - 1;
            last_n = g.n;
            for (int k = 0; k < g.norder; k++) {
                int c = g.order[k], want = g.sol[c];
                if (want < 0 || want == 2) continue;
                if (g.kind[c] == MR_PIVOT && g.way[c] != want) CHECK(mr_tap(&g, c));
                if (g.kind[c] == MR_EMPTY) {
                    CHECK(mr_tap(&g, c));       /* '/' */
                    if (want == MR_BACK) CHECK(mr_tap(&g, c));
                }
            }
            CHECK(mr_solved(&g) && g.moves == g.par);
            for (int j = 0; j < i; j++) CHECK(strcmp(mr_levels[i], mr_levels[j]));
        }
        CHECK(grows);
        CHECK(mr_decode(&g, mr_levels[0]) && mr_hint(&g) >= 0 && mr_undo(&g) >= 0 && g.hints == 1 && g.moves == 0);
        CHECK(!mr_decode(&g, "50:x") && !mr_decode(&g, "9") && !mr_decode(&g, "20:...."));
    }

    if (fails) printf("mirrors: %d failures\n", fails);
    else printf("mirrors: all passed\n");
    return fails != 0;
}
