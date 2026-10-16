/*
 * Pinochle: the rules' test (the deal, the stock and its turned card, the dix, melds and their reuse, the dix's
 * exchange, the strict play, counters and the last trick, the auction and the stuck dealer, the melds counted for
 * four, going set, the ends at 100 and 150, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pinochle.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static pino_game G, H;
static pino_pos *P = &G.pos;

static int parse(const char *s, uint8_t *out)
{
    int n = 0;
    while (*s) {
        while (*s == ' ') s++;
        int c = trick_parse(&s);
        if (c == CARD_NONE) break;
        out[n++] = (uint8_t)c;
    }
    return n;
}

static void hand_set(int pl, const char *s, int flags)
{
    P->n[pl] = (uint8_t)parse(s, P->hand[pl]);
    for (int i = 0; i < P->n[pl]; i++) P->flag[pl][i] = (uint8_t)flags;
}

static int legal_all(const pino_pos *p, int *out)   /* every legal action (a few bids) */
{
    int n = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        for (int t = 0; t < 2; t++) {
            int a = PA_PLAY | p->hand[p->turn][i] | (t ? 0x80 : 0);
            if (pinochle_legal(p, a)) out[n++] = a;
        }
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < 4; s++)
            if (pinochle_legal(p, PA_MELD_OF(k, s))) out[n++] = PA_MELD_OF(k, s);
    static const int other[] = {PA_SKIP, PA_PASS, PA_GO, PA_NEXT, PA_TRUMP, PA_TRUMP | 1, PA_TRUMP | 2, PA_TRUMP | 3};
    for (int k = 0; k < 8; k++)
        if (pinochle_legal(p, other[k])) out[n++] = other[k];
    for (int b = 0; b < 4; b++) {
        int v = (p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1) + b;
        if (pinochle_legal(p, PA_BID | v)) out[n++] = PA_BID | v;
    }
    return n;
}

static void test_deal(void)
{
    pinochle_new(&G, 2, 42);
    int cnt[256] = {0}, total = 0;
    for (int pl = 0; pl < 2; pl++) {
        CHECK(P->n[pl] == 12);
        for (int i = 0; i < 12; i++) cnt[P->hand[pl][i]]++, total++;
    }
    for (int i = 0; i < P->nstock; i++) cnt[P->stock[i]]++, total++;
    int twice = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 9; r <= 14; r++) twice &= cnt[CARD(s, r == 14 ? RANK_ACE : r)] == 2;
    CHECK(twice && total == 48 && P->nstock == 24 && P->stock[0] == P->up && P->trump == CARD_SUIT(P->up));
    CHECK(P->phase == PP_PLAY && P->dealer == 0 && P->turn == 1);
    int dix = 0;
    for (uint32_t seed = 1; seed < 400 && !dix; seed++) {   /* a 9 turned: the dealer's point */
        pinochle_new(&G, 2, seed);
        if (CARD_RANK(P->up) == 9) { dix = 1; CHECK(P->score[0] == 1 && P->score[1] == 0 && P->melds[0] == 1); }
    }
    CHECK(dix);
    pinochle_new(&G, 4, 42);
    CHECK(P->phase == PP_BID && P->turn == 1 && P->nstock == 0 && P->trump == PN_NONE);
    for (int pl = 0; pl < 4; pl++) CHECK(P->n[pl] == 12);
    pinochle_new(&H, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_meld_names(void)
{
    uint8_t m[8];
    CHECK(pinochle_meld_of(m, parse("Ah Th Kh Qh Jh", m), SUIT_HEARTS) == PA_MELD_OF(PM_RUN, 0));
    CHECK(pinochle_meld_of(m, parse("Qh Kh", m), SUIT_HEARTS) == PA_MELD_OF(PM_ROYAL, 0));
    CHECK(pinochle_meld_of(m, parse("Kc Qc", m), SUIT_HEARTS) == PA_MELD_OF(PM_MARRIAGE, SUIT_CLUBS));
    CHECK(pinochle_meld_of(m, parse("Jd Qs", m), SUIT_HEARTS) == PA_MELD_OF(PM_PINOCHLE, 0));
    CHECK(pinochle_meld_of(m, parse("Jd Qs Qs Jd", m), SUIT_HEARTS) == PA_MELD_OF(PM_DOUBLE, 0));
    CHECK(pinochle_meld_of(m, parse("Ah As Ad Ac", m), SUIT_HEARTS) == PA_MELD_OF(PM_ACES, 0));
    CHECK(pinochle_meld_of(m, parse("9h", m), SUIT_HEARTS) == PA_MELD_OF(PM_DIX, 0));
    CHECK(!pinochle_meld_of(m, parse("9c", m), SUIT_HEARTS) && !pinochle_meld_of(m, parse("Ah As Ad", m), 1));
    CHECK(!pinochle_meld_of(m, parse("Kh Qc", m), SUIT_HEARTS));
    /* four players' melds: a run and a second royal marriage, double pinochle, aces around, the dix */
    int k, pts = pinochle_hand_melds(m, 0, 0, NULL, NULL);
    CHECK(pts == 0);
    uint8_t h[12], c[12];
    int n = parse("Ah Th Kh Kh Qh Qh Jh 9h Qs Qs Jd Jd", h);
    pts = pinochle_hand_melds(h, n, SUIT_HEARTS, c, &k);
    CHECK(pts == 15 + 4 + 1 + 30 && k == 12);
    n = parse("As Ah Ad Ac Ks Kh Kd Kc Qs Jd 9c 9c", h);
    pts = pinochle_hand_melds(h, n, SUIT_CLUBS, c, &k);
    CHECK(pts == 10 + 8 + 2 + 4 + 2 && k == 12);   /* aces, kings, the spades' marriage, pinochle, two dix */
    n = parse("As As Ah Ah Ad Ad Ac Ac Ks Qs Jd 9h", h);
    CHECK(pinochle_hand_melds(h, n, SUIT_HEARTS, c, &k) == 100 + 2 + 4 + 1);
}

static void meld_setup(void)
{
    pinochle_new(&G, 2, 9);
    P->trump = SUIT_HEARTS;
    P->stock[0] = P->up = CARD(SUIT_HEARTS, RANK_ACE);
    hand_set(0, "Ah Th Kh Qh Jh 9h Qs Jd Qd Qc Ks", 0);   /* a trick just won: 11 cards */
    for (int i = 1; i < P->nstock; i++) P->stock[i] = CARD(SUIT_CLUBS, 9);   /* nothing that melds drawn */
    P->phase = PP_MELD;
    P->turn = P->last_winner = 0;
}

static void test_melds(void)
{
    meld_setup();
    int s0 = P->score[0], n0 = P->nstock;
    CHECK(pinochle_can_meld(P));
    CHECK(pinochle_script(&G, "MKhQh"));
    CHECK(P->score[0] == s0 + 4 && P->nstock == n0 - 2 && P->phase == PP_PLAY && P->turn == 0);
    int table = 0;
    for (int i = 0; i < P->n[0]; i++) table += (P->flag[0][i] & PK_TABLE) != 0;
    CHECK(table == 2 && P->n[0] == 12);         /* 11 and the card drawn */
    P->phase = PP_MELD;                         /* the run with the royal marriage's king and queen: not again */
    CHECK(!pinochle_legal(P, PA_MELD_OF(PM_RUN, 0)));
    CHECK(pinochle_script(&G, "MQsJd"));        /* the pinochle; then the queens around, its queen and the royal one */
    P->phase = PP_MELD;
    CHECK(pinochle_legal(P, PA_MELD_OF(PM_QUEENS, 0)));
    CHECK(!pinochle_legal(P, PA_MELD_OF(PM_PINOCHLE, 0)));
    s0 = P->score[0];
    CHECK(pinochle_script(&G, "MQsQhQdQc") && P->score[0] == s0 + 6);
    meld_setup();                               /* the dix exchanged for the turned ace */
    CHECK(pinochle_script(&G, "M9h"));
    CHECK(P->stock[0] == CARD(SUIT_HEARTS, 9) && P->up == CARD(SUIT_HEARTS, 9));
    int aces = 0;
    for (int i = 0; i < P->n[0]; i++) aces += P->hand[0][i] == CARD(SUIT_HEARTS, RANK_ACE);
    CHECK(aces >= 2);
    meld_setup();                               /* every card on the table already: none from the hand */
    hand_set(0, "Ks Qs 9c 9c", PK_TABLE | PK_AROUND);
    P->flag[0][2] = P->flag[0][3] = 0;
    CHECK(!pinochle_legal(P, PA_MELD_OF(PM_MARRIAGE, SUIT_SPADES)) && !pinochle_can_meld(P));
    P->flag[0][1] = PK_AROUND;                  /* the queen from the hand: it may */
    CHECK(pinochle_legal(P, PA_MELD_OF(PM_MARRIAGE, SUIT_SPADES)));
    CHECK(pinochle_script(&G, "x") && P->phase == PP_PLAY);
}

static void test_strict(void)
{
    uint8_t h[12], t[4], L[12];
    int n = parse("Ah 9h Kc Tc 9s", h);
    parse("Qc", t);
    int k = pinochle_strict(h, n, t, 1, SUIT_SPADES, L);
    CHECK(k == 2);                              /* beat the queen: king or ten of clubs */
    parse("Ac", t);
    k = pinochle_strict(h, n, t, 1, SUIT_SPADES, L);
    CHECK(k == 2);                              /* follow, can not beat: any club */
    parse("Kd", t);
    k = pinochle_strict(h, n, t, 1, SUIT_SPADES, L);
    CHECK(k == 1 && L[0] == CARD(SUIT_SPADES, 9));   /* void: trump */
    n = parse("Ah 9h Kc Tc 9s Ts", h);
    parse("Kd Js", t);
    k = pinochle_strict(h, n, t, 2, SUIT_SPADES, L);
    CHECK(k == 1 && L[0] == CARD(SUIT_SPADES, 10));  /* trumped: overtrump */
    parse("Kd As", t);
    k = pinochle_strict(h, n, t, 2, SUIT_SPADES, L);
    CHECK(k == 2);                              /* can not overtrump: any trump */
    n = parse("Ah 9h Kc", h);
    k = pinochle_strict(h, n, t, 2, SUIT_SPADES, L);
    CHECK(k == 3);                              /* nothing: any card */
    CHECK(pinochle_power(CARD(SUIT_HEARTS, 10), SUIT_SPADES, SUIT_HEARTS) >
          pinochle_power(CARD(SUIT_HEARTS, RANK_KING), SUIT_SPADES, SUIT_HEARTS));
}

static void test_auction(void)
{
    pinochle_new(&G, 4, 5);
    CHECK(!pinochle_legal(P, PA_BID | 19) && pinochle_legal(P, PA_BID | 20));
    CHECK(pinochle_script(&G, "p p p"));
    CHECK(P->turn == 0 && !pinochle_legal(P, PA_PASS));   /* the dealer, stuck */
    CHECK(pinochle_script(&G, "b20") && P->phase == PP_TRUMP && P->bidder == 0 && P->turn == 0);
    pinochle_new(&G, 4, 5);
    CHECK(pinochle_script(&G, "b21 b22 p b25 p b26 p"));
    CHECK(P->phase == PP_TRUMP && P->bidder == 2 && P->high == 26);
    CHECK(pinochle_script(&G, "th") && P->phase == PP_MELDS && P->trump == SUIT_HEARTS);
    int m0 = 0;
    for (int q = 0; q < 4; q++) m0 += q % 2 == 0 ? P->meldpts[q] : 0;
    CHECK(P->melds[0] == m0);
    CHECK(pinochle_script(&G, "g") && P->phase == PP_PLAY && P->turn == 2 && P->leader == 2);
    CHECK(pinochle_script(&G, "#"));
    int took0 = P->won[0] + P->won[2] > 0, total0 = (took0 ? P->melds[0] : 0) + P->counters[0];
    CHECK(P->counters[0] + P->counters[1] == 25);
    CHECK(total0 >= 26 ? P->pts[0] == total0 && !P->set : P->pts[0] == -26 && P->set);
}

/* whole games: the cards kept, 25 counters a deal, the scores, the end; undo replays */
static void test_games(void)
{
    int ok = 1, ended = 0, deals = 0, melds = 0, sets = 0;
    for (int gn = 0; gn < 80; gn++) {
        int players = gn % 2 ? 4 : 2;
        pinochle_new(&G, players, 700 + gn);
        uint32_t rng = 31 + gn;
        int16_t before[2] = {P->score[0], P->score[1]}, mscore[2] = {0, 0};
        for (int guard = 0; guard < 30000 && P->phase != PP_OVER; guard++) {
            int L[64], k = legal_all(P, L);
            if (!k) { ok = 0; printf("no action, phase %d\n", P->phase); break; }
            rng = rng * 1103515245u + 12345u;
            int a = L[(rng >> 16) % k];
            if (PA_KIND(a) == PA_BID && (rng >> 8) % 4) a = pinochle_legal(P, PA_PASS) ? PA_PASS : a;
            if (P->phase == PP_MELD && (rng >> 4) % 3 == 0) a = PA_SKIP;
            char name[32];
            pino_game keep = G;
            ok &= pinochle_name(P, a, name, sizeof name);
            H = keep;
            ok &= pinochle_script(&H, name) && H.act[H.n - 1] == a;   /* the notation round trip */
            int was = P->phase, pl = P->turn;
            int16_t sc = P->score[pinochle_side(P, pl)];
            pinochle_play(&G, a);
            if (PA_KIND(a) == PA_MELD) {
                melds++;
                ok &= P->score[pl] == sc + pinochle_meld_value(PA_ARG(a) >> 2, 2);
                mscore[pl] = (int16_t)(mscore[pl] + pinochle_meld_value(PA_ARG(a) >> 2, 2));
            }
            if (was == PP_PLAY || was == PP_MELD) {
                int held = P->nstock + P->ntrick + P->tricks * players;
                for (int q = 0; q < players; q++) held += P->n[q];
                ok &= P->phase >= PP_ROUND || held == 48;
            }
            if (PA_KIND(a) == PA_NEXT) {
                before[0] = P->score[0], before[1] = P->score[1];
                mscore[0] = mscore[1] = 0;
            }
            if (was == PP_PLAY && P->phase >= PP_ROUND) {   /* the deal's end */
                deals++;
                ok &= P->counters[0] + P->counters[1] == 25 && P->tricks == (players == 2 ? 24 : 12);
                if (players == 2)
                    for (int s = 0; s < 2; s++) {
                        ok &= P->score[s] == before[s] + mscore[s] + P->counters[s];
                    }
                else
                    for (int s = 0; s < 2; s++) ok &= P->score[s] == before[s] + P->pts[s];
                sets += P->set;
            }
        }
        if (P->phase == PP_OVER) {
            ended++;
            int w = pinochle_winner(P);
            ok &= w >= 0 && P->score[w] >= (players == 2 ? PN_TARGET2 : PN_TARGET4);
        }
        if (gn < 6) {
            pino_game keep = G;
            pinochle_undo(&G);
            pinochle_new(&H, keep.players, keep.seed);
            for (int i = 0; i < keep.n - 1; i++) pinochle_play(&H, keep.act[i]);
            ok &= !memcmp(&G.pos, &H.pos, sizeof H.pos);
        }
    }
    printf("     %d deals, %d games ended, %d melds, %d sets\n", deals, ended, melds, sets);
    CHECK(ok && ended == 80 && melds > 50 && sets > 0);
}

static void test_notation(void)
{
    pinochle_new(&G, 2, 3);
    CHECK(pinochle_script(&G, "!") && P->phase == PP_OVER && pinochle_winner(P) >= 0);
    pinochle_new(&G, 4, 3);
    CHECK(pinochle_script(&G, "!") && P->phase == PP_OVER);
    pinochle_new(&G, 2, 3);
    CHECK(!pinochle_script(&G, "Zz") && !pinochle_script(&G, "b20") && !pinochle_script(&G, "MKsQs"));
}

int main(void)
{
    test_deal();
    test_meld_names();
    test_melds();
    test_strict();
    test_auction();
    test_games();
    test_notation();
    printf("pinochle: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
