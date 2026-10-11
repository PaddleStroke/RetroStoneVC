/*
 * Dots and boxes: the rules (no SDL; tested by tests/dots_test.c). The board, the notation and the rules: dots.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/dots.h"

int dots_lines(int n) { return 2 * n * (n + 1); }

int dots_h(int n, int x, int y) { return x < 0 || x >= n || y < 0 || y > n ? -1 : y * n + x; }

int dots_v(int n, int x, int y) { return x < 0 || x > n || y < 0 || y >= n ? -1 : n * (n + 1) + y * (n + 1) + x; }

void dots_ends(int n, int line, int *x0, int *y0, int *x1, int *y1)
{
    int across = line < n * (n + 1);
    if (across) {
        *x0 = line % n;
        *y0 = line / n;
    } else {
        line -= n * (n + 1);
        *x0 = line % (n + 1);
        *y0 = line / (n + 1);
    }
    *x1 = *x0 + across;
    *y1 = *y0 + !across;
}

int dots_sides(int n, int box, int *s)
{
    int x = box % n, y = box / n;
    s[0] = dots_h(n, x, y);
    s[1] = dots_h(n, x, y + 1);
    s[2] = dots_v(n, x, y);
    s[3] = dots_v(n, x + 1, y);
    return 4;
}

void dots_start(dpos *p, int n)
{
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)(n < 1 ? 1 : n > DOTS_MAX ? DOTS_MAX : n);
    memset(p->line, -1, sizeof p->line);
    memset(p->box, -1, sizeof p->box);
}

/* the boxes on either side of a line (one at the edge): their count */
static int beside(int n, int line, int *b)
{
    int x, y, x1, y1, k = 0;
    dots_ends(n, line, &x, &y, &x1, &y1);
    if (y1 == y) {                              /* across: the box below it, the box above it */
        if (y > 0) b[k++] = (y - 1) * n + x;
        if (y < n) b[k++] = y * n + x;
    } else {                                    /* up: the box on its left, the box on its right */
        if (x > 0) b[k++] = y * n + x - 1;
        if (x < n) b[k++] = y * n + x;
    }
    return k;
}

int dots_make(dpos *p, int line)
{
    int n = p->n, b[2], s[4], claimed = 0;
    if (line < 0 || line >= dots_lines(n) || p->line[line] >= 0) return -1;
    p->line[line] = (int8_t)p->side;
    p->drawn++;
    for (int i = beside(n, line, b) - 1; i >= 0; i--) {
        dots_sides(n, b[i], s);
        if (p->line[s[0]] >= 0 && p->line[s[1]] >= 0 && p->line[s[2]] >= 0 && p->line[s[3]] >= 0) {
            p->box[b[i]] = (int8_t)p->side;
            p->boxes[p->side]++;
            claimed++;
        }
    }
    if (!claimed) p->side ^= 1;                 /* a box claimed: the same player draws again */
    return claimed;
}

static int number(const char *s, int *len)
{
    int v = 0, k = 0;
    while (s[k] >= '0' && s[k] <= '9' && k < 3) v = v * 10 + (s[k++] - '0');
    *len = k;
    return k ? v : -1;
}

int dots_parse(int n, const char *s, int *len)
{
    int a, b, la, lb;
    if (s[0] != 'h' && s[0] != 'v') return -1;
    if ((a = number(s + 1, &la)) < 1 || s[1 + la] != ',' || (b = number(s + 2 + la, &lb)) < 1) return -1;
    *len = 2 + la + lb;
    return s[0] == 'h' ? dots_h(n, a - 1, b - 1) : dots_v(n, a - 1, b - 1);
}

void dots_new(dgame *g, int n)
{
    g->ply = 0;
    dots_start(&g->pos[0], n);
}

int dots_play(dgame *g, int line)
{
    if (g->ply >= DOTS_MAX_LINES) return -1;
    dpos *next = &g->pos[g->ply + 1];
    *next = g->pos[g->ply];
    int claimed = dots_make(next, line);
    if (claimed < 0) return -1;
    g->move[g->ply++] = (int16_t)line;
    return claimed;
}

int dots_undo(dgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

dstatus dots_status(const dgame *g)
{
    const dpos *p = dots_pos(g);
    if (p->drawn < dots_lines(p->n)) return DS_PLAY;
    return p->boxes[0] > p->boxes[1] ? DS_WIN0 : p->boxes[1] > p->boxes[0] ? DS_WIN1 : DS_DRAW;
}

int dots_script(dgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, line = dots_parse(dots_pos(g)->n, s, &len);
        if (line < 0 || dots_play(g, line) < 0) return 0;
        s += len;
    }
    return 1;
}
