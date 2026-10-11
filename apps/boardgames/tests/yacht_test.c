/*
 * Yacht: the rules' test (every category of every variant and its points, the upper bonuses, classic's extra five of
 * a kind and its joker, generala's served hands, its double and its instant win, the throws and the dice kept, the
 * turn round the table, the end and the winner, one to six players, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/yacht.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ygame G;
static const ypos *P = &G.pos;

/* the points of five dice ("12345") in a category of a variant */
static int base(int variant, int cat, const char *s)
{
    uint8_t d[YA_DICE];
    for (int i = 0; i < YA_DICE; i++) d[i] = (uint8_t)(s[i] - '0');
    return ya_base(variant, cat, d);
}

static void test_classic_categories(void)
{
    const int V = YA_CLASSIC;
    int cats[YA_MAX_CATS];
    CHECK(ya_cats(V, cats) == 13 && cats[0] == YC_ONES && cats[6] == YC_THREE_KIND && cats[12] == YC_CHANCE);
    CHECK(base(V, YC_ONES, "11234") == 2 && base(V, YC_TWOS, "22226") == 8 && base(V, YC_THREES, "12456") == 0);
    CHECK(base(V, YC_FOURS, "44441") == 16 && base(V, YC_FIVES, "55555") == 25 && base(V, YC_SIXES, "61626") == 18);
    CHECK(base(V, YC_THREE_KIND, "33325") == 16 && base(V, YC_THREE_KIND, "33225") == 0);
    CHECK(base(V, YC_THREE_KIND, "66666") == 30);
    CHECK(base(V, YC_FOUR_KIND, "25555") == 22 && base(V, YC_FOUR_KIND, "25553") == 0);
    CHECK(base(V, YC_FULL, "22333") == 25 && base(V, YC_FULL, "22334") == 0 && base(V, YC_FULL, "44444") == 0);
    CHECK(base(V, YC_SMALL, "12346") == 30 && base(V, YC_SMALL, "63451") == 30 && base(V, YC_SMALL, "34566") == 30);
    CHECK(base(V, YC_SMALL, "12456") == 0 && base(V, YC_SMALL, "12345") == 30);
    CHECK(base(V, YC_LARGE, "12345") == 40 && base(V, YC_LARGE, "65432") == 40 && base(V, YC_LARGE, "12346") == 0);
    CHECK(base(V, YC_YACHT, "33333") == 50 && base(V, YC_YACHT, "33332") == 0);
    CHECK(base(V, YC_CHANCE, "12346") == 16 && base(V, YC_CHANCE, "66666") == 30);
    CHECK(ya_bonus_at(V) == 35);
}

static void test_nordic_categories(void)
{
    const int V = YA_NORDIC;
    int cats[YA_MAX_CATS];
    CHECK(ya_cats(V, cats) == 15 && cats[6] == YC_PAIR && cats[7] == YC_TWO_PAIRS && cats[14] == YC_YACHT);
    CHECK(base(V, YC_PAIR, "33456") == 6 && base(V, YC_PAIR, "33556") == 10 && base(V, YC_PAIR, "12345") == 0);
    CHECK(base(V, YC_PAIR, "66662") == 12);
    CHECK(base(V, YC_TWO_PAIRS, "33556") == 16 && base(V, YC_TWO_PAIRS, "22266") == 16);
    CHECK(base(V, YC_TWO_PAIRS, "44442") == 0 && base(V, YC_TWO_PAIRS, "55555") == 0);
    CHECK(base(V, YC_TWO_PAIRS, "11234") == 0);
    CHECK(base(V, YC_THREE_KIND, "44412") == 12 && base(V, YC_THREE_KIND, "55555") == 15);
    CHECK(base(V, YC_THREE_KIND, "44112") == 0);
    CHECK(base(V, YC_FOUR_KIND, "22223") == 8 && base(V, YC_FOUR_KIND, "66666") == 24);
    CHECK(base(V, YC_FOUR_KIND, "22233") == 0);
    CHECK(base(V, YC_SMALL, "12345") == 15 && base(V, YC_SMALL, "54321") == 15 && base(V, YC_SMALL, "23456") == 0);
    CHECK(base(V, YC_SMALL, "12346") == 0);
    CHECK(base(V, YC_LARGE, "23456") == 20 && base(V, YC_LARGE, "12345") == 0);
    CHECK(base(V, YC_FULL, "22333") == 13 && base(V, YC_FULL, "66555") == 27 && base(V, YC_FULL, "33333") == 0);
    CHECK(base(V, YC_CHANCE, "66543") == 24 && base(V, YC_YACHT, "11111") == 50 && base(V, YC_YACHT, "11112") == 0);
    CHECK(ya_bonus_at(V) == 50);
}

static void test_generala_categories(void)
{
    const int V = YA_GENERALA;
    int cats[YA_MAX_CATS];
    CHECK(ya_cats(V, cats) == 11 && cats[6] == YC_STRAIGHT && cats[9] == YC_YACHT && cats[10] == YC_DOUBLE);
    CHECK(base(V, YC_SIXES, "66612") == 18);
    CHECK(base(V, YC_STRAIGHT, "12345") == 20 && base(V, YC_STRAIGHT, "23456") == 20);
    CHECK(base(V, YC_STRAIGHT, "13456") == 20);
    CHECK(base(V, YC_STRAIGHT, "12346") == 0 && base(V, YC_STRAIGHT, "12456") == 0);
    CHECK(base(V, YC_FULL, "11666") == 30 && base(V, YC_FULL, "66666") == 0);
    CHECK(base(V, YC_FOUR_KIND, "33331") == 40 && base(V, YC_FOUR_KIND, "33333") == 40);
    CHECK(base(V, YC_FOUR_KIND, "33311") == 0);
    CHECK(base(V, YC_YACHT, "22222") == 50 && base(V, YC_DOUBLE, "22222") == 100);
    CHECK(ya_bonus_at(V) == 0);
}

/* the throws in a turn, the dice kept, the score, the next player */
static void test_turn(void)
{
    ya_new(&G, YA_CLASSIC, 2);
    CHECK(P->turn == 0 && P->throws == 0 && ya_points(P, YC_CHANCE) == -1);   /* no throw yet */
    CHECK(!ya_keep(&G, 1));
    CHECK(!ya_script(&G, "d--123"));            /* nothing to keep on the first throw */
    ya_new(&G, YA_CLASSIC, 2);
    CHECK(ya_script(&G, "d33215") && P->throws == 1 && P->dice[0] == 3 && P->dice[4] == 5);
    CHECK(ya_points(P, YC_THREES) == 6 && ya_points(P, YC_CHANCE) == 14 && ya_points(P, YC_FULL) == 0);
    CHECK(ya_keep(&G, 3) && P->kept == 3);
    int v[5] = {0, 0, 3, 3, 4};
    CHECK(ya_throw(&G, 3, v) == 1 && P->throws == 2 && P->dice[0] == 3 && P->dice[2] == 3 && P->dice[4] == 4);
    CHECK(ya_script(&G, "d----6"));             /* the third */
    CHECK(P->throws == 3 && !ya_keep(&G, 1) && !ya_script(&G, "d1----"));   /* no fourth */
    CHECK(ya_points(P, YC_FOUR_KIND) == 18 && ya_points(P, YC_THREES) == 12);
    int all[5] = {1, 1, 1, 1, 1};
    CHECK(ya_throw(&G, 0x1f, all) == 0);       /* all kept: nothing to throw (and no throws left anyway) */
    CHECK(ya_score(&G, YC_FOUR_KIND) == 1 && P->box[0][YC_FOUR_KIND] == 18 && P->turn == 1 && P->throws == 0);
    CHECK(P->kept == 0 && P->dice[0] == 0 && P->turns == 1);
    CHECK(ya_script(&G, "d12345 ls") && P->box[1][YC_LARGE] == 40 && P->turn == 0);
    CHECK(ya_script(&G, "d12345") && !ya_score(&G, YC_FOUR_KIND));   /* filled already */
    CHECK(ya_score(&G, YC_FOUR_KIND) == 0 && ya_points(P, YC_LARGE) == 40);
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "d11111") && !ya_script(&G, "d-----"));   /* all kept */
    CHECK(!ya_script(&G, "d1234") && !ya_script(&G, "d123457") && !ya_script(&G, "d12370"));
    CHECK(!ya_score(&G, YC_PAIR) && !ya_score(&G, YC_STRAIGHT));     /* not in this variant */
}

/* the upper bonus at 63 */
static void test_bonus(void)
{
    ya_new(&G, YA_CLASSIC, 1);                  /* three of each face: 63 */
    CHECK(ya_script(&G, "d11123 1 d22213 2 d33312 3 d44412 4 d55512 5 d66612 6"));
    CHECK(ya_upper(P, 0) == 63 && ya_bonus(P, 0) == 35 && ya_total(P, 0) == 98);
    ya_new(&G, YA_NORDIC, 1);
    CHECK(ya_script(&G, "d11123 1 d22213 2 d33312 3 d44412 4 d55512 5 d66123 6"));   /* 57: two sixes */
    CHECK(ya_upper(P, 0) == 57 && ya_bonus(P, 0) == 0);
    ya_new(&G, YA_NORDIC, 1);
    CHECK(ya_script(&G, "d11111 1 d22222 2 d33312 3 d44412 4 d55512 5 d66612 6"));
    CHECK(ya_upper(P, 0) == 69 && ya_bonus(P, 0) == 50 && ya_total(P, 0) == 119);
    ya_new(&G, YA_GENERALA, 1);
    CHECK(ya_script(&G, "d11123 1 d22213 2 d33312 3 d44412 4 d55512 5 d66612 6"));
    CHECK(ya_upper(P, 0) == 63 && ya_bonus(P, 0) == 0 && ya_total(P, 0) == 63);
}

/* classic: the extra five of a kind (100) and the forced joker */
static void test_joker(void)
{
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "d44444 y") && P->box[0][YC_YACHT] == 50);
    CHECK(ya_script(&G, "d44444"));             /* fours empty: there only */
    CHECK(ya_points(P, YC_FOURS) == 20 && ya_points(P, YC_FULL) == -1 && ya_points(P, YC_CHANCE) == -1);
    CHECK(!ya_score(&G, YC_CHANCE) && ya_score(&G, YC_FOURS) && P->extra[0] == 1 && ya_total(P, 0) == 170);
    CHECK(ya_script(&G, "d44444"));             /* fours filled: any lower box, in full */
    CHECK(ya_points(P, YC_FULL) == 25 && ya_points(P, YC_SMALL) == 30 && ya_points(P, YC_LARGE) == 40);
    CHECK(ya_points(P, YC_THREE_KIND) == 20 && ya_points(P, YC_CHANCE) == 20 && ya_points(P, YC_ONES) == -1);
    CHECK(ya_score(&G, YC_LARGE) && P->extra[0] == 2 && ya_total(P, 0) == 50 + 20 + 40 + 200);
    /* every lower box filled: an empty upper box, 0 */
    CHECK(ya_script(&G, "d12346 3k d12346 4k d12346 fh d12346 ss d12346 c"));
    CHECK(ya_script(&G, "d44444") && ya_points(P, YC_ONES) == 0 && ya_points(P, YC_SIXES) == 0);
    CHECK(ya_points(P, YC_CHANCE) == -1 && ya_score(&G, YC_SIXES) && P->extra[0] == 3);
    /* a scratched five of a kind: the joker, no bonus */
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "d12346 y") && P->box[0][YC_YACHT] == 0);
    CHECK(ya_script(&G, "d22222") && ya_points(P, YC_TWOS) == 10 && ya_points(P, YC_FULL) == -1);
    CHECK(ya_script(&G, "2 d22222") && ya_points(P, YC_FULL) == 25 && ya_script(&G, "fh"));
    CHECK(P->extra[0] == 0 && ya_total(P, 0) == 35);
    /* the five-of-a-kind box empty: no joker (a five of a kind is no full house) */
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "d33333") && ya_points(P, YC_FULL) == 0 && ya_points(P, YC_ONES) == 0);
    CHECK(ya_points(P, YC_FOUR_KIND) == 15 && ya_points(P, YC_YACHT) == 50);
    /* nordic: no joker, no extra */
    ya_new(&G, YA_NORDIC, 1);
    CHECK(ya_script(&G, "d55555 y d55555") && ya_points(P, YC_FULL) == 0 && ya_points(P, YC_PAIR) == 10);
    CHECK(ya_script(&G, "p") && P->extra[0] == 0 && ya_total(P, 0) == 60);
}

/* generala: served hands (+5), the double, the instant win */
static void test_generala(void)
{
    ya_new(&G, YA_GENERALA, 2);
    CHECK(ya_script(&G, "d13456") && ya_points(P, YC_STRAIGHT) == 25);           /* served */
    CHECK(ya_script(&G, "st d22333") && P->box[0][YC_STRAIGHT] == 25 && ya_points(P, YC_FULL) == 35);
    CHECK(ya_script(&G, "d--5--") && ya_points(P, YC_FULL) == 0 && ya_points(P, YC_FIVES) == 5);
    CHECK(ya_script(&G, "d--2--") && ya_points(P, YC_FULL) == 30);              /* not served */
    CHECK(ya_script(&G, "fh d66661") && ya_points(P, YC_FOUR_KIND) == 45 && ya_points(P, YC_SIXES) == 24);
    CHECK(ya_script(&G, "4k") && P->box[0][YC_FOUR_KIND] == 45);
    CHECK(ya_script(&G, "d12346 d----5") && ya_points(P, YC_STRAIGHT) == 20 && ya_script(&G, "st"));
    /* the double: only after a generala of 50 */
    CHECK(ya_script(&G, "d12345 d-1111") && ya_points(P, YC_DOUBLE) == 0 && ya_points(P, YC_YACHT) == 50);
    CHECK(ya_script(&G, "y") && P->box[0][YC_YACHT] == 50);
    CHECK(ya_script(&G, "d12345 d-6666 d6----") && ya_points(P, YC_DOUBLE) == 0);
    CHECK(ya_script(&G, "yy") && P->box[1][YC_DOUBLE] == 0 && P->box[1][YC_YACHT] == -1);
    CHECK(ya_script(&G, "d12345 d-3333 d3----") && ya_points(P, YC_DOUBLE) == 100 && ya_script(&G, "yy"));
    CHECK(ya_total(P, 0) == 25 + 45 + 50 + 100 && ya_total(P, 1) == 30 + 20);
    /* served generala: the game won at once */
    CHECK(P->turn == 1 && ya_script(&G, "d55555"));
    CHECK(P->over && P->served && P->winner == 1 && !ya_script(&G, "y") && !ya_script(&G, "d12345"));
    ya_new(&G, YA_GENERALA, 1);                 /* five of a kind on a later throw: just a generala */
    CHECK(ya_script(&G, "d55551 d----5") && !P->over && ya_points(P, YC_YACHT) == 50);
}

/* one to six players: the turn round the table, the end, the highest total, a draw */
static void test_players_end(void)
{
    for (int np = 1; np <= YA_PLAYERS; np++) {
        ya_new(&G, YA_GENERALA, np);
        CHECK(P->players == np);
        int cats[YA_MAX_CATS], n = ya_cats(YA_GENERALA, cats), ok = 1;
        for (int c = 0; c < n && ok; c++)
            for (int q = 0; q < np && ok; q++) {
                ok &= P->turn == q && !P->over;
                ok &= ya_throw(&G, 0, (int[5]){1, 2, 4, 6, q == 0 ? 6 : 1}) == 1;   /* player 0 the highest chance */
                ok &= ya_score(&G, cats[c]) == (c == n - 1 && q == np - 1 ? 2 : 1);
            }
        CHECK(ok && P->over && P->turns == n * np);
        CHECK(np == 1 ? P->winner == -1 : P->winner == 0);
    }
    ya_new(&G, YA_CLASSIC, 2);                  /* a draw: the same every turn */
    int cats[YA_MAX_CATS], n = ya_cats(YA_CLASSIC, cats);
    for (int c = 0; c < n; c++)
        for (int q = 0; q < 2; q++) {
            ya_throw(&G, 0, (int[5]){2, 2, 2, 5, 5});
            ya_score(&G, cats[c]);
        }
    CHECK(P->over && P->winner == -1 && ya_total(P, 0) == ya_total(P, 1) && ya_total(P, 0) > 0);
    CHECK(!ya_script(&G, "d12345"));
    /* 6 players, 15 categories: every event fits */
    ya_new(&G, YA_NORDIC, 6);
    n = ya_cats(YA_NORDIC, cats);
    for (int c = 0; c < n; c++)
        for (int q = 0; q < 6; q++) {
            ya_throw(&G, 0, (int[5]){1, 1, 2, 2, 3});
            ya_throw(&G, 0, (int[5]){1, 1, 2, 2, 3});
            ya_throw(&G, 0, (int[5]){1, 1, 2, 2, 3});
            ya_score(&G, cats[c]);
        }
    CHECK(P->over && G.n == 6 * 15 * 4);
}

/* undo: back to before the last score; never a throw again */
static void test_undo(void)
{
    ya_new(&G, YA_CLASSIC, 2);
    CHECK(!ya_undo(&G));
    CHECK(ya_script(&G, "d12345") && !ya_undo(&G) && P->throws == 1);   /* a throw is never undone */
    CHECK(ya_script(&G, "d-6--- ls") && P->turn == 1);
    CHECK(ya_undo(&G) && P->turn == 0 && P->throws == 2 && P->dice[1] == 6 && P->box[0][YC_LARGE] == -1);
    CHECK(ya_points(P, YC_LARGE) == 0 && ya_script(&G, "c") && P->box[0][YC_CHANCE] == 19);
    CHECK(ya_script(&G, "d66666 d--1--") && ya_undo(&G) && P->turn == 0 && P->throws == 2);   /* the throws after:
                                                                                               * dropped */
    CHECK(!ya_undo(&G) && P->box[0][YC_CHANCE] == -1);
    /* undo out of the end */
    ya_new(&G, YA_GENERALA, 1);
    int cats[YA_MAX_CATS], n = ya_cats(YA_GENERALA, cats);
    for (int c = 0; c < n; c++) {
        ya_throw(&G, 0, (int[5]){1, 2, 3, 4, 6});
        ya_score(&G, cats[c]);
    }
    CHECK(P->over && ya_undo(&G) && !P->over && P->box[0][YC_DOUBLE] == -1 && P->throws == 1);
    /* the kept dice come back with the position */
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "d12345 d-2-2- 2") && P->kept == 0);
    CHECK(ya_undo(&G) && P->kept == 0x15 && P->throws == 2);
}

static void test_script(void)
{
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(!ya_script(&G, "x"));
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(!ya_script(&G, "d12345 zz"));
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(!ya_script(&G, "d12345 st"));         /* generala's straight: not classic's */
    ya_new(&G, YA_CLASSIC, 1);
    CHECK(ya_script(&G, "  d12345   d1----  3k ") && P->box[0][YC_THREE_KIND] == 0 && G.n == 3);
    CHECK((G.ev[0] & YA_THROW_EV) && (G.ev[0] & 7) == 1 && (G.ev[0] >> 12 & 7) == 5 && G.ev[2] == YC_THREE_KIND);
    CHECK(ya_cat_by_name("yy", 2) == YC_DOUBLE && ya_cat_by_name("y", 1) == YC_YACHT && ya_cat_by_name("7", 1) < 0);
}

int main(void)
{
    test_classic_categories();
    test_nordic_categories();
    test_generala_categories();
    test_turn();
    test_bonus();
    test_joker();
    test_generala();
    test_players_end();
    test_undo();
    test_script();
    printf("yacht: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
