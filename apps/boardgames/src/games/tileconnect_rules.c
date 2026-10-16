/*
 * Tile Connect's rules (games/tileconnect.h): the line of at most two turns, the falls, the plan that makes every
 * board clear, the search behind the hint, the shuffle, undo and the notation. No SDL: the tests link only this.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/tileconnect.h"

static const char TCN_FACE_CH[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP";
static const char TCN_STEP_CH[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static int is_tile(uint8_t f) { return f < TCN_FACES; }

void tcn_size(int variant, int *w, int *h)
{
    static const int S[TCN_VARIANTS][2] = {{6, 4}, {8, 6}, {10, 7}, {12, 8}};
    int v = variant < 0 || variant >= TCN_VARIANTS ? 0 : variant;
    *w = S[v][0];
    *h = S[v][1];
}

/* ---- the line ----------------------------------------------------------------------------------------------------- */
/* (x, y) open for the line: outside the rectangle (one cell beyond it), an empty cell, or one of the two ends */
static int open_at(const tcn_board *b, int x, int y, int a, int c)
{
    if (x < 0 || y < 0 || x >= b->w || y >= b->h) return x >= -1 && y >= -1 && x <= b->w && y <= b->h;
    int i = x + y * b->w;
    return i == a || i == c || b->face[i] == TCN_EMPTY;
}

/* the points a straight stroke reaches from (x, y) along one axis (vertical: the rows, else the columns), the start
 * itself included: r[k + 1] for k -1..n */
static void rays(const tcn_board *b, int x, int y, int vertical, int a, int c, uint8_t *r)
{
    int n = vertical ? b->h : b->w, p = vertical ? y : x;
    memset(r, 0, (size_t)n + 2);
    r[p + 1] = 1;
    for (int k = p - 1; k >= -1 && open_at(b, vertical ? x : k, vertical ? k : y, a, c); k--) r[k + 1] = 1;
    for (int k = p + 1; k <= n && open_at(b, vertical ? x : k, vertical ? k : y, a, c); k++) r[k + 1] = 1;
}

/* the cells strictly between (x0, y0) and (x1, y1) on one row or one column open */
static int clear_between(const tcn_board *b, int x0, int y0, int x1, int y1, int a, int c)
{
    int dx = x1 > x0 ? 1 : x1 < x0 ? -1 : 0, dy = y1 > y0 ? 1 : y1 < y0 ? -1 : 0;
    for (int x = x0 + dx, y = y0 + dy; x != x1 || y != y1; x += dx, y += dy)
        if (!open_at(b, x, y, a, c)) return 0;
    return 1;
}

static int iabs(int v) { return v < 0 ? -v : v; }

int tcn_connect(const tcn_board *b, int a, int c, tcn_pt *path, int *np)
{
    int n = b->w * b->h;
    if (a < 0 || c < 0 || a >= n || c >= n || a == c) return 0;
    int ax = a % b->w, ay = a / b->w, cx = c % b->w, cy = c / b->w;
    uint8_t va[TCN_MAXH + 2], vc[TCN_MAXH + 2], ha[TCN_MAXW + 2], hc[TCN_MAXW + 2];
    rays(b, ax, ay, 1, a, c, va);
    rays(b, cx, cy, 1, a, c, vc);
    rays(b, ax, ay, 0, a, c, ha);
    rays(b, cx, cy, 0, a, c, hc);
    int best = 1 << 20, bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    for (int y = -1; y <= b->h; y++)            /* down or up to a row, along it, then up or down to c */
        if (va[y + 1] && vc[y + 1] && clear_between(b, ax, y, cx, y, a, c)) {
            int len = 4 * (iabs(ay - y) + iabs(ax - cx) + iabs(cy - y)) + (y != ay) + (y != cy);
            if (len < best) best = len, bx0 = ax, by0 = y, bx1 = cx, by1 = y;
        }
    for (int x = -1; x <= b->w; x++)            /* along to a column, along it, then along to c */
        if (ha[x + 1] && hc[x + 1] && clear_between(b, x, ay, x, cy, a, c)) {
            int len = 4 * (iabs(ax - x) + iabs(ay - cy) + iabs(cx - x)) + (x != ax) + (x != cx);
            if (len < best) best = len, bx0 = x, by0 = ay, bx1 = x, by1 = cy;
        }
    if (best == 1 << 20) return 0;
    if (path) {
        tcn_pt p[4] = {{(int8_t)ax, (int8_t)ay}, {(int8_t)bx0, (int8_t)by0}, {(int8_t)bx1, (int8_t)by1},
                       {(int8_t)cx, (int8_t)cy}};
        int k = 0;
        for (int i = 0; i < 4; i++) {           /* the points where it turns: the repeated and the straight ones out */
            if (k && p[i].x == path[k - 1].x && p[i].y == path[k - 1].y) continue;
            if (k >= 2) {
                tcn_pt u = path[k - 2], v = path[k - 1];
                if ((u.x == v.x && v.x == p[i].x) || (u.y == v.y && v.y == p[i].y)) k--;
            }
            path[k++] = p[i];
        }
        if (np) *np = k;
    }
    return 1;
}

int tcn_can_take(const tcn_board *b, int a, int c)
{
    int n = b->w * b->h;
    if (a < 0 || c < 0 || a >= n || c >= n || a == c) return 0;
    if (!is_tile(b->face[a]) || b->face[a] != b->face[c]) return 0;
    return tcn_connect(b, a, c, NULL, NULL);
}

/* ---- the falls ---------------------------------------------------------------------------------------------------- */
/* a line of cells, from the end the tiles fall towards: each stretch between blocks packed towards that end */
static void pack(tcn_board *b, const int *line, int n, uint8_t *from)
{
    for (int i = 0; i < n;) {
        if (b->face[line[i]] == TCN_BLOCK) { i++; continue; }
        int put = i, j = i;
        for (; j < n && b->face[line[j]] != TCN_BLOCK; j++) {
            int c = line[j];
            if (b->face[c] == TCN_EMPTY) continue;
            if (put != j) {
                int d = line[put];
                b->face[d] = b->face[c];
                b->id[d] = b->id[c];
                b->face[c] = TCN_EMPTY;
                b->id[c] = TCN_EMPTY;
                if (from) {
                    from[d] = from[c];
                    from[c] = TCN_EMPTY;
                }
            }
            put++;
        }
        i = j;
    }
}

void tcn_fall(tcn_board *b, uint8_t *from)
{
    int w = b->w, h = b->h, line[TCN_MAXW];
    if (from)
        for (int c = 0; c < w * h; c++) from[c] = is_tile(b->face[c]) ? (uint8_t)c : TCN_EMPTY;
    if (b->fall == TCN_DOWN)
        for (int x = 0; x < w; x++) {
            for (int k = 0; k < h; k++) line[k] = x + (h - 1 - k) * w;
            pack(b, line, h, from);
        }
    else if (b->fall == TCN_LEFT)
        for (int y = 0; y < h; y++) {
            for (int k = 0; k < w; k++) line[k] = k + y * w;
            pack(b, line, w, from);
        }
    else if (b->fall == TCN_CENTRE)
        for (int y = 0; y < h; y++) {
            int m = w / 2;
            for (int k = 0; k < m; k++) line[k] = m - 1 - k + y * w;
            pack(b, line, m, from);
            for (int k = 0; k < w - m; k++) line[k] = m + k + y * w;
            pack(b, line, w - m, from);
        }
}

static void take_tiles(tcn_board *b, int a, int c, uint8_t *from)
{
    b->face[a] = b->face[c] = TCN_EMPTY;
    b->id[a] = b->id[c] = TCN_EMPTY;
    tcn_fall(b, from);
}

/* ---- the pairs ---------------------------------------------------------------------------------------------------- */
/* the tiles of each face: cells[f][0..cnt[f]-1] */
static void by_face(const tcn_board *b, uint8_t cnt[TCN_FACES], uint8_t cells[TCN_FACES][8])
{
    memset(cnt, 0, TCN_FACES);
    for (int c = 0; c < b->w * b->h; c++)
        if (is_tile(b->face[c]) && cnt[b->face[c]] < 8) cells[b->face[c]][cnt[b->face[c]]++] = (uint8_t)c;
}

int tcn_find_pair(const tcn_board *b, int *a, int *c)
{
    uint8_t cnt[TCN_FACES], cells[TCN_FACES][8];
    by_face(b, cnt, cells);
    for (int f = 0; f < TCN_FACES; f++)
        for (int i = 0; i < cnt[f]; i++)
            for (int j = i + 1; j < cnt[f]; j++)
                if (tcn_connect(b, cells[f][i], cells[f][j], NULL, NULL)) {
                    if (a) *a = cells[f][i];
                    if (c) *c = cells[f][j];
                    return 1;
                }
    return 0;
}

int tcn_count_pairs(const tcn_board *b)
{
    uint8_t cnt[TCN_FACES], cells[TCN_FACES][8];
    by_face(b, cnt, cells);
    int n = 0;
    for (int f = 0; f < TCN_FACES; f++)
        for (int i = 0; i < cnt[f]; i++)
            for (int j = i + 1; j < cnt[f]; j++) n += tcn_connect(b, cells[f][i], cells[f][j], NULL, NULL);
    return n;
}

/* ---- the search: depth first, the positions that cannot be cleared kept (by a hash of their faces) ------------- */
#define TT_BITS 16
static uint64_t tt[1 << TT_BITS];
static int tt_n;
static long sv_nodes, sv_budget;

static uint64_t board_hash(const tcn_board *b)
{
    uint64_t h = 1469598103934665603ull;
    for (int c = 0; c < b->w * b->h; c++) h = (h ^ b->face[c]) * 1099511628211ull;
    return h ? h : 1;
}

static int tt_has(uint64_t h)
{
    for (uint32_t i = (uint32_t)h & ((1u << TT_BITS) - 1);; i = (i + 1) & ((1u << TT_BITS) - 1)) {
        if (!tt[i]) return 0;
        if (tt[i] == h) return 1;
    }
}

static void tt_add(uint64_t h)
{
    if (tt_n >= (3 << TT_BITS) / 4) return;     /* full enough: no more kept */
    uint32_t i = (uint32_t)h & ((1u << TT_BITS) - 1);
    while (tt[i] && tt[i] != h) i = (i + 1) & ((1u << TT_BITS) - 1);
    if (!tt[i]) tt_n++;
    tt[i] = h;
}

static int tiles_on(const tcn_board *b)
{
    int n = 0;
    for (int c = 0; c < b->w * b->h; c++) n += is_tile(b->face[c]);
    return n;
}

static int dfs(const tcn_board *b, int tiles, int *fa, int *fc)
{
    if (!tiles) return 1;
    if (++sv_nodes > sv_budget) return -1;
    uint64_t h = board_hash(b);
    if (tt_has(h)) return 0;
    uint8_t cnt[TCN_FACES], cells[TCN_FACES][8];
    by_face(b, cnt, cells);
    int pa[256], pc[256], n = 0, safe = -1;
    for (int f = 0; f < TCN_FACES && safe < 0; f++)
        for (int i = 0; i < cnt[f] && safe < 0; i++)
            for (int j = i + 1; j < cnt[f] && n < 256; j++) {
                if (!tcn_connect(b, cells[f][i], cells[f][j], NULL, NULL)) continue;
                pa[n] = cells[f][i];
                pc[n] = cells[f][j];
                /* still tiles: a face down to its last two is always right to take (taking tiles only opens ways) */
                if (cnt[f] == 2 && b->fall == TCN_STILL) { safe = n; n++; break; }
                n++;
            }
    int from = safe >= 0 ? safe : 0, to = safe >= 0 ? safe + 1 : n;
    for (int k = from; k < to; k++) {
        tcn_board t = *b;
        take_tiles(&t, pa[k], pc[k], NULL);
        int r = dfs(&t, tiles - 2, NULL, NULL);
        if (r < 0) return -1;
        if (r) {
            if (fa) *fa = pa[k];
            if (fc) *fc = pc[k];
            return 1;
        }
    }
    tt_add(h);
    return 0;
}

int tcn_solve(const tcn_board *b, long budget, int *a, int *c, long *nodes)
{
    memset(tt, 0, sizeof tt);
    tt_n = 0;
    sv_nodes = 0;
    sv_budget = budget;
    int r = dfs(b, tiles_on(b), a, c);
    if (nodes) *nodes = sv_nodes;
    return r;
}

/* ---- the plan: the tiles played out by chance, their faces left aside ------------------------------------------- */
int tcn_plan(const tcn_board *b0, uint32_t seed, uint8_t *step)
{
    tcn_board b = *b0;
    uint32_t s = pr_seed(seed);
    int k = 0;
    for (;;) {
        int cells[TCN_CELLS], n = 0;
        for (int c = 0; c < b.w * b.h; c++)
            if (is_tile(b.face[c])) cells[n++] = c;
        if (!n) return 1;
        pr_shuffle(&s, cells, n);
        int a = -1, c = -1;
        for (int i = 0; i < n && a < 0; i++) {   /* a tile at random, then one a line joins it to, at random */
            int cand[TCN_CELLS], m = 0;
            for (int j = 0; j < n; j++)
                if (j != i && tcn_connect(&b, cells[i], cells[j], NULL, NULL)) cand[m++] = cells[j];
            if (m) {
                a = cells[i];
                c = cand[pr_below(&s, m)];
            }
        }
        if (a < 0) return 0;
        step[b.id[a]] = step[b.id[c]] = (uint8_t)k++;
        take_tiles(&b, a, c, NULL);
    }
}

/* ---- a game ------------------------------------------------------------------------------------------------------- */
static void begin(tcn_game *g, uint32_t seed)
{
    g->plan = 0;
    g->tiles = tiles_on(&g->b);
    g->pairs = g->tiles / 2;
    g->hints = g->shuffles = 0;
    g->rng = pr_seed(seed ^ 0x5bd1e995u);
    g->nh = 0;
}

int tcn_make(tcn_game *g, int w, int h, int fall, const uint8_t *blocks, int faces, uint32_t seed)
{
    if (w < 2 || h < 1 || w > TCN_MAXW || h > TCN_MAXH || fall < 0 || fall >= TCN_FALLS) return 0;
    tcn_board b;
    memset(&b, 0, sizeof b);
    b.w = w;
    b.h = h;
    b.fall = fall;
    int n = 0;
    for (int c = 0; c < TCN_CELLS; c++) {
        int on = c < w * h && !(blocks && blocks[c]);
        b.face[c] = c >= w * h ? TCN_EMPTY : on ? 0 : TCN_BLOCK;
        b.id[c] = on ? (uint8_t)c : TCN_EMPTY;
        n += on;
    }
    if (n < 2 || n % 2) return 0;
    int pairs = n / 2;
    if (faces > TCN_FACES) faces = TCN_FACES;
    if (faces > pairs) faces = pairs;
    if (faces < (pairs + 2) / 3) faces = (pairs + 2) / 3;
    uint32_t s = pr_seed(seed);
    uint8_t step[TCN_CELLS];
    int ok = 0;
    for (int t = 0; t < 64 && !ok; t++) ok = tcn_plan(&b, pr_next(&s), step);
    if (!ok) return 0;
    int pick[TCN_FACES], pf[TCN_CELLS / 2], used[TCN_FACES] = {0};
    for (int f = 0; f < TCN_FACES; f++) pick[f] = f;
    pr_shuffle(&s, pick, TCN_FACES);
    for (int i = 0; i < pairs; i++) {           /* each face once, then the pairs left to faces at random (three at most) */
        int f = i < faces ? pick[i] : pick[pr_below(&s, faces)];
        while (used[f] >= 3) f = pick[pr_below(&s, faces)];
        used[f]++;
        pf[i] = f;
    }
    pr_shuffle(&s, pf, pairs);
    for (int c = 0; c < w * h; c++)
        if (b.face[c] == 0 && b.id[c] != TCN_EMPTY) b.face[c] = (uint8_t)pf[step[b.id[c]]];
    memset(g, 0, sizeof *g);
    g->b = b;
    memcpy(g->step, step, sizeof step);
    begin(g, seed);
    return 1;
}

void tcn_new(tcn_game *g, int variant, uint32_t seed)
{
    int w, h;
    tcn_size(variant, &w, &h);
    for (uint32_t k = 0; !tcn_make(g, w, h, TCN_STILL, NULL, (w * h / 2 + 1) / 2, seed + k); k++) {}
}

int tcn_load(tcn_game *g, int w, int h, int fall, const char *faces, const char *order)
{
    if (w < 2 || h < 1 || w > TCN_MAXW || h > TCN_MAXH || fall < 0 || fall >= TCN_FALLS) return 0;
    if (!faces || !order || (int)strlen(faces) != w * h || (int)strlen(order) != w * h) return 0;
    tcn_game t;
    memset(&t, 0, sizeof t);
    t.b.w = w;
    t.b.h = h;
    t.b.fall = fall;
    memset(t.b.face, TCN_EMPTY, sizeof t.b.face);
    memset(t.b.id, TCN_EMPTY, sizeof t.b.id);
    memset(t.step, TCN_EMPTY, sizeof t.step);
    int seen[TCN_CELLS / 2 + 1] = {0}, face_of[TCN_CELLS / 2 + 1], n = 0;
    for (int c = 0; c < w * h; c++) {
        if (faces[c] == '#' || order[c] == '#') {
            if (faces[c] != order[c]) return 0;
            t.b.face[c] = TCN_BLOCK;
            continue;
        }
        const char *f = strchr(TCN_FACE_CH, faces[c]), *s = strchr(TCN_STEP_CH, order[c]);
        if (!f || !s || !*f || !*s) return 0;
        int k = (int)(s - TCN_STEP_CH);
        if (k >= TCN_CELLS / 2 || seen[k] >= 2) return 0;
        if (seen[k] && face_of[k] != (int)(f - TCN_FACE_CH)) return 0;   /* a step's two tiles: one face */
        face_of[k] = (int)(f - TCN_FACE_CH);
        seen[k]++;
        t.b.face[c] = (uint8_t)(f - TCN_FACE_CH);
        t.b.id[c] = (uint8_t)c;
        t.step[c] = (uint8_t)k;
        n++;
    }
    if (n < 2 || n % 2) return 0;
    for (int k = 0; k < n / 2; k++)
        if (seen[k] != 2) return 0;
    uint32_t seed = (uint32_t)w * 131u + (uint32_t)h * 7u + (uint32_t)fall;
    for (const char *p = faces; *p; p++) seed = seed * 33u + (uint8_t)*p;
    *g = t;
    begin(g, seed);
    return 1;
}

#ifndef TCN_GEN                                 /* not in the levels' tool (tools/tileconnect_gen.c): it makes them */
#include "games/tileconnect_levels.h"

int tcn_level(tcn_game *g, int level)
{
    if (level < 0 || level >= TCN_LEVELS) return 0;
    const tcn_lvl *l = &tcn_levels[level];
    return tcn_load(g, l->w, l->h, l->fall, l->faces, l->order);
}
#endif

static void push(tcn_game *g)
{
    if (g->nh == TCN_HIST) {
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (TCN_HIST - 1));
        g->nh--;
    }
    tcn_snap *s = &g->hist[g->nh++];
    memcpy(s->face, g->b.face, sizeof s->face);
    memcpy(s->id, g->b.id, sizeof s->id);
    memcpy(s->step, g->step, sizeof s->step);
    s->plan = g->plan;
}

int tcn_take(tcn_game *g, int a, int c, uint8_t *from)
{
    if (!tcn_can_take(&g->b, a, c)) return 0;
    push(g);
    if (g->plan >= 0 && g->step[g->b.id[a]] == g->plan && g->step[g->b.id[c]] == g->plan) g->plan++;
    else g->plan = -1;
    take_tiles(&g->b, a, c, from);
    g->tiles -= 2;
    return 1;
}

int tcn_undo(tcn_game *g)
{
    if (!g->nh) return 0;
    tcn_snap *s = &g->hist[--g->nh];
    memcpy(g->b.face, s->face, sizeof s->face);
    memcpy(g->b.id, s->id, sizeof s->id);
    memcpy(g->step, s->step, sizeof s->step);
    g->plan = s->plan;
    g->tiles = tiles_on(&g->b);
    return 1;
}

int tcn_won(const tcn_game *g) { return g->tiles == 0; }
int tcn_stuck(const tcn_game *g) { return g->tiles > 0 && !tcn_find_pair(&g->b, NULL, NULL); }

/* the plan's next pair, while it is followed */
static int plan_pair(const tcn_game *g, int *a, int *c)
{
    if (g->plan < 0) return 0;
    int k = 0, p[2] = {-1, -1};
    for (int i = 0; i < g->b.w * g->b.h && k < 2; i++)
        if (is_tile(g->b.face[i]) && g->step[g->b.id[i]] == g->plan) p[k++] = i;
    if (k < 2 || !tcn_can_take(&g->b, p[0], p[1])) return 0;
    *a = p[0];
    *c = p[1];
    return 1;
}

int tcn_hint(const tcn_game *g, int *a, int *c)
{
    if (!g->tiles) return TCN_HINT_NONE;
    if (plan_pair(g, a, c)) return TCN_HINT_PAIR;
    int r = tcn_solve(&g->b, 20000, a, c, NULL);
    if (r == 1) return TCN_HINT_PAIR;
    if (r < 0 && tcn_find_pair(&g->b, a, c)) return TCN_HINT_PAIR;   /* not known: a pair at least */
    return TCN_HINT_SHUFFLE;
}

int tcn_shuffle(tcn_game *g)
{
    if (!g->tiles) return 0;
    uint8_t step[TCN_CELLS];
    int ok = 0;
    for (int t = 0; t < 40 && !ok; t++) ok = tcn_plan(&g->b, pr_next(&g->rng), step);
    if (!ok) return 0;
    int faces[TCN_CELLS], n = 0, cnt[TCN_FACES] = {0};
    for (int c = 0; c < g->b.w * g->b.h; c++)
        if (is_tile(g->b.face[c])) cnt[g->b.face[c]]++;
    for (int f = 0; f < TCN_FACES; f++)         /* the faces left, a pair at a time */
        for (int k = 0; k + 1 < cnt[f]; k += 2) faces[n++] = f;
    pr_shuffle(&g->rng, faces, n);
    push(g);
    for (int c = 0; c < g->b.w * g->b.h; c++)
        if (is_tile(g->b.face[c])) {
            g->step[g->b.id[c]] = step[g->b.id[c]];
            g->b.face[c] = (uint8_t)faces[step[g->b.id[c]]];
        }
    g->plan = 0;
    g->shuffles++;
    return 1;
}

int tcn_cell(const char *s, int w, int h, int *n)
{
    if (s[0] < 'a' || s[0] >= 'a' + w || s[1] < '1' || s[1] > '9') return -1;
    int x = s[0] - 'a', y = 0, k = 1;
    while (s[k] >= '0' && s[k] <= '9') y = y * 10 + s[k++] - '0';
    if (y < 1 || y > h) return -1;
    if (n) *n = k;
    return x + (y - 1) * w;
}

/* the next pair of a way that clears the board: the plan's, else the search's */
static int way_pair(const tcn_game *g, int *a, int *c)
{
    if (plan_pair(g, a, c)) return 1;
    return tcn_solve(&g->b, 200000, a, c, NULL) == 1;
}

int tcn_script(tcn_game *g, const char *moves)
{
    const char *p = moves;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        int a, c, n;
        if (*p == 'h') {
            p++;
            if (tcn_hint(g, &a, &c) != TCN_HINT_PAIR || !tcn_take(g, a, c, NULL)) return 0;
            g->hints++;
        } else if (*p == 's') {
            p++;
            if (!tcn_shuffle(g)) return 0;
        } else if (*p == '=' || *p == '+') {
            char k = *p++;
            int times = 1;
            if (*p == '*') times = 1 << 20, p++;
            else if (*p >= '0' && *p <= '9') times = (int)strtol(p, (char **)&p, 10);
            for (int i = 0; i < times && g->tiles; i++) {
                int ok = k == '=' ? way_pair(g, &a, &c) : tcn_find_pair(&g->b, &a, &c);
                if (!ok || !tcn_take(g, a, c, NULL)) return 0;
            }
        } else {
            a = tcn_cell(p, g->b.w, g->b.h, &n);
            if (a < 0) return 0;
            p += n;
            if (*p++ != '-') return 0;
            c = tcn_cell(p, g->b.w, g->b.h, &n);
            if (c < 0) return 0;
            p += n;
            if (!tcn_take(g, a, c, NULL)) return 0;
        }
    }
    return 1;
}
