/*
 * The rules against the standard perft counts (the number of move sequences of a given length): the start position
 * and the usual tricky ones (castling through check, en passant pins, promotions). Then the draw rules, then
 * Chess960: its perft suite, its 960 start positions, its castling.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/chess.h"

static long perft(const cpos *p, int depth)
{
    cmove m[MAX_MOVES];
    int n = chess_legal(p, m);
    if (depth == 1) return n;
    long total = 0;
    for (int i = 0; i < n; i++) {
        cpos q = *p;
        chess_make(&q, m[i]);
        total += perft(&q, depth - 1);
    }
    return total;
}

static int fails;

static void check(const char *name, const char *fen, int depth, long want)
{
    cpos p;
    chess_from_fen(&p, fen);
    long got = perft(&p, depth);
    printf("%s %-10s depth %d: %ld%s\n", got == want ? "ok  " : "FAIL", name, depth, got,
           got == want ? "" : " (wrong)");
    fails += got != want;
}

static cmove find(const cpos *p, const char *uci)
{
    cmove m[MAX_MOVES], none = {0, 0, 0, 0};
    int n = chess_legal(p, m), from = (uci[0] - 'a') + (uci[1] - '1') * 8, to = (uci[2] - 'a') + (uci[3] - '1') * 8;
    for (int i = 0; i < n; i++)
        if (m[i].from == from && m[i].to == to && (!m[i].promo || m[i].promo == QUEEN)) return m[i];
    return none;
}

static void status(const char *name, const char *moves, cstatus want)
{
    static cgame g;
    game_new(&g);
    for (const char *s = moves; *s; ) {
        game_play(&g, find(game_pos(&g), s));
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    cstatus got = game_status(&g);
    printf("%s %-22s status %d\n", got == want ? "ok  " : "FAIL", name, got);
    fails += got != want;
}

/* Chess960's 960 start positions: each legal (bishops on both colours, the king between the rooks, the same for
 * both sides, every castling right), all different, number 518 the standard one; and one castling played out */
static void starts960(void)
{
    static uint8_t seen[960][8];
    int bad = 0, same = 0;
    for (int n = 0; n < 960; n++) {
        cpos p;
        chess_start960(&p, n);
        int b[2] = {0, 0}, k = -1, rooks = 0, between = 0, mirror = 1;
        for (int f = 0; f < 8; f++) {
            int t = TYPE(p.sq[SQ(f, 0)]);
            seen[n][f] = (uint8_t)t;
            mirror &= p.sq[SQ(f, 7)] == PC(BLACK, t) && p.sq[SQ(f, 1)] == PC(WHITE, PAWN);
            if (t == BISHOP) b[f & 1]++;
            if (t == KING) k = f;
            if (t == ROOK) { rooks++; between += k >= 0; }
        }
        bad += b[0] != 1 || b[1] != 1 || rooks != 2 || between != 1 || !mirror || p.castle != 15 ||
               p.rook[0] <= k || p.rook[1] >= k;
        for (int m = 0; m < n; m++) same += !memcmp(seen[m], seen[n], 8);
    }
    cpos a, s;
    chess_start960(&a, 518);
    chess_start(&s);
    int standard = !memcmp(&a, &s, sizeof a);
    printf("%s 960 start positions: %d not legal, %d repeated, 518 %s\n", bad || same || !standard ? "FAIL" : "ok  ",
           bad, same, standard ? "standard" : "NOT standard");
    fails += bad || same || !standard;

    /* the king on f1, the rook on g1: castling on the h side swaps them (the move goes onto the rook) */
    int ok = 0;
    chess_from_fen(&a, "1nr1bkr1/pppppppp/8/8/8/8/PPPPPPPP/1NR1BKR1 w GCgc - 0 1");
    cmove m[MAX_MOVES];
    int n = chess_legal(&a, m);
    for (int i = 0; i < n; i++)
        if ((m[i].flags & MV_CASTLE) && m[i].to == SQ(6, 0)) {
            chess_make(&a, m[i]);
            ok = a.sq[SQ(6, 0)] == PC(WHITE, KING) && a.sq[SQ(5, 0)] == PC(WHITE, ROOK) && !(a.castle & 3) &&
                 (a.castle & 12) == 12;
        }
    printf("%s 960 castling: king g1, rook f1\n", ok ? "ok  " : "FAIL");
    fails += !ok;
}

int main(void)
{
    const char *start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    check("start", start, 1, 20);
    check("start", start, 2, 400);
    check("start", start, 3, 8902);
    check("start", start, 4, 197281);
    check("kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862);
    check("position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238);
    check("position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467);
    check("position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379);
    status("fool's mate", "f2f3 e7e5 g2g4 d8h4", ST_CHECKMATE);
    status("threefold repetition", "g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8", ST_REPETITION);
    status("still playing", "e2e4 e7e5", ST_PLAY);

    /* the same castling written with the rooks' files (Shredder-FEN) */
    check("kiwipete HAha", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w HAha - 0 1", 3, 97862);
    /* Chess960: the published perft suite (chess960.epd) */
    const char *c1 = "bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9";
    check("960 #1", c1, 1, 21);
    check("960 #1", c1, 2, 528);
    check("960 #1", c1, 3, 12189);
    check("960 #1", c1, 4, 326672);
    const char *c2 = "2nnrbkr/p1qppppp/8/1ppb4/6PP/3PP3/PPP2P2/BQNNRBKR w HEhe - 1 9";
    check("960 #2", c2, 1, 21);
    check("960 #2", c2, 2, 807);
    check("960 #2", c2, 3, 18002);
    const char *c3 = "b1q1rrkb/pppppppp/3nn3/8/P7/1PPP4/4PPPP/BQNNRKRB w GE - 1 9";
    check("960 #3", c3, 1, 20);
    check("960 #3", c3, 2, 479);
    check("960 #3", c3, 3, 10471);
    check("960 #5", "1nbbnrkr/p1p1ppp1/3p4/1p3P1p/3Pq2P/8/PPP1P1P1/QNBBNRKR w HFhf - 0 9", 3, 31058);
    check("960 #6", "qnbnr1kr/ppp1b1pp/4p3/3p1p2/8/2NPP3/PPP1BPPP/QNB1R1KR w HEhe - 1 9", 3, 26578);
    check("960 #7", "q1bnrkr1/ppppp2p/2n2p2/4b1p1/2NP4/8/PPP1PPPP/QNB1RRKB w ge - 1 9", 3, 24566);
    check("960 #8", "qbn1brkr/ppp1p1p1/2n4p/3p1p2/P7/6PP/QPPPPP2/1BNNBRKR w HFhf - 0 9", 3, 17054);
    check("960 #9", "qnnbbrkr/1p2ppp1/2pp3p/p7/1P5P/2NP4/P1P1PPP1/Q1NBBRKR w HFhf - 0 9", 3, 15243);
    check("960 #10", "qn1rbbkr/ppp2p1p/1n1pp1p1/8/3P4/P6P/1PP1PPPK/QNNRBB1R w hd - 2 9", 3, 23175);
    /* Stockfish's Chess960 checks: a king on b1 castling with the e1 rook, the rook hiding an attack */
    check("960 sf 1", "rr6/2kpp3/1ppn2p1/p2b1q1p/P4P1P/1PNN2P1/2PP4/1K2R2R b E - 1 20", 2, 1438);
    check("960 sf 2", "rr6/2kpp3/1ppn2p1/p2b1q1p/P4P1P/1PNN2P1/2PP4/1K2RR2 w E - 0 20", 3, 37340);
    check("960 sf 3", "rr6/2kpp3/1ppnb1p1/p2Q1q1p/P4P1P/1PNN2P1/2PP4/1K2RR2 b E - 2 19", 4, 2237725);
    check("960 sf 4", "rr6/2kpp3/1ppnb1p1/p4q1p/P4P1P/1PNN2P1/2PP2Q1/1K2RR2 w E - 1 19", 4, 2098209);

    starts960();
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
