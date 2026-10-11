/*
 * Surakarta: the rules (no SDL; tested by tests/surakarta_test.c). The board, the circuits, the notation and the rules:
 * surakarta.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/surakarta.h"

#define PI_F 3.14159265f

const int8_t sk_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, sk_dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};

/* each circuit: up its left column, right along its top row, down its right column, left along its bottom row; the
 * loops round the corners between (radius 1: the lines b, e, 2, 5; radius 2: c, d, 3, 4) */
const int8_t sk_track[2][SK_TRACK] = {
    {1, 7, 13, 19, 25, 31, 24, 25, 26, 27, 28, 29, 34, 28, 22, 16, 10, 4, 11, 10, 9, 8, 7, 6},
    {2, 8, 14, 20, 26, 32, 18, 19, 20, 21, 22, 23, 33, 27, 21, 15, 9, 3, 17, 16, 15, 14, 13, 12},
};

int sk_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'f' || s[1] < '1' || s[1] > '6') return -1;
    return (s[0] - 'a') + SK_SIDE * (s[1] - '1');
}

static int edge(int v) { return v == 0 || v == SK_SIDE - 1; }

int sk_corner(int c, int i)
{
    int a = sk_track[c][i], b = sk_track[c][(i + 1) % SK_TRACK];
    int x = edge(sk_x(a)) ? sk_x(a) : sk_x(b), y = edge(sk_y(a)) ? sk_y(a) : sk_y(b);
    return x + SK_SIDE * y;
}

void sk_start(skpos *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 0; i < SK_POINTS; i++) p->at[i] = (int8_t)(sk_y(i) < 2 ? 0 : sk_y(i) > 3 ? 1 : -1);
    p->left[0] = p->left[1] = SK_PIECES;
}

/* the step from entry j in direction dir goes round a loop */
static int through_loop(int j, int dir)
{
    return sk_loop_after(dir > 0 ? j : (j + SK_TRACK - 1) % SK_TRACK);
}

int sk_loops(skmove m)
{
    int n = 0;
    for (int k = 0, j = m.start; m.circuit >= 0 && k < m.steps; k++, j = (j + m.dir + SK_TRACK) % SK_TRACK)
        n += through_loop(j, m.dir);
    return n;
}

float sk_length(skmove m)
{
    if (m.circuit < 0) return (sk_x(m.from) != sk_x(m.to) && sk_y(m.from) != sk_y(m.to)) ? 1.41421356f : 1;
    int loops = sk_loops(m);                    /* a loop: three quarters of a circle of radius circuit + 1 */
    return (float)(m.steps - loops) + loops * 1.5f * PI_F * (m.circuit + 1);
}

/* the runs from the piece on pt: along each circuit through it, each way; the shortest to each piece it can take */
static int runs(const skpos *p, int pt, skmove *out, int n)
{
    int me = p->at[pt];
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < SK_TRACK; i++) {
            if (sk_track[c][i] != pt) continue;
            for (int dir = -1; dir <= 1; dir += 2) {
                int j = i, loops = 0;
                for (int k = 1; k <= SK_TRACK; k++) {
                    loops += through_loop(j, dir);
                    j = (j + dir + SK_TRACK) % SK_TRACK;
                    int q = sk_track[c][j];
                    if (q == pt || p->at[q] < 0) continue;   /* its own starting point is empty now */
                    if (p->at[q] != me && loops) {
                        skmove m = {(int8_t)pt, (int8_t)q, (int8_t)c, (int8_t)i, (int8_t)dir, (int8_t)k};
                        int seen = -1;
                        for (int e = 0; e < n; e++)
                            if (out[e].from == pt && out[e].to == q) seen = e;
                        if (seen < 0) out[n++] = m;
                        else if (sk_length(m) < sk_length(out[seen])) out[seen] = m;
                    }
                    break;
                }
            }
        }
    return n;
}

int sk_legal(const skpos *p, skmove *out)
{
    int n = 0;
    for (int pt = 0; pt < SK_POINTS; pt++) {
        if (p->at[pt] != p->side) continue;
        for (int d = 0; d < 8; d++) {
            int x = sk_x(pt) + sk_dx[d], y = sk_y(pt) + sk_dy[d], to = x + SK_SIDE * y;
            if (x >= 0 && x < SK_SIDE && y >= 0 && y < SK_SIDE && p->at[to] < 0)
                out[n++] = (skmove){(int8_t)pt, (int8_t)to, -1, 0, 0, 0};
        }
    }
    for (int pt = 0; pt < SK_POINTS; pt++)
        if (p->at[pt] == p->side) n = runs(p, pt, out, n);
    return n;
}

void sk_make(skpos *p, skmove m)
{
    if (p->at[m.to] >= 0) {
        p->left[p->at[m.to]]--;
        p->quiet = 0;
    } else {
        p->quiet++;
    }
    p->at[m.to] = p->at[m.from];
    p->at[m.from] = -1;
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void sk_new(skgame *g)
{
    g->ply = 0;
    sk_start(&g->pos[0]);
}

int sk_play(skgame *g, skmove m)
{
    if (g->ply >= SK_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    sk_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int sk_undo(skgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

skstatus sk_status(const skgame *g)
{
    const skpos *p = sk_pos(g);
    skmove m[SK_MAX_MOVES];
    if (!p->left[p->side] || !sk_legal(p, m)) return SS_STUCK;
    if (p->quiet >= SK_QUIET) return SS_QUIET;
    int seen = 0;                               /* back to the last capture: nothing before it repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) {
        const skpos *q = &g->pos[i];
        seen += q->side == p->side && !memcmp(q->at, p->at, sizeof p->at);
    }
    return seen >= 3 ? SS_REPETITION : SS_PLAY;
}

int sk_winner(const skgame *g)
{
    const skpos *p = sk_pos(g);
    switch (sk_status(g)) {
    case SS_PLAY: return -1;
    case SS_STUCK: return !p->side;
    default: return p->left[0] > p->left[1] ? 0 : p->left[1] > p->left[0] ? 1 : 2;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int sk_script(skgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = sk_point(s), b;
        if (a < 0) return 0;
        s += 2;
        if (*s == '-' || *s == 'x') s++;
        if ((b = sk_point(s)) < 0) return 0;
        s += 2;
        if (*s && *s != ' ') return 0;
        skmove l[SK_MAX_MOVES];
        int n = sk_legal(sk_pos(g), l), ok = 0;
        for (int i = 0; i < n && !ok; i++)
            if (l[i].from == a && l[i].to == b) ok = sk_play(g, l[i]);
        if (!ok) return 0;
    }
    return 1;
}
