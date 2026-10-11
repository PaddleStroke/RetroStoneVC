/*
 * Belote: the rules' test (the deal and the bidding of both variants, the cards one may play, the tricks, belote and
 * rebelote, the dix de der, the scores: made, dedans, capot, coinche and surcoinche; undo, the notation, whole random
 * games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/belote.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bgame G, H;
static bpos *P = &G.pos;

#define S SUIT_SPADES
#define HE SUIT_HEARTS
#define D SUIT_DIAMONDS
#define C SUIT_CLUBS

static uint32_t rng = 12345;
static int rnd(int n)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

/* every card of the pack once: in the hands, the pack still to deal, the turned card, the tricks */
static int all_cards(const bpos *p, const uint8_t *gone, int ngone)
{
    int seen[256] = {0};
    for (int q = 0; q < BEL_P; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++;
    for (int i = p->top; i < 32; i++) seen[p->pack[i]]++;
    if (p->turned != CARD_NONE && p->taker == BEL_NONE) seen[p->turned]++;
    for (int i = 0; i < p->ntrick; i++) seen[p->trick[i]]++;
    for (int i = 0; i < ngone; i++) seen[gone[i]]++;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if ((r == 1 || r >= 7) && seen[CARD(s, r)] != 1) return 0;
    return 1;
}

static void test_ranks(void)
{
    /* trumps J 9 A 10 K Q 8 7, the others A 10 K Q J 9 8 7; points 152 in all for every trump suit */
    static const int trump_order[8] = {11, 9, 1, 10, 13, 12, 8, 7}, plain_order[8] = {1, 10, 13, 12, 11, 9, 8, 7};
    for (int i = 0; i < 8; i++) {
        CHECK(bel_rank(CARD(HE, trump_order[i]), HE) == 8 - i);
        CHECK(bel_rank(CARD(S, plain_order[i]), HE) == 8 - i);
    }
    CHECK(bel_points(CARD(HE, 11), HE) == 20 && bel_points(CARD(HE, 9), HE) == 14 && bel_points(CARD(HE, 1), HE) == 11);
    CHECK(bel_points(CARD(S, 11), HE) == 2 && bel_points(CARD(S, 9), HE) == 0 && bel_points(CARD(S, 10), HE) == 10);
    for (int t = 0; t < 4; t++) {
        uint8_t pack[CARDS_MAX];
        int n = cards_pack(PACK_32, pack), sum = 0;
        for (int i = 0; i < n; i++) sum += bel_points(pack[i], t);
        CHECK(n == 32 && sum == 152);
    }
}

static void test_deal_classic(void)
{
    bel_new(&G, BEL_CLASSIC, 42);
    bel_new(&H, BEL_CLASSIC, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->phase == BP_BID1 && P->dealer == 3 && P->turn == 0 && P->top == 21 && P->turned != CARD_NONE);
    for (int q = 0; q < 4; q++) CHECK(P->n[q] == 5);
    CHECK(all_cards(P, NULL, 0));
    CHECK(!bel_play(&G, BA_NAME | SUIT_SPADES) && !bel_play(&G, BA_NEXT) && !bel_play(&G, BA_PLAY | P->hand[0][0]));
    /* round one: all pass; round two: the turned suit cannot be named; the second player names one */
    int turned = P->turned;
    for (int i = 0; i < 4; i++) CHECK(bel_play(&G, BA_PASS));
    CHECK(P->phase == BP_BID2 && P->turn == 0);
    CHECK(!bel_play(&G, BA_TAKE) && !bel_play(&G, BA_NAME | CARD_SUIT(turned)));
    CHECK(bel_play(&G, BA_PASS));
    int other = (CARD_SUIT(turned) + 1) % 4;
    CHECK(bel_play(&G, BA_NAME | other));
    CHECK(P->phase == BP_PLAY && P->taker == 1 && P->trump == other && P->turn == 0 && P->leader == 0);
    for (int q = 0; q < 4; q++) CHECK(P->n[q] == 8);
    int has = 0;
    for (int i = 0; i < 8; i++) has |= P->hand[1][i] == turned;
    CHECK(has && P->top == 32 && all_cards(P, NULL, 0));
    /* everyone passes twice: a new deal, the next dealer */
    bel_new(&G, BEL_CLASSIC, 42);
    for (int i = 0; i < 8; i++) CHECK(bel_play(&G, BA_PASS));
    CHECK(P->phase == BP_BID1 && P->dealer == 0 && P->turn == 1 && P->deal == 0 && all_cards(P, NULL, 0));
    CHECK(memcmp(P->hand, H.pos.hand, sizeof P->hand) != 0);
    /* round one: the third player takes the turned card */
    bel_new(&G, BEL_CLASSIC, 7);
    turned = P->turned;
    CHECK(bel_play(&G, BA_PASS) && bel_play(&G, BA_PASS) && bel_play(&G, BA_TAKE));
    CHECK(P->taker == 2 && P->trump == CARD_SUIT(turned) && P->said[0] == BA_PASS && P->said[2] == BA_TAKE);
}

static void test_auction(void)
{
    bel_new(&G, BEL_COINCHE, 9);
    CHECK(P->phase == BP_AUCTION && P->turned == CARD_NONE && P->top == 32);
    for (int q = 0; q < 4; q++) CHECK(P->n[q] == 8);
    CHECK(!bel_play(&G, BA_TAKE) && !bel_play(&G, BA_COINCHE));        /* nothing to coinche yet */
    CHECK(bel_play(&G, BA_BIDOF(0, HE)));                           /* 0: 80 hearts */
    CHECK(!bel_play(&G, BA_BIDOF(0, S)));                           /* not higher */
    CHECK(bel_play(&G, BA_BIDOF(2, S)));                            /* 1: 100 spades */
    CHECK(P->turn == 2 && P->bidder == 1 && P->level == 2);
    CHECK(bel_play(&G, BA_PASS));                                   /* 2 passes */
    CHECK(!bel_play(&G, BA_COINCHE));                               /* 3: their partner's bid */
    CHECK(bel_play(&G, BA_PASS));                                   /* 3 */
    CHECK(P->phase == BP_AUCTION && P->turn == 0);                  /* two passes: not over */
    CHECK(bel_play(&G, BA_PASS));                                   /* 0 passes: three in a row */
    CHECK(P->phase == BP_PLAY && P->taker == 1 && P->trump == S && P->coinche == 0 && P->turn == 0);
    /* coinche and surcoinche */
    bel_new(&G, BEL_COINCHE, 9);
    CHECK(bel_play(&G, BA_BIDOF(4, D)) && bel_play(&G, BA_COINCHE));   /* 0: 120 diamonds; 1 coinches */
    CHECK(P->coinche == 1 && P->turn == 0 && !bel_play(&G, BA_BIDOF(5, D)));
    CHECK(bel_play(&G, BA_SURCOINCHE));
    CHECK(P->phase == BP_PLAY && P->coinche == 2 && P->taker == 0 && P->trump == D);
    bel_new(&G, BEL_COINCHE, 9);
    CHECK(bel_play(&G, BA_PASS) && bel_play(&G, BA_BIDOF(BEL_CAPOT, C)) && bel_play(&G, BA_COINCHE));
    CHECK(!bel_play(&G, BA_SURCOINCHE | 1) && bel_play(&G, BA_PASS));
    CHECK(P->phase == BP_PLAY && P->coinche == 1 && P->level == BEL_CAPOT && P->taker == 1);
    /* four passes: a new deal */
    bel_new(&G, BEL_COINCHE, 9);
    for (int i = 0; i < 4; i++) CHECK(bel_play(&G, BA_PASS));
    CHECK(P->phase == BP_AUCTION && P->dealer == 0 && P->turn == 1);
}

/* a position in the play: hearts trumps, the hands given, the trick so far */
static void setup(int leader, const int *trick, int nt, int turn, const int *hand, int nh)
{
    bel_new(&G, BEL_CLASSIC, 1);
    P->phase = BP_PLAY;
    P->trump = HE;
    P->taker = 0;
    P->leader = (uint8_t)leader;
    P->ntrick = (uint8_t)nt;
    for (int i = 0; i < nt; i++) P->trick[i] = (uint8_t)trick[i];
    P->turn = (uint8_t)turn;
    P->n[turn] = (uint8_t)nh;
    for (int i = 0; i < nh; i++) P->hand[turn][i] = (uint8_t)hand[i];
}

static int ok(int card) { return bel_legal(P, BA_PLAY | card); }

static void test_follow(void)
{
    /* follow suit when you can */
    int t1[] = {CARD(S, 10)}, h1[] = {CARD(S, 7), CARD(HE, 11), CARD(C, 1)};
    setup(0, t1, 1, 1, h1, 3);
    CHECK(ok(CARD(S, 7)) && !ok(CARD(HE, 11)) && !ok(CARD(C, 1)));
    CHECK(!ok(CARD(D, 1)));                                     /* not in the hand */
    /* void, an opponent holds the trick: trump */
    int h2[] = {CARD(HE, 7), CARD(HE, 11), CARD(C, 1)};
    setup(0, t1, 1, 1, h2, 3);
    CHECK(ok(CARD(HE, 7)) && ok(CARD(HE, 11)) && !ok(CARD(C, 1)));
    /* an opponent trumped: over-trump if you can (the 9 beats the ace, not the 10) */
    int t3[] = {CARD(S, 10), CARD(HE, 1)}, h3[] = {CARD(HE, 10), CARD(HE, 9), CARD(C, 1)};
    setup(0, t3, 2, 2, h3, 3);                                  /* player 2: their partner (0) led, 1 trumped */
    CHECK(ok(CARD(HE, 9)) && !ok(CARD(HE, 10)) && !ok(CARD(C, 1)));
    /* cannot over-trump: any trump all the same (under-trump) */
    int h4[] = {CARD(HE, 10), CARD(HE, 7), CARD(C, 1)};
    setup(0, t3, 2, 2, h4, 3);
    CHECK(ok(CARD(HE, 10)) && ok(CARD(HE, 7)) && !ok(CARD(C, 1)));
    /* void, your partner holds the trick: anything */
    int t5[] = {CARD(S, 1), CARD(S, 7)}, h5[] = {CARD(HE, 7), CARD(C, 1), CARD(D, 8)};
    setup(0, t5, 2, 2, h5, 3);
    CHECK(ok(CARD(HE, 7)) && ok(CARD(C, 1)) && ok(CARD(D, 8)));
    /* your partner trumped and holds it: anything, even a lower trump */
    int t6[] = {CARD(S, 1), CARD(HE, 9), CARD(S, 7)}, h6[] = {CARD(HE, 7), CARD(C, 1)};
    setup(0, t6, 3, 3, h6, 2);
    CHECK(ok(CARD(HE, 7)) && ok(CARD(C, 1)));
    /* trumps led: over-trump, even your partner's trump */
    int t7[] = {CARD(HE, 1), CARD(HE, 8)}, h7[] = {CARD(HE, 7), CARD(HE, 11), CARD(S, 1)};
    setup(0, t7, 2, 2, h7, 3);
    CHECK(ok(CARD(HE, 11)) && !ok(CARD(HE, 7)) && !ok(CARD(S, 1)));
    int h8[] = {CARD(HE, 7), CARD(HE, 12), CARD(S, 1)};               /* only lower trumps: any of them */
    setup(0, t7, 2, 2, h8, 3);
    CHECK(ok(CARD(HE, 7)) && ok(CARD(HE, 12)) && !ok(CARD(S, 1)));
    int h9[] = {CARD(S, 7), CARD(C, 1)};                              /* no trump: anything */
    setup(0, t7, 2, 2, h9, 2);
    CHECK(ok(CARD(S, 7)) && ok(CARD(C, 1)));
    /* a trick's best card: the highest trump, else the highest of the suit led */
    int t10[] = {CARD(S, 12), CARD(S, 1), CARD(C, 1), CARD(S, 10)};
    setup(0, t10, 4, 0, h9, 2);
    CHECK(bel_trick_best(P) == 1);
    int t11[] = {CARD(S, 12), CARD(S, 1), CARD(HE, 7), CARD(HE, 8)};
    setup(0, t11, 4, 0, h9, 2);
    CHECK(bel_trick_best(P) == 3);
}

/* the deal played from a script to its end: every card once, 162 points, the score as the rules say */
static void test_scoring(void)
{
    /* belote and rebelote: find a deal where the taker's team holds them, check the 20 */
    int found = 0;
    for (uint32_t seed = 1; seed < 400 && found < 3; seed++) {
        bel_new(&G, BEL_CLASSIC, seed);
        bel_play(&G, BA_TAKE);
        if (P->belote == BEL_NONE) continue;
        int holder = P->belote, said = 0;
        while (P->phase == BP_PLAY) {
            int before = P->belote_said;
            CHECK(bel_play(&G, bel_auto(P)));
            if (P->belote_said != before) said++;
        }
        CHECK(said == 2 && P->belote_said == 2);
        int t = bel_team(P->taker), bt = bel_team(holder);
        CHECK(P->made_pts[bt] == (P->won[bt] == 8 ? 250 : P->won[1 - bt] == 8 ? 0 : P->points[bt]) + 20);
        CHECK(P->made == (P->made_pts[t] > P->made_pts[1 - t]));
        found++;
    }
    CHECK(found == 3);
    /* a capot: player 0 takes every trick (a deal where the auto play gives all eight to one team) */
    int capots = 0, dedans = 0, made = 0;
    for (uint32_t seed = 1; seed < 3000; seed++) {
        bel_new(&G, BEL_CLASSIC, seed);
        bel_script(&G, "#");
        if (P->phase != BP_DEAL && P->phase != BP_OVER) { CHECK(0); break; }
        CHECK(P->points[0] + P->points[1] == 162);
        CHECK(P->won[0] + P->won[1] == 8);
        int t = bel_team(P->taker), d = 1 - t, bel[2] = {0, 0};
        if (P->belote != BEL_NONE) bel[bel_team(P->belote)] = 20;
        if (P->capot) {
            capots++;
            int c = P->capot - 1;
            CHECK(P->won[c] == 8 && P->made_pts[c] == 250 + bel[c] && P->made_pts[1 - c] == bel[1 - c]);
        }
        if (P->made) {
            made++;
            CHECK(P->score[t] == P->made_pts[t] && P->score[d] == P->made_pts[d]);
        } else {
            dedans++;
            CHECK(P->score[t] == bel[t] && P->score[d] == (P->won[d] == 8 ? 250 : 162) + bel[d]);
        }
        CHECK(P->total[0] == P->score[0] && P->total[1] == P->score[1] && P->deal == 1);
    }
    CHECK(capots > 0 && dedans > 0 && made > 0);
    printf("   classic auto deals: %d made, %d dedans, %d capots\n", made, dedans, capots);
}

static void test_coinche_score(void)
{
    int seen[4] = {0};
    for (uint32_t seed = 1; seed < 2000; seed++) {
        bel_new(&G, BEL_COINCHE, seed);
        int level = rnd(BEL_LEVELS), co = rnd(3);
        CHECK(bel_play(&G, BA_BIDOF(level, rnd(4))));
        if (co) {
            CHECK(bel_play(&G, BA_COINCHE));
            CHECK(bel_play(&G, co == 2 ? BA_SURCOINCHE : BA_PASS));
        } else
            for (int i = 0; i < 3; i++) CHECK(bel_play(&G, BA_PASS));
        CHECK(P->phase == BP_PLAY && P->taker == 0 && P->coinche == co);
        while (P->phase == BP_PLAY) CHECK(bel_play(&G, bel_auto(P)));
        int t = 0, d = 1, bel[2] = {0, 0}, v = bel_value(level), m = 1 << co;
        if (P->belote != BEL_NONE) bel[bel_team(P->belote)] = 20;
        int pt = P->made_pts[t], pd = P->made_pts[d];
        int made = level == BEL_CAPOT ? P->won[t] == 8 : pt >= v && pt > 81;
        CHECK(P->made == made);
        if (!co) {
            CHECK(P->score[t] == (made ? v + pt : bel[t]));
            CHECK(P->score[d] == (made ? pd : 160 + v + bel[d]));
        } else {
            int w = made ? t : d;
            CHECK(P->score[w] == (160 + v) * m + bel[w] && P->score[1 - w] == bel[1 - w]);
        }
        seen[made * 2 + (co > 0)]++;
    }
    CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
}

/* undo: back one action is the same as never playing it; the notation */
static void test_undo_script(void)
{
    bel_new(&G, BEL_CLASSIC, 5);
    CHECK(bel_script(&G, "p t . . . ."));
    bpos before = G.pos;
    CHECK(bel_play(&G, bel_auto(P)));
    CHECK(bel_undo(&G));
    CHECK(!memcmp(&before, &G.pos, sizeof before) && G.n == 6);
    bel_new(&G, BEL_CLASSIC, 5);
    CHECK(!bel_undo(&G));
    CHECK(!bel_script(&G, "q"));
    bel_new(&G, BEL_CLASSIC, 5);
    int turned = P->turned;
    char buf[32];
    static const char SU[] = "shdc", RK[] = " A     789TJQK";
    CHECK(bel_script(&G, "t"));
    snprintf(buf, sizeof buf, "%c%c", RK[CARD_RANK(P->hand[0][0])], SU[CARD_SUIT(P->hand[0][0])]);
    int card = P->hand[0][0];
    CHECK(bel_script(&G, buf) && P->trick[0] == card && P->turn == 1 && P->trump == CARD_SUIT(turned));
    bel_new(&G, BEL_COINCHE, 5);
    CHECK(bel_script(&G, "b90h x xx") && P->coinche == 2 && P->level == 1 && P->trump == HE);
    bel_new(&G, BEL_COINCHE, 5);
    CHECK(bel_script(&G, "p b250c p p p") && P->level == BEL_CAPOT && P->taker == 1 && P->trump == C);
    CHECK(!bel_script(&G, "b85h"));
}

/* whole games from seeds, random legal actions: every card once, 162 points a deal, the totals, the end */
static void test_random_games(void)
{
    for (int v = 0; v < BEL_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 150; seed++) {
            bel_new(&G, v, seed);
            rng = seed * 7919u + 1;
            uint8_t gone[32];
            int ngone = 0, sum[2] = {0, 0}, deals = 0, good = 1, steps = 0;
            while (P->phase != BP_OVER && steps++ < 20000) {
                int acts[64], n = bel_moves(P, acts);
                if (n <= 0) { good = 0; break; }
                for (int i = 0; i < n; i++) good &= bel_legal(P, acts[i]);
                /* a bias: bids are rarer than passes, so the auctions end */
                int a = acts[rnd(n)];
                if ((P->phase == BP_AUCTION || P->phase == BP_BID2) && rnd(3)) a = BA_PASS;
                if (!bel_legal(P, a)) a = acts[0];
                int phase = P->phase, nt = P->ntrick;
                uint8_t trick[4];
                memcpy(trick, P->trick, 4);
                if (!bel_play(&G, a)) { good = 0; break; }
                if (phase == BP_PLAY && nt == 3) {   /* a trick done: its cards gone */
                    for (int i = 0; i < 3; i++) gone[ngone++] = trick[i];
                    gone[ngone++] = (uint8_t)BA_ARG(a);
                }
                if (P->phase == BP_PLAY) good &= all_cards(P, gone, ngone);
                if (phase == BP_PLAY && (P->phase == BP_DEAL || P->phase == BP_OVER)) {
                    good &= ngone == 32 && P->points[0] + P->points[1] == 162;
                    sum[0] += P->score[0], sum[1] += P->score[1], deals++;
                    good &= P->total[0] == sum[0] && P->total[1] == sum[1] && P->deal == deals;
                    ngone = 0;
                }
                if (P->phase == BP_BID1 || P->phase == BP_AUCTION) ngone = 0;
            }
            CHECK(good);
            CHECK(P->phase == BP_OVER);
            int w = bel_winner_team(P), target = bel_target(P);
            CHECK(w >= 0 && P->total[w] > P->total[1 - w] && P->total[w] >= target);
        }
}

int main(void)
{
    test_ranks();
    test_deal_classic();
    test_auction();
    test_follow();
    test_scoring();
    test_coinche_score();
    test_undo_script();
    test_random_games();
    printf("belote: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
