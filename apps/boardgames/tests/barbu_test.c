/*
 * Barbu: the rules' test (the declarers and their contracts, the leads and discards each contract allows, trumps'
 * obligations, the early end of a deal, dominoes, doubling and its settling, the totals adding up to nothing, undo,
 * the notation; whole random games from seeds, every variant). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/barbu.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static barbu_game G, H;
static barbu_pos *P = &G.pos;
static const int SUM[BARBU_CONTRACTS] = {-26, -30, -24, -20, -30, 65, 65};

static int card_of(const char *c)
{
    const char *s = c;
    return trick_parse(&s);
}

/* the hands set by hand (play phase, the contract chosen, no doubles) */
static void set_hands(const char *h0, const char *h1, const char *h2, const char *h3, int contract, int x)
{
    barbu_new(&G, BARBU_FULL_PLAIN, 3);
    CHECK(barbu_play(&G, BBA_CONTRACT(contract, x)));
    const char *hs[4] = {h0, h1, h2, h3};
    for (int pl = 0; pl < 4; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        trick_sort(P->hand[pl], P->n[pl]);
    }
    G.n = 0;
}

static void test_deals(void)
{
    for (int v = 0; v < BARBU_VARIANTS; v++) {
        barbu_new(&G, v, 9);
        CHECK(P->deals == (BARBU_IS_SHORT(v) ? 7 : 28) && P->phase == BBP_CHOOSE && P->declarer == 0 && P->turn == 0);
        int seen[256] = {0}, ok = 1;
        for (int pl = 0; pl < 4; pl++) {
            ok &= P->n[pl] == 13;
            for (int i = 0; i < 13; i++) seen[P->hand[pl][i]]++;
        }
        for (int c = 0; c < 256; c++) ok &= seen[c] <= 1;
        CHECK(ok);
    }
    for (int d = 0; d < 28; d++) CHECK(barbu_declarer_of(BARBU_FULL, d) == d / 7);
    for (int d = 0; d < 7; d++) CHECK(barbu_declarer_of(BARBU_SHORT, d) == d % 4);
    /* the full game: player 0 declares seven deals, each contract once; then player 1 may choose any again */
    barbu_new(&G, BARBU_FULL_PLAIN, 5);
    CHECK(!barbu_legal(P, BBA_CONTRACT(BB_TRUMPS, 4)) && !barbu_legal(P, BBA_CONTRACT(BB_DOMINO, 1)));
    CHECK(!barbu_legal(P, BBA_CONTRACT(BB_NOTRICKS, 2)) && !barbu_legal(P, BBA_CONTRACT(7, 0)));
    CHECK(barbu_script(&G, "c3 # n"));
    CHECK(P->declarer == 0 && !barbu_can_choose(P, BB_KING) && barbu_can_choose(P, BB_NOTRICKS));
    CHECK(barbu_script(&G, "c0 # n c1 # n c2 # n c4 # n c5h # n c6J # n"));
    CHECK(P->deal == 7 && P->declarer == 1 && P->turn == 1 && barbu_can_choose(P, BB_KING) && P->chosen[0] == 127);
    /* the short game: each contract once in all */
    barbu_new(&G, BARBU_SHORT_PLAIN, 5);
    CHECK(barbu_script(&G, "c3 # n"));
    CHECK(P->declarer == 1 && !barbu_can_choose(P, BB_KING));
    CHECK(barbu_script(&G, "! "));
    CHECK(P->phase == BBP_OVER && P->used == 127 && P->deal == 7);
}

static void test_tricks(void)
{
    /* no hearts: no heart lead with something else; the deal stops once the 13 hearts are out */
    set_hands("2h 3c", "Ah 4c", "Kh 5c", "Qh 6c", BB_NOHEARTS, 0);
    CHECK(P->turn == 0 && !barbu_can_play(P, card_of("2h")) && barbu_can_play(P, card_of("3c")));
    P->taken = 9;                               /* nine hearts out already */
    CHECK(barbu_script(&G, "3c 4c 5c 6c"));
    CHECK(P->last_winner == 3 && P->turn == 3);
    CHECK(barbu_script(&G, "Qh 2h Ah Kh"));     /* the last four hearts: player 1 takes the ace */
    CHECK(P->phase == BBP_ROUND && P->pts[1] == -12 && P->score[1] == -12 && P->table[1][BB_NOHEARTS] == -12);
    /* the king of hearts: the king's holder without the suit led must throw him; the deal stops when he is taken */
    set_hands("2c 3c", "Kh 4d", "5c 6c", "7c 8c", BB_KING, 0);
    CHECK(barbu_script(&G, "2c"));
    CHECK(!barbu_can_play(P, card_of("4d")) && barbu_can_play(P, BARBU_KING));
    CHECK(barbu_script(&G, "Kh 5c 7c"));
    CHECK(P->phase == BBP_ROUND && P->score[3] == -20 && P->tricks == 1);
    set_hands("2h 3c", "Kh 4c", "5c 6c", "7c 8c", BB_KING, 0);
    CHECK(!barbu_can_play(P, card_of("2h")));   /* no heart lead */
    /* no queens: minus 6 each, over at the fourth */
    set_hands("Qs Qh 2c", "Qd 3c 4d", "Qc 5c 6c", "7c 8c 9c", BB_NOQUEENS, 0);
    CHECK(barbu_script(&G, "2c 3c 5c 7c"));     /* player 3 wins with the 7 */
    CHECK(barbu_script(&G, "8c Qs Qd 6c"));     /* the 8 again: two queens thrown on it */
    CHECK(P->phase == BBP_PLAY && P->pts[3] == -12 && P->taken == 2);
    CHECK(barbu_script(&G, "9c Qh 4d Qc"));     /* the queen of clubs takes the last two */
    CHECK(P->phase == BBP_ROUND && P->pts[3] == -12 && P->pts[2] == -12 && P->tricks == 3);
    /* no tricks and the last two */
    set_hands("2c", "3c", "4c", "5c", BB_NOTRICKS, 0);
    P->tricks = 12;
    CHECK(barbu_script(&G, "2c 3c 4c 5c") && P->phase == BBP_ROUND && P->pts[3] == -2);
    set_hands("2c 9d", "3c 9h", "4c 9s", "5c Tc", BB_LASTTWO, 0);
    P->tricks = 11;
    CHECK(barbu_script(&G, "2c 3c 4c 5c"));
    CHECK(P->pts[3] == -10 && P->phase == BBP_PLAY);
    CHECK(barbu_script(&G, "Tc 9d 9h 9s") && P->phase == BBP_ROUND && P->pts[3] == -30);
    /* trumps (hearts): beat the best trump on a trump lead; void: trump, over the best trump if possible */
    set_hands("5h 2c", "7h 3h 4c", "2h Ac 3d", "Kh 4h 5d", BB_TRUMPS, SUIT_HEARTS);
    CHECK(barbu_script(&G, "5h"));
    CHECK(!barbu_can_play(P, card_of("3h")) && barbu_can_play(P, card_of("7h")));
    CHECK(barbu_script(&G, "7h 2h"));           /* nothing over the 7: any heart */
    CHECK(!barbu_can_play(P, card_of("4h")) && barbu_script(&G, "Kh"));
    CHECK(P->last_winner == 3 && P->pts[3] == 5);
    set_hands("2c 3s", "3h 4h 5s", "Ac 3d", "2h 5d 6s", BB_TRUMPS, SUIT_HEARTS);
    P->turn = P->leader = 2;
    CHECK(barbu_script(&G, "Ac"));
    CHECK(!barbu_can_play(P, card_of("5d")) && !barbu_can_play(P, card_of("6s")) && barbu_can_play(P, card_of("2h")));
    CHECK(barbu_script(&G, "2h 2c"));           /* player 3 trumps; player 0 follows */
    CHECK(!barbu_can_play(P, card_of("5s")) && barbu_can_play(P, card_of("3h")) && barbu_can_play(P, card_of("4h")));
    set_hands("2c 3s", "3h 4h 5s", "Ac 3d", "Kh 5d 6s", BB_TRUMPS, SUIT_HEARTS);
    P->turn = P->leader = 2;
    CHECK(barbu_script(&G, "Ac Kh 2c"));        /* nothing over the king: any trump, but a trump */
    CHECK(!barbu_can_play(P, card_of("5s")) && barbu_can_play(P, card_of("3h")) && barbu_can_play(P, card_of("4h")));
    set_hands("2c 3s", "5s 6s 7s", "Ac 3d", "Kh 5d 6s", BB_TRUMPS, SUIT_HEARTS);
    P->turn = P->leader = 2;
    CHECK(barbu_script(&G, "Ac Kh 2c"));        /* no club, no trump: anything */
    CHECK(barbu_can_play(P, card_of("5s")) && barbu_script(&G, "7s") && P->last_winner == 3);
}

static void test_dominoes(void)
{
    set_hands("Jh Qh 2c", "Js 3c", "Th Kd", "Jc Qc", BB_DOMINO, RANK_JACK);
    CHECK(P->turn == 0 && P->rank == 11);
    CHECK(!barbu_can_play(P, card_of("Qh")) && !barbu_can_play(P, card_of("2c")) && barbu_can_play(P, card_of("Jh")));
    CHECK(!barbu_legal(P, BBA_PASS) && barbu_script(&G, "Jh"));
    CHECK(barbu_script(&G, "Js"));              /* player 1: a jack starts spades */
    CHECK(barbu_can_play(P, card_of("Th")) && !barbu_can_play(P, card_of("Kd")));
    CHECK(barbu_script(&G, "Th Jc Qh"));        /* down and up from the jack */
    CHECK(P->lo[SUIT_HEARTS] == 10 && P->hi[SUIT_HEARTS] == 12);
    CHECK(barbu_must_pass(P) && barbu_script(&G, "p"));   /* player 1: the 3 of clubs fits nowhere */
    CHECK(barbu_must_pass(P) && barbu_script(&G, "p"));   /* player 2: the king of diamonds neither */
    CHECK(barbu_script(&G, "Qc"));              /* player 3 out first: 45 */
    CHECK(P->nout == 1 && P->out[0] == 3 && P->pts[3] == 45 && P->turn == 0);
    CHECK(barbu_must_pass(P) && barbu_script(&G, "p p p"));
    CHECK(P->turn == 0);
    P->lo[SUIT_CLUBS] = 4;                       /* the clubs down to 4: the 3 and then the 2 fit */
    CHECK(barbu_script(&G, "p 3c"));            /* player 1 out second: 20 */
    CHECK(P->pts[1] == 20 && P->turn == 2);
    CHECK(barbu_script(&G, "p 2c"));            /* player 0 out third: 5; player 2 last: minus 5 */
    CHECK(P->phase == BBP_ROUND && P->pts[0] == 5 && P->pts[2] == -5);
}

/* doubling: who may double whom, redoubles, the settling */
static void test_doubles(void)
{
    barbu_new(&G, BARBU_FULL, 21);
    CHECK(barbu_script(&G, "c3"));              /* the king, player 0 declares */
    CHECK(P->phase == BBP_DOUBLE && P->turn == 1 && barbu_double_mask(P) == (1u << 0 | 1u << 2 | 1u << 3));
    CHECK(!barbu_legal(P, BBA_DOUBLE | 1 << 1) && barbu_script(&G, "d02"));
    CHECK(P->turn == 2 && barbu_double_mask(P) == (1u << 0 | 1u << 3));   /* not back at player 1 */
    CHECK(barbu_script(&G, "d"));
    CHECK(P->turn == 3 && barbu_script(&G, "d0"));
    CHECK(P->phase == BBP_REDOUBLE && P->turn == 2 && barbu_double_mask(P) == 1u << 1);   /* player 2 first */
    CHECK(barbu_script(&G, "r1"));
    CHECK(P->turn == 0 && barbu_double_mask(P) == (1u << 1 | 1u << 3) && barbu_script(&G, "r3"));
    CHECK(P->phase == BBP_PLAY && P->turn == 0 && P->leader == 0);
    CHECK(P->dbl[1][0] == 1 && P->dbl[1][2] == 2 && P->dbl[3][0] == 2 && P->dbl[2][0] == 0);
    CHECK(barbu_script(&G, "#"));
    int s = 0, k = -1;
    for (int q = 0; q < 4; q++) s += P->score[q], k = P->pts[q] ? q : k;
    CHECK(s == -20 && k >= 0);
    /* the settling by hand: 1 doubled 0 (x1), 1 and 2 (x2), 3 and 0 (x2) */
    int16_t pts[4] = {P->pts[0], P->pts[1], P->pts[2], P->pts[3]};
    int16_t want[4] = {pts[0], pts[1], pts[2], pts[3]};
    want[1] += pts[1] - pts[0], want[0] -= pts[1] - pts[0];
    want[1] += 2 * (pts[1] - pts[2]), want[2] -= 2 * (pts[1] - pts[2]);
    want[3] += 2 * (pts[3] - pts[0]), want[0] -= 2 * (pts[3] - pts[0]);
    for (int q = 0; q < 4; q++) CHECK(P->score[q] == want[q]);
    /* positive contracts: only the declarer may be doubled */
    barbu_new(&G, BARBU_SHORT, 4);
    CHECK(barbu_script(&G, "c6Q"));
    CHECK(P->rank == 12 && barbu_double_mask(P) == 1u << 0 && !barbu_legal(P, BBA_DOUBLE | 1 << 2));
    CHECK(barbu_script(&G, "d d d"));           /* nobody doubles: straight to the play */
    CHECK(P->phase == BBP_PLAY && P->turn == 0);
}

static void test_undo(void)
{
    barbu_new(&G, BARBU_FULL, 77);
    CHECK(barbu_script(&G, "c5s d0 d0 d . . . . . . . ."));
    int n = G.n;
    barbu_pos before = G.pos;
    CHECK(barbu_script(&G, ". ."));
    CHECK(barbu_undo(&G) && barbu_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (barbu_undo(&G)) {}
    barbu_new(&H, BARBU_FULL, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    CHECK(!barbu_script(&G, "c7") && !barbu_script(&G, "c5x") && !barbu_script(&G, "c6Z"));
}

/* whole games from seeds, random legal actions in every variant: the cards kept, each deal's points the contract's,
 * every contract chosen once a declarer, the totals adding up to nothing */
static void test_random_games(void)
{
    int games = 0;
    for (uint32_t seed = 1; seed <= 120; seed++) {
        int v = (int)(seed % BARBU_VARIANTS);
        barbu_new(&G, v, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0;
        while (P->phase != BBP_OVER && ok && steps++ < 200000) {
            if (P->phase == BBP_ROUND) {
                int s = 0, pts = 0;
                for (int q = 0; q < 4; q++) s += P->score[q], pts += P->pts[q];
                ok &= s == SUM[P->contract] && pts == s;
                ok &= barbu_play(&G, BBA_NEXT);
                continue;
            }
            int pl = P->turn;
            if (P->phase == BBP_CHOOSE) {
                int c = (int)(cards_rand(&r) % 7), x = 0;
                while (!barbu_can_choose(P, c)) c = (c + 1) % 7;
                if (c == BB_TRUMPS) x = (int)(cards_rand(&r) % 4);
                if (c == BB_DOMINO) x = 2 + (int)(cards_rand(&r) % 13);
                ok &= barbu_play(&G, BBA_CONTRACT(c, x));
                continue;
            }
            if (P->phase == BBP_DOUBLE || P->phase == BBP_REDOUBLE) {
                int m = (int)(barbu_double_mask(P) & cards_rand(&r));
                ok &= barbu_play(&G, (P->phase == BBP_DOUBLE ? BBA_DOUBLE : BBA_REDOUBLE) | m);
                continue;
            }
            uint8_t legal[BARBU_HAND];
            int k = 0, cards = 0;
            for (int i = 0; i < P->n[pl]; i++)
                if (barbu_can_play(P, P->hand[pl][i])) legal[k++] = P->hand[pl][i];
            for (int q = 0; q < 4; q++) cards += P->n[q];
            if (P->contract != BB_DOMINO) ok &= cards + P->ntrick == 52 - 4 * P->tricks;
            if (!k) {
                ok &= P->contract == BB_DOMINO && barbu_play(&G, BBA_PASS);
                continue;
            }
            ok &= !barbu_legal(P, BBA_PASS);
            ok &= barbu_play(&G, BBA_PLAY | legal[cards_rand(&r) % (uint32_t)k]);
        }
        int s = 0;
        for (int q = 0; q < 4; q++) s += P->total[q];
        ok &= P->phase == BBP_OVER && s == 0 && P->used == 127;
        if (!BARBU_IS_SHORT(v))
            for (int q = 0; q < 4; q++) ok &= P->chosen[q] == 127;
        CHECK(ok);
        if (!ok) { printf("  seed %u variant %d\n", seed, v); break; }
        games++;
    }
    CHECK(games == 120);
    printf("  %d random games\n", games);
}

int main(void)
{
    test_deals();
    test_tricks();
    test_dominoes();
    test_doubles();
    test_undo();
    test_random_games();
    printf("barbu: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
