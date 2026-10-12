/*
 * Piquet: the rules' test (the deal, the exchange's limits, the declarations: point, sequences, sets and their
 * comparison, carte blanche, repique, pique, the points of the play, the cards and capot, the Rubicon, undo, the
 * notation; whole random parties from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/piquet.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static piquet_game G, H;
static piquet_pos *P = &G.pos;

static int parse_hand(const char *s, uint8_t *h)
{
    int n = 0;
    while (*s) {
        while (*s == ' ') s++;
        int c = trick_parse(&s);
        if (c == CARD_NONE) break;
        h[n++] = (uint8_t)c;
    }
    trick_sort(h, n);
    return n;
}

/* both hands set by hand, the declarations about to be made (player 0 the elder) */
static void set_hands(const char *h0, const char *h1)
{
    piquet_new(&G, 1);
    P->n[0] = (uint8_t)parse_hand(h0, P->hand[0]);
    P->n[1] = (uint8_t)parse_hand(h1, P->hand[1]);
    P->blanche[0] = (uint8_t)piquet_blanche(P->hand[0], P->n[0]);
    P->blanche[1] = (uint8_t)piquet_blanche(P->hand[1], P->n[1]);
    P->phase = PQ_DECLARE;
    P->turn = 0;
    G.n = 0;
}

static void test_deal(void)
{
    for (uint32_t seed = 1; seed <= 30; seed++) {
        piquet_new(&G, seed);
        int seen[256] = {0}, ok = 1, total = 0;
        for (int pl = 0; pl < 2; pl++)
            for (int i = 0; i < 12; i++) seen[P->hand[pl][i]]++;
        for (int i = 0; i < 8; i++) seen[P->talon[i]]++;
        for (int c = 0; c < 256; c++) ok &= seen[c] <= 1, total += seen[c];
        CHECK(ok && total == 32 && P->elder == 0 && P->dealer == 1 && P->turn == 0 && P->phase == PQ_EXCHANGE);
    }
    piquet_new(&G, 2);
    piquet_new(&H, 2);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

static void test_exchange(void)
{
    piquet_new(&G, 4);
    CHECK(piquet_throw_min(P) == 1 && piquet_throw_max(P) == 5);
    CHECK(!piquet_legal(P, PQA_EXCHANGE) && !piquet_legal(P, PQA_EXCHANGE | 0x3f));
    CHECK(piquet_legal(P, PQA_EXCHANGE | 0x1f));
    uint8_t t0 = P->talon[0], t1 = P->talon[1];
    CHECK(piquet_play(&G, PQA_EXCHANGE | 0x3));   /* the elder takes two: the talon's first two */
    CHECK(trick_find(P->hand[0], 12, t0) >= 0 && trick_find(P->hand[0], 12, t1) >= 0 && P->ex[0] == 2);
    CHECK(P->turn == 1 && piquet_throw_min(P) == 0 && piquet_throw_max(P) == 6);
    CHECK(piquet_play(&G, PQA_EXCHANGE));       /* the younger keeps all */
    CHECK(P->phase == PQ_DECLARE && P->turn == 0 && P->top == 2);
    CHECK(piquet_script(&G, "k") && P->turn == 1 && P->phase == PQ_DECLARE);
    CHECK(piquet_script(&G, "k") && P->phase == PQ_PLAY && P->turn == 0);
}

static void test_declarations(void)
{
    piquet_decl d;
    uint8_t h[12];
    int n = parse_hand("As Ks Qs Js Ts 7h 8h 9h Ah Ad Ac 7c", h);
    piquet_declare(h, n, &d);
    CHECK(d.pn == 5 && d.pv == 51 && d.psuit == SUIT_SPADES);
    CHECK(d.sn == 5 && d.stop == 7 && d.ssuit == SUIT_SPADES && d.seqs == 15 + 3);   /* a quint and a tierce */
    CHECK(d.setn == 4 && d.setrank == 7 && d.sets == 14);   /* four aces */
    n = parse_hand("7s 8s 9s Ts Js Qs Ks As 7h 8h 9h Th", h);
    piquet_declare(h, n, &d);
    CHECK(d.pn == 8 && d.sn == 8 && d.seqs == 18 + 4 && d.setn == 0 && d.sets == 0);
    n = parse_hand("Ks Kh Kd Qs Qh Qc Ts Th Td 9c 8c 7c", h);
    piquet_declare(h, n, &d);
    CHECK(d.setn == 3 && d.setrank == 6 && d.sets == 9);   /* three trios: kings best */
    CHECK(piquet_blanche(h, n) == 0);
    n = parse_hand("As Ts 9s 8s Ah Th 9h 7h Ad Td 9c 8c", h);
    CHECK(piquet_blanche(h, n) == 1);
    /* the comparison: the point by length, then value; sequences by length, then height */
    piquet_decl e = {0}, y = {0};
    e.pn = 5, e.pv = 45, y.pn = 5, y.pv = 46;
    CHECK(piquet_compare(&e, &y, PQ_POINT) == PQ_NOT_GOOD);
    y.pv = 45;
    CHECK(piquet_compare(&e, &y, PQ_POINT) == PQ_EQUAL);
    e.pn = 6;
    CHECK(piquet_compare(&e, &y, PQ_POINT) == PQ_GOOD);
    CHECK(piquet_compare(&e, &y, PQ_SEQUENCE) == PQ_NONE && piquet_compare(&e, &y, PQ_SETS) == PQ_NONE);
    e.sn = 4, e.stop = 5, y.sn = 4, y.stop = 6;
    CHECK(piquet_compare(&e, &y, PQ_SEQUENCE) == PQ_NOT_GOOD);
    y.sn = 3;
    CHECK(piquet_compare(&e, &y, PQ_SEQUENCE) == PQ_GOOD);
    y.setn = 3, y.setrank = 3;
    CHECK(piquet_compare(&e, &y, PQ_SETS) == PQ_NOT_GOOD);
}

static void test_scoring(void)
{
    /* the elder: point of 5 (spades), a quint, four aces; the younger nothing better: a repique */
    set_hands("As Ks Qs Js Ts Ah Ad Ac 7h 8d 9c 7c", "9s 8s 7s Kh Qh Jh Th 9h Kd Qd Jd Kc");
    CHECK(!piquet_script(&G, "As") && piquet_script(&G, "k") && P->turn == 1 && P->phase == PQ_DECLARE);
    CHECK(piquet_script(&G, "k"));
    CHECK(P->phase == PQ_PLAY && P->turn == 0);
    CHECK(P->verdict[PQ_POINT] == PQ_GOOD);     /* five against five: 51 against the younger's 49 */
    CHECK(P->decl[0].pn == 5 && P->decl[1].pn == 5 && P->decl[1].pv == 49 && P->decl[0].pv == 51);
    CHECK(P->decl[1].sn == 5 && P->decl[1].stop == 6 && P->verdict[PQ_SEQUENCE] == PQ_GOOD);   /* the ace's quint */
    /* the pique: 25 declared, then five leads of 10 or more while the younger has nothing */
    set_hands("Ah Kh Qh Jh Th 9h Ks Kd 7s 8d 7c 8c", "As Qs Js 9s Td Qd Jd 9d Tc Qc Jc 9c");
    CHECK(piquet_script(&G, "k k"));
    CHECK(P->dpts[0] == 6 + 16 + 3 && P->dpts[1] == 0 && P->repique == 255);
    CHECK(piquet_script(&G, "Ah 9s Kh 9d Qh 9c Jh Td") && P->pique == 255 && P->ppts[0] == 4);
    CHECK(piquet_script(&G, "Th") && P->pique == 0 && P->bonus[0] == 30);
}

static void test_scoring2(void)
{
    /* the elder: five spades (51), a quint major, four aces; the younger's longest suit four */
    set_hands("As Ks Qs Js Ts Ah Ad Ac 7h 8d 9c 7c", "9s 8s 7s Kh Qh Jh 9h Kd Qd Jd Kc Qc");
    CHECK(piquet_script(&G, "k k"));
    CHECK(P->decl[1].pn == 4 && P->verdict[PQ_POINT] == PQ_GOOD);
    CHECK(P->verdict[PQ_SEQUENCE] == PQ_GOOD && P->verdict[PQ_SETS] == PQ_GOOD);
    CHECK(P->dpts[0] == 5 + 15 + 14 && P->dpts[1] == 0 && P->repique == 0 && P->bonus[0] == 60);
    uint8_t out[12];
    CHECK(piquet_shown(P, 0, PQ_POINT, out) == 5 && piquet_shown(P, 0, PQ_SETS, out) == 4);
    CHECK(piquet_shown(P, 0, PQ_SEQUENCE, out) == 5 && piquet_shown(P, 1, PQ_SEQUENCE, out) == 9);
    /* the younger's sequence counts before the elder's sets: no repique */
    set_hands("As Ks Qs Js Ts Ah Ad Ac 7h 8d 9c 7c", "9s 8s 7s 6s Kh Qh Jh Th 9h Kd Qd Kc");
    P->n[1] = (uint8_t)parse_hand("9s 8s 7s Kh Qh Jh Th 9h 8h Kd Qd Kc", P->hand[1]);
    CHECK(piquet_script(&G, "k k"));
    CHECK(P->verdict[PQ_POINT] == PQ_NOT_GOOD && P->verdict[PQ_SEQUENCE] == PQ_NOT_GOOD);   /* six hearts */
    CHECK(P->dpts[1] == 6 + 16 + 3 && P->dpts[0] == 14 && P->repique == 255);
}

static void test_play(void)
{
    /* nothing to declare but points; the play: leads, wins, the last trick, the cards */
    set_hands("As Ks Qs Js Ts 9s 8s 7s Ah Kh Qh Jh", "Ad Kd Qd Jd Td 9d 8d 7d Ac Kc Qc Jc");
    CHECK(piquet_script(&G, "k k"));
    CHECK(P->verdict[PQ_POINT] == PQ_EQUAL && P->verdict[PQ_SEQUENCE] == PQ_EQUAL);   /* eight and eight */
    CHECK(P->dpts[0] == 0 && P->dpts[1] == 0);
    CHECK(piquet_script(&G, "As"));             /* a lead of 10 or more: 1 */
    CHECK(P->ppts[0] == 1 && piquet_can_play(P, CARD(SUIT_DIAMONDS, 7)));
    CHECK(piquet_script(&G, "7d"));             /* the younger has no spade */
    CHECK(P->last_winner == 0 && P->ppts[1] == 0);
    CHECK(piquet_script(&G, "Ks 8d Qs 9d Js Td Ts Jd 9s Qd 8s Kd 7s Ad Ah Ac Kh Kc Qh Qc"));
    CHECK(P->ppts[0] == 8 && P->tricks == 11 && P->turn == 0);   /* 9, 8 and 7 led: nothing */
    CHECK(piquet_script(&G, "Jh Jc"));
    /* the elder: all twelve tricks: capot 40, the last trick 1; no pique (the capot comes after the play) */
    CHECK(P->phase == PQ_ROUND && P->capot == 0 && P->pique == 255);
    CHECK(P->ppts[0] == 9 + 1 + 40 && P->bonus[0] == 0 && P->score[0] == 50 && P->score[1] == 0);
    /* the younger wins the elder's lead with a 10 or higher: 1 */
    set_hands("7s 8s 9s Ah Kh Qh Jh Th 9h 8h 7h Ad", "As Ks Qs Js Ts Ac Kc Qc Jc Tc 9c 8c");
    CHECK(piquet_script(&G, "k k 7s Ts"));
    CHECK(P->ppts[1] >= 1 && P->last_winner == 1);
}

static void test_rubicon(void)
{
    piquet_new(&G, 9);
    CHECK(piquet_script(&G, "# n # n # n # n # n"));
    CHECK(P->phase == PQ_PLAY || P->phase == PQ_EXCHANGE);
    P->total[0] = 150, P->total[1] = 90;
    CHECK(piquet_script(&G, "#"));
    CHECK(P->phase == PQ_OVER && P->deal == 6);
    int w = piquet_winner(P), l = 1 - w;
    CHECK(w >= 0 && P->rubicon == (P->total[l] < 100));
    CHECK(P->final == (P->rubicon ? P->total[w] + P->total[l] + 100 : P->total[w] - P->total[l] + 100));
    CHECK(!piquet_legal(P, PQA_NEXT));
}

static void test_undo(void)
{
    piquet_new(&G, 77);
    CHECK(piquet_script(&G, ". . k k . . . ."));
    int n = G.n;
    piquet_pos before = G.pos;
    CHECK(piquet_script(&G, ". ."));
    CHECK(piquet_undo(&G) && piquet_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (piquet_undo(&G)) {}
    piquet_new(&H, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_random_games(void)
{
    int games = 0, repiques = 0, piques = 0, capots = 0, rubicons = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        piquet_new(&G, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0;
        while (P->phase != PQ_OVER && ok && steps++ < 20000) {
            int pl = P->turn;
            switch (P->phase) {
            case PQ_ROUND:
                ok &= P->won[0] + P->won[1] == 12;
                for (int q = 0; q < 2; q++) ok &= P->score[q] == P->dpts[q] + P->ppts[q] + P->bonus[q];
                repiques += P->repique != 255, piques += P->pique != 255, capots += P->capot != 255;
                ok &= piquet_play(&G, PQA_NEXT);
                break;
            case PQ_EXCHANGE: {
                int lo = piquet_throw_min(P), hi = piquet_throw_max(P);
                int k = lo + (int)(cards_rand(&r) % (uint32_t)(hi - lo + 1)), m = 0;
                while (k > 0) {
                    int i = (int)(cards_rand(&r) % 12u);
                    if (!(m >> i & 1)) m |= 1 << i, k--;
                }
                ok &= piquet_play(&G, PQA_EXCHANGE | m);
                break;
            }
            case PQ_DECLARE: ok &= piquet_play(&G, PQA_DECLARE); break;
            default: {
                uint8_t legal[12];
                int k = 0;
                for (int i = 0; i < P->n[pl]; i++)
                    if (piquet_can_play(P, P->hand[pl][i])) legal[k++] = P->hand[pl][i];
                ok &= k > 0 && P->n[0] + P->n[1] + P->ntrick == 24 - 2 * P->tricks;
                if (k) ok &= piquet_play(&G, PQA_PLAY | legal[cards_rand(&r) % (uint32_t)k]);
            }
            }
        }
        ok &= P->phase == PQ_OVER && P->deal == 6;
        int w = piquet_winner(P);
        ok &= w >= 0 ? P->final >= 100 : P->total[0] == P->total[1];
        rubicons += P->rubicon;
        CHECK(ok);
        if (!ok) { printf("  seed %u\n", seed); break; }
        games++;
    }
    CHECK(games == 300);
    printf("  %d random parties: %d repiques, %d piques, %d capots, %d rubicons\n", games, repiques, piques, capots,
           rubicons);
}

int main(void)
{
    test_deal();
    test_exchange();
    test_declarations();
    test_scoring();
    test_scoring2();
    test_play();
    test_rubicon();
    test_undo();
    test_random_games();
    printf("piquet: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
