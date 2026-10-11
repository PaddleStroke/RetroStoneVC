/*
 * Hnefatafl's rules (Copenhagen, 11 x 11) and Tablut's (9 x 9): the moves, the captures, the king, the end of the game
 * (tafl.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/tafl.h"

/* the starting positions, the top row first: A an attacker, D a defender, K the king */
static const char *const START[2] = {
    "...AAAAA..."
    ".....A....."
    "..........."
    "A....D....A"
    "A...DDD...A"
    "AA.DDKDD.AA"
    "A...DDD...A"
    "A....D....A"
    "..........."
    ".....A....."
    "...AAAAA...",
    "...AAA..."
    "....A...."
    "....D...."
    "A...D...A"
    "AADDKDDAA"
    "A...D...A"
    "....D...."
    "....A...."
    "...AAA...",
};

static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

int tf_size(int variant) { return variant == TF_TABLUT ? 9 : 11; }

int tf_count(int variant, int piece)
{
    int n = tf_size(variant), c = 0;
    char want = piece == TF_ATT ? 'A' : piece == TF_DEF ? 'D' : 'K';
    for (int i = 0; i < n * n; i++) c += START[variant == TF_TABLUT][i] == want;
    return c;
}

void tf_start(tpos *p, int variant)
{
    int n = tf_size(variant);
    memset(p, 0, sizeof *p);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            char c = START[variant == TF_TABLUT][(n - 1 - y) * n + x];
            p->sq[y * n + x] = c == 'A' ? TF_ATT : c == 'D' ? TF_DEF : c == 'K' ? TF_KING : TF_EMPTY;
        }
}

int tf_throne(int variant)
{
    int n = tf_size(variant);
    return n / 2 * n + n / 2;
}

static int corner(int variant, int s)
{
    int n = tf_size(variant), x = s % n, y = s / n;
    return variant != TF_TABLUT && (x == 0 || x == n - 1) && (y == 0 || y == n - 1);
}

int tf_restricted(int variant, int s) { return s == tf_throne(variant) || corner(variant, s); }

int tf_escape(int variant, int s)
{
    int n = tf_size(variant), x = s % n, y = s / n;
    if (variant == TF_TABLUT) return x == 0 || y == 0 || x == n - 1 || y == n - 1;
    return corner(variant, s);
}

int tf_king(const tpos *p, int variant)
{
    int n = tf_size(variant);
    for (int s = 0; s < n * n; s++)
        if (p->sq[s] == TF_KING) return s;
    return -1;
}

/* the square one step from s in direction d; -1: off the board */
static int step(int variant, int s, int d)
{
    int n = tf_size(variant), x = s % n + DX[d], y = s / n + DY[d];
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : y * n + x;
}

int tf_legal(const tpos *p, int variant, tmove *out)
{
    int n = tf_size(variant), count = 0;
    for (int s = 0; s < n * n; s++) {
        int pc = p->sq[s];
        if (!pc || TF_OWNER(pc) != p->side) continue;
        for (int d = 0; d < 4; d++)
            for (int t = step(variant, s, d); t >= 0 && !p->sq[t]; t = step(variant, t, d)) {
                if (tf_restricted(variant, t) && pc != TF_KING) continue;   /* passes over the empty throne */
                out[count].from = (uint8_t)s;
                out[count].to = (uint8_t)t;
                out[count++].ncap = 0;
            }
    }
    return count;
}

/* 1: square s (on the board, or -1) helps capture a piece of player `victim`: a piece of the other player, or a hostile
 * empty square (a corner, the empty throne) */
static int hostile(const tpos *p, int variant, int s, int victim)
{
    if (s < 0) return 0;
    if (p->sq[s]) return TF_OWNER(p->sq[s]) != victim;
    return tf_restricted(variant, s);
}

/* the king at k, an attacker just moved next to it: taken? */
static int king_taken(const tpos *p, int variant, int k, int from_dir)
{
    int th = tf_throne(variant), near = k == th;
    for (int d = 0; d < 4; d++) near |= step(variant, k, d) == th;
    if (variant == TF_TABLUT && !near) {        /* Tablut, away from the throne: a plain sandwich */
        int b = step(variant, k, from_dir);
        return b >= 0 && p->sq[b] == TF_ATT;
    }
    for (int d = 0; d < 4; d++) {               /* four sides: attackers, or the empty throne */
        int b = step(variant, k, d);
        if (b < 0 || !(p->sq[b] == TF_ATT || (b == th && !p->sq[b]))) return 0;
    }
    return 1;
}

void tf_make(tpos *p, int variant, tmove *m)
{
    int pc = p->sq[m->from], me = TF_OWNER(pc);
    p->sq[m->to] = (uint8_t)pc;
    p->sq[m->from] = TF_EMPTY;
    m->ncap = 0;
    for (int d = 0; d < 4; d++) {
        int a = step(variant, m->to, d);
        if (a < 0 || !p->sq[a] || TF_OWNER(p->sq[a]) == me) continue;
        int taken = p->sq[a] == TF_KING ? king_taken(p, variant, a, d)
                                        : hostile(p, variant, step(variant, a, d), !me);
        if (taken) m->cap[m->ncap++] = (uint8_t)a;
    }
    for (int i = 0; i < m->ncap; i++) p->sq[m->cap[i]] = TF_EMPTY;
    p->side = (uint8_t)!p->side;
}

int tf_square(int variant, const char *s, int *used)
{
    int n = tf_size(variant), x = s[0] - 'a', y = 0, i = 1;
    if (x < 0 || x >= n || s[1] < '1' || s[1] > '9') return -1;
    while (s[i] >= '0' && s[i] <= '9' && i < 3) y = y * 10 + s[i++] - '0';
    if (y < 1 || y > n) return -1;
    *used = i;
    return (y - 1) * n + x;
}

/* ---- the game ------------------------------------------------------------------------------------------------------- */
void tf_new(tgame *g, int variant)
{
    g->variant = variant;
    g->ply = 0;
    tf_start(&g->pos[0], variant);
}

int tf_play(tgame *g, tmove *m)
{
    if (tf_status(g) != TF_PLAY) return 0;
    tmove all[TF_MAX_MOVES];
    int n = tf_legal(tf_pos(g), g->variant, all), ok = 0;
    for (int i = 0; i < n; i++) ok |= all[i].from == m->from && all[i].to == m->to;
    if (!ok) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    tf_make(&g->pos[g->ply + 1], g->variant, m);
    g->move[g->ply++] = *m;
    return 1;
}

int tf_undo(tgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

tstatus tf_status(const tgame *g)
{
    const tpos *p = tf_pos(g);
    int k = tf_king(p, g->variant);
    if (k < 0) return TF_WON0;
    if (tf_escape(g->variant, k)) return TF_WON1;
    tmove all[TF_MAX_MOVES];
    if (!tf_legal(p, g->variant, all)) return p->side ? TF_WON0 : TF_WON1;   /* no move: that player loses */
    int same = 1, n = tf_size(g->variant);
    for (int i = g->ply - 2; i >= 0; i -= 2)    /* the same player to move: every other position */
        same += !memcmp(g->pos[i].sq, p->sq, (size_t)(n * n));
    if (same >= 3 || g->ply >= TF_MAX_PLY) return TF_DRAW;
    return TF_PLAY;
}

int tf_script(tgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int used, from = tf_square(g->variant, s, &used);
        if (from < 0) return 0;
        s += used;
        if (*s == '-' || *s == 'x') s++;
        int to = tf_square(g->variant, s, &used);
        if (to < 0) return 0;
        s += used;
        tmove m = {(uint8_t)from, (uint8_t)to, 0, {0}};
        if (!tf_play(g, &m)) return 0;
    }
    return 1;
}
