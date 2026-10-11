/*
 * Gomoku's rules (gomoku.h): five in a row, the freestyle and standard ways; six in a row, two stones a turn.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/gomoku.h"

static const int DIR[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

int gk_size(int variant) { return variant == GK_SIX ? GK_MAX : GK_N; }

void gk_new(gkgame *g, int variant)
{
    memset(g, 0, sizeof *g);
    g->variant = variant >= 0 && variant < GK_VARIANTS ? variant : GK_FREESTYLE;
    g->n = gk_size(g->variant);
}

/* the turn stone k belongs to: one stone each (gomoku), or 1, 2, 2, 2... (six in a row) */
static int turn_of(const gkgame *g, int k) { return g->variant != GK_SIX ? k : (k + 1) / 2; }

int gk_side(const gkgame *g) { return turn_of(g, g->nmoves) & 1; }

int gk_turn_start(const gkgame *g, int k) { return g->variant != GK_SIX || !k ? k : k - !(k & 1); }

int gk_left(const gkgame *g)
{
    int k = g->nmoves;
    return g->variant == GK_SIX && k && (k & 1) ? 2 : 1;
}

int gk_last(const gkgame *g) { return g->nmoves ? g->move[g->nmoves - 1] : -1; }

/* the stones of one colour in a row through p, along d (p's own included); from: the first of them */
static int run(const gkgame *g, int p, const int d[2], int *from)
{
    int n = g->n, x0 = p % n, y0 = p / n, c = g->pt[p], back = 0, ahead = 0;
    for (int x = x0 - d[0], y = y0 - d[1]; x >= 0 && y >= 0 && x < n && y < n && g->pt[x + y * n] == c;
         x -= d[0], y -= d[1])
        back++;
    for (int x = x0 + d[0], y = y0 + d[1]; x >= 0 && y >= 0 && x < n && y < n && g->pt[x + y * n] == c;
         x += d[0], y += d[1])
        ahead++;
    *from = (x0 - back * d[0]) + (y0 - back * d[1]) * n;
    return back + 1 + ahead;
}

static int wins(const gkgame *g, int len)
{
    return g->variant == GK_STANDARD ? len == 5 : g->variant == GK_SIX ? len >= 6 : len >= 5;
}

int gk_line(const gkgame *g, uint8_t line[GK_MAXPTS])
{
    memset(line, 0, (size_t)g->n * g->n);
    int p = gk_last(g), k = 0;
    if (p < 0) return 0;
    for (int i = 0; i < 4; i++) {
        int from, len = run(g, p, DIR[i], &from);
        if (!wins(g, len)) continue;
        for (int j = 0; j < len; j++) {
            int q = from + j * (DIR[i][0] + DIR[i][1] * g->n);
            k += !line[q];
            line[q] = 1;
        }
    }
    return k;
}

int gk_status(const gkgame *g)
{
    uint8_t line[GK_MAXPTS];
    if (gk_line(g, line)) return g->pt[gk_last(g)] == GK_EBONY ? GK_WIN0 : GK_WIN1;
    return g->nmoves == g->n * g->n ? GK_DRAW : GK_PLAY;
}

int gk_play(gkgame *g, int p)
{
    if (p < 0 || p >= g->n * g->n || g->pt[p] != GK_EMPTY || gk_status(g) != GK_PLAY) return 0;
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

int gk_point(int n, const char *s, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + n || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[k++] - '0';
    if (y > n) return -1;
    *len = k;
    return (s[0] - 'a') + (y - 1) * n;
}

int gk_script(gkgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, p = gk_point(g->n, s, &len);
        if (p < 0 || (s[len] && s[len] != ' ') || !gk_play(g, p)) return 0;
        s += len;
    }
    return 1;
}
