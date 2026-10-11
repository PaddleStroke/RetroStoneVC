/*
 * Happy Families: the rules' test (the deal, what may be asked, given, "pioche" and the lucky draw, families laid
 * down, an empty hand drawing, the end and its winners, whole games from seeds, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/families.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static fgame G, H;
static fpos *P = &G.pos;

/* the 42 cards once each: hands, stock, families down */
static int whole(const fpos *p)
{
    int seen[256] = {0}, m = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++, m++;
    for (int i = 0; i < p->nstock; i++) seen[p->stock[i]]++, m++;
    for (int f = 0; f < FAM_FAMILIES; f++)
        if (p->owner[f] != FAM_NONE)
            for (int k = 0; k < FAM_MEMBERS; k++) seen[FAM_CARD(f, k)]++, m++;
    for (int f = 0; f < FAM_FAMILIES; f++)
        for (int k = 0; k < FAM_MEMBERS; k++)
            if (seen[FAM_CARD(f, k)] != 1) return 0;
    return m == FAM_CARDS;
}

static void hand(int pl, const int *cards, int n)
{
    P->n[pl] = 0;
    for (int i = 0; i < n; i++) P->hand[pl][P->n[pl]++] = (uint8_t)cards[i];
}

static void test_deal(void)
{
    static const int each[7] = {0, 0, 7, 7, 6, 5, 5};
    for (int np = 2; np <= 6; np++) {
        fam_new(&G, np, 42);
        fam_new(&H, np, 42);
        CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && whole(P) && P->turn == 0 && P->phase == FP_ASK);
        CHECK(P->nstock == FAM_CARDS - np * each[np] && P->n[0] == each[np]);
    }
}

static void test_ask(void)
{
    fam_new(&G, 3, 7);
    const int a[] = {FAM_CARD(FAM_CATS, FAM_SON), FAM_CARD(FAM_OWLS, FAM_MOTHER)},
              b[] = {FAM_CARD(FAM_CATS, FAM_FATHER), FAM_CARD(FAM_FROGS, 0)}, c[] = {FAM_CARD(FAM_BEARS, 1)};
    hand(0, a, 2);
    hand(1, b, 2);
    hand(2, c, 1);
    P->turn = 0;
    CHECK(fam_legal(P, FAM_ASK(1, FAM_CARD(FAM_CATS, FAM_FATHER))));
    CHECK(!fam_legal(P, FAM_ASK(1, FAM_CARD(FAM_FROGS, 0))));                /* no frog in hand */
    CHECK(!fam_legal(P, FAM_ASK(1, FAM_CARD(FAM_CATS, FAM_SON))));           /* held */
    CHECK(!fam_legal(P, FAM_ASK(0, FAM_CARD(FAM_CATS, FAM_FATHER))));        /* oneself */
    CHECK(!fam_legal(P, FAM_ASK(3, FAM_CARD(FAM_CATS, FAM_FATHER))));
    /* given: the asker goes on */
    CHECK(fam_play(&G, FAM_ASK(1, FAM_CARD(FAM_CATS, FAM_FATHER))) && P->turn == 0 && P->n[0] == 3 &&
          P->n[1] == 1 && P->answer == FAM_GIVEN && fam_holds(P, 0, FAM_CARD(FAM_CATS, FAM_FATHER)));
    /* pioche: a card drawn, the turn passes */
    int top = P->stock[P->nstock - 1], ns = P->nstock;
    if (top == FAM_CARD(FAM_OWLS, FAM_SON)) top = P->stock[P->nstock - 1] = FAM_CARD(FAM_LIONS, 0);
    CHECK(fam_play(&G, FAM_ASK(2, FAM_CARD(FAM_OWLS, FAM_SON))) && P->answer == FAM_MISSED && P->drawn == top &&
          P->nstock == ns - 1 && P->turn == 1 && P->ask_from == 0 && P->ask_to == 2);
    /* the lucky draw: the card asked for, the turn goes on */
    P->turn = 0;
    P->stock[P->nstock - 1] = FAM_CARD(FAM_OWLS, FAM_DAUGHTER);
    for (int i = 0; i < P->nstock - 1; i++)
        if (P->stock[i] == FAM_CARD(FAM_OWLS, FAM_DAUGHTER)) P->stock[i] = (uint8_t)top;   /* (the copy out) */
    CHECK(fam_play(&G, FAM_ASK(1, FAM_CARD(FAM_OWLS, FAM_DAUGHTER))) && P->answer == FAM_LUCKY && P->turn == 0);
}

static void test_lay(void)
{
    fam_new(&G, 2, 11);
    int a[6], k = 0;
    for (int m = 0; m < FAM_MEMBERS; m++)
        if (m != FAM_GRANDMOTHER) a[k++] = FAM_CARD(FAM_MICE, m);
    hand(0, a, 5);
    const int b[] = {FAM_CARD(FAM_MICE, FAM_GRANDMOTHER), FAM_CARD(FAM_LIONS, 2)};
    hand(1, b, 2);
    P->turn = 0;
    CHECK(fam_play(&G, FAM_ASK(1, FAM_CARD(FAM_MICE, FAM_GRANDMOTHER))));
    CHECK(P->owner[FAM_MICE] == 0 && P->nlaid[0] == 1 && P->laid[0][0] == FAM_MICE);
    CHECK(P->n[0] == 1 && P->turn == 0);        /* an empty hand draws, and the turn goes on */
    /* the stock empty: nobody else holding cards, anyone may be asked (the answer: pioche, the turn passes) */
    fam_new(&G, 3, 11);
    const int c[] = {FAM_CARD(FAM_OWLS, 0)};
    hand(0, c, 1);
    P->n[1] = P->n[2] = 0;
    P->nstock = 0;
    P->turn = 0;
    CHECK(fam_legal(P, FAM_ASK(1, FAM_CARD(FAM_OWLS, 1))) && fam_legal(P, FAM_ASK(2, FAM_CARD(FAM_OWLS, 1))));
}

static void test_games(void)
{
    for (int np = 2; np <= 6; np++) {
        int ok = 1, asks = 0, ties = 0;
        for (uint32_t seed = 1; seed <= 40; seed++) {
            fam_new(&G, np, seed * 17u + (uint32_t)np);
            uint32_t rng = seed;
            while (P->phase == FP_ASK) {
                int a = fam_auto(P);
                /* now and then a question to the wrong player (pioche) */
                rng = rng * 1103515245u + 12345u;
                if ((rng >> 16) % 4 == 0)
                    for (int k = 1; k < np; k++) {
                        int q = (P->turn + k) % np, alt = FAM_ASK(q, FAM_ASK_CARD(a));
                        if (q != FAM_ASK_PLAYER(a) && fam_legal(P, alt)) { a = alt; break; }
                    }
                if (!fam_play(&G, a)) { ok = 0; break; }
                if (!whole(P)) { ok = 0; break; }
            }
            asks += G.n;
            int total = 0;
            for (int q = 0; q < np; q++) total += P->nlaid[q];
            unsigned w = fam_winners(P);
            ties += (w & (w - 1)) != 0;
            ok &= P->phase == FP_OVER && total == FAM_FAMILIES && w != 0;
        }
        CHECK(ok);
        printf("ok   %d players: 40 games, %d questions, %d shared wins\n", np, asks, ties);
    }
    fam_new(&G, 4, 3);
    CHECK(fam_script(&G, "x x x"));
    int n = G.n;
    fpos before = G.pos;
    CHECK(fam_script(&G, "x") && fam_undo(&G) && G.n == n && !memcmp(&before, &G.pos, sizeof before));
    CHECK(fam_script(&G, "#") && P->phase == FP_OVER && !fam_script(&G, "x"));
    fam_new(&G, 2, 3);
    CHECK(!fam_script(&G, "a170") && !fam_script(&G, "a0") && !fam_script(&G, "z"));
}

int main(void)
{
    test_deal();
    test_ask();
    test_lay();
    test_games();
    printf("families: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
