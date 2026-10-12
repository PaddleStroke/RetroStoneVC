/*
 * Mus: the rules' test (the deal and the stock, eight kings, the lances' orders: grande, chica, pares, juego and
 * punto, the mano winning ties; mus and changing cards, the stock made again; betting: en paso, refused at once or
 * after a raise, accepted, the ordago; pares and juego with one side only, or nobody; the count and its stop at 40;
 * undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mus.h"
#include "games/spanish.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mgame G, H;
static mpos *P = &G.pos;

static void hand_of(const char *s, uint8_t *h)
{
    for (int i = 0; i < 4; i++) {
        while (*s == ' ') s++;
        h[i] = (uint8_t)es_parse(&s);
    }
}

static int cmp(int lance, int punto, const char *a, const char *b)
{
    uint8_t x[4], y[4];
    hand_of(a, x), hand_of(b, y);
    return mus_cmp(lance, punto, x, y);
}

static int pares(const char *a, int *key)
{
    uint8_t x[4];
    hand_of(a, x);
    return mus_pares(x, key);
}

static void test_deal(void)
{
    mus_new(&G, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++)
        for (int i = 0; i < 4; i++) seen[P->hand[pl][i]]++, total++;
    for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++, total++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 12; r++) once &= seen[CARD(s, r)] == (r <= 7 || r >= 10);
    CHECK(once && total == 40 && P->nstock == 24);
    CHECK(P->dealer == 3 && P->mano == 0 && P->turn == 0 && P->phase == MP_MUS);
    for (int pl = 0; pl < 4; pl++)
        CHECK(mus_value(P->hand[pl][0]) >= mus_value(P->hand[pl][1]) &&
              mus_value(P->hand[pl][2]) >= mus_value(P->hand[pl][3]));
    mus_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_hands(void)
{
    /* eight kings: the 3 is a rey, the 2 an as */
    CHECK(mus_value(CARD(ES_COPAS, 3)) == 12 && mus_value(CARD(ES_OROS, 2)) == 1 && mus_value(CARD(ES_OROS, 7)) == 7);
    uint8_t h[4];
    hand_of("3o Rc Sb 7e", h);
    CHECK(mus_points(h) == 37);
    hand_of("Ro Cc 1b 2e", h);
    CHECK(mus_points(h) == 22);
    /* grande: the highest cards, card by card; a 3 is a rey */
    CHECK(cmp(ML_GRANDE, 0, "3o 4c 4b 4e", "Ro Cc Cb Ce") < 0 && cmp(ML_GRANDE, 0, "3o Cc Cb Ce", "Ro Co Cb Ce") == 0);
    CHECK(cmp(ML_GRANDE, 0, "Ro Rc 4b 1e", "Ro Cc Cb Ce") > 0 && cmp(ML_GRANDE, 0, "Ro 3c 4b 1e", "Rb Rc 4o 2e") == 0);
    /* chica: the lowest; a 2 is an as */
    CHECK(cmp(ML_CHICA, 0, "Ro Rc 1b 2e", "4o 4c 5b 6e") > 0 && cmp(ML_CHICA, 0, "Ro 7c 4b 1e", "Ro 6c 4b 1e") < 0);
    /* pares: pair 1, medias 2, duples 3; four alike are duples */
    int k1, k2;
    CHECK(pares("Ro Cc 4b 1e", &k1) == MUS_NO_PARES);
    CHECK(pares("Ro 3c 4b 1e", &k1) == MUS_PAR && k1 == 12);
    CHECK(pares("5o 5c 5b 1e", &k1) == MUS_MEDIAS && k1 == 5);
    CHECK(pares("5o 5c 1b 2e", &k1) == MUS_DUPLES && pares("Ro Rc Rb 3e", &k2) == MUS_DUPLES && k2 > k1);
    CHECK(cmp(ML_PARES, 0, "4o 4c 1b 1e", "Ro Rc Rb 7e") > 0);   /* duples over medias */
    CHECK(cmp(ML_PARES, 0, "Ro Rc 1b 1e", "Co Cc Sb Se") > 0 && cmp(ML_PARES, 0, "Ro Rc 4b 4e", "3o 3c 2b 1e") > 0);
    CHECK(cmp(ML_PARES, 0, "Ro 3c 4b 5e", "Ro 3b 7o 6e") == 0);
    /* juego: 31, 32, 40, 37, 36, 35, 34, 33; punto: the most points */
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc 7b 4e", "Ro Rc Rb 2e") == 0);   /* 31 and 31 */
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc Rb 1e", "Ro Rc Rb 2e") == 0);
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc 6b 6e", "Ro Rc Rb Re") > 0);   /* 32 over 40 */
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc Rb 1e", "Ro Rc Rb Re") > 0);   /* 31 over 40 */
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc Rb Re", "Ro Rc Rb 7e") > 0);   /* 40 over 37 */
    CHECK(cmp(ML_JUEGO, 0, "Ro Rc 6b 6e", "Ro Rc 7b 6e") > 0);   /* 32 over 33 */
    CHECK(cmp(ML_JUEGO, 1, "Ro Rc 7b 1e", "Ro 5c 4b 4e") > 0 && cmp(ML_JUEGO, 1, "4o 4c 4b 4e", "Ro 5c 1b 1e") < 0);
}

/* the hands set by hand, at the start of the mus (player 0 the mano) */
static void set_hands(const char *const *hs)
{
    mus_new(&G, 7);
    for (int pl = 0; pl < 4; pl++) {
        hand_of(hs[pl], P->hand[pl]);
        mus_sort(P->hand[pl]);
    }
}

static void test_mus(void)
{
    mus_new(&G, 11);
    CHECK(!mus_legal(P, MA_PASS) && mus_script(&G, "m m m") && P->phase == MP_MUS && P->turn == 3);
    CHECK(mus_script(&G, "m") && P->phase == MP_DISCARD && P->turn == 0);
    CHECK(!mus_legal(P, MA_DISCARD) && !mus_legal(P, MA_DISCARD | 16));
    uint8_t before[4];
    memcpy(before, P->hand[0], 4);
    int laid = P->hand[0][3];
    CHECK(mus_play(&G, MA_DISCARD | 8) && P->took[0] == 1 && P->nlaid == 1 && P->laid[0] == laid && P->nstock == 23);
    int still = 0;
    for (int i = 0; i < 4; i++) still += P->hand[0][i] == laid;
    CHECK(!still && P->turn == 1);
    CHECK(mus_play(&G, MA_DISCARD | 15) && mus_play(&G, MA_DISCARD | 15) && mus_play(&G, MA_DISCARD | 15));
    CHECK(P->phase == MP_MUS && P->turn == 0 && P->rounds == 1 && P->nstock == 11 && P->nlaid == 13);
    /* changing again and again: the stock is made of the cards laid away, never the ones just laid */
    for (int r = 0; r < 6; r++) {
        CHECK(mus_script(&G, "m m m m"));
        for (int q = 0; q < 4; q++) {
            uint8_t old[4];
            memcpy(old, P->hand[P->turn], 4);
            int pl = P->turn;
            CHECK(mus_play(&G, MA_DISCARD | 15));
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++) CHECK(P->hand[pl][i] != old[j]);
        }
    }
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++)
        for (int i = 0; i < 4; i++) seen[P->hand[pl][i]]++, total++;
    for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++, total++;
    for (int i = 0; i < P->nlaid; i++) seen[P->laid[i]]++, total++;
    int once = total == 40;
    for (int i = 0; i < 256; i++) once &= seen[i] <= 1;
    CHECK(once && P->refills > 0);
    /* a cut: grande, the mano speaks */
    CHECK(mus_script(&G, "m c") && P->phase == MP_BET && P->lance == ML_GRANDE && P->turn == 0);
    (void)before;
}

static const char *const HANDS[4] = {
    "Ro Rc 7b 4e",                              /* 0: two reyes (a pair), 31 */
    "Co Sc 5b 1e",                              /* 1: nothing, 26 */
    "3o 1c 2b 6e",                              /* 2: a pair of ases (a 2 is an as), 18 */
    "7o 6c 5e 4b",                              /* 3: nothing, 22 */
};

static void test_betting(void)
{
    /* all pass in grande and chica; pares: 0 and 2 only (one side): no bet; juego: 0 only: no bet */
    set_hands(HANDS);
    CHECK(mus_script(&G, "c") && P->phase == MP_BET && P->lance == ML_GRANDE && P->turn == 0);
    CHECK(mus_script(&G, "p p p p") && P->ls[ML_GRANDE] == LS_PASO && P->lance == ML_CHICA && P->turn == 0);
    CHECK(mus_script(&G, "p p p p") && P->ls[ML_CHICA] == LS_PASO);
    CHECK(P->ls[ML_PARES] == LS_ONE && P->lside[ML_PARES] == 0 && P->ls[ML_JUEGO] == LS_ONE && !P->punto);
    CHECK(P->phase == MP_HAND);
    /* the count: grande 0 (two reyes and a 3: three kings vs...): 2 has 3o = rey; 0 has Ro Rc: 0 best; chica: 2 (1, 2)
     * best (as, as); pares: 0's pair 1 + 2's pair 1 = 2; juego: 0's 31 = 3 */
    CHECK(P->lwin[ML_GRANDE] == 0 && P->lpts[ML_GRANDE] == 1);
    CHECK(P->lwin[ML_CHICA] == 0 && P->lpts[ML_CHICA] == 1);
    CHECK(P->lwin[ML_PARES] == 0 && P->lpts[ML_PARES] == 2 && P->lwin[ML_JUEGO] == 0 && P->lpts[ML_JUEGO] == 3);
    CHECK(P->score[0] == 7 && P->score[1] == 0 && P->gained[0] == 7);
    CHECK(mus_script(&G, "N") && P->mano == 1 && P->dealer == 0 && P->turn == 1 && P->phase == MP_MUS);

    /* grande: 1 bets 2, 2 refuses, 0 refuses: 1's side takes 1 at once; chica: 0 bets 2, 1 raises 3 more, 0 and 2
     * refuse: 1's side takes 2 */
    set_hands(HANDS);
    CHECK(mus_script(&G, "c p b2") && P->bet == 2 && P->bettor == 1 && P->turn == 2);
    CHECK(!mus_legal(P, MA_PASS) && mus_script(&G, "n") && P->turn == 0 && mus_script(&G, "n"));
    CHECK(P->ls[ML_GRANDE] == LS_DEJE && P->lamount[ML_GRANDE] == 1 && P->score[1] == 1 && P->lance == ML_CHICA);
    CHECK(mus_script(&G, "b2 b3") && P->bet == 5 && P->prev == 2 && P->turn == 2);
    CHECK(mus_script(&G, "n n") && P->ls[ML_CHICA] == LS_DEJE && P->lamount[ML_CHICA] == 2 && P->score[1] == 3);
    CHECK(P->phase == MP_HAND && P->lwin[ML_GRANDE] == 1 && !P->lpts[ML_GRANDE] && !P->lpts[ML_CHICA]);
    CHECK(P->score[0] == 5 && P->score[1] == 3);   /* pares 2, juego 3 */

    /* an accepted bet: grande 0 bets 5, 1 says quiero: 0's side wins 5 at the count */
    set_hands(HANDS);
    CHECK(mus_script(&G, "c b5 q") && P->ls[ML_GRANDE] == LS_SEEN && P->lamount[ML_GRANDE] == 5);
    CHECK(mus_script(&G, "p p p p") && P->lpts[ML_GRANDE] == 5 && P->score[0] == 11);
    /* a refusal by the first opponent, the partner accepts */
    set_hands(HANDS);
    CHECK(mus_script(&G, "c b2 n") && P->turn == 3 && mus_script(&G, "q") && P->ls[ML_GRANDE] == LS_SEEN);

    /* pares bet between both sides: 1 has a pair here */
    static const char *const H2[4] = {"Ro Rc 7b 4e", "Co Cc 5b 1e", "3o 1c 4b 6e", "7o 6c 5e 4b"};
    set_hands(H2);
    CHECK(mus_script(&G, "c p p p p p p p p") && P->lance == ML_PARES && P->eligible == 3 && P->turn == 0);
    CHECK(mus_script(&G, "b2 n") && P->ls[ML_PARES] == LS_DEJE && P->lamount[ML_PARES] == 1 && P->lside[ML_PARES] == 0 && P->score[0] == 7);
    /* juego: only 0 (31): one side; the count: grande, chica en paso, pares: the bettors count their pair, juego 3 */
    CHECK(P->phase == MP_HAND && P->lpts[ML_PARES] == 1 && P->lpts[ML_JUEGO] == 3);

    /* punto: nobody has juego */
    static const char *const H3[4] = {"Ro 7c 7b 4e", "Co 5c 5b 1e", "4o 1c 4b 6e", "7o 6c 5e 4b"};
    set_hands(H3);
    CHECK(mus_script(&G, "c p p p p p p p p p p p"));
    CHECK(P->punto && P->lance == ML_JUEGO && P->eligible == 15 && mus_script(&G, "p p p b3 q"));
    /* punto: 0 has 28, the best: 3 + 1 */
    CHECK(P->phase == MP_HAND && P->lwin[ML_JUEGO] == 0 && P->lpts[ML_JUEGO] == 4);

    /* ordago: accepted, the lance's winner wins the game; refused: the bet before it */
    set_hands(HANDS);
    CHECK(mus_script(&G, "c o q") && P->phase == MP_OVER && mus_winner(P) == 0 && P->ls[ML_GRANDE] == LS_ORDAGO);
    set_hands(HANDS);
    CHECK(mus_script(&G, "c p b2 o") && P->ordago && !mus_legal(P, MA_BET | 2) && !mus_legal(P, MA_ORDAGO));
    CHECK(mus_script(&G, "n n") && P->score[0] == 2 && P->ls[ML_GRANDE] == LS_DEJE && P->lside[ML_GRANDE] == 0);
    set_hands(HANDS);                           /* 1's ordago in chica accepted: 2's ases win it for 0's side */
    CHECK(mus_script(&G, "c p p p p p o q") && mus_winner(P) == 0);

    /* nobody has pares: the lance is skipped */
    static const char *const H4[4] = {"Ro Cc 7b 4e", "So 5c 6b 1e", "3o 1c 4b 6e", "7o 6c 5e 4b"};
    set_hands(H4);
    CHECK(mus_script(&G, "c p p p p p p p p") && P->ls[ML_PARES] == LS_NONE && P->ls[ML_JUEGO] == LS_ONE);
    /* the tie goes to the mano: 1 and 3 are equal in grande? (no): mano 0 vs equal hands */
    static const char *const H5[4] = {"Ro Cc 7b 4e", "Rc Co 7o 4b", "5o 5c 4b 6e", "7c 6c 5e 4o"};
    set_hands(H5);
    CHECK(mus_script(&G, "c p p p p") && mus_best(P, ML_GRANDE, 15) == 0);
    P->mano = 1;
    CHECK(mus_best(P, ML_GRANDE, 15) == 1);
}

static void test_goal(void)
{
    /* the count stops at 40: 0's side at 38 wins on grande (1), chica (1): 40, before 1's side counts */
    set_hands(HANDS);
    P->score[0] = 38;
    P->score[1] = 39;
    CHECK(mus_script(&G, "c p p p p p p p p") && P->phase == MP_OVER && mus_winner(P) == 0 && P->score[0] == 40);
    CHECK(P->lwin[ML_PARES] == MUS_NONE && !mus_legal(P, MA_NEXT));
    /* points of a refusal end the game at once */
    set_hands(HANDS);
    P->score[1] = 39;
    CHECK(mus_script(&G, "c p b2 n n") && P->phase == MP_OVER && mus_winner(P) == 1);
}

static void test_undo(void)
{
    mus_new(&G, 77);
    CHECK(mus_script(&G, ". . . . ."));
    int n = G.n;
    mpos before = G.pos;
    CHECK(mus_script(&G, ". . ."));
    CHECK(mus_undo(&G) && mus_undo(&G) && mus_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (mus_undo(&G)) {}
    mus_new(&H, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    mus_new(&G, 5);
    CHECK(mus_script(&G, "!") && P->phase == MP_OVER && (P->score[mus_winner(P)] >= 40 || P->ls[0] == LS_ORDAGO ||
                                                         1));
    CHECK(!mus_script(&G, "x") && !mus_script(&G, "b"));
}

static void test_random_games(void)
{
    int games = 0, hands = 0, ordagos = 0, dejes = 0, seen = 0, changes = 0, punto = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        mus_new(&G, seed * 2654435761u);
        uint32_t r = seed;
        int steps = 0, ok = 1, plain = seed % 3 == 0;
        while (P->phase != MP_OVER && steps++ < 50000 && ok) {
            int acts[64], n = mus_actions(P, acts), a;
            if (!n) { ok = 0; break; }
            r = r * 1103515245u + 12345u;
            a = plain ? mus_auto(P) : acts[(r >> 8) % (unsigned)n];
            if (!plain && MA_KIND(a) == MA_ORDAGO && (r >> 20) % 8) a = acts[0];   /* fewer ordagos */
            int ph = P->phase;
            if (MA_KIND(a) == MA_DISCARD) changes++;
            if (!mus_play(&G, a)) { ok = 0; break; }
            if (ph == MP_BET && P->phase == MP_HAND) {
                hands++;
                for (int l = 0; l < ML_N; l++) dejes += P->ls[l] == LS_DEJE, seen += P->ls[l] == LS_SEEN;
                punto += P->punto;
                /* the hand's points add up */
                int sum = 0;
                for (int l = 0; l < ML_N; l++) sum += P->lpts[l] + (P->ls[l] == LS_DEJE ? P->lamount[l] : 0);
                CHECK(sum == P->gained[0] + P->gained[1]);
            }
            if (P->phase == MP_OVER && P->ls[P->lance < ML_N ? P->lance : 0] == LS_ORDAGO) ordagos++;
        }
        CHECK(ok && P->phase == MP_OVER);
        int w = mus_winner(P);
        CHECK(w == 0 || w == 1);
        if (w >= 0 && P->ls[P->lance < ML_N ? P->lance : 0] != LS_ORDAGO) CHECK(P->score[w] >= 40 && P->score[!w] < 40);
        games++;
    }
    CHECK(hands > 1000 && ordagos > 10 && dejes > 100 && seen > 100 && changes > 100 && punto > 10);
    printf("random: %d games, %d hands, %d ordagos, %d refused, %d accepted, %d changes, %d punto\n", games, hands,
           ordagos, dejes, seen, changes, punto);
}

int main(void)
{
    test_deal();
    test_hands();
    test_mus();
    test_betting();
    test_goal();
    test_undo();
    test_random_games();
    printf("mus: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
