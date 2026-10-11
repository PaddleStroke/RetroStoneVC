/*
 * Liar's Dice: the rules' test (the throw from a seed, the bids and what raises them, the wild ones, the call and
 * who loses a die, the next round's starter, players out, the end, undo, the notation; whole random games from
 * seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "cards.h"
#include "games/liarsdice.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static lgame G, H;
static lpos *P = &G.pos;

static void set_dice(int q, const char *faces)
{
    P->ndice[q] = (uint8_t)strlen(faces);
    for (int i = 0; i < LD_DICE; i++) P->dice[q][i] = (uint8_t)(faces[i] && i < P->ndice[q] ? faces[i] - '0' : 0);
}

static void test_new(void)
{
    ld_new(&G, 4, 42);
    ld_new(&H, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->players == 4 && ld_total(P) == 20 && P->turn == 0 && P->phase == LP_BID && P->count == 0);
    int ok = 1;
    for (int q = 0; q < 4; q++)
        for (int i = 0; i < 5; i++) ok &= P->dice[q][i] >= 1 && P->dice[q][i] <= 6;
    CHECK(ok);
    ld_new(&H, 4, 43);
    CHECK(memcmp(H.pos.dice, G.pos.dice, sizeof G.pos.dice) != 0);
    /* over many seeds, every face comes up about as often */
    int n[7] = {0};
    for (uint32_t s = 1; s <= 2000; s++) {
        ld_new(&H, 6, s);
        for (int q = 0; q < 6; q++)
            for (int i = 0; i < 5; i++) n[H.pos.dice[q][i]]++;
    }
    for (int f = 1; f <= 6; f++) CHECK(n[f] > 9000 && n[f] < 11000);
}

static void test_bids(void)
{
    ld_new(&G, 3, 7);
    CHECK(!ld_legal(P, LD_ACT(LA_LIAR, 0, 0)));            /* nothing to call yet */
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 2, 1)));             /* no bids on ones: they are wild */
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 16, 3)));            /* more dice than there are */
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 0, 3)));
    CHECK(ld_script(&G, "3x4") && P->turn == 1 && P->count == 3 && P->face == 4 && P->bidder == 0);
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 3, 4)));             /* the same bid */
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 3, 2)));             /* as many of a lower face */
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 2, 6)));             /* fewer dice */
    CHECK(ld_legal(P, LD_ACT(LA_BID, 3, 5)) && ld_legal(P, LD_ACT(LA_BID, 4, 2)));
    CHECK(ld_script(&G, "3x6 4x2") && P->turn == 0 && P->nbids == 3 && P->bids[2].player == 2);
    CHECK(!ld_script(&G, "4x7") && !ld_script(&G, "x2"));
}

static void test_call(void)
{
    /* the ones count for the bid's face */
    ld_new(&G, 3, 5);
    set_dice(0, "11234");
    set_dice(1, "44566");
    set_dice(2, "22222");
    CHECK(ld_counts(P, 4) == 5 && ld_counts(P, 2) == 8 && ld_counts(P, 6) == 4);
    /* the bid holds (exactly): the caller loses a die */
    CHECK(ld_script(&G, "5x4 l"));
    CHECK(P->phase == LP_REVEAL && P->caller == 1 && P->found == 5 && P->held && P->loser == 1 && P->ndice[1] == 4);
    CHECK(!ld_legal(P, LD_ACT(LA_BID, 6, 4)) && ld_script(&G, "n"));
    CHECK(P->phase == LP_BID && P->turn == 1 && P->round == 1 && P->count == 0 && P->nbids == 0);
    CHECK(P->dice[1][4] == 0 && ld_total(P) == 14);
    /* a lie: the bidder loses a die and starts */
    set_dice(0, "11234");
    set_dice(1, "4456");
    set_dice(2, "22222");
    CHECK(ld_script(&G, "6x6 l") && P->found == 3 && !P->held && P->loser == 1 && P->ndice[1] == 3);
    CHECK(ld_script(&G, "n") && P->turn == 1);
}

static void test_out(void)
{
    /* a player with no dice is out: skipped in the turn; their next player starts if they lost the last one */
    ld_new(&G, 3, 9);
    set_dice(0, "6");
    set_dice(1, "55");
    set_dice(2, "33");
    CHECK(ld_script(&G, "2x6 l"));                 /* one six (no ones): player 0 lied, has no die left */
    CHECK(P->loser == 0 && P->ndice[0] == 0 && P->phase == LP_REVEAL && ld_alive(P) == 2);
    CHECK(ld_script(&G, "n") && P->turn == 1 && P->starter == 1);
    CHECK(ld_script(&G, "1x5") && P->turn == 2);
    CHECK(ld_script(&G, "2x5") && P->turn == 1);   /* player 0 is skipped */
    /* the last player with dice wins */
    set_dice(1, "2");
    set_dice(2, "3");
    P->count = 0, P->turn = 1;
    CHECK(ld_script(&G, "2x2 l") && P->phase == LP_OVER && P->winner == 2 && P->ndice[1] == 0);
    CHECK(!ld_legal(P, LD_ACT(LA_NEXT, 0, 0)));
}

/* whole games from seeds, plain and random actions: the dice only ever go down by one a call, the game ends with
 * one player holding dice; undo replays */
static void test_random(void)
{
    int games = 0, ok = 1, calls = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        int players = 2 + (int)(seed % 5);
        ld_new(&G, players, seed);
        uint32_t rng = seed * 2654435761u;
        int steps = 0;
        while (P->phase != LP_OVER && steps < 3000) {
            int a = ld_auto(P);
            if (P->phase == LP_BID && cards_rand(&rng) % 3 == 0) {   /* a random legal action now and then */
                int c = 1 + (int)(cards_rand(&rng) % (uint32_t)ld_total(P)), f = 2 + (int)(cards_rand(&rng) % 5);
                a = P->count && cards_rand(&rng) % 4 == 0 ? LD_ACT(LA_LIAR, 0, 0) : LD_ACT(LA_BID, c, f);
                if (!ld_legal(P, a)) a = ld_auto(P);
            }
            int before = ld_total(P), kind = LD_KIND(a), found = P->count ? ld_counts(P, P->face) : 0;
            int bidder = P->bidder, caller = P->turn, count = P->count;
            if (!ld_play(&G, a)) { ok = 0; break; }
            if (kind == LA_LIAR) {
                calls++;
                ok &= ld_total(P) == before - 1 && P->found == found && P->held == (found >= count);
                ok &= P->loser == (found >= count ? caller : bidder);
            }
            ok &= P->ndice[P->turn] > 0 || P->phase != LP_BID;
            steps++;
        }
        games++;
        ok &= P->phase == LP_OVER && ld_alive(P) == 1 && P->ndice[P->winner] > 0;
        if (seed % 50 == 0) {
            H = G;
            for (int k = 0; k < 7; k++) ld_undo(&G);
            for (int i = G.n; i < H.n; i++) ld_play(&G, H.act[i]);
            ok &= !memcmp(&G.pos, &H.pos, sizeof G.pos);
        }
    }
    CHECK(ok);
    printf("  %d random games, %d calls\n", games, calls);
}

int main(void)
{
    test_new();
    test_bids();
    test_call();
    test_out();
    test_random();
    printf("liarsdice: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
