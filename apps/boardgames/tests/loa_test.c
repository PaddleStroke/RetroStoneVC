/*
 * Lines of Action's rules: the distance counted along the whole line (both colours), jumping one's own pieces but not
 * the enemy's, captures, the start's moves, the wins by a joined group (a single piece, both at once), undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/loa.h"

static int fails;

static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static int sq(const char *s) { return loa_square(s); }

/* a position: ebony's pieces, ivory's ("a1 c3"), the player to move */
static void setup(lpos *p, const char *ebony, const char *ivory, int side)
{
    memset(p, 0, sizeof *p);
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = pl ? ivory : ebony; *s; s++)
            if (*s >= 'a' && *s <= 'h') p->at[sq(s)] = (uint8_t)(pl + 1);
    p->side = (uint8_t)side;
}

/* the move "b1-b3" among the side's legal moves: its index, -1 */
static int has(const lpos *p, const char *m, lmove *out)
{
    lmove legal[LOA_MAX_MOVES];
    int n = loa_legal(p, p->side, legal), used, i = loa_find(legal, n, m, &used);
    if (i >= 0 && out) *out = legal[i];
    return i;
}

/* plays moves in a game; 0: one is not legal */
static int plays(lgame *g, const char *moves)
{
    for (const char *s = moves; *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        lmove legal[LOA_MAX_MOVES];
        int n = loa_legal(loa_pos(g), loa_pos(g)->side, legal), used, i = loa_find(legal, n, s, &used);
        if (i < 0 || !loa_play(g, legal[i])) return 0;
        s += used;
    }
    return 1;
}

int main(void)
{
    static lgame g;
    lmove legal[LOA_MAX_MOVES], m;
    loa_new(&g);
    const lpos *p = loa_pos(&g);
    int n0 = 0, n1 = 0;
    for (int s = 0; s < 64; s++) n0 += p->at[s] == 1, n1 += p->at[s] == 2;
    check("start: 12 pieces each, ebony to move", n0 == 12 && n1 == 12 && p->side == 0 && p->result == LOA_PLAY);
    check("start: 36 moves for ebony, 36 for ivory", loa_legal(p, 0, legal) == 36 && loa_legal(p, 1, legal) == 36);
    check("d1's lines count both colours: column 2, row 6, each diagonal 2 (h5, a4)",
          loa_line(p, sq("d1"), 0, 1) == 2 && loa_line(p, sq("d1"), 1, 0) == 6 && loa_line(p, sq("d1"), 1, 1) == 2
          && loa_line(p, sq("d1"), -1, 1) == 2);
    check("start: b1-b3, b1-d3, b1-h1 (over its own row); not b1-b2, b1-c2",
          has(p, "b1-b3", 0) >= 0 && has(p, "b1-d3", 0) >= 0 && has(p, "b1-h1", 0) >= 0 && has(p, "b1-b2", 0) < 0
          && has(p, "b1-c2", 0) < 0);

    lpos t;
    setup(&t, "d1 d6 f1 f2", "d2 h8", 0);
    check("over its own piece: f1-f3", has(&t, "f1-f3", 0) >= 0);
    check("not over an enemy piece: no d1-d4", loa_line(&t, sq("d1"), 0, 1) == 3 && has(&t, "d1-d4", 0) < 0);
    t.at[sq("f4")] = 1;
    check("not onto its own piece: with f4 ebony's, f2-f5 but no f1-f4",
          loa_line(&t, sq("f2"), 0, 1) == 3 && has(&t, "f2-f5", 0) >= 0 && has(&t, "f1-f4", 0) < 0);

    setup(&t, "c3 a8", "c5 h1", 0);
    check("a capture: c3xc5", has(&t, "c3xc5", &m) >= 0 && m.capture);
    loa_make(&t, m);
    check("the piece is taken; ivory's single piece left is a group: ivory wins",
          t.at[sq("c5")] == 1 && t.result == LOA_WIN1);

    setup(&t, "a1 c1", "e8 h5", 0);
    check("a1-b2 joins ebony's pieces: ebony wins",
          has(&t, "a1-b2", &m) >= 0 && (loa_make(&t, m), t.result == LOA_WIN0));

    setup(&t, "a1 b5", "b2 b7 c8", 0);
    check("before: neither joined", !loa_joined(&t, 0) && !loa_joined(&t, 1));
    check("the group of a piece (8-neighbours)", loa_group(&t, sq("b7")) == ((1ull << sq("b7")) | (1ull << sq("c8")))
          && loa_group(&t, sq("a1")) == 1ull << sq("a1") && !loa_group(&t, sq("d4")));
    check("b5xb2 joins both at once: the mover (ebony) wins", has(&t, "b5xb2", &m) >= 0
          && (loa_make(&t, m), loa_joined(&t, 0) && loa_joined(&t, 1) && t.result == LOA_WIN0));

    setup(&t, "b1 b2 a2 g1 g2 h2 d5", "a1 h1", 0);   /* ivory's two pieces shut in their corners */
    check("ivory with no move passes: the turn comes back to ebony", has(&t, "d5-d6", &m) >= 0
          && (loa_make(&t, m), t.result == LOA_PLAY && t.side == 0 && t.passed && !loa_legal(&t, 1, legal)));

    loa_new(&g);
    check("b1-b3, then ivory's a3 is blocked by b3 (no a3-d3), a2-c2 is not",
          plays(&g, "b1-b3") && loa_line(loa_pos(&g), sq("a3"), 1, 0) == 3 && !plays(&g, "a3-d3")
          && plays(&g, "a2-c2"));
    check("moves in turn, then undo all the way", plays(&g, "d1-d3") && g.ply == 3 && loa_pos(&g)->side == 1
          && loa_undo(&g) && loa_undo(&g) && loa_undo(&g) && !loa_undo(&g)
          && !memcmp(loa_pos(&g), &g.pos[0], sizeof(lpos)) && loa_legal(loa_pos(&g), 0, legal) == 36);
    check("an illegal move is refused", !plays(&g, "b1-b4") && g.ply == 0);
    loa_pos(&g)->result = LOA_WIN0;
    check("after a win, no more moves", !plays(&g, "b1-b3"));

    printf(fails ? "%d FAILED\n" : "all ok\n", fails);
    return fails != 0;
}
