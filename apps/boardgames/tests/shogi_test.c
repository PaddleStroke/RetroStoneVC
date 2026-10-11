/*
 * The shogi rules on a few positions (SFEN): each piece's moves, the promotions (optional, compulsory), the drops and
 * their three limits (nifu, a piece that could never move, uchifuzume), check, mate, the repetition draw and perpetual
 * check, undo, and the move counts from the start (perft 30, 900, 25470); mini shogi (5 x 5): its start and perft,
 * its one-rank promotion zone, its drops, and sennichite lost by sente.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/shogi.h"

static int fails;
static spos P;
static smove M[SG_MAX_MOVES];
static int N;

static void ok(const char *name, int good)
{
    printf("%s %s\n", good ? "ok  " : "FAIL", name);
    fails += !good;
}

static void pos(const char *sfen)
{
    if (!sg_from_sfen(&P, sfen)) printf("bad sfen %s\n", sfen);
    N = sg_legal(&P, M);
}

static int has(const char *move)
{
    int used;
    return sg_parse(P.n, M, N, move, &used) >= 0;
}

/* the legal moves of the piece on a square ("5e"), each promotion counted apart */
static int from(const char *sq)
{
    int col = P.n - (sq[0] - '0'), row = sq[1] - 'a', n = 0;
    for (int i = 0; i < N; i++) n += !M[i].drop && M[i].from == SG_SQ(col, row);
    return n;
}

static int drops(int kind)
{
    int n = 0;
    for (int i = 0; i < N; i++) n += M[i].drop == kind;
    return n;
}

static long perft(const spos *p, int depth)
{
    smove m[SG_MAX_MOVES];
    int n = sg_legal(p, m);
    if (depth == 1) return n;
    long t = 0;
    for (int i = 0; i < n; i++) {
        spos q = *p;
        sg_make(&q, m[i]);
        t += perft(&q, depth - 1);
    }
    return t;
}

/* plays the moves (USI) from a position; the game's status after them, -1 if a move is not legal */
static int game(const char *sfen, const char *moves)
{
    static sgame g;
    g.ply = 0;
    sg_from_sfen(&g.pos[0], sfen);
    for (const char *s = moves; *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        smove m[SG_MAX_MOVES];
        int n = sg_legal(sg_pos(&g), m), used, k = sg_parse(sg_pos(&g)->n, m, n, s, &used);
        if (k < 0) return -1;
        sg_play(&g, m[k]);
        s += used;
    }
    return sg_status(&g);
}

#define START "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
#define MINI "rbsgk/4p/5/P4/KGSBR b - 1"

int main(void)
{
    spos s;
    sg_start(&s, SG_STANDARD);
    ok("perft 1 = 30", perft(&s, 1) == 30);
    ok("perft 2 = 900", perft(&s, 2) == 900);
    ok("perft 3 = 25470", perft(&s, 3) == 25470);

    /* the pieces, alone in the middle (the kings out of the way) */
    pos("4k4/9/9/9/4G4/9/9/9/4K4 b - 1");
    ok("gold: 6", from("5e") == 6 && has("5e4d") && has("5e5f") && !has("5e4f"));
    pos("4k4/9/9/9/4S4/9/9/9/4K4 b - 1");
    ok("silver: 5", from("5e") == 5 && has("5e4f") && !has("5e5f") && !has("5e4e"));
    pos("4k4/9/9/9/9/9/4N4/9/4K4 b - 1");
    ok("knight: 2 jumps", from("5g") == 2 && has("5g4e") && has("5g6e"));
    pos("k8/9/9/9/9/9/9/9/4K3L b - 1");
    ok("lance: 8 squares, 1c 1b may promote, 1a must", from("1i") == 10 && has("1i1a+") && !has("1i1a") &&
                                                       has("1i1b") && has("1i1b+"));
    pos("4k4/9/9/9/4B4/9/9/9/4K4 b - 1");
    ok("bishop: 16 squares, 6 of them in the zone", from("5e") == 22);
    pos("8k/9/9/9/4R4/9/9/9/K8 b - 1");
    ok("rook: 16 squares, 3 of them in the zone", from("5e") == 19);
    pos("8k/9/9/9/4+R4/9/9/9/K8 b - 1");
    ok("dragon: 16 + 4", from("5e") == 20);
    pos("4k4/9/9/9/4+B4/9/9/9/4K4 b - 1");
    ok("horse: 16 + 4", from("5e") == 20);
    pos("4k4/9/9/9/4+S4/9/9/9/4K4 b - 1");
    ok("promoted silver moves as a gold", from("5e") == 6);
    pos("4k4/9/9/9/4K4/9/9/9/9 b - 1");
    ok("king: 8", from("5e") == 8);
    pos("4k4/9/9/9/4g4/9/9/9/K8 w - 1");
    ok("gote's gold goes down the board", from("5e") == 6 && has("5e5f") && has("5e4f") && has("5e5d") &&
                                          !has("5e4d"));
    pos("4k4/9/9/4P4/9/9/9/9/4K4 b - 1");
    ok("pawn into the zone: may promote", from("5d") == 2 && has("5d5c") && has("5d5c+"));
    pos("k8/4P4/9/9/9/9/9/9/4K4 b - 1");
    ok("pawn to the last rank: must promote", from("5b") == 1 && has("5b5a+"));
    pos("k8/9/4N4/9/9/9/9/9/4K4 b - 1");
    ok("knight to the last two ranks: must promote", from("5c") == 2 && has("5c4a+") && !has("5c4a"));
    pos("k8/4R4/9/9/9/9/9/9/4K4 b - 1");
    ok("a move out of the zone may promote too", has("5b5e+") && has("5b5e") && from("5b") == 2 * 15);

    /* a capture: the piece changes sides, unpromoted */
    {
        static sgame g;
        sg_new(&g, SG_STANDARD);
        const char *seq[] = {"7g7f", "3c3d", "8h2b+", "3a2b"};
        for (int i = 0; i < 4; i++) {
            smove m[SG_MAX_MOVES];
            int n = sg_legal(sg_pos(&g), m), used, k = sg_parse(9, m, n, seq[i], &used);
            if (k >= 0) sg_play(&g, m[k]);
        }
        const spos *p = sg_pos(&g);
        ok("bishop for bishop: one each in hand, unpromoted", g.ply == 4 && p->hand[0][SG_BISHOP] == 1 &&
           p->hand[1][SG_BISHOP] == 1 && p->sq[SG_SQ(7, 1)] == SG_PC(1, SG_SILVER));
        pos("4k4/9/9/9/9/9/9/9/4K4 b B 1");
        ok("a bishop drops on every empty square", drops(SG_BISHOP) == 79);
        for (int i = 0; i < 4; i++) sg_undo(&g);
        spos st;
        sg_start(&st, SG_STANDARD);
        ok("undo all the way", g.ply == 0 && !memcmp(sg_pos(&g), &st, sizeof st) && !sg_undo(&g));
    }

    /* drops */
    pos("4k4/9/9/9/4P4/9/9/9/4K4 b P 1");
    ok("nifu: no pawn drop on the 5 file", !has("P*5f") && !has("P*5c") && has("P*4f") && drops(SG_PAWN) == 8 * 8);
    pos("4k4/9/9/9/9/9/9/9/4K4 b LN 1");
    ok("no drop where the piece could never move", !has("L*1a") && has("L*1b") && !has("N*1b") && has("N*1c") &&
                                                    drops(SG_LANCE) == 79 - 8 && drops(SG_KNIGHT) == 79 - 17);
    pos("8k/9/6NG1/9/9/9/9/9/4K4 b P 1");
    ok("uchifuzume: no pawn drop that mates", !has("P*1b") && has("P*2b"));
    pos("8k/9/7G1/9/9/9/9/9/4K4 b P 1");
    ok("a pawn drop that checks but does not mate", has("P*1b"));
    ok("a pawn moved (not dropped) may mate", game("8k/9/6NGP/9/9/9/9/9/4K4 b - 1", "1c1b") == SG_WON0);

    /* check */
    pos("k3r4/9/9/9/9/9/9/9/4K4 b G 1");
    int safe = 1;
    for (int i = 0; i < N; i++) {
        spos q = P;
        sg_make(&q, M[i]);
        safe &= !sg_in_check(&q, 0);
    }
    ok("check: 4 king moves and 7 gold drops between", sg_in_check(&P, 0) && N == 11 && safe && !has("5i5h"));
    pos("k8/9/9/9/9/9/9/4r4/3GKG3 b - 1");
    ok("check: take the checking piece", N == 3 && has("5i5h") && has("6i5h") && has("4i5h"));
    ok("checkmate: the gold drop", game("4k4/9/4G4/9/9/9/9/9/4K4 b G 1", "G*5b") == SG_WON0);
    ok("checkmate: gote mates", game("4k4/9/9/9/9/9/4g4/9/4K4 w g 1", "G*5h") == SG_WON1);
    ok("no mate: the king takes", game("4k4/9/9/9/9/9/9/9/4K4 w g 1", "G*5h") == SG_PLAY);

    /* repetition: the start four times; perpetual check loses */
    ok("three times: play on", game(START, "5i4h 5a4b 4h5i 4b5a 5i4h 5a4b 4h5i 4b5a") == SG_PLAY);
    ok("four times: a draw", game(START, "5i4h 5a4b 4h5i 4b5a 5i4h 5a4b 4h5i 4b5a 5i4h 5a4b 4h5i 4b5a") == SG_DRAW);
    ok("perpetual check: the checking side loses",
       game("8k/9/4R4/9/9/9/9/9/4K4 b - 1",
            "5c5a 1a1b 5a5b 1b1a 5b5a 1a1b 5a5b 1b1a 5b5a 1a1b 5a5b 1b1a 5b5a") == SG_WON1);

    /* the notation round trip */
    sg_start(&P, SG_STANDARD);
    N = sg_legal(&P, M);
    int round = 1;
    for (int i = 0; i < N; i++) {
        char u[8];
        int used;
        sg_usi(P.n, M[i], u);
        round &= sg_parse(P.n, M, N, u, &used) == i;
    }
    ok("USI round trip", round);

    /* mini shogi: 5 x 5, the promotion zone the last rank, sennichite lost by sente */
    spos mini;
    sg_start(&s, SG_MINI);
    ok("mini: the SFEN of the start", sg_from_sfen(&mini, MINI) && !memcmp(&s, &mini, sizeof s) && s.n == 5);
    ok("mini: K G S B R and a pawn before the king, each side",
       s.sq[SG_SQ(0, 4)] == SG_PC(0, SG_KING) && s.sq[SG_SQ(4, 4)] == SG_PC(0, SG_ROOK) &&
       s.sq[SG_SQ(0, 3)] == SG_PC(0, SG_PAWN) && s.sq[SG_SQ(4, 0)] == SG_PC(1, SG_KING) &&
       s.sq[SG_SQ(0, 0)] == SG_PC(1, SG_ROOK) && s.sq[SG_SQ(4, 1)] == SG_PC(1, SG_PAWN));
    long m1 = perft(&s, 1), m2 = perft(&s, 2), m3 = perft(&s, 3);
    ok("mini perft 1 = 14", m1 == 14);
    ok("mini perft 2 = 181", m2 == 181);
    ok("mini perft 3 = 2512", m3 == 2512);
    ok("a bad board size", !sg_from_sfen(&mini, "rbsg/4/4/KGSB b - 1") && !sg_from_sfen(&mini, "5/5/5/5 b - 1"));
    pos("k4/5/2R2/5/4K b - 1");
    ok("mini rook: 8 squares, 1 in the zone", from("3c") == 9 && has("3c3a+") && !has("3c3f"));
    pos("k4/2P2/5/5/4K b - 1");
    ok("mini pawn to the last rank: must promote", from("3b") == 1 && has("3b3a+"));
    pos("k4/5/2S2/5/4K b - 1");
    ok("mini silver: the second rank is not the zone", from("3c") == 5 && has("3c3b") && !has("3c3b+"));
    pos("k4/2S2/5/5/4K b - 1");
    ok("mini silver into the last rank: may promote", from("3b") == 8 && has("3b2a") && has("3b2a+"));
    pos("k4/5/5/5/4K b P 1");
    ok("mini pawn drops: not on the last rank", drops(SG_PAWN) == 19 && !has("P*3a") && has("P*3b"));
    pos("k4/5/2P2/5/4K b P 1");
    ok("mini nifu", drops(SG_PAWN) == 15 && !has("P*3d"));
    pos("k4/2G2/1S3/5/4K b P 1");
    ok("mini uchifuzume", !has("P*5b"));
    pos("k4/2G2/5/5/4K b P 1");
    ok("mini pawn drop check, not mate", has("P*5b"));
    ok("mini checkmate", game("k4/5/G4/5/4K b G 1", "G*5b") == SG_WON0);
    ok("mini: three times, play on", game("k4/5/5/5/4K b - 1", "1e1d 5a5b 1d1e 5b5a 1e1d 5a5b 1d1e 5b5a") == SG_PLAY);
    ok("mini: four times, sente loses",
       game("k4/5/5/5/4K b - 1", "1e1d 5a5b 1d1e 5b5a 1e1d 5a5b 1d1e 5b5a 1e1d 5a5b 1d1e 5b5a") == SG_WON1);
    ok("the same on 9 x 9: a draw",
       game("k8/9/9/9/9/9/9/9/8K b - 1", "1i1h 9a9b 1h1i 9b9a 1i1h 9a9b 1h1i 9b9a 1i1h 9a9b 1h1i 9b9a") == SG_DRAW);
    ok("mini perpetual check: the checking side loses",
       game("4k/5/2R2/5/K4 b - 1", "3c1c 1a2a 1c2c 2a1a 2c1c 1a2a 1c2c 2a1a 2c1c 1a2a 1c2c 2a1a 2c1c") == SG_WON1);
    sg_start(&P, SG_MINI);
    N = sg_legal(&P, M);
    round = 1;
    for (int i = 0; i < N; i++) {
        char u[8];
        int used;
        sg_usi(P.n, M[i], u);
        round &= sg_parse(P.n, M, N, u, &used) == i && u[0] >= '1' && u[0] <= '5' && u[1] >= 'a' && u[1] <= 'e';
    }
    ok("mini USI round trip", round && N == 14 && has("5d5c"));

    printf("%s\n", fails ? "SHOGI TESTS FAILED" : "shogi: all ok");
    return fails != 0;
}
