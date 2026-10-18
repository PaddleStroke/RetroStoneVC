/*
 * Hex Stacks' rules: the honeycomb's neighbours, stacks as text both ways, the chain (gathering on the purest stack,
 * a band given away, a clear at ten, the chain going on past it, a lock opening, a stack's height), the deals, undo
 * of one placement, the notation; every level read back, mirrored about the board's middle, won by the rules' player
 * in exactly its par with room to spare; endless play from a seed (the same game twice, its end, the hint's time).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/hexstack.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

#define CELL(x, y) ((x) + (y) * HX_COLS)

static int mirror(int c)
{
    int x = c % HX_COLS, y = c / HX_COLS, x2 = 12 - (2 * x + (y & 1)), mx = (x2 - (y & 1)) / 2;
    return mx >= 0 && mx < HX_COLS ? mx + y * HX_COLS : -1;
}

static int tiles(const hx_pos *p)
{
    int n = 0;
    for (int c = 0; c < HX_CELLS; c++) n += p->n[c];
    return n;
}

int main(void)
{
    static hx_game g, t;

    /* neighbours: even and odd rows, the board's edge */
    CHECK(hx_dir(CELL(3, 2), 0) == CELL(4, 2) && hx_dir(CELL(3, 2), 1) == CELL(3, 3) && hx_dir(CELL(3, 2), 2) == CELL(2, 3));
    CHECK(hx_dir(CELL(3, 2), 4) == CELL(2, 1) && hx_dir(CELL(3, 2), 5) == CELL(3, 1) && hx_dir(CELL(3, 2), 3) == CELL(2, 2));
    CHECK(hx_dir(CELL(3, 1), 1) == CELL(4, 2) && hx_dir(CELL(3, 1), 2) == CELL(3, 2) && hx_dir(CELL(3, 1), 5) == CELL(4, 0));
    CHECK(hx_dir(CELL(0, 0), 3) == -1 && hx_dir(CELL(0, 0), 4) == -1 && hx_dir(CELL(6, 1), 0) == -1);
    for (int c = 0; c < HX_CELLS; c++)          /* each neighbour's neighbour back */
        for (int d = 0; d < 6; d++) {
            int k = hx_dir(c, d);
            if (k >= 0) CHECK(hx_dir(k, (d + 3) % 6) == c);
        }

    /* stacks as text */
    hx_tray_stack st[4];
    char txt[16];
    CHECK(hx_parse_stacks("BeJ", st, 4) == 2 && st[0].n == 3 && st[0].t[0] == 0 && st[0].t[2] == 1 && st[1].n == 2);
    hx_stack_text(&st[0], txt);
    CHECK(!strcmp(txt, "Be"));
    CHECK(!hx_parse_stacks("bE", st, 4) && !hx_parse_stacks("Y", st, 4));

    /* the first level: the stack put in the middle of the red ring gathers all of it and clears */
    CHECK(hx_level(&g, 0));
    CHECK(hx_open_empty(&g.p) == 13 && g.p.tray[0].n == 2 && g.p.tray[0].t[0] == 0);
    CHECK(hx_place(&g, 0, CELL(3, 2)));
    CHECK(hx_solved(&g) && g.p.cleared == 14 && tiles(&g.p) == 0 && g.p.placed == 1);
    CHECK(g.nev == 7 && g.ev[6].kind == HX_EV_CLEAR && g.ev[6].count == 14 && g.ev[0].kind == HX_EV_MOVE);
    CHECK(!hx_place(&g, 1, CELL(3, 0)));       /* solved: nothing more */
    CHECK(hx_undo(&g) && !hx_undo(&g) && g.p.cleared == 0 && tiles(&g.p) == 12 && g.p.tray[0].n == 2);

    /* giving: the stack put (red 2 on blue 1) gives its red to the purer red beside it (red 2 alone), not to the red
     * on blue on its other side; what it uncovers matches nothing */
    CHECK(hx_load(&g, ".......|..SoS..|..ooo..", "BFb", "EbEbEb", HX_TILES, 0, 50, 3));
    {
        hx_pos *p = &g.p;
        CHECK(p->n[CELL(2, 1)] == 2 && p->n[CELL(4, 1)] == 4 && hx_top(p, CELL(4, 1), NULL) == 0);
        CHECK(p->tray[0].n == 3 && p->tray[0].t[0] == 1 && p->tray[0].t[2] == 0);
        CHECK(hx_place(&g, 0, CELL(3, 1)));
        int b;
        CHECK(hx_top(p, CELL(2, 1), &b) == 0 && b == 4 && p->n[CELL(2, 1)] == 4);
        CHECK(p->n[CELL(3, 1)] == 1 && hx_top(p, CELL(3, 1), NULL) == 1 && p->n[CELL(4, 1)] == 4);
        CHECK(g.nev == 1 && g.ev[0].a == CELL(3, 1) && g.ev[0].b == CELL(2, 1) && g.ev[0].count == 2);
    }
    /* gathering and clearing: red 4 put between two reds of 3 (on blue) gathers both, ten clear, the lock below
     * (ten tiles) opens; the blues left apart */
    CHECK(hx_load(&g, "..SoS..|...1...", "EcEc", "DDD", HX_TILES, 0, 99, 3));
    {
        hx_pos *p = &g.p;
        CHECK(p->n[CELL(2, 0)] == 4 && hx_top(p, CELL(2, 0), NULL) == 0 && p->kind[CELL(3, 1)] == HX_LOCK);
        CHECK(hx_open_empty(p) == 1 && !hx_full(p));
        CHECK(hx_place(&g, 0, CELL(3, 0)));
        CHECK(p->cleared == 10 && p->by_colour[0] == 10 && tiles(p) == 2 && p->n[CELL(3, 0)] == 0);
        CHECK(p->kind[CELL(3, 1)] == HX_OPEN && hx_open_empty(p) == 2);
        CHECK(g.nev == 4 && g.ev[2].kind == HX_EV_CLEAR && g.ev[3].kind == HX_EV_UNLOCK);
        CHECK(hx_goal_left(&g) == 89 && !hx_over(&g));
    }
    /* the board full: over, failed */
    CHECK(hx_load(&g, "..ooo..", "", "EIEIEI", HX_TILES, 0, 99, 3));
    CHECK(hx_script(&g, "aa1") == 0 && hx_script(&g, "ac1 bd1 ce1") && hx_full(&g.p) && hx_over(&g) && !hx_solved(&g));
    {
        int ok = 1;                             /* random chains: the tiles kept or cleared, none higher than allowed */
        for (uint32_t seed = 1; seed <= 300; seed++) {
            hx_endless(&g, (int)(seed % HX_VARIANTS), seed);
            uint32_t r = pr_seed(seed);
            for (int k = 0; k < 60 && !hx_over(&g); k++) {
                int before = tiles(&g.p), sl = pr_below(&r, 3), c = pr_below(&r, HX_CELLS), add = g.p.tray[sl].n;
                int cl = g.p.cleared;
                if (!hx_place(&g, sl, c)) continue;
                for (int i = 0; i < HX_CELLS; i++) {
                    if (g.p.n[i] > HX_TALL || (g.p.n[i] && g.p.kind[i] != HX_OPEN)) ok = 0;
                    int b, top = hx_top(&g.p, i, &b);
                    if (top >= 0 && b >= HX_CLEAR) ok = 0;   /* a band of ten left standing */
                }
                if (tiles(&g.p) + (g.p.cleared - cl) != before + add) ok = 0;
            }
        }
        CHECK(ok);
    }

    /* undo: one placement, its tray back */
    hx_endless(&g, 1, 77);
    t = g;
    {
        int s, c;
        CHECK(hx_hint(&g, &s, &c) && hx_place(&g, s, c));
        CHECK(hx_undo(&g) && !memcmp(&g.p, &t.p, sizeof g.p) && !hx_undo(&g));
        CHECK(hx_place(&g, s, c) && hx_hint(&g, &s, &c) && hx_place(&g, s, c) && hx_undo(&g) && !hx_undo(&g));
    }
    /* the notation */
    CHECK(hx_level(&g, 0) && hx_script(&g, "ad3") && hx_solved(&g));
    CHECK(hx_level(&g, 0) && !hx_script(&g, "ac2"));   /* (2,1) holds a stack */
    CHECK(hx_level(&g, 0) && hx_script(&g, "=") && hx_solved(&g) && g.p.placed == 1);

    /* every level */
    int per = HX_LEVELS / HX_CHAPTERS;
    for (int lv = 0; lv < HX_LEVELS; lv++) {
        const hx_level_def *L = &hx_levels[lv];
        if (!hx_level(&g, lv)) { CHECK(0); continue; }
        CHECK(!hx_solved(&g) && g.par == L->par && L->par >= 1 && L->target >= 10);
        CHECK(L->colours >= 3 && L->colours <= HX_COLOURS && (L->goal == HX_TILES || L->goal == HX_COLOUR));
        for (int c = 0; c < HX_CELLS; c++) {    /* mirrored about the middle */
            int m = mirror(c);
            if (g.p.kind[c] == HX_VOID) continue;
            CHECK(m >= 0 && g.p.kind[m] == g.p.kind[c] && g.p.n[m] == g.p.n[c] &&
                  !memcmp(g.p.t[m], g.p.t[c], g.p.n[c]) && g.p.need[m] == g.p.need[c]);
        }
        int won, room, k = hx_play_out(&g, 0, 1, 300, &won, &room);
        CHECK(won && k == L->par && room >= 2);
        if (!won || k != L->par) printf("hexstack: level %d, won %d in %d, par %d\n", lv + 1, won, k, L->par);
        int ch = lv / per;
        if (ch == 0) CHECK(L->colours <= 5 && L->goal == HX_TILES);
        if (ch >= 2) {
            int stones = 0, locks = 0;
            hx_level(&t, lv);
            for (int c = 0; c < HX_CELLS; c++) stones += t.p.kind[c] == HX_BLOCK, locks += t.p.kind[c] == HX_LOCK;
            if (ch == 2) CHECK(stones > 0);
            if (ch == 3) CHECK(locks > 0);
        }
        for (int k2 = 0; k2 < lv; k2++)
            CHECK(strcmp(hx_levels[k2].seq, L->seq) != 0);
    }
    CHECK(hx_levels[0].par == 1 && !hx_level(&g, HX_LEVELS) && !hx_level(&g, -1));

    /* endless: the same game from the same seed; it ends; the hint is quick on the largest board */
    for (int v = 0; v < HX_VARIANTS; v++) {
        double worst = 0;
        for (uint32_t seed = 1; seed <= 6; seed++) {
            hx_endless(&g, v, seed);
            hx_endless(&t, v, seed);
            CHECK(!memcmp(&g.p, &t.p, sizeof g.p));
            uint32_t r = pr_seed(seed);
            int k = 0;
            while (!hx_over(&g) && k < 3000) {
                int s, c;
                clock_t t0 = clock();
                if (!hx_best(&g, 4000, &r, &s, &c)) break;
                double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
                if (ms > worst) worst = ms;
                CHECK(hx_place(&g, s, c));
                k++;
            }
            CHECK(hx_over(&g) && hx_full(&g.p) && !hx_solved(&g) && hx_stars(&g) >= 1);
        }
        CHECK(worst < 50);
        printf("hexstack: endless %d, the slowest hint %.1f ms\n", v, worst);
    }

    if (fails) { printf("hexstack: %d failures\n", fails); return 1; }
    printf("hexstack: all passed\n");
    return 0;
}
