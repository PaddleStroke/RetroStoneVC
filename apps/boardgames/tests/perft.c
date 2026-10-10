/*
 * The rules against the standard perft counts (the number of move sequences of a given length): the start position
 * and the usual tricky ones (castling through check, en passant pins, promotions). Then the draw rules.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>

#include "chess.h"

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
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
