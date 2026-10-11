/*
 * Sea Battle: the rules' test (the fleets and where ships may lie, the paper fleet's ships apart, the placing and its
 * undo, the hand-offs, shots and salvos, sinking, the end; whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/seabattle.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sbgame G, H;
static sbpos *P = &G.pos;

static int squares(const sbpos *p, int pl)
{
    int n = 0;
    for (int c = 0; c < SB_CELLS; c++) n += sb_ship_at(p, p->ship[pl], c) >= 0;
    return n;
}

static void test_fleets(void)
{
    sb_new(&G, SB_CLASSIC, 7);
    sb_new(&H, SB_CLASSIC, 7);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));   /* the seed, the same fleets */
    CHECK(P->nships == 5 && P->len[0] == 5 && P->len[4] == 2 && !P->salvo && !P->apart);
    CHECK(P->phase == SP_PLACE && P->turn == 0);
    CHECK(sb_valid(P, P->ship[0]) && sb_valid(P, P->ship[1]) && squares(P, 0) == 17 && squares(P, 1) == 17);
    sb_new(&H, SB_CLASSIC, 8);
    CHECK(memcmp(H.pos.ship, G.pos.ship, sizeof G.pos.ship) != 0);
    sb_new(&G, SB_PAPER_SALVO, 3);
    CHECK(P->nships == 10 && P->apart && P->salvo && squares(P, 0) == 20 && sb_valid(P, P->ship[1]));
    for (int seed = 1; seed < 200; seed++) {    /* random fleets always valid, apart for the paper fleet */
        sb_new(&G, seed % SB_VARIANTS, (uint32_t)seed);
        CHECK(sb_valid(P, P->ship[0]) && sb_valid(P, P->ship[1]));
    }

    sb_new(&G, SB_CLASSIC, 1);
    sbship f[SB_MAXSHIPS] = {{0, 0, 5, 0}, {0, 2, 4, 0}, {0, 4, 3, 0}, {0, 6, 3, 0}, {0, 8, 2, 0}};
    CHECK(sb_valid(P, f));
    f[4] = (sbship){9, 9, 2, 0};                /* off the sea */
    CHECK(!sb_valid(P, f));
    f[4] = (sbship){9, 8, 2, 1};
    CHECK(sb_valid(P, f));
    f[4] = (sbship){4, 0, 2, 1};                /* across the 5 */
    CHECK(!sb_valid(P, f));
    f[4] = (sbship){5, 0, 2, 1};                /* touching it: allowed in the modern fleet */
    CHECK(sb_valid(P, f));
    f[4] = (sbship){5, 0, 3, 1};                /* not its length */
    CHECK(!sb_valid(P, f));
    sb_new(&G, SB_PAPER, 1);
    sbship g[SB_MAXSHIPS] = {{0, 0, 4, 0}, {0, 2, 3, 0}, {4, 2, 3, 0}, {0, 4, 2, 0}, {3, 4, 2, 0}, {6, 4, 2, 0},
                             {0, 6, 1, 0}, {2, 6, 1, 0}, {4, 6, 1, 0}, {6, 6, 1, 0}};
    CHECK(sb_valid(P, g));
    g[9] = (sbship){5, 7, 1, 0};                /* a corner touching the 1 at e7 */
    CHECK(!sb_valid(P, g));
    g[9] = (sbship){9, 9, 1, 0};
    CHECK(sb_valid(P, g));
    g[2] = (sbship){3, 2, 3, 0};                /* end to end with the 3 at a3 */
    CHECK(!sb_valid(P, g));
}

static void test_placing(void)
{
    sb_new(&G, SB_CLASSIC, 5);
    sbship before[SB_MAXSHIPS];
    memcpy(before, P->ship[0], sizeof before);
    CHECK(!sb_undo(&G));
    CHECK(sb_script(&G, "0=a1h 1=a3h 2=a5h 3=a7h 4=a9h"));
    CHECK(P->ship[0][0].x == 0 && P->ship[0][0].y == 0 && P->ship[0][4].y == 8 && !P->ship[0][4].vert);
    CHECK(sb_script(&G, "4=j9v") && P->ship[0][4].vert && P->ship[0][4].x == 9);
    CHECK(!sb_script(&G, "4=a1v"));             /* on the 5 */
    CHECK(sb_undo(&G) && !P->ship[0][4].vert);
    sbship moved[SB_MAXSHIPS];
    memcpy(moved, P->ship[0], sizeof moved);
    CHECK(sb_reroll(&G) && memcmp(moved, P->ship[0], sizeof moved) && sb_undo(&G));
    CHECK(!memcmp(moved, P->ship[0], sizeof moved));
    CHECK(sb_ready(&G) && P->turn == 1 && P->phase == SP_PLACE && !sb_undo(&G));
    CHECK(sb_script(&G, "r ok") && P->phase == SP_SHOOT && P->turn == 0);
    CHECK(!sb_reroll(&G) && !sb_script(&G, "ok"));
}

/* player 1's fleet known: shots, sinking, the end */
static void test_shooting(void)
{
    sb_new(&G, SB_CLASSIC, 9);
    CHECK(sb_script(&G, "ok 0=a1h 1=a3h 2=a5h 3=a7h 4=a9h ok"));
    CHECK(sb_shots(P) == 1);
    int two[2] = {0, 1};
    CHECK(!sb_fire(&G, two, 2));                /* one shot a turn */
    CHECK(sb_script(&G, "a9") && P->shot[1][80] == SHOT_HIT && P->turn == 1 && P->nlast[1] == 1);
    CHECK(!sb_undo(&G));                        /* fired: the device has passed */
    CHECK(sb_script(&G, "j10") && P->turn == 0);
    CHECK(!sb_script(&G, "a9"));                /* shot already */
    CHECK(!sb_sunk(P, 1, 4) && sb_script(&G, "b9") && sb_sunk(P, 1, 4) && sb_afloat(P, 1) == 4);
    CHECK(sb_script(&G, "j9 c9 j8") && P->shot[1][82] == SHOT_MISS);
    const char *rest = "a1 b1 c1 d1 e1 a3 b3 c3 d3 a5 b5 c5 a7 b7";
    for (const char *s = rest; *s;) {           /* player 0's shots, player 1 answering anywhere */
        char w[4] = {0};
        int k = 0;
        while (*s && *s != ' ') w[k++] = *s++;
        while (*s == ' ') s++;
        CHECK(sb_script(&G, w));
        if (P->phase == SP_OVER) break;
        CHECK(sb_script(&G, "*"));
    }
    CHECK(P->phase == SP_SHOOT && sb_afloat(P, 1) == 1);
    CHECK(sb_script(&G, "c7") && P->phase == SP_OVER && P->winner == 0 && sb_afloat(P, 1) == 0);
    CHECK(!sb_script(&G, "*") && !sb_undo(&G));
}

static void test_salvo(void)
{
    sb_new(&G, SB_SALVO, 4);
    CHECK(sb_script(&G, "ok 0=a1h 1=a3h 2=a5h 3=a7h 4=a9h ok"));
    CHECK(sb_shots(P) == 5);
    CHECK(!sb_script(&G, "a1+b1+c1+d1"));       /* five shots */
    CHECK(!sb_script(&G, "a1+b1+c1+d1+a1"));    /* five different squares */
    CHECK(sb_script(&G, "a9+b9+c9+d9+e9") && P->nlast[1] == 5 && sb_afloat(P, 1) == 4 && P->turn == 1);
    CHECK(sb_script(&G, "*") && P->nlast[0] == 4);   /* player 1 has four ships afloat */
    CHECK(sb_shots(P) == sb_afloat(P, 0));
    sb_new(&G, SB_PAPER_SALVO, 4);
    CHECK(sb_script(&G, "ok ok") && sb_shots(P) == 10);
}

/* whole games at random: the end always comes, every hit was a ship's square, a salvo is the ships afloat */
static void test_random_games(void)
{
    int ends = 0;
    for (int seed = 1; seed <= 200; seed++) {
        sb_new(&G, seed % SB_VARIANTS, (uint32_t)seed * 31u);
        CHECK(sb_script(&G, seed & 1 ? "r * r *" : "* *"));
        int turns = 0;
        while (P->phase == SP_SHOOT && turns < 400) {
            int me = P->turn, n = sb_shots(P);
            int open = 0;
            for (int c = 0; c < SB_CELLS; c++) open += P->shot[!me][c] == SHOT_NONE;
            int want = P->salvo ? sb_afloat(P, me) : 1;
            CHECK(n == (want < open ? want : open) && n >= 1);
            CHECK(sb_script(&G, "*"));
            int them = P->phase == SP_OVER ? !P->winner : me == 0 ? 1 : 0;
            for (int i = 0; i < P->nlast[them]; i++) {
                int c = P->last[them][i];
                CHECK((P->shot[them][c] == SHOT_HIT) == (sb_ship_at(P, P->ship[them], c) >= 0));
            }
            turns++;
        }
        CHECK(P->phase == SP_OVER && sb_afloat(P, !P->winner) == 0 && sb_afloat(P, P->winner) > 0);
        int hits = 0;
        for (int c = 0; c < SB_CELLS; c++) hits += P->shot[!P->winner][c] == SHOT_HIT;
        CHECK(hits == (P->apart ? 20 : 17));
        ends++;
    }
    CHECK(ends == 200);
    sb_new(&G, SB_PAPER_SALVO, 77);            /* "#": the whole game at random */
    CHECK(sb_script(&G, "#") && P->phase == SP_OVER && !sb_script(&G, "#a1"));
}

int main(void)
{
    test_fleets();
    test_placing();
    test_shooting();
    test_salvo();
    test_random_games();
    printf("seabattle: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
