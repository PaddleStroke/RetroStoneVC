/*
 * Tiến lên: the rules' test (the order of the cards, the combinations, sequences without 2s, the bombs on the 2s,
 * the deal for 2 to 4 and the lowest card first, a pass that leaves the trick, the end of a trick, going out and the
 * places, undo, the notation, whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "games/tienlen.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static const shed_rules *R = &tln_rules;
static shed_game G, H;
static shed_pos *P = &G.pos;

static uint32_t rng = 99;
static int rnd(int n)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

#define S SUIT_SPADES
#define HT SUIT_HEARTS
#define D SUIT_DIAMONDS
#define C SUIT_CLUBS

static uint64_t cards(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    uint64_t m = 0;
    for (int i = 0; i < n; i++) m |= shed_bit(va_arg(ap, int));
    va_end(ap);
    return m;
}

static int combo(shed_combo *c, uint64_t m) { return shed_combo_of(R, m, c); }

static void hand(int pl, const int *cs, int n)
{
    P->n[pl] = (uint8_t)n;
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cs[i];
    shed_sort(R, P->hand[pl], n);
}

static void test_order_combos(void)
{
    CHECK(tln_power(CARD(S, 3)) == 0 && tln_power(CARD(C, 3)) == 1 && tln_power(CARD(D, 3)) == 2);
    CHECK(tln_power(CARD(HT, 3)) == 3 && tln_power(CARD(HT, 2)) == 51 && tln_power(CARD(S, 2)) == 48);
    shed_combo a, b;
    CHECK(combo(&a, cards(1, CARD(D, 7))) && a.kind == TLN_SINGLE);
    CHECK(combo(&a, cards(4, CARD(D, 7), CARD(S, 7), CARD(C, 7), CARD(HT, 7))) && a.kind == TLN_QUAD);
    CHECK(combo(&a, cards(3, CARD(D, 7), CARD(S, 8), CARD(C, 9))) && a.kind == TLN_SEQ);
    CHECK(combo(&a, cards(3, CARD(D, 12), CARD(S, 13), CARD(C, 1))) && a.kind == TLN_SEQ);   /* Q K A */
    CHECK(!combo(&a, cards(3, CARD(D, 13), CARD(S, 1), CARD(C, 2))));   /* no 2 in a sequence */
    CHECK(!combo(&a, cards(3, CARD(D, 1), CARD(S, 2), CARD(C, 3))));
    CHECK(!combo(&a, cards(2, CARD(D, 7), CARD(S, 8))) && !combo(&a, cards(3, CARD(D, 7), CARD(S, 8), CARD(C, 10))));
    CHECK(combo(&a, cards(12, CARD(S, 3), CARD(S, 4), CARD(S, 5), CARD(S, 6), CARD(S, 7), CARD(S, 8), CARD(S, 9),
                          CARD(S, 10), CARD(S, 11), CARD(S, 12), CARD(S, 13), CARD(S, 1))) && a.kind == TLN_SEQ);
    CHECK(combo(&a, cards(6, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4), CARD(S, 5), CARD(D, 5))) &&
          a.kind == TLN_DSEQ && tln_bomb(&a) == 1);
    CHECK(!combo(&a, cards(6, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4), CARD(S, 6), CARD(D, 6))));
    CHECK(!combo(&a, cards(4, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4))));   /* two pairs: no */
    CHECK(!combo(&a, cards(6, CARD(D, 13), CARD(S, 13), CARD(C, 1), CARD(HT, 1), CARD(S, 2), CARD(D, 2))));
    /* what beats what */
    combo(&a, cards(1, CARD(HT, 9))), combo(&b, cards(1, CARD(D, 9)));
    CHECK(tln_beats(&a, &b) && !tln_beats(&b, &a));   /* hearts over diamonds */
    combo(&a, cards(3, CARD(S, 8), CARD(D, 9), CARD(S, 10))), combo(&b, cards(3, CARD(HT, 7), CARD(HT, 8), CARD(HT, 9)));
    CHECK(tln_beats(&a, &b) && !tln_beats(&b, &a));
    combo(&b, cards(4, CARD(S, 7), CARD(S, 8), CARD(S, 9), CARD(S, 10)));
    CHECK(!tln_beats(&b, &a) && !tln_beats(&a, &b));   /* not the same length */
    shed_combo two, twos, three2s, d3, d4, quad, quad2, d5;
    combo(&two, cards(1, CARD(S, 2)));
    combo(&twos, cards(2, CARD(S, 2), CARD(C, 2)));
    combo(&three2s, cards(3, CARD(S, 2), CARD(C, 2), CARD(D, 2)));
    combo(&d3, cards(6, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4), CARD(S, 5), CARD(D, 5)));
    combo(&d4, cards(8, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4), CARD(S, 5), CARD(D, 5), CARD(S, 6), CARD(D, 6)));
    combo(&d5, cards(10, CARD(D, 3), CARD(S, 3), CARD(C, 4), CARD(HT, 4), CARD(S, 5), CARD(D, 5), CARD(S, 6), CARD(D, 6),
                     CARD(S, 7), CARD(D, 7)));
    combo(&quad, cards(4, CARD(D, 5), CARD(S, 5), CARD(C, 5), CARD(HT, 5)));
    combo(&quad2, cards(4, CARD(D, 9), CARD(S, 9), CARD(C, 9), CARD(HT, 9)));
    CHECK(tln_beats(&d3, &two) && tln_beats(&quad, &two) && tln_beats(&d4, &two));
    CHECK(!tln_beats(&d3, &twos) && !tln_beats(&quad, &twos) && tln_beats(&d4, &twos));
    CHECK(!tln_beats(&d4, &three2s) && tln_beats(&d5, &three2s));
    CHECK(tln_beats(&quad, &d3) && !tln_beats(&d3, &quad) && tln_beats(&d4, &quad) && tln_beats(&quad2, &quad));
    combo(&a, cards(1, CARD(HT, 1)));
    CHECK(!tln_beats(&quad, &a) && !tln_beats(&d3, &a));   /* bombs only on 2s and bombs */
    combo(&a, cards(1, CARD(HT, 2)));
    CHECK(tln_beats(&a, &two) && !tln_beats(&two, &a));
}

static void test_deal(void)
{
    for (int np = 2; np <= 4; np++) {
        shed_new(&G, R, np, 3 + np);
        int seen[256] = {0}, low = 99, who = -1;
        for (int q = 0; q < np; q++) {
            CHECK(P->n[q] == 13);
            for (int i = 0; i < 13; i++) {
                seen[P->hand[q][i]]++;
                if (tln_power(P->hand[q][i]) < low) low = tln_power(P->hand[q][i]), who = q;
            }
        }
        for (int c = 0; c < 256; c++) CHECK(seen[c] <= 1);
        CHECK(P->turn == who && tln_power(P->must) == low);
        if (np == 4) CHECK(P->must == CARD(S, 3));
    }
}

static void test_play(void)
{
    shed_new(&G, R, 4, 1);
    int h0[] = {CARD(S, 3), CARD(C, 4), CARD(D, 5), CARD(HT, 9), CARD(S, 9)};
    int h1[] = {CARD(HT, 6), CARD(D, 7), CARD(S, 8), CARD(S, 12), CARD(S, 2)};
    int h2[] = {CARD(D, 10), CARD(C, 11), CARD(HT, 12), CARD(C, 3)};
    int h3[] = {CARD(D, 4), CARD(D, 6), CARD(HT, 7), CARD(C, 8), CARD(S, 13), CARD(C, 13), CARD(D, 13), CARD(HT, 13)};
    hand(0, h0, 5), hand(1, h1, 5), hand(2, h2, 4), hand(3, h3, 8);
    P->turn = 0, P->must = CARD(S, 3);
    CHECK(!shed_legal(R, P, cards(1, CARD(C, 4))) && shed_legal(R, P, cards(3, CARD(S, 3), CARD(C, 4), CARD(D, 5))));
    CHECK(shed_play(&G, R, cards(3, CARD(S, 3), CARD(C, 4), CARD(D, 5))));   /* 3 4 5 */
    CHECK(P->turn == 1 && shed_play(&G, R, cards(3, CARD(HT, 6), CARD(D, 7), CARD(S, 8))));
    CHECK(shed_play(&G, R, SHED_PASS) && P->passed == 4);   /* 2 passes: out of the trick */
    CHECK(P->turn == 3 && shed_play(&G, R, cards(3, CARD(D, 6), CARD(HT, 7), CARD(C, 8))));   /* the 8 of clubs > spades */
    CHECK(P->turn == 0 && shed_play(&G, R, SHED_PASS));
    CHECK(P->turn == 1);                        /* 2 is skipped: they passed */
    CHECK(shed_play(&G, R, SHED_PASS));         /* 1 passes: nobody left but 3, who leads */
    CHECK(P->turn == 3 && P->top.n == 0 && P->passed == 0 && P->tricks == 1);
    CHECK(shed_play(&G, R, cards(1, CARD(D, 4))) && shed_play(&G, R, cards(1, CARD(HT, 9))));   /* 3, then 0 */
    CHECK(shed_play(&G, R, cards(1, CARD(S, 2))));   /* 1: a 2 */
    CHECK(P->turn == 2 && shed_play(&G, R, SHED_PASS));
    CHECK(P->turn == 3 && shed_play(&G, R, cards(4, CARD(S, 13), CARD(C, 13), CARD(D, 13), CARD(HT, 13))));   /* chopped */
    CHECK(P->turn == 0 && shed_play(&G, R, SHED_PASS));
    CHECK(P->turn == 1 && shed_play(&G, R, SHED_PASS));   /* 2 passed already: the trick is over */
    /* 3 went out with the chop, first: the lead goes on to the next one holding cards */
    CHECK(P->turn == 0 && P->top.n == 0 && P->n[3] == 0 && shed_place(P, 3) == 0);
}

static void test_out_and_places(void)
{
    shed_new(&G, R, 3, 1);
    int h0[] = {CARD(S, 3), CARD(C, 9)}, h1[] = {CARD(HT, 4)}, h2[] = {CARD(D, 5), CARD(HT, 5), CARD(S, 2)};
    hand(0, h0, 2), hand(1, h1, 1), hand(2, h2, 3);
    P->turn = 0, P->must = CARD(S, 3);
    CHECK(shed_play(&G, R, cards(1, CARD(S, 3))) && shed_play(&G, R, cards(1, CARD(HT, 4))));   /* 1 is out first */
    CHECK(shed_place(P, 1) == 0 && P->phase == SP_PLAY && P->turn == 2);
    CHECK(shed_play(&G, R, SHED_PASS) && P->turn == 0 && shed_play(&G, R, SHED_PASS));
    CHECK(P->top.n == 0 && P->turn == 2);       /* 1 is out: the next one holding cards leads */
    CHECK(shed_play(&G, R, cards(2, CARD(D, 5), CARD(HT, 5))) && P->turn == 0 && shed_play(&G, R, SHED_PASS));
    CHECK(P->turn == 2 && shed_play(&G, R, cards(1, CARD(S, 2))));   /* 2 out second: 0 is last */
    CHECK(P->phase == SP_OVER && P->nout == 3 && P->out[0] == 1 && P->out[1] == 2 && P->out[2] == 0);
}

static void test_undo_script(void)
{
    shed_new(&G, R, 4, 5);
    CHECK(shed_script(&G, R, ". . ."));
    shed_pos before = G.pos;
    CHECK(shed_play(&G, R, shed_auto(R, P)));
    CHECK(shed_undo(&G, R) && !memcmp(&before, &G.pos, sizeof before) && G.n == 3);
    shed_new(&G, R, 4, 5);
    CHECK(!shed_script(&G, R, "p") && shed_script(&G, R, "3s") && P->pile[0] == CARD(S, 3));
    shed_new(&H, R, 2, 8);
    CHECK(shed_script(&H, R, "#") && H.pos.phase == SP_OVER && H.pos.nout == 2);
}

static int bad_line;
#define GOOD(c) do { if (!(c) && !bad_line) bad_line = __LINE__; } while (0)

/* whole deals, random plays (singles, groups, and the first legal play of each size): every card once, a pass out
 * of the trick, the places */
static void test_random_games(void)
{
    for (int np = 2; np <= 4; np++)
        for (uint32_t seed = 1; seed <= 200; seed++) {
            shed_new(&G, R, np, seed);
            rng = seed * 31u + (uint32_t)np;
            int steps = 0;
            bad_line = 0;
            while (P->phase == SP_PLAY && steps++ < 5000 && !bad_line) {
                uint64_t acts[400];
                int n = 0, pl = P->turn, k = P->n[pl];
                GOOD(shed_in_trick(R, P, pl));
                if (shed_legal(R, P, SHED_PASS)) acts[n++] = SHED_PASS;
                for (int a = 0; a < k; a++) {
                    uint64_t ma = shed_bit(P->hand[pl][a]);
                    if (shed_legal(R, P, ma)) acts[n++] = ma;
                    for (int b = a + 1; b < k; b++)
                        if (shed_legal(R, P, ma | shed_bit(P->hand[pl][b]))) acts[n++] = ma | shed_bit(P->hand[pl][b]);
                }
                for (int size = 3; size <= k; size++) {
                    uint64_t m = shed_search(R, P, size);
                    if (m) acts[n++] = m;
                }
                GOOD(n > 0);
                if (!n) break;
                uint64_t a = acts[rnd(n)];
                if (a == SHED_PASS && n > 1 && rnd(2)) a = acts[1 + rnd(n - 1)];
                uint8_t locked = P->passed;
                int lead = !P->top.n;
                shed_combo top = P->top;
                GOOD(shed_play(&G, R, a));
                if (a != SHED_PASS && !lead) {
                    shed_combo c;
                    GOOD(shed_combo_of(R, a, &c) && tln_beats(&c, &top));
                }
                if (P->phase == SP_PLAY && P->top.n) GOOD(!(locked >> P->turn & 1));   /* a pass: out of it */
                int total = P->played + P->npile;
                for (int q = 0; q < np; q++) total += P->n[q];
                GOOD(total == 13 * np);
            }
            CHECK(!bad_line);
            if (bad_line) printf("   %d players, seed %u: line %d, step %d\n", np, seed, bad_line, steps);
            CHECK(P->phase == SP_OVER && P->nout == np);
            int holding = 0, seen = 0;
            for (int i = 0; i < np; i++) holding += P->n[P->out[i]] > 0, seen |= 1 << P->out[i];
            CHECK(holding == 1 && P->n[P->out[np - 1]] > 0 && seen == (1 << np) - 1);
        }
}

int main(void)
{
    test_order_combos();
    test_deal();
    test_play();
    test_out_and_places();
    test_undo_script();
    test_random_games();
    printf("tienlen: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
