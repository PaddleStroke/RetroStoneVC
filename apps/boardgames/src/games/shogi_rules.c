/*
 * Shogi: the rules (shogi.h). The moves are made on a copy of the position and kept when they leave the king safe.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/shogi.h"

/* ---- how the pieces move, as sente sees them (forward: dy -1); gote's are turned round --------------------------- */
typedef struct { int nstep, nslide; int8_t step[8][2], slide[4][2]; } pattern;

#define FWD3 {-1, -1}, {0, -1}, {1, -1}
#define ORTHO {0, -1}, {-1, 0}, {1, 0}, {0, 1}
#define DIAG {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
static const pattern P_PAWN = {1, 0, {{0, -1}}, {{0}}};
static const pattern P_LANCE = {0, 1, {{0}}, {{0, -1}}};
static const pattern P_KNIGHT = {2, 0, {{-1, -2}, {1, -2}}, {{0}}};
static const pattern P_SILVER = {5, 0, {FWD3, {-1, 1}, {1, 1}}, {{0}}};
static const pattern P_GOLD = {6, 0, {FWD3, {-1, 0}, {1, 0}, {0, 1}}, {{0}}};
static const pattern P_BISHOP = {0, 4, {{0}}, {DIAG}};
static const pattern P_ROOK = {0, 4, {{0}}, {ORTHO}};
static const pattern P_KING = {8, 0, {FWD3, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}, {{0}}};
static const pattern P_HORSE = {4, 4, {ORTHO}, {DIAG}};
static const pattern P_DRAGON = {4, 4, {DIAG}, {ORTHO}};

static const pattern *pattern_of(int face)
{
    switch (face) {
    case SG_PAWN: return &P_PAWN;
    case SG_LANCE: return &P_LANCE;
    case SG_KNIGHT: return &P_KNIGHT;
    case SG_SILVER: return &P_SILVER;
    case SG_BISHOP: return &P_BISHOP;
    case SG_ROOK: return &P_ROOK;
    case SG_KING: return &P_KING;
    case SG_BISHOP | SG_PROMOTED: return &P_HORSE;
    case SG_ROOK | SG_PROMOTED: return &P_DRAGON;
    default: return &P_GOLD;                    /* the gold, and the promoted silver, knight, lance, pawn */
    }
}

/* the ranks left in front of a piece of the player on the row (0: the last rank) */
static int ahead(int player, int row) { return player ? 8 - row : row; }

/* 1: a piece of this kind could never move again from the row (it must promote, it may not be dropped there) */
static int dead(int kind, int player, int row)
{
    int a = ahead(player, row);
    return ((kind == SG_PAWN || kind == SG_LANCE) && a == 0) || (kind == SG_KNIGHT && a <= 1);
}

static int can_promote(int pc) { return !(pc & SG_PROMOTED) && SG_KIND(pc) != SG_GOLD && SG_KIND(pc) != SG_KING; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
void sg_start(spos *p) { sg_from_sfen(p, "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"); }

static int kind_of_letter(char c)
{
    const char *k = strchr("PLNSGBRK", c >= 'a' ? c - 32 : c);
    return k && c ? (int)(k - "PLNSGBRK") + SG_PAWN : 0;
}

int sg_from_sfen(spos *p, const char *s)
{
    memset(p, 0, sizeof *p);
    int row = 0, col = 0, promoted = 0;
    for (; *s && *s != ' '; s++) {
        if (*s == '/') { if (col != 9) return 0; row++; col = 0; continue; }
        if (*s >= '1' && *s <= '9') { col += *s - '0'; continue; }
        if (*s == '+') { promoted = 1; continue; }
        int k = kind_of_letter(*s);
        if (!k || row > 8 || col > 8) return 0;
        p->sq[SG_SQ(col, row)] = (uint8_t)(SG_PC(*s >= 'a', k) | (promoted ? SG_PROMOTED : 0));
        promoted = 0;
        col++;
    }
    if (row != 8 || col != 9 || *s++ != ' ') return 0;
    p->side = *s == 'w';
    if (*s != 'b' && *s != 'w') return 0;
    s++;
    while (*s == ' ') s++;
    for (int n = 0; *s && *s != ' ' && *s != '-'; s++) {
        if (*s >= '0' && *s <= '9') { n = n * 10 + *s - '0'; continue; }
        int k = kind_of_letter(*s);
        if (!k || k == SG_KING) return 0;
        p->hand[*s >= 'a'][k] = (uint8_t)(p->hand[*s >= 'a'][k] + (n ? n : 1));
        n = 0;
    }
    return 1;
}

int sg_king(const spos *p, int player)
{
    for (int s = 0; s < 81; s++)
        if (p->sq[s] == SG_PC(player, SG_KING)) return s;
    return -1;
}

/* 1: the piece on `from` attacks `to` */
static int attacks(const spos *p, int from, int to)
{
    int pc = p->sq[from], turn = SG_OWNER(pc) ? -1 : 1;
    int dx = (SG_COL(to) - SG_COL(from)) * turn, dy = (SG_ROW(to) - SG_ROW(from)) * turn;
    const pattern *pt = pattern_of(SG_FACE(pc));
    for (int i = 0; i < pt->nstep; i++)
        if (pt->step[i][0] == dx && pt->step[i][1] == dy) return 1;
    int k = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    for (int i = 0; i < pt->nslide; i++) {
        int sx = pt->slide[i][0], sy = pt->slide[i][1];
        if (!k || dx != sx * k || dy != sy * k) continue;
        for (int j = 1; j < k; j++)             /* the squares between must be empty */
            if (p->sq[from + (sy * 9 + sx) * turn * j]) return 0;
        return 1;
    }
    return 0;
}

int sg_attacked(const spos *p, int sq, int by)
{
    for (int s = 0; s < 81; s++)
        if (p->sq[s] && SG_OWNER(p->sq[s]) == by && attacks(p, s, sq)) return 1;
    return 0;
}

int sg_in_check(const spos *p, int player)
{
    int k = sg_king(p, player);
    return k >= 0 && sg_attacked(p, k, !player);
}

void sg_make(spos *p, smove m)
{
    int me = p->side;
    if (m.drop) {
        p->sq[m.to] = SG_PC(me, m.drop);
        p->hand[me][m.drop]--;
    } else {
        int cap = p->sq[m.to];
        if (cap) p->hand[me][SG_KIND(cap)]++;  /* it changes sides, unpromoted */
        p->sq[m.to] = (uint8_t)(p->sq[m.from] | (m.promo ? SG_PROMOTED : 0));
        p->sq[m.from] = 0;
    }
    p->side = (uint8_t)!me;
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
typedef struct { smove *out; int n; } list;

static void add(list *l, int from, int to, int drop, int promo)
{
    if (l->out) l->out[l->n] = (smove){(uint8_t)from, (uint8_t)to, (uint8_t)drop, (uint8_t)promo};
    l->n++;
}

/* a piece's move: both forms when it may promote, only the promoted one when it must */
static void add_move(list *l, const spos *p, int from, int to)
{
    int pc = p->sq[from], me = SG_OWNER(pc);
    if (can_promote(pc) && (ahead(me, SG_ROW(from)) < 3 || ahead(me, SG_ROW(to)) < 3)) {
        add(l, from, to, 0, 1);
        if (dead(SG_KIND(pc), me, SG_ROW(to))) return;
    }
    add(l, from, to, 0, 0);
}

static void pseudo(const spos *p, list *l)
{
    int me = p->side, turn = me ? -1 : 1;
    for (int s = 0; s < 81; s++) {
        int pc = p->sq[s];
        if (!pc || SG_OWNER(pc) != me) continue;
        const pattern *pt = pattern_of(SG_FACE(pc));
        int x = SG_COL(s), y = SG_ROW(s);
        for (int i = 0; i < pt->nstep; i++) {
            int tx = x + pt->step[i][0] * turn, ty = y + pt->step[i][1] * turn;
            if (tx < 0 || tx > 8 || ty < 0 || ty > 8) continue;
            int t = p->sq[SG_SQ(tx, ty)];
            if (!t || SG_OWNER(t) != me) add_move(l, p, s, SG_SQ(tx, ty));
        }
        for (int i = 0; i < pt->nslide; i++) {
            int dx = pt->slide[i][0] * turn, dy = pt->slide[i][1] * turn;
            for (int tx = x + dx, ty = y + dy; tx >= 0 && tx <= 8 && ty >= 0 && ty <= 8; tx += dx, ty += dy) {
                int t = p->sq[SG_SQ(tx, ty)];
                if (t && SG_OWNER(t) == me) break;
                add_move(l, p, s, SG_SQ(tx, ty));
                if (t) break;
            }
        }
    }
    for (int k = SG_PAWN; k <= SG_ROOK; k++) {
        if (!p->hand[me][k]) continue;
        int pawn_on[9] = {0};                   /* nifu: no second unpromoted pawn on a file */
        if (k == SG_PAWN)
            for (int s = 0; s < 81; s++) if (p->sq[s] == SG_PC(me, SG_PAWN)) pawn_on[SG_COL(s)] = 1;
        for (int s = 0; s < 81; s++)
            if (!p->sq[s] && !dead(k, me, SG_ROW(s)) && !pawn_on[SG_COL(s)]) add(l, 0, s, k, 0);
    }
}

static int legal(const spos *p, smove *out, int stop_at_one);

/* uchifuzume: a pawn drop may not give checkmate */
static int pawn_drop_mates(const spos *after)
{
    return sg_in_check(after, after->side) && !legal(after, NULL, 1);
}

static int legal(const spos *p, smove *out, int stop_at_one)
{
    static smove buf[SG_MAX_MOVES * 2];         /* the pseudo-moves; the pawn drop test's own on the stack */
    smove local[SG_MAX_MOVES * 2];
    smove *all = stop_at_one ? local : buf;
    list l = {all, 0};
    pseudo(p, &l);
    int n = 0;
    for (int i = 0; i < l.n; i++) {
        spos q = *p;
        sg_make(&q, all[i]);
        if (sg_in_check(&q, p->side)) continue;
        if (all[i].drop == SG_PAWN && !stop_at_one && pawn_drop_mates(&q)) continue;
        if (out) out[n] = all[i];
        n++;
        if (stop_at_one) return 1;
    }
    return n;
}

int sg_legal(const spos *p, smove *out) { return legal(p, out, 0); }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int square(const char *s)
{
    if (s[0] < '1' || s[0] > '9' || s[1] < 'a' || s[1] > 'i') return -1;
    return SG_SQ(9 - (s[0] - '0'), s[1] - 'a');
}

int sg_parse(const smove *legal_moves, int nlegal, const char *s, int *used)
{
    int from = -1, to, drop = 0, promo = 0, n;
    if (s[0] && s[1] == '*') {
        drop = kind_of_letter(s[0]);
        to = square(s + 2);
        n = 4;
        if (!drop || drop == SG_KING) return -1;
    } else {
        from = square(s);
        to = from < 0 ? -1 : square(s + 2);
        n = 4;
        if (to >= 0 && s[4] == '+') promo = 1, n = 5;
    }
    if (to < 0) return -1;
    *used = n;
    for (int i = 0; i < nlegal; i++) {
        smove m = legal_moves[i];
        if (m.to != to || m.drop != drop || m.promo != promo || (!drop && m.from != from)) continue;
        return i;
    }
    return -1;
}

void sg_usi(smove m, char out[8])
{
    int c = SG_COL(m.to), r = SG_ROW(m.to);
    if (m.drop) {
        out[0] = "?PLNSGBR"[m.drop];
        out[1] = '*';
    } else {
        out[0] = (char)('9' - SG_COL(m.from));
        out[1] = (char)('a' + SG_ROW(m.from));
    }
    out[2] = (char)('9' - c);
    out[3] = (char)('a' + r);
    out[4] = m.promo ? '+' : 0;
    out[5] = 0;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void sg_new(sgame *g)
{
    g->ply = 0;
    sg_start(&g->pos[0]);
}

int sg_play(sgame *g, smove m)
{
    if (g->ply >= SG_MAX_PLY) return 0;
    g->move[g->ply] = m;
    g->pos[g->ply + 1] = g->pos[g->ply];
    sg_make(&g->pos[g->ply + 1], m);
    g->ply++;
    return 1;
}

int sg_undo(sgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

sstatus sg_status(const sgame *g)
{
    const spos *p = sg_pos(g);
    if (!legal(p, NULL, 0)) return p->side ? SG_WON0 : SG_WON1;   /* in full: the pawn drop rule counts */
    int seen = 1, first = g->ply;
    for (int i = g->ply - 2; i >= 0; i -= 2)    /* the same player to move: every second ply */
        if (!memcmp(&g->pos[i], p, sizeof *p)) seen++, first = i;
    if (seen < 4) return SG_PLAY;
    int checked[2] = {1, 1};                    /* perpetual check: every move of one player since the first time */
    for (int i = first; i < g->ply; i++)
        if (!sg_in_check(&g->pos[i + 1], g->pos[i + 1].side)) checked[g->pos[i].side] = 0;
    if (checked[0] != checked[1]) return checked[0] ? SG_WON1 : SG_WON0;
    return SG_DRAW;
}
