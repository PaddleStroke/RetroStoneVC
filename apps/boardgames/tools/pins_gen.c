/*
 * Pull the Pins' levels: src/games/pins_levels.h. Our own levels: no published or commercial set is used. Each chapter
 * fills the rules' hand-drawn templates (pins_rules.c: boxes, towers, a hopper, zigzags, pockets, a V of slanting pins,
 * steps, a roof; for two cups, slanting pins that send a fall to one cup or the other) with its things (gems; water
 * and lava; grey balls; two cups and two colours; everything) from fixed seeds (ptp_make), mirrored or not, and keeps
 * a level when the search (every order of pulling the pins, each pull once everything is at rest) finds an order that
 * wins and, past the first chapter, the naive order (the pins from the top down) loses; a chapter's first two levels
 * may be won that way too (they bring its new thing in). Never two levels alike: the same template with the same
 * things in the same boxes is met once (mirrored or not). The difficulty: the pins, how few of the orders win, the
 * kinds of things. Each chapter keeps its share spread over its difficulties (the first from the easiest), its last
 * level the hardest of the most crowded ones (a showpiece). The first chapter, where every order wins, is only three
 * levels (a start: 133 levels in all; the other chapters' candidates are too few to take its share). -DREPORT prints
 * each level's pins, the share of random orders that win and the naive order's end, and the chapters' means.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ptpgen apps/boardgames/tools/pins_gen.c -lm && \
 *       for c in 0 1 2 3 4 5; do /tmp/ptpgen $c > /tmp/ptp$c.txt; done && \
 *       /tmp/ptpgen join /tmp/ptp?.txt > apps/boardgames/src/games/pins_levels.h
 * (the chapters may run at the same time, one process each). Deterministic (fixed seeds).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PTP_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pins_rules.c"

static const int TRIES[PTP_CHAPTERS] = {700, 1500, 2500, 5000, 1500, 1500};

typedef struct {
    char text[PTP_TEXT];
    char sig[64];
    double diff, look;
    int naive_wins;
} cand;

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* the level without its counts and its mirror: the template and each box's thing, the colours as a pattern */
static void signature(const ptp_map *m, char *sig)
{
    int n = sprintf(sig, "%02d:", m->tpl), first = -1;
    for (int b = 0; b < m->nbox; b++) {
        int k = m->bkind[b];
        char c = k == 255 ? '-' : "gdwl"[k];
        int col = 0;
        if (k == PTP_GEM && m->cups == 2)       /* two cups: the cup of the template's left side or not */
            col = m->bcol[b] == m->cupcol[m->mirror] ? 1 : 2;
        else if (k == PTP_GEM) {
            if (first < 0) first = m->bcol[b];
            col = m->bcol[b] == first ? 1 : 2;
        }
        n += sprintf(sig + n, "%c%d", c, col);
    }
}

/* a chapter's candidates, read back: sorted by difficulty */
static int read_cands(const char *path, cand *c, int max)
{
    FILE *f = fopen(path, "r");
    char line[1024];
    int n = 0;
    if (!f) return 0;
    while (n < max && fgets(line, sizeof line, f)) {
        cand *k = &c[n];
        char *t = strchr(line, '\n');
        if (t) *t = 0;
        if (sscanf(line, "%63s %lf %lf %d", k->sig, &k->diff, &k->look, &k->naive_wins) != 4) continue;
        char *q = strchr(line, '"');
        if (!q) continue;
        snprintf(k->text, sizeof k->text, "%s", q + 1);
        if ((t = strchr(k->text, '"'))) *t = 0;
        n++;
    }
    fclose(f);
    return n;
}

/* the levels from the chapters' candidates, in order: a chapter never takes a level alike one taken before it; its
 * share spread over its difficulties (level i from the i-th part of the sorted list), its last level the best looking
 * of the hardest few */
static int join(int argc, char **argv)
{
    static cand c[8192];
    static char taken[PTP_LEVELS][64];
    int ntaken = 0;
    printf("/* Pull the Pins' levels: made by tools/pins_gen.c (our own levels, filled from the rules' templates and\n"
           " * checked by its search; do not edit). Each level: its cups, its template and boxes (pins.h), the orders\n"
           " * of pulling the pins that win. */\n");
    printf("const ptp_level_t ptp_levels[PTP_LEVELS] = {\n");
    for (int ch = 0; ch < PTP_CHAPTERS && ch + 2 < argc; ch++) {
        int got = read_cands(argv[ch + 2], c, 8192), made = 0, want = CH_LEN[ch];
        for (int i = 0; i < got; i++) {         /* those alike a level taken before left out */
            int dup = 0;
            for (int j = 0; j < ntaken; j++)
                if (!strcmp(taken[j], c[i].sig)) dup = 1;
            if (!dup) c[made++] = c[i];
        }
        fprintf(stderr, "chapter %d: %d candidates for %d\n", ch + 1, made, want);
        if (made < want) return 1;
        int used[8192] = {0};
        printf("    /* chapter %d */\n", ch + 1);
        for (int i = 0; i < want; i++) {
            int at;
            if (i == want - 1) {
                at = -1;
                for (int j = made - 1; j >= made - 6 && j >= 0; j--)
                    if (!used[j] && (at < 0 || c[j].look > c[at].look)) at = j;
            } else {
                at = (int)((double)i * (made - 2) / (want - 1));
                while (at < made && (used[at] || (ch && i >= 2 && made >= 2 * want && c[at].naive_wins))) at++;
                if (at >= made)                 /* (past the opening the naive order loses, when there is room) */
                    for (at = 0; at < made && used[at]; at++) {}
            }
            if (at < 0 || at >= made) return 1;
            used[at] = 1;
            snprintf(taken[ntaken++], 64, "%s", c[at].sig);
            printf("    {\"%s\"},\n", c[at].text);
        }
    }
    printf("};\n");
    return ntaken == PTP_LEVELS ? 0 : 1;
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in pins_levels.h */
#include "games/pins_levels.h"
/* each level: its pins, the share of random orders that win, the naive order (top down) winning; the chapters' means.
 * Trivial: the naive order wins, or most random orders do (share >= 0.5) */
static int report(void)
{
    double sh[PTP_CHAPTERS] = {0}, np[PTP_CHAPTERS] = {0};
    int cnt[PTP_CHAPTERS] = {0}, nv[PTP_CHAPTERS] = {0}, triv[PTP_CHAPTERS] = {0};
    fprintf(stderr, "lvl ch pins share naive\n");
    for (int l = 0; l < PTP_LEVELS; l++) {
        ptp_map m;
        ptp_report r;
        ptp_parse(&m, ptp_levels[l].text);
        int c = ptp_chapter_of(l);
        ptp_judge(&m, c, &r);
        int naive = r.naive == PTP_WON;
        fprintf(stderr, "%4d %2d %4d %5.2f %5d\n", l + 1, c + 1, m.np, r.share, naive);
        sh[c] += r.share, np[c] += m.np, cnt[c]++, nv[c] += naive, triv[c] += naive || r.share >= 0.5;
    }
    fprintf(stderr, "chapter: levels, pins mean, share of random orders winning mean, naive wins; trivial (naive wins "
                    "or share >= 0.5)\n");
    for (int c = 0; c < PTP_CHAPTERS; c++)
        fprintf(stderr, "  %d: %2d levels  pins %.1f  share %.2f  naive %d  trivial %d\n", c + 1, cnt[c],
                cnt[c] ? np[c] / cnt[c] : 0, cnt[c] ? sh[c] / cnt[c] : 0, nv[c], triv[c]);
    return 0;
}
#endif

/* a chapter's candidates (stdout: signature, difficulty, look, naive win, text; sorted by difficulty) */
int main(int argc, char **argv)
{
#ifdef REPORT
    return report();
#endif
    if (argc > 1 && !strcmp(argv[1], "join")) return join(argc, argv);
    int ch = argc > 1 ? atoi(argv[1]) : 0, made = 0;
    cand *c = calloc((size_t)TRIES[ch], sizeof *c);
    if (!c) return 1;
    int naive_ok = 0;
    for (int seed = 1; seed <= TRIES[ch]; seed++) {
        uint32_t rs = pr_seed((uint32_t)(ch * 100003 + seed));
        ptp_map m;
        ptp_report r;
        if (!ptp_make(&m, ch, &rs)) continue;
        int good = ptp_judge(&m, ch, &r);
        if (!r.wins) continue;
        if (!good && !(r.naive == PTP_WON && naive_ok < (ch == 2 ? 8 : 4))) continue;
        cand *k = &c[made];
        signature(&m, k->sig);
        int dup = 0;
        for (int i = 0; i < made; i++)
            if (!strcmp(c[i].sig, k->sig)) { dup = 1; break; }
        if (dup) continue;
        if (!good) naive_ok++;
        k->naive_wins = !good;
        ptp_text(&m, k->text);
        static ptp_world w;
        ptp_sim s;
        ptp_build(&m, &w, &s);
        int kinds = 0;
        for (int b = 0; b < m.nbox; b++)
            if (m.bkind[b] != 255) kinds |= 1 << m.bkind[b];
        int nk = 0;
        for (int i = 0; i < PTP_KINDS; i++) nk += kinds >> i & 1;
        k->diff = m.np + 4.0 * (1 - r.share) + 0.6 * nk + (m.cups == 2 ? 0.8 : 0) + 0.004 * s.nb +
                  (k->naive_wins ? -3 : 0);
        k->look = s.nb + 6.0 * m.nf;
        made++;
        fprintf(stderr, "ch %d seed %d: %s  np %d share %.2f naive %d settles %ld diff %.2f\n", ch, seed, k->sig, m.np,
                r.share, r.naive, r.settles, k->diff);
    }
    qsort(c, (size_t)made, sizeof *c, by_diff);
    fprintf(stderr, "chapter %d: %d candidates\n", ch, made);
    for (int i = 0; i < made; i++)
        printf("%s %.4f %.1f %d \"%s\"\n", c[i].sig, c[i].diff, c[i].look, c[i].naive_wins, c[i].text);
    return 0;
}
