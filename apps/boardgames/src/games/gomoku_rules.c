/*
 * Gomoku's rules (gomoku.h): five in a row, the freestyle and standard ways; six in a row, two stones a turn; renju,
 * its forbidden points for ebony (found as the renju rules define them: a three only when the point that makes it a
 * straight four is not forbidden itself, a few levels deep).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
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

/* a row of len stones of that colour wins: renju, exactly five for ebony, five or more for ivory */
static int wins(const gkgame *g, int colour, int len)
{
    if (g->variant == GK_RENJU) return colour == GK_EBONY ? len == 5 : len >= 5;
    return g->variant == GK_STANDARD ? len == 5 : g->variant == GK_SIX ? len >= 6 : len >= 5;
}

/* ---- renju's forbidden points ------------------------------------------------------------------------------------
 * On a copy of the game, ebony's stone on p already: the points along direction d (offsets -4..4 from p) where one
 * more ebony stone makes exactly five through p */
static int fives(gkgame *g, int p, int d, int off[9])
{
    int n = g->n, x0 = p % n, y0 = p / n, k = 0, from;
    for (int i = -4; i <= 4; i++) {
        int x = x0 + i * DIR[d][0], y = y0 + i * DIR[d][1], q = x + y * n;
        if (!i || x < 0 || y < 0 || x >= n || y >= n || g->pt[q] != GK_EMPTY) continue;
        g->pt[q] = GK_EBONY;
        if (run(g, p, DIR[d], &from) == 5) off[k++] = i;
        g->pt[q] = GK_EMPTY;
    }
    return k;
}

/* the fours through p along d: a straight four (two points five apart, four stones between) counts once */
static int fours(gkgame *g, int p, int d)
{
    int off[9], k = fives(g, p, d, off);
    if (k == 2 && off[1] - off[0] == 5) return 1;
    return k > 2 ? 2 : k;
}

static int straight_four(gkgame *g, int p, int d)
{
    int off[9];
    return fives(g, p, d, off) == 2 && off[1] - off[0] == 5;
}

static int forbidden_at(gkgame *g, int p, int depth);

/* a three through p along d: a point where one more stone makes a straight four, the point not forbidden itself */
static int three(gkgame *g, int p, int d, int depth)
{
    int n = g->n, x0 = p % n, y0 = p / n;
    for (int i = -4; i <= 4; i++) {
        int x = x0 + i * DIR[d][0], y = y0 + i * DIR[d][1], q = x + y * n;
        if (!i || x < 0 || y < 0 || x >= n || y >= n || g->pt[q] != GK_EMPTY) continue;
        g->pt[q] = GK_EBONY;
        int four = straight_four(g, p, d);
        g->pt[q] = GK_EMPTY;
        if (four && (depth >= 4 || !forbidden_at(g, q, depth + 1))) return 1;
    }
    return 0;
}

static int forbidden_at(gkgame *g, int p, int depth)
{
    int from, five = 0, over = 0, f4 = 0, f3 = 0, bad;
    g->pt[p] = GK_EBONY;
    for (int d = 0; d < 4; d++) {
        int len = run(g, p, DIR[d], &from);
        five |= len == 5;
        over |= len > 5;
    }
    if (five) bad = 0;                          /* a five wins, whatever else the stone makes */
    else if (over) bad = 1;
    else {
        for (int d = 0; d < 4; d++) f4 += fours(g, p, d);
        if (f4 >= 2) bad = 1;
        else {
            for (int d = 0; d < 4 && f3 < 2; d++) f3 += three(g, p, d, depth);
            bad = f3 >= 2;
        }
    }
    g->pt[p] = GK_EMPTY;
    return bad;
}

int gk_forbidden(const gkgame *g, int p)
{
    if (g->variant != GK_RENJU || p < 0 || p >= g->n * g->n || g->pt[p] != GK_EMPTY) return 0;
    gkgame copy = *g;                           /* a local: the bot's thread tests points too (bots.h) */
    return forbidden_at(&copy, p, 0);
}

int gk_allowed(const gkgame *g, int p)
{
    if (p < 0 || p >= g->n * g->n || g->pt[p] != GK_EMPTY || gk_status(g) != GK_PLAY) return 0;
    if (g->variant != GK_RENJU) return 1;
    int c = g->n / 2, dx = abs(p % g->n - c), dy = abs(p / g->n - c);
    if (g->nmoves < 3 && (dx > g->nmoves || dy > g->nmoves)) return 0;   /* the opening: near the middle */
    return gk_side(g) != 0 || !gk_forbidden(g, p);
}

int gk_line(const gkgame *g, uint8_t line[GK_MAXPTS])
{
    memset(line, 0, (size_t)g->n * g->n);
    int p = gk_last(g), k = 0;
    if (p < 0) return 0;
    for (int i = 0; i < 4; i++) {
        int from, len = run(g, p, DIR[i], &from);
        if (!wins(g, g->pt[p], len)) continue;
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
    if (g->nmoves == g->n * g->n) return GK_DRAW;
    if (g->variant == GK_RENJU && gk_side(g) == 0 && g->nmoves >= 3) {   /* renju: ebony with every point forbidden */
        for (int p = 0; p < g->n * g->n; p++)
            if (g->pt[p] == GK_EMPTY && !gk_forbidden(g, p)) return GK_PLAY;
        return GK_DRAW;
    }
    return GK_PLAY;
}

int gk_play(gkgame *g, int p)
{
    if (!gk_allowed(g, p)) return 0;
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
