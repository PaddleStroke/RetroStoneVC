/*
 * Paper Fold's rules: a fold lays one side over the other turned over, a fold that lands off the board or has paper on
 * one side only refused, piles of layers, holes, undo; sheets and pictures as text both ways; every level's par found
 * again by a search written here (its own paper: every layer of every pile kept, each turned over or not), its
 * shortest way played at par, the chapters' features, no two levels alike; the puzzles made at random (their par, the
 * same from the same seed, the time they take); whole games from a seed (random folds, a hint, the shortest way to the
 * end, undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/paperfold.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own paper: each square a pile of layers from the bottom, each layer a square of the sheet, turned
 * over or not; a fold takes the piles on one side, turns each upside down onto its mirrored square ---------------- */
#define DEEP 40
typedef struct { unsigned char n[PFO_CELLS], src[PFO_CELLS][DEEP], over[PFO_CELLS][DEEP]; } paper;

static int own_colour(const pfo_map *m, const paper *p, int c)   /* what square c shows on top, -1 nothing */
{
    if (!p->n[c]) return -1;
    int k = p->n[c] - 1, v = m->start.cell[p->src[c][k]];
    return p->over[c][k] ? v & 15 : v >> 4;
}

static int own_fold(const pfo_map *m, const paper *p, int axis, int line, int side, paper *out)
{
    *out = *p;
    int flap = 0, base = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (!p->n[c]) continue;
        int pos = axis ? c / m->w : c % m->w, in = side ? pos >= line : pos < line;
        if (!in) { base++; continue; }
        flap++;
        int x = c % m->w, y = c / m->w;
        if (axis) y = 2 * line - 1 - y;
        else x = 2 * line - 1 - x;
        if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    }
    if (!flap || !base) return 0;
    for (int c = 0; c < m->w * m->h; c++) {
        int pos = axis ? c / m->w : c % m->w, in = side ? pos >= line : pos < line;
        if (in) out->n[c] = 0;
    }
    for (int c = 0; c < m->w * m->h; c++) {
        int pos = axis ? c / m->w : c % m->w, in = side ? pos >= line : pos < line;
        if (!in || !p->n[c]) continue;
        int x = c % m->w, y = c / m->w;
        if (axis) y = 2 * line - 1 - y;
        else x = 2 * line - 1 - x;
        int t = x + y * m->w;
        for (int k = p->n[c] - 1; k >= 0; k--) {   /* the pile upside down: its top layer now at the bottom */
            int at = out->n[t]++;
            if (at >= DEEP) return 0;
            out->src[t][at] = p->src[c][k];
            out->over[t][at] = (unsigned char)!p->over[c][k];
        }
    }
    return 1;
}

static int own_done(const pfo_map *m, const paper *p)
{
    for (int c = 0; c < m->w * m->h; c++) {
        int v = own_colour(m, p, c), t = m->target[c] == PFO_NONE ? -1 : m->target[c];
        if (v != t) return 0;
    }
    return 1;
}

static int own_dfs(const pfo_map *m, const paper *p, int left)   /* solved within `left` folds */
{
    if (own_done(m, p)) return 1;
    if (!left) return 0;
    for (int axis = 0; axis < 2; axis++)
        for (int line = 1; line < (axis ? m->h : m->w); line++)
            for (int side = 0; side < 2; side++) {
                paper n;
                if (own_fold(m, p, axis, line, side, &n) && own_dfs(m, &n, left - 1)) return 1;
            }
    return 0;
}

static int own_par(const pfo_map *m, int most)
{
    static paper p;
    memset(&p, 0, sizeof p);
    for (int c = 0; c < m->w * m->h; c++)
        if (m->start.cell[c] != PFO_NONE) { p.n[c] = 1; p.src[c][0] = (unsigned char)c; }
    for (int d = 0; d <= most; d++)
        if (own_dfs(m, &p, d)) return d;
    return -1;
}

static int count_holes(const char *s)
{
    int n = 0;
    for (; s[0] && s[1]; s++) {
        if (*s == '|') continue;
        n += s[0] == '.';
        s++;
    }
    return n;
}

static int two_sided(const char *s, int back)   /* the front (back 0) or the back has two colours or more */
{
    char first = 0;
    for (; s[0] && s[1]; s++) {
        if (*s == '|') continue;
        if (s[0] != '.') {
            char c = s[back];
            if (!first) first = c;
            else if (c != first) return 1;
        }
        s++;
    }
    return 0;
}

int main(void)
{
    static pfo_game g, t, first;
    char sheet[PFO_TEXT], pic[PFO_TEXT];

    /* a fold: the left column over crease 1 turned over onto the middle; front a, back b */
    CHECK(pfo_parse(&g.m, "ababab|ababab", "...|...") && g.m.w == 3 && g.m.h == 2);
    pfo_begin(&g);
    CHECK(!pfo_try(&g.m, &g.s, PFO_FOLD(0, 1, 1), NULL));   /* the right part (two columns) over: off the board */
    CHECK(!pfo_try(&g.m, &g.s, PFO_FOLD(0, 3, 0), NULL) && !pfo_try(&g.m, &g.s, PFO_FOLD(1, 2, 0), NULL));
    CHECK(pfo_move(&g, PFO_FOLD(0, 1, 0)));
    CHECK(g.s.cell[0] == PFO_NONE && g.s.cell[1] == 1 * 16 + 1 && g.s.layers[1] == 2 && g.s.cell[2] == 0 * 16 + 1);
    /* again: the middle pile (b on top) over onto the right square: it shows its underneath, b */
    CHECK(pfo_move(&g, PFO_FOLD(0, 2, 0)) && g.s.cell[2] == 1 * 16 + 1 && g.s.layers[2] == 3);
    CHECK(!pfo_try(&g.m, &g.s, PFO_FOLD(0, 1, 0), NULL));   /* nothing left of crease 1 */
    CHECK(pfo_undo(&g) && pfo_undo(&g) && !pfo_undo(&g) && !memcmp(&g.s, &g.m.start, sizeof g.s) && !g.moves);
    /* the target and the notation */
    CHECK(pfo_parse(&g.m, "abab|abab", "b.|b."));
    pfo_begin(&g);
    CHECK(pfo_script(&g, "R1") && pfo_solved(&g.m, &g.s) && g.moves == 1);
    CHECK(pfo_parse(&g.m, "abab|abab", "..|.b"));
    pfo_begin(&g);
    CHECK(pfo_solve(&g.m, &g.s, NULL, NULL) == 2 && pfo_script(&g, "=") && g.moves == 2);
    pfo_text(&g.m, sheet, pic);
    CHECK(!strcmp(sheet, "abab|abab") && !strcmp(pic, "..|.b"));
    /* holes: a cut square takes nothing; a flap over a hole shows its back alone */
    CHECK(pfo_parse(&g.m, "ab..|abab", ".b|.b"));
    pfo_begin(&g);
    CHECK(pfo_script(&g, "L1") && pfo_solved(&g.m, &g.s));
    CHECK(!pfo_parse(&g.m, "ab.", "..") && !pfo_parse(&g.m, "abab|ab", "..|..") && !pfo_parse(&g.m, "ax", ".") &&
          !pfo_parse(&g.m, "....", "..") && !pfo_parse(&g.m, "abab", "b"));

    /* every level: read back as written, its par the search's and the test's own, its shortest way at par */
    int per = PFO_LEVELS / PFO_CHAPTERS;
    for (int lv = 0; lv < PFO_LEVELS; lv++) {
        const pfo_level_t *L = &pfo_levels[lv];
        CHECK(pfo_level(&g, lv));
        pfo_text(&g.m, sheet, pic);
        CHECK(!strcmp(sheet, L->sheet) && !strcmp(pic, L->picture));
        CHECK(!pfo_solved(&g.m, &g.s) && g.par == L->par && g.par > 0);
        CHECK(pfo_solve(&g.m, &g.s, NULL, NULL) == g.par);
        int op = own_par(&g.m, g.par);
        CHECK(op == g.par);
        if (op != g.par) printf("paperfold: level %d, the test's own search %d, par %d\n", lv + 1, op, g.par);
        t = g;
        CHECK(pfo_script(&t, "=") && pfo_solved(&t.m, &t.s) && t.moves == g.par);
        int ch = lv / per;
        if (ch == 0) CHECK(!count_holes(L->sheet) && !two_sided(L->sheet, 0) && !two_sided(L->sheet, 1));
        if (ch == 1) CHECK(!count_holes(L->sheet) && two_sided(L->sheet, 0));
        if (ch == 2 || ch == 4) CHECK(count_holes(L->sheet) > 0);
        if (ch == 3 || ch == 5) CHECK(two_sided(L->sheet, 0) && two_sided(L->sheet, 1));
        if (lv && lv % per) CHECK(L->par + 1 >= pfo_levels[lv - 1].par);   /* a smooth curve */
        if (lv % per == 0) CHECK(lv == 0 || L->par <= pfo_levels[lv - 1].par);
        for (int k = 0; k < lv; k++)
            CHECK(strcmp(pfo_levels[k].sheet, L->sheet) != 0 || strcmp(pfo_levels[k].picture, L->picture) != 0);
    }
    CHECK(pfo_levels[0].par == 1);
    CHECK(!pfo_level(&g, PFO_LEVELS) && !pfo_level(&g, -1));

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < PFO_VARIANTS; v++) {
        double worst = 0;
        int lo = 99, hi = 0;
        for (uint32_t seed = 1; seed <= 20; seed++) {
            clock_t t0 = clock();
            pfo_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 1 && own_par(&g.m, g.par) == g.par);
            pfo_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
            pfo_text(&g.m, sheet, pic);
            CHECK(pfo_parse(&t.m, sheet, pic) && !memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 400);
        printf("paperfold: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* the hint from positions along random games: one fold fewer after it, its way ends solved; undo to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        pfo_level(&g, (int)(seed * 7 % PFO_LEVELS));
        first = g;
        int steps = pr_below(&r, 3);
        for (int k = 0; k < 30 && steps && !pfo_solved(&g.m, &g.s); k++)
            if (pfo_move(&g, pr_below(&r, PFO_MOVES))) steps--;
        if (!pfo_solved(&g.m, &g.s)) {
            int f, left = pfo_solve(&g.m, &g.s, &f, NULL);
            if (left > 0) {                     /* (a fold may have made the picture impossible) */
                if (!pfo_move(&g, f)) ok = 0;
                if (pfo_solve(&g.m, &g.s, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                pfo_script(&g, "=");
                if (!pfo_solved(&g.m, &g.s) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (pfo_undo(&g)) {}
        if (memcmp(&first.s, &g.s, sizeof g.s) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("paperfold: %d failures\n", fails); return 1; }
    printf("paperfold: all passed\n");
    return 0;
}
