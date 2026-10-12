/*
 * Doppelkopf: the rules' test (the double pack and the deal, the trumps of each game and their order, the first of two
 * equal cards, following; the reservations, a solo over a marriage, the marriage's partner and the bride alone, the
 * silent solo; the announcements and their limits, replies, the marriage's shift; who wins, the game value, the extra
 * points, the scores; the parties as the table knows them; undo, the notation; whole random games from seeds checked
 * against the DDV's counting). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/doppelkopf.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static dkgame G, H;
static dkpos *P = &G.pos;

#define C(s, r) CARD(SUIT_##s, r)
#define QC C(CLUBS, RANK_QUEEN)

static int cards(const char *s, uint8_t *out)
{
    int n = 0;
    for (;;) {
        while (*s == ' ') s++;
        int c = trick_parse(&s);
        if (c == CARD_NONE) return n;
        out[n++] = (uint8_t)c;
    }
}

/* a new deal with these cards in each hand, the rest of the double pack filling them up */
static void set_hands(const char *const *spec)
{
    static const int ranks[] = {RANK_ACE, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING};
    int left[256] = {0};
    for (int s = 0; s < 4; s++)
        for (int r = 0; r < 6; r++) left[CARD(s, ranks[r])] = 2;
    dko_new(&G, DKO_SHORT, 7);
    for (int q = 0; q < 4; q++) {
        P->t.n[q] = (uint8_t)cards(spec[q], P->t.hand[q]);
        for (int i = 0; i < P->t.n[q]; i++) left[P->t.hand[q][i]]--;
    }
    for (int q = 0; q < 4; q++)
        for (int c = 0; c < 256 && P->t.n[q] < 12; c++)
            while (left[c] > 0 && P->t.n[q] < 12) P->t.hand[q][P->t.n[q]++] = (uint8_t)c, left[c]--;
    for (int q = 0; q < 4; q++) dko_sort(P->t.hand[q], P->t.n[q], DKG_NORMAL);
    G.n = 0;
}

static void test_cards(void)
{
    dko_new(&G, DKO_SHORT, 42);
    int seen[256] = {0}, total = 0, pts = 0;
    for (int q = 0; q < 4; q++) {
        CHECK(P->t.n[q] == 12);
        for (int i = 0; i < 12; i++) seen[P->t.hand[q][i]]++, total++, pts += dko_points(P->t.hand[q][i]);
    }
    int twice = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) twice &= seen[CARD(s, r)] == (r == 1 || r >= 9 ? 2 : 0);
    CHECK(twice && total == 48 && pts == 240 && P->dealer == 3 && P->turn == 0 && P->phase == DKP_RESERVE);
    dko_new(&H, DKO_SHORT, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    /* the normal trumps: the Dulle, the queens, the jacks, diamonds A 10 K 9 */
    static const uint8_t order[13] = {C(HEARTS, 10), QC, C(SPADES, RANK_QUEEN), C(HEARTS, RANK_QUEEN),
                                      C(DIAMONDS, RANK_QUEEN), C(CLUBS, RANK_JACK), C(SPADES, RANK_JACK),
                                      C(HEARTS, RANK_JACK), C(DIAMONDS, RANK_JACK), C(DIAMONDS, RANK_ACE),
                                      C(DIAMONDS, 10), C(DIAMONDS, RANK_KING), C(DIAMONDS, 9)};
    int ok = 1;
    for (int i = 0; i < 13; i++) {
        ok &= dko_trump(order[i], DKG_NORMAL);
        if (i) ok &= dko_power(order[i - 1], DKG_NORMAL) > dko_power(order[i], DKG_NORMAL);
    }
    CHECK(ok && !dko_trump(C(HEARTS, RANK_ACE), DKG_NORMAL) && !dko_trump(C(CLUBS, RANK_ACE), DKG_NORMAL));
    uint8_t t[4];
    cards("Th Th Qc Ah", t);                    /* two Dullen: the first wins */
    CHECK(dko_winner(t, 4, DKG_NORMAL) == 0);
    cards("Ac Ac Tc 9d", t);                    /* two aces: the first; a trump over them */
    CHECK(dko_winner(t, 4, DKG_NORMAL) == 3 && dko_winner(t, 3, DKG_NORMAL) == 0);
    cards("Ah Kh Th 9h", t);                    /* hearts led, the ten is a trump */
    CHECK(dko_winner(t, 4, DKG_NORMAL) == 2 && dko_winner(t, 4, DKG_FLESHLESS) == 0 &&
          dko_winner(t, 4, DKG_QUEENS) == 0);
    /* the solos */
    cards("Kc Jd Qd Ac", t);
    CHECK(dko_winner(t, 4, DKG_QUEENS) == 2 && dko_winner(t, 4, DKG_JACKS) == 1 && dko_winner(t, 4, DKG_FLESHLESS) == 3);
    CHECK(dko_winner(t, 4, DKG_SUIT + SUIT_CLUBS) == 2 && dko_trump(C(CLUBS, 9), DKG_SUIT + SUIT_CLUBS) &&
          !dko_trump(C(DIAMONDS, RANK_ACE), DKG_SUIT + SUIT_CLUBS));
    CHECK(dko_trump(C(HEARTS, 10), DKG_SUIT + SUIT_HEARTS) &&
          dko_power(C(HEARTS, 10), DKG_SUIT + SUIT_HEARTS) > dko_power(QC, DKG_SUIT + SUIT_HEARTS) &&
          dko_power(C(HEARTS, RANK_ACE), DKG_SUIT + SUIT_HEARTS) < dko_power(C(DIAMONDS, RANK_JACK),
                                                                              DKG_SUIT + SUIT_HEARTS));
    CHECK(!dko_trump(C(HEARTS, 10), DKG_FLESHLESS) && !dko_trump(QC, DKG_JACKS) && !dko_trump(C(HEARTS, 10), DKG_QUEENS));
    /* a hand as shown: trumps first, the strongest first */
    uint8_t h[5] = {C(CLUBS, RANK_ACE), C(DIAMONDS, 9), C(HEARTS, 10), C(SPADES, RANK_JACK), C(HEARTS, RANK_KING)};
    dko_sort(h, 5, DKG_NORMAL);
    CHECK(h[0] == C(HEARTS, 10) && h[1] == C(SPADES, RANK_JACK) && h[2] == C(DIAMONDS, 9) &&
          h[3] == C(CLUBS, RANK_ACE) && h[4] == C(HEARTS, RANK_KING));
}

static void test_reservations(void)
{
    const char *const A[4] = {"Qc Qc", "", "", ""};
    set_hands(A);
    CHECK(dko_say_ok(P, DKG_WEDDING) && dko_say_ok(P, DKG_QUEENS) && dko_say_ok(P, 0) && !dko_say_ok(P, DKG_SILENT));
    CHECK(dko_script(&G, "w") && P->said[0] == DKW_RESERVED && P->turn == 1 && !dko_say_ok(P, DKG_WEDDING));
    CHECK(dko_script(&G, "g j g") && P->game == DKG_JACKS && P->soloist == 2 && P->re == 4 && P->open);
    CHECK(P->turn == 0 && P->t.leader == 0 && dko_known(P, 2) == 0 && dko_known(P, 0) == 1);
    set_hands(A);
    CHECK(dko_script(&G, "g sh f g") && P->game == DKG_SUIT + SUIT_HEARTS && P->soloist == 1);   /* the first solo */
    set_hands(A);
    CHECK(dko_script(&G, "g g g g") && P->game == DKG_SILENT && P->re == 1 && P->soloist == 0);
    CHECK(dko_known(P, 0) == -1 && dko_known(P, 1) == -1);   /* the table thinks it a normal game */
    const char *const B[4] = {"Qc", "Qc", "", ""};
    set_hands(B);
    CHECK(!dko_say_ok(P, DKG_WEDDING) && dko_script(&G, "g g g g") && P->game == DKG_NORMAL && P->re == 3);
}

static void test_wedding(void)
{
    const char *const A[4] = {"Qc Qc 9s 9s Ks Ks 9c 9c Kc Kc 9h 9h", "As As", "Ts", "Ts"};
    set_hands(A);
    CHECK(dko_script(&G, "w g g g") && P->game == DKG_WEDDING && !P->open && P->re == 1 && !dko_announce_ok(P));
    CHECK(dko_known(P, 0) == 0 && dko_known(P, 1) == -1);
    CHECK(dko_script(&G, "9s As Ts Ts") && P->open && P->re == 3 && P->shift == 1 && P->t.leader == 1);
    CHECK(dko_known(P, 1) == 0 && dko_known(P, 2) == 1);
    CHECK(dko_announce_ok(P));                  /* the limits a trick later: Re at 11 cards... */
    P->t.n[1] = 10;
    CHECK(dko_announce_ok(P));                  /* ...and at 10 */
    P->t.n[1] = 9;
    CHECK(!dko_announce_ok(P));
    /* the bride takes the first three: alone, scored as a solo */
    const char *const B[4] = {"Qc Qc Th Th Ac Ac As As Ah Ah 9c 9s", "", "", ""};
    set_hands(B);
    CHECK(dko_script(&G, "w g g g Th . . . Th . . . Qc . . .") && P->open && P->re == 1 && P->shift == 3);
    CHECK(dko_script(&G, "#") && P->list[0].solo && P->list[0].game == DKG_WEDDING);
    const dkdeal *r = &P->list[0];
    CHECK(r->score[0] == 3 * r->re_points && r->score[1] == -r->re_points && !r->fox[0] && !r->doko[0]);
}

static void test_announcements(void)
{
    const char *const A[4] = {"Qc", "Qc", "", ""};
    set_hands(A);
    CHECK(dko_script(&G, "g g g g") && P->re == 3 && dko_announce_ok(P));
    CHECK(dko_script(&G, "a") && P->ann[0] == DKL_PARTY && P->level[0] == 1 && P->turn == 0 && dko_known(P, 0) == 0);
    CHECK(dko_script(&G, "a a") && P->ann[0] == DKL_60 && P->level[0] == DKL_60);   /* no 90, no 60 at 12 cards */
    CHECK(dko_script(&G, "a") && P->ann[0] == DKL_30 && dko_script(&G, "a") && P->ann[0] == DKL_BLACK &&
          !dko_announce_ok(P) && !dko_script(&G, "a"));
    CHECK(dko_known(P, 1) == -1);               /* 1 is Re too, unknown yet */
    /* the limits: Re at 11 cards, not 10; no 90 at 10, not 9 */
    set_hands(A);
    CHECK(dko_script(&G, "g g g g"));
    P->t.n[0] = 11;
    CHECK(dko_announce_ok(P));
    P->t.n[0] = 10;
    CHECK(!dko_announce_ok(P));
    P->ann[0] = DKL_PARTY;                      /* Re said already: no 90 at 10 cards */
    CHECK(dko_announce_ok(P));
    P->t.n[0] = 9;
    CHECK(!dko_announce_ok(P));
    /* a reply: Kontra one card after the limit of Re's announcement */
    P->turn = 2;
    P->t.leader = 2;
    P->ann[0] = DKL_PARTY;
    P->t.n[2] = 10;
    CHECK(dko_announce_ok(P));                  /* Kontra in reply to Re, at 10 cards */
    P->t.n[2] = 9;
    CHECK(!dko_announce_ok(P));
    P->ann[0] = DKL_90;
    CHECK(dko_announce_ok(P));                  /* in reply to no 90: at 9 */
    P->t.n[2] = 8;
    CHECK(!dko_announce_ok(P));
}

/* the last trick to play, the points so far set: Re (bit n) with re_eyes and re_tricks before it, Kontra the rest;
 * the announcements; the last trick (no points) goes to player 0 */
static const dkdeal *last_trick(int re, int re_eyes, int re_tricks, int ann0, int ann1)
{
    dko_new(&G, DKO_SHORT, 3);
    P->phase = DKP_PLAY;
    P->game = DKG_NORMAL;
    P->re = (uint8_t)re;
    P->open = 1;
    P->ann[0] = (uint8_t)ann0;
    P->ann[1] = (uint8_t)ann1;
    P->t.tricks = 11;
    P->t.leader = 0;
    P->t.ntrick = 0;
    for (int i = 0; i < 11; i++) {
        P->hist_leader[i] = 0;
        for (int j = 0; j < 4; j++) P->hist[i][j] = C(HEARTS, 9);
    }
    memset(P->t.eyes, 0, sizeof P->t.eyes);
    memset(P->t.won, 0, sizeof P->t.won);
    int rq = re & 1 ? 0 : 1, kq = re & 2 ? 2 : 1;   /* one player of each party */
    if (re == 5) rq = 0, kq = 1;
    P->t.eyes[rq] = (int16_t)re_eyes;
    P->t.won[rq] = (uint8_t)re_tricks;
    P->t.eyes[kq] = (int16_t)(240 - re_eyes);
    P->t.won[kq] = (uint8_t)(11 - re_tricks);
    const char *const last[4] = {"9s", "9s", "9c", "9c"};
    for (int q = 0; q < 4; q++) P->t.n[q] = (uint8_t)cards(last[q], P->t.hand[q]);
    P->turn = 0;
    G.n = 0;
    CHECK(dko_script(&G, "9s 9s 9c 9c") && P->phase == DKP_DEAL);
    return &P->list[0];
}

static void test_value(void)
{
    const dkdeal *r = last_trick(5, 121, 6, 0, 0);
    CHECK(r->winner == 0 && r->value == 1 && r->re_points == 1 && r->score[0] == 1 && r->score[1] == -1);
    r = last_trick(5, 120, 6, 0, 0);            /* 120: Kontra wins, against the Alten */
    CHECK(r->winner == 1 && r->value == 1 && r->alten && r->re_points == -2 && r->score[1] == 2);
    r = last_trick(5, 120, 6, 0, DKL_PARTY);    /* only Kontra announced: Re wins with 120 */
    CHECK(r->winner == 0 && r->value == 3 && r->re_points == 3);
    r = last_trick(5, 151, 8, DKL_90, 0);       /* Re's no 90 kept */
    CHECK(r->winner == 0 && r->value == 5);
    r = last_trick(5, 150, 8, DKL_90, 0);       /* not kept: Kontra wins with 90 */
    CHECK(r->winner == 1 && r->value == 4 && r->re_points == -5);
    r = last_trick(5, 160, 8, DKL_90, DKL_90);  /* both announced no 90: Re keeps it, 120 against Kontra's */
    CHECK(r->winner == 0 && r->value == 9);
    r = last_trick(5, 120, 6, DKL_90, DKL_90);  /* neither kept: nobody wins */
    CHECK(r->winner == -1 && r->value == 0 && r->re_points == 0 && r->score[0] == 0);
    r = last_trick(5, 240, 11, 0, 0);           /* schwarz */
    CHECK(r->winner == 0 && r->value == 5 && r->tricks[1] == 0);
    r = last_trick(5, 240, 11, DKL_BLACK, 0);   /* schwarz announced and played: 1 + 4 + 2 + 4 */
    CHECK(r->winner == 0 && r->value == 11);
    r = last_trick(5, 200, 9, 0, 0);            /* 40 for Kontra: under 90, 60 */
    CHECK(r->winner == 0 && r->value == 3);
}

static void test_extras(void)
{
    /* a normal game: the extra points as counted from the tricks; Re's tricks */
    const char *const A[4] = {"Qc Ad", "Qc", "Ad", ""};
    set_hands(A);
    CHECK(dko_script(&G, "g g g g #") && P->phase == DKP_DEAL);
    const dkdeal *r = &P->list[0];
    int sum = 0;
    for (int q = 0; q < 4; q++) sum += r->score[q];
    CHECK(sum == 0 && r->eyes[0] + r->eyes[1] == 240 && r->tricks[0] + r->tricks[1] == 12);
    CHECK(r->re_points == (r->winner == 0 ? r->value : -r->value) + r->fox[0] + r->doko[0] + r->karl[0] - r->fox[1] -
                          r->doko[1] - r->karl[1] - r->alten);
    CHECK(r->fox[0] + r->fox[1] <= 2 && r->karl[0] + r->karl[1] <= 1 && r->alten == (r->winner == 1));
    /* no extra points in a solo */
    set_hands(A);
    CHECK(dko_script(&G, "q g g g #"));
    r = &P->list[0];
    CHECK(r->solo && !r->fox[0] && !r->fox[1] && !r->doko[0] && !r->doko[1] && !r->alten &&
          r->score[0] == 3 * r->re_points && r->score[2] == -r->re_points);
    /* a sure queens' solo: all eight queens, every trick */
    const char *const B[4] = {"Qc Qc Qs Qs Qh Qh Qd Qd Ac Ac As As", "", "", ""};
    set_hands(B);
    CHECK(dko_script(&G, "q g g g #"));
    r = &P->list[0];
    CHECK(r->winner == 0 && r->tricks[1] == 0 && r->value == 5 && r->score[0] == 15 && r->score[1] == -5);
}

static void test_undo_notation(void)
{
    dko_new(&G, DKO_SHORT, 9);
    CHECK(dko_script(&G, "g g g g a . ."));
    dkpos keep = G.pos;
    CHECK(dko_script(&G, ".") && dko_undo(&G) && !memcmp(&keep, &G.pos, sizeof keep));
    CHECK(!dko_script(&G, "zz") && !dko_script(&G, "sx") && !dko_script(&G, "g"));
    dko_new(&G, DKO_SHORT, 9);
    CHECK(dko_script(&G, "!") && P->phase == DKP_OVER && P->ndeals == 12 && dko_winners(P));
    dko_new(&G, DKO_LONG, 9);
    CHECK(dko_script(&G, "!") && P->ndeals == 24 && dko_deals(DKO_LONG) == 24);
}

/* the DDV's counting, again, from a deal's record */
static int ref_value(const dkdeal *r, int *winner)
{
    int a0 = r->ann[0], a1 = r->ann[1], only = a1 && !a0;
    int re_ok, ko_ok;
    re_ok = a0 == 5 ? r->tricks[1] == 0 : a0 == 4 ? r->eyes[1] < 30 : a0 == 3 ? r->eyes[1] < 60 :
            a0 == 2 ? r->eyes[1] < 90 : r->eyes[0] >= (only ? 120 : 121);
    ko_ok = a1 == 5 ? r->tricks[0] == 0 : a1 == 4 ? r->eyes[0] < 30 : a1 == 3 ? r->eyes[0] < 60 :
            a1 == 2 ? r->eyes[0] < 90 : r->eyes[1] >= (only ? 121 : 120);
    int w;
    if (a0 >= 2 && a1 >= 2) w = re_ok ? 0 : ko_ok ? 1 : -1;
    else if (a0 >= 2) w = re_ok ? 0 : 1;
    else if (a1 >= 2) w = ko_ok ? 1 : 0;
    else w = re_ok ? 0 : 1;
    *winner = w;
    if (w < 0) return 0;
    int l = 1 - w, v = 1;
    static const int marks[4] = {90, 60, 30, 0};
    for (int k = 0; k < 3; k++) v += r->eyes[l] < marks[k];
    v += r->tricks[l] == 0;
    v += (a0 ? 2 : 0) + (a1 ? 2 : 0) + (a0 > 1 ? a0 - 1 : 0) + (a1 > 1 ? a1 - 1 : 0);
    static const int against[4] = {120, 90, 60, 30};
    for (int k = 2; k <= r->ann[l]; k++) v += r->eyes[w] >= against[k - 2];
    return v;
}

static void test_random(void)
{
    int games[DKG_TYPES] = {0}, anns = 0, nobody = 0, deals = 0, extras = 0;
    for (uint32_t seed = 1; seed <= 150; seed++) {
        dko_new(&G, DKO_SHORT, seed);
        uint32_t rs = seed * 7919u;
        int guard = 0;
        while (P->phase != DKP_OVER && guard++ < 4000) {
            int acts[96], n = 0;
            if (P->phase == DKP_PLAY) {
                for (int i = 0; i < P->t.n[P->turn]; i++)
                    if (dko_can_play(P, P->t.hand[P->turn][i])) acts[n++] = DKA_PLAY | P->t.hand[P->turn][i];
                if (dko_announce_ok(P) && cards_rand(&rs) % 4 == 0)
                    acts[0] = DKA_ANNOUNCE | (P->ann[dko_party(P, P->turn)] + 1), n = 1;
            } else if (P->phase == DKP_RESERVE) {
                int healthy = 12 + 40 * (seed & 1);     /* half the games: mostly normal ones */
                if (dko_say_ok(P, DKG_WEDDING)) healthy = 2;
                for (int k = 0; k < healthy; k++) acts[n++] = DKA_SAY;
                for (int g = 1; g < DKG_TYPES; g++)
                    if (dko_say_ok(P, g)) for (int k = g == DKG_WEDDING ? 8 : 1; k > 0; k--) acts[n++] = DKA_SAY | g;
            } else
                acts[n++] = DKA_NEXT;
            if (!n) { CHECK(0); break; }
            if (!dko_play(&G, acts[cards_rand(&rs) % (uint32_t)n])) { CHECK(0); break; }
        }
        CHECK(P->phase == DKP_OVER && P->ndeals == 12);
        int tot[4] = {0}, ok = 1;
        for (int d = 0; d < P->ndeals; d++) {
            const dkdeal *r = &P->list[d];
            int sum = 0, w, v = ref_value(r, &w);
            for (int q = 0; q < 4; q++) sum += r->score[q], tot[q] += r->score[q];
            ok &= sum == 0 && r->eyes[0] + r->eyes[1] == 240 && r->tricks[0] + r->tricks[1] == 12;
            ok &= w == r->winner && v == r->value;
            games[r->game]++, anns += r->ann[0] || r->ann[1], nobody += r->winner < 0, deals++;
            extras += r->fox[0] + r->fox[1] + r->doko[0] + r->doko[1] + r->karl[0] + r->karl[1];
        }
        for (int q = 0; q < 4; q++) ok &= tot[q] == P->total[q];
        CHECK(ok);
    }
    printf("  150 random games, %d deals: %d normal, %d marriages, %d silent, %d solos; %d announced, %d won by "
           "nobody, %d extra points\n", deals, games[DKG_NORMAL], games[DKG_WEDDING], games[DKG_SILENT],
           deals - games[DKG_NORMAL] - games[DKG_WEDDING] - games[DKG_SILENT], anns, nobody, extras);
}

int main(void)
{
    test_cards();
    test_reservations();
    test_wedding();
    test_announcements();
    test_value();
    test_extras();
    test_undo_notation();
    test_random();
    printf("doppelkopf: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
