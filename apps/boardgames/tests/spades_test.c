/*
 * Spades: the rules' test (the deal, the bids, following suit, spades as trumps and broken, the contract made or
 * set, bags and their penalty, nil made and failed, the end at 500, undo, the notation; whole random games from
 * seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/spades.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sgame G, H;
static spos *P = &G.pos;

static void test_deal(void)
{
    spades_new(&G, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->n[pl] == 13 && P->bid[pl] == SPADES_NOBID);
        for (int i = 0; i < 13; i++) seen[P->hand[pl][i]]++, total++;
    }
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(once && total == 52 && P->phase == SP_BID && P->turn == 0 && P->first == 0);
    spades_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_bids(void)
{
    spades_new(&G, 3);
    CHECK(!spades_legal(P, SA_BID | 14) && spades_legal(P, SA_BID | 0) && spades_legal(P, SA_BID | 13));
    CHECK(!spades_legal(P, SA_PLAY | P->hand[0][0]));
    CHECK(spades_script(&G, "b3 b0 b4"));
    CHECK(P->phase == SP_BID && P->turn == 3);
    CHECK(spades_script(&G, "b2"));
    CHECK(P->phase == SP_PLAY && P->turn == 0 && P->leader == 0);
    CHECK(spades_contract(P, 0) == 7 && spades_contract(P, 1) == 2);
    CHECK(!spades_legal(P, SA_BID | 3));
}

/* hands set by hand, bids given, play phase, player 0 to lead */
static void set_hands(const char *const *hs, const int *bids)
{
    spades_new(&G, 3);
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
        P->bid[pl] = (uint8_t)bids[pl];
    }
    P->phase = SP_PLAY;
    P->turn = P->leader = 0;
    G.n = 0;
}

static void test_tricks(void)
{
    static const char *const a[4] = {"As 2h 3c", "Kh 4h 2s", "5h 3s 4c", "6c 7c 8c"};
    static const int bids[4] = {1, 1, 1, 1};
    set_hands(a, bids);
    CHECK(!spades_script(&G, "As"));            /* spades not broken: no spade lead (other cards in hand) */
    CHECK(spades_script(&G, "2h"));
    CHECK(!spades_script(&G, "2s"));            /* follow suit */
    CHECK(spades_script(&G, "Kh 5h"));
    CHECK(spades_script(&G, "6c"));             /* void: anything */
    CHECK(P->last_winner == 1 && P->won[1] == 1 && P->turn == 1 && !P->broken);
    CHECK(spades_script(&G, "4h 3s"));          /* a spade on a heart: trumps it, and breaks spades */
    CHECK(P->broken && spades_script(&G, "7c As"));
    CHECK(P->last_winner == 0 && P->won[0] == 1);   /* the ace of spades over the 3 */
    CHECK(spades_can_play(P, CARD(SUIT_CLUBS, 3)));
    /* only spades: a spade may lead */
    static const char *const b[4] = {"2s 3s", "2h 3h", "4h 5h", "6h 7h"};
    set_hands(b, bids);
    CHECK(spades_can_play(P, CARD(SUIT_SPADES, 2)));
    CHECK(trick_winner((const uint8_t[]){CARD(SUIT_HEARTS, 5), CARD(SUIT_HEARTS, 1), CARD(SUIT_CLUBS, 1),
                                         CARD(SUIT_SPADES, 2)}, 4, SUIT_SPADES) == 3);
    CHECK(trick_winner((const uint8_t[]){CARD(SUIT_HEARTS, 5), CARD(SUIT_HEARTS, 1), CARD(SUIT_CLUBS, 1),
                                         CARD(SUIT_HEARTS, 13)}, 4, SUIT_SPADES) == 1);
}

/* a round's last trick with tricks taken so far: the score */
static void last_trick(const int *bids, const int *won, int winner_side)
{
    /* player 0 leads a heart; the winner: player 0 (side 0) with the ace, or player 1 (side 1) with a spade */
    static const char *const a[4] = {"Ah", "2s", "3h", "4h"};
    static const char *const b[4] = {"Ah", "2h", "3h", "4h"};
    set_hands(winner_side ? a : b, bids);
    for (int pl = 0; pl < 4; pl++) P->won[pl] = (uint8_t)won[pl];
    P->tricks = 12;
    CHECK(spades_script(&G, ". . . ."));
    CHECK(P->round == 1);
}

static void test_scoring(void)
{
    /* side 0 bids 4 + 3 and takes 7 + 3 (the last trick to player 0): 70 and 3 bags; side 1 bids 2 + 2, takes 3: set */
    const int b1[4] = {4, 2, 3, 2}, w1[4] = {6, 1, 3, 2};
    last_trick(b1, w1, 0);
    CHECK(P->won[0] == 7 && P->pts[0] == 70 + 3 && P->over[0] == 3 && P->bags[0] == 3 && P->score[0] == 73);
    CHECK(P->pts[1] == -40 && P->score[1] == -40 && P->bags[1] == 0 && P->phase == SP_ROUND && P->turn == 1);
    /* nil failed: player 1 bids nil and takes the last trick; player 3 takes 1 for a bid of 2 */
    const int b3[4] = {6, 0, 5, 2}, w3[4] = {6, 0, 5, 1};
    last_trick(b3, w3, 1);
    CHECK(P->nil[1] == -100 && P->won[1] == 1);
    CHECK(spades_contract(P, 1) == 2 && spades_side_won(P, 1) == 1 && P->pts[1] == -20 - 100 + 1 && P->bags[1] == 1);
    CHECK(P->pts[0] == 110 && P->over[0] == 0);
    /* nil made: player 1 takes nothing, player 3 makes 2 */
    const int b4[4] = {6, 0, 5, 2}, w4[4] = {6, 0, 4, 2};
    last_trick(b4, w4, 0);
    CHECK(P->nil[1] == 100 && P->pts[1] == 20 + 100 && P->pts[0] == 110 && P->nil[0] == 0);
}

/* bags carried over: the tenth costs 100 */
static void test_bags(void)
{
    const int b[4] = {4, 2, 3, 2}, w[4] = {6, 1, 3, 2};
    set_hands((const char *const[]){"Ah", "2h", "3h", "4h"}, b);
    for (int pl = 0; pl < 4; pl++) P->won[pl] = (uint8_t)w[pl];
    P->tricks = 12;
    P->bags[0] = 8;
    P->score[0] = 200;
    CHECK(spades_script(&G, ". . . ."));
    CHECK(P->bags[0] == 1 && P->penalty[0] == 1 && P->pts[0] == 70 + 3 - 100 && P->score[0] == 173);
}

/* the end: a side reaches 500 (the higher wins); equal: one more round */
static void ending(int s0, int s1)
{
    const int b[4] = {4, 2, 3, 2}, w[4] = {6, 1, 3, 2};   /* side 0: +73; side 1: -40 */
    set_hands((const char *const[]){"Ah", "2h", "3h", "4h"}, b);
    for (int pl = 0; pl < 4; pl++) P->won[pl] = (uint8_t)w[pl];
    P->tricks = 12;
    P->score[0] = (int16_t)s0, P->score[1] = (int16_t)s1;
    CHECK(spades_script(&G, ". . . ."));
}

static void test_end(void)
{
    ending(450, 600);
    CHECK(P->phase == SP_OVER && P->score[0] == 523 && P->score[1] == 560 && spades_winner(P) == 1);
    ending(450, 300);
    CHECK(P->phase == SP_OVER && spades_winner(P) == 0 && !spades_legal(P, SA_NEXT));
    ending(450, 563);
    CHECK(P->score[0] == 523 && P->score[1] == 523 && P->phase == SP_ROUND && spades_winner(P) == -1);
    ending(400, 300);
    CHECK(P->phase == SP_ROUND);
}

static void test_undo(void)
{
    spades_new(&G, 77);
    CHECK(spades_script(&G, ". . . . . . . . ."));
    int n = G.n;
    spos before = G.pos;
    CHECK(spades_script(&G, ". . ."));
    CHECK(spades_undo(&G) && spades_undo(&G) && spades_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (spades_undo(&G)) {}
    spades_new(&H, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    for (uint32_t seed = 1; seed <= 20; seed++) {   /* "!": plain actions to the end of the game */
        spades_new(&G, seed);
        CHECK(spades_script(&G, "!") && P->phase == SP_OVER);
    }
}

/* whole games: random legal cards, plain or random bids (nil now and then) */
static void test_random_games(void)
{
    int games = 0, nils = 0, rounds = 0, penalties = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        spades_new(&G, seed * 104729u);
        uint32_t r = seed;
        int ok = 1, steps = 0;
        while (P->phase != SP_OVER && ok && steps++ < 200000) {
            if (P->phase == SP_ROUND) {
                int sum = 0;
                for (int pl = 0; pl < 4; pl++) sum += P->won[pl];
                ok &= sum == 13;
                for (int t = 0; t < 2; t++) ok &= P->bags[t] >= 0 && P->bags[t] < 10, penalties += P->penalty[t];
                rounds++;
                ok &= spades_play(&G, SA_NEXT);
                continue;
            }
            if (P->phase == SP_BID) {
                int a = spades_auto(P);
                if (SA_ARG(a) <= 1 && cards_rand(&r) % 3 == 0) a = SA_BID | SPADES_NIL, nils++;
                ok &= spades_play(&G, a);
                continue;
            }
            /* a legal card: random now and then; else the highest while the side still needs tricks (not a nil
             * bidder), the lowest when it does not */
            int pl = P->turn, k = 0, side = pl & 1;
            uint8_t legal[SPADES_HAND];
            for (int i = 0; i < P->n[pl]; i++)
                if (spades_can_play(P, P->hand[pl][i])) legal[k++] = P->hand[pl][i];
            ok &= k > 0;
            if (!k) break;
            int want = P->bid[pl] != SPADES_NIL && spades_side_won(P, side) < spades_contract(P, side);
            int pick = cards_rand(&r) % 4 == 0 ? (int)(cards_rand(&r) % k) : 0;
            if (!pick)
                for (int i = 1; i < k; i++) {
                    int better = trick_rank(legal[i]) + 20 * (CARD_SUIT(legal[i]) == SUIT_SPADES);
                    int now = trick_rank(legal[pick]) + 20 * (CARD_SUIT(legal[pick]) == SUIT_SPADES);
                    if (want ? better > now : better < now) pick = i;
                }
            ok &= spades_play(&G, SA_PLAY | legal[pick]);
        }
        ok &= P->phase == SP_OVER && (P->score[0] >= SPADES_LIMIT || P->score[1] >= SPADES_LIMIT);
        ok &= P->score[spades_winner(P)] > P->score[!spades_winner(P)];
        CHECK(ok);
        if (!ok) {
            printf("  seed %u: %d %d, phase %d, %d actions\n", seed, P->score[0], P->score[1], P->phase, G.n);
            break;
        }
        games++;
    }
    CHECK(games == 300);
    printf("  %d random games, %d rounds, %d nils, %d bag penalties\n", games, rounds, nils, penalties);
}

int main(void)
{
    test_deal();
    test_bids();
    test_tricks();
    test_scoring();
    test_bags();
    test_end();
    test_undo();
    test_random_games();
    printf("spades: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
