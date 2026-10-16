/*
 * Traffic Jam's rules: pars found again by a breadth-first search written here (over the lots as text), every level's
 * par (one in ten by that search too) and its shortest way played, the puzzles made at random (their par, their range,
 * the time they take), a family's distances against the hint's search, drives, a vehicle driven again at once (the
 * same move) or back (no move), undo, the notation, and whole games from a seed (random drives, then hints to the end,
 * then undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/parking.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own search: lots as 36 letters, every vehicle slid bay by bay ---------------------------------- */
#define CAP (1 << 20)
static char (*seen)[37];
static int *dist_;
static char (*queue)[37];

static unsigned hash(const char *k)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < 36; i++) h = (h ^ (unsigned char)k[i]) * 16777619u;
    return h & (CAP - 1);
}

static int find(const char *k, int add)
{
    unsigned h = hash(k);
    while (seen[h][0]) {
        if (!memcmp(seen[h], k, 36)) return (int)h;
        h = (h + 1) & (CAP - 1);
    }
    if (!add) return -1;
    memcpy(seen[h], k, 37);
    return (int)h;
}

static int bfs_par(const char *lot)
{
    memset(seen, 0, sizeof(char[37]) * CAP);
    int head = 0, tail = 0;
    memcpy(queue[tail++], lot, 37);
    dist_[find(lot, 1)] = 0;
    while (head < tail) {
        char cur[37];
        memcpy(cur, queue[head++], 37);
        int d = dist_[find(cur, 0)];
        if (cur[2 * 6 + 4] == 'A' && cur[2 * 6 + 5] == 'A') return d;
        for (char v = 'A'; v <= 'Z'; v++) {
            int first = -1, last = -1;
            for (int c = 0; c < 36; c++)
                if (cur[c] == v) { if (first < 0) first = c; last = c; }
            if (first < 0) continue;
            int step = last - first >= 6 ? 6 : 1;
            for (int dir = -1; dir <= 1; dir += 2) {
                char t[37];
                memcpy(t, cur, 37);
                int a = first, b = last;
                for (;;) {                      /* one bay further each time: each a position one move away */
                    int head_c = dir < 0 ? a - step : b + step;
                    if (head_c < 0 || head_c >= 36 || (step == 1 && head_c / 6 != a / 6) || t[head_c] != '.') break;
                    int tail_c = dir < 0 ? b : a;
                    t[head_c] = v;
                    t[tail_c] = '.';
                    a += dir * step;
                    b += dir * step;
                    if (find(t, 0) < 0) {
                        dist_[find(t, 1)] = d + 1;
                        memcpy(queue[tail++], t, 37);
                    }
                }
            }
        }
    }
    return -1;
}

/* each position of a family: its distance is the fewest moves the hint's search finds (one in 61 of them) */
static int family_ok, family_n;

static void check_family(const pkj_game *pos, int dist, void *ctx)
{
    (void)ctx;
    if (family_n++ % 61) return;
    if (pkj_solve(pos, NULL, NULL) != dist || pos->par != dist) family_ok = 0;
}

int main(void)
{
    seen = calloc(CAP, sizeof *seen);
    dist_ = calloc(CAP, sizeof *dist_);
    queue = malloc(CAP * sizeof *queue);
    if (!seen || !dist_ || !queue) return 1;

    /* a lot of the test's own: the red car on a3..b3, a truck standing on e1..e3 in its way, a car lying on e6..f6
     * under the truck: three moves */
    static pkj_game g, t, first;
    static const char *const LOT = "....B." "....B." "AA..B." "......" "......" "....CC";
    CHECK(pkj_parse(&g, LOT) && g.nc == 3 && !pkj_solved(&g));
    CHECK(g.c[0].x == 0 && g.c[0].y == 2 && g.c[1].vert && g.c[1].len == 3 && !g.c[2].vert && g.c[2].x == 4);
    char text[37];
    pkj_text(&g, text);
    CHECK(!strcmp(text, LOT));
    CHECK(!pkj_parse(&t, "AA") && !pkj_parse(&t, "....B." "....B." "A...B." "......" "......" "....CC") &&
          !pkj_parse(&t, "AA..B." "....B." "....B." "......" "......" "....CC") &&   /* the red car out of its row */
          !pkj_parse(&t, "....B." "....B." "AA..B." "......" "B....." "....CC"));     /* B twice */
    int lo, hi;
    pkj_range(&g, 0, &lo, &hi);
    CHECK(lo == 0 && hi == 2);                  /* the truck stands in its way */
    pkj_range(&g, 1, &lo, &hi);
    CHECK(lo == 0 && hi == 2);                  /* the car under it */
    CHECK(pkj_solve(&g, NULL, NULL) == 3 && bfs_par(LOT) == 3 && pkj_movers(&g) == 3);
    CHECK(pkj_script(&g, "e1dd") && g.moves == 1 && g.c[1].y == 2);
    CHECK(pkj_drive(&g, 1, 1) && g.moves == 1);   /* the same vehicle again at once: the same move */
    CHECK(pkj_drive(&g, 1, 0) && g.moves == 0 && g.nh == 0);   /* back where it was: no move */
    CHECK(!pkj_script(&g, "e1r") && !pkj_script(&g, "a1d") && !pkj_script(&g, "e1") && !pkj_script(&g, "a3uu"));
    t = g;
    CHECK(pkj_script(&t, "f6ll e1ddd a3rrrr") && pkj_solved(&t) && t.moves == 3);
    CHECK(pkj_undo(&t) == 0 && t.moves == 2 && pkj_undo(&t) == 1 && pkj_undo(&t) == 2 && pkj_undo(&t) == -1);
    CHECK(!memcmp(t.c, g.c, sizeof g.c));

    /* every level: well formed, not solved, its par the rules' search finds (one in ten by the test's own search
     * too), its shortest way takes exactly that many; no two alike; the chapters getting harder */
    double slowest = 0;
    for (int lv = 0; lv < PKJ_LEVELS; lv++) {
        CHECK(pkj_level(&g, lv));
        CHECK(!pkj_solved(&g) && g.par == pkj_level_par[lv] && g.par > 0);
        clock_t t0 = clock();
        int mine = pkj_solve(&g, NULL, NULL);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > slowest) slowest = ms;
        CHECK(mine == g.par);
        if (lv % 10 == 0) CHECK(bfs_par(pkj_levels[lv]) == g.par);
        t = g;
        CHECK(pkj_script(&t, "=") && pkj_solved(&t) && t.moves == g.par);
        if (lv >= 25 && lv % (PKJ_LEVELS / PKJ_CHAPTERS) == 0) CHECK(pkj_level_par[lv] >= pkj_level_par[lv - 25]);
        for (int k = 0; k < lv; k++) CHECK(strcmp(pkj_levels[k], pkj_levels[lv]) != 0);
    }
    printf("parking: the slowest level's search %.1f ms\n", slowest);
    CHECK(!pkj_level(&g, PKJ_LEVELS));

    /* the puzzles made at random: their par found again, in the variant's range, the same from the same seed */
    static const int LO[PKJ_VARIANTS] = {5, 10, 17}, HI[PKJ_VARIANTS] = {9, 16, 99};
    for (int v = 0; v < PKJ_VARIANTS; v++) {
        double worst = 0;
        int plo = 999, phi = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            clock_t t0 = clock();
            CHECK(pkj_random(&g, v, seed));
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < plo) plo = g.par;
            if (g.par > phi) phi = g.par;
            CHECK(g.par >= LO[v] && g.par <= HI[v] && pkj_solve(&g, NULL, NULL) == g.par && !pkj_solved(&g));
            CHECK(pkj_random(&t, v, seed) && !memcmp(t.c, g.c, sizeof g.c));
        }
        printf("parking: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, plo, phi, worst);
    }

    /* a family's distances against the hint's search */
    family_ok = 1;
    family_n = 0;
    pkj_level(&g, 140);
    CHECK(pkj_family(&g, PKJ_FAMILY, check_family, NULL) > 100 && family_ok);

    /* the hint from positions along random games: its first move leaves one move fewer, its way ends solved; undo
     * all the way back gives the start again */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        pkj_level(&g, (int)(seed * 7 % PKJ_LEVELS));
        first = g;
        int steps = pr_below(&r, 30);
        for (int k = 0; k < steps; k++) {
            int i = pr_below(&r, g.nc);
            pkj_range(&g, i, &lo, &hi);
            g.last = pr_below(&r, 2) ? -1 : g.last;
            pkj_drive(&g, i, lo + pr_below(&r, hi - lo + 1));
            if (pkj_solved(&g)) break;
        }
        if (!pkj_solved(&g)) {
            int i, p, left = pkj_solve(&g, &i, &p);
            if (left <= 0) ok = 0;
            else {
                g.last = -1;
                if (!pkj_drive(&g, i, p)) ok = 0;
                if (pkj_solve(&g, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                pkj_script(&g, "=");
                if (!pkj_solved(&g) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (pkj_undo(&g) >= 0) {}
        if (memcmp(first.c, g.c, sizeof g.c) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("parking: %d failures\n", fails); return 1; }
    printf("parking: all passed\n");
    return 0;
}
