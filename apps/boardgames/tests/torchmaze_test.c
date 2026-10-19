/*
 * Torch Maze's rules: steps and walls, keys and doors, one-way doors, plates and gates, the fuel and the flasks, runs
 * (to the next junction, across a room, never round a loop for ever), undo (the map kept), the map fading, the light
 * (walls stop it, gates let it through); maps as text both ways; every level's best found again by a search written
 * here, its shortest way walked at the best, the chapters' things there, no two levels alike, the curve (no trivial
 * level from level 4 but a chapter's first); the puzzles made at random (solvable, the same from the same seed, the
 * time they take), and whole games from a seed (random steps, the rest of a shortest way, undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/torchmaze.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a corridor of n cells left to right, open throughout, the start at its left, the exit at its right */
static void corridor(tmz_map *m, int n)
{
    memset(m, 0, sizeof *m);
    m->w = n;
    m->h = 2;
    for (int x = 0; x < n; x++) {
        m->in[x] = 1;
        if (x + 1 < n) m->e[0][x] = TMZ_OPEN;
    }
    m->start = 0;
    m->exit = n - 1;
    m->item[n - 1] = TMZ_EXIT;
    m->radius = 25;
}

/* ---- the test's own search: the fewest steps, by its own reading of the edges ------------------------------------ */
static int own_ok(const tmz_map *m, int c, int d, int keys, int gates)
{
    int x = c % m->w, y = c / m->w, nx = x + (d == 1) - (d == 3), ny = y + (d == 2) - (d == 0);
    if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || !m->in[nx + ny * m->w]) return 0;
    int side = d == 1 || d == 3 ? 0 : 1, e = d == 1 ? c : d == 3 ? c - 1 : d == 2 ? c : c - m->w;
    int t = m->e[side][e], k = m->k[side][e];
    if (t == TMZ_WALL) return 0;
    if (t == TMZ_DOOR) return keys >> k & 1;
    if (t == TMZ_ONEWAY) return d == k;
    if (t == TMZ_GATE) return (k ^ gates) & 1;
    return 1;
}

static int own_best(const tmz_map *m)
{
    int K = 1 << m->ncol, F = 1 << m->nflask, U = m->fuel ? m->fuel + 1 : 1, N = m->w * m->h;
    long total = (long)N * K * 2 * F * U;
    int *dist = malloc(sizeof *dist * (size_t)total);
    long *q = malloc(sizeof *q * (size_t)total);
    if (!dist || !q) { free(dist); free(q); return -2; }
    for (long i = 0; i < total; i++) dist[i] = -1;
#define ID(c, k, g, f, u) (((((long)(c) * K + (k)) * 2 + (g)) * F + (f)) * U + (u))
    long head = 0, tail = 0, s = ID(m->start, 0, 0, 0, m->fuel ? m->fuel : 0);
    dist[s] = 0;
    q[tail++] = s;
    int best = -1;
    while (head < tail && best < 0) {
        long v = q[head++], r = v;
        int u = (int)(r % U); r /= U;
        int f = (int)(r % F); r /= F;
        int g = (int)(r % 2); r /= 2;
        int k = (int)(r % K); r /= K;
        int c = (int)r;
        if (c == m->exit) { best = dist[v]; break; }
        if (m->fuel && u <= 0) continue;
        for (int d = 0; d < 4; d++) {
            if (!own_ok(m, c, d, k, g)) continue;
            int n = c + (d == 1) - (d == 3) + ((d == 2) - (d == 0)) * m->w, nk = k, ng = g, nf = f,
                nu = m->fuel ? u - 1 : 0;
            if (m->item[n] == TMZ_KEY) nk |= 1 << m->ik[n];
            if (m->item[n] == TMZ_PLATE) ng ^= 1;
            if (m->item[n] == TMZ_OIL && !(nf >> m->ik[n] & 1)) { nf |= 1 << m->ik[n]; nu = m->fuel; }
            long w = ID(n, nk, ng, nf, nu);
            if (dist[w] >= 0) continue;
            dist[w] = dist[v] + 1;
            q[tail++] = w;
        }
    }
#undef ID
    free(dist);
    free(q);
    return best;
}

static int dir_to(const tmz_map *m, int a, int b)
{
    for (int d = 0; d < 4; d++)
        if (tmz_next(m, a, d) == b) return d;
    return -1;
}

/* ---- the moves ----------------------------------------------------------------------------------------------- */
static void test_moves(void)
{
    static tmz_game g;
    corridor(&g.m, 6);
    tmz_begin(&g);
    CHECK(!tmz_step(&g, TMZ_LEFT));             /* off the maze */
    CHECK(!tmz_step(&g, TMZ_DOWN));             /* rock below */
    CHECK(tmz_step(&g, TMZ_RIGHT) && g.s.at == 1 && g.s.steps == 1);
    int16_t cells[TMZ_PATH];
    int n;
    CHECK(tmz_run(&g, TMZ_RIGHT, cells, &n) && n == 4 && g.s.at == 5 && tmz_solved(&g) && g.s.steps == 5);
    CHECK(cells[0] == 2 && cells[3] == 5);
    CHECK(tmz_undo(&g) && g.s.at == 1 && g.s.steps == 1 && !tmz_solved(&g));
    CHECK(g.first[5] > 0);                       /* undo never takes back the map */
    CHECK(tmz_undo(&g) && g.s.at == 0 && !tmz_undo(&g));
    /* a door: shut without its key, open with it */
    corridor(&g.m, 6);
    g.m.e[0][2] = TMZ_DOOR;
    g.m.k[0][2] = 1;
    g.m.ncol = 2;
    tmz_begin(&g);
    CHECK(tmz_run(&g, TMZ_RIGHT, NULL, &n) && g.s.at == 2);   /* the run stops before the door */
    CHECK(!tmz_step(&g, TMZ_RIGHT));
    g.m.item[1] = TMZ_KEY;
    g.m.ik[1] = 1;
    tmz_begin(&g);
    CHECK(tmz_step(&g, TMZ_RIGHT) && (g.s.keys & 2) && tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT));
    /* a one-way door: right only */
    corridor(&g.m, 4);
    g.m.e[0][1] = TMZ_ONEWAY;
    g.m.k[0][1] = TMZ_RIGHT;
    tmz_begin(&g);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && !tmz_step(&g, TMZ_LEFT));
    /* a gate, shut at first: the plate opens it; again, it shuts */
    corridor(&g.m, 5);
    g.m.e[0][3] = TMZ_GATE;
    g.m.k[0][3] = 0;
    g.m.item[1] = TMZ_PLATE;
    tmz_begin(&g);
    CHECK(tmz_step(&g, TMZ_RIGHT) && g.s.gates == 1);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && tmz_solved(&g));
    tmz_begin(&g);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_LEFT) && tmz_step(&g, TMZ_RIGHT) && g.s.gates == 0);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && !tmz_step(&g, TMZ_RIGHT));
    /* the fuel: out after its steps; a flask fills it; its light shrinks */
    corridor(&g.m, 8);
    g.m.fuel = 3;
    tmz_begin(&g);
    float r0 = tmz_light(&g.m, &g.s);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_light(&g.m, &g.s) < r0);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && g.s.fuel == 0 && !tmz_step(&g, TMZ_RIGHT));
    CHECK(tmz_light(&g.m, &g.s) == 0);
    g.m.item[2] = TMZ_OIL;
    g.m.ik[2] = 0;
    g.m.nflask = 1;
    tmz_begin(&g);
    CHECK(tmz_step(&g, TMZ_RIGHT) && tmz_step(&g, TMZ_RIGHT) && g.s.fuel == 3 && g.s.flasks == 1);
    CHECK(own_best(&g.m) == -1);              /* 2 steps, filled: 3 more, 5 to go */
    g.m.fuel = 5;
    CHECK(own_best(&g.m) == 7);
    /* the map fading: a cell shown, then left behind long enough, is gone; a lit brazier's stays */
    corridor(&g.m, 14);
    g.m.radius = 15;
    g.m.fade = 4;
    tmz_begin(&g);
    float a;
    CHECK(tmz_remembered(&g, 0, &a) && a == 1);
    for (int i = 0; i < 12; i++) tmz_step(&g, TMZ_RIGHT);
    CHECK(!tmz_remembered(&g, 0, &a));
    CHECK(tmz_remembered(&g, 12, &a) && a == 1);
}

/* the light: a wall stops it, a gate does not, the top of a wall it meets is lit */
static void test_light(void)
{
    static tmz_game g;
    corridor(&g.m, 4);
    tmz_begin(&g);
    CHECK(tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 3.5f, 0.5f, 0));
    g.m.e[0][1] = TMZ_WALL;
    CHECK(!tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 3.5f, 0.5f, 0));
    CHECK(tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 2.1f, 0.5f, 0.13f));   /* on the wall's top */
    CHECK(!tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 2.2f, 0.5f, 0.13f));  /* past it */
    g.m.e[0][1] = TMZ_GATE;
    CHECK(tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 3.5f, 0.5f, 0));
    CHECK(!tmz_ray(&g.m, &g.s, 0.5f, 0.5f, 0.5f, 1.5f, 0));     /* into the rock */
}

/* a run: round a bend, stopped at a junction, across a room, never round a loop for ever */
static void test_runs(void)
{
    static tmz_game g;
    tmz_map *m = &g.m;
    memset(m, 0, sizeof *m);
    m->w = m->h = 3;
    for (int c = 0; c < 9; c++) m->in[c] = 1;
    /* a ring round the middle: 0-1-2, 2-5-8, 8-7-6, 6-3-0; the middle (4) walled off */
    m->e[0][0] = m->e[0][1] = m->e[0][6] = m->e[0][7] = TMZ_OPEN;
    m->e[1][0] = m->e[1][3] = m->e[1][2] = m->e[1][5] = TMZ_OPEN;
    m->start = 0;
    m->exit = 4;
    m->radius = 20;
    tmz_begin(&g);
    int n;
    CHECK(tmz_run(&g, TMZ_RIGHT, NULL, &n) && n == 7 && g.s.at == 3);   /* round the ring, not into its start */
    m->e[0][3] = TMZ_OPEN;                      /* the middle opened from the left: 3 a junction */
    tmz_begin(&g);
    CHECK(tmz_run(&g, TMZ_DOWN, NULL, &n) && n == 1 && g.s.at == 3);
    /* a room 3 x 3, all open: across it in one run */
    memset(m, 0, sizeof *m);
    m->w = m->h = 3;
    for (int c = 0; c < 9; c++) {
        m->in[c] = 1;
        if (c % 3 < 2) m->e[0][c] = TMZ_OPEN;
        if (c / 3 < 2) m->e[1][c] = TMZ_OPEN;
    }
    m->start = 3;
    m->exit = 2;
    tmz_begin(&g);
    CHECK(tmz_run(&g, TMZ_RIGHT, NULL, &n) && g.s.at == 5);
}

/* ---- the levels ---------------------------------------------------------------------------------------------- */
static void test_levels(void)
{
    static tmz_game g;
    static char text[TMZ_TEXT];
    static uint32_t seen[TMZ_LEVELS];
    int per = TMZ_LEVELS / TMZ_CHAPTERS, trivial = 0;
    for (int l = 0; l < TMZ_LEVELS; l++) {
        if (!tmz_level(&g, l)) { CHECK(!"a level parses"); continue; }
        tmz_text(&g.m, text);
        CHECK(!strcmp(text, tmz_levels[l]));
        int own = own_best(&g.m);
        CHECK(own == tmz_level_best[l] && own > 0);
        if (own != tmz_level_best[l]) printf("level %d: best %d, stored %d\n", l + 1, own, tmz_level_best[l]);
        tmz_way w;
        tmz_solve(&g.m, &g.s, &w);
        CHECK(w.steps == own);
        for (int i = 0; i < w.n; i++) CHECK(tmz_step(&g, dir_to(&g.m, g.s.at, w.path[i])));
        CHECK(tmz_solved(&g) && g.s.steps == own && g.par >= own);
        /* the chapter's things */
        int ch = l / per, keys = 0, gates = 0, ow = 0;
        for (int s = 0; s < 2; s++)
            for (int c = 0; c < g.m.w * g.m.h; c++) {
                keys += g.m.e[s][c] == TMZ_DOOR;
                gates += g.m.e[s][c] == TMZ_GATE;
                ow += g.m.e[s][c] == TMZ_ONEWAY;
            }
        if (ch == 1) CHECK(keys > 0);
        if (ch == 2) CHECK(gates > 0);
        if (ch == 3) CHECK(ow > 0);
        if (ch == 4) CHECK(g.m.fuel > 0);
        if (ch == 5) CHECK(g.m.fade > 0);
        /* no two alike */
        uint32_t h = 2166136261u;
        for (const char *p = tmz_levels[l]; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
        for (int k = 0; k < l; k++) CHECK(seen[k] != h);
        seen[l] = h;
        /* the curve: from level 4, only a chapter's first may be walked straight through */
        if (l >= 3 && l % per != 0) {
            tmz_metric mt;
            tmz_measure(&g.m, &mt);
            if (mt.ratio < 1.3) { trivial++; printf("level %d: trivial (%.2f)\n", l + 1, mt.ratio); }
        }
    }
    CHECK(trivial == 0);
}

/* ---- at random --------------------------------------------------------------------------------------------- */
static void test_random(void)
{
    static tmz_game g, h;
    for (int v = 0; v < TMZ_VARIANTS; v++) {
        double worst = 0;
        int lo = 9999, hi = 0;
        for (uint32_t s = 1; s <= 12; s++) {
            clock_t t0 = clock();
            tmz_random(&g, v, s * 977u + 5u);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            int own = own_best(&g.m);
            CHECK(own > 0 && own == g.best);
            if (own < lo) lo = own;
            if (own > hi) hi = own;
            tmz_random(&h, v, s * 977u + 5u);
            CHECK(!memcmp(&g.m, &h.m, sizeof g.m));
        }
        CHECK(worst < 900);
        printf("torchmaze: random variant %d, best %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }
}

/* whole games: random steps and runs, then the rest of a shortest way; undo back to the start */
static void test_games(void)
{
    static tmz_game g;
    for (uint32_t seed = 1; seed <= 40; seed++) {
        uint32_t r = pr_seed(seed);
        tmz_level(&g, pr_below(&r, TMZ_LEVELS));
        int start = g.s.at, moves = 0;
        for (int i = 0; i < 30; i++) {
            int d = pr_below(&r, 4);
            moves += pr_below(&r, 2) ? tmz_run(&g, d, NULL, NULL) > 0 : tmz_step(&g, d);
            if (tmz_solved(&g)) break;
        }
        tmz_way w;
        tmz_solve(&g.m, &g.s, &w);
        if (w.steps >= 0 && !tmz_solved(&g)) {
            CHECK(tmz_script(&g, "="));
            CHECK(tmz_solved(&g));
            moves += w.n;
        }
        while (tmz_undo(&g)) moves--;
        CHECK(g.s.at == start && g.s.steps == 0);
        CHECK(moves == 0 || !tmz_solved(&g));
    }
}

int main(void)
{
    test_moves();
    test_light();
    test_runs();
    test_levels();
    test_random();
    test_games();
    if (fails) { printf("torchmaze: %d failures\n", fails); return 1; }
    printf("torchmaze: all passed\n");
    return 0;
}
