/*
 * Paper Fold: the rules (paperfold.h): sheets and pictures as text, folds, undo, the breadth-first search for the
 * fewest folds (the par, the hint), the puzzles' maker, the levels. No SDL: tested by tests/paperfold_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/paperfold.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

/* ---- sheets and pictures as text --------------------------------------------------------------------------------- */
static int colour_of(char ch) { return ch >= 'a' && ch < 'a' + PFO_COLOURS ? ch - 'a' : -1; }

/* rows split by '|', each row `per` characters a square: its width and height; 0 a bad shape */
static int shape_of(const char *s, int per, int *w, int *h)
{
    int x = 0, y = 0;
    *w = 0;
    for (;; s++) {
        if (*s == '|' || !*s) {
            if (x) {
                if (x % per) return 0;
                if (!*w) *w = x / per;
                if (x / per != *w) return 0;
                y++;
            }
            x = 0;
            if (!*s) break;
            continue;
        }
        x++;
    }
    *h = y;
    return *w > 0 && *w <= PFO_MAXW && y > 0 && y <= PFO_MAXH;
}

int pfo_parse(pfo_map *m, const char *sheet, const char *picture)
{
    memset(m, 0, sizeof *m);
    int w, h, pw, ph;
    if (!shape_of(sheet, 2, &w, &h) || !shape_of(picture, 1, &pw, &ph) || pw != w || ph != h) return 0;
    m->w = w;
    m->h = h;
    memset(m->start.cell, PFO_NONE, sizeof m->start.cell);
    memset(m->target, PFO_NONE, sizeof m->target);
    int c = 0, paper = 0;
    for (const char *s = sheet; *s; s++) {
        if (*s == '|') continue;
        char a = s[0], b = s[1];
        if (!b) return 0;
        s++;
        if (a == '.' && b == '.') { c++; continue; }
        int f = colour_of(a), k = colour_of(b);
        if (f < 0 || k < 0) return 0;
        m->start.cell[c] = (uint8_t)(f * 16 + k);
        m->start.layers[c] = 1;
        paper++;
        c++;
    }
    c = 0;
    for (const char *s = picture; *s; s++) {
        if (*s == '|') continue;
        if (*s == '.') { c++; continue; }
        int f = colour_of(*s);
        if (f < 0) return 0;
        m->target[c++] = (uint8_t)f;
    }
    return paper > 0;
}

void pfo_text(const pfo_map *m, char *sheet, char *picture)
{
    int n = 0, k = 0;
    for (int y = 0; y < m->h; y++) {
        if (y) { sheet[n++] = '|'; picture[k++] = '|'; }
        for (int x = 0; x < m->w; x++) {
            int c = x + y * m->w;
            uint8_t v = m->start.cell[c];
            sheet[n++] = v == PFO_NONE ? '.' : (char)('a' + pfo_up(v));
            sheet[n++] = v == PFO_NONE ? '.' : (char)('a' + pfo_down(v));
            picture[k++] = m->target[c] == PFO_NONE ? '.' : (char)('a' + m->target[c]);
        }
    }
    sheet[n] = picture[k] = 0;
}

/* ---- folds ------------------------------------------------------------------------------------------------------- */
void pfo_begin(pfo_game *g)
{
    g->s = g->m.start;
    g->nh = g->moves = 0;
}

int pfo_image(const pfo_map *m, int fold, int c)
{
    int x = c % m->w, y = c / m->w, k = PFO_LINE(fold);
    if (PFO_AXIS(fold) == 0) x = 2 * k - 1 - x;
    else y = 2 * k - 1 - y;
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return -1;
    return x + y * m->w;
}

static int in_flap(const pfo_map *m, int fold, int c)
{
    int p = PFO_AXIS(fold) == 0 ? c % m->w : c / m->w, k = PFO_LINE(fold);
    return PFO_SIDE(fold) == 0 ? p < k : p >= k;
}

void pfo_flap(const pfo_map *m, const pfo_state *s, int fold, uint64_t *mask)
{
    *mask = 0;
    for (int c = 0; c < m->w * m->h; c++)
        if (s->cell[c] != PFO_NONE && in_flap(m, fold, c)) *mask |= 1ull << c;
}

int pfo_try(const pfo_map *m, const pfo_state *s, int fold, pfo_state *out)
{
    if (fold < 0 || fold >= PFO_MOVES) return 0;
    int axis = PFO_AXIS(fold), k = PFO_LINE(fold), n = axis == 0 ? m->w : m->h;
    if (axis > 1 || k < 1 || k >= n) return 0;
    int flap = 0, base = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (s->cell[c] == PFO_NONE) continue;
        if (in_flap(m, fold, c)) {
            flap++;
            if (pfo_image(m, fold, c) < 0) return 0;   /* it would land off the board */
        } else base++;
    }
    if (!flap || !base) return 0;
    pfo_state ns = *s;
    for (int c = 0; c < m->w * m->h; c++)
        if (s->cell[c] != PFO_NONE && in_flap(m, fold, c)) { ns.cell[c] = PFO_NONE; ns.layers[c] = 0; }
    for (int c = 0; c < m->w * m->h; c++) {
        if (s->cell[c] == PFO_NONE || !in_flap(m, fold, c)) continue;
        int t = pfo_image(m, fold, c);
        uint8_t f = s->cell[c], b = s->cell[t];
        /* turned over, laid on top: its underneath shows on top; the pile's underneath stays the base's */
        ns.cell[t] = (uint8_t)(pfo_down(f) * 16 + (b == PFO_NONE ? pfo_up(f) : pfo_down(b)));
        ns.layers[t] = (uint8_t)(s->layers[c] + (b == PFO_NONE ? 0 : s->layers[t]));
    }
    if (out) *out = ns;
    return 1;
}

int pfo_solved(const pfo_map *m, const pfo_state *s)
{
    for (int c = 0; c < m->w * m->h; c++) {
        uint8_t v = s->cell[c];
        if ((v == PFO_NONE) != (m->target[c] == PFO_NONE)) return 0;
        if (v != PFO_NONE && pfo_up(v) != m->target[c]) return 0;
    }
    return 1;
}

int pfo_move(pfo_game *g, int fold)
{
    pfo_state n;
    if (pfo_solved(&g->m, &g->s) || !pfo_try(&g->m, &g->s, fold, &n)) return 0;
    if (g->nh >= PFO_HIST) return 0;            /* (never: each fold narrows the paper) */
    g->hist[g->nh++] = g->s;
    g->s = n;
    g->moves++;
    return 1;
}

int pfo_undo(pfo_game *g)
{
    if (!g->nh) return 0;
    g->s = g->hist[--g->nh];
    g->moves--;
    return 1;
}

/* ---- the search: the states (their cells only) in a table found again by a hash ------------------------------- */
#define CAP (1 << 17)
typedef struct { uint8_t cell[PFO_CELLS]; } skey;

static uint32_t hash_of(const uint8_t *c, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ c[i]) * 16777619u;
    return h ^ (h >> 15);
}

typedef struct {
    skey *q;
    int32_t *tab;
    int16_t *depth;
    int8_t *first;
    uint32_t *count;
    int n, size;
} search_t;

static int search_open(search_t *S)
{
    S->size = 2 * CAP;
    S->q = malloc((size_t)CAP * sizeof *S->q);
    S->tab = malloc((size_t)S->size * sizeof *S->tab);
    S->depth = malloc((size_t)CAP * sizeof *S->depth);
    S->first = malloc((size_t)CAP);
    S->count = malloc((size_t)CAP * sizeof *S->count);
    S->n = 0;
    if (!S->q || !S->tab || !S->depth || !S->first || !S->count) return 0;
    memset(S->tab, -1, (size_t)S->size * sizeof *S->tab);
    return 1;
}

static void search_close(search_t *S)
{
    free(S->q);
    free(S->tab);
    free(S->depth);
    free(S->first);
    free(S->count);
}

/* the state's index, added when new (*fresh 1); -1 the table is full */
static int search_find(search_t *S, const uint8_t *cell, int nc, int *fresh)
{
    uint32_t h = hash_of(cell, nc) & (uint32_t)(S->size - 1);
    while (S->tab[h] >= 0) {
        if (!memcmp(S->q[S->tab[h]].cell, cell, (size_t)nc)) { *fresh = 0; return S->tab[h]; }
        h = (h + 1) & (uint32_t)(S->size - 1);
    }
    if (S->n >= CAP) return -1;
    *fresh = 1;
    memcpy(S->q[S->n].cell, cell, (size_t)nc);
    S->tab[h] = S->n;
    return S->n++;
}

/* a breadth-first search from s: the fewest folds to a solved state, the first fold, the shortest ways' count */
static int search(const pfo_map *m, const pfo_state *s, int *first, int *states, int *ways, int cap)
{
    search_t S;
    int nc = m->w * m->h, best = -1;
    uint32_t total = 0;
    if (states) *states = 0;
    if (pfo_solved(m, s)) { if (ways) *ways = 1; return 0; }
    if (!search_open(&S)) { search_close(&S); return -1; }
    int fresh, i0 = search_find(&S, s->cell, nc, &fresh);
    S.depth[i0] = 0;
    S.first[i0] = -1;
    S.count[i0] = 1;
    pfo_state a, b;
    memset(&a, 0, sizeof a);
    for (int head = 0; head < S.n; head++) {
        if (best >= 0 && S.depth[head] >= best) break;
        memcpy(a.cell, S.q[head].cell, (size_t)nc);
        for (int f = 0; f < PFO_MOVES; f++) {
            if (!pfo_try(m, &a, f, &b)) continue;
            int j = search_find(&S, b.cell, nc, &fresh);
            if (j < 0) { best = -1; goto done; }
            if (fresh) {
                S.depth[j] = (int16_t)(S.depth[head] + 1);
                S.first[j] = (int8_t)(head == 0 ? f : S.first[head]);
                S.count[j] = 0;
            }
            if (S.depth[j] != S.depth[head] + 1) continue;
            S.count[j] = S.count[j] + S.count[head] > (uint32_t)cap ? (uint32_t)cap : S.count[j] + S.count[head];
            if (fresh && pfo_solved(m, &b)) {
                if (best < 0) {
                    best = S.depth[j];
                    if (first) *first = S.first[j];
                }
            }
        }
    }
    if (best >= 0)                              /* the ways into every solved state at that depth */
        for (int i = 0; i < S.n; i++) {
            if (S.depth[i] != best) continue;
            memcpy(a.cell, S.q[i].cell, (size_t)nc);
            if (pfo_solved(m, &a)) total += S.count[i];
        }
done:
    if (states) *states = S.n;
    if (ways) *ways = total > (uint32_t)cap ? cap : (int)total;
    search_close(&S);
    return best;
}

int pfo_solve(const pfo_map *m, const pfo_state *s, int *first, int *states)
{
    return search(m, s, first, states, NULL, 1);
}

int pfo_ways(const pfo_map *m, int cap)
{
    int w = 0;
    search(m, &m->start, NULL, NULL, &w, cap);
    return w;
}

int pfo_script(pfo_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'h' || *s == '=') {
            int all = *s == '=';
            do {
                int f;
                if (pfo_solve(&g->m, &g->s, &f, NULL) <= 0) break;
                pfo_move(g, f);
            } while (all && !pfo_solved(&g->m, &g->s));
            continue;
        }
        const char *k = strchr("LRTB", *s);
        if (!k || s[1] < '1' || s[1] > '9') return 0;
        int i = (int)(k - "LRTB"), line = s[1] - '0';
        s++;
        if (!pfo_move(g, PFO_FOLD(i / 2, line, i % 2))) return 0;
    }
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* a pattern's colour index (0, 1, 2) at (x, y) of a w x h sheet: symmetric designs */
static int pattern(int kind, int x, int y, int w, int h)
{
    int mx = x < w - 1 - x ? x : w - 1 - x, my = y < h - 1 - y ? y : h - 1 - y, edge = mx < my ? mx : my;
    switch (kind) {
    case 1: return x & 1;                                       /* stripes down */
    case 2: return y & 1;                                       /* stripes across */
    case 3: return edge == 0;                                   /* a frame */
    case 4: return (x + y) & 1;                                 /* a chequer */
    case 5: return 2 * x >= w;                                  /* halves, left and right */
    case 6: return 2 * y >= h;                                  /* halves, top and bottom */
    case 7: return (2 * x >= w) != (2 * y >= h);                /* quarters */
    case 8: return 2 * mx + 1 >= w - 1 || 2 * my + 1 >= h - 1;  /* a cross through the middle */
    case 9: return edge & 1 ? 1 : 0;                            /* rings */
    case 10: return 3 * x / w == 1 ? 1 : 0;                     /* a band down the middle */
    case 11: return (mx + my) % 3 == 0 ? 1 : (mx + my) % 3 == 1 ? 2 : 0;   /* a diamond of three colours */
    default: return 0;                                          /* plain */
    }
}

#define PATTERNS 12

static int connected(const pfo_state *s, int w, int h)
{
    int stack[PFO_CELLS], seen[PFO_CELLS] = {0}, n = 0, cnt = 0, total = 0, s0 = -1;
    for (int c = 0; c < w * h; c++)
        if (s->cell[c] != PFO_NONE) { total++; if (s0 < 0) s0 = c; }
    if (s0 < 0) return 0;
    stack[n++] = s0;
    seen[s0] = 1;
    while (n) {
        int c = stack[--n], x = c % w, y = c / w;
        cnt++;
        const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        for (int i = 0; i < 4; i++) {
            int nx = nb[i][0], ny = nb[i][1], d = nx + ny * w;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || seen[d] || s->cell[d] == PFO_NONE) continue;
            seen[d] = 1;
            stack[n++] = d;
        }
    }
    return cnt == total;
}

static int spans(const pfo_state *s, int w, int h)   /* every edge row and column keeps some paper */
{
    int l = 0, r = 0, t = 0, b = 0;
    for (int i = 0; i < h; i++) { l |= s->cell[i * w] != PFO_NONE; r |= s->cell[w - 1 + i * w] != PFO_NONE; }
    for (int i = 0; i < w; i++) { t |= s->cell[i] != PFO_NONE; b |= s->cell[i + (h - 1) * w] != PFO_NONE; }
    return l && r && t && b;
}

static void sheet(pfo_map *m, const pfo_spec *sp, uint32_t *r)
{
    int w = sp->w, h = sp->h, cols[6] = {0, 1, 2, 3, 4, 5};
    m->w = w;
    m->h = h;
    memset(&m->start, 0, sizeof m->start);
    memset(m->start.cell, PFO_NONE, sizeof m->start.cell);
    memset(m->target, PFO_NONE, sizeof m->target);
    int nc = sp->colours < 2 ? 2 : sp->colours > PFO_COLOURS ? PFO_COLOURS : sp->colours;
    pr_shuffle(r, cols, PFO_COLOURS);
    int fr[3], bk[3];                           /* the front's colours, the back's */
    for (int i = 0; i < 3; i++) { fr[i] = cols[i % nc]; bk[i] = cols[(i + 1 + pr_below(r, nc - 1)) % nc]; }
    if (sp->front == 0) fr[0] = cols[0];
    if (sp->back == 0 && bk[0] == fr[0]) bk[0] = cols[1];
    int pf = sp->front < 0 ? pr_below(r, PATTERNS) : sp->front, pb = sp->back < 0 ? pr_below(r, PATTERNS) : sp->back;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = x + y * w;
            m->start.cell[c] = (uint8_t)(fr[pattern(pf, x, y, w, h)] * 16 + bk[pattern(pb, x, y, w, h)]);
            m->start.layers[c] = 1;
        }
    for (int k = 0, cut = 0; k < 40 && cut < sp->cuts; k++) {   /* cut-outs in mirrored sets */
        int c = pr_below(r, w * h), x = c % w, y = c / w, sym = pr_below(r, 3), o[4], n = 0;
        o[n++] = c;
        if (sym == 0 || sym == 2) o[n++] = w - 1 - x + y * w;
        if (sym == 1 || sym == 2) o[n++] = x + (h - 1 - y) * w;
        if (sym == 2) o[n++] = w - 1 - x + (h - 1 - y) * w;
        pfo_state keep = m->start;
        for (int i = 0; i < n; i++) m->start.cell[o[i]] = PFO_NONE, m->start.layers[o[i]] = 0;
        int paper = 0;
        for (int i = 0; i < w * h; i++) paper += m->start.cell[i] != PFO_NONE;
        if (!connected(&m->start, w, h) || !spans(&m->start, w, h) || paper * 10 < w * h * 6) m->start = keep;
        else cut++;
    }
}

void pfo_make(pfo_map *m, const pfo_spec *sp, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    for (int tries = 0; tries < 30; tries++) {
        sheet(m, sp, &r);
        pfo_state s = m->start, n;
        int made = 0, least = sp->least > 0 ? sp->least : m->w * m->h / 5 > 2 ? m->w * m->h / 5 : 2;
        while (made < sp->folds) {             /* folds at random, the paper left not too small */
            int can[PFO_MOVES], nc = 0;
            for (int f = 0; f < PFO_MOVES; f++) {
                if (!pfo_try(m, &s, f, &n)) continue;
                int left = 0;
                for (int c = 0; c < m->w * m->h; c++) left += n.cell[c] != PFO_NONE;
                if (left >= least) can[nc++] = f;
            }
            if (!nc) break;
            pfo_try(m, &s, can[pr_below(&r, nc)], &s);
            made++;
        }
        if (made < 1) continue;
        int used = 0;
        for (int c = 0; c < m->w * m->h; c++) {
            m->target[c] = s.cell[c] == PFO_NONE ? PFO_NONE : (uint8_t)pfo_up(s.cell[c]);
            if (m->target[c] != PFO_NONE) used |= 1 << m->target[c];
        }
        if (!(used & (used - 1)) && tries < 20) continue;   /* one colour alone: another */
        if (pfo_solve(m, &m->start, NULL, NULL) >= 1) return;
    }
    pfo_parse(m, "abab", "b.");                 /* (never seen) */
}

/* ---- the variants and the levels --------------------------------------------------------------------------------- */
void pfo_variant(int v, pfo_spec *sp)
{
    static const pfo_spec V[PFO_VARIANTS] = {
        {3, 3, 0, 0, 0, 3, 2, 2}, {4, 4, -1, 0, 1, 5, 3, 2}, {5, 5, -1, -1, 2, 7, 4, 2},
    };
    *sp = V[v < 0 || v >= PFO_VARIANTS ? 0 : v];
}

#ifndef PFO_GEN                                 /* not in the levels' tool (tools/paperfold_gen.c): it makes them */
#include "games/paperfold_levels.h"

int pfo_level(pfo_game *g, int level)
{
    if (level < 0 || level >= PFO_LEVELS || !pfo_parse(&g->m, pfo_levels[level].sheet, pfo_levels[level].picture))
        return 0;
    pfo_begin(g);
    g->par = pfo_levels[level].par;
    return 1;
}

/* a puzzle at random: made to the variant until its par is the one aimed at, by its size: 3 folds on 3 x 3, 4 or 5
 * on 4 x 4 (half each), 5 on 5 x 5 (6 there needs a picture of one square, too plain); else the nearest found */
void pfo_random(pfo_game *g, int variant, uint32_t seed)
{
    static const uint8_t LO[PFO_VARIANTS] = {3, 4, 5}, HI[PFO_VARIANTS] = {3, 5, 5};
    int v = variant < 0 || variant >= PFO_VARIANTS ? 0 : variant;
    pfo_spec sp;
    pfo_variant(v, &sp);
    uint32_t r = pr_seed(seed);
    int aim = LO[v] + (int)pr_below(&r, (uint32_t)(HI[v] - LO[v] + 1)), best = -1;
    for (int k = 0; k < 30 && best != aim; k++) {
        pfo_map m;
        pfo_make(&m, &sp, pr_next(&r));
        int p = pfo_solve(&m, &m.start, NULL, NULL);
        if (p >= 1 && (best < 1 || abs(p - aim) < abs(best - aim))) { best = p; g->m = m; }
    }
    if (best < 1) { pfo_level(g, 0); return; }
    pfo_begin(g);
    g->par = best;
}
#endif
