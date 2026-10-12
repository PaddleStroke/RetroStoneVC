/*
 * Dara's rules (dara.h): placing without rows of three, moving, the rows of exactly three and their captures. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/dara.h"

/* the run of the piece on hole h along a row (dx 1) or a column (dy 1): its holes */
static uint32_t run(const darpos *p, int h, int dx, int dy)
{
    int who = p->c[h], x0 = h % DAR_W, y0 = h / DAR_W;
    uint32_t r = 1u << h;
    for (int s = -1; s <= 1; s += 2)
        for (int x = x0 + s * dx, y = y0 + s * dy; x >= 0 && y >= 0 && x < DAR_W && y < DAR_H;
             x += s * dx, y += s * dy) {
            if (p->c[DAR_AT(x, y)] != who) break;
            r |= 1u << DAR_AT(x, y);
        }
    return r;
}

static int bits(uint32_t v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

uint32_t dar_line(const darpos *p, int h)
{
    if (!p->c[h]) return 0;
    uint32_t a = run(p, h, 1, 0), b = run(p, h, 0, 1), l = 0;
    if (bits(a) == 3) l |= a;
    if (bits(b) == 3) l |= b;
    return l;
}

int dar_can_place(const darpos *p, int player, int h)
{
    if (p->c[h]) return 0;
    darpos q = *p;
    q.c[h] = (uint8_t)(player + 1);
    return bits(run(&q, h, 1, 0)) < 3 && bits(run(&q, h, 0, 1)) < 3;
}

static int places(const darpos *p, int player)
{
    if (!p->hand[player]) return 0;
    for (int h = 0; h < DAR_N; h++)
        if (dar_can_place(p, player, h)) return 1;
    return 0;
}

static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

int dar_legal(const darpos *p, darmove *out)
{
    int n = 0, me = p->side + 1;
    for (int h = 0; h < DAR_N; h++) {
        if (p->phase == DAR_PLACE) {
            if (p->hand[p->side] && dar_can_place(p, p->side, h)) out[n++] = (darmove){DAR_PLACE, (uint8_t)h,
                                                                                       (uint8_t)h};
        } else if (p->phase == DAR_TAKE) {
            if (p->c[h] && p->c[h] != me) out[n++] = (darmove){DAR_TAKE, (uint8_t)h, (uint8_t)h};
        } else if (p->c[h] == me)
            for (int d = 0; d < 4; d++) {
                int x = h % DAR_W + DX[d], y = h / DAR_W + DY[d];
                if (x >= 0 && y >= 0 && x < DAR_W && y < DAR_H && !p->c[DAR_AT(x, y)])
                    out[n++] = (darmove){DAR_MOVE, (uint8_t)h, (uint8_t)DAR_AT(x, y)};
            }
    }
    return n;
}

void dar_make(darpos *p, darmove m)
{
    p->line = 0;
    if (m.kind == DAR_PLACE) {
        p->c[m.to] = (uint8_t)(p->side + 1);
        p->hand[p->side]--;
        int other = !p->side;                   /* the other places next if they can, else the same, else moving */
        if (places(p, other)) p->side = (uint8_t)other;
        else if (!places(p, p->side)) {
            p->phase = DAR_MOVE;
            p->side = (uint8_t)other;
            p->hand[0] = p->hand[1] = 0;
        }
        return;
    }
    if (m.kind == DAR_TAKE) {
        p->c[m.to] = 0;
        p->phase = DAR_MOVE;
        p->quiet = 0;
        p->side ^= 1;
        return;
    }
    p->c[m.to] = p->c[m.from];
    p->c[m.from] = 0;
    p->quiet++;
    p->line = dar_line(p, m.to);
    if (p->line) p->phase = DAR_TAKE;           /* the same player takes */
    else p->side ^= 1;
}

int dar_count(const darpos *p, int player)
{
    int n = 0;
    for (int h = 0; h < DAR_N; h++) n += p->c[h] == player + 1;
    return n;
}

int dar_hole(const char *s)
{
    int x = s[0] - 'a', y = s[1] - '1';
    return x >= 0 && y >= 0 && x < DAR_W && y < DAR_H ? DAR_AT(x, y) : -1;
}

void dar_new(dargame *g)
{
    darpos *p = &g->pos[0];
    memset(p, 0, sizeof *p);
    p->hand[0] = p->hand[1] = DAR_PIECES;
    g->ply = 0;
}

int dar_status(const dargame *g)
{
    const darpos *p = dar_pos(g);
    if (p->phase != DAR_MOVE) return DAR_PLAY;
    darmove m[DAR_MAX_MOVES];
    if (dar_count(p, p->side) < 3 || !dar_legal(p, m)) return p->side ? DAR_WIN0 : DAR_WIN1;
    if (p->quiet >= DAR_QUIET) return DAR_DRAW;
    return DAR_PLAY;
}

int dar_play(dargame *g, darmove m)
{
    if (g->ply >= DAR_MAX_PLY || dar_status(g) != DAR_PLAY) return 0;
    darmove l[DAR_MAX_MOVES];
    int n = dar_legal(dar_pos(g), l), ok = 0;
    for (int i = 0; i < n && !ok; i++) ok = l[i].kind == m.kind && l[i].to == m.to && (m.kind != DAR_MOVE ||
                                                                                     l[i].from == m.from);
    if (!ok) return 0;
    if (m.kind != DAR_MOVE) m.from = m.to;
    g->pos[g->ply + 1] = g->pos[g->ply];
    dar_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int dar_undo(dargame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int dar_script(dargame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        const darpos *p = dar_pos(g);
        if (*s == 'x') s++;
        int a = dar_hole(s);
        if (a < 0) return 0;
        darmove m = {p->phase, (uint8_t)a, (uint8_t)a};
        if (p->phase == DAR_MOVE) {
            int b = s[1] ? dar_hole(s + 2) : -1;
            if (b < 0) return 0;
            m.to = (uint8_t)b;
            s += 2;
        }
        if (!dar_play(g, m)) return 0;
        s += 2;
    }
    return 1;
}
