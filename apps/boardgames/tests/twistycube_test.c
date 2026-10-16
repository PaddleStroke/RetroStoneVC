/*
 * Twisty Cube's rules: the move group's laws on every size (a move then its inverse, four quarter turns, moves along
 * other axes or layers moving other stickers), the solved check (a turn of the whole cube still solved, one move not),
 * scrambles of every length undone by their inverse, every level (its moves, its par: none shorter for the short ones,
 * by a search of the test's own; the hints solving it in par moves, also after moves at random), the reduction of a
 * row of moves, undo, the notation, and puzzles at random (their length, the time they take).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/twistycube.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int same(const tcb_cube *a, const tcb_cube *b) { return a->n == b->n && !memcmp(a->c, b->c, sizeof a->c); }

/* the test's own search: a way of fewer than k moves (any order of moves, half turns as two) */
static int shorter(const tcb_cube *c, int left, int la, int ll)
{
    if (tcb_solved(c)) return 1;
    if (!left) return 0;
    for (int a = 0; a < 3; a++)
        for (int l = 0; l < c->n; l++)
            for (int b = 0; b < 2; b++) {
                if (a == la && l < ll) continue;    /* moves along one axis commute: take their layers rising */
                tcb_cube d = *c;
                tcb_turn(&d, tcb_mv(a, l, b));
                if (shorter(&d, left - 1, a, l)) return 1;
            }
    return 0;
}

static void laws(int n)
{
    tcb_cube s, c;
    tcb_init(&s, n);
    CHECK(tcb_solved(&s));
    for (int a = 0; a < 3; a++)
        for (int l = 0; l < n; l++)
            for (int b = 0; b < 2; b++) {
                tcb_move m = tcb_mv(a, l, b);
                CHECK(tcb_valid(n, m));
                c = s;
                tcb_turn(&c, m);
                CHECK(!tcb_solved(&c));             /* one move is never solved */
                tcb_turn(&c, tcb_inverse(m));
                CHECK(same(&c, &s));                /* a move then its inverse */
                for (int i = 0; i < 4; i++) {
                    tcb_turn(&c, m);
                    CHECK(i == 3 || !same(&c, &s));
                }
                CHECK(same(&c, &s));                /* four quarter turns */
                tcb_cube h = s, h2 = s;             /* a half turn either way is the same */
                tcb_turn(&h, m);
                tcb_turn(&h, m);
                tcb_turn(&h2, tcb_inverse(m));
                tcb_turn(&h2, tcb_inverse(m));
                CHECK(same(&h, &h2));
                /* the stickers moved: those of the layer only, n * 4 of the sides (+ a face's n * n) */
                c = s;
                tcb_turn(&c, m);
                int moved = 0, wrong = 0;
                for (int k = 0; k < 6 * n * n; k++)
                    if (c.c[k] != s.c[k]) {
                        moved++;
                        wrong += tcb_layer_of(n, k, a) != l;
                    }
                CHECK(wrong == 0);
                CHECK(moved == 4 * n);
            }
    /* a turn of the whole cube: all its layers; still solved */
    for (int a = 0; a < 3; a++) {
        c = s;
        for (int l = 0; l < n; l++) tcb_turn(&c, tcb_mv(a, l, 0));
        CHECK(tcb_solved(&c));
        CHECK(!same(&c, &s));
        tcb_turn(&c, tcb_mv(a, 0, 1));              /* then one layer back: not solved */
        CHECK(!tcb_solved(&c));
    }
    /* moves on different axes do not commute, on one axis they do */
    tcb_cube x = s, y = s;
    tcb_turn(&x, tcb_mv(0, 0, 0));
    tcb_turn(&x, tcb_mv(1, 0, 0));
    tcb_turn(&y, tcb_mv(1, 0, 0));
    tcb_turn(&y, tcb_mv(0, 0, 0));
    CHECK(!same(&x, &y));
    x = y = s;
    tcb_turn(&x, tcb_mv(2, 0, 0));
    tcb_turn(&x, tcb_mv(2, n - 1, 1));
    tcb_turn(&y, tcb_mv(2, n - 1, 1));
    tcb_turn(&y, tcb_mv(2, 0, 0));
    CHECK(same(&x, &y));
    /* every sticker's place found again */
    for (int k = 0; k < 6 * n * n; k++) {
        int p[3], nr[3];
        tcb_place(n, k, p, nr);
        CHECK(tcb_sticker_at(n, p, nr) == k);
    }
    /* scrambles of every length, undone by their inverse */
    uint32_t r = pr_seed(77u + (uint32_t)n);
    for (int t = 0; t < 300; t++) {
        tcb_move m[TCB_MAXSCR];
        int k = 1 + t % TCB_MAXSCR, got = tcb_scramble(&r, n, k, m);
        CHECK(got == k);
        c = s;
        for (int i = 0; i < k; i++) tcb_turn(&c, m[i]);
        if (k <= 2) CHECK(!tcb_solved(&c));
        for (int i = k - 1; i >= 0; i--) tcb_turn(&c, tcb_inverse(m[i]));
        CHECK(same(&c, &s));
        tcb_move red[TCB_MAXSCR];                  /* a scramble is already reduced */
        CHECK(tcb_reduce(n, m, k, red) == k);
    }
    /* the scrambles of three moves or less are never shorter */
    for (int t = 0; t < 40; t++) {
        tcb_move m[TCB_MAXSCR];
        int k = 1 + t % 3;
        tcb_scramble(&r, n, k, m);
        c = s;
        for (int i = 0; i < k; i++) tcb_turn(&c, m[i]);
        CHECK(!shorter(&c, k - 1, -1, -1));
    }
}

static void reduce_checks(void)
{
    tcb_move in[8] = {tcb_mv(0, 0, 0), tcb_mv(0, 2, 1), tcb_mv(0, 0, 1)}, out[16];
    CHECK(tcb_reduce(3, in, 3, out) == 1 && out[0] == tcb_mv(0, 2, 1));   /* the same axis: commuting */
    in[0] = in[1] = in[2] = tcb_mv(1, 1, 0);
    CHECK(tcb_reduce(3, in, 3, out) == 1 && out[0] == tcb_mv(1, 1, 1));   /* three quarters: one back */
    in[3] = tcb_mv(1, 1, 0);
    CHECK(tcb_reduce(3, in, 4, out) == 0);                                /* four: nothing */
    in[0] = tcb_mv(0, 0, 0);
    in[1] = tcb_mv(1, 0, 0);
    in[2] = tcb_mv(1, 0, 1);
    in[3] = tcb_mv(0, 0, 1);
    CHECK(tcb_reduce(3, in, 4, out) == 0);                                /* nested inverses */
}

static void game_checks(void)
{
    char w[4];
    for (int a = 0; a < 3; a++)
        for (int l = 0; l < 4; l++)
            for (int b = 0; b < 2; b++) {
                tcb_move m = tcb_mv(a, l, b), m2;
                tcb_move_text(m, w);
                CHECK(tcb_parse_move(4, w, &m2) && m2 == m);
                CHECK(tcb_parse_move(3, w, &m2) == (l < 3));
            }
    tcb_game g;
    tcb_move scr[2] = {tcb_mv(0, 2, 0), tcb_mv(1, 0, 1)};
    tcb_new(&g, 3, scr, 2, 2, 0);
    tcb_cube start = g.cube;
    CHECK(!tcb_solved(&g.cube));
    CHECK(tcb_script(&g, "z1+ z1- x0+"));
    CHECK(tcb_moves(&g) == 3);
    CHECK(!tcb_script(&g, "x3+"));                  /* no layer 3 on a 3 x 3 */
    while (tcb_undo(&g)) {}
    CHECK(same(&g.cube, &start) && tcb_moves(&g) == 0);
    CHECK(tcb_script(&g, "h h") && tcb_solved(&g.cube) && g.hints == 2);
    tcb_move m;
    CHECK(!tcb_hint(&g, &m));                       /* solved: no hint */
    tcb_random(&g, 1, 5);
    CHECK(g.full && g.n == 3 && !tcb_hint(&g, &m));   /* a full scramble: none */
}

static void level_checks(void)
{
    int full = 0, shorts = 0, searched = 0;
    for (int i = 0; i < TCB_LEVELS; i++) {
        tcb_game g;
        int n, par, f;
        CHECK(tcb_level_info(i, &n, &par, &f));
        CHECK(tcb_level(&g, i));
        CHECK(g.n == n && n >= 2 && n <= 4 && g.par == par && g.full == f);
        CHECK(!tcb_solved(&g.cube));
        CHECK(g.nscr > 0);
        if (f) {
            full++;
            CHECK(g.nscr == tcb_full_depth(n));
            CHECK(i >= TCB_LEVELS - TCB_LEVELS / TCB_CHAPTERS);   /* the last chapter's */
        } else {
            shorts++;
            CHECK(par == g.nscr);
            if (par <= 3) {                         /* no shorter way, by the test's own search */
                CHECK(!shorter(&g.cube, par - 1, -1, -1));
                searched++;
            }
            /* the hints solve it in par moves */
            tcb_game h = g;
            int k = 0;
            while (!tcb_solved(&h.cube) && k < 100 && tcb_script(&h, "h")) k++;
            CHECK(tcb_solved(&h.cube) && k == par);
            /* and after moves at random too */
            uint32_t r = pr_seed(1000u + (uint32_t)i);
            h = g;
            for (int j = 0; j < 6; j++) tcb_play(&h, tcb_mv(pr_below(&r, 3), pr_below(&r, n), pr_below(&r, 2)));
            k = 0;
            while (!tcb_solved(&h.cube) && k < 100 && tcb_script(&h, "h")) k++;
            CHECK(tcb_solved(&h.cube) && k <= par + 6);
        }
        /* the scramble's inverse solves it */
        for (int j = g.nscr - 1; j >= 0; j--) tcb_play(&g, tcb_inverse(g.scr[j]));
        CHECK(tcb_solved(&g.cube));
        if (i > 0) {                                /* the pars never drop within a chapter of short scrambles */
            int n0, p0, f0;
            tcb_level_info(i - 1, &n0, &p0, &f0);
            if ((i % (TCB_LEVELS / TCB_CHAPTERS)) && !f && !f0) CHECK(par >= p0);
        }
    }
    CHECK(full == TCB_LEVELS / TCB_CHAPTERS && shorts == TCB_LEVELS - full);
    printf("levels: %d short (%d searched here), %d full\n", shorts, searched, full);
}

static void random_checks(void)
{
    clock_t t0 = clock();
    for (int v = 0; v < TCB_VARIANTS; v++)
        for (int s = 1; s <= 200; s++) {
            tcb_game g;
            tcb_random(&g, v, (uint32_t)s);
            CHECK(g.n == v + 2 && g.full && g.par == 0 && g.nscr == tcb_full_depth(g.n));
            CHECK(!tcb_solved(&g.cube));
            tcb_game g2;
            tcb_random(&g2, v, (uint32_t)s);
            CHECK(same(&g.cube, &g2.cube));         /* the same seed, the same puzzle */
        }
    double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / 600;
    printf("random: %.3f ms a puzzle\n", ms);
    CHECK(ms < 50);
}

int main(void)
{
    for (int n = 2; n <= TCB_MAXN; n++) laws(n);
    reduce_checks();
    game_checks();
    level_checks();
    random_checks();
    if (fails) { printf("twistycube: %d failures\n", fails); return 1; }
    printf("twistycube: ok\n");
    return 0;
}
