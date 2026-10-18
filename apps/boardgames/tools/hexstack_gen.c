/*
 * Hex Stacks' levels: src/games/hexstack_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of board (small boards of four or five colours; goals of one colour; stones on the board;
 * locked cells; larger boards of six colours with stacks already there; the largest, everything) and makes many
 * candidates from fixed seeds: a board shape of the chapter, its stones, locks and stacks laid mirrored about the
 * board's middle, a goal, a tray sequence made by the rules' own chance (hx_chance_stack). A candidate is kept when
 * the rules' player (hx_best, the hint) reaches the goal with at least two cells to spare all the way (its placements:
 * the par), when a naive player (a stack and a cell by chance) mostly loses it, and when the share of players choosing
 * worse (noise added to every value) that win is within the chapter's range: easy chapters forgive more. The
 * difficulty: those shares, the par, the colours, the size. Each chapter opens with two looser levels (its new idea;
 * the first chapter's first level drawn by hand: one stack makes a chain), then 22 spread over the rest by difficulty
 * (no two boards of one shape in a row when it can help), its last one, the showpiece, the largest of its hardest
 * few. The per-level table and the chapters' summary go to stderr. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hxgen apps/boardgames/tools/hexstack_gen.c -lm && \
 *       /tmp/hxgen > apps/boardgames/src/games/hexstack_levels.h
 * The same table for the levels already in hexstack_levels.h (nothing written):
 *   gcc -O2 -DREPORT -Iapps/boardgames/src -o /tmp/hxrep apps/boardgames/tools/hexstack_gen.c -lm && /tmp/hxrep
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
#define PLAYERS 16                              /* the worse players tried on each */
#define NOISE 3000                              /* how much worse they choose */
#define SEQ 80                                  /* the stacks of a level's sequence */

typedef struct {
    char board[96], pre[256], seq[3 * SEQ * 2 + 8];
    int goal, colour, target, colours, par;
    double diff, win, size, hard, rnd;
    int shape;
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
    "..ooo..|.oooo..|.oo.oo.|.oooo..|..ooo..",                          /* 9: 18, a ring */
    "...o...|..oo...|..ooo..|.oooo..|..ooo..|..oo...|...o...",          /* 10: 16, a diamond */
};

/* a chapter: its shapes, colours (lo..hi), the deals' press, the goal (HX_TILES or HX_COLOUR or -1 either), the
 * target (lo..hi), stones and locks (pairs, at most), stacks there at the start (pairs, at most), the par (lo..hi);
 * then the share of worse players (noise NOISE) that must win (lo..hi), the share of naive players (a stack and a cell
 * by chance) that may win at most, and the share of much worse ones (noise HARD) at most. The chapter's first two
 * levels (its new idea) may be looser: naive players up to INTRO_RND, worse ones at least INTRO_WIN. */
typedef struct {
    int shapes[6], nshapes, c0, c1, press, goal, t0, t1, stones, locks, pre, p0, p1;
    double wlo, whi, rmax, hmax;
} chapter;

#define INTRO_RND 0.7
#define INTRO_WIN 0.8

static const chapter CH[HX_CHAPTERS] = {
    {{0, 1, 9, 10}, 4, 4, 5, 0, HX_TILES, 40, 120, 0, 0, 3, 4, 30, 0.5, 0.95, 0.3, 1.0},
    {{1, 2, 3, 9, 10}, 5, 4, 5, 1, -1, 60, 160, 0, 0, 3, 8, 40, 0.45, 0.9, 0.2, 0.9},
    {{2, 3, 4, 5, 6}, 5, 5, 5, 3, -1, 80, 180, 3, 0, 2, 10, 45, 0.4, 0.9, 0.12, 0.8},
    {{3, 4, 5, 6}, 4, 5, 6, 4, -1, 100, 200, 2, 2, 2, 14, 50, 0.35, 0.85, 0.06, 0.7},
    {{4, 6, 7, 8}, 4, 6, 6, 5, -1, 120, 220, 2, 2, 3, 18, 55, 0.3, 0.75, 0.03, 0.55},
    {{6, 7, 8}, 3, 6, 6, 6, -1, 160, 280, 3, 3, 3, 22, 65, 0.2, 0.6, 0.0, 0.35},
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
    }
    c->win = wins / (double)PLAYERS;
    return 1;
}

/* a naive player: a tray stack and an empty cell by chance; 1 won */
static int random_play(const cand *c, uint32_t seed)
{
    static hx_game g;
    hx_load(&g, c->board, c->pre, c->seq, c->goal, c->colour, c->target, c->colours);
    uint32_t r = pr_seed(seed);
    for (int k = 0; k < 200 && !hx_over(&g); k++) {
        int sl[HX_TRAY], ns = 0, ce[HX_CELLS], nc = 0;
        for (int s = 0; s < HX_TRAY; s++)
            if (g.p.tray[s].n) sl[ns++] = s;
        for (int i = 0; i < HX_CELLS; i++)
            if (g.p.kind[i] == HX_OPEN && !g.p.n[i]) ce[nc++] = i;
        if (!ns || !nc || !hx_place(&g, sl[pr_below(&r, ns)], ce[pr_below(&r, nc)])) break;
    }
    return hx_solved(&g);
}

/* the difficulty metrics past judge(): the share of naive players (a stack and a cell by chance) and of much worse
 * players (noise HARD) that win, MEASURE each */
#define MEASURE 16
#define HARD 9000
static void measure(cand *c)
{
    static hx_game g;
    int a = 0, b = 0, won, room;
    for (int k = 0; k < MEASURE; k++) {
        a += random_play(c, 77u + (uint32_t)k);
        hx_load(&g, c->board, c->pre, c->seq, c->goal, c->colour, c->target, c->colours);
        hx_play_out(&g, HARD, 991u + (uint32_t)k, 200, &won, &room);
        b += won;
    }
    c->rnd = a / (double)MEASURE;
    c->hard = b / (double)MEASURE;
}

static void table_row(int l, const cand *c)
{
    fprintf(stderr, "%4d %3d %3d %4d %4d %5.2f %5.2f %5.2f %6.2f\n", l + 1, (int)c->size, c->colours, c->target, c->par,
            c->win, c->hard, c->rnd, c->diff);
}

static void table(const cand *lv, int n, int per)
{
    fprintf(stderr, "lvl size col tgt  par   win  hard  rand   diff\n");
    for (int l = 0; l < n; l++) table_row(l, &lv[l]);
    fprintf(stderr, "chapter: par mean, win (noise %d) mean, hard (noise %d) mean, rand mean; trivial (rand >= 0.5)\n",
            NOISE, HARD);
    for (int ch = 0; ch * per < n; ch++) {
        double p = 0, w = 0, h = 0, r = 0;
        int t = 0, k = 0;
        for (int l = ch * per; l < n && l < (ch + 1) * per; l++, k++) {
            p += lv[l].par, w += lv[l].win, h += lv[l].hard, r += lv[l].rnd;
            t += lv[l].rnd >= 0.5;
        }
        fprintf(stderr, "  %d: par %5.1f  win %.2f  hard %.2f  rand %.2f  trivial %d/%d\n", ch + 1, p / k, w / k, h / k,
                r / k, t, k);
    }
}

static int make(const chapter *C, int ch, uint32_t *r, cand *c)
{
    int sh = C->shapes[pr_below(r, C->nshapes)], rows = 1;
    c->shape = sh;
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

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in hexstack_levels.h, no output */
#include "games/hexstack_levels.h"
static int report(void)
{
    static cand lv[HX_LEVELS];
    for (int l = 0; l < HX_LEVELS; l++) {
        const hx_level_def *d = &hx_levels[l];
        cand *c = &lv[l];
        snprintf(c->board, sizeof c->board, "%s", d->board);
        snprintf(c->pre, sizeof c->pre, "%s", d->pre);
        snprintf(c->seq, sizeof c->seq, "%s", d->seq);
        c->goal = d->goal, c->colour = d->colour, c->target = d->target, c->colours = d->colours;
        c->size = 0;
        for (const char *s = d->board; *s; s++) c->size += *s != '.' && *s != '|' && *s != '#';
        if (!judge(c, (uint32_t)l + 7u)) fprintf(stderr, "level %d: not won\n", l + 1);
        measure(c);
        c->diff = 0;
    }
    table(lv, HX_LEVELS, PER);
    return 0;
}
#endif

int main(void)
{
#ifdef REPORT
    return report();
#endif
    static cand pool[TRIES], out[HX_LEVELS], first;
    printf("/* Hex Stacks' levels: made by tools/hexstack_gen.c (our own boards and sequences, each won by the rules'\n"
           " * player in `par` stacks, and by most players choosing worse; do not edit). Each: the goal, its colour, the "
           "colours\n * dealt, the par, the target, the board, the stacks already there, the tray's sequence "
           "(hexstack.h). */\n");
    for (int ch = 0; ch < HX_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(9100u + (uint32_t)ch);
        int n = 0;
        if (ch == 0) {                          /* the first level: a ring of red round the middle, red to come */
            cand *c = &first;
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
            measure(c);
            c->diff = -1;
            c->shape = 2;
        }
        static cand intro[TRIES];
        int ni = 0, want_intro = 2;
        int why[6] = {0};
        for (int k = 0; k < 400 * TRIES && (n < TRIES || (ni < want_intro && k < 150 * TRIES)); k++) {
            static cand tmp;
            cand *c = &tmp;
            make(C, ch, &r, c);
            int rw = 0;                         /* the naive players first: cheap */
            for (int i = 0; i < MEASURE; i++) rw += random_play(c, 77u + (uint32_t)i);
            c->rnd = rw / (double)MEASURE;
            if (c->rnd > INTRO_RND) { why[0]++; continue; }
            int is_intro = c->rnd > C->rmax;
            if (is_intro && ni >= 3 * want_intro) continue;
            if (!is_intro && n >= TRIES) continue;
            if (!judge(c, (uint32_t)k + 7u)) { why[1]++; continue; }
            if (c->par < C->p0 || c->par > C->p1) { why[2]++; continue; }
            measure(c);
            if (is_intro ? c->win < INTRO_WIN : c->win < C->wlo) { why[3]++; continue; }
            if (!is_intro && (c->win > C->whi || c->hard > C->hmax)) { why[4]++; continue; }
            cand *lst = is_intro ? intro : pool;
            int m = is_intro ? ni : n, dup = 0;
            for (int i = 0; i < m && !dup; i++) dup = !strcmp(lst[i].board, c->board) && !strcmp(lst[i].pre, c->pre);
            if (dup && c->pre[0]) continue;
            c->diff = (1 - c->win) * 3 + (1 - c->hard) * 1.5 + (1 - c->rnd) * 1.5 + c->par / 15.0 + c->colours * 0.2 +
                      c->size / 60.0;
            if (is_intro)
                intro[ni++] = *c;
            else
                pool[n++] = *c;
        }
        fprintf(stderr, "chapter %d: naive won %d, lost by the hint %d, par %d, worse lost %d, too easy %d\n", ch + 1,
                why[0], why[1], why[2], why[3], why[4]);
        if (n < PER) {
            fprintf(stderr, "chapter %d: only %d levels, %d intro\n", ch + 1, n, ni);
            return 1;
        }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        qsort(intro, (size_t)ni, sizeof *intro, by_diff);
        if (ni > want_intro) ni = want_intro;   /* (too few: the pool's easiest instead) */
        int base = ch * PER, at = 0;
        if (ch == 0) out[base + at++] = first;
        for (int i = 0; i < ni; i++) out[base + at++] = intro[i];
        int rest = PER - at - 1;                /* spread over the pool; the showpiece last */
        double step = (double)(n - 7) / (rest - 1);
        int used[TRIES] = {0};
        for (int k = 0; k < rest; k++) {
            int i = (int)(k * step + 0.5);
            while (used[i]) i++;
            /* not the same board shape as the level before, when a near one differs */
            for (int j = i; j < n && j < i + 8; j++)
                if (!used[j] && pool[j].shape != out[base + at - 1].shape) { i = j; break; }
            used[i] = 1;
            out[base + at++] = pool[i];
        }
        int show = -1;                          /* the showpiece: the largest of the hardest few */
        for (int i = n - 1; i >= n - 6 && i >= 0; i--)
            if (!used[i] && (show < 0 || pool[i].size + strlen(pool[i].pre) * 0.5 > pool[show].size +
                                                                                   strlen(pool[show].pre) * 0.5))
                show = i;
        out[base + at++] = pool[show];
        fprintf(stderr, "chapter %d: %d candidates (%d intro), pars %d .. %d\n", ch + 1, n, ni, out[base].par,
                out[base + PER - 1].par);
    }
    table(out, HX_LEVELS, PER);
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
