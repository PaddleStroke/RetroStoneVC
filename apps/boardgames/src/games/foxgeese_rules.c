/*
 * Fox and geese: the rules (no SDL; tested by tests/foxgeese_test.c). The board, the notation and the rules:
 * foxgeese.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/foxgeese.h"

const int8_t fg_x[FG_POINTS] = {2, 3, 4, 2, 3, 4, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6,
                                2, 3, 4, 2, 3, 4};
const int8_t fg_y[FG_POINTS] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
                                5, 5, 5, 6, 6, 6};
const int8_t fg_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, fg_dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};
int8_t fg_next[FG_POINTS][8];

static int at_xy(int x, int y)
{
    for (int i = 0; i < FG_POINTS; i++)
        if (fg_x[i] == x && fg_y[i] == y) return i;
    return -1;
}

void fg_init(void)
{
    for (int i = 0; i < FG_POINTS; i++)
        for (int d = 0; d < 8; d++) {
            int odd = fg_dx[d] && fg_dy[d] && ((fg_x[i] + fg_y[i]) & 1);   /* no diagonal through this point */
            fg_next[i][d] = odd ? -1 : (int8_t)at_xy(fg_x[i] + fg_dx[d], fg_y[i] + fg_dy[d]);
        }
}

int fg_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'g' || s[1] < '1' || s[1] > '7') return -1;
    return at_xy(s[0] - 'a', s[1] - '1');
}

void fg_start(fgpos *p)
{
    memset(p, 0, sizeof *p);
    memset(p->at, -1, sizeof p->at);
    for (int i = 0; i < FG_POINTS; i++)
        if (fg_y[i] <= 2) p->at[i] = FG_GOOSE;
    p->fox = (int8_t)at_xy(3, 3);
    p->at[p->fox] = FG_FOX;
    p->geese = FG_GEESE;
}

/* the fox's jumps from its point */
static int jumps(const fgpos *p, fgmove *out)
{
    int n = 0, f = p->fox;
    for (int d = 0; d < 8; d++) {
        int over = fg_next[f][d], to = over >= 0 ? fg_next[over][d] : -1;
        if (to >= 0 && p->at[over] == FG_GOOSE && p->at[to] < 0)
            out[n++] = (fgmove){(int8_t)f, (int8_t)to, (int8_t)over};
    }
    return n;
}

int fg_legal(const fgpos *p, fgmove *out)
{
    int n = 0;
    if (p->side == FG_FOX) {
        if (!p->chain)
            for (int d = 0; d < 8; d++) {
                int to = fg_next[p->fox][d];
                if (to >= 0 && p->at[to] < 0) out[n++] = (fgmove){p->fox, (int8_t)to, -1};
            }
        return n + jumps(p, out + n);
    }
    for (int i = 0; i < FG_POINTS; i++) {       /* a goose: forwards (dy 1) or sideways (dy 0) */
        if (p->at[i] != FG_GOOSE) continue;
        for (int d = 0; d < 8; d++) {
            int to = fg_next[i][d];
            if (fg_dy[d] >= 0 && to >= 0 && p->at[to] < 0) out[n++] = (fgmove){(int8_t)i, (int8_t)to, -1};
        }
    }
    return n;
}

void fg_make(fgpos *p, fgmove m)
{
    int who = p->at[m.from];
    p->at[m.from] = -1;
    p->at[m.to] = (int8_t)who;
    p->chain = 0;
    if (who == FG_FOX) {
        p->fox = m.to;
        if (m.over >= 0) {
            p->at[m.over] = -1;
            p->geese--;
            p->quiet = 0;
            fgmove more[8];
            if (jumps(p, more)) { p->chain = 1; return; }   /* the fox jumps on: still its turn */
        } else {
            p->quiet++;
        }
    } else {
        p->quiet = fg_y[m.to] > fg_y[m.from] ? 0 : p->quiet + 1;
    }
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void fg_new(fggame *g)
{
    fg_init();
    g->ply = 0;
    fg_start(&g->pos[0]);
}

int fg_play(fggame *g, fgmove m)
{
    if (g->ply >= FG_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    fg_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int fg_undo(fggame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

fgstatus fg_status(const fggame *g)
{
    const fgpos *p = fg_pos(g);
    if (p->geese < FG_FOX_WINS) return FS_FEW;
    if (p->chain) return FS_PLAY;
    fgmove m[FG_MAX_MOVES];
    if (!fg_legal(p, m)) return p->side == FG_FOX ? FS_TRAPPED : FS_STUCK;
    if (p->quiet >= FG_QUIET) return FS_QUIET;
    int seen = 0;                               /* back to the last capture or forward move: nothing before repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) {
        const fgpos *q = &g->pos[i];
        seen += q->side == p->side && q->chain == p->chain && !memcmp(q->at, p->at, sizeof p->at);
    }
    return seen >= 3 ? FS_REPETITION : FS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int play_one(fggame *g, int from, int to)
{
    fgmove l[FG_MAX_MOVES];
    int n = fg_legal(fg_pos(g), l);
    for (int i = 0; i < n; i++)
        if (l[i].from == from && l[i].to == to) return fg_play(g, l[i]);
    return 0;
}

int fg_script(fggame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = fg_point(s);
        if (a < 0) return 0;
        s += 2;
        do {                                    /* d4-d2, d4d2, d4xd2xb2: a move to each next point */
            if (*s == '-' || *s == 'x') s++;
            int b = fg_point(s);
            if (b < 0 || !play_one(g, a, b)) return 0;
            s += 2;
            a = b;
        } while (*s && *s != ' ');
    }
    return 1;
}
