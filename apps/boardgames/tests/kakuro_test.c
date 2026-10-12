/*
 * Kakuro's rules: the puzzles made from many seeds for every variant (the dark squares' pattern: turned half round it
 * stays the same, the top row and the left column dark, every run 2..9 squares, the white squares joined; the
 * solution's runs without repeats and making their clues; one solution by a plain search; the level they were made
 * for; made fast), a small puzzle by hand, the player's digits, notes, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/kakuro.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int pattern_ok(const kkr_puzzle *p)
{
    int n = p->n, m = n - 1, whites = 0;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if ((x == 0 || y == 0) && p->white[c]) return 0;
        if (x && y && p->white[c] != p->white[(m - y + 1) * n + (m - x + 1)]) return 0;
        whites += p->white[c];
    }
    kkr_run runs[KKR_RUNS];
    uint8_t run_of[KKR_CELLS][2];
    memset(run_of, 0xff, sizeof run_of);
    int k = kkr_runs(p, runs, run_of);
    for (int i = 0; i < k; i++) {
        if (runs[i].len < 2 || runs[i].len > 9) return 0;
        unsigned used = 0;
        int sum = 0;
        for (int j = 0; j < runs[i].len; j++) {
            int d = p->sol[runs[i].cell[j]];
            if (d < 1 || d > 9 || (used >> d & 1)) return 0;
            used |= 1u << d;
            sum += d;
        }
        if (sum != runs[i].sum) return 0;
    }
    for (int c = 0; c < n * n; c++)
        if (p->white[c] && (run_of[c][0] == 0xff || run_of[c][1] == 0xff)) return 0;
    int seen[KKR_CELLS] = {0}, stack[KKR_CELLS], sp = 0, reached = 0;
    for (int c = 0; c < n * n && !sp; c++)
        if (p->white[c]) stack[sp++] = c, seen[c] = 1;
    while (sp) {
        int c = stack[--sp];
        reached++;
        const int nb[4] = {c - 1, c + 1, c - n, c + n};
        for (int t = 0; t < 4; t++)
            if (nb[t] >= 0 && nb[t] < n * n && p->white[nb[t]] && !seen[nb[t]]) seen[nb[t]] = 1, stack[sp++] = nb[t];
    }
    return reached == whites;
}

int main(void)
{
    /* by hand: a 3 x 3 square of white squares under a dark row and column */
    kkr_puzzle h;
    memset(&h, 0, sizeof h);
    h.n = 4;
    const uint8_t sol[3][3] = {{1, 2, 4}, {2, 4, 1}, {4, 1, 2}};  /* not unique: a Latin square of 1, 2, 4 */
    for (int y = 1; y < 4; y++)
        for (int x = 1; x < 4; x++) h.white[y * 4 + x] = 1, h.sol[y * 4 + x] = sol[y - 1][x - 1];
    for (int i = 1; i < 4; i++) h.across[i * 4] = 7, h.down[i] = 7;
    CHECK(kkr_count(&h, 3) >= 2);
    CHECK(kkr_rate(&h) == PZB_GUESS);
    h.across[4] = 6;                             /* now 1 2 3 on the first row: no way down (7 in each column) */
    CHECK(kkr_count(&h, 2) == 0);
    CHECK(kkr_rate(&h) == PZB_NONE);

    for (int var = 0; var < KKR_VARIANTS; var++) {
        int n = kkr_size_of(var), level = kkr_level_of(var), exact = 0, whites = 0;
        double slowest = 0, total = 0;
        int tries = n >= 10 ? 30 : 50;
        for (int s = 1; s <= tries; s++) {
            kkr_puzzle p;
            clock_t t0 = clock();
            kkr_make(n, level, (uint32_t)s * 7919u + (uint32_t)var, &p);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(p.n == n);
            CHECK(pattern_ok(&p));
            int r = kkr_rate(&p);
            CHECK(r >= 0 && r <= level);
            exact += r == level;
            for (int c = 0; c < n * n; c++) whites += p.white[c];
            if (s <= 8) CHECK(kkr_count(&p, 2) == 1);
        }
        printf("kakuro %dx%d level %d: %d/%d of the level, %.0f white squares, %.1f ms on average, slowest %.1f ms\n",
               n, n, level, exact, tries, (double)whites / tries, 1000 * total / tries, 1000 * slowest);
        CHECK(exact >= tries * 9 / 10);
        CHECK(slowest < 0.35);
    }

    /* the player's grid */
    kkr_game g;
    kkr_new(&g, 1, 2024);                        /* 6 x 6 medium */
    int n = g.p.n, a = -1, b = -1;
    for (int c = 0; c < n * n; c++)
        if (g.p.white[c]) { a = c; break; }
    CHECK(a >= 0 && g.p.white[a + 1]);           /* the first white square has another beside it */
    b = a + 1;
    CHECK(!kkr_put(&g, 0, 3));                   /* a dark square takes nothing */
    CHECK(kkr_note(&g, b, 5) && g.notes[b] == 1u << 5);
    int d = g.p.sol[a] == 5 ? 6 : 5;
    CHECK(kkr_put(&g, a, 5) && !(g.notes[b] >> 5 & 1));   /* the note 5 leaves the run */
    if (d == 6) CHECK(g.val[a] == g.p.sol[a]);
    CHECK(kkr_put(&g, b, 5) && kkr_clash(&g, a) && kkr_clash(&g, b));
    int moves = g.hist.moves;
    CHECK(kkr_undo(&g) == b && g.hist.moves == moves - 1 && !g.val[b]);
    CHECK(kkr_undo(&g) >= 0 && !g.val[a] && (g.notes[b] >> 5 & 1));
    CHECK(kkr_erase(&g, b) && !g.notes[b]);
    while (kkr_undo(&g) >= 0) {}
    CHECK(g.hist.moves == 0);
    CHECK(kkr_hint(&g) >= 0);
    CHECK(!kkr_script(&g, "a1=3"));              /* a1 is dark */

    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t rng = pr_seed(seed);
        kkr_new(&g, (int)(seed % KKR_VARIANTS), seed);
        n = g.p.n;
        int steps = 0;
        while (!kkr_solved(&g) && steps < 3000) {
            int c = pr_below(&rng, n * n), roll = steps < n * n ? pr_below(&rng, 10) : 9;
            if (roll < 4) kkr_put(&g, c, 1 + pr_below(&rng, 9));
            else if (roll < 6) kkr_note(&g, c, 1 + pr_below(&rng, 9));
            else if (roll < 7) kkr_erase(&g, c);
            else CHECK(kkr_hint(&g) >= 0);
            steps++;
        }
        CHECK(kkr_solved(&g));
        CHECK(kkr_hint(&g) < 0);
        while (kkr_undo(&g) >= 0) {}
        CHECK(g.hist.moves == 0);
        for (int c = 0; c < n * n; c++) CHECK(!g.val[c] && !g.notes[c]);
    }

    printf(fails ? "kakuro: %d FAILED\n" : "kakuro: all passed\n", fails);
    return fails != 0;
}
