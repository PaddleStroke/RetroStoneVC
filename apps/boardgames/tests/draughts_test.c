/*
 * The draughts rules on a dozen positions: the start, the longest capture (international), flying kings, the pieces
 * taken staying on the board until the move ends, the crowning, English men and multi-jumps, the draws, the wins; then
 * Russian (any capture, crowned in a capture), Brazilian (the maximum on 8 x 8) and Turkish draughts (straight lines,
 * the pieces taken going at once, no turning back), and their draws; Frisian (rows and files, the value rule, the
 * kings' three moves, its draws), Canadian (12 x 12) and bashni (columns); whole random games of these three. The
 * squares are the standard numbers, or a1-h8 (draughts.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/draughts.h"

#define I DR_INTERNATIONAL
#define E DR_ENGLISH
#define RU DR_RUSSIAN
#define BR DR_BRAZILIAN
#define TK DR_TURKISH
#define FR DR_FRISIAN
#define CA DR_CANADIAN
#define BA DR_BASHNI

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

/* a square by its name ("c3") */
static int at(int variant, const char *name)
{
    int used;
    return dr_read(variant, name, &used);
}

/* plays the moves (the variant's notation) from a position; the game's status after them */
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

/* Frisian: captures along rows and files, the value rule, the king first, the kings' three moves, the draws */
static void frisian(void)
{
    dr_start(&P, FR);
    ok("Frisian start: 20 men each, 9 moves", (N = dr_legal(&P, FR, M)) == 9 && has(FR, "32-28"));
    pos(FR, 0, "28", "29");
    ok("Frisian: a man takes along the row", N == 1 && has(FR, "28x30"));
    pos(FR, 0, "28", "18");
    ok("Frisian: ... along the file, forwards", N == 1 && has(FR, "28x8"));
    pos(FR, 0, "28", "38");
    ok("Frisian: ... and backwards", N == 1 && has(FR, "28x48"));
    pos(FR, 0, "28", "26");                                 /* 26 on the edge: nothing beyond it */
    ok("Frisian: no landing square, no capture", all_take(0));
    pos(FR, 0, "K46", "48");
    ok("Frisian king: takes along the row from afar", N == 2 && all_take(1) && has(FR, "46x49") && has(FR, "46x50"));
    pos(FR, 0, "K46", "21");                                /* 46 (0, 0) up the file: 36, 26, 16, 6 */
    ok("Frisian king: moves along the diagonals only", !has(FR, "46-36") && has(FR, "46-41") && all_take(0));

    pos(FR, 0, "41 44", "37 27 K40");                       /* two men, or one king: the men */
    ok("Frisian: two men are worth more than a king", N == 1 && has(FR, "41x21") && all_take(2));
    pos(FR, 0, "41 44", "37 27 K40 30");                    /* two men, or a king and a man: the king and the man */
    ok("Frisian: a king and a man more than two men", N == 1 && has(FR, "44x24") && all_take(2));
    pos(FR, 0, "41 44", "K37 K27 33");                      /* two kings (41), or a man (44)... */
    int two = N >= 1 && has(FR, "41x21");
    pos(FR, 0, "41 K44", "37 40");                          /* a man each: the king takes */
    ok("Frisian: two kings more than one man", two);
    ok("Frisian: of equal value, the king takes", N == 1 && has(FR, "44x35"));

    /* the king on 46 moves three times in a row: then the man must move; with kings alone, no limit */
    ok("Frisian: three king moves in a row", game(FR, 0, "K46 50", "K1", 0, "46-41 1-6 41-46 6-1 46-41 1-6") ==
       DR_PLAY);
    static dgame g;
    g.variant = FR;
    g.ply = 0;
    dr_setup(&g.pos[0], FR, 0, "K46 50", "K1");
    const char *three = "46-41 1-6 41-46 6-1 46-41 1-6";
    for (const char *s = three; *s;) {
        int n = dr_legal(dr_pos(&g), FR, M), used, k = dr_parse(M, n, FR, s, &used);
        if (k < 0) break;
        dr_play(&g, &M[k]);
        s += used;
        while (*s == ' ') s++;
    }
    N = dr_legal(dr_pos(&g), FR, M);
    int only_man = N > 0;
    for (int i = 0; i < N; i++) only_man &= M[i].from == dr_square(FR, 50);
    ok("... then not a fourth: the man moves", only_man);
    ok("... and after it the king again", game(FR, 0, "K46 50", "K1", 0, "46-41 1-6 41-46 6-1 46-41 1-6 50-45 6-1 41-46")
       == DR_PLAY);
    ok("... another king may move", game(FR, 0, "K46 K47 50", "K1", 0, "46-41 1-6 41-46 6-1 46-41 1-6 47-42") ==
       DR_PLAY);
    ok("Frisian: kings alone, no limit", game(FR, 0, "K46 K50", "K1", 0, "46-41 1-6 41-46 6-1 46-41 1-6 41-46") !=
       99);
    ok("Frisian: two kings against one, seven moves each", game(FR, 0, "K46 K50", "K1", 13, "46-41") == DR_QUIET &&
       game(FR, 0, "K46 K50", "K1", 12, "46-41") == DR_PLAY);
    ok("Frisian: one king each, two moves each", game(FR, 0, "K46", "K1", 3, "46-41") == DR_QUIET &&
       game(FR, 0, "K46", "K1", 2, "46-41") == DR_PLAY);
    ok("Frisian: men on the board, no such draw", game(FR, 0, "K46 31", "K1", 40, "46-41") == DR_PLAY);
}

/* Canadian: the international rules on 12 x 12 */
static void canadian(void)
{
    int round = 1;
    for (int k = 1; k <= 72; k++) round &= dr_number(CA, dr_square(CA, k)) == k;
    ok("Canadian: square numbers both ways", round);
    dr_start(&P, CA);
    int men[2] = {0, 0};
    for (int s = 0; s < 144; s++)
        if (P.sq[s]) men[DR_OWNER(P.sq[s])]++;
    ok("Canadian start: 30 men each, 11 moves", men[0] == 30 && men[1] == 30 && (N = dr_legal(&P, CA, M)) == 11 &&
       P.sq[dr_square(CA, 72)] == DR_MAN0 && P.sq[dr_square(CA, 1)] == DR_MAN1);
    pos(CA, 0, "61 63", "56 58 47");                        /* 61 takes one, 63 two: only the two */
    ok("Canadian: the most pieces must be taken", N == 1 && all_take(2));
    pos(CA, 0, "K67", "56");
    ok("Canadian: flying king", all_take(1) && N >= 2);
}

/* bashni: Russian draughts with columns */
static void bashni(void)
{
    dr_start(&P, BA);
    ok("bashni start: 7 moves", (N = dr_legal(&P, BA, M)) == 7 && has(BA, "c3-d4"));
    pos(BA, 0, "c3", "d4");
    dr_make(&P, BA, &M[0]);
    ok("bashni: the man taken goes under the taker", N == 1 && !P.sq[at(BA, "d4")] &&
       dr_height(&P, at(BA, "e5")) == 2 && P.sq[at(BA, "e5")] == DR_MAN0 && dr_prisoners(&P, 0) == 1);
    pos(BA, 0, "c3", "d4");
    dr_stack(&P, at(BA, "d4"), "w");                        /* a man of player 0 under the black one */
    N = dr_legal(&P, BA, M);
    dr_make(&P, BA, &M[0]);
    ok("bashni: only the top piece taken, the one beneath freed", N == 1 && P.sq[at(BA, "d4")] == DR_MAN0 &&
       dr_height(&P, at(BA, "d4")) == 1 && dr_height(&P, at(BA, "e5")) == 2 && dr_column(&P, at(BA, "e5")) == 4);
    pos(BA, 0, "Ka1", "d4");
    dr_stack(&P, at(BA, "d4"), "b");
    N = dr_legal(&P, BA, M);
    ok("bashni: a column jumped once in a capture", N == 4 && all_take(1));
    pos(BA, 0, "c3", "b2 d4");
    dr_stack(&P, at(BA, "c3"), "bb");                       /* a white man commanding two black ones */
    N = dr_legal(&P, BA, M);
    ok("bashni: a column moves as its top piece", N == 2 && has(BA, "c3:a1") && has(BA, "c3:e5"));
    dr_make(&P, BA, &M[has(BA, "c3:e5") && M[0].path[0] == at(BA, "e5") ? 0 : 1]);
    ok("bashni: the pieces taken go to the bottom", dr_height(&P, at(BA, "e5")) == 4 && dr_prisoners(&P, 0) == 3);
    pos(BA, 0, "f6", "e7 b6");                              /* crowned on d8, it takes b6 as a king */
    dr_stack(&P, at(BA, "f6"), "b");
    N = dr_legal(&P, BA, M);
    int k = N == 1 && has(BA, "f6:d8:a5") ? 0 : -1;
    if (!k) dr_make(&P, BA, &M[0]);
    ok("bashni: only the top piece crowned", !k && P.sq[at(BA, "a5")] == DR_KING0 &&
       dr_column(&P, at(BA, "a5")) == (2u | 1u << 2 | 1u << 4 | 1u << 6));
    pos(BA, 0, "c3", "d4");
    dr_stack(&P, at(BA, "c3"), "b");
    static dgame g;
    g.variant = BA;
    g.ply = 0;
    g.pos[0] = P;
    ok("bashni: a column's pieces count in the repetition", dr_status(&g) == DR_PLAY);
}

/* whole random games (a seed each): every move legal by the rules' own tests, the pieces never more (bashni: never
 * fewer either, 24 in the columns), Frisian's king never four simple moves in a row with men left, an end */
static void random_games(int variant, int games)
{
    static dgame g;
    unsigned seed = 777u + (unsigned)variant;
    int bad = 0, ends[5] = {0}, n = dr_size(variant);
    for (int k = 0; k < games; k++) {
        dr_new(&g, variant);
        dstatus st;
        int streak[2] = {0, 0}, last_at[2] = {-1, -1};
        while ((st = dr_status(&g)) == DR_PLAY && g.ply < DR_MAX_PLY) {
            int nm = dr_legal(dr_pos(&g), variant, M);
            seed = seed * 1103515245u + 12345u;
            const dmove *m = &M[(seed >> 8) % (unsigned)nm];
            const dpos *p = dr_pos(&g);
            int side = p->side, pc = p->sq[m->from], men = 0;
            for (int s = 0; s < n * n; s++) men += p->sq[s] == DR_MAN0 + side;
            if (!m->ncap && DR_IS_KING(pc)) {
                streak[side] = last_at[side] == m->from ? streak[side] + 1 : 1;
                last_at[side] = m->path[m->n - 1];
                if (variant == FR && men && streak[side] > 3) bad++;
            } else streak[side] = 0;
            dr_play(&g, m);
            int pieces = 0;
            for (int s = 0; s < n * n; s++) pieces += dr_height(dr_pos(&g), s);
            if (variant == BA ? pieces != 24 : pieces > 2 * dr_pieces(variant)) bad++;
        }
        ends[st]++;
    }
    printf("%s %d random games, variant %d: %d won, %d drawn, %d unfinished, %d bad\n", bad ? "FAIL" : "ok  ", games,
           variant, ends[DR_WON0] + ends[DR_WON1], ends[DR_REPETITION] + ends[DR_QUIET], ends[DR_PLAY], bad);
    fails += bad != 0;
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

    /* Russian: 8 x 8, ivory first, flying kings, men take backwards, any capture, crowned in the middle of a capture */
    dr_start(&P, RU);
    ok("Russian start: 7 moves, c3-d4 among them", (N = dr_legal(&P, RU, M)) == 7 && has(RU, "c3-d4"));
    pos(RU, 0, "a3 e3", "b4 f4 f6");                        /* a3 can take one, e3 two: either */
    ok("Russian: no maximum rule", N == 2 && has(RU, "a3:c5") && has(RU, "e3:g5:e7"));
    pos(RU, 0, "c5", "d4");
    ok("Russian men take backwards", N == 1 && has(RU, "c5:e3"));
    pos(RU, 0, "f6", "e7 b6");                              /* crowned on d8, the new king takes b6 from afar */
    k = N == 1 && has(RU, "f6:d8:a5") && M[0].crown && M[0].ncap == 2 ? 0 : -1;
    if (!k) dr_make(&P, RU, &M[0]);
    ok("Russian: crowned in a capture, it takes on as a king", !k && P.sq[at(RU, "a5")] == DR_KING0);
    pos(RU, 0, "Ka1", "c3 g3");                             /* past c3 only from e5 can it take on */
    ok("Russian king: lands where it can take on", N == 1 && has(RU, "a1:e5:h2"));


    /* Brazilian: the international rules on 8 x 8 */
    dr_start(&P, BR);
    ok("Brazilian start: 7 moves", (N = dr_legal(&P, BR, M)) == 7 && has(BR, "c3-d4"));
    pos(BR, 0, "a3 e3", "b4 f4 f6");
    ok("Brazilian: the most pieces must be taken", N == 1 && has(BR, "e3:g5:e7") && all_take(2));
    pos(BR, 0, "f6", "e7 b6");
    ok("Brazilian: crowned ending on the far row, no taking on", N == 1 && has(BR, "f6:d8") && M[0].crown);
    pos(BR, 0, "Ka1", "c3");
    ok("Brazilian: flying king", N == 5 && all_take(1) && has(BR, "a1:h8"));

    /* Turkish: every square, straight lines, men forwards and sideways, pieces taken removed at once */
    dr_start(&P, TK);
    ok("Turkish start: 16 men each, 8 moves", (N = dr_legal(&P, TK, M)) == 8 && has(TK, "a3-a4") &&
       P.sq[at(TK, "a2")] == DR_MAN0 && P.sq[at(TK, "h7")] == DR_MAN1 && !P.sq[at(TK, "a1")]);
    pos(TK, 0, "d4", "h8");
    ok("Turkish men: forwards and sideways", N == 3 && has(TK, "d4-d5") && has(TK, "d4-c4") && has(TK, "d4-e4") &&
       !has(TK, "d4-d3") && !has(TK, "d4-e5"));
    pos(TK, 0, "d4", "d5 c4 d3");                           /* d3 behind: not taken */
    ok("Turkish men take forwards and sideways", N == 2 && all_take(1) && has(TK, "d4xd6") && has(TK, "d4xb4"));
    pos(TK, 0, "a4 e4", "a5 e5 f6");
    ok("Turkish: the most pieces must be taken", N == 1 && has(TK, "e4xe6xg6"));
    pos(TK, 0, "Kc1", "c3 e5 g4 b3");                       /* back along row 3 over c3, gone already */
    ok("Turkish: pieces taken go at once", N == 1 && all_take(4) && has(TK, "c1xc5xg5xg3xa3"));
    pos(TK, 0, "Kd4", "f4 b4");                             /* past f4, back to b4: a turn back, not allowed */
    ok("Turkish: no turning straight back", N == 3 && all_take(1));
    pos(TK, 0, "Kd4", "h8");
    ok("Turkish king: flies along rows and files", N == 14 && has(TK, "d4-d8") && has(TK, "d4-a4") &&
       !has(TK, "d4-e5"));
    pos(TK, 0, "Ka1", "a5");
    ok("Turkish king: takes from afar", N == 3 && all_take(1) && has(TK, "a1xa6") && has(TK, "a1xa8"));
    pos(TK, 0, "e7", "a1");
    k = dr_parse(M, N, TK, "e7-e8", &round);
    ok("Turkish: crowned on the far row", N == 3 && k >= 0 && M[k].crown);

    /* the draws: repetition, and the moves without a capture or a man moved */
    const char *steps = "b3-c3 g6-f6 c3-b3 f6-g6 b3-c3 g6-f6 c3-b3";
    ok("Turkish: men stepping sideways repeat", game(TK, 0, "b3", "g6", 0, steps) == DR_PLAY &&
       game(TK, 0, "b3", "g6", 0, "b3-c3 g6-f6 c3-b3 f6-g6 b3-c3 g6-f6 c3-b3 f6-g6") == DR_REPETITION);
    ok("Russian: 15 moves each with kings only", game(RU, 0, "Kc1", "Kh8", 29, "c1-d2") == DR_QUIET &&
       game(RU, 0, "Kc1", "Kh8", 28, "c1-d2") == DR_PLAY);
    ok("Brazilian: 20 moves each", game(BR, 0, "Kc1", "Kh8", 39, "c1-d2") == DR_QUIET &&
       game(BR, 0, "Kc1", "Kh8", 38, "c1-d2") == DR_PLAY);
    ok("Turkish: 25 moves each", game(TK, 0, "Ka1 c3", "Kh8", 49, "a1-b1") == DR_QUIET &&
       game(TK, 0, "Ka1 c3", "Kh8", 48, "a1-b1") == DR_PLAY && game(TK, 0, "Ka1 c3", "Kh8", 49, "c3-c4") == DR_PLAY);
    ok("Turkish: no piece left, lost", game(TK, 0, "d4", "d5", 0, "d4xd6") == DR_WON0);

    frisian();
    canadian();
    bashni();
    for (int v = FR; v <= BA; v++) random_games(v, 60);

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
