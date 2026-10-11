/*
 * Makruk: the rules' test (the start, each piece's moves, the Bia's promotion on the sixth rank, check, mate,
 * stalemate, the counting rules, the other draws, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/makruk.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mkgame G;
static mkmove L[MK_MAX_MOVES];

/* a position: "Kd1 Ra1 ke8 pd4": K Khun, M Met, S Khon, N Ma, R Rua, P Bia (white capitals, black small letters; Q
 * a promoted Met), who moves */
static void setup(const char *pieces, int side)
{
    static const char *KINDS = "?PNSRMK";
    mk_new(&G);
    mkpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    for (const char *t = pieces; *t; t++) {
        if (*t == ' ') continue;
        int black = *t >= 'a', c = black ? *t - 32 : *t, promoted = c == 'Q';
        const char *k = strchr(KINDS, promoted ? 'M' : c);
        int s = mk_square(t + 1);
        if (!k || s < 0) { printf("bad setup %s\n", t); failed++; return; }
        p->sq[s] = (uint8_t)(MK_PC(black, k - KINDS) | (promoted ? MK_PROMOTED : 0));
        t += 2;
    }
    p->side = (uint8_t)side;
}

static const mkpos *pos(void) { return mk_pos(&G); }
static int nlegal(void) { return mk_legal(pos(), L); }
static int at(const char *s) { return pos()->sq[mk_square(s)]; }

static int moves_from(const char *sq)           /* the legal moves of the piece on sq */
{
    int n = nlegal(), k = 0;
    for (int i = 0; i < n; i++) k += L[i].from == mk_square(sq);
    return k;
}

static int can(const char *from, const char *to)
{
    int n = nlegal();
    for (int i = 0; i < n; i++)
        if (L[i].from == mk_square(from) && L[i].to == mk_square(to)) return 1;
    return 0;
}

static long perft(const mkpos *p, int depth)
{
    mkmove m[MK_MAX_MOVES];
    int n = mk_legal(p, m);
    if (depth == 1) return n;
    long sum = 0;
    for (int i = 0; i < n; i++) {
        mkpos q = *p;
        mk_make(&q, m[i]);
        sum += perft(&q, depth - 1);
    }
    return sum;
}

/* the classical setup, the kings not facing each other; the known move counts from it */
static void test_start(void)
{
    mk_new(&G);
    CHECK(at("d1") == MK_PC(MK_WHITE, MK_KHUN) && at("e1") == MK_PC(MK_WHITE, MK_MET));
    CHECK(at("e8") == MK_PC(MK_BLACK, MK_KHUN) && at("d8") == MK_PC(MK_BLACK, MK_MET));
    CHECK(at("a1") == MK_PC(MK_WHITE, MK_RUA) && at("b1") == MK_PC(MK_WHITE, MK_MA) && at("c1") == MK_PC(0, MK_KHON));
    CHECK(at("f8") == MK_PC(MK_BLACK, MK_KHON) && at("g8") == MK_PC(MK_BLACK, MK_MA) && at("h8") == MK_PC(1, MK_RUA));
    int bia = 1;
    for (int f = 0; f < 8; f++)
        bia &= pos()->sq[MK_SQ(f, 2)] == MK_PC(MK_WHITE, MK_BIA) && pos()->sq[MK_SQ(f, 5)] == MK_PC(MK_BLACK, MK_BIA)
               && !pos()->sq[MK_SQ(f, 1)] && !pos()->sq[MK_SQ(f, 6)];
    CHECK(bia && pos()->side == MK_WHITE && pos()->count_kind == MK_COUNT_NONE);
    CHECK(nlegal() == 23);
    CHECK(perft(pos(), 2) == 529);
    CHECK(perft(pos(), 3) == 12012);
    CHECK(perft(pos(), 4) == 273026);
}

/* each piece's moves, alone in the middle of the board (the kings far away) */
static void test_pieces(void)
{
    setup("Ka1 kh8 Me4", MK_WHITE);
    CHECK(moves_from("e4") == 4 && can("e4", "d5") && can("e4", "f3") && !can("e4", "e5"));
    setup("Ka1 kh8 Qe4", MK_WHITE);             /* a promoted Met: the same */
    CHECK(moves_from("e4") == 4 && can("e4", "d3"));
    setup("Ka1 kh8 Se4", MK_WHITE);             /* the Khon: the diagonals and straight forwards */
    CHECK(moves_from("e4") == 5 && can("e4", "e5") && can("e4", "d3") && !can("e4", "e3") && !can("e4", "d4"));
    setup("Ka1 kh8 se5", MK_BLACK);             /* black's forwards: down the board */
    CHECK(moves_from("e5") == 5 && can("e5", "e4") && !can("e5", "e6") && can("e5", "f6"));
    setup("Ka1 kh8 Ne4", MK_WHITE);
    CHECK(moves_from("e4") == 8 && can("e4", "f6") && can("e4", "c3"));
    setup("Ka1 kh8 Re4", MK_WHITE);
    CHECK(moves_from("e4") == 14 && can("e4", "e8") && can("e4", "a4"));
    setup("Ka1 kh8 Re4 Pe6 ne2", MK_WHITE);     /* blocked by its own Bia, takes the Ma, not beyond */
    CHECK(can("e4", "e5") && !can("e4", "e6") && can("e4", "e2") && !can("e4", "e1"));
    setup("Ke4 kh8", MK_WHITE);                 /* the Khun: one square any way */
    CHECK(moves_from("e4") == 8);
    setup("Kd1 Ra1 Rh1 ke8", MK_WHITE);         /* no castling */
    CHECK(moves_from("d1") == 5 && !can("d1", "b1") && !can("d1", "f1"));
    setup("Ka1 kh8 Pe3 pd4 pf4", MK_WHITE);     /* the Bia: one square forwards, no double step, takes diagonally */
    CHECK(moves_from("e3") == 3 && can("e3", "e4") && can("e3", "d4") && !can("e3", "e5"));
    setup("Ka1 kh8 Pe3 pe4", MK_WHITE);         /* never takes straight ahead */
    CHECK(moves_from("e3") == 0);
    setup("Ka1 kh8 pe6 Pd5", MK_BLACK);         /* black's Bia goes down */
    CHECK(can("e6", "e5") && can("e6", "d5") && !can("e6", "e7"));
}

/* a Bia becomes a Met on the sixth rank (black's: the third), straight or by a capture */
static void test_promotion(void)
{
    setup("Ka1 kh8 Pe5 pd6", MK_WHITE);
    CHECK(mk_script(&G, "e5e6") && at("e6") == (MK_PC(MK_WHITE, MK_MET) | MK_PROMOTED));
    mk_undo(&G);
    CHECK(mk_script(&G, "e5d6") && at("d6") == (MK_PC(MK_WHITE, MK_MET) | MK_PROMOTED) && G.move[0].taken);
    CHECK(G.move[0].promo && G.move[0].taken == MK_PC(MK_BLACK, MK_BIA));
    mk_undo(&G);
    CHECK(MK_KIND(at("e5")) == MK_BIA);
    setup("Ka1 kh8 Pe4", MK_WHITE);             /* the fifth rank: still a Bia */
    CHECK(mk_script(&G, "e4e5") && at("e5") == MK_PC(MK_WHITE, MK_BIA));
    setup("Ka1 kh8 pd4", MK_BLACK);
    CHECK(mk_script(&G, "d4d3") && at("d3") == (MK_PC(MK_BLACK, MK_MET) | MK_PROMOTED));
    CHECK(mk_script(&G, "a1b1") && mk_script(&G, "d3c2") && mk_script(&G, "b1a1") && mk_script(&G, "c2d1"));
    CHECK(MK_KIND(at("d1")) == MK_MET);         /* the new Met moves one square diagonally */
    setup("Ka1 kh8 Qe6", MK_WHITE);             /* the promoted Met goes on as a Met, never straight on */
    CHECK(moves_from("e6") == 4 && !can("e6", "e7"));
}

/* check: no move may leave the Khun attacked; checkmate wins, stalemate is a draw */
static void test_check(void)
{
    setup("Ke1 Se2 ke8 re7", MK_WHITE);         /* the Khon is pinned along the e file */
    CHECK(moves_from("e2") == 1 && can("e2", "e3") && !mk_in_check(pos(), MK_WHITE));   /* along the pin only */
    setup("Ke1 Pe3 ke8 nd3", MK_WHITE);         /* in check from the Ma: the Bia cannot help */
    CHECK(mk_in_check(pos(), MK_WHITE) && moves_from("e3") == 0);
    CHECK(nlegal() == 4 && can("e1", "d2") && can("e1", "e2") && !can("e1", "f2"));   /* f2: the Ma covers it */
    setup("Kf7 Ra1 kh8", MK_WHITE);
    CHECK(mk_status(&G) == MS_PLAY && mk_script(&G, "a1h1"));
    CHECK(mk_in_check(pos(), MK_BLACK) && nlegal() == 0 && mk_status(&G) == MS_CHECKMATE);
    setup("Ka6 Rb1 ka8", MK_BLACK);             /* black cannot move and is not in check */
    CHECK(nlegal() == 0 && !mk_in_check(pos(), MK_BLACK) && mk_status(&G) == MS_STALEMATE);
    setup("Kd6 Mf6 kd8", MK_WHITE);             /* the Met gives check diagonally... */
    CHECK(mk_script(&G, "f6e7") && mk_in_check(pos(), MK_BLACK));
    setup("Kb6 Mc6 kd8", MK_WHITE);             /* ...not straight ahead */
    CHECK(mk_script(&G, "c6d7") && !mk_in_check(pos(), MK_BLACK));
    setup("Kd6 Se6 kd8", MK_WHITE);             /* the Khon: diagonally, or straight ahead */
    CHECK(mk_script(&G, "e6e7") && mk_in_check(pos(), MK_BLACK));
    setup("Kd6 Se6 kd8", MK_WHITE);
    CHECK(mk_script(&G, "e6f7") && !mk_in_check(pos(), MK_BLACK));
    setup("Ka1 Sd7 kd6", MK_BLACK);             /* ...never straight backwards */
    CHECK(!mk_in_check(pos(), MK_BLACK));
    setup("Ka1 Sd5 kd6", MK_BLACK);
    CHECK(mk_in_check(pos(), MK_BLACK));
    setup("Ka1 Pd4 ke5", MK_BLACK);             /* a Bia attacks diagonally forwards: white's up, black's down */
    CHECK(mk_in_check(pos(), MK_BLACK));
    setup("Kd4 kh8 pe5", MK_WHITE);
    CHECK(mk_in_check(pos(), MK_WHITE));
    setup("Kd4 kh8 pd5", MK_WHITE);
    CHECK(!mk_in_check(pos(), MK_WHITE));
}

/* the counting: the board's honour, the pieces' honour, their limits */
static void test_counting(void)
{
    setup("Kd1 Ra1 Me4 ke8 ng8 pa6", MK_WHITE);   /* a Bia left: no count */
    CHECK(mk_script(&G, "a1a2") && pos()->count_kind == MK_COUNT_NONE && mk_moves_left(pos()) < 0);
    CHECK(mk_script(&G, "g8f6") && mk_script(&G, "a2a6"));   /* the last Bia taken: the board's count starts */
    CHECK(pos()->count_kind == MK_COUNT_BOARD && pos()->counter == MK_BLACK && pos()->count == 0);
    CHECK(pos()->limit == 64 && mk_moves_left(pos()) == 64);
    CHECK(mk_script(&G, "f6g8") && pos()->count == 1 && mk_moves_left(pos()) == 64);
    CHECK(mk_script(&G, "a6a5") && pos()->count == 1 && mk_moves_left(pos()) == 63);
    G.pos[G.ply].count = 62;                     /* near the end */
    CHECK(mk_script(&G, "g8f6 a5a6") && mk_status(&G) == MS_PLAY && pos()->count == 63);
    CHECK(mk_script(&G, "f6g8") && pos()->count == 64 && mk_moves_left(pos()) == 1 && mk_status(&G) == MS_PLAY);
    CHECK(mk_script(&G, "a6a5") && mk_status(&G) == MS_COUNT);   /* no mate with the last move: a draw */
    /* a mate on the last move still wins */
    setup("Kf7 Rb1 kh8 na3", MK_BLACK);
    G.pos[0].count_kind = MK_COUNT_BOARD, G.pos[0].counter = MK_BLACK, G.pos[0].count = 63, G.pos[0].limit = 64;
    CHECK(mk_script(&G, "a3b5") && pos()->count == 64 && mk_moves_left(pos()) == 1);
    CHECK(mk_script(&G, "b1h1") && mk_status(&G) == MS_CHECKMATE);
    mk_undo(&G);
    CHECK(mk_script(&G, "f7f6") && mk_status(&G) == MS_COUNT);
    /* the pieces' honour: the last piece of black taken */
    setup("Kd1 Ra1 Nb1 ke8 ma8", MK_WHITE);
    CHECK(mk_script(&G, "a1a8") && pos()->count_kind == MK_COUNT_PIECES && pos()->counter == MK_BLACK);
    CHECK(pos()->count == 4 && pos()->limit == 16 && mk_moves_left(pos()) == 12);
    CHECK(mk_script(&G, "e8e7") && pos()->count == 5 && mk_moves_left(pos()) == 12);
    CHECK(mk_script(&G, "a8a6") && mk_moves_left(pos()) == 11);
    /* the board's count running, then a bare Khun: the pieces' honour takes over */
    setup("Kd1 Ra1 Se4 ke8 ma8", MK_BLACK);
    G.pos[0].count_kind = MK_COUNT_BOARD, G.pos[0].counter = MK_BLACK, G.pos[0].count = 30, G.pos[0].limit = 64;
    CHECK(mk_script(&G, "e8f7 a1a8") && pos()->count_kind == MK_COUNT_PIECES && pos()->count == 4);
    CHECK(pos()->limit == 16);
    /* the limits */
    static const struct { const char *white; int limit; } LIM[] = {
        {"Kd1 Ra1 Rh1", 8}, {"Kd1 Ra1", 16}, {"Kd1 Ra1 Sc1 Sf1", 16}, {"Kd1 Sc1 Sf1", 22}, {"Kd1 Sc1 Sf1 Nb1", 22},
        {"Kd1 Nb1 Ng1", 32}, {"Kd1 Nb1 Ng1 Sc1", 32}, {"Kd1 Sc1", 44}, {"Kd1 Sc1 Nb1", 44}, {"Kd1 Nb1", 64},
        {"Kd1 Me1", 64}, {"Kd1 Qe3 Qd3", 64},
    };
    for (unsigned i = 0; i < sizeof LIM / sizeof LIM[0]; i++) {
        char s[64];
        snprintf(s, sizeof s, "%s kh8 me2", LIM[i].white);   /* the white Khun takes black's last piece */
        setup(s, MK_WHITE);
        CHECK(mk_script(&G, "d1e2") && pos()->count_kind == MK_COUNT_PIECES && pos()->limit == LIM[i].limit);
    }
    /* the count starts past its limit: a draw at once (unless mate) */
    setup("Kd1 Ra1 Rh1 Sc1 Sf1 Nb1 Ng1 Me1 kh8 ma8", MK_WHITE);
    CHECK(mk_script(&G, "a1a8") && pos()->count == 9 && pos()->limit == 8 && mk_status(&G) == MS_COUNT);
    /* while a Bia (even the stronger side's) is on the board, no count */
    setup("Kd1 Ra1 Pc3 ke8 ma8", MK_WHITE);
    CHECK(mk_script(&G, "a1a8") && pos()->count_kind == MK_COUNT_NONE && mk_moves_left(pos()) < 0);
}

/* the other draws: two bare Khun, a third repetition */
static void test_draws(void)
{
    setup("Kd1 ke8 Ma4", MK_BLACK);
    CHECK(mk_status(&G) == MS_PLAY);
    setup("Kd2 ke5 Me4", MK_BLACK);
    CHECK(mk_script(&G, "e5e4") && mk_status(&G) == MS_BARE);
    mk_new(&G);
    CHECK(mk_script(&G, "b1d2 b8d7 d2b1 d7b8") && mk_status(&G) == MS_PLAY);
    CHECK(mk_script(&G, "b1d2 b8d7 d2b1") && mk_status(&G) == MS_PLAY);
    CHECK(mk_script(&G, "d7b8") && mk_status(&G) == MS_REPETITION);
}

/* undo: back to the start; bad moves refused */
static void test_undo(void)
{
    mk_new(&G);
    mkpos start = *pos();
    CHECK(mk_script(&G, "e3e4 d6d5 e4d5 c8d7") && G.ply == 4);
    CHECK(at("d5") == MK_PC(MK_WHITE, MK_BIA) && at("d7") == MK_PC(MK_BLACK, MK_KHON));
    while (mk_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !mk_undo(&G));
    CHECK(!mk_script(&G, "e3e5") && !mk_script(&G, "d1c1") && !mk_script(&G, "e3") && !mk_script(&G, "i3i4"));
    CHECK(!mk_script(&G, "e6e5") && G.ply == 0);   /* not white's piece */
    CHECK(mk_script(&G, "e3-e4 d6-d5 e4xd5") && G.ply == 3 && !mk_script(&G, "d7d6"));   /* no Bia on rank 7 */
}

int main(void)
{
    test_start();
    test_pieces();
    test_promotion();
    test_check();
    test_counting();
    test_draws();
    test_undo();
    printf("makruk: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
