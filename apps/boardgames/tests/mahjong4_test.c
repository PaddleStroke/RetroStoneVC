/*
 * Mahjong for four: the rules' test (the readings of a hand and the winning hands, the special ones too; the faan;
 * the claims and their order; the payments; the deal, the flowers, the dead wall; robbing a kong; whole random games
 * from seeds, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "cards.h"
#include "games/mahjong4.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static m4game G;
static m4pos *P = &G.pos;

/* ---- hands written as "123o 456b EE RRR": digits then their suit (o dots, b bamboo, c characters); E S W N winds,
 * R G P dragons ------------------------------------------------------------------------------------------------- */
static int kinds(const char *s, uint8_t *out)
{
    int n = 0;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s >= '1' && *s <= '9') {
            const char *d = s;
            while (*s >= '1' && *s <= '9') s++;
            int base = *s == 'o' ? 0 : *s == 'b' ? 9 : 18;
            for (; d < s; d++) out[n++] = (uint8_t)(base + *d - '1');
            s++;
            continue;
        }
        const char *t = s;
        int k = m4_parse_tile(&t);
        if (k < 0) return -1;
        out[n++] = (uint8_t)k;
        s = t;
    }
    return n;
}

/* a hand: the concealed tiles (the winning one with them), melds "pung RRR" "chow 123o" "kong EEEE" "ckong 5555c"
 * separated by '|', the winning tile, how */
static m4hand H(const char *conc, const char *melds, const char *win, int self)
{
    m4hand h;
    memset(&h, 0, sizeof h);
    h.nconc = kinds(conc, h.conc);
    for (const char *m = melds; m && *m;) {
        char buf[64];
        int len = 0;
        while (*m && *m != '|' && len < 63) buf[len++] = *m++;
        buf[len] = 0;
        if (*m == '|') m++;
        uint8_t k[8];
        const char *sp = strchr(buf, ' ');
        int n = kinds(sp + 1, k), type = buf[0] == 'c' && buf[1] == 'h' ? MS_CHOW : n == 4 ? MS_KONG : MS_PUNG;
        h.meld[h.nmeld].type = (uint8_t)type;
        h.meld[h.nmeld].kind = k[0];
        h.meld[h.nmeld++].concealed = buf[0] == 'c' && buf[1] == 'k';
    }
    uint8_t w[2];
    kinds(win, w);
    h.win_kind = w[0];
    h.self_drawn = self;
    return h;
}

static int has(const m4score *s, int item)
{
    for (int i = 0; i < s->n; i++)
        if (s->item[i] == item) return s->value[i];
    return 0;
}

static void test_readings(void)
{
    m4score s;
    m4hand h;
    /* all chows, concealed, won on a discard, no flowers: 1 + 1 + 1 */
    h = H("123o 456o 789o 123b 55c", NULL, "5c", 0);
    CHECK(m4_score_hand(&h, &s) == 3 && has(&s, MI_ALL_CHOWS) && has(&s, MI_CONCEALED) && has(&s, MI_NO_FLOWERS));
    CHECK(s.nset == 5 && s.set_type[4] == MS_PAIR && s.set_kind[4] == 18 + 4);
    /* not complete */
    h = H("123o 456o 789o 12b 556c", NULL, "5c", 0);
    CHECK(m4_score_hand(&h, NULL) == -1);
    h = H("123o 456o 789o 123b 5c", NULL, "5c", 0);
    CHECK(m4_score_hand(&h, NULL) == -1);       /* 13 tiles */
    h = H("11o 22o 33b 44b 55c 66c 77c", NULL, "7c", 0);
    CHECK(m4_score_hand(&h, NULL) == 6);       /* seven pairs 4 + concealed + no flowers */
    m4_score_hand(&h, &s);
    CHECK(has(&s, MI_SEVEN_PAIRS) == 4 && s.nset == 7);
    /* the best reading: 111 222 333 as three chows (all chows) rather than three pungs */
    h = H("111222333o 456b 77c", NULL, "7c", 0);
    CHECK(m4_score_hand(&h, &s) == 3 && has(&s, MI_ALL_CHOWS) && s.set_type[0] == MS_CHOW);
    /* ... but as pungs when the rest is pungs too: all pungs */
    h = H("111222333o EEE 77c", NULL, "7c", 1);
    h.seat = 1;
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_FOUR_CONCEALED));   /* self-drawn: four concealed pungs */
    h.self_drawn = 0;
    h.win_kind = 2;                             /* won on the 3o: that pung was not concealed */
    CHECK(m4_score_hand(&h, &s) == 6 && has(&s, MI_ALL_PUNGS) == 3 && has(&s, MI_ROUND_WIND) &&
          !has(&s, MI_SEAT_WIND));
    h.win_kind = 18 + 6;                        /* won on the pair: still four concealed pungs */
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_FOUR_CONCEALED));
    /* all pungs with melds, self-drawn */
    h = H("222c 333c 55o", "pung 111o|pung 999b", "5o", 1);
    CHECK(m4_score_hand(&h, &s) == 5 && has(&s, MI_ALL_PUNGS) == 3 && has(&s, MI_SELF_DRAWN) && !has(&s, MI_CONCEALED));
    /* half flush with a dragon */
    h = H("123o 456o 789o RRR 22o", NULL, "2o", 0);
    CHECK(m4_score_hand(&h, &s) == 6 && has(&s, MI_HALF_FLUSH) == 3 && has(&s, MI_DRAGON_PUNG) == 1);
    /* full flush */
    h = H("123o 345o 678o 999o 55o", NULL, "5o", 0);
    CHECK(m4_score_hand(&h, &s) == 9 && has(&s, MI_FULL_FLUSH) == 7);
    /* seven pairs, a half flush */
    h = H("11b 33b 55b 77b 99b EE RR", NULL, "R", 1);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_SEVEN_PAIRS) && has(&s, MI_HALF_FLUSH) && s.raw == 10);
    /* four of a kind is not two pairs */
    h = H("11b 11b 55b 77b 99b EE RR", NULL, "R", 0);
    CHECK(m4_score_hand(&h, NULL) == -1);
    /* thirteen orphans */
    h = H("19o 19b 19c ESWNRGP 1o", NULL, "1o", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_THIRTEEN) && s.n == 1);
    h = H("19o 19b 19c ESWNRGP 2o", NULL, "2o", 0);
    CHECK(m4_score_hand(&h, NULL) == -1);
    /* the dragons: big three, small three */
    h = H("PPP 123o 55b", "pung RRR|pung GGG", "5b", 0);
    CHECK(m4_score_hand(&h, &s) == 9 && has(&s, MI_BIG_DRAGONS) == 8 && !has(&s, MI_DRAGON_PUNG));
    h = H("PP 123o 456b", "pung RRR|pung GGG", "6b", 0);
    CHECK(m4_score_hand(&h, &s) == 6 && has(&s, MI_SMALL_DRAGONS) == 5 && !has(&s, MI_DRAGON_PUNG));
    /* the winds: seat and prevailing, small and big four */
    h = H("123o 456b SS", "pung WWW|pung EEE", "S", 0);
    h.seat = 2;                                 /* west */
    CHECK(m4_score_hand(&h, &s) == 3 && has(&s, MI_SEAT_WIND) && has(&s, MI_ROUND_WIND));
    h = H("NN 123o", "pung EEE|pung SSS|pung WWW", "3o", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_SMALL_WINDS));
    h = H("NNN 55o", "pung EEE|pung SSS|pung WWW", "5o", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_BIG_WINDS));
    /* all honours, all terminals, nine gates, four kongs */
    h = H("NN RRR", "pung EEE|pung SSS|pung GGG", "N", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_ALL_HONOURS));
    h = H("111o 999o 111b 999c 99b", NULL, "9b", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_ALL_TERMINALS));
    h = H("1112345678999b 5b", NULL, "5b", 0);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_NINE_GATES));
    h = H("1112345678999b 1b", NULL, "1b", 1);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_NINE_GATES));
    h = H("1112345678999b 1o", NULL, "1o", 1);
    CHECK(m4_score_hand(&h, NULL) == -1);
    h = H("55o", "kong 1111b|kong 2222c|ckong RRRR|kong EEEE", "5o", 1);
    CHECK(m4_score_hand(&h, &s) == 10 && has(&s, MI_FOUR_KONGS));
    /* a concealed kong keeps the hand concealed */
    h = H("123o 456o 789o 55b", "ckong 9999c", "5b", 0);
    CHECK(m4_score_hand(&h, &s) == 2 && has(&s, MI_CONCEALED) && has(&s, MI_NO_FLOWERS));
    /* the flowers: one's own (east: flower 1, season 1), a full set, none */
    h = H("123o 456o 789o 55b", "chow 123c", "5b", 0);
    CHECK(m4_score_hand(&h, &s) == 2);          /* all chows, no flowers */
    h.flower[h.nflower++] = M4_FLOWERS;         /* east's own flower */
    h.flower[h.nflower++] = M4_SEASONS + 2;     /* another's season */
    CHECK(m4_score_hand(&h, &s) == 2 && has(&s, MI_OWN_FLOWER) == 1 && !has(&s, MI_NO_FLOWERS));
    h.nflower = 0;
    for (int i = 0; i < 4; i++) h.flower[h.nflower++] = (uint8_t)(M4_FLOWERS + i);
    CHECK(m4_score_hand(&h, &s) == 4 && has(&s, MI_FLOWER_SET) == 2 && has(&s, MI_OWN_FLOWER) == 1);
    /* the way it was won */
    h = H("123o 456o 789o 55b", "chow 123c", "5b", 1);
    h.last = h.replacement = 1;
    CHECK(m4_score_hand(&h, &s) == 5 && has(&s, MI_LAST_TILE) && has(&s, MI_REPLACEMENT) && has(&s, MI_SELF_DRAWN));
    h.self_drawn = h.replacement = h.last = 0;
    h.robbing = 1;
    CHECK(m4_score_hand(&h, &s) == 3 && has(&s, MI_ROBBING));
    /* the limit caps everything */
    h = H("11223344556677o", NULL, "7o", 1);    /* seven pairs, full flush, concealed, self-drawn... */
    CHECK(m4_score_hand(&h, &s) == M4_LIMIT && s.raw > M4_LIMIT);
    CHECK(M4_POINTS[3] == 8 && M4_POINTS[M4_LIMIT] == 128);
}

/* ---- positions set by hand: tiles given out by kind, each tile once ---------------------------------------------- */
static int used[M4_TILES];
static int tile_of(int kind)
{
    for (int id = 0; id < M4_TILES; id++)
        if (!used[id] && m4_kind(id) == kind) { used[id] = 1; return id; }
    return -1;
}

static void give(int q, const char *s)
{
    uint8_t k[20];
    int n = kinds(s, k);
    P->nhand[q] = 0;
    for (int i = 0; i < n; i++) {
        int id = tile_of(k[i]);
        CHECK(id >= 0);                         /* the test itself: no fifth tile of a kind */
        P->hand[q][P->nhand[q]++] = (uint8_t)id;
    }
}

/* a fresh position: player 0 deals and holds 14 (the last one drawn), nothing else on the table; the wall made of the
 * tiles left */
static void setup(const char *h0, const char *h1, const char *h2, const char *h3)
{
    m4_new(&G, M4_EAST, 7);
    memset(used, 0, sizeof used);
    give(0, h0);
    give(1, h1);
    give(2, h2);
    give(3, h3);
    memset(P->nmeld, 0, sizeof P->nmeld);
    memset(P->nflower, 0, sizeof P->nflower);
    memset(P->npond, 0, sizeof P->npond);
    int n = 0;
    for (int id = 0; id < M4_TILES; id++)
        if (!used[id] && !m4_bonus(m4_kind(id))) P->wall[n++] = (uint8_t)id;
    for (int id = 0; id < M4_TILES; id++)
        if (!used[id] && m4_bonus(m4_kind(id))) P->wall[n++] = (uint8_t)id;   /* the flowers at the far end */
    P->head = 0;
    P->tail = (int16_t)n;
    P->turn = 0;
    P->dealer = 0;
    P->phase = MP_DISCARD;
    P->drawn = P->hand[0][P->nhand[0] - 1];
    G.start = *P;
    G.n = 0;
}

static int play(const char *moves) { return m4_script(&G, moves); }

#define H0 "5o 1b2b3b 4b5b6b 7b8b9b 1c2c3c E"
#define P1_CHOW "3o4o 9c9c 1o1o 7c8c 2c3c4c ES"
#define P1_NONE "1o1o 9c9c 3o3o 7c8c 2c3c4c ES"
#define P1_WIN "4o6o 1b2b3b 4b5b6b 7c8c9c NN"
#define P2_PUNG "5o5o 6c6c 7o8o 9o9o 4c5c6c SS"
#define P2_NONE "2o2o 6c6c 7o8o 9o9o 4c5c6c SS"
#define P2_KONG "5o5o5o 6c 7o8o 9o9o 4c5c6c SS"
#define P3_WIN "4o6o 1b2b3b 4b5b6b 7c8c9c WW"
#define P3_NONE "4o7o 1b2b3b 4b5b6b 7c8c9c NN"

/* 0 holds a pung of 3c exposed and draws the fourth; 1 waits on the 3c */
static void setup_rob(void)
{
    setup("5o 1b2b3b 4b5b6b 7b8b9b 3c", "1o2o3o 4o5o6o 7o8o9o 1c2c 9c9c", P2_NONE, P3_NONE);
    int k = M4_CHARS + 2;
    P->meld[0][0] = (m4meld){MS_PUNG, (uint8_t)k, 3, 3, {(uint8_t)tile_of(k), (uint8_t)tile_of(k), (uint8_t)tile_of(k),
                                                         M4_NONE}, M4_NONE};
    P->nmeld[0] = 1;
    G.start = *P;
}

static void test_claims(void)
{
    int ch[3], nch;
    /* 0 discards the 5o: 1 could chow, 2 pung, 3 win; 2 and 3 are asked (in that order), then 1 only if they can win */
    setup(H0, P1_CHOW, P2_PUNG, P1_WIN);
    CHECK(play("x5o"));
    CHECK(P->phase == MP_CLAIM && P->nask == 3 && P->ask[0] == 2 && P->ask[1] == 3 && P->ask[2] == 1);
    CHECK(P->opts[1] == MO_CHOW && P->opts[2] == MO_PUNG && P->opts[3] == MO_WIN);
    CHECK(m4_options(P, ch, &nch) == MO_PUNG && m4_actor(P) == 2);
    CHECK(!m4_legal(P, MA_CHOW | 2) && !m4_legal(P, MA_WIN) && m4_legal(P, MA_PASS));
    CHECK(play("u") && m4_actor(P) == 3 && P->pend == 2);    /* 2's pung waits: 3 may still win */
    CHECK(play("w"));                           /* 3 wins: 1 (who could only chow) is not asked */
    CHECK(P->phase == MP_SCORE && P->result.winner == 3 && P->result.payer == 0 && P->result.score.faan == 3);
    /* the payments: the discarder twice the points, the two others once */
    int pts = M4_POINTS[P->result.score.faan];
    CHECK(P->result.pay[0] == -2 * pts && P->result.pay[1] == -pts && P->result.pay[2] == -pts &&
          P->result.pay[3] == 4 * pts && P->total[3] == 4 * pts && P->npond[0] == 0 && P->nhand[3] == 14);
    CHECK(m4_actor(P) == 1 && play("n") && P->dealer == 1 && P->turn == 1 && P->total[3] == 32);
    /* the same, 3 passes: 2's pung is made, 1 is not asked */
    setup(H0, P1_CHOW, P2_PUNG, P1_WIN);
    CHECK(play("x5o u p"));
    CHECK(P->phase == MP_DISCARD && P->turn == 2 && P->drawn == M4_NONE && P->nmeld[2] == 1 &&
          P->meld[2][0].type == MS_PUNG && P->meld[2][0].from == 0 && P->nhand[2] == 11 && P->npond[0] == 0);
    CHECK(!m4_legal(P, MA_WIN) && m4_kongs(P, NULL) == 0);   /* after a claim: only a discard */
    CHECK(play("xS") && P->from == 2 && m4_actor(P) == 3);   /* play goes on after the claimer */
    /* nobody but the next player can use it: no one else is asked; the chow */
    setup(H0, P1_CHOW, P2_NONE, P3_NONE);
    CHECK(play("x5o") && P->nask == 1 && m4_actor(P) == 1);
    CHECK(m4_options(P, ch, &nch) == MO_CHOW && nch == 1 && ch[0] == 2);   /* 3-4-5 */
    CHECK(!m4_legal(P, MA_CHOW | 3) && play("c3o"));
    CHECK(P->turn == 1 && P->meld[1][0].type == MS_CHOW && P->meld[1][0].kind == 2 && P->meld[1][0].claimed != M4_NONE);
    /* nobody at all: the next player passes, that is draws */
    setup(H0, P1_NONE, P2_NONE, P3_NONE);
    int live = m4_live(P);
    CHECK(play("x5o") && P->nask == 1 && m4_options(P, NULL, NULL) == 0);
    CHECK(play("p") && P->turn == 1 && P->phase == MP_DISCARD && P->nhand[1] == 14 && m4_live(P) == live - 1);
    /* two wins on one discard: the first after the discarder takes it (1 before 3) */
    setup(H0, P1_WIN, P2_PUNG, P3_WIN);
    CHECK(play("x5o") && P->nask == 3 && P->opts[1] == (MO_WIN | MO_CHOW));
    CHECK(play("p w") && m4_actor(P) == 1 && m4_options(P, NULL, NULL) == MO_WIN);   /* 3's win waits; 1 can win */
    CHECK(play("w") && P->result.winner == 1);
    setup(H0, P1_WIN, P2_PUNG, P3_WIN);
    CHECK(play("x5o p w p") && P->result.winner == 3);        /* 1 lets it go: 3 wins */
    setup(H0, P1_WIN, P2_PUNG, P3_WIN);
    CHECK(play("x5o u p") && m4_actor(P) == 1 && play("p") && P->turn == 2 && P->meld[2][0].type == MS_PUNG);
    /* the kong of a discard: a replacement from the dead wall's far end */
    setup(H0, P1_CHOW, P2_KONG, P3_NONE);
    int tail = P->tail;
    CHECK(play("x5o") && P->opts[2] == (MO_PUNG | MO_KONG) && play("k p"));
    CHECK(P->turn == 2 && P->meld[2][0].type == MS_KONG && P->meld[2][0].n == 4 && P->drawn != M4_NONE &&
          P->replacement && P->tail < tail && P->nhand[2] == 11);
    /* the last discard, the wall used up: only a win; else the hand is drawn at once */
    setup(H0, P1_CHOW, P2_PUNG, P3_NONE);
    P->head = (int16_t)(P->tail - M4_DEAD);
    G.start = *P;
    CHECK(m4_live(P) == 0 && play("x5o") && P->phase == MP_SCORE && P->result.winner == M4_NONE);
    CHECK(P->total[0] == 0 && P->total[1] == 0 && m4_actor(P) == 1);
    setup(H0, P1_CHOW, P2_PUNG, P1_WIN);
    P->head = (int16_t)(P->tail - M4_DEAD);
    G.start = *P;
    CHECK(play("x5o") && P->nask == 3 && P->opts[2] == 0 && P->opts[3] == MO_WIN && play("p w"));
    CHECK(P->result.winner == 3 && has(&P->result.score, MI_LAST_TILE));
    /* a concealed kong: its replacement */
    setup("RRRR 1b2b3b 4b5b6b 7b8b9b E", P1_NONE, P2_NONE, P3_NONE);
    CHECK(m4_kongs(P, NULL) == 1 && play("KR"));
    CHECK(P->meld[0][0].type == MS_KONG && P->meld[0][0].from == M4_NONE && P->nhand[0] == 11 && P->replacement);
    /* an added kong, robbed */
    setup_rob();
    CHECK(m4_kongs(P, NULL) == 1 && play("K3c"));
    CHECK(P->phase == MP_ROB && P->nask == 3 && P->ask[0] == 1 && P->ask[1] == 2 && P->ask[2] == 3);
    CHECK(m4_actor(P) == 1 && m4_options(P, NULL, NULL) == MO_WIN);
    CHECK(play("w") && P->result.winner == 1 && P->result.payer == 0 && has(&P->result.score, MI_ROBBING));
    CHECK(P->meld[0][0].type == MS_PUNG && P->meld[0][0].n == 3 && P->nhand[1] == 14);
    /* let go by all: the kong stands, its replacement drawn; undo back into the window */
    setup_rob();
    CHECK(play("K3c p p p") && P->phase == MP_DISCARD && P->turn == 0 && P->meld[0][0].type == MS_KONG &&
          P->replacement);
    m4pos keep = *P;
    CHECK(m4_undo(&G) && P->phase == MP_ROB && P->k == 2 && play("p") && !memcmp(&keep, P, sizeof keep));
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int count_tiles(const m4pos *p)
{
    int seen[M4_TILES] = {0}, n = 0;
    for (int i = p->head; i < p->tail; i++) seen[p->wall[i]]++, n++;
    for (int q = 0; q < M4_P; q++) {
        for (int i = 0; i < p->nhand[q]; i++) seen[p->hand[q][i]]++, n++;
        for (int i = 0; i < p->nflower[q]; i++) seen[p->flower[q][i]]++, n++;
        for (int i = 0; i < p->npond[q]; i++) seen[p->pond[q][i]]++, n++;
        for (int m = 0; m < p->nmeld[q]; m++)
            for (int i = 0; i < p->meld[q][m].n; i++) seen[p->meld[q][m].tile[i]]++, n++;
    }
    for (int id = 0; id < M4_TILES; id++)
        if (seen[id] != 1) return 0;
    return n == M4_TILES;
}

static void test_deal(void)
{
    int ok = 1, flowers = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        m4_new(&G, M4_FULL, seed);
        ok &= count_tiles(P) && P->phase == MP_DISCARD && P->turn == 0 && P->dealer == 0 && P->wind == 0;
        int dice = P->dice[0] + P->dice[1] + P->dice[2];
        ok &= dice >= 3 && dice <= 18 && P->start == (((4 - (dice - 1) % 4) % 4) * 18 + dice) % M4_STACKS;
        for (int q = 0; q < M4_P; q++) {
            ok &= P->nhand[q] == (q == 0 ? 14 : 13);
            for (int i = 0; i < P->nhand[q]; i++) ok &= !m4_bonus(m4_kind(P->hand[q][i]));
            for (int i = 0; i < P->nflower[q]; i++) ok &= m4_bonus(m4_kind(P->flower[q][i]));
            flowers += P->nflower[q];
        }
        ok &= P->head == 53 && P->tail == M4_TILES - (P->nflower[0] + P->nflower[1] + P->nflower[2] + P->nflower[3]);
        ok &= m4_live(P) == P->tail - 53 - M4_DEAD && m4_seat(P, 0) == 0 && m4_seat(P, 3) == 3;
    }
    CHECK(ok);
    CHECK(flowers > 200);                       /* about 2.9 a deal */
    m4_new(&G, M4_EAST, 5);
    m4game H2;
    m4_new(&H2, M4_EAST, 5);
    CHECK(!memcmp(&G.pos, &H2.pos, sizeof G.pos));   /* a seed is a deal */
}

/* ---- whole games ------------------------------------------------------------------------------------------------- */
static uint32_t rnd;
static int pick(int n) { return (int)(cards_rand(&rnd) % (uint32_t)n); }

static int sane(const m4pos *p)
{
    if (!count_tiles(p)) return 0;
    int32_t sum = 0;
    for (int q = 0; q < M4_P; q++) {
        sum += p->total[q];
        int held = p->nhand[q] + 3 * p->nmeld[q];
        if (p->phase == MP_DISCARD && q == p->turn) {
            if (held != 14) return 0;
        } else if (p->phase == MP_SCORE && q == p->result.winner) {
            if (held != 14) return 0;
        } else if (p->phase != MP_ROB && held != 13) return 0;
    }
    return sum == 0;
}

static void test_random(void)
{
    int games = 0, ended = 0, ok = 1, wins = 0, self = 0, draws = 0, claims = 0, kongs = 0, robs = 0, undo_ok = 1;
    int faan_ok = 1, pay_ok = 1, limit = 0;
    for (uint32_t seed = 1; seed <= 120; seed++) {
        int v = seed % 2;
        m4_new(&G, v, seed * 7919u);
        rnd = seed;
        for (int step = 0; step < 60000 && P->phase != MP_OVER; step++) {
            int acts[64], n = 0;
            if (P->phase == MP_DISCARD) {
                for (int i = 0; i < P->nhand[P->turn]; i++) acts[n++] = MA_DISCARD | P->hand[P->turn][i];
                int ks[8], nk = m4_kongs(P, ks);
                for (int i = 0; i < nk; i++) acts[n++] = MA_KONG | ks[i];
            } else if (P->phase == MP_CLAIM || P->phase == MP_ROB) {
                int ch[3], nch, o = m4_options(P, ch, &nch);
                acts[n++] = MA_PASS;
                if (o & MO_PUNG) acts[n++] = MA_PUNG;
                if (o & MO_KONG) acts[n++] = MA_KONG | m4_kind(P->tile);
                for (int i = 0; i < nch; i++) acts[n++] = MA_CHOW | ch[i];
            }
            int a = m4_auto(P);
            if (MA_KIND(a) != MA_WIN && MA_KIND(a) != MA_NEXT && n && pick(3) == 0) a = acts[pick(n)];
            int legal = 1;
            for (int i = 0; i < n; i++) legal &= m4_legal(P, acts[i]);
            ok &= legal;
            int phase = P->phase;
            if (!m4_play(&G, a)) { ok = 0; break; }
            claims += phase == MP_CLAIM && (MA_KIND(a) == MA_PUNG || MA_KIND(a) == MA_CHOW || MA_KIND(a) == MA_KONG);
            kongs += MA_KIND(a) == MA_KONG;
            if (P->phase == MP_ROB) robs++;
            if (P->phase == MP_SCORE && phase != MP_SCORE) {
                const m4result *r = &P->result;
                if (r->winner == M4_NONE) draws++;
                else {
                    wins++;
                    self += r->payer == M4_NONE;
                    limit += r->score.faan == M4_LIMIT;
                    faan_ok &= r->score.faan >= M4_MIN && r->score.faan <= M4_LIMIT;
                    int pts = M4_POINTS[r->score.faan];
                    pay_ok &= r->pay[r->winner] == (r->payer == M4_NONE ? 6 : 4) * pts;
                    if (r->payer != M4_NONE) pay_ok &= r->pay[r->payer] == -2 * pts;
                }
            }
            if (P->phase != MP_OVER && !sane(P)) { ok = 0; break; }
            if (step % 97 == 0 && G.n) {        /* undo: the same position again by replay */
                m4pos keep = *P;
                uint16_t last = G.act[G.n - 1];
                undo_ok &= m4_undo(&G) && m4_play(&G, last) && !memcmp(&keep, P, sizeof keep);
            }
        }
        games++;
        ended += P->phase == MP_OVER && m4_winners(P) != 0;
        ok &= P->phase != MP_OVER || P->wind == (v == M4_FULL ? 2 : 1);
    }
    CHECK(ok);
    CHECK(undo_ok);
    CHECK(faan_ok);
    CHECK(pay_ok);
    CHECK(ended == games);
    CHECK(wins > 50 && self > 5 && draws > 50 && claims > 100 && kongs > 5);
    printf("mahjong4: %d games, %d hands won (%d self-drawn, %d at the limit), %d drawn, %d claims, %d kongs, "
           "%d robbing windows\n", games, wins, self, limit, draws, claims, kongs, robs);
    /* the plain actions and the notation: whole games end too */
    for (int v = 0; v < M4_VARIANTS; v++) {
        m4_new(&G, v, 99u + (unsigned)v);
        CHECK(play("*") && P->phase == MP_OVER && m4_winners(P) != 0);
    }
    m4_new(&G, M4_FULL, 3);
    CHECK(play("# n #") && P->hands == 1 && (P->phase == MP_SCORE || P->phase == MP_OVER));
    CHECK(!play("x9z") && !play("q"));
    int won = 0;                                /* the notation's plain play: on to a win, take it */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        m4_new(&G, M4_FULL, seed);
        if (play("!") && P->phase != MP_OVER)
            won += m4_legal(P, MA_WIN) && play("t") && P->phase == MP_SCORE && P->result.winner != M4_NONE;
    }
    CHECK(won >= 10);
    m4_new(&G, M4_EAST, 4);
    CHECK(play("?") && (m4_options(P, NULL, NULL) || m4_legal(P, MA_WIN) || m4_kongs(P, NULL)));
}

int main(void)
{
    test_readings();
    test_claims();
    test_deal();
    test_random();
    printf("mahjong4: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
