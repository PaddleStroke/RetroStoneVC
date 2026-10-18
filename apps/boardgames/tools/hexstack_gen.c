/*
 * Hex Stacks' levels: src/games/hexstack_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of board (small boards of three colours; four colours and goals of one colour; stones on the
 * board; locked cells; larger boards of five colours with stacks already there; the largest, everything) and makes
 * many candidates from fixed seeds: a board shape of the chapter, its stones, locks and stacks laid mirrored about the
 * board's middle, a goal, a tray sequence made by the rules' own chance (hx_chance_stack). A candidate is kept when
 * the rules' player (hx_best, the hint) reaches the goal with at least two cells to spare all the way (its placements:
 * the par), and when most of a set of players choosing worse (noise added to every value) win it too: the chapter's
 * share at least. The difficulty: the share of those players lost, the par, the colours. Each chapter keeps 25 spread
 * over its difficulties (the first level of the first chapter drawn by hand: one stack makes a chain), its last one,
 * the showpiece, the largest of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hxgen apps/boardgames/tools/hexstack_gen.c -lm && \
 *       /tmp/hxgen > apps/boardgames/src/games/hexstack_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define HX_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexstack_rules.c"

#define PER (HX_LEVELS / HX_CHAPTERS)
#define TRIES 150                               /* candidates kept a chapter, at most */
#define PLAYERS 8                               /* the worse players tried on each */
#define NOISE 3000                              /* how much worse they choose */
#define SEQ 60                                  /* the stacks of a level's sequence */

typedef struct {
    char board[96], pre[256], seq[3 * SEQ * 2 + 8];
    int goal, colour, target, colours, par;
    double diff, win, size;
} cand;

/* the shapes (symmetric about the middle of the 7 columns) */
static const char *const SHAPE[] = {
    "..ooo..|.oooo..|..ooo..",                                          /* 0: 10 */
    "..ooo..|.oooo..|..ooo..|.oooo..",                                  /* 1: 14 */
    "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..",                          /* 2: 19, a hexagon */
    "..ooo..|.oooo..|..ooo..|.oooo..|..ooo..|.oooo..|..ooo..",          /* 3: 24, tall */
    "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..",          /* 4: 28 */
    ".ooooo.|oooooo.|.ooooo.|oooooo.|.ooooo.",                          /* 5: 27, wide */
    ".ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.",          /* 6: 32 */
    ".......|.oooo..|.ooooo.|oooooo.|ooooooo|oooooo.|.ooooo.|.oooo..",  /* 7: 37, a hexagon */
    "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..",   /* 8: 37, tall */
};

/* a chapter: its shapes, colours (lo..hi), the deals' press, the goal (HX_TILES or HX_COLOUR or -1 either), the
 * target (lo..hi), stones and locks (pairs, at most), stacks there at the start (pairs, at most), the share of worse
 * players that must win */
typedef struct {
    int shapes[6], nshapes, c0, c1, press, goal, t0, t1, stones, locks, pre, p0, p1;
    double share;
} chapter;

static const chapter CH[HX_CHAPTERS] = {
    {{0, 1, 2, 3}, 4, 3, 3, 0, HX_TILES, 10, 40, 0, 0, 3, 1, 12, 0.9},
    {{1, 2, 3, 4, 5}, 5, 4, 4, 1, -1, 20, 60, 0, 0, 3, 6, 18, 0.75},
    {{2, 3, 4, 5, 6}, 5, 4, 4, 3, -1, 30, 70, 3, 0, 2, 8, 22, 0.65},
    {{3, 4, 5, 6}, 4, 4, 5, 4, -1, 40, 90, 2, 2, 2, 12, 28, 0.6},
    {{4, 6, 7, 8}, 4, 5, 5, 5, -1, 50, 110, 2, 2, 3, 16, 34, 0.55},
    {{6, 7, 8}, 3, 5, 6, 6, -1, 60, 130, 3, 3, 3, 20, 40, 0.5},
};

/* the cell across the board's middle (columns: the doubled x of a cell, 2 col + odd row, mirrored about 6) */
static int mirror(int c)
{
    int x = c % HX_COLS, y = c / HX_COLS, x2 = 12 - (2 * x + (y & 1)), mx = (x2 - (y & 1)) / 2;
    return mx >= 0 && mx < HX_COLS ? mx + y * HX_COLS : -1;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->seq, ((const cand *)b)->seq);
}

/* the board text of a grid of characters (rows of HX_COLS) */
static void board_text(const char *grid, int rows, char *out)
{
    int n = 0;
    for (int y = 0; y < rows; y++) {
        if (y) out[n++] = '|';
        for (int x = 0; x < HX_COLS; x++) out[n++] = grid[x + y * HX_COLS];
    }
    out[n] = 0;
}

/* the level's worth: the rules' player (its par, its room), the worse players' share */
static int judge(cand *c, uint32_t seed)
{
    static hx_game g;
    if (!hx_load(&g, c->board, c->pre, c->seq, c->goal, c->colour, c->target, c->colours)) return 0;
    if (hx_solved(&g)) return 0;
    int won, room, par = hx_play_out(&g, 0, 1, 200, &won, &room);
    if (!won || room < 2 || par < 1) return 0;
    c->par = par;
    int wins = 0;
    for (int k = 0; k < PLAYERS; k++) {
        hx_load(&g, c->board, c->pre, c->seq, c->goal, c->colour, c->target, c->colours);
        hx_play_out(&g, NOISE, seed * 31 + (uint32_t)k, 200, &won, &room);
        wins += won;
        if (k == 3 && wins < 3) return 0;      /* (lost early: not worth the rest) */
    }
    c->win = wins / (double)PLAYERS;
    return 1;
}

static int make(const chapter *C, int ch, uint32_t *r, cand *c)
{
    int sh = C->shapes[pr_below(r, C->nshapes)], rows = 1;
    char grid[HX_CELLS + 1];
    memset(grid, '.', HX_CELLS);
    grid[HX_CELLS] = 0;
    {
        int x = 0, y = 0;
        for (const char *s = SHAPE[sh]; *s; s++) {
            if (*s == '|') { y++; x = 0; continue; }
            grid[x + y * HX_COLS] = *s;
            x++;
        }
        rows = y + 1;
    }
    int open[HX_CELLS], no = 0;
    for (int i = 0; i < HX_CELLS; i++)
        if (grid[i] == 'o') open[no++] = i;
    c->size = no;
    c->colours = C->c0 + pr_below(r, C->c1 - C->c0 + 1);
    /* stones, locks, stacks: each a cell and its mirror (or the middle column's cell alone) */
    hx_tray_stack pre[HX_CELLS];
    int stones = C->stones ? pr_below(r, C->stones + 1) : 0, locks = C->locks ? 1 + pr_below(r, C->locks) : 0;
    int pres = C->pre ? pr_below(r, C->pre + 1) : 0;
    if (ch == 2) stones += 1;
    for (int kind = 0; kind < 3; kind++) {
        int want = kind == 0 ? stones : kind == 1 ? locks : pres;
        for (int k = 0, tries = 0; k < want && tries < 50; tries++) {
            int a = open[pr_below(r, no)], b = mirror(a);
            if (b < 0 || grid[a] != 'o' || grid[b] != 'o') continue;
            char ch_ = kind == 0 ? '#' : kind == 1 ? (char)('1' + pr_below(r, 2 + ch)) : 'S';
            grid[a] = grid[b] = ch_;
            if (kind == 2) {
                hx_tray_stack s;
                hx_chance_stack(r, c->colours, C->press, NULL, 0, &s);
                pre[a] = s;
                pre[b] = s;
            }
            k++;
        }
    }
    board_text(grid, rows, c->board);
    c->pre[0] = 0;
    for (int i = 0; i < HX_CELLS; i++)          /* the stacks there, in the board's reading order */
        if (grid[i] == 'S') {
            char t[16];
            hx_stack_text(&pre[i], t);
            strcat(c->pre, t);
        }
    c->seq[0] = 0;
    for (int i = 0; i < SEQ; i++) {
        hx_tray_stack s;
        char t[16];
        hx_chance_stack(r, c->colours, C->press, NULL, 0, &s);
        hx_stack_text(&s, t);
        strcat(c->seq, t);
    }
    c->goal = C->goal >= 0 ? C->goal : pr_below(r, 2) ? HX_COLOUR : HX_TILES;
    c->colour = pr_below(r, c->colours);
    int span = C->t1 - C->t0;
    c->target = C->t0 + pr_below(r, span / 10 + 1) * 10;
    if (c->goal == HX_COLOUR) c->target = (c->target / 2 + 9) / 10 * 10;
    if (c->target < 10) c->target = 10;
    return 1;
}

int main(void)
{
    static cand pool[TRIES], out[HX_LEVELS];
    printf("/* Hex Stacks' levels: made by tools/hexstack_gen.c (our own boards and sequences, each won by the rules'\n"
           " * player in `par` stacks, and by most players choosing worse; do not edit). Each: the goal, its colour, the "
           "colours\n * dealt, the par, the target, the board, the stacks already there, the tray's sequence "
           "(hexstack.h). */\n");
    for (int ch = 0; ch < HX_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(9100u + (uint32_t)ch);
        int n = 0;
        if (ch == 0) {                          /* the first level: a ring of red round the middle, red to come */
            cand *c = &pool[n];
            strcpy(c->board, "..ooo..|.oSSo..|.oSoSo.|.oSSo..|..ooo..");
            strcpy(c->pre, "BBBBBB");
            strcpy(c->seq, "BEJ");
            for (int i = 3; i < SEQ; i++) {
                hx_tray_stack s;
                char t[16];
                hx_chance_stack(&r, 3, 0, NULL, 0, &s);
                hx_stack_text(&s, t);
                strcat(c->seq, t);
            }
            c->goal = HX_TILES, c->colour = 0, c->target = 10, c->colours = 3, c->size = 19;
            if (!judge(c, 1)) { fprintf(stderr, "the first level fails\n"); return 1; }
            c->diff = -1;
            n++;
        }
        for (int k = 0; k < 60 * TRIES && n < TRIES; k++) {
            cand *c = &pool[n];
            make(C, ch, &r, c);
            if (!judge(c, (uint32_t)k + 7u) || c->win < C->share || c->par < C->p0 || c->par > C->p1) continue;
            int dup = 0;
            for (int i = 0; i < n && !dup; i++) dup = !strcmp(pool[i].board, c->board) && !strcmp(pool[i].pre, c->pre);
            if (dup && c->pre[0]) continue;
            c->diff = (1 - c->win) * 4 + c->par / 10.0 + c->colours * 0.4 + c->target / 80.0 + c->size / 40.0;
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d levels\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 10;
        double step = (double)(n - 2 - lo) / (PER - 2);
        int used[TRIES] = {0};
        for (int k = 0; k < PER - 1; k++) {
            int i = lo + (int)(k * step + 0.5);
            while (used[i]) i++;
            used[i] = 1;
            out[ch * PER + k] = pool[i];
        }
        int show = -1;                          /* the showpiece: the largest of the hardest few */
        for (int i = n - 1; i >= n - 6 && i >= 0; i--)
            if (!used[i] && (show < 0 || pool[i].size + strlen(pool[i].pre) * 0.5 > pool[show].size +
                                                                                   strlen(pool[show].pre) * 0.5))
                show = i;
        out[ch * PER + PER - 1] = pool[show];
        fprintf(stderr, "chapter %d: %d candidates, pars %d .. %d, the share won %.2f .. %.2f\n", ch + 1, n,
                out[ch * PER].par, out[ch * PER + PER - 1].par, out[ch * PER].win, out[ch * PER + PER - 1].win);
    }
    printf("const hx_level_def hx_levels[HX_LEVELS] = {\n");
    static const char *const GOAL[] = {"HX_ENDLESS", "HX_TILES", "HX_COLOUR"};
    for (int l = 0; l < HX_LEVELS; l++) {
        const cand *c = &out[l];
        printf("    {%s, %d, %d, %d, %d, /* %d: %.2f */\n     \"%s\", \"%s\",\n     \"%s\"},\n", GOAL[c->goal], c->colour,
               c->colours, c->par, c->target, l + 1, c->win, c->board, c->pre, c->seq);
    }
    printf("};\n");
    return 0;
}
