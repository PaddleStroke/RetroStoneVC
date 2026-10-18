/*
 * Corner Blocks' rules test (src/games/cornerblocks_rules.c, no SDL): the blocks (89 squares, 91 distinct turns), the
 * notation, the start squares, the corner rule (a corner of one's own: yes; an edge of one's own: no; the others'
 * edges: yes), the board's edges, the blocks used once, the turn passing over the players who are out, the scores and
 * their bonuses, the shared win, then random games for two, three and four (every move legal, the game ends when
 * nobody can play, the squares laid as counted).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/cornerblocks.h"

static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static cblgame G;

static int sq(int x, int y) { return y * CBL_MAXN + x; }

static int ok(const char *moves)
{
    cblgame g = G;
    return cbl_script(&g, moves);
}

int main(void)
{
    /* ---- the blocks ------------------------------------------------------------------------------------------- */
    int squares = 0, turns = 0;
    for (int b = 0; b < CBL_BLOCKS; b++) {
        squares += cbl_size(b);
        for (int o = 0; o < 8; o++) {
            cbl_shape s;
            cbl_shape_of(b, o, &s);
            turns += cbl_unique(b, o);
            CHECK(s.n == cbl_size(b) && s.w <= 5 && s.h <= 5 && cbl_unique(b, cbl_same(b, o)));
            int seen[5][5] = {{0}}, linked = 1;
            for (int i = 0; i < s.n; i++) seen[s.y[i]][s.x[i]]++;
            for (int i = 0; i < s.n; i++) {     /* each square beside another (one piece), none twice */
                int x = s.x[i], y = s.y[i], next = s.n == 1;
                CHECK(seen[y][x] == 1);
                if (x > 0) next |= seen[y][x - 1];
                if (x < 4) next |= seen[y][x + 1];
                if (y > 0) next |= seen[y - 1][x];
                if (y < 4) next |= seen[y + 1][x];
                linked &= next;
            }
            CHECK(linked);
        }
    }
    CHECK(squares == 89 && turns == 91);

    /* ---- two players: the start, the corner rule ------------------------------------------------------------ */
    cbl_newgame(&G, 2);
    const cblpos *p = cbl_pos(&G);
    CHECK(p->n == 14 && cbl_start(p, 0) == sq(4, 9) && cbl_start(p, 1) == sq(9, 4) && p->turn == 0);
    {
        int len;
        char nm[8];
        CHECK(cbl_parse(p, "t3c12", &len) == cbl_move(19, 3, sq(2, 11)) && len == 5);
        cbl_name(p, cbl_move(19, 3, sq(2, 11)), nm);
        CHECK(!strcmp(nm, "t3c12"));
        CHECK(cbl_parse(p, "v0a1", &len) < 0 && cbl_parse(p, "a8a1", &len) < 0 && cbl_parse(p, "a0o1", &len) < 0);
        CHECK(cbl_parse(p, "a0a15", &len) < 0 && cbl_parse(p, "a0a", &len) < 0);
    }
    CHECK(!ok("a0f10") && ok("a0e10") && ok("u0d9"));       /* the first block covers the start square */
    CHECK(!ok("j0a1"));
    CHECK(cbl_script(&G, "a0e10 a0j5"));
    CHECK(ok("b0f11"));                                     /* corner to corner */
    CHECK(!ok("b0e11") && !ok("b1e11"));                    /* along an edge of one's own */
    CHECK(!ok("b0g12"));                                    /* touching nothing of one's own */
    CHECK(!ok("a0f11"));                                    /* the single square: used */
    CHECK(!ok("e1n12") && !ok("e0l11"));                    /* off the board */
    CHECK(cbl_script(&G, "b0f11"));
    CHECK(!ok("b0i6"));                                     /* ebony: along its own edge */
    CHECK(ok("b1k6") && !ok("b1j6"));
    {   /* the others' edges may be touched; their squares may not be covered */
        cblpos q;
        cbl_new(&q, 2);
        q.cell[sq(9, 4)] = 2, q.cell[sq(10, 4)] = 1;
        q.left[1] = CBL_ALL & ~1u, q.left[0] = CBL_ALL & ~1u;
        q.turn = 1;
        CHECK(cbl_fits(&q, 1, 0, sq(10, 3)));               /* (10, 3) by ivory's (10, 4): fine */
        CHECK(!cbl_fits(&q, 1, 1, sq(10, 3)));              /* down onto ivory's square */
        int mv[2000], k = cbl_moves(&q, mv, 2000), all = k > 0;
        for (int i = 0; i < k; i++) all &= cbl_legal(&q, mv[i]);
        for (int i = 0; i < k; i++)
            for (int j = i + 1; j < k && all; j++) all &= mv[i] != mv[j];
        CHECK(all);
        int brute = 0;                              /* every legal move, found the slow way, by its distinct shape */
        for (int m = 0; m < CBL_MOVES; m++)
            brute += cbl_unique(cbl_block(m), cbl_orient(m)) && cbl_legal(&q, m);
        CHECK(brute == k);
    }

    /* ---- the scores, the shared win, the turn past those who are out ----------------------------------------- */
    {
        cblpos q;
        cbl_new(&q, 4);
        CHECK(q.n == 20 && cbl_start(&q, 0) == sq(0, 19) && cbl_start(&q, 1) == sq(0, 0) &&
              cbl_start(&q, 2) == sq(19, 0) &&
              cbl_start(&q, 3) == sq(19, 19));
        q.left[0] = 0, q.laid[0] = 89, q.last[0] = CBL_MONO;
        q.left[1] = 0, q.laid[1] = 89, q.last[1] = 7;
        q.left[2] = 1, q.laid[2] = 88;
        CHECK(cbl_score(&q, 0) == 109 && cbl_score(&q, 1) == 104 && cbl_score(&q, 2) == 88 && cbl_score(&q, 3) == 0);
        CHECK(cbl_winners(&q) == 1u);
        q.last[1] = CBL_MONO;
        CHECK(cbl_winners(&q) == 3u);
    }
    {   /* four: the corners; a player boxed in is out, and the turn goes past them */
        cbl_newgame(&G, 4);
        CHECK(!ok("a0b20") && ok("a0a20") && cbl_script(&G, "a0a20 a0a1 a0t1 a0t20"));
        CHECK(cbl_pos(&G)->turn == 0);
        cblgame g = G;
        cblpos *q = &g.pos[g.ply];
        q->cell[sq(1, 18)] = 3;                         /* ivory's only corner taken: ivory is boxed in */
        CHECK(!cbl_can_move(q, 0) && cbl_can_move(q, 1));
        q->turn = 3;
        int mv[4000], k = cbl_moves(q, mv, 4000);
        CHECK(k > 0);
        cbl_apply(q, mv[0]);
        CHECK(q->turn == 1 && q->out[0] && !q->over);
    }

    /* ---- random games ----------------------------------------------------------------------------------------- */
    unsigned seed = 2026;
    int ends = 0, bonus = 0;
    for (int game = 0; game < 90; game++) {
        int seats = 2 + game % 3, good = 1;
        cbl_newgame(&G, seats);
        static int mv[CBL_MOVES];
        while (!cbl_pos(&G)->over && good) {
            const cblpos *q = cbl_pos(&G);
            int k = cbl_moves(q, mv, CBL_MOVES);
            good = k > 0 && cbl_can_move(q, q->turn);
            if (!good) break;
            seed = seed * 1103515245u + 12345u;
            int pick = (seed >> 8) % (unsigned)k;
            if (game % 2) {                         /* half the games: the biggest blocks first */
                for (int i = 0; i < k; i++)
                    if (cbl_size(cbl_block(mv[i])) > cbl_size(cbl_block(mv[pick]))) pick = i;
            }
            char nm[8];
            cbl_name(q, mv[pick], nm);
            good = cbl_script(&G, nm);
        }
        const cblpos *q = cbl_pos(&G);
        int laid[CBL_SEATS] = {0};
        for (int i = 0; i < CBL_SQ; i++)
            if (q->cell[i]) laid[q->cell[i] - 1]++;
        for (int s = 0; s < seats; s++) {
            good &= laid[s] == q->laid[s] && !cbl_can_move(q, s);
            bonus += !q->left[s];
        }
        good &= cbl_winners(q) != 0;
        CHECK(good);
        ends += good;
    }
    printf("random games: %d ended, %d players laid all their blocks\n", ends, bonus);
    printf("cornerblocks: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
