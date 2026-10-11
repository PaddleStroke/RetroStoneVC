/*
 * Janggi's rules for RetroStone Board (janggi.h): the pieces' moves along the lines and the palaces' diagonals, the
 * cannon's jump, check, the pass, the end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/janggi.h"

static const int DIRS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

static int on(int x, int y) { return x >= 0 && x < JG_W && y >= 0 && y < JG_H; }
static int in_palace(int x, int y) { return x >= 3 && x <= 5 && (y <= 2 || y >= 7); }
static int own_palace(int player, int x, int y) { return in_palace(x, y) && (player ? y >= 7 : y <= 2); }
static int ahead(int player) { return player ? -1 : 1; }   /* a soldier's forward step */

/* a diagonal step along a palace's diagonal: both points in one palace, one of them its middle */
static int link(int x0, int y0, int x1, int y1)
{
    if (!in_palace(x0, y0) || !in_palace(x1, y1) || (y0 <= 2) != (y1 <= 2)) return 0;
    return (x0 == 4 && (y0 == 1 || y0 == 8)) || (x1 == 4 && (y1 == 1 || y1 == 8));
}

void jg_start(jpos *p)
{
    static const uint8_t back[JG_W] = {JG_CHARIOT, JG_HORSE, JG_ELEPHANT, JG_GUARD, JG_EMPTY, JG_GUARD,
                                       JG_ELEPHANT, JG_HORSE, JG_CHARIOT};
    memset(p, 0, sizeof *p);
    for (int pl = 0; pl < 2; pl++) {
        int y0 = pl ? 9 : 0, yg = pl ? 8 : 1, yc = pl ? 7 : 2, ys = pl ? 6 : 3;
        for (int x = 0; x < JG_W; x++)
            if (back[x]) p->sq[JG_PT(x, y0)] = JG_PC(pl, back[x]);
        p->sq[JG_PT(4, yg)] = JG_PC(pl, JG_GENERAL);
        p->sq[JG_PT(1, yc)] = p->sq[JG_PT(7, yc)] = JG_PC(pl, JG_CANNON);
        for (int x = 0; x < JG_W; x += 2) p->sq[JG_PT(x, ys)] = JG_PC(pl, JG_SOLDIER);
    }
}

int jg_general(const jpos *p, int player)
{
    for (int i = 0; i < JG_PTS; i++)
        if (p->sq[i] == JG_PC(player, JG_GENERAL)) return i;
    return -1;
}

static int piece(const jpos *p, int x, int y) { return on(x, y) ? p->sq[JG_PT(x, y)] : -1; }

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static void add(const jpos *p, int me, jmove *out, int *n, int from, int x, int y)
{
    if (!on(x, y)) return;
    int to = JG_PT(x, y), pc = p->sq[to];
    if (pc && JG_OWNER(pc) == me) return;
    out[(*n)++] = (jmove){(uint8_t)from, (uint8_t)to, (uint8_t)pc};
}

/* a chariot's or a cannon's line from `from`, one way (a diagonal only along a palace's). The cannon needs a screen:
 * exactly one piece to jump, not a cannon; it never takes a cannon. */
static void slide(const jpos *p, int me, jmove *out, int *n, int from, int dx, int dy, int cannon)
{
    int x = JG_X(from), y = JG_Y(from), screen = 0;
    for (;;) {
        int nx = x + dx, ny = y + dy;
        if (!on(nx, ny) || (dx && dy && !link(x, y, nx, ny))) return;
        x = nx;
        y = ny;
        int t = p->sq[JG_PT(x, y)];
        if (!cannon) {
            add(p, me, out, n, from, x, y);
            if (t) return;
        } else if (!screen) {
            if (t && JG_TYPE(t) == JG_CANNON) return;
            screen = t != 0;
        } else if (!t) add(p, me, out, n, from, x, y);
        else {
            if (JG_TYPE(t) != JG_CANNON) add(p, me, out, n, from, x, y);
            return;
        }
    }
}

/* every move of `me`, its own general's safety not yet checked */
static int pseudo(const jpos *p, int me, jmove *out)
{
    int n = 0;
    for (int from = 0; from < JG_PTS; from++) {
        int pc = p->sq[from];
        if (!pc || JG_OWNER(pc) != me) continue;
        int x = JG_X(from), y = JG_Y(from);
        switch (JG_TYPE(pc)) {
        case JG_GENERAL:
        case JG_GUARD:                          /* one point along the palace's lines, inside it */
            for (int d = 0; d < 8; d++) {
                int tx = x + DIRS[d][0], ty = y + DIRS[d][1];
                if (own_palace(me, tx, ty) && (d < 4 || link(x, y, tx, ty))) add(p, me, out, &n, from, tx, ty);
            }
            break;
        case JG_HORSE:                          /* one point straight (free), then one diagonally outward */
        case JG_ELEPHANT:                       /* one point straight, then two diagonally: both points free */
            for (int d = 0; d < 4; d++) {
                int dx = DIRS[d][0], dy = DIRS[d][1];
                if (piece(p, x + dx, y + dy) != 0) continue;
                for (int s = -1; s <= 1; s += 2) {
                    int ex = dx + s * dy, ey = dy + s * dx;   /* the diagonal step outward */
                    if (JG_TYPE(pc) == JG_HORSE) add(p, me, out, &n, from, x + dx + ex, y + dy + ey);
                    else if (piece(p, x + dx + ex, y + dy + ey) == 0)
                        add(p, me, out, &n, from, x + dx + 2 * ex, y + dy + 2 * ey);
                }
            }
            break;
        case JG_CHARIOT:
        case JG_CANNON:
            for (int d = 0; d < 8; d++)
                slide(p, me, out, &n, from, DIRS[d][0], DIRS[d][1], JG_TYPE(pc) == JG_CANNON);
            break;
        case JG_SOLDIER:                        /* forward or sideways; forward along a palace's diagonal */
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

int jg_in_check(const jpos *p, int player)
{
    int g = jg_general(p, player);
    if (g < 0) return 0;
    jmove m[JG_MAX_MOVES];
    int n = pseudo(p, !player, m);
    for (int i = 0; i < n; i++)
        if (m[i].to == g) return 1;
    return 0;
}

int jg_facing(const jpos *p)
{
    int a = jg_general(p, 0), b = jg_general(p, 1);
    if (a < 0 || b < 0 || JG_X(a) != JG_X(b)) return 0;
    for (int s = a + JG_W; s < b; s += JG_W)
        if (p->sq[s]) return 0;
    return 1;
}

void jg_make(jpos *p, jmove m)
{
    if (m.from != JG_PASS) {
        p->sq[m.to] = p->sq[m.from];
        p->sq[m.from] = 0;
    }
    p->quiet = m.cap ? 0 : (uint16_t)(p->quiet + 1);
    p->side ^= 1;
}

int jg_legal(const jpos *p, jmove *out)
{
    jmove all[JG_MAX_MOVES];
    int n = pseudo(p, p->side, all), k = 0;
    for (int i = 0; i < n; i++) {
        jpos q = *p;
        jg_make(&q, all[i]);
        if (!jg_in_check(&q, p->side) && !jg_facing(&q)) out[k++] = all[i];
    }
    return k;
}

int jg_can_pass(const jpos *p) { return !jg_in_check(p, p->side); }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int jg_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '0' || s[1] > '9') return -1;
    return JG_PT(s[0] - 'a', s[1] - '0');
}

int jg_parse(const jmove *legal, int n, const char *s)
{
    int from = jg_point(s), to = from < 0 ? -1 : jg_point(s + 2);
    for (int i = 0; to >= 0 && i < n; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

int jg_setup(jpos *p, int side, const char *pieces0, const char *pieces1)
{
    static const char LETTERS[] = " KAEHRCP";
    memset(p, 0, sizeof *p);
    p->side = (uint8_t)side;
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = pl ? pieces1 : pieces0; *s;) {
            if (*s == ' ') { s++; continue; }
            const char *t = strchr(LETTERS + 1, *s);
            int pt = jg_point(s + 1);
            if (!t || pt < 0) return 0;
            p->sq[pt] = JG_PC(pl, (int)(t - LETTERS));
            s += 3;
        }
    return 1;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void jg_new(jgame *g)
{
    g->ply = 0;
    jg_start(&g->pos[0]);
}

int jg_play(jgame *g, jmove m)
{
    if (g->ply >= JG_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    jg_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int jg_undo(jgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

jstatus jg_status(const jgame *g)
{
    const jpos *p = jg_pos(g);
    jmove m[JG_MAX_MOVES];
    if (!jg_legal(p, m) && jg_in_check(p, p->side)) return JG_MATE;
    int seen = 0;                               /* the same position earlier (none before the last capture) */
    for (int k = g->ply - 2; k >= 0 && k >= g->ply - p->quiet; k -= 2)
        seen += !memcmp(g->pos[k].sq, p->sq, sizeof p->sq);
    if (seen >= 2) return JG_REPETITION;
    if (g->ply >= JG_MAX_PLY) return JG_LONG;
    return JG_PLAY;
}
