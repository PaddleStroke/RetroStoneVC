/*
 * Ludo: the rules' test (the yard and the 6, the extra throw, three 6s, captures and safe squares, the home column and
 * the exact finish, a throw with no move, the win, undo); the petits chevaux (out on a 6, captures, no passing, the
 * exact stop at the foot of the stairs, each step's number, the final 6, the win, whole games); three and four players
 * (the turn round the table, captures between any players, the win). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/ludo.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static lgame G;
static const lpos *P = &G.pos;
static int V = LUDO_CLASSIC, NP = 2;           /* the variant and the players setup() starts */

/* a position: each player's four tokens (-1 the yard), the player to throw */
static void setup(const int a[4], const int b[4], int turn)
{
    ludo_new(&G, V, NP);
    for (int t = 0; t < LUDO_TOKENS; t++) {
        G.pos.at[0][t] = (int8_t)a[t];
        G.pos.at[1][t] = (int8_t)b[t];
    }
    G.pos.turn = (uint8_t)turn;
}

static const int YARD[4] = {-1, -1, -1, -1};

static void test_board(void)
{
    int safe = 0;
    for (int s = 0; s < LUDO_TRACK; s++) safe += ludo_safe(s);
    CHECK(safe == 8 && ludo_safe(0) && ludo_safe(8) && ludo_safe(13) && ludo_safe(26) && !ludo_safe(1));
    CHECK(ludo_square(0, 0) == 0 && ludo_square(1, 0) == 26 && ludo_square(1, 30) == 4 && ludo_square(0, 50) == 50);
    CHECK(ludo_square(0, 51) == -1 && ludo_square(1, -1) == -1);
}

/* out of the yard on a 6 only; a 6 throws again; a non-6 passes the turn */
static void test_yard(void)
{
    ludo_new(&G, LUDO_CLASSIC, 2);
    CHECK(P->turn == 0 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 5) == LT_PASS && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(!ludo_move(&G, 0));                   /* no move now: a throw first */
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_movable(P) == 15);
    CHECK(ludo_move(&G, 2) == 1 && P->at[1][2] == 0 && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_movable(P) == 4);    /* only the token out moves */
    CHECK(!ludo_move(&G, 0) && ludo_move(&G, 2) == 1 && P->at[1][2] == 3 && P->turn == 0);
}

/* three 6s in a row: the third is lost, the turn ends (the first two moves stay) */
static void test_sixes(void)
{
    ludo_new(&G, LUDO_CLASSIC, 2);
    CHECK(ludo_script(&G, "6 a 6 a"));
    CHECK(P->turn == 0 && P->at[0][0] == 6 && P->sixes == 2);
    CHECK(ludo_throw(&G, 6) == LT_SIXES && P->turn == 1 && P->sixes == 0 && P->at[0][0] == 6);
    /* a 6 with nothing to move: throw again */
    int a[4] = {LUDO_HOME, LUDO_HOME, LUDO_HOME, 53};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 6) == LT_AGAIN && P->turn == 0 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 4) == LT_PASS && P->turn == 1);
}

/* captures: a landing sends the other's tokens home (both of a pair); not on a safe square */
static void test_capture(void)
{
    int a[4] = {10, -1, -1, -1}, b[4] = {-1, -1, -1, -1};
    b[0] = 14 + 26;                             /* player 1's token on square 14: 4 ahead of player 0's */
    b[1] = 14 + 26;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_move(&G, 0) == 3);
    CHECK(P->at[0][0] == 14 && P->at[1][0] == -1 && P->at[1][1] == -1);
    b[0] = 21 + 26;                             /* square 21: a star */
    b[1] = -1;
    a[0] = 15;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == 21 && P->at[1][0] == 47);
    /* on player 1's start square (26): safe, both stand there */
    a[0] = 22;
    b[0] = 0;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[1][0] == 0);
    /* a token coming out onto its start square does not capture there either */
    a[0] = 26;
    setup(a, YARD, 1);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1 && P->at[0][0] == 26 && P->at[1][3] == 0);
    /* two tokens of one player do not block: the other passes them */
    int c[4] = {20, 20, -1, -1}, d[4] = {44, -1, -1, -1};   /* player 1 at 44: square 18, two behind 20 */
    setup(c, d, 1);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[1][0] == 49 && P->at[0][0] == 20);
}

/* the home column: entered after a full lap, the exact number to the centre */
static void test_home(void)
{
    int a[4] = {48, -1, -1, -1};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == 53);
    CHECK(ludo_square(0, 53) == -1);            /* in the home column: off the track, safe from all */
    CHECK(ludo_throw(&G, 4) == LT_PASS);        /* player 1 cannot move */
    CHECK(ludo_throw(&G, 4) == LT_PASS && P->at[0][0] == 53);   /* 53 + 4 > 56: no move, the turn passes */
    CHECK(ludo_throw(&G, 2) == LT_PASS);
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == LUDO_HOME && ludo_home(P, 0) == 1);
    /* a token passing the other player's start goes on round (its own lap ends at 50) */
    int b[4] = {-1, -1, -1, 24};
    setup(YARD, b, 1);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 3) == 1 && P->at[1][3] == 29 && ludo_square(1, 29) == 3);
}

/* the win: the last token home ends the game, even on a 6 */
static void test_win(void)
{
    int a[4] = {LUDO_HOME, LUDO_HOME, LUDO_HOME, 50};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1);
    CHECK(P->winner == 0 && P->phase == LUDO_OVER && !ludo_throw(&G, 3));
}

/* undo: back to the last move's choice, the throws after it too; the throw itself stays */
static void test_undo(void)
{
    ludo_new(&G, LUDO_CLASSIC, 2);
    CHECK(!ludo_undo(&G));
    CHECK(ludo_script(&G, "6 a 4 a 3 6 b"));    /* player 0: out, 4; player 1: 3 (no move); player 0: 6, out */
    CHECK(P->turn == 0 && P->at[0][0] == 4 && P->at[0][1] == 0 && P->throws == 4);
    CHECK(ludo_undo(&G) && P->phase == LUDO_MOVE && P->die == 6 && P->turn == 0 && P->at[0][1] == -1);
    CHECK(ludo_undo(&G) && P->phase == LUDO_MOVE && P->die == 4 && P->at[0][0] == 0 && P->throws == 2);
    CHECK(ludo_move(&G, 0) == 1 && P->at[0][0] == 4);   /* another choice: the same throw */
    CHECK(ludo_undo(&G) && ludo_undo(&G) && P->die == 6 && P->at[0][0] == -1 && P->phase == LUDO_MOVE);
    CHECK(ludo_undo(&G) && G.n == 0 && P->phase == LUDO_THROW && P->throws == 0);
    CHECK(!ludo_undo(&G));
    CHECK(!ludo_script(&G, "6 e") && !ludo_script(&G, "7"));
}

/* a whole game: player 0 always moves its first token that can; player 1 throws 1s and never leaves its yard */
static void test_game(void)
{
    static const int seq[9] = {6, 6, 1, 6, 2, 3, 6, 4, 5};
    ludo_new(&G, LUDO_CLASSIC, 2);
    int k = 0, guard = 0;
    while (P->phase != LUDO_OVER && guard++ < 4000) {
        if (P->phase == LUDO_THROW) {
            CHECK(ludo_throw(&G, P->turn ? 1 : seq[k++ % 9]));
        } else {
            int m = ludo_movable(P), t = 0;
            while (!(m >> t & 1)) t++;
            CHECK(ludo_move(&G, t));
        }
    }
    CHECK(P->winner == 0 && P->phase == LUDO_OVER && ludo_home(P, 0) == 4 && ludo_home(P, 1) == 0);
    while (ludo_undo(&G)) {}                    /* undo all the way */
    CHECK(G.n == 0 && P->throws == 0 && P->at[0][0] == -1 && P->winner == -1);
}

/* ---- the petits chevaux ------------------------------------------------------------------------------------------ */
/* out of the stable on a 6 only, onto the start square (not onto one's own horse); every 6 throws again, no limit */
static void test_ch_exit(void)
{
    ludo_new(&G, LUDO_CHEVAUX, 2);
    CHECK(P->variant == LUDO_CHEVAUX && ludo_goal(P->variant) == CHEVAUX_HOME && ludo_goal(LUDO_CLASSIC) == LUDO_HOME);
    CHECK(ludo_throw(&G, 5) == LT_PASS && P->turn == 1);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_movable(P) == 15);
    CHECK(ludo_move(&G, 2) == 1 && P->at[1][2] == 0 && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_movable(P) == 4);    /* the start square taken: only that horse */
    CHECK(!ludo_move(&G, 0) && ludo_move(&G, 2) == 1 && P->at[1][2] == 6);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && P->turn == 1);    /* the third 6 in a row: no loss in this game */
    CHECK(ludo_move(&G, 0) == 1 && P->at[1][0] == 0 && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 2) == LT_MOVE && ludo_move(&G, 2) == 1 && P->turn == 0);
}

/* captures: stopping on the other player's horse sends it back, anywhere (no safe square), coming out too */
static void test_ch_capture(void)
{
    V = LUDO_CHEVAUX;
    int a[4] = {10, -1, -1, -1}, b[4] = {14 + 26, -1, -1, -1};
    setup(a, b, 0);
    CHECK(ludo_hits(P, 0) == 0);                /* no throw yet */
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_hits(P, 0) == 1 << LUDO_TOKENS && ludo_move(&G, 0) == 2);
    CHECK(P->at[0][0] == 14 && P->at[1][0] == -1);
    a[0] = 15;                                  /* square 21: a star in Ludo, nothing special here */
    b[0] = 21 + 26;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 0) == 2 && P->at[1][0] == -1);
    a[0] = 26;                                  /* on player 1's start square: a horse coming out takes it */
    b[0] = 30;
    setup(a, b, 1);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_movable(P) == 15 && ludo_hits(P, 1) == 1);
    CHECK(ludo_move(&G, 1) == 2 && P->at[1][1] == 0 && P->at[0][0] == -1);
}

/* no horse passes another, of either player, nor stops on one of its own */
static void test_ch_block(void)
{
    V = LUDO_CHEVAUX;
    int a[4] = {10, -1, -1, -1}, b[4] = {14 + 26, -1, -1, -1};
    setup(a, b, 0);                             /* player 1's horse 4 squares ahead */
    CHECK(ludo_throw(&G, 5) == LT_PASS && P->at[0][0] == 10);
    int c[4] = {10, 12, -1, -1};
    setup(c, YARD, 0);                          /* its own horse 2 ahead */
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_movable(P) == 2 && ludo_dest(P, 0) == -1);
    setup(c, YARD, 0);
    CHECK(ludo_throw(&G, 2) == LT_MOVE && ludo_movable(P) == 2);     /* not onto its own horse either */
    setup(c, YARD, 0);
    CHECK(ludo_throw(&G, 1) == LT_MOVE && ludo_movable(P) == 3 && ludo_move(&G, 0) == 1 && P->at[0][0] == 11);
    /* a horse waiting at the foot of its stairs blocks the other player's horses behind it (or is taken) */
    int d[4] = {LUDO_FOOT, -1, -1, -1}, e[4] = {20, -1, -1, -1};
    CHECK(ludo_square(0, LUDO_FOOT) == 50 && ludo_square(1, 20) == 46);
    setup(d, e, 1);
    CHECK(ludo_throw(&G, 5) == LT_PASS && P->turn == 0);
    setup(d, e, 1);
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_move(&G, 0) == 2 && P->at[0][0] == -1);
    /* in Ludo, the same tokens pass each other */
    V = LUDO_CLASSIC;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == 15);
}

/* the lap ends exactly at the foot of the stairs; then each step needs its number, and a 6 the centre */
static void test_ch_stairs(void)
{
    V = LUDO_CHEVAUX;
    int a[4] = {47, CHEVAUX_HOME, CHEVAUX_HOME, CHEVAUX_HOME};   /* the others home: a 6 cannot move them */
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 4) == LT_PASS && P->at[0][0] == 47);   /* past the foot: no move */
    CHECK(ludo_throw(&G, 1) == LT_PASS && P->turn == 0);        /* player 1: nothing out */
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == LUDO_FOOT);
    for (int step = 1; step <= LUDO_STEPS + 1; step++) {
        int need = step <= LUDO_STEPS ? step : 6;   /* step n needs an n; the centre a 6 */
        for (int v = 1; v <= 6; v++) {
            if (v == need) continue;
            G.pos.turn = 0;
            G.pos.phase = LUDO_THROW;
            CHECK(ludo_throw(&G, v) == (v == 6 ? LT_AGAIN : LT_PASS) && P->at[0][0] == LUDO_FOOT + step - 1);
        }
        G.pos.turn = 0;
        G.pos.phase = LUDO_THROW;
        CHECK(ludo_throw(&G, need) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == LUDO_FOOT + step);
        CHECK(ludo_square(0, P->at[0][0]) == -1);   /* off the track: nobody takes it there */
    }
    CHECK(P->at[0][0] == CHEVAUX_HOME && ludo_home(P, 0) == 4 && P->winner == 0 && P->phase == LUDO_OVER);
    /* one horse a step: the one below waits */
    int b[4] = {LUDO_FOOT + 1, LUDO_FOOT, -1, -1};
    setup(b, YARD, 0);
    CHECK(ludo_throw(&G, 1) == LT_PASS);
    setup(b, YARD, 0);
    CHECK(ludo_throw(&G, 2) == LT_MOVE && ludo_movable(P) == 1 && ludo_move(&G, 0) == 1);
    CHECK(ludo_throw(&G, 1) == LT_PASS);        /* player 1 */
    CHECK(ludo_throw(&G, 1) == LT_MOVE && ludo_movable(P) == 2 && ludo_move(&G, 1) == 1 && P->at[0][1] == 51);
}

/* the win: the last horse into the centre, with the final 6 */
static void test_ch_win(void)
{
    V = LUDO_CHEVAUX;
    int a[4] = {CHEVAUX_HOME, CHEVAUX_HOME, CHEVAUX_HOME, LUDO_FOOT + LUDO_STEPS};
    setup(a, YARD, 0);
    CHECK(ludo_home(P, 0) == 3 && P->winner == -1);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1);
    CHECK(P->winner == 0 && P->phase == LUDO_OVER && ludo_home(P, 0) == 4 && !ludo_throw(&G, 3));
    CHECK(ludo_undo(&G) && P->winner == -1 && P->phase == LUDO_MOVE && P->at[0][3] == LUDO_FOOT + LUDO_STEPS);
}

/* never two horses on one square (but the centre) */
static int apart(void)
{
    for (int i = 0; i < 2 * LUDO_TOKENS; i++)
        for (int j = 0; j < i; j++) {
            int pi = i / LUDO_TOKENS, pj = j / LUDO_TOKENS, ai = P->at[pi][i % LUDO_TOKENS],
                aj = P->at[pj][j % LUDO_TOKENS];
            if (ai < 0 || aj < 0 || ai == CHEVAUX_HOME) continue;
            if (pi == pj ? ai == aj : ludo_square(pi, ai) >= 0 && ludo_square(pi, ai) == ludo_square(pj, aj)) return 0;
        }
    return 1;
}

/* whole games with pseudo-random throws, each player moving its first horse that can: every game ends, far inside
 * the events a game keeps; undo goes back to the start */
static void test_ch_games(void)
{
    unsigned seed = 12345;
    int longest = 0, wins[2] = {0, 0}, ok = 1;
    for (int game = 0; game < 300; game++) {
        ludo_new(&G, LUDO_CHEVAUX, 2);
        while (P->phase != LUDO_OVER && G.n < LUDO_MAX_EVENTS) {
            if (P->phase == LUDO_THROW) {
                seed = seed * 1103515245u + 12345u;
                ok &= ludo_throw(&G, 1 + (int)(seed >> 16) % 6) != 0;
            } else {
                int m = ludo_movable(P), t = 0;
                while (!(m >> t & 1)) t++;
                ok &= ludo_move(&G, t) != 0 && apart();
            }
        }
        CHECK(P->phase == LUDO_OVER && ludo_home(P, P->winner) == 4);
        if (P->winner >= 0) wins[P->winner]++;
        if (G.n > longest) longest = G.n;
    }
    CHECK(ok);
    while (ludo_undo(&G)) {}
    CHECK(G.n == 0 && P->throws == 0 && P->at[0][0] == -1 && P->variant == LUDO_CHEVAUX);
    CHECK(longest < LUDO_MAX_EVENTS / 2 && wins[0] && wins[1]);
    printf("petits chevaux: 300 games, the longest %d events, won %d / %d\n", longest, wins[0], wins[1]);
}

/* ---- three and four players ------------------------------------------------------------------------------------- */
/* a position of `players`: each one's four tokens, the player to throw */
static void setup_n(int variant, int players, const int at[][4], int turn)
{
    ludo_new(&G, variant, players);
    for (int p = 0; p < players; p++)
        for (int t = 0; t < LUDO_TOKENS; t++) G.pos.at[p][t] = (int8_t)at[p][t];
    G.pos.turn = (uint8_t)turn;
}

/* the seats clockwise: 0, 2, 1, 3; the turn goes round them, skipping an empty yard */
static void test_n_turns(void)
{
    CHECK(ludo_seat(0) == 0 && ludo_seat(2) == 1 && ludo_seat(1) == 2 && ludo_seat(3) == 3);
    CHECK(ludo_square(2, 0) == 13 && ludo_square(1, 0) == 26 && ludo_square(3, 0) == 39 && ludo_square(3, 13) == 0);
    ludo_new(&G, LUDO_CLASSIC, 3);
    CHECK(P->players == 3 && P->turn == 0);
    CHECK(ludo_throw(&G, 2) == LT_PASS && P->turn == 2);
    CHECK(ludo_throw(&G, 2) == LT_PASS && P->turn == 1);
    CHECK(ludo_throw(&G, 2) == LT_PASS && P->turn == 0);
    ludo_new(&G, LUDO_CHEVAUX, 4);
    static const int order[8] = {2, 1, 3, 0, 2, 1, 3, 0};
    for (int i = 0; i < 8; i++) CHECK(ludo_throw(&G, 3) == LT_PASS && P->turn == order[i]);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 0) == 1 && P->turn == 0);   /* a 6: the same player again */
    CHECK(ludo_throw(&G, 1) == LT_MOVE && ludo_move(&G, 0) == 1 && P->turn == 2 && P->at[0][0] == 1);
    ludo_new(&G, LUDO_CLASSIC, 5);              /* not a count it takes: two */
    CHECK(P->players == 2 && ludo_throw(&G, 1) == LT_PASS && P->turn == 1);
}

/* captures between any players: all the tokens of the others on the square go back (one move, two players hit) */
static void test_n_capture(void)
{
    const int at[4][4] = {{14, -1, -1, -1}, {-1, -1, -1, -1}, {0, -1, -1, -1}, {27, 27, -1, -1}};
    setup_n(LUDO_CLASSIC, 4, at, 2);            /* player 2 on its start (13); players 0 and 3 on square 14 */
    CHECK(ludo_square(0, 14) == 14 && ludo_square(3, 27) == 14 && ludo_square(2, 1) == 14);
    CHECK(ludo_throw(&G, 1) == LT_MOVE && ludo_hits(P, 0) == (1 | 1 << 12 | 1 << 13));
    CHECK(ludo_move(&G, 0) == 4 && P->at[0][0] == -1 && P->at[3][0] == -1 && P->at[3][1] == -1 && P->turn == 1);
    /* three players: player 2 takes player 1 */
    const int b[3][4] = {{-1, -1, -1, -1}, {15, -1, -1, -1}, {26, -1, -1, -1}};
    setup_n(LUDO_CLASSIC, 3, b, 2);             /* player 2 at square 39, player 1 at 41 */
    CHECK(ludo_throw(&G, 2) == LT_MOVE && ludo_move(&G, 0) == 2 && P->at[2][0] == 28 && P->at[1][0] == -1);
    CHECK(P->turn == 1);
    /* the petits chevaux, four players: a horse of another player blocks; on the square, it is taken */
    const int c[4][4] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}, {38, -1, -1, -1}, {10, -1, -1, -1}};
    setup_n(LUDO_CHEVAUX, 4, c, 3);             /* player 3 at square 49, player 2 at 51 */
    CHECK(ludo_square(3, 10) == 49 && ludo_square(2, 38) == 51);
    CHECK(ludo_throw(&G, 3) == LT_PASS && P->turn == 0);
    setup_n(LUDO_CHEVAUX, 4, c, 3);
    CHECK(ludo_throw(&G, 2) == LT_MOVE && ludo_hits(P, 0) == 1 << 8 && ludo_move(&G, 0) == 2 && P->at[2][0] == -1);
}

/* the first to bring all its tokens home wins, and the game ends there */
static void test_n_win(void)
{
    const int at[3][4] = {{50, 50, -1, -1}, {LUDO_HOME, LUDO_HOME, 40, -1}, {LUDO_HOME, LUDO_HOME, LUDO_HOME, 50}};
    setup_n(LUDO_CLASSIC, 3, at, 2);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1);
    CHECK(P->winner == 2 && P->phase == LUDO_OVER && !ludo_throw(&G, 1) && ludo_home(P, 2) == 4);
    CHECK(ludo_undo(&G) && P->winner == -1 && P->turn == 2);
}

/* whole games, three and four players, both rules: each ends with one winner, the turns going round the table */
static void test_n_games(void)
{
    unsigned seed = 777;
    int ok = 1, wins[LUDO_PLAYERS] = {0};
    for (int game = 0; game < 200; game++) {
        int np = 3 + game % 2;
        ludo_new(&G, game / 2 % 2 ? LUDO_CHEVAUX : LUDO_CLASSIC, np);
        while (P->phase != LUDO_OVER && G.n < LUDO_MAX_EVENTS) {
            if (P->phase == LUDO_THROW) {
                int before = P->turn, v;
                seed = seed * 1103515245u + 12345u;
                v = 1 + (int)(seed >> 16) % 6;
                int r = ludo_throw(&G, v);
                ok &= r != 0;
                if (r == LT_PASS || r == LT_SIXES) {    /* the next seat with a player */
                    static const int who[4] = {0, 2, 1, 3};     /* a seat's player */
                    int s = ludo_seat(before);
                    do s = (s + 1) & 3; while (who[s] >= np);
                    ok &= P->turn == who[s];
                }
            } else {
                int m = ludo_movable(P), t = 3;
                while (!(m >> t & 1)) t--;
                ok &= ludo_move(&G, t) != 0;
            }
        }
        CHECK(P->phase == LUDO_OVER && P->winner >= 0 && P->winner < np && ludo_home(P, P->winner) == 4);
        if (P->winner >= 0) wins[P->winner]++;
    }
    CHECK(ok && wins[0] && wins[1] && wins[2] && wins[3]);
}

int main(void)
{
    test_board();
    test_yard();
    test_sixes();
    test_capture();
    test_home();
    test_win();
    test_undo();
    test_game();
    test_ch_exit();
    test_ch_capture();
    test_ch_block();
    test_ch_stairs();
    test_ch_win();
    test_ch_games();
    test_n_turns();
    test_n_capture();
    test_n_win();
    test_n_games();
    printf("ludo: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
