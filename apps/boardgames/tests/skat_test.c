/*
 * Skat: the rules' test (the bid values, the deal, the bidding duels and a deal thrown in, trumps and following suit,
 * matadors, game values, hand, schneider, schwarz, announcements, ouvert, null, overbidding, the Seeger-Fabian totals,
 * the list's end, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/skat.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static skgame G, H;
static skpos *P = &G.pos;

#define C(s, r) CARD(SUIT_##s, r)
#define J(s) CARD(SUIT_##s, RANK_JACK)

static void test_bids(void)
{
    static const int16_t want[] = {18, 20, 22, 23, 24, 27, 30, 33, 35, 36, 40, 44, 45, 46, 48, 50, 54, 55, 59, 60,
                                   63, 66, 70, 72, 77, 80, 81, 84, 88, 90, 96, 99, 100, 108, 110, 117, 120, 121, 126,
                                   130, 132, 135, 140, 143, 144, 150, 153, 154, 156, 160, 162, 165, 168, 170, 176,
                                   180, 187, 192, 198, 204, 216, 240, 264};
    int16_t got[100];
    int n = skat_bids(got);
    CHECK(n == (int)(sizeof want / sizeof want[0]) && !memcmp(got, want, sizeof want));
    CHECK(skat_next_bid(0) == 18 && skat_next_bid(18) == 20 && skat_next_bid(22) == 23 && skat_next_bid(240) == 264);
    CHECK(skat_next_bid(264) == 0 && skat_deals(SKAT_SHORT) == 12 && skat_deals(SKAT_LONG) == 36);
}

static void test_deal(void)
{
    skat_new(&G, SKAT_SHORT, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 3; pl++) {
        CHECK(P->n[pl] == 10);
        for (int i = 0; i < P->n[pl]; i++) seen[P->hand[pl][i]]++, total++;
    }
    for (int i = 0; i < 2; i++) seen[P->skat[i]]++, total++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == (r == 1 || r >= 7);
    CHECK(once && total == 32 && P->nskat == 2);
    CHECK(P->dealer == 2 && P->phase == SK_BID && P->sayer == 1 && P->hearer == 0 && P->turn == 1);
    skat_new(&H, SKAT_SHORT, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    /* the hand as shown: jacks first (clubs, spades, hearts, diamonds), then suits, the strongest first */
    uint8_t h[6] = {C(HEARTS, 10), J(HEARTS), C(CLUBS, 7), J(CLUBS), C(HEARTS, RANK_ACE), C(HEARTS, RANK_KING)};
    skat_sort(h, 6, -1);
    CHECK(h[0] == J(CLUBS) && h[1] == J(HEARTS) && h[2] == C(CLUBS, 7) && h[3] == C(HEARTS, RANK_ACE) &&
          h[4] == C(HEARTS, 10) && h[5] == C(HEARTS, RANK_KING));
    skat_sort(h, 6, SUIT_HEARTS);               /* trumps first: the jacks, then the hearts */
    CHECK(h[0] == J(CLUBS) && h[1] == J(HEARTS) && h[2] == C(HEARTS, RANK_ACE) && h[5] == C(CLUBS, 7));
    skat_sort(h, 6, SG_NULL);                   /* null: the jacks in their suits, A K Q J 10 */
    CHECK(h[0] == J(CLUBS) && h[1] == C(CLUBS, 7) && h[2] == C(HEARTS, RANK_ACE) && h[3] == C(HEARTS, RANK_KING) &&
          h[4] == J(HEARTS) && h[5] == C(HEARTS, 10));
}

static void test_bidding(void)
{
    /* middlehand (1) says to forehand (0); then rearhand (2) to the survivor */
    skat_new(&G, SKAT_SHORT, 5);
    CHECK(!skat_legal(P, SA_HOLD) && !skat_legal(P, SA_SAY | 19) && !skat_legal(P, SA_SAY | 17));
    CHECK(skat_saying(P) && skat_script(&G, "b18"));
    CHECK(P->turn == 0 && !skat_saying(P) && !skat_legal(P, SA_SAY | 20) && skat_legal(P, SA_HOLD));
    CHECK(skat_script(&G, "h b20") && P->turn == 0 && P->word[0] == SW_HELD && P->said[0] == 18);
    CHECK(skat_script(&G, "p") && P->stage == 2 && P->sayer == 2 && P->hearer == 1 && P->turn == 2 && P->bid == 20);
    CHECK(!skat_script(&G, "b20") && skat_script(&G, "b22 h"));
    CHECK(skat_script(&G, "p") && P->phase == SK_SKAT && P->declarer == 1 && P->turn == 1 && P->bid == 22);
    /* the hearer passes at once: the sayer goes on with rearhand */
    skat_new(&G, SKAT_SHORT, 5);
    CHECK(skat_script(&G, "b18 p p") && P->phase == SK_SKAT && P->declarer == 1 && P->bid == 18);
    /* nobody says anything: forehand may bid, or the deal is thrown in */
    skat_new(&G, SKAT_SHORT, 5);
    CHECK(skat_script(&G, "p p") && P->stage == 3 && P->turn == 0 && skat_saying(P) && !skat_legal(P, SA_HOLD));
    CHECK(skat_script(&G, "b18") && P->phase == SK_SKAT && P->declarer == 0 && P->bid == 18);
    skat_new(&G, SKAT_SHORT, 5);
    CHECK(skat_script(&G, "p p p") && P->phase == SK_DEAL && P->ndeals == 1 && P->list[0].declarer == SKAT_NONE);
    CHECK(P->score[0] == 0 && skat_total(P, 0) == 0 && P->turn == 2);
    CHECK(skat_script(&G, "N") && P->deal == 1 && P->dealer == 0 && P->sayer == 2 && P->hearer == 1);
}

/* a deal set by hand: the hands and the skat, the declarer and his bid, waiting for him to take the skat or not */
static void set_deal(const char *const *hs, const char *skat, int declarer, int bid)
{
    skat_new(&G, SKAT_SHORT, 3);
    for (int pl = 0; pl < 3; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        skat_sort(P->hand[pl], P->n[pl], -1);
    }
    const char *s = skat;
    P->skat[0] = (uint8_t)trick_parse(&s);
    while (*s == ' ') s++;
    P->skat[1] = (uint8_t)trick_parse(&s);
    P->nskat = 2;
    P->bid = (int16_t)bid;
    P->declarer = (uint8_t)declarer;
    P->turn = (uint8_t)declarer;
    P->phase = SK_SKAT;
    G.n = 0;
}

/* every club and jack with forehand (0), the 7 of clubs in the skat */
static const char *const STRONG[3] = {"Jc Js Jh Jd Ac Tc Kc Qc 9c 8c", "Ts Ks Qs 9s 8s 7s Ah Th Kh Qh",
                                      "9h 8h 7h Ad Td Kd Qd 9d 8d 7d"};

static void test_cards(void)
{
    CHECK(skat_points(C(SPADES, RANK_ACE)) == 11 && skat_points(C(SPADES, 10)) == 10 &&
          skat_points(C(SPADES, RANK_KING)) == 4 && skat_points(C(SPADES, RANK_QUEEN)) == 3 &&
          skat_points(J(SPADES)) == 2 && skat_points(C(SPADES, 9)) == 0);
    int sum = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (r == 1 || r >= 7) sum += skat_points(CARD(s, r));
    CHECK(sum == 120);
    CHECK(skat_base(SUIT_DIAMONDS, 0, 0) == 9 && skat_base(SUIT_HEARTS, 0, 0) == 10 &&
          skat_base(SUIT_SPADES, 0, 0) == 11 && skat_base(SUIT_CLUBS, 0, 0) == 12 && skat_base(SG_GRAND, 0, 0) == 24);
    CHECK(skat_base(SG_NULL, 0, 0) == 23 && skat_base(SG_NULL, 0, 1) == 35 && skat_base(SG_NULL, SF_OUVERT, 0) == 46 &&
          skat_base(SG_NULL, SF_OUVERT, 1) == 59);
    /* the trumps: the jacks in order, then the suit's A 10 K Q 9 8 7 */
    CHECK(skat_trump(J(DIAMONDS), SUIT_HEARTS) && skat_trump(J(DIAMONDS), SG_GRAND) && !skat_trump(J(DIAMONDS), SG_NULL));
    CHECK(!skat_trump(C(HEARTS, RANK_ACE), SG_GRAND) && skat_trump(C(HEARTS, 7), SUIT_HEARTS));
    uint8_t t1[3] = {C(HEARTS, RANK_ACE), J(DIAMONDS), C(HEARTS, 10)};
    CHECK(skat_trick_best(t1, 3, SUIT_HEARTS) == 1 && skat_trick_best(t1, 3, SG_NULL) == 0);
    uint8_t t2[3] = {J(DIAMONDS), J(CLUBS), J(SPADES)};
    CHECK(skat_trick_best(t2, 3, SG_GRAND) == 1);
    uint8_t t3[3] = {C(SPADES, 10), C(SPADES, RANK_KING), C(SPADES, RANK_ACE)};
    CHECK(skat_trick_best(t3, 3, SUIT_HEARTS) == 2);
    uint8_t t4[3] = {C(SPADES, 10), C(SPADES, RANK_KING), C(CLUBS, RANK_ACE)};   /* 10 over king, no trump */
    CHECK(skat_trick_best(t4, 3, SUIT_HEARTS) == 0 && skat_trick_best(t4, 3, SG_NULL) == 1);
    uint8_t t5[3] = {C(SPADES, 10), J(SPADES), C(SPADES, RANK_QUEEN)};   /* null: the jack between queen and 10 */
    CHECK(skat_trick_best(t5, 3, SG_NULL) == 2 && skat_trick_best(t5, 3, SUIT_DIAMONDS) == 1);
    uint8_t t6[3] = {C(SPADES, 7), C(HEARTS, 7), C(DIAMONDS, RANK_ACE)};   /* off-suit cards never win */
    CHECK(skat_trick_best(t6, 3, SG_GRAND) == 0 && skat_trick_best(t6, 3, SUIT_HEARTS) == 1);
    /* matadors */
    CHECK(skat_matadors((const uint8_t[]){J(CLUBS), J(SPADES), C(HEARTS, RANK_ACE)}, 3, SG_GRAND) == 2);
    CHECK(skat_matadors((const uint8_t[]){J(CLUBS), J(SPADES), J(HEARTS), J(DIAMONDS)}, 4, SG_GRAND) == 4);
    CHECK(skat_matadors((const uint8_t[]){J(HEARTS), C(HEARTS, RANK_ACE)}, 2, SG_GRAND) == -2);
    CHECK(skat_matadors((const uint8_t[]){C(HEARTS, RANK_ACE)}, 1, SG_GRAND) == -4);
    CHECK(skat_matadors((const uint8_t[]){C(HEARTS, 10)}, 1, SUIT_HEARTS) == -5);
    CHECK(skat_matadors((const uint8_t[]){J(CLUBS), J(SPADES), J(HEARTS), J(DIAMONDS), C(HEARTS, RANK_ACE),
                                          C(HEARTS, 10), C(HEARTS, 9)}, 7, SUIT_HEARTS) == 6);
    /* following suit: a jack is a trump, not of its suit */
    static const char *const f[3] = {"As 7h 8h", "Js Ac 7c", "Ks 7d 8d"};
    set_deal(f, "9c 8c", 0, 18);
    CHECK(skat_script(&G, "gh"));               /* a hand game in hearts */
    CHECK(P->phase == SK_PLAY && P->turn == 0 && P->hand_game);
    CHECK(skat_script(&G, "As") && skat_can_play(P, J(SPADES)) && skat_can_play(P, C(CLUBS, 7)));
    CHECK(skat_script(&G, "Ac") && !skat_can_play(P, C(DIAMONDS, 7)) && skat_script(&G, "Ks"));
    CHECK(P->last_winner == 0 && P->points[0] == 11 + 11 + 4 && P->turn == 0);
    CHECK(skat_script(&G, "7h") && !skat_can_play(P, C(CLUBS, 7)) && skat_can_play(P, J(SPADES)));
}

static void test_games(void)
{
    /* a hand game with 11 (the 7 of clubs in the skat), every trick: schneider and schwarz, 15 levels: 180 */
    set_deal(STRONG, "7c As", 0, 18);
    CHECK(skat_matadors(P->hand[0], 10, SUIT_CLUBS) == 10);
    CHECK(skat_script(&G, "gc #"));
    skdeal *r = &P->list[0];
    CHECK(P->phase == SK_DEAL && r->declarer == 0 && r->won && r->hand && r->matadors == 11 && r->schneider &&
          r->schwarz && r->levels == 15 && r->value == 180 && r->score == 180 && r->points == 120 && r->tricks == 10);
    CHECK(P->score[0] == 180 && skat_total(P, 0) == 230 && skat_total(P, 1) == 0 && P->wins[0] == 1);
    /* the same announced schwarz, ouvert: 18 levels, 216 */
    set_deal(STRONG, "7c As", 0, 18);
    CHECK(!skat_legal(P, SKA_GAME(SUIT_CLUBS, SF_SCHWARZ)) && !skat_legal(P, SKA_GAME(SUIT_CLUBS, SF_OUVERT)));
    CHECK(skat_script(&G, "gcSZO") && P->flags == 7 && skat_script(&G, "#"));
    CHECK(P->list[0].won && P->list[0].levels == 18 && P->list[0].value == 216);
    /* picked up: no hand level; announcements are for hand games only */
    set_deal(STRONG, "7c As", 0, 18);
    CHECK(skat_script(&G, "s") && P->phase == SK_DISCARD && P->n[0] == 12 && P->nskat == 0);
    CHECK(!skat_legal(P, SKA_GAME(SUIT_CLUBS, 0)) && skat_script(&G, "dAs d8c u8c") && P->nskat == 1);
    CHECK(skat_script(&G, "d9c") && !skat_legal(P, SA_PUT | C(CLUBS, 8)) && !skat_legal(P, SKA_GAME(SUIT_CLUBS, 1)));
    CHECK(skat_script(&G, "gc #"));
    r = &P->list[0];
    CHECK(r->won && !r->hand && r->matadors == 11 && r->levels == 14 && r->value == 168 && r->points == 120);
    /* without 4 in grand (no jack), lost with 30 or less: schneider against the declarer */
    static const char *const weak[3] = {"7c 8c 9c 7s 8s 9s 7h 8h 9h 7d", "Jc Js Jh Jd Ac Tc Kc Qc As Ts",
                                        "Ks Qs Ah Th Kh Qh Ad Td Kd Qd"};
    set_deal(weak, "8d 9d", 0, 18);
    CHECK(skat_script(&G, "gg #"));
    r = &P->list[0];
    CHECK(!r->won && r->matadors == -4 && r->points == 0 && r->schneider && r->schwarz && r->hand);
    CHECK(r->levels == 4 + 1 + 1 + 1 + 1 && r->value == 24 * 8 && r->score == -2 * 24 * 8);
    CHECK(skat_total(P, 0) == -384 - 50 && skat_total(P, 1) == 40 && skat_total(P, 2) == 40 && P->losses[0] == 1);
}

static void test_overbid_and_null(void)
{
    /* a clubs game with 1 (the club jack alone), picked up: 2 levels, 24; a bid of 48: overbid, lost at 48 */
    static const char *const h[3] = {"Jc Ac Tc Kc Qc 9c 8c 7c As Ah", "Js Jh Ts Ks Qs 9s 8s 7s Th Kh",
                                     "Jd Qh 9h 8h 7h Ad Td Kd Qd 9d"};
    set_deal(h, "8d 7d", 0, 48);
    CHECK(skat_script(&G, "s d8d d7d gc #"));
    skdeal *r = &P->list[0];
    CHECK(r->matadors == 1 && r->overbid && !r->won && r->value == 48 && r->score == -96 && skat_total(P, 0) == -146);
    /* null: worth less than the bid: not allowed; the declarer takes a trick: lost at once */
    static const char *const n[3] = {"7c 8c 9c 7s 8s 9s 7h 8h 9h Ad", "Jc Js Jh Jd Ac Tc Kc Qc As Ts",
                                     "Ks Qs Ah Th Kh Qh 7d Td Kd Qd"};
    set_deal(n, "8d 9d", 0, 36);               /* null hand 35: under the bid; null hand ouvert 59 */
    CHECK(!skat_legal(P, SKA_GAME(SG_NULL, 0)) && skat_legal(P, SKA_GAME(SG_NULL, SF_OUVERT)));
    CHECK(skat_script(&G, "s d8d d9d") && !skat_legal(P, SKA_GAME(SG_NULL, 0)) &&
          skat_legal(P, SKA_GAME(SG_NULL, SF_OUVERT)));   /* 23, 46 */
    set_deal(n, "8d 9d", 0, 24);
    CHECK(skat_script(&G, "gn") && P->hand_game && P->type == SG_NULL);   /* null hand: 35 */
    CHECK(!skat_script(&G, "Ad 7c") && skat_script(&G, "Jd 7d"));   /* the ace of diamonds takes the first trick */
    r = &P->list[0];
    CHECK(P->phase == SK_DEAL && P->tricks == 1 && !r->won && r->value == 35 && r->score == -70);
    /* null ouvert, the skat picked up, every trick lost: 46 */
    static const char *const safe[3] = {"7c 8c 9c 7s 8s 9s 7h 8h 7d 8d", "Jc Js Jh Jd Ac Tc Kc Qc As Ts",
                                        "Ks Qs Ah Th Kh Qh 9h Td Kd Qd"};
    set_deal(safe, "Ad 9d", 0, 23);
    CHECK(skat_script(&G, "s dAd d9d gnO") && P->flags == SF_OUVERT && !P->hand_game && skat_script(&G, "#"));
    r = &P->list[0];
    CHECK(P->tricks == 10 && r->won && r->value == 46 && r->score == 46 && skat_total(P, 0) == 96);
}

static void test_end_and_undo(void)
{
    skat_new(&G, SKAT_SHORT, 77);
    CHECK(skat_script(&G, "!") && P->phase == SK_OVER && P->ndeals == 12 && !skat_legal(P, SA_NEXT));
    int sum = 0, w = skat_winners(P);
    for (int q = 0; q < 3; q++) sum += skat_total(P, q);
    CHECK(w != 0 && sum != 0x7fffffff);
    for (int q = 0; q < 3; q++)
        if (w >> q & 1)
            for (int o = 0; o < 3; o++) CHECK(skat_total(P, q) >= skat_total(P, o));
    skat_new(&G, SKAT_LONG, 77);
    CHECK(skat_script(&G, "!") && P->ndeals == 36);
    skat_new(&G, SKAT_SHORT, 9);
    CHECK(skat_script(&G, "# N . . . ."));
    int n = G.n;
    skpos before = G.pos;
    CHECK(skat_script(&G, ". . ."));
    CHECK(skat_undo(&G) && skat_undo(&G) && skat_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (skat_undo(&G)) {}
    skat_new(&H, SKAT_SHORT, 9);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
}

/* whole games: random legal actions of every kind */
static uint32_t pick(uint32_t *r)
{
    static uint32_t cand[600];
    int k = 0;
    const skpos *p = P;
    if (p->phase == SK_BID) {
        int next = skat_next_bid(p->bid);
        if (cards_rand(r) % 3 == 0) return SA_PASS;
        for (int v = next; v && v <= next + 30; v = skat_next_bid(v)) cand[k++] = SA_SAY | (uint32_t)v;
        cand[k++] = SA_HOLD;
        cand[k++] = SA_PASS;
    } else {
        cand[k++] = SA_PICKUP;
        cand[k++] = SA_NEXT;
        for (int i = 0; i < p->n[p->turn]; i++) {
            cand[k++] = SA_PLAY | p->hand[p->turn][i];
            cand[k++] = SA_PUT | p->hand[p->turn][i];
        }
        for (int i = 0; i < p->nskat && cards_rand(r) % 4 == 0; i++) cand[k++] = SA_TAKE | p->skat[i];
        for (int t = 0; t < SG_TYPES; t++)
            for (int f = 0; f < 8; f++) cand[k++] = SKA_GAME(t, f);
    }
    int m = 0;
    for (int i = 0; i < k; i++)
        if (skat_legal(p, cand[i])) cand[m++] = cand[i];
    return m ? cand[cards_rand(r) % (uint32_t)m] : 0;
}

static void test_random_games(void)
{
    int games = 0, deals = 0, won = 0, thrown = 0, nulls = 0, hands = 0, overbids = 0, grands = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        skat_new(&G, seed % 5 ? SKAT_SHORT : SKAT_LONG, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0, plain = seed % 2;
        while (P->phase != SK_OVER && ok && steps++ < 100000) {
            int d = P->ndeals;
            uint32_t a = plain ? skat_auto(P) : pick(&r);
            ok &= a != 0 && skat_play(&G, a);
            if (P->ndeals == d) continue;
            const skdeal *x = &P->list[d];
            deals++;
            if (x->declarer == SKAT_NONE) { thrown++; continue; }
            won += x->won, hands += x->hand, overbids += x->overbid;
            nulls += x->type == SG_NULL, grands += x->type == SG_GRAND;
            ok &= x->value >= x->bid && x->score == (x->won ? x->value : -2 * x->value);
            if (x->type != SG_NULL) {
                int pts = 0;
                for (int q = 0; q < 3; q++) pts += P->points[q];
                ok &= pts + skat_points(P->skat[0]) + skat_points(P->skat[1]) == 120 && P->tricks == 10;
                ok &= x->value == x->base * x->levels || x->overbid;
                ok &= x->won == (!x->overbid && x->points >= 61 && (!(x->flags & SF_SCHNEIDER) || x->points >= 90) &&
                                 (!(x->flags & SF_SCHWARZ) || x->tricks == 10));
            } else
                ok &= x->won == (x->tricks == 0) && (x->won ? P->tricks == 10 : 1);
        }
        int sum = 0, sc = 0, losses = 0, wins = 0;
        for (int q = 0; q < 3; q++)
            sum += skat_total(P, q), sc += P->score[q], losses += P->losses[q], wins += P->wins[q];
        ok &= P->phase == SK_OVER && P->ndeals == skat_deals(G.variant) && sum == sc + 50 * wins + 30 * losses;
        ok &= skat_winners(P) != 0;
        CHECK(ok);
        if (!ok) {
            printf("  seed %u: phase %d, %d deals, %d actions\n", seed, P->phase, P->ndeals, G.n);
            break;
        }
        games++;
    }
    CHECK(games == 300);
    printf("  %d random games, %d deals: %d thrown in, %d won, %d hand, %d null, %d grand, %d overbid\n", games, deals,
           thrown, won, hands, nulls, grands, overbids);
}

int main(void)
{
    test_bids();
    test_deal();
    test_bidding();
    test_cards();
    test_games();
    test_overbid_and_null();
    test_end_and_undo();
    test_random_games();
    printf("skat: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
