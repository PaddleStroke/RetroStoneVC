/*
 * Jungle: the rules' test (the start, ranks and the rat and the elephant, the water, the jumps, the traps, the dens,
 * the ends, draws, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/jungle.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static jggame G;
static jgmove L[JG_MAX_MOVES];

/* a position: "0L g1 1E g7 0R b5" (player, animal letter RCDWPTLE, square), who moves */
static void setup(const char *animals, int side)
{
    static const char letters[] = "RCDWPTLE";
    jg_new(&G);
    jgpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    for (const char *t = animals; *t; t++) {
        if (*t == ' ') continue;
        int c = t[0] - '0', rank = (int)(strchr(letters, t[1]) - letters) + 1, s = jg_square(t + 3);
        p->sq[s] = (uint8_t)JG_PC(c, rank);
        p->left[c]++;
        t += 4;
    }
    p->side = (uint8_t)side;
}

static const jgpos *pos(void) { return jg_pos(&G); }
static int nlegal(void) { return jg_legal(pos(), L); }
static int at(const char *s) { return pos()->sq[jg_square(s)]; }
static int can(const char *from, const char *to)
{
    int n = nlegal(), a = jg_square(from), b = jg_square(to);
    for (int i = 0; i < n; i++)
        if (L[i].from == a && L[i].to == b) return 1;
    return 0;
}

/* the board and the start */
static void test_board(void)
{
    int water = 0, traps[2] = {0, 0};
    for (int s = 0; s < JG_SQUARES; s++) {
        water += jg_water(s);
        if (jg_trap(s) >= 0) traps[jg_trap(s)]++;
    }
    CHECK(water == 12 && jg_water(jg_square("b4")) && jg_water(jg_square("f6")) && !jg_water(jg_square("d5")));
    CHECK(traps[0] == 3 && traps[1] == 3 && jg_trap(jg_square("d2")) == 0 && jg_trap(jg_square("e9")) == 1);
    CHECK(jg_den(jg_square("d1")) == 0 && jg_den(jg_square("d9")) == 1 && jg_den(jg_square("d8")) < 0);
    jg_new(&G);
    CHECK(at("a3") == JG_PC(0, JG_ELEPHANT) && at("g1") == JG_PC(0, JG_LION) && at("g3") == JG_PC(0, JG_RAT));
    CHECK(at("g7") == JG_PC(1, JG_ELEPHANT) && at("a9") == JG_PC(1, JG_LION) && at("a7") == JG_PC(1, JG_RAT));
    CHECK(pos()->side == 0 && pos()->left[0] == 8 && pos()->left[1] == 8);
    /* the first moves: rat 3 (g4 f3 g2), elephant 3, wolf 3 and leopard 3 (not into the water), tiger 2, lion 2,
     * cat 4, dog 4 */
    CHECK(nlegal() == 3 + 3 + 3 + 3 + 2 + 2 + 4 + 4);
    CHECK(can("a3", "a4") && can("a3", "b3") && !can("c3", "c4") && !can("e3", "e4") && !can("a3", "b4"));
}

/* ranks: the higher or equal takes; the rat takes the elephant, the elephant never takes the rat */
static void test_ranks(void)
{
    setup("0T d4 1P d5 0W a4 1D a5 0C g4 1C g5", 0);
    CHECK(can("d4", "d5") && can("a4", "a5") && can("g4", "g5"));
    setup("0P d4 1T d5 0D a4 1W a5", 0);
    CHECK(!can("d4", "d5") && !can("a4", "a5"));
    setup("0R d4 1E d5 0E a4 1R a5", 0);
    CHECK(can("d4", "d5") && !can("a4", "a5"));
    CHECK(jg_script(&G, "d4d5") && at("d5") == JG_PC(0, JG_RAT) && pos()->left[1] == 1 && pos()->quiet == 0);
}

/* the water: only the rat; no capture between the water and the land, the rats meet in the water */
static void test_water(void)
{
    setup("0D a4 0R d4 1R c4 1E e7", 0);
    CHECK(!can("a4", "b4") && can("d4", "e4"));
    CHECK(!can("d4", "c4"));                    /* a rat on land does not take a rat in the water */
    setup("0R b5 1E b7 1R c5 0C a6 1C b3", 0);
    CHECK(can("b5", "c5"));                     /* rat takes rat, both in the water */
    CHECK(can("b5", "b6") && can("b5", "a5") && can("b5", "b4"));
    setup("0R b6 1E b7 1C a6", 0);
    CHECK(!can("b6", "b7") && !can("b6", "a6"));   /* from the water: not even the elephant, not even a cat */
    setup("0R a6 1R b6 0E g1", 1);
    CHECK(!can("b6", "a6"));                    /* the rat in the water does not take the rat on land */
}

/* lions and tigers jump across the rivers, unless a rat is in the way */
static void test_jumps(void)
{
    setup("0L b3 0T a5 1E g9", 0);
    CHECK(can("b3", "b7") && can("a5", "d5") && !can("b3", "b4") && !can("a5", "b5"));
    setup("0L d5 0P a5 1E g9", 0);
    CHECK(can("d5", "g5") && !can("d5", "a5") && !can("a5", "b5"));   /* landing on its own: no; leopard: no */
    setup("0L b3 0T a5 1R b6 1E g9", 0);
    CHECK(!can("b3", "b7") && can("a5", "d5"));  /* the rat on b6 blocks the lion's file, not the tiger's rank */
    setup("0L b3 0R c5 0T a5 1E g9", 0);
    CHECK(can("b3", "b7") && !can("a5", "d5")); /* its own rat blocks too */
    setup("0L b3 1T b7 0T a4 1L d4", 0);
    CHECK(can("b3", "b7") && !can("a4", "d4")); /* a jump captures (equal or lower rank) */
    CHECK(jg_script(&G, "b3b7") && at("b7") == JG_PC(0, JG_LION) && at("b3") == 0 && pos()->left[1] == 1);
}

/* traps: an animal in its enemy's trap has rank 0; in its own traps it keeps its rank */
static void test_traps(void)
{
    setup("1E d2 0C d3 0R a1", 0);
    CHECK(can("d3", "d2"));                     /* the cat takes the elephant in ivory's trap */
    setup("1L c1 0C b1 0E c2 1R e1 1E a9", 0);
    CHECK(can("b1", "c1") && can("c2", "c1")); /* the cat takes the lion in ivory's trap */
    setup("1R e1 0E e2 1E g9", 0);
    CHECK(can("e2", "e1"));                     /* the elephant takes a rat in its own side's trap */
    setup("0L c1 1C b1 1E g9", 1);
    CHECK(!can("b1", "c1"));                    /* in its own trap the lion keeps its rank */
    setup("1L c8 0T c9 0E a1", 0);
    CHECK(!can("c9", "c8"));                    /* the trapped tiger still attacks with its rank 6 */
    setup("0T c9 1C c8 0E a1", 1);
    CHECK(can("c8", "c9"));                     /* and ebony's cat takes it in ebony's trap */
}

/* the dens: never one's own; entering the enemy's wins */
static void test_dens(void)
{
    setup("0D d2 0R e1 1C d8 1R c9", 0);
    CHECK(!can("d2", "d1") && !can("e1", "d1"));
    G.pos[0].side = 1;
    CHECK(!can("d8", "d9") && !can("c9", "d9"));
    setup("0D d8 1C a5 1R a6", 0);
    CHECK(jg_status(&G) == JS_PLAY && jg_script(&G, "d8d9") && jg_status(&G) == JS_DEN && pos()->side == 1);
    CHECK(!jg_script(&G, "a5a4"));              /* the game is over */
}

/* the other ends: no animal left, no move */
static void test_ends(void)
{
    setup("0T d4 1C d5 0R a1", 0);
    CHECK(jg_script(&G, "d4d5") && pos()->left[1] == 0 && jg_status(&G) == JS_ALL);
    setup("1R a1 0D a2 0C b1 0R g9", 1);        /* the rat cornered by a dog and a cat */
    CHECK(nlegal() == 0 && jg_status(&G) == JS_STUCK);
    G.pos[0].side = 0;
    CHECK(jg_status(&G) == JS_PLAY);
}

/* draws: the same position the third time; 50 moves each without a capture */
static void test_draws(void)
{
    setup("0L a1 1L g9", 0);
    CHECK(jg_script(&G, "a1a2 g9g8 a2a1 g8g9") && jg_status(&G) == JS_PLAY);
    CHECK(jg_script(&G, "a1a2 g9g8 a2a1") && jg_status(&G) == JS_PLAY);
    CHECK(jg_script(&G, "g8g9") && jg_status(&G) == JS_REPETITION);
    CHECK(!jg_script(&G, "a1a2"));
    setup("0L a1 1L g9", 0);
    G.pos[0].quiet = JG_QUIET - 2;
    CHECK(jg_script(&G, "a1a2") && jg_status(&G) == JS_PLAY);
    CHECK(jg_script(&G, "g9g8") && jg_status(&G) == JS_QUIET);
}

/* undo: a move at a time, back to the start */
static void test_undo(void)
{
    jg_new(&G);
    jgpos start = *pos();
    CHECK(jg_script(&G, "g3g4 a7a6 g4g5 a6a5 g5f5 a5b5 a3a4 g7g6"));
    int ply = G.ply;
    CHECK(jg_undo(&G) && G.ply == ply - 1 && at("g7") == JG_PC(1, JG_ELEPHANT) && pos()->side == 1);
    CHECK(jg_script(&G, "b5a5 f5f4 a5a4"));    /* the rat leaves the water, takes the elephant */
    CHECK(at("a4") == JG_PC(1, JG_RAT) && pos()->left[0] == 7);
    CHECK(jg_undo(&G) && at("a4") == JG_PC(0, JG_ELEPHANT) && at("a5") == JG_PC(1, JG_RAT) && pos()->left[0] == 8);
    while (jg_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !jg_undo(&G));
    CHECK(!jg_script(&G, "a3b4") && !jg_script(&G, "a3a5") && !jg_script(&G, "a3"));
}

int main(void)
{
    test_board();
    test_ranks();
    test_water();
    test_jumps();
    test_traps();
    test_dens();
    test_ends();
    test_draws();
    test_undo();
    printf("jungle: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
