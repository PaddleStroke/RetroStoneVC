/*
 * Same Game: the rules (samegame.h): the groups, the removals (the blocks falling, the columns closing up), the score,
 * undo, the boards made backwards so that each can be cleared, the search for a way to clear, the hint. No SDL: tested
 * by tests/samegame_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/samegame.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int SIZES[SM_VARIANTS][3] = {{10, 10, 3}, {15, 10, 4}, {20, 12, 5}};

int sm_points(int n) { return n > 2 ? (n - 2) * (n - 2) : 0; }

int sm_group(const sm_board *b, int w, int h, int x, int y, uint8_t mask[SM_W][SM_H])
{
    if (x < 0 || y < 0 || x >= w || y >= h || !b->c[x][y]) return 0;
    uint8_t seen[SM_W][SM_H] = {{0}};
    int stack[SM_W * SM_H], n = 0, size = 0, col = b->c[x][y];
    stack[n++] = x * SM_H + y;
    seen[x][y] = 1;
    while (n) {
        int p = stack[--n], px = p / SM_H, py = p % SM_H;
        size++;
        if (mask) mask[px][py] = 1;
        static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++) {
            int qx = px + d[k][0], qy = py + d[k][1];
            if (qx < 0 || qy < 0 || qx >= w || qy >= h || seen[qx][qy] || b->c[qx][qy] != col) continue;
            seen[qx][qy] = 1;
            stack[n++] = qx * SM_H + qy;
        }
    }
    return size;
}

/* the blocks in mask taken out: the ones above fall, the empty columns close up to the left */
static void take(sm_board *b, int w, int h, uint8_t mask[SM_W][SM_H])
{
    int nx = 0;
    for (int x = 0; x < w; x++) {
        int8_t col[SM_H];
        int n = 0;
        for (int y = 0; y < h; y++)
            if (b->c[x][y] && !mask[x][y]) col[n++] = b->c[x][y];
        if (!n) continue;
        memset(b->c[nx], 0, SM_H);
        memcpy(b->c[nx], col, (size_t)n);
        nx++;
    }
    for (int x = nx; x < w; x++) memset(b->c[x], 0, SM_H);
}

static int board_empty(const sm_board *b) { return !b->c[0][0]; }

int sm_remove(sm_game *g, int x, int y)
{
    uint8_t mask[SM_W][SM_H] = {{0}};
    int n = sm_group(&g->b, g->w, g->h, x, y, mask);
    if (n < 2 || g->nh >= SM_HIST) return 0;
    g->hist[g->nh].b = g->b;
    g->hist[g->nh++].score = g->score;
    take(&g->b, g->w, g->h, mask);
    g->score += sm_points(n);
    if (board_empty(&g->b)) g->score += SM_BONUS;
    g->removed++;
    return n;
}

int sm_undo(sm_game *g)
{
    if (!g->nh) return 0;
    g->nh--;
    g->b = g->hist[g->nh].b;
    g->score = g->hist[g->nh].score;
    g->removed--;
    return 1;
}

static int any_group(const sm_board *b, int w, int h)
{
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h && b->c[x][y]; y++)
            if ((x + 1 < w && b->c[x + 1][y] == b->c[x][y]) || (y + 1 < h && b->c[x][y + 1] == b->c[x][y])) return 1;
    return 0;
}

int sm_over(const sm_game *g) { return !any_group(&g->b, g->w, g->h); }
int sm_cleared(const sm_game *g) { return board_empty(&g->b); }

int sm_blocks(const sm_game *g)
{
    int n = 0;
    for (int x = 0; x < g->w; x++)
        for (int y = 0; y < g->h; y++) n += g->b.c[x][y] != 0;
    return n;
}

/* ---- the board made backwards ------------------------------------------------------------------------------------ */
typedef struct { sm_board b; int hgt[SM_W], ncols; } gen_state;

static void open_column(gen_state *s, int p)    /* an empty column at p, the others to the right */
{
    for (int x = s->ncols; x > p; x--) {
        s->hgt[x] = s->hgt[x - 1];
        memcpy(s->b.c[x], s->b.c[x - 1], SM_H);
    }
    memset(s->b.c[p], 0, SM_H);
    s->hgt[p] = 0;
    s->ncols++;
}

static void put(gen_state *s, int x, int y, int k, int col)   /* k blocks at row y of column x, the others up */
{
    memmove(&s->b.c[x][y + k], &s->b.c[x][y], (size_t)(s->hgt[x] - y));
    for (int i = 0; i < k; i++) s->b.c[x][y + i] = (int8_t)col;
    s->hgt[x] += k;
}

enum { GEN_UP, GEN_ACROSS, GEN_ACROSS3, GEN_L, GEN_NEWCOL, GEN_NEWCOLS, GEN_KINDS };

/* one group put back at random (kind, place, colour); 1: done, its place in (*gx, *gy) */
static int put_group(gen_state *s, int w, int h, int colours, const int *used, uint32_t *r, int *gx, int *gy)
{
    gen_state t = *s;
    int kind = pr_below(r, GEN_KINDS), k = 2 + pr_below(r, 2), col = 0;
    int cells[3][2], nc = 0;
    if (kind == GEN_NEWCOL || kind == GEN_NEWCOLS) {
        if (t.ncols + (kind == GEN_NEWCOLS ? 2 : 1) > w || (kind == GEN_NEWCOL && k > h)) return 0;
        int p = pr_below(r, t.ncols + 1);
        open_column(&t, p);
        if (kind == GEN_NEWCOLS) {
            open_column(&t, p);
            put(&t, p, 0, 1, 1), put(&t, p + 1, 0, 1, 1);
            cells[0][0] = p, cells[0][1] = 0, cells[1][0] = p + 1, cells[1][1] = 0, nc = 2;
        } else {
            put(&t, p, 0, k, 1);
            for (int i = 0; i < k; i++) cells[i][0] = p, cells[i][1] = i;
            nc = k;
        }
    } else {
        if (!t.ncols) return 0;
        int x = pr_below(r, t.ncols), span = kind == GEN_ACROSS ? 2 : kind == GEN_ACROSS3 ? 3 : kind == GEN_L ? 2 : 1;
        if (x + span > t.ncols) return 0;
        int top = h;                            /* the row: no higher than any column's top (no hole) */
        for (int i = 0; i < span; i++) top = t.hgt[x + i] < top ? t.hgt[x + i] : top;
        int y = pr_below(r, top + 1);
        if (kind == GEN_UP) {
            if (t.hgt[x] + k > h) return 0;
            put(&t, x, y, k, 1);
            for (int i = 0; i < k; i++) cells[i][0] = x, cells[i][1] = y + i;
            nc = k;
        } else if (kind == GEN_L) {             /* two upright in one column, one beside the lower */
            int tall = pr_below(r, 2);
            if (t.hgt[x + tall] + 2 > h || t.hgt[x + !tall] + 1 > h) return 0;
            put(&t, x + tall, y, 2, 1), put(&t, x + !tall, y, 1, 1);
            cells[0][0] = x + tall, cells[0][1] = y, cells[1][0] = x + tall, cells[1][1] = y + 1;
            cells[2][0] = x + !tall, cells[2][1] = y, nc = 3;
        } else {
            for (int i = 0; i < span; i++)
                if (t.hgt[x + i] + 1 > h) return 0;
            for (int i = 0; i < span; i++) {
                put(&t, x + i, y, 1, 1);
                cells[i][0] = x + i, cells[i][1] = y;
            }
            nc = span;
        }
    }
    /* its colour: none of its neighbours of that colour (else its removal would take more); the least used often */
    int ok[8] = {0}, nok = 0, least = -1;
    for (int c = 1; c <= colours; c++) {
        int fine = 1;
        for (int i = 0; i < nc && fine; i++) {
            static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int j = 0; j < 4 && fine; j++) {
                int qx = cells[i][0] + d[j][0], qy = cells[i][1] + d[j][1], mine = 0;
                if (qx < 0 || qy < 0 || qx >= w || qy >= h) continue;
                for (int m = 0; m < nc; m++) mine |= cells[m][0] == qx && cells[m][1] == qy;
                if (!mine && t.b.c[qx][qy] == c) fine = 0;
            }
        }
        if (!fine) continue;
        ok[nok++] = c;
        if (least < 0 || used[c] < used[least]) least = c;
    }
    if (!nok) return 0;
    col = pr_below(r, 3) ? least : ok[pr_below(r, nok)];
    for (int i = 0; i < nc; i++) t.b.c[cells[i][0]][cells[i][1]] = (int8_t)col;
    *s = t;
    *gx = cells[0][0];
    *gy = cells[0][1];
    return col;
}

/* a full board, backwards from the empty one; 0: stuck (a place left that no group fits) */
static int make_board(sm_game *g, uint32_t *r)
{
    gen_state s;
    memset(&s, 0, sizeof s);
    int used[8] = {0}, need = g->w * g->h, have = 0, n = 0;
    uint8_t back[SM_HIST][2];
    while (have < need) {
        int gx = 0, gy = 0, col = 0;
        for (int tries = 0; tries < 400 && !col; tries++) col = put_group(&s, g->w, g->h, g->colours, used, r, &gx, &gy);
        if (!col || n >= SM_HIST) return 0;
        int size = 0;
        for (int x = 0; x < s.ncols; x++) size += s.hgt[x];
        used[col] += size - have;
        have = size;
        back[n][0] = (uint8_t)gx;
        back[n][1] = (uint8_t)gy;
        n++;
    }
    if (s.ncols != g->w) return 0;
    g->b = g->start = s.b;
    g->nway = n;                                /* the way: the groups taken in the reverse order */
    for (int i = 0; i < n; i++) memcpy(g->way[i], back[n - 1 - i], 2);
    return 1;
}

void sm_new(sm_game *g, int variant, uint32_t seed)
{
    int v = variant < 0 || variant >= SM_VARIANTS ? 0 : variant;
    memset(g, 0, sizeof *g);
    g->w = SIZES[v][0];
    g->h = SIZES[v][1];
    g->colours = SIZES[v][2];
    uint32_t r = pr_seed(seed);
    while (!make_board(g, &r)) {}
}

/* ---- the search for a way to clear ----------------------------------------------------------------------------- */
typedef struct { uint64_t *seen; int mask, budget; } search;

static uint64_t board_key(const sm_board *b, int w, int h)
{
    uint64_t k = 1469598103934665603ull;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) k = (k ^ (uint64_t)(b->c[x][y] + 1)) * 1099511628211ull;
    return k ? k : 1;
}

static int clear_dfs(const sm_board *b, int w, int h, search *s, int *fx, int *fy)
{
    if (board_empty(b)) return 1;
    if (s->budget-- <= 0) return 0;
    int count[8] = {0};
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) count[b->c[x][y]]++;
    for (int c = 1; c < 8; c++)
        if (count[c] == 1) return 0;            /* a lone block of a colour: it can never go */
    uint64_t k = board_key(b, w, h);
    int i = (int)(k & (uint64_t)s->mask);
    if (s->seen[i] == k) return 0;
    s->seen[i] = k;
    /* the groups, the colours with the fewest blocks first, then the larger groups */
    uint8_t done[SM_W][SM_H] = {{0}};
    int gx[SM_W * SM_H / 2], gy[SM_W * SM_H / 2], gs[SM_W * SM_H / 2], ng = 0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h && b->c[x][y]; y++) {
            if (done[x][y]) continue;
            int n = sm_group(b, w, h, x, y, done);
            if (n < 2) continue;
            int key = count[b->c[x][y]] * 256 - n, j = ng++;
            while (j > 0 && gs[j - 1] > key) { gx[j] = gx[j - 1], gy[j] = gy[j - 1], gs[j] = gs[j - 1]; j--; }
            gx[j] = x, gy[j] = y, gs[j] = key;
        }
    for (int j = 0; j < ng && s->budget > 0; j++) {
        sm_board nb = *b;
        uint8_t m[SM_W][SM_H] = {{0}};
        sm_group(&nb, w, h, gx[j], gy[j], m);
        take(&nb, w, h, m);
        int ax, ay;
        if (clear_dfs(&nb, w, h, s, &ax, &ay)) {
            *fx = gx[j];
            *fy = gy[j];
            return 1;
        }
    }
    return 0;
}

int sm_solve(const sm_board *b, int w, int h, int cap, int *x, int *y)
{
    search s = {calloc(1 << 16, sizeof(uint64_t)), (1 << 16) - 1, cap};
    if (!s.seen) return 0;
    int ok = clear_dfs(b, w, h, &s, x, y);
    free(s.seen);
    return ok && !board_empty(b);
}

/* the kept way: the step the board is at on it (-1: off it) */
static int on_way(const sm_game *g)
{
    sm_board b = g->start;
    for (int k = 0; k <= g->nway; k++) {
        if (!memcmp(&b, &g->b, sizeof b)) return k;
        if (k == g->nway) break;
        uint8_t m[SM_W][SM_H] = {{0}};
        if (sm_group(&b, g->w, g->h, g->way[k][0], g->way[k][1], m) < 2) break;
        take(&b, g->w, g->h, m);
    }
    return -1;
}

int sm_hint(const sm_game *g, int *x, int *y)
{
    if (sm_over(g)) return 0;
    int k = on_way(g);
    if (k >= 0 && k < g->nway) {
        *x = g->way[k][0];
        *y = g->way[k][1];
        return 2;
    }
    if (sm_solve(&g->b, g->w, g->h, 6000, x, y)) return 2;
    int best = 0;                               /* no way found: the group that scores most */
    uint8_t done[SM_W][SM_H] = {{0}};
    for (int a = 0; a < g->w; a++)
        for (int c = 0; c < g->h && g->b.c[a][c]; c++) {
            if (done[a][c]) continue;
            int n = sm_group(&g->b, g->w, g->h, a, c, done);
            if (n >= 2 && n > best) best = n, *x = a, *y = c;
        }
    return best ? 1 : 0;
}

int sm_script(sm_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '=') {                       /* "=": the rest of the way */
            int k = 0, all = s[1] < '0' || s[1] > '9';
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            if (all) k = on_way(g) < 0 ? 1 : g->nway - on_way(g);
            for (int i = 0; i < k; i++) {
                int at = on_way(g);
                if (at < 0 || at >= g->nway || !sm_remove(g, g->way[at][0], g->way[at][1])) return 0;
            }
            continue;
        }
        if (*s == '~') {                        /* a careless game to its end: the first group from the bottom left */
            s++;
            for (int found = 1; found;) {
                found = 0;
                for (int x = 0; x < g->w && !found; x++)
                    for (int y = 0; y < g->h && !found; y++) found = sm_remove(g, x, y) > 0;
            }
            continue;
        }
        if (*s < 'a' || *s >= 'a' + g->w) return 0;
        int x = *s++ - 'a', y = 0;
        if (*s < '1' || *s > '9') return 0;
        while (*s >= '0' && *s <= '9') y = y * 10 + *s++ - '0';
        if (!sm_remove(g, x, y - 1)) return 0;
    }
    return 1;
}
