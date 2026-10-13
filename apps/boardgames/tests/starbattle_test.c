/*
 * Star Battle's rules: the solver and the grader on a puzzle by hand, the puzzles made from many seeds at every size
 * and level (one solution, counted apart from the grader; regions in one piece, k stars each, none touching; the level
 * they were made for), the time the biggest take, the 150 designed levels (one solution each, the chapters growing),
 * the player's marks, runs of dots, clashes, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/starbattle.h"
#include "games/starbattle_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int connected(const sbx_puz *p, int r)
{
    int n = p->n, seen[SBX_CELLS] = {0}, q[SBX_CELLS], qh = 0, qt = 0, size = 0;
    for (int c = 0; c < n * n; c++)
        if (p->region[c] == r) {
            size++;
            if (!qt) seen[c] = 1, q[qt++] = c;
        }
    while (qh < qt) {
        int c = q[qh++], x = c % n, y = c / n;
        int nb[4] = {x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y > 0 ? c - n : -1, y < n - 1 ? c + n : -1};
        for (int i = 0; i < 4; i++)
            if (nb[i] >= 0 && !seen[nb[i]] && p->region[nb[i]] == r) seen[nb[i]] = 1, q[qt++] = nb[i];
    }
    return size > 0 && qt == size;
}

static int solution_ok(const sbx_puz *p, const uint8_t *st)
{
    int n = p->n, rows[SBX_MAX] = {0}, cols[SBX_MAX] = {0}, regs[SBX_MAX] = {0};
    for (int c = 0; c < n * n; c++) {
        if (!st[c]) continue;
        rows[c / n]++, cols[c % n]++, regs[p->region[c]]++;
        for (int d = 0; d < n * n; d++) {
            int dx = c % n - d % n, dy = c / n - d / n;
            if (d != c && st[d] && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) return 0;
        }
    }
    for (int i = 0; i < n; i++)
        if (rows[i] != p->k || cols[i] != p->k || regs[i] != p->k) return 0;
    return 1;
}

int main(void)
{
    /* 4 x 4 grids by hand, one star */
    sbx_puz p;
    p.n = 4, p.k = 1;
    static const char *rows[4] = {"0011", "0011", "2233", "2233"};
    for (int c = 0; c < 16; c++) p.region[c] = (uint8_t)(rows[c / 4][c % 4] - '0');
    uint8_t sol[SBX_CELLS];
    CHECK(sbx_count(&p, 10, sol) == 2);         /* four 2 x 2 regions: the two ways of four queens apart */
    CHECK(solution_ok(&p, sol) && sbx_rate(&p) == SBX_BEYOND);
    static const char *rows2[4] = {"0001", "2001", "2331", "2333"};
    for (int c = 0; c < 16; c++) p.region[c] = (uint8_t)(rows2[c / 4][c % 4] - '0');
    int k = sbx_count(&p, 10, sol);
    if (k >= 1) CHECK(solution_ok(&p, sol));
    if (k == 1) CHECK(sbx_rate(&p) != SBX_BEYOND);

    int seeds = 40;
    double total = 0;
    for (int v = 0; v < SBX_VARIANTS; v++) {
        int n = sbx_size_of(v), stars = sbx_stars_of(v), level = sbx_level_of(v), rated[4] = {0};
        double slowest = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            sbx_puz q;
            uint8_t s[SBX_CELLS], found[SBX_CELLS];
            clock_t t0 = clock();
            sbx_make(n, stars, level, (uint32_t)seed * 7919u + (uint32_t)v, &q, s);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(q.n == n && q.k == stars);
            CHECK(solution_ok(&q, s));
            for (int r = 0; r < n; r++) CHECK(connected(&q, r));
            CHECK(sbx_count(&q, 2, found) == 1);
            CHECK(!memcmp(found, s, (size_t)(n * n)));
            rated[sbx_rate(&q)]++;
        }
        printf("starbattle %dx%d/%d level %d: graded easy %d medium %d hard %d beyond %d; slowest %.3f s\n", n, n,
               stars, level, rated[0], rated[1], rated[2], rated[3], slowest);
        CHECK(rated[SBX_BEYOND] == 0);
        CHECK(rated[level] >= seeds * 9 / 10);
        for (int l = level + 1; l < 4; l++) CHECK(rated[l] == 0);
        CHECK(slowest < 0.5);
    }
    printf("starbattle: %d puzzles in %.1f s\n", seeds * SBX_VARIANTS, total);

    /* the same seed: the same puzzle */
    sbx_game a, b;
    sbx_new(&a, 4, 42);
    sbx_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, SBX_CELLS));

    /* the designed levels: each decodes (its code again the same), regions in one piece, one solution the grader finds;
     * the chapters grow (their sizes and the grader's tiers: a two-star grid's easy needs tier 1) */
    {
        int last_n = 0, weight[SBX_CHAPTERS] = {0};
        for (int i = 0; i < SBX_STAGES; i++) {
            sbx_puz p;
            uint8_t st[SBX_CELLS];
            char again[SBX_CELLS + 8];
            int ok = sbx_decode(sbx_levels[i], &p);
            CHECK(ok);
            if (!ok) continue;
            for (int r = 0; r < p.n; r++) CHECK(connected(&p, r));
            CHECK(sbx_count(&p, 2, st) == 1 && solution_ok(&p, st));
            int r = sbx_rate(&p);
            CHECK(r >= SBX_EASY && r <= SBX_HARD);
            weight[i / (SBX_STAGES / SBX_CHAPTERS)] += p.n + 2 * (r + (p.k == 2));
            CHECK(sbx_encode(&p, again, sizeof again) && !strcmp(again, sbx_levels[i]));
            if (i % (SBX_STAGES / SBX_CHAPTERS) == 0) {
                CHECK(p.n >= last_n);
                last_n = p.n;
            }
        }
        for (int c = 1; c < SBX_CHAPTERS; c++) CHECK(weight[c] > weight[c - 1]);
        static sbx_game lv;
        CHECK(sbx_open(&lv, sbx_levels[0]) && lv.p.n == 5 && !sbx_solved(&lv));
        CHECK(sbx_script(&lv, "+5") && sbx_solved(&lv));
        CHECK(!sbx_open(&lv, "4/1:aaaabbbbccccdddd") && !sbx_open(&lv, "4/1:aaaabbbbccccdddz"));
    }

    /* the player's marks: the tap's cycle, runs of dots, clashes, undo */
    sbx_game G;
    sbx_new(&G, 3, 7);                          /* 8 x 8, one star */
    int n = G.p.n;
    CHECK(sbx_tap(&G, 0) && G.mark[0] == SBX_STAR);
    CHECK(sbx_tap(&G, 0) && G.mark[0] == SBX_DOT);
    CHECK(sbx_tap(&G, 0) && G.mark[0] == SBX_NONE);
    CHECK(G.hist.moves == 3);
    CHECK(sbx_set(&G, 0, SBX_STAR) && sbx_set(&G, n + 1, SBX_STAR));
    CHECK(sbx_clash(&G, 0) && sbx_clash(&G, n + 1));    /* corner to corner */
    CHECK(sbx_set(&G, n + 1, SBX_NONE) && !sbx_clash(&G, 0));
    CHECK(sbx_set(&G, 3, SBX_STAR) && sbx_clash(&G, 3) && sbx_clash(&G, 0));   /* two in the first row */
    CHECK(sbx_unit_full(&G, 0, 0) == 0);
    CHECK(sbx_set(&G, 3, SBX_NONE) && sbx_unit_full(&G, 0, 0));
    CHECK(sbx_set(&G, 0, SBX_NONE));
    CHECK(sbx_run(&G, n, n + 5, SBX_DOT));      /* dots along row 2 */
    for (int c = n; c <= n + 5; c++) CHECK(G.mark[c] == SBX_DOT);
    int before = G.hist.moves;
    CHECK(sbx_run(&G, n + 7, n + 2, SBX_DOT));  /* from an empty cell: only the empty ones change */
    CHECK(G.mark[n + 6] == SBX_DOT && G.mark[n + 7] == SBX_DOT && G.hist.moves == before + 1);
    CHECK(sbx_run(&G, n + 2, n + 4, SBX_NONE) && G.mark[n + 3] == SBX_NONE && G.mark[n + 5] == SBX_DOT);
    CHECK(!sbx_run(&G, 0, n + 1, SBX_DOT));     /* not a line */
    CHECK(sbx_undo(&G) >= 0 && G.mark[n + 3] == SBX_DOT);
    while (sbx_undo(&G) >= 0) {}
    for (int c = 0; c < n * n; c++) CHECK(G.mark[c] == SBX_NONE);
    CHECK(G.hist.moves == 0);

    /* a hint takes a wrong mark away first, else places a star */
    int wrong = 0;
    while (G.sol[wrong]) wrong++;
    CHECK(sbx_set(&G, wrong, SBX_STAR) && sbx_hint(&G) == wrong && G.mark[wrong] == SBX_NONE);
    int h = sbx_hint(&G);
    CHECK(h >= 0 && G.sol[h] && G.mark[h] == SBX_STAR);

    /* whole games from seeds */
    for (int v = 0; v < SBX_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 6; seed++) {
            sbx_game g;
            sbx_new(&g, v, seed * 131u + (uint32_t)v);
            int m = g.p.n, guard = 0;
            uint32_t r = seed;
            while (!sbx_solved(&g) && guard++ < 200) {
                r = r * 1103515245u + 12345u;
                int c = (int)(r >> 8) % (m * m);
                switch ((r >> 4) % 5) {
                case 0: sbx_tap(&g, c); break;
                case 1: sbx_run(&g, c, c / m * m + (int)(r >> 20) % m, SBX_DOT); break;
                case 2: sbx_undo(&g); break;
                default: CHECK(sbx_hint(&g) >= 0); break;
                }
            }
            for (guard = 0; !sbx_solved(&g) && guard < 200; guard++) CHECK(sbx_hint(&g) >= 0);
            CHECK(sbx_solved(&g));
            for (int c = 0; c < m * m; c++) CHECK(!sbx_clash(&g, c));
        }

    /* the notation */
    sbx_new(&G, 0, 5);
    CHECK(sbx_script(&G, "*a1 .b1 -a1 .c2-c5") && G.mark[1] == SBX_DOT && G.mark[0] == SBX_NONE);
    CHECK(G.mark[2 * 6 + 2] == SBX_DOT && G.mark[4 * 6 + 2] == SBX_DOT);
    CHECK(!sbx_script(&G, "*z1"));
    CHECK(sbx_script(&G, "+99") && sbx_solved(&G));

    if (fails) printf("starbattle: %d checks failed\n", fails);
    else printf("starbattle: all checks passed\n");
    return fails != 0;
}
