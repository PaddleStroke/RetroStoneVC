/*
 * Poker Run: the rules' test (every hand type and its edge cases, the wheel; the scoring's order with charms, the
 * enhancements, the bosses; the economy; whole runs played by the bot from seeds, replayed and scripted the same; the
 * bot's results). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pokerrun.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static pkr_game G, H;
static pkr_state *S = &G.s;

/* a hand type from cards written "AS 2H 10D QC" */
static int parse_card(const char *t, int *len)
{
    int r, i = 0;
    if (t[0] == '1' && t[1] == '0') r = 10, i = 2;
    else {
        const char *R = "A23456789?JQK";
        const char *p = strchr(R, t[0]);
        r = p ? (int)(p - R) + 1 : 0;
        i = 1;
    }
    const char *U = "SHDC";
    int s = (int)(strchr(U, t[i]) - U);
    *len = i + 1;
    return CARD(s, r);
}

static int eval_str(const char *str, uint8_t *scoring, int prism_mask)
{
    uint8_t rank[5], suits[5];
    int n = 0, len;
    while (*str) {
        while (*str == ' ') str++;
        if (!*str) break;
        int c = parse_card(str, &len);
        str += len;
        rank[n] = (uint8_t)CARD_RANK(c);
        suits[n] = prism_mask >> n & 1 ? 0xf : (uint8_t)(1 << CARD_SUIT(c));
        n++;
    }
    return pkr_eval(rank, suits, n, scoring);
}

static void test_eval(void)
{
    uint8_t m;
    CHECK(eval_str("AS", &m, 0) == HT_HIGH && m == 1);
    CHECK(eval_str("2S 9H AD 5C", &m, 0) == HT_HIGH && m == 4);   /* the ace is high */
    CHECK(eval_str("KS 9H 4D", &m, 0) == HT_HIGH && m == 1);
    CHECK(eval_str("5S 5H", &m, 0) == HT_PAIR && m == 3);
    CHECK(eval_str("5S 9H 5D KC", &m, 0) == HT_PAIR && m == 5);   /* the kickers do not score */
    CHECK(eval_str("5S 5H 9D 9C KC", &m, 0) == HT_TWO && m == 15);
    CHECK(eval_str("7S 7H 7D 2C", &m, 0) == HT_TRIPS && m == 7);
    CHECK(eval_str("AS 2H 3D 4C 5C", &m, 0) == HT_STRAIGHT && m == 31);   /* the wheel */
    CHECK(eval_str("10S JH QD KC AC", &m, 0) == HT_STRAIGHT);           /* ace high */
    CHECK(eval_str("6S 2H 3D 4C 5C", &m, 0) == HT_STRAIGHT);
    CHECK(eval_str("KS AH 2D 3C 4C", &m, 0) == HT_HIGH);                 /* no going round the corner */
    CHECK(eval_str("AS 2H 3D 4C", &m, 0) == HT_HIGH);                    /* four cards: no straight */
    CHECK(eval_str("2H 7H 9H JH KH", &m, 0) == HT_FLUSH && m == 31);
    CHECK(eval_str("2H 7H 9H JH", &m, 0) == HT_HIGH);                    /* four cards: no flush */
    CHECK(eval_str("2H 7H 9S JH KH", &m, 4) == HT_FLUSH);                /* a prism card: any suit */
    CHECK(eval_str("2H 7H 9S JH KH", &m, 0) == HT_HIGH);
    CHECK(eval_str("QS QH QD 4C 4H", &m, 0) == HT_FULL && m == 31);
    CHECK(eval_str("4S QH 4D QC 4H", &m, 0) == HT_FULL);
    CHECK(eval_str("9S 9H 9D 9C 2H", &m, 0) == HT_QUADS && m == 15);
    CHECK(eval_str("9S 9H 9D 9C", &m, 0) == HT_QUADS && m == 15);
    CHECK(eval_str("5D 6D 7D 8D 9D", &m, 0) == HT_SFLUSH && m == 31);
    CHECK(eval_str("AC 2C 3C 4C 5C", &m, 0) == HT_SFLUSH);              /* the wheel in one suit */
    CHECK(eval_str("10H JH QH KH AH", &m, 0) == HT_SFLUSH);
    CHECK(eval_str("5D 6D 7D 8D 9S", &m, 16) == HT_SFLUSH);
    CHECK(eval_str("5D 5D 5D 5D 5D", &m, 0) == HT_QUADS && m == 31);    /* five alike (a deck with copies) */
}

/* a blind set up by hand: the hand's cards, nothing else drawn */
static void setup(const char *cards)
{
    pkr_new(&G, 1);
    S->phase = PH_PLAY;
    S->ante = 1, S->blind = 0;
    S->target = 1000000;
    S->hands_left = 4, S->discards_left = 3, S->hand_size = 8;
    S->ndraw = 0, S->nhand = 0;
    int len;
    while (*cards) {
        while (*cards == ' ') cards++;
        if (!*cards) break;
        int c = parse_card(cards, &len);
        cards += len;
        for (int i = 0; i < S->ndeck; i++)
            if (S->deck[i].card == c) S->hand[S->nhand++] = (uint8_t)i;
    }
}

static void give(int charm)
{
    S->charm[S->ncharms].id = (uint8_t)charm;
    S->charm[S->ncharms].price = pkr_charms[charm].price;
    S->charm[S->ncharms].val = charm == CH_IVY ? 100 : 0;
    S->ncharms++;
}

static int idx_of(const char *card)
{
    int len, c = parse_card(card, &len);
    for (int i = 0; i < S->ndeck; i++)
        if (S->deck[i].card == c) return i;
    return -1;
}

static void test_scoring(void)
{
    pkr_log L;
    /* a pair of fives: (10 + 5 + 5) x 2 */
    setup("5S 5H 9D KC 2H");
    CHECK(pkr_preview(S, 3, &L) == 40 && L.type == HT_PAIR && L.chips == 20 && L.mult == 2);
    /* the order of the charms: +5 mult then x2 makes (2 + 5) x 2, the other way 2 x 2 + 5 */
    setup("5S 5H 9D KC 2H");
    give(CH_WORRY), give(CH_MOTH);
    CHECK(pkr_preview(S, 3, &L) == 20 * 14);
    setup("5S 5H 9D KC 2H");
    give(CH_MOTH), give(CH_WORRY);
    CHECK(pkr_preview(S, 3, &L) == 20 * 9);
    /* moved along the row: the first to the second place, the order back */
    CHECK(pkr_apply(S, PKR_ACT(A_MOVE, 0 * 8 + 1)) && S->charm[0].id == CH_WORRY && S->charm[1].id == CH_MOTH);
    CHECK(pkr_preview(S, 3, &L) == 20 * 14);
    CHECK(!pkr_legal(S, PKR_ACT(A_MOVE, 0 * 8 + 2)) && !pkr_legal(S, PKR_ACT(A_MOVE, 1 * 8 + 1)));
    /* per card: the ruby adds 4 mult for the heart that scores (5H), not for the 2H held */
    setup("5S 5H 9D KC 2H");
    give(CH_RUBY);
    CHECK(pkr_preview(S, 3, &L) == 20 * 6);
    /* the steps: base, then each card's chips and the charm's mult, in order */
    CHECK(L.step[0].kind == SS_BASE && L.step[0].chips == 10 && L.step[0].mult == 2);
    CHECK(L.step[1].kind == SS_CARD && L.step[1].what == SV_CHIPS && L.step[1].idx == 0 && L.step[1].chips == 15);
    CHECK(L.step[2].kind == SS_CARD && L.step[2].idx == 1 && L.step[2].chips == 20);
    CHECK(L.step[3].kind == SS_CARD && L.step[3].what == SV_MULT && L.step[3].src == 2 && L.step[3].mult == 6);
    CHECK(L.nsteps == 4);
    /* the court lens: a face card scores twice; the gong: the first hand twice */
    setup("KS KH 9D");
    give(CH_LENS);
    CHECK(pkr_preview(S, 3, &L) == (10 + 40) * 2);
    setup("9S 9H 2D");
    give(CH_GONG);
    CHECK(pkr_preview(S, 3, &L) == (10 + 36) * 2);
    S->hands_played = 1;
    CHECK(pkr_preview(S, 3, &L) == (10 + 18) * 2);
    /* the times-mults are rounded: jade x1.25 per club, 2 -> 3 (2.5 up) -> 4 (3.75) */
    setup("9C 9S 4C 4D");
    give(CH_JADE);
    CHECK(pkr_preview(S, 15, &L) == (20 + 26) * 4 && L.mult == 4);   /* 9C: 2 x 1.25 = 2.5 -> 3; 4C: 3.75 -> 4 */
    /* the enhancements: etched +30 chips, ember +4 mult, crystal x2, iron x1.5 held */
    setup("7S 7H 3D");
    S->deck[idx_of("7S")].enh = EN_ETCHED;
    S->deck[idx_of("7H")].enh = EN_EMBER;
    CHECK(pkr_preview(S, 3, &L) == (10 + 14 + 30) * 6);
    S->deck[idx_of("3D")].enh = EN_IRON;
    CHECK(pkr_preview(S, 3, &L) == (10 + 14 + 30) * 9);
    S->deck[idx_of("7H")].enh = EN_CRYSTAL;
    CHECK(pkr_preview(S, 3, &L) == (10 + 14 + 30) * 6);   /* 2 x 2 = 4, x1.5 held = 6 */
    /* the bosses: low tide halves the base, the fading suit's cards score nothing, rust turns enhancements off */
    setup("7S 7H 3D");
    S->blind = 2, S->boss = BO_TIDE;
    CHECK(pkr_preview(S, 3, &L) == (5 + 14) * 1);
    S->boss = BO_FADE, S->boss_suit = SUIT_HEARTS;
    CHECK(pkr_preview(S, 3, &L) == (10 + 7) * 2);
    S->boss = BO_RUST;
    S->deck[idx_of("7S")].enh = EN_ETCHED;
    CHECK(pkr_preview(S, 3, &L) == (10 + 14) * 2);
    /* a prism card is every suit: a flush, and the ruby counts it a heart */
    setup("2S 6S 9S JS KD");
    S->deck[idx_of("KD")].enh = EN_PRISM;
    CHECK(pkr_preview(S, 31, &L) && L.type == HT_FLUSH);
    give(CH_RUBY);
    CHECK(pkr_preview(S, 31, &L) == (40 + 2 + 6 + 9 + 10 + 10) * 8);
    /* the open door: every card played scores */
    setup("5S 5H 9D");
    give(CH_DOOR);
    CHECK(pkr_preview(S, 7, &L) == (10 + 19) * 2);
    /* the growing charms: the snowball grows with each hand played */
    setup("5S 5H 9D 2C 3C 8S 8H JD");
    S->ndraw = 0;
    give(CH_SNOW);
    CHECK(pkr_apply(S, PKR_ACT(A_PLAY, 3)) && S->log.total == 20 * 3 && S->charm[0].val == 1);
    /* levels: a tome makes the pair 25 chips x 3 */
    setup("5S 5H");
    S->level[HT_PAIR] = 2;
    CHECK(pkr_preview(S, 3, &L) == 35 * 3);
}

static void test_economy(void)
{
    pkr_new(&G, 7);
    CHECK(S->phase == PH_BLIND && S->coins == 4 && S->ante == 1 && S->target == pkr_target(1, 0, S->boss));
    CHECK(pkr_target(1, 0, BO_FADE) == 250 && pkr_target(1, 1, BO_FADE) == 375 && pkr_target(1, 2, BO_FADE) == 500);
    CHECK(pkr_target(1, 2, BO_WALL) == 750);
    CHECK(pkr_target(9, 0, 0) > pkr_target(8, 0, 0) && pkr_target(12, 0, 0) > pkr_target(11, 0, 0));
    S->coins = 23;
    CHECK(pkr_interest(S) == 4);
    S->coins = 80;
    CHECK(pkr_interest(S) == 5);
    S->charm[0].id = CH_LEDGER, S->ncharms = 1;
    CHECK(pkr_interest(S) == 10);
    S->ncharms = 0;
    /* a blind won: the reward, a coin a hand left, the interest */
    pkr_new(&G, 7);
    CHECK(pkr_apply(S, PKR_ACT(A_BEGIN, 0)) && S->nhand == 8 && S->ndraw == 44 && S->hands_left == 4);
    S->target = 1;
    S->coins = 12;
    CHECK(pkr_apply(S, PKR_ACT(A_PLAY, 1)) && S->phase == PH_CASH);
    CHECK(S->cash_blind == 3 && S->cash_hands == 3 && S->cash_interest == 2);
    CHECK(pkr_apply(S, PKR_ACT(A_CASH, 0)) && S->coins == 12 + 3 + 3 + 2 && S->phase == PH_SHOP);
    /* the shop: buying, selling, rerolling */
    int coins = S->coins, bought = -1;
    for (int i = 0; i < PKR_SHOP_ITEMS; i++)
        if (S->shop[i].kind == IT_CHARM) bought = i;
    if (bought >= 0) {
        int price = pkr_price(S, &S->shop[bought]);
        CHECK(pkr_apply(S, PKR_ACT(A_BUY, bought)) && S->ncharms == 1 && S->coins == coins - price);
        CHECK(!pkr_apply(S, PKR_ACT(A_BUY, bought)));          /* sold already */
        CHECK(pkr_apply(S, PKR_ACT(A_SELL, 0)) && S->ncharms == 0 && S->coins == coins - price + price / 2);
    }
    coins = S->coins;
    int cost = pkr_reroll_cost(S);
    CHECK(pkr_apply(S, PKR_ACT(A_REROLL, 0)) && S->coins == coins - cost && pkr_reroll_cost(S) == cost + 1);
    S->coins = 0;
    CHECK(!pkr_legal(S, PKR_ACT(A_REROLL, 0)));
    CHECK(pkr_apply(S, PKR_ACT(A_LEAVE, 0)) && S->phase == PH_BLIND && S->blind == 1);
    /* the haggler: a coin off */
    pkr_item it = {IT_CHARM, CH_WORRY, 3, 0, {0, 0}};
    S->charm[0].id = CH_HAGGLE, S->ncharms = 1;
    CHECK(pkr_price(S, &it) == 2);
    /* gilded cards held and the clay pig at the cash-out; a crystal that breaks leaves the deck after the blind */
    setup("5S 5H 9D KC 2H");
    S->target = 1;
    S->deck[idx_of("KC")].enh = EN_GILDED;
    give(CH_PIG);
    S->coins = 0;
    CHECK(pkr_apply(S, PKR_ACT(A_PLAY, 3)) && S->phase == PH_CASH && S->cash_gilded == 3 && S->cash_charms == 4);
    int broke = 0;
    for (uint32_t seed = 1; seed < 60 && !broke; seed++) {
        setup("5S 5H 9D");
        S->rng = seed;
        S->target = 1;
        S->deck[idx_of("5S")].enh = EN_CRYSTAL;
        pkr_apply(S, PKR_ACT(A_PLAY, 3));
        if (S->log.broke) {
            broke = 1;
            CHECK(S->ndeck == 51 && S->phase == PH_CASH);
        }
    }
    CHECK(broke);
    /* the toll bridge: a coin a hand; the fog: some cards dealt face down */
    setup("5S 5H 9D 2C");
    S->blind = 2, S->boss = BO_TOLL, S->coins = 3;
    CHECK(pkr_apply(S, PKR_ACT(A_PLAY, 3)) && S->coins == 2 && S->log.coins == -1);
    pkr_new(&G, 4);
    S->blind = 2, S->boss = BO_FOG;
    pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    int hidden = 0;
    for (int k = 0; k < 20; k++) {              /* over a few discards, some come face down */
        for (int i = 0; i < S->nhand; i++) hidden += S->hid[S->hand[i]];
        if (S->discards_left) pkr_apply(S, PKR_ACT(A_DISCARD, 31));
        else S->discards_left = 3;
    }
    CHECK(hidden > 0);
    /* a card bundle: the card chosen joins the deck */
    pkr_new(&G, 5);
    S->phase = PH_SHOP, S->coins = 50;
    S->shop[3].kind = IT_PACK, S->shop[3].id = PK_CARD, S->shop[3].price = 4, S->shop[3].sold = 0;
    CHECK(pkr_apply(S, PKR_ACT(A_BUY, 3)) && S->phase == PH_PACK && S->npack == 4);
    CHECK(pkr_apply(S, PKR_ACT(A_PICK, 2)) && S->ndeck == 53 && S->phase == PH_SHOP);
    /* an enhancement: two cards at most */
    S->shop[0].kind = IT_ENH, S->shop[0].id = EN_PRISM, S->shop[0].price = 3, S->shop[0].sold = 0;
    CHECK(pkr_apply(S, PKR_ACT(A_BUY, 0)) && S->phase == PH_ENH && S->npick == PKR_PICK);
    CHECK(!pkr_legal(S, PKR_ACT(A_ENH, 7)) && pkr_apply(S, PKR_ACT(A_ENH, 5)));
    CHECK(S->deck[S->pick[0]].enh == EN_PRISM && S->deck[S->pick[2]].enh == EN_PRISM);
    /* a lost blind */
    pkr_new(&G, 9);
    pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    for (int h = 0; h < 4; h++) CHECK(pkr_apply(S, PKR_ACT(A_PLAY, 1 << 7)));   /* the lowest card alone */
    CHECK(S->phase == PH_LOST && pkr_over(S));
    /* bosses: the short rope, the tight fist, the dry well */
    pkr_new(&G, 3);
    S->blind = 2;
    S->boss = BO_ROPE, pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    CHECK(S->hands_left == 3);
    S->phase = PH_BLIND, S->boss = BO_FIST, pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    CHECK(S->nhand == 7);
    S->phase = PH_BLIND, S->boss = BO_WELL, pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    CHECK(S->discards_left == 0 && !pkr_legal(S, PKR_ACT(A_DISCARD, 1)));
    S->phase = PH_BLIND, S->boss = BO_GATE, pkr_apply(S, PKR_ACT(A_BEGIN, 0));
    CHECK(!pkr_legal(S, PKR_ACT(A_PLAY, 3)) && pkr_legal(S, PKR_ACT(A_PLAY, 31)));
}

/* whole runs by the bot: the same seed, the same run; replayed from its actions, and from its notation */
static int run_bot(pkr_game *g, uint32_t seed, int max_ante)
{
    pkr_new(g, seed);
    for (int i = 0; i < PKR_MAX_ACTS - 1 && !pkr_over(&g->s); i++) {
        int a = pkr_bot(&g->s);
        if (!a || !pkr_play(g, a)) { printf("bot stuck: seed %u phase %d act %x\n", seed, g->s.phase, a); return -1; }
        if (g->s.ante > max_ante) break;
    }
    return g->s.won;
}

static void test_runs(void)
{
    static pkr_game R;
    static char notes[PKR_MAX_ACTS * 6];
    int wins = 0, ante_sum = 0, runs = 300, stuck = 0, ok_replay = 1, ok_script = 1;
    int ante_hist[12] = {0};
    for (int k = 0; k < runs; k++) {
        uint32_t seed = 1000 + (uint32_t)k * 7919;
        int w = run_bot(&G, seed, 99);
        if (w < 0) { stuck++; continue; }
        wins += w;
        ante_sum += G.s.ante;
        ante_hist[G.s.ante > 11 ? 11 : G.s.ante]++;
        if (k < 40) {
            run_bot(&H, seed, 99);
            CHECK(H.n == G.n && !memcmp(&H.s, &G.s, sizeof H.s));
            pkr_new(&R, seed);                  /* the actions replayed */
            for (int i = 0; i < G.n; i++) pkr_play(&R, G.act[i]);
            ok_replay &= !memcmp(&R.s, &G.s, sizeof R.s);
            int len = 0;                        /* the notation, scripted */
            for (int i = 0; i < G.n; i++) {
                len += pkr_notation(G.act[i], notes + len, (int)sizeof notes - len);
                notes[len++] = ' ';
            }
            notes[len] = 0;
            pkr_new(&R, seed);
            ok_script &= pkr_script(&R, notes) && !memcmp(&R.s, &G.s, sizeof R.s);
        }
    }
    CHECK(!stuck);
    CHECK(ok_replay);
    CHECK(ok_script);
    printf("bot: %d runs, %d won (%.1f%%), mean ante reached %.2f; antes:", runs, wins, 100.0 * wins / runs,
           (double)ante_sum / runs);
    for (int a = 1; a < 12; a++) printf(" %d", ante_hist[a]);
    printf("\n");
    CHECK(wins >= 1);                           /* it sometimes wins */
    CHECK(wins < runs / 2);                     /* not too easy */
    CHECK(ante_sum >= 3 * runs);                /* it plays sensibly */
    /* different seeds, different runs */
    pkr_new(&G, 1), pkr_new(&H, 2);
    pkr_apply(&G.s, PKR_ACT(A_BEGIN, 0)), pkr_apply(&H.s, PKR_ACT(A_BEGIN, 0));
    CHECK(memcmp(G.s.hand, H.s.hand, 8) != 0);
    /* "#b": the bot to the next blind */
    pkr_new(&G, 5);
    CHECK(pkr_script(&G, "b #b") && (G.s.phase == PH_BLIND || pkr_over(&G.s)));
    CHECK(!pkr_script(&G, "zz"));
}

int main(void)
{
    test_eval();
    test_scoring();
    test_economy();
    test_runs();
    printf("pokerrun: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
