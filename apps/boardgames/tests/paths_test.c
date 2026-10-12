/*
 * Paths' rules: the solver on known grids (one solution, two, none), the puzzles from many seeds (the grid filled by
 * paths that touch themselves nowhere, as many pairs as the size wants, one solution only, the same from the same
 * seed), the maker's time on every size, the strokes (on free squares, cutting another path and giving it back,
 * backing off, never onto another colour's dot, stopping at its own), hints, undo, mistakes, whole games from seeds,
 * and every level (one solution, found by the deductions alone; solved stroke by stroke without a mistake).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/paths.h"
#include "games/paths_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a grid drawn in letters ('a' the first colour..., '.' nothing), each letter on exactly two squares: its dots */
static int dots_of(int n, const char *rows, uint8_t (*end)[2])
{
    int k[NK_COLOURS] = {0}, pairs = 0;
    for (int i = 0; i < n * n; i++) {
        if (rows[i] == '.') continue;
        int c = rows[i] - 'a';
        end[c][k[c]++] = (uint8_t)i;
        if (c + 1 > pairs) pairs = c + 1;
    }
    return pairs;
}

/* a stroke along the solution's path of colour c, every square named: "a1-a2-b2..." */
static void stroke_of(const nk_game *g, int c, char *out)
{
    int k = 0;
    for (int i = 0; i < g->slen[c]; i++) {
        int s = g->spath[c][i];
        k += sprintf(out + k, "%s%c%d", i ? "-" : "", 'a' + s % g->n, s / g->n + 1);
    }
    out[k++] = ' ';
    out[k] = 0;
}

int main(void)
{
    uint8_t end[NK_COLOURS][2];
    /* the solver: a 3 x 3 with one solution; one with two; one with none */
    int p = dots_of(3, "a.a" "b.b" "c.c", end);
    CHECK(nk_count(3, p, (const uint8_t (*)[2])end, 5, 100000) == 1);
    p = dots_of(3, "a.." "..." "..a", end);
    CHECK(nk_count(3, p, (const uint8_t (*)[2])end, 5, 100000) == 2);   /* corner to corner: the two snakes */
    p = dots_of(3, "a.." "a.." "...", end);
    CHECK(nk_count(3, p, (const uint8_t (*)[2])end, 5, 100000) == 0);   /* 9 squares between neighbours: none */
    p = dots_of(4, "a..a" "...." "...." "b..b", end);
    CHECK(nk_count(4, p, (const uint8_t (*)[2])end, 5, 100000) >= 2);
    p = dots_of(2, "ab" "ab", end);
    CHECK(nk_count(2, p, (const uint8_t (*)[2])end, 5, 100000) == 1);
    p = dots_of(2, "ab" "ba", end);
    CHECK(nk_count(2, p, (const uint8_t (*)[2])end, 5, 100000) == 0);
    for (int n = 5; n <= NK_MAX; n++) {        /* the maker's last resort, each row a pair: the deductions alone */
        for (int y = 0; y < n; y++) end[y][0] = (uint8_t)(y * n), end[y][1] = (uint8_t)(y * n + n - 1);
        CHECK(nk_count(n, n, (const uint8_t (*)[2])end, 2, 1) == 1);
    }

    /* the puzzles from many seeds */
    for (int v = 0; v < NK_VARIANTS; v++) {
        int n, lo, hi, seeds = v == 3 ? 40 : v == 2 ? 120 : 200, tries = 0, worst_tries = 0, pairs_sum = 0;
        nk_variant(v, &n, &lo, &hi);
        clock_t c0 = clock();
        int worst_ms = 0;
        for (uint32_t seed = 1; seed <= (uint32_t)seeds; seed++) {
            uint8_t colour[NK_CELLS], colour2[NK_CELLS], end2[NK_COLOURS][2];
            int pairs, pairs2;
            clock_t a = clock();
            int t = nk_make(v, seed * 2654435761u, colour, end, &pairs);
            int ms = (int)((clock() - a) * 1000 / CLOCKS_PER_SEC);
            if (ms > worst_ms) worst_ms = ms;
            tries += t;
            if (t > worst_tries) worst_tries = t;
            pairs_sum += pairs;
            if (seed <= 20) {
                nk_make(v, seed * 2654435761u, colour2, end2, &pairs2);
                CHECK(pairs == pairs2 && !memcmp(colour, colour2, (size_t)n * n) &&
                      !memcmp(end, end2, sizeof end[0] * pairs));
            }
            CHECK(pairs >= lo && pairs <= hi);
            CHECK(nk_count(n, pairs, (const uint8_t (*)[2])end, 2, 1) == 1);   /* by the deductions alone */
            if (seed <= 40) CHECK(nk_count(n, pairs, (const uint8_t (*)[2])end, 9, 50000000L) == 1);   /* in full */
            /* each colour: a path from its first dot to its second, touching itself nowhere, the grid full */
            nk_game *g = malloc(sizeof *g);
            nk_load(g, n, pairs, colour, (const uint8_t (*)[2])end);
            int covered = 0, ok = 1;
            for (int c = 0; c < pairs; c++) {
                int L = g->slen[c];
                ok &= L >= 3 && g->spath[c][0] == end[c][0] && g->spath[c][L - 1] == end[c][1];
                covered += L;
                for (int i = 0; i < L; i++)
                    for (int j = i + 2; j < L; j++) {
                        int a = g->spath[c][i], b = g->spath[c][j], dx = a % n - b % n, dy = a / n - b / n;
                        ok &= dx * dx + dy * dy != 1;
                    }
            }
            CHECK(ok && covered == n * n);
            free(g);
        }
        float avg = (clock() - c0) * 1000.0f / CLOCKS_PER_SEC / seeds;
        printf("paths %dx%d: %.1f pairs, %.1f tries a puzzle (%d at worst), %.1f ms a puzzle (%d ms at worst, with "
               "the checks)\n", n, n, pairs_sum / (float)seeds, tries / (float)seeds, worst_tries, avg, worst_ms);
        CHECK(worst_ms < 400);
    }

    /* the strokes, on a known puzzle:
     *   a a a b      a: a1-c1 (along the top), b: d1-d4, c: a2-a4-c4, d: b2-c2-c3-b3
     *   c d d b
     *   c d d b
     *   c c c b                                                                                                   */
    static nk_game G;
    nk_game *g = &G;
    const char *sol = "aaab" "cddb" "cddb" "cccb";
    uint8_t colour[16];
    for (int i = 0; i < 16; i++) colour[i] = (uint8_t)(sol[i] - 'a');
    uint8_t e4[4][2] = {{0, 2}, {3, 15}, {4, 14}, {5, 9}};
    nk_load(g, 4, 4, colour, (const uint8_t (*)[2])e4);
    CHECK(g->slen[0] == 3 && g->slen[1] == 4 && g->slen[2] == 5 && g->slen[3] == 4);
    CHECK(nk_free(g) == 8 && !nk_solved(g));
    CHECK(nk_script(g, "a1-c1") && nk_linked(g, 0) && nk_right(g, 0) && g->moves == 1);
    CHECK(nk_begin(g, 1) == 0 && g->len[0] == 2);   /* begun on its square b1: cut back there */
    CHECK(nk_toward(g, 2) == 1 && nk_linked(g, 0) && nk_toward(g, 3) == 0);   /* linked: no further */
    CHECK(!nk_end(g) && g->moves == 1);         /* as it was: no action */
    CHECK(nk_begin(g, 2) == 0 && g->len[0] == 1 && nk_end(g) && g->moves == 2 && g->len[0] == 0);   /* its dot */
    CHECK(nk_undo(g) && nk_linked(g, 0) && g->moves == 1);
    CHECK(nk_begin(g, 7) == -1);                /* a free square: no stroke */
    CHECK(nk_script(g, "a2-a4-c4") && nk_linked(g, 2) && g->moves == 2);
    /* d drawn from its dot b3 across c's path: c cut at b4, and back as it was when d backs off */
    CHECK(nk_begin(g, 9) == 3 && nk_toward(g, 13) == 1 && nk_head(g, 3) == 13);
    CHECK(g->len[2] == 3 && !nk_linked(g, 2) && g->own[13] == 4);
    CHECK(nk_toward(g, 9) == 1 && g->len[2] == 5 && nk_linked(g, 2));
    CHECK(!nk_end(g) && g->moves == 2 && g->len[3] == 0);   /* nothing changed */
    /* the stroke begins on a path's square: cut back there */
    CHECK(nk_begin(g, 12) == 2 && g->len[2] == 3 && nk_end(g) && g->moves == 3);
    CHECK(nk_undo(g) && g->len[2] == 5 && g->moves == 2);
    /* a stroke from the other dot starts the path again from it */
    CHECK(nk_begin(g, 9) == 3 && g->len[3] == 1 && g->path[3][0] == 9);
    CHECK(nk_toward(g, 10) == 1 && nk_toward(g, 6) == 1 && nk_toward(g, 5) == 1 && nk_linked(g, 3));
    CHECK(nk_end(g) && nk_right(g, 3));
    CHECK(nk_script(g, "d1-d4") && nk_solved(g) && g->moves == 4);
    while (nk_undo(g)) {}
    CHECK(g->moves == 0 && nk_free(g) == 8);
    CHECK(!nk_script(g, "b2-b5") && !nk_script(g, "e1") && !nk_script(g, "a1-b2"));   /* off the grid; a diagonal */
    while (nk_undo(g)) {}
    /* hints: each lays one pair, the shortest first; cut what lay in the way */
    CHECK(nk_script(g, "b2-b1"));                /* d drawn wrong, over a's square */
    CHECK(nk_hint(g) == 0 && nk_right(g, 0) && g->len[3] <= 1);
    CHECK(nk_hint(g) >= 0 && nk_hint(g) >= 0 && nk_hint(g) >= 0 && nk_solved(g) && nk_hint(g) < 0);

    /* whole games from seeds: random strokes, then the solution's strokes in a random order (solved by the last one
     * only), undo all the way back, then hints to the end */
    for (uint32_t seed = 1; seed <= 20; seed++)
        for (int v = 0; v < NK_VARIANTS; v++) {
            nk_new(g, v, seed);
            int nc = g->n * g->n;
            uint32_t r = pr_seed(seed * 31 + (uint32_t)v);
            for (int i = 0; i < 25; i++) {
                if (nk_begin(g, pr_below(&r, nc)) < 0) continue;
                for (int k = 0; k < 4; k++) nk_toward(g, pr_below(&r, nc));
                nk_end(g);
            }
            int order[NK_COLOURS], early = 0;
            for (int c = 0; c < g->pairs; c++) order[c] = c;
            pr_shuffle(&r, order, g->pairs);
            for (int i = 0; i < g->pairs; i++) {
                char s[NK_CELLS * 5];
                int right = 0;                  /* solved exactly when every pair is right */
                for (int c = 0; c < g->pairs; c++) right += nk_right(g, c);
                early |= nk_solved(g) != (right == g->pairs);
                stroke_of(g, order[i], s);
                CHECK(nk_script(g, s) && nk_right(g, order[i]));
            }
            CHECK(!early && nk_solved(g));
            for (int c = 0; c < g->pairs; c++) CHECK(nk_right(g, c));
            while (nk_undo(g)) {}
            CHECK(g->moves == 0 && nk_free(g) == nc - 2 * g->pairs);
            int hints = 0;
            while (nk_hint(g) >= 0) hints++;
            CHECK(nk_solved(g) && hints == g->pairs && g->moves == hints);
        }
    /* mistakes: a stroke over a square of another colour in the solution, once a stroke; undo keeps the count */
    nk_new(g, 1, 5);
    {
        int c = 0, a = g->end[0][0], wrong = -1, n = g->n;
        for (int d = 0; d < 4 && wrong < 0; d++) {   /* a square beside a's dot, not a's colour, not a dot */
            int x = a % n + (d == 1) - (d == 3), y = a / n + (d == 2) - (d == 0), b = y * n + x;
            if (x >= 0 && y >= 0 && x < n && y < n && g->scol[b] != c && !g->dot[b]) wrong = b;
        }
        if (wrong >= 0) {
            CHECK(nk_begin(g, a) == c && nk_toward(g, wrong) == 1 && nk_end(g));
            CHECK(g->mistakes == 1);
            CHECK(nk_undo(g) && g->mistakes == 1);
        }
        char s[NK_CELLS * 5];
        stroke_of(g, 0, s);
        CHECK(nk_script(g, s) && g->mistakes == (wrong >= 0));
        int h = nk_hint(g);
        CHECK(h >= 0 && g->hints == 1 && g->mistakes == (wrong >= 0));
    }

    /* every level: it reads back the same, has one solution that the deductions alone find, the first chapter's are
     * small; drawn stroke by stroke from the solution it is solved without a mistake; no two alike */
    {
        int grades[3] = {0};
        for (int i = 0; i < NK_LEVELS; i++) {
            CHECK(nk_decode(g, nk_levels[i]));
            int rounds, grade = nk_grade(g->n, g->pairs, (const uint8_t (*)[2])g->end, &rounds);
            CHECK(grade >= 0);
            if (grade >= 0) grades[grade]++;
            CHECK(nk_count(g->n, g->pairs, (const uint8_t (*)[2])g->end, 2, 1000000) == 1);
            if (i < 25) CHECK(g->n <= 6);
            char code[NK_CELLS + 8];
            CHECK(nk_encode(g->n, g->scol, (const uint8_t (*)[2])g->end, g->pairs, code, sizeof code) &&
                  !strcmp(code, nk_levels[i]));
            for (int c = 0; c < g->pairs; c++) {
                char s[NK_CELLS * 5];
                CHECK(!nk_solved(g));
                stroke_of(g, c, s);
                CHECK(nk_script(g, s));
            }
            CHECK(nk_solved(g) && g->mistakes == 0 && g->moves == g->pairs);
            for (int j = 0; j < i; j++) CHECK(strcmp(nk_levels[i], nk_levels[j]));
        }
        printf("paths levels: grades %d %d %d\n", grades[0], grades[1], grades[2]);
    }
    CHECK(!nk_decode(g, "3:Aaa") && !nk_decode(g, "3:AaAbbbBcC") && !nk_decode(g, "x"));

    nk_new(g, 3, 9);
    CHECK(nk_cell(g, "l12", NULL) == 143 && nk_cell(g, "a10", NULL) == 108 && nk_cell(g, "m1", NULL) < 0 &&
          nk_cell(g, "a13", NULL) < 0);

    if (fails) {
        printf("paths: %d failed\n", fails);
        return 1;
    }
    printf("paths: all passed\n");
    return 0;
}
