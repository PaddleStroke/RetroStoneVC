/*
 * Tracks' rules: the solver and the search on a small hand-made puzzle (and what each level of reasoning can do), the
 * puzzles made from many seeds (the solver finds the line, the search finds no other, the size's reasoning, made
 * fast, the same from the same seed), taps, strokes, crosses, hints to the end and undo all the way back; mistakes;
 * every level (the solver lays the one line; laid cell by cell it is solved without a mistake). The test's own count
 * (sides guessed, the plain rules between) finds no other line for every puzzle and level, the big ones too.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/tracks.h"
#include "games/tracks_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a puzzle from its line: the cells in order (x, y), the entry's and the exit's sides; the ends given */
static void load(tk_game *g, int n, const int *xy, int len, int a_side, int b_side)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    int path[TK_CELLS];
    for (int i = 0; i < len; i++) path[i] = xy[2 * i] + xy[2 * i + 1] * n;
    g->a = path[0];
    g->b = path[len - 1];
    g->a_side = (uint8_t)a_side;
    g->b_side = (uint8_t)b_side;
    for (int i = 0; i < len; i++) {
        int c = path[i], v = 0;
        for (int k = -1; k <= 1; k += 2) {
            int j = i + k;
            if (j < 0) { v |= a_side; continue; }
            if (j >= len) { v |= b_side; continue; }
            int d = path[j];
            v |= d == c - n ? TK_N : d == c + 1 ? TK_E : d == c + n ? TK_S : TK_W;
        }
        g->sol[c] = (uint8_t)v;
        g->rows[c / n]++;
        g->cols[c % n]++;
    }
    g->given[g->a] = g->given[g->b] = 1;
}

static int on_line(const tk_game *g)            /* the pieces of the solution form one line from a to b */
{
    int n = g->n, c = g->a, from = g->a_side, k = 0, cells = 0;
    for (int i = 0; i < n * n; i++) cells += g->sol[i] != 0;
    while (k <= n * n) {
        k++;
        int v = g->sol[c];
        if (!(v & from) || __builtin_popcount(v) != 2) return 0;
        int out = v & ~from;
        if (c == g->b && out == g->b_side) return k == cells;
        int x = c % n + (out == TK_E) - (out == TK_W), y = c / n + (out == TK_S) - (out == TK_N);
        if (x < 0 || y < 0 || x >= n || y >= n) return 0;
        c = x + y * n;
        from = tk_opp(out);
    }
    return 0;
}

/* ---- a second count, the test's own (the rules' search gives up on the big boards): each side of each cell open,
 * closed or not known; between two guesses only the plain rules (a cell has two ends or none, a row's or a column's
 * count), every full guess checked as one line from the entry to the exit. Up to limit. */
typedef struct { uint8_t yes[TK_CELLS], no[TK_CELLS], st[TK_CELLS]; int bad; } sides_t;   /* st: 1 track, 2 none */

static const int SIDES4[4] = {TK_N, TK_E, TK_S, TK_W};

static int beside(int n, int c, int sd)
{
    int x = c % n + (sd == TK_E) - (sd == TK_W), y = c / n + (sd == TK_S) - (sd == TK_N);
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : x + y * n;
}

static int side_put(sides_t *s, int n, int c, int sd, int open)
{
    uint8_t *m = open ? s->yes : s->no, *o = open ? s->no : s->yes;
    if (m[c] & sd) return 0;
    if (o[c] & sd) return s->bad = 1, 0;
    m[c] |= (uint8_t)sd;
    int d = beside(n, c, sd);
    if (d < 0) return open ? (s->bad = 1, 0) : 1;
    if (o[d] & tk_opp(sd)) return s->bad = 1, 0;
    m[d] |= (uint8_t)tk_opp(sd);
    return 1;
}

static int st_put(sides_t *s, int c, int v)
{
    if (s->st[c] == v) return 0;
    if (s->st[c]) return s->bad = 1, 0;
    s->st[c] = (uint8_t)v;
    return 1;
}

static void plain_rules(sides_t *s, const tk_game *g)
{
    int n = g->n, ch = 1;
    while (ch && !s->bad) {
        ch = 0;
        for (int c = 0; c < n * n && !s->bad; c++) {
            int unk = 15 & ~(s->yes[c] | s->no[c]), y = __builtin_popcount(s->yes[c]), u = __builtin_popcount(unk);
            if (y > 2) { s->bad = 1; break; }
            if (y) ch |= st_put(s, c, 1);
            if (y + u < 2) ch |= st_put(s, c, 2);
            if (s->st[c] == 2 || y == 2)
                for (int i = 0; i < 4; i++) if (unk & SIDES4[i]) ch |= side_put(s, n, c, SIDES4[i], 0);
            if (s->st[c] == 1 && y + u == 2)
                for (int i = 0; i < 4; i++) if (unk & SIDES4[i]) ch |= side_put(s, n, c, SIDES4[i], 1);
        }
        for (int way = 0; way < 2 && !s->bad; way++)
            for (int i = 0; i < n; i++) {
                int t = 0, u = 0, clue = way ? g->cols[i] : g->rows[i];
                for (int j = 0; j < n; j++) {
                    int c = way ? i + j * n : j + i * n;
                    t += s->st[c] == 1, u += s->st[c] == 0;
                }
                if (t > clue || t + u < clue) { s->bad = 1; break; }
                if (u && (t == clue || t + u == clue))
                    for (int j = 0; j < n; j++) {
                        int c = way ? i + j * n : j + i * n;
                        if (!s->st[c]) ch |= st_put(s, c, t == clue ? 2 : 1);
                    }
            }
    }
}

static int count_sides(sides_t *s, const tk_game *g, int limit)
{
    int n = g->n;
    plain_rules(s, g);
    if (s->bad) return 0;
    for (int c = 0; c < n * n; c++)
        for (int i = 0; i < 4; i++) {
            if ((s->yes[c] | s->no[c]) & SIDES4[i]) continue;
            int total = 0;
            for (int open = 1; open >= 0 && total < limit; open--) {
                sides_t t = *s;
                side_put(&t, n, c, SIDES4[i], open);
                if (!t.bad) total += count_sides(&t, g, limit - total);
            }
            return total;
        }
    static tk_game line;                        /* every side known: one line through every piece? */
    line = *g;
    for (int c = 0; c < n * n; c++) line.sol[c] = s->yes[c];
    return on_line(&line);
}

static int lines_by_sides(const tk_game *g, int limit)
{
    static sides_t s;
    int n = g->n;
    memset(&s, 0, sizeof s);
    for (int c = 0; c < n * n; c++)
        for (int i = 0; i < 4; i++)
            if (beside(n, c, SIDES4[i]) < 0) {
                int stub = (c == g->a && SIDES4[i] == g->a_side) || (c == g->b && SIDES4[i] == g->b_side);
                (stub ? s.yes : s.no)[c] |= (uint8_t)SIDES4[i];
            }
    for (int c = 0; c < n * n; c++)
        if (g->given[c])
            for (int i = 0; i < 4; i++) side_put(&s, n, c, SIDES4[i], (g->sol[c] & SIDES4[i]) != 0);
    return s.bad ? 0 : count_sides(&s, g, limit);
}

int main(void)
{
    static tk_game g, h;
    uint8_t out[TK_CELLS];
    int grade;

    /* a 4 x 4 line: in at the top of a1, down, across the bottom, up the right side, out at the right of d2 */
    static const int l1[] = {0, 0, 0, 1, 0, 2, 0, 3, 1, 3, 2, 3, 3, 3, 3, 2, 3, 1};
    load(&g, 4, l1, 9, TK_N, TK_E);
    CHECK(on_line(&g));
    CHECK(g.rows[0] == 1 && g.rows[3] == 4 && g.cols[0] == 4 && g.cols[3] == 3);
    CHECK(tk_deduce(&g, 0, &grade, out) && !memcmp(out, g.sol, 16) && grade == 0);
    CHECK(tk_count(&g, 9) == 1 && lines_by_sides(&g, 9) == 1);
    /* a line where the counts alone leave two ways: the loop rule settles it, or a piece given does */
    static const int l2[] = {0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 3, 1};
    load(&g, 4, l2, 6, TK_W, TK_E);
    int k2 = tk_count(&g, 9);
    CHECK(lines_by_sides(&g, 9) == k2);
    CHECK(k2 >= 1);
    if (k2 > 1) {
        CHECK(!tk_deduce(&g, 3, NULL, NULL));   /* nothing can deduce what is not unique */
        g.given[5] = 1;
        CHECK(tk_count(&g, 9) == 1);
    }
    CHECK(tk_deduce(&g, 3, NULL, out) && !memcmp(out, g.sol, 16));

    /* the player: taps (the best fitting piece first), crosses, strokes, the counts, undo */
    load(&g, 4, l1, 9, TK_N, TK_E);
    CHECK(!tk_tap(&g, g.a) && !tk_set(&g, g.b, TK_X));   /* the given pieces */
    CHECK(tk_tap(&g, 4) && g.cell[4] == (TK_N | TK_S));  /* a2: meets a1's end above */
    CHECK(tk_tap(&g, 4) && g.cell[4] != (TK_N | TK_S) && g.cell[4] != 0);
    for (int i = 0; i < 4; i++) tk_tap(&g, 4);
    CHECK(g.cell[4] != 0 && tk_tap(&g, 4) && g.cell[4] == 0);   /* six pieces, then empty */
    CHECK(tk_set(&g, 5, TK_X) && g.cell[5] == TK_X && tk_tap(&g, 5) && g.cell[5] == 0);
    CHECK(!tk_set(&g, 5, TK_N) && !tk_set(&g, 5, TK_N | TK_E | TK_S));
    CHECK(tk_script(&g, "a1-a4-d4"));           /* a stroke from the entry: a2..d4 laid */
    CHECK(g.cell[4] == (TK_N | TK_S) && g.cell[12] == (TK_N | TK_E));
    CHECK(g.cell[15] == (TK_W | TK_N));         /* the stroke's end: turned up, the only way on */
    CHECK(tk_line_count(&g, 0, 3) == 4 && tk_line_count(&g, 1, 3) == 2);
    CHECK(!tk_solved(&g));
    CHECK(tk_hint(&g) == 11 && g.cell[11] == (TK_S | TK_N) && tk_solved(&g));
    int moves = g.moves;
    CHECK(moves == 11);
    while (tk_undo(&g) >= 0) {}
    CHECK(g.moves == 0);
    for (int c = 0; c < 16; c++) CHECK(g.cell[c] == 0);
    CHECK(!tk_script(&g, "a1-b2") && !tk_script(&g, "z1") && !tk_script(&g, "b2nq"));
    CHECK(tk_script(&g, "b2ne c3x =") && tk_solved(&g) && g.cell[10] == TK_X);

    /* the puzzles from many seeds */
    for (int v = 0; v < TK_VARIANTS; v++) {
        double worst = 0;
        int seeds = v == 2 ? 120 : 300, grades[4] = {0}, counted = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            clock_t t0 = clock();
            tk_new(&g, v, (uint32_t)seed * 6151u + (uint32_t)v);
            double ms = 1000.0 * (clock() - t0) / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            int n = g.n;
            CHECK(n == 6 + 2 * v && on_line(&g) && g.given[g.a] && g.given[g.b]);
            CHECK(g.a_side != g.b_side);
            int cells = 0;
            for (int c = 0; c < n * n; c++) cells += g.sol[c] != 0;
            CHECK(3 * cells >= n * n && 3 * cells <= 2 * n * n);
            CHECK(tk_deduce(&g, 3, &grade, out) && !memcmp(out, g.sol, (size_t)(n * n)));
            CHECK(g.grade >= 0 && g.grade <= 3);
            grades[g.grade]++;
            if (v == 0) CHECK(g.grade <= 1);
            if (v == 1) CHECK(g.grade >= 1);
            if (v < 2 || seed % 6 == 0) {       /* no other line (the search gives up on some big ones) */
                int k = tk_count(&g, 2);
                CHECK(k == 1 || (v == 2 && k < 0));
                counted += k == 1;
            }
            CHECK(lines_by_sides(&g, 2) == 1);   /* every one, by the test's own count */
            tk_new(&h, v, (uint32_t)seed * 6151u + (uint32_t)v);
            CHECK(!memcmp(h.sol, g.sol, sizeof g.sol) && !memcmp(h.given, g.given, sizeof g.given));
            /* played: some taps and crosses, the hints to the end, undo all the way back */
            for (int i = 0; i < 8; i++) {
                int c = (seed * 7 + i * 11) % (n * n);
                if (i % 3) tk_tap(&g, c);
                else tk_set(&g, c, TK_X);
            }
            int hints = 0;
            while (!tk_solved(&g) && hints < 2 * n * n && tk_hint(&g) >= 0) hints++;
            CHECK(tk_solved(&g));
            while (tk_undo(&g) >= 0) {}
            for (int c = 0; c < n * n; c++) CHECK(g.cell[c] == 0);
            CHECK(g.moves == 0);
        }
        printf("tracks %dx%d: grades %d %d %d %d, %d searched, slowest %.1f ms\n", 6 + 2 * v, 6 + 2 * v, grades[0],
               grades[1], grades[2], grades[3], counted, worst);
        CHECK(worst < 400);
    }

    /* mistakes: a piece on a cell off the line, a cross on a cell of it; once an action, kept by undo; hints count */
    {
        static tk_game g;
        tk_new(&g, 0, 17);
        int off = -1, on = -1;
        for (int c = 0; c < g.n * g.n; c++) {
            if (!g.sol[c] && off < 0) off = c;
            if (g.sol[c] && !g.given[c] && on < 0) on = c;
        }
        CHECK(off >= 0 && on >= 0);
        CHECK(tk_set(&g, off, TK_N | TK_S) && g.mistakes == 1);
        CHECK(tk_set(&g, off, TK_E | TK_W) && g.mistakes == 1);   /* still wrong: not a new one */
        CHECK(tk_set(&g, on, TK_X) && g.mistakes == 2);
        CHECK(tk_undo(&g) >= 0 && g.mistakes == 2);
        CHECK(tk_set(&g, on, g.sol[on]) && g.mistakes == 2);
        CHECK(tk_hint(&g) >= 0 && g.hints == 1 && g.mistakes == 2);
    }

    /* every level: it reads back the same, the solver lays its one line (a search finds no other on the smaller
     * ones), the first chapter needs the plainest reasoning; laid cell by cell, solved without a mistake */
    {
        static tk_game g;
        int grades[4] = {0};
        for (int i = 0; i < TK_LEVELS; i++) {
            CHECK(tk_decode(&g, tk_levels[i]));
            int n = g.n, grade;
            char code[TK_CELLS + 8];
            CHECK(tk_encode(&g, code, sizeof code) && !strcmp(code, tk_levels[i]));
            uint8_t out[TK_CELLS];
            CHECK(tk_deduce(&g, 3, &grade, out) && !memcmp(out, g.sol, (size_t)(n * n)));
            grades[grade < 0 ? 0 : grade > 3 ? 3 : grade]++;
            if (i < 25) CHECK(grade == 0 && n <= 6);
            if (n <= 7) CHECK(tk_count(&g, 2) == 1);
            CHECK(lines_by_sides(&g, 2) == 1);   /* the big ones too */
            CHECK(g.given[g.a] && g.given[g.b] && !tk_solved(&g));
            for (int c = 0; c < n * n; c++)
                if (g.sol[c] && !g.given[c]) CHECK(tk_set(&g, c, g.sol[c]));
            CHECK(tk_solved(&g) && g.mistakes == 0 && g.hints == 0);
            for (int j = 0; j < i; j++) CHECK(strcmp(tk_levels[i], tk_levels[j]));
        }
        printf("tracks levels: grades %d %d %d %d\n", grades[0], grades[1], grades[2], grades[3]);
        CHECK(!tk_decode(&g, "3:...") && !tk_decode(&g, "3:a.") && !tk_decode(&g, "x"));
    }
    CHECK(fails == 0);

    if (fails) printf("tracks: %d failures\n", fails);
    else printf("tracks: all passed\n");
    return fails != 0;
}
