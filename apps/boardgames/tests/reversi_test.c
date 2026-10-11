/*
 * Reversi's rules: the start, the turned lines (several at once, none across a gap or off the edge), the automatic
 * pass, the end, the count and the draw, undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>

#include "games/reversi.h"

static int fails;

static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static int sq(const char *s) { return RV_SQ(s[0] - 'a', s[1] - '1'); }

/* "d3 c4 f5": the squares as a set */
static uint64_t set(const char *s)
{
    uint64_t m = 0;
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'h') m |= 1ull << sq(s);
    return m;
}

static int plays(rgame *g, const char *moves)
{
    for (const char *s = moves; *s; s++)
        if (*s >= 'a' && *s <= 'h' && !rv_play(g, sq(s))) return 0;
    return 1;
}

/* a position: ebony's discs, ivory's, the player to move */
static rgame *setup(const char *ebony, const char *ivory, int side)
{
    static rgame g;
    rv_new(&g);
    g.pos[0] = (rpos){{set(ebony), set(ivory)}, side, 0, 0};
    return &g;
}

int main(void)
{
    rgame g;
    rv_new(&g);
    const rpos *p = rv_pos(&g);
    check("start: 2 discs each, ebony to move", rv_count(p, 0) == 2 && rv_count(p, 1) == 2 && p->side == 0);
    check("start: ebony's 4 moves d3 c4 f5 e6", rv_legal(p, 0) == set("d3 c4 f5 e6"));
    check("start: ivory's 4 moves c5 d6 e3 f4", rv_legal(p, 1) == set("c5 d6 e3 f4"));
    check("d3 turns d4, ivory to move", rv_play(&g, sq("d3")) && rv_pos(&g)->disc[0] == set("d3 d4 d5 e4")
                                        && rv_pos(&g)->side == 1 && rv_count(rv_pos(&g), 1) == 1);
    check("ivory's answers c3 e3 c5", rv_legal(rv_pos(&g), 1) == set("c3 e3 c5"));
    check("a full square, a move that turns nothing: refused", !rv_play(&g, sq("d4")) && !rv_play(&g, sq("a1")));
    check("undo to the start, then nothing", rv_undo(&g) && g.ply == 0 && !rv_undo(&g)
                                             && rv_legal(rv_pos(&g), 0) == set("d3 c4 f5 e6"));

    rgame *t = setup("c5 h5 e3 e7 c3 g7", "d5 f5 g5 e4 e6 d4 f6", 0);
    check("e5 turns six lines at once (seven discs)", rv_flips(rv_pos(t), 0, sq("e5")) == set("d5 f5 g5 e4 e6 d4 f6"));
    rv_play(t, sq("e5"));
    check("... and ivory has no disc left", rv_count(rv_pos(t), 1) == 0 && rv_count(rv_pos(t), 0) == 14);

    t = setup("a3", "c3 e3 f3 g3 h3", 0);
    check("no turn across an empty square, nor to the edge", rv_flips(rv_pos(t), 0, sq("d3")) == 0);
    t = setup("a3 b3", "c3 e3 f3 g3 h3", 0);
    check("d3 turns c3 only", rv_flips(rv_pos(t), 0, sq("d3")) == set("c3"));

    t = setup("a1 a8", "b1 b8", 0);
    rv_play(t, sq("c1"));
    p = rv_pos(t);
    check("ivory cannot move: passes, ebony again", p->side == 0 && p->passed && !p->over && rv_status(t) == RV_PLAY);
    rv_play(t, sq("c8"));
    check("neither can move: the end, ebony wins 6-0", rv_pos(t)->over && rv_status(t) == RV_WIN0
                                                     && rv_count(rv_pos(t), 0) == 6 && !rv_play(t, sq("d1")));
    check("undo goes back over the pass", rv_undo(t) && rv_pos(t)->passed && rv_status(t) == RV_PLAY);

    t = setup("a1", "b1 a8 b8 c8", 0);
    rv_play(t, sq("c1"));
    check("3 against 3, no move left: a draw", rv_pos(t)->over && rv_status(t) == RV_DRAW);
    t = setup("a1", "b1 a8 b8 c8 h8", 0);
    rv_play(t, sq("c1"));
    check("3 against 4: ivory wins, empty squares left", rv_pos(t)->over && rv_status(t) == RV_WIN1);

    rv_new(&g);
    check("the shortest game: d3 c3 b3 d2 e1 d6 d7 e3 f4, 13-0",
          plays(&g, "d3 c3 b3 d2 e1 d6 d7 e3 f4") && rv_status(&g) == RV_WIN0 && rv_count(rv_pos(&g), 0) == 13);

    printf(fails ? "%d FAILED\n" : "all good\n", fails);
    return fails != 0;
}
