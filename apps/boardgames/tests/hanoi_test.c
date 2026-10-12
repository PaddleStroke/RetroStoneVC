/*
 * The Tower of Hanoi's rules: the start, legal and illegal moves, undo, the shortest way for every size (2^n - 1
 * moves, each legal, ending solved), whole games of random legal moves from a seed, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/hanoi.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int well_stacked(const hn_game *g)
{
    int total = 0;
    for (int p = 0; p < 3; p++) {
        total += g->h[p];
        for (int i = 1; i < g->h[p]; i++)
            if (g->disc[p][i] >= g->disc[p][i - 1]) return 0;
    }
    return total == g->n;
}

/* a position as a number: disc d's peg the (d - 1)-th digit in base 3 */
static int code_of(const hn_game *g)
{
    int c = 0, pow3[HN_MAX + 1] = {1};
    for (int i = 1; i <= HN_MAX; i++) pow3[i] = 3 * pow3[i - 1];
    for (int p = 0; p < 3; p++)
        for (int i = 0; i < g->h[p]; i++) c += p * pow3[g->disc[p][i] - 1];
    return c;
}

static void from_code(hn_game *g, int n, int code, int goal)
{
    int8_t peg[HN_MAX];
    for (int d = 0; d < n; d++, code /= 3) peg[d] = (int8_t)(code % 3);
    hn_set(g, n, peg, goal);
}

/* the fewest moves from every position to the tower on `goal`, breadth first from the tower (moves go both ways) */
static int dist[2187];

static void search(int n, int goal)
{
    int states = 1, q[2187], head = 0, tail = 0;
    for (int i = 0; i < n; i++) states *= 3;
    for (int i = 0; i < states; i++) dist[i] = -1;
    int tower = 0;
    for (int d = 0, p = 1; d < n; d++, p *= 3) tower += goal * p;
    dist[tower] = 0;
    q[tail++] = tower;
    while (head < tail) {
        int c = q[head++];
        for (int f = 0; f < 3; f++)
            for (int t = 0; t < 3; t++) {
                hn_game g;
                from_code(&g, n, c, goal);
                if (!hn_move(&g, f, t)) continue;
                int k = code_of(&g);
                if (dist[k] < 0) { dist[k] = dist[c] + 1; q[tail++] = k; }
            }
    }
}

int main(void)
{
    hn_game g;
    /* the fewest moves from anywhere (3 to 7 discs, every goal) against a breadth-first search, and the hints played
     * from there: each legal, solved in exactly that many */
    for (int n = 3; n <= HN_MAX; n++)
        for (int goal = 0; goal < 3; goal++) {
            search(n, goal);
            int states = 1, ok = 1;
            for (int i = 0; i < n; i++) states *= 3;
            for (int c = 0; c < states; c++) {
                from_code(&g, n, c, goal);
                if (hn_left(&g) != dist[c] || g.par != dist[c] || hn_solved(&g) != (dist[c] == 0)) ok = 0;
                if (c % 7 && n > 5) continue;   /* the hints played from some of them */
                int k = 0, f, t;
                while (hn_hint(&g, &f, &t) && k <= dist[c]) {
                    if (!hn_move(&g, f, t)) ok = 0;
                    k++;
                }
                if (!hn_solved(&g) || k != dist[c] || !well_stacked(&g)) ok = 0;
            }
            CHECK(ok);
        }
    /* random positions: the largest disc must move, solvable by the hints, the same from the same seed */
    for (uint32_t seed = 1; seed <= 400; seed++) {
        int n = 4 + (int)(seed % HN_VARIANTS);
        hn_random(&g, n, seed);
        CHECK(g.n == n && well_stacked(&g) && g.goal >= 0 && g.par >= 1 << (n - 1) && !hn_solved(&g));
        hn_game t = g, again;
        CHECK(hn_script(&t, "=") && hn_solved(&t) && t.moves == g.par);
        hn_random(&again, n, seed);
        CHECK(!memcmp(&again, &g, sizeof g));
    }
    /* the levels: each loads, unsolved, its fewest moves kept; a chapter a number of discs (3..7), each ending with the
     * classic tower; no two alike */
    for (int l = 0; l < HN_LEVELS; l++) {
        CHECK(hn_level(&g, l));
        int ch = hn_chapter_of(l);
        CHECK(ch >= 0 && ch < HN_CHAPTERS && g.n == 3 + ch && well_stacked(&g) && g.goal >= 0);
        CHECK(!hn_solved(&g) && g.par > 0 && g.par == hn_level_par[l] && g.moves == 0);
        if (l > 0) CHECK(hn_chapter_of(l - 1) <= ch);
        if (l + 1 == HN_LEVELS || hn_chapter_of(l + 1) != ch) CHECK(g.par == hn_optimum(g.n));   /* the classic */
        hn_game t = g;
        CHECK(hn_script(&t, "=") && hn_solved(&t) && t.moves == g.par);
        for (int k = 0; k < l; k++) CHECK(strcmp(hn_levels[k], hn_levels[l]) != 0);
    }
    for (int c = 0; c < HN_CHAPTERS; c++) CHECK(hn_chapter_of(hn_chapter_start(c)) == c);
    CHECK(hn_chapter_start(HN_CHAPTERS) == HN_LEVELS);
    CHECK(!hn_level(&g, HN_LEVELS) && !hn_parse(&g, "d:abc") && !hn_parse(&g, "a:ab"));

    for (int n = 4; n <= 7; n++) {
        hn_new(&g, n);
        CHECK(g.h[0] == n && g.h[1] == 0 && g.h[2] == 0 && hn_top(&g, 0) == 1 && g.disc[0][0] == n);
        CHECK(!hn_solved(&g) && hn_optimum(n) == (1 << n) - 1);
    }

    hn_new(&g, 4);
    CHECK(!hn_move(&g, 1, 2) && !hn_move(&g, 0, 0));   /* an empty peg; the same peg */
    CHECK(hn_move(&g, 0, 1) && hn_top(&g, 1) == 1 && g.moves == 1);
    CHECK(!hn_can(&g, 0, 1));                   /* 2 on 1: never */
    CHECK(hn_move(&g, 0, 2) && hn_top(&g, 2) == 2);
    CHECK(hn_move(&g, 1, 2) && hn_top(&g, 2) == 1 && g.h[2] == 2);
    CHECK(hn_undo(&g) && hn_top(&g, 1) == 1 && g.moves == 2);
    CHECK(hn_undo(&g) && hn_undo(&g) && g.moves == 0 && g.h[0] == 4 && !hn_undo(&g));

    /* the shortest way: every move legal, solved after exactly 2^n - 1, to the right peg or the middle one */
    for (int n = 4; n <= 7; n++)
        for (int target = 1; target <= 2; target++) {
            hn_new(&g, n);
            int ok = 1;
            for (int k = 0; k < hn_optimum(n); k++) {
                int f, t;
                hn_way(n, 0, target, k, &f, &t);
                if (hn_solved(&g) || !hn_move(&g, f, t)) ok = 0;
            }
            CHECK(ok && hn_solved(&g) && g.h[target] == n && g.moves == hn_optimum(n));
        }

    /* random legal moves from seeds: the discs always well stacked; undone back to the start */
    for (uint32_t seed = 1; seed <= 300; seed++) {
        uint32_t s = pr_seed(seed);
        int n = 4 + seed % 4;
        hn_new(&g, n);
        int ok = 1, made = 0;
        for (int k = 0; k < 500; k++) {
            int f = pr_below(&s, 3), t = pr_below(&s, 3);
            int can = hn_can(&g, f, t);
            if (hn_move(&g, f, t) != can) ok = 0;
            made += can;
            if (!well_stacked(&g)) ok = 0;
        }
        CHECK(ok && g.moves == made);
        while (hn_undo(&g)) {}
        CHECK(g.h[0] == n && g.moves == 0);
    }

    hn_new(&g, 5);
    CHECK(hn_script(&g, "ac ab cb") && hn_top(&g, 1) == 1 && g.h[1] == 2 && g.moves == 3);
    CHECK(!hn_script(&g, "ab"));                /* 3 on 1 */
    hn_new(&g, 6);
    CHECK(hn_script(&g, "=62") && !hn_solved(&g) && hn_script(&g, "=1") && hn_solved(&g) && g.h[2] == 6);
    CHECK(!hn_script(&g, "=1"));

    if (fails) {
        printf("hanoi: %d failed\n", fails);
        return 1;
    }
    printf("hanoi: all passed\n");
    return 0;
}
