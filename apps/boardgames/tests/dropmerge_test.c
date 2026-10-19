/*
 * Fruit Drop's rules: the physics (fruits stay in the jar, settle, do not sink into each other; the same game twice
 * comes out the same, stepped tick by tick or the clock jumped at rest), merges and the score, the rim's end, whole
 * endless games from seeds, every level (its data sound, its fruits settled apart, the maker's way replayed: the goal
 * reached in exactly par drops, within its fruits), the notation, and the time a tick takes with a full jar.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/dropmerge.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static dmj_game G, H;

/* every fruit inside the glass, none deep in another (a small fruit squeezed under a big pile sinks a few units) */
static int sane(const dmj_world *w)
{
    for (int i = 0; i < w->nb; i++) {
        const dmj_ball *a = &w->b[i];
        if (a->x - a->r < -DMJ_FP || a->x + a->r > (w->J.width + 1) * DMJ_FP) return 0;
        if (a->y + a->r > (DMJ_JH + 1) * DMJ_FP) return 0;
        for (int j = i + 1; j < w->nb; j++) {
            const dmj_ball *b = &w->b[j];
            int64_t dx = b->x - a->x, dy = b->y - a->y, rs = a->r + b->r - 6 * DMJ_FP;
            if (rs > 0 && dx * dx + dy * dy < rs * rs) return 0;
        }
    }
    return 1;
}

static void test_merge(void)
{
    dmj_jar j;
    CHECK(dmj_parse(&j, DMJ_SIZE, 3, 0, 400, "11", "", ""));
    dmj_world w;
    dmj_build(&w, &j);
    CHECK(w.cur == 1 && w.next == 1 && w.rest);
    CHECK(dmj_drop(&w, 200 * DMJ_FP));
    CHECK(!dmj_drop(&w, 200 * DMJ_FP));         /* the dropper is not ready yet */
    dmj_settle(&w, 600);
    CHECK(w.nb == 1 && w.drops == 1 && w.score == 0);
    while (!dmj_can_drop(&w)) dmj_step(&w);
    CHECK(dmj_drop(&w, 204 * DMJ_FP));
    dmj_settle(&w, 900);
    if (w.end != DMJ_OUT) printf("rest %d end %d cur %d tick %d nb %d\n", w.rest, w.end, w.cur, w.tick, w.nb);
    CHECK(w.nb == 1 && w.b[0].size == 2 && w.score == 1 && w.made[2] == 1 && w.biggest == 2);
    CHECK(w.cur == 0 && w.end == DMJ_OUT);      /* fruits used up at rest, goal (a grape) not made */
    /* two of the biggest: gone */
    CHECK(dmj_parse(&j, DMJ_SCORE, 66, 0, 400, "1", "", "11 110 360 11 300 360"));
    dmj_build(&w, &j);
    CHECK(w.nb == 2);
    w.b[1].x = w.b[0].x + w.b[0].r * 2;        /* pushed together */
    w.rest = 0;
    dmj_step(&w);
    CHECK(w.nb == 0 && w.score == 66 && w.end == DMJ_WON);
}

static void test_physics(void)
{
    /* the same endless game twice: the same; one stepped tick by tick, one with the clock run in jumps */
    for (int v = 0; v < DMJ_VARIANTS; v++) {
        dmj_endless(&G, v, 77u + (unsigned)v);
        dmj_endless(&H, v, 77u + (unsigned)v);
        uint32_t r1 = 5, r2 = 5;
        for (int d = 0; d < 40 && G.w.end == DMJ_PLAYING; d++) {
            dmj_play(&G.w, NULL, 0, 150, &r1, 1, NULL);
            dmj_play(&H.w, NULL, 0, 150, &r2, 1, NULL);
            for (int t = 0; t < 130; t++) dmj_step(&G.w);
            dmj_run(&H.w, 130);
            CHECK(G.w.tick == H.w.tick);
        }
        CHECK(G.w.nb == H.w.nb && G.w.score == H.w.score && !memcmp(G.w.b, H.w.b, sizeof G.w.b[0] * G.w.nb));
        CHECK(sane(&G.w));
    }
    /* whole games from seeds: they end by the rim, the score is the merges', the jar sane at every rest */
    long drops = 0;
    for (int s = 1; s <= 12; s++) {
        dmj_endless(&G, s % DMJ_VARIANTS, (uint32_t)s * 1013u);
        uint32_t rs = pr_seed((uint32_t)s);
        int bad = 0;
        while (G.w.end == DMJ_PLAYING && G.w.drops < 600) {
            dmj_play(&G.w, NULL, 0, 250, &rs, 1, NULL);
            dmj_settle(&G.w, DMJ_SETTLE);
            if (G.w.rest && !sane(&G.w)) bad++;
        }
        CHECK(bad == 0);
        CHECK(G.w.end == DMJ_OVERFLOW);
        int pts = 0;
        for (int k = 2; k <= DMJ_SIZES; k++) pts += G.w.made[k] * (k - 1) * k / 2;
        CHECK(pts <= G.w.score && G.w.score - pts <= 66 * 20 && (G.w.score - pts) % 66 == 0);
        CHECK(G.w.biggest >= 7);
        drops += G.w.drops;
        CHECK(dmj_stars(&G) >= 1);
    }
    printf("dropmerge: 12 endless games, %ld drops\n", drops);
}

static void test_levels(void)
{
    int chapters[DMJ_CHAPTERS] = {0};
    for (int L = 0; L < DMJ_LEVELS; L++) {
        const dmj_level_def *d = &dmj_levels[L];
        dmj_jar j;
        CHECK(dmj_load(&j, d));
        CHECK(d->par >= 1 && d->par <= j.nseq);
        CHECK(dmj_level(&G, L));
        CHECK(G.w.end == DMJ_PLAYING && dmj_goal_left(&G.w) > 0);
        chapters[dmj_chapter_of(L)]++;
        int xs[DMJ_SEQ], n = 0;
        for (const char *s = d->way; *s && n < DMJ_SEQ;) {
            while (*s == ' ') s++;
            if (!*s) break;
            xs[n++] = atoi(s);
            while (*s && *s != ' ') s++;
        }
        CHECK(n == d->par);
        int e = dmj_play(&G.w, xs, n, 0, NULL, n, NULL);
        if (e != DMJ_WON) {
            while (G.w.end == DMJ_PLAYING && !G.w.rest) dmj_step(&G.w);
            e = G.w.end;
        }
        if (e != DMJ_WON || G.w.drops != d->par) printf("level %d: end %d after %d drops (par %d)\n", L + 1, e, G.w.drops, d->par);
        CHECK(e == DMJ_WON && G.w.drops == d->par);
        CHECK(dmj_stars(&G) == 3);
        dmj_restart(&G);
        CHECK(G.w.drops == 0 && G.w.end == DMJ_PLAYING && G.restarts == 1);
    }
    for (int c = 0; c < DMJ_CHAPTERS; c++) CHECK(chapters[c] == DMJ_LEVELS / DMJ_CHAPTERS);
    CHECK(!dmj_level(&G, DMJ_LEVELS) && !dmj_level(&G, -1));
}

static void test_script(void)
{
    dmj_endless(&G, 0, 9);
    CHECK(dmj_script(&G, "d100 d100 d300 w60"));
    CHECK(G.w.drops == 3);
    CHECK(dmj_script(&G, "=5"));
    CHECK(G.w.drops == 8);
    CHECK(dmj_script(&G, "r") && G.w.drops == 0);
    CHECK(!dmj_script(&G, "x"));
    CHECK(dmj_script(&G, "="));
    CHECK(G.w.end == DMJ_OVERFLOW);
    CHECK(!dmj_script(&G, "d200"));
}

static void test_speed(void)
{
    dmj_jar j;
    CHECK(dmj_parse(&j, DMJ_SCORE, 99999, 0, 400, "1", "", ""));
    dmj_world w;
    dmj_build(&w, &j);
    w.still = 1;
    for (int r = 0; r < 10; r++)                /* a full jar: 60 fruits in rows, no two of a kind side by side */
        for (int c = 0; c < 6; c++) {
            dmj_ball *b = &w.b[w.nb++];
            memset(b, 0, sizeof *b);
            b->size = (uint8_t)((r * 2 + c) % 4 + 1);
            b->r = dmj_radius[b->size] * DMJ_FP;
            b->x = b->px = (34 + c * 64 + (r & 1) * 8) * DMJ_FP;
            b->y = b->py = (DMJ_JH - 32 - r * 64) * DMJ_FP;
            b->born = -1;
            b->id = (uint16_t)(w.nb + 100);
        }
    clock_t c0 = clock();
    int ticks = 0;
    for (; ticks < 1200; ticks++) {
        w.rest = 0;
        w.end = DMJ_PLAYING;                    /* (above the rim: kept playing) */
        dmj_step(&w);
    }
    double ms = (double)(clock() - c0) * 1000 / CLOCKS_PER_SEC / ticks;
    printf("dropmerge: %d fruits, %.3f ms a tick\n", w.nb, ms);
    CHECK(ms < 0.5);
    CHECK(sane(&w));
}

int main(void)
{
    test_merge();
    test_physics();
    test_levels();
    test_script();
    test_speed();
    if (fails) {
        printf("dropmerge: %d FAILED\n", fails);
        return 1;
    }
    printf("dropmerge: all passed\n");
    return 0;
}
