/*
 * Jungle (Dou Shou Qi): the rules (jungle.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/jungle.h"

static const int DF[4] = {0, 1, 0, -1}, DR[4] = {1, 0, -1, 0};   /* up, right, down, left */

int jg_water(int s)
{
    int f = JG_FILE(s), r = JG_ROW(s);
    return r >= 3 && r <= 5 && f != 0 && f != 3 && f != 6;
}

int jg_den(int s) { return s == JG_SQ(3, 0) ? 0 : s == JG_SQ(3, 8) ? 1 : -1; }

int jg_trap(int s)
{
    int f = JG_FILE(s), r = JG_ROW(s);
    if (((f == 2 || f == 4) && r == 0) || (f == 3 && r == 1)) return 0;
    if (((f == 2 || f == 4) && r == 8) || (f == 3 && r == 7)) return 1;
    return -1;
}

int jg_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'g' || s[1] < '1' || s[1] > '9') return -1;
    return JG_SQ(s[0] - 'a', s[1] - '1');
}

void jg_start(jgpos *p)
{
    static const char *const place[2][8] = {
        {"g3", "b2", "f2", "c3", "e3", "a1", "g1", "a3"},  /* player 0: rat, cat, dog, wolf, leopard, tiger, ... */
        {"a7", "f8", "b8", "e7", "c7", "g9", "a9", "g7"},
    };
    memset(p, 0, sizeof *p);
    for (int c = 0; c < 2; c++)
        for (int k = 0; k < 8; k++) p->sq[jg_square(place[c][k])] = (uint8_t)JG_PC(c, k + 1);
    p->left[0] = p->left[1] = 8;
}

/* can the animal on `from` take the one on `to`? */
static int takes(const jgpos *p, int from, int to)
{
    int a = p->sq[from], d = p->sq[to], ar = JG_RANK(a), dr = JG_RANK(d);
    if (jg_water(from) != jg_water(to)) return 0;   /* never between the water and the land */
    if (jg_trap(to) == JG_OWNER(a)) return 1;      /* in the attacker's trap: rank 0 */
    if (ar == JG_RAT && dr == JG_ELEPHANT) return 1;
    if (ar == JG_ELEPHANT && dr == JG_RAT) return 0;
    return ar >= dr;
}

int jg_legal(const jgpos *p, jgmove *out)
{
    int n = 0;
    for (int s = 0; s < JG_SQUARES; s++) {
        int pc = p->sq[s];
        if (!pc || JG_OWNER(pc) != p->side) continue;
        int rank = JG_RANK(pc);
        for (int d = 0; d < 4; d++) {
            int f = JG_FILE(s) + DF[d], r = JG_ROW(s) + DR[d];
            if (f < 0 || f >= JG_W || r < 0 || r >= JG_H) continue;
            int t = JG_SQ(f, r);
            if (jg_water(t) && rank != JG_RAT) {
                if (rank != JG_LION && rank != JG_TIGER) continue;
                int blocked = 0;                /* the jump: across the whole water, no rat on the way */
                while (jg_water(t)) {
                    blocked |= p->sq[t] != 0;
                    f += DF[d];
                    r += DR[d];
                    t = JG_SQ(f, r);
                }
                if (blocked) continue;
            }
            if (jg_den(t) == p->side) continue;
            if (p->sq[t] && (JG_OWNER(p->sq[t]) == p->side || !takes(p, s, t))) continue;
            out[n++] = (jgmove){(int8_t)s, (int8_t)t, p->sq[t]};
        }
    }
    return n;
}

void jg_make(jgpos *p, jgmove m)
{
    if (m.taken) {
        p->left[JG_OWNER(m.taken)]--;
        p->quiet = 0;
    } else {
        p->quiet++;
    }
    p->sq[m.to] = p->sq[m.from];
    p->sq[m.from] = 0;
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void jg_new(jggame *g)
{
    g->ply = 0;
    jg_start(&g->pos[0]);
}

int jg_play(jggame *g, jgmove m)
{
    if (g->ply >= JG_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    jg_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int jg_undo(jggame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

jgstatus jg_status(const jggame *g)
{
    const jgpos *p = jg_pos(g);
    int den = JG_SQ(3, p->side ? 8 : 0);
    if (p->sq[den]) return JS_DEN;              /* only the enemy can stand in a den */
    if (!p->left[p->side]) return JS_ALL;
    jgmove m[JG_MAX_MOVES];
    if (!jg_legal(p, m)) return JS_STUCK;
    int seen = 0;                               /* back to the last capture: nothing before it repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i -= 2)
        seen += !memcmp(g->pos[i].sq, p->sq, sizeof p->sq);
    if (seen >= 3) return JS_REPETITION;
    return p->quiet >= JG_QUIET ? JS_QUIET : JS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int jg_script(jggame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = jg_square(s), b;
        if (a < 0) return 0;
        s += 2;
        if (*s == '-' || *s == 'x') s++;
        if ((b = jg_square(s)) < 0) return 0;
        s += 2;
        if (*s && *s != ' ') return 0;
        jgmove l[JG_MAX_MOVES];
        int n = jg_legal(jg_pos(g), l), k = 0;
        while (k < n && !(l[k].from == a && l[k].to == b)) k++;
        if (k == n || jg_status(g) != JS_PLAY || !jg_play(g, l[k])) return 0;
    }
    return 1;
}
