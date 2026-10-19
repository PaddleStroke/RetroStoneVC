/*
 * Bataille corse: the rules' test (the deal, turns, tribute paid and unpaid, doubles and sandwiches, wrong slaps and
 * their penalty, coming back with a slap, out for good, the end, undo, the notation; whole random games from seeds).
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/slap.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static slgame G, H;
static slpos *P = &G.pos;

static int total(const slpos *p)
{
    int n = p->nc;
    for (int q = 0; q < p->players; q++) n += p->n[q];
    return n;
}

static int whole(const slpos *p)
{
    int seen[256] = {0};
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->pile[q][i]]++;
    for (int i = 0; i < p->nc; i++) seen[p->centre[i]]++;
    int ok = total(p) == 52;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) ok &= seen[CARD(s, r)] == 1;
    return ok;
}

/* piles set by hand (each listed bottom first, its top last): ranks only, the suits going round */
static void set(int players, int variant, const char *const *piles)
{
    slap_new(&G, variant, players, 1);
    for (int q = 0; q < players; q++) {
        P->n[q] = 0;
        for (const char *s = piles[q]; *s; s++) {
            static const char ranks[] = "A23456789TJQK";
            const char *r = strchr(ranks, *s);
            int i = P->n[q];
            if (r) P->pile[q][i] = (uint8_t)CARD(i % 4, (int)(r - ranks) + 1), P->n[q]++;
        }
    }
}

static void test_deal(void)
{
    slap_new(&G, SLAP_DOUBLE, 3, 9);
    CHECK(P->n[0] == 18 && P->n[1] == 17 && P->n[2] == 17 && whole(P) && P->turn == 0 && P->phase == SLP_PLAY);
    slap_new(&H, SLAP_DOUBLE, 3, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    slap_new(&G, SLAP_DOUBLE, 4, 9);
    CHECK(P->n[0] == 13 && P->n[3] == 13 && whole(P));
}

static void test_tribute(void)
{
    static const char *const a[2] = {"55K", "6234"};        /* player 0 turns a king: player 1 owes 3 tries */
    set(2, SLAP_DOUBLE, a);
    CHECK(slap_script(&G, "p") && P->turn == 1 && P->owed == 3 && P->claimer == 0);
    CHECK(slap_script(&G, "p p") && P->owed == 1 && P->turn == 1);
    CHECK(!slap_legal(P, SLA_CLAIM));
    CHECK(slap_script(&G, "p") && P->phase == SLP_CLAIM && P->turn == 0);
    CHECK(!slap_legal(P, SLA_PLAY));
    CHECK(slap_script(&G, "c") && P->phase == SLP_PLAY && P->turn == 0 && P->n[0] == 6 && P->nc == 0);
    CHECK(P->pile[0][0] == CARD(1, 2));          /* the centre turned over under the pile: its last card lowest */
    /* a face card in the tribute passes the debt on */
    static const char *const b[2] = {"55J", "7Q"};
    set(2, SLAP_DOUBLE, b);
    CHECK(slap_script(&G, "p p") && P->turn == 0 && P->owed == 2 && P->claimer == 1);
    /* the payer runs out: the pile to the player of the face card */
    static const char *const c[2] = {"5A", "9"};
    set(2, SLAP_DOUBLE, c);
    CHECK(slap_script(&G, "p p") && P->phase == SLP_CLAIM && P->turn == 0);
    CHECK(slap_script(&G, "c") && P->phase == SLP_OVER && P->winner == 0);
}

static void test_slaps(void)
{
    static const char *const a[2] = {"K77", "3557"};
    set(2, SLAP_DOUBLE, a);
    CHECK(slap_script(&G, "p p") && slap_right(P));        /* a seven, a seven: a double */
    CHECK(P->nc == 2);
    set(2, SLAP_DOUBLE, a);
    CHECK(slap_script(&G, "p"));
    CHECK(!slap_right(P) && slap_script(&G, "s1"));         /* a wrong slap: player 1's top card under the pile */
    CHECK(P->nc == 2 && P->centre[0] == CARD(3, 7) && P->n[1] == 3 && P->event == SLE_PENALTY);
    CHECK(P->buried == 1 && !slap_right(P));    /* a seven face down under a seven: no double */
    static const char *const b[2] = {"K5", "Q5"};
    set(2, SLAP_DOUBLE, b);
    CHECK(slap_script(&G, "p p") && slap_right(P));         /* 5, 5: a double */
    CHECK(slap_script(&G, "s0") && P->n[0] == 3 && P->nc == 0 && P->turn == 0 && P->event == SLE_TAKE);
    /* a sandwich counts only in its variant */
    static const char *const c[2] = {"K55", "Q8"};
    set(2, SLAP_DOUBLE, c);
    CHECK(slap_script(&G, "p p p") && !slap_right(P));
    set(2, SLAP_SANDWICH, c);
    CHECK(slap_script(&G, "p p p") && slap_right(P) && slap_script(&G, "s1") && P->n[1] == 4);
    /* a slap during a tribute wins it too */
    static const char *const d[2] = {"5J", "3J"};
    set(2, SLAP_DOUBLE, d);
    CHECK(slap_script(&G, "p p") && slap_right(P) && slap_script(&G, "s0") && P->owed == 0 && P->n[0] == 3);
    /* empty-handed: still slapping, back in with a double; out for good with a wrong slap */
    static const char *const e[2] = {"9", "8559"};
    set(2, SLAP_DOUBLE, e);
    CHECK(slap_script(&G, "p p") && slap_right(P) && slap_script(&G, "s0") && P->n[0] == 0 + 2 && P->turn == 0);
    static const char *const f[2] = {"9", "8642"};
    set(2, SLAP_DOUBLE, f);
    CHECK(slap_script(&G, "p p") && P->n[0] == 0 && P->turn == 1);
    CHECK(slap_script(&G, "s0") && P->phase == SLP_OVER && P->winner == 1 && P->event == SLE_OUT);
    CHECK(!slap_legal(P, SLA_SLAP | 1) && !slap_legal(P, SLA_PLAY));
}

/* touches, as the fingers come (the app's finger() and press() call slap_touch then play what it says) */
static int tap(int q, int on_pile)
{
    int a = slap_touch(P, q, on_pile);
    return a && slap_play(&G, a) ? a : 0;
}

static void test_touches(void)
{
    static const char *const a[4] = {"K5", "Q5", "J3", "74"};
    set(4, SLAP_DOUBLE, a);
    CHECK(slap_touch(P, 1, 1) == 0);            /* not your turn, nothing in the middle: nothing */
    CHECK(slap_touch(P, 0, 0) == 0);            /* your turn but not on your pile, nothing to slap */
    CHECK(tap(0, 1) == SLA_PLAY && tap(1, 1) == SLA_PLAY);   /* 5, 5: a double */
    /* three fingers come down on it: the first one (player 3) wins, the others are too late and cost nothing */
    CHECK(tap(3, 0) == (SLA_SLAP | 3) && P->n[3] == 4 && P->turn == 3);
    CHECK(tap(2, 0) == 0 && tap(0, 0) == 0 && P->n[2] == 2 && P->n[0] == 1);
    /* a finger on your own pile out of turn slaps */
    CHECK(tap(3, 1) == SLA_PLAY && tap(2, 1) == (SLA_SLAP | 2) && P->event == SLE_PENALTY && P->n[2] == 1);
    /* an unpaid tribute: the face card's player's touch takes the pile at once */
    static const char *const b[2] = {"5K", "823"};
    set(2, SLAP_DOUBLE, b);
    CHECK(tap(0, 1) && tap(1, 1) && tap(1, 1) && P->phase == SLP_PLAY && tap(1, 1));
    CHECK(P->phase == SLP_CLAIM && P->turn == 0 && tap(0, 0) == SLA_CLAIM && P->n[0] == 5);
    CHECK(slap_touch(P, 5, 0) == 0 && slap_touch(P, -1, 0) == 0);
}

static void test_random(void)
{
    int over = 0, takes = 0, pens = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int players = 2 + seed % 3;
        slap_new(&G, seed % 2, players, seed);
        uint32_t r = seed * 2246822519u + 7;
        int ok = 1;
        for (int step = 0; step < 30000 && P->phase != SLP_OVER; step++) {
            r = r * 1103515245u + 12345u;
            int a = slap_auto(P);
            if ((r >> 16) % 40 == 0) a = SLA_SLAP | (int)((r >> 8) % (unsigned)players);   /* a slap at random */
            if (!slap_legal(P, a)) a = slap_auto(P);
            if (!slap_play(&G, a)) { ok = 0; break; }
            takes += P->event == SLE_TAKE && P->how == 1;
            pens += P->event == SLE_PENALTY;
            if (!whole(P)) { ok = 0; break; }
        }
        CHECK(ok);
        over += P->phase == SLP_OVER;
        if (P->phase == SLP_OVER) CHECK(P->winner < players);
        if (seed % 25 == 0 && G.n) {
            slpos end = *P;
            int last = G.act[G.n - 1];
            CHECK(slap_undo(&G) && slap_play(&G, last) && !memcmp(&end, P, sizeof end));
        }
    }
    CHECK(over > 250 && takes > 100 && pens > 100);
    printf("random games: %d over, %d slaps won, %d penalties\n", over, takes, pens);
    slap_new(&G, SLAP_DOUBLE, 2, 3);
    CHECK(slap_script(&G, "#") && whole(P));
    CHECK(!slap_script(&G, "q"));
    slap_new(&G, SLAP_DOUBLE, 2, 3);           /* a bot's moves: the actions it saw after each */
    CHECK(slap_script(&G, "p/0 p/1") && G.n == 2 && !slap_script(&G, "p/") && !slap_script(&G, "px"));
}

int main(void)
{
    test_deal();
    test_tribute();
    test_slaps();
    test_touches();
    test_random();
    printf("slap: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
