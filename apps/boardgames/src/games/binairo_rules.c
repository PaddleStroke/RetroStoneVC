/*
 * Binairo: the rules (binairo.h): the ways of reasoning the solver grades with, the maker, the player's grid and undo.
 * No SDL: tested by tests/binairo_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/binairo.h"

#include <string.h>

#include "games/puzrand.h"

/* a cell's values: bit 0 ivory, bit 1 ebony; a line as a mask: bit i set for an ebony disc at its i-th cell */
static const int SIZES[BN_SIZES] = {6, 8, 10, 12};

int bn_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % BN_SIZES]; }
int bn_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* every line a size allows (as many of each colour, never three alike) */
static uint16_t LINES[BN_SIZES][1024];
static int nlines[BN_SIZES];

static int line_ok(unsigned m, int n)
{
    unsigned all = (1u << n) - 1, z = ~m & all;
    return gl_bits(m) == n / 2 && !(m & m >> 1 & m >> 2) && !(z & z >> 1 & z >> 2);
}

static int size_index(int n) { return n <= 6 ? 0 : n <= 8 ? 1 : n <= 10 ? 2 : 3; }

static void lines_make(int n)
{
    int s = size_index(n);
    if (nlines[s]) return;
    for (unsigned m = 0; m < 1u << n; m++)
        if (line_ok(m, n) && nlines[s] < 1024) LINES[s][nlines[s]++] = (uint16_t)m;
}

typedef struct {
    int n;
    const uint16_t *lines;
    int nlines;
} ctx;

static int at(int n, int line, int i) { return line < n ? line * n + i : i * n + line - n; }

/* a line's ebony cells and its decided ones, as masks */
static void masks(const gl_grid *g, int n, int line, unsigned *ebony, unsigned *known)
{
    *ebony = *known = 0;
    for (int i = 0; i < n; i++) {
        uint8_t d = g->d[at(n, line, i)];
        if (!gl_one(d)) continue;
        *known |= 1u << i;
        if (d == 2) *ebony |= 1u << i;
    }
}

/* full lines alike: rows with rows, columns with columns */
static int twins(const gl_grid *g, int n)
{
    unsigned full[2 * BN_MAX];
    int is_full[2 * BN_MAX];
    for (int l = 0; l < 2 * n; l++) {
        unsigned known;
        masks(g, n, l, &full[l], &known);
        is_full[l] = known == (1u << n) - 1;
    }
    for (int a = 0; a < 2 * n; a++)
        for (int b = a + 1; b < (a < n ? n : 2 * n); b++)
            if (is_full[a] && is_full[b] && full[a] == full[b]) return 1;
    return 0;
}

/* easy: three cells side by side, two alike: the third the other colour; a colour's count reached */
static int easy(gl_grid *g, const ctx *k)
{
    int n = k->n, ch = 0;
    for (int l = 0; l < 2 * n; l++) {
        for (int i = 0; i + 2 < n; i++) {
            int c[3] = {at(n, l, i), at(n, l, i + 1), at(n, l, i + 2)};
            for (unsigned bit = 1; bit <= 2; bit++) {
                int same = 0, free_ = -1;
                for (int j = 0; j < 3; j++) {
                    if (g->d[c[j]] == bit) same++;
                    else if (g->d[c[j]] == 3) free_ = c[j];
                }
                if (same == 3) return -1;
                if (same == 2 && free_ >= 0) g->d[free_] = (uint8_t)(3 - bit), ch = 1;
            }
        }
        int cnt[4] = {0, 0, 0, 0};                 /* by the cell's values: 1 ivory, 2 ebony, 3 open */
        for (int i = 0; i < n; i++) cnt[g->d[at(n, l, i)]]++;
        if (cnt[1] > n / 2 || cnt[2] > n / 2) return -1;
        if (!cnt[3] || (cnt[1] != n / 2 && cnt[2] != n / 2)) continue;
        for (int i = 0; i < n; i++)
            if (g->d[at(n, l, i)] == 3) g->d[at(n, l, i)] = cnt[1] == n / 2 ? 2 : 1;
        ch = 1;
    }
    return twins(g, n) ? -1 : ch;
}

/* medium: a line seen whole: the ways it can be filled (unlike the full lines of its kind) all agree on a cell */
static int medium(gl_grid *g, const ctx *k)
{
    int n = k->n, ch = 0;
    unsigned all = (1u << n) - 1, eb[2 * BN_MAX], kn[2 * BN_MAX];
    unsigned full[2][BN_MAX];                   /* the full rows, the full columns */
    int nfull[2] = {0, 0};
    for (int l = 0; l < 2 * n; l++) {
        masks(g, n, l, &eb[l], &kn[l]);
        if (kn[l] == all) full[l >= n][nfull[l >= n]++] = eb[l];
    }
    for (int l = 0; l < 2 * n; l++) {
        if (kn[l] == all) continue;
        unsigned both = all, any = 0;
        int ways = 0, way = l >= n;
        for (int i = 0; i < k->nlines; i++) {
            unsigned m = k->lines[i];
            if ((m & kn[l]) != eb[l]) continue;
            int twin = 0;
            for (int o = 0; o < nfull[way] && !twin; o++) twin = full[way][o] == m;
            if (twin) continue;
            ways++;
            both &= m;
            any |= m;
        }
        if (!ways) return -1;
        for (int i = 0; i < n; i++) {
            int c = at(n, l, i);
            if (g->d[c] != 3) continue;
            if (both >> i & 1) g->d[c] = 2, ch = 1;
            else if (!(any >> i & 1)) g->d[c] = 1, ch = 1;
        }
        if (ch) return 1;
    }
    return 0;
}

static int deduce(gl_grid *g, int tier, const void *c)
{
    return tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
}

static int valid(const gl_grid *g, const void *c)
{
    const ctx *k = c;
    for (int l = 0; l < 2 * k->n; l++) {
        unsigned eb, kn;
        masks(g, k->n, l, &eb, &kn);
        if (!line_ok(eb, k->n)) return 0;
    }
    return !twins(g, k->n);
}

static void setup(ctx *k, int n, gl_rules *r, long *work)
{
    lines_make(n);
    k->n = n;
    k->lines = LINES[size_index(n)];
    k->nlines = nlines[size_index(n)];
    *r = (gl_rules){n * n, 2, 1, deduce, valid, k, work};
}

static void grid_of(const bn_puzzle *p, gl_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < p->n * p->n; c++) g->d[c] = (uint8_t)(p->given[c] ? p->given[c] : 3);
}

static int grade_work(const bn_puzzle *p, uint8_t *sol, long *work, int cap)
{
    ctx k;
    gl_rules r;
    setup(&k, p->n, &r, work);
    gl_grid g, out;
    grid_of(p, &g);
    int lv = gl_grade_upto(&g, &r, &out, cap);
    if (sol && lv >= GL_EASY && lv <= GL_HARD)
        for (int c = 0; c < p->n * p->n; c++) sol[c] = out.d[c];
    return lv;
}

int bn_grade(const bn_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, GL_HARD); }

int bn_count(const bn_puzzle *p, int limit)
{
    ctx k;
    gl_rules r;
    setup(&k, p->n, &r, NULL);
    gl_grid g;
    grid_of(p, &g);
    return gl_search(&g, &r, limit, NULL, NULL, 0);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
#define MAKE_BUDGET 40000L                      /* the solver's passes a puzzle may cost (well under a second) */

typedef struct {
    bn_puzzle *q;
    uint8_t full[BN_CELLS];
    long *work;
} maker;

static int maker_grade(void *u, int cap)
{
    maker *m = u;
    return grade_work(m->q, NULL, m->work, cap);
}

static void maker_clue(void *u, int c, int on)
{
    maker *m = u;
    m->q->given[c] = on ? m->full[c] : BN_EMPTY;
}

void bn_make(int n, int level, uint32_t seed, bn_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > BN_MAX || n % 2) n = 6;
    if (level < GL_EASY || level > GL_HARD) level = GL_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    bn_puzzle best, q;
    uint8_t best_sol[BN_CELLS];
    int best_lv = -2, cells = n * n;
    maker m = {&q, {0}, &work};
    gl_maker mk = {&m, maker_grade, maker_clue};
    for (int attempt = 0; attempt < 400; attempt++) {
        ctx k;
        gl_rules r;
        setup(&k, n, &r, &work);
        gl_grid g, full;                        /* a full grid by chance */
        memset(&g, 0, sizeof g);
        for (int c = 0; c < cells; c++) g.d[c] = 3;
        if (gl_search(&g, &r, 1, &full, &s, work + 4000) != 1) continue;
        q.n = n;
        for (int c = 0; c < cells; c++) q.given[c] = m.full[c] = full.d[c];
        int order[BN_CELLS];
        for (int c = 0; c < cells; c++) order[c] = c;
        pr_shuffle(&s, order, cells);
        int lv = gl_reduce(&mk, order, cells, level, &work, 3 * MAKE_BUDGET);
        if (lv > best_lv && lv <= level) {
            best_lv = lv;
            best = q;
            memcpy(best_sol, m.full, sizeof best_sol);
        }
        if (best_lv == level || (work >= 3 * MAKE_BUDGET && best_lv >= GL_EASY)) break;   /* past it: the nearest */
    }
    *p = best;
    memcpy(sol, best_sol, (size_t)cells);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
void bn_load(bn_game *g, const bn_puzzle *p, int level)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = level;
    bn_grade(p, g->sol);
    for (int c = 0; c < p->n * p->n; c++) g->st[c] = p->given[c];
}

void bn_new(bn_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= BN_VARIANTS) variant = 0;
    bn_puzzle p;
    uint8_t sol[BN_CELLS];
    bn_make(bn_size(variant), bn_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = bn_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
    for (int c = 0; c < p.n * p.n; c++) g->st[c] = p.given[c];
}

int bn_free(const bn_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.given[c]; }
int bn_tap_state(const bn_game *g, int c) { return (g->st[c] + 1) % 3; }
int bn_hold_state(const bn_game *g, int c) { return (g->st[c] + 2) % 3; }

int bn_paint(bn_game *g, const int *cells, int n, int state)
{
    gl_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (bn_free(g, cells[i])) gl_set(&g->hist, g->st, cells[i], state);
    return gl_end(&g->hist);
}

static int put(bn_game *g, int c, int state) { return bn_paint(g, &c, 1, state); }

int bn_hint(bn_game *g)
{
    int n = g->p.n, cells = n * n;
    for (int c = 0; c < cells; c++)             /* a wrong disc first */
        if (g->st[c] && g->st[c] != g->sol[c]) return put(g, c, g->sol[c]) ? c : -1;
    ctx k;
    gl_rules r;
    setup(&k, n, &r, NULL);
    gl_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < cells; c++) d.d[c] = (uint8_t)(g->st[c] ? g->st[c] : 3);
    int c = gl_next(&d, &r);
    for (int i = 0; i < cells && c < 0; i++)
        if (!g->st[i]) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c]) ? c : -1;
}

int bn_undo(bn_game *g) { return gl_undo(&g->hist, g->st); }

static int st_at(const bn_game *g, int line, int i) { return g->st[at(g->p.n, line, i)]; }

int bn_triple(const bn_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n, v = g->st[c];
    if (!v) return 0;
    for (int way = 0; way < 2; way++) {
        int line = way ? n + x : y, pos = way ? y : x;
        for (int s = pos - 2; s <= pos; s++)
            if (s >= 0 && s + 2 < n && st_at(g, line, s) == v && st_at(g, line, s + 1) == v &&
                st_at(g, line, s + 2) == v)
                return 1;
    }
    return 0;
}

int bn_count_line(const bn_game *g, int way, int i, int colour)
{
    int k = 0;
    for (int j = 0; j < g->p.n; j++) k += st_at(g, way * g->p.n + i, j) == colour;
    return k;
}

int bn_line_over(const bn_game *g, int way, int i)
{
    return bn_count_line(g, way, i, BN_IVORY) > g->p.n / 2 || bn_count_line(g, way, i, BN_EBONY) > g->p.n / 2;
}

int bn_line_twin(const bn_game *g, int way, int i)
{
    int n = g->p.n;
    for (int o = 0; o < n; o++) {
        if (o == i) continue;
        int same = 1;
        for (int j = 0; j < n && same; j++) {
            int a = st_at(g, way * n + i, j);
            same = a && a == st_at(g, way * n + o, j);
        }
        if (same) return 1;
    }
    return 0;
}

int bn_solved(const bn_game *g)
{
    int n = g->p.n;
    for (int c = 0; c < n * n; c++)
        if (!g->st[c] || bn_triple(g, c)) return 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++)
            if (bn_line_over(g, way, i) || bn_line_twin(g, way, i)) return 0;
    return 1;
}

int bn_script(bn_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (bn_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (g->st[c] != g->sol[c]) put(g, c, g->sol[c]), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = gl_cell(s, n, n, &len);
        if (!bn_free(g, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = gl_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? bn_hold_state(g, a) : bn_tap_state(g, a), cells[BN_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (bn_free(g, c) && g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!bn_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
