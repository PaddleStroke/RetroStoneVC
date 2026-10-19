/*
 * Pull the Pins' levels and the random tab's puzzles: src/games/pins_levels.h. Our own levels: no published or
 * commercial set is used. Each chapter fills the rules' hand-drawn templates (pins_rules.c: towers of doors, filters
 * of slanting pins over the drain, forks, crossing pins over two cups, and the first plain boxes) with its things
 * (grey balls; water and lava; both; two cups; everything; everything at its hardest) from fixed seeds (ptp_make),
 * mirrored or not, and the search (ptp_search: every order of pulling the pins, each pull once everything rests)
 * measures it:
 * - the obvious player (ptp_obvious: it pulls what looks safest, gems first, never lava first, and passes over a pull
 *   that loses at once) must lose: past the first three levels, a level it wins is trivial and is never kept;
 * - the share of random orders that win: at most a half, the hardest chapter an eighth with five pins or more;
 * - the first pull the obvious player makes leading nowhere (no winning order starts with it) counts as harder.
 * Never two levels alike: the same template with the same things in the same boxes is met once (mirrored or not). The
 * difficulty: how few orders win, the pins, the obvious first pull a dead end. Each chapter keeps up to its share
 * spread over its difficulties (the first from the easiest), its last level the hardest. The leftovers make the random
 * tab's three pools (easy, medium, hard by the same measures).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ptpgen apps/boardgames/tools/pins_gen.c -lm && \
 *       for c in 0 1 2 3 4 5; do /tmp/ptpgen $c > /tmp/ptp$c.txt; done && \
 *       /tmp/ptpgen join /tmp/ptp?.txt > apps/boardgames/src/games/pins_levels.h
 * (the chapters may run at the same time, one process each). -DREPORT: the levels' measures, by chapter (stderr).
 * Deterministic (fixed seeds).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PTP_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pins_rules.c"

static const int TRIES[PTP_CHAPTERS] = {16000, 16000, 20000, 12000, 12000, 14000};
static const int WANT[PTP_CHAPTERS] = {14, 16, 16, 14, 16, 14};
#define POOL 24

typedef struct {
    char text[PTP_TEXT];
    char sig[64];
    double diff, share, first;
    int np, obvious_wins, tpl, used;
} cand;

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* the level without its counts and its mirror: the template and each box's thing, the colours as a pattern (two cups:
 * the cup of the template's left side or not) */
static void signature(const ptp_map *m, char *sig)
{
    int n = sprintf(sig, "%02d:", m->tpl), first = -1;
    for (int b = 0; b < m->nbox; b++) {
        int k = m->bkind[b];
        char c = k == 255 ? '-' : "gdwl"[k];
        int col = 0;
        if (k == PTP_GEM && m->cups == 2)
            col = m->bcol[b] == m->cupcol[m->mirror] ? 1 : 2;
        else if (k == PTP_GEM) {
            if (first < 0) first = m->bcol[b];
            col = m->bcol[b] == first ? 1 : 2;
        }
        n += sprintf(sig + n, "%c%d", c, col);
    }
}

static double difficulty(const ptp_report *r, int np)
{
    return 8.0 * (1 - r->share) + 0.9 * np + (r->obv_first_share == 0 ? 1.5 : 0);
}

/* ---- a chapter's candidates (stdout: signature, difficulty, share, first-pull share, pins, obvious win, template,
 * text; sorted by difficulty) --------------------------------------------------------------------------------- */
static int chapter(int ch)
{
    cand *c = calloc((size_t)TRIES[ch], sizeof *c);
    int made = 0, plain = 0;
    if (!c) return 1;
    for (int seed = 1; seed <= TRIES[ch]; seed++) {
        uint32_t rs = pr_seed((uint32_t)(ch * 100003 + seed));
        ptp_map m;
        ptp_report r;
        if (!ptp_make(&m, ch, &rs)) continue;
        int good = ptp_judge(&m, ch, &r);
        if (!r.wins || r.capped) continue;
        if (!good && !(ch == 0 && plain < 40)) continue;   /* the obvious player wins: only for the opening */
        cand *k = &c[made];
        signature(&m, k->sig);
        int dup = 0;
        for (int i = 0; i < made && !dup; i++) dup = !strcmp(c[i].sig, k->sig);
        if (dup) continue;
        plain += !good;
        k->obvious_wins = !good;
        k->share = r.share;
        k->first = r.obv_first_share;
        k->np = m.np;
        k->tpl = m.tpl;
        k->diff = difficulty(&r, m.np) - (good ? 0 : 20);
        ptp_text(&m, k->text);
        made++;
        fprintf(stderr, "ch %d seed %d: %s np %d share %.3f obvious %d first %.2f settles %ld\n", ch, seed, k->sig,
                m.np, r.share, r.obvious, r.obv_first_share, r.settles);
    }
    qsort(c, (size_t)made, sizeof *c, by_diff);
    fprintf(stderr, "chapter %d: %d candidates\n", ch, made);
    for (int i = 0; i < made; i++)
        printf("%s %.4f %.4f %.4f %d %d %d \"%s\"\n", c[i].sig, c[i].diff, c[i].share, c[i].first, c[i].np,
               c[i].obvious_wins, c[i].tpl, c[i].text);
    return 0;
}

static int read_cands(const char *path, cand *c, int max)
{
    FILE *f = fopen(path, "r");
    char line[1024];
    int n = 0;
    if (!f) return 0;
    while (n < max && fgets(line, sizeof line, f)) {
        cand *k = &c[n];
        memset(k, 0, sizeof *k);
        if (sscanf(line, "%63s %lf %lf %lf %d %d %d", k->sig, &k->diff, &k->share, &k->first, &k->np,
                   &k->obvious_wins, &k->tpl) != 7)
            continue;
        char *q = strchr(line, '"'), *e;
        if (!q) continue;
        snprintf(k->text, sizeof k->text, "%s", q + 1);
        if ((e = strchr(k->text, '"'))) *e = 0;
        n++;
    }
    fclose(f);
    return n;
}

/* a level for chapter ch at place i of its levels: the measures it must meet */
static int fits(const cand *k, int ch, int i)
{
    if (ch == 0 && i < 3) return k->obvious_wins && k->np <= 1 + i && !strchr(k->sig, 'd');   /* the opening */
    if (k->obvious_wins || k->share > 0.5) return 0;
    if (ch == PTP_CHAPTERS - 1) return k->share <= 0.125 && k->np >= 5;
    return 1;
}

static int join(int argc, char **argv)
{
    static cand all[PTP_CHAPTERS][8192];
    static char taken[512][64], out[512][PTP_TEXT];
    int got[PTP_CHAPTERS] = {0}, len[PTP_CHAPTERS] = {0}, ntaken = 0, total = 0;
    for (int ch = 0; ch < PTP_CHAPTERS && ch + 2 < argc; ch++) got[ch] = read_cands(argv[ch + 2], all[ch], 8192);
    for (int ch = 0; ch < PTP_CHAPTERS; ch++) {
        cand *c = all[ch];
        int want = WANT[ch];
        for (int i = 0; i < want; i++) {        /* spread over the difficulties of those that fit place i */
            static int list[8192];
            int nl = 0;
            for (int j = 0; j < got[ch]; j++) {
                if (c[j].used || !fits(&c[j], ch, i)) continue;
                int dup = 0;                    /* never one alike a level taken before */
                for (int t = 0; t < ntaken && !dup; t++) dup = !strcmp(taken[t], c[j].sig);
                if (!dup) list[nl++] = j;
            }
            if (!nl) break;
            int at;
            if (ch == 0 && i < 3) at = list[0];
            else if (i == want - 1) at = list[nl - 1];
            else at = list[nl / (want - i + 1)];
            c[at].used = 1;
            snprintf(taken[ntaken++], 64, "%s", c[at].sig);
            snprintf(out[total++], PTP_TEXT, "%s", c[at].text);
            len[ch]++;
        }
        fprintf(stderr, "chapter %d: %d levels of %d candidates\n", ch + 1, len[ch], got[ch]);
    }
    /* the random tab: what is left (lost by the obvious player, at most half the orders winning, none alike), from the
     * easiest to the hardest, split in three */
    static cand *left[4096];
    static char pool[3][POOL][PTP_TEXT];
    int nleft = 0;
    for (int ch = 0; ch < PTP_CHAPTERS; ch++)
        for (int i = 0; i < got[ch] && nleft < 4096; i++) {
            cand *k = &all[ch][i];
            if (k->used || k->obvious_wins || k->share > 0.5) continue;
            int dup = 0;
            for (int j = 0; j < ntaken && !dup; j++) dup = !strcmp(taken[j], k->sig);
            if (dup) continue;
            snprintf(taken[ntaken++], 64, "%s", k->sig);
            left[nleft++] = k;
        }
    for (int a = 1; a < nleft; a++)            /* by difficulty */
        for (int b = a; b > 0 && left[b]->diff < left[b - 1]->diff; b--) {
            cand *t = left[b];
            left[b] = left[b - 1];
            left[b - 1] = t;
        }
    int pmin = nleft / 3 < POOL ? nleft / 3 : POOL;
    for (int v = 0; v < 3; v++)
        for (int i = 0; i < pmin; i++)
            snprintf(pool[v][i], PTP_TEXT, "%s", left[v * (nleft / 3) + i * (nleft / 3) / pmin]->text);
    fprintf(stderr, "random tab: %d puzzles left, %d a variant\n", nleft, pmin);
    if (pmin < 4) return 1;
    printf("/* Pull the Pins' levels and the random tab's puzzles: made by tools/pins_gen.c (our own, filled from the\n"
           " * rules' templates and measured by its search: past the first three, the obvious player loses them; do not\n"
           " * edit). Each one: its cups, its template and boxes (pins.h), the orders of pulling the pins that win. */\n");
    printf("#ifndef PINS_LEVELS_H\n#define PINS_LEVELS_H\n#define PTP_LEVELS %d\n#define PTP_CH_LENS {", total);
    for (int ch = 0; ch < PTP_CHAPTERS; ch++) printf("%s%d", ch ? ", " : "", len[ch]);
    printf("}\n#define PTP_POOL %d\n#endif\n#ifdef PTP_LEVEL_DATA\n#undef PTP_LEVEL_DATA\n", pmin);
    printf("const ptp_level_t ptp_levels[PTP_LEVELS] = {\n");
    for (int ch = 0, at = 0; ch < PTP_CHAPTERS; ch++) {
        printf("    /* chapter %d */\n", ch + 1);
        for (int i = 0; i < len[ch]; i++) printf("    {\"%s\"},\n", out[at++]);
    }
    printf("};\nconst ptp_level_t ptp_pool[PTP_VARIANTS][PTP_POOL] = {\n");
    for (int v = 0; v < 3; v++) {
        printf("    {\n");
        for (int i = 0; i < pmin; i++) printf("        {\"%s\"},\n", pool[v][i]);
        printf("    },\n");
    }
    printf("};\n#endif\n");
    return 0;
}

#ifdef REPORT                                   /* -DREPORT: the measures of the levels in pins_levels.h */
#define PTP_LEVEL_DATA
#include "games/pins_levels.h"
static int report(void)
{
    double sh[PTP_CHAPTERS] = {0}, np[PTP_CHAPTERS] = {0}, mx[PTP_CHAPTERS] = {0};
    int cnt[PTP_CHAPTERS] = {0}, obv[PTP_CHAPTERS] = {0}, dead[PTP_CHAPTERS] = {0}, lo[PTP_CHAPTERS], hi[PTP_CHAPTERS];
    for (int c = 0; c < PTP_CHAPTERS; c++) lo[c] = 99, hi[c] = 0;
    fprintf(stderr, "level chapter pins share obvious-wins obvious-first-share\n");
    for (int l = 0; l < PTP_LEVELS; l++) {
        ptp_map m;
        ptp_report r;
        ptp_parse(&m, ptp_levels[l].text);
        int c = ptp_chapter_of(l);
        ptp_judge(&m, c, &r);
        fprintf(stderr, "%4d %2d %4d %6.3f %3d %6.2f\n", l + 1, c + 1, m.np, r.share, r.obvious == PTP_WON,
                r.obv_first_share);
        sh[c] += r.share, np[c] += m.np, cnt[c]++, obv[c] += r.obvious == PTP_WON, dead[c] += r.obv_first_share == 0;
        if (r.share > mx[c]) mx[c] = r.share;
        if (m.np < lo[c]) lo[c] = m.np;
        if (m.np > hi[c]) hi[c] = m.np;
    }
    fprintf(stderr, "chapter: levels; pins (range, mean); winning share of random orders (mean, most); obvious player "
                    "wins; its first pull a dead end\n");
    for (int c = 0; c < PTP_CHAPTERS; c++)
        fprintf(stderr, "  %d: %2d levels  pins %d-%d mean %.1f  share mean %.3f max %.3f  obvious wins %d  dead first %d\n",
                c + 1, cnt[c], lo[c], hi[c], cnt[c] ? np[c] / cnt[c] : 0, cnt[c] ? sh[c] / cnt[c] : 0, mx[c], obv[c],
                dead[c]);
    for (int v = 0; v < PTP_VARIANTS; v++) {
        double s = 0, p = 0;
        int o = 0;
        for (int i = 0; i < PTP_POOL; i++) {
            ptp_map m;
            ptp_report r;
            ptp_parse(&m, ptp_pool[v][i].text);
            ptp_judge(&m, 0, &r);
            s += r.share, p += m.np, o += r.obvious == PTP_WON;
        }
        fprintf(stderr, "  random %d: %d puzzles  pins mean %.1f  share mean %.3f  obvious wins %d\n", v, PTP_POOL,
                p / PTP_POOL, s / PTP_POOL, o);
    }
    return 0;
}
#endif

int main(int argc, char **argv)
{
#ifdef REPORT
    (void)argc, (void)argv;
    return report();
#else
    if (argc > 1 && !strcmp(argv[1], "join")) return join(argc, argv);
    return chapter(argc > 1 ? atoi(argv[1]) : 0);
#endif
}
