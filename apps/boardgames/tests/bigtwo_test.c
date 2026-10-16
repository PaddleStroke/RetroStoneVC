/*
 * Big Two: the rules' test (the order of the cards, the combinations and what beats what, the deal for 4 and 3, the
 * 3 of diamonds first, passing and playing again, the end of a trick, the first out and the penalties, undo, the
 * notation, whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "games/bigtwo.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static const shed_rules *R = &bigt_rules;
static shed_game G, H;
static shed_pos *P = &G.pos;

static uint32_t rng = 4242;
static int rnd(int n)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

#define S SUIT_SPADES
#define HT SUIT_HEARTS
#define D SUIT_DIAMONDS
#define C SUIT_CLUBS

/* the combination of the cards given (any order) */
static int combo(shed_combo *c, int n, ...)
{
    va_list ap;
    va_start(ap, n);
    uint64_t m = 0;
    for (int i = 0; i < n; i++) m |= shed_bit(va_arg(ap, int));
    va_end(ap);
    return shed_combo_of(R, m, c);
}

static uint64_t mask(int n, const int *cards)
{
    uint64_t m = 0;
    for (int i = 0; i < n; i++) m |= shed_bit(cards[i]);
    return m;
}

static void hand(int pl, const int *cards, int n)
{
    P->n[pl] = (uint8_t)n;
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cards[i];
    shed_sort(R, P->hand[pl], n);
}

static void test_order(void)
{
    CHECK(bigt_power(CARD(D, 3)) == 0 && bigt_power(CARD(C, 3)) == 1 && bigt_power(CARD(HT, 3)) == 2);
    CHECK(bigt_power(CARD(S, 3)) == 3 && bigt_power(CARD(S, 2)) == 51 && bigt_power(CARD(D, 2)) == 48);
    CHECK(bigt_power(CARD(S, RANK_ACE)) < bigt_power(CARD(D, 2)) && bigt_power(CARD(S, RANK_KING)) < bigt_power(CARD(D, 1)));
    CHECK(bigt_penalty(9) == 9 && bigt_penalty(10) == 20 && bigt_penalty(13) == 26 && bigt_penalty(0) == 0);
}

static void test_combos(void)
{
    shed_combo a, b;
    CHECK(combo(&a, 1, CARD(S, 2)) && a.kind == BIGT_SINGLE);
    CHECK(combo(&a, 2, CARD(S, 9), CARD(D, 9)) && a.kind == BIGT_PAIR && !combo(&b, 2, CARD(S, 9), CARD(D, 10)));
    CHECK(combo(&a, 3, CARD(S, 9), CARD(D, 9), CARD(C, 9)) && a.kind == BIGT_TRIPLE);
    CHECK(!combo(&a, 4, CARD(S, 9), CARD(D, 9), CARD(C, 9), CARD(HT, 9)));   /* four alone: no */
    CHECK(!combo(&a, 4, CARD(S, 3), CARD(D, 4), CARD(C, 5), CARD(HT, 6)));
    CHECK(combo(&a, 5, CARD(S, 3), CARD(D, 4), CARD(C, 5), CARD(HT, 6), CARD(S, 7)) && a.kind == BIGT_FIVE &&
          a.key >> 24 == BIGT_STRAIGHT);
    CHECK(combo(&a, 5, CARD(S, 10), CARD(D, 11), CARD(C, 12), CARD(HT, 13), CARD(S, 1)) && a.key >> 24 == BIGT_STRAIGHT);
    CHECK(!combo(&a, 5, CARD(S, 11), CARD(D, 12), CARD(C, 13), CARD(HT, 1), CARD(S, 2)));   /* no 2 in a straight */
    CHECK(!combo(&a, 5, CARD(S, 1), CARD(D, 2), CARD(C, 3), CARD(HT, 4), CARD(S, 5)));
    CHECK(combo(&a, 5, CARD(HT, 3), CARD(HT, 9), CARD(HT, 5), CARD(HT, 12), CARD(HT, 7)) && a.key >> 24 == BIGT_FLUSH);
    CHECK(combo(&a, 5, CARD(HT, 3), CARD(S, 3), CARD(D, 3), CARD(HT, 12), CARD(C, 12)) && a.key >> 24 == BIGT_HOUSE);
    CHECK(combo(&a, 5, CARD(HT, 4), CARD(S, 3), CARD(D, 3), CARD(C, 3), CARD(HT, 3)) && a.key >> 24 == BIGT_FOUR);
    CHECK(combo(&a, 5, CARD(C, 8), CARD(C, 9), CARD(C, 10), CARD(C, 11), CARD(C, 12)) && a.key >> 24 == BIGT_SFLUSH);
    CHECK(!combo(&a, 5, CARD(C, 8), CARD(C, 9), CARD(D, 10), CARD(HT, 10), CARD(S, 12)));
    CHECK(!combo(&a, 6, CARD(C, 3), CARD(C, 4), CARD(C, 5), CARD(C, 6), CARD(C, 7), CARD(C, 8)));
    /* what beats what */
    combo(&a, 1, CARD(S, 9)), combo(&b, 1, CARD(HT, 9));
    CHECK(bigt_beats(&a, &b) && !bigt_beats(&b, &a));   /* spades over hearts */
    combo(&a, 1, CARD(D, 2)), combo(&b, 1, CARD(S, RANK_ACE));
    CHECK(bigt_beats(&a, &b));
    combo(&a, 2, CARD(S, 9), CARD(D, 9)), combo(&b, 2, CARD(HT, 9), CARD(C, 9));
    CHECK(bigt_beats(&a, &b) && !bigt_beats(&b, &a));   /* the pair holding the spade */
    combo(&b, 1, CARD(D, 3));
    CHECK(!bigt_beats(&a, &b));                 /* not the same kind */
    shed_combo st, fl, fh, fo, sf, st2, fl2;
    combo(&st, 5, CARD(S, 9), CARD(D, 10), CARD(C, 11), CARD(HT, 12), CARD(S, 13));
    combo(&st2, 5, CARD(D, 9), CARD(D, 10), CARD(C, 11), CARD(HT, 12), CARD(HT, 13));
    combo(&fl, 5, CARD(D, 3), CARD(D, 5), CARD(D, 7), CARD(D, 9), CARD(D, 11));
    combo(&fl2, 5, CARD(C, 3), CARD(C, 5), CARD(C, 7), CARD(C, 9), CARD(C, 11));
    combo(&fh, 5, CARD(D, 4), CARD(C, 4), CARD(S, 4), CARD(D, 6), CARD(C, 6));
    combo(&fo, 5, CARD(D, 4), CARD(C, 4), CARD(S, 4), CARD(HT, 4), CARD(C, 6));
    combo(&sf, 5, CARD(D, 3), CARD(D, 4), CARD(D, 5), CARD(D, 6), CARD(D, 7));
    CHECK(bigt_beats(&fl, &st) && bigt_beats(&fh, &fl) && bigt_beats(&fo, &fh) && bigt_beats(&sf, &fo));
    CHECK(!bigt_beats(&st, &fl) && bigt_beats(&st, &st2) && !bigt_beats(&st2, &st));   /* the king of spades */
    CHECK(bigt_beats(&fl2, &fl));               /* the same ranks: clubs over diamonds */
    combo(&a, 5, CARD(D, 3), CARD(D, 5), CARD(D, 7), CARD(D, 9), CARD(D, 12));
    CHECK(bigt_beats(&a, &fl2));                /* a queen high over a jack high */
    combo(&a, 5, CARD(D, 5), CARD(C, 5), CARD(S, 5), CARD(D, 3), CARD(C, 3));
    CHECK(bigt_beats(&a, &fh));                 /* fives full over fours full */
}

static void test_deal(void)
{
    for (int np = 3; np <= 4; np++) {
        shed_new(&G, R, np, 77 + np);
        int seen[256] = {0}, total = 0, holder = -1;
        for (int q = 0; q < np; q++) {
            total += P->n[q];
            for (int i = 0; i < P->n[q]; i++) {
                seen[P->hand[q][i]]++;
                if (P->hand[q][i] == CARD(D, 3)) holder = q;
                if (i) CHECK(bigt_power(P->hand[q][i - 1]) < bigt_power(P->hand[q][i]));
            }
        }
        CHECK(total == 52 && P->must == CARD(D, 3) && P->turn == holder && P->phase == SP_PLAY);
        for (int q = 0; q < np; q++) CHECK(P->n[q] == (np == 4 ? 13 : q == holder ? 18 : 17));
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 13; r++) CHECK(seen[CARD(s, r)] == 1);
    }
    /* three players: the 3 of diamonds may be the odd card itself; its holder still leads with it */
    int odd = 0;
    for (uint32_t seed = 1; seed <= 3000; seed++) {
        uint8_t pack[CARDS_MAX];
        int n = cards_pack(PACK_52, pack);
        cards_shuffle(pack, n, seed);
        shed_new(&G, R, 3, seed);
        int holder = -1;
        for (int q = 0; q < 3; q++)
            if (shed_find(P, q, CARD(D, 3)) >= 0) holder = q;
        odd += pack[n - 1] == CARD(D, 3);
        CHECK(P->must == CARD(D, 3) && P->turn == holder && P->n[holder] == 18);
    }
    CHECK(odd > 0);
    shed_new(&H, R, 4, 9);
    shed_new(&G, R, 4, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

static void test_play(void)
{
    shed_new(&G, R, 4, 1);
    int h0[] = {CARD(D, 3), CARD(C, 3), CARD(S, 5), CARD(D, 9), CARD(HT, 9), CARD(S, 2)};
    int h1[] = {CARD(S, 4), CARD(D, 10), CARD(C, 10), CARD(HT, 1)};
    int h2[] = {CARD(D, 4), CARD(S, 9), CARD(C, 13), CARD(D, 13), CARD(C, 1)};
    int h3[] = {CARD(HT, 4), CARD(HT, 2)};
    hand(0, h0, 6), hand(1, h1, 4), hand(2, h2, 5), hand(3, h3, 2);
    P->turn = 0, P->must = CARD(D, 3);
    CHECK(!shed_legal(R, P, SHED_PASS) && !shed_legal(R, P, shed_bit(CARD(S, 5))));   /* the 3 of diamonds first */
    CHECK(shed_legal(R, P, shed_bit(CARD(D, 3))) && shed_legal(R, P, mask(2, (int[]){CARD(D, 3), CARD(C, 3)})));
    CHECK(!shed_legal(R, P, mask(2, (int[]){CARD(D, 3), CARD(S, 5)})));
    CHECK(shed_play(&G, R, mask(2, (int[]){CARD(D, 3), CARD(C, 3)})));   /* a pair of 3s */
    CHECK(P->turn == 1 && P->top.kind == BIGT_PAIR && P->must == SHED_NONE);
    CHECK(!shed_legal(R, P, shed_bit(CARD(S, 4))));   /* a single on a pair: no */
    CHECK(shed_play(&G, R, SHED_PASS) && P->turn == 2 && P->passed == 2);
    CHECK(shed_play(&G, R, mask(2, (int[]){CARD(C, 13), CARD(D, 13)})) && P->passed == 0);
    CHECK(shed_play(&G, R, SHED_PASS) && shed_play(&G, R, SHED_PASS));   /* 3, 0 pass */
    CHECK(P->turn == 1 && shed_play(&G, R, mask(2, (int[]){CARD(D, 10), CARD(C, 10)})) == 0);   /* lower */
    CHECK(shed_play(&G, R, SHED_PASS));         /* 1 passes again: three in a row, 2 leads */
    CHECK(P->turn == 2 && P->top.n == 0 && P->tricks == 1 && P->played == 4);
    CHECK(shed_play(&G, R, shed_bit(CARD(D, 4))));   /* 2 leads a 4 */
    CHECK(shed_play(&G, R, shed_bit(CARD(HT, 4))));  /* 3: higher suit */
    CHECK(shed_play(&G, R, SHED_PASS) && P->turn == 1);
    CHECK(shed_play(&G, R, shed_bit(CARD(HT, 1))));  /* 1 had passed before: may play now */
    CHECK(shed_play(&G, R, SHED_PASS) && shed_play(&G, R, SHED_PASS) && P->turn == 0);
    CHECK(shed_play(&G, R, shed_bit(CARD(S, 2))));
    CHECK(shed_play(&G, R, SHED_PASS) && shed_play(&G, R, SHED_PASS) && shed_play(&G, R, SHED_PASS));
    CHECK(P->turn == 0 && P->top.n == 0);
    CHECK(shed_play(&G, R, shed_bit(CARD(S, 5))) && shed_play(&G, R, SHED_PASS));   /* 1 passes */
    CHECK(shed_play(&G, R, shed_bit(CARD(C, 1))));   /* 2: an ace */
    CHECK(shed_play(&G, R, shed_bit(CARD(HT, 2))));  /* 3: the 2 of hearts, their last card: out, the deal over */
    CHECK(P->phase == SP_OVER && P->out[0] == 3 && P->nout == 4);
    CHECK(P->n[0] == 2 && bigt_penalty(P->n[0]) == 2);
}

static void test_undo_script(void)
{
    shed_new(&G, R, 4, 5);
    CHECK(shed_script(&G, R, ". . ."));
    shed_pos before = G.pos;
    CHECK(shed_play(&G, R, shed_auto(R, P)));
    CHECK(shed_undo(&G, R) && !memcmp(&before, &G.pos, sizeof before) && G.n == 3);
    shed_new(&G, R, 4, 5);
    CHECK(!shed_undo(&G, R) && !shed_script(&G, R, "p") && !shed_script(&G, R, "z"));
    CHECK(shed_script(&G, R, "3d") && P->top.kind == BIGT_SINGLE && P->pile[0] == CARD(D, 3));
    shed_new(&G, R, 4, 5);
    CHECK(shed_script(&G, R, "#") && P->phase == SP_OVER && P->n[P->out[0]] == 0);
}

/* whole deals, random legal plays: every card once, the trick's rules, the first out ends it */
static int bad_line;
#define GOOD(c) do { if (!(c) && !bad_line) bad_line = __LINE__; } while (0)

static void test_random_games(void)
{
    for (int np = 3; np <= 4; np++)
        for (uint32_t seed = 1; seed <= 150; seed++) {
            shed_new(&G, R, np, seed);
            rng = seed * 31u + (uint32_t)np;
            int steps = 0;
            bad_line = 0;
            while (P->phase == SP_PLAY && steps++ < 5000 && !bad_line) {
                static uint64_t acts[20000];
                int n = 0, pl = P->turn, k = P->n[pl];
                GOOD(shed_active(P, pl));
                if (shed_legal(R, P, SHED_PASS)) acts[n++] = SHED_PASS;
                for (int a = 0; a < k; a++) {   /* singles, pairs, threes */
                    uint64_t ma = shed_bit(P->hand[pl][a]);
                    if (shed_legal(R, P, ma)) acts[n++] = ma;
                    for (int b = a + 1; b < k; b++) {
                        uint64_t mb = ma | shed_bit(P->hand[pl][b]);
                        if (shed_legal(R, P, mb)) acts[n++] = mb;
                        for (int c = b + 1; c < k; c++)
                            if (shed_legal(R, P, mb | shed_bit(P->hand[pl][c]))) acts[n++] = mb | shed_bit(P->hand[pl][c]);
                    }
                }
                if (!P->top.n || P->top.n == 5) {   /* the five-card hands */
                    int idx[5] = {0, 1, 2, 3, 4};
                    while (k >= 5) {
                        uint64_t m = 0;
                        for (int i = 0; i < 5; i++) m |= shed_bit(P->hand[pl][idx[i]]);
                        if (shed_legal(R, P, m)) acts[n++] = m;
                        int i = 4;
                        while (i >= 0 && idx[i] == k - 5 + i) i--;
                        if (i < 0) break;
                        idx[i]++;
                        for (int j = i + 1; j < 5; j++) idx[j] = idx[j - 1] + 1;
                    }
                }
                GOOD(n > 0);
                if (!n) break;
                uint64_t a = acts[rnd(n)];
                if (a == SHED_PASS && n > 1 && rnd(2)) a = acts[1 + rnd(n - 1)];
                shed_combo before = P->top;
                int lead = !P->top.n;
                GOOD(shed_play(&G, R, a));
                if (a != SHED_PASS && !lead) {
                    shed_combo c;
                    GOOD(shed_combo_of(R, a, &c) && c.kind == before.kind && c.key > before.key);
                }
                int total = P->played + P->npile;
                for (int q = 0; q < np; q++) total += P->n[q];
                GOOD(total == 52 && P->must == SHED_NONE);
            }
            CHECK(!bad_line);
            if (bad_line) printf("   %d players, seed %u: line %d, step %d\n", np, seed, bad_line, steps);
            CHECK(P->phase == SP_OVER && P->nout == np && P->n[P->out[0]] == 0);
            for (int i = 1; i < np; i++) CHECK(P->n[P->out[i]] > 0);
        }
}

int main(void)
{
    test_order();
    test_combos();
    test_deal();
    test_play();
    test_undo_script();
    test_random_games();
    printf("bigtwo: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
