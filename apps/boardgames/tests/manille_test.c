/*
 * Manille: the rules' test (the deal and the turned card, the order and the points, doubling, following and beating,
 * trumping and over-trumping, the partner master, the count to 68 and the score over 34, the game to 34, undo, the
 * notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/manille.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mgame G, H;
static mpos *P = &G.pos;

static int C(const char *s) { return trick_parse(&s); }

static void test_cards(void)
{
    CHECK(man_rank(C("Ts")) == 8 && man_rank(C("As")) == 7 && man_rank(C("Ks")) == 6 && man_rank(C("Js")) == 4);
    CHECK(man_rank(C("9s")) == 3 && man_rank(C("7s")) == 1);
    int sum = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (r == 1 || r >= 7) sum += man_points(CARD(s, r));
    CHECK(sum == 60);
}

static void test_deal(void)
{
    man_new(&G, 42);
    int seen[256] = {0}, total = 0;
    for (int q = 0; q < 4; q++) {
        CHECK(P->n[q] == 8);
        for (int i = 0; i < 8; i++) seen[P->hand[q][i]]++, total++;
    }
    int once = total == 32;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (r == 1 || r >= 7) once &= seen[CARD(s, r)] == 1;
    CHECK(once);
    CHECK(P->dealer == 3 && P->turn == 0 && P->phase == MP_SPEAK && P->mult == 1);
    CHECK(trick_find(P->hand[3], 8, P->turned) >= 0 && P->trump == CARD_SUIT(P->turned));
    CHECK(CARD_SUIT(P->hand[0][0]) == P->trump || !trick_has_suit(P->hand[0], 8, P->trump));   /* trumps first */
    man_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_doubling(void)
{
    man_new(&G, 5);
    CHECK(!man_legal(P, MA_REDOUBLE) && man_legal(P, MA_DOUBLE));
    CHECK(man_script(&G, "p p p p") && P->phase == MP_PLAY && P->mult == 1 && P->turn == 0);
    man_new(&G, 5);
    CHECK(man_script(&G, "p d"));                /* player 1 doubles: players 2 and 0 (the opponents) may redouble */
    CHECK(P->mult == 2 && P->turn == 2 && P->phase == MP_SPEAK);
    CHECK(!man_legal(P, MA_DOUBLE) && man_legal(P, MA_REDOUBLE));
    CHECK(man_script(&G, "p") && P->turn == 0 && man_script(&G, "r"));
    CHECK(P->mult == 4 && P->phase == MP_PLAY && P->turn == 0);
    man_new(&G, 5);
    CHECK(man_script(&G, "p p p d p p") && P->mult == 2 && P->phase == MP_PLAY);
}

/* hands set by hand (sorted), trumps given, the play with player 0 to lead */
static void set_hands(const char *const *hs, int trump)
{
    man_new(&G, 3);
    for (int q = 0; q < 4; q++) {
        const char *s = hs[q];
        P->n[q] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[q][P->n[q]++] = (uint8_t)c;
        }
        man_sort(P->hand[q], P->n[q], trump);
    }
    P->trump = (uint8_t)trump;
    P->phase = MP_PLAY;
    P->turn = P->leader = 0;
}

static void test_play(void)
{
    static const char *const h1[4] = {"Ah 7h 8c", "Kh Th 7c", "9h Qs 7d", "Jc Ks 8d"};
    set_hands(h1, SUIT_SPADES);
    CHECK(man_script(&G, "Ah"));
    /* player 1 follows; the opponent holds it with the ace: must beat it with the ten */
    CHECK(!man_can_play(P, C("Kh")) && man_can_play(P, C("Th")) && !man_can_play(P, C("7c")));
    CHECK(man_script(&G, "Th"));
    /* player 2: the partner (0) does not hold it now; the opponent's ten cannot be beaten: any heart */
    CHECK(man_can_play(P, C("9h")) && !man_can_play(P, C("Qs")));
    CHECK(man_script(&G, "9h"));
    /* player 3: no heart; their partner (1) holds the trick: anything */
    CHECK(man_can_play(P, C("Jc")) && man_can_play(P, C("Ks")) && man_can_play(P, C("8d")));
    CHECK(man_script(&G, "Jc"));
    CHECK(P->last_winner == 1 && P->turn == 1 && P->won[1] == 1 && P->pts[1] == 4 + 5 + 1);
    /* void, an opponent holds it: trump; over-trump an opponent's trump if you can */
    static const char *const h2[4] = {"Ac 9c", "7s Ks 8d", "Qs 7h 7c", "Ts 8h"};
    set_hands(h2, SUIT_SPADES);
    CHECK(man_script(&G, "Ac"));
    CHECK(man_can_play(P, C("7s")) && man_can_play(P, C("Ks")) && !man_can_play(P, C("8d")));
    CHECK(man_script(&G, "Ks"));
    /* player 2 holds the clubs seven: must follow */
    CHECK(man_can_play(P, C("7c")) && !man_can_play(P, C("Qs")) && !man_can_play(P, C("7h")));
    CHECK(man_script(&G, "7c"));
    /* player 3: partner (1) holds it with a trump: free */
    CHECK(man_can_play(P, C("8h")) && man_can_play(P, C("Ts")));
    static const char *const h3[4] = {"Ac 9c", "Ks 8d", "Qs Ts 7h", "7s 8h"};
    set_hands(h3, SUIT_SPADES);
    CHECK(man_script(&G, "Ac Ks"));
    /* player 2, void in clubs, the opponent trumped with the king: over-trump with the ten (the queen won't do) */
    CHECK(man_can_play(P, C("Ts")) && !man_can_play(P, C("Qs")) && !man_can_play(P, C("7h")));
    static const char *const h4[4] = {"Ac 9c", "Ts 8d", "Qs 7h", "7s 8h"};
    set_hands(h4, SUIT_SPADES);
    CHECK(man_script(&G, "Ac Ts"));
    /* cannot over-trump: any card */
    CHECK(man_can_play(P, C("Qs")) && man_can_play(P, C("7h")));
}

static void test_score(void)
{
    man_new(&G, 11);
    CHECK(man_script(&G, "#"));
    CHECK(P->phase == MP_DEAL || P->phase == MP_OVER);
    CHECK(P->made[0] + P->made[1] == 68 && P->won[0] + P->won[1] == 8 && P->pts[0] + P->pts[1] == 60);
    int t = P->made[0] > P->made[1] ? 0 : 1;
    if (P->made[0] != P->made[1]) CHECK(P->score[t] == P->made[t] - 34 && P->score[1 - t] == 0);
    else CHECK(P->score[0] == 0 && P->score[1] == 0);
    /* doubled: twice the score */
    man_new(&H, 11);
    CHECK(man_script(&H, "d p p #"));
    CHECK(H.pos.made[t] == P->made[t] && H.pos.score[t] == 2 * P->score[t]);
    man_new(&H, 11);
    CHECK(man_script(&H, "d r #") && H.pos.score[t] == 4 * P->score[t]);
    /* the next deal: the dealer moves on */
    if (P->phase == MP_DEAL) {
        CHECK(man_script(&G, "N") && P->dealer == 0 && P->turn == 1 && P->phase == MP_SPEAK);
    }
}

static void test_random(void)
{
    int over = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        man_new(&G, seed);
        uint32_t r = seed * 2654435761u;
        int ok = 1;
        for (int step = 0; step < 3000 && P->phase != MP_OVER; step++) {
            int legal[40], n = 0;
            if (P->phase == MP_SPEAK) {
                static const int sp[3] = {MA_PASS, MA_DOUBLE, MA_REDOUBLE};
                for (int i = 0; i < 3; i++)
                    if (man_legal(P, sp[i])) legal[n++] = sp[i];
            } else if (P->phase == MP_PLAY) {
                for (int i = 0; i < P->n[P->turn]; i++)
                    if (man_can_play(P, P->hand[P->turn][i])) legal[n++] = MA_PLAY | P->hand[P->turn][i];
            } else legal[n++] = MA_NEXT;
            if (!n) { ok = 0; break; }
            r = r * 1103515245u + 12345u;
            int a = legal[(r >> 8) % (unsigned)n];
            if (P->phase == MP_SPEAK && (r >> 20) % 4) a = MA_PASS;
            if (!man_play(&G, a)) { ok = 0; break; }
            if (P->phase == MP_DEAL || P->phase == MP_OVER) {
                ok &= P->made[0] + P->made[1] == 68;
                ok &= (P->score[0] == 0 || P->score[1] == 0);
            }
            int cards = P->ntrick;
            for (int q = 0; q < 4; q++) cards += P->n[q];
            ok &= P->phase != MP_PLAY || cards == 32 - 4 * P->tricks;
        }
        CHECK(ok);
        over += P->phase == MP_OVER;
        CHECK(P->phase != MP_OVER || P->total[man_winner_team(P)] >= MAN_TARGET);
        if (seed % 40 == 0) {
            mpos end = *P;
            int last = G.act[G.n - 1];
            CHECK(man_undo(&G) && man_play(&G, last) && !memcmp(&end, P, sizeof end));
        }
    }
    CHECK(over == 400);
    man_new(&G, 9);
    CHECK(man_script(&G, "!") && P->phase == MP_OVER);
    CHECK(!man_script(&G, "Zz"));
}

int main(void)
{
    test_cards();
    test_deal();
    test_doubling();
    test_play();
    test_score();
    test_random();
    printf("manille: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
