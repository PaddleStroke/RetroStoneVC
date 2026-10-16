/*
 * Block Puzzle's rules: the pieces, a placement and its clears (rows, columns, both at once), the score (combo and
 * streak), gold blocks and the pattern, the end, undo (classic's pieces coming back the same), whole classic games from
 * seeds (a tray-only player and random placements), every level (its data sound, its way replayed: legal, the goal
 * reached in exactly par pieces, within the pieces given), the hint and its search's time, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/blocks.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static bk_game G, H;

static int bits(unsigned v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

static void test_shapes(void)
{
    for (int i = 0; i < BK_SHAPES; i++) {
        const bk_shape *s = &bk_shapes[i];
        int n = 0;
        unsigned cols = 0;
        for (int r = 0; r < 5; r++) {
            if (r >= s->h) CHECK(s->row[r] == 0);
            else CHECK(s->row[r] != 0);         /* no empty row inside its box */
            n += bits(s->row[r]);
            cols |= s->row[r];
        }
        CHECK(n == s->n);
        CHECK(cols == (1u << s->w) - 1);        /* every column of its box used, none past it */
        CHECK(bk_shape_of(bk_shape_char(i)) == i);
        for (int j = 0; j < i; j++) CHECK(memcmp(bk_shapes[j].row, s->row, 5) || bk_shapes[j].w != s->w);
        CHECK(bk_colour(i) >= 1 && bk_colour(i) <= 12);
    }
    CHECK(bk_shape_of('?') < 0);
}

static void empty_core(bk_core *k)
{
    memset(k, 0, sizeof *k);
    k->hand[0] = k->hand[1] = k->hand[2] = BK_NONE;
}

static void test_lay(void)
{
    bk_core k;
    bk_result r;
    empty_core(&k);
    k.hand[0] = 7;                              /* a line of five across */
    CHECK(!bk_lay(&k, 0, 6, 0, &r));            /* past the edge */
    CHECK(bk_lay(&k, 0, 5, 0, &r));
    CHECK(k.fill[0] == 0x3e0 && r.lines == 0 && r.gain == 5 && k.score == 5 && k.hand[0] == BK_NONE);
    CHECK(!bk_lay(&k, 0, 0, 0, &r));            /* the place is empty now */
    k.hand[1] = 7;
    CHECK(!bk_fits(k.fill, 7, 1, 0));           /* over a block */
    CHECK(bk_lay(&k, 1, 0, 0, &r));             /* the row full: cleared */
    CHECK(r.lines == 1 && r.rows == 1 && r.cols == 0 && k.fill[0] == 0 && r.gain == 5 + 10 && k.streak == 1);
    /* a row and a column at once (their crossing counted once), right after a clear: the streak */
    empty_core(&k);
    k.streak = 1;
    for (int y = 0; y < BK_N; y++) k.fill[y] = y == 4 ? 0x3ef : 0x010;   /* row 4 and column 4 full but (4, 4) */
    k.gold[4] = 0x001;
    k.gold[7] = 0x010;
    k.hand[2] = 0;
    CHECK(bk_lay(&k, 2, 4, 4, &r));
    CHECK(r.lines == 2 && r.rows == 1u << 4 && r.cols == 1u << 4 && r.golds == 2);
    CHECK(r.gain == 1 + 30 + 20 && k.streak == 2 && r.streak == 2);   /* 1 cell, 10 + 20, 10 a line for the clear before */
    CHECK(bk_popcount(k.fill) == 0 && bk_popcount(k.gold) == 0 && k.lines == 2);
    /* nothing cleared: the streak broken */
    k.hand[0] = 0;
    CHECK(bk_lay(&k, 0, 0, 0, &r) && k.streak == 0 && r.gain == 1);
    /* the pattern: covered once, for good */
    empty_core(&k);
    k.want[2] = 0x00c;
    k.hand[0] = 1;                              /* two across */
    CHECK(bk_lay(&k, 0, 1, 2, &r) && k.want[2] == 0x008 && bk_goal_left(&k, BK_PATTERN, 0) == 1);
    CHECK(bk_goal_left(&k, BK_LINES, 3) == 3 && bk_goal_left(&k, BK_SCORE, 10) == 8);
}

static void test_classic(void)
{
    clock_t t0 = clock();
    int longest = 0;
    long total = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        bk_new_classic(&G, seed);
        CHECK(bk_can_move(&G.p.k) && !bk_hand_empty(&G.p.k));
        uint32_t r = pr_seed(seed * 77);
        int n = 0, sum = 0;
        while (!bk_over(&G) && n < 5000) {
            int s = 0, x = 0, y = 0;
            if (seed % 2) {
                CHECK(bk_greedy(&G.p.k, BK_CLASSIC, 0, 200, &r, &s, &x, &y));
            } else {                            /* a random legal placement */
                int opts = 0;
                for (int i = 0; i < 3; i++)
                    for (int c = 0; c < BK_CELLS; c++) opts += bk_fits(G.p.k.fill, G.p.k.hand[i], c % BK_N, c / BK_N);
                CHECK(opts > 0);
                int pick = pr_below(&r, opts);
                for (int i = 0; i < 3; i++)
                    for (int c = 0; c < BK_CELLS; c++)
                        if (bk_fits(G.p.k.fill, G.p.k.hand[i], c % BK_N, c / BK_N) && pick-- == 0)
                            s = i, x = c % BK_N, y = c / BK_N;
            }
            bk_pos before = G.p;
            CHECK(bk_place(&G, s, x, y));
            sum += G.last.gain;
            n++;
            /* no full line ever stays; the colours follow the blocks */
            for (int i = 0; i < BK_N; i++) CHECK(G.p.k.fill[i] != BK_FULL);
            for (int c = 0; c < BK_CELLS; c++) CHECK(!G.p.col[c] == !(G.p.k.fill[c / BK_N] >> (c % BK_N) & 1));
            if (n % 7 == 0) {                   /* undo, and the same placement again: the same pieces after */
                bk_pos after = G.p;
                CHECK(bk_undo(&G) && !memcmp(&G.p, &before, sizeof before));
                CHECK(bk_place(&G, s, x, y) && !memcmp(&G.p, &after, sizeof after));
            }
        }
        CHECK(bk_over(&G) && !bk_can_move(&G.p.k) && G.p.k.score == sum && G.p.k.placed == n);
        CHECK(bk_stars(&G) >= 1 && bk_stars(&G) <= 3 && !bk_solved(&G));
        if (n > longest) longest = n;
        total += G.p.k.score;
    }
    printf("classic: 200 games, longest %d pieces, mean score %ld, %.2fs\n", longest, total / 200,
           (double)(clock() - t0) / CLOCKS_PER_SEC);
    /* undo all the way to the start */
    bk_new_classic(&G, 9);
    bk_pos first = G.p;
    CHECK(bk_script(&G, "=12"));
    while (bk_undo(&G)) {}
    CHECK(!memcmp(&G.p, &first, sizeof first));
}

static void test_levels(void)
{
    clock_t t0 = clock();
    int per_goal[5] = {0};
    for (int lv = 0; lv < BK_LEVELS; lv++) {
        const bk_level_def *d = &bk_levels[lv];
        CHECK(bk_level(&G, lv));
        CHECK(d->goal >= BK_LINES && d->goal <= BK_PATTERN);
        per_goal[d->goal]++;
        CHECK(G.nseq == d->pieces && d->par >= 1 && d->par <= d->pieces && G.nway == d->par);
        CHECK(!bk_solved(&G) && bk_can_move(&G.p.k));
        CHECK(G.p.k.hand[0] == G.seq[0] && G.p.k.hand[1] == G.seq[1] && G.p.k.hand[2] == G.seq[2] && G.p.k.next == 3);
        for (int i = 0; i < BK_N; i++) CHECK(G.p.k.fill[i] != BK_FULL);
        if (d->goal == BK_GOLDS) CHECK(G.golds > 0);
        if (d->goal == BK_PATTERN) CHECK(bk_popcount(G.pat) > 0);
        if (d->goal == BK_LINES || d->goal == BK_SCORE) CHECK(d->target > 0);
        for (int j = 0; j < lv; j++)
            CHECK(strcmp(bk_levels[j].grid, d->grid) || strcmp(bk_levels[j].seq, d->seq));
        /* the way, replayed */
        for (int i = 0; i < G.nway; i++) {
            CHECK(!bk_over(&G));
            int c = G.way[i][1];
            CHECK(bk_place(&G, G.way[i][0], c % BK_N, c / BK_N));
        }
        CHECK(bk_solved(&G) && bk_over(&G) && G.p.k.placed == d->par && bk_stars(&G) == 3);
        /* the hint follows the way */
        CHECK(bk_level(&G, lv) && bk_script(&G, "=") && bk_solved(&G) && G.p.k.placed == d->par);
    }
    printf("levels: %d (lines %d, score %d, gold %d, pattern %d), ways replayed, %.2fs\n", BK_LEVELS,
           per_goal[BK_LINES], per_goal[BK_SCORE], per_goal[BK_GOLDS], per_goal[BK_PATTERN],
           (double)(clock() - t0) / CLOCKS_PER_SEC);
    /* failing: a level played badly runs out (failed, not solved), and undo comes back */
    CHECK(bk_level(&G, 0));
    int n = 0;
    while (!bk_over(&G) && n < 100) {
        int s = 0, x = 0, y = 0, found = 0;
        for (int i = 0; i < 3 && !found; i++)    /* the first place from the bottom right: careless */
            for (int c = BK_CELLS - 1; c >= 0 && !found; c--)
                if (bk_fits(G.p.k.fill, G.p.k.hand[i], c % BK_N, c / BK_N)) {
                    bk_core t = G.p.k;
                    bk_lay(&t, i, c % BK_N, c / BK_N, NULL);
                    if (t.lines == 0) s = i, x = c % BK_N, y = c / BK_N, found = 1;
                }
        if (!found) break;
        CHECK(bk_place(&G, s, x, y));
        n++;
    }
    if (bk_over(&G)) {
        CHECK(!bk_solved(&G) && bk_stars(&G) == 0 && bk_left(&G) >= 0);
        CHECK(bk_undo(&G) && !bk_over(&G));
    }
}

static void test_hint(void)
{
    /* off the way: the hint looks for a new one, quickly, and keeps it */
    double worst = 0;
    int solved = 0, tried = 0;
    for (int lv = 0; lv < BK_LEVELS; lv += 5) {
        CHECK(bk_level(&G, lv));
        int s, x, y, moved = 0;
        for (int i = 0; i < 3 && !moved; i++)  /* a first placement off the way */
            for (int c = 0; c < BK_CELLS && !moved; c++)
                if (bk_fits(G.p.k.fill, G.p.k.hand[i], c % BK_N, c / BK_N) &&
                    (i != G.way[0][0] || c != G.way[0][1])) {
                    CHECK(bk_place(&G, i, c % BK_N, c / BK_N));
                    moved = 1;
                }
        if (bk_over(&G)) continue;
        CHECK(!bk_on_way(&G));
        clock_t t0 = clock();
        CHECK(bk_hint(&G, &s, &x, &y) && bk_fits(G.p.k.fill, G.p.k.hand[s], x, y));
        double t = (double)(clock() - t0) / CLOCKS_PER_SEC;
        if (t > worst) worst = t;
        H = G;
        tried++;
        if (bk_script(&H, "=") && bk_solved(&H)) solved++;
    }
    printf("hint off the way: %d of %d levels still solved by it, its search %.3fs at worst\n", solved, tried, worst);
    CHECK(worst < 0.5);
    CHECK(solved * 2 >= tried);
}

static void test_script(void)
{
    bk_new_classic(&G, 3);
    CHECK(!bk_script(&G, "zz1"));
    CHECK(!bk_script(&G, "ak11"));
    bk_new_classic(&G, 3);
    CHECK(bk_script(&G, "#" "GGGGGGGGG." ".........." ".........." ".........." ".........."
                        ".........." ".........." ".........." ".........." ".........."));
    CHECK(G.p.k.fill[0] == 0x1ff && G.p.k.gold[0] == 0x1ff && G.p.col[0] == BK_GOLD);
    G.p.k.hand[0] = 0;
    CHECK(bk_script(&G, "aj1") && G.p.k.fill[0] == 0 && G.last.golds == 9 && G.last.lines == 1);
    bk_new_classic(&G, 3);
    G.p.k.hand[1] = 8;                          /* a line of five down, at j6 */
    CHECK(bk_script(&G, "bj6") && (G.p.k.fill[9] >> 9 & 1) && (G.p.k.fill[5] >> 9 & 1));
    CHECK(!bk_script(&G, "bj6"));               /* nothing in that place now */
}

int main(void)
{
    test_shapes();
    test_lay();
    test_classic();
    test_levels();
    test_hint();
    test_script();
    if (fails) {
        printf("blocks: %d failures\n", fails);
        return 1;
    }
    printf("blocks: all passed\n");
    return 0;
}
