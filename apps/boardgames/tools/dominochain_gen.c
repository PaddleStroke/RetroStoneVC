/*
 * Domino Chain's levels: src/games/dominochain_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of chain (straight runs and given turns with a few dominoes missing; turns from the stock; long
 * dominoes over the felt; splitters and their dead ends; all of them; all of them on the largest boards with more
 * wood to mislead) and makes many with the rules' maker (dmc_make, from fixed seeds: a chain laid at random from the
 * start to the bell, no two parts of it touching, pieces taken out of it for the stock, wooden squares added round it
 * in mirrored pairs). A board is kept when its par (the search's fewest pieces) is in the chapter's range, when the
 * chapter's new piece is needed from the stock, and when it was not met before (mirrored either way). The difficulty:
 * the par, more for the nodes the search went through and the wood that misleads. Each chapter keeps 25 spread over
 * its difficulties (chapter 1 from the very easiest); of the boards near each place on the curve the one that looks
 * best is kept (a long chain winding over the board, the board near the phone's shape, well filled); the chapter's last
 * level, its showpiece, the best looking of its hardest few. Deterministic (fixed seeds).
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
#define MAKES 220                               /* boards kept a chapter, at most */
#define CAP 3000000L

typedef struct {
    char text[DMC_TEXT], stock[DMC_KINDS + 1];
    int par;
    double diff, look;
} cand;

/* a chapter: its spec's size (up to grow more each way), runs, gaps (up to 2 more), the chain's turns, longs, splits
 * and those of the stock, the decoys, the extra dominoes; lo..hi the par; need: the stock kind the par needs (-1:
 * any) */
typedef struct { dmc_spec sp; int grow, more, lo, hi, need; } chapter;

static const chapter CH[DMC_CHAPTERS] = {
    {{5, 6, 3, 1, 2, 0, 0, 0, 0, 0, 1, 0}, 1, 2, 1, 3, -1},             /* dominoes missing, turns given */
    {{6, 7, 4, 2, 3, 0, 0, 1, 0, 0, 2, 0}, 1, 2, 2, 5, DMC_K_TURN},     /* turns from the stock */
    {{6, 8, 4, 2, 2, 1, 0, 1, 1, 0, 3, 0}, 1, 2, 3, 6, DMC_K_LONG},     /* long dominoes over the felt */
    {{6, 8, 4, 2, 2, 0, 1, 1, 0, 1, 3, 0}, 1, 2, 3, 6, DMC_K_SPLIT},    /* splitters */
    {{7, 9, 5, 4, 3, 1, 1, 2, 1, 1, 4, 1}, 1, 2, 4, 8, -1},             /* all of them */
    {{7, 9, 6, 5, 3, 1, 1, 2, 1, 1, 6, 1}, 1, 3, 5, 10, -1},             /* all of them, larger, more wood */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: the board mirrored either way (the pieces' ways mirrored with it) ----------------------- */
static char (*kept)[DMC_TEXT];
static int nkept;

static char mirror_ch(char c, int across, int down)
{
    const char *starts = "URDL";
    const char *p = strchr(starts, c);
    if (p && c) {
        int d = (int)(p - starts);
        if (across && (d & 1)) d ^= 2;
        if (down && !(d & 1)) d ^= 2;
        return starts[d];
    }
    if (c >= '1' && c <= '4') {                 /* corners 0 NE, 1 ES, 2 SW, 3 WN */
        static const char ACROSS[4] = {'4', '3', '2', '1'}, DOWN[4] = {'2', '1', '4', '3'};
        if (across) c = ACROSS[c - '1'];
        if (down) c = DOWN[c - '1'];
    }
    return c;
}

static void form_of(const dmc_map *m, int tr, char *out)
{
    char t[DMC_TEXT];
    dmc_text(m, NULL, t);
    int n = 0;
    for (int y = 0; y < m->h; y++) {
        for (int x = 0; x < m->w; x++) {
            int sx = tr & 1 ? m->w - 1 - x : x, sy = tr & 2 ? m->h - 1 - y : y;
            out[n++] = mirror_ch(t[sx + sy * (m->w + 1)], tr & 1, tr & 2);
        }
        out[n++] = '|';
    }
    out[n] = 0;
}

static int met(const dmc_map *m)
{
    char l[4][DMC_TEXT];
    for (int tr = 0; tr < 4; tr++) form_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 4; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

/* the par's pieces need this kind from the stock: without it the board cannot be done in the par */
static int needs(const dmc_map *m, int kind, int par)
{
    if (kind < 0) return 1;
    if (!m->stock[kind]) return 0;
    dmc_map n = *m;
    dmc_state e;
    n.stock[kind] = 0;
    memset(&e, 0, sizeof e);
    memset(e.t, DMC_WOOD, sizeof e.t);
    memcpy(e.left, n.stock, sizeof e.left);
    int p = dmc_solve(&n, &e, par, CAP, NULL, NULL, NULL);
    return p < 0;
}

/* a board that looks laid by hand: a long chain, near the phone's upright shape, the board well filled */
static double look_of(const dmc_map *m)
{
    int pieces = 0, wood = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        pieces += m->t[c] >= DMC_DOMINO;
        wood += m->t[c] != DMC_FELT;
    }
    double fill = (double)wood / (m->w * m->h), shape = (double)m->h / m->w;
    return 0.04 * pieces - fabs(fill - 0.55) - 0.4 * fabs(shape - 1.35);
}

int main(void)
{
    kept = malloc((size_t)DMC_CHAPTERS * MAKES * 60 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[DMC_LEVELS];
    printf("/* Domino Chain's levels: made by tools/dominochain_gen.c (our own boards, found by its search; do not "
           "edit).\n * Each level: its board (dominochain.h), its stock (dominoes, turns, long dominoes, splitters), "
           "its par: the\n * fewest pieces. */\n");
    for (int ch = 0; ch < DMC_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(9100u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 60 * MAKES && n < MAKES; k++) {
            dmc_spec sp = C->sp;
            sp.w += pr_below(&r, C->grow + 1);
            sp.h += pr_below(&r, C->grow + 1);
            if (sp.w > DMC_MAXW) sp.w = DMC_MAXW;
            if (sp.h > DMC_MAXH) sp.h = DMC_MAXH;
            sp.gaps += pr_below(&r, C->more + 1);
            sp.runs += pr_below(&r, 2);
            if (ch == 0) sp.gaps = 1 + pr_below(&r, 3);
            dmc_map m;
            dmc_state sol, e;
            dmc_make(&m, &sol, &sp, pr_next(&r));
            memset(&e, 0, sizeof e);
            memset(e.t, DMC_WOOD, sizeof e.t);
            memcpy(e.left, m.stock, sizeof e.left);
            long nodes;
            int par = dmc_solve(&m, &e, sol.placed, CAP, NULL, NULL, &nodes);
            if (par < C->lo || par > C->hi || nodes > CAP || !needs(&m, C->need, par) || met(&m)) continue;
            int wood = 0;
            for (int c = 0; c < m.w * m.h; c++) wood += m.t[c] == DMC_WOOD;
            dmc_text(&m, NULL, pool[n].text);
            for (int q = 0; q < DMC_KINDS; q++) pool[n].stock[q] = (char)('0' + m.stock[q]);
            pool[n].stock[DMC_KINDS] = 0;
            pool[n].par = par;
            pool[n].diff = par + 0.6 * log10((double)nodes + 1) + 0.08 * (wood - par);
            pool[n].look = look_of(&m);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 8;
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && ch == 0) a = b = 0;
            if (k == PER - 1) a = n - 1 - n / 25 - 2, b = n - 1;
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            out[ch * PER + k] = pool[i];
        }
        fprintf(stderr, "chapter %d: %d boards, pieces %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par);
    }
    printf("const dmc_level_t dmc_levels[DMC_LEVELS] = {\n");
    for (int l = 0; l < DMC_LEVELS; l++) printf("    {\"%s\", \"%s\", %d},\n", out[l].text, out[l].stock, out[l].par);
    printf("};\n");
    return 0;
}
