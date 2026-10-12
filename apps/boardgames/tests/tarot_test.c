/*
 * French Tarot: the rules' test (the deal in threes and the dog, the Petit sec, the bidding, the dog and the discard,
 * the king called, following, trumping, over-trumping, the Excuse, the count and the scores, petit au bout, poignées,
 * the chelem; whole random games from seeds for 3, 4 and 5 players; undo; no shuffle between deals: the gathering,
 * the cut, the warm-up deal, how bunched the hands are against shuffled ones). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tarot.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static tgame G, H;
static tpos *P = &G.pos;

#define S(r) CARD(SUIT_SPADES, r)
#define HE(r) CARD(SUIT_HEARTS, r)
#define D(r) CARD(SUIT_DIAMONDS, r)
#define C(r) CARD(SUIT_CLUBS, r)
#define T(r) CARD(SUIT_TRUMP, r)
#define EX TAROT_EXCUSE

static int has(const tpos *p, int pl, int c)
{
    for (int i = 0; i < p->nhand[pl]; i++)
        if (p->hand[pl][i] == c) return 1;
    return 0;
}

static void set_hand(int pl, const int *cards, int n)
{
    P->nhand[pl] = (uint8_t)n;
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cards[i];
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static void test_deal(void)
{
    for (int n = 3; n <= 5; n++) {
        tarot_new(&G, n, 42);
        tarot_new(&H, n, 42);
        CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
        int hs = n == 3 ? 24 : n == 4 ? 18 : 15, dogn = n == 5 ? 3 : 6;
        CHECK(P->handsize == hs && P->dogsize == dogn && P->deals == n && P->dealer == n - 1);
        int seen[256] = {0}, total = 0, sorted = 1;
        for (int q = 0; q < n; q++) {
            CHECK(P->nhand[q] == hs);
            for (int i = 0; i < P->nhand[q]; i++, total++) {
                seen[P->hand[q][i]]++;
                if (i && tarot_key(P->hand[q][i - 1]) > tarot_key(P->hand[q][i])) sorted = 0;
            }
        }
        for (int i = 0; i < dogn; i++, total++) seen[P->dog[i]]++;
        uint8_t pack[CARDS_MAX];
        int once = 1;
        for (int i = 0; i < cards_pack(PACK_TAROT, pack); i++) once &= seen[pack[i]] == 1;
        CHECK(total == 78 && once && sorted);
        /* the deal in threes: packets to each player in turn from the dealer's next, the dog's cards one at a time,
         * never in the first nor the last place */
        int q = 0, packet = 0, dogs = 0, ok = 1;
        while (q < 78) {
            if (P->deal_to[q] == TAROT_DOG) {
                ok &= q > 3 && q < 75 && P->deal_to[q - 1] != TAROT_DOG;
                dogs++;
                q++;
                continue;
            }
            int pl = (P->dealer + 1 + packet) % n;
            for (int k = 0; k < 3; k++) ok &= P->deal_to[q + k] == pl;
            q += 3;
            packet++;
        }
        CHECK(ok && dogs == dogn && packet == (78 - dogn) / 3 && P->deal_to[77] != TAROT_DOG);
        tarot_new(&H, n, 43);
        CHECK(memcmp(H.pos.hand, G.pos.hand, sizeof G.pos.hand) != 0);
    }
}

/* the Petit sec: the Petit as the only trump, no Excuse: the deal is cancelled, the next dealer deals */
static void test_petit_sec(void)
{
    int found = 0;
    for (uint32_t seed = 1; seed < 20000 && !found; seed++) {
        tarot_new(&G, 4, seed);
        if (P->phase != TP_VOID) continue;
        found = 1;
        int pl = P->sec;
        CHECK(pl < 4 && has(P, pl, TAROT_PETIT) && tarot_trumps(P, pl) == 1);
        CHECK(!tarot_legal(P, TACT(TA_BID, TB_PASS)) && tarot_legal(P, TACT(TA_NEXT, 0)));
        int d = P->dealer;
        CHECK(tarot_play(&G, TACT(TA_NEXT, 0)) && P->dealer == (d + 1) % 4 && P->dealno == 1 && P->played == 0);
    }
    CHECK(found);
    tarot_new(&G, 4, 5);                        /* the Petit with the Excuse is not sec */
    if (P->phase == TP_BID) {
        const int h[3] = {S(2), T(1), EX};
        set_hand(0, h, 3);
        CHECK(tarot_trumps(P, 0) == 2);
    }
}

static void fresh(int n, uint32_t seed)          /* a deal that is not cancelled */
{
    for (;; seed++) {
        tarot_new(&G, n, seed);
        if (P->phase == TP_BID) return;
    }
}

/* ---- the bidding ------------------------------------------------------------------------------------------------- */
static void test_bidding(void)
{
    fresh(4, 7);
    CHECK(P->turn == 0);
    CHECK(tarot_play(&G, TACT(TA_BID, TB_GARDE)) && P->turn == 1);
    CHECK(!tarot_legal(P, TACT(TA_BID, TB_PETITE)) && !tarot_legal(P, TACT(TA_BID, TB_GARDE)));
    CHECK(tarot_legal(P, TACT(TA_BID, TB_SANS)) && tarot_legal(P, TACT(TA_BID, TB_PASS)));
    CHECK(tarot_script(&G, "p b3 p"));
    CHECK(P->taker == 2 && P->contract == TB_SANS && P->phase == TP_PLAY && P->turn == 0 && P->leader == 0);
    CHECK(P->necart == 6 && P->aside_attack == 1 && P->nhand[2] == 18);   /* Garde sans: the dog to the taker */
    fresh(4, 7);
    CHECK(tarot_script(&G, "p p p b4") && P->contract == TB_CONTRE && P->aside_attack == 0 && P->phase == TP_PLAY);
    fresh(4, 7);
    CHECK(tarot_script(&G, "p p p p") && P->phase == TP_VOID && P->sec == TAROT_NONE && P->turn == 3);
    CHECK(tarot_script(&G, "n") && P->dealer == 0 && P->dealno == 1 && (P->phase == TP_VOID || P->turn == 1));
    CHECK(P->played == 0);
}

/* ---- the dog and the discard ------------------------------------------------------------------------------------- */
static void test_discard(void)
{
    fresh(4, 11);
    CHECK(tarot_script(&G, "b2 p p p") && P->phase == TP_DOG && P->turn == 0 && P->taker == 0);
    CHECK(!tarot_legal(P, TACT(TA_PLAY, P->hand[0][0])));
    CHECK(tarot_script(&G, "t") && P->phase == TP_DISCARD && P->nhand[0] == 24);
    /* a hand of our own: kings, oudlers, trumps, plain cards */
    const int h[24] = {S(14), S(2), S(3), HE(14), HE(5), C(9), D(13), T(1), T(2), T(3), T(4), T(5), T(6), T(7), T(8),
                       T(9), T(10), T(11), T(12), T(13), T(14), T(15), T(21), EX};
    set_hand(0, h, 24);
    CHECK(!tarot_legal(P, TACT(TA_DISCARD, S(14))));          /* no king */
    CHECK(!tarot_legal(P, TACT(TA_DISCARD, T(1))) && !tarot_legal(P, TACT(TA_DISCARD, T(21))) &&
          !tarot_legal(P, TACT(TA_DISCARD, EX)));               /* no oudler */
    CHECK(tarot_legal(P, TACT(TA_DISCARD, D(13))));            /* a queen will do */
    /* five plain cards (no kings): S2 S3 H5 C9 D13; one trump is forced, no more */
    CHECK(tarot_legal(P, TACT(TA_DISCARD, T(2))));
    CHECK(tarot_script(&G, "xs2 xs3 xh5 xc9"));
    CHECK(!tarot_legal(P, TACT(TA_DONE, 0)));
    CHECK(tarot_script(&G, "xa2") && !tarot_legal(P, TACT(TA_DISCARD, T(3))));
    CHECK(tarot_script(&G, "xd13") && P->necart == 6 && tarot_legal(P, TACT(TA_DONE, 0)));   /* six: full */
    CHECK(tarot_script(&G, "rs3") && P->necart == 5 && has(P, 0, S(3)));          /* taken back */
    CHECK(!tarot_legal(P, TACT(TA_DISCARD, T(3))));             /* a plain card is back: no second trump */
    CHECK(tarot_script(&G, "xs3 f") && P->phase == TP_PLAY && P->turn == 0 && P->nhand[0] == 18);
    /* forced to discard two trumps: four plain cards only */
    fresh(4, 11);
    CHECK(tarot_script(&G, "b1 p p p t"));
    const int h2[24] = {S(14), S(2), HE(14), HE(5), C(9), D(14), C(14), T(1), T(2), T(3), T(4), T(5), T(6), T(7),
                        T(8), T(9), T(10), T(11), T(12), T(13), T(14), T(15), T(21), C(1)};
    set_hand(0, h2, 24);
    CHECK(tarot_script(&G, "xs2 xh5 xc9 xc1 xa3 xa2") && P->necart == 6);
    CHECK(tarot_script(&G, "ra3") && !tarot_legal(P, TACT(TA_DISCARD, S(14))) && tarot_script(&G, "xa20") == 0);
    CHECK(tarot_script(&G, "xa4 f") && P->phase == TP_PLAY);
}

/* ---- five players: the king called, the partner ----------------------------------------------------------------- */
static void test_call(void)
{
    fresh(5, 3);
    CHECK(tarot_script(&G, "b1 p p p p") && P->phase == TP_CALL && P->turn == 0);
    CHECK(tarot_call_rank(P) == TAROT_KING && !tarot_legal(P, TACT(TA_CALL, S(13))));
    int k = CARD(SUIT_HEARTS, TAROT_KING), holder = 0;
    for (int q = 0; q < 5; q++)
        if (has(P, q, k)) holder = q;
    int in_dog = 0;
    for (int i = 0; i < 3; i++) in_dog |= P->dog[i] == k;
    CHECK(tarot_play(&G, TACT(TA_CALL, k)) && P->called == k && P->phase == TP_DOG);
    CHECK(P->partner == (in_dog ? 0 : holder));
    CHECK(tarot_attack(P, 0) && tarot_attack(P, P->partner) && !P->revealed);
    /* all four kings: a queen is called */
    fresh(5, 3);
    CHECK(tarot_script(&G, "b1 p p p p"));
    const int h[15] = {S(14), HE(14), C(14), D(14), S(2), S(3), S(4), S(5), S(6), S(7), S(8), S(9), S(10), S(11), T(5)};
    set_hand(0, h, 15);
    CHECK(tarot_call_rank(P) == TAROT_QUEEN && tarot_legal(P, TACT(TA_CALL, D(13))));
    CHECK(!tarot_legal(P, TACT(TA_CALL, D(14))));
}

/* ---- following, trumping, the Excuse ----------------------------------------------------------------------------- */
static void play_setup(int n)
{
    fresh(n, 21);
    CHECK(tarot_script(&G, n == 5 ? "b4 p p p p ks14" : "b4 p p p"));
    CHECK(P->phase == TP_PLAY);
}

static void test_follow(void)
{
    play_setup(4);
    const int h0[3] = {S(5), S(9), T(4)}, h1[4] = {S(2), HE(3), T(6), T(12)}, h2[3] = {HE(1), T(3), T(10)};
    const int h3[3] = {HE(9), D(4), C(2)};
    set_hand(0, h0, 3);
    set_hand(1, h1, 4);
    set_hand(2, h2, 3);
    set_hand(3, h3, 3);
    P->turn = P->leader = 0;
    CHECK(tarot_legal(P, TACT(TA_PLAY, T(4))));                 /* the lead: anything */
    CHECK(tarot_script(&G, "s9"));
    CHECK(tarot_legal(P, TACT(TA_PLAY, S(2))) && !tarot_legal(P, TACT(TA_PLAY, HE(3))) &&
          !tarot_legal(P, TACT(TA_PLAY, T(6))));                /* follow the suit */
    const int h1b[3] = {HE(3), T(6), T(12)};
    set_hand(1, h1b, 3);
    CHECK(!tarot_legal(P, TACT(TA_PLAY, HE(3))) && tarot_legal(P, TACT(TA_PLAY, T(6))));   /* void: trump */
    CHECK(tarot_script(&G, "a12"));
    CHECK(tarot_legal(P, TACT(TA_PLAY, T(10))) && !tarot_legal(P, TACT(TA_PLAY, HE(1))) &&
          tarot_legal(P, TACT(TA_PLAY, T(3))));   /* cannot beat the 12: any lower trump */
    CHECK(tarot_script(&G, "a3"));
    CHECK(tarot_legal(P, TACT(TA_PLAY, HE(9))) && tarot_legal(P, TACT(TA_PLAY, C(2))));     /* no trump: anything */
    CHECK(tarot_script(&G, "c2") && P->tricks == 1 && P->lastwin == 1 && P->turn == 1 && P->won[1] == 1);
    CHECK(P->npile[1] == 4);
    /* trumps led: a higher one when you can */
    play_setup(4);
    const int g0[2] = {T(8), S(4)}, g1[3] = {T(5), T(9), S(1)}, g2[2] = {EX, T(2)}, g3[2] = {D(2), D(3)};
    set_hand(0, g0, 2);
    set_hand(1, g1, 3);
    set_hand(2, g2, 2);
    set_hand(3, g3, 2);
    P->turn = P->leader = 0;
    CHECK(tarot_script(&G, "a8") && !tarot_legal(P, TACT(TA_PLAY, T(5))) && tarot_legal(P, TACT(TA_PLAY, T(9))) &&
          !tarot_legal(P, TACT(TA_PLAY, S(1))));
    CHECK(tarot_script(&G, "a9"));
    CHECK(tarot_legal(P, TACT(TA_PLAY, EX)) && tarot_legal(P, TACT(TA_PLAY, T(2))));      /* the Excuse: any time */
    CHECK(tarot_script(&G, "e d2") && P->lastwin == 1);
    CHECK(P->npile[2] == 1 && P->pile[2][0] == EX && P->npile[1] == 3);   /* its owner keeps it */
    /* the Excuse led: the next card sets the suit */
    play_setup(4);
    const int e0[2] = {EX, S(3)}, e1[2] = {HE(4), S(5)}, e2[2] = {HE(10), T(2)}, e3[2] = {S(13), T(1)};
    set_hand(0, e0, 2);
    set_hand(1, e1, 2);
    set_hand(2, e2, 2);
    set_hand(3, e3, 2);
    P->turn = P->leader = 0;
    CHECK(tarot_script(&G, "e") && tarot_legal(P, TACT(TA_PLAY, HE(4))) && tarot_legal(P, TACT(TA_PLAY, S(5))));
    CHECK(tarot_script(&G, "h4") && !tarot_legal(P, TACT(TA_PLAY, T(2))) && tarot_script(&G, "h10"));
    CHECK(tarot_legal(P, TACT(TA_PLAY, T(1))) && !tarot_legal(P, TACT(TA_PLAY, S(13))));  /* void in hearts: trump */
    const uint8_t tr[4] = {EX, HE(4), HE(10), S(13)};
    CHECK(tarot_trick_winner(P, tr, 4, 0) == 2);
    const uint8_t tr2[4] = {S(3), S(14), T(1), S(12)};
    CHECK(tarot_trick_winner(P, tr2, 4, 1) == 3);                /* the Petit trumps the king */
}

/* ---- the count --------------------------------------------------------------------------------------------------- */
static void test_values(void)
{
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_TAROT, pack), sum = 0, oud = 0;
    for (int i = 0; i < n; i++) sum += tarot_value2(pack[i]), oud += tarot_oudler(pack[i]);
    CHECK(n == 78 && sum == 182 && oud == 3);
    CHECK(tarot_value2(S(14)) == 9 && tarot_value2(S(13)) == 7 && tarot_value2(S(12)) == 5 && tarot_value2(S(11)) == 3);
    CHECK(tarot_value2(S(10)) == 1 && tarot_value2(T(20)) == 1 && tarot_value2(T(21)) == 9 && tarot_value2(EX) == 9);
    CHECK(tarot_need(0) == 56 && tarot_need(1) == 51 && tarot_need(2) == 41 && tarot_need(3) == 36);
    CHECK(tarot_mult(TB_PETITE) == 1 && tarot_mult(TB_GARDE) == 2 && tarot_mult(TB_SANS) == 4 &&
          tarot_mult(TB_CONTRE) == 6);
    CHECK(tarot_poignee_size(4, 1) == 10 && tarot_poignee_size(4, 2) == 13 && tarot_poignee_size(4, 3) == 15);
    CHECK(tarot_poignee_size(3, 1) == 13 && tarot_poignee_size(3, 3) == 18 && tarot_poignee_size(5, 1) == 8 &&
          tarot_poignee_size(5, 3) == 13);
    CHECK(tarot_poignee_bonus(1) == 20 && tarot_poignee_bonus(2) == 30 && tarot_poignee_bonus(3) == 40);
}

/* a deal brought to its last trick: the cards won so far given out by hand (the rest of the pack to the defence) */
static void last_trick(int n, int contract, const int *taker_won, int ntw, const int *hands)
{
    fresh(n, 31);
    char bids[32] = "";
    static const char *b[5] = {"", "b1", "b2", "b3", "b4"};
    strcat(bids, b[contract]);
    for (int q = 1; q < n; q++) strcat(bids, " p");
    CHECK(tarot_script(&G, bids));
    if (n == 5) CHECK(tarot_script(&G, "ks14"));
    if (contract <= TB_GARDE) CHECK(tarot_script(&G, "t"));
    while (P->phase == TP_DISCARD) CHECK(tarot_play(&G, tarot_auto(P)));
    CHECK(P->phase == TP_PLAY);
    int used[256] = {0};
    for (int q = 0; q < n; q++) used[hands[q]] = 1;
    for (int i = 0; i < ntw; i++) used[taker_won[i]] = 1;
    for (int q = 0; q < n; q++) P->npile[q] = 0, P->won[q] = 0;
    for (int i = 0; i < ntw; i++) P->pile[0][P->npile[0]++] = (uint8_t)taker_won[i];
    P->necart = 0;
    P->aside_attack = 0;
    uint8_t pack[CARDS_MAX];
    cards_pack(PACK_TAROT, pack);
    int d = P->partner == 1 ? 2 : 1;            /* the defence's pile: a defender's */
    for (int i = 0; i < 78; i++)
        if (!used[pack[i]]) P->pile[d][P->npile[d]++] = pack[i];
    P->won[0] = (uint8_t)(P->handsize / 2);
    P->won[d] = (uint8_t)(P->handsize - 1 - P->won[0]);
    P->tricks = (uint8_t)(P->handsize - 1);
    P->xowner = P->xto = TAROT_NONE;
    for (int q = 0; q < n; q++) {
        P->nhand[q] = 1;
        P->hand[q][0] = (uint8_t)hands[q];
        P->poignee[q] = 0;
    }
    P->turn = P->leader = 0;
    P->ntrick = 0;
}

static int sum_scores(void)
{
    int s = 0;
    for (int q = 0; q < P->players; q++) s += P->score[q];
    return s;
}

static void test_score(void)
{
    /* Garde, four players: the taker wins the last trick with the 21 and has the Petit: two oudlers, 41 needed.
     * Taker's cards: 21 (4.5), Petit (4.5), 4 kings (18), the last trick's three others */
    const int won[5] = {T(1), S(14), HE(14), D(14), C(14)};
    const int hands[4] = {T(21), S(2), S(3), S(4)};
    last_trick(4, TB_GARDE, won, 5, hands);
    CHECK(tarot_script(&G, "a21 s2 s3 s4"));
    CHECK(P->phase == TP_SCORE && P->played == 1);
    /* 21, Petit, 4 kings: 9 * 6 = 54 half points, three low cards 3: 57 = 28.5; 41 needed: lost by 12.5, 13 */
    CHECK(P->res.half == 57 && P->res.oudlers == 2 && P->res.need == 41 && !P->res.made && P->res.diff == 13);
    CHECK(P->res.pab == 0 && P->res.mult == 2);
    CHECK(P->res.unit == -(25 + 13) * 2);
    CHECK(P->score[0] == 3 * P->res.unit && P->score[1] == -P->res.unit && sum_scores() == 0);
    CHECK(P->total[0] == P->score[0]);
    /* petit au bout, won by the taker (Petite): +10; 56 needed without oudlers... the Petit counts */
    int many[40], m = 0;
    for (int r = 11; r <= 14; r++)
        for (int s = 0; s < 4; s++) many[m++] = CARD(s, r);
    for (int r = 2; r <= 9; r++) many[m++] = T(r);   /* 16 courts (9+7+5+3)*4 = 96, 8 trumps 8: 104 half = 52 */
    const int hands2[4] = {T(1), S(2), S(3), S(4)};
    last_trick(4, TB_PETITE, many, m, hands2);
    CHECK(tarot_script(&G, "a1 s2 s3 s4"));
    /* 104 + Petit 9 + three low 3 = 116 half = 58; one oudler: 51; won by 7 */
    CHECK(P->res.half == 116 && P->res.oudlers == 1 && P->res.made && P->res.diff == 7 && P->res.pab == 10);
    CHECK(P->res.unit == (25 + 7) * 1 + 10 && P->score[0] == 3 * 42 && sum_scores() == 0);
    /* the Petit caught in the last trick by the defence: the bonus is theirs */
    const int hands3[4] = {T(1), T(5), S(3), S(4)};
    last_trick(4, TB_GARDE, many, m, hands3);
    CHECK(tarot_script(&G, "a1 a5 s3 s4") && P->res.pab == -10 && P->lastwin == 1);
    /* 104 half; no oudler: 56 needed: 52 lost by 4 */
    CHECK(P->res.half == 104 && P->res.oudlers == 0 && !P->res.made && P->res.diff == 4);
    CHECK(P->res.unit == -(25 + 4) * 2 - 10 * 2 && sum_scores() == 0);
    /* a half point: three players (tricks of three) */
    const int won3[3] = {T(21), S(14), S(13)};
    const int hands4[3] = {HE(14), HE(2), HE(3)};
    last_trick(3, TB_PETITE, won3, 3, hands4);
    CHECK(tarot_script(&G, "h14 h2 h3"));
    /* 9 + 9 + 7 + 9 + 1 + 1 = 36 half = 18; one oudler: 51: lost by 33 */
    CHECK(P->res.half == 36 && !P->res.made && P->res.diff == 33 && P->score[0] == 2 * P->res.unit);
    const int won4[2] = {T(21), S(1)};
    last_trick(3, TB_PETITE, won4, 2, hands4);
    CHECK(tarot_script(&G, "h14 h2 h3"));
    CHECK(P->res.half == 9 + 1 + 11 && P->res.diff == (102 - 21 + 1) / 2);   /* 10.5: lost by 40.5 -> 41 */
    CHECK(P->res.diff == 41);
}

static void test_five(void)
{
    /* five players, a partner: the taker twice, the partner once, each defender pays once */
    const int won[1] = {T(21)};
    const int hands[5] = {T(5), S(2), S(3), S(4), S(6)};
    last_trick(5, TB_GARDE, won, 1, hands);
    CHECK(tarot_script(&G, "a5 s2 s3 s4 s6") && P->phase == TP_SCORE);
    int u = P->res.unit;
    CHECK(sum_scores() == 0);
    if (P->partner != 0) CHECK(P->score[0] == 2 * u && P->score[P->partner] == u);
    else CHECK(P->score[0] == 4 * u);
    for (int q = 1; q < 5; q++)
        if (q != P->partner) CHECK(P->score[q] == -u);
    /* the partner shows once the called card is played */
    fresh(5, 41);
    CHECK(tarot_script(&G, "b4 p p p p ks14") && !P->revealed);
    while (P->phase == TP_PLAY && !P->revealed) tarot_play(&G, tarot_auto(P));
    CHECK(P->revealed || P->partner == 0);
}

/* ---- the Excuse: the half point, the last trick, the chelem ----------------------------------------------------- */
static void test_excuse(void)
{
    /* played by the taker in a trick the defence wins: the taker keeps it, gives a half point */
    play_setup(4);
    const int a0[2] = {EX, S(5)}, a1[2] = {S(3), S(6)}, a2[2] = {S(13), S(7)}, a3[2] = {S(2), S(8)};
    set_hand(0, a0, 2);
    set_hand(1, a1, 2);
    set_hand(2, a2, 2);
    set_hand(3, a3, 2);
    P->turn = P->leader = 0;
    P->tricks = (uint8_t)(P->handsize - 2);
    CHECK(tarot_script(&G, "e s3 s13 s2") && P->xowner == 0 && P->xto == 2 && P->pile[0][P->npile[0] - 1] == EX);
    /* the last trick: the Excuse goes over to the trick's winner */
    play_setup(4);
    const int b0[1] = {EX}, b1[1] = {S(3)}, b2[1] = {S(13)}, b3[1] = {S(2)};
    set_hand(0, b0, 1);
    set_hand(1, b1, 1);
    set_hand(2, b2, 1);
    set_hand(3, b3, 1);
    for (int q = 0; q < 4; q++) P->npile[q] = 0, P->won[q] = 0;
    P->won[2] = (uint8_t)(P->handsize - 1);
    P->turn = P->leader = 0;
    P->tricks = (uint8_t)(P->handsize - 1);
    CHECK(tarot_script(&G, "e s3 s13 s2") && P->xowner == TAROT_NONE && P->lastwin == 2);
    int found = 0;
    for (int i = 0; i < P->npile[2]; i++) found |= P->pile[2][i] == EX;
    CHECK(found && P->res.chelem == -200);
    /* the taker swept every trick: the Excuse in the last one wins it, the chelem */
    play_setup(4);
    set_hand(0, b0, 1);
    set_hand(1, b1, 1);
    set_hand(2, b2, 1);
    set_hand(3, b3, 1);
    for (int q = 0; q < 4; q++) P->npile[q] = 0, P->won[q] = 0;
    P->won[0] = (uint8_t)(P->handsize - 1);
    P->turn = P->leader = 0;
    P->tricks = (uint8_t)(P->handsize - 1);
    CHECK(tarot_script(&G, "e s3 s13 s2") && P->lastwin == 0 && P->res.chelem == 200);
}

/* ---- poignées -------------------------------------------------------------------------------------------------- */
static void test_poignee(void)
{
    play_setup(4);
    const int h[18] = {S(2), S(3), S(4), S(5), S(6), S(7), S(8), S(9), T(2), T(3), T(4), T(5), T(6), T(7), T(8), T(9),
                       T(21), EX};
    set_hand(0, h, 18);
    P->turn = P->leader = 0;
    CHECK(tarot_trumps(P, 0) == 10 && tarot_legal(P, TACT(TA_POIGNEE, 1)) && !tarot_legal(P, TACT(TA_POIGNEE, 2)));
    CHECK(tarot_script(&G, "g1") && P->poignee[0] == 1 && P->nshown[0] == 10);
    int ex = 0;
    for (int i = 0; i < 10; i++) ex |= P->shown[0][i] == EX;
    CHECK(ex && P->shown[0][0] == T(2));        /* nine trumps and the Excuse: it completes them */
    CHECK(!tarot_legal(P, TACT(TA_POIGNEE, 1)));
    CHECK(tarot_script(&G, "s2") && !tarot_legal(P, TACT(TA_POIGNEE, 1)));
    const int h2[18] = {S(2), S(3), S(4), S(5), S(6), S(7), T(1), T(2), T(3), T(4), T(5), T(6), T(7), T(8), T(9),
                        T(10), T(21), EX};
    set_hand(1, h2, 18);
    CHECK(tarot_script(&G, "g1") && P->nshown[1] == 10);
    ex = 0;
    for (int i = 0; i < 10; i++) ex |= P->shown[1][i] == EX;
    CHECK(!ex && P->shown[1][0] == T(1));       /* enough trumps: no Excuse */
    /* the bonus to the side that wins the contract */
    const int won[5] = {T(1), S(14), HE(14), D(14), C(14)};
    const int hands[4] = {T(21), S(2), S(3), S(4)};
    last_trick(4, TB_GARDE, won, 5, hands);
    P->poignee[0] = 1;
    P->poignee[2] = 2;
    CHECK(tarot_script(&G, "a21 s2 s3 s4") && !P->res.made && P->res.poignee == -50);
    CHECK(P->res.unit == -(25 + 13) * 2 - 50 && sum_scores() == 0);
}

/* ---- whole games ------------------------------------------------------------------------------------------------- */
static uint32_t rng = 12345;
static int rnd(int n)
{
    rng = rng * 1103515245u + 12345u;
    return (int)((rng >> 16) % (uint32_t)n);
}

static int legal_acts(const tpos *p, int *out)
{
    int n = 0;
    for (int k = TA_BID; k <= TA_NEXT; k++)
        for (int a = 0; a < 256; a++)
            if (tarot_legal(p, TACT(k, a))) out[n++] = TACT(k, a);
    return n;
}

static int pick(const tpos *p, const int *acts, int n)
{
    int done = -1, ret = 0, poig = -1;
    for (int i = 0; i < n; i++) {
        if (TA_KIND(acts[i]) == TA_DONE) done = acts[i];
        if (TA_KIND(acts[i]) == TA_RETURN) ret++;
        if (TA_KIND(acts[i]) == TA_POIGNEE) poig = acts[i];
    }
    if (done >= 0 && rnd(3)) return done;
    if (poig >= 0 && rnd(2)) return poig;
    if (p->phase == TP_BID) return rnd(3) ? acts[0] : acts[rnd(n)];   /* passes, mostly */
    for (int tries = 0; tries < 50; tries++) {
        int a = acts[rnd(n)];
        if (TA_KIND(a) == TA_RETURN && rnd(6)) continue;
        if (TA_KIND(a) == TA_POIGNEE) continue;
        return a;
    }
    (void)ret;
    return acts[0];
}

static void test_games(void)
{
    static int acts[2600];
    int games = 0, deals = 0, bad_sum = 0, bad_pts = 0, bad_end = 0, bad_cards = 0, contracts[5] = {0}, made = 0;
    int chelems = 0, pabs = 0, poignees = 0, voids = 0;
    for (int n = 3; n <= 5; n++)
        for (uint32_t seed = 1; seed <= 60; seed++) {
            tarot_new(&G, n, seed * 7919u);
            int steps = 0, prev_played = 0;
            while (P->phase != TP_OVER && steps++ < 20000) {
                if (P->phase == TP_VOID) voids++;
                int k = legal_acts(P, acts);
                if (!k) { bad_end++; break; }
                int a = pick(P, acts, k);
                if (!tarot_play(&G, a)) { bad_end++; break; }
                int cards = 0;                  /* every card somewhere, once */
                if (P->phase == TP_PLAY) {
                    for (int q = 0; q < n; q++) cards += P->nhand[q] + P->npile[q];
                    cards += P->ntrick + P->necart;
                    if (cards != 78) bad_cards++;
                }
                if (P->played != prev_played) {
                    prev_played = P->played;
                    deals++;
                    contracts[P->contract]++;
                    made += P->res.made;
                    chelems += P->res.chelem != 0;
                    pabs += P->res.pab != 0;
                    poignees += P->res.poignee != 0;
                    if (sum_scores() != 0) bad_sum++;
                    int def = 0;                /* the two sides' points: 91 in all */
                    for (int q = 0; q < n; q++)
                        if (!tarot_attack(P, q))
                            for (int i = 0; i < P->npile[q]; i++) def += tarot_value2(P->pile[q][i]);
                    if (!P->aside_attack)
                        for (int i = 0; i < P->necart; i++) def += tarot_value2(P->ecart[i]);
                    if (P->xowner != TAROT_NONE) def += tarot_attack(P, P->xowner) ? 1 : -1;
                    if (def + P->res.half != 182) bad_pts++;
                }
            }
            int tot = 0;
            for (int q = 0; q < n; q++) tot += P->total[q];
            if (P->phase != TP_OVER || P->played != n || tot != 0) bad_end++;
            games++;
            if (seed % 15 == 0) {               /* undo: a replay from the seed gives the same position */
                H = G;
                tarot_undo(&H);
                tarot_play(&H, G.act[G.n - 1]);
                if (memcmp(&H.pos, &G.pos, sizeof G.pos)) bad_end++;
            }
        }
    CHECK(bad_sum == 0 && bad_pts == 0 && bad_end == 0 && bad_cards == 0);
    CHECK(deals == 3 * 60 * 4 && contracts[TB_PASS] == 0);
    printf("tarot: %d games, %d deals (%d cancelled on the way), contracts %d/%d/%d/%d, %d made, %d chelems, %d petits "
           "au bout, %d with poignées\n", games, deals, voids, contracts[1], contracts[2], contracts[3], contracts[4],
           made, chelems, pabs, poignees);
}

static void test_script(void)
{
    fresh(4, 99);
    CHECK(tarot_script(&G, "#") && P->phase == TP_SCORE && P->played == 1);
    CHECK(tarot_script(&G, "n #") && P->played == 2);
    CHECK(tarot_script(&G, "n # n #") && P->phase == TP_OVER && P->played == 4);
    CHECK(!tarot_legal(P, TACT(TA_NEXT, 0)));
    int w = tarot_winner(P), best = -10000, tie = 0;
    for (int q = 0; q < 4; q++) {
        if (P->total[q] > best) best = P->total[q], tie = 0;
        else if (P->total[q] == best) tie = 1;
    }
    CHECK(tie ? w == -1 : P->total[w] == best);
    fresh(4, 99);
    CHECK(!tarot_script(&G, "b5") && !tarot_script(&G, "zz"));
    const char *s = "h14";
    CHECK(tarot_parse_card(&s) == HE(14) && !*s);
    s = "a21";
    CHECK(tarot_parse_card(&s) == T(21));
    s = "e";
    CHECK(tarot_parse_card(&s) == EX);
    s = "s15";
    CHECK(tarot_parse_card(&s) == CARD_NONE);
}

/* ---- no shuffle between deals: gathered, cut, dealt -------------------------------------------------------------- */
static int is_pack(const uint8_t *pack)          /* the 78 cards, once each */
{
    uint8_t all[CARDS_MAX];
    int seen[256] = {0}, ok = 1;
    for (int i = 0; i < 78; i++) seen[pack[i]]++;
    for (int i = 0; i < cards_pack(PACK_TAROT, all); i++) ok &= seen[all[i]] == 1;
    return ok;
}

static int dealt_from(const tpos *p, const uint8_t *pack)   /* the deal is the pack, cut, in order */
{
    if (p->cut < 4 || p->cut > 74) return 0;
    for (int i = 0; i < 78; i++)
        if (p->deal_card[i] != pack[(p->cut + i) % 78]) return 0;
    return 1;
}

static void test_cut(void)
{
    uint8_t pack[78], cut[78];
    cards_pack(PACK_TAROT, pack);
    int lo = 99, hi = 0, ok = 1;
    for (uint32_t seed = 0; seed < 5000; seed++) {
        memcpy(cut, pack, 78);
        int k = tarot_cut(cut, seed);
        lo = k < lo ? k : lo;
        hi = k > hi ? k : hi;
        for (int i = 0; i < 78; i++) ok &= cut[i] == pack[(k + i) % 78];   /* the lower part on top */
    }
    CHECK(ok && lo == 4 && hi == 74);           /* more than 3 cards in each part, every cut possible */
    for (int n = 3; n <= 5; n++) {              /* the cutter: the dealer's left, the player before them */
        fresh(n, 3);
        CHECK(P->cutter == (P->dealer + n - 1) % n && P->cut >= 4 && P->cut <= 74);
        CHECK(dealt_from(P, G.start));
    }
}

static void test_warmup(void)
{
    int done = 0, packs = 0, differ = 0;
    uint8_t a[78], b[78], prev[78] = {0};
    for (int n = 3; n <= 5; n++)
        for (uint32_t seed = 1; seed <= 200; seed++) {
            done += tarot_warmup(a, n, seed * 2654435761u);
            packs += is_pack(a);
            differ += memcmp(a, prev, 78) != 0;
            memcpy(prev, a, 78);
        }
    CHECK(done == 600 && packs == 600 && differ == 600);
    CHECK(tarot_warmup(a, 4, 77) && tarot_warmup(b, 4, 77) && !memcmp(a, b, 78));   /* a seed, a pack */
    tarot_new(&G, 4, 77);
    CHECK(!memcmp(G.start, a, 78) && dealt_from(P, a));
}

/* a deal played out: each side's pile bottom up (the aside, the tricks as won), the defence's on the taker's */
static void test_gather(void)
{
    uint8_t pack[78], next[78];
    fresh(4, 21);
    CHECK(tarot_script(&G, "b2 p p p t") && P->phase == TP_DISCARD);
    tarot_gather(P, pack);                      /* mid-deal too: every card, once */
    CHECK(is_pack(pack));
    CHECK(tarot_script(&G, ".7") && P->phase == TP_PLAY);
    uint8_t ecart[6];
    memcpy(ecart, P->ecart, 6);
    CHECK(tarot_script(&G, ".5"));
    tarot_gather(P, pack);
    CHECK(is_pack(pack) && P->nheap[1] >= 6 && !memcmp(P->heap[1], ecart, 6));   /* the discard at the bottom */
    CHECK(tarot_script(&G, "#") && P->phase == TP_SCORE);
    tarot_gather(P, pack);
    CHECK(is_pack(pack) && P->nheap[0] + P->nheap[1] == 78);
    int ok = 1;                                 /* top first: the defence's pile reversed, then the taker's */
    for (int i = 0; i < P->nheap[0]; i++) ok &= pack[i] == P->heap[0][P->nheap[0] - 1 - i];
    for (int i = 0; i < P->nheap[1]; i++) ok &= pack[P->nheap[0] + i] == P->heap[1][P->nheap[1] - 1 - i];
    CHECK(ok && pack[77] == ecart[0]);
    /* the same game twice: the same next deal; and it is that pack, cut */
    H = G;
    CHECK(tarot_script(&G, "n") && tarot_script(&H, "n") && !memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(dealt_from(P, pack));
    /* Garde contre: the dog at the bottom of the defence's pile */
    fresh(4, 21);
    CHECK(tarot_script(&G, "p p p b4") && P->nheap[0] == 6 && !memcmp(P->heap[0], P->dog, 6) && P->nheap[1] == 0);
    fresh(4, 21);
    CHECK(tarot_script(&G, "b3 p p p") && P->nheap[1] == 6 && !memcmp(P->heap[1], P->dog, 6));
    CHECK(tarot_script(&G, "#"));
    tarot_gather(P, pack);
    CHECK(is_pack(pack) && pack[77] == P->dog[0]);
    /* a cancelled deal: the hands as held from the dealer's next, each on the one before, the dog on top */
    fresh(4, 7);
    CHECK(tarot_script(&G, "p p p p") && P->phase == TP_VOID);
    tarot_gather(P, pack);
    uint8_t want[78];
    int k = 0;
    for (int i = P->dogsize - 1; i >= 0; i--) want[k++] = P->dog[i];
    for (int j = 3; j >= 0; j--) {
        int pl = (P->dealer + 1 + j) % 4;
        for (int i = P->nhand[pl] - 1; i >= 0; i--) want[k++] = P->hand[pl][i];
    }
    CHECK(k == 78 && !memcmp(pack, want, 78) && is_pack(pack));
    CHECK(tarot_script(&G, "n") && dealt_from(P, want));
    memcpy(next, pack, 78);
    tarot_start(&H, 5, 1234, next);             /* a pack carries over to a game of any size */
    CHECK(!memcmp(H.start, next, 78) && dealt_from(&H.pos, next) && H.pos.players == 5);
}

/* how bunched the hands are: the trumps per hand (their variance), the longest suit per hand, packets of one suit */
typedef struct { double trumps, trumps2, longest, packets; int hands, deals, npk; } clump;

static void measure(clump *m, const tpos *p)
{
    for (int q = 0; q < p->players; q++) {
        int t = tarot_trumps(p, q), per[5] = {0}, top = 0;
        for (int i = 0; i < p->nhand[q]; i++) per[CARD_SUIT(p->hand[q][i])]++;
        for (int s = 0; s < 5; s++) top = per[s] > top ? per[s] : top;
        m->trumps += t;
        m->trumps2 += t * t;
        m->longest += top;
        m->hands++;
    }
    for (int i = 0; i + 2 < 78; i++) {          /* the packets of three: all of one suit (the trumps: one suit) */
        if (p->deal_to[i] == TAROT_DOG || p->deal_to[i + 1] != p->deal_to[i] || p->deal_to[i + 2] != p->deal_to[i])
            continue;
        int s = CARD_SUIT(p->deal_card[i]);
        m->packets += CARD_SUIT(p->deal_card[i + 1]) == s && CARD_SUIT(p->deal_card[i + 2]) == s;
        m->npk++;
        i += 2;
    }
    m->deals++;
}

static void report(const char *what, const clump *m)
{
    double mean = m->trumps / m->hands;
    printf("tarot: %-26s %d deals: trumps per hand %.2f (variance %.2f), longest suit %.2f, packets of one suit "
           "%.1f%%\n", what, m->deals, mean, m->trumps2 / m->hands - mean * mean, m->longest / m->hands,
           100.0 * m->packets / m->npk);
}

static void test_clumps(void)
{
    static int acts[2600];
    clump real = {0}, after_play = {0}, after_void = {0}, shuffled = {0};
    uint8_t pack[78];
    rng = 2026;
    tarot_warmup(pack, 4, 99);
    int prev_void = 0;
    while (real.deals < 500) {                  /* games in a row, as in a session: each from the last one's cards */
        tarot_start(&G, 4, (uint32_t)rnd(1 << 30), pack);
        for (;;) {
            measure(&real, P);
            measure(prev_void ? &after_void : &after_play, P);
            while (P->phase != TP_SCORE && P->phase != TP_VOID && P->phase != TP_OVER) {
                int k = legal_acts(P, acts);
                if (!k || !tarot_play(&G, pick(P, acts, k))) break;
            }
            prev_void = P->phase == TP_VOID;
            if (P->phase == TP_OVER || real.deals >= 500) break;
            tarot_play(&G, TACT(TA_NEXT, 0));
        }
        tarot_gather(P, pack);
    }
    for (uint32_t seed = 1; seed <= 500; seed++) {   /* the same deal from packs shuffled at random */
        cards_pack(PACK_TAROT, pack);
        cards_shuffle(pack, 78, seed * 40503u);
        tarot_start(&H, 4, seed, pack);
        measure(&shuffled, &H.pos);
    }
    report("gathered and cut:", &real);
    report("  after a deal played:", &after_play);
    report("  after a cancelled deal:", &after_void);
    report("shuffled:", &shuffled);
    double vr = real.trumps2 / real.hands - (real.trumps / real.hands) * (real.trumps / real.hands);
    double vs = shuffled.trumps2 / shuffled.hands - (shuffled.trumps / shuffled.hands) * (shuffled.trumps / shuffled.hands);
    CHECK(vr > vs && real.longest / real.hands > shuffled.longest / shuffled.hands);
    CHECK(after_play.longest / after_play.hands > shuffled.longest / shuffled.hands);
    CHECK(real.packets / real.npk > shuffled.packets / shuffled.npk);
}

int main(void)
{
    test_deal();
    test_petit_sec();
    test_bidding();
    test_discard();
    test_call();
    test_follow();
    test_values();
    test_score();
    test_five();
    test_excuse();
    test_poignee();
    test_script();
    test_games();
    test_cut();
    test_warmup();
    test_gather();
    test_clumps();
    printf("tarot: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
