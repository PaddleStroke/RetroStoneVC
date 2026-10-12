/*
 * Rithmomachia's rules (rithmomachia.h): the layout, the moves by shape, the four captures, the victories. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/rithmomachia.h"

#define R RHM_ROUND
#define T RHM_TRIANGLE
#define Q RHM_SQUARE
#define P RHM_PYRAMID

const rhmpiece rhm_piece[RHM_PIECES] = {
    /* the evens */
    {R, 0, 2}, {R, 0, 4}, {R, 0, 6}, {R, 0, 8}, {R, 0, 4}, {R, 0, 16}, {R, 0, 36}, {R, 0, 64},
    {T, 0, 6}, {T, 0, 9}, {T, 0, 20}, {T, 0, 25}, {T, 0, 42}, {T, 0, 49}, {T, 0, 72}, {T, 0, 81},
    {Q, 0, 15}, {Q, 0, 25}, {Q, 0, 45}, {Q, 0, 81}, {Q, 0, 153}, {Q, 0, 169}, {Q, 0, 289}, {P, 0, 91},
    /* the odds */
    {R, 1, 3}, {R, 1, 5}, {R, 1, 7}, {R, 1, 9}, {R, 1, 9}, {R, 1, 25}, {R, 1, 49}, {R, 1, 81},
    {T, 1, 12}, {T, 1, 16}, {T, 1, 30}, {T, 1, 36}, {T, 1, 56}, {T, 1, 64}, {T, 1, 90}, {T, 1, 100},
    {Q, 1, 28}, {Q, 1, 49}, {Q, 1, 66}, {Q, 1, 120}, {Q, 1, 121}, {Q, 1, 225}, {Q, 1, 361}, {P, 1, 190},
};

const uint8_t rhm_layers[2][6] = {{36, 25, 16, 9, 4, 1}, {64, 49, 36, 25, 16, 0}};

/* where each piece starts: the file x (a..h) and the rank y (from the evens' side), the order of rhm_piece */
static const int8_t START[RHM_PIECES][2] = {
    {5, 3}, {4, 3}, {3, 3}, {2, 3}, {5, 2}, {4, 2}, {3, 2}, {2, 2},
    {6, 2}, {7, 2}, {4, 1}, {5, 1}, {3, 1}, {2, 1}, {1, 2}, {0, 2},
    {7, 1}, {7, 0}, {6, 1}, {6, 0}, {0, 1}, {1, 0}, {0, 0}, {1, 1},
    {2, 12}, {3, 12}, {4, 12}, {5, 12}, {2, 13}, {3, 13}, {4, 13}, {5, 13},
    {1, 13}, {0, 13}, {3, 14}, {2, 14}, {4, 14}, {5, 14}, {6, 13}, {7, 13},
    {0, 14}, {0, 15}, {1, 14}, {6, 14}, {1, 15}, {6, 15}, {7, 15}, {7, 14},
};

static const int8_t DIRS[8][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

int rhm_goal(int variant) { return variant == RHM_COMMON8 ? 8 : variant == RHM_COMMON12 ? 12 : 0; }

static int inside(int x, int y) { return x >= 0 && y >= 0 && x < RHM_W && y < RHM_H; }
static const rhmpiece *at(const rhmpos *p, int s) { return p->sq[s] ? &rhm_piece[p->sq[s] - 1] : NULL; }
static int owner(const rhmpos *p, int s) { return p->sq[s] ? rhm_piece[p->sq[s] - 1].owner : -1; }

/* the shape moves: a round (step 1, diagonals), a triangle (2, straight), a square (3, straight) */
static int shape_reach(const rhmpos *p, int shape, int a, int b)
{
    int dx = b % RHM_W - a % RHM_W, dy = b / RHM_W - a / RHM_W, n = shape == RHM_TRIANGLE ? 2 : 3;
    if (shape == RHM_ROUND) return abs(dx) == 1 && abs(dy) == 1;
    if (!((dx == 0 && abs(dy) == n) || (dy == 0 && abs(dx) == n))) return 0;
    int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    for (int i = 1; i < n; i++)
        if (p->sq[a + i * (sx + sy * RHM_W)]) return 0;   /* never over a piece */
    return 1;
}

int rhm_reach(const rhmpos *p, int a, int b)
{
    const rhmpiece *k = at(p, a);
    if (!k || a == b) return 0;
    if (k->shape != RHM_PYRAMID) return shape_reach(p, k->shape, a, b);
    return shape_reach(p, RHM_ROUND, a, b) || shape_reach(p, RHM_TRIANGLE, a, b) || shape_reach(p, RHM_SQUARE, a, b);
}

int rhm_legal(const rhmpos *p, rhmmove *out)
{
    static const int8_t TARGETS[12][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {2, 0}, {-2, 0}, {0, 2}, {0, -2},
                                          {3, 0}, {-3, 0}, {0, 3}, {0, -3}};
    int n = 0;
    for (int s = 0; s < RHM_N; s++) {
        if (owner(p, s) != p->side) continue;
        for (int t = 0; t < 12; t++) {
            int x = s % RHM_W + TARGETS[t][0], y = s / RHM_W + TARGETS[t][1];
            if (!inside(x, y) || p->sq[RHM_SQ(x, y)] || !rhm_reach(p, s, RHM_SQ(x, y))) continue;
            out[n++] = (rhmmove){(uint8_t)s, (uint8_t)RHM_SQ(x, y)};
        }
    }
    return n;
}

int rhm_threat(const rhmpos *p, int player, int s, int *att)
{
    const rhmpiece *e = at(p, s);
    if (!e || e->owner == player) return 0;
    int w = e->value, reach[RHM_PIECES], nr = 0;
    for (int a = 0; a < RHM_N; a++) {           /* meeting; and the pieces that could move onto it */
        if (owner(p, a) != player || !rhm_reach(p, a, s)) continue;
        if (at(p, a)->value == w) {
            if (att) att[0] = a, att[1] = -1;
            return RHM_MEETING;
        }
        reach[nr++] = a;
    }
    for (int a = 0; a < RHM_N; a++) {           /* assault: a smaller piece, along its own lines */
        const rhmpiece *k = at(p, a);
        if (!k || k->owner != player) continue;
        int d0 = k->shape == RHM_ROUND ? 0 : k->shape == RHM_PYRAMID ? 0 : 4, d1 = k->shape == RHM_ROUND ? 4 : 8;
        for (int d = d0; d < d1; d++) {
            int x = a % RHM_W + DIRS[d][0], y = a / RHM_W + DIRS[d][1], gap = 0;
            while (inside(x, y) && !p->sq[RHM_SQ(x, y)]) x += DIRS[d][0], y += DIRS[d][1], gap++;
            if (inside(x, y) && RHM_SQ(x, y) == s && gap > 1 && k->value * gap == w) {   /* a greater one */
                if (att) att[0] = a, att[1] = -1;
                return RHM_ASSAULT;
            }
        }
    }
    for (int i = 0; i < nr; i++)                /* ambush: two that could move onto it */
        for (int j = i + 1; j < nr; j++)
            if (at(p, reach[i])->value + at(p, reach[j])->value == w) {
                if (att) att[0] = reach[i], att[1] = reach[j];
                return RHM_AMBUSH;
            }
    int x = s % RHM_W, y = s / RHM_W, closed = 1;   /* siege */
    for (int d = 4; d < 8 && closed; d++) {
        int nx = x + DIRS[d][0], ny = y + DIRS[d][1];
        if (inside(nx, ny) && owner(p, RHM_SQ(nx, ny)) != player) closed = 0;
    }
    if (closed) {
        if (att) att[0] = att[1] = -1;
        return RHM_SIEGE;
    }
    return 0;
}

void rhm_step(rhmpos *p, rhmmove m)
{
    p->sq[m.to] = p->sq[m.from];
    p->sq[m.from] = 0;
}

int rhm_harmonic(int a, int b, int c)
{
    int lo = a < c ? a : c, hi = a < c ? c : a;
    if (!(lo < b && b < hi)) return 0;
    return 2 * b == lo + hi || b * b == lo * hi || b * (lo + hi) == 2 * lo * hi;
}

int rhm_find_harmony(const rhmpos *p, int player, uint8_t out[3])
{
    int sq[RHM_PIECES], n = 0;
    for (int s = 0; s < RHM_N; s++) {
        int y = s / RHM_W;
        if (owner(p, s) == player && (player == 0 ? y >= RHM_H / 2 : y < RHM_H / 2)) sq[n++] = s;
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            int a = sq[i], c = sq[j], dx = c % RHM_W - a % RHM_W, dy = c / RHM_W - a / RHM_W;
            if ((dx & 1) || (dy & 1) || !(dx == 0 || dy == 0 || abs(dx) == abs(dy))) continue;
            int b = a + dx / 2 + dy / 2 * RHM_W;
            if (owner(p, b) != player) continue;   /* the middle one, equally spaced */
            if (rhm_harmonic(at(p, a)->value, at(p, b)->value, at(p, c)->value)) {
                out[0] = (uint8_t)a, out[1] = (uint8_t)b, out[2] = (uint8_t)c;
                return 1;
            }
        }
    return 0;
}

void rhm_make(rhmpos *p, rhmmove m, int variant)
{
    rhm_step(p, m);
    int me = p->side, gone[RHM_N], n = 0;
    for (int s = 0; s < RHM_N; s++)             /* all at once, on the position the move made */
        if (owner(p, s) == !me && rhm_threat(p, me, s, NULL)) gone[n++] = s;
    for (int i = 0; i < n; i++) p->sq[gone[i]] = 0;
    p->taken[me] = (uint8_t)(p->taken[me] + n);
    p->quiet = n ? 0 : (uint8_t)(p->quiet + 1);
    p->proper = variant == RHM_PROPER && rhm_find_harmony(p, me, p->harmony);
    p->side ^= 1;
}

int rhm_square(const char *s, int *len)
{
    int x = s[0] - 'a', y = 0, k = 1;
    while (k < 3 && s[k] >= '0' && s[k] <= '9') y = y * 10 + s[k++] - '0';
    if (k == 1 || x < 0 || x >= RHM_W || y < 1 || y > RHM_H) return -1;
    if (len) *len = k;
    return RHM_SQ(x, y - 1);
}

void rhm_new(rhmgame *g, int variant)
{
    rhmpos *p = &g->pos[0];
    memset(p, 0, sizeof *p);
    for (int i = 0; i < RHM_PIECES; i++) p->sq[RHM_SQ(START[i][0], START[i][1])] = (uint8_t)(i + 1);
    g->ply = 0;
    g->variant = variant < 0 || variant >= RHM_VARIANTS ? 0 : variant;
}

int rhm_status(const rhmgame *g)
{
    const rhmpos *p = rhm_pos(g);
    int goal = rhm_goal(g->variant);
    if (p->proper) return p->side ? RHM_WIN0 : RHM_WIN1;   /* the player who just moved */
    for (int k = 0; k < 2; k++)
        if (goal && p->taken[k] >= goal) return k ? RHM_WIN1 : RHM_WIN0;
    rhmmove m[RHM_MAX_MOVES];
    if (!rhm_legal(p, m)) return p->side ? RHM_WIN0 : RHM_WIN1;
    if (p->quiet >= RHM_QUIET) return RHM_DRAW;
    return RHM_PLAY;
}

int rhm_play(rhmgame *g, rhmmove m)
{
    if (g->ply >= RHM_MAX_PLY || rhm_status(g) != RHM_PLAY) return 0;
    const rhmpos *p = rhm_pos(g);
    if (m.from >= RHM_N || m.to >= RHM_N || owner(p, m.from) != p->side || p->sq[m.to] || !rhm_reach(p, m.from, m.to))
        return 0;
    g->pos[g->ply + 1] = *p;
    rhm_make(&g->pos[g->ply + 1], m, g->variant);
    g->move[g->ply++] = m;
    return 1;
}

int rhm_undo(rhmgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int rhm_script(rhmgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int la, lb, a = rhm_square(s, &la), b = a < 0 ? -1 : rhm_square(s + la, &lb);
        if (a < 0 || b < 0 || !rhm_play(g, (rhmmove){(uint8_t)a, (uint8_t)b})) return 0;
        s += la + lb;
    }
    return 1;
}
