/*
 * The Spy: the rules' test (the deal: a spy each round among the players, the places never twice in a game; the
 * phases; every way a round scores; the notation; whole games from seeds, 3 to 10 players; the places in the 22
 * languages). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/spy.h"
#include "party_lists.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void look_all(spy_state *s)
{
    for (int i = 0; i < s->n; i++) CHECK(spy_looked(s));
    CHECK(s->phase == SPY_TALK);
}

static void test_deal(void)
{
    for (int n = SPY_MINP; n <= SPY_MAXP; n++)
        for (unsigned seed = 1; seed < 60; seed++) {
            spy_state s;
            spy_new(&s, n, seed);
            CHECK(s.n == n && s.phase == SPY_LOOK && s.looker == 0 && s.round == 0);
            int used[SPY_PLACES] = {0};
            for (int r = 0; r < SPY_ROUNDS; r++) {
                CHECK(s.round == r && s.phase == SPY_LOOK);
                CHECK(s.place >= 0 && s.place < SPY_PLACES && !used[s.place]++);
                CHECK(s.spy >= 0 && s.spy < n);
                CHECK(s.first == r % n);
                look_all(&s);
                CHECK(spy_call(&s));
                CHECK(spy_accuse(&s, (s.spy + 1) % n));
                CHECK(spy_next(&s));
            }
            CHECK(s.phase == SPY_OVER);
        }
    spy_state a, b;                             /* the same seed: the same game */
    spy_new(&a, 6, 1234);
    spy_new(&b, 6, 1234);
    CHECK(!memcmp(&a, &b, sizeof a));
    spy_new(&b, 6, 1235);
    CHECK(memcmp(a.deck, b.deck, sizeof a.deck) || a.spy != b.spy);
    int spies[4] = {0};                         /* each player is the spy about as often */
    for (unsigned seed = 0; seed < 4000; seed++) {
        spy_new(&a, 4, seed);
        spies[a.spy]++;
    }
    for (int i = 0; i < 4; i++) CHECK(spies[i] > 850 && spies[i] < 1150);
}

static void test_phases(void)
{
    spy_state s;
    spy_new(&s, 4, 9);
    CHECK(!spy_call(&s) && !spy_stop(&s) && !spy_accuse(&s, 0) && !spy_name(&s, 0) && !spy_next(&s));
    look_all(&s);
    CHECK(!spy_looked(&s) && !spy_accuse(&s, 0) && !spy_name(&s, 0));
    CHECK(spy_call(&s) && s.phase == SPY_VOTE);
    CHECK(!spy_accuse(&s, -1) && !spy_accuse(&s, 4) && !spy_name(&s, 1));
}

/* one round of 5 players to its score: accused (-1: the spy stops the talk), the place named (-1: the right one) */
static void round_to(spy_state *s, int accused, int named)
{
    spy_new(s, 5, 77);
    look_all(s);
    if (accused < 0) CHECK(spy_stop(s));
    else {
        CHECK(spy_call(s));
        CHECK(spy_accuse(s, accused));
    }
    if (s->phase == SPY_GUESS) CHECK(spy_name(s, named < 0 ? s->place : named));
    CHECK(s->phase == SPY_SCORE);
}

static void test_scores(void)
{
    spy_state s;
    spy_new(&s, 5, 77);
    int spy = s.spy, other = (spy + 2) % 5, wrong = (s.place + 1) % SPY_PLACES;
    round_to(&s, other, 0);                     /* someone else accused */
    for (int i = 0; i < 5; i++) CHECK(s.gain[i] == (i == spy ? 2 : 0) && s.score[i] == s.gain[i]);
    CHECK(s.accused == other && s.guess == -1);
    round_to(&s, spy, -1);                      /* caught, names the place */
    for (int i = 0; i < 5; i++) CHECK(s.gain[i] == (i == spy ? 3 : 0));
    CHECK(s.how == SPY_CAUGHT);
    round_to(&s, spy, wrong);                   /* caught, a wrong place */
    for (int i = 0; i < 5; i++) CHECK(s.gain[i] == (i == spy ? 0 : 1));
    round_to(&s, -1, -1);                       /* stops the talk, right */
    for (int i = 0; i < 5; i++) CHECK(s.gain[i] == (i == spy ? 3 : 0));
    CHECK(s.how == SPY_STOPPED);
    round_to(&s, -1, wrong);                    /* stops the talk, wrong */
    for (int i = 0; i < 5; i++) CHECK(s.gain[i] == (i == spy ? 0 : 1));
}

static void test_script(void)
{
    spy_state s, t;
    spy_new(&s, 3, 5);
    spy_new(&t, 3, 5);
    char m[64];
    snprintf(m, sizeof m, "l l l c v%d g%d n", s.spy, s.place);
    CHECK(spy_script(&s, m));
    CHECK(s.round == 1 && s.phase == SPY_LOOK && s.score[t.spy] == 3);
    CHECK(!spy_script(&s, "c"));
    CHECK(!spy_script(&s, "l l l c v9"));
    CHECK(!spy_script(&s, "x"));
}

static void test_games(void)
{
    for (int n = SPY_MINP; n <= SPY_MAXP; n++)
        for (unsigned seed = 1; seed <= 200; seed++) {
            spy_state s;
            spy_new(&s, n, seed);
            srand(seed);
            int sum = 0, steps = 0;
            while (s.phase != SPY_OVER && steps++ < 1000) {
                switch (s.phase) {
                case SPY_LOOK: CHECK(spy_looked(&s)); break;
                case SPY_TALK: CHECK(rand() % 4 ? spy_call(&s) : spy_stop(&s)); break;
                case SPY_VOTE: CHECK(spy_accuse(&s, rand() % 2 ? s.spy : rand() % n)); break;
                case SPY_GUESS: CHECK(spy_name(&s, rand() % 2 ? s.place : rand() % SPY_PLACES)); break;
                case SPY_SCORE: {
                    int g = 0;
                    for (int i = 0; i < n; i++) g += s.gain[i];
                    CHECK(g == 2 || g == 3 || g == n - 1);
                    sum += g;
                    CHECK(spy_next(&s));
                    break;
                }
                }
            }
            CHECK(s.phase == SPY_OVER);
            int total = 0;
            for (int i = 0; i < n; i++) total += s.score[i];
            CHECK(total == sum);
            unsigned w = spy_winners(&s);
            CHECK(w != 0 && w < 1u << n);
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    if (w >> i & 1) CHECK(s.score[i] >= s.score[j]);
        }
}

static void test_lists(void)
{
    if (!pl_found("spy_places")) {
        printf("(the places' files not found from here: not checked)\n");
        return;
    }
    checks += 3;
    failed += pl_check("spy_places", SPY_PLACES, 0);
    failed += pl_check("spy_words", 1, 0);
    failed += pl_check("spy_variants", SPY_VARIANTS, 0);
}

int main(void)
{
    test_deal();
    test_phases();
    test_scores();
    test_script();
    test_games();
    test_lists();
    printf("spy: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
