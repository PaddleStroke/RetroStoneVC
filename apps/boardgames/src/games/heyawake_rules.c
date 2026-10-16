/*
 * Heyawake: the rules (heyawake.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/heyawake_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/heyawake.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define WHITE 1u                                /* a cell's values: bit 0 unshaded, bit 1 shaded */
#define SHADE 2u
#define BOTH 3u
#define SEGS (2 * HYK_MAX * HYK_MAX)            /* the lines through three rooms a grid can have, at most */

static const int SIZES[HYK_SIZES] = {6, 8, 10};

int hyk_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % HYK_SIZES]; }
int hyk_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a grid: the neighbours, the rooms' cells, the lines through three rooms ----------- *
 * A line through three rooms: in a row (or a column), the last cell of a room's stretch, all of the next one's and
 * the first of the one after; one of them at least is shaded. */
typedef struct {
    const hyk_puzzle *p;
    int n, cells, rooms, segs;
    uint8_t nn[HYK_CELLS], nb[HYK_CELLS][4];
    uint8_t rcell[HYK_CELLS];                   /* the cells room by room: rstart[r] .. rstart[r + 1] - 1 */
    int16_t rstart[HYK_CELLS + 1];
    uint8_t scell[6 * HYK_CELLS];               /* the lines' cells: sstart[s] .. sstart[s + 1] - 1 */
    int16_t sstart[SEGS + 1];
} ctx;

/* the lines through three rooms of one row or column: its cells first, then each step (along: 1, down: n) */
static void line_segs(ctx *k, int first, int step)
{
    int n = k->n, start[HYK_MAX + 1], runs = 0;
    for (int i = 0; i < n; i++)
        if (!i || k->p->room[first + i * step] != k->p->room[first + (i - 1) * step]) start[runs++] = i;
    start[runs] = n;
    for (int r = 0; r + 2 < runs; r++) {
        int at = k->sstart[k->segs];
        for (int i = start[r + 1] - 1; i <= start[r + 2]; i++) k->scell[at++] = (uint8_t)(first + i * step);
        k->sstart[++k->segs] = (int16_t)at;
    }
}

static void ctx_make(ctx *k, const hyk_puzzle *p)
{
    int n = p->n;
    memset(k, 0, sizeof *k);
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->rooms = p->rooms;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
    }
    int at = 0;
    for (int r = 0; r < p->rooms; r++) {
        k->rstart[r] = (int16_t)at;
        for (int c = 0; c < n * n; c++)
            if (p->room[c] == r) k->rcell[at++] = (uint8_t)c;
    }
    k->rstart[p->rooms] = (int16_t)at;
    for (int i = 0; i < n; i++) {
        line_segs(k, i * n, 1);
        line_segs(k, i, n);
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = BOTH;
}

static void force(bench_grid *g, int c, unsigned to) { g->d[c] &= (uint16_t)to; }

static int settled(const bench_grid *was, const bench_grid *g, int cells)
{
    int ch = 0;
    for (int c = 0; c < cells; c++) {
        if (!g->d[c]) return -1;
        ch |= g->d[c] != was->d[c];
    }
    return ch;
}

/* the pieces of unshaded cells known: each cell's piece (-1: not known unshaded); how many */
static int white_pieces(const ctx *k, const bench_grid *g, int16_t *of)
{
    int stack[HYK_CELLS], count = 0;
    for (int c = 0; c < k->cells; c++) of[c] = -1;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != WHITE || of[c] >= 0) continue;
        int sp = 0;
        of[c] = (int16_t)count;
        stack[sp++] = c;
        while (sp) {
            int x = stack[--sp];
            for (int j = 0; j < k->nn[x]; j++) {
                int o = k->nb[x][j];
                if (g->d[o] == WHITE && of[o] < 0) of[o] = (int16_t)count, stack[sp++] = o;
            }
        }
        count++;
    }
    return count;
}

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    int cells = k->cells;
    for (int c = 0; c < cells; c++) {           /* round a shaded cell nothing is shaded */
        if (w.d[c] != SHADE) continue;
        for (int j = 0; j < k->nn[c]; j++) {
            if (w.d[k->nb[c][j]] == SHADE) return -1;
            force(g, k->nb[c][j], WHITE);
        }
    }
    for (int r = 0; r < k->rooms; r++) {        /* the rooms' numbers */
        int want = k->p->num[r], s = 0, u = 0;
        if (want < 0) continue;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) {
            s += w.d[k->rcell[i]] == SHADE;
            u += w.d[k->rcell[i]] == BOTH;
        }
        if (s > want || s + u < want) return -1;
        if (!u || (s < want && s + u > want)) continue;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++)
            if (w.d[k->rcell[i]] == BOTH) force(g, k->rcell[i], s == want ? WHITE : SHADE);
    }
    for (int s = 0; s < k->segs; s++) {         /* a line through three rooms: one shaded */
        int shaded = 0, u = 0, last = -1;
        for (int i = k->sstart[s]; i < k->sstart[s + 1]; i++) {
            int c = k->scell[i];
            shaded += w.d[c] == SHADE;
            if (w.d[c] == BOTH) u++, last = c;
        }
        if (shaded) continue;
        if (!u) return -1;
        if (u == 1) force(g, last, SHADE);
    }
    int16_t of[HYK_CELLS];                      /* a piece of unshaded cells with one way out takes it */
    int np = white_pieces(k, &w, of);
    if (np > 1) {
        int ways[HYK_CELLS], way[HYK_CELLS];
        memset(ways, 0, sizeof ways);
        for (int c = 0; c < cells; c++) {
            if (w.d[c] != BOTH) continue;
            int got[4], ng = 0;
            for (int j = 0; j < k->nn[c]; j++) {
                int q = of[k->nb[c][j]], dup = 0;
                if (q < 0) continue;
                for (int i = 0; i < ng; i++) dup |= got[i] == q;
                if (dup) continue;
                got[ng++] = q;
                ways[q]++;
                way[q] = c;
            }
        }
        for (int q = 0; q < np; q++) {
            if (!ways[q]) return -1;
            if (ways[q] == 1) force(g, way[q], WHITE);
        }
    }
    return settled(&w, g, cells);
}

/* the cells not yet known the unshaded ones cannot do without: the cut points of the unshaded and unknown cells
 * (depth first, the low points), each part's unshaded cells counted */
typedef struct {
    const ctx *k;
    const bench_grid *g;
    int disc[HYK_CELLS], low[HYK_CELLS], sub[HYK_CELLS], t, whites;
    uint8_t *cut;
} cuts;

static void cut_dfs(cuts *D, int u, int parent)
{
    const ctx *k = D->k;
    int split = 0, apart = 0;
    D->disc[u] = D->low[u] = ++D->t;
    D->sub[u] = D->g->d[u] == WHITE;
    for (int j = 0; j < k->nn[u]; j++) {
        int v = k->nb[u][j];
        if (D->g->d[v] == SHADE) continue;
        if (!D->disc[v]) {
            cut_dfs(D, v, u);
            D->sub[u] += D->sub[v];
            if (D->low[v] < D->low[u]) D->low[u] = D->low[v];
            if (D->low[v] >= D->disc[u] && D->sub[v]) split++, apart += D->sub[v];
        } else if (v != parent && D->disc[v] < D->low[u]) {
            D->low[u] = D->disc[v];
        }
    }
    if (D->g->d[u] == BOTH && split + (D->whites - apart > 0) >= 2) D->cut[u] = 1;
}

/* 0: the unshaded cells cannot be joined any more */
static int white_cuts(const ctx *k, const bench_grid *g, uint8_t *cut)
{
    cuts D;
    int first = -1;
    memset(cut, 0, (size_t)k->cells);
    memset(D.disc, 0, sizeof D.disc);
    D.k = k;
    D.g = g;
    D.t = D.whites = 0;
    D.cut = cut;
    for (int c = 0; c < k->cells; c++)
        if (g->d[c] == WHITE) {
            D.whites++;
            if (first < 0) first = c;
        }
    if (D.whites < 2) return 1;
    cut_dfs(&D, first, -1);
    return D.sub[first] == D.whites;
}

/* medium */
static int medium(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    uint8_t cut[HYK_CELLS];
    if (!white_cuts(k, &w, cut)) return -1;
    for (int c = 0; c < k->cells; c++)
        if (cut[c]) force(g, c, WHITE);
    return settled(&w, g, k->cells);
}

static long finds[2];                           /* each tier's finds (hyk_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    for (int c = 0; c < k->cells; c++)
        for (int j = 0; j < k->nn[c] && g->d[c] == SHADE; j++)
            if (g->d[k->nb[c][j]] == SHADE) return 0;
    for (int r = 0; r < k->rooms; r++) {
        int s = 0;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) s += g->d[k->rcell[i]] == SHADE;
        if (k->p->num[r] >= 0 && s != k->p->num[r]) return 0;
    }
    for (int s = 0; s < k->segs; s++) {
        int shaded = 0;
        for (int i = k->sstart[s]; i < k->sstart[s + 1]; i++) shaded += g->d[k->scell[i]] == SHADE;
        if (!shaded) return 0;
    }
    int16_t of[HYK_CELLS];
    return white_pieces(k, g, of) == 1;
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const hyk_puzzle *p, uint8_t *sol, long *work, int cap, long budget)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = budget;
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = g.d[c] == SHADE;
    return lv;
}

int hyk_grade(const hyk_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0); }

int hyk_count(const hyk_puzzle *p, int limit)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int hyk_effort(const hyk_puzzle *p, long *passes, long *mediums)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    finds[1] = 0;
    int lv = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    if (mediums) *mediums = finds[1];
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
static const char B36[] = "0123456789abcdefghijklmnopqrstuvwxyz";

int hyk_encode(const hyk_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + 5 * p->rooms + 1 > size) return 0;
    for (int r = 0; r < p->rooms; r++) {
        int x0 = n, y0 = n, x1 = -1, y1 = -1;
        for (int c = 0; c < n * n; c++) {
            if (p->room[c] != r) continue;
            int x = c % n, y = c / n;
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
        }
        if (x1 < 0) return 0;
        out[len++] = B36[x0];
        out[len++] = B36[y0];
        out[len++] = B36[x1 - x0 + 1];
        out[len++] = B36[y1 - y0 + 1];
        out[len++] = p->num[r] < 0 ? '.' : B36[p->num[r] % 36];
    }
    out[len] = 0;
    return len;
}

static int b36(char ch)
{
    const char *d = ch ? strchr(B36, ch) : NULL;
    return d ? (int)(d - B36) : -1;
}

int hyk_decode(const char *s, hyk_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > HYK_MAX) return 0;
    p->n = n;
    memset(p->room, 0xff, sizeof p->room);
    while (*s) {
        if (strlen(s) < 5 || p->rooms >= HYK_CELLS) return 0;
        int x = b36(s[0]), y = b36(s[1]), w = b36(s[2]), h = b36(s[3]), num = s[4] == '.' ? -1 : b36(s[4]);
        if (x < 0 || y < 0 || w < 1 || h < 1 || x + w > n || y + h > n || (s[4] != '.' && num < 0)) return 0;
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) {
                if (p->room[yy * n + xx] != 0xff) return 0;
                p->room[yy * n + xx] = (uint8_t)p->rooms;
            }
        p->num[p->rooms++] = (int8_t)num;
        s += 5;
    }
    for (int c = 0; c < n * n; c++)
        if (p->room[c] == 0xff) return 0;
    return 1;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* rooms cut by chance: at each cell not yet in a room (in reading order) a rectangle of up to 4 x 4, seldom 1 x 1 */
static void cut_rooms(hyk_puzzle *p, int n, uint32_t *s)
{
    memset(p, 0, sizeof *p);
    p->n = n;
    memset(p->room, 0xff, sizeof p->room);
    for (int c = 0; c < n * n; c++) {
        if (p->room[c] != 0xff) continue;
        int x = c % n, y = c / n, maxw = 1, w = 1, h = 1;
        while (x + maxw < n && maxw < 4 && p->room[c + maxw] == 0xff) maxw++;
        int maxh = n - y < 4 ? n - y : 4;
        for (int t = 0; t < 4 && w * h == 1; t++) {
            w = 1 + pr_below(s, maxw);
            h = 1 + pr_below(s, maxh);
            for (int ok = 0; !ok;) {            /* lower rows may be taken by a room begun further right */
                ok = 1;
                for (int yy = y; yy < y + h && ok; yy++)
                    for (int xx = x; xx < x + w; xx++)
                        if (p->room[yy * n + xx] != 0xff) ok = 0;
                if (!ok) h--;
            }
        }
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) p->room[yy * n + xx] = (uint8_t)p->rooms;
        p->num[p->rooms++] = -1;
    }
}

/* the unshaded cells of sh (1: shaded) in one piece */
static int joined(const ctx *k, const uint8_t *sh)
{
    int stack[HYK_CELLS], sp = 0, got = 0, whites = 0, first = -1;
    uint8_t seen[HYK_CELLS];
    memset(seen, 0, sizeof seen);
    for (int c = 0; c < k->cells; c++)
        if (!sh[c]) {
            whites++;
            if (first < 0) first = c;
        }
    if (first < 0) return 0;
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int x = stack[--sp];
        got++;
        for (int j = 0; j < k->nn[x]; j++) {
            int o = k->nb[x][j];
            if (!sh[o] && !seen[o]) seen[o] = 1, stack[sp++] = o;
        }
    }
    return got == whites;
}

/* c can be shaded: no shaded cell beside it, the rest kept joined */
static int can_shade(const ctx *k, uint8_t *sh, int c)
{
    if (sh[c]) return 0;
    for (int j = 0; j < k->nn[c]; j++)
        if (sh[k->nb[c][j]]) return 0;
    sh[c] = 1;
    int ok = joined(k, sh);
    sh[c] = 0;
    return ok;
}

/* every line through three rooms has a shaded cell */
static int lines_broken(const ctx *k, const uint8_t *sh)
{
    for (int s = 0; s < k->segs; s++) {
        int shaded = 0;
        for (int i = k->sstart[s]; i < k->sstart[s + 1]; i++) shaded |= sh[k->scell[i]];
        if (!shaded) return 0;
    }
    return 1;
}

/* the shading: a cell shaded by chance in each line through three rooms that has none (when none can be, the shaded
 * cells beside one of its cells taken away first), then a few more; 0: stuck */
static int shade(const ctx *k, uint8_t *sh, uint32_t *s, int extra)
{
    memset(sh, 0, HYK_CELLS);
    for (int round = 0; round < 6 * k->cells; round++) {
        int open[SEGS], no = 0;
        for (int g = 0; g < k->segs; g++) {
            int shaded = 0;
            for (int i = k->sstart[g]; i < k->sstart[g + 1]; i++) shaded |= sh[k->scell[i]];
            if (!shaded) open[no++] = g;
        }
        if (!no) break;
        int g = open[pr_below(s, no)], opts[HYK_MAX + 2], nopt = 0, len = k->sstart[g + 1] - k->sstart[g];
        for (int i = k->sstart[g]; i < k->sstart[g + 1]; i++)
            if (can_shade(k, sh, k->scell[i])) opts[nopt++] = k->scell[i];
        if (nopt) {
            sh[opts[pr_below(s, nopt)]] = 1;
            continue;
        }
        int c = k->scell[k->sstart[g] + pr_below(s, len)];   /* room made beside a cell of it */
        for (int j = 0; j < k->nn[c]; j++) sh[k->nb[c][j]] = 0;
        if (can_shade(k, sh, c)) sh[c] = 1;
    }
    for (int t = 0; t < 4 * extra && extra > 0; t++) {
        int c = pr_below(s, k->cells);
        if (can_shade(k, sh, c)) sh[c] = 1, extra--;
    }
    return lines_broken(k, sh);
}

/* the rooms numbered again in the order of their top left cells (their numbers with them) */
static void relabel(hyk_puzzle *p)
{
    int map[HYK_CELLS], next = 0;
    int8_t num[HYK_CELLS];
    for (int r = 0; r < p->rooms; r++) map[r] = -1;
    for (int c = 0; c < p->n * p->n; c++)
        if (map[p->room[c]] < 0) map[p->room[c]] = next++;
    for (int r = 0; r < p->rooms; r++) num[map[r]] = p->num[r];
    for (int c = 0; c < p->n * p->n; c++) p->room[c] = (uint8_t)map[p->room[c]];
    memcpy(p->num, num, sizeof num);
}

/* the room of c cut in two by chance (across or down); 0: a single cell */
static int split(hyk_puzzle *p, int c, uint32_t *s)
{
    int n = p->n, r = p->room[c], x0 = n, y0 = n, x1 = -1, y1 = -1;
    for (int i = 0; i < n * n; i++) {
        if (p->room[i] != r) continue;
        int x = i % n, y = i / n;
        if (x < x0) x0 = x;
        if (y < y0) y0 = y;
        if (x > x1) x1 = x;
        if (y > y1) y1 = y;
    }
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w * h < 2) return 0;
    int down = h < 2 || (w >= 2 && pr_below(s, w + h) < w), at = down ? x0 + 1 + pr_below(s, w - 1)
                                                                          : y0 + 1 + pr_below(s, h - 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (down ? x >= at : y >= at) p->room[y * n + x] = (uint8_t)p->rooms;
    p->num[p->rooms++] = -1;
    relabel(p);
    return 1;
}
static void count_rooms(hyk_puzzle *p, const uint8_t *sh)
{
    for (int r = 0; r < p->rooms; r++) p->num[r] = 0;
    for (int c = 0; c < p->n * p->n; c++) p->num[p->room[c]] += sh[c];
}

#define MAKE_BUDGET 25000L                      /* the solver's passes a puzzle may cost (well under a second) */
#define GRADE_BUDGET 5000L                      /* one grade's passes at most */

/* how far the reasoning gets (cells decided) up to cap: *lv its grade, und the cells left (*nu) */
static int progress(const hyk_puzzle *p, long *work, int cap, int *lv, int *und, int *nu)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = GRADE_BUDGET;
    bench_grid g;
    grid_of(&k, &g);
    *lv = bench_grade(&g, &r, cap);
    int got = 0;
    *nu = 0;
    for (int c = 0; c < k.cells; c++) {
        if (bench_one(g.d[c])) got++;
        else und[(*nu)++] = c;
    }
    return got;
}

typedef struct {
    hyk_puzzle *p;
    const int8_t *full;
    long *work;
} reducer;

static int red_grade(void *u, int cap)
{
    reducer *R = u;
    return grade_work(R->p, NULL, R->work, cap, GRADE_BUDGET);
}

static void red_clue(void *u, int i, int on)
{
    reducer *R = u;
    R->p->num[i] = on ? R->full[i] : -1;
}

/* a puzzle made: rooms and a shading by chance, every room's number (a shaded cell moved where the reasoning gets
 * stuck until it finishes), then numbers taken away while it stays at its level; the nearest level when that one is
 * not found */
void hyk_make_ex(int n, int level, uint32_t seed, long budget, hyk_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > HYK_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static hyk_puzzle best, q;
    static ctx k;
    uint8_t sh[HYK_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 300; attempt++) {
        cut_rooms(&q, n, &s);
        ctx_make(&k, &q);
        if (!shade(&k, sh, &s, pr_below(&s, n))) {
#ifdef HYK_DEBUG
            fprintf(stderr, "shade failed\n");
#endif
            continue;
        }
        count_rooms(&q, sh);
        int und[HYK_CELLS], nu, lv, got = progress(&q, &work, level, &lv, und, &nu);
        for (int step = 0; step < 30 * n && (lv < BENCH_EASY || lv > level); step++) {
            uint8_t t[HYK_CELLS];
            memcpy(t, sh, sizeof t);
            hyk_puzzle m = q;
            int c = nu && pr_below(&s, 100) < 85 ? und[pr_below(&s, nu)] : pr_below(&s, n * n);
            if (pr_below(&s, 2)) {              /* its room cut in two */
                if (!split(&m, c, &s)) continue;
                ctx_make(&k, &m);
                if (!lines_broken(&k, t)) continue;
            } else if (t[c]) {                  /* its shading taken away, or put */
                t[c] = 0;
                if (!lines_broken(&k, t)) continue;
            } else {
                if (!can_shade(&k, t, c)) continue;
                t[c] = 1;
            }
            count_rooms(&m, t);
            int u2[HYK_CELLS], n2, lv2, got2 = progress(&m, &work, level, &lv2, u2, &n2);
            if (got2 >= got || (lv2 >= BENCH_EASY && lv2 <= level)) {
                memcpy(sh, t, sizeof t);
                q = m;
                got = got2;
                lv = lv2;
                nu = n2;
                memcpy(und, u2, sizeof u2);
            }
            ctx_make(&k, &q);
            if (work > 2 * budget) break;
        }
#ifdef HYK_DEBUG
        fprintf(stderr, "climb: lv %d got %d of %d work %ld\n", lv, got, n * n, work);
#endif
        if (lv >= BENCH_EASY && lv <= level) {
            int8_t full[HYK_CELLS];
            memcpy(full, q.num, sizeof full);
            int order[HYK_CELLS];
            for (int r = 0; r < q.rooms; r++) order[r] = r;
            pr_shuffle(&s, order, q.rooms);
            reducer R = {&q, full, &work};
            bench_maker mk = {&R, red_grade, red_clue};
            lv = bench_reduce(&mk, order, q.rooms, level, &work, 3 * budget);
            if (lv >= BENCH_EASY && lv <= level && lv > best_lv) best_lv = lv, best = q;
        }
        if (best_lv == level || (work >= 3 * budget && best_lv >= BENCH_EASY)) break;
    }
    if (best_lv < BENCH_EASY && n > 5) {        /* never seen: a smaller one rather than none */
        hyk_make_ex(n - 1, level, seed + 1, budget, p, sol);
        return;
    }
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0);
}

void hyk_make(int n, int level, uint32_t seed, hyk_puzzle *p, uint8_t *sol)
{
    hyk_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int hyk_load(hyk_game *g, const hyk_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = hyk_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void hyk_new(hyk_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= HYK_VARIANTS) variant = 0;
    hyk_puzzle p;
    uint8_t sol[HYK_CELLS];
    hyk_make(hyk_size(variant), hyk_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = hyk_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int hyk_free(const hyk_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }
int hyk_tap_state(const hyk_game *g, int c) { return g->st[c] == HYK_SHADE ? HYK_EMPTY : HYK_SHADE; }
int hyk_hold_state(const hyk_game *g, int c) { return g->st[c] == HYK_DOT ? HYK_EMPTY : HYK_DOT; }

static int wrongs(const hyk_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != HYK_EMPTY && (g->st[c] == HYK_SHADE) != g->sol[c];
    return k;
}

int hyk_paint(hyk_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (hyk_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(hyk_game *g, int c, int state) { return hyk_paint(g, &c, 1, state); }

static int hint_step(hyk_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (g->st[c] != HYK_EMPTY && (g->st[c] == HYK_SHADE) != g->sol[c])
            return put(g, c, g->sol[c] ? HYK_SHADE : HYK_DOT) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != HYK_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHADE : WHITE);
    int c = bench_next(&d, &r);
    if (c >= 0 && g->st[c] != HYK_EMPTY) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HYK_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HYK_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? HYK_SHADE : HYK_DOT) ? c : -1;
}

int hyk_hint(hyk_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int hyk_undo(hyk_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int hyk_touching(const hyk_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != HYK_SHADE) return 0;
    return (x > 0 && g->st[c - 1] == HYK_SHADE) || (x < n - 1 && g->st[c + 1] == HYK_SHADE) ||
           (y > 0 && g->st[c - n] == HYK_SHADE) || (y < n - 1 && g->st[c + n] == HYK_SHADE);
}

int hyk_apart(const hyk_game *g, int c)
{
    int n = g->p.n, cells = n * n, stack[HYK_CELLS], size[HYK_CELLS], count = 0;
    int16_t of[HYK_CELLS];
    if (g->st[c] == HYK_SHADE) return 0;
    for (int i = 0; i < cells; i++) of[i] = -1;
    for (int i = 0; i < cells; i++) {           /* the pieces of unshaded cells; all but the biggest are apart */
        if (g->st[i] == HYK_SHADE || of[i] >= 0) continue;
        int sp = 0;
        size[count] = 0;
        of[i] = (int16_t)count;
        stack[sp++] = i;
        while (sp) {
            int x = stack[--sp], px = x % n, py = x / n;
            size[count]++;
            const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1,
                               py < n - 1 ? x + n : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && g->st[nb[j]] != HYK_SHADE && of[nb[j]] < 0)
                    of[nb[j]] = (int16_t)count, stack[sp++] = nb[j];
        }
        count++;
    }
    int big = 0;
    for (int i = 1; i < count; i++)
        if (size[i] > size[big]) big = i;
    return count > 1 && of[c] != big;
}

int hyk_long(const hyk_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != HYK_DOT) return 0;
    for (int way = 0; way < 2; way++) {         /* the dots in a row with c, then in a column */
        int step = way ? n : 1, pos = way ? y : x, a = pos, b = pos;
        while (a > 0 && g->st[c - (pos - a + 1) * step] == HYK_DOT) a--;
        while (b < n - 1 && g->st[c + (b - pos + 1) * step] == HYK_DOT) b++;
        int walls = 0;
        for (int i = a; i < b; i++) {
            int u = c + (i - pos) * step;
            walls += g->p.room[u] != g->p.room[u + step];
        }
        if (walls >= 2) return 1;
    }
    return 0;
}

int hyk_room(const hyk_game *g, int r)
{
    int want = g->p.num[r], s = 0;
    if (want < 0) return 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) s += g->p.room[c] == r && g->st[c] == HYK_SHADE;
    return s > want ? 1 : s == want ? 2 : 0;
}

int hyk_solved(const hyk_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < k.cells; c++) d.d[c] = (uint16_t)(g->st[c] == HYK_SHADE ? SHADE : WHITE);
    return valid(&d, &k);
}

int hyk_script(hyk_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (hyk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == HYK_SHADE) != g->sol[c]) put(g, c, g->sol[c] ? HYK_SHADE : HYK_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (a < 0) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? hyk_hold_state(g, a) : hyk_tap_state(g, a), cells[HYK_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!hyk_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
