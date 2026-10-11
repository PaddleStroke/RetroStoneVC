/*
 * 421: the rules' test (every combination's rank and worth, the ranking's order and its ties, the charge from the
 * pot, the discharge between players, the leader's limit, stopping, going out, the end and its loser, two to six
 * players, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/fourtwoone.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static tgame G;
static const tpos *P = &G.pos;

static void faces(const char *s, uint8_t d[3])
{
    for (int i = 0; i < 3; i++) d[i] = (uint8_t)(s[i] - '0');
}
static int R(const char *s) { uint8_t d[3]; faces(s, d); return fto_rank(d); }
static int W(const char *s) { uint8_t d[3]; faces(s, d); return fto_value(d); }

static void test_combinations(void)
{
    /* the worth of each */
    CHECK(W("421") == 10 && W("124") == 10 && W("214") == 10);
    CHECK(W("111") == 7);
    CHECK(W("611") == 6 && W("666") == 6 && W("115") == 5 && W("555") == 5 && W("141") == 4 && W("444") == 4);
    CHECK(W("311") == 3 && W("333") == 3 && W("112") == 2 && W("222") == 2);
    CHECK(W("654") == 2 && W("345") == 2 && W("432") == 2 && W("123") == 2);
    CHECK(W("665") == 1 && W("631") == 1 && W("221") == 1 && W("531") == 1 && W("642") == 1);
    /* the ranking, best first */
    static const char *order[] = {"421", "111", "116", "666", "115", "555", "114", "444", "113", "333", "112", "222",
                                  "654", "543", "432", "321", "665", "664", "663", "662", "661", "655", "653", "652",
                                  "651", "644", "643", "642", "641", "633", "632", "631", "622", "621", "554", "553",
                                  "552", "551", "544", "542", "541", "533", "532", "531", "522", "521", "443", "442",
                                  "441", "433", "431", "422", "332", "331", "322", "221"};
    int n = sizeof order / sizeof order[0], ok = 1;
    for (int i = 1; i < n; i++) ok &= R(order[i - 1]) > R(order[i]);
    CHECK(ok && n == 56);                       /* every throw (56 sets of three faces) */
    CHECK(R("221") == 0 && R("212") == 0 && R("122") == 0);   /* the nénette, last */
    CHECK(R("164") == R("641") && R("246") == R("642"));
}

/* the charge: one throw each, the loser takes from the pot the best throw's worth */
static void test_charge(void)
{
    fto_new(&G, 3);
    CHECK(P->pot == 11 && P->phase == FTO_CHARGE && P->limit == 1 && P->turn == 0 && P->leader == 0);
    CHECK(!fto_can_stop(P) && !fto_stop(&G));
    CHECK(fto_script(&G, "d665") && P->turn == 1 && P->throws == 0);  /* one throw: the turn over */
    CHECK(fto_script(&G, "d116") && P->turn == 2);
    CHECK(fto_script(&G, "d532"));              /* 532 the worst: takes 6 (1-1-6) */
    CHECK(P->chips[2] == 6 && P->pot == 5 && P->rounds == 1 && P->leader == 2 && P->turn == 2);
    CHECK(P->last_win == 1 && P->last_lose == 2 && P->last_amount == 6 && P->last_pot);
    CHECK(fto_script(&G, "d421 d222 d221"));    /* 2: 421; 0: 222; 1: the nénette, takes 10, only 5 left */
    CHECK(P->chips[1] == 5 && P->pot == 0 && P->phase == FTO_DISCHARGE && P->last_amount == 5);
    CHECK(P->in == 6 && P->first_out == 0 && P->leader == 1 && P->turn == 1 && P->limit == 3);   /* 0: out, safe */
    /* the charge: a tie for the worst: the one made last loses */
    fto_new(&G, 2);
    CHECK(fto_script(&G, "d653 d635") && P->last_lose == 1 && P->chips[1] == 1);
    /* one player takes all the pot in the charge: they have lost */
    fto_new(&G, 2);
    CHECK(fto_script(&G, "d421 d653 d653 d421") && P->phase == FTO_OVER && P->loser == 1 && P->chips[1] == 11);
    CHECK(fto_winners(P) == 1 && P->first_out == 0 && !fto_script(&G, "d123"));
}

/* the discharge: the limit set by the leader, stopping, keeping dice, the winner gives, going out */
static void test_discharge(void)
{
    fto_new(&G, 3);
    CHECK(fto_script(&G, "d665 d116 d532 d421 d222 d221"));  /* 0: 0, 1: 5, 2: 6; 1 leads */
    CHECK(P->turn == 1 && P->limit == 3 && !fto_can_stop(P));
    CHECK(fto_script(&G, "d531") && fto_can_stop(P) && P->throws == 1);
    CHECK(fto_script(&G, "d-6-") && P->dice[1] == 6 && P->dice[0] == 5 && P->throws == 2);  /* 5 6 1 */
    CHECK(fto_script(&G, "s") && P->limit == 2 && P->turn == 2);  /* 0 is out: 2 next; at most two throws */
    CHECK(fto_script(&G, "d222") && fto_can_stop(P));
    CHECK(fto_script(&G, "d--1") && P->turn == 2 && P->rounds == 3);  /* 2's second throw: the limit, the round over */
    /* 2: 2-2-1 nénette, the worst; 1: 6-5-1 the best, gives 1 */
    CHECK(P->last_win == 1 && P->last_lose == 2 && P->chips[1] == 4 && P->chips[2] == 7 && P->last_amount == 1);
    CHECK(P->leader == 2 && P->turn == 2 && P->limit == 3);
    /* the winner gives what their throw is worth, and goes out with their last chip */
    CHECK(fto_script(&G, "d221 s d421"));       /* 2 stops at one throw: 1 may throw once */
    CHECK(P->chips[1] == 0 && P->chips[2] == 11 && P->phase == FTO_OVER && P->loser == 2);
    CHECK(fto_winners(P) == 3 && P->first_out == 0);
    /* equal throws in the discharge: the one made in fewer throws is better */
    fto_new(&G, 2);
    CHECK(fto_script(&G, "d421 d653 d421 d653") && P->phase == FTO_DISCHARGE && P->chips[0] == 1 && P->leader == 0);
    CHECK(fto_script(&G, "d531 d-6- s d651 s"));  /* 0: 5-6-1 in two throws; 1: 6-5-1 in one */
    CHECK(P->last_win == 1 && P->last_lose == 0 && P->chips[0] == 2 && P->chips[1] == 9);
}

/* the leader's limit: one throw, then the others too */
static void test_limit(void)
{
    fto_new(&G, 4);
    CHECK(fto_script(&G, "d665 d655 d643 d532"));      /* 3 takes 1 */
    CHECK(P->chips[3] == 1 && P->leader == 3);
    CHECK(fto_script(&G, "d665 d655 d643 d532"));      /* charge, leader 3: 3 665, 0 655, 1 643, 2 532: 2 takes 1 */
    CHECK(P->chips[2] == 1 && P->leader == 2);
    CHECK(fto_script(&G, "d421 d111 d116 d666"));      /* 2 421, 3 111, 0 116, 1 666: 1 the worst takes 9 */
    CHECK(P->chips[1] == 9 && P->pot == 0 && P->phase == FTO_DISCHARGE);
    CHECK(P->in == 0x0e && P->first_out == 0 && P->turn == 1 && P->leader == 1);   /* 0: out */
    CHECK(fto_script(&G, "d642 s") && P->limit == 1 && P->turn == 2);  /* the leader: one throw */
    CHECK(!fto_can_stop(P) && fto_script(&G, "d654") && P->turn == 3);  /* one throw only: over at once */
    CHECK(fto_script(&G, "d221") && P->rounds == 4);   /* 3: nénette, the worst; 2: 654 the best gives 2... */
    CHECK(P->last_win == 2 && P->last_lose == 3 && P->chips[2] == 0 && P->chips[3] == 2);   /* ...only 1: out */
    CHECK(P->in == 0x0a && P->leader == 3);
}

/* two to six players: the turn round the table, skipping who is out */
static void test_players(void)
{
    for (int np = 2; np <= FTO_PLAYERS; np++) {
        fto_new(&G, np);
        int ok = P->players == np && P->in == (1u << np) - 1;
        for (int q = 0; q < np; q++) {
            ok &= P->turn == q;
            fto_throw(&G, 0, (int[3]){6, 5, q == np - 1 ? 2 : 3});  /* the last player the worst */
        }
        CHECK(ok && P->chips[np - 1] == 1 && P->pot == 10 && P->leader == np - 1 && P->turn == np - 1);
    }
    fto_new(&G, 1);
    CHECK(P->players == 2);
}

/* undo: back to before the last stop; never a throw again */
static void test_undo(void)
{
    fto_new(&G, 2);
    CHECK(!fto_undo(&G));
    CHECK(fto_script(&G, "d665") && !fto_undo(&G));  /* the charge's throws: never undone */
    fto_new(&G, 3);
    CHECK(fto_script(&G, "d665 d116 d532 d421 d222 d221 d531 s"));
    CHECK(P->turn == 2 && fto_undo(&G) && P->turn == 1 && P->throws == 1 && P->dice[0] == 5);
    CHECK(fto_script(&G, "d-6- s d222") && P->turn == 2 && P->throws == 1);
    CHECK(fto_undo(&G) && P->turn == 1 && P->throws == 2 && G.n == 8);  /* the throws after the stop: dropped */
    CHECK(!fto_undo(&G));
}

static void test_script(void)
{
    fto_new(&G, 2);
    CHECK(!fto_script(&G, "x"));
    fto_new(&G, 2);
    CHECK(!fto_script(&G, "d12"));
    fto_new(&G, 2);
    CHECK(!fto_script(&G, "d1-2"));             /* nothing kept on the first throw */
    fto_new(&G, 2);
    CHECK(!fto_script(&G, "d127"));
    fto_new(&G, 2);
    CHECK(!fto_script(&G, "s"));
    fto_new(&G, 2);
    CHECK(fto_script(&G, "  d421   d653 ") && G.n == 2 && (G.ev[0] & 7) == 4 && (G.ev[0] >> 6 & 7) == 1);
}

int main(void)
{
    test_combinations();
    test_charge();
    test_discharge();
    test_limit();
    test_players();
    test_undo();
    test_script();
    printf("fourtwoone: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
