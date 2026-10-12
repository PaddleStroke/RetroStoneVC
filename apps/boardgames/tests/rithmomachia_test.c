/*
 * Rithmomachia: the rules' test (the layout and its numbers, the moves by shape, the four captures, the harmonies, the
 * victories, the draw, undo; whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/rithmomachia.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static rhmgame G;

static int sq(const char *s) { return rhm_square(s, NULL); }

/* a position: "id@c4 id@d5" pieces by their id (rhm_piece), the side to move, the variant */
static void setup(const char *list, int side, int variant)
{
    rhm_new(&G, variant);
    rhmpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    for (const char *t = list; *t;) {
        if (*t == ' ') { t++; continue; }
        int id = 0, len;
        while (*t >= '0' && *t <= '9') id = id * 10 + *t++ - '0';
        if (*t++ != '@') { printf("bad setup\n"); failed++; return; }
        int s = rhm_square(t, &len);
        if (s < 0) { printf("bad square %s\n", t); failed++; return; }
        p->sq[s] = (uint8_t)(id + 1);
        t += len;
    }
    p->side = (uint8_t)side;
}

static int can(const char *a, const char *b)
{
    rhmmove m[RHM_MAX_MOVES];
    int n = rhm_legal(rhm_pos(&G), m);
    for (int i = 0; i < n; i++)
        if (m[i].from == sq(a) && m[i].to == sq(b)) return 1;
    return 0;
}

static int id_at(const char *s) { return rhm_pos(&G)->sq[sq(s)] - 1; }

/* the numbers: Boethius's families from the bases 2 4 6 8 and 3 5 7 9 */
static void test_layout(void)
{
    for (int side = 0; side < 2; side++) {
        const rhmpiece *k = &rhm_piece[24 * side];
        int shapes[5] = {0};
        for (int i = 0; i < 24; i++) {
            CHECK(k[i].owner == side);
            shapes[k[i].shape]++;
        }
        CHECK(shapes[RHM_ROUND] == 8 && shapes[RHM_TRIANGLE] == 8 && shapes[RHM_SQUARE] == 7 &&
              shapes[RHM_PYRAMID] == 1);
        for (int b = 0; b < 4; b++) {
            int a = side ? 3 + 2 * b : 2 + 2 * b;
            CHECK(k[b].value == a && k[4 + b].value == a * a);
            CHECK(k[8 + 2 * b].value == a + a * a && k[9 + 2 * b].value == (a + 1) * (a + 1));
        }
        int layers = 0;
        for (int i = 0; i < 6; i++) layers += rhm_layers[side][i];
        CHECK(k[23].shape == RHM_PYRAMID && k[23].value == layers && layers == (side ? 190 : 91));
    }
    /* the squares: the two triangles' sum, then (2a + 1) squared */
    static const int sq1[2][4] = {{15, 45, 91, 153}, {28, 66, 120, 190}}, sq2[2][4] = {{25, 81, 169, 289},
                                                                                       {49, 121, 225, 361}};
    for (int side = 0; side < 2; side++)
        for (int b = 0; b < 4; b++) {
            int a = side ? 3 + 2 * b : 2 + 2 * b;
            CHECK(sq1[side][b] == a + a * a + (a + 1) * (a + 1) && sq2[side][b] == (2 * a + 1) * (2 * a + 1));
            int found1 = 0, found2 = 0;
            for (int i = 16; i < 24; i++) {
                found1 |= rhm_piece[24 * side + i].value == sq1[side][b];
                found2 |= rhm_piece[24 * side + i].value == sq2[side][b];
            }
            CHECK(found1 && found2);
        }
    rhm_new(&G, 0);
    const rhmpos *p = rhm_pos(&G);
    int n[2] = {0}, threats = 0;
    for (int s = 0; s < RHM_N; s++)
        if (p->sq[s]) {
            int o = rhm_piece[p->sq[s] - 1].owner;
            n[o]++;
            CHECK(o == 0 ? s / RHM_W < 4 : s / RHM_W >= 12);
            threats += rhm_threat(p, !o, s, NULL) != 0;
        }
    CHECK(n[0] == 24 && n[1] == 24 && threats == 0);
    CHECK(id_at("h1") == 17 && id_at("a1") == 22 && id_at("b2") == 23);   /* 25, 289, the pyramid 91 */
    CHECK(id_at("h16") == 46 && id_at("h15") == 47 && id_at("c13") == 24);   /* 361, the pyramid 190, 3 */
    rhmmove m[RHM_MAX_MOVES];
    CHECK(rhm_legal(p, m) > 0 && rhm_status(&G) == RHM_PLAY && p->side == 0);
}

static void test_moves(void)
{
    setup("0@d4 8@d6 16@a1 23@e10 24@h16", 0, 0);   /* round 2, triangle 6, square 15, the pyramid */
    CHECK(can("d4", "c5") && can("d4", "e3") && !can("d4", "d5") && !can("d4", "b6"));
    CHECK(can("d6", "d8") && can("d6", "f6") && !can("d6", "d7") && !can("d6", "e7") && !can("d6", "d9"));
    CHECK(can("a1", "a4") && can("a1", "d1") && !can("a1", "a3") && !can("a1", "b2"));
    CHECK(can("e10", "d11") && can("e10", "e12") && can("e10", "e13") && can("e10", "b10") && !can("e10", "e11"));
    setup("16@a1 0@a2 8@d6 1@d5", 0, 0);        /* never over a piece */
    CHECK(!can("a1", "a4") && can("a1", "d1"));
    CHECK(!can("d6", "d4") && can("d6", "d8"));
    CHECK(rhm_script(&G, "a1d1") && id_at("d1") == 16 && rhm_pos(&G)->side == 1);
    CHECK(!rhm_script(&G, "d1d4"));             /* not the mover's */
}

static void test_captures(void)
{
    int att[2];
    /* meeting: the round 16 could step onto the triangle 16 */
    setup("5@d5 33@e6 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("e6"), att) == RHM_MEETING && att[0] == sq("d5"));
    CHECK(rhm_threat(rhm_pos(&G), 1, sq("d5"), NULL) == 0);   /* the triangle cannot step diagonally */
    /* assault: the square 15 with two empty squares to the triangle 30 */
    setup("16@a1 34@a4 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("a4"), att) == RHM_ASSAULT && att[0] == sq("a1"));
    setup("16@a1 34@a5 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("a5"), NULL) == 0);   /* three between: 45 */
    setup("1@a1 32@e5 24@h16", 0, 0);           /* the round 4 along its diagonal, three between: 12 */
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("e5"), NULL) == RHM_ASSAULT);
    setup("1@a1 32@a5 24@h16", 0, 0);           /* not along a column: a round's lines are its diagonals */
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("a5"), NULL) == 0);
    setup("1@a1 0@c3 32@e5 24@h16", 0, 0);      /* a piece between */
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("e5"), NULL) == 0);
    /* ambush: the round 16 and the triangle 9, both able to move onto the round 25 */
    setup("5@c4 9@d3 29@d5 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("d5"), att) == RHM_AMBUSH);
    CHECK((att[0] == sq("c4") && att[1] == sq("d3")) || (att[0] == sq("d3") && att[1] == sq("c4")));
    setup("5@c4 9@d2 29@d5 24@h16", 0, 0);      /* the triangle cannot reach it */
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("d5"), NULL) == 0);
    /* siege: four sides, the edge closing one */
    setup("26@d8 0@d7 1@d9 2@c8 3@e8 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("d8"), NULL) == RHM_SIEGE);
    setup("26@d8 0@d7 1@d9 2@c8 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("d8"), NULL) == 0);
    setup("26@a8 0@a7 1@a9 2@b8 24@h16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("a8"), NULL) == RHM_SIEGE);
    /* the pyramid: one piece of its whole number: 25 + 66 = 91 */
    setup("23@d5 29@c6 42@d8 6@h1", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 1, sq("d5"), NULL) == RHM_AMBUSH);
    setup("23@d5 31@c6 47@e6 6@h1", 0, 0);      /* 81 + 190: no */
    CHECK(rhm_threat(rhm_pos(&G), 1, sq("d5"), NULL) == 0);
    /* one move, two captures at once: the round 36 meets the triangle 36 and closes the siege of h8 */
    setup("6@f7 35@f9 26@h8 0@h7 1@h9 24@a16", 0, 0);
    CHECK(rhm_threat(rhm_pos(&G), 0, sq("f9"), NULL) == 0 && rhm_threat(rhm_pos(&G), 0, sq("h8"), NULL) == 0);
    CHECK(rhm_script(&G, "f7g8"));
    const rhmpos *p = rhm_pos(&G);
    CHECK(p->sq[sq("f9")] == 0 && p->sq[sq("h8")] == 0 && p->taken[0] == 2 && p->sq[sq("g8")] == 6 + 1);
}

static void test_capture_move(void)
{
    /* the white round 16 steps next to the black triangle 16: meeting; and a siege closed by the same move */
    setup("5@c4 33@e6 26@h8 0@h7 1@h9 24@a16", 0, 0);
    CHECK(rhm_script(&G, "c4d5"));
    const rhmpos *p = rhm_pos(&G);
    CHECK(p->sq[sq("e6")] == 0 && p->sq[sq("h8")] == 26 + 1);   /* h8 still has g8 open */
    CHECK(p->taken[0] == 1 && p->quiet == 0 && p->sq[sq("d5")] == 5 + 1);
    setup("5@c4 33@e6 26@h8 0@h7 1@h9 2@f7 24@a16", 0, 0);
    CHECK(rhm_script(&G, "f7g8"));              /* closes the siege of h8; d5 stays empty: no meeting yet */
    p = rhm_pos(&G);
    CHECK(p->sq[sq("h8")] == 0 && p->sq[sq("e6")] == 33 + 1 && p->taken[0] == 1);
    CHECK(rhm_undo(&G) && rhm_pos(&G)->sq[sq("h8")] == 26 + 1);
}

static void test_harmony(void)
{
    CHECK(rhm_harmonic(4, 6, 8) && rhm_harmonic(8, 6, 4) && rhm_harmonic(4, 16, 64) && rhm_harmonic(3, 4, 6));
    CHECK(rhm_harmonic(2, 4, 6) && !rhm_harmonic(6, 4, 8) && !rhm_harmonic(4, 4, 4) && !rhm_harmonic(2, 5, 9));
    CHECK(rhm_harmonic(6, 8, 12) && rhm_harmonic(16, 36, 81) && !rhm_harmonic(16, 36, 80));
    uint8_t h[3];
    setup("1@c10 2@d10 3@e10 24@h1", 0, RHM_PROPER);   /* 4 6 8 in a row, in the odds' half */
    CHECK(rhm_find_harmony(rhm_pos(&G), 0, h) && h[1] == sq("d10"));
    setup("1@c10 2@d10 3@f10 24@h1", 0, RHM_PROPER);   /* not equally spaced */
    CHECK(!rhm_find_harmony(rhm_pos(&G), 0, h));
    setup("1@c10 2@e10 3@g10 24@h1", 0, RHM_PROPER);   /* spaced by two: fine */
    CHECK(rhm_find_harmony(rhm_pos(&G), 0, h));
    setup("1@c8 2@d8 3@e8 24@h1", 0, RHM_PROPER);      /* rank 8: their own half */
    CHECK(!rhm_find_harmony(rhm_pos(&G), 0, h));
    setup("1@c9 5@d10 7@e11 24@h1", 0, RHM_PROPER);    /* 4 16 64 on a diagonal */
    CHECK(rhm_find_harmony(rhm_pos(&G), 0, h));
    setup("2@c10 1@d10 3@e10 24@h1", 0, RHM_PROPER);   /* 6 4 8: not in order */
    CHECK(!rhm_find_harmony(rhm_pos(&G), 0, h));
    /* the move that makes it wins; in the common victory it is nothing */
    setup("1@c10 2@d10 3@f9 24@h1", 0, RHM_PROPER);
    CHECK(rhm_script(&G, "f9e10") && rhm_status(&G) == RHM_WIN0 && rhm_pos(&G)->proper);
    setup("1@c10 2@d10 3@f9 24@h1", 0, RHM_COMMON8);
    CHECK(rhm_script(&G, "f9e10") && rhm_status(&G) == RHM_PLAY);
    setup("24@c5 25@d5 26@f4 0@h16", 1, RHM_PROPER);   /* the odds: 3 5 7 in the evens' half */
    CHECK(rhm_script(&G, "f4e5") && rhm_status(&G) == RHM_WIN1);
}

static void test_end(void)
{
    setup("5@c4 33@e6 24@a16", 0, RHM_COMMON8);
    G.pos[0].taken[0] = 7;
    CHECK(rhm_script(&G, "c4d5") && rhm_status(&G) == RHM_WIN0);
    setup("5@c4 33@e6 24@a16", 0, RHM_COMMON12);
    G.pos[0].taken[0] = 7;
    CHECK(rhm_script(&G, "c4d5") && rhm_status(&G) == RHM_PLAY);
    setup("24@a16 1@b15 0@h1", 1, 0);           /* the odds' round cannot move: they lose */
    CHECK(rhm_status(&G) == RHM_WIN0);
    setup("0@a1 24@h16", 0, 0);
    const char *walk[4] = {"a1b2", "h16g15", "b2a1", "g15h16"};
    int i = 0;
    while (rhm_status(&G) == RHM_PLAY && i < 300) CHECK(rhm_script(&G, walk[i++ % 4]));
    CHECK(i == RHM_QUIET && rhm_status(&G) == RHM_DRAW);
}

static uint32_t rnd;
static int next(int n)
{
    rnd = rnd * 1103515245u + 12345u;
    return (int)((rnd >> 16) % (uint32_t)n);
}

static void test_random(void)
{
    int ends[RHM_VARIANTS][4] = {{0}}, plies = 0, games = 0;
    for (int v = 0; v < RHM_VARIANTS; v++)
        for (int game = 0; game < 60; game++) {
            rnd = (uint32_t)(game * 977 + v * 131 + 5);
            rhm_new(&G, v);
            rhmpos start = *rhm_pos(&G);
            while (rhm_status(&G) == RHM_PLAY) {
                const rhmpos *p = rhm_pos(&G);
                rhmmove m[RHM_MAX_MOVES];
                int n = rhm_legal(p, m), k = next(n), side = p->side;
                CHECK(n > 0);
                /* forwards more often than not, and a capture when one is there: games that end */
                for (int tries = 0; tries < 3; tries++) {
                    int j = next(n), dy = m[j].to / RHM_W - m[j].from / RHM_W;
                    if (side ? dy < 0 : dy > 0) { k = j; break; }
                }
                for (int i = 0; i < n; i++) {
                    rhmpos q = *p;
                    rhm_make(&q, m[i], v);
                    if (q.taken[side] > p->taken[side] && next(3)) { k = i; break; }
                }
                int pieces = 0, before = p->taken[side];
                CHECK(rhm_play(&G, m[k]));
                p = rhm_pos(&G);
                for (int s = 0; s < RHM_N; s++) pieces += p->sq[s] != 0;
                CHECK(pieces == 48 - p->taken[0] - p->taken[1]);
                CHECK(p->taken[side] >= before && p->taken[!side] == rhm_pos(&G)[-1].taken[!side]);
            }
            ends[v][rhm_status(&G)]++;
            plies += G.ply;
            games++;
            while (rhm_undo(&G)) {}
            CHECK(memcmp(rhm_pos(&G), &start, sizeof start) == 0);
        }
    for (int v = 0; v < RHM_VARIANTS; v++)
        printf("variant %d: evens %d, odds %d, draws %d\n", v, ends[v][RHM_WIN0], ends[v][RHM_WIN1],
               ends[v][RHM_DRAW]);
    printf("%d moves a game on average\n", plies / games);
    CHECK(ends[0][RHM_WIN0] + ends[0][RHM_WIN1] > 0);
}

int main(void)
{
    test_layout();
    test_moves();
    test_captures();
    test_capture_move();
    test_harmony();
    test_end();
    test_random();
    printf("rithmomachia: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
