/*
 * Chinese checkers' rules test (src/games/halma_rules.c, no SDL): the 121 holes and their neighbours, the notation,
 * steps, single and chained jumps over both colours, no hole visited twice, the win, the anti-blocking rule, undo,
 * whole games played greedily (always won, all undone); three, four and six players: the star's points, the start, the
 * turn round the star, the wins, passing when walled in, greedy games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/halma.h"

static hl_game G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return hl_hole(name, &len);
}

static int has(const int *list, int k, int h)
{
    for (int i = 0; i < k; i++)
        if (list[i] == h) return 1;
    return 0;
}

/* an empty board, the turn given */
static void bare(int turn)
{
    hl_new(&G);
    memset(G.cell, 0, sizeof G.cell);
    G.ply = turn;
}

static void put(int x2, int row, int who) { G.cell[hl_at(x2, row)] = (uint8_t)who; }

static void test_board(void)
{
    int n = 0, links = 0, homes[2] = {0, 0}, nb[6], nb2[6];
    for (int r = 0; r < HL_ROWS; r++)
        for (int x = -12; x <= 12; x++) {
            int h = hl_at(x, r);
            if (h < 0) continue;
            n++;
            CHECK(hl_row(h) == r && hl_x2(h) == x);
        }
    CHECK(n == HL_HOLES);
    for (int h = 0; h < HL_HOLES; h++) {
        int k = hl_around(h, nb);
        CHECK(k >= 2 && k <= 6);
        links += k;
        for (int i = 0; i < k; i++) CHECK(has(nb2, hl_around(nb[i], nb2), h));   /* both ways */
        if (hl_home(h) >= 0) homes[hl_home(h)]++;
        char name[4];
        hl_name(h, name);
        CHECK(at(name) == h);
    }
    CHECK(links == 2 * 312);                    /* the star's 312 lines between neighbours */
    CHECK(homes[0] == HL_MARBLES && homes[1] == HL_MARBLES);
    CHECK(hl_around(at("a1"), nb) == 2 && hl_around(at("m5"), nb) == 2 && hl_around(at("e9"), nb) == 6);
    CHECK(at("a1") == 0 && at("a17") == HL_HOLES - 1 && at("n5") < 0 && at("e4") < 0 && at("a18") < 0);
    CHECK(hl_goal(0, at("a17")) && hl_goal(1, at("a1")) && !hl_goal(0, at("a1")) && !hl_goal(0, at("a5")));
}

static void test_start(void)
{
    int to[HL_HOLES];
    hl_new(&G);
    CHECK(hl_turn(&G) == 0 && hl_status(&G) == HL_PLAY);
    CHECK(G.cell[at("a1")] == HL_IVORY && G.cell[at("d4")] == HL_IVORY && G.cell[at("a17")] == HL_EBONY);
    int k = hl_targets(&G, at("d4"), to);       /* two steps */
    CHECK(k == 2 && has(to, k, at("h5")) && has(to, k, at("i5")));
    k = hl_targets(&G, at("c3"), to);           /* two jumps, over d4 and over c4 */
    CHECK(k == 2 && has(to, k, at("i5")) && has(to, k, at("g5")));
    CHECK(hl_targets(&G, at("a1"), to) == 0);   /* walled in */
    CHECK(hl_targets(&G, at("a17"), to) == 0);  /* not ebony's turn */
    CHECK(!hl_script(&G, "d4-d6"));             /* not a step, not a jump */
    CHECK(hl_script(&G, "c3-i5 a15-g13"));      /* a jump each */
    CHECK(G.cell[at("i5")] == HL_IVORY && G.cell[at("c3")] == HL_EMPTY && hl_turn(&G) == 0);
    CHECK(G.cell[at("g13")] == HL_EBONY);
}

/* a chain of three jumps from e9 (0, 8): over ebony to (4, 8), over ivory to (6, 10), over ebony to (2, 10); from
 * (4, 8) it could jump back over the first marble onto e9, where it started: not allowed */
static void test_chain(void)
{
    int to[HL_HOLES];
    hl_path p;
    bare(0);
    put(0, 8, HL_IVORY);
    put(2, 8, HL_EBONY);
    put(5, 9, HL_IVORY);
    put(4, 10, HL_EBONY);
    int from = hl_at(0, 8), end = hl_at(2, 10), k = hl_targets(&G, from, to);
    CHECK(has(to, k, hl_at(4, 8)) && has(to, k, hl_at(6, 10)) && has(to, k, end));
    CHECK(!has(to, k, from));
    CHECK(has(to, k, hl_at(-1, 9)) && has(to, k, hl_at(-2, 8)) && !has(to, k, hl_at(2, 8)));   /* steps */
    CHECK(hl_route(&G, from, end, &p) && p.n == 4 && p.hole[1] == hl_at(4, 8) && p.hole[2] == hl_at(6, 10));
    CHECK(hl_valid(&G, &p));
    hl_path back = {3, {(uint8_t)from, (uint8_t)hl_at(4, 8), (uint8_t)from}};   /* there and back again */
    CHECK(!hl_valid(&G, &back));
    hl_path two = {3, {(uint8_t)from, (uint8_t)hl_at(-1, 9), (uint8_t)hl_at(-2, 10)}};   /* a step then more */
    CHECK(!hl_valid(&G, &two));
    hl_path over_none = {2, {(uint8_t)from, (uint8_t)hl_at(-4, 8)}};   /* a jump over an empty hole */
    CHECK(!hl_valid(&G, &over_none));
    CHECK(hl_play(&G, &p) && G.cell[end] == HL_IVORY && !G.cell[from] && hl_turn(&G) == 1);
    CHECK(hl_last(&G)->n == 4);
    CHECK(hl_undo(&G) && G.cell[from] == HL_IVORY && !G.cell[end] && hl_turn(&G) == 0 && !hl_last(&G));
    CHECK(!hl_undo(&G));
}

/* ivory has 9 marbles in the top point; the 10th steps in: a win. The same with 3 of ebony's marbles never moved from
 * there: ivory wins when the 7 other holes are full of its own (the anti-blocking rule) */
static void test_win(void)
{
    int top[HL_MARBLES], n = 0;
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_home(h) == 1) top[n++] = h;
    bare(0);
    for (int i = 1; i < HL_MARBLES; i++) G.cell[top[i]] = HL_IVORY;
    int gap = top[0];                           /* a hole of the bottom row of the point (row 13) */
    put(hl_x2(gap) - 1, 12, HL_IVORY);
    put(0, 0, HL_EBONY);                        /* ebony somewhere */
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 0) == 9);
    hl_path p;
    CHECK(hl_route(&G, hl_at(hl_x2(gap) - 1, 12), gap, &p) && hl_play(&G, &p));
    CHECK(hl_status(&G) == HL_WIN0 && hl_arrived(&G, 0) == 10);
    CHECK(!hl_play(&G, &p));                    /* over */
    CHECK(hl_undo(&G) && hl_status(&G) == HL_PLAY);

    bare(0);
    for (int i = 0; i < 3; i++) G.cell[top[HL_MARBLES - 1 - i]] = HL_EBONY;   /* the tip: ebony never left */
    for (int i = 1; i < HL_MARBLES - 3; i++) G.cell[top[i]] = HL_IVORY;
    put(hl_x2(gap) - 1, 12, HL_IVORY);
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 0) == 6);
    CHECK(hl_route(&G, hl_at(hl_x2(gap) - 1, 12), gap, &p) && hl_play(&G, &p));
    CHECK(hl_status(&G) == HL_WIN0);

    bare(0);                                    /* ebony's point full of ebony alone is no win for ivory */
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_home(h) == 1) G.cell[h] = HL_EBONY;
    put(0, 8, HL_IVORY);
    CHECK(hl_status(&G) == HL_PLAY);
}

/* a game, then undo all the way back to the start */
static void test_undo(void)
{
    static hl_game start;
    hl_new(&start);
    hl_new(&G);
    CHECK(hl_script(&G, "d4-i5 a14-e13 c3-d4 a15-a14 b3-h5 b15-a15 i5-i6"));
    CHECK(G.ply == 7 && hl_turn(&G) == 1);
    while (hl_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(G.cell, start.cell, sizeof G.cell));
}

/* hex distance between two holes */
static int dist(int a, int b)
{
    int dr = abs(hl_row(a) - hl_row(b)), dx = abs(hl_x2(a) - hl_x2(b));
    return dr + (dx > dr ? (dx - dr) / 2 : 0);
}

/* whole games, each player taking the move that goes furthest towards the far tip (a little chance between equals):
 * every move found is valid, every game ends with a winner, and undo takes it all back */
static void test_games(void)
{
    static hl_game start;
    unsigned rng = 12345;
    hl_new(&start);
    for (int game = 0; game < 20; game++) {
        hl_new(&G);
        int plies = 0, bad = 0;
        while (hl_status(&G) == HL_PLAY && plies < 600) {
            int p = hl_turn(&G), tip = p ? 0 : HL_HOLES - 1, bf = -1, bt = -1, best = -(1 << 30);
            for (int h = 0; h < HL_HOLES; h++) {
                if (G.cell[h] != p + 1) continue;
                int to[HL_HOLES], k = hl_targets(&G, h, to);
                for (int i = 0; i < k; i++) {
                    rng = rng * 1103515245u + 12345u;
                    int gain = 8 * (dist(h, tip) - dist(to[i], tip) - 10 * (hl_goal(p, h) && !hl_goal(p, to[i])));
                    gain += (int)(rng >> 16) % 3;
                    if (gain > best) best = gain, bf = h, bt = to[i];
                }
            }
            hl_path path;
            if (bf < 0 || !hl_route(&G, bf, bt, &path) || !hl_valid(&G, &path) || !hl_play(&G, &path)) {
                bad = 1;
                break;
            }
            plies++;
        }
        int st = hl_status(&G);
        CHECK(!bad && (st == HL_WIN0 || st == HL_WIN1));
        CHECK(hl_arrived(&G, st) > 0);
        while (hl_undo(&G)) {}
        CHECK(G.ply == 0 && !memcmp(G.cell, start.cell, sizeof G.cell));
    }
}

/* ---- three, four and six players ------------------------------------------------------------------------------- */
static const int COUNTS[3] = {3, 4, 6};

/* the six points of 10 holes, the middle hexagon of 61; two players: the same as the two-player functions */
static void test_points(void)
{
    int n[HL_POINTS + 1] = {0}, same = 1;
    for (int h = 0; h < HL_HOLES; h++) n[hl_point(h) + 1]++;
    CHECK(n[0] == 61);
    for (int k = 0; k < HL_POINTS; k++) CHECK(n[k + 1] == HL_MARBLES);
    CHECK(hl_point(at("a1")) == 0 && hl_point(at("a5")) == 1 && hl_point(at("a13")) == 2 && hl_point(at("a17")) == 3);
    CHECK(hl_point(at("m13")) == 4 && hl_point(at("m5")) == 5 && hl_point(at("e9")) == -1);
    static hl_game two;
    hl_new(&two);
    hl_new_players(&G, 2);
    CHECK(G.players == 2 && !memcmp(G.cell, two.cell, sizeof G.cell));
    for (int h = 0; h < HL_HOLES; h++)
        for (int p = 0; p < 2; p++) same &= hl_dest(&G, p, h) == hl_goal(p, h);
    CHECK(same);
    hl_new_players(&G, 5);                      /* not a count the star takes: two */
    CHECK(G.players == 2);
}

/* the start: each player's ten marbles in their point (3: every other point; 4: two pairs; 6: all), the destination
 * the point opposite; the turn round the star, 0, 1, 2... */
static void test_players_start(void)
{
    static const int SEATS[3][6] = {{0, 2, 4}, {0, 1, 3, 4}, {0, 1, 2, 3, 4, 5}};
    for (int c = 0; c < 3; c++) {
        int np = COUNTS[c], ok = 1;
        hl_new_players(&G, np);
        CHECK(G.players == np && hl_turn(&G) == 0 && hl_status(&G) == HL_PLAY);
        for (int p = 0; p < np; p++) {
            CHECK(hl_seat(np, p) == SEATS[c][p]);
            int mine = 0, dest = 0;
            for (int h = 0; h < HL_HOLES; h++) {
                mine += G.cell[h] == p + 1;
                ok &= (G.cell[h] == p + 1) == (hl_point(h) == SEATS[c][p]);
                dest += hl_dest(&G, p, h);
                ok &= !hl_dest(&G, p, h) || hl_point(h) == (SEATS[c][p] + 3) % 6;
            }
            CHECK(mine == HL_MARBLES && dest == HL_MARBLES && hl_arrived(&G, p) == 0);
        }
        int used = 0;
        for (int h = 0; h < HL_HOLES; h++) used += G.cell[h] != HL_EMPTY;
        CHECK(ok && used == np * HL_MARBLES);
        /* a round: each player in turn steps their marble nearest the middle towards it */
        for (int i = 0; i < 2 * np; i++) {
            int p = hl_turn(&G), best = -1, bd = 99, to[HL_HOLES];
            CHECK(p == i % np);
            for (int h = 0; h < HL_HOLES; h++) {
                if (G.cell[h] != p + 1) continue;
                int d = abs(hl_row(h) - 8) + abs(hl_x2(h)), k = hl_targets(&G, h, to);
                if (k && d < bd) bd = d, best = h;
            }
            int other = (p + 1) % np, theirs = -1;  /* another's marble may not be moved */
            for (int h = 0; h < HL_HOLES && theirs < 0; h++)
                if (G.cell[h] == other + 1) theirs = h;
            CHECK(hl_targets(&G, theirs, to) == 0);
            hl_path path;
            CHECK(best >= 0 && hl_targets(&G, best, to) && hl_route(&G, best, to[0], &path) && hl_play(&G, &path));
        }
        CHECK(hl_turn(&G) == 0 && G.ply == 2 * np);
        while (hl_undo(&G)) {}
        CHECK(hl_turn(&G) == 0 && G.ply == 0);
    }
}

/* the holes of a point, in the order of the board */
static int point_holes(int k, int *out)
{
    int n = 0;
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_point(h) == k) out[n++] = h;
    return n;
}

/* an empty board for np players, the turn given */
static void bare_n(int np, int turn)
{
    hl_new_players(&G, np);
    memset(G.cell, 0, sizeof G.cell);
    G.turn = turn;
}

/* a free hole next to h, outside every point; -1: none */
static int beside(int h)
{
    int nb[6], k = hl_around(h, nb);
    for (int i = 0; i < k; i++)
        if (hl_point(nb[i]) < 0 && !G.cell[nb[i]]) return nb[i];
    return -1;
}

/* the first to fill their point wins, whoever they are; the anti-blocking rule with the marbles of the point's owner
 * (six players: player 4 goes to player 1's point) */
static void test_players_win(void)
{
    int pt[HL_MARBLES];
    hl_path p;
    /* three players: player 2 (upper right) fills the lower left point */
    bare_n(3, 2);
    point_holes(1, pt);
    for (int i = 1; i < HL_MARBLES; i++) G.cell[pt[i]] = 3;
    int gap = -1, from = -1;
    for (int i = 0; i < HL_MARBLES && from < 0; i++) {   /* an empty hole of it with a free hole beside it */
        gap = pt[i];
        G.cell[gap] = 0;
        from = beside(gap);
        if (from < 0) G.cell[gap] = 3;
    }
    G.cell[from] = 3;
    G.cell[at("a1")] = 1;
    G.cell[at("a17")] = 2;
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 2) == 9 && hl_turn(&G) == 2);
    CHECK(hl_route(&G, from, gap, &p) && hl_play(&G, &p));
    CHECK(hl_status(&G) == HL_WIN2 && hl_winner(hl_status(&G)) == 2 && hl_arrived(&G, 2) == 10);
    CHECK(!hl_play(&G, &p));                    /* over */
    CHECK(hl_undo(&G) && hl_status(&G) == HL_PLAY && hl_turn(&G) == 2);

    /* six players: player 4's point to fill is player 1's home; three of player 1's marbles never left it */
    bare_n(6, 4);
    point_holes(1, pt);
    for (int i = 0; i < HL_MARBLES; i++) G.cell[pt[i]] = i < 3 ? 2 : 5;
    from = -1;
    for (int i = 3; i < HL_MARBLES && from < 0; i++) {
        gap = pt[i];
        G.cell[gap] = 0;
        from = beside(gap);
        if (from < 0) G.cell[gap] = 5;
    }
    G.cell[from] = 5;
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 4) == 6);
    CHECK(hl_route(&G, from, gap, &p) && hl_play(&G, &p) && hl_status(&G) == HL_WIN4);
    CHECK(hl_winner(HL_WIN5) == 5 && hl_win(5) == HL_WIN5 && hl_winner(HL_DRAW) == -1 && hl_winner(HL_PLAY) == -1);

    /* player 1's point full of player 1's own marbles is no win for player 4 */
    bare_n(6, 4);
    for (int i = 0; i < HL_MARBLES; i++) G.cell[pt[i]] = 2;
    G.cell[at("e9")] = 5;
    CHECK(hl_status(&G) == HL_PLAY);

    /* four players: player 3 (upper right) fills the lower left */
    bare_n(4, 3);
    for (int i = 0; i < HL_MARBLES; i++) G.cell[pt[i]] = 4;
    CHECK(hl_status(&G) == HL_WIN3 && hl_arrived(&G, 3) == 10);
}

/* a player walled in: two players, the game is drawn; three or more, they pass. Nobody able to move: drawn */
static void test_players_pass(void)
{
    for (int np = 2; np <= 3; np++) {
        bare_n(np, 0);
        put(0, 16, HL_EBONY);                   /* player 1 on the top tip, walled in by player 0 */
        put(-1, 15, HL_IVORY);
        put(1, 15, HL_IVORY);
        put(-2, 14, HL_IVORY);
        put(2, 14, HL_IVORY);
        put(0, 8, HL_IVORY);
        if (np == 3) put(0, 0, 3);              /* player 2 on the bottom tip, free */
        hl_path p = {2, {(uint8_t)hl_at(0, 8), (uint8_t)hl_at(2, 8)}};
        CHECK(hl_status(&G) == HL_PLAY && hl_play(&G, &p));
        if (np == 2) {
            CHECK(hl_turn(&G) == 1 && hl_status(&G) == HL_DRAW);
        } else {
            CHECK(hl_turn(&G) == 2 && hl_status(&G) == HL_PLAY);   /* player 1 passes */
            hl_path q = {2, {(uint8_t)hl_at(0, 0), (uint8_t)hl_at(1, 1)}};
            CHECK(hl_play(&G, &q) && hl_turn(&G) == 0);
            CHECK(hl_undo(&G) && hl_turn(&G) == 2 && hl_undo(&G) && hl_turn(&G) == 0);
        }
    }
    /* the whole star full: nobody can move, nobody has won (each point to fill holds no marble of its player) */
    bare_n(3, 0);
    for (int h = 0; h < HL_HOLES; h++) {
        int k = hl_point(h);
        G.cell[h] = (uint8_t)(k == 3 ? 2 : k == 5 ? 3 : 1);   /* 0 goes to 3, 1 to 5, 2 to 1 */
    }
    CHECK(hl_status(&G) == HL_DRAW);
}

/* the farthest hole of the point the player must fill */
static int far_tip(int player)
{
    int best = -1, bd = -1, mid = hl_at(0, 8);
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_dest(&G, player, h) && dist(h, mid) > bd) bd = dist(h, mid), best = h;
    return best;
}

/* whole games with 3, 4 and 6 players, each taking the move that goes furthest towards their far tip: every move
 * valid, the turn round the star, each game won by the first to fill their point, all undone */
static void test_players_games(void)
{
    static hl_game start;
    unsigned rng = 777;
    for (int c = 0; c < 3; c++)
        for (int game = 0; game < 6; game++) {
            int np = COUNTS[c], plies = 0, bad = 0, order = 1;
            hl_new_players(&start, np);
            hl_new_players(&G, np);
            while (hl_status(&G) == HL_PLAY && plies < 3000) {
                int p = hl_turn(&G), tip = far_tip(p), bf = -1, bt = -1, best = -(1 << 30);
                for (int h = 0; h < HL_HOLES; h++) {
                    if (G.cell[h] != p + 1) continue;
                    int to[HL_HOLES], k = hl_targets(&G, h, to);
                    for (int i = 0; i < k; i++) {
                        rng = rng * 1103515245u + 12345u;
                        int out = hl_dest(&G, p, h) && !hl_dest(&G, p, to[i]);
                        int gain = 8 * (dist(h, tip) - dist(to[i], tip) - 10 * out);
                        gain += (int)(rng >> 16) % 3;
                        if (gain > best) best = gain, bf = h, bt = to[i];
                    }
                }
                hl_path path;
                if (bf < 0 || !hl_route(&G, bf, bt, &path) || !hl_valid(&G, &path) || !hl_play(&G, &path)) {
                    bad = 1;
                    break;
                }
                order &= hl_turn(&G) == (p + 1) % np || hl_status(&G) != HL_PLAY;
                plies++;
            }
            int st = hl_status(&G), w = hl_winner(st);
            CHECK(!bad && order && w >= 0 && w < np);
            CHECK(w >= 0 && hl_arrived(&G, w) > 0 && G.who[G.nhist - 1] == w);   /* the last to move won */
            while (hl_undo(&G)) {}
            CHECK(G.ply == 0 && hl_turn(&G) == 0 && !memcmp(G.cell, start.cell, sizeof G.cell));
        }
}

int main(void)
{
    test_board();
    test_start();
    test_chain();
    test_win();
    test_undo();
    test_games();
    test_points();
    test_players_start();
    test_players_win();
    test_players_pass();
    test_players_games();
    printf("halma: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
