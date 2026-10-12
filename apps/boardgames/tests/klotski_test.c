/*
 * Klotski's rules: every start's par found again by a breadth-first search written here (moves as the tradition
 * counts them; Heng Dao Li Ma's 81), the hint's search from the starts and from positions along the way (its way is
 * always as long as it says, and one move shorter after its first move), slides, moves round corners, a block slid
 * back to where it began (no move), undo, and whole games from a seed (random moves, then hints to the end).
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
    for (int i = 0; i < KLT_PIECES; i++) {
        const klt_piece *p = &g->p[i];
        int w = p->type == KLT_BIG || p->type == KLT_WIDE ? 2 : 1, h = p->type <= KLT_TALL ? 2 : 1;
        for (int dy = 0; dy < h; dy++)
            for (int dx = 0; dx < w; dx++) k[p->x + dx + (p->y + dy) * 4] = "BTWS"[p->type];
        k[p->x + p->y * 4] = "btws"[p->type];
    }
}

/* the test's own search: the positions seen, in a hash of strings */
#define CAP (1 << 19)
static char (*seen)[21];
static int *dist_;
static klt_game *q;

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

static int bfs_par(int level, int *positions)
{
    memset(seen, 0, sizeof(char[21]) * CAP);
    int head = 0, tail = 0;
    klt_new(&q[tail], level);
    char k[21];
    key_of(&q[tail], k);
    dist_[find(k, 1)] = 0;
    tail++;
    int best = -1;
    while (head < tail) {
        klt_game g = q[head++];
        key_of(&g, k);
        int d = dist_[find(k, 0)];
        if (klt_solved(&g)) {
            if (best < 0) best = d;
            continue;
        }
        for (int i = 0; i < KLT_PIECES; i++) {
            int8_t reach[20];
            klt_reach(&g, i, reach);
            for (int c = 0; c < 20; c++) {
                if (reach[c] <= 0) continue;
                klt_game h = g;
                h.nh = 0;
                h.last = -1;
                if (!klt_move(&h, i, c % 4, c / 4)) continue;
                key_of(&h, k);
                if (find(k, 0) >= 0) continue;
                dist_[find(k, 1)] = d + 1;
                q[tail++] = h;
            }
        }
    }
    *positions = tail;
    return best;
}

int main(void)
{
    seen = calloc(CAP, sizeof *seen);
    dist_ = calloc(CAP, sizeof *dist_);
    q = malloc(CAP / 2 * sizeof *q);
    if (!seen || !dist_ || !q) return 1;

    /* each start: well formed (10 blocks, 2 cells empty), its par by the test's own search and by the rules' */
    for (int lv = 0; lv < KLT_LEVELS; lv++) {
        klt_game g;
        klt_new(&g, lv);
        int empty = 0, big = 0;
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 4; x++) empty += klt_at(&g, x, y) < 0;
        for (int i = 0; i < KLT_PIECES; i++) big += g.p[i].type == KLT_BIG;
        CHECK(empty == 2 && big == 1 && !klt_solved(&g));
        int positions, par = bfs_par(lv, &positions);
        clock_t t0 = clock();
        int mine = klt_solve(&g, NULL, NULL, NULL);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("klotski %-20s par %3d (table %3d, rules %3d), %6d positions, %.1f ms\n", klt_name(lv), par,
               klt_par(lv), mine, positions, ms);
        CHECK(par == klt_par(lv) && mine == par);
        CHECK(ms < 500);
        CHECK(klt_script(&g, "=") && klt_solved(&g) && g.moves == par);   /* the hints from the start: the par */
    }
    CHECK(klt_par(0) == 81);

    /* slides and moves */
    klt_game g;
    klt_new(&g, 0);                             /* Heng Dao Li Ma: the empty cells b5 and c5 */
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
        klt_new(&g, (int)(seed % KLT_LEVELS));
        int steps = pr_below(&r, 40);
        for (int k = 0; k < steps; k++) {
            int i = pr_below(&r, KLT_PIECES), c = pr_below(&r, 20);
            klt_reach(&g, i, reach);
            if (reach[c] > 0) {
                g.last = -1;
                klt_move(&g, i, c % 4, c / 4);
            }
        }
        if (klt_solved(&g)) continue;
        int i, x, y, left = klt_solve(&g, &i, &x, &y);
        if (left <= 0) { ok = 0; continue; }
        g.last = -1;
        if (!klt_move(&g, i, x, y)) ok = 0;
        if (klt_solve(&g, NULL, NULL, NULL) != left - 1) ok = 0;
        int before = g.moves;
        klt_script(&g, "=");
        if (!klt_solved(&g) || g.moves - before != left - 1) ok = 0;
        while (klt_undo(&g) >= 0) {}
        klt_game fresh;
        klt_new(&fresh, g.level);
        if (memcmp(fresh.p, g.p, sizeof g.p) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("klotski: %d failures\n", fails); return 1; }
    printf("klotski: all passed\n");
    return 0;
}
