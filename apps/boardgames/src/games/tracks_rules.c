/*
 * Tracks: the rules (tracks.h): the line drawn by chance, the solver that reasons (tk_deduce), the search that counts
 * the solutions (tk_count, for the tests), the maker, and the player's cells with undo. No SDL: tested by
 * tests/tracks_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tracks.h"

#include <string.h>

#include "games/puzrand.h"

static const int SIDE[4] = {TK_N, TK_E, TK_S, TK_W};
static const int PIECES[6] = {TK_E | TK_W, TK_N | TK_S, TK_N | TK_E, TK_E | TK_S, TK_S | TK_W, TK_N | TK_W};

static int bits(int v) { return __builtin_popcount((unsigned)v); }

static int next_to(int n, int c, int side)
{
    int x = c % n, y = c / n;
    if (side == TK_N) y--;
    else if (side == TK_E) x++;
    else if (side == TK_S) y++;
    else x--;
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : x + y * n;
}

static int side_to(int n, int c, int d)         /* the side of c that faces its neighbour d (0: not next to it) */
{
    for (int i = 0; i < 4; i++)
        if (next_to(n, c, SIDE[i]) == d) return SIDE[i];
    return 0;
}

/* ---- the solver -------------------------------------------------------------------------------------------------- */
enum { UNKNOWN, TRACK, EMPTY };

typedef struct {
    int n, a, b, total, bad, used;
    uint8_t yes[TK_CELLS], no[TK_CELLS], st[TK_CELLS];
    uint8_t rows[TK_MAX], cols[TK_MAX];
} ks;

static int set_side(ks *s, int c, int side, int yes, int level)
{
    uint8_t *m = yes ? s->yes : s->no, *o = yes ? s->no : s->yes;
    if (m[c] & side) return 0;
    if (o[c] & side) { s->bad = 1; return 0; }
    m[c] |= (uint8_t)side;
    int d = next_to(s->n, c, side), op = tk_opp(side);
    if (d >= 0) {
        if (o[d] & op) { s->bad = 1; return 0; }
        m[d] |= (uint8_t)op;
    } else if (yes) {
        s->bad = 1;
    }
    if (level > s->used) s->used = level;
    return 1;
}

static int set_state(ks *s, int c, int v, int level)
{
    if (s->st[c] == v) return 0;
    if (s->st[c]) { s->bad = 1; return 0; }
    s->st[c] = (uint8_t)v;
    if (level > s->used) s->used = level;
    return 1;
}

/* level 0: a cell's own sides */
static int r_cells(ks *s)
{
    int ch = 0;
    for (int c = 0; c < s->n * s->n && !s->bad; c++) {
        int y = bits(s->yes[c]), unk = 15 & ~(s->yes[c] | s->no[c]), u = bits(unk);
        if (y > 2) { s->bad = 1; break; }
        if (y >= 1) ch |= set_state(s, c, TRACK, 0);
        if (s->st[c] == EMPTY || y == 2)
            for (int i = 0; i < 4; i++)
                if (unk & SIDE[i]) ch |= set_side(s, c, SIDE[i], 0, 0);
        if (y + u < 2) ch |= set_state(s, c, EMPTY, 0);
        if (s->st[c] == TRACK && y + u == 2)
            for (int i = 0; i < 4; i++)
                if (unk & SIDE[i]) ch |= set_side(s, c, SIDE[i], 1, 0);
    }
    return ch;
}

/* level 0: the counts of the rows and the columns */
static int r_counts(ks *s)
{
    int n = s->n, ch = 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n && !s->bad; i++) {
            int t = 0, u = 0, clue = way ? s->cols[i] : s->rows[i];
            for (int j = 0; j < n; j++) {
                int c = way ? i + j * n : j + i * n;
                t += s->st[c] == TRACK;
                u += s->st[c] == UNKNOWN;
            }
            if (t > clue || t + u < clue) { s->bad = 1; break; }
            if (u && (t == clue || t + u == clue))
                for (int j = 0; j < n; j++) {
                    int c = way ? i + j * n : j + i * n;
                    if (s->st[c] == UNKNOWN) ch |= set_state(s, c, t == clue ? EMPTY : TRACK, 0);
                }
            int along = way ? TK_S : TK_E;      /* a side between two cells of the line: both would be track */
            for (int j = 0; j + 1 < n; j++) {
                int c = way ? i + j * n : j + i * n, d = next_to(n, c, along);
                if ((s->yes[c] | s->no[c]) & along) continue;
                if (t + (s->st[c] == UNKNOWN) + (s->st[d] == UNKNOWN) > clue) ch |= set_side(s, c, along, 0, 0);
            }
        }
    return ch;
}

static int find(int *p, int c)
{
    while (p[c] != c) c = p[c] = p[p[c]];
    return c;
}

/* level 1: no loop, and the line not closed too soon */
static int r_loops(ks *s)
{
    int n = s->n, cells = n * n, p[TK_CELLS], size[TK_CELLS], ch = 0, track = 0;
    for (int c = 0; c < cells; c++) p[c] = c, size[c] = 0;
    for (int c = 0; c < cells; c++)
        for (int k = 1; k <= 2; k++) {          /* east and south */
            int sd = SIDE[k], d = next_to(n, c, sd);
            if (d >= 0 && (s->yes[c] & sd)) p[find(p, c)] = find(p, d);
        }
    for (int c = 0; c < cells; c++) {
        size[find(p, c)] += s->st[c] == TRACK;
        track += s->st[c] == TRACK;
    }
    int ra = find(p, s->a), rb = find(p, s->b);
    if (ra == rb) {                             /* the line is whole: nothing else */
        if (size[ra] != s->total || track != size[ra]) { s->bad = 1; return 0; }
        for (int c = 0; c < cells; c++)
            if (s->st[c] == UNKNOWN) ch |= set_state(s, c, EMPTY, 1);
        return ch;
    }
    for (int c = 0; c < cells && !s->bad; c++)
        for (int k = 1; k <= 2; k++) {
            int sd = SIDE[k], d = next_to(n, c, sd);
            if (d < 0 || ((s->yes[c] | s->no[c]) & sd)) continue;
            int pc = find(p, c), pd = find(p, d);
            if (pc == pd) ch |= set_side(s, c, sd, 0, 1);
            else if ((pc == ra && pd == rb) || (pc == rb && pd == ra)) {
                int inside = size[ra] + size[rb];
                if (inside < s->total || track > size[ra] + size[rb]) ch |= set_side(s, c, sd, 0, 1);
            }
        }
    return ch;
}

/* level 2: the line crosses a cut between two columns (rows) an odd number of times when the entry and the exit are
 * on either side of it */
static int r_cuts(ks *s)
{
    int n = s->n, ch = 0;
    for (int way = 0; way < 2; way++)
        for (int k = 0; k + 1 < n && !s->bad; k++) {
            int pa = way ? s->a / n : s->a % n, pb = way ? s->b / n : s->b % n, odd = (pa <= k) != (pb <= k);
            int sd = way ? TK_S : TK_E, yes = 0, unk = 0, last = -1;
            for (int j = 0; j < n; j++) {
                int c = way ? j + k * n : k + j * n;
                if (s->yes[c] & sd) yes++;
                else if (!(s->no[c] & sd)) unk++, last = c;
            }
            if (!unk && yes % 2 != odd) s->bad = 1;
            else if (unk == 1) ch |= set_side(s, last, sd, yes % 2 != odd, 2);
        }
    return ch;
}

static void reason(ks *s, int level);

/* level 3: a side tried one way, followed through with the rest: a contradiction proves the other way. The sides of
 * the cells known to hold track first (where a person looks), then the others; a budget of tries (the maker's time) */
static long trial_budget = -1;

static int r_trial(ks *s)
{
    int n = s->n, found = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int c = 0; c < n * n; c++)
            for (int k = 0; k < 4; k++) {
                int sd = SIDE[k], d = next_to(n, c, sd);
                if (d < 0 || ((s->yes[c] | s->no[c]) & sd)) continue;
                if ((s->st[c] == TRACK || s->st[d] == TRACK) != !pass || (pass && k >= 2)) continue;
                for (int yes = 1; yes >= 0; yes--) {
                    if (trial_budget == 0) return found;
                    if (trial_budget > 0) trial_budget--;
                    ks t = *s;
                    set_side(&t, c, sd, yes, 2);
                    reason(&t, 2);
                    if (t.bad) {               /* proved: on with the rest, from here */
                        set_side(s, c, sd, !yes, 3);
                        reason(s, 2);
                        if (s->bad) return 1;
                        found = 1;
                        break;
                    }
                }
            }
    return found;
}

static void reason(ks *s, int level)
{
    while (!s->bad) {
        if (r_cells(s) || r_counts(s)) continue;
        if (s->bad) break;
        if (level >= 1 && r_loops(s)) continue;
        if (s->bad) break;
        if (level >= 2 && r_cuts(s)) continue;
        if (s->bad) break;
        if (level >= 3 && r_trial(s)) continue;
        break;
    }
}

static void ks_init(ks *s, const tk_game *g)
{
    int n = g->n;
    memset(s, 0, sizeof *s);
    s->n = n;
    s->a = g->a;
    s->b = g->b;
    memcpy(s->rows, g->rows, sizeof s->rows);
    memcpy(s->cols, g->cols, sizeof s->cols);
    for (int i = 0; i < n; i++) s->total += g->rows[i];
    for (int c = 0; c < n * n; c++)
        for (int i = 0; i < 4; i++)
            if (next_to(n, c, SIDE[i]) < 0) {
                int stub = (c == g->a && SIDE[i] == g->a_side) || (c == g->b && SIDE[i] == g->b_side);
                (stub ? s->yes : s->no)[c] |= (uint8_t)SIDE[i];
            }
    for (int c = 0; c < n * n; c++) {
        if (!g->given[c]) continue;
        s->st[c] = TRACK;
        for (int i = 0; i < 4; i++) set_side(s, c, SIDE[i], (g->sol[c] & SIDE[i]) != 0, 0);
    }
    s->used = 0;
}

static int ks_done(const ks *s)
{
    for (int c = 0; c < s->n * s->n; c++)
        if ((s->yes[c] | s->no[c]) != 15) return 0;
    return !s->bad;
}

int tk_deduce(const tk_game *g, int level, int *grade, uint8_t *out)
{
    static ks s;
    ks_init(&s, g);
    reason(&s, level);
    if (grade) *grade = s.used;
    if (!ks_done(&s)) return 0;
    if (out)
        for (int c = 0; c < g->n * g->n; c++) out[c] = s.yes[c];
    return 1;
}

/* ---- the search: the line followed from the entry, a cell at a time ---------------------------------------------- */
typedef struct {
    const tk_game *g;
    int n, total, count, limit, len;
    long nodes;
    uint8_t used[TK_CELLS], row[TK_MAX], col[TK_MAX];
} searcher;

static void walk_on(searcher *k, int c, int from)   /* at c, come in through side from */
{
    const tk_game *g = k->g;
    int n = k->n, x = c % n, y = c / n;
    if (k->count >= k->limit || ++k->nodes > 50000000L) return;
    if (k->used[c] || k->row[y] >= g->rows[y] || k->col[x] >= g->cols[x]) return;
    if (g->given[c] && !(g->sol[c] & from)) return;
    int left = k->total - k->len - 1, dx = x - g->b % n, dy = y - g->b / n;
    int far = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (far > left || (far - left) % 2) return;  /* the exit too far, or on the wrong square colour */
    k->used[c] = 1, k->row[y]++, k->col[x]++, k->len++;
    for (int i = 0; i < 4; i++) {
        int sd = SIDE[i];
        if (sd == from || (g->given[c] && !(g->sol[c] & sd))) continue;
        if (c == g->b && sd == g->b_side) {     /* out through the exit: every count met, every given used */
            if (k->len != k->total) continue;
            int ok = 1;
            for (int j = 0; j < n && ok; j++) ok = k->row[j] == g->rows[j] && k->col[j] == g->cols[j];
            for (int d = 0; d < n * n && ok; d++) ok = !g->given[d] || k->used[d];
            k->count += ok;
            continue;
        }
        int d = next_to(n, c, sd);
        if (d >= 0) walk_on(k, d, tk_opp(sd));
    }
    k->used[c] = 0, k->row[y]--, k->col[x]--, k->len--;
}

int tk_count(const tk_game *g, int limit)
{
    static searcher k;
    memset(&k, 0, sizeof k);
    k.g = g;
    k.n = g->n;
    k.limit = limit;
    for (int i = 0; i < g->n; i++) k.total += g->rows[i];
    walk_on(&k, g->a, g->a_side);
    return k.nodes > 50000000L && k.count < limit ? -1 : k.count;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static const struct { int n, lmin, lmax, level, gmin; } SIZE[TK_VARIANTS] = {
    {6, 13, 22, 1, 0},
    {8, 24, 40, 2, 1},
    {10, 38, 62, 3, 2},
};

/* the line drawn by chance: from a cell of the edge, cell to cell (straight on more often than not), to a cell of
 * another edge */
static int draw_line(tk_game *g, uint32_t *r, int lmin, int lmax)
{
    int n = g->n;
    int path[TK_CELLS], dirs[TK_CELLS][4], tried[TK_CELLS], len = 0;
    uint8_t used[TK_CELLS] = {0};
    int x = pr_below(r, n), y = pr_below(r, n), e = pr_below(r, 4);
    if (e == 0) y = 0;
    else if (e == 1) x = n - 1;
    else if (e == 2) y = n - 1;
    else x = 0;
    g->a = x + y * n;
    g->a_side = (uint8_t)SIDE[e];
    path[len] = g->a;
    used[g->a] = 1;
    tried[len] = 0;
    int in = SIDE[(e + 2) & 3];                 /* the way the line goes in */
    for (int i = 0; i < 4; i++) dirs[len][i] = SIDE[i];
    pr_shuffle(r, dirs[len], 4);
    for (int i = 0; i < 4; i++)
        if (dirs[len][i] == in) { dirs[len][i] = dirs[len][0]; dirs[len][0] = in; }
    len = 1;
    for (long steps = 0; len > 0 && steps < 6000; steps++) {
        int c = path[len - 1];
        if (len >= lmin && len <= lmax && pr_below(r, 3) == 0) {   /* an exit here? */
            int sides[4], ns = 0;
            for (int i = 0; i < 4; i++)
                if (next_to(n, c, SIDE[i]) < 0 && SIDE[i] != g->a_side && c != g->a) sides[ns++] = SIDE[i];
            if (ns) {
                g->b = c;
                g->b_side = (uint8_t)sides[pr_below(r, ns)];
                memset(g->sol, 0, sizeof g->sol);
                for (int i = 0; i < len; i++) {
                    int p = path[i];
                    g->sol[p] = (uint8_t)((i ? side_to(n, p, path[i - 1]) : g->a_side) |
                                          (i + 1 < len ? side_to(n, p, path[i + 1]) : g->b_side));
                }
                return 1;
            }
        }
        if (len >= lmax || tried[len - 1] >= 4) {   /* back */
            used[c] = 0;
            len--;
            continue;
        }
        int sd = dirs[len - 1][tried[len - 1]++], d = next_to(n, c, sd);
        if (d < 0 || used[d]) continue;
        used[d] = 1;
        path[len] = d;
        tried[len] = 0;
        for (int i = 0; i < 4; i++) dirs[len][i] = SIDE[i];
        pr_shuffle(r, dirs[len], 4);
        if (pr_below(r, 2))                     /* straight on first, half of the time */
            for (int i = 0; i < 4; i++)
                if (dirs[len][i] == sd) { dirs[len][i] = dirs[len][0]; dirs[len][0] = sd; }
        len++;
    }
    return 0;
}

static int try_make(tk_game *g, int variant, uint32_t *r)
{
    int n = SIZE[variant].n, level = SIZE[variant].level;
    memset(g, 0, sizeof *g);
    g->n = n;
    if (!draw_line(g, r, SIZE[variant].lmin, SIZE[variant].lmax)) return 0;
    for (int c = 0; c < n * n; c++)
        if (g->sol[c]) g->rows[c / n]++, g->cols[c % n]++;
    g->given[g->a] = g->given[g->b] = 1;
    int grade;
    for (int k = 0; k < n * n; k++) {           /* stuck: a piece given where the solver is stuck */
        static ks s;
        ks_init(&s, g);
        reason(&s, level);
        if (s.bad) return 0;
        if (ks_done(&s)) break;
        int cand[TK_CELLS], nc = 0;
        for (int c = 0; c < n * n; c++)
            if (g->sol[c] && !g->given[c] && (s.yes[c] | s.no[c]) != 15) cand[nc++] = c;
        if (!nc) return 0;
        g->given[cand[pr_below(r, nc)]] = 1;
    }
    int gv[TK_CELLS], ng = 0;                   /* the pieces not needed taken away again */
    for (int c = 0; c < n * n; c++)
        if (g->given[c] && c != g->a && c != g->b) gv[ng++] = c;
    pr_shuffle(r, gv, ng);
    for (int i = 0; i < ng; i++) {
        g->given[gv[i]] = 0;
        if (!tk_deduce(g, level, NULL, NULL)) g->given[gv[i]] = 1;
    }
    uint8_t out[TK_CELLS];
    if (!tk_deduce(g, level, &grade, out) || memcmp(out, g->sol, (size_t)(n * n))) return 0;
    g->grade = grade;
    return grade >= SIZE[variant].gmin;
}

void tk_new(tk_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= TK_VARIANTS) variant = 0;
    uint32_t r = pr_seed(seed);
    static tk_game best;
    int have = 0;
    trial_budget = 12000;                       /* the maker's time: past it, trying stops */
    for (int tries = 0; tries < 400; tries++) {
        if (try_make(g, variant, &r)) { have = 2; break; }
        if (!have && g->n && g->b != g->a && tk_deduce(g, SIZE[variant].level, NULL, NULL)) best = *g, have = 1;
    }
    if (have == 1) *g = best;                   /* none needs the size's reasoning: the last one solvable */
    trial_budget = -1;
    memset(g->cell, 0, sizeof g->cell);
    g->nh = g->moves = 0;
}

/* ---- the player's cells ------------------------------------------------------------------------------------------ */
static void change(tk_game *g, int c, int v, int first)
{
    if (g->nh >= TK_HIST) {
        int k = 1;
        while (k < g->nh && !g->hist[k].first) k++;
        memmove(g->hist, g->hist + k, (size_t)(g->nh - k) * sizeof g->hist[0]);
        g->nh -= k;
    }
    g->hist[g->nh].cell = (uint8_t)c, g->hist[g->nh].was = g->cell[c], g->hist[g->nh++].first = (uint8_t)first;
    g->cell[c] = (uint8_t)v;
}

static int shown(const tk_game *g, int c) { return g->given[c] ? g->sol[c] : g->cell[c]; }

int tk_set(tk_game *g, int c, int v)
{
    if (c < 0 || c >= g->n * g->n || g->given[c] || g->cell[c] == v) return 0;
    if (v != 0 && v != TK_X && (bits(v) != 2 || v > 15)) return 0;
    change(g, c, v, 1);
    g->moves++;
    return 1;
}

/* how well a piece fits its neighbours: its ends meeting theirs, not running off the board nor into a cross */
static int fit(const tk_game *g, int c, int piece)
{
    int score = 0;
    for (int i = 0; i < 4; i++) {
        if (!(piece & SIDE[i])) continue;
        int d = next_to(g->n, c, SIDE[i]);
        if (d < 0) { score -= 4; continue; }
        int v = shown(g, d);
        if (v == TK_X) score -= 2;
        else if (v & tk_opp(SIDE[i])) score += 2;
        else if (v) score -= 1;
    }
    return score;
}

int tk_tap(tk_game *g, int c)
{
    if (c < 0 || c >= g->n * g->n || g->given[c]) return 0;
    if (g->cell[c] == TK_X) return tk_set(g, c, 0);
    int order[6], sc[6];
    for (int i = 0; i < 6; i++) {               /* the pieces, the best fitting first */
        int j = i, s = fit(g, c, PIECES[i]);
        while (j > 0 && sc[j - 1] < s) { order[j] = order[j - 1]; sc[j] = sc[j - 1]; j--; }
        order[j] = PIECES[i];
        sc[j] = s;
    }
    int at = -1;
    for (int i = 0; i < 6; i++)
        if (order[i] == g->cell[c]) at = i;
    return tk_set(g, c, at + 1 < 6 ? order[at + 1] : 0);
}

int tk_lay(tk_game *g, const int *cells, int len)
{
    int n = g->n, val[TK_CELLS];
    if (len < 2) return 0;
    for (int i = 0; i < len; i++) {
        if (cells[i] < 0 || cells[i] >= n * n) return 0;
        if (i && !side_to(n, cells[i], cells[i - 1])) return 0;
        for (int j = 0; j < i; j++)
            if (cells[j] == cells[i]) return 0;
    }
    for (int i = 0; i < len; i++) {
        int c = cells[i], ends = 0;
        val[i] = g->cell[c];
        if (g->given[c]) continue;
        if (i) ends |= side_to(n, c, cells[i - 1]);
        if (i + 1 < len) ends |= side_to(n, c, cells[i + 1]);
        if (bits(ends) == 1) {                  /* an end of the stroke: its other end kept, or found */
            int cur = g->cell[c] & 15 & ~ends, other = 0;
            for (int k = 0; k < 4 && !other; k++) {   /* an end meeting a neighbour's, the piece's own first */
                int sd = SIDE[k], d = next_to(n, c, sd);
                if (sd != ends && d >= 0 && (shown(g, d) & 15 & tk_opp(sd)) && (cur & sd)) other = sd;
            }
            for (int k = 0; k < 4 && !other; k++) {
                int sd = SIDE[k], d = next_to(n, c, sd);
                int in_stroke = (i && d == cells[i - 1]) || (i + 1 < len && d == cells[i + 1]);
                if (sd != ends && d >= 0 && !in_stroke && shown(g, d) != TK_X && (shown(g, d) & tk_opp(sd)))
                    other = sd;
            }
            if (!other && cur) other = cur & -cur;
            if (!other && next_to(n, c, tk_opp(ends)) >= 0) other = tk_opp(ends);
            for (int k = 0; k < 4 && !other; k++)
                if (SIDE[k] != ends && next_to(n, c, SIDE[k]) >= 0) other = SIDE[k];
            ends |= other;
        }
        val[i] = ends;
    }
    int first = 1;
    for (int i = 0; i < len; i++)
        if (!g->given[cells[i]] && g->cell[cells[i]] != val[i]) {
            change(g, cells[i], val[i], first);
            first = 0;
        }
    if (first) return 0;
    g->moves++;
    return 1;
}

int tk_mark(tk_game *g, const int *cells, int len)
{
    if (len < 1 || cells[0] < 0 || cells[0] >= g->n * g->n) return 0;
    int clear = g->cell[cells[0]] == TK_X, first = 1;
    for (int i = 0; i < len; i++) {
        int c = cells[i];
        if (c < 0 || c >= g->n * g->n || g->given[c]) continue;
        int to = clear ? (g->cell[c] == TK_X ? 0 : g->cell[c]) : (g->cell[c] ? g->cell[c] : TK_X);
        if (to == g->cell[c]) continue;
        change(g, c, to, first);
        first = 0;
    }
    if (first) return 0;
    g->moves++;
    return 1;
}

int tk_solved(const tk_game *g)
{
    for (int c = 0; c < g->n * g->n; c++) {
        if (g->given[c]) continue;
        int v = g->cell[c] & 15;
        if (g->sol[c] ? v != g->sol[c] : v != 0) return 0;
    }
    return 1;
}

int tk_hint(tk_game *g)
{
    int n = g->n;
    if (tk_solved(g)) return -1;
    for (int c = 0; c < n * n; c++) {           /* a cell set wrong: put right */
        if (g->given[c] || !g->cell[c]) continue;
        int v = g->cell[c], p = g->sol[c];
        if (p ? v != p : v != TK_X) {
            if (!p && v == TK_X) continue;
            tk_set(g, c, p ? p : TK_X);
            return c;
        }
    }
    int c = g->a, from = g->a_side;             /* the first cell along the line not laid yet */
    for (int k = 0; k < n * n; k++) {
        if (!g->given[c] && g->cell[c] != g->sol[c]) {
            tk_set(g, c, g->sol[c]);
            return c;
        }
        int out = g->sol[c] & ~from;
        if (c == g->b) break;
        int d = next_to(n, c, out);
        if (d < 0) break;
        from = tk_opp(out);
        c = d;
    }
    return -1;
}

int tk_undo(tk_game *g)
{
    if (!g->nh) return -1;
    int c = -1;
    for (;;) {
        tk_change h = g->hist[--g->nh];
        g->cell[h.cell] = h.was;
        c = h.cell;
        if (h.first || !g->nh) break;
    }
    if (g->moves > 0) g->moves--;
    return c;
}

int tk_line_count(const tk_game *g, int way, int i)
{
    int k = 0;
    for (int j = 0; j < g->n; j++) {
        int c = way ? i + j * g->n : j + i * g->n;
        k += (shown(g, c) & 15) != 0;
    }
    return k;
}

int tk_cell(const tk_game *g, const char *s)
{
    if (!s || s[0] < 'a' || s[0] >= 'a' + g->n || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0';
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0';
    if (y < 1 || y > g->n) return -1;
    return s[0] - 'a' + (y - 1) * g->n;
}

static const char *cell_end(const char *s) { return s + (s[2] >= '0' && s[2] <= '9' ? 3 : 2); }

int tk_script(tk_game *g, const char *s)
{
    int n = g->n;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != ' ') e++;
        int ok = 0;
        if (*s == 'h' && e == s + 1) {
            ok = tk_hint(g) >= 0;
        } else if (*s == '=') {
            int k = e > s + 1 ? 0 : 9999;
            for (const char *p = s + 1; p < e; p++) k = k * 10 + (*p - '0');
            ok = 1;
            for (int i = 0; i < k && !tk_solved(g); i++)
                if (tk_hint(g) < 0) { ok = 0; break; }
        } else {
            int c = tk_cell(g, s);
            if (c < 0) return 0;
            const char *p = cell_end(s);
            if (p == e) ok = tk_tap(g, c);
            else if (*p == 'x' && p + 1 == e) ok = tk_set(g, c, TK_X);
            else if (*p == '.' && p + 1 == e) ok = tk_set(g, c, 0);
            else if (*p == '-') {               /* a stroke: straight runs between the cells named */
                int path[TK_CELLS], len = 1;
                path[0] = c;
                while (*p == '-') {
                    int d = tk_cell(g, p + 1);
                    if (d < 0) return 0;
                    int x = path[len - 1] % n, y = path[len - 1] / n, tx = d % n, ty = d / n;
                    if (x != tx && y != ty) return 0;
                    while ((x != tx || y != ty) && len < TK_CELLS) {
                        x += tx > x ? 1 : tx < x ? -1 : 0;
                        y += ty > y ? 1 : ty < y ? -1 : 0;
                        path[len++] = x + y * n;
                    }
                    p = cell_end(p + 1);
                }
                ok = p == e && tk_lay(g, path, len);
            } else {
                int v = 0;
                for (; p < e; p++)
                    v |= *p == 'n' ? TK_N : *p == 'e' ? TK_E : *p == 's' ? TK_S : *p == 'w' ? TK_W : 64;
                ok = v < 16 && tk_set(g, c, v);
            }
        }
        if (!ok) return 0;
        s = e;
    }
    return 1;
}
