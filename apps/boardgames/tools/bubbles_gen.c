/*
 * Bubble Pop's levels: src/games/bubbles_levels.h. Our own levels: no published or commercial set is used. Each chapter
 * makes many boxes with the rules' maker (bub_make, from fixed seeds: a mirrored shape of marbles, coloured in a
 * pattern, its chapter's specials laid in mirrored pairs, a sequence of marbles), solves each one with the rules' beam
 * search (the fewest shots found: the par; the shots allowed: the par and one or two more) and measures it:
 * - par: the fewest shots found;
 * - greedy: the obvious player (each shot the one that clears the most now) wins in the shots allowed, or not;
 * - traps: the share of the first shots after which the box can no longer be won in the shots allowed;
 * - banks: the solution's shots off a wall;
 * - nodes: the moves the search weighed.
 * A chapter keeps its boxes: one or two easy ones first that show its idea (chapter 1: three, the only obvious levels),
 * then boxes the obvious player loses, from the chapter's floor to its hardest by measured difficulty, no two shapes
 * alike in a row; its last level, its showpiece, the biggest of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bubgen apps/boardgames/tools/bubbles_gen.c -lm && \
 *       /tmp/bubgen > apps/boardgames/src/games/bubbles_levels.h
 * One chapter's table and levels alone: /tmp/bubgen <chapter 1..6> (the table on stderr); "merge" reads the six
 * chapters' outputs (files ch1.txt .. ch6.txt in the folder given) and writes the header, so the chapters can be made
 * side by side.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define BUB_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/bubbles_rules.c"

#define PER 25
#define MAXC 1500

/* a chapter: a few specs (sizes), the par's range, how many easy levels open it, the candidates made */
typedef struct {
    const char *idea;
    bub_spec sp[3];
    int nsp, lo, hi, intro, tries;
    uint32_t seed;
} chapter;

/* w rows every colours hmin hmax gems stones locks bombs rainbows slack beam */
static const chapter CH[BUB_CHAPTERS] = {
    {"clear the box", {{7, 11, 0, 3, 3, 5, 0, 0, 0, 0, 0, 1, 60}, {8, 12, 0, 3, 4, 6, 0, 0, 0, 0, 0, 1, 60},
                       {7, 11, 0, 2, 2, 4, 0, 0, 0, 0, 0, 1, 60}}, 3, 3, 9, 3, 2600, 1000},
    {"free the gems", {{8, 12, 0, 3, 4, 6, 2, 0, 0, 0, 0, 1, 60}, {8, 12, 0, 4, 5, 7, 2, 0, 0, 0, 0, 1, 60},
                       {7, 11, 0, 3, 4, 6, 1, 0, 0, 0, 0, 1, 60}}, 3, 4, 10, 2, 2600, 2000},
    {"stones", {{8, 12, 0, 3, 4, 6, 0, 4, 0, 0, 0, 1, 60}, {8, 12, 0, 4, 4, 7, 2, 4, 0, 0, 0, 1, 60},
                {9, 12, 0, 4, 4, 6, 0, 6, 0, 0, 0, 1, 60}}, 3, 5, 11, 2, 2600, 3000},
    {"bombs and rainbows", {{8, 12, 0, 4, 4, 7, 0, 2, 0, 1, 1, 1, 60}, {8, 12, 0, 4, 5, 7, 2, 4, 0, 1, 0, 1, 60},
                            {9, 12, 0, 5, 4, 6, 0, 2, 0, 0, 2, 1, 60}}, 3, 5, 12, 2, 2600, 4000},
    {"locks", {{8, 12, 0, 4, 4, 7, 0, 0, 4, 0, 0, 1, 60}, {8, 12, 0, 4, 5, 7, 2, 2, 4, 0, 1, 1, 60},
               {9, 12, 0, 5, 4, 7, 0, 2, 6, 1, 0, 1, 60}}, 3, 6, 13, 2, 2600, 5000},
    {"the ceiling comes down", {{8, 12, 4, 4, 4, 6, 0, 2, 2, 0, 1, 1, 60}, {8, 11, 3, 5, 4, 6, 2, 2, 2, 1, 0, 1, 60},
                                {9, 12, 4, 5, 5, 7, 0, 4, 4, 1, 1, 1, 60}}, 3, 7, 14, 2, 2600, 6000},
};

typedef struct {
    bub_level L;
    int par, greedy, banks, cells, nmoves;
    double trap, diff;
    char text[BUB_TEXT];
} cand;

static cand C[MAXC];

/* the same box (the text up to the marbles) */
static int same_box(const char *a, const char *b)
{
    const char *e = strchr(a, ':');
    e = e ? strchr(e + 1, ':') : NULL;
    size_t n = e ? (size_t)(e - a) : strlen(a);
    return !strncmp(a, b, n) && (b[n] == ':' || !b[n]);
}

/* the share of first shots after which no win in the shots allowed is found */
static double traps(const bub_level *L, int *nmoves)
{
    bub_state s;
    bub_begin(L, &s);
    bub_move m[BUB_MOVES];
    int n = bub_moves(L, &s, m, BUB_MOVES), bad = 0;
    *nmoves = n;
    for (int i = 0; i < n; i++) {
        bub_state t = s;
        if (!bub_shoot(L, &t, m[i].aim, m[i].swap, NULL)) { bad++; continue; }
        if (t.end == BUB_WON) continue;
        if (t.end != BUB_PLAYING || bub_solve(L, &t, 40, NULL) < 0) bad++;
    }
    return n ? (double)bad / n : 0;
}

static int banks_of(const bub_level *L)
{
    bub_state s;
    bub_begin(L, &s);
    int b = 0;
    for (int i = 0; i < L->nsol; i++) {
        bub_shot o;
        bub_trace(L, &s, L->sol[i], &o);
        if (o.bounces) b++;
        bub_shoot(L, &s, L->sol[i], L->solswap[i], NULL);
    }
    return b;
}

static int by_diff(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->diff < y->diff ? -1 : x->diff > y->diff ? 1 : strcmp(x->text, y->text);
}

/* one chapter's levels (its table on stderr): their texts in out */
static int make_chapter(int ch, char out[PER][BUB_TEXT])
{
    const chapter *c = &CH[ch];
    int n = 0;
    for (int i = 0; i < c->tries && n < MAXC; i++) {
        bub_spec sp = c->sp[i % c->nsp];
        bub_level L;
        if (!bub_make(&L, &sp, c->seed + (uint32_t)i)) continue;
        if (!bub_settle(&L, &sp)) continue;
        if (L.nsol > c->hi + 2) continue;
        cand *k = &C[n];
        memset(k, 0, sizeof *k);
        bub_text(&L, k->text);
        int dup = 0;
        for (int j = 0; j < n && !dup; j++) dup = same_box(C[j].text, k->text);
        if (dup) continue;
        /* a wider search for the par, then the obvious player against the shots allowed */
        sp.beam = 300;
        bub_level W = L;
        if (bub_settle(&W, &sp) && W.nsol < L.nsol) L = W;
        bub_text(&L, k->text);
        k->L = L;
        k->par = L.nsol;
        bub_state s;
        bub_begin(&L, &s);
        k->greedy = bub_greedy(&L, &s);
        k->cells = bub_count(&s, -1);
        n++;
    }
    /* the intro levels: the obvious player wins them, few shots; the others: it loses them, in the chapter's range */
    int ni = 0, nh = 0;
    cand *intro = malloc(sizeof(cand) * MAXC), *hard = malloc(sizeof(cand) * MAXC);
    for (int i = 0; i < n; i++) {
        cand *k = &C[i];
        if (k->greedy >= 0 && k->par <= c->lo && k->par >= (ch ? 2 : 1)) intro[ni++] = *k;
        else if (k->greedy < 0 && k->par >= c->lo && k->par <= c->hi) {
            k->banks = banks_of(&k->L);
            k->trap = traps(&k->L, &k->nmoves);
            if (k->trap < 0.25) continue;
            k->diff = k->par + 8 * k->trap + 0.8 * k->banks + 0.04 * k->cells + 0.6 * k->L.gems;
            hard[nh++] = *k;
        }
    }
    fprintf(stderr, "chapter %d (%s): %d made, %d intro, %d hard\n", ch + 1, c->idea, n, ni, nh);
    qsort(hard, (size_t)nh, sizeof *hard, by_diff);
    int want_intro = c->intro, got = 0;
    cand pick[PER];
    for (int i = 0; i < ni; i++) intro[i].diff = intro[i].par + 0.02 * intro[i].cells;
    qsort(intro, (size_t)ni, sizeof *intro, by_diff);
    for (int i = 0; i < want_intro && i < ni; i++) {   /* the easy ones, from the easiest, growing a little */
        int at = (int)((double)i * ni / (3.0 * want_intro));
        if (got && pick[got - 1].L.shape == intro[at].L.shape && at + 1 < ni) at++;
        pick[got++] = intro[at];
    }
    int rest = PER - got;
    if (nh < rest) {
        fprintf(stderr, "chapter %d: only %d hard boxes\n", ch + 1, nh);
        rest = nh;
    }
    /* spread over the hard ones by difficulty, the showpiece last: the most cells among the hardest five */
    int used[MAXC] = {0}, show = nh - 1;
    for (int i = nh - 1; i >= 0 && i >= nh - 5; i--)
        if (hard[i].cells > hard[show].cells) show = i;
    for (int i = 0; i < rest - 1; i++) {
        int at = (int)((double)i * (nh - 1) / (rest - 1) * 0.92);
        while (at < nh && (used[at] || at == show)) at++;
        if (at >= nh) break;
        /* not the same shape as the level before: the next one of another shape, if near */
        if (got && pick[got - 1].L.shape == hard[at].L.shape)
            for (int d = 1; d < 4 && at + d < nh; d++)
                if (!used[at + d] && at + d != show && hard[at + d].L.shape != pick[got - 1].L.shape) { at += d; break; }
        used[at] = 1;
        pick[got++] = hard[at];
    }
    if (nh) pick[got++] = hard[show];
    fprintf(stderr, "  lvl par greedy traps banks cells moves  diff  shape\n");
    double sp = 0, st = 0, sb = 0;
    int ng = 0;
    for (int i = 0; i < got; i++) {
        cand *k = &pick[i];
        if (!k->nmoves) { k->banks = banks_of(&k->L); k->trap = traps(&k->L, &k->nmoves); }
        fprintf(stderr, "  %3d %3d %6d %5.2f %5d %5d %5d %5.1f  %d/%d\n", ch * PER + i + 1, k->par, k->greedy, k->trap,
                k->banks, k->cells, k->nmoves, k->diff, k->L.shape, k->L.pattern);
        sp += k->par;
        st += k->trap;
        sb += k->banks;
        ng += k->greedy >= 0;
        strcpy(out[i], k->text);
    }
    fprintf(stderr, "  chapter %d: %d levels, par %.1f, traps %.2f, banks %.1f, the obvious player wins %d\n", ch + 1,
            got, sp / got, st / got, sb / got, ng);
    free(intro);
    free(hard);
    return got;
}

static void header(int counts[BUB_CHAPTERS], char lv[BUB_CHAPTERS][PER][BUB_TEXT])
{
    int total = 0;
    for (int c = 0; c < BUB_CHAPTERS; c++) total += counts[c];
    printf("/* Bubble Pop's levels: made by tools/bubbles_gen.c (our own boxes, solved and measured by its search: past the\n"
           " * first three, the obvious player loses them; do not edit). Each one: its box, its marbles, the shots of the\n"
           " * fewest found (bubbles.h). */\n");
    printf("#ifndef BUBBLES_LEVELS_H\n#define BUBBLES_LEVELS_H\n#define BUB_LEVELS %d\n#define BUB_CH_LENS {", total);
    for (int c = 0; c < BUB_CHAPTERS; c++) printf("%s%d", c ? ", " : "", counts[c]);
    printf("}\n#endif\n#ifdef BUB_LEVEL_DATA\n#undef BUB_LEVEL_DATA\nconst char *const bub_levels[BUB_LEVELS] = {\n");
    for (int c = 0; c < BUB_CHAPTERS; c++) {
        printf("    /* chapter %d: %s */\n", c + 1, CH[c].idea);
        for (int i = 0; i < counts[c]; i++) printf("    \"%s\",\n", lv[c][i]);
    }
    printf("};\n#endif\n");
}

int main(int argc, char **argv)
{
    static char lv[BUB_CHAPTERS][PER][BUB_TEXT];
    int counts[BUB_CHAPTERS] = {0};
    if (argc > 2 && !strcmp(argv[1], "merge")) {
        for (int c = 0; c < BUB_CHAPTERS; c++) {
            char fn[512];
            snprintf(fn, sizeof fn, "%s/ch%d.txt", argv[2], c + 1);
            FILE *f = fopen(fn, "r");
            if (!f) { fprintf(stderr, "no %s\n", fn); return 1; }
            while (counts[c] < PER && fgets(lv[c][counts[c]], BUB_TEXT, f)) {
                lv[c][counts[c]][strcspn(lv[c][counts[c]], "\r\n")] = 0;
                if (lv[c][counts[c]][0]) counts[c]++;
            }
            fclose(f);
        }
        header(counts, lv);
        return 0;
    }
    if (argc > 1) {
        int c = atoi(argv[1]) - 1;
        if (c < 0 || c >= BUB_CHAPTERS) return 1;
        int n = make_chapter(c, lv[c]);
        for (int i = 0; i < n; i++) printf("%s\n", lv[c][i]);
        return 0;
    }
    for (int c = 0; c < BUB_CHAPTERS; c++) counts[c] = make_chapter(c, lv[c]);
    header(counts, lv);
    return 0;
}
