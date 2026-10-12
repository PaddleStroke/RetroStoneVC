/*
 * Futoshiki's rules: the solver and the grader on a known puzzle, the puzzles made from many seeds at every size and
 * level (one solution, the level they were made for, their clues the solution's), the time the biggest take, the
 * player's digits, notes, clashes, hints and undo, and whole games played from a seed with hints and wrong digits.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/futoshiki.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int latin_ok(int n, const uint8_t *g)
{
    for (int i = 0; i < n; i++) {
        unsigned r = 0, c = 0;
        for (int j = 0; j < n; j++) {
            if (g[i * n + j] < 1 || g[i * n + j] > n) return 0;
            r |= 1u << g[i * n + j];
            c |= 1u << g[j * n + i];
        }
        if (r != ((1u << n) - 1) << 1 || c != r) return 0;
    }
    return 1;
}

static int signs_hold(const fsk_puz *p, const uint8_t *g)
{
    int n = p->n;
    for (int c = 0; c < n * n; c++)
        for (int way = 0; way < 2; way++) {
            int r = p->rel[way][c];
            if (!r) continue;
            if (way ? c / n >= n - 1 : c % n >= n - 1) return 0;   /* a sign with no neighbour */
            int o = c + (way ? n : 1);
            if ((r == FSK_LT) != (g[c] < g[o])) return 0;
        }
    return 1;
}

int main(void)
{
    /* a small puzzle by hand: 4 x 4, the signs along the first row 1 < 2 < 3 < 4 and down the first column */
    fsk_puz p;
    memset(&p, 0, sizeof p);
    p.n = 4;
    for (int i = 0; i < 3; i++) p.rel[0][i] = FSK_LT, p.rel[1][i * 4] = FSK_LT;
    uint8_t sol[FSK_CELLS];
    CHECK(fsk_count(&p, 100, sol) >= 2);        /* not enough: several squares */
    p.g[5] = 3, p.g[10] = 1;                    /* b2 = 3, c3 = 1 */
    int k = fsk_count(&p, 100, sol);
    CHECK(k >= 1);
    if (k == 1) CHECK(latin_ok(4, sol) && signs_hold(&p, sol) && sol[0] == 1 && sol[3] == 4 && sol[12] == 4);
    p.g[1] = 1;                                 /* a clash with the first row's 1 < 2: none */
    CHECK(fsk_count(&p, 2, NULL) == 0);
    CHECK(fsk_rate(&p) == FSK_BEYOND);

    /* the puzzles from many seeds: one solution, of their level, the clues the solution's; the time they take */
    int rated[FSK_VARIANTS][4] = {{0}}, clues[FSK_VARIANTS][2], signs[FSK_VARIANTS][2];
    double slowest[FSK_VARIANTS] = {0}, total = 0;
    int seeds = 60;
    for (int v = 0; v < FSK_VARIANTS; v++) {
        int n = fsk_size_of(v), level = fsk_level_of(v);
        clues[v][0] = signs[v][0] = 999, clues[v][1] = signs[v][1] = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            fsk_puz q;
            uint8_t s[FSK_CELLS], found[FSK_CELLS];
            clock_t t0 = clock();
            fsk_make(n, level, (uint32_t)seed * 7919u + (uint32_t)v, &q, s);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(q.n == n);
            CHECK(latin_ok(n, s) && signs_hold(&q, s));
            CHECK(fsk_count(&q, 2, found) == 1);
            CHECK(!memcmp(found, s, (size_t)n * n));
            int nc = 0, ns = 0;
            for (int c = 0; c < n * n; c++) {
                if (q.g[c]) { nc++; CHECK(q.g[c] == s[c]); }
                ns += (q.rel[0][c] != 0) + (q.rel[1][c] != 0);
            }
            int r = fsk_rate(&q);
            rated[v][r]++;
            if (nc < clues[v][0]) clues[v][0] = nc;
            if (nc > clues[v][1]) clues[v][1] = nc;
            if (ns < signs[v][0]) signs[v][0] = ns;
            if (ns > signs[v][1]) signs[v][1] = ns;
        }
        printf("futoshiki %dx%d level %d: graded easy %d medium %d hard %d beyond %d; digits %d-%d, signs %d-%d; "
               "slowest %.3f s\n", n, n, level, rated[v][0], rated[v][1], rated[v][2], rated[v][3], clues[v][0],
               clues[v][1], signs[v][0], signs[v][1], slowest[v]);
        CHECK(rated[v][FSK_BEYOND] == 0);
        CHECK(rated[v][level] >= seeds * 9 / 10);   /* nearly always its own level (else the nearest below) */
        for (int l = level + 1; l < 4; l++) CHECK(rated[v][l] == 0);
        CHECK(slowest[v] < 0.5);
    }
    printf("futoshiki: %d puzzles in %.1f s\n", seeds * FSK_VARIANTS, total);

    /* the same seed: the same puzzle */
    fsk_game a, b;
    fsk_new(&a, 7, 42);
    fsk_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, FSK_CELLS));

    /* the player's grid: digits, the notes they clear, clashes, undo */
    fsk_game G;
    fsk_new(&G, 6, 7);                          /* 6 x 6 easy */
    int n = G.n, e = -1, given = -1;
    for (int c = 0; c < n * n; c++) {
        if (!G.p.g[c] && e < 0) e = c;
        if (G.p.g[c] && given < 0) given = c;
    }
    if (given >= 0) CHECK(!fsk_put(&G, given, 1) && !fsk_note(&G, given, 1) && !fsk_erase(&G, given));
    CHECK(!fsk_put(&G, e, n + 1) && !fsk_note(&G, e, 0));
    int peer = e % n == n - 1 ? e - 1 : e + 1;
    while (G.p.g[peer]) peer = (peer + n) % (n * n);
    CHECK(fsk_note(&G, peer, 2) && fsk_note(&G, peer, 3) && G.notes[peer] == (1u << 2 | 1u << 3));
    CHECK(fsk_note(&G, e, 1) && G.notes[e] == 1u << 1);
    CHECK(G.hist.moves == 3);
    int same_line = peer / n == e / n || peer % n == e % n;
    CHECK(fsk_put(&G, e, 2) && G.val[e] == 2 && G.notes[e] == 0);
    if (same_line) CHECK(G.notes[peer] == 1u << 3);
    CHECK(!fsk_put(&G, e, 2));
    CHECK(G.hist.moves == 4);
    CHECK(fsk_undo(&G) == e && G.val[e] == 0 && G.notes[e] == 1u << 1 && G.notes[peer] == (1u << 2 | 1u << 3));
    CHECK(G.hist.moves == 3);
    CHECK(fsk_put(&G, e, G.sol[e]) && !fsk_clash(&G, e));
    for (int j = 0; j < n; j++) {               /* the same digit elsewhere in its row: a clash */
        int x = e / n * n + j;
        if (x != e && !G.p.g[x] && !G.val[x]) {
            CHECK(fsk_put(&G, x, G.sol[e]) && fsk_clash(&G, x) && fsk_clash(&G, e));
            CHECK(fsk_erase(&G, x) && !G.val[x] && !fsk_clash(&G, e));
            break;
        }
    }
    /* a sign broken: a clash on both sides */
    for (int c = 0; c < n * n; c++) {
        if (c % n == n - 1 || !G.p.rel[0][c] || G.p.g[c] || G.p.g[c + 1]) continue;
        fsk_game T = G;
        int lo = G.p.rel[0][c] == FSK_LT;
        CHECK(fsk_put(&T, c, lo ? n : 1) && fsk_put(&T, c + 1, lo ? 1 : n));
        CHECK(fsk_clash(&T, c) && fsk_clash(&T, c + 1));
        break;
    }
    CHECK(fsk_erase(&G, e) && G.val[e] == 0);

    /* a hint puts a wrong digit right first, else fills a cell */
    fsk_new(&G, 11, 99);                        /* 7 x 7 hard */
    int w = -1;
    for (int c = 0; c < G.n * G.n && w < 0; c++)
        if (!G.p.g[c]) w = c;
    CHECK(fsk_put(&G, w, G.sol[w] % G.n + 1));
    CHECK(fsk_hint(&G) == w && G.val[w] == G.sol[w]);
    int before = 0, after = 0;
    for (int c = 0; c < G.n * G.n; c++) before += G.val[c] != 0;
    int h = fsk_hint(&G);
    for (int c = 0; c < G.n * G.n; c++) after += G.val[c] != 0;
    CHECK(h >= 0 && after == before + 1 && G.val[h] == G.sol[h]);

    /* whole games from seeds: wrong digits, notes, hints, undo; solved at the end; undo all the way back */
    for (int v = 0; v < FSK_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 8; seed++) {
            fsk_game g;
            fsk_new(&g, v, seed * 131u + (uint32_t)v);
            int m = g.n, guard = 0;
            uint32_t r = seed;
            while (!fsk_solved(&g) && guard++ < 2000) {
                r = r * 1103515245u + 12345u;
                int c = (int)(r >> 8) % (m * m), d = (int)(r >> 20) % m + 1;
                switch ((r >> 4) % 6) {
                case 0: fsk_note(&g, c, d); break;
                case 1: fsk_put(&g, c, d); break;     /* maybe wrong */
                case 2: fsk_erase(&g, c); break;
                case 3: fsk_undo(&g); break;
                default: CHECK(fsk_hint(&g) >= 0); break;
                }
            }
            CHECK(fsk_solved(&g));
            for (int c = 0; c < m * m; c++) CHECK(!fsk_clash(&g, c));
            while (fsk_undo(&g) >= 0) {}
            CHECK(g.hist.moves == 0 || g.hist.n == 0);
            CHECK(!memcmp(g.val, g.p.g, FSK_CELLS) || g.hist.moves != 0);
        }

    /* the notation */
    fsk_new(&G, 0, 5);
    char moves[64];
    int c0 = -1;
    for (int c = 0; c < 16 && c0 < 0; c++)
        if (!G.p.g[c]) c0 = c;
    snprintf(moves, sizeof moves, "%c%d'1 %c%d=%d", 'a' + c0 % 4, c0 / 4 + 1, 'a' + c0 % 4, c0 / 4 + 1, G.sol[c0]);
    CHECK(fsk_script(&G, moves) && G.val[c0] == G.sol[c0]);
    CHECK(!fsk_script(&G, "z9=1"));
    CHECK(fsk_script(&G, "+99") && fsk_solved(&G));

    if (fails) printf("futoshiki: %d checks failed\n", fails);
    else printf("futoshiki: all checks passed\n");
    return fails != 0;
}
