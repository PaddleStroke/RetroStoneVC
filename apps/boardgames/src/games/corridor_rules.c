/*
 * Corridor's rules (corridor.h): steps, jumps and side steps, walls that fit and leave every pawn a way home, the end.
 * No SDL: the tests and the bot link it alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/corridor.h"

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {1, 0, -1, 0};   /* CR_UP, CR_RIGHT, CR_DOWN, CR_LEFT */

/* the edge each player races to: 0 the top (rank 9), 1 the right (file i), 2 the bottom, 3 the left */
static int goal_side(const crpos *p, int player)
{
    if (p->players == 2) return player ? 2 : 0;
    return player & 3;
}

void cr_start(crpos *p, int players)
{
    static const uint8_t START4[CR_PLAYERS] = {4, 4 * 9, 8 * 9 + 4, 4 * 9 + 8};
    memset(p, 0, sizeof *p);
    p->players = (uint8_t)(players == 4 ? 4 : 2);
    p->winner = -1;
    for (int i = 0; i < p->players; i++) {
        p->pawn[i] = p->players == 2 ? (uint8_t)(i ? 8 * 9 + 4 : 4) : START4[i];
        p->left[i] = (uint8_t)(p->players == 2 ? 10 : 5);
    }
}

int cr_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '1' || s[1] > '9') return -1;
    return (s[0] - 'a') + (s[1] - '1') * CR_N;
}

int cr_goal(const crpos *p, int player, int sq)
{
    int x = sq % CR_N, y = sq / CR_N;
    switch (goal_side(p, player)) {
    case 0: return y == CR_N - 1;
    case 1: return x == CR_N - 1;
    case 2: return y == 0;
    default: return x == 0;
    }
}

static int has(const crpos *p, int sx, int sy, int o)
{
    return sx >= 0 && sx < 8 && sy >= 0 && sy < 8 && p->wall[sx + 8 * sy] == o;
}

int cr_blocked(const crpos *p, int sq, int dir)
{
    int x = sq % CR_N, y = sq / CR_N;
    switch (dir) {
    case CR_UP:    return y == CR_N - 1 || has(p, x, y, CR_H) || has(p, x - 1, y, CR_H);
    case CR_DOWN:  return y == 0 || has(p, x, y - 1, CR_H) || has(p, x - 1, y - 1, CR_H);
    case CR_RIGHT: return x == CR_N - 1 || has(p, x, y, CR_V) || has(p, x, y - 1, CR_V);
    default:       return x == 0 || has(p, x - 1, y, CR_V) || has(p, x - 1, y - 1, CR_V);
    }
}

int cr_step(int sq, int dir)
{
    int x = sq % CR_N + DX[dir], y = sq / CR_N + DY[dir];
    return x < 0 || x >= CR_N || y < 0 || y >= CR_N ? -1 : x + CR_N * y;
}

static int pawn_on(const crpos *p, int sq)
{
    for (int i = 0; i < p->players; i++)
        if (p->pawn[i] == sq) return 1;
    return 0;
}

static int add(uint8_t *out, int n, int sq)
{
    for (int i = 0; i < n; i++)
        if (out[i] == sq) return n;
    out[n] = (uint8_t)sq;
    return n + 1;
}

int cr_pawn_moves(const crpos *p, uint8_t *out)
{
    int me = p->pawn[p->turn], n = 0;
    for (int d = 0; d < 4; d++) {
        if (cr_blocked(p, me, d)) continue;
        int t = cr_step(me, d);
        if (!pawn_on(p, t)) { n = add(out, n, t); continue; }
        int b = cr_blocked(p, t, d) ? -1 : cr_step(t, d);   /* straight over it */
        if (b >= 0 && !pawn_on(p, b)) { n = add(out, n, b); continue; }
        for (int s = 1; s <= 3; s += 2) {      /* blocked behind: beside it */
            int side = (d + s) & 3;
            if (cr_blocked(p, t, side)) continue;
            int c = cr_step(t, side);
            if (!pawn_on(p, c)) n = add(out, n, c);
        }
    }
    return n;
}

int cr_wall_fits(const crpos *p, int slot, int orient)
{
    if (slot < 0 || slot >= CR_SLOTS || (orient != CR_H && orient != CR_V) || p->wall[slot]) return 0;
    int sx = slot % 8, sy = slot / 8;
    if (orient == CR_H) return !has(p, sx - 1, sy, CR_H) && !has(p, sx + 1, sy, CR_H);
    return !has(p, sx, sy - 1, CR_V) && !has(p, sx, sy + 1, CR_V);
}

int cr_path(const crpos *p, int player, uint8_t *path)
{
    uint8_t from[CR_CELLS], queue[CR_CELLS];
    memset(from, 255, sizeof from);
    int start = p->pawn[player], head = 0, tail = 0, end = -1;
    from[start] = (uint8_t)start;
    queue[tail++] = (uint8_t)start;
    while (head < tail) {
        int s = queue[head++];
        if (cr_goal(p, player, s)) { end = s; break; }
        for (int d = 0; d < 4; d++) {
            if (cr_blocked(p, s, d)) continue;
            int t = cr_step(s, d);
            if (from[t] != 255) continue;
            from[t] = (uint8_t)s;
            queue[tail++] = (uint8_t)t;
        }
    }
    if (end < 0) return -1;
    int n = 0;
    for (int s = end; s != start; s = from[s]) n++;
    if (path) {
        int i = n;
        for (int s = end;; s = from[s]) {
            path[i--] = (uint8_t)s;
            if (s == start) break;
        }
    }
    return n;
}

int cr_dist(const crpos *p, int player)
{
    int n = cr_path(p, player, NULL);
    return n < 0 ? CR_FAR : n;
}

int cr_wall_ok(const crpos *p, int slot, int orient)
{
    if (p->winner >= 0 || !p->left[p->turn] || !cr_wall_fits(p, slot, orient)) return 0;
    crpos q = *p;
    q.wall[slot] = (uint8_t)orient;
    for (int i = 0; i < q.players; i++)
        if (cr_path(&q, i, NULL) < 0) return 0;
    return 1;
}

int cr_moves(const crpos *p, int *out)
{
    if (p->winner >= 0) return 0;
    uint8_t to[8];
    int n = 0, k = cr_pawn_moves(p, to);
    for (int i = 0; i < k; i++) out[n++] = to[i];
    if (p->left[p->turn])
        for (int s = 0; s < CR_SLOTS; s++)
            for (int o = CR_H; o <= CR_V; o++)
                if (cr_wall_ok(p, s, o)) out[n++] = CR_WALL(s, o);
    if (!n) out[n++] = CR_PASS;
    return n;
}

int cr_legal(const crpos *p, int move)
{
    if (p->winner >= 0) return 0;
    if (CR_IS_WALL(move)) return cr_wall_ok(p, CR_SLOT(move), CR_ORIENT(move));
    uint8_t to[8];
    int k = cr_pawn_moves(p, to);
    if (move == CR_PASS) {
        if (k) return 0;
        if (p->left[p->turn])
            for (int s = 0; s < CR_SLOTS; s++)
                if (cr_wall_ok(p, s, CR_H) || cr_wall_ok(p, s, CR_V)) return 0;
        return 1;
    }
    for (int i = 0; i < k; i++)
        if (to[i] == move) return 1;
    return 0;
}

void cr_apply(crpos *p, int move)
{
    int me = p->turn;
    if (CR_IS_WALL(move)) {
        p->wall[CR_SLOT(move)] = (uint8_t)CR_ORIENT(move);
        p->left[me]--;
    } else if (move != CR_PASS) {
        p->pawn[me] = (uint8_t)move;
        if (cr_goal(p, me, move)) p->winner = (int8_t)me;
    }
    p->turns++;
    p->turn = (uint8_t)((me + 1) % p->players);
    if (p->winner < 0 && p->turns >= CR_TURNS * p->players) p->winner = CR_DRAW;
}

int cr_parse(const crpos *p, const char *s, int *len)
{
    (void)p;
    if (s[0] == '-') { *len = 1; return CR_PASS; }
    int sq = cr_square(s);
    if (sq < 0) return -1;
    if (s[2] == 'h' || s[2] == 'v') {
        int x = sq % CR_N, y = sq / CR_N;
        if (x > 7 || y > 7) return -1;
        *len = 3;
        return CR_WALL(x + 8 * y, s[2] == 'h' ? CR_H : CR_V);
    }
    *len = 2;
    return sq;
}

void cr_name(int move, char *out)
{
    if (move == CR_PASS) { out[0] = '-'; out[1] = 0; return; }
    if (CR_IS_WALL(move)) {
        int s = CR_SLOT(move);
        out[0] = (char)('a' + s % 8);
        out[1] = (char)('1' + s / 8);
        out[2] = CR_ORIENT(move) == CR_H ? 'h' : 'v';
        out[3] = 0;
        return;
    }
    out[0] = (char)('a' + move % CR_N);
    out[1] = (char)('1' + move / CR_N);
    out[2] = 0;
}

void cr_new(crgame *g, int players)
{
    g->ply = 0;
    cr_start(&g->pos[0], players);
}

int cr_play(crgame *g, int move)
{
    if (g->ply >= CR_MAX_PLY || !cr_legal(cr_pos(g), move)) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    cr_apply(&g->pos[g->ply + 1], move);
    g->move[g->ply] = (int16_t)move;
    g->ply++;
    return 1;
}

int cr_undo(crgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int cr_script(crgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, m = cr_parse(cr_pos(g), s, &len);
        if (m < 0 || (s[len] && s[len] != ' ') || !cr_play(g, m)) return 0;
        s += len;
    }
    return 1;
}
