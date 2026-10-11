/*
 * Gomoku's rules test (src/games/gomoku_rules.c, no SDL): five in each direction, at the edges, the overline in both
 * variants, a full board's draw, undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/gomoku.h"

static gkgame G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return gk_point(name, &len);
}

static int fresh(int variant, const char *moves)
{
    gk_new(&G, variant);
    int ok = gk_script(&G, moves);
    if (!ok) printf("bad script: %s\n", moves);
    return ok;
}

static int lit(void)
{
    uint8_t line[GK_PTS];
    return gk_line(&G, line);
}

int main(void)
{
    /* notation: a..o (the i too), numbers from the bottom */
    CHECK(at("a1") == 0 && at("o15") == GK_PTS - 1 && at("h8") == 7 + 7 * GK_N && at("i1") == 8);
    CHECK(at("p1") < 0 && at("a16") < 0 && at("a0") < 0 && at("") < 0);

    /* five in each direction; four is not enough */
    fresh(GK_FREESTYLE, "d8 d1 e8 e1 f8 f1 g8 g1");
    CHECK(gk_status(&G) == GK_PLAY && lit() == 0);
    gk_play(&G, at("h8"));
    CHECK(gk_status(&G) == GK_WIN0 && lit() == 5);
    fresh(GK_FREESTYLE, "a1 h4 a2 h5 a3 h6 a4 h7 b9 h8");
    CHECK(gk_status(&G) == GK_WIN1 && lit() == 5);
    fresh(GK_FREESTYLE, "c3 a15 d4 b15 f6 c15 g7 d15 e5");
    CHECK(gk_status(&G) == GK_WIN0);
    fresh(GK_FREESTYLE, "k5 a1 j6 a2 i7 a3 h8 a4 g9");
    CHECK(gk_status(&G) == GK_WIN0);

    /* at the edges and in the corners */
    fresh(GK_STANDARD, "k15 a1 l15 a2 m15 a3 n15 a4 o15");
    CHECK(gk_status(&G) == GK_WIN0);
    fresh(GK_STANDARD, "o1 a3 o2 a5 o3 a7 o4 a9 o5");
    CHECK(gk_status(&G) == GK_WIN0);
    fresh(GK_STANDARD, "h8 a1 h9 b2 h10 c3 g9 d4 j10 e5");
    CHECK(gk_status(&G) == GK_WIN1 && lit() == 5);
    fresh(GK_STANDARD, "a15 o1 b14 n2 c13 m3 d12 l4 e11");
    CHECK(gk_status(&G) == GK_WIN0);

    /* the game is over: no more stones; a taken point is refused */
    CHECK(!gk_play(&G, at("h8")));
    fresh(GK_FREESTYLE, "h8");
    CHECK(!gk_play(&G, at("h8")) && !gk_play(&G, -1) && !gk_play(&G, GK_PTS) && G.nmoves == 1);

    /* six in a row (the last stone joins a four and a one): freestyle wins with all six, standard does not */
    const char *six = "c8 a1 d8 c1 e8 e1 f8 g1 h8 i1";
    fresh(GK_FREESTYLE, six);
    CHECK(gk_status(&G) == GK_PLAY);
    gk_play(&G, at("g8"));
    CHECK(gk_status(&G) == GK_WIN0 && lit() == 6);
    fresh(GK_STANDARD, six);
    gk_play(&G, at("g8"));
    CHECK(gk_status(&G) == GK_PLAY && lit() == 0 && gk_side(&G) == 1);
    /* standard: the overline stays, an exact five elsewhere still wins (here through the overline's own stone) */
    CHECK(gk_script(&G, "a15 g7 b15 g6 c15 g5 d15 g4"));
    CHECK(gk_status(&G) == GK_WIN0 && lit() == 5);
    /* standard: one stone makes an overline and a five at once: the five wins, only it lights */
    fresh(GK_STANDARD, "c8 a1 d8 a2 e8 a3 f8 a4 h8 a6 g4 b6 g5 c6 g6 d6 g7 e9");
    gk_play(&G, at("g8"));
    uint8_t line[GK_PTS];
    CHECK(gk_status(&G) == GK_WIN0 && gk_line(&G, line) == 5 && line[at("g4")] && !line[at("c8")]);

    /* a full board with no five: a draw (the pattern (x + 2y) mod 4 < 2 has no three alike in any line) */
    gk_new(&G, GK_STANDARD);
    int ebony[GK_PTS], ivory[GK_PTS], ne = 0, ni = 0;
    for (int p = 0; p < GK_PTS; p++) {
        int x = p % GK_N, y = p / GK_N;
        if ((x + 2 * y) % 4 < 2) ebony[ne++] = p;
        else ivory[ni++] = p;
    }
    CHECK(ne == 113 && ni == 112);
    int ok = 1;
    for (int k = 0; k < GK_PTS && ok; k++) {
        ok = gk_play(&G, k & 1 ? ivory[k / 2] : ebony[k / 2]);
        if (k < GK_PTS - 1 && gk_status(&G) != GK_PLAY) ok = 0;
    }
    CHECK(ok && G.nmoves == GK_PTS && gk_status(&G) == GK_DRAW);
    gk_new(&G, GK_FREESTYLE);
    for (int k = 0; k < GK_PTS && ok; k++) ok = gk_play(&G, k & 1 ? ivory[k / 2] : ebony[k / 2]);
    CHECK(ok && gk_status(&G) == GK_DRAW);

    /* undo: from the win back to play, then all the way to the empty board */
    fresh(GK_FREESTYLE, "d8 d1 e8 e1 f8 f1 g8 g1 h8");
    CHECK(gk_status(&G) == GK_WIN0);
    CHECK(gk_undo(&G) && gk_status(&G) == GK_PLAY && gk_side(&G) == 0 && G.pt[at("h8")] == GK_EMPTY);
    CHECK(gk_last(&G) == at("g1"));
    while (gk_undo(&G)) {}
    uint8_t empty[GK_PTS] = {0};
    CHECK(G.nmoves == 0 && !memcmp(G.pt, empty, GK_PTS) && gk_last(&G) == -1 && gk_side(&G) == 0);
    CHECK(!gk_undo(&G));

    /* bad scripts */
    gk_new(&G, GK_FREESTYLE);
    CHECK(!gk_script(&G, "h8 h8") && !gk_script(&G, "z1") && !gk_script(&G, "h16"));

    printf("gomoku: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
