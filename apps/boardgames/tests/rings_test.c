/*
 * Ring Puzzle's rules: the rings' shared places, a turn and its inverse, a full turn back to the start, the targets;
 * every level read back, its par found again by the search and (up to eight moves) by the test's own search from one
 * end, its stored way played to the target in par moves, the chapters' rings; the hint's way, undo back to the start;
 * the random puzzles (their par, the same from the same seed, the time they take).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/rings.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the test's own search: breadth first from p, every position kept (a sorted list a layer), up to `most` moves */
static int cmp64(const void *a, const void *b)
{
    rg_pos x = *(const rg_pos *)a, y = *(const rg_pos *)b;
    return x < y ? -1 : x > y;
}

static int own_par(const rg_map *m, rg_pos p, int most)
{
    if (p == m->target) return 0;
    size_t cap = 4000000, n = 1, seen_n = 1;
    rg_pos *layer = malloc(cap * sizeof *layer), *next = malloc(cap * sizeof *next), *seen = malloc(cap * sizeof *seen);
    int found = -1;
    layer[0] = p;
    seen[0] = p;
    for (int d = 1; d <= most && found < 0 && n; d++) {
        size_t k = 0;
        for (size_t i = 0; i < n && k + 8 < cap; i++)
            for (int mv = 0; mv < rg_moves(m); mv++) {
                rg_pos s = rg_apply(m, layer[i], mv);
                if (s == m->target) found = d;
                if (bsearch(&s, seen, seen_n, sizeof s, cmp64)) continue;
                next[k++] = s;
            }
        qsort(next, k, sizeof *next, cmp64);
        size_t u = 0;
        for (size_t i = 0; i < k; i++)
            if (!u || next[i] != next[u - 1]) next[u++] = next[i];
        if (seen_n + u >= cap) break;
        memcpy(seen + seen_n, next, u * sizeof *next);
        seen_n += u;
        qsort(seen, seen_n, sizeof *seen, cmp64);
        rg_pos *t = layer;
        layer = next;
        next = t;
        n = u;
    }
    free(layer);
    free(next);
    free(seen);
    return found;
}

int main(void)
{
    static rg_game g, t;
    rg_map m;
    char txt[RG_PLACES + 1];

    /* the rings: places shared where they cross */
    CHECK(rg_build(&m, RG_TWO, 8) && m.places == 14 && m.rings == 2);
    CHECK(rg_build(&m, RG_TWO, 12) && m.places == 22);
    CHECK(rg_build(&m, RG_CHAIN, 8) && m.places == 20 && m.rings == 3 && !rg_build(&m, RG_CHAIN, 10));
    CHECK(rg_build(&m, RG_TRIANGLE, 12) && m.places == 30);
    CHECK(!rg_build(&m, RG_TRIANGLE, 8) && !rg_build(&m, RG_TWO, 14));
    {
        int shared3 = 0;                        /* the triangle: no place on all three rings */
        rg_build(&m, RG_TRIANGLE, 12);
        for (int p = 0; p < m.places; p++) {
            int c = 0;
            for (int k = 0; k < 3; k++)
                for (int i = 0; i < 12; i++) c += m.slot[k][i] == p;
            shared3 += c == 3;
        }
        CHECK(shared3 == 0);
    }
    /* a turn: its inverse undoes it, n of them come round; the shared bead moves the other ring's order */
    for (int kind = RG_TWO; kind <= RG_TRIANGLE; kind++) {
        rg_build(&m, kind, kind == RG_CHAIN ? 8 : 12);
        rg_pattern(&m, 1, 0);
        uint32_t r = pr_seed(9);
        rg_pos p = m.target;
        for (int i = 0; i < 40; i++) p = rg_apply(&m, p, pr_below(&r, rg_moves(&m)));
        for (int mv = 0; mv < rg_moves(&m); mv++) {
            CHECK(rg_apply(&m, rg_apply(&m, p, mv), rg_inverse(mv)) == p);
            rg_pos q = p;
            for (int i = 0; i < m.n; i++) q = rg_apply(&m, q, mv);
            CHECK(q == p);
        }
    }
    for (int kind = RG_TWO; kind <= RG_TRIANGLE; kind++)   /* room for the beads: no two places close */
        for (int n = 8; n <= 12; n += 2) {
            if (!rg_build(&m, kind, n)) continue;
            float least = 9;
            for (int i = 0; i < m.places; i++)
                for (int j = 0; j < i; j++) {
                    float dx = m.px[i] - m.px[j], dy = m.py[i] - m.py[j], d = dx * dx + dy * dy;
                    if (d < least) least = d;
                }
            CHECK(least > 0.5f * 0.5f);
            if (least <= 0.25f) printf("rings: kind %d n %d, two places %.3f apart\n", kind, n, sqrtf(least));
        }
    rg_build(&m, RG_TWO, 8);
    rg_pattern(&m, 0, 0);
    CHECK(m.colours == 2);
    {
        int ones = 0;
        for (int p = 0; p < m.places; p++) ones += rg_colour(m.target, p);
        CHECK(ones == 7);                       /* each ring its colour, the shared places one each */
        rg_pos q = rg_apply(&m, m.target, 0);
        CHECK(q != m.target && rg_solve(&m, q, NULL) == 1);
        int f;
        CHECK(rg_solve(&m, q, &f) == 1 && f == 1);
    }
    rg_pattern(&m, 2, 0);
    CHECK(m.colours == 4);
    rg_build(&m, RG_TRIANGLE, 12);
    rg_pattern(&m, 1, 0);
    CHECK(m.colours == 4);
    CHECK(rg_text(&m, m.target, txt) == 30 && rg_parse(&m, txt, &t.p) && t.p == m.target);
    CHECK(!rg_parse(&m, "0123", &t.p));

    /* every level */
    int per = RG_LEVELS / RG_CHAPTERS;
    for (int lv = 0; lv < RG_LEVELS; lv++) {
        const rg_level_def *L = &rg_levels[lv];
        if (!rg_level(&g, lv)) { CHECK(0); continue; }
        CHECK(!rg_solved(&g) && g.par == L->par && (int)strlen(L->way) == L->par);
        int f, s = rg_solve(&g.m, g.p, &f);
        CHECK(s == L->par && f >= 0);
        if (L->par <= 8) {
            int op = own_par(&g.m, g.p, L->par);
            CHECK(op == L->par);
            if (op != L->par) printf("rings: level %d, the test's own search %d, par %d\n", lv + 1, op, L->par);
        }
        t = g;
        for (const char *w = L->way; *w; w++) CHECK(rg_move(&t, *w - 'a'));
        CHECK(rg_solved(&t) && t.nh == L->par);
        int ch = lv / per;
        static const int KIND[RG_CHAPTERS] = {RG_TWO, RG_TWO, RG_TWO, RG_CHAIN, RG_TRIANGLE, -1};
        if (KIND[ch] >= 0) CHECK(L->kind == KIND[ch]);
        if (ch == 0) CHECK(L->colours == 2 && L->n == 8);
        if (lv >= 3) CHECK(L->par >= 3);         /* (only the first three are one or two moves) */
        if (ch == RG_CHAPTERS - 1) CHECK(L->par >= 7);
        for (int k = 0; k < lv; k++)
            CHECK(strcmp(rg_levels[k].start, L->start) != 0 || rg_levels[k].kind != L->kind || rg_levels[k].n != L->n);
    }
    CHECK(rg_levels[0].par == 1 && !rg_level(&g, RG_LEVELS) && !rg_level(&g, -1));

    /* the hint's way and undo */
    for (int lv = 0; lv < RG_LEVELS; lv += 7) {
        rg_level(&g, lv);
        rg_pos first = g.p;
        uint32_t r = pr_seed((uint32_t)lv + 1);
        for (int i = 0; i < 3; i++) rg_move(&g, pr_below(&r, rg_moves(&g.m)));
        int left = rg_solve(&g.m, g.p, NULL), n0 = g.nh;
        CHECK(rg_script(&g, "=") && rg_solved(&g) && g.nh - n0 == left);
        while (rg_undo(&g)) {}
        CHECK(g.p == first && g.nh == 0);
    }
    CHECK(rg_level(&g, 0) && rg_script(&g, "a+ a-") && g.nh == 2 && !rg_script(&g, "d+") && !rg_script(&g, "a*"));

    /* the random puzzles */
    for (int v = 0; v < RG_VARIANTS; v++) {
        static const int LO[RG_VARIANTS] = {4, 6, 7}, HI[RG_VARIANTS] = {6, 8, 9};
        double worst = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            clock_t t0 = clock();
            rg_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            CHECK(g.par >= LO[v] - 1 && g.par <= HI[v] + 1 && rg_solve(&g.m, g.p, NULL) == g.par);
            rg_random(&t, v, seed);
            CHECK(t.p == g.p && t.m.target == g.m.target);
        }
        CHECK(worst < 500);
        printf("rings: random variant %d, the slowest %.0f ms\n", v, worst);
    }

    if (fails) { printf("rings: %d failures\n", fails); return 1; }
    printf("rings: all passed\n");
    return 0;
}
