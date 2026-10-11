/*
 * Chess rules (chess.h). Pseudo-legal moves from a plain 64-square board, kept when the mover's king is safe after
 * them. Checked against the standard perft counts (tests/perft.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "chess.h"

#include <string.h>

static const int8_t KN[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
static const int8_t KG[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
static const int8_t DIAG[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
static const int8_t ORTH[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

static int on(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }

void chess_start(cpos *p)
{
    chess_from_fen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

static const char *number(const char *f, int *v)
{
    while (*f == ' ') f++;
    if (*f < '0' || *f > '9') return f;
    *v = 0;
    while (*f >= '0' && *f <= '9') *v = *v * 10 + (*f++ - '0');
    return f;
}

int chess_from_fen(cpos *p, const char *f)
{
    static const char types[] = "pnbrqk";
    memset(p, 0, sizeof *p);
    p->ep = -1;
    int r = 7, c = 0;
    for (; *f && *f != ' '; f++) {
        if (*f == '/') { r--; c = 0; continue; }
        if (*f >= '1' && *f <= '8') { c += *f - '0'; continue; }
        const char *t = strchr(types, *f | 0x20);
        if (!t || c > 7 || r < 0) return 0;
        p->sq[SQ(c, r)] = PC(*f >= 'a' ? BLACK : WHITE, (int)(t - types) + 1);
        c++;
    }
    while (*f == ' ') f++;
    p->side = *f == 'b' ? BLACK : WHITE;
    if (*f) f++;
    while (*f == ' ') f++;
    for (; *f && *f != ' '; f++)
        p->castle |= *f == 'K' ? CASTLE_WK : *f == 'Q' ? CASTLE_WQ : *f == 'k' ? CASTLE_BK : *f == 'q' ? CASTLE_BQ : 0;
    while (*f == ' ') f++;
    if (*f >= 'a' && *f <= 'h' && f[1] >= '1' && f[1] <= '8') { p->ep = (int8_t)SQ(f[0] - 'a', f[1] - '1'); f += 2; }
    else if (*f == '-') f++;
    int hm = 0, fm = 1;
    f = number(f, &hm);
    number(f, &fm);
    p->halfmove = (uint16_t)hm;
    p->fullmove = (uint16_t)fm;
    return 1;
}

int chess_attacked(const cpos *p, int s, int by)
{
    int f = FILE_OF(s), r = RANK_OF(s);
    int back = by == WHITE ? -1 : 1;                /* a white pawn takes upwards: it stands a rank below */
    for (int df = -1; df <= 1; df += 2)
        if (on(f + df, r + back) && p->sq[SQ(f + df, r + back)] == PC(by, PAWN)) return 1;
    for (int i = 0; i < 8; i++) {
        if (on(f + KN[i][0], r + KN[i][1]) && p->sq[SQ(f + KN[i][0], r + KN[i][1])] == PC(by, KNIGHT)) return 1;
        if (on(f + KG[i][0], r + KG[i][1]) && p->sq[SQ(f + KG[i][0], r + KG[i][1])] == PC(by, KING)) return 1;
    }
    for (int i = 0; i < 4; i++) {
        for (int k = 1; on(f + DIAG[i][0] * k, r + DIAG[i][1] * k); k++) {
            int q = p->sq[SQ(f + DIAG[i][0] * k, r + DIAG[i][1] * k)];
            if (!q) continue;
            if (q == PC(by, BISHOP) || q == PC(by, QUEEN)) return 1;
            break;
        }
        for (int k = 1; on(f + ORTH[i][0] * k, r + ORTH[i][1] * k); k++) {
            int q = p->sq[SQ(f + ORTH[i][0] * k, r + ORTH[i][1] * k)];
            if (!q) continue;
            if (q == PC(by, ROOK) || q == PC(by, QUEEN)) return 1;
            break;
        }
    }
    return 0;
}

int chess_king(const cpos *p, int color)
{
    for (int s = 0; s < 64; s++)
        if (p->sq[s] == PC(color, KING)) return s;
    return -1;
}

int chess_in_check(const cpos *p, int color)
{
    int k = chess_king(p, color);
    return k >= 0 && chess_attacked(p, k, !color);
}

static int add(cmove *out, int n, int from, int to, int flags, int promo)
{
    out[n].from = (uint8_t)from;
    out[n].to = (uint8_t)to;
    out[n].flags = (uint8_t)flags;
    out[n].promo = (uint8_t)promo;
    return n + 1;
}

static int pawn_add(cmove *out, int n, int from, int to, int flags, int promotes)
{
    if (!promotes) return add(out, n, from, to, flags, 0);
    for (int t = QUEEN; t >= KNIGHT; t--) n = add(out, n, from, to, flags, t);
    return n;
}

static int castles(const cpos *p, int us, cmove *out, int n)
{
    int hr = us == WHITE ? 0 : 7, them = !us;
    int ks = us == WHITE ? CASTLE_WK : CASTLE_BK, qs = us == WHITE ? CASTLE_WQ : CASTLE_BQ;
    if (p->sq[SQ(4, hr)] != PC(us, KING) || chess_attacked(p, SQ(4, hr), them)) return n;
    if ((p->castle & ks) && p->sq[SQ(7, hr)] == PC(us, ROOK) && !p->sq[SQ(5, hr)] && !p->sq[SQ(6, hr)] &&
        !chess_attacked(p, SQ(5, hr), them) && !chess_attacked(p, SQ(6, hr), them))
        n = add(out, n, SQ(4, hr), SQ(6, hr), MV_CASTLE, 0);
    if ((p->castle & qs) && p->sq[SQ(0, hr)] == PC(us, ROOK) && !p->sq[SQ(1, hr)] && !p->sq[SQ(2, hr)] &&
        !p->sq[SQ(3, hr)] && !chess_attacked(p, SQ(3, hr), them) && !chess_attacked(p, SQ(2, hr), them))
        n = add(out, n, SQ(4, hr), SQ(2, hr), MV_CASTLE, 0);
    return n;
}

static int slide(const cpos *p, int s, const int8_t (*d)[2], int nd, cmove *out, int n)
{
    int f = FILE_OF(s), r = RANK_OF(s), us = COLOR(p->sq[s]);
    for (int i = 0; i < nd; i++)
        for (int k = 1; on(f + d[i][0] * k, r + d[i][1] * k); k++) {
            int to = SQ(f + d[i][0] * k, r + d[i][1] * k), q = p->sq[to];
            if (!q) { n = add(out, n, s, to, 0, 0); continue; }
            if (COLOR(q) != us) n = add(out, n, s, to, MV_CAPTURE, 0);
            break;
        }
    return n;
}

static int pseudo(const cpos *p, cmove *out)
{
    int n = 0, us = p->side;
    for (int s = 0; s < 64; s++) {
        int pc = p->sq[s];
        if (!pc || COLOR(pc) != us) continue;
        int f = FILE_OF(s), r = RANK_OF(s), t = TYPE(pc);
        if (t == PAWN) {
            int dir = us == WHITE ? 1 : -1, start = us == WHITE ? 1 : 6, last = us == WHITE ? 7 : 0, r1 = r + dir;
            if (!p->sq[SQ(f, r1)]) {
                n = pawn_add(out, n, s, SQ(f, r1), 0, r1 == last);
                if (r == start && !p->sq[SQ(f, r1 + dir)]) n = add(out, n, s, SQ(f, r1 + dir), MV_DOUBLE, 0);
            }
            for (int df = -1; df <= 1; df += 2) {
                if (!on(f + df, r1)) continue;
                int to = SQ(f + df, r1), q = p->sq[to];
                if (q && COLOR(q) != us) n = pawn_add(out, n, s, to, MV_CAPTURE, r1 == last);
                else if (!q && to == p->ep) n = add(out, n, s, to, MV_CAPTURE | MV_EP, 0);
            }
        } else if (t == KNIGHT || t == KING) {
            const int8_t (*d)[2] = t == KNIGHT ? KN : KG;
            for (int i = 0; i < 8; i++) {
                if (!on(f + d[i][0], r + d[i][1])) continue;
                int to = SQ(f + d[i][0], r + d[i][1]), q = p->sq[to];
                if (!q) n = add(out, n, s, to, 0, 0);
                else if (COLOR(q) != us) n = add(out, n, s, to, MV_CAPTURE, 0);
            }
            if (t == KING) n = castles(p, us, out, n);
        } else {
            if (t != ROOK) n = slide(p, s, DIAG, 4, out, n);
            if (t != BISHOP) n = slide(p, s, ORTH, 4, out, n);
        }
    }
    return n;
}

int chess_legal(const cpos *p, cmove *out)
{
    cmove all[MAX_MOVES];
    int n = pseudo(p, all), k = 0;
    for (int i = 0; i < n; i++) {
        cpos q = *p;
        chess_make(&q, all[i]);
        if (!chess_in_check(&q, p->side)) out[k++] = all[i];
    }
    return k;
}

/* the castling rights a move from or to this square keeps (a king or a rook leaves, or a rook is taken) */
static int keeps(int s)
{
    switch (s) {
    case SQ(0, 0): return ~CASTLE_WQ;
    case SQ(7, 0): return ~CASTLE_WK;
    case SQ(4, 0): return ~(CASTLE_WK | CASTLE_WQ);
    case SQ(0, 7): return ~CASTLE_BQ;
    case SQ(7, 7): return ~CASTLE_BK;
    case SQ(4, 7): return ~(CASTLE_BK | CASTLE_BQ);
    default: return ~0;
    }
}

void chess_make(cpos *p, cmove m)
{
    int pc = p->sq[m.from], us = p->side, them = !us;
    int resets = TYPE(pc) == PAWN || p->sq[m.to];
    p->sq[m.to] = m.promo ? PC(us, m.promo) : (uint8_t)pc;
    p->sq[m.from] = EMPTY;
    if (m.flags & MV_EP) p->sq[SQ(FILE_OF(m.to), RANK_OF(m.from))] = EMPTY;
    if (m.flags & MV_CASTLE) {
        int r = RANK_OF(m.from), rf = FILE_OF(m.to) == 6 ? 7 : 0, rt = FILE_OF(m.to) == 6 ? 5 : 3;
        p->sq[SQ(rt, r)] = p->sq[SQ(rf, r)];
        p->sq[SQ(rf, r)] = EMPTY;
    }
    p->castle &= (uint8_t)(keeps(m.from) & keeps(m.to));
    p->ep = -1;
    if (m.flags & MV_DOUBLE) {                      /* only when it can be taken: repeated positions compare right */
        int f = FILE_OF(m.to), r = RANK_OF(m.to);
        if ((f > 0 && p->sq[SQ(f - 1, r)] == PC(them, PAWN)) || (f < 7 && p->sq[SQ(f + 1, r)] == PC(them, PAWN)))
            p->ep = (int8_t)((m.from + m.to) / 2);
    }
    p->halfmove = resets ? 0 : (uint16_t)(p->halfmove + 1);
    if (us == BLACK) p->fullmove++;
    p->side = (uint8_t)them;
}

/* ---- the game ------------------------------------------------------------------------------------------------------ */
void game_new(cgame *g)
{
    g->ply = 0;
    chess_start(&g->pos[0]);
}

int game_play(cgame *g, cmove m)
{
    if (g->ply >= MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    chess_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int game_undo(cgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

static int same(const cpos *a, const cpos *b)
{
    return !memcmp(a->sq, b->sq, 64) && a->side == b->side && a->castle == b->castle && a->ep == b->ep;
}

/* no checkmate is possible: kings alone, a king and one minor piece, or bishops all on one colour of squares */
static int insufficient(const cpos *p)
{
    int minors = 0, bishops[2] = {0, 0};
    for (int s = 0; s < 64; s++) {
        int t = TYPE(p->sq[s]);
        if (!t || t == KING) continue;
        if (t != KNIGHT && t != BISHOP) return 0;
        minors++;
        if (t == BISHOP) bishops[(FILE_OF(s) + RANK_OF(s)) & 1]++;
    }
    if (minors <= 1) return 1;
    return bishops[0] + bishops[1] == minors && (!bishops[0] || !bishops[1]);
}

cstatus game_status(const cgame *g)
{
    const cpos *p = game_pos(g);
    cmove m[MAX_MOVES];
    if (!chess_legal(p, m)) return chess_in_check(p, p->side) ? ST_CHECKMATE : ST_STALEMATE;
    if (insufficient(p)) return ST_INSUFFICIENT;
    if (p->halfmove >= 100) return ST_FIFTY;
    int seen = 0;
    for (int i = g->ply; i >= 0 && g->ply - i <= p->halfmove; i--)
        if (same(&g->pos[i], p) && ++seen >= 3) return ST_REPETITION;
    return ST_PLAY;
}
