/*
 * The rules against the standard perft counts (the number of move sequences of a given length): the start position
 * and the usual tricky ones (castling through check, en passant pins, promotions). Then the draw rules, then
 * Chess960: its perft suite, its 960 start positions, its castling. Then crazyhouse, three-check and king of the hill:
 * their published perft counts, their rules played out, and whole random games.
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
 * the hands, 32 always), the checks given never pass three, the game ends by the rules' own way */
static void random_games(int rules, int games)
{
    static cgame g;
    unsigned seed = 12345u + (unsigned)rules;
    int bad = 0, ends[ST_HILL + 1] = {0};
    for (int k = 0; k < games; k++) {
        game_new_rules(&g, rules);
        cstatus st;
        while ((st = game_status(&g)) == ST_PLAY && g.ply < MAX_PLY) {
            cmove m[MAX_MOVES];
            int n = chess_legal(game_pos(&g), m);
            seed = seed * 1103515245u + 12345u;
            game_play(&g, m[(seed >> 8) % (unsigned)n]);
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
        if (st == ST_INSUFFICIENT && (rules == CR_CRAZYHOUSE || rules == CR_HILL)) bad++;
    }
    printf("%s %d random games, rules %d: %d mates, %d by the rules' own end, %d draws, %d bad\n", bad ? "FAIL" : "ok  ",
           games, rules, ends[ST_CHECKMATE], ends[ST_CHECKS] + ends[ST_HILL],
           ends[ST_STALEMATE] + ends[ST_INSUFFICIENT] + ends[ST_REPETITION] + ends[ST_FIFTY], bad);
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
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
