/*
 * War (la bataille): the rules' test (the deal, a battle, a war with a card face down, a war with one card left,
 * players going out, the short game's count, whole games from seeds, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/war.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static wgame G, H;
static wpos *P = &G.pos;

static int whole(const wpos *p)
{
    int seen[256] = {0}, m = 0;
    for (int q = 0; q < p->players; q++) {
        for (int i = 0; i < p->n[q]; i++) seen[p->pile[q][i]]++, m++;
        for (int i = 0; i < p->nt[q]; i++) seen[p->table[q][i]]++, m++;
    }
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return m == 52;
}

/* piles of our own, the top card last */
static void piles(int players, const int *a, int na, const int *b, int nb, const int *c, int nc)
{
    war_new(&G, WAR_FULL, players, 1);
    const int *src[3] = {a, b, c};
    int cnt[3] = {na, nb, nc};
    for (int q = 0; q < players; q++) {
        P->n[q] = (uint8_t)cnt[q];
        for (int i = 0; i < cnt[q]; i++) P->pile[q][i] = (uint8_t)src[q][i];
    }
    P->in = (uint8_t)((1u << players) - 1);
}

static void test_deal(void)
{
    war_new(&G, WAR_FULL, 2, 42);
    war_new(&H, WAR_FULL, 2, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && whole(P) && P->n[0] == 26 && P->n[1] == 26);
    war_new(&G, WAR_FULL, 3, 42);
    CHECK(whole(P) && P->n[0] == 18 && P->n[1] == 17 && P->n[2] == 17);
    war_new(&G, WAR_SHORT, 4, 42);
    CHECK(whole(P) && P->n[3] == 13 && war_turn(P) == 0);
    CHECK(war_value(CARD(0, RANK_ACE)) == 14 && war_value(CARD(0, RANK_KING)) == 13 && war_value(CARD(1, 2)) == 2);
}

static void test_battle(void)
{
    const int a[] = {CARD(0, 3), CARD(0, 9)}, b[] = {CARD(1, 4), CARD(1, RANK_ACE)};
    piles(2, a, 2, b, 2, NULL, 0);
    CHECK(war_play(&G, 1) && P->nt[1] == 1 && P->n[1] == 1 && !war_legal(P, 1) && war_turn(P) == 0);
    CHECK(war_play(&G, 0) && P->winner == 1 && P->n[1] == 3 && P->n[0] == 1 && P->battles == 1);
    CHECK(P->nt[0] == 0 && P->nt[1] == 0 && P->pile[1][2] == CARD(1, 4));   /* won cards go under */
    CHECK(war_play(&G, 0) && war_play(&G, 1) && P->winner == 1 && P->phase == WP_OVER && P->n[1] == 4);
    CHECK(war_winners(P) == 2u && !war_play(&G, 0));
}

static void test_war(void)
{
    /* a tie: a card face down, then one face up */
    const int a[] = {CARD(0, 2), CARD(0, 12), CARD(0, 5), CARD(0, 9)}, b[] = {CARD(1, 2), CARD(1, 3), CARD(1, 6),
                                                                          CARD(2, 9)};
    piles(2, a, 4, b, 4, NULL, 0);
    CHECK(war_script(&G, "01") && P->war && P->in == 3 && P->nt[0] == 1);
    CHECK(war_play(&G, 0) && P->nt[0] == 3 && !(P->up[0] >> 1 & 1) && (P->up[0] >> 2 & 1));
    CHECK(war_play(&G, 1) && P->winner == 0 && P->n[0] == 1 + 6 && P->n[1] == 1 && !P->war);
    /* one card left in a war: face up */
    const int c[] = {CARD(0, 4), CARD(0, 9)}, d[] = {CARD(1, 8), CARD(1, 7), CARD(2, 9)};
    piles(2, c, 2, d, 3, NULL, 0);
    CHECK(war_script(&G, "01") && P->war);
    CHECK(war_play(&G, 0) && P->nt[0] == 2 && (P->up[0] >> 1 & 1));
    CHECK(war_play(&G, 1) && P->winner == 1 && P->phase == WP_OVER);   /* 7 face down, then 8 beats 4 */
    /* three players: only the tied play the war; a tied player with no card left drops out */
    const int e[] = {CARD(0, 10)}, f[] = {CARD(1, 2), CARD(1, 10)}, h[] = {CARD(2, 5), CARD(2, 3)};
    piles(3, e, 1, f, 2, h, 2);
    CHECK(war_script(&G, "012") && P->winner == 1 && P->n[1] == 1 + 3 && P->n[0] == 0);   /* player 0 is out */
    CHECK(P->in == 6 && war_turn(P) == 1 && !war_legal(P, 0));
}

static void test_short(void)
{
    war_new(&G, WAR_SHORT, 2, 3);
    int b = 0;
    while (P->phase == WP_TURN && b < 100) {
        CHECK(war_script(&G, "b"));
        b++;
    }
    CHECK(P->phase == WP_OVER && (P->battles == WAR_SHORT_BATTLES || war_cards(P, 0) == 0 || war_cards(P, 1) == 0));
    unsigned w = war_winners(P);
    CHECK(w && (!(w & 1) || war_cards(P, 0) >= war_cards(P, 1)) && (!(w & 2) || war_cards(P, 1) >= war_cards(P, 0)));
    /* a tie at the count: both win */
    const int a[] = {CARD(0, 3), CARD(0, 9)}, c[] = {CARD(1, 4), CARD(1, 2), CARD(1, 6), CARD(1, 5)};
    piles(2, a, 2, c, 4, NULL, 0);
    G.pos.variant = WAR_SHORT;
    P->battles = WAR_SHORT_BATTLES - 1;
    CHECK(war_script(&G, "01") && P->phase == WP_OVER && war_cards(P, 0) == 3 &&
          war_cards(P, 1) == 3 && war_winners(P) == 3);
}

static void test_games(void)
{
    for (int v = 0; v < WAR_VARIANTS; v++)
        for (int np = 2; np <= 4; np++) {
            int ok = 1, battles = 0, wars = 0, longest = 0;
            for (uint32_t seed = 1; seed <= 20; seed++) {
                war_new(&G, v, np, seed * 101u + (uint32_t)np);
                while (P->phase == WP_TURN) {
                    int was = P->war;
                    if (!war_play(&G, war_turn(P))) { ok = 0; break; }
                    if (!was && P->war) wars++;
                    if (!whole(P)) { ok = 0; break; }
                }
                battles += P->battles;
                longest = P->battles > longest ? P->battles : longest;
                ok &= P->phase == WP_OVER && war_winners(P) != 0;
                if (v == WAR_FULL) ok &= war_cards(P, __builtin_ctz(war_winners(P))) == 52;
            }
            CHECK(ok);
            printf("ok   %s, %d players: %d battles (longest %d), %d wars in 20 games\n", v ? "short" : "full", np,
                   battles, longest, wars);
        }
    war_new(&G, WAR_FULL, 3, 8);
    CHECK(war_script(&G, "012 b b 0"));
    int n = G.n;
    wpos before = G.pos;
    CHECK(war_script(&G, "1") && war_undo(&G) && G.n == n && !memcmp(&before, &G.pos, sizeof before));
    CHECK(!war_script(&G, "0") && !war_script(&G, "x"));
}

int main(void)
{
    test_deal();
    test_battle();
    test_war();
    test_short();
    test_games();
    printf("war: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
