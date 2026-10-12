/*
 * Same Game's rules: the groups, a removal (the blocks falling, the columns closing up to the left), the score and the
 * bonus, undo, the end; the boards over hundreds of seeds a size (full, every colour there, cleared by their own way;
 * the time taken), the search for a way to clear, the hint, whole games of random removals from a seed, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/samegame.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a small game from columns written bottom up ("12" ...), w x h */
static void load(sm_game *g, int w, int h, const char *const *cols)
{
    memset(g, 0, sizeof *g);
    g->w = w, g->h = h, g->colours = 3;
    for (int x = 0; x < w; x++)
        for (int y = 0; cols[x][y]; y++) g->b.c[x][y] = (int8_t)(cols[x][y] - '0');
    g->start = g->b;
}

static int column_is(const sm_game *g, int x, const char *want)
{
    int n = (int)strlen(want);
    for (int y = 0; y < g->h; y++)
        if (g->b.c[x][y] != (y < n ? want[y] - '0' : 0)) return 0;
    return 1;
}

static void rules(void)
{
    sm_game g;
    static const char *const a[] = {"112", "21", "1", "33"};
    load(&g, 4, 3, a);
    CHECK(sm_group(&g.b, 4, 3, 0, 0, NULL) == 3);   /* 1 1 up the first column, the 1 beside it at (1, 1)... */
    CHECK(sm_group(&g.b, 4, 3, 1, 1, NULL) == 3);
    CHECK(sm_group(&g.b, 4, 3, 2, 0, NULL) == 1);   /* the lone 1 in column c is not joined (column b's bottom is 2) */
    CHECK(sm_group(&g.b, 4, 3, 3, 1, NULL) == 2);
    CHECK(sm_group(&g.b, 4, 3, 2, 2, NULL) == 0);   /* no block */
    CHECK(sm_points(2) == 0 && sm_points(3) == 1 && sm_points(5) == 9 && sm_points(12) == 100);
    CHECK(!sm_remove(&g, 2, 0));                /* a lone block */
    CHECK(sm_remove(&g, 0, 1) == 3 && g.score == 1);
    CHECK(column_is(&g, 0, "2") && column_is(&g, 1, "2") && column_is(&g, 2, "1") && column_is(&g, 3, "33"));
    CHECK(sm_remove(&g, 0, 0) == 2 && g.score == 1);   /* the two 2s: column a and b empty, c and d close up left */
    CHECK(column_is(&g, 0, "1") && column_is(&g, 1, "33") && column_is(&g, 2, "") && column_is(&g, 3, ""));
    CHECK(!sm_over(&g));
    CHECK(sm_remove(&g, 1, 0) == 2);
    CHECK(sm_over(&g) && !sm_cleared(&g) && sm_blocks(&g) == 1 && g.removed == 3);
    CHECK(sm_undo(&g) && sm_undo(&g) && sm_undo(&g) && !sm_undo(&g));
    CHECK(!memcmp(&g.b, &g.start, sizeof g.b) && g.score == 0 && g.removed == 0);
    /* cleared: the bonus */
    static const char *const b[] = {"1122", "1122"};
    load(&g, 2, 4, b);
    CHECK(sm_remove(&g, 0, 0) == 4 && g.score == 4);
    CHECK(sm_remove(&g, 1, 0) == 4 && sm_cleared(&g) && sm_over(&g) && g.score == 8 + SM_BONUS);
    CHECK(sm_undo(&g) && g.score == 4);
    /* the notation */
    load(&g, 4, 3, a);
    CHECK(sm_script(&g, "a2") && g.score == 1);
    CHECK(!sm_script(&g, "c1"));
    CHECK(!sm_script(&g, "z1"));
    load(&g, 4, 3, a);
    CHECK(sm_script(&g, "~") && sm_over(&g));
}

static int full(const sm_game *g)
{
    int count[8] = {0};
    for (int x = 0; x < SM_W; x++)
        for (int y = 0; y < SM_H; y++) {
            int c = g->b.c[x][y], in = x < g->w && y < g->h;
            if (in != (c != 0) || c < 0 || c > g->colours) return 0;
            count[c]++;
        }
    for (int c = 1; c <= g->colours; c++)
        if (count[c] < g->w * g->h / g->colours / 2) return 0;   /* every colour well there */
    return 1;
}

static void boards(void)
{
    static const int seeds[SM_VARIANTS] = {400, 300, 200};
    for (int v = 0; v < SM_VARIANTS; v++) {
        double slowest = 0;
        clock_t t0 = clock();
        int biggest = 0, groups = 0;
        for (int s = 1; s <= seeds[v]; s++) {
            sm_game g;
            clock_t a = clock();
            sm_new(&g, v, (uint32_t)s * 2654435u);
            double ms = (clock() - a) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > slowest) slowest = ms;
            CHECK(full(&g));
            CHECK(!sm_over(&g));
            /* the kept way clears it, a group of two or more each time */
            int sum = 0;
            for (int k = 0; k < g.nway; k++) {
                int n = sm_remove(&g, g.way[k][0], g.way[k][1]);
                CHECK(n >= 2);
                if (n > biggest) biggest = n;
                sum += sm_points(n);
            }
            CHECK(sm_cleared(&g) && g.score == sum + SM_BONUS);
            groups += g.nway;
        }
        double avg = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC / seeds[v];
        printf("  variant %d: %.1f groups on the way; boards made in %.2f ms (slowest %.1f)\n", v,
               (double)groups / seeds[v], avg, slowest);
        CHECK(slowest < 200);
    }
    sm_game a, b;
    sm_new(&a, 1, 9);
    sm_new(&b, 1, 9);
    CHECK(!memcmp(&a.b, &b.b, sizeof a.b));
    sm_new(&b, 1, 10);
    CHECK(memcmp(&a.b, &b.b, sizeof a.b));
}

/* the hint: on the way, its next group; off it, a way found (played to the end on the small boards) or the best group */
static void hints(void)
{
    for (int s = 1; s <= 40; s++) {
        sm_game g;
        sm_new(&g, 0, (uint32_t)s);
        int x, y;
        CHECK(sm_hint(&g, &x, &y) == 2 && x == g.way[0][0] && y == g.way[0][1]);
        CHECK(sm_script(&g, "=3") && g.removed == 3);
        CHECK(sm_hint(&g, &x, &y) == 2 && x == g.way[3][0] && y == g.way[3][1]);
        sm_game h = g;
        CHECK(sm_script(&h, "=") && sm_cleared(&h) && h.removed == h.nway);
        /* off the way: a group of another order; then the hints to the end */
        uint32_t r = pr_seed((uint32_t)s);
        for (int k = 0; k < 2; k++) {
            for (int tries = 0; tries < 500; tries++)
                if (sm_remove(&g, pr_below(&r, g.w), pr_below(&r, g.h))) break;
        }
        int steps = 0, sure = 0;
        while (!sm_over(&g) && steps < 200) {
            int kind = sm_hint(&g, &x, &y);
            CHECK(kind > 0);
            sure += kind == 2;
            CHECK(sm_remove(&g, x, y) >= 2);
            steps++;
        }
        CHECK(sm_over(&g));
    }
    /* the search: a way it finds clears the board, step by step */
    int found = 0;
    for (int s = 1; s <= 20; s++) {
        sm_game g;
        sm_new(&g, 0, (uint32_t)s + 100);
        uint32_t r = pr_seed((uint32_t)s);
        for (int tries = 0; tries < 500; tries++)
            if (sm_remove(&g, pr_below(&r, g.w), pr_below(&r, g.h))) break;
        int x, y;
        if (!sm_solve(&g.b, g.w, g.h, 200000, &x, &y)) continue;
        found++;
        while (!sm_cleared(&g)) {
            CHECK(sm_remove(&g, x, y) >= 2);
            if (sm_cleared(&g)) break;
            if (!sm_solve(&g.b, g.w, g.h, 200000, &x, &y)) { CHECK(0); break; }
        }
        CHECK(sm_cleared(&g));
    }
    printf("  the search found a way on %d boards of 20 taken off the kept way\n", found);
    CHECK(found > 5);
}

/* whole games of random removals from a seed: the score is the sum, the blocks only go, undo back to the start */
static void games(void)
{
    for (int s = 1; s <= 100; s++) {
        sm_game g;
        sm_new(&g, s % 3, (uint32_t)s * 77u);
        uint32_t r = pr_seed((uint32_t)s);
        int sum = 0, blocks = sm_blocks(&g);
        while (!sm_over(&g)) {
            int x = pr_below(&r, g.w), y = pr_below(&r, g.h), n = sm_remove(&g, x, y);
            if (!n) continue;
            sum += sm_points(n);
            CHECK(sm_blocks(&g) == blocks - n);
            blocks -= n;
            for (int c = 0; c + 1 < g.w; c++)   /* no empty column before a full one; no block over a gap */
                CHECK(g.b.c[c][0] || !g.b.c[c + 1][0]);
            for (int c = 0; c < g.w; c++)
                for (int h = 0; h + 1 < g.h; h++) CHECK(g.b.c[c][h] || !g.b.c[c][h + 1]);
        }
        CHECK(g.score == sum + (sm_cleared(&g) ? SM_BONUS : 0));
        while (sm_undo(&g)) {}
        CHECK(!memcmp(&g.b, &g.start, sizeof g.b) && g.score == 0);
    }
}

/* the goal: the beam search's best is never below a greedy game's nor a game that clears by the kept way; the stars;
 * a random board's goal and the time it takes */
static void goals(void)
{
    static sm_game g;
    for (uint32_t seed = 1; seed <= 20; seed++) {
        sm_new(&g, 0, seed);
        int greedy = sm_best(&g.start, g.w, g.h, 1), wide = sm_reference(&g, 32);
        sm_game t = g;
        CHECK(sm_script(&t, "=") && sm_cleared(&t));
        CHECK(greedy > 0 && wide >= greedy && wide >= t.score && sm_goal_of(wide) == wide * 9 / 10);
    }
    g.goal = 100;
    g.score = 100;
    CHECK(sm_stars(&g) == 3);
    g.score = 50;
    CHECK(sm_stars(&g) == 2);
    g.score = 49;
    CHECK(sm_stars(&g) == 1);
    g.goal = 0;
    CHECK(sm_stars(&g) == 3);
    double slowest = 0;
    for (int v = 0; v < SM_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 6; seed++) {
            sm_new(&g, v, seed);
            clock_t t0 = clock();
            int best = sm_reference(&g, 6);
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (ms > slowest) slowest = ms;
            CHECK(best > 0);
        }
    printf("samegame: a random board's goal in %.0f ms at most\n", slowest);
}

/* the levels: each one loads, its way clears it (the bonus earned), its goal below what that search can reach and
 * above nothing; the first chapter's goals found again; the boards never smaller from a chapter to the next; the text
 * read back; no two alike */
static void levels(void)
{
    static sm_game g, t;
    static char text[SM_W * SM_H * 3 + 4];
    int last = 0;
    for (int l = 0; l < SM_LEVELS; l++) {
        CHECK(sm_level(&g, l));
        CHECK(g.goal == sm_level_goal[l] && g.goal > 0 && g.score == 0 && !sm_over(&g) && g.nway > 0);
        int blocks = sm_blocks(&g), count[8] = {0};
        CHECK(blocks == g.w * g.h);
        for (int x = 0; x < g.w; x++)
            for (int y = 0; y < g.h; y++) count[g.b.c[x][y]]++;
        for (int c = 1; c <= g.colours; c++) CHECK(count[c] >= 2);
        t = g;
        CHECK(sm_script(&t, "=") && sm_cleared(&t) && t.score >= SM_BONUS);
        if (l < SM_LEVELS / SM_CHAPTERS) CHECK(sm_goal_of(sm_reference(&g, 256)) == g.goal);
        if (l % (SM_LEVELS / SM_CHAPTERS) == 0) {
            CHECK(g.w * g.h >= last);
            last = g.w * g.h;
        }
        sm_text(&g, text);
        CHECK(!strcmp(text, sm_levels[l]));
        for (int k = 0; k < l; k++) CHECK(strcmp(sm_levels[k], sm_levels[l]) != 0);
    }
    CHECK(!sm_level(&g, SM_LEVELS) && !sm_parse(&g, "12|3") && !sm_parse(&g, "18|12 aa") && !sm_parse(&g, "12|21 az"));
    CHECK(sm_parse(&g, "12|12 aa") && g.w == 2 && g.h == 2 && g.b.c[0][1] == 1 && g.nway == 1);
}

int main(void)
{
    rules();
    boards();
    hints();
    games();
    goals();
    levels();
    if (fails) { printf("samegame: %d checks failed\n", fails); return 1; }
    printf("samegame: all checks passed\n");
    return 0;
}
