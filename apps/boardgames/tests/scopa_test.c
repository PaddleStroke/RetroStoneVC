/*
 * Scopa: the rules' test (the deal and its re-deal on three re, captures by value and by sum, a single card before a
 * sum, a card that can take must take, scopas and the last card's none, the table's last cards to the last taker,
 * cards, coins, settebello, primiera, the end at 11 and a tie going on, partnerships, undo, the notation; whole random
 * games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/scopa.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static scgame G, H;
static scpos *P = &G.pos;

static int card(const char *s) { return it_parse(&s); }

/* the cards of a string ("5d 2c 3s") into out; their number */
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

static int total_cards(const scpos *p)
{
    int n = p->ntable + p->nstock;
    for (int q = 0; q < p->players; q++) n += p->n[q] + p->ntook[q];
    return n;
}

static void test_deal(void)
{
    for (int players = 2; players <= 4; players += 2) {
        scopa_new(&G, players, 42);
        int seen[256] = {0}, ok = 1;
        for (int q = 0; q < players; q++) {
            ok &= P->n[q] == 3;
            for (int i = 0; i < 3; i++) seen[P->hand[q][i]]++;
        }
        for (int i = 0; i < P->ntable; i++) seen[P->table[i]]++;
        for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 10; r++) ok &= seen[CARD(s, r)] == 1;
        CHECK(ok && P->ntable == 4 && P->nstock == 40 - 4 - 3 * players && total_cards(P) == 40);
        CHECK(P->turn == 0 && P->dealer == players - 1 && P->phase == SC_PLAY);
    }
    int re3 = 0;                                /* never three re on the table at the deal */
    for (uint32_t seed = 1; seed < 3000; seed++) {
        scopa_new(&G, 2, seed);
        int re = 0;
        for (int i = 0; i < 4; i++) re += CARD_RANK(P->table[i]) == IT_RE;
        re3 += re >= 3;
    }
    CHECK(re3 == 0);
    scopa_new(&H, 2, 42);
    scopa_new(&G, 2, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

/* a position set by hand: the table, player 0's hand, player 1's; the stock left as it is unless empty */
static void set(const char *table, const char *h0, const char *h1, int stock)
{
    scopa_new(&G, 2, 7);
    P->ntable = (uint8_t)cards(table, P->table);
    P->n[0] = (uint8_t)cards(h0, P->hand[0]);
    P->n[1] = (uint8_t)cards(h1, P->hand[1]);
    if (!stock) P->nstock = 0;
    P->turn = 0;
}

static void test_captures(void)
{
    uint32_t m[64];
    set("5d 2c 3s 7b", "5c 7d Cs", "Rc 4b 1d", 1);
    CHECK(scopa_captures(P, card("5c"), m, 64) == 1 && m[0] == 1u);           /* the single 5, not 2 + 3 */
    CHECK(scopa_captures(P, card("Cs"), m, 64) == 1 && m[0] == (1u << 1 | 1u << 3));   /* 2 + 7 */
    CHECK(scopa_captures(P, card("Rc"), m, 64) == 2);                          /* 5 + 2 + 3, 3 + 7 */
    CHECK(!scopa_legal(P, SCA(card("5c"), 0)));                                /* it must take */
    CHECK(!scopa_legal(P, SCA(card("5c"), 1u << 1 | 1u << 2)));               /* not the sum */
    CHECK(scopa_legal(P, SCA(card("5c"), 1u)));
    CHECK(!scopa_legal(P, SCA(card("Rc"), 1u)));                               /* not player 0's card */
    set("1d 1c 4s", "6b 2d 5c", "Rc 4b 1s", 1);
    CHECK(scopa_captures(P, card("6b"), m, 64) == 1 && m[0] == 7u);           /* 1 + 1 + 4 */
    CHECK(scopa_captures(P, card("2d"), m, 64) == 1 && m[0] == 3u);            /* the two aces */
    CHECK(scopa_captures(P, card("5c"), m, 64) == 2);                          /* each ace with the 4 */
    set("2d 3c", "Cs 1b 4d", "Rc 4b 1s", 1);
    CHECK(scopa_captures(P, card("Cs"), m, 64) == 0 && scopa_legal(P, SCA(card("Cs"), 0)));
    CHECK(!scopa_legal(P, SCA(card("Cs"), 1u)));
    CHECK(scopa_play(&G, SCA(card("Cs"), 0)) && P->ntable == 3 && P->turn == 1 && P->n[0] == 2);
    /* the notation: names the cards taken; several sets, the first by default */
    set("5d 2c 3s 7b", "Rd 7d Cs", "Rc 4b 1d", 1);
    CHECK(scopa_script(&G, "Rd:3s7b") && P->ntable == 2 && P->ntook[0] == 3 && P->last_taker == 0);
    CHECK(P->table[0] == card("5d") && P->table[1] == card("2c"));
    CHECK(!scopa_script(&G, "Rc:5d"));
    CHECK(scopa_script(&G, "1d") && P->ntable == 3);                           /* nothing to take */
}

static void test_scopa(void)
{
    set("4d 3c", "7c 2b 5s", "Rc 4b 1d", 1);
    CHECK(scopa_script(&G, "7c") && P->ntable == 0 && P->scope[0] == 1 && P->lscopa);
    CHECK(P->scopa_card[0][0] == card("7c"));
    /* the very last card of the round clears the table: no scopa; the cards go to it anyway */
    set("4d", "4c", "Rc", 0);
    P->n[1] = 0;
    CHECK(scopa_script(&G, "4c") && P->phase != SC_PLAY && P->scope[0] == 0 && P->ntook[0] == 2);
    /* the last cards on the table go to the last taker, no scopa */
    set("4d Cb", "4c", "Rc", 0);
    CHECK(scopa_script(&G, "4c") && P->turn == 1 && P->last_taker == 0);
    CHECK(scopa_script(&G, "Rc") && P->phase != SC_PLAY && P->ntook[0] == 4 && P->ntook[1] == 0);
    CHECK(P->nswept == 2 && P->sweeper == 0 && P->scope[0] == 0);
}

/* the round's count: took[0] and took[1] set by hand */
static void count(const char *a, const char *b)
{
    scopa_new(&G, 2, 9);
    P->ntook[0] = (uint8_t)cards(a, P->took[0]);
    P->ntook[1] = (uint8_t)cards(b, P->took[1]);
}

static void test_primiera(void)
{
    CHECK(scopa_primiera_value(card("7d")) == 21 && scopa_primiera_value(card("6c")) == 18);
    CHECK(scopa_primiera_value(card("1s")) == 16 && scopa_primiera_value(card("5b")) == 15);
    CHECK(scopa_primiera_value(card("2d")) == 12 && scopa_primiera_value(card("Rd")) == 10);
    count("7d 7c 6s 1b Rd", "7s 7b 2d 3c");
    CHECK(scopa_primiera(P, 0) == 21 + 21 + 18 + 16 && scopa_primiera(P, 1) == 21 + 21 + 12 + 13);
    count("7d 7c 6s", "2d 2c 2s 2b");
    CHECK(scopa_primiera(P, 0) == 0 && scopa_primiera(P, 1) == 48);           /* no clubs: none */
}

/* a whole round's score from the cards each side took */
static void test_score(void)
{
    /* side 0: 21 cards, 6 coins, the settebello, the better primiera; side 1: two scopas */
    set("", "1d", "", 0);
    P->n[1] = 0;
    P->ntable = 1;
    P->table[0] = card("1c");                   /* 1d takes 1c: the last card of the round, no scopa */
    P->ntook[0] = (uint8_t)cards("7d 2d 3d 4d 5d 7c 7s 7b 6c 6s 6b 1s 1b 2c 2s 2b 3c 3s 3b", P->took[0]);
    P->ntook[1] = (uint8_t)cards("6d Fd Cd Rd 4c 4s 4b 5c 5s 5b Fc Fs Fb Cc Cs Cb Rc Rs Rb", P->took[1]);
    P->scope[1] = 2;
    CHECK(total_cards(P) == 40);
    CHECK(scopa_script(&G, "1d") && P->phase == SC_ROUND);
    CHECK(P->count[0][SCORE_CARDS] == 21 && P->count[1][SCORE_CARDS] == 19);
    CHECK(P->count[0][SCORE_COINS] == 6 && P->count[1][SCORE_COINS] == 4);
    CHECK(P->won[0][SCORE_CARDS] && P->won[0][SCORE_COINS] && P->won[0][SCORE_SETTEBELLO] && P->won[0][SCORE_PRIMIERA]);
    CHECK(!P->won[1][SCORE_CARDS] && P->rscope[1] == 2 && P->pts[0] == 4 && P->pts[1] == 2);
    CHECK(P->score[0] == 4 && P->score[1] == 2);
    /* a tie in cards (20 each): nobody's point */
    set("", "1d", "", 0);
    P->n[1] = 0;
    P->ntable = 1;
    P->table[0] = card("1c");
    P->ntook[0] = (uint8_t)cards("7d 2d 3d 4d 5d 7c 7s 7b 6c 6s 6b 1s 1b 2c 2s 2b 3c 3s", P->took[0]);
    P->ntook[1] = (uint8_t)cards("6d Fd Cd Rd 4c 4s 4b 5c 5s 5b Fc Fs Fb Cc Cs Cb Rc Rs Rb 3b", P->took[1]);
    CHECK(scopa_script(&G, "1d") && P->count[0][SCORE_CARDS] == 20);
    CHECK(!P->won[0][SCORE_CARDS] && !P->won[1][SCORE_CARDS]);
    /* the end: 11 reached; equal totals over 11 go on */
    P->score[0] = 7;
    P->score[1] = 11;
    P->phase = SC_ROUND;
    CHECK(scopa_script(&G, "n") && P->phase == SC_PLAY && P->round == 1 && P->score[1] == 11);
}

static void test_end(void)
{
    scopa_new(&G, 2, 5);
    G.pos.score[0] = 10;
    G.pos.score[1] = 10;
    int rounds = 0;
    while (P->phase != SC_OVER && rounds < 50) {
        CHECK(scopa_script(&G, "#"));
        if (P->phase == SC_ROUND) CHECK(P->score[0] == P->score[1] || (P->score[0] < 11 && P->score[1] < 11));
        if (P->phase == SC_ROUND) scopa_script(&G, "n");
        rounds++;
    }
    CHECK(P->phase == SC_OVER && P->score[0] != P->score[1] && scopa_winner(P) == (P->score[1] > P->score[0]));
}

/* random legal play: a random card and a random capture of it */
static uint32_t rnd(uint32_t *s) { return cards_rand(s); }

static void random_games(void)
{
    int games = 0, bad = 0, maxtable = 0, scopas = 0, overs = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        int players = seed % 3 == 0 ? 4 : 2;
        scopa_new(&G, players, seed * 7919u);
        uint32_t r = seed;
        int steps = 0;
        while (P->phase != SC_OVER && steps < 20000) {
            uint32_t act;
            if (P->phase == SC_ROUND) {
                int took = 0;
                for (int q = 0; q < players; q++) took += P->ntook[q];
                if (took != 40 || P->ntable || P->nstock) bad++;
                int sum = 0;
                for (int s = 0; s < 2; s++) {
                    int k = 0;
                    for (int j = 0; j < SCORE_KINDS; j++) k += P->won[s][j];
                    if (P->pts[s] != k + P->rscope[s]) bad++;
                    sum += k;
                }
                if (sum > 4) bad++;
                act = SCA_NEXT;
            } else {
                int q = P->turn, c = P->hand[q][rnd(&r) % P->n[q]];
                uint32_t m[64];
                int n = scopa_captures(P, c, m, 64);
                act = SCA(c, n ? m[rnd(&r) % n] : 0);
            }
            if (!scopa_play(&G, act)) { bad++; break; }
            if (total_cards(P) != 40 && P->phase == SC_PLAY) bad++;
            if (P->ntable > maxtable) maxtable = P->ntable;
            scopas += P->lscopa;
            steps++;
        }
        overs += P->phase == SC_OVER;
        if (P->phase == SC_OVER && (P->score[0] == P->score[1] || (P->score[0] < 11 && P->score[1] < 11))) bad++;
        if (seed % 40 == 0) {                    /* undo replays to the same position */
            H = G;
            if (scopa_undo(&G)) {
                scopa_play(&G, H.act[H.n - 1]);
                if (memcmp(&G.pos, &H.pos, sizeof H.pos)) bad++;
            }
        }
        games++;
    }
    CHECK(bad == 0 && overs == games && scopas > 0);
    CHECK(maxtable < SCOPA_TABLE);
    printf("scopa: %d random games, the table at most %d cards, %d scopas\n", games, maxtable, scopas);
}

static void test_partners(void)
{
    scopa_new(&G, 4, 11);
    CHECK(scopa_side(P, 0) == 0 && scopa_side(P, 2) == 0 && scopa_side(P, 1) == 1 && scopa_side(P, 3) == 1);
    CHECK(scopa_script(&G, "!") && P->phase == SC_OVER);
    scopa_new(&G, 4, 11);
    CHECK(scopa_script(&G, ". . . ."));
    CHECK(P->turn == 0 && P->n[0] == 2 && P->n[3] == 2);
}

int main(void)
{
    test_deal();
    test_captures();
    test_scopa();
    test_primiera();
    test_score();
    test_end();
    test_partners();
    random_games();
    printf("scopa: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
