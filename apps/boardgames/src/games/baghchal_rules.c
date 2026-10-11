/*
 * Bagh-Chal: the rules (no SDL; tested by tests/baghchal_test.c). The board, the notation and the rules: baghchal.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/baghchal.h"

const int8_t bc_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, bc_dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};

int bc_next(int pt, int d)
{
    int x = bc_x(pt) + bc_dx[d], y = bc_y(pt) + bc_dy[d];
    if (bc_dx[d] && bc_dy[d] && ((bc_x(pt) + bc_y(pt)) & 1)) return -1;   /* no diagonal through this point */
    return x < 0 || y < 0 || x >= BC_SIDE || y >= BC_SIDE ? -1 : x + BC_SIDE * y;
}

int bc_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'e' || s[1] < '1' || s[1] > '5') return -1;
    return (s[0] - 'a') + BC_SIDE * (s[1] - '1');
}

void bc_start(bcpos *p)
{
    memset(p, 0, sizeof *p);
    memset(p->at, -1, sizeof p->at);
    p->at[0] = p->at[BC_SIDE - 1] = p->at[BC_POINTS - BC_SIDE] = p->at[BC_POINTS - 1] = BC_TIGER;
    p->side = BC_GOAT;
}

int bc_legal(const bcpos *p, bcmove *out)
{
    int n = 0;
    if (p->side == BC_GOAT && p->placed < BC_GOATS) {   /* a goat to set down: any empty point */
        for (int i = 0; i < BC_POINTS; i++)
            if (p->at[i] < 0) out[n++] = (bcmove){-1, (int8_t)i, -1};
        return n;
    }
    for (int i = 0; i < BC_POINTS; i++) {
        if (p->at[i] != p->side) continue;
        for (int d = 0; d < 8; d++) {
            int to = bc_next(i, d);
            if (to < 0) continue;
            if (p->at[to] < 0) {
                out[n++] = (bcmove){(int8_t)i, (int8_t)to, -1};
            } else if (p->side == BC_TIGER && p->at[to] == BC_GOAT) {   /* a jump over the goat, never a tiger */
                int land = bc_next(to, d);
                if (land >= 0 && p->at[land] < 0) out[n++] = (bcmove){(int8_t)i, (int8_t)land, (int8_t)to};
            }
        }
    }
    return n;
}

void bc_make(bcpos *p, bcmove m)
{
    if (m.from < 0) {
        p->at[m.to] = BC_GOAT;
        p->placed++;
        p->quiet = 0;
    } else {
        p->at[m.to] = p->at[m.from];
        p->at[m.from] = -1;
        if (m.over >= 0) {
            p->at[m.over] = -1;
            p->taken++;
            p->quiet = 0;
        } else {
            p->quiet++;
        }
    }
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void bc_new(bcgame *g)
{
    g->ply = 0;
    bc_start(&g->pos[0]);
}

int bc_play(bcgame *g, bcmove m)
{
    if (g->ply >= BC_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    bc_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int bc_undo(bcgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

bcstatus bc_status(const bcgame *g)
{
    const bcpos *p = bc_pos(g);
    if (p->taken >= BC_TIGERS_WIN) return BS_FIVE;
    bcmove m[BC_MAX_MOVES];
    if (!bc_legal(p, m)) return p->side == BC_TIGER ? BS_TRAPPED : BS_STUCK;
    if (p->quiet >= BC_QUIET) return BS_QUIET;
    int seen = 0;                               /* back to the last capture or goat set down: nothing before repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) {
        const bcpos *q = &g->pos[i];
        seen += q->side == p->side && !memcmp(q->at, p->at, sizeof p->at);
    }
    return seen >= 3 ? BS_REPETITION : BS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int bc_script(bcgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = bc_point(s), b = -1;
        if (a < 0) return 0;
        s += 2;
        if (*s && *s != ' ') {                  /* a1-a2, a1a2, a1xc3: a move; c3 alone: a goat set down */
            if (*s == '-' || *s == 'x') s++;
            if ((b = bc_point(s)) < 0) return 0;
            s += 2;
            if (*s && *s != ' ') return 0;
        }
        bcmove l[BC_MAX_MOVES];
        int n = bc_legal(bc_pos(g), l), k = 0;
        for (; k < n; k++)
            if (b < 0 ? l[k].from < 0 && l[k].to == a : l[k].from == a && l[k].to == b) break;
        if (k == n || !bc_play(g, l[k])) return 0;
    }
    return 1;
}
