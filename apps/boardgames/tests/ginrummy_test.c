/*
 * Gin Rummy: the rules' test (the melds and the best arrangement, deadwood; the first upcard's offer; drawing and
 * discarding; knocking: when it is allowed; the layoffs, the undercut, gin; the dead hand; the end of the game with
 * the game, box and shutout bonuses; undo; the notation; whole games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/ginrummy.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static gngame G, H;
static gnpos *P = &G.pos;

/* cards from names: "7h 8h 9h" */
static int cards(const char *s, uint8_t *out)
{
    int n = 0, c;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int k = gin_card_name(s, &c);
        if (!k) return -1;
        out[n++] = (uint8_t)c;
        s += k;
    }
    return n;
}

static int dead(const char *s)
{
    uint8_t c[11];
    int n = cards(s, c);
    return gin_arrange(c, n, NULL);
}

static void test_melds(void)
{
    CHECK(gin_value(CARD(0, 1)) == 1 && gin_value(CARD(0, 9)) == 9 && gin_value(CARD(0, RANK_KING)) == 10);
    CHECK(dead("7h 8h 9h") == 0 && dead("7h 8h 9s") == 24 && dead("7h 7s 7d") == 0 && dead("7h 7s") == 14);
    CHECK(dead("Ah 2h 3h") == 0 && dead("Qh Kh Ah") == 21);   /* the ace is low only */
    /* a card in a set or a run: the best choice */
    CHECK(dead("7h 7s 7d 8h 9h") == 14);        /* the set 777 leaves 8h 9h (17), the run 789h leaves 7s 7d (14) */
    CHECK(dead("7h 7s 7d 7c 8h 9h") == 0);      /* 7s 7d 7c and 7h 8h 9h */
    CHECK(dead("5s 6s 7s 8s 9s Ts") == 0 && dead("2c 3c 4c 5c Kd Kh Ks") == 0);
    CHECK(dead("As 2d 3h 4c 5s 6d 7h 8c 9s Td") == 55);
    gin_arr a;
    uint8_t c[11];
    int n = cards("Kd 2c Kh 3c 4c 9s Ks 5c Ad 8h", c);
    CHECK(gin_arrange(c, n, &a) == 18 && a.nm == 2 && a.mlen[0] == 4 && a.mlen[1] == 3 && a.n == 10);
    CHECK(a.card[0] == CARD(SUIT_CLUBS, 2) && a.card[3] == CARD(SUIT_CLUBS, 5));   /* the run first (the lowest) */
    CHECK(a.card[7] == CARD(SUIT_DIAMONDS, 1) && a.card[9] == CARD(SUIT_SPADES, 9));   /* the deadwood by rank */
    CHECK(gin_without(c, n, CARD(SUIT_SPADES, 9)) == 9 && gin_without(c, n, CARD(SUIT_SPADES, 1)) > 100);
}

/* sets a hand up: the player's cards, the turn, the phase */
static void set_hand(int pl, const char *s)
{
    P->n[pl] = (uint8_t)cards(s, P->hand[pl]);
}

static void test_offer(void)
{
    gin_new(&G, 3);
    gin_new(&H, 3);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->dealer == 1 && P->turn == 0 && P->phase == GN_OFFER && P->n[0] == 10 && P->n[1] == 10);
    CHECK(P->nstock == 31 && P->npile == 1);
    int seen[256] = {0};
    for (int q = 0; q < 2; q++)
        for (int i = 0; i < 10; i++) seen[P->hand[q][i]]++;
    for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
    seen[P->pile[0]]++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(once);
    CHECK(!gin_legal(P, GNA_STOCK) && gin_legal(P, GNA_PASS) && gin_legal(P, GNA_TAKE));
    CHECK(gin_play(&G, GNA_PASS) && P->turn == 1 && P->phase == GN_OFFER);   /* the dealer's turn to choose */
    CHECK(gin_play(&G, GNA_PASS) && P->turn == 0 && P->phase == GN_DRAW);    /* both passed: the stock */
    CHECK(!gin_legal(P, GNA_TAKE) && gin_legal(P, GNA_STOCK));
    CHECK(gin_play(&G, GNA_STOCK) && P->n[0] == 11 && P->phase == GN_DISCARD);
    int c = P->hand[0][0];
    CHECK(gin_play(&G, GNA_DISCARD | c) && P->turn == 1 && P->phase == GN_DRAW && P->pile[P->npile - 1] == c);
    CHECK(gin_legal(P, GNA_TAKE));              /* now the discard can be taken */
    /* the card taken from the pile cannot go straight back */
    CHECK(gin_play(&G, GNA_TAKE) && !gin_legal(P, GNA_DISCARD | c) && P->taken == c);
    CHECK(!gin_legal(P, GNA_DISCARD | P->hand[0][1]));   /* not in this hand */
    /* the non-dealer takes the first upcard */
    gin_new(&G, 3);
    int up = P->pile[0];
    CHECK(gin_play(&G, GNA_TAKE) && P->n[0] == 11 && P->taken == up && P->npile == 0 && P->phase == GN_DISCARD);
}

static void test_knock(void)
{
    gin_new(&G, 5);
    /* player 0: 2c 3c 4c, 7h 7s 7d, Kd Kh Ks and a 9s: knocks with... */
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 9s 5h");
    set_hand(1, "5c 6c 8h 9h Th 7c Qd Qs 2d 3d");
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    CHECK(gin_can_knock(P));
    int c9;
    gin_card_name("9s", &c9);
    CHECK(gin_legal(P, GNA_KNOCK | c9) && gin_legal(P, GNA_KNOCK | CARD(SUIT_HEARTS, 5)));
    CHECK(!gin_legal(P, GNA_KNOCK | CARD(SUIT_CLUBS, 2)));   /* breaking the run: 9 + 5 + 3 + 4 > 10 */
    CHECK(gin_script(&G, "k9s"));
    const gin_result *r = &P->res;
    /* knocker: deadwood 5; defender: melds 8h 9h Th? no: 8h 9h Th is a run; 5c 6c 7c a run: deadwood Qd Qs 2d 3d =
     * 25; lays off nothing on 234c (5c is in its run? the best: 5c 6c 7c run, or lay off 5c on 2c 3c 4c and keep
     * 6c 7c dead) */
    CHECK(r->kind == GR_KNOCK && r->knocker == 0 && r->winner == 0 && r->dead[0] == 5);
    CHECK(r->dead[1] == 25 && r->points == 20 && P->score[0] == 20 && P->boxes[0] == 1 && P->phase == GN_END);
    /* layoffs: the defender's 5c and 6c extend the knocker's run 2c 3c 4c; Kc on the kings */
    gin_new(&G, 5);
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 9s 5h");
    set_hand(1, "5c 6c Kc 8h 9h Th Qd 2d 3d Js");
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    CHECK(gin_script(&G, "k9s"));
    CHECK(r->kind == GR_KNOCK && r->nlay == 3 && r->dead[1] == 10 + 2 + 3 + 10 && r->points == 25 - 5);
    /* an undercut: the defender as low or lower */
    gin_new(&G, 5);
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 9s 8h");
    set_hand(1, "4d 5d 6d 9h Th Jh Qs Qd Qc As");
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    CHECK(gin_script(&G, "k9s"));
    CHECK(r->kind == GR_UNDERCUT && r->winner == 1 && r->dead[0] == 8 && r->dead[1] == 1);
    CHECK(r->points == GIN_BONUS + 7 && P->score[1] == 32 && P->boxes[1] == 1 && P->turn == 1);
    /* gin: no layoffs, 25 and the defender's deadwood */
    gin_new(&G, 5);
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 5c 9s");
    set_hand(1, "6c 8c Kc 8h 9h Th Qd 2d 3d Js");
    CHECK(gin_script(&G, "k9s"));
    CHECK(r->kind == GR_GIN && r->winner == 0 && r->dead[0] == 0 && r->nlay == 0);
    CHECK(r->dead[1] == 6 + 8 + 10 + 10 + 2 + 3 + 10 && r->points == GIN_BONUS + r->dead[1]);
    /* the next hand: the deal alternates */
    CHECK(gin_script(&G, "n") && P->dealer == 0 && P->turn == 1 && P->phase == GN_OFFER && P->hands == 2);
}

static void test_dead_hand(void)
{
    gin_new(&G, 8);
    CHECK(gin_script(&G, "p p"));
    while (P->nstock > 3 && P->phase != GN_END && P->phase != GN_OVER) {   /* stock, throw the card drawn */
        int c = P->stock[P->nstock - 1];
        CHECK(gin_play(&G, GNA_STOCK) && gin_play(&G, GNA_DISCARD | c));
    }
    if (P->phase == GN_DRAW) {
        int c = P->stock[P->nstock - 1], dealer = P->dealer;
        CHECK(gin_play(&G, GNA_STOCK) && gin_play(&G, GNA_DISCARD | c));
        CHECK(P->phase == GN_END && P->res.kind == GR_DEAD && P->nstock == 2);
        CHECK(P->score[0] == 0 && P->score[1] == 0);
        CHECK(gin_script(&G, "n") && P->dealer == dealer && P->hands == 2);   /* the same dealer again */
    }
}

static void test_game_end(void)
{
    gin_new(&G, 5);
    P->score[0] = 90;
    P->boxes[0] = 3;
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 9s 5h");
    set_hand(1, "5c 6c 8h 9h Th 7c Qd Qs 2d 3d");
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    CHECK(gin_script(&G, "k9s") && P->phase == GN_OVER && gin_winner(P) == 0);
    CHECK(P->score[0] == 110 && P->boxes[0] == 4 && P->shutout);
    CHECK(P->final[0] == 2 * (110 + GIN_GAME_BONUS + 4 * GIN_BOX) && P->final[1] == 0);
    CHECK(!gin_legal(P, GNA_NEXT));
    gin_new(&G, 5);                             /* no shutout: the loser won a hand */
    P->score[0] = 90;
    P->boxes[0] = 3;
    P->score[1] = 30;
    P->boxes[1] = 1;
    set_hand(0, "2c 3c 4c 7h 7s 7d Kd Kh Ks 9s 5h");
    set_hand(1, "5c 6c 8h 9h Th 7c Qd Qs 2d 3d");
    P->phase = GN_DISCARD;
    P->turn = 0;
    P->taken = CARD_NONE;
    CHECK(gin_script(&G, "k9s") && !P->shutout);
    CHECK(P->final[0] == 110 + GIN_GAME_BONUS + 4 * GIN_BOX && P->final[1] == 30 + GIN_BOX);
}

static void test_undo(void)
{
    gin_new(&G, 21);
    CHECK(gin_script(&G, "p p s"));
    gnpos before = G.pos;
    int c = P->hand[0][3];
    CHECK(gin_play(&G, GNA_DISCARD | c));
    CHECK(gin_undo(&G) && !memcmp(&before, &G.pos, sizeof before));
    while (gin_undo(&G)) {}
    gin_new(&H, 21);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos) && G.n == 0);
    CHECK(!gin_script(&G, "x") && !gin_script(&G, "dZz"));
}

/* whole games, the plain play: every hand ends (a knock or a dead hand), the cards all there, the end at 100 */
static void test_games(void)
{
    int over = 0, gins = 0, undercuts = 0, deads = 0;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        gin_new(&G, seed);
        int steps = 0, ok = 1;
        while (P->phase != GN_OVER && steps++ < 20000 && ok) {
            int a = gin_auto(P);
            ok = gin_play(&G, a);
            if (P->phase == GN_END) {
                gins += P->res.kind == GR_GIN;
                undercuts += P->res.kind == GR_UNDERCUT;
                deads += P->res.kind == GR_DEAD;
            }
            if (P->phase == GN_DRAW || P->phase == GN_OFFER) {
                int seen[256] = {0}, n = 0;
                for (int q = 0; q < 2; q++)
                    for (int i = 0; i < P->n[q]; i++) seen[P->hand[q][i]]++, n++;
                for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++, n++;
                for (int i = 0; i < P->npile; i++) seen[P->pile[i]]++, n++;
                ok &= n == 52 && P->n[0] == 10 && P->n[1] == 10;
                for (int c = 0; c < 256; c++) ok &= seen[c] <= 1;
            }
        }
        CHECK(ok);
        if (P->phase == GN_OVER) {
            int w = gin_winner(P);
            over += w >= 0 && P->score[w] >= GIN_GOAL && P->score[1 - w] < GIN_GOAL && P->final[w] > P->final[1 - w];
        }
    }
    CHECK(over == 60);
    CHECK(gins > 0 && undercuts > 0);
    printf("ginrummy: 60 games, %d gins, %d undercuts, %d dead hands\n", gins, undercuts, deads);
}

int main(void)
{
    test_melds();
    test_offer();
    test_knock();
    test_dead_hand();
    test_game_end();
    test_undo();
    test_games();
    printf("ginrummy: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
