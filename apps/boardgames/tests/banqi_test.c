/*
 * Banqi: the rules' test (the deal, the first flip's colours, the ranks, the soldier and the general, the cannon's
 * jump, face-down pieces safe, the ends, undo, whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/banqi.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bqgame G;

/* a position: "Gc2 sd2 ?a1": a letter a piece (red capitals, black small letters: G general, A advisor, E elephant,
 * R chariot, H horse, C cannon, S soldier; ? a face-down red soldier), side to move, colours decided */
static void setup(const char *pieces, int side)
{
    static const char *KINDS = "?GAERHCS";
    bq_new(&G, 1);
    bqpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    for (const char *t = pieces; *t; t++) {
        if (*t == ' ') continue;
        int s = bq_square(t + 1);
        if (*t == '?') p->sq[s] = BQ_PC(0, BQ_SOLDIER) | BQ_HIDDEN;
        else {
            int black = *t >= 'a', c = black ? *t - 32 : *t;
            const char *k = strchr(KINDS, c);
            if (!k || s < 0) { printf("bad setup %s\n", t); failed++; return; }
            p->sq[s] = BQ_PC(black, k - KINDS);
        }
        t += 2;
    }
    p->side = (uint8_t)side;
    p->decided = 1;
}

static int can(const char *from, const char *to)
{
    bqmove m[BQ_MAX_MOVES];
    int n = bq_legal(bq_pos(&G), m), a = bq_square(from), b = bq_square(to);
    for (int i = 0; i < n; i++)
        if (m[i].kind != BQ_FLIP && m[i].from == a && m[i].to == b) return 1;
    return 0;
}

static void test_deal(void)
{
    int count[2][8] = {{0}}, hidden = 0;
    bq_new(&G, 7);
    const bqpos *p = bq_pos(&G);
    for (int s = 0; s < BQ_N; s++) {
        hidden += (p->sq[s] & BQ_HIDDEN) != 0;
        count[BQ_COLOUR(p->sq[s])][BQ_TYPE(p->sq[s])]++;
    }
    CHECK(hidden == 32);
    for (int c = 0; c < 2; c++)
        CHECK(count[c][BQ_GENERAL] == 1 && count[c][BQ_ADVISOR] == 2 && count[c][BQ_ELEPHANT] == 2 &&
              count[c][BQ_CHARIOT] == 2 && count[c][BQ_HORSE] == 2 && count[c][BQ_CANNON] == 2 &&
              count[c][BQ_SOLDIER] == 5);
    bqpos a, b;
    bq_deal(&a, 7);
    bq_deal(&b, 8);
    CHECK(memcmp(a.sq, p->sq, BQ_N) == 0);      /* the same seed, the same deal */
    CHECK(memcmp(a.sq, b.sq, BQ_N) != 0);
    bqmove m[BQ_MAX_MOVES];
    CHECK(bq_legal(p, m) == 32);                /* only flips at first */
    CHECK(bq_status(&G) == BQ_PLAY && p->side == 0 && !p->decided);
    bq_new(&G, 1);                             /* "@7": the deal of seed 7, before any move */
    CHECK(bq_script(&G, "@7 a1") && memcmp(G.pos[0].sq, a.sq, BQ_N) == 0 && G.ply == 1);
    CHECK(!bq_script(&G, "@8"));
}

/* the first flip gives its colour to the flipper; the other colour moves next */
static void test_first_flip(void)
{
    for (uint32_t seed = 1; seed < 40; seed++) {
        bq_new(&G, seed);
        int s = 5, c = BQ_COLOUR(bq_pos(&G)->sq[s]);
        CHECK(bq_script(&G, "f1"));
        const bqpos *p = bq_pos(&G);
        CHECK(p->decided && p->side == !c && !(p->sq[s] & BQ_HIDDEN));
        CHECK(!bq_script(&G, "f1"));            /* face up now: not a flip */
        CHECK(bq_undo(&G) && !bq_pos(&G)->decided && bq_pos(&G)->side == 0);
    }
}

static void test_ranks(void)
{
    CHECK(bq_beats(BQ_GENERAL, BQ_ADVISOR) && bq_beats(BQ_GENERAL, BQ_GENERAL) && !bq_beats(BQ_GENERAL, BQ_SOLDIER));
    CHECK(bq_beats(BQ_SOLDIER, BQ_GENERAL) && bq_beats(BQ_SOLDIER, BQ_SOLDIER) && !bq_beats(BQ_SOLDIER, BQ_CANNON));
    CHECK(bq_beats(BQ_ELEPHANT, BQ_CHARIOT) && !bq_beats(BQ_CHARIOT, BQ_ELEPHANT));
    CHECK(bq_beats(BQ_HORSE, BQ_CANNON) && !bq_beats(BQ_CANNON, BQ_SOLDIER) && !bq_beats(BQ_SOLDIER, BQ_HORSE));
    CHECK(bq_beats(BQ_ADVISOR, BQ_SOLDIER) && !bq_beats(BQ_ADVISOR, BQ_GENERAL));
    setup("Gc2 sd2 ac3 Sa1 gb1 ?c1", 0);
    CHECK(!can("c2", "d2"));                    /* the general cannot take the soldier */
    CHECK(can("c2", "c3"));                     /* but the advisor */
    CHECK(!can("c2", "c1"));                    /* nor a face-down piece */
    CHECK(can("a1", "b1"));                     /* the soldier takes the general */
    CHECK(can("a1", "a2") && !can("a1", "b2")); /* one step along a row or a column only */
    CHECK(bq_script(&G, "a1b1"));
    CHECK(bq_pos(&G)->taken[1] == 1 && bq_pos(&G)->quiet == 0);
}

static void test_cannon(void)
{
    /* the cannon on a1: d1 (black horse) behind the screen b1; c1 is empty */
    setup("Ca1 ?b1 hd1 ge1 ra2 sa4", 0);
    CHECK(can("a1", "d1"));                     /* over exactly one piece, any distance */
    CHECK(!can("a1", "e1"));                    /* not over two */
    CHECK(!can("a1", "a2"));                    /* not next to it, without a screen */
    CHECK(can("a1", "a4"));                     /* a2 the screen, a3 empty: a4 */
    setup("Ca1 ra2 sa4 hd1", 0);
    CHECK(can("a1", "a4"));                     /* a soldier, a low rank: the cannon takes any */
    CHECK(!can("a1", "d1"));                    /* no screen */
    setup("Ca1 Ra2 Ga3 ca4", 0);
    CHECK(!can("a1", "a3") && !can("a1", "a4")); /* not its own pieces; only the first piece past the screen */
    setup("Ca1 Ra2 ?a3", 0);
    CHECK(!can("a1", "a3"));                    /* face down: safe */
    setup("Ca1", 0);
    CHECK(can("a1", "b1") && can("a1", "a2"));  /* steps like the others */
}

static void test_end(void)
{
    setup("Ra1 rh4", 1);
    G.pos[0].taken[1] = 15;
    CHECK(bq_status(&G) == BQ_PLAY);
    setup("Ra1 rb1", 0);
    G.pos[0].taken[1] = 15;
    CHECK(bq_script(&G, "a1b1") && bq_status(&G) == BQ_WIN0);   /* all sixteen taken */
    setup("Sa1 Gb1 ga2 eb2", 0);
    CHECK(bq_status(&G) == BQ_PLAY);            /* the soldier can take the general */
    setup("Sa1 Sb1 ea2 eb2 ec1", 0);
    CHECK(bq_status(&G) == BQ_WIN1);            /* no move: red loses */
    setup("Ra1 rh4", 0);                        /* quiet turns: a draw at fifty */
    const char *walk[4] = {"a1a2", "h4h3", "a2a1", "h3h4"};
    int i = 0;
    while (bq_status(&G) == BQ_PLAY && i < 100) CHECK(bq_script(&G, walk[i++ % 4]));
    CHECK(bq_status(&G) == BQ_DRAW && i == BQ_QUIET);
}

static uint32_t rnd = 12345;
static int next(int n)
{
    rnd = rnd * 1103515245u + 12345u;
    return (int)((rnd >> 16) % (uint32_t)n);
}

/* whole games: random legal moves; the pieces always 32 less the captured ones, undo back to the start */
static void test_random(void)
{
    int ends[4] = {0};
    for (int game = 0; game < 300; game++) {
        rnd = (uint32_t)game * 7919u + 1;
        bq_new(&G, (uint32_t)game);
        bqpos start = *bq_pos(&G);
        while (bq_status(&G) == BQ_PLAY) {
            bqmove m[BQ_MAX_MOVES];
            int n = bq_legal(bq_pos(&G), m);
            CHECK(n > 0);
            int k = next(n);
            for (int i = 0; i < n; i++)         /* captures preferred half the time: games that end */
                if (m[i].kind == BQ_TAKE && next(2)) k = i;
            CHECK(bq_play(&G, m[k]));
            const bqpos *p = bq_pos(&G);
            int pieces = 0;
            for (int s = 0; s < BQ_N; s++) pieces += p->sq[s] != 0;
            CHECK(pieces == 32 - p->taken[0] - p->taken[1]);
            if (G.ply >= BQ_MAX_PLY) break;
        }
        ends[bq_status(&G)]++;
        while (bq_undo(&G)) {}
        CHECK(memcmp(bq_pos(&G), &start, sizeof start) == 0);
    }
    CHECK(ends[BQ_WIN0] > 0 && ends[BQ_WIN1] > 0 && ends[BQ_PLAY] == 0);
    printf("random games: red %d, black %d, draws %d\n", ends[BQ_WIN0], ends[BQ_WIN1], ends[BQ_DRAW]);
}

int main(void)
{
    test_deal();
    test_first_flip();
    test_ranks();
    test_cannon();
    test_end();
    test_random();
    printf("banqi: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
