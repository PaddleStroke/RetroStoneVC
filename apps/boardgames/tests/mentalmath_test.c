/*
 * Mental Math's rules: the sums (true, whole, nothing below zero), the 150 designed levels (each read back, its sums
 * true and of its chapter, none twice, no answer twice in a row, par from its time), drills made for every variant
 * from many seeds, the typing (right, wrong, a lone 0, taking back), hints, the stars.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mentalmath.h"
#include "games/mentalmath_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void drill_ok(const mmh_game *g)
{
    CHECK(g->nq >= 8 && g->nq <= MMH_MAXQ && g->par >= g->nq);
    for (int i = 0; i < g->nq; i++) {
        const mmh_q *q = &g->q[i];
        CHECK(mmh_check(q));
        CHECK(mmh_answer(q) >= 0 && mmh_answer(q) <= 144);
        for (int j = 0; j < i; j++) CHECK(!(g->q[j].op == q->op && g->q[j].n[0] == q->n[0] && g->q[j].n[1] == q->n[1]));
        if (i) CHECK(mmh_answer(&g->q[i - 1]) != mmh_answer(q));
        if (q->op == MMH_MUL || q->op == MMH_DIV) CHECK(q->n[1] >= 1 && q->n[1] <= 12);
    }
}

static void test_levels(void)
{
    static const unsigned OPS[MMH_CHAPTERS] = {1u << MMH_ADD, 1u << MMH_ADD | 1u << MMH_SUB,
                                               1u << MMH_ADD | 1u << MMH_SUB, 1u << MMH_MUL, 1u << MMH_DIV, 0x1e};
    for (int i = 0; i < MMH_LEVELS; i++) {
        mmh_game g;
        int ok = mmh_decode(mmh_levels[i], &g);
        CHECK(ok);
        if (!ok) continue;
        drill_ok(&g);
        char s[1024];
        CHECK(mmh_encode(&g, s, sizeof s) > 0 && !strcmp(s, mmh_levels[i]));
        int ch = i / 25;
        for (int k = 0; k < g.nq; k++) {
            CHECK(OPS[ch] >> g.q[k].op & 1);
            if (ch < 2) CHECK(g.q[k].n[0] <= 20 && g.q[k].n[1] <= 20 && g.q[k].n[2] <= 20);   /* within twenty */
            if (i < 5) CHECK(g.q[k].n[2] <= 5 && g.q[k].miss == 2);                         /* a gentle start */
            if (ch < 3) CHECK(g.q[k].n[0] <= 100 && g.q[k].n[2] <= 100);   /* to a hundred */
        }
        for (int j = 0; j < i; j++) CHECK(strcmp(mmh_levels[j], mmh_levels[i]));
    }
}

static void test_variants(void)
{
    for (int v = 0; v < MMH_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 200; seed++) {
            mmh_spec sp = mmh_variant_spec(v);
            mmh_game g;
            mmh_drill(&sp, seed, &g);
            drill_ok(&g);
            CHECK(g.nq == sp.count);
            for (int k = 0; k < g.nq; k++) CHECK(sp.ops >> g.q[k].op & 1);
            mmh_game h;                         /* the same seed, the same drill */
            mmh_drill(&sp, seed, &h);
            CHECK(!memcmp(&g, &h, sizeof g));
        }
}

static void test_typing(void)
{
    mmh_game g;
    CHECK(mmh_decode("20:7+5=?12,?3*4=12,9-9=?0,36/?4=9", &g));
    CHECK(g.nq == 4 && g.par == 20 && g.q[1].miss == 0 && g.q[3].op == MMH_DIV && mmh_answer(&g.q[3]) == 4);
    CHECK(!mmh_decode("20:7+5=?13", &g));      /* false */
    CHECK(!mmh_decode("20:7+?5=?12", &g));     /* two missing */
    CHECK(!mmh_decode("20:3-5=?0", &g));
    CHECK(mmh_decode("20:7+5=?12,?3*4=12,9-9=?0,36/?4=9", &g));
    CHECK(mmh_type(&g, 1) == MMH_MORE && mmh_back(&g) && !mmh_back(&g));
    CHECK(mmh_type(&g, 1) == MMH_MORE && mmh_type(&g, 3) == MMH_WRONG);   /* 13 */
    CHECK(g.mistakes == 1 && g.cur == 0 && g.ndig == 0);
    CHECK(mmh_type(&g, 0) == MMH_MORE && mmh_type(&g, 1) == MMH_MORE);    /* the 0 gives way */
    CHECK(mmh_type(&g, 2) == MMH_RIGHT && g.cur == 1);
    CHECK(mmh_type(&g, 3) == MMH_RIGHT);       /* ? x 4 = 12 */
    CHECK(mmh_type(&g, 0) == MMH_RIGHT);       /* 9 - 9 = 0 */
    CHECK(mmh_hint(&g) == MMH_RIGHT && g.hints == 1 && mmh_done(&g));
    CHECK(!mmh_hint(&g) && mmh_type(&g, 1) == MMH_MORE);
    CHECK(mmh_stars(&g, 150) == 2);             /* in time, two slips */
    g.hints = g.mistakes = 0;
    CHECK(mmh_stars(&g, 200) == 3 && mmh_stars(&g, 300) == 2 && mmh_stars(&g, 301) == 1);
    CHECK(mmh_decode(mmh_levels[0], &g) && mmh_script(&g, "+99") && mmh_done(&g) && !g.mistakes);
    CHECK(mmh_decode(mmh_levels[149], &g) && mmh_script(&g, "= h +3") && g.cur == 5 && g.hints == 1);
}

static void test_clock(void)
{
    mmh_clock c;
    memset(&c, 0, sizeof c);
    CHECK(mmh_clock_ms(&c, 5000, 5000) == 0);  /* not started */
    mmh_clock_start(&c, 1000, 1000);
    for (uint32_t t = 1000; t <= 3000; t += 16) mmh_clock_frame(&c, t, t);
    CHECK(mmh_clock_ms(&c, 3000, 3000) == 2000);
    CHECK(mmh_clock_ms(&c, 3200, 3200) == 2200);   /* between frames */
    CHECK(mmh_clock_ms(&c, 63000, 63000) == 2000); /* away a minute: not counted */
    mmh_clock_frame(&c, 63000, 63000);             /* back on the screen */
    CHECK(mmh_clock_ms(&c, 63000, 63000) == 2000);
    mmh_clock_frame(&c, 63100, 63100);
    mmh_clock_stop(&c, 63200, 63200);
    CHECK(mmh_clock_ms(&c, 99000, 99000) == 2200);   /* stopped */
    mmh_clock_start(&c, 10, 10);                  /* the picture mode: its clock jumps, the real time does not */
    mmh_clock_frame(&c, 60010, 20);
    CHECK(mmh_clock_ms(&c, 60010, 20) == 60000);
    /* a game kept, played again on its own clock (fast: the real time barely moves), landing 10 minutes on, then
     * back on the real clock (2^30 ms later): only the drill's own time counts */
    uint32_t r0 = 1000000u - 0x40000000u;
    mmh_clock_start(&c, r0, 500);
    for (uint32_t t = r0; t != r0 + 4000; t += 50) mmh_clock_frame(&c, t, 501);
    mmh_clock_frame(&c, r0 + 9000, 502);         /* a pause between two taps: counted */
    CHECK(mmh_clock_ms(&c, r0 + 9000, 502) == 9000);
    mmh_clock_frame(&c, r0 + 609000, 503);       /* the record's end: every animation landed */
    CHECK(mmh_clock_ms(&c, r0 + 609000, 503) == 9000);
    mmh_clock_frame(&c, 1000000u, 520);          /* the app's clock again */
    mmh_clock_frame(&c, 1000016u, 536);
    CHECK(mmh_clock_ms(&c, 1000016u, 536) == 9016);
}

int main(void)
{
    test_typing();
    test_clock();
    test_levels();
    test_variants();
    if (fails) {
        printf("mentalmath: %d failures\n", fails);
        return 1;
    }
    printf("mentalmath: ok\n");
    return 0;
}
