/*
 * Backgammon's rules: the start and the opening roll, hitting and entering from the bar (a closed board: the throw
 * passes), using both dice or else the larger, doubles, bearing off (the exact number, a higher one), the gammon and
 * the backgammon, undo (within a turn, over the other player's throw); plakoto (pins, the mother), fevga (the start,
 * one checker holding a point, the six in a row) and long nardy (the head, the first doubles, the six in a row).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/backgammon.h"

static int fails;
static bgame G;

static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* "13:1 9:1 off:13": a player's checkers on their own points */
static void put(bpos *p, int player, const char *spec)
{
    memset(p->n[player], 0, sizeof p->n[player]);
    for (const char *s = spec; *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        int at = !strncmp(s, "bar", 3) ? BG_BAR : !strncmp(s, "off", 3) ? BG_OFF : atoi(s);
        s = strchr(s, ':') + 1;
        p->n[player][at] = (signed char)strtol(s, (char **)&s, 10);
    }
}

/* a position of a variant waiting for side's throw (not anybody's first) */
static bgame *setup_v(int variant, int side, const char *ivory, const char *ebony)
{
    bg_new(&G, variant);
    bpos *p = &G.pos[0];
    put(p, 0, ivory);
    put(p, 1, ebony);
    p->side = (signed char)side;
    p->phase = BG_THROW;
    p->throws[0] = p->throws[1] = 2;
    return &G;
}

static bgame *setup(int side, const char *ivory, const char *ebony)
{
    return setup_v(BG_BACKGAMMON, side, ivory, ebony);
}

static int has(const bpos *p, int from, int to)
{
    bmove m;
    return bg_find(p, from, to, &m);
}

/* plakoto: all on the 24, pinning instead of hitting, the mother, 1 or 2 points */
static void test_plakoto(void)
{
    bg_new(&G, BG_PLAKOTO);
    const bpos *p = bg_pos(&G);
    check("plakoto: fifteen each on their 24", p->n[0][24] == 15 && p->n[1][24] == 15 && bg_pips(p, 1) == 360);
    check("plakoto: the opening's winner throws again", bg_moves(&G, "31:") && bg_pos(&G)->phase == BG_THROW
                                                         && bg_pos(&G)->side == 0);
    check("plakoto: then plays its own throw", bg_moves(&G, "64: 24/18 24/20") && bg_pos(&G)->side == 1
                                               && bg_pos(&G)->n[0][24] == 13);

    setup_v(BG_PLAKOTO, 0, "24:14 13:1", "24:14 16:1");
    bg_throw(&G, 4, 2);
    bmove l[BG_MAXMOVES];
    int n = bg_legal(bg_pos(&G), l), pin = 0;
    for (int i = 0; i < n; i++) pin |= l[i].from == 13 && l[i].to == 9 && l[i].hit;
    check("plakoto: landing on a single checker pins it", pin && bg_moves(&G, "13/9*") && bg_pos(&G)->pin[1][16]
                                    && bg_pos(&G)->n[1][16] == 1 && bg_pos(&G)->n[0][9] == 1
                                    && bg_pos(&G)->n[1][BG_BAR] == 0);
    check("plakoto: undo sets it free", bg_undo(&G) && !bg_pos(&G)->pin[1][16] && bg_pos(&G)->n[0][13] == 1);

    setup_v(BG_PLAKOTO, 1, "24:13 13:1 9:1", "24:13 22:1 16:1");
    G.pos[0].pin[1][16] = 1;                    /* ebony's 16 pinned by ivory's 9 */
    bg_throw(&G, 6, 3);
    p = bg_pos(&G);
    check("plakoto: a pinned checker cannot move", !has(p, 16, 10) && !has(p, 16, 13) && has(p, 24, 18));
    check("plakoto: no pinning the pinner", !has(p, 22, 16) && has(p, 22, 19));
    bg_moves(&G, "24/18 22/19 41:");
    p = bg_pos(&G);
    bmove m;
    check("plakoto: the pinner's side may add to it", bg_find(p, 13, 9, &m) && !m.hit && bg_moves(&G, "13/9")
                                                      && bg_pos(&G)->n[0][9] == 2 && bg_pos(&G)->pin[1][16]);
    setup_v(BG_PLAKOTO, 0, "24:14 9:1", "24:14 16:1");
    G.pos[0].pin[1][16] = 1;
    check("plakoto: the pinner leaves, the checker is free", bg_moves(&G, "41: 9/5") && !bg_pos(&G)->pin[1][16]);

    setup_v(BG_PLAKOTO, 0, "3:1 6:14", "24:1 12:14");
    check("plakoto: the mother pinned: 2 points", bg_moves(&G, "21: 3/1*") && bg_status(&G) == BG_WIN0
                                                 && bg_points(bg_pos(&G)) == 2);
    setup_v(BG_PLAKOTO, 0, "24:1 3:1 6:13", "24:1 12:14");
    check("plakoto: not while the pinner has a checker on its own start", bg_moves(&G, "21: 3/1*")
                                    && bg_status(&G) == BG_PLAY && bg_pos(&G)->pin[1][24]);
    check("plakoto: then once its start is empty", bg_moves(&G, "24/23") && bg_status(&G) == BG_WIN0
                                                    && bg_points(bg_pos(&G)) == 2);
    setup_v(BG_PLAKOTO, 0, "24:1 3:1 6:13", "1:1 24:1 12:13");
    G.pos[0].pin[0][24] = 1;                    /* ivory's mother pinned already */
    check("plakoto: both mothers pinned: a draw", bg_moves(&G, "21: 3/1*") && bg_status(&G) == BG_DRAW
                                                 && bg_points(bg_pos(&G)) == 0);

    setup_v(BG_PLAKOTO, 0, "8:1 3:1 off:13", "17:2 24:13");
    G.pos[0].pin[0][8] = 1;
    bg_throw(&G, 3, 1);
    p = bg_pos(&G);
    check("plakoto: a pinned checker outside home: no bearing off", !has(p, 3, BG_OFF) && !has(p, 8, 5)
                                                                     && has(p, 3, 2));
    setup_v(BG_PLAKOTO, 0, "1:1 off:14", "6:14 20:1");
    check("plakoto: none borne off: 2 points (no 3)", bg_moves(&G, "21: 1/off") && bg_points(bg_pos(&G)) == 2);
    setup_v(BG_PLAKOTO, 0, "1:1 off:14", "off:1 6:14");
    check("plakoto: else 1 point", bg_moves(&G, "21: 1/off") && bg_points(bg_pos(&G)) == 1);
}

/* fevga: the same way round, one checker holds a point, the first checker past the other's start, the primes */
static void test_fevga(void)
{
    bg_new(&G, BG_FEVGA);
    const bpos *p = bg_pos(&G);
    check("fevga: fifteen each on their 24; ebony's 24 is ivory's 12", p->n[0][24] == 15 && p->n[1][24] == 15
                                    && bg_opp(BG_FEVGA, 12) == 24 && bg_opp(BG_FEVGA, 1) == 13);
    check("fevga: the first checker off the start", bg_moves(&G, "31: 64: 24/18") && !has(bg_pos(&G), 24, 20)
                                                     && has(bg_pos(&G), 18, 14));
    check("fevga: ebony's turn", bg_moves(&G, "18/14") && bg_pos(&G)->side == 1);
    setup_v(BG_FEVGA, 0, "24:14 18:1", "24:15");
    bg_throw(&G, 6, 5);
    check("fevga: the other's start is held", !has(bg_pos(&G), 18, 12) && has(bg_pos(&G), 18, 13));
    setup_v(BG_FEVGA, 0, "24:14 10:1", "24:14 20:1");
    bg_throw(&G, 2, 1);
    p = bg_pos(&G);
    check("fevga: a single checker holds its point", !has(p, 10, 8) && has(p, 10, 9));
    check("fevga: once past the other's start, more may leave", has(p, 24, 22));
    setup_v(BG_FEVGA, 0, "24:9 23:1 22:1 21:1 20:1 6:1 5:1", "24:14 1:1");
    bg_throw(&G, 5, 1);
    p = bg_pos(&G);
    check("fevga: not all six points of the starting table", !has(p, 24, 19) && has(p, 6, 1) && has(p, 24, 23));
    setup_v(BG_FEVGA, 0, "24:8 15:1 11:1 10:1 9:2 8:1 7:1", "24:15");
    bg_throw(&G, 3, 1);
    check("fevga: no six in a row with all the other's checkers behind", !has(bg_pos(&G), 9, 6)
                                                                         && has(bg_pos(&G), 7, 6));
    setup_v(BG_FEVGA, 0, "24:8 15:1 11:1 10:1 9:2 8:1 7:1", "24:14 10:1");
    bg_throw(&G, 3, 1);
    check("fevga: with one of them ahead, the six is allowed", has(bg_pos(&G), 9, 6));
    setup_v(BG_FEVGA, 0, "1:1 off:14", "20:15");
    check("fevga: none borne off: 2 points", bg_moves(&G, "21: 1/off") && bg_status(&G) == BG_WIN0
                                            && bg_points(bg_pos(&G)) == 2);
    setup_v(BG_FEVGA, 1, "off:1 20:14", "1:1 off:14");
    check("fevga: else 1 point", bg_moves(&G, "21: 1/off") && bg_status(&G) == BG_WIN1
                                 && bg_points(bg_pos(&G)) == 1);
}

/* long nardy: one checker off the head a turn, the first throw's doubles, the prime, oyn and mars */
static void test_nardy(void)
{
    bg_new(&G, BG_NARDY);
    check("nardy: one checker off the head", bg_moves(&G, "31: 53: 24/19") && !has(bg_pos(&G), 24, 21)
                                              && has(bg_pos(&G), 19, 16));
    check("nardy: undo brings the head's move back", bg_undo(&G) && has(bg_pos(&G), 24, 21)
                                                     && bg_pos(&G)->head == 0);
    bg_new(&G, BG_NARDY);
    check("nardy: a first 6-6, two off the head (18/12 is the other's head)",
          bg_moves(&G, "31: 66: 24/18 24/18") && bg_pos(&G)->side == 1 && bg_pos(&G)->n[0][18] == 2);
    check("nardy: ebony's first 6-6 too", bg_moves(&G, "66: 24/18 24/18") && bg_pos(&G)->side == 0
                                          && bg_pos(&G)->n[1][18] == 2);
    bg_new(&G, BG_NARDY);
    check("nardy: a first 4-4", bg_moves(&G, "31: 44: 24/20/16 24/20/16") && bg_pos(&G)->side == 1
                                && bg_pos(&G)->n[0][16] == 2);
    bg_new(&G, BG_NARDY);
    check("nardy: a first 3-3", bg_moves(&G, "31: 33: 24/21/18/15 24/21") && bg_pos(&G)->side == 1
                                && bg_pos(&G)->n[0][24] == 13);
    bg_new(&G, BG_NARDY);
    check("nardy: a first 5-5: one checker", bg_moves(&G, "31: 55: 24/19") && !has(bg_pos(&G), 24, 19)
                                             && bg_moves(&G, "19/14/9/4") && bg_pos(&G)->n[0][4] == 1);
    setup_v(BG_NARDY, 0, "24:14 10:1", "24:15");
    check("nardy: a later 6-6: one off the head", bg_moves(&G, "66: 24/18") && !has(bg_pos(&G), 24, 18));
    setup_v(BG_NARDY, 0, "24:9 23:1 22:1 21:1 20:1 6:1 5:1", "24:14 1:1");
    bg_throw(&G, 5, 1);
    check("nardy: six in a row in the start table, one of the other's ahead: allowed", has(bg_pos(&G), 24, 19));
    setup_v(BG_NARDY, 0, "24:8 15:1 11:1 10:1 9:2 8:1 7:1", "24:15");
    bg_throw(&G, 3, 1);
    check("nardy: no six in a row with all the other's checkers behind", !has(bg_pos(&G), 9, 6));
    setup_v(BG_NARDY, 0, "1:1 off:14", "20:15");
    check("nardy: mars, 2 points", bg_moves(&G, "21: 1/off") && bg_points(bg_pos(&G)) == 2);
    setup_v(BG_NARDY, 0, "1:1 off:14", "off:3 20:12");
    check("nardy: oyn, 1 point", bg_moves(&G, "21: 1/off") && bg_points(bg_pos(&G)) == 1);
}

int main(void)
{
    bg_new(&G, BG_BACKGAMMON);
    const bpos *p = bg_pos(&G);
    check("start: 15 each, 167 pips, the opening roll", p->phase == BG_OPENING && bg_pips(p, 0) == 167
                                                        && bg_pips(p, 1) == 167 && p->n[0][13] == 5
                                                        && p->n[1][24] == 2);
    check("no move before the throw", !bg_moves(&G, "8/5"));
    check("opening tie 44: throw again", bg_moves(&G, "44:") && bg_pos(&G)->phase == BG_OPENING);
    check("opening 13: ebony plays 3 and 1", bg_moves(&G, "13:") && bg_pos(&G)->side == 1
                                            && bg_pos(&G)->phase == BG_MOVE);
    bg_new(&G, BG_BACKGAMMON);
    check("opening 31: ivory plays 8/5 6/5", bg_moves(&G, "31: 8/5 6/5") && bg_pos(&G)->n[0][5] == 2
                                            && bg_pos(&G)->side == 1 && bg_pos(&G)->phase == BG_THROW);
    check("a move the dice do not give: refused", bg_moves(&G, "62:") && !bg_moves(&G, "13/12"));

    /* hitting: ebony leaves blots on its 18 and 11 (ivory's 7 and 14), ivory hits on 7 */
    check("ebony 62: 24/18 13/11", bg_moves(&G, "24/18 13/11"));
    check("ivory 64: 13/7* hits, 24/20", bg_moves(&G, "64: 13/7* 24/20") && bg_pos(&G)->n[1][BG_BAR] == 1
                                         && bg_pos(&G)->n[1][18] == 0 && bg_pos(&G)->n[0][7] == 1);
    check("ebony must enter first", bg_moves(&G, "43:") && !has(bg_pos(&G), 13, 9) && has(bg_pos(&G), BG_BAR, 21)
                                    && has(bg_pos(&G), BG_BAR, 22));
    check("ebony enters bar/21, then 13/10", bg_moves(&G, "bar/21 13/10") && bg_pos(&G)->n[1][BG_BAR] == 0);

    /* a closed board: the checker on the bar cannot enter, the throw passes */
    setup(1, "6:2 5:2 4:2 3:2 2:2 1:2 13:3", "bar:1 13:14");
    check("closed board: ebony's 31 passes", bg_throw(&G, 3, 1) && bg_pos(&G)->passed == 1
                                             && bg_pos(&G)->side == 0 && bg_pos(&G)->phase == BG_THROW);
    setup(1, "6:2 5:2 4:2 13:9", "bar:1 13:14");
    check("6 5 4 held: ebony's 65 passes", bg_throw(&G, 6, 5) && bg_pos(&G)->passed == 1);
    setup(1, "6:2 5:2 4:2 13:9", "bar:1 13:14");
    bg_throw(&G, 6, 3);
    check("63: only the 3 enters (bar/22), then the 6 goes on", has(bg_pos(&G), BG_BAR, 22)
                                    && !has(bg_pos(&G), 13, 7) && bg_moves(&G, "bar/22 13/7")
                                    && bg_pos(&G)->side == 0);

    /* both dice if they can be played, else the larger */
    setup(0, "13:1 9:1 off:13", "23:2 21:2 off:11");
    bg_throw(&G, 6, 5);
    check("both dice: 13/8 and 9/3 are moves, 13/7 is not", has(bg_pos(&G), 13, 8) && has(bg_pos(&G), 9, 3)
                                                            && !has(bg_pos(&G), 13, 7));
    setup(0, "13:1 10:1 off:13", "23:2 21:2 20:2 off:9");
    bg_throw(&G, 6, 5);
    bmove l[BG_MAXMOVES];
    int n = bg_legal(bg_pos(&G), l);
    check("one die only: the larger (13/7)", n == 1 && l[0].from == 13 && l[0].to == 7
                                             && bg_moves(&G, "13/7") && bg_pos(&G)->side == 1);
    setup(0, "13:1 off:14", "18:2 23:2 off:11");
    bg_throw(&G, 6, 5);
    n = bg_legal(bg_pos(&G), l);
    check("the larger cannot: the smaller (13/8)", n == 1 && l[0].to == 8);

    /* doubles: four moves */
    bg_new(&G, BG_BACKGAMMON);
    check("ivory's 66: 24/18(2) 13/7(2)", bg_moves(&G, "31: 8/5 6/5 21: 13/11 6/5 66:") && bg_pos(&G)->nd == 4
                                          && bg_moves(&G, "24/18(2) 13/7(2)") && bg_pos(&G)->side == 1
                                          && bg_pos(&G)->n[0][7] == 2 && bg_pos(&G)->n[0][18] == 2);
    bg_new(&G, BG_BACKGAMMON);
    check("a checker on with several dice: 33: 24/15 13/10", bg_moves(&G, "31: 8/5 6/5 21: 13/11 6/5 33: 24/15 13/10")
                                                         && bg_pos(&G)->n[0][15] == 1 && bg_pos(&G)->side == 1);

    /* bearing off */
    setup(0, "7:1 3:1 off:13", "13:15");
    bg_throw(&G, 4, 3);
    check("not all home: no bearing off", !has(bg_pos(&G), 3, BG_OFF) && has(bg_pos(&G), 7, 3));
    setup(0, "6:1 3:1 off:13", "13:15");
    bg_throw(&G, 6, 5);
    check("6/off exact; the 5 not from the 3 (the 6 is higher)", has(bg_pos(&G), 6, BG_OFF)
                                                              && !has(bg_pos(&G), 3, BG_OFF) && has(bg_pos(&G), 6, 1));
    setup(0, "3:1 2:1 off:13", "13:15");
    bg_throw(&G, 6, 5);
    check("higher numbers bear off the highest: 3/off 2/off", !has(bg_pos(&G), 2, BG_OFF) && bg_moves(&G, "3/off 2/off")
                                                              && bg_status(&G) == BG_WIN0);

    /* the end: 1, 2 or 3 points */
    setup(0, "1:1 off:14", "off:1 6:14");
    bg_moves(&G, "21: 1/off");
    check("a single game: 1 point", bg_status(&G) == BG_WIN0 && bg_points(bg_pos(&G)) == 1);
    setup(0, "1:1 off:14", "6:15");
    bg_moves(&G, "21: 1/off");
    check("a gammon: 2 points", bg_status(&G) == BG_WIN0 && bg_points(bg_pos(&G)) == 2);
    setup(0, "1:1 off:14", "6:14 20:1");
    bg_moves(&G, "21: 1/off");
    check("a backgammon (in the winner's home): 3 points", bg_points(bg_pos(&G)) == 3);
    setup(1, "6:14 bar:1", "1:1 off:14");
    bg_moves(&G, "21: 1/off");
    check("a backgammon (on the bar), ebony wins", bg_status(&G) == BG_WIN1 && bg_points(bg_pos(&G)) == 3);
    check("no throw after the end", !bg_moves(&G, "21:"));

    /* undo: die by die, then over the other player's throw (it never comes back thrown again) */
    bg_new(&G, BG_BACKGAMMON);
    bg_moves(&G, "31: 8/5");
    check("undo within the turn: the die comes back", bg_undo(&G) && bg_pos(&G)->used == 0
                                                      && bg_pos(&G)->die[0] == 3 && bg_pos(&G)->phase == BG_MOVE);
    check("nothing before the first move", !bg_undo(&G) && bg_pos(&G)->phase == BG_MOVE);
    bg_moves(&G, "8/5 6/5 64:");
    check("undo over ebony's throw: ivory's 31 back, one die to play", bg_undo(&G) && bg_pos(&G)->side == 0
                                    && bg_pos(&G)->phase == BG_MOVE && bg_pos(&G)->used == 1
                                    && bg_pos(&G)->n[0][6] == 5 && bg_pos(&G)->n[0][5] == 1);
    setup(0, "6:2 5:2 4:2 3:2 2:2 1:2 13:3", "bar:1 13:14");
    bg_moves(&G, "21: 13/10 31:");
    check("undo over a pass", bg_pos(&G)->passed == 1 && bg_undo(&G) && bg_pos(&G)->side == 0
                              && bg_pos(&G)->n[0][11] == 1);

    test_plakoto();
    test_fevga();
    test_nardy();

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
