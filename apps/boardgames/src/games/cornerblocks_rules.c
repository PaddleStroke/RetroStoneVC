/*
 * Corner Blocks' rules (cornerblocks.h): the 21 blocks and their turns, where a block may go, the turn passing over
 * the players who are out, the scores.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The blocks, a..u: the single square; the domino; the straight and bent threes; the five fours (straight, L, T,
 * square, S); the twelve fives (straight, L, Y, N, P, U, V, W, Z, T, F, X).
 */
#include <stdio.h>
#include <string.h>

#include "games/cornerblocks.h"

static const struct { uint8_t n; int8_t c[5][2]; } BASE[CBL_BLOCKS] = {
    {1, {{0, 0}}},
    {2, {{0, 0}, {1, 0}}},
    {3, {{0, 0}, {1, 0}, {2, 0}}},
    {3, {{0, 0}, {0, 1}, {1, 1}}},
    {4, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}},
    {4, {{0, 0}, {0, 1}, {0, 2}, {1, 2}}},
    {4, {{0, 0}, {1, 0}, {2, 0}, {1, 1}}},
    {4, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}},
    {4, {{1, 0}, {2, 0}, {0, 1}, {1, 1}}},
    {5, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}},
    {5, {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}}},
    {5, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {1, 1}}},
    {5, {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {3, 1}}},
    {5, {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 2}}},
    {5, {{0, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}}},
    {5, {{0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}}},
    {5, {{0, 0}, {0, 1}, {1, 1}, {1, 2}, {2, 2}}},
    {5, {{0, 0}, {1, 0}, {1, 1}, {1, 2}, {2, 2}}},
    {5, {{0, 0}, {1, 0}, {2, 0}, {1, 1}, {1, 2}}},
    {5, {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {5, {{1, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 2}}},
};

static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
static const int DXD[4] = {1, 1, -1, -1}, DYD[4] = {1, -1, 1, -1};

int cbl_size(int block) { return BASE[block].n; }

void cbl_shape_of(int block, int o, cbl_shape *s)
{
    int n = BASE[block].n, mx = 99, my = 99, X[5], Y[5];
    for (int i = 0; i < n; i++) {
        int x = BASE[block].c[i][0], y = BASE[block].c[i][1];
        if (o >= 4) x = -x;
        for (int k = 0; k < (o & 3); k++) {     /* a quarter clockwise (y down): (x, y) -> (-y, x) */
            int t = x;
            x = -y;
            y = t;
        }
        X[i] = x, Y[i] = y;
        mx = x < mx ? x : mx;
        my = y < my ? y : my;
    }
    for (int i = 0; i < n; i++) X[i] -= mx, Y[i] -= my;
    for (int i = 1; i < n; i++)                 /* in reading order: two shapes compare square by square */
        for (int j = i; j > 0 && (Y[j] < Y[j - 1] || (Y[j] == Y[j - 1] && X[j] < X[j - 1])); j--) {
            int t = X[j]; X[j] = X[j - 1]; X[j - 1] = t;
            t = Y[j]; Y[j] = Y[j - 1]; Y[j - 1] = t;
        }
    s->n = (uint8_t)n;
    s->w = s->h = 0;
    for (int i = 0; i < n; i++) {
        s->x[i] = (int8_t)X[i], s->y[i] = (int8_t)Y[i];
        if (X[i] + 1 > s->w) s->w = (uint8_t)(X[i] + 1);
        if (Y[i] + 1 > s->h) s->h = (uint8_t)(Y[i] + 1);
    }
}

int cbl_same(int block, int o)
{
    cbl_shape a, b;
    cbl_shape_of(block, o, &a);
    for (int k = 0; k < o; k++) {
        cbl_shape_of(block, k, &b);
        if (a.w == b.w && a.h == b.h && !memcmp(a.x, b.x, a.n) && !memcmp(a.y, b.y, a.n)) return k;
    }
    return o;
}

int cbl_unique(int block, int o) { return cbl_same(block, o) == o; }

int cbl_start(const cblpos *p, int seat)
{
    int n = p->n, x, y;
    if (n == 14) x = seat ? 9 : 4, y = seat ? 4 : 9;
    else x = seat >= 2 ? n - 1 : 0, y = seat == 1 || seat == 2 ? 0 : n - 1;
    return y * CBL_MAXN + x;
}

void cbl_new(cblpos *p, int seats)
{
    memset(p, 0, sizeof *p);
    p->seats = (uint8_t)(seats >= 2 && seats <= 4 ? seats : 2);
    p->n = p->seats == 2 ? 14 : 20;
    for (int s = 0; s < p->seats; s++) p->left[s] = CBL_ALL, p->last[s] = 255;
}

int cbl_move(int block, int o, int anchor) { return (block * 8 + o) * CBL_SQ + anchor; }
int cbl_block(int move) { return move / CBL_SQ / 8; }
int cbl_orient(int move) { return move / CBL_SQ % 8; }
int cbl_anchor(int move) { return move % CBL_SQ; }

static int own_at(const cblpos *p, int x, int y, int me)
{
    return x >= 0 && y >= 0 && x < p->n && y < p->n && p->cell[y * CBL_MAXN + x] == me;
}

/* the shape s on the anchor (ax, ay) for the player to move */
static int fits_shape(const cblpos *p, const cbl_shape *s, int ax, int ay)
{
    int me = p->turn + 1, n = p->n, corner = 0, first = p->left[p->turn] == CBL_ALL, start = cbl_start(p, p->turn);
    if (ax < 0 || ay < 0 || ax + s->w > n || ay + s->h > n) return 0;
    for (int i = 0; i < s->n; i++) {
        int x = ax + s->x[i], y = ay + s->y[i], c = y * CBL_MAXN + x;
        if (p->cell[c]) return 0;
        for (int k = 0; k < 4; k++)
            if (own_at(p, x + DX4[k], y + DY4[k], me)) return 0;
        if (first) corner |= c == start;
        else
            for (int k = 0; k < 4 && !corner; k++) corner = own_at(p, x + DXD[k], y + DYD[k], me);
    }
    return corner;
}

int cbl_fits(const cblpos *p, int block, int o, int anchor)
{
    if (p->over || block < 0 || block >= CBL_BLOCKS || o < 0 || o > 7 || !(p->left[p->turn] >> block & 1)) return 0;
    if (anchor < 0 || anchor >= CBL_SQ) return 0;
    cbl_shape s;
    cbl_shape_of(block, o, &s);
    return fits_shape(p, &s, anchor % CBL_MAXN, anchor / CBL_MAXN);
}

int cbl_legal(const cblpos *p, int move)
{
    return move >= 0 && move < CBL_MOVES && cbl_fits(p, cbl_block(move), cbl_orient(move), cbl_anchor(move));
}

int cbl_corners(const cblpos *p, int seat, uint8_t *mark)
{
    int n = p->n, me = seat + 1, k = 0;
    if (mark) memset(mark, 0, CBL_SQ);
    if (p->left[seat] == CBL_ALL) {
        int s = cbl_start(p, seat);
        if (p->cell[s]) return 0;
        if (mark) mark[s] = 1;
        return 1;
    }
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            if (p->cell[y * CBL_MAXN + x]) continue;
            int edge = 0, diag = 0;
            for (int d = 0; d < 4; d++) {
                edge |= own_at(p, x + DX4[d], y + DY4[d], me);
                diag |= own_at(p, x + DXD[d], y + DYD[d], me);
            }
            if (diag && !edge) {
                if (mark) mark[y * CBL_MAXN + x] = 1;
                k++;
            }
        }
    return k;
}

/* the moves of the player to move (cap 0: is there one?); each distinct shape once, found from the corner squares */
static int gen(const cblpos *p, int *out, int cap)
{
    uint8_t mark[CBL_SQ];
    if (p->over || !cbl_corners(p, p->turn, mark)) return 0;
    uint8_t seen[CBL_MOVES / 8 + 1];
    if (cap) memset(seen, 0, sizeof seen);
    int k = 0;
    for (int b = CBL_BLOCKS - 1; b >= 0; b--) {
        if (!(p->left[p->turn] >> b & 1)) continue;
        for (int o = 0; o < 8; o++) {
            if (!cbl_unique(b, o)) continue;
            cbl_shape s;
            cbl_shape_of(b, o, &s);
            for (int c = 0; c < CBL_SQ; c++) {
                if (!mark[c]) continue;
                for (int i = 0; i < s.n; i++) {  /* each square of the block on the corner square */
                    int ax = c % CBL_MAXN - s.x[i], ay = c / CBL_MAXN - s.y[i];
                    if (!fits_shape(p, &s, ax, ay)) continue;
                    if (!cap) return 1;
                    int m = cbl_move(b, o, ay * CBL_MAXN + ax);
                    if (seen[m >> 3] >> (m & 7) & 1) continue;
                    seen[m >> 3] |= (uint8_t)(1 << (m & 7));
                    if (k < cap) out[k++] = m;
                }
            }
        }
    }
    return k;
}

int cbl_moves(const cblpos *p, int *out, int cap) { return cap > 0 ? gen(p, out, cap) : 0; }

int cbl_can_move(const cblpos *p, int seat)
{
    if (p->out[seat] || !p->left[seat]) return 0;
    cblpos q = *p;
    q.turn = (uint8_t)seat;
    return gen(&q, NULL, 0);
}

void cbl_apply(cblpos *p, int move)
{
    int b = cbl_block(move), a = cbl_anchor(move), me = p->turn;
    cbl_shape s;
    cbl_shape_of(b, cbl_orient(move), &s);
    for (int i = 0; i < s.n; i++) p->cell[a + s.y[i] * CBL_MAXN + s.x[i]] = (uint8_t)(me + 1);
    p->left[me] &= ~(1u << b);
    p->last[me] = (uint8_t)b;
    p->laid[me] = (uint16_t)(p->laid[me] + s.n);
    for (int k = 1; k <= p->seats; k++) {       /* the next player who can still play; the others are out */
        int q = (me + k) % p->seats;
        if (p->out[q]) continue;
        if (cbl_can_move(p, q)) { p->turn = (uint8_t)q; return; }
        p->out[q] = 1;
    }
    p->over = 1;
}

int cbl_score(const cblpos *p, int seat)
{
    int s = p->laid[seat];
    if (!p->left[seat]) s += 15 + (p->last[seat] == CBL_MONO ? 5 : 0);
    return s;
}

unsigned cbl_winners(const cblpos *p)
{
    int best = -1;
    unsigned w = 0;
    for (int s = 0; s < p->seats; s++) {
        int v = cbl_score(p, s);
        if (v > best) best = v, w = 0;
        if (v == best) w |= 1u << s;
    }
    return w;
}

int cbl_parse(const cblpos *p, const char *s, int *len)
{
    int b = s[0] - 'a', o = s[1] - '0', x = s[1] ? s[2] - 'a' : -1, y = 0, k = 3;
    if (b < 0 || b >= CBL_BLOCKS || o < 0 || o > 7 || x < 0 || x >= p->n) { *len = 1; return -1; }
    while (k < 5 && s[k] >= '0' && s[k] <= '9') y = y * 10 + s[k++] - '0';
    *len = k;
    if (k == 3 || y < 1 || y > p->n) return -1;
    return cbl_move(b, o, (y - 1) * CBL_MAXN + x);
}

void cbl_name(const cblpos *p, int move, char *out)
{
    (void)p;
    int a = cbl_anchor(move);
    snprintf(out, 8, "%c%d%c%d", 'a' + cbl_block(move), cbl_orient(move), 'a' + a % CBL_MAXN, a / CBL_MAXN + 1);
}

void cbl_newgame(cblgame *g, int seats)
{
    g->ply = 0;
    cbl_new(&g->pos[0], seats);
}

int cbl_play(cblgame *g, int move)
{
    if (g->ply >= CBL_MAX_PLY || !cbl_legal(cbl_pos(g), move)) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    cbl_apply(&g->pos[g->ply + 1], move);
    g->move[g->ply++] = move;
    return 1;
}

int cbl_undo(cblgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int cbl_script(cblgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, m = cbl_parse(cbl_pos(g), s, &len);
        if (m < 0 || !cbl_play(g, m)) return 0;
        s += len;
    }
    return 1;
}
