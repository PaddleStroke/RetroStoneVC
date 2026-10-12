/*
 * Klotski: the rules (klotski.h): the traditional starts, slides, moves as the tradition counts them, undo, and the
 * breadth-first search for the fewest moves (the par, the hint). No SDL: tested by tests/klotski_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/klotski.h"

#include <stdlib.h>
#include <string.h>

/* the starts: each block's kind (B the great block, T standing, W lying, S small) and its top left cell, row then
 * column; their names in pinyin, and the fewest moves the search finds */
static const struct { const char *name, *blocks; int par; } LEVEL[KLT_LEVELS] = {
    {"Heng Dao Li Ma", "B01 T00 T03 T20 T23 W21 S31 S32 S40 S43", 81},
    {"Jie Zu Xian Deng", "B01 T30 T31 T32 T33 W21 S00 S03 S10 S13", 32},
    {"Yi Lu Shun Feng", "B01 T00 T20 T23 T32 W21 S03 S13 S31 S41", 39},
    {"Yu Sheng Xi Li", "B01 T00 T20 T23 T31 W21 S03 S13 S40 S43", 47},
    {"Zuo You Bu Bing", "B01 T20 T21 T22 T23 W41 S00 S03 S10 S13", 54},
    {"Yi Lu Jin Jun", "B01 T00 T20 T21 T22 W41 S03 S13 S23 S33", 58},
    {"Qi Tou Bing Jin", "B01 T00 T03 T30 T33 W31 S20 S21 S22 S23", 60},
    {"Cha Chi Nan Fei", "B01 T00 T30 T33 W20 W31 S03 S13 S22 S23", 62},
    {"Zhi Hui Ruo Ding", "B01 T00 T03 T30 T33 W21 S20 S23 S31 S32", 70},
    {"Tao Hua Yuan Zhong", "B01 T10 T13 T21 T22 W41 S00 S03 S30 S33", 70},
    {"Bing Fen San Lu", "B01 T10 T13 T30 T33 W21 S00 S03 S31 S32", 72},
    {"Jiang Yong Cao Ying", "B01 T10 T13 T21 T22 W40 S30 S33 S42 S43", 72},
    {"Ceng Ceng She Fang", "B01 T00 T03 W21 W31 W41 S20 S23 S30 S33", 102},
    {"Feng Hui Lu Zhuan", "B10 T03 T12 T23 W31 W42 S00 S01 S02 S41", 138},
};

static const int8_t PW[4] = {2, 1, 2, 1}, PH[4] = {2, 2, 1, 1};   /* each kind's width and height */

const char *klt_name(int level) { return LEVEL[level < 0 || level >= KLT_LEVELS ? 0 : level].name; }
int klt_par(int level) { return LEVEL[level < 0 || level >= KLT_LEVELS ? 0 : level].par; }

void klt_new(klt_game *g, int level)
{
    if (level < 0 || level >= KLT_LEVELS) level = 0;
    memset(g, 0, sizeof *g);
    g->level = level;
    g->last = -1;
    const char *s = LEVEL[level].blocks;
    for (int i = 0; i < KLT_PIECES; i++, s += 4) {
        const char *k = strchr("BTWS", s[0]);
        g->p[i].type = (int8_t)(k - "BTWS");
        g->p[i].y = (int8_t)(s[1] - '0');
        g->p[i].x = (int8_t)(s[2] - '0');
    }
}

/* the board as cells: the block on each (-1 empty) */
static void cells(const klt_piece *p, int8_t *at)
{
    memset(at, -1, KLT_W * KLT_H);
    for (int i = 0; i < KLT_PIECES; i++)
        for (int dy = 0; dy < PH[p[i].type]; dy++)
            for (int dx = 0; dx < PW[p[i].type]; dx++) at[p[i].x + dx + (p[i].y + dy) * KLT_W] = (int8_t)i;
}

int klt_at(const klt_game *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= KLT_W || y >= KLT_H) return -1;
    int8_t at[KLT_W * KLT_H];
    cells(g->p, at);
    return at[x + y * KLT_W];
}

/* can block i stand at (x, y), the others where they are */
static int fits(const int8_t *at, const klt_piece *p, int i, int x, int y)
{
    int t = p[i].type;
    if (x < 0 || y < 0 || x + PW[t] > KLT_W || y + PH[t] > KLT_H) return 0;
    for (int dy = 0; dy < PH[t]; dy++)
        for (int dx = 0; dx < PW[t]; dx++) {
            int o = at[x + dx + (y + dy) * KLT_W];
            if (o >= 0 && o != i) return 0;
        }
    return 1;
}

int klt_can(const klt_game *g, int i, int dx, int dy)
{
    if (i < 0 || i >= KLT_PIECES || abs(dx) + abs(dy) != 1) return 0;
    int8_t at[KLT_W * KLT_H];
    cells(g->p, at);
    return fits(at, g->p, i, g->p[i].x + dx, g->p[i].y + dy);
}

static void remember(klt_game *g)
{
    if (g->nh >= KLT_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + KLT_HIST / 2, sizeof g->hist / 2);
        g->nh -= KLT_HIST / 2;
    }
    for (int i = 0; i < KLT_PIECES; i++) {
        g->hist[g->nh][i][0] = g->p[i].x;
        g->hist[g->nh][i][1] = g->p[i].y;
    }
    g->nh++;
}

int klt_step(klt_game *g, int i, int dx, int dy)
{
    if (!klt_can(g, i, dx, dy)) return 0;
    if (g->last != i) {                         /* another block: a new move */
        remember(g);
        g->moves++;
        g->last = i;
    }
    g->p[i].x = (int8_t)(g->p[i].x + dx);
    g->p[i].y = (int8_t)(g->p[i].y + dy);
    int back = 1;                               /* slid back where it began: no move at all */
    for (int k = 0; k < KLT_PIECES; k++)
        if (g->p[k].x != g->hist[g->nh - 1][k][0] || g->p[k].y != g->hist[g->nh - 1][k][1]) back = 0;
    if (back) {
        g->nh--;
        g->moves--;
        g->last = -1;
    }
    return 1;
}

/* the block's places one move away, by a search of its slides; from: the place each was reached from */
static void reach_from(const klt_piece *p, int i, int8_t *reach, int8_t *from)
{
    int8_t at[KLT_W * KLT_H];
    cells(p, at);
    memset(reach, -1, KLT_W * KLT_H);
    int queue[KLT_W * KLT_H], head = 0, tail = 0, start = p[i].x + p[i].y * KLT_W;
    reach[start] = 0;
    if (from) from[start] = -1;
    queue[tail++] = start;
    static const int D[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int x = c % KLT_W + D[d][0], y = c / KLT_W + D[d][1];
            if (!fits(at, p, i, x, y) || reach[x + y * KLT_W] >= 0) continue;
            reach[x + y * KLT_W] = (int8_t)(reach[c] + 1);
            if (from) from[x + y * KLT_W] = (int8_t)c;
            queue[tail++] = x + y * KLT_W;
        }
    }
}

void klt_reach(const klt_game *g, int i, int8_t *reach)
{
    if (i < 0 || i >= KLT_PIECES) { memset(reach, -1, KLT_W * KLT_H); return; }
    reach_from(g->p, i, reach, NULL);
}

int klt_path(const klt_game *g, int i, int x, int y, int8_t *path)
{
    if (i < 0 || i >= KLT_PIECES || x < 0 || y < 0 || x >= KLT_W || y >= KLT_H) return 0;
    int8_t reach[KLT_W * KLT_H], from[KLT_W * KLT_H];
    reach_from(g->p, i, reach, from);
    int c = x + y * KLT_W, n = reach[c];
    if (n <= 0) return 0;
    for (int k = n - 1; k >= 0; k--, c = from[c]) {
        path[2 * k] = (int8_t)(c % KLT_W);
        path[2 * k + 1] = (int8_t)(c / KLT_W);
    }
    return n;
}

int klt_move(klt_game *g, int i, int x, int y)
{
    int8_t path[2 * KLT_PATH];
    int n = klt_path(g, i, x, y, path);
    for (int k = 0; k < n; k++) klt_step(g, i, path[2 * k] - g->p[i].x, path[2 * k + 1] - g->p[i].y);
    return n > 0;
}

int klt_undo(klt_game *g)
{
    if (!g->nh) return -1;
    g->nh--;
    int moved = -1;
    for (int i = 0; i < KLT_PIECES; i++) {
        if (g->p[i].x != g->hist[g->nh][i][0] || g->p[i].y != g->hist[g->nh][i][1]) moved = i;
        g->p[i].x = g->hist[g->nh][i][0];
        g->p[i].y = g->hist[g->nh][i][1];
    }
    g->moves--;
    g->last = -1;
    return moved;
}

int klt_solved(const klt_game *g)
{
    for (int i = 0; i < KLT_PIECES; i++)
        if (g->p[i].type == KLT_BIG) return g->p[i].x == 1 && g->p[i].y == 3;
    return 0;
}

/* ---- the search: a position as its cells, 3 bits each (the kind of block and which part of it), so blocks of one
 * kind are alike; every position one move away, breadth first, until the great block stands at the opening ------- */
static uint64_t code_of(const klt_piece *p)
{
    uint64_t k = 0;
    for (int i = 0; i < KLT_PIECES; i++) {
        int t = p[i].type, c = p[i].x + p[i].y * KLT_W;
        uint64_t v = t == KLT_BIG ? 6 : t == KLT_TALL ? 2 : t == KLT_WIDE ? 4 : 1;
        k |= v << (3 * c);
        if (t == KLT_BIG) k |= 7ull << (3 * (c + 1)) | 7ull << (3 * (c + KLT_W)) | 7ull << (3 * (c + KLT_W + 1));
        if (t == KLT_TALL) k |= 3ull << (3 * (c + KLT_W));
        if (t == KLT_WIDE) k |= 5ull << (3 * (c + 1));
    }
    return k;
}

static void blocks_of(uint64_t k, klt_piece *p)
{
    int n = 0;
    for (int c = 0; c < KLT_W * KLT_H && n < KLT_PIECES; c++) {
        int v = (int)(k >> (3 * c) & 7);
        int t = v == 6 ? KLT_BIG : v == 2 ? KLT_TALL : v == 4 ? KLT_WIDE : v == 1 ? KLT_SMALL : -1;
        if (t < 0) continue;
        p[n].type = (int8_t)t;
        p[n].x = (int8_t)(c % KLT_W);
        p[n++].y = (int8_t)(c / KLT_W);
    }
}

static int goal(uint64_t k) { return (k >> (3 * (1 + 3 * KLT_W)) & 7) == 6; }

#define TABLE (1 << 18)

int klt_solve(const klt_game *g, int *piece, int *x, int *y)
{
    if (klt_solved(g)) return 0;
    uint64_t *key = calloc(TABLE, sizeof *key), *order = malloc(TABLE * sizeof *order);
    int32_t *parent = malloc(TABLE * sizeof *parent);
    int16_t *depth = malloc(TABLE * sizeof *depth);
    int found = -1, best = -1;
    if (!key || !order || !parent || !depth) goto done;
    int head = 0, tail = 0;
    uint64_t root = code_of(g->p);
    /* the table: open addressing on the code (never 0: the board always holds blocks) */
    #define FIND(k, s)                                                                                              \
        do {                                                                                                        \
            s = (int)(((k) * 0x9e3779b97f4a7c15ull) >> 46);                                                        \
            while (key[s] && key[s] != (k)) s = (s + 1) & (TABLE - 1);                                              \
        } while (0)
    int s;
    FIND(root, s);
    key[s] = root;
    order[tail] = root;
    parent[tail] = -1;
    depth[tail++] = 0;
    while (head < tail && found < 0) {
        int at = head++;
        klt_piece p[KLT_PIECES];
        blocks_of(order[at], p);
        for (int i = 0; i < KLT_PIECES && found < 0; i++) {
            int8_t reach[KLT_W * KLT_H];
            reach_from(p, i, reach, NULL);
            int8_t ox = p[i].x, oy = p[i].y;
            for (int c = 0; c < KLT_W * KLT_H && found < 0; c++) {
                if (reach[c] <= 0) continue;
                p[i].x = (int8_t)(c % KLT_W);
                p[i].y = (int8_t)(c / KLT_W);
                uint64_t k = code_of(p);
                p[i].x = ox;
                p[i].y = oy;
                FIND(k, s);
                if (key[s]) continue;
                if (tail >= TABLE / 2) { head = tail; break; }   /* too many: give up (never with these trays) */
                key[s] = k;
                order[tail] = k;
                parent[tail] = at;
                depth[tail] = (int16_t)(depth[at] + 1);
                if (goal(k)) found = tail;
                tail++;
            }
        }
    }
    #undef FIND
    if (found < 0) goto done;
    best = depth[found];
    int first = found;
    while (parent[first] > 0) first = parent[first];
    if (piece || x || y) {                      /* which block of this game makes that first move */
        for (int i = 0; i < KLT_PIECES; i++) {
            int8_t reach[KLT_W * KLT_H];
            reach_from(g->p, i, reach, NULL);
            for (int c = 0; c < KLT_W * KLT_H; c++) {
                if (reach[c] <= 0) continue;
                klt_piece p[KLT_PIECES];
                memcpy(p, g->p, sizeof p);
                p[i].x = (int8_t)(c % KLT_W);
                p[i].y = (int8_t)(c / KLT_W);
                if (code_of(p) != order[first]) continue;
                if (piece) *piece = i;
                if (x) *x = c % KLT_W;
                if (y) *y = c / KLT_W;
                goto done;
            }
        }
    }
done:
    free(key);
    free(order);
    free(parent);
    free(depth);
    return best;
}

int klt_script(klt_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s++ == '=';
            do {
                int i, x, y;
                if (klt_solve(g, &i, &x, &y) <= 0) break;
                g->last = -1;
                klt_move(g, i, x, y);
            } while (all && !klt_solved(g));
            continue;
        }
        if (*s < 'a' || *s > 'd' || s[1] < '1' || s[1] > '5') return 0;
        int i = klt_at(g, s[0] - 'a', s[1] - '1');
        s += 2;
        if (i < 0) return 0;
        g->last = -1;                           /* a block named: a move of its own */
        int n = 0;
        for (; *s && strchr("udlr", *s); s++, n++) {
            int dx = *s == 'l' ? -1 : *s == 'r' ? 1 : 0, dy = *s == 'u' ? -1 : *s == 'd' ? 1 : 0;
            if (!klt_step(g, i, dx, dy)) return 0;
        }
        if (!n) return 0;
    }
    return 1;
}
