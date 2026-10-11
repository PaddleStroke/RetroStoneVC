/*
 * Konane: the rules (no SDL; tested by tests/konane_test.c). The board, the notation and the rules: konane.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/konane.h"

static const int8_t DX[4] = {0, 1, 0, -1}, DY[4] = {1, 0, -1, 0};

int kn_size(int variant) { return variant == 0 ? 6 : variant == 2 ? 10 : 8; }

int kn_square(int n, const char *s, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + n || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') y = 10 * y + s[2] - '0', k = 3;
    if (y > n) return -1;
    if (len) *len = k;
    return (s[0] - 'a') + n * (y - 1);
}

int kn_owner(int n, int sq) { return ((sq % n + sq / n) & 1) ? 0 : 1; }

void kn_start(knpos *p, int n)
{
    memset(p, 0, sizeof *p);
    memset(p->at, -1, sizeof p->at);
    p->n = (uint8_t)n;
    for (int i = 0; i < n * n; i++) p->at[i] = (int8_t)kn_owner(n, i);
}

/* the square d steps from sq in direction k; -1 off the board */
static int step(int n, int sq, int k, int d)
{
    int x = sq % n + DX[k] * d, y = sq / n + DY[k] * d;
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : x + n * y;
}

int kn_legal(const knpos *p, knmove *out)
{
    int n = p->n, c = 0, h = n / 2;
    if (p->opening == 0) {                      /* player 0 lifts a stone: the middle four, or a corner */
        const int sq[8] = {h - 1 + n * (h - 1), h + n * (h - 1), h - 1 + n * h, h + n * h,
                           0, n - 1, n * (n - 1), n * n - 1};
        for (int i = 0; i < 8; i++)
            if (p->at[sq[i]] == 0) out[c++] = (knmove){(int8_t)sq[i], (int8_t)sq[i]};
        return c;
    }
    if (p->opening == 1) {                      /* player 1 lifts a stone next to the hole */
        int hole = -1;
        for (int i = 0; i < n * n && hole < 0; i++)
            if (p->at[i] < 0) hole = i;
        for (int k = 0; k < 4; k++) {
            int s = step(n, hole, k, 1);
            if (s >= 0 && p->at[s] == 1) out[c++] = (knmove){(int8_t)s, (int8_t)s};
        }
        return c;
    }
    for (int s = 0; s < n * n; s++) {
        if (p->at[s] != p->side) continue;
        for (int k = 0; k < 4; k++)             /* jumps straight on, one more each time */
            for (int d = 2;; d += 2) {
                int over = step(n, s, k, d - 1), to = step(n, s, k, d);
                if (to < 0 || p->at[over] != !p->side || p->at[to] >= 0) break;
                out[c++] = (knmove){(int8_t)s, (int8_t)to};
            }
    }
    return c;
}

int kn_jumped(const knpos *p, knmove m, int8_t *out)
{
    int n = p->n, dx = m.to % n - m.from % n, dy = m.to / n - m.from / n, len = abs(dx) + abs(dy);
    if (m.from == m.to || (dx && dy)) return 0;
    int k = dy > 0 ? 0 : dx > 0 ? 1 : dy < 0 ? 2 : 3, c = 0;
    for (int d = 1; d < len; d += 2) out[c++] = (int8_t)step(n, m.from, k, d);
    return c;
}

void kn_make(knpos *p, knmove m)
{
    if (m.from == m.to) {                       /* an opening removal */
        p->at[m.from] = -1;
        p->opening++;
    } else {
        int8_t over[KN_MAX_N];
        int c = kn_jumped(p, m, over);
        for (int i = 0; i < c; i++) p->at[over[i]] = -1;
        p->at[m.to] = p->at[m.from];
        p->at[m.from] = -1;
        p->took[p->side] += (uint8_t)c;
    }
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void kn_new(kngame *g, int n)
{
    g->ply = 0;
    kn_start(&g->pos[0], n);
}

int kn_play(kngame *g, knmove m)
{
    if (g->ply >= KN_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    kn_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int kn_undo(kngame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int kn_winner(const kngame *g)
{
    knmove m[KN_MAX_MOVES];
    const knpos *p = kn_pos(g);
    return kn_legal(p, m) ? -1 : !p->side;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int kn_script(kngame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        const knpos *p = kn_pos(g);
        int n = p->n, len, a = kn_square(n, s, &len), b = a;
        if (a < 0) return 0;
        s += len;
        while (*s == '-' || *s == 'x') {        /* c4-e4-g4: the landing squares, straight on */
            int c = kn_square(n, s + 1, &len);
            if (c < 0) return 0;
            int dx = c % n - b % n, dy = c / n - b / n, ax = b % n - a % n, ay = b / n - a / n;
            if ((dx && dy) || (!dx && !dy) || (b != a && (dx * ax < 0 || dy * ay < 0 || (dx && ay) || (dy && ax))))
                return 0;                       /* not a straight line on from the last landing */
            b = c;
            s += 1 + len;
        }
        if (*s && *s != ' ') return 0;
        knmove l[KN_MAX_MOVES];
        int nl = kn_legal(p, l), found = 0;
        for (int i = 0; i < nl && !found; i++)
            if (l[i].from == a && l[i].to == b) found = kn_play(g, l[i]);
        if (!found) return 0;
    }
    return 1;
}
