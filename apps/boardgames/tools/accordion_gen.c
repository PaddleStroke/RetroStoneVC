/*
 * Accordion's levels: src/games/accordion_levels.h. Deals shuffled from fixed seeds (a run always makes the same
 * levels), each won down to one pile by the rules' search (accordion_rules.c: acd_solve), its win kept with it (the
 * tests play it); graded by plain games (100 games of random legal moves: the fewer piles they leave on average, the
 * easier the deal), chosen evenly along that grade, easiest first.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/accordion_gen apps/boardgames/tools/accordion_gen.c \
 *       apps/boardgames/src/games/accordion_rules.c && /tmp/accordion_gen > apps/boardgames/src/games/accordion_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/accordion.h"

#define LEVELS 150
#define CHAPTERS 6
#define BUDGET 400000

typedef struct {
    uint8_t deck[52];
    double grade;
    long nodes;
    char win[3 * 52 + 1];
} cand;

static uint32_t rs = 1;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

/* the piles a game of random legal moves leaves, on average over `tries` */
static double plain(const solrec *r, int tries, uint32_t seed)
{
    static solrec w;
    static solmove list[2 * ACD_PILES];
    double sum = 0;
    rs = seed ? seed : 1;
    for (int t = 0; t < tries; t++) {
        w = *r;
        for (;;) {
            int n = acd_search.cands(&w.pos, list);
            if (!n) break;
            acd_play(&w, list[rnd() % (uint32_t)n]);
        }
        sum += acd_piles(&w.pos);
    }
    return sum / tries;
}

static int by_grade(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    if (x->grade != y->grade) return x->grade < y->grade ? -1 : 1;
    return x->nodes < y->nodes ? -1 : x->nodes > y->nodes;
}

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1, tries = 1200 / scale, want = LEVELS / scale;
    static cand c[2000];
    static solrec r;
    solmove path[ACD_PILES];
    int nc = 0;
    for (int i = 0; i < tries && nc < 2000; i++) {
        uint8_t deck[104];
        cards_pack(PACK_52, deck);
        cards_shuffle(deck, 52, 0xacc0u + 7919u * (uint32_t)(i + 1));
        acd_deal_cards(&r, 0, deck);
        int len;
        long nodes;
        int left = acd_solve(&r.pos, BUDGET, path, &len, &nodes);
        if (getenv("SG_DEBUG")) fprintf(stderr, "  deal %d: %d piles, %ld positions\n", i, left, nodes);
        if (left != 1) continue;
        memcpy(c[nc].deck, deck, 52);
        c[nc].nodes = nodes;
        c[nc].grade = plain(&r, 100, 977u * (uint32_t)(i + 1));
        sol_win_write(path, len, c[nc].win);
        nc++;
    }
    fprintf(stderr, "Accordion: %d of %d deals won\n", nc, tries);
    if (nc < want) { fprintf(stderr, "not enough deals\n"); return 1; }
    qsort(c, (size_t)nc, sizeof *c, by_grade);
    printf("/*\n * Accordion's levels: our own deals, made by tools/accordion_gen.c.\n"
           " * %d levels in %d chapters, from the easiest: each one a deal the search won down to one pile.\n"
           " * Each: its pack in the order dealt (sollevels.h) and the search's win, three letters a move\n"
           " * (sol_win_write: the tests play it); in the comment, the piles plain games of random moves leave on\n"
           " * average.\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n", want, CHAPTERS);
    printf("#define ACD_LEVELS %d\n#define ACD_CHAPTERS %d\n\nstatic const sollevel acd_levels[ACD_LEVELS] = {\n", want,
           CHAPTERS);
    for (int j = 0; j < want; j++) {
        int i = nc <= 1 ? 0 : (int)((double)j * (nc - 1) / (want > 1 ? want - 1 : 1) + 0.5);
        char s[64];
        sol_cards_write(c[i].deck, 52, s);
        printf("    {0, \"%s\",\n     \"", s);
        for (const char *w = c[i].win; *w; w++) printf(*w == '\\' ? "\\\\" : "%c", *w);   /* a C string */
        printf("\"}, /* %d: %.1f piles */\n", j + 1, c[i].grade);
    }
    printf("};\n");
    return 0;
}
