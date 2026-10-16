/*
 * Y's rules test (src/games/ygame_rules.c, no SDL): the notation, the neighbours, the sides (the corners on two), the
 * wins (a side from corner to corner, a Y inside, two sides only), the swap, undo, the winner's Y, random games
 * (always a winner, the same one as the board's reduction).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/ygame.h"

static ygame G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return yg_cell(G.n, name, &len);
}

static int stone(const char *name) { return G.cell[at(name)]; }

static void fresh(int n, const char *moves)
{
    yg_new(&G, n);
    if (!yg_script(&G, moves)) printf("bad script: %s\n", moves);
}

static int has(const int *nb, int k, int cell)
{
    for (int i = 0; i < k; i++)
        if (nb[i] == cell) return 1;
    return 0;
}

/* the winner's Y: the winner's stones, all in one piece, touching the three sides */
static int chain_ok(void)
{
    int cells[YG_CELLS], nb[6], n = G.n, k = yg_chain(&G, cells), p = G.winner, sides = 0;
    if (k <= 0 || p < 0) return 0;
    uint8_t in[YG_CELLS] = {0}, seen[YG_CELLS] = {0};
    for (int i = 0; i < k; i++) {
        if (G.cell[cells[i]] != p + 1) return 0;
        in[cells[i]] = 1;
    }
    int stack[YG_CELLS], top = 0, reached = 0;
    stack[top++] = cells[0];
    seen[cells[0]] = 1;
    while (top) {
        int q = stack[--top];
        reached++;
        sides |= yg_sides(n, q);
        for (int m = yg_around(n, q, nb), j = 0; j < m; j++)
            if (in[nb[j]] && !seen[nb[j]]) seen[nb[j]] = 1, stack[top++] = nb[j];
    }
    return reached == k && sides == YG_ALL ? k : 0;
}

/* the board's reduction: each small triangle of three cells (c, r), (c, r + 1), (c + 1, r + 1) gives its colour's
 * majority to the cell (c, r) of a board one smaller; the last cell's colour is the winner of a full board */
static int reduce(const uint8_t *full, int n)
{
    uint8_t b[YG_CELLS];
    memcpy(b, full, (size_t)yg_count(n));
    for (int m = n - 1; m >= 1; m--)
        for (int r = 0; r < m; r++)
            for (int c = 0; c <= r; c++) {
                int k = (b[yg_index(c, r)] == 1) + (b[yg_index(c, r + 1)] == 1) + (b[yg_index(c + 1, r + 1)] == 1);
                b[yg_index(c, r)] = k >= 2 ? 1 : 2;   /* row by row from the top: the cells read are not yet changed */
            }
    return b[0] - 1;
}

/* does player p have a group touching the three sides on the board as it stands? */
static int has_y(int p)
{
    for (int i = 0; i < yg_count(G.n); i++)
        if (G.cell[i] == p + 1 && yg_group(&G, i, NULL, NULL) == YG_ALL) return 1;
    return 0;
}

static unsigned rng = 12345;
static int roll(int n)
{
    rng = rng * 1103515245u + 12345u;
    return (int)((rng >> 8) % (unsigned)n);
}

int main(void)
{
    /* notation: the letter in the row, the row from the apex */
    yg_new(&G, 13);
    CHECK(G.n == 13 && yg_count(13) == 91);
    CHECK(at("a1") == 0 && at("a2") == 1 && at("b2") == 2 && at("a13") == yg_index(0, 12) && at("m13") == 90);
    CHECK(at("b1") == YG_NONE && at("n13") == YG_NONE && at("a14") == YG_NONE && at("a0") == YG_NONE);
    CHECK(at("a") == YG_NONE && at("swap") == YG_SWAP);
    for (int i = 0; i < 91; i++) {
        char s[8];
        int len;
        yg_name(i, s, sizeof s);
        CHECK(yg_cell(13, s, &len) == i && len == (int)strlen(s));
        CHECK(yg_index(yg_col(i), yg_row(i)) == i);
    }
    yg_new(&G, 7);
    CHECK(G.n == 13);                           /* not a size: 13 */

    /* the neighbours: six inside, four on a side, two in each corner; touching is mutual */
    int nb[6], k;
    yg_new(&G, 9);
    k = yg_around(9, at("c5"), nb);
    CHECK(k == 6 && has(nb, k, at("b5")) && has(nb, k, at("d5")) && has(nb, k, at("b4")) && has(nb, k, at("c4")) &&
          has(nb, k, at("c6")) && has(nb, k, at("d6")));
    CHECK(!has(nb, k, at("d4")) && !has(nb, k, at("b6")));
    CHECK(yg_around(9, at("a1"), nb) == 2 && yg_around(9, at("a9"), nb) == 2 && yg_around(9, at("i9"), nb) == 2);
    CHECK(yg_around(9, at("a5"), nb) == 4 && yg_around(9, at("e5"), nb) == 4 && yg_around(9, at("d9"), nb) == 4);
    for (int c = 0; c < 45; c++) {
        int nb2[6];
        for (int m = yg_around(9, c, nb), j = 0; j < m; j++) CHECK(has(nb2, yg_around(9, nb[j], nb2), c));
    }

    /* the sides: a corner is on both of its sides */
    CHECK(yg_sides(9, at("a1")) == (YG_LEFT | YG_RIGHT));
    CHECK(yg_sides(9, at("a9")) == (YG_LEFT | YG_BOTTOM));
    CHECK(yg_sides(9, at("i9")) == (YG_RIGHT | YG_BOTTOM));
    CHECK(yg_sides(9, at("a5")) == YG_LEFT && yg_sides(9, at("e5")) == YG_RIGHT && yg_sides(9, at("d9")) == YG_BOTTOM);
    CHECK(yg_sides(9, at("c5")) == 0);

    /* a whole side, from corner to corner, joins all three: its two corners touch the other two sides */
    fresh(9, "a1 c3 a2 c4 a3 c5 a4 c6 a5 c7 a6 c8 a7 c9 a8 d9");
    CHECK(G.winner == -1 && yg_turn(&G) == 0);
    CHECK(yg_script(&G, "a9") && G.winner == 0 && chain_ok() == 9);
    CHECK(!yg_legal(&G, at("e9")) && !yg_play(&G, at("e9")));   /* the game is over */
    /* one short of the corner: the left and the right side only (through the apex) */
    fresh(9, "a1 c3 a2 c4 a3 c5 a4 c6 a5 c7 a6 c8 a7 c9 a8");
    CHECK(G.winner == -1 && yg_group(&G, at("a1"), NULL, NULL) == (YG_LEFT | YG_RIGHT));
    /* the bottom side for ebony, after an ivory stone: its two corners */
    fresh(9, "e5 a9 c5 b9 c6 c9 c7 d9 c8 e9 d5 f9 d6 g9 d7 h9");
    CHECK(G.winner == -1 && yg_group(&G, at("a9"), NULL, NULL) == (YG_LEFT | YG_BOTTOM));
    CHECK(yg_script(&G, "e6 i9") && G.winner == 1 && chain_ok() == 9);

    /* a Y inside, no corner: across row 5 (a5 on the left, e5 on the right), down from c5 to c9 on the bottom */
    fresh(9, "a5 a1 b5 a2 c5 b3 d5 a3 c6 a4 c7 a6 c8 a7 e5 h9");
    CHECK(G.winner == -1 && yg_group(&G, at("a5"), NULL, NULL) == (YG_LEFT | YG_RIGHT));
    CHECK(yg_script(&G, "c9") && G.winner == 0 && chain_ok() == 9);
    /* the same without e5: the left and the bottom, no win */
    fresh(9, "a5 a1 b5 a2 c5 b3 d5 a3 c6 a4 c7 a6 c8 a7 c9");
    CHECK(G.winner == -1 && yg_group(&G, at("c9"), NULL, NULL) == (YG_LEFT | YG_BOTTOM));
    /* two groups touching all three sides between them, not joined: no win */
    fresh(9, "a5 a1 b5 a2 c5 b3 e5 a3 c6 a4 c7 a6 c8 a7 c9");
    CHECK(G.winner == -1);
    /* the winner's Y leaves out a branch it does not need */
    fresh(9, "a5 a1 b5 a2 c5 b3 d5 a3 c6 a4 c7 a6 c8 a7 d6 b9 d7 f9 e5 h9 c9");
    CHECK(G.winner == 0 && chain_ok() == 9);

    /* the swap: the second move only; the stone turns ebony where it stands, then ivory plays */
    fresh(13, "d7 swap");
    CHECK(G.nmoves == 2 && yg_turn(&G) == 0 && stone("d7") == YG_EBONY && yg_last(&G) == at("d7"));
    CHECK(!yg_can_swap(&G) && !yg_script(&G, "swap"));
    CHECK(yg_undo(&G) && stone("d7") == YG_IVORY && yg_can_swap(&G) && yg_turn(&G) == 1);
    yg_new(&G, 9);
    CHECK(!yg_can_swap(&G) && !yg_script(&G, "swap"));
    fresh(9, "a1 b2");
    CHECK(!yg_swap(&G));
    fresh(9, "a1");
    CHECK(!yg_play(&G, at("a1")));             /* an occupied cell */
    fresh(9, "a1 swap");                        /* a swapped corner: ebony's, both sides */
    CHECK(yg_group(&G, at("a1"), NULL, NULL) == (YG_LEFT | YG_RIGHT) && G.cell[0] == YG_EBONY);

    /* undo all the way, and after a win */
    fresh(9, "e5 swap a1 b2 c3");
    CHECK(G.nmoves == 5);
    while (yg_undo(&G)) {}
    CHECK(G.nmoves == 0 && yg_turn(&G) == 0 && yg_last(&G) == -1);
    for (int c = 0; c < 45; c++) CHECK(G.cell[c] == YG_EMPTY);
    fresh(9, "a1 c3 a2 c4 a3 c5 a4 c6 a5 c7 a6 c8 a7 c9 a8 d9 a9");
    CHECK(G.winner == 0 && yg_undo(&G) && G.winner == -1 && stone("a9") == YG_EMPTY && yg_play(&G, at("b9")));

    /* random games on every size: a winner always comes, at the latest when the board is full, with a Y to show;
     * the board then filled up has the same winner by the reduction, and only that player has a Y */
    int full_wins = 0;
    for (int game = 0; game < 600; game++) {
        int n = game % 3 == 0 ? 9 : game % 3 == 1 ? 13 : 19, cells = yg_count(n);
        yg_new(&G, n);
        while (G.winner < 0 && G.nmoves < cells) {
            int c = roll(cells);
            while (!yg_legal(&G, c)) c = (c + 1) % cells;
            yg_play(&G, c);
        }
        CHECK(G.winner >= 0 && chain_ok() >= 3);
        full_wins += G.nmoves == cells;
        int w = G.winner;
        G.winner = -1;                          /* the rest of the board, filled at random */
        for (int c = 0; c < cells; c++)
            if (!G.cell[c]) G.cell[c] = (uint8_t)(1 + roll(2));
        CHECK(reduce(G.cell, n) == w && has_y(w) && !has_y(1 - w));
    }
    CHECK(full_wins < 600);                     /* most games end before the board is full */

    printf("ygame: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
