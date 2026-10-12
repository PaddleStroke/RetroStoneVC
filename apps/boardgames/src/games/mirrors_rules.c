/*
 * Mirrors: the rules (mirrors.h): the beam traced, the search that counts the ways to solve a puzzle, the maker, and
 * the player's mirrors with undo. No SDL: tested by tests/mirrors_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mirrors.h"

#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, -1, 0, 1};

static int step(int n, int c, int d)
{
    int x = c % n + DX[d], y = c / n + DY[d];
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : x + y * n;
}

static int is_mirror(int k) { return k == MR_PIVOT || k == MR_LOOSE; }

/* ---- the beam ---------------------------------------------------------------------------------------------------- */
void mr_trace(const mr_game *g, mr_beam *b)
{
    int n = g->n;
    uint8_t seen[MR_CELLS] = {0};               /* bit d: a beam has left the cell going d */
    struct { uint8_t c, d; uint16_t t; } q[4 * MR_CELLS];
    int qh = 0, qt = 0;
    memset(b->side, 0, sizeof b->side);
    for (int c = 0; c < MR_CELLS; c++) b->dist[c] = 0xffff;
    b->gems = b->lit = b->length = 0;
    b->bomb = -1;
    b->dist[g->lamp] = 0;
    q[qt].c = (uint8_t)g->lamp, q[qt].d = g->way[g->lamp], q[qt++].t = 0;
    while (qh < qt) {
        int c = q[qh].c, d = q[qh].d, t = q[qh++].t;
        for (;;) {
            if (seen[c] >> d & 1) break;
            seen[c] |= (uint8_t)(1 << d);
            b->side[c] |= (uint8_t)(1 << d);
            int nc = step(n, c, d);
            if (nc < 0) break;                  /* off the board, into the frame */
            int k = g->kind[nc];
            if (k == MR_WALL || k == MR_LAMP) break;   /* stopped at its face */
            t++;
            if (b->dist[nc] > t) b->dist[nc] = (uint16_t)t;
            if (t > b->length) b->length = t;
            b->side[nc] |= (uint8_t)(1 << ((d + 2) & 3));
            if (k == MR_BOMB) {
                if (b->bomb < 0) b->bomb = nc;
                break;
            }
            c = nc;
            if (is_mirror(k)) d = mr_reflect(d, g->way[nc]);
            else if (k == MR_GLASS && qt < 4 * MR_CELLS)
                q[qt].c = (uint8_t)nc, q[qt].d = (uint8_t)mr_reflect(d, g->way[nc]), q[qt++].t = (uint16_t)t;
        }
    }
    for (int c = 0; c < n * n; c++)
        if (g->kind[c] == MR_GEM) {
            b->gems++;
            if (b->side[c]) b->lit++;
        }
}

int mr_solved(const mr_game *g)
{
    mr_beam b;
    mr_trace(g, &b);
    return b.bomb < 0 && b.lit == b.gems;
}

int mr_loose_left(const mr_game *g)
{
    int used = 0;
    for (int c = 0; c < g->n * g->n; c++) used += g->kind[c] == MR_LOOSE;
    return g->loose - used;
}

/* ---- the search: the beam followed from the lamp; at each pivot it meets, both ways tried; at each empty cell, left
 * empty or a loose mirror stood there either way (while some are left). Beams split by glass wait in a queue. ------ */
typedef struct {
    int n, left, count, limit;
    long work, max_work;
    uint8_t kind[MR_CELLS], way[MR_CELLS], seen[MR_CELLS], lit[MR_CELLS];
    int8_t dec[MR_CELLS];                       /* -1 not decided; 0, 1 a mirror's way; 2 nothing */
    struct { uint8_t c, seen, lit; } trail[2048];
    int nt;
    uint8_t qc[512], qd[512];
    uint8_t stack[MR_CELLS];                    /* the cells decided, in order */
    int ns;
    int8_t sol[MR_CELLS];
    uint8_t order[MR_CELLS];
    int norder;
} solver;

static void save(solver *s, int c)
{
    if (s->nt < 2048) {
        s->trail[s->nt].c = (uint8_t)c, s->trail[s->nt].seen = s->seen[c], s->trail[s->nt].lit = s->lit[c];
        s->nt++;
    }
}

static void restore(solver *s, int mark)
{
    while (s->nt > mark) {
        s->nt--;
        s->seen[s->trail[s->nt].c] = s->trail[s->nt].seen;
        s->lit[s->trail[s->nt].c] = s->trail[s->nt].lit;
    }
}

static void leaf(solver *s)
{
    for (int c = 0; c < s->n * s->n; c++)
        if (s->kind[c] == MR_GEM && !s->lit[c]) return;
    if (++s->count == 1) {
        memcpy(s->sol, s->dec, sizeof s->sol);
        memcpy(s->order, s->stack, (size_t)s->ns);
        s->norder = s->ns;
    }
}

static void follow(solver *s, int c, int d, int head, int tail)
{
    int mark = s->nt;
    if (s->count >= s->limit || s->work > s->max_work) return;
    for (;;) {
        int nc = -1, k = MR_WALL;
        if (!(s->seen[c] >> d & 1)) {
            save(s, c);
            s->seen[c] |= (uint8_t)(1 << d);
            nc = step(s->n, c, d);
            if (nc >= 0) k = s->kind[nc];
        }
        if (nc < 0 || k == MR_WALL || k == MR_LAMP) {   /* this beam ends: the next waiting one, or the end */
            if (head < tail) {
                c = s->qc[head];
                d = s->qd[head++];
                continue;
            }
            leaf(s);
            break;
        }
        save(s, nc);
        s->lit[nc]++;
        if (k == MR_BOMB) break;
        if (k == MR_GLASS) {
            if (tail < 512) s->qc[tail] = (uint8_t)nc, s->qd[tail++] = (uint8_t)mr_reflect(d, s->way[nc]);
            c = nc;
            continue;
        }
        if (k == MR_GEM || (k == MR_EMPTY && (s->dec[nc] == 2 || (s->dec[nc] < 0 && !s->left)))) {
            c = nc;                             /* straight on (no loose mirror left: no choice) */
            continue;
        }
        if (s->dec[nc] >= 0) { d = mr_reflect(d, s->dec[nc]); c = nc; continue; }
        s->work++;                              /* a choice */
        s->stack[s->ns++] = (uint8_t)nc;
        if (k == MR_EMPTY) {
            s->dec[nc] = 2;
            follow(s, nc, d, head, tail);
        }
        if (k == MR_PIVOT || s->left > 0) {
            s->left -= k == MR_EMPTY;
            for (int w = 0; w < 2; w++) {
                s->dec[nc] = (int8_t)w;
                follow(s, nc, mr_reflect(d, w), head, tail);
            }
            s->left += k == MR_EMPTY;
        }
        s->dec[nc] = -1;
        s->ns--;
        break;
    }
    restore(s, mark);
}

int mr_solve(mr_game *g, int limit, long *work)
{
    static solver s;
    memset(&s, 0, sizeof s);
    s.n = g->n;
    s.left = g->loose;
    s.limit = limit;
    s.max_work = 400000;
    for (int c = 0; c < MR_CELLS; c++) {
        s.kind[c] = g->kind[c] == MR_LOOSE ? MR_EMPTY : g->kind[c];
        s.way[c] = g->way[c];
        s.dec[c] = -1;
    }
    follow(&s, g->lamp, g->way[g->lamp], 0, 0);
    if (work) *work = s.work > s.max_work ? -1 : s.work;
    if (s.count >= 1) {
        memcpy(g->sol, s.sol, sizeof g->sol);
        memcpy(g->order, s.order, sizeof g->order);
        g->norder = s.norder;
    }
    return s.count;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static const struct {
    int n, pivots, loose, glass, gems, bombs, walls, decoys;
    long work_min, work_max;
} LEVEL[MR_VARIANTS] = {
    {5, 4, 0, 0, 2, 0, 1, 1, 5, 400},
    {6, 3, 1, 0, 3, 1, 2, 1, 10, 4000},
    {7, 3, 2, 1, 3, 2, 3, 2, 40, 40000},
    {8, 3, 3, 1, 4, 3, 3, 2, 200, 300000},
};

static int pick(uint32_t *r, const int *list, int n) { return n ? list[pr_below(r, n)] : -1; }

/* the beam's path drawn: straight runs, a mirror (or the glass) at the end of each; 0: no room */
static int walk(mr_game *g, uint32_t *r, int turns, int glass_at)
{
    int n = g->n, pos = g->lamp, d = g->way[g->lamp];
    uint8_t crossed[MR_CELLS] = {0};
    crossed[pos] = 1;
    for (int i = 0; i < turns; i++) {
        int opt[2 * MR_MAX], no = 0;
        for (int c = step(n, pos, d), l = 1; c >= 0; c = step(n, c, d), l++) {
            int k = g->kind[c];
            if (k != MR_EMPTY) break;           /* a block, a mirror placed before: the run stops */
            if (crossed[c]) continue;
            for (int turn = 0; turn < 2; turn++) {
                int nd = (d + (turn ? 1 : 3)) & 3, a = step(n, c, nd);
                if (a >= 0 && g->kind[a] == MR_EMPTY) opt[no++] = l * 2 + turn;
            }
        }
        if (!no) return 0;
        int o = opt[pr_below(r, no)], l = o / 2, nd = (d + (o & 1 ? 1 : 3)) & 3;
        int c = pos;
        for (int j = 0; j < l; j++) {
            c = step(n, c, d);
            crossed[c] = 1;
        }
        g->kind[c] = i == glass_at ? MR_GLASS : MR_LOOSE;
        g->way[c] = (uint8_t)(nd == (d ^ 1) ? MR_SLASH : MR_BACK);
        pos = c;
        d = i == glass_at && pr_below(r, 2) ? d : nd;   /* past the glass: either beam goes on */
    }
    return 1;
}

static int near_beam(const mr_game *g, const mr_beam *b, int c)
{
    for (int d = 0; d < 4; d++) {
        int a = step(g->n, c, d);
        if (a >= 0 && (b->side[a] || is_mirror(g->kind[a]))) return 1;
    }
    return 0;
}

static int try_make(mr_game *g, int level, uint32_t *r)
{
    int n = LEVEL[level].n, cells = n * n;
    memset(g->kind, 0, sizeof g->kind);
    memset(g->way, 0, sizeof g->way);
    g->n = n;
    g->level = level;
    /* the lamp: on the board's edge mostly, facing in */
    int lx = pr_below(r, n), ly = pr_below(r, n), d;
    if (pr_below(r, 4)) {
        int side = pr_below(r, 4);
        if (side == 0) lx = 0, d = 0;
        else if (side == 1) ly = n - 1, d = 1;
        else if (side == 2) lx = n - 1, d = 2;
        else ly = 0, d = 3;
    } else {
        d = pr_below(r, 4);
    }
    g->lamp = lx + ly * n;
    int ahead = step(n, g->lamp, d);
    if (ahead < 0 || step(n, ahead, d) < 0) return 0;
    g->kind[g->lamp] = MR_LAMP;
    g->way[g->lamp] = (uint8_t)d;
    for (int i = 0; i < LEVEL[level].walls; i++) {
        int c = pr_below(r, cells);
        if (g->kind[c] == MR_EMPTY && c != ahead) g->kind[c] = MR_WALL;
    }
    int pivots = LEVEL[level].pivots, loose = LEVEL[level].loose, glass = LEVEL[level].glass;
    int turns = pivots + loose + glass;
    if (!walk(g, r, turns, glass ? 1 + pr_below(r, turns - 1) : -1)) return 0;
    /* which of the path's mirrors pivot, which are loose */
    int ms[MR_CELLS], nm = 0;
    for (int c = 0; c < cells; c++)
        if (g->kind[c] == MR_LOOSE) ms[nm++] = c;
    pr_shuffle(r, ms, nm);
    for (int i = 0; i < pivots && i < nm; i++) g->kind[ms[i]] = MR_PIVOT;
    mr_beam b;
    mr_trace(g, &b);
    for (int c = 0; c < cells; c++)
        if ((is_mirror(g->kind[c]) || g->kind[c] == MR_GLASS) && !b.side[c]) return 0;   /* every one in use */
    /* the gems: the farthest lit cell, then others along the beam */
    int lit[MR_CELLS], nl = 0, far = -1;
    for (int c = 0; c < cells; c++)
        if (g->kind[c] == MR_EMPTY && b.side[c]) {
            lit[nl++] = c;
            if (far < 0 || b.dist[c] > b.dist[far]) far = c;
        }
    if (nl < LEVEL[level].gems + 1) return 0;
    g->kind[far] = MR_GEM;
    for (int i = 1, tries = 0; i < LEVEL[level].gems && tries < 200; tries++) {
        int c = pick(r, lit, nl);
        if (g->kind[c] != MR_EMPTY) continue;
        g->kind[c] = MR_GEM;
        i++;
    }
    /* the bombs beside the beam, the misleading pivots off it */
    for (int pass = 0; pass < 2; pass++) {
        int want = pass ? LEVEL[level].decoys : LEVEL[level].bombs, cand[MR_CELLS], nc = 0;
        for (int c = 0; c < cells; c++)
            if (g->kind[c] == MR_EMPTY && !b.side[c] && near_beam(g, &b, c)) cand[nc++] = c;
        pr_shuffle(r, cand, nc);
        for (int i = 0; i < want && i < nc; i++) {
            g->kind[cand[i]] = (uint8_t)(pass ? MR_PIVOT : MR_BOMB);
            g->way[cand[i]] = (uint8_t)pr_below(r, 2);
        }
    }
    /* the puzzle: the loose mirrors off the board, the pivots turned by chance */
    g->loose = 0;
    int wrong = 0, right[MR_CELLS], nr = 0;
    for (int c = 0; c < cells; c++) {
        if (g->kind[c] == MR_LOOSE) g->kind[c] = MR_EMPTY, g->loose++;
        if (g->kind[c] != MR_PIVOT) continue;
        int was = g->way[c];
        g->way[c] = (uint8_t)pr_below(r, 2);
        if (!b.side[c]) continue;
        if (g->way[c] != was) wrong++;
        else right[nr++] = c;
    }
    pr_shuffle(r, right, nr);                   /* a few steps from the solution at least */
    for (int i = 0; i < nr && wrong + g->loose < 2 + level / 2; i++, wrong++) g->way[right[i]] ^= 1;
    if (mr_solved(g)) return 0;
    long work;
    if (mr_solve(g, 2, &work) != 1) return 0;
    return work >= LEVEL[level].work_min && work <= LEVEL[level].work_max;
}

void mr_new(mr_game *g, int level, uint32_t seed)
{
    if (level < 0 || level >= MR_VARIANTS) level = 0;
    uint32_t r = pr_seed(seed);
    memset(g, 0, sizeof *g);
    for (int tries = 0; tries < 20000; tries++)
        if (try_make(g, level, &r)) break;
    g->nh = g->moves = 0;
}

/* ---- the player's mirrors ---------------------------------------------------------------------------------------- */
static void change(mr_game *g, int c, int kind, int way, int first)
{
    if (g->nh >= MR_HIST) {                     /* full: the oldest action forgotten */
        int k = 1;
        while (k < g->nh && !g->hist[k].first) k++;
        memmove(g->hist, g->hist + k, (size_t)(g->nh - k) * sizeof g->hist[0]);
        g->nh -= k;
    }
    g->hist[g->nh].cell = (uint8_t)c, g->hist[g->nh].kind = g->kind[c], g->hist[g->nh].way = g->way[c];
    g->hist[g->nh++].first = (uint8_t)first;
    g->kind[c] = (uint8_t)kind;
    g->way[c] = (uint8_t)way;
}

int mr_set(mr_game *g, int c, int to)
{
    if (c < 0 || c >= g->n * g->n || to < 0 || to > 2) return 0;
    int k = g->kind[c];
    if (k == MR_PIVOT) {
        if (to == 2 || g->way[c] == to) return 0;
        change(g, c, MR_PIVOT, to, 1);
    } else if (k == MR_LOOSE) {
        if (to == 2) change(g, c, MR_EMPTY, 0, 1);
        else if (g->way[c] != to) change(g, c, MR_LOOSE, to, 1);
        else return 0;
    } else if (k == MR_EMPTY) {
        if (to == 2 || mr_loose_left(g) <= 0) return 0;
        change(g, c, MR_LOOSE, to, 1);
    } else {
        return 0;
    }
    g->moves++;
    return 1;
}

int mr_tap(mr_game *g, int c)
{
    if (c < 0 || c >= g->n * g->n) return 0;
    int k = g->kind[c];
    if (k == MR_PIVOT) return mr_set(g, c, g->way[c] ^ 1);
    if (k == MR_LOOSE) return mr_set(g, c, g->way[c] == MR_SLASH ? MR_BACK : 2);
    if (k == MR_EMPTY) return mr_set(g, c, MR_SLASH);
    return 0;
}

int mr_move(mr_game *g, int from, int to)
{
    int cells = g->n * g->n;
    if (from < 0 || to < 0 || from >= cells || to >= cells || from == to) return 0;
    if (g->kind[from] != MR_LOOSE || g->kind[to] != MR_EMPTY) return 0;
    int w = g->way[from];
    change(g, from, MR_EMPTY, 0, 1);
    change(g, to, MR_LOOSE, w, 0);
    g->moves++;
    return 1;
}

int mr_hint(mr_game *g)
{
    if (mr_solved(g)) return -1;
    for (int i = 0; i < g->norder; i++) {      /* the first cell on the solution's path not as it should be */
        int c = g->order[i], want = g->sol[c], k = g->kind[c], have = is_mirror(k) ? g->way[c] : 2;
        if (want < 0 || have == want) continue;
        if (k == MR_PIVOT || k == MR_LOOSE || want == 2) return mr_set(g, c, want) ? c : -1;
        if (mr_loose_left(g) > 0) return mr_set(g, c, want) ? c : -1;
        for (int a = 0; a < g->n * g->n; a++)  /* none left: one standing where none should be comes here */
            if (g->kind[a] == MR_LOOSE && g->sol[a] != MR_SLASH && g->sol[a] != MR_BACK) {
                int w = g->way[a];
                change(g, a, MR_EMPTY, 0, 1);
                change(g, c, MR_LOOSE, w, 0);
                if (w != want) change(g, c, MR_LOOSE, want, 0);
                g->moves++;
                return c;
            }
        return -1;
    }
    /* the path is right: a loose mirror standing where the solution has none is in the way (on the beam first) */
    mr_beam b;
    mr_trace(g, &b);
    for (int pass = 0; pass < 2; pass++)
        for (int a = 0; a < g->n * g->n; a++)
            if (g->kind[a] == MR_LOOSE && g->sol[a] != MR_SLASH && g->sol[a] != MR_BACK && (pass || b.side[a]))
                return mr_set(g, a, 2) ? a : -1;
    return -1;
}

int mr_undo(mr_game *g)
{
    if (!g->nh) return -1;
    int c = -1;
    for (;;) {
        mr_change h = g->hist[--g->nh];
        g->kind[h.cell] = h.kind;
        g->way[h.cell] = h.way;
        c = h.cell;
        if (h.first || !g->nh) break;
    }
    if (g->moves > 0) g->moves--;
    return c;
}

int mr_cell(const mr_game *g, const char *s)
{
    if (!s || s[0] < 'a' || s[0] >= 'a' + g->n || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0';
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0';
    if (y < 1 || y > g->n) return -1;
    return s[0] - 'a' + (y - 1) * g->n;
}

static const char *cell_end(const char *s) { return s + (s[2] >= '0' && s[2] <= '9' ? 3 : 2); }

int mr_script(mr_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != ' ') e++;
        int ok = 0;
        if (*s == 'h' && e == s + 1) {
            ok = mr_hint(g) >= 0;
        } else if (*s == '=') {
            int k = e > s + 1 ? 0 : 999;
            for (const char *p = s + 1; p < e; p++) k = k * 10 + (*p - '0');
            ok = 1;
            for (int i = 0; i < k && !mr_solved(g); i++)
                if (mr_hint(g) < 0) { ok = 0; break; }
        } else {
            int c = mr_cell(g, s);
            if (c < 0) return 0;
            const char *p = cell_end(s);
            if (p == e) ok = mr_tap(g, c);
            else if (*p == 's' && p + 1 == e) ok = mr_set(g, c, MR_SLASH);
            else if (*p == 'b' && p + 1 == e) ok = mr_set(g, c, MR_BACK);
            else if (*p == 'e' && p + 1 == e) ok = mr_set(g, c, 2);
            else if (*p == '>') ok = mr_move(g, c, mr_cell(g, p + 1));
        }
        if (!ok) return 0;
        s = e;
    }
    return 1;
}
