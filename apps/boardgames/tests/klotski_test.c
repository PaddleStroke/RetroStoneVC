/*
 * Klotski's rules: pars found again by a breadth-first search written here (moves as the tradition counts them; the
 * traditional Heng Dao Li Ma's 81), every level's par (one in ten by that search too), the puzzles made at random
 * (their par, the time it takes), a family's distances against the hint's search, the hint's search from positions
 * along the way (its way is always as long as it says, and one move shorter after its first move), slides, moves round
 * corners, a block slid back to where it began (no move), undo, and whole games from a seed (random moves, then hints
 * to the end).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/klotski.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a position as a string of its cells (the kind of block on each; its top left cell in small letters): blocks of a
 * kind alike */
static void key_of(const klt_game *g, char *k)
{
    memset(k, '.', 20);
    k[20] = 0;
    for (int i = 0; i < g->np; i++) {
        const klt_piece *p = &g->p[i];
        int w = p->type == KLT_BIG || p->type == KLT_WIDE ? 2 : 1, h = p->type <= KLT_TALL ? 2 : 1;
        for (int dy = 0; dy < h; dy++)
            for (int dx = 0; dx < w; dx++) k[p->x + dx + (p->y + dy) * 4] = "BTWS"[p->type];
        k[p->x + p->y * 4] = "btws"[p->type];
    }
}

/* the test's own search: the positions seen, in a hash of strings; the positions themselves (blocks only) */
#define CAP (1 << 19)
typedef struct { int np; klt_piece p[KLT_PIECES]; } tray;
static char (*seen)[21];
static int *dist_;
static tray *q;

static unsigned hash(const char *k)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < 20; i++) h = (h ^ (unsigned char)k[i]) * 16777619u;
    return h & (CAP - 1);
}

static int find(const char *k, int add)
{
    unsigned h = hash(k);
    while (seen[h][0]) {
        if (!memcmp(seen[h], k, 20)) return (int)h;
        h = (h + 1) & (CAP - 1);
    }
    if (!add) return -1;
    memcpy(seen[h], k, 21);
    return (int)h;
}

static void as_game(const tray *t, klt_game *g)
{
    g->np = t->np;
    memcpy(g->p, t->p, sizeof g->p);
    g->nh = g->moves = 0;
    g->last = -1;
}

static int bfs_par(const klt_game *start, int *positions)
{
    static klt_game g, h;
    memset(seen, 0, sizeof(char[21]) * CAP);
    int head = 0, tail = 0;
    q[tail].np = start->np;
    memcpy(q[tail].p, start->p, sizeof q[tail].p);
    char k[21];
    as_game(&q[tail], &g);
    key_of(&g, k);
    dist_[find(k, 1)] = 0;
    tail++;
    int best = -1;
    while (head < tail) {
        as_game(&q[head++], &g);
        key_of(&g, k);
        int d = dist_[find(k, 0)];
        if (klt_solved(&g)) {
            if (best < 0) best = d;
            continue;
        }
        for (int i = 0; i < g.np; i++) {
            int8_t reach[20];
            klt_reach(&g, i, reach);
            for (int c = 0; c < 20; c++) {
                if (reach[c] <= 0) continue;
                as_game(&q[head - 1], &h);
                if (!klt_move(&h, i, c % 4, c / 4)) continue;
                key_of(&h, k);
                if (find(k, 0) >= 0) continue;
                dist_[find(k, 1)] = d + 1;
                q[tail].np = h.np;
                memcpy(q[tail].p, h.p, sizeof h.p);
                tail++;
            }
        }
    }
    *positions = tail;
    return best;
}

/* each position of a family: its distance is the fewest moves the hint's search finds (one in 97 of them) */
static int family_ok, family_n;

static void check_family(const klt_game *pos, int dist, void *ctx)
{
    (void)ctx;
    if (family_n++ % 97) return;
    if (klt_solve(pos, NULL, NULL, NULL) != dist || pos->par != dist) family_ok = 0;
}

int main(void)
{
    seen = calloc(CAP, sizeof *seen);
    dist_ = calloc(CAP, sizeof *dist_);
    q = malloc(CAP / 2 * sizeof *q);
    if (!seen || !dist_ || !q) return 1;

    /* the traditional start Heng Dao Li Ma (a test tray, not a level): 81 moves by both searches */
    static const char *const CLASSIC = "B01 T00 T03 T20 T23 W21 S31 S32 S40 S43";
    static klt_game g, classic, t, first;
    CHECK(klt_parse(&classic, CLASSIC) && classic.np == 10 && !klt_solved(&classic));
    int positions;
    CHECK(bfs_par(&classic, &positions) == 81 && klt_solve(&classic, NULL, NULL, NULL) == 81);
    char text[KLT_PIECES * 4 + 1];
    klt_text(&classic, text);
    CHECK(!strcmp(text, CLASSIC));
    CHECK(!klt_parse(&g, "B01 T00") && !klt_parse(&g, "B01 B21 T00 T03 W40 W42 S30 S33") && !klt_parse(&g, "X01"));

    /* every level: well formed (the great block, two cells empty), not solved, its par the rules' search finds (one in
     * ten by the test's own search too), the hints from the start take exactly that many; no two alike */
    double slowest = 0;
    for (int lv = 0; lv < KLT_LEVELS; lv++) {
        CHECK(klt_level(&g, lv));
        int empty = 0, big = 0;
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 4; x++) empty += klt_at(&g, x, y) < 0;
        for (int i = 0; i < g.np; i++) big += g.p[i].type == KLT_BIG;
        CHECK(empty == 2 && big == 1 && !klt_solved(&g) && g.par == klt_level_par[lv] && g.par > 0);
        clock_t t0 = clock();
        int mine = klt_solve(&g, NULL, NULL, NULL);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > slowest) slowest = ms;
        CHECK(mine == g.par);
        if (lv % 10 == 0) CHECK(bfs_par(&g, &positions) == g.par);
        t = g;
        CHECK(klt_script(&t, "=") && klt_solved(&t) && t.moves == g.par);
        if (lv >= 25 && lv % (KLT_LEVELS / KLT_CHAPTERS) == 0) CHECK(klt_level_par[lv] >= klt_level_par[lv - 25]);
        for (int k = 0; k < lv; k++) CHECK(strcmp(klt_levels[k], klt_levels[lv]) != 0);
    }
    printf("klotski: the slowest level's search %.1f ms\n", slowest);
    CHECK(!klt_level(&g, KLT_LEVELS));

    /* the puzzles made at random: their par found again, in the variant's range (hard: far), the time it takes */
    for (int v = 0; v < KLT_VARIANTS; v++) {
        double worst = 0;
        int lo = 999, hi = 0;
        for (uint32_t seed = 1; seed <= 12; seed++) {
            clock_t t0 = clock();
            CHECK(klt_random(&g, v, seed));
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par > 0 && klt_solve(&g, NULL, NULL, NULL) == g.par && !klt_solved(&g));
            CHECK(v == 2 ? g.par >= 55 : g.par >= (v ? 30 : 15) && g.par <= (v ? 55 : 30));
            CHECK(klt_random(&t, v, seed) && !memcmp(t.p, g.p, sizeof g.p));
        }
        printf("klotski: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* a family's distances: the classic's, against the hint's search on some of its positions */
    family_ok = 1;
    family_n = 0;
    CHECK(klt_family(&classic, check_family, NULL) > 1000 && family_ok);

    /* slides and moves */
    g = classic;                                /* Heng Dao Li Ma: the empty cells b5 and c5 */
    CHECK(klt_at(&g, 1, 4) < 0 && klt_at(&g, 2, 4) < 0);
    int s = klt_at(&g, 1, 3);                   /* the small block on b4 */
    CHECK(s >= 0 && g.p[s].type == KLT_SMALL);
    CHECK(klt_can(&g, s, 0, 1) && !klt_can(&g, s, 0, -1) && !klt_can(&g, s, -1, 0) && !klt_can(&g, s, 1, 1));
    CHECK(klt_script(&g, "b4d") && g.moves == 1 && g.p[s].y == 4);
    CHECK(klt_step(&g, s, 1, 0) && g.moves == 1);   /* the same block again at once: the same move */
    CHECK(klt_script(&g, "c4l") && g.moves == 2);
    int8_t reach[20];
    klt_reach(&g, s, reach);
    CHECK(reach[2 + 4 * 4] == 0 && reach[1 + 4 * 4] == 1 && reach[3 + 4 * 4] < 0);
    klt_undo(&g);
    CHECK(g.moves == 1 && klt_at(&g, 2, 3) >= 0);
    klt_undo(&g);
    CHECK(g.moves == 0 && g.nh == 0 && klt_at(&g, 1, 3) == s);
    /* round a corner in one move: the small block on b4 to c5 (down, then right) */
    int8_t path[2 * KLT_PATH];
    CHECK(klt_path(&g, s, 2, 4, path) == 2 && path[0] == 1 && path[1] == 4 && path[2] == 2 && path[3] == 4);
    CHECK(klt_move(&g, s, 2, 4) && g.moves == 1);
    CHECK(klt_step(&g, s, -1, 0) && klt_step(&g, s, 0, -1) && g.moves == 0 && g.nh == 0);   /* back: no move */
    CHECK(!klt_move(&g, s, 0, 0) && !klt_script(&g, "a1d") && !klt_script(&g, "e1d") && !klt_script(&g, "b4"));

    /* the hint from positions along random games: its first move leaves one move fewer, its way ends solved; undo
     * all the way back gives the start again */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        klt_level(&g, (int)(seed * 7 % KLT_LEVELS));
        first = g;
        int steps = pr_below(&r, 40);
        for (int k = 0; k < steps; k++) {
            int i = pr_below(&r, g.np), c = pr_below(&r, 20);
            klt_reach(&g, i, reach);
            if (reach[c] > 0) {
                g.last = -1;
                klt_move(&g, i, c % 4, c / 4);
            }
        }
        if (!klt_solved(&g)) {
            int i, x, y, left = klt_solve(&g, &i, &x, &y);
            if (left <= 0) ok = 0;
            else {
                g.last = -1;
                if (!klt_move(&g, i, x, y)) ok = 0;
                if (klt_solve(&g, NULL, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                klt_script(&g, "=");
                if (!klt_solved(&g) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (klt_undo(&g) >= 0) {}
        if (memcmp(first.p, g.p, sizeof g.p) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("klotski: %d failures\n", fails); return 1; }
    printf("klotski: all passed\n");
    return 0;
}
