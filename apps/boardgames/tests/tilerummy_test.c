/*
 * Tile Rummy: the rules' test (the tiles, the deal, runs and groups with jokers, the first meld, rearranging the
 * table, winning a joker back, drawing, undo and reset, the end of a round and its scoring, blocked rounds, several
 * rounds, the notation; whole games from seeds, with random moves on the table). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "cards.h"
#include "games/tilerummy.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static tgame G, H;
static tpos *P = &G.pos;

/* a tile: colour 0..3 (red, blue, amber, black), number 1..13, copy 0 or 1; jokers 104, 105 */
static int T(int c, int v, int k) { return k * 52 + c * 13 + v - 1; }
enum { J0 = 104, J1 = 105 };

static void test_tiles(void)
{
    int seen[256] = {0}, ok = 1;
    for (int t = 0; t < TR_TILES; t++) seen[tr_face(t)]++;
    for (int c = 0; c < 4; c++)
        for (int v = 1; v <= 13; v++) ok &= seen[CARD(c, v)] == 2 && tr_face(T(c, v, 1)) == CARD(c, v);
    CHECK(ok && tr_is_joker(J0) && tr_is_joker(J1) && !tr_is_joker(T(3, 13, 1)));
    CHECK(tr_penalty(J0) == 30 && tr_penalty(T(2, 11, 0)) == 11);
}

static int count_where(const tpos *p, int w)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++) n += p->where[t] == w;
    return n;
}

static void test_deal(void)
{
    tr_new(&G, TR_ONE, 4, 42);
    tr_new(&H, TR_ONE, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(count_where(P, TR_RACK) == 14 && count_where(P, TR_RACK + 3) == 14 && P->npool == 106 - 56);
    CHECK(count_where(P, TR_POOL) == P->npool && P->nsets == 0 && P->turn == 0 && !P->out[0]);
    tr_new(&H, TR_ONE, 4, 43);
    CHECK(memcmp(H.pos.where, G.pos.where, sizeof G.pos.where) != 0);
    tr_new(&G, TR_MATCH, 2, 5);
    CHECK(P->npool == 106 - 28 && P->variant == TR_MATCH);
}

/* a table made by hand: everything in the pool, then the sets and the racks */
static void clear(int players)
{
    tr_new(&G, TR_ONE, players, 1);
    for (int t = 0; t < TR_TILES; t++) P->where[t] = TR_POOL;
    P->npool = 0;
    for (int t = 0; t < TR_TILES; t++) P->pool[P->npool++] = (uint8_t)t;
    P->nsets = 0;
}

static void give(int w, const int *t, int n)
{
    for (int i = 0; i < n; i++) {
        P->where[t[i]] = (uint8_t)w;
        for (int j = 0; j < P->npool; j++)      /* out of the pool */
            if (P->pool[j] == t[i]) {
                memmove(&P->pool[j], &P->pool[j + 1], (size_t)(P->npool - j - 1));
                P->npool--;
                break;
            }
    }
    if (w < TR_MAXSETS && w >= P->nsets) P->nsets = (uint8_t)(w + 1);
}

static void ready(void)                         /* the turn starts from the table made */
{
    G.start = G.pos;
    G.nundo = 0;
}

static int kind_of(const int *t, int n, int *value)
{
    clear(2);
    give(0, t, n);
    int kind;
    tr_set(P, 0, NULL, &kind, value);
    return kind;
}

static void test_sets(void)
{
    int v;
    const int run[] = {T(0, 5, 0), T(0, 3, 0), T(0, 4, 1)};
    CHECK(kind_of(run, 3, &v) == TS_RUN && v == 12);
    const int grp[] = {T(0, 9, 0), T(2, 9, 1), T(3, 9, 0), T(1, 9, 0)};
    CHECK(kind_of(grp, 4, &v) == TS_GROUP && v == 36);
    const int two[] = {T(0, 9, 0), T(2, 9, 1)};
    CHECK(kind_of(two, 2, &v) == TS_BAD);
    const int same_colour[] = {T(0, 9, 0), T(0, 9, 1), T(3, 9, 0)};   /* a colour twice in a group */
    CHECK(kind_of(same_colour, 3, &v) == TS_BAD);
    const int gap[] = {T(1, 4, 0), T(1, 5, 0), T(1, 7, 0)};
    CHECK(kind_of(gap, 3, &v) == TS_BAD);
    const int joker_gap[] = {T(1, 4, 0), J0, T(1, 7, 0), T(1, 5, 1)};
    CHECK(kind_of(joker_gap, 4, &v) == TS_RUN && v == 4 + 5 + 6 + 7);
    uint8_t o[8];
    tr_set(P, 0, o, NULL, NULL);
    CHECK(o[0] == T(1, 4, 0) && o[1] == T(1, 5, 1) && o[2] == J0 && o[3] == T(1, 7, 0));   /* the joker as the 6 */
    const int wrap[] = {T(2, 12, 0), T(2, 13, 0), T(2, 1, 0)};         /* 1 does not follow 13 */
    CHECK(kind_of(wrap, 3, &v) == TS_BAD);
    const int high[] = {T(2, 12, 0), T(2, 13, 0), J1};                 /* the joker: 11 (no 14) */
    CHECK(kind_of(high, 3, &v) == TS_RUN && v == 36);
    const int end[] = {T(3, 5, 0), T(3, 6, 0), J1};                    /* the joker at the high end: 7 */
    CHECK(kind_of(end, 3, &v) == TS_RUN && v == 18);
    const int jg[] = {T(0, 8, 0), J0, T(3, 8, 1)};
    CHECK(kind_of(jg, 3, &v) == TS_GROUP && v == 24);
    const int five[] = {T(0, 8, 0), T(1, 8, 0), T(2, 8, 0), T(3, 8, 0), J0};   /* a group of five: no */
    CHECK(kind_of(five, 5, &v) == TS_BAD);
    const int full[] = {T(1, 1, 0), T(1, 2, 0), T(1, 3, 0), T(1, 4, 0), T(1, 5, 0), T(1, 6, 0), T(1, 7, 0),
                        T(1, 8, 0), T(1, 9, 0), T(1, 10, 0), T(1, 11, 0), T(1, 12, 0), T(1, 13, 0)};
    CHECK(kind_of(full, 13, &v) == TS_RUN && v == 91);
}

static void test_first_meld(void)
{
    clear(2);
    const int table[] = {T(1, 3, 0), T(1, 4, 0), T(1, 5, 0)};
    give(0, table, 3);
    const int rack[] = {T(0, 10, 0), T(0, 11, 0), T(0, 12, 0), T(2, 1, 0), T(2, 2, 0), T(2, 3, 0), T(1, 6, 0)};
    give(TR_RACK, rack, 7);
    const int other[] = {T(3, 1, 0), T(3, 2, 0)};
    give(TR_RACK + 1, other, 2);
    ready();
    CHECK(tr_check(&G) == TC_NOTHING && !tr_end(&G));
    uint8_t six = (uint8_t)T(1, 6, 0);
    CHECK(!tr_put(&G, &six, 1, 0));             /* not out: the table's set is not yours yet */
    uint8_t take = (uint8_t)T(1, 5, 0);
    CHECK(!tr_put(&G, &take, 1, TR_NEWSET) && !tr_can_put(&G, T(1, 3, 0), TR_TORACK));
    uint8_t low[] = {(uint8_t)T(2, 1, 0), (uint8_t)T(2, 2, 0), (uint8_t)T(2, 3, 0)};
    CHECK(tr_put(&G, low, 3, TR_NEWSET) && P->nsets == 2);
    CHECK(tr_meld(&G) == 6 && tr_check(&G) == TC_LOW && !tr_end(&G));
    uint8_t high[] = {(uint8_t)T(0, 10, 0), (uint8_t)T(0, 11, 0)};
    CHECK(tr_put(&G, high, 2, TR_NEWSET));
    CHECK(tr_check(&G) == TC_INVALID);          /* two tiles: not a set */
    uint8_t q = (uint8_t)T(0, 12, 0);
    CHECK(tr_put(&G, &q, 1, 2) && tr_meld(&G) == 33 + 6 && tr_check(&G) == TC_OK);
    CHECK(!tr_draw(&G));                        /* the table changed: no drawing */
    CHECK(tr_end(&G) && P->out[0] && P->turn == 1 && P->lastplay[T(0, 11, 0)] && !P->lastplay[T(1, 3, 0)]);
    CHECK(count_where(P, TR_RACK) == 1 && P->nsets == 3);
}

static void test_rearrange(void)
{
    clear(2);
    const int run[] = {T(0, 1, 0), T(0, 2, 0), T(0, 3, 0), T(0, 4, 0), T(0, 5, 0), T(0, 6, 0)};
    give(0, run, 6);
    const int grp[] = {T(0, 9, 0), T(1, 9, 0), J0};
    give(1, grp, 3);
    const int rack[] = {T(2, 9, 0), T(3, 7, 0), T(3, 8, 0), T(0, 7, 0), T(1, 1, 0)};
    give(TR_RACK, rack, 5);
    give(TR_RACK + 1, (const int[]){T(3, 1, 1)}, 1);
    P->out[0] = 1;
    ready();
    /* win the joker back: the amber 9 in the group, the joker to a new run with the black 7 and 8 */
    CHECK(tr_script(&G, "a9>1"));
    int kind;
    tr_set(P, 1, NULL, &kind, NULL);
    CHECK(kind == TS_GROUP);
    CHECK(!tr_can_put(&G, J0, TR_TORACK));      /* the table's joker never goes to a rack */
    CHECK(tr_script(&G, "k7+k8>n"));
    CHECK(tr_check(&G) == TC_INVALID);
    CHECK(tr_script(&G, "tj>2") && tr_check(&G) == TC_OK);
    /* split the run: 4 5 6 to a new set next to it; the red 7 joins it */
    CHECK(tr_script(&G, "tr4+tr5+tr6>n") && P->nsets == 4);
    uint8_t o[16];
    int n = tr_set(P, 1, o, &kind, NULL);
    CHECK(n == 3 && kind == TS_RUN && o[0] == T(0, 4, 0));   /* the new set right after the run it came from */
    CHECK(tr_script(&G, "r7>1") && tr_check(&G) == TC_OK && tr_laid(&G) == 4);
    /* undo, reset */
    CHECK(tr_undo(&G) && count_where(P, TR_RACK) == 2 && tr_check(&G) == TC_OK);
    tpos before = G.pos;
    CHECK(tr_script(&G, "b1>n") && tr_check(&G) == TC_INVALID);
    CHECK(tr_script(&G, "u") && !memcmp(&before, &G.pos, sizeof before));
    CHECK(tr_script(&G, "x") && !tr_changed(&G) && !tr_undo(&G) && !tr_reset(&G));
    /* draw: the table untouched; the turn passes */
    int pool = P->npool;
    CHECK(tr_draw(&G) && P->npool == pool - 1 && count_where(P, TR_RACK) == 6 && P->turn == 1);
}

static void test_round(void)
{
    clear(3);
    give(0, (const int[]){T(1, 7, 0), T(1, 8, 0), T(1, 9, 0)}, 3);
    give(TR_RACK, (const int[]){T(1, 10, 0)}, 1);
    give(TR_RACK + 1, (const int[]){T(0, 13, 0), J0}, 2);
    give(TR_RACK + 2, (const int[]){T(2, 2, 0), T(3, 3, 1)}, 2);
    P->out[0] = 1;
    ready();
    CHECK(tr_script(&G, "b10>0 e"));
    CHECK(P->phase == TP_OVER && P->winner == 0 && P->last[1] == -43 && P->last[2] == -5 && P->last[0] == 48);
    CHECK(tr_winners(P) == 1u && !tr_next(&G));
    /* several rounds: on to the next one, the first player one seat on */
    clear(2);
    P->variant = TR_MATCH;
    give(0, (const int[]){T(1, 7, 0), T(1, 8, 0), T(1, 9, 0)}, 3);
    give(TR_RACK, (const int[]){T(1, 10, 0)}, 1);
    give(TR_RACK + 1, (const int[]){T(0, 13, 0), T(0, 12, 1)}, 2);
    P->out[0] = 1;
    ready();
    CHECK(tr_script(&G, "b10>0 e") && P->phase == TP_ROUND && P->total[0] == 25 && P->total[1] == -25);
    CHECK(tr_script(&G, "n") && P->phase == TP_PLAY && P->turn == 1 && P->first == 1 && P->nsets == 0);
    CHECK(count_where(P, TR_RACK) == 14 && count_where(P, TR_RACK + 1) == 14 && !P->out[0] && P->total[0] == 25);
    /* blocked: the pool empty, everybody passing; the lowest rack wins the differences */
    clear(3);
    give(TR_RACK, (const int[]){T(1, 10, 0)}, 1);
    give(TR_RACK + 1, (const int[]){T(0, 3, 0)}, 1);
    give(TR_RACK + 2, (const int[]){T(2, 6, 0), T(3, 1, 0)}, 2);
    P->npool = 0;
    ready();
    CHECK(tr_draw(&G) && tr_draw(&G) && P->phase == TP_PLAY);
    CHECK(tr_draw(&G) && P->phase == TP_OVER && P->winner == 1);
    CHECK(P->last[0] == -7 && P->last[2] == -4 && P->last[1] == 11);
}

/* whole games: plain turns from seeds (with random moves tried and taken back first); every tile somewhere, the
 * table valid between turns, the scores adding up */
static uint32_t rnd;
static int pick(int n) { return (int)(cards_rand(&rnd) % (uint32_t)n); }

static int table_valid(const tpos *p)
{
    for (int k = 0; k < p->nsets; k++) {
        int kind;
        if (tr_set(p, k, NULL, &kind, NULL) < 3 || kind == TS_BAD) return 0;
    }
    return 1;
}

static void test_games(void)
{
    int valid = 1, counts = 1, sums = 1, over = 0, games = 0, reset_ok = 1, laid = 0;
    for (int seed = 1; seed <= 60; seed++) {
        int np = 2 + seed % 3, v = seed % 2;
        tr_new(&G, v, np, (uint32_t)seed * 2654435761u);
        rnd = (uint32_t)seed;
        for (int turn = 0; turn < 4000 && P->phase != TP_OVER; turn++) {
            if (P->phase == TP_PLAY && pick(3) == 0) {   /* random moves, then back to the start */
                tpos keep = G.pos;
                for (int m = 0; m < 6; m++) {
                    uint8_t t = (uint8_t)pick(TR_TILES);
                    int dest = pick(4) == 0 ? TR_NEWSET : pick(5) == 0 ? TR_TORACK
                               : P->nsets ? pick(P->nsets) : TR_NEWSET;
                    tr_put(&G, &t, 1, dest);
                }
                if (tr_check(&G) == TC_OK && !table_valid(P)) valid = 0;
                tr_reset(&G);
                if (memcmp(&keep, &G.pos, sizeof keep)) reset_ok = 0;
            }
            int was = P->phase, sets = P->nsets;
            if (!tr_auto(&G)) break;
            laid += P->nsets > sets;
            if (P->phase == TP_PLAY && !table_valid(P)) valid = 0;
            int n = P->npool + count_where(P, TR_POOL) * 0;
            for (int q = 0; q < P->players; q++) n += count_where(P, TR_RACK + q);
            for (int k = 0; k < P->nsets; k++) n += count_where(P, k);
            if (n != TR_TILES || count_where(P, TR_POOL) != P->npool) counts = 0;
            if (was == TP_PLAY && P->phase != TP_PLAY) {
                int s = 0;
                for (int q = 0; q < P->players; q++) s += P->last[q];
                if (s > 0 || s < -P->players) sums = 0;   /* zero, but for a shared gain rounded down */
            }
        }
        games++;
        over += P->phase == TP_OVER;
    }
    CHECK(valid);
    CHECK(counts);
    CHECK(sums);
    CHECK(reset_ok);
    CHECK(over == games);
    CHECK(laid > 100);
}

static void test_notation(void)
{
    tr_new(&G, TR_ONE, 2, 77);
    CHECK(!tr_script(&G, "q5>n") && !tr_script(&G, "r5") && !tr_script(&G, "e"));
    CHECK(tr_script(&G, "d") && P->turn == 1);
    CHECK(tr_script(&G, "a") && P->turn == 0);
    CHECK(tr_script(&G, "#") && P->phase == TP_OVER);
}

int main(void)
{
    test_tiles();
    test_deal();
    test_sets();
    test_first_meld();
    test_rearrange();
    test_round();
    test_games();
    test_notation();
    printf("tilerummy: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
