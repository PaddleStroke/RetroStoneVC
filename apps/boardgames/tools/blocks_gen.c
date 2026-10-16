/*
 * Block Puzzle's levels: src/games/blocks_levels.h. Our own levels: no published set is used. Six chapters of 25;
 * each level a starting grid, a fixed sequence of pieces and a goal (clear N lines, reach a score, clear the gold
 * blocks, cover the pattern), the goals mixed by a schedule per chapter (the pattern from chapter 2).
 * A candidate: a grid made with a symmetry (blocks stamped in mirror pairs, in lacquer colours; gold blocks among
 * them; a pattern grown from a middle cell, mirrored), never a full line at the start; a sequence of 45 pieces drawn
 * from the chapter's families; a goal of the chapter's size. The rules' beam search (bk_search) finds the fewest
 * pieces that reach the goal: that is the par, and the way found is kept (the test replays it). The level gives par
 * plus the chapter's slack pieces (6 in chapter 1 down to 1 in chapter 6). The difficulty: how often a player who sees
 * only the tray (bk_greedy with noise) reaches the goal within them, over 40 games. Each level takes, of a dozen
 * candidates of its goal, the one whose success rate is nearest a curve falling from 0.97 at level 1 to about 0.1 at
 * level 150; repeats (the same grid and pieces) are dropped. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bkgen apps/boardgames/tools/blocks_gen.c && \
 *       /tmp/bkgen > apps/boardgames/src/games/blocks_levels.h
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
    int goal, target, par, pieces, nway;
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

static int blocks_in(const char *g)
{
    int n = 0;
    for (int i = 0; i < BK_CELLS; i++) n += g[i] != '.' && g[i] != 'o';
    return n;
}

static int hue_char(int hue) { return hue < 10 ? '0' + hue : 'a' + hue - 10; }

/* a mirror of cell (x, y): across the middle column (sym 0), or the half turn (sym 1) */
static int mirror(int x, int y, int sym) { return sym ? (BK_N - 1 - y) * BK_N + BK_N - 1 - x : y * BK_N + BK_N - 1 - x; }

/* the grid's blocks: small pieces stamped in mirror pairs, a colour a pair, up to the density */
static void make_grid(char *g, float dens, int sym, uint32_t *r)
{
    memset(g, '.', BK_CELLS);
    g[BK_CELLS] = 0;
    int want = (int)(dens * BK_CELLS + 0.5f);
    for (int tries = 0; tries < 400 && blocks_in(g) < want; tries++) {
        int sh = pr_below(r, 14), hue = LACQUER[pr_below(r, 9)];   /* the small families (0..6) */
        const bk_shape *s = &bk_shapes[sh];
        int x = pr_below(r, BK_N - s->w + 1), y = pr_below(r, BK_N - s->h + 1);
        char t[BK_CELLS + 1];
        memcpy(t, g, sizeof t);
        int ok = 1;
        for (int i = 0; i < s->h && ok; i++)
            for (int j = 0; j < s->w; j++) {
                if (!(s->row[i] >> j & 1)) continue;
                int a = (y + i) * BK_N + x + j, b = mirror(x + j, y + i, sym);
                if (t[a] != '.') { ok = 0; break; }
                t[a] = t[b] = (char)hue_char(hue);
            }
        if (ok && !full_line(t) && blocks_in(t) <= want + 4) memcpy(g, t, sizeof t);
    }
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

/* a pattern of n empty cells grown from the middle, mirrored, joined side by side */
static int add_pattern(char *g, int n, int sym, uint32_t *r)
{
    int got = 0;
    for (int tries = 0; tries < 2000 && got < n; tries++) {
        int x, y;
        if (!got) {
            x = 3 + pr_below(r, 4);
            y = 2 + pr_below(r, 6);
        } else {                                /* next to a cell of the pattern */
            int i = pr_below(r, BK_CELLS);
            if (g[i] != 'o') continue;
            static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int d = pr_below(r, 4);
            x = i % BK_N + D[d][0];
            y = i / BK_N + D[d][1];
            if (x < 0 || y < 0 || x >= BK_N || y >= BK_N) continue;
        }
        int i = y * BK_N + x, m = mirror(x, y, sym);
        if (g[i] != '.') continue;
        g[i] = 'o';
        got++;
        if (m != i && g[m] == '.') g[m] = 'o', got++;
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
    int sym = pr_below(&r, 2);
    memset(c, 0, sizeof *c);
    c->goal = goal;
    if (goal == BK_PATTERN) dens *= 0.7f;
    if (goal == BK_SCORE) dens = fminf(0.5f, dens * 1.35f);   /* more to clear: the points come sooner */
    make_grid(c->grid, dens, sym, &r);
    switch (goal) {
    case BK_LINES: c->target = C->lines0 + pr_below(&r, C->lines1 - C->lines0 + 1); break;
    case BK_SCORE: c->target = (C->score0 + pr_below(&r, C->score1 - C->score0 + 1)) / 10 * 10; break;
    case BK_GOLDS: {
        int n = C->gold0 + pr_below(&r, C->gold1 - C->gold0 + 1);
        if (add_gold(c->grid, n, sym, &r) < 1) return 0;
        break;
    }
    case BK_PATTERN: {
        int n = C->pat0 + pr_below(&r, C->pat1 - C->pat0 + 1);
        if (add_pattern(c->grid, n, sym, &r) < C->pat0) return 0;
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

int main(void)
{
    static cand chosen[BK_LEVELS];
    static uint32_t seen[BK_LEVELS];
    for (int lv = 0; lv < BK_LEVELS; lv++) {
        int chap = lv / PER;
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
        fprintf(stderr, "level %3d %-7s target %3d par %2d pieces %2d rate %.2f (want %.2f)\n", lv + 1,
                GOAL_NAME[goal], best.target, best.par, best.pieces, best.rate, want);
    }
    printf("/*\n * Block Puzzle's levels: our own, made by tools/blocks_gen.c (grids stamped in mirror pairs, sequences "
           "drawn by\n * chapter, each goal reached by the rules' beam search in `par` pieces; six chapters of 25). "
           "Each: the goal, its\n * target, the par, the pieces given, the grid (rows from the top: '.' empty, a "
           "colour, 'G' gold, 'o' the\n * pattern), the pieces in order, the way found; in the comment the share of "
           "tray-only games won.\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const bk_level_def bk_levels[BK_LEVELS] = {\n");
    static const char *const GOAL_ID[5] = {"BK_CLASSIC", "BK_LINES", "BK_SCORE", "BK_GOLDS", "BK_PATTERN"};
    for (int lv = 0; lv < BK_LEVELS; lv++) {
        const cand *c = &chosen[lv];
        char seq[SEQ_LEN + 1], way[BK_SEQ * 4 + 1];
        for (int i = 0; i < c->pieces; i++) seq[i] = bk_shape_char(c->seq[i]);
        seq[c->pieces] = 0;
        put_way(c, way);
        printf("    {%s, %d, %d, %d, /* %d: %.2f */\n     \"%s\",\n     \"%s\", \"%s\"},\n", GOAL_ID[c->goal],
               c->target, c->par, c->pieces, lv + 1, c->rate, c->grid, seq, way);
    }
    printf("};\n");
    return 0;
}
