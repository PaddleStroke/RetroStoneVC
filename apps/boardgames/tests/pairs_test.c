/*
 * Pairs: the rules' test (the layout: each picture twice, the animals first; turning cards; a pair kept and the same
 * player again; a miss turned back and the next player; the moves; the end and the winners, ties shared; undo; the
 * notation; whole games from seeds, 1 to 4 players). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pairs.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static prgame G, H;
static prpos *P = &G.pos;

/* the card with the same picture as card c (-1) */
static int mate(int c)
{
    for (int i = 0; i < P->n; i++)
        if (i != c && P->pic[i] == P->pic[c]) return i;
    return -1;
}

/* a card whose picture differs from card c's */
static int other(int c)
{
    for (int i = 0; i < P->n; i++)
        if (P->pic[i] != P->pic[c] && P->found[i] == 255) return i;
    return -1;
}

static void test_layout(void)
{
    for (int v = 0; v < PR_VARIANTS; v++)
        for (uint32_t seed = 1; seed < 60; seed++) {
            pr_new(&G, v, 2, seed);
            pr_new(&H, v, 2, seed);
            CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
            CHECK(P->n == pr_cards(v) && P->cols * P->rows == P->n);
            int count[PR_POOL] = {0}, animals = 0, kinds = 0;
            for (int i = 0; i < P->n; i++) count[P->pic[i]]++;
            for (int k = 0; k < PR_POOL; k++) {
                CHECK(count[k] == 0 || count[k] == 2);
                kinds += count[k] == 2;
                animals += k < PR_ANIMALS && count[k] == 2;
            }
            CHECK(kinds == P->n / 2);
            CHECK(animals == (P->n / 2 < PR_ANIMALS ? P->n / 2 : PR_ANIMALS));
        }
    CHECK(pr_cards(PR_SMALL) == 12 && pr_cards(PR_MEDIUM) == 24 && pr_cards(PR_LARGE) == 36);
    int t, c, seen[8][6] = {{0}};
    for (int k = PR_ANIMALS; k < PR_POOL; k++) {
        pr_piece(k, &t, &c);
        CHECK(t >= 1 && t <= 6 && c >= 0 && c <= 5 && !seen[t][c]);
        seen[t][c] = 1;
    }
}

static void test_turns(void)
{
    pr_new(&G, PR_SMALL, 2, 9);
    CHECK(P->turn == 0 && P->phase == PP_PLAY);
    int a = 0, b = mate(0);
    CHECK(pr_play(&G, PA_TURN | a) && P->up[0] == a);
    CHECK(!pr_play(&G, PA_TURN | a));           /* the same card again: no */
    CHECK(!pr_play(&G, PA_BACK));               /* nothing to turn back */
    CHECK(pr_play(&G, PA_TURN | b) && P->found[a] == 0 && P->found[b] == 0 && P->pairs[0] == 1);
    CHECK(P->turn == 0 && P->moves == 1 && P->up[0] < 0);   /* a pair: the same player again */
    CHECK(!pr_play(&G, PA_TURN | a));           /* found: out of play */
    int c = other(a), d = other(c);             /* two cards still in play, not a pair */
    CHECK(pr_play(&G, PA_TURN | c) && pr_play(&G, PA_TURN | d) && P->phase == PP_SHOW && P->moves == 2);
    CHECK(!pr_play(&G, PA_TURN | mate(c)));     /* shown: only turning back */
    CHECK(pr_play(&G, PA_BACK) && P->turn == 1 && P->phase == PP_PLAY && P->up[0] < 0 && P->seen[c] && P->seen[d]);
    CHECK(pr_left(P) == 10);
    /* undo: back one action at a time */
    prpos before = G.pos;
    CHECK(pr_play(&G, PA_TURN | c) && pr_undo(&G) && !memcmp(&G.pos, &before, sizeof before));
    while (G.n) pr_undo(&G);
    pr_new(&H, PR_SMALL, 2, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos) && !pr_undo(&G));
}

static void test_end(void)
{
    /* one player: every pair found, the moves counted */
    pr_new(&G, PR_SMALL, 1, 4);
    for (int i = 0; i < P->n; i++)
        if (P->found[i] == 255) {
            int m = mate(i);
            CHECK(pr_play(&G, PA_TURN | i) && pr_play(&G, PA_TURN | m));
        }
    CHECK(P->phase == PP_OVER && P->moves == 6 && P->pairs[0] == 6 && pr_winners(P) == 1);
    /* two players sharing: three pairs each */
    pr_new(&G, PR_SMALL, 2, 4);
    int k = 0;
    for (int i = 0; i < P->n; i++) {
        if (P->found[i] != 255) continue;
        if (k++ == 3) {                         /* a miss after three pairs: player 1's turn */
            int o = other(i);
            CHECK(pr_play(&G, PA_TURN | i) && pr_play(&G, PA_TURN | o) && pr_play(&G, PA_BACK) && P->turn == 1);
        }
        CHECK(pr_play(&G, PA_TURN | i) && pr_play(&G, PA_TURN | mate(i)));
    }
    CHECK(P->phase == PP_OVER && P->pairs[0] == 3 && P->pairs[1] == 3 && pr_winners(P) == 3);
    CHECK(!pr_legal(P, PA_TURN | 0) && !pr_legal(P, PA_BACK));
}

static void test_script(void)
{
    pr_new(&G, PR_MEDIUM, 3, 21);
    char s[64];
    snprintf(s, sizeof s, "t0 t%d", mate(0));
    CHECK(pr_script(&G, s) && P->pairs[0] == 1);
    CHECK(!pr_script(&G, "t0") && !pr_script(&G, "x") && !pr_script(&G, "t99"));
    CHECK(pr_script(&G, "#") && P->phase == PP_OVER);
}

/* whole games: a player who remembers every card; at most one miss on each card before its pair is found */
static void test_games(void)
{
    for (int players = 1; players <= 4; players++)
        for (int v = 0; v < PR_VARIANTS; v++)
            for (uint32_t seed = 1; seed <= 30; seed++) {
                pr_new(&G, v, players, seed * 977u);
                int steps = 0, ok = 1;
                while (P->phase != PP_OVER && steps < 1000) {
                    int a = pr_auto(P);
                    if (!pr_play(&G, a)) { ok = 0; break; }
                    steps++;
                }
                int sum = 0;
                for (int q = 0; q < players; q++) sum += P->pairs[q];
                CHECK(ok && P->phase == PP_OVER && sum == P->n / 2);
                CHECK(P->moves >= P->n / 2 && P->moves <= P->n);
                unsigned w = pr_winners(P);
                CHECK(w && w < (1u << players));
            }
}

/* a player who remembers nothing (cards turned at random) still finishes the biggest grid: the actions kept never
 * run out (they once stopped the game at 1024, a third of such games) */
static void test_forgetful(void)
{
    unsigned r = 12345;
    for (int seed = 1; seed <= 300; seed++) {
        pr_new(&G, PR_LARGE, 1 + seed % PR_MAXP, (uint32_t)seed);
        int ok = 1;
        while (ok && P->phase != PP_OVER) {
            int a = PA_BACK;
            if (P->phase == PP_PLAY) {
                do {
                    r = r * 1103515245u + 12345u;
                    a = PA_TURN | (int)(r >> 16) % P->n;
                } while (!pr_legal(P, a));
            }
            ok = pr_play(&G, a);
        }
        CHECK(ok && P->phase == PP_OVER);
    }
}

int main(void)
{
    test_layout();
    test_turns();
    test_end();
    test_script();
    test_games();
    test_forgetful();
    printf("pairs: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
