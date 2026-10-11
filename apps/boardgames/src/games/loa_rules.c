/*
 * Lines of Action's rules (loa.h): the moves counted along their lines, the groups, the end, the game's history.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/loa.h"

static const int DIR[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};

static int on_board(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }

int loa_square(const char *s)
{
    if (!s[0]) return -1;
    int f = (s[0] | 32) - 'a', r = s[1] - '1';
    return f >= 0 && f < 8 && r >= 0 && r < 8 ? r * 8 + f : -1;
}

void loa_start(lpos *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 1; i < 7; i++) {
        p->at[i] = p->at[56 + i] = 1;           /* ebony: b1..g1, b8..g8 */
        p->at[i * 8] = p->at[i * 8 + 7] = 2;    /* ivory: a2..a7, h2..h7 */
    }
}

int loa_line(const lpos *p, int s, int dx, int dy)
{
    int n = 1, x0 = s % 8, y0 = s / 8;
    for (int k = -1; k <= 1; k += 2)            /* both ways from the piece */
        for (int x = x0 + k * dx, y = y0 + k * dy; on_board(x, y); x += k * dx, y += k * dy) n += p->at[y * 8 + x] != 0;
    return n;
}

int loa_legal(const lpos *p, int side, lmove *out)
{
    int n = 0, own = side + 1;
    for (int s = 0; s < LOA_SQUARES; s++) {
        if (p->at[s] != own) continue;
        for (int d = 0; d < 8; d++) {
            int dx = DIR[d][0], dy = DIR[d][1], len = loa_line(p, s, dx, dy), x = s % 8, y = s / 8, ok = 1;
            for (int k = 1; k < len && ok; k++)  /* over its own pieces, not over the enemy's */
                ok = on_board(x + k * dx, y + k * dy) && p->at[(y + k * dy) * 8 + x + k * dx] != (own ^ 3);
            int tx = x + len * dx, ty = y + len * dy;
            if (!ok || !on_board(tx, ty) || p->at[ty * 8 + tx] == own) continue;
            out[n++] = (lmove){(int8_t)s, (int8_t)(ty * 8 + tx), (int8_t)(p->at[ty * 8 + tx] != 0)};
        }
    }
    return n;
}

uint64_t loa_group(const lpos *p, int s)
{
    if (s < 0 || s >= LOA_SQUARES || !p->at[s]) return 0;
    int stack[LOA_SQUARES], top = 0, who = p->at[s];
    uint64_t seen = 1ull << s;
    stack[top++] = s;
    while (top) {
        int c = stack[--top], x = c % 8, y = c / 8;
        for (int d = 0; d < 8; d++) {
            int nx = x + DIR[d][0], ny = y + DIR[d][1], t = ny * 8 + nx;
            if (!on_board(nx, ny) || p->at[t] != who || (seen >> t & 1)) continue;
            seen |= 1ull << t;
            stack[top++] = t;
        }
    }
    return seen;
}

int loa_joined(const lpos *p, int player)
{
    int first = -1, count = 0;
    for (int s = 0; s < LOA_SQUARES; s++)
        if (p->at[s] == player + 1) {
            if (first < 0) first = s;
            count++;
        }
    if (first < 0) return 0;
    uint64_t g = loa_group(p, first);
    int n = 0;
    for (; g; g &= g - 1) n++;
    return n == count;
}

void loa_make(lpos *p, lmove m)
{
    int mover = p->side;
    lmove any[LOA_MAX_MOVES];
    p->at[m.to] = p->at[m.from];
    p->at[m.from] = 0;
    p->passed = 0;
    p->result = loa_joined(p, mover) ? LOA_WIN0 + mover      /* both joined at once: the mover wins */
              : loa_joined(p, !mover) ? LOA_WIN0 + !mover : LOA_PLAY;
    if (p->result != LOA_PLAY) { p->side = (uint8_t)!mover; return; }
    if (loa_legal(p, !mover, any)) p->side = (uint8_t)!mover;
    else if (loa_legal(p, mover, any)) p->passed = 1;          /* the other has no move: the turn comes back */
    else p->result = LOA_DRAW;
}

void loa_new(lgame *g)
{
    g->ply = 0;
    loa_start(&g->pos[0]);
}

int loa_play(lgame *g, lmove m)
{
    const lpos *p = loa_pos(g);
    lmove legal[LOA_MAX_MOVES];
    int n = p->result == LOA_PLAY && g->ply < LOA_MAX_PLY ? loa_legal(p, p->side, legal) : 0, i = 0;
    while (i < n && !(legal[i].from == m.from && legal[i].to == m.to)) i++;
    if (i == n) return 0;
    g->pos[g->ply + 1] = *p;
    g->move[g->ply] = legal[i];
    loa_make(&g->pos[++g->ply], legal[i]);
    if (g->ply == LOA_MAX_PLY && loa_pos(g)->result == LOA_PLAY) loa_pos(g)->result = LOA_DRAW;
    return 1;
}

int loa_undo(lgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int loa_find(const lmove *legal, int n, const char *s, int *used)
{
    const char *c = s;
    int from = loa_square(c);
    if (from < 0) return -1;
    c += 2;
    if (*c == '-' || *c == 'x' || *c == 'X') c++;
    if (!c[0] || !c[1]) return -1;
    int to = loa_square(c);
    for (int i = 0; to >= 0 && i < n; i++)
        if (legal[i].from == from && legal[i].to == to) {
            *used = (int)(c + 2 - s);
            return i;
        }
    return -1;
}
