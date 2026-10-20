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

/* every fruit sound: a size, a radius, a turn in range; the last events in range */
static int whole(const dmj_world *w)
{
    if (w->nb < 0 || w->nb > DMJ_MAXB) return 0;
    for (int i = 0; i < w->nb; i++) {
        const dmj_ball *b = &w->b[i];
        if (b->size < 1 || b->size > DMJ_SIZES || b->r <= 0 || b->r > dmj_radius[DMJ_SIZES] * DMJ_FP) return 0;
        if (b->ang < -32768 || b->ang > 65535) return 0;
    }
    for (int i = w->nev > DMJ_EVENTS ? w->nev - DMJ_EVENTS : 0; i < w->nev; i++) {
        const dmj_event *e = &w->ev[i % DMJ_EVENTS];
        if (e->size < 1 || e->size > DMJ_SIZES || e->type > DMJ_E_GONE) return 0;
    }
    return 1;
}

/* the crash hunt's cases (the owner's phone, fruits merging): fruits at exactly the same place (no distance between
 * their centres), the biggest ones merging in pairs (both gone) at the walls, on the floor and on pegs, chains, fruits
 * crammed past what the jar holds, a long game rolling (the turn kept in range), the clock jumped; then random jars
 * with drops at random times and places (a fuzz kept short here; the full one ran with ASan and UBSan, and on 32-bit
 * ARM) */
static void test_hard(void)
{
    dmj_jar j;
    dmj_world w;
    /* the hang found by the hunt: level 1, a mandarin then a cherry dropped at the same place (x 200): the cherry rests
     * exactly on top, at exactly the touching distance, was never seen touching, stayed "just dropped" and the jar never
     * came to rest (the pictures' third drop waited for ever; the app stepped every tick of any gap) */
    CHECK(dmj_level(&G, 0));
    int at = 200;
    for (int d = 0; d < 2; d++) {
        dmj_play(&G.w, &at, 1, 0, NULL, 1, NULL);
        dmj_settle(&G.w, 2000);
        CHECK(G.w.rest);
    }
    CHECK(G.w.nb == 2 && !G.w.b[0].fresh && !G.w.b[1].fresh && G.w.b[1].x == G.w.b[0].x);
    for (int t = 0; t < 4 * DMJ_HZ && !(dmj_can_drop(&G.w) && G.w.rest); t++) dmj_step(&G.w);
    CHECK(dmj_can_drop(&G.w) && G.w.rest);
    /* the same place twice, and twice again: pushed apart without a division by zero, then merged */
    CHECK(dmj_parse(&j, DMJ_SCORE, 9999, 0, 280, "5555", "140 300 12", "11 140 200 11 140 200 10 106 300 10 106 300 1 140 100 1 140 100"));
    dmj_build(&w, &j);
    CHECK(whole(&w));
    w.rest = 0;                                 /* (woken: the jar is built at rest) */
    for (int t = 0; t < 600; t++) dmj_step(&w);
    CHECK(whole(&w) && w.score >= 66);
    /* drops onto one spot as fast as the dropper allows, at both walls: chains, watermelons gone */
    CHECK(dmj_parse(&j, DMJ_SCORE, 999999, 0, 280, "111111111111111111111111111111111111111111111111111111111111", "", ""));
    for (int side = 0; side < 3; side++) {
        dmj_build(&w, &j);
        for (int d = 0; d < 60; d++) {
            while (!dmj_can_drop(&w) && w.end == DMJ_PLAYING) dmj_step(&w);
            dmj_drop(&w, side == 0 ? 0 : side == 1 ? 280 * DMJ_FP : 140 * DMJ_FP);
            CHECK(whole(&w));
        }
        dmj_settle(&w, 2000);
        CHECK(whole(&w) && w.best_chain >= 2);
    }
    /* long endless games (many merges, the clock jumping now and then), their fruits rolling all the while */
    uint32_t rs = 7;
    for (int v = 0; v < DMJ_VARIANTS; v++) {
        dmj_endless(&G, v, 4242u + (unsigned)v);
        for (int d = 0; d < 400 && G.w.end == DMJ_PLAYING; d++) {
            dmj_play(&G.w, NULL, 0, 120, &rs, 1, NULL);
            if (d % 37 == 0) dmj_run(&G.w, 100000);
            CHECK(whole(&G.w));
        }
    }
    /* random jars, random drops at random times */
    int bad = 0;
    for (int s = 1; s <= 60; s++) {
        rs = pr_seed((uint32_t)s);
        char seq[DMJ_SEQ + 1], pegs[256] = "", pre[512] = "";
        int L = 1 + pr_below(&rs, DMJ_SEQ);
        for (int i = 0; i < L; i++) seq[i] = (char)('1' + pr_below(&rs, 5));
        seq[L] = 0;
        int width = 232 + pr_below(&rs, 169), np = pr_below(&rs, DMJ_MAXPEG + 1), nq = pr_below(&rs, DMJ_MAXPRE + 1);
        char *p = pegs;
        for (int i = 0; i < np; i++) p += sprintf(p, "%d %d %d ", pr_below(&rs, width + 1), pr_below(&rs, DMJ_JH + 1), 1 + pr_below(&rs, 30));
        p = pre;
        for (int i = 0; i < nq; i++) p += sprintf(p, "%d %d %d ", 1 + pr_below(&rs, 11), pr_below(&rs, width + 1), pr_below(&rs, DMJ_JH + 1));
        CHECK(dmj_parse(&j, 1 + pr_below(&rs, 4), 1 + pr_below(&rs, 5000), 1 + pr_below(&rs, 11), width, seq, pegs, pre));
        dmj_build(&w, &j);
        for (int k = 0; k < 400 && w.end == DMJ_PLAYING; k++) {
            if (pr_below(&rs, 2)) dmj_drop(&w, pr_below(&rs, width * DMJ_FP + 1));
            for (int t = pr_below(&rs, 30); t > 0; t--) dmj_step(&w);
            if (pr_below(&rs, 40) == 0) dmj_run(&w, pr_below(&rs, 100000));
            if (!whole(&w)) bad++;
        }
    }
    CHECK(bad == 0);
}

int main(void)
{
    test_merge();
    test_physics();
    test_levels();
    test_script();
    test_hard();
    test_speed();
    if (fails) {
        printf("dropmerge: %d FAILED\n", fails);
        return 1;
    }
    printf("dropmerge: all passed\n");
    return 0;
}
