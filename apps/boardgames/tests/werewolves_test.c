/*
 * Werewolves: the rules' test (the deal: the roles for 5 to 12 players, at random seats; the night's steps, skipping
 * the dead seer and witch; the witch's potions, once each; the dawn's dead, the vote; both ways to win; the notation;
 * whole games from seeds; the roles' names in the 22 languages). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/werewolves.h"
#include "party_lists.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int first(const ww_state *s, int role, int skip)    /* the first player alive of a role, not skip (-1) */
{
    for (int i = 0; i < s->n; i++)
        if (s->alive[i] && s->role[i] == role && i != skip) return i;
    return -1;
}

static int first_not(const ww_state *s, int role, int skip)
{
    for (int i = 0; i < s->n; i++)
        if (s->alive[i] && s->role[i] != role && i != skip) return i;
    return -1;
}

static void deal_all(ww_state *s)
{
    for (int i = 0; i < s->n; i++) CHECK(ww_looked(s));
    CHECK(s->phase == WP_WOLVES && s->night == 1);
}

static void test_deal(void)
{
    for (int n = WW_MINP; n <= WW_MAXP; n++) {
        int wolf_at[WW_MAXP] = {0};
        for (unsigned seed = 1; seed <= 400; seed++) {
            ww_state s;
            ww_new(&s, n, seed);
            int k[WW_ROLES] = {0};
            for (int i = 0; i < n; i++) {
                CHECK(s.role[i] < WW_ROLES && s.alive[i]);
                k[s.role[i]]++;
                wolf_at[i] += s.role[i] == WW_WOLF;
            }
            CHECK(k[WW_WOLF] == (n >= 10 ? 3 : n >= 7 ? 2 : 1));
            CHECK(k[WW_SEER] == 1 && k[WW_WITCH] == (n >= 7));
            CHECK(k[WW_VILLAGER] == n - k[WW_WOLF] - 1 - (n >= 7));
            CHECK(s.heal == (n >= 7) && s.poison == (n >= 7) && s.phase == WP_DEAL && s.looker == 0);
        }
        for (int i = 0; i < n; i++) CHECK(wolf_at[i] > 0);   /* any seat may be a werewolf */
    }
    ww_state a, b;
    ww_new(&a, 8, 99);
    ww_new(&b, 8, 99);
    CHECK(!memcmp(&a, &b, sizeof a));
}

static void test_night(void)
{
    ww_state s;
    ww_new(&s, 8, 3);
    CHECK(!ww_kill(&s, 0) && !ww_next(&s) && !ww_vote(&s, 0));
    deal_all(&s);
    int wolf = first(&s, WW_WOLF, -1), seer = first(&s, WW_SEER, -1), witch = first(&s, WW_WITCH, -1);
    int vil = first(&s, WW_VILLAGER, -1);
    CHECK(!ww_kill(&s, wolf) && !ww_kill(&s, -1) && !ww_kill(&s, 8));
    CHECK(ww_holder_role(&s) == WW_WOLF);
    CHECK(ww_kill(&s, vil) && s.phase == WP_SEER && s.victim == vil);
    CHECK(!ww_look(&s, seer) && ww_look(&s, wolf) && s.phase == WP_WITCH && s.seen == wolf);
    CHECK(!ww_witch(&s, 0, witch));             /* not herself */
    CHECK(ww_witch(&s, 1, -1) && s.phase == WP_DAWN && s.ndead == 0 && s.alive[vil] && !s.heal && s.poison);
    CHECK(ww_next(&s) && s.phase == WP_VOTE);
    CHECK(ww_vote(&s, wolf) && s.phase == WP_OUT && !s.alive[wolf] && s.out == wolf);
    CHECK(ww_next(&s) && s.phase == WP_WOLVES && s.night == 2);
    int wolf2 = first(&s, WW_WOLF, -1);
    CHECK(ww_kill(&s, witch) && ww_look(&s, vil));
    CHECK(s.phase == WP_WITCH);                 /* she is still alive tonight: she acts, then dies at dawn */
    CHECK(!ww_witch(&s, 1, -1));                /* the life potion is gone */
    CHECK(ww_witch(&s, 0, wolf2) && s.phase == WP_OVER && s.winner == WW_VILLAGE_WINS);
    CHECK(s.ndead == 2 && !s.alive[witch] && !s.alive[wolf2]);
}

static void test_skips(void)
{
    ww_state s;                                 /* 5 players: no witch; the seer dead: straight to the dawn */
    ww_new(&s, 5, 11);
    deal_all(&s);
    int seer = first(&s, WW_SEER, -1);
    CHECK(ww_kill(&s, seer) && s.phase == WP_SEER);
    CHECK(ww_look(&s, first_not(&s, WW_SEER, -1)) && s.phase == WP_DAWN && !s.alive[seer] && s.ndead == 1);
    CHECK(ww_next(&s));
    CHECK(ww_vote(&s, first(&s, WW_VILLAGER, -1)));
    if (s.phase == WP_OUT) {
        CHECK(ww_next(&s) && s.phase == WP_WOLVES);
        CHECK(ww_kill(&s, first(&s, WW_VILLAGER, -1)) && (s.phase == WP_DAWN || s.phase == WP_OVER));
    }
}

static void test_wolves_win(void)
{
    ww_state s;                                 /* 5 players, 1 wolf: two villagers out and it is one against one */
    ww_new(&s, 5, 21);
    deal_all(&s);
    int wolf = first(&s, WW_WOLF, -1);
    CHECK(ww_kill(&s, first_not(&s, WW_WOLF, -1)));
    if (s.phase == WP_SEER) CHECK(ww_look(&s, wolf));
    CHECK(s.phase == WP_DAWN && ww_next(&s));
    CHECK(ww_vote(&s, first_not(&s, WW_WOLF, -1)) && s.phase == WP_OUT);
    CHECK(ww_next(&s) && ww_kill(&s, first_not(&s, WW_WOLF, -1)));
    if (s.phase == WP_SEER) CHECK(ww_look(&s, wolf));
    CHECK(s.phase == WP_OVER && s.winner == WW_WOLVES_WIN && ww_alive(&s, -1) == 2);
}

static void test_script(void)
{
    ww_state s, t;
    ww_new(&s, 7, 5);
    t = s;
    char m[128];
    int wolf = first(&s, WW_WOLF, -1), vil = first(&s, WW_VILLAGER, -1), other = first(&s, WW_VILLAGER, vil);
    snprintf(m, sizeof m, "l l l l l l l k%d e%d whp%d n v%d", vil, wolf, other, wolf);
    CHECK(ww_script(&s, m));
    CHECK(s.alive[vil] && !s.alive[other] && !s.alive[wolf] && s.phase == WP_OUT);
    CHECK(!ww_script(&s, "v1"));
    CHECK(!ww_script(&t, "l l k0"));
    CHECK(!ww_script(&t, "q"));
}

static void test_games(void)
{
    for (int n = WW_MINP; n <= WW_MAXP; n++)
        for (unsigned seed = 1; seed <= 300; seed++) {
            ww_state s;
            ww_new(&s, n, seed);
            srand(seed * 7 + (unsigned)n);
            int steps = 0;
            while (s.phase != WP_OVER && steps++ < 2000) {
                int p = rand() % n;
                switch (s.phase) {
                case WP_DEAL: CHECK(ww_looked(&s)); break;
                case WP_WOLVES: if (ww_can_kill(&s, p)) CHECK(ww_kill(&s, p)); break;
                case WP_SEER: if (ww_can_look(&s, p)) CHECK(ww_look(&s, p)); break;
                case WP_WITCH: {
                    int save = s.heal && rand() % 3 == 0, poison = rand() % 4 == 0 && ww_can_poison(&s, p) ? p : -1;
                    CHECK(ww_witch(&s, save, poison));
                    break;
                }
                case WP_VOTE: if (s.alive[p]) CHECK(ww_vote(&s, p)); break;
                default: CHECK(ww_next(&s)); break;
                }
                CHECK(ww_alive(&s, WW_WITCH) <= 1 && ww_alive(&s, WW_SEER) <= 1);
            }
            CHECK(s.phase == WP_OVER);
            int w = ww_alive(&s, WW_WOLF), o = ww_alive(&s, -1) - w;
            CHECK(s.winner == WW_VILLAGE_WINS ? w == 0 : w >= o && w > 0);
            CHECK(s.night >= 1 && s.night <= n);
        }
}

static void test_lists(void)
{
    if (!pl_found("werewolves_words")) {
        printf("(the words' files not found from here: not checked)\n");
        return;
    }
    checks += 2;
    failed += pl_check("werewolves_words", WW_ROLES, 0);
    failed += pl_check("werewolves_variants", WW_VARIANTS, 0);
}

int main(void)
{
    test_deal();
    test_night();
    test_skips();
    test_wolves_win();
    test_script();
    test_games();
    test_lists();
    printf("werewolves: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
