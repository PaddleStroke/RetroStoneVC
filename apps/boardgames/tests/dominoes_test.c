/*
 * Dominoes: the rules' test (the bones, the deal, the lead, the ends, All Fives' count and spinner, passing, drawing,
 * the end of a round and its scoring, whole random games from seeds, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "cards.h"
#include "games/dominoes.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static dgame G, H;
static dpos *P = &G.pos;

static void test_bones(void)
{
    int seen[DOM_BONES] = {0}, ok = 1;
    for (int a = 0; a <= 6; a++)
        for (int b = a; b <= 6; b++) {
            int k = dom_bone(a, b);
            ok &= k >= 0 && k < DOM_BONES && dom_lo(k) == a && dom_hi(k) == b && dom_bone(b, a) == k;
            seen[k]++;
        }
    for (int k = 0; k < DOM_BONES; k++) ok &= seen[k] == 1;
    CHECK(ok);
    CHECK(dom_double(dom_bone(4, 4)) && !dom_double(dom_bone(3, 4)) && dom_pips(dom_bone(6, 5)) == 11);
}

static int count_all(const dpos *p)
{
    int seen[DOM_BONES] = {0}, n = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->nhand[q]; i++) seen[p->hand[q][i]]++, n++;
    for (int i = 0; i < p->nyard; i++) seen[p->yard[i]]++, n++;
    if (p->center != DOM_NONE) seen[p->center]++, n++;
    for (int e = 0; e < ENDS; e++)
        for (int i = 0; i < p->narm[e]; i++) seen[p->arm[e][i]]++, n++;
    for (int k = 0; k < DOM_BONES; k++)
        if (seen[k] != 1) return 0;
    return n == DOM_BONES;
}

static void test_deal(void)
{
    dom_new(&G, DOM_BLOCK, 2, 42);
    dom_new(&H, DOM_BLOCK, 2, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->nhand[0] == 7 && P->nhand[1] == 7 && P->nyard == 14 && count_all(P));
    dom_new(&G, DOM_DRAW, 4, 42);
    CHECK(P->nhand[0] == 5 && P->nhand[3] == 5 && P->nyard == 8 && count_all(P));
    dom_new(&G, DOM_FIVES, 3, 9);
    CHECK(P->nhand[2] == 5 && P->nyard == 13 && count_all(P));
    /* the first round: the holder of the highest double leads it */
    int best = -1, who = -1;
    for (int q = 0; q < 3; q++)
        for (int i = 0; i < 5; i++) {
            int b = P->hand[q][i];
            if (dom_double(b) && dom_lo(b) > best) best = dom_lo(b), who = q;
        }
    CHECK(who < 0 || (P->turn == who && P->lead == dom_bone(best, best)));
    int other = -1;
    for (int i = 0; i < P->nhand[P->turn]; i++)
        if (P->hand[P->turn][i] != P->lead) other = P->hand[P->turn][i];
    CHECK(other < 0 || !dom_legal(P, DA(DA_PLAY, other, END_C)));
    CHECK(dom_legal(P, DA(DA_PLAY, P->lead, END_C)) && !dom_legal(P, DA_DRAW) && !dom_legal(P, DA_PASS));
}

/* a position made by hand: hands, the yard, the turn */
static void setup(int variant, int players, const char *const *hands, const char *yard)
{
    dom_new(&G, variant, players, 1);
    for (int q = 0; q < players; q++) {
        P->nhand[q] = 0;
        for (const char *s = hands[q]; s[0] && s[1]; s += 2 + (s[2] == ' '))
            P->hand[q][P->nhand[q]++] = (uint8_t)dom_bone(s[0] - '0', s[1] - '0');
    }
    P->nyard = 0;
    for (const char *s = yard; s[0] && s[1]; s += 2 + (s[2] == ' '))
        P->yard[P->nyard++] = (uint8_t)dom_bone(s[0] - '0', s[1] - '0');
    P->turn = P->leader = 0;
    P->lead = DOM_NONE;
}

static void test_line(void)
{
    const char *hands[2] = {"35 56 14 44", "13 66 02 12"};
    setup(DOM_BLOCK, 2, hands, "");
    CHECK(dom_script(&G, "p35"));
    CHECK(P->center == dom_bone(3, 5) && dom_end(P, END_W) == 3 && dom_end(P, END_E) == 5 && P->turn == 1);
    CHECK(!dom_open(P, END_N) && !dom_open(P, END_S));
    CHECK(!dom_script(&G, "p66e"));             /* fits nowhere */
    CHECK(dom_script(&G, "p13w"));              /* 1-3 on the west: 1 shows */
    CHECK(dom_end(P, END_W) == 1 && P->out[END_W][0] == 1);
    CHECK(dom_script(&G, "p56e") && dom_end(P, END_E) == 6);
    CHECK(dom_script(&G, "p66e") && dom_end(P, END_E) == 6 && P->narm[END_E] == 2);   /* a double, no spinner */
    CHECK(dom_script(&G, "p14w") && dom_end(P, END_W) == 4);
    CHECK(!dom_open(P, END_N));
}

static void test_fives(void)
{
    const char *hands[2] = {"23 33 52 31 46", "35 05 11 60 22"};
    setup(DOM_FIVES, 2, hands, "40 44");
    CHECK(dom_script(&G, "p23"));               /* 2 + 3 = 5: scores 5 */
    CHECK(P->total[0] == 5 && P->scored == 5 && dom_count(P) == 5);
    CHECK(dom_script(&G, "p35e"));              /* 2 + 5 = 7: nothing */
    CHECK(P->total[1] == 0 && dom_count(P) == 7 && P->scored == 0);
    CHECK(dom_script(&G, "p33w") == 0);         /* 3-3 does not fit the west end (2) */
    CHECK(dom_script(&G, "p52w"));              /* west shows 5: 5 + 5 = 10 */
    CHECK(P->total[0] == 15 && dom_count(P) == 10);
    CHECK(dom_script(&G, "p05w"));              /* 0 + 5 = 5 */
    CHECK(P->total[1] == 5);
    CHECK(dom_script(&G, "x") == 0);            /* the boneyard is not empty: no pass */
    /* player 0: 33 31 46; ends 0 and 5: nothing fits, draw (44 on top, then 40) */
    CHECK(dom_legal(P, DA_DRAW) && !dom_legal(P, DA_PASS));
    CHECK(dom_script(&G, "d") && P->nhand[0] == 4 && P->turn == 0);
    CHECK(dom_script(&G, "d") && P->nhand[0] == 5 && !dom_legal(P, DA_DRAW) && !dom_legal(P, DA_PASS));
    CHECK(dom_script(&G, "p40w"));              /* west shows 4: 4 + 5 = 9 */
    CHECK(dom_count(P) == 9 && P->total[0] == 15);
    /* player 1: 11 60 22; nothing fits 4 or 5, the boneyard is empty: pass */
    CHECK(dom_legal(P, DA_PASS) && dom_script(&G, "x") && P->turn == 0);
    CHECK(dom_script(&G, "p44w"));              /* the first double, on the west end: the spinner */
    CHECK(P->spinner && P->center == dom_bone(4, 4) && P->narm[END_W] == 0 && P->narm[END_E] == 5);
    CHECK(dom_end(P, END_E) == 5 && dom_end(P, END_W) == 4 && !dom_open(P, END_N));
    CHECK(dom_count(P) == 8 + 5);               /* the spinner at the west end counts 8 */
    static const int arm[5][3] = {{4, 0, 0}, {0, 5, 5}, {5, 2, 2}, {2, 3, 3}, {3, 5, 5}};   /* bone, outer pip */
    int ok = 1;
    for (int i = 0; i < 5; i++)
        ok &= P->arm[END_E][i] == dom_bone(arm[i][0], arm[i][1]) && P->out[END_E][i] == arm[i][2];
    CHECK(ok);                                  /* the line turned round: from the spinner back to the far end */
}

static void test_spinner(void)
{
    const char *hands[2] = {"55 50 52 53 11", "54 51 22 00 66"};
    setup(DOM_FIVES, 2, hands, "");
    CHECK(dom_script(&G, "p55") && P->spinner && P->total[0] == 10);
    CHECK(!dom_open(P, END_N) && !dom_open(P, END_S));
    CHECK(dom_script(&G, "p54e") && dom_count(P) == 10 + 4 && P->total[1] == 0);
    CHECK(!dom_open(P, END_N));
    CHECK(dom_script(&G, "p50w") && dom_count(P) == 4 && P->total[0] == 10);   /* the spinner's sides: not yet */
    CHECK(dom_open(P, END_N) && dom_open(P, END_S) && dom_end(P, END_N) == 5);
    CHECK(dom_script(&G, "p51n") && dom_count(P) == 5 && P->total[1] == 5);   /* 4 + 0 + 1 */
    CHECK(dom_script(&G, "p53s") && dom_count(P) == 8);
}

static void test_round_end(void)
{
    const char *hands[2] = {"12 23", "34 66 55"};
    setup(DOM_BLOCK, 2, hands, "");
    CHECK(dom_script(&G, "p12 x p23e"));        /* player 0 out: scores 7 + 12 + 10 */
    CHECK(P->phase == DP_ROUND && P->winner == 0 && P->last[0] == 29 && P->total[0] == 29 && P->leader == 0);
    CHECK(dom_script(&G, "n") && P->phase == DP_PLAY && P->turn == 0 && P->lead == DOM_NONE && count_all(P));
    /* blocked: nobody can play; the lowest hand wins the others' pips */
    const char *b[3] = {"12 66", "00 33", "45 44"};
    setup(DOM_BLOCK, 3, b, "");
    CHECK(dom_script(&G, "p12 x x"));
    CHECK(dom_script(&G, "x") && P->phase == DP_ROUND && P->winner == 1 && P->last[1] == 12 + 17);
    /* a tie for the lowest: nobody scores; the next leader is the one after */
    const char *c[2] = {"12 06", "33"};
    setup(DOM_BLOCK, 2, c, "");
    CHECK(dom_script(&G, "p12 x x") && P->phase == DP_ROUND && P->winner == DOM_NONE && P->total[0] == 0 &&
          P->total[1] == 0 && P->leader == 1);
    /* All Fives: the pips rounded to the nearest 5 */
    const char *d[2] = {"12", "34 66"};
    setup(DOM_FIVES, 2, d, "");
    CHECK(dom_script(&G, "p12") && P->phase == DP_ROUND && P->last[0] == 20);   /* 19 -> 20 */
    const char *e[2] = {"12", "34 65"};
    setup(DOM_FIVES, 2, e, "");
    CHECK(dom_script(&G, "p12") && P->last[0] == 20);   /* 18 -> 20 */
    const char *f[2] = {"12", "34 64"};
    setup(DOM_FIVES, 2, f, "");
    CHECK(dom_script(&G, "p12") && P->last[0] == 15);   /* 17 -> 15 */
    /* the game: 100 */
    const char *g2[2] = {"12", "34 66"};
    setup(DOM_DRAW, 2, g2, "");
    P->total[0] = 90;
    CHECK(dom_script(&G, "p12") && P->phase == DP_OVER && dom_winners(P) == 1u);
    CHECK(!dom_legal(P, DA_NEXT) && !dom_script(&G, "n"));
}

/* whole games, random legal actions from seeds: the bones all there, All Fives' points in fives, an end */
static uint32_t rnd;
static int pick(int n) { return (int)(cards_rand(&rnd) % (uint32_t)n); }

static void test_random(void)
{
    int games = 0, ended = 0, bones_ok = 1, fives_ok = 1, undo_ok = 1;
    for (int seed = 1; seed <= 240; seed++) {
        int v = seed % 3, np = 2 + seed % 3;
        dom_new(&G, v, np, (uint32_t)seed * 7919u);
        rnd = (uint32_t)seed;
        for (int step = 0; step < 3000 && P->phase != DP_OVER; step++) {
            int acts[64], n = 0;
            for (int i = 0; i < P->nhand[P->turn]; i++)
                for (int e = 0; e <= END_C; e++)
                    if ((e < ENDS || e == END_C) && dom_legal(P, DA(DA_PLAY, P->hand[P->turn][i], e)) && n < 64)
                        acts[n++] = DA(DA_PLAY, P->hand[P->turn][i], e);
            if (dom_legal(P, DA_DRAW)) acts[n++] = DA_DRAW;
            if (dom_legal(P, DA_PASS)) acts[n++] = DA_PASS;
            if (dom_legal(P, DA_NEXT)) acts[n++] = DA_NEXT;
            if (!n) break;
            int before = P->total[P->turn], who = P->turn;
            if (!dom_play(&G, acts[pick(n)])) { bones_ok = 0; break; }
            if (P->phase == DP_PLAY && !count_all(P)) bones_ok = 0;
            if (v == DOM_FIVES && (P->total[who] - before) % 5) fives_ok = 0;
            if (v != DOM_FIVES && P->phase == DP_PLAY && P->total[who] != before) fives_ok = 0;
        }
        games++;
        ended += P->phase == DP_OVER;
        if (seed % 40 == 0) {                   /* undo: the same position again by replay */
            dpos keep = G.pos;
            int n = G.n;
            uint16_t last = G.act[n - 1];
            CHECK(dom_undo(&G) && G.n == n - 1 && dom_play(&G, last) && !memcmp(&keep, &G.pos, sizeof keep));
            undo_ok &= G.n == n;
        }
    }
    CHECK(bones_ok);
    CHECK(fives_ok);
    CHECK(undo_ok);
    CHECK(ended == games);
    /* the plain actions: whole games end too */
    for (int v = 0; v < DOM_VARIANTS; v++)
        for (int np = 2; np <= 4; np++) {
            dom_new(&G, v, np, 1234u + (unsigned)v * 10 + (unsigned)np);
            for (int r = 0; r < 200 && P->phase != DP_OVER; r++) CHECK(dom_script(&G, r ? "n #" : "#"));
            CHECK(P->phase == DP_OVER && dom_winners(P) != 0);
        }
}

int main(void)
{
    test_bones();
    test_deal();
    test_line();
    test_fives();
    test_spinner();
    test_round_end();
    test_random();
    printf("dominoes: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
