/*
 * Durak: the rules' test (the deal and the trump, who attacks first, beating, the limits of a bout, throwing in,
 * taking, beating off and the draw's order, the transfer variant, going out and the durak, undo, the notation, whole
 * random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/durak.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static dgame G, H;
static dpos *P = &G.pos;

static uint32_t rng = 777;
static int rnd(int n)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

/* every card once: hands, talon, bout; the beaten-off ones counted */
static int all_cards(const dpos *p)
{
    int seen[256] = {0}, n = p->discards;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++, n++;
    for (int i = 0; i < p->ntalon; i++) seen[p->talon[i]]++, n++;
    for (int i = 0; i < p->natt; i++) {
        seen[p->att[i]]++, n++;
        if (p->def[i] != DUR_NONE) seen[p->def[i]]++, n++;
    }
    for (int c = 0; c < 256; c++)
        if (seen[c] > 1) return 0;
    for (int i = 0; i < DUR_CARDS; i++)
        if (seen[dur_card(i)] > 1) return 0;
    return n == DUR_CARDS;
}

static void hand(int pl, const int *cards, int n)
{
    P->n[pl] = (uint8_t)n;
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cards[i];
}

static uint64_t m1(int c) { return DA_PLAY | 1ull << dur_index(c); }
static uint64_t m2(int a, int b) { return DA_PLAY | 1ull << dur_index(a) | 1ull << dur_index(b); }

#define S SUIT_SPADES
#define HT SUIT_HEARTS
#define D SUIT_DIAMONDS
#define C SUIT_CLUBS

static void test_deal(void)
{
    for (int np = 2; np <= 6; np++) {
        dur_new(&G, DUR_CLASSIC, np, 42 + np);
        CHECK(all_cards(P) && P->ntalon == 36 - 6 * np && P->phase == DP_ATTACK && P->fool == DUR_NONE);
        for (int q = 0; q < np; q++) CHECK(P->n[q] == 6);
        CHECK(P->trump == CARD_SUIT(P->trump_card));
        if (np < 6) CHECK(P->talon[0] == P->trump_card);
        int low = 99, who = -1;
        for (int q = 0; q < np; q++)
            for (int i = 0; i < 6; i++)
                if (CARD_SUIT(P->hand[q][i]) == P->trump && dur_strength(P->hand[q][i]) < low)
                    low = dur_strength(P->hand[q][i]), who = q;
        CHECK(who < 0 || (P->attacker == who && P->turn == who));
        CHECK(P->defender == (P->attacker + 1) % np);
    }
    for (int i = 0; i < 36; i++) CHECK(dur_index(dur_card(i)) == i && CARD_RANK(dur_card(i)) != 2);
    dur_new(&H, DUR_CLASSIC, 4, 9);
    dur_new(&G, DUR_CLASSIC, 4, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

/* a bare table: three players, trumps hearts, 0 attacks 1, an empty talon unless given */
static void table(int np, int variant)
{
    dur_new(&G, variant, np, 1);
    P->trump = HT;
    P->ntalon = 0;
    P->attacker = 0, P->defender = 1, P->turn = 0, P->phase = DP_ATTACK;
    P->discards = (uint8_t)(36);                /* the rest counted as gone: the hands below make up the total */
}

static void fix_total(void)
{
    int n = 0;
    for (int q = 0; q < P->players; q++) n += P->n[q];
    P->discards = (uint8_t)(36 - n - P->ntalon);
    P->start_n = P->n[P->defender];
}

static void test_beats(void)
{
    table(2, DUR_CLASSIC);
    CHECK(dur_beats(P, CARD(S, 9), CARD(S, 7)) && !dur_beats(P, CARD(S, 7), CARD(S, 9)));
    CHECK(dur_beats(P, CARD(S, RANK_ACE), CARD(S, RANK_KING)) && !dur_beats(P, CARD(S, 7), CARD(S, 7)));
    CHECK(dur_beats(P, CARD(HT, 6), CARD(S, RANK_ACE)) && !dur_beats(P, CARD(S, RANK_ACE), CARD(HT, 6)));
    CHECK(dur_beats(P, CARD(HT, 8), CARD(HT, 7)) && !dur_beats(P, CARD(HT, 7), CARD(HT, 8)));
    CHECK(!dur_beats(P, CARD(D, RANK_ACE), CARD(C, 6)));
}

static void test_bout(void)
{
    table(3, DUR_CLASSIC);
    int h0[] = {CARD(S, 7), CARD(D, 7), CARD(C, 9), CARD(S, 10), CARD(D, 6)};
    int h1[] = {CARD(S, 9), CARD(D, RANK_KING), CARD(HT, 6), CARD(C, 6)};
    int h2[] = {CARD(C, 7), CARD(S, RANK_QUEEN), CARD(D, 9), CARD(C, RANK_JACK), CARD(HT, 7), CARD(S, 6)};
    hand(0, h0, 5), hand(1, h1, 4), hand(2, h2, 6);
    fix_total();
    CHECK(all_cards(P));
    CHECK(!dur_legal(P, DA_PASS) && !dur_legal(P, m2(CARD(S, 7), CARD(C, 9))));   /* one rank to lead */
    CHECK(!dur_legal(P, m1(CARD(C, 7))));       /* not theirs */
    CHECK(dur_play(&G, m2(CARD(S, 7), CARD(D, 7))));
    CHECK(P->phase == DP_DEFEND && P->turn == 1 && P->natt == 2 && dur_open(P) == 2 && dur_room(P) == 2);
    CHECK(!dur_legal(P, DA_BEATS(CARD(C, 6), 0)) && !dur_legal(P, DA_BEATS(CARD(S, 9), 1)));
    CHECK(dur_play(&G, DA_BEATS(CARD(S, 9), 0)) && P->turn == 1 && P->phase == DP_DEFEND);
    CHECK(dur_play(&G, DA_BEATS(CARD(D, RANK_KING), 1)));
    /* all beaten: the throwers' round, the attacker first */
    CHECK(P->phase == DP_THROW && P->turn == 0 && !P->passed);
    CHECK(dur_can_throw(P, CARD(C, 9)) && dur_can_throw(P, CARD(S, 7)) && !dur_can_throw(P, CARD(S, 10)));
    CHECK(!dur_legal(P, m1(CARD(S, 10))) && !dur_legal(P, m2(CARD(C, 9), CARD(D, 6))));
    CHECK(dur_play(&G, DA_PASS) && P->turn == 2 && P->passed == 1);
    CHECK(dur_room(P) == 2 && !dur_legal(P, DA_PLAY | 1ull << dur_index(CARD(C, 7)) | 1ull << dur_index(CARD(D, 9)) |
                                                     1ull << dur_index(CARD(HT, 7))));   /* three: no room */
    CHECK(dur_play(&G, m1(CARD(C, 7))));        /* 2 throws in a 7 */
    CHECK(P->phase == DP_DEFEND && P->turn == 1 && dur_room(P) == 1);
    CHECK(dur_play(&G, DA_TAKE) && P->taking);
    CHECK(P->phase == DP_THROW && P->turn == 0);   /* each thrower adds once more: to the limit */
    CHECK(!dur_legal(P, m2(CARD(D, 6), CARD(C, 9))));
    CHECK(dur_play(&G, m1(CARD(C, 9))));        /* the limit: 4 attacking cards, the defender had 4 */
    CHECK(P->phase == DP_ATTACK && P->bouts == 1 && P->last_end == 2 && P->last_by == 1);
    CHECK(P->n[1] == 2 + 6 && P->natt == 0 && all_cards(P));
    CHECK(P->attacker == 2 && P->defender == 0 && P->turn == 2);   /* the taker loses the attack */
}

static void test_bito_and_draw(void)
{
    table(3, DUR_CLASSIC);
    int h0[] = {CARD(S, 7), CARD(D, 8)};
    int h1[] = {CARD(S, 9), CARD(D, RANK_KING), CARD(HT, 6)};
    int h2[] = {CARD(C, 7), CARD(S, RANK_QUEEN)};
    hand(0, h0, 2), hand(1, h1, 3), hand(2, h2, 2);
    int tal[] = {CARD(HT, RANK_ACE), CARD(C, 10), CARD(C, RANK_KING), CARD(C, 8), CARD(D, 10), CARD(D, 9),
                 CARD(S, 8), CARD(S, RANK_KING), CARD(C, RANK_QUEEN)};
    P->ntalon = 9;
    for (int i = 0; i < 9; i++) P->talon[i] = (uint8_t)tal[i];
    fix_total();
    CHECK(all_cards(P));
    CHECK(dur_play(&G, m1(CARD(S, 7))) && dur_play(&G, DA_BEATS(CARD(S, 9), 0)));
    CHECK(dur_play(&G, DA_PASS) && dur_play(&G, DA_PASS));   /* 0, 2 pass: beaten off */
    CHECK(P->phase == DP_ATTACK && P->last_end == 1 && P->bouts == 1);
    /* the draw: the attacker first (5 from the top), then 2 (4: the talon then has 0), the defender last */
    CHECK(P->n[0] == 6 && P->n[2] == 6 && P->n[1] == 2 && P->ntalon == 0 && all_cards(P));
    CHECK(P->hand[0][1] == CARD(C, RANK_QUEEN) && P->hand[2][P->n[2] - 1] == CARD(HT, RANK_ACE));
    CHECK(P->attacker == 1 && P->defender == 2);   /* the defender attacks next */
}

static void test_limit(void)
{
    table(2, DUR_CLASSIC);
    int h0[] = {CARD(S, 7), CARD(D, 7), CARD(C, 7), CARD(HT, 7), CARD(S, 8), CARD(D, 8), CARD(C, 8)};
    int h1[] = {CARD(S, 9), CARD(D, 9), CARD(C, 9), CARD(HT, 9), CARD(S, 10), CARD(D, 10), CARD(C, 10),
                CARD(HT, 10)};
    hand(0, h0, 7), hand(1, h1, 8);
    fix_total();
    CHECK(dur_play(&G, DA_PLAY | 1ull << dur_index(CARD(S, 7)) | 1ull << dur_index(CARD(D, 7)) |
                       1ull << dur_index(CARD(C, 7)) | 1ull << dur_index(CARD(HT, 7))));
    CHECK(P->att[0] == CARD(S, 7) && P->att[1] == CARD(HT, 7));   /* the places in the order of the cards: s h d c */
    CHECK(dur_play(&G, DA_BEATS(CARD(S, 9), 0)) && dur_play(&G, DA_BEATS(CARD(D, 9), 2)));
    CHECK(dur_play(&G, DA_BEATS(CARD(C, 9), 3)) && dur_play(&G, DA_BEATS(CARD(HT, 9), 1)));
    CHECK(P->phase == DP_THROW && dur_room(P) == 2);
    CHECK(!dur_legal(P, DA_PLAY | 1ull << dur_index(CARD(S, 8)) | 1ull << dur_index(CARD(D, 8)) |
                            1ull << dur_index(CARD(C, 8))));   /* 8s are not on the table */
    CHECK(!dur_legal(P, m1(CARD(S, 8))));
    /* a trump beats a 7 of hearts? 9 of hearts did: hearts are trumps; the defender keeps the 10s */
    CHECK(dur_play(&G, DA_PASS) && P->phase == DP_ATTACK && P->last_end == 1);   /* 2 players: one thrower */
    CHECK(P->n[1] == 4 && P->attacker == 1);
}

static void test_transfer(void)
{
    table(3, DUR_TRANSFER);
    int h0[] = {CARD(S, 7), CARD(D, 9)};
    int h1[] = {CARD(D, 7), CARD(C, 7), CARD(S, 10)};
    int h2[] = {CARD(C, 10), CARD(S, RANK_QUEEN), CARD(HT, 8)};
    hand(0, h0, 2), hand(1, h1, 3), hand(2, h2, 3);
    fix_total();
    CHECK(dur_play(&G, m1(CARD(S, 7))));
    CHECK(dur_can_shift(P, CARD(D, 7)) && !dur_can_shift(P, CARD(S, 10)));
    CHECK(dur_play(&G, DA_SHIFT | CARD(D, 7)));
    CHECK(P->attacker == 1 && P->defender == 2 && P->turn == 2 && P->natt == 2 && P->phase == DP_DEFEND);
    CHECK(!dur_can_shift(P, CARD(C, 7)));       /* not the defender's */
    CHECK(dur_play(&G, DA_BEATS(CARD(S, RANK_QUEEN), 0)) && dur_play(&G, DA_BEATS(CARD(HT, 8), 1)));
    CHECK(P->phase == DP_THROW && P->turn == 1);   /* the new attacker throws in first */
    CHECK(dur_play(&G, m1(CARD(C, 7))) && P->turn == 2);
    CHECK(!dur_can_shift(P, CARD(C, 10)));      /* cards beaten already */
    CHECK(dur_play(&G, DA_TAKE));               /* three attacking cards: the defender held three, no room */
    CHECK(P->phase == DP_ATTACK && P->n[2] == 1 + 5 && all_cards(P) && P->attacker == 0);
    /* the classic game: no transfer */
    table(3, DUR_CLASSIC);
    hand(0, h0, 2), hand(1, h1, 3), hand(2, h2, 3);
    fix_total();
    CHECK(dur_play(&G, m1(CARD(S, 7))) && !dur_can_shift(P, CARD(D, 7)) && !dur_legal(P, DA_SHIFT | CARD(D, 7)));
    /* a transfer needs the next player to hold enough cards */
    table(3, DUR_TRANSFER);
    int g1[] = {CARD(D, 7), CARD(C, 7), CARD(HT, 7)}, g2[] = {CARD(C, 10)};
    int g0[] = {CARD(S, 7), CARD(D, 9)};
    hand(0, g0, 2), hand(1, g1, 3), hand(2, g2, 1);
    fix_total();
    CHECK(dur_play(&G, m1(CARD(S, 7))) && !dur_can_shift(P, CARD(D, 7)));
}

static void test_end(void)
{
    table(3, DUR_CLASSIC);                      /* the talon empty: 0 leads their last card, 1 beats it */
    int h0[] = {CARD(S, 7)};
    int h1[] = {CARD(S, 9), CARD(D, 6)};
    int h2[] = {CARD(C, 10), CARD(C, 6)};
    hand(0, h0, 1), hand(1, h1, 2), hand(2, h2, 2);
    fix_total();
    CHECK(dur_play(&G, m1(CARD(S, 7))) && dur_play(&G, DA_BEATS(CARD(S, 9), 0)));
    CHECK(P->phase == DP_THROW && P->turn == 2);   /* 0 holds nothing: 2 throws in or passes */
    CHECK(dur_play(&G, DA_PASS));
    CHECK(P->nout == 1 && P->out[0] == 0 && !dur_active(P, 0) && P->phase == DP_ATTACK);
    CHECK(P->attacker == 1 && P->defender == 2);
    CHECK(dur_play(&G, m1(CARD(D, 6))) && dur_play(&G, DA_TAKE));
    CHECK(P->phase == DP_OVER && P->fool == 2 && dur_winners(P) == 3);   /* 1 is out; 2 holds cards: the durak */
    table(2, DUR_CLASSIC);                      /* both out at once: a draw */
    int k0[] = {CARD(S, 7)}, k1[] = {CARD(S, 9)};
    hand(0, k0, 1), hand(1, k1, 1);
    fix_total();
    CHECK(dur_play(&G, m1(CARD(S, 7))) && dur_play(&G, DA_BEATS(CARD(S, 9), 0)));
    CHECK(P->phase == DP_OVER && P->fool == DUR_NONE && dur_winners(P) == 3);
}

static void test_undo_script(void)
{
    dur_new(&G, DUR_CLASSIC, 3, 5);
    CHECK(dur_script(&G, ". . ."));
    dpos before = G.pos;
    CHECK(dur_play(&G, dur_auto(P)));
    CHECK(dur_undo(&G) && !memcmp(&before, &G.pos, sizeof before) && G.n == 3);
    dur_new(&G, DUR_CLASSIC, 3, 5);
    CHECK(!dur_undo(&G) && !dur_script(&G, "z") && !dur_script(&G, "p") && !dur_script(&G, "t"));
    CHECK(!dur_script(&G, "2s"));
    table(3, DUR_TRANSFER);
    int h0[] = {CARD(S, 7), CARD(D, 7)}, h1[] = {CARD(S, 9), CARD(D, RANK_KING), CARD(C, 7)},
        h2[] = {CARD(C, 6), CARD(C, 9), CARD(HT, 6)};
    hand(0, h0, 2), hand(1, h1, 3), hand(2, h2, 3);
    fix_total();
    H = G;
    CHECK(dur_script(&G, "7s7d Kd/7d 9s"));
    CHECK(P->phase == DP_THROW && P->def[0] == CARD(S, 9) && P->def[1] == CARD(D, RANK_KING));
    G = H;
    CHECK(dur_script(&G, "7s x7c") && P->defender == 2 && P->natt == 2);
    CHECK(dur_script(&G, "t p p") && P->phase == DP_ATTACK && P->n[2] == 5);
    dur_new(&G, DUR_TRANSFER, 4, 77);
    CHECK(dur_script(&G, "#") && P->phase == DP_OVER);
}

/* whole games, random legal actions: every card once, the limits, the draw back to six, the end and the durak */
static int bad_line;
#define GOOD(c) do { if (!(c) && !bad_line) bad_line = __LINE__; } while (0)

static void test_random_games(void)
{
    int over = 0, games = 0, draws = 0;
    for (int v = 0; v < DUR_VARIANTS; v++)
        for (int np = 2; np <= 6; np++)
            for (uint32_t seed = 1; seed <= 150; seed++) {
                dur_new(&G, v, np, seed);
                rng = seed * 31u + (uint32_t)np + 1000u * (uint32_t)v;
                int steps = 0;
                bad_line = 0;
                games++;
                while (P->phase != DP_OVER && steps++ < 20000 && !bad_line) {
                    uint64_t acts[512];
                    int n = 0, pl = P->turn;
                    GOOD(dur_active(P, pl));
                    if (P->phase == DP_ATTACK || P->phase == DP_THROW) {
                        if (P->phase == DP_THROW) acts[n++] = DA_PASS;
                        GOOD(P->phase == DP_THROW || P->n[pl] > 0);
                        for (int i = 0; i < P->n[pl]; i++) {
                            uint64_t a = DA_PLAY | 1ull << dur_index(P->hand[pl][i]);
                            if (dur_legal(P, a)) acts[n++] = a;
                            for (int j = i + 1; j < P->n[pl]; j++) {
                                uint64_t b = a | 1ull << dur_index(P->hand[pl][j]);
                                if (dur_legal(P, b)) acts[n++] = b;
                            }
                        }
                    } else if (P->phase == DP_DEFEND) {
                        if (dur_legal(P, DA_TAKE)) acts[n++] = DA_TAKE;
                        int beats = 0;
                        for (int i = 0; i < P->n[pl]; i++) {
                            for (int s = 0; s < P->natt; s++)
                                if (dur_legal(P, DA_BEATS(P->hand[pl][i], s))) acts[n++] = DA_BEATS(P->hand[pl][i], s),
                                                                               beats++;
                            if (dur_legal(P, DA_SHIFT | P->hand[pl][i])) acts[n++] = DA_SHIFT | P->hand[pl][i];
                        }
                        if (beats && rnd(4) && acts[0] == DA_TAKE) acts[0] = acts[--n];   /* mostly beat */
                    }
                    GOOD(n > 0);
                    if (!n) break;
                    uint64_t a = acts[rnd(n)];
                    if (a == DA_PASS && n > 1 && rnd(3)) a = acts[1 + rnd(n - 1)];
                    int before_bouts = P->bouts;
                    GOOD(dur_play(&G, a));
                    GOOD(all_cards(P));
                    GOOD(P->natt <= DUR_FULL && P->natt <= P->start_n);
                    if (P->bouts != before_bouts && P->phase != DP_OVER) {
                        GOOD(P->natt == 0 && !P->taking && P->turn == P->attacker && P->attacker != P->defender);
                        for (int q = 0; q < np; q++)
                            GOOD(!dur_active(P, q) || P->n[q] >= DUR_FULL || P->ntalon == 0);
                        GOOD(P->ntalon > 0 || P->n[P->attacker] > 0);
                    }
                }
                CHECK(!bad_line);
                if (bad_line) printf("   variant %d, %d players, seed %u: line %d, step %d\n", v, np, seed, bad_line, steps);
                if (P->phase != DP_OVER) continue;
                over++;
                int holding = 0;
                for (int q = 0; q < np; q++) holding += P->n[q] > 0;
                CHECK(P->ntalon == 0 && P->nout + (P->fool != DUR_NONE) == np);
                CHECK(holding == (P->fool != DUR_NONE));
                if (P->fool == DUR_NONE) {
                    draws++;
                    CHECK(dur_winners(P) == (1u << np) - 1);
                } else CHECK(dur_winners(P) == (((1u << np) - 1) & ~(1u << P->fool)) && P->n[P->fool] > 0);
            }
    CHECK(over > games * 9 / 10);
    printf("durak: %d of %d random games over, %d draws\n", over, games, draws);
}

int main(void)
{
    test_deal();
    test_beats();
    test_bout();
    test_bito_and_draw();
    test_limit();
    test_transfer();
    test_end();
    test_undo_script();
    test_random_games();
    printf("durak: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
