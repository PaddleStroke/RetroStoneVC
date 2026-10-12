/*
 * Écarté: the rules' test (the ranks, the deal and the turned king, proposing, accepting and refusing, the exchange,
 * following and winning the trick, trumping, the king of trumps held, the points with the vole and the penalties, the
 * end at 5, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/ecarte.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ecarte_game G, H;
static ecarte_pos *P = &G.pos;

static int card_of(const char *c)
{
    const char *s = c;
    return trick_parse(&s);
}

/* hands set by hand, the play about to begin (the elder, player 0, leads), trumps given */
static void set_hands(const char *h0, const char *h1, int trump)
{
    ecarte_new(&G, 1);
    const char *hs[2] = {h0, h1};
    for (int pl = 0; pl < 2; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        ecarte_sort(P->hand[pl], P->n[pl]);
    }
    P->trump = (uint8_t)trump;
    P->turned = CARD(trump, 7);
    P->total[0] = P->total[1] = 0;
    P->king[0] = P->king[1] = 0;
    P->score[0] = P->score[1] = 0;
    G.n = 0;
}

static void test_deal(void)
{
    CHECK(ecarte_rank(CARD(0, RANK_KING)) > ecarte_rank(CARD(0, RANK_QUEEN)));
    CHECK(ecarte_rank(CARD(0, RANK_JACK)) > ecarte_rank(CARD(0, RANK_ACE)));
    CHECK(ecarte_rank(CARD(0, RANK_ACE)) > ecarte_rank(CARD(0, 10)));
    CHECK(ecarte_rank(CARD(0, 8)) > ecarte_rank(CARD(0, 7)));
    for (uint32_t seed = 1; seed <= 40; seed++) {
        ecarte_new(&G, seed);
        int seen[256] = {0}, ok = 1;
        for (int pl = 0; pl < 2; pl++)
            for (int i = 0; i < 5; i++) seen[P->hand[pl][i]]++;
        for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
        seen[P->turned]++;
        int total = 0;
        for (int c = 0; c < 256; c++) ok &= seen[c] <= 1, total += seen[c];
        CHECK(ok && total == 32 && P->nstock == 21 && P->trump == CARD_SUIT(P->turned));
        CHECK(P->dealer == 1 && P->elder == 0 && P->turn == 0 && (P->phase == EC_PROPOSE || P->phase == EC_OVER));
        CHECK(P->total[1] == (CARD_RANK(P->turned) == RANK_KING) && P->total[0] == 0);
    }
    ecarte_new(&G, 3);
    ecarte_new(&H, 3);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

/* proposing: accepted, the exchange (elder 1..5, dealer 0..5), proposing again; refused: the play */
static void test_exchange(void)
{
    ecarte_new(&G, 5);
    CHECK(ecarte_legal(P, ECA_ASK) && !ecarte_legal(P, ECA_YES) && !ecarte_legal(P, ECA_DISCARD | 1));
    CHECK(ecarte_script(&G, "q"));
    CHECK(P->phase == EC_ANSWER && P->turn == 1 && P->asked == 1);
    CHECK(ecarte_script(&G, "y"));
    CHECK(P->phase == EC_DISCARD && P->turn == 0 && ecarte_throw_min(P) == 1 && ecarte_throw_max(P) == 5);
    CHECK(!ecarte_legal(P, ECA_DISCARD | 0) && !ecarte_legal(P, ECA_DISCARD | 1 << 5));
    uint8_t before[5];
    memcpy(before, P->hand[0], 5);
    CHECK(ecarte_play(&G, ECA_DISCARD | 0x7));  /* three thrown */
    int kept = 0;
    for (int i = 3; i < 5; i++) kept += trick_find(P->hand[0], 5, before[i]) >= 0;
    for (int i = 0; i < 3; i++) CHECK(trick_find(P->hand[0], 5, before[i]) < 0);
    CHECK(kept == 2 && P->nstock == 18 && P->drew[0] == 3 && P->turn == 1 && ecarte_throw_min(P) == 0);
    CHECK(ecarte_play(&G, ECA_DISCARD | 0));     /* the dealer keeps all */
    CHECK(P->phase == EC_PROPOSE && P->turn == 0 && P->thrown == 3);
    CHECK(ecarte_script(&G, "q x"));            /* the second proposal refused: no penalty */
    CHECK(P->phase == EC_PLAY && P->turn == 0 && !P->refused && !P->forced);
    ecarte_new(&G, 5);
    CHECK(ecarte_script(&G, "q x") && P->refused && P->phase == EC_PLAY);
    ecarte_new(&G, 5);                          /* played at once */
    int c = P->hand[0][2];
    CHECK(ecarte_play(&G, ECA_PLAY | c) && P->forced && P->ntrick == 1 && P->turn == 1);
    /* the stock running out: the last draws take what is left */
    ecarte_new(&G, 6);
    P->nstock = 3;
    CHECK(ecarte_script(&G, "q y") && ecarte_throw_max(P) == 3 && !ecarte_legal(P, ECA_DISCARD | 0xf));
    CHECK(ecarte_play(&G, ECA_DISCARD | 0x3) && ecarte_throw_max(P) == 1);
    CHECK(ecarte_play(&G, ECA_DISCARD | 0x1) && P->nstock == 0 && P->phase == EC_PLAY);
}

static void test_play(void)
{
    set_hands("Kh 9h 7c 8d 7s", "Qh Ah Jc 9d 8s", SUIT_SPADES);
    P->phase = EC_PROPOSE;
    CHECK(ecarte_script(&G, "x") == 0);         /* not the dealer's turn to answer */
    CHECK(ecarte_script(&G, "9h"));             /* played at once: forced */
    CHECK(P->forced && P->turn == 1);
    CHECK(ecarte_can_play(P, card_of("Ah")) && ecarte_can_play(P, card_of("Qh")));   /* both beat the 9 */
    CHECK(!ecarte_can_play(P, card_of("8s")) && !ecarte_can_play(P, card_of("Jc")));
    CHECK(ecarte_script(&G, "Qh") && P->last_winner == 1 && P->turn == 1);
    CHECK(ecarte_script(&G, "Jc"));
    CHECK(ecarte_can_play(P, card_of("7c")) && !ecarte_can_play(P, card_of("Kh")));   /* the 7 cannot beat: follows */
    CHECK(ecarte_script(&G, "7c") && P->last_winner == 1);
    CHECK(ecarte_script(&G, "9d"));
    CHECK(!ecarte_can_play(P, card_of("Kh")) && ecarte_can_play(P, card_of("8d")));
    CHECK(ecarte_script(&G, "8d Ah"));          /* player 1 leads the ace of hearts */
    CHECK(!ecarte_can_play(P, card_of("7s")) && ecarte_script(&G, "Kh"));   /* the king must beat it */
    CHECK(P->last_winner == 0 && P->turn == 0);
    CHECK(ecarte_script(&G, "7s 8s"));
    CHECK(P->phase == EC_ROUND && P->won[1] == 4 && P->won[0] == 1);
    CHECK(P->pts[1] == 2 && P->total[1] == 2);  /* the elder played at once and lost: 2 */
    set_hands("Kh Qh Jh 7c 8c", "7h 8h 9h Kc Qc", SUIT_DIAMONDS);
    P->phase = EC_PLAY;
    CHECK(ecarte_script(&G, "Kh 7h Qh 8h Jh 9h 8c Qc"));   /* the vole? no: player 1 wins the fourth */
    CHECK(P->won[1] == 1 && ecarte_script(&G, "Kc 7c") && P->won[0] == 3 && P->pts[0] == 1);
    set_hands("Kh Qh Jh Kc Qc", "7h 8h 9h 7c 8c", SUIT_DIAMONDS);
    P->phase = EC_PLAY;
    CHECK(ecarte_script(&G, "Kh 7h Qh 8h Jh 9h Kc 7c Qc 8c"));
    CHECK(P->won[0] == 5 && P->pts[0] == 2 && P->total[0] == 2);   /* the vole */
    /* trumping when void; the trump wins */
    set_hands("Kh 9h 7c 8d 7s", "Qd Ad Jc 9d 8s", SUIT_SPADES);
    P->phase = EC_PLAY;
    CHECK(ecarte_script(&G, "Kh"));
    CHECK(!ecarte_can_play(P, card_of("Qd")) && ecarte_can_play(P, card_of("8s")));
    CHECK(ecarte_script(&G, "8s") && P->last_winner == 1);
    /* no suit, no trump: anything, the leader wins */
    set_hands("Kh 9h 7c 8d 7s", "Qd Ad Jc 9d 8d", SUIT_SPADES);
    P->phase = EC_PLAY;
    CHECK(ecarte_script(&G, "9h") && ecarte_can_play(P, card_of("Jc")) && ecarte_script(&G, "Jc"));
    CHECK(P->last_winner == 0);
}

static void test_scoring(void)
{
    /* the elder played at once and took two tricks: the dealer scores 2 */
    set_hands("7h 8h 9h Kc Qc", "Kh Qh Jh 7c 8c", SUIT_DIAMONDS);
    P->phase = EC_PROPOSE;
    CHECK(ecarte_script(&G, "Kc 7c Qc 8c 7h Jh Qh 8h Kh 9h"));
    CHECK(P->phase == EC_ROUND && P->won[0] == 2 && P->won[1] == 3 && P->pts[1] == 2 && P->total[1] == 2);
    /* the dealer refused and lost: the elder scores 2 */
    set_hands("Kh Qh Jh 7c 8c", "7h 8h 9h Kc Qc", SUIT_DIAMONDS);
    P->phase = EC_ANSWER, P->turn = 1, P->asked = 1;
    CHECK(ecarte_script(&G, "x"));
    CHECK(P->refused && P->phase == EC_PLAY && P->turn == 0);
    CHECK(ecarte_script(&G, "Kh 7h Qh 8h Jh 9h 7c Kc Qc 8c"));
    CHECK(P->won[0] == 3 && P->pts[0] == 2);
    /* the king of trumps held: 1 before the play; three tricks: 1 */
    set_hands("Kd Qh Jh 7c 8c", "7h 8h 9h Kc Qc", SUIT_DIAMONDS);
    P->phase = EC_ANSWER, P->turn = 1, P->asked = 2;
    CHECK(ecarte_script(&G, "x"));
    CHECK(P->king[0] == 1 && P->total[0] == 1 && !P->refused);
    CHECK(ecarte_script(&G, "Qh 7h Jh 8h Kd 9h 7c Kc Qc 8c"));
    CHECK(P->won[0] == 3 && P->pts[0] == 1 && P->score[0] == 2 && P->total[0] == 2);
    /* the end: 5 points */
    set_hands("Kd Qh Jh 7c 8c", "7h 8h 9h Kc Qc", SUIT_DIAMONDS);
    P->total[0] = 4;
    P->phase = EC_ANSWER, P->turn = 1, P->asked = 2;
    CHECK(ecarte_script(&G, "x") && P->phase == EC_OVER && ecarte_winner(P) == 0);   /* by the king */
}

static void test_undo(void)
{
    ecarte_new(&G, 77);
    CHECK(ecarte_script(&G, "q y . . . ."));
    int n = G.n;
    ecarte_pos before = G.pos;
    CHECK(ecarte_script(&G, ". ."));
    CHECK(ecarte_undo(&G) && ecarte_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (ecarte_undo(&G)) {}
    ecarte_new(&H, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    CHECK(ecarte_script(&G, "!") && P->phase == EC_OVER && ecarte_winner(P) >= 0);
}

static void test_random_games(void)
{
    int games = 0, voles = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        ecarte_new(&G, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0;
        while (P->phase != EC_OVER && ok && steps++ < 20000) {
            int cards = P->n[0] + P->n[1] + P->ntrick + P->nstock + P->thrown + 2 * P->tricks + 1;
            ok &= cards == 32;
            switch (P->phase) {
            case EC_ROUND:
                ok &= P->won[0] + P->won[1] == 5 && P->pts[0] + P->pts[1] >= 1;
                voles += P->pts[0] + P->pts[1] == 2;
                ok &= ecarte_play(&G, ECA_NEXT);
                break;
            case EC_ANSWER: ok &= ecarte_play(&G, cards_rand(&r) % 3 ? ECA_YES : ECA_NO); break;
            case EC_DISCARD: {
                int lo = ecarte_throw_min(P), hi = ecarte_throw_max(P);
                int k = lo + (int)(cards_rand(&r) % (uint32_t)(hi - lo + 1)), shift = (int)(cards_rand(&r) % (6u - k));
                ok &= ecarte_play(&G, ECA_DISCARD | ((1 << k) - 1) << shift);
                break;
            }
            case EC_PROPOSE:
                if (P->nstock && cards_rand(&r) % 3) { ok &= ecarte_play(&G, ECA_ASK); break; }
                /* fall through */
            default: {
                uint8_t legal[5];
                int k = 0;
                for (int i = 0; i < P->n[P->turn]; i++)
                    if (ecarte_can_play(P, P->hand[P->turn][i])) legal[k++] = P->hand[P->turn][i];
                ok &= k > 0;
                if (k) ok &= ecarte_play(&G, ECA_PLAY | legal[cards_rand(&r) % (uint32_t)k]);
            }
            }
        }
        ok &= P->phase == EC_OVER && (P->total[0] >= 5) != (P->total[1] >= 5);
        CHECK(ok);
        if (!ok) { printf("  seed %u\n", seed); break; }
        games++;
    }
    CHECK(games == 300);
    printf("  %d random games, %d deals for 2\n", games, voles);
}

int main(void)
{
    test_deal();
    test_exchange();
    test_play();
    test_scoring();
    test_undo();
    test_random_games();
    printf("ecarte: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
