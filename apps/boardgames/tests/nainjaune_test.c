/*
 * Le Nain jaune: the rules' test (src/games/nainjaune_rules.c). The deal and the talon for 3 to 6 players, the stakes,
 * sequences by rank (the player themself first, then round the table), the king and the blocked sequence giving the
 * lead back, the five cards taking their compartments, going out (a chip a card, the stakes again for the five cards
 * held), grand opera, the next deal, the end of the game, undo; whole games from seeds. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/nainjaune.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define S(r) CARD(SUIT_SPADES, r)
#define H(r) CARD(SUIT_HEARTS, r)
#define D(r) CARD(SUIT_DIAMONDS, r)
#define C(r) CARD(SUIT_CLUBS, r)

/* a crafted deal: the hands given (0-terminated lists), player `turn` to lead, nothing played yet */
static void craft(njgame *g, int variant, int players, const int *const *hands, int turn)
{
    nj_new(g, variant, players, 1);
    njpos *p = &g->pos;
    for (int q = 0; q < players; q++) {
        p->n[q] = 0;
        for (const int *c = hands[q]; *c; c++) p->hand[q][p->n[q]++] = (uint8_t)*c;
    }
    p->turn = p->lead = p->last = (uint8_t)turn;
    p->need = 0;
    p->nplayed = 0;
    p->played_by = 0;
}

static int chips_total(const njpos *p)
{
    int t = 0;
    for (int q = 0; q < p->players; q++) t += p->chips[q];
    for (int k = 0; k < NJ_BOXES; k++) t += p->box[k];
    return t;
}

/* every card once: the hands, the talon, the cards played */
static int all_cards(const njpos *p)
{
    int seen[256] = {0}, n = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++, n++;
    for (int i = 0; i < p->ntalon; i++) seen[p->talon[i]]++, n++;
    for (int i = 0; i < p->nplayed; i++) seen[p->played[i]]++, n++;
    for (int i = 0; i < 256; i++)
        if (seen[i] > 1) return 0;
    return n == 52;
}

int main(void)
{
    static njgame g;
    njpos *p = &g.pos;

    /* the five cards and their compartments */
    CHECK(nj_box_of(D(10)) == NJ_TEN && nj_box_of(C(11)) == NJ_JACK && nj_box_of(S(12)) == NJ_QUEEN);
    CHECK(nj_box_of(H(13)) == NJ_KING && nj_box_of(D(7)) == NJ_SEVEN && nj_box_of(H(7)) < 0 && nj_box_of(D(11)) < 0);

    /* the deal: the cards each, the talon, the stakes; player 0 leads the first deal */
    static const int each[7] = {0, 0, 0, 15, 12, 9, 8}, talon[7] = {0, 0, 0, 7, 4, 7, 4};
    for (int np = 3; np <= 6; np++) {
        nj_new(&g, NJ_CLASSIC, np, 42);
        int ok = p->players == np && p->ntalon == talon[np] && all_cards(p) && p->turn == 0 && p->dealer == np - 1;
        for (int q = 0; q < np; q++) ok &= p->n[q] == each[np] && p->chips[q] == NJ_CHIPS - NJ_STAKES;
        for (int k = 0; k < NJ_BOXES; k++) ok &= p->box[k] == np * (k + 1);
        CHECK(ok && p->need == 0 && p->phase == NP_PLAY && p->deals == 1 && chips_total(p) == np * NJ_CHIPS);
    }
    nj_new(&g, NJ_CLASSIC, 2, 1);
    CHECK(p->players == 3);
    nj_new(&g, NJ_CLASSIC, 8, 1);
    CHECK(p->players == 6);

    /* a sequence: on with the next rank while you hold it; then the next player round the table who holds it */
    const int h0[] = {S(1), S(2), S(3), S(5), 0}, h1[] = {H(4), H(9), 0}, h2[] = {D(6), D(7), C(13), 0};
    const int *const hands[3] = {h0, h1, h2};
    craft(&g, NJ_CLASSIC, 3, hands, 0);
    CHECK(!nj_legal(p, H(4)) && nj_legal(p, S(5)) && !nj_legal(p, NJ_NEXT));    /* any card to lead, but its own */
    CHECK(nj_play(&g, S(1)) && p->need == 2 && p->turn == 0 && !nj_play(&g, S(5)));
    CHECK(nj_play(&g, S(2)) && nj_play(&g, S(3)) && p->need == 4 && p->turn == 1);   /* without four: player 1 */
    CHECK(!nj_play(&g, H(9)) && nj_play(&g, H(4)) && p->need == 5 && p->turn == 0);  /* 2 has no five: 0 has */
    int c1 = p->chips[1], c2 = p->chips[2], c0 = p->chips[0], seven = p->box[NJ_SEVEN];
    CHECK(nj_play(&g, S(5)) && p->phase == NP_DEALT && p->out == 0);   /* out: a chip a card, the dwarf's 5 again */
    CHECK(p->chips[1] == c1 - 1 && p->chips[2] == c2 - 3 - 5 && p->chips[0] == c0 + 4);
    CHECK(p->box[NJ_SEVEN] == seven + 5 && p->got_cards[0] == 4 && p->got_cards[2] == -3 && p->paid_again[2] == -5);
    CHECK(!p->opera && !nj_play(&g, S(1)) && nj_legal(p, NJ_NEXT));

    /* the five cards take their compartments; a king ends the sequence, its player leads again; grand opera: out
     * before anyone else played, every compartment too */
    const int k0[] = {S(12), H(13), C(3), 0}, k1[] = {H(4), D(7), 0}, k2[] = {D(10), C(11), 0};
    const int *const khands[3] = {k0, k1, k2};
    craft(&g, NJ_CLASSIC, 3, khands, 0);
    int q0 = p->chips[0], boxes = 0;
    for (int k = 0; k < NJ_BOXES; k++) boxes += p->box[k];
    int queen = p->box[NJ_QUEEN], king = p->box[NJ_KING];
    CHECK(nj_play(&g, S(12)) && p->box[NJ_QUEEN] == 0 && p->chips[0] == q0 + queen && p->turn == 0 && p->need == 13);
    CHECK(nj_play(&g, H(13)) && p->box[NJ_KING] == 0 && p->need == 0 && p->turn == 0 && p->lead == 0);
    CHECK(nj_play(&g, C(3)) && p->phase == NP_DEALT && p->opera);
    /* the others pay: 2 cards each, and the stakes again: the dwarf (5) from 1; the ten (1) and the jack (2) from 2 */
    CHECK(p->chips[0] == q0 + boxes + 4 + 5 + 3 && p->box[NJ_SEVEN] == 0 && p->box[NJ_TEN] == 0);
    craft(&g, NJ_NO_OPERA, 3, khands, 0);
    CHECK(nj_script(&g, "qs kh 3c") && !p->opera && p->box[NJ_SEVEN] == 3 * 5 + 5 && p->box[NJ_TEN] == 3 + 1);
    CHECK(p->box[NJ_QUEEN] == 0 && p->box[NJ_KING] == 0 && p->chips[0] == q0 + queen + king + 4);

    /* a blocked sequence: nobody holds the next rank, the last to play leads again */
    const int b0[] = {S(5), C(9), 0}, b1[] = {H(2), 0}, b2[] = {D(3), D(8), 0};
    const int *const bhands[3] = {b0, b1, b2};
    craft(&g, NJ_CLASSIC, 3, bhands, 0);
    CHECK(nj_play(&g, S(5)) && p->need == 0 && p->turn == 0 && p->lead == 0);
    CHECK(nj_play(&g, C(9)) && p->phase == NP_DEALT);
    /* passed over: 1 has no nine, 2 has */
    craft(&g, NJ_CLASSIC, 3, (const int *const[]){(const int[]){S(8), S(1), 0}, (const int[]){H(2), 0},
                                                  (const int[]){D(9), D(1), 0}}, 0);
    CHECK(nj_play(&g, S(8)) && p->turn == 2 && p->need == 9 && nj_play(&g, D(9)) && p->turn == 2 && p->need == 0);
    /* a player who cannot pay pays what they have */
    craft(&g, NJ_CLASSIC, 3, (const int *const[]){(const int[]){S(4), 0}, (const int[]){H(2), H(3), H(5), 0},
                                                  (const int[]){D(9), 0}}, 0);
    p->chips[1] = 2;
    CHECK(nj_play(&g, S(4)) && p->chips[1] == 0 && p->got_cards[1] == -2 && p->got_cards[0] == 3);

    /* the next deal: the dealer moves on, the stakes again, the compartments not taken stay */
    nj_new(&g, NJ_CLASSIC, 4, 9);
    CHECK(nj_script(&g, "#") && p->phase == NP_DEALT && p->deals == 1);
    int left[NJ_BOXES], chips[NJ_MAXP];
    for (int k = 0; k < NJ_BOXES; k++) left[k] = p->box[k];
    for (int q = 0; q < 4; q++) chips[q] = p->chips[q];
    CHECK(nj_script(&g, "n") && p->phase == NP_PLAY && p->deals == 2 && p->dealer == 0 && p->turn == 1);
    int ok = all_cards(p);
    for (int k = 0; k < NJ_BOXES; k++) ok &= p->box[k] == left[k] + 4 * (k + 1);
    for (int q = 0; q < 4; q++) ok &= p->chips[q] == chips[q] - NJ_STAKES;
    CHECK(ok);

    /* undo: within one player's turn only (the device passed: no going back) */
    nj_new(&g, NJ_CLASSIC, 3, 5);
    int kept = 0, passed = 0;
    for (int i = 0; i < 400 && p->phase == NP_PLAY && !(kept && passed); i++) {
        njpos before = *p;
        int who = p->turn;
        nj_play(&g, nj_auto(p));
        if (p->turn == who && p->phase == NP_PLAY) {   /* the player goes on: undo takes it back */
            kept = 1;
            CHECK(nj_undo(&g) && memcmp(&before, p, sizeof before) == 0);
            nj_play(&g, nj_auto(p));
        } else {
            passed = 1;
            CHECK(!nj_undo(&g));
        }
    }
    CHECK(kept && passed);

    /* the end: every player has dealt twice; or a player who cannot stake */
    nj_new(&g, NJ_CLASSIC, 3, 77);
    while (nj_script(&g, "#") && nj_script(&g, "n") && p->phase != NP_OVER) {}
    int broke = 0;
    for (int q = 0; q < 3; q++) broke |= p->chips[q] < NJ_STAKES;
    CHECK(p->phase == NP_OVER && (p->deals == 6 || broke) && nj_winners(p) != 0 && !nj_legal(p, NJ_NEXT));
    nj_new(&g, NJ_CLASSIC, 3, 77);
    CHECK(nj_script(&g, "#"));
    p->chips[2] = NJ_STAKES - 1;
    CHECK(nj_script(&g, "n") && p->phase == NP_OVER);

    /* whole games from seeds: the chips are kept (players' and compartments'), the cards too, it ends */
    ok = 1;
    int deals_max = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int np = 3 + (int)(seed % 4), v = (int)(seed / 4 % 2);
        nj_new(&g, v, np, seed);
        int guard = 0;
        while (p->phase != NP_OVER && guard++ < 5000) {
            int a = nj_auto(p);
            if (p->phase == NP_PLAY && (seed & 1)) {   /* some games: a random legal card, not the lowest */
                int pick[NJ_HAND], k = 0, q = p->turn;
                for (int i = 0; i < p->n[q]; i++)
                    if (nj_legal(p, p->hand[q][i])) pick[k++] = p->hand[q][i];
                if (k) a = pick[(seed * 31u + (unsigned)guard) % (unsigned)k];
            }
            ok &= nj_play(&g, a);
            ok &= chips_total(p) == np * NJ_CHIPS;
            if (p->phase == NP_PLAY) ok &= all_cards(p) && (p->need == 0 || nj_auto(p) >= 0);
            for (int q = 0; q < np; q++) ok &= p->chips[q] >= 0;
        }
        ok &= p->phase == NP_OVER && p->deals <= nj_deals(np) && nj_winners(p) != 0;
        if (p->deals > deals_max) deals_max = p->deals;
    }
    CHECK(ok && deals_max == 12);

    printf("nainjaune: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
