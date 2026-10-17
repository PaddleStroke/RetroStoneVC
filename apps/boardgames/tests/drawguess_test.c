/*
 * Draw and Guess: the rules' test (the turns: each player draws in order, as often as the rules say, never the same
 * word twice in a game; the phases; the points of a guess and of the time running out; the notation; whole games
 * from seeds, 2 to 8 players; the words in the 22 languages, each once). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/drawguess.h"
#include "party_lists.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void test_turns(void)
{
    for (int n = DG_MINP; n <= DG_MAXP; n++)
        for (unsigned seed = 1; seed <= 100; seed++) {
            dg_state s;
            dg_new(&s, n, seed);
            CHECK(s.turns == n * (n <= 3 ? 3 : 2));
            int used[DG_WORDS] = {0}, drew[DG_MAXP] = {0};
            for (int t = 0; t < s.turns; t++) {
                CHECK(s.phase == DG_LOOK && s.turn == t && s.drawer == t % n && s.guesser == -1);
                CHECK(s.word >= 0 && s.word < DG_WORDS && !used[s.word]++);
                drew[s.drawer]++;
                CHECK(dg_begin(&s) && s.phase == DG_DRAW);
                CHECK(dg_timeout(&s) && s.phase == DG_SHOW);
                CHECK(dg_next(&s));
            }
            CHECK(s.phase == DG_OVER);
            for (int i = 0; i < n; i++) CHECK(drew[i] == dg_rounds(n) && s.score[i] == 0);
            CHECK(dg_winners(&s) == (1u << n) - 1);   /* nobody scored: everyone shares it */
        }
    dg_state a, b;
    dg_new(&a, 5, 42);
    dg_new(&b, 5, 42);
    CHECK(!memcmp(&a, &b, sizeof a));
    dg_new(&b, 5, 43);
    CHECK(memcmp(a.deck, b.deck, sizeof a.deck));
}

static void test_points(void)
{
    dg_state s;
    dg_new(&s, 4, 7);
    CHECK(!dg_guessed(&s, 1) && !dg_timeout(&s) && !dg_next(&s));
    CHECK(dg_begin(&s) && !dg_begin(&s));
    CHECK(!dg_guessed(&s, 0) && !dg_guessed(&s, 4) && !dg_guessed(&s, -1));   /* not the drawer, not a stranger */
    CHECK(dg_guessed(&s, 2));
    CHECK(s.score[2] == 2 && s.score[0] == 1 && s.score[1] == 0 && s.gain[2] == 2 && s.gain[0] == 1);
    CHECK(!dg_guessed(&s, 3) && !dg_timeout(&s));
    CHECK(dg_next(&s) && s.drawer == 1 && s.gain[2] == 0);
    CHECK(dg_begin(&s) && dg_guessed(&s, 0));
    CHECK(s.score[0] == 3 && s.score[1] == 1 && s.score[2] == 2);
    CHECK(dg_winners(&s) == 1u);
}

static void test_script(void)
{
    dg_state s;
    dg_new(&s, 2, 3);
    CHECK(dg_script(&s, "l g1 n l t n"));
    CHECK(s.turn == 2 && s.drawer == 0 && s.score[0] == 1 && s.score[1] == 2);
    CHECK(!dg_script(&s, "g1"));
    CHECK(!dg_script(&s, "l g0"));
    CHECK(!dg_script(&s, "z"));
}

static void test_games(void)
{
    for (int n = DG_MINP; n <= DG_MAXP; n++)
        for (unsigned seed = 1; seed <= 300; seed++) {
            dg_state s;
            dg_new(&s, n, seed);
            srand(seed + 1000u * (unsigned)n);
            int sum = 0, steps = 0;
            while (s.phase != DG_OVER && steps++ < 1000) {
                if (s.phase == DG_LOOK) CHECK(dg_begin(&s));
                else if (s.phase == DG_DRAW) {
                    int p = rand() % n;
                    if (rand() % 4 && p != s.drawer) {
                        CHECK(dg_guessed(&s, p));
                        sum += 3;
                    } else CHECK(dg_timeout(&s));
                } else CHECK(dg_next(&s));
            }
            CHECK(s.phase == DG_OVER);
            int total = 0;
            for (int i = 0; i < n; i++) total += s.score[i];
            CHECK(total == sum);
            unsigned w = dg_winners(&s);
            CHECK(w && w < 1u << n);
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    if (w >> i & 1) CHECK(s.score[i] >= s.score[j]);
        }
}

static void test_lists(void)
{
    if (!pl_found("drawguess_words")) {
        printf("(the words' files not found from here: not checked)\n");
        return;
    }
    checks += 2;
    failed += pl_check("drawguess_words", DG_WORDS, 0);
    failed += pl_check("drawguess_variants", DG_VARIANTS, 0);
}

int main(void)
{
    test_turns();
    test_points();
    test_script();
    test_games();
    test_lists();
    printf("drawguess: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
