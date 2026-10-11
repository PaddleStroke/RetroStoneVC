/*
 * Makruk: the rules (no SDL; tested by tests/makruk_test.c). The board, the pieces, the notation and the rules:
 * makruk.h. Its own move generation: only the Ma and the Rua move as in chess, the board is small, so a piece's moves
 * are found from its square and the legality by trying each move (the Khun not left attacked).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/makruk.h"

static const int8_t KNIGHT[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
static const int8_t KING[8][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};

static int fwd(int color) { return color == MK_WHITE ? 1 : -1; }

int mk_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return -1;
    return MK_SQ(s[0] - 'a', s[1] - '1');
}

void mk_start(mkpos *p)
{
    static const uint8_t BACK[8] = {MK_RUA, MK_MA, MK_KHON, MK_KHUN, MK_MET, MK_KHON, MK_MA, MK_RUA};
    memset(p, 0, sizeof *p);
    for (int f = 0; f < 8; f++) {
        p->sq[MK_SQ(f, 0)] = MK_PC(MK_WHITE, BACK[f]);
        p->sq[MK_SQ(f, 2)] = MK_PC(MK_WHITE, MK_BIA);
        p->sq[MK_SQ(f, 5)] = MK_PC(MK_BLACK, MK_BIA);
        p->sq[MK_SQ(7 - f, 7)] = MK_PC(MK_BLACK, BACK[f]);   /* turned round: the Khun on e8, the Met on d8 */
    }
}

/* 1: the piece on `from` attacks `to` (a Bia only its two diagonals forwards) */
static int attacks(const mkpos *p, int from, int to)
{
    int pc = p->sq[from], df = MK_FILE(to) - MK_FILE(from), dr = MK_RANK(to) - MK_RANK(from);
    int adf = abs(df), adr = abs(dr), f = fwd(MK_COLOR(pc));
    switch (MK_KIND(pc)) {
    case MK_BIA: return dr == f && adf == 1;
    case MK_MET: return adf == 1 && adr == 1;
    case MK_KHON: return (adf == 1 && adr == 1) || (df == 0 && dr == f);
    case MK_MA: return (adf == 1 && adr == 2) || (adf == 2 && adr == 1);
    case MK_KHUN: return adf <= 1 && adr <= 1 && (adf || adr);
    case MK_RUA: {
        if ((df && dr) || (!df && !dr)) return 0;
        int step = df ? (df > 0 ? 1 : -1) : (dr > 0 ? 8 : -8);
        for (int s = from + step; s != to; s += step)
            if (p->sq[s]) return 0;
        return 1;
    }
    default: return 0;
    }
}

int mk_attacked(const mkpos *p, int sq, int by)
{
    for (int s = 0; s < 64; s++)
        if (p->sq[s] && MK_COLOR(p->sq[s]) == by && attacks(p, s, sq)) return 1;
    return 0;
}

int mk_king(const mkpos *p, int color)
{
    for (int s = 0; s < 64; s++)
        if (p->sq[s] == MK_PC(color, MK_KHUN)) return s;
    return -1;
}

int mk_in_check(const mkpos *p, int color)
{
    int k = mk_king(p, color);
    return k >= 0 && mk_attacked(p, k, !color);
}

/* a move to (file, rank), if on the board and not onto a piece of the mover's: empty (want 1), a capture (want 2),
 * either (3) */
static int add(const mkpos *p, int from, int file, int rank, int want, mkmove *out, int n)
{
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return n;
    int to = MK_SQ(file, rank), pc = p->sq[from], t = p->sq[to];
    if (t ? MK_COLOR(t) == MK_COLOR(pc) || !(want & 2) : !(want & 1)) return n;
    uint8_t promo = MK_KIND(pc) == MK_BIA && rank == MK_PROMO_RANK(MK_COLOR(pc));
    out[n] = (mkmove){(uint8_t)from, (uint8_t)to, promo, t};
    return n + 1;
}

static int pseudo(const mkpos *p, mkmove *out)
{
    int n = 0, me = p->side, f = fwd(me);
    for (int s = 0; s < 64; s++) {
        int pc = p->sq[s];
        if (!pc || MK_COLOR(pc) != me) continue;
        int x = MK_FILE(s), y = MK_RANK(s);
        switch (MK_KIND(pc)) {
        case MK_BIA:
            n = add(p, s, x, y + f, 1, out, n);
            n = add(p, s, x - 1, y + f, 2, out, n);
            n = add(p, s, x + 1, y + f, 2, out, n);
            break;
        case MK_KHON:                           /* straight forwards, then the Met's four diagonals */
            n = add(p, s, x, y + f, 3, out, n);
            /* fall through */
        case MK_MET:
            for (int d = 1; d < 8; d += 2) n = add(p, s, x + KING[d][0], y + KING[d][1], 3, out, n);
            break;
        case MK_MA:
            for (int d = 0; d < 8; d++) n = add(p, s, x + KNIGHT[d][0], y + KNIGHT[d][1], 3, out, n);
            break;
        case MK_KHUN:
            for (int d = 0; d < 8; d++) n = add(p, s, x + KING[d][0], y + KING[d][1], 3, out, n);
            break;
        case MK_RUA:
            for (int d = 0; d < 8; d += 2)
                for (int k = 1; k < 8; k++) {
                    int tx = x + k * KING[d][0], ty = y + k * KING[d][1], m = add(p, s, tx, ty, 3, out, n);
                    if (m == n) break;  /* off the board, or the mover's own piece */
                    n = m;
                    if (out[n - 1].taken) break;
                }
            break;
        }
    }
    return n;
}

/* the board without the counting (the legality test plays moves only to look at the Khun) */
static void move_pieces(mkpos *p, mkmove m)
{
    uint8_t pc = p->sq[m.from];
    p->sq[m.from] = 0;
    p->sq[m.to] = m.promo ? (uint8_t)(MK_PC(MK_COLOR(pc), MK_MET) | MK_PROMOTED) : pc;
}

int mk_legal(const mkpos *p, mkmove *out)
{
    mkmove all[MK_MAX_MOVES];
    int n = pseudo(p, all), k = 0;
    for (int i = 0; i < n; i++) {
        mkpos q = *p;
        move_pieces(&q, all[i]);
        if (!mk_in_check(&q, p->side)) out[k++] = all[i];
    }
    return k;
}

/* the counting's limit for the pieces' honour: the stronger side's pieces */
static int pieces_limit(const mkpos *p, int strong)
{
    int n[8] = {0};
    for (int s = 0; s < 64; s++)
        if (p->sq[s] && MK_COLOR(p->sq[s]) == strong) n[MK_KIND(p->sq[s])]++;
    return n[MK_RUA] >= 2 ? 8 : n[MK_RUA] ? 16 : n[MK_KHON] >= 2 ? 22 : n[MK_MA] >= 2 ? 32 : n[MK_KHON] ? 44 : 64;
}

/* after a move: the counts start (or the pieces' honour takes over) when there is no Bia left */
static void counting(mkpos *p)
{
    int n[2] = {0, 0};
    for (int s = 0; s < 64; s++) {
        int pc = p->sq[s];
        if (!pc) continue;
        if (MK_KIND(pc) == MK_BIA) return;      /* a Bia left: no count */
        n[MK_COLOR(pc)]++;
    }
    for (int c = 0; c < 2; c++)
        if (n[c] == 1 && n[!c] > 1) {           /* a bare Khun: the pieces' honour */
            if (p->count_kind == MK_COUNT_PIECES && p->counter == c) return;
            p->count_kind = MK_COUNT_PIECES;
            p->counter = (uint8_t)c;
            p->count = (uint8_t)(n[0] + n[1]);
            p->limit = (uint8_t)pieces_limit(p, !c);
            return;
        }
    if (p->count_kind == MK_COUNT_NONE) {       /* the board's honour */
        p->count_kind = MK_COUNT_BOARD;
        p->counter = p->side;
        p->count = 0;
        p->limit = 64;
    }
}

void mk_make(mkpos *p, mkmove m)
{
    int bia = MK_KIND(p->sq[m.from]) == MK_BIA;
    move_pieces(p, m);
    p->quiet = bia || m.taken ? 0 : p->quiet + 1;
    if (p->count_kind != MK_COUNT_NONE && p->counter == p->side && p->count < 255) p->count++;
    p->side = !p->side;
    counting(p);
}

int mk_moves_left(const mkpos *p)
{
    if (p->count_kind == MK_COUNT_NONE) return -1;
    int left = p->limit - p->count + (p->side != p->counter);
    return left < 0 ? 0 : left;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void mk_new(mkgame *g)
{
    g->ply = 0;
    mk_start(&g->pos[0]);
}

int mk_play(mkgame *g, mkmove m)
{
    if (g->ply >= MK_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    mk_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int mk_undo(mkgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

mkstatus mk_status(const mkgame *g)
{
    const mkpos *p = mk_pos(g);
    mkmove m[MK_MAX_MOVES];
    if (!mk_legal(p, m)) return mk_in_check(p, p->side) ? MS_CHECKMATE : MS_STALEMATE;
    int n = 0;
    for (int s = 0; s < 64; s++) n += p->sq[s] != 0;
    if (n == 2) return MS_BARE;
    if (p->count_kind != MK_COUNT_NONE && p->side == p->counter && p->count >= p->limit) return MS_COUNT;
    int seen = 0;                               /* back to the last capture or Bia move: nothing before repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) {
        const mkpos *q = &g->pos[i];
        seen += q->side == p->side && !memcmp(q->sq, p->sq, sizeof p->sq);
    }
    if (seen >= 3) return MS_REPETITION;
    return g->ply >= MK_MAX_PLY ? MS_LONG : MS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int mk_script(mkgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = mk_square(s), b;
        if (a < 0) return 0;
        s += 2;
        if (*s == '-' || *s == 'x') s++;
        if ((b = mk_square(s)) < 0) return 0;
        s += 2;
        if (*s && *s != ' ') return 0;
        mkmove l[MK_MAX_MOVES];
        int n = mk_legal(mk_pos(g), l), k = 0;
        while (k < n && !(l[k].from == a && l[k].to == b)) k++;
        if (k == n || !mk_play(g, l[k])) return 0;
    }
    return 1;
}
