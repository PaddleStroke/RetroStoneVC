/*
 * Klotski: the rules (klotski.h): trays as text, slides, moves as the tradition counts them, undo, the breadth-first
 * search for the fewest moves (the par, the hint), a tray's whole family of positions with their distances (the
 * puzzles made at random and the levels' tool), the levels. No SDL: tested by tests/klotski_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/klotski.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const int8_t PW[4] = {2, 1, 2, 1}, PH[4] = {2, 2, 1, 1};   /* each kind's width and height */

/* the board as cells: the block on each (-1 empty) */
static void cells(const klt_piece *p, int np, int8_t *at)
{
    memset(at, -1, KLT_W * KLT_H);
    for (int i = 0; i < np; i++)
        for (int dy = 0; dy < PH[p[i].type]; dy++)
            for (int dx = 0; dx < PW[p[i].type]; dx++) at[p[i].x + dx + (p[i].y + dy) * KLT_W] = (int8_t)i;
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

static void reset(klt_game *g)
{
    g->nh = g->moves = 0;
    g->last = -1;
}

/* the blocks given: one great block, the cells covered once, two left empty */
static int valid(const klt_piece *p, int np)
{
    int covered = 0, big = 0;
    int8_t at[KLT_W * KLT_H];
    memset(at, -1, sizeof at);
    for (int i = 0; i < np; i++) {
        if (p[i].type < 0 || p[i].type > 3) return 0;
        big += p[i].type == KLT_BIG;
        if (!fits(at, p, i, p[i].x, p[i].y)) return 0;
        for (int dy = 0; dy < PH[p[i].type]; dy++)
            for (int dx = 0; dx < PW[p[i].type]; dx++) at[p[i].x + dx + (p[i].y + dy) * KLT_W] = (int8_t)i;
        covered += PW[p[i].type] * PH[p[i].type];
    }
    return big == 1 && covered == KLT_W * KLT_H - 2;
}

int klt_parse(klt_game *g, const char *s)
{
    memset(g, 0, sizeof *g);
    while (*s) {
        if (*s == ' ') { s++; continue; }
        const char *k = strchr("BTWS", *s);
        if (!k || !*s || g->np >= KLT_PIECES || s[1] < '0' || s[1] > '4' || s[2] < '0' || s[2] > '3') return 0;
        g->p[g->np].type = (int8_t)(k - "BTWS");
        g->p[g->np].y = (int8_t)(s[1] - '0');
        g->p[g->np].x = (int8_t)(s[2] - '0');
        g->np++;
        s += 3;
    }
    if (!valid(g->p, g->np)) return 0;
    reset(g);
    return 1;
}

void klt_text(const klt_game *g, char *out)
{
    int k = 0;
    for (int i = 0; i < g->np; i++) {
        if (i) out[k++] = ' ';
        out[k++] = "BTWS"[g->p[i].type];
        out[k++] = (char)('0' + g->p[i].y);
        out[k++] = (char)('0' + g->p[i].x);
    }
    out[k] = 0;
}

int klt_at(const klt_game *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= KLT_W || y >= KLT_H) return -1;
    int8_t at[KLT_W * KLT_H];
    cells(g->p, g->np, at);
    return at[x + y * KLT_W];
}

int klt_can(const klt_game *g, int i, int dx, int dy)
{
    if (i < 0 || i >= g->np || abs(dx) + abs(dy) != 1) return 0;
    int8_t at[KLT_W * KLT_H];
    cells(g->p, g->np, at);
    return fits(at, g->p, i, g->p[i].x + dx, g->p[i].y + dy);
}

static void remember(klt_game *g)
{
    if (g->nh >= KLT_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + KLT_HIST / 2, sizeof g->hist / 2);
        g->nh -= KLT_HIST / 2;
    }
    for (int i = 0; i < g->np; i++) {
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
    for (int k = 0; k < g->np; k++)
        if (g->p[k].x != g->hist[g->nh - 1][k][0] || g->p[k].y != g->hist[g->nh - 1][k][1]) back = 0;
    if (back) {
        g->nh--;
        g->moves--;
        g->last = -1;
    }
    return 1;
}

/* the block's places one move away, by a search of its slides; from: the place each was reached from */
static void reach_from(const klt_piece *p, int np, int i, int8_t *reach, int8_t *from)
{
    int8_t at[KLT_W * KLT_H];
    cells(p, np, at);
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
    if (i < 0 || i >= g->np) { memset(reach, -1, KLT_W * KLT_H); return; }
    reach_from(g->p, g->np, i, reach, NULL);
}

int klt_path(const klt_game *g, int i, int x, int y, int8_t *path)
{
    if (i < 0 || i >= g->np || x < 0 || y < 0 || x >= KLT_W || y >= KLT_H) return 0;
    int8_t reach[KLT_W * KLT_H], from[KLT_W * KLT_H];
    reach_from(g->p, g->np, i, reach, from);
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
    for (int i = 0; i < g->np; i++) {
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
    for (int i = 0; i < g->np; i++)
        if (g->p[i].type == KLT_BIG) return g->p[i].x == 1 && g->p[i].y == 3;
    return 0;
}

/* ---- positions as codes: the cells, 3 bits each (the kind of block and which part of it), so blocks of one kind are
 * alike ------------------------------------------------------------------------------------------------------------ */
static uint64_t code_of(const klt_piece *p, int np)
{
    uint64_t k = 0;
    for (int i = 0; i < np; i++) {
        int t = p[i].type, c = p[i].x + p[i].y * KLT_W;
        uint64_t v = t == KLT_BIG ? 6 : t == KLT_TALL ? 2 : t == KLT_WIDE ? 4 : 1;
        k |= v << (3 * c);
        if (t == KLT_BIG) k |= 7ull << (3 * (c + 1)) | 7ull << (3 * (c + KLT_W)) | 7ull << (3 * (c + KLT_W + 1));
        if (t == KLT_TALL) k |= 3ull << (3 * (c + KLT_W));
        if (t == KLT_WIDE) k |= 5ull << (3 * (c + 1));
    }
    return k;
}

static int blocks_of(uint64_t k, klt_piece *p)  /* the blocks in cell order: how many */
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
    return n;
}

static int goal(uint64_t k) { return (k >> (3 * (1 + 3 * KLT_W)) & 7) == 6; }

#define TABLE (1 << 18)                         /* more than twice KLT_FAMILY */

static int slot_of(const uint64_t *key, uint64_t k)
{
    int s = (int)((k * 0x9e3779b97f4a7c15ull) >> 46);
    while (key[s] && key[s] != k) s = (s + 1) & (TABLE - 1);
    return s;
}

int klt_solve(const klt_game *g, int *piece, int *x, int *y)
{
    if (klt_solved(g)) return 0;
    uint64_t *key = calloc(TABLE, sizeof *key), *order = malloc(TABLE / 2 * sizeof *order);
    int32_t *parent = malloc(TABLE / 2 * sizeof *parent);
    int16_t *depth = malloc(TABLE / 2 * sizeof *depth);
    int found = -1, best = -1;
    if (!key || !order || !parent || !depth) goto done;
    int head = 0, tail = 0;
    uint64_t root = code_of(g->p, g->np);
    key[slot_of(key, root)] = root;
    order[tail] = root;
    parent[tail] = -1;
    depth[tail++] = 0;
    if (g->last >= 0 && g->last < g->np) {     /* the block moved last slides on in the same move: no move more */
        int8_t reach[KLT_W * KLT_H];
        reach_from(g->p, g->np, g->last, reach, NULL);
        for (int c = 0; c < KLT_W * KLT_H && found < 0; c++) {
            if (reach[c] <= 0) continue;
            klt_piece p[KLT_PIECES];
            memcpy(p, g->p, sizeof p);
            p[g->last].x = (int8_t)(c % KLT_W);
            p[g->last].y = (int8_t)(c / KLT_W);
            uint64_t k = code_of(p, g->np);
            int s = slot_of(key, k);
            if (key[s]) continue;
            key[s] = k;
            order[tail] = k;
            parent[tail] = -1;
            depth[tail] = 0;
            if (goal(k)) found = tail;
            tail++;
        }
    }
    while (head < tail && found < 0) {
        int at = head++;
        klt_piece p[KLT_PIECES];
        int np = blocks_of(order[at], p);
        for (int i = 0; i < np && found < 0; i++) {
            int8_t reach[KLT_W * KLT_H];
            reach_from(p, np, i, reach, NULL);
            int8_t ox = p[i].x, oy = p[i].y;
            for (int c = 0; c < KLT_W * KLT_H && found < 0; c++) {
                if (reach[c] <= 0) continue;
                p[i].x = (int8_t)(c % KLT_W);
                p[i].y = (int8_t)(c / KLT_W);
                uint64_t k = code_of(p, np);
                p[i].x = ox;
                p[i].y = oy;
                int s = slot_of(key, k);
                if (key[s]) continue;
                if (tail >= TABLE / 2) { head = tail; break; }   /* too many: give up */
                key[s] = k;
                order[tail] = k;
                parent[tail] = at;
                depth[tail] = (int16_t)(depth[at] + 1);
                if (goal(k)) found = tail;
                tail++;
            }
        }
    }
    if (found < 0) goto done;
    best = depth[found] > 0 ? depth[found] : 1;    /* (the move going on, slid on to the end: one move left) */
    int first = found;                          /* the first move: from the start, or the move going on slid on */
    while (parent[first] >= 0 && depth[parent[first]] > 0) first = parent[first];
    if (parent[first] > 0) first = parent[first];
    if (piece || x || y) {                      /* which block of this game makes that first move (the last first) */
        for (int k = -1; k < g->np; k++) {
            int i = k < 0 ? g->last : k;
            if (i < 0 || i >= g->np) continue;
            int8_t reach[KLT_W * KLT_H];
            reach_from(g->p, g->np, i, reach, NULL);
            for (int c = 0; c < KLT_W * KLT_H; c++) {
                if (reach[c] <= 0) continue;
                klt_piece p[KLT_PIECES];
                memcpy(p, g->p, sizeof p);
                p[i].x = (int8_t)(c % KLT_W);
                p[i].y = (int8_t)(c / KLT_W);
                if (code_of(p, g->np) != order[first]) continue;
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

/* ---- a family: every position a tray can reach, the moves between them, and each one's fewest moves out --------- */
typedef struct {
    uint64_t *key, *order;
    int32_t *first, *edge;
    int16_t *dist;
    int n, ne, ecap;
} family;

static void family_free(family *F)
{
    free(F->key);
    free(F->order);
    free(F->first);
    free(F->edge);
    free(F->dist);
    memset(F, 0, sizeof *F);
}

/* 1: built (F->n positions, F->dist[i] the fewest moves out of order[i], -1 none); 0: more than cap, or no memory */
static int family_build(family *F, const klt_piece *start, int np0, int cap)
{
    memset(F, 0, sizeof *F);
    if (cap > KLT_FAMILY) cap = KLT_FAMILY;
    F->key = calloc(TABLE, sizeof *F->key);
    F->order = malloc((size_t)cap * sizeof *F->order);
    F->first = malloc(((size_t)cap + 1) * sizeof *F->first);
    F->dist = malloc((size_t)cap * sizeof *F->dist);
    int32_t *idx = malloc(TABLE * sizeof *idx);
    F->ecap = 8 * cap;
    F->edge = malloc((size_t)F->ecap * sizeof *F->edge);
    int ok = 0;
    if (!F->key || !F->order || !F->first || !F->dist || !idx || !F->edge) goto out;
    uint64_t root = code_of(start, np0);
    int s = slot_of(F->key, root);
    F->key[s] = root;
    idx[s] = 0;
    F->order[F->n++] = root;
    for (int at = 0; at < F->n; at++) {
        F->first[at] = F->ne;
        klt_piece p[KLT_PIECES];
        int np = blocks_of(F->order[at], p);
        for (int i = 0; i < np; i++) {
            int8_t reach[KLT_W * KLT_H];
            reach_from(p, np, i, reach, NULL);
            int8_t ox = p[i].x, oy = p[i].y;
            for (int c = 0; c < KLT_W * KLT_H; c++) {
                if (reach[c] <= 0) continue;
                p[i].x = (int8_t)(c % KLT_W);
                p[i].y = (int8_t)(c / KLT_W);
                uint64_t k = code_of(p, np);
                p[i].x = ox;
                p[i].y = oy;
                s = slot_of(F->key, k);
                if (!F->key[s]) {
                    if (F->n >= cap) goto out;
                    F->key[s] = k;
                    idx[s] = F->n;
                    F->order[F->n++] = k;
                }
                if (F->ne >= F->ecap) {
                    int32_t *e = realloc(F->edge, (size_t)F->ecap * 2 * sizeof *e);
                    if (!e) goto out;
                    F->edge = e;
                    F->ecap *= 2;
                }
                F->edge[F->ne++] = idx[s];
            }
        }
    }
    F->first[F->n] = F->ne;
    /* the fewest moves out: breadth first from every solved position (a move can always be made back) */
    int32_t *queue = malloc((size_t)F->n * sizeof *queue);
    if (!queue) goto out;
    int head = 0, tail = 0;
    for (int i = 0; i < F->n; i++) {
        F->dist[i] = -1;
        if (goal(F->order[i])) { F->dist[i] = 0; queue[tail++] = i; }
    }
    while (head < tail) {
        int at = queue[head++];
        for (int e = F->first[at]; e < F->first[at + 1]; e++)
            if (F->dist[F->edge[e]] < 0) {
                F->dist[F->edge[e]] = (int16_t)(F->dist[at] + 1);
                queue[tail++] = F->edge[e];
            }
    }
    free(queue);
    ok = 1;
out:
    free(idx);
    if (!ok) family_free(F);
    return ok;
}

static void load_code(klt_game *g, uint64_t code)   /* (the history left as it is: none) */
{
    memset(g->p, 0, sizeof g->p);
    g->np = blocks_of(code, g->p);
    g->par = 0;
    reset(g);
}

int klt_family(const klt_game *g, void (*each)(const klt_game *pos, int dist, void *ctx), void *ctx)
{
    family F;
    if (!family_build(&F, g->p, g->np, KLT_FAMILY)) return 0;
    for (int i = 0; i < F.n; i++) {
        klt_game pos;
        load_code(&pos, F.order[i]);
        pos.par = F.dist[i];
        each(&pos, F.dist[i], ctx);
    }
    int n = F.n;
    family_free(&F);
    return n;
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
                klt_move(g, i, x, y);           /* (the block moved last slides on: the same move, as solved) */
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

#ifndef KLT_GEN                                 /* not in the levels' tool (tools/klotski_gen.c): it makes them */
#include "games/klotski_levels.h"

/* a puzzle at random: the family of a level drawn by chance (one of the smaller families, under 30000 positions, whose
 * level is at least as far as the range: few trays laid by chance have a family worth playing, most are jammed), and
 * in it a position drawn at a distance in the variant's range (hard: the farthest), turned over half the time */
int klt_random(klt_game *g, int variant, uint32_t seed)
{
    static const int LO[KLT_VARIANTS] = {15, 30, 55}, HI[KLT_VARIANTS] = {30, 55, 999};
    int v = variant < 0 || variant >= KLT_VARIANTS ? 0 : variant, ok[KLT_LEVELS], n = 0;
    uint32_t s = pr_seed(seed);
    for (int l = 0; l < KLT_LEVELS; l++)
        if (klt_level_family[l] <= 30000 && klt_level_par[l] >= LO[v]) ok[n++] = l;
    for (int tries = 0; tries < 4 && n; tries++) {
        klt_game t;
        if (!klt_parse(&t, klt_levels[ok[pr_below(&s, n)]])) continue;
        family F;
        if (!family_build(&F, t.p, t.np, 30000)) continue;
        int pick = -1, seen = 0, far = 0;
        for (int i = 0; i < F.n; i++)
            if (F.dist[i] > far) far = F.dist[i];
        for (int i = 0; i < F.n; i++) {
            int d = F.dist[i];
            if (v == 2 ? d != far || d < LO[v] : d < LO[v] || d > HI[v]) continue;
            if (pr_below(&s, ++seen) == 0) pick = i;
        }
        if (pick >= 0) {
            load_code(g, F.order[pick]);
            g->par = F.dist[pick];
            if (pr_below(&s, 2))                /* turned over, left to right */
                for (int i = 0; i < g->np; i++) g->p[i].x = (int8_t)(KLT_W - PW[g->p[i].type] - g->p[i].x);
        }
        family_free(&F);
        if (pick >= 0) return 1;
    }
    return 0;
}

int klt_level(klt_game *g, int level)
{
    if (level < 0 || level >= KLT_LEVELS || !klt_parse(g, klt_levels[level])) return 0;
    g->par = klt_level_par[level];
    return 1;
}
#endif
