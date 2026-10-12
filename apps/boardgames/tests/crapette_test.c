/*
 * Crapette: the rules' test (the deal and who starts, building, loading, the talon and the waste turned over, the
 * priorities and the call, passing, winning, a blocked game, undo, the notation; whole random games from seeds). No
 * SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/crapette.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static cgame G, H;
static cpos *P = &G.pos;

static int count(const cpos *p, int *seen)
{
    int n = 0;
    memset(seen, 0, 256 * sizeof *seen);
    for (int q = 0; q < 2; q++) {
        for (int i = 0; i < p->ncrap[q]; i++) seen[p->crap[q][i]]++, n++;
        for (int i = 0; i < p->ntalon[q]; i++) seen[p->talon[q][i]]++, n++;
        for (int i = 0; i < p->nwaste[q]; i++) seen[p->waste[q][i]]++, n++;
    }
    for (int k = 0; k < CR_COLS; k++)
        for (int i = 0; i < p->ncol[k]; i++) seen[p->col[k][i]]++, n++;
    for (int f = 0; f < CR_FOUND; f++)
        if (p->found[f] != CARD_NONE) {
            seen[p->found[f]]++, n++;
            for (int r = 1; r < CARD_RANK(p->found[f]); r++) seen[CARD(CARD_SUIT(p->found[f]), r)]++, n++;
        }
    if (p->held != CARD_NONE) seen[p->held]++, n++;
    return n;
}

static int whole(const cpos *p)                 /* two of each card, 104 in all */
{
    int seen[256], n = count(p, seen), ok = n == 104;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) ok &= seen[CARD(s, r)] == 2;
    return ok;
}

static void test_deal(void)
{
    crap_new(&G, 7);
    CHECK(whole(P));
    CHECK(P->ncrap[0] == 13 && P->ncrap[1] == 13 && P->ntalon[0] == 35 && P->ntalon[1] == 35);
    for (int k = 0; k < CR_COLS; k++) CHECK(P->ncol[k] == 1);
    CHECK(P->held == CARD_NONE && P->phase == CP_PLAY && P->fault == CF_NONE);
    int d = CARD_RANK(P->crap[1][12]) - CARD_RANK(P->crap[0][12]);
    for (int k = 0; !d && k < 4; k++) d = CARD_RANK(P->col[4 + k][0]) - CARD_RANK(P->col[k][0]);
    CHECK(P->turn == (d > 0));
    crap_new(&H, 7);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    int firsts[2] = {0, 0};
    for (uint32_t s = 1; s < 60; s++) {
        crap_new(&H, s);
        firsts[H.pos.turn]++;
    }
    CHECK(firsts[0] > 5 && firsts[1] > 5);
}

/* an empty table: no foundations, empty houses; player `me` to move; crapettes and talons given (top last) */
static void bare(int me)
{
    crap_new(&G, 1);
    memset(P->ncol, 0, sizeof P->ncol);
    for (int f = 0; f < CR_FOUND; f++) P->found[f] = CARD_NONE;
    P->ncrap[0] = P->ncrap[1] = P->ntalon[0] = P->ntalon[1] = P->nwaste[0] = P->nwaste[1] = 0;
    P->turn = (uint8_t)me;
}

static void col(int k, int c) { P->col[k][P->ncol[k]++] = (uint8_t)c; }
static void crap(int q, int c) { P->crap[q][P->ncrap[q]++] = (uint8_t)c; }
static void talon(int q, int c) { P->talon[q][P->ntalon[q]++] = (uint8_t)c; }
static void waste(int q, int c) { P->waste[q][P->nwaste[q]++] = (uint8_t)c; }

static void test_building(void)
{
    bare(0);
    crap(0, CARD(SUIT_SPADES, 9));
    crap(1, CARD(SUIT_HEARTS, 5));
    waste(1, CARD(SUIT_CLUBS, 4));
    talon(0, CARD(SUIT_CLUBS, 2));
    col(0, CARD(SUIT_HEARTS, 10));
    col(1, CARD(SUIT_SPADES, 10));
    col(2, CARD(SUIT_HEARTS, RANK_ACE));
    col(3, CARD(SUIT_HEARTS, 6));
    int nine = CARD(SUIT_SPADES, 9);
    CHECK(crap_fits(P, nine, 0) && !crap_fits(P, nine, 1));      /* down in alternating colours */
    CHECK(crap_fits(P, nine, 4));                                 /* an empty house takes anything */
    CHECK(!crap_fits(P, nine, CL_FOUND));                         /* a foundation starts with an ace */
    CHECK(crap_fits(P, CARD(SUIT_HEARTS, RANK_ACE), CL_FOUND + 3));
    CHECK(crap_fits(P, CARD(SUIT_HEARTS, 4), CL_OCRAP) && crap_fits(P, CARD(SUIT_HEARTS, 6), CL_OCRAP));
    CHECK(!crap_fits(P, CARD(SUIT_DIAMONDS, 6), CL_OCRAP) && !crap_fits(P, CARD(SUIT_HEARTS, 7), CL_OCRAP));
    CHECK(crap_fits(P, CARD(SUIT_CLUBS, 3), CL_OWASTE) && crap_fits(P, CARD(SUIT_CLUBS, 5), CL_OWASTE));
    /* the ace must go first: anything else is a fault (the crapette is not the one) */
    CHECK(crap_fault_of(P, CA_MOVEOF(CL_CRAP, 0)) == CF_FOUND);
    CHECK(crap_fault_of(P, CA_MOVEOF(2, CL_FOUND + 5)) == CF_NONE);
    CHECK(crap_play(&G, CA_MOVEOF(2, CL_FOUND + 5)) && P->found[5] == CARD(SUIT_HEARTS, RANK_ACE) && !P->ncol[2]);
    CHECK(!crap_legal(P, CA_MOVEOF(CL_FOUND + 5, 2)));            /* nothing leaves a foundation */
    CHECK(!crap_legal(P, CA_MOVEOF(CL_OCRAP, 2)) && !crap_legal(P, CA_MOVEOF(CL_OWASTE, 2)));
    CHECK(!crap_legal(P, CA_MOVEOF(0, CL_CRAP)));                 /* nor goes onto your own crapette */
    /* the crapette (it can go on the ten of hearts) must be played before the talon is turned */
    CHECK(crap_fault_of(P, CA_TURN) == CF_CRAP_FIRST);
    CHECK(crap_fault_of(P, CA_MOVEOF(3, 2)) == CF_NONE);          /* moves between houses are free */
    CHECK(crap_play(&G, CA_MOVEOF(CL_CRAP, 0)) && P->ncrap[0] == 0 && P->ncol[0] == 2);
    CHECK(crap_fault_of(P, CA_TURN) == CF_NONE);
    CHECK(crap_play(&G, CA_TURN) && P->held == CARD(SUIT_CLUBS, 2) && !P->ntalon[0]);
    CHECK(!crap_legal(P, CA_TURN) && !crap_legal(P, CA_PASS));
    CHECK(crap_play(&G, CA_DISCARD) && P->turn == 1 && P->nwaste[0] == 1);
}

static void test_talon(void)
{
    bare(0);
    crap(0, CARD(SUIT_SPADES, RANK_KING));
    crap(1, CARD(SUIT_SPADES, RANK_KING));
    waste(0, CARD(SUIT_CLUBS, 7));
    waste(0, CARD(SUIT_CLUBS, 8));
    waste(0, CARD(SUIT_CLUBS, 9));
    /* the talon used up: the waste turned over, its first card on top */
    CHECK(crap_play(&G, CA_TURN) && P->held == CARD(SUIT_CLUBS, 7) && P->ntalon[0] == 2 && !P->nwaste[0]);
    CHECK(P->talon[0][1] == CARD(SUIT_CLUBS, 8));
    CHECK(crap_play(&G, CA_DISCARD) && P->turn == 1);
    /* nothing left to turn: a pass */
    CHECK(!crap_legal(P, CA_TURN) && crap_legal(P, CA_PASS));
    CHECK(crap_play(&G, CA_PASS) && P->turn == 0);
}

static void test_call(void)
{
    bare(0);
    crap(0, CARD(SUIT_DIAMONDS, 9));
    crap(0, CARD(SUIT_SPADES, RANK_ACE));
    crap(1, CARD(SUIT_HEARTS, RANK_KING));
    talon(0, CARD(SUIT_CLUBS, 4));
    talon(0, CARD(SUIT_CLUBS, 3));
    col(0, CARD(SUIT_HEARTS, 5));
    col(1, CARD(SUIT_SPADES, 6));
    col(4, CARD(SUIT_CLUBS, RANK_ACE));
    /* the crapette's ace goes first, even before the ace of the houses */
    CHECK(crap_fault_of(P, CA_MOVEOF(4, CL_FOUND)) == CF_CRAP_FOUND);
    CHECK(crap_fault_of(P, CA_MOVEOF(CL_CRAP, CL_FOUND)) == CF_NONE);
    CHECK(!crap_legal(P, CA_CALL));
    cpos before = *P;
    CHECK(crap_play(&G, CA_MOVEOF(0, 1)));                         /* a fault: the hearts five onto the spades six */
    CHECK(P->fault == CF_CRAP_FOUND && P->fault_card == CARD(SUIT_SPADES, RANK_ACE) && P->turn == 0);
    CHECK(crap_play(&G, CA_MOVEOF(CL_CRAP, CL_FOUND)));            /* playing on: still callable */
    CHECK(crap_play(&G, CA_TURN) && P->held == CARD(SUIT_CLUBS, 3) && P->fault == CF_CRAP_FOUND);
    CHECK(crap_legal(P, CA_CALL));
    CHECK(crap_play(&G, CA_CALL));                                 /* "Crapette!" */
    CHECK(P->turn == 1 && P->fault == CF_NONE && P->held == CARD_NONE);
    CHECK(!memcmp(P->col, before.col, sizeof P->col) && !memcmp(P->ncol, before.ncol, sizeof P->ncol));
    CHECK(P->ncrap[0] == 2 && P->ntalon[0] == 2 && P->nwaste[0] == 0);
    /* the talon card held at the fault goes to the waste */
    bare(0);                                    /* (no crapette: it would have to go to an empty house first) */
    crap(1, CARD(SUIT_HEARTS, RANK_KING));
    talon(0, CARD(SUIT_CLUBS, 9));
    talon(0, CARD(SUIT_DIAMONDS, 2));
    col(0, CARD(SUIT_HEARTS, 10));
    col(1, CARD(SUIT_DIAMONDS, RANK_ACE));
    CHECK(crap_play(&G, CA_MOVEOF(1, CL_FOUND)) && crap_play(&G, CA_TURN) && P->held == CARD(SUIT_DIAMONDS, 2));
    CHECK(crap_fault_of(P, CA_MOVEOF(CL_HELD, 2)) == CF_FOUND);
    CHECK(crap_play(&G, CA_MOVEOF(CL_HELD, 2)) && P->fault == CF_FOUND);
    CHECK(crap_play(&G, CA_CALL) && P->turn == 1 && P->nwaste[0] == 1 && P->waste[0][0] == CARD(SUIT_DIAMONDS, 2));
    CHECK(P->ncol[2] == 0 && P->found[0] == CARD(SUIT_DIAMONDS, RANK_ACE));
    /* a fault ends with the turn */
    bare(0);
    crap(0, CARD(SUIT_HEARTS, RANK_KING));
    crap(1, CARD(SUIT_HEARTS, RANK_KING));
    talon(0, CARD(SUIT_CLUBS, 9));
    col(0, CARD(SUIT_HEARTS, RANK_ACE));
    CHECK(crap_play(&G, CA_TURN) && P->fault == CF_FOUND && crap_play(&G, CA_DISCARD));
    CHECK(P->turn == 1 && P->fault == CF_NONE && !crap_legal(P, CA_CALL));
    /* undo replays the call too */
    crap_new(&G, 99);
    CHECK(crap_script(&G, "...."));
    int n = G.n;
    CHECK(crap_undo(&G) && G.n == n - 1);
}

static void test_win(void)
{
    bare(0);
    crap(0, CARD(SUIT_HEARTS, 2));
    crap(1, CARD(SUIT_CLUBS, 9));
    talon(1, CARD(SUIT_CLUBS, 3));
    P->found[0] = CARD(SUIT_HEARTS, RANK_ACE);
    CHECK(crap_play(&G, CA_MOVEOF(CL_CRAP, CL_FOUND)));
    CHECK(P->phase == CP_OVER && P->winner == 0);
    CHECK(!crap_legal(P, CA_TURN));
    /* blocked: nothing can move; the fewer cards left wins */
    bare(0);
    crap(0, CARD(SUIT_HEARTS, 9));
    crap(1, CARD(SUIT_CLUBS, 9));
    crap(1, CARD(SUIT_CLUBS, 7));
    talon(0, CARD(SUIT_SPADES, 5));
    talon(1, CARD(SUIT_SPADES, 5));
    for (int k = 0; k < CR_COLS; k++) col(k, CARD(SUIT_DIAMONDS, RANK_KING));
    int steps = 0;
    for (; P->phase == CP_PLAY && steps < 200; steps++) CHECK(crap_play(&G, crap_auto(P)));
    CHECK(P->phase == CP_OVER && P->blocked && P->winner == 0);
}

static void test_script(void)
{
    crap_new(&G, 5);
    CHECK(crap_script(&G, "#"));
    CHECK(P->phase == CP_OVER || G.n >= CR_MAX_ACTS - 1);
    CHECK(whole(P));
    crap_new(&G, 5);
    CHECK(!crap_script(&G, "c-c"));
    CHECK(!crap_script(&G, "x"));
}

/* whole games: random legal actions (often a call when one is open), every card kept, undo = replay */
static void test_random(void)
{
    int over = 0, calls = 0, blocked = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        crap_new(&G, seed);
        uint32_t r = seed * 7919u + 1;
        int acts[600], ok = 1;
        for (int step = 0; step < 6000 && P->phase == CP_PLAY; step++) {
            int n = crap_moves(P, acts);
            if (!n) { ok = 0; break; }
            r = r * 1103515245u + 12345u;
            int pick = acts[(r >> 8) % (unsigned)n];
            if (crap_legal(P, CA_CALL) && (r >> 20) % 3 == 0) pick = CA_CALL;
            else if ((r >> 24) % 4 && crap_auto(P)) pick = crap_auto(P);   /* mostly plain play: games end */
            if (pick == CA_CALL) calls++;
            if (!crap_play(&G, pick)) { ok = 0; break; }
            if (!whole(P)) { ok = 0; break; }
        }
        CHECK(ok);
        over += P->phase == CP_OVER;
        blocked += P->blocked;
        if (seed % 30 == 0) {                   /* undo goes back exactly */
            cpos end = *P;
            int last = G.act[G.n - 1];
            CHECK(crap_undo(&G) && crap_play(&G, last));
            CHECK(!memcmp(&end, P, sizeof end));
        }
    }
    CHECK(over > 250);
    CHECK(calls > 10);
    printf("random games: %d over (%d blocked), %d calls\n", over, blocked, calls);
}

int main(void)
{
    test_deal();
    test_building();
    test_talon();
    test_call();
    test_win();
    test_script();
    test_random();
    printf("crapette: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
