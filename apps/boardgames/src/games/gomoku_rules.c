/*
 * Gomoku's rules (gomoku.h): five in a row, the freestyle and standard ways.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/gomoku.h"

static const int DIR[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

void gk_new(gkgame *g, int variant)
{
    memset(g, 0, sizeof *g);
    g->variant = variant;
}

int gk_side(const gkgame *g) { return g->nmoves & 1; }

int gk_last(const gkgame *g) { return g->nmoves ? g->move[g->nmoves - 1] : -1; }

/* the stones of one colour in a row through p, along d (p's own included); from: the first of them */
static int run(const gkgame *g, int p, const int d[2], int *from)
{
    int x0 = p % GK_N, y0 = p / GK_N, c = g->pt[p], back = 0, ahead = 0;
    for (int x = x0 - d[0], y = y0 - d[1]; x >= 0 && y >= 0 && x < GK_N && y < GK_N && g->pt[x + y * GK_N] == c;
         x -= d[0], y -= d[1])
        back++;
    for (int x = x0 + d[0], y = y0 + d[1]; x >= 0 && y >= 0 && x < GK_N && y < GK_N && g->pt[x + y * GK_N] == c;
         x += d[0], y += d[1])
        ahead++;
    *from = (x0 - back * d[0]) + (y0 - back * d[1]) * GK_N;
    return back + 1 + ahead;
}

static int wins(const gkgame *g, int len) { return g->variant == GK_STANDARD ? len == 5 : len >= 5; }

int gk_line(const gkgame *g, uint8_t line[GK_PTS])
{
    memset(line, 0, GK_PTS);
    int p = gk_last(g), k = 0;
    if (p < 0) return 0;
    for (int i = 0; i < 4; i++) {
        int from, len = run(g, p, DIR[i], &from);
        if (!wins(g, len)) continue;
        for (int j = 0; j < len; j++) {
            int q = from + j * (DIR[i][0] + DIR[i][1] * GK_N);
            k += !line[q];
            line[q] = 1;
        }
    }
    return k;
}

int gk_status(const gkgame *g)
{
    uint8_t line[GK_PTS];
    if (gk_line(g, line)) return g->pt[gk_last(g)] == GK_EBONY ? GK_WIN0 : GK_WIN1;
    return g->nmoves == GK_PTS ? GK_DRAW : GK_PLAY;
}

int gk_play(gkgame *g, int p)
{
    if (p < 0 || p >= GK_PTS || g->pt[p] != GK_EMPTY || gk_status(g) != GK_PLAY) return 0;
    g->pt[p] = (uint8_t)(gk_side(g) + 1);
    g->move[g->nmoves++] = p;
    return 1;
}

int gk_undo(gkgame *g)
{
    if (!g->nmoves) return 0;
    g->pt[g->move[--g->nmoves]] = GK_EMPTY;
    return 1;
}

int gk_point(const char *s, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + GK_N || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', n = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[n++] - '0';
    if (y > GK_N) return -1;
    *len = n;
    return (s[0] - 'a') + (y - 1) * GK_N;
}

int gk_script(gkgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, p = gk_point(s, &len);
        if (p < 0 || (s[len] && s[len] != ' ') || !gk_play(g, p)) return 0;
        s += len;
    }
    return 1;
}
