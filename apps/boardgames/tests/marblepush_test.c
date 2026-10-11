/*
 * Marble Push: the rules' test (src/games/marblepush_rules.c, no SDL): the board, the starting layouts, the moves in
 * line and sideways, the pushes (2 against 1, 3 against 2, the blocked ones), pushing off the edge, the win at 6, the
 * draws, undo, the notation, random games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/marblepush.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mpgame G;
static mpmove L[MP_MAX_MOVES];

static int hole(const char *s)
{
    int len;
    return mp_hole(s, &len);
}

static const mppos *pos(void) { return mp_pos(&G); }
static int at(const char *s) { return pos()->cell[hole(s)]; }   /* 0 empty, else the player + 1 */
static int on_board(int player)
{
    int n = 0;
    for (int h = 0; h < MP_HOLES; h++) n += pos()->cell[h] == player + 1;
    return n;
}

/* a position: player 0's marbles ("e3 e4"), player 1's, who moves */
static void setup(const char *mine, const char *theirs, int side)
{
    mp_new(&G, MP_STANDARD);
    mppos *p = &G.pos[0];
    memset(p->cell, 0, sizeof p->cell);
    const char *list[2] = {mine, theirs};
    for (int c = 0; c < 2; c++)
        for (const char *t = list[c]; *t;) {
            int len, h = mp_hole(t, &len);
            if (h < 0) { t++; continue; }
            p->cell[h] = (uint8_t)(c + 1);
            t += len;
        }
    p->side = (uint8_t)side;
}

static int play(const char *moves) { return mp_script(&G, moves); }

/* the board: 61 holes in 9 rows, the names, the neighbours */
static void test_board(void)
{
    static const int len[MP_ROWS] = {5, 6, 7, 8, 9, 8, 7, 6, 5};
    int count[MP_ROWS] = {0}, links = 0, ok = 1;
    for (int h = 0; h < MP_HOLES; h++) {
        count[mp_row(h)]++;
        ok &= mp_at(mp_row(h), mp_diag(h)) == h;
        char name[3];
        mp_name(h, name);
        ok &= hole(name) == h;
        for (int d = 0; d < 6; d++) {
            int x = mp_next(h, d);
            if (x < 0) continue;
            links++;
            ok &= mp_next(x, (d + 3) % 6) == h;   /* the way back */
        }
    }
    CHECK(ok);
    CHECK(!memcmp(count, len, sizeof len));
    CHECK(links == 2 * 156);                    /* 3 * 61 * 2 / 2 - the edges' missing links: 156 pairs */
    CHECK(hole("a1") == 0 && hole("a5") == 4 && hole("e5") == 30 && hole("i9") == 60);
    CHECK(hole("a6") < 0 && hole("i4") < 0 && hole("e0") < 0 && hole("j5") < 0);
    CHECK(mp_next(hole("e5"), MP_NE) == hole("f6") && mp_next(hole("e5"), MP_NW) == hole("f5"));
    CHECK(mp_next(hole("e5"), MP_SE) == hole("d5") && mp_next(hole("e5"), MP_SW) == hole("d4"));
    CHECK(mp_next(hole("e9"), MP_E) < 0 && mp_next(hole("a1"), MP_SW) < 0 && mp_next(hole("i5"), MP_NW) < 0);
    int8_t c[3];
    CHECK(mp_line(hole("c3"), hole("e5"), c) == 3 && c[1] == hole("d4"));
    CHECK(mp_line(hole("e3"), hole("e4"), c) == 2 && mp_line(hole("e5"), hole("e5"), c) == 1);
    CHECK(!mp_line(hole("e3"), hole("e6"), c) && !mp_line(hole("e3"), hole("f5"), c));
}

/* the layouts: 14 marbles each, the second player's the first's image (turned half round, or mirrored) */
static void test_layouts(void)
{
    static const char *const daisy[MP_VARIANTS][4] = {
        {0}, {"b2", "h8", "b5", "h5"}, {"c2", "g8", "c6", "g4"},
    };
    for (int v = 0; v < MP_VARIANTS; v++) {
        mp_new(&G, v);
        CHECK(on_board(0) == MP_MARBLES && on_board(1) == MP_MARBLES);
        CHECK(pos()->side == 0 && !pos()->lost[0] && !pos()->lost[1] && mp_status(&G) == MP_PLAY);
        int sym = 1;
        for (int h = 0; h < MP_HOLES; h++) {
            int r = mp_row(h), d = mp_diag(h), turned = mp_at(8 - r, 8 - d), mirror = mp_at(r, r + 4 - d);
            int c = pos()->cell[h], o = c ? 3 - c : 0;
            if (v == MP_STANDARD) sym &= pos()->cell[turned] == o;
            else sym &= pos()->cell[mirror] == o && pos()->cell[turned] == c;
        }
        CHECK(sym);
        if (v == MP_STANDARD) {
            CHECK(at("a1") == 1 && at("b6") == 1 && at("c3") == 1 && at("c5") == 1 && !at("c2") && !at("c6"));
            CHECK(at("i9") == 2 && at("h4") == 2 && at("g5") == 2 && at("g7") == 2 && !at("g4") && !at("g8"));
            CHECK(mp_legal(pos(), L) == 44);    /* the well-known count of opening moves */
            continue;
        }
        for (int k = 0; k < 4; k++) {           /* each daisy: a hole and its six neighbours, one player's */
            int h = hole(daisy[v][k]), who = k < 2 ? 1 : 2, all = pos()->cell[h] == who;
            for (int d = 0; d < 6; d++) all &= pos()->cell[mp_next(h, d)] == who;
            CHECK(all);
        }
    }
}

/* a move's legality on a position: the moves of these marbles that way */
static int legal(const char *cells, const char *dir)
{
    char s[32];
    snprintf(s, sizeof s, "%s:%s", cells, dir);
    mpgame keep = G;
    int ok = play(s);
    G = keep;
    return ok;
}

static void test_moves(void)
{
    setup("e5", "a1", 0);                       /* a single marble: six ways */
    CHECK(mp_legal(pos(), L) == 6);
    CHECK(play("e5:ne") && at("f6") == 1 && !at("e5") && pos()->side == 1);
    setup("e3 e4 e5", "a1", 0);                 /* a row of three: in line both ways, sideways four ways */
    CHECK(legal("e3e5", "e") && legal("e3e5", "w") && legal("e3e5", "ne") && legal("e3e5", "nw"));
    CHECK(legal("e3e5", "se") && legal("e3e5", "sw") && legal("e5e3", "e") && legal("e3e4e5", "w"));
    CHECK(play("e3e5:ne") && at("f4") == 1 && at("f5") == 1 && at("f6") == 1 && !at("e3") && !at("e5"));
    setup("e3 e5 e6", "a1", 0);                 /* not a row: a gap, too long, not in line */
    CHECK(!legal("e3e5", "e") && !legal("e3e6", "w") && !legal("e5f6", "e"));
    setup("e3 e4", "a1", 0);                    /* not one's own, or not the player's turn */
    CHECK(!legal("a1", "e") && !legal("e2", "e"));
    setup("e3 e4 e5", "f4", 0);                 /* sideways: every hole ahead empty, no pushing */
    CHECK(!legal("e3e5", "ne") && !legal("e3e4", "ne") && legal("e3e5", "se") && legal("e4e5", "ne"));
    setup("a1 a2 a3", "i9", 0);                 /* not off the board: sideways, or in line */
    CHECK(!legal("a1a3", "sw") && !legal("a1a3", "se") && legal("a1a3", "nw"));
    setup("e8 e9", "i9", 0);
    CHECK(!legal("e8e9", "e") && !legal("e9", "e") && legal("e8e9", "w"));
    setup("c3 d4 e5", "a1", 0);                 /* a row along a slanting line */
    CHECK(play("c3e5:ne") && at("f6") == 1 && at("d4") == 1 && !at("c3"));
}

static void test_pushes(void)
{
    setup("e3 e4", "e5", 0);                    /* 2 push 1 */
    CHECK(play("e3e4:e") && at("e4") == 1 && at("e5") == 1 && at("e6") == 2 && !at("e3"));
    CHECK(G.move[0].push == 1 && !G.move[0].off && pos()->quiet == 1);
    setup("e2 e3 e4", "e5 e6", 0);              /* 3 push 2 */
    CHECK(play("e2e4:e") && at("e5") == 1 && at("e6") == 2 && at("e7") == 2 && !at("e2"));
    CHECK(G.move[0].push == 2);
    setup("e2 e3 e4", "e5", 0);                 /* 3 push 1 */
    CHECK(play("e4e2:e") && at("e6") == 2 && on_board(1) == 1);
    setup("e2 e3 e4", "e5 e6 e7", 0);           /* 3 against 3, 2 against 2, 1 against 1: no */
    CHECK(!legal("e2e4", "e"));
    setup("e3 e4", "e5 e6", 0);
    CHECK(!legal("e3e4", "e"));
    setup("e4", "e5", 0);
    CHECK(!legal("e4", "e"));
    setup("e3 e4 e6", "e5", 0);                 /* one's own marble right behind the pushed one */
    CHECK(!legal("e3e4", "e"));
    setup("e2 e3 e4 e7", "e5 e6", 0);           /* ... or behind two */
    CHECK(!legal("e2e4", "e"));
    setup("e2 e3 e4", "e5 e6 e7", 0);
    CHECK(!legal("e2e4", "e"));
    setup("e3 e4", "e5 e7", 0);                 /* a gap after the pushed marble: it moves into it */
    CHECK(play("e3e4:e") && at("e6") == 2 && at("e7") == 2);
    setup("e3 e4", "e6", 0);                    /* a gap first: a plain move, nothing pushed */
    CHECK(play("e3e4:e") && at("e5") == 1 && at("e6") == 2 && G.move[0].push == 0);
    setup("e3 e4", "e5", 0);                    /* pushing backwards is not a push: the row's back moves first */
    CHECK(legal("e3e4", "w") && G.ply == 0);
    setup("d4 e5", "f6", 0);                    /* a push along a slanting line */
    CHECK(play("d4e5:ne") && at("f6") == 1 && at("g7") == 2);
    setup("e4 e5", "f5", 0);                    /* no sideways push */
    CHECK(!legal("e4e5", "nw") && !legal("e4e5", "ne"));
}

/* pushing off the edge: the marble leaves the board, counted for the pusher */
static void test_edge(void)
{
    setup("e7 e8", "e9", 0);
    CHECK(play("e7e8:e") && !at("e7") && at("e9") == 1 && on_board(1) == 0);
    CHECK(pos()->lost[1] == 1 && G.move[0].off && pos()->quiet == 0);
    setup("e5 e6 e7", "e8 e9", 0);              /* 3 push 2: the outer one off */
    CHECK(play("e5e7:e") && at("e9") == 2 && at("e8") == 1 && pos()->lost[1] == 1);
    setup("g7 h8", "i9 a1", 0);                 /* off a corner, along a slanting line */
    CHECK(play("g7h8:ne") && pos()->lost[1] == 1 && at("i9") == 1);
    setup("c3 b2", "a1 i9", 0);                 /* the other way, towards one's own side */
    CHECK(play("c3b2:sw") && pos()->lost[1] == 1 && at("a1") == 1);
    setup("a1 a2", "e5", 1);                    /* ebony's marbles pushed off by ivory */
    G.pos[0].cell[hole("e7")] = 2;
    G.pos[0].cell[hole("e8")] = 2;
    G.pos[0].cell[hole("e9")] = 1;
    CHECK(play("e7e8:e") && pos()->lost[0] == 1 && pos()->side == 0);
}

/* the win: the sixth marble pushed off; then no more moves */
static void test_win(void)
{
    setup("e7 e8 a1", "e9 i9", 0);
    G.pos[0].lost[1] = 5;
    CHECK(mp_status(&G) == MP_PLAY);
    CHECK(play("e7e8:e") && mp_status(&G) == MP_WIN0);
    CHECK(!play("a1:e"));
    setup("a1 a2", "e2 e3 i9", 1);
    G.pos[0].cell[hole("e1")] = 1;
    G.pos[0].lost[0] = 5;
    CHECK(play("e3e2:w") && pos()->lost[0] == 6 && mp_status(&G) == MP_WIN1);
    CHECK(mp_undo(&G) && mp_status(&G) == MP_PLAY);
}

/* the draws: a third repetition, 50 moves each without a push-off, no move at all */
static void test_draws(void)
{
    mp_new(&G, MP_STANDARD);
    const char *shuffle = "c3:nw g7:se d3:se f7:nw";
    CHECK(play(shuffle) && mp_status(&G) == MP_PLAY);   /* the start again: twice */
    CHECK(play("c3:nw g7:se d3:se") && mp_status(&G) == MP_PLAY);
    CHECK(play("f7:nw") && mp_status(&G) == MP_DRAW_REPEAT && !play("c3:nw"));
    CHECK(mp_undo(&G) && mp_status(&G) == MP_PLAY);
    mp_new(&G, MP_STANDARD);                    /* the same holes, the other player to move: not a repetition */
    CHECK(play("c3:nw g7:se d3:se f7:nw c3:nw g7:se d3:se") && mp_status(&G) == MP_PLAY);
    mp_new(&G, MP_STANDARD);                    /* 100 moves without a push-off */
    G.pos[0].quiet = MP_QUIET - 3;
    CHECK(play("c3:nw g7:se") && mp_status(&G) == MP_PLAY && pos()->quiet == MP_QUIET - 1);
    CHECK(play("c4:nw") && mp_status(&G) == MP_DRAW_QUIET);
    setup("e7 e8", "e9 a1", 0);                 /* a push-off starts the count again */
    G.pos[0].quiet = MP_QUIET - 1;
    CHECK(play("e7e8:e") && pos()->quiet == 0 && mp_status(&G) == MP_PLAY);
    setup("a1", "a2 b1 b2", 0);                 /* walled in: no move */
    CHECK(mp_legal(pos(), L) == 0 && mp_status(&G) == MP_DRAW_STUCK);
}

static void test_undo_notation(void)
{
    mp_new(&G, MP_BELGIAN);
    mppos start = *pos();
    CHECK(!mp_undo(&G));
    CHECK(play("c3:ne g4g5:se"));
    CHECK(G.ply == 2 && mp_undo(&G) && mp_undo(&G) && !memcmp(pos(), &start, sizeof start));
    CHECK(!play("c3") && !play("c3:") && !play("c3:x") && !play("c3:nee") && !play("z3:e") && !play("c3c3:e"));
    mp_new(&G, MP_GERMAN);
    CHECK(play("c3:e") && G.ply == 1 && !play("d2d3d4:e") && G.ply == 1);
}

/* random games: the marbles are all accounted for, every game ends */
static void test_random(void)
{
    unsigned s = 12345;
    int ends[6] = {0};
    for (int game = 0; game < 60; game++) {
        mp_new(&G, game % MP_VARIANTS);
        int ok = 1;
        while (mp_status(&G) == MP_PLAY && G.ply < MP_MAX_PLY) {
            int n = mp_legal(pos(), L), pick = -1;
            for (int i = 0; i < n && pick < 0; i++)   /* push off when possible: the games end */
                if (L[i].off) pick = i;
            s = s * 1103515245u + 12345u;
            if (pick < 0 || (s >> 16) % 3 == 0) pick = (int)((s >> 16) % (unsigned)n);
            ok &= mp_play(&G, &L[pick]);
            for (int p = 0; p < 2; p++) ok &= on_board(p) + pos()->lost[p] == MP_MARBLES;
        }
        CHECK(ok);
        ends[mp_status(&G)]++;
    }
    CHECK(!ends[MP_PLAY] && ends[MP_WIN0] + ends[MP_WIN1] > 0);
}

int main(void)
{
    test_board();
    test_layouts();
    test_moves();
    test_pushes();
    test_edge();
    test_win();
    test_draws();
    test_undo_notation();
    test_random();
    printf("marblepush: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
