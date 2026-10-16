/*
 * Whist: the rules' test (the deal and the turned card, following suit, trumps, the book, honours and the point
 * short, the end at 5 or 10, German whist's stock and its draws, undo, the notation; whole random games from seeds).
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/tricks.h"
#include "games/whist.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static whist_game G, H;
static whist_pos *P = &G.pos;

static void hand_set(int pl, const char *s)
{
    P->n[pl] = 0;
    while (*s) {
        while (*s == ' ') s++;
        int c = trick_parse(&s);
        if (c == CARD_NONE) break;
        P->hand[pl][P->n[pl]++] = (uint8_t)c;
    }
    trick_sort(P->hand[pl], P->n[pl]);
}

static void test_deal(void)
{
    whist_new(&G, 4, WV_SHORT, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->n[pl] == 13);
        for (int i = 0; i < 13; i++) seen[P->hand[pl][i]]++, total++;
    }
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(once && total == 52 && P->phase == WP_PLAY && P->dealer == 0 && P->turn == 1 && P->leader == 1);
    CHECK(trick_find(P->hand[0], 13, P->up) >= 0 && P->trump == CARD_SUIT(P->up));
    CHECK(P->target == 5 && P->nstock == 0);
    whist_new(&H, 4, WV_SHORT, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    whist_new(&G, 4, WV_LONG, 42);
    CHECK(P->target == 10 && whist_honours_count(P));
    whist_new(&G, 4, WV_SHORT, 42);
    CHECK(!whist_honours_count(P));
    whist_new(&G, 2, WV_HONOURS, 7);            /* German whist: 13 each, a stock of 26, its top face up */
    CHECK(P->n[0] == 13 && P->n[1] == 13 && P->nstock == 26 && P->up == P->stock[25]);
    CHECK(P->trump == CARD_SUIT(P->up) && P->turn == 1 && !whist_honours_count(P));
}

static void test_follow(void)
{
    whist_new(&G, 4, WV_SHORT, 3);
    P->trump = SUIT_HEARTS;
    hand_set(1, "2c 9c Ah 3d");
    hand_set(2, "Kc 2h 4d 5d");
    hand_set(3, "Th Jh Qh 7s");
    hand_set(0, "Ac 8c 9d Td");
    P->turn = P->leader = 1;
    CHECK(whist_script(&G, "9c"));
    CHECK(!whist_can_play(P, CARD(SUIT_HEARTS, 2)) && whist_can_play(P, CARD(SUIT_CLUBS, RANK_KING)));
    CHECK(whist_script(&G, "Kc"));
    CHECK(whist_can_play(P, CARD(SUIT_SPADES, 7)) && whist_can_play(P, CARD(SUIT_HEARTS, 10)));
    CHECK(whist_script(&G, "Th Ac"));           /* the trump wins over the ace led's suit */
    CHECK(P->last_winner == 3 && P->turn == 3 && P->won[3] == 1 && P->tricks == 1);
    CHECK(!whist_script(&G, "5d"));             /* not player 3's card */
}

/* whole games: the cards kept, 13 tricks a deal (26 for two), the book, honours, the end; undo replays */
static void test_games(void)
{
    int ok = 1, deals = 0, ended = 0, honours_seen = 0, short_rule = 0;
    for (int gn = 0; gn < 120; gn++) {
        int players = gn % 4 == 3 ? 2 : 4, variant = gn % 3;
        whist_new(&G, players, variant, 900 + gn);
        uint32_t rng = 77 + gn;
        int16_t before[2] = {0, 0};
        int honours[2] = {P->honours[0], P->honours[1]};
        for (int guard = 0; guard < 20000 && P->phase != WP_OVER; guard++) {
            if (P->phase == WP_ROUND) {
                whist_play(&G, WA_NEXT);
                before[0] = P->score[0], before[1] = P->score[1];
                honours[0] = P->honours[0], honours[1] = P->honours[1];
                continue;
            }
            uint8_t L[13];
            int k = 0;
            for (int i = 0; i < P->n[P->turn]; i++)
                if (whist_can_play(P, P->hand[P->turn][i])) L[k++] = P->hand[P->turn][i];
            if (!k) { ok = 0; break; }
            rng = rng * 1103515245u + 12345u;
            int tricks = P->tricks;
            whist_play(&G, WA_PLAY | L[(rng >> 16) % k]);
            if (P->phase == WP_PLAY) {
                int held = P->ntrick;
                for (int s = 0; s < players; s++) held += P->n[s];
                if (players == 2) {
                    ok &= held + P->nstock + 2 * P->tricks == 52;
                    if (P->tricks != tricks && P->drew[0] != CARD_NONE) {
                        ok &= trick_find(P->hand[P->last_winner], P->n[P->last_winner], P->drew[0]) >= 0;
                        ok &= P->n[0] == 13 && P->n[1] == 13;
                    }
                } else
                    ok &= held + 4 * P->tricks == 52;
                continue;
            }
            deals++;                            /* the deal is over: its points */
            ok &= P->tricks == (players == 2 ? 26 : 13);
            int t[2], hon_got = 0, reached = 0;
            for (int s = 0; s < 2; s++) {
                t[s] = whist_side_tricks(P, s);
                ok &= P->pts[s] == (t[s] > 6 ? t[s] - 6 : 0);
                reached |= before[s] + P->pts[s] >= P->target;
            }
            ok &= t[0] + t[1] == 13;
            for (int s = 0; s < 2; s++) {
                int h = honours[s] == 4 ? 4 : honours[s] == 3 ? 2 : 0, want = 0;
                if (whist_honours_count(P) && !reached && before[s] < P->target - 1) want = h;
                if (h && whist_honours_count(P) && before[s] >= P->target - 1) short_rule++;
                ok &= P->hon[s] == want && P->score[s] == before[s] + P->pts[s] + P->hon[s];
                hon_got += P->hon[s] != 0;
            }
            honours_seen += hon_got;
        }
        if (P->phase == WP_OVER) {
            ended++;
            int w = whist_winner(P);
            ok &= w >= 0 && P->score[w] >= P->target;
        }
        if (gn < 8) {                           /* undo: the same as replaying one action less */
            whist_game keep = G;
            whist_undo(&G);
            whist_new(&H, keep.players, keep.variant, keep.seed);
            for (int i = 0; i < keep.n - 1; i++) whist_play(&H, keep.act[i]);
            ok &= !memcmp(&G.pos, &H.pos, sizeof H.pos);
        }
    }
    printf("     %d deals, %d games ended, %d honours scored, %d honours kept out by the point short\n", deals, ended,
           honours_seen, short_rule);
    CHECK(ok && ended == 120 && honours_seen > 5 && short_rule > 0);
}

static void test_german(void)
{
    whist_new(&G, 2, WV_SHORT, 11);
    int up = P->up, next = P->stock[24], lead = P->turn;
    CHECK(whist_script(&G, "."));
    CHECK(whist_script(&G, "."));
    int w = P->last_winner;
    CHECK(P->drew[0] == up && P->drew[1] == next && P->nstock == 24 && P->up == P->stock[23]);
    CHECK(trick_find(P->hand[w], 13, up) >= 0 && trick_find(P->hand[1 - w], 13, next) >= 0 && P->turn == w);
    CHECK(lead == 1 && P->late[w] == 0 && P->won[w] == 1);
    CHECK(whist_script(&G, "#"));
    CHECK(P->tricks == 26 && P->late[0] + P->late[1] == 13 && P->won[0] + P->won[1] == 26);
    CHECK(P->phase == WP_ROUND && P->turn == 0);   /* the next deal: player 0 leads (player 1 deals) */
    CHECK(whist_script(&G, "n") && P->dealer == 1 && P->turn == 0);
}

static void test_notation(void)
{
    whist_new(&G, 4, WV_LONG, 5);
    CHECK(whist_script(&G, "!"));
    CHECK(P->phase == WP_OVER && whist_winner(P) >= 0);
    CHECK(!whist_script(&G, "n"));
    whist_new(&G, 4, WV_SHORT, 5);
    CHECK(!whist_script(&G, "Zz") && !whist_script(&G, "n"));
    CHECK(whist_script(&G, "# n") && P->round == 1 && P->dealer == 1 && P->turn == 2);
}

int main(void)
{
    test_deal();
    test_follow();
    test_games();
    test_german();
    test_notation();
    printf("whist: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
