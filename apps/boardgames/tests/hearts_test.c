/*
 * Hearts: the rules' test (the deal for 3 to 6 players, the passing directions and the exchange, the 2 of clubs
 * leading, following suit, no points on the first trick, hearts broken, the queen of spades, shooting the moon, the
 * end at 100 and a tie, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/hearts.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static hgame G, H;
static hpos *P = &G.pos;

static uint32_t play_of(const char *c)
{
    const char *s = c;
    return HA_PLAY | (uint32_t)trick_parse(&s);
}

/* every card of the deal once; the right cards out; the hands sorted and equal */
static void test_deal(void)
{
    static const int sizes[7] = {0, 0, 0, 17, 13, 10, 8};
    for (int np = 3; np <= 6; np++) {
        hearts_new(&G, np, 42);
        int seen[256] = {0}, total = 0, sorted = 1;
        for (int pl = 0; pl < np; pl++) {
            CHECK(P->n[pl] == sizes[np]);
            for (int i = 0; i < P->n[pl]; i++) seen[P->hand[pl][i]]++, total++;
            for (int i = 1; i < P->n[pl]; i++)
                sorted &= trick_sort_key(P->hand[pl][i - 1]) < trick_sort_key(P->hand[pl][i]);
        }
        CHECK(sorted && total == np * sizes[np]);
        int pts = 0, once = 1;
        for (int c = 0; c < 256; c++) once &= seen[c] <= 1, pts += seen[c] * hearts_points(c);
        CHECK(once && pts == 26 && seen[HEARTS_LEAD] == 1);
        CHECK(seen[CARD(SUIT_DIAMONDS, 2)] == (np == 4));
        CHECK(seen[CARD(SUIT_SPADES, 2)] == (np < 5));
        CHECK(seen[CARD(SUIT_SPADES, 3)] == (np < 6) && seen[CARD(SUIT_DIAMONDS, 3)] == (np < 6));
        CHECK(P->phase == HP_PASS && P->turn == 0 && P->dir == PASS_LEFT);
    }
    hearts_new(&H, 4, 42);
    hearts_new(&G, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    hearts_new(&H, 4, 43);
    CHECK(memcmp(G.pos.hand, H.pos.hand, sizeof G.pos.hand) != 0);
}

/* the directions round after round: four or six players left, right, across, none; three or five left, right, none */
static void test_directions(void)
{
    static const int four[5] = {PASS_LEFT, PASS_RIGHT, PASS_ACROSS, PASS_HOLD, PASS_LEFT};
    static const int three[4] = {PASS_LEFT, PASS_RIGHT, PASS_HOLD, PASS_LEFT};
    hearts_new(&G, 4, 9);
    for (int r = 0; r < 5; r++) {
        CHECK(P->dir == four[r] && P->round == r);
        CHECK((P->phase == HP_PASS) == (four[r] != PASS_HOLD));
        CHECK(hearts_script(&G, "#") && (P->phase == HP_ROUND || P->phase == HP_OVER));
        if (P->phase == HP_OVER) break;
        CHECK(hearts_script(&G, "n"));
    }
    hearts_new(&G, 3, 9);
    for (int r = 0; r < 4; r++) {               /* three rounds: 78 points at most, the game goes on */
        CHECK(P->dir == three[r]);
        if (r < 3) CHECK(hearts_script(&G, "# n"));
    }
    hearts_new(&G, 4, 1);
    P->dir = PASS_LEFT;
    CHECK(hearts_target(P, 0) == 1 && hearts_target(P, 3) == 0);
    P->dir = PASS_RIGHT;
    CHECK(hearts_target(P, 0) == 3 && hearts_target(P, 2) == 1);
    P->dir = PASS_ACROSS;
    CHECK(hearts_target(P, 0) == 2 && hearts_target(P, 3) == 1);
    hearts_new(&G, 6, 1);
    P->dir = PASS_ACROSS;
    CHECK(hearts_target(P, 1) == 4);
}

/* passing: each chooses in turn, the cards change hands once all have chosen; then the 2 of clubs' holder leads */
static void test_pass(void)
{
    hearts_new(&G, 4, 5);
    uint8_t chose[4][3];
    CHECK(!hearts_legal(P, HA_PASS3(P->hand[0][0], P->hand[0][0], P->hand[0][1])));   /* the same card twice */
    CHECK(!hearts_legal(P, HA_PASS3(P->hand[1][0], P->hand[0][0], P->hand[0][1])));   /* not in the hand */
    CHECK(!hearts_legal(P, play_of("2c")));
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->turn == pl && P->phase == HP_PASS);
        int n = P->n[pl];
        for (int i = 0; i < 3; i++) chose[pl][i] = P->hand[pl][n - 1 - i];   /* the three highest */
        CHECK(hearts_play(&G, HA_PASS3(chose[pl][0], chose[pl][1], chose[pl][2])));
        if (pl < 3) CHECK(P->n[pl] == 10 && P->got[(pl + 1) % 4][0] == CARD_NONE);
    }
    CHECK(P->phase == HP_PLAY);
    for (int pl = 0; pl < 4; pl++) {
        int to = (pl + 1) % 4;
        CHECK(P->n[to] == 13);
        for (int i = 0; i < 3; i++) {
            CHECK(trick_find(P->hand[to], 13, chose[pl][i]) >= 0 && P->got[to][i] == chose[pl][i]);
            CHECK(trick_find(P->hand[pl], 13, chose[pl][i]) < 0);
        }
    }
    CHECK(trick_find(P->hand[P->turn], 13, HEARTS_LEAD) >= 0 && P->leader == P->turn);
    for (int i = 0; i < 13; i++)
        CHECK(hearts_can_play(P, P->hand[P->turn][i]) == (P->hand[P->turn][i] == HEARTS_LEAD));
}

/* hands set by hand, for four players, the 2 of clubs with player 0, play phase */
static void set_hands(const char *h0, const char *h1, const char *h2, const char *h3)
{
    hearts_new(&G, 4, 3);
    const char *hs[4] = {h0, h1, h2, h3};
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
    }
    P->phase = HP_PLAY;
    P->turn = P->leader = 0;
    G.n = 0;
}

static void test_tricks(void)
{
    set_hands("2c Ac 3h 4d", "Kc Qs 5h 5d", "3c 2h 6h 6d", "4h Qh Kh Ah");
    CHECK(!hearts_script(&G, "Ac"));            /* the 2 of clubs leads */
    CHECK(hearts_script(&G, "2c"));
    CHECK(!hearts_script(&G, "Qs") && !hearts_script(&G, "5h"));   /* follow suit */
    CHECK(hearts_script(&G, "Kc 3c"));
    CHECK(P->turn == 3);
    CHECK(hearts_script(&G, "4h"));             /* nothing but hearts: allowed on the first trick */
    CHECK(P->tricks == 1 && P->last_winner == 1 && P->turn == 1 && P->taken[1] == 1 && P->broken);
    CHECK(P->last[0] == HEARTS_LEAD && P->last_leader == 0);
    /* the next trick: player 1 leads; the queen of spades on a diamond lead */
    CHECK(hearts_script(&G, "5d 6d Qh 4d"));    /* player 3 void: a heart */
    CHECK(P->last_winner == 2 && P->taken[2] == 1);
    set_hands("2c Ac 3h 4d", "Kc Qs 5h 5d", "3d 2h 6h 6d", "5c Qh Kh Ah");
    CHECK(hearts_script(&G, "2c Kc"));
    CHECK(!hearts_script(&G, "2h") && !hearts_script(&G, "6h"));   /* first trick: no hearts while holding a diamond */
    CHECK(hearts_script(&G, "3d 5c"));
    CHECK(P->last_winner == 1 && P->taken[1] == 0 && !P->broken);
    CHECK(!hearts_can_play(P, CARD(SUIT_HEARTS, 5)));   /* hearts not broken: no heart lead */
    CHECK(hearts_can_play(P, HEARTS_QUEEN) && hearts_can_play(P, CARD(SUIT_DIAMONDS, 5)));
    CHECK(hearts_script(&G, "Qs"));             /* the queen leads; player 2 void in spades: a heart breaks them */
    CHECK(hearts_script(&G, "2h"));
    CHECK(P->broken);
    CHECK(hearts_script(&G, "Qh 3h"));          /* player 0: no spade; the queen wins her own trick */
    CHECK(P->last_winner == 1 && P->taken[1] == 13 + 3);
    set_hands("2c Ac 3h 4d", "Kc Qs 5h 5d", "3c 2h 6h 6d", "Ad Qh Kh Ah");
    CHECK(hearts_script(&G, "2c Kc 3c"));
    CHECK(!hearts_script(&G, "Qh") && hearts_script(&G, "Ad"));
    /* leading hearts with nothing else */
    set_hands("2c 3h", "3c 4h", "4c 5h", "5c 6h");
    CHECK(hearts_script(&G, "2c 3c 4c 5c"));
    CHECK(P->turn == 3 && !P->broken && hearts_can_play(P, CARD(SUIT_HEARTS, 6)));
}

/* scoring: the points taken; the moon; the end of the game at 100, lowest wins, a tie shared */
static void test_scoring(void)
{
    hearts_new(&G, 4, 11);
    CHECK(hearts_script(&G, "#"));
    int sum = 0;
    for (int pl = 0; pl < 4; pl++) {
        sum += P->score[pl];
        CHECK(P->score[pl] == P->total[pl]);
    }
    CHECK(sum == 26 || (P->moon != TRICK_NONE && sum == 78));
    CHECK(P->phase == HP_ROUND && P->round == 1 && P->turn == 1);
    /* the moon: player 2 takes every point */
    set_hands("2c 3c", "4c 5c", "Ac Qs", "6c 7c");
    for (int pl = 0; pl < 4; pl++) P->total[pl] = 10;
    P->tricks = 12;
    P->taken[2] = 13;                           /* the hearts so far */
    CHECK(hearts_script(&G, "2c 4c Ac 6c"));
    CHECK(P->turn == 2 && hearts_script(&G, "Qs 7c 3c 5c"));
    CHECK(P->moon == 2 && P->score[2] == 0 && P->score[0] == 26 && P->total[3] == 36 && P->total[2] == 10);
    CHECK(P->phase == HP_ROUND);
    /* the end: someone reaches 100 */
    set_hands("2c 3c", "4c 5c", "Ac Qs", "6c 7c");
    P->tricks = 12;
    P->total[0] = 90, P->total[1] = 40, P->total[2] = 90, P->total[3] = 40;
    CHECK(hearts_script(&G, "2c 4c Ac 6c Qs 7c 3c 5c"));
    CHECK(P->phase == HP_OVER && P->total[2] == 103 && hearts_winner(P) == -1);   /* 40 and 40: a tie */
    set_hands("2c 3c", "4c 5c", "Ac Qs", "6c 7c");
    P->tricks = 12;
    P->total[0] = 90, P->total[1] = 40, P->total[2] = 90, P->total[3] = 41;
    CHECK(hearts_script(&G, "2c 4c Ac 6c Qs 7c 3c 5c"));
    CHECK(P->phase == HP_OVER && hearts_winner(P) == 1 && !hearts_legal(P, HA_NEXT));
}

/* undo: back one action at a time, the same position as playing to there */
static void test_undo(void)
{
    hearts_new(&G, 5, 77);
    CHECK(hearts_script(&G, ". . . . . . . ."));
    int n = G.n;
    hpos before = G.pos;
    CHECK(hearts_script(&G, ". ."));
    CHECK(hearts_undo(&G) && hearts_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (hearts_undo(&G)) {}
    hearts_new(&H, 5, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    for (int np = 3; np <= 6; np++) {           /* "!": plain actions to the end of the game */
        hearts_new(&G, np, 5u + np);
        CHECK(hearts_script(&G, "!") && P->phase == HP_OVER);
    }
}

/* whole games from seeds, random legal actions: the cards are kept, the points add up, a game ends */
static uint32_t rng(uint32_t *s) { return cards_rand(s); }

static void test_random_games(void)
{
    int games = 0, moons = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int np = 3 + (int)(seed % 4);
        hearts_new(&G, np, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0;
        while (P->phase != HP_OVER && ok && steps++ < 100000) {
            if (P->phase == HP_ROUND) {
                int s = 0;
                for (int pl = 0; pl < np; pl++) s += P->score[pl];
                ok &= s == 26 || s == 26 * (np - 1);
                moons += P->moon != TRICK_NONE;
                ok &= hearts_play(&G, HA_NEXT);
                continue;
            }
            int pl = P->turn, n = P->n[pl];
            if (P->phase == HP_PASS) {
                int a = (int)(rng(&r) % n), b = (a + 1 + (int)(rng(&r) % (n - 1))) % n, c = 0;
                while (c == a || c == b) c++;
                ok &= hearts_play(&G, HA_PASS3(P->hand[pl][a], P->hand[pl][b], P->hand[pl][c]));
                continue;
            }
            uint8_t legal[HEARTS_HAND];
            int k = 0;
            for (int i = 0; i < n; i++)
                if (hearts_can_play(P, P->hand[pl][i])) legal[k++] = P->hand[pl][i];
            ok &= k > 0;
            if (!k) break;
            int total = 0;                      /* the cards in the hands and the trick: one deal's */
            for (int q = 0; q < np; q++) total += P->n[q];
            ok &= total + P->ntrick == P->size * np - P->tricks * np;
            ok &= hearts_play(&G, HA_PLAY | legal[rng(&r) % k]);
        }
        ok &= P->phase == HP_OVER;
        int top = 0;
        for (int pl = 0; pl < np; pl++) top = P->total[pl] > top ? P->total[pl] : top;
        ok &= top >= HEARTS_LIMIT;
        CHECK(ok);
        if (!ok) { printf("  seed %u players %d\n", seed, np); break; }
        games++;
    }
    CHECK(games == 300);
    printf("  %d random games, %d moons\n", games, moons);
}

int main(void)
{
    test_deal();
    test_directions();
    test_pass();
    test_tricks();
    test_scoring();
    test_undo();
    test_random_games();
    printf("hearts: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
