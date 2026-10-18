/*
 * Corridor: the rules' test (the start, steps, jumps and side steps, walls that fit or overlap or cross, the path that
 * must stay open, the end, the pass, the notation, undo, whole random games). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/corridor.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static crgame G;

static crpos *pos(void) { return &G.pos[G.ply]; }
static int sq(const char *s) { return cr_square(s); }

static int move_of(const char *s)
{
    int len;
    return cr_parse(pos(), s, &len);
}

static int legal(const char *s) { return cr_legal(pos(), move_of(s)); }

/* the pawn's targets as a sorted list of names: "d1 e2 f1" */
static const char *targets(void)
{
    static char out[64];
    uint8_t to[8];
    int n = cr_pawn_moves(pos(), to);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && to[j - 1] > to[j]; j--) {
            uint8_t t = to[j];
            to[j] = to[j - 1];
            to[j - 1] = t;
        }
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        char nm[4];
        cr_name(to[i], nm);
        if (i) strcat(out, " ");
        strcat(out, nm);
    }
    return out;
}

/* a hand-made position: the pawns ("e5 e6"), the walls ("e6h d5v"), who moves */
static void setup(int players, const char *pawns, const char *walls, int turn)
{
    cr_new(&G, players);
    crpos *p = pos();
    int i = 0;
    for (const char *s = pawns; *s && i < players;) {
        p->pawn[i++] = (uint8_t)cr_square(s);
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    for (const char *s = walls; *s;) {
        int len, m = cr_parse(p, s, &len);
        if (CR_IS_WALL(m)) p->wall[CR_SLOT(m)] = (uint8_t)CR_ORIENT(m);
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    p->turn = (uint8_t)turn;
}

static void test_start(void)
{
    cr_new(&G, 2);
    CHECK(pos()->pawn[0] == sq("e1") && pos()->pawn[1] == sq("e9") && pos()->left[0] == 10 && pos()->left[1] == 10);
    CHECK(pos()->turn == 0 && pos()->winner == -1);
    CHECK(!strcmp(targets(), "d1 f1 e2"));
    CHECK(cr_dist(pos(), 0) == 8 && cr_dist(pos(), 1) == 8);
    int mv[CR_MAX_MOVES];
    CHECK(cr_moves(pos(), mv) == 3 + 128);    /* every wall fits on the empty board */
    cr_new(&G, 4);
    CHECK(pos()->pawn[1] == sq("a5") && pos()->pawn[2] == sq("e9") && pos()->pawn[3] == sq("i5"));
    CHECK(pos()->left[0] == 5 && pos()->left[3] == 5);
    CHECK(cr_goal(pos(), 1, sq("i3")) && !cr_goal(pos(), 1, sq("a3")) && cr_goal(pos(), 3, sq("a7")));
    CHECK(cr_goal(pos(), 2, sq("c1")) && cr_goal(pos(), 0, sq("c9")));
}

/* walls: in the grooves, two squares long; no overlap, no crossing */
static void test_walls(void)
{
    cr_new(&G, 2);
    CHECK(cr_script(&G, "e1h"));               /* between ranks 1 and 2, files e and f */
    CHECK(cr_blocked(pos(), sq("e1"), CR_UP) && cr_blocked(pos(), sq("f1"), CR_UP));
    CHECK(cr_blocked(pos(), sq("e2"), CR_DOWN) && !cr_blocked(pos(), sq("d1"), CR_UP) && !cr_blocked(pos(), sq("g1"), CR_UP));
    CHECK(pos()->left[0] == 9 && pos()->turn == 1);
    CHECK(!legal("d1h") && !legal("f1h") && !legal("e1h") && !legal("e1v"));   /* overlapping, crossing */
    CHECK(legal("c1h") && legal("g1h") && legal("d1v") && legal("f1v") && legal("e2v"));
    CHECK(cr_script(&G, "c5v"));               /* between files c and d, ranks 5 and 6 */
    CHECK(cr_blocked(pos(), sq("c5"), CR_RIGHT) && cr_blocked(pos(), sq("d6"), CR_LEFT));
    CHECK(!cr_blocked(pos(), sq("c7"), CR_RIGHT) && !cr_blocked(pos(), sq("c4"), CR_RIGHT));
    CHECK(!legal("c4v") && !legal("c6v") && !legal("c5h") && legal("c3v") && legal("c7v") && legal("b5h"));
    CHECK(!legal("i1h") && move_of("i1h") < 0 && move_of("a9v") < 0);   /* no slot past the eighth file / rank */
    CHECK(cr_dist(pos(), 0) == 9);             /* round the wall in front */
    /* the walls run out */
    cr_new(&G, 2);
    pos()->left[0] = 0;
    CHECK(!legal("a3h") && legal("e2"));
}

/* the path rule: a wall may never shut a pawn off from its goal, its own or another's */
static void test_path(void)
{
    setup(2, "e1 e9", "a1h c1h e1h g1h", 0);
    CHECK(cr_dist(pos(), 0) == 12);            /* along rank 1, up the i file */
    CHECK(!legal("h1v"));                      /* it would seal rank 1 */
    CHECK(legal("h2v") && !legal("h1h"));      /* h1h overlaps g1h */
    pos()->turn = 1;                           /* nor may the other player seal it */
    CHECK(!legal("h1v") && legal("h3v"));
    /* four players: walling in a third player is just as forbidden */
    setup(4, "e1 a5 e9 i5", "a5h a4v", 0);     /* a5 and a4 open only below */
    CHECK(cr_dist(pos(), 1) == 10);
    CHECK(!legal("a3h"));                      /* would seal them */
    CHECK(legal("a2h") && legal("b3v"));
}

/* steps, jumps, side steps */
static void test_pawns(void)
{
    setup(2, "e5 e6", "", 0);
    CHECK(!strcmp(targets(), "e4 d5 f5 e7"));  /* straight over */
    setup(2, "e5 e6", "e6h", 0);               /* a wall behind: beside it */
    CHECK(!strcmp(targets(), "e4 d5 f5 d6 f6"));
    setup(2, "e5 e6", "e6h d5v", 0);           /* and a wall on one side of it (and of e5) */
    CHECK(!strcmp(targets(), "e4 f5 f6"));
    setup(2, "e8 e9", "", 0);                  /* the edge behind it */
    CHECK(!strcmp(targets(), "e7 d8 f8 d9 f9"));
    CHECK(cr_play(&G, sq("d9")) && pos()->winner == 0);   /* home: the game is won */
    CHECK(!legal("e1") && !legal("a3h"));
    setup(2, "e5 e6", "e5h", 0);               /* a wall between them: no jump at all */
    CHECK(!strcmp(targets(), "e4 d5 f5"));
    setup(2, "a5 a6", "", 1);                  /* down over a pawn, the other's turn */
    CHECK(!strcmp(targets(), "a4 b6 a7"));
    CHECK(legal("a4") && !legal("a5") && !legal("b5"));
    setup(2, "a5 a6", "a4h", 1);               /* behind it a wall, at the edge: only the side step to b5 */
    CHECK(!strcmp(targets(), "b5 b6 a7"));
    /* four players: a pawn behind blocks the jump as a wall does; never over two */
    setup(4, "e5 e6 e7 a1", "", 0);
    CHECK(!strcmp(targets(), "e4 d5 f5 d6 f6"));
    setup(4, "e5 e6 e7 d6", "", 0);            /* and one beside it: the other side only */
    CHECK(!strcmp(targets(), "e4 d5 f5 f6"));
}

/* the pass: no step and no wall (here a hand-made box), and only then */
static void test_pass(void)
{
    setup(4, "a1 a2 b1 a3", "a1h b1v", 0);
    pos()->left[0] = 0;
    int mv[CR_MAX_MOVES];
    CHECK(cr_pawn_moves(pos(), (uint8_t *)mv) == 0);
    CHECK(cr_moves(pos(), mv) == 1 && mv[0] == CR_PASS && legal("-"));
    CHECK(cr_play(&G, CR_PASS) && pos()->turn == 1);
    cr_new(&G, 2);
    CHECK(!legal("-"));
}

/* the notation, undo, the turns' limit */
static void test_misc(void)
{
    char nm[4];
    for (int m = 0; m < CR_CELLS; m++) {
        cr_name(m, nm);
        int len;
        CHECK(cr_parse(pos(), nm, &len) == m && len == 2);
    }
    for (int s = 0; s < CR_SLOTS; s++)
        for (int o = CR_H; o <= CR_V; o++) {
            int len, m = CR_WALL(s, o);
            cr_name(m, nm);
            CHECK(cr_parse(pos(), nm, &len) == m && len == 3);
        }
    cr_new(&G, 2);
    CHECK(cr_script(&G, "e2 e8 e3 e7 e3h"));
    CHECK(G.ply == 5 && pos()->left[0] == 9 && pos()->turn == 1);
    CHECK(!cr_script(&G, "e4"));               /* e7 to e4: no */
    CHECK(cr_undo(&G) && G.ply == 4 && pos()->left[0] == 10 && pos()->turn == 0);
    while (cr_undo(&G)) {}
    CHECK(G.ply == 0 && pos()->pawn[0] == sq("e1"));
    /* nobody home after a hundred turns each: a draw */
    cr_new(&G, 2);
    pos()->turns = 2 * CR_TURNS - 1;
    pos()->turn = 1;
    CHECK(cr_script(&G, "e8") && pos()->winner == CR_DRAW && !legal("e2"));
}

/* whole random games: every move legal, every pawn always with a way home, the walls counted, an end */
static void test_games(void)
{
    srand(7);
    int ends = 0, wins = 0;
    for (int game = 0; game < 300; game++) {
        int players = game % 3 ? 2 : 4;
        cr_new(&G, players);
        while (pos()->winner < 0) {
            int mv[CR_MAX_MOVES], n = cr_moves(pos(), mv);
            int m = mv[rand() % n];
            if (rand() % 3 && pos()->left[pos()->turn]) {   /* lean to steps forward so the games end */
                uint8_t path[CR_CELLS];
                if (cr_path(pos(), pos()->turn, path) > 0 && cr_legal(pos(), path[1])) m = path[1];
            } else if (rand() % 2) {
                uint8_t path[CR_CELLS];
                if (cr_path(pos(), pos()->turn, path) > 0 && cr_legal(pos(), path[1])) m = path[1];
            }
            int before = 0, after = 0;
            for (int i = 0; i < players; i++) before += pos()->left[i];
            if (!cr_play(&G, m)) { CHECK(0); break; }
            for (int i = 0; i < players; i++) {
                after += pos()->left[i];
                CHECK(cr_dist(pos(), i) != CR_FAR);
            }
            int walls = 0;
            for (int s = 0; s < CR_SLOTS; s++) walls += pos()->wall[s] != CR_NONE;
            CHECK(after == before - CR_IS_WALL(m) && walls + after == players * (players == 2 ? 10 : 5));
        }
        ends += pos()->winner >= 0;
        wins += pos()->winner >= 0 && pos()->winner < players;
        if (pos()->winner < players) CHECK(cr_goal(pos(), pos()->winner, pos()->pawn[pos()->winner]));
    }
    CHECK(ends == 300 && wins > 250);
}

int main(void)
{
    test_start();
    test_walls();
    test_path();
    test_pawns();
    test_pass();
    test_misc();
    test_games();
    printf("corridor: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
