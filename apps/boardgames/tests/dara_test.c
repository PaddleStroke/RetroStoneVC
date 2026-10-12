/*
 * Dara: the rules' test (placing without rows of three, the pass, moving, rows of exactly three and their capture, a
 * row of four, two rows at once, the end, the draw, undo; whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/dara.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static dargame G;

static int at(const char *s) { return dar_pos(&G)->c[dar_hole(s)]; }

/* a moving position: player 0's holes, player 1's, the side to move */
static void setup(const char *p0, const char *p1, int side)
{
    dar_new(&G);
    darpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    const char *l[2] = {p0, p1};
    for (int k = 0; k < 2; k++)
        for (const char *t = l[k]; *t; t++) {
            if (*t == ' ') continue;
            p->c[dar_hole(t)] = (uint8_t)(k + 1);
            t++;
        }
    p->phase = DAR_MOVE;
    p->side = (uint8_t)side;
}

static void test_place(void)
{
    dar_new(&G);
    CHECK(dar_pos(&G)->phase == DAR_PLACE && dar_pos(&G)->hand[0] == 12 && dar_pos(&G)->side == 0);
    CHECK(dar_script(&G, "a1 f5 b1 f4"));
    CHECK(!dar_script(&G, "c1"));               /* a1 b1 c1: three in a row */
    CHECK(dar_script(&G, "d1"));                /* a gap: fine */
    CHECK(!dar_script(&G, "f3"));               /* f5 f4 f3: a column of three */
    CHECK(!dar_script(&G, "a1"));               /* taken */
    CHECK(dar_script(&G, "e3"));
    CHECK(!dar_script(&G, "c1"));               /* a1 b1 c1 d1: four, still not allowed */
    darpos *p = &G.pos[G.ply];
    CHECK(p->hand[0] == 9 && p->hand[1] == 9);
    /* a whole placing: 24 pieces, then player 0 moves */
    dar_new(&G);
    const char *fill = "a1 a2 b1 b2 e1 e2 f1 f2 a3 a4 b3 b4 e3 e4 f3 f4 a5 c1 b5 d1 e5 c3 f5 d3";
    CHECK(dar_script(&G, fill));
    p = &G.pos[G.ply];
    CHECK(p->phase == DAR_MOVE && p->side == 0 && dar_count(p, 0) == 12 && dar_count(p, 1) == 12);
    CHECK(dar_status(&G) == DAR_PLAY);
}

/* a player who cannot place passes; once neither can, the moving begins */
static void test_pass(void)
{
    dar_new(&G);
    darpos *p = &G.pos[0];
    for (int h = 0; h < DAR_N; h++)            /* runs of two at most: rows 1 1 2 2 1 1, columns alternating */
        p->c[h] = (uint8_t)(((h % DAR_W + 2 * (h / DAR_W)) % 4 < 2) ? 1 : 2);
    p->c[dar_hole("c2")] = p->c[dar_hole("d2")] = 0;
    p->hand[0] = 2;
    p->hand[1] = 1;
    CHECK(!dar_can_place(p, 1, dar_hole("c2")) && !dar_can_place(p, 1, dar_hole("d2")));   /* a2 b2, e2 f2 */
    CHECK(dar_can_place(p, 0, dar_hole("c2")) && dar_can_place(p, 0, dar_hole("d2")));
    CHECK(dar_script(&G, "d2"));
    CHECK(dar_pos(&G)->side == 0 && dar_pos(&G)->phase == DAR_PLACE);   /* player 1 passes */
    CHECK(dar_script(&G, "c2"));
    CHECK(dar_pos(&G)->phase == DAR_MOVE && dar_pos(&G)->side == 1 && dar_pos(&G)->hand[1] == 0);
}

static void test_lines(void)
{
    setup("a1 b1 d2 a5", "f5 f4 e3", 0);
    CHECK(dar_script(&G, "d2c2"));              /* no row */
    CHECK(dar_pos(&G)->phase == DAR_MOVE && dar_pos(&G)->side == 1);
    setup("a1 b1 c2 a5", "f5 f4 e3 d5", 0);
    CHECK(dar_script(&G, "c2c1"));              /* a1 b1 c1: exactly three */
    CHECK(dar_pos(&G)->phase == DAR_TAKE && dar_pos(&G)->side == 0);
    CHECK(dar_pos(&G)->line == ((1u << dar_hole("a1")) | (1u << dar_hole("b1")) | (1u << dar_hole("c1"))));
    CHECK(!dar_script(&G, "xa5"));              /* not one's own */
    CHECK(dar_script(&G, "xf4"));
    CHECK(at("f4") == 0 && dar_pos(&G)->side == 1 && dar_pos(&G)->phase == DAR_MOVE && dar_pos(&G)->quiet == 0);
    setup("a1 b1 d1 c2 a5", "f5 f4 e3", 0);
    CHECK(dar_script(&G, "c2c1"));              /* a1 b1 c1 d1: four, no capture */
    CHECK(dar_pos(&G)->phase == DAR_MOVE && dar_pos(&G)->side == 1);
    setup("a1 b1 c4 c5 d2", "f5 f4 e3 e2", 0);
    CHECK(dar_script(&G, "d2c2"));              /* nothing: c2 joins neither a1 b1 nor c4 c5 */
    setup("a2 b2 c3 c4 d1", "f5 f4 e3 e4", 0);
    CHECK(dar_script(&G, "d1c1") && dar_pos(&G)->phase == DAR_MOVE);
    setup("a2 b2 c3 c4 c1", "f5 f4 e3 e4", 0);
    CHECK(dar_script(&G, "c1c2"));              /* a2 b2 c2 and c2 c3 c4: two rows, one capture */
    CHECK(dar_pos(&G)->phase == DAR_TAKE);
    CHECK(dar_script(&G, "e3") && dar_pos(&G)->side == 1 && dar_pos(&G)->phase == DAR_MOVE);
}

static void test_end(void)
{
    setup("a1 b1 c2", "f5 f4 e3", 0);
    CHECK(dar_script(&G, "c2c1 f4"));           /* player 1 is left with two */
    CHECK(dar_status(&G) == DAR_WIN0);
    setup("a1 b2 c3", "a2 b1 f5", 1);
    CHECK(dar_status(&G) == DAR_PLAY);          /* f5 can move */
    setup("a1 a2 b1 c5 d5", "f5 e5 e4 f4", 0);
    CHECK(dar_status(&G) == DAR_PLAY);
    setup("a1 a2 b1 b2 c5 d5 e4 f3", "f5 e5 f4", 1);   /* hemmed in */
    CHECK(dar_status(&G) == DAR_WIN0);
    setup("a1 c3 e1", "f5 d3 b5", 0);           /* quiet moves: a draw at 100 */
    const char *walk[4] = {"a1a2", "f5f4", "a2a1", "f4f5"};
    int i = 0;
    while (dar_status(&G) == DAR_PLAY && i < 200) CHECK(dar_script(&G, walk[i++ % 4]));
    CHECK(i == DAR_QUIET && dar_status(&G) == DAR_DRAW);
}

static uint32_t rnd;
static int next(int n)
{
    rnd = rnd * 1103515245u + 12345u;
    return (int)((rnd >> 16) % (uint32_t)n);
}

static void test_random(void)
{
    int ends[4] = {0};
    for (int game = 0; game < 500; game++) {
        rnd = (uint32_t)game * 40503u + 3;
        dar_new(&G);
        while (dar_status(&G) == DAR_PLAY) {
            const darpos *p = dar_pos(&G);
            darmove m[DAR_MAX_MOVES];
            int n = dar_legal(p, m);
            CHECK(n > 0);
            int k = next(n);
            if (p->phase == DAR_MOVE)           /* rows of three preferred: games that end */
                for (int i = 0; i < n; i++) {
                    darpos q = *p;
                    dar_make(&q, m[i]);
                    if (q.phase == DAR_TAKE && next(4)) k = i;
                }
            int phase = p->phase, before = dar_count(p, !p->side);
            CHECK(dar_play(&G, m[k]));
            p = dar_pos(&G);
            if (phase == DAR_PLACE)             /* never a row of three while placing */
                for (int h = 0; h < DAR_N; h++) CHECK(!p->c[h] || dar_line(p, h) == 0);
            if (phase == DAR_TAKE) CHECK(dar_count(p, p->side) == before - 1);
            CHECK(dar_count(p, 0) <= 12 && dar_count(p, 1) <= 12);
        }
        ends[dar_status(&G)]++;
        while (dar_undo(&G)) {}
        CHECK(dar_pos(&G)->hand[0] == 12 && dar_count(dar_pos(&G), 0) == 0);
    }
    CHECK(ends[DAR_WIN0] > 0 && ends[DAR_WIN1] > 0);
    printf("random games: %d / %d / draws %d\n", ends[DAR_WIN0], ends[DAR_WIN1], ends[DAR_DRAW]);
}

int main(void)
{
    test_place();
    test_pass();
    test_lines();
    test_end();
    test_random();
    printf("dara: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
