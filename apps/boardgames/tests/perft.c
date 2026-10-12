/*
 * The rules against the standard perft counts (the number of move sequences of a given length): the start position
 * and the usual tricky ones (castling through check, en passant pins, promotions). Then the draw rules, then
 * Chess960: its perft suite, its 960 start positions, its castling. Then crazyhouse, three-check and king of the hill,
 * and losing chess, atomic, racing kings and horde: their published perft counts, their rules played out, and whole
 * random games.
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

static void check_rules(const char *name, const char *fen, int rules, int depth, long want)
{
    cpos p;
    chess_from_fen(&p, fen);
    p.rules = (uint8_t)rules;
    long got = perft(&p, depth);
    printf("%s %-10s depth %d: %ld%s\n", got == want ? "ok  " : "FAIL", name, depth, got,
           got == want ? "" : " (wrong)");
    fails += got != want;
}

static void check(const char *name, const char *fen, int depth, long want)
{
    cpos p;
    chess_from_fen(&p, fen);
    long got = perft(&p, depth);
    printf("%s %-10s depth %d: %ld%s\n", got == want ? "ok  " : "FAIL", name, depth, got,
           got == want ? "" : " (wrong)");
    fails += got != want;
}

/* "e2e4", "e7e8q", or a drop "N@f3"; a move with from 0 and to 0 if none */
static cmove find(const cpos *p, const char *uci)
{
    static const char types[] = "PNBRQK";
    cmove m[MAX_MOVES], none = {0, 0, 0, 0, 0};
    int n = chess_legal(p, m), drop = 0;
    if (uci[1] == '@') {
        const char *t = strchr(types, uci[0]);
        drop = t ? (int)(t - types) + 1 : -1;
        uci += 2;
    }
    int from = (uci[0] - 'a') + (uci[1] - '1') * 8, to = (uci[2] - 'a') + (uci[3] - '1') * 8;
    for (int i = 0; i < n; i++) {
        if (drop && m[i].drop == drop && m[i].to == uci[0] - 'a' + (uci[1] - '1') * 8) return m[i];
        if (!drop && !m[i].drop && m[i].from == from && m[i].to == to && (!m[i].promo || m[i].promo == QUEEN))
            return m[i];
    }
    return none;
}

static int play_all(cgame *g, const char *moves)
{
    for (const char *s = moves; *s; ) {
        cmove m = find(game_pos(g), s);
        if (!m.from && !m.to) { printf("illegal: %s\n", s); return 0; }
        game_play(g, m);
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    return 1;
}

static void status_rules(const char *name, int rules, const char *moves, cstatus want)
{
    static cgame g;
    game_new_rules(&g, rules);
    play_all(&g, moves);
    cstatus got = game_status(&g);
    printf("%s %-22s status %d\n", got == want ? "ok  " : "FAIL", name, got);
    fails += got != want;
}

static void status(const char *name, const char *moves, cstatus want) { status_rules(name, CR_CHESS, moves, want); }

static void ok(const char *name, int good)
{
    printf("%s %s\n", good ? "ok  " : "FAIL", name);
    fails += !good;
}

/* whole random games by each rules (a seed each): every position keeps its pieces (crazyhouse: on the board and in
 * the hands, 32 always), the checks given never pass three, the game ends by the rules' own way; losing chess: a
 * capture whenever one can; atomic: nothing left on a capture's square; racing kings: never a check, no pawn; the
 * horde: never a white king */
static void random_games(int rules, int games)
{
    static cgame g;
    unsigned seed = 12345u + (unsigned)rules;
    int bad = 0, ends[ST_HORDE + 1] = {0};
    for (int k = 0; k < games; k++) {
        game_new_rules(&g, rules);
        cstatus st;
        while ((st = game_status(&g)) == ST_PLAY && g.ply < MAX_PLY) {
            cmove m[MAX_MOVES];
            int n = chess_legal(game_pos(&g), m);
            seed = seed * 1103515245u + 12345u;
            int caps = 0;
            for (int i = 0; i < n; i++) caps += (m[i].flags & MV_CAPTURE) != 0;
            if (rules == CR_ANTI && caps && caps != n) bad++;
            cmove played = m[(seed >> 8) % (unsigned)n];
            game_play(&g, played);
            if (rules == CR_ATOMIC && (played.flags & MV_CAPTURE) && game_pos(&g)->sq[played.to]) bad++;
            if (rules == CR_RACING && (chess_in_check(game_pos(&g), WHITE) || chess_in_check(game_pos(&g), BLACK)))
                bad++;
            for (int s = 0; s < 64; s++) {
                int q = game_pos(&g)->sq[s];
                if ((rules == CR_RACING && TYPE(q) == PAWN) || (rules == CR_HORDE && q == PC(WHITE, KING))) bad++;
            }
            const cpos *p = game_pos(&g);
            int pieces = 0;
            for (int s = 0; s < 64; s++) pieces += p->sq[s] != 0;
            for (int c = 0; c < 2; c++)
                for (int t = PAWN; t <= KING; t++) pieces += p->hand[c][t];
            if (rules == CR_CRAZYHOUSE && pieces != 32) bad++;
            if (rules != CR_CRAZYHOUSE && (p->hand[0][PAWN] || p->hand[1][QUEEN] || p->promoted)) bad++;
            if (p->checks[0] > 3 || p->checks[1] > 3 || (rules != CR_THREECHECK && (p->checks[0] || p->checks[1])))
                bad++;
            for (int s = 0; s < 64; s++)          /* a promoted mark on an empty square, or a king, or a pawn */
                if ((p->promoted >> s & 1) && (!p->sq[s] || TYPE(p->sq[s]) == PAWN || TYPE(p->sq[s]) == KING)) bad++;
        }
        ends[st]++;
        if ((st == ST_CHECKS && rules != CR_THREECHECK) || (st == ST_HILL && rules != CR_HILL)) bad++;
        if (st == ST_INSUFFICIENT && (rules == CR_CRAZYHOUSE || rules == CR_HILL || rules == CR_RACING ||
                                      rules == CR_HORDE)) bad++;
        if ((st == ST_ANTI && rules != CR_ANTI) || (st == ST_EXPLODED && rules != CR_ATOMIC) ||
            ((st == ST_RACE || st == ST_RACE_DRAW) && rules != CR_RACING) || (st == ST_HORDE && rules != CR_HORDE) ||
            (rules == CR_ANTI && (st == ST_CHECKMATE || st == ST_STALEMATE)))
            bad++;
    }
    printf("%s %d random games, rules %d: %d mates, %d by the rules' own end, %d draws, %d bad\n", bad ? "FAIL" : "ok  ",
           games, rules, ends[ST_CHECKMATE], ends[ST_CHECKS] + ends[ST_HILL] +
           ends[ST_ANTI] + ends[ST_EXPLODED] + ends[ST_RACE] + ends[ST_HORDE],
           ends[ST_STALEMATE] + ends[ST_INSUFFICIENT] + ends[ST_REPETITION] + ends[ST_FIFTY] + ends[ST_RACE_DRAW], bad);
    fails += bad != 0;
}

/* crazyhouse, three-check, king of the hill: the published perft suites, then the rules played out */
static void variants(void)
{
    const char *start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    /* crazyhouse (python-chess's crazyhouse.perft, Fairy-Stockfish's tests) */
    check_rules("zh start", start, CR_CRAZYHOUSE, 4, 197281);
    const char *z1 = "2k5/8/8/8/8/8/8/4K3[QRBNPqrbnp] w - -";
    check_rules("zh drops", z1, CR_CRAZYHOUSE, 1, 301);
    check_rules("zh drops", z1, CR_CRAZYHOUSE, 2, 75353);
    const char *z2 = "2k5/8/8/8/8/8/8/4K3[Qn] w - -";
    check_rules("zh Qn", z2, CR_CRAZYHOUSE, 1, 67);
    check_rules("zh Qn", z2, CR_CRAZYHOUSE, 2, 3083);
    check_rules("zh Qn", z2, CR_CRAZYHOUSE, 3, 88634);
    check_rules("zh Qn", z2, CR_CRAZYHOUSE, 4, 932554);
    const char *z3 = "r1bqk2r/pppp1ppp/2n1p3/4P3/1b1Pn3/2NB1N2/PPP2PPP/R1BQK2R[] b KQkq -";
    check_rules("zh middle", z3, CR_CRAZYHOUSE, 1, 42);
    check_rules("zh middle", z3, CR_CRAZYHOUSE, 2, 1347);
    check_rules("zh middle", z3, CR_CRAZYHOUSE, 3, 58057);
    check_rules("zh middle", z3, CR_CRAZYHOUSE, 4, 2083382);
    const char *z4 = "4k3/1Q~6/8/8/4b3/8/Kpp5/8/ b - -";
    check_rules("zh promoted", z4, CR_CRAZYHOUSE, 1, 20);
    check_rules("zh promoted", z4, CR_CRAZYHOUSE, 2, 360);
    check_rules("zh promoted", z4, CR_CRAZYHOUSE, 3, 5445);
    check_rules("zh promoted", z4, CR_CRAZYHOUSE, 4, 132758);
    const char *z5 = "2rn1b1r/1pp2n1p/B1PBk1pP/5p1P/P5p1/2P2RP1/RP1QNP2/1NBK4[QPp] w - -";
    check_rules("zh midgame", z5, CR_CRAZYHOUSE, 1, 99);
    check_rules("zh midgame", z5, CR_CRAZYHOUSE, 2, 3932);
    check_rules("zh midgame", z5, CR_CRAZYHOUSE, 3, 314782);
    /* three-check (python-chess's 3check.perft: "1+1" one check still to give each; Fairy-Stockfish's) */
    const char *t1 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 1+1";
    check_rules("3check kiwi", t1, CR_THREECHECK, 1, 48);
    check_rules("3check kiwi", t1, CR_THREECHECK, 2, 2039);
    check_rules("3check kiwi", t1, CR_THREECHECK, 3, 97848);
    const char *t2 = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1+1";
    check_rules("3check castle", t2, CR_THREECHECK, 1, 26);
    check_rules("3check castle", t2, CR_THREECHECK, 2, 562);
    check_rules("3check castle", t2, CR_THREECHECK, 3, 13410);
    check_rules("3check sf", "7r/1p4p1/pk3p2/RN6/8/P5Pp/3p1P1P/4R1K1 w - - 1+3 1 39", CR_THREECHECK, 3, 12407);
    check_rules("3check sf +", "7r/1p4p1/pk3p2/RN6/8/P5Pp/3p1P1P/4R1K1 w - - 1 39 +2+0", CR_THREECHECK, 3, 12407);
    check_rules("3check start", start, CR_THREECHECK, 4, 197281);
    /* king of the hill (Fairy-Stockfish's) */
    check_rules("hill sf", "rnb2b1r/ppp2ppp/3k4/8/1PKp1pn1/3Pq3/PBP1P2P/RN1Q1B1R w - - 4 12", CR_HILL, 3, 19003);

    /* the rules played out */
    static cgame g;
    game_new_rules(&g, CR_CRAZYHOUSE);
    play_all(&g, "e2e4 d7d5 e4d5 g8f6");
    const cpos *p = game_pos(&g);
    ok("zh: the pawn taken in white's hand", p->hand[WHITE][PAWN] == 1 && !p->hand[BLACK][PAWN]);
    ok("zh: a pawn dropped with check", play_all(&g, "P@d7") && chess_in_check(game_pos(&g), BLACK) &&
       game_pos(&g)->sq[SQ(3, 6)] == PC(WHITE, PAWN) && !game_pos(&g)->hand[WHITE][PAWN]);
    cpos z;
    chess_from_fen(&z, "4k3/1Q~6/8/8/4b3/8/K7/8 b - - 0 1");
    z.rules = CR_CRAZYHOUSE;
    cmove m = find(&z, "e4b7");
    chess_make(&z, m);
    ok("zh: a promoted queen taken goes to the hand as a pawn", z.hand[BLACK][PAWN] == 1 && !z.hand[BLACK][QUEEN] &&
       !z.promoted);
    chess_from_fen(&z, "4k3/1P6/8/8/8/8/8/K7 w - - 0 1");
    z.rules = CR_CRAZYHOUSE;
    m = find(&z, "b7b8q");
    chess_make(&z, m);
    ok("zh: a pawn promoted is marked", z.promoted == 1ull << SQ(1, 7) && z.sq[SQ(1, 7)] == PC(WHITE, QUEEN));
    chess_from_fen(&z, "4k3/8/8/8/8/8/8/K7[P] w - - 0 1");
    z.rules = CR_CRAZYHOUSE;
    cmove all[MAX_MOVES];
    int n = chess_legal(&z, all), first = 0;
    for (int i = 0; i < n; i++) first += all[i].drop && (RANK_OF(all[i].to) == 0 || RANK_OF(all[i].to) == 7);
    ok("zh: no pawn dropped on the first or last rank", n == 3 + 48 - 0 && !first);
    status_rules("zh: checkmate as usual", CR_CRAZYHOUSE, "f2f3 e7e5 g2g4 d8h4", ST_CHECKMATE);
    g.ply = 0;
    chess_from_fen(&g.pos[0], "6k1/5ppp/8/8/8/8/8/K7[R] w - - 0 1");
    g.pos[0].rules = CR_CRAZYHOUSE;
    ok("zh: a rook dropped mates", play_all(&g, "R@e8") && game_status(&g) == ST_CHECKMATE);
    status_rules("zh: no insufficient material", CR_CRAZYHOUSE, "e2e4", ST_PLAY);

    status_rules("3check: the third check", CR_THREECHECK, "e2e4 e7e5 f1c4 a7a6 c4f7 e8f7 d1h5 f7e7 h5e5",
                 ST_CHECKS);
    game_new_rules(&g, CR_THREECHECK);
    play_all(&g, "e2e4 e7e5 f1c4 a7a6 c4f7 e8f7 d1h5 f7e7");
    ok("3check: two checks counted", game_pos(&g)->checks[WHITE] == 2 && !game_pos(&g)->checks[BLACK] &&
       game_status(&g) == ST_PLAY);
    status_rules("hill: a king on e4", CR_HILL, "e2e3 e7e6 e1e2 e8e7 e2d3 e7d6 d3e4", ST_HILL);
    status_rules("hill: not yet", CR_HILL, "e2e3 e7e6 e1e2 e8e7 e2d3 e7d6", ST_PLAY);
    game_new_rules(&g, CR_HILL);
    play_all(&g, "e2e3 e7e6 e1e2 e8e7 e2d3 e7d6 d3e4");
    ok("hill: the game over, no move", !chess_legal(game_pos(&g), all));

    random_games(CR_CHESS, 40);
    random_games(CR_CRAZYHOUSE, 40);
    random_games(CR_THREECHECK, 40);
    random_games(CR_HILL, 40);
}

/* a position from a FEN played by those rules, then moves; its status (-1: a move not legal) */
static cstatus after(int rules, const char *fen, const char *moves)
{
    static cgame g;
    g.ply = 0;
    chess_from_fen(&g.pos[0], fen);
    g.pos[0].rules = (uint8_t)rules;
    if (!play_all(&g, moves)) return (cstatus)-1;
    return game_status(&g);
}

static int legal_count(int rules, const char *fen)
{
    cpos p;
    cmove m[MAX_MOVES];
    chess_from_fen(&p, fen);
    p.rules = (uint8_t)rules;
    return chess_legal(&p, m);
}

/* losing chess, atomic, racing kings, horde: python-chess's perft suites (examples/perft: giveaway.perft, whose
 * antichess positions have no castling, atomic.perft, racingkings.perft, horde.perft; the deepest counts, past a few
 * million moves, left out), then the rules played out */
static void variants2(void)
{
    const char *start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1";
    check_rules("anti start", start, CR_ANTI, 1, 20);
    check_rules("anti start", start, CR_ANTI, 2, 400);
    check_rules("anti start", start, CR_ANTI, 3, 8067);
    check_rules("anti start", start, CR_ANTI, 4, 153299);
    const char *ab = "8/1p6/8/8/8/8/P7/8 w - -";
    static const long abn[6] = {2, 4, 4, 3, 1, 0};
    for (int d = 1; d <= 6; d++) check_rules("anti a-b", ab, CR_ANTI, d, abn[d - 1]);
    const char *ac = "8/2p5/8/8/8/8/P7/8 w - -";
    static const long acn[13] = {2, 4, 4, 4, 4, 4, 4, 4, 12, 36, 312, 2557, 30873};
    for (int d = 1; d <= 13; d++) check_rules("anti a-c", ac, CR_ANTI, d, acn[d - 1]);

    const char *a0 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
    static const long a0n[4] = {20, 400, 8902, 197326};
    for (int d = 1; d <= 4; d++) check_rules("atomic start", a0, CR_ATOMIC, d, a0n[d - 1]);
    const char *a1 = "rn2kb1r/1pp1p2p/p2q1pp1/3P4/2P3b1/4PN2/PP3PPP/R2QKB1R b KQkq -";
    static const long a1n[4] = {40, 1238, 45237, 1434825};
    for (int d = 1; d <= 4; d++) check_rules("atomic pf1", a1, CR_ATOMIC, d, a1n[d - 1]);
    const char *a2 = "rn1qkb1r/p5pp/2p5/3p4/N3P3/5P2/PPP4P/R1BQK3 w Qkq -";
    static const long a2n[4] = {28, 833, 23353, 714499};
    for (int d = 1; d <= 4; d++) check_rules("atomic pf2", a2, CR_ATOMIC, d, a2n[d - 1]);
    const char *a3 = "8/8/8/8/8/8/2k5/rR4KR w KQ -";
    static const long a3n[5] = {18, 180, 4364, 61401, 1603055};
    for (int d = 1; d <= 5; d++) check_rules("atomic castle1", a3, CR_ATOMIC, d, a3n[d - 1]);
    const char *a4 = "r3k1rR/5K2/8/8/8/8/8/8 b kq -";
    static const long a4n[5] = {25, 282, 6753, 98729, 2587730};
    for (int d = 1; d <= 5; d++) check_rules("atomic castle2", a4, CR_ATOMIC, d, a4n[d - 1]);
    const char *a5 = "Rr2k1rR/3K4/3p4/8/8/8/7P/8 w kq -";
    static const long a5n[4] = {21, 465, 10631, 241478};
    for (int d = 1; d <= 4; d++) check_rules("atomic castle3", a5, CR_ATOMIC, d, a5n[d - 1]);

    const char *r0 = "8/8/8/8/8/8/krbnNBRK/qrbnNBRQ w - -";
    static const long r0n[4] = {21, 421, 11264, 296242};
    for (int d = 1; d <= 4; d++) check_rules("racing start", r0, CR_RACING, d, r0n[d - 1]);
    const char *r1 = "4brn1/2K2k2/8/8/8/8/8/8 w - -";
    static const long r1n[6] = {6, 33, 178, 3151, 12981, 265932};
    for (int d = 1; d <= 6; d++) check_rules("racing goal", r1, CR_RACING, d, r1n[d - 1]);
    const char *r2 = "8/8/1rk4K/8/8/8/2bnNBR1/qrbnNBRQ b - -";
    static const long r2n[4] = {36, 697, 26592, 661533};
    for (int d = 1; d <= 4; d++) check_rules("racing disc", r2, CR_RACING, d, r2n[d - 1]);

    const char *h0 = "rnbqkbnr/pppppppp/8/1PP2PP1/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPP w kq -";
    static const long h0n[4] = {8, 128, 1274, 23310};
    for (int d = 1; d <= 4; d++) check_rules("horde start", h0, CR_HORDE, d, h0n[d - 1]);
    const char *h1 = "4k3/pp4q1/3P2p1/8/P3PP2/PPP2r2/PPP5/PPPP4 b - -";
    static const long h1n[4] = {30, 241, 6633, 56539};
    for (int d = 1; d <= 4; d++) check_rules("horde flank", h1, CR_HORDE, d, h1n[d - 1]);
    const char *h2 = "k7/5p2/4p2P/3p2P1/2p2P2/1p2P2P/p2P2P1/2P2P2 w - -";
    static const long h2n[4] = {13, 172, 2205, 33781};
    for (int d = 1; d <= 4; d++) check_rules("horde ep", h2, CR_HORDE, d, h2n[d - 1]);

    /* the rules played out */
    static cgame g;
    cmove all[MAX_MOVES];
    cpos z;
    game_new_rules(&g, CR_ANTI);
    ok("anti: no castling at the start", !game_pos(&g)->castle);
    play_all(&g, "e2e4 d7d5");
    ok("anti: the capture is compulsory", chess_legal(game_pos(&g), all) == 1 && (all[0].flags & MV_CAPTURE));
    ok("anti: all pieces lost wins", after(CR_ANTI, "8/8/8/8/8/8/1p6/R7 b - - 0 1", "b2a1q") == ST_ANTI);
    ok("anti: stalemated wins", after(CR_ANTI, "8/8/8/8/8/1p6/1P6/8 w - - 0 1", "") == ST_ANTI);
    ok("anti: the king can be taken", after(CR_ANTI, "8/8/8/8/8/8/1k6/K7 w - - 0 1", "a1b2") == ST_ANTI);
    chess_from_fen(&z, "8/P7/8/8/8/8/8/7k w - - 0 1");
    z.rules = CR_ANTI;
    int n = chess_legal(&z, all), kings = 0;
    for (int i = 0; i < n; i++) kings += all[i].promo == KING;
    ok("anti: a pawn promotes to a king too", n == 5 && kings == 1);
    ok("anti: bishops that never meet draw", after(CR_ANTI, "8/8/8/8/8/1b6/8/B7 w - - 0 1", "") == ST_INSUFFICIENT &&
       after(CR_ANTI, "8/8/8/8/8/2b5/8/B7 w - - 0 1", "") == ST_PLAY);

    game_new_rules(&g, CR_ATOMIC);
    play_all(&g, "e2e4 d7d5 e4d5");
    ok("atomic: both pawns gone", !game_pos(&g)->sq[SQ(3, 4)] && !game_pos(&g)->sq[SQ(4, 3)]);
    ok("atomic: the king never captures", legal_count(CR_ATOMIC, "8/8/8/8/8/8/1p6/K6k w - - 0 1") == 2);
    ok("atomic: blowing up the king wins", after(CR_ATOMIC, "3qk3/8/8/8/8/8/8/3QK3 w - - 0 1", "d1d8") == ST_EXPLODED);
    g.ply = 0;
    chess_from_fen(&g.pos[0], "4k3/8/8/8/8/8/3PPq2/4K3 b - - 0 1");
    g.pos[0].rules = CR_ATOMIC;
    play_all(&g, "f2e2");
    ok("atomic: the pawns round it stay", game_status(&g) == ST_EXPLODED &&
       game_pos(&g)->sq[SQ(3, 1)] == PC(WHITE, PAWN) && !game_pos(&g)->sq[SQ(4, 1)]);
    chess_from_fen(&z, "8/8/8/8/8/3k4/3K3r/8 w - - 0 1");
    z.rules = CR_ATOMIC;
    ok("atomic: kings side by side give no check", !chess_in_check(&z, WHITE));
    game_new_rules(&g, CR_ATOMIC);
    play_all(&g, "g1f3 d7d5 f3e5 d8d6 e5f7");
    ok("atomic: an explosion takes the castling rights", (game_pos(&g)->castle & 12) == 0 &&
       game_status(&g) == ST_EXPLODED);

    game_new_rules(&g, CR_RACING);
    ok("racing: the start", chess_legal(game_pos(&g), all) == 21 && game_pos(&g)->sq[SQ(7, 1)] == PC(WHITE, KING));
    chess_from_fen(&z, "8/8/8/8/8/8/k7/6RK w - - 0 1");
    z.rules = CR_RACING;
    ok("racing: no check given", !find(&z, "g1g2").to && !find(&z, "g1a1").to && find(&z, "g1g3").to);
    ok("racing: black may follow", after(CR_RACING, "8/K6k/8/8/8/8/8/8 w - - 0 1", "a7a8") == ST_PLAY);
    ok("racing: both there, a draw", after(CR_RACING, "8/K6k/8/8/8/8/8/8 w - - 0 1", "a7a8 h7h8") == ST_RACE_DRAW);
    ok("racing: black did not follow", after(CR_RACING, "8/K6k/8/8/8/8/8/8 w - - 0 1", "a7a8 h7h6") == ST_RACE);
    ok("racing: black cannot follow", after(CR_RACING, "8/K7/8/8/8/8/7k/8 w - - 0 1", "a7a8") == ST_RACE);
    ok("racing: black first", after(CR_RACING, "8/k6K/8/8/8/8/8/8 b - - 0 1", "a7a8") == ST_RACE);

    game_new_rules(&g, CR_HORDE);
    ok("horde: 36 pawns, no white king", chess_king(game_pos(&g), WHITE) < 0 &&
       chess_legal(game_pos(&g), all) == 8 && game_pos(&g)->castle == 12);
    ok("horde: two squares from the first rank", legal_count(CR_HORDE, "4k3/8/8/8/8/8/8/P7 w - - 0 1") == 2);
    g.ply = 0;
    chess_from_fen(&g.pos[0], "4k3/8/8/8/8/p7/8/1P6 w - - 0 1");
    g.pos[0].rules = CR_HORDE;
    play_all(&g, "b1b3");
    ok("horde: no en passant after it", game_pos(&g)->ep < 0 && !find(game_pos(&g), "a3b2").to);
    ok("horde: black takes the last piece", after(CR_HORDE, "4k3/8/8/8/8/8/1q6/P7 b - - 0 1", "b2a1") == ST_HORDE);
    ok("horde: white mates", after(CR_HORDE, "k7/2P5/1P6/8/8/8/8/8 w - - 0 1", "c7c8") == ST_CHECKMATE);

    random_games(CR_ANTI, 40);
    random_games(CR_ATOMIC, 40);
    random_games(CR_RACING, 40);
    random_games(CR_HORDE, 20);
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
    variants();
    variants2();
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
