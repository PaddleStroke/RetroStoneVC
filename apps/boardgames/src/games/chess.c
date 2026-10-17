/*
 * Chess rules (chess.h). Pseudo-legal moves from a plain 64-square board, kept when the mover's king is safe after
 * them. Castling follows Chess960's rules, of which standard castling is one case. Checked against the standard and
 * the Chess960 perft counts, and the variants' published ones (python-chess, Fairy-Stockfish: tests/perft.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "chess.h"

#include <stdio.h>
#include <stdlib.h>
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

/* the k-th empty square of the back rank */
static void put(char *rank, int k, char piece)
{
    for (int f = 0; f < 8; f++)
        if (!rank[f] && k-- == 0) { rank[f] = piece; return; }
}

/* Scharnagl's numbering: the light-squared bishop (b d f h), the dark one (a c e g), the queen on one of the six
 * empty squares, the knights on two of the five left, then rook, king, rook on the last three */
void chess_start960(cpos *p, int n)
{
    static const int8_t KNIGHTS[10][2] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4},
                                          {3, 4}};
    char rank[9] = {0}, fen[96];
    n = (n % 960 + 960) % 960;
    rank[2 * (n % 4) + 1] = 'b';
    n /= 4;
    rank[2 * (n % 4)] = 'b';
    n /= 4;
    put(rank, n % 6, 'q');
    n /= 6;
    put(rank, KNIGHTS[n][1], 'n');              /* the further one first: the nearer one's count stays the same */
    put(rank, KNIGHTS[n][0], 'n');
    put(rank, 0, 'r');
    put(rank, 0, 'k');
    put(rank, 0, 'r');
    char white[9];
    for (int f = 0; f < 9; f++) white[f] = rank[f] ? (char)(rank[f] - 32) : 0;
    snprintf(fen, sizeof fen, "%s/pppppppp/8/8/8/8/PPPPPPPP/%s w KQkq - 0 1", rank, white);
    chess_from_fen(p, fen);
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
    int r = 7, c = 0, hand = 0;
    for (; *f && *f != ' '; f++) {
        if (*f == '[') { hand = 1; continue; }                  /* crazyhouse: the pieces in hand */
        if (*f == ']') { hand = 0; continue; }
        if (*f == '~') {                                        /* the piece before it was a pawn */
            if (c > 0 && r >= 0) p->promoted |= 1ull << SQ(c - 1, r);
            continue;
        }
        if (*f == '/') { r--; c = 0; continue; }
        if (!hand && *f >= '1' && *f <= '8') { c += *f - '0'; continue; }
        const char *t = strchr(types, *f | 0x20);
        if (!t || !*t) return 0;
        if (hand) {
            p->hand[*f >= 'a' ? BLACK : WHITE][(int)(t - types) + 1]++;
            continue;
        }
        if (c > 7 || r < 0) return 0;
        p->sq[SQ(c, r)] = PC(*f >= 'a' ? BLACK : WHITE, (int)(t - types) + 1);
        c++;
    }
    while (*f == ' ') f++;
    p->side = *f == 'b' ? BLACK : WHITE;
    if (*f) f++;
    while (*f == ' ') f++;
    static const uint8_t usual[4] = {7, 0, 7, 0};
    memcpy(p->rook, usual, 4);
    for (; *f && *f != ' '; f++) {              /* K / Q: the outermost rook on that side (X-FEN); A..H: that file */
        int color = *f >= 'a' ? BLACK : WHITE, c = *f | 0x20, hr = color == WHITE ? 0 : 7, k = chess_king(p, color);
        if (k < 0 || RANK_OF(k) != hr) continue;
        int kf = FILE_OF(k), file = -1;
        if (c == 'k')
            for (int x = 7; x > kf && file < 0; x--) file = p->sq[SQ(x, hr)] == PC(color, ROOK) ? x : -1;
        else if (c == 'q')
            for (int x = 0; x < kf && file < 0; x++) file = p->sq[SQ(x, hr)] == PC(color, ROOK) ? x : -1;
        else if (c >= 'a' && c <= 'h')
            file = c - 'a';
        if (file < 0 || file == kf) continue;
        int bit = color * 2 + (file < kf);
        p->castle |= (uint8_t)(1 << bit);
        p->rook[bit] = (uint8_t)file;
    }
    while (*f == ' ') f++;
    if (*f >= 'a' && *f <= 'h' && f[1] >= '1' && f[1] <= '8') { p->ep = (int8_t)SQ(f[0] - 'a', f[1] - '1'); f += 2; }
    else if (*f == '-') f++;
    int hm = 0, fm = 1, nums = 0;
    for (;;) {                                  /* the move counters, and three-check's counts ("3+3" or "+0+0") */
        while (*f == ' ') f++;
        if (f[0] == '+' && f[1] >= '0' && f[1] <= '3' && f[2] == '+' && f[3] >= '0' && f[3] <= '3') {
            p->checks[WHITE] = (uint8_t)(f[1] - '0');
            p->checks[BLACK] = (uint8_t)(f[3] - '0');
            f += 4;
        } else if (f[0] >= '0' && f[0] <= '3' && f[1] == '+' && f[2] >= '0' && f[2] <= '3') {
            p->checks[WHITE] = (uint8_t)(3 - (f[0] - '0'));
            p->checks[BLACK] = (uint8_t)(3 - (f[2] - '0'));
            f += 3;
        } else if (*f >= '0' && *f <= '9' && nums < 2) {
            f = number(f, nums++ ? &fm : &hm);
        } else break;
    }
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

/* atomic: the two kings side by side (neither can be taken: it would blow up its taker's king too) */
static int kings_touch(const cpos *p)
{
    int a = chess_king(p, WHITE), b = chess_king(p, BLACK);
    return a >= 0 && b >= 0 && abs(FILE_OF(a) - FILE_OF(b)) <= 1 && abs(RANK_OF(a) - RANK_OF(b)) <= 1;
}

int chess_in_check(const cpos *p, int color)
{
    if (p->rules == CR_ANTI || (p->rules == CR_ATOMIC && kings_touch(p))) return 0;
    int k = chess_king(p, color);
    return k >= 0 && chess_attacked(p, k, !color);
}

static int add(cmove *out, int n, int from, int to, int flags, int promo)
{
    out[n].from = (uint8_t)from;
    out[n].to = (uint8_t)to;
    out[n].flags = (uint8_t)flags;
    out[n].promo = (uint8_t)promo;
    out[n].drop = 0;
    return n + 1;
}

/* promotes: to a queen, a knight, a rook or a bishop (losing chess, top KING: a king too) */
static int pawn_add(cmove *out, int n, int from, int to, int flags, int promotes, int top)
{
    if (!promotes) return add(out, n, from, to, flags, 0);
    for (int t = top; t >= KNIGHT; t--) n = add(out, n, from, to, flags, t);
    return n;
}

/* every square from a to b (on one rank) empty, but for the castling king and rook */
static int clear(const cpos *p, int a, int b, int k, int r)
{
    for (int s = a < b ? a : b, e = a < b ? b : a; s <= e; s++)
        if (p->sq[s] && s != k && s != r) return 0;
    return 1;
}

/* atomic: square s attacked by `them` in q, but for the squares round their king (taking there would blow it up) */
static int atomic_attacked(const cpos *q, int s, int them)
{
    int ek = chess_king(q, them);
    if (ek >= 0 && abs(FILE_OF(ek) - FILE_OF(s)) <= 1 && abs(RANK_OF(ek) - RANK_OF(s)) <= 1) return 0;
    return chess_attacked(q, s, them);
}

/* atomic's castling (python-chess's): the king's square and the squares it crosses safe with the king lifted (the
 * rook still home), its destination safe with the rook on its own; a square next to the other king is always safe */
static int castles_atomic(const cpos *p, int k, cmove *out, int n)
{
    int us = p->side, them = !us, hr = us == WHITE ? 0 : 7;
    if (RANK_OF(k) != hr) return n;
    for (int side = 0; side < 2; side++) {
        int bit = us * 2 + side;
        if (!(p->castle >> bit & 1)) continue;
        int r = SQ(p->rook[bit], hr), kt = SQ(side ? 2 : 6, hr), rt = SQ(side ? 3 : 5, hr);
        if (p->sq[r] != PC(us, ROOK) || !clear(p, k, kt, k, r) || !clear(p, r, rt, k, r)) continue;
        cpos q = *p;
        q.sq[k] = EMPTY;
        int safe = !atomic_attacked(&q, k, them);
        for (int s = (k < kt ? k : kt) + 1, e = k < kt ? kt : k; s < e && safe; s++)
            safe = !atomic_attacked(&q, s, them);
        q.sq[r] = EMPTY;
        q.sq[rt] = PC(us, ROOK);
        if (safe && !atomic_attacked(&q, kt, them)) n = add(out, n, k, r, MV_CASTLE, 0);
    }
    return n;
}

/* the king on k castles: onto the rook's square (chess.h). The king and the rook end on g and f (or c and d), the
 * squares they cross empty; the king not in check, crossing and reaching only squares no enemy attacks (seen with
 * the king and the rook off the board: a rook may hide an attack on the king's new square). Leaving the rook on gives
 * the same moves, as chess_legal() then finds the king in check once castled: the only line the rook can shut is
 * along the back rank from beyond it, and that line then reaches the castled king (tests/perft.c: against FIDE's
 * rule) */
static int castles(const cpos *p, int k, cmove *out, int n)
{
    int us = p->side, them = !us, hr = us == WHITE ? 0 : 7;
    if (p->rules == CR_ATOMIC) return castles_atomic(p, k, out, n);
    if (RANK_OF(k) != hr || chess_attacked(p, k, them)) return n;
    for (int side = 0; side < 2; side++) {     /* 0: towards h, 1: towards a */
        int bit = us * 2 + side;
        if (!(p->castle >> bit & 1)) continue;
        int r = SQ(p->rook[bit], hr), kt = SQ(side ? 2 : 6, hr), rt = SQ(side ? 3 : 5, hr);
        if (p->sq[r] != PC(us, ROOK) || !clear(p, k, kt, k, r) || !clear(p, r, rt, k, r)) continue;
        cpos q = *p;
        q.sq[k] = q.sq[r] = EMPTY;
        int safe = 1;
        for (int s = k < kt ? k : kt, e = k < kt ? kt : k; s <= e && safe; s++) safe = !chess_attacked(&q, s, them);
        if (safe) n = add(out, n, k, r, MV_CASTLE, 0);
    }
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
    int n = 0, us = p->side, top = p->rules == CR_ANTI ? KING : QUEEN;
    for (int s = 0; s < 64; s++) {
        int pc = p->sq[s];
        if (!pc || COLOR(pc) != us) continue;
        int f = FILE_OF(s), r = RANK_OF(s), t = TYPE(pc);
        if (t == PAWN) {
            int dir = us == WHITE ? 1 : -1, start = us == WHITE ? 1 : 6, last = us == WHITE ? 7 : 0, r1 = r + dir;
            if (!on(f, r1)) continue;
            if (!p->sq[SQ(f, r1)]) {
                n = pawn_add(out, n, s, SQ(f, r1), 0, r1 == last, top);
                if (r == start && !p->sq[SQ(f, r1 + dir)]) n = add(out, n, s, SQ(f, r1 + dir), MV_DOUBLE, 0);
                /* the horde: from the first rank too, two squares (no en passant after it) */
                else if (p->rules == CR_HORDE && r == start - dir && !p->sq[SQ(f, r1 + dir)])
                    n = add(out, n, s, SQ(f, r1 + dir), 0, 0);
            }
            for (int df = -1; df <= 1; df += 2) {
                if (!on(f + df, r1)) continue;
                int to = SQ(f + df, r1), q = p->sq[to];
                if (q && COLOR(q) != us) n = pawn_add(out, n, s, to, MV_CAPTURE, r1 == last, top);
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
            if (t == KING) n = castles(p, s, out, n);
        } else {
            if (t != ROOK) n = slide(p, s, DIAG, 4, out, n);
            if (t != BISHOP) n = slide(p, s, ORTH, 4, out, n);
        }
    }
    return n;
}

/* crazyhouse: the pieces in hand onto every empty square (a pawn not on the first or the last rank) */
static int drops(const cpos *p, cmove *out, int n)
{
    for (int t = PAWN; t <= QUEEN; t++) {
        if (!p->hand[p->side][t]) continue;
        for (int s = 0; s < 64; s++)
            if (!p->sq[s] && (t != PAWN || (RANK_OF(s) > 0 && RANK_OF(s) < 7))) {
                n = add(out, n, s, s, MV_DROP, 0);
                out[n - 1].drop = (uint8_t)t;
            }
    }
    return n;
}

/* racing kings: over when a king stands on the eighth rank, unless the white king got there and black, to move, can
 * still bring its king there too (a square next to it on the eighth rank, not black's own, that white does not
 * attack: python-chess's reading) */
static int race_over(const cpos *p)
{
    int w = chess_king(p, WHITE), b = chess_king(p, BLACK), w8 = w >= 0 && RANK_OF(w) == 7,
        b8 = b >= 0 && RANK_OF(b) == 7;
    if (!w8 && !b8) return 0;
    if (w8 && b8) return ST_RACE_DRAW;
    if (b8 || p->side == WHITE || b < 0) return ST_RACE;
    for (int i = 0; i < 8; i++) {
        int f = FILE_OF(b) + KG[i][0], r = RANK_OF(b) + KG[i][1];
        if (!on(f, r) || r != 7) continue;
        int q = p->sq[SQ(f, r)];
        if ((!q || COLOR(q) != BLACK) && !chess_attacked(p, SQ(f, r), WHITE)) return 0;
    }
    return ST_RACE;
}

int chess_over(const cpos *p)
{
    if (p->rules == CR_THREECHECK && (p->checks[WHITE] >= 3 || p->checks[BLACK] >= 3)) return ST_CHECKS;
    if (p->rules == CR_HILL)
        for (int c = 0; c < 2; c++) {
            int k = chess_king(p, c);
            if (k >= 0 && CHESS_HILL(k)) return ST_HILL;
        }
    if (p->rules == CR_ATOMIC && (chess_king(p, WHITE) < 0 || chess_king(p, BLACK) < 0)) return ST_EXPLODED;
    if (p->rules == CR_RACING) return race_over(p);
    if (p->rules == CR_HORDE) {
        for (int s = 0; s < 64; s++)
            if (p->sq[s] && COLOR(p->sq[s]) == WHITE) return 0;
        return ST_HORDE;
    }
    return 0;
}

int chess_legal(const cpos *p, cmove *out)
{
    cmove all[MAX_MOVES];
    if (p->rules != CR_CHESS && chess_over(p)) return 0;
    int n = pseudo(p, all), k = 0;
    if (p->rules == CR_CRAZYHOUSE) n = drops(p, all, n);
    if (p->rules == CR_ANTI) {                  /* no check; a capture when there is one */
        int some = 0, theirs = 0;
        for (int s = 0; s < 64; s++) theirs |= p->sq[s] && COLOR(p->sq[s]) != p->side;
        if (!theirs) return 0;                  /* they have nothing left: the game is over */
        for (int i = 0; i < n; i++) some |= (all[i].flags & MV_CAPTURE) != 0;
        for (int i = 0; i < n; i++)
            if (!some || (all[i].flags & MV_CAPTURE)) out[k++] = all[i];
        return k;
    }
    for (int i = 0; i < n; i++) {
        cpos q = *p;
        chess_make(&q, all[i]);
        if (p->rules == CR_ATOMIC) {            /* our king still there; theirs blown up, or ours not in check */
            if (chess_king(&q, p->side) < 0) continue;
            if (chess_king(&q, !p->side) >= 0 && chess_in_check(&q, p->side)) continue;
        } else if (chess_in_check(&q, p->side)) continue;
        if (p->rules == CR_RACING && chess_in_check(&q, !p->side)) continue;   /* no check given */
        out[k++] = all[i];
    }
    return k;
}

/* crazyhouse: the piece taken on s goes to the taker's hand (a promoted one as a pawn) */
static void to_hand(cpos *p, int s, int us)
{
    if (!p->sq[s]) return;
    p->hand[us][p->promoted >> s & 1 ? PAWN : TYPE(p->sq[s])]++;
    p->promoted &= ~(1ull << s);
}

/* atomic: the capture on s explodes: the piece that took, and every piece but a pawn round s; the castling rights of
 * a rook or a king blown up go with it */
static void explode(cpos *p, int s)
{
    p->sq[s] = EMPTY;
    for (int i = 0; i < 8; i++) {
        int f = FILE_OF(s) + KG[i][0], r = RANK_OF(s) + KG[i][1];
        if (!on(f, r)) continue;
        int t = SQ(f, r), q = p->sq[t];
        if (q && TYPE(q) == PAWN) continue;
        for (int b = 0; b < 4; b++)
            if (t == SQ(p->rook[b], b < 2 ? 0 : 7)) p->castle &= (uint8_t)~(1 << b);
        if (q && TYPE(q) == KING) p->castle &= (uint8_t)~(3 << COLOR(q) * 2);
        p->sq[t] = EMPTY;
    }
}

void chess_make(cpos *p, cmove m)
{
    int pc = p->sq[m.from], us = p->side, them = !us;
    int resets = TYPE(pc) == PAWN || (p->sq[m.to] && !(m.flags & MV_CASTLE));
    if (m.flags & MV_DROP) {                    /* a piece from the hand: nothing else changes but the turn */
        p->sq[m.to] = PC(us, m.drop);
        p->hand[us][m.drop]--;
        p->ep = -1;
        p->halfmove++;
        if (us == BLACK) p->fullmove++;
        p->side = (uint8_t)them;
        if (p->rules == CR_THREECHECK && chess_in_check(p, them)) p->checks[us]++;
        return;
    }
    if (p->rules == CR_CRAZYHOUSE && !(m.flags & MV_CASTLE)) {
        to_hand(p, m.flags & MV_EP ? SQ(FILE_OF(m.to), RANK_OF(m.from)) : m.to, us);
        int was = p->promoted >> m.from & 1;
        p->promoted &= ~(1ull << m.from);
        if (was || m.promo) p->promoted |= 1ull << m.to;
    }
    if (m.flags & MV_CASTLE) {                  /* the king and the rook both lift, then land */
        int rook = p->sq[m.to];
        p->sq[m.from] = p->sq[m.to] = EMPTY;
        p->sq[chess_castle_king(m)] = (uint8_t)pc;
        p->sq[chess_castle_rook(m)] = (uint8_t)rook;
    } else {
        p->sq[m.to] = m.promo ? PC(us, m.promo) : (uint8_t)pc;
        p->sq[m.from] = EMPTY;
        if (m.flags & MV_EP) p->sq[SQ(FILE_OF(m.to), RANK_OF(m.from))] = EMPTY;
    }
    /* the castling rights: lost when the king moves, or a rook leaves its square or is taken there */
    if (TYPE(pc) == KING) p->castle &= (uint8_t)~(3 << us * 2);
    for (int i = 0; i < 4; i++) {
        int home = SQ(p->rook[i], i < 2 ? 0 : 7);
        if (m.from == home || m.to == home) p->castle &= (uint8_t)~(1 << i);
    }
    if (p->rules == CR_ATOMIC && (m.flags & MV_CAPTURE)) explode(p, m.to);
    p->ep = -1;
    if (m.flags & MV_DOUBLE) {                      /* only when it can be taken: repeated positions compare right */
        int f = FILE_OF(m.to), r = RANK_OF(m.to);
        if ((f > 0 && p->sq[SQ(f - 1, r)] == PC(them, PAWN)) || (f < 7 && p->sq[SQ(f + 1, r)] == PC(them, PAWN)))
            p->ep = (int8_t)((m.from + m.to) / 2);
    }
    p->halfmove = resets ? 0 : (uint16_t)(p->halfmove + 1);
    if (us == BLACK) p->fullmove++;
    p->side = (uint8_t)them;
    if (p->rules == CR_THREECHECK && chess_in_check(p, them)) p->checks[us]++;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void game_new(cgame *g)
{
    g->ply = 0;
    chess_start(&g->pos[0]);
}

void game_new960(cgame *g, int n)
{
    g->ply = 0;
    chess_start960(&g->pos[0], n);
}

void game_new_rules(cgame *g, int rules)
{
    g->ply = 0;
    if (rules == CR_RACING) chess_from_fen(&g->pos[0], "8/8/8/8/8/8/krbnNBRK/qrbnNBRQ w - - 0 1");
    else if (rules == CR_HORDE)
        chess_from_fen(&g->pos[0], "rnbqkbnr/pppppppp/8/1PP2PP1/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPP w kq - 0 1");
    else chess_start(&g->pos[0]);
    if (rules == CR_ANTI) g->pos[0].castle = 0; /* the king is a plain piece: it never castles */
    g->pos[0].rules = (uint8_t)rules;
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
    return !memcmp(a->sq, b->sq, 64) && a->side == b->side && a->castle == b->castle && a->ep == b->ep &&
           !memcmp(a->hand, b->hand, sizeof a->hand) && a->promoted == b->promoted &&
           a->checks[0] == b->checks[0] && a->checks[1] == b->checks[1];
}

/* no checkmate is possible: kings alone, a king and one minor piece, or bishops all on one colour of squares */
/* the pieces by colour and type, and the bishops by colour and square colour (0: dark, a1's) */
typedef struct { int n[2][7], bishops[2][2], all; } census;
static census count_pieces(const cpos *p)
{
    census c;
    memset(&c, 0, sizeof c);
    for (int s = 0; s < 64; s++) {
        int q = p->sq[s];
        if (!q) continue;
        c.n[COLOR(q)][TYPE(q)]++;
        c.all++;
        if (TYPE(q) == BISHOP) c.bishops[COLOR(q)][(FILE_OF(s) + RANK_OF(s)) & 1]++;
    }
    return c;
}

/* atomic: `color` can never win (python-chess's has_insufficient_material) */
static int atomic_poor(const census *c, int color)
{
    int them = !color, ours = 0, theirs = 0;
    for (int t = PAWN; t <= QUEEN; t++) ours += c->n[color][t], theirs += c->n[them][t];
    if (!c->n[them][KING]) return 0;
    if (!ours) return 1;                        /* a bare king */
    if (theirs) {                               /* their pieces may blow up next to their king: unless bishops alone */
        int b = c->n[0][BISHOP] + c->n[1][BISHOP], k = c->n[0][KING] + c->n[1][KING];
        if (b + k == c->all) {
            if (!c->bishops[WHITE][0]) return !c->bishops[BLACK][1];
            if (!c->bishops[WHITE][1]) return !c->bishops[BLACK][0];
        }
        return 0;
    }
    int q = c->n[0][QUEEN] + c->n[1][QUEEN] + c->n[0][PAWN] + c->n[1][PAWN];
    if (q) return 0;
    int minor = 0, knights = c->n[0][KNIGHT] + c->n[1][KNIGHT];
    for (int s = 0; s < 2; s++) minor += c->n[s][KNIGHT] + c->n[s][BISHOP] + c->n[s][ROOK];
    if (minor == 1) return 1;
    if (knights + c->n[0][KING] + c->n[1][KING] == c->all) return knights <= 2;
    return 0;
}

static int insufficient(const cpos *p)
{
    int minors = 0, bishops[2] = {0, 0};
    if (p->rules == CR_CRAZYHOUSE || p->rules == CR_HILL) return 0;   /* drops; a king walking to the hill */
    if (p->rules == CR_RACING || p->rules == CR_HORDE) return 0;      /* a king can always run; the horde */
    if (p->rules == CR_ATOMIC) {
        census c = count_pieces(p);
        return atomic_poor(&c, WHITE) && atomic_poor(&c, BLACK);
    }
    if (p->rules == CR_ANTI) {                  /* bishops alone, each side's on its own colour of squares */
        census c = count_pieces(p);
        if (c.n[0][BISHOP] + c.n[1][BISHOP] != c.all || !c.n[0][BISHOP] || !c.n[1][BISHOP]) return 0;
        for (int sc = 0; sc < 2; sc++)
            if (c.bishops[WHITE][sc] == c.n[WHITE][BISHOP] && c.bishops[BLACK][!sc] == c.n[BLACK][BISHOP]) return 1;
        return 0;
    }
    if (p->rules == CR_THREECHECK) {            /* any piece may still check */
        for (int s = 0; s < 64; s++)
            if (p->sq[s] && TYPE(p->sq[s]) != KING) return 0;
        return 1;
    }
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
    int over = p->rules == CR_CHESS ? 0 : chess_over(p);
    if (over) return (cstatus)over;
    if (!chess_legal(p, m)) {
        if (p->rules == CR_ANTI) return ST_ANTI;   /* nothing left, or stalemated: the side to move won */
        return chess_in_check(p, p->side) ? ST_CHECKMATE : ST_STALEMATE;
    }
    if (insufficient(p)) return ST_INSUFFICIENT;
    if (p->halfmove >= 100) return ST_FIFTY;
    int seen = 0;
    for (int i = g->ply; i >= 0 && g->ply - i <= p->halfmove; i--)
        if (same(&g->pos[i], p) && ++seen >= 3) return ST_REPETITION;
    return ST_PLAY;
}

int chess_move_text(cmove m, char *out, int cap)
{
    char s[8];
    if (m.flags & MV_DROP) {
        s[0] = "?PNBRQ"[m.drop];
        s[1] = '@';
        s[2] = (char)('a' + FILE_OF(m.to));
        s[3] = (char)('1' + RANK_OF(m.to));
        s[4] = 0;
    } else {
        s[0] = (char)('a' + FILE_OF(m.from));
        s[1] = (char)('1' + RANK_OF(m.from));
        s[2] = (char)('a' + FILE_OF(m.to));
        s[3] = (char)('1' + RANK_OF(m.to));
        s[4] = m.promo ? "??nbrqk"[m.promo] : 0;
        s[5] = 0;
    }
    if (!out || cap < (int)strlen(s) + 1) return 0;
    memcpy(out, s, strlen(s) + 1);
    return 1;
}
