/*
 * Falling Dominoes' levels: src/games/dominochain_levels.h. Our own levels: no published or commercial set is used.
 * Each chapter brings its piece (plain dominoes and turns, stoppers, splitters, delay dominoes, long dominoes over the
 * gaps, all of them) and makes many boards with the rules' maker (dmc_make, from fixed seeds: a chain laid at random
 * from the domino to push to the trigger, solved; wood round it, the chain's mirror image a ghost of its way; then
 * pieces moved at random). The search finds each board's par (the fewest moves), how many positions at that depth a
 * push solves, and how many of the first moves begin a shortest way. A board is kept when its par is in the chapter's
 * range, when no push works before a move, when the chapter's piece matters (stoppers: kept still, the par grows;
 * delay dominoes: made plain, the par grows), and when it was not met before (mirrored either way). The difficulty:
 * the par, the share of first moves that lead nowhere, the few solving positions. Each chapter keeps its levels spread
 * over its difficulties (chapter 1 from the easiest); of the boards near each place on the curve the one that misleads
 * most is kept, and the chapter ends on its hardest. Deterministic (fixed seeds). The chapters' figures on stderr.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dmcgen apps/boardgames/tools/dominochain_gen.c -lm && \
 *       /tmp/dmcgen > apps/boardgames/src/games/dominochain_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define DMC_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominochain_rules.c"

#define PER (DMC_LEVELS / DMC_CHAPTERS)
#define MAKES 120                               /* boards kept a chapter, at most */
#define CAP 400000L

typedef struct {
    char text[DMC_TEXT], sol[64];
    int par, goals, firsts, good;
    long states;
    double diff, mislead;
} cand;

/* a chapter: its spec (size, runs, turns, splitters, delays, longs, stoppers, moves made, depth), the par lo..hi, the
 * piece that must matter (DMC_STOPPER or DMC_DELAY; 0 none), the piece the board must have, the boards tried */
typedef struct { dmc_spec sp; int grow, lo, hi, matter, have, tries; } chapter;

static const chapter CH[DMC_CHAPTERS] = {
    {{5, 6, 3, 1, 0, 0, 0, 2, 2, 2}, 1, 1, 2, 0, DMC_STOPPER, 20000},          /* dominoes, turns, stoppers */
    {{6, 7, 3, 1, 1, 0, 0, 1, 2, 2}, 1, 1, 2, 0, DMC_SPLIT, 20000},            /* splitters */
    {{6, 7, 3, 1, 1, 1, 0, 1, 2, 2}, 1, 1, 2, DMC_DELAY, DMC_DELAY, 20000},    /* delay dominoes */
    {{6, 7, 3, 1, 0, 0, 1, 1, 2, 2}, 1, 1, 2, 0, DMC_LONG, 20000},             /* long dominoes over the gaps */
    {{6, 7, 4, 1, 1, 1, 1, 1, 3, 3}, 1, 2, 3, 0, DMC_SPLIT, 2500},             /* all of them */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: mirrored either way (the turns' corners mirrored with them) -------------------------------- */
static char (*kept)[DMC_TEXT];
static int nkept;

static char mirror_ch(char c, int across, int down)
{
    if (c >= '1' && c <= '4') {                 /* corners 0 NE, 1 ES, 2 SW, 3 WN */
        static const char ACROSS[4] = {'4', '3', '2', '1'}, DOWN[4] = {'2', '1', '4', '3'};
        if (across) c = ACROSS[c - '1'];
        if (down) c = DOWN[c - '1'];
    }
    return c;
}

static int met(const dmc_state *s)
{
    char t[DMC_TEXT], l[4][DMC_TEXT];
    dmc_text(s, t);
    for (int tr = 0; tr < 4; tr++) {
        int n = 0;
        for (int y = 0; y < s->h; y++) {
            for (int x = 0; x < s->w; x++) {
                int sx = tr & 1 ? s->w - 1 - x : x, sy = tr & 2 ? s->h - 1 - y : y;
                l[tr][n++] = mirror_ch(t[sx + sy * (s->w + 1)], tr & 1, tr & 2);
            }
            l[tr][n++] = '|';
        }
        l[tr][n] = 0;
    }
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 4; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

static int has(const dmc_state *s, int t)
{
    for (int c = 0; c < s->w * s->h; c++)
        if (s->t[c] == t) return 1;
    return 0;
}

/* the piece matters: stoppers kept still, or delay dominoes made plain, and the board needs more moves */
static int matters(const dmc_state *s, int t, int par)
{
    if (!t) return 1;
    dmc_state n = *s;
    dmc_info in;
    if (t == DMC_STOPPER) dmc_frozen = 1 << DMC_STOPPER;
    else
        for (int c = 0; c < n.w * n.h; c++)
            if (n.t[c] == DMC_DELAY) n.t[c] = DMC_DOMINO;
    int ok = !dmc_search(&n, par, CAP, &in) || in.par > par;
    dmc_frozen = 0;
    return ok;
}

int main(void)
{
    kept = malloc((size_t)DMC_CHAPTERS * 10000 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[DMC_LEVELS];
    printf("/* Falling Dominoes' levels: made by tools/dominochain_gen.c (our own boards, found by its search; do not "
           "edit).\n * Each level: its board (dominochain.h), its par (the fewest moves), a shortest way and its push. "
           "*/\n");
    for (int ch = 0; ch < DMC_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(12100u + (uint32_t)ch);
        int n = 0, why[6] = {0};          /* (the probe's count of boards dropped: not made, no answer, par, piece, obvious) */
        for (int k = 0; k < C->tries && n < MAKES; k++) {
            dmc_spec sp = C->sp;
            sp.w += pr_below(&r, C->grow + 1);
            sp.h += pr_below(&r, C->grow + 1);
            if (ch == 0 && k % 3 == 0) sp.scramble = 1;
            dmc_state s;
            if (!dmc_make(&s, &sp, pr_next(&r)) || !has(&s, C->have)) { why[0]++; continue; }
            dmc_info in;
            if (!dmc_search(&s, sp.depth, CAP, &in)) { why[1]++; continue; }
            if (in.par < C->lo || in.par > C->hi) { why[2]++; continue; }
            int counts = matters(&s, C->matter, in.par);   /* (a delay domino that matters: rarer, kept harder) */
            if (!counts && C->matter != DMC_DELAY) { why[3]++; continue; }
            int plain = dmc_obvious(&s, in.par);
            if (plain && (ch > 0 || n >= 3)) { why[4]++; continue; }
            if (met(&s)) continue;   /* only the first three levels may be obvious */
            cand *p = &pool[n];
            dmc_text(&s, p->text);
            int len = 0;
            for (int i = 0; i < in.par; i++) {
                dmc_notation(&s, in.sol[i], p->sol + len);
                len += (int)strlen(p->sol + len);
            }
            sprintf(p->sol + len, "p%c%c%c", 'a' + in.push / 4 % s.w, 'a' + in.push / 4 / s.w, "urdl"[in.push % 4]);
            p->par = in.par;
            p->goals = in.goals;
            p->firsts = in.firsts;
            p->good = in.good;
            p->states = in.states;
            p->mislead = in.firsts ? 1 - (double)in.good / in.firsts : 0;
            p->diff = 3.0 * in.par + 3 * p->mislead + 1.5 / in.goals + 0.3 * log10((double)in.states + 1);
            if (C->matter == DMC_DELAY && counts) p->diff += 1.5;
            if (plain) p->diff -= 10;           /* the obvious ones first: the three first levels */
            n++;
        }
        if (getenv("PROBE")) {
            fprintf(stderr, "chapter %d: %d boards of %d tried (dropped: %d not made, %d no answer, %d par, %d piece, "
                            "%d obvious)\n", ch + 1, n, C->tries, why[0], why[1], why[2], why[3], why[4]);
            continue;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 6;
        double step = (double)(n - 1 - lo) / (PER - 1), mis = 0, goals = 0;
        int pmin = 99, pmax = 0, gmin = 1 << 30;
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && ch == 0) a = b = 0;
            if (k == PER - 1) a = b = n - 1;
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].mislead > pool[i].mislead) i = j;
            last = i;
            out[ch * PER + k] = pool[i];
            mis += pool[i].mislead;
            goals += pool[i].goals;
            if (pool[i].par < pmin) pmin = pool[i].par;
            if (pool[i].par > pmax) pmax = pool[i].par;
            if (pool[i].goals < gmin) gmin = pool[i].goals;
        }
        fprintf(stderr, "chapter %d: %d boards; moves %d .. %d; first moves leading nowhere %.0f%% on average; "
                        "solving positions %.1f on average, %d at least\n",
                ch + 1, n, pmin, pmax, 100 * mis / PER, goals / PER, gmin);
    }
    printf("const dmc_level_t dmc_levels[DMC_LEVELS] = {\n");
    for (int l = 0; l < DMC_LEVELS; l++)
        printf("    {\"%s\", %d, \"%s\"},   /* %d of %d first moves good, %d solving */\n", out[l].text, out[l].par,
               out[l].sol, out[l].good, out[l].firsts, out[l].goals);
    printf("};\n");
    return 0;
}
