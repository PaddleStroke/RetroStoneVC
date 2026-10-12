/*
 * Nonograms: the rules (nonogram.h): the line solver, the house's pictures, the pictures drawn by chance, the player's
 * squares and undo. No SDL: tested by tests/nonogram_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/nonogram.h"

#include <string.h>

#include "games/puzrand.h"

const uint8_t ng_rgb[NG_COLOURS][3] = {
    {0, 0, 0},       {58, 50, 48},   {168, 52, 40},  {222, 168, 66}, {62, 128, 98},  {52, 84, 150},  {242, 234, 216},
    {112, 74, 48},   {134, 64, 126}, {128, 178, 212}, {226, 128, 140}, {104, 156, 64}, {214, 182, 130},
};
static const char LETTERS[NG_COLOURS + 1] = ".kryqbwnpsolm";   /* q jade, l leaf, m sand */

/* the house's pictures: drawn for this game (each checked line-solvable by the tests) */
const ng_picture ng_pictures[] = {
    {5, {".r.r.", "rrrrr", "rrrrr", ".rrr.", "..r.."}},                         /* a heart */
    {5, {"..l..", ".lll.", "lllll", "..n..", ".nnn."}},                         /* a tree */
    {5, {"..w..", "..ww.", "..k..", "nnnnn", ".nnn."}},                         /* a boat */
    {5, {"k...k", "kkkkk", "k.k.k", "kkkkk", ".kkk."}},                         /* a cat */
    {5, {".rrr.", "rrwrr", "rrrrr", "..m..", ".mmm."}},                         /* a mushroom */
    {5, {"..b..", ".bbb.", "b.b.b", "..b..", "..b.."}},                         /* an arrow */
    {10, {".k......k.", ".kk....kk.", ".kkkkkkkk.", ".k.kkkk.k.", ".kkkkkkkk.",   /* a cat */
          ".kkkrrkkk.", ".kkkkkkkk.", "..kkkkkk..", "...kkkk...", "..kkkkkk.."}},
    {10, {"....k.....", "....kw....", "....kww...", "....kwww..", "....kwwww.",   /* a sailing boat */
          "....kwwwww", "....k.....", "nnnnnnnnnn", ".nnnnnnnn.", "..nnnnnn.."}},
    {10, {"...rrrr...", ".rrwrrrrr.", "rrrrrrwrrr", "rwrrrrrrrr", "rrrrrwrrrr",   /* a mushroom */
          ".rrrrrrrr.", "...mmmm...", "...mmmm...", "...mmmm...", "..mmmmmm.."}},
    {10, {"....rr....", "...rrrr...", "..rrrrrr..", ".rrrrrrrr.", "rrrrrrrrrr",   /* a house */
          ".nnnnnnnn.", ".n.nnnn.n.", ".nnnnnnnn.", ".nnnn..nn.", ".nnnn..nn."}},
    {10, {"....ll....", "...llll...", "..llllll..", "...llll...", "..llllll..",   /* a fir tree */
          ".llllllll.", "..llllll..", ".llllllll.", "llllllllll", "....nn...."}},
    {10, {"..........", "..s.s.s...", "...s.s....", ".wwwwwww..", ".wwwwwwwww", /* a cup of tea */
          ".wwwwwww.w", ".wwwwwwwww", "..wwwww...", "...www....", ".nnnnnnnn."}},
    {10, {"..........", "...bbbb...", ".bbbbbbb.b", "bb.bbbbbbb", "bbbbbbbbbb",   /* a fish */
          "bbbbbbbbbb", ".bbbbbbbbb", "..bbbbbb.b", "...bbbb...", ".........."}},
    {15, {".kk.........kk.", ".kkk.......kkk.", ".kkkk.....kkkk.", ".kkkkkkkkkkkkk.",   /* a cat */
          "kkkkkkkkkkkkkkk", "kkk..kkkkk..kkk", "kkk.ykkkkky.kkk", "kkkkkkkkkkkkkkk", "kkkkkkkrkkkkkkk",
          ".kkkk.....kkkk.", "..kkkkk.kkkkk..", "...kkkkkkkkk...", ".....kkkkk.....", "...............",
          "..............."}},
    {15, {".......k.......", ".......kw......", "......wkww.....", ".....wwkwww....",   /* a sailing boat */
          "....wwwkwwww...", "...wwwwkwwwww..", "..wwwwwkwwwwww.", ".......k.......", ".......k.......",
          "nnnnnnnnnnnnnnn", ".nnnnnnnnnnnnn.", "..nnnnnnnnnnn..", "...............", "s.s.s.s.s.s.s.s",
          "sssssssssssssss"}},
    {15, {".......n.......", "......nnn......", "....bbbbbbb....", "..bbbbbbbbbbb..",   /* a teapot */
          "b.bbbbbbbbbbbbb", "bbbbbbbbbbbb..b", ".bbbbbbbbbbb..b", "..bbbbbbbbbbbb.", "..bbbbbbbbbbb..",
          "...bbbbbbbbb...", "....bbbbbbb....", "...............", "..nnnnnnnnnnn..", "...............",
          "..............."}},
    {15, {".......r.......", "......rrr......", "......www......", ".....wwwww.....",   /* a rocket */
          ".....wwsww.....", ".....wsssw.....", ".....wwsww.....", ".....wwwww.....", ".....wwwww.....",
          "....rwwwwwr....", "...rrwwwwwrr...", "..rrr.www.rrr..", ".......y.......", "......yyy......",
          ".......y......."}},
};
const int ng_npictures = (int)(sizeof ng_pictures / sizeof ng_pictures[0]);

/* ---- the line solver --------------------------------------------------------------------------------------------- */
int ng_line(int len, const uint8_t *runs, int k, uint8_t *line)
{
    /* suf[i][j]: squares i.. can hold runs j..; pre[i][j]: squares ..i-1 can hold runs ..j-1 */
    uint8_t suf[NG_MAX + 2][NG_RUNS + 1], pre[NG_MAX + 1][NG_RUNS + 1];
    memset(suf, 0, sizeof suf);
    memset(pre, 0, sizeof pre);
    suf[len][k] = 1;
    suf[len + 1][k] = 1;
    for (int i = len - 1; i >= 0; i--)
        for (int j = k; j >= 0; j--) {
            int ok = line[i] != NG_ON && suf[i + 1][j];
            if (!ok && j < k && i + runs[j] <= len) {
                int e = i + runs[j], fits = 1;
                for (int t = i; t < e && fits; t++) fits = line[t] != NG_OFF;
                if (fits && (e == len || line[e] != NG_ON)) ok = suf[e == len ? len : e + 1][j + 1];
            }
            suf[i][j] = (uint8_t)ok;
        }
    if (!suf[0][0]) return -1;
    pre[0][0] = 1;
    for (int i = 1; i <= len; i++)
        for (int j = 0; j <= k; j++) {
            int ok = line[i - 1] != NG_ON && pre[i - 1][j];
            if (!ok && j > 0 && i - runs[j - 1] >= 0) {
                int s = i - runs[j - 1], fits = 1;
                for (int t = s; t < i && fits; t++) fits = line[t] != NG_OFF;
                if (fits) ok = s == 0 ? j == 1 : line[s - 1] != NG_ON && pre[s - 1][j - 1];
            }
            pre[i][j] = (uint8_t)ok;
        }
    uint8_t can_on[NG_MAX] = {0}, can_off[NG_MAX] = {0};
    for (int c = 0; c < len; c++)
        for (int j = 0; j <= k && !can_off[c]; j++) can_off[c] = line[c] != NG_ON && pre[c][j] && suf[c + 1][j];
    for (int j = 0; j < k; j++)
        for (int s = 0; s + runs[j] <= len; s++) {
            int e = s + runs[j], ok = s == 0 ? j == 0 : line[s - 1] != NG_ON && pre[s - 1][j];
            for (int t = s; t < e && ok; t++) ok = line[t] != NG_OFF;
            if (ok) ok = e == len ? j == k - 1 : line[e] != NG_ON && suf[e + 1][j + 1];
            if (ok)
                for (int t = s; t < e; t++) can_on[t] = 1;
        }
    int found = 0;
    for (int c = 0; c < len; c++) {
        if (!can_on[c] && !can_off[c]) return -1;
        if (line[c] != NG_UNKNOWN) continue;
        if (can_on[c] != can_off[c]) {
            line[c] = can_on[c] ? NG_ON : NG_OFF;
            found++;
        }
    }
    return found;
}

int ng_runs_of(const uint8_t *cells, int len, int step, uint8_t *out)
{
    int n = 0, run = 0;
    for (int i = 0; i <= len; i++) {
        int on = i < len && cells[i * step] == 1;
        if (on) run++;
        else if (run) {
            if (n < NG_RUNS) out[n] = (uint8_t)run;
            n++;
            run = 0;
        }
    }
    return n;
}

static void clues_of(int n, const uint8_t *pic, uint8_t clue[2][NG_MAX][NG_RUNS], uint8_t runs[2][NG_MAX])
{
    for (int i = 0; i < n; i++) {
        runs[0][i] = (uint8_t)ng_runs_of(pic + i * n, n, 1, clue[0][i]);
        runs[1][i] = (uint8_t)ng_runs_of(pic + i, n, n, clue[1][i]);
    }
}

int ng_solvable(int n, const uint8_t *pic)
{
    uint8_t clue[2][NG_MAX][NG_RUNS], runs[2][NG_MAX], st[NG_SQ], line[NG_MAX];
    clues_of(n, pic, clue, runs);
    for (int i = 0; i < n; i++)
        if (runs[0][i] > NG_RUNS || runs[1][i] > NG_RUNS) return 0;
    memset(st, NG_UNKNOWN, sizeof st);
    for (int changed = 1; changed;) {
        changed = 0;
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                int step = way ? n : 1, at = way ? i : i * n;
                for (int t = 0; t < n; t++) line[t] = st[at + t * step];
                int f = ng_line(n, clue[way][i], runs[way][i], line);
                if (f < 0) return 0;
                if (!f) continue;
                changed = 1;
                for (int t = 0; t < n; t++) st[at + t * step] = line[t];
            }
    }
    for (int s = 0; s < n * n; s++)
        if (st[s] != (pic[s] ? NG_ON : NG_OFF)) return 0;
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
void ng_load(ng_game *g, int n, const uint8_t *pic, const uint8_t *colour, int index)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->pic = index;
    for (int s = 0; s < n * n; s++) {
        g->sol[s] = pic[s] ? 1 : 0;
        g->colour[s] = colour ? colour[s] : 0;
    }
    clues_of(n, g->sol, g->clue, g->runs);
}

int ng_load_picture(ng_game *g, int index)
{
    if (index < 0 || index >= ng_npictures) return 0;
    const ng_picture *p = &ng_pictures[index];
    uint8_t pic[NG_SQ], col[NG_SQ];
    for (int y = 0; y < p->n; y++)
        for (int x = 0; x < p->n; x++) {
            const char *l = strchr(LETTERS, p->rows[y][x]);
            int c = l ? (int)(l - LETTERS) : 0;
            pic[y * p->n + x] = c != 0;
            col[y * p->n + x] = (uint8_t)c;
        }
    ng_load(g, p->n, pic, col, index);
    return 1;
}

/* a picture by chance: squares filled at random on the left half and mirrored, then smoothed into blobs (each square
 * taking what most of its neighbourhood is); in one colour, a lighter one on its top edges */
static void draw_by_chance(int n, uint32_t *s, uint8_t *pic, uint8_t *col)
{
    uint8_t a[NG_SQ];
    int fill = 50 + pr_below(s, 14), half = (n + 1) / 2;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < half; x++) a[y * n + x] = a[y * n + n - 1 - x] = (uint8_t)(pr_below(s, 100) < fill);
    for (int pass = 0; pass < (n >= 10 ? 1 : 0); pass++) {
        uint8_t b[NG_SQ];
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int on = 0, all = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int u = x + dx, v = y + dy;
                        if (u < 0 || v < 0 || u >= n || v >= n) continue;
                        all++;
                        on += a[v * n + u];
                    }
                b[y * n + x] = (uint8_t)(2 * on > all || (2 * on == all && a[y * n + x]));
            }
        memcpy(a, b, sizeof b);
    }
    static const uint8_t inks[][2] = {{5, 9}, {4, 11}, {2, 10}, {8, 10}, {1, 7}, {7, 12}};   /* deep, light */
    const uint8_t *ink = inks[pr_below(s, 6)];
    for (int i = 0; i < n * n; i++) {
        pic[i] = a[i];
        int top = i < n || !a[i - n];
        col[i] = a[i] ? ink[top] : 0;
    }
}

void ng_new(ng_game *g, int variant, uint32_t seed)
{
    int n = variant == 0 ? 5 : variant == 1 ? 10 : 15;
    uint32_t s = pr_seed(seed);
    if (pr_below(&s, 2)) {                      /* one of the house's pictures of that size */
        int k = 0, which[32];
        for (int i = 0; i < ng_npictures; i++)
            if (ng_pictures[i].n == n) which[k++] = i;
        if (k && ng_load_picture(g, which[pr_below(&s, k)])) return;
    }
    uint8_t pic[NG_SQ], col[NG_SQ];
    for (int tries = 0; tries < 4000; tries++) {
        draw_by_chance(n, &s, pic, col);
        int on = 0;
        for (int i = 0; i < n * n; i++) on += pic[i];
        if (on < n * n / 4 || on > n * n * 4 / 5) continue;   /* not too bare, not too full */
        if (ng_solvable(n, pic)) {
            ng_load(g, n, pic, col, -1);
            return;
        }
    }
    for (int i = 0; i < ng_npictures; i++)      /* never (the tests say so): a house picture */
        if (ng_pictures[i].n == n && ng_load_picture(g, i)) return;
}

/* ---- the player's squares ---------------------------------------------------------------------------------------- */
/* the wrong marks on the board: a square filled off the picture, a cross on it (the stars' mistakes) */
static int wrongs(const ng_game *g)
{
    int k = 0;
    for (int s = 0; s < g->n * g->n; s++) k += g->sq[s] == NG_FILL ? !g->sol[s] : g->sq[s] == NG_CROSS && g->sol[s];
    return k;
}

int ng_paint(ng_game *g, int a, int b, int to)
{
    int n = g->n;
    if (a < 0 || b < 0 || a >= n * n || b >= n * n || to < NG_EMPTY || to > NG_CROSS) return 0;
    int ax = a % n, ay = a / n, bx = b % n, by = b / n;
    if (ax != bx && ay != by) return 0;
    int step = ax == bx ? (by > ay ? n : -n) : (bx > ax ? 1 : -1);
    int from = g->sq[a];
    bench_begin(&g->hist, NG_MAX);
    for (int s = a;; s += step) {
        if (g->sq[s] == from && from != to) {
            bench_log(&g->hist, s, g->sq[s]);
            g->sq[s] = (uint8_t)to;
        }
        if (s == b) break;
    }
    return bench_done(&g->hist, wrongs(g));
}

static void put_back(void *ctx, int s, uint32_t was) { ((ng_game *)ctx)->sq[s] = (uint8_t)was; }

int ng_undo(ng_game *g)
{
    int s = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return s;
}


int ng_line_done(const ng_game *g, int way, int i)
{
    int n = g->n, step = way ? n : 1, at = way ? i : i * n;
    uint8_t r[NG_RUNS + 1];
    int k = ng_runs_of(g->sq + at, n, step, r);
    if (k != g->runs[way][i]) return 0;
    for (int j = 0; j < k; j++)
        if (r[j] != g->clue[way][i][j]) return 0;
    return 1;
}

int ng_solved(const ng_game *g)
{
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < g->n; i++)
            if (!ng_line_done(g, way, i)) return 0;
    return 1;
}

int ng_square(const ng_game *g, const char *s)
{
    int x = s[0] - 'a', y = 0, k = 1;
    if (x < 0 || x >= g->n || s[1] < '1' || s[1] > '9') return -1;
    for (; s[k] >= '0' && s[k] <= '9' && k < 3; k++) y = y * 10 + s[k] - '0';
    return y < 1 || y > g->n ? -1 : (y - 1) * g->n + x;
}

static int square_len(const char *s)            /* the letters a square takes: "c4" 2, "c12" 3 */
{
    return s[2] >= '0' && s[2] <= '9' ? 3 : 2;
}

int ng_script(ng_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '=') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int i = 0; i < g->n * g->n && k > 0; i++)
                if (g->sol[i] && g->sq[i] != NG_FILL) { ng_paint(g, i, i, NG_FILL); k--; }
            continue;
        }
        int cross = *s == 'x';
        if (cross) s++;
        int a = ng_square(g, s);
        if (a < 0) return 0;
        s += square_len(s);
        int b = a;
        if (*s == '-') {
            b = ng_square(g, s + 1);
            if (b < 0) return 0;
            s += 1 + square_len(s + 1);
        }
        int want = cross ? NG_CROSS : NG_FILL;
        int to = a == b && g->sq[a] == want ? NG_EMPTY : want;   /* a square tapped again: emptied */
        if (!ng_paint(g, a, b, to)) return 0;
    }
    return 1;
}
