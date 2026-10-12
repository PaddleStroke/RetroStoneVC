/*
 * Koi-Koi: the rules' test (the deck's types; every yaku and its points; teshi and kuttsuki; the deal; matching none,
 * one, two (a choice) or three; the stock's card; stop and koi-koi, the doublings; a round played out; the game's
 * length; undo; the notation; whole games from seeds, every card always somewhere once). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/koikoi.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static kgame G, H;
static kpos *P = &G.pos;

/* cards from names: "1a 8a 9a" */
static int cards(const char *s, uint8_t *out)
{
    int n = 0, c;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int k = kk_card_name(s, &c);
        if (!k) return -1;
        out[n++] = (uint8_t)c;
        s += k;
    }
    return n;
}

static int yaku(const char *s, int *pts)
{
    uint8_t c[KK_CARDS];
    int n = cards(s, c);
    return kk_yaku(c, n, pts);
}

static void test_deck(void)
{
    int type[HTYPES] = {0}, rib[4] = {0};
    for (int c = 0; c < KK_CARDS; c++) type[kk_type(c)]++, rib[kk_ribbon(c)]++;
    CHECK(type[HT_BRIGHT] == 5 && type[HT_ANIMAL] == 9 && type[HT_RIBBON] == 10 && type[HT_PLAIN] == 24);
    CHECK(rib[HR_POETRY] == 3 && rib[HR_BLUE] == 3 && rib[HR_RED] == 4);
    CHECK(kk_type(KK_CRANE) == HT_BRIGHT && kk_type(KK_RAINMAN) == HT_BRIGHT && kk_type(KK_SAKE) == HT_ANIMAL);
    CHECK(kk_type(KK_LIGHTNING) == HT_PLAIN && kk_type(KK_SWALLOW) == HT_ANIMAL && kk_type(KK_GEESE) == HT_ANIMAL);
    CHECK(kk_ribbon(HANA(M_WILLOW, 2)) == HR_RED && kk_ribbon(HANA(M_PEONY, 1)) == HR_BLUE);
    int c;
    CHECK(kk_card_name("8a", &c) == 2 && c == KK_MOON && kk_card_name("12d", &c) == 3 && c == HANA(11, 3));
    CHECK(!kk_card_name("13a", &c) && !kk_card_name("0a", &c) && !kk_card_name("3e", &c));
}

static void test_yaku(void)
{
    int y[YAKU_N];
    CHECK(yaku("1a 3a 8a 11a 12a", y) == 10 && y[Y_GOKO] == 10);
    CHECK(yaku("1a 3a 8a 12a", y) == 8 && y[Y_SHIKO] == 8);
    CHECK(yaku("1a 3a 8a 11a", y) == 7 && y[Y_AMESHIKO] == 7);
    CHECK(yaku("1a 3a 12a", y) == 5 && y[Y_SANKO] == 5);
    CHECK(yaku("1a 11a 12a", y) == 0);          /* three with the rain man: nothing */
    CHECK(yaku("1a 8a", y) == 0);
    CHECK(yaku("7a 10a 6a", y) == 5 && y[Y_INOSHIKACHO] == 5);
    CHECK(yaku("1b 2b 3b", y) == 5 && y[Y_AKATAN] == 5);
    CHECK(yaku("6b 9b 10b", y) == 5 && y[Y_AOTAN] == 5);
    CHECK(yaku("1b 2b 3b 6b 9b 10b", y) == 12 && y[Y_AKATAN] == 5 && y[Y_AOTAN] == 5 && y[Y_TAN] == 2);
    CHECK(yaku("3a 9a", y) == 5 && y[Y_HANAMI] == 5);
    CHECK(yaku("8a 9a", y) == 5 && y[Y_TSUKIMI] == 5);
    CHECK(yaku("3a 8a 9a", y) == 10 && y[Y_HANAMI] == 5 && y[Y_TSUKIMI] == 5);
    CHECK(yaku("2a 4a 5a 8b 11b", y) == 1 && y[Y_TANE] == 1);
    CHECK(yaku("2a 4a 5a 8b 11b 9a", y) == 2 + 0 && y[Y_TANE] == 2);
    CHECK(yaku("6a 7a 10a 2a 4a", y) == 6 && y[Y_INOSHIKACHO] == 5 && y[Y_TANE] == 1);
    CHECK(yaku("4b 5b 7b 11c 1b", y) == 1 && y[Y_TAN] == 1);
    CHECK(yaku("1c 1d 2c 2d 3c 3d 4c 4d 5c", y) == 0);
    CHECK(yaku("1c 1d 2c 2d 3c 3d 4c 4d 5c 5d", y) == 1 && y[Y_KASU] == 1);
    CHECK(yaku("1c 1d 2c 2d 3c 3d 4c 4d 5c 9a", y) == 1 && y[Y_KASU] == 1);   /* the sake cup is a plain too */
    CHECK(yaku("1c 1d 2c 2d 3c 3d 4c 4d 5c 5d 11d 12b 12c", y) == 4 && y[Y_KASU] == 4);
    CHECK(yaku("1a 3a 8a 12a 9a", y) == 18 && y[Y_SHIKO] == 8 && y[Y_HANAMI] == 5 && y[Y_TSUKIMI] == 5);
    CHECK(yaku("", y) == 0);
}

static void test_instant(void)
{
    uint8_t h[8];
    cards("1a 1b 1c 1d 2a 3b 5c 7d", h);
    CHECK(kk_instant(h, 8) == Y_TESHI);
    cards("1a 1b 3c 3d 5a 5b 7c 7d", h);
    CHECK(kk_instant(h, 8) == Y_KUTTSUKI);
    cards("1a 1b 3c 3d 5a 5b 7c 8d", h);
    CHECK(kk_instant(h, 8) == -1);
    cards("1a 1b 1c 3d 5a 5b 7c 7d", h);         /* three of a month and two pairs: nothing */
    CHECK(kk_instant(h, 8) == -1);
}

/* every card once: in a hand, on the field, in the stock, captured, or held */
static int whole(const kpos *p)
{
    int seen[KK_CARDS] = {0}, n = 0;
    for (int q = 0; q < 2; q++) {
        for (int i = 0; i < p->nh[q]; i++) seen[p->hand[q][i]]++, n++;
        for (int i = 0; i < p->ncap[q]; i++) seen[p->cap[q][i]]++, n++;
    }
    for (int s = 0; s < KK_FIELD; s++)
        if (p->field[s] != KK_NONE) seen[p->field[s]]++, n++;
    for (int i = 0; i < p->ndeck; i++) seen[p->deck[i]]++, n++;
    if (p->phase == KP_CHOOSE) seen[p->held]++, n++;
    for (int c = 0; c < KK_CARDS; c++)
        if (seen[c] != 1) return 0;
    return n == KK_CARDS;
}

static void test_deal(void)
{
    kk_new(&G, KK_FULL, 42);
    kk_new(&H, KK_FULL, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->rounds == 12 && P->round == 0 && P->dealer == 0);
    int y = -1;
    for (uint32_t s = 1; s < 400; s++) {        /* many deals: eight each, eight on the field, never four of a month */
        kk_new(&G, KK_FULL, s);
        int m[12] = {0}, four = 0, n = 0;
        for (int i = 0; i < KK_FIELD; i++)
            if (P->field[i] != KK_NONE) four |= ++m[HANA_MONTH(P->field[i])] == 4, n++;
        CHECK(whole(P) && n == 8 && !four && P->nh[0] == 8 && P->nh[1] == 8 && P->ndeck == 24);
        for (int i = 1; i < 8; i++) CHECK(P->hand[0][i - 1] < P->hand[0][i]);
        if (P->phase == KP_ROUND && y < 0) y = s;   /* teshi or kuttsuki dealt */
        if (P->phase == KP_ROUND) {
            CHECK(P->res.kind == KR_INSTANT && P->res.points == KK_INSTANT && P->score[P->res.winner] == 6);
            CHECK(kk_instant(P->hand[P->res.winner], 8) >= 0);
        } else {
            CHECK(P->phase == KP_PLAY && P->turn == 0 && kk_instant(P->hand[0], 8) < 0 &&
                  kk_instant(P->hand[1], 8) < 0);
        }
    }
    CHECK(y > 0);                               /* some deal among them was won at once */
    kk_new(&G, KK_HALF, 5);
    CHECK(P->rounds == 6);
}

/* a position set up by hand: the player to move's hand, the field, the stock (its top last) */
static void setup(const char *hand, const char *field, const char *deck)
{
    kk_new(&G, KK_FULL, 1);
    uint8_t c[KK_CARDS];
    memset(P->field, KK_NONE, sizeof P->field);
    P->nh[0] = (uint8_t)cards(hand, P->hand[0]);
    P->nh[1] = (uint8_t)cards("12b 12c", P->hand[1]);
    int n = cards(field, c);
    for (int i = 0; i < n; i++) P->field[i] = c[i];
    P->ndeck = (uint8_t)cards(deck, P->deck);
    P->ncap[0] = P->ncap[1] = 0;
    P->turn = 0;
    P->phase = KP_PLAY;
    P->koi[0] = P->koi[1] = P->base[0] = P->base[1] = 0;
    P->score[0] = P->score[1] = 0;
}

static int captured(const kpos *p, int pl, const char *s)
{
    uint8_t c[KK_CARDS];
    int n = cards(s, c);
    if (n != p->ncap[pl]) return 0;
    for (int i = 0; i < n; i++)
        if (p->cap[pl][i] != c[i]) return 0;
    return 1;
}

static void test_play(void)
{
    /* no match: the card stays, in the first empty place */
    setup("1a 2c", "3c 4c", "5c 6c");
    CHECK(!kk_play(&G, KA_PLAY | HANA(5, 0)) && !kk_play(&G, KA_DRAW));   /* not in the hand; not yet */
    CHECK(kk_play(&G, KA_PLAY | KK_CRANE) && P->field[2] == KK_CRANE && P->phase == KP_DRAW && P->nh[0] == 1);
    CHECK(kk_play(&G, KA_DRAW) && P->drawn == HANA(5, 2) && P->field[3] == HANA(5, 2));   /* the top: last */
    CHECK(P->phase == KP_PLAY && P->turn == 1 && P->ncap[0] == 0);
    /* one match: both captured; the stock's card matches too */
    setup("1a 2c", "1c 4c 6c", "5c 6d");
    CHECK(kk_play(&G, KA_PLAY | KK_CRANE) && captured(P, 0, "1a 1c") && P->field[0] == KK_NONE);
    CHECK(kk_play(&G, KA_DRAW) && captured(P, 0, "1a 1c 6d 6c") && P->field[2] == KK_NONE && P->turn == 1);
    /* the hole is filled first */
    setup("2a 3c", "1c 2c 6c", "5c 7d");
    kk_play(&G, KA_PLAY | HANA(1, 0));
    kk_play(&G, KA_DRAW);
    CHECK(P->field[1] == HANA(6, 3));
    /* two matches: a choice */
    setup("1a 2c", "1c 4c 1d", "5c 7d");
    CHECK(kk_play(&G, KA_PLAY | KK_CRANE) && P->phase == KP_CHOOSE && P->held == KK_CRANE);
    CHECK(!kk_play(&G, KA_TAKE | HANA(3, 2)) && !kk_play(&G, KA_DRAW));
    CHECK(kk_play(&G, KA_TAKE | HANA(0, 3)) && captured(P, 0, "1a 1d") && P->field[0] == HANA(0, 2));
    CHECK(P->phase == KP_DRAW);
    /* three matches: all four */
    setup("1a 2c", "1b 1c 4c 1d", "5c 7d");
    CHECK(kk_play(&G, KA_PLAY | KK_CRANE) && captured(P, 0, "1a 1b 1c 1d") && P->phase == KP_DRAW);
    /* the stock's card with two matches: the choice, then the turn ends */
    setup("3c 2c", "1c 4c 1d", "5c 1b");
    kk_play(&G, KA_PLAY | HANA(2, 2));
    CHECK(kk_play(&G, KA_DRAW) && P->phase == KP_CHOOSE && P->from_deck && P->held == HANA(0, 1));
    CHECK(kk_play(&G, KA_TAKE | HANA(0, 2)) && captured(P, 0, "1b 1c") && P->turn == 1 && P->phase == KP_PLAY);
}

static void test_koikoi(void)
{
    /* a yaku formed: stop or koi-koi (first the curtain: no yaku yet, the turn passes) */
    setup("3c 9a 4c", "3a 9c 6c", "5d 7d 7c");
    P->ncap[0] = (uint8_t)cards("8a", P->cap[0]);
    kk_play(&G, KA_PLAY | HANA(2, 2));          /* takes the curtain */
    kk_play(&G, KA_DRAW);                       /* 7c: nothing */
    CHECK(P->phase == KP_PLAY && P->turn == 1 && kk_points(P, 0) == 0);
    P->turn = 0;
    kk_play(&G, KA_PLAY | KK_SAKE);             /* the sake cup with 9c: hanami and tsukimi */
    kk_play(&G, KA_DRAW);
    CHECK(kk_points(P, 0) == 10 && P->phase == KP_DECIDE && P->turn == 0);
    CHECK(!kk_play(&G, KA_PLAY | HANA(3, 2)) && !kk_play(&G, KA_NEXT));
    kgame K = G;
    CHECK(kk_play(&G, KA_STOP) && P->phase == KP_ROUND && P->res.kind == KR_STOP && P->res.winner == 0);
    CHECK(P->res.base == 10 && P->res.seven && !P->res.koi && P->res.points == 20 && P->score[0] == 20);
    CHECK(P->res.pts[Y_HANAMI] == 5 && P->res.pts[Y_TSUKIMI] == 5 && P->dealer == 0);
    G = K;
    CHECK(kk_play(&G, KA_KOI) && P->koi[0] == 1 && P->base[0] == 10 && P->turn == 1 && P->phase == KP_PLAY);
    /* the other then forms a yaku and stops: doubled for the koi-koi */
    setup("3b 4c", "3d 6c", "6d 5d");
    P->koi[1] = 1;
    P->ncap[0] = (uint8_t)cards("1b 2b", P->cap[0]);
    kk_play(&G, KA_PLAY | HANA(2, 1));
    kk_play(&G, KA_DRAW);
    CHECK(P->phase == KP_DECIDE && kk_points(P, 0) == 5);
    kk_play(&G, KA_STOP);
    CHECK(P->res.base == 5 && !P->res.seven && P->res.koi && P->res.points == 10 && P->dealer == 0);
    /* after a koi-koi, the same points again: no new decision */
    setup("3c 4c", "4d 6c", "6d 5d");
    P->ncap[0] = (uint8_t)cards("1b 2b 3b", P->cap[0]);
    P->koi[0] = 1;
    P->base[0] = 5;
    kk_play(&G, KA_PLAY | HANA(2, 2));
    kk_play(&G, KA_DRAW);
    CHECK(P->phase == KP_PLAY && P->turn == 1);
    /* raised: decide again */
    setup("4b 6c", "4d 6c", "6d 5d");
    P->ncap[0] = (uint8_t)cards("1b 2b 3b 5b 7b", P->cap[0]);
    P->koi[0] = 1;
    P->base[0] = 6;
    kk_play(&G, KA_PLAY | HANA(3, 1));
    kk_play(&G, KA_DRAW);
    CHECK(P->phase == KP_DECIDE && kk_points(P, 0) == 7);
    /* the last card: no choice, a stop */
    setup("3b", "3d", "6d 5d");
    P->ncap[0] = (uint8_t)cards("1b 2b", P->cap[0]);
    P->nh[1] = 0;
    kk_play(&G, KA_PLAY | HANA(2, 1));
    kk_play(&G, KA_DRAW);
    CHECK(P->phase == KP_ROUND && P->res.kind == KR_STOP && P->res.points == 5);
    /* both hands played out, nobody stopping: nobody scores, the dealer stays */
    setup("12a", "3d", "6d 5d");
    P->nh[1] = 0;
    P->dealer = 1;
    kk_play(&G, KA_PLAY | KK_PHOENIX);
    kk_play(&G, KA_DRAW);
    CHECK(P->phase == KP_ROUND && P->res.kind == KR_DRAW && P->res.winner == KK_NONE && P->dealer == 1);
    CHECK(P->score[0] == 0 && P->score[1] == 0);
}

static void test_game(void)
{
    kk_new(&G, KK_FULL, 77);
    CHECK(kk_script(&G, "*") && P->phase == KP_OVER && P->round == 11);
    kk_new(&G, KK_HALF, 77);
    CHECK(kk_script(&G, "*") && P->phase == KP_OVER && P->round == 5);
    CHECK(!kk_play(&G, KA_NEXT));
    int w = kk_winner(P);
    CHECK(w == (P->score[0] == P->score[1] ? -1 : P->score[1] > P->score[0]));
    /* the notation and undo */
    kk_new(&G, KK_FULL, 9);
    CHECK(kk_script(&G, "#") && P->phase == KP_ROUND && kk_script(&G, "n") && P->round == 1);
    H = G;
    CHECK(kk_script(&G, "...") && G.n == H.n + 3);
    for (int i = 0; i < 3; i++) CHECK(kk_undo(&G));
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(!kk_script(&G, "x") && !kk_script(&G, "p13a"));
    kk_new(&G, KK_FULL, 9);
    char s[8];
    snprintf(s, sizeof s, "p%d%c", HANA_MONTH(P->hand[0][0]) + 1, 'a' + HANA_INDEX(P->hand[0][0]));
    CHECK(kk_script(&G, s) && P->nh[0] == 7);
    kk_new(&G, KK_FULL, 9);
    CHECK(!kk_undo(&G));
}

/* whole games with random choices: every card always somewhere once, the points as the results say */
static void test_random(void)
{
    uint32_t r = 12345;
    int stops = 0, kois = 0, draws = 0, instants = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        kk_new(&G, seed % 3 ? KK_FULL : KK_HALF, seed);
        int total[2] = {P->score[0], P->score[1]}, ok = 1, guard = 0;   /* a deal won at once: already scored */
        while (P->phase != KP_OVER && guard++ < 2000) {
            ok &= whole(P);
            int acts[16], n = 0;
            if (P->phase == KP_PLAY)
                for (int i = 0; i < P->nh[P->turn]; i++) acts[n++] = KA_PLAY | P->hand[P->turn][i];
            else if (P->phase == KP_CHOOSE) {
                for (int s = 0; s < KK_FIELD; s++)
                    if (P->field[s] != KK_NONE && kk_legal(P, KA_TAKE | P->field[s])) acts[n++] = KA_TAKE | P->field[s];
            } else if (P->phase == KP_DRAW) acts[n++] = KA_DRAW;
            else if (P->phase == KP_DECIDE) acts[n++] = KA_STOP, acts[n++] = KA_KOI, acts[n++] = KA_KOI;
            else acts[n++] = KA_NEXT;
            ok &= n > 0;
            r = r * 1664525u + 1013904223u;
            int a = acts[(r >> 8) % (unsigned)n];
            kpos b = *P;
            ok &= kk_play(&G, a);
            if (KA_KIND(a) == KA_KOI) kois++;
            if (P->phase == KP_ROUND || P->phase == KP_OVER) {
                if (b.phase != KP_ROUND || KA_KIND(a) == KA_NEXT) {
                    const kk_result *res = &P->res;
                    if (res->winner != KK_NONE) total[res->winner] += res->points;
                    stops += res->kind == KR_STOP, draws += res->kind == KR_DRAW, instants += res->kind == KR_INSTANT;
                    if (res->kind == KR_STOP) {
                        int y = kk_points(P, res->winner);
                        ok &= res->base == y && res->points == (y << (y >= 7) << res->koi);
                    }
                }
            }
            for (int q = 0; q < 2; q++) ok &= P->nh[q] <= KK_HAND;
        }
        CHECK(ok && P->phase == KP_OVER && P->round == P->rounds - 1);
        CHECK(P->score[0] == total[0] && P->score[1] == total[1]);
        if (seed % 50 == 0) {                   /* a replay through undo lands where it was */
            H = G;
            int n = G.n;
            CHECK(kk_undo(&G) && G.n == n - 1);
            kk_play(&G, H.act[n - 1]);
            CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
        }
    }
    CHECK(stops > 100 && kois > 50 && draws > 0);
    printf("random games: %d stops, %d koi-koi, %d played out, %d won at the deal\n", stops, kois, draws, instants);
}

int main(void)
{
    test_deck();
    test_yaku();
    test_instant();
    test_deal();
    test_play();
    test_koikoi();
    test_game();
    test_random();
    printf("koikoi: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
