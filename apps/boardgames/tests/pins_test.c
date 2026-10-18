/*
 * Pull the Pins' rules: the text both ways, the physics the same every time (pulls at rest and in the middle of a
 * fall), the things' rules (a gem burnt by lava, a grey ball coloured by a gem or spoiling the cup, lava turned to
 * stone, lava or the wrong colour in a cup, a ball left out), every level: each order it keeps wins, past the first
 * chapter most of them lose the naive way, no two alike; the hints lead to the end; the puzzles made at random (the
 * same from the same seed, solvable, their time).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/pins.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static ptp_world W;

/* a template filled: kinds and counts per box (-1 empty) */
static void fill(ptp_map *m, int t, const int *kinds, const int *cols, const int *counts, const int *cupcol)
{
    CHECK(ptp_from_tpl(m, t, 0, kinds, cols, counts, cupcol));
}

static int run(const ptp_map *m, const char *order, ptp_sim *s)
{
    uint8_t o[PTP_MAXPIN];
    int n = (int)strlen(order);
    for (int i = 0; i < n; i++) o[i] = (uint8_t)(order[i] - '0');
    return ptp_try(m, o, n, s);
}

static int count_kind(const ptp_sim *s, int kind)
{
    int n = 0;
    for (int i = 0; i < s->nb; i++) n += s->b[i].live && s->b[i].kind == kind;
    return n;
}

static void rules(void)
{
    ptp_map m;
    ptp_sim s;
    /* two boxes stacked (pin 0 the upper floor, pin 1 the lower) */
    fill(&m, 1, (int[]){PTP_LAVA, PTP_GEM}, (int[]){0, 0}, (int[]){10, 6}, NULL);
    CHECK(run(&m, "0", &s) == PTP_BURNT);       /* lava on the gems */
    CHECK(run(&m, "10", &s) == PTP_WON);        /* the gems in and at rest: won, the lava still held */
    CHECK(run(&m, "1", &s) == PTP_WON && s.incup == 6 && s.target == 6);   /* won at rest, a pin left */
    fill(&m, 1, (int[]){PTP_GEM, PTP_LAVA}, (int[]){0, 0}, (int[]){6, 10}, NULL);
    CHECK(run(&m, "1", &s) == PTP_LAVA_IN);     /* lava reaching the cup */
    fill(&m, 1, (int[]){PTP_GEM, PTP_DULL}, (int[]){2, 0}, (int[]){6, 5}, NULL);
    CHECK(run(&m, "1", &s) == PTP_GREY_IN);     /* a grey ball reaching the cup */
    CHECK(run(&m, "01", &s) == PTP_WON);        /* the gems fall on them first: coloured */
    CHECK(count_kind(&s, PTP_DULL) == 0 && s.incup == 11);
    for (int i = 0; i < s.nb; i++) CHECK(s.b[i].kind != PTP_GEM || s.b[i].col == 2);
    /* three boxes: gems, water, lava: the water first makes stone of the lava */
    fill(&m, 2, (int[]){PTP_GEM, PTP_WATER, PTP_LAVA}, (int[]){1, 0, 0}, (int[]){5, 14, 12}, NULL);
    CHECK(run(&m, "1", &s) == PTP_PLAYING);
    CHECK(count_kind(&s, PTP_LAVA) == 0 && count_kind(&s, PTP_STONE) == 12);
    CHECK(run(&m, "102", &s) == PTP_WON);
    /* every pin pulled, a ball left out: a box whose floor is a wall */
    CHECK(ptp_parse(&m, "C1- w2800285a w5800585a w285a585a p10c830c8 fg0042a105658"));
    CHECK(run(&m, "0", &s) == PTP_STUCK);
    /* two cups: the right box's gems sent to the left cup by the slanting pin */
    fill(&m, 10, (int[]){PTP_GEM, PTP_GEM}, (int[]){0, 2}, (int[]){4, 4}, (int[]){0, 2});
    CHECK(run(&m, "01", &s) == PTP_WRONG_CUP);
    CHECK(run(&m, "021", &s) == PTP_WON);
}

/* the same world from the same pulls: at rest, and in the middle of a fall (a pull after so many ticks) */
static void same(void)
{
    ptp_map m;
    CHECK(ptp_parse(&m, ptp_levels[100].text));
    for (int rep = 0; rep < 2; rep++) {
        ptp_sim a, b;
        ptp_build(&m, &W, &a);
        ptp_build(&m, &W, &b);
        CHECK(!memcmp(&a, &b, sizeof a));
        for (int i = 0; i < m.np; i++) {
            ptp_pull(&W, &a, i);
            ptp_pull(&W, &b, i);
            for (int t = 0; t < 17 + 23 * rep; t++) { ptp_step(&W, &a); ptp_step(&W, &b); }
        }
        for (int t = 0; t < 600; t++) { ptp_step(&W, &a); ptp_step(&W, &b); }
        CHECK(!memcmp(&a, &b, sizeof a));
    }
}

/* a level without its counts and its mirror (as the maker tells them apart): the template, each box's thing, the gems'
 * colours as a pattern (two cups: the cup of the template's left side or not) */
static void signature(const ptp_map *m, char *sig)
{
    int n = sprintf(sig, "%02d:", m->tpl), first = -1;
    for (int b = 0; b < m->nbox; b++) {
        int k = m->bkind[b], col = 0;
        if (k == PTP_GEM && m->cups == 2) col = m->bcol[b] == m->cupcol[m->mirror] ? 1 : 2;
        else if (k == PTP_GEM) {
            if (first < 0) first = m->bcol[b];
            col = m->bcol[b] == first ? 1 : 2;
        }
        n += sprintf(sig + n, "%c%d", k == 255 ? '-' : "gdwl"[k], col);
    }
}

static void levels(void)
{
    static char sigs[PTP_LEVELS][80];
    int naive_lost = 0, later = 0;
    for (int l = 0; l < PTP_LEVELS; l++) {
        ptp_map m;
        int ok = ptp_parse(&m, ptp_levels[l].text);
        CHECK(ok);
        if (!ok) { printf("  level %d does not read\n", l + 1); continue; }
        char t[PTP_TEXT];
        ptp_text(&m, t);
        CHECK(!strcmp(t, ptp_levels[l].text));
        CHECK(m.nsol > 0);
        for (int k = 0; k < m.nsol; k++) {
            ptp_sim s;
            int e = ptp_try(&m, m.sol[k], m.sollen[k], &s);
            CHECK(e == PTP_WON);
            if (e != PTP_WON) printf("  level %d order %d: %d\n", l + 1, k, e);
        }
        ptp_sim s;
        ptp_build(&m, &W, &s);
        CHECK(s.end == PTP_PLAYING && s.target > 0 && s.nb <= PTP_MAXB);
        if (ptp_chapter_of(l) > 0) {
            uint8_t o[PTP_MAXPIN];
            int n = ptp_naive(&m, o);
            later++;
            naive_lost += ptp_try(&m, o, n, NULL) != PTP_WON;
        }
        signature(&m, sigs[l]);
        for (int j = 0; j < l; j++) CHECK(strcmp(sigs[j], sigs[l]));
        CHECK(l == 0 || ptp_chapter_of(l) >= ptp_chapter_of(l - 1));
    }
    printf("levels: %d of %d past the first chapter lose the naive way\n", naive_lost, later);
    CHECK(naive_lost * 10 >= later * 8);
}

static void hints(void)
{
    for (int l = 0; l < PTP_LEVELS; l += 7) {
        ptp_game *g = malloc(sizeof *g);
        CHECK(ptp_level(g, l));
        int n = 0;
        while (g->s.end == PTP_PLAYING && n++ < 10) CHECK(ptp_script(g, "h"));
        CHECK(g->s.end == PTP_WON && g->hints > 0 && g->restarts == 0);
        free(g);
    }
    ptp_game *g = malloc(sizeof *g);             /* a wrong pull: the hint restarts first */
    CHECK(ptp_level(g, 60));
    uint8_t o[PTP_MAXPIN];
    int n = ptp_naive(&g->m, o);
    for (int i = 0; i < n && g->s.end == PTP_PLAYING; i++) {
        char mv[4] = {'p', (char)('0' + o[i]), 0};
        ptp_script(g, mv);
    }
    if (g->s.end != PTP_PLAYING) {
        CHECK(ptp_hint(g) == -2);
        CHECK(ptp_script(g, "r"));
        CHECK(g->restarts == 1 && g->s.npulled == 0);
    }
    while (g->s.end == PTP_PLAYING && n++ < 20) CHECK(ptp_script(g, "h"));
    CHECK(g->s.end == PTP_WON);
    free(g);
}

static void random_puzzles(void)
{
    ptp_game *a = malloc(sizeof *a), *b = malloc(sizeof *b);
    for (int v = 0; v < PTP_VARIANTS; v++) {
        double worst = 0;
        for (uint32_t seed = 1; seed <= 6; seed++) {
            clock_t c0 = clock();
            ptp_random(a, v, seed);
            double ms = (clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            ptp_random(b, v, seed);
            char ta[PTP_TEXT], tb[PTP_TEXT];
            ptp_text(&a->m, ta);
            ptp_text(&b->m, tb);
            CHECK(!strcmp(ta, tb));
            CHECK(a->m.nsol > 0);
            CHECK(ptp_try(&a->m, a->m.sol[0], a->m.sollen[0], NULL) == PTP_WON);
        }
        printf("random variant %d: the slowest %.0f ms\n", v, worst);
    }
    free(a);
    free(b);
}

int main(void)
{
    rules();
    same();
    levels();
    hints();
    random_puzzles();
    printf("pins: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails != 0;
}
