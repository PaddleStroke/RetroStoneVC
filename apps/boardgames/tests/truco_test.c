/*
 * Truco: the rules' test (the deal of 40 without 8s and 9s, the cards' order, the envido's count, the rounds and the
 * pardas, truco, retruco, vale cuatro and who may raise, refusals, the envido's calls and their worth, the falta in
 * the malas and the buenas, the envido first when answering a truco, me voy al mazo, the flor, two and four players,
 * the end at 15 and 30, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/spanish.h"
#include "games/truco.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static tgame G, H;
static tpos *P = &G.pos;

static int card(const char *s) { return es_parse(&s); }

static void three(const char *s, uint8_t *h)
{
    for (int i = 0; i < 3; i++) {
        while (*s == ' ') s++;
        h[i] = (uint8_t)es_parse(&s);
    }
}

static int envido(const char *s)
{
    uint8_t h[3];
    three(s, h);
    return truco_envido(h);
}

/* a new game with these hands (player 0 the mano) */
static void set_hands(int players, int goal, int flor, const char *const *hs)
{
    truco_new(&G, players, goal, 0, 9);
    P->flor = (uint8_t)flor;
    for (int pl = 0; pl < players; pl++) {
        three(hs[pl], P->dealt[pl]);
        memcpy(P->hand[pl], P->dealt[pl], 3);
        P->env_value[pl] = (uint8_t)truco_envido(P->dealt[pl]);
    }
    G.n = 0;
}

static void test_deal(void)
{
    truco_new(&G, 4, 15, 0, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->n[pl] == 3);
        for (int i = 0; i < 3; i++) { seen[P->hand[pl][i]]++, total++; CHECK(CARD_RANK(P->hand[pl][i]) != 8); }
    }
    int once = total == 12;
    for (int i = 0; i < 256; i++) once &= seen[i] <= 1;
    CHECK(once && P->dealer == 3 && P->mano == 0 && P->turn == 0 && P->level == 1 && P->env == TE_OPEN);
    truco_new(&H, 4, 15, 0, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    truco_new(&G, 2, 30, 0, 42);
    CHECK(P->players == 2 && P->dealer == 1 && P->mano == 0 && P->goal == 30 && P->n[2] == 0);
}

static void test_cards(void)
{
    static const char *const order[] = {"1e", "1b", "7e", "7o", "3c", "2b", "1c", "Ro", "Cc", "Se", "7c", "6o", "5b",
                                        "4e"};
    for (int i = 0; i + 1 < 14; i++) CHECK(truco_power(card(order[i])) > truco_power(card(order[i + 1])));
    CHECK(truco_power(card("1c")) == truco_power(card("1o")) && truco_power(card("7c")) == truco_power(card("7b")));
    CHECK(truco_power(card("3o")) == truco_power(card("3e")));
    /* the envido: two of a suit 20 + numbers (figures 0); else the highest number */
    CHECK(envido("7e 6e 1o") == 33 && envido("Re Se 1o") == 20 && envido("7e Ce 5o") == 27 && envido("Ro Cc Se") == 0);
    CHECK(envido("7e 4o 5c") == 7 && envido("1e 2e 3e") == 25);
    uint8_t h[3];
    three("1e 2e Re", h);
    CHECK(truco_has_flor(h));
    three("1e 2e Ro", h);
    CHECK(!truco_has_flor(h));
}

static void test_rounds(void)
{
    /* two players: 0 wins the first two rounds: 1 point */
    static const char *const a[2] = {"1e 7o 4c", "3c 2o 5b"};
    set_hands(2, 15, 0, a);
    CHECK(P->turn == 0 && !truco_legal(P, TA_PLAY | card("3c")) && truco_script(&G, "1e 3c"));
    CHECK(P->result[0] == 0 && P->turn == 0 && truco_script(&G, "7o 2o"));
    CHECK(P->phase == TP_HAND && P->hand_side == 0 && P->hand_pts == 1 && P->score[0] == 1);
    CHECK(truco_script(&G, "N") && P->mano == 1 && P->turn == 1 && P->dealer == 0);
    /* a parda first: the second round decides; the same leader leads again */
    static const char *const b[2] = {"3o 4e 5c", "3e 1o 6b"};
    set_hands(2, 15, 0, b);
    CHECK(truco_script(&G, "3o 3e") && P->result[0] == TR_PARDA && P->turn == 0);
    CHECK(truco_script(&G, "4e 1o") && P->phase == TP_HAND && P->hand_side == 1);
    /* won, then a parda: the first round's winner takes it */
    static const char *const c[2] = {"1e 2o 4c", "3c 2e 5b"};
    set_hands(2, 15, 0, c);
    CHECK(truco_script(&G, "1e 3c 2o 2e") && P->phase == TP_HAND && P->hand_side == 0);
    /* one each, then a parda: the first round's winner */
    static const char *const d[2] = {"1e 4o Rc", "3c 1b Re"};
    set_hands(2, 15, 0, d);
    CHECK(truco_script(&G, "1e 3c 4o 1b") && P->phase == TP_PLAY && P->turn == 1);
    CHECK(truco_script(&G, "Re Rc") && P->phase == TP_HAND && P->hand_side == 0);
    /* three pardas: the mano */
    static const char *const e[2] = {"3o 2o 6c", "3e 2e 6b"};
    set_hands(2, 15, 0, e);
    P->mano = 1;
    CHECK(truco_script(&G, "3o 3e 2o 2e 6c 6b") && P->phase == TP_HAND && P->hand_side == 1);
    /* four players: partners tied win the round; the leader is the first who played the winning strength */
    static const char *const f[4] = {"3o 4c 5c", "Ro 4o 5o", "3e 4b 6b", "Co 6e 5e"};
    set_hands(4, 15, 0, f);
    CHECK(truco_script(&G, "3o Ro 3e Co") && P->result[0] == 0 && P->best[0] == 0 && P->turn == 0);
    /* opponents tied: parda */
    static const char *const g[4] = {"3o 4c 5c", "3c 4o 5o", "Re 4b 6b", "Co 6e 5e"};
    set_hands(4, 15, 0, g);
    CHECK(truco_script(&G, "3o 3c Re Co") && P->result[0] == TR_PARDA && P->turn == 0);
}

static void test_truco(void)
{
    static const char *const a[2] = {"1e 7o 4c", "3c 2o 5b"};
    /* truco accepted: the hand is worth 2; only the accepting side may raise, on its turn */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t") && P->call == 2 && truco_actor(P) == 1 && !truco_legal(P, TA_PLAY | card("1e")));
    CHECK(!truco_legal(P, TA_MAZO) && truco_script(&G, "q") && P->level == 2 && P->owner == 1 && truco_actor(P) == 0);
    CHECK(!truco_legal(P, TA_TRUCO) && truco_script(&G, "1e") && truco_legal(P, TA_TRUCO));
    CHECK(truco_script(&G, "t") && P->call == 3 && truco_actor(P) == 0 && truco_script(&G, "t"));   /* vale cuatro */
    CHECK(P->level == 3 && P->owner == 0 && P->call == 4 && truco_actor(P) == 1 && !truco_legal(P, TA_TRUCO));
    CHECK(truco_script(&G, "q 3c 7o 2o") && P->phase == TP_HAND && P->hand_side == 0 && P->hand_pts == 4);
    /* refused: the callers take the hand at its worth before the call */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t n") && P->phase == TP_HAND && P->hand_side == 0 && P->hand_pts == 1);
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t t n") && P->hand_side == 1 && P->hand_pts == 2);   /* retruco refused: 2 */
    /* me voy al mazo: the opponents take the hand */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t q 1e m") && P->phase == TP_HAND && P->hand_side == 0 && P->hand_pts == 2);
    CHECK(P->mazo == 2);
}

static void test_envido(void)
{
    static const char *const a[2] = {"7e 6e 4c", "1o 5o 3c"};   /* 33 against 26 */
    set_hands(2, 15, 0, a);
    CHECK(truco_can_envido(P) && truco_script(&G, "e") && P->env == TE_CALLING && truco_actor(P) == 1);
    CHECK(!truco_legal(P, TA_PLAY | card("1o")) && !truco_legal(P, TA_TRUCO));
    CHECK(truco_script(&G, "q") && P->env == TE_DONE && P->env_winner == 0 && P->env_pts == 2 && P->score[0] == 2);
    CHECK(!truco_can_envido(P) && truco_actor(P) == 0);
    /* envido, envido, real envido, falta: refused: 2 + 2 + 3 = 7 to the falta's caller */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "e e") && truco_actor(P) == 0);
    CHECK(!truco_legal(P, TA_ENVIDO) && truco_script(&G, "r") && !truco_legal(P, TA_REAL) && truco_script(&G, "f"));
    CHECK(!truco_legal(P, TA_FALTA) && truco_script(&G, "n") && P->score[1] == 7 && P->env_pts == 7);
    /* refused at once: 1 */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "r n") && P->score[0] == 1);
    /* real envido accepted: 3; the mano wins a tie */
    static const char *const b[2] = {"7e 6e 4c", "7o 6o 3c"};
    set_hands(2, 15, 0, b);
    CHECK(truco_script(&G, "4c r q") && P->env_winner == 0 && P->score[0] == 3);
    /* only before one's first card, in the first round */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "4c 1o") && !truco_can_envido(P));
    /* falta: in the malas (both under 15, game to 30) the whole game; else what the leader lacks */
    set_hands(2, 30, 0, a);
    CHECK(truco_falta(P, 0) == 30 && truco_script(&G, "f q") && P->phase == TP_OVER && truco_winner(P) == 0);
    set_hands(2, 30, 0, a);
    P->score[0] = 10, P->score[1] = 18;
    CHECK(truco_falta(P, 0) == 12 && truco_script(&G, "f q") && P->score[0] == 22 && P->phase == TP_PLAY);
    set_hands(2, 15, 0, a);
    P->score[0] = 3, P->score[1] = 9;
    CHECK(truco_falta(P, 1) == 6 && truco_script(&G, "e f q") && P->score[0] == 9);
    /* answering a truco, the envido comes first; then the truco is answered */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t") && truco_can_envido(P) && truco_script(&G, "e") && truco_actor(P) == 0);
    CHECK(truco_script(&G, "q") && P->score[0] == 2 && P->call == 2 && truco_actor(P) == 1 && !truco_can_envido(P));
    CHECK(truco_script(&G, "q") && P->level == 2);
    /* once the truco is accepted, no envido */
    set_hands(2, 15, 0, a);
    CHECK(truco_script(&G, "t q") && !truco_can_envido(P) && !truco_legal(P, TA_ENVIDO));
    /* four players: the envido's responder is the next opponent; the best of all four wins */
    static const char *const c[4] = {"4c 5o 6b", "7e 6e 1o", "Re Se 2c", "1b 2b Ro"};
    set_hands(4, 15, 0, c);
    CHECK(truco_script(&G, "e") && truco_actor(P) == 1 && truco_script(&G, "q") && P->env_winner == 1);
    CHECK(P->score[1] == 2);
}

static void test_flor(void)
{
    static const char *const a[2] = {"7e 6e 4e", "1o 5o 3c"};
    truco_new(&G, 2, 15, 1, 1);
    set_hands(2, 15, 1, a);
    /* the deal sees the flor: set it again through a deal with these cards */
    P->flor_mask = 0;
    int f = truco_has_flor(P->dealt[0]);
    CHECK(f);
    /* run the flor through a real deal: search a seed that deals a flor */
    int found = 0;
    for (uint32_t s = 1; s < 3000 && !found; s++) {
        truco_new(&G, 2, 15, 1, s);
        if (P->flor_mask) found = 1;
    }
    CHECK(found && P->env == TE_DONE && P->flor_pts >= 3 && P->score[P->flor_side] == P->flor_pts);
    CHECK(!truco_legal(P, TA_ENVIDO));
    int both = 0;
    for (uint32_t s = 1; s < 200000 && !both; s++) {
        truco_new(&G, 4, 30, 1, s);
        if ((P->flor_mask & 5) && (P->flor_mask & 10)) both = 1;
    }
    CHECK(both && P->flor_pts >= 6 && P->score[P->flor_side] == P->flor_pts && P->score[!P->flor_side] == 0);
    truco_new(&G, 2, 15, 0, 1);
    for (uint32_t s = 1; s < 3000; s++) {
        truco_new(&G, 2, 15, 0, s);
        CHECK(!P->flor_mask);
    }
}

static void test_undo(void)
{
    truco_new(&G, 4, 30, 1, 77);
    CHECK(truco_script(&G, ". . . . ."));
    int n = G.n;
    tpos before = G.pos;
    CHECK(truco_script(&G, ". . ."));
    CHECK(truco_undo(&G) && truco_undo(&G) && truco_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (truco_undo(&G)) {}
    truco_new(&H, 4, 30, 1, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    CHECK(truco_script(&G, "!") && P->phase == TP_OVER && P->score[truco_winner(P)] >= 30);
    CHECK(!truco_script(&G, "x"));
}

static void test_random_games(void)
{
    int games = 0, hands = 0, envidos = 0, trucos = 0, mazos = 0, flores = 0;
    for (uint32_t seed = 1; seed <= 600; seed++) {
        int players = seed & 1 ? 2 : 4, goal = seed % 3 ? 15 : 30, flor = seed % 5 == 0;
        truco_new(&G, players, goal, flor, seed * 2654435761u);
        uint32_t r = seed;
        int steps = 0, ok = 1, plain = seed % 4 == 0;
        while (P->phase != TP_OVER && steps++ < 50000 && ok) {
            int acts[32], n = truco_actions(P, acts);
            if (!n) { ok = 0; break; }
            r = r * 1103515245u + 12345u;
            int a = plain ? truco_auto(P) : acts[(r >> 8) % (unsigned)n];
            if (!plain && TA_KIND(a) == TA_MAZO && (r >> 20) % 6) a = acts[0];
            int ph = P->phase;
            if (!truco_play(&G, a)) { ok = 0; break; }
            if (ph == TP_PLAY && P->phase == TP_HAND) {
                hands++;
                envidos += P->env_pts > 0;
                trucos += P->level > 1;
                mazos += P->mazo > 0;
                flores += P->flor_pts > 0;
                CHECK(P->hand_pts >= 1 && P->hand_pts <= 4);
                int sum = 0;
                for (int s = 0; s < 2; s++) sum += P->gained[s][0] + P->gained[s][1] + P->gained[s][2];
                CHECK(sum == P->hand_pts + P->env_pts + P->flor_pts);
            }
        }
        CHECK(ok && P->phase == TP_OVER);
        int w = truco_winner(P);
        CHECK((w == 0 || w == 1) && P->score[w] >= goal && P->score[!w] < goal);
        games++;
    }
    CHECK(hands > 2000 && envidos > 100 && trucos > 100 && mazos > 50 && flores > 10);
    printf("random: %d games, %d hands, %d envidos, %d trucos, %d mazos, %d flores\n", games, hands, envidos, trucos,
           mazos, flores);
}

int main(void)
{
    test_deal();
    test_cards();
    test_rounds();
    test_truco();
    test_envido();
    test_flor();
    test_undo();
    test_random_games();
    printf("truco: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
