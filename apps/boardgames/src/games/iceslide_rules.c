/*
 * Ice Slide: the rules (iceslide.h): the slides, the search for the fewest pushes, the generator, undo, the notation.
 * No SDL: tested by tests/iceslide_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/iceslide.h"

#include <string.h>

#include "games/puzrand.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

void ics_variant(int variant, int *w, int *h, int *lo, int *hi, int *rocks)
{
    static const int V[ICS_VARIANTS][5] = {{7, 6, 4, 6, 7}, {8, 7, 7, 10, 10}, {10, 9, 11, 16, 15}};
    if (variant < 0 || variant >= ICS_VARIANTS) variant = 0;
    *w = V[variant][0];
    *h = V[variant][1];
    *lo = V[variant][2];
    *hi = V[variant][3];
    *rocks = V[variant][4];
}

int ics_slide(const ics_board *b, int from, int dir, int *len)
{
    int x = from % b->w, y = from / b->w, n = 0;
    for (;;) {
        int nx = x + DX[dir], ny = y + DY[dir];
        if (nx < 0 || ny < 0 || nx >= b->w || ny >= b->h || b->rock[nx + ny * b->w]) break;
        x = nx;
        y = ny;
        n++;
        if (x + y * b->w == b->goal) break;     /* over the hole: it drops in */
    }
    if (len) *len = n;
    return x + y * b->w;
}

int ics_solve(const ics_board *b, int from, int8_t *way)
{
    if (from == b->goal) return 0;
    int16_t dist[ICS_CELLS];
    int8_t by_dir[ICS_CELLS];
    int16_t prev[ICS_CELLS];
    int queue[ICS_CELLS], head = 0, tail = 0, cells = b->w * b->h;
    for (int i = 0; i < cells; i++) dist[i] = -1;
    dist[from] = 0;
    queue[tail++] = from;
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int len, to = ics_slide(b, c, d, &len);
            if (!len || dist[to] >= 0) continue;
            dist[to] = (int16_t)(dist[c] + 1);
            prev[to] = (int16_t)c;
            by_dir[to] = (int8_t)d;
            if (to == b->goal) {
                int n = dist[to];
                if (way && n <= ICS_LONGEST)
                    for (int k = n - 1, at = to; k >= 0; k--, at = prev[at]) way[k] = by_dir[at];
                return n;
            }
            queue[tail++] = to;
        }
    }
    return -1;
}

int ics_reach(const ics_board *b, int from)
{
    uint8_t seen[ICS_CELLS] = {0};
    int queue[ICS_CELLS], head = 0, tail = 0;
    seen[from] = 1;
    queue[tail++] = from;
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int len, to = ics_slide(b, c, d, &len);
            if (!len || seen[to] || to == b->goal) continue;
            seen[to] = 1;
            queue[tail++] = to;
        }
    }
    return tail;
}

static void reset(ics_game *g)
{
    g->puck = g->start;
    g->nh = g->moves = 0;
}

void ics_new(ics_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= ICS_VARIANTS) variant = 0;
    int w, h, lo, hi, rocks;
    ics_variant(variant, &w, &h, &lo, &hi, &rocks);
    uint32_t s = pr_seed(seed);
    memset(g, 0, sizeof *g);
    g->variant = variant;
    ics_board *b = &g->b;
    b->w = w;
    b->h = h;
    int cells = w * h, want = lo + pr_below(&s, hi - lo + 1);   /* the par sought: any in the range alike */
    for (;;) {
        memset(b->rock, 0, sizeof b->rock);
        int n = rocks + pr_below(&s, rocks / 3 + 1);
        for (int k = 0; k < n;) {
            int c = pr_below(&s, cells);
            if (!b->rock[c]) { b->rock[c] = 1; k++; }
        }
        int start;
        do start = pr_below(&s, cells);
        while (b->rock[start]);
        b->goal = -1;
        if (ics_reach(b, start) < lo + 3) continue;   /* hardly anywhere to go */
        int cand[ICS_CELLS], nc = 0;
        for (int c = 0; c < cells; c++) {
            if (b->rock[c] || c == start) continue;
            b->goal = c;
            int d = ics_solve(b, start, NULL);
            if (d == want) cand[nc++] = c;
        }
        if (!nc) continue;
        b->goal = cand[pr_below(&s, nc)];
        g->start = start;
        g->par = want;
        break;
    }
    reset(g);
}

int ics_load(ics_game *g, int w, int h, const char *rows)
{
    if (w < 1 || h < 1 || w > ICS_MAXW || h > ICS_MAXH) return 0;
    memset(g, 0, sizeof *g);
    g->b.w = w;
    g->b.h = h;
    g->b.goal = g->start = -1;
    int c = 0;
    for (const char *p = rows; *p && c < w * h; p++) {
        if (*p == ' ' || *p == '\n' || *p == '/') continue;
        if (*p == '#') g->b.rock[c] = 1;
        else if (*p == 'P') g->start = c;
        else if (*p == 'O') g->b.goal = c;
        else if (*p != '.') return 0;
        c++;
    }
    if (c != w * h || g->start < 0 || g->b.goal < 0) return 0;
    g->par = ics_solve(&g->b, g->start, NULL);
    reset(g);
    return 1;
}

int ics_solved(const ics_game *g) { return g->puck == g->b.goal; }

int ics_can(const ics_game *g, int dir)
{
    if (dir < 0 || dir > 3 || ics_solved(g)) return 0;
    int len;
    ics_slide(&g->b, g->puck, dir, &len);
    return len;
}

int ics_move(ics_game *g, int dir)
{
    if (!ics_can(g, dir)) return 0;
    if (g->nh >= ICS_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + ICS_HIST / 2, ICS_HIST / 2);
        g->nh -= ICS_HIST / 2;
    }
    int len;
    g->hist[g->nh++] = (int8_t)g->puck;
    g->puck = ics_slide(&g->b, g->puck, dir, &len);
    g->moves++;
    return len;
}

int ics_undo(ics_game *g)
{
    if (!g->nh) return 0;
    g->puck = g->hist[--g->nh];
    g->moves--;
    return 1;
}

int ics_hint(const ics_game *g)
{
    int8_t way[ICS_LONGEST];
    if (ics_solved(g)) return -1;
    int n = ics_solve(&g->b, g->puck, way);
    return n > 0 ? way[0] : -1;
}

int ics_script(ics_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c == '=') {
            int k = 0, any = 0;
            for (; *s >= '0' && *s <= '9'; s++, any = 1) k = k * 10 + *s - '0';
            if (!any) k = ICS_LONGEST;
            for (int i = 0; i < k && !ics_solved(g); i++) {
                int d = ics_hint(g);
                if (d < 0) return 0;
                ics_move(g, d);
            }
            continue;
        }
        const char *dirs = "urdl", *p = strchr(dirs, c | 32);
        if (!p || !c || !ics_move(g, (int)(p - dirs))) return 0;
    }
    return 1;
}
