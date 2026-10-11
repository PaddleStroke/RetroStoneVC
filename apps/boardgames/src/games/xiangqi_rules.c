/*
 * Xiangqi's rules for RetroStone Board (xiangqi.h): the pieces' moves, check and the flying general, the end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/xiangqi.h"

static const int ORTH[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}, DIAG[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

static int on(int x, int y) { return x >= 0 && x < XQ_W && y >= 0 && y < XQ_H; }
static int palace(int player, int x, int y) { return x >= 3 && x <= 5 && (player ? y >= 7 : y <= 2); }
static int home(int player, int y) { return player ? y >= 5 : y <= 4; }   /* the player's side of the river */
static int ahead(int player) { return player ? -1 : 1; }                    /* a soldier's forward step */

void xq_start(xpos *p)
{
    static const uint8_t back[XQ_W] = {XQ_CHARIOT, XQ_HORSE, XQ_ELEPHANT, XQ_ADVISOR, XQ_GENERAL, XQ_ADVISOR,
                                       XQ_ELEPHANT, XQ_HORSE, XQ_CHARIOT};
    memset(p, 0, sizeof *p);
    for (int pl = 0; pl < 2; pl++) {
        int y0 = pl ? 9 : 0, yc = pl ? 7 : 2, ys = pl ? 6 : 3;
        for (int x = 0; x < XQ_W; x++) p->sq[XQ_PT(x, y0)] = XQ_PC(pl, back[x]);
        p->sq[XQ_PT(1, yc)] = p->sq[XQ_PT(7, yc)] = XQ_PC(pl, XQ_CANNON);
        for (int x = 0; x < XQ_W; x += 2) p->sq[XQ_PT(x, ys)] = XQ_PC(pl, XQ_SOLDIER);
    }
}

int xq_general(const xpos *p, int player)
{
    for (int i = 0; i < XQ_PTS; i++)
        if (p->sq[i] == XQ_PC(player, XQ_GENERAL)) return i;
    return -1;
}

static int piece(const xpos *p, int x, int y) { return on(x, y) ? p->sq[XQ_PT(x, y)] : -1; }

int xq_in_check(const xpos *p, int player)
{
    int g = xq_general(p, player), e = !player;
    if (g < 0) return 0;
    int gx = XQ_X(g), gy = XQ_Y(g);
    for (int d = 0; d < 4; d++) {               /* the files and ranks: a chariot, the other general, a cannon */
        int x = gx + ORTH[d][0], y = gy + ORTH[d][1], seen = 0;
        for (; on(x, y); x += ORTH[d][0], y += ORTH[d][1]) {
            int pc = p->sq[XQ_PT(x, y)];
            if (!pc) continue;
            if (!seen) {
                if (pc == XQ_PC(e, XQ_CHARIOT) || (pc == XQ_PC(e, XQ_GENERAL) && ORTH[d][0] == 0)) return 1;
                seen = 1;
            } else {
                if (pc == XQ_PC(e, XQ_CANNON)) return 1;
                break;
            }
        }
    }
    for (int d = 0; d < 4; d++)                 /* a horse two points away, its leg free */
        for (int s = -1; s <= 1; s += 2) {
            int dx = ORTH[d][0], dy = ORTH[d][1];
            int hx = gx - 2 * dx - (dx ? 0 : s), hy = gy - 2 * dy - (dy ? 0 : s);   /* its leg: one step from it */
            if (piece(p, hx, hy) == XQ_PC(e, XQ_HORSE) && piece(p, hx + dx, hy + dy) == 0) return 1;
        }
    if (piece(p, gx, gy - ahead(e)) == XQ_PC(e, XQ_SOLDIER)) return 1;
    if (!home(e, gy) && (piece(p, gx - 1, gy) == XQ_PC(e, XQ_SOLDIER) || piece(p, gx + 1, gy) == XQ_PC(e, XQ_SOLDIER)))
        return 1;
    return 0;
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static void add(const xpos *p, xmove *out, int *n, int from, int x, int y)
{
    if (!on(x, y)) return;
    int to = XQ_PT(x, y), pc = p->sq[to];
    if (pc && XQ_OWNER(pc) == p->side) return;
    out[(*n)++] = (xmove){(uint8_t)from, (uint8_t)to, (uint8_t)pc};
}

/* every move of the side to move, its own general's safety not yet checked */
static int pseudo(const xpos *p, xmove *out)
{
    int n = 0, me = p->side;
    for (int from = 0; from < XQ_PTS; from++) {
        int pc = p->sq[from];
        if (!pc || XQ_OWNER(pc) != me) continue;
        int x = XQ_X(from), y = XQ_Y(from);
        switch (XQ_TYPE(pc)) {
        case XQ_GENERAL:
            for (int d = 0; d < 4; d++)
                if (palace(me, x + ORTH[d][0], y + ORTH[d][1])) add(p, out, &n, from, x + ORTH[d][0], y + ORTH[d][1]);
            break;
        case XQ_ADVISOR:
            for (int d = 0; d < 4; d++)
                if (palace(me, x + DIAG[d][0], y + DIAG[d][1])) add(p, out, &n, from, x + DIAG[d][0], y + DIAG[d][1]);
            break;
        case XQ_ELEPHANT:                       /* two points diagonally, its eye free, never over the river */
            for (int d = 0; d < 4; d++) {
                int tx = x + 2 * DIAG[d][0], ty = y + 2 * DIAG[d][1];
                int eye = piece(p, x + DIAG[d][0], y + DIAG[d][1]);
                if (on(tx, ty) && home(me, ty) && !eye) add(p, out, &n, from, tx, ty);
            }
            break;
        case XQ_HORSE:                          /* one point straight (its leg, free), then one diagonally outward */
            for (int d = 0; d < 4; d++) {
                int dx = ORTH[d][0], dy = ORTH[d][1];
                if (piece(p, x + dx, y + dy) != 0) continue;
                add(p, out, &n, from, x + 2 * dx + dy, y + 2 * dy + dx);
                add(p, out, &n, from, x + 2 * dx - dy, y + 2 * dy - dx);
            }
            break;
        case XQ_CHARIOT:
        case XQ_CANNON:                         /* the cannon takes by jumping exactly one piece, its screen */
            for (int d = 0; d < 4; d++) {
                int tx = x + ORTH[d][0], ty = y + ORTH[d][1], screen = 0;
                for (; on(tx, ty); tx += ORTH[d][0], ty += ORTH[d][1]) {
                    int t = p->sq[XQ_PT(tx, ty)];
                    if (XQ_TYPE(pc) == XQ_CHARIOT) {
                        add(p, out, &n, from, tx, ty);
                        if (t) break;
                    } else if (!screen) {
                        if (!t) add(p, out, &n, from, tx, ty);
                        else screen = 1;
                    } else if (t) {
                        add(p, out, &n, from, tx, ty);
                        break;
                    }
                }
            }
            break;
        case XQ_SOLDIER:                        /* forward; over the river, sideways too */
            add(p, out, &n, from, x, y + ahead(me));
            if (!home(me, y)) {
                add(p, out, &n, from, x - 1, y);
                add(p, out, &n, from, x + 1, y);
            }
            break;
        }
    }
    return n;
}

void xq_make(xpos *p, xmove m)
{
    p->sq[m.to] = p->sq[m.from];
    p->sq[m.from] = 0;
    p->quiet = m.cap ? 0 : (uint16_t)(p->quiet + 1);
    p->side ^= 1;
}

int xq_legal(const xpos *p, xmove *out)
{
    xmove all[XQ_MAX_MOVES];
    int n = pseudo(p, all), k = 0;
    for (int i = 0; i < n; i++) {
        xpos q = *p;
        xq_make(&q, all[i]);
        if (!xq_in_check(&q, p->side)) out[k++] = all[i];
    }
    return k;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int xq_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '0' || s[1] > '9') return -1;
    return XQ_PT(s[0] - 'a', s[1] - '0');
}

int xq_parse(const xmove *legal, int n, const char *s)
{
    int from = xq_point(s), to = from < 0 ? -1 : xq_point(s + 2);
    for (int i = 0; to >= 0 && i < n; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

int xq_setup(xpos *p, int side, const char *pieces0, const char *pieces1)
{
    static const char LETTERS[] = " KAEHRCP";
    memset(p, 0, sizeof *p);
    p->side = (uint8_t)side;
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = pl ? pieces1 : pieces0; *s;) {
            if (*s == ' ') { s++; continue; }
            const char *t = strchr(LETTERS + 1, *s);
            int pt = xq_point(s + 1);
            if (!t || pt < 0) return 0;
            p->sq[pt] = XQ_PC(pl, (int)(t - LETTERS));
            s += 3;
        }
    return 1;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void xq_new(xgame *g)
{
    g->ply = 0;
    xq_start(&g->pos[0]);
}

int xq_play(xgame *g, xmove m)
{
    if (g->ply >= XQ_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    xq_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int xq_undo(xgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

xstatus xq_status(const xgame *g)
{
    const xpos *p = xq_pos(g);
    xmove m[XQ_MAX_MOVES];
    if (!xq_legal(p, m)) return xq_in_check(p, p->side) ? XQ_MATE : XQ_STALEMATE;
    int seen = 0;                               /* the same position earlier (none before the last capture) */
    for (int k = g->ply - 2; k >= 0 && k >= g->ply - p->quiet; k -= 2)
        seen += !memcmp(g->pos[k].sq, p->sq, sizeof p->sq);
    if (seen >= 2) return XQ_REPETITION;
    if (p->quiet >= XQ_QUIET_PLIES) return XQ_QUIET;
    return XQ_PLAY;
}
