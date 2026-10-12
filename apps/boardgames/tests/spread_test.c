/*
 * Spread: the rules' test (the start and the boards, clones and jumps, the pieces turned, the end: a full board, no
 * move and the empty cells to the other player, the idle count; undo; whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/spread.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static spdgame G;

static int at(const char *s) { return spd_pos(&G)->c[spd_cell(s)]; }

/* a position: "a1 b2" player 0's pieces, then player 1's, "#" blocked cells; side to move */
static void setup(const char *p0, const char *p1, const char *blocks, int side)
{
    spd_new(&G, 0);
    spdpos *p = &G.pos[0];
    memset(p, 0, sizeof *p);
    const char *l[3] = {p0, p1, blocks};
    for (int k = 0; k < 3; k++)
        for (const char *t = l[k]; *t; t++) {
            if (*t == ' ') continue;
            p->c[spd_cell(t)] = (uint8_t)(k == 2 ? SPD_BLOCK : SPD_P0 + k);
            t++;
        }
    p->side = (uint8_t)side;
}

static void test_start(void)
{
    for (int b = 0; b < SPD_BOARDS; b++) {
        spd_new(&G, b);
        const spdpos *p = spd_pos(&G);
        CHECK(at("a1") == SPD_P0 && at("g7") == SPD_P0 && at("a7") == SPD_P1 && at("g1") == SPD_P1);
        CHECK(spd_count(p, 0) == 2 && spd_count(p, 1) == 2 && p->side == 0);
        int blocks = 0;
        for (int c = 0; c < SPD_CELLS; c++) {
            blocks += p->c[c] == SPD_BLOCK;
            /* symmetric: turned round, and mirrored with the players swapped */
            int x = c % SPD_N, y = c / SPD_N, r = SPD_AT(6 - x, 6 - y), m = SPD_AT(6 - x, y);
            CHECK((p->c[c] == SPD_BLOCK) == (p->c[r] == SPD_BLOCK) && (p->c[c] == SPD_BLOCK) == (p->c[m] == SPD_BLOCK));
        }
        CHECK(b == 0 ? blocks == 0 : blocks >= 4);
    }
    spd_new(&G, 0);
    spdmove m[SPD_MAX_MOVES];
    CHECK(spd_legal(spd_pos(&G), m) == 16);     /* each corner: 3 clones, 5 jumps */
}

static void test_moves(void)
{
    spd_new(&G, 0);
    CHECK(spd_script(&G, "a1b2"));              /* a clone: a1 stays */
    CHECK(at("a1") == SPD_P0 && at("b2") == SPD_P0 && spd_pos(&G)->idle == 0 && spd_pos(&G)->side == 1);
    CHECK(spd_script(&G, "a7a5"));              /* a jump: a7 is left */
    CHECK(at("a7") == SPD_EMPTY && at("a5") == SPD_P1 && spd_pos(&G)->idle == 1);
    CHECK(!spd_script(&G, "a1a4"));             /* three away */
    CHECK(!spd_script(&G, "b2a1"));             /* onto a piece */
    CHECK(!spd_script(&G, "a5a4"));             /* not the mover's */
    /* the turns: every enemy piece next to the cell reached */
    setup("d4", "c5 d5 e5 c3 e3 f6 b4", "", 0);
    CHECK(spd_script(&G, "d4c4"));              /* c4's neighbours: b4 c5 d5 c3, not e5 e3 f6 */
    CHECK(at("b4") == SPD_P0 && at("c5") == SPD_P0 && at("d5") == SPD_P0 && at("c3") == SPD_P0);
    CHECK(at("e5") == SPD_P1 && at("e3") == SPD_P1 && at("f6") == SPD_P1);
    CHECK(spd_count(spd_pos(&G), 0) == 6 && spd_count(spd_pos(&G), 1) == 3);
    setup("a1", "b2", "a2 b1", 0);
    CHECK(!spd_script(&G, "a1a2"));             /* blocked */
    CHECK(spd_script(&G, "a1c1"));              /* jumps over a block */
    CHECK(at("b2") == SPD_P0);
}

static void test_end(void)
{
    /* player 1 has no move: player 0 takes the empty cells */
    setup("a1 b1 c1 a2 c2 a3 b3 c3", "b2", "", 1);
    CHECK(spd_status(&G) == SPD_PLAY);          /* b2 can jump out */
    setup("a1 b1 c1 d1 e1 a2 c2 e2 a3 b3 c3 d3 e3 a4 b4 c4 d4 e4 a5 b5 c5 d5 e5 d2", "b2", "", 1);
    CHECK(spd_status(&G) == SPD_WIN0 && spd_filler(&G) == 0);   /* hemmed in: no move */
    setup("d4", "", "", 1);                     /* no pieces: no move */
    CHECK(spd_status(&G) == SPD_WIN0 && spd_filler(&G) == 0 && spd_score(&G, 0) == 49 && spd_score(&G, 1) == 0);
    /* a full board */
    setup("", "", "", 0);
    for (int c = 0; c < SPD_CELLS; c++) G.pos[0].c[c] = (uint8_t)(c < 24 ? SPD_P0 : SPD_P1);
    CHECK(spd_status(&G) == SPD_WIN1 && spd_filler(&G) == -1);
    for (int c = 0; c < SPD_CELLS; c++) G.pos[0].c[c] = (uint8_t)(c == 48 ? SPD_BLOCK : c < 24 ? SPD_P0 : SPD_P1);
    CHECK(spd_status(&G) == SPD_DRAW);
    /* 50 jumps in a row: counted */
    setup("a1", "g7", "", 0);
    const char *walk[4] = {"a1a3", "g7g5", "a3a1", "g5g7"};
    int i = 0;
    while (spd_status(&G) == SPD_PLAY && i < 100) CHECK(spd_script(&G, walk[i++ % 4]));
    CHECK(i == SPD_IDLE && spd_status(&G) == SPD_DRAW && spd_filler(&G) == -1);
}

static uint32_t rnd;
static int next(int n)
{
    rnd = rnd * 1103515245u + 12345u;
    return (int)((rnd >> 16) % (uint32_t)n);
}

static void test_random(void)
{
    int ends[4] = {0}, longest = 0;
    static spdmove m[SPD_MAX_MOVES];
    for (int game = 0; game < 400; game++) {
        rnd = (uint32_t)game * 2654435761u + 7;
        spd_new(&G, game % SPD_BOARDS);
        spdpos start = *spd_pos(&G);
        int blocks = 0;
        for (int c = 0; c < SPD_CELLS; c++) blocks += start.c[c] == SPD_BLOCK;
        while (spd_status(&G) == SPD_PLAY) {
            const spdpos *p = spd_pos(&G);
            int n = spd_legal(p, m), k = next(n);
            CHECK(n > 0 && n <= SPD_MAX_MOVES);
            if (next(3)) {                      /* clones more often than jumps: games fill up */
                for (int i = 0; i < n; i++)
                    if (spd_dist(m[i].from, m[i].to) == 1 && next(2)) { k = i; break; }
            }
            int before = spd_count(p, 0) + spd_count(p, 1), clone = spd_dist(m[k].from, m[k].to) == 1;
            CHECK(spd_play(&G, m[k]));
            p = spd_pos(&G);
            CHECK(spd_count(p, 0) + spd_count(p, 1) == before + clone);
            int b = 0;
            for (int c = 0; c < SPD_CELLS; c++) b += p->c[c] == SPD_BLOCK;
            CHECK(b == blocks);
            CHECK(G.ply <= SPD_MAX_PLY);
        }
        int s0 = spd_score(&G, 0), s1 = spd_score(&G, 1), st = spd_status(&G);
        CHECK(st == (s0 > s1 ? SPD_WIN0 : s1 > s0 ? SPD_WIN1 : SPD_DRAW));
        if (spd_filler(&G) >= 0) CHECK(s0 + s1 + blocks == SPD_CELLS);
        ends[st]++;
        if (G.ply > longest) longest = G.ply;
        while (spd_undo(&G)) {}
        CHECK(memcmp(spd_pos(&G), &start, sizeof start) == 0);
    }
    CHECK(ends[SPD_WIN0] > 0 && ends[SPD_WIN1] > 0);
    printf("random games: %d / %d / draws %d, the longest %d moves\n", ends[SPD_WIN0], ends[SPD_WIN1],
           ends[SPD_DRAW], longest);
}

int main(void)
{
    test_start();
    test_moves();
    test_end();
    test_random();
    printf("spread: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
