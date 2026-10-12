/*
 * Briscola: the rules' test (the deal and the turned briscola, the cards' order and points, a trick won by the
 * highest trump or the highest card of the suit led, no need to follow suit, the draw from the winner on and the
 * briscola drawn last, the end and its count, a draw at 60, partnerships, undo, the notation; whole random games
 * from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/briscola.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static brgame G, H;
static brpos *P = &G.pos;

static int card(const char *s) { return it_parse(&s); }

static int cards(const char *s, uint8_t *out)
{
    int n = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int c = it_parse(&s);
        if (c == CARD_NONE) break;
        out[n++] = (uint8_t)c;
    }
    return n;
}

static int total_cards(const brpos *p)
{
    int n = p->nstock + p->ntrick + p->ntook[0] + p->ntook[1];
    for (int q = 0; q < p->players; q++) n += p->n[q];
    return n;
}

static void test_deal(void)
{
    for (int players = 2; players <= 4; players += 2) {
        briscola_new(&G, players, 99);
        int seen[256] = {0}, ok = 1;
        for (int q = 0; q < players; q++) {
            ok &= P->n[q] == 3;
            for (int i = 0; i < 3; i++) seen[P->hand[q][i]]++;
        }
        for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 10; r++) ok &= seen[CARD(s, r)] == 1;
        CHECK(ok && P->nstock == 40 - 3 * players && P->stock[0] == P->briscola);
        CHECK(P->trump == CARD_SUIT(P->briscola) && P->turn == 0 && P->phase == BR_PLAY);
    }
    briscola_new(&H, 2, 99);
    briscola_new(&G, 2, 99);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_order(void)
{
    CHECK(briscola_points(card("1d")) == 11 && briscola_points(card("3c")) == 10 && briscola_points(card("Rs")) == 4);
    CHECK(briscola_points(card("Cb")) == 3 && briscola_points(card("Fd")) == 2 && briscola_points(card("7d")) == 0);
    int total = 0;
    uint8_t all[40];
    it_pack(all);
    for (int i = 0; i < 40; i++) total += briscola_points(all[i]);
    CHECK(total == BRISCOLA_TOTAL);
    const char *order[10] = {"1c", "3c", "Rc", "Cc", "Fc", "7c", "6c", "5c", "4c", "2c"};
    int ok = 1;
    for (int i = 0; i < 9; i++) ok &= briscola_strength(card(order[i])) > briscola_strength(card(order[i + 1]));
    CHECK(ok);
    uint8_t t[4];
    cards("3c 1c", t);
    CHECK(briscola_trick_winner(t, 2, IT_SWORDS) == 1);    /* the ace over the three */
    cards("1c 2s", t);
    CHECK(briscola_trick_winner(t, 2, IT_SWORDS) == 1);    /* the small trump over the ace */
    cards("2c 1d", t);
    CHECK(briscola_trick_winner(t, 2, IT_SWORDS) == 0);    /* another suit, no trump: the leader */
    cards("5b 3s Rs 1b", t);
    CHECK(briscola_trick_winner(t, 4, IT_SWORDS) == 1);    /* the highest trump */
    cards("5b 7b 4c Rb", t);
    CHECK(briscola_trick_winner(t, 4, IT_SWORDS) == 3);
}

/* hands set by hand, player 0 to lead; the stock's top last */
static void set(const char *h0, const char *h1, const char *stock, int trump)
{
    briscola_new(&G, 2, 5);
    P->n[0] = (uint8_t)cards(h0, P->hand[0]);
    P->n[1] = (uint8_t)cards(h1, P->hand[1]);
    P->nstock = (uint8_t)cards(stock, P->stock);
    P->trump = (uint8_t)trump;
    P->briscola = P->nstock ? P->stock[0] : CARD_NONE;
}

static void test_tricks(void)
{
    set("1c 2d 3b", "4s 5s 6s", "Rd 7c 7d 7s", IT_COINS);
    CHECK(briscola_legal(P, card("1c")) && !briscola_legal(P, card("4s")));
    CHECK(briscola_script(&G, "1c") && P->turn == 1 && P->ntrick == 1);
    CHECK(briscola_legal(P, card("4s")));       /* no need to follow suit */
    CHECK(briscola_script(&G, "4s") && P->last_winner == 0 && P->points[0] == 11 && P->points[1] == 0);
    CHECK(P->ndrew == 2 && P->drew[0] == card("7s") && P->drew[1] == card("7d") && P->turn == 0 && P->nstock == 2);
    CHECK(P->hand[0][2] == card("7s") && P->hand[1][2] == card("7d"));
    CHECK(briscola_script(&G, "2d 7d") && P->last_winner == 1 && P->turn == 1 && P->tricks == 2);
    CHECK(P->drew[0] == card("7c") && P->drew[1] == card("Rd") && P->nstock == 0);   /* the briscola last */
    CHECK(briscola_script(&G, "5s Rd") && P->last_winner == 0 && P->points[0] == 15);   /* the re of trumps */
    CHECK(P->ndrew == 0 && P->n[0] == 2 && P->n[1] == 2 && P->turn == 0);
    CHECK(briscola_script(&G, "3b 6s") && P->points[0] == 25 && P->turn == 0);   /* the leader's suit */
    CHECK(briscola_script(&G, "7s 7c") && P->phase == BR_OVER && P->points[1] == 0);
    CHECK(briscola_winner(P) == 0 && !briscola_legal(P, card("1c")));
}

static void test_draw_and_partners(void)
{
    /* 60 each: a draw */
    set("1c 3d", "1d 2s", "", IT_SWORDS);
    P->points[0] = 38;
    P->points[1] = 50;
    CHECK(briscola_script(&G, "1c 1d 3d 2s") && P->phase == BR_OVER);
    CHECK(P->points[0] == 60 && P->points[1] == 60 && briscola_winner(P) == 2);
    /* four players: partners count together, the next trick's leader is the winner */
    briscola_new(&G, 4, 3);
    CHECK(briscola_side(P, 0) == 0 && briscola_side(P, 2) == 0 && briscola_side(P, 1) == 1 && briscola_side(P, 3) == 1);
    CHECK(briscola_script(&G, ". . . .") && P->tricks == 1 && P->ndrew == 4 && P->turn == P->last_winner);
    CHECK(P->nstock == 40 - 12 - 4 && total_cards(P) == 40);
    CHECK(briscola_script(&G, "#") && P->phase == BR_OVER && P->tricks == 10 && P->points[0] + P->points[1] == 120);
}

static void random_games(void)
{
    int bad = 0, games = 0, draws = 0, wins[2] = {0, 0};
    for (uint32_t seed = 1; seed <= 600; seed++) {
        int players = seed % 3 == 0 ? 4 : 2;
        briscola_new(&G, players, seed * 104729u);
        uint32_t r = seed;
        int steps = 0;
        while (P->phase == BR_PLAY && steps < 100) {
            int q = P->turn;
            if (!P->n[q] || !briscola_play(&G, P->hand[q][cards_rand(&r) % P->n[q]])) { bad++; break; }
            if (total_cards(P) != 40) bad++;
            if (P->ntrick == 0 && P->nstock == 0 && P->tricks < 40 / players)   /* the stock gone: hands even */
                for (int k = 0; k < players; k++) if (P->n[k] != P->n[0]) bad++;
            steps++;
        }
        if (P->phase != BR_OVER || steps != 40 || P->tricks != 40 / players) bad++;
        if (P->points[0] + P->points[1] != BRISCOLA_TOTAL || P->ntook[0] + P->ntook[1] != 40) bad++;
        int w = briscola_winner(P);
        if (w == 2) draws++;
        else if (w >= 0) wins[w]++;
        if ((w == 2) != (P->points[0] == 60)) bad++;
        if (seed % 50 == 0) {                   /* undo replays to the same position */
            H = G;
            if (briscola_undo(&G)) {
                briscola_play(&G, H.act[H.n - 1]);
                if (memcmp(&G.pos, &H.pos, sizeof H.pos)) bad++;
            }
        }
        games++;
    }
    CHECK(bad == 0 && wins[0] > 0 && wins[1] > 0);
    printf("briscola: %d random games, %d / %d / %d draws\n", games, wins[0], wins[1], draws);
}

int main(void)
{
    test_deal();
    test_order();
    test_tricks();
    test_draw_and_partners();
    random_games();
    printf("briscola: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
