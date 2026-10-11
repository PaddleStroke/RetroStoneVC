/*
 * The draughts rules on a dozen positions: the start, the longest capture (international), flying kings, the pieces
 * taken staying on the board until the move ends, the crowning, English men and multi-jumps, the draws, the wins.
 * The squares are the standard numbers (draughts.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/draughts.h"

#define I DR_INTERNATIONAL
#define E DR_ENGLISH

static int fails;
static dpos P;
static dmove M[DR_MAX_MOVES];
static int N;

static void ok(const char *name, int good)
{
    printf("%s %s\n", good ? "ok  " : "FAIL", name);
    fails += !good;
}

/* a position (player `side` to move) and its legal moves in M[0..N) */
static void pos(int variant, int side, const char *pieces0, const char *pieces1)
{
    if (!dr_setup(&P, variant, side, pieces0, pieces1)) printf("bad setup %s / %s\n", pieces0, pieces1);
    N = dr_legal(&P, variant, M);
}

static int has(int variant, const char *move)
{
    int used;
    return dr_parse(M, N, variant, move, &used) >= 0;
}

/* every legal move takes n pieces */
static int all_take(int n)
{
    for (int i = 0; i < N; i++)
        if (M[i].ncap != n) return 0;
    return N > 0;
}

static int takes(const dmove *m, int variant, int number)
{
    return memchr(m->cap, dr_square(variant, number), m->ncap) != NULL;
}

/* plays the moves (square numbers) from a position; the game's status after them */
static dstatus game(int variant, int side, const char *pieces0, const char *pieces1, int quiet, const char *moves)
{
    static dgame g;
    g.variant = variant;
    g.ply = 0;
    dr_setup(&g.pos[0], variant, side, pieces0, pieces1);
    g.pos[0].quiet = (uint16_t)quiet;
    for (const char *s = moves; *s;) {
        dmove m[DR_MAX_MOVES];
        int n = dr_legal(dr_pos(&g), variant, m), used, k = dr_parse(m, n, variant, s, &used);
        if (k < 0) { printf("illegal %s\n", s); return 99; }
        dr_play(&g, &m[k]);
        s += used;
        while (*s == ' ') s++;
    }
    return dr_status(&g);
}

int main(void)
{
    int round = 1;
    for (int v = I; v <= E; v++)
        for (int k = 1; k <= dr_size(v) * dr_size(v) / 2; k++) round &= dr_number(v, dr_square(v, k)) == k;
    ok("square numbers both ways", round);

    dr_start(&P, I);
    ok("international start: 9 moves, 32-28 among them", (N = dr_legal(&P, I, M)) == 9 && has(I, "32-28"));
    dr_start(&P, E);
    ok("English start: 7 moves, 11-15 among them", (N = dr_legal(&P, E, M)) == 7 && has(E, "11-15"));

    pos(I, 0, "41 43", "37 39 29");                         /* 41 can take one, 43 two: only the two */
    ok("international: the most pieces must be taken", N == 1 && has(I, "43x34x23") && all_take(2));

    pos(I, 0, "28", "32");
    ok("international men take backwards", N == 1 && has(I, "28x37"));
    pos(E, 0, "18", "15");
    ok("English men do not", N == 2 && all_take(0));

    pos(I, 0, "K46", "23");
    ok("flying king: lands anywhere beyond", N == 4 && all_take(1) && has(I, "46x19") && has(I, "46x5"));
    pos(I, 0, "K46", "23 19");
    ok("flying king: two pieces in a row block it", N == 4 && all_take(0));
    pos(I, 0, "K46", "32 29");                              /* only from 23 can it take the second */
    int via = N == 3 && all_take(2);
    for (int i = 0; i < N; i++) via &= M[i].path[0] == dr_square(I, 23);
    ok("flying king: lands where it can take on", via);

    pos(I, 0, "K46", "32 29 39 21");                        /* 32, 29, 39 then back across 32 to 21: blocked */
    int turk = N == 2 && all_take(3);
    for (int i = 0; i < N; i++) turk &= !takes(&M[i], I, 21);
    ok("pieces taken stay until the move ends (Turkish strike)", turk);
    pos(I, 0, "K27", "22 23 33 32");                        /* round the four and back home: no piece twice */
    ok("no piece jumped twice", all_take(4));

    pos(I, 0, "12", "8 9");
    int k = has(I, "12x3x14") ? 0 : -1;
    if (k == 0) dr_make(&P, I, &M[0]);
    ok("international: no crown passing the far row", N == 1 && !k && P.sq[dr_square(I, 14)] == DR_MAN0);
    pos(I, 0, "12", "8");
    dr_make(&P, I, &M[0]);
    ok("international: crowned ending there", N == 1 && P.sq[dr_square(I, 3)] == DR_KING0);

    pos(E, 0, "24", "27 26");                               /* crowned on 31, the king could take 26: the move ends */
    ok("English: crowning ends the move", N == 1 && has(E, "24x31") && M[0].crown && M[0].ncap == 1);
    pos(E, 0, "8 13", "11 18 17");
    ok("English: any capture, but a multi-jump to its end", N == 2 && has(E, "8x15x22") && has(E, "13x22") &&
       M[0].ncap + M[1].ncap == 3);
    pos(E, 0, "K4", "11");
    ok("English king: one square at a time", N == 1 && has(E, "4-8"));

    const char *shuffle = "46-41 1-6 41-46 6-1 46-41 1-6 41-46";
    ok("twice the same position: still playing", game(I, 0, "K46", "K1", 0, shuffle) == DR_PLAY);
    ok("threefold repetition", game(I, 0, "K46", "K1", 0, "46-41 1-6 41-46 6-1 46-41 1-6 41-46 6-1") ==
       DR_REPETITION);
    ok("international: 25 moves each with kings only", game(I, 0, "K46 31", "K5", 49, "46-41") == DR_QUIET);
    ok("... a man moved: on", game(I, 0, "K46 31", "K5", 49, "31-26") == DR_PLAY);
    ok("English: 40 moves each", game(E, 0, "K4", "K32", 79, "4-8") == DR_QUIET &&
       game(E, 0, "K4", "K32", 78, "4-8") == DR_PLAY);

    ok("no move left: lost", game(I, 0, "46", "41 37", 0, "") == DR_WON1);
    ok("no piece left: lost", game(I, 0, "28", "32", 0, "28x37") == DR_WON0 && game(E, 0, "", "K32", 0, "") == DR_WON1);

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
