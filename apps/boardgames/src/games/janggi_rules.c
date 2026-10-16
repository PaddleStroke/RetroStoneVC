/*
 * Janggi's rules for RetroStone Board (janggi.h): the pieces' moves along the lines and the palaces' diagonals, the
 * cannon's jump, check, the pass, the end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/janggi.h"

static const int DIRS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

static int on(int x, int y) { return x >= 0 && x < JN_W && y >= 0 && y < JN_H; }
static int in_palace(int x, int y) { return x >= 3 && x <= 5 && (y <= 2 || y >= 7); }
static int own_palace(int player, int x, int y) { return in_palace(x, y) && (player ? y >= 7 : y <= 2); }
static int ahead(int player) { return player ? -1 : 1; }   /* a soldier's forward step */

/* a diagonal step along a palace's diagonal: both points in one palace, one of them its middle */
static int link(int x0, int y0, int x1, int y1)
{
    if (!in_palace(x0, y0) || !in_palace(x1, y1) || (y0 <= 2) != (y1 <= 2)) return 0;
    return (x0 == 4 && (y0 == 1 || y0 == 8)) || (x1 == 4 && (y1 == 1 || y1 == 8));
}

void jn_start(jpos *p)
{
    static const uint8_t back[JN_W] = {JN_CHARIOT, JN_HORSE, JN_ELEPHANT, JN_GUARD, JN_EMPTY, JN_GUARD,
                                       JN_ELEPHANT, JN_HORSE, JN_CHARIOT};
    memset(p, 0, sizeof *p);
    for (int pl = 0; pl < 2; pl++) {
        int y0 = pl ? 9 : 0, yg = pl ? 8 : 1, yc = pl ? 7 : 2, ys = pl ? 6 : 3;
        for (int x = 0; x < JN_W; x++)
            if (back[x]) p->sq[JN_PT(x, y0)] = JN_PC(pl, back[x]);
        p->sq[JN_PT(4, yg)] = JN_PC(pl, JN_GENERAL);
        p->sq[JN_PT(1, yc)] = p->sq[JN_PT(7, yc)] = JN_PC(pl, JN_CANNON);
        for (int x = 0; x < JN_W; x += 2) p->sq[JN_PT(x, ys)] = JN_PC(pl, JN_SOLDIER);
    }
}

int jn_general(const jpos *p, int player)
{
    for (int i = 0; i < JN_PTS; i++)
        if (p->sq[i] == JN_PC(player, JN_GENERAL)) return i;
    return -1;
}

static int piece(const jpos *p, int x, int y) { return on(x, y) ? p->sq[JN_PT(x, y)] : -1; }

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static void add(const jpos *p, int me, jmove *out, int *n, int from, int x, int y)
{
    if (!on(x, y)) return;
    int to = JN_PT(x, y), pc = p->sq[to];
    if (pc && JN_OWNER(pc) == me) return;
    out[(*n)++] = (jmove){(uint8_t)from, (uint8_t)to, (uint8_t)pc};
}

/* a chariot's or a cannon's line from `from`, one way (a diagonal only along a palace's). The cannon needs a screen:
 * exactly one piece to jump, not a cannon; it never takes a cannon. */
static void slide(const jpos *p, int me, jmove *out, int *n, int from, int dx, int dy, int cannon)
{
    int x = JN_X(from), y = JN_Y(from), screen = 0;
    for (;;) {
        int nx = x + dx, ny = y + dy;
        if (!on(nx, ny) || (dx && dy && !link(x, y, nx, ny))) return;
        x = nx;
        y = ny;
        int t = p->sq[JN_PT(x, y)];
        if (!cannon) {
            add(p, me, out, n, from, x, y);
            if (t) return;
        } else if (!screen) {
            if (t && JN_TYPE(t) == JN_CANNON) return;
            screen = t != 0;
        } else if (!t) add(p, me, out, n, from, x, y);
        else {
            if (JN_TYPE(t) != JN_CANNON) add(p, me, out, n, from, x, y);
            return;
        }
    }
}

/* every move of `me`, its own general's safety not yet checked */
static int pseudo(const jpos *p, int me, jmove *out)
{
    int n = 0;
    for (int from = 0; from < JN_PTS; from++) {
        int pc = p->sq[from];
        if (!pc || JN_OWNER(pc) != me) continue;
        int x = JN_X(from), y = JN_Y(from);
        switch (JN_TYPE(pc)) {
        case JN_GENERAL:
        case JN_GUARD:                          /* one point along the palace's lines, inside it */
            for (int d = 0; d < 8; d++) {
                int tx = x + DIRS[d][0], ty = y + DIRS[d][1];
                if (own_palace(me, tx, ty) && (d < 4 || link(x, y, tx, ty))) add(p, me, out, &n, from, tx, ty);
            }
            break;
        case JN_HORSE:                          /* one point straight (free), then one diagonally outward */
        case JN_ELEPHANT:                       /* one point straight, then two diagonally: both points free */
            for (int d = 0; d < 4; d++) {
                int dx = DIRS[d][0], dy = DIRS[d][1];
                if (piece(p, x + dx, y + dy) != 0) continue;
                for (int s = -1; s <= 1; s += 2) {
                    int ex = dx + s * dy, ey = dy + s * dx;   /* the diagonal step outward */
                    if (JN_TYPE(pc) == JN_HORSE) add(p, me, out, &n, from, x + dx + ex, y + dy + ey);
                    else if (piece(p, x + dx + ex, y + dy + ey) == 0)
                        add(p, me, out, &n, from, x + dx + 2 * ex, y + dy + 2 * ey);
                }
            }
            break;
        case JN_CHARIOT:
        case JN_CANNON:
            for (int d = 0; d < 8; d++)
                slide(p, me, out, &n, from, DIRS[d][0], DIRS[d][1], JN_TYPE(pc) == JN_CANNON);
            break;
        case JN_SOLDIER:                        /* forward or sideways; forward along a palace's diagonal */
            add(p, me, out, &n, from, x, y + ahead(me));
            add(p, me, out, &n, from, x - 1, y);
            add(p, me, out, &n, from, x + 1, y);
            for (int s = -1; s <= 1; s += 2)
                if (link(x, y, x + s, y + ahead(me))) add(p, me, out, &n, from, x + s, y + ahead(me));
            break;
        }
    }
    return n;
}

int jn_in_check(const jpos *p, int player)
{
    int g = jn_general(p, player);
    if (g < 0) return 0;
    jmove m[JN_MAX_MOVES];
    int n = pseudo(p, !player, m);
    for (int i = 0; i < n; i++)
        if (m[i].to == g) return 1;
    return 0;
}

int jn_facing(const jpos *p)
{
    int a = jn_general(p, 0), b = jn_general(p, 1);
    if (a < 0 || b < 0 || JN_X(a) != JN_X(b)) return 0;
    for (int s = a + JN_W; s < b; s += JN_W)
        if (p->sq[s]) return 0;
    return 1;
}

void jn_make(jpos *p, jmove m)
{
    if (m.from != JN_PASS) {
        p->sq[m.to] = p->sq[m.from];
        p->sq[m.from] = 0;
    }
    p->quiet = m.cap ? 0 : (uint16_t)(p->quiet + 1);
    p->side ^= 1;
}

int jn_legal(const jpos *p, jmove *out)
{
    jmove all[JN_MAX_MOVES];
    int n = pseudo(p, p->side, all), k = 0;
    for (int i = 0; i < n; i++) {
        jpos q = *p;
        jn_make(&q, all[i]);
        if (!jn_in_check(&q, p->side) && !jn_facing(&q)) out[k++] = all[i];
    }
    return k;
}

int jn_can_pass(const jpos *p) { return !jn_in_check(p, p->side); }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int jn_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '0' || s[1] > '9') return -1;
    return JN_PT(s[0] - 'a', s[1] - '0');
}

int jn_parse(const jmove *legal, int n, const char *s)
{
    int from = jn_point(s), to = from < 0 ? -1 : jn_point(s + 2);
    for (int i = 0; to >= 0 && i < n; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

int jn_setup(jpos *p, int side, const char *pieces0, const char *pieces1)
{
    static const char LETTERS[] = " KAEHRCP";
    memset(p, 0, sizeof *p);
    p->side = (uint8_t)side;
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = pl ? pieces1 : pieces0; *s;) {
            if (*s == ' ') { s++; continue; }
            const char *t = strchr(LETTERS + 1, *s);
            int pt = jn_point(s + 1);
            if (!t || pt < 0) return 0;
            p->sq[pt] = JN_PC(pl, (int)(t - LETTERS));
            s += 3;
        }
    return 1;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void jn_new(jgame *g)
{
    g->ply = 0;
    jn_start(&g->pos[0]);
}

int jn_play(jgame *g, jmove m)
{
    if (g->ply >= JN_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    jn_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int jn_undo(jgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

jstatus jn_status(const jgame *g)
{
    const jpos *p = jn_pos(g);
    jmove m[JN_MAX_MOVES];
    if (!jn_legal(p, m) && jn_in_check(p, p->side)) return JN_MATE;
    int seen = 0;                               /* the same position earlier (none before the last capture) */
    for (int k = g->ply - 2; k >= 0 && k >= g->ply - p->quiet; k -= 2)
        seen += !memcmp(g->pos[k].sq, p->sq, sizeof p->sq);
    if (seen >= 2) return JN_REPETITION;
    if (g->ply >= JN_MAX_PLY) return JN_LONG;
    return JN_PLAY;
}
