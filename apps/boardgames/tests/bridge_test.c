/*
 * Contract bridge: the rules' test (the deal, the auction's calls and its end, the declarer, the dummy and who holds
 * the device, following suit and trumps, the score table, Chicago's vulnerability and its four deals, the rubber's
 * games, honours and bonus, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/bridge.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static brg_game G, H;
static brg_pos *P = &G.pos;

static void test_deal(void)
{
    brg_new(&G, BRG_CHICAGO, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->n[pl] == 13);
        for (int i = 0; i < 13; i++) seen[P->hand[pl][i]]++, total++;
    }
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(once && total == 52 && P->phase == BRG_AUCTION && P->turn == 0 && P->dealer == 0);
    CHECK(brg_dummy(P) == BRG_NONE && !brg_dummy_shown(P) && brg_controller(P) == 0);
    brg_new(&H, BRG_CHICAGO, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_auction(void)
{
    brg_new(&G, BRG_CHICAGO, 7);
    CHECK(brg_can_call(P, BRG_PASS) && brg_can_call(P, BRG_BID(1, BRG_CLUBS)) && brg_can_call(P, BRG_BID(7, BRG_NT)));
    CHECK(!brg_can_call(P, BRG_DOUBLE) && !brg_can_call(P, BRG_REDOUBLE) && !brg_can_call(P, 38));
    CHECK(!brg_legal(P, BRG_APLAY | P->hand[0][0]) && !brg_legal(P, BRG_ANEXT));
    CHECK(brg_script(&G, "b1h"));
    CHECK(P->turn == 1 && P->bid == BRG_BID(1, BRG_HEARTS) && P->bidder == 0);
    CHECK(!brg_can_call(P, BRG_BID(1, BRG_CLUBS)) && !brg_can_call(P, BRG_BID(1, BRG_HEARTS)));
    CHECK(brg_can_call(P, BRG_BID(1, BRG_SPADES)) && brg_can_call(P, BRG_DOUBLE) && !brg_can_call(P, BRG_REDOUBLE));
    CHECK(brg_script(&G, "x"));                 /* 1 doubles 0's bid */
    CHECK(P->dbl == 1 && !brg_can_call(P, BRG_DOUBLE) && brg_can_call(P, BRG_REDOUBLE));   /* 2: the partner */
    CHECK(brg_script(&G, "p"));
    CHECK(P->turn == 3 && !brg_can_call(P, BRG_REDOUBLE) && !brg_can_call(P, BRG_DOUBLE));   /* 3: an opponent */
    CHECK(brg_script(&G, "p"));
    CHECK(brg_can_call(P, BRG_REDOUBLE) && brg_script(&G, "xx") && P->dbl == 2 && P->passes == 0);
    CHECK(!brg_script(&G, "x") && !brg_script(&G, "xx"));
    /* a new bid clears the double; the declarer is the first of the side to name the strain */
    CHECK(brg_script(&G, "b2h") && P->dbl == 0 && P->bidder == 1);   /* 1 bids hearts too (the other side) */
    CHECK(brg_script(&G, "b3h") && brg_script(&G, "p p"));   /* 2 raises: side 0's hearts, named first by 0 */
    CHECK(P->phase == BRG_AUCTION && P->turn == 1);
    CHECK(brg_script(&G, "p"));
    CHECK(P->phase == BRG_PLAY && P->level == 3 && P->strain == BRG_HEARTS && P->contract_dbl == 0);
    CHECK(P->declarer == 0 && brg_dummy(P) == 2 && P->turn == 1 && P->leader == 1);
    CHECK(P->ncalls == 10 && P->call[0] == BRG_BID(1, BRG_HEARTS) && P->call[1] == BRG_DOUBLE);
    /* a doubled contract stays doubled; the declarer of the other side */
    brg_new(&G, BRG_CHICAGO, 9);
    CHECK(brg_script(&G, "p b1s b2c x p p p"));
    CHECK(P->phase == BRG_PLAY && P->declarer == 2 && P->contract_dbl == 1 && P->level == 2 && P->turn == 3);
    /* passed out: Chicago deals again by the same dealer */
    brg_new(&G, BRG_CHICAGO, 9);
    CHECK(brg_script(&G, "p p p"));
    CHECK(P->phase == BRG_AUCTION && P->turn == 3);
    CHECK(brg_script(&G, "p") && P->phase == BRG_SCORE && P->passed_out && P->deals == 0 && P->nrec == 0);
    uint8_t before[13];
    memcpy(before, P->hand[0], 13);
    CHECK(brg_script(&G, "N") && P->phase == BRG_AUCTION && P->dealer == 0 && P->turn == 0 && P->dealt == 2);
    CHECK(memcmp(before, P->hand[0], 13) != 0 && !P->passed_out);
    /* the rubber: the next dealer */
    brg_new(&G, BRG_RUBBER, 9);
    CHECK(brg_script(&G, "p p p p N") && P->dealer == 1 && P->turn == 1);
    /* the longest auction fits */
    brg_new(&G, BRG_CHICAGO, 11);
    for (int c = 1; c <= 35; c++) {
        CHECK(brg_play(&G, BRG_ACALL | c));
        CHECK(brg_play(&G, BRG_ACALL | BRG_PASS) && brg_play(&G, BRG_ACALL | BRG_PASS));
        CHECK(brg_play(&G, BRG_ACALL | BRG_DOUBLE));
        CHECK(brg_play(&G, BRG_ACALL | BRG_PASS) && brg_play(&G, BRG_ACALL | BRG_PASS));
        CHECK(brg_play(&G, BRG_ACALL | BRG_REDOUBLE));
        if (c < 35) CHECK(brg_play(&G, BRG_ACALL | BRG_PASS) && brg_play(&G, BRG_ACALL | BRG_PASS));
    }
    CHECK(brg_script(&G, "p p p") && P->phase == BRG_PLAY && P->level == 7 && P->strain == BRG_NT);
    CHECK(P->contract_dbl == 2 && P->ncalls == 35 * 9 - 2 + 3);
}

/* hands set by hand, a contract, the play phase: the declarer's left-hand opponent to lead */
static void set_hands(int variant, const char *const *hs, int declarer, int level, int strain, int dbl)
{
    brg_new(&G, variant, 3);
    memset(P->hand, CARD_NONE, sizeof P->hand);   /* nothing left of the deal */
    for (int pl = 0; pl < 4; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        trick_sort(P->hand[pl], P->n[pl]);
    }
    memcpy(P->orig, P->hand, sizeof P->orig);
    P->declarer = (uint8_t)declarer, P->level = (uint8_t)level, P->strain = (uint8_t)strain;
    P->contract_dbl = (uint8_t)dbl;
    P->phase = BRG_PLAY;
    P->turn = P->leader = (uint8_t)((declarer + 1) % 4);
    G.n = 0;
}

static void test_play(void)
{
    static const char *const a[4] = {"As 2h 3c", "Kh 4h 2s", "5h 3s 4c", "6c 7c 8c"};
    set_hands(BRG_CHICAGO, a, 3, 1, BRG_SPADES, 0);   /* 3 declares spades: 0 leads, 2 is the dummy */
    CHECK(P->turn == 0 && brg_dummy(P) == 1 && brg_controller(P) == 0 && !brg_dummy_shown(P));
    CHECK(brg_script(&G, "2h"));               /* the opening lead: the dummy lies face up */
    CHECK(brg_dummy_shown(P) && P->turn == 1 && brg_controller(P) == 3);   /* the declarer plays the dummy */
    CHECK(!brg_script(&G, "2s"));               /* follow suit */
    CHECK(brg_script(&G, "Kh") && brg_controller(P) == 2 && brg_script(&G, "5h"));
    CHECK(brg_controller(P) == 3 && brg_script(&G, "6c"));   /* void: anything */
    CHECK(P->last_winner == 1 && P->won[1] == 1 && P->turn == 1 && brg_controller(P) == 3);   /* the dummy leads */
    CHECK(brg_script(&G, "4h 3s"));             /* a spade on a heart: the trump wins */
    CHECK(brg_script(&G, "7c As"));
    CHECK(P->last_winner == 0 && P->won[0] == 1 && P->tricks == 2);
    CHECK(P->keeper[1] == 3 && P->keeper[0] == 0);   /* the declarer keeps the dummy's trick; the first defender */
    /* no trumps: the suit led wins, whatever is played on it */
    static const char *const b[4] = {"2h", "Ah", "As", "Kh"};
    set_hands(BRG_CHICAGO, b, 3, 1, BRG_NT, 0);
    CHECK(brg_script(&G, "2h Ah As Kh") && P->last_winner == 1);
    static const char *const c[4] = {"2h", "Ah", "2s", "Kh"};
    set_hands(BRG_CHICAGO, c, 3, 1, BRG_SPADES, 0);
    CHECK(brg_script(&G, "2h Ah 2s Kh") && P->last_winner == 2);
}

static int made(int level, int strain, int dbl, int vul, int taken)
{
    int below, ad, af;
    brg_score(level, strain, dbl, vul, taken, 1, &below, &ad, &af);
    return af ? -af : below + ad;
}

static void test_score(void)
{
    CHECK(made(1, BRG_NT, 0, 0, 7) == 90);
    CHECK(made(2, BRG_HEARTS, 0, 0, 8) == 110);
    CHECK(made(3, BRG_CLUBS, 0, 1, 10) == 130);
    CHECK(made(3, BRG_NT, 0, 0, 9) == 400 && made(3, BRG_NT, 0, 0, 10) == 430 && made(3, BRG_NT, 0, 1, 9) == 600);
    CHECK(made(4, BRG_SPADES, 0, 1, 10) == 620 && made(4, BRG_HEARTS, 0, 0, 11) == 450);
    CHECK(made(5, BRG_DIAMONDS, 0, 0, 11) == 400 && made(5, BRG_CLUBS, 0, 1, 11) == 600);
    CHECK(made(6, BRG_NT, 0, 1, 12) == 1440 && made(6, BRG_SPADES, 0, 0, 12) == 980);
    CHECK(made(7, BRG_NT, 0, 1, 13) == 2220 && made(7, BRG_CLUBS, 0, 0, 13) == 1440);
    CHECK(made(2, BRG_HEARTS, 1, 0, 8) == 470 && made(2, BRG_HEARTS, 1, 1, 8) == 670);
    CHECK(made(1, BRG_SPADES, 2, 0, 7) == 520 && made(1, BRG_SPADES, 2, 0, 8) == 720);
    CHECK(made(1, BRG_CLUBS, 1, 0, 7) == 140 && made(1, BRG_CLUBS, 1, 0, 9) == 340 && made(1, BRG_CLUBS, 1, 1, 9) == 540);
    CHECK(made(7, BRG_SPADES, 2, 1, 13) == 2940);
    CHECK(made(4, BRG_SPADES, 0, 0, 9) == -50 && made(4, BRG_SPADES, 0, 1, 8) == -200);
    CHECK(made(3, BRG_CLUBS, 1, 0, 8) == -100 && made(3, BRG_CLUBS, 1, 0, 7) == -300);
    CHECK(made(3, BRG_CLUBS, 1, 0, 6) == -500 && made(3, BRG_CLUBS, 1, 0, 5) == -800);
    CHECK(made(4, BRG_HEARTS, 1, 1, 9) == -200 && made(4, BRG_HEARTS, 1, 1, 7) == -800);
    CHECK(made(4, BRG_HEARTS, 2, 0, 8) == -600 && made(4, BRG_HEARTS, 2, 1, 9) == -400);
    CHECK(made(7, BRG_NT, 1, 0, 0) == -3500 && made(7, BRG_NT, 2, 1, 0) == -7600);
    /* rubber: no game bonus, the trick points below the line */
    int below, ad, af;
    brg_score(3, BRG_NT, 0, 0, 10, 0, &below, &ad, &af);
    CHECK(below == 100 && ad == 30 && af == 0);
    brg_score(2, BRG_SPADES, 1, 1, 9, 0, &below, &ad, &af);
    CHECK(below == 120 && ad == 200 + 50 && af == 0);
    /* honours */
    uint8_t h[13];
    const char *s = "As Ks Qs Js Ts 2h 3h 4h 5h 6h 7h 8h 9h";
    for (int i = 0; i < 13; i++) { while (*s == ' ') s++; h[i] = (uint8_t)trick_parse(&s); }
    CHECK(brg_honours(h, 13, BRG_SPADES) == 150 && brg_honours(h, 13, BRG_HEARTS) == 0);
    h[0] = CARD(SUIT_CLUBS, 2);
    CHECK(brg_honours(h, 13, BRG_SPADES) == 100 && brg_honours(h, 13, BRG_NT) == 0);
    s = "As Ah Ad Ac 2h 3h 4h 5h 6h 7h 8h 9h Th";
    for (int i = 0; i < 13; i++) { while (*s == ' ') s++; h[i] = (uint8_t)trick_parse(&s); }
    CHECK(brg_honours(h, 13, BRG_NT) == 150 && brg_honours(h, 13, BRG_HEARTS) == 0);
}

/* plays a whole deal from a script: an auction, then plain cards to the end */
static void deal_with(const char *auction)
{
    CHECK(brg_script(&G, auction));
    CHECK(P->phase == BRG_PLAY);
    CHECK(brg_script(&G, "#"));
    CHECK(P->phase == BRG_SCORE || P->phase == BRG_OVER);
}

static void test_chicago(void)
{
    brg_new(&G, BRG_CHICAGO, 21);
    int vul[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};   /* none, the dealer's side (1, then 0), both */
    for (int d = 0; d < 4; d++) {
        CHECK(P->dealer == d && P->turn == d && P->deals == d);
        CHECK(brg_vulnerable(P, 0) == vul[d][0] && brg_vulnerable(P, 1) == vul[d][1]);
        if (d == 1) {                           /* a passed-out deal: the same dealer again, the same vulnerability */
            CHECK(brg_script(&G, "p p p p N") && P->dealer == 1 && P->deals == 1 && brg_vulnerable(P, 1));
        }
        deal_with("b1c p p p");
        const brg_deal *r = &P->rec[P->nrec - 1];
        CHECK(r->vul == (vul[d][0] | vul[d][1] << 1) && r->declarer == d && r->level == 1);
        int below, ad, af;
        brg_score(1, BRG_CLUBS, 0, vul[d][d & 1], r->taken, 1, &below, &ad, &af);
        CHECK(r->below[d & 1] == below && r->above[d & 1] == ad && r->above[(d & 1) ^ 1] == af);
        if (d < 3) CHECK(P->phase == BRG_SCORE && brg_winner(P) == -1 && brg_script(&G, "N"));
    }
    CHECK(P->phase == BRG_OVER && P->nrec == 4 && P->deals == 4);
    int t[2] = {0, 0};
    for (int i = 0; i < 4; i++)
        for (int s = 0; s < 2; s++) t[s] += P->rec[i].below[s] + P->rec[i].above[s];
    CHECK(t[0] == P->total[0] && t[1] == P->total[1]);
    CHECK(brg_winner(P) == (t[0] == t[1] ? 2 : t[1] > t[0]));
    CHECK(!brg_script(&G, "N") && !brg_script(&G, "p"));
}

/* the rubber: the last trick of a deal decides; the score sheet's games, lines and bonus */
static void last_trick(int declarer, int level, int strain, int dbl, int taken_before, int decl_wins)
{
    static const char *const win[4] = {"Ah", "2h", "3h", "4h"};   /* player 0 leads */
    static const char *const lose[4] = {"2h", "Ah", "3h", "4h"};
    brg_pos keep = G.pos;
    set_hands(BRG_RUBBER, decl_wins == (brg_side(declarer) == 0) ? win : lose, declarer, level, strain, dbl);
    brg_pos *p = P;
    memcpy(p->games, keep.games, sizeof p->games);
    memcpy(p->part, keep.part, sizeof p->part);
    memcpy(p->total, keep.total, sizeof p->total);
    memcpy(p->rec, keep.rec, sizeof p->rec);
    p->nrec = keep.nrec, p->deals = keep.deals, p->dealer = keep.dealer;
    p->turn = p->leader = 0;
    p->won[declarer] = (uint8_t)taken_before;
    p->won[(declarer + 1) % 4] = (uint8_t)(12 - taken_before);
    p->tricks = 12;
    CHECK(brg_script(&G, ". . . ."));
}

static void test_rubber(void)
{
    brg_new(&G, BRG_RUBBER, 5);
    last_trick(0, 2, BRG_HEARTS, 0, 7, 1);      /* 2 hearts made exactly: 60 below */
    CHECK(P->phase == BRG_SCORE && P->part[0] == 60 && P->games[0] == 0 && P->rec[0].game == 0);
    CHECK(P->rec[0].below[0] == 60 && P->rec[0].above[0] == 0 && P->dealer == 1);
    last_trick(1, 2, BRG_SPADES, 0, 7, 1);      /* the other side: 60, a part-score of its own */
    CHECK(P->part[1] == 60 && P->part[0] == 60 && P->games[1] == 0);
    last_trick(2, 2, BRG_CLUBS, 0, 8, 1);       /* 2 clubs + 1: 40 below, 60 + 40 = a game; the line */
    CHECK(P->games[0] == 1 && P->part[0] == 0 && P->part[1] == 0 && P->rec[2].game == 1);
    CHECK(P->rec[2].below[0] == 40 && P->rec[2].above[0] == 20);
    CHECK(brg_vulnerable(P, 0) && !brg_vulnerable(P, 1) && P->phase == BRG_SCORE);
    last_trick(0, 4, BRG_SPADES, 1, 8, 0);      /* 4 spades doubled, vulnerable, two down: 500 to the defenders */
    CHECK(P->rec[3].above[1] == 500 && P->rec[3].below[0] == 0 && P->games[0] == 1);
    last_trick(3, 3, BRG_NT, 0, 8, 1);          /* 3NT made by side 1: a game each */
    CHECK(P->games[1] == 1 && P->rec[4].game == 2 && P->phase == BRG_SCORE && brg_vulnerable(P, 1));
    last_trick(0, 4, BRG_HEARTS, 0, 9, 1);      /* 4 hearts made: the rubber, two games to one: 500 */
    CHECK(P->phase == BRG_OVER && P->games[0] == 2 && P->rec[5].rubber == 1 && P->rec[5].bonus == 500);
    int t[2] = {0, 0};
    for (int i = 0; i < P->nrec; i++)
        for (int s = 0; s < 2; s++) t[s] += P->rec[i].below[s] + P->rec[i].above[s];
    CHECK(t[0] == P->total[0] && t[1] == P->total[1]);
    CHECK(P->total[0] == 60 + 60 + 120 + 500 && P->total[1] == 60 + 500 + 100);
    CHECK(brg_winner(P) == 0);
    /* two games to none: 700; honours to the hand that holds them, even a defender's */
    brg_new(&G, BRG_RUBBER, 5);
    last_trick(0, 4, BRG_HEARTS, 0, 9, 1);
    CHECK(P->games[0] == 1);
    last_trick(2, 3, BRG_NT, 0, 8, 1);
    CHECK(P->phase == BRG_OVER && P->rec[1].bonus == 700 && P->rec[1].rubber == 1);
    static const char *const hon[4] = {"2c Ah", "Kh Qh", "2h 3c", "3h 4c"};
    brg_new(&G, BRG_RUBBER, 5);
    set_hands(BRG_RUBBER, hon, 0, 1, BRG_HEARTS, 0);
    P->orig[1][0] = CARD(SUIT_HEARTS, RANK_ACE);   /* player 1 held the five honours: ace, king, queen, jack, 10 */
    P->orig[1][1] = CARD(SUIT_HEARTS, RANK_KING), P->orig[1][2] = CARD(SUIT_HEARTS, RANK_QUEEN);
    P->orig[1][3] = CARD(SUIT_HEARTS, RANK_JACK), P->orig[1][4] = CARD(SUIT_HEARTS, 10);
    P->orig[0][0] = CARD(SUIT_CLUBS, 2), P->orig[0][1] = CARD(SUIT_HEARTS, 2);
    for (int i = 5; i < 13; i++) P->orig[1][i] = CARD(SUIT_CLUBS, 3 + i - 5);
    P->won[0] = 0, P->won[1] = 11;
    P->tricks = 11;
    CHECK(brg_script(&G, ". . . . . . . ."));
    CHECK(P->phase == BRG_SCORE && P->rec[0].honours[1] == 150 && P->rec[0].above[1] == 150 + 6 * 50);
}

static void test_undo_script(void)
{
    brg_new(&G, BRG_CHICAGO, 77);
    CHECK(brg_script(&G, "b1n p b3n p p p"));
    CHECK(P->phase == BRG_PLAY && P->declarer == 0 && P->strain == BRG_NT && P->turn == 1);
    brg_pos snap = G.pos;
    CHECK(brg_script(&G, ". . ."));
    CHECK(brg_undo(&G) && brg_undo(&G) && brg_undo(&G));
    CHECK(!memcmp(&snap, &G.pos, sizeof snap));
    CHECK(!brg_script(&G, "b9c") && !brg_script(&G, "b1z") && !brg_script(&G, "Zz"));
    brg_new(&G, BRG_RUBBER, 77);
    CHECK(brg_script(&G, "!") && P->phase == BRG_OVER && brg_winner(P) >= 0);
}

/* whole games from seeds: random legal actions (the auction mostly passes), the rules' invariants */
static uint32_t rnd(uint32_t *s) { return cards_rand(s); }

static void random_game(int variant, uint32_t seed)
{
    uint32_t r = seed * 7919u + 1;
    brg_new(&G, variant, seed);
    int steps = 0, bad = 0;
    while (P->phase != BRG_OVER && steps < 12000) {
        steps++;
        int act = 0;
        if (P->phase == BRG_AUCTION) {
            int k = rnd(&r) % 10;
            if (k < 6) act = BRG_ACALL | BRG_PASS;
            else if (k == 6 && brg_can_call(P, BRG_DOUBLE)) act = BRG_ACALL | BRG_DOUBLE;
            else if (k == 7 && brg_can_call(P, BRG_REDOUBLE)) act = BRG_ACALL | BRG_REDOUBLE;
            else {
                int c = P->bid + 1 + (int)(rnd(&r) % 4);
                act = BRG_ACALL | (c <= 35 ? c : BRG_PASS);
            }
        } else if (P->phase == BRG_PLAY) {
            const uint8_t *h = P->hand[P->turn];
            int n = P->n[P->turn], legal[13], k = 0;
            for (int i = 0; i < n; i++)
                if (brg_can_play(P, h[i])) legal[k++] = h[i];
            bad |= k == 0;
            if (k) act = BRG_APLAY | legal[rnd(&r) % k];
            int cards = 0;
            for (int pl = 0; pl < 4; pl++) cards += P->n[pl];
            bad |= cards + P->ntrick + 4 * P->tricks != 52;
            bad |= P->turn == brg_dummy(P) ? brg_controller(P) != P->declarer : brg_controller(P) != P->turn;
            bad |= brg_dummy_shown(P) != (P->tricks > 0 || P->ntrick > 0);
        } else act = BRG_ANEXT;
        int tricks_before = P->tricks, phase = P->phase;
        if (!brg_play(&G, act)) { bad = 1; break; }
        if (phase == BRG_PLAY && P->phase != BRG_PLAY) {
            const brg_deal *d = &P->rec[P->nrec - 1];
            int w = 0;
            for (int pl = 0; pl < 4; pl++) w += P->won[pl];
            bad |= w != 13 || tricks_before != 12 || P->tricks != 13;
            bad |= d->taken != P->won[d->declarer] + P->won[(d->declarer + 2) % 4];
        }
    }
    int t[2] = {0, 0};
    for (int i = 0; i < P->nrec; i++)
        for (int s = 0; s < 2; s++) t[s] += P->rec[i].below[s] + P->rec[i].above[s];
    CHECK(!bad && P->phase == BRG_OVER);
    CHECK(t[0] == P->total[0] && t[1] == P->total[1]);
    if (variant == BRG_CHICAGO) CHECK(P->deals == 4);
    else CHECK(P->games[0] == 2 || P->games[1] == 2);
    /* replayed, the same game */
    brg_new(&H, variant, seed);
    for (int i = 0; i < G.n; i++) brg_play(&H, G.act[i]);
    CHECK(!memcmp(&H.pos, &G.pos, sizeof H.pos));
}

int main(void)
{
    test_deal();
    test_auction();
    test_play();
    test_score();
    test_chicago();
    test_rubber();
    test_undo_script();
    for (uint32_t s = 1; s <= 60; s++) random_game(s & 1 ? BRG_CHICAGO : BRG_RUBBER, s);
    printf("bridge: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
