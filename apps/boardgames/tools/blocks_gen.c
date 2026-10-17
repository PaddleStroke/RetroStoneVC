/*
 * Block Puzzle's levels: src/games/blocks_levels.h. Our own levels: no published set is used. Six chapters of 25;
 * each level a starting grid, a fixed sequence of pieces and a goal (clear N lines, reach a score, clear the gold
 * blocks, cover the pattern), the goals mixed by a schedule per chapter (the pattern from chapter 2).
 * A candidate: a grid drawn from a hand-made picture (MOTIF: a heart, a tree, a ring, a castle...; mirrored across,
 * maybe upside down and moved a row, its parts in two or three lacquers) near the chapter's density and not one of the
 * last 8 levels' pictures; gold blocks on it in mirror pairs; the pattern a hand-made shape (PSHAPE) centred across in
 * the free cells; never a full line at the start; a sequence of 45 pieces drawn
 * from the chapter's families; a goal of the chapter's size. The rules' beam search (bk_search) finds the fewest
 * pieces that reach the goal: that is the par, and the way found is kept (the test replays it). The level gives par
 * plus the chapter's slack pieces (6 in chapter 1 down to 1 in chapter 6). The difficulty: how often a player who sees
 * only the tray (bk_greedy with noise) reaches the goal within them, over 40 games. Each level takes, of a dozen
 * candidates of its goal, the one whose success rate is nearest a curve falling from 0.97 at level 1 to about 0.1 at
 * level 150; repeats (the same grid and pieces) are dropped. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bkgen apps/boardgames/tools/blocks_gen.c -lm && \
 *       /tmp/bkgen > apps/boardgames/src/games/blocks_levels.h
 * or faster, the six chapters side by side (the same file):
 *   (/tmp/bkgen head; for c in 0 1 2 3 4 5; do /tmp/bkgen $c > /tmp/bk$c & done; wait; cat /tmp/bk[0-5];
 *       /tmp/bkgen tail) > apps/boardgames/src/games/blocks_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define BK_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/blocks_rules.c"

#define PER (BK_LEVELS / BK_CHAPTERS)
#define CANDS 12
#define SEQ_LEN 45
#define BEAM 120
#define TRIES 40

typedef struct {
    float dens0, dens1;                         /* the grid's blocks (a part of the cells) */
    unsigned fams;                              /* the families of pieces drawn (bit f) */
    int lines0, lines1, score0, score1, gold0, gold1, pat0, pat1, slack;
} chapter;

static const chapter CH[BK_CHAPTERS] = {
    {0.10f, 0.20f, 0x07f, 1, 3, 40, 100, 1, 2, 4, 7, 6},
    {0.16f, 0.28f, 0x67f, 2, 4, 80, 150, 2, 3, 6, 9, 5},
    {0.20f, 0.32f, 0xfff, 3, 5, 120, 200, 3, 4, 8, 12, 4},
    {0.24f, 0.36f, 0x1fff, 4, 6, 160, 240, 4, 5, 10, 14, 3},
    {0.28f, 0.40f, 0x1fff, 5, 7, 200, 280, 5, 6, 12, 16, 2},
    {0.30f, 0.44f, 0x1fff, 6, 8, 230, 320, 6, 8, 14, 19, 1},
};

/* the goals, level by level in each chapter: L lines, S score, G gold, P pattern */
static const char *const PLAN[BK_CHAPTERS] = {
    "LLLLSLGLSGLGSLGGLSGLSGLGS",
    "PLPGSPLGPSLPGLSPGLPSGPLSP",
    "LGPSGLPSLGSPGLPSLGPSGLSPL",
    "GPLSLGPSPGLSLPGSGLPSLGPSG",
    "PSLGLPGSGSLPLGPSPLGSGLSPL",
    "SGPLGLSPLPGSGSPLPLGSLGPSG",
};

static const uint8_t LACQUER[9] = {1, 2, 3, 4, 7, 8, 9, 10, 11};

typedef struct {
    char grid[BK_CELLS + 1];
    uint8_t seq[SEQ_LEN];
    int goal, target, par, pieces, nway, motif;
    uint8_t way[BK_SEQ][2];
    float rate;
} cand;

static int full_line(const char *g)
{
    for (int i = 0; i < BK_N; i++) {
        int r = 1, c = 1;
        for (int j = 0; j < BK_N; j++) {
            r &= g[i * BK_N + j] != '.' && g[i * BK_N + j] != 'o';
            c &= g[j * BK_N + i] != '.' && g[j * BK_N + i] != 'o';
        }
        if (r || c) return 1;
    }
    return 0;
}

static int hue_char(int hue) { return hue < 10 ? '0' + hue : 'a' + hue - 10; }

/* a mirror of cell (x, y): across the middle column (sym 0), or the half turn (sym 1) */
static int mirror(int x, int y, int sym) { return sym ? (BK_N - 1 - y) * BK_N + BK_N - 1 - x : y * BK_N + BK_N - 1 - x; }

/* ---- the pictures: hand-drawn grids (the left half mirrored; 'A'..'C' colour roles) and the pattern's shapes ---- */
typedef struct { const char *name; int flip; const char *row[BK_N]; } motif;   /* flip: may stand upside down */
static const motif MOTIF[] = {
    {"smile", 0, {"..........", "..........", "...A..A...", "...A..A...", "..........", ".B......B.", "..B....B..",
                  "...BBBB...", "..........", ".........."}},
    {"stars", 1, {".A......A.", "..........", "....BB....", ".A......A.", "..........", "..........", ".A......A.",
                  "....BB....", "..........", ".A......A."}},
    {"eyes", 1, {"..........", "..........", "..AA..AA..", "..AA..AA..", "..........", "..........", "..........",
                 "..........", "....BB....", ".........."}},
    {"corners", 0, {"AA......AA", "A........A", "..........", "....BB....", "..........", "..........",
                    "....BB....", "..........", "A........A", "AA......AA"}},
    {"ticks", 1, {"AA..AA..AA", "..........", "..........", "..........", "..........", "..........", "..........",
                  "..........", "..........", "BB..BB..BB"}},
    {"vees", 1, {"A........A", ".A......A.", "..A....A..", "...A..A...", "....AA....", "..........", "B........B",
                 ".B......B.", "..B....B..", "...B..B..."}},
    {"heart", 1, {"..........", "..........", "...A..A...", "..AAAAAA..", "..AAAAAA..", "...AAAA...", "....AA....",
                  "..........", "..........", ".........."}},
    {"steps", 1, {"..........", "..........", "..........", "..........", "..........", "..........", "....AA....",
                  "...BBBB...", "..AAAAAA..", ".BBBBBBBB."}},
    {"kite", 1, {"....AA....", "...A..A...", "..A....A..", ".A......A.", "A........A", "A........A", ".A......A.",
                 "..A....A..", "...A..A...", "....AA...."}},
    {"pillars", 1, {"..........", ".A..BB..A.", ".A..BB..A.", ".A......A.", ".A......A.", ".A......A.", ".A......A.",
                    ".A..BB..A.", ".A..BB..A.", ".........."}},
    {"ring", 1, {"..........", "...AAAA...", "..A....A..", ".A......A.", ".A..BB..A.", ".A..BB..A.", ".A......A.",
                 "..A....A..", "...AAAA...", ".........."}},
    {"arrows", 1, {"....AA....", "...AAAA...", "..AA..AA..", ".AA....AA.", "....BB....", "...BBBB...", "..BB..BB..",
                   ".BB....BB.", "..........", ".........."}},
    {"cross", 1, {"..........", "....AA....", "....AA....", "....AA....", ".AAABBAAA.", ".AAABBAAA.", "....AA....",
                  "....AA....", "....AA....", ".........."}},
    {"tree", 0, {"....AA....", "...AAAA...", "..AAAAAA..", "...AAAA...", "..AAAAAA..", ".AAAAAAAA.", "....BB....",
                 "....BB....", "..........", ".........."}},
    {"mushroom", 0, {"..........", "...AAAA...", "..AABBAA..", ".AAAAAAAA.", ".ABAAAABA.", "....CC....", "....CC....",
                     "...CCCC...", "..........", ".........."}},
    {"flower", 1, {"..........", "....AA....", "..A.AA.A..", "...AAAA...", ".AAABBAAA.", ".AAABBAAA.", "...AAAA...",
                   "..A.AA.A..", "....AA....", ".........."}},
    {"diamond", 1, {"....AA....", "...A..A...", "..A.BB.A..", ".A.B..B.A.", "A.B....B.A", "A.B....B.A", ".A.B..B.A.",
                    "..A.BB.A..", "...A..A...", "....AA...."}},
    {"house", 0, {"..........", "....AA....", "...AAAA...", "..AAAAAA..", ".AAAAAAAA.", "..BB..BB..", "..BB..BB..",
                  "..BBCCBB..", "..BBCCBB..", ".........."}},
    {"castle", 0, {"..........", ".A.A..A.A.", ".AAAAAAAA.", ".AA.AA.AA.", ".AAAAAAAA.", ".AAA..AAA.", ".AAA..AAA.",
                   "..........", "..........", ".........."}},
    {"butterfly", 1, {"..........", ".A......A.", ".AA....AA.", ".AAA..AAA.", ".AAABBAAA.", ".AAABBAAA.", ".AAA..AAA.",
                      ".AA....AA.", ".A......A.", ".........."}},
    {"hourglass", 1, {".AAAAAAAA.", "..AAAAAA..", "...AAAA...", "....AA....", "....BB....", "...BBBB...", "..BBBBBB..",
                      ".BBBBBBBB.", "..........", ".........."}},
    {"bigheart", 0, {"..........", "..AA..AA..", ".AAAAAAAA.", ".AAABBAAA.", ".AAAAAAAA.", "..AAAAAA..", "...AAAA...",
                     "....AA....", "..........", ".........."}},
    {"frame", 1, {"AAA....AAA", "A........A", "A..B..B..A", "..........", "....CC....", "....CC....", "..........",
                  "A..B..B..A", "A........A", "AAA....AAA"}},
    {"bridge", 1, {"..........", "..........", "..........", ".AAAAAAAA.", "A.B....B.A", "A.B....B.A", "A........A",
                   "..........", "..........", ".........."}},
};
#define NMOTIF ((int)(sizeof MOTIF / sizeof MOTIF[0]))

static const char *const PSHAPE[] = {             /* the pattern's shapes, centred across ('o' a cell, '/' a row) */
    "oo/oo", ".oo./oooo/.oo.", "o..o/oooo/o..o", "oo....oo/oo....oo", "oooo/o..o/o..o/oooo",
    ".oo./oooo/oooo/.oo.", "..oo../.oooo./oooooo", "oooooo/.oooo./..oo..", "o....o/.o..o./..oo../..oo../.o..o./o....o",
    "o.oo.o/oooooo/o.oo.o", "oo..oo/oooooo/oo..oo", "oooooo/o....o/o....o/oooooo", "..oo../.oooo./oooooo/oooooo",
    "..oo../..oo../oooooo/oooooo/..oo../..oo..", "oooooooo/o......o/oooooooo",
};
#define NPSHAPE ((int)(sizeof PSHAPE / sizeof PSHAPE[0]))

static int motif_cells(int m)
{
    int n = 0;
    for (int y = 0; y < BK_N; y++)
        for (int x = 0; x < BK_N / 2; x++) n += 2 * (MOTIF[m].row[y][x] != '.');
    return n;
}

static int pshape_cells(int s)
{
    int n = 0;
    for (const char *p = PSHAPE[s]; *p; p++) n += *p == 'o';
    return n;
}

static int recent[BK_LEVELS], nrecent;          /* the pictures chosen so far, in order */

/* the grid: a picture near the density, not one of the last levels' (the left half mirrored), maybe upside down and
 * moved a row; its colour roles in three lacquers of the level. The picture's index, or -1. */
static int make_grid(char *g, float dens, uint32_t *r)
{
    int want = (int)(dens * BK_CELLS + 0.5f), ok[NMOTIF], n = 0;
    for (int slack = 4; !n && slack < 60; slack += 3)
        for (int m = 0; m < NMOTIF; m++) {
            int c = motif_cells(m), rep = 0;
            for (int i = nrecent - 1; i >= 0 && i >= nrecent - 8; i--) rep |= recent[i] == m;
            if (!rep && c >= want - slack && c <= want + slack) ok[n++] = m;
        }
    if (!n) return -1;
    int m = ok[pr_below(r, n)], flip = MOTIF[m].flip && pr_below(r, 2), hue[3];
    hue[0] = LACQUER[pr_below(r, 9)];
    do hue[1] = LACQUER[pr_below(r, 9)]; while (hue[1] == hue[0]);
    do hue[2] = LACQUER[pr_below(r, 9)]; while (hue[2] == hue[0] || hue[2] == hue[1]);
    int top = BK_N, bot = -1;
    for (int y = 0; y < BK_N; y++)
        if (strspn(MOTIF[m].row[y], ".") < BK_N) top = top < y ? top : y, bot = y;
    int lo = flip ? -(BK_N - 1 - bot) : -top, hi = flip ? top : BK_N - 1 - bot, dy = -1 + pr_below(r, 3);
    if (dy < lo) dy = lo;
    if (dy > hi) dy = hi;
    memset(g, '.', BK_CELLS);
    g[BK_CELLS] = 0;
    for (int y = 0; y < BK_N; y++) {
        int sy = flip ? BK_N - 1 - y : y, ty = y + dy;
        if (ty < 0 || ty >= BK_N) continue;
        for (int x = 0; x < BK_N / 2; x++) {
            char c = MOTIF[m].row[sy][x];
            if (c == '.') continue;
            g[ty * BK_N + x] = g[ty * BK_N + BK_N - 1 - x] = (char)hue_char(hue[c - 'A']);
        }
    }
    return full_line(g) ? -1 : m;
}

/* n gold blocks among the blocks (in mirror pairs), on the fuller rows and columns first */
static int add_gold(char *g, int n, int sym, uint32_t *r)
{
    int got = 0;
    for (int tries = 0; tries < 300 && got < n; tries++) {
        int i = pr_below(r, BK_CELLS), x = i % BK_N, y = i / BK_N;
        if (g[i] == '.' || g[i] == 'G') continue;
        int rowfill = 0, colfill = 0;
        for (int j = 0; j < BK_N; j++) rowfill += g[y * BK_N + j] != '.', colfill += g[j * BK_N + x] != '.';
        if (rowfill < 3 && colfill < 3 && tries < 200) continue;
        int m = mirror(x, y, sym);
        g[i] = 'G';
        got++;
        if (m != i && got < n && g[m] != '.' && g[m] != 'G') g[m] = 'G', got++;
    }
    return got;
}

/* the pattern: a shape of n0..n1 cells centred across, at a height where its cells are all empty; its cells, or 0 */
static int add_pattern(char *g, int n0, int n1, uint32_t *r)
{
    int ok[NPSHAPE], n = 0;
    for (int s = 0; s < NPSHAPE; s++)
        if (pshape_cells(s) >= n0 && pshape_cells(s) <= n1) ok[n++] = s;
    if (!n) return 0;
    const char *sh = PSHAPE[ok[pr_below(r, n)]];
    int w = (int)strcspn(sh, "/"), h = 1;
    for (const char *p = sh; *p; p++) h += *p == '/';
    int x0 = (BK_N - w) / 2, ys[BK_N], ny = 0;
    for (int y0 = 0; y0 + h <= BK_N; y0++) {
        int fits = 1;
        for (int i = 0, y = y0, x = x0; sh[i]; i++) {
            if (sh[i] == '/') { y++, x = x0; continue; }
            if (sh[i] == 'o' && g[y * BK_N + x] != '.') fits = 0;
            x++;
        }
        if (fits) ys[ny++] = y0;
    }
    if (!ny) return 0;
    int y0 = ys[pr_below(r, ny)], got = 0;
    for (int i = 0, y = y0, x = x0; sh[i]; i++) {
        if (sh[i] == '/') { y++, x = x0; continue; }
        if (sh[i] == 'o') g[y * BK_N + x] = 'o', got++;
        x++;
    }
    return got;
}

static int draw_fam(unsigned fams, int chap, uint32_t *r)
{
    for (;;) {
        int s = draw_shape(r);
        int f = bk_shapes[s].fam;
        if (!(fams >> f & 1)) continue;
        if (chap >= 5 && (f == 0 || f == 1) && pr_below(r, 2)) continue;   /* the last chapter: fewer small ones */
        return s;
    }
}

static void load_cand(bk_game *g, const cand *c, int n)
{
    bk_level_def d = {(uint8_t)c->goal, (uint16_t)c->target, (uint8_t)c->par, (uint8_t)n, c->grid, NULL, ""};
    char s[SEQ_LEN + 1];
    for (int i = 0; i < n; i++) s[i] = bk_shape_char(c->seq[i]);
    s[n] = 0;
    d.seq = s;
    if (!bk_load(g, &d)) {
        fprintf(stderr, "bad candidate\n");
        exit(1);
    }
}

static bk_game G;

/* the share of games a tray-only player wins within the level's pieces */
static float success(const cand *c, uint32_t seed)
{
    int won = 0;
    uint32_t r = pr_seed(seed);
    for (int t = 0; t < TRIES; t++) {
        load_cand(&G, c, c->pieces);
        while (!bk_over(&G)) {
            int s, x, y;
            if (!bk_greedy(&G.p.k, G.goal, G.target, 260, &r, &s, &x, &y)) break;
            bk_place(&G, s, x, y);
        }
        won += bk_solved(&G);
    }
    return (float)won / TRIES;
}

static int make(cand *c, int level, int goal, uint32_t seed)
{
    int chap = level / PER;
    const chapter *C = &CH[chap];
    uint32_t r = pr_seed(seed);
    float t = (level % PER) / (float)(PER - 1);
    float dens = C->dens0 + (C->dens1 - C->dens0) * (0.3f * t + 0.7f * pr_below(&r, 1000) / 1000.0f);
    int sym = 0;                                /* gold blocks mirrored across, as the pictures */
    memset(c, 0, sizeof *c);
    c->goal = goal;
    if (goal == BK_PATTERN) dens *= 0.7f;
    if (goal == BK_SCORE) dens = fminf(0.5f, dens * 1.35f);   /* more to clear: the points come sooner */
    if ((c->motif = make_grid(c->grid, dens, &r)) < 0) return 0;
    switch (goal) {
    case BK_LINES: c->target = C->lines0 + pr_below(&r, C->lines1 - C->lines0 + 1); break;
    case BK_SCORE: c->target = (C->score0 + pr_below(&r, C->score1 - C->score0 + 1)) / 10 * 10; break;
    case BK_GOLDS: {
        int n = C->gold0 + pr_below(&r, C->gold1 - C->gold0 + 1);
        if (add_gold(c->grid, n, sym, &r) < 1) return 0;
        break;
    }
    case BK_PATTERN: {
        if (!add_pattern(c->grid, C->pat0, C->pat1, &r)) return 0;
        break;
    }
    }
    if (level < 4) c->target = goal == BK_LINES ? 1 + level / 2 : c->target;
    for (int i = 0; i < SEQ_LEN; i++) c->seq[i] = (uint8_t)draw_fam(C->fams, chap, &r);
    load_cand(&G, c, 28);                       /* the search looks at the first 28 pieces only */
    if (bk_solved(&G) || !bk_can_move(&G.p.k)) return 0;
    uint8_t way[BK_SEQ][2];
    int par = bk_search(&G.p.k, G.seq, G.nseq, goal, c->target, BEAM, way);
    if (par < (level < 6 ? 2 : 3) || par > 26) return 0;
    /* the last piece the way takes: the sequence must reach it */
    int last = 0;
    {
        bk_game *T = malloc(sizeof *T);
        if (!T) return 0;
        load_cand(T, c, SEQ_LEN);
        for (int i = 0; i < par; i++) {
            int s = way[i][0], cell = way[i][1];
            int idx = (int)T->p.k.next - 3 + s;   /* the tray's place s holds piece next - 3 + s of a full deal */
            if (idx > last) last = idx;
            if (!bk_place(T, s, cell % BK_N, cell / BK_N)) { free(T); return 0; }
        }
        int ok = bk_solved(T);
        free(T);
        if (!ok) return 0;
    }
    c->par = par;
    c->pieces = par + C->slack;
    if (c->pieces < last + 1) c->pieces = last + 1;
    if (c->pieces > SEQ_LEN) c->pieces = SEQ_LEN;
    c->nway = par;
    memcpy(c->way, way, sizeof way[0] * (size_t)par);
    c->rate = success(c, seed ^ 0x5bd1e995u);
    return 1;
}

static uint32_t cand_hash(const cand *c)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < BK_CELLS; i++) h = (h ^ (uint8_t)c->grid[i]) * 16777619u;
    for (int i = 0; i < c->pieces; i++) h = (h ^ c->seq[i]) * 16777619u;
    return h;
}

static void put_way(const cand *c, char *out)
{
    int n = 0;
    for (int i = 0; i < c->nway; i++) {
        int cell = c->way[i][1];
        n += sprintf(out + n, "%c%c%d", 'a' + c->way[i][0], 'a' + cell % BK_N, cell / BK_N + 1);
    }
    out[n] = 0;
}

static const char *const GOAL_NAME[5] = {"", "lines", "score", "gold", "pattern"};

int main(int argc, char **argv)
{
    static cand chosen[BK_LEVELS];
    static uint32_t seen[BK_LEVELS];
    /* an argument: only that chapter (0..5), its levels alone (chapters can be made side by side, then joined: the
     * same file as all at once); "head" or "tail": the file's start or end alone */
    int only = argc > 1 ? (argv[1][0] == 'h' ? -2 : argv[1][0] == 't' ? -3 : atoi(argv[1])) : -1;
    for (int lv = 0; lv < BK_LEVELS; lv++) {
        int chap = lv / PER;
        if (only != -1 && only != chap) continue;
        if (lv % PER == 0) nrecent = 0;         /* each chapter on its own */
        char gc = PLAN[chap][lv % PER];
        int goal = gc == 'L' ? BK_LINES : gc == 'S' ? BK_SCORE : gc == 'G' ? BK_GOLDS : BK_PATTERN;
        float x = lv / (float)(BK_LEVELS - 1), want = 0.97f - 0.87f * powf(x, 0.85f);
        cand best, c;
        float bd = 9;
        int got = 0;
        for (int a = 0; a < 120 && got < CANDS; a++) {
            if (!make(&c, lv, goal, (uint32_t)(lv * 7919 + a * 104729 + 17))) continue;
            uint32_t h = cand_hash(&c);
            int dup = 0;
            for (int i = 0; i < lv; i++) dup |= seen[i] == h;
            if (dup) continue;
            got++;
            float d = fabsf(c.rate - want);
            if (d < bd) bd = d, best = c;
        }
        if (!got) {
            fprintf(stderr, "level %d: no candidate\n", lv + 1);
            return 1;
        }
        chosen[lv] = best;
        seen[lv] = cand_hash(&best);
        recent[nrecent++] = best.motif;
        fprintf(stderr, "level %3d %-9s %-7s target %3d par %2d pieces %2d rate %.2f (want %.2f)\n", lv + 1,
                MOTIF[best.motif].name, GOAL_NAME[goal], best.target, best.par, best.pieces, best.rate, want);
    }
    if (only == -1 || only == -2)
        printf("/*\n * Block Puzzle's levels: our own, made by tools/blocks_gen.c (grids drawn from hand-made pictures, "
               "sequences drawn by\n * chapter, each goal reached by the rules' beam search in `par` pieces; six "
               "chapters of 25). Each: the goal, its\n * target, the par, the pieces given, the grid (rows from the top: "
               "'.' empty, a colour, 'G' gold, 'o' the\n * pattern), the pieces in order, the way found; in the comment "
               "the share of tray-only games won.\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
               "const bk_level_def bk_levels[BK_LEVELS] = {\n");
    static const char *const GOAL_ID[5] = {"BK_CLASSIC", "BK_LINES", "BK_SCORE", "BK_GOLDS", "BK_PATTERN"};
    for (int lv = 0; lv < BK_LEVELS; lv++) {
        if (only != -1 && only != lv / PER) continue;
        const cand *c = &chosen[lv];
        char seq[SEQ_LEN + 1], way[BK_SEQ * 4 + 1];
        for (int i = 0; i < c->pieces; i++) seq[i] = bk_shape_char(c->seq[i]);
        seq[c->pieces] = 0;
        put_way(c, way);
        printf("    {%s, %d, %d, %d, /* %d: %.2f */\n     \"%s\",\n     \"%s\", \"%s\"},\n", GOAL_ID[c->goal],
               c->target, c->par, c->pieces, lv + 1, c->rate, c->grid, seq, way);
    }
    if (only == -1 || only == -3) printf("};\n");
    return 0;
}
