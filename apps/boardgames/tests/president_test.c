/*
 * Président: the rules' test (the deal, the plays one may make, passes and the end of a trick, the 2, going out,
 * places and roles, the exchange of cards, the scores, undo, the notation, whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/president.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static pgame G, H;
static ppos *P = &G.pos;

static uint32_t rng = 777;
static int rnd(int n)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

static int all_cards(const ppos *p, const uint8_t *gone, int ngone)
{
    int seen[256] = {0};
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++;
    for (int i = 0; i < p->npile; i++) seen[p->pile[i]]++;
    for (int i = 0; i < ngone; i++) seen[gone[i]]++;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return 1;
}

/* the hands and the cards played so far: every card once; the trick's cards among those played */
static int held_or_played(const ppos *p, const uint8_t *played, int n)
{
    int seen[256] = {0};
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++;
    for (int i = 0; i < n; i++) seen[played[i]]++;
    for (int i = 0; i < p->npile; i++)
        if (!seen[p->pile[i]]) return 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return 1;
}

/* give a player exactly these cards */
static void hand(int pl, const int *cards, int n)
{
    P->n[pl] = (uint8_t)n;
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cards[i];
}

static void test_deal(void)
{
    for (int np = 3; np <= 6; np++) {
        pres_new(&G, np, 42);
        CHECK(P->phase == PP_PLAY && P->turn == 0 && P->deal == 0 && all_cards(P, NULL, 0));
        int total = 0;
        for (int q = 0; q < np; q++) {
            total += P->n[q];
            CHECK(P->n[q] == 52 / np + (q < 52 % np) && P->role[q] == PR_NONE);
        }
        CHECK(total == 52);
    }
    pres_new(&H, 4, 42);
    pres_new(&G, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(pres_strength(3) == 1 && pres_strength(13) == 11 && pres_strength(1) == 12 && pres_strength(2) == 13);
}

static void test_plays(void)
{
    pres_new(&G, 4, 1);
    int h0[] = {CARD(0, 5), CARD(1, 5), CARD(2, 5), CARD(0, 9), CARD(1, 12)};
    int h1[] = {CARD(3, 5), CARD(0, 7), CARD(1, 7), CARD(2, 13), CARD(3, 13)};
    int h2[] = {CARD(0, 4), CARD(1, 2), CARD(3, 6)};
    int h3[] = {CARD(0, 8), CARD(1, 8), CARD(2, 8), CARD(3, 8), CARD(3, 1)};
    hand(0, h0, 5), hand(1, h1, 5), hand(2, h2, 3), hand(3, h3, 5);
    CHECK(!pres_legal(P, PA_PASS));                             /* the leader must play */
    CHECK(!pres_legal(P, PA_PLAYOF(5, 8)));                     /* the 5 of clubs: not theirs */
    CHECK(!pres_legal(P, PA_PLAYOF(4, 1)) && !pres_legal(P, PA_PLAYOF(5, 0)));
    CHECK(pres_legal(P, PA_PLAYOF(5, 7)) && pres_legal(P, PA_PLAYOF(5, 1)) && pres_legal(P, PA_PLAYOF(9, 1)));
    CHECK(pres_play(&G, PA_PLAYOF(5, 3)));                      /* a pair of 5s */
    CHECK(P->turn == 1 && P->top_count == 2 && P->top_rank == 5 && P->top_player == 0);
    CHECK(!pres_legal(P, PA_PLAYOF(13, 8)));                    /* one card: no */
    CHECK(!pres_legal(P, PA_PLAYOF(5, 8)));                     /* not higher */
    CHECK(pres_legal(P, PA_PLAYOF(7, 3)) && pres_legal(P, PA_PLAYOF(13, 12)) && pres_legal(P, PA_PASS));
    CHECK(pres_play(&G, PA_PLAYOF(7, 3)));                      /* 1: a pair of 7s */
    CHECK(pres_play(&G, PA_PASS));                              /* 2 passes */
    CHECK(P->turn == 3 && P->passes == 1 && P->passed == 4);
    CHECK(pres_play(&G, PA_PLAYOF(8, 3)));                      /* 3: a pair of 8s */
    CHECK(P->passes == 0 && P->passed == 0 && P->nplays == 3 && P->npile == 6);
    CHECK(pres_play(&G, PA_PASS) && pres_play(&G, PA_PLAYOF(13, 12)));   /* 0 passes; 1: kings */
    CHECK(P->turn == 2);
    CHECK(pres_play(&G, PA_PASS) && pres_play(&G, PA_PASS) && P->turn == 0);   /* 2, 3 pass */
    CHECK(pres_play(&G, PA_PASS));                              /* 0 passes: all three in a row */
    CHECK(P->turn == 1 && P->top_count == 0 && P->npile == 0);  /* 1 leads */
    /* 1 has one card left: their 5; they lead it and are out */
    CHECK(P->n[1] == 1 && pres_play(&G, PA_PLAYOF(5, 8)));
    CHECK(pres_place(P, 1) == 0 && !pres_active(P, 1) && P->turn == 2);
    /* 2 plays the 2: the trick ends at once, they lead again */
    CHECK(pres_play(&G, PA_PLAYOF(2, 2)));
    CHECK(P->turn == 2 && P->top_count == 0);
    CHECK(pres_play(&G, PA_PLAYOF(4, 1)));                      /* a 4 */
    CHECK(pres_play(&G, PA_PLAYOF(1, 8)));                      /* 3: an ace */
    CHECK(pres_play(&G, PA_PASS) && pres_play(&G, PA_PASS));    /* 0 and 2 pass (1 is out) */
    CHECK(P->turn == 3 && P->top_count == 0);
    /* a player out on a card nobody beats: the lead goes on to the next player still in */
    CHECK(pres_play(&G, PA_PLAYOF(8, 12)));                     /* 3: their last two 8s, and out */
    CHECK(pres_place(P, 3) == 1 && P->turn == 0);
    CHECK(pres_play(&G, PA_PASS) && pres_play(&G, PA_PASS));    /* 0, 2 pass */
    CHECK(P->turn == 0 && P->top_count == 0);                   /* the next after 3 still in: 0 */
    CHECK(pres_play(&G, PA_PLAYOF(9, 1)));                      /* 0: a 9 */
    CHECK(pres_play(&G, PA_PLAYOF(6, 8)) == 0);                 /* lower: no */
    CHECK(pres_play(&G, PA_PASS) && P->turn == 0);              /* 2 passes: 0 leads */
    CHECK(pres_play(&G, PA_PLAYOF(12, 2)));                     /* the queen */
    CHECK(pres_play(&G, PA_PASS) && P->turn == 0 && P->phase == PP_PLAY);
    CHECK(pres_play(&G, PA_PLAYOF(5, 4)));                      /* the last 5: 0 is out third */
    CHECK(P->phase == PP_DEAL && P->nout == 4 && P->out[2] == 0 && P->out[3] == 2);
    CHECK(P->last[1] == 3 && P->last[3] == 2 && P->last[0] == 1 && P->last[2] == 0 && P->deal == 1);
    CHECK(P->role[1] == PR_PRES && P->role[3] == PR_VICE && P->role[0] == PR_VICETROU && P->role[2] == PR_TROU);
}

static int strongest(const ppos *p, int pl)
{
    int b = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (pres_strength(CARD_RANK(p->hand[pl][i])) > b) b = pres_strength(CARD_RANK(p->hand[pl][i]));
    return b;
}

/* the exchange: the Trou's two best cards to the Président, the Vice-trou's best to the Vice; then they give back */
static void test_exchange(void)
{
    for (int np = 3; np <= 6; np++) {
        pres_new(&G, np, 99 + np);
        CHECK(pres_script(&G, "#"));
        CHECK(P->phase == PP_DEAL);
        int role[PRES_MAXP];
        for (int q = 0; q < np; q++) role[q] = P->role[q];
        /* the deal before the exchange, from the same shuffle: replay it by hand */
        H = G;
        CHECK(pres_play(&G, PA_NEXT));
        int pres = -1, trou = -1, vice = -1, vt = -1;
        for (int q = 0; q < np; q++) {
            if (role[q] == PR_PRES) pres = q;
            if (role[q] == PR_TROU) trou = q;
            if (role[q] == PR_VICE) vice = q;
            if (role[q] == PR_VICETROU) vt = q;
        }
        CHECK(pres >= 0 && trou >= 0 && (np >= 4) == (vice >= 0) && (np >= 4) == (vt >= 0));
        CHECK(P->phase == PP_GIVE && P->turn == pres && P->give_left == 2 && P->receiver == trou);
        CHECK(P->ngot[pres] == 2 && P->ngot[trou] == 0);
        /* the Trou kept nothing better than what they gave */
        int g0 = pres_strength(CARD_RANK(P->got[pres][0])), g1 = pres_strength(CARD_RANK(P->got[pres][1]));
        CHECK(strongest(P, trou) <= g0 && strongest(P, trou) <= g1);
        CHECK(P->n[trou] == 52 / np + (0 < 52 % np) - 2);       /* dealt first */
        if (vice >= 0) CHECK(P->ngot[vice] == 1 && strongest(P, vt) <= pres_strength(CARD_RANK(P->got[vice][0])));
        CHECK(!pres_legal(P, PA_PASS) && !pres_legal(P, PA_PLAYOF(CARD_RANK(P->hand[pres][0]), 1)));
        int c0 = P->hand[pres][0], c1 = P->hand[pres][1];
        CHECK(!pres_legal(P, PA_GIVE | P->hand[trou][0]));      /* not theirs */
        CHECK(pres_play(&G, PA_GIVE | c0) && P->turn == pres && P->give_left == 1);
        CHECK(pres_play(&G, PA_GIVE | c1));
        CHECK(P->ngot[trou] == 2 && P->got[trou][0] == c0 && P->got[trou][1] == c1);
        if (vice >= 0) {
            CHECK(P->phase == PP_GIVE && P->turn == vice && P->receiver == vt && P->give_left == 1);
            CHECK(pres_play(&G, PA_GIVE | P->hand[vice][0]));
        }
        CHECK(P->phase == PP_PLAY && P->turn == trou && all_cards(P, NULL, 0));
        int total = 0;
        for (int q = 0; q < np; q++) total += P->n[q];
        CHECK(total == 52);
    }
}

static void test_undo_script(void)
{
    pres_new(&G, 4, 3);
    CHECK(pres_script(&G, ". . . ."));
    ppos before = G.pos;
    CHECK(pres_play(&G, pres_auto(P)));
    CHECK(pres_undo(&G) && !memcmp(&before, &G.pos, sizeof before) && G.n == 4);
    pres_new(&G, 4, 3);
    CHECK(!pres_undo(&G) && !pres_script(&G, "z") && !pres_script(&G, "p"));   /* the leader cannot pass */
    /* a run of one rank's characters: that many of it */
    pres_new(&G, 3, 3);
    int r = CARD_RANK(P->hand[0][0]), m = 0, k = 0;
    for (int i = 0; i < P->n[0]; i++)
        if (CARD_RANK(P->hand[0][i]) == r) m |= 1 << CARD_SUIT(P->hand[0][i]), k++;
    static const char RK[] = " A23456789TJQK";
    char buf[8] = {0};
    for (int i = 0; i < k; i++) buf[i] = RK[r];
    CHECK(pres_script(&G, buf) && P->top_count == k && P->top_rank == r && P->npile == k);
    CHECK(!pres_script(&G, "gAs"));
}

/* whole games, random legal actions: every card once, the places and points, five deals, the winners */
static int bad_line;
#define GOOD(c) do { if (!(c) && !bad_line) bad_line = __LINE__; } while (0)

static void test_random_games(void)
{
    for (int np = 3; np <= 6; np++)
        for (uint32_t seed = 1; seed <= 120; seed++) {
            pres_new(&G, np, seed);
            rng = seed * 31u + (uint32_t)np;
            uint8_t gone[52];
            int ngone = 0, steps = 0, sum[PRES_MAXP] = {0}, deals = 0;
            bad_line = 0;
            while (P->phase != PP_OVER && steps++ < 50000 && !bad_line) {
                int acts[256], n = 0, pl = P->turn;
                if (P->phase == PP_GIVE)
                    for (int i = 0; i < P->n[pl]; i++) acts[n++] = PA_GIVE | P->hand[pl][i];
                else if (P->phase == PP_DEAL) acts[n++] = PA_NEXT;
                else {
                    if (pres_legal(P, PA_PASS)) acts[n++] = PA_PASS;
                    for (int r = 1; r <= 13; r++)
                        for (int mk = 1; mk < 16; mk++)
                            if (pres_legal(P, PA_PLAYOF(r, mk))) acts[n++] = PA_PLAYOF(r, mk);
                }
                GOOD(n > 0);
                if (!n) break;
                GOOD(P->phase != PP_PLAY || pres_active(P, pl));
                int a = acts[rnd(n)];
                if (a == PA_PASS && n > 1 && rnd(2)) a = acts[1 + rnd(n - 1)];
                int phase = P->phase;
                GOOD(pres_play(&G, a));
                if (PA_KIND(a) == PA_PLAY)
                    for (int s = 0; s < 4; s++)
                        if (PA_MASK(a) >> s & 1) gone[ngone++] = (uint8_t)CARD(s, PA_RANK(a));
                if (P->phase == PP_PLAY) GOOD(held_or_played(P, gone, ngone));
                if (phase == PP_PLAY && P->phase != PP_PLAY) {
                    deals++;
                    int seen = 0;
                    for (int i = 0; i < np; i++) {
                        seen |= 1 << P->out[i];
                        GOOD(P->last[P->out[i]] == pres_points(np, i));
                    }
                    GOOD(P->nout == np && seen == (1 << np) - 1 && P->deal == deals);
                    for (int q = 0; q < np; q++) {
                        sum[q] += P->last[q];
                        GOOD(P->score[q] == sum[q]);
                    }
                    ngone = 0;
                }
                if (P->phase == PP_GIVE) ngone = 0;
            }
            CHECK(!bad_line);
            if (bad_line) printf("   %d players, seed %u: line %d, step %d\n", np, seed, bad_line, steps);
            CHECK(P->phase == PP_OVER && P->deal == PRES_DEALS);
            unsigned w = pres_winners(P);
            int best = -1;
            for (int q = 0; q < np; q++) best = sum[q] > best ? sum[q] : best;
            for (int q = 0; q < np; q++) CHECK(((w >> q) & 1) == (sum[q] == best));
        }
}

int main(void)
{
    test_deal();
    test_plays();
    test_exchange();
    test_undo_script();
    test_random_games();
    printf("president: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
