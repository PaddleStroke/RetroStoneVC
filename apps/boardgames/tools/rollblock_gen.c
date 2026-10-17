/*
 * Roll the Block's levels: src/games/rollblock_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of map (its size, its special tiles: plain tiles, then larger ones, cracked tiles, bridges, the
 * split tile, all of them) and makes many with the rules' maker (rlb_make, from fixed seeds: tiles taken away one at a
 * time while the fewest moves do not drop). A map is kept when its special tiles all matter (the cracked tiles: made
 * whole, the way gets shorter; the bridges and the split tile cross the map's gaps: always needed) and when it was
 * not met before (nor its mirror). The difficulty: the fewest moves, and a little more for the places the block can
 * reach (more ways to go wrong). Each chapter keeps 25 maps spread evenly over its difficulties, from an easy one
 * (chapter 1 from the very easiest: a first look) to its hardest. The look, for a hand-laid feel in the diamond view:
 * one-tile notches in the edges filled when the fewest moves stay (plain and cracked maps), and of the maps near each
 * place on the curve the one kept that looks best - a compact footprint near square (the diamond stays large on a
 * phone), few lone spurs of tiles, a mirror or half-turn symmetry; the chapter's last map, its showpiece, the
 * best-looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/rlbgen apps/boardgames/tools/rollblock_gen.c -lm && \
 *       /tmp/rlbgen > apps/boardgames/src/games/rollblock_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define RLB_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/rollblock_rules.c"

#define PER (RLB_LEVELS / RLB_CHAPTERS)
#define MAKES 900                               /* maps made a chapter */
#define TEXT (RLB_CELLS + RLB_MAXH + 1)

typedef struct {
    char text[TEXT];
    int par, states;
    double diff, look;
} cand;

/* a chapter's maps: their size (w x h, up to one more each way), special tiles and changes tried; lo..hi: the moves */
typedef struct { int w, h, grow, fragile, bridges, split, iters, from_easiest, lo, hi; } chapter;

static const chapter CH[RLB_CHAPTERS] = {
    {5, 4, 2, 0, 0, 0, 160, 1, 3, 12},  /* plain tiles, small */
    {8, 6, 2, 0, 0, 0, 500, 0, 11, 24}, /* plain tiles, larger, longer ways */
    {8, 6, 1, 1, 0, 0, 500, 0, 13, 30}, /* cracked tiles */
    {9, 7, 1, 0, 2, 0, 500, 0, 15, 40}, /* buttons and bridges (one or two colours) */
    {9, 7, 1, 0, 0, 1, 450, 0, 16, 44}, /* the split tile */
    {10, 8, 1, 1, 2, 1, 600, 0, 22, 60}, /* all of them */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

static char (*kept)[TEXT];
static int nkept;

static int met(const char *t)                   /* a text met before; its mirror (each row read from the right) too */
{
    char mir[TEXT];
    strcpy(mir, t);
    for (char *row = mir; *row;) {
        char *end = strchr(row, '|');
        int n = end ? (int)(end - row) : (int)strlen(row);
        for (int i = 0; i < n / 2; i++) { char c = row[i]; row[i] = row[n - 1 - i]; row[n - 1 - i] = c; }
        row += n + (end ? 1 : 0);
        if (!end) break;
    }
    for (int i = 0; i < nkept; i++)
        if (!strcmp(kept[i], t) || !strcmp(kept[i], mir)) return 1;
    strcpy(kept[nkept++], t);
    return 0;
}

/* the cracked tiles matter: made whole, the fewest moves drop */
static int cracks_matter(const rlb_map *m, int par)
{
    rlb_map w = *m;
    int n = 0;
    for (int c = 0; c < w.w * w.h; c++)
        if (w.t[c] == RLB_FRAGILE) { w.t[c] = RLB_TILE; n++; }
    return n >= 1 && par_of(&w) < par;
}

/* one-tile notches in the edges (a void cell with three tiles round it) filled while the fewest moves stay */
static void tidy(rlb_map *m, int par, int fragile)
{
    for (int pass = 0; pass < 2; pass++)
        for (int c = 0; c < m->w * m->h; c++) {
            if (m->t[c] != RLB_VOID) continue;
            int near = 0;
            for (int d = 0; d < 4; d++) near += floor_at(m, 0xff, c % m->w + DX[d], c / m->w + DY[d]);
            if (near < 3) continue;
            m->t[c] = RLB_TILE;
            if (par_of(m) != par || (fragile && !cracks_matter(m, par))) m->t[c] = RLB_VOID;
        }
}

/* how hand-laid a map looks: the footprint filled, near square, few spurs, symmetric */
static double look_of(const char *text)
{
    rlb_map m;
    if (!rlb_parse(&m, text)) return 0;
    int tiles = 0, spurs = 0, sym[3] = {0, 0, 0};
    for (int c = 0; c < m.w * m.h; c++) {
        int x = c % m.w, y = c / m.w, on = m.t[c] != RLB_VOID;
        sym[0] += on == (m.t[m.w - 1 - x + y * m.w] != RLB_VOID);
        sym[1] += on == (m.t[x + (m.h - 1 - y) * m.w] != RLB_VOID);
        sym[2] += on == (m.t[m.w - 1 - x + (m.h - 1 - y) * m.w] != RLB_VOID);
        if (!on) continue;
        tiles++;
        int near = 0;
        for (int d = 0; d < 4; d++) near += floor_at(&m, 0xff, x + DX[d], y + DY[d]);
        spurs += near <= 1 && !special(&m, c);
    }
    int best = sym[0] > sym[1] ? sym[0] : sym[1];
    if (sym[2] > best) best = sym[2];
    double area = m.w * m.h;
    return 0.6 * tiles / area + 0.5 * best / area - 2.0 * spurs / tiles - 0.4 * abs(m.w - m.h) / (m.w + m.h);
}

int main(void)
{
    kept = malloc((size_t)RLB_CHAPTERS * MAKES * sizeof *kept);
    static cand pool[MAKES];
    printf("/* Roll the Block's levels: made by tools/rollblock_gen.c (our own maps, found by its search; "
           "do not edit).\n"
           " * Each map's rows from the top split by '|' (rollblock.h), and its par: the fewest moves. */\n");
    static char out[RLB_LEVELS][TEXT];
    static int outpar[RLB_LEVELS];
    for (int ch = 0; ch < RLB_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(7000u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < (C->fragile && !C->bridges ? 3 * MAKES : MAKES) && n < MAKES; k++) {
            rlb_spec sp = {C->w + pr_below(&r, C->grow + 1), C->h + pr_below(&r, C->grow + 1), C->fragile,
                           C->bridges ? 1 + pr_below(&r, C->bridges) : 0, C->split,
                           C->from_easiest ? 20 + pr_below(&r, C->iters) : C->iters};
            rlb_map m;
            rlb_make(&m, &sp, pr_next(&r));
            rlb_state s = start_of(&m);
            int states, par = rlb_solve(&m, &s, NULL, NULL, &states);
            if (par < C->lo || par > C->hi || (C->fragile && !C->bridges && !cracks_matter(&m, par))) continue;
            if (!C->bridges && !C->split) tidy(&m, par, C->fragile);
            char t[TEXT];
            rlb_text(&m, t);
            if (met(t)) continue;
            strcpy(pool[n].text, t);
            pool[n].par = par;
            pool[n].states = states;
            pool[n].diff = par + states / 150.0;
            pool[n].look = look_of(t);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d maps\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = C->from_easiest ? 0 : n / 5;   /* the later chapters: from their easier fifth on */
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && C->from_easiest) a = b = 0;   /* the very first: the easiest */
            if (k == PER - 1) a = n - 1 - (n - lo) / 40 - 2, b = n - 1;   /* the showpiece: of the hardest few */
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;            /* each map once, the curve kept rising */
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            strcpy(out[ch * PER + k], pool[i].text);
            outpar[ch * PER + k] = pool[i].par;
        }
        double avg = 0;
        for (int k = 0; k < PER; k++) avg += look_of(out[ch * PER + k]) / PER;
        fprintf(stderr, "chapter %d: look %.3f, %d maps, moves %d .. %d\n", ch + 1, avg, n, outpar[ch * PER],
                outpar[ch * PER + PER - 1]);
    }
    printf("const char *const rlb_levels[RLB_LEVELS] = {\n");
    for (int l = 0; l < RLB_LEVELS; l++) printf("    \"%s\",\n", out[l]);
    printf("};\n\nconst uint8_t rlb_level_par[RLB_LEVELS] = {");
    for (int l = 0; l < RLB_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", outpar[l]);
    printf("\n};\n");
    return 0;
}
