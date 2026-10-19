/*
 * Metro Lines' rules: the lines' ways (straight, diagonal, a bend; their lengths; the water they cross), the edits
 * (a new line, lengthened at both ends, a station put between two stops, shortened, a loop, taken away; refused
 * without tunnels), a passenger carried to its shape and one changing lines, a loop's one way, the week's gifts and
 * where they go, a station crowding to the end, levels as text both ways; every level read back, its plan winning
 * it, the naive planner's result by the level's band (3 stars on the first levels, failing the hard ones); the same
 * run twice the same; the endless maps (stations on land, apart, in time).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/metrolines.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static mtl_state s;

/* a small city: stations given as x,y,shape (all at once), a river across at y = ry (none: -1) */
static void city(const int *st, int n, int ry, int tunnels)
{
    mtl_level L;
    memset(&L, 0, sizeof L);
    L.w = 10, L.h = 10;
    L.lines0 = 3, L.lines_max = 5, L.tunnels0 = tunnels;
    L.week = 60 * MTL_HZ, L.spawn0 = 100000, L.spawn_min = 100000, L.ramp = 0;   /* no passengers by chance */
    L.seed = 5;
    for (int i = 0; i < n; i++) {
        L.st[i].x = (int8_t)st[3 * i], L.st[i].y = (int8_t)st[3 * i + 1], L.st[i].shape = (int8_t)st[3 * i + 2];
    }
    L.ns = n;
    if (ry >= 0) {
        L.nriver = 1;
        L.rn[0] = 2;
        L.rhw[0] = 2;
        L.rx[0][0] = -8, L.ry[0][0] = (int8_t)(ry * 4), L.rx[0][1] = 48, L.ry[0][1] = (int8_t)(ry * 4);
    }
    mtl_begin(&s, &L);
    s.next_spawn = 0x7fffffff;
}

static unsigned hash(const mtl_state *x)
{
    unsigned h = 2166136261u;
    h = (h ^ (unsigned)x->tick) * 16777619u;
    h = (h ^ (unsigned)x->delivered) * 16777619u;
    for (int i = 0; i < MTL_MAXS; i++) h = (h ^ (unsigned)(x->s[i].nwait * 31 + x->s[i].crowd)) * 16777619u;
    for (int i = 0; i < MTL_MAXL; i++) h = (h ^ (unsigned)(x->t[i].pos + x->t[i].k * 7 + x->t[i].nride * 131)) * 16777619u;
    return h;
}

int main(void)
{
    clock_t t0 = clock();
    /* ---- the ways ---- */
    {
        static const int C[] = {1, 1, MTL_CIRCLE, 5, 1, MTL_TRIANGLE, 4, 4, MTL_SQUARE, 1, 7, MTL_CIRCLE};
        city(C, 4, -1, 0);
        mtl_pt p[3];
        CHECK(mtl_route(&s, 0, 1, 0, p) == 2 && mtl_path_len(p, 2) == 4 * MTL_FP);   /* straight across */
        CHECK(mtl_route(&s, 0, 2, 0, p) == 2 && mtl_path_len(p, 2) == 3 * MTL_DIAG); /* a diagonal */
        int n = mtl_route(&s, 0, 3, 0, p);
        CHECK(n == 2 && p[0].x == MTL_FP && p[1].y == 7 * MTL_FP);
        n = mtl_route(&s, 1, 3, 0, p);          /* (5,1) to (1,7): a diagonal of 4, then 2 down */
        CHECK(n == 3 && mtl_path_len(p, 3) == 4 * MTL_DIAG + 2 * MTL_FP);
        CHECK(p[0].x == 5 * MTL_FP && p[2].y == 7 * MTL_FP);
        n = mtl_route(&s, 3, 1, 0, p);          /* the same way back */
        CHECK(n == 3 && p[0].x == MTL_FP && p[0].y == 7 * MTL_FP && p[2].x == 5 * MTL_FP);
    }
    /* ---- water ---- */
    {
        static const int C[] = {2, 1, MTL_CIRCLE, 2, 7, MTL_TRIANGLE, 7, 2, MTL_SQUARE};
        city(C, 3, 4, 0);
        CHECK(mtl_wet(&s, 2 * MTL_FP, 4 * MTL_FP));
        CHECK(!mtl_wet(&s, 2 * MTL_FP, 3 * MTL_FP));
        mtl_pt p[3];
        int n = mtl_route(&s, 0, 1, 0, p), c;
        CHECK(mtl_crossings(&s, p, n) == 1);
        mtl_best_bend(&s, 0, 2, &c);
        CHECK(c == 0);
        int ab[2] = {0, 1};
        CHECK(!mtl_new_line(&s, ab, 2));        /* no tunnel */
        s.tunnels = 1;
        CHECK(mtl_new_line(&s, ab, 2) && mtl_tunnels_used(&s) == 1);
        CHECK(!mtl_extend(&s, 0, 0, 2) || mtl_tunnels_used(&s) <= 1);
    }
    /* ---- the edits ---- */
    {
        static const int C[] = {1, 1, MTL_CIRCLE, 4, 1, MTL_TRIANGLE, 7, 1, MTL_SQUARE, 4, 4, MTL_CIRCLE,
                                7, 4, MTL_TRIANGLE};
        city(C, 5, -1, 0);
        int ab[2] = {0, 1};
        CHECK(mtl_new_line(&s, ab, 2) && s.l[0].n == 2);
        CHECK(mtl_extend(&s, 0, 1, 2) && s.l[0].n == 3 && s.l[0].stop[2] == 2);
        CHECK(!mtl_extend(&s, 0, 1, 1));         /* on it already */
        CHECK(mtl_extend(&s, 0, 0, 3) && s.l[0].stop[0] == 3 && s.l[0].n == 4);
        CHECK(mtl_insert(&s, 0, 2, 4) && s.l[0].n == 5 && s.l[0].stop[3] == 4);
        CHECK(mtl_cut(&s, 0, 1) && s.l[0].n == 4 && s.l[0].stop[3] == 4);
        CHECK(mtl_cut(&s, 0, 0) && s.l[0].n == 3 && s.l[0].stop[0] == 0);
        CHECK(mtl_extend(&s, 0, 1, 0) && s.l[0].loop);   /* back to its first stop: a loop */
        mtl_line keep[MTL_MAXL];
        memcpy(keep, s.l, sizeof keep);
        CHECK(mtl_remove(&s, 0) && s.l[0].n == 0 && mtl_free_line(&s) == 0);
        CHECK(mtl_set_lines(&s, keep) && s.l[0].n == 3 && s.l[0].loop);
        int three[3] = {2, 3, 4};
        CHECK(mtl_do(&s, "n3-4-5") && s.l[1].n == 3);
        (void)three;
        CHECK(mtl_do(&s, "x2") && s.l[1].n == 0);
        CHECK(!mtl_do(&s, "e9t-1") && !mtl_do(&s, "n1"));
    }
    /* ---- a passenger carried; one changing lines; a loop one way ---- */
    {
        static const int C[] = {1, 1, MTL_CIRCLE, 5, 1, MTL_TRIANGLE, 5, 5, MTL_SQUARE};
        city(C, 3, -1, 0);
        CHECK(mtl_do(&s, "n1-2") && mtl_do(&s, "n2-3"));
        s.s[0].wait[s.s[0].nwait++] = MTL_SQUARE;   /* at the circle, for the square: two lines */
        s.s[0].wait[s.s[0].nwait++] = MTL_TRIANGLE;
        CHECK(s.dist[MTL_SQUARE][0] == 2 && s.dist[MTL_TRIANGLE][0] == 1);
        mtl_do(&s, "w30");
        CHECK(s.delivered == 2);
        city(C, 3, -1, 0);
        CHECK(mtl_do(&s, "n1-2-3-1") && s.l[0].loop);
        CHECK(s.dist[MTL_CIRCLE][1] == 2 && s.dist[MTL_CIRCLE][2] == 1);   /* round one way only */
        s.s[1].wait[s.s[1].nwait++] = MTL_CIRCLE;
        mtl_do(&s, "w40");
        CHECK(s.delivered == 1 && s.s[1].nwait == 0);
    }
    /* ---- a station crowding to the end; the week's gifts ---- */
    {
        static const int C[] = {1, 1, MTL_CIRCLE, 5, 1, MTL_TRIANGLE, 5, 5, MTL_SQUARE};
        city(C, 3, -1, 0);
        for (int i = 0; i < MTL_ROOM + 2; i++) s.s[2].wait[s.s[2].nwait++] = MTL_CIRCLE;
        mtl_do(&s, "w29");
        CHECK(!s.over && s.s[2].crowd > 0);
        mtl_do(&s, "w2");
        CHECK(s.over && s.over_st == 2 && mtl_ended(&s));
        CHECK(!mtl_do(&s, "n1-2"));             /* over: nothing more */
        city(C, 3, -1, 0);
        s.L.week = 10;
        s.next_week = 10;
        mtl_do(&s, "n1-2");
        mtl_do(&s, "w2");
        CHECK(s.choosing && !mtl_running(&s) && s.tick == 10);
        int g = s.opt[0];
        CHECK(mtl_choose(&s, 0));
        if (g == MTL_UP_CAR) CHECK(s.placing == MTL_UP_CAR && mtl_place(&s, 0) && s.t[0].cars == 2);
        else if (g == MTL_UP_INTER) CHECK(s.placing == MTL_UP_INTER && mtl_place(&s, 1) && s.s[1].inter);
        else if (g == MTL_UP_LINE) CHECK(s.lines == 4);
        CHECK(mtl_running(&s));
    }
    /* ---- levels as text, read back; plans win; the naive planner by band ---- */
    {
        int naive3 = 0, naive_fail = 0;
        for (int lv = 0; lv < MTL_LEVELS; lv++) {
            mtl_level L, M;
            char t[4096], t2[4096];
            CHECK(mtl_level_get(lv, &L));
            CHECK(mtl_text(&L, t, sizeof t));
            CHECK(mtl_parse(t, &M) && mtl_text(&M, t2, sizeof t2) && !strcmp(t, t2));
            CHECK(L.goal[0] > 0 && L.goal[0] < L.goal[1] && L.goal[1] < L.goal[2]);
            mtl_begin(&s, &L);
            for (int i = 0; i < L.ns; i++) {    /* stations on land, apart */
                CHECK(!mtl_wet(&s, L.st[i].x * MTL_FP, L.st[i].y * MTL_FP));
                for (int j = 0; j < i; j++)
                    CHECK(L.st[i].x != L.st[j].x || L.st[i].y != L.st[j].y);
            }
            mtl_script(&s, mtl_plans[lv]);
            while (!mtl_ended(&s) && mtl_running(&s)) mtl_step(&s);
            if (!s.won) printf("level %d: its plan did not win (%d of %d)\n", lv + 1, s.delivered, L.goal[2]);
            CHECK(s.won && mtl_stars(&s) == 3);
            unsigned h = hash(&s);
            mtl_begin(&s, &L);                  /* the same run again: the same */
            mtl_script(&s, mtl_plans[lv]);
            while (!mtl_ended(&s) && mtl_running(&s)) mtl_step(&s);
            CHECK(hash(&s) == h);
            int d = mtl_bot_run(&L, 1, 15 * 60 * MTL_HZ, NULL, 0, NULL);
            if (lv < 3) { CHECK(d >= L.goal[2]); naive3 += d >= L.goal[2]; }
            else CHECK(d < L.goal[2]);          /* from level 4: never 3 stars the naive way */
            if (lv >= 12) { CHECK(d < L.goal[0]); naive_fail += d < L.goal[0]; }
        }
        printf("levels: the naive planner 3 stars on %d of the first 3, fails %d of the last %d\n", naive3, naive_fail,
               MTL_LEVELS - 12);
        for (int ch = 0; ch < MTL_CHAPTERS; ch++) {
            mtl_level L;
            CHECK(mtl_parse(mtl_icons[ch], &L));
        }
        CHECK(mtl_chapter_of(0) == 0 && mtl_chapter_of(MTL_LEVELS - 1) == MTL_CHAPTERS - 1);
    }
    /* ---- the endless maps ---- */
    {
        clock_t c0 = clock();
        for (int v = 0; v < MTL_VARIANTS; v++)
            for (uint32_t seed = 1; seed <= 20; seed++) {
                mtl_level L;
                mtl_variant_level(v, seed, &L);
                CHECK(L.ns >= 9 && L.goal[2] == 0);
                mtl_begin(&s, &L);
                for (int i = 0; i < L.ns; i++) {
                    CHECK(!mtl_wet(&s, L.st[i].x * MTL_FP, L.st[i].y * MTL_FP));
                    CHECK(L.st[i].x >= 1 && L.st[i].x <= L.w - 2 && L.st[i].y >= 1 && L.st[i].y <= L.h - 2);
                    if (i) CHECK(L.st[i].tick >= L.st[i - 1].tick);
                }
                CHECK(L.st[0].shape == MTL_CIRCLE && L.st[1].shape == MTL_TRIANGLE && L.st[2].shape == MTL_SQUARE);
            }
        double ms = 1000.0 * (double)(clock() - c0) / CLOCKS_PER_SEC / 60;
        printf("an endless map made and begun in %.1f ms\n", ms);
        CHECK(ms < 300);
        mtl_level L;                            /* a whole endless run by the planner: over in the end */
        mtl_variant_level(1, 3, &L);
        int t, d = mtl_bot_run(&L, 1, 20 * 60 * MTL_HZ, NULL, 0, &t);
        CHECK(d > 50 && t < 20 * 60 * MTL_HZ);
        CHECK(mtl_endless_stars(1, d) >= 1);
    }
    printf("metrolines: %s (%.1f s)\n", fails ? "FAILED" : "ok", (double)(clock() - t0) / CLOCKS_PER_SEC);
    return fails ? 1 : 0;
}
