/*
 * Block Puzzle's rules (blocks.h): the pieces, a placement and its clears, the score, the deals (a level's sequence,
 * classic's chance), undo, the goals, the evaluation, the beam search and the tray-only player, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/blocks.h"
#include "games/puzrand.h"

/* ---- the pieces ------------------------------------------------------------------------------------------------- */
/* families: 0 one, 1 two, 2 three in a line, 3 four, 4 five, 5 the small corner, 6 the square of four, 7 the square of
 * nine, 8 the big corner, 9 the L of four, 10 the T, 11 the S and Z, 12 the block of six */
const bk_shape bk_shapes[BK_SHAPES] = {
    {1, 1, 1, 0, {1}},
    {2, 1, 2, 1, {3}},       {1, 2, 2, 1, {1, 1}},
    {3, 1, 3, 2, {7}},       {1, 3, 3, 2, {1, 1, 1}},
    {4, 1, 4, 3, {15}},      {1, 4, 4, 3, {1, 1, 1, 1}},
    {5, 1, 5, 4, {31}},      {1, 5, 5, 4, {1, 1, 1, 1, 1}},
    {2, 2, 3, 5, {1, 3}},    {2, 2, 3, 5, {2, 3}},    {2, 2, 3, 5, {3, 1}},    {2, 2, 3, 5, {3, 2}},
    {2, 2, 4, 6, {3, 3}},
    {3, 3, 9, 7, {7, 7, 7}},
    {3, 3, 5, 8, {1, 1, 7}}, {3, 3, 5, 8, {4, 4, 7}}, {3, 3, 5, 8, {7, 1, 1}}, {3, 3, 5, 8, {7, 4, 4}},
    {2, 3, 4, 9, {1, 1, 3}}, {2, 3, 4, 9, {2, 2, 3}}, {2, 3, 4, 9, {3, 1, 1}}, {2, 3, 4, 9, {3, 2, 2}},
    {3, 2, 4, 9, {7, 1}},    {3, 2, 4, 9, {7, 4}},    {3, 2, 4, 9, {1, 7}},    {3, 2, 4, 9, {4, 7}},
    {3, 2, 4, 10, {7, 2}},   {3, 2, 4, 10, {2, 7}},   {2, 3, 4, 10, {1, 3, 1}}, {2, 3, 4, 10, {2, 3, 2}},
    {3, 2, 4, 11, {6, 3}},   {3, 2, 4, 11, {3, 6}},   {2, 3, 4, 11, {1, 3, 2}}, {2, 3, 4, 11, {2, 3, 1}},
    {2, 3, 6, 12, {3, 3, 3}}, {3, 2, 6, 12, {7, 7}},
};

/* each family's lacquer (hues.h): amber, jade, lapis, turquoise, red, leaf, orange, plum, rose, ivory, red, turquoise,
 * amber */
static const uint8_t FAM_HUE[13] = {3, 2, 4, 9, 1, 11, 8, 7, 10, 5, 1, 9, 3};
int bk_colour(int shape) { return shape >= 0 && shape < BK_SHAPES ? FAM_HUE[bk_shapes[shape].fam] : 1; }

/* classic's chance: each shape's weight (halves: the long and big ones rarer) */
static const uint8_t WEIGHT[BK_SHAPES] = {
    4, 6, 6, 6, 6, 4, 4, 2, 2, 4, 4, 4, 4, 6, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
};

static const char SHAPE_CH[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
char bk_shape_char(int shape) { return shape >= 0 && shape < BK_SHAPES ? SHAPE_CH[shape] : '?'; }
int bk_shape_of(char c)
{
    const char *p = c ? strchr(SHAPE_CH, c) : NULL;
    return p ? (int)(p - SHAPE_CH) : -1;
}

static int pop16(unsigned v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

int bk_popcount(const uint16_t m[BK_N])
{
    int n = 0;
    for (int r = 0; r < BK_N; r++) n += pop16(m[r]);
    return n;
}

/* ---- the rules -------------------------------------------------------------------------------------------------- */
int bk_fits(const uint16_t fill[BK_N], int shape, int x, int y)
{
    if (shape < 0 || shape >= BK_SHAPES) return 0;
    const bk_shape *s = &bk_shapes[shape];
    if (x < 0 || y < 0 || x + s->w > BK_N || y + s->h > BK_N) return 0;
    for (int r = 0; r < s->h; r++)
        if (fill[y + r] & (unsigned)(s->row[r] << x)) return 0;
    return 1;
}

int bk_fits_anywhere(const uint16_t fill[BK_N], int shape)
{
    if (shape < 0 || shape >= BK_SHAPES) return 0;
    const bk_shape *s = &bk_shapes[shape];
    for (int y = 0; y + s->h <= BK_N; y++)
        for (int x = 0; x + s->w <= BK_N; x++)
            if (bk_fits(fill, shape, x, y)) return 1;
    return 0;
}

int bk_hand_empty(const bk_core *k) { return k->hand[0] == BK_NONE && k->hand[1] == BK_NONE && k->hand[2] == BK_NONE; }

int bk_can_move(const bk_core *k)
{
    for (int i = 0; i < 3; i++)
        if (k->hand[i] != BK_NONE && bk_fits_anywhere(k->fill, k->hand[i])) return 1;
    return 0;
}

int bk_lay(bk_core *k, int slot, int x, int y, bk_result *r)
{
    if (slot < 0 || slot > 2 || k->hand[slot] == BK_NONE) return 0;
    int sh = k->hand[slot];
    if (!bk_fits(k->fill, sh, x, y)) return 0;
    const bk_shape *s = &bk_shapes[sh];
    for (int i = 0; i < s->h; i++) {
        unsigned m = (unsigned)(s->row[i] << x);
        k->fill[y + i] |= (uint16_t)m;
        k->want[y + i] &= (uint16_t)~m;
    }
    k->hand[slot] = BK_NONE;
    k->placed++;
    unsigned rows = 0, cols = BK_FULL;
    for (int i = 0; i < BK_N; i++) {
        if (k->fill[i] == BK_FULL) rows |= 1u << i;
        cols &= k->fill[i];
    }
    int lines = pop16(rows) + pop16(cols), golds = 0;
    for (int i = 0; i < BK_N && lines; i++) {
        unsigned clr = rows >> i & 1 ? BK_FULL : cols;
        golds += pop16(k->gold[i] & clr);
        k->fill[i] &= (uint16_t)~clr;
        k->gold[i] &= (uint16_t)~clr;
    }
    int gain = s->n + (lines ? 10 * lines * (lines + 1) / 2 + 10 * lines * k->streak : 0);
    if (r) {
        r->cells = s->n, r->lines = lines, r->gain = gain, r->golds = golds;
        r->rows = (uint16_t)rows, r->cols = (uint16_t)cols;
        r->streak = lines ? k->streak + 1 : 0;
    }
    k->streak = lines ? (uint8_t)(k->streak < 250 ? k->streak + 1 : 250) : 0;
    k->score += gain;
    k->lines = (uint16_t)(k->lines + lines);
    return 1;
}

void bk_deal_seq(bk_core *k, const uint8_t *seq, int nseq)
{
    if (!bk_hand_empty(k)) return;
    for (int i = 0; i < 3; i++) k->hand[i] = k->next < nseq ? seq[k->next++] : BK_NONE;
}

int bk_goal_left(const bk_core *k, int goal, int target)
{
    switch (goal) {
    case BK_LINES: return k->lines >= target ? 0 : target - k->lines;
    case BK_SCORE: return k->score >= target ? 0 : target - k->score;
    case BK_GOLDS: return bk_popcount(k->gold);
    case BK_PATTERN: return bk_popcount(k->want);
    default: return 1;                          /* classic: no goal */
    }
}

/* ---- a game ----------------------------------------------------------------------------------------------------- */
static int draw_shape(uint32_t *rng)
{
    int total = 0;
    for (int i = 0; i < BK_SHAPES; i++) total += WEIGHT[i];
    int r = pr_below(rng, total);
    for (int i = 0; i < BK_SHAPES; i++)
        if ((r -= WEIGHT[i]) < 0) return i;
    return 0;
}

static void deal_random(bk_pos *p)
{
    for (int tries = 0; tries < 3; tries++) {   /* a deal where none fits is drawn again, twice at most */
        for (int i = 0; i < 3; i++) p->k.hand[i] = (uint8_t)draw_shape(&p->rng);
        if (bk_can_move(&p->k)) return;
    }
}

void bk_new_classic(bk_game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->goal = BK_CLASSIC;
    g->p.rng = pr_seed(seed);
    deal_random(&g->p);
}

static int hexcol(char c)                       /* a grid's colour: '1'..'9', 'a'..'c' (10..12) */
{
    return c >= '1' && c <= '9' ? c - '0' : c >= 'a' && c <= 'c' ? c - 'a' + 10 : 0;
}

/* a placement in the notation at s ("ae7"): its tray place, column and row; the characters read (0: not one) */
static int parse_move(const char *s, int *slot, int *x, int *y)
{
    if (s[0] < 'a' || s[0] > 'c' || s[1] < 'a' || s[1] > 'j' || s[2] < '0' || s[2] > '9') return 0;
    int n = 2, row = 0;
    while (s[n] >= '0' && s[n] <= '9' && n < 4) row = row * 10 + (s[n++] - '0');
    if (row < 1 || row > BK_N) return 0;
    *slot = s[0] - 'a';
    *x = s[1] - 'a';
    *y = row - 1;
    return n;
}

static int set_grid(bk_game *g, const char *grid)
{
    bk_core *k = &g->p.k;
    memset(k->fill, 0, sizeof k->fill);
    memset(k->gold, 0, sizeof k->gold);
    memset(k->want, 0, sizeof k->want);
    memset(g->p.col, 0, sizeof g->p.col);
    for (int i = 0; i < BK_CELLS; i++) {
        char c = grid[i];
        int x = i % BK_N, y = i / BK_N;
        if (!c) return 0;
        if (c == '.') continue;
        if (c == 'o') { k->want[y] |= (uint16_t)(1u << x); continue; }
        if (c == 'G') {
            k->fill[y] |= (uint16_t)(1u << x);
            k->gold[y] |= (uint16_t)(1u << x);
            g->p.col[i] = BK_GOLD;
            continue;
        }
        int h = hexcol(c);
        if (!h) return 0;
        k->fill[y] |= (uint16_t)(1u << x);
        g->p.col[i] = (uint8_t)h;
    }
    memcpy(g->pat, k->want, sizeof g->pat);
    g->golds = bk_popcount(k->gold);
    return 1;
}

int bk_load(bk_game *g, const bk_level_def *d)
{
    memset(g, 0, sizeof *g);
    if (!d->grid || strlen(d->grid) != BK_CELLS || !set_grid(g, d->grid)) return 0;
    g->goal = d->goal;
    g->target = d->target;
    g->par = d->par;
    int n = (int)strlen(d->seq);
    if (n < 1 || n > BK_SEQ) return 0;
    for (int i = 0; i < n; i++) {
        int s = bk_shape_of(d->seq[i]);
        if (s < 0) return 0;
        g->seq[i] = (uint8_t)s;
    }
    g->nseq = n;
    g->pieces = n;
    g->p.k.hand[0] = g->p.k.hand[1] = g->p.k.hand[2] = BK_NONE;
    const char *w = d->way ? d->way : "";
    while (*w && g->nway < BK_SEQ) {
        int slot, x, y, m = parse_move(w, &slot, &x, &y);
        if (!m) return 0;
        g->way[g->nway][0] = (uint8_t)slot;
        g->way[g->nway][1] = (uint8_t)(y * BK_N + x);
        g->nway++;
        w += m;
    }
    bk_deal_seq(&g->p.k, g->seq, g->nseq);
    return 1;
}

int bk_level(bk_game *g, int level)
{
#ifdef BK_GEN
    (void)g;
    (void)level;
    return 0;
#else
    if (level < 0 || level >= BK_LEVELS) return 0;
    return bk_load(g, &bk_levels[level]);
#endif
}

int bk_solved(const bk_game *g) { return g->goal != BK_CLASSIC && bk_goal_left(&g->p.k, g->goal, g->target) == 0; }
int bk_over(const bk_game *g) { return bk_solved(g) || !bk_can_move(&g->p.k); }

int bk_left(const bk_game *g)
{
    if (g->goal == BK_CLASSIC) return 0;
    int n = g->nseq - g->p.k.next;
    for (int i = 0; i < 3; i++) n += g->p.k.hand[i] != BK_NONE;
    return n;
}

int bk_stars(const bk_game *g)
{
    if (g->goal == BK_CLASSIC) return g->p.k.score >= BK_STAR3 ? 3 : g->p.k.score >= BK_STAR2 ? 2 : 1;
    if (!bk_solved(g)) return 0;
    int used = g->p.k.placed, par = g->par;
    return par <= 0 || used <= par ? 3 : 2 * used <= 3 * par ? 2 : 1;   /* as ui_par_stars */
}

int bk_place(bk_game *g, int slot, int x, int y)
{
    if (bk_over(g)) return 0;
    if (slot < 0 || slot > 2 || !bk_fits(g->p.k.fill, g->p.k.hand[slot], x, y)) return 0;
    if (g->nh == BK_HIST) {                     /* undo's memory full: the oldest half forgotten */
        memmove(g->hist, g->hist + BK_HIST / 2, sizeof g->hist[0] * (BK_HIST / 2));
        g->nh = BK_HIST / 2;
    }
    g->hist[g->nh].p = g->p;
    g->hist[g->nh].mv[0] = (uint8_t)slot;
    g->hist[g->nh].mv[1] = (uint8_t)(y * BK_N + x);
    g->nh++;
    int sh = g->p.k.hand[slot];
    bk_lay(&g->p.k, slot, x, y, &g->last);
    const bk_shape *s = &bk_shapes[sh];
    for (int r = 0; r < s->h; r++)
        for (int c = 0; c < s->w; c++)
            if (s->row[r] >> c & 1) g->p.col[(y + r) * BK_N + x + c] = (uint8_t)bk_colour(sh);
    for (int i = 0; i < BK_CELLS; i++)
        if (!(g->p.k.fill[i / BK_N] >> (i % BK_N) & 1)) g->p.col[i] = 0;
    if (g->goal != BK_CLASSIC && g->p.k.placed <= BK_SEQ) {
        g->mv[g->p.k.placed - 1][0] = (uint8_t)slot;
        g->mv[g->p.k.placed - 1][1] = (uint8_t)(y * BK_N + x);
    }
    if (bk_hand_empty(&g->p.k) && !bk_solved(g)) {
        if (g->goal == BK_CLASSIC) deal_random(&g->p);
        else bk_deal_seq(&g->p.k, g->seq, g->nseq);
    }
    return 1;
}

int bk_undo(bk_game *g)
{
    if (g->nh <= 0) return 0;
    g->p = g->hist[--g->nh].p;
    memset(&g->last, 0, sizeof g->last);
    return 1;
}

int bk_on_way(const bk_game *g)
{
    int n = g->p.k.placed;
    if (g->goal == BK_CLASSIC || n > g->nway) return 0;
    for (int i = 0; i < n; i++)
        if (g->mv[i][0] != g->way[i][0] || g->mv[i][1] != g->way[i][1]) return 0;
    return 1;
}

int bk_hint(bk_game *g, int *slot, int *x, int *y)
{
    if (bk_over(g)) return 0;
    const bk_core *k = &g->p.k;
    if (g->goal != BK_CLASSIC) {
        int n = k->placed;
        if (!bk_on_way(g) || n >= g->nway) {    /* off the way: look for another from here, and keep it */
            uint8_t w[BK_SEQ][2];
            int m = bk_search(k, g->seq, g->nseq, g->goal, g->target, 10, w);
            if (m > 0 && n + m <= BK_SEQ) {
                memcpy(g->way, g->mv, sizeof g->way[0] * (size_t)n);
                memcpy(g->way + n, w, sizeof w[0] * (size_t)m);
                g->nway = n + m;
            }
        }
        if (bk_on_way(g) && n < g->nway) {
            int s = g->way[n][0], c = g->way[n][1];
            if (bk_fits(k->fill, k->hand[s], c % BK_N, c / BK_N)) {
                *slot = s, *x = c % BK_N, *y = c / BK_N;
                return 1;
            }
        }
    }
    return bk_greedy(k, g->goal, g->target, 0, NULL, slot, x, y);
}

int bk_script(bk_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (*s == '=') {
            s++;
            int k = 0, all = !(*s >= '0' && *s <= '9');
            while (*s >= '0' && *s <= '9') k = k * 10 + (*s++ - '0');
            for (int i = 0; (all ? i < 100000 : i < k) && !bk_over(g); i++) {
                int sl, x, y;
                if (!bk_hint(g, &sl, &x, &y) || !bk_place(g, sl, x, y)) return 0;
            }
            continue;
        }
        if (*s == '#') {                        /* a grid of the levels' kind, the tray and the score kept */
            s++;
            char grid[BK_CELLS + 1];
            int n = 0;
            while (n < BK_CELLS && s[n] && s[n] != ' ') grid[n] = s[n], n++;
            grid[n] = 0;
            if (n != BK_CELLS || !set_grid(g, grid)) return 0;
            g->nh = 0;
            s += n;
            continue;
        }
        int slot, x, y, n = parse_move(s, &slot, &x, &y);
        if (!n || !bk_place(g, slot, x, y)) return 0;
        s += n;
    }
    return 1;
}

/* ---- the evaluation --------------------------------------------------------------------------------------------- */
int bk_eval(const bk_core *k, int goal, int target)
{
    int rc[BK_N] = {0}, cc[BK_N] = {0}, trans = 0, holes = 0, empty = 0;
    for (int y = 0; y < BK_N; y++) {
        unsigned r = (unsigned)k->fill[y], w = (r << 1) | 0x801u;   /* the walls as blocks */
        trans += pop16((w ^ (w >> 1)) & 0x7ffu);
        rc[y] = pop16(r);
        empty += BK_N - rc[y];
        for (int x = 0; x < BK_N; x++) {
            if (r >> x & 1) { cc[x]++; continue; }
            int up = y == 0 || (k->fill[y - 1] >> x & 1), dn = y == BK_N - 1 || (k->fill[y + 1] >> x & 1);
            int lf = x == 0 || (r >> (x - 1) & 1), rt = x == BK_N - 1 || (r >> (x + 1) & 1);
            if (up && dn && lf && rt) holes++;
        }
    }
    for (int x = 0; x < BK_N; x++) {
        int prev = 1;
        for (int y = 0; y < BK_N; y++) {
            int b = k->fill[y] >> x & 1;
            trans += b != prev;
            prev = b;
        }
        trans += prev != 1;
    }
    int v = -8 * trans - 40 * holes + 2 * empty;
    v += bk_fits_anywhere(k->fill, 14) ? 70 : -90;   /* room for the square of nine, the lines of five */
    v += bk_fits_anywhere(k->fill, 7) ? 25 : -40;
    v += bk_fits_anywhere(k->fill, 8) ? 25 : -40;
    int near = 0;
    for (int i = 0; i < BK_N; i++) near += rc[i] * rc[i] + cc[i] * cc[i];
    switch (goal) {
    case BK_LINES:
        v += 3000 * (k->lines < target ? k->lines : target) + 3 * near;
        break;
    case BK_SCORE:
        v += 40 * (k->score < target ? k->score : target) + 2 * near;
        break;
    case BK_GOLDS: {
        int g = 0;
        for (int i = 0; i < BK_N; i++) {
            if (k->gold[i]) g += 6 * rc[i] * rc[i];
            for (int x = 0; x < BK_N; x++)
                if (k->gold[i] >> x & 1) g += 6 * cc[x] * cc[x];
        }
        v += -3000 * bk_popcount(k->gold) + g + near;
        break;
    }
    case BK_PATTERN:
        v += -3000 * bk_popcount(k->want) + near;
        break;
    default:
        v += 12 * k->score + near;
        break;
    }
    return v;
}

/* ---- the search ------------------------------------------------------------------------------------------------- */
typedef struct {
    bk_core k;
    int parent, v;
    uint8_t slot, cell;
} node;

static uint32_t core_hash(const bk_core *k)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)k->fill;
    for (size_t i = 0; i < sizeof k->fill * 3; i++) h = (h ^ p[i]) * 16777619u;   /* fill, gold, want */
    uint8_t a = k->hand[0], b = k->hand[1], c = k->hand[2], t;   /* the tray as a set: its order does not matter */
    if (a > b) t = a, a = b, b = t;
    if (b > c) t = b, b = c, c = t;
    if (a > b) t = a, a = b, b = t;
    uint32_t more[5] = {a, b, c, k->next, (uint32_t)k->score * 31u + k->streak};
    for (int i = 0; i < 5; i++) h = (h ^ more[i]) * 16777619u;
    return h ? h : 1;
}

static int by_value(const void *a, const void *b)
{
    const node *x = a, *y = b;
    return x->v != y->v ? (x->v < y->v ? 1 : -1) : (x->parent > y->parent) - (x->parent < y->parent);
}

int bk_search(const bk_core *k0, const uint8_t *seq, int nseq, int goal, int target, int width, uint8_t way[][2])
{
    if (bk_goal_left(k0, goal, target) == 0) return 0;
    int depth_max = nseq - k0->next;
    for (int i = 0; i < 3; i++) depth_max += k0->hand[i] != BK_NONE;
    if (depth_max <= 0 || width < 1) return -1;
    if (depth_max > BK_SEQ) depth_max = BK_SEQ;
    int cap = width * 3 * BK_CELLS, hsize = 1;
    while (hsize < 2 * cap) hsize <<= 1;
    node **layer = calloc((size_t)depth_max + 1, sizeof *layer);
    int *nl = calloc((size_t)depth_max + 1, sizeof *nl);
    node *kids = malloc(sizeof *kids * (size_t)cap);
    uint32_t *seen = malloc(sizeof *seen * (size_t)hsize);
    int found = -1;
    if (!layer || !nl || !kids || !seen) goto done;
    layer[0] = malloc(sizeof(node));
    if (!layer[0]) goto done;
    layer[0][0].k = *k0;
    layer[0][0].parent = -1;
    nl[0] = 1;
    for (int d = 0; d < depth_max && found < 0; d++) {
        int nk = 0;
        memset(seen, 0, sizeof *seen * (size_t)hsize);
        for (int i = 0; i < nl[d] && found < 0; i++) {
            const bk_core *k = &layer[d][i].k;
            for (int s = 0; s < 3 && found < 0; s++) {
                int sh = k->hand[s];
                if (sh == BK_NONE || (s > 0 && k->hand[0] == sh) || (s > 1 && k->hand[1] == sh)) continue;
                const bk_shape *S = &bk_shapes[sh];
                for (int y = 0; y + S->h <= BK_N && found < 0; y++)
                    for (int x = 0; x + S->w <= BK_N; x++) {
                        if (!bk_fits(k->fill, sh, x, y)) continue;
                        node c;
                        c.k = *k;
                        bk_lay(&c.k, s, x, y, NULL);
                        c.parent = i;
                        c.slot = (uint8_t)s;
                        c.cell = (uint8_t)(y * BK_N + x);
                        if (bk_goal_left(&c.k, goal, target) == 0) {   /* reached: the fewest placements */
                            kids[0] = c;
                            nk = 1;
                            found = d + 1;
                            break;
                        }
                        bk_deal_seq(&c.k, seq, nseq);
                        if (!bk_can_move(&c.k)) continue;
                        uint32_t h = core_hash(&c.k);
                        unsigned at = h & (unsigned)(hsize - 1);
                        int dup = 0;
                        while (seen[at]) {
                            if (seen[at] == h) { dup = 1; break; }
                            at = (at + 1) & (unsigned)(hsize - 1);
                        }
                        if (dup || nk >= cap) continue;
                        seen[at] = h;
                        c.v = bk_eval(&c.k, goal, target);
                        kids[nk++] = c;
                    }
            }
        }
        if (!nk) break;
        if (found < 0) qsort(kids, (size_t)nk, sizeof *kids, by_value);
        int keep = nk < width ? nk : width;
        layer[d + 1] = malloc(sizeof(node) * (size_t)keep);
        if (!layer[d + 1]) break;
        memcpy(layer[d + 1], kids, sizeof(node) * (size_t)keep);
        nl[d + 1] = keep;
    }
    if (found > 0) {                            /* the way back from the goal */
        int i = 0;
        for (int d = found; d > 0; d--) {
            way[d - 1][0] = layer[d][i].slot;
            way[d - 1][1] = layer[d][i].cell;
            i = layer[d][i].parent;
        }
    }
done:
    if (layer)
        for (int d = 0; d <= depth_max; d++) free(layer[d]);
    free(layer);
    free(nl);
    free(kids);
    free(seen);
    return found;
}

int bk_greedy(const bk_core *k, int goal, int target, int noise, uint32_t *rng, int *slot, int *x, int *y)
{
    int best = 0, bv = 0;
    for (int s = 0; s < 3; s++) {
        int sh = k->hand[s];
        if (sh == BK_NONE || (s > 0 && k->hand[0] == sh) || (s > 1 && k->hand[1] == sh)) continue;
        const bk_shape *S = &bk_shapes[sh];
        for (int yy = 0; yy + S->h <= BK_N; yy++)
            for (int xx = 0; xx + S->w <= BK_N; xx++) {
                if (!bk_fits(k->fill, sh, xx, yy)) continue;
                bk_core c = *k;
                bk_lay(&c, s, xx, yy, NULL);
                int v;
                if (goal != BK_CLASSIC && bk_goal_left(&c, goal, target) == 0) v = 1 << 28;
                else {
                    v = bk_eval(&c, goal, target);
                    for (int o = 0; o < 3; o++)   /* the tray's other pieces must still fit */
                        if (c.hand[o] != BK_NONE && !bk_fits_anywhere(c.fill, c.hand[o])) v -= 100000;
                    if (noise > 0 && rng) v += pr_below(rng, noise);
                }
                if (!best || v > bv) best = 1, bv = v, *slot = s, *x = xx, *y = yy;
            }
    }
    return best;
}

#ifndef BK_GEN                                  /* not in the levels' tool (tools/blocks_gen.c): it makes them */
#include "games/blocks_levels.h"
#endif
