/*
 * Belote: the rules' test (the deal and the bidding of both variants, the cards one may play, the tricks, belote and
 * rebelote, the dix de der, the scores: made, dedans, capot, coinche and surcoinche; undo, the notation, whole random
 * games from seeds; no shuffle between deals: the gathering, the cut, the warm-up deal, how bunched the hands are
 * against shuffled ones). No SDL.
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

/* ---- no shuffle between deals: gathered, cut, dealt -------------------------------------------------------------- */
static int is_pack(const uint8_t *pack)          /* the 32 cards, once each */
{
    uint8_t all[CARDS_MAX];
    int seen[256] = {0}, ok = 1;
    for (int i = 0; i < 32; i++) seen[pack[i]]++;
    for (int i = 0; i < cards_pack(PACK_32, all); i++) ok &= seen[all[i]] == 1;
    return ok;
}

static int dealt_from(const bpos *p, const uint8_t *pack)   /* the deal is the pack, cut, in order */
{
    if (p->cut < 3 || p->cut > 29) return 0;
    for (int i = 0; i < 32; i++)
        if (p->pack[i] != pack[(p->cut + i) % 32]) return 0;
    return 1;
}

/* the packets in pack order (top first): classic 3 each, 2 each, the turned card; coinche 3, 3, 2 each */
static int first_dealt(const bpos *p, int q, int i)   /* the i-th card player q was given at the first deal */
{
    int first = (p->dealer + 1) % BEL_P, k = (q - first + BEL_P) % BEL_P;
    if (p->variant == BEL_CLASSIC) return i < 3 ? p->pack[3 * k + i] : p->pack[12 + 2 * k + i - 3];
    return i < 3 ? p->pack[3 * k + i] : i < 6 ? p->pack[12 + 3 * k + i - 3] : p->pack[24 + 2 * k + i - 6];
}

static void test_cut(void)
{
    uint8_t pack[CARDS_MAX], cut[32];
    cards_pack(PACK_32, pack);
    int lo = 99, hi = 0, ok = 1;
    for (uint32_t seed = 0; seed < 5000; seed++) {
        memcpy(cut, pack, 32);
        int k = bel_cut(cut, seed);
        lo = k < lo ? k : lo;
        hi = k > hi ? k : hi;
        for (int i = 0; i < 32; i++) ok &= cut[i] == pack[(k + i) % 32];   /* the lower part on top */
    }
    CHECK(ok && lo == 3 && hi == 29);           /* at least 3 cards in each part, every cut possible */
    for (int v = 0; v < BEL_VARIANTS; v++) {    /* the cutter: the player before the dealer; the packets in order */
        bel_new(&G, v, 3);
        CHECK(P->cutter == (P->dealer + 3) % 4 && P->cut >= 3 && P->cut <= 29 && dealt_from(P, G.start));
        int good = 1;
        for (int q = 0; q < 4; q++)
            for (int i = 0; i < P->n[q]; i++) good &= P->hand[q][i] == first_dealt(P, q, i);
        CHECK(good && (v == BEL_COINCHE ? P->top == 32 : P->top == 21 && P->turned == P->pack[20]));
    }
    /* classic: after the taking, the rest in packets of 3 from the dealer's next, the taker 2 after the turned card */
    bel_new(&G, BEL_CLASSIC, 3);
    uint8_t pk[32];
    memcpy(pk, P->pack, 32);
    CHECK(bel_script(&G, "p t"));               /* player 1 takes */
    CHECK(P->hand[0][5] == pk[21] && P->hand[0][7] == pk[23] && P->hand[1][5] == pk[20] && P->hand[1][6] == pk[24] &&
          P->hand[1][7] == pk[25] && P->hand[2][5] == pk[26] && P->hand[3][7] == pk[31]);
}

static void test_warmup(void)
{
    int done = 0, packs = 0, differ = 0;
    uint8_t a[32], b[32], prev[32] = {0};
    for (int v = 0; v < BEL_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 300; seed++) {
            done += bel_warmup(a, v, seed * 2654435761u);
            packs += is_pack(a);
            differ += memcmp(a, prev, 32) != 0;
            memcpy(prev, a, 32);
        }
    CHECK(done == 600 && packs == 600 && differ == 600);
    printf("   warm-up deals: %d of 600 played out, %d whole packs\n", done, packs);
    CHECK(bel_warmup(a, BEL_CLASSIC, 77) && bel_warmup(b, BEL_CLASSIC, 77) && !memcmp(a, b, 32));   /* a seed, a pack */
    bel_new(&G, BEL_CLASSIC, 77);
    CHECK(!memcmp(G.start, a, 32) && dealt_from(P, a));
}

/* a deal played out: each team's pile bottom up (its tricks as won, in their order of play), the defenders' on the
 * takers'; then the same game twice gives the same next deal, from that pack cut */
static void test_gather(void)
{
    uint8_t pack[32], want[32];
    for (int v = 0; v < BEL_VARIANTS; v++) {
        bel_new(&G, v, 21);
        CHECK(bel_script(&G, v == BEL_CLASSIC ? "p t" : "p b80h p p p") && P->phase == BP_PLAY);
        bel_gather(P, pack);
        CHECK(is_pack(pack));
        CHECK(bel_script(&G, ". . . . . ."));   /* a trick and a half: every card, once */
        bel_gather(P, pack);
        int t = bel_team(P->taker), w = bel_team(P->last_winner);
        CHECK(is_pack(pack) && P->nheap[w] == 4 && !memcmp(P->heap[w], P->last, 4) && P->ntrick == 2);
        CHECK(bel_script(&G, "#") && P->phase == BP_DEAL);
        CHECK(P->nheap[0] + P->nheap[1] == 32 && P->nheap[0] == 4 * P->won[0]);
        w = bel_team(P->last_winner);
        CHECK(!memcmp(P->heap[w] + P->nheap[w] - 4, P->last, 4));   /* the last trick on its pile's top */
        bel_gather(P, pack);
        int k = 0;                              /* top first: the defenders' pile reversed, then the takers' */
        for (int i = P->nheap[1 - t] - 1; i >= 0; i--) want[k++] = P->heap[1 - t][i];
        for (int i = P->nheap[t] - 1; i >= 0; i--) want[k++] = P->heap[t][i];
        CHECK(k == 32 && !memcmp(pack, want, 32) && is_pack(pack));
        H = G;
        CHECK(bel_script(&G, "N") && bel_script(&H, "N") && !memcmp(&G.pos, &H.pos, sizeof G.pos));
        CHECK(dealt_from(P, pack) && P->dealer == 0 && P->cutter == 3);
        /* undo goes back across the next deal to the same pack */
        CHECK(bel_undo(&G) && P->phase == BP_DEAL && bel_script(&G, "N") && !memcmp(&G.pos, &H.pos, sizeof G.pos));
    }
}

/* a deal passed out: the hands as held (sorted), from the dealer's next, each on the one before; classic: the rest of
 * the pack on them, the turned card on its top where it lies. No shuffle: the next deal is that pack, cut. */
static void test_redeal(void)
{
    uint8_t pack[32], want[32];
    for (int v = 0; v < BEL_VARIANTS; v++) {
        bel_new(&G, v, 7);
        H = G;
        const bpos *q = &H.pos;
        int k = 0;
        if (v == BEL_CLASSIC) {
            want[k++] = q->turned;
            for (int i = q->top; i < 32; i++) want[k++] = q->pack[i];
        }
        for (int j = 3; j >= 0; j--) {
            int pl = (q->dealer + 1 + j) % 4;
            uint8_t h[BEL_HAND];
            memcpy(h, q->hand[pl], q->n[pl]);
            bel_sort(h, q->n[pl], BEL_NONE);
            for (int i = q->n[pl] - 1; i >= 0; i--) want[k++] = h[i];
        }
        CHECK(k == 32 && is_pack(want));
        bel_gather(P, pack);
        CHECK(!memcmp(pack, want, 32));
        CHECK(bel_script(&G, v == BEL_CLASSIC ? "p p p p p p p p" : "p p p p"));
        CHECK(P->phase == (v == BEL_CLASSIC ? BP_BID1 : BP_AUCTION) && P->dealer == 0 && P->cutter == 3);
        CHECK(dealt_from(P, want) && P->dealt == 2 && P->deal == 0);
        /* the hand sorted as on the screen: the suits spades, hearts, clubs, diamonds, the strongest first */
        uint8_t h[BEL_HAND] = {CARD(D, 1), CARD(S, 7), CARD(C, 11), CARD(HE, 10), CARD(S, 1), CARD(HE, 11)};
        bel_sort(h, 6, BEL_NONE);
        CHECK(h[0] == CARD(S, 1) && h[1] == CARD(S, 7) && h[2] == CARD(HE, 10) && h[3] == CARD(HE, 11) &&
              h[4] == CARD(C, 11) && h[5] == CARD(D, 1));
        bel_sort(h, 6, HE);
        CHECK(h[2] == CARD(HE, 11) && h[3] == CARD(HE, 10));
    }
    /* a pack carries over to a game of the other variant */
    bel_new(&G, BEL_CLASSIC, 9);
    CHECK(bel_script(&G, "t #"));
    bel_gather(P, pack);
    bel_start(&H, BEL_COINCHE, 1234, pack);
    CHECK(!memcmp(H.start, pack, 32) && dealt_from(&H.pos, pack) && H.pos.variant == BEL_COINCHE);
    /* a bad pack (a card twice): a whole pack all the same */
    pack[1] = pack[0];
    bel_start(&H, BEL_CLASSIC, 1, pack);
    CHECK(is_pack(H.start) && is_pack(H.pos.pack));
}

/* how bunched the hands are: the longest suit per hand, the packets all of one suit, the longest run of one suit in
 * the pack as dealt */
typedef struct { double longest, run; int hands, deals, packets[4], npk[4]; } clump;   /* packets: by size (2, 3) */

static void measure(clump *m, const bpos *p)
{
    for (int q = 0; q < 4; q++) {
        int per[4] = {0}, top = 0;
        for (int i = 0; i < p->n[q]; i++) per[CARD_SUIT(p->hand[q][i])]++;
        for (int s = 0; s < 4; s++) top = per[s] > top ? per[s] : top;
        m->longest += top;
        m->hands++;
    }
    static const int classic[] = {3, 3, 3, 3, 2, 2, 2, 2}, coinche[] = {3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2};
    const int *pk = p->variant == BEL_CLASSIC ? classic : coinche;
    int n = p->variant == BEL_CLASSIC ? 8 : 12;
    for (int j = 0, at = 0; j < n; at += pk[j++]) {
        int s = CARD_SUIT(p->pack[at]), same = 1;
        for (int i = 1; i < pk[j]; i++) same &= CARD_SUIT(p->pack[at + i]) == s;
        m->packets[pk[j]] += same;
        m->npk[pk[j]]++;
    }
    int run = 1, best = 1;
    for (int i = 1; i < 32; i++) {
        run = CARD_SUIT(p->pack[i]) == CARD_SUIT(p->pack[i - 1]) ? run + 1 : 1;
        best = run > best ? run : best;
    }
    m->run += best;
    m->deals++;
}

static void report(const char *what, const clump *m)
{
    if (!m->deals) return;
    printf("     %-28s %3d deals: longest suit per hand %.2f; packets of one suit: of 3 %4.1f%%, of 2 %4.1f%%; "
           "longest run of one suit in the pack %.2f\n", what, m->deals, m->longest / m->hands,
           100.0 * m->packets[3] / m->npk[3], 100.0 * m->packets[2] / m->npk[2], m->run / m->deals);
}

static int pick(const bpos *p, int *acts, int n)   /* a random legal action; bids rarer than passes */
{
    int a = acts[rnd(n)];
    if ((p->phase == BP_AUCTION || p->phase == BP_BID2 || p->phase == BP_BID1) && rnd(3) && bel_legal(p, BA_PASS))
        a = BA_PASS;
    return a;
}

static void test_clumps(void)
{
    static const char *NAME[2] = {"classic", "coinche"};
    for (int v = 0; v < BEL_VARIANTS; v++) {
        clump real = {0}, after_play = {0}, after_void = {0}, shuffled = {0};
        uint8_t pack[32];
        rng = 2026 + (uint32_t)v;
        bel_warmup(pack, v, 99);
        while (real.deals < 500) {              /* games in a row, as in a session: each from the last one's cards */
            bel_start(&G, v, (uint32_t)rnd(1 << 30), pack);
            int prev_void = 0;
            for (;;) {
                measure(&real, P);
                measure(prev_void ? &after_void : &after_play, P);
                int dealt = P->dealt;
                while (P->phase != BP_DEAL && P->phase != BP_OVER && P->dealt == dealt) {
                    int acts[64], k = bel_moves(P, acts);
                    if (!k || !bel_play(&G, pick(P, acts, k))) break;
                }
                if (P->phase == BP_OVER || real.deals >= 500) break;
                prev_void = P->dealt != dealt;  /* passed out: already dealt again */
                if (!prev_void) bel_play(&G, BA_NEXT);
            }
            bel_gather(P, pack);
            CHECK(is_pack(pack));
        }
        for (uint32_t seed = 1; seed <= 500; seed++) {   /* the same deal from packs shuffled at random */
            cards_pack(PACK_32, pack);
            cards_shuffle(pack, 32, seed * 40503u + (uint32_t)v);
            bel_start(&H, v, seed, pack);
            measure(&shuffled, &H.pos);
        }
        printf("   %s:\n", NAME[v]);
        report("gathered and cut:", &real);
        report("  after a deal played:", &after_play);
        report("  after a deal passed out:", &after_void);
        report("shuffled:", &shuffled);
        CHECK(real.longest / real.hands > shuffled.longest / shuffled.hands);
        CHECK(after_play.longest / after_play.hands > shuffled.longest / shuffled.hands);
        for (int k = 2; k <= 3; k++)
            CHECK((double)real.packets[k] / real.npk[k] > (double)shuffled.packets[k] / shuffled.npk[k]);
        CHECK(real.run / real.deals > shuffled.run / shuffled.deals);
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
    test_cut();
    test_warmup();
    test_gather();
    test_redeal();
    test_clumps();
    printf("belote: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
