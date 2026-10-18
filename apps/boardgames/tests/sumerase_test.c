/*
 * Sum Erase's rules: the 150 designed levels (each read back, one solution by a plain search, the solver's solution
 * the one stored, the chapters growing), puzzles made from many seeds for every variant (one solution, at their level,
 * made fast), a small puzzle by hand, the player's cells, the mistakes, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/sumerase.h"
#include "games/sumerase_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* every line keeps a digit and loses one, and its kept digits make its target */
static int sound(const sme_puzzle *p, const uint8_t *sol)
{
    int n = p->n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int s = 0, gone = 0;
            for (int j = 0; j < n; j++) {
                int c = way ? j * n + i : i * n + j;
                if (p->num[c] < 1 || p->num[c] > 9) return 0;
                if (sol[c]) gone++;
                else s += p->num[c];
            }
            if (!gone || gone == n || s != (way ? p->col[i] : p->row[i])) return 0;
        }
    return 1;
}

static void test_levels(void)
{
    int last_n = 0, grades[3] = {0, 0, 0};
    for (int i = 0; i < SME_LEVELS; i++) {
        sme_puzzle p;
        uint8_t sol[SME_CELLS], mine[SME_CELLS];
        int ok = sme_decode(sme_levels[i], &p, sol);
        CHECK(ok);
        if (!ok) continue;
        CHECK(sound(&p, sol));
        CHECK(sme_count(&p, 2) == 1);
        int lv = sme_grade(&p, BENCH_HARD, mine);
        CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD);
        if (lv >= 0 && lv <= 2) grades[lv]++;
        CHECK(!memcmp(sol, mine, (size_t)(p.n * p.n)));
        char s[SME_CELLS + 8];
        CHECK(sme_encode(&p, sol, s, sizeof s) > 0 && !strcmp(s, sme_levels[i]));
        if (i % 25) CHECK(p.n >= last_n);       /* within a chapter the sizes grow */
        last_n = p.n;
        for (int j = 0; j < i; j++) CHECK(strcmp(sme_levels[j], sme_levels[i]));
        if (i < 10) CHECK(p.n == 3 && lv == BENCH_EASY);   /* a gentle start */
    }
    CHECK(grades[0] && grades[1] && grades[2]);
    printf("levels: %d easy, %d medium, %d hard\n", grades[0], grades[1], grades[2]);
}

static void test_make(void)
{
    clock_t worst = 0;
    for (int v = 0; v < SME_VARIANTS; v++) {
        int at_level = 0;
        for (uint32_t seed = 1; seed <= 40; seed++) {
            sme_puzzle p;
            uint8_t sol[SME_CELLS];
            clock_t t0 = clock();
            sme_make(sme_size(v), 9, sme_level(v), seed, 0, &p, sol);
            clock_t t = clock() - t0;
            if (t > worst) worst = t;
            CHECK(p.n >= 3 && p.n <= sme_size(v));
            CHECK(sound(&p, sol));
            CHECK(sme_count(&p, 2) == 1);
            uint8_t mine[SME_CELLS];
            int lv = sme_grade(&p, BENCH_HARD, mine);
            CHECK(lv >= BENCH_EASY && lv <= sme_level(v));
            CHECK(!memcmp(sol, mine, (size_t)(p.n * p.n)));
            at_level += lv == sme_level(v) && p.n == sme_size(v);
        }
        if (at_level < 36) printf("variant %d: %d of 40 at its level\n", v, at_level);
        /* nearly always the level asked for; a hard 6 x 6 is rarer (the nearest: a medium one) */
        CHECK(at_level >= (sme_size(v) == 6 && sme_level(v) == BENCH_HARD ? 20 : 36));
    }
    printf("make: the slowest puzzle %.0f ms\n", worst * 1000.0 / CLOCKS_PER_SEC);
    CHECK(worst * 1000.0 / CLOCKS_PER_SEC < 400);
}

/* a 3 x 3 by hand: rows 1 2 3 / 4 5 6 / 7 8 9, the corners erased */
static void test_hand(void)
{
    sme_puzzle p;
    uint8_t sol[SME_CELLS] = {1, 0, 1, 0, 0, 0, 1, 0, 1};
    memset(&p, 0, sizeof p);
    p.n = 3;
    for (int c = 0; c < 9; c++) p.num[c] = (uint8_t)(c + 1);
    sme_sums(&p, sol);
    CHECK(p.row[0] == 2 && p.row[1] == 15 && p.row[2] == 8);
    CHECK(p.col[0] == 4 && p.col[1] == 15 && p.col[2] == 6);
    sme_game g;
    CHECK(sme_load(&g, &p));
    CHECK(!memcmp(g.sol, sol, 9));
    CHECK(!sme_solved(&g));
    CHECK(sme_line(&g, 0, 0) == 0);             /* row 1: 1 2 3 make 6, the target 2 */
    CHECK(sme_script(&g, "a1"));                /* 1 erased: 5 left */
    CHECK(g.st[0] == SME_ERASE && sme_line(&g, 0, 0) == 0);
    CHECK(sme_script(&g, "b1"));                /* 2 erased too: 3 left, below 2? no: 3 > 2 */
    CHECK(sme_line(&g, 0, 0) == 0);
    CHECK(sme_script(&g, "c1"));                /* all erased: 0 < 2, wrong */
    CHECK(sme_line(&g, 0, 0) == -1);
    CHECK(sme_undo(&g) == 2 && sme_undo(&g) == 1);
    CHECK(g.hist.mistakes == 1);                /* b1 was wrong (kept in the solution) and was taken back */
    CHECK(sme_script(&g, ".b1"));               /* b1 ringed */
    CHECK(g.st[1] == SME_KEEP);
    CHECK(sme_script(&g, "c1 a3 c3"));
    CHECK(sme_solved(&g));
    CHECK(g.hist.hints == 0 && g.hist.moves == 5);
    sme_game h;                                 /* hints alone solve it */
    CHECK(sme_load(&h, &p));
    for (int i = 0; i < 9 && !sme_solved(&h); i++) CHECK(sme_hint(&h) >= 0);
    CHECK(sme_solved(&h) && h.hist.hints >= 1);
    CHECK(sme_load(&h, &p));
    CHECK(sme_script(&h, "a2-c2"));             /* a drag across row 2: erased, one move */
    CHECK(h.st[3] == SME_ERASE && h.st[4] == SME_ERASE && h.st[5] == SME_ERASE && h.hist.moves == 1);
    CHECK(sme_line(&h, 0, 1) == -1 && sme_undo(&h) == 3 && h.st[5] == SME_EMPTY);
}

/* whole games: hints and wrong taps, undone, from seeds */
static void test_games(void)
{
    for (uint32_t seed = 1; seed <= 30; seed++) {
        sme_game g;
        sme_new(&g, (int)(seed % SME_VARIANTS), seed);
        int n = g.p.n, cells = n * n;
        uint32_t s = pr_seed(seed);
        for (int k = 0; k < 2 * cells && !sme_solved(&g); k++) {
            int c = pr_below(&s, cells), to = pr_below(&s, 2) ? SME_ERASE : SME_KEEP;
            sme_paint(&g, &c, 1, to);
            if (pr_below(&s, 3) == 0) sme_undo(&g);
        }
        int guard = 0;
        while (!sme_solved(&g) && guard++ < 4 * cells) CHECK(sme_hint(&g) >= 0);
        CHECK(sme_solved(&g));
        while (sme_undo(&g) >= 0) {}
        for (int c = 0; c < cells; c++) CHECK(g.st[c] == SME_EMPTY);
        CHECK(g.hist.moves == 0);
        CHECK(sme_script(&g, "+999") && sme_solved(&g));
    }
}

int main(void)
{
    test_hand();
    test_levels();
    test_make();
    test_games();
    if (fails) {
        printf("sumerase: %d failures\n", fails);
        return 1;
    }
    printf("sumerase: ok\n");
    return 0;
}
