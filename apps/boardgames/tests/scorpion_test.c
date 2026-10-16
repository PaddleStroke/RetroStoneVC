/*
 * Scorpion: the rules' test (the deal; any face-up card moving with what lies on it onto the next card of its suit;
 * kings to empty columns; the reserve dealt once; a whole suit going off by itself; the taps; whole random games from
 * seeds; the hint; the levels). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/scorpion.h"
#include "games/scorpion_levels.h"
#include "levels_check.h"
#include "patience_check.h"

static void clear(void)
{
    scp_new(&G, 1);
    for (int i = 0; i < SCP_PILES; i++) P->n[i] = P->down[i] = 0;
}

static void put(int pile, int down, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
    P->down[pile] = (uint8_t)down;
}

static int legal(int a, int b, int n) { return scp_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    scp_new(&G, 42);
    scp_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int c = 0; c < 7; c++) CHECK(P->n[c] == 7 && P->down[c] == (c < 4 ? 3 : 0));
    CHECK(P->n[SCP_STOCK] == 3 && P->down[SCP_STOCK] == 3);
    for (int f = SCP_FOUND; f < SCP_STOCK; f++) CHECK(P->n[f] == 0);
    /* the reserve: its three cards face up on the first three columns, once */
    int top = P->c[SCP_STOCK][2];
    CHECK(scp_script(&G, "s") && P->n[SCP_STOCK] == 0 && sol_top(P, 0) == top && P->n[1] == 8 && P->n[2] == 8);
    CHECK(P->n[3] == 7 && scp_rules.stock(P).kind == SM_NONE && !scp_script(&G, "s"));
    CHECK(sol_undo(&G) && P->n[SCP_STOCK] == 3 && P->n[0] == 7);
}

static void test_moves(void)
{
    clear();
    /* column 0: a back, the 9 of spades, the 2 of hearts, the jack of clubs (not in order) */
    const int a[] = {S(HEARTS, 5), S(SPADES, 9), S(HEARTS, 2), S(CLUBS, 11)}, b[] = {S(SPADES, 10)},
              c[] = {S(CLUBS, 10)}, k[] = {S(DIAMONDS, 3), S(CLUBS, 13), S(HEARTS, 4)};
    put(0, 1, a, 4);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, k, 3);
    CHECK(legal(0, 1, 3));                      /* the 9 of spades with all that lies on it, onto the 10 of spades */
    CHECK(!legal(0, 2, 3));                     /* not onto the 10 of clubs: another suit */
    CHECK(!legal(0, 1, 4));                     /* not the face-down card */
    CHECK(!legal(0, 1, 1) && !legal(0, 1, 2));
    CHECK(legal(3, 4, 2) && !legal(0, 4, 3));   /* only a king to an empty column */
    CHECK(!legal(1, 0, 1));                     /* the 10 of spades on the jack of clubs: no */
    CHECK(!legal(0, SCP_FOUND, 1));             /* the foundations take only a whole suit */
    CHECK(scp_play(&G, sol_mv(SM_MOVE, 0, 1, 3)));
    CHECK(P->n[0] == 1 && P->down[0] == 0 && P->n[1] == 4 && sol_top(P, 1) == S(CLUBS, 11));   /* turned up */
    /* the tap: onto the card it follows; a king under cards to an empty column; nothing to a foundation */
    solmove m = scp_rules.tap(P, 1, 1, 0);
    CHECK(m.kind == SM_NONE);                   /* the 9 of spades sits on the 10 already: nowhere else */
    m = scp_rules.tap(P, 3, 1, 0);
    CHECK(m.kind == SM_MOVE && m.to == 4 && m.n == 2);
    CHECK(scp_rules.tap(P, 3, 1, 1).kind == SM_NONE);
    CHECK(scp_play(&G, m) && scp_rules.tap(P, 4, 0, 0).kind == SM_NONE);   /* a king at the bottom stays */
}

/* a whole suit, king to ace, on top of a column: off by itself; the four: won; undo takes it back with the move */
static void test_suits(void)
{
    clear();
    for (int s = 0; s < 4; s++)
        for (int r = 13; r >= 1; r--)
            if (!(s == 3 && r == 1)) P->c[s][P->n[s]++] = (uint8_t)CARD(s, r);
    P->c[4][P->n[4]++] = (uint8_t)CARD(3, 1);   /* the ace of the last suit on its own */
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(pc_whole(P, 1, 1));
    CHECK(!scp_won(P));
    CHECK(scp_script(&G, "54") && scp_won(P) && G.moves == 1 && sol_on_found(P) == 52);
    CHECK(sol_undo(&G) && !memcmp(&G.pos, &G.start, sizeof G.pos));
    /* a run of a suit out from the king down does not go off */
    clear();
    for (int r = 12; r >= 1; r--) P->c[0][P->n[0]++] = (uint8_t)CARD(SUIT_SPADES, r);
    const int king[] = {S(SPADES, 13)};
    put(1, 0, king, 1);
    CHECK(!legal(0, SCP_FOUND, 12) && legal(0, 1, 12));
    CHECK(scp_play(&G, sol_mv(SM_MOVE, 0, 1, 12)) && sol_on_found(P) == 13 && P->n[1] == 0);
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    for (int c = 0; c < 7; c++)
        if (p->down[c] > p->n[c] || (p->n[c] && p->down[c] == p->n[c])) return 0;
    for (int f = SCP_FOUND; f < SCP_STOCK; f++) {
        if (p->n[f] && p->n[f] != 13) return 0;
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != 13 - k || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    }
    return p->n[SCP_STOCK] == 0 || p->n[SCP_STOCK] == 3;
}

static void new_game(solrec *r, uint32_t seed) { scp_new(r, seed); }

static void test_levels(void)
{
    static solrec A, B;
    uint8_t deck[104];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, 77);
    scp_new(&A, 77);
    scp_deal_cards(&B, A.variant, deck);
    CHECK(!memcmp(&A.pos, &B.pos, sizeof A.pos));
    CHECK(lc_levels("scorpion", scp_levels, SCP_LEVELS, 52, scp_deal_cards, &scp_search, 300000) == SCP_LEVELS);
}

int main(void)
{
    test_deal();
    test_moves();
    test_suits();
    int ends, wins;
    pc_random(&scp_rules, new_game, sane, 300, 500, &ends, &wins);
    printf("ok   300 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&scp_rules, new_game, 200, 1500);
    printf("ok   the hint alone wins %d deals of 200\n", won);
    test_levels();
    printf("scorpion: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
