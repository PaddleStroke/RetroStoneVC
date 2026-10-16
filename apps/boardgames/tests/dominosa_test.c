/*
 * Dominosa's rules: the solver and the grader on a puzzle by hand, the puzzles made from many seeds at every set and
 * level (one solution, counted apart from the grader; the full set, each domino once; the level they were made for),
 * the time the biggest take, the 150 designed levels (one solution each, the chapters growing), the player's
 * dominoes, clashes, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/dominosa.h"
#include "games/dominosa_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the solution pairs neighbours, every domino of the set once */
static int solution_ok(const dsa_puz *p, const uint8_t *mate)
{
    int w = p->w, cells = w * p->h, seen[DSA_TYPES] = {0};
    for (int c = 0; c < cells; c++) {
        int o = mate[c];
        if (o >= cells || mate[o] != c || o == c) return 0;
        if (!((o / w == c / w && (o - c == 1 || c - o == 1)) || o - c == w || c - o == w)) return 0;
        if (o > c && seen[dsa_type(p->n, p->num[c], p->num[o])]++) return 0;
    }
    return 1;
}

int main(void)
{
    /* the smallest set by hand: double-0 is one domino on a grid of 2 x 1 */
    dsa_puz p;
    memset(&p, 0, sizeof p);
    p.n = 0, p.w = 2, p.h = 1;
    uint8_t sol[DSA_CELLS];
    CHECK(dsa_count(&p, 10, sol) == 1 && sol[0] == 1 && sol[1] == 0);
    CHECK(dsa_rate(&p) == DSA_EASY);
    /* double-1 on 3 x 2: 0 0 1 / 0 1 1 has its three dominoes 0-0, 0-1, 1-1 laid one way only */
    p.n = 1, p.w = 3, p.h = 2;
    static const uint8_t g1[6] = {0, 0, 1, 0, 1, 1};
    memcpy(p.num, g1, 6);
    int k = dsa_count(&p, 10, sol);
    CHECK(k >= 1 && solution_ok(&p, sol));
    CHECK(dsa_type(6, 0, 0) == 0 && dsa_type(6, 6, 6) == 27 && dsa_type(6, 3, 1) == dsa_type(6, 1, 3));

    int seeds = 40;
    double total = 0;
    for (int v = 0; v < DSA_VARIANTS; v++) {
        int n = dsa_set_of(v), level = dsa_level_of(v), rated[4] = {0};
        double slowest = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            dsa_puz q;
            uint8_t s[DSA_CELLS], found[DSA_CELLS];
            clock_t t0 = clock();
            dsa_make(n, level, (uint32_t)seed * 7919u + (uint32_t)v, &q, s);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(q.n == n && q.w == n + 2 && q.h == n + 1);
            CHECK(solution_ok(&q, s));
            CHECK(dsa_count(&q, 2, found) == 1);
            CHECK(!memcmp(found, s, (size_t)(q.w * q.h)));
            rated[dsa_rate(&q)]++;
        }
        printf("dominosa double-%d level %d: graded easy %d medium %d hard %d beyond %d; slowest %.3f s\n", n, level,
               rated[0], rated[1], rated[2], rated[3], slowest);
        CHECK(rated[DSA_BEYOND] == 0);
        CHECK(rated[level] >= seeds * 9 / 10);
        for (int l = level + 1; l < 4; l++) CHECK(rated[l] == 0);
        CHECK(slowest < 0.5);
    }
    printf("dominosa: %d puzzles in %.1f s\n", seeds * DSA_VARIANTS, total);

    /* the same seed: the same puzzle */
    dsa_game a, b;
    dsa_new(&a, 4, 42);
    dsa_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, DSA_CELLS));
    /* byte for byte: nothing past the grid is left as the stack had it */
    int past = 0;
    for (int c = a.p.w * a.p.h; c < DSA_CELLS; c++) past |= a.p.num[c] | a.sol[c];
    CHECK(!past);
    /* the odd sets (the levels' double-3, 5 and 7: an odd width) are made on a true tiling too: the maker's solution
     * pairs neighbours, every domino once */
    for (int n = 3; n <= 7; n += 2)
        for (uint32_t seed = 1; seed <= 5; seed++) {
            dsa_puz q;
            uint8_t s[DSA_CELLS];
            dsa_make(n, DSA_EASY, seed, &q, s);
            CHECK(q.w == n + 2 && solution_ok(&q, s));
        }

    /* the designed levels: each decodes (its code again the same), has one solution the grader finds; the chapters
     * grow (their sets and grades) */
    {
        int last_n = 0, weight[DSA_CHAPTERS] = {0};
        for (int i = 0; i < DSA_STAGES; i++) {
            dsa_puz p;
            uint8_t mate[DSA_CELLS];
            char again[DSA_CELLS + 8];
            int ok = dsa_decode(dsa_levels[i], &p);
            CHECK(ok);
            if (!ok) continue;
            CHECK(dsa_count(&p, 2, mate) == 1 && solution_ok(&p, mate));
            int r = dsa_rate(&p);
            CHECK(r >= DSA_EASY && r <= DSA_HARD);
            weight[i / (DSA_STAGES / DSA_CHAPTERS)] += p.n + 2 * r;
            CHECK(dsa_encode(&p, again, sizeof again) && !strcmp(again, dsa_levels[i]));
            if (i % (DSA_STAGES / DSA_CHAPTERS) == 0) {
                CHECK(p.n >= last_n);
                last_n = p.n;
            }
        }
        for (int c = 1; c < DSA_CHAPTERS; c++) CHECK(weight[c] > weight[c - 1]);
        static dsa_game lv;
        CHECK(dsa_open(&lv, dsa_levels[0]) && lv.p.n == 3 && !dsa_solved(&lv));
        CHECK(dsa_script(&lv, "+10") && dsa_solved(&lv));
        CHECK(!dsa_open(&lv, "1:000111") && !dsa_open(&lv, "1:00011") && !dsa_open(&lv, "1:002111"));
    }

    /* the player's dominoes: laid over others, lifted, clashes, undo */
    dsa_game G;
    dsa_new(&G, 3, 7);                          /* double-6 */
    int w = G.p.w;
    CHECK(!dsa_lay(&G, 0, 2) && !dsa_lay(&G, w - 1, w));   /* not neighbours (the end of a row and the next) */
    CHECK(dsa_lay(&G, 0, 1) && G.link[0] == 2 && G.link[1] == 1);
    CHECK(!dsa_lay(&G, 1, 0));                  /* the same again */
    CHECK(dsa_lay(&G, 1, w + 1) && G.link[0] == 0 && G.link[1] == w + 2 && G.link[w + 1] == 2);
    CHECK(G.hist.moves == 2);
    CHECK(dsa_undo(&G) >= 0 && G.link[0] == 2 && G.link[w + 1] == 0);
    CHECK(dsa_lift(&G, 1) && G.link[0] == 0 && !dsa_lift(&G, 0));
    /* the same domino twice: both clash */
    int c0 = -1, c1 = -1;
    for (int x = 0; x + 1 < w && c1 < 0; x++)
        for (int y = 0; y < G.p.h && c1 < 0; y++) {
            int c = y * w + x, t = dsa_type(G.p.n, G.p.num[c], G.p.num[c + 1]);
            if (c0 < 0) { c0 = c; continue; }
            if (t == dsa_type(G.p.n, G.p.num[c0], G.p.num[c0 + 1]) && c != c0 && c != c0 + 1 && c + 1 != c0) c1 = c;
        }
    if (c1 >= 0) {
        CHECK(dsa_lay(&G, c0, c0 + 1) && dsa_lay(&G, c1, c1 + 1));
        CHECK(dsa_clash(&G, c0) && dsa_clash(&G, c1 + 1));
        CHECK(dsa_lift(&G, c1) && !dsa_clash(&G, c0));
    }
    while (dsa_undo(&G) >= 0) {}
    for (int c = 0; c < w * G.p.h; c++) CHECK(G.link[c] == 0);

    /* a hint takes a wrong domino away first, else lays a right one */
    int wrong = 0;
    while (G.sol[wrong] == wrong + 1) wrong++;
    CHECK(dsa_lay(&G, wrong, wrong + 1) && dsa_hint(&G) == wrong && G.link[wrong] == 0);
    int h = dsa_hint(&G);
    CHECK(h >= 0 && G.link[h] == G.sol[h] + 1);

    /* whole games from seeds */
    for (int v = 0; v < DSA_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 6; seed++) {
            dsa_game g;
            dsa_new(&g, v, seed * 131u + (uint32_t)v);
            int m = g.p.w * g.p.h, guard = 0;
            uint32_t r = seed;
            while (!dsa_solved(&g) && guard++ < 200) {
                r = r * 1103515245u + 12345u;
                int c = (int)(r >> 8) % m;
                switch ((r >> 4) % 5) {
                case 0: dsa_lay(&g, c, (r >> 20) & 1 ? c + 1 : c + g.p.w); break;
                case 1: dsa_lift(&g, c); break;
                case 2: dsa_undo(&g); break;
                default: CHECK(dsa_hint(&g) >= 0); break;
                }
            }
            for (guard = 0; !dsa_solved(&g) && guard < 200; guard++) CHECK(dsa_hint(&g) >= 0);
            CHECK(dsa_solved(&g));
            for (int c = 0; c < m; c++) CHECK(g.link[c] == g.sol[c] + 1 && !dsa_clash(&g, c));
        }

    /* the notation */
    dsa_new(&G, 0, 5);
    CHECK(dsa_script(&G, "a1-b1 xa1") && G.link[0] == 0);
    CHECK(!dsa_script(&G, "a1-c1"));
    CHECK(dsa_script(&G, "+99") && dsa_solved(&G));

    if (fails) printf("dominosa: %d checks failed\n", fails);
    else printf("dominosa: all checks passed\n");
    return fails != 0;
}
